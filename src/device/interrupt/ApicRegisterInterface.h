#ifndef HHUOS_APICREGISTERINTERFACE_H
#define HHUOS_APICREGISTERINTERFACE_H

#include "kernel/interrupt/InterruptDispatcher.h"

/*
 * I chose to implement the APIC register interaction this way because:
 * 1. Only standard C bitfields have guaranteed ordering (C99 Standard Chapter 6.7.2.1.13),
 *    as soon as the "struct" becomes a C++ "class", the ordering is implementation defined,
 *    GCC's __attribute__ ((packed)) doesn't help there
 * 2. Doing the masking everytime manually is bug prone
 */

namespace Device {

using InterruptVector = Kernel::InterruptDispatcher::Interrupt;

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

/**
 * Information obtainable from the spurious interrupt vector register of the
 * current CPU's local APIC.
 */
struct SVREntry {
    InterruptVector vector;
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

    InterruptVector vector;
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

    InterruptVector vector;
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

/**
 * Information obtainable from the redirection table of an IO APIC.
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

    InterruptVector vector;
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
