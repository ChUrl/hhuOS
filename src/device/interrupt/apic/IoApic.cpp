#include "IoApic.h"
#include "kernel/paging/Paging.h"
#include "kernel/service/MemoryService.h"
#include "kernel/system/System.h"
#include "lib/util/base/Constants.h"

namespace Device {

Kernel::GlobalSystemInterrupt IoApic::systemGsiMax = static_cast<Kernel::GlobalSystemInterrupt>(0);
Util::ArrayList<IoApic::IrqOverride *> IoApic::irqOverrides;
Util::Async::Spinlock IoApic::registerLock;
Util::Async::Spinlock IoApic::redtblLock;

Kernel::Logger IoApic::log = Kernel::Logger::get("IoApic");

IoApic::IoApic(uint8_t ioId, uint32_t baseAddress, Kernel::GlobalSystemInterrupt gsiBase)
  : ioId(ioId), baseAddress(baseAddress), gsiBase(gsiBase) {}

uint8_t IoApic::getVersion() {
    return readIndirectRegister(VER) & 0xFF;
}

void IoApic::initialize() {
    auto &memoryService = Kernel::System::getService<Kernel::MemoryService>();
    void *virtAddress = memoryService.mapIO(baseAddress, Util::PAGESIZE, true); // Throws

    // Account for possible misalignment
    const uint32_t pageOffset = baseAddress % Util::PAGESIZE;
    mmioAddress = reinterpret_cast<uint32_t>(virtAddress) + pageOffset;

    // Set the I/O APIC ID (the id register is initialized to 0) read from ACPI
    writeIndirectRegister(ID, static_cast<uint32_t>(ioId) << 24); // ICH5, sec. 9.5.6

    // With the IRQPA there is a way to address more than 255 GSIs although maxREDTBLEntries only has 8 bits
    // With ICH5 and other ICHs it is always 24 (ICH5 only has 1 IO APIC, as other consumer hardware)
    gsiMax = static_cast<Kernel::GlobalSystemInterrupt>(gsiBase + (readIndirectRegister(VER) >> 16));
    if (gsiMax > systemGsiMax) {
        systemGsiMax = gsiMax;
    }

    initializeREDTBL();

    // Configure NMIs
    for (const auto *nmi : nmiSources) {
        REDTBLEntry redtblEntry{};
        redtblEntry.vector = static_cast<Kernel::InterruptVector>(0);
        redtblEntry.deliveryMode = REDTBLEntry::DeliveryMode::NMI;
        redtblEntry.destinationMode = REDTBLEntry::DestinationMode::PHYSICAL;
        redtblEntry.pinPolarity = nmi->polarity;
        redtblEntry.triggerMode = nmi->trigger;
        redtblEntry.isMasked = false;
        redtblEntry.destination = LocalApic::getId(); // Send to the BSP
        writeREDTBL(nmi->source, redtblEntry);
    }
}

void IoApic::allow(Kernel::GlobalSystemInterrupt gsi) {
    REDTBLEntry redtblEntry = readREDTBL(gsi);
    redtblEntry.isMasked = false;
    writeREDTBL(gsi, redtblEntry);
}

void IoApic::forbid(Kernel::GlobalSystemInterrupt gsi) {
    REDTBLEntry redtblEntry = readREDTBL(gsi);
    redtblEntry.isMasked = true;
    writeREDTBL(gsi, redtblEntry);
}

bool IoApic::status(Kernel::GlobalSystemInterrupt gsi) {
    return readREDTBL(gsi).isMasked;
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

void IoApic::printRedtbl(Util::String &string) {
    string += Util::String::format("Redirection Table [%d]:\n", ioId);
    for (uint32_t gsi = gsiBase; gsi < gsiMax; ++gsi) {
        const REDTBLEntry redtblEntry = readREDTBL(static_cast<Kernel::GlobalSystemInterrupt>(gsi));
        string += Util::String::format(
          "Vector: [0x%x], Masked: [%d], Destination: [%d], Polarity: [%s], Trigger: [%s] (IRQ %d)\n",
          static_cast<uint8_t>(redtblEntry.vector),
          static_cast<uint8_t>(redtblEntry.isMasked),
          redtblEntry.destination,
          redtblEntry.pinPolarity == REDTBLEntry::PinPolarity::HIGH ? "HIGH" : "LOW",
          redtblEntry.triggerMode == REDTBLEntry::TriggerMode::EDGE ? "EDGE" : "LEVEL",
          gsi);
    }
}

bool IoApic::isNonMaskableInterrupt(Kernel::GlobalSystemInterrupt interrupt) {
    for (uint32_t i = 0; i < nmiSources.size(); ++i) {
        const NmiSource &nmi = *nmiSources.get(i);
        if (nmi.source == interrupt) {
            return true;
        }
    }

    return false;
}

void IoApic::addNonMaskableInterrupt(Kernel::GlobalSystemInterrupt nmiGsi, REDTBLEntry::PinPolarity nmiPolarity,
                                     REDTBLEntry::TriggerMode nmiTrigger) {
    auto *nmi = new NmiSource{};
    nmi->source = nmiGsi;
    nmi->polarity = nmiPolarity;
    nmi->trigger = nmiTrigger;
    nmiSources.add(nmi);
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
    registerLock.acquire(); // This needs to be synchronized in case multiple APs access a MMIO register
    writeMMIORegister<uint8_t>(IND, reg);
    auto val = readMMIORegister<uint32_t>(DAT);
    registerLock.release();
    return val;
}

void IoApic::writeIndirectRegister(IndirectRegister reg, uint32_t val) {
    registerLock.acquire(); // This needs to be synchronized in case multiple APs access a MMIO register
    writeMMIORegister<uint8_t>(IND, reg);
    writeMMIORegister<uint32_t>(DAT, val);
    registerLock.release();
}

REDTBLEntry IoApic::readREDTBL(Kernel::GlobalSystemInterrupt gsi) {
    auto interruptInput = static_cast<uint8_t>(gsi - gsiBase);

    // The first register is the low DW, the second register is the high DW
    redtblLock.acquire(); // This needs to be synchronized in case multiple APs access the REDTBL
    const uint32_t low = readIndirectRegister(static_cast<IndirectRegister>(REDTBL + 2 * interruptInput));
    const uint64_t high = readIndirectRegister(static_cast<IndirectRegister>(REDTBL + 2 * interruptInput + 1));
    redtblLock.release();
    return static_cast<REDTBLEntry>(low | high << 32);
}

void IoApic::writeREDTBL(Kernel::GlobalSystemInterrupt gsi, const REDTBLEntry &redtbl) {
    auto interruptInput = static_cast<uint8_t>(gsi - gsiBase);

    // The first register is the low DW, the second register is the high DW
    auto val = static_cast<uint64_t>(redtbl);
    redtblLock.acquire(); // This needs to be synchronized in case multiple APs access the REDTBL
    writeIndirectRegister(static_cast<IndirectRegister>(REDTBL + 2 * interruptInput), val & 0xFFFFFFFF);
    writeIndirectRegister(static_cast<IndirectRegister>(REDTBL + 2 * interruptInput + 1), val >> 32);
    redtblLock.release();
}

} // namespace Device