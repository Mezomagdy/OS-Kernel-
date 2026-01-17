/*
 * fault_handler.c
 *
 *  Created on: Oct 12, 2022
 *      Author: HP
 */

#include "trap.h"
#include <inc/queue.h>
#include <kern/proc/user_environment.h>
#include <kern/cpu/sched.h>
#include <kern/cpu/cpu.h>
#include <kern/disk/pagefile_manager.h>
#include <kern/mem/memory_manager.h>
#include <kern/mem/kheap.h>

//2014 Test Free(): Set it to bypass the PAGE FAULT on an instruction with this length and continue executing the next one
// 0 means don't bypass the PAGE FAULT
uint8 bypassInstrLength = 0;

//===============================
// REPLACEMENT STRATEGIES
//===============================
//2020
void setPageReplacmentAlgorithmLRU(int LRU_TYPE)
{
	assert(LRU_TYPE == PG_REP_LRU_TIME_APPROX || LRU_TYPE == PG_REP_LRU_LISTS_APPROX);
	_PageRepAlgoType = LRU_TYPE ;
}
void setPageReplacmentAlgorithmCLOCK(){_PageRepAlgoType = PG_REP_CLOCK;}
void setPageReplacmentAlgorithmFIFO(){_PageRepAlgoType = PG_REP_FIFO;}
void setPageReplacmentAlgorithmModifiedCLOCK(){_PageRepAlgoType = PG_REP_MODIFIEDCLOCK;}
/*2018*/ void setPageReplacmentAlgorithmDynamicLocal(){_PageRepAlgoType = PG_REP_DYNAMIC_LOCAL;}
/*2021*/ void setPageReplacmentAlgorithmNchanceCLOCK(int PageWSMaxSweeps){_PageRepAlgoType = PG_REP_NchanceCLOCK;  page_WS_max_sweeps = PageWSMaxSweeps;}
/*2024*/ void setFASTNchanceCLOCK(bool fast){ FASTNchanceCLOCK = fast; };
/*2025*/ void setPageReplacmentAlgorithmOPTIMAL(){ _PageRepAlgoType = PG_REP_OPTIMAL; };

//2020
uint32 isPageReplacmentAlgorithmLRU(int LRU_TYPE){return _PageRepAlgoType == LRU_TYPE ? 1 : 0;}
uint32 isPageReplacmentAlgorithmCLOCK(){if(_PageRepAlgoType == PG_REP_CLOCK) return 1; return 0;}
uint32 isPageReplacmentAlgorithmFIFO(){if(_PageRepAlgoType == PG_REP_FIFO) return 1; return 0;}
uint32 isPageReplacmentAlgorithmModifiedCLOCK(){if(_PageRepAlgoType == PG_REP_MODIFIEDCLOCK) return 1; return 0;}
/*2018*/ uint32 isPageReplacmentAlgorithmDynamicLocal(){if(_PageRepAlgoType == PG_REP_DYNAMIC_LOCAL) return 1; return 0;}
/*2021*/ uint32 isPageReplacmentAlgorithmNchanceCLOCK(){if(_PageRepAlgoType == PG_REP_NchanceCLOCK) return 1; return 0;}
/*2021*/ uint32 isPageReplacmentAlgorithmOPTIMAL(){if(_PageRepAlgoType == PG_REP_OPTIMAL) return 1; return 0;}

//===============================
// PAGE BUFFERING
//===============================
void enableModifiedBuffer(uint32 enableIt){_EnableModifiedBuffer = enableIt;}
uint8 isModifiedBufferEnabled(){  return _EnableModifiedBuffer ; }

void enableBuffering(uint32 enableIt){_EnableBuffering = enableIt;}
uint8 isBufferingEnabled(){  return _EnableBuffering ; }

void setModifiedBufferLength(uint32 length) { _ModifiedBufferLength = length;}
uint32 getModifiedBufferLength() { return _ModifiedBufferLength;}

//===============================
// FAULT HANDLERS
//===============================

