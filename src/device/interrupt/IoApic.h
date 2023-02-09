#ifndef __IOAPIC_include__
#define __IOAPIC_include__

#include "LocalApic.h"

namespace Device {

/**
 * @brief This class implements the I/O APIC hardware interrupt controller.
 *
 * In contrast to the local APIC the I/O APIC is not part of the CPU, but of the system chipset.
 * It receives hardware interrupts from external devices (comparable with the PIC) and forwards them to local APICs.
 * This class allows the system to interact with a single I/O APIC.
 * To support systems with multiple I/O APICs, the Apic "controller" class manages multiple IoApic instances.
 */
class IoApic {
    friend class Apic;

public:
    IoApic(IoApicPlatform *ioApicPlatform, IoApicInformation &&ioApicInformation);

    IoApic(const IoApic &copy) = delete;

    IoApic &operator=(const IoApic &copy) = delete;

    ~IoApic() = default;

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
    enum IndirectRegister : uint8_t {
        ID = 0x00, // ID
        VER = 0x01, // Version
        ARB = 0x02, // Arbitration ID
        REDTBL = 0x10 // Redirection table base address (24x 64 bit entry)
    };

private:
    [[nodiscard]] bool isInitialized() const;

    /**
     * @brief Initialize this existing IO APIC.
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
     */
    void sendEndOfInterrupt(Kernel::InterruptVector vector, Kernel::GlobalSystemInterrupt gsi);

    /**
     * @brief Ensure that this I/O APIC's MMIO region has been initialized.
     */
    void ensureRegisterAccess() const;

    /**
     * @brief Ensure that a GSI belongs to this I/O APIC.
     */
    void ensureValidGsi(Kernel::GlobalSystemInterrupt gsi) const;

    /**
     * @brief Allocate the memory region used to access this I/O APIC's registers.
     */
    void initializeMMIORegion();

    /**
     * @brief Initialize a this I/O APIC's interrupt redirection table.
     *
     * TODO: "unless trigger mode or pin polarity are overridden" is not valid as ACPI is bugged...
     * Marks entries for all supported interrupt inputs of the IO APIC as edge-triggered, active high,
     * masked, physical destination mode to local APIC of the current CPU and fixed delivery mode,
     * unless trigger mode or pin polarity are overridden.
     * Sets vector numbers to corresponding InterruptVector.
     */
    void initializeREDTBL();

#if HHUOS_APIC_ENABLE_DEBUG == 1
    void dumpREDTBL();
#endif

    template<typename T>
    [[nodiscard]] T readDirectRegister(Register reg);

    template<typename T>
    void writeDirectRegister(Register reg, T val);

    [[nodiscard]] uint32_t readDoubleWord(IndirectRegister reg);

    void writeDoubleWord(IndirectRegister reg, uint32_t val);

    [[nodiscard]] REDTBLEntry readREDTBL(Kernel::GlobalSystemInterrupt gsi);

    void writeREDTBL(Kernel::GlobalSystemInterrupt gsi, const REDTBLEntry &redtbl);

private:
    bool initialized = false;
    static IoApicPlatform *ioPlatform;
    IoApicInformation ioInfo;

    static Kernel::Logger log;
};

template<typename T>
T IoApic::readDirectRegister(Register reg) {
    ensureRegisterAccess();
    return *reinterpret_cast<volatile T *>(ioInfo.virtAddress + reg);
}

template<typename T>
void IoApic::writeDirectRegister(Register reg, T val) {
    ensureRegisterAccess();
    *reinterpret_cast<volatile T *>(ioInfo.virtAddress + reg) = val;
}

}

#endif
