#ifndef HHUOS_LOCALAPICERROR_H
#define HHUOS_LOCALAPICERROR_H

#include "kernel/interrupt/InterruptHandler.h"
#include "kernel/log/Logger.h"

namespace Device {

class LocalApicError : public Kernel::InterruptHandler {
public:
    LocalApicError() = default;

    LocalApicError(const LocalApicError &copy) = delete;

    LocalApicError &operator=(const LocalApicError &other) = delete;

    ~LocalApicError() override = default;

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

#endif //HHUOS_LOCALAPICERROR_H
