#include "IoApic.h"
#include "kernel/system/System.h"
#include "kernel/paging/Paging.h"

namespace Device {

IoApicPlatform *IoApic::ioPlatform = nullptr;
Kernel::Logger IoApic::log = Kernel::Logger::get("IoApic");

// TODO: Should I arbitrarily assign IDs like this or does ACPI already contain valid IDs?
uint8_t ioApicId = 0; // IoApics don't start with an assigned id

IoApic::IoApic(IoApicPlatform *ioApicPlatform, IoApicInformation &&ioApicInformation)
: ioInfo(ioApicInformation) {
    ioPlatform = ioApicPlatform;
}

bool IoApic::isInitialized() const {
    return initialized;
}

void IoApic::initialize() {
    LocalApic::ensureBspInitialized();
    initializeMMIORegion();

    // https://github.com/torvalds/linux/blob/master/arch/x86/kernel/apic/io_apic.c#L470
    ioPlatform->version = readDoubleWord(VER);
    ioPlatform->directEoiSupported = ioPlatform->version >= 0x20;

    // With the IRQPA there is a way to address more than 255 GSIs although maxREDTBLEntries only has 8 bits
    // With ICH5 (and other ICHs?) it is always 24 (also ICH5 only has 1 IO APIC, as other  consumer hardware)
    ioInfo.gsiMax = static_cast<Kernel::GlobalSystemInterrupt>(ioInfo.gsiBase + (readDoubleWord(VER) >> 16));
    if (ioInfo.gsiMax > ioPlatform->globalMaxGsi) {
        ioPlatform->globalMaxGsi = ioInfo.gsiMax;
    }

    initializeREDTBL();

    // Configure NMI if it exists
    if (ioInfo.hasNmi) {
        REDTBLEntry redtblEntry{};
        redtblEntry.vector = static_cast<Kernel::InterruptVector>(0);
        redtblEntry.deliveryMode = REDTBLEntry::DeliveryMode::NMI;
        redtblEntry.destinationMode = REDTBLEntry::DestinationMode::PHYSICAL;
        redtblEntry.pinPolarity = ioInfo.nmiPolarity;
        redtblEntry.triggerMode = ioInfo.nmiTriggerMode;
        redtblEntry.isMasked = false;
        redtblEntry.destination = LocalApic::getId();
        writeREDTBL(ioInfo.nmiGsi, redtblEntry);
    }

    initialized = true;
}

void IoApic::allow(Kernel::GlobalSystemInterrupt gsi) {
    ensureValidGsi(gsi);
    REDTBLEntry redtblEntry = readREDTBL(gsi);
    redtblEntry.isMasked = false;
    writeREDTBL(gsi, redtblEntry);
}

void IoApic::forbid(Kernel::GlobalSystemInterrupt gsi) {
    ensureValidGsi(gsi);
    REDTBLEntry redtblEntry = readREDTBL(gsi);
    redtblEntry.isMasked = true;
    writeREDTBL(gsi, redtblEntry);
}

bool IoApic::status(Kernel::GlobalSystemInterrupt gsi) {
    ensureValidGsi(gsi);
    return readREDTBL(gsi).isMasked;
}

void IoApic::sendEndOfInterrupt(Kernel::InterruptVector vector, Kernel::GlobalSystemInterrupt gsi) {
    const REDTBLEntry redtblEntry = readREDTBL(gsi);
    if (redtblEntry.triggerMode == REDTBLEntry::TriggerMode::EDGE) {
        return; // Edge triggered interrupts are EOI'd only by the local APIC
    }

    if (ioPlatform->directEoiSupported) {
        writeDirectRegister<uint32_t>(EOI, vector);
    } else {
        // Marking a level triggered interrupt as edge triggered and masked clears the remote IRR bit
        // See https://github.com/torvalds/linux/blob/master/arch/x86/kernel/apic/io_apic.c#L470
        REDTBLEntry tempRedtblEntry = redtblEntry; // Copy to restore old values afterwards
        tempRedtblEntry.isMasked = true;
        tempRedtblEntry.triggerMode = REDTBLEntry::TriggerMode::EDGE;

        writeREDTBL(gsi, tempRedtblEntry); // Mark as edge triggered and masked
        writeREDTBL(gsi, redtblEntry); // Restore old values
    }
}

void IoApic::ensureRegisterAccess() const {
    if (ioInfo.virtAddress == 0) {
        Util::Exception::throwException(Util::Exception::NULL_POINTER, "IoApic MMIO not initialized!");
    }
}

void IoApic::ensureValidGsi(Kernel::GlobalSystemInterrupt gsi) const {
    if (gsi < ioInfo.gsiBase || gsi > ioInfo.gsiMax) {
        Util::Exception::throwException(Util::Exception::INVALID_ARGUMENT, "GSI not handled by this IO APIC!");
    }
}

void IoApic::initializeMMIORegion() {
    uint32_t physAddress = ioInfo.physAddress;
    uint32_t pageOffset = physAddress % Util::Memory::PAGESIZE;

    auto &memoryService = Kernel::System::getService<Kernel::MemoryService>();
    void *virtAddress = memoryService.mapIO(physAddress, Util::Memory::PAGESIZE, true);

    // Account for possible misalignment
    ioInfo.virtAddress = reinterpret_cast<uint32_t>(virtAddress) + pageOffset;
}

void IoApic::initializeREDTBL() {
    REDTBLEntry redtblEntry{};
    redtblEntry.deliveryMode = REDTBLEntry::DeliveryMode::FIXED;
    redtblEntry.destinationMode = REDTBLEntry::DestinationMode::PHYSICAL;
    redtblEntry.isMasked = true;
    redtblEntry.destination = LocalApic::getId(); // NOTE: All interrupts are sent to the BSP, which is inefficient

    for (uint32_t interruptInput = ioInfo.gsiBase; interruptInput <= ioInfo.gsiMax; ++interruptInput) {
        auto gsi = static_cast<Kernel::GlobalSystemInterrupt>(interruptInput); // GSIs match interrupt inputs on IO APIC

        redtblEntry.vector = static_cast<Kernel::InterruptVector>(gsi + 32); // If no override exists GSI matches vector
        redtblEntry.pinPolarity = REDTBLEntry::PinPolarity::HIGH;
        redtblEntry.triggerMode = REDTBLEntry::TriggerMode::EDGE;

        const IoApicPlatform::IoApicIrqOverride *override = ioPlatform->getIoApicIrqOverride(gsi);
        if (override != nullptr) {
            redtblEntry.vector = static_cast<Kernel::InterruptVector>(override->source + 32);
            // NOTE: This is disabled because some ACPI entries are bugged (for example the PIT redirect)
            // redtblEntry.pinPolarity = override->polarity;
            // redtblEntry.triggerMode = override->triggerMode;
        }

        writeREDTBL(gsi, redtblEntry);
    }
}

#if HHUOS_APIC_ENABLE_DEBUG == 1
void IoApic::dumpREDTBL() {
    log.info("Redirection Table (I/O APIC Id: [%d]):", ioInfo.id);
    for (uint32_t gsi = ioInfo.gsiBase; gsi < ioInfo.gsiMax; ++gsi) {
        REDTBLEntry redtblEntry = readREDTBL(static_cast<Kernel::GlobalSystemInterrupt>(gsi));
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
    ensureRegisterAccess();
    writeDirectRegister<uint8_t>(IND, reg);
    auto val = readDirectRegister<uint32_t>(DAT);
    return val;
}

void IoApic::writeDoubleWord(IndirectRegister reg, uint32_t val) {
    ensureRegisterAccess();
    writeDirectRegister<uint8_t>(IND, reg);
    writeDirectRegister<uint32_t>(DAT, val);
}

REDTBLEntry IoApic::readREDTBL(Kernel::GlobalSystemInterrupt gsi) {
    ensureValidGsi(gsi);
    auto interruptInput = static_cast<uint8_t>(gsi - ioInfo.gsiBase);

    // The first register is the low DW, the second register is the high DW
    uint32_t low = readDoubleWord(static_cast<IndirectRegister>(REDTBL + 2 * interruptInput));
    uint64_t high = readDoubleWord(static_cast<IndirectRegister>(REDTBL + 2 * interruptInput + 1));
    return static_cast<REDTBLEntry>(low | high << 32);
}

void IoApic::writeREDTBL(Kernel::GlobalSystemInterrupt gsi, const REDTBLEntry &redtbl) {
    ensureValidGsi(gsi);
    auto interruptInput = static_cast<uint8_t>(gsi - ioInfo.gsiBase);

    // The first register is the low DW, the second register is the high DW
    auto val = static_cast<uint64_t>(redtbl);
    writeDoubleWord(static_cast<IndirectRegister>(REDTBL + 2 * interruptInput), val & 0xFFFFFFFF);
    writeDoubleWord(static_cast<IndirectRegister>(REDTBL + 2 * interruptInput + 1), val >> 32);
}

}