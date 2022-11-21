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

#include <soc/sprd/sci.h>
#include <soc/sprd/sci_glb_regs.h>
#include <soc/sprd/arch_lock.h>
#include "hwspinlock_internal.h"

/* hwspinlock reg */
#define HWSPINLOCK_CLEAR			(0x0)
#define HWSPINLOCK_RECCTRL			(0x4)
#define HWSPINLOCK_TTLSTS			(0x8)
#define HWSPINLOCK_CLEAREN			(0xc)
#define HWSPINLOCK_FLAG0			(0x10)
#define HWSPINLOCK_FLAG1			(0x14)
#define HWSPINLOCK_FLAG2			(0x18)
#define HWSPINLOCK_FLAG3			(0x1c)
#define HWSPINLOCK_MASTERID(_X_)	(0x80 + 0x4*(_X_))
#define HWSPINLOCK_TOKEN_V0(_X_)	(0x80 + 0x4*(_X_))
#define HWSPINLOCK_TOKEN_V1(_X_)	(0x800 + 0x4*(_X_))
#define HWSPINLOCK_VERID			(0xFFC)

/* RECCTRL */
#define	HWSPINLOCK_ID				BIT(0)
#define	HWSPINLOCK_USER_BITS		BIT(1)

/* first processor */
#define THIS_PROCESSOR_KEY			(HWSPINLOCK_WRITE_KEY)

/* spinlock enable */
#define HWSPINLOCK_ENABLE_CLEAR		(0x454e434c)

/* hwspinlock controller */
#define	AON_HWSPLOCK				(1)
#define	AP_HWSPLOCK					(0)

struct sprd_hwspinlock_dev {
	unsigned int hwspinlock_vid;
	unsigned long hwspinlock_base;
	unsigned int hwspinlock_cnt;
	struct hwspinlock_device bank;
};

/* array[0] stands for lock0 */
unsigned char hwlocks_implemented[HWSPINLOCK_ID_TOTAL_NUMS];
unsigned char local_hwlocks_status[HWSPINLOCK_ID_TOTAL_NUMS];
struct hwspinlock *hwlocks[HWSPINLOCK_ID_TOTAL_NUMS];
int hwspinlock_vid_early;

static struct sprd_hwspinlock_dev *hwlock_to_sprdlock_dev(struct hwspinlock *hwlock)
{
	struct hwspinlock_device *hwbank;
	int lock_id;
	
	if(!hwlock)
		return NULL;

	lock_id = hwlock_to_id(hwlock);
	hwbank = container_of(hwlock,struct hwspinlock_device,lock[lock_id]);
	if(hwbank)
		return container_of(hwbank,struct sprd_hwspinlock_dev,bank);
	else
		return NULL;
}

#ifdef	CONFIG_ARCH_WHALE
int sprd_hwlock_init(void)
{
	return 0;
}
#else
int sprd_hwlock_init(void)
{
	writel_relaxed(BIT_SPINLOCK_EB, (void __iomem *)(REG_GLB_SET(REG_AP_AHB_AHB_EB)));
	writel_relaxed(BIT_SPLK_EB, (void __iomem *)(REG_GLB_SET(REG_AON_APB_APB_EB0)));
	return 0;
}
#endif

static void inline sprd_hwlocks_implemented(int hwlock_id)
{
	if (hwlock_id == AON_HWSPLOCK){
		hwlocks_implemented[HWLOCK_ADI] = 1;
		hwlocks_implemented[HWLOCK_GLB] = 1;
		hwlocks_implemented[HWLOCK_AGPIO] = 1;
		hwlocks_implemented[HWLOCK_AEIC] = 1;
		hwlocks_implemented[HWLOCK_ADC] = 1;
		hwlocks_implemented[HWLOCK_EFUSE] = 1;
	}else if (hwlock_id == AP_HWSPLOCK){
		/* Caution: ap hwspinlock id need add 31 */
		/* hwlocks_implemented[HWLOCK_ADI+31] = 1; */
	}
}

int sprd_record_hwlock_sts(unsigned int lock_id, unsigned int sts)
{
	if(lock_id >= HWSPINLOCK_ID_TOTAL_NUMS){
		printk(KERN_ERR "Hwspinlock id is wrong!\n");
		return -ENXIO;
	}

	local_hwlocks_status[lock_id] = sts;
	return sts;
}

