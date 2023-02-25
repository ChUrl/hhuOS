#ifndef HHUOS_IOAPICREGISTERS_H
#define HHUOS_IOAPICREGISTERS_H

#include "kernel/interrupt/InterruptVector.h"

namespace Device {

/**
 * @brief Information obtainable from the redirection table of an IO APIC.
 *
 * Affects handling of external interrupts.
 */
struct REDTBLEntry {
    enum class DeliveryMode : uint8_t {
        FIXED = 0,
        LOWPRIO = 1,
        SMI = 0b10,
        NMI = 0b100,
        INIT = 0b101,
        EXTINT = 0b111
    };
    enum class DestinationMode : uint8_t {
        PHYSICAL = 0,
        LOGICAL = 1
    };
    enum class DeliveryStatus : uint8_t {
        IDLE = 0,
        PENDING = 1
    };
    enum class PinPolarity : uint8_t {
        HIGH = 0,
        LOW = 1
    };
    enum class TriggerMode : uint8_t {
        EDGE = 0,
        LEVEL = 1
    };

    Kernel::InterruptVector vector;
    DeliveryMode deliveryMode;
    DestinationMode destinationMode;
    DeliveryStatus deliveryStatus; // RO
    PinPolarity pinPolarity;
    TriggerMode triggerMode;
    bool isMasked;
    uint8_t destination;

    REDTBLEntry() = default;

    explicit REDTBLEntry(uint64_t registerValue);

    explicit operator uint64_t() const;
};

}

#endif //HHUOS_IOAPICREGISTERS_H
