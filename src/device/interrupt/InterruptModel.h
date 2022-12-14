#ifndef HHUOS_INTERRUPTMODEL_H
#define HHUOS_INTERRUPTMODEL_H

#include <cstdint>
#include "ApicRegisterInterface.h"
#include "GlobalSystemInterrupt.h"
#include "kernel/log/Logger.h"

/*
 * InterruptArchitecture:
 * This class implements a way to access information about the running system's interrupt model.
 * It can be initialized from different backends, the existing one uses ACPI 1.0b.
 * Backends needed for increased compatibility would be newer ACPI versions (especially >= ACPI 2.0)
 * and Intel's MultiProcessor tables (for ancient systems that don't support ACPI at all).
 * Some values stored in this class have to be read from memory mapped APIC registers.
 * These will be set when initializing the local APICs and IO APICs.
 *
 * GlobalSystemInterrupt:
 * The 8295 PIC is usually used in a master-slave configuration with 2 PICs, so a PC/AT compatible system
 * always support at least 16 hardware interrupts (including the PIC cascade).
 * Modern systems using the APIC interrupt architecture can support a variable amount (and significantly more).
 * To decouple the firmware/OS from the physical interrupt architecture, ACPI introduces "Global System Interrupts".
 * GSIs only abstract hardware interrupts (no exceptions/faults) and start at 0!
 * Because PC/AT compatibility is maintained, the first 16 (GSIs 0 - 15) global system interrupts are identity
 * mapped to the PIC hardware interrupts.
 * This introduces a problem, because the APIC architecture doesn't enforce how devices are physically wired
 * to the IO APIC's interrupt inputs.
 * To solve this, ACPI provides "Interrupt Source Overrides" in the MADT, which specify variances
 * between PIC and APIC hardware interrupt configurations.
 *
 * Technically the notion of "GSIs" only got introduced in some ACPI version after 1.0b,
 * but I see no problem with using it globally (even when ACPI/APIC is not available).
 */

namespace Device {

class InterruptModel {
    friend class LApic; // Initializes some values // TODO
    friend class IoApic; // Initializes some values // TODO

public:
    enum Model : uint8_t {
        PIC = 0,
        APIC = 1,
        // There are more models on other architectures
    };

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

public:
    /**
     * Initialize the InterruptArchitecture.
     *
     * @tparam T The backend to use
     */
    template<typename T>
    static void initialize();

    /**
     * Ensures that the InterruptArchitecture has been initialized.
     */
    static void verifyInitialized();

    /**
     * Ensure that the system runs with the APIC interrupt model.
     */
    static void verifyApic();

    /**
     * Determine if the system is currently running with the APIC interrupt model.
     */
    static bool hasApic() noexcept;

    /**
     * Determine the number of GSIs the system supports.
     *
     * @return The last supported GSI
     */
    static GlobalSystemInterrupt getGlobalGsiMax();

    static Util::Data::ArrayList<LApicInformation *> &lapics();

    static Util::Data::ArrayList<IoApicInformation *> &ioapics();

    static LApicInformation *getLApicInformation(uint8_t lapicId);

    static LNMIConfiguration *getLNMIConfiguration(LApicInformation *lapic);

    static IoApicInformation *getIoApicInformation(GlobalSystemInterrupt gsi);

    static IoNMIConfiguration *getIoNMIConfiguration(IoApicInformation *ioapic);

    static IoInterruptOverride *getInterruptOverride(GlobalSystemInterrupt gsi);

    static bool hasOverride(InterruptInput inti);

    static void dumpPlatformInformation();

private:
    static bool initialized;
    static Model systemModel;

    static LPlatformInformation *localPlatform;
    static IoPlatformInformation *ioPlatform;

    static Kernel::Logger log;
};

template<typename Backend>
void InterruptModel::initialize() {
    Backend::initializeLPlatformInformation(localPlatform);
    Backend::initializeIoPlatformInformation(ioPlatform);

    if (localPlatform->xApicSupported || localPlatform->x2ApicSupported) {
        systemModel = APIC;
    } else {
        systemModel = PIC;
    }

    initialized = true;
}

// TODO: Does this have a benefit to just move the structs out of the InterruptArchitecture class?
//       - Yes, the structs can access the InterruptArchitecture members directly
// The InterruptArchitecture defines some structures that should be in the Device namespace directly
using InterruptInput = InterruptModel::InterruptInput;
using LApicInformation = InterruptModel::LApicInformation;
using LNMIConfiguration = InterruptModel::LNMIConfiguration;
using LPlatformInformation = InterruptModel::LPlatformInformation;
using IoApicInformation = InterruptModel::IoApicInformation;
using IoInterruptOverride = InterruptModel::IoInterruptOverride;
using IoNMIConfiguration = InterruptModel::IoNMIConfiguration;
using IoPlatformInformation = InterruptModel::IoPlatformInformation;

}

#endif //HHUOS_INTERRUPTMODEL_H
