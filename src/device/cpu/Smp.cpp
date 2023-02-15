#include "Smp.h"
#include "device/interrupt/apic/Apic.h"
#include "device/cpu/Cpu.h"

namespace Device {

uint32_t **apStacks = nullptr;
volatile uint64_t runningAPs = 0; // This is used as a bitmap, once an AP is running it sets its corresponding bit to 1

Kernel::Logger log = Kernel::Logger::get("SMP");

[[noreturn]] void smpEntry(uint8_t apicid) {
    log.info("CPU [%d] is now online!", apicid);
    runningAPs |= (1 << apicid); // Mark that this AP is running

    // Wait until the BSP APIC has been initialized fully before continuing:
    // At this point the interrupts will be enabled again, so the current timer
    // would be able to calibrate itself (which it doesn't do because the BSP
    // timer already did it).
    while (!Device::Apic::isBspTimerRunning()) {}

    // Initialize this AP's APIC
    Device::Apic::initializeCurrentLocalApic();
    Device::Apic::enableCurrentErrorHandler();
    Device::Apic::startCurrentTimer();

    // Bore the AP to death
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
}

} // namespace Device
