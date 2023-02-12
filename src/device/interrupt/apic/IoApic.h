#ifndef __IOAPIC_include__
#define __IOAPIC_include__

#include "LocalApic.h"

namespace Device {

/**
 * @brief This class implements the I/O APIC hardware interrupt controller.
 *
 * In contrast to the local APIC, the I/O APIC is not part of the CPU, but of the system chipset.
 * It receives hardware interrupts from external devices (comparable to the PIC) and forwards them to local APICs.
 * This class allows the system to interact with a single I/O APIC.
 * To support systems with multiple I/O APICs, the Apic class manages multiple IoApic instances.
 */
class IoApic {
    friend class Apic; // Apic exposes this class' functionality to the OS

public:
    /**
     * @brief Constructs an IoApic instance.
     *
     * @param ioApicPlatform General information about the I/O APICs, parsed from ACPI
     * @param ioApicInformation Information about the specific I/O APIC, parsed from ACPI
     */
    IoApic(IoApicPlatform *ioApicPlatform, IoApicInformation &&ioApicInformation);

    IoApic(const IoApic &copy) = delete;

    IoApic &operator=(const IoApic &copy) = delete;

    ~IoApic() = default;

private:
    /**
     * @brief MMIO accessible registers.
     *
     * ICH5 datasheet, sec. 9.5
     */
    enum Register : uint8_t {
        IND = 0x00, ///< @brief Indirection register, for indirect register access
        DAT = 0x10, ///< @brief Data register, for indirect register access
        IRQPA = 0x20,
        EOI = 0x40 ///< @brief Dedicated end-of-interrupt register, since version 0x20
    };

    /**
     * @brief Indirectly accessible registers.
     *
     * ICH5 datasheet, sec. 9.5
     */
    enum IndirectRegister : uint8_t {
        ID = 0x00,    ///< @brief I/O APIC id.
        VER = 0x01,   ///< @brief I/O APIC version.
        ARB = 0x02,   ///< @brief Arbitration ID.
        REDTBL = 0x10 ///< @brief Redirection table base address (24x 64 bit entry)
    };

private:
    /**
     * @brief Initialize the I/O APIC.
     */
    void initialize();

    /**
     * @brief Unmask an interrupt in this IO APIC.
     *
     * @param gsi The GSI to unmask
     */
    void allow(Kernel::GlobalSystemInterrupt gsi);

    /**
     * @brief Mask an interrupt in the local APIC.
     *
     * @param gsi The GSI to mask
     */
    void forbid(Kernel::GlobalSystemInterrupt gsi);

    /**
     * @brief Get the state of this interrupt - whether it is masked out or not.
     *
     * @param gsi The GSI
     * @return True, if the GSI is masked
     */
    bool status(Kernel::GlobalSystemInterrupt gsi);

    /**
     * @brief Send an end of interrupt signal (only acts on level-triggered interrupts).
     *
     * @param vector The vector number of the interrupt that will be marked as completed
     * @param gsi The GSI number of the interrupt that will be marked as completed
     */
    void sendEndOfInterrupt(Kernel::InterruptVector vector, Kernel::GlobalSystemInterrupt gsi);

    /**
     * @brief Ensure that a GSI belongs to this I/O APIC.
     */
    void ensureValidGsi(Kernel::GlobalSystemInterrupt gsi) const;

    /**
     * @brief Ensure that this I/O APIC's MMIO region has been initialized.
     */
    void ensureRegisterAccess() const;

    /**
     * @brief Allocate the memory region used to access this I/O APIC's registers.
     */
    void initializeMMIORegion();

    /**
     * @brief Initialize a this I/O APIC's interrupt redirection table.
     *
     * Marks entries for all supported interrupt inputs of the IO APIC as edge-triggered, active high,
     * masked, physical destination mode to local APIC of the current CPU and fixed delivery mode,
     * unless trigger mode or pin polarity are overridden. Sets vector numbers to corresponding InterruptVector.
     */
    void initializeREDTBL();

    void dumpREDTBL();

    /**
     * @brief Read a MMIO register, identified by the offset to the I/O APIC base address.
     *
     * @tparam T The type of the register data, e.g. uint32_t
     */
    template<typename T>
    [[nodiscard]] T readMMIORegister(Register reg);

    /**
     * @brief Write a MMIO register, identified by the offset to the I/O APIC base address.
     *
     * @tparam T The type of the register data, e.g. uint32_t
     */
    template<typename T>
    void writeMMIORegister(Register reg, T val);

    /**
     * @brief Read an indirect register, identified by the address written to the indirection register.
     */
    [[nodiscard]] uint32_t readIndirectRegister(IndirectRegister reg);

    /**
     * @brief Write an indirect register, identified by the address written to the indirection register.
     */
    void writeIndirectRegister(IndirectRegister reg, uint32_t val);

    /**
     * @brief Write a redirection table entry, identified by the corresponding GSI.
     */
    [[nodiscard]] REDTBLEntry readREDTBL(Kernel::GlobalSystemInterrupt gsi);

    /**
     * @brief Read a redirection table entry, identified by the corresponding GSI.
     */
    void writeREDTBL(Kernel::GlobalSystemInterrupt gsi, const REDTBLEntry &redtbl);

private:
    bool initialized = false; ///< @brief Indicates if IoApic::initialize() has been called.

    IoApicInformation info;          ///< @brief Information about a single I/O APIC.
    static IoApicPlatform *platform; ///< @brief Information about all I/O APICs.

    static Kernel::Logger log;
};

template<typename T>
T IoApic::readMMIORegister(Register reg) {
    ensureRegisterAccess();
    return *reinterpret_cast<volatile T *>(info.virtAddress + reg);
}

template<typename T>
void IoApic::writeMMIORegister(Register reg, T val) {
    ensureRegisterAccess();
    *reinterpret_cast<volatile T *>(info.virtAddress + reg) = val;
}

} // namespace Device

#endif
