#include<inc/lib.h>
//==================================================================================//
//============================== GIVEN FUNCTIONS ===================================//
//===============================================================
// ===============================================================
// مصفوفة بنسجل فيها حجم الallocation اللي بدأ عند كل صفحة
// لو القيمة = 0 → الصفحة فاضية
// ===============================================================
#define MAX_HEAP_PAGES ((USER_HEAP_MAX - USER_HEAP_START) / PAGE_SIZE)
uint32 pages_alloc_sizes[MAX_HEAP_PAGES];
// ===============================================================
// بتسجّل إن فيه allocation بدأ في صفحة معينة
// ===============================================================
void record_page_allocation(uint32 va,uint32 size)
{    // لو العنوان برّه الheap ملهوش لازمة
	if(va<USER_HEAP_START||va>=USER_HEAP_MAX)return;
    // هات رقم الصفحة اللي جوّه الheap
	uint32 idx=(va-USER_HEAP_START)/PAGE_SIZE;
    // سجّل حجم الallocation اللي بدأ من الصفحة دي
pages_alloc_sizes[idx]=size;
	}
// ===============================================================
// ترجع حجم الallocation اللي بدأ عند عنوان معين
// ===============================================================
uint32 get_page_allocation(uint32 va,uint32 size){
	if(va<USER_HEAP_START||va>=USER_HEAP_MAX)return 0;
	    uint32 idx=(va-USER_HEAP_START)/PAGE_SIZE;
	    return pages_alloc_sizes[idx];
}
// ===============================================================
//متغيرات عامة للهيب
// ===============================================================
int __firstTimeFlag=1;//علشان نعمل init مرة واحدة
uint32 uheapPlaceStrategy; //استراتيجية الheap
uint32 uheapPageAllocStart;// بداية منطقة page allocator
uint32 uheapPageAllocBreak; // آخر عنوان اتخصص
// ===============================================================
//تهيئة اليوزر هيب لأول مرة
// ===============================================================
void uheap_init()
{if(__firstTimeFlag){        //تجهيز الدايناميك allocator للبلوكات الصغيرة
    initialize_dynamic_allocator(USER_HEAP_START,USER_HEAP_START+DYN_ALLOC_MAX_SIZE);
    // بجيب الاستراتيجية من الكيرنل
    uheapPlaceStrategy=sys_get_uheap_strategy();
    // أول صفحة نخصص فيها بتكون بعد نهاية الدايناميك allocator
    uheapPageAllocStart=dynAllocEnd+PAGE_SIZE;
    // لسه مش مخصصين حاجة
    uheapPageAllocBreak=uheapPageAllocStart;
__firstTimeFlag=0;
}
	}
