#ifndef __LAPIC_include__
#define __LAPIC_include__

#include <cstdint>
#include "device/cpu/IoPort.h"
#include "device/interrupt/Pic.h"
#include "device/interrupt/ModelSpecificRegister.h"
#include "kernel/log/Logger.h"

#define HHUOS_LAPIC_ENABLE_DEBUG 1

namespace Device {

class LApic {
public:
    /**
     * Default-Constructor.
     */
    LApic() = default;

    /**
     * Copy Constructor.
     */
    LApic(const LApic &copy) = delete;

    /**
     * Assignment operator.
     */
    LApic &operator=(const LApic &other) = delete;

    /**
     * Destructor.
     */
    ~LApic() = default;

    /**
     * Check if local APIC support is present (using CPUID).
     *
     * @return True if local APIC is supported
     */
    static bool hasApicSupport();

    /**
     * Check if x2APIC support is present (using CPUID).
     *
     * @return True if x2APIC is supported
     */
    static bool hasX2ApicSupport();

    /**
     * Check if APIC is running in x2Apic mode.
     *
     * @return True if APIC is running in x2Apic mode
     */
    [[nodiscard]] bool isX2Apic() const;

    /**
     * Get the ID of the local APIC belonging to the current CPU core.
     *
     * @return The local APIC's ID
     */
    [[nodiscard]] uint8_t getId() const;

    /**
     * Get the local APIC version.
     * Should be identical over all CPU cores.
     *
     * @return The local APIC's version.
     */
    [[nodiscard]] uint8_t getVersion() const;

    /**
     * Set the local APIC MSR_ENABLE and MSR_BSP flags without modifying the MSR_BASE_FIELD.
     * Only use for the bootstrap processor.
     */
    void enable();

    /**
     * Set the local APIC MSR_BASE_FIELD (IA-32 Architecture Manual Chapter 10.4.5), MSR_ENABLE and MSR_BSP flags.
     * Only use for the bootstrap processor.
     *
     * @param base_address MMIO registers are mapped starting at this address
     */
    void enable(void* base_address);

    /**
     * Check if the local APIC is enabled.
     *
     * @return True if the local APIC is HW enabled
     */
    [[nodiscard]] bool isEnabled() const;

    /**
     * Determine the local APIC's base address.
     *
     * @return The start of the APIC's MMIO region
     */
    [[nodiscard]] void* getBaseAddr() const;

    /**
     * Unset the APIC MSR_ENABLE flag.
     */
    void disable();

    /**
     * Initialize the local APIC.
     * Only use for the bootstrap processor.
     */
    void init();

    /**
     * Unmask an interrupt number in the corresponding PIC. If this is done,
     * all interrupts with this number will be passed to the CPU.
     *
     * @param interrupt The number of the interrupt to activated
     */
    void allow(Pic::Interrupt interrupt);

    /**
     * Forbid an interrupt. If this is done, the interrupt is masked out
     * and every interrupt with this number that is thrown will be
     * suppressed and not arrive the CPU.
     *
     * @param interrupt The number of the interrupt to deactivate
     */
    void forbid(Pic::Interrupt interrupt);

private:
    /**
     * Read a value from a local APIC register.
     * Only accesses the local APIC registers of the current CPU core.
     *
     * @param reg The register (offset) to read from
     * @return The value contained in the specified register
     */
    [[nodiscard]] uint32_t readDoubleWord(uint16_t reg) const;

    /**
     * Write a value to a local APIC register.
     * Only accesses the local APIC registers of the current CPU core.
     *
     * @param reg The register (offset) to write to
     * @param val The value to write
     */
    void writeDoubleWord(uint16_t reg, uint32_t val);

    /**
     * Set the IMCR (Interrupt Mode Control Register) to connect the APIC to the BSP.
     * Set local APIC LINT0 to ExtINT for external (PIC) interrupt controller.
     * By default the PIC is hardwired to the BSP, ignoring the APIC.
     */
    void enableVirtualWire();

#if HHUOS_LAPIC_ENABLE_DEBUG == 1
    /**
     * Dump some status registers to DBG log.
     */
    void logDebugDump();
#endif

    // TODO: Copied from Pic.h, what of these do I need? EOI for sure...

    // /**
    //  * Get the state of this interrupt - whether it is masked out or not.
    //  *
    //  * @param interrupt The number of the interrupt
    //  * @return true, if the interrupt is disabled
    //  */
    // bool status(Interrupt interrupt);

    // /**
    //  * Send an end of interrupt signal to the corresponding PIC.
    //  *
    //  * @param interrupt The number of the interrupt for which to send an EOI
    //  */
    // void sendEndOfInterrupt(Interrupt interrupt);

private:
    void* APIC_BASE_VIRT_ADDRESS = nullptr;
    static const constexpr uint32_t APIC_BASE_DEFAULT_PHYS_ADDRESS = 0xFEE00000;

