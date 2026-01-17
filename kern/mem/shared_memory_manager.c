#include <inc/memlayout.h>
#include "shared_memory_manager.h"

#include <inc/mmu.h>
#include <inc/error.h>
#include <inc/string.h>
#include <inc/assert.h>
#include <inc/queue.h>
#include <inc/environment_definitions.h>

#include <kern/proc/user_environment.h>
#include <kern/trap/syscall.h>
#include "kheap.h"
#include "memory_manager.h"

//==================================================================================//
//============================== GIVEN FUNCTIONS ===================================//
//==================================================================================//

//===========================
// [1] INITIALIZE SHARES:
//===========================
//Initialize the list and the corresponding lock
void sharing_init()
{
#if USE_KHEAP
	LIST_INIT(&AllShares.shares_list) ;
	init_kspinlock(&AllShares.shareslock, "shares lock");
	//init_sleeplock(&AllShares.sharessleeplock, "shares sleep lock");
#else
	panic("not handled when KERN HEAP is disabled");
#endif
}

//=========================
// [2] Find Share Object:
//=========================
//Search for the given shared object in the "shares_list"
//Return:
//	a) if found: ptr to Share object
//	b) else: NULL
struct Share* find_share(int32 ownerID, char* name)
{
#if USE_KHEAP
	struct Share * ret = NULL;
	bool wasHeld = holding_kspinlock(&(AllShares.shareslock));
	if (!wasHeld)
	{
		acquire_kspinlock(&(AllShares.shareslock));
	}
	{
		struct Share * shr ;
		LIST_FOREACH(shr, &(AllShares.shares_list))
		{
			//cprintf("shared var name = %s compared with %s\n", name, shr->name);
			if(shr->ownerID == ownerID && strcmp(name, shr->name)==0)
			{
				//cprintf("%s found\n", name);
				ret = shr;
				break;
			}
		}
	}
	if (!wasHeld)
	{
		release_kspinlock(&(AllShares.shareslock));
	}
	return ret;
#else
	panic("not handled when KERN HEAP is disabled");
#endif
}

//==============================
// [3] Get Size of Share Object:
//==============================
int size_of_shared_object(int32 ownerID, char* shareName)
{
	// This function should return the size of the given shared object
	// RETURN:
	//	a) If found, return size of shared object
	//	b) Else, return E_SHARED_MEM_NOT_EXISTS
	//
	struct Share* ptr_share = find_share(ownerID, shareName);
	if (ptr_share == NULL)
		return E_SHARED_MEM_NOT_EXISTS;
	else
		return ptr_share->size;

	return 0;
}
//===========================================================


//==================================================================================//
//============================ REQUIRED FUNCTIONS ==================================//
//==================================================================================//

//=====================================
// [1] Alloc & Initialize Share Object:
//=====================================
//Allocates a new shared object and initialize its member
//It dynamically creates the "framesStorage"
//Return: allocatedObject (pointer to struct Share) passed by reference
struct Share* alloc_share(int32 ownerID, char* shareName, uint32 size, uint8 isWritable)
{
	//TODO: [PROJECT'25.IM#3] SHARED MEMORY - #1 alloc_share
	//Your code is here
	//Comment the following line
	struct Share*p=kmalloc(sizeof(struct Share));
	if(p==NULL)
		return NULL;
	p->references=1;
	p->ownerID=ownerID;
	p->ID=((uint32)p)&0x7FFFFFFF;
	p->size=size;
	p->isWritable=isWritable;

	strncpy(p->name,shareName,63);
	p->name[63]='\0';

	uint32 numframes=ROUNDUP(size,PAGE_SIZE)/PAGE_SIZE;
	p->framesStorage=(struct FrameInfo**)kmalloc(numframes*sizeof(struct FrameInfo*));

	//in case failed
	if(p->framesStorage==NULL){
		kfree(p);
		return NULL;
	}
	//initialize it by zero
	memset(p->framesStorage,0,numframes*sizeof(struct FrameInfo*));
	// in case successed
	return p;
}


