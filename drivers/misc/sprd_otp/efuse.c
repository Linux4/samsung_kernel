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
 * The electrical fuse is a type of non-volatile memory fabricated
 * in standard CMOS logic process. This electrical fuse macro is widely
 * used in chip ID, memory redundancy, security code, configuration setting,
 * and feature selection, etc.
 *
 * This efuse controller is designed for 32*32 bits electrical fuses,
 * support TSMC HPM 28nm product of "TEF28HPM32X32HD18_PHRM".
 *
 * and we default use double-bit, 512bits efuse are visiable for software.
 *
 * efuse driver ONLY support module ip version since from r3p0
 * which had integrated into scx30g and scx35l etc.
 *
 * TODO:
 * 1. do something when block had been locked with bit31 if need
 * 1. check and clear blk prog/err flag if need
 * 1. wait *300ms* for read/prog ready time-out
 * 1. only mutexlock and hwspinlock for efuse access sync with cp
 * 1. be care not do use efuse API in interrupt function
 * 1. no need soft reset after efuse read/prog and power on/off
 * 1. efuse block width should less than 8bits
 * 1. efuse block count should not bigger than 32! or not should expland the cached otp arrary
 * 1. support efuse DT info (version, blocks, ...) later
 * 1. there is no handle for efuse module removed
 */

#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/stat.h>
#include <linux/err.h>
#include <linux/delay.h>
#include <linux/of.h>
#include <linux/of_device.h>
#include <mach/arch_lock.h>

#include "sprd_otp.h"
#include "__regs_efuse.h"

#define REGS_EFUSE_BASE                 SPRD_UIDEFUSE_BASE

#define EFUSE_BLOCK_MAX                 ( 16 )
#define EFUSE_BLOCK_WIDTH               ( 32 )	/* bit counts */

static u32 efuse_magic;
static DEFINE_MUTEX(efuse_mtx);
static void efuse_lock(void)
{
	mutex_lock(&efuse_mtx);
	arch_hwlock_fast(HWLOCK_EFUSE);
}

static void efuse_unlock(void)
{
	arch_hwunlock_fast(HWLOCK_EFUSE);
	mutex_unlock(&efuse_mtx);
}

static void efuse_reset(void)
{
	/* should enable module before soft reset efuse */
	WARN_ON(!sci_glb_read(REG_AON_APB_APB_EB0, BIT_EFUSE_EB));
	sci_glb_set(REG_AON_APB_APB_RST0, BIT_EFUSE_SOFT_RST);
	udelay(5);
	sci_glb_clr(REG_AON_APB_APB_RST0, BIT_EFUSE_SOFT_RST);
}

/* FIXME: Set EFS_VDD_ON will open 0.9v static power supply for efuse memory,
 * before any operation towards to efuse memory this bit have to set to 1.
 * Once this bit is cleared, the efuse will go to power down mode.
 *
 * each time when EFS_VDD_ON changes, software should wait at least 1ms to let
 * VDD become stable.
 *
 * For VDDQ(1.8v) power, to prevent the overshot of VDDQ, a extra power switch
 * connected to ground are controlled by "EFS_VDDQ_K2_ON"
 */
void __efuse_prog_power_on(void)
{
	u32 cfg0;
	sci_glb_set(REG_AON_APB_APB_EB0, BIT_EFUSE_EB);
	cfg0 = __raw_readl((void *)REG_EFUSE_CFG0);
	cfg0 &= ~(BIT_EFS_VDDQ_K2_ON | BIT_EFS_VDDQ_K1_ON);
	cfg0 |= BIT_EFS_VDD_ON;
	__raw_writel(cfg0, (void *)REG_EFUSE_CFG0);
	msleep(1);

	cfg0 |= BIT_EFS_VDDQ_K1_ON;
	__raw_writel(cfg0, (void *)REG_EFUSE_CFG0);
	msleep(1);
}

void __efuse_power_on(void)
{
	u32 cfg0;
	sci_glb_set(REG_AON_APB_APB_EB0, BIT_EFUSE_EB);
	cfg0 = __raw_readl((void *)REG_EFUSE_CFG0);
	cfg0 &= ~BIT_EFS_VDDQ_K1_ON;
	cfg0 |= BIT_EFS_VDD_ON | BIT_EFS_VDDQ_K2_ON;
	__raw_writel(cfg0, (void *)REG_EFUSE_CFG0);
	msleep(1);
}

void __efuse_power_off(void)
{
	u32 cfg0 = __raw_readl((void *)REG_EFUSE_CFG0);
	if (cfg0 & BIT_EFS_VDDQ_K1_ON) {
		cfg0 &= ~BIT_EFS_VDDQ_K1_ON;
		__raw_writel(cfg0, (void *)REG_EFUSE_CFG0);
		msleep(1);
	}

	cfg0 |= BIT_EFS_VDDQ_K2_ON;
	cfg0 &= ~BIT_EFS_VDD_ON;
	__raw_writel(cfg0, (void *)REG_EFUSE_CFG0);
	msleep(1);

	sci_glb_clr(REG_AON_APB_APB_EB0, BIT_EFUSE_EB);
}

