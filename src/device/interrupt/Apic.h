#ifndef HHUOS_APIC_H
#define HHUOS_APIC_H

#include "LocalApic.h"
#include "IoApic.h"
#include "ApicAcpiParser.h"
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
public:
    /**
     * @brief Initialize the InterruptModel.
     *
     * @tparam Parser The parser to use
     */
    template<typename Parser>
    static void initialize();

    static void initializeSMP();

    /**
     * @brief Check if the system is running with the APIC interrupt model.
     */
    static bool isInitialized();

    static bool isSMPInitialized();

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
    static bool isSupported();

    /**
     * @brief Check if an interrupt vector belongs to a local interrupt (local APIC).
     */
    static bool isLocalInterrupt(InterruptVector vector);

    static void allowLocalInterrupt(LocalApic::LocalInterrupt localInterrupt);

    static void forbidLocalInterrupt(LocalApic::LocalInterrupt localInterrupt);

    static bool localInterruptStatus(LocalApic::LocalInterrupt localInterrupt);

    static void sendLocalEndOfInterrupt();

    /**
     * @brief Check if an interrupt vector belongs to an external hardware interrupt (PIC/IO APIC).
     *
     * Depends on the number of external interrupts (GSIs) the system supports.
     */
    static bool isExternalInterrupt(InterruptVector vector);

    static void allowExternalInterrupt(InterruptSource interruptSource);

    static void forbidExternalInterrupt(InterruptSource interruptSource);

    static bool externalInterruptStatus(InterruptSource interruptSource);

    static void sendExternalEndOfInterrupt(InterruptVector vector);

private:
    static IoApic *getIoApic(GlobalSystemInterrupt gsi);

private:
    // Local Apics are not accessed through instances because they have the same memory address.
    // Also, the register access always works with the registers of the currently running processor.
    // Io Apics are accessed through instances because they have different memory addresses and the same
    // processor has to work with multiple Io Apics.
    static Util::Data::ArrayList<IoApic *> ioApics;

    static Kernel::Logger log;
};

template<typename Parser>
void Apic::initialize() {
    // TODO: Ensure not initialized
    ensureApicSupported();

    // Initialize the local Apic of the BSP
    auto* localPlatform = new LocalApicPlatform;
    Parser::parseLocalPlatformInformation(localPlatform);
    LocalApic::initialize(localPlatform);

    // Initialize all IO Apics
    auto* ioPlatform = new IoApicPlatform;
    Parser::parseIoPlatformInformation(ioPlatform);

    Util::Data::ArrayList<IoApicInformation *> ioInfos;
    Parser::parseIoApicInformation(&ioInfos);

    if (ioInfos.size() > 1) {
        log.warn("Support for multiple IO APICs is untested!");
    }

    for (auto* ioInfo : ioInfos) {
        auto* ioApic = new IoApic;
        ioApic->initialize(ioPlatform, ioInfo);
        ioApics.add(ioApic);
    }
}

}

#endif //HHUOS_APIC_H
