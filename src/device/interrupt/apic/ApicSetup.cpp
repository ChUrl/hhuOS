#include "Apic.h"
#include "filesystem/memory/MemoryDriver.h"
#include "filesystem/memory/MemoryFileNode.h"
#include "kernel/paging/Paging.h"
#include "kernel/service/FilesystemService.h"
#include "kernel/service/InterruptService.h"
#include "kernel/system/System.h"
#include "kernel/system/TaskStateSegment.h"
#include "lib/util/base/Constants.h"

namespace Device {

bool Apic::apicEnabled = false;
Util::ArrayList<LocalApic *> Apic::localApics;
Util::ArrayList<ApicTimer *> Apic::timers;
IoApic *Apic::ioApic = nullptr;
LocalApicError *Apic::errorHandler = new LocalApicError();
Util::Array<Util::Array<uint32_t> *> *Apic::interruptCounter = nullptr;
Util::Array<Util::Array<Util::Async::Atomic<uint32_t> *> *> *Apic::interruptCounterWrapper = nullptr;

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

    // Initialize the I/O APIC
    ioApic->initialize();

    // TODO: Remove This again, just for testing external interrupt redirection
    // Results:
    // - When the AP has interrupts disabled, the keyboard interrupt (33) is registered in the
    //   local APIC's IRR (of CPU 1)
    // - When the AP has interrupts enabled, the keyboard interrupt will be handled.
    //   This only happens rarely though, as the OS usually crashes beforehand x_x
    // - When trying often enough with GDB and a breakpoint on Keyboard::trigger, it can be
    //   verified that the interrupt handler is indeed called from the AP (QEMU thread2)
    // IoApic &ioApic = *ioApics.get(0);
    // REDTBLEntry keyboard = ioApic.readREDTBL(static_cast<Kernel::GlobalSystemInterrupt>(1));
    // keyboard.destination = 1;
    // ioApic.writeREDTBL(static_cast<Kernel::GlobalSystemInterrupt>(1), keyboard);

    prepareInterruptCounters();

    // We only require one error handler, as every AP can only access its own local APIC's error register
    errorHandler->plugin();      // Does not allow the interrupt!
    enableCurrentErrorHandler(); // Allows the interrupt for this AP

    // In contrast to the errorHandler, there are multiple timers in multicore systems, because they keep
    // track of the "core-local" time.
    ApicTimer::calibrate();
    startCurrentTimer();
}

void Apic::mountVirtualFilesystemNodes() {
    ensureApic();
    // TODO: Create some UpdateOnReadFileNode or sth. that executes a updateCallback function before read
    //       to update these files lazily (wouldn't want to update the interrupts file on every interrupt...)

    auto &filesystemService = Kernel::System::getService<Kernel::FilesystemService>();
    auto &driver = filesystemService.getFilesystem().getVirtualDriver("/device");

    auto *localApicNode = new Filesystem::Memory::MemoryFileNode("lapic");
    auto *ioApicNode = new Filesystem::Memory::MemoryFileNode("ioapic");
    auto *lvtNode = new Filesystem::Memory::MemoryFileNode("lvt");
    auto *redtblNode = new Filesystem::Memory::MemoryFileNode("redtbl");
    auto *picNode = new Filesystem::Memory::MemoryFileNode("pic");
    auto *irqsNode = new Filesystem::Memory::MemoryFileNode("irqs");

    Util::String lapic, ioapic, lvt, redtbl, pic, irqs;
    printLocalApics(lapic);
    printIoApic(ioapic);
    LocalApic::printLvt(lvt);
    ioApic->printRedtbl(redtbl);
    Pic::printStatus(pic);
    printInterrupts(irqs);

    localApicNode->writeData(static_cast<const uint8_t *>(lapic), 0, lapic.length());
    ioApicNode->writeData(static_cast<const uint8_t *>(ioapic), 0, ioapic.length());
    lvtNode->writeData(static_cast<const uint8_t *>(lvt), 0, lvt.length());
    redtblNode->writeData(static_cast<const uint8_t *>(redtbl), 0, redtbl.length());
    picNode->writeData(static_cast<const uint8_t *>(pic), 0, pic.length());
    irqsNode->writeData(static_cast<const uint8_t *>(irqs), 0, irqs.length());

    filesystemService.createDirectory("/device/apic");
    driver.addNode("/apic/", localApicNode);
    driver.addNode("/apic/", ioApicNode);
    driver.addNode("/apic/", lvtNode);
    driver.addNode("/apic/", redtblNode);
    driver.addNode("/apic/", picNode);
    driver.addNode("/apic/", irqsNode);
}

