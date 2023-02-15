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

#include "InterruptService.h"
#include "device/interrupt/apic/Apic.h"

namespace Kernel {
class InterruptHandler;
struct InterruptFrame;

void InterruptService::assignInterrupt(InterruptVector slot, InterruptHandler &handler) {
    dispatcher.assign(slot, handler);
}

void InterruptService::dispatchInterrupt(const InterruptFrame &frame) {
    dispatcher.dispatch(frame);
}

void InterruptService::allowHardwareInterrupt(Device::InterruptRequest interrupt) {
    if (Device::Apic::isEnabled()) {
        Device::Apic::allow(interrupt);
    } else {
        pic.allow(interrupt);
    }
}

void InterruptService::forbidHardwareInterrupt(Device::InterruptRequest interrupt) {
    if (Device::Apic::isEnabled()) {
        Device::Apic::forbid(interrupt);
    } else {
        pic.forbid(interrupt);
    }
}

void InterruptService::sendEndOfInterrupt(InterruptVector interrupt) {
    if (Device::Apic::isEnabled()) {
        Device::Apic::sendEndOfInterrupt(interrupt);
    } else if (interrupt - 32 <= Device::InterruptRequest::SECONDARY_ATA) {
        pic.sendEndOfInterrupt(static_cast<Device::InterruptRequest>(interrupt - 32));
    }
}

bool InterruptService::checkSpuriousInterrupt(InterruptVector interrupt) {
    if (Device::Apic::isEnabled()) {
        return interrupt == InterruptVector::SPURIOUS;
    }

    if (interrupt != InterruptVector::LPT1 && interrupt != InterruptVector::SECONDARY_ATA) {
        return false;
    }

    return pic.isSpurious(static_cast<Device::InterruptRequest>(interrupt - 32));
}

}