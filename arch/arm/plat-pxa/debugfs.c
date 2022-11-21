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

#include <mach/addr-map.h>
#include <plat/debugfs.h>

#ifdef CONFIG_CACHE_L2X0
#include <asm/hardware/cache-l2x0.h>
#endif

#ifdef CONFIG_ARM_GIC
#include <asm/hardware/gic.h>
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

	len += snprintf(p + len, buf_len - len, "\tL2 prefetch: %s\n",
			(value & (1 << 1)) ? "Enabled" : "Disabled");

	asm volatile("mrc p15, 0, %0, c1, c0, 2" : "=r"(value));
	len += snprintf(p + len, buf_len - len,
			"Coprocessor Access Control: 0x%08x\n", value);

	asm volatile("mrc p15, 0, %0, c1, c1, 0" : "=r"(value));
	len += snprintf(p + len, buf_len - len,
			"Secure Configuration: 0x%08x\n", value);

	asm volatile("mrc p15, 0, %0, c1, c1, 1" : "=r"(value));
	len += snprintf(p + len, buf_len - len,
			"Secure Debug Enable: 0x%08x\n", value);

	asm volatile("mrc p15, 0, %0, c1, c1, 2" : "=r"(value));
	len += snprintf(p + len, buf_len - len,
			"Non-Secure Access Control: 0x%08x\n", value);

	asm volatile("mrc p15, 0, %0, c1, c1, 3" : "=r"(value));
	len += snprintf(p + len, buf_len - len,
			"Virtualization Control: 0x%08x\n", value);

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

	/* c10 registers */
	asm volatile("mrc p15, 0, %0, c10, c0, 0" : "=r"(value));
	len += snprintf(p + len, buf_len - len,
			"TLB Lockdown: 0x%08x\n", value);

	asm volatile("mrc p15, 0, %0, c10, c2, 0" : "=r"(value));
	len += snprintf(p + len, buf_len - len,
			"Memory Attribute PRRR: 0x%08x\n", value);

	asm volatile("mrc p15, 0, %0, c10, c2, 1" : "=r"(value));
	len += snprintf(p + len, buf_len - len,
			"Memory Attribute NMRR: 0x%08x\n", value);

	/* c11 register */
	asm volatile("mrc p15, 0, %0, c11, c1, 0" : "=r"(value));
	len += snprintf(p + len, buf_len - len,
			"Preload Engine User Accessibility: 0x%08x\n", value);

	asm volatile("mrc p15, 0, %0, c11, c1, 1" : "=r"(value));
	len += snprintf(p + len, buf_len - len,
			"Preload Engine Parameters Control: 0x%08x\n", value);

	/* c12 register */
	asm volatile("mrc p15, 0, %0, c12, c0, 0" : "=r"(value));
	len += snprintf(p + len, buf_len - len,
			"Vector Base Address: 0x%08x\n", value);

	asm volatile("mrc p15, 0, %0, c12, c0, 1" : "=r"(value));
	len += snprintf(p + len, buf_len - len,
			"Monitor Vector Base Address: 0x%08x\n", value);

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

	sscanf(buf, "%x", &offset);
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
	void __iomem *dist_base = (void __iomem *)GIC_DIST_VIRT_BASE;
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

#ifdef CONFIG_CACHE_L2X0

static int l2_offset = -1;

static ssize_t l2_write(struct file *filp, const char __user *buffer,
		size_t count, loff_t *ppos)
{
	char buf[32] = {0};
	int offset;

	/* copy user's input to kernel space */
	if (copy_from_user(buf, buffer, count))
		return -EFAULT;

	sscanf(buf, "%x", &offset);
	pr_info("Check L2 offset: 0x%x\n", offset);

	if (offset < 0 || offset > 0xffc)
		pr_err("The offset is out of L2 register range.\n");
	else if (offset % 4)
		pr_err("offset should be aligned to 4 bytes.\n");
	else
		l2_offset = offset;

	return count;

}

static ssize_t l2_read(struct file *filp, char __user *buffer,
		size_t count, loff_t *ppos)
{
	char buf[256];
	char *p = buf;
	size_t ret, buf_len;
	u32 value;
	int len = 0;

	buf_len = sizeof(buf) - 1;

	if (l2_offset != -1) {
		value = readl_relaxed(l2x0_base + l2_offset);
		len += snprintf(p + len, buf_len - len,
				"offset[0x%x]: 0x%08x\n", l2_offset, value);
	} else {
		value = readl_relaxed(l2x0_base + L2X0_AUX_CTRL);
		len += snprintf(p + len, buf_len - len,
				"Auxiliary Control: 0x%08x\n", value);

		value = readl_relaxed(l2x0_base + L2X0_TAG_LATENCY_CTRL);
		len += snprintf(p + len, buf_len - len,
				"Tag RAM Latency: 0x%08x\n", value);

		value = readl_relaxed(l2x0_base + L2X0_DATA_LATENCY_CTRL);
		len += snprintf(p + len, buf_len - len,
				"Data RAM Latency: 0x%08x\n", value);

		value = readl_relaxed(l2x0_base + L2X0_ADDR_FILTER_START);
		len += snprintf(p + len, buf_len - len,
				"Address filtering Start: 0x%08x\n", value);

		value = readl_relaxed(l2x0_base + L2X0_ADDR_FILTER_END);
		len += snprintf(p + len, buf_len - len,
				"Address filtering End: 0x%08x\n", value);

		value = readl_relaxed(l2x0_base + L2X0_PREFETCH_CTRL);
		len += snprintf(p + len, buf_len - len,
				"Prefetch Control: 0x%08x\n", value);

		value = readl_relaxed(l2x0_base + L2X0_POWER_CTRL);
		len += snprintf(p + len, buf_len - len,
				"Power Control: 0x%08x\n", value);

		value = readl_relaxed(l2x0_base + L2X0_CTRL);
		len += snprintf(p + len, buf_len - len, "L2 Cache: %s\n",
			(value & (1 << 0)) ? "Enabled" : "Disabled");
	}

	if (len == buf_len)
		pr_warn("The buffer for dumpping L2 is full now!\n");

	ret = simple_read_from_buffer(buffer, count, ppos, buf, len);
	if (l2_offset != -1 && !ret)
		l2_offset = -1;
	return ret;
}

