#ifndef HHUOS_APICSTRUCTURES_H
#define HHUOS_APICSTRUCTURES_H

#include "ApicRegisterInterface.h"

// TODO: Put these inside of Apic class?

/*
 * This header defines structures used by the InterruptModel.
 * I wanted to seperate the InterruptModel from the ACPI table memory representation
 * because they're semantically different.
 * Additionally, this makes it easier to support different ACPI versions or MP tables.
 */

namespace Device {

/**
 * @brief The GlobalSystemInterrupts abstract the hardware interrupt model from the system.
 *
 * They cannot be named statically, as this depends on the system configuration.
 * GlobalSystemInterrupts map 1:1 to IO APIC interrupt inputs.
 */
enum GlobalSystemInterrupt : uint32_t {};

// TODO: Move these (independent of APIC)
/**
 * @brief This describes devices that trigger external interrupts.
 *
 * InterruptSources map 1:1 to PIC interrupt inputs and system interrupt vectors (translated by 32)
 */
enum InterruptSource : uint8_t {
    // PIC compatible devices
    PIT = 0x00,
    KEYBOARD = 0x01,
    CASCADE = 0x02,
    COM2 = 0x03,
    COM1 = 0x04,
    LPT2 = 0x05,
    FLOPPY = 0x06,
    LPT1 = 0x07,
    RTC = 0x08,
    MOUSE = 0x0C,
    FPU = 0x0D,
    PRIMARY_ATA = 0x0E,
    SECONDARY_ATA = 0x0F,

    // Other devices
};

/**
 * @brief Contains basic information about a single local APIC.
 */
struct LApicInformation {
    uint8_t acpiId; ///< @brief The ID used by ACPI
    // TODO: Check if ACPI sets this by itself of if the ID reg values have to be signalled to ACPI
    uint8_t id; ///< @brief The ID also present in the local APIC's ID register
    bool enabled; ///< @brief If set to false the operating system can't use this processor
    bool isX2Apic; ///< @brief Indicates the mode the local APIC actually runs in
    // TODO: Put bool initialized; here?
    // TODO: If local Apics would be relocated individually the address would have to be stored here
    //       - Memory should be allocated with mapIO (the one that only takes a size), the physical
    //         address should be written to the local Apics MSR
    //       - The physical and virtual addresses should be stored in the corresponding LApicInformation struct
    //       - If this should be supported all local APIC register accesses would need a LApicInformation* arg,
    //         similar to IoApic, or would have to lookup the address everytime...
};

/**
 * @brief Contains information about a single local APIC's non maskable interrupt source.
 */
struct LNMIConfiguration {
    uint8_t acpiId; ///< @brief 0xFF means all CPUs
    uint8_t id; ///< @brief Matches LApicInformation::id, 0xFF means all CPUs
    LVTEntry::PinPolarity polarity;
    LVTEntry::TriggerMode triggerMode;
    uint8_t lint; ///< @brief Local APIC pin number
};

/**
 * @brief This describes the hardware configuration of the system for all local APICs.
 */
struct LPlatformInformation {
    bool xApicSupported; ///< @brief xAPIC platform only supports register access over MMIO
    bool x2ApicSupported; ///< @brief x2APIC platfrom only supports register access over MSRs
    uint8_t version;
    uint32_t address = 0;
    uint32_t virtAddress = 0;
    Util::Data::ArrayList<LApicInformation *> lapics;
    Util::Data::ArrayList<LNMIConfiguration *> lnmis;
};

/**
 * @brief Contains basic information about a single IO APIC.
 */
struct IoApicInformation {
    uint8_t id;
    uint32_t address = 0;
    uint32_t virtAddress = 0;
    GlobalSystemInterrupt gsiBase; ///< @brief First GSI handled by this IO APIC
    GlobalSystemInterrupt gsiMax = static_cast<GlobalSystemInterrupt>(0); ///< @brief Last GSI handled by this IO APIC
    // TODO: Put bool initialized; here?
};

/**
 * @brief This struct represents an ISA IRQ override.
 *
 * Example: When PIT (IRQ0) is connected to IO APIC INTI2: source = 0, target = 2.
 */
struct IoInterruptOverride {
    uint8_t bus; ///< @brief 0 means irqSource is ISA IRQ relative
    InterruptSource source; ///< @brief The ISA IRQ equivalent GSI that will be remapped
    GlobalSystemInterrupt target; ///< @brief The GSI the device is actually connected to
    REDTBLEntry::PinPolarity polarity;
    REDTBLEntry::TriggerMode triggerMode;
};

/**
 * @brief Contains information about a IO local APIC's non maskable interrupt source.
 */
struct IoNMIConfiguration {
    REDTBLEntry::PinPolarity polarity;
    REDTBLEntry::TriggerMode triggerMode;
    GlobalSystemInterrupt gsi;
};

/**
 * @brief This describes the hardware configuration of the system for all IO APICs.
 */
struct IoPlatformInformation {
    uint8_t version;
    bool eoiSupported; ///< @brief Older IO APICs require recieving EOIs sent by the local APIC
    GlobalSystemInterrupt globalMaxGsi = static_cast<GlobalSystemInterrupt>(15); ///< @brief The last GSI the system supports (15 for PIC)
    Util::Data::ArrayList<IoApicInformation *> ioapics;
    Util::Data::ArrayList<IoInterruptOverride *> irqOverrides;
    Util::Data::ArrayList<IoNMIConfiguration *> ionmis;
};

}

#endif //HHUOS_APICSTRUCTURES_H
