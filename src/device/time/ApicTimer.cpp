#include "ApicTimer.h"
#include "device/interrupt/apic/LocalApic.h"
#include "device/time/Pit.h"
#include "kernel/service/InterruptService.h"
#include "kernel/service/SchedulerService.h"
#include "kernel/system/System.h"

namespace Device {

uint32_t ApicTimer::ticksIn1ms = 0;
Kernel::Logger ApicTimer::log = Kernel::Logger::get("ApicTimer");

ApicTimer::ApicTimer(uint32_t timerInterval, uint32_t yieldInterval)
  : cpuId(LocalApic::getId()), timerInterval(timerInterval), yieldInterval(yieldInterval) {
    if (ticksIn1ms == 0) {
        Util::Exception::throwException(Util::Exception::INVALID_ARGUMENT, "APIC timer not calibrated!");
    }
    if (timerInterval == 0) {
        Util::Exception::throwException(Util::Exception::INVALID_ARGUMENT, "APIC timer interval can't be 0!");
    }
    if (yieldInterval == 0) {
        Util::Exception::throwException(Util::Exception::INVALID_ARGUMENT, "APIC timer yield interval can't be 0!");
    }

    const uint32_t counter = ticksIn1ms * timerInterval;
    log.info("Setting APIC timer interval for CPU [%d] to [%ums] (Initial count: [%u])",
             cpuId, timerInterval, counter);

    // Recommended order: Divide -> LVT -> Initial Count (OSDev)
    LocalApic::writeDoubleWord(LocalApic::TIMER_DIVIDE, Divide::BY_16); // BY_1 is the highest resolution (overkill)
    LVTEntry lvtEntry = LocalApic::readLVT(LocalApic::TIMER);
    lvtEntry.timerMode = LVTEntry::TimerMode::PERIODIC;
    LocalApic::writeLVT(LocalApic::TIMER, lvtEntry);
    LocalApic::writeDoubleWord(LocalApic::TIMER_INITIAL, counter);
}

void ApicTimer::plugin() {
    auto &interruptService = Kernel::System::getService<Kernel::InterruptService>();
    interruptService.assignInterrupt(Kernel::InterruptVector::APICTIMER, *this);
    LocalApic::allow(LocalApic::TIMER);
}

void ApicTimer::trigger(const Kernel::InterruptFrame &frame) {
    if (cpuId != LocalApic::getId()) {
        // Every core's timer uses the same (this) handler, but it exists once per core (each core
        // has its own ApicTimer instance). All handlers are registered to the same interrupt
        // vector, we only want to reach the instance belonging to this core.
        return;
    }

    // Increase the "core-local" time, the systemtime is still managed by the PIT.
    time.addNanoseconds(timerInterval);

    if (cpuId != 0) {
        // Currently there is only one scheduler, it should get triggered only by the BSP.
        // Otherwise, the scheduler would be triggered n-times faster than intended, where n
        // is the CPU count.
        return;
    }

    if (time.toMilliseconds() % yieldInterval == 0) {
        // Currently there is only one main scheduler, for SMP systems this should yield the core-scheduler
        // or something similar.
        Kernel::System::getService<Kernel::SchedulerService>().yield();
    }
}

Util::Time::Timestamp ApicTimer::getTime() {
    return time;
}

void ApicTimer::calibrate() {
    // The calibration works by waiting the desired interval and measuring how many ticks the timer does.
    LocalApic::writeDoubleWord(LocalApic::TIMER_INITIAL, 0xFFFFFFFF);                     // Max initial counter, writing starts timer
    Pit::earlyDelay(50'000);                                                              // Wait 50 ms
    ticksIn1ms = (0xFFFFFFFF - LocalApic::readDoubleWord(LocalApic::TIMER_CURRENT)) / 50; // Ticks in 1 ms
}

} // namespace Device