static __inline int __efuse_wait_clear(u32 bits)
{
	int ret = 0;
	unsigned long timeout;

	pr_debug("wait %x\n", __raw_readl((void *)REG_EFUSE_STATUS));

	/* wait for maximum of 300 msec */
	timeout = jiffies + msecs_to_jiffies(300);
	while (__raw_readl((void *)REG_EFUSE_STATUS) & bits) {
		if (time_after(jiffies, timeout)) {
			WARN_ON(1);
			ret = -ETIMEDOUT;
			break;
		}
		cpu_relax();
	}
	return ret;
}

static u32 __efuse_read(int blk)
{
	u32 val = 0;

	/* enable efuse module clk and power before */

	__raw_writel(BITS_READ_WRITE_INDEX(blk),
		     (void *)REG_EFUSE_READ_WRITE_INDEX);
	__raw_writel(BIT_RD_START, (void *)REG_EFUSE_MODE_CTRL);

	if (IS_ERR_VALUE(__efuse_wait_clear(BIT_READ_BUSY)))
		goto out;

	val = __raw_readl((void *)REG_EFUSE_DATA_RD);

out:
	return val;
}

static u32 efuse_read(int blk_index)
{
	u32 val;
	pr_debug("efuse read %d\n", blk_index);
	efuse_lock();

	__efuse_power_on();
	val = __efuse_read(blk_index);
	__efuse_power_off();

	efuse_unlock();
	return val;
}

static int __efuse_prog(int blk, u32 val)
{
	u32 cfg0 = __raw_readl((void *)REG_EFUSE_CFG0);

	if (blk < 0 || blk >= EFUSE_BLOCK_MAX)	/* debug purpose */
		goto out;

	/* enable pgm mode and setup magic number before programming */
	cfg0 |= BIT_PGM_EN;
	__raw_writel(cfg0, (void *)REG_EFUSE_CFG0);
	__raw_writel(BITS_MAGIC_NUMBER(efuse_magic),
		     (void *)REG_EFUSE_MAGIC_NUMBER);

	__raw_writel(val, (void *)REG_EFUSE_DATA_WR);
	__raw_writel(BITS_READ_WRITE_INDEX(blk),
		     (void *)REG_EFUSE_READ_WRITE_INDEX);
	pr_debug("cfg0 %x\n", __raw_readl((void *)REG_EFUSE_CFG0));
	__raw_writel(BIT_PG_START, (void *)REG_EFUSE_MODE_CTRL);
	if (IS_ERR_VALUE(__efuse_wait_clear(BIT_PGM_BUSY)))
		goto out;

out:
	__raw_writel(0, (void *)REG_EFUSE_MAGIC_NUMBER);
	cfg0 &= ~BIT_PGM_EN;
	__raw_writel(cfg0, (void *)REG_EFUSE_CFG0);
	return 0;
}

static int efuse_prog(int blk_index, u32 val)
{
	int ret;
	pr_debug("efuse prog %d %08x\n", blk_index, val);

	efuse_lock();

	/* enable vddon && vddq */
	__efuse_prog_power_on();
	ret = __efuse_prog(blk_index, val);
	__efuse_power_off();

	efuse_unlock();
	return ret;
}

static struct sprd_otp_operations efuse_ops = {
	.reset = efuse_reset,
	.read = efuse_read,
	.prog = efuse_prog,
};

u32 __ddie_efuse_read(int blk_index)
{
	return efuse_ops.read(blk_index);
}

EXPORT_SYMBOL_GPL(__ddie_efuse_read);

static ssize_t efuse_block_show(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	return sprintf(buf, "ddie efuse blocks: %uX%ubits\nprog magic: 0x%x\n",
		       EFUSE_BLOCK_MAX, EFUSE_BLOCK_WIDTH, efuse_magic);
}

static ssize_t efuse_magic_store(struct device *pdev,
				 struct device_attribute *attr,
				 const char *buff, size_t size)
{
	sscanf(buff, "%x", &efuse_magic);
	return size;
}

static ssize_t efuse_block_dump(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	int idx;
	char *p = buf;

	p += sprintf(p, "ddie efuse blocks dump:\n");
	for (idx = 0; idx < EFUSE_BLOCK_MAX; idx++) {
		p += sprintf(p, "[%02d] %08x\n", idx, efuse_read(idx));
	}
	return p - buf;
}

static DEVICE_ATTR(block, S_IRUGO, efuse_block_show, NULL);
static DEVICE_ATTR(magic, S_IWUSR, NULL, efuse_magic_store);
static DEVICE_ATTR(dump, S_IRUGO, efuse_block_dump, NULL);

int __init sprd_efuse_init(void)
{
	int ret;
	void *dev;
	dev =
	    sprd_otp_register("sprd_efuse_otp", &efuse_ops, EFUSE_BLOCK_MAX,
			      EFUSE_BLOCK_WIDTH / 8);
	if (IS_ERR_OR_NULL(dev))
		return PTR_ERR(dev);

	ret = device_create_file(dev, &dev_attr_block);
	ret |= device_create_file(dev, &dev_attr_magic);
	ret |= device_create_file(dev, &dev_attr_dump);
	if (ret)
		return ret;

	return 0;
}
