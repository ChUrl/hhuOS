#ifndef HHUOS_ERRORINTERRUPTHANDLER_H
#define HHUOS_ERRORINTERRUPTHANDLER_H

#include "kernel/interrupt/InterruptHandler.h"

namespace Device {

class ErrorInterruptHandler : public Kernel::InterruptHandler {
public:
    ErrorInterruptHandler() = default;

    ErrorInterruptHandler(const ErrorInterruptHandler &copy) = delete;

    ErrorInterruptHandler &operator=(const ErrorInterruptHandler &other) = delete;

    ~ErrorInterruptHandler() override = default;


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

#endif //HHUOS_ERRORINTERRUPTHANDLER_H
