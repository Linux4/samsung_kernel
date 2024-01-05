/*
 * Copyright (c) 2013 Samsung Electronics Co., Ltd.
 *		http://www.samsung.com
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/gpio.h>
#include <linux/io.h>
#include <linux/module.h>
#include <linux/platform_data/modemif.h>

#include <plat/gpio-cfg.h>
#include <plat/cpu.h>
#include <plat/clock.h>
#include <plat/devs.h>

#include <mach/regs-pmu.h>

#if (CONFIG_EXYNOS_MEM_BASE == 0x80000000)
#define SHDMEM_BASE 0xA0000000
#else
#define SHDMEM_BASE 0x60000000
#endif

#define IPCMEM_OFFSET 96 * SZ_1M
#define SHDMEM_SIZE 100
#define IPCMEM_SIZE 4

#define MEMSIZE_OFFSET 24
#define MEMBASE_ADDR_OFFSET 14

#define CP_READ_EN (0x1 << 2)
#define CP_WRITE_EN (0x1 << 3)
#define CP_READ_EN_BY_SEM (0x1 << 0)
#define CP_WRITE_EN_BY_SEM (0x1 << 1)
#define CP_NO_ACCESS 0

#define CP_SECURE_BOOT

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

enum modemif_cp_mem_size {
	MODEM_MEMSZ_4M = 1,
	MODEM_MEMSZ_8M = 2,
	MODEM_MEMSZ_16M = 4,
	MODEM_MEMSZ_32M = 8,
	MODEM_MEMSZ_64M = 16,
	MODEM_MEMSZ_96M = 24,
	MODEM_MEMSZ_128M = 32,
	MODEM_MEMSZ_132M = 33,
};

unsigned long shm_get_phys_base(void)
{
        return SHDMEM_BASE;
}
EXPORT_SYMBOL(shm_get_phys_base);

unsigned long shm_get_phys_size(void)
{
        return SHDMEM_SIZE * SZ_1M;
}
EXPORT_SYMBOL(shm_get_phys_size);

unsigned long shm_get_ipc_rgn_offset(void)
{
        return IPCMEM_OFFSET;
}
EXPORT_SYMBOL(shm_get_ipc_rgn_offset);

unsigned long shm_get_ipc_rgn_size(void)
{
        return IPCMEM_SIZE * SZ_1M;
}
EXPORT_SYMBOL(shm_get_ipc_rgn_size);

#ifndef CP_SECURE_BOOT
static void __init set_shdmem_size(int memsz)
{
	unsigned int tmp;
	pr_info("[Modem_IF]Set shared mem size : %dMB\n", memsz);

	memsz /= 4;
	tmp = readl(EXYNOS3470_CP2AP_MEM_CFG);

	tmp &= ~(0xff << MEMSIZE_OFFSET);
	tmp |= (memsz << MEMSIZE_OFFSET);

	writel(tmp, EXYNOS3470_CP2AP_MEM_CFG);
}

static void __init set_shdmem_base(void)
{
	unsigned int tmp, base_addr;
	pr_info("[Modem_IF]Set shared mem baseaddr : 0x%x\n", SHDMEM_BASE);

	base_addr = (SHDMEM_BASE >> 22);

	tmp = readl(EXYNOS3470_CP2AP_MEM_CFG);

	tmp &= ~(0x3ff << MEMBASE_ADDR_OFFSET);
	tmp |= (base_addr << MEMBASE_ADDR_OFFSET);

	writel(tmp, EXYNOS3470_CP2AP_MEM_CFG);
}

static void __init set_access_win(enum access_region acc_region, u32 val)
{
	u32 reg_val;

	if (acc_region == ACC_ALL_DEV) {
		reg_val = (val << ACC_TZASC_LR) | (val << ACC_TZASC_LW) |
			(val << ACC_PMU_MIF) | (val << ACC_DREX_L) | (val << ACC_DREX_R) |
			(val <<  ACC_DFS_CTRL) | (val << ACC_CMU_MIF);
	} else {
		reg_val = readl(EXYNOS3470_CP2AP_PERI_ACC_WIN);
		reg_val &= ~(0xf << acc_region);
		reg_val |= (val << acc_region);
	}

	writel(reg_val, EXYNOS3470_CP2AP_PERI_ACC_WIN);
}
#endif

static struct platform_device *smdk4270_modem_if_devices[] __initdata = {
#ifdef CONFIG_EXYNOS4_DEV_MCU_IPC
	&exynos4_device_mcu_ipc,
#endif
};

void __init exynos4_smdk4270_modemif_init(void)
{
	pr_debug("[Modem_IF] modem interface initialization.\n");
#ifndef CP_SECURE_BOOT
	/* Set default modem configuration */
	set_shdmem_size(SHDMEM_SIZE);
	set_shdmem_base();
	set_access_win(ACC_PMU_MIF, CP_READ_EN | CP_WRITE_EN);
	set_access_win(ACC_CMU_MIF, CP_READ_EN | CP_WRITE_EN);
	set_access_win(ACC_DREX_L, CP_READ_EN | CP_WRITE_EN);
	set_access_win(ACC_DREX_R, CP_READ_EN | CP_WRITE_EN);
#endif

	platform_add_devices(smdk4270_modem_if_devices,
			ARRAY_SIZE(smdk4270_modem_if_devices));
}
