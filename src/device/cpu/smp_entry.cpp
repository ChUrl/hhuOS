#include "device/interrupt/apic/Apic.h"
#include "device/time/Cmos.h"
#include "smp_defs.h"

namespace Device {

volatile uint64_t runningAPs = 0; // This is used as a bitmap, once an AP is running it sets its corresponding bit to 1

Kernel::Logger log = Kernel::Logger::get("SMP");

[[noreturn]] void smpEntry(uint8_t apicid) {
    log.info("CPU [%d] is now online!", apicid);

    // Initialize this AP's APIC
    Device::Apic::initializeCurrentLocalApic();
    Device::Apic::enableCurrentErrorHandler();
    Device::Apic::startCurrentTimer();

    runningAPs |= (1 << apicid); // Mark that this AP is running

    // Enable interrupts for this AP (usually results in a crash)
    // asm volatile ("sti"); // Can't use the CPU class, as it's only for singlecore
    // Device::Cmos::enableNmi(); // TODO: I don't know if this is core-local

    // Bore the AP to death. Sadly we can't do anything here, as the paging is not
    // designed to work with multiple CPUs.
    while (true) {}
}

} // namespace Device