//==================
// [0] INIT HANDLER:
//==================
void fault_handler_init()
{
	//setPageReplacmentAlgorithmLRU(PG_REP_LRU_TIME_APPROX);
	//setPageReplacmentAlgorithmOPTIMAL();
	setPageReplacmentAlgorithmCLOCK();
	//setPageReplacmentAlgorithmModifiedCLOCK();
	enableBuffering(0);
	enableModifiedBuffer(0) ;
	setModifiedBufferLength(1000);
}
//==================
// [1] MAIN HANDLER:
//==================
/*2022*/
uint32 last_eip = 0;
uint32 before_last_eip = 0;
uint32 last_fault_va = 0;
uint32 before_last_fault_va = 0;
int8 num_repeated_fault  = 0;


struct Env* last_faulted_env = NULL;
void fault_handler(struct Trapframe *tf)
{
	/******************************************************/
	// Read processor's CR2 register to find the faulting address
	uint32 fault_va = rcr2();
	//cprintf("************Faulted VA = %x************\n", fault_va);
	//	print_trapframe(tf);
	/******************************************************/

	//If same fault va for 3 times, then panic
	//UPDATE: 3 FAULTS MUST come from the same environment (or the kernel)
	struct Env* cur_env = get_cpu_proc();
	if (last_fault_va == fault_va && last_faulted_env == cur_env)
	{
		num_repeated_fault++ ;
		if (num_repeated_fault == 3)
		{
			print_trapframe(tf);
			panic("Failed to handle fault! fault @ at va = %x from eip = %x causes va (%x) to be faulted for 3 successive times\n", before_last_fault_va, before_last_eip, fault_va);
		}
	}
	else
	{
		before_last_fault_va = last_fault_va;
		before_last_eip = last_eip;
		num_repeated_fault = 0;
	}
	last_eip = (uint32)tf->tf_eip;
	last_fault_va = fault_va ;
	last_faulted_env = cur_env;
	/******************************************************/
	//2017: Check stack overflow for Kernel
	int userTrap = 0;
	if ((tf->tf_cs & 3) == 3) {
		userTrap = 1;
	}
	if (!userTrap)
	{
		struct cpu* c = mycpu();
		//cprintf("trap from KERNEL\n");
		if (cur_env && fault_va >= (uint32)cur_env->kstack && fault_va < (uint32)cur_env->kstack + PAGE_SIZE)
			panic("User Kernel Stack: overflow exception!");
		else if (fault_va >= (uint32)c->stack && fault_va < (uint32)c->stack + PAGE_SIZE)
			panic("Sched Kernel Stack of CPU #%d: overflow exception!", c - CPUS);
#if USE_KHEAP
		if (fault_va >= KERNEL_HEAP_MAX)
			panic("Kernel: heap overflow exception!");
#endif
	}
	//2017: Check stack underflow for User
	else
	{
		//cprintf("trap from USER\n");
		if (fault_va >= USTACKTOP && fault_va < USER_TOP)
			panic("User: stack underflow exception!");
	}

	//get a pointer to the environment that caused the fault at runtime
	//cprintf("curenv = %x\n", curenv);
	struct Env* faulted_env = cur_env;
	if (faulted_env == NULL)
	{
		cprintf("\nFaulted VA = %x\n", fault_va);
		print_trapframe(tf);
		panic("faulted env == NULL!");
	}
	//check the faulted address, is it a table or not ?
	//If the directory entry of the faulted address is NOT PRESENT then
	if ( (faulted_env->env_page_directory[PDX(fault_va)] & PERM_PRESENT) != PERM_PRESENT)
	{
		faulted_env->tableFaultsCounter ++ ;
		table_fault_handler(faulted_env, fault_va);
	}
	else
	{
		if (userTrap)
		{
			int prm = pt_get_page_permissions(faulted_env->env_page_directory, fault_va);

					if(prm )
					{
						if (PERM_PRESENT)
					 {
						if(!(prm & PERM_WRITEABLE))
						{
							env_exit();
						}

					   }

					 }
					if
					(fault_va >= USER_HEAP_START )
					{
						if ( fault_va < USER_HEAP_MAX)
						{
						if(!(prm & PERM_UHPAGE ))
						  {
							env_exit();
						  }
					     }
					 }
					if(fault_va >= USER_LIMIT)
						{
							env_exit();
						}

/*=====================================================================*/
		}

		/*2022: Check if fault due to Access Rights */
		int perms = pt_get_page_permissions(faulted_env->env_page_directory, fault_va);
		if (perms & PERM_PRESENT)
			panic("Page @va=%x is exist! page fault due to violation of ACCESS RIGHTS\n", fault_va) ;
		/*============================================================================================*/


		// we have normal page fault =============================================================
		faulted_env->pageFaultsCounter ++ ;

//				cprintf("[%08s] user PAGE fault va %08x\n", faulted_env->prog_name, fault_va);
//				cprintf("\nPage working set BEFORE fault handler...\n");
//				env_page_ws_print(faulted_env);
		//int ffb = sys_calculate_free_frames();

		if(isBufferingEnabled())
		{
			__page_fault_handler_with_buffering(faulted_env, fault_va);
		}
		else
		{
			page_fault_handler(faulted_env, fault_va);
		}

		//		cprintf("\nPage working set AFTER fault handler...\n");
		//		env_page_ws_print(faulted_env);
		//		int ffa = sys_calculate_free_frames();
		//		cprintf("fault handling @%x: difference in free frames (after - before = %d)\n", fault_va, ffa - ffb);
	}

	/*************************************************************/
	//Refresh the TLB cache
	tlbflush();
	/*************************************************************/
}


