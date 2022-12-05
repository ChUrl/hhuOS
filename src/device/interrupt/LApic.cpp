#include "LApic.h"
#include "lib/util/cpu/CpuId.h"
#include "kernel/system/System.h"
#include "kernel/paging/Paging.h"
#include "kernel/interrupt/InterruptDispatcher.h"
#include "device/cpu/Cpu.h"

namespace Device {

bool LApic::initialized = false;
uint32_t LApic::baseVirtAddress = 0;
Device::ModelSpecificRegister LApic::ia32ApicBaseMsr = Device::ModelSpecificRegister(0x1B);
Kernel::Logger LApic::log = Kernel::Logger::get("LApic");
const IoPort LApic::registerSelectorPort = IoPort(0x22);
const IoPort LApic::registerDataPort = IoPort(0x23);

/*
 * Public Member Functions
 */

bool LApic::isInitialized() {
    return initialized;
}

bool LApic::hasApicSupport() {
    const auto features = Util::Cpu::CpuId::getCpuFeatures();
    for (auto feature : features) {
        if (feature == Util::Cpu::CpuId::CpuFeature::APIC) {
            return true;
        }
    }
    return false;
}

bool LApic::hasX2ApicSupport() {
    const auto features = Util::Cpu::CpuId::getCpuFeatures();
    for (auto feature : features) {
        if (feature == Util::Cpu::CpuId::CpuFeature::X2APIC) {
            return true;
        }
    }
    return false;
}

// IA-32 Architecture Manual Chapter 10.12.1
bool LApic::isX2Apic() {
    return readMSR().isX2Apic;
}

bool LApic::isEnabled() {
    return isEnabledHW() && isEnabledSW();
}

// IA-32 Architecture Manual Chapter 10.4.6
uint8_t LApic::getId() {
    return (readDoubleWord(Register::ID) >> 24) & 0xFF;
}

// IA-32 Architecture Manual Chapter 10.4.8
uint8_t LApic::getVersion() {
    return readDoubleWord(Register::VER) & 0xFF;
}

void LApic::init() {
    if (!hasApicSupport()) {
        Util::Exception::throwException(Util::Exception::UNSUPPORTED_OPERATION,
                                        "LApic::init(): xApic support not present!");
    }

    // x2Apic doesn't have MMIO register access (x2Apic uses MSRs)
    if (hasX2ApicSupport() && isX2Apic()) {
        Util::Exception::throwException(Util::Exception::UNSUPPORTED_OPERATION,
                                        "LApic::init(): Only xApic mode is implemented!");
    }

    // TODO: Move MMIO stuff to other function
    auto& memoryService = Kernel::System::getService<Kernel::MemoryService>();
    auto& pageDirectory = memoryService.getKernelAddressSpace().getPageDirectory();

    // Using default physical address without relocation
    // TODO: Check if physAddress could be not 4kb aligned (then it would also cross page boundaries)
    // TODO: If unaligned add the offset to virtAddress (virtAddress += physAddress % PAGESIZE) and allocate 2 pages
    // NOTE: The default physical address is 4kb aligned and thus doesn't cross pages
    // NOTE: IO memory region is 0xEEE00000 - 0xFFFFFFFF, so it contains the default physical address
    // NOTE: (https://hhuos.github.io/docs/paging_mm#the-virtual-memory-layout-in-hhuos)
    void* virtAddress = memoryService.mapIO(APIC_BASE_DEFAULT_PHYS_ADDRESS, Util::Memory::PAGESIZE, true);

    if (virtAddress == nullptr) {
        Util::Exception::throwException(Util::Exception::OUT_OF_MEMORY,
                                        "LApic::init(): Not enough space left on kernel heap!");
    }

    // Set page to uncacheable as described in IA-32 Architecture Manual Chapter 10.4.1
    // TODO: "Strong Uncacheable"
    // TODO: How do I check that the mapping was successful?
    pageDirectory.setPageFlags(reinterpret_cast<uint32_t>(virtAddress),
                               Kernel::Paging::PRESENT | Kernel::Paging::DO_NOT_UNMAP
                               | Kernel::Paging::CACHE_DISABLE | Kernel::Paging::WRITE_THROUGH
                               | Kernel::Paging::READ_WRITE);

    // Use this address to access the local APIC's memory mapped registers
    baseVirtAddress = reinterpret_cast<uint32_t>(virtAddress);

    // HW Enable APIC without relocation
    enableHW();

    // Mask all the interrupts to reenable them when needed
    forbid(Interrupt::LINT0); // Gets reenabled when enabling virtual wire mode
    forbid(Interrupt::LINT1); // TODO: Not entirely sure what LINT1 is used for
    forbid(Interrupt::CMCI);
    forbid(Interrupt::TIMER);
    forbid(Interrupt::THERMAL);
    forbid(Interrupt::PERFORMANCE);
    forbid(Interrupt::ERROR);

    // SW Enable APIC by setting the Spurious Interrupt Vector Register with spurious vector number 0xFF (OSDev)
    // and the SW ENABLE flag. Also allow EOI broadcasting to other APICs/IO APICs
    writeSVR({ .spuriousVector = Kernel::InterruptDispatcher::SPURIOUS,
                     .isSWEnabled = true,
                     .hasEOIBroadcastSuppression = true });

    clearErrors(); // Clear possible error interrupts
    sendEndOfInterrupt(); // Clear other outstanding interrupts

    // NOTE: QEMU does this by default
    // Allow all interrupts to be forwarded to the CPU by setting the Task-Priority Class and Sub Class thresholds to 0
    // (IA-32 Architecture Manual Chapter 10.8.3.1)
    writeDoubleWord(Register::TPR, 0);

    initialized = true;

#if HHUOS_LAPIC_ENABLE_DEBUG == 1
    logDebugDump();
#endif
}

void LApic::allow(Interrupt lint, Kernel::InterruptDispatcher::Interrupt slot) {
    LVT_Entry entry = readLVT(lint);
    entry.slot = slot;
    entry.isMasked = false;
    writeLVT(lint, entry);
}

void LApic::forbid(Interrupt lint) {
    LVT_Entry entry = readLVT(lint);
    entry.isMasked = true;
    writeLVT(lint, entry);
}

bool LApic::status(Interrupt lint) {
    return readLVT(lint).isMasked;
}

void LApic::sendEndOfInterrupt() {
    writeDoubleWord(Register::EOI, 0);
}

void LApic::enableVirtualWireMode() {
    // NOTE: Interrupts have to be disabled beforehand
    registerSelectorPort.writeByte(0x70); // IMCR address is 0x70
    registerDataPort.writeByte(0x01); // 0x00 connects PIC to BSP, 0x01 connects APIC to BSP

    // Set LINT0 to ExtINT for external IC (PIC)
    writeLVT(Interrupt::LINT0, { .deliveryMode = LVT_Delivery_Mode::EXTINT, .isMasked = false });
}

void LApic::enableIoApicMode() {
    // NOTE: Interrupts have to be disabled beforehand
    registerSelectorPort.writeByte(0x70); // IMCR address is 0x70
    registerDataPort.writeByte(0x01); // 0x00 connects PIC to BSP, 0x01 connects APIC to BSP

    // Mask LINT0 to suppress external IC interrupts (PIC)
    writeLVT(Interrupt::LINT0, { .deliveryMode = LVT_Delivery_Mode::FIXED, .isMasked = true });
}

void LApic::verifyIPI() {
    // TODO: Unsafe if interrupts weren't enabled before
    writeICR({ .slot = Kernel::InterruptDispatcher::IPITEST,
               .deliveryMode = ICR_Delivery_Mode::FIXED,
               .triggerMode = ICR_Trigger_Mode::EDGE,
               .destinationShorthand = ICR_Destination_Shorthand::SELF });
}

/*
 * Private Member Functions
 */

// IA-32 Architecture Manual Chapter 10.4.4
LApic::MSR_Entry LApic::readMSR() {
    uint64_t val = ia32ApicBaseMsr.readQuadWord();

    return {
        .isBSP = static_cast<bool>((val & (1 << 8)) >> 8),
        .isX2Apic = static_cast<bool>((val & (1 << 10)) >> 10),
        .isHWEnabled = static_cast<bool>((val & (1 << 11)) >> 11),
        .baseField = static_cast<uint32_t>(val & 0xFFFFF000)
    };
}

// IA-32 Architecture Manual Chapter 10.4.4
void LApic::writeMSR(MSR_Entry entry) {
    uint64_t val = static_cast<uint64_t>(entry.isBSP) << 8
            | static_cast<uint64_t>(entry.isX2Apic) << 10
            | static_cast<uint64_t>(entry.isHWEnabled) << 11
            | static_cast<uint64_t>(entry.baseField) << 12;

    ia32ApicBaseMsr.writeQuadWord(val);
}

uint32_t LApic::readDoubleWord(uint16_t reg) {
    if (baseVirtAddress == 0) {
        Util::Exception::throwException(Util::Exception::NULL_POINTER,
                                        "LApic::readDoubleWord(): APIC MMIO not initialized!");
    }

    volatile auto* regAddr = reinterpret_cast<uint32_t*>(baseVirtAddress + reg);
    return *regAddr;
}

void LApic::writeDoubleWord(uint16_t reg, uint32_t val) {
    if (baseVirtAddress == 0) {
        Util::Exception::throwException(Util::Exception::NULL_POINTER,
                                        "LApic::writeDoubleWord(): APIC MMIO not initialized!");
    }

    volatile auto* regAddr = reinterpret_cast<uint32_t*>(baseVirtAddress + reg);
    *regAddr = val;
}

// IA-32 Architecture Manual Chapter 10.9
LApic::SVR_Entry LApic::readSVR() {
    uint32_t val = readDoubleWord(Register::SVR);

    return {
        .spuriousVector = static_cast<Kernel::InterruptDispatcher::Interrupt>(val & 0xFF),
        .isSWEnabled = static_cast<bool>((val & (1 << 8)) >> 8),
        .hasFocusProcessorChecking = static_cast<bool>((val & (1 << 9)) >> 9),
        .hasEOIBroadcastSuppression = static_cast<bool>((val & (1 << 12)) >> 12)
    };
}

// IA-32 Architecture Manual Chapter 10.9
void LApic::writeSVR(SVR_Entry svr) {
    uint32_t val = static_cast<uint32_t>(svr.spuriousVector)
            | static_cast<uint32_t>(svr.isSWEnabled) << 8
            | static_cast<uint32_t>(svr.hasFocusProcessorChecking) << 9
            | static_cast<uint32_t>(svr.hasEOIBroadcastSuppression) << 12;

    writeDoubleWord(Register::SVR, val);
}

// IA-32 Architecture Manual Chapter 10.5.1
LApic::LVT_Entry LApic::readLVT(Interrupt lint) {
    uint32_t val = readDoubleWord(lint);

    return {
        .slot = static_cast<Kernel::InterruptDispatcher::Interrupt>(val & 0xFF),
        .deliveryMode = static_cast<LVT_Delivery_Mode>((val & (0b111 << 8)) >> 8), // Mask is <<, result is shifted back
        .deliveryStatus = static_cast<LVT_Delivery_Status>((val & (1 << 12)) >> 12),
        .pinPolarity = static_cast<LVT_Pin_Polarity>((val & (1 << 13)) >> 13),
        .triggerMode = static_cast<LVT_Trigger_Mode>((val & (1 << 15)) >> 15),
        .isMasked = static_cast<bool>((val & (1 << 16)) >> 16),
        .timerMode = static_cast<LVT_Timer_Mode>((val & (0b11 << 17)) >> 17)
    };
}

// TODO: Check if it is a problem to write to readonly/reserved areas
// IA-32 Architecture Manual Chapter 10.5.1
void LApic::writeLVT(Interrupt lint, LVT_Entry entry) {
    uint32_t val = static_cast<uint32_t>(entry.slot)
            | static_cast<uint32_t>(entry.deliveryMode) << 8
            | static_cast<uint32_t>(entry.pinPolarity) << 13
            | static_cast<uint32_t>(entry.triggerMode) << 15
            | static_cast<uint32_t>(entry.isMasked) << 16
            | static_cast<uint32_t>(entry.timerMode) << 17;

    writeDoubleWord(lint, val);
}

// IA-32 Architecture Manual Chapter 10.6.1
LApic::ICR_Entry LApic::readICR() {
    // NOTE: Interrupts have to be disabled beforehand
    uint32_t low, high;
    low = readDoubleWord(Register::ICR_LOW);
    high = readDoubleWord(Register::ICR_HIGH);

    return {
        .slot = static_cast<Kernel::InterruptDispatcher::Interrupt>(low & 0xFF),
        .deliveryMode = static_cast<ICR_Delivery_Mode>((low & (0b111 << 8)) >> 8),
        .destinationMode = static_cast<ICR_Destination_Mode>((low & (1 << 11)) >> 11),
        .deliveryStatus = static_cast<ICR_Delivery_Status>((low & (1 << 12)) >> 12),
        .level = static_cast<ICR_Level>((low & (1 << 14)) >> 14),
        .triggerMode = static_cast<ICR_Trigger_Mode>((low & (1 << 15)) >> 15),
        .destinationShorthand = static_cast<ICR_Destination_Shorthand>((low & (0b11 << 18)) >> 18),
        .destinationField = static_cast<uint8_t>(high >> 24)
    };
}

// IA-32 Architecture Manual Chapter 10.6.1
void LApic::writeICR(ICR_Entry icr) {
    uint32_t low, high;
    low = static_cast<uint32_t>(icr.slot)
            | static_cast<uint32_t>(icr.deliveryMode) << 8
            | static_cast<uint32_t>(icr.destinationMode) << 11
            | static_cast<uint32_t>(icr.deliveryStatus) << 12
            | static_cast<uint32_t>(icr.level) << 14
            | static_cast<uint32_t>(icr.triggerMode) << 15
            | static_cast<uint32_t>(icr.destinationShorthand) << 18;
    high = static_cast<uint32_t>(icr.destinationField) << 24;

    // NOTE: Interrupts have to be disabled beforehand
    writeDoubleWord(Register::ICR_HIGH, high);
    writeDoubleWord(Register::ICR_LOW, low); // Last as writing low DW sends the IPI
}

void LApic::enableHW() {
    MSR_Entry msr = readMSR();
    msr.isBSP = true;
    msr.isHWEnabled = true;
    writeMSR(msr);
}

void LApic::enableHW(uint32_t base_address) {
    MSR_Entry msr = readMSR();
    msr.isBSP = true;
    msr.isHWEnabled = true;
    msr.baseField = base_address;
    writeMSR(msr);
}

void LApic::disableHW() {
    MSR_Entry msr = readMSR();
    msr.isHWEnabled = false;
    writeMSR(msr);
}

bool LApic::isEnabledHW() {
    return readMSR().isHWEnabled;
}

bool LApic::isEnabledSW() {
    return readSVR().isSWEnabled;
}

void LApic::clearErrors() {
    // TODO: Why is this written twice in xv6?
    writeDoubleWord(Register::ESR, 0);
    writeDoubleWord(Register::ESR, 0);
}

#if HHUOS_LAPIC_ENABLE_DEBUG == 1
void LApic::logDebugDump() {
    uint8_t id = getId();
    uint8_t version = getVersion();

    log.debug("Has Apic Support: %u", hasApicSupport());
    log.debug("Has x2Apic Support: %u (Is x2Apic: %u)", hasX2ApicSupport(), isX2Apic());
    log.debug("Local APIC Enabled: %u (HW: %u, SW: %u)", isEnabled(), isEnabledHW(), isEnabledSW());
    log.debug("Local APIC Base Phys Address: 0x%x", APIC_BASE_DEFAULT_PHYS_ADDRESS);
    log.debug("Local APIC Base Virt Address: 0x%x", reinterpret_cast<uint32_t>(baseVirtAddress));
    log.debug("Local APIC ID: %u", id);
    log.debug("Local APIC VER: 0x%x (Integrated APIC: %u)", version, 0x10 <= version && version <= 0x15);
    log.debug("Local APIC Spurious interrupt vector: 0x%x", readSVR().spuriousVector);
}
#endif

}