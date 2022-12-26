#ifndef HHUOS_APIC_H
#define HHUOS_APIC_H

#include "ApicStructures.h"
#include "kernel/log/Logger.h"

// TODO: Should this class really be concerned with stuff other than APIC model?
//       - Could also be renamed to Apic... or sth.

namespace Device {

/**
 * @brief This class implements a way to access information about the running system's interrupt model.
 *
 * The InterruptModel is only concerned about hardware interrupts, software interrupts are independent.
 * Both external (PIC/IO APIC) and local (local APIC) interrupts are considered hardware interrupts.
 *
 * Some values stored in this class have to be read from memory mapped APIC registers.
 * These will be set when initializing the local APICs and IO APICs.
 *
 * Note:
 * For a technically correct implementation of the APIC interrupt model, an AML interpreter is needed
 * to communicate the usage of the IO APIC to ACPI.
 * Hot-plug devices
 */
class Apic {
    friend class LocalApic; // Initializes some values depending on MMIO
    friend class IoApic; // Initializes some values depending on MMIO

public:
    // TODO: Also initialize LApic/IoApic here?
    /**
     * @brief Initialize the InterruptModel.
     *
     * @tparam T The parser to use
     */
    template<typename T>
    static void initialize();

    /**
     * @brief Check if the system is running with the APIC interrupt model.
     */
    static bool isInitialized();

    /**
     * @brief Ensure that the InterruptModel has been initialized.
     *
     * Only ensures that initial system information was read.
     */
    static void ensureInitialized();

    /**
     * @brief Ensure that the system supports the APIC interrupt model.
     */
    static void ensureApicSupported();

    /**
     * @brief Check if the system supports the APIC interrupt model.
     *
     * Only checks for support, system could still be in PIC mode.
     */
    static bool apicSupported();

    /**
     * @brief Check if an interrupt vector belongs to an external hardware interrupt (PIC/IO APIC).
     *
     * Depends on the number of external interrupts (GSIs) the system supports.
     */
    static bool isExternalInterrupt(InterruptVector vector);

    /**
     * @brief Check if an interrupt vector belongs to a local interrupt (local APIC).
     */
    static bool isLocalInterrupt(InterruptVector vector);

    /**
     * @brief Determine the number of GSIs the system supports.
     *
     * Returns GlobalSystemInterrupt(15) if running with PIC model,
     * APIC model usually has max GlobalSystemInterrupt(23) but can vary.
     *
     * @return The last supported GSI
     */
    static GlobalSystemInterrupt getGlobalMaxGsi();

    /**
     * @brief Log the InterruptModel information.
     */
    static void dumpPlatformInformation();

private:
    /**
     * @brief Get information about the system's local APICs.
     *
     * This function is exclusive to the APIC interrupt model.
     *
     * @return A list of LApicInformation structures
     */
    static Util::Data::ArrayList<LApicInformation *> &lapics();

    /**
     * @brief Get information about the system's IO APICs.
     *
     * This function is exclusive to the APIC interrupt model.
     *
     * @return A list of IoApicInformation structures
     */
    static Util::Data::ArrayList<IoApicInformation *> &ioapics();

    /**
     * @brief Get information about a specific local APIC.
     *
     * This function is exclusive to the APIC interrupt model.
     *
     * @param lapicId The id of the local APIC
     * @return A LApicInformation structure
     */
    static LApicInformation *getLApicInformation(uint8_t lapicId);

    /**
     * @brief Get information about a specific local APIC's non-maskable-interrupt source.
     *
     * This function is exclusive to the APIC interrupt model.
     *
     * @param lapic The LApicInformation structure of the local APIC
     * @return A LNMIConfiguration structure
     */
    static LNMIConfiguration *getLNMIConfiguration(LApicInformation *lapic);

    /**
     * @brief Get information about a specific IO APIC by supplying a pin number.
     *
     * This function is exclusive to the APIC interrupt model.
     *
     * @param gsi The interrupt input on the IO APIC
     * @return An IoApicInformation structure
     */
    static IoApicInformation *getIoApicInformation(GlobalSystemInterrupt gsi);

    /**
     * @brief Get information about a specific IO APIC's non-maskable-interrupt source.
     *
     * This function is exclusive to the APIC interrupt model.
     *
     * @param ioapic The IoApicInformation structure of the local APIC
     * @return An IoNMIConfiguration structure
     */
    static IoNMIConfiguration *getIoNMIConfiguration(IoApicInformation *ioapic);

    /**
     * @brief Get information about an ISA IRQ override.
     *
     * @param source The overridden (PIC) interrupt source
     * @return An IoInterruptOverride structure or nullptr if the GSI wasn't overridden
     */
    static IoInterruptOverride *getInterruptOverride(InterruptSource source);

    /**
     * @brief Get information about an ISA IRQ override.
     *
     * @param target The The overridden interrupt target
     * @return An IoInterruptOverride structure or nullptr if the GSI wasn't overridden
     */
    static IoInterruptOverride *getInterruptOverride(GlobalSystemInterrupt target);

    /**
     * @brief Get the GSI that is mapped to an InterruptSource.
     */
    static GlobalSystemInterrupt getInterruptOverrideTarget(InterruptSource source);

    /**
     * @brief Get the InterruptSource a GSI is mapped to.
     */
    static InterruptSource getInterruptOverrideSource(GlobalSystemInterrupt target);

private:
    static bool initialized; ///< @brief Indicates if initial system information was read

    static LPlatformInformation *localPlatform;
    static IoPlatformInformation *ioPlatform;

    static Kernel::Logger log;
};

template<typename Backend>
void Apic::initialize() {
    localPlatform = Backend::parseLPlatformInformation();
    ioPlatform = Backend::parseIoPlatformInformation();

    if (ioPlatform != nullptr && ioPlatform->ioapics.size() > 1) {
        log.warn("Support for multiple IO APICs is untested!");
    }

    // Initialized in this context only means that the system information was parsed,
    // there are values that will only be set when initializing local APIC and IO APIC.
    initialized = true;
}

}

#endif //HHUOS_APIC_H
