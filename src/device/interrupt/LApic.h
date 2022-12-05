#ifndef __LAPIC_include__
#define __LAPIC_include__

#include <cstdint>
#include "device/cpu/IoPort.h"
#include "device/interrupt/Pic.h"
#include "device/interrupt/ModelSpecificRegister.h"
#include "kernel/log/Logger.h"
#include "kernel/interrupt/InterruptDispatcher.h"

#define HHUOS_LAPIC_ENABLE 1
#define HHUOS_LAPIC_ENABLE_DEBUG 1

// NOTE: There are now 4 enums in total that are called "Interrupt":
// NOTE: - Pic::Interrupt (Physical IRQ lines on the PIC), referred to as irq
// NOTE: - LApic::Interrupt (Local interrupt on one of the 7 physical pins on the local APIC), referred to as lint
// NOTE: - IoApic::Interrupt (Physical IRQ lines on the IO APIC), referred to as gsi (Global System Interrupt)
// NOTE: - InterruptDispatcher::Interrupt (IDT vector number), referred to as slot

namespace Device {

// TODO: Most of this is static, is this the preferred way?
class LApic {
    friend class ApicTimer; // ApicTimer is configured by using LApic registers

public:
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

public:
    LApic() = delete;

    LApic(const LApic &copy) = delete;

    LApic &operator=(const LApic &other) = delete;

    ~LApic() = default;

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
     * Check if local APIC of the current CPU is HW and SW enabled.
     *
     * @return True if APIC is HW and SW enabled
     */
    [[nodiscard]] static bool isEnabled();

    /**
     * Get the ID of the local APIC belonging to the current CPU.
     *
     * @return The local APIC's ID
     */
    [[nodiscard]] static uint8_t getId();

    /**
     * Get the local APIC version.
     *
     * @return The local APIC's version.
     */
    [[nodiscard]] static uint8_t getVersion();

    /**
     * Initialize the local APIC with all local APIC interrupts masked and
     * EOI broadcasting disabled.
     * Only call explicitly for the bootstrap processor.
     *
     * Must not be called with enabled interrupts.
     */
    static void init();

    /**
     * Initialize the local APIC of an additional AP (Application Processor).
     * (MultiProcessor Specification Appendix B.4)
     *
     * Must not be called with enabled interrupts.
     *
     * @param destination The local APIC ID of the target CPU
     * @param startup_routine The physical address of the entry point for the target CPU
     */
    static void initAP(uint8_t destination, uint32_t startup_routine) = delete;

    /**
     * Unmask a local interrupt in the local APIC of the current CPU.
     *
     * @param lint The local interrupt to activated
     * @param slot The vector number of the local interrupt
     */
    static void allow(Interrupt lint, Kernel::InterruptDispatcher::Interrupt slot);

    /**
     * Forbid a local interrupt in the local APIC of the current CPU.
     *
     * @param lint The local interrupt to deactivate
     */
    static void forbid(Interrupt lint);

    /**
     * Get the state of this interrupt - whether it is masked out or not.
     *
     * @param lint The register offset of the interrupt
     * @return True, if the interrupt is disabled in the local APIC of the current CPU
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

    // TODO: To determine if this is available MP/ACPI tables have to be parsed
    /**
     * Set the IMCR (Interrupt Mode Control Register) to physically connect the PIC to the BSP.
     * IMCR is only available on legacy hardware.
     * Only valid with a single CPU.
     *
     * Must not be called with enabled interrupts.
     */
    static void enablePicMode() = delete;

    // TODO: To determine if this has to be set MP/ACPI tables have to be parsed
    /**
     * Set the IMCR (Interrupt Mode Control Register) to physically connect the APIC to the BSP.
     * IMCR is only available on legacy hardware.
     * Only valid with a single CPU.
     * (QEMU: This is the default mode, PIC mode is not supported.)
     *
     * Must not be called with enabled interrupts.
     */
    static void enableVirtualWireMode();

    // TODO: To determine if this is available MP/ACPI tables have to be parsed
    /**
     * Disables virtual wire mode and configures the local APIC
     * for SMP mode with the IO APIC.
     *
     * Must not be called with enabled interrupts.
     */
    static void enableIoApicMode();

