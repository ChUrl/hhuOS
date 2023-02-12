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

void InterruptService::allowHardwareInterrupt(Device::InterruptRequest interruptRequest) {
    if (Device::Apic::isInitialized()) {
        Device::Apic::allow(interruptRequest);
    } else {
        pic.allow(interruptRequest);
    }
}

void InterruptService::forbidHardwareInterrupt(Device::InterruptRequest interruptRequest) {
    if (Device::Apic::isInitialized()) {
        Device::Apic::forbid(interruptRequest);
    } else {
        pic.forbid(interruptRequest);
    }
}

void InterruptService::sendEndOfInterrupt(InterruptVector interruptVector) {
    if (Device::Apic::isInitialized()) {
        Device::Apic::sendEndOfInterrupt(interruptVector);
    } else if (interruptVector - 32 <= Device::InterruptRequest::SECONDARY_ATA) {
        pic.sendEndOfInterrupt(static_cast<Device::InterruptRequest>(interruptVector - 32));
    }
}

bool InterruptService::checkSpuriousInterrupt(InterruptVector interruptVector) {
    if (Device::Apic::isInitialized()) {
        return interruptVector == InterruptVector::SPURIOUS;
    }

    if (interruptVector != InterruptVector::LPT1 && interruptVector != InterruptVector::SECONDARY_ATA) {
        return false;
    }

    return pic.isSpurious(static_cast<Device::InterruptRequest>(interruptVector - 32));
}

}