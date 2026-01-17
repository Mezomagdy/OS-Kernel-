#include "kheap.h"

#include <inc/memlayout.h>
#include <inc/dynamic_allocator.h>
#include <kern/conc/sleeplock.h>
#include <kern/proc/user_environment.h>
#include <kern/mem/memory_manager.h>
#include "../conc/kspinlock.h"
#define max 1024

struct kalloc {
	uint32 va;
	uint32 num_pages;
};

struct kalloc alocs[max];
uint32 nums = 0;
//==================================================================================//
//============================== GIVEN FUNCTIONS ===================================//
//==================================================================================//

//==============================================
// [1] INITIALIZE KERNEL HEAP:
//==============================================
//TODO: [PROJECT'25.GM#2] KERNEL HEAP - #0 kheap_init [GIVEN]
//Remember to initialize locks (if any)
void kheap_init()
{
	//==================================================================================
	//DON'T CHANGE THESE LINES==========================================================
	//==================================================================================
	{
		initialize_dynamic_allocator(KERNEL_HEAP_START, KERNEL_HEAP_START + DYN_ALLOC_MAX_SIZE);
		set_kheap_strategy(KHP_PLACE_CUSTOMFIT);
		kheapPageAllocStart = dynAllocEnd + PAGE_SIZE;
		kheapPageAllocBreak = kheapPageAllocStart;
	}
	//==================================================================================
	//==================================================================================
}

//==============================================
// [2] GET A PAGE FROM THE KERNEL FOR DA:
//==============================================
int get_page(void* va)
{
	int ret = alloc_page(ptr_page_directory, ROUNDDOWN((uint32)va, PAGE_SIZE), PERM_WRITEABLE, 1);
	if (ret < 0)
		panic("get_page() in kern: failed to allocate page from the kernel");
	return 0;
}

//==============================================
// [3] RETURN A PAGE FROM THE DA TO KERNEL:
//==============================================
void return_page(void* va)
{
	unmap_frame(ptr_page_directory, ROUNDDOWN((uint32)va, PAGE_SIZE));
}

//==================================================================================//
//============================ REQUIRED FUNCTIONS ==================================//
//==================================================================================//
//===================================
// [1] ALLOCATE SPACE IN KERNEL HEAP:
//===================================
void* kmalloc(unsigned int size)
{
    if (size == 0) return NULL;

    if (size <= DYN_ALLOC_MAX_BLOCK_SIZE) {
        return alloc_block(size);
    }

    uint32 num_of_pages = ROUNDUP(size, PAGE_SIZE) / PAGE_SIZE;

    uint32 va = kheapPageAllocStart;
    uint32 i_va = 0;
    uint32 count = 0;
    uint32 va_exact = 0;
    uint32 va_worst = 0;
    uint32 max_free = 0;
    uint32 found_exact = 0;

    struct FrameInfo *frame = NULL;

    while (va < kheapPageAllocBreak) {
        uint32 *page_table = NULL;
        struct FrameInfo *fi = get_frame_info(ptr_page_directory, va, &page_table);

        if (fi == NULL) {
            if (count == 0) i_va = va;
            count++;
            uint32 *next_page_table = NULL;
            struct FrameInfo *frame_info = NULL;
            get_page_table(ptr_page_directory, (uint32)va+PAGE_SIZE, &next_page_table);
            frame_info = get_frame_info(ptr_page_directory, (uint32)va+PAGE_SIZE, &next_page_table);
            if (count == num_of_pages&&(frame_info != NULL)) {
                va_exact = i_va;
                found_exact = 1;
                break;
            }

            if (count > max_free) {
                max_free = count;
                va_worst = i_va;
            }
        }
        else {
            count = 0;
        }

        va += PAGE_SIZE;
    }

    if (found_exact) {
        for (uint32 i = 0; i < num_of_pages; i++) {
            if (allocate_frame(&frame) != 0 || frame == NULL) {
                for (uint32 j = 0; j < i; j++)
                    unmap_frame(ptr_page_directory,va_exact + j * PAGE_SIZE);
                return NULL;
            }
            map_frame(ptr_page_directory, frame,va_exact + i * PAGE_SIZE,PERM_PRESENT | PERM_WRITEABLE);
            frame->va =va_exact + i * PAGE_SIZE;
        }

        alocs[nums].va = va_exact;
        alocs[nums].num_pages = num_of_pages;
        nums++;
        return (void*)va_exact;
    }

    if (max_free >= num_of_pages && va_worst != 0) {
        for (uint32 i = 0; i < num_of_pages; i++) {
            if (allocate_frame(&frame) != 0 || frame == NULL) {
                for (uint32 j = 0; j < i; j++)
                    unmap_frame(ptr_page_directory, va_worst + j * PAGE_SIZE);
                return NULL;
            }

            map_frame(ptr_page_directory, frame,va_worst + i * PAGE_SIZE,PERM_PRESENT | PERM_WRITEABLE);
            frame->va = va_worst + i * PAGE_SIZE;
        }

        alocs[nums].va = va_worst;
        alocs[nums].num_pages = num_of_pages;
        nums++;
        return (void*)va_worst;
    }


    if (num_of_pages > (0xFFFFFFFF / PAGE_SIZE))
        return NULL;

    uint32 need_allocate = num_of_pages * PAGE_SIZE;

    if (kheapPageAllocBreak > (KERNEL_HEAP_MAX - need_allocate))
        return NULL;

    uint32 first_break = kheapPageAllocBreak;

    if (kheapPageAllocBreak + need_allocate <= KERNEL_HEAP_MAX)
    {
        for (uint32 i = 0; i < num_of_pages; i++) {
            if (allocate_frame(&frame) != 0 || frame == NULL) {

                for (uint32 j = 0; j < i; j++)
                    unmap_frame(ptr_page_directory, first_break + j * PAGE_SIZE);

                return NULL;
            }

            map_frame(ptr_page_directory, frame,first_break + i * PAGE_SIZE,PERM_PRESENT | PERM_WRITEABLE);
            frame->va = first_break + i * PAGE_SIZE;
        }

        alocs[nums].va = first_break;
        alocs[nums].num_pages = num_of_pages;
        nums++;

        kheapPageAllocBreak += need_allocate;
        return (void*)first_break;
    }

    return NULL;
}

