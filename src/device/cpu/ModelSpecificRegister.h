#ifndef HHUOS_MODELSPECIFICREGISTER_H
#define HHUOS_MODELSPECIFICREGISTER_H

#include <cstdint>

// TODO: Is Device the correct namespace for a register?
// TODO: MSR stuff should be moved to some arch dependent location in hhuOS (x86/i386)
// TODO: This is arch dependent

namespace Device {

class ModelSpecificRegister {
public:
    /**
     * Constructor initializes the address.
     * TODO: Should I check if CPUID MSR is available here? Because apparently it's not needed for some MSRs...
     */
    explicit ModelSpecificRegister(uint32_t msr) : MSR_ADDRESS(msr) {};

    /**
     * Copy Constructor.
     */
    ModelSpecificRegister(const ModelSpecificRegister &copy) = delete;

    /**
     * Assignment operator.
     */
    ModelSpecificRegister &operator=(const ModelSpecificRegister &other) = delete;

    /**
     * Destructor.
     */
    ~ModelSpecificRegister() = default;

    /**
     * Read from a model specific register.
     *
     * @return The read 64 bit value
     */
    [[nodiscard]] uint64_t readQuadWord() const;

    /**
     * Write to a model specific register.
     *
     * @param val The 64 bit value to write
     */
    void writeQuadWord(uint64_t val) const;

private:
    const uint32_t MSR_ADDRESS;
};

}

#endif //HHUOS_MODELSPECIFICREGISTER_H
