#ifndef __LAPIC_include__
#define __LAPIC_include__

#include "ApicAcpiInterface.h"
#include "ApicRegisters.h"
#include "device/cpu/IoPort.h"
#include "device/cpu/ModelSpecificRegister.h"
#include "kernel/log/Logger.h"

namespace Device {

/**
 * @brief This class implements the local APIC hardware interrupt controller.
 *
 * The local APIC is an internal component of every individual CPU core.
 * It handles "local interrupts" directly connected to one of the local APIC's local interrupt inputs
 * and interrupts signalled over the system bus (IPIs and I/O APIC interrupts).
 * Using this class means interacting with the local APIC of the current CPU core.
 */
class LocalApic {
    friend class Apic;             // Apic exposes this class' functionality to the OS
    friend class ApicTimer;        // ApicTimer is configured by using LApic registers
    friend class ApicErrorHandler; // ApicErrorInterruptHandler uses the ERR register
    friend class IoApic;

public:
    /**
     * @brief Lists the local APIC's local interrupts.
     *
     * Every individual local APIC has these, they are completely separate from
     * the usual (PIC and I/O APIC) hardware interrupt inputs.
     */
    enum LocalInterrupt : uint8_t {
        CMCI = 0,  // Might not exist
        TIMER = 1, ///< @brief The APIC timer local interrupt
        THERMAL = 2,
        PERFORMANCE = 3,
        LINT0 = 4, ///< @brief Local interrupt 0, used in virtual wire mode
        LINT1 = 5, ///< @brief Local interrupt 1, used as NMI source
        ERROR = 6  ///< @brief The APIC error interrupt
    };

public:
    /**
     * @brief Constructs a LocalApic instance.
     *
     * @param localApicPlatform General information about the local APICs, parsed from ACPI
     * @param localApicInformation Information about the specific local APIC, parsed from ACPI
     */
    LocalApic(uint8_t cpuId, uint32_t baseAddress,
              LocalInterrupt nmiLint, LVTEntry::PinPolarity nmiPolarity, LVTEntry::TriggerMode nmiTrigger);

    LocalApic(const LocalApic &copy) = delete;

    LocalApic &operator=(const LocalApic &copy) = delete;

    ~LocalApic() = default;

    /**
     * @brief Initialize LVT, SVR and TPR of the executing core's local APIC.
     *
     * The local APIC initialization consists of multiple steps:
     * 1. The BSP calls LocalApic::enableXApicMode(), to set up the system for local APIC initialization.
     * 2. The BSP calls LocalApic::initialize(), to complete the BSP's local APIC initialization.
     * 3. The APs are booted up.
     * 4. Every AP calls LocalApic::initialize() individually.
     *
     * This function must not be called before LocalApic::enableXApicMode().
     */
    void initialize() const;

    static void dumpLVT();

private:
    /**
     * @brief Lists the offsets, relative to the APIC base address, for MMIO register access.
     *
     * Described in the IA-32 manual, sec. 3.11.4.1
     */
    enum Register : uint16_t {
        ID = 0x20,        ///< @brief Local APIC id, in SMP systems the id is used as the CPU id
        VER = 0x30,       ///< @brief Local APIC version
        TPR = 0x80,       ///< @brief Task Priority Register
        APR = 0x90,       ///< @brief Arbitration Priority Register
        PPR = 0xA0,       ///< @brief Processor Priority Register
        EOI = 0xB0,       ///< @brief End-of-Interrupt Register
        RRD = 0xC0,       ///< @brief Remote Read Register
        LDR = 0xD0,       ///< @brief Logical Destination Register
        DFR = 0xE0,       ///< @brief Destination Format Register
        SVR = 0xF0,       ///< @brief Spurious Interrupt Vector Register
        ISR = 0x100,      ///< @brief In-Service Register (255 bit)
        TMR = 0x180,      ///< @brief Trigger Mode Register (255 bit)
        IRR = 0x200,      ///< @brief Interrupt Request Register (255 bit)
        ESR = 0x280,      ///< @brief Error Status Register
        ICR_LOW = 0x300,  ///< @brief Interrupt Command Register (lower 32 bit)
        ICR_HIGH = 0x310, ///< @brief Interrupt Command Register (upper 32 bit)

        // These are located here, instead of in the ApicTimer class, because this class does the register access
        TIMER_INITIAL = 0x380, ///< @brief Timer Initial Count Register
        TIMER_CURRENT = 0x390, ///< @brief Timer Current Count Register
        TIMER_DIVIDE = 0x3E0   ///< @brief Timer Divide Configuration Register
    };

private:
    /**
     * @brief Check if the local APIC supports xApic mode (xApic uses MMIO-based register access)
     *
     * Determined using CPUID.
     */
    static bool supportsXApic();

    /**
     * @brief Check if the local APIC supports x2Apic mode (x2Apic uses MSR-based register access)
     *
     * Determined using CPUID.
     */
    static bool supportsX2Apic();

    /**
     * @brief Get the id of the local APIC belonging to the current CPU.
     *
     * Can be used to determine what CPU is currently executing the calling code in SMP systems.
     * To get the id of a LocalApic instance, use the "cpuId" field.
     */
    [[nodiscard]] static uint8_t getId();

