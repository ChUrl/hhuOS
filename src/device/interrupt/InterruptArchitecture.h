#ifndef HHUOS_INTERRUPTARCHITECTURE_H
#define HHUOS_INTERRUPTARCHITECTURE_H

#include <cstdint>
#include "ApicRegisterInterface.h"
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

class InterruptArchitecture {
    friend class LApic; // Initializes some values
    friend class IoApic; // Initializes some values

public:
    enum IntArch : uint8_t {
        PIC = 0,
        APIC = 1
    };

    // ! Global system interrupts

    enum InterruptInput : uint8_t {};

    // TODO: This class uses the outside class members of InterruptArchitecture, I don't like this
    /**
     * Global system interrupts abstract the hardware interrupt pins from the software.
     * Supports conversion from/to vector numbers.
     */
    struct GlobalSystemInterrupt {
        enum Gsi : uint8_t {
            // PIC compatible GSIs
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

            // Other GSIs, currently none are used
        };

        GlobalSystemInterrupt() = default;
        ~GlobalSystemInterrupt() = default;

        explicit GlobalSystemInterrupt(uint8_t gsi); // From int because enums can be implicitly converted to int

        /**
         * Convert an interrupt vector number to a GlobalSystemInterrupt.
         * GSIs are mapped to vector numbers 1:1 but translated by 32 (NOT influenced by IO APIC remappings!).
         */
        explicit GlobalSystemInterrupt(Kernel::InterruptDispatcher::Interrupt vector);

        /**
         * Convert the InterruptInput to a GlobalSystemInterrupt.
         * This takes into account the IO APIC remappings.
         */
        explicit GlobalSystemInterrupt(InterruptInput inti);

        operator Gsi() const; // To enum because enums can be implicitly converted to int

        /**
         * Convert the GlobalSystemInterrupt to an interrupt vector number.
         * GSIs are mapped to vector numbers 1:1 but translated by 32 (NOT influenced by IO APIC remappings!).
         */
        explicit operator Kernel::InterruptDispatcher::Interrupt() const;

        /**
         * Convert the GlobalSystemInterrupt to an IO APIC interrupt input.
         * This takes into account the IO APIC remappings.
         */
        explicit operator InterruptInput() const;

        bool isValid();

        // Convenient for iterating over GSIs
        GlobalSystemInterrupt &operator++();

    private:
        Gsi gsi;
    };

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
        GlobalSystemInterrupt gsiBase; // GSI where IO APIC interrupt inputs start
        GlobalSystemInterrupt gsiMax; // Set by IoApic after MMIO is available
    };

    struct IoInterruptOverride {
        uint8_t bus; // TODO: What is this
        GlobalSystemInterrupt source;
        InterruptInput target;
        REDTBLEntry::PinPolarity polarity;
        REDTBLEntry::TriggerMode triggerMode;
    };

    struct IoNMIConfiguration {
        REDTBLEntry::PinPolarity polarity;
        REDTBLEntry::TriggerMode triggerMode;
        GlobalSystemInterrupt gsi;
    };

    /**
     * This describes the hardware configuration of the system for all IO APICs.
     */
    struct IoPlatformInformation {
        uint8_t version; // Set by IoApic after MMIO is available
        bool eoiSupported; // Set by IoApic after MMIO is available
        // TODO: This only allows to map PIC GSIs to INTIs in the range [0, 15] (use a hashmap or something?)
        GlobalSystemInterrupt intiToGsiMappings[16]; // Inti is the index
        InterruptInput gsiToIntiMappings[16]; // Gsi is the index
        GlobalSystemInterrupt globalGsiMax; // Systemwide max gsi
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

    static void dumpPlatformInformation();

private:
    static LApicInformation *getLApicInformation(uint8_t lapicId);
    static LNMIConfiguration *getLNMIConfiguration(LApicInformation *lapic);
    static IoApicInformation *getIoApicInformation(GlobalSystemInterrupt gsi);
    static IoNMIConfiguration *getIoNMIConfiguration(IoApicInformation *ioapic);

private:
    static bool initialized;
    static IntArch runningArchitecture;

    static LPlatformInformation *localPlatform;
    static IoPlatformInformation *ioPlatform;

    static Kernel::Logger log;
};

template<typename Backend>
void InterruptArchitecture::initialize() {
    Backend::initializeLPlatformInformation(localPlatform);
    Backend::initializeIoPlatformInformation(ioPlatform);

    // Initialize mappings
    for (uint8_t i = 0; i < 16; ++i) {
        ioPlatform->gsiToIntiMappings[i] = static_cast<InterruptInput>(i);
        ioPlatform->intiToGsiMappings[i] = static_cast<GlobalSystemInterrupt>(i);
    }
    for (auto *mapping : ioPlatform->irqOverrides) {
        ioPlatform->gsiToIntiMappings[mapping->source] = mapping->target;
        ioPlatform->intiToGsiMappings[mapping->target] = mapping->source;

        // If e.g. the PIT (GSI0) is remapped to IO APIC INTI 2, the default mapping of GSI2 is no longer valid.
        ioPlatform->gsiToIntiMappings[mapping->target] = static_cast<InterruptInput>(0xFF);
        ioPlatform->intiToGsiMappings[mapping->source] = static_cast<GlobalSystemInterrupt>(0xFF);
    }

    if (localPlatform->xApicSupported || localPlatform->x2ApicSupported) {
        runningArchitecture = APIC;
    } else {
        runningArchitecture = PIC;
    }

    initialized = true;
}

// TODO: Does this have a benefit to just move the structs out of the InterruptArchitecture class?
//       - Yes, the structs can access the InterruptArchitecture members directly
// The InterruptArchitecture defines some structures that should be in the Device namespace directly
using InterruptInput = InterruptArchitecture::InterruptInput;
using GlobalSystemInterrupt = InterruptArchitecture::GlobalSystemInterrupt;
using LApicInformation = InterruptArchitecture::LApicInformation;
using LNMIConfiguration = InterruptArchitecture::LNMIConfiguration;
using LPlatformInformation = InterruptArchitecture::LPlatformInformation;
using IoApicInformation = InterruptArchitecture::IoApicInformation;
using IoInterruptOverride = InterruptArchitecture::IoInterruptOverride;
using IoNMIConfiguration = InterruptArchitecture::IoNMIConfiguration;
using IoPlatformInformation = InterruptArchitecture::IoPlatformInformation;

}

#endif //HHUOS_INTERRUPTARCHITECTURE_H
