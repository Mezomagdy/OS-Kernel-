
/*
 * chunk_operations.c
 *
 *  Created on: Oct 12, 2022
 *      Author: HP
 */
#include <kern/trap/fault_handler.h>
#include <kern/disk/pagefile_manager.h>
#include <kern/proc/user_environment.h>
#include "kheap.h"
#include "memory_manager.h"
#include <inc/queue.h>
#include <kern/mem/memory_manager.h>
#include <inc/mmu.h>
extern void pf_remove_env_page(struct Env* e, uint32 virtual_address);
//extern void inctst();
/******************************/
/*[1] RAM CHUNKS MANIPULATION */
/******************************/
//===============================
// 1) CUT-PASTE PAGES IN RAM:
//===============================
//This function should cut-paste the given number of pages from source_va to dest_va on the given page_directory
//	If the page table at any destination page in the range is not exist, it should create it
//	If ANY of the destination pages exists, deny the entire process and return -1. Otherwise, cut-paste the number of pages and return 0
//	ALL 12 permission bits of the destination should be TYPICAL to those of the source
//	The given addresses may be not aligned on 4 KB
int cut_paste_pages(uint32* page_directory,uint32 source_va,uint32 dest_va,uint32 num_of_pages)
{
	panic("cut_paste_pages() is not implemented yet...!!");
	return 0;
}
//===============================
// 2) COPY-PASTE RANGE IN RAM:
//===============================
//This function should copy-paste the given size from source_va to dest_va on the given page_directory
//	Ranges DO NOT overlapped.
//	If ANY of the destination pages exists with READ ONLY permission, deny the entire process and return -1.
//	If the page table at any destination page in the range is not exist, it should create it
//	If ANY of the destination pages doesn't exist, create it with the following permissions then copy.
//	Otherwise, just copy!
//		1. WRITABLE permission
//		2. USER/SUPERVISOR permission must be SAME as the one of the source
//	The given range(s) may be not aligned on 4 KB
int copy_paste_chunk(uint32* page_directory, uint32 source_va, uint32 dest_va, uint32 size)
{
	//TODO: PRACTICE: fill this function.
		//Comment the following line
	panic("copy_paste_chunk() is not implemented yet...!!");
	return 0;
}

//===============================
// 3) SHARE RANGE IN RAM:
//===============================
//This function should share the given size from source_va to dest_va on the given page_directory
//	Ranges DO NOT overlapped.
//	It should set the permissions of the second range by the given perms
//	If ANY of the destination pages exists, deny the entire process and return -1.
//	Otherwise, share the required range and return 0
//	During the share process:
//		1. If the page table at any destination page in the range is not exist, it should create it
//	The given range(s) may be not aligned on 4 KB
int share_chunk(uint32* page_directory,uint32 source_va,uint32 dest_va,uint32 size,uint32 perms)
{
	//TODO: PRACTICE: fill this function.
	//Comment the following line
	panic("share_chunk() is not implemented yet...!!");
	return 0;
}
//==================================================================
// 4) ALLOCATE CHUNK IN RAM (LAZY ALLOCATION)
//==================================================================
//This function should allocate the given virtual range [<va>, <va> + <size>) in the given address space  <page_directory> with the given permissions <perms>.
//	If ANY of the destination pages exists, deny the entire process and return -1. Otherwise, allocate the required range and return 0
//	If the page table at any destination page in the range is not exist, it should create it
//	Allocation should be aligned on page boundary. However, the given range may be not aligned.
int allocate_chunk(uint32* page_directory,uint32 virtual_address,uint32 size,uint32 perms)
{
	//TODO: PRACTICE: fill this function.
		//Comment the following line
	uint32 start_va=ROUNDDOWN(virtual_address,PAGE_SIZE);// بنظبط البداية والنهاية على حدودالصفحات
	uint32 end_va=ROUNDUP(virtual_address+size,PAGE_SIZE);
	// الأول:نشوف هلفي صفحة موجودة ومأخوذة قبل كده ولا لأ
	for (uint32 va=start_va;va<end_va;va+=PAGE_SIZE)
	{		uint32* pt=NULL;
		get_page_table(page_directory,va,&pt);
		// لو في Page Table
		if (pt!=NULL)
		{uint32 entry=pt[PTX(va)];
			// لو الـ frame موجودة بجد = PRESENT → مينفعش نoverwrite
			if(entry&PERM_PRESENT)
			return -1;	}
	}
	// هنحط ال permission بس من غير م نعمل mapping للـ frame(lazy allocation)
	uint32 lazy_mark=(perms|PERM_UHPAGE)&~PERM_PRESENT;
	for (uint32 va=start_va;va<end_va;va+=PAGE_SIZE)
	{		// نضمن وجود Page Table
		void*p=create_page_table(page_directory,va);
		if(p==NULL)return -1;
		uint32* pt=NULL;
		get_page_table(page_directory,va,&pt);
		if(pt!=NULL)
		{// لو الخانة فاضية نحط العلامة
		if(pt[PTX(va)]==0)
				pt[PTX(va)]=lazy_mark;
			else
			{
				return -1;//لو فيها أي حاجة تانية يبقى مينفعش نعدل عليها
			}
		}
	}
	return 0;}