//=========================
// [2] TABLE FAULT HANDLER:
//=========================
void table_fault_handler(struct Env * curenv, uint32 fault_va)
{
	//panic("table_fault_handler() is not implemented yet...!!");
	//Check if it's a stack page
	uint32* ptr_table;
#if USE_KHEAP
	{
		ptr_table = create_page_table(curenv->env_page_directory, (uint32)fault_va);
	}
#else
	{
		__static_cpt(curenv->env_page_directory, (uint32)fault_va, &ptr_table);
	}
#endif
}
void avoid_CH(int x)
{
	}
int x=0 ;


//=========================
// [3] PAGE FAULT HANDLER:
//=========================
/* Calculate the number of page faults according th the OPTIMAL replacement strategy
 * Given:
 * 	1. Initial Working Set List (that the process started with)
 * 	2. Max Working Set Size
 * 	3. Page References List (contains the stream of referenced VAs till the process finished)
 *
 * 	IMPORTANT: This function SHOULD NOT change any of the given lists
 */


 int get_optimal_num_faults_1(struct WS_List *init, int maxx, struct PageRef_List *pageReferences)
{

		LIST_HEAD(init, initWorkingSet) ;

		int opt = 0;
		avoid_CH(x) ;
		    struct WS_List currws;

		    LIST_INIT(&currws);

		    struct WorkingSetElement *oelm;
		    LIST_FOREACH(oelm, init) {
		        struct WorkingSetElement *celem = kmalloc(sizeof(struct WorkingSetElement));
		        if (!celem) {

		            while (!LIST_EMPTY(&currws)) {

		                struct WorkingSetElement *temp = LIST_FIRST(&currws);
		                LIST_REMOVE(&currws, temp);
		                kfree(temp);
		            }
		            return -1;
		        }
		        celem->virtual_address = oelm->virtual_address;

		        LIST_INSERT_TAIL(&currws, celem);
		    }
		    avoid_CH(x) ;
		    struct PageRefElement *current_ref;
		    LIST_FOREACH(current_ref, pageReferences) {
		        uint32 ref_va = ROUNDDOWN(current_ref->virtual_address, PAGE_SIZE);
		        int p_foun = 0;

		        struct WorkingSetElement *ws_elem;
		        LIST_FOREACH(ws_elem, &currws) {
		            if (ROUNDDOWN(ws_elem->virtual_address, PAGE_SIZE) == ref_va) {
		                p_foun = 1;
		                break;
		            }
		        }

		        if (!p_foun) {
		            opt++;

		            if (LIST_SIZE(&currws) < maxx) {
		                struct WorkingSetElement *new_elem = kmalloc(sizeof(struct WorkingSetElement));
		                if (!new_elem) {
		                    while (!LIST_EMPTY(&currws)) {
		                        ws_elem = LIST_FIRST(&currws);
		                        LIST_REMOVE(&currws, ws_elem);
		                        kfree(ws_elem);
		                    }
		                    return -1;
		                }
		                new_elem->virtual_address = ref_va;
		                LIST_INSERT_TAIL(&currws, new_elem);
		            } else {
		                struct WorkingSetElement *vict = NULL;
		                int farthest_future_use = -1;
		                avoid_CH(x) ;
		                LIST_FOREACH(ws_elem, &currws) {
		                    int n_dis = -1;
		                    int dc = 0;
		                    struct PageRefElement *future_ref = current_ref;

		                    while ((future_ref = LIST_NEXT(future_ref)) != NULL) {
		                        dc++;
		                        if (ROUNDDOWN(future_ref->virtual_address, PAGE_SIZE) ==
		                            ROUNDDOWN(ws_elem->virtual_address, PAGE_SIZE)) {
		                            n_dis = dc;
		                            break;
		                        }
		                    }

		                    if (n_dis == -1) {
		                        vict = ws_elem;
		                        break;
		                    } else if (n_dis > farthest_future_use) {
		                        farthest_future_use = n_dis;
		                        vict = ws_elem;
		                    }
		                }
		                avoid_CH(x) ;
		                if (vict == NULL) {
		                    vict = LIST_FIRST(&currws);
		                }

		                if (vict) {
		                    vict->virtual_address = ref_va;
		                }
		            }
		        }
		    }
		    avoid_CH(x) ;
		    while (!LIST_EMPTY(&currws)) {
		        struct WorkingSetElement *elem = LIST_FIRST(&currws);
		        LIST_REMOVE(&currws, elem);
		        kfree(elem);
		    }

		    return opt;

		    avoid_CH(x) ;

}



