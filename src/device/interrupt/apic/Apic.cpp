#include "Apic.h"
#include "kernel/paging/Paging.h"
#include "kernel/service/FilesystemService.h"
#include "lib/util/base/Constants.h"

namespace Device {

Kernel::Logger Apic::log = Kernel::Logger::get("Apic");

bool Apic::isSupported() {
    // Only supports ACPI 1.0 fully, there are changes in later versions, but it should still work, so don't force.
    return LocalApic::supportsXApic() && Acpi::isAvailable(); // && Acpi::getRsdp().revision == 0;
}

bool Apic::isEnabled() {
    return apicEnabled;
}

void Apic::ensureApic() {
    if (!apicEnabled) {
        Util::Exception::throwException(Util::Exception::ILLEGAL_STATE, "APIC not initialized!");
    }
}

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
    return usableProcessors;
}

LocalApic &Apic::getCurrentLocalApic() {
    ensureApic();
    return *(*localApics)[LocalApic::getId()];
}

bool Apic::isCurrentTimerRunning() {
    ensureApic();
    return (*localTimers)[LocalApic::getId()] != nullptr;
}

void Apic::startCurrentTimer() {
    ensureApic();
    if (isCurrentTimerRunning()) {
        Util::Exception::throwException(Util::Exception::ILLEGAL_STATE, "APIC timer for this CPU has already been initialized!");
    }

    // We use multiple instances because each timer has its own timestamp
    auto *apicTimer = new Device::ApicTimer();
    apicTimer->plugin(); // Multiple invocations register multiple handlers to the APICTIMER vector
    (*localTimers)[LocalApic::getId()] = apicTimer;
}

ApicTimer &Apic::getCurrentTimer() {
    ensureApic();
    if (!isCurrentTimerRunning()) {
        Util::Exception::throwException(Util::Exception::ILLEGAL_STATE, "Couldn't find timer for current CPU!");
    }

    return *(*localTimers)[LocalApic::getId()];
}

void Apic::enableCurrentErrorHandler() {
    ensureApic();
    // This part needs to be done for each AP
    LocalApic::allow(LocalApic::ERROR);
    LocalApic::clearErrors(); // Arm the Error interrupt
}

void Apic::allow(InterruptRequest interruptRequest) {
    ensureApic();
    const Kernel::GlobalSystemInterrupt gsi = mapInterruptRequest(interruptRequest);
    if (ioApic->isNonMaskableInterrupt(gsi)) {
        Util::Exception::throwException(Util::Exception::INVALID_ARGUMENT, "GSI is non-maskable!");
    }
    ioApic->allow(gsi);
}

void Apic::forbid(InterruptRequest interruptRequest) {
    ensureApic();
    const Kernel::GlobalSystemInterrupt gsi = mapInterruptRequest(interruptRequest);
    if (ioApic->isNonMaskableInterrupt(gsi)) {
        Util::Exception::throwException(Util::Exception::INVALID_ARGUMENT, "GSI is non-maskable!");
    }
    ioApic->forbid(gsi);
}

bool Apic::status(InterruptRequest interruptRequest) {
    ensureApic();
    const Kernel::GlobalSystemInterrupt gsi = mapInterruptRequest(interruptRequest);
    return ioApic->status(gsi);
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
    // Do not throw here, just don't count if it's an early interrupt
    if (counters != nullptr && wrappers != nullptr) {
        // Array width * row + col
        (*wrappers)[localApics->length() * vector + LocalApic::getId()]->inc();
    }
}

Kernel::GlobalSystemInterrupt Apic::mapInterruptRequest(InterruptRequest interruptRequest) {
    const IoApic::IrqOverride *override = IoApic::getOverride(interruptRequest);
    return override == nullptr ? static_cast<Kernel::GlobalSystemInterrupt>(interruptRequest) : override->target;
}

} // namespace Device
