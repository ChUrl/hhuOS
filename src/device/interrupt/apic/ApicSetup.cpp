#include "Apic.h"
#include "kernel/paging/Paging.h"
#include "kernel/service/InterruptService.h"
#include "lib/util/base/Constants.h"

namespace Device {

bool Apic::apicEnabled = false;
uint32_t Apic::usableProcessors = 0;
Util::Array<LocalApic *> *Apic::localApics = nullptr;
Util::Array<ApicTimer *> *Apic::localTimers = nullptr;
IoApic *Apic::ioApic = nullptr;
LocalApicError *Apic::errorHandler = nullptr;

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
    populateIoApic();

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
    errorHandler = new LocalApicError();
    errorHandler->plugin();      // Does not allow the interrupt!
    enableCurrentErrorHandler(); // Allows the interrupt for this AP

    // In contrast to the errorHandler, there are multiple timers in multicore systems, because they keep
    // track of the "core-local" time.
    localTimers = new Util::Array<ApicTimer *>(localApics->length());
    for (uint32_t i = 0; i < localApics->length(); ++i) {
        (*localTimers)[i] = nullptr;
    }
    ApicTimer::calibrate();
    startCurrentTimer();
}

// NOTE: The instances are stored inside an array, indexed by the APIC ID.
// NOTE: This requires sequential IDs, I think (?) they should be sequential (MPSPec, sec. B.4)
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

    localApics = new Util::Array<LocalApic *>(acpiProcessorLocalApics.size());

    // Create LocalApic instances
    for (const auto *localInfo : acpiProcessorLocalApics) {
        if (!(localInfo->flags & 0x1)) {
            // When ACPI reports this local APIC as disabled, it may not be used by the OS.
            // ACPI 1.0 specification, sec. 5.2.8.1
            (*localApics)[localInfo->apicId] = nullptr;
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

        usableProcessors++;
        (*localApics)[localInfo->apicId] = new LocalApic(localInfo->apicId, madt->localApicAddress,
                                                         nmiInfo->localApicLint == 0
                                                           ? LocalApic::LINT0
                                                           : LocalApic::LINT1,
                                                         nmiInfo->flags & Acpi::IntiFlag::ACTIVE_HIGH
                                                           ? LVTEntry::PinPolarity::HIGH
                                                           : LVTEntry::PinPolarity::LOW,
                                                         nmiInfo->flags & Acpi::IntiFlag::EDGE_TRIGGERED
                                                           ? LVTEntry::TriggerMode::EDGE
                                                           : LVTEntry::TriggerMode::LEVEL);
    }

    log.info("Found [%d] CPUs of which [%d] are usable.", localApics->length(), usableProcessors);
}

void Apic::populateIoApic() {
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

    const auto *ioInfo = acpiIoApics.get(0);

    ioApic = new IoApic(ioInfo->ioApicId, ioInfo->ioApicAddress,
                        static_cast<Kernel::GlobalSystemInterrupt>(ioInfo->globalSystemInterruptBase));

    // Add all NMIs that belong to this I/O APIC
    for (const auto *nmi : acpiNmiSources) {
        ioApic->addNonMaskableInterrupt(static_cast<Kernel::GlobalSystemInterrupt>(nmi->globalSystemInterrupt),
                                        nmi->flags & Acpi::IntiFlag::ACTIVE_HIGH ? REDTBLEntry::PinPolarity::HIGH : REDTBLEntry::PinPolarity::LOW,
                                        nmi->flags & Acpi::IntiFlag::EDGE_TRIGGERED ? REDTBLEntry::TriggerMode::EDGE : REDTBLEntry::TriggerMode::LEVEL);
    }

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
    // 256 vector numbers for n CPUs. Allocates space also for disabled CPUs, to allow for indexing using the ID.
    const uint32_t entries = 256 * localApics->length();

    counters = new Util::Array<uint32_t>(entries);
    wrappers = new Util::Array<Util::Async::Atomic<uint32_t> *>(entries);
    for (uint32_t i = 0; i < entries; ++i) {
        (*counters)[i] = 0;
        (*wrappers)[i] = new Util::Async::Atomic<uint32_t>((*counters)[i]);
    }
}

} // namespace Device