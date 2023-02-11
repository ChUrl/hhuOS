#ifndef HHUOS_MODELSPECIFICREGISTER_H
#define HHUOS_MODELSPECIFICREGISTER_H

#include <cstdint>

namespace Device {

class ModelSpecificRegister {
public:
    /**
     * Constructor initializes the address.
     */
    explicit ModelSpecificRegister(uint32_t msr);

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
