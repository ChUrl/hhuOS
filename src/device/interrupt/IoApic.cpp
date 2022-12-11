#include "IoApic.h"
#include "kernel/system/System.h"
#include "device/interrupt/LApic.h"

namespace Device {

bool IoApic::initialized = false;

IoApic::IoPlatformConfiguration IoApic::platformConfiguration;

Kernel::Logger IoApic::log = Kernel::Logger::get("IoApic");


bool IoApic::isInitialized() {
    return initialized;
}

void IoApic::initialize() {
    if (!Acpi::isAvailable()) {
        Util::Exception::throwException(Util::Exception::UNSUPPORTED_OPERATION, "LApic::initialize(): ACPI support not present!");
    }
    if (!LApic::isInitialized()) {
        Util::Exception::throwException(Util::Exception::ILLEGAL_STATE, "IoApic::initialize(): Local APIC is not initialized!");
    }

    initializePlatformConfiguration();

    for (auto *ioapic : platformConfiguration.ioapics) {
        initializeController(ioapic);
    }

    initialized = true;

#if HHUOS_IOAPIC_ENABLE_DEBUG == 1
    logDebugDump();
#endif
}

// TODO: Need to set the IO APIC ID in the ID register (it's set to 0 on power up)
//       Copy the value from ACPI? Or is this value 0 aswell?
void IoApic::initializeController(IoApicConfiguration *ioapic) {
    auto &memoryService = Kernel::System::getService<Kernel::MemoryService>();
    void *virtAddress = memoryService.mapIO(ioapic->address, Util::Memory::PAGESIZE, true);

    if (virtAddress == nullptr) {
        Util::Exception::throwException(Util::Exception::OUT_OF_MEMORY,
                                        "IoApic::initializeController(): Not enough space left on kernel heap!");
    }

    // Use this addresses to access this IO APIC's memory mapped registers
    ioapic->virtAddress = reinterpret_cast<uint32_t>(virtAddress) + ioapic->address % Util::Memory::PAGESIZE;

    // NOTE: This value is overwritten for each IO APIC but this doesn't matter as they are equal
    // https://github.com/torvalds/linux/blob/master/arch/x86/kernel/apic/io_apic.c#L470
    platformConfiguration.version = readDoubleWord(ioapic, Indirect_Register::VER) & 0xFF;
    platformConfiguration.needsCompatibilityEOI = platformConfiguration.version < 0x20;

    // NOTE: With the IRQPA there is a way to address more than 255 GSIs although maxREDTBLEntries only has 8 bits
    // NOTE: With ICH5 (and other ICHs?) it is always 24
    ioapic->redtblEntries = ((readDoubleWord(ioapic, Indirect_Register::VER) >> 16) & 0xFF) + 1;
    initializeREDTBL(ioapic);
}

void IoApic::allow(Interrupt gsi) {
    IoApicConfiguration *ioapic = getIoApicConfiguration(gsi);

    REDTBLEntry entry = readREDTBL(ioapic, gsi);
    entry.isMasked = false;
    writeREDTBL(ioapic, gsi, entry);
}

void IoApic::allow(Pic::Interrupt irq) {
    // TODO: Don't use currently running CPU for all interrupts
    allow(static_cast<Interrupt>(platformConfiguration.irqToGsiMappings[static_cast<uint8_t>(irq)]));
}

void IoApic::forbid(Interrupt gsi) {
    IoApicConfiguration *ioapic = getIoApicConfiguration(gsi);

    REDTBLEntry entry = readREDTBL(ioapic, gsi);
    entry.isMasked = true;
    writeREDTBL(ioapic, gsi, entry);
}

void IoApic::forbid(Pic::Interrupt irq) {
    if (irq == Pic::Interrupt::CASCADE) {
        return;
    }

    forbid(static_cast<Interrupt>(platformConfiguration.irqToGsiMappings[static_cast<uint8_t>(irq)]));
}

bool IoApic::status(Interrupt gsi) {
    IoApicConfiguration *ioapic = getIoApicConfiguration(gsi);

    return readREDTBL(ioapic, gsi).isMasked;
}

// NOTE: Do not allocate memory here, that also means no iterators!
// TODO: Kind of suboptimal that for every EOI the corresponding ioapic config has to be searched?
// TODO: Compatibility mode
// Intel ICH5 Datasheet Chapter 9.5.5
void IoApic::sendEndOfInterrupt(Kernel::InterruptDispatcher::Interrupt vector) {
    IoApicConfiguration *ioapic = getIoApicConfiguration(vector);

    volatile auto *regAddr = reinterpret_cast<uint32_t *>(ioapic->virtAddress + Register::EOI);
    *regAddr = static_cast<uint8_t>(vector);
}

// ! Private member functions start here

void IoApic::initializePlatformConfiguration() {
    // Default is identity mapping
    for (uint8_t irq = 0; irq < 16; ++irq) {
        platformConfiguration.irqToGsiMappings[irq] = irq;
        platformConfiguration.gsiToIrqMappings[irq] = irq;
    }

    Util::Data::ArrayList<const Acpi::IoApic *> ioApics;
    Util::Data::ArrayList<const Acpi::InterruptSourceOverride *> interruptSourceOverrides;
    Util::Data::ArrayList<const Acpi::NMISource *> nmiConfigurations;
    Acpi::getApicStructures(&ioApics, Acpi::IO_APIC);
    Acpi::getApicStructures(&interruptSourceOverrides, Acpi::INTERRUPT_SOURCE_OVERRIDE);
    Acpi::getApicStructures(&nmiConfigurations, Acpi::NON_MASKABLE_INTERRUPT_SOURCE);

    if (ioApics.size() == 0) {
        Util::Exception::throwException(Util::Exception::ILLEGAL_STATE,
                                        "IoApic::initializePlatformConfiguration(): Didn't find IO APIC(s)!");
    }
    // TODO: I think nmiConfigurations are optional for the IO APIC?

    for (auto ioapic : ioApics) {
        platformConfiguration.ioapics.add(new IoApicConfiguration {
            .id = ioapic->ioApicId,
            .address = ioapic->ioApicAddress,
            .gsiBase = ioapic->globalSystemInterruptBase
        });
    }

    for (auto irqOverride : interruptSourceOverrides) {
        platformConfiguration.irqOverrides.add(new InterruptOverride {
            .bus = irqOverride->bus,
            .source = static_cast<Pic::Interrupt>(irqOverride->source),
            .gsi = irqOverride->globalSystemInterrupt,
            .polarity = irqOverride->flags & Acpi::IntiFlag::ACTIVE_HIGH ? REDTBLPinPolarity::HIGH : REDTBLPinPolarity::LOW,
            .triggerMode = irqOverride->flags & Acpi::IntiFlag::EDGE_TRIGGERED ? REDTBLTriggerMode::EDGE : REDTBLTriggerMode::LEVEL
        });

        // TODO: Remove old entry with the destination another entry was remapped to?
        //       If PIT gets remapped from GSI 0 to 2, irqMappings[2] and gsiMappings[2] also point to 2
        // Update mappings
        platformConfiguration.irqToGsiMappings[irqOverride->source] = irqOverride->globalSystemInterrupt;
        platformConfiguration.gsiToIrqMappings[irqOverride->globalSystemInterrupt] = irqOverride->source;
    }

    for (auto nmi : nmiConfigurations) {
        platformConfiguration.ionmis.add(new NMIConfiguration {
            .polarity = nmi->flags & Acpi::IntiFlag::ACTIVE_HIGH ? REDTBLPinPolarity::HIGH : REDTBLPinPolarity::LOW,
            .triggerMode = nmi->flags & Acpi::IntiFlag::EDGE_TRIGGERED ? REDTBLTriggerMode::EDGE : REDTBLTriggerMode::LEVEL,
            .gsi = nmi->globalSystemInterrupt
        });
    }
}

IoApic::IoApicConfiguration *IoApic::getIoApicConfiguration(Interrupt gsi) {
    // NOTE: Using an iterator here would allocate heap memory, don't do that!
    IoApicConfiguration *ioapic;
    for (uint32_t i = 0; i < platformConfiguration.ioapics.size(); ++i) {
        ioapic = platformConfiguration.ioapics.get(i);
        if (gsi >= ioapic->gsiBase && gsi <= ioapic->gsiBase + ioapic->redtblEntries) {
            return ioapic;
        }
    }

    Util::Exception::throwException(Util::Exception::ILLEGAL_STATE,
                                    "LApic::getIoApicConfiguration(): Didn't find configuration matching GSI!");
}

// NOTE: The GSIs and IRQs are not identity mapped
IoApic::IoApicConfiguration *IoApic::getIoApicConfiguration(Pic::Interrupt irq) {
    return getIoApicConfiguration(static_cast<Interrupt>(platformConfiguration.irqToGsiMappings[static_cast<uint8_t>(irq)]));
}

// NOTE: The GSIs and vectors are identity mapped, translated by 32
IoApic::IoApicConfiguration *IoApic::getIoApicConfiguration(Kernel::InterruptDispatcher::Interrupt vector) {
    return getIoApicConfiguration(static_cast<Interrupt>(vector - 32));
}

// TODO: Can a single IO APIC have multiple NMIs?
IoApic::NMIConfiguration *IoApic::getNMIConfiguration(IoApicConfiguration *ioapic) {
    uint32_t gsiBase = ioapic->gsiBase;
    uint32_t gsiEnd = gsiBase + ioapic->redtblEntries;

    // Check if a NMI exists assigned to one of this IO APICs pins
    for (auto *nmi : platformConfiguration.ionmis) {
        if (nmi->gsi >= gsiBase && nmi->gsi < gsiEnd) {
            return nmi;
        }
    }

    // TODO: Does not need to exist, so don't throw up (use/implement optional?)
    Util::Exception::throwException(Util::Exception::ILLEGAL_STATE,
                                    "LApic::getNMIConfiguration(): Didn't find configuration matching ID!");
}

// TODO: Spinlock?
// TODO: Use Indirect_Register type and overload the name with a function accepting Register type
uint32_t IoApic::readDoubleWord(IoApicConfiguration *ioapic, uint8_t reg) {
    if (ioapic->virtAddress == 0) {
        Util::Exception::throwException(Util::Exception::NULL_POINTER,
                                        "IoApic::readDoubleWord(): IoApic MMIO not initialized!");
    }

    volatile auto *indAddr = reinterpret_cast<uint8_t *>(ioapic->virtAddress + Register::IND);
    volatile auto *datAddr = reinterpret_cast<uint32_t *>(ioapic->virtAddress + Register::DAT);

    // NOTE: Interrupts have to be disabled beforehand
    *indAddr = static_cast<uint8_t>(reg); // Select register
    return *datAddr; // Read value indirectly
}

// TODO: Spinlock?
// TODO: Use Indirect_Register type
void IoApic::writeDoubleWord(IoApicConfiguration *ioapic, uint8_t reg, uint32_t val) {
    if (ioapic->virtAddress == 0) {
        Util::Exception::throwException(Util::Exception::NULL_POINTER,
                                        "IoApic::readDoubleWord(): IoApic MMIO not initialized!");
    }

    volatile auto *indAddr = reinterpret_cast<uint8_t *>(ioapic->virtAddress + Register::IND);
    volatile auto *datAddr = reinterpret_cast<uint32_t *>(ioapic->virtAddress + Register::DAT);

    // NOTE: Interrupts have to be disabled beforehand
    *indAddr = static_cast<uint8_t>(reg); // Select register
    *datAddr = val; // Write value indirectly
}

// TODO: Spinlock?
// TODO: Use Interrupt type
REDTBLEntry IoApic::readREDTBL(IoApicConfiguration *ioapic, uint8_t gsi) {
    if (gsi < ioapic->gsiBase || gsi >= ioapic->gsiBase + ioapic->redtblEntries) {
        Util::Exception::throwException(Util::Exception::INVALID_ARGUMENT,
                                        "IoApic::readREDTBL(): GSI is handled by different IO APIC!");
    }

    // NOTE: Interrupts have to be disabled beforehand
    // The first register is the low DW, the second register is the high DW
    uint8_t entry = gsi - ioapic->gsiBase;
    uint32_t low, high;
    low = readDoubleWord(ioapic, Indirect_Register::REDTBL + 2 * entry);
    high = readDoubleWord(ioapic, Indirect_Register::REDTBL + 2 * entry + 1);

    // Intel ICH5 Datasheet Chapter 9.5.8
    return {
            .vector = static_cast<Kernel::InterruptDispatcher::Interrupt>(low & 0xFF),
            .deliveryMode = static_cast<REDTBLDeliveryMode>((low & (0b111 << 8)) >> 8), // Mask is <<, result is shifted back
            .destinationMode = static_cast<REDTBLDestinationMode>((low & (1 << 11)) >> 11),
            .deliveryStatus = static_cast<REDTBLDeliveryStatus>((low & (1 << 12)) >> 12),
            .pinPolarity = static_cast<REDTBLPinPolarity>((low & (1 << 13)) >> 13),
            .triggerMode = static_cast<REDTBLTriggerMode>((low & (1 << 15)) >> 15),
            .isMasked = static_cast<bool>(low & (1 << 16)),
            .destination = static_cast<uint8_t>(high >> 24)
    };
}

// TODO: Spinlock?
// TODO: Use Interrupt type
void IoApic::writeREDTBL(IoApicConfiguration *ioapic, uint8_t gsi, REDTBLEntry redtbl) {
    if (gsi < ioapic->gsiBase || gsi >= ioapic->gsiBase + ioapic->redtblEntries) {
        Util::Exception::throwException(Util::Exception::INVALID_ARGUMENT,
                                        "IoApic::readREDTBL(): GSI is handled by different IO APIC!");
    }

    // Intel ICH5 Datasheet Chapter 9.5.8
    uint32_t low, high;
    low = static_cast<uint32_t>(redtbl.vector)
          | static_cast<uint32_t>(redtbl.deliveryMode) << 8
          | static_cast<uint32_t>(redtbl.destinationMode) << 11
          | static_cast<uint32_t>(redtbl.pinPolarity) << 13
          | static_cast<uint32_t>(redtbl.triggerMode) << 15
          | static_cast<uint32_t>(redtbl.isMasked) << 16;
    high = redtbl.destination << 24;

    // NOTE: Interrupts have to be disabled beforehand
    // The first register is the low DW, the second register is the high DW
    uint8_t entry = gsi - ioapic->gsiBase;
    writeDoubleWord(ioapic, Indirect_Register::REDTBL + 2 * entry, low);
    writeDoubleWord(ioapic, Indirect_Register::REDTBL + 2 * entry + 1, high);
}

void IoApic::initializeREDTBL(IoApicConfiguration *ioapic) {
    uint8_t id = LApic::getId(); // TODO: Don't send all interrupts to one cpu

    for (uint32_t entry = 0; entry < ioapic->redtblEntries; ++entry) {
        uint8_t gsi = entry + ioapic->gsiBase;

        Kernel::InterruptDispatcher::Interrupt vector;
        if (gsi <= Pic::Interrupt::SECONDARY_ATA) {
            // GSIs 0 to 15 are mapped depending on the InterruptOverride structures
            vector = static_cast<Kernel::InterruptDispatcher::Interrupt>(platformConfiguration.gsiToIrqMappings[gsi] + 32);
        } else {
            // Rest of GSIs are identity mapped but translated by 32
            vector = static_cast<Kernel::InterruptDispatcher::Interrupt>(gsi + 32);
        }

        // NOTE: Interrupts have to be disabled beforehand
        writeREDTBL(ioapic, gsi, {
                .vector = vector,
                .deliveryMode = REDTBLDeliveryMode::FIXED, // TODO
                .destinationMode = REDTBLDestinationMode::PHYSICAL, // TODO
                .pinPolarity = REDTBLPinPolarity::HIGH,
                .triggerMode = REDTBLTriggerMode::EDGE,
                .isMasked = true,
                .destination = id
        });
    }

}

#if HHUOS_IOAPIC_ENABLE_DEBUG == 1

// TODO
void IoApic::logDebugDump() {
}

#endif

}