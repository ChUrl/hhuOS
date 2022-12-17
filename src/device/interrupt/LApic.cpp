#include "LApic.h"
#include "kernel/system/System.h"
#include "kernel/paging/Paging.h"
#include "device/cpu/Cpu.h"

namespace Device {

bool LApic::initialized = false;
Device::ModelSpecificRegister LApic::ia32ApicBaseMsr = Device::ModelSpecificRegister(0x1B);
Kernel::Logger LApic::log = Kernel::Logger::get("LApic");
LApic::Register LApic::lintRegs[7] = {static_cast<Register>(0x2F0),
                                      static_cast<Register>(0x320),
                                      static_cast<Register>(0x330),
                                      static_cast<Register>(0x340),
                                      static_cast<Register>(0x350),
                                      static_cast<Register>(0x360),
                                      static_cast<Register>(0x370)};

// ! Public member functions start here

bool LApic::isInitialized() {
    return initialized;
}

void LApic::ensureInitialized() {
    if (!initialized) {
        Util::Exception::throwException(Util::Exception::ILLEGAL_STATE, "Local APICs are not initialized!");
    }
}

void LApic::initialize() {
    // InterruptModel::ensureApicSupported();
    if (!readBaseMSR().isBSP) {
        // NOTE: IA32_APIC_BASE_MSR is unique (every core has its own)
        Util::Exception::throwException(Util::Exception::UNSUPPORTED_OPERATION, "May only be called by BSP!");
    }

    InterruptModel::localPlatform->isX2Apic = readBaseMSR().isX2Apic;

    // TODO: Does every AP has to call this before initializing its local APIC?
    //       - Every AP writes/reads the same physical address, but different registers are affected
    //       - How does paging work in MP? Do I have to mapIO the same physical address multiple times?
    //         - Probably not if there is only a single kernel address space
    //       - Every AP has its own MMU, but are page tables shared or individual?
    //         - It would make sense to keep the kernel addresses only once
    //         - I guess the TLB at least has to be consistent over APs regarding kernel address space?
    initializeMMIORegion();

    // Initialize the local APIC of the BSP before initializing any APs
    initializeController(InterruptModel::getLApicInformation(getId()));

    InterruptModel::localPlatform->version = readDoubleWord(VER);

    // TODO: Should probably not do this automatically inside LApic::initialize()...
    for (auto *lapic: InterruptModel::lapics()) {
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
    writeDoubleWord(EOI, 0);
}

// TODO: Make sure this is called by the correct processor?
//       - Should happen anyway because the CPU that received the ERROR interrupt also runs the handler
void LApic::handleErrors() {
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

void LApic::ensureMMIO() {
    if (InterruptModel::localPlatform->virtAddress == 0) {
        Util::Exception::throwException(Util::Exception::ILLEGAL_STATE, "LApic MMIO region not initialized!");
    }
}

uint8_t LApic::getId() {
    return readDoubleWord(ID) >> 24;
}

void LApic::initializeMMIORegion() {
    // TODO: mapIO strong uncacheable?
    // Allocate memory (4 kb) for the memory mapped registers (without relocation)
    // The address returned is page aligned, if the size crosses page boundary an additional page will be allocated.
    auto &memoryService = Kernel::System::getService<Kernel::MemoryService>();
    void *virtAddress = memoryService.mapIO(InterruptModel::localPlatform->address, Util::Memory::PAGESIZE, true);

    // Use this address to access the local APIC's memory mapped registers
    InterruptModel::localPlatform->virtAddress =
            reinterpret_cast<uint32_t>(virtAddress) + InterruptModel::localPlatform->address % Util::Memory::PAGESIZE;
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
    if (InterruptModel::localPlatform->x2ApicSupported && InterruptModel::localPlatform->isX2Apic) {
        MSREntry msrEntry = readBaseMSR();
        msrEntry.isX2Apic = false; // Operate in xApic compatibility mode // TODO: Test this
        writeBaseMSR(msrEntry);
        InterruptModel::localPlatform->isX2Apic = false;
    }

    initializeLVT();

    // Configure the NMI (non maskable interrupt) pin
    LNMIConfiguration *lnmi = InterruptModel::getLNMIConfiguration(lapic);
    if (lnmi != nullptr) {
        LVTEntry lvtEntry{};
        lvtEntry.vector = static_cast<InterruptVector>(0); // NMI doesn't have vector
        lvtEntry.deliveryMode = LVTEntry::DeliveryMode::NMI;
        lvtEntry.pinPolarity = lnmi->polarity;
        lvtEntry.triggerMode = lnmi->triggerMode;
        lvtEntry.isMasked = false;
        writeLVT(lnmi->lint == 0 ? LINT0 : LINT1, lvtEntry);
    }

    // SW Enable APIC by setting the Spurious Interrupt Vector Register with spurious vector number 0xFF (OSDev)
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

void LApic::initializeLVT() {
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

uint32_t LApic::readDoubleWord(Register reg) {
    ensureMMIO();
    return *reinterpret_cast<volatile uint32_t *>(InterruptModel::localPlatform->virtAddress + reg);
}

void LApic::writeDoubleWord(Register reg, uint32_t val) {
    ensureMMIO();
    *reinterpret_cast<volatile uint32_t *>(InterruptModel::localPlatform->virtAddress + reg) = val;
}

// IA-32 Architecture Manual Chapter 10.4.4
MSREntry LApic::readBaseMSR() {
    return MSREntry(ia32ApicBaseMsr.readQuadWord());
}

// IA-32 Architecture Manual Chapter 10.4.4
void LApic::writeBaseMSR(const MSREntry &msrEntry) {
    ia32ApicBaseMsr.writeQuadWord(static_cast<uint64_t>(msrEntry)); // Atomic write
}

// IA-32 Architecture Manual Chapter 10.9
SVREntry LApic::readSVR() {
    return SVREntry(readDoubleWord(SVR));
}

// IA-32 Architecture Manual Chapter 10.9
void LApic::writeSVR(const SVREntry &svrEntry) {
    writeDoubleWord(SVR, static_cast<uint32_t>(svrEntry));
}

// IA-32 Architecture Manual Chapter 10.5.1
LVTEntry LApic::readLVT(Lint lint) {
    return LVTEntry(readDoubleWord(lintRegs[lint]));
}

// TODO: Check if it is a problem to write to readonly/reserved areas
// IA-32 Architecture Manual Chapter 10.5.1
void LApic::writeLVT(Lint lint, const LVTEntry &lvtEntry) {
    writeDoubleWord(lintRegs[lint], static_cast<uint32_t>(lvtEntry));
}

// NOTE: In x2APIC mode this could be read atomically (rdmsr)
ICREntry LApic::readICR() {
    // Cpu::disableInterrupts(); // Do not let another interrupt handler fuck this up
    uint32_t low = readDoubleWord(ICR_LOW);
    uint64_t high = readDoubleWord(ICR_HIGH);
    // Cpu::enableInterrupts();
    return ICREntry(low | high << 32);
}

// NOTE: In x2APIC mode this could be written atomically (wrmsr)
void LApic::writeICR(const ICREntry &icrEntry) {
    auto val = static_cast<uint64_t>(icrEntry);
    // Cpu::disableInterrupts(); // Do not let another interrupt handler fuck this up
    writeDoubleWord(ICR_HIGH, val >> 32);
    writeDoubleWord(ICR_LOW, val & 0xFFFFFFFF); // Writing the low DW sends the IPI
    // Cpu::enableInterrupts();
}

}