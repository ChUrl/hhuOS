#include "Apic.h"
#include "device/cpu/Cpu.h"
#include "device/time/Cmos.h"
#include "device/time/Pit.h"
#include "kernel/paging/Paging.h"
#include "kernel/service/FilesystemService.h"
#include "kernel/service/InterruptService.h"
#include "kernel/system/System.h"
#include "kernel/system/TaskStateSegment.h"
#include "lib/util/base/Constants.h"

namespace Device {

bool Apic::smpEnabled = false;

bool Apic::isSmpSupported() {
    ensureApic();
    return usableProcessors > 1;
}

void Device::Apic::startupSmp() {
    ensureApic();
    if (smpEnabled) {
        Util::Exception::throwException(Util::Exception::ILLEGAL_STATE, "Already initialized!");
    }
    if (localApics->length() > 64) {
        // This limit is pretty arbitrary, but the runningAPs bitmap currently only has 64 bits (in Smp.h).
        // Technically xApic supports 8-bit CPU ids though, x2Apic even more (32-bit CPU ids).
        Util::Exception::throwException(Util::Exception::UNSUPPORTED_OPERATION, "CPUs with more than 64 cores are not supported!");
    }

    void *apGdts = prepareApGdts();
    void *apStacks = prepareApStacks();
    void *apStartupCode = prepareApStartupCode(apGdts, apStacks);
    void *apWarmReset = prepareApWarmReset(); // This is technically only required for discrete APIC, see below

    // Universal Startup Algorithm requires all interrupts disabled (they should be disabled anyway,
    // but disabling them a second time is twice as good)
    Cpu::disableInterrupts();
    Cmos::disableNmi();

    // Call the startup code on each AP using the SIPI
    for (uint32_t i = 0; i < localApics->length(); ++i) {
        const LocalApic *localApic = (*localApics)[i];
        if (localApic == nullptr || localApic->cpuId == LocalApic::getId()) {
            // Skip this AP if it's the BSP or disabled
            continue;
        }

        // Info on discrete APIC:
        // The INIT IPI is required for CPUs with a discrete APIC, these ignore the STARTUP IPI.
        // For these CPUs, the startup routines address has to be written to the BIOS memory segment
        // (warm reset vector), and the AP has to be configured for warm-reset to start executing there.
        // This is unused for xApic. The INIT IPI is still issued though, to follow the IA-32 manual's
        // "INIT-SIPI-SIPI" sequence and the "universal startup algorithm" (MPSpec, sec. B.4):
        LocalApic::clearErrors();
        LocalApic::sendInitIpi(localApic->cpuId, ICREntry::Level::ASSERT);   // Level-triggered, needs to be...
        LocalApic::waitForIpiDispatch();                                     // xv6 waits 200 us instead.
        LocalApic::sendInitIpi(localApic->cpuId, ICREntry::Level::DEASSERT); // ...deasserted manually
        LocalApic::waitForIpiDispatch();                                     // Not necessary with 10ms delay
        Pit::earlyDelay(10'000);                                             // 10 ms, xv6 waits 100 us instead.

        // Issue the SIPI twice (for xApic):
        for (uint8_t j = 0; j < 2; ++j) {
            LocalApic::clearErrors();
            LocalApic::sendStartupIpi(localApic->cpuId, apStartupAddress);
            LocalApic::waitForIpiDispatch();
            Pit::earlyDelay(200); // 200 us
        }

        // Wait until the AP marks it is running, so we can continue to the next one.
        // Because we initialize the APs one at a time, runningAPs is not synchronized.
        // If the AP initialization fails (and the system doesn't crash), this will lock the BSP,
        // the same will happen if the SIPI does not reach its target. That's why we abort.
        // Because the systemtime is not yet functional, we delay to measure the ~ time.
        uint32_t readCount = 0;
        while (!(runningAPs & (1 << localApic->cpuId))) {
            if (readCount > 10) {
                // Waited 10 * 10 ms = 0.1 s in total (pretty arbitrarily chosen by me)
                log.error("CPU [%d] didn't phone home, it could be in undefined state!", i);
                break;
            }
            Pit::earlyDelay(10'000); // 10 ms
            readCount++;
        }
    }

    Cmos::enableNmi();
    Cpu::enableInterrupts();

    // TODO: We're deleting void * here, but I think this is safe, because no destructors or similar C++ stuff
    //       is involved? Otherwise, cast to uint32_t * or some other primitive pointer type?
    // Free the startup routine page, stackpointer array, gdts array and warm-reset vector memory,
    // now that all APs are running. Keep the stacks though, they are not temporary!
    delete apGdts;
    delete apStacks;
    delete apStartupCode;
    delete apWarmReset;

    smpEnabled = true;
}

void *Apic::prepareApStacks() {
    auto &memoryService = Kernel::System::getService<Kernel::MemoryService>();

    // Allocate the stackpointer array
    auto **stacks = reinterpret_cast<uint32_t **>(new uint8_t *[localApics->length()]);
    if (stacks == nullptr) {
        Util::Exception::throwException(Util::Exception::NULL_POINTER, "Failed to allocate AP stack memory!");
    }

    // Allocate the stacks, just iterates from 0 to cpuCount - 1 because ids are contiguous (we assume)
    for (uint32_t i = 0; i < localApics->length(); ++i) {
        if (i == LocalApic::getId() || (*localApics)[i] == nullptr) {
            // Skip BSP or disabled processors
            stacks[i] = nullptr;
            continue;
        }

        stacks[i] = reinterpret_cast<uint32_t *>(new uint8_t[apStackSize]);
        if (stacks[i] == nullptr) {
            Util::Exception::throwException(Util::Exception::NULL_POINTER, "Failed to allocate AP stack memory!");
        }
    }

    // Return the address for later cleanup
    return reinterpret_cast<void *>(stacks);
}

void *Apic::prepareApStartupCode(void *apGdts, void *apStacks) {
    if (boot_ap_size > Util::PAGESIZE) {
        Util::Exception::throwException(Util::Exception::ILLEGAL_STATE, "Startup code does not fit into one page!");
    }

    // Allocate physical memory for copying the startup routine
    auto &memoryService = Kernel::System::getService<Kernel::MemoryService>();
    void *startupCodeMemory = memoryService.mapIO(apStartupAddress, Util::PAGESIZE);

    // Identity map the allocated physical memory to the kernel address space
    // (Seems to be required to switch to protected mode with enabled paging)
    memoryService.unmap(reinterpret_cast<uint32_t>(startupCodeMemory));
    memoryService.mapPhysicalAddress(apStartupAddress, apStartupAddress,
                                     Kernel::Paging::PRESENT | Kernel::Paging::READ_WRITE);

    // Sanity check because am dumb
    if (reinterpret_cast<uint32_t>(memoryService.getPhysicalAddress(reinterpret_cast<void *>(apStartupAddress))) != apStartupAddress) {
        Util::Exception::throwException(Util::Exception::ILLEGAL_STATE, "Failed to identity map startup code memory!");
    }

    // Virtual addresses
    const Util::Address<uint32_t> startupCode = Util::Address<uint32_t>(reinterpret_cast<uint32_t>(&boot_ap));
    const Util::Address<uint32_t> destination = Util::Address<uint32_t>(apStartupAddress);

    // Prepare the empty variables in the startup routine at their original location
    asm volatile("sgdt %0"
                 : "=m"(boot_ap_gdtr));
    asm volatile("sidt %0"
                 : "=m"(boot_ap_idtr));
    asm volatile("mov %%cr0, %%eax;"
                 : "=a"(boot_ap_cr0));
    asm volatile("mov %%cr3, %%eax;"
                 : "=a"(boot_ap_cr3));
    asm volatile("mov %%cr4, %%eax;"
                 : "=a"(boot_ap_cr4));
    boot_ap_gdts = reinterpret_cast<uint32_t>(apGdts);
    boot_ap_stacks = reinterpret_cast<uint32_t>(apStacks);
    boot_ap_entry = reinterpret_cast<uint32_t>(&smpEntry);

    // Copy the startup routine and prepared variables to the identity mapped page
    // log.info("Copying AP startup routine from [0x%x] (virt) to [0x%x] (phys)", reinterpret_cast<uint32_t>(&boot_ap), apStartupAddress);
    destination.copyRange(startupCode, boot_ap_size);

    // Return the address for later cleanup
    return reinterpret_cast<void *>(apStartupAddress);
}

// NOTE: Booting APs using this method was never tested, as QEMU only has xApic or x2Apic which uses the SIPI
void *Apic::prepareApWarmReset() {
    Cmos::write(0xF, 0x0A); // Shutdown status byte (MPSpec, sec. B.4)

    auto &memoryService = Kernel::System::getService<Kernel::MemoryService>();
    const uint32_t wrvPhys = 0x40 << 4 | 0x67;                     // MPSpec, sec. B.4
    void *warmResetVector = memoryService.mapIO(wrvPhys, 2, true); // WRV is DWORD

    // Account for possible misalignment, as mapIO returns a page-aligned pointer
    const uint32_t pageOffset = wrvPhys % Util::PAGESIZE;
    const uint32_t wrvVirt = reinterpret_cast<uint32_t>(warmResetVector) + pageOffset;

    *reinterpret_cast<volatile uint16_t *>(wrvVirt) = apStartupAddress;

    // Return the address for later cleanup
    return reinterpret_cast<void *>(wrvVirt);
}

void *Apic::prepareApGdts() {
    auto &memoryService = Kernel::System::getService<Kernel::MemoryService>();

    // Allocate descriptor pointer array
    auto **gdts = reinterpret_cast<Descriptor **>(new Descriptor *[localApics->length()]);
    if (gdts == nullptr) {
        Util::Exception::throwException(Util::Exception::NULL_POINTER, "Failed to allocate AP GDTs memory!");
    }

    for (uint32_t i = 0; i < localApics->length(); ++i) {
        if (i == LocalApic::getId() || (*localApics)[i] == nullptr) {
            // Skip BSP or disabled processors
            gdts[i] = nullptr;
            continue;
        }

        gdts[i] = allocateApGdt();
    }

    // Return the address for later cleanup
    return reinterpret_cast<void *>(gdts);
}

Descriptor *Apic::allocateApGdt() {
    // Allocate memory for the GDT and TSS. This is never freed, as its used as long as the system runs.
    auto &memoryService = Kernel::System::getService<Kernel::MemoryService>();

    auto *gdt = reinterpret_cast<uint16_t *>(memoryService.allocateLowerMemory(48));
    if (gdt == nullptr) {
        Util::Exception::throwException(Util::Exception::NULL_POINTER, "Failed to allocate AP GDT memory!");
    }

    const uint32_t tssSize = sizeof(Kernel::TaskStateSegment);
    auto *tss = reinterpret_cast<void *>(memoryService.allocateLowerMemory(tssSize));
    if (tss == nullptr) {
        Util::Exception::throwException(Util::Exception::NULL_POINTER, "Failed to allocate AP TSS memory!");
    }

    // Zero everything
    Util::Address<uint32_t>(gdt).setRange(0, 48);
    Util::Address<uint32_t>(tss).setRange(0, tssSize);

    // Set up general GDT for the AP
    // First entry has to be null
    Kernel::System::createGlobalDescriptorTableEntry(gdt, 0, 0, 0, 0, 0);
    // Kernel code segment
    Kernel::System::createGlobalDescriptorTableEntry(gdt, 1, 0, 0xFFFFFFFF, 0x9A, 0xC);
    // Kernel data segment
    Kernel::System::createGlobalDescriptorTableEntry(gdt, 2, 0, 0xFFFFFFFF, 0x92, 0xC);
    // User code segment
    Kernel::System::createGlobalDescriptorTableEntry(gdt, 3, 0, 0xFFFFFFFF, 0xFA, 0xC);
    // User data segment
    Kernel::System::createGlobalDescriptorTableEntry(gdt, 4, 0, 0xFFFFFFFF, 0xF2, 0xC);
    // TSS segment
    Kernel::System::createGlobalDescriptorTableEntry(gdt, 5, reinterpret_cast<uint32_t>(tss), tssSize, 0x89, 0x4);

    return new Descriptor{
      .size = 6 * 8,                             // TODO: Why 6 * 8? Isn't it 24 * 4 (boot.asm gdt: "times (24) dw 0")?
      .address = reinterpret_cast<uint64_t>(gdt) // + Kernel::MemoryLayout::KERNEL_START
    };
}

} // namespace Device