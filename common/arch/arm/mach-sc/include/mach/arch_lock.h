/*
 * Copyright (C) 2012 Spreadtrum Communications Inc.
 * Author: steve.zhan <steve.zhan@spreadtrum.com>
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#ifndef __ARCH_LOCK_H
#define __ARCH_LOCK_H

#include <linux/hwspinlock.h>
#include <linux/io.h>

#include <mach/hardware.h>
#include <mach/sci.h>
#include <mach/sci_glb_regs.h>

extern struct hwspinlock *hwlocks[];
extern unsigned char __initdata hwlocks_implemented[];
extern unsigned int hwspinlock_vid;
extern unsigned char local_hwlocks_status[];

#define	arch_get_hwlock(_ID_)	(hwlocks[_ID_])
#define FILL_HWLOCKS(_X_)	do {hwlocks_implemented[(_X_)] = 1;} while(0)

#ifdef CONFIG_SEC_DEBUG_REG_ACCESS
extern unsigned char *sec_debug_local_hwlocks_status;
#define RECORD_HWLOCKS_STATUS_LOCK(_X_)	do {local_hwlocks_status[(_X_)] = 1;\
											if(sec_debug_local_hwlocks_status)\
												sec_debug_local_hwlocks_status[(_X_)] = 1;} while(0)

#define RECORD_HWLOCKS_STATUS_UNLOCK(_X_)	do {local_hwlocks_status[(_X_)] = 0;\
												if(sec_debug_local_hwlocks_status)\
													sec_debug_local_hwlocks_status[(_X_)] = 0;} while(0)
#else
#define RECORD_HWLOCKS_STATUS_LOCK(_X_)	do {local_hwlocks_status[(_X_)] = 1;} while(0)
#define RECORD_HWLOCKS_STATUS_UNLOCK(_X_)	do {local_hwlocks_status[(_X_)] = 0;} while(0)
#endif

#if	defined (CONFIG_ARCH_SC8825)
static __inline __init int __hwspinlock_init(void)
{
	hwspinlock_vid = 0;
	__raw_writel(BIT_SPINLOCK_EB, REG_GLB_SET(REG_AHB_AHB_CTL0));
	return 0;
}

#else
static __inline __init int __hwspinlock_init(void)
{
	hwspinlock_vid = __raw_readl((void *)(SPRD_HWLOCK0_BASE + 0xffc));
	__raw_writel(BIT_SPINLOCK_EB, (void *)REG_GLB_SET(REG_AP_AHB_AHB_EB));
	__raw_writel(BIT_SPLK_EB, (void *)REG_GLB_SET(REG_AON_APB_APB_EB0));
	return 0;
}
#endif


//Configs lock id
#define HWSPINLOCK_WRITE_KEY	(0x1)	/*processor specific write lock id */

#define HWSPINLOCK_NOTTAKEN_V0		(0x524c534c)	/*free: RLSL */
#define HWSPINLOCK_NOTTAKEN_V1		(0x55aa10c5)	/*free: RLSL */

#define HWSPINLOCK_ID_TOTAL_NUMS	(64)
#define HWLOCK_ADI	(0)
#define HWLOCK_GLB	(1)
#define HWLOCK_AGPIO	(2)
#define HWLOCK_AEIC	(3)
#define HWLOCK_ADC	(4)
#define HWLOCK_EFUSE	(8)

static inline void arch_hwlocks_implemented(void)
{
	FILL_HWLOCKS(HWLOCK_ADI);
	FILL_HWLOCKS(HWLOCK_GLB);
	FILL_HWLOCKS(HWLOCK_AGPIO);
	FILL_HWLOCKS(HWLOCK_AEIC);
	FILL_HWLOCKS(HWLOCK_AGPIO);
	FILL_HWLOCKS(HWLOCK_ADC);
	FILL_HWLOCKS(HWLOCK_EFUSE);
}

#ifndef SPRD_HWLOCK1_BASE
#define SPRD_HWLOCK1_BASE SPRD_HWLOCK0_BASE
#endif
static inline unsigned long HWLOCK_ADDR(unsigned int id)
{
	if (hwspinlock_vid == 0x100) {
		BUG_ON(id > 63);
		if (id < 31)
			return SPRD_HWLOCK1_BASE + 0x800 + 0x4*id;
		else
			return SPRD_HWLOCK0_BASE + 0x800 + 0x4*id;
	} else if (hwspinlock_vid == 0x0) {
		return SPRD_HWLOCK0_BASE + 0x80 + 0x4*id;
	} else {
		printk(KERN_ERR "hwspinlock module version is wrong!");
		return -1;
	}
}

static inline int arch_hwlock_fast_trylock(unsigned int lock_id)
{
	unsigned long addr;

	__hwspinlock_init();
	addr = HWLOCK_ADDR(lock_id);

	if (hwspinlock_vid == 0x100) {
		if (!readl((void *)addr))
			goto __locked;
	} else if (hwspinlock_vid == 0) {
		if (HWSPINLOCK_NOTTAKEN_V0 == __raw_readl((void *)addr)) {
			__raw_writel(HWSPINLOCK_WRITE_KEY, (void *)addr);
			if (HWSPINLOCK_WRITE_KEY == __raw_readl((void *)addr)) {
				goto __locked;
			}
		}
	}
	return 0;

__locked:
	dsb();
	RECORD_HWLOCKS_STATUS_LOCK(lock_id);
	return 1;
}

static inline void arch_hwlock_fast_unlock(unsigned int lock_id)
{
	unsigned long addr = HWLOCK_ADDR(lock_id);
	dsb();
	RECORD_HWLOCKS_STATUS_UNLOCK(lock_id);

	if (hwspinlock_vid == 0x100)
		__raw_writel(HWSPINLOCK_NOTTAKEN_V1, (void *)addr);
	else
		__raw_writel(HWSPINLOCK_NOTTAKEN_V0, (void *)addr);
}

#define arch_hwlock_fast(_LOCK_ID_) do { \
	while (!arch_hwlock_fast_trylock(_LOCK_ID_)) \
	cpu_relax();} while (0)

#define arch_hwunlock_fast(_LOCK_ID_) do { \
				arch_hwlock_fast_unlock(_LOCK_ID_);} while (0)


static inline void __arch_default_lock(unsigned int lock_id, unsigned long *flags)
{
	if (arch_get_hwlock(lock_id))
		WARN_ON(IS_ERR_VALUE
			(hwspin_lock_timeout_irqsave
			 (arch_get_hwlock(lock_id), -1, flags)));
	else
		arch_hwlock_fast(lock_id);
}

static inline void __arch_default_unlock(unsigned int lock_id, unsigned long *flags)
{
	if (arch_get_hwlock(lock_id))
		hwspin_unlock_irqrestore(arch_get_hwlock(lock_id), flags);
	else
		arch_hwunlock_fast(lock_id);
}

#endif
