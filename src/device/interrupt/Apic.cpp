#include "smp_startup.h"
#include "lib/util/cpu/CpuId.h"
#include "kernel/paging/Paging.h"
#include "kernel/system/System.h"
#include "kernel/service/InterruptService.h"
#include "LocalApic.h"
#include "Apic.h"

namespace Device {

Util::Data::ArrayList<LocalApic *> Apic::localApics;
Util::Data::ArrayList<IoApic *> Apic::ioApics;
ApicTimer *Apic::apicTimer = nullptr;
ApicErrorInterruptHandler *Apic::errorHandler = nullptr;
Kernel::Logger Apic::log = Kernel::Logger::get("Apic");

bool Apic::isSupported() {
    return LocalApic::supportsXApic();
}

void Apic::ensureSupported() {
    if (!isSupported()) {
        Util::Exception::throwException(Util::Exception::UNSUPPORTED_OPERATION, "APIC support not present!");
    }
}

void Apic::initialize() {
    if (!isSupported()) {
        log.warn("APIC not supported, skippint initialization!");
        return;
    }

    if (!(Acpi::isAvailable() && Acpi::getRsdp().revision == 0)) {
        log.warn("ACPI 1.0 not available, skipping APIC initialization!");
        return;
    }

    if (isBspInitialized()) {
        log.warn("APIC already initialized, skipping initialization!");
        return;
    }

    // Get our required information from ACPI
    auto *madt = Acpi::getTable<Acpi::Madt>("APIC");
    Util::Data::ArrayList<const Acpi::ProcessorLocalApic *> acpiProcessorLocalApics;
    Util::Data::ArrayList<const Acpi::LocalApicNmi *> acpiLocalApicNmis;
    Util::Data::ArrayList<const Acpi::IoApic *> acpiIoApics;
    Util::Data::ArrayList<const Acpi::NmiSource *> acpiNmiSources;
    Util::Data::ArrayList<const Acpi::InterruptSourceOverride *> acpiInterruptSourceOverrides;
    Acpi::collectMadtStructures(&acpiProcessorLocalApics, Acpi::PROCESSOR_LOCAL_APIC);
    Acpi::collectMadtStructures(&acpiLocalApicNmis, Acpi::LOCAL_APIC_NMI);
    Acpi::collectMadtStructures(&acpiIoApics, Acpi::IO_APIC);
    Acpi::collectMadtStructures(&acpiNmiSources, Acpi::NON_MASKABLE_INTERRUPT_SOURCE);
    Acpi::collectMadtStructures(&acpiInterruptSourceOverrides, Acpi::INTERRUPT_SOURCE_OVERRIDE);

    if (acpiProcessorLocalApics.size() == 0) {
        log.warn("Didn't find any local APIC(s), skipping initialization!");
        return;
    }

    if (acpiIoApics.size() == 0) {
        log.error("Didn't find any I/O APIC(s), skipping initialization!");
        return;
    }

    // Initialize all local APICs
    auto *localPlatform = new LocalApicPlatform(madt->localApicAddress);
    for (const auto *localInfo : acpiProcessorLocalApics) {
        // Find the NMI belonging to the current localInfo
        const Acpi::LocalApicNmi *nmiInfo = nullptr;
        for (const auto *localNmi : acpiLocalApicNmis) {
            // 0xFF means all APs
            if (localNmi->acpiProcessorId == localInfo->acpiProcessorId | localNmi->acpiProcessorId == 0xFF) {
                nmiInfo = localNmi;
                break;
            }
        }

        if (nmiInfo == nullptr) {
            log.warn("Couldn't find NMI for local APIC, skipping initialization!");
            for (auto *localApic : localApics) {
                delete localApic;
            }
            localApics.clear();
            return;
        }

        localApics.add(new LocalApic(localPlatform, LocalApicInformation(localInfo, nmiInfo)));
    }

    // Initialize the local APIC of the BSP
    // Currently, we can't know the id of the BSP, but we know that it is the running processor.
    // Because the running processor can only acccess its own local APIC's registers, we can reach the
    // BSP local APIC by calling this static function, without an instance.
    uint8_t bspId = LocalApic::initializeBsp();

    // Now that we know which local APIC is the BSP, we can initialize the rest of its local APIC
    for (auto *localApic : localApics) {
        if (localApic->localInfo.id == bspId) {
            localApic->initializeApApic();
            break;
        }
    }

    // Initialize all I/O APICs
    auto *ioPlatform = new IoApicPlatform(&acpiInterruptSourceOverrides);
    for (auto *ioInfo : acpiIoApics) {
        // The Nmi is assigned to the correct I/O APIC. This is kind of a mess, because ACPI does not report
        // the maximum GSI an I/O APIC supports.

        // Find the maximum GSI of ioInfo by finding the next larger GSI base
        const Acpi::IoApic *nextIoInfo = acpiIoApics.get(0);
        for (auto *nIoInfo : acpiIoApics) {
            if (nIoInfo->globalSystemInterruptBase > ioInfo->globalSystemInterruptBase
            && nIoInfo->globalSystemInterruptBase <= nextIoInfo->globalSystemInterruptBase) {
                nextIoInfo = nIoInfo;
            }
        }

        // Select the NMI that belongs to ioInfo (if it exists)
        const Acpi::NmiSource *nmiInfo = nullptr;
        for (const auto *ioNmi : acpiNmiSources) {
            if (ioNmi->globalSystemInterrupt >= ioInfo->globalSystemInterruptBase
            && (ioNmi->globalSystemInterrupt < nextIoInfo->globalSystemInterruptBase
            || ioInfo->globalSystemInterruptBase == nextIoInfo->globalSystemInterruptBase)) {
                nmiInfo = ioNmi;
                break;
            }
        }

        ioApics.add(new IoApic(ioPlatform, IoApicInformation(ioInfo, nmiInfo)));
    }

    // Multiple I/O APICs are possible, but in the usual Intel consumer chipsets there is only one
    if (ioApics.size() > 1) {
        log.warn("Support for multiple I/O APICs is untested!");
    }

    for (auto *ioApic : ioApics) {
        ioApic->initialize();
    }

    // Local APIC error interrupt handler
    // We only require one of these, as every AP can only access its own local APIC's error register
    errorHandler = new ApicErrorInterruptHandler();
    errorHandler->plugin();
}

bool Apic::isBspInitialized() {
    bool initialized = LocalApic::isBspInitialized();
    for (uint32_t i = 0; i < ioApics.size(); ++i) {
        initialized &= ioApics.get(i)->isInitialized();
    }
    return initialized;
}

bool Apic::isSmpSupported() {
    return isBspInitialized() && localApics.size() > 1;
}

void Apic::ensureSmpSupported() {
    if (!isSmpSupported()) {
        Util::Exception::throwException(Util::Exception::UNSUPPORTED_OPERATION, "SMP support not present!");
    }
}

// TODO: This works if local APIC IDs are contiguous
// NOTE: SMP was only an afterthought for me, so the inspiration for this code comes from
// NOTE: https://github.com/acceptable-security/brackos, it has only been modified for hhuOS
void Device::Apic::initializeSmp() {
    if (!isSmpSupported()) {
        log.warn("SMP not supported, skipping initialization!");
        return;
    }

    uint8_t cpuCount = localApics.size();
    if (cpuCount > 8) {
        // This limit is pretty arbitrary, but the runningAPs bitmap currently only has 8 bits (in smp_startup.h)
        Util::Exception::throwException(Util::Exception::UNSUPPORTED_OPERATION, "CPUs with more than 8 cores are not supported!");
    }

    // Check local APIC id contiguity, just for testing
    uint8_t idBitmap = 0;
    for (uint32_t i = 0; i < cpuCount; ++i) {
        LocalApic *localApic = localApics.get(i);
        idBitmap |= (1 << localApic->localInfo.id); // Should yield 0b00000011 for 2 CPUs
    }
    uint8_t idBitmapMask = 0b11111111 >> (8 - cpuCount); // Yields 0b00000011 for 2 CPUs (0b11111111 >> 6)
    if (idBitmap != idBitmapMask) {
        Util::Exception::throwException(Util::Exception::ILLEGAL_STATE, "APIC ids are not contiguous!");
    }

    // Prepare AP boot environment
    initializeSmpStartupStacks(); // Allocates AP stack memory
    uint32_t startupCodeAddress = initializeSmpStartupCode(); // Copies the AP startup routine to 0x8000

    // Call the startup code on each AP
    for (uint32_t i = 0; i < cpuCount; ++i) {
        LocalApic *localApic = localApics.get(i);
        if (localApic->localInfo.id == LocalApic::getId() || !localApic->localInfo.enabled) {
            // Skip this AP if it's the BSP or ACPI reports it as disabled
            continue;
        }

        // Procedure taken from OsDev:
        // Send INIT IPI
        LocalApic::clearErrors();
        LocalApic::sendIpiInit(localApic->localInfo.id, ICREntry::Level::ASSERT); // Waits for delivery
        LocalApic::sendIpiInit(localApic->localInfo.id, ICREntry::Level::DEASSERT); // Waits for delivery

        for (uint32_t j = 0; j < 1000000; ++j); // Ugly wait, because we have no PIT yet

        // Send STARTUP IPI (twice)
        LocalApic::clearErrors();
        LocalApic::sendIpiStartup(localApic->localInfo.id, startupCodeAddress);
        LocalApic::clearErrors();
        LocalApic::sendIpiStartup(localApic->localInfo.id, startupCodeAddress);

        while (!(runningAPs & (1 << localApic->localInfo.id))); // Wait until AP is running
    }
}

void Device::Apic::initializeSmpStartupStacks() {
    auto &memoryService = Kernel::System::getService<Kernel::MemoryService>();
    uint8_t cpuCount = localApics.size();

    // Allocate the stackpointer array
    apStacks = reinterpret_cast<uint32_t**>(memoryService.allocateKernelMemory(sizeof(uint32_t*) * cpuCount));
    if (apStacks == nullptr) {
        Util::Exception::throwException(Util::Exception::NULL_POINTER, "Failed to allocate AP stack memory!");
    }

    // Allocate the stacks, just iterates from 0 to cpuCount - 1 because ids are contiguous
    for (uint32_t i = 0; i < cpuCount; ++i) {
        if (i == LocalApic::getId()) {
            // The BSP already has a stack, we need this entry to keep the AP stacks addressible by their ids
            apStacks[i] = nullptr;
            continue;
        }

        apStacks[i] = reinterpret_cast<uint32_t*>(memoryService.allocateKernelMemory(Kernel::Paging::PAGESIZE));
        if (apStacks[i] == nullptr) {
            Util::Exception::throwException(Util::Exception::NULL_POINTER, "Failed to allocate AP stack memory!");
        }
    }

    // TODO: Free this
}

uint32_t Device::Apic::initializeSmpStartupCode() {
    auto &memoryService = Kernel::System::getService<Kernel::MemoryService>();

    if (boot_ap_size > Kernel::Paging::PAGESIZE) {
        Util::Exception::throwException(Util::Exception::ILLEGAL_STATE, "Startup code does not fit into one page!");
    }

    // Allocate memory for copying the startup code
    void *startupCodeDestination = memoryService.allocateLowerMemory(Kernel::Paging::PAGESIZE);
    if (startupCodeDestination == nullptr) {
        Util::Exception::throwException(Util::Exception::NULL_POINTER, "Failed to allocate AP startup code memory!");
    }
    auto startupCodeDestinationPhys = reinterpret_cast<uint32_t>(memoryService.getPhysicalAddress(startupCodeDestination));
    LocalApic::log.debug("Copying AP startup routine to [0x%x] (phys)", startupCodeDestinationPhys);

    // Virtual addresses
    Util::Memory::Address<uint32_t> startupCode = Util::Memory::Address<uint32_t>(reinterpret_cast<uint32_t>(&boot_ap));
    Util::Memory::Address<uint32_t> destination = Util::Memory::Address<uint32_t>(startupCodeDestination);

    // Map the destination page to the desired physical memory location
    // memoryService.unmap(destination.get());
    // memoryService.mapPhysicalAddress(destination.get(), apStartupRoutineAddress,
    //                                  Kernel::Paging::PRESENT | Kernel::Paging::READ_WRITE);

    // Copy the startup code from smp_startup.asm to the page
    destination.copyRange(startupCode, boot_ap_size);

    // TODO: Free this

    return startupCodeDestinationPhys;
}

void Apic::ensureBspInitialized() {
    if (!isBspInitialized()) {
        Util::Exception::throwException(Util::Exception::ILLEGAL_STATE, "APIC not initialized!");
    }
}

void Apic::initializeTimer() {
    apicTimer = new Device::ApicTimer();
    apicTimer->plugin();
}

bool Apic::isTimerInitialized() {
    return apicTimer != nullptr && ApicTimer::isInitialized();
}

#if HHUOS_APIC_ENABLE_DEBUG == 1
void Apic::dumpDebugInfo() {
    if (!isBspInitialized()) {
        log.info("APIC not initialized, running in PIC mode");
        return;
    }

    log.info("Local APIC supported modes: [%s%s] (Current mode: [%s])",
             LocalApic::supportsXApic() ? "xApic" : "None",
             LocalApic::supportsX2Apic() ? ", x2Apic" : "",
             LocalApic::localPlatform->isX2Apic ? "x2Apic" : "xApic");
    log.info("Local APIC version: [0x%x]", LocalApic::getVersion());
    log.info("Local APIC xApic MMIO: ([0x%x] (phys) -> [0x%x] (virt))",
             LocalApic::localPlatform->physAddress,
             LocalApic::localPlatform->virtAddress);
    log.info("Local APIC x2Apic MSR base: [0x%x]", LocalApic::localPlatform->msrAddress);
    log.info("Local APICs:");
    for (uint32_t i = 0; i < localApics.size(); ++i) {
        LocalApic *localApic = localApics.get(i);
        log.info("- Id: [0x%x], Enabled: [%d], NMI: (LINT: [%d], Polarity: [%s], TriggerMode: [%s])",
                 localApic->localInfo.id,
                 localApic->localInfo.enabled,
                 localApic->localInfo.nmiLint,
                 localApic->localInfo.nmiPolarity == LVTEntry::PinPolarity::HIGH ? "HIGH" : "LOW",
                 localApic->localInfo.nmiTriggerMode == LVTEntry::TriggerMode::EDGE ? "EDGE" : "LEVEL");
    }
    LocalApic::dumpLVT();

    log.info("I/O APIC version: [0x%x] (EOI support: [%d])",
             IoApic::ioPlatform->version,
             IoApic::ioPlatform->directEoiSupported);
    log.info("I/O APIC max GSI: [%d]", static_cast<uint32_t>(IoApic::ioPlatform->globalMaxGsi));
    log.info("I/O APIC IRQ overrides:");
    for (uint32_t i = 0; i < IoApic::ioPlatform->overrides.size(); ++i) {
        const IoApicPlatform::IoApicIrqOverride *override = IoApic::ioPlatform->overrides.get(i);
        log.info("- Source: [%d], Target: [%d], Polarity: [%s], TriggerMode: [%s]",
                 static_cast<uint32_t>(override->source),
                 static_cast<uint32_t>(override->target),
                 override->polarity == REDTBLEntry::PinPolarity::HIGH ? "HIGH" : "LOW",
                 override->triggerMode == REDTBLEntry::TriggerMode::EDGE ? "EDGE" : "LEVEL");
    }
    log.info("I/O APICs:");
    for (uint32_t i = 0; i < ioApics.size(); ++i) {
        IoApic *ioApic = ioApics.get(i);
        log.info("- Id: [%d], GSI: (Base: [%d], Max [%d]), MMIO: ([0x%x] (phys) -> [0x%x] (virt))",
                 ioApic->ioInfo.id,
                 static_cast<uint32_t>(ioApic->ioInfo.gsiBase),
                 static_cast<uint32_t>(ioApic->ioInfo.gsiMax),
                 ioApic->ioInfo.physAddress,
                 ioApic->ioInfo.virtAddress);
        if (ioApic->ioInfo.hasNmi) {
            log.info("  NMI: (GSI: [%d], Polarity: [%s], TriggerMode: [%s])",
                     static_cast<uint32_t>(ioApic->ioInfo.nmiGsi),
                     ioApic->ioInfo.nmiPolarity == REDTBLEntry::PinPolarity::HIGH ? "HIGH" : "LOW",
                     ioApic->ioInfo.nmiTriggerMode == REDTBLEntry::TriggerMode::EDGE ? "EDGE" : "LEVEL");
        }
    }
    for (uint32_t i = 0; i < ioApics.size(); ++i) {
        IoApic *ioApic = ioApics.get(i);
        ioApic->dumpREDTBL();
    }

    log.info("Using %s for scheduling", ApicTimer::isInitialized() ? "ApicTimer" : "Pit");
}
#endif

bool Apic::isLocalInterrupt(Kernel::InterruptVector vector) {
    return vector >= Kernel::InterruptVector::CMCI && vector <= Kernel::InterruptVector::ERROR;
}

// The LocalApic::allow etc. functions are not exposed intentionally

void Apic::sendLocalEndOfInterrupt() {
    LocalApic::sendEndOfInterrupt();
}

bool Apic::isExternalInterrupt(Kernel::InterruptVector vector) {
    // Remapping can be ignored here, as all GSIs are contiguous anyway
    return static_cast<Kernel::GlobalSystemInterrupt>(vector - 32) <= IoApic::ioPlatform->globalMaxGsi;
}

void Apic::allowExternalInterrupt(InterruptRequest interruptRequest) {
    Kernel::GlobalSystemInterrupt gsi = IoApic::ioPlatform->getIoApicIrqOverrideTarget(interruptRequest);
    IoApic &ioApic = getIoApic(gsi); // Select responsible I/O APIC
    ioApic.allow(gsi);
}

void Apic::forbidExternalInterrupt(InterruptRequest interruptRequest) {
    Kernel::GlobalSystemInterrupt gsi = IoApic::ioPlatform->getIoApicIrqOverrideTarget(interruptRequest);
    IoApic &ioApic = getIoApic(gsi);
    ioApic.forbid(gsi);
}

bool Apic::externalInterruptStatus(InterruptRequest interruptRequest) {
    Kernel::GlobalSystemInterrupt gsi = IoApic::ioPlatform->getIoApicIrqOverrideTarget(interruptRequest);
    IoApic &ioApic = getIoApic(gsi);
    return ioApic.status(gsi);
}

void Apic::sendExternalEndOfInterrupt(Kernel::InterruptVector vector) {
    auto interruptRequest = static_cast<InterruptRequest>(vector - 32); // Hardware interrupt pin
    Kernel::GlobalSystemInterrupt gsi = IoApic::ioPlatform->getIoApicIrqOverrideTarget(interruptRequest);
    IoApic &ioApic = getIoApic(gsi);

    LocalApic::sendEndOfInterrupt(); // External interrupts get forwarded by the local APIC, so local EOI required
    ioApic.sendEndOfInterrupt(vector, gsi); // This is only required for level-triggered interrupts
}

IoApic &Apic::getIoApic(Kernel::GlobalSystemInterrupt gsi) {
    ensureBspInitialized();

    for (uint32_t i = 0; i < ioApics.size(); ++i) {
        IoApic *ioApic = ioApics.get(i);
        if (gsi >= ioApic->ioInfo.gsiBase && gsi <= ioApic->ioInfo.gsiMax) {
            return *ioApic;
        }
    }

    Util::Exception::throwException(Util::Exception::INVALID_ARGUMENT, "No I/O APIC found for the supplied GSI!");
}

}
