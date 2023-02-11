#ifndef HHUOS_APICACPIINTERFACE_H
#define HHUOS_APICACPIINTERFACE_H

#include "ApicRegisters.h"
#include "device/interrupt/InterruptRequest.h"
#include "device/power/acpi/Acpi.h"
#include "kernel/interrupt/GlobalSystemInterrupt.h"

namespace Device {

/**
 * @brief Information about a single local APIC.
 */
struct LocalApicInformation {
    uint8_t id;      ///< @brief The local APIC id, in SMP systems this is also the processor id
    bool enabled;    ///< @brief If false, this processor can't be used by the OS
    uint8_t nmiLint; ///< @brief Local APIC pin number used as NMI source, usually 0x01
    LVTEntry::PinPolarity nmiPolarity;
    LVTEntry::TriggerMode nmiTriggerMode;

    LocalApicInformation(const Acpi::ProcessorLocalApic *processorLocalApic, const Acpi::LocalApicNmi *localApicNmi);

    ~LocalApicInformation() = default;
};

/**
 * @brief Information about all local APICs.
 */
struct LocalApicPlatform {
    bool isX2Apic = false;       ///< @brief The APIC architecture used (xApic or x2Apic)
    uint32_t physAddress;        ///< @brief The physical MMIO address used for register access in xApic mode
    uint32_t virtAddress = 0;    ///< @brief The virtual MMIO address used for register access in xApic mode
    uint32_t msrAddress = 0x800; ///< @brief The MSR base address used for register access in x2Apic mode

    explicit LocalApicPlatform(uint32_t physAddress);

    ~LocalApicPlatform() = default;
};

/**
 * @brief Information about a single IO APIC.
 */
struct IoApicInformation {
    uint8_t id;
    uint32_t physAddress;
    uint32_t virtAddress = 0;
    Kernel::GlobalSystemInterrupt gsiBase;                                   ///< @brief First GSI handled by this IO APIC
    Kernel::GlobalSystemInterrupt gsiMax = Kernel::GlobalSystemInterrupt(0); ///< @brief Last GSI handled by this IO APIC
    bool hasNmi;
    Kernel::GlobalSystemInterrupt nmiGsi;
    REDTBLEntry::PinPolarity nmiPolarity;
    REDTBLEntry::TriggerMode nmiTriggerMode;

    IoApicInformation(const Acpi::IoApic *ioApic, const Acpi::NmiSource *nmiSource);

    ~IoApicInformation() = default;
};

/**
 * @brief Information about all IO APICs.
 */
struct IoApicPlatform {
    /**
    * @brief Represents an IRQ to GSI mapping/override.
    */
    struct IoApicIrqOverride {
        uint8_t bus;                          ///< @brief 0 means irqSource is ISA IRQ relative.
        InterruptRequest source;              ///< @brief The ISA IRQ equivalent GSI that will be remapped.
        Kernel::GlobalSystemInterrupt target; ///< @brief The GSI the device is actually connected to.
        REDTBLEntry::PinPolarity polarity;    ///< @brief If this is BUS, then the polarity is the bus default.
        REDTBLEntry::TriggerMode triggerMode; ///< @brief If this is BUS, then the trigger mode is the bus default.

        explicit IoApicIrqOverride(const Acpi::InterruptSourceOverride *interruptSourceOverride);

        ~IoApicIrqOverride() = default;
    };

    uint8_t version = 0;
    bool directEoiSupported = false;                                               ///< @brief Older IO APICs require recieving EOIs sent by the local APIC
    Kernel::GlobalSystemInterrupt globalMaxGsi = Kernel::GlobalSystemInterrupt(0); ///< @brief The last GSI the system supports
    Util::ArrayList<IoApicIrqOverride *> overrides;                                ///< @brief All overridden ISA IRQs, equal for all IO APICs

    explicit IoApicPlatform(Util::ArrayList<const Acpi::InterruptSourceOverride *> *interruptSourceOverrides);

    ~IoApicPlatform() = default;

    [[nodiscard]] IoApicIrqOverride *getIoApicIrqOverride(Kernel::GlobalSystemInterrupt target) const;
    [[nodiscard]] IoApicIrqOverride *getIoApicIrqOverride(InterruptRequest source) const;
    [[nodiscard]] InterruptRequest getIoApicIrqOverrideSource(Kernel::GlobalSystemInterrupt target) const;
    [[nodiscard]] Kernel::GlobalSystemInterrupt getIoApicIrqOverrideTarget(InterruptRequest source) const;
};

} // namespace Device

#endif //HHUOS_APICACPIINTERFACE_H
