/*
 * Copyright (C) 2014 Spreadtrum Communications Inc.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

/* IMPORTANT:
 *
 * TODO:
 */

#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/stat.h>
#include <linux/err.h>
#include <linux/delay.h>
#include <linux/of.h>
#include <linux/of_device.h>
#include <mach/arch_lock.h>

#include <mach/hardware.h>
#include <mach/adi.h>
#include <mach/sci.h>
#include <mach/sci_glb_regs.h>

#include "sprd_otp.h"

#define EFUSE_BLOCK_MAX                 ( 4 )
#define EFUSE_BLOCK_WIDTH               ( 16 )	/* bit counts */

static DEFINE_MUTEX(afuse_mtx);
static void afuse_lock(void)
{
	mutex_lock(&afuse_mtx);
	arch_hwlock_fast(HWLOCK_EFUSE);
}

static void afuse_unlock(void)
{
	arch_hwunlock_fast(HWLOCK_EFUSE);
	mutex_unlock(&afuse_mtx);
}

static u32 afuse_read(int blk)
{
	unsigned long timeout;
	u32 val = 0;

	pr_debug("adie laser_fuse read %d\n", blk);
	afuse_lock();

	if (blk < 0 || blk >= EFUSE_BLOCK_MAX)	/* debug purpose */
		goto out;

	sci_adi_write_fast(ANA_REG_GLB_AFUSE_CTRL, BIT_AFUSE_READ_REQ, 1);
	udelay(1);
	/* wait for maximum of 300 msec */
	timeout = jiffies + msecs_to_jiffies(300);
	while (BIT_AFUSE_READ_REQ & sci_adi_read(ANA_REG_GLB_AFUSE_CTRL)) {
		if (time_after(jiffies, timeout)) {
			WARN_ON(1);
			goto out;
		}
		cpu_relax();
	}
	val = sci_adi_read(ANA_REG_GLB_AFUSE_OUT0 + 4 * blk);

out:
	afuse_unlock();
	return val;
}

#define BITS_AFUSE_DLY_PROT_KEY         (0xa2 << 8)
static void afuse_set_readdly(u32 dly)
{
	u32 val = BITS_AFUSE_DLY_PROT_KEY + BITS_AFUSE_READ_DLY(dly);
	afuse_lock();

	sci_adi_write_fast(ANA_REG_GLB_AFUSE_CTRL, val, 0);
	val &= ~BITS_AFUSE_DLY_PROT_KEY;	/*release lock */
	sci_adi_write_fast(ANA_REG_GLB_AFUSE_CTRL, val, 1);

	afuse_unlock();
}

static struct sprd_otp_operations afuse_ops = {
	.read = afuse_read,
};

u32 __adie_laserfuse_read(int blk_index)
{
	return afuse_ops.read(blk_index);
}

EXPORT_SYMBOL_GPL(__adie_laserfuse_read);

static ssize_t afuse_block_show(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	return sprintf(buf, "adie laser_fuse blocks: %uX%ubits\n",
		       EFUSE_BLOCK_MAX, EFUSE_BLOCK_WIDTH);
}

static ssize_t afuse_readdly_store(struct device *pdev,
				   struct device_attribute *attr,
				   const char *buff, size_t size)
{
	u32 dly;
	sscanf(buff, "%x", &dly);
	afuse_set_readdly(dly);
	return size;
}

static ssize_t afuse_block_dump(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	int idx;
	char *p = buf;

	p += sprintf(p, "adie laser_fuse blocks dump:\n");
	for (idx = 0; idx < EFUSE_BLOCK_MAX; idx++) {
		p += sprintf(p, "[%02d] %08x\n", idx,
			     __adie_laserfuse_read(idx));
	}
	return p - buf;
}

static DEVICE_ATTR(block, S_IRUGO, afuse_block_show, NULL);
static DEVICE_ATTR(readdly, S_IWUSR, NULL, afuse_readdly_store);
static DEVICE_ATTR(dump, S_IRUGO, afuse_block_dump, NULL);

int __init sprd_adie_laserfuse_init(void)
{
	int ret;
	void *dev;
	dev =
	    sprd_otp_register("sprd_afuse_otp", &afuse_ops,
			      EFUSE_BLOCK_MAX, EFUSE_BLOCK_WIDTH / 8);
	if (IS_ERR_OR_NULL(dev))
		return PTR_ERR(dev);

	ret = device_create_file(dev, &dev_attr_block);
	ret |= device_create_file(dev, &dev_attr_readdly);
	ret |= device_create_file(dev, &dev_attr_dump);
	if (ret)
		return ret;

	return 0;
}
