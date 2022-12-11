#ifndef __IOAPIC_include__
#define __IOAPIC_include__

#include <cstdint>
#include "ApicRegisterInterface.h"
#include "kernel/log/Logger.h"
#include "kernel/interrupt/InterruptDispatcher.h"
#include "device/interrupt/Pic.h"

#define HHUOS_IOAPIC_ENABLE 1
#define HHUOS_IOAPIC_ENABLE_DEBUG 1

namespace Device {

// TODO: In order to detect IO APIC(s), MP/ACPI tables have to be parsed.
//       Currently I just assume that one IO APIC exists, which is true for QEMU with +apic
//       This code doesn't account for multiple IO APICs at all

// TODO: Add "unstatic" members to handle multiple IO APICS through instances?

// TODO: Crosscheck references to the original IO APIC Datasheet with Intel ICH5 Datasheet

class IoApic {
public:
    // TODO: Remove this, set the mapping in an array in the platformConfiguration
    // TODO: Unify the PIC/IO APIC interrupt structs
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

        // TODO: Depends on how many GSIs the system supports
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
    IoApic() = delete; // Static class

    IoApic(const IoApic &copy) = delete;

    IoApic &operator=(const IoApic &copy) = delete;

    IoApic(IoApic &&move) = delete;

    IoApic &operator=(IoApic &&move) = delete;

    ~IoApic() = delete; // Static class


    // TODO: Does not account for multiple IO APICs
    static bool isInitialized();

    // NOTE: This only initializes the IO APIC at the default physical address
    // TODO: If there are multiple IO APICs the MMIO addresses have to be read from MP/ACPI tables
    /**
     * Initialize the IO APIC.
     *
     * Must not be called with enabled interrupts.
     */
    static void initialize();

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
     * @param vector The vector number of the interrupt that is marked as completed
     */
    static void sendEndOfInterrupt(Kernel::InterruptDispatcher::Interrupt vector);

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

    typedef struct IoApicConfiguration {
        uint8_t id;
        uint32_t address;
        uint32_t virtAddress; // Needs to be set when initializing MMIO for this IO APIC
        uint32_t gsiBase; // GSI where IO APIC interrupt inputs start
        uint32_t redtblEntries; // Needs to be set after MMIO is available

        bool operator!=(const IoApicConfiguration &other) const {
            return id != other.id || address != other.address || virtAddress != other.virtAddress
            || gsiBase != other.gsiBase || redtblEntries != other.redtblEntries;
        }
    } IoApicConfiguration;

    typedef struct InterruptOverride {
        uint8_t bus;
        Pic::Interrupt source;
        uint32_t gsi;
        REDTBLPinPolarity polarity;
        REDTBLTriggerMode triggerMode;

        bool operator!=(const InterruptOverride &other) const {
            return bus != other.bus || source != other.source || gsi != other.gsi || polarity != other.polarity
            || triggerMode != other.triggerMode;
        }
    } InterruptOverride;

    typedef struct NMIConfiguration {
        REDTBLPinPolarity polarity;
        REDTBLTriggerMode triggerMode;
        uint32_t gsi;

        bool operator!=(const NMIConfiguration &other) const {
            return polarity != other.polarity || triggerMode != other.triggerMode || gsi != other.gsi;
        }
    } NMIConfiguration;

    typedef struct IoPlatformConfiguration {
        uint8_t version; // Needs to be set after MMIO is available
        bool needsCompatibilityEOI; // If IoApic has no EOI register, needs to be set after MMIO is available
        uint32_t irqToGsiMappings[16]; // GSIs for PIC IRQs (IRQ is the index)
        uint8_t gsiToIrqMappings[16]; // Inverse of irqToGsiMappings
        Util::Data::ArrayList<IoApicConfiguration *> ioapics;
        Util::Data::ArrayList<InterruptOverride *> irqOverrides;
        Util::Data::ArrayList<NMIConfiguration *> ionmis;
    } IoPlatformConfiguration;

private:
    /**
     * Reads information influencing IO APIC initialization from ACPI tables.
     */
    static void initializePlatformConfiguration();

    /**
     * Get the configuration structure of the IO APIC that handles a specific GSI.
     *
     * @param gsi The GSI that's handled by the searched IO APIC
     * @return The IO APIC configuration
     */
    static IoApicConfiguration *getIoApicConfiguration(Interrupt gsi);
    static IoApicConfiguration *getIoApicConfiguration(Pic::Interrupt irq);
    static IoApicConfiguration *getIoApicConfiguration(Kernel::InterruptDispatcher::Interrupt vector);

    /**
     * Get the configuration structure that describes the NMI of a specific IO APIC.
     *
     * @param ioapic The IO APIC which NMI configuration is searched
     * @return The NMI configuration
     */
    static NMIConfiguration *getNMIConfiguration(IoApicConfiguration *ioapic);

    /**
     * Initialize a single IO APIC.
     *
     * @param id The ID of the IO APIC to initialize
     */
    static void initializeController(IoApicConfiguration *ioapic);

    /**
     * Marks every entry in the redirection table as edge-triggered, active high, masked,
     * physical destination mode to local APIC of the current CPU and fixed delivery mode.
     * Sets vector numbers to corresponding InterruptDispatcher::Interrupt.
     *
     * Must not be called with enabled interrupts.
     */
    static void initializeREDTBL(IoApicConfiguration *ioapic);

    // NOTE: Reading and writing IO APIC's registers.
    // NOTE: Parses the read/written value to/from types from ApicRegisterInterface.h

    [[nodiscard]] static uint32_t readDoubleWord(IoApicConfiguration *ioapic, uint8_t reg);

    static void writeDoubleWord(IoApicConfiguration *ioapic, uint8_t reg, uint32_t val);

    [[nodiscard]] static REDTBLEntry readREDTBL(IoApicConfiguration *ioapic, uint8_t gsi);

    static void writeREDTBL(IoApicConfiguration *ioapic, uint8_t gsi, REDTBLEntry redtbl);

#if HHUOS_IOAPIC_ENABLE_DEBUG == 1

    static void logDebugDump();

#endif

private:
    static bool initialized;

    // NOTE: I chose to keep this and the contained structs stack allocated
    // NOTE: because I didn't want the hassle of having heap-allocated static objects.
    // NOTE: The ArrayLists are only allocated once elements are added.
    static IoPlatformConfiguration platformConfiguration;

    static Kernel::Logger log;
};

}

#endif
