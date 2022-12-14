#ifndef HHUOS_INTERRUPTMODEL_H
#define HHUOS_INTERRUPTMODEL_H

#include <cstdint>
#include "InterruptModelStructures.h"
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

}

#endif //HHUOS_INTERRUPTMODEL_H
