#include "ApicTimer.h"
#include "device/interrupt/apic/LocalApic.h"
#include "kernel/service/InterruptService.h"
#include "kernel/service/SchedulerService.h"
#include "kernel/service/TimeService.h"
#include "kernel/system/System.h"

namespace Device {

uint32_t ApicTimer::timerInt = 0;
uint32_t ApicTimer::yieldInt = 0;
uint32_t ApicTimer::counter = 0;
Kernel::Logger ApicTimer::log = Kernel::Logger::get("ApicTimer");

ApicTimer::ApicTimer(uint32_t timerInterval, uint32_t yieldInterval) : cpuId(LocalApic::getId()) {
    if (timerInterval == 0) {
        Util::Exception::throwException(Util::Exception::INVALID_ARGUMENT, "APIC timer interval can't be 0!");
    }
    if (yieldInterval == 0) {
        Util::Exception::throwException(Util::Exception::INVALID_ARGUMENT, "APIC timer yield interval can't be 0!");
    }

    // Only set this once, so all cores are equal with single calibration.
    // Of course every timer could calibrate itself to a different interval, but
    // I thought that would be useless.
    if (timerInt == 0) {
        timerInt = timerInterval;
    } else if (timerInt != timerInterval) {
        log.warn("Changed APIC timer interval for CPU [%d] to [%uns] instead of the requested [%uns]!", LocalApic::getId(), timerInt, timerInterval);
    }

    // Recommended order: Divide -> LVT -> Initial Count (OSDev)
    LocalApic::writeDoubleWord(LocalApic::TIMER_DIVIDE, Divide::BY_16); // BY_1 is the highest resolution (overkill)
    LVTEntry lvtEntry = LocalApic::readLVT(LocalApic::TIMER);
    lvtEntry.timerMode = LVTEntry::TimerMode::PERIODIC;
    LocalApic::writeLVT(LocalApic::TIMER, lvtEntry);

    // Note that we do not need to specify a destination CPU (it is not possible), because this is only called
    // once per core. When an AP calls this, this AP's local APIC timer is configured, so only the calling AP receives
    // its timer interrupts.

    // Only do the calibration once, APs get initialized with the same value as the BSP
    if (counter == 0) {
        counter = calibrateInitialCounter();
    }

    log.info("Setting APIC timer interval for CPU [%d] to [%uns] (Initial count: [%u])", cpuId, timerInt, counter);
    LocalApic::writeDoubleWord(LocalApic::TIMER_INITIAL, counter);
}

void ApicTimer::plugin() {
    auto &interruptService = Kernel::System::getService<Kernel::InterruptService>();
    interruptService.assignInterrupt(Kernel::InterruptVector::APICTIMER, *this);
    LocalApic::allow(LocalApic::TIMER);
}

void ApicTimer::trigger(const Kernel::InterruptFrame &frame) {
    time.addNanoseconds(timerInt);

    if (cpuId != 0) {
        // Currently there is only a scheduler in the BSP
        return;
    }

    if (time.toMilliseconds() % yieldInt == 0) {
        // Currently there is only one main scheduler, for SMP systems this should yield the core-scheduler
        Kernel::System::getService<Kernel::SchedulerService>().yield();
    }
}

Util::Time::Timestamp ApicTimer::getTime() {
    return time;
}

uint32_t ApicTimer::calibrateInitialCounter() {
    auto &timeService = Kernel::System::getService<Kernel::TimeService>();

    // If the timerInterval is 10ms, we wait longer, because calibrating with too few PIT ticks is imprecise
    // (at least in QEMU). If I took more attention to this, it should probably depend on the interval the PIT
    // (or other calibration source) uses, because the APIC timer calibration time needs to be larger.
    // It should probably be a direct multiple of the calibration source tick interval, like factor 10 or sth.
    const uint8_t calibrationTimeFactor = 10;

    // The calibration works by waiting the desired interval and measuring how many ticks the timer does.
    // The interval should not be too small, since the measurement becomes inaccurate for small intervals.
    // This is obviously slow, as time is spent waiting, if two very accurate timestamps could be taken, the initial
    // counter could be calculated by the difference, with very little waiting time.
    LocalApic::writeDoubleWord(LocalApic::TIMER_INITIAL, 0xFFFFFFFF);                                            // Max initial counter, writing starts timer
    timeService.busyWait(Util::Time::Timestamp::ofMilliseconds((timerInt / 1'000'000) * calibrationTimeFactor)); // Interval but in milliseconds
    const uint32_t initialCount = 0xFFFFFFFF - LocalApic::readDoubleWord(LocalApic::TIMER_CURRENT);              // Ticks in interval

    return initialCount / calibrationTimeFactor; // Return the counter to initialize AP timers with the same value
}

} // namespace Device