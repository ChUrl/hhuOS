#ifndef HHUOS_APICACPIPARSER_H
#define HHUOS_APICACPIPARSER_H

#include "ApicAcpiInterface.h"
#include "device/power/acpi/Acpi.h"

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

    static void parseLocalPlatformInformation(LocalApicPlatform *localPlatform);

    static void parseIoPlatformInformation(IoApicPlatform *ioPlatform);

    static void parseIoApicInformation(Util::Data::ArrayList<IoApicInformation *> *ioInfos);

private:
    /**
     * @brief Ensure the system supports ACPI 1.0.
     */
    static void ensureAcpi10();

    /**
     * @brief Lookup an APIC id by an ACPI APIC uid.
     */
    static uint8_t acpiIdToApicId(const Util::Data::ArrayList<const Acpi::ProcessorLocalApic *> &lapics, uint8_t acpiUid);

    static LocalApicInformation *getLocalApicInfo(const Util::Data::ArrayList<LocalApicInformation *> &infos, uint8_t id);

    static IoApicInformation *getIoApicInfo(const Util::Data::ArrayList<IoApicInformation *> &infos, GlobalSystemInterrupt gsi);
};

}

#endif //HHUOS_APICACPIPARSER_H
