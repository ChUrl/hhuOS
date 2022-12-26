#include "ErrorInterruptHandler.h"
#include "kernel/service/InterruptService.h"
#include "kernel/system/System.h"
#include "LocalApic.h"

namespace Device {

void ErrorInterruptHandler::plugin() {
    auto &interruptService = Kernel::System::getService<Kernel::InterruptService>();
    interruptService.assignInterrupt(Kernel::InterruptDispatcher::ERROR, *this);
    LocalApic::allow(LocalApic::ERROR); // TODO: This in interruptservice
}

void ErrorInterruptHandler::trigger(const Kernel::InterruptFrame &frame) {
    LocalApic::handleErrors();
}

}
