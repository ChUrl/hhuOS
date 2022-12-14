#ifndef HHUOS_GLOBALSYSTEMINTERRUPT_H
#define HHUOS_GLOBALSYSTEMINTERRUPT_H

namespace Device {

#include <cstdint>
#include "kernel/interrupt/InterruptDispatcher.h"

/**
 * Global system interrupts abstract the hardware interrupt pins from the software.
 * When the system is running in PIC mode there are only 16 valid GSIs (0 - 15).
 * Supports conversion from/to vector numbers.
 */
class GlobalSystemInterrupt {
public:
    enum Gsi : uint8_t {
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
        MOUSE = 0x0C,
        FPU = 0x0D,
        PRIMARY_ATA = 0x0E,
        SECONDARY_ATA = 0x0F,

        // Other GSIs, currently none are used
    };

public:
    GlobalSystemInterrupt() = default;

    ~GlobalSystemInterrupt() = default;

    constexpr GlobalSystemInterrupt(Gsi gsi); // NOLINT(google-explicit-constructor)

    constexpr explicit GlobalSystemInterrupt(uint8_t gsi);

    /**
     * Convert an interrupt vector number to a GlobalSystemInterrupt.
     * GSIs are mapped to vector numbers 1:1 but translated by 32 (NOT influenced by IO APIC remappings!).
     */
    constexpr explicit GlobalSystemInterrupt(Kernel::InterruptDispatcher::Interrupt vector);

    constexpr explicit operator uint8_t() const;

    /**
     * Convert the GlobalSystemInterrupt to an interrupt vector number.
     * GSIs are mapped to vector numbers 1:1 but translated by 32 (NOT influenced by IO APIC remappings!).
     */
    constexpr explicit operator Kernel::InterruptDispatcher::Interrupt() const;

    constexpr bool operator==(GlobalSystemInterrupt other) const;

    constexpr bool operator!=(GlobalSystemInterrupt other) const;

    constexpr bool operator>(GlobalSystemInterrupt other) const;

    constexpr bool operator>=(GlobalSystemInterrupt other) const;

    constexpr bool operator<(GlobalSystemInterrupt other) const;

    constexpr bool operator<=(GlobalSystemInterrupt other) const;

    constexpr GlobalSystemInterrupt &operator++();

    constexpr GlobalSystemInterrupt operator+(uint8_t other) const;

private:
    uint8_t gsi;
};

constexpr GlobalSystemInterrupt::GlobalSystemInterrupt(Gsi gsi) : gsi(gsi) {}

constexpr GlobalSystemInterrupt::GlobalSystemInterrupt(uint8_t gsi) : gsi(gsi) {}

constexpr GlobalSystemInterrupt::GlobalSystemInterrupt(Kernel::InterruptDispatcher::Interrupt vector)
        : gsi(vector - Kernel::InterruptDispatcher::PIT) {}

constexpr GlobalSystemInterrupt::operator uint8_t() const {
    return gsi;
}

constexpr GlobalSystemInterrupt::operator Kernel::InterruptDispatcher::Interrupt() const {
    return static_cast<Kernel::InterruptDispatcher::Interrupt>(gsi + Kernel::InterruptDispatcher::PIT);
}

constexpr bool GlobalSystemInterrupt::operator==(const GlobalSystemInterrupt other) const {
    return gsi == other.gsi;
}

constexpr bool GlobalSystemInterrupt::operator!=(const GlobalSystemInterrupt other) const {
    return gsi != other.gsi;
}

constexpr bool GlobalSystemInterrupt::operator>(const GlobalSystemInterrupt other) const {
    return gsi > other.gsi;
}

constexpr bool GlobalSystemInterrupt::operator>=(const GlobalSystemInterrupt other) const {
    return gsi >= other.gsi;
}

constexpr bool GlobalSystemInterrupt::operator<(const GlobalSystemInterrupt other) const {
    return gsi < other.gsi;
}

constexpr bool GlobalSystemInterrupt::operator<=(const GlobalSystemInterrupt other) const {
    return gsi <= other.gsi;
}

constexpr GlobalSystemInterrupt &GlobalSystemInterrupt::operator++() {
    ++gsi;
    return *this;
}

constexpr GlobalSystemInterrupt GlobalSystemInterrupt::operator+(uint8_t other) const {
    return GlobalSystemInterrupt(gsi + other);
}

}

#endif //HHUOS_GLOBALSYSTEMINTERRUPT_H
