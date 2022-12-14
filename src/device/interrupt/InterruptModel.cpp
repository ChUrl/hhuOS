#include "InterruptModel.h"

namespace Device {

bool InterruptModel::initialized;
InterruptModel::Model InterruptModel::systemModel = Model::PIC;
LPlatformInformation *InterruptModel::localPlatform = new LPlatformInformation;
IoPlatformInformation *InterruptModel::ioPlatform = new IoPlatformInformation;
Kernel::Logger InterruptModel::log = Kernel::Logger::get("InterruptArchitecture");

// ! Public member functions start here

void InterruptModel::verifyInitialized() {
    if (!initialized) {
        Util::Exception::throwException(Util::Exception::ILLEGAL_STATE, "InterruptArchitecture not initialized!");
    }
}

void InterruptModel::verifyApic() {
    verifyInitialized();
    if (systemModel != APIC) {
        Util::Exception::throwException(Util::Exception::ILLEGAL_STATE, "System is not running in APIC mode!");
    }
}

bool InterruptModel::hasApic() noexcept {
    return systemModel == APIC;
}

GlobalSystemInterrupt InterruptModel::getGlobalGsiMax() {
    verifyInitialized();
    return ioPlatform->globalGsiMax;
}

Util::Data::ArrayList<LApicInformation *> &InterruptModel::lapics() {
    verifyApic();
    return localPlatform->lapics;
}

Util::Data::ArrayList<IoApicInformation *> &InterruptModel::ioapics() {
    verifyApic();
    return ioPlatform->ioapics;
}

// NOTE: Do not use iterators in the get... functions as some of these may be called from an interrupt handler
LApicInformation *InterruptModel::getLApicInformation(uint8_t id) {
    verifyApic();
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
LNMIConfiguration *InterruptModel::getLNMIConfiguration(LApicInformation *lapic) {
    verifyApic();
    LNMIConfiguration *lnmi;
    for (uint32_t i = 0; i < localPlatform->lnmis.size(); ++i) {
        lnmi = localPlatform->lnmis.get(i);
        if (lnmi->acpiId == 0xFF || lnmi->id == lapic->id) { // 0xFF means all CPUs
            return lnmi;
        }
    }

    return nullptr; // Not every core has to have one
}

// TODO: The case where a PIC interrupt is remapped to another IO APIC is not handled
//       - If the system uses 2 IO APICs with 8 inputs each and the PIT GSI is remapped to pin 10
//         the first IO APIC will be returned because the PIT has GSI0
IoApicInformation *InterruptModel::getIoApicInformation(GlobalSystemInterrupt gsi) {
    verifyApic();
    IoApicInformation *ioapic;
    for (uint32_t i = 0; i < ioPlatform->ioapics.size(); ++i) {
        ioapic = ioPlatform->ioapics.get(i);
        if (gsi >= ioapic->gsiBase && gsi <= ioapic->gsiMax) { // TODO: INTIs have to be compared (remapping)
            return ioapic;
        }
    }

    Util::Exception::throwException(Util::Exception::ILLEGAL_STATE,
                                    "LApic::getIoApicConfiguration(): Didn't find configuration matching GSI!");
}

// TODO: Can a single IO APIC have multiple NMIs?
IoNMIConfiguration *InterruptModel::getIoNMIConfiguration(IoApicInformation *ioapic) {
    verifyApic();
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

IoInterruptOverride *InterruptModel::getInterruptOverride(GlobalSystemInterrupt gsi) {
    verifyInitialized();

    // Devices connected to the PIC don't have to be connected to the same pins on the IO APIC
    for (uint32_t i = 0; i < ioPlatform->irqOverrides.size(); ++i) {
        auto *override = ioPlatform->irqOverrides.get(i);
        if (override->gsi == gsi) {
            return override;
        }
    }

    // If no override exists the identity mapping is used
    if (gsi > ioPlatform->globalGsiMax) {
        Util::Exception::throwException(Util::Exception::UNSUPPORTED_OPERATION,
                                        "GSI is not supported by the system!");
    }

    return nullptr;
}

bool InterruptModel::hasOverride(InterruptInput inti) {
    verifyInitialized();

    // Check if another GSI has been remapped to this interrupt input
    for (uint32_t i = 0; i < ioPlatform->irqOverrides.size(); ++i) {
        auto *override = ioPlatform->irqOverrides.get(i);
        if (override->inti == inti) {
            return true;
        }
    }

    if (inti > static_cast<uint8_t>(ioPlatform->globalGsiMax)) {
        Util::Exception::throwException(Util::Exception::UNSUPPORTED_OPERATION,
                                        "INTI is not supported by the system!");
    }

    return false;
}

void InterruptModel::dumpPlatformInformation() {
    verifyInitialized();

    if (systemModel == PIC) {
        log.info("Running in PIC mode");
        return;
    }

    log.info("Running in APIC mode");

    // Local APICs
    log.info("Local APIC: Supported modes: [%s%s], Selected mode: [%s], Cores: [%d], Version: [0x%x]",
             localPlatform->xApicSupported ? "xApic" : "None",
             localPlatform->x2ApicSupported ? ",x2Apic" : "",
             localPlatform->isX2Apic ? "x2Apic" : "xApic",
             localPlatform->lapics.size(),
             localPlatform->version);
    log.info("Local APIC MMIO: [0x%x] (phys) -> [0x%x] (virt)", localPlatform->address, localPlatform->virtAddress);

    for (auto *lapic : localPlatform->lapics) {
        log.info("- LApic: Id: [0x%x], Enabled: [%d]", lapic->id, lapic->enabled);
    }

    for (auto *lnmi : localPlatform->lnmis) {
        log.info("- NMI: Id: [0x%x], Lint: [LINT%d]", lnmi->id, lnmi->lint);
    }

    // IO APICs
    log.info("Io APIC: Version: [0x%x], EOI supported: [%d], Global GSI max: [%d]",
             ioPlatform->version,
             ioPlatform->eoiSupported,
             static_cast<uint8_t>(ioPlatform->globalGsiMax));

    for (auto *ioapic : ioPlatform->ioapics) {
        log.info("- IoApic: Id: [0x%x], MMIO: [0x%x] (phys) -> [0x%x] (virt), GSI base: [%d], GSI max: [%d]",
                 ioapic->id,
                 ioapic->address,
                 ioapic->virtAddress,
                 static_cast<uint8_t>(ioapic->gsiBase),
                 static_cast<uint8_t>(ioapic->gsiMax));
    }

    for (auto *override : ioPlatform->irqOverrides) {
        log.info("- Override: IRQ Source: [%d], GSI Target: [%d]",
                 static_cast<uint8_t>(override->inti),
                 static_cast<uint8_t>(override->gsi));
    }
    if (ioPlatform->irqOverrides.size() == 0) {
        log.info("- There are no IRQ overrides");
    }

    for (auto *nmi : ioPlatform->ionmis) {
        log.info("- NMI: GSI: [%d]", static_cast<uint8_t>(nmi->gsi));
    }
    if (ioPlatform->ionmis.size() == 0) {
        log.info("- There are no IO NMIs");
    }
}

}
