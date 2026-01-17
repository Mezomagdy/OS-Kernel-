/*
 * dynamic_allocator.c
 *
 *  Created on: Sep 21, 2023
 *      Author: HP
 */
#include <inc/assert.h>
#include <inc/string.h>
#include "../inc/dynamic_allocator.h"
//==================================================================================//
//============================== GIVEN FUNCTIONS ===================================//
//==================================================================================//
//==================================
//==================================
// [1] GET PAGE VA:
//==================================
__inline__ uint32 to_page_va(struct PageInfoElement *ptrPageInfo)
{
	if (ptrPageInfo < &pageBlockInfoArr[0] || ptrPageInfo >= &pageBlockInfoArr[DYN_ALLOC_MAX_SIZE/PAGE_SIZE])
			panic("to_page_va called with invalid pageInfoPtr");
	//Get start VA of the page from the corresponding Page Info pointer
	int idxInPageInfoArr = (ptrPageInfo - pageBlockInfoArr);
	return dynAllocStart + (idxInPageInfoArr << PGSHIFT);
}

//==================================
// [2] GET PAGE INFO OF PAGE VA:
//==================================
__inline__ struct PageInfoElement * to_page_info(uint32 va)
{
	int idxInPageInfoArr = (va - dynAllocStart) >> PGSHIFT;
	if (idxInPageInfoArr < 0 || idxInPageInfoArr >= DYN_ALLOC_MAX_SIZE/PAGE_SIZE)
		panic("to_page_info called with invalid pa");
	return &pageBlockInfoArr[idxInPageInfoArr];
}
//==================================================================================//
//============================ REQUIRED FUNCTIONS ==================================//
//==================================================================================//

//==================================
// [1] INITIALIZE DYNAMIC ALLOCATOR:
//==================================
bool is_initialized = 0;
void initialize_dynamic_allocator(uint32 daStart, uint32 daEnd)
{
	//==================================================================================
	//DON'T CHANGE THESE LINES==========================================================
	//==================================================================================
	{
		assert(daEnd <= daStart + DYN_ALLOC_MAX_SIZE);
		is_initialized = 1;
	}
	//==================================================================================
	//==================================================================================
	//TODO: [PROJECT'25.GM#1] DYNAMIC ALLOCATOR - #1 initialize_dynamic_allocator
	//Your code is here
	dynAllocStart = daStart;
	dynAllocEnd = daEnd;
	int pages = (daEnd - daStart) / PAGE_SIZE;
	LIST_INIT(&freePagesList);
	for(int i = 0; i < pages; i++){
		struct PageInfoElement *mypage = to_page_info(daStart+(PAGE_SIZE*i));
		mypage->block_size = 0;
		mypage->num_of_free_blocks = 0;
		LIST_INSERT_TAIL(&freePagesList, mypage);
//====================== DEBUGING =============================
//		if(i < 5)
//			cprintf("block info arr [%d] = %x \n", i , to_page_va(mypage));
//=============================================================
	}

	for(int i = 0; i <= LOG2_MAX_SIZE - LOG2_MIN_SIZE; i++){
			LIST_INIT(&freeBlockLists[i]);
		}
//	for(int i = 0; i < pages; i++){
//		LIST_INSERT_TAIL(&freePagesList, &pageBlockInfoArr[i]);
//	}


//====================== DEBUGING =============================
	struct PageInfoElement *page;
	LIST_FOREACH(page, &freePagesList)
	{
	    uint32 va = to_page_va(page);
//	    cprintf("Page VA = %p\n", va);
	}
//	cprintf("total pages = %d \n", pages); //total pages = 8192
//=============================================================

	//Comment the following line
	//panic("initialize_dynamic_allocator() Not implemented yet");

}

//===========================
// [2] GET BLOCK SIZE:
//===========================
__inline__ uint32 get_block_size(void *va)
{
	//TODO: [PROJECT'25.GM#1] DYNAMIC ALLOCATOR - #2 get_block_size
	//Your code is here
	if((uint32)va < dynAllocStart || (uint32)va > dynAllocEnd)
		panic("this va not for the dynamic allocator\n");
	struct PageInfoElement *mypage = to_page_info((uint32)va);
	return mypage->block_size;
	//Comment the following line
	//panic("get_block_size() Not implemented yet");
}

