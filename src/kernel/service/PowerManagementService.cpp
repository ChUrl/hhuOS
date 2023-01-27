/*
 * Copyright (C) 2018-2022 Heinrich-Heine-Universitaet Duesseldorf,
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

#include "PowerManagementService.h"

#include <stdarg.h>

#include "kernel/system/SystemCall.h"
#include "lib/util/hardware/Machine.h"
#include "kernel/system/System.h"
#include "device/power/Machine.h"
#include "lib/util/base/System.h"

namespace Kernel {

PowerManagementService::PowerManagementService(Device::Machine *machine) : machine(*machine) {
    SystemCall::registerSystemCall(Util::System::SHUTDOWN, [](uint32_t paramCount, va_list arguments) -> Util::System::Result {
        if (paramCount < 1) {
            return Util::System::INVALID_ARGUMENT;
        }

        auto type = static_cast<Util::Hardware::Machine::ShutdownType>(va_arg(arguments, uint32_t));
        auto &powerManagementService = System::getService<PowerManagementService>();

        if (type == Util::Hardware::Machine::SHUTDOWN) {
            powerManagementService.shutdownMachine();
        } else if (type == Util::Hardware::Machine::REBOOT) {
            powerManagementService.rebootMachine();
        } else {
            return Util::System::INVALID_ARGUMENT;
        }

        // Should never be executed
        return Util::System::Result::ILLEGAL_STATE;
    });
}

PowerManagementService::~PowerManagementService() {
    delete &machine;
}

void PowerManagementService::shutdownMachine() {
    machine.shutdown();
}

void PowerManagementService::rebootMachine() {
    machine.reboot();
}

}