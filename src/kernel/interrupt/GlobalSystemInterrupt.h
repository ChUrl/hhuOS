#ifndef HHUOS_GLOBALSYSTEMINTERRUPT_H
#define HHUOS_GLOBALSYSTEMINTERRUPT_H

#include <cstdint>

namespace Kernel {

/**
 * @brief The GlobalSystemInterrupts abstract the hardware interrupt model from the system.
 *
 * They cannot be named statically, as this depends on the system configuration.
 * GlobalSystemInterrupts map 1:1 to IO APIC interrupt inputs.
 * They do not translate 1:1 to InterruptSources or system interrupt vectors.
 */
enum GlobalSystemInterrupt : uint32_t {};

}

#endif //HHUOS_GLOBALSYSTEMINTERRUPT_H
