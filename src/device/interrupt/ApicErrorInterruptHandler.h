#ifndef HHUOS_APICERRORINTERRUPTHANDLER_H
#define HHUOS_APICERRORINTERRUPTHANDLER_H

#include "kernel/interrupt/InterruptHandler.h"

namespace Device {

class ApicErrorInterruptHandler : public Kernel::InterruptHandler {
public:
    ApicErrorInterruptHandler() = default;

    ApicErrorInterruptHandler(const ApicErrorInterruptHandler &copy) = delete;

    ApicErrorInterruptHandler &operator=(const ApicErrorInterruptHandler &other) = delete;

    ~ApicErrorInterruptHandler() override = default;


    /**
     * Overriding function from InterruptHandler.
     */
    void plugin() override;

    /**
     * Overriding function from InterruptHandler.
     */
    void trigger(const Kernel::InterruptFrame &frame) override;
};

}

#endif //HHUOS_APICERRORINTERRUPTHANDLER_H
