/*
 * Copyright (C) 2018-2023 Heinrich-Heine-Universitaet Duesseldorf,
 * Institute of Computer Science, Department Operating Systems
 * Burak Akguel, Christian Gesse, Fabian Ruhland, Filip Krakowski, Michael Schoettner
 *
 * This program is free software: you can redistribute it and/or modify it under the terms of the GNU General Public
 * License as published by the Free Software Foundation, either version 3 of the License, or (at your option) any
 * later version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied
 * warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for more
 * details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>
 */

#ifndef __InterruptDispatcher_include__
#define __InterruptDispatcher_include__

#include <cstdint>

#include "lib/util/async/Atomic.h"

namespace Util {

template <typename T> class List;

}  // namespace Util

namespace Kernel {
class InterruptHandler;
struct InterruptFrame;

/**
 * InterruptDispatcher - responsible for registering and dispatching interrupts to the
 * corresponding handlers.
 *
 * @author Michael Schoettner, Filip Krakowski, Fabian Ruhland, Burak Akguel, Christian Gesse
 * @date HHU, 2018
 */
class InterruptDispatcher {

public:
    // NOTE: These don't correspong to GSI order but to IRQ order (that's why ACPI Interrupt Source Override exists)
    enum Interrupt : uint8_t {
        DEVICE_NOT_AVAILABLE = 7,
        PAGEFAULT = 14,

        // PIC interrupts
        PIT = 32,
        KEYBOARD = 33,
        // NOTE: CASCADE
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

        // TODO: Just as example, depends on how many GSIs are supported by the system
        // NOTE: There should be no gap here as I calculate the vector number by adding 32 to GSI number
        // IoApic interrupts, added to support at least 24
        IO1 = 48,
        IO2 = 49,
        IO3 = 50,
        IO4 = 51,
        IO5 = 52,
        IO6 = 53,
        IO7 = 54,
        IO8 = 55,

        SYSTEM_CALL = 0x86,

        // Local APIC interrupts
        CMCI = 0xF8,
        APICTIMER = 0xF9,
        THERMAL = 0xFA,
        PERFORMANCE = 0xFB,
        LINT0 = 0xFC,
        LINT1 = 0xFD,
        ERROR = 0xFE,
        SPURIOUS = 0xFF
    };

    /**
     * Default Constructor.
     */
    InterruptDispatcher();

    InterruptDispatcher(const InterruptDispatcher &other) = delete;

    InterruptDispatcher& operator=(const InterruptDispatcher &other) = delete;

    ~InterruptDispatcher() = default;

    /**
     * Register an interrupt handler to an interrupt number.
     *
     * @param slot Interrupt number for this handler
     * @param isr Pointer to the handler itself
     */
    void assign(uint8_t slot, InterruptHandler &isr);

    /**
     * Dispatched the interrupt to all registered interrupt handlers.
     *
     * @param frame The interrupt frame
     */
    void dispatch(const InterruptFrame &frame);

    [[nodiscard]] uint32_t getInterruptDepth() const;

private:

    static bool isUnrecoverableException(Interrupt slot);

    uint32_t interruptDepth = 0;
    uint32_t spuriousCounter = 0;
    Util::Async::Atomic<uint32_t> interruptDepthWrapper = Util::Async::Atomic<uint32_t>(interruptDepth);
    Util::Async::Atomic<uint32_t> spuriousCounterWrapper = Util::Async::Atomic<uint32_t>(spuriousCounter);

    Util::List<InterruptHandler*>** handler;

};

}

#endif