int sprd_hwspinlock_isbusy(unsigned int lock_id)
{
	unsigned int status = 0;
	struct sprd_hwspinlock_dev *sprd_hwlock;

	if (!hwlocks[lock_id]){
		printk(KERN_ERR "This hwspinlock is not register!\n");
		return -ENODEV;
	}

	sprd_hwlock = hwlock_to_sprdlock_dev(hwlocks[lock_id]);
	if (!sprd_hwlock){
		printk(KERN_ERR "Can't find hwspinlock device!\n");
		return -ENODEV;
	}

	status = readl_relaxed((void __iomem *)(sprd_hwlock->hwspinlock_base + HWSPINLOCK_TTLSTS));
	if (status < 0)
		return -EIO;
	return ((status & (1 << lock_id)) ? 1 : 0);
}

int sprd_hwspinlocks_isbusy(void)
{
	int id;

	for(id = 0; id < HWSPINLOCK_ID_TOTAL_NUMS; id++){
		if(sprd_hwspinlock_isbusy(id) > 0)
			return 1;
	}
	return 0;
}

int sprd_hwspinlock_master_id(unsigned int lock_id)
{
	unsigned int master_id = 0;
	struct sprd_hwspinlock_dev *sprd_hwlock;

	if(!hwlocks[lock_id]){
		printk(KERN_ERR "This hwspinlock is not register!\n");
		return -ENODEV;
	}

	sprd_hwlock = hwlock_to_sprdlock_dev(hwlocks[lock_id]);
	if (!sprd_hwlock){
		printk(KERN_ERR "Can't find hwspinlock device!\n");
		return -ENODEV;
	}
	
	master_id = readl_relaxed((void __iomem *)(sprd_hwlock->hwspinlock_base + HWSPINLOCK_MASTERID(lock_id)));
	return master_id;
}

void hwspinlock_clear(unsigned int lock_id)
{
	struct sprd_hwspinlock_dev *sprd_hwlock;

	if(!hwlocks[lock_id]){
		printk(KERN_ERR "This hwspinlock is not register!\n");
		return;
	}

	sprd_hwlock = hwlock_to_sprdlock_dev(hwlocks[lock_id]);
	if (!sprd_hwlock){
		printk(KERN_ERR "Can't find hwspinlock device!\n");
		return;
	}
	/*setting the abnormal clear bit to 1 makes the corresponding
	 *lock to Not Taken state
	 */
	writel_relaxed(HWSPINLOCK_ENABLE_CLEAR,(void __iomem *)(sprd_hwlock->hwspinlock_base + HWSPINLOCK_CLEAREN));
	writel_relaxed((1 << lock_id), (void __iomem *)(sprd_hwlock->hwspinlock_base + HWSPINLOCK_CLEAR));
	writel_relaxed(~HWSPINLOCK_ENABLE_CLEAR, (void __iomem *)(sprd_hwlock->hwspinlock_base + HWSPINLOCK_CLEAREN));
}

void hwspinlock_clear_all(void)
{
	unsigned int lock_id = 0;
	do {
		hwspinlock_clear(lock_id);
	} while (lock_id++ < HWSPINLOCK_ID_TOTAL_NUMS);
}

static int sprd_check_hwspinlock_vid(struct hwspinlock *lock)
{
	struct sprd_hwspinlock_dev *sprd_hwlock;

	sprd_hwlock = hwlock_to_sprdlock_dev(lock);
	if (!sprd_hwlock){
		printk(KERN_ERR "%s:Can't find hwspinlock device!\n",__func__);
		return -ENODEV;
	}

	/* for sharkl/sharkl64/whale project */
	if (sprd_hwlock->hwspinlock_vid == 0x100 || sprd_hwlock->hwspinlock_vid == 0x200
		|| sprd_hwlock->hwspinlock_vid == 0x300){
		return 1;
	} else {
		printk(KERN_ERR "Warning:hwlock version id is 0x%x!\n",sprd_hwlock->hwspinlock_vid);
		return 0;
	}
}

static unsigned long sprd_hwspinlock_addr(unsigned int lock_id)
{
	unsigned long addr;
	struct hwspinlock *lock;

	if(lock_id >= HWSPINLOCK_ID_TOTAL_NUMS){
		printk(KERN_ERR "Hwspinlock id is wrong!\n");
		return 0;
	}

	lock = hwlocks[lock_id];
	if (!lock){
		printk(KERN_ERR "Call %s: can't find hwspinlock!\n",__func__);
		return 0;
	}
	else
		addr = (unsigned long)lock->priv;

	return addr;
}