//==================================================================
// ALLOCATE USER MEMORY
//==================================================================
void allocate_user_mem(struct Env* e, uint32 virtual_address, uint32 size)
{
	//TODO: [PROJECT'25.IM#2] USER HEAP - #2 allocate_user_mem
	//Your code is here
	//Comment the following line
	// نرتب العنوانين على حدود الصفحات
	uint32 start_va=ROUNDDOWN(virtual_address,PAGE_SIZE);
	uint32 end_va=ROUNDUP(virtual_address+size,PAGE_SIZE);
	// صلاحيات الصفحة .. writable + user + userheap page
	uint32 perms=PERM_USER|PERM_WRITEABLE|PERM_UHPAGE;
	uint32 *ptr=NULL;
	for(uint32 va=start_va;va<end_va;va +=PAGE_SIZE)
	{// لو الـ page table مش موجودة نعملها
	int table_ptr=get_page_table(e->env_page_directory,va,&ptr);
        if (table_ptr==TABLE_NOT_EXIST)
            create_page_table(e->env_page_directory,va);
		// نحدّث الصلاحيات من غير ما نعمل mapping لفرام فعلي
		pt_set_page_permissions(e->env_page_directory,va,perms,0);	}}
//=====================================
// 5) CALCULATE FREE SPACE:
//=====================================
//It should count the number of free pages in the given range [va1, va2)
//(i.e. number of pages that are not mapped).
//Addresses may not be aligned on page boundaries
uint32 calculate_free_space(uint32* page_directory,uint32 sva,uint32 eva)
{	//TODO: PRACTICE: fill this function.
	//Comment the following line
	// نرتب البداية والنهاية على حدود الصفحات
	uint32 start_va=ROUNDDOWN(sva,PAGE_SIZE);
	uint32 end_va=ROUNDUP(eva,PAGE_SIZE);
	uint32 free_pages=0;
	for(uint32 va=start_va;va<end_va;va+=PAGE_SIZE)
	{
		uint32* pt=NULL;
		get_page_table(page_directory,va,&pt);
		// لو مفيش Page Table أصلاً → دي صفحة فاضية
		if(pt==NULL)
		{
	free_pages++;
		}
		else
		{			// لو الـ entry فاضية → برضه صفحة فاضية
			if(pt[PTX(va)]==0)
				free_pages++;}}
	return free_pages;
}
//==================================================================
// 6) CALCULATE ALLOCATED SPACE
//==================================================================
void calculate_allocated_space(uint32* page_directory,uint32 sva,uint32 eva,uint32 *num_tables,uint32 *num_pages)
{//TODO: PRACTICE: fill this function.
	//Comment the following line
	uint32 start_va=ROUNDDOWN(sva,PAGE_SIZE);	uint32 end_va=ROUNDUP(eva,PAGE_SIZE);
	// تصفير العدادات
	*num_tables=0;
	*num_pages=0;
	// نعدّ الصفحات المتعملة بجد
	for(uint32 va=start_va;va<end_va;va+=PAGE_SIZE)
	{
		uint32* pt=NULL;
		get_page_table(page_directory,va,&pt);
		if(pt!=NULL)
		{
			uint32 entry=pt[PTX(va)];
			if(entry&PERM_PRESENT)
		(*num_pages)++;}
	}
	// نعدّ عدد الـ page tables الموجودة
	uint32 s=PDX(start_va);
	uint32 e=PDX(end_va - 1);
	for(uint32 i=s;i<=e;i++)
	{
		if(page_directory[i]&PERM_PRESENT)
			(*num_tables)++;
	}
}