//=========================
// [4] Create Share Object:
//=========================
int create_shared_object(int32 ownerID, char* shareName, uint32 size, uint8 isWritable, void* virtual_address)
{
	//TODO: [PROJECT'25.IM#3] SHARED MEMORY - #3 create_shared_object
	//Your code is here
	//Comment the following line
	struct Env* myenv = get_cpu_proc(); //The calling environment

	// This function should create the shared object at the given virtual address with the given size
	// and return the ShareObjectID
	// RETURN:
	//	a) ID of the shared object (its VA after masking out its msb) if success
	//	b) E_SHARED_MEM_EXISTS if the shared object already exists
	//	c) E_NO_SHARE if failed to create a shared object

	struct Share*found=find_share(ownerID,shareName);
	if(found!=NULL)
		return E_SHARED_MEM_EXISTS;
	struct Share*newp=alloc_share(ownerID,shareName,size,isWritable);
	if(newp==NULL)
		return E_NO_SHARE;

	uint32 va=(uint32)virtual_address;
	int permission=PERM_USER|PERM_WRITEABLE;
	if(isWritable)
		permission|=PERM_WRITEABLE;
	uint32 numframes=ROUNDUP(size,PAGE_SIZE)/PAGE_SIZE;

	uint32 i;
	for(i=0;i<numframes;i++){
		struct FrameInfo* frame=NULL;
		if(allocate_frame(&frame)!=0||frame==NULL)
			goto removing;

	uint32 curr_va=va+(i*PAGE_SIZE);
	if(map_frame(myenv->env_page_directory,frame,curr_va,permission)!=0 ){
	free_frame(frame);
	goto removing;}
	newp->framesStorage[i]=frame;}

	acquire_kspinlock(&(AllShares.shareslock));
	LIST_INSERT_HEAD(&(AllShares.shares_list),newp);
	release_kspinlock(&(AllShares.shareslock));

	return newp->ID;

	removing:
	for(uint32 j=0;j<i;j++){
		unmap_frame(myenv->env_page_directory,va+j*PAGE_SIZE);
		if(newp->framesStorage[j]!=NULL);
		free_frame(newp->framesStorage[j]);
	}
	if(newp->framesStorage!=NULL)
		kfree(newp->framesStorage);
	kfree(newp);
	return E_NO_SHARE;
}


//======================
// [5] Get Share Object:
//======================
int get_shared_object(int32 ownerID, char* shareName, void* virtual_address)
{
	//TODO: [PROJECT'25.IM#3] SHARED MEMORY - #5 get_shared_object
	//Your code is here
	//Comment the following line


	struct Env* myenv = get_cpu_proc(); //The calling environment

	// 	This function should share the required object in the heap of the current environment
	//	starting from the given virtual_address with the specified permissions of the object: read_only/writable
	// 	and return the ShareObjectID
	// RETURN:
	//	a) ID of the shared object (its VA after masking out its msb) if success
	//	b) E_SHARED_MEM_NOT_EXISTS if the shared object is not exists
	struct Share*found=find_share(ownerID,shareName);
		if(found==NULL)
			return E_SHARED_MEM_NOT_EXISTS;

		uint32 numframes=ROUNDUP(found->size,PAGE_SIZE)/PAGE_SIZE;
		struct FrameInfo**frames=found->framesStorage;
		uint32 va=(uint32)virtual_address;

		for(uint32 i=0;i<numframes;i++){
			struct FrameInfo*frame=frames[i];
			if(frame==NULL){
				for(uint32 j=0;j<i;j++)
					unmap_frame(myenv->env_page_directory,va+j*PAGE_SIZE);
				return E_SHARED_MEM_NOT_EXISTS;
			}
			int permission=PERM_USER|PERM_PRESENT;
			if(found->isWritable)
				permission|=PERM_WRITEABLE;

			if(map_frame(myenv->env_page_directory,frame,va+i*PAGE_SIZE,permission)!=0){
			for(uint32 j=0;j<i;j++)
				unmap_frame(myenv->env_page_directory,va+j*PAGE_SIZE);
			return  E_SHARED_MEM_NOT_EXISTS;
			}


		}
found->references++;
return found->ID;

}

//==================================================================================//
//============================== BONUS FUNCTIONS ===================================//
//==================================================================================//
//=========================
// [1] Delete Share Object:
//=========================
//delete the given shared object from the "shares_list"
//it should free its framesStorage and the share object itself
void free_share(struct Share* ptrShare)
{
	//TODO: [PROJECT'25.BONUS#5] EXIT #2 - free_share
	//Your code is here
	//Comment the following line
	panic("free_share() is not implemented yet...!!");
}


//=========================
// [2] Free Share Object:
//=========================
int delete_shared_object(int32 sharedObjectID, void *startVA)
{
	//TODO: [PROJECT'25.BONUS#5] EXIT #2 - delete_shared_object
	//Your code is here
	//Comment the following line
	panic("delete_shared_object() is not implemented yet...!!");

	struct Env* myenv = get_cpu_proc(); //The calling environment

	// This function should free (delete) the shared object from the User Heapof the current environment
	// If this is the last shared env, then the "frames_store" should be cleared and the shared object should be deleted
	// RETURN:
	//	a) 0 if success
	//	b) E_SHARED_MEM_NOT_EXISTS if the shared object is not exists

	// Steps:
	//	1) Get the shared object from the "shares" array (use get_share_object_ID())
	//	2) Unmap it from the current environment "myenv"
	//	3) If one or more table becomes empty, remove it
	//	4) Update references
	//	5) If this is the last share, delete the share object (use free_share())
	//	6) Flush the cache "tlbflush()"

}
