#include "smp.h"
#include "kernel/log/Logger.h"

uint32_t** apStacks = nullptr;
volatile uint8_t runningAPs = 0; // This is used as a bitmap, once an AP is running it sets its corresponding bit to 1

Kernel::Logger log = Kernel::Logger::get("SMP");

[[noreturn]] void smpEntry(uint8_t apicid) {
    runningAPs |= (1 << apicid); // Mark that this AP is running
    log.info("CPU [%d] online", apicid);

    // Device::LocalApic::initializeAp();
    // Device::Apic::initializeTimer();

    // Nothing to do...
    while (true);
};
