#ifndef __LAPIC_include__
#define __LAPIC_include__

#include "ApicRegisterInterface.h"
#include "ApicAcpiInterface.h"
#include "ModelSpecificRegister.h"
#include "kernel/log/Logger.h"

namespace Device {

// TODO: The LApic doesn't support individual relocation for SMP systems

/**
 * @brief This class implements the local APIC hardware interrupt controller.
 *
 * The local APIC is an internal component of every individual CPU core.
 * It handles "local interrupts" directly connected to one of the local APIC's local interrupt inputs
 * and interrupts signalled over the system or APIC bus (IPIs and IO APIC interrupts).
 * Using this class mostly means interacting with the local APIC of the current CPU core.
 */
class LocalApic {
    friend class Apic;
    friend class ApicTimer; // ApicTimer is configured by using LApic registers

public:
    // TODO: Move this and InterruptSource and GlobalSystemInterrupt to same file?
    /**
     * @brief This lists the local APIC's local interrupt pins.
     *
     * Every individual local APIC has these pins, they are completely separate from
     * the usual (PIC/IO APIC) hardware interrupt inputs/GlobalSystemInterrupts.
     */
    enum LocalInterrupt : uint8_t {
        CMCI = 0, // TODO: Might not exist
        TIMER = 1,
        THERMAL = 2,
        PERFORMANCE = 3,
        LINT0 = 4,
        LINT1 = 5,
        ERROR = 6
    };

public:
    LocalApic() = delete; // Static class

    LocalApic(const LocalApic &copy) = delete;

    LocalApic &operator=(const LocalApic &copy) = delete;

    ~LocalApic() = delete; // Static class

    // TODO: Make everything private, use Apic class

    static bool isInitialized();

    static bool isSmpInitialized();

    /**
     * @brief Ensure that all local APICs are initialized.
     */
    static void ensureInitialized();

    /**
     * @brief Initialize the local APIC.
     *
     * All local APIC interrupts will be masked and EOI broadcasting disabled.
     * Note that the APIC system interrupt model initialization is only completed
     * after both local APICs and IO APICs have been initialized!
     *
     * Must be called by the bootstrap processor.
     */
    static void initialize(LocalApicPlatform *platform);

    /**
     * @brief Get the ID of the local APIC belonging to the current CPU.
     *
     * Can be used to determine what CPU is currently running.
     *
     * @return The local APIC's ID
     */
    [[nodiscard]] static uint8_t getId();

    /**
     * @brief Unmask a local interrupt in the local APIC of the current CPU.
     *
     * @param lint The local interrupt to activate
     */
    static void allow(LocalInterrupt lint);

    /**
     * @brief Forbid a local interrupt in the local APIC of the current CPU.
     *
     * @param lint The local interrupt to deactivate
     */
    static void forbid(LocalInterrupt lint);

    /**
     * @brief Get the state of this interrupt - whether it is masked out or not.
     *
     * @param lint The local interrupt
     * @return True, if the interrupt is disabled in the local APIC of the current CPU
     */
    static bool status(LocalInterrupt lint);

    // TODO: For compatibility with older IO APICs (< version 0x20) this has to be used in
    //       combination with temporarily setting all IO APIC redirection entries to level-triggered
    //       (https://github.com/torvalds/linux/blob/master/arch/x86/kernel/apic/io_apic.c#L470)
    /**
     * @brief Send an end of interrupt signal to the local APIC of the current CPU.
     *
     * The signal will be broadcasted to IO APICs if the interrupt was level-triggered and broadcasting
     * is enabled in the SVR (IA-32 Architecture Manual Chapter 10.8.5).
     *
     * For IO APICs with version >= 0x20 prefer sending the EOI to the IO APIC.
     */
    static void sendEndOfInterrupt();

    static void handleErrors();

private:
    // Offsets, IA-32 Architecture Manual Chapter 10.4.1
    // NOTE: Omitted entries already in LApic::Interrupt and ApicTimer
    enum Register : uint16_t {
        ID = 0x20, // ID
        VER = 0x30, // Version
        TPR = 0x80, // Task Priority Register
        APR = 0x90, // Arbitration Priority Register
        PPR = 0xA0, // Processor Priority Register
        EOI = 0xB0, // End of Interrupt Register
        RRD = 0xC0, // Remote Read Register
        LDR = 0xD0, // Logical Destination Register
        DFR = 0xE0, // Destination Format Register
        SVR = 0xF0, // Spurious Interrupt Vector Register
        ISR = 0x100, // In-Service Register (255 bit)
        TMR = 0x180, // Trigger Mode Register (255 bit)
        IRR = 0x200, // Interrupt Request Register (255 bit)
        ESR = 0x280, // Error Status Register
        ICR_LOW = 0x300, // Interrupt Command Register (64 bit)
        ICR_HIGH = 0x310,

        // These are located here, instead of in the ApicTimer class, because this class does the register access
        TIMER_INITIAL = 0x380, // Timer Initial Count Register
        TIMER_CURRENT = 0x390, // Timer Current Count Register
        TIMER_DIVIDE = 0x3E0 // Timer Divide Configuration Register
    };

private:
    /**
     * @brief Ensure that the local APIC's MMIO region has been initialized.
     */
    static void ensureMMIO();

    // TODO
    /**
     * @brief Initialize the local APIC of an additional AP (Application Processor).
     *
     * (MultiProcessor Specification Appendix B.4)
     */
    // static void initializeApplicationProcessor(LocalApicInformation *lapic);

    /**
     * @brief Allocate the memory region used to access the local APIC's registers.
     */
    static void initializeMMIORegion();

    /**
     * @brief Initializes the local APIC's local vector table.
     *
     * Marks every local interrupt in the local vector table as edge-triggered,
     * active high, masked and fixed delivery mode.
     * Vector numbers are set to InterruptDispatcher equivalent vectors.
     */
    static void initializeLVT();

    // NOTE: Reading and writing local APIC's registers
    // NOTE: Parses the read/written value to/from types from ApicRegisterInterface.h
    // NOTE: Only registers of currently running CPU will be affected
    // NOTE: The LApicInformation has to be passed anyway since every LApic could have a different MMIO address

    [[nodiscard]] static MSREntry readBaseMSR();

    static void writeBaseMSR(const MSREntry &msrEntry);

    [[nodiscard]] static uint32_t readDoubleWord(Register reg);

    static void writeDoubleWord(Register reg, uint32_t val);

    [[nodiscard]] static SVREntry readSVR();

    static void writeSVR(const SVREntry &svrEntry);

    [[nodiscard]] static LVTEntry readLVT(LocalInterrupt lint);

    static void writeLVT(LocalInterrupt lint, const LVTEntry &lvtEntry);

    [[nodiscard]] static ICREntry readICR(); // Obtain delivery status of IPI

    static void writeICR(const ICREntry &icrEntry); // Issue IPIs

private:
    static bool initialized;
    static bool smpInitialized;
    static LocalApicPlatform *localPlatform; ///< @brief General information valid for all local APICs

    static Device::ModelSpecificRegister ia32ApicBaseMsr; // Core unique MSR (unique although static)
    static Register lintRegs[7]; // Register offsets for the LINTs

    static Kernel::Logger log; // TODO: Remove?
};

}

#endif