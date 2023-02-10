#ifndef HHUOS_INTERRUPTVECTOR_H
#define HHUOS_INTERRUPTVECTOR_H

#include <cstdint>

namespace Kernel {

enum InterruptVector : uint8_t {
    DEVICE_NOT_AVAILABLE = 7,
    PAGEFAULT = 14,

    // PC/AT compatible interrupts
    PIT = 32,
    KEYBOARD = 33,
    COM2 = 35,
    COM1 = 36,
    LPT2 = 37,
    FLOPPY = 38,
    LPT1 = 39,
    RTC = 40,
    FREE1 = 41,
    FREE2 = 42,
    FREE3 = 43,
    MOUSE = 44,
    FPU = 45,
    PRIMARY_ATA = 46,
    SECONDARY_ATA = 47,

    // Possibly some other interrupts supported by IO APICs

    SYSTEM_CALL = 0x86,

    // Local APIC interrupts (247 - 254)
    CMCI = 0xF8,
    APICTIMER = 0xF9,
    THERMAL = 0xFA,
    PERFORMANCE = 0xFB,
    LINT0 = 0xFC,
    LINT1 = 0xFD,
    ERROR = 0xFE,

    SPURIOUS = 0xFF
};

}


#endif //HHUOS_INTERRUPTVECTOR_H
