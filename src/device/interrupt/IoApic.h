#ifndef __IOAPIC_include__
#define __IOAPIC_include__

#include "ApicRegisterInterface.h"
#include "ApicAcpiInterface.h"
#include "InterruptSource.h"
#include "kernel/log/Logger.h"

namespace Device {

// TODO: Crosscheck references to the original IO APIC Datasheet with Intel ICH5 Datasheet

// TODO: Find the chapter where this is described in some manual (the range is mentioned in ICH5,
//       but I remember to have seen it somewhere else aswell...)
// NOTE: Valid IO APIC REDTBL vectors range between 0x10 and 0xFE

// NOTE: SAPIC/IO SAPIC is not supported

/**
 * @brief This class implements the IO APIC hardware interrupt controller.
 *
 * In contrast to the local APIC the IO APIC is not part of the CPU, but of the system chipset (not exclusively).
 * It receives hardware interrupts from external devices (comparable with the PIC) and forwards them to local APICs.
 * This class allows the system to interact with a single IO APIC.
 * To support systems with multiple IO APICs, the Apic "controller" class manages multiple IoApic instances.
 */
class IoApic {
    friend class Apic;

public:
    IoApic() = default;

    IoApic(const IoApic &copy) = delete;

    IoApic &operator=(const IoApic &copy) = delete;

    ~IoApic() = default;

    bool isInitialized() const;

    /**
     * @brief Initialize this existing IO APIC.
     */
    void initialize(IoApicPlatform *platform, IoApicInformation *info);

    /**
     * @brief Unmask an interrupt in this IO APIC.
     *
     * @param gsi The GSI to unmask
     */
    void allow(GlobalSystemInterrupt gsi);

    /**
     * @brief Mask an interrupt in the local APIC.
     *
     * @param gsi The GSI to mask
     */
    void forbid(GlobalSystemInterrupt gsi);

    /**
     * @brief Get the state of this interrupt - whether it is masked out or not.
     *
     * @param gsi The GSI
     * @return True, if the GSI is masked
     */
    bool status(GlobalSystemInterrupt gsi);

    /**
     * @brief Send an end of interrupt signal.
     *
     * Only compatible with IO APIC version >= 0x20.
     * (https://github.com/torvalds/linux/blob/master/arch/x86/kernel/apic/io_apic.c#L470)
     * (Intel ICH5 Datasheet Chapter 9.5.5)
     *
     * @param vector The vector number of the interrupt that will be marked as completed
     */
    void sendEndOfInterrupt(InterruptVector vector);

private:
    /**
     * @brief MMIO accessible registers.
     */
    enum Register : uint8_t {
        IND = 0x00,
        DAT = 0x10,
        IRQPA = 0x20,
        EOI = 0x40
    };

    /**
     * @brief Indirectly accessible registers.
     */
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
    void ensureMMIO() const;

    /**
     * @brief Ensure that the GSI belongs to the supplied IO APIC.
     */
    void ensureValidGsi(GlobalSystemInterrupt gsi) const;

    /**
     * @brief Allocate the memory region used to access a single IO APIC's registers.
     */
    void initializeMMIORegion();

    /**
     * @brief Initialize a single IO APIC's interrupt redirection table.
     *
     * Marks entries for all supported interrupt inputs of the IO APIC as edge-triggered, active high,
     * masked, physical destination mode to local APIC of the current CPU and fixed delivery mode,
     * unless trigger mode or pin polarity are overridden.
     * Sets vector numbers to corresponding InterruptDispatcher::Interrupt.
     */
    void initializeREDTBL();

    // NOTE: Reading and writing IO APIC's registers.
    // NOTE: Parses the read/written value to/from types from ApicRegisterInterface.h
    // NOTE: Affects the registers of the passed IO APIC

    template<typename T>
    [[nodiscard]] T readDirectRegister(Register reg);

    template<typename T>
    void writeDirectRegister(Register reg, T val);

    [[nodiscard]] uint32_t readDoubleWord(Indirect_Register reg);

    void writeDoubleWord(Indirect_Register reg, uint32_t val);

    [[nodiscard]] REDTBLEntry readREDTBL(GlobalSystemInterrupt gsi);

    void writeREDTBL(GlobalSystemInterrupt gsi, const REDTBLEntry &redtbl);

private:
    bool initialized = false;

    IoApicInformation *ioInfo = nullptr; ///< @brief General information about a single (this) IO APIC
    static IoApicPlatform *ioPlatform; ///< @brief General information valid for all IO APICs

    static Kernel::Logger log;
};

template<typename T>
T IoApic::readDirectRegister(Register reg) {
    ensureMMIO();
    return *reinterpret_cast<volatile T *>(ioInfo->virtAddress + reg);
}

template<typename T>
void IoApic::writeDirectRegister(Register reg, T val) {
    ensureMMIO();
    *reinterpret_cast<volatile T *>(ioInfo->virtAddress + reg) = val;
}

}

#endif
