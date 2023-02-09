#ifndef HHUOS_APIC_H
#define HHUOS_APIC_H

#include "ModelSpecificRegister.h"
#include "ApicAcpiInterface.h"
#include "ApicRegisterInterface.h"
#include "kernel/log/Logger.h"
#include "LocalApic.h"
#include "IoApic.h"
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
    static void initialize();

    /**
     * @brief Check if both LocalApic and IoApic devices are initialized.
     */
    static bool isBspInitialized();

    /**
     * @brief Ensure that both LocalApic and IoApic devices are initialized.
     */
    static void ensureBspInitialized();

    static bool isSmpSupported();

    static void ensureSmpSupported();

    static void initializeSmp();

    static void initializeTimer();

    static bool isTimerInitialized();

#if HHUOS_APIC_ENABLE_DEBUG == 1
    static void dumpDebugInfo();
#endif

    /**
     * @brief Check if an interrupt vector belongs to a local interrupt (Local APIC).
     */
    static bool isLocalInterrupt(Kernel::InterruptVector vector);

    static void sendLocalEndOfInterrupt();

    /**
     * @brief Check if an interrupt vector belongs to an external hardware interrupt (I/O APIC).
     *
     * Depends on the number of external interrupts (GSIs) the system supports.
     */
    static bool isExternalInterrupt(Kernel::InterruptVector vector);

    static void allowExternalInterrupt(InterruptRequest interruptRequest);

    static void forbidExternalInterrupt(InterruptRequest interruptRequest);

    static bool externalInterruptStatus(InterruptRequest interruptRequest);

    static void sendExternalEndOfInterrupt(Kernel::InterruptVector vector);

private:

    static void initializeSmpStartupStacks();

    static uint32_t initializeSmpStartupCode();

    /**
     * @brief Get the IoApic instance that is responsible for handling a specific GSI.
     */
    static IoApic &getIoApic(Kernel::GlobalSystemInterrupt gsi);

private:
    static Util::Data::ArrayList<IoApic *> ioApics;
    static Util::Data::ArrayList<LocalApic *> localApics;
    static ApicTimer *apicTimer;
    static ApicErrorInterruptHandler *errorHandler;

    static Kernel::Logger log;
};

}

#endif //HHUOS_APIC_H
