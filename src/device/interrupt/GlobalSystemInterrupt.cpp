#include "GlobalSystemInterrupt.h"

namespace Device {

// These values are always valid if the system runs with PIC interrupt architecture
// Thus ACPI is only required when running with APIC interrupt architecture
uint8_t GlobalSystemInterrupt::gsiToIntiMappings[16] = {0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15};
GlobalSystemInterrupt::Interrupt GlobalSystemInterrupt::systemGsiMax = GlobalSystemInterrupt::SECONDARY_ATA;

// ! Public member functions start here

// Construction
GlobalSystemInterrupt::GlobalSystemInterrupt(uint8_t gsi) : gsi(static_cast<Interrupt>(gsi)) {}
GlobalSystemInterrupt::GlobalSystemInterrupt(const Kernel::InterruptDispatcher::Interrupt &vector)
: GlobalSystemInterrupt(static_cast<Interrupt>(vector - Kernel::InterruptDispatcher::PIT)) {}

// Conversion
GlobalSystemInterrupt::operator Interrupt() const {
    return gsi;
}
GlobalSystemInterrupt::operator Kernel::InterruptDispatcher::Interrupt() const {
    return static_cast<Kernel::InterruptDispatcher::Interrupt>(gsi + Kernel::InterruptDispatcher::PIT);
}

// Hardware
uint8_t GlobalSystemInterrupt::inti() {
    if (gsi <= 15) {
        return gsiToIntiMappings[gsi]; // If PIC architecture is used this is the identity mapping
    }

    return gsi;
}

}
