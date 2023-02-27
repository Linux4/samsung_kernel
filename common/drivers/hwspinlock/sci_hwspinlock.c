/*
 * SCI hardware spinlock driver
 *
 * Copyright (C) 2012 Spreadtrum  - http://www.spreadtrum.com
 * Copyright (C) 2010 Texas Instruments Incorporated - http://www.ti.com
 *
 * Contact: steve.zhan <steve.zhan@spreadtrum.com>
 *        
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 */

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/device.h>
#include <linux/delay.h>
#include <linux/io.h>
#include <linux/bitops.h>
#include <linux/pm_runtime.h>
#include <linux/slab.h>
#include <linux/spinlock.h>
#include <linux/hwspinlock.h>
#include <linux/platform_device.h>
#include <linux/of.h>
#include <linux/of_device.h>

#include <mach/sci.h>
#include <mach/hardware.h>
#include <mach/arch_lock.h>

#include "hwspinlock_internal.h"


#define HWSPINLOCK_ENABLE_CLEAR		(0x454e434c)

#define HWSPINLOCK_CLEAR		(0x0)
#define HWSPINLOCK_TTLSTS		(0x4)
#define HWSPINLOCK_DTLSTL		(0x8)
#define HWSPINLOCK_CLEAREN		(0xc)

#define HWSPINLOCK_TOKEN_V0(_X_)	(0x80 + 0x4*(_X_))
#define HWSPINLOCK_TOKEN_V1(_X_)	(0x800 + 0x4*(_X_))

#define THIS_PROCESSOR_KEY	(HWSPINLOCK_WRITE_KEY)	/*first processor */
static void __iomem *hwspinlock_base;
static int hwspinlock_exist_cnt = 0;

#define SPINLOCKS_BUSY()	(readl(hwspinlock_base + HWSPINLOCK_TTLSTS))
#define SPINLOCKS_DETAIL_STATUS(_X_)	(_X_ = readl(hwspinlock_base + HWSPINLOCK_DTLSTL))
#define SPINLOCKS_ENABLE_CLEAR()	writel(HWSPINLOCK_ENABLE_CLEAR,hwspinlock_base + HWSPINLOCK_CLEAREN)
#define SPINLOCKS_DISABLE_CLEAR()	writel(~HWSPINLOCK_ENABLE_CLEAR, hwspinlock_base + HWSPINLOCK_CLEAREN)

__used static int hwspinlock_isbusy(unsigned int lockid)
{
	unsigned int status = 0;
	SPINLOCKS_DETAIL_STATUS(status);
	return ((status & (1 << lockid)) ? 1 : 0);
}

__used static int hwspinlocks_isbusy(void)
{
	return ((SPINLOCKS_BUSY())? 1 : 0);
}

__used static void hwspinlock_clear(unsigned int lockid)
{
	/*setting the abnormal clear bit to 1 makes the corresponding
	 *lock to Not Taken state
	 */
	SPINLOCKS_ENABLE_CLEAR();
	writel(1 << lockid, hwspinlock_base + HWSPINLOCK_CLEAR);
	SPINLOCKS_DISABLE_CLEAR();
}

__used static void hwspinlock_clear_all(void)
{
	/*clear all the locks
	 */
	unsigned int lockid = 0;
	SPINLOCKS_ENABLE_CLEAR();
	do {
		writel(1 << lockid, hwspinlock_base + HWSPINLOCK_CLEAR);
	} while (lockid++ < HWSPINLOCK_ID_TOTAL_NUMS);
	SPINLOCKS_DISABLE_CLEAR();
}

static unsigned int do_lock_key(struct hwspinlock *lock)
{
	unsigned int key = THIS_PROCESSOR_KEY;
	return key;
}

static int __hwspinlock_trylock(struct hwspinlock *lock)
{
	void __iomem *addr = lock->priv;

	if (hwspinlock_vid == 0x100) {
		if (!readl(addr))
			goto __locked;
	} else {
		unsigned int key = do_lock_key(lock);
		if (!(key ^ HWSPINLOCK_NOTTAKEN_V0))
			BUG_ON(1);

		if (HWSPINLOCK_NOTTAKEN_V0 == readl(addr)) {
			writel(key, addr);
			if (key == readl(addr))
				goto __locked;
		}
	}
	return 0;

__locked:
	RECORD_HWLOCKS_STATUS_LOCK(hwlock_to_id(lock));
	return 1;
}

