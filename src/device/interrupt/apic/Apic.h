#ifndef HHUOS_APIC_H
#define HHUOS_APIC_H

#include "ApicAcpiInterface.h"
#include "ApicRegisters.h"
#include "kernel/log/Logger.h"
#include "LocalApic.h"
#include "IoApic.h"
#include "ApicErrorHandler.h"
#include "device/time/ApicTimer.h"

namespace Device {

/**
 * @brief This class provides a common interface to the APIC interrupt model.
 */
class Apic {
public:
    Apic() = delete; // Static class

    Apic(const Apic &move) = delete;

    Apic &operator=(const Apic &move) = delete;

    ~Apic() = delete; // Static class

    /**
     * @brief Check if the system supports the APIC interrupt architecture.
     */
    static bool isSupported();

    /**
     * @brief Check if both LocalApic and IoApic devices are initialized.
     */
    static bool isInitialized();

    /**
     * @brief Initialize the BSP's local APIC and all I/O APICs.
     */
    static void initialize();

#if HHUOS_APIC_ENABLE_SMP == 1
    /**
     * @brief Check if the system supports symmetric multiprocessing mode with multiple processors.
     */
    static bool isSmpSupported();

    /**
     * @brief Initialize the APs when SMP is supported.
     */
    static void initializeSmp();
#endif

    /**
     * @brief Get the LocalApic instance that belongs to the current CPU.
     */
     static LocalApic &getCurrentLocalApic();

    /**
     * @brief Check if the BSP's local APIC timer has been initialized.
     */
    static bool isBspTimerInitialized();

    /**
     * @brief Initialize the current processor's local APIC timer.
     */
    static void initializeTimer();

    /**
     * @brief Get the ApicTimer instance that belongs to the current CPU.
     */
    static ApicTimer &getCurrentTimer();

    /**
     * @brief Create an ErrorHandler instance (if it hasn't been created yet) and allow the local ERROR interrupt.
     */
    static void initializeErrorHandling();

    /**
     * @brief Unmask an external interrupt.
     */
    static void allow(InterruptRequest interruptRequest);

    /**
     * @brief Mask an external interrupt.
     */
    static void forbid(InterruptRequest interruptRequest);

    /**
     * @brief Check if an external interrupt is masked or unmasked.
     *
     * @return True, if the interrupt is masked.
     */
    static bool status(InterruptRequest interruptRequest);

    /**
     * @brief Signal the completion of an interrupt, local or external.
     */
    static void sendEndOfInterrupt(Kernel::InterruptVector vector);

    /**
     * @brief Check if an interrupt vector belongs to a local interrupt (Local APIC).
     */
    static bool isLocalInterrupt(Kernel::InterruptVector vector);

    /**
     * @brief Check if an interrupt vector belongs to an external hardware interrupt (I/O APIC).
     */
    static bool isExternalInterrupt(Kernel::InterruptVector vector);

private:
#if HHUOS_APIC_ENABLE_SMP == 1
    /**
     * @brief Prepare the memory regions used by the AP's stacks.
     */
    static void allocateSmpStacks();

    /**
     * @brief Copy the AP startup routine to lower physical memory.
     *
     * @return The page, on which the startup routine is located
     */
    static void copySmpStartupCode();
#endif

    /**
     * @brief Get the IoApic instance that is responsible for handling a specific GSI.
     */
    static IoApic &getIoApic(Kernel::GlobalSystemInterrupt gsi);

#if HHUOS_APIC_ENABLE_DEBUG == 1
    static void dumpDebugInfo();
#endif

private:
    static bool initialized; ///< @brief Indicates if Apic::initialize() has been called.
    static bool timerInitialized; ///< @brief Indicates if Apic::initializeTimer() has been called at least once.
    static Util::ArrayList<LocalApic *> localApics; ///< @brief All LocalApic instances.
    static Util::ArrayList<IoApic *> ioApics; ///< @brief All IoApic instance..
    static Util::ArrayList<ApicTimer *> timers; ///< @brief All ApicTimer instances.
    static ApicErrorHandler *errorHandler; ///< @brief The interrupt handler that gets triggered on an internal APIC error.

    static Kernel::Logger log;
};

}

#endif //HHUOS_APIC_H
