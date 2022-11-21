/*
*  arch/arm/plat-pxa/gpu_mem.c
*
*  GPU reserved memory management
*
*  This program is free software; you can redistribute it and/or modify
*  it under the terms of the GNU General Public License version 2 as
*  published by the Free Software Foundation.
*
*  (C) Copyright 2011 Marvell International Ltd.
*  All Rights Reserved
*/
#include <linux/kernel.h>
#include <linux/version.h>
#include <linux/module.h>
#include <linux/miscdevice.h>
#include <linux/slab.h>
#include <linux/platform_device.h>
#include <linux/dma-mapping.h>
#include <linux/memblock.h>
#include <mach/gpu_mem.h>

#ifdef CONFIG_ARCH_MMP
#include <mach/cputype.h>
#elif defined(CONFIG_ARCH_PXA)
#include <mach/hardware.h>
#endif

#define DEVICE_NAME_GC	   "galcore"

#define MAKE_GPU_PARAM_X(plat, irq, regBase, regSize, memSize, memBase) \
{							   \
	.name = plat,			   \
	.irqLine = irq,			 \
	.registerMemBase = regBase, \
	.registerMemSize = regSize, \
	.contiguousSize  = memSize, \
	.contiguousBase  = memBase, \
}

struct gpu_param_table {
	const char *name;
	int irqLine;
	ulong registerMemBase;
	ulong registerMemSize;
	long contiguousSize;
	ulong contiguousBase;
};

static struct gpu_param_table gpu_params[] = {
	/* dkbttc, dkbtd */
	MAKE_GPU_PARAM_X("ttctd", 8, 0xc0400000, 0x40000, 0x2000000, 0x0),
	/* brownstone, g50 */
	MAKE_GPU_PARAM_X("mmp2", 8, 0xd420d000, 0x40000, 0x4000000, 0x0),
	/* abilene */
	MAKE_GPU_PARAM_X("mmp3", 137, 0xd420d000, 0x40000, 0x2000000, 0x0),
	/* 988 */
	MAKE_GPU_PARAM_X("988", 40, 0xC0400000, 0x2000, 0x4000000, 0x0),
	/* mg1, mg2, nevo */
	MAKE_GPU_PARAM_X("mg", 70, 0x54000000, 0x40000, 0x2000000, 0x0),
	/*1x88: 1T88/1L88*/
	MAKE_GPU_PARAM_X("1x88", 40, 0xC0400000, 0x1000, 0x4000000, 0x0),
};

static struct resource gpu_resources[] = {
	{
		.name = "gpu_irq",
		.flags = IORESOURCE_IRQ,
	},
	{
		.name = "gpu_base",
		.flags = IORESOURCE_MEM,
	},
	{
		.name   = "gpu_mem",
		.flags  = IORESOURCE_MEM,
	},
};

/*default reserve size:16MB*/
static size_t  __initdata gpu_reserve_size = 0x1000000;
static phys_addr_t __initdata gpu_reserve_pa;

static int __init pxa_reserve_gpu_early_init(char *arg)
{
	gpu_reserve_size = memparse(arg, &arg);
	return 0;
}
early_param("reserve_gpu", pxa_reserve_gpu_early_init);

static int __init __prepare_gpu_resources(void)
{
	int index = -1;
	ulong memSize = 0;

#ifdef CONFIG_ARCH_MMP
	if (cpu_is_pxa910()) {
		index = 0;
	} else if (cpu_is_mmp2()) {
		index = 1;
	} else if (cpu_is_mmp3()) {
		index = 2;
#ifdef CONFIG_CPU_PXA988
	} else if (cpu_is_pxa988() || cpu_is_pxa986()) {
		index = 3;
#endif
#ifdef CONFIG_CPU_PXA1088
	} else if (cpu_is_pxa1088()) {
		index = 5;
#endif
#ifdef CONFIG_CPU_PXA1L88
	} else if (cpu_is_pxa1L88()) {
		index = 5;
#endif
#elif defined(CONFIG_ARCH_PXA)
	if (cpu_is_pxa95x()) {
		index = 4;
#endif
	} else {
		pr_err("%s: can't recognize chip!\n", __func__);
		return -ENODEV;
	}

	memSize = gpu_reserve_size;

	gpu_resources[0].start = gpu_params[index].irqLine;
	gpu_resources[0].end   = gpu_params[index].irqLine;

	gpu_resources[1].start = gpu_params[index].registerMemBase;
	gpu_resources[1].end   = gpu_params[index].registerMemBase +
				gpu_params[index].registerMemSize - 1;
	gpu_resources[2].start = gpu_reserve_pa;
	gpu_resources[2].end   = gpu_reserve_pa + gpu_reserve_size - 1;

	return 0;
}

static int __init __pxa_add_gpu(
			const char *name,
			size_t size,
			int cached,
			int buffered
)
{
	int ret = 0;
	struct platform_device *gpu_device;

	ret = __prepare_gpu_resources();
	if (ret) {
		pr_err("galcore: initialize gpu_resources failed.\n");
		goto out;
	}

	/* Allocate device */
	gpu_device = platform_device_alloc(name, -1);
	if (!gpu_device) {
		pr_err("galcore: platform_device_alloc failed.\n");
		ret = -ENOMEM;
		goto out;
	}

	/* Insert resource */
	ret = platform_device_add_resources(gpu_device, gpu_resources, 3);
	if (ret) {
		pr_err("galcore: platform_device_add_resources failed.\n");
		goto put_dev;
	}

	/* Add device */
	ret = platform_device_add(gpu_device);
	if (ret) {
		pr_err("galcore: platform_device_add failed.\n");
		goto del_dev;
	}

	goto out;

del_dev:
	platform_device_del(gpu_device);
put_dev:
	platform_device_put(gpu_device);

out:
	return ret;
}

void __init pxa_reserve_gpu_memblock(void)
{
	if (!gpu_reserve_size) {
		pr_err("%s: gpu reserve size is 0\n", __func__);
		return;
	}

	gpu_reserve_pa = memblock_alloc(gpu_reserve_size, PAGE_SIZE);
	if (!gpu_reserve_pa) {
		pr_err("%s: failed to reserve %dMB\n",
			   __func__, (unsigned)gpu_reserve_size/0x100000);
		return;
	}

	pr_info("Reserved GC memory: %dMB at %#.8x\n",
		   (unsigned)gpu_reserve_size/0x100000,
		   (unsigned)gpu_reserve_pa);
}

void __init pxa_add_gpu(void)
{
	__pxa_add_gpu(DEVICE_NAME_GC, gpu_reserve_size, 0, 0);
}
