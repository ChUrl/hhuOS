#ifndef HHUOS_MODELSPECIFICREGISTER_H
#define HHUOS_MODELSPECIFICREGISTER_H

#include <cstdint>

namespace Device {

class ModelSpecificRegister {
public:
    /**
     * @brief Constructor.
     *
     * @param msr The MSR address as listed in IA-32 manual, sec. 4
     */
    explicit ModelSpecificRegister(uint32_t msr);

    /**
     * @brief Copy Constructor.
     */
    ModelSpecificRegister(const ModelSpecificRegister &copy) = delete;

    /**
     * @brief Assignment operator.
     */
    ModelSpecificRegister &operator=(const ModelSpecificRegister &other) = delete;

    /**
     * @brief Destructor.
     */
    ~ModelSpecificRegister() = default;

    /**
     * @brief Read from a model specific register.
     *
     * @return The read 64 bit value
     */
    [[nodiscard]] uint64_t readQuadWord() const;

    /**
     * @brief Write to a model specific register.
     *
     * @param val The 64 bit value to write
     */
    void writeQuadWord(uint64_t val) const;

private:
    uint32_t MSR_ADDRESS;
};

} // namespace Device

#endif //HHUOS_MODELSPECIFICREGISTER_H
