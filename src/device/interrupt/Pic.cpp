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

void Pic::allow(GlobalSystemInterrupt gsi) {
    auto &port = getDataPort(gsi);
    uint8_t mask = getMask(gsi);

    port.writeByte(port.readByte() & ~mask);
}

void Pic::forbid(GlobalSystemInterrupt gsi) {
    auto &port = getDataPort(gsi);
    uint8_t mask = getMask(gsi);

    port.writeByte(port.readByte() | mask);
}

bool Pic::status(GlobalSystemInterrupt gsi) {
    const IoPort &port = getDataPort(gsi);
    uint8_t mask = getMask(gsi);

    return port.readByte() & mask;
}

void Pic::sendEndOfInterrupt(GlobalSystemInterrupt gsi) {
    if (gsi >= GlobalSystemInterrupt::RTC) {
        slaveCommandPort.writeByte(EOI);
    }

    masterCommandPort.writeByte(EOI);
}

const IoPort &Pic::getDataPort(GlobalSystemInterrupt gsi) {
    if (gsi >= GlobalSystemInterrupt::RTC) {
        return slaveDataPort;
    }

    return masterDataPort;
}

uint8_t Pic::getMask(GlobalSystemInterrupt gsi) {
    if (gsi >= GlobalSystemInterrupt::RTC) {
        return (uint8_t) (1 << ((uint8_t) gsi - 8));
    }

    return (uint8_t) (1 << (uint8_t) gsi);
}

bool Pic::isSpurious(GlobalSystemInterrupt gsi) {
    if (gsi == GlobalSystemInterrupt::LPT1) {
        masterCommandPort.writeByte(READ_ISR);
        return (masterCommandPort.readByte() & SPURIOUS_INTERRUPT) == 0;
    } else if (gsi == GlobalSystemInterrupt::SECONDARY_ATA) {
        slaveCommandPort.writeByte(READ_ISR);
        if ((slaveCommandPort.readByte() & SPURIOUS_INTERRUPT) == 0) {
            sendEndOfInterrupt(GlobalSystemInterrupt::CASCADE);
            return true;
        }
    }

    return false;
}

}