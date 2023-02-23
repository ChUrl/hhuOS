#ifndef HHUOS_APIC_H
#define HHUOS_APIC_H

#include "device/cpu/Cpu.h"
#include "device/interrupt/apic/IoApic.h"
#include "device/interrupt/apic/LocalApic.h"
#include "device/interrupt/apic/LocalApicError.h"
#include "device/interrupt/apic/LocalApicRegisters.h"
#include "device/power/acpi/Acpi.h"
#include "device/time/ApicTimer.h"
#include "kernel/log/Logger.h"
#include "lib/util/async/Atomic.h"

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
     * @brief Get the total count of usable CPUs.
     */
    static uint8_t getCpuCount();

    /**
     * @brief Get the LocalApic instance that belongs to the current CPU.
     */
    static LocalApic &getCurrentLocalApic();

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

    /**
     * @brief Mount info nodes to /device/apic/.
     */
    static void mountVirtualFilesystemNodes();

    static void countInterrupt(Kernel::InterruptVector vector);

private:
    /**
     * @brief Throws if APIC not enabled.
     */
    static void ensureApic();

    /**
     * @brief Read information from ACPI's MADT and create an instance for each local APIC found.
     */
    static void populateLocalApics();

    /**
     * @brief Read information from ACPI's MADT and create an instance for each I/O APIC found.
     */
    static void populateIoApic();

    static void prepareInterruptCounters();

    /**
     * @brief Prepare the memory regions used by the AP's stacks.
     *
     * @return The virtual address of the stackpointer array
     */
    static void *prepareApStacks();

    /**
     * @brief Copy the AP startup routine to lower physical memory.
     *
     * Because this memory is identity-mapped, the physical address can be used to free the memory again.
     *
     * @return The virtual/physical address at which the startup routine is located
     */
    static void *prepareApStartupCode(void *apGdts, void *apStacks);

    /**
     * @brief Place the AP startup routine address into the warm reset vector and prepare CMOS for warm reset.
     *
     * @return The virtual address of the allocated memory used to write the warm-reset vector
     */
    static void *prepareApWarmReset();

    static void *prepareApGdts();

    /**
     * @brief Sets up the GDT for the AP.
     *
     * The memory is allocated by MemoryService::AllocateKernelMemory with enabled paging, so its a virtual address.
     * This is basically a shorter and slightly modified version of System::InitializeGlobalDescriptorTables.
     * The main difference is that only a single GDT is used and its memory is allocated by this function.
     */
    static Cpu::Descriptor *allocateApGdt();

    static Kernel::GlobalSystemInterrupt mapInterruptRequest(InterruptRequest interruptRequest);

    static void printLocalApics(Util::String &string);

    static void printLvt(Util::String &string);

    static void printIoApic(Util::String &string);

    static void printRedtbl(Util::String &string);

    static void printInterrupts(Util::String &string);

private:
    static bool apicEnabled;          ///< @brief Indicates if Apic::enable() has been called.
    static uint32_t usableProcessors; ///< @brief The amount of CPUs usable by the system.
    static bool smpEnabled;           ///< @brief Indicates if Apic::startupSmp() has been called.

    // Memory allocated for or by instances contained in these lists is never freed,
    // this implementation doesn't support disabling the APIC at all.
    // Once the switch from PIC to APIC is done, it can't be switched back.
    static Util::Array<LocalApic *> *localApics;  ///< @brief All LocalApic instances.
    static Util::Array<ApicTimer *> *localTimers; ///< @brief All ApicTimer instances.
    static IoApic *ioApic;                        ///< @brief The IoApic instance responsible for the external interrupts.
    static LocalApicError *errorHandler;          ///< @brief The interrupt handler that gets triggered on an internal APIC error.

    // The PIT counter will overflow after a while...
    static Util::Array<uint32_t> *counters;
    static Util::Array<Util::Async::Atomic<uint32_t> *> *wrappers;

    static Kernel::Logger log;
};

} // namespace Device

#endif //HHUOS_APIC_H
