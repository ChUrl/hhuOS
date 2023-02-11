#include "LocalApic.h"
#include "kernel/service/InterruptService.h"
#include "kernel/system/System.h"
#include "lib/util/hardware/CpuId.h"
#include "lib/util/base/Constants.h"
#include "Apic.h"

namespace Device {

LocalApicPlatform *LocalApic::localPlatform = nullptr;
bool LocalApic::bspInitialized = false;
const ModelSpecificRegister LocalApic::ia32ApicBaseMsr = ModelSpecificRegister(0x1B);
const LocalApic::Register LocalApic::lintRegs[7] = {static_cast<Register>(0x2F0),
                                                    static_cast<Register>(0x320),
                                                    static_cast<Register>(0x330),
                                                    static_cast<Register>(0x340),
                                                    static_cast<Register>(0x350),
                                                    static_cast<Register>(0x360),
                                                    static_cast<Register>(0x370)};
const IoPort LocalApic::registerSelectorPort = IoPort(0x22);
const IoPort LocalApic::registerDataPort = IoPort(0x23);
Kernel::Logger LocalApic::log = Kernel::Logger::get("LocalApic");

LocalApic::LocalApic(LocalApicPlatform *localApicPlatform, const LocalApicInformation &&localApicInformation)
        : localInfo(localApicInformation) {
    localPlatform = localApicPlatform;
}

uint8_t LocalApic::getId() {
    return readDoubleWord(ID) >> 24;
}

bool LocalApic::supportsXApic() {
    auto features = Util::Hardware::CpuId::getCpuFeatures();
    for (auto feature: features) {
        if (feature == Util::Hardware::CpuId::CpuFeature::APIC) {
            return true;
        }
    }
    return false;
}

bool LocalApic::supportsX2Apic() {
    auto features = Util::Hardware::CpuId::getCpuFeatures();
    for (auto feature: features) {
        if (feature == Util::Hardware::CpuId::CpuFeature::X2APIC) {
            return true;
        }
    }
    return false;
}

uint8_t LocalApic::getVersion() {
    return readDoubleWord(VER);
}

void LocalApic::ensureBspInitialized() {
    if (!bspInitialized) {
        Util::Exception::throwException(Util::Exception::ILLEGAL_STATE, "BSP not initialized!");
    }
}

uint8_t LocalApic::initializeBsp() {
    if (bspInitialized) {
        log.error("BSP already initialized, skipping initialization!");
        return getId();
    }

    if (!readBaseMSR().isBSP) {
        // IA32_APIC_BASE_MSR is unique (every core has its own)
        Util::Exception::throwException(Util::Exception::UNSUPPORTED_OPERATION, "May only be called by the BSP!");
    }

    disablePicMode(); // Physically connect the APIC to the BSP, just in case

    // Set operating mode:
    // This implementation only supports xApic mode. Because the local APIC starts with xApic mode and every AP uses
    // the same address space, memory allocation only has to be done once and the IA32_APIC_BASE_MSR does not
    // have to be written. To enable x2Apic mode, every AP would have to set the x2Apic-enable flag in its
    // IA32_APIC_BASE_MSR, in that case this code should be relocated to LocalApic::initializeAp().
    if (supportsX2Apic()) {
        // QEMU doesn't support emulation of x2Apic via TCG (QEMU Tiny Code Generator)
        // KVM would be possible, but then GDB can't be attached, so compatibility mode will always be chosen
        log.info("X2Apic support detected but not implemented, running in xApic compatibility mode");
    } else {
        log.info("No x2Apic mode support detected, running in xApic mode");
    }
    localPlatform->isX2Apic = false;
    initializeXApicMMIO();

    // Mask all PIC interrupts that have been enabled previously. This has to be done before bspInitialized
    // is set to true, to reach the PIC through the InterruptService. After the APIC has been initialized, the
    // InterruptService only reaches the I/O APIC's REDTBL registers.
    auto &interruptService = Kernel::System::getService<Kernel::InterruptService>();
    for (uint8_t i = 0; i < 16; ++i) {
        interruptService.forbidHardwareInterrupt(static_cast<InterruptRequest>(i));
    }

    bspInitialized = true; // System is now ready for LocalApic::initializeAp().
    return getId(); // The BSP's id is required to call LocalApic::initializeAp() on the correct LocalApic instance.
}

void LocalApic::initializeAp() {
    ensureBspInitialized();

    // Mask all local interrupt sources
    initializeLVT();

    // Configure the non maskable interrupt pin
    // This is always LINT1, but as ACPI reports this it doesn't have to be set statically
    LVTEntry lvtEntry{};
    lvtEntry.vector = static_cast<Kernel::InterruptVector>(0); // NMI doesn't have vector
    lvtEntry.deliveryMode = LVTEntry::DeliveryMode::NMI;
    lvtEntry.pinPolarity = localInfo.nmiPolarity;
    lvtEntry.triggerMode = localInfo.nmiTriggerMode;
    lvtEntry.isMasked = false;
    writeLVT(localInfo.nmiLint == 0 ? LINT0 : LINT1, lvtEntry);

    // SW Enable APIC by setting the Spurious Interrupt Vector Register with spurious vector number 0xFF
    // and the SW ENABLE flag.
    SVREntry svrEntry{};
    svrEntry.vector = Kernel::InterruptVector::SPURIOUS;
    svrEntry.isSWEnabled = true;
    svrEntry.hasEOIBroadcastSuppression = true; // TODO
    writeSVR(svrEntry);

    // Clear outstanding stuff
    clearErrors();
    sendEndOfInterrupt();

    // Allow all interrupts to be forwarded to the CPU by setting the Task-Priority Class and Sub Class thresholds to 0
    writeDoubleWord(TPR, 0);
    initialized = true;
}

void LocalApic::disablePicMode() {
    registerSelectorPort.writeByte(0x70); // IMCR address is 0x70
    registerDataPort.writeByte(0x01); // 0x00 connects PIC to LINT0, 0x01 disconnects
}

void LocalApic::sendIpiInit(uint8_t id, ICREntry::Level level) {
    ICREntry icrEntry{};
    icrEntry.vector = static_cast<Kernel::InterruptVector>(0), // INIT should have vector number 0
    icrEntry.deliveryMode = ICREntry::DeliveryMode::INIT,
    icrEntry.destinationMode = ICREntry::DestinationMode::PHYSICAL,
    icrEntry.level = level, // ASSERT or DEASSERT
    icrEntry.triggerMode = ICREntry::TriggerMode::LEVEL,
    icrEntry.destinationShorthand = ICREntry::DestinationShorthand::NO, // Only broadcast to CPU in destination field
    icrEntry.destination = id;
    writeICR(icrEntry); // Writing ICR issues IPI

    do {
        asm volatile ("pause" : : : "memory");
    } while (readICR().deliveryStatus == ICREntry::DeliveryStatus::PENDING); // Wait for delivery
}

void LocalApic::sendIpiStartup(uint8_t id, uint32_t startupCodeAddress) {
    ICREntry icrEntry{};
    icrEntry.vector = static_cast<Kernel::InterruptVector>(startupCodeAddress >> 12), // Startup code physical page
    icrEntry.deliveryMode = ICREntry::DeliveryMode::STARTUP,
    icrEntry.destinationMode = ICREntry::DestinationMode::PHYSICAL, // Ignored
    icrEntry.level = ICREntry::Level::DEASSERT, // Ignored
    icrEntry.triggerMode = ICREntry::TriggerMode::EDGE, // Ignored
    icrEntry.destinationShorthand = ICREntry::DestinationShorthand::NO, // Ignored
    icrEntry.destination = id;
    writeICR(icrEntry); // Writing ICR issues IPI

    for (uint32_t j = 0; j < 1000000; ++j) {} // Ugly wait, because we have no PIT yet

    do {
        asm volatile ("pause" : : : "memory");
    } while (readICR().deliveryStatus == ICREntry::DeliveryStatus::PENDING); // Wait for delivery
}

void LocalApic::clearErrors() {
    // Clear possible error interrupts (write twice because ESR is read/write register, writing once does not
    // change a subsequently read value, in fact the register should always be written once before reading)
    writeDoubleWord(ESR, 0);
    writeDoubleWord(ESR, 0);
}

void LocalApic::allow(LocalInterrupt lint) {
    LVTEntry entry = readLVT(lint);
    entry.isMasked = false;
    writeLVT(lint, entry);
}

void LocalApic::forbid(LocalInterrupt lint) {
    LVTEntry entry = readLVT(lint);
    entry.isMasked = true;
    writeLVT(lint, entry);
}

bool LocalApic::status(LocalInterrupt lint) {
    return readLVT(lint).isMasked;
}

void LocalApic::sendEndOfInterrupt() {
    // This works for multiple cores because the core that handles the interrupt calls this function
    // and thus reaches the correct local APIC
    writeDoubleWord(EOI, 0);
}

void LocalApic::ensureRegisterAccess() {
    if (!localPlatform->isX2Apic && localPlatform->virtAddress == 0) {
        Util::Exception::throwException(Util::Exception::ILLEGAL_STATE, "LocalApic MMIO region not initialized!");
    }
}

void LocalApic::initializeXApicMMIO() {
    uint32_t physAddress = localPlatform->physAddress;
    uint32_t pageOffset = physAddress % Util::PAGESIZE;

    auto &memoryService = Kernel::System::getService<Kernel::MemoryService>();
    void *virtAddress = memoryService.mapIO(physAddress, Util::PAGESIZE, true);

    // Account for possible misalignment
    localPlatform->virtAddress = reinterpret_cast<uint32_t>(virtAddress) + pageOffset;
}

void LocalApic::initializeLVT() {
    // Default values
    LVTEntry lvtEntry{};
    lvtEntry.deliveryMode = LVTEntry::DeliveryMode::FIXED, // TODO: Do I need to care about deliveryMode?
    lvtEntry.pinPolarity = LVTEntry::PinPolarity::HIGH,
    lvtEntry.triggerMode = LVTEntry::TriggerMode::EDGE,
    lvtEntry.isMasked = true;

    // Set all the vector numbers
    lvtEntry.vector = Kernel::InterruptVector::CMCI;
    writeLVT(CMCI, lvtEntry); // The CMCI might not exist
    lvtEntry.vector = Kernel::InterruptVector::APICTIMER;
    writeLVT(TIMER, lvtEntry);
    lvtEntry.vector = Kernel::InterruptVector::THERMAL;
    writeLVT(THERMAL, lvtEntry);
    lvtEntry.vector = Kernel::InterruptVector::PERFORMANCE;
    writeLVT(PERFORMANCE, lvtEntry);
    lvtEntry.vector = Kernel::InterruptVector::LINT0;
    writeLVT(LINT0, lvtEntry);
    lvtEntry.vector = Kernel::InterruptVector::LINT1;
    writeLVT(LINT1, lvtEntry);
    lvtEntry.vector = Kernel::InterruptVector::ERROR;
    writeLVT(ERROR, lvtEntry);
}

#if HHUOS_APIC_ENABLE_DEBUG == 1

void LocalApic::dumpLVT() {
    log.info("Local Vector Table (Local APIC Id: [%d]):", getId());
    for (uint8_t lint = CMCI; lint <= ERROR; ++lint) {
        LVTEntry lvtEntry = readLVT(static_cast<LocalInterrupt>(lint));
        log.debug("- Interrupt [%s]: (Vector: [0x%x], Masked: [%d], DeliveryMode: [0b%b], PinPolarity: [%s], TriggerMode: [%s])",
                  lintNames[lint],
                  static_cast<uint8_t>(lvtEntry.vector),
                  static_cast<uint8_t>(lvtEntry.isMasked),
                  static_cast<uint8_t>(lvtEntry.deliveryMode),
                  lvtEntry.pinPolarity == LVTEntry::PinPolarity::HIGH ? "HIGH" : "LOW",
                  lvtEntry.triggerMode == LVTEntry::TriggerMode::EDGE ? "EDGE" : "LEVEL");
    }
}

#endif

BaseMSREntry LocalApic::readBaseMSR() {
    return static_cast<BaseMSREntry>(ia32ApicBaseMsr.readQuadWord()); // Atmoic read
}

void LocalApic::writeBaseMSR(const BaseMSREntry &msrEntry) {
    ia32ApicBaseMsr.writeQuadWord(static_cast<uint64_t>(msrEntry)); // Atomic write
}

uint32_t LocalApic::readDoubleWord(Register reg) {
    ensureRegisterAccess();
    if (localPlatform->isX2Apic) {
        Util::Exception::throwException(Util::Exception::UNSUPPORTED_OPERATION, "X2Apic mode not supported!");
        // ModelSpecificRegister msr = getMSR(reg);
        // return static_cast<uint32_t>(msr.readQuadWord()); // Atomic read
    } else {
        return *reinterpret_cast<volatile uint32_t *>(localPlatform->virtAddress + reg);
    }
}

void LocalApic::writeDoubleWord(Register reg, uint32_t val) {
    ensureRegisterAccess();
    if (localPlatform->isX2Apic) {
        Util::Exception::throwException(Util::Exception::UNSUPPORTED_OPERATION, "X2Apic mode not supported!");
        // ModelSpecificRegister msr = getMSR(reg);
        // msr.writeQuadWord(static_cast<uint64_t>(val)); // Atomic write
    } else {
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
    uint32_t low = readDoubleWord(ICR_LOW);
    uint64_t high = readDoubleWord(ICR_HIGH);
    return static_cast<ICREntry>(low | high << 32);
}

void LocalApic::writeICR(const ICREntry &icrEntry) {
    auto val = static_cast<uint64_t>(icrEntry);
    writeDoubleWord(ICR_HIGH, val >> 32);
    writeDoubleWord(ICR_LOW, val & 0xFFFFFFFF); // Writing the low DW sends the IPI
}

// x2Apic is not supported
// ModelSpecificRegister LocalApic::getMSR(Register reg) {
//     // The MSR offsets match the MMIO offsets, but right shifted by 4 bits
//     return ModelSpecificRegister(localPlatform->msrAddress + (reg >> 4));
// }

}
