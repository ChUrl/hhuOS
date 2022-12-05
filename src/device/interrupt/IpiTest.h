#ifndef HHUOS_IPITEST_H
#define HHUOS_IPITEST_H

#include "kernel/interrupt/InterruptHandler.h"

#define HHUOS_IPITEST_ENABLE 1

class IpiTest : public Kernel::InterruptHandler {
public:
    IpiTest() = default;
    IpiTest(const IpiTest &copy) = delete;
    IpiTest &operator=(const IpiTest &other) = delete;
    ~IpiTest() override = default;
    
    /**
     * Overriding function from InterruptHandler.
     */
    void plugin() override;

    /**
     * Overriding function from InterruptHandler.
     */
    void trigger(const Kernel::InterruptFrame &frame) override;
};

#endif //HHUOS_IPITEST_H
