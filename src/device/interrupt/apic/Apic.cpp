#include "Apic.h"
#include "device/cpu/Smp.h"
#include "device/power/acpi/Acpi.h"
#include "device/time/Cmos.h"
#include "device/time/Pit.h"
#include "filesystem/memory/MemoryDriver.h"
#include "filesystem/memory/MemoryFileNode.h"
#include "kernel/paging/Paging.h"
#include "kernel/service/FilesystemService.h"
#include "kernel/service/InterruptService.h"
#include "kernel/system/System.h"
#include "lib/util/base/Constants.h"
#include "LocalApic.h"

namespace Device {

bool Apic::apicEnabled = false;
bool Apic::smpEnabled = false;
Util::ArrayList<LocalApic *> Apic::localApics;
Util::ArrayList<IoApic *> Apic::ioApics;
Util::ArrayList<ApicTimer *> Apic::timers;
ApicErrorHandler Apic::errorHandler = ApicErrorHandler();
Kernel::Logger Apic::log = Kernel::Logger::get("Apic");

bool Apic::isSupported() {
    // Only support ACPI 1.0 fully, there are changes in later versions, but it should still work, so don't force.
    return LocalApic::supportsXApic() && Acpi::isAvailable(); // && Acpi::getRsdp().revision == 0;
}

bool Apic::isEnabled() {
    return apicEnabled;
}

void Apic::enable() {
    if (apicEnabled) {
        Util::Exception::throwException(Util::Exception::ILLEGAL_STATE, "Already initialized!");
    }

    if (!LocalApic::readBaseMSR().isBSP) {
        // IA32_APIC_BASE_MSR is unique (every core has its own)
        Util::Exception::throwException(Util::Exception::UNSUPPORTED_OPERATION, "May only be called by the BSP!");
    }

    // Read information from ACPI's MADT and create our LocalApic/IoApic instances
    populateLocalApics();
    populateIoApics();

    // Initialize our local APIC, all others are only initialized when SMP is started up
    LocalApic::enableXApicMode();
    apicEnabled = true; // Now everything is ready for normal operation
    initializeCurrentLocalApic();

    // Multiple I/O APICs are possible, but in the usual Intel consumer chipsets there is only one
    if (ioApics.size() > 1) {
        log.warn("Support for multiple I/O APICs is untested!");
    }

    // Initialize all I/O APICs
    for (auto *ioApic : ioApics) {
        ioApic->initialize();
    }

    // We only require one error handler, as every AP can only access its own local APIC's error register
    errorHandler.plugin();       // Does not allow the interrupt!
    enableCurrentErrorHandler(); // Allows the interrupt for this AP

    // In contrast to the errorHandler, there are multiple timers in multicore systems, because they keep
    // track of the "core-local" time.
    ApicTimer::calibrate();
    startCurrentTimer();
}

void Apic::mountDeviceNodes() {
    if (!apicEnabled) {
        Util::Exception::throwException(Util::Exception::ILLEGAL_STATE, "Not initialized!");
    }

    auto &filesystemService = Kernel::System::getService<Kernel::FilesystemService>();
    auto &driver = filesystemService.getFilesystem().getVirtualDriver("/device");

    auto *localApicNode = new Filesystem::Memory::MemoryFileNode("lapic");
    auto *ioApicNode = new Filesystem::Memory::MemoryFileNode("ioapic");
    auto *lvtNode = new Filesystem::Memory::MemoryFileNode("lvt");
    auto *redtblNode = new Filesystem::Memory::MemoryFileNode("redtbl");

    Util::String lapic, ioapic, lvt, redtbl;
    printLocalApics(lapic);
    printIoApics(ioapic);
    LocalApic::printLvt(lvt);
    (*ioApics.begin())->printRedtbl(redtbl);

    localApicNode->writeData(static_cast<const uint8_t *>(lapic), 0, lapic.length());
    ioApicNode->writeData(static_cast<const uint8_t *>(ioapic), 0, ioapic.length());
    lvtNode->writeData(static_cast<const uint8_t *>(lvt), 0, lvt.length());
    redtblNode->writeData(static_cast<const uint8_t *>(redtbl), 0, redtbl.length());

    filesystemService.createDirectory("/device/apic");
    driver.addNode("/apic/", localApicNode);
    driver.addNode("/apic/", ioApicNode);
    driver.addNode("/apic/", lvtNode);
    driver.addNode("/apic/", redtblNode);
}

bool Apic::isSmpSupported() {
    return getCpuCount() > 1;
}

void Device::Apic::startupSmp() {
    if (smpEnabled) {
        Util::Exception::throwException(Util::Exception::ILLEGAL_STATE, "Already initialized!");
    }
    if (!apicEnabled) {
        Util::Exception::throwException(Util::Exception::ILLEGAL_STATE, "APIC not initialized!");
    }
    if (getCpuCount() > 64) {
        // This limit is pretty arbitrary, but the runningAPs bitmap currently only has 64 bits (in Smp.h).
        // Technically xApic supports 8-bit CPU ids though, x2Apic even more (32-bit CPU ids).
        Util::Exception::throwException(Util::Exception::UNSUPPORTED_OPERATION, "CPUs with more than 64 cores are not supported!");
    }

    void *apStacks = prepareApStacks();
    void *apStartupCode = prepareApStartupCode(apStacks);
    void *apWarmReset = prepareApWarmReset(); // This is technically only required for discrete APIC, see below

    // Call the startup code on each AP using the SIPI
    for (uint32_t i = 0; i < getCpuCount(); ++i) {
        const LocalApic &localApic = *localApics.get(i);
        if (localApic.cpuId == LocalApic::getId()) {
            // Skip this AP if it's the BSP (disabled processors won't even show up in this list)
            continue;
        }

        // Info on discrete APIC:
        // The INIT IPI is required for CPUs with a discrete APIC, these ignore the STARTUP IPI.
        // For these CPUs, the startup routines address has to be written to the BIOS memory segment
        // (warm reset vector), and the AP has to be configured for warm-reset to start executing there.
        // This is unused for xApic. The INIT IPI is still issued though, to follow the IA-32 manual's
        // "INIT-SIPI-SIPI" sequence and the "universal startup algorithm" (MPSpec, sec. B.4):
        LocalApic::clearErrors();
        LocalApic::sendIpiInit(localApic.cpuId, ICREntry::Level::ASSERT);   // Level-triggered, needs to be...
        LocalApic::waitForIpiDispatch();                                     // xv6 waits 200 us instead.
        LocalApic::sendIpiInit(localApic.cpuId, ICREntry::Level::DEASSERT); // ...deasserted manually
        LocalApic::waitForIpiDispatch();                                     // Not necessary with 10ms delay
        Pit::earlyDelay(10'000);                                             // 10 ms, xv6 waits 100 us instead.

        // Issue the SIPI twice (for xApic):
        for (uint8_t j = 0; j < 2; ++j) {
            LocalApic::clearErrors();
            LocalApic::sendIpiStartup(localApic.cpuId, apStartupAddress);
            LocalApic::waitForIpiDispatch();
            Pit::earlyDelay(200); // 200 us
        }

        // Wait until the AP marks it is running, so we can continue to the next one.
        // Because we initialize the APs one at a time, runningAPs is not synchronized.
        // If the AP initialization fails (and the system doesn't crash), this will lock the BSP,
        // the same will happen if the SIPI does not reach its target. That's why we abort.
        // Because the systemtime is not yet functional, we delay to measure the ~ time.
        uint32_t readCount = 0;
        while (!(runningAPs & (1 << localApic.cpuId))) {
            if (readCount > 100) {
                // Waited 100 * 10 ms = 1 s in total (arbitrary, could be way shorter)
                log.error("CPU [%d] didn't phone home, it could be in undefined state!", i);
                break;
            }
            Pit::earlyDelay(10'000); // 10 ms
            readCount++;
        }
    }

    // Free the startup routine page, stackpointer array and warm-reset vector memory,
    // now that all APs are running. Keep the stacks though!
    delete apStacks;
    delete apStartupCode;
    delete apWarmReset;

    smpEnabled = true;
}

void Apic::initializeCurrentLocalApic() {
    if (!apicEnabled) {
        Util::Exception::throwException(Util::Exception::ILLEGAL_STATE, "APIC not initialized!");
    }
    LocalApic &localApic = getCurrentLocalApic();
    if (localApic.initialized) {
        Util::Exception::throwException(Util::Exception::ILLEGAL_STATE, "Already initialized!");
    }
    if (localApic.cpuId != LocalApic::getId()) {
        Util::Exception::throwException(Util::Exception::ILLEGAL_STATE, "AP can only enable itself!");
    }
    localApic.initialize();
}

uint8_t Apic::getCpuCount() {
    if (!apicEnabled) {
        Util::Exception::throwException(Util::Exception::ILLEGAL_STATE, "Uninitialized CPU count!");
    }

    return localApics.size();
}

LocalApic &Apic::getCurrentLocalApic() {
    for (uint32_t i = 0; i < localApics.size(); ++i) {
        LocalApic &localApic = *localApics.get(i);
        if (localApic.cpuId == LocalApic::getId()) {
            return localApic;
        }
    }

    Util::Exception::throwException(Util::Exception::ILLEGAL_STATE, "Couldn't find local APIC for current CPU!");
}

bool Apic::isCurrentTimerRunning() {
    for (uint32_t i = 0; i < timers.size(); ++i) {
        const ApicTimer &timer = *timers.get(i);
        if (timer.cpuId == LocalApic::getId()) {
            return true;
        }
    }

    return false;
}

void Apic::startCurrentTimer() {
    if (!apicEnabled) {
        Util::Exception::throwException(Util::Exception::ILLEGAL_STATE, "APIC not initialized!");
    }
    if (isCurrentTimerRunning()) {
        Util::Exception::throwException(Util::Exception::ILLEGAL_STATE, "APIC timer for this CPU has already been initialized!");
    }

    // We use multiple instances because each timer has its own timestamp
    auto *apicTimer = new Device::ApicTimer();
    apicTimer->plugin(); // Multiple invocations register multiple handlers to the APICTIMER vector
    timers.add(apicTimer);
}

ApicTimer &Apic::getCurrentTimer() {
    for (uint32_t i = 0; i < timers.size(); ++i) {
        ApicTimer &timer = *timers.get(i);
        if (timer.cpuId == LocalApic::getId()) {
            return timer;
        }
    }

    Util::Exception::throwException(Util::Exception::ILLEGAL_STATE, "Couldn't find timer for current CPU!");
}

void Apic::enableCurrentErrorHandler() {
    if (!apicEnabled) {
        Util::Exception::throwException(Util::Exception::ILLEGAL_STATE, "APIC not initialized!");
    }
    // This part needs to be done for each AP
    LocalApic::allow(LocalApic::ERROR);
}

void Apic::allow(InterruptRequest interruptRequest) {
    if (!apicEnabled) {
        Util::Exception::throwException(Util::Exception::ILLEGAL_STATE, "Already initialized!");
    }

    const IoApic::IrqOverride *override = IoApic::getOverride(interruptRequest);
    const Kernel::GlobalSystemInterrupt gsi = override == nullptr
                                              ? static_cast<Kernel::GlobalSystemInterrupt>(interruptRequest)
                                              : override->target;
    IoApic &ioApic = getIoApic(gsi); // Select responsible I/O APIC
    if (ioApic.isNonMaskableInterrupt(gsi)) {
        Util::Exception::throwException(Util::Exception::INVALID_ARGUMENT, "GSI is non-maskable!");
    }
    ioApic.allow(gsi);
}

void Apic::forbid(InterruptRequest interruptRequest) {
    if (!apicEnabled) {
        Util::Exception::throwException(Util::Exception::ILLEGAL_STATE, "Already initialized!");
    }

    const IoApic::IrqOverride *override = IoApic::getOverride(interruptRequest);
    const Kernel::GlobalSystemInterrupt gsi = override == nullptr
                                              ? static_cast<Kernel::GlobalSystemInterrupt>(interruptRequest)
                                              : override->target;
    IoApic &ioApic = getIoApic(gsi);
    if (ioApic.isNonMaskableInterrupt(gsi)) {
        Util::Exception::throwException(Util::Exception::INVALID_ARGUMENT, "GSI is non-maskable!");
    }
    ioApic.forbid(gsi);
}

bool Apic::status(InterruptRequest interruptRequest) {
    if (!apicEnabled) {
        Util::Exception::throwException(Util::Exception::ILLEGAL_STATE, "Already initialized!");
    }

    const IoApic::IrqOverride *override = IoApic::getOverride(interruptRequest);
    const Kernel::GlobalSystemInterrupt gsi = override == nullptr
                                              ? static_cast<Kernel::GlobalSystemInterrupt>(interruptRequest)
                                              : override->target;
    IoApic &ioApic = getIoApic(gsi);
    return ioApic.status(gsi);
}

void Apic::sendEndOfInterrupt(Kernel::InterruptVector vector) {
    if (!apicEnabled) {
        Util::Exception::throwException(Util::Exception::ILLEGAL_STATE, "Already initialized!");
    }

    if (isLocalInterrupt(vector) && vector != Kernel::InterruptVector::LINT1) {
        // Excludes NMI, IPIs and SMIs are also excluded, but these don't have vector numbers,
        // so they won't reach this anyway.
        LocalApic::sendEndOfInterrupt();
    } else if (isExternalInterrupt(vector)) {
        // Edge-triggered external interrupts have to be EOId in the local APIC,
        // level-triggered external interrupts can EOId in the local APIC if EOI-broadcasting is enabled,
        // otherwise they can be directly EOId in the I/O APIC by using the I/O APICs EOI register or
        // masking them and setting them as edge-triggered temporarily (which clears the remote IRR bit).
        // Here, EOI broadcasting is enabled, which makes it very simple:
        LocalApic::sendEndOfInterrupt(); // External interrupts get forwarded by the local APIC, so local EOI required
    }
}

bool Apic::isLocalInterrupt(Kernel::InterruptVector vector) {
    if (!apicEnabled) {
        Util::Exception::throwException(Util::Exception::ILLEGAL_STATE, "APIC not initialized!");
    }
    return vector >= Kernel::InterruptVector::CMCI && vector <= Kernel::InterruptVector::ERROR;
}

bool Apic::isExternalInterrupt(Kernel::InterruptVector vector) {
    if (!apicEnabled) {
        Util::Exception::throwException(Util::Exception::ILLEGAL_STATE, "APIC not initialized!");
    }
    // Remapping can be ignored here, as all GSIs are contiguous anyway
    return static_cast<Kernel::GlobalSystemInterrupt>(vector - 32) <= IoApic::systemGsiMax;
}

void Apic::populateLocalApics() {
    // Get our required information from ACPI
    const auto *madt = Acpi::getTable<Acpi::Madt>("APIC");
    Util::ArrayList<const Acpi::ProcessorLocalApic *> acpiProcessorLocalApics;
    Util::ArrayList<const Acpi::LocalApicNmi *> acpiLocalApicNmis;
    Acpi::collectMadtStructures(acpiProcessorLocalApics, Acpi::PROCESSOR_LOCAL_APIC);
    Acpi::collectMadtStructures(acpiLocalApicNmis, Acpi::LOCAL_APIC_NMI);

    if (acpiProcessorLocalApics.size() == 0) {
        Util::Exception::throwException(Util::Exception::ILLEGAL_STATE, "Didn't find any local APIC(s)!");
    }

    // Create LocalApic instances
    for (const auto *localInfo : acpiProcessorLocalApics) {
        if (!(localInfo->flags & 0x1)) {
            // When ACPI reports this local APIC as disabled, it may not be used by the OS.
            // ACPI 1.0 specification, sec. 5.2.8.1
            continue;
        }

        // Find the NMI belonging to the current localInfo, every local APIC should have exactly one
        const Acpi::LocalApicNmi *nmiInfo = nullptr;
        for (const auto *localNmi : acpiLocalApicNmis) {
            // 0xFF means all APs
            if ((localNmi->acpiProcessorId == localInfo->acpiProcessorId) | (localNmi->acpiProcessorId == 0xFF)) {
                nmiInfo = localNmi;
                break;
            }
        }
        if (nmiInfo == nullptr) {
            Util::Exception::throwException(Util::Exception::ILLEGAL_STATE, "Couldn't find NMI for local APIC!");
        }

        localApics.add(new LocalApic(localInfo->apicId, madt->localApicAddress,
                                     nmiInfo->localApicLint == 0 ? LocalApic::LINT0 : LocalApic::LINT1,
                                     nmiInfo->flags & Acpi::IntiFlag::ACTIVE_HIGH ? LVTEntry::PinPolarity::HIGH : LVTEntry::PinPolarity::LOW,
                                     nmiInfo->flags & Acpi::IntiFlag::EDGE_TRIGGERED ? LVTEntry::TriggerMode::EDGE : LVTEntry::TriggerMode::LEVEL));
    }

    log.info("Found [%d] CPUs of which [%d] are usable.", acpiProcessorLocalApics.size(), localApics.size());
}

void Apic::populateIoApics() {
    // Get our required information from ACPI
    Util::ArrayList<const Acpi::IoApic *> acpiIoApics;
    Util::ArrayList<const Acpi::NmiSource *> acpiNmiSources;
    Util::ArrayList<const Acpi::InterruptSourceOverride *> acpiInterruptSourceOverrides;
    Acpi::collectMadtStructures(acpiIoApics, Acpi::IO_APIC);
    Acpi::collectMadtStructures(acpiNmiSources, Acpi::NON_MASKABLE_INTERRUPT_SOURCE);
    Acpi::collectMadtStructures(acpiInterruptSourceOverrides, Acpi::INTERRUPT_SOURCE_OVERRIDE);

    if (acpiIoApics.size() == 0) {
        Util::Exception::throwException(Util::Exception::ILLEGAL_STATE, "Didn't find any I/O APIC(s)!");
    }

    // Create IoApic instances
    for (const auto *ioInfo : acpiIoApics) {
        auto *ioApic = new IoApic(ioInfo->ioApicId, ioInfo->ioApicAddress,
                                  static_cast<Kernel::GlobalSystemInterrupt>(ioInfo->globalSystemInterruptBase));

        // Add all NMIs that belong to this I/O APIC
        const uint32_t maxGsi = getIoApicMaxGsi(*ioInfo, acpiIoApics);
        for (const auto *nmi : acpiNmiSources) {
            if (maxGsi == ioInfo->globalSystemInterruptBase
                || (nmi->globalSystemInterrupt >= ioInfo->globalSystemInterruptBase
                    && nmi->globalSystemInterrupt <= maxGsi)) {
                // The first condition is valid for a single I/O APIC
                ioApic->addNonMaskableInterrupt(static_cast<Kernel::GlobalSystemInterrupt>(nmi->globalSystemInterrupt),
                                                nmi->flags & Acpi::IntiFlag::ACTIVE_HIGH ? REDTBLEntry::PinPolarity::HIGH : REDTBLEntry::PinPolarity::LOW,
                                                nmi->flags & Acpi::IntiFlag::EDGE_TRIGGERED ? REDTBLEntry::TriggerMode::EDGE : REDTBLEntry::TriggerMode::LEVEL);
            }
        }

        ioApics.add(ioApic);
    }

    // Add the IRQ overrides
    for (const auto *override : acpiInterruptSourceOverrides) {
        // ISA bus default values
        REDTBLEntry::PinPolarity polarity = REDTBLEntry::PinPolarity::HIGH;
        REDTBLEntry::TriggerMode trigger = REDTBLEntry::TriggerMode::EDGE;

        if ((override->flags & 0x3) != 0 && (override->flags & Acpi::IntiFlag::ACTIVE_LOW)) {
            // If flags[0:1] is 0, the bus default is used
            polarity = REDTBLEntry::PinPolarity::LOW;
        }

        if ((override->flags & 0xC) != 0 && (override->flags & Acpi::IntiFlag::LEVEL_TRIGGERED)) {
            // If flags[2:3] is 0, the bus default is used
            trigger = REDTBLEntry::TriggerMode::LEVEL;
        }

        IoApic::addIrqOverride(static_cast<InterruptRequest>(override->source),
                               static_cast<Kernel::GlobalSystemInterrupt>(override->globalSystemInterrupt),
                               polarity, trigger);
    }
}

void *Apic::prepareApStacks() {
    auto &memoryService = Kernel::System::getService<Kernel::MemoryService>();

    // Allocate the stackpointer array
    auto **apStacks = reinterpret_cast<uint32_t **>(memoryService.allocateKernelMemory(sizeof(uint32_t *) * getCpuCount()));
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

    return reinterpret_cast<void *>(apStacks);
}

void *Apic::prepareApStartupCode(void *apStacks) {
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
    // log.info("Copying AP startup routine from [0x%x] (virt) to [0x%x] (phys)", reinterpret_cast<uint32_t>(&boot_ap), apStartupAddress);
    destination.copyRange(startupCode, boot_ap_size);

    return reinterpret_cast<void *>(apStartupAddress);
}

void *Apic::prepareApWarmReset() {
    Cmos::write(0xF, 0x0A); // Shutdown status byte (MPSpec, sec. B.4)

    auto &memoryService = Kernel::System::getService<Kernel::MemoryService>();
    const uint32_t wrvPhys = 0x40 << 4 | 0x67; // MPSpec, sec. B.4
    void *warmResetVector = memoryService.mapIO(wrvPhys, Util::PAGESIZE);

    // Account for possible misalignment, as mapIO returns a page-aligned pointer
    const uint32_t pageOffset = wrvPhys % Util::PAGESIZE;
    const uint32_t wrvVirt = reinterpret_cast<uint32_t>(warmResetVector) + pageOffset;

    *reinterpret_cast<volatile uint16_t *>(wrvVirt) = apStartupAddress;

    return reinterpret_cast<void *>(wrvVirt);
}

IoApic &Apic::getIoApic(Kernel::GlobalSystemInterrupt gsi) {
    for (uint32_t i = 0; i < ioApics.size(); ++i) {
        IoApic &ioApic = *ioApics.get(i);
        if (gsi >= ioApic.gsiBase && gsi <= ioApic.gsiMax) {
            return ioApic;
        }
    }

    Util::Exception::throwException(Util::Exception::INVALID_ARGUMENT, "No I/O APIC found for the supplied GSI!");
}

Kernel::GlobalSystemInterrupt Apic::getIoApicMaxGsi(const Acpi::IoApic &ioInfo,
                                                    const Util::ArrayList<const Acpi::IoApic *> &acpiIoApics) {
    if (acpiIoApics.size() == 1) {
        return static_cast<Kernel::GlobalSystemInterrupt>(acpiIoApics.get(0)->globalSystemInterruptBase);
    }

    // Find the maximum GSI of ioInfo by finding the next larger GSI base
    const Acpi::IoApic *nextIoInfo = acpiIoApics.get(1);
    for (const auto *nIoInfo : acpiIoApics) {
        if (nIoInfo->globalSystemInterruptBase > ioInfo.globalSystemInterruptBase
            && nIoInfo->globalSystemInterruptBase <= nextIoInfo->globalSystemInterruptBase) {
            nextIoInfo = nIoInfo;
        }
    }
    return static_cast<Kernel::GlobalSystemInterrupt>(nextIoInfo->globalSystemInterruptBase - 1);
}

void Apic::printLocalApics(Util::String &string) {
    string += Util::String::format("Local APIC supported modes: [%s%s] (Current mode: [xApic])\n",
                                   LocalApic::supportsXApic() ? "xApic" : "None",
                                   LocalApic::supportsX2Apic() ? ", x2Apic" : "");
    string += Util::String::format("Local APIC version: [0x%x]\n", LocalApic::getVersion());
    string += Util::String::format("Local APIC xApic MMIO: [0x%x] (phys) -> [0x%x] (virt)\n",
                                   LocalApic::baseAddress,
                                   LocalApic::mmioAddress);

    string += "\nLocal APICs:\n";
    for (uint32_t i = 0; i < localApics.size(); ++i) {
        const LocalApic &localApic = *localApics.get(i);
        string += Util::String::format("Id: [0x%x], Running: [%d], NMI: (LINT: [%d], Polarity: [%s], Trigger: [%s])\n",
                                       localApic.cpuId,
                                       localApic.initialized,
                                       localApic.nmiLint - LocalApic::LINT0,
                                       localApic.nmiPolarity == LVTEntry::PinPolarity::HIGH ? "HIGH" : "LOW",
                                       localApic.nmiTrigger == LVTEntry::TriggerMode::EDGE ? "EDGE" : "LEVEL");
    }
}

void Apic::printIoApics(Util::String &string) {
    string += Util::String::format("I/O APIC version: [0x%x]\n",
                                   (*ioApics.begin())->getVersion());
    string += Util::String::format("Supports directed EOI: [%d]\n",
                                   static_cast<int>((*ioApics.begin())->getVersion() >= 0x20));

    string += "\nI/O APICs:\n";
    for (uint32_t i = 0; i < ioApics.size(); ++i) {
        const IoApic &ioApic = *ioApics.get(i);
        string += Util::String::format("Id: [%d], GSI: [%d] - [%d], MMIO: [0x%x] (phys) -> [0x%x] (virt)\n",
                                       ioApic.ioId,
                                       static_cast<uint32_t>(ioApic.gsiBase),
                                       static_cast<uint32_t>(ioApic.gsiMax),
                                       ioApic.baseAddress,
                                       ioApic.mmioAddress);
        for (uint32_t j = 0; j < ioApic.nmiSources.size(); ++j) {
            const IoApic::NmiSource &nmi = *ioApic.nmiSources.get(j);
            string += Util::String::format("  NMI: (GSI: [%d], Polarity: [%s], Trigger: [%s])\n",
                                           static_cast<uint32_t>(nmi.source),
                                           nmi.polarity == REDTBLEntry::PinPolarity::HIGH ? "HIGH" : "LOW",
                                           nmi.trigger == REDTBLEntry::TriggerMode::EDGE ? "EDGE" : "LEVEL");
        }
    }

    string += ("\nI/O APIC IRQ overrides:\n");
    for (uint32_t i = 0; i < IoApic::irqOverrides.size(); ++i) {
        const IoApic::IrqOverride &override = *IoApic::irqOverrides.get(i);
        string += Util::String::format("Source: [%d], Target: [%d], Polarity: [%s], Trigger: [%s]\n",
                                       static_cast<uint32_t>(override.source),
                                       static_cast<uint32_t>(override.target),
                                       override.polarity == REDTBLEntry::PinPolarity::HIGH ? "HIGH" : "LOW",
                                       override.trigger == REDTBLEntry::TriggerMode::EDGE ? "EDGE" : "LEVEL");
    }
}

} // namespace Device
