#include "IoApic.h"
#include "device/cpu/Cpu.h"
#include "kernel/system/System.h"
#include "kernel/paging/Paging.h"

namespace Device {

IoApicPlatform *IoApic::ioPlatform = nullptr;
Kernel::Logger IoApic::log = Kernel::Logger::get("IoApic");

// TODO: Should I arbitrarily assign IDs like this or does ACPI already contain valid IDs?
uint8_t ioApicId = 0; // IoApics don't start with an assigned id

bool IoApic::isInitialized() const {
    return initialized;
}

// TODO: Need to set the IO APIC ID in the ID register (it's set to 0 on power up)
//       Copy the value from ACPI? Or is this value 0 aswell?
void IoApic::initialize(IoApicPlatform *platform, IoApicInformation *info) {
    LocalApic::ensureInitialized();
    ioPlatform = platform; // This is set multiple times, doesn't matter
    ioInfo = info;
    initializeMMIORegion();

    // https://github.com/torvalds/linux/blob/master/arch/x86/kernel/apic/io_apic.c#L470
    ioPlatform->version = readDoubleWord(VER);
    ioPlatform->directEoiSupported = ioPlatform->version >= 0x20;

    // With the IRQPA there is a way to address more than 255 GSIs although maxREDTBLEntries only has 8 bits
    // With ICH5 (and other ICHs?) it is always 24 (also ICH5 only has 1 IO APIC, as all (?) consumer hardware)
    ioInfo->gsiMax = static_cast<GlobalSystemInterrupt>(ioInfo->gsiBase + (readDoubleWord(VER) >> 16));
    if (ioInfo->gsiMax > ioPlatform->globalMaxGsi) {
        ioPlatform->globalMaxGsi = ioInfo->gsiMax;
    }

    initializeREDTBL();

    // Configure NMI if it exists
    if (ioInfo->nmi != nullptr) {
        REDTBLEntry redtblEntry{};
        redtblEntry.vector = static_cast<InterruptVector>(0);
        redtblEntry.deliveryMode = REDTBLEntry::DeliveryMode::NMI;
        redtblEntry.destinationMode = REDTBLEntry::DestinationMode::PHYSICAL; // TODO: What to set here?
        redtblEntry.pinPolarity = ioInfo->nmi->polarity;
        redtblEntry.triggerMode = ioInfo->nmi->triggerMode;
        redtblEntry.isMasked = false;
        redtblEntry.destination = LocalApic::getId();
        writeREDTBL(ioInfo->nmi->gsi, redtblEntry);
    }

    initialized = true;
}

void IoApic::allow(GlobalSystemInterrupt gsi) {
    ensureValidGsi(gsi);
    REDTBLEntry redtblEntry = readREDTBL(gsi);
    redtblEntry.isMasked = false;
    writeREDTBL(gsi, redtblEntry);
}

void IoApic::forbid(GlobalSystemInterrupt gsi) {
    ensureValidGsi(gsi);
    REDTBLEntry redtblEntry = readREDTBL(gsi);
    redtblEntry.isMasked = true;
    writeREDTBL(gsi, redtblEntry);
}

bool IoApic::status(GlobalSystemInterrupt gsi) {
    ensureValidGsi(gsi);
    return readREDTBL(gsi).isMasked;
}

void IoApic::sendEndOfInterrupt(InterruptVector vector, GlobalSystemInterrupt gsi) {
    if (ioPlatform->directEoiSupported) {
        writeDirectRegister<uint32_t>(EOI, vector);
    } else {
        // TODO: Find this in the manual
        // https://github.com/torvalds/linux/blob/master/arch/x86/kernel/apic/io_apic.c#L470
        REDTBLEntry redtblEntry = readREDTBL(gsi);
    }
}

void IoApic::ensureMMIO() const {
    if (ioInfo->virtAddress == 0) {
        Util::Exception::throwException(Util::Exception::NULL_POINTER, "IoApic MMIO not initialized!");
    }
}

void IoApic::ensureValidGsi(GlobalSystemInterrupt gsi) const {
    if (gsi < ioInfo->gsiBase || gsi > ioInfo->gsiMax) {
        Util::Exception::throwException(Util::Exception::INVALID_ARGUMENT, "GSI not handled by this IO APIC!");
    }
}

void IoApic::initializeMMIORegion() {
    uint32_t physAddress = ioInfo->physAddress;
    uint32_t pageOffset = physAddress % Util::Memory::PAGESIZE;

    auto &memoryService = Kernel::System::getService<Kernel::MemoryService>();
    void *virtAddress = memoryService.mapIO(physAddress, Util::Memory::PAGESIZE, true);

    // Account for possible misalignment
    ioInfo->virtAddress = reinterpret_cast<uint32_t>(virtAddress) + pageOffset;
}

// TODO: I read that GSI0 is typically used for ExtINT (PIC), if I can verify that the GSI0 can always be masked
//       as I don't support virtual wire
//       GSI0 is also for NMIs?
void IoApic::initializeREDTBL() {
    REDTBLEntry redtblEntry{};
    redtblEntry.deliveryMode = REDTBLEntry::DeliveryMode::FIXED;
    redtblEntry.destinationMode = REDTBLEntry::DestinationMode::PHYSICAL;
    redtblEntry.isMasked = true;
    redtblEntry.destination = LocalApic::getId(); // TODO: Don't send all interrupts to one CPU

    for (uint32_t interruptInput = ioInfo->gsiBase; interruptInput <= ioInfo->gsiMax; ++interruptInput) {
        auto gsi = static_cast<GlobalSystemInterrupt>(interruptInput); // GSIs match interrupt inputs on IO APIC

        redtblEntry.vector = static_cast<InterruptVector>(gsi + 32); // If no override exists GSI matches vector
        redtblEntry.pinPolarity = REDTBLEntry::PinPolarity::HIGH;
        redtblEntry.triggerMode = REDTBLEntry::TriggerMode::EDGE;

        IoApicIrqOverride *override = ioPlatform->getIoApicIrqOverride(gsi);
        if (override != nullptr) {
            redtblEntry.vector = static_cast<InterruptVector>(override->source + 32);
            // NOTE: This is disabled because some ACPI entries are bugged (for example the PIT redirect)
            // redtblEntry.pinPolarity = override->polarity;
            // redtblEntry.triggerMode = override->triggerMode;
        }

        writeREDTBL(gsi, redtblEntry);
    }
}

#if HHUOS_APIC_ENABLE_DEBUG == 1
void IoApic::dumpREDTBL() {
    log.info("Redirection Table (I/O APIC Id: [%d]):", ioInfo->id);
    for (uint32_t gsi = ioInfo->gsiBase; gsi < ioInfo->gsiMax; ++gsi) {
        REDTBLEntry redtblEntry = readREDTBL(static_cast<GlobalSystemInterrupt>(gsi));
        log.info("- Interrupt [%d]: (Vector: [0x%x], Masked: [%d], Destination: [%d], DeliveryMode: [0b%b], DestinationMode: [%s], Polarity: [%s], TriggerMode: [%s])",
                 gsi,
                 static_cast<uint8_t>(redtblEntry.vector),
                 static_cast<uint8_t>(redtblEntry.isMasked),
                 redtblEntry.destination,
                 static_cast<uint8_t>(redtblEntry.deliveryMode),
                 redtblEntry.destinationMode == REDTBLEntry::DestinationMode::PHYSICAL ? "PHYSICAL" : "LOGICAL",
                 redtblEntry.pinPolarity == REDTBLEntry::PinPolarity::HIGH ? "HIGH" : "LOW",
                 redtblEntry.triggerMode == REDTBLEntry::TriggerMode::EDGE ? "EDGE" : "LEVEL");
    }
}
#endif

uint32_t IoApic::readDoubleWord(IndirectRegister reg) {
    ensureMMIO();
    Cpu::disableInterrupts(); // Do not let another interrupt handler fuck this up
    writeDirectRegister<uint8_t>(IND, reg);
    auto val = readDirectRegister<uint32_t>(DAT);
    Cpu::enableInterrupts();
    return val;
}

void IoApic::writeDoubleWord(IndirectRegister reg, uint32_t val) {
    ensureMMIO();
    Cpu::disableInterrupts(); // Do not let another interrupt handler fuck this up
    writeDirectRegister<uint8_t>(IND, reg);
    writeDirectRegister<uint32_t>(DAT, val);
    Cpu::enableInterrupts();
}

REDTBLEntry IoApic::readREDTBL(GlobalSystemInterrupt gsi) {
    ensureValidGsi(gsi);
    auto interruptInput = static_cast<uint8_t>(gsi - ioInfo->gsiBase);

    // The first register is the low DW, the second register is the high DW
    Cpu::disableInterrupts(); // Do not let another interrupt handler fuck this up
    uint32_t low = readDoubleWord(static_cast<IndirectRegister>(REDTBL + 2 * interruptInput));
    uint64_t high = readDoubleWord(static_cast<IndirectRegister>(REDTBL + 2 * interruptInput + 1));
    Cpu::enableInterrupts();
    return static_cast<REDTBLEntry>(low | high << 32);
}

void IoApic::writeREDTBL(GlobalSystemInterrupt gsi, const REDTBLEntry &redtbl) {
    ensureValidGsi(gsi);
    auto interruptInput = static_cast<uint8_t>(gsi - ioInfo->gsiBase);

    // The first register is the low DW, the second register is the high DW
    auto val = static_cast<uint64_t>(redtbl);
    Cpu::disableInterrupts(); // Do not let another interrupt handler fuck this up
    writeDoubleWord(static_cast<IndirectRegister>(REDTBL + 2 * interruptInput), val & 0xFFFFFFFF);
    writeDoubleWord(static_cast<IndirectRegister>(REDTBL + 2 * interruptInput + 1), val >> 32);
    Cpu::enableInterrupts();
}

}