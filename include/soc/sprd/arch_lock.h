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

#include <soc/sprd/sci.h>
#include <soc/sprd/sci_glb_regs.h>

#define	arch_get_hwlock(_ID_)		(hwlocks[_ID_])

/* processor specific write lock id */
#define HWSPINLOCK_WRITE_KEY		(0x1)

/* hwspinlock id */
#define HWLOCK_ADI					(0)
#define HWLOCK_GLB					(1)
#define HWLOCK_AGPIO				(2)
#define HWLOCK_AEIC					(3)
#define HWLOCK_ADC					(4)
#define HWLOCK_EFUSE				(8)

/* untoken lock value */
#define HWSPINLOCK_NOTTAKEN_V1		(0x55aa10c5)
#define HWSPINLOCK_NOTTAKEN_V0		(0x524c534c)

/* hwspinlock number */
#ifdef	CONFIG_ARCH_WHALE
	#define HWSPINLOCK_ID_TOTAL_NUMS	(32)
#else
	#define HWSPINLOCK_ID_TOTAL_NUMS	(64)
#endif

#define arch_hwlock_fast(_LOCK_ID_) do { \
	while (!arch_hwlock_fast_trylock(_LOCK_ID_)) \
	cpu_relax();} while (0)

#define arch_hwunlock_fast(_LOCK_ID_) do { \
	arch_hwlock_fast_unlock(_LOCK_ID_);} while (0)

extern int hwspinlock_vid_early;
extern struct hwspinlock *hwlocks[];
int early_hwspinlocks_init(void);
int sprd_hwspinlock_isbusy(unsigned int lock_id);
int sprd_hwspinlocks_isbusy(void);
int sprd_hwspinlock_master_id(unsigned int lock_id);
int sprd_record_hwlock_sts(unsigned int lock_id, unsigned int sts);

static void early_fast_hwlock_init(unsigned int lock_id)
{
	static int early_hwlock_init = 0;

	if (early_hwlock_init == 0){
		writel_relaxed(BIT_SPINLOCK_EB, (void __iomem *)(REG_GLB_SET(REG_AP_AHB_AHB_EB)));
		writel_relaxed(BIT_SPLK_EB, (void __iomem *)(REG_GLB_SET(REG_AON_APB_APB_EB0)));
	}

	if (lock_id < 31)
		hwspinlock_vid_early = readl_relaxed((void *)(SPRD_HWSPINLOCK1_BASE + 0xFFC));
	else if(lock_id < 63)
		hwspinlock_vid_early = readl_relaxed((void *)(SPRD_HWSPINLOCK0_BASE + 0xFFC));
	else
		hwspinlock_vid_early = 0;

	early_hwlock_init = 1;
}

static unsigned long early_hwlock_addr(unsigned int id)
{
	unsigned long addr = 0;

	if (hwspinlock_vid_early == 0x100 || hwspinlock_vid_early == 0x200
		|| hwspinlock_vid_early == 0x300) {
		if (id < 31)
			addr = SPRD_HWSPINLOCK1_BASE + 0x800 + 0x4*id;
		else if(id < 63)
			addr = SPRD_HWSPINLOCK0_BASE + 0x800 + 0x4*id;
		else
			printk(KERN_ERR "hwspinlock id [%d] is wrong!\n",id);
	} else if (hwspinlock_vid_early == 0x0) {
		addr = SPRD_HWSPINLOCK0_BASE + 0x80 + 0x4*id;
	} else
		printk(KERN_ERR "hwspinlock module version [0x%x] is wrong!\n",hwspinlock_vid_early);

	return addr;
}

static int arch_hwlock_fast_trylock(unsigned int lock_id)
{
	void __iomem *addr;

	early_fast_hwlock_init(lock_id);
	addr = (void __iomem *)early_hwlock_addr(lock_id);
	if (!addr){
		printk(KERN_ERR "Hwspinlock [%d] lock failed,get wrong addr!\n",lock_id);
		return -ENXIO;
	}

	if (hwspinlock_vid_early == 0x100 || hwspinlock_vid_early == 0x200
		|| hwspinlock_vid_early == 0x300) {
		if (!readl_relaxed(addr))
			goto __locked;
	} else if (hwspinlock_vid_early == 0) {
		if (HWSPINLOCK_NOTTAKEN_V0 == __raw_readl(addr)) {
			writel_relaxed(HWSPINLOCK_WRITE_KEY, addr);
			if (HWSPINLOCK_WRITE_KEY == readl_relaxed(addr)) {
				goto __locked;
			}
		}
	}

	printk(KERN_ERR "Hwspinlock [%d] lock failed!\n",lock_id);
	return 0;

__locked:
#ifdef CONFIG_64BIT
	dsb(sy);
#else
	dsb();
#endif
	sprd_record_hwlock_sts(lock_id, 1);
	return 1;
}

static void arch_hwlock_fast_unlock(unsigned int lock_id)
{
	void __iomem *addr = (void __iomem *)early_hwlock_addr(lock_id);
#ifdef CONFIG_64BIT
	dsb(sy);
#else
	dsb();
#endif

	if (hwspinlock_vid_early == 0x100 || hwspinlock_vid_early == 0x200
		|| hwspinlock_vid_early == 0x300)
		writel_relaxed(HWSPINLOCK_NOTTAKEN_V1, addr);
	else
		writel_relaxed(HWSPINLOCK_NOTTAKEN_V0, addr);

	sprd_record_hwlock_sts(lock_id, 0);
}

static void __arch_default_lock(unsigned int lock_id, unsigned long *flags)
{
#ifdef CONFIG_SC_FPGA
/* for the hwspinlock not in the bitfile */
#else
	if (hwlocks[lock_id])
		BUG_ON(IS_ERR_VALUE
			(hwspin_lock_timeout_irqsave(hwlocks[lock_id], 3000, flags)));
	else
		arch_hwlock_fast(lock_id);
#endif
}

static void __arch_default_unlock(unsigned int lock_id, unsigned long *flags)
{
#ifdef CONFIG_SC_FPGA
/* for the hwspinlock not in the bitfile */
#else
	if (hwlocks[lock_id])
		hwspin_unlock_irqrestore(hwlocks[lock_id], flags);
	else
		arch_hwunlock_fast(lock_id);
#endif
}

#endif
