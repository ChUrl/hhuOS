#ifndef HHUOS_APICACPIPARSER_H
#define HHUOS_APICACPIPARSER_H

#include "ApicStructures.h"

namespace Device {

/**
 * @brief This class provides functions to parse information about the InterruptModel from ACPI 1.0.
 */
class ApicAcpiParser {
public:
    ApicAcpiParser() = delete;

    ApicAcpiParser(const ApicAcpiParser &copy) = delete;

    ApicAcpiParser &operator=(const ApicAcpiParser &copy) = delete;

    ~ApicAcpiParser() = delete;

    /**
     * @brief Initialize a LPlatformInformation structure with information parsed from ACPI 1.0.
     */
    static LPlatformInformation *parseLPlatformInformation();

    /**
     * @brief Initialize an IoPlatformInformation structure with information parsed from ACPI 1.0.
     */
    static IoPlatformInformation *parseIoPlatformInformation();

private:
    /**
     * @brief Check if the system supports ACPI 1.0.
     */
    static bool hasACPI10();

    /**
     * @brief Lookup an APIC id by an ACPI APIC id.
     */
    static uint8_t acpiIdToApicId(LPlatformInformation *info, uint8_t uid);
};

}

#endif //HHUOS_APICACPIPARSER_H