// ===============================================================
// تخصيص صفحة واحدة للدايناميك allocator من الكيرنل
// ===============================================================
int get_page(void* va)
{
    int ret=__sys_allocate_page(ROUNDDOWN(va,PAGE_SIZE),PERM_USER|PERM_WRITEABLE|PERM_UHPAGE);
    if(ret<0)
        panic("get_page() user: page allocation failed");
    return 0;
}
// ===============================================================
// ترجيع صفحة للكيرنل
// ===============================================================
void return_page(void *va)
{
    int ret=__sys_unmap_frame(ROUNDDOWN((uint32)va,PAGE_SIZE));
    if (ret<0)
        panic("return_page() user: unmap failed");
}
// ===============================================================
// دالة تشوف هل الصفحة دي فاضية ولا لأ
// ===============================================================
int is_page_free(uint32 va){
    if(va<USER_HEAP_START||va>=USER_HEAP_MAX)
        return 0;
    uint32 idx=(va-USER_HEAP_START)/PAGE_SIZE;
    // لو فيه size هنا يبقى دي بداية allocation → مش فاضية
    if(pages_alloc_sizes[idx]!=0)
        return 0;
    uint32 i=idx;
    while(i>0)
    {
        i--;
        if(pages_alloc_sizes[i]!=0)
        {            // حجم الallocation اللي بدأ في الصفحة i
          uint32 alloc_size=pages_alloc_sizes[i];
           uint32 alloc_pages=ROUNDUP(alloc_size,PAGE_SIZE)/PAGE_SIZE;
            if (i+alloc_pages>idx)            // لو الallocation مغطي الصفحة المطلوبة → مش فاضية

                return 0;
        }
    }
    return 1;   // لو عدّينا كل ده → الصفحة فاضية
}
// ===============================================================
// البحث عن مكان مناسب للallocation
// ===============================================================
uint32 search_for_custom_fit(uint32 size)
{
    // حجم غلط أو أكبر من الهيب
    if (size==0||size>USER_HEAP_MAX)
    {
        return 0;
    }
    uint32 needed_size=ROUNDUP(size,PAGE_SIZE);    // نعمله Round-up لصفحات
    if (needed_size<size||needed_size>USER_HEAP_MAX)
    {
        return 0;
    }

    uint32 best_worst_addr=0;
    uint32 max_size_found=0;
    uint32 current_hole_start=0;
    uint32 current_hole_size=0;
    for(uint32 va=uheapPageAllocStart;va<uheapPageAllocBreak;va+=PAGE_SIZE)    // نمشي على الصفحات لحد آخر مكان اتخصص
    {
        if(is_page_free(va))// لو الصفحة فاضية → بنبني فتحة
        {
            if(current_hole_size==0)
                current_hole_start=va;

            current_hole_size+=PAGE_SIZE;
        }
        else
        {
            if(current_hole_size>=needed_size) // طلعنا من فتحة → افحصها
            {
                if(current_hole_size==needed_size)
                    return current_hole_start;

                if(current_hole_size>max_size_found)
                {
                    max_size_found=current_hole_size;
                    best_worst_addr=current_hole_start;
                }
            }

            current_hole_size=0;
        }
    }
    if(current_hole_size>=needed_size)    // لو آخر فتحة وصلت لنهاية الheap
    {
         if(current_hole_size==needed_size)
            return current_hole_start;

        if(current_hole_size>max_size_found)
            best_worst_addr=current_hole_start;
    }
    if(best_worst_addr!=0)    // لو لقينا Worst Fit → رجعه
        return best_worst_addr;
    // لو مفيش مكان فاضي → وسّع الheap
    if(uheapPageAllocBreak>USER_HEAP_MAX-needed_size)
    {
        return 0;
    }

    uint32 ret=uheapPageAllocBreak;
    uheapPageAllocBreak+=needed_size;
    return ret;

}
//==================================================================================//
//============================ REQUIRED FUNCTIONS ==================================//
//==================================================================================//
// ===================================================================
// ============================ malloc =================================
// ===================================================================
void* malloc(uint32 size)
{//==============================================================
	//DON'T CHANGE THIS CODE========================================
	uheap_init();
	if (size==0) return NULL ;
	//==============================================================
	//TODO: [PROJECT'25.IM#2] USER HEAP - #1 malloc
	//Your code is here
	//Comment the following line
    if (size<=DYN_ALLOC_MAX_BLOCK_SIZE)    // لو الحجم صغير → استخدم dynamic allocator العادي
        return alloc_block(size);
    // غير كده → تخصيص صفحات
    uint32 pageSize=ROUNDUP(size,PAGE_SIZE);
    uint32 allocated_va=search_for_custom_fit(pageSize);
    if (allocated_va==0)
        return NULL;
    allocated_va=ROUNDDOWN(allocated_va,PAGE_SIZE);
    record_page_allocation(allocated_va,pageSize);    // سجل الallocation
    sys_allocate_user_mem(allocated_va,pageSize);    // اطلب الميموري من الكيرنل
    return (void*)allocated_va;
}
// ===================================================================
// ============================== free =================================
// ===================================================================
void free(void*virtual_address)
{
	//TODO: [PROJECT'25.IM#2] USER HEAP - #3 free
	//Your code is here
	//Comment the following line
    if(virtual_address==NULL)
        return;
    uint32 va=(uint32)virtual_address;
    if(va>=USER_HEAP_START&&va<uheapPageAllocStart)    // لو العنوان داخل منطقة dynamic allocator
    {
        free_block(virtual_address);
        return;
    }
    if (va>=uheapPageAllocStart&&va<USER_HEAP_MAX)    // لو داخل منطقة page allocator
    {
        va=ROUNDDOWN(va,PAGE_SIZE);
        uint32 idx=(va-USER_HEAP_START) / PAGE_SIZE;
        uint32 size=pages_alloc_sizes[idx];
         if (size==0)        // مفيش allocation بدأ هنا → Double Free
     panic("free(): invalid address or double free");
        pages_alloc_sizes[idx]=0;        // امسح السجل
        sys_free_user_mem(va,size);        // رجع الميموري للكيرنل
        if(va+size==uheapPageAllocBreak)        // لو ده آخر بلوك في الهيب → قلّل break
        {
            while(uheapPageAllocBreak>uheapPageAllocStart)
            {                // نرجع break خطوة خطوة لو آخر صفحة فاضية
                if(is_page_free(uheapPageAllocBreak-PAGE_SIZE))
                   uheapPageAllocBreak-=PAGE_SIZE;
             else
                    break;
            }
        }
        return;
    }
    panic("free(): invalid address");
}
//=================================
// [3] ALLOCATE SHARED VARIABLE:
//=================================
void* smalloc(char *sharedVarName, uint32 size, uint8 isWritable)
{
	//==============================================================
	//DON'T CHANGE THIS CODE========================================
	uheap_init();
	if (size == 0) return NULL ;
	//==============================================================

	//TODO: [PROJECT'25.IM#3] SHARED MEMORY - #2 smalloc
	//Your code is here
	//Comment the following line

	uint32 pagesize=ROUNDUP(size,PAGE_SIZE);
		if(pagesize>(USER_HEAP_MAX-USER_HEAP_START))
			return NULL;

		uint32 exact=0;
		uint32 worst=0;
		uint32 max_hole_size=0;

		uint32 search=ROUNDUP(uheapPageAllocStart,PAGE_SIZE);

		while(search<uheapPageAllocBreak){
			uint32 idx=(search-USER_HEAP_START)/PAGE_SIZE;
		if(pages_alloc_sizes[idx]!=0){
			search+=pages_alloc_sizes[idx];
			continue;
		}
		uint32 start=search;
		uint32 hole_size=0;
		while(search<uheapPageAllocBreak){
			uint32 tmpidx=(search-USER_HEAP_START)/PAGE_SIZE;
			if(pages_alloc_sizes[tmpidx]!=0){
				break;}
			hole_size+=PAGE_SIZE;
			search+=PAGE_SIZE;}

		if(hole_size>=pagesize){
			if(hole_size==pagesize){
				exact=start;
				break;
			}
		if(hole_size>max_hole_size){
			max_hole_size=hole_size;
			worst=start;
		}
		}
		}
	uint32 va=0;
	if(exact!=0)
		va=exact;
	else if(worst!=0)
		va=worst;
	  else{
		va=ROUNDUP(uheapPageAllocBreak,PAGE_SIZE);
		if(va+pagesize>USER_HEAP_MAX)
			return NULL;}
	if(va+pagesize>uheapPageAllocBreak)
		uheapPageAllocBreak=va+pagesize;

	uint32 numpages=pagesize/PAGE_SIZE;
	uint32 index=(va-USER_HEAP_START)/PAGE_SIZE;

	for(uint32 i=0;i<numpages;i++)
		pages_alloc_sizes[index+i]=pagesize;
int result=sys_create_shared_object(sharedVarName,size,isWritable,(void*)va);
	if(result<0){
		for(uint32 i=0;i<numpages;i++)
			pages_alloc_sizes[index+i]=0;
		return NULL;

}
	return (void*)va;
	}

