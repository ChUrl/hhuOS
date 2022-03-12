/*
 * Copyright (C) 2018-2022 Heinrich-Heine-Universitaet Duesseldorf,
 * Institute of Computer Science, Department Operating Systems
 * Burak Akguel, Christian Gesse, Fabian Ruhland, Filip Krakowski, Michael Schoettner
 *
 * This program is free software: you can redistribute it and/or modify it under the terms of the GNU General Public
 * License as published by the Free Software Foundation, either version 3 of the License, or (at your option) any
 * later version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied
 * warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for more
 * details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>
 */

#include "kernel/paging/Paging.h"
#include "kernel/system/System.h"
#include "kernel/interrupt/InterruptDispatcher.h"
#include "kernel/paging/PageDirectory.h"
#include "kernel/paging/MemoryLayout.h"
#include "asm_interface.h"
#include "MemoryService.h"

namespace Kernel {

MemoryService::MemoryService(PageFrameAllocator *pageFrameAllocator, PagingAreaManager *pagingAreaManager, VirtualAddressSpace *kernelAddressSpace)
        : pageFrameAllocator(*pageFrameAllocator), pagingAreaManager(*pagingAreaManager), currentAddressSpace(kernelAddressSpace), kernelAddressSpace(*kernelAddressSpace) {
    addressSpaces.add(kernelAddressSpace);
    lowerMemoryManager.initialize(MemoryLayout::BIOS_CODE_MEMORY.toVirtual().endAddress + 1, MemoryLayout::USABLE_LOWER_MEMORY.toVirtual().endAddress);

    SystemCall::registerSystemCall(Util::System::UNMAP, [](uint32_t paramCount, va_list arguments) -> Util::System::Result {
        if (paramCount < 2) {
            return Util::System::INVALID_ARGUMENT;
        }

        uint32_t virtualStartAddress = va_arg(arguments, uint32_t);
        uint32_t virtualEndAddress = va_arg(arguments, uint32_t);

        if (virtualStartAddress > MemoryLayout::KERNEL_START || virtualEndAddress > MemoryLayout::KERNEL_START) {
            return Util::System::OUT_OF_BOUNDS;
        }

        auto status = Kernel::System::getService<Kernel::MemoryService>().unmap(virtualStartAddress, virtualEndAddress);
        if (status == 0) {
            return Util::System::INVALID_ARGUMENT;
        }

        return Util::System::Result::OK;
    });

    SystemCall::registerSystemCall(Util::System::MAP_IO, [](uint32_t paramCount, va_list arguments) -> Util::System::Result {
        if (paramCount < 3) {
            return Util::System::INVALID_ARGUMENT;
        }

        uint32_t physicalAddress = va_arg(arguments, uint32_t);
        uint32_t size = va_arg(arguments, uint32_t);
        void **mappedAddress = va_arg(arguments, void**);

        *mappedAddress = Kernel::System::getService<Kernel::MemoryService>().mapIO(physicalAddress, size);
        return Util::System::Result::OK;
    });
}

MemoryService::~MemoryService() {
    delete &pageFrameAllocator;
    delete &pagingAreaManager;

    for (const auto *addressSpace : addressSpaces) {
        delete addressSpace;
    }
}

void *MemoryService::allocateKernelMemory(uint32_t size, uint32_t alignment) {
    return kernelAddressSpace.getMemoryManager().allocateMemory(size, alignment);
}

void *MemoryService::reallocateKernelMemory(void *pointer, uint32_t size, uint32_t alignment) {
    return kernelAddressSpace.getMemoryManager().reallocateMemory(pointer, size, alignment);
}

void MemoryService::freeKernelMemory(void *pointer, uint32_t alignment) {
    kernelAddressSpace.getMemoryManager().freeMemory(pointer, alignment);
}

void *MemoryService::allocateUserMemory(uint32_t size, uint32_t alignment) {
    return currentAddressSpace->getMemoryManager().allocateMemory(size, alignment);
}

void *MemoryService::reallocateUserMemory(void *pointer, uint32_t size, uint32_t alignment) {
    return currentAddressSpace->getMemoryManager().reallocateMemory(pointer, size, alignment);
}

void MemoryService::freeUserMemory(void *pointer, uint32_t alignment) {
    currentAddressSpace->getMemoryManager().freeMemory(pointer, alignment);
}

void *MemoryService::allocateLowerMemory(uint32_t size, uint32_t alignment) {
    return lowerMemoryManager.allocateMemory(size, alignment);
}

void *MemoryService::reallocateLowerMemory(void *pointer, uint32_t size, uint32_t alignment) {
    return lowerMemoryManager.reallocateMemory(pointer, size, alignment);
}

void MemoryService::freeLowerMemory(void *pointer, uint32_t alignment) {
    lowerMemoryManager.freeMemory(pointer, alignment);
}

void *MemoryService::allocatePageTable() {
    return pagingAreaManager.allocateBlock();
}

void MemoryService::freePageTable(void *virtualTableAddress) {
    void *physicalAddress = getPhysicalAddress(virtualTableAddress);
    // Free virtual memory
    pagingAreaManager.freeBlock(virtualTableAddress);
    // Free physical memory
    pageFrameAllocator.freeBlock(physicalAddress);
}

void MemoryService::createPageTable(PageDirectory *directory, uint32_t index) {
    // Get some virtual memory for the table
    void *virtAddress = pagingAreaManager.allocateBlock();
    // Get physical memory for the table
    void *physAddress = getPhysicalAddress(virtAddress);
    // There must be no mapping from virtual to physical address done here,
    // because the page is zeroed out after allocation by the PagingAreaManager

    uint32_t startAddress = index * Kernel::Paging::PAGESIZE * 1024;
    // Initialize the table in the page directory
    directory->createTable(index, reinterpret_cast<uint32_t>(physAddress),reinterpret_cast<uint32_t>(virtAddress),
                           Paging::PRESENT | Paging::READ_WRITE | (startAddress < Kernel::MemoryLayout::KERNEL_START ? Paging::USER_ACCESS : 0));
}

void Kernel::MemoryService::mapPhysicalAddress(uint32_t virtualAddress, uint32_t physicalAddress, uint16_t flags) {
    // Mark the physical page frame as used
    physicalAddress = reinterpret_cast<uint32_t>(pageFrameAllocator.allocateBlockAtAddress(reinterpret_cast<void *>(physicalAddress)));
    // Map the page into the directory
    currentAddressSpace->getPageDirectory().map(physicalAddress, virtualAddress, flags);
}

void Kernel::MemoryService::mapRange(uint32_t virtualStartAddress, uint32_t virtualEndAddress, uint16_t flags) {
    // Get 4 KiB aligned start and end address
    uint32_t alignedStartAddress = virtualStartAddress & 0xFFFFF000;
    uint32_t alignedEndAddress = virtualEndAddress & 0xFFFFF000;
    alignedEndAddress += (virtualEndAddress % Kernel::Paging::PAGESIZE == 0) ? 0 : Kernel::Paging::PAGESIZE;

    // Map all pages
    for (uint32_t i = alignedStartAddress; i < alignedEndAddress; i += Kernel::Paging::PAGESIZE) {
        map(i, flags);
    }
}

void Kernel::MemoryService::map(uint32_t virtualAddress, uint16_t flags) {
    // Allocate a physical page frame where the page should be mapped
    const auto physicalAddress = reinterpret_cast<uint32_t>(pageFrameAllocator.allocateBlock());
    // Map the page into the directory
    currentAddressSpace->getPageDirectory().map(physicalAddress, virtualAddress, flags);
}

uint32_t Kernel::MemoryService::unmap(uint32_t virtualAddress) {
    uint32_t physAddress = currentAddressSpace->getPageDirectory().unmap(virtualAddress);
    if (!physAddress) {
        return 0;
    }

    pageFrameAllocator.freeBlock((void *) (physAddress));

    // Invalidate entry in TLB
    asm volatile("push %%edx;"
                 "movl %0,%%edx;"
                 "invlpg (%%edx);"
                 "pop %%edx;"  : : "r"(virtualAddress));

    return physAddress;
}

uint32_t Kernel::MemoryService::unmap(uint32_t virtualStartAddress, uint32_t virtualEndAddress) {
    // Remark: if given addresses are not aligned on pages, we do not want to unmap
    // data that could be on the same page before virtualStartAddress or behind virtualEndAddress

    // Get aligned start and end address of the area to be freed
    uint32_t alignedStartAddress = virtualStartAddress & 0xFFFFF000;
    alignedStartAddress += ((virtualStartAddress % Kernel::Paging::PAGESIZE != 0) ? Kernel::Paging::PAGESIZE : 0);
    // Calculate start address of the last page we want to unmap
    uint32_t alignedEndAddress = virtualEndAddress & 0xFFFFF000;
    alignedEndAddress -= (((virtualEndAddress + 1) % Kernel::Paging::PAGESIZE != 0) ? Kernel::Paging::PAGESIZE : 0);

    // Check if an unmap is possible (the start and end address have to contain at least one complete page)
    if (alignedEndAddress < virtualStartAddress) {
        return 0;
    }
    // Amount of pages to be unmapped
    uint32_t pageCount = (alignedEndAddress - alignedStartAddress) / Kernel::Paging::PAGESIZE + 1;

    // loop through the pages and unmap them
    uint32_t ret = 0;
    uint8_t cnt = 0;
    for (uint32_t i = 0; i < pageCount; i++) {
        ret = unmap(alignedStartAddress + i * Kernel::Paging::PAGESIZE);

        if (ret) {
            cnt = 0;
        } else {
            cnt++;
        }

        // If there were three pages after each other already unmapped, we break here.
        // This is sort of a workaround because by merging large free memory blocks in memory management
        // it might happen that some parts of the memory are already unmapped.
        if (cnt == 3) {
            break;
        }
    }


    return ret;
}

void *Kernel::MemoryService::mapIO(uint32_t physicalAddress, uint32_t size) {
    // Get amount of needed pages
    uint32_t pageCnt = size / Kernel::Paging::PAGESIZE;
    pageCnt += (size % Kernel::Paging::PAGESIZE == 0) ? 0 : 1;

    // Allocate 4 KiB aligned virtual memory
    void *virtStartAddress = currentAddressSpace->getMemoryManager().allocateMemory(pageCnt * Kernel::Paging::PAGESIZE, Kernel::Paging::PAGESIZE);

    // Check for nullpointer
    if (virtStartAddress == nullptr) {
        Util::Exception::throwException(Util::Exception::OUT_OF_MEMORY, "mapIO: Not enough space left on kernel heap!");
    }

    // Map the allocated virtual IO memory to physical addresses
    for (uint32_t i = 0; i < pageCnt; i++) {
        // Since the virtual memory is one block, we can update the virtual address this way
        uint32_t virtAddress = (uint32_t) virtStartAddress + i * Kernel::Paging::PAGESIZE;

        // If the virtual address is already mapped, we have to unmap it.
        // This can happen because the headers of the free list are mapped to arbitrary physical addresses,
        // but the IO Memory should be mapped to given physical addresses.
        unmap(virtAddress);

        // Map the page to given physical address
        mapPhysicalAddress(virtAddress, physicalAddress + i * Kernel::Paging::PAGESIZE,
                           Paging::PRESENT | Paging::READ_WRITE | Paging::CACHE_DISABLE | (virtAddress < Kernel::MemoryLayout::KERNEL_START ? Paging::USER_ACCESS : 0));
    }

    return virtStartAddress;
}

void *MemoryService::mapIO(uint32_t size) {
    // Get amount of needed pages
    uint32_t pageCnt = size / Kernel::Paging::PAGESIZE;
    pageCnt += (size % Kernel::Paging::PAGESIZE == 0) ? 0 : 1;

    // Allocate block of physical memory
    void *physStartAddress = pageFrameAllocator.allocateBlock();
    void *currentPhysAddress = physStartAddress;
    void *lastPhysAddress;
    bool contiguous;

    do {
        contiguous = true;

        for (uint32_t i = 0; i < pageCnt; i++) {
            lastPhysAddress = currentPhysAddress;
            currentPhysAddress = pageFrameAllocator.allocateBlockAfterAddress(physStartAddress);

            if (reinterpret_cast<uint32_t>(currentPhysAddress) - reinterpret_cast<uint32_t>(lastPhysAddress) != Kernel::Paging::PAGESIZE) {
                contiguous = false;

                for (uint32_t j = 0; j < i; j++) {
                    pageFrameAllocator.freeBlock(reinterpret_cast<void *>(reinterpret_cast<uint32_t>(physStartAddress) + j * Kernel::Paging::PAGESIZE));
                }

                physStartAddress = currentPhysAddress;
                break;
            }
        }
    } while (!contiguous);

    return mapIO(reinterpret_cast<uint32_t>(physStartAddress), size);
}

VirtualAddressSpace & MemoryService::createAddressSpace() {
    auto addressSpace = new VirtualAddressSpace(kernelAddressSpace.getPageDirectory());
    addressSpaces.add(addressSpace);

    return *addressSpace;
}

void MemoryService::switchAddressSpace(VirtualAddressSpace &addressSpace) {
    // Set current address space
    currentAddressSpace = &addressSpace;
    // load cr3-register with phys. address of Page Directory
    load_page_directory(addressSpace.getPageDirectory().getPageDirectoryPhysicalAddress());

    /*if (!addressSpace.isInitialized()) {
        addressSpace.initialize();
    }*/
}

void MemoryService::removeAddressSpace(VirtualAddressSpace &addressSpace) {
    if (currentAddressSpace == &addressSpace) {
        Util::Exception::throwException(Util::Exception::INVALID_ARGUMENT, "MemoryService: Trying to delete the currently active address space!");
    }

    addressSpaces.remove(&addressSpace);
    delete &addressSpace;
}

void *MemoryService::getPhysicalAddress(void *virtualAddress) {
    return currentAddressSpace->getPageDirectory().getPhysicalAddress(virtualAddress);
}

void MemoryService::plugin() {
    InterruptDispatcher::getInstance().assign(InterruptDispatcher::PAGEFAULT, *this);
}

void MemoryService::trigger(InterruptFrame &frame) {
    // Get page fault address and flags
    uint32_t faultAddress = 0;
    // The faulted linear address is loaded in the cr2 register
    asm ("mov %%cr2, %0" : "=r" (faultAddress));

    // There should be no access to the first page (address 0)
    if (faultAddress == 0) {
        frame.interrupt = (uint32_t) Util::Exception::NULL_POINTER;
        System::panic(frame);
    }

    // check if page fault was caused by illegal page access
    if ((frame.error & 0x00000001u) > 0) {
        Util::Exception::throwException(Util::Exception::ILLEGAL_PAGE_ACCESS);
    }

    // Map the faulted Page
    map(faultAddress, Paging::PRESENT | Paging::READ_WRITE | (faultAddress < Kernel::MemoryLayout::KERNEL_START ? Paging::USER_ACCESS : 0));
    // TODO: Check other Faults
}

MemoryService::MemoryStatus MemoryService::getMemoryStatus() {
    return {pageFrameAllocator.getTotalMemory(), pageFrameAllocator.getFreeMemory(),
            lowerMemoryManager.getTotalMemory(), lowerMemoryManager.getFreeMemory(),
            kernelAddressSpace.getMemoryManager().getTotalMemory(), kernelAddressSpace.getMemoryManager().getFreeMemory(),
            pagingAreaManager.getTotalMemory(), pagingAreaManager.getFreeMemory()};
}

// TODO: Remove this method, once it is no longer needed by FilesystemService
VirtualAddressSpace &MemoryService::getCurrentAddressSpace() {
    return *currentAddressSpace;
}

}