#ifndef HHUOS_INTERRUPTARCHITECTUREACPI10_H
#define HHUOS_INTERRUPTARCHITECTUREACPI10_H

#include "InterruptModel.h"

namespace Device {

class InterruptArchitectureACPI10 {
public:
    InterruptArchitectureACPI10() = default;

    InterruptArchitectureACPI10(const InterruptArchitectureACPI10 &copy) = delete;

    InterruptArchitectureACPI10 &operator=(const InterruptArchitectureACPI10 &copy) = delete;

    ~InterruptArchitectureACPI10() = default;


    static void initializeLPlatformInformation(LPlatformInformation *info);

    static void initializeIoPlatformInformation(IoPlatformInformation *info);

private:
    static bool hasACPI10();
    static void verifyACPI10();

    static uint8_t uidToId(LPlatformInformation *info, uint8_t uid);
};

}

#endif //HHUOS_INTERRUPTARCHITECTUREACPI10_H
