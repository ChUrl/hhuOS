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
#include "device/cpu/IoPort.h"

namespace Device {

void Pic::allow(InterruptSource interruptSource) {
    auto &port = getDataPort(interruptSource);
    uint8_t mask = getMask(interruptSource);

    port.writeByte(port.readByte() & ~mask);
}

void Pic::forbid(InterruptSource interruptSource) {
    auto &port = getDataPort(interruptSource);
    uint8_t mask = getMask(interruptSource);

    port.writeByte(port.readByte() | mask);
}

bool Pic::status(InterruptSource interruptSource) {
    const IoPort &port = getDataPort(interruptSource);
    uint8_t mask = getMask(interruptSource);

    return port.readByte() & mask;
}

void Pic::sendEndOfInterrupt(InterruptSource interruptSource) {
    if (interruptSource >= InterruptSource::RTC) {
        slaveCommandPort.writeByte(EOI);
    }

    masterCommandPort.writeByte(EOI);
}

const IoPort &Pic::getDataPort(InterruptSource interruptSource) {
    if (interruptSource >= InterruptSource::RTC) {
        return slaveDataPort;
    }

    return masterDataPort;
}

uint8_t Pic::getMask(InterruptSource interruptSource) {
    if (interruptSource >= InterruptSource::RTC) {
        return (uint8_t) (1 << ((uint8_t) interruptSource - 8));
    }

    return (uint8_t) (1 << (uint8_t) interruptSource);
}

bool Pic::isSpurious(InterruptSource interruptSource) {
    if (interruptSource == InterruptSource::LPT1) {
        masterCommandPort.writeByte(READ_ISR);
        return (masterCommandPort.readByte() & SPURIOUS_INTERRUPT) == 0;
    } else if (interruptSource == InterruptSource::SECONDARY_ATA) {
        slaveCommandPort.writeByte(READ_ISR);
        if ((slaveCommandPort.readByte() & SPURIOUS_INTERRUPT) == 0) {
            sendEndOfInterrupt(InterruptSource::CASCADE);
            return true;
        }
    }

    return false;
}

}