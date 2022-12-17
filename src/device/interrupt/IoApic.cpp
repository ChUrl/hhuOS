#include "IoApic.h"
#include "kernel/system/System.h"
#include "device/interrupt/LApic.h"

namespace Device {

bool IoApic::initialized = false;
Kernel::Logger IoApic::log = Kernel::Logger::get("IoApic");

// TODO: Should I arbitrarily assign IDs like this or does ACPI already contain valid IDs?
uint8_t ioApicId = 0; // IoApics don't start with an assigned id

// ! Public member functions start here

bool IoApic::isInitialized() {
    return initialized;
}

void IoApic::initialize() {
    LApic::ensureInitialized();

    // Initialize all IO APICs of the system
    for (auto *ioapic : InterruptModel::ioapics()) {
        initializeController(ioapic);
    }

    initialized = true;
}

// TODO: Should this take an InterruptInput?
void IoApic::allow(GlobalSystemInterrupt gsi) {
    IoApicInformation *ioapic = InterruptModel::getIoApicInformation(gsi);

    REDTBLEntry redtblEntry = readREDTBL(ioapic, gsi);
    redtblEntry.isMasked = false;
    writeREDTBL(ioapic, gsi, redtblEntry);
}

// TODO: Should this take an InterruptInput?
void IoApic::forbid(GlobalSystemInterrupt gsi) {
    IoApicInformation *ioapic = InterruptModel::getIoApicInformation(gsi);

    REDTBLEntry redtblEntry = readREDTBL(ioapic, gsi);
    redtblEntry.isMasked = true;
    writeREDTBL(ioapic, gsi, redtblEntry);
}

// TODO: Should this take an InterruptInput?
bool IoApic::status(GlobalSystemInterrupt gsi) {
    IoApicInformation *ioapic = InterruptModel::getIoApicInformation(gsi);

    return readREDTBL(ioapic, gsi).isMasked;
}

// TODO: Should this take an InterruptInput?
// TODO: Compatibility mode
// Intel ICH5 Datasheet Chapter 9.5.5
void IoApic::sendEndOfInterrupt(Kernel::InterruptDispatcher::Interrupt vector) {
    IoApicInformation *ioapic = InterruptModel::getIoApicInformation(static_cast<GlobalSystemInterrupt>(vector));

    volatile auto *regAddr = reinterpret_cast<uint32_t *>(ioapic->virtAddress + Register::EOI);
    *regAddr = static_cast<uint8_t>(vector);
}

// ! Private member functions start here

void IoApic::verifyMMIO(IoApicInformation *ioapic) {
    if (ioapic->virtAddress == 0) {
        Util::Exception::throwException(Util::Exception::NULL_POINTER, "IoApic::readDoubleWord(): IoApic MMIO not initialized!");
    }
}

void IoApic::verifyGSI(Device::IoApicInformation *ioapic, Device::GlobalSystemInterrupt gsi) {
    if (gsi < ioapic->gsiBase || gsi > ioapic->gsiMax) {
        Util::Exception::throwException(Util::Exception::INVALID_ARGUMENT, "IoApic::readREDTBL(): GSI is handled by different IO APIC!");
    }
}

// TODO: Need to set the IO APIC ID in the ID register (it's set to 0 on power up)
//       Copy the value from ACPI? Or is this value 0 aswell?
void IoApic::initializeController(IoApicInformation *ioapic) {
    initializeMMIORegion(ioapic);

    // NOTE: These are overwritten for each IO APIC but this doesn't matter as they are equal
    // https://github.com/torvalds/linux/blob/master/arch/x86/kernel/apic/io_apic.c#L470
    InterruptModel::ioPlatform->version = readDoubleWord(ioapic, Indirect_Register::VER) & 0xFF;
    InterruptModel::ioPlatform->eoiSupported = InterruptModel::ioPlatform->version >= 0x20;

    // NOTE: With the IRQPA there is a way to address more than 255 GSIs although maxREDTBLEntries only has 8 bits
    // NOTE: With ICH5 (and other ICHs?) it is always 24
    ioapic->gsiMax = ioapic->gsiBase + ((readDoubleWord(ioapic, Indirect_Register::VER) >> 16) & 0xFF);
    if (ioapic->gsiMax > InterruptModel::getGlobalGsiMax()) {
        InterruptModel::ioPlatform->globalGsiMax = ioapic->gsiMax;
    }

    initializeREDTBL(ioapic);

    // Configure NMI if it exists
    auto *nmi = InterruptModel::getIoNMIConfiguration(ioapic);
    if (nmi != nullptr) {
        REDTBLEntry redtblEntry {};
        redtblEntry.vector = static_cast<Kernel::InterruptDispatcher::Interrupt>(0);
        redtblEntry.deliveryMode = REDTBLEntry::DeliveryMode::NMI;
        redtblEntry.destinationMode = REDTBLEntry::DestinationMode::PHYSICAL; // TODO: What to set here?
        redtblEntry.pinPolarity = nmi->polarity;
        redtblEntry.triggerMode = nmi->triggerMode;
        redtblEntry.isMasked = false;
        redtblEntry.destination = LApic::getId();
        writeREDTBL(ioapic, nmi->gsi, redtblEntry);
    }
}

void IoApic::initializeMMIORegion(IoApicInformation *ioapic) {
    auto &memoryService = Kernel::System::getService<Kernel::MemoryService>();
    void *virtAddress = memoryService.mapIO(ioapic->address, Util::Memory::PAGESIZE, true);

    if (virtAddress == nullptr) {
        Util::Exception::throwException(Util::Exception::OUT_OF_MEMORY, "IoApic::initializeController(): Not enough space left on kernel heap!");
    }

    // Use this addresses to access this IO APIC's memory mapped registers
    ioapic->virtAddress = reinterpret_cast<uint32_t>(virtAddress) + ioapic->address % Util::Memory::PAGESIZE;
}

// TODO: Has to set polarity etc. for overidden interrupts
// TODO: I read that GSI0 is typically used for ExtINT (PIC), if I can verify that the GSI0 can always be masked
//       as I don't support virtual wire
void IoApic::initializeREDTBL(IoApicInformation *ioapic) {
    uint8_t id = LApic::getId(); // TODO: Don't send all interrupts to one cpu

    for (GlobalSystemInterrupt gsi = ioapic->gsiBase; gsi <= ioapic->gsiMax; ++gsi) {
        // Prevent overwriting entries that were already written because of an interrupt override
        if (InterruptModel::hasOverride(static_cast<InterruptInput>(static_cast<uint8_t>(gsi)))) {
            continue;
        }

        // NOTE: Interrupts have to be disabled beforehand
        REDTBLEntry redtblEntry {};
        redtblEntry.vector = static_cast<Kernel::InterruptDispatcher::Interrupt>(gsi);
        redtblEntry.deliveryMode = REDTBLEntry::DeliveryMode::FIXED; // TODO
        redtblEntry.destinationMode = REDTBLEntry::DestinationMode::PHYSICAL; // TODO
        redtblEntry.pinPolarity = REDTBLEntry::PinPolarity::HIGH;
        redtblEntry.triggerMode = REDTBLEntry::TriggerMode::EDGE;
        redtblEntry.isMasked = true;
        redtblEntry.destination = id;
        writeREDTBL(ioapic, gsi, redtblEntry);
    }
}

// ! Private register member functions start here

// TODO: Spinlock?
// TODO: Use Indirect_Register type and overload the name with a function accepting Register type
uint32_t IoApic::readDoubleWord(IoApicInformation *ioapic, uint8_t reg) {
    verifyMMIO(ioapic);
    *reinterpret_cast<volatile uint8_t *>(ioapic->virtAddress + Register::IND) = static_cast<uint8_t>(reg);
    return *reinterpret_cast<volatile uint32_t *>(ioapic->virtAddress + Register::DAT);
}

// TODO: Spinlock?
// TODO: Use Indirect_Register type
void IoApic::writeDoubleWord(IoApicInformation *ioapic, uint8_t reg, uint32_t val) {
    verifyMMIO(ioapic);
    *reinterpret_cast<uint8_t *>(ioapic->virtAddress + Register::IND) = static_cast<uint8_t>(reg);
    *reinterpret_cast<uint32_t *>(ioapic->virtAddress + Register::DAT) = val;
}

// TODO: Spinlock?
// TODO: Should this be so complicated? The remapping stuff could be taken out and the table entry passed...
REDTBLEntry IoApic::readREDTBL(IoApicInformation *ioapic, GlobalSystemInterrupt gsi) {
    verifyGSI(ioapic, gsi);

    if (InterruptModel::hasOverride(static_cast<InterruptInput>(static_cast<uint8_t>(gsi)))) {
        Util::Exception::throwException(Util::Exception::INVALID_ARGUMENT, "INTI belongs to other GSI!");
    }

    auto *override = InterruptModel::getInterruptOverride(gsi);
    uint8_t entry;
    if (override != nullptr) {
        entry = override->inti - static_cast<uint8_t>(ioapic->gsiBase);
    } else {
        entry = static_cast<uint8_t>(gsi) - static_cast<uint8_t>(ioapic->gsiBase);
    }

    // NOTE: Interrupts have to be disabled beforehand
    // The first register is the low DW, the second register is the high DW
    uint32_t low = readDoubleWord(ioapic, Indirect_Register::REDTBL + 2 * entry);
    uint64_t high = readDoubleWord(ioapic, Indirect_Register::REDTBL + 2 * entry + 1);
    return REDTBLEntry(low | high << 32);
}

// TODO: Spinlock?
// TODO: Should this be so complicated? The remapping stuff could be taken out and the table entry passed...
void IoApic::writeREDTBL(IoApicInformation *ioapic, GlobalSystemInterrupt gsi, REDTBLEntry redtbl) {
    verifyGSI(ioapic, gsi);

    if (InterruptModel::hasOverride(static_cast<InterruptInput>(static_cast<uint8_t>(gsi)))) {
        Util::Exception::throwException(Util::Exception::INVALID_ARGUMENT, "INTI belongs to other GSI!");
    }

    auto *override = InterruptModel::getInterruptOverride(gsi);
    uint8_t entry;
    if (override != nullptr) {
        entry = override->inti - static_cast<uint8_t>(ioapic->gsiBase);
    } else {
        entry = static_cast<uint8_t>(gsi) - static_cast<uint8_t>(ioapic->gsiBase);
    }

    // NOTE: Interrupts have to be disabled beforehand
    // The first register is the low DW, the second register is the high DW
    auto val = static_cast<uint64_t>(redtbl);
    writeDoubleWord(ioapic, Indirect_Register::REDTBL + 2 * entry, val & 0xFFFFFFFF);
    writeDoubleWord(ioapic, Indirect_Register::REDTBL + 2 * entry + 1, val >> 32);
}

}