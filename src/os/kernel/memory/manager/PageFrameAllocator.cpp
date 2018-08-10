/* PageFramAllocator - Allocates and frees the page frames in physical memory.
 * Bitmap-based implementation.
 *
 * @author Burak Akguel, Christian Gesse, Fabian Ruhland, Filip Krakowski, Michael Schoettner
 * @date HHU, 2018
 */

#include <lib/multiboot/Structure.h>
#include "PageFrameAllocator.h"

#include "kernel/memory/Paging.h"


/**
 * Constructor - calls baseClass constructor.
 */
PageFrameAllocator::PageFrameAllocator(uint32_t memoryStartAddress, uint32_t memoryEndAddress)
        : BitmapMemoryManager(memoryStartAddress, memoryEndAddress, PAGESIZE, "PAGEFRAMEALLOCATOR", false, false) {

    managerType = PAGE_FRAME_ALLOCATOR;

    freeMemory = memoryEndAddress - memoryStartAddress;

    // calculate amount of physical page frames
    uint32_t pageFrameCnt = freeMemory / blockSize;
    // allocate bitmap for page frames
    freeBitmapLength = pageFrameCnt / 32;
    freeBitmap = new uint32_t[freeBitmapLength];

    // zero out the free bitmap length
    memset(freeBitmap, 0, freeBitmapLength * sizeof(uint32_t));

    // read out how much memory is already used by the system and the initrd
    uint32_t maxIndex = (Multiboot::Structure::physReservedMemoryEnd / PAGESIZE + 1024 + 256) / 32;

    // first X MB are already allocated by 4MB paging
    for(uint32_t i=0; i < maxIndex; i++) {
        freeBitmap[i] = 0xFFFFFFFF;
    }
    // X MB + 8KB are already used by kernel and page tables/dirs
    freeBitmap[maxIndex] = 0xC0000000;

    bmpSearchOffset = maxIndex;

    // subtract already reserved memory from free memory
    freeMemory -= (maxIndex * 32 * blockSize + 2 * blockSize);
}




