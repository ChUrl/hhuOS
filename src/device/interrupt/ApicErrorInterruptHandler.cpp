#include "ApicErrorInterruptHandler.h"
#include "kernel/service/InterruptService.h"
#include "kernel/system/System.h"
#include "LocalApic.h"

namespace Device {

// TODO: Check how serenityOS does error handling

void ApicErrorInterruptHandler::plugin() {
    auto &interruptService = Kernel::System::getService<Kernel::InterruptService>();
    interruptService.assignInterrupt(Kernel::InterruptDispatcher::ERROR, *this);
    LocalApic::allow(LocalApic::ERROR);
}

void ApicErrorInterruptHandler::trigger(const Kernel::InterruptFrame &frame) {
    LocalApic::handleErrors();
}

}
