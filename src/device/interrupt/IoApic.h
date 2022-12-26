#ifndef __IOAPIC_include__
#define __IOAPIC_include__

#include "ApicRegisterInterface.h"
#include "Apic.h"
#include "kernel/log/Logger.h"

namespace Device {

// TODO: Crosscheck references to the original IO APIC Datasheet with Intel ICH5 Datasheet

// TODO: Do IO APIC GSIs have to be continuous (could there be gaps between IO APICs)?

// TODO: Find the chapter where this is described in some manual (the range is mentioned in ICH5,
//       but I remember to have seen it somewhere else aswell...)
// NOTE: Valid IO APIC REDTBL vectors range between 0x10 and 0xFE

// NOTE: SAPIC/IO SAPIC is not supported

/**
 * @brief This class implements the IO APIC hardware interrupt controller.
 *
 * In contrast to the local APIC the IO APIC is not part of the CPU but of the system chipset (not exclusively).
 * It receives hardware interrupts from external devices (comparable with the PIC) and forwards them to local APICs.
 * This class allows the system to interact with multiple IO APICs.
 */
class IoApic {
public:
    IoApic() = delete; // Static class

    IoApic(const IoApic &copy) = delete;

    IoApic &operator=(const IoApic &copy) = delete;

    IoApic(IoApic &&move) = delete;

    IoApic &operator=(IoApic &&move) = delete;

    ~IoApic() = delete; // Static class

    /**
     * @brief Check if all IO APICs are initialized.
     */
    static bool isInitialized();

    /**
     * @brief Ensure that all IO APICs are initialized.
     */
    static void ensureInitialized();

    /**
     * @brief Initialize all existing IO APICs.
     *
     * Must not be called with enabled interrupts.
     */
    static void initialize();

    /**
     * @brief Unmask an interrupt in the IO APIC.
     *
     * The IO APIC responsible for this GSI will be selected and the interrupt will be sent
     * to the current CPU's local APIC.
     * Must not be called with enabled interrupts.
     *
     * @param interruptSource The GSI to unmask
     */
    static void allow(InterruptSource interruptSource);

    /**
     * @brief Mask an interrupt in the local APIC.
     *
     * The IO APIC responsible for this GSI will be selected.
     * Must not be called with enabled interrupts.
     *
     * @param interruptSource The GSI to mask
     */
    static void forbid(InterruptSource interruptSource);

    /**
     * @brief Get the state of this interrupt - whether it is masked out or not.
     *
     * The IO APIC responsible for this GSI will be selected.
     *
     * @param interruptSource The GSI
     * @return True, if the GSI is masked
     */
    static bool status(InterruptSource interruptSource);

    /**
     * @brief Send an end of interrupt signal.
     *
     * The EOI will be sent to the IO APIC responsible for the GSI.
     * Only compatible with IO APIC version >= 0x20.
     * (https://github.com/torvalds/linux/blob/master/arch/x86/kernel/apic/io_apic.c#L470)
     * (Intel ICH5 Datasheet Chapter 9.5.5)
     *
     * @param vector The vector number of the interrupt that will be marked as completed
     */
    static void sendEndOfInterrupt(InterruptVector vector);

private:
    // Offsets, Intel ICH5 Datasheet Chapter 9.5.1 Table 144
    enum Register : uint8_t {
        IND = 0x00,
        DAT = 0x10,
        IRQPA = 0x20,
        EOI = 0x40
    };

    // Selectors, IOAPIC Datasheet Chapter 3.0 Table 2
    enum Indirect_Register : uint8_t {
        ID = 0x00, // ID
        VER = 0x01, // Version
        ARB = 0x02, // Arbitration ID
        REDTBL = 0x10 // Redirection table base address (24x 64 bit entry)
    };

private:
    /**
     * @brief Ensure that the IO APIC's MMIO region has been initialized.
     */
    static void ensureMMIO(IoApicInformation *ioapic);

    /**
     * @brief Ensure that the GSI belongs to the supplied IO APIC.
     */
    static void ensureValidGsi(IoApicInformation *ioapic, GlobalSystemInterrupt gsi);

    /**
     * @brief Initialize a single IO APIC.
     */
    static void initializeController(IoApicInformation *ioapic);

    /**
     * @brief Allocate the memory region used to access a single IO APIC's registers.
     */
    static void initializeMMIORegion(IoApicInformation *ioapic);

    /**
     * @brief Initialize a single IO APIC's interrupt redirection table.
     *
     * Marks entries for all supported interrupt inputs of the IO APIC as edge-triggered, active high,
     * masked, physical destination mode to local APIC of the current CPU and fixed delivery mode,
     * unless trigger mode or pin polarity are overridden.
     * Sets vector numbers to corresponding InterruptDispatcher::Interrupt.
     * Must not be called with enabled interrupts.
     */
    static void initializeREDTBL(IoApicInformation *ioapic);

    // NOTE: Reading and writing IO APIC's registers.
    // NOTE: Parses the read/written value to/from types from ApicRegisterInterface.h
    // NOTE: Affects the registers of the passed IO APIC

    template<typename T>
    [[nodiscard]] static T readDirectRegister(IoApicInformation *ioapic, Register reg);

    template<typename T>
    static void writeDirectRegister(IoApicInformation *ioapic, Register reg, T val);

    [[nodiscard]] static uint32_t readDoubleWord(IoApicInformation *ioapic, Indirect_Register reg);

    static void writeDoubleWord(IoApicInformation *ioapic, Indirect_Register reg, uint32_t val);

    [[nodiscard]] static REDTBLEntry readREDTBL(IoApicInformation *ioapic, GlobalSystemInterrupt gsi);

    static void writeREDTBL(IoApicInformation *ioapic, GlobalSystemInterrupt gsi, const REDTBLEntry &redtbl);

private:
    static bool initialized;
    static Kernel::Logger log; // TODO: Remove?
};

template<typename T>
T IoApic::readDirectRegister(IoApicInformation *ioapic, Register reg) {
    ensureMMIO(ioapic);
    return *reinterpret_cast<volatile T *>(ioapic->virtAddress + reg);
}

template<typename T>
void IoApic::writeDirectRegister(IoApicInformation *ioapic, Register reg, T val) {
    ensureMMIO(ioapic);
    *reinterpret_cast<volatile T *>(ioapic->virtAddress + reg) = val;
}

}

#endif
