#include "ErrorInterruptHandler.h"
#include "kernel/service/InterruptService.h"
#include "kernel/system/System.h"
#include "LApic.h"

namespace Device {

void ErrorInterruptHandler::plugin() {
    auto &interruptService = Kernel::System::getService<Kernel::InterruptService>();
    interruptService.assignInterrupt(Kernel::InterruptDispatcher::ERROR, *this);
    LApic::allow(LApic::ERROR); // TODO: This in interruptservice
}

void ErrorInterruptHandler::trigger(const Kernel::InterruptFrame &frame) {
    LApic::handleErrors();
}

}
