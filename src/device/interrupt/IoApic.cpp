#include "IoApic.h"
#include "kernel/system/System.h"
#include "device/interrupt/LApic.h"

namespace Device {

bool IoApic::initialized = false;
IoApic::IoPlatformConfiguration IoApic::platformConfiguration;
Kernel::Logger IoApic::log = Kernel::Logger::get("IoApic");

// TODO: Should I arbitrarily assign IDs like this or does ACPI already contain valid IDs?
uint8_t ioApicId = 0; // IoApics don't start with an assigned id

// ! Public member functions start here

bool IoApic::isInitialized() {
    return initialized;
}

void IoApic::initialize() {
    if (!LApic::isInitialized()) {
        Util::Exception::throwException(Util::Exception::ILLEGAL_STATE, "IoApic::initialize(): Local APIC is not initialized!");
    }

    initializePlatformConfiguration();

    // Initialize all IO APICs of the system
    for (auto *ioapic : platformConfiguration.ioapics) {
        initializeController(ioapic);
    }

    initialized = true;

#if HHUOS_IOAPIC_ENABLE_DEBUG == 1
    dumpIoPlatformConfiguration();
#endif
}

uint8_t IoApic::getSystemMaxGsi() {
    return platformConfiguration.systemGsiMax;
}

void IoApic::allow(uint8_t gsi) {
    IoApicConfiguration *ioapic = getIoApicConfiguration(gsi);

    REDTBLEntry entry = readREDTBL(ioapic, gsi);
    entry.isMasked = false;
    writeREDTBL(ioapic, gsi, entry);
}

void IoApic::allow(Pic::Interrupt irq) {
    allow(platformConfiguration.irqToGsiMappings[static_cast<uint8_t>(irq)]);
}

void IoApic::forbid(uint8_t gsi) {
    IoApicConfiguration *ioapic = getIoApicConfiguration(gsi);

    REDTBLEntry entry = readREDTBL(ioapic, gsi);
    entry.isMasked = true;
    writeREDTBL(ioapic, gsi, entry);
}

void IoApic::forbid(Pic::Interrupt irq) {
    forbid(platformConfiguration.irqToGsiMappings[static_cast<uint8_t>(irq)]);
}

bool IoApic::status(uint8_t gsi) {
    IoApicConfiguration *ioapic = getIoApicConfiguration(gsi);

    return readREDTBL(ioapic, gsi).isMasked;
}

bool IoApic::status(Pic::Interrupt irq) {
    return status(platformConfiguration.irqToGsiMappings[static_cast<uint8_t>(irq)]);
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
    if (!Acpi::isAvailable()) {
        Util::Exception::throwException(Util::Exception::UNSUPPORTED_OPERATION, "LApic::initialize(): ACPI support not present!");
    }

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
        Util::Exception::throwException(Util::Exception::ILLEGAL_STATE, "IoApic::initializePlatformConfiguration(): Didn't find IO APIC(s)!");
    }

    for (auto ioapic : ioApics) {
        platformConfiguration.ioapics.add(new IoApicConfiguration {
            .id = ioapic->ioApicId,
            .address = ioapic->ioApicAddress,
            .gsiBase = ioapic->globalSystemInterruptBase
        });
    }

    for (auto override : interruptSourceOverrides) {
        platformConfiguration.irqOverrides.add(new IoInterruptOverride {
            .bus = override->bus,
            .source = static_cast<Pic::Interrupt>(override->source),
            .gsi = override->globalSystemInterrupt,
            .polarity = override->flags & Acpi::IntiFlag::ACTIVE_HIGH ? REDTBLPinPolarity::HIGH : REDTBLPinPolarity::LOW,
            .triggerMode = override->flags & Acpi::IntiFlag::EDGE_TRIGGERED ? REDTBLTriggerMode::EDGE : REDTBLTriggerMode::LEVEL
        });

        // TODO: Remove old entry with the destination another entry was remapped to?
        //       If PIT gets remapped from GSI 0 to 2, irqMappings[2] and gsiMappings[2] also point to 2
        // Update mappings
        platformConfiguration.irqToGsiMappings[override->source] = override->globalSystemInterrupt;
        platformConfiguration.gsiToIrqMappings[override->globalSystemInterrupt] = override->source;
    }

    for (auto nmi : nmiConfigurations) {
        platformConfiguration.ionmis.add(new IoNMIConfiguration {
            .polarity = nmi->flags & Acpi::IntiFlag::ACTIVE_HIGH ? REDTBLPinPolarity::HIGH : REDTBLPinPolarity::LOW,
            .triggerMode = nmi->flags & Acpi::IntiFlag::EDGE_TRIGGERED ? REDTBLTriggerMode::EDGE : REDTBLTriggerMode::LEVEL,
            .gsi = nmi->globalSystemInterrupt
        });
    }
}

