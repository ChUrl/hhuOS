#ifndef SMPSTARTUP_H
#define SMPSTARTUP_H

#include <cstdint>

// Copied from the Linux kernel:
// https://github.com/torvalds/linux/blob/master/arch/x86/include/asm/desc_defs.h#L112 (visited on 10/02/23)
// https://github.com/torvalds/linux/blob/master/arch/x86/include/asm/desc.h#L218 (visited on 10/02/23)
struct Descriptor {
    uint16_t size;
    uint64_t address;
} __attribute__((packed)) ;

// Import
extern "C" void boot_ap(void);
extern const uint16_t boot_ap_size;
extern Descriptor boot_ap_gdtr;
extern Descriptor boot_ap_idtr;
extern uint32_t boot_ap_cr0;
extern uint32_t boot_ap_cr3;
extern uint32_t boot_ap_cr4;
extern volatile uint32_t boot_ap_stacks; // Not written by asm volatile (), so add volatile here
extern volatile uint32_t boot_ap_entry; // Not written by asm volatile (), so add volatile here

// Export
extern "C" [[noreturn]] void smpEntry(uint8_t apicid);
extern uint32_t** apStacks;
extern volatile uint8_t runningAPs; // This is used as a bitmap, once an AP is running it sets its corresponding bit to 1

// If any of these two are changed, smp.asm has to be changed too (the %defines at the top)!
const constexpr uint32_t apStackSize = 0x1000; ///< @brief Size of the stack allocated for each AP.
const constexpr uint32_t apStartupAddress = 0x8000; ///< @brief Physical address the AP startup routine is copied to.

#endif