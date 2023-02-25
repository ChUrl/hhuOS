#ifndef HHUOS_APICTIMER_H
#define HHUOS_APICTIMER_H

#include "device/time/TimeProvider.h"
#include "kernel/interrupt/InterruptHandler.h"
#include "kernel/log/Logger.h"

namespace Device {

/**
 * @brief This class implements the APIC timer device.
 *
 * Its purpose is to trigger per-core scheduler preemption in SMP systems, although it is also used
 * in singlecore systems. It is not used for system-time keeping, this is still done by the PIT.
 *
 * It receives its tick interval in milliseconds, which should be precise enough for scheduling.
 * If a more precise interval is required, the timer divider might need adjustment.
 */
class ApicTimer : public Kernel::InterruptHandler, public TimeProvider {
    friend class Apic;

public:
    ApicTimer(const ApicTimer &copy) = delete;

    ApicTimer &operator=(const ApicTimer &other) = delete;

    /**
     * Overriding function from InterruptHandler.
     */
    void plugin() override;

    /**
     * Overriding function from InterruptHandler.
     */
    void trigger(const Kernel::InterruptFrame &frame) override;

    /**
     * Overriding function from TimeProvider.
     */
    [[nodiscard]] Util::Time::Timestamp getTime() override;

private:
    /**
     * @brief These are the different divider mode the APIC timer's counter supports.
     *
     * The APIC timer generates signals of a certain frequency by counting down a register.
     * If the divider is set to "BY_1", the register is counted down on every bus clock, yielding
     * the highest precision. If this is not required, the countdown can be slowed by dividing with
     * a higher value. This can achieve longer intervals that would otherwise not be possible with
     * a 32-bit counter register. See IA-32 manual, sec. 3.11.5.4
     */
    enum Divide : uint32_t {
        BY_1 = 0b1011,
        BY_2 = 0b0000,
        BY_4 = 0b0001,
        BY_8 = 0b0010,
        BY_16 = 0b0011,
        BY_32 = 0b1000,
        BY_64 = 0b1001,
        BY_128 = 0b1010
    };

private:
    /**
     * @brief Construct an ApicTimer instance.
     *
     * @param timerInterval The tick interval in milliseconds (10 milliseconds by default)
     * @param yieldInterval The preemption interval in milliseconds (10 milliseconds by default)
     */
    explicit ApicTimer(uint32_t timerInterval = 10, uint32_t yieldInterval = 10);

    ~ApicTimer() override = default;

    /**
     * @brief Calibrate the APIC timer using the PIT.
     *
     * Uses the PIT to measure how often the APIC timer ticks in 10ms. When constructing
     * a new timer, this value will be used to calculate the initial counter for the desired interval.
     */
    static void calibrate();

private:
    uint8_t cpuId;          ///< @brief The id of the CPU that uses this timer.
    uint32_t timerInterval; ///< @brief The interrupt trigger interval in milliseconds.
    uint32_t yieldInterval; ///< @brief The preemption trigger interval in milliseconds.

    static uint32_t ticksIn1ms; ///< @brief The number of ticks the APIC timer does in 10 ms.
    static Divide divider;      ///< @brief The used divider, it has to be consistent to get consistent timings.

    Util::Time::Timestamp time{}; ///< @brief The "core-local" timestamp.

    static Kernel::Logger log;
};

} // namespace Device

#endif //HHUOS_APICTIMER_H
