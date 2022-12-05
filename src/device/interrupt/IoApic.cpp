#include "IoApic.h"
#include "kernel/system/System.h"
#include "kernel/paging/Paging.h"
#include "device/interrupt/LApic.h"

namespace Device {

bool IoApic::initialized = false;
uint8_t IoApic::version = 0;
uint8_t IoApic::maxREDTBLEntry = 0;
uint32_t IoApic::baseVirtAddress = 0;
Kernel::Logger IoApic::log = Kernel::Logger::get("IoApic");

/*
 * Public Member Functions
 */

bool IoApic::isInitialized() {
    return initialized;
}

uint8_t IoApic::getId() {
    return (readDoubleWord(Indirect_Register::ID) >> 24) & 0xFF;
}

uint8_t IoApic::getVersion() {
    // Save the version as it could potentially be queried for every EOI
    if (!initialized) {
        return readDoubleWord(Indirect_Register::VER) & 0xFF;
    }

    return version;
}

// https://github.com/torvalds/linux/blob/master/arch/x86/kernel/apic/io_apic.c#L470
bool IoApic::hasEOIRegister() {
    return getVersion() >= 0x20;
}

void IoApic::init() {
    // TODO: Check if IO APIC supported (is this possible without MP/ACPI?)

    // TODO: Does the LApic even have to be initialized before?
    if (!LApic::isInitialized()) {
        Util::Exception::throwException(Util::Exception::ILLEGAL_STATE,
                                        "IoApic::init(): Local APIC is not initialized!");
    }

    auto& memoryService = Kernel::System::getService<Kernel::MemoryService>();
    auto& pageDirectory = memoryService.getKernelAddressSpace().getPageDirectory();

    // TODO: The IO APIC MMIO registers could cross a page boundary and don't have to be 4kb aligned
    //       If unaligned add the offset to virtAddress (virtAddress += physAddress % PAGESIZE)
    //       If page boundary crossed allocate 2 pages
    // NOTE: The default physical address is 4kb aligned and thus doesn't cross pages
    // NOTE: IO memory region is 0xEEE00000 - 0xFFFFFFFF, so it contains the default physical address
    // NOTE: (https://hhuos.github.io/docs/paging_mm#the-virtual-memory-layout-in-hhuos)
    void* virtAddress = memoryService.mapIO(IOAPIC_BASE_DEFAULT_PHYS_ADDRESS, Util::Memory::PAGESIZE, true);

    if (virtAddress == nullptr) {
        Util::Exception::throwException(Util::Exception::OUT_OF_MEMORY,
                                        "IoApic::init(): Not enough space left on kernel heap!");
    }

    // TODO: How do I check that the mapping was successful?
    // TODO: Necessary with mapIO?
    pageDirectory.setPageFlags(reinterpret_cast<uint32_t>(virtAddress),
                               Kernel::Paging::PRESENT | Kernel::Paging::DO_NOT_UNMAP
                               | Kernel::Paging::CACHE_DISABLE | Kernel::Paging::WRITE_THROUGH
                               | Kernel::Paging::READ_WRITE);

    // Use this addresses to access the IO APIC's memory mapped registers
    baseVirtAddress = reinterpret_cast<uint32_t>(virtAddress);

    maxREDTBLEntry = getMaxREDTBLEntry();
    version = getVersion();

    // TODO: Check possible valid IRQ amounts, for now we need at least 16 for PIC compatibility
    if (maxREDTBLEntry < 15) {
        Util::Exception::throwException(Util::Exception::ILLEGAL_STATE,
                                        "ioApic::init(): IO APIC needs to support at least 16 IRQs!");
    }

    // Initialize legacy IRQs, IRQ2 is PIC master-slave cascade and thus is ignored
    // TODO: To know if PIC IRQs are mapped 1:1 to IO APIC IRQs MP/ACPI tables have to be parsed
    initREDTBL();

    initialized = true;

#if HHUOS_IOAPIC_ENABLE_DEBUG == 1
    logDebugDump();
#endif
}

// TODO: Do I have to check if 0 <= gsi <= 23? Or is this excessive
void IoApic::allow(Interrupt gsi, uint8_t destination) {
    REDTBL_Entry entry = readREDTBL(gsi);
    entry.isMasked = false;
    entry.destination = destination;
    writeREDTBL(gsi, entry);
}

void IoApic::allow(Interrupt gsi) {
    allow(gsi, LApic::getId()); // Currently running CPU as destination
}

void IoApic::allow(Pic::Interrupt irq, uint8_t destination) {
    if (irq == Pic::Interrupt::CASCADE) {
        return;
    }

    // TODO: SECONDARY_ATA stops the system from booting (time stops increasing, probably EOI bug?)
    if (irq == Pic::Interrupt::SECONDARY_ATA) {
        return;
    }

    allow(getIrqToGsiMapping(irq), destination);
}

void IoApic::allow(Pic::Interrupt irq) {
    allow(getIrqToGsiMapping(irq), LApic::getId()); // Currently running CPU as destination
}

void IoApic::forbid(Interrupt gsi) {
    REDTBL_Entry entry = readREDTBL(gsi);
    entry.isMasked = true;
    writeREDTBL(gsi, entry);
}

void IoApic::forbid(Pic::Interrupt irq) {
    if (irq == Pic::Interrupt::CASCADE) {
        return;
    }

    forbid(getIrqToGsiMapping(irq));
}

bool IoApic::status(Interrupt gsi) {
    return readREDTBL(gsi).isMasked;
}

// Intel ICH5 Datasheet Chapter 9.5.5
void IoApic::sendEndOfInterrupt(Kernel::InterruptDispatcher::Interrupt interrupt) {
    // TODO: Check if EOI register supported? But don't access version register every EOI...
    volatile auto* regAddr = reinterpret_cast<uint32_t*>(baseVirtAddress + Register::EOI);
    *regAddr = static_cast<uint8_t>(interrupt);
}

/*
 * Private Member Functions
 */

uint8_t IoApic::getMaxREDTBLEntry() {
    if (!initialized) {
        return (readDoubleWord(Indirect_Register::VER) >> 16) & 0xFF;
    }

    return maxREDTBLEntry;
}

uint32_t IoApic::readDoubleWord(uint8_t reg) {
    if (baseVirtAddress == 0) {
        Util::Exception::throwException(Util::Exception::NULL_POINTER,
                                        "IoApic::readDoubleWord(): IoApic MMIO not initialized!");
    }

    volatile auto* indAddr = reinterpret_cast<uint8_t*>(baseVirtAddress + Register::IND);
    volatile auto* datAddr = reinterpret_cast<uint32_t*>(baseVirtAddress + Register::DAT);

    // NOTE: Interrupts have to be disabled beforehand
    *indAddr = static_cast<uint8_t>(reg); // Select register
    return *datAddr; // Read value indirectly
}

void IoApic::writeDoubleWord(uint8_t reg, uint32_t val) {
    if (baseVirtAddress == 0) {
        Util::Exception::throwException(Util::Exception::NULL_POINTER,
                                        "IoApic::readDoubleWord(): IoApic MMIO not initialized!");
    }

    volatile auto* indAddr = reinterpret_cast<uint8_t*>(baseVirtAddress + Register::IND);
    volatile auto* datAddr = reinterpret_cast<uint32_t*>(baseVirtAddress + Register::DAT);

    // NOTE: Interrupts have to be disabled beforehand
    *indAddr = static_cast<uint8_t>(reg); // Select register
    *datAddr = val; // Write value indirectly
}

IoApic::REDTBL_Entry IoApic::readREDTBL(uint8_t gsi) {
    if (gsi > maxREDTBLEntry) {
        Util::Exception::throwException(Util::Exception::INVALID_ARGUMENT,
                                        "IoApic::readREDTBL(): REDTBL only has 24 entries!");
    }

    // NOTE: Interrupts have to be disabled beforehand
    // The first register is the low DW, the second register is the high DW
    uint32_t low, high;
    low = readDoubleWord(Indirect_Register::REDTBL + 2 * gsi);
    high = readDoubleWord(Indirect_Register::REDTBL + 2 * gsi + 1);

    // Intel ICH5 Datasheet Chapter 9.5.8
    return {
        .slot = static_cast<Kernel::InterruptDispatcher::Interrupt>(low & 0xFF),
        .deliveryMode = static_cast<Delivery_Mode>((low & (0b111 << 8)) >> 8), // Mask is <<, result is shifted back
        .destinationMode = static_cast<Destination_Mode>((low & (1 << 11)) >> 11),
        .deliveryStatus = static_cast<Delivery_Status>((low & (1 << 12)) >> 12),
        .pinPolarity = static_cast<Pin_Polarity>((low & (1 << 13)) >> 13),
        .triggerMode = static_cast<Trigger_Mode>((low & (1 << 15)) >> 15),
        .isMasked = static_cast<bool>((low & (1 << 16)) >> 16),
        .destination = static_cast<uint8_t>(high >> 24)
    };
}

void IoApic::writeREDTBL(uint8_t gsi, REDTBL_Entry entry) {
    if (gsi > maxREDTBLEntry) {
        Util::Exception::throwException(Util::Exception::INVALID_ARGUMENT,
                                        "IoApic::readREDTBL(): REDTBL only has 24 entries!");
    }

    // Intel ICH5 Datasheet Chapter 9.5.8
    uint32_t low, high;
    low = static_cast<uint32_t>(entry.slot)
            | static_cast<uint32_t>(entry.deliveryMode) << 8
            | static_cast<uint32_t>(entry.destinationMode) << 11
            | static_cast<uint32_t>(entry.pinPolarity) << 13
            | static_cast<uint32_t>(entry.triggerMode) << 15
            | static_cast<uint32_t>(entry.isMasked) << 16;
    high = entry.destination << 24;

    // NOTE: Interrupts have to be disabled beforehand
    // The first register is the low DW, the second register is the high DW
    writeDoubleWord(Indirect_Register::REDTBL + 2 * gsi, low);
    writeDoubleWord(Indirect_Register::REDTBL + 2 * gsi + 1, high);
}

// TODO: For real hardware the MP/ACPI tables have to be parsed to get the mapping
// NOTE: In QEMU the IRQ/GSI mappings match (except PIT)
// NOTE: (https://github.com/qemu/qemu/blob/master/hw/intc/ioapic.c#L153)
IoApic::Interrupt IoApic::getIrqToGsiMapping(Pic::Interrupt irq) {
    if (irq == Pic::Interrupt::CASCADE) {
        Util::Exception::throwException(Util::Exception::INVALID_ARGUMENT,
                                        "IoApic::getIrqToGsiMapping(): IoApic doesn't have CASCADE IRQ!");
    }

    if (irq == Pic::Interrupt::PIT) {
        return Interrupt::PIT;
    }

    return static_cast<Interrupt>(irq);
}

Kernel::InterruptDispatcher::Interrupt IoApic::getGsiToSlotMapping(Interrupt gsi) {
    // PIT is mapped to GSI 2 but slot 32
    if (gsi == Interrupt::PIT) {
        return Kernel::InterruptDispatcher::PIT;
    }

    // Other IRQs are mapped to slots 32 - 47
    if (gsi <= Interrupt::SECONDARY_ATA && gsi != Interrupt::FREE0) {
        return static_cast<Kernel::InterruptDispatcher::Interrupt>(gsi + Kernel::InterruptDispatcher::PIT);
    }

    // Rest is mapped arbitrarily
    switch (gsi) {
        case FREE0:
            return Kernel::InterruptDispatcher::FREE0;
        case FREE4:
        case FREE5:
        case FREE6:
        case FREE7:
        case FREE8:
        case FREE9:
        case FREE10:
        case FREE11:
            return static_cast<Kernel::InterruptDispatcher::Interrupt>(gsi + 156);
    }
}

void IoApic::initREDTBL() {
    if (baseVirtAddress == 0) {
        Util::Exception::throwException(Util::Exception::NULL_POINTER,
                                        "IoApic::initREDTBL(): IoApic MMIO not initialized!");
    }

    // NOTE: The id corresponds to the current CPU (the BSP if other cores get started after this)
    uint8_t id = LApic::getId();
    for (uint8_t gsi = 0; gsi <= maxREDTBLEntry; ++gsi) {
        // NOTE: Interrupts have to be disabled beforehand
        writeREDTBL(gsi, {
            .slot = getGsiToSlotMapping(static_cast<Interrupt>(gsi)),
            .deliveryMode = Delivery_Mode::FIXED,
            .destinationMode = Destination_Mode::PHYSICAL,
            .pinPolarity = Pin_Polarity::HIGH,
            .triggerMode = Trigger_Mode::EDGE,
            .isMasked = true,
            .destination = id });
    }

}

#if HHUOS_IOAPIC_ENABLE_DEBUG == 1
void IoApic::logDebugDump() {
    uint8_t id = getId();

    log.debug("IO APIC Base Phys Address: 0x%x", IOAPIC_BASE_DEFAULT_PHYS_ADDRESS);
    log.debug("IO APIC Base Virt Address: 0x%x", baseVirtAddress);
    log.debug("IO APIC ID: %u", id);
    log.debug("IO APIC VER: 0x%x (Has EOI Register: %u)", getVersion(), hasEOIRegister());
    log.debug("IO APIC REDTLB Entries: %u", maxREDTBLEntry + 1);
}
#endif

}