    /**
     * Set the ICR of the current CPU's local APIC to issue an IPI with self destination.
     *
     * Must not be called with enabled interrupts.
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
    } MSR_Entry;

    // IA-32 Architecture Manual Chapter 10.9
    typedef struct {
        Kernel::InterruptDispatcher::Interrupt spuriousVector;
        bool isSWEnabled;
        bool hasFocusProcessorChecking;
        bool hasEOIBroadcastSuppression;
    } SVR_Entry;

    // IA-32 Architecture Manual Chapter 10.5.1
    enum class LVT_Delivery_Mode : uint8_t {
        FIXED = 0,
        SMI = 0b10,
        NMI = 0b100,
        INIT = 0b101,
        EXTINT = 0b111
    };
    enum class LVT_Delivery_Status : uint8_t {
        IDLE = 0, PENDING = 1
    };
    enum class LVT_Pin_Polarity : uint8_t {
        HIGH = 0, LOW = 1
    }; // TODO: Verify
    enum class LVT_Trigger_Mode : uint8_t {
        EDGE = 0, LEVEL = 1
    };
    enum class LVT_Timer_Mode : uint8_t {
        ONESHOT = 0, PERIODIC = 1
    };
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
    enum class ICR_Destination_Mode : uint8_t {
        PHYSICAL = 0, LOGICAL = 1
    };
    enum class ICR_Delivery_Status : uint8_t {
        IDLE = 0, PENDING = 1
    };
    enum class ICR_Level : uint8_t {
        DEASSERT = 0, ASSERT = 1
    };
    enum class ICR_Trigger_Mode : uint8_t {
        EDGE = 0, LEVEL = 1
    };
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
    } ICR_Entry;

private:
    /**
     * Read the APIC MSR.
     *
     * @return The read MSR entry
     */
    [[nodiscard]] static MSR_Entry readMSR();

    /**
     * Write the APIC MSR.
     *
     * @param entry The MSR entry to write
     */
    static void writeMSR(MSR_Entry entry);

    /**
     * Read a value from a memory mapped register of the current CPU's local APIC.
     *
     * @param reg The register (offset) to read from (can use LApic::Register)
     * @return The value contained in the specified register
     */
    [[nodiscard]] static uint32_t readDoubleWord(uint16_t reg);

    /**
     * Write a value to a memory mapped register of the current CPU's local APIC.
     *
     * @param reg The register (offset) to write to (can use LApic::Register)
     * @param val The value to write
     */
    static void writeDoubleWord(uint16_t reg, uint32_t val);

    /**
     * Read the SVR (Spurious Interrupt Vector Register) of the current CPU's local APIC.
     *
     * @return The read SVR entry
     */
    [[nodiscard]] static SVR_Entry readSVR();

    /**
     * Write the SVR (Spurious Interrupt Vector Register) of the current CPU's local APIC.
     *
     * @param svr The SVR entry to write
     */
    static void writeSVR(SVR_Entry svr);

    /**
     * Read a LVT (Local Vector Table) entry of the current CPU's local APIC.
     *
     * @param lint The local interrupt
     * @return The read LVT entry belonging to the specified local interrupt
     */
    [[nodiscard]] static LVT_Entry readLVT(Interrupt lint);

    /**
     * Write a LVT (Local Vector Table) entry of the current CPU's local APIC.
     *
     * @param lint The local interrupt
     * @param entry The LVT entry to write belonging to the specified local interrupt
     */
    static void writeLVT(Interrupt lint, LVT_Entry entry);

    /**
     * Read the ICR (Interrupt Command Register) of the current CPU's local APIC.
     *
     * Must not be called with enabled interrupts.
     *
     * @return The read ICR entry
     */
    [[nodiscard]] static ICR_Entry readICR();

    /**
     * Write the ICR (Interrupt Command Register) of the current CPU's local APIC..
     * Used to issue IPIs (Interprocessor Interrupts)
     *
     * Must not be called with enabled interrupts.
     *
     * @param val The ICR entry to write
     */
    static void writeICR(ICR_Entry icr);

    /**
     * Set the local APIC MSR_ENABLE and MSR_BSP flags without modifying the MSR_BASE_FIELD.
     * Only use for the BSP (Bootstrap Processor).
     */
    static void enableHW();

    /**
     * Set the local APIC MSR_BASE_FIELD (IA-32 Architecture Manual Chapter 10.4.5),
     * MSR_ENABLE and MSR_BSP flags.
     * Only use for the BSP (Bootstrap Processor).
     *
     * @param base_address MMIO registers will be mapped starting at this address
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
     * Clear the ESR (Error Status Register) of the current CPU's local APIC.
     */
    static void clearErrors();

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