IoApic::IoApicConfiguration *IoApic::getIoApicConfiguration(uint8_t gsi) {
    // NOTE: Using an iterator here would allocate heap memory, don't do that!
    // NOTE: (Can't allocate heap memory while sending EOI)
    IoApicConfiguration *ioapic;
    for (uint32_t i = 0; i < platformConfiguration.ioapics.size(); ++i) {
        ioapic = platformConfiguration.ioapics.get(i);
        if (gsi >= ioapic->gsiBase && gsi <= ioapic->gsiMax) {
            return ioapic;
        }
    }

    Util::Exception::throwException(Util::Exception::ILLEGAL_STATE, "LApic::getIoApicConfiguration(): Didn't find configuration matching GSI!");
}

// NOTE: The GSIs and IRQs are not identity mapped
IoApic::IoApicConfiguration *IoApic::getIoApicConfiguration(Pic::Interrupt irq) {
    return getIoApicConfiguration(platformConfiguration.irqToGsiMappings[static_cast<uint8_t>(irq)]);
}

// NOTE: The GSIs and vectors are identity mapped, translated by 32
IoApic::IoApicConfiguration *IoApic::getIoApicConfiguration(Kernel::InterruptDispatcher::Interrupt vector) {
    return getIoApicConfiguration(vector - 32);
}

// TODO: Can a single IO APIC have multiple NMIs?
IoApic::IoNMIConfiguration *IoApic::getNMIConfiguration(IoApicConfiguration *ioapic) {
    // Check if a NMI exists assigned to one of this IO APICs pins
    for (auto *nmi : platformConfiguration.ionmis) {
        if (nmi->gsi >= ioapic->gsiBase && nmi->gsi <= ioapic->gsiMax) {
            return nmi;
        }
    }

    return nullptr; // NMI is optional for IO APIC
}

// TODO: Need to set the IO APIC ID in the ID register (it's set to 0 on power up)
//       Copy the value from ACPI? Or is this value 0 aswell?
void IoApic::initializeController(IoApicConfiguration *ioapic) {
    initializeMMIORegion(ioapic);

    // NOTE: These are overwritten for each IO APIC but this doesn't matter as they are equal
    // https://github.com/torvalds/linux/blob/master/arch/x86/kernel/apic/io_apic.c#L470
    platformConfiguration.version = readDoubleWord(ioapic, Indirect_Register::VER) & 0xFF;
    platformConfiguration.hasEOIRegister = platformConfiguration.version >= 0x20;

    // NOTE: With the IRQPA there is a way to address more than 255 GSIs although maxREDTBLEntries only has 8 bits
    // NOTE: With ICH5 (and other ICHs?) it is always 24
    ioapic->gsiMax = ioapic->gsiBase + (readDoubleWord(ioapic, Indirect_Register::VER) >> 16) & 0xFF;
    platformConfiguration.systemGsiMax = ioapic->gsiMax > platformConfiguration.systemGsiMax ? ioapic->gsiMax : platformConfiguration.systemGsiMax;

    initializeREDTBL(ioapic);

    // Configure NMI if it exists
    auto *nmi = getNMIConfiguration(ioapic);
    if (nmi != nullptr) {
        writeREDTBL(ioapic, nmi->gsi, {.vector = static_cast<Kernel::InterruptDispatcher::Interrupt>(0),
                                       .deliveryMode = REDTBLDeliveryMode::NMI,
                                       .destinationMode = REDTBLDestinationMode::PHYSICAL, // TODO: What to set here?
                                       .pinPolarity = nmi->polarity,
                                       .triggerMode = nmi->triggerMode,
                                       .isMasked = false,
                                       .destination = LApic::getId()});
    }
}

void IoApic::initializeMMIORegion(IoApicConfiguration *ioapic) {
    auto &memoryService = Kernel::System::getService<Kernel::MemoryService>();
    void *virtAddress = memoryService.mapIO(ioapic->address, Util::Memory::PAGESIZE, true);

    if (virtAddress == nullptr) {
        Util::Exception::throwException(Util::Exception::OUT_OF_MEMORY, "IoApic::initializeController(): Not enough space left on kernel heap!");
    }

    // Use this addresses to access this IO APIC's memory mapped registers
    ioapic->virtAddress = reinterpret_cast<uint32_t>(virtAddress) + ioapic->address % Util::Memory::PAGESIZE;
}

// TODO: I read that GSI0 is typically used for ExtINT (PIC), if I can verify that the GSI0 can always be masked
//       as I don't support virtual wire
void IoApic::initializeREDTBL(IoApicConfiguration *ioapic) {
    uint8_t id = LApic::getId(); // TODO: Don't send all interrupts to one cpu

    for (uint32_t gsi = ioapic->gsiBase; gsi <= ioapic->gsiMax; ++gsi) {
        Kernel::InterruptDispatcher::Interrupt vector;
        if (gsi <= Pic::Interrupt::SECONDARY_ATA) {
            // GSIs 0 to 15 are mapped depending on the InterruptOverride structures
            vector = static_cast<Kernel::InterruptDispatcher::Interrupt>(platformConfiguration.gsiToIrqMappings[gsi] + 32);
        } else {
            // Rest of GSIs are identity mapped but translated by 32
            vector = static_cast<Kernel::InterruptDispatcher::Interrupt>(gsi + 32);
        }

        // NOTE: Interrupts have to be disabled beforehand
        writeREDTBL(ioapic, gsi, {.vector = vector,
                                  .deliveryMode = REDTBLDeliveryMode::FIXED, // TODO
                                  .destinationMode = REDTBLDestinationMode::PHYSICAL, // TODO
                                  .pinPolarity = REDTBLPinPolarity::HIGH,
                                  .triggerMode = REDTBLTriggerMode::EDGE,
                                  .isMasked = true,
                                  .destination = id});
    }
}

