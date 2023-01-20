#include "Apic.h"
#include "lib/util/cpu/CpuId.h"

namespace Device {

Util::Data::ArrayList<IoApic *> Apic::ioApics;
Kernel::Logger Apic::log = Kernel::Logger::get("Apic");

bool Apic::isSupported() {
    return LocalApic::supportsXApic() || LocalApic::supportsX2Apic();
}

void Apic::ensureSupported() {
    if (!isSupported()) {
        Util::Exception::throwException(Util::Exception::UNSUPPORTED_OPERATION, "APIC support not present!");
    }
}

bool Apic::isInitialized() {
    bool initialized = LocalApic::isInitialized();
    for (uint32_t i = 0; i < ioApics.size(); ++i) {
        initialized &= ioApics.get(i)->isInitialized();
    }
    return initialized;
}

void Apic::ensureInitialized() {
    if (!isInitialized()) {
        Util::Exception::throwException(Util::Exception::ILLEGAL_STATE, "APIC not initialized!");
    }
}

// TODO: SMP
// bool Apic::isSMPInitialized() {
//     return LocalApic::isSmpInitialized();
// }

void Apic::initializeTimer() {
    auto* apictimer = new Device::ApicTimer();
    apictimer->plugin();
}

bool Apic::isTimerInitialized() {
    return ApicTimer::isInitialized();
}

void Apic::printDebugInfo() {
    if (!isInitialized()) {
        log.info("APIC interrupt model not initialized!");
    }

    log.info("Local APIC supported modes: [%s%s] (Current mode: [%s])",
             LocalApic::supportsXApic() ? "xApic" : "None",
             LocalApic::supportsX2Apic() ? ", x2Apic" : "",
             LocalApic::localPlatform->isX2Apic ? "x2Apic" : "xApic");
    log.info("Local APIC version: [0x%x]", LocalApic::localPlatform->version);
    log.info("Local APIC xApic MMIO: ([0x%x] (phys) -> [0x%x] (virt))",
             LocalApic::localPlatform->physAddress,
             LocalApic::localPlatform->virtAddress);
    log.info("Local APIC x2Apic MSR base: [0x%x]", LocalApic::localPlatform->msrAddress);
    log.info("Local APICs:");
    for (uint32_t i = 0; i < LocalApic::localPlatform->localApics.size(); ++i) {
        LocalApicInformation *localApic = LocalApic::localPlatform->localApics.get(i);
        log.info("- Id: [0x%x], Enabled: [%d], NMI: (LINT: [%d], Polarity: [%s], TriggerMode: [%s])",
                 localApic->id,
                 localApic->enabled,
                 localApic->nmi->lint,
                 localApic->nmi->polarity == LVTEntry::PinPolarity::HIGH ? "HIGH" : "LOW",
                 localApic->nmi->triggerMode == LVTEntry::TriggerMode::EDGE ? "EDGE" : "LEVEL");
    }

    log.info("I/O APIC version: [0x%x] (EOI support: [%d])",
             IoApic::ioPlatform->version,
             IoApic::ioPlatform->eoiSupported);
    log.info("I/O APIC max GSI: [%d]", static_cast<uint32_t>(IoApic::ioPlatform->globalMaxGsi));
    log.info("I/O APIC IRQ overrides:");
    for (uint32_t i = 0; i < IoApic::ioPlatform->overrides.size(); ++i) {
        IoApicIrqOverride *override = IoApic::ioPlatform->overrides.get(i);
        log.info("- Source: [%d], Target: [%d], Polarity: [%s], TriggerMode: [%s]",
                 static_cast<uint32_t>(override->source),
                 static_cast<uint32_t>(override->target),
                 override->polarity == REDTBLEntry::PinPolarity::HIGH ? "HIGH" : "LOW",
                 override->triggerMode == REDTBLEntry::TriggerMode::EDGE ? "EDGE" : "LEVEL");
    }
    log.info("I/O APICs:");
    for (uint32_t i = 0; i < ioApics.size(); ++i) {
        IoApicInformation *ioApic = ioApics.get(i)->ioInfo;
        log.info("- Id: [%d], GSI: (Base: [%d], Max [%d]), MMIO: ([0x%x] (phys) -> [0x%x] (virt))",
                 ioApic->id,
                 static_cast<uint32_t>(ioApic->gsiBase),
                 static_cast<uint32_t>(ioApic->gsiMax),
                 ioApic->physAddress,
                 ioApic->virtAddress);
        if (ioApic->nmi != nullptr) {
            log.info("  NMI: (GSI: [%d], Polarity: [%s], TriggerMode: [%s])",
                     static_cast<uint32_t>(ioApic->nmi->gsi),
                     ioApic->nmi->polarity == REDTBLEntry::PinPolarity::HIGH ? "HIGH" : "LOW",
                     ioApic->nmi->triggerMode == REDTBLEntry::TriggerMode::EDGE ? "EDGE" : "LEVEL");
        }
    }

}

// ! Local Apic

bool Apic::isLocalInterrupt(InterruptVector vector) {
    return vector >= InterruptVector::CMCI && vector <= InterruptVector::ERROR;
}

void Apic::sendLocalEndOfInterrupt() {
    LocalApic::sendEndOfInterrupt();
}

// ! Io Apic

bool Apic::isExternalInterrupt(InterruptVector vector) {
    return static_cast<GlobalSystemInterrupt>(vector - 32) <= IoApic::ioPlatform->globalMaxGsi;
}

void Apic::allowExternalInterrupt(InterruptSource interruptSource) {
    GlobalSystemInterrupt gsi = IoApic::ioPlatform->getIoApicIrqOverrideTarget(interruptSource);
    IoApic &ioApic = getIoApic(gsi);
    ioApic.allow(gsi);
}

void Apic::forbidExternalInterrupt(InterruptSource interruptSource) {
    GlobalSystemInterrupt gsi = IoApic::ioPlatform->getIoApicIrqOverrideTarget(interruptSource);
    IoApic &ioApic = getIoApic(gsi);
    ioApic.forbid(gsi);
}

bool Apic::externalInterruptStatus(InterruptSource interruptSource) {
    GlobalSystemInterrupt gsi = IoApic::ioPlatform->getIoApicIrqOverrideTarget(interruptSource);
    IoApic &ioApic = getIoApic(gsi);
    return ioApic.status(gsi);
}

void Apic::sendExternalEndOfInterrupt(InterruptVector vector) {
    auto interruptSource = static_cast<InterruptSource>(vector - 32);
    GlobalSystemInterrupt gsi = IoApic::ioPlatform->getIoApicIrqOverrideTarget(interruptSource);
    IoApic &ioApic = getIoApic(gsi);

    LocalApic::sendEndOfInterrupt();
    ioApic.sendEndOfInterrupt(vector);
}

// ! Private functions

IoApic &Apic::getIoApic(GlobalSystemInterrupt gsi) {
    ensureInitialized();

    for (uint32_t i = 0; i < ioApics.size(); ++i) {
        IoApic *ioApic = ioApics.get(i);
        if (gsi >= ioApic->ioInfo->gsiBase && gsi <= ioApic->ioInfo->gsiMax) {
            return *ioApic;
        }
    }

    Util::Exception::throwException(Util::Exception::INVALID_ARGUMENT, "No I/O APIC found for the supplied GSI!");
}

}
