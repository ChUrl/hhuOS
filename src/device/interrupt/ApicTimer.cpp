#include "ApicTimer.h"
#include "LApic.h"
#include "ApicRegisterInterface.h"
#include "kernel/service/TimeService.h"
#include "lib/util/async/Thread.h"

namespace Device {

bool ApicTimer::initialized = false;
Kernel::Logger ApicTimer::log = Kernel::Logger::get("ApicTimer");

ApicTimer::ApicTimer(uint32_t timerInterval, uint32_t yieldInterval) : yieldInterval(yieldInterval) {
    LApic::ensureInitialized();

    auto &timeService = Kernel::System::getService<Kernel::TimeService>();

    // Recommended order: Divide -> LVT -> Initial Count (OSDev)
    LApic::writeDoubleWord(LApic::TIMER_DIVIDE, Divide::BY_1); // TODO: What divider?
    LVTEntry lvtEntry = LApic::readLVT(LApic::TIMER);
    lvtEntry.timerMode = LVTEntry::TimerMode::PERIODIC;
    LApic::writeLVT(LApic::TIMER, lvtEntry);

    setInterruptRate(timerInterval);
    initialized = true;
}

void ApicTimer::plugin() {
    auto &interruptService = Kernel::System::getService<Kernel::InterruptService>();
    interruptService.assignInterrupt(Kernel::InterruptDispatcher::APICTIMER, *this);
    LApic::allow(LApic::TIMER); // TODO: This in interruptservice?
}

void ApicTimer::trigger(const Kernel::InterruptFrame &frame) {
    // TODO: Get the timestamp from the system and remove TimeProvider from ApicTimer
    time.addNanoseconds(timerInterval);
    if (time.toMilliseconds() % yieldInterval == 0) {
        Kernel::System::getService<Kernel::SchedulerService>().yield();
    }
}

Util::Time::Timestamp ApicTimer::getTime() {
    return time;
}

// TODO: This is very inaccurate, improve by:
//       1. Get start system timestamp
//       2. Start the timer with max counter
//       3. Busywait for fixed interval of 10ms
//       4. Get end system timestamp
//       5. Calculate the counter for the given interval
//          based on the number of counts occured in the end - start interval
// TODO: Check CPUID if the timer stops in deep C-States (IA-32 10.5.4)
void ApicTimer::setInterruptRate(uint32_t interval) {
    LApic::writeDoubleWord(LApic::TIMER_INITIAL, 0xFFFFFFFF); // Max initial counter, writing starts timer
    Util::Async::Thread::sleep(Util::Time::Timestamp::ofMilliseconds(interval / 1000000));
    uint32_t initialCount = 0xFFFFFFFF - LApic::readDoubleWord(LApic::TIMER_CURRENT);

    log.info("Setting APIC Timer interval to [%uns] (Initial count: [%u])", interval, initialCount);
    LApic::writeDoubleWord(LApic::TIMER_INITIAL, initialCount);

    timerInterval = interval;
}

}