#ifndef __LAPIC_include__
#define __LAPIC_include__

#include <cstdint>
#include "device/cpu/IoPort.h"
#include "device/interrupt/Pic.h"
#include "device/interrupt/ModelSpecificRegister.h"
#include "kernel/log/Logger.h"
#include "kernel/interrupt/InterruptDispatcher.h"

#define HHUOS_LAPIC_ENABLE_DEBUG 1

namespace Device {

// TODO: Most of this is static, is this the preferred way?
class LApic {
    friend class ApicTimer; // ApicTimer is configured by using LApic registers

public:
    LApic() = delete;
    LApic(const LApic &copy) = delete;
    LApic &operator=(const LApic &other) = delete;
    ~LApic() = default;

    // NOTE: The values have nothing to do with physical pins, they are the register offsets for the LVT
    // NOTE: Register::LVT_<...> and Interrupt::<...> is interchangable
    enum Interrupt : uint16_t {
        CMCI = 0x2F0,
        TIMER = 0x320,
        THERMAL = 0x330,
        PERFORMANCE = 0x340,
        LINT0 = 0x350,
        LINT1 = 0x360,
        ERROR = 0x370
    };

    static bool isInitialized();

    /**
     * Check if local APIC support is present on the system (using CPUID).
     *
     * @return True if local APIC is supported
     */
    static bool hasApicSupport();

    /**
     * Check if x2APIC support is present on the system (using CPUID).
     *
     * @return True if x2APIC is supported
     */
    static bool hasX2ApicSupport();

    /**
     * Check if local APIC is running in x2Apic mode.
     *
     * @return True if APIC is running in x2Apic mode
     */
    [[nodiscard]] static bool isX2Apic();

    /**
     * Check if local APIC is HW and SW enabled.
     *
     * @return True if APIC is HW and SW enabled
     */
    [[nodiscard]] static bool isEnabled();

    /**
     * Get the ID of the local APIC belonging to the current CPU core.
     *
     * @return The local APIC's ID
     */
    [[nodiscard]] static uint8_t getId();

    /**
     * Get the local APIC version.
     * Should be identical over all CPU cores.
     *
     * @return The local APIC's version.
     */
    [[nodiscard]] static uint8_t getVersion();

    /**
     * Initialize the local APIC.
     * Only used for the bootstrap processor.
     *
     * Must not be called with enabled interrupts.
     */
    static void init();

    /**
     * Unmask an interrupt in the APIC of the current CPU. If this is done,
     * all interrupts with this number will be passed to the CPU.
     *
     * @param lint The register offset of the interrupt to activated
     */
    static void allow(Interrupt lint);

    /**
     * Forbid an interrupt in the APIC of the current CPU. If this is done,
     * the interrupt is masked out and every interrupt with this number
     * that is thrown will be suppressed and not arrive the CPU.
     *
     * @param lint The register offset of the interrupt to deactivate
     */
    static void forbid(Interrupt lint);

    /**
     * Get the state of this interrupt - whether it is masked out or not.
     *
     * @param lint The register offset of the interrupt
     * @return true, if the interrupt is disabled
     */
    static bool status(Interrupt lint);

    // TODO: For compatibility with older IO APICs (< version 0x20) this has to be used in
    //       combination with temporarily setting all IO APIC redirection entries to level-triggered
    //       (https://github.com/torvalds/linux/blob/master/arch/x86/kernel/apic/io_apic.c#L470)
    /**
     * Send an end of interrupt signal to the local APIC of the current CPU.
     * The signal will be broadcasted to IO APICs if the interrupt was
     * level-triggered and broadcasting is enabled in the SVR.
     * (IA-32 Architecture Manual Chapter 10.8.5)
     *
     * For IO APICs with version >= 0x20 prefer sending the EOI to the IO APIC.
     */
    static void sendEndOfInterrupt();

    // NOTE: Not supported on QEMU and newer hardware
    static void enablePicMode();