const struct file_operations dumpregs_l2_fops = {
	.read = l2_read,
	.write = l2_write,
};
#endif

#ifdef CONFIG_HAVE_ARM_SCU

#define SCU_CTRL		0x00
#define SCU_CONFIG		0x04
#define SCU_CPU_STATUS		0x08

static int scu_offset = -1;

static ssize_t scu_write(struct file *filp, const char __user *buffer,
		size_t count, loff_t *ppos)
{
	char buf[32] = {0};
	int offset;

	/* copy user's input to kernel space */
	if (copy_from_user(buf, buffer, count))
		return -EFAULT;

	sscanf(buf, "%x", &offset);
	pr_info("Check SCU offset: 0x%x\n", offset);

	if (offset < 0 || offset > 0x54)
		pr_err("The offset is out of SCU register range.\n");
	else if (offset % 4)
		pr_err("offset should be aligned to 4 bytes.\n");
	else
		scu_offset = offset;

	return count;
}

static ssize_t scu_read(struct file *filp, char __user *buffer,
		size_t count, loff_t *ppos)
{
	char buf[256];
	char *p = buf;
	size_t ret, buf_len;
	u32 value;
	int len = 0;

	buf_len = sizeof(buf) - 1;

	if (scu_offset != -1) {
		value = readl_relaxed(SCU_VIRT_BASE + scu_offset);
		len += snprintf(p + len, buf_len - len,
				"offset[0x%x]: 0x%08x\n", scu_offset, value);
	} else {
		value = readl_relaxed(SCU_VIRT_BASE + SCU_CTRL);
		len += snprintf(p + len, buf_len - len,
				"SCU Control: 0x%08x\n", value);

		value = readl_relaxed(SCU_VIRT_BASE + SCU_CONFIG);
		len += snprintf(p + len, buf_len - len,
				"SCU Configuration: 0x%08x\n", value);

		value = readl_relaxed(SCU_VIRT_BASE + SCU_CPU_STATUS);
		len += snprintf(p + len, buf_len - len,
				"SCU CPU Power Status: 0x%08x\n", value);
	}

	if (len == buf_len)
		pr_warn("The buffer for dumpping SCU is full now!\n");

	ret = simple_read_from_buffer(buffer, count, ppos, buf, len);
	if (scu_offset != -1 && !ret)
		scu_offset = -1;
	return ret;
}

const struct file_operations dumpregs_scu_fops = {
	.read = scu_read,
	.write = scu_write,
};
#endif

struct dentry *pxa;

static int __init pxa_debugfs_init(void)
{
	struct dentry *dumpregs_cp15, *dumpregs_gic, *dumpregs_l2;
	struct dentry *dumpregs_scu;

	pxa = debugfs_create_dir("pxa", NULL);
	if (!pxa)
		return -ENOENT;

	dumpregs_cp15 = debugfs_create_file("cp15", 0664,
					pxa, NULL, &dumpregs_cp15_fops);
	if (!dumpregs_cp15)
		goto err_cp15;

#ifdef CONFIG_ARM_GIC
	dumpregs_gic = debugfs_create_file("gic_dist", 0664,
					pxa, NULL, &dumpregs_gic_fops);
	if (!dumpregs_gic)
		goto err_gic;
#endif

#ifdef CONFIG_CACHE_L2X0
	dumpregs_l2 = debugfs_create_file("l2", 0664,
					pxa, NULL, &dumpregs_l2_fops);
	if (!dumpregs_l2)
		goto err_l2;
#endif

#ifdef CONFIG_HAVE_ARM_SCU
	dumpregs_scu = debugfs_create_file("scu", 0664,
				pxa, NULL, &dumpregs_scu_fops);
	if (!dumpregs_scu)
		goto err_scu;
#endif

	return 0;

#ifdef CONFIG_HAVE_ARM_SCU
err_scu:
#endif

#ifdef CONFIG_CACHE_L2X0
	debugfs_remove(dumpregs_l2);
	dumpregs_l2 = NULL;
err_l2:
#endif

#ifdef CONFIG_ARM_GIC
	debugfs_remove(dumpregs_gic);
	dumpregs_gic = NULL;
#endif

#ifdef CONFIG_ARM_GIC
err_gic:
#endif
	debugfs_remove(dumpregs_cp15);
	dumpregs_cp15 = NULL;

err_cp15:
	debugfs_remove(pxa);
	pxa = NULL;

	return -ENOENT;
}

postcore_initcall(pxa_debugfs_init);
