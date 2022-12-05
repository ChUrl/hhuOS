#include "IpiTest.h"
#include "kernel/service/InterruptService.h"
#include "kernel/system/System.h"

void IpiTest::plugin() {
    auto &interruptService = Kernel::System::getService<Kernel::InterruptService>();
    interruptService.assignInterrupt(Kernel::InterruptDispatcher::IPITEST, *this);
}

void IpiTest::trigger(const Kernel::InterruptFrame &frame) {
    Kernel::Logger::get("IpiTest").debug("Called IpiTest::trigger()");
}