void optimal(struct Env * faulted_env, uint32 fault_va)
{
	static int one_no_more = 0;

	if (!one_no_more)
	{
		LIST_INIT(&(faulted_env->ActiveList));
		struct WorkingSetElement *or_ws, *c_ws;
		LIST_FOREACH(or_ws, &(faulted_env->page_WS_list))
		{
			avoid_CH(x) ;
			c_ws = (struct WorkingSetElement*) kmalloc(sizeof(struct WorkingSetElement));

			c_ws->virtual_address = or_ws->virtual_address;

			c_ws->empty = or_ws->empty;

			c_ws->time_stamp = or_ws->time_stamp;
			avoid_CH(x) ;

			c_ws->sweeps_counter = or_ws->sweeps_counter;

			LIST_INSERT_TAIL(&(faulted_env->ActiveList), c_ws);
		}
		one_no_more =one_no_more+ 1;
	}

	uint32 p_va = ROUNDDOWN(fault_va, PAGE_SIZE);


	uint32 *pt_d = NULL;
	struct FrameInfo *pa_info = get_frame_info(faulted_env->env_page_directory, p_va, &pt_d);

	if (pa_info == NULL)
	{
		avoid_CH(x) ;

		struct FrameInfo *neFr = NULL;
		if (allocate_frame(&neFr) == E_NO_MEM)
			panic(" No mem");

		map_frame(faulted_env->env_page_directory, neFr, p_va,PERM_USER | PERM_WRITEABLE | PERM_PRESENT);

		int res = pf_read_env_page(faulted_env, (void*)p_va);
		if (res == E_PAGE_NOT_EXIST_IN_PF)
		{
			if (!  ((fault_va >= USTACKBOTTOM) && (fault_va < USTACKTOP))||(((fault_va < USER_HEAP_MAX) && (fault_va >= USER_HEAP_START))))

			{
				avoid_CH(x) ;
				env_exit();
				return;
			}
			memset((void*)p_va, 0, PAGE_SIZE);
		}
	}
	else
	{
		avoid_CH(x) ;
		pt_set_page_permissions(faulted_env->env_page_directory, p_va, PERM_PRESENT, 0);
	}


	struct WorkingSetElement *elmm;
	int founAct = 0;

	LIST_FOREACH(elmm, &(faulted_env->ActiveList))
	{
		if (elmm->virtual_address == p_va)
		{
			founAct = 1;
			break;
		}
	}

	if (!founAct)
	{
		avoid_CH(x) ;

						if (LIST_SIZE(&(faulted_env->ActiveList)) >= faulted_env->page_WS_max_size)
						{
							     LIST_FOREACH(elmm, &(faulted_env->ActiveList))
							{
								pt_set_page_permissions(faulted_env->env_page_directory,
														elmm->virtual_address, 0, PERM_PRESENT);
							}

							  while (!LIST_EMPTY(&(faulted_env->ActiveList)))
							{
								struct WorkingSetElement *elemm = LIST_FIRST(&(faulted_env->ActiveList));
								LIST_REMOVE(&(faulted_env->ActiveList), elemm);
								kfree(elemm);
							}
	}

						avoid_CH(x) ;


		  struct WorkingSetElement *n_elm_s = kmalloc(sizeof(struct WorkingSetElement));


	n_elm_s->virtual_address = p_va;
		n_elm_s->empty = 0;
		 n_elm_s->time_stamp = 0;
		n_elm_s->sweeps_counter = 0;


		LIST_INSERT_TAIL(&(faulted_env->ActiveList), n_elm_s);
	}
	avoid_CH(x) ;
	struct PageRefElement *outt = kmalloc(sizeof(struct PageRefElement));



	outt->virtual_address = p_va;
	LIST_INSERT_TAIL(&(faulted_env->referenceStreamList), outt);

}