void IoApic::dumpIoPlatformConfiguration() {
    log.info("Version: [0x%x]", platformConfiguration.version);
    log.info("Has EOI register: [%d]", platformConfiguration.hasEOIRegister);
    log.info("System max GSI: [%d]", platformConfiguration.systemGsiMax);

    log.info("Io Apic status:");
    for (auto *ioapic : platformConfiguration.ioapics) {
        log.info("- Id: [0x%x], MMIO: [0x%x] (phys) -> [0x%x] (virt), GSI base: [%d], GSI max: [%d]",
                 ioapic->id, ioapic->address, ioapic->virtAddress, ioapic->gsiBase, ioapic->gsiMax);
    }

    log.info("Io IRQ overrides:");
    for (auto *override : platformConfiguration.irqOverrides) {
        log.info("- IRQ source: [%d], GSI target: [%d]", override->source, override->gsi);
    }
    if (platformConfiguration.irqOverrides.size() == 0) {
        log.info("- There are no IRQ overrides");
    }

    log.info("Io NMI status:");
    for (auto *nmi : platformConfiguration.ionmis) {
        log.info("- GSI: [%d]", nmi->gsi);
    }
    if (platformConfiguration.ionmis.size() == 0) {
        log.info("- There are no IO NMIs");
    }
}

// ! Private register member functions start here

// TODO: Spinlock?
// TODO: Use Indirect_Register type and overload the name with a function accepting Register type
uint32_t IoApic::readDoubleWord(IoApicConfiguration *ioapic, uint8_t reg) {
    if (ioapic->virtAddress == 0) {
        Util::Exception::throwException(Util::Exception::NULL_POINTER, "IoApic::readDoubleWord(): IoApic MMIO not initialized!");
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
        Util::Exception::throwException(Util::Exception::NULL_POINTER, "IoApic::readDoubleWord(): IoApic MMIO not initialized!");
    }

    volatile auto *indAddr = reinterpret_cast<uint8_t *>(ioapic->virtAddress + Register::IND);
    volatile auto *datAddr = reinterpret_cast<uint32_t *>(ioapic->virtAddress + Register::DAT);

    // NOTE: Interrupts have to be disabled beforehand
    *indAddr = static_cast<uint8_t>(reg); // Select register
    *datAddr = val; // Write value indirectly
}

// TODO: Spinlock?
REDTBLEntry IoApic::readREDTBL(IoApicConfiguration *ioapic, uint8_t gsi) {
    if (gsi < ioapic->gsiBase || gsi > ioapic->gsiMax) {
        Util::Exception::throwException(Util::Exception::INVALID_ARGUMENT, "IoApic::readREDTBL(): GSI is handled by different IO APIC!");
    }

    // NOTE: Interrupts have to be disabled beforehand
    // The first register is the low DW, the second register is the high DW
    uint8_t entry = gsi - ioapic->gsiBase;
    uint32_t low, high;
    low = readDoubleWord(ioapic, Indirect_Register::REDTBL + 2 * entry);
    high = readDoubleWord(ioapic, Indirect_Register::REDTBL + 2 * entry + 1);

    // Intel ICH5 Datasheet Chapter 9.5.8
    return {.vector = static_cast<Kernel::InterruptDispatcher::Interrupt>(low & 0xFF),
            .deliveryMode = static_cast<REDTBLDeliveryMode>((low & (0b111 << 8)) >> 8),
            .destinationMode = static_cast<REDTBLDestinationMode>((low & (1 << 11)) >> 11),
            .deliveryStatus = static_cast<REDTBLDeliveryStatus>((low & (1 << 12)) >> 12),
            .pinPolarity = static_cast<REDTBLPinPolarity>((low & (1 << 13)) >> 13),
            .triggerMode = static_cast<REDTBLTriggerMode>((low & (1 << 15)) >> 15),
            .isMasked = static_cast<bool>(low & (1 << 16)),
            .destination = static_cast<uint8_t>(high >> 24)};
}

// TODO: Spinlock?
// TODO: Use Interrupt type
void IoApic::writeREDTBL(IoApicConfiguration *ioapic, uint8_t gsi, REDTBLEntry redtbl) {
    if (gsi < ioapic->gsiBase || gsi > ioapic->gsiMax) {
        Util::Exception::throwException(Util::Exception::INVALID_ARGUMENT, "IoApic::readREDTBL(): GSI is handled by different IO APIC!");
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

}