static int sprd_init_hwlocks(int hwlock_id)
{
	struct hwspinlock **plock;
	int i;

	sprd_hwlocks_implemented(hwlock_id);

	if (hwlock_id == AON_HWSPLOCK)
		i = 0;
	else if (hwlock_id == AP_HWSPLOCK)
		i = 31;
	else
		i = HWSPINLOCK_ID_TOTAL_NUMS;

	for (; i < HWSPINLOCK_ID_TOTAL_NUMS; ++i) {
		if (hwlocks_implemented[i]) {
			plock = &hwlocks[i];
			*plock = hwspin_lock_request_specific(i);
			if (WARN_ON(IS_ERR_OR_NULL(*plock)))
				*plock = NULL;
			else
				pr_info("early alloc hwspinlock id %d\n",
					hwspin_lock_get_id(*plock));
		}
	}
	return 0;
}

static unsigned int do_lock_key(struct hwspinlock *lock)
{
	unsigned int key = THIS_PROCESSOR_KEY;
	return key;
}

static unsigned int
hwspinlock_show_lock_status(struct hwspinlock *lock)
{

	struct sprd_hwspinlock_dev *sprd_hwlock;

	sprd_hwlock = hwlock_to_sprdlock_dev(lock);
	return readl_relaxed(sprd_hwlock->hwspinlock_base + HWSPINLOCK_TTLSTS);
}

static int __hwspinlock_trylock(struct hwspinlock *lock)
{
	unsigned int status = 0;
	void __iomem *addr = lock->priv;

	if (sprd_check_hwspinlock_vid(lock)) {
		if (!readl_relaxed(addr))
			goto __locked;
	} else {
		unsigned int key = do_lock_key(lock);
		if (!(key ^ HWSPINLOCK_NOTTAKEN_V0))
			BUG_ON(1);
		if (HWSPINLOCK_NOTTAKEN_V0 == readl_relaxed(addr)) {
			writel_relaxed(key, addr);
			if (key == readl_relaxed(addr))
				goto __locked;
		}
	}

	status =  hwspinlock_show_lock_status(lock);
	printk(KERN_ERR "Hwspinlock [%d] lock failed!, hwspinlock reg = 0x%x\n",hwlock_to_id(lock), status);
	return 0;

__locked:
	sprd_record_hwlock_sts(hwlock_to_id(lock), 1);
	return 1;
}

