#include "ApicRegisters.h"

namespace Device {

// IA-32 manual, sec. 3.11.12.1
BaseMSREntry::BaseMSREntry(uint64_t registerValue)
  : isBSP(registerValue & (1 << 8)),
    isX2Apic(registerValue & (1 << 10)),
    isXApic(registerValue & (1 << 11)),
    baseField(registerValue & 0xFFFFF000) {}

BaseMSREntry::operator uint64_t() const {
    return static_cast<uint64_t>(isBSP) << 8 | static_cast<uint64_t>(isX2Apic) << 10 | static_cast<uint64_t>(isXApic) << 11 | static_cast<uint64_t>(baseField) << 12;
}

// IA-32 manual, sec. 3.11.9
SVREntry::SVREntry(uint32_t registerValue)
  : vector(static_cast<Kernel::InterruptVector>(registerValue & 0xFF)),
    isSWEnabled(registerValue & (1 << 8)),
    hasFocusProcessorChecking(registerValue & (1 << 9)),
    hasEOIBroadcastSuppression(registerValue & (1 << 12)) {}

SVREntry::operator uint32_t() const {
    return static_cast<uint32_t>(vector) | static_cast<uint32_t>(isSWEnabled) << 8 | static_cast<uint32_t>(hasFocusProcessorChecking) << 9 | static_cast<uint32_t>(hasEOIBroadcastSuppression) << 12;
}

// IA-32 manual, sec. 3.11.5.1
LVTEntry::LVTEntry(uint32_t registerValue)
  : vector(static_cast<Kernel::InterruptVector>(registerValue & 0xFF)),
    deliveryMode(static_cast<DeliveryMode>((registerValue & (0b111 << 8)) >> 8)),
    deliveryStatus(static_cast<DeliveryStatus>((registerValue & (1 << 12)) >> 12)),
    pinPolarity(static_cast<PinPolarity>((registerValue & (1 << 13)) >> 13)),
    triggerMode(static_cast<TriggerMode>((registerValue & (1 << 15)) >> 15)),
    isMasked(registerValue & (1 << 16)),
    timerMode(static_cast<TimerMode>((registerValue & (0b11 << 17)) >> 17)) {}

LVTEntry::operator uint32_t() const {
    return static_cast<uint32_t>(vector) | static_cast<uint32_t>(deliveryMode) << 8 | static_cast<uint32_t>(pinPolarity) << 13 | static_cast<uint32_t>(triggerMode) << 15 | static_cast<uint32_t>(isMasked) << 16 | static_cast<uint32_t>(timerMode) << 17;
}

// IA-32 manual, sec. 3.11.6.1
ICREntry::ICREntry(uint64_t registerValue)
  : vector(static_cast<Kernel::InterruptVector>(registerValue & 0xFF)),
    deliveryMode(static_cast<DeliveryMode>((registerValue & (0b111 << 8)) >> 8)),
    destinationMode(static_cast<DestinationMode>((registerValue & (1 << 11)) >> 11)),
    deliveryStatus(static_cast<DeliveryStatus>((registerValue & (1 << 12)) >> 12)),
    level(static_cast<Level>((registerValue & (1 << 14)) >> 14)),
    triggerMode(static_cast<TriggerMode>((registerValue & (1 << 15)) >> 15)),
    destinationShorthand(static_cast<DestinationShorthand>((registerValue & (0b11 << 18)) >> 18)),
    destination(registerValue >> 56) {}

ICREntry::operator uint64_t() const {
    return static_cast<uint64_t>(vector) | static_cast<uint64_t>(deliveryMode) << 8 | static_cast<uint64_t>(destinationMode) << 11 | static_cast<uint64_t>(deliveryStatus) << 12 | static_cast<uint64_t>(level) << 14 | static_cast<uint64_t>(triggerMode) << 15 | static_cast<uint64_t>(destinationShorthand) << 18 | static_cast<uint64_t>(destination) << 56;
}

// ICH5 spec, sec. 9.5.8
REDTBLEntry::REDTBLEntry(uint64_t registerValue)
  : vector(static_cast<Kernel::InterruptVector>(registerValue & 0xFF)),
    deliveryMode(static_cast<DeliveryMode>((registerValue & (0b111 << 8)) >> 8)),
    destinationMode(static_cast<DestinationMode>((registerValue & (1 << 11)) >> 11)),
    deliveryStatus(static_cast<DeliveryStatus>((registerValue & (1 << 12)) >> 12)),
    pinPolarity(static_cast<PinPolarity>((registerValue & (1 << 13)) >> 13)),
    triggerMode(static_cast<TriggerMode>((registerValue & (1 << 15)) >> 15)),
    isMasked(registerValue & (1 << 16)),
    destination(registerValue >> 56) {}

REDTBLEntry::operator uint64_t() const {
    return static_cast<uint64_t>(vector) | static_cast<uint64_t>(deliveryMode) << 8 | static_cast<uint64_t>(destinationMode) << 11 | static_cast<uint64_t>(pinPolarity) << 13 | static_cast<uint64_t>(triggerMode) << 15 | static_cast<uint64_t>(isMasked) << 16 | static_cast<uint64_t>(destination) << 56;
}

} // namespace Device