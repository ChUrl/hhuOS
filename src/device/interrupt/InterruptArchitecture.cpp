#include "InterruptArchitecture.h"

namespace Device {

bool InterruptArchitecture::initialized;
InterruptArchitecture::IntArch InterruptArchitecture::runningArchitecture = IntArch::PIC;
LPlatformInformation *InterruptArchitecture::localPlatform = new LPlatformInformation;
IoPlatformInformation *InterruptArchitecture::ioPlatform = new IoPlatformInformation;
Kernel::Logger InterruptArchitecture::log = Kernel::Logger::get("InterruptArchitecture");

// ! Global system interrupts

GlobalSystemInterrupt::GlobalSystemInterrupt(uint8_t gsi) : gsi(static_cast<Gsi>(gsi)) {}

GlobalSystemInterrupt::GlobalSystemInterrupt(Kernel::InterruptDispatcher::Interrupt vector) {
    if (vector < Kernel::InterruptDispatcher::PIT
    || vector > static_cast<Kernel::InterruptDispatcher::Interrupt>(getGlobalGsiMax() + 32)) {
        Util::Exception::throwException(Util::Exception::INVALID_ARGUMENT, "Vector does not belong to a hardware interrupt!");
    }

    gsi = static_cast<Gsi>(vector - Kernel::InterruptDispatcher::PIT);
}

GlobalSystemInterrupt::GlobalSystemInterrupt(InterruptInput inti) {
    if (inti > static_cast<uint8_t>(getGlobalGsiMax())) {
        Util::Exception::throwException(Util::Exception::INVALID_ARGUMENT, "INTI is not supported by the system!");
    }

    if (runningArchitecture == PIC) {
        gsi = static_cast<Gsi>(inti);
    }

    if (gsi <= 15) {
        verifyApic();
        gsi = ioPlatform->intiToGsiMappings[inti];
        if (gsi == 0xFF) {
            Util::Exception::throwException(Util::Exception::UNSUPPORTED_OPERATION, "INTI is invalid!");
        }
    }

    gsi = static_cast<Gsi>(inti);
}

bool GlobalSystemInterrupt::isValid() {
    return ioPlatform->gsiToIntiMappings[gsi] != 0xFF;
}

GlobalSystemInterrupt::operator Gsi() const {
    return gsi;
}

GlobalSystemInterrupt::operator Kernel::InterruptDispatcher::Interrupt() const {
    auto vector = static_cast<Kernel::InterruptDispatcher::Interrupt>(gsi + Kernel::InterruptDispatcher::PIT);
    if (vector < Kernel::InterruptDispatcher::PIT
    || vector > static_cast<Kernel::InterruptDispatcher::Interrupt>(getGlobalGsiMax() + 32)) {
        Util::Exception::throwException(Util::Exception::UNSUPPORTED_OPERATION, "Vector does not belong to a hardware interrupt!");
    }
    return vector;
}

GlobalSystemInterrupt::operator InterruptInput() const {
    if (gsi > getGlobalGsiMax()) {
        Util::Exception::throwException(Util::Exception::UNSUPPORTED_OPERATION, "INTI is not supported by the system!");
    }

    if (runningArchitecture == PIC) {
        return static_cast<InterruptInput>(gsi);
    }

    // Devices connected to the PIC don't have to be connected to the same pins on the IO APIC
    if (gsi <= 15) {
        verifyApic();
        InterruptInput inti = ioPlatform->gsiToIntiMappings[gsi];
        if (inti == 0xFF) {
            Util::Exception::throwException(Util::Exception::UNSUPPORTED_OPERATION, "INTI is invalid!");
        }
        return inti;
    }

    // Devices that are not connected to the PIC don't matter
    return static_cast<InterruptInput>(gsi);
}

GlobalSystemInterrupt &GlobalSystemInterrupt::operator++() {
    gsi = static_cast<Gsi>(gsi + 1);
    return *this;
}

// ! Public member functions start here

void InterruptArchitecture::verifyInitialized() {
    if (!initialized) {
        Util::Exception::throwException(Util::Exception::ILLEGAL_STATE, "InterruptArchitecture not initialized!");
    }
}

void InterruptArchitecture::verifyApic() {
    verifyInitialized();
    if (runningArchitecture != APIC) {
        Util::Exception::throwException(Util::Exception::ILLEGAL_STATE, "System is not running in APIC mode!");
    }
}

bool InterruptArchitecture::hasApic() noexcept {
    return runningArchitecture == APIC;
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

void InterruptArchitecture::dumpPlatformInformation() {
    verifyInitialized();

    if (runningArchitecture == PIC) {
        log.info("Running in PIC mode");
        return;
    }

    log.info("Running in APIC mode");

    // Local APICs
    log.info("Local APIC: Supported modes: [%s%s], Selected mode: [%s], Cores: [%d], Version: [0x%x]",
             localPlatform->xApicSupported ? "xApic" : "None", localPlatform->x2ApicSupported ? ",x2Apic" : "",
             localPlatform->isX2Apic ? "x2Apic" : "xApic", localPlatform->lapics.size(), localPlatform->version);
    log.info("Local APIC MMIO: [0x%x] (phys) -> [0x%x] (virt)", localPlatform->address, localPlatform->virtAddress);

    for (auto *lapic : localPlatform->lapics) {
        log.info("- LApic: Id: [0x%x], Enabled: [%d]", lapic->id, lapic->enabled);
    }

    for (auto *lnmi : localPlatform->lnmis) {
        log.info("- NMI: Id: [0x%x], Lint: [LINT%d]", lnmi->id, lnmi->lint);
    }

    // IO APICs
    log.info("Io APIC: Version: [0x%x], EOI supported: [%d], Global GSI max: [%d]",
             ioPlatform->version, ioPlatform->eoiSupported, static_cast<uint8_t>(ioPlatform->globalGsiMax));

    for (auto *ioapic : ioPlatform->ioapics) {
        log.info("- IoApic: Id: [0x%x], MMIO: [0x%x] (phys) -> [0x%x] (virt), GSI base: [%d], GSI max: [%d]",
                 ioapic->id, ioapic->address, ioapic->virtAddress,
                 static_cast<uint8_t>(ioapic->gsiBase), static_cast<uint8_t>(ioapic->gsiMax));
    }

    for (auto *override : ioPlatform->irqOverrides) {
        log.info("- Override: Source: [%d], Target: [%d]",
                 static_cast<uint8_t>(override->source), static_cast<uint8_t>(override->target));
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

// TODO: The case where a PIC interrupt is remapped to another IO APIC is not handled
//       - If the system uses 2 IO APICs with 8 inputs each and the PIT GSI is remapped to pin 10
//         the first IO APIC will be returned because the PIT has GSI0
InterruptArchitecture::IoApicInformation *InterruptArchitecture::getIoApicInformation(GlobalSystemInterrupt gsi) {
    IoApicInformation *ioapic;
    for (uint32_t i = 0; i < ioPlatform->ioapics.size(); ++i) {
        ioapic = ioPlatform->ioapics.get(i);
        if (gsi >= ioapic->gsiBase && gsi <= ioapic->gsiMax) { // TODO: INTIs have to be compared (remapping)
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