void place(struct Env * faulted_env, uint32 fault_va)
{

	 struct FrameInfo *frptr = NULL;
			    uint32 ret = allocate_frame(&frptr);

			    if(ret != E_NO_MEM)
			    {
			        uint32 pva = ROUNDDOWN(fault_va, PAGE_SIZE);


			        map_frame(faulted_env->env_page_directory, frptr, pva, PERM_USER | PERM_WRITEABLE | PERM_PRESENT);


			        uint32 tmp = pf_read_env_page(faulted_env, (void *)pva);

			        if(tmp == E_PAGE_NOT_EXIST_IN_PF)
			        {

			            if(!(((pva >= USER_HEAP_START) && (pva < USER_HEAP_MAX)) ||((pva >= USTACKBOTTOM) && (pva < USTACKTOP))))
			 {
			                env_exit();
			                return;
			                 }
			            }


			        struct WorkingSetElement *new_elem =
			            env_page_ws_list_create_element(faulted_env, pva);


			      if(faulted_env->page_last_WS_element != NULL)
			         {

			                LIST_INSERT_BEFORE(&(faulted_env->page_WS_list), faulted_env->page_last_WS_element,new_elem);
			      }
			          else
			        {

			       LIST_INSERT_TAIL(&(faulted_env->page_WS_list), new_elem);
			        }
			      avoid_CH(x) ;

			           uint32 ns = LIST_SIZE(&(faulted_env->page_WS_list));

			        if(ns == faulted_env->page_WS_max_size)
			        {

			            if(faulted_env->page_last_WS_element == NULL)
			            {

			                faulted_env->page_last_WS_element =LIST_FIRST(&(faulted_env->page_WS_list));
			            }

			        }
			        else if(!(ns!=1) )
			        {
			        	avoid_CH(x) ;
			            faulted_env->page_last_WS_element = new_elem;
			        }

			    }
}

