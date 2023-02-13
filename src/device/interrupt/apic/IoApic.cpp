#include "IoApic.h"
#include "kernel/paging/Paging.h"
#include "kernel/service/MemoryService.h"
#include "kernel/system/System.h"
#include "lib/util/base/Constants.h"

namespace Device {

bool IoApic::supportsDirectedEOI = false;
Kernel::GlobalSystemInterrupt IoApic::systemGsiMax = static_cast<Kernel::GlobalSystemInterrupt>(0);
Util::ArrayList<IoApic::IrqOverride *> IoApic::irqOverrides;

Kernel::Logger IoApic::log = Kernel::Logger::get("IoApic");

IoApic::IoApic(uint8_t ioId, uint32_t baseAddress, Kernel::GlobalSystemInterrupt gsiBase)
  : ioId(ioId), baseAddress(baseAddress), gsiBase(gsiBase) {}

uint8_t IoApic::getVersion() {
    return readIndirectRegister(VER) & 0xFF;
}

void IoApic::initialize() {
    auto &memoryService = Kernel::System::getService<Kernel::MemoryService>();
    void *virtAddress = memoryService.mapIO(baseAddress, Util::PAGESIZE, true);

    // Account for possible misalignment
    const uint32_t pageOffset = baseAddress % Util::PAGESIZE;
    mmioAddress = reinterpret_cast<uint32_t>(virtAddress) + pageOffset;

    // https://github.com/torvalds/linux/blob/master/arch/x86/kernel/apic/io_apic.c#L470
    supportsDirectedEOI = (readIndirectRegister(VER) & 0xFF) >= 0x20;

    // With the IRQPA there is a way to address more than 255 GSIs although maxREDTBLEntries only has 8 bits
    // With ICH5 and other ICHs it is always 24 (ICH5 only has 1 IO APIC, as other consumer hardware)
    gsiMax = static_cast<Kernel::GlobalSystemInterrupt>(gsiBase + (readIndirectRegister(VER) >> 16));
    if (gsiMax > systemGsiMax) {
        systemGsiMax = gsiMax;
    }

    initializeREDTBL();

    // Configure NMI if it exists
    if (hasNmi) {
        REDTBLEntry redtblEntry{};
        redtblEntry.vector = static_cast<Kernel::InterruptVector>(0);
        redtblEntry.deliveryMode = REDTBLEntry::DeliveryMode::NMI;
        redtblEntry.destinationMode = REDTBLEntry::DestinationMode::PHYSICAL;
        redtblEntry.pinPolarity = nmiPolarity;
        redtblEntry.triggerMode = nmiTrigger;
        redtblEntry.isMasked = false;
        redtblEntry.destination = LocalApic::getId(); // Send to the BSP
        writeREDTBL(nmiGsi, redtblEntry);
    }
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

    if (supportsDirectedEOI) {
        writeMMIORegister<uint32_t>(EOI, vector);
    } else {
        // Marking a level triggered interrupt as edge triggered and masked clears the remote IRR bit
        // See https://github.com/torvalds/linux/blob/master/arch/x86/kernel/apic/io_apic.c#L470
        REDTBLEntry tempRedtblEntry = redtblEntry; // Copy to restore old values afterwards
        tempRedtblEntry.isMasked = true;
        tempRedtblEntry.triggerMode = REDTBLEntry::TriggerMode::EDGE;

        writeREDTBL(gsi, tempRedtblEntry); // Mark as edge triggered and masked
        writeREDTBL(gsi, redtblEntry);     // Restore old values
    }
}

void IoApic::ensureValidGsi(Kernel::GlobalSystemInterrupt gsi) const {
    if (gsi < gsiBase || gsi > gsiMax) {
        Util::Exception::throwException(Util::Exception::INVALID_ARGUMENT, "GSI not handled by this I/O APIC!");
    }
}

void IoApic::initializeREDTBL() {
    REDTBLEntry redtblEntry{};
    redtblEntry.deliveryMode = REDTBLEntry::DeliveryMode::FIXED;
    redtblEntry.destinationMode = REDTBLEntry::DestinationMode::PHYSICAL;
    redtblEntry.isMasked = true;
    redtblEntry.destination = LocalApic::getId(); // ! All interrupts are sent to the BSP, which can be inefficient

    for (uint32_t interruptInput = gsiBase; interruptInput <= gsiMax; ++interruptInput) {
        auto gsi = static_cast<Kernel::GlobalSystemInterrupt>(interruptInput); // GSIs match interrupt inputs on IO APIC

        redtblEntry.vector = static_cast<Kernel::InterruptVector>(gsi + 32); // If no override exists GSI matches vector
        redtblEntry.pinPolarity = REDTBLEntry::PinPolarity::HIGH;            // ISA bus default
        redtblEntry.triggerMode = REDTBLEntry::TriggerMode::EDGE;            // ISA bus default

        const IrqOverride *override = getOverride(gsi);
        if (override != nullptr) {
            // Apply a mapping differing from the identity mapping
            redtblEntry.vector = static_cast<Kernel::InterruptVector>(override->source + 32);
            redtblEntry.pinPolarity = override->polarity;
            redtblEntry.triggerMode = override->trigger;
        }

        writeREDTBL(gsi, redtblEntry);
    }
}

void IoApic::dumpREDTBL() {
    log.info("Redirection Table (I/O APIC Id: [%d]):", ioId);
    for (uint32_t gsi = gsiBase; gsi < gsiMax; ++gsi) {
        const REDTBLEntry redtblEntry = readREDTBL(static_cast<Kernel::GlobalSystemInterrupt>(gsi));
        log.info("- External Interrupt [%d]: (Vector: [0x%x], Masked: [%d], Destination: [%d], DeliveryMode: [0b%b], DestinationMode: [%s], PinPolarity: [%s], TriggerMode: [%s])",
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

void IoApic::addNonMaskableInterrupt(Kernel::GlobalSystemInterrupt nmiGsi, REDTBLEntry::PinPolarity nmiPolarity,
                                     REDTBLEntry::TriggerMode nmiTrigger) {
    hasNmi = true;
    this->nmiGsi = nmiGsi;
    this->nmiPolarity = nmiPolarity;
    this->nmiTrigger = nmiTrigger;
}

void IoApic::addIrqOverride(InterruptRequest source, Kernel::GlobalSystemInterrupt target,
                            REDTBLEntry::PinPolarity polarity, REDTBLEntry::TriggerMode trigger) {
    // This memory is never freed, as the APIC can't be disabled
    auto *override = new IrqOverride{};
    override->source = source;
    override->target = target;
    override->polarity = polarity;
    override->trigger = trigger;
    irqOverrides.add(override);
}

IoApic::IrqOverride *IoApic::getOverride(Kernel::GlobalSystemInterrupt target) {
    for (uint32_t i = 0; i < irqOverrides.size(); ++i) {
        auto *override = irqOverrides.get(i);
        if (override->target == target) {
            return override;
        }
    }

    return nullptr;
}

IoApic::IrqOverride *IoApic::getOverride(InterruptRequest source) {
    for (uint32_t i = 0; i < irqOverrides.size(); ++i) {
        auto *override = irqOverrides.get(i);
        if (override->source == source) {
            return override;
        }
    }

    return nullptr;
}

uint32_t IoApic::readIndirectRegister(IndirectRegister reg) {
    writeMMIORegister<uint8_t>(IND, reg);
    auto val = readMMIORegister<uint32_t>(DAT);
    return val;
}

void IoApic::writeIndirectRegister(IndirectRegister reg, uint32_t val) {
    writeMMIORegister<uint8_t>(IND, reg);
    writeMMIORegister<uint32_t>(DAT, val);
}

REDTBLEntry IoApic::readREDTBL(Kernel::GlobalSystemInterrupt gsi) {
    ensureValidGsi(gsi);
    auto interruptInput = static_cast<uint8_t>(gsi - gsiBase);

    // The first register is the low DW, the second register is the high DW
    const uint32_t low = readIndirectRegister(static_cast<IndirectRegister>(REDTBL + 2 * interruptInput));
    const uint64_t high = readIndirectRegister(static_cast<IndirectRegister>(REDTBL + 2 * interruptInput + 1));
    return static_cast<REDTBLEntry>(low | high << 32);
}

void IoApic::writeREDTBL(Kernel::GlobalSystemInterrupt gsi, const REDTBLEntry &redtbl) {
    ensureValidGsi(gsi);
    auto interruptInput = static_cast<uint8_t>(gsi - gsiBase);

    // The first register is the low DW, the second register is the high DW
    auto val = static_cast<uint64_t>(redtbl);
    writeIndirectRegister(static_cast<IndirectRegister>(REDTBL + 2 * interruptInput), val & 0xFFFFFFFF);
    writeIndirectRegister(static_cast<IndirectRegister>(REDTBL + 2 * interruptInput + 1), val >> 32);
}

} // namespace Device