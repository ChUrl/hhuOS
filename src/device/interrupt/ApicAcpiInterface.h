#ifndef HHUOS_APICACPIINTERFACE_H
#define HHUOS_APICACPIINTERFACE_H

#include "ApicRegisterInterface.h"
#include "GlobalSystemInterrupt.h"
#include "InterruptSource.h"

namespace Device {

// TODO: Rename Information to Configuration

/**
 * @brief Information about a single local APIC's non maskable interrupt source.
 */
struct LocalApicNMI {
    LVTEntry::PinPolarity polarity;
    LVTEntry::TriggerMode triggerMode;
    uint8_t lint; ///< @brief Local APIC pin number
};

/**
 * @brief Information about a single local APIC.
 */
struct LocalApicInformation {
    uint8_t id;
    bool enabled = false; ///< @brief If false this processor can't be used
    LocalApicNMI *nmi = nullptr; ///< @brief Information about this local APIC's non maskable interrupt pin
};

/**
 * @brief Information about all local APICs.
 */
struct LocalApicPlatform {
    bool isX2Apic; ///< @brief The xApic architecture uses MMIO for register access, x2Apic uses MSRs
    uint8_t version;
    uint32_t physAddress = 0; // xApic MMIO
    uint32_t virtAddress = 0; // xApic MMIO
    uint32_t msrAddress = 0x800; // x2Apic
    Util::Data::ArrayList<LocalApicInformation *> localApics;

    [[nodiscard]] LocalApicInformation *getLocalApicInformation(uint8_t id) const;
    [[nodiscard]] LocalApicNMI *getLocalNMIConfiguration(uint8_t id) const;
};

/**
 * @brief Information about a single IO APIC's non maskable interrupt source.
 */
struct IoApicNMI {
    REDTBLEntry::PinPolarity polarity;
    REDTBLEntry::TriggerMode triggerMode;
    GlobalSystemInterrupt gsi;
};

/**
 * @brief Information about a single IO APIC.
 */
struct IoApicInformation {
    uint8_t id;
    uint32_t physAddress = 0;
    uint32_t virtAddress = 0;
    GlobalSystemInterrupt gsiBase; ///< @brief First GSI handled by this IO APIC
    GlobalSystemInterrupt gsiMax = GlobalSystemInterrupt(0); ///< @brief Last GSI handled by this IO APIC
    IoApicNMI *nmi = nullptr; ///< @brief Information about this IO APIC's non maskable interrupt pin
};

/**
 * @brief Represents an ISA IRQ to GSI mapping/override.
 *
 * Example: When PIT (IRQ0) is connected to IO APIC INTI2: source = 0, target = 2.
 */
struct IoApicIrqOverride {
    uint8_t bus; ///< @brief 0 means irqSource is ISA IRQ relative
    InterruptSource source; ///< @brief The ISA IRQ equivalent GSI that will be remapped
    GlobalSystemInterrupt target; ///< @brief The GSI the device is actually connected to
    REDTBLEntry::PinPolarity polarity;
    REDTBLEntry::TriggerMode triggerMode;
};

/**
 * @brief Information about all IO APICs.
 */
struct IoApicPlatform {
    uint8_t version;
    bool eoiSupported; ///< @brief Older IO APICs require recieving EOIs sent by the local APIC
    GlobalSystemInterrupt globalMaxGsi = GlobalSystemInterrupt(0); ///< @brief The last GSI the system supports
    Util::Data::ArrayList<IoApicIrqOverride *> overrides; ///< @brief All overridden ISA IRQs, equal for all IO APICs

    [[nodiscard]] IoApicIrqOverride *getIoApicIrqOverride(GlobalSystemInterrupt target) const;
    [[nodiscard]] IoApicIrqOverride *getIoApicIrqOverride(InterruptSource source) const;
    [[nodiscard]] InterruptSource getIoApicIrqOverrideSource(GlobalSystemInterrupt target) const;
    [[nodiscard]] GlobalSystemInterrupt getIoApicIrqOverrideTarget(InterruptSource source) const;
};

}

#endif //HHUOS_APICACPIINTERFACE_H
