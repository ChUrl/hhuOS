#ifndef HHUOS_APICERRORHANDLER_H
#define HHUOS_APICERRORHANDLER_H

#include "kernel/interrupt/InterruptHandler.h"
#include "kernel/log/Logger.h"

namespace Device {

class ApicErrorHandler : public Kernel::InterruptHandler {
public:
    ApicErrorHandler() = default;

    ApicErrorHandler(const ApicErrorHandler &copy) = delete;

    ApicErrorHandler &operator=(const ApicErrorHandler &other) = delete;

    ~ApicErrorHandler() override = default;

    /**
     * Overriding function from InterruptHandler.
     */
    void plugin() override;

    /**
     * Overriding function from InterruptHandler.
     */
    void trigger(const Kernel::InterruptFrame &frame) override;

private:
    static Kernel::Logger log;
};

} // namespace Device

#endif //HHUOS_APICERRORHANDLER_H
