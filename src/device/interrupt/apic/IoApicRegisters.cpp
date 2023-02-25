#include "IoApicRegisters.h"

namespace Device {

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

}