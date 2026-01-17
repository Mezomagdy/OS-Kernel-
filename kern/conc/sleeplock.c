// Sleeping locks

#include "inc/types.h"
#include "inc/x86.h"
#include "inc/memlayout.h"
#include "inc/mmu.h"
#include "inc/environment_definitions.h"
#include "inc/assert.h"
#include "inc/string.h"
#include "sleeplock.h"
#include "channel.h"
#include "../cpu/cpu.h"
#include "../proc/user_environment.h"

void init_sleeplock(struct sleeplock *lk, char *name)
{
	init_channel(&(lk->chan), "sleep lock channel");
	char prefix[30] = "lock of sleeplock - ";
	char guardName[30+NAMELEN];
	strcconcat(prefix, name, guardName);
	init_kspinlock(&(lk->lk), guardName);
	strcpy(lk->name, name);
	lk->locked = 0;
	lk->pid = 0;
}

void acquire_sleeplock(struct sleeplock *slk)
{
	//TODO: [PROJECT'25.IM#5] KERNEL PROTECTION: #4 SLEEP LOCK - acquire_sleeplock
	//Your code is here
	acquire_kspinlock(&(slk->lk));
	while (slk->locked) {
		sleep(&(slk->chan), &(slk->lk));
	}
	slk->locked = 1;
	struct Env* cur_env = get_cpu_proc();
	slk->pid = cur_env->env_id;
	release_kspinlock(&(slk->lk));
	//Comment the following line
	//panic("acquire_sleeplock() is not implemented yet...!!");
}

void release_sleeplock(struct sleeplock *slk)
{
	//TODO: [PROJECT'25.IM#5] KERNEL PROTECTION: #5 SLEEP LOCK - release_sleeplock
	//Your code is here
	acquire_kspinlock(&(slk->lk));
	//check the second condition
	if (!slk->locked || slk->pid != get_cpu_proc()->env_id) {
		panic("release_sleeplock: not holding lock!");
	}
	wakeup_all(&(slk->chan));
	slk->locked = 0;
	slk->pid = 0;
	release_kspinlock(&(slk->lk));
	//Comment the following line
	//panic("release_sleeplock() is not implemented yet...!!");
}

int holding_sleeplock(struct sleeplock *lk)
{
	int r;
	acquire_kspinlock(&(lk->lk));
	r = lk->locked && (lk->pid == get_cpu_proc()->env_id);
	release_kspinlock(&(lk->lk));
	return r;
}



