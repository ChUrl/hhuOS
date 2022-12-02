#include "LApic.h"
#include "lib/util/cpu/CpuId.h"
#include "kernel/system/System.h"
#include "kernel/paging/Paging.h"

namespace Device {

Kernel::Logger LApic::log = Kernel::Logger::get("LApic");

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
bool LApic::isX2Apic() const {
    uint64_t val = IA32_APIC_BASE_MSR.readQuadWord();
    return val & APIC_MSR_X2APIC_ENABLE_FLAG;
}

// IA-32 Architecture Manual Chapter 10.4.6
uint8_t LApic::getId() const {
    return readDoubleWord(Device::LApic::APIC_ID) >> 24;
}

// IA-32 Architecture Manual Chapter 10.4.8
uint8_t LApic::getVersion() const {
    return readDoubleWord(Device::LApic::APIC_VER) & 0xFF;
}

void LApic::enable() {
    uint64_t val = IA32_APIC_BASE_MSR.readQuadWord();
    IA32_APIC_BASE_MSR.writeQuadWord(val | APIC_MSR_ENABLE_FLAG | APIC_MSR_BSP_FLAG);
}

void LApic::enable(void* base_address) {
    uint64_t val = (reinterpret_cast<uint64_t>(base_address) & APIC_MSR_BASE_FIELD_MASK)
            | APIC_MSR_ENABLE_FLAG
            | APIC_MSR_BSP_FLAG;
    IA32_APIC_BASE_MSR.writeQuadWord(val);
}

bool LApic::isEnabled() const {
    uint64_t val = IA32_APIC_BASE_MSR.readQuadWord();
    return val & APIC_MSR_ENABLE_FLAG;
}

void* LApic::getBaseAddr() const {
    uint64_t val = IA32_APIC_BASE_MSR.readQuadWord();
    return reinterpret_cast<void*>(val & APIC_MSR_BASE_FIELD_MASK);
}

void LApic::disable() {
    uint64_t val = IA32_APIC_BASE_MSR.readQuadWord();
    IA32_APIC_BASE_MSR.writeQuadWord(val & ~APIC_MSR_ENABLE_FLAG);
}

void LApic::init() {
    auto& memoryService = Kernel::System::getService<Kernel::MemoryService>();
    auto& pageDirectory = memoryService.getKernelAddressSpace().getPageDirectory();

    // Relocation
    // NOTE: Register reads yield wrong values
    // void* virtAddress = memoryService.allocateKernelMemory(4096, Kernel::Paging::PAGESIZE);
    // NOTE: All register reads yield only 1s
    // void* virtAddress = memoryService.mapIO(4096, true);

    // MP
    // NOTE: Can't find the MP Structures with 1 CPU core in QEMU. It doesn't work even with -smp 2. MP stuff is bugged.
    // auto* mp_ct = Util::Cpu::MultiProcessor::searchMPConfigurationTable();
    // if (mp_ct == nullptr) {
    //     Util::Exception::throwException(Util::Exception::NULL_POINTER,
    //                                     "LApic::writeDoubleWord(): Didn't find valid MP Configuration Table!");
    // }
    // uint32_t lapicBaseAddress = mp_ct->lapic_address;
    // void* virtAddress = memoryService.mapIO(lapicBaseAddress, 4096, true);

    // No relocation
    void* virtAddress = memoryService.mapIO(APIC_BASE_DEFAULT_PHYS_ADDRESS, 4096, true);

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
    APIC_BASE_VIRT_ADDRESS = virtAddress;

    // HW Enable APIC with relocation/MP
    // void* physAddress = memoryService.getPhysicalAddress(virtAddress);
    // enable(physAddress);

    // HW Enable APIC without relocation
    enable();

    // SW Enable APIC by setting the Spurious Interrupt Vector Register with spurious vector number 0xFF (OSDev)
    uint32_t svr_val = readDoubleWord(APIC_SVR);
    writeDoubleWord(APIC_SVR, svr_val | APIC_SVR_SPURIOUS_VECTOR_MASK | APIC_SVR_SW_ENABLE_FLAG);

    // Allow all interrupts to be forwarded to the CPU by setting the Task-Priority Class and Sub Class thresholds to 0
    // (IA-32 Architecture Manual Chapter 10.8.3.1)
    writeDoubleWord(APIC_TPR, 0);

    // Mask all the interrupts I currently don't care about (all except LINT0 for virtual wire mode and LINT1)
    writeDoubleWord(APIC_LVT_CMCI, readDoubleWord(APIC_LVT_CMCI) | APIC_LVT_MASKED_FLAG);
    writeDoubleWord(APIC_LVT_TIMER, readDoubleWord(APIC_LVT_TIMER) | APIC_LVT_MASKED_FLAG);
    writeDoubleWord(APIC_LVT_THERMAL, readDoubleWord(APIC_LVT_THERMAL) | APIC_LVT_MASKED_FLAG);
    writeDoubleWord(APIC_LVT_PERFORMANCE, readDoubleWord(APIC_LVT_PERFORMANCE) | APIC_LVT_MASKED_FLAG);
    writeDoubleWord(APIC_LVT_ERROR, readDoubleWord(APIC_LVT_ERROR) | APIC_LVT_MASKED_FLAG);

    // NOTE: Either QEMU does this by default with +apic or the IMCR doesn't exist? The IMCR always reads 0xFF...
    // Connect APIC to BSP and set LINT0 to ExtINT
    enableVirtualWire();

    // NOTE: If LINT0 is masked OS should be stuck when booting as no interrupts should arrive through virtual wire
    // writeDoubleWord(APIC_LVT_LINT0, readDoubleWord(APIC_LVT_LINT0) | APIC_LVT_MASKED_FLAG);

    log.info("Local APIC has been initialized");
#if HHUOS_LAPIC_ENABLE_DEBUG == 1
    logDebugDump();
#endif
}

uint32_t LApic::readDoubleWord(uint16_t reg) const {
    if (APIC_BASE_VIRT_ADDRESS == nullptr) {
        Util::Exception::throwException(Util::Exception::NULL_POINTER,
                                        "LApic::readDoubleWord(): APIC_BASE_ADDRESS not initialized!");
    }

    auto* regAddr = reinterpret_cast<uint32_t*>(reinterpret_cast<uint32_t>(APIC_BASE_VIRT_ADDRESS) + reg);
    return *regAddr;
}

void LApic::writeDoubleWord(uint16_t reg, uint32_t val) {
    if (APIC_BASE_VIRT_ADDRESS == nullptr) {
        Util::Exception::throwException(Util::Exception::NULL_POINTER,
                                        "LApic::writeDoubleWord(): APIC_BASE_ADDRESS not initialized!");
    }

    auto* regAddr = reinterpret_cast<uint32_t*>(reinterpret_cast<uint32_t>(APIC_BASE_VIRT_ADDRESS) + reg);
    *regAddr = val;
}

void LApic::enableVirtualWire() {
    registerSelectorPort.writeByte(0x70); // IMCR address is 0x70
    registerDataPort.writeByte(0x01); // 0x00 connects PIC to BSP, 0x01 connects APIC to BSP

    // Set LINT0 to ExtINT for external IC (PIC)
    uint32_t lint0_val = readDoubleWord(APIC_LVT_LINT0);
    writeDoubleWord(APIC_LVT_LINT0, lint0_val | static_cast<uint32_t>(APIC_LVT_DELIVERY_MODE_EXTINT) << 8);
}

#if HHUOS_LAPIC_ENABLE_DEBUG == 1
void LApic::logDebugDump() {
    uint8_t id = getId();
    uint8_t version = getVersion();

    log.debug("APIC Enabled: %u", isEnabled());
    log.debug("Has Apic Support: %u", Device::LApic::hasApicSupport());
    log.debug("Has x2Apic Support: %u", Device::LApic::hasX2ApicSupport());
    log.debug("Is x2Apic: %u", isX2Apic());
    log.debug("Base Phys Address: 0x%x", reinterpret_cast<uint32_t>(getBaseAddr()));
    log.debug("Base Virt Address: 0x%x", reinterpret_cast<uint32_t>(APIC_BASE_VIRT_ADDRESS));
    log.debug("APIC ID: %u", id);
    log.debug("APIC VER: 0x%x (Integrated APIC: %u)", version, 0x10 <= version && version <= 0x15);
    log.debug("Spurious interrupt vector: 0x%x", readDoubleWord(APIC_SVR) & APIC_SVR_SPURIOUS_VECTOR_MASK);
}
#endif

}