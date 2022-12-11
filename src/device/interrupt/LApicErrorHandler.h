#ifndef HHUOS_LAPICERRORHANDLER_H
#define HHUOS_LAPICERRORHANDLER_H

#include "kernel/interrupt/InterruptHandler.h"

namespace Device {

class LApicErrorHandler : public Kernel::InterruptHandler {
public:
    LApicErrorHandler() = default;

    LApicErrorHandler(const LApicErrorHandler &copy) = delete;

    LApicErrorHandler &operator=(const LApicErrorHandler &other) = delete;

    ~LApicErrorHandler() override = default;


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

#endif //HHUOS_LAPICERRORHANDLER_H
