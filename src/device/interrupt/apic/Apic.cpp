#include "Apic.h"
#include "device/cpu/Smp.h"
#include "kernel/paging/Paging.h"
#include "kernel/service/InterruptService.h"
#include "kernel/system/System.h"
#include "lib/util/base/Constants.h"
#include "LocalApic.h"

namespace Device {

bool Apic::initialized = false;
bool Apic::bspTimerInitialized = false;
Util::ArrayList<LocalApic *> Apic::localApics;
Util::ArrayList<IoApic *> Apic::ioApics;
Util::ArrayList<ApicTimer *> Apic::timers;
ApicErrorHandler Apic::errorHandler = ApicErrorHandler();
Kernel::Logger Apic::log = Kernel::Logger::get("Apic");

bool Apic::isSupported() {
    // Only support ACPI 1.0, there are changes in later versions
    return LocalApic::supportsXApic() && Acpi::isAvailable() && Acpi::getRsdp().revision == 0;
}

bool Apic::isInitialized() {
    return initialized;
}

void Apic::initialize() {
    if (!LocalApic::readBaseMSR().isBSP) {
        // IA32_APIC_BASE_MSR is unique (every core has its own)
        Util::Exception::throwException(Util::Exception::UNSUPPORTED_OPERATION, "May only be called by the BSP!");
    }

    // Read information from ACPI's MADT and create our LocalApic/IoApic instances
    populateLocalApics();
    populateIoApics();

    // Initialize our local APIC, all others are only initialized when SMP is started up
    LocalApic::enableXApicMode();
    getCurrentLocalApic().initialize();

    // Multiple I/O APICs are possible, but in the usual Intel consumer chipsets there is only one
    if (ioApics.size() > 1) {
        log.warn("Support for multiple I/O APICs is untested!");
    }

    // Initialize all I/O APICs
    for (auto *ioApic : ioApics) {
        ioApic->initialize();
    }

    // We only require one error handler, as every AP can only access its own local APIC's error register
    errorHandler.plugin(); // Does not allow the interrupt!
    enableErrorHandling(); // Allows the interrupt for this AP

    if constexpr (HHUOS_APIC_ENABLE_DEBUG) {
        dumpDebugInfo();
    }

    initialized = true;
}

bool Apic::isSmpSupported() {
    return getCpuCount() > 1;
}

// This works if local APIC IDs are contiguous, but I think they always are
void Device::Apic::initializeSmp() {
    if (getCpuCount() > 64) {
        // This limit is pretty arbitrary, but the runningAPs bitmap currently only has 64 bits (in Smp.h).
        // Technically xApic supports 8-bit CPU ids, x2Apic even more (32-bit CPU ids).
        Util::Exception::throwException(Util::Exception::UNSUPPORTED_OPERATION, "CPUs with more than 64 cores are not supported!");
    }

    // Verify that the ids are contiguous. We don't take assumptions about the order of appearance though.
    // This is only here to verify some assumptions I made, based on the manuals (ACPI and IA-32), OSdev
    // and some implementations (xv6, SerenityOS).
    // This hardware specific stuff is however hard to verify, and I couldn't find a definitive answer
    // to these assumptions, so let's choose the dumb way and just abort the whole OS if they are wrong.
    uint64_t idBitmap = 0;
    for (uint32_t i = 0; i < localApics.size(); ++i) {
        LocalApic *localApic = localApics.get(i);
        idBitmap |= (1 << localApic->info.id);
    }
    const uint64_t idBitmapMask = static_cast<uint64_t>(-1) >> (64 - localApics.size());
    if (idBitmap != idBitmapMask) {
        // We require contiguous ids, because the AP stackpointer array uses the id as index
        Util::Exception::throwException(Util::Exception::ILLEGAL_STATE, "APIC ids are not contiguous!");
    }

    allocateSmpStacks();
    copySmpStartupCode();

    // Call the startup code on each AP
    // https://wiki.osdev.org/Symmetric_Multiprocessing#Initialisation_of_an_old_SMP_system
    for (uint32_t i = 0; i < getCpuCount(); ++i) {
        LocalApic *localApic = localApics.get(i);
        if (localApic->info.id == LocalApic::getId() || !localApic->info.enabled) {
            // Skip this AP if it's the BSP or ACPI reports it as disabled
            continue;
        }

        LocalApic::clearErrors();
        LocalApic::sendIpiInit(localApic->info.id, ICREntry::Level::ASSERT);
        // The deassert is required for CPUs with a discrete APIC, these do not support the STARTUP IPI,
        // and this implementation only supports xApic anyway, so this is disabled.
        // LocalApic::sendIpiInit(localApic->localInfo.id, ICREntry::Level::DEASSERT);

        // Ugly wait, because we have no PIT yet.
        // These timings are basically random, I would be surprised if this works anywhere else than in QEMU.
        for (uint32_t j = 0; j < 100000; ++j) {}

        LocalApic::clearErrors();
        LocalApic::sendIpiStartup(localApic->info.id, apStartupAddress);
        // It is not clear if the second one is required or not, so just send one.
        LocalApic::clearErrors();
        LocalApic::sendIpiStartup(localApic->info.id, apStartupAddress);

        // Wait until the AP is running, so we can continue to the next one.
        // Because we initialize the APs one at a time, runningAPs is not synchronized.
        // If the AP initialization fails (and the system doesn't crash), this will lock the BSP...
        while (!(runningAPs & (1 << localApic->info.id))) {}
    }

    // Free the startup routine page and the stackpointer array, now that all APs are running
    auto &memoryService = Kernel::System::getService<Kernel::MemoryService>();
    memoryService.freeKernelMemory(reinterpret_cast<void *>(apStacks));
    memoryService.freeKernelMemory(reinterpret_cast<void *>(apStartupAddress));
    apStacks = nullptr;
}

uint8_t Apic::getCpuCount() {
    if (!initialized) {
        Util::Exception::throwException(Util::Exception::ILLEGAL_STATE, "Uninitialized CPU count!");
    }

    return localApics.size();
}

LocalApic &Apic::getCurrentLocalApic() {
    for (uint32_t i = 0; i < localApics.size(); ++i) {
        LocalApic *localApic = localApics.get(i);
        if (localApic->info.id == LocalApic::getId()) {
            return *localApic;
        }
    }

    Util::Exception::throwException(Util::Exception::ILLEGAL_STATE, "Couldn't find local APIC for current CPU!");
}

bool Apic::isBspTimerInitialized() {
    return bspTimerInitialized;
}

void Apic::initializeTimer() {
    for (uint32_t i = 0; i < timers.size(); ++i) {
        ApicTimer *timer = timers.get(i);
        if (timer->cpuId == LocalApic::getId()) {
            Util::Exception::throwException(Util::Exception::ILLEGAL_STATE, "APIC timer for this CPU has already been initialized!");
        }
    }

    // We use multiple instances because each timer has its own timestamp
    auto *apicTimer = new Device::ApicTimer();
    apicTimer->plugin();
    timers.add(apicTimer);
    bspTimerInitialized = true; // Only used to skip the PIT preemption trigger when APIC timer is online
}

ApicTimer &Apic::getCurrentTimer() {
    for (uint32_t i = 0; i < timers.size(); ++i) {
        ApicTimer *timer = timers.get(i);
        if (timer->cpuId == LocalApic::getId()) {
            return *timer;
        }
    }

    Util::Exception::throwException(Util::Exception::ILLEGAL_STATE, "Couldn't find timer for current CPU!");
}

void Apic::enableErrorHandling() {
    // This part needs to be done for each AP
    LocalApic::allow(LocalApic::ERROR);
}

void Apic::allow(InterruptRequest interruptRequest) {
    const Kernel::GlobalSystemInterrupt gsi = IoApic::platform->getIoApicIrqOverrideTarget(interruptRequest);
    IoApic &ioApic = getIoApic(gsi); // Select responsible I/O APIC
    ioApic.allow(gsi);
}

void Apic::forbid(InterruptRequest interruptRequest) {
    const Kernel::GlobalSystemInterrupt gsi = IoApic::platform->getIoApicIrqOverrideTarget(interruptRequest);
    IoApic &ioApic = getIoApic(gsi);
    ioApic.forbid(gsi);
}

bool Apic::status(InterruptRequest interruptRequest) {
    const Kernel::GlobalSystemInterrupt gsi = IoApic::platform->getIoApicIrqOverrideTarget(interruptRequest);
    IoApic &ioApic = getIoApic(gsi);
    return ioApic.status(gsi);
}

void Apic::sendEndOfInterrupt(Kernel::InterruptVector vector) {
    if (isLocalInterrupt(vector) && vector != Kernel::InterruptVector::LINT1) {
        // Excludes LINT1 (NMI)
        LocalApic::sendEndOfInterrupt();
    } else if (isExternalInterrupt(vector)) {
        auto interruptRequest = static_cast<InterruptRequest>(vector - 32); // Hardware interrupt pin
        const Kernel::GlobalSystemInterrupt gsi = IoApic::platform->getIoApicIrqOverrideTarget(interruptRequest);
        IoApic &ioApic = getIoApic(gsi);

        LocalApic::sendEndOfInterrupt();        // External interrupts get forwarded by the local APIC, so local EOI required
        ioApic.sendEndOfInterrupt(vector, gsi); // This is only required for level-triggered interrupts
    }
}

bool Apic::isLocalInterrupt(Kernel::InterruptVector vector) {
    return vector >= Kernel::InterruptVector::CMCI && vector <= Kernel::InterruptVector::ERROR;
}

bool Apic::isExternalInterrupt(Kernel::InterruptVector vector) {
    // Remapping can be ignored here, as all GSIs are contiguous anyway
    return static_cast<Kernel::GlobalSystemInterrupt>(vector - 32) <= IoApic::platform->globalMaxGsi;
}

void Apic::populateLocalApics() {
    // Get our required information from ACPI
    const auto *madt = Acpi::getTable<Acpi::Madt>("APIC");
    Util::ArrayList<const Acpi::ProcessorLocalApic *> acpiProcessorLocalApics;
    Util::ArrayList<const Acpi::LocalApicNmi *> acpiLocalApicNmis;
    Acpi::collectMadtStructures(&acpiProcessorLocalApics, Acpi::PROCESSOR_LOCAL_APIC);
    Acpi::collectMadtStructures(&acpiLocalApicNmis, Acpi::LOCAL_APIC_NMI);

    if (acpiProcessorLocalApics.size() == 0) {
        Util::Exception::throwException(Util::Exception::ILLEGAL_STATE, "Didn't find any local APIC(s)!");
    }

    // Create LocalApic instances
    auto *localPlatform = new LocalApicPlatform(madt->localApicAddress);
    for (const auto *localInfo : acpiProcessorLocalApics) {
        // Find the NMI belonging to the current localInfo
        const Acpi::LocalApicNmi *nmiInfo = nullptr;
        for (const auto *localNmi : acpiLocalApicNmis) {
            // 0xFF means all APs
            if ((localNmi->acpiProcessorId == localInfo->acpiProcessorId) | (localNmi->acpiProcessorId == 0xFF)) {
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
}

void Apic::populateIoApics() {
    // Get our required information from ACPI
    const auto *madt = Acpi::getTable<Acpi::Madt>("APIC");
    Util::ArrayList<const Acpi::IoApic *> acpiIoApics;
    Util::ArrayList<const Acpi::NmiSource *> acpiNmiSources;
    Util::ArrayList<const Acpi::InterruptSourceOverride *> acpiInterruptSourceOverrides;
    Acpi::collectMadtStructures(&acpiIoApics, Acpi::IO_APIC);
    Acpi::collectMadtStructures(&acpiNmiSources, Acpi::NON_MASKABLE_INTERRUPT_SOURCE);
    Acpi::collectMadtStructures(&acpiInterruptSourceOverrides, Acpi::INTERRUPT_SOURCE_OVERRIDE);

    if (acpiIoApics.size() == 0) {
        Util::Exception::throwException(Util::Exception::ILLEGAL_STATE, "Didn't find any I/O APIC(s)!");
    }

    // Create IoApic instances
    auto *ioPlatform = new IoApicPlatform(&acpiInterruptSourceOverrides);
    for (const auto *ioInfo : acpiIoApics) {
        // The Nmi is assigned to the correct I/O APIC. This is a mess, because ACPI does not report
        // the maximum GSI an I/O APIC supports.

        // Find the maximum GSI of ioInfo by finding the next larger GSI base
        const Acpi::IoApic *nextIoInfo = acpiIoApics.get(0);
        for (const auto *nIoInfo : acpiIoApics) {
            if (nIoInfo->globalSystemInterruptBase > ioInfo->globalSystemInterruptBase && nIoInfo->globalSystemInterruptBase <= nextIoInfo->globalSystemInterruptBase) {
                nextIoInfo = nIoInfo;
            }
        }

        // Select the NMI that belongs to ioInfo (if it exists)
        const Acpi::NmiSource *nmiInfo = nullptr;
        for (const auto *ioNmi : acpiNmiSources) {
            if (ioNmi->globalSystemInterrupt >= ioInfo->globalSystemInterruptBase && (ioNmi->globalSystemInterrupt < nextIoInfo->globalSystemInterruptBase || ioInfo->globalSystemInterruptBase == nextIoInfo->globalSystemInterruptBase)) {
                nmiInfo = ioNmi;
                break;
            }
        }

        ioApics.add(new IoApic(ioPlatform, IoApicInformation(ioInfo, nmiInfo)));
    }
}

void Device::Apic::allocateSmpStacks() {
    auto &memoryService = Kernel::System::getService<Kernel::MemoryService>();

    // Allocate the stackpointer array
    apStacks = reinterpret_cast<uint32_t **>(memoryService.allocateKernelMemory(sizeof(uint32_t *) * getCpuCount()));
    if (apStacks == nullptr) {
        Util::Exception::throwException(Util::Exception::NULL_POINTER, "Failed to allocate AP stack memory!");
    }

    // Allocate the stacks, just iterates from 0 to cpuCount - 1 because ids are contiguous (we assume)
    for (uint32_t i = 0; i < getCpuCount(); ++i) {
        if (i == LocalApic::getId()) {
            // The BSP already has a stack, we need this entry to keep the AP stacks addressible by their ids
            apStacks[i] = nullptr;
            continue;
        }

        apStacks[i] = reinterpret_cast<uint32_t *>(memoryService.allocateKernelMemory(apStackSize));
        if (apStacks[i] == nullptr) {
            Util::Exception::throwException(Util::Exception::NULL_POINTER, "Failed to allocate AP stack memory!");
        }
    }
}

void Device::Apic::copySmpStartupCode() {
    if (boot_ap_size > Util::PAGESIZE) {
        Util::Exception::throwException(Util::Exception::ILLEGAL_STATE, "Startup code does not fit into one page!");
    }

    // Allocate physical memory for copying the startup routine
    auto &memoryService = Kernel::System::getService<Kernel::MemoryService>();
    void *startupCodeMemory = memoryService.mapIO(apStartupAddress, Util::PAGESIZE);

    // Identity map the allocated physical memory to the kernel address space
    // (Seems to be required to switch to protected mode with enabled paging)
    memoryService.unmap(reinterpret_cast<uint32_t>(startupCodeMemory));
    memoryService.mapPhysicalAddress(apStartupAddress, apStartupAddress,
                                     Kernel::Paging::PRESENT | Kernel::Paging::READ_WRITE);

    // Sanity check because am dumb
    if (reinterpret_cast<uint32_t>(memoryService.getPhysicalAddress(reinterpret_cast<void *>(apStartupAddress))) != apStartupAddress) {
        Util::Exception::throwException(Util::Exception::ILLEGAL_STATE, "Failed to identity map startup code memory!");
    }

    // Virtual addresses
    const Util::Address<uint32_t> startupCode = Util::Address<uint32_t>(reinterpret_cast<uint32_t>(&boot_ap));
    const Util::Address<uint32_t> destination = Util::Address<uint32_t>(apStartupAddress);

    // Prepare the empty variables in the startup routine at their original location
    asm volatile("sgdt %0"
                 : "=m"(boot_ap_gdtr));
    asm volatile("sidt %0"
                 : "=m"(boot_ap_idtr));
    asm volatile("mov %%cr0, %%eax;"
                 : "=a"(boot_ap_cr0));
    asm volatile("mov %%cr3, %%eax;"
                 : "=a"(boot_ap_cr3));
    asm volatile("mov %%cr4, %%eax;"
                 : "=a"(boot_ap_cr4));
    boot_ap_stacks = reinterpret_cast<uint32_t>(apStacks);
    boot_ap_entry = reinterpret_cast<uint32_t>(&smpEntry);

    // Copy the startup routine and prepared variables to the identity mapped page
    log.debug("Copying AP startup routine from [0x%x] (virt) to [0x%x] (phys)", reinterpret_cast<uint32_t>(&boot_ap), apStartupAddress);
    destination.copyRange(startupCode, boot_ap_size);
}

IoApic &Apic::getIoApic(Kernel::GlobalSystemInterrupt gsi) {
    for (uint32_t i = 0; i < ioApics.size(); ++i) {
        IoApic *ioApic = ioApics.get(i);
        if (gsi >= ioApic->info.gsiBase && gsi <= ioApic->info.gsiMax) {
            return *ioApic;
        }
    }

    Util::Exception::throwException(Util::Exception::INVALID_ARGUMENT, "No I/O APIC found for the supplied GSI!");
}

void Apic::dumpDebugInfo() {
    log.info("Local APIC supported modes: [%s%s] (Current mode: [%s])",
             LocalApic::supportsXApic() ? "xApic" : "None",
             LocalApic::supportsX2Apic() ? ", x2Apic" : "",
             LocalApic::platform->isX2Apic ? "x2Apic" : "xApic");
    log.info("Local APIC version: [0x%x]", LocalApic::getVersion());
    log.info("Local APIC xApic MMIO: ([0x%x] (phys) -> [0x%x] (virt))",
             LocalApic::platform->physAddress,
             LocalApic::platform->virtAddress);
    log.info("Local APIC x2Apic MSR base: [0x%x]", LocalApic::platform->msrAddress);
    log.info("Local APICs:");
    for (uint32_t i = 0; i < localApics.size(); ++i) {
        LocalApic *localApic = localApics.get(i);
        log.info("- Id: [0x%x], Enabled: [%d], NMI: (LINT: [%d], Polarity: [%s], TriggerMode: [%s])",
                 localApic->info.id,
                 static_cast<int>(localApic->info.enabled),
                 localApic->info.nmiLint,
                 localApic->info.nmiPolarity == LVTEntry::PinPolarity::HIGH ? "HIGH" : "LOW",
                 localApic->info.nmiTriggerMode == LVTEntry::TriggerMode::EDGE ? "EDGE" : "LEVEL");
    }
    LocalApic::dumpLVT();

    log.info("I/O APIC version: [0x%x] (EOI support: [%d])",
             IoApic::platform->version,
             static_cast<int>(IoApic::platform->directEoiSupported));
    log.info("I/O APIC max GSI: [%d]", static_cast<uint32_t>(IoApic::platform->globalMaxGsi));
    log.info("I/O APIC IRQ overrides:");
    for (uint32_t i = 0; i < IoApic::platform->overrides.size(); ++i) {
        const IoApicPlatform::IoApicIrqOverride *override = IoApic::platform->overrides.get(i);
        log.info("- Source: [%d], Target: [%d], Polarity: [%s], TriggerMode: [%s]",
                 static_cast<uint32_t>(override->source),
                 static_cast<uint32_t>(override->target),
                 override->polarity == REDTBLEntry::PinPolarity::HIGH ? "HIGH" : (override->polarity == REDTBLEntry::PinPolarity::LOW ? "LOW" : "BUS"),
                 override->triggerMode == REDTBLEntry::TriggerMode::EDGE ? "EDGE" : (override->triggerMode == REDTBLEntry::TriggerMode::LEVEL ? "LEVEL" : "BUS"));
    }
    log.info("I/O APICs:");
    for (uint32_t i = 0; i < ioApics.size(); ++i) {
        IoApic *ioApic = ioApics.get(i);
        log.info("- Id: [%d], GSI: (Base: [%d], Max [%d]), MMIO: ([0x%x] (phys) -> [0x%x] (virt))",
                 ioApic->info.id,
                 static_cast<uint32_t>(ioApic->info.gsiBase),
                 static_cast<uint32_t>(ioApic->info.gsiMax),
                 ioApic->info.physAddress,
                 ioApic->info.virtAddress);
        if (ioApic->info.hasNmi) {
            log.info("  NMI: (GSI: [%d], Polarity: [%s], TriggerMode: [%s])",
                     static_cast<uint32_t>(ioApic->info.nmiGsi),
                     ioApic->info.nmiPolarity == REDTBLEntry::PinPolarity::HIGH ? "HIGH" : "LOW",
                     ioApic->info.nmiTriggerMode == REDTBLEntry::TriggerMode::EDGE ? "EDGE" : "LEVEL");
        }
    }
    for (uint32_t i = 0; i < ioApics.size(); ++i) {
        IoApic *ioApic = ioApics.get(i);
        ioApic->dumpREDTBL();
    }
}

} // namespace Device
