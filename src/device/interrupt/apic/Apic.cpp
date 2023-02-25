#include "Apic.h"
#include "kernel/paging/Paging.h"
#include "kernel/service/FilesystemService.h"
#include "lib/util/base/Constants.h"

namespace Device {

Kernel::Logger Apic::log = Kernel::Logger::get("Apic");

void Apic::initializeCurrentLocalApic() {
    ensureApic();
    LocalApic &localApic = getCurrentLocalApic();
    if (localApic.initialized) {
        Util::Exception::throwException(Util::Exception::ILLEGAL_STATE, "Already initialized!");
    }
    if (localApic.cpuId != LocalApic::getId()) {
        Util::Exception::throwException(Util::Exception::ILLEGAL_STATE, "AP can only enable itself!");
    }
    localApic.initialize();
}

uint8_t Apic::getCpuCount() {
    ensureApic();
    return localApics.size();
}

LocalApic &Apic::getCurrentLocalApic() {
    for (uint32_t i = 0; i < localApics.size(); ++i) {
        LocalApic &localApic = *localApics.get(i);
        if (localApic.cpuId == LocalApic::getId()) {
            return localApic;
        }
    }

    Util::Exception::throwException(Util::Exception::ILLEGAL_STATE, "Couldn't find local APIC for current CPU!");
}

bool Apic::isCurrentTimerRunning() {
    for (uint32_t i = 0; i < timers.size(); ++i) {
        const ApicTimer &timer = *timers.get(i);
        if (timer.cpuId == LocalApic::getId()) {
            return true;
        }
    }

    return false;
}

void Apic::startCurrentTimer() {
    ensureApic();
    if (isCurrentTimerRunning()) {
        Util::Exception::throwException(Util::Exception::ILLEGAL_STATE, "APIC timer for this CPU has already been initialized!");
    }

    // We use multiple instances because each timer has its own timestamp
    auto *apicTimer = new Device::ApicTimer();
    apicTimer->plugin(); // Multiple invocations register multiple handlers to the APICTIMER vector
    timers.add(apicTimer);
}

ApicTimer &Apic::getCurrentTimer() {
    for (uint32_t i = 0; i < timers.size(); ++i) {
        ApicTimer &timer = *timers.get(i);
        if (timer.cpuId == LocalApic::getId()) {
            return timer;
        }
    }

    Util::Exception::throwException(Util::Exception::ILLEGAL_STATE, "Couldn't find timer for current CPU!");
}

void Apic::enableCurrentErrorHandler() {
    ensureApic();
    // This part needs to be done for each AP
    LocalApic::allow(LocalApic::ERROR);
    LocalApic::clearErrors(); // Arm the Error interrupt
}

void Apic::allow(InterruptRequest interruptRequest) {
    ensureApic();
    const IoApic::IrqOverride *override = IoApic::getOverride(interruptRequest);
    const Kernel::GlobalSystemInterrupt gsi = override == nullptr
                                              ? static_cast<Kernel::GlobalSystemInterrupt>(interruptRequest)
                                              : override->target;
    IoApic &ioApic = getIoApic(gsi); // Select responsible I/O APIC
    if (ioApic.isNonMaskableInterrupt(gsi)) {
        Util::Exception::throwException(Util::Exception::INVALID_ARGUMENT, "GSI is non-maskable!");
    }
    ioApic.allow(gsi);
}

void Apic::forbid(InterruptRequest interruptRequest) {
    ensureApic();
    const IoApic::IrqOverride *override = IoApic::getOverride(interruptRequest);
    const Kernel::GlobalSystemInterrupt gsi = override == nullptr
                                              ? static_cast<Kernel::GlobalSystemInterrupt>(interruptRequest)
                                              : override->target;
    IoApic &ioApic = getIoApic(gsi);
    if (ioApic.isNonMaskableInterrupt(gsi)) {
        Util::Exception::throwException(Util::Exception::INVALID_ARGUMENT, "GSI is non-maskable!");
    }
    ioApic.forbid(gsi);
}

bool Apic::status(InterruptRequest interruptRequest) {
    ensureApic();
    const IoApic::IrqOverride *override = IoApic::getOverride(interruptRequest);
    const Kernel::GlobalSystemInterrupt gsi = override == nullptr
                                              ? static_cast<Kernel::GlobalSystemInterrupt>(interruptRequest)
                                              : override->target;
    IoApic &ioApic = getIoApic(gsi);
    return ioApic.status(gsi);
}

void Apic::sendEndOfInterrupt(Kernel::InterruptVector vector) {
    ensureApic();
    if (isLocalInterrupt(vector) && vector != Kernel::InterruptVector::LINT1) {
        // Excludes NMI, IPIs and SMIs are also excluded, but these don't have vector numbers,
        // so they won't reach this anyway.
        LocalApic::sendEndOfInterrupt();
    } else if (isExternalInterrupt(vector)) {
        // Edge-triggered external interrupts have to be EOId in the local APIC,
        // level-triggered external interrupts can EOId in the local APIC if EOI-broadcasting is enabled,
        // otherwise they can be directly EOId in the I/O APIC by using the I/O APICs EOI register or
        // masking them and setting them as edge-triggered temporarily (which clears the remote IRR bit).
        // Here, EOI broadcasting is enabled, which makes it very simple:
        LocalApic::sendEndOfInterrupt(); // External interrupts get forwarded by the local APIC, so local EOI required
    }
}

bool Apic::isLocalInterrupt(Kernel::InterruptVector vector) {
    ensureApic();
    return vector >= Kernel::InterruptVector::CMCI && vector <= Kernel::InterruptVector::ERROR;
}

bool Apic::isExternalInterrupt(Kernel::InterruptVector vector) {
    ensureApic();
    // Remapping can be ignored here, as all GSIs are contiguous anyway
    return static_cast<Kernel::GlobalSystemInterrupt>(vector - 32) <= IoApic::systemGsiMax;
}

void Apic::countInterrupt(Kernel::InterruptVector vector) {
    // Do not throw here, just do nothing if it's an early interrupt
    if (interruptCounter != nullptr && interruptCounterWrapper != nullptr) {
        (*(*interruptCounterWrapper)[vector])[LocalApic::getId()]->inc();
    }
}

} // namespace Device