//=================================
// [2] FREE SPACE FROM KERNEL HEAP:
//=================================
void kfree(void* virtual_address)
{
    if (virtual_address == NULL)
        return;

    unsigned int va = (unsigned int)virtual_address;

    //DYNAMIC BLOCK ALLOCATOR
    if (va >= dynAllocStart && va < dynAllocEnd) {
        free_block((void*)va);
        return;
    }

    //PAGE ALLOCATOR
    else if (va >= kheapPageAllocStart && va < KERNEL_HEAP_MAX) {
        for (uint32 i = 0; i < nums; i++) {

            if (alocs[i].va == va) {

                for (uint32 k = 0; k < alocs[i].num_pages; k++) {
                    unmap_frame(ptr_page_directory, va + k * PAGE_SIZE);
                }

                for (uint32 j = i; j < nums - 1; j++) {
                    alocs[j] = alocs[j + 1];
                }
                nums--;

                if (nums == 0) {
                    kheapPageAllocBreak = kheapPageAllocStart;
                } else {

                    uint32 last_end = 0;
                    for (uint32 k = 0; k < nums; k++) {
                        uint32 end = alocs[k].va + alocs[k].num_pages * PAGE_SIZE;
                        if (end > last_end)
                            last_end = end;
                    }
                    kheapPageAllocBreak = last_end;
                }

                return;
            }
        }

    }

    // --------- INVALID ADDRESS ---------
    else {
        panic("kfree() - INVALID ADDRESS!");
    }
}

//=================================
// [3] FIND VA OF GIVEN PA:
//=================================
unsigned int kheap_virtual_address(unsigned int physical_address)
{
	unsigned int phys=  physical_address ;
		if (phys != 0)
		{

		struct FrameInfo* frm = to_frame_info(phys);
		if (frm != NULL && frm->va != 0)
			return frm->va | PGOFF(phys);

		uint32 frmp = phys - PGOFF(phys);
		uint32 scan;
		uint32* ptr_page_table = NULL;
		struct FrameInfo* f;

		for (scan = dynAllocStart; scan < dynAllocEnd; scan += PAGE_SIZE)
		{
			f = get_frame_info(ptr_page_directory, scan, &ptr_page_table);
			if (f != NULL && to_physical_address(f) == frmp)
			{
				f->va = scan;
				return scan | PGOFF(phys);
			}
		}
		}
		return 0;
}

//=================================
// [4] FIND PA OF GIVEN VA:
//=================================
unsigned int kheap_physical_address(unsigned int virtual_address)
{


	uint32* ptrn = NULL;
	struct FrameInfo* frm = get_frame_info(ptr_page_directory, virtual_address, &ptrn);

	if (frm != NULL)
		return to_physical_address(frm) | PGOFF(virtual_address);
	return 0;
}

//=================================================================================//
//============================== BONUS FUNCTION ===================================//
//=================================================================================//
// krealloc():

//	Attempts to resize the allocated space at "virtual_address" to "new_size" bytes,
//	possibly moving it in the heap.
//	If successful, returns the new virtual_address, in which case the old virtual_address must no longer be accessed.
//	On failure, returns a null pointer, and the old virtual_address remains valid.

//	A call with virtual_address = null is equivalent to kmalloc().
//	A call with new_size = zero is equivalent to kfree().

extern __inline__ uint32 get_block_size(void *va);

void *krealloc(void *virtual_address, uint32 new_size)
{
	//TODO: [PROJECT'25.BONUS#2] KERNEL REALLOC - krealloc
	//Your code is here
	//Comment the following line
	panic("krealloc() is not implemented yet...!!");
}
