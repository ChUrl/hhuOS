#ifndef HHUOS_INTERRUPTREQUEST_H
#define HHUOS_INTERRUPTREQUEST_H

#include <cstdint>

namespace Device {

/**
 * @brief This allows addressing an interrupt line by device name.
 *
 * InterruptRequests map 1:1 to PIC interrupt inputs, and to system InterruptVectors translated by 32.
 * They do not translate 1:1 to the IO APIC's GlobalSystemInterrupts.
 */
enum InterruptRequest : uint8_t {
    // PIC compatible devices
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

    // Other devices
};

}

#endif //HHUOS_INTERRUPTREQUEST_H
