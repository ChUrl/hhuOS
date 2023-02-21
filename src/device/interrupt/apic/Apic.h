#ifndef HHUOS_APIC_H
#define HHUOS_APIC_H

#include "ApicErrorHandler.h"
#include "ApicRegisters.h"
#include "device/time/ApicTimer.h"
#include "IoApic.h"
#include "kernel/log/Logger.h"
#include "LocalApic.h"

namespace Device {

/**
 * @brief This class provides a common interface to the APIC interrupt model.
 *
 * It also contains the SMP init code, which is oddly placed, but I didn't move it because I
 * didn't want to spent more time on it, it's just a proof of concept after all.
 */
class Apic {
public:
    Apic() = delete; // Static class

    Apic(const Apic &move) = delete;

    Apic &operator=(const Apic &move) = delete;

    ~Apic() = delete; // Static class

    // Initialization stuff

    /**
     * @brief Check if the system supports the APIC interrupt architecture.
     */
    static bool isSupported();

    /**
     * @brief Check if both LocalApic and IoApic devices are initialized.
     */
    static bool isEnabled();

    /**
     * @brief Initialize the BSP's local APIC and all I/O APICs.
     *
     * Includes APIC timer and APIC error handler.
     */
    static void enable();

    /**
     * @brief Mount info nodes to /device/apic/.
     *
     * Those do not get updated if something changes during runtime.
     */
    static void mountDeviceNodes();

    /**
     * @brief Check if the system supports symmetric multiprocessing mode with multiple processors.
     */
    static bool isSmpSupported();

    /**
     * @brief Initialize the APs when SMP is supported.
     *
     * Implements the "Universal Startup Algorithm" from the Intel MultiProcessor Specification.
     */
    static void startupSmp();

    /**
     * @brief Initialize the local APIC of the current CPU, called by the APs.
     */
    static void initializeCurrentLocalApic();

    /**
     * @brief Get the total CPU count.
     */
    static uint8_t getCpuCount();

    /**
     * @brief Check if this core's local APIC timer has been initialized.
     */
    static bool isCurrentTimerRunning();

    /**
     * @brief Initialize the current processor's local APIC timer.
     */
    static void startCurrentTimer();

    /**
     * @brief Get the ApicTimer instance that belongs to the current CPU.
     */
    static ApicTimer &getCurrentTimer();

    /**
     * @brief Create an ErrorHandler instance (if it hasn't been created yet) and allow the local ERROR interrupt.
     */
    static void enableCurrentErrorHandler();

    // Interrupt controller stuff

    /**
     * @brief Unmask an external interrupt for the current CPU.
     */
    static void allow(InterruptRequest interruptRequest);

    /**
     * @brief Mask an external interrupt for the current CPU.
     */
    static void forbid(InterruptRequest interruptRequest);

    /**
     * @brief Check if an external interrupt is masked or unmasked for the current CPU.
     *
     * @return True, if the interrupt is masked.
     */
    static bool status(InterruptRequest interruptRequest);

    /**
     * @brief Signal the completion of an interrupt to the current CPU, local or external.
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
    /**
     * @brief Read information from ACPI's MADT and create an instance for each local APIC found.
     */
    static void populateLocalApics();

    /**
     * @brief Read information from ACPI's MADT and create an instance for each I/O APIC found.
     */
    static void populateIoApics();

    /**
     * @brief Prepare the memory regions used by the AP's stacks.
     */
    static void *prepareApStacks();

    /**
     * @brief Copy the AP startup routine to lower physical memory.
     *
     * @return The page, on which the startup routine is located
     */
    static void *prepareApStartupCode(void *apStacks);

    /**
     * @brief Place the AP startup routine address into the warm reset vector and prepare CMOS for warm reset.
     */
    static void *prepareApWarmReset();

    /**
     * @brief Get the LocalApic instance that belongs to the current CPU.
     */
    static LocalApic &getCurrentLocalApic();

    /**
     * @brief Get the IoApic instance that is responsible for handling a specific GSI.
     */
    static IoApic &getIoApic(Kernel::GlobalSystemInterrupt gsi);

    static void printLocalApics(Util::String &string);

    static void printIoApics(Util::String &string);

private:
    static bool apicEnabled;  ///< @brief Indicates if Apic::enable() has been called.
    static bool smpEnabled;   ///< @brief Indicates if Apic::startupSmp() has been called.

    // Memory allocated for or by instances contained in these lists is never freed,
    // this implementation doesn't support disabling the APIC at all.
    // Once the switch from PIC to APIC is done, it can't be switched back.
    static Util::ArrayList<LocalApic *> localApics; ///< @brief All LocalApic instances.
    static Util::ArrayList<IoApic *> ioApics;       ///< @brief All IoApic instances.
    static Util::ArrayList<ApicTimer *> timers;     ///< @brief All ApicTimer instances.
    static ApicErrorHandler errorHandler;           ///< @brief The interrupt handler that gets triggered on an internal APIC error.

    static Kernel::Logger log;
};

} // namespace Device

#endif //HHUOS_APIC_H
