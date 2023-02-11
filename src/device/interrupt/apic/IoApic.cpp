#include "IoApic.h"
#include "kernel/system/System.h"
#include "kernel/paging/Paging.h"
#include "lib/util/base/Constants.h"
#include "kernel/service/MemoryService.h"

namespace Device {

IoApicPlatform *IoApic::ioPlatform = nullptr;
Kernel::Logger IoApic::log = Kernel::Logger::get("IoApic");

IoApic::IoApic(IoApicPlatform *ioApicPlatform, IoApicInformation &&ioApicInformation)
        : ioInfo(ioApicInformation) {
    ioPlatform = ioApicPlatform;
}

void IoApic::initialize() {
    initializeMMIORegion();

    // https://github.com/torvalds/linux/blob/master/arch/x86/kernel/apic/io_apic.c#L470
    ioPlatform->version = readIndirectRegister(VER);
    ioPlatform->directEoiSupported = ioPlatform->version >= 0x20;

    // With the IRQPA there is a way to address more than 255 GSIs although maxREDTBLEntries only has 8 bits
    // With ICH5 (and other ICHs?) it is always 24 (also ICH5 only has 1 IO APIC, as other  consumer hardware)
    ioInfo.gsiMax = static_cast<Kernel::GlobalSystemInterrupt>(ioInfo.gsiBase + (readIndirectRegister(VER) >> 16));
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
        redtblEntry.destination = 0; // Send to the BSP
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
        writeMMIORegister<uint32_t>(EOI, vector);
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

void IoApic::ensureValidGsi(Kernel::GlobalSystemInterrupt gsi) const {
    if (gsi < ioInfo.gsiBase || gsi > ioInfo.gsiMax) {
        Util::Exception::throwException(Util::Exception::INVALID_ARGUMENT, "GSI not handled by this IO APIC!");
    }
}

void IoApic::ensureRegisterAccess() const {
    if (ioInfo.virtAddress == 0) {
        Util::Exception::throwException(Util::Exception::NULL_POINTER, "IoApic MMIO not initialized!");
    }
}

void IoApic::initializeMMIORegion() {
    uint32_t physAddress = ioInfo.physAddress;
    uint32_t pageOffset = physAddress % Util::PAGESIZE;

    auto &memoryService = Kernel::System::getService<Kernel::MemoryService>();
    void *virtAddress = memoryService.mapIO(physAddress, Util::PAGESIZE, true);

    // Account for possible misalignment
    ioInfo.virtAddress = reinterpret_cast<uint32_t>(virtAddress) + pageOffset;
}

void IoApic::initializeREDTBL() {
    REDTBLEntry redtblEntry{};
    redtblEntry.deliveryMode = REDTBLEntry::DeliveryMode::FIXED;
    redtblEntry.destinationMode = REDTBLEntry::DestinationMode::PHYSICAL;
    redtblEntry.isMasked = true;
    redtblEntry.destination = 0; // ! All interrupts are sent to the BSP, which is inefficient

    for (uint32_t interruptInput = ioInfo.gsiBase; interruptInput <= ioInfo.gsiMax; ++interruptInput) {
        auto gsi = static_cast<Kernel::GlobalSystemInterrupt>(interruptInput); // GSIs match interrupt inputs on IO APIC

        redtblEntry.vector = static_cast<Kernel::InterruptVector>(gsi + 32); // If no override exists GSI matches vector
        redtblEntry.pinPolarity = REDTBLEntry::PinPolarity::HIGH; // ISA bus default
        redtblEntry.triggerMode = REDTBLEntry::TriggerMode::EDGE; // ISA bus default

        const IoApicPlatform::IoApicIrqOverride *override = ioPlatform->getIoApicIrqOverride(gsi);
        if (override != nullptr) {
            // Apply a mapping differing from the identity mapping
            redtblEntry.vector = static_cast<Kernel::InterruptVector>(override->source + 32);

            // Apply a specified trigger mode and polarity. If the trigger mode/polarity is configured to "BUS",
            // it means that the bus defaults are used. In this case, the ISA bus defaults (edge-triggered, active
            // high) are assumed.
            if (override->polarity != REDTBLEntry::PinPolarity::BUS) {
                redtblEntry.pinPolarity = override->polarity;
            }
            if (override->triggerMode != REDTBLEntry::TriggerMode::BUS) {
                redtblEntry.triggerMode = override->triggerMode;
            }
        }

        writeREDTBL(gsi, redtblEntry);
    }
}

#if HHUOS_APIC_ENABLE_DEBUG == 1

void IoApic::dumpREDTBL() {
    log.info("Redirection Table (I/O APIC Id: [%d]):", ioInfo.id);
    for (uint32_t gsi = ioInfo.gsiBase; gsi < ioInfo.gsiMax; ++gsi) {
        REDTBLEntry redtblEntry = readREDTBL(static_cast<Kernel::GlobalSystemInterrupt>(gsi));
        log.debug(
                "- Interrupt [%d]: (Vector: [0x%x], Masked: [%d], Destination: [%d], DeliveryMode: [0b%b], DestinationMode: [%s], PinPolarity: [%s], TriggerMode: [%s])",
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

uint32_t IoApic::readIndirectRegister(IndirectRegister reg) {
    ensureRegisterAccess();
    writeMMIORegister<uint8_t>(IND, reg);
    auto val = readMMIORegister<uint32_t>(DAT);
    return val;
}

void IoApic::writeIndirectRegister(IndirectRegister reg, uint32_t val) {
    ensureRegisterAccess();
    writeMMIORegister<uint8_t>(IND, reg);
    writeMMIORegister<uint32_t>(DAT, val);
}

REDTBLEntry IoApic::readREDTBL(Kernel::GlobalSystemInterrupt gsi) {
    ensureValidGsi(gsi);
    auto interruptInput = static_cast<uint8_t>(gsi - ioInfo.gsiBase);

    // The first register is the low DW, the second register is the high DW
    uint32_t low = readIndirectRegister(static_cast<IndirectRegister>(REDTBL + 2 * interruptInput));
    uint64_t high = readIndirectRegister(static_cast<IndirectRegister>(REDTBL + 2 * interruptInput + 1));
    return static_cast<REDTBLEntry>(low | high << 32);
}

void IoApic::writeREDTBL(Kernel::GlobalSystemInterrupt gsi, const REDTBLEntry &redtbl) {
    ensureValidGsi(gsi);
    auto interruptInput = static_cast<uint8_t>(gsi - ioInfo.gsiBase);

    // The first register is the low DW, the second register is the high DW
    auto val = static_cast<uint64_t>(redtbl);
    writeIndirectRegister(static_cast<IndirectRegister>(REDTBL + 2 * interruptInput), val & 0xFFFFFFFF);
    writeIndirectRegister(static_cast<IndirectRegister>(REDTBL + 2 * interruptInput + 1), val >> 32);
}

}