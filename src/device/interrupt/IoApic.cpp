#include "IoApic.h"
#include "LocalApic.h"
#include "device/cpu/Cpu.h"
#include "kernel/system/System.h"
#include "kernel/paging/Paging.h"

namespace Device {

bool IoApic::initialized = false;
Kernel::Logger IoApic::log = Kernel::Logger::get("IoApic");

// TODO: IoApics could be managed through instances...
//       - Apic would be the class the OS interacts with, Apic manages multiple IoApic instances...
//       - Apic could get all the interaction methods (EOI, allowExternalInterrupt, allowLocalInterrupt, ...)
//       - The same could be done for local APIC, but the main part of the functions would stay static,
//         as local APIC always accesses registers of the current CPU, don't need instancing here?
//       - Or use instancing to manage the state, like individual relocation?

// TODO: Should I arbitrarily assign IDs like this or does ACPI already contain valid IDs?
uint8_t ioApicId = 0; // IoApics don't start with an assigned id

bool IoApic::isInitialized() {
    return initialized;
}

void IoApic::ensureInitialized() {
    if (!initialized) {
        Util::Exception::throwException(Util::Exception::ILLEGAL_STATE, "IO APICs are not initialized!");
    }
}

void IoApic::initialize() {
    LocalApic::ensureInitialized();

    // Initialize all IO APICs of the system
    for (auto *ioapic: Apic::ioapics()) {
        initializeController(ioapic);
    }

    initialized = true;
}

void IoApic::allow(InterruptSource interruptSource) {
    // The GsiSources don't translate to interrupt inputs 1:1 because of possible remappings
    GlobalSystemInterrupt gsi = Apic::getInterruptOverrideTarget(interruptSource);

    // We need to find out what IO APIC is responsible for this interrupt
    IoApicInformation *ioapic = Apic::getIoApicInformation(gsi);

    REDTBLEntry redtblEntry = readREDTBL(ioapic, gsi);
    redtblEntry.isMasked = false;
    writeREDTBL(ioapic, gsi, redtblEntry);
}

void IoApic::forbid(InterruptSource interruptSource) {
    GlobalSystemInterrupt gsi = Apic::getInterruptOverrideTarget(interruptSource);
    IoApicInformation *ioapic = Apic::getIoApicInformation(gsi);

    REDTBLEntry redtblEntry = readREDTBL(ioapic, gsi);
    redtblEntry.isMasked = true;
    writeREDTBL(ioapic, gsi, redtblEntry);
}

bool IoApic::status(InterruptSource interruptSource) {
    GlobalSystemInterrupt gsi = Apic::getInterruptOverrideTarget(interruptSource);
    IoApicInformation *ioapic = Apic::getIoApicInformation(gsi);

    return readREDTBL(ioapic, gsi).isMasked;
}

// TODO: EOI compatibility mode
// Intel ICH5 Datasheet Chapter 9.5.5
void IoApic::sendEndOfInterrupt(InterruptVector vector) {
    // Signal EOI to every IO APIC, otherwise we would have to check beforehand which IO APIC
    // is responsible for the current ended interrupt
    for (uint32_t i = 0; i < Apic::ioapics().size(); ++i) {
        IoApicInformation *ioapic = Apic::ioapics().get(i);
        writeDirectRegister<uint32_t>(ioapic, EOI, vector);
    }
}

void IoApic::ensureMMIO(IoApicInformation *ioapic) {
    if (ioapic->virtAddress == 0) {
        Util::Exception::throwException(Util::Exception::NULL_POINTER, "IoApic MMIO not initialized!");
    }
}

void IoApic::ensureValidGsi(IoApicInformation *ioapic, GlobalSystemInterrupt gsi) {
    if (gsi < ioapic->gsiBase || gsi > ioapic->gsiMax) {
        Util::Exception::throwException(Util::Exception::INVALID_ARGUMENT, "GSI not handled by this IO APIC!");
    }
}

// TODO: Need to set the IO APIC ID in the ID register (it's set to 0 on power up)
//       Copy the value from ACPI? Or is this value 0 aswell?
void IoApic::initializeController(IoApicInformation *ioapic) {
    initializeMMIORegion(ioapic);

    // NOTE: These are overwritten with every IO APIC but this doesn't matter as they are equal
    // https://github.com/torvalds/linux/blob/master/arch/x86/kernel/apic/io_apic.c#L470
    Apic::ioPlatform->version = readDoubleWord(ioapic, VER);
    Apic::ioPlatform->eoiSupported = Apic::ioPlatform->version >= 0x20;

    // NOTE: With the IRQPA there is a way to address more than 255 GSIs although maxREDTBLEntries only has 8 bits
    // NOTE: With ICH5 (and other ICHs?) it is always 24 (also ICH5 only has 1 IO APIC)
    ioapic->gsiMax = static_cast<GlobalSystemInterrupt>(ioapic->gsiBase + (readDoubleWord(ioapic, VER) >> 16));
    if (ioapic->gsiMax > Apic::ioPlatform->globalMaxGsi) {
        Apic::ioPlatform->globalMaxGsi = ioapic->gsiMax;
    }

    initializeREDTBL(ioapic);

    // Configure NMI if it exists
    auto *nmi = Apic::getIoNMIConfiguration(ioapic);
    if (nmi != nullptr) {
        REDTBLEntry redtblEntry{};
        redtblEntry.vector = static_cast<InterruptVector>(0);
        redtblEntry.deliveryMode = REDTBLEntry::DeliveryMode::NMI;
        redtblEntry.destinationMode = REDTBLEntry::DestinationMode::PHYSICAL; // TODO: What to set here?
        redtblEntry.pinPolarity = nmi->polarity;
        redtblEntry.triggerMode = nmi->triggerMode;
        redtblEntry.isMasked = false;
        redtblEntry.destination = LocalApic::getId();
        writeREDTBL(ioapic, nmi->gsi, redtblEntry);
    }
}

void IoApic::initializeMMIORegion(IoApicInformation *ioapic) {
    uint32_t physAddress = ioapic->address;
    uint32_t pageOffset = physAddress % Util::Memory::PAGESIZE;

    auto &memoryService = Kernel::System::getService<Kernel::MemoryService>();
    void *virtAddress = memoryService.mapIO(physAddress, Util::Memory::PAGESIZE, true);

    // Account for possible misalignment
    ioapic->virtAddress = reinterpret_cast<uint32_t>(virtAddress) + pageOffset;
}

// TODO: I read that GSI0 is typically used for ExtINT (PIC), if I can verify that the GSI0 can always be masked
//       as I don't support virtual wire
void IoApic::initializeREDTBL(IoApicInformation *ioapic) {
    REDTBLEntry redtblEntry{};
    redtblEntry.deliveryMode = REDTBLEntry::DeliveryMode::FIXED;
    redtblEntry.destinationMode = REDTBLEntry::DestinationMode::PHYSICAL;
    redtblEntry.isMasked = true;
    redtblEntry.destination = LocalApic::getId(); // TODO: Don't send all interrupts to one CPU

    for (uint8_t interruptInput = ioapic->gsiBase; interruptInput <= ioapic->gsiMax; ++interruptInput) {
        auto gsi = static_cast<GlobalSystemInterrupt>(interruptInput); // GSIs match interrupt inputs on IO APIC

        redtblEntry.vector = static_cast<InterruptVector>(gsi + 32); // If no override exists GSI matches vector
        redtblEntry.pinPolarity = REDTBLEntry::PinPolarity::HIGH;
        redtblEntry.triggerMode = REDTBLEntry::TriggerMode::EDGE;

        auto *override = Apic::getInterruptOverride(gsi);
        if (override != nullptr) {
            redtblEntry.vector = static_cast<InterruptVector>(override->source + 32);
            // TODO: This is disabled because ACPI entries are bugged?
            //       PIT polarity/triggermode are overridden but wrong...
            // redtblEntry.pinPolarity = override->polarity;
            // redtblEntry.triggerMode = override->triggerMode;
        }

        writeREDTBL(ioapic, gsi, redtblEntry);
    }
}

uint32_t IoApic::readDoubleWord(IoApicInformation *ioapic, Indirect_Register reg) {
    ensureMMIO(ioapic);
    // Cpu::disableInterrupts(); // Do not let another interrupt handler fuck this up
    writeDirectRegister<uint8_t>(ioapic, IND, reg);
    auto val = readDirectRegister<uint32_t>(ioapic, DAT);
    // Cpu::enableInterrupts();
    return val;
}

void IoApic::writeDoubleWord(IoApicInformation *ioapic, Indirect_Register reg, uint32_t val) {
    ensureMMIO(ioapic);
    // Cpu::disableInterrupts(); // Do not let another interrupt handler fuck this up
    writeDirectRegister<uint8_t>(ioapic, IND, reg);
    writeDirectRegister<uint32_t>(ioapic, DAT, val);
    // Cpu::enableInterrupts();
}

REDTBLEntry IoApic::readREDTBL(IoApicInformation *ioapic, GlobalSystemInterrupt gsi) {
    ensureValidGsi(ioapic, gsi);
    auto interruptInput = static_cast<uint8_t>(gsi - ioapic->gsiBase);

    // The first register is the low DW, the second register is the high DW
    // Cpu::disableInterrupts(); // Do not let another interrupt handler fuck this up
    uint32_t low = readDoubleWord(ioapic, static_cast<Indirect_Register>(REDTBL + 2 * interruptInput));
    uint64_t high = readDoubleWord(ioapic, static_cast<Indirect_Register>(REDTBL + 2 * interruptInput + 1));
    // Cpu::enableInterrupts();
    return static_cast<REDTBLEntry>(low | high << 32);
}

void IoApic::writeREDTBL(IoApicInformation *ioapic, GlobalSystemInterrupt gsi, const REDTBLEntry &redtbl) {
    ensureValidGsi(ioapic, gsi);
    auto interruptInput = static_cast<uint8_t>(gsi - ioapic->gsiBase);

    // The first register is the low DW, the second register is the high DW
    auto val = static_cast<uint64_t>(redtbl);
    // Cpu::disableInterrupts(); // Do not let another interrupt handler fuck this up
    writeDoubleWord(ioapic, static_cast<Indirect_Register>(REDTBL + 2 * interruptInput), val & 0xFFFFFFFF);
    writeDoubleWord(ioapic, static_cast<Indirect_Register>(REDTBL + 2 * interruptInput + 1), val >> 32);
    // Cpu::enableInterrupts();
}

}