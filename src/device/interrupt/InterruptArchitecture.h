#ifndef HHUOS_INTERRUPTARCHITECTURE_H
#define HHUOS_INTERRUPTARCHITECTURE_H

#include <cstdint>
#include "ApicRegisterInterface.h"
#include "GlobalSystemInterrupt.h"
#include "kernel/log/Logger.h"

/*
 * This class implements a way to access information about the running system's interrupt model.
 * It can be initialized from different backends, the existing one uses ACPI 1.0b.
 * Backends needed for increased compatibility would be newer ACPI versions (especially >= ACPI 2.0)
 * and Intel's MultiProcessor tables (for ancient systems that don't support ACPI at all).
 *
 * TODO: I hate this, but separation is also weird since semantically it fits
 * Some values stored in this class have to be read from memory mapped APIC registers.
 * These will be initialized by the local APIC or IO APIC itself.
 */

namespace Device {

class InterruptArchitecture {
    friend class Pic;
    friend class LApic;
    friend class IoApic;
    friend class ApicTimer;

public:
    enum IntArch : uint8_t {
        PIC = 0,
        APIC = 1
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
        uint8_t id; // Added for convenience (matches LApicConfiguration::id), 0xFF means all CPUs
        LVTEntry::PinPolarity polarity;
        LVTEntry::TriggerMode triggerMode;
        uint8_t lint;
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
        GlobalSystemInterrupt target;
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
        uint32_t irqToGsiMappings[16]; // GSIs for PIC IRQs (IRQ is the index)
        uint8_t gsiToIrqMappings[16]; // Inverse of irqToGsiMappings
        GlobalSystemInterrupt globalGsiMax; // Systemwide max gsi
        Util::Data::ArrayList<IoApicInformation *> ioapics;
        Util::Data::ArrayList<IoInterruptOverride *> irqOverrides;
        Util::Data::ArrayList<IoNMIConfiguration *> ionmis;
    };

public:
    template<typename T>
    static void initialize();

    static void verifyInitialized();
    static bool hasApic();
    static void verifyApic();
    static IntArch getRunningArchitecture();
    static void dumpPlatformInformation();

    static GlobalSystemInterrupt getGlobalGsiMax();

    static Util::Data::ArrayList<LApicInformation *> &lapics();
    static Util::Data::ArrayList<IoApicInformation *> &ioapics();

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

    if (localPlatform->xApicSupported || localPlatform->x2ApicSupported) {
        runningArchitecture = APIC;
    } else {
        runningArchitecture = PIC;
    }

    initialized = true;
}

}

#endif //HHUOS_INTERRUPTARCHITECTURE_H
