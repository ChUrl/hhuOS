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

#include "Pic.h"

namespace Device {

void Pic::allow(InterruptRequest interruptSource) {
    auto &port = getDataPort(interruptSource);
    uint8_t mask = getMask(interruptSource);

    port.writeByte(port.readByte() & ~mask);
}

void Pic::forbid(InterruptRequest interruptSource) {
    auto &port = getDataPort(interruptSource);
    uint8_t mask = getMask(interruptSource);

    port.writeByte(port.readByte() | mask);
}

bool Pic::status(InterruptRequest interruptSource) {
    const IoPort &port = getDataPort(interruptSource);
    uint8_t mask = getMask(interruptSource);

    return port.readByte() & mask;
}

void Pic::sendEndOfInterrupt(InterruptRequest interruptSource) {
    if (interruptSource >= InterruptRequest::RTC) {
        slaveCommandPort.writeByte(EOI);
    }

    masterCommandPort.writeByte(EOI);
}

const IoPort &Pic::getDataPort(InterruptRequest interruptSource) {
    if (interruptSource >= InterruptRequest::RTC) {
        return slaveDataPort;
    }

    return masterDataPort;
}

uint8_t Pic::getMask(InterruptRequest interruptSource) {
    if (interruptSource >= InterruptRequest::RTC) {
        return (uint8_t) (1 << ((uint8_t) interruptSource - 8));
    }

    return (uint8_t) (1 << (uint8_t) interruptSource);
}

bool Pic::isSpurious(InterruptRequest interruptSource) {
    if (interruptSource == InterruptRequest::LPT1) {
        masterCommandPort.writeByte(READ_ISR);
        return (masterCommandPort.readByte() & SPURIOUS_INTERRUPT) == 0;
    }

    if (interruptSource == InterruptRequest::SECONDARY_ATA) {
        slaveCommandPort.writeByte(READ_ISR);
        if ((slaveCommandPort.readByte() & SPURIOUS_INTERRUPT) == 0) {
            sendEndOfInterrupt(InterruptRequest::CASCADE);
            return true;
        }
    }

    return false;
}

} // namespace Device