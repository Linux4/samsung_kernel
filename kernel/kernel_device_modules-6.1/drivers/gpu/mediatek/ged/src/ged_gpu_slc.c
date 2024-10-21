// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2023 MediaTek Inc.
 */

#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/init.h>
#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/of_address.h>

#include <ged_type.h>
#include <ged_base.h>
#include <ged_gpu_slc.h>


#if defined(MTK_GPU_SLC_POLICY)
static unsigned int g_sysram_support;
static phys_addr_t g_counter_pa, g_counter_va;
static unsigned int g_counter_size;
static struct gpu_slc_stat *g_slc_stat;

static GED_ERROR gpu_slc_sysram_init(void)
{
	struct resource res = {};
	struct device_node *node = NULL;
	phys_addr_t counter_pa = 0, counter_va = 0, counter_size = 0;

	/* get GPU SLC pre-defined device node from dts */
	node = of_find_compatible_node(NULL, NULL, "mediatek,gpu_slc");
	if (unlikely(!node)) {
		GED_LOGE("[GPU_SLC]%s Cannot find gpu slc dts node", __func__);
		return GED_OK;
	}

	/* check if sysram support by getting slc-sysram-support property */
	of_property_read_u32(node, "slc-sysram-support", &g_sysram_support);
	if (unlikely(!g_sysram_support)) {
		GED_LOGI("[GPU_SLC]%s sysram not support", __func__);
		return GED_OK;
	}

	/* get sysram address from "reg" property then translate into a resource */
	if (unlikely(of_address_to_resource(node, 0, &res))) {
		GED_LOGE("[GPU_SLC]%s Cannot get physical memory addr", __func__);
		return GED_OK;
	}
	counter_pa = res.start;
	counter_size = resource_size(&res);
	/* Transfer physical addr to virtual addr */
	counter_va = (phys_addr_t)(size_t)ioremap_wc(counter_pa, counter_size);

	g_counter_pa = counter_pa;
	g_counter_va = counter_va;
	g_counter_size = counter_size;
	g_slc_stat = (struct gpu_slc_stat *)(uintptr_t)g_counter_va;

	/* release reference count for node using */
	of_node_put(node);

	/* clear the buffer*/
	memset((void *)g_counter_va, 0, g_counter_size);

	GED_LOGI("[GPU_SLC]%s sysram phys_addr: 0x%llx, virt_addr: 0x%llx, size: %x",
		__func__, g_counter_pa, g_counter_va, g_counter_size);
	GED_LOGI("[GPU_SLC]%s slc sysram usage from 0x%llx ~ 0x%llx size: %x",
		__func__, res.start, res.end, resource_size(&res));

	return GED_OK;
}


struct gpu_slc_stat *get_gpu_slc_stat(void)
{
	if (likely(g_slc_stat))
		return g_slc_stat;
	GED_LOGI("[GPU_SLC]%s null g_slc_stat\n", __func__);
	return NULL;
}

GED_ERROR ged_gpu_slc_init(void)
{
	int ret = 0;

	/* init sysram usage */
	ret = gpu_slc_sysram_init();

	if (unlikely(ret)) {
		GED_LOGE("[GPU_SLC]%s gpu_slc_sysram_init failed.", __func__);
		return GED_OK;
	}

	if (g_slc_stat) {
		g_slc_stat->mode = 15;
		g_slc_stat->policy_0_hit_rate_r = 0;
		g_slc_stat->policy_1_hit_rate_r = 0;
		g_slc_stat->policy_2_hit_rate_r = 0;
		g_slc_stat->policy_3_hit_rate_r = 0;
		g_slc_stat->policy = 0;
		g_slc_stat->hit_rate_r = 0;
		g_slc_stat->isoverflow = 0;
		GED_LOGI("[GPU_SLC] %s get slc stat successfully.",  __func__);
		GED_LOGI("[GPU_SLC] mode: %d, ptr: %x, addr: %x\n", g_slc_stat->mode, &g_slc_stat->mode, &g_slc_stat);
	} else {
		GED_LOGE("[GPU_SLC]%s null slc stat", __func__);
	}

	return GED_OK;
}

void ged_gpu_slc_dynamic_mode(unsigned int idx)
{
	/* clear and change mode*/
	g_slc_stat->mode = idx;
	g_slc_stat->policy_0_hit_rate_r = 0;
	g_slc_stat->policy_1_hit_rate_r = 0;
	g_slc_stat->policy_2_hit_rate_r = 0;
	g_slc_stat->policy_3_hit_rate_r = 0;
	g_slc_stat->policy = 0;
	g_slc_stat->hit_rate_r = 0;
	g_slc_stat->isoverflow = 0;
}

GED_ERROR ged_gpu_slc_exit(void)
{
	/*Do Nothing*/
	return GED_OK;
	}
#endif /* MTK_GPU_SLC_POLICY */
