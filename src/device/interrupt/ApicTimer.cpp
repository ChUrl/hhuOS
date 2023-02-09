#include "ApicTimer.h"
#include "LocalApic.h"
#include "ApicRegisterInterface.h"
#include "kernel/service/TimeService.h"
#include "lib/util/async/Thread.h"

namespace Device {

bool ApicTimer::initialized = false;
Kernel::Logger ApicTimer::log = Kernel::Logger::get("ApicTimer");

ApicTimer::ApicTimer(uint32_t timerInterval, uint32_t yieldInterval) : yieldInterval(yieldInterval) {
    LocalApic::ensureBspInitialized();

    // Recommended order: Divide -> LVT -> Initial Count (OSDev)
    LocalApic::writeDoubleWord(LocalApic::TIMER_DIVIDE, Divide::BY_16); // BY_1 is the highest resolution (overkill)
    LVTEntry lvtEntry = LocalApic::readLVT(LocalApic::TIMER);
    lvtEntry.timerMode = LVTEntry::TimerMode::PERIODIC;
    LocalApic::writeLVT(LocalApic::TIMER, lvtEntry);

    setInterruptRate(timerInterval);
    initialized = true;
}

void ApicTimer::plugin() {
    auto &interruptService = Kernel::System::getService<Kernel::InterruptService>();
    interruptService.assignInterrupt(Kernel::InterruptVector::APICTIMER, *this);
    LocalApic::allow(LocalApic::TIMER);
}

void ApicTimer::trigger(const Kernel::InterruptFrame &frame) {
    time.addNanoseconds(timerInterval);
    if (time.toMilliseconds() % yieldInterval == 0) {
        // Currently there is only one main scheduler, for SMP systems this should yield the core-scheduler
        Kernel::System::getService<Kernel::SchedulerService>().yield();
    }
}

Util::Time::Timestamp ApicTimer::getTime() {
    return time;
}

void ApicTimer::setInterruptRate(uint32_t interval) {
    // The calibration works by waiting the desired interval and measuring how many ticks the timer does
    // The interval should not be too small, since the measurement becomes inaccurate for small intervals
    // This is obviously slow, as time is spent waiting, if two more accurate timestamps could be taken, the initial
    // counter could be calculated by the difference, with very little waiting time.
    auto &timeService = Kernel::System::getService<Kernel::TimeService>();
    LocalApic::writeDoubleWord(LocalApic::TIMER_INITIAL, 0xFFFFFFFF); // Max initial counter, writing starts timer
    timeService.busyWait(Util::Time::Timestamp::ofMilliseconds(100 * interval / 1000000)); // Interval but in milliseconds
    uint32_t initialCount = 0xFFFFFFFF - LocalApic::readDoubleWord(LocalApic::TIMER_CURRENT); // Ticks in interval

    log.info("Setting APIC Timer interval to [%uns] (Initial count: [%u])", interval, initialCount / 100);
    LocalApic::writeDoubleWord(LocalApic::TIMER_INITIAL, initialCount / 100);

    timerInterval = interval;
}

bool ApicTimer::isInitialized() {
    return initialized;
}

}