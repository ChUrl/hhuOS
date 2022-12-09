#ifndef __IOAPIC_include__
#define __IOAPIC_include__

#include <cstdint>
#include "kernel/log/Logger.h"
#include "kernel/interrupt/InterruptDispatcher.h"
#include "device/interrupt/Pic.h"

#define HHUOS_IOAPIC_ENABLE 1
#define HHUOS_IOAPIC_ENABLE_DEBUG 1

namespace Device {

// TODO: In order to detect IO APIC(s), MP/ACPI tables have to be parsed.
//       Currently I just assume that one IO APIC exists, which is true for QEMU with +apic
//       This code doesn't account for multiple IO APICs at all

class IoApic {
public:
    enum Interrupt : uint8_t {
        FREE0 = 0x00,
        KEYBOARD = 0x01,
        PIT = 0x02, // (QEMU: PIT is mapped to IO APIC GSI2)
        COM2 = 0x03,
        COM1 = 0x04,
        LPT2 = 0x05,
        FLOPPY = 0x06,
        LPT1 = 0x07,
        RTC = 0x08,
        FREE1 = 0x09,
        FREE2 = 0x0A,
        FREE3 = 0x0B,
        MOUSE = 0x0C,
        FPU = 0x0D,
        PRIMARY_ATA = 0x0E,
        SECONDARY_ATA = 0x0F,

        // TODO: Technically depends on max REDTBL
        FREE4 = 0x10,
        FREE5 = 0x11,
        FREE6 = 0x12,
        FREE7 = 0x13,
        FREE8 = 0x14,
        FREE9 = 0x15,
        FREE10 = 0x16,
        FREE11 = 0x17,
    };

public:
    IoApic() = delete;

    IoApic(const IoApic &copy) = delete;

    IoApic &operator=(const IoApic &other) = delete;

    ~IoApic() = default;

    static bool isInitialized();

    [[nodiscard]] static uint8_t getId();

    // NOTE: Intel ICH2, ICH5 Datasheets Chapter 9.5.7 describe version 0x20
    /**
     * Determine the IO APIC's version.
     *
     * @return The IO APIC's version
     */
    [[nodiscard]] static uint8_t getVersion();

    // NOTE: IO APICs with version below 0x20 don't support EOI register (QEMU reports 0x20)
    // NOTE: (https://github.com/torvalds/linux/blob/master/arch/x86/kernel/apic/io_apic.c#L470)
    /**
     * Determine if the IO APIC supports sending EOIs.
     *
     * @return True if sendinf EOIs is supported
     */
    [[nodiscard]] static bool hasEOIRegister();

    // NOTE: This only initializes the IO APIC at the default physical address
    // TODO: If there are multiple IO APICs the MMIO addresses have to be read from MP/ACPI tables
    /**
     * Initialize the IO APIC.
     *
     * Must not be called with enabled interrupts.
     */
    static void init();

    /**
     * Unmask an interrupt in the IO APIC.
     *
     * Must not be called with enabled interrupts.
     *
     * @param gsi The number of the interrupt to activated
     * @param destination Target CPU to send the interrupt to
     */
    static void allow(Interrupt gsi, uint8_t destination);

    /**
     * Unmask an interrupt in the IO APIC.
     * The interrupt will be sent to the current CPU's local APIC.
     *
     * Must not be called with enabled interrupts.
     *
     * @param gsi The number of the interrupt to activated
     */
    static void allow(Interrupt gsi);

    /**
     * Unmask an interrupt in the IO APIC.
     *
     * Must not be called with enabled interrupts.
     *
     * @param irq The number of the interrupt to activated
     * @param destination Target CPU to send the interrupt to
     */
    static void allow(Pic::Interrupt irq, uint8_t destination); // Destination means CPU

    /**
     * Unmask an interrupt in the IO APIC.
     * The interrupt will be sent to the current CPU's local APIC.
     *
     * Must not be called with enabled interrupts.
     *
     * @param irq The number of the interrupt to activated
     */
    static void allow(Pic::Interrupt irq);

    /**
     * Mask an interrupt in the local APIC.
     *
     * Must not be called with enabled interrupts.
     *
     * @param gsi The number of the interrupt to deactivate
     */
    static void forbid(Interrupt gsi);

    /**
     * Mask an interrupt in the local APIC.
     *
     * Must not be called with enabled interrupts.
     *
     * @param irq The number of the interrupt to deactivate
     */
    static void forbid(Pic::Interrupt irq);

    /**
     * Get the state of this interrupt - whether it is masked out or not.
     *
     * @param gsi The IO APIC GSI
     * @return True, if the interrupt is disabled
     */
    static bool status(Interrupt gsi);

    /**
     * Send an end of interrupt signal to the IO APIC.
     * Only compatible with IO APIC version >= 0x20.
     * (https://github.com/torvalds/linux/blob/master/arch/x86/kernel/apic/io_apic.c#L470)
     * (Intel ICH5 Datasheet Chapter 9.5.5)
     *
     * @param interrupt The vector number of the interrupt that is marked as completed
     */
    static void sendEndOfInterrupt(Kernel::InterruptDispatcher::Interrupt interrupt);

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

