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
#include "device/interrupt/LocalApic.h"
#include "device/interrupt/IoApic.h"

namespace Kernel {
class InterruptHandler;
struct InterruptFrame;

void InterruptService::assignInterrupt(InterruptDispatcher::Interrupt slot, InterruptHandler &handler) {
    dispatcher.assign(slot, handler);
}

void InterruptService::dispatchInterrupt(const InterruptFrame &frame) {
    dispatcher.dispatch(frame);
}

void InterruptService::allowHardwareInterrupt(Device::InterruptSource interruptSource) {
    if (Device::Apic::isInitialized()) {
        Device::Apic::allowExternalInterrupt(interruptSource);
    } else {
        pic.allow(interruptSource);
    }
}

void InterruptService::forbidHardwareInterrupt(Device::InterruptSource interruptSource) {
    if (Device::Apic::isInitialized()) {
        Device::Apic::forbidExternalInterrupt(interruptSource);
    } else {
        pic.forbid(interruptSource);
    }
}

void InterruptService::sendEndOfInterrupt(InterruptDispatcher::Interrupt interrupt) {
    if (Device::Apic::isInitialized()) {
        // TODO: local/IoApic should not accept InterruptVectors? Only GSIs...
        //       (decouple hardware from InterruptDispatcher)
        // TODO: NMI and other stuff (SMI, etc...) should be excluded.
        //       I think NMI and the others do not send a valid vector number (should send 0),
        //       if that is the case nothing has to be done here as 0 is not a local/external interrupt.
        //       This needs to be checked though (that the vectors are set correctly to 0)
        if (Device::Apic::isExternalInterrupt(interrupt)) {
            Device::Apic::sendExternalEndOfInterrupt(interrupt);
        } else if (Device::Apic::isLocalInterrupt(interrupt)) {
            Device::LocalApic::sendEndOfInterrupt();
        }
    }

    if (!Device::Apic::isInitialized() && interrupt - 32 <= Device::InterruptSource::SECONDARY_ATA) {
        pic.sendEndOfInterrupt(static_cast<Device::InterruptSource>(interrupt - 32));
    }
}

bool InterruptService::checkSpuriousInterrupt(InterruptDispatcher::Interrupt interrupt) {
    if (Device::Apic::isInitialized()) {
        return interrupt == InterruptDispatcher::SPURIOUS;
    }

    else if (interrupt != InterruptDispatcher::LPT1 && interrupt != InterruptDispatcher::SECONDARY_ATA) {
        return false;
    }

    return pic.isSpurious(static_cast<Device::InterruptSource>(interrupt - 32));
}

}