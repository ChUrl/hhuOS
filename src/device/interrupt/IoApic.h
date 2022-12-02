#ifndef __IOAPIC_include__
#define __IOAPIC_include__

#include <cstdint>

namespace Device {

class IoApic {

public:
    /**
     * Default-Constructor.
     */
    IoApic() = default;

    /**
     * Copy Constructor.
     */
    IoApic(const IoApic &copy) = delete;

    /**
     * Assignment operator.
     */
    IoApic &operator=(const IoApic &other) = delete;

    /**
     * Destructor.
     */
    ~IoApic() = default;

private:

    // TODO: Determine base address (ACPI?)

    // Offsets
    static const constexpr uint8_t IOREGSEL = 0x00; // Register selector (MMIO)
    static const constexpr uint8_t IOWIN = 0x10; // Data register (MMIO)

    // Selector
    static const constexpr uint8_t IOAPICID = 0x00; // ID
    static const constexpr uint8_t IOAPICVER = 0x01; // Version
    static const constexpr uint8_t IOAPICARB = 0x02; // Arbitration ID
    static const constexpr uint8_t IOREDTBL = 0x10; // Redirection table base address
};

}

#endif