int get_optimal_num_faults(struct WS_List *initWorkingSet, int maxWSSize, struct PageRef_List *pageReferences)
{
	return get_optimal_num_faults_1(initWorkingSet, maxWSSize, pageReferences) ;

}

void clock(struct Env * faulted_env, uint32 fault_va)
{

	avoid_CH(x) ;

			    struct WorkingSetElement *vict = NULL;
			    struct WorkingSetElement *currt = faulted_env->page_last_WS_element;

			    if (currt == NULL)
			        currt = LIST_FIRST(&(faulted_env->page_WS_list));

			    uint32 pa = ROUNDDOWN(fault_va, PAGE_SIZE);
			    while (1)
			    {
			        uint32 vall = currt->virtual_address;
			        uint32 *ptr_pt = NULL;
			        get_page_table(faulted_env->env_page_directory, vall, &ptr_pt);

			        if (ptr_pt != NULL)
			        {
			        	avoid_CH(x) ;
			            uint32 ptee = ptr_pt[PTX(vall)];
			            if (!(ptee & PERM_USED))
			            {
			                vict = currt;
			                break;
			            }
			            else
			            {
			                pt_set_page_permissions(faulted_env->env_page_directory, vall, 0, PERM_USED);
			            }
			        }

			        currt = LIST_NEXT(currt);
			        if (currt == NULL)
			            currt = LIST_FIRST(&(faulted_env->page_WS_list));
			    }
			    avoid_CH(x) ;

			    uint32 vict2 = vict->virtual_address;
			    uint32 *prpt = NULL;
			    get_page_table(faulted_env->env_page_directory, vict2, &prpt);

			    if (prpt != NULL && (prpt[PTX(vict2)] & PERM_MODIFIED))
			    {
			        uint32 *ptte;
			        struct FrameInfo *fi = get_frame_info(faulted_env->env_page_directory, vict2, &ptte);
			        pf_update_env_page(faulted_env, vict2, fi);
			    }

			    unmap_frame(faulted_env->env_page_directory, vict2);

			    struct FrameInfo *new_frame = NULL;
			    if (allocate_frame(&new_frame) == E_NO_MEM)
			        panic(" No mem");

			    map_frame(faulted_env->env_page_directory, new_frame, pa, PERM_USER | PERM_WRITEABLE | PERM_PRESENT);

			        int read_result = pf_read_env_page(faulted_env, (void*)pa);
	  if (read_result == E_PAGE_NOT_EXIST_IN_PF)
			    {
			     if (!(((fault_va < USER_HEAP_MAX) && (fault_va >= USER_HEAP_START)) ||((fault_va >= USTACKBOTTOM) && (fault_va < USTACKTOP))))
			           {
			              env_exit();
			              avoid_CH(x) ;
			            return;
			          }
			   memset((void*)pa, 0, PAGE_SIZE);
			        }


			           vict->virtual_address = pa;
			           avoid_CH(x) ;

faulted_env->page_last_WS_element = LIST_NEXT(vict);
			            if (faulted_env->page_last_WS_element == NULL)
			        faulted_env->page_last_WS_element = LIST_FIRST(&(faulted_env->page_WS_list));
			            avoid_CH(x) ;

}