void Apic::ensureApic() {
    if (!apicEnabled) {
        Util::Exception::throwException(Util::Exception::ILLEGAL_STATE, "APIC not initialized!");
    }
}

// TODO: Sort these lists beforehand, this way the getIoApic etc. functions can just use ioApics.get(id) in O(1)
// TODO: This requires sequential ids
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
            // TODO: When using an array, the disabled CPUs have to be marked as nullptr
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

// TODO: I think I can just remove multi I/O APICs (IA-32 only has a single I/O APIC).
// TODO: Do this when the working tree is clean and compare, remove if the code is significantly cleaner
void Apic::populateIoApics() {
    // Get our required information from ACPI
    Util::ArrayList<const Acpi::IoApic *> acpiIoApics;
    Util::ArrayList<const Acpi::NmiSource *> acpiNmiSources;
    Util::ArrayList<const Acpi::InterruptSourceOverride *> acpiInterruptSourceOverrides;
    Acpi::collectMadtStructures(acpiIoApics, Acpi::IO_APIC);
    Acpi::collectMadtStructures(acpiNmiSources, Acpi::NON_MASKABLE_INTERRUPT_SOURCE);
    Acpi::collectMadtStructures(acpiInterruptSourceOverrides, Acpi::INTERRUPT_SOURCE_OVERRIDE);

    if (acpiIoApics.size() == 0) {
        // This is illegal, because this implementation does not support virtual wire mode
        Util::Exception::throwException(Util::Exception::ILLEGAL_STATE, "Didn't find any I/O APIC(s)!");
    }

    // Multiple I/O APICs are possible, but in the usual Intel consumer chipsets there is only one
    if (acpiIoApics.size() > 1) {
        Util::Exception::throwException(Util::Exception::ILLEGAL_STATE, "Multiple I/O APICs are unsupported!");
    }

    const auto *ioInfo = *acpiIoApics.begin();

    ioApic = new IoApic(ioInfo->ioApicId, ioInfo->ioApicAddress,
                        static_cast<Kernel::GlobalSystemInterrupt>(ioInfo->globalSystemInterruptBase));

    // Add all NMIs that belong to this I/O APIC
    for (const auto *nmi : acpiNmiSources) {
        ioApic->addNonMaskableInterrupt(static_cast<Kernel::GlobalSystemInterrupt>(nmi->globalSystemInterrupt),
                                        nmi->flags & Acpi::IntiFlag::ACTIVE_HIGH ? REDTBLEntry::PinPolarity::HIGH : REDTBLEntry::PinPolarity::LOW,
                                        nmi->flags & Acpi::IntiFlag::EDGE_TRIGGERED ? REDTBLEntry::TriggerMode::EDGE : REDTBLEntry::TriggerMode::LEVEL);
    }

    // TODO: I could store them inside an IrqOverride[16] array (inside IoApic) and improve the getOverride function
    // Add the IRQ overrides
    for (const auto *override : acpiInterruptSourceOverrides) {
        // ISA bus default values
        REDTBLEntry::PinPolarity polarity = REDTBLEntry::PinPolarity::HIGH;
        REDTBLEntry::TriggerMode trigger = REDTBLEntry::TriggerMode::EDGE;

        // If flags[0:1] is 0, the bus default is used
        if ((override->flags & 0x3) != 0 && (override->flags & Acpi::IntiFlag::ACTIVE_LOW)) {
            // Use override instead of bus default (HIGH)
            polarity = REDTBLEntry::PinPolarity::LOW;
        }

        // If flags[2:3] is 0, the bus default is used
        if ((override->flags & 0xC) != 0 && (override->flags & Acpi::IntiFlag::LEVEL_TRIGGERED)) {
            // Use override instead of bus default (EDGE)
            trigger = REDTBLEntry::TriggerMode::LEVEL;
        }

        IoApic::addIrqOverride(static_cast<InterruptRequest>(override->source),
                               static_cast<Kernel::GlobalSystemInterrupt>(override->globalSystemInterrupt),
                               polarity, trigger);
    }
}

