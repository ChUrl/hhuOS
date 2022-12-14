#ifndef HHUOS_INTERRUPTMODELSTRUCTURES_H
#define HHUOS_INTERRUPTMODELSTRUCTURES_H

#include "ApicRegisterInterface.h"
#include "GlobalSystemInterrupt.h"

namespace Device {

/**
 * Provide a name to integers describing a hardware interupt input.
 */
enum InterruptInput : uint8_t {};

// ! Processor local APIC architecture

struct LApicInformation {
    uint8_t acpiId; // ACPI also stores this
    uint8_t id; // TODO: Check if ACPI sets this by itself of if the ID reg values have to be signalled to ACPI
    // TODO: Does this mean the processor can't be used currently but could be started? Or can't be started at all?
    bool enabled; // If false the operating system can't use this processor
};

struct LNMIConfiguration {
    uint8_t acpiId; // 0xFF means all CPUs
    uint8_t id; // Added for convenience (matches LApicInformation::id), 0xFF means all CPUs
    LVTEntry::PinPolarity polarity;
    LVTEntry::TriggerMode triggerMode;
    uint8_t lint; // Local APIC pin number
};

/**
 * This describes the hardware configuration of the system for all local APICs.
 */
struct LPlatformInformation {
    bool xApicSupported;
    bool x2ApicSupported;
    bool isX2Apic; // Set by LApic
    uint8_t version; // Set by LApic after MMIO is available
    uint32_t address;
    uint32_t virtAddress; // Set by LApic after MMIO is available
    Util::Data::ArrayList<LApicInformation *> lapics;
    Util::Data::ArrayList<LNMIConfiguration *> lnmis;
};

// ! IO APIC architecture

struct IoApicInformation {
    uint8_t id;
    uint32_t address;
    uint32_t virtAddress; // Set by IoApic after MMIO is available
    GlobalSystemInterrupt gsiBase; // GSI where IO APIC interrupt inputs start // TODO: Switch to InterruptInput
    GlobalSystemInterrupt gsiMax; // Set by IoApic after MMIO is available // TODO: Switch to InterruptInput
};

/**
 * This struct represents an ISA IRQ override.
 * Example: When PIT (IRQ0) is connected to IO APIC INTI2: gsi = 0, inti = 2.
 */
struct IoInterruptOverride {
    uint8_t bus; // 0 means irqSource is ISA IRQ relative
    GlobalSystemInterrupt gsi;
    InterruptInput inti;
    REDTBLEntry::PinPolarity polarity;
    REDTBLEntry::TriggerMode triggerMode;
};

struct IoNMIConfiguration {
    REDTBLEntry::PinPolarity polarity;
    REDTBLEntry::TriggerMode triggerMode;
    GlobalSystemInterrupt gsi; // TODO: Is this before ore after remapping?
};

/**
 * This describes the hardware configuration of the system for all IO APICs.
 */
struct IoPlatformInformation {
    uint8_t version; // Set by IoApic after MMIO is available
    bool eoiSupported; // Set by IoApic after MMIO is available
    GlobalSystemInterrupt globalGsiMax; // Systemwide max gsi // TODO: Switch to InterruptInput
    Util::Data::ArrayList<IoApicInformation *> ioapics;
    Util::Data::ArrayList<IoInterruptOverride *> irqOverrides;
    Util::Data::ArrayList<IoNMIConfiguration *> ionmis;
};

}

#endif //HHUOS_INTERRUPTMODELSTRUCTURES_H
