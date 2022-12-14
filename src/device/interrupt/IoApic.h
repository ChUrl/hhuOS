#ifndef __IOAPIC_include__
#define __IOAPIC_include__

#include <cstdint>
#include "ApicRegisterInterface.h"
#include "InterruptModel.h"
#include "kernel/interrupt/InterruptDispatcher.h"
#include "kernel/log/Logger.h"

namespace Device {

// TODO: Crosscheck references to the original IO APIC Datasheet with Intel ICH5 Datasheet

// TODO: Do IO APIC GSIs have to be continuous (could there be gaps between IO APICs)?

// TODO: Find the chapter where this is described in some manual (the range is mentioned in ICH5,
//       but I remember to have seen it somewhere else aswell...)
// NOTE: Valid IO APIC REDTBL vectors range between 0x10 and 0xFE

// NOTE: SAPIC/IO SAPIC is not supported

class IoApic {
public:
    IoApic() = delete; // Static class

    IoApic(const IoApic &copy) = delete;

    IoApic &operator=(const IoApic &copy) = delete;

    IoApic(IoApic &&move) = delete;

    IoApic &operator=(IoApic &&move) = delete;

    ~IoApic() = delete; // Static class


    static bool isInitialized();

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
    static void allow(GlobalSystemInterrupt gsi);

    /**
     * Mask an interrupt in the local APIC.
     *
     * Must not be called with enabled interrupts.
     *
     * @param gsi The number of the interrupt to deactivate
     */
    static void forbid(GlobalSystemInterrupt gsi);

    /**
     * Get the state of this interrupt - whether it is masked out or not.
     *
     * @param gsi The IO APIC GSI
     * @return True, if the interrupt is disabled
     */
    static bool status(GlobalSystemInterrupt gsi);

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

private:
    /**
     * Throws an exception if the IO APIC's MMIO region hasn't been initialized.
     */
    static void verifyMMIO(IoApicInformation *ioapic);

    /**
     * Throws an exception if the supplied GSI is handled by a different IO APIC than the supplied.
     */
    static void verifyGSI(IoApicInformation *ioapic, GlobalSystemInterrupt gsi);

    /**
     * Initialize a single IO APIC.
     *
     * @param id The ID of the IO APIC to initialize
     */
    static void initializeController(IoApicInformation *ioapic);

    /**
     * Allocate the memory region used to access a IO APIC's registers.
     */
    static void initializeMMIORegion(IoApicInformation *ioapic);

    /**
     * Marks every entry in the redirection table as edge-triggered, active high, masked,
     * physical destination mode to local APIC of the current CPU and fixed delivery mode.
     * Sets vector numbers to corresponding InterruptDispatcher::Interrupt.
     *
     * Must not be called with enabled interrupts.
     */
    static void initializeREDTBL(IoApicInformation *ioapic);

    // NOTE: Reading and writing IO APIC's registers.
    // NOTE: Parses the read/written value to/from types from ApicRegisterInterface.h
    // NOTE: Affects the registers of the passed IO APIC

    [[nodiscard]] static uint32_t readDoubleWord(IoApicInformation*ioapic, uint8_t reg);

    static void writeDoubleWord(IoApicInformation *ioapic, uint8_t reg, uint32_t val);

    [[nodiscard]] static REDTBLEntry readREDTBL(IoApicInformation *ioapic, GlobalSystemInterrupt gsi);

    static void writeREDTBL(IoApicInformation *ioapic, GlobalSystemInterrupt gsi, REDTBLEntry redtbl);

private:
    static bool initialized;
    static Kernel::Logger log;
};

}

#endif
