#ifndef HHUOS_APICREGISTERINTERFACE_H
#define HHUOS_APICREGISTERINTERFACE_H

#include <cstdint>
#include "kernel/interrupt/InterruptDispatcher.h"

// TODO: Is it fine to keep this here? As the structs are now no longer "hidden" behind the class name

// TODO: I could implement implicit conversion from uin32_t for the entries?

// NOTE: I chose this approach instead of bitfields because the ordering of bitfields is not fixed (?)
// NOTE: __attribute__ ((packed)) only guarantees that there are no gaps
// NOTE: C99 Standard 7.1.2.13 says ordering in structs is fixed?
// NOTE: So if I use a plain C struct without any C++ additions it should work?

namespace Device {

// ! LApic register interface

// IA-32 Architecture Manual Chapter 10.4.4
/**
 * Information obtainable from the local APIC's model specific register.
 */
struct MSREntry {
    bool isBSP;
    bool isX2Apic;
    bool isHWEnabled;
    uint32_t baseField;

    MSREntry() = default;
    explicit MSREntry(uint64_t registerValue);
    explicit operator uint64_t() const;
};

// IA-32 Architecture Manual Chapter 10.9
/**
 * Information obtainable from the spurious interrupt vector register of the
 * current CPU's local APIC.
 */
struct SVREntry {
    Kernel::InterruptDispatcher::Interrupt vector;
    bool isSWEnabled;
    bool hasFocusProcessorChecking;
    bool hasEOIBroadcastSuppression;

    SVREntry() = default;
    explicit SVREntry(uint32_t registerValue);
    explicit operator uint32_t() const;
};

/**
 * Information obtainable from the local vector table of the current CPU's local APIC.
 * Affects handling of local interrupts.
 */
struct LVTEntry {
    // IA-32 Architecture Manual Chapter 10.5.1
    enum class DeliveryMode : uint8_t {
        FIXED = 0,
        SMI = 0b10,
        NMI = 0b100,
        INIT = 0b101,
        EXTINT = 0b111
    };
    enum class DeliveryStatus : uint8_t {
        IDLE = 0, PENDING = 1
    };
    enum class PinPolarity : uint8_t {
        HIGH = 0, LOW = 1
    };
    enum class TriggerMode : uint8_t {
        EDGE = 0, LEVEL = 1
    };
    enum class TimerMode : uint8_t {
        ONESHOT = 0, PERIODIC = 1
    };

    Kernel::InterruptDispatcher::Interrupt vector;
    DeliveryMode deliveryMode; // All except timer
    DeliveryStatus deliveryStatus; // RO
    PinPolarity pinPolarity; // Only LINT0, LINT1
    TriggerMode triggerMode; // Only LINT0, LINT1
    bool isMasked;
    TimerMode timerMode; // Only timer

    LVTEntry() = default;
    explicit LVTEntry(uint32_t registerValue);
    explicit operator uint32_t() const;
};

/**
 * Information obtainable from the interrupt command register of the current CPU's local APIC.
 * Affects what interprocessor interrupt is issued.
 */
struct ICREntry {
    // IA-32 Architecture Manual Chapter 10.6.1
    enum class DeliveryMode : uint8_t {
        FIXED = 0,
        LOWPRIO = 1, // Model specific
        SMI = 0b10,
        NMI = 0b100,
        INIT = 0b101,
        STARTUP = 0b110
    };
    enum class DestinationMode : uint8_t {
        PHYSICAL = 0, LOGICAL = 1
    };
    enum class DeliveryStatus : uint8_t {
        IDLE = 0, PENDING = 1
    };
    enum class Level : uint8_t {
        DEASSERT = 0, ASSERT = 1
    };
    enum class TriggerMode : uint8_t {
        EDGE = 0, LEVEL = 1
    };
    enum class DestinationShorthand : uint8_t { // If used ICR_DESTINATION_FIELD is ignored
        SELF = 1,
        ALL = 0b10,
        ALL_NO_SELF = 0b11
    };

    Kernel::InterruptDispatcher::Interrupt vector;
    DeliveryMode deliveryMode;
    DestinationMode destinationMode;
    DeliveryStatus deliveryStatus; // RO
    Level level;
    TriggerMode triggerMode;
    DestinationShorthand destinationShorthand;
    uint8_t destination;

    ICREntry() = default;
    explicit ICREntry(uint64_t registerValue);
    explicit operator uint64_t() const;
};

// ! IoApic register interface

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
        PHYSICAL = 0, LOGICAL = 1
    };
    enum class DeliveryStatus : uint8_t {
        IDLE = 0, PENDING = 1
    };
    enum class PinPolarity : uint8_t {
        HIGH = 0, LOW = 1
    };
    enum class TriggerMode : uint8_t {
        EDGE = 0, LEVEL = 1
    };

    Kernel::InterruptDispatcher::Interrupt vector;
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

#endif //HHUOS_APICREGISTERINTERFACE_H