//========================================
// [4] SHARE ON ALLOCATED SHARED VARIABLE:
//========================================
void* sget(int32 ownerEnvID, char *sharedVarName)
{
	//==============================================================
	//DON'T CHANGE THIS CODE========================================
	uheap_init();
	//==============================================================

	//TODO: [PROJECT'25.IM#3] SHARED MEMORY - #4 sget
	//Your code is here
	//Comment the following line

	int sizeres=sys_size_of_shared_object(ownerEnvID,sharedVarName);
	if(sizeres<=0)
		return NULL;
	uint32 size=(uint32)sizeres;
	uint32 pagesize=ROUNDUP(size,PAGE_SIZE);
	if(pagesize>(USER_HEAP_MAX-USER_HEAP_START))
		return NULL;

	uint32 exact=0;
	uint32 worst=0;
	uint32 max_hole_size=0;

	uint32 search=ROUNDUP(uheapPageAllocStart,PAGE_SIZE);

	while(search<uheapPageAllocBreak){
		uint32 idx=(search-USER_HEAP_START)/PAGE_SIZE;
	if(pages_alloc_sizes[idx]!=0){
		search+=pages_alloc_sizes[idx];
		continue;
	}
	uint32 start=search;
	uint32 hole_size=0;
	while(search<uheapPageAllocBreak){
		uint32 tmpidx=(search-USER_HEAP_START)/PAGE_SIZE;
		if(pages_alloc_sizes[tmpidx]!=0){
			break;}
		hole_size+=PAGE_SIZE;
		search+=PAGE_SIZE;}

	if(hole_size>=pagesize){
		if(hole_size==pagesize){
			exact=start;
			break;
		}
	if(hole_size>max_hole_size){
		max_hole_size=hole_size;
		worst=start;
	}
	}
	}
uint32 va=0;
if(exact!=0)
	va=exact;
else if(worst!=0)
	va=worst;
  else{
	va=ROUNDUP(uheapPageAllocBreak,PAGE_SIZE);
	if(va+pagesize>USER_HEAP_MAX)
		return NULL;}
if(va+pagesize>uheapPageAllocBreak)
	uheapPageAllocBreak=va+pagesize;

uint32 numpages=pagesize/PAGE_SIZE;
uint32 index=(va-USER_HEAP_START)/PAGE_SIZE;

for(uint32 i=0;i<numpages;i++)
	pages_alloc_sizes[index+i]=pagesize;

int result=sys_get_shared_object(ownerEnvID,sharedVarName,(void*)va);
	if(result<0){
		for(uint32 i=0;i<numpages;i++)
			pages_alloc_sizes[index+i]=0;
		return NULL;

}
	return (void*)va;

}


