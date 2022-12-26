#include "LocalApic.h"
#include "kernel/system/System.h"
#include "kernel/paging/Paging.h"
#include "device/cpu/Cpu.h"

namespace Device {

bool LocalApic::initialized = false;
Device::ModelSpecificRegister LocalApic::ia32ApicBaseMsr = Device::ModelSpecificRegister(0x1B);
Kernel::Logger LocalApic::log = Kernel::Logger::get("LApic");
LocalApic::Register LocalApic::lintRegs[7] = {static_cast<Register>(0x2F0),
                                              static_cast<Register>(0x320),
                                              static_cast<Register>(0x330),
                                              static_cast<Register>(0x340),
                                              static_cast<Register>(0x350),
                                              static_cast<Register>(0x360),
                                              static_cast<Register>(0x370)};

// ! Public member functions start here

// TODO: Does not account for multiple cores
bool LocalApic::isInitialized() {
    return initialized;
}

// TODO: Does not account for multiple cores
void LocalApic::ensureInitialized() {
    if (!initialized) {
        Util::Exception::throwException(Util::Exception::ILLEGAL_STATE, "Local APICs are not initialized!");
    }
}

void LocalApic::initialize() {
    Apic::ensureApicSupported();
    if (!readBaseMSR().isBSP) {
        // IA32_APIC_BASE_MSR is unique (every core has its own)
        Util::Exception::throwException(Util::Exception::UNSUPPORTED_OPERATION, "May only be called by BSP!");
    }

    // TODO: Does every AP has to call this before initializing its local APIC?
    //       - Every AP writes/reads the same physical address, but different registers are affected
    //       - How does paging work in MP? Do I have to mapIO the same physical address multiple times?
    //         - Probably not if there is only a single kernel address space
    //       - Every AP has its own MMU, but are page tables shared or individual?
    //         - It would make sense to keep the kernel addresses only once
    initializeMMIORegion();

    Apic::localPlatform->version = readDoubleWord(VER);

    // Initialize the local APIC of the BSP before initializing any APs
    // TODO: The BSP has to be set to use xApic mode or getId() has to support x2Apic mode
    initializeController(Apic::getLApicInformation(getId()));

    // TODO: Should probably not do this automatically inside LApic::initialize()...
    // for (auto *lapic: InterruptModel::lapics()) {
    //     // TODO: !lapic->enabled == true could also mean that the cpu is just not initialized yet...
    //     if (!lapic->enabled || lapic->id == getId()) { // Skip BSP and unavailable processors
    //         continue;
    //     }

    //     initializeApplicationProcessor(lapic);
    // }

    // TODO: Mask all the PIC interrupts in the PIC aswell (they should still be all masked though...)
    // TODO: Make some PIC functions (allow, forbid, status) static so I can just call them?

    initialized = true;
}

// TODO: Does not account for multiple cores
void LocalApic::allow(Lint lint) {
    LVTEntry entry = readLVT(lint);
    entry.isMasked = false;
    writeLVT(lint, entry);
}

// TODO: Does not account for multiple cores
void LocalApic::forbid(Lint lint) {
    LVTEntry entry = readLVT(lint);
    entry.isMasked = true;
    writeLVT(lint, entry);
}

// TODO: Does not account for multiple cores
bool LocalApic::status(Lint lint) {
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

// ! Private member functions start here

void LocalApic::ensureMMIO() {
    if (Apic::localPlatform->virtAddress == 0) {
        Util::Exception::throwException(Util::Exception::ILLEGAL_STATE, "LApic MMIO region not initialized!");
    }
}

uint8_t LocalApic::getId() {
    return readDoubleWord(ID) >> 24;
}

void LocalApic::initializeApplicationProcessor(LApicInformation *lapic) {
    // TODO: Prepare stack for entrycode
    // TODO: Send INIT and STARTUP interrupts with entry code address
    // TODO: The entrycode needs to call initializeController to initialize its own local APIC registers

    lapic->enabled = true;
}

// TODO: Does this have to be synchronized when initializing other APs?
//       - Probably not as all of them work in different address spaces?
//       - Also only one is initialized at a time (and MP init sequence requires acquiring BIOS semaphore...)
// TODO: IA-32 Architecture Manual Chapter 8.4.3.5: APIC ID has to be signalled to ACPI?
void LocalApic::initializeController(LApicInformation *lapic) {
    // x2Apic doesn't have MMIO register access (x2Apic uses MSRs)
    // Currently only supports xApic mode
    lapic->isX2Apic = readBaseMSR().isX2Apic;
    if (Apic::localPlatform->x2ApicSupported && lapic->isX2Apic) {
        MSREntry msrEntry = readBaseMSR();
        msrEntry.isX2Apic = false; // Operate in xApic compatibility mode // TODO: Test this how?
        writeBaseMSR(msrEntry);
        lapic->isX2Apic = false;
    }

    // Mask all local interrupt sources
    initializeLVT();

    // Configure the NMI (non maskable interrupt) pin
    LNMIConfiguration *lnmi = Apic::getLNMIConfiguration(lapic);
    if (lnmi != nullptr) {
        LVTEntry lvtEntry{};
        lvtEntry.vector = static_cast<InterruptVector>(0); // NMI doesn't have vector
        lvtEntry.deliveryMode = LVTEntry::DeliveryMode::NMI;
        lvtEntry.pinPolarity = lnmi->polarity;
        lvtEntry.triggerMode = lnmi->triggerMode;
        lvtEntry.isMasked = false;
        writeLVT(lnmi->lint == 0 ? LINT0 : LINT1, lvtEntry);
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
}

// TODO: Relocation?
// TODO: How to make sure the memory at the lapic address will not be allocated by something else?
//       Right now the memory is not allocated through the memory manager, it is just mapped to virtual space...
void LocalApic::initializeMMIORegion() {
    uint32_t physAddress = Apic::localPlatform->address;
    uint32_t pageOffset = physAddress % Util::Memory::PAGESIZE;

    auto &memoryService = Kernel::System::getService<Kernel::MemoryService>();
    void *virtAddress = memoryService.mapIO(physAddress, Util::Memory::PAGESIZE, true);

    // Account for possible misalignment
    Apic::localPlatform->virtAddress = reinterpret_cast<uint32_t>(virtAddress) + pageOffset;
}

void LocalApic::initializeLVT() {
    ensureMMIO();

    // Default values
    LVTEntry lvtEntry{};
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

// IA-32 Architecture Manual Chapter 10.4.4
MSREntry LocalApic::readBaseMSR() {
    return static_cast<MSREntry>(ia32ApicBaseMsr.readQuadWord());
}

// IA-32 Architecture Manual Chapter 10.4.4
void LocalApic::writeBaseMSR(const MSREntry &msrEntry) {
    ia32ApicBaseMsr.writeQuadWord(static_cast<uint64_t>(msrEntry)); // Atomic write
}

uint32_t LocalApic::readDoubleWord(Register reg) {
    ensureMMIO();
    return *reinterpret_cast<volatile uint32_t *>(Apic::localPlatform->virtAddress + reg);
}

void LocalApic::writeDoubleWord(Register reg, uint32_t val) {
    ensureMMIO();
    *reinterpret_cast<volatile uint32_t *>(Apic::localPlatform->virtAddress + reg) = val;
}

// IA-32 Architecture Manual Chapter 10.9
SVREntry LocalApic::readSVR() {
    return static_cast<SVREntry>(readDoubleWord(SVR));
}

// IA-32 Architecture Manual Chapter 10.9
void LocalApic::writeSVR(const SVREntry &svrEntry) {
    writeDoubleWord(SVR, static_cast<uint32_t>(svrEntry));
}

// IA-32 Architecture Manual Chapter 10.5.1
LVTEntry LocalApic::readLVT(Lint lint) {
    return static_cast<LVTEntry>(readDoubleWord(lintRegs[lint]));
}

// TODO: Check if it is a problem to write to readonly/reserved areas
// IA-32 Architecture Manual Chapter 10.5.1
void LocalApic::writeLVT(Lint lint, const LVTEntry &lvtEntry) {
    writeDoubleWord(lintRegs[lint], static_cast<uint32_t>(lvtEntry));
}

// NOTE: In x2APIC mode this could be read atomically (rdmsr)
ICREntry LocalApic::readICR() {
    // Cpu::disableInterrupts(); // Do not let another interrupt handler fuck this up
    uint32_t low = readDoubleWord(ICR_LOW);
    uint64_t high = readDoubleWord(ICR_HIGH);
    // Cpu::enableInterrupts();
    return static_cast<ICREntry>(low | high << 32);
}

// NOTE: In x2APIC mode this could be written atomically (wrmsr)
void LocalApic::writeICR(const ICREntry &icrEntry) {
    auto val = static_cast<uint64_t>(icrEntry);
    // Cpu::disableInterrupts(); // Do not let another interrupt handler fuck this up
    writeDoubleWord(ICR_HIGH, val >> 32);
    writeDoubleWord(ICR_LOW, val & 0xFFFFFFFF); // Writing the low DW sends the IPI
    // Cpu::enableInterrupts();
}

}