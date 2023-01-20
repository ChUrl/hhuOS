#include "ApicAcpiParser.h"

namespace Device {

// TODO: Move this into ApicAcpiInterface?

void ApicAcpiParser::parseLocalPlatformInformation(LocalApicPlatform *localPlatform) {
    ensureAcpi10();

    auto *madt = Acpi::getTable<Acpi::Madt>("APIC");
    localPlatform->physAddress = madt->localApicAddress;

    Util::Data::ArrayList<const Acpi::ProcessorLocalApic *> processorLocalApics;
    Util::Data::ArrayList<const Acpi::LocalApicNMI *> nmiConfigurations;
    Acpi::getApicStructures<Acpi::ProcessorLocalApic>(&processorLocalApics, Acpi::PROCESSOR_LOCAL_APIC);
    Acpi::getApicStructures<Acpi::LocalApicNMI>(&nmiConfigurations, Acpi::LOCAL_APIC_NMI);

    if (processorLocalApics.size() == 0) {
        Util::Exception::throwException(Util::Exception::ILLEGAL_STATE, "Didn't find local APIC(s)!");
    }
    if (nmiConfigurations.size() == 0) {
        Util::Exception::throwException(Util::Exception::ILLEGAL_STATE, "Didn't find NMI configuration(s)!");
    }

    for (auto *lapic: processorLocalApics) {
        localPlatform->localApics.add(new LocalApicInformation{
                .id = lapic->apicId,
                .enabled = static_cast<bool>(lapic->flags & 0x1),
        });
    }

    for (auto *nmiConfiguration: nmiConfigurations) {
        auto *lnmi = new LocalApicNMI{
                .polarity = nmiConfiguration->flags & Acpi::IntiFlag::ACTIVE_HIGH
                            ? LVTEntry::PinPolarity::HIGH
                            : LVTEntry::PinPolarity::LOW,
                .triggerMode = nmiConfiguration->flags & Acpi::IntiFlag::EDGE_TRIGGERED
                               ? LVTEntry::TriggerMode::EDGE
                               : LVTEntry::TriggerMode::LEVEL,
                .lint = nmiConfiguration->localApicLint
        };

        // NOTE: This assumes that every local Apic only has one non maskable interrupt source

        // Assign the nmi to either all CPUs or one specific CPU
        if (nmiConfiguration->acpiProcessorId == 0xFF) {
            // 0xFF means all CPUs
            for (auto *localApic: localPlatform->localApics) {
                localApic->nmi = lnmi;
            }
        } else {
            auto apicId = acpiIdToApicId(processorLocalApics, nmiConfiguration->acpiProcessorId);
            auto *localInfo = getLocalApicInfo(localPlatform->localApics, apicId);
            localInfo->nmi = lnmi;
        }
    }
}

void ApicAcpiParser::parseIoPlatformInformation(IoApicPlatform *ioPlatform) {
    ensureAcpi10();

    Util::Data::ArrayList<const Acpi::InterruptSourceOverride *> interruptSourceOverrides;
    Acpi::getApicStructures(&interruptSourceOverrides, Acpi::INTERRUPT_SOURCE_OVERRIDE);

    for (auto *override: interruptSourceOverrides) {
        ioPlatform->overrides.add(new IoApicIrqOverride{
                .bus = override->bus,
                .source = static_cast<InterruptSource>(override->source),
                .target = static_cast<GlobalSystemInterrupt>(override->globalSystemInterrupt),
                .polarity = override->flags & Acpi::IntiFlag::ACTIVE_HIGH
                            ? REDTBLEntry::PinPolarity::HIGH
                            : REDTBLEntry::PinPolarity::LOW,
                .triggerMode = override->flags & Acpi::IntiFlag::EDGE_TRIGGERED
                               ? REDTBLEntry::TriggerMode::EDGE
                               : REDTBLEntry::TriggerMode::LEVEL
        });
    }
}

void ApicAcpiParser::parseIoApicInformation(Util::Data::ArrayList<IoApicInformation *> *ioInfos) {
    ensureAcpi10();

    Util::Data::ArrayList<const Acpi::IoApic *> ioApics;
    Acpi::getApicStructures(&ioApics, Acpi::IO_APIC);

    if (ioApics.size() == 0) {
        Util::Exception::throwException(Util::Exception::ILLEGAL_STATE, "Didn't find any I/O APIC!");
    }

    for (auto *ioapic: ioApics) {
        ioInfos->add(new IoApicInformation{
                .id = ioapic->ioApicId,
                .physAddress = ioapic->ioApicAddress,
                .gsiBase = static_cast<GlobalSystemInterrupt>(ioapic->globalSystemInterruptBase)
        });
    }

    Util::Data::ArrayList<const Acpi::NMISource *> nmiConfigurations;
    Acpi::getApicStructures(&nmiConfigurations, Acpi::NON_MASKABLE_INTERRUPT_SOURCE);

    for (auto *ionmi: nmiConfigurations) {
        auto *ioInfo = getIoApicInfo(*ioInfos, static_cast<GlobalSystemInterrupt>(ionmi->globalSystemInterrupt));
        ioInfo->nmi = new IoApicNMI{
                .polarity = ionmi->flags & Acpi::IntiFlag::ACTIVE_HIGH
                            ? REDTBLEntry::PinPolarity::HIGH
                            : REDTBLEntry::PinPolarity::LOW,
                .triggerMode = ionmi->flags & Acpi::IntiFlag::EDGE_TRIGGERED
                               ? REDTBLEntry::TriggerMode::EDGE
                               : REDTBLEntry::TriggerMode::LEVEL,
                .gsi = static_cast<GlobalSystemInterrupt>(ionmi->globalSystemInterrupt)
        };
    }
}

void ApicAcpiParser::ensureAcpi10() {
    if (!(Acpi::isAvailable() && Acpi::getRsdp().revision == 0)) {
        Util::Exception::throwException(Util::Exception::UNSUPPORTED_OPERATION, "Acpi 1.0 is not available!");
    }
}

uint8_t
ApicAcpiParser::acpiIdToApicId(const Util::Data::ArrayList<const Acpi::ProcessorLocalApic *> &lapics, uint8_t acpiUid) {
    for (auto *lapic: lapics) {
        if (lapic->acpiProcessorId == acpiUid) {
            // 0xFF means all CPUs
            return lapic->apicId;
        }
    }

    Util::Exception::throwException(Util::Exception::ILLEGAL_STATE, "Didn't find local APIC matching UID!");
}

LocalApicInformation *ApicAcpiParser::getLocalApicInfo(const Util::Data::ArrayList<LocalApicInformation *> &infos,
                                                       uint8_t id) {
    for (auto *info: infos) {
        if (info->id == id) {
            return info;
        }
    }

    return nullptr;
}

// TODO: Test this
IoApicInformation *
ApicAcpiParser::getIoApicInfo(const Util::Data::ArrayList<IoApicInformation *> &infos, GlobalSystemInterrupt gsi) {
    auto *ioInfo = infos.get(0);
    for (auto *info: infos) {
        // Find the largest gsiBase that is still smaller than the gsi
        if (gsi >= info->gsiBase && info->gsiBase > ioInfo->gsiBase) {
            ioInfo = info;
        }
    }
    return ioInfo;
}

}
