#include "LApicErrorHandler.h"
#include "kernel/service/InterruptService.h"
#include "kernel/system/System.h"
#include "LApic.h"

namespace Device {

void LApicErrorHandler::plugin() {
    auto &interruptService = Kernel::System::getService<Kernel::InterruptService>();
    interruptService.assignInterrupt(Kernel::InterruptDispatcher::ERROR, *this);
    LApic::allow(LApic::ERROR); // TODO: This in interruptservice
}

void LApicErrorHandler::trigger(const Kernel::InterruptFrame &frame) {
    LApic::handleErrors();
}

}
