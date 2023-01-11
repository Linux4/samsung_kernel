/*
 * arch/arm/plat-pxa/debugfs.c
 *
 * Author:	Neil Zhang <zhangwm@marvell.com>
 * Copyright:	(C) 2012 Marvell International Ltd.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */

#include <linux/debugfs.h>
#include <linux/uaccess.h>
#include <linux/module.h>
#include <linux/io.h>
#include <linux/cpufreq.h>
#include <linux/debugfs-pxa.h>

#ifdef CONFIG_ARM_GIC
#include <linux/irqchip/arm-gic.h>
#endif

#ifdef CONFIG_ARM_GIC
#define GIC_DIST_STATUS		0xD00

static int gic_offset = -1;

static ssize_t gic_write(struct file *filp, const char __user *buffer,
		size_t count, loff_t *ppos)
{
	char buf[32] = {0};
	int offset;
	int ret;

	/* copy user's input to kernel space */
	if (copy_from_user(buf, buffer, count))
		return -EFAULT;

	ret = sscanf(buf, "%x", &offset);
	pr_info("Check gic offset: 0x%x\n", offset);

	if (offset < 0 || offset > 0xffc)
		pr_err("The offset is out of GIC distributor range.\n");
	else if (offset % 4)
		pr_err("offset should be aligned to 4 bytes.\n");
	else
		gic_offset = offset;

	return count;

}

static ssize_t gic_read(struct file *filp, char __user *buffer,
		size_t count, loff_t *ppos)
{
	void __iomem *dist_base = gic_get_dist_base();
	char *p;
	size_t ret, buf_len;
	u32 gic_irqs;
	u32 value;
	int i, len = 0;

	p = (char *)__get_free_pages(GFP_KERNEL, 0);
	if (!p)
		return -ENOMEM;

	buf_len = (PAGE_SIZE - 1);

	if (gic_offset != -1) {
		value = readl_relaxed(dist_base + gic_offset);
		len += snprintf(p + len, buf_len - len,
				"offset[0x%x]: 0x%08x\n", gic_offset, value);
	} else {
		gic_irqs = readl_relaxed(dist_base + GIC_DIST_CTR) & 0x1f;
		gic_irqs = (gic_irqs + 1) * 32;
		if (gic_irqs > 1020)
			gic_irqs = 1020;

		value = readl_relaxed(dist_base + GIC_DIST_CTRL);
		len += snprintf(p + len, buf_len - len,
				"Dist Control Register: 0x%08x\n", value);

		for (i = 32; i < gic_irqs; i += 4) {
			value = readl_relaxed(dist_base + GIC_DIST_TARGET
					+ i * 4 / 4);
			len += snprintf(p + len, buf_len - len,
				"Target setting[%d]: 0x%08x\n", i / 4, value);
		}

		for (i = 32; i < gic_irqs; i += 32) {
			value = readl_relaxed(dist_base + GIC_DIST_ENABLE_SET
					+ i * 4 / 32);
			len += snprintf(p + len, buf_len - len,
				"Enable setting[%d]: 0x%08x\n", i / 32, value);
		}

		for (i = 32; i < gic_irqs; i += 32) {
			value = readl_relaxed(dist_base + GIC_DIST_PENDING_SET
					+ i * 4 / 32);
			len += snprintf(p + len, buf_len - len,
				"Pending status[%d]: 0x%08x\n", i / 32, value);
		}

		for (i = 32; i < gic_irqs; i += 32) {
			value = readl_relaxed(dist_base + GIC_DIST_STATUS
					+ i * 4 / 32);
			len += snprintf(p + len, buf_len - len,
				"SPI status[%d]: 0x%08x\n", i / 32, value);
		}
	}

	if (len == buf_len)
		pr_warn("The buffer for dumpping gic is full now!\n");

	ret = simple_read_from_buffer(buffer, count, ppos, p, len);

	free_pages((unsigned long)p, 0);

	if (gic_offset != -1 && !ret)
		gic_offset = -1;

	return ret;
}

const struct file_operations dumpregs_gic_fops = {
	.read = gic_read,
	.write = gic_write,
};
#endif

static int __init pxa_debugfs_init(void)
{
#ifdef CONFIG_ARM_GIC
	struct dentry *dumpregs_gic;

	dumpregs_gic = debugfs_create_file("gic_dist", 0664,
					pxa, NULL, &dumpregs_gic_fops);
	if (!dumpregs_gic)
		return -ENOENT;

	return 0;
#endif
}
postcore_initcall(pxa_debugfs_init);
