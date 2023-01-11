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

#include <mach/addr-map.h>
#include <linux/cputype.h>

#ifdef CONFIG_ARM_GIC
#include <linux/irqchip/arm-gic.h>
#endif

static ssize_t cp15_write(struct file *filp, const char __user *buffer,
		size_t count, loff_t *ppos)
{
	pr_info("cp15 doesn't support read a giving register now.\n"
		"Please cat it directly.\n");

	return count;
}

static ssize_t cp15_read(struct file *filp, char __user *buffer,
		size_t count, loff_t *ppos)
{
	char *p;
	size_t ret, buf_len;
	u32 value;
	int len = 0;

	p = (char *)__get_free_pages(GFP_KERNEL, 0);
	if (!p)
		return -ENOMEM;

	buf_len	= (PAGE_SIZE - 1);

	/* c0 registers */
	asm volatile("mrc p15, 0, %0, c0, c0, 0" : "=r"(value));
	len += snprintf(p + len, buf_len - len,
			"Main ID: 0x%08x\n", value);

	asm volatile("mrc p15, 0, %0, c0, c0, 1" : "=r"(value));
	len += snprintf(p + len, buf_len - len,
			"Cache Type: 0x%08x\n", value);

	asm volatile("mrc p15, 0, %0, c0, c0, 3" : "=r"(value));
	len += snprintf(p + len, buf_len - len,
			"TLB Type: 0x%08x\n", value);

	asm volatile("mrc p15, 0, %0, c0, c1, 0" : "=r"(value));
	len += snprintf(p + len, buf_len - len,
			"Processor Feature 0: 0x%08x\n", value);

	asm volatile("mrc p15, 0, %0, c0, c1, 1" : "=r"(value));
	len += snprintf(p + len, buf_len - len,
			"Processor Feature 1: 0x%08x\n", value);

	asm volatile("mrc p15, 0, %0, c0, c1, 2" : "=r"(value));
	len += snprintf(p + len, buf_len - len,
			"Debug Feature 0: 0x%08x\n", value);

	asm volatile("mrc p15, 0, %0, c0, c1, 3" : "=r"(value));
	len += snprintf(p + len, buf_len - len,
			"Auxiliary Feature 0: 0x%08x\n", value);

	asm volatile("mrc p15, 0, %0, c0, c1, 4" : "=r"(value));
	len += snprintf(p + len, buf_len - len,
			"Memory Model Feature 0: 0x%08x\n", value);

	asm volatile("mrc p15, 0, %0, c0, c1, 5" : "=r"(value));
	len += snprintf(p + len, buf_len - len,
			"Memory Model Feature 1: 0x%08x\n", value);

	asm volatile("mrc p15, 0, %0, c0, c1, 6" : "=r"(value));
	len += snprintf(p + len, buf_len - len,
			"Memory Model Feature 2: 0x%08x\n", value);

	asm volatile("mrc p15, 0, %0, c0, c1, 7" : "=r"(value));
	len += snprintf(p + len, buf_len - len,
			"Memory Model Feature 3: 0x%08x\n", value);

	asm volatile("mrc p15, 0, %0, c0, c2, 0" : "=r"(value));
	len += snprintf(p + len, buf_len - len,
			"Instruction Set Attribute 0: 0x%08x\n", value);

	asm volatile("mrc p15, 0, %0, c0, c2, 1" : "=r"(value));
	len += snprintf(p + len, buf_len - len,
			"Instruction Set Attribute 1: 0x%08x\n", value);

	asm volatile("mrc p15, 0, %0, c0, c2, 2" : "=r"(value));
	len += snprintf(p + len, buf_len - len,
			"Instruction Set Attribute 2: 0x%08x\n", value);

	asm volatile("mrc p15, 0, %0, c0, c2, 3" : "=r"(value));
	len += snprintf(p + len, buf_len - len,
			"Instruction Set Attribute 3: 0x%08x\n", value);

	asm volatile("mrc p15, 0, %0, c0, c2, 4" : "=r"(value));
	len += snprintf(p + len, buf_len - len,
			"Instruction Set Attribute 4: 0x%08x\n", value);

	asm volatile("mrc p15, 1, %0, c0, c0, 0" : "=r"(value));
	len += snprintf(p + len, buf_len - len,
			"Current Cache Size ID: 0x%08x\n", value);

	asm volatile("mrc p15, 1, %0, c0, c0, 1" : "=r"(value));
	len += snprintf(p + len, buf_len - len,
			"Current Cache Level ID: 0x%08x\n", value);

	asm volatile("mrc p15, 2, %0, c0, c0, 0" : "=r"(value));
	len += snprintf(p + len, buf_len - len,
			"Cache Size Selection: 0x%08x\n", value);

	/* c1 registers */
	asm volatile("mrc p15, 0, %0, c1, c0, 0" : "=r"(value));
	len += snprintf(p + len, buf_len - len,
			"System Control: 0x%08x\n", value);

	asm volatile("mrc p15, 0, %0, c1, c0, 1" : "=r"(value));
	len += snprintf(p + len, buf_len - len,
			"Auxiliary Control: 0x%08x\n", value);

	len += snprintf(p + len, buf_len - len, "L2 prefetch: %s\n",
			(value & (1 << 1)) ? "Enabled" : "Disabled");

	asm volatile("mrc p15, 0, %0, c1, c0, 2" : "=r"(value));
	len += snprintf(p + len, buf_len - len,
			"Coprocessor Access Control: 0x%08x\n", value);

	if (cpu_is_ca9()) {
		asm volatile("mrc p15, 0, %0, c1, c1, 3" : "=r"(value));
		len += snprintf(p + len, buf_len - len,
				"Virtualization Control: 0x%08x\n", value);
	}

	/* c2 registers */
	asm volatile("mrc p15, 0, %0, c2, c0, 0" : "=r"(value));
	len += snprintf(p + len, buf_len - len,
			"Translation Table Base 0: 0x%08x\n", value);

	asm volatile("mrc p15, 0, %0, c2, c0, 1" : "=r"(value));
	len += snprintf(p + len, buf_len - len,
			"Translation Table Base 1: 0x%08x\n", value);

	asm volatile("mrc p15, 0, %0, c2, c0, 2" : "=r"(value));
	len += snprintf(p + len, buf_len - len,
			"Translation Table Control: 0x%08x\n", value);

	/* c3 registers */
	asm volatile("mrc p15, 0, %0, c3, c0, 0" : "=r"(value));
	len += snprintf(p + len, buf_len - len,
			"Domain Access Control: 0x%08x\n", value);

	/* c5 registers */
	asm volatile("mrc p15, 0, %0, c5, c0, 0" : "=r"(value));
	len += snprintf(p + len, buf_len - len,
			"Data Fault Status: 0x%08x\n", value);

	asm volatile("mrc p15, 0, %0, c5, c0, 1" : "=r"(value));
	len += snprintf(p + len, buf_len - len,
			"Instruction Fault Status: 0x%08x\n", value);

	/* c6 registers */
	asm volatile("mrc p15, 0, %0, c6, c0, 0" : "=r"(value));
	len += snprintf(p + len, buf_len - len,
			"Data Fault Address: 0x%08x\n", value);

	asm volatile("mrc p15, 0, %0, c6, c0, 2" : "=r"(value));
	len += snprintf(p + len, buf_len - len,
			"Instruction Fault Address: 0x%08x\n", value);

	/* c7 register */
	asm volatile("mrc p15, 0, %0, c7, c4, 0" : "=r"(value));
	len += snprintf(p + len, buf_len - len,
			"Physical Address: 0x%08x\n", value);

	/* c9 register */
	asm volatile("mrc p15, 0, %0, c9, c12, 0" : "=r"(value));
	len += snprintf(p + len, buf_len - len,
			"Performance Monitor Control(PMCR): 0x%08x\n", value);

	asm volatile("mrc p15, 0, %0, c9, c12, 1" : "=r"(value));
	len += snprintf(p + len, buf_len - len,
			"Count Enable Set(PMCNTENSET): 0x%08x\n", value);

	asm volatile("mrc p15, 0, %0, c9, c12, 2" : "=r"(value));
	len += snprintf(p + len, buf_len - len,
			"Count Enable Clear(PMCNTENCLR): 0x%08x\n", value);

	asm volatile("mrc p15, 0, %0, c9, c12, 3" : "=r"(value));
	len += snprintf(p + len, buf_len - len,
			"Overflow Flag Status(PMOVSR): 0x%08x\n", value);

	asm volatile("mrc p15, 0, %0, c9, c12, 5" : "=r"(value));
	len += snprintf(p + len, buf_len - len,
			"Event Counter Selection(PMSELR): 0x%08x\n", value);

	asm volatile("mrc p15, 0, %0, c9, c13, 0" : "=r"(value));
	len += snprintf(p + len, buf_len - len,
			"Cycle Count(PMCCNTR): 0x%08x\n", value);

	asm volatile("mrc p15, 0, %0, c9, c13, 1" : "=r"(value));
	len += snprintf(p + len, buf_len - len,
			"Event Type Select(PMXEVTYPER): 0x%08x\n", value);

	asm volatile("mrc p15, 0, %0, c9, c13, 2" : "=r"(value));
	len += snprintf(p + len, buf_len - len,
			"Event Count(PMXEVCNTR): 0x%08x\n", value);

	asm volatile("mrc p15, 0, %0, c9, c14, 0" : "=r"(value));
	len += snprintf(p + len, buf_len - len,
			"User Enable(PMUSERENR): 0x%08x\n", value);

	asm volatile("mrc p15, 0, %0, c9, c14, 1" : "=r"(value));
	len += snprintf(p + len, buf_len - len,
			"Interrupt Enable Set(PMINTENSET): 0x%08x\n", value);

	asm volatile("mrc p15, 0, %0, c9, c14, 2" : "=r"(value));
	len += snprintf(p + len, buf_len - len,
			"Interrupt Enable Clear(PMINTENCLR): 0x%08x\n", value);

	if (cpu_is_ca7()) {
		asm volatile("mrc p15, 1, %0, c9, c0, 2" : "=r"(value));
		len += snprintf(p + len, buf_len - len,
				"L2 Control: 0x%08x\n", value);

		asm volatile("mrc p15, 1, %0, c9, c0, 3" : "=r"(value));
		len += snprintf(p + len, buf_len - len,
				"L2 Extended Control: 0x%08x\n", value);
	}

	/* c10 registers */
	if (cpu_is_ca9()) {
		asm volatile("mrc p15, 0, %0, c10, c0, 0" : "=r"(value));
		len += snprintf(p + len, buf_len - len,
				"TLB Lockdown: 0x%08x\n", value);
	}
	asm volatile("mrc p15, 0, %0, c10, c2, 0" : "=r"(value));
	len += snprintf(p + len, buf_len - len,
			"Memory Attribute PRRR: 0x%08x\n", value);

	asm volatile("mrc p15, 0, %0, c10, c2, 1" : "=r"(value));
	len += snprintf(p + len, buf_len - len,
			"Memory Attribute NMRR: 0x%08x\n", value);

	if (cpu_is_ca7()) {
		asm volatile("mrc p15, 0, %0, c10, c2, 0" : "=r"(value));
		len += snprintf(p + len, buf_len - len,
				"Memory Attribute Indirection 0: 0x%08x\n",
				value);

		asm volatile("mrc p15, 0, %0, c10, c2, 1" : "=r"(value));
		len += snprintf(p + len, buf_len - len,
				"Memory Attribute Indirection 1: 0x%08x\n",
				value);
	}

	/* c11 register */
	if (cpu_is_ca9()) {
		asm volatile("mrc p15, 0, %0, c11, c1, 0" : "=r"(value));
		len += snprintf(p + len, buf_len - len,
				"Preload Engine User Accessibility: 0x%08x\n",
				value);

		asm volatile("mrc p15, 0, %0, c11, c1, 1" : "=r"(value));
		len += snprintf(p + len, buf_len - len,
				"Preload Engine Parameters Control: 0x%08x\n",
				value);
	}

	/* c12 register */
	asm volatile("mrc p15, 0, %0, c12, c0, 0" : "=r"(value));
	len += snprintf(p + len, buf_len - len,
			"Vector Base Address: 0x%08x\n", value);

	asm volatile("mrc p15, 0, %0, c12, c1, 0" : "=r"(value));
	len += snprintf(p + len, buf_len - len,
			"Interrupt Status: 0x%08x\n", value);

	/* c13 register */
	asm volatile("mrc p15, 0, %0, c13, c0, 0" : "=r"(value));
	len += snprintf(p + len, buf_len - len,
			"FCSE Process ID: 0x%08x\n", value);

	asm volatile("mrc p15, 0, %0, c13, c0, 1" : "=r"(value));
	len += snprintf(p + len, buf_len - len,
			"Context ID: 0x%08x\n", value);

	asm volatile("mrc p15, 0, %0, c13, c0, 2" : "=r"(value));
	len += snprintf(p + len, buf_len - len,
			"User Thread ID: 0x%08x\n", value);

	asm volatile("mrc p15, 0, %0, c13, c0, 4" : "=r"(value));
	len += snprintf(p + len, buf_len - len,
			"Privileged Only Thread ID: 0x%08x\n", value);

	/* c15 registers */
	if (cpu_is_ca9()) {
		asm volatile("mrc p15, 0, %0, c15, c0, 0" : "=r"(value));
		len += snprintf(p + len, buf_len - len,
				"Power Control: 0x%08x\n", value);

		asm volatile("mrc p15, 0, %0, c15, c1, 0" : "=r"(value));
		len += snprintf(p + len, buf_len - len, "NEON is: %s\n",
				(value & (1 << 0)) ?  "Busy" : "Idle");

		asm volatile("mrc p15, 0, %0, c15, c5, 2" : "=r"(value));
		len += snprintf(p + len, buf_len - len,
				"Main TLB VA: 0x%08x\n", value);

		asm volatile("mrc p15, 0, %0, c15, c6, 2" : "=r"(value));
		len += snprintf(p + len, buf_len - len,
				"Main TLB PA: 0x%08x\n", value);

		asm volatile("mrc p15, 0, %0, c15, c7, 2" : "=r"(value));
		len += snprintf(p + len, buf_len - len,
				"Main TLB Attribute: 0x%08x\n", value);
	} else if (cpu_is_ca7()) {

	}
	if (len == buf_len)
		pr_warn("The buffer for dumpping cp15 is full now!\n");

	ret = simple_read_from_buffer(buffer, count, ppos, p, len);
	free_pages((unsigned long)p, 0);

	return ret;
}

