#ifndef HHUOS_APIC_H
#define HHUOS_APIC_H

#include "LocalApic.h"
#include "IoApic.h"
#include "ApicAcpiParser.h"
#include "kernel/log/Logger.h"
#include "ApicTimer.h"
#include "ApicErrorInterruptHandler.h"

namespace Device {

/**
 * @brief This class implements a way to interact with the APIC interrupt model.
 *
 * The InterruptModel is only concerned about hardware interrupts, software interrupts are independent.
 * This class handles initialization of both the LocalApic and IoApic class (it calls the supplied information backend,
 * for example the ApicAcpiParser, to read the information from the system that is required by those classes).
 *
 * For a technically correct implementation of the APIC interrupt model using ACPI, an AML interpreter is needed
 * to communicate the usage of the IO APIC to ACPI.
 * This would also be needed for hot-plugging support.
 */
class Apic {
public:
    /**
     * @brief Check if the system supports the APIC interrupt model.
     */
    static bool isSupported();

    /**
     * @brief Ensure that the system supports the APIC interrupt model.
     */
    static void ensureSupported();

    /**
     * @brief Initialize both LocalApic and IoApic devices.
     *
     * @tparam Parser The parser to use
     */
    template<typename Parser>
    static void initialize();

    /**
     * @brief Check if both LocalApic and IoApic devices are initialized.
     */
    static bool isInitialized();

    /**
     * @brief Ensure that both LocalApic and IoApic devices are initialized.
     */
    static void ensureInitialized();

    static void initializeTimer();

    static bool isTimerInitialized();

#if HHUOS_APIC_ENABLE_DEBUG == 1
    static void dumpDebugInfo();
#endif

    /**
     * @brief Check if an interrupt vector belongs to a local interrupt (Local APIC).
     */
    static bool isLocalInterrupt(InterruptVector vector);

    static void sendLocalEndOfInterrupt();

    /**
     * @brief Check if an interrupt vector belongs to an external hardware interrupt (I/O APIC).
     *
     * Depends on the number of external interrupts (GSIs) the system supports.
     */
    static bool isExternalInterrupt(InterruptVector vector);

    static void allowExternalInterrupt(InterruptSource interruptSource);

    static void forbidExternalInterrupt(InterruptSource interruptSource);

    static bool externalInterruptStatus(InterruptSource interruptSource);

    static void sendExternalEndOfInterrupt(InterruptVector vector);

private:
    /**
     * @brief Get the IoApic instance that is responsible for handling a specific GSI.
     */
    static IoApic &getIoApic(GlobalSystemInterrupt gsi);

private:
    // Local APICs are not accessed through instances because they have the same memory address.
    // Also, the local APIC register access always works with the registers of the currently running processor.
    // Io Apics are accessed through instances because they have different memory addresses and the same
    // processor has to work with multiple Io Apics.
    static Util::Data::ArrayList<IoApic *> ioApics;
    static ApicTimer *apicTimer;
    static ApicErrorInterruptHandler *errorHandler;

    static Kernel::Logger log;
};

template<typename Parser>
void Apic::initialize() {
    ensureSupported();
    if (isInitialized()) {
        log.warn("The APIC interrupt model has already been initialized, skipping!");
        return;
    }

    auto* localPlatform = new LocalApicPlatform;
    auto* ioPlatform = new IoApicPlatform;
    Util::Data::ArrayList<IoApicInformation *> ioInfos;

    // Get system information from the supplied parser
    Parser::parseLocalPlatformInformation(localPlatform);
    Parser::parseIoPlatformInformation(ioPlatform);
    Parser::parseIoApicInformation(&ioInfos);

    // Initialize the local APIC of the BSP
    LocalApic::initialize(localPlatform);

    // Initialize all I/O APICs
    for (auto* ioInfo : ioInfos) {
        auto* ioApic = new IoApic;
        ioApic->initialize(ioPlatform, ioInfo);
        ioApics.add(ioApic);
    }

    // Multiple I/O APICs are theoretically possible, but in the usual Intel consumer processors this is not utilized
    // TODO: Mention what is used in the Intel consumer chipsets (ICH2, ICH5)
    if (ioInfos.size() > 1) {
        log.warn("Support for multiple IO APICs is untested!");
    }

    // Local APIC error interrupt handler
    errorHandler = new ApicErrorInterruptHandler();
    errorHandler->plugin();
}

}

#endif //HHUOS_APIC_H
