#include "ApicAcpiParser.h"
#include "device/power/acpi/Acpi.h"
#include "lib/util/cpu/CpuId.h"

namespace Device {

LPlatformInformation *ApicAcpiParser::parseLPlatformInformation() {
    auto *info = new LPlatformInformation;
    auto features = Util::Cpu::CpuId::getCpuFeatures();
    for (auto feature: features) {
        if (feature == Util::Cpu::CpuId::CpuFeature::APIC) {
            info->xApicSupported = true;
        }
        if (feature == Util::Cpu::CpuId::CpuFeature::X2APIC) {
            info->x2ApicSupported = true;
        }
    }

    // Abort if APIC/ACPI support not present
    if (!(info->xApicSupported || info->x2ApicSupported) || !hasACPI10()) {
        delete info;
        return nullptr;
    }

    auto *madt = Acpi::getTable<Acpi::Madt>("APIC");
    info->address = madt->localApicAddress;

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
        info->lapics.add(new LApicInformation{
                .acpiId = lapic->acpiProcessorId,
                .id = lapic->apicId,
                .enabled = static_cast<bool>(lapic->flags & 0x1),
        });
    }

    for (auto *lnmi: nmiConfigurations) {
        info->lnmis.add(new LNMIConfiguration{
                .acpiId = lnmi->acpiProcessorId,
                .id = static_cast<uint8_t>(lnmi->acpiProcessorId == 0xFF
                                           ? 0xFF
                                           : acpiIdToApicId(info, lnmi->acpiProcessorId)),
                .polarity = lnmi->flags & Acpi::IntiFlag::ACTIVE_HIGH
                            ? LVTEntry::PinPolarity::HIGH
                            : LVTEntry::PinPolarity::LOW,
                .triggerMode = lnmi->flags & Acpi::IntiFlag::EDGE_TRIGGERED
                               ? LVTEntry::TriggerMode::EDGE
                               : LVTEntry::TriggerMode::LEVEL,
                .lint = lnmi->localApicLint
        });
    }

    return info;
}

IoPlatformInformation *ApicAcpiParser::parseIoPlatformInformation() {
    bool apicSupported = false;
    auto features = Util::Cpu::CpuId::getCpuFeatures();
    for (auto feature: features) {
        if (feature == Util::Cpu::CpuId::CpuFeature::APIC || feature == Util::Cpu::CpuId::CpuFeature::X2APIC) {
            apicSupported = true;
        }
    }

    if (!apicSupported || !hasACPI10()) {
        return nullptr;
    }

    auto *info = new IoPlatformInformation;

    Util::Data::ArrayList<const Acpi::IoApic *> ioApics;
    Util::Data::ArrayList<const Acpi::InterruptSourceOverride *> interruptSourceOverrides;
    Util::Data::ArrayList<const Acpi::NMISource *> nmiConfigurations;
    Acpi::getApicStructures(&ioApics, Acpi::IO_APIC);
    Acpi::getApicStructures(&interruptSourceOverrides, Acpi::INTERRUPT_SOURCE_OVERRIDE);
    Acpi::getApicStructures(&nmiConfigurations, Acpi::NON_MASKABLE_INTERRUPT_SOURCE);

    if (ioApics.size() == 0) {
        Util::Exception::throwException(Util::Exception::ILLEGAL_STATE, "Didn't find IO APIC(s)!");
    }

    for (auto *ioapic: ioApics) {
        info->ioapics.add(new IoApicInformation{
                .id = ioapic->ioApicId,
                .address = ioapic->ioApicAddress,
                .gsiBase = static_cast<GlobalSystemInterrupt>(ioapic->globalSystemInterruptBase)
        });
    }

    /*
     * The .gsi/.inti assigned values seem switched, but I think ACPI is confusing regarding overrides:
     * Example when PIT (IRQ0) is mapped to IO APIC interrupt input 2:
     * The ACPI "Source" field would be 0, the ACPI "Global System Interrupt" field would be 2.
     * This makes working with GSIs slightly difficult, because to determine what GSI2 actually is, one would have
     * to look at the ACPI override structures.
     * Instead, I treat the GSIs as fixed (GSI0 will always be the PIT) and map them to an
     * IO APIC interrupt input instead.
     */
    for (auto *override: interruptSourceOverrides) {
        info->irqOverrides.add(new IoInterruptOverride{
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

    for (auto *ionmi: nmiConfigurations) {
        info->ionmis.add(new IoNMIConfiguration{
                .polarity = ionmi->flags & Acpi::IntiFlag::ACTIVE_HIGH
                            ? REDTBLEntry::PinPolarity::HIGH
                            : REDTBLEntry::PinPolarity::LOW,
                .triggerMode = ionmi->flags & Acpi::IntiFlag::EDGE_TRIGGERED
                               ? REDTBLEntry::TriggerMode::EDGE
                               : REDTBLEntry::TriggerMode::LEVEL,
                .gsi = static_cast<GlobalSystemInterrupt>(ionmi->globalSystemInterrupt)
        });
    }

    return info;
}

// ! Private member functions start here

bool ApicAcpiParser::hasACPI10() {
    return Acpi::isAvailable() && Acpi::getRsdp().revision == 0;
}

uint8_t ApicAcpiParser::acpiIdToApicId(LPlatformInformation *info, uint8_t uid) {
    if (info->lapics.size() == 0) {
        Util::Exception::throwException(Util::Exception::ILLEGAL_STATE, "LApicInformation not initialized!");
    }

    for (auto *lapic: info->lapics) {
        if (lapic->acpiId == uid) {
            return lapic->id;
        }
    }

    Util::Exception::throwException(Util::Exception::ILLEGAL_STATE, "Didn't find local APIC matching UID!");
}

}