const struct file_operations dumpregs_cp15_fops = {
	.read = cp15_read,
	.write = cp15_write,
};


#ifdef CONFIG_ARM_GIC
#define GIC_DIST_STATUS		0xD00

static int gic_offset = -1;

static ssize_t gic_write(struct file *filp, const char __user *buffer,
		size_t count, loff_t *ppos)
{
	char buf[32] = {0};
	int offset;

	/* copy user's input to kernel space */
	if (copy_from_user(buf, buffer, count))
		return -EFAULT;

	if (sscanf(buf, "%x", &offset) != 1)
		return -EINVAL;

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
	struct dentry *dumpregs_cp15;
#ifdef CONFIG_ARM_GIC
	struct dentry *dumpregs_gic;
#endif

	dumpregs_cp15 = debugfs_create_file("cp15", 0664,
					pxa, NULL, &dumpregs_cp15_fops);
	if (!dumpregs_cp15)
		return -ENOENT;

#ifdef CONFIG_ARM_GIC
	dumpregs_gic = debugfs_create_file("gic_dist", 0664,
					pxa, NULL, &dumpregs_gic_fops);
	if (!dumpregs_gic)
		goto err_gic;
#endif

	return 0;

err_gic:
	debugfs_remove(dumpregs_cp15);
	dumpregs_cp15 = NULL;

	return -ENOENT;
}

postcore_initcall(pxa_debugfs_init);
