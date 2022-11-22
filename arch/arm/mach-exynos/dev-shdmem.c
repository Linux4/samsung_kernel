/*
 * Copyright (c) 2013 Samsung Electronics Co., Ltd.
 *		http://www.samsung.com
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/kernel.h>
#include <linux/io.h>
#include <linux/dma-mapping.h>
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/vmalloc.h>

#include <mach/regs-pmu.h>
#include <mach/shdmem.h>
#include "dev-shdmem.h"

#if (CONFIG_EXYNOS_MEM_BASE == 0x80000000)
#define SHDMEM_BASE		0xA0000000
#else
#define SHDMEM_BASE		0x60000000
#endif

#define SHDMEM_SIZE		(100 * SZ_1M)	/* 100 MB */
#define BOOT_RGN_OFFSET		0
#define BOOT_RGN_SIZE		(8 * SZ_1K)	/* 8 KB */
#define IPC_RGN_OFFSET		(96 * SZ_1M)	/* 96 MB ~ */
#define IPC_RGN_SIZE		(4 * SZ_1M)	/* 4 MB */

#define MEMSIZE_OFFSET		24
#define MEMBASE_ADDR_OFFSET	14

#define CP_READ_EN		(0x1 << 2)
#define CP_WRITE_EN		(0x1 << 3)
#define CP_READ_EN_BY_SEM	(0x1 << 0)
#define CP_WRITE_EN_BY_SEM	(0x1 << 1)
#define CP_NO_ACCESS		0

enum access_region {
	ACC_CMU_MIF = 0,
	ACC_DFS_CTRL = 4,
	ACC_DREX_L = 8,
	ACC_DREX_R = 12,
	ACC_PMU_MIF = 16,
	ACC_TZASC_LW = 20,
	ACC_TZASC_LR = 24,
	ACC_ALL_DEV,
};

unsigned long shm_get_phys_base(void)
{
	return SHDMEM_BASE;
}
EXPORT_SYMBOL(shm_get_phys_base);

unsigned long shm_get_phys_size(void)
{
	return SHDMEM_SIZE;
}
EXPORT_SYMBOL(shm_get_phys_size);

unsigned long shm_get_boot_rgn_offset(void)
{
	return BOOT_RGN_OFFSET;
}
EXPORT_SYMBOL(shm_get_boot_rgn_offset);

unsigned long shm_get_boot_rgn_size(void)
{
	return BOOT_RGN_SIZE;
}
EXPORT_SYMBOL(shm_get_boot_rgn_size);

unsigned long shm_get_ipc_rgn_offset(void)
{
	return IPC_RGN_OFFSET;
}
EXPORT_SYMBOL(shm_get_ipc_rgn_offset);

unsigned long shm_get_ipc_rgn_size(void)
{
	return IPC_RGN_SIZE;
}
EXPORT_SYMBOL(shm_get_ipc_rgn_size);

void __iomem *shm_request_region(unsigned long phys_addr, unsigned long size)
{
	int i;
	struct page **pages;
	void __iomem *addr;
	unsigned long base = phys_addr;
	unsigned int num_pages = (size >> PAGE_SHIFT);
	pgprot_t prot = pgprot_noncached(PAGE_KERNEL);

	pages = kmalloc(num_pages * sizeof(*pages), GFP_KERNEL);
	for (i = 0; i < num_pages; i++) {
		pages[i] = phys_to_page(base);
		base += PAGE_SIZE;
	}

	addr = (void __iomem *)vmap(pages, num_pages, VM_MAP, prot);
	kfree(pages);

	return addr;
}
EXPORT_SYMBOL(shm_request_region);

void shm_release_region(void *rgn)
{
	vunmap(rgn);
}
EXPORT_SYMBOL(shm_release_region);

#ifndef CONFIG_SEC_CP_SECURE_BOOT
static void __init set_shdmem_size(unsigned int bytes)
{
	unsigned int mwords = bytes >> 22;	/* Mega-words*/
	unsigned int tmp;

	pr_err("mif: %s: size %dMB\n", __func__, (bytes >> 20));

	tmp = readl(EXYNOS3470_CP2AP_MEM_CFG);

	tmp &= ~(0xff << MEMSIZE_OFFSET);
	tmp |= (mwords << MEMSIZE_OFFSET);

	writel(tmp, EXYNOS3470_CP2AP_MEM_CFG);
}

static void __init set_shdmem_base(unsigned int addr)
{
	unsigned int tag = addr >> 22;
	unsigned int tmp;

	pr_err("mif: %s: base 0x%x\n", __func__, addr);

	tmp = readl(EXYNOS3470_CP2AP_MEM_CFG);

	tmp &= ~(0x3ff << MEMBASE_ADDR_OFFSET);
	tmp |= (tag << MEMBASE_ADDR_OFFSET);

	writel(tmp, EXYNOS3470_CP2AP_MEM_CFG);
}

static void __init set_access_win(enum access_region acc_region, u32 val)
{
	u32 reg_val;

	if (acc_region == ACC_ALL_DEV) {
		reg_val = (val << ACC_TZASC_LR) | (val << ACC_TZASC_LW) |
			  (val << ACC_PMU_MIF) | (val << ACC_DREX_L) |
			  (val << ACC_DREX_R) | (val <<  ACC_DFS_CTRL) |
			  (val << ACC_CMU_MIF);
	} else {
		reg_val = readl(EXYNOS3470_CP2AP_PERI_ACC_WIN);
		reg_val &= ~(0xf << acc_region);
		reg_val |= (val << acc_region);
	}

	writel(reg_val, EXYNOS3470_CP2AP_PERI_ACC_WIN);
}
#endif

int __init init_exynos_dev_shdmem(void)
{
	int err;
	pr_err("mif: %s: +++\n", __func__);

#ifndef CONFIG_SEC_CP_SECURE_BOOT
	set_shdmem_size(SHDMEM_SIZE);
	set_shdmem_base(SHDMEM_BASE);
	set_access_win(ACC_PMU_MIF, CP_READ_EN | CP_WRITE_EN);
	set_access_win(ACC_CMU_MIF, CP_READ_EN | CP_WRITE_EN);
	set_access_win(ACC_DREX_L, CP_READ_EN | CP_WRITE_EN);
	set_access_win(ACC_DREX_R, CP_READ_EN | CP_WRITE_EN);
#endif

	err = init_exynos_dev_mcu_ipc();
	if (err) {
		pr_err("mif: %s: ERR! init_exynos_dev_mcu_ipc fail (err %d)\n",
			__func__, err);
		goto error;
	}

	pr_err("mif: %s: ---\n", __func__);
	return 0;

error:
	pr_err("mif: %s: xxx\n", __func__);
	return err;
}
EXPORT_SYMBOL(init_exynos_dev_shdmem);

MODULE_DESCRIPTION("Exynos Shared-Memory Device for AP/CP Interface");
MODULE_AUTHOR("Hankook Jang <hankook.jang@samsung.com>");
MODULE_LICENSE("GPL");