//==================================================================================//
//============================== BONUS FUNCTIONS ===================================//
//==================================================================================//


//=================================
// REALLOC USER SPACE:
//=================================
//	Attempts to resize the allocated space at "virtual_address" to "new_size" bytes,
//	possibly moving it in the heap.
//	If successful, returns the new virtual_address, in which case the old virtual_address must no longer be accessed.
//	On failure, returns a null pointer, and the old virtual_address remains valid.

//	A call with virtual_address = null is equivalent to malloc().
//	A call with new_size = zero is equivalent to free().

//  Hint: you may need to use the sys_move_user_mem(...)
//		which switches to the kernel mode, calls move_user_mem(...)
//		in "kern/mem/chunk_operations.c", then switch back to the user mode here
//	the move_user_mem() function is empty, make sure to implement it.
void *realloc(void *virtual_address, uint32 new_size)
{
	//==============================================================
	//DON'T CHANGE THIS CODE========================================
	uheap_init();
	//==============================================================
	panic("realloc() is not implemented yet...!!");
}


//=================================
// FREE SHARED VARIABLE:
//=================================
//	This function frees the shared variable at the given virtual_address
//	To do this, we need to switch to the kernel, free the pages AND "EMPTY" PAGE TABLES
//	from main memory then switch back to the user again.
//
//	use sys_delete_shared_object(...); which switches to the kernel mode,
//	calls delete_shared_object(...) in "shared_memory_manager.c", then switch back to the user mode here
//	the delete_shared_object() function is empty, make sure to implement it.
void sfree(void* virtual_address)
{
	//TODO: [PROJECT'25.BONUS#5] EXIT #2 - sfree
	//Your code is here
	//Comment the following line
	panic("sfree() is not implemented yet...!!");

	//	1) you should find the ID of the shared variable at the given address
	//	2) you need to call sys_freeSharedObject()
}


//==================================================================================//
//========================== MODIFICATION FUNCTIONS ================================//
//==================================================================================//
