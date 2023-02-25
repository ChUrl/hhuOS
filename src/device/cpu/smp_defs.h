#ifndef SMPSTARTUP_H
#define SMPSTARTUP_H

#include <cstdint>

// Descriptor for either GDT or IDT
struct Descriptor {
    uint16_t size;
    uint64_t address;
} __attribute__((packed));

// Import from smp.asm
extern "C" void boot_ap(void);
extern const uint16_t boot_ap_size;
extern Descriptor boot_ap_gdtr;
extern Descriptor boot_ap_idtr;
extern uint32_t boot_ap_cr0;
extern uint32_t boot_ap_cr3;
extern uint32_t boot_ap_cr4;
extern volatile uint32_t boot_ap_gdts;   // Not written by asm volatile (), so add volatile here
extern volatile uint32_t boot_ap_stacks; // Not written by asm volatile (), so add volatile here
extern volatile uint32_t boot_ap_entry;  // Not written by asm volatile (), so add volatile here

namespace Device {

// Export to smp.asm
extern "C" [[noreturn]] void smpEntry(uint8_t apicid);
// This is used as a bitmap, once an AP is running it sets its corresponding bit to 1.
// MPSpec requires the ids to be sequential (sec. B.4), so it works for a max. of 64 CPUs.
extern volatile uint64_t runningAPs;

// If any of these two are changed, smp.asm has to be changed too (the %defines at the top)!
const constexpr uint32_t apStackSize = 0x1000;      ///< @brief Size of the stack allocated for each AP.
const constexpr uint16_t apStartupAddress = 0x8000; ///< @brief Physical address the AP startup routine is copied to.

} // namespace Device

#endif