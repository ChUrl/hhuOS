#include "Smp.h"
#include "kernel/log/Logger.h"
#include "device/interrupt/apic/Apic.h"

uint32_t** apStacks = nullptr;
volatile uint64_t runningAPs = 0; // This is used as a bitmap, once an AP is running it sets its corresponding bit to 1

Kernel::Logger log = Kernel::Logger::get("SMP");

[[noreturn]] void smpEntry(uint8_t apicid) {
    runningAPs |= (1 << apicid); // Mark that this AP is running
    log.info("CPU [%d] is now online!", apicid);

    Device::Apic::getCurrentLocalApic().initializeAp();
    Device::Apic::initializeErrorHandling();
    Device::Apic::initializeTimer();
    Device::ApicTimer &timer = Device::Apic::getCurrentTimer();

    volatile uint32_t localCoreTime;
    uint32_t lastLog = 0;
    while (true) {
        localCoreTime = timer.getTime().toSeconds();
        if (localCoreTime > lastLog + 2) {
            // We can't really do anything in here yet, but I kept this to check the timer interrupt in GDB
            // log.info("CPU [%d]: Timer is running!", apicid);
            lastLog = localCoreTime;
        }
    }
};
