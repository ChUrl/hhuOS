#ifndef HHUOS_GLOBALSYSTEMINTERRUPT_H
#define HHUOS_GLOBALSYSTEMINTERRUPT_H

#include <cstdint>
#include "kernel/interrupt/InterruptDispatcher.h"

// TODO: Move to kernel/interrupt?
// TODO: Or move to ACPI? Since the concept of GSIs is part of the ACPI specification?

// NOTE: This is an experiment, I don't know if it makes sense yet
// NOTE: I think this should compile down to just one byte per GSI?

/*
 * This class implements the ACPI concept of global system interrupts.
 * It is basically a fancy wrapped uint8_t.
 * The goal is to abstract the hardware interrupts from different interrupt controller implementations.
 *
 * The 8295 PIC is usually used in a master-slave configuration with 2 PICs, so a PC/AT compatible system
 * always support at least 16 hardware interrupts (including the PIC cascade).
 * Modern systems using the APIC interrupt architecture can support a variable amount (and significantly more).
 * To decouple the firmware/OS from the physical interrupt architecture, ACPI introduces "Global System Interrupts".
 * GSIs only abstract hardware interrupts (no exceptions/faults) and start at 0!
 * Because PC/AT compatibility is maintained, the first 16 (GSIs 0 - 15) global system interrupts are identity
 * mapped to the PIC hardware interrupts.
 * This introduces a problem, because the APIC architecture doesn't enforce how devices are physically wired
 * to the IO APIC's interrupt inputs.
 * To solve this, ACPI provides "Interrupt Source Overrides" in the MADT, which specify variances
 * between PIC and APIC hardware interrupt configurations.
 *
 * Technically the notion of "GSIs" only got introduced in some ACPI version after 1.0b,
 * but I see no problem with using it globally (even when ACPI/APIC is not available).
 */

namespace Device {

class GlobalSystemInterrupt {
    friend class IoApic;

public:
    enum Interrupt : uint8_t {
        // PIC compatible GSIs
        PIT = 0x00,
        KEYBOARD = 0x01,
        CASCADE = 0x02,
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

        // Additional GSIs
    };

public:
    GlobalSystemInterrupt() = default;
    ~GlobalSystemInterrupt() = default;

    explicit GlobalSystemInterrupt(uint8_t gsi); // From int because enums can be implicitly converted to int
    explicit GlobalSystemInterrupt(const Kernel::InterruptDispatcher::Interrupt &vector);

    operator Interrupt() const; // To enum because enums can be implicitly converted to int
    explicit operator Kernel::InterruptDispatcher::Interrupt() const;

    /**
     * Determine the interrupt input the GSI is assigned to.
     */
    uint8_t inti(); // INTI: Interrupt Input

private:
    static uint8_t gsiToIntiMappings[16]; // Inverse of irqToGsiMappings
    static Interrupt systemGsiMax; // Systemwide max gsi
    Interrupt gsi;
};

}

#endif //HHUOS_GLOBALSYSTEMINTERRUPT_H
