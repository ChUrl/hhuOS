#include "ApicTimer.h"
#include "LApic.h"

namespace Device {

Kernel::Logger ApicTimer::log = Kernel::Logger::get("ApicTimer");

ApicTimer::ApicTimer() {
    // Recommended order: Divide -> LVT -> Initial Count (OSDev)
    LApic::writeDoubleWord(LApic::Register::TIMER_DIVIDE, Divide::BY_1);
    LApic::writeLVT(LApic::TIMER, { .timerMode = LApic::LVT_Timer_Mode::PERIODIC });

    // TODO: Has to be calibrated (using PIT or RTC or probably just the TimeService)
    // TODO: Setting this to values smaller than 0xFFFFFFFF doesn't work?
    //       Probably because the timer interrupt is bugged so if the timer counts down too fast it burns
    LApic::writeDoubleWord(LApic::Register::TIMER_INITIAL, 0xFFFFFF);
}

void ApicTimer::plugin() {
    auto &interruptService = Kernel::System::getService<Kernel::InterruptService>();
    interruptService.assignInterrupt(Kernel::InterruptDispatcher::APICTIMER, *this);
    LApic::allow(LApic::TIMER, Kernel::InterruptDispatcher::APICTIMER); // TODO: This in interruptservice?
}

void ApicTimer::trigger(const Kernel::InterruptFrame &frame) {
    // TODO: Do not try to heap allocate a string literal in the timer interrupt handler
    // log.debug("Called ApicTimer::trigger()");
}

}