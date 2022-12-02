#include "ModelSpecificRegister.h"

namespace Device {

// NOTE: Inline Assembly: https://gcc.gnu.org/onlinedocs/gcc/Extended-Asm.html
// NOTE: https://wiki.osdev.org/Model_Specific_Registers#Accessing_Model_Specific_Registers

uint64_t ModelSpecificRegister::readQuadWord() const {
    uint32_t low, high;

    // rdmsr writes read value to eax/edx from register specified in ecx, it has no operands
    asm volatile ("rdmsr" : "=a" (low), "=d" (high) : "c" (MSR_ADDRESS));

    return low | (static_cast<uint64_t>(high) << 32);
}

void ModelSpecificRegister::writeQuadWord(uint64_t val) {
    uint32_t low, high;
    low = val & 0xFFFFFFFF;
    high = val >> 32;

    // wrmsr writes values from eax/edx to register specified in ecx, it has no operands
    asm volatile ("wrmsr" : : "a" (low), "d" (high), "c" (MSR_ADDRESS));
}

}