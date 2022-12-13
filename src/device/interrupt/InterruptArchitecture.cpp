#include "InterruptArchitecture.h"

namespace Device {

bool InterruptArchitecture::initialized;
InterruptArchitecture::IntArch InterruptArchitecture::runningArchitecture = IntArch::PIC;

InterruptArchitecture::LPlatformInformation *InterruptArchitecture::localPlatform =
        new InterruptArchitecture::LPlatformInformation;
InterruptArchitecture::IoPlatformInformation *InterruptArchitecture::ioPlatform =
        new InterruptArchitecture::IoPlatformInformation;

Kernel::Logger InterruptArchitecture::log = Kernel::Logger::get("InterruptArchitecture");

// ! Public member functions start here

void InterruptArchitecture::verifyInitialized() {
    if (!initialized) {
        Util::Exception::throwException(Util::Exception::ILLEGAL_STATE, "InterruptArchitecture not initialized!");
    }
}

bool InterruptArchitecture::hasApic() {
    verifyInitialized();
    return runningArchitecture == APIC;
}

void InterruptArchitecture::verifyApic() {
    if (!hasApic()) {
        Util::Exception::throwException(Util::Exception::ILLEGAL_STATE, "System is not running in APIC mode!");
    }
}

InterruptArchitecture::IntArch InterruptArchitecture::getRunningArchitecture() {
    return runningArchitecture;
}

void InterruptArchitecture::dumpPlatformInformation() {
    verifyInitialized();

    if (runningArchitecture == PIC) {
        log.info("Running in PIC mode");
        return;
    }

    log.info("Running in APIC mode");

    // Local APICs
    log.info("Local Apic information:");
    log.info("Supported modes: %s%s, Selected: %s, Cores: [%d], Version: [0x%x]",
             localPlatform->xApicSupported ? "[xApic]" : "[None]", localPlatform->x2ApicSupported ? " [x2Apic]" : "",
             localPlatform->isX2Apic ? "[x2Apic]" : "[xApic]", localPlatform->lapics.size(), localPlatform->version);
    log.info("MMIO: [0x%x] (phys) -> [0x%x] (virt)", localPlatform->address, localPlatform->virtAddress);

    log.info("Local Apic status:");
    for (auto *lapic : localPlatform->lapics) {
        log.info("- Id: [0x%x], Enabled: [%d]", lapic->id, lapic->enabled);
    }

    log.info("Local NMI status:");
    for (auto *lnmi : localPlatform->lnmis) {
        log.info("- Id: [0x%x], Lint: [LINT%d]", lnmi->id, lnmi->lint);
    }

    // IO APICs
    log.info("Io Apic information:");
    log.info("Version: [0x%x], EOI supported: [%d], Global GSI max: [%d]",
             ioPlatform->version, ioPlatform->eoiSupported, static_cast<uint8_t>(ioPlatform->globalGsiMax));

    log.info("Io Apic status:");
    for (auto *ioapic : ioPlatform->ioapics) {
        log.info("- Id: [0x%x], MMIO: [0x%x] (phys) -> [0x%x] (virt), GSI base: [%d], GSI max: [%d]",
                 ioapic->id, ioapic->address, ioapic->virtAddress,
                 static_cast<uint8_t>(ioapic->gsiBase), static_cast<uint8_t>(ioapic->gsiMax));
    }

    log.info("Io IRQ overrides:");
    for (auto *override : ioPlatform->irqOverrides) {
        log.info("- IRQ source: [%d], GSI target: [%d]",
                 static_cast<uint8_t>(override->source), static_cast<uint8_t>(override->target));
    }
    if (ioPlatform->irqOverrides.size() == 0) {
        log.info("- There are no IRQ overrides");
    }

    log.info("Io NMI status:");
    for (auto *nmi : ioPlatform->ionmis) {
        log.info("- GSI: [%d]", static_cast<uint8_t>(nmi->gsi));
    }
    if (ioPlatform->ionmis.size() == 0) {
        log.info("- There are no IO NMIs");
    }
}

GlobalSystemInterrupt InterruptArchitecture::getGlobalGsiMax() {
    verifyApic();
    return ioPlatform->globalGsiMax;
}

Util::Data::ArrayList<InterruptArchitecture::LApicInformation *> &InterruptArchitecture::lapics() {
    verifyApic();
    return localPlatform->lapics;
}

Util::Data::ArrayList<InterruptArchitecture::IoApicInformation *> &InterruptArchitecture::ioapics() {
    verifyApic();
    return ioPlatform->ioapics;
}

// ! Private member functions start here

// NOTE: Do not use iterators in the get... functions as some of these may be called from an interrupt handler
InterruptArchitecture::LApicInformation *InterruptArchitecture::getLApicInformation(uint8_t id) {
    LApicInformation *lapic;
    for (uint32_t i = 0; i < localPlatform->lapics.size(); ++i) {
        lapic = localPlatform->lapics.get(i);
        if (lapic->id == id) {
            return lapic;
        }
    }

    Util::Exception::throwException(Util::Exception::ILLEGAL_STATE, "LApic::getLApicConfiguration(): Didn't find configuration matching ID!");
}

// There is a maximum of 1 NMI configuration per core
InterruptArchitecture::LNMIConfiguration *InterruptArchitecture::getLNMIConfiguration(LApicInformation *lapic) {
    LNMIConfiguration *lnmi;
    for (uint32_t i = 0; i < localPlatform->lnmis.size(); ++i) {
        lnmi = localPlatform->lnmis.get(i);
        if (lnmi->acpiId == 0xFF || lnmi->id == lapic->id) { // 0xFF means all CPUs
            return lnmi;
        }
    }

    return nullptr; // Not every core has to have one
}

InterruptArchitecture::IoApicInformation *InterruptArchitecture::getIoApicInformation(GlobalSystemInterrupt gsi) {
    IoApicInformation *ioapic;
    for (uint32_t i = 0; i < ioPlatform->ioapics.size(); ++i) {
        ioapic = ioPlatform->ioapics.get(i);
        if (gsi >= ioapic->gsiBase && gsi <= ioapic->gsiMax) {
            return ioapic;
        }
    }

    Util::Exception::throwException(Util::Exception::ILLEGAL_STATE, "LApic::getIoApicConfiguration(): Didn't find configuration matching GSI!");
}

// TODO: Can a single IO APIC have multiple NMIs?
InterruptArchitecture::IoNMIConfiguration *InterruptArchitecture::getIoNMIConfiguration(IoApicInformation *ioapic) {
    // Check if a NMI that is assigned to one of this IO APICs pins exists
    IoNMIConfiguration *ionmi;
    for (uint32_t i = 0; i < ioPlatform->ionmis.size(); ++i) {
        ionmi = ioPlatform->ionmis.get(i);
        if (ionmi->gsi >= ioapic->gsiBase && ionmi->gsi <= ioapic->gsiMax) {
            return ionmi;
        }
    }

    return nullptr; // NMI is optional for IO APIC
}

}
