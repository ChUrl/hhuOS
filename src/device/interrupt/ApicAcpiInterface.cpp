#include "ApicAcpiInterface.h"

// TODO: Rename?
//       - This has nothing to do with ACPI, only the parsing
//       - The information could also be parsed from different sources

Device::LocalApicInformation *Device::LocalApicPlatform::getLocalApicInformation(uint8_t id) const {
    for (uint32_t i = 0; i < localApics.size(); ++i) {
        LocalApicInformation *localApic = localApics.get(i);
        if (localApic->id == id) {
            return localApic;
        }
    }

    // TODO: Use reference return type when not nullable
    return nullptr;
}

Device::LocalApicNMI *Device::LocalApicPlatform::getLocalNMIConfiguration(uint8_t id) const {
    return getLocalApicInformation(id)->nmi;
}

Device::IoApicIrqOverride *Device::IoApicPlatform::getIoApicIrqOverride(Device::GlobalSystemInterrupt target) const {
    for (uint32_t i = 0; i < overrides.size(); ++i) {
        IoApicIrqOverride *override = overrides.get(i);
        if (override->target == target) {
            return override;
        }
    }
    return nullptr;
}

Device::IoApicIrqOverride *Device::IoApicPlatform::getIoApicIrqOverride(Device::InterruptSource source) const {
    for (uint32_t i = 0; i < overrides.size(); ++i) {
        IoApicIrqOverride *override = overrides.get(i);
        if (override->source == source) {
            return override;
        }
    }
    return nullptr;
}

Device::InterruptSource Device::IoApicPlatform::getIoApicIrqOverrideSource(Device::GlobalSystemInterrupt target) const {
    auto *override = getIoApicIrqOverride(target);
    if (override != nullptr) {
        return override->source;
    }
    return static_cast<InterruptSource>(target);
}

Device::GlobalSystemInterrupt Device::IoApicPlatform::getIoApicIrqOverrideTarget(Device::InterruptSource source) const {
    auto *override = getIoApicIrqOverride(source);
    if (override != nullptr) {
        return override->target;
    }
    return static_cast<GlobalSystemInterrupt>(source);
}