//===========================
// 3) ALLOCATE BLOCK:
//===========================
void *alloc_block(uint32 size)
{
	//==================================================================================
	//DON'T CHANGE THESE LINES==========================================================
	//==================================================================================
	{
		assert(size <= DYN_ALLOC_MAX_BLOCK_SIZE);
	}
	//==================================================================================
	//==================================================================================
	//TODO: [PROJECT'25.GM#1] DYNAMIC ALLOCATOR - #3 alloc_block

	if (size == 0)
	{
		return NULL;
	}

	uint32 nearst_size = DYN_ALLOC_MIN_BLOCK_SIZE;
	int index = 0;
	while (nearst_size < size)
	{
		nearst_size *= 2;
		index++;
	}
//====================== DEBUGING =============================
	int test1 = 0;
	int test2 = 0;
	int test3 = 0;
//=============================================================

	//CASE 1 : there is free block in FreeBlockLists[i]
	if(!LIST_EMPTY(&freeBlockLists[index])){
		//cprintf("case 1 here %d \n", test1); test1++; (TRACING)
		struct BlockElement *myblock = LIST_FIRST(&freeBlockLists[index]);
		LIST_REMOVE(&freeBlockLists[index], myblock);

		struct PageInfoElement *mypage = to_page_info((uint32)myblock);
		mypage->num_of_free_blocks--;
		return (void *)myblock;
	}
	//CASE 2 : NO FREE BLOCKS GET NEW FREE PAGE
	if(LIST_EMPTY(&freeBlockLists[index]) && !LIST_EMPTY(&freePagesList)){
		//cprintf("case 2 here %d \n", test2); test2++; (TRACING)
		struct PageInfoElement *mypage = LIST_FIRST(&freePagesList);
		uint32 mypage_va = to_page_va(mypage);
		//cprintf("mypage_va = %x \n", mypage_va); //(TRACING)
		int ret = get_page((void *)mypage_va);

		if(ret == 0){ //mypage maped to physical frame
			LIST_REMOVE(&freePagesList, mypage);


			mypage->block_size = nearst_size;
			mypage->num_of_free_blocks = PAGE_SIZE / nearst_size - 1; // حجزنا واحد (الأول)


			for (int i = 0; i < mypage->num_of_free_blocks+1; i++)
			{
				uint32 block_va = mypage_va + (i * nearst_size);
				struct BlockElement *myblock = (struct BlockElement *)block_va;
				LIST_INSERT_TAIL(&freeBlockLists[index], myblock);
			}

			struct BlockElement *myblock = LIST_FIRST(&freeBlockLists[index]);
			LIST_REMOVE(&freeBlockLists[index], myblock);

			return (void *)myblock;
		}
		else{ //FAILED TO MAP THE FREE PAGE TO PHYSICAL FRAME
			return (void *) NULL;
		}
	}
	//CASE 3 : NO FREE BLOCKS OR PAGES ALLOCATE BLOCK FROM THE NEXT SIZE
	if(LIST_EMPTY(&freeBlockLists[index]) && LIST_EMPTY(&freePagesList)){
//		cprintf("case 3 here %d \n", test3); test3++;
		int next_index = index + 1;
		int found = 0; //FALG
		for(int i = next_index; i <= LOG2_MAX_SIZE - LOG2_MIN_SIZE; i++){
			if(!LIST_EMPTY(&freeBlockLists[i])){
				found = 1;
				next_index = i;
				break;
			}
		}
		if(found){
		struct BlockElement *myblock = LIST_FIRST(&freeBlockLists[next_index]);
		LIST_REMOVE(&freeBlockLists[next_index], myblock);

		struct PageInfoElement *mypage = to_page_info((uint32)myblock);
		mypage->num_of_free_blocks--;

		return (void *)myblock;
		}
		else{
			return (void *)NULL;
		}
	}
	return (void *)NULL;
}

//===========================
// [4] FREE BLOCK:
//===========================
void free_block(void *va)
{
	//==================================================================================
	//DON'T CHANGE THESE LINES==========================================================
	//==================================================================================
	{
		assert((uint32)va >= dynAllocStart && (uint32)va < dynAllocEnd);
	}
	//==================================================================================
	//==================================================================================

	//TODO: [PROJECT'25.GM#1] DYNAMIC ALLOCATOR - #4 free_block
	//Your code is here
	uint32 size = get_block_size(va);
	if(size == 0){
		panic("THE SIZE IS ZERO (THE PAGE NOT ALLOCATED YET!)\n");
	}
	int index = (-LOG2_MIN_SIZE)-1;
	uint32 tmp = size;
	while(tmp){
		tmp /= 2;
		index++;
	}

	struct PageInfoElement* mypage = to_page_info((uint32)va);
	uint32 mypage_va = to_page_va(mypage);
	struct BlockElement* myblock = (struct BlockElement*)va;
	LIST_INSERT_TAIL(&freeBlockLists[index], myblock);
	mypage->num_of_free_blocks++;

	if (mypage->num_of_free_blocks == PAGE_SIZE / size){
		struct BlockElement* blk;
		LIST_FOREACH(blk, &freeBlockLists[index]){
			//GET PAGE VA TO THE CORESSPONDING BLK
			struct PageInfoElement* blk_page = to_page_info((uint32)blk);
			uint32 blk_page_va = to_page_va(blk_page);
			if(mypage_va == blk_page_va){
				LIST_REMOVE(&freeBlockLists[index], blk);
			}
		}
		return_page((void*)mypage_va);
		mypage->block_size = 0;
		mypage->num_of_free_blocks = 0;
		LIST_INSERT_TAIL(&freePagesList, mypage);
	}
	//Comment the following line
	//panic("free_block() Not implemented yet");
}

//==================================================================================//
//============================== BONUS FUNCTIONS ===================================//
//==================================================================================//

//===========================
// [1] REALLOCATE BLOCK:
//===========================
void *realloc_block(void* va, uint32 new_size)
{
	//TODO: [PROJECT'25.BONUS#2] KERNEL REALLOC - realloc_block
	//Your code is here
	//Comment the following line
	panic("realloc_block() Not implemented yet");
}