    // NOTE: QEMU does this by default and the IMCR doesn't exist, so the PIC can't be hardwired to the BSP
    // TODO: To determine if this has to be set MP/ACPI tables have to be parsed
    /**
     * Set the IMCR (Interrupt Mode Control Register) to connect the APIC to the BSP.
     * Set local APIC LINT0 to ExtINT for external (PIC) interrupt controller.
     * QEMU: This is the default mode, PIC mode is not supported.
     *
     * Must not be called with enabled interrupts.
     */
    static void enableVirtualWireMode();

    /**
     * Disables virtual wire mode and configures the local APIC
     * for SMP mode with the IO APIC.
     *
     * Must not be called with enabled interrupts.
     */
    static void enableIoApicMode();

    /**
     * Set the ICR to issue an IPI with self destination.
     *
     * Must be called with enabled interrupts.
     */
    static void verifyIPI();

private:
    // Offsets, IA-32 Architecture Manual Chapter 10.4.1
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
        LVT_CMCI = 0x2F0, // LVT Corrected Machine Check Interrupt Register
        ICR_LOW = 0x300, // Interrupt Command Register (64 bit)
        ICR_HIGH = 0x310,
        LVT_TIMER = 0x320, // LVT Timer Register
        LVT_THERMAL = 0x330, // LVT Thermal Sensor Register
        LVT_PERFORMANCE = 0x340, // LVT Performance Monitoring Counters Register
        LVT_LINT0 = 0x350, // LVT LINT0 Register
        LVT_LINT1 = 0x360, // LVT LINT1 Register
        LVT_ERROR = 0x370, // LVT Error Register
        TIMER_INITIAL = 0x380, // Timer Initial Count Register
        TIMER_CURRENT = 0x390, // Timer Current Count Register
        TIMER_DIVIDE = 0x3E0 // Timer Divide Configuration Register
    };

    // IA-32 Architecture Manual Chapter 10.4.4
    typedef struct {
        bool isBSP;
        bool isX2Apic;
        bool isHWEnabled;
        uint32_t baseField;
    } Apic_MSR;

    // IA-32 Architecture Manual Chapter 10.9
    typedef struct {
        Kernel::InterruptDispatcher::Interrupt spuriousVector;
        bool isSWEnabled;
        bool hasFocusProcessorChecking;
        bool hasEOIBroadcastSuppression;
    } Apic_SVR;

    // IA-32 Architecture Manual Chapter 10.5.1
    enum class LVT_Delivery_Mode : uint8_t {
        FIXED = 0,
        SMI = 0b10,
        NMI = 0b100,
        INIT = 0b101,
        EXTINT = 0b111
    };
    enum class LVT_Delivery_Status : uint8_t { IDLE = 0, PENDING = 1 };
    enum class LVT_Pin_Polarity : uint8_t { HIGH = 0, LOW = 1 }; // TODO: Verify
    enum class LVT_Trigger_Mode : uint8_t { EDGE = 0, LEVEL = 1 };
    enum class LVT_Timer_Mode : uint8_t { ONESHOT = 0, PERIODIC = 1 };
    typedef struct {
        Kernel::InterruptDispatcher::Interrupt slot;
        LVT_Delivery_Mode deliveryMode; // All except timer
        LVT_Delivery_Status deliveryStatus;
        LVT_Pin_Polarity pinPolarity; // Only LINT0, LINT1
        LVT_Trigger_Mode triggerMode; // Only LINT0, LINT1
        bool isMasked;
        LVT_Timer_Mode timerMode; // Only timer
    } LVT_Entry;

