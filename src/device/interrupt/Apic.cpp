#include "Apic.h"
#include "lib/util/cpu/CpuId.h"

namespace Device {

Util::Data::ArrayList<IoApic *> Apic::ioApics;
Kernel::Logger Apic::log = Kernel::Logger::get("Apic");

bool Apic::isInitialized() {
    bool initialized = LocalApic::isInitialized();
    for (uint32_t i = 0; i < ioApics.size(); ++i) {
        initialized &= ioApics.get(i)->isInitialized();
    }
    return initialized;
}

bool Apic::isSMPInitialized() {
    return LocalApic::isSmpInitialized();
}

void Apic::ensureInitialized() {
    if (!isInitialized()) {
        Util::Exception::throwException(Util::Exception::ILLEGAL_STATE, "APIC not initialized!");
    }
}

bool Apic::isSupported() {
    bool supported = false;
    auto features = Util::Cpu::CpuId::getCpuFeatures();
    for (auto feature: features) {
        if (feature == Util::Cpu::CpuId::CpuFeature::APIC) {
            supported = true;
        }
        if (feature == Util::Cpu::CpuId::CpuFeature::X2APIC) {
            supported = true;
        }
    }
    return supported;
}

void Apic::ensureApicSupported() {
    if (!isSupported()) {
        Util::Exception::throwException(Util::Exception::UNSUPPORTED_OPERATION, "APIC support not present!");
    }
}

// ! Local Apic

bool Apic::isLocalInterrupt(InterruptVector vector) {
    return vector >= InterruptVector::CMCI && vector <= InterruptVector::ERROR;
}

void Apic::allowLocalInterrupt(LocalApic::LocalInterrupt localInterrupt) {
    LocalApic::allow(localInterrupt);
}

void Apic::forbidLocalInterrupt(LocalApic::LocalInterrupt localInterrupt) {
    LocalApic::forbid(localInterrupt);
}

bool Apic::localInterruptStatus(LocalApic::LocalInterrupt localInterrupt) {
    return LocalApic::status(localInterrupt);
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
    IoApic *ioApic = getIoApic(gsi);
    ioApic->allow(gsi);
}

void Apic::forbidExternalInterrupt(InterruptSource interruptSource) {
    GlobalSystemInterrupt gsi = IoApic::ioPlatform->getIoApicIrqOverrideTarget(interruptSource);
    IoApic *ioApic = getIoApic(gsi);
    ioApic->forbid(gsi);
}

bool Apic::externalInterruptStatus(InterruptSource interruptSource) {
    GlobalSystemInterrupt gsi = IoApic::ioPlatform->getIoApicIrqOverrideTarget(interruptSource);
    IoApic *ioApic = getIoApic(gsi);
    return ioApic->status(gsi);
}

void Apic::sendExternalEndOfInterrupt(InterruptVector vector) {
    auto interruptSource = static_cast<InterruptSource>(vector - 32);
    GlobalSystemInterrupt gsi = IoApic::ioPlatform->getIoApicIrqOverrideTarget(interruptSource);
    IoApic *ioApic = getIoApic(gsi);

    LocalApic::sendEndOfInterrupt();
    ioApic->sendEndOfInterrupt(vector);
}

// ! Private functions

IoApic *Apic::getIoApic(GlobalSystemInterrupt gsi) {
    for (uint32_t i = 0; i < ioApics.size(); ++i) {
        IoApic *ioApic = ioApics.get(i);
        if (gsi >= ioApic->ioInfo->gsiBase && gsi <= ioApic->ioInfo->gsiMax) {
            return ioApic;
        }
    }

    // TODO: Use reference type as return type to prevent nullable
    return nullptr;
}

}