    enum class Delivery_Mode : uint8_t {
        FIXED = 0,
        LOWPRIO = 1,
        SMI = 0b10,
        NMI = 0b100,
        INIT = 0b101,
        EXTINT = 0b111
    };
    enum class Destination_Mode : uint8_t {
        PHYSICAL = 0, LOGICAL = 1
    };
    enum class Delivery_Status : uint8_t {
        IDLE = 0, PENDING = 1
    };
    enum class Pin_Polarity : uint8_t {
        HIGH = 0, LOW = 1
    };
    enum class Trigger_Mode : uint8_t {
        EDGE = 0, LEVEL = 1
    };
    typedef struct {
        Kernel::InterruptDispatcher::Interrupt slot;
        Delivery_Mode deliveryMode;
        Destination_Mode destinationMode;
        Delivery_Status deliveryStatus;
        Pin_Polarity pinPolarity;
        Trigger_Mode triggerMode;
        bool isMasked;
        uint8_t destination;
    } REDTBL_Entry;

private:
    // TODO: Under what circumstances is this different from 24? Every datasheet lists 24 REDTBL entries...
    /**
     * Determine the number of GSIs this IO APIC supports.
     *
     * @return The number of supported GSIs - 1
     */
    [[nodiscard]] static uint8_t getMaxREDTBLEntry();

    // NOTE: This only works for the IO APIC at the default physical address
    // NOTE: If there are multiple IO APICs the MMIO addresses have to be read from MP/ACPI tables
    /**
     * Read a value from an indirect IO APIC register.
     *
     * Must not be called with enabled interrupts.
     *
     * @param reg The register (offset) to read from (same as IoApic::Indirect_Register)
     * @return The value contained in the specified register
     */
    [[nodiscard]] static uint32_t readDoubleWord(uint8_t reg);

    // NOTE: This only works for the IO APIC at the default physical address
    // NOTE: If there are multiple IO APICs the MMIO addresses have to be read from MP/ACPI tables
    /**
     * Write a value to an indirect IO APIC register.
     *
     * Must not be called with enabled interrupts.
     *
     * @param reg The register (offset) to write to (same as IoApic::Indirect_Register)
     * @param val The value to write
     */
    static void writeDoubleWord(uint8_t reg, uint32_t val);

    /**
     * Read an entry from the redirection table.
     *
     * Must not be called with enabled interrupts.
     *
     * @param entry The number of the entry to read (same as IoApic::Interrupt)
     * @return The redirection table entry
     */
    [[nodiscard]] static REDTBL_Entry readREDTBL(uint8_t gsi);

    /**
     * Write an entry to the redirection table.
     *
     * Must not be called with enabled interrupts.
     *
     * @param entry The number of the entry to write (same as IoApic::Interrupt)
     * @param entry The redirection table entry value
     */
    static void writeREDTBL(uint8_t gsi, REDTBL_Entry entry);

    /**
     * Return a corresponding IO APIC GSI for a legacy PIC IRQ.
     *
     * @param irq PIC IRQ to map
     * @return Corresponding IO APIC GSI
     */
    static Interrupt getIrqToGsiMapping(Pic::Interrupt irq);

    /**
     * Return a corresponding vector number for an IO APIC GSI.
     *
     * @param gsi IO APIC GSI to map
     * @return Corresponding vector number
     */
    static Kernel::InterruptDispatcher::Interrupt getGsiToSlotMapping(Interrupt gsi);

    /**
     * Marks every GSI in the redirection table as edge-triggered, active high, masked,
     * physical destination mode to local APIC of BSP and fixed delivery mode.
     * Vector numbers are set to InterruptDispatcher equivalent vectors.
     *
     * Must not be called with enabled interrupts.
     */
    static void initREDTBL();

    // TODO: Functions to write REDTBL more easily
    // TODO: Just use a function that reads REDTBL and returns a struct (no bitfields) and the same for writing

#if HHUOS_IOAPIC_ENABLE_DEBUG == 1

    static void logDebugDump();

#endif

private:
    static bool initialized;
    static uint8_t version;
    static uint8_t maxREDTBLEntry;
    static uint32_t baseVirtAddress;

    // TODO: Crosscheck references to the original IO APIC Datasheet with Intel ICH5 Datasheet

    // TODO: Byte 2 is determined by the APIC Base Address Relocation Register if IO APIC is located in PIIX3 bridge
    //       The location could also be parsed from MP/ACPI tables
    // MMIO, IOAPIC Datasheet Chapter 3.0 Table 1
    static const constexpr uint32_t IOAPIC_BASE_DEFAULT_PHYS_ADDRESS = 0xFEC00000;

    static Kernel::Logger log;
};

}

#endif
