#include "LApic.h"
#include "lib/util/cpu/CpuId.h"
#include "kernel/system/System.h"

namespace Device {

bool LApic::initialized = false;
Device::ModelSpecificRegister LApic::ia32ApicBaseMsr = Device::ModelSpecificRegister(0x1B);

LApic::LPlatformConfiguration LApic::platformConfiguration;

Util::Async::Spinlock LApic::mmioLock;
Util::Async::Spinlock LApic::ipiLock;

Kernel::Logger LApic::log = Kernel::Logger::get("LApic");

// ! Public member functions start here

bool LApic::isInitialized() {
    return initialized;
}

void LApic::initialize() {
    // Read system information from ACPI
    initializePlatformConfiguration();

    if (!platformConfiguration.xApicSupported) {
        Util::Exception::throwException(Util::Exception::UNSUPPORTED_OPERATION, "LApic::initialize(): xApic support not present!");
    }

    if (!readBaseMSR().isBSP) {
        // TODO: I think architectural MSRs are shared between cores/they exist only once,
        //       but what does the BSP flag mean then?
        // TODO: I think I misunderstood what BSP even means, investigate IA-32 Architecture Manual Chapter 8.4.3.4
        // TODO: If multiple APIC MSRs exist, can local APICs be individually enabled/disabled/relocated?
        //       Answer: Yes, at least they can be individually relocated. This doesn't have to mean that
        //               there are multiple APIC MSRs though...
        Util::Exception::throwException(Util::Exception::UNSUPPORTED_OPERATION, "LApic::initialize(): May only be called by BSP!");
    }

    // x2Apic doesn't have MMIO register access (x2Apic uses MSRs)
    if (platformConfiguration.x2ApicSupported && platformConfiguration.isX2Apic) {
        MSREntry msr = readBaseMSR();
        msr.isX2Apic = false; // Operate in xApic compatibility mode // TODO: Test this
        writeBaseMSR(msr);
        platformConfiguration.isX2Apic = false;
    }

    // TODO: Does every AP has to call this before initializing its local APIC?
    //       - Every AP writes/reads the same physical address, but different registers are affected
    //       - How does paging work in MP? Do I have to mapIO the same physical address multiple times?
    //         - Probably not if there is only a single kernel address space
    //       - Every AP has its own MMU, but are page tables shared or individual?
    //         - It would make sense to keep the kernel addresses only once
    //         - I guess the TLB at least has to be consistent over APs regarding kernel address space?
    initializeMMIORegion();

    platformConfiguration.version = readDoubleWord(Register::VER) & 0xFF;

    // Initialize the local APIC of the BSP before initializing any APs
    initializeController(getLApicConfiguration(getId()));

    // TODO: Should probably not do this automatically inside LApic::initialize()...
    for (auto *lapic : platformConfiguration.lapics) {
        if (lapic->enabled) { // Skip already running processors
            continue;
        }

        // TODO: Only for ACPI version >= 2?
        if (lapic->canEnable) {
            initializeApplicationProcessor(lapic);
        }
    }

    // TODO: Mask all the PIC interrupts in the PIC aswell (they should still be all masked though...)

    initialized = true;

#if HHUOS_LAPIC_ENABLE_DEBUG == 1
    dumpLPlatformConfiguration();
#endif
}

void LApic::allow(Interrupt lint) {
    LVTEntry entry = readLVT(lint);
    entry.isMasked = false;
    writeLVT(lint, entry);
}

void LApic::forbid(Interrupt lint) {
    LVTEntry entry = readLVT(lint);
    entry.isMasked = true;
    writeLVT(lint, entry);
}

bool LApic::status(Interrupt lint) {
    return readLVT(lint).isMasked;
}

void LApic::sendEndOfInterrupt() {
    writeDoubleWord(Register::EOI, 0);
}

// TODO: Make sure this is called by the correct processor?
//       - Should happen anyway because the CPU that received the ERROR interrupt also runs the handler
void LApic::handleErrors() {
    // Write before read (read/write register, IA-32 Architecture Manual Chapter 10.5.3)
    writeDoubleWord(Register::ESR, 0);
    uint32_t errors = readDoubleWord(Register::ESR);

    // Errors for all CPUs
    bool illegalVectorReceived = errors & (1 << 6);
    bool illegalVectorSent = errors & (1 << 5);

    // Errors reserved on original Pentium CPUs
    bool illegalRegisterAccess = errors & (1 << 7);

    // Errors reserved on Core, P4, Xeon CPUs
    bool receiveAcceptError = errors & (1 << 3);
    bool sendAcceptError = errors & (1 << 2);
    bool receiveChecksumError = errors & (1 << 1);
    bool sendChecksumError = errors & 1;

    // TODO: Don't know how to handle, for now just log
    // TODO: Can I log here? This happens during the ERROR interrupt handler
    if (illegalVectorReceived) {
        log.error("ERROR: Illegal vector received!");
    }
    if (illegalVectorSent) {
        log.error("ERROR: Illegal vector sent!");
    }
    if (illegalRegisterAccess) {
        log.error("ERROR: Illegal register access!");
    }
    if (receiveAcceptError) {
        log.error("ERROR: Receive accept error!");
    }
    if (sendAcceptError) {
        log.error("ERROR: Send accept error!");
    }
    if (receiveChecksumError) {
        log.error("ERROR: Receive checksum error!");
    }
    if (sendChecksumError) {
        log.error("ERROR: Send checksum error!");
    }


    // Clear errors
    writeDoubleWord(Register::ESR, 0);
    writeDoubleWord(Register::ESR, 0);
}

// ! Private member functions start here

uint8_t LApic::getId() {
    return (readDoubleWord(Register::ID) >> 24) & 0xFF;
}

void LApic::initializePlatformConfiguration() {
    if (!Acpi::isAvailable()) {
        Util::Exception::throwException(Util::Exception::UNSUPPORTED_OPERATION, "LApic::initialize(): ACPI support not present!");
    }

    auto features = Util::Cpu::CpuId::getCpuFeatures();
    for (auto feature: features) {
        if (feature == Util::Cpu::CpuId::CpuFeature::APIC) {
            platformConfiguration.xApicSupported = true;
        }
        if (feature == Util::Cpu::CpuId::CpuFeature::X2APIC) {
            platformConfiguration.x2ApicSupported = true;
        }
    }

    platformConfiguration.isX2Apic = readBaseMSR().isX2Apic;
    platformConfiguration.address = Acpi::getTable<Acpi::Madt>("APIC")->localApicAddress;

    if (platformConfiguration.address == 0) {
        Util::Exception::throwException(Util::Exception::ILLEGAL_STATE,
                                        "LApic::initializePlatformConfiguration(): Didn't find local APIC address!");
    }

    Util::Data::ArrayList<const Acpi::ProcessorLocalApic *> processorLocalApics;
    Util::Data::ArrayList<const Acpi::LocalApicNMI *> nmiConfigurations;
    Acpi::getApicStructures<Acpi::ProcessorLocalApic>(&processorLocalApics, Acpi::PROCESSOR_LOCAL_APIC);
    Acpi::getApicStructures<Acpi::LocalApicNMI>(&nmiConfigurations, Acpi::LOCAL_APIC_NMI);

    if (processorLocalApics.size() == 0) {
        Util::Exception::throwException(Util::Exception::ILLEGAL_STATE,
                                        "LApic::initializePlatformConfiguration(): Didn't find local APIC(s)!");
    }
    if (nmiConfigurations.size() == 0) {
        // TODO: Are nmiConfigurations mandatory? I guess there should be at least 1
        Util::Exception::throwException(Util::Exception::ILLEGAL_STATE,
                                        "LApic::initializePlatformConfiguration(): Didn't find NMI configuration(s)!");
    }

    for (auto lapic : processorLocalApics) {
        platformConfiguration.lapics.add(new LApicConfiguration {
            .uid = lapic->acpiProcessorUid,
            .id = lapic->apicId,
            .enabled = static_cast<bool>(lapic->flags & Acpi::ProcessorFlag::ENABLED),
            .canEnable = static_cast<bool>(lapic->flags & Acpi::ProcessorFlag::ONLINE_CAPABLE)
        });
    }

    for (auto nmi : nmiConfigurations) {
        platformConfiguration.lnmis.add(new LNMIConfiguration {
            .uid = nmi->acpiProcessorUid,
            .id = static_cast<uint8_t>(nmi->acpiProcessorUid == 0xFF ? 0xFF : uidToId(nmi->acpiProcessorUid)),
            .polarity = nmi->flags & Acpi::IntiFlag::ACTIVE_HIGH ? LVTPinPolarity::HIGH : LVTPinPolarity::LOW,
            .triggerMode = nmi->flags & Acpi::IntiFlag::EDGE_TRIGGERED ? LVTTriggerMode::EDGE : LVTTriggerMode::LEVEL,
            .lint = nmi->localApicLint == 0 ? LINT0 : LINT1
        });
    }
}

uint8_t LApic::uidToId(uint8_t uid) {
    for (auto *lapic : platformConfiguration.lapics) {
        if (lapic->uid == uid) {
            return lapic->id;
        }
    }

    Util::Exception::throwException(Util::Exception::ILLEGAL_STATE,
                                    "LApic::idToUid(): Didn't find local APIC matching UID!");
}

LApic::LApicConfiguration *LApic::getLApicConfiguration(uint8_t id) {
    for (auto *lapic : platformConfiguration.lapics) {
        if (lapic->id == id) {
            return lapic;
        }
    }

    Util::Exception::throwException(Util::Exception::ILLEGAL_STATE,
                                    "LApic::getLApicConfiguration(): Didn't find configuration matching ID!");
}

// There is a maximum of 1 NMI configuration per core
LApic::LNMIConfiguration *LApic::getNMIConfiguration(LApicConfiguration *lapic) {
    for (auto *nmi : platformConfiguration.lnmis) {
        if (nmi->uid == 0xFF || nmi->id == lapic->id) { // 0xFF means all CPUs
            return nmi;
        }
    }

    // TODO: Not every core has to have one, so don't throw up (use/implement optional?)
    Util::Exception::throwException(Util::Exception::ILLEGAL_STATE,
                                    "LApic::getNMIConfiguration(): Didn't find NMI configuration matching ID!");
}

void LApic::initializeMMIORegion() {
    // TODO: mapIO strong uncacheable?
    // Allocate memory (4 kb) for the memory mapped registers (without relocation)
    // The address returned is page aligned, if the size crosses page boundary an additional page will be allocated.
    auto &memoryService = Kernel::System::getService<Kernel::MemoryService>();
    void *virtAddress = memoryService.mapIO(platformConfiguration.address, Util::Memory::PAGESIZE, true);

    if (virtAddress == nullptr) {
        Util::Exception::throwException(Util::Exception::OUT_OF_MEMORY,
                                        "LApic::initialize(): Not enough space left on kernel heap!");
    }

    // Use this address to access the local APIC's memory mapped registers
    platformConfiguration.virtAddress =
            reinterpret_cast<uint32_t>(virtAddress) + platformConfiguration.address % Util::Memory::PAGESIZE;
}

void LApic::initializeApplicationProcessor(LApicConfiguration *lapic) {
    // TODO: Prepare stack for entrycode
    // TODO: Send INIT and STARTUP interrupts with entry code address
    // TODO: The entrycode needs to call initializeController to initialize its own local APIC registers

    lapic->enabled = true;
}

// TODO: Does this have to be synchronized when initializing other APs?
//       - Probably not as all of them work in different address spaces?
//       - Also only one is initialized at a time (and MP init sequence requires acquiring BIOS semaphore...)
// TODO: IA-32 Architecture Manual Chapter 8.4.3.5: APIC ID has to be signalled to ACPI?
void LApic::initializeController(LApicConfiguration *lapic) {
    initializeLVT();

    // Configure the NMI (non maskable interrupt) pin
    LNMIConfiguration *nmi = getNMIConfiguration(lapic);
    writeLVT(nmi->lint, {.vector = static_cast<Kernel::InterruptDispatcher::Interrupt>(0), // NMI doesn't have vector
            .deliveryMode = LVTDeliveryMode::NMI,
            .pinPolarity = nmi->polarity,
            .triggerMode = nmi->triggerMode,
            .isMasked = false});

    // SW Enable APIC by setting the Spurious Interrupt Vector Register with spurious vector number 0xFF (OSDev)
    // and the SW ENABLE flag.
    writeSVR({.vector = Kernel::InterruptDispatcher::SPURIOUS,
                     .isSWEnabled = true,
                     .hasEOIBroadcastSuppression = true});

    // Clear possible error interrupts (write twice because ESR is read/write register, writing once does not
    // change a subsequently read value, in fact the register should always be written once before reading)
    writeDoubleWord(Register::ESR, 0);
    writeDoubleWord(Register::ESR, 0);

    // Clear other outstanding interrupts
    sendEndOfInterrupt();

    // Allow all interrupts to be forwarded to the CPU by setting the Task-Priority Class and Sub Class thresholds to 0
    // (IA-32 Architecture Manual Chapter 10.8.3.1)
    writeDoubleWord(Register::TPR, 0);
}

void LApic::initializeLVT() {
    // Default values
    LVTEntry entry = {.deliveryMode = LVTDeliveryMode::FIXED, // TODO
                      .pinPolarity = LVTPinPolarity::HIGH,
                      .triggerMode = LVTTriggerMode::EDGE,
                      .isMasked = true};

    // Set all the vector numbers
    entry.vector = Kernel::InterruptDispatcher::CMCI;
    writeLVT(CMCI, entry); // TODO: The CMCI might not exist?
    entry.vector = Kernel::InterruptDispatcher::APICTIMER;
    writeLVT(TIMER, entry);
    entry.vector = Kernel::InterruptDispatcher::THERMAL;
    writeLVT(THERMAL, entry);
    entry.vector = Kernel::InterruptDispatcher::PERFORMANCE;
    writeLVT(PERFORMANCE, entry);
    entry.vector = Kernel::InterruptDispatcher::LINT0;
    writeLVT(LINT0, entry);
    entry.vector = Kernel::InterruptDispatcher::LINT1;
    writeLVT(LINT1, entry);
    entry.vector = Kernel::InterruptDispatcher::ERROR;
    writeLVT(ERROR, entry);
}

void LApic::dumpLPlatformConfiguration() {
    log.info("Supported modes: %s %s", platformConfiguration.xApicSupported ? "[xApic]" : "[None]", platformConfiguration.x2ApicSupported ? "[x2Apic]" : "");
    log.info("Selected mode: %s", platformConfiguration.isX2Apic ? "[x2Apic]" : "[xApic]");
    log.info("Version: [0x%x]", platformConfiguration.version);
    log.info("MMIO: [0x%x] (phys) -> [0x%x] (virt)", platformConfiguration.address, platformConfiguration.virtAddress);
    log.info("Cores: [%d]", platformConfiguration.lapics.size());

    log.info("Local Apic status:");
    for (auto *lapic : platformConfiguration.lapics) {
        log.info("- Id: [0x%x], Enabled: [%d], Can enable: [%d]", lapic->id, lapic->enabled, lapic->canEnable);
    }

    log.info("Local NMI status:");
    for (auto *lnmi : platformConfiguration.lnmis) {
        log.info("- Id: [0x%x], Lint: [LINT%d]", lnmi->id, lnmi->lint == LINT0 ? 0 : 1);
    }
}

// ! Private register member functions start here

// IA-32 Architecture Manual Chapter 10.4.4
MSREntry LApic::readBaseMSR() {
    uint64_t val = ia32ApicBaseMsr.readQuadWord(); // Atomic read

    return {
            .isBSP = static_cast<bool>(val & (1 << 8)),
            .isX2Apic = static_cast<bool>(val & (1 << 10)),
            .isHWEnabled = static_cast<bool>(val & (1 << 11)),
            .baseField = static_cast<uint32_t>(val & 0xFFFFF000)
    };
}

// IA-32 Architecture Manual Chapter 10.4.4
void LApic::writeBaseMSR(MSREntry entry) {
    uint64_t val = static_cast<uint64_t>(entry.isBSP) << 8
                   | static_cast<uint64_t>(entry.isX2Apic) << 10
                   | static_cast<uint64_t>(entry.isHWEnabled) << 11
                   | static_cast<uint64_t>(entry.baseField) << 12;

    ia32ApicBaseMsr.writeQuadWord(val); // Atomic write
}

// NOTE: https://forum.osdev.org/viewtopic.php?f=1&t=16653#p123105
// NOTE: In x2APIC mode this can be written atomically, so no spinlock there
// TODO: Needs spinlock?
//       - If a single cpu would somehow write the regAddr, switch threads and then read regAddr this would be a bug
//         This case shouldn't happen though as there is no (really?) situation where a thread would write these...
//       - If a cpu writes the regAddr and another cpu writes the regAddr again there would be no difference as
//         different registers would be written
//       - If an accepted interrupt can change register values interrupts would have to be disabled
uint32_t LApic::readDoubleWord(uint16_t reg) {
    volatile auto *regAddr = reinterpret_cast<uint32_t *>(platformConfiguration.virtAddress + reg);
    volatile auto val = *regAddr;

    return val;
}

// TODO: Needs spinlock?
void LApic::writeDoubleWord(uint16_t reg, uint32_t val) {
    volatile auto *regAddr = reinterpret_cast<uint32_t *>(platformConfiguration.virtAddress + reg);
    *regAddr = val;
}

// IA-32 Architecture Manual Chapter 10.9
SVREntry LApic::readSVR() {
    uint32_t val = readDoubleWord(Register::SVR);

    return {
            .vector = static_cast<Kernel::InterruptDispatcher::Interrupt>(val & 0xFF),
            .isSWEnabled = static_cast<bool>(val & (1 << 8)),
            .hasFocusProcessorChecking = static_cast<bool>(val & (1 << 9)),
            .hasEOIBroadcastSuppression = static_cast<bool>(val & (1 << 12))
    };
}

// IA-32 Architecture Manual Chapter 10.9
void LApic::writeSVR(SVREntry svr) {
    uint32_t val = static_cast<uint32_t>(svr.vector)
                   | static_cast<uint32_t>(svr.isSWEnabled) << 8
                   | static_cast<uint32_t>(svr.hasFocusProcessorChecking) << 9
                   | static_cast<uint32_t>(svr.hasEOIBroadcastSuppression) << 12;

    writeDoubleWord(Register::SVR, val);
}

// IA-32 Architecture Manual Chapter 10.5.1
LVTEntry LApic::readLVT(Interrupt lint) {
    uint32_t val = readDoubleWord(lint);

    return {
            .vector = static_cast<Kernel::InterruptDispatcher::Interrupt>(val & 0xFF),
            .deliveryMode = static_cast<LVTDeliveryMode>((val & (0b111 << 8)) >> 8),
            .deliveryStatus = static_cast<LVTDeliveryStatus>((val & (1 << 12)) >> 12),
            .pinPolarity = static_cast<LVTPinPolarity>((val & (1 << 13)) >> 13),
            .triggerMode = static_cast<LVTTriggerMode>((val & (1 << 15)) >> 15),
            .isMasked = static_cast<bool>(val & (1 << 16)),
            .timerMode = static_cast<LVTTimerMode>((val & (0b11 << 17)) >> 17)
    };
}

// TODO: Check if it is a problem to write to readonly/reserved areas
// IA-32 Architecture Manual Chapter 10.5.1
void LApic::writeLVT(Interrupt lint, LVTEntry entry) {
    uint32_t val = static_cast<uint32_t>(entry.vector)
                   | static_cast<uint32_t>(entry.deliveryMode) << 8
                   | static_cast<uint32_t>(entry.pinPolarity) << 13
                   | static_cast<uint32_t>(entry.triggerMode) << 15
                   | static_cast<uint32_t>(entry.isMasked) << 16
                   | static_cast<uint32_t>(entry.timerMode) << 17;

    writeDoubleWord(lint, val);
}

// NOTE: https://forum.osdev.org/viewtopic.php?f=1&t=16653#p123105
// NOTE: In x2APIC mode this can be written atomically, so no spinlock there
// TODO: Needs spinlock?
//       - The case that a single cpu writes the ICR from different threads shouldn't happen
//         as IPIs are only issued to startup different cores (happens before scheduling)?
//       - The delivery status changes when an ipi is accepted, so interrups have to be disabled?
// IA-32 Architecture Manual Chapter 10.6.1
ICREntry LApic::readICR() {
    // NOTE: Interrupts have to be disabled beforehand
    uint32_t low, high;
    low = readDoubleWord(Register::ICR_LOW);
    high = readDoubleWord(Register::ICR_HIGH);

    return {
            .vector = static_cast<Kernel::InterruptDispatcher::Interrupt>(low & 0xFF),
            .deliveryMode = static_cast<ICRDeliveryMode>((low & (0b111 << 8)) >> 8),
            .destinationMode = static_cast<ICRDestinationMode>((low & (1 << 11)) >> 11),
            .deliveryStatus = static_cast<ICRDeliveryStatus>((low & (1 << 12)) >> 12),
            .level = static_cast<ICRLevel>((low & (1 << 14)) >> 14),
            .triggerMode = static_cast<ICRTriggerMode>((low & (1 << 15)) >> 15),
            .destinationShorthand = static_cast<ICRDestinationShorthand>((low & (0b11 << 18)) >> 18),
            .destination = static_cast<uint8_t>(high >> 24)
    };
}

// TODO: Needs spinlock?
// IA-32 Architecture Manual Chapter 10.6.1
void LApic::writeICR(ICREntry icr) {
    uint32_t low, high;
    low = static_cast<uint32_t>(icr.vector)
          | static_cast<uint32_t>(icr.deliveryMode) << 8
          | static_cast<uint32_t>(icr.destinationMode) << 11
          | static_cast<uint32_t>(icr.deliveryStatus) << 12
          | static_cast<uint32_t>(icr.level) << 14
          | static_cast<uint32_t>(icr.triggerMode) << 15
          | static_cast<uint32_t>(icr.destinationShorthand) << 18;
    high = static_cast<uint32_t>(icr.destination) << 24;

    // NOTE: Interrupts have to be disabled beforehand
    writeDoubleWord(Register::ICR_HIGH, high);
    writeDoubleWord(Register::ICR_LOW, low); // Last as writing low DW sends the IPI
}

}