//=====================================
// 7) CALCULATE REQUIRED FRAMES IN RAM:
//=====================================
//This function should calculate the required number of pages for allocating and mapping the given range [start va, start va + size) (either for the pages themselves or for the page tables required for mapping)
//	Pages and/or page tables that are already exist in the range SHOULD NOT be counted.
//	The given range(s) may be not aligned on 4 KB
uint32 calculate_required_frames(uint32* page_directory, uint32 sva, uint32 size)
{
	uint32 start_va=ROUNDDOWN(sva,PAGE_SIZE);
	uint32 end_va=ROUNDUP(sva+size,PAGE_SIZE);
	uint32 frames=0;
	for(uint32 va=start_va;va<end_va;va+=PAGE_SIZE)
	{		// لو مفيش Page Table → هنحتاج واحدة + frame
		uint32* pt=NULL;
		get_page_table(page_directory,va,&pt);
		if(pt==NULL)
		{
			frames+=2;// page table + page
		}
		else
		{	// لو مفيش frame mapped → نعدّ واحدة
			if((pt[PTX(va)]&PERM_PRESENT)==0)
			frames++;
		}
	}
	return frames;
}
//=================================================================================//
//===========================END RAM CHUNKS MANIPULATION ==========================//
//=================================================================================//
/*******************************/
/*[2] USER CHUNKS MANIPULATION */
/*******************************/
//======================================================
/// functions used for USER HEAP (malloc, free, ...)
//======================================================
//=====================================
/* DYNAMIC ALLOCATOR SYSTEM CALLS */
//=====================================
/*2024*/
void* sys_sbrk(int numOfPages)
{
panic("not implemented function");
}

//==================================================================
// 2) FREE USER MEMORY
//==================================================================
void free_user_mem(struct Env *e,uint32 virtual_address,uint32 size)
{
	//TODO: [PROJECT'25.IM#2] USER HEAP - #4 free_user_mem
	//Your code is here
	//Comment the following line
    uint32 start_va=ROUNDDOWN(virtual_address,PAGE_SIZE);
    uint32 end_va=ROUNDUP(virtual_address+size,PAGE_SIZE);
    for(uint32 va=start_va;va<end_va;va+=PAGE_SIZE)
    {        uint32 *pt=NULL;
        get_page_table(e->env_page_directory,va,&pt);
        if(pt!=NULL)
        {
           uint32 entry=pt[PTX(va)];
			// لو في frame حقيقية → نفكها
            if(entry&PERM_PRESENT)
            {
               struct FrameInfo *fi=to_frame_info(entry & ~0xFFF);
                free_frame(fi);
                tlb_invalidate(e->env_page_directory,(void*)va);
            }
			// نمسحها منالـ working set
            env_page_ws_invalidate(e,va);
			// لوكانت buffered أو user heap → نمسحهامن ال page file
            if(entry&(PERM_BUFFERED|PERM_UHPAGE))
            {
                pf_remove_env_page(e,va);
            }
			// نمسح الـ entry خالص
            pt[PTX(va)]=0;
        }
    }
}
void __free_user_mem_with_buffering(struct Env* e, uint32 virtual_address, uint32 size)
{
	panic("__free_user_mem_with_buffering() is not implemented yet...!!");
}
//=====================================
// 3) MOVE USER MEMORY:
//=====================================
void move_user_mem(struct Env* e, uint32 src_va, uint32 dst_va, uint32 size)
{
	panic("move_user_mem() is not implemented yet...!!");
}
//=================================================================================//
//========================== END USER CHUNKS MANIPULATION =========================//
//=================================================================================//

