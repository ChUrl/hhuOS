#ifndef HHUOS_APICTIMER_H
#define HHUOS_APICTIMER_H

#include "kernel/interrupt/InterruptHandler.h"
#include "kernel/service/InterruptService.h"
#include "kernel/log/Logger.h"
#include "kernel/system/System.h"
#include "device/time/TimeProvider.h"

// NOTE: The APIC Timer's counter is decremented at external CPU frequency (bus frequency)
// NOTE: divided by the divisor specified in the divide register (thus Divide::BY_1 is the fastest)

namespace Device {

// This class implements the TimeProvider interface, but is currently not used for the TimeService
// Its purpose is to trigger the preemption in SMP systems for individual cores
class ApicTimer : public Kernel::InterruptHandler, public TimeProvider {
public:
    /**
     * Constructor.
     *
     * @param timerInterval The tick interval in nanoseconds (10 milliseconds by default)
     * @param yieldInterval The preemption interval in milliseconds (10 milliseconds by default)
     */
    explicit ApicTimer(uint32_t timerInterval = 1000000, uint32_t yieldInterval = 10);

    ApicTimer(const ApicTimer &copy) = delete;

    ApicTimer &operator=(const ApicTimer &other) = delete;

    ~ApicTimer() override = default;

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

    static bool isInitialized();

private:
    // IA-32 Architecture Manual Chapter 10.5.4
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
     * Sets the interval at which the APIC Timer fires interrupts.
     *
     * @param interval The interval in nanoseconds
     */
    void setInterruptRate(uint32_t interval);

private:
    static bool initialized;

    Util::Time::Timestamp time{};
    uint32_t timerInterval = 0;
    uint32_t yieldInterval;

    static Kernel::Logger log;
};

}

#endif //HHUOS_APICTIMER_H
