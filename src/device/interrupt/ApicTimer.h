#ifndef HHUOS_APICTIMER_H
#define HHUOS_APICTIMER_H

#include "kernel/interrupt/InterruptHandler.h"
#include "kernel/service/InterruptService.h"
#include "kernel/log/Logger.h"
#include "kernel/system/System.h"

#define HHUOS_APICTIMER_ENABLE 0

namespace Device {

class ApicTimer : public Kernel::InterruptHandler {
public:
    // TODO: Parameters for interval, calibration source etc.
    /**
     * Constructor.
     */
    explicit ApicTimer();

    /**
     * Copy Constructor.
     */
    ApicTimer(const ApicTimer &copy) = delete;

    /**
     * Assignment operator.
     */
    ApicTimer &operator=(const ApicTimer &other) = delete;

    /**
     * Destructor.
     */
    ~ApicTimer() override = default;
    
    /**
     * Overriding function from InterruptHandler.
     */
    void plugin() override;

    /**
     * Overriding function from InterruptHandler.
     */
    void trigger(const Kernel::InterruptFrame &frame) override;

    // TODO: Functions to set mode/interval more easily

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

    static Kernel::Logger log;
};

}

#endif //HHUOS_APICTIMER_H
