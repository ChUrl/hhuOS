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
#include "device/interrupt/Apic.h"
#include "device/interrupt/Pic.h"
#include "kernel/interrupt/InterruptVector.h"

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
    if (Device::Apic::isBspInitialized()) {
        Device::Apic::allowExternalInterrupt(interruptRequest);
    } else {
        Device::Pic::allow(interruptRequest);
    }
}

void InterruptService::forbidHardwareInterrupt(Device::InterruptRequest interruptRequest) {
    if (Device::Apic::isBspInitialized()) {
        Device::Apic::forbidExternalInterrupt(interruptRequest);
    } else {
        Device::Pic::forbid(interruptRequest);
    }
}

void InterruptService::sendEndOfInterrupt(InterruptVector interruptVector) {
    if (Device::Apic::isBspInitialized()) {
        // NMI, SMI, INIT, ExtINT and STARTUP don't receive EOI
        if (Device::Apic::isExternalInterrupt(interruptVector)) {
            Device::Apic::sendExternalEndOfInterrupt(interruptVector);
        } else if (Device::Apic::isLocalInterrupt(interruptVector) && interruptVector != InterruptVector::LINT1) {
            Device::Apic::sendLocalEndOfInterrupt();
        }
    }

    else if (interruptVector - 32 <= Device::InterruptRequest::SECONDARY_ATA) {
        Device::Pic::sendEndOfInterrupt(static_cast<Device::InterruptRequest>(interruptVector - 32));
    }
}

bool InterruptService::checkSpuriousInterrupt(InterruptVector interruptVector) {
    if (Device::Apic::isBspInitialized()) {
        return interruptVector == InterruptVector::SPURIOUS;
    }

    else if (interruptVector != InterruptVector::LPT1 && interruptVector != InterruptVector::SECONDARY_ATA) {
        return false;
    }

    return Device::Pic::isSpurious(static_cast<Device::InterruptRequest>(interruptVector - 32));
}

}