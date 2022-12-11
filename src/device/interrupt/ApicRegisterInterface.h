#ifndef HHUOS_APICREGISTERINTERFACE_H
#define HHUOS_APICREGISTERINTERFACE_H

#include <cstdint>
#include "kernel/interrupt/InterruptDispatcher.h"

// TODO: Is it fine to keep this here? As the structs are now no longer "hidden" behind the class name

// NOTE: I chose this approach instead of bitfields because the ordering of bitfields is not fixed

namespace Device {

// ! LApic register interface

// IA-32 Architecture Manual Chapter 10.4.4
/**
 * Information obtainable from the local APIC's model specific register.
 */
typedef struct MSREntry {
    bool isBSP;
    bool isX2Apic;
    bool isHWEnabled;
    uint32_t baseField;
} MSREntry;

// IA-32 Architecture Manual Chapter 10.9
/**
 * Information obtainable from the spurious interrupt vector register of the
 * current CPU's local APIC.
 */
typedef struct SVREntry {
    Kernel::InterruptDispatcher::Interrupt vector;
    bool isSWEnabled;
    bool hasFocusProcessorChecking;
    bool hasEOIBroadcastSuppression;
} SVREntry;

// IA-32 Architecture Manual Chapter 10.5.1
enum class LVTDeliveryMode : uint8_t {
    FIXED = 0,
    SMI = 0b10,
    NMI = 0b100,
    INIT = 0b101,
    EXTINT = 0b111
};
enum class LVTDeliveryStatus : uint8_t {
    IDLE = 0, PENDING = 1
};
enum class LVTPinPolarity : uint8_t {
    HIGH = 0, LOW = 1
};
enum class LVTTriggerMode : uint8_t {
    EDGE = 0, LEVEL = 1
};
enum class LVTTimerMode : uint8_t {
    ONESHOT = 0, PERIODIC = 1
};

/**
 * Information obtainable from the local vector table of the current CPU's local APIC.
 * Affects handling of local interrupts.
 */
typedef struct LVTEntry {
    Kernel::InterruptDispatcher::Interrupt vector;
    LVTDeliveryMode deliveryMode; // All except timer
    LVTDeliveryStatus deliveryStatus;
    LVTPinPolarity pinPolarity; // Only LINT0, LINT1
    LVTTriggerMode triggerMode; // Only LINT0, LINT1
    bool isMasked;
    LVTTimerMode timerMode; // Only timer
} LVTEntry;

// IA-32 Architecture Manual Chapter 10.6.1
enum class ICRDeliveryMode : uint8_t {
    FIXED = 0,
    LOWPRIO = 1, // Model specific
    SMI = 0b10,
    NMI = 0b100,
    INIT = 0b101,
    STARTUP = 0b110
};
enum class ICRDestinationMode : uint8_t {
    PHYSICAL = 0, LOGICAL = 1
};
enum class ICRDeliveryStatus : uint8_t {
    IDLE = 0, PENDING = 1
};
enum class ICRLevel : uint8_t {
    DEASSERT = 0, ASSERT = 1
};
enum class ICRTriggerMode : uint8_t {
    EDGE = 0, LEVEL = 1
};
enum class ICRDestinationShorthand : uint8_t { // If used ICR_DESTINATION_FIELD is ignored
    SELF = 1,
    ALL = 0b10,
    ALL_NO_SELF = 0b11
};

/**
 * Information obtainable from the interrupt command register of the current CPU's local APIC.
 * Affects what interprocessor interrupt is issued.
 */
typedef struct ICREntry {
    Kernel::InterruptDispatcher::Interrupt vector;
    ICRDeliveryMode deliveryMode;
    ICRDestinationMode destinationMode;
    ICRDeliveryStatus deliveryStatus;
    ICRLevel level;
    ICRTriggerMode triggerMode;
    ICRDestinationShorthand destinationShorthand;
    uint8_t destination;
} ICREntry;

// ! IoApic register interface

enum class REDTBLDeliveryMode : uint8_t {
    FIXED = 0,
    LOWPRIO = 1,
    SMI = 0b10,
    NMI = 0b100,
    INIT = 0b101,
    EXTINT = 0b111
};
enum class REDTBLDestinationMode : uint8_t {
    PHYSICAL = 0, LOGICAL = 1
};
enum class REDTBLDeliveryStatus : uint8_t {
    IDLE = 0, PENDING = 1
};
enum class REDTBLPinPolarity : uint8_t {
    HIGH = 0, LOW = 1
};
enum class REDTBLTriggerMode : uint8_t {
    EDGE = 0, LEVEL = 1
};
typedef struct REDTBLEntry {
    Kernel::InterruptDispatcher::Interrupt vector;
    REDTBLDeliveryMode deliveryMode;
    REDTBLDestinationMode destinationMode;
    REDTBLDeliveryStatus deliveryStatus;
    REDTBLPinPolarity pinPolarity;
    REDTBLTriggerMode triggerMode;
    bool isMasked;
    uint8_t destination;
} REDTBLEntry;

}

#endif //HHUOS_APICREGISTERINTERFACE_H