static void __hwspinlock_unlock(struct hwspinlock *lock)
{
	void __iomem *lock_addr = lock->priv;
	int unlock_key;

	if (sprd_check_hwspinlock_vid(lock)) {
		unlock_key = HWSPINLOCK_NOTTAKEN_V1;
	} else {
		unlock_key = HWSPINLOCK_NOTTAKEN_V0;
		if (!(readl_relaxed(lock_addr) ^ unlock_key))
			BUG_ON(1);
	}

	writel_relaxed(unlock_key, lock_addr);
	sprd_record_hwlock_sts(hwlock_to_id(lock), 0);
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

static const struct hwspinlock_ops sprd_hwspinlock_ops = {
	.trylock = __hwspinlock_trylock,
	.unlock = __hwspinlock_unlock,
	.relax = __hwspinlock_relax,
};

static int sprd_hwspinlock_probe(struct platform_device *pdev)
{
	struct hwspinlock *hwlock;
	struct sprd_hwspinlock_dev *sprd_hwlock;
	static int hwspinlock_exist_cnt = 0;
	u32 num_locks = 0;
	int i, ret, hwlock_id;

	ret = of_alias_get_id(pdev->dev.of_node, "hwspinlock");
	if (IS_ERR_VALUE(ret)) {
		printk(KERN_ERR "Hwspinlock get alias failed!\n");
		return ret;
	}
	hwlock_id = ret;

	ret = of_property_read_u32(pdev->dev.of_node,"hwspinlock_cnt",&num_locks);
	if(ret){
		printk(KERN_ERR "Hwspinlock get count failed!\n");
		num_locks = 32;
	}

	sprd_hwlock = devm_kzalloc(&pdev->dev, sizeof(struct sprd_hwspinlock_dev) 
					+ num_locks * sizeof(*hwlock), GFP_KERNEL);
	if (!sprd_hwlock) {
		printk(KERN_ERR "Hwspinlock device alloc memory failed!\n");
		return -ENOMEM;
	}

	/* intial the hwspinlock */
	sprd_hwlock_init();
	sprd_hwlock->hwspinlock_cnt = num_locks;
	if (hwlock_id == 1)
		sprd_hwlock->hwspinlock_base = SPRD_HWSPINLOCK1_BASE;
	else if (hwlock_id == 0)
		sprd_hwlock->hwspinlock_base = SPRD_HWSPINLOCK0_BASE;
	else {
		printk(KERN_ERR "Hwspinlock id is wrong!\n");
		return -ENODEV;
	}
	sprd_hwlock->hwspinlock_vid = readl_relaxed((void __iomem *)(sprd_hwlock->hwspinlock_base + HWSPINLOCK_VERID));

	for (i = 0, hwlock = &sprd_hwlock->bank.lock[0]; i < num_locks; i++, hwlock++) {
		if (sprd_hwlock->hwspinlock_vid == 0x100 || sprd_hwlock->hwspinlock_vid == 0x200
			|| sprd_hwlock->hwspinlock_vid == 0x300)
			hwlock->priv = (void __iomem *)(sprd_hwlock->hwspinlock_base + HWSPINLOCK_TOKEN_V1(i));
		else
			hwlock->priv = (void __iomem *)(sprd_hwlock->hwspinlock_base + HWSPINLOCK_TOKEN_V0(i));
	}

	/*
	 * runtime PM will make sure the clock of this module is
	 * enabled if at least one lock is requested
	 */
	platform_set_drvdata(pdev, sprd_hwlock);
	pm_runtime_enable(&pdev->dev);

	ret = hwspin_lock_register(&sprd_hwlock->bank, &pdev->dev, &sprd_hwspinlock_ops,
				   hwspinlock_exist_cnt, num_locks);
	if (ret){
		printk(KERN_ERR "Hwspinlock register failed!\n");
		goto reg_fail;
	}

	hwspinlock_exist_cnt += num_locks;
	/* inital the hwlocks */
	sprd_init_hwlocks(hwlock_id);
#ifdef	CONFIG_ARCH_WHALE
	/* record master user bits */
	writel_relaxed(HWSPINLOCK_USER_BITS, (void __iomem *)(sprd_hwlock->hwspinlock_base + HWSPINLOCK_RECCTRL));
#endif

	printk("sprd hwspinlock probe ok: hwspinlock name = %s, hwspinlock_vid = 0x%x,"
			"hwspinlock_exist_cnt = %d\n", pdev->name,sprd_hwlock->hwspinlock_vid,hwspinlock_exist_cnt); 

	return 0;

reg_fail:
	pm_runtime_disable(&pdev->dev);
	return ret;
}

static int sprd_hwspinlock_remove(struct platform_device *pdev)
{
	struct sprd_hwspinlock_dev *sprd_hwlock = platform_get_drvdata(pdev);
	int ret;

	ret = hwspin_lock_unregister(&sprd_hwlock->bank);
	if (ret) {
		dev_err(&pdev->dev, "%s failed: %d\n", __func__, ret);
		return ret;
	}

	pm_runtime_disable(&pdev->dev);
	return 0;
}

static const struct of_device_id sprd_hwspinlock_of_match[] = {
	{ .compatible = "sprd,sprd-hwspinlock", },
	{ /* sentinel */ }
};

static struct platform_driver sprd_hwspinlock_driver = {
	.probe = sprd_hwspinlock_probe,
	.remove = sprd_hwspinlock_remove,
	.driver = {
		.name = "sprd_hwspinlock",
		.owner = THIS_MODULE,
		.of_match_table = of_match_ptr(sprd_hwspinlock_of_match),
	},
};

static int __init sprd_hwspinlock_init(void)
{
	return platform_driver_register(&sprd_hwspinlock_driver);
}

/* board init code might need to reserve hwspinlocks for predefined purposes */
postcore_initcall(sprd_hwspinlock_init);

static void __exit sprd_hwspinlock_exit(void)
{
	platform_driver_unregister(&sprd_hwspinlock_driver);
}

module_exit(sprd_hwspinlock_exit);

static int remove_node(struct device_node *np)
{
	struct property *ps;

	ps = kzalloc(sizeof(*ps), GFP_KERNEL);
	if (!ps)
		return -1;

	ps->value = kstrdup("no", GFP_KERNEL);
	ps->length = strlen(ps->value);
	ps->name = kstrdup("status", GFP_KERNEL);
	of_update_property(np, ps);
	printk("After register,hwspinlock1 node available=%d\n",of_device_is_available(np));

	return 0;
}

int __init early_hwspinlocks_init(void)
{
	struct device_node *np;
	struct platform_device *pdev;
	int ret;

	np = of_find_node_by_name(NULL, "hwspinlock1");
	if (!np) {
		pr_warn("Can't get the hwspinlock1 node!\n");
		return -ENODEV;
	}

	pdev = of_platform_device_create(np, 0, NULL);
	if (!pdev) {
		pr_warn("register hwspinlock1 failed!\n");
		return -ENODEV;
	}

	pr_info("SPRD register hwspinlock1 ok,hwspinlock1's name is %s!\n",pdev->name);
	ret = remove_node(np);
	return ret;
}

MODULE_LICENSE("GPL v2");
MODULE_DESCRIPTION("Hardware spinlock driver for Spreadtrum");
MODULE_AUTHOR("baolin.wang <baolin.wang@spreadtrum.com>");
