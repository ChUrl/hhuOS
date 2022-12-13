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
#include "device/interrupt/LApic.h"
#include "device/interrupt/IoApic.h"
#include "device/interrupt/InterruptArchitecture.h"
#include "device/cpu/Cpu.h"

namespace Kernel {
class InterruptHandler;
struct InterruptFrame;

void InterruptService::assignInterrupt(InterruptDispatcher::Interrupt slot, InterruptHandler &handler) {
    dispatcher.assign(slot, handler);
}

void InterruptService::dispatchInterrupt(const InterruptFrame &frame) {
    dispatcher.dispatch(frame);
}

void InterruptService::allowHardwareInterrupt(Device::Pic::Interrupt interrupt) {
    if (Device::IoApic::isInitialized()) {
        Device::IoApic::allow(interrupt); // TODO: Needs to be synchronized as LVT is written?
    } else {
        pic.allow(interrupt);
    }
}

void InterruptService::forbidHardwareInterrupt(Device::Pic::Interrupt interrupt) {
    if (Device::IoApic::isInitialized()) {
        Device::IoApic::forbid(interrupt); // TODO: Needs to be synchronized as LVT is written?
    } else {
        pic.forbid(interrupt);
    }
}

void InterruptService::sendEndOfInterrupt(InterruptDispatcher::Interrupt interrupt) {
    if (Device::InterruptArchitecture::hasApic()) {
        // TODO: Exclude SMI, Init, Startup, Init-Deassert somehow?
        // TODO: Only disable NMI pin, not both LINTs
        // EOI for local interrupts, NMI doesn't have to be EOI'd
        if (interrupt >= InterruptDispatcher::CMCI && interrupt <= InterruptDispatcher::ERROR
            && interrupt != InterruptDispatcher::LINT0 && interrupt != InterruptDispatcher::LINT1) {
            Device::LApic::sendEndOfInterrupt();
        }

        // TODO: Exclude SMI, Init, Startup, Init-Deassert somehow?
        // TODO: <= IO8 depends on how many GSIs are supported by the system
        if (interrupt >= InterruptDispatcher::PIT
        && interrupt <= InterruptDispatcher::PIT + Device::InterruptArchitecture::getGlobalGsiMax()) {
            Device::LApic::sendEndOfInterrupt(); // TODO: Do IO APIC GSIs need local APIC EOI?
            Device::IoApic::sendEndOfInterrupt(interrupt);
        }
    }

    else if (interrupt >= InterruptDispatcher::PIT && interrupt <= InterruptDispatcher::SECONDARY_ATA) {
        pic.sendEndOfInterrupt(static_cast<Device::Pic::Interrupt>(interrupt - InterruptDispatcher::PIT));
    }
}

bool InterruptService::checkSpuriousInterrupt(InterruptDispatcher::Interrupt interrupt) {
    if (Device::InterruptArchitecture::hasApic()) {
        // NOTE: The APIC always reports vector number set in the SVR for spurious interrupts (0xFF)
        return interrupt == InterruptDispatcher::SPURIOUS;
    }

    // NOTE: When using the PIC the spurious interrupt has the lowest priority of the corresponding chip (7 or 15)
    else if (interrupt != InterruptDispatcher::LPT1 && interrupt != InterruptDispatcher::SECONDARY_ATA) {
        return false;
    }

    // NOTE: If an interrupt (number 7 or 15) happens (PIC) but the interrupt flag in ISR is not set, it is spurious
    return pic.isSpurious(static_cast<Device::Pic::Interrupt>(interrupt - InterruptDispatcher::PIT));
}

}