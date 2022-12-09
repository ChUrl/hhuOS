#include "ApicTimer.h"
#include "LApic.h"
#include "kernel/service/TimeService.h"
#include "lib/util/async/Thread.h"

namespace Device {

Kernel::Logger ApicTimer::log = Kernel::Logger::get("ApicTimer");

ApicTimer::ApicTimer(uint32_t timerInterval, uint32_t yieldInterval) : yieldInterval(yieldInterval) {
    auto &timeService = Kernel::System::getService<Kernel::TimeService>();

    // Recommended order: Divide -> LVT -> Initial Count (OSDev)
    LApic::writeDoubleWord(LApic::Register::TIMER_DIVIDE, Divide::BY_1); // TODO: What divider?
    LApic::writeLVT(LApic::TIMER, {.isMasked = true, .timerMode = LApic::LVT_Timer_Mode::PERIODIC});

    setInterruptRate(timerInterval);
}

void ApicTimer::plugin() {
    auto &interruptService = Kernel::System::getService<Kernel::InterruptService>();
    interruptService.assignInterrupt(Kernel::InterruptDispatcher::APICTIMER, *this);
    LApic::allow(LApic::TIMER, Kernel::InterruptDispatcher::APICTIMER); // TODO: This in interruptservice?
}

void ApicTimer::trigger(const Kernel::InterruptFrame &frame) {
    time.addNanoseconds(timerInterval);
    if (time.toMilliseconds() % yieldInterval == 0) {
        Kernel::System::getService<Kernel::SchedulerService>().yield();
    }
}

Util::Time::Timestamp ApicTimer::getTime() {
    return time;
}

// TODO: Check CPUID if the timer stops in deep C-States (IA-32 10.5.4)
// https://wiki.osdev.org/APIC_timer#Example_code_in_C
void ApicTimer::setInterruptRate(uint32_t interval) {
    LApic::writeDoubleWord(LApic::Register::TIMER_INITIAL, 0xFFFFFFFF); // Max initial counter, writing starts timer

    // TODO: Suboptimal for 2 reasons: Large interval delays the system and inaccurate?
    Util::Async::Thread::sleep(Util::Time::Timestamp::ofMilliseconds(interval / 1000000));
    uint32_t initialCount = 0xFFFFFFFF - LApic::readDoubleWord(LApic::Register::TIMER_CURRENT);

    log.info("Setting APIC Timer interval to [%uns] (Initial count: [%u])", interval, initialCount);
    LApic::writeDoubleWord(LApic::Register::TIMER_INITIAL, initialCount);

    timerInterval = interval;
}

}