static void __hwspinlock_unlock(struct hwspinlock *lock)
{
	void __iomem *lock_addr = lock->priv;
	int unlock_key = 0;

	if (hwspinlock_vid == 0x100) {
		unlock_key = HWSPINLOCK_NOTTAKEN_V1;
	} else {
		unlock_key = HWSPINLOCK_NOTTAKEN_V0;
		if (!(readl(lock_addr) ^ unlock_key))
			BUG_ON(1);
	}
	RECORD_HWLOCKS_STATUS_UNLOCK(hwlock_to_id(lock));
	writel(unlock_key, lock_addr);
}

/*
 * relax the  interconnect while spinning on it.
 *
 * The specs recommended that the retry delay time will be
 * just over half of the time that a requester would be
 * expected to hold the lock.
 *
 * The number below is taken from an hardware specs example,
 * obviously it is somewhat arbitrary.
 */
static void __hwspinlock_relax(struct hwspinlock *lock)
{
	ndelay(10);
}

static const struct hwspinlock_ops sci_hwspinlock_ops = {
	.trylock = __hwspinlock_trylock,
	.unlock = __hwspinlock_unlock,
	.relax = __hwspinlock_relax,
};

static int sci_hwspinlock_probe(struct platform_device *pdev)
{
	struct hwspinlock_device *bank;
	struct hwspinlock *hwlock;
	struct resource *res;
	int i, ret, num_locks;

	__hwspinlock_init();

	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if (!res || (!res->start))
		return -ENODEV;

	/*each group have 32 locks*/
	num_locks = 32;
	hwspinlock_base = (void __iomem *)res->start;
	hwspinlock_vid = __raw_readl(hwspinlock_base + 0xffc);

	bank = kzalloc(sizeof(*bank) + num_locks * sizeof(*hwlock), GFP_KERNEL);
	if (!bank) {
		ret = -ENOMEM;
		goto exit;
	}

	platform_set_drvdata(pdev, bank);

	for (i = 0, hwlock = &bank->lock[0]; i < num_locks; i++, hwlock++) {
		if (hwspinlock_vid == 0x100)
			hwlock->priv =
			    (void __iomem *)(res->start +
					     HWSPINLOCK_TOKEN_V1(i));
		else
			hwlock->priv =
			    (void __iomem *)(res->start +
					     HWSPINLOCK_TOKEN_V0(i));
	}
	/*
	 * runtime PM will make sure the clock of this module is
	 * enabled if at least one lock is requested
	 */
	pm_runtime_enable(&pdev->dev);

	ret = hwspin_lock_register(bank, &pdev->dev, &sci_hwspinlock_ops,
				   hwspinlock_exist_cnt, num_locks);

	if (ret)
		goto reg_fail;

	hwspinlock_exist_cnt += num_locks;

	printk("sci_hwspinlock_probe ok: hwspinlock_vid = 0x%x, add num_locks = %d\n",
	       hwspinlock_vid,num_locks);

	return 0;

reg_fail:
	pm_runtime_disable(&pdev->dev);
	kfree(bank);
exit:
	return ret;
}

static int sci_hwspinlock_remove(struct platform_device *pdev)
{
	struct hwspinlock_device *bank = platform_get_drvdata(pdev);
	int ret;

	ret = hwspin_lock_unregister(bank);
	if (ret) {
		dev_err(&pdev->dev, "%s failed: %d\n", __func__, ret);
		return ret;
	}

	pm_runtime_disable(&pdev->dev);
	kfree(bank);

	return 0;
}

static const struct of_device_id sprd_hwspinlock_of_match[] = {
	{ .compatible = "sprd,sprd-hwspinlock", },
	{ /* sentinel */ }
};

static struct platform_driver sci_hwspinlock_driver = {
	.probe = sci_hwspinlock_probe,
	.remove = sci_hwspinlock_remove,
	.driver = {
		.name = "sci_hwspinlock",
		.owner = THIS_MODULE,
		.of_match_table = of_match_ptr(sprd_hwspinlock_of_match),
	},
};

static int __init sci_hwspinlock_init(void)
{
	return platform_driver_register(&sci_hwspinlock_driver);
}

/* board init code might need to reserve hwspinlocks for predefined purposes */
postcore_initcall(sci_hwspinlock_init);

static void __exit sci_hwspinlock_exit(void)
{
	platform_driver_unregister(&sci_hwspinlock_driver);
}

module_exit(sci_hwspinlock_exit);

MODULE_LICENSE("GPL v2");
MODULE_DESCRIPTION("Hardware spinlock driver for Spreadtrum");
MODULE_AUTHOR("steve.zhan <steve.zhan@spreadtrum.com>");
