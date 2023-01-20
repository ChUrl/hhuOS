#include "LocalApic.h"
#include "kernel/system/System.h"
#include "kernel/paging/Paging.h"
#include "device/cpu/Cpu.h"
#include "lib/util/cpu/CpuId.h"
#include "Pic.h"

namespace Device {

bool LocalApic::initialized = false;
// bool LocalApic::smpInitialized = false;
LocalApicPlatform *LocalApic::localPlatform = nullptr;
ModelSpecificRegister LocalApic::ia32ApicBaseMsr = ModelSpecificRegister(0x1B);
LocalApic::Register LocalApic::lintRegs[7] = {static_cast<Register>(0x2F0),
                                              static_cast<Register>(0x320),
                                              static_cast<Register>(0x330),
                                              static_cast<Register>(0x340),
                                              static_cast<Register>(0x350),
                                              static_cast<Register>(0x360),
                                              static_cast<Register>(0x370)};
Kernel::Logger LocalApic::log = Kernel::Logger::get("LocalApic");

bool LocalApic::supportsXApic() {
    auto features = Util::Cpu::CpuId::getCpuFeatures();
    for (auto feature: features) {
        if (feature == Util::Cpu::CpuId::CpuFeature::APIC) {
            return true;
        }
    }
    return false;
}

bool LocalApic::supportsX2Apic() {
    auto features = Util::Cpu::CpuId::getCpuFeatures();
    for (auto feature: features) {
        if (feature == Util::Cpu::CpuId::CpuFeature::X2APIC) {
            return true;
        }
    }
    return false;
}

bool LocalApic::isInitialized() {
    return initialized;
}

// TODO: SMP
// bool LocalApic::isSmpInitialized() {
//     return smpInitialized;
// }

// TODO: Does not account for multiple cores
void LocalApic::ensureInitialized() {
    if (!initialized) {
        Util::Exception::throwException(Util::Exception::ILLEGAL_STATE, "Local APICs are not initialized!");
    }
}

void LocalApic::initialize(LocalApicPlatform *platform) {
    BaseMSREntry msrEntry = readBaseMSR();

    if (!msrEntry.isBSP) {
        // IA32_APIC_BASE_MSR is unique (every core has its own)
        Util::Exception::throwException(Util::Exception::UNSUPPORTED_OPERATION, "May only be called by BSP!");
    }

    localPlatform = platform;

    // Decide which mode to use (xApic or x2Apic)
    if (supportsX2Apic()) {
        // QEMU doesn't support emulation of x2Apic via TCG
        // KVM would be possible, but then GDB can't be attached, so compatibility mode will be chosen
        log.info("X2Apic support present but not implemented, running in xApic compatibility mode");
        localPlatform->isX2Apic = false;
        initializeMMIORegion();

        // log.info("X2Apic support present -> Enabling x2Apic mode");
        // msrEntry.isX2Apic = true;
        // writeBaseMSR(msrEntry);
        // localPlatform->isX2Apic = true;
    } else {
        log.info("Running in xApic mode");
        localPlatform->isX2Apic = false;
        initializeMMIORegion();
    }

    localPlatform->version = readDoubleWord(VER);

    // Mask all local interrupt sources
    initializeLVT();

    // Configure the non maskable interrupt pin
    LocalApicNMI *nmi = localPlatform->getLocalNMIConfiguration(getId());
    if (nmi != nullptr) {
        LVTEntry lvtEntry{};
        lvtEntry.vector = static_cast<InterruptVector>(0); // NMI doesn't have vector
        lvtEntry.deliveryMode = LVTEntry::DeliveryMode::NMI;
        lvtEntry.pinPolarity = nmi->polarity;
        lvtEntry.triggerMode = nmi->triggerMode;
        lvtEntry.isMasked = false;
        writeLVT(nmi->lint == 0 ? LINT0 : LINT1, lvtEntry);
    }

    // SW Enable APIC by setting the Spurious Interrupt Vector Register with spurious vector number 0xFF
    // and the SW ENABLE flag.
    SVREntry svrEntry{};
    svrEntry.vector = Kernel::InterruptDispatcher::SPURIOUS;
    svrEntry.isSWEnabled = true;
    svrEntry.hasEOIBroadcastSuppression = true;
    writeSVR(svrEntry);

    // Clear possible error interrupts (write twice because ESR is read/write register, writing once does not
    // change a subsequently read value, in fact the register should always be written once before reading)
    writeDoubleWord(ESR, 0);
    writeDoubleWord(ESR, 0);

    // Clear other outstanding interrupts
    sendEndOfInterrupt();

    // Allow all interrupts to be forwarded to the CPU by setting the Task-Priority Class and Sub Class thresholds to 0
    // (IA-32 Architecture Manual Chapter 10.8.3.1)
    writeDoubleWord(TPR, 0);

    // Mask all PIC interrupts that have been enabled previously
    for (uint8_t i = 0; i < 16; ++i) {
        Pic::forbid(static_cast<InterruptSource>(i));
    }

    initialized = true;
}

// TODO: Does not account for multiple cores
void LocalApic::allow(LocalInterrupt lint) {
    LVTEntry entry = readLVT(lint);
    entry.isMasked = false;
    writeLVT(lint, entry);
}

// TODO: Does not account for multiple cores
void LocalApic::forbid(LocalInterrupt lint) {
    LVTEntry entry = readLVT(lint);
    entry.isMasked = true;
    writeLVT(lint, entry);
}

// TODO: Does not account for multiple cores
bool LocalApic::status(LocalInterrupt lint) {
    return readLVT(lint).isMasked;
}

// This works for multiple cores because the core that handles the interrupt calls this function
// and thus reaches the correct local APIC
void LocalApic::sendEndOfInterrupt() {
    writeDoubleWord(EOI, 0);
}

// This works for multiple cores because the core that handles the interrupt calls this function
// and thus reaches the correct local APIC
void LocalApic::handleErrors() {
    // Write before read (read/write register, IA-32 Architecture Manual Chapter 10.5.3)
    writeDoubleWord(ESR, 0);
    uint32_t errors = readDoubleWord(ESR);

    // TODO: This is architecture dependent...
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
    if (illegalVectorReceived) { log.error("ERROR: Illegal vector received!"); }
    if (illegalVectorSent) { log.error("ERROR: Illegal vector sent!"); }
    if (illegalRegisterAccess) { log.error("ERROR: Illegal register access!"); }
    if (receiveAcceptError) { log.error("ERROR: Receive accept error!"); }
    if (sendAcceptError) { log.error("ERROR: Send accept error!"); }
    if (receiveChecksumError) { log.error("ERROR: Receive checksum error!"); }
    if (sendChecksumError) { log.error("ERROR: Send checksum error!"); }

    // Clear errors
    writeDoubleWord(ESR, 0);
    writeDoubleWord(ESR, 0);
}

void LocalApic::ensureMMIO() {
    if (localPlatform->virtAddress == 0) {
        Util::Exception::throwException(Util::Exception::ILLEGAL_STATE, "LocalApic MMIO region not initialized!");
    }
}

uint8_t LocalApic::getId() {
    return readDoubleWord(ID) >> 24;
}

void LocalApic::initializeMMIORegion() {
    uint32_t physAddress = localPlatform->physAddress;
    uint32_t pageOffset = physAddress % Util::Memory::PAGESIZE;

    auto &memoryService = Kernel::System::getService<Kernel::MemoryService>();
    void *virtAddress = memoryService.mapIO(physAddress, Util::Memory::PAGESIZE, true);

    // Account for possible misalignment
    localPlatform->virtAddress = reinterpret_cast<uint32_t>(virtAddress) + pageOffset;
}

void LocalApic::initializeLVT() {
    ensureMMIO();

    // Default values
    LVTEntry lvtEntry{};
    lvtEntry.deliveryMode = LVTEntry::DeliveryMode::FIXED, // TODO: Do I need to care about deliveryMode?
    lvtEntry.pinPolarity = LVTEntry::PinPolarity::HIGH,
    lvtEntry.triggerMode = LVTEntry::TriggerMode::EDGE,
    lvtEntry.isMasked = true;

    // Set all the vector numbers
    lvtEntry.vector = Kernel::InterruptDispatcher::CMCI;
    writeLVT(CMCI, lvtEntry); // TODO: The CMCI might not exist
    lvtEntry.vector = Kernel::InterruptDispatcher::APICTIMER;
    writeLVT(TIMER, lvtEntry);
    lvtEntry.vector = Kernel::InterruptDispatcher::THERMAL;
    writeLVT(THERMAL, lvtEntry);
    lvtEntry.vector = Kernel::InterruptDispatcher::PERFORMANCE;
    writeLVT(PERFORMANCE, lvtEntry);
    lvtEntry.vector = Kernel::InterruptDispatcher::LINT0;
    writeLVT(LINT0, lvtEntry);
    lvtEntry.vector = Kernel::InterruptDispatcher::LINT1;
    writeLVT(LINT1, lvtEntry);
    lvtEntry.vector = Kernel::InterruptDispatcher::ERROR;
    writeLVT(ERROR, lvtEntry);
}

BaseMSREntry LocalApic::readBaseMSR() {
    return static_cast<BaseMSREntry>(ia32ApicBaseMsr.readQuadWord()); // Atmoic read
}

void LocalApic::writeBaseMSR(const BaseMSREntry &msrEntry) {
    ia32ApicBaseMsr.writeQuadWord(static_cast<uint64_t>(msrEntry)); // Atomic write
}

uint32_t LocalApic::readDoubleWord(Register reg) {
    if (localPlatform->isX2Apic) {
        Util::Exception::throwException(Util::Exception::UNSUPPORTED_OPERATION, "X2Apic mode not supported!");
        // ModelSpecificRegister msr = getMSR(reg);
        // return static_cast<uint32_t>(msr.readQuadWord()); // Atomic read
    } else {
        ensureMMIO();
        return *reinterpret_cast<volatile uint32_t *>(localPlatform->virtAddress + reg);
    }
}

void LocalApic::writeDoubleWord(Register reg, uint32_t val) {
    if (localPlatform->isX2Apic) {
        Util::Exception::throwException(Util::Exception::UNSUPPORTED_OPERATION, "X2Apic mode not supported!");
        // ModelSpecificRegister msr = getMSR(reg);
        // msr.writeQuadWord(static_cast<uint64_t>(val)); // Atomic write
    } else {
        ensureMMIO();
        *reinterpret_cast<volatile uint32_t *>(localPlatform->virtAddress + reg) = val;
    }
}

SVREntry LocalApic::readSVR() {
    return static_cast<SVREntry>(readDoubleWord(SVR));
}

void LocalApic::writeSVR(const SVREntry &svrEntry) {
    writeDoubleWord(SVR, static_cast<uint32_t>(svrEntry));
}

LVTEntry LocalApic::readLVT(LocalInterrupt lint) {
    return static_cast<LVTEntry>(readDoubleWord(lintRegs[lint]));
}

void LocalApic::writeLVT(LocalInterrupt lint, const LVTEntry &lvtEntry) {
    writeDoubleWord(lintRegs[lint], static_cast<uint32_t>(lvtEntry));
}

ICREntry LocalApic::readICR() {
    Cpu::disableInterrupts(); // Do not let another interrupt handler fuck this up
    uint32_t low = readDoubleWord(ICR_LOW);
    uint64_t high = readDoubleWord(ICR_HIGH);
    Cpu::enableInterrupts();
    return static_cast<ICREntry>(low | high << 32);
}

void LocalApic::writeICR(const ICREntry &icrEntry) {
    auto val = static_cast<uint64_t>(icrEntry);
    Cpu::disableInterrupts(); // Do not let another interrupt handler fuck this up
    writeDoubleWord(ICR_HIGH, val >> 32);
    writeDoubleWord(ICR_LOW, val & 0xFFFFFFFF); // Writing the low DW sends the IPI
    Cpu::enableInterrupts();
}

// x2Apic is not supported
// ModelSpecificRegister LocalApic::getMSR(Register reg) {
//     // The MSR offsets match the MMIO offsets, but right shifted by 4 bits
//     return ModelSpecificRegister(localPlatform->msrAddress + (reg >> 4));
// }

}