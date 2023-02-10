#include "ApicAcpiInterface.h"

namespace Device {

// TODO: These are shit

LocalApicInformation::LocalApicInformation(const Acpi::ProcessorLocalApic *processorLocalApic,
                                           const Acpi::LocalApicNmi *localApicNmi)
        : id(processorLocalApic->apicId),
          enabled(processorLocalApic->flags & 0x1),
          nmiLint(localApicNmi->localApicLint),
          nmiPolarity(localApicNmi->flags & Acpi::IntiFlag::ACTIVE_HIGH
                      ? LVTEntry::PinPolarity::HIGH
                      : LVTEntry::PinPolarity::LOW),
          nmiTriggerMode(localApicNmi->flags & Acpi::IntiFlag::EDGE_TRIGGERED
                         ? LVTEntry::TriggerMode::EDGE
                         : LVTEntry::TriggerMode::LEVEL) {}

LocalApicPlatform::LocalApicPlatform(uint32_t physAddres) : physAddress(physAddres) {}

IoApicInformation::IoApicInformation(const Acpi::IoApic *ioApic, const Acpi::NmiSource *nmiSource)
        : id(ioApic->ioApicId),
          physAddress(ioApic->ioApicAddress),
          gsiBase(static_cast<Kernel::GlobalSystemInterrupt>(ioApic->globalSystemInterruptBase)),
          hasNmi(nmiSource != nullptr),
          nmiGsi(hasNmi
                 ? static_cast<Kernel::GlobalSystemInterrupt>(nmiSource->globalSystemInterrupt)
                 : Kernel::GlobalSystemInterrupt(0)),
          nmiPolarity(hasNmi ? (nmiSource->flags & Acpi::IntiFlag::ACTIVE_HIGH
                                ? REDTBLEntry::PinPolarity::HIGH
                                : REDTBLEntry::PinPolarity::LOW)
                             : REDTBLEntry::PinPolarity::HIGH),
          nmiTriggerMode(hasNmi ? (nmiSource->flags & Acpi::IntiFlag::EDGE_TRIGGERED
                                   ? REDTBLEntry::TriggerMode::EDGE
                                   : REDTBLEntry::TriggerMode::LEVEL)
                                : REDTBLEntry::TriggerMode::EDGE) {}


IoApicPlatform::IoApicIrqOverride::IoApicIrqOverride(const Acpi::InterruptSourceOverride *interruptSourceOverride)
        : bus(interruptSourceOverride->bus),
          source(static_cast<InterruptRequest>(interruptSourceOverride->source)),
          target(static_cast<Kernel::GlobalSystemInterrupt>(interruptSourceOverride->globalSystemInterrupt)),
          polarity((interruptSourceOverride->flags & 0x3) == 0 ? REDTBLEntry::PinPolarity::BUS
                                                               : (interruptSourceOverride->flags &
                                                                  Acpi::IntiFlag::ACTIVE_HIGH
                                                                  ? REDTBLEntry::PinPolarity::HIGH
                                                                  : REDTBLEntry::PinPolarity::LOW)),
          triggerMode((interruptSourceOverride->flags & 0xc) == 0 ? REDTBLEntry::TriggerMode::BUS
                                                                  : (interruptSourceOverride->flags &
                                                                     Acpi::IntiFlag::EDGE_TRIGGERED
                                                                     ? REDTBLEntry::TriggerMode::EDGE
                                                                     : REDTBLEntry::TriggerMode::LEVEL)) {}

IoApicPlatform::IoApicPlatform(Util::Data::ArrayList<const Acpi::InterruptSourceOverride *> *interruptSourceOverrides) {
    for (const auto *interruptSourceOverride: *interruptSourceOverrides) {
        overrides.add(new IoApicIrqOverride(interruptSourceOverride));
    }
}

const IoApicPlatform::IoApicIrqOverride *
IoApicPlatform::getIoApicIrqOverride(Kernel::GlobalSystemInterrupt target) const {
    for (uint32_t i = 0; i < overrides.size(); ++i) {
        const IoApicIrqOverride *override = overrides.get(i);
        if (override->target == target) {
            return override;
        }
    }
    return nullptr;
}

const IoApicPlatform::IoApicIrqOverride *IoApicPlatform::getIoApicIrqOverride(InterruptRequest source) const {
    for (uint32_t i = 0; i < overrides.size(); ++i) {
        const IoApicIrqOverride *override = overrides.get(i);
        if (override->source == source) {
            return override;
        }
    }
    return nullptr;
}

InterruptRequest IoApicPlatform::getIoApicIrqOverrideSource(Kernel::GlobalSystemInterrupt target) const {
    const auto *override = getIoApicIrqOverride(target);
    if (override != nullptr) {
        return override->source;
    }
    return static_cast<InterruptRequest>(target);
}

Kernel::GlobalSystemInterrupt IoApicPlatform::getIoApicIrqOverrideTarget(InterruptRequest source) const {
    const auto *override = getIoApicIrqOverride(source);
    if (override != nullptr) {
        return override->target;
    }
    return static_cast<Kernel::GlobalSystemInterrupt>(source);
}

}
