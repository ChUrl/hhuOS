#include "LApic.h"
#include "kernel/system/System.h"

namespace Device {

bool LApic::initialized = false;
Device::ModelSpecificRegister LApic::ia32ApicBaseMsr = Device::ModelSpecificRegister(0x1B);
Kernel::Logger LApic::log = Kernel::Logger::get("LApic");
uint16_t LApic::lintRegs[7] = {0x2F0, 0x320, 0x330, 0x340, 0x350, 0x360, 0x370};

// ! Public member functions start here

bool LApic::isInitialized() {
    return initialized;
}

void LApic::verifyInitialized() {
    if (!initialized) {
        Util::Exception::throwException(Util::Exception::ILLEGAL_STATE, "LApic is not initialized!");
    }
}

void LApic::initialize() {
    InterruptArchitecture::verifyApic();
    if (!readBaseMSR().isBSP) {
        // NOTE: IA32_APIC_BASE_MSR is unique (every core has its own)
        Util::Exception::throwException(Util::Exception::UNSUPPORTED_OPERATION, "LApic::initialize(): May only be called by BSP!");
    }

    InterruptArchitecture::localPlatform->isX2Apic = readBaseMSR().isX2Apic;

    // TODO: Does every AP has to call this before initializing its local APIC?
    //       - Every AP writes/reads the same physical address, but different registers are affected
    //       - How does paging work in MP? Do I have to mapIO the same physical address multiple times?
    //         - Probably not if there is only a single kernel address space
    //       - Every AP has its own MMU, but are page tables shared or individual?
    //         - It would make sense to keep the kernel addresses only once
    //         - I guess the TLB at least has to be consistent over APs regarding kernel address space?
    initializeMMIORegion();

    // Initialize the local APIC of the BSP before initializing any APs
    // Enables xApic compatibility mode if necessary
    initializeController(InterruptArchitecture::getLApicInformation(getId()));

    // TODO: I hate this
    InterruptArchitecture::localPlatform->version = readDoubleWord(Register::VER) & 0xFF;

    // TODO: Should probably not do this automatically inside LApic::initialize()...
    for (auto *lapic : InterruptArchitecture::lapics()) {
        // TODO: !lapic->enabled == true could also mean that the cpu is just not initialized yet...
        if (!lapic->enabled || lapic->id == getId()) { // Skip BSP and unavailable processors
            continue;
        }

        initializeApplicationProcessor(lapic);
    }

    // TODO: Mask all the PIC interrupts in the PIC aswell (they should still be all masked though...)
    // TODO: Make some PIC functions (allow, forbid, status) static so I can just call them?

    initialized = true;
}

void LApic::allow(Lint lint) {
    LVTEntry entry = readLVT(lint);
    entry.isMasked = false;
    writeLVT(lint, entry);
}

void LApic::forbid(Lint lint) {
    LVTEntry entry = readLVT(lint);
    entry.isMasked = true;
    writeLVT(lint, entry);
}

bool LApic::status(Lint lint) {
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
    writeDoubleWord(Register::ESR, 0);
    writeDoubleWord(Register::ESR, 0);
}

// ! Private member functions start here

void LApic::verifyMMIO() {
    if (InterruptArchitecture::localPlatform->virtAddress == 0) {
        Util::Exception::throwException(Util::Exception::ILLEGAL_STATE, "LApic MMIO region not initialized!");
    }
}

uint8_t LApic::getId() {
    return (readDoubleWord(Register::ID) >> 24) & 0xFF;
}

void LApic::initializeMMIORegion() {
    // TODO: mapIO strong uncacheable?
    // Allocate memory (4 kb) for the memory mapped registers (without relocation)
    // The address returned is page aligned, if the size crosses page boundary an additional page will be allocated.
    auto &memoryService = Kernel::System::getService<Kernel::MemoryService>();
    void *virtAddress = memoryService.mapIO(InterruptArchitecture::localPlatform->address, Util::Memory::PAGESIZE, true);

    if (virtAddress == nullptr) {
        Util::Exception::throwException(Util::Exception::OUT_OF_MEMORY, "LApic::initialize(): Not enough space left on kernel heap!");
    }

    // Use this address to access the local APIC's memory mapped registers
    InterruptArchitecture::localPlatform->virtAddress = reinterpret_cast<uint32_t>(virtAddress)
            + InterruptArchitecture::localPlatform->address % Util::Memory::PAGESIZE;
}

void LApic::initializeApplicationProcessor(LApicInformation *lapic) {
    // TODO: Prepare stack for entrycode
    // TODO: Send INIT and STARTUP interrupts with entry code address
    // TODO: The entrycode needs to call initializeController to initialize its own local APIC registers

    lapic->enabled = true;
}

// TODO: Does this have to be synchronized when initializing other APs?
//       - Probably not as all of them work in different address spaces?
//       - Also only one is initialized at a time (and MP init sequence requires acquiring BIOS semaphore...)
// TODO: IA-32 Architecture Manual Chapter 8.4.3.5: APIC ID has to be signalled to ACPI?
void LApic::initializeController(LApicInformation *lapic) {
    // x2Apic doesn't have MMIO register access (x2Apic uses MSRs)
    if (InterruptArchitecture::localPlatform->x2ApicSupported && InterruptArchitecture::localPlatform->isX2Apic) {
        MSREntry msrEntry = readBaseMSR();
        msrEntry.isX2Apic = false; // Operate in xApic compatibility mode // TODO: Test this
        writeBaseMSR(msrEntry);
        InterruptArchitecture::localPlatform->isX2Apic = false;
    }

    initializeLVT();

    // Configure the NMI (non maskable interrupt) pin
    LNMIConfiguration *lnmi = InterruptArchitecture::getLNMIConfiguration(lapic);
    if (lnmi != nullptr) {
        LVTEntry lvtEntry {};
        lvtEntry.vector = static_cast<Kernel::InterruptDispatcher::Interrupt>(0); // NMI doesn't have vector
        lvtEntry.deliveryMode = LVTEntry::DeliveryMode::NMI;
        lvtEntry.pinPolarity = lnmi->polarity;
        lvtEntry.triggerMode = lnmi->triggerMode;
        lvtEntry.isMasked = false;
        writeLVT(lnmi->lint == 0 ? LINT0 : LINT1, lvtEntry);
    }

    // SW Enable APIC by setting the Spurious Interrupt Vector Register with spurious vector number 0xFF (OSDev)
    // and the SW ENABLE flag.
    SVREntry svrEntry {};
    svrEntry.vector = Kernel::InterruptDispatcher::SPURIOUS;
    svrEntry.isSWEnabled = true;
    svrEntry.hasEOIBroadcastSuppression = true;
    writeSVR(svrEntry);

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
    LVTEntry lvtEntry {};
    lvtEntry.deliveryMode = LVTEntry::DeliveryMode::FIXED, // TODO
    lvtEntry.pinPolarity = LVTEntry::PinPolarity::HIGH,
    lvtEntry.triggerMode = LVTEntry::TriggerMode::EDGE,
    lvtEntry.isMasked = true;

    // Set all the vector numbers
    lvtEntry.vector = Kernel::InterruptDispatcher::CMCI;
    writeLVT(CMCI, lvtEntry); // TODO: The CMCI might not exist?
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

// ! Private register member functions start here

// NOTE: https://forum.osdev.org/viewtopic.php?f=1&t=16653#p123105
// NOTE: In x2APIC mode this can be written atomically, so no spinlock there
// TODO: Needs spinlock?
//       - If a single cpu would somehow write the regAddr, switch threads and then read regAddr this would be a bug
//         This case shouldn't happen though as there is no (really?) situation where a thread would write these...
//       - If a cpu writes the regAddr and another cpu writes the regAddr again there would be no difference as
//         different registers would be written
//       - If an accepted interrupt can change register values interrupts would have to be disabled
uint32_t LApic::readDoubleWord(uint16_t reg) {
    verifyMMIO();

    volatile auto *regAddr = reinterpret_cast<uint32_t *>(InterruptArchitecture::localPlatform->virtAddress + reg);
    volatile auto val = *regAddr;

    return val;
}

// TODO: Needs spinlock?
void LApic::writeDoubleWord(uint16_t reg, uint32_t val) {
    verifyMMIO();

    volatile auto *regAddr = reinterpret_cast<uint32_t *>(InterruptArchitecture::localPlatform->virtAddress + reg);
    *regAddr = val;
}

// IA-32 Architecture Manual Chapter 10.4.4
MSREntry LApic::readBaseMSR() {
    return MSREntry(ia32ApicBaseMsr.readQuadWord());
}

// IA-32 Architecture Manual Chapter 10.4.4
void LApic::writeBaseMSR(MSREntry msrEntry) {
    ia32ApicBaseMsr.writeQuadWord(static_cast<uint64_t>(msrEntry)); // Atomic write
}

// IA-32 Architecture Manual Chapter 10.9
SVREntry LApic::readSVR() {
    return SVREntry(readDoubleWord(Register::SVR));
}

// IA-32 Architecture Manual Chapter 10.9
void LApic::writeSVR(SVREntry svrEntry) {
    writeDoubleWord(Register::SVR, static_cast<uint32_t>(svrEntry));
}

// IA-32 Architecture Manual Chapter 10.5.1
LVTEntry LApic::readLVT(Lint lint) {
    return LVTEntry(readDoubleWord(lintRegs[lint]));
}

// TODO: Check if it is a problem to write to readonly/reserved areas
// IA-32 Architecture Manual Chapter 10.5.1
void LApic::writeLVT(Lint lint, LVTEntry lvtEntry) {
    writeDoubleWord(lintRegs[lint], static_cast<uint32_t>(lvtEntry));
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
    uint32_t low = readDoubleWord(Register::ICR_LOW);
    uint64_t high = readDoubleWord(Register::ICR_HIGH);
    return ICREntry(low | high << 32);
}

// TODO: Needs spinlock?
// IA-32 Architecture Manual Chapter 10.6.1
void LApic::writeICR(ICREntry icrEntry) {
    // NOTE: Interrupts have to be disabled beforehand
    auto val = static_cast<uint64_t>(icrEntry);
    writeDoubleWord(Register::ICR_HIGH, val >> 32);
    writeDoubleWord(Register::ICR_LOW, val & 0xFFFFFFFF); // Last as writing low DW sends the IPI
}

}