    // IA-32 Architecture Manual Figure 10-5
    Device::ModelSpecificRegister IA32_APIC_BASE_MSR = Device::ModelSpecificRegister(0x1B);
    static const constexpr uint16_t APIC_MSR_BSP_FLAG = 0x100; // Bit 8
    static const constexpr uint16_t APIC_MSR_X2APIC_ENABLE_FLAG = 0x400; // Bit 10
    static const constexpr uint16_t APIC_MSR_ENABLE_FLAG = 0x800; // Bit 11
    static const constexpr uint32_t APIC_MSR_BASE_FIELD_MASK = 0xFFFFF000; // Bits 12 through 35

    // Offsets, IA-32 Architecture Manual Table 10-1
    static const constexpr uint8_t APIC_ID = 0x20; // ID
    static const constexpr uint8_t APIC_VER = 0x30; // Version
    static const constexpr uint8_t APIC_TPR = 0x80; // Task Priority Register
    static const constexpr uint8_t APIC_APR = 0x90; // Arbitration Priority Register
    static const constexpr uint8_t APIC_PPR = 0xA0; // Processor Priority Register
    static const constexpr uint8_t APIC_EOI = 0xB0; // End of Interrupt Register
    static const constexpr uint8_t APIC_RRD = 0xC0; // Remote Read Register
    static const constexpr uint8_t APIC_LDR = 0xD0; // Local Destination Register
    static const constexpr uint8_t APIC_DFR = 0xE0; // Destination Format Register
    static const constexpr uint8_t APIC_SVR = 0xF0; // Spurious Interrupt Vector Register
    static const constexpr uint16_t APIC_ISR = 0x100; // In-Service Register (255 bit)
    static const constexpr uint16_t APIC_TMR = 0x180; // Trigger Mode Register (255 bit)
    static const constexpr uint16_t APIC_IRR = 0x200; // Interrupt Request Register (255 bit)
    static const constexpr uint16_t APIC_ESR = 0x280; // Error Status Register
    static const constexpr uint16_t APIC_LVT_CMCI = 0x2F0; // LVT Corrected Machine Check Interrupt Register
    static const constexpr uint16_t APIC_ICR = 0x300; // Interrupt Command Register (64 bit)
    static const constexpr uint16_t APIC_LVT_TIMER = 0x320; // LVT Timer Register
    static const constexpr uint16_t APIC_LVT_THERMAL = 0x330; // LVT Thermal Sensor Register
    static const constexpr uint16_t APIC_LVT_PERFORMANCE = 0x340; // LVT Performance Monitoring Counters Register
    static const constexpr uint16_t APIC_LVT_LINT0 = 0x350; // LVT LINT0 Register
    static const constexpr uint16_t APIC_LVT_LINT1 = 0x360; // LVT LINT1 Register
    static const constexpr uint16_t APIC_LVT_ERROR = 0x370; // LVT Error Register
    static const constexpr uint16_t APIC_TIMER_INITIAL = 0x380; // Timer Initial Count Register
    static const constexpr uint16_t APIC_TIMER_CURRENT = 0x390; // Timer Current Count Register
    static const constexpr uint16_t APIC_TIMER_DIVIDE = 0x3E0; // Timer Divide Configuration Register

    // IA-32 Architecture Manual Figure 10-8
    static const constexpr uint32_t APIC_LVT_MASKED_FLAG = 0x10000; // Bit 16
    static const constexpr uint16_t APIC_LVT_DELIVERY_MODE_MASK = 0x700; // Bits 8 through 10
    static const constexpr uint8_t APIC_LVT_DELIVERY_MODE_FIXED = 0x0; // 000
    static const constexpr uint8_t APIC_LVT_DELIVERY_MODE_SMI = 0x2; // 010
    static const constexpr uint8_t APIC_LVT_DELIVERY_MODE_NMI = 0x4; // 100
    static const constexpr uint8_t APIC_LVT_DELIVERY_MODE_INIT = 0x5; // 101
    static const constexpr uint8_t APIC_LVT_DELIVERY_MODE_EXTINT = 0x7; // 111
    static const constexpr uint16_t APIC_LVT_DELIVERY_STATUS = 0x1000; // Bit 12
    static const constexpr uint16_t APIC_LVT_TRIGGER_MODE = 0x8000; // Bit 15

    // IA-32 Architecture Manual Figure 10-23
    static const constexpr uint8_t APIC_SVR_SPURIOUS_VECTOR_MASK = 0xFF; // Bits 0 through 7
    static const constexpr uint16_t APIC_SVR_SW_ENABLE_FLAG = 0x100; // Bit 8

    // MultiProcessor Specification Chapter 3.6.2.1
    const IoPort registerSelectorPort = IoPort(0x22);
    const IoPort registerDataPort = IoPort(0x23);

    static Kernel::Logger log;
};

}

#endif
