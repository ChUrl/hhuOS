#include "LocalApic.h"
#include "Apic.h"
#include "kernel/service/InterruptService.h"
#include "kernel/system/System.h"
#include "lib/util/base/Constants.h"
#include "lib/util/hardware/CpuId.h"

namespace Device {

uint32_t LocalApic::baseAddress = 0;
uint32_t LocalApic::mmioAddress = 0;
const ModelSpecificRegister LocalApic::ia32ApicBaseMsr = ModelSpecificRegister(0x1B);
const Util::Array<LocalApic::Register> LocalApic::lintRegs = {static_cast<Register>(0x2F0),
                                                              static_cast<Register>(0x320),
                                                              static_cast<Register>(0x330),
                                                              static_cast<Register>(0x340),
                                                              static_cast<Register>(0x350),
                                                              static_cast<Register>(0x360),
                                                              static_cast<Register>(0x370)};
Util::Async::Spinlock LocalApic::icrLock;

Kernel::Logger LocalApic::log = Kernel::Logger::get("LocalApic");

LocalApic::LocalApic(uint8_t cpuId, uint32_t baseAddress,
                     LocalInterrupt nmiLint, LVTEntry::PinPolarity nmiPolarity, LVTEntry::TriggerMode nmiTrigger)
  : cpuId(cpuId), nmiLint(nmiLint), nmiPolarity(nmiPolarity), nmiTrigger(nmiTrigger) {
    LocalApic::baseAddress = baseAddress;
}

bool LocalApic::supportsXApic() {
    auto features = Util::Hardware::CpuId::getCpuFeatures();
    for (auto feature : features) {
        if (feature == Util::Hardware::CpuId::CpuFeature::APIC) {
            return true;
        }
    }
    return false;
}

bool LocalApic::supportsX2Apic() {
    auto features = Util::Hardware::CpuId::getCpuFeatures();
    for (auto feature : features) {
        if (feature == Util::Hardware::CpuId::CpuFeature::X2APIC) {
            return true;
        }
    }
    return false;
}

uint8_t LocalApic::getId() {
    return readDoubleWord(ID) >> 24;
}

uint8_t LocalApic::getVersion() {
    return readDoubleWord(VER) & 0xFF;
}

void LocalApic::initialize() {
    // Mask all local interrupt sources
    initializeLVT();

    // Configure the non maskable interrupt pin.
    // This is usually LINT1, edge-triggered and active-high, but ACPI reports this in case of deviations.
    LVTEntry lvtEntry{};
    lvtEntry.vector = static_cast<Kernel::InterruptVector>(0); // NMI doesn't have vector
    lvtEntry.deliveryMode = LVTEntry::DeliveryMode::NMI;
    lvtEntry.pinPolarity = nmiPolarity;
    lvtEntry.triggerMode = nmiTrigger;
    lvtEntry.isMasked = false;
    writeLVT(nmiLint, lvtEntry);

    // SW Enable APIC by setting the Spurious Interrupt Vector Register with spurious vector number 0xFF
    // and the SW ENABLE flag.
    SVREntry svrEntry{};
    svrEntry.vector = Kernel::InterruptVector::SPURIOUS;
    svrEntry.isSWEnabled = true;
    svrEntry.suppressEoiBroadcasting = false; // EOI level triggered external interrupts through the local APIC
    writeSVR(svrEntry);

    // Clear outstanding stuff
    clearErrors();
    sendEndOfInterrupt();

    // Allow all interrupts to be forwarded to the CPU by setting the Task-Priority Class and Sub Class thresholds to 0
    // This should be 0 after power-up, but it doesn't hurt to set it again
    writeDoubleWord(TPR, 0);

    initialized = true;
}

void LocalApic::enableXApicMode() {
    // Mask all PIC interrupts that have been enabled previously. After the APIC has been initialized, the
    // InterruptService only reaches the I/O APIC's REDTBL registers.
    // At this point, no PIC interrupts should be unmasked, plugging in interrupt handlers should
    // be done after the APIC is initialized!
    // Otherwise, these would be "plugged out" here.
    auto &interruptService = Kernel::System::getService<Kernel::InterruptService>();
    for (uint8_t i = 0; i < 16; ++i) {
        interruptService.forbidHardwareInterrupt(static_cast<InterruptRequest>(i));
    }

    // Physically connect the APIC to the BSP, just in case the IMCR actually exists
    IoPort(0x22).writeByte(0x70); // Select IMCR at 0x70
    IoPort(0x23).writeByte(0x01); // Write IMCR, 0x00 connects PIC to LINT0, 0x01 disconnects

    // The memory allocated here is never freed, because this implementation does not support
    // disabling the APIC after enabling it.
    // If this is supposed to be freed, the last LocalApic instance has to do it.
    auto &memoryService = Kernel::System::getService<Kernel::MemoryService>();
    void *virtAddress = memoryService.mapIO(baseAddress, Util::PAGESIZE);

    // Account for possible misalignment, as mapIO returns a page-aligned pointer
    const uint32_t pageOffset = baseAddress % Util::PAGESIZE;
    mmioAddress = reinterpret_cast<uint32_t>(virtAddress) + pageOffset;

    // This implementation only supports xApic mode. Because the local APIC starts with xApic mode and every AP uses
    // the same address space, memory allocation only has to be done once and the IA32_APIC_BASE_MSR does not
    // have to be written. To enable x2Apic mode, every AP would have to set the x2Apic-enable flag in its
    // IA32_APIC_BASE_MSR, without requiring the MMIO region.
    log.info("Running in xApic mode.");
}

void LocalApic::sendIpiStartup(uint8_t id, uint32_t startupCodeAddress) {
    ICREntry icrEntry{};
    icrEntry.vector = static_cast<Kernel::InterruptVector>(startupCodeAddress >> 12); // Startup code physical page
    icrEntry.deliveryMode = ICREntry::DeliveryMode::STARTUP;
    icrEntry.destinationMode = ICREntry::DestinationMode::PHYSICAL;
    icrEntry.level = ICREntry::Level::ASSERT;
    icrEntry.triggerMode = ICREntry::TriggerMode::EDGE;
    icrEntry.destinationShorthand = ICREntry::DestinationShorthand::NO;
    icrEntry.destination = id;
    writeICR(icrEntry); // Writing ICR issues IPI
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

void LocalApic::initializeLVT() {
    // Default values
    LVTEntry lvtEntry{};
    lvtEntry.deliveryMode = LVTEntry::DeliveryMode::FIXED;
    lvtEntry.pinPolarity = LVTEntry::PinPolarity::HIGH;
    lvtEntry.triggerMode = LVTEntry::TriggerMode::EDGE;
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

void LocalApic::dumpLVT() {
    const Util::Array<const char *> lintNames = {"CMCI", "TIMER", "THERMAL", "PERFORMANCE", "LINT0", "LINT1", "ERROR"};

    log.info("Local Vector Table (Local APIC Id: [%d]):", getId());
    for (uint8_t lint = CMCI; lint <= ERROR; ++lint) {
        const LVTEntry lvtEntry = readLVT(static_cast<LocalInterrupt>(lint));
        log.info("- Local Interrupt [%s]: (Vector: [0x%x], Masked: [%d], DeliveryMode: [0b%b], PinPolarity: [%s], TriggerMode: [%s])",
                 lintNames[lint],
                 static_cast<uint8_t>(lvtEntry.vector),
                 static_cast<uint8_t>(lvtEntry.isMasked),
                 static_cast<uint8_t>(lvtEntry.deliveryMode),
                 lvtEntry.pinPolarity == LVTEntry::PinPolarity::HIGH ? "HIGH" : "LOW",
                 lvtEntry.triggerMode == LVTEntry::TriggerMode::EDGE ? "EDGE" : "LEVEL");
    }
}

BaseMSREntry LocalApic::readBaseMSR() {
    return static_cast<BaseMSREntry>(ia32ApicBaseMsr.readQuadWord()); // Atomic read
}

void LocalApic::writeBaseMSR(const BaseMSREntry &msrEntry) {
    ia32ApicBaseMsr.writeQuadWord(static_cast<uint64_t>(msrEntry)); // Atomic write
}

uint32_t LocalApic::readDoubleWord(Register reg) {
    return *reinterpret_cast<volatile uint32_t *>(mmioAddress + reg);
}

void LocalApic::writeDoubleWord(Register reg, uint32_t val) {
    *reinterpret_cast<volatile uint32_t *>(mmioAddress + reg) = val;
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
    icrLock.acquire(); // This needs to be synchronized in case multiple APs issue IPIs
    const uint32_t low = readDoubleWord(ICR_LOW);
    const uint64_t high = readDoubleWord(ICR_HIGH);
    icrLock.release();
    return static_cast<ICREntry>(low | high << 32);
}

void LocalApic::writeICR(const ICREntry &icrEntry) {
    auto val = static_cast<uint64_t>(icrEntry);
    icrLock.acquire(); // This needs to be synchronized in case multiple APs issue IPIs
    writeDoubleWord(ICR_HIGH, val >> 32);
    writeDoubleWord(ICR_LOW, val & 0xFFFFFFFF); // Writing the low DW sends the IPI
    icrLock.release();
}

} // namespace Device