    /**
     * @brief Determine the local APIC version
     */
    static uint8_t getVersion();

    /**
     * @brief Prepare the BSP for local APIC initialization.
     *
     * Only has to be called once, not once per AP.
     */
    static void enableXApicMode();

    /**
     * @brief Set the IMCR to disconnect the PIC from the CPU.
     *
     * The IMCR is only available on some hardware, not in QEMU.
     */
    static void disablePicMode();

    /**
     * @brief Send an INIT IPI to an AP.
     *
     * The INIT IPI prepares an unitialized AP for startup.
     *
     * @param id The local APIC id/CPU id of the AP to initialize
     * @param level Assert or deassert
     */
    static void sendIpiInit(uint8_t id, ICREntry::Level level);

    /**
     * @brief Send an STARTUP IPI (SIPI) to an AP.
     *
     * The STARTUP IPI instructs an AP in INIT state to load its startup routine from a supplied address and
     * execute it, booting the AP.
     *
     * @param id The local APIC id/CPU id of the AP to boot
     * @param startupCodeAddress The page on which the startup routine is located in physical memory
     */
    static void sendIpiStartup(uint8_t id, uint32_t startupCodeAddress);

    /**
     * @brief Clear the local APIC error register of the current CPU.
     */
    static void clearErrors();

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

    /**
     * @brief Send an end-of-interrupt signal to the local APIC of the current CPU.
     *
     * The signal will be broadcasted to all I/O APICs if the interrupt was level-triggered and broadcasting
     * is enabled in the SVR (IA-32 manual, sec. 3.11.8.5), as for level-triggered interrupts servicing
     * completion has to be signaled to both, the local APIC and the I/O APIC(s).
     * Edge-triggered interrupts are only EOI'd to the local APIC.
     */
    static void sendEndOfInterrupt();

    /**
     * @brief Initialize the local APIC's local vector table.
     *
     * Marks every local interrupt in the local vector table as edge-triggered,
     * active high, masked and fixed delivery mode.
     * Vector numbers are set to InterruptVector equivalents.
     */
    static void initializeLVT();

    // Reading and writing local APIC's registers parses the read/written values to/from
    // types from ApicRegisterInterface. Only registers of the current CPU will be affected.

    /**
     * @brief Read the IA32_APIC_BASE_MSR.
     *
     * IA-32 manual, sec. 3.11.12.1 and 4.1
     */
    [[nodiscard]] static BaseMSREntry readBaseMSR();

    /**
     * @brief Write the IA32_APIC_BASE_MSR.
     *
     * IA-32 manual, sec. 3.11.12.1 and 4.1
     */
    static void writeBaseMSR(const BaseMSREntry &msrEntry);

    /**
     * @brief Read a 32-bit register identified by a memory offset relative to the APIC base address.
     */
    [[nodiscard]] static uint32_t readDoubleWord(Register reg);

    /**
     * @brief Write a 32-bit register identified by a memory offset relative to the APIC base address.
     */
    static void writeDoubleWord(Register reg, uint32_t val);

    /**
     * @brief Read the spurious interrupt vector register.
     *
     * IA-32 manual, sec. 3.11.9
     */
    [[nodiscard]] static SVREntry readSVR();

    /**
     * @brief Write the spurious interrupt vector register.
     *
     * IA-32 manual, sec. 3.11.9
     */
    static void writeSVR(const SVREntry &svrEntry);

    /**
     * @brief Read a local vector table register, identified by the local interrupt.
     *
     * IA-32 manual, sec. 3.11.5.1
     */
    [[nodiscard]] static LVTEntry readLVT(LocalInterrupt lint);

    /**
     * @brief Write a local vector table register, identified by the local interrupt.
     *
     * IA-32 manual, sec. 3.11.5.1
     */
    static void writeLVT(LocalInterrupt lint, const LVTEntry &lvtEntry);

    /**
     * @brief Read the interrupt command register.
     *
     * IA-32 manual, sec. 3.11.6.1
     */
    [[nodiscard]] static ICREntry readICR(); // Obtain delivery status of IPI

    /**
     * @brief Write the interrupt command register.
     *
     * IA-32 manual, sec. 3.11.6.1
     */
    static void writeICR(const ICREntry &icrEntry); // Issue IPIs

private:
    uint8_t cpuId;               ///< @brief The CPU core this instance belongs to, LocalApic::getId() only returns the current AP's id!
    static uint32_t baseAddress; ///< @brief The physical address where the local APIC MMIO region is located.
    static uint32_t mmioAddress; ///< @brief The virtual address used to access registers in xApic mode.

    LocalInterrupt nmiLint;            ///< @brief The local interrupt pin that acts as NMI source.
    LVTEntry::PinPolarity nmiPolarity; ///< @brief The NMI source's pin polarity.
    LVTEntry::TriggerMode nmiTrigger;  ///< @brief The NMI source's trigger mode.

    static const ModelSpecificRegister ia32ApicBaseMsr; ///< @brief Core unique MSR (every core can only address its own MSR).
    static const Util::Array<Register> lintRegs;        ///< @brief Local interrupt to register offset lookup.

    static Kernel::Logger log;
};

} // namespace Device

#endif