void Apic::prepareInterruptCounters() {
    // This turned out to be very unenjoyable
    auto &memoryService = Kernel::System::getService<Kernel::MemoryService>();

    interruptCounter = new Util::Array<Util::Array<uint32_t> *>(256);
    interruptCounterWrapper = new Util::Array<Util::Array<Util::Async::Atomic<uint32_t> *> *>(256);

    for (uint32_t i = 0; i < 256; ++i) {
        (*interruptCounter)[i] = new Util::Array<uint32_t>(getCpuCount());
        (*interruptCounterWrapper)[i] = new Util::Array<Util::Async::Atomic<uint32_t> *>(getCpuCount());
        for (uint32_t cpu = 0; cpu < getCpuCount(); ++cpu) {
            (*(*interruptCounter)[i])[cpu] = 0;
            (*(*interruptCounterWrapper)[i])[cpu] = new Util::Async::Atomic<uint32_t>((*(*interruptCounter)[i])[cpu]);
        }
    }
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

void Apic::printIoApic(Util::String &string) {
    string += Util::String::format("I/O APIC version: [0x%x]\n",
                                   ioApic->getVersion());
    string += Util::String::format("Supports directed EOI: [%d]\n",
                                   static_cast<int>(ioApic->getVersion() >= 0x20));

    string += "\nI/O APIC:\n";
    string += Util::String::format("Id: [%d], GSI: [%d] - [%d], MMIO: [0x%x] (phys) -> [0x%x] (virt)\n",
                                   ioApic->ioId,
                                   static_cast<uint32_t>(ioApic->gsiBase),
                                   static_cast<uint32_t>(ioApic->gsiMax),
                                   ioApic->baseAddress,
                                   ioApic->mmioAddress);
    for (uint32_t j = 0; j < ioApic->nmiSources.size(); ++j) {
        const IoApic::NmiSource &nmi = *ioApic->nmiSources.get(j);
        string += Util::String::format("NMI: (GSI: [%d], Polarity: [%s], Trigger: [%s])\n",
                                       static_cast<uint32_t>(nmi.source),
                                       nmi.polarity == REDTBLEntry::PinPolarity::HIGH ? "HIGH" : "LOW",
                                       nmi.trigger == REDTBLEntry::TriggerMode::EDGE ? "EDGE" : "LEVEL");
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

void Apic::printInterrupts(Util::String &string) {
    // Print the header
    string += "vector";
    for (uint32_t i = 0; i < getCpuCount(); ++i) {
        string += Util::String::format(",cpu%d", i);
    }
    string += "\n";

    // Print a line for each interrupt, listing the amounts per core
    for (uint32_t i = 0; i < 256; ++i) {
        Util::String nextline = Util::String::format("%d", i);
        bool occured = false;
        const auto *cpulist = (*interruptCounter)[i];
        for (uint32_t cpu = 0; cpu < getCpuCount(); ++cpu) {
            const auto count = (*cpulist)[cpu];
            occured = occured | (count != 0);
            nextline += Util::String::format(",%d", count);
        }
        nextline += "\n";

        // Skip lines that only contain 0
        if (occured) {
            string += nextline;
        }
    }
}

} // namespace Device