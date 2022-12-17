#ifndef HHUOS_APICTIMER_H
#define HHUOS_APICTIMER_H

#include "kernel/interrupt/InterruptHandler.h"
#include "kernel/service/InterruptService.h"
#include "kernel/log/Logger.h"
#include "kernel/system/System.h"
#include "device/time/TimeProvider.h"

// TODO: This code doesn't account for multicore systems
//       - The calibration only has to be done once, but the registers have to be written
//         for every core (do before APs get initialized and set registers in AP init sequence?)
//       - I think it wouldn't make sense to use the APICTIMER for timekeeping then
//         because every CPU core would influence the timestamp (if it was static) or have
//         an individual timestamp

// NOTE: If the ApicTimer would be used exclusively the PIT handler could be reused, but I don't want
// NOTE: to exclude the possibility of using both (PIT for time, ApicTimer for scheduling)

// NOTE: The APIC Timer's counter is decremented at external CPU frequency (bus frequency)
// NOTE: divided by the divisor specified in the divide register (thus Divide::BY_1 is the fastest)

namespace Device {

class ApicTimer : public Kernel::InterruptHandler, public TimeProvider {
public:
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
