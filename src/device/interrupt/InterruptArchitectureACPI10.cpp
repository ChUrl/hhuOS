#include "InterruptArchitectureACPI10.h"
#include "device/power/acpi/Acpi.h"
#include "lib/util/cpu/CpuId.h"

namespace Device {

void InterruptArchitectureACPI10::initializeLPlatformInformation(InterruptArchitecture::LPlatformInformation *info) {
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
        return;
    }

    info->address = Acpi::getTable<Acpi::Madt>("APIC")->localApicAddress;

    if (info->address == 0) {
        Util::Exception::throwException(Util::Exception::ILLEGAL_STATE, "LApic::initializePlatformConfiguration(): Didn't find local APIC address!");
    }

    Util::Data::ArrayList<const Acpi::ProcessorLocalApic *> processorLocalApics;
    Util::Data::ArrayList<const Acpi::LocalApicNMI *> nmiConfigurations;
    Acpi::getApicStructures<Acpi::ProcessorLocalApic>(&processorLocalApics, Acpi::PROCESSOR_LOCAL_APIC);
    Acpi::getApicStructures<Acpi::LocalApicNMI>(&nmiConfigurations, Acpi::LOCAL_APIC_NMI);

    if (processorLocalApics.size() == 0) {
        Util::Exception::throwException(Util::Exception::ILLEGAL_STATE, "LApic::initializePlatformConfiguration(): Didn't find local APIC(s)!");
    }
    if (nmiConfigurations.size() == 0) {
        Util::Exception::throwException(Util::Exception::ILLEGAL_STATE, "LApic::initializePlatformConfiguration(): Didn't find NMI configuration(s)!");
    }

    for (auto *lapic : processorLocalApics) {
        info->lapics.add(new InterruptArchitecture::LApicInformation {
                .acpiId = lapic->acpiProcessorId,
                .id = lapic->apicId,
                .enabled = static_cast<bool>(lapic->flags & 0x1),
        });
    }

    for (auto *lnmi : nmiConfigurations) {
        info->lnmis.add(new InterruptArchitecture::LNMIConfiguration {
                .acpiId = lnmi->acpiProcessorId,
                .id = static_cast<uint8_t>(lnmi->acpiProcessorId == 0xFF ? 0xFF : uidToId(info, lnmi->acpiProcessorId)),
                .polarity = lnmi->flags & Acpi::IntiFlag::ACTIVE_HIGH ? LVTEntry::PinPolarity::HIGH : LVTEntry::PinPolarity::LOW,
                .triggerMode = lnmi->flags & Acpi::IntiFlag::EDGE_TRIGGERED ? LVTEntry::TriggerMode::EDGE : LVTEntry::TriggerMode::LEVEL,
                .lint = lnmi->localApicLint
        });
    }
}

void InterruptArchitectureACPI10::initializeIoPlatformInformation(InterruptArchitecture::IoPlatformInformation *info) {
    if (!hasACPI10()) {
        return;
    }

    // Default is identity mapping
    for (uint8_t irq = 0; irq < 16; ++irq) {
        info->irqToGsiMappings[irq] = irq;
        info->gsiToIrqMappings[irq] = irq;
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

    for (auto *ioapic : ioApics) {
        info->ioapics.add(new InterruptArchitecture::IoApicInformation {
                .id = ioapic->ioApicId,
                .address = ioapic->ioApicAddress,
                .gsiBase = static_cast<GlobalSystemInterrupt>(ioapic->globalSystemInterruptBase)
        });
    }

    for (auto *override : interruptSourceOverrides) {
        info->irqOverrides.add(new InterruptArchitecture::IoInterruptOverride {
                .bus = override->bus,
                .source = static_cast<GlobalSystemInterrupt>(override->source),
                .target = static_cast<GlobalSystemInterrupt>(override->globalSystemInterrupt),
                .polarity = override->flags & Acpi::IntiFlag::ACTIVE_HIGH ? REDTBLEntry::PinPolarity::HIGH : REDTBLEntry::PinPolarity::LOW,
                .triggerMode = override->flags & Acpi::IntiFlag::EDGE_TRIGGERED ? REDTBLEntry::TriggerMode::EDGE : REDTBLEntry::TriggerMode::LEVEL
        });

        // Update mappings
        info->irqToGsiMappings[override->source] = override->globalSystemInterrupt;
        info->gsiToIrqMappings[override->globalSystemInterrupt] = override->source;
    }

    for (auto *ionmi : nmiConfigurations) {
        info->ionmis.add(new InterruptArchitecture::IoNMIConfiguration {
                .polarity = ionmi->flags & Acpi::IntiFlag::ACTIVE_HIGH ? REDTBLEntry::PinPolarity::HIGH : REDTBLEntry::PinPolarity::LOW,
                .triggerMode = ionmi->flags & Acpi::IntiFlag::EDGE_TRIGGERED ? REDTBLEntry::TriggerMode::EDGE : REDTBLEntry::TriggerMode::LEVEL,
                .gsi = static_cast<GlobalSystemInterrupt>(ionmi->globalSystemInterrupt)
        });
    }
}

// ! Private member functions start here

bool InterruptArchitectureACPI10::hasACPI10() {
    return Acpi::isAvailable() && Acpi::getRsdp().revision == 0;
}

void InterruptArchitectureACPI10::verifyACPI10() {
    if (!hasACPI10()) {
        Util::Exception::throwException(Util::Exception::UNSUPPORTED_OPERATION, "InterruptArchitectureACPI10: ACPI 1.0b support not present!");
    }
}

uint8_t InterruptArchitectureACPI10::uidToId(InterruptArchitecture::LPlatformInformation *info, uint8_t uid) {
    if (info->lapics.size() == 0) {
        Util::Exception::throwException(Util::Exception::ILLEGAL_STATE, "LApic::idToUid(): LApicInformation not initialized!");
    }

    for (auto *lapic : info->lapics) {
        if (lapic->acpiId == uid) {
            return lapic->id;
        }
    }

    Util::Exception::throwException(Util::Exception::ILLEGAL_STATE, "LApic::idToUid(): Didn't find local APIC matching UID!");
}

}