void page_fault_handler(struct Env * faulted_env, uint32 fault_va)
{
#if USE_KHEAP
	if (isPageReplacmentAlgorithmOPTIMAL())
	{
		optimal(faulted_env, fault_va) ;
	}



	else
	{
		avoid_CH(x) ;
		uint32 sizz = LIST_SIZE(&(faulted_env->page_WS_list));
		if(sizz < (faulted_env->page_WS_max_size))
		{
			place( faulted_env, fault_va) ;
		}
		else
		{
			if (isPageReplacmentAlgorithmCLOCK())
			{
				clock( faulted_env,fault_va) ;
		    }



else if (isPageReplacmentAlgorithmLRU(PG_REP_LRU_TIME_APPROX))
			{
	 uint32 pcs = ROUNDDOWN(fault_va, PAGE_SIZE);

	    if(LIST_SIZE(&(faulted_env->page_WS_list)) < faulted_env->page_WS_max_size)
	    {


	        struct FrameInfo *frptr = NULL;
	        uint32 ret = allocate_frame(&frptr);

	        if(ret != E_NO_MEM)
	        {

	            map_frame(faulted_env->env_page_directory, frptr, pcs,
	                     PERM_USER | PERM_WRITEABLE | PERM_PRESENT);


	            uint32 tppe = pf_read_env_page(faulted_env, (void *)pcs);

	            if(tppe == E_PAGE_NOT_EXIST_IN_PF)
	            {

	                if(!(((pcs >= USER_HEAP_START) && (pcs < USER_HEAP_MAX)) ||
	                     ((pcs >= USTACKBOTTOM) && (pcs < USTACKTOP))))
	                {
	                    env_exit();
	                    return;
	                }

	                memset((void*)pcs, 0, PAGE_SIZE);
	            }


	            struct WorkingSetElement *elm2 =
	                env_page_ws_list_create_element(faulted_env, pcs);


	            elm2->time_stamp = 0x80000000;


	            if(faulted_env->page_last_WS_element != NULL)
	            {
	                LIST_INSERT_BEFORE(&(faulted_env->page_WS_list),
	                                   faulted_env->page_last_WS_element, elm2);
	            }
	            else
	            {
	                LIST_INSERT_TAIL(&(faulted_env->page_WS_list), elm2);
	            }


	            uint32 ns = LIST_SIZE(&(faulted_env->page_WS_list));

	            if(ns == faulted_env->page_WS_max_size)
	            {
	                if(faulted_env->page_last_WS_element == NULL)
	                {
	                    faulted_env->page_last_WS_element =
	                        LIST_FIRST(&(faulted_env->page_WS_list));
	                }
	            }
	            else if(ns == 1)
	            {
	                faulted_env->page_last_WS_element = elm2;
	            }
	        }
	    }
	    else
	    {

	        struct WorkingSetElement *vc = NULL;
	        uint32 mins = 0xFFFFFFFF;

	        struct WorkingSetElement *elem3;
	        LIST_FOREACH(elem3, &(faulted_env->page_WS_list))
	        {
	            if(elem3->time_stamp < mins)
	            {
	            	mins = elem3->time_stamp;
	                vc = elem3;
	            }
	        }


	        if(vc == NULL)
	        {
	            panic("no lru");
	        }


	        uint32 vic33 =vc->virtual_address;


	        uint32 *ptr_pt = NULL;
	        get_page_table(faulted_env->env_page_directory, vic33, &ptr_pt);

	        if(ptr_pt != NULL && (ptr_pt[PTX(vic33)] & PERM_MODIFIED))
	        {

	            uint32 *pt;
	            struct FrameInfo *fi = get_frame_info(faulted_env->env_page_directory,
	            		vic33, &pt);
	            pf_update_env_page(faulted_env, vic33, fi);
	        }


	        unmap_frame(faulted_env->env_page_directory,vic33);

	        struct FrameInfo *nefre = NULL;
	        if(allocate_frame(&nefre) == E_NO_MEM)
	        {
	            panic("no memo");
	            }


	            map_frame(faulted_env->env_page_directory, nefre, pcs, PERM_USER | PERM_WRITEABLE | PERM_PRESENT);


	        int read_result = pf_read_env_page(faulted_env, (void*)pcs);
	        if(read_result == E_PAGE_NOT_EXIST_IN_PF)
	        {

	            if(!(((fault_va < USER_HEAP_MAX) && (fault_va >= USER_HEAP_START)) ||((fault_va >= USTACKBOTTOM) && (fault_va < USTACKTOP))))
	            {
	                   env_exit();
	                return;
	        }
	               memset((void*)pcs, 0, PAGE_SIZE);
	    }


	        vc->virtual_address = pcs;


	        vc->time_stamp = 0x80000000;

	    }
			}
			else if (isPageReplacmentAlgorithmModifiedCLOCK())
			{
				 uint32 pav = ROUNDDOWN(fault_va, PAGE_SIZE);
								    struct WorkingSetElement *victtr = NULL;
								    struct WorkingSetElement *cre = faulted_env->page_last_WS_element;

								    if(cre == NULL)
								        cre = LIST_FIRST(&(faulted_env->page_WS_list));

								    struct WorkingSetElement *stf = cre;


				    while(victtr == NULL)
								    {
								        int found1= 0;

								        do
								        {
								            uint32 va = cre->virtual_address;
								            uint32 *pt = NULL;
								            get_page_table(faulted_env->env_page_directory, va, &pt);

								            if(pt != NULL)
								            {
								                uint32 pte = pt[PTX(va)];
								                int used = (pte & PERM_USED) ? 1 : 0;
								                int modified = (pte & PERM_MODIFIED) ? 1 : 0;

								                if(used == 0 && modified == 0)
								                {
								                    victtr = cre;
								                    found1 = 1;
								                    break;
								                }
							    }

								            cre = LIST_NEXT(cre);
								            if(cre == NULL)
								                cre = LIST_FIRST(&(faulted_env->page_WS_list));

								 } while(cre != stf);


								        if(!found1)
								        {
								            struct WorkingSetElement *str2 = cre;

								            do
								 {
								                uint32 va = cre->virtual_address;
								                uint32 *pt33 = NULL;
								                get_page_table(faulted_env->env_page_directory, va, &pt33);

								                if(pt33 != NULL)
								                {
								                    uint32 pte = pt33[PTX(va)];
								                    int sd = (pte & PERM_USED) ? 1 : 0;

								                    if(sd == 0)
								                    {    victtr = cre;
								                        break;
								                         }
								                    else
								              {
								                        pt_set_page_permissions(faulted_env->env_page_directory,
								                                              va, 0, PERM_USED);
								                                }
								    }

								                cre = LIST_NEXT(cre);
								                if(cre == NULL)
								                    cre = LIST_FIRST(&(faulted_env->page_WS_list));

								               } while(cre != str2);


								            stf = cre;
								        }
				  }



								    uint32 vict5 = victtr->virtual_address;
								    uint32 *ptr_pt = NULL;
								    get_page_table(faulted_env->env_page_directory, vict5, &ptr_pt);

								    if(ptr_pt != NULL && (ptr_pt[PTX(vict5)] & PERM_MODIFIED))
						 {
								        uint32 *pt;
								        struct FrameInfo *fi = get_frame_info(faulted_env->env_page_directory,
								        		vict5, &pt);
								        pf_update_env_page(faulted_env, vict5, fi);
								    }

								    unmap_frame(faulted_env->env_page_directory,vict5);

								    struct FrameInfo *new_frame = NULL;
								        if(allocate_frame(&new_frame) == E_NO_MEM)
								    {
								        panic("no mem");
								       }

								    map_frame(faulted_env->env_page_directory, new_frame, pav, PERM_USER | PERM_WRITEABLE | PERM_PRESENT);

								    int rdres = pf_read_env_page(faulted_env, (void*)pav);
								    if(rdres == E_PAGE_NOT_EXIST_IN_PF)
								    {
								        if(!(((fault_va < USER_HEAP_MAX) && (fault_va >= USER_HEAP_START)) ||((fault_va >= USTACKBOTTOM) && (fault_va < USTACKTOP))))
								        {
								                env_exit();
								            return;}
								        memset((void*)pav, 0, PAGE_SIZE);
						}

								    victtr->virtual_address = pav;

								    faulted_env->page_last_WS_element = LIST_NEXT(victtr);
								    if(faulted_env->page_last_WS_element == NULL)   faulted_env->page_last_WS_element = LIST_FIRST(&(faulted_env->page_WS_list));
			}
		}
	}
#endif
}

void __page_fault_handler_with_buffering(struct Env * curenv, uint32 fault_va)
{
	panic("this function is not required...!!");
}