    // IA-32 Architecture Manual Chapter 10.6.1
    enum class ICR_Delivery_Mode : uint8_t {
        FIXED = 0,
        LOWPRIO = 1, // Model specific
        SMI = 0b10,
        NMI = 0b100,
        INIT = 0b101,
        STARTUP = 0b110
    };
    enum class ICR_Destination_Mode : uint8_t { PHYSICAL = 0, LOGICAL = 1 };
    enum class ICR_Delivery_Status : uint8_t { IDLE = 0, PENDING = 1 };
    enum class ICR_Level : uint8_t { DEASSERT = 0, ASSERT = 1 };
    enum class ICR_Trigger_Mode : uint8_t { EDGE = 0, LEVEL = 1 };
    enum class ICR_Destination_Shorthand : uint8_t { // If used ICR_DESTINATION_FIELD is ignored
        SELF = 1,
        ALL = 0b10,
        ALL_NO_SELF = 0b11
    };
    typedef struct {
        Kernel::InterruptDispatcher::Interrupt slot;
        ICR_Delivery_Mode deliveryMode;
        ICR_Destination_Mode destinationMode;
        ICR_Delivery_Status deliveryStatus;
        ICR_Level level;
        ICR_Trigger_Mode triggerMode;
        ICR_Destination_Shorthand destinationShorthand;
        uint8_t destinationField;
    } Apic_ICR;

private:
    [[nodiscard]] static Apic_MSR readMSR();

    static void writeMSR(Apic_MSR msr);

    /**
     * Read a value from a local APIC register.
     * Only accesses the local APIC registers of the current CPU core.
     *
     * @param reg The register (offset) to read from (same as LApic::Register)
     * @return The value contained in the specified register
     */
    [[nodiscard]] static uint32_t readDoubleWord(uint16_t reg);

    /**
     * Write a value to a local APIC register.
     * Only accesses the local APIC registers of the current CPU core.
     *
     * @param reg The register (offset) to write to (same as LApic::Register)
     * @param val The value to write
     */
    static void writeDoubleWord(uint16_t reg, uint32_t val);

    [[nodiscard]] static Apic_SVR readSVR();

    static void writeSVR(Apic_SVR svr);

    [[nodiscard]] static LVT_Entry readLVT(Interrupt lint);

    static void writeLVT(Interrupt lint, LVT_Entry entry);

    /**
     * Read the current value from the ICR (Interrupt Command Register).
     *
     * Must not be called with enabled interrupts.
     *
     * @return The ICRs value
     */
    [[nodiscard]] static Apic_ICR readICR();

    /**
     * Issue an IPI (Interprocessor Interrupt) by writing the ICR (Interrupt Command Register).
     *
     * Must not be called with enabled interrupts.
     *
     * @param val The value to write
     */
    static void writeICR(Apic_ICR icr);

    /**
     * Set the local APIC MSR_ENABLE and MSR_BSP flags without modifying the MSR_BASE_FIELD.
     * Only use for the bootstrap processor.
     */
    static void enableHW();

    /**
     * Set the local APIC MSR_BASE_FIELD (IA-32 Architecture Manual Chapter 10.4.5),
     * MSR_ENABLE and MSR_BSP flags.
     * Only use for the bootstrap processor.
     *
     * @param base_address MMIO registers are mapped starting at this address
     */
    static void enableHW(uint32_t base_address);

    /**
     * Unset the APIC MSR_ENABLE flag.
     * Depending on the architecture the local APIC can't be reenabled without a reset.
     */
    static void disableHW();

    /**
     * Check if the local APIC is HW enabled (in the MSR).
     *
     * @return True if the local APIC is HW enabled
     */
    [[nodiscard]] static bool isEnabledHW();

    /**
     * Check if the local APIC is SW enabled (in the SVR).
     *
     * @return True if the local APIC is SW enabled
     */
    [[nodiscard]] static bool isEnabledSW();

    /**
     * Clear the Error Status Register.
     */
    static void clearErrors();

    // TODO: Functions to write LVT more easily

#if HHUOS_LAPIC_ENABLE_DEBUG == 1
    static void logDebugDump();
#endif

private:
    static bool initialized;
    static uint32_t baseVirtAddress; // The MMIO base address is the same for every local APIC
    static Device::ModelSpecificRegister ia32ApicBaseMsr;

    // TODO: The location should be parsed from MP/ACPI tables
    // IA-32 Architecture Manual Chaper 10.4.1
    static const constexpr uint32_t APIC_BASE_DEFAULT_PHYS_ADDRESS = 0xFEE00000;

    // IMCR register, MultiProcessor Specification Chapter 3.6.2.1
    static const IoPort registerSelectorPort;
    static const IoPort registerDataPort;

    static Kernel::Logger log;
};

}

#endif
