/*
 * Copyright (c) 2015 Samsung Electronics Co., Ltd.
 *		http://www.samsung.com
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include "esca_drv.h"

void __iomem *fvmap_base_address;

void *get_fvmap_base(void)
{
	return fvmap_base_address;
}
EXPORT_SYMBOL_GPL(get_fvmap_base);

void esca_init_eint_clk_req(u32 eint_num)
{
	u32 pend;

	exynos_pmu_write(EXYNOS_PMU_SPARE7, 0);
	exynos_pmu_write(EXYNOS_PMU_SPARE7, (1 << 28) | (eint_num & 0xff));		//PMU_SPARE7
	exynos_pmu_write(EXYNOS_SPARE_CTRL__DATA7 | 0xc000, 7);		//SPARE_CTRL__DATA7
	do {
		exynos_pmu_read(EXYNOS_SPARE_CTRL__DATA7, &pend);
	} while (pend & (1 << 7));
}
EXPORT_SYMBOL_GPL(esca_init_eint_clk_req);

void esca_init_eint_nfc_clk_req(u32 eint_num)
{
	u32 pend;

	exynos_pmu_write(EXYNOS_PMU_SPARE7, 0);
	exynos_pmu_write(EXYNOS_PMU_SPARE7, (1 << 27) | (eint_num & 0xff));			//PMU_SPARE7
	exynos_pmu_write(EXYNOS_SPARE_CTRL__DATA7 | 0xc000, 7);		//SPARE_CTRL__DATA7
	do {
		exynos_pmu_read(EXYNOS_SPARE_CTRL__DATA7, &pend);
	} while (pend & (1 << 7));
}
EXPORT_SYMBOL_GPL(esca_init_eint_nfc_clk_req);

u32 esca_enable_flexpmu_profiler(u32 start)
{
	u32 pend;

	exynos_pmu_write(EXYNOS_PMU_SPARE7, 0);
	exynos_pmu_write(EXYNOS_PMU_SPARE7, (1 << 3) | start);			//PMU_SPARE7
	exynos_pmu_write(EXYNOS_SPARE_CTRL__DATA7 | 0xc000, 7);		//SPARE_CTRL__DATA7
	do {
		exynos_pmu_read(EXYNOS_SPARE_CTRL__DATA7, &pend);
	} while (pend & (1 << 7));
	return 0;
}
EXPORT_SYMBOL_GPL(esca_enable_flexpmu_profiler);

void get_plugin_dbg_addr(u32 esca_layer, u32 pid, u64 *sram_base, u64 *dbg_addr)
{
	struct esca_framework *framework;
	struct plugin *plugins;
	struct esca_info *esca;

	esca = exynos_esca[esca_layer];

	framework = esca->initdata;
	plugins = (struct plugin *)(esca->sram_base + framework->plugins);

	if (plugins[pid].dbg) {
		*sram_base = (u64)(esca->sram_base);
		*dbg_addr = (u64)(esca->sram_base + plugins[pid].dbg);
	}
	else {
		pr_err("[ESCA] plugin_dbg offset error!\n");
	}
}
EXPORT_SYMBOL_GPL(get_plugin_dbg_addr);
