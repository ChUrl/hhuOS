#include "Apic.h"

namespace Device {

bool Apic::initialized = false;
LPlatformInformation *Apic::localPlatform = nullptr;
IoPlatformInformation *Apic::ioPlatform = nullptr;
Kernel::Logger Apic::log = Kernel::Logger::get("InterruptModel");

// TODO: Check all ensures

bool Apic::isInitialized() {
    return initialized;
}

void Apic::ensureInitialized() {
    if (!isInitialized()) {
        Util::Exception::throwException(Util::Exception::ILLEGAL_STATE, "InterruptModel not initialized!");
    }
}

bool Apic::apicSupported() {
    ensureInitialized();
    if (localPlatform == nullptr || ioPlatform == nullptr) {
        return false;
    }
    return localPlatform->xApicSupported || localPlatform->x2ApicSupported;
}

void Apic::ensureApicSupported() {
    if (!apicSupported()) {
        Util::Exception::throwException(Util::Exception::ILLEGAL_STATE, "System does not support APIC model!");
    }
}

bool Apic::isExternalInterrupt(InterruptVector vector) {
    // TODO: Ensure
    return static_cast<GlobalSystemInterrupt>(vector - 32) <= getGlobalMaxGsi();
}

bool Apic::isLocalInterrupt(InterruptVector vector) {
    // TODO: Ensure
    return vector >= InterruptVector ::CMCI && vector <= InterruptVector ::ERROR;
}

GlobalSystemInterrupt Apic::getGlobalMaxGsi() {
    // TODO: Ensure
    return ioPlatform->globalMaxGsi;
}

void Apic::dumpPlatformInformation() {
    ensureInitialized();

    // Local APICs
    log.info("Local APIC: Supported modes: [%s%s], Cores: [%d], Version: [0x%x], MMIO: [0x%x] (phys) -> [0x%x] (virt)",
             localPlatform->xApicSupported ? "xApic" : "None",
             localPlatform->x2ApicSupported ? ",x2Apic" : "",
             localPlatform->lapics.size(),
             localPlatform->version,
             localPlatform->address,
             localPlatform->virtAddress);

    for (auto *lapic: localPlatform->lapics) {
        log.info("- LApic: Id: [0x%x], Enabled: [%d], Selected mode: [%s]",
                 lapic->id,
                 lapic->enabled,
                 lapic->isX2Apic ? "x2Apic" : "xApic");
    }

    for (auto *lnmi: localPlatform->lnmis) {
        log.info("- NMI: Id: [0x%x], Lint: [LINT%d]", lnmi->id, lnmi->lint);
    }

    // IO APICs
    log.info("Io APIC: Version: [0x%x], EOI supported: [%d], Global GSI max: [%d]",
             ioPlatform->version,
             ioPlatform->eoiSupported,
             static_cast<uint8_t>(ioPlatform->globalMaxGsi));

    for (auto *ioapic: ioPlatform->ioapics) {
        log.info("- IoApic: Id: [0x%x], MMIO: [0x%x] (phys) -> [0x%x] (virt), GSI base: [%d], GSI max: [%d]",
                 ioapic->id,
                 ioapic->address,
                 ioapic->virtAddress,
                 static_cast<uint8_t>(ioapic->gsiBase),
                 static_cast<uint8_t>(ioapic->gsiMax));
    }

    for (auto *override: ioPlatform->irqOverrides) {
        log.info("- Override: IRQ Source: [%d], GSI Target: [%d], Polarity: [%s], TriggerMode: [%s]",
                 static_cast<uint8_t>(override->source),
                 static_cast<uint8_t>(override->target),
                 override->polarity == REDTBLEntry::PinPolarity::HIGH ? "HIGH" : "LOW",
                 override->triggerMode == REDTBLEntry::TriggerMode::EDGE ? "EDGE" : "LEVEL");
    }
    if (ioPlatform->irqOverrides.size() == 0) {
        log.info("- There are no IRQ overrides");
    }

    for (auto *nmi: ioPlatform->ionmis) {
        log.info("- NMI: GSI: [%d]", static_cast<uint8_t>(nmi->gsi));
    }
    if (ioPlatform->ionmis.size() == 0) {
        log.info("- There are no IO NMIs");
    }
}

Util::Data::ArrayList<LApicInformation *> &Apic::lapics() {
    ensureApicSupported();
    return localPlatform->lapics;
}

Util::Data::ArrayList<IoApicInformation *> &Apic::ioapics() {
    ensureApicSupported();
    return ioPlatform->ioapics;
}

LApicInformation *Apic::getLApicInformation(uint8_t id) {
    ensureApicSupported();
    for (uint32_t i = 0; i < localPlatform->lapics.size(); ++i) {
        LApicInformation *lapic = localPlatform->lapics.get(i);
        if (lapic->id == id) {
            return lapic;
        }
    }

    Util::Exception::throwException(Util::Exception::ILLEGAL_STATE, "No LApicInformation matching ID!");
}

LNMIConfiguration *Apic::getLNMIConfiguration(LApicInformation *lapic) {
    ensureApicSupported();
    for (uint32_t i = 0; i < localPlatform->lnmis.size(); ++i) {
        LNMIConfiguration *lnmi = localPlatform->lnmis.get(i);
        if (lnmi->id == 0xFF || lnmi->id == lapic->id) { // 0xFF means all CPUs
            return lnmi;
        }
    }

    return nullptr; // Not every core has to have one
}

IoApicInformation *Apic::getIoApicInformation(GlobalSystemInterrupt gsi) {
    ensureApicSupported();
    for (uint32_t i = 0; i < ioPlatform->ioapics.size(); ++i) {
        IoApicInformation *ioapic = ioPlatform->ioapics.get(i);
        if (gsi >= ioapic->gsiBase && gsi <= ioapic->gsiMax) {
            return ioapic;
        }
    }

    Util::Exception::throwException(Util::Exception::ILLEGAL_STATE, "No IoApicInformation matching GsiSource!");
}

IoNMIConfiguration *Apic::getIoNMIConfiguration(IoApicInformation *ioapic) {
    ensureApicSupported();
    for (uint32_t i = 0; i < ioPlatform->ionmis.size(); ++i) {
        IoNMIConfiguration *ionmi = ioPlatform->ionmis.get(i);
        if (ionmi->gsi >= ioapic->gsiBase && ionmi->gsi <= ioapic->gsiMax) {
            return ionmi;
        }
    }

    return nullptr; // NMI is optional for IO APIC
}

IoInterruptOverride *Apic::getInterruptOverride(InterruptSource source) {
    ensureInitialized();
    for (uint32_t i = 0; i < ioPlatform->irqOverrides.size(); ++i) {
        IoInterruptOverride *override = ioPlatform->irqOverrides.get(i);
        if (override->source == source) {
            return override;
        }
    }

    return nullptr;
}

IoInterruptOverride *Apic::getInterruptOverride(GlobalSystemInterrupt target) {
    ensureInitialized();
    for (uint32_t i = 0; i < ioPlatform->irqOverrides.size(); ++i) {
        IoInterruptOverride *override = ioPlatform->irqOverrides.get(i);
        if (override->target == target) {
            return override;
        }
    }

    return nullptr;
}

GlobalSystemInterrupt Apic::getInterruptOverrideTarget(InterruptSource source) {
    IoInterruptOverride *override = getInterruptOverride(source);
    if (override != nullptr) {
        return override->target;
    }

    return static_cast<GlobalSystemInterrupt>(source); // Without override Gsi and GsiSource match 1:1
}

InterruptSource Apic::getInterruptOverrideSource(GlobalSystemInterrupt target) {
    IoInterruptOverride *override = getInterruptOverride(target);
    if (override != nullptr) {
        return override->source;
    }

    return static_cast<InterruptSource>(target); // Without override Gsi and GsiSource match 1:1
}

}
