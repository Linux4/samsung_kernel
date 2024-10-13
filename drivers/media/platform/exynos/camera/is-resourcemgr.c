/*
 * Samsung Exynos5 SoC series FIMC-IS driver
 *
 * exynos5 fimc-is video functions
 *
 * Copyright (c) 2011 Samsung Electronics Co., Ltd
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/memblock.h>
#include <linux/slab.h>
#include <linux/irq.h>
#include <linux/interrupt.h>
#include <linux/delay.h>
#include <linux/version.h>
#include <linux/gpio.h>
#include <linux/clk.h>
#include <linux/regulator/consumer.h>
#include <linux/videodev2.h>
#include <videodev2_exynos_camera.h>
#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/platform_device.h>
#include <media/v4l2-ctrls.h>
#include <media/v4l2-device.h>
#include <media/v4l2-subdev.h>
#if IS_ENABLED(CONFIG_ARM_FREQ_QOS_TRACER)
#include <soc/samsung/freq-qos-tracer.h>
#else
#include <linux/pm_qos.h>
#include <linux/cpufreq.h>
#endif
#if IS_ENABLED(CONFIG_EXYNOS_THERMAL) || IS_ENABLED(CONFIG_EXYNOS_THERMAL_V2)
#include <soc/samsung/tmu.h>
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(5, 4, 0))
#include <soc/samsung/isp_cooling.h>
#else
#include <linux/isp_cooling.h>
#endif
#endif
#include <linux/cpuidle.h>
#if IS_ENABLED(CONFIG_CMU_EWF)
#include <soc/samsung/cmu_ewf.h>
#endif
#include <linux/notifier.h>

#include <linux/of_fdt.h>
#include <linux/of_reserved_mem.h>

#include "exynos-is.h"
#include "is-resourcemgr.h"
#include "is-hw.h"
#include "pablo-debug.h"
#include "is-core.h"
#include "is-dvfs.h"
#include "is-interface-library.h"
#include "votf/pablo-votf.h"
#include "is-device-camif-dma.h"
#include "pablo-smc.h"
#include "pablo-lib.h"
#include "is-device-csi.h"
#include "is-interface-wrap.h"

#define CLUSTER_MIN_MASK			0x0000FFFF
#define CLUSTER_MIN_SHIFT			0
#define CLUSTER_MAX_MASK			0xFFFF0000
#define CLUSTER_MAX_SHIFT			16

#if IS_ENABLED(CONFIG_CMU_EWF)
static unsigned int idx_ewf;
#endif

static struct is_pm_qos_request exynos_isp_pm_qos[IS_PM_QOS_MAX];
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(5, 4, 0))
static struct freq_qos_request exynos_isp_freq_qos[IS_FREQ_QOS_MAX];
#endif

#if (LINUX_VERSION_CODE >= KERNEL_VERSION(5, 4, 0))
#define IS_GET_CPU_QOS(cpu)										\
	(cpufreq_cpu_get(cpu) ? &(cpufreq_cpu_get(cpu)->constraints) : NULL)

#if IS_ENABLED(CONFIG_ARM_FREQ_QOS_TRACER)
#define C0MIN_QOS_ADD(freq)										\
	do {												\
		struct pablo_lib_platform_data *plpd = pablo_lib_get_platform_data();			\
		freq_qos_tracer_add_request(IS_GET_CPU_QOS(plpd->cpu_cluster[PABLO_CPU_CLUSTER_LIT]),	\
			&exynos_isp_freq_qos[IS_FREQ_QOS_CLUSTER0_MIN], FREQ_QOS_MIN, freq * 1000);	\
	} while (0)
#define C0MIN_QOS_DEL()											\
	freq_qos_tracer_remove_request(&exynos_isp_freq_qos[IS_FREQ_QOS_CLUSTER0_MIN])
#define C0MIN_QOS_UPDATE(freq)										\
	freq_qos_update_request(&exynos_isp_freq_qos[IS_FREQ_QOS_CLUSTER0_MIN], freq * 1000)
#define C0MAX_QOS_ADD(freq)										\
	do {												\
		struct pablo_lib_platform_data *plpd = pablo_lib_get_platform_data();			\
		freq_qos_tracer_add_request(IS_GET_CPU_QOS(plpd->cpu_cluster[PABLO_CPU_CLUSTER_LIT]),	\
			&exynos_isp_freq_qos[IS_FREQ_QOS_CLUSTER0_MAX], FREQ_QOS_MAX, freq * 1000);	\
	} while (0)
#define C0MAX_QOS_DEL()											\
	freq_qos_tracer_remove_request(&exynos_isp_freq_qos[IS_FREQ_QOS_CLUSTER0_MAX])
#define C0MAX_QOS_UPDATE(freq)										\
	freq_qos_update_request(&exynos_isp_freq_qos[IS_FREQ_QOS_CLUSTER0_MAX], freq * 1000)
#define C1MIN_QOS_ADD(freq)										\
	do {												\
		struct pablo_lib_platform_data *plpd = pablo_lib_get_platform_data();			\
		freq_qos_tracer_add_request(IS_GET_CPU_QOS(plpd->cpu_cluster[PABLO_CPU_CLUSTER_MID]),	\
			&exynos_isp_freq_qos[IS_FREQ_QOS_CLUSTER1_MIN], FREQ_QOS_MIN, freq * 1000);	\
	} while (0)
#define C1MIN_QOS_DEL()											\
	freq_qos_tracer_remove_request(&exynos_isp_freq_qos[IS_FREQ_QOS_CLUSTER1_MIN])
#define C1MIN_QOS_UPDATE(freq)										\
	freq_qos_update_request(&exynos_isp_freq_qos[IS_FREQ_QOS_CLUSTER1_MIN], freq * 1000)
#define C1MAX_QOS_ADD(freq)										\
	do {												\
		struct pablo_lib_platform_data *plpd = pablo_lib_get_platform_data();			\
		freq_qos_tracer_add_request(IS_GET_CPU_QOS(plpd->cpu_cluster[PABLO_CPU_CLUSTER_MID]),	\
			&exynos_isp_freq_qos[IS_FREQ_QOS_CLUSTER1_MAX], FREQ_QOS_MAX, freq * 1000);	\
	} while (0)
#define C1MAX_QOS_DEL()											\
	freq_qos_tracer_remove_request(&exynos_isp_freq_qos[IS_FREQ_QOS_CLUSTER1_MAX])
#define C1MAX_QOS_UPDATE(freq)										\
	freq_qos_update_request(&exynos_isp_freq_qos[IS_FREQ_QOS_CLUSTER1_MAX], freq * 1000)
#define C2MIN_QOS_ADD(freq)										\
	do {												\
		struct pablo_lib_platform_data *plpd = pablo_lib_get_platform_data();			\
		freq_qos_tracer_add_request(IS_GET_CPU_QOS(plpd->cpu_cluster[PABLO_CPU_CLUSTER_BIG]),	\
			&exynos_isp_freq_qos[IS_FREQ_QOS_CLUSTER2_MIN], FREQ_QOS_MIN, freq * 1000);	\
	} while (0)
#define C2MIN_QOS_DEL()											\
	freq_qos_tracer_remove_request(&exynos_isp_freq_qos[IS_FREQ_QOS_CLUSTER2_MIN])
#define C2MIN_QOS_UPDATE(freq)										\
	freq_qos_update_request(&exynos_isp_freq_qos[IS_FREQ_QOS_CLUSTER2_MIN], freq * 1000)
#define C2MAX_QOS_ADD(freq)										\
	do {												\
		struct pablo_lib_platform_data *plpd = pablo_lib_get_platform_data();			\
		freq_qos_tracer_add_request(IS_GET_CPU_QOS(plpd->cpu_cluster[PABLO_CPU_CLUSTER_BIG]),	\
			&exynos_isp_freq_qos[IS_FREQ_QOS_CLUSTER2_MAX], FREQ_QOS_MAX, freq * 1000);	\
	} while (0)
#define C2MAX_QOS_DEL()											\
	freq_qos_tracer_remove_request(&exynos_isp_freq_qos[IS_FREQ_QOS_CLUSTER2_MAX])
#define C2MAX_QOS_UPDATE(freq)										\
	freq_qos_update_request(&exynos_isp_freq_qos[IS_FREQ_QOS_CLUSTER2_MAX], freq * 1000)
#else
#define C0MIN_QOS_ADD(freq)										\
	do {												\
		struct pablo_lib_platform_data *plpd = pablo_lib_get_platform_data();			\
		freq_qos_add_request(IS_GET_CPU_QOS(plpd->cpu_cluster[PABLO_CPU_CLUSTER_LIT]),		\
			&exynos_isp_freq_qos[IS_FREQ_QOS_CLUSTER0_MIN], FREQ_QOS_MIN, freq * 1000);	\
	} while (0)
#define C0MIN_QOS_DEL() freq_qos_remove_request(&exynos_isp_freq_qos[IS_FREQ_QOS_CLUSTER0_MIN])
#define C0MIN_QOS_UPDATE(freq)										\
	freq_qos_update_request(&exynos_isp_freq_qos[IS_FREQ_QOS_CLUSTER0_MIN], freq * 1000)
#define C0MAX_QOS_ADD(freq)										\
	do {												\
		struct pablo_lib_platform_data *plpd = pablo_lib_get_platform_data();			\
		freq_qos_add_request(IS_GET_CPU_QOS(plpd->cpu_cluster[PABLO_CPU_CLUSTER_LIT]),		\
			&exynos_isp_freq_qos[IS_FREQ_QOS_CLUSTER0_MAX], FREQ_QOS_MAX, freq * 1000);	\
	} while (0)
#define C0MAX_QOS_DEL() freq_qos_remove_request(&exynos_isp_freq_qos[IS_FREQ_QOS_CLUSTER0_MAX])
#define C0MAX_QOS_UPDATE(freq)										\
	freq_qos_update_request(&exynos_isp_freq_qos[IS_FREQ_QOS_CLUSTER0_MAX], freq * 1000)
#define C1MIN_QOS_ADD(freq)										\
	do {												\
		struct pablo_lib_platform_data *plpd = pablo_lib_get_platform_data();			\
		freq_qos_add_request(IS_GET_CPU_QOS(plpd->cpu_cluster[PABLO_CPU_CLUSTER_MID]),		\
			&exynos_isp_freq_qos[IS_FREQ_QOS_CLUSTER1_MIN], FREQ_QOS_MIN, freq * 1000);	\
	} while (0)
#define C1MIN_QOS_DEL() freq_qos_remove_request(&exynos_isp_freq_qos[IS_FREQ_QOS_CLUSTER1_MIN])
#define C1MIN_QOS_UPDATE(freq)										\
	freq_qos_update_request(&exynos_isp_freq_qos[IS_FREQ_QOS_CLUSTER1_MIN], freq * 1000)
#define C1MAX_QOS_ADD(freq)										\
	do {												\
		struct pablo_lib_platform_data *plpd = pablo_lib_get_platform_data();			\
		freq_qos_add_request(IS_GET_CPU_QOS(plpd->cpu_cluster[PABLO_CPU_CLUSTER_MID]),		\
			&exynos_isp_freq_qos[IS_FREQ_QOS_CLUSTER1_MAX], FREQ_QOS_MAX, freq * 1000);	\
	} while (0)
#define C1MAX_QOS_DEL() freq_qos_remove_request(&exynos_isp_freq_qos[IS_FREQ_QOS_CLUSTER1_MAX])
#define C1MAX_QOS_UPDATE(freq)										\
	freq_qos_update_request(&exynos_isp_freq_qos[IS_FREQ_QOS_CLUSTER1_MAX], freq * 1000)
#define C2MIN_QOS_ADD(freq)										\
	do {												\
		struct pablo_lib_platform_data *plpd = pablo_lib_get_platform_data();			\
		freq_qos_add_request(IS_GET_CPU_QOS(plpd->cpu_cluster[PABLO_CPU_CLUSTER_BIG]),		\
			&exynos_isp_freq_qos[IS_FREQ_QOS_CLUSTER2_MIN], FREQ_QOS_MIN, freq * 1000);	\
	} while (0)
#define C2MIN_QOS_DEL() freq_qos_remove_request(&exynos_isp_freq_qos[IS_FREQ_QOS_CLUSTER2_MIN])
#define C2MIN_QOS_UPDATE(freq)										\
	freq_qos_update_request(&exynos_isp_freq_qos[IS_FREQ_QOS_CLUSTER2_MIN], freq * 1000)
#define C2MAX_QOS_ADD(freq)										\
	do {												\
		struct pablo_lib_platform_data *plpd = pablo_lib_get_platform_data();			\
		freq_qos_add_request(IS_GET_CPU_QOS(plpd->cpu_cluster[PABLO_CPU_CLUSTER_BIG]),		\
			&exynos_isp_freq_qos[IS_FREQ_QOS_CLUSTER2_MAX], FREQ_QOS_MAX, freq * 1000);	\
	} while (0)
#define C2MAX_QOS_DEL()											\
	freq_qos_remove_request(&exynos_isp_freq_qos[IS_FREQ_QOS_CLUSTER2_MAX])
#define C2MAX_QOS_UPDATE(freq)										\
	freq_qos_update_request(&exynos_isp_freq_qos[IS_FREQ_QOS_CLUSTER2_MAX], freq * 1000)
#endif /* #if IS_ENABLED(CONFIG_ARM_FREQ_QOS_TRACER) */
#else
#define C0MIN_QOS_ADD(freq)									\
	is_pm_qos_add_request(&exynos_isp_pm_qos[IS_PM_QOS_CLUSTER0_MIN],			\
		PM_QOS_CLUSTER0_FREQ_MIN, freq * 1000)
#define C0MIN_QOS_DEL() is_pm_qos_remove_request(&exynos_isp_pm_qos[IS_PM_QOS_CLUSTER0_MIN])
#define C0MIN_QOS_UPDATE(freq)									\
	is_pm_qos_update_request(&exynos_isp_pm_qos[IS_PM_QOS_CLUSTER0_MIN], freq * 1000)
#define C0MAX_QOS_ADD(freq)									\
	is_pm_qos_add_request(&exynos_isp_pm_qos[IS_PM_QOS_CLUSTER0_MAX],			\
		PM_QOS_CLUSTER0_FREQ_MAX, freq * 1000)
#define C0MAX_QOS_DEL() is_pm_qos_remove_request(&exynos_isp_pm_qos[IS_PM_QOS_CLUSTER0_MAX])
#define C0MAX_QOS_UPDATE(freq)									\
	is_pm_qos_update_request(&exynos_isp_pm_qos[IS_PM_QOS_CLUSTER0_MAX], freq * 1000)
#define C1MIN_QOS_ADD(freq)									\
	is_pm_qos_add_request(&exynos_isp_pm_qos[IS_PM_QOS_CLUSTER1_MIN],			\
		PM_QOS_CLUSTER1_FREQ_MIN, freq * 1000)
#define C1MIN_QOS_DEL() is_pm_qos_remove_request(&exynos_isp_pm_qos[IS_PM_QOS_CLUSTER1_MIN])
#define C1MIN_QOS_UPDATE(freq)									\
	is_pm_qos_update_request(&exynos_isp_pm_qos[IS_PM_QOS_CLUSTER1_MIN], freq * 1000)
#define C1MAX_QOS_ADD(freq)									\
	is_pm_qos_add_request(&exynos_isp_pm_qos[IS_PM_QOS_CLUSTER1_MAX],			\
		PM_QOS_CLUSTER1_FREQ_MAX, freq * 1000)
#define C1MAX_QOS_DEL() is_pm_qos_remove_request(&exynos_isp_pm_qos[IS_PM_QOS_CLUSTER1_MAX])
#define C1MAX_QOS_UPDATE(freq)									\
	is_pm_qos_update_request(&exynos_isp_pm_qos[IS_PM_QOS_CLUSTER1_MAX], freq * 1000)
#define C2MIN_QOS_ADD(freq)									\
	is_pm_qos_add_request(&exynos_isp_pm_qos[IS_PM_QOS_CLUSTER2_MIN], 			\
		PM_QOS_CLUSTER2_FREQ_MIN, freq * 1000)
#define C2MIN_QOS_DEL() is_pm_qos_remove_request(&exynos_isp_pm_qos[IS_PM_QOS_CLUSTER2_MIN])
#define C2MIN_QOS_UPDATE(freq)									\
	is_pm_qos_update_request(&exynos_isp_pm_qos[IS_PM_QOS_CLUSTER2_MIN], freq * 1000)
#define C2MAX_QOS_ADD(freq)									\
	is_pm_qos_add_request(&exynos_isp_pm_qos[IS_PM_QOS_CLUSTER2_MAX],			\
		PM_QOS_CLUSTER2_FREQ_MAX, freq * 1000)
#define C2MAX_QOS_DEL() is_pm_qos_remove_request(&exynos_isp_pm_qos[IS_PM_QOS_CLUSTER2_MAX])
#define C2MAX_QOS_UPDATE(freq)									\
	is_pm_qos_update_request(&exynos_isp_pm_qos[IS_PM_QOS_CLUSTER2_MAX], freq * 1000)
#endif /* #if (LINUX_VERSION_CODE >= KERNEL_VERSION(5, 4, 0)) */

extern int is_sensor_runtime_suspend(struct device *dev);
extern int is_sensor_runtime_resume(struct device *dev);
extern void is_vendor_resource_clean(struct is_core *core);

static BLOCKING_NOTIFIER_HEAD(cam_pwr_noti_chain);

static unsigned long pablo_vmap(unsigned long addr, unsigned int size)
{
	int i;
	unsigned int npages = size >> PAGE_SHIFT;
	pgprot_t prot = pgprot_writecombine(PAGE_KERNEL);
	struct page **pages;
	void *vaddr;

	pages = kmalloc_array(npages, sizeof(struct page *), GFP_ATOMIC);
	if (!pages)
		return -ENOMEM;

	for (i = 0; i < npages; i++) {
		pages[i] = phys_to_page(addr);
		addr += PAGE_SIZE;
	}

	vaddr = vmap(pages, npages, VM_MAP, prot);
	kfree(pages);

	return (unsigned long)vaddr;
}

static int pablo_rscmgr_init_log_rmem(struct is_resourcemgr *rscmgr, struct reserved_mem *rmem)
{
	rscmgr->minfo->phaddr_debug = rmem->base;
	rscmgr->minfo->kvaddr_debug = pablo_vmap(rmem->base, rmem->size);
	if (!rscmgr->minfo->kvaddr_debug) {
		probe_err("failed to map [%s]", rmem->name);
		return -EINVAL;
	}

	memset((void *)rscmgr->minfo->kvaddr_debug, 0x0, rmem->size - 1);
#if IS_ENABLED(CONFIG_DEBUG_SNAPSHOT)
	dbg_snapshot_add_bl_item_info("log_camera", rmem->base, rmem->size);
#endif

	probe_info("[RSC]log rmem(V/P/S): %p/%pap/%pap\n",
				(void *)rscmgr->minfo->kvaddr_debug,
				&rmem->base, &rmem->size);

	return 0;
}

static int pablo_rscmgr_init_rmem(struct is_resourcemgr *rscmgr, struct device_node *np)
{
	struct of_phandle_iterator i;
	struct reserved_mem *rmem;

	if (of_phandle_iterator_init(&i, np, "memory-region", NULL, 0)) {
		probe_warn("no memory-region for reserved memory");
		return 0;
	}

	while (!of_phandle_iterator_next(&i)) {
		if (!i.node) {
			probe_warn("invalid memory-region phandle");
			continue;
		}

		rmem = of_reserved_mem_lookup(i.node);
		if (!rmem) {
			probe_err("failed to get [%s] reserved memory", i.node->name);
			return -EINVAL;
		}

		if (!strcmp(i.node->name, "camera_rmem"))
			pablo_rscmgr_init_log_rmem(rscmgr, rmem);
		else if (!strcmp(i.node->name, "camera_ddk") ||
				!strcmp(i.node->name, "camera-bin"))
			pablo_lib_init_reserv_mem(rmem);
		else
			probe_warn("callback not found for [%s], skipping", i.node->name);
	}

	return 0;
}

static int is_resourcemgr_alloc_mem(struct is_resourcemgr *resourcemgr)
{
	struct is_mem *mem = &resourcemgr->mem;
	struct is_minfo *minfo = resourcemgr->minfo;
#if !defined(ENABLE_DYNAMIC_MEM) && defined(ENABLE_TNR)
	size_t tnr_size = TNR_DMA_SIZE;
#endif
	int i;

	minfo->total_size = 0;

	for (i = 0; i < SENSOR_POSITION_MAX; i++) {
		/* calibration data for each sensor postion */
		minfo->pb_cal[i] = CALL_PTR_MEMOP(mem, alloc, mem->priv, TOTAL_CAL_DATA_SIZE, NULL, 0);
		if (IS_ERR_OR_NULL(minfo->pb_cal[i])) {
			err("failed to allocate buffer for TOTAL_CAL_DATA");
			return -ENOMEM;
		}
		minfo->total_size += minfo->pb_cal[i]->size;
		info("[RSC]memory_alloc(TOTAL_CAL_DATA_SIZE[%d]): 0x%08lx\n",
			i, minfo->pb_cal[i]->size);
	}

	/* library logging */
	if (!IS_ENABLED(CLOG_RESERVED_MEM)) {
		minfo->pb_debug = mem->contig_alloc(DEBUG_REGION_SIZE + 0x10);
		if (IS_ERR_OR_NULL(minfo->pb_debug)) {
			/* retry by ION */
			minfo->pb_debug = CALL_PTR_MEMOP(mem, alloc,
						mem->priv,
						DEBUG_REGION_SIZE + 0x10,
						NULL, 0);
			if (IS_ERR_OR_NULL(minfo->pb_debug)) {
				err("failed to allocate buffer for DEBUG_REGION");
				return -ENOMEM;
			}
		}
		minfo->total_size += minfo->pb_debug->size;
		info("[RSC]memory_alloc(DEBUG_REGION_SIZE): 0x%08lx\n",
				minfo->pb_debug->size);
	}

	/* library event logging */
	minfo->pb_event = mem->contig_alloc(EVENT_REGION_SIZE + 0x10);
	if (IS_ERR_OR_NULL(minfo->pb_event)) {
		/* retry by ION */
		minfo->pb_event = CALL_PTR_MEMOP(mem, alloc, mem->priv, EVENT_REGION_SIZE + 0x10, NULL, 0);
		if (IS_ERR_OR_NULL(minfo->pb_event)) {
			err("failed to allocate buffer for EVENT_REGION");
			return -ENOMEM;
		}
	}
	minfo->total_size += minfo->pb_event->size;
	info("[RSC]memory_alloc(EVENT_REGION_SIZE): 0x%08lx\n", minfo->pb_event->size);

	/* parameter region */
	minfo->pb_pregion = CALL_PTR_MEMOP(mem, alloc, mem->priv,
						(IS_STREAM_COUNT * PARAM_REGION_SIZE), NULL, 0);
	if (IS_ERR_OR_NULL(minfo->pb_pregion)) {
		err("failed to allocate buffer for PARAM_REGION");
		return -ENOMEM;
	}
	minfo->total_size += minfo->pb_pregion->size;
	info("[RSC]memory_alloc(PARAM_REGION_SIZE x %d): 0x%08lx\n",
		IS_STREAM_COUNT, minfo->pb_pregion->size);

	/* sfr dump addr region */
	minfo->pb_sfr_dump_addr = CALL_PTR_MEMOP(mem, alloc, mem->priv, SFR_DUMP_SIZE, NULL, 0);
	if (IS_ERR_OR_NULL(minfo->pb_sfr_dump_addr)) {
		err("failed to allocate buffer for SFR_DUMP_ADDR");
		return -ENOMEM;
	}
	minfo->total_size += minfo->pb_sfr_dump_addr->size;
	info("[RSC]memory_alloc(SFR_DUMP_ADDR_SIZE): 0x%08lx\n", minfo->pb_sfr_dump_addr->size);

	/* sfr dump value region */
	minfo->pb_sfr_dump_value = CALL_PTR_MEMOP(mem, alloc, mem->priv, SFR_DUMP_SIZE, NULL, 0);
	if (IS_ERR_OR_NULL(minfo->pb_sfr_dump_value)) {
		err("failed to allocate buffer for SFR_DUMP_VALUE");
		return -ENOMEM;
	}
	minfo->total_size += minfo->pb_sfr_dump_value->size;
	info("[RSC]memory_alloc(SFR_DUMP_VALUE_SIZE): 0x%08lx\n", minfo->pb_sfr_dump_value->size);

#if !defined(ENABLE_DYNAMIC_MEM)
	/* 3aa/isp internal DMA buffer */
	minfo->pb_taaisp = CALL_PTR_MEMOP(mem, alloc, mem->priv,
				TAAISP_DMA_SIZE, NULL, 0);
	if (IS_ERR_OR_NULL(minfo->pb_taaisp)) {
		err("failed to allocate buffer for TAAISP_DMA");
		return -ENOMEM;
	}
	minfo->total_size += minfo->pb_taaisp->size;
	info("[RSC]memory_alloc(TAAISP_DMA_SIZE): 0x%08lx\n", minfo->pb_taaisp->size);

#if defined(ENABLE_TNR)
	/* TNR internal DMA buffer */
	minfo->pb_tnr = CALL_PTR_MEMOP(mem, alloc, mem->priv, tnr_size, NULL, 0);
	if (IS_ERR_OR_NULL(minfo->pb_tnr)) {
		err("failed to allocate buffer for TNR DMA");
		return -ENOMEM;
	}
	minfo->total_size += minfo->pb_tnr->size;
	info("[RSC]memory_alloc(TNR_DMA_SIZE): 0x%08lx\n", minfo->pb_tnr->size);
#endif
#if (ORBMCH_DMA_SIZE > 0)
	/* ORBMCH internal DMA buffer */
	minfo->pb_orbmch = CALL_PTR_MEMOP(mem, alloc, mem->priv, ORBMCH_DMA_SIZE, NULL, 0);
	if (IS_ERR_OR_NULL(minfo->pb_orbmch)) {
		err("failed to allocate buffer for ORBMCH DMA");
		return -ENOMEM;
	}
	minfo->total_size += minfo->pb_orbmch->size;
	info("[RSC]memory_alloc(ORBMCH_DMA_SIZE): 0x%08lx\n", minfo->pb_orbmch->size);
#endif
#endif

	if (DUMMY_DMA_SIZE) {
		minfo->pb_dummy = CALL_PTR_MEMOP(mem, alloc, mem->priv, DUMMY_DMA_SIZE,
			"camera_heap", 0);
		if (IS_ERR_OR_NULL(minfo->pb_dummy)) {
			err("failed to allocate buffer for dummy");
			return -ENOMEM;
		}
		minfo->total_size += minfo->pb_dummy->size;
		info("[RSC]memory_alloc(DUMMY_DMA_SIZE): 0x%08lx\n", minfo->pb_dummy->size);
	}

	probe_info("[RSC]memory_alloc(Internal total): 0x%08lx\n", minfo->total_size);

	return 0;
}

static int is_resourcemgr_init_mem(struct is_resourcemgr *resourcemgr)
{
	struct is_minfo *minfo = resourcemgr->minfo;
	int ret = 0;
	int i;

	probe_info("%s\n", __func__);

	ret = is_resourcemgr_alloc_mem(resourcemgr);
	if (ret) {
		err("Couldn't alloc for FIMC-IS\n");
		ret = -ENOMEM;
		goto p_err;
	}

	/* set information */
	if (!minfo->kvaddr_debug) {
		minfo->kvaddr_debug =
			CALL_BUFOP(minfo->pb_debug, kvaddr, minfo->pb_debug);
		minfo->phaddr_debug =
			CALL_BUFOP(minfo->pb_debug, phaddr, minfo->pb_debug);
	}

	minfo->kvaddr_event = CALL_BUFOP(minfo->pb_event, kvaddr, minfo->pb_event);
	minfo->phaddr_event = CALL_BUFOP(minfo->pb_event, phaddr, minfo->pb_event);

	minfo->kvaddr_sfr_dump_addr = CALL_BUFOP(minfo->pb_sfr_dump_addr, kvaddr, minfo->pb_sfr_dump_addr);
	minfo->kvaddr_sfr_dump_value =
		CALL_BUFOP(minfo->pb_sfr_dump_value, kvaddr, minfo->pb_sfr_dump_value);

	minfo->kvaddr_region = CALL_BUFOP(minfo->pb_pregion, kvaddr, minfo->pb_pregion);

	minfo->kvaddr_debug_cnt =  minfo->kvaddr_debug
						+ DEBUG_REGION_SIZE;
	minfo->kvaddr_event_cnt =  minfo->kvaddr_event
						+ EVENT_REGION_SIZE;

	for (i = 0; i < SENSOR_POSITION_MAX; i++)
		minfo->kvaddr_cal[i] =
			CALL_BUFOP(minfo->pb_cal[i], kvaddr, minfo->pb_cal[i]);

	if (DUMMY_DMA_SIZE) {
		minfo->dvaddr_dummy =
			CALL_BUFOP(minfo->pb_dummy, dvaddr, minfo->pb_dummy);
		minfo->kvaddr_dummy =
			CALL_BUFOP(minfo->pb_dummy, kvaddr, minfo->pb_dummy);
		minfo->phaddr_dummy =
			CALL_BUFOP(minfo->pb_dummy, phaddr, minfo->pb_dummy);

		probe_info("[RSC] Dummy buffer: dva(%pad) kva(0x%p) pha(%pa)\n",
			&minfo->dvaddr_dummy,
			(void *)minfo->kvaddr_dummy,
			&minfo->phaddr_dummy);
	}

	probe_info("[RSC] Kernel virtual for debug: 0x%p\n", (void *)minfo->kvaddr_debug);
	probe_info("[RSC] is_init_mem done\n");
p_err:
	return ret;
}

#ifdef ENABLE_DYNAMIC_MEM
static int is_resourcemgr_alloc_dynamic_mem(struct is_resourcemgr *resourcemgr)
{
	struct is_mem *mem = &resourcemgr->mem;
	struct is_minfo *minfo = resourcemgr->minfo;
	int ret;

	if (TAAISP_DMA_SIZE > 0) {
		/* 3aa/isp internal DMA buffer */
		minfo->pb_taaisp = CALL_PTR_MEMOP(mem, alloc, mem->priv,
					TAAISP_DMA_SIZE, NULL, 0);
		if (IS_ERR_OR_NULL(minfo->pb_taaisp)) {
			err("failed to allocate buffer for TAAISP_DMA memory");
			ret = -ENOMEM;
			goto err_alloc_taaisp;
		}

		info("[RSC]memory_alloc(TAAISP_DMA_SIZE): 0x%08lx\n", minfo->pb_taaisp->size);
	} else {
		minfo->pb_taaisp = NULL;
	}

	if (TNR_DMA_SIZE > 0) {
		/* TNR internal DMA buffer */
		minfo->pb_tnr = CALL_PTR_MEMOP(mem, alloc, mem->priv, TNR_DMA_SIZE, NULL, 0);
		if (IS_ERR_OR_NULL(minfo->pb_tnr)) {
			err("failed to allocate buffer for TNR DMA");
			ret = -ENOMEM;
			goto err_alloc_tnr;
		}

		info("[RSC]memory_alloc(TNR_DMA_SIZE): 0x%08lx\n", minfo->pb_tnr->size);
	} else {
		minfo->pb_tnr = NULL;
	}

	if (ORBMCH_DMA_SIZE > 0) {
		/* ORBMCH internal DMA buffer */
		minfo->pb_orbmch = CALL_PTR_MEMOP(mem, alloc, mem->priv,
				ORBMCH_DMA_SIZE, NULL, 0);
		if (IS_ERR_OR_NULL(minfo->pb_orbmch)) {
			err("failed to allocate buffer for ORBMCH DMA");
			ret = -ENOMEM;
			goto err_alloc_orbmch;
		}

		info("[RSC]memory_alloc(ORBMCH_DMA_SIZE): 0x%08lx\n", minfo->pb_orbmch->size);
	} else {
		minfo->pb_orbmch = NULL;
	}

	return 0;

err_alloc_orbmch:
	if (minfo->pb_tnr)
		CALL_VOID_BUFOP(minfo->pb_tnr, free, minfo->pb_tnr);
err_alloc_tnr:
err_alloc_taaisp:
	minfo->pb_orbmch = NULL;
	minfo->pb_tnr = NULL;
	minfo->pb_taaisp = NULL;

	return ret;
}

static int is_resourcemgr_init_dynamic_mem(struct is_resourcemgr *resourcemgr)
{
	struct is_minfo *minfo = resourcemgr->minfo;
	int ret = 0;
	unsigned long kva;
	dma_addr_t dva;

	probe_info("is_init_mem - ION\n");

	ret = is_resourcemgr_alloc_dynamic_mem(resourcemgr);
	if (ret) {
		err("Couldn't alloc for FIMC-IS\n");
		ret = -ENOMEM;
		goto p_err;
	}

	if (minfo->pb_taaisp) {
		kva = CALL_BUFOP(minfo->pb_taaisp, kvaddr, minfo->pb_taaisp);
		dva = CALL_BUFOP(minfo->pb_taaisp, dvaddr, minfo->pb_taaisp);
		info("[RSC] TAAISP_DMA memory kva:0x%pK, dva: %pad\n", (void *)kva, &dva);
	}

	if (minfo->pb_tnr) {
		kva = CALL_BUFOP(minfo->pb_tnr, kvaddr, minfo->pb_tnr);
		dva = CALL_BUFOP(minfo->pb_tnr, dvaddr, minfo->pb_tnr);
		info("[RSC] TNR_DMA memory kva:0x%pK, dva: %pad\n", (void *)kva, &dva);
	}

	if (minfo->pb_orbmch) {
		kva = CALL_BUFOP(minfo->pb_orbmch, kvaddr, minfo->pb_orbmch);
		dva = CALL_BUFOP(minfo->pb_orbmch, dvaddr, minfo->pb_orbmch);
		info("[RSC] ORBMCH_DMA memory kva:0x%pK, dva: %pad\n", (void *)kva, &dva);
	}

	info("[RSC] %s done\n", __func__);

p_err:
	return ret;
}

static int is_resourcemgr_deinit_dynamic_mem(struct is_resourcemgr *resourcemgr)
{
	struct is_minfo *minfo = resourcemgr->minfo;

	if (minfo->pb_orbmch)
		CALL_VOID_BUFOP(minfo->pb_orbmch, free, minfo->pb_orbmch);

	if (minfo->pb_tnr)
		CALL_VOID_BUFOP(minfo->pb_tnr, free, minfo->pb_tnr);

	if (minfo->pb_taaisp)
		CALL_VOID_BUFOP(minfo->pb_taaisp, free, minfo->pb_taaisp);

	minfo->pb_orbmch = NULL;
	minfo->pb_tnr = NULL;
	minfo->pb_taaisp = NULL;

	return 0;
}
#endif /* #ifdef ENABLE_DYNAMIC_MEM */

#if IS_ENABLED(CONFIG_PABLO_KUNIT_TEST)
int pablo_kunit_resourcemgr_init_dynamic_mem(struct is_resourcemgr *resourcemgr) {
	return is_resourcemgr_init_dynamic_mem(resourcemgr);
}
KUNIT_EXPORT_SYMBOL(pablo_kunit_resourcemgr_init_dynamic_mem);

int pablo_kunit_resourcemgr_deinit_dynamic_mem(struct is_resourcemgr *resourcemgr) {
	return is_resourcemgr_deinit_dynamic_mem(resourcemgr);
}
KUNIT_EXPORT_SYMBOL(pablo_kunit_resourcemgr_deinit_dynamic_mem);
#endif

static int is_resourcemgr_alloc_secure_mem(struct is_resourcemgr *resourcemgr)
{
	struct is_mem *mem = &resourcemgr->mem;
	struct is_minfo *minfo = resourcemgr->minfo;
	int ret;

	if (IS_ENABLED(SECURE_CAMERA_TAAISP)) {
		if (TAAISP_DMA_SIZE > 0) {
			/* 3aa/isp internal DMA buffer */
			minfo->pb_taaisp_s = CALL_PTR_MEMOP(mem, alloc, mem->priv,
					TAAISP_DMA_SIZE, "camera_heap",
					ION_EXYNOS_FLAG_PROTECTED);
			if (IS_ERR_OR_NULL(minfo->pb_taaisp_s)) {
				err("failed to allocate buffer for TAAISP_DMA_S");
				ret = -ENOMEM;
				goto err_alloc_taaisp_s;
			}

			info("[RSC]memory_alloc(TAAISP_DMA_S): %08x\n", TAAISP_DMA_SIZE);
		} else {
			minfo->pb_taaisp_s = NULL;
		}
	}

	if (IS_ENABLED(SECURE_CAMERA_TNR)) {
		if (TNR_S_DMA_SIZE > 0) {
			minfo->pb_tnr_s = CALL_PTR_MEMOP(mem, alloc, mem->priv,
					TNR_S_DMA_SIZE, "secure_camera_heap",
					ION_EXYNOS_FLAG_PROTECTED);
			if (IS_ERR_OR_NULL(minfo->pb_tnr_s)) {
				err("failed to allocate buffer for TNR_DMA_S");
				ret = -ENOMEM;
				goto err_alloc_tnr_s;
			}

			info("[RSC]memory_alloc(TNR_DMA_S): %08x\n", TNR_S_DMA_SIZE);
		} else {
			minfo->pb_tnr_s = NULL;
		}
	}

	return 0;

err_alloc_tnr_s:
err_alloc_taaisp_s:
	minfo->pb_tnr_s = NULL;
	minfo->pb_taaisp_s = NULL;

	return ret;
}

static int is_resourcemgr_init_secure_mem(struct is_resourcemgr *resourcemgr)
{
	struct is_core *core;
	int ret = 0;

	core = container_of(resourcemgr, struct is_core, resourcemgr);
	FIMC_BUG(!core);

	if (core->scenario != IS_SCENARIO_SECURE)
		return ret;

	info("is_init_secure_mem - ION\n");

	ret = is_resourcemgr_alloc_secure_mem(resourcemgr);
	if (ret) {
		err("Couldn't alloc for FIMC-IS\n");
		ret = -ENOMEM;
		goto p_err;
	}

	info("[RSC] %s done\n", __func__);
p_err:
	return ret;
}

static int is_resourcemgr_deinit_secure_mem(struct is_resourcemgr *resourcemgr)
{
	struct is_minfo *minfo = resourcemgr->minfo;
	struct is_core *core;
	int ret = 0;

	core = container_of(resourcemgr, struct is_core, resourcemgr);
	FIMC_BUG(!core);

	if (minfo->pb_taaisp_s)
		CALL_VOID_BUFOP(minfo->pb_taaisp_s, free, minfo->pb_taaisp_s);
	if (minfo->pb_tnr_s)
		CALL_VOID_BUFOP(minfo->pb_tnr_s, free, minfo->pb_tnr_s);

	minfo->pb_taaisp_s = NULL;
	minfo->pb_tnr_s = NULL;

	info("[RSC] %s done\n", __func__);

	return ret;
}

void is_bts_scen(struct is_resourcemgr *resourcemgr,
	unsigned int index, bool enable)
{
#if IS_ENABLED(CONFIG_EXYNOS_BTS)
	int ret = 0;
	unsigned int scen_idx;
	const char *name;

	if (resourcemgr->bts_scen != NULL)
		name = resourcemgr->bts_scen[index].name;
	else {
		err("%s : null bts_scen struct\n", __func__);
		return;
	}

	scen_idx = bts_get_scenindex(name);
	if (scen_idx) {
		if (enable)
			ret = bts_add_scenario(scen_idx);
		else
			ret = bts_del_scenario(scen_idx);

		if (ret) {
			err("call bts_%s_scenario is fail (%d:%s)\n",
				enable ? "add" : "del", scen_idx, name);
		} else {
			info("call bts_%s_scenario (%d:%s)\n",
				enable ? "add" : "del", scen_idx, name);
		}
	}
#endif
}

#if IS_ENABLED(CONFIG_EXYNOS_THERMAL) || IS_ENABLED(CONFIG_EXYNOS_THERMAL_V2)
static int is_tmu_notifier(struct notifier_block *nb,
	unsigned long state, void *data)
{
	int ret = 0, fps = 0;
	struct is_resourcemgr *resourcemgr;
	struct is_core *core;
	struct is_dvfs_ctrl *dvfs_ctrl;
#if IS_ENABLED(CONFIG_EXYNOS_SNAPSHOT_THERMAL) || IS_ENABLED(CONFIG_DEBUG_SNAPSHOT)
	char *cooling_device_name = "ISP";
#endif
	resourcemgr = container_of(nb, struct is_resourcemgr, tmu_notifier);
	dvfs_ctrl = &(resourcemgr->dvfs_ctrl);

	core = is_get_is_core();

	if (!atomic_read(&core->rsccount))
		warn("[RSC] Camera is not opened");

	switch (state) {
	case ISP_NORMAL:
		resourcemgr->tmu_state = ISP_NORMAL;
		resourcemgr->limited_fps = 0;
		break;
	case ISP_COLD:
		resourcemgr->tmu_state = ISP_COLD;
		resourcemgr->limited_fps = 0;
		break;
	case ISP_THROTTLING:
		fps = isp_cooling_get_fps(0, *(unsigned long *)data);
		set_bit(IS_DVFS_TMU_THROTT, &dvfs_ctrl->state);

		/* The FPS can be defined to any specific value. */
		if (fps >= 60) {
			resourcemgr->tmu_state = ISP_NORMAL;
			resourcemgr->limited_fps = 0;
			clear_bit(IS_DVFS_TICK_THROTT, &dvfs_ctrl->state);
			warn("[RSC] THROTTLING : Unlimited FPS");
		} else {
			resourcemgr->tmu_state = ISP_THROTTLING;
			resourcemgr->limited_fps = fps;
			warn("[RSC] THROTTLING : Limited %d FPS", fps);
		}
		break;
	case ISP_TRIPPING:
		resourcemgr->tmu_state = ISP_TRIPPING;
		resourcemgr->limited_fps = 5;
		warn("[RSC] TRIPPING : Limited 5FPS");
		break;
	default:
		err("[RSC] invalid tmu state(%ld)", state);
		break;
	}

#if IS_ENABLED(CONFIG_EXYNOS_SNAPSHOT_THERMAL)
	exynos_ss_thermal(NULL, 0, cooling_device_name, resourcemgr->limited_fps);
#elif IS_ENABLED(CONFIG_DEBUG_SNAPSHOT)
	dbg_snapshot_thermal(NULL, 0, cooling_device_name, resourcemgr->limited_fps);
#endif

	return ret;
}
#endif

#if IS_ENABLED(CONFIG_EXYNOS_ITMON)
static int due_to_is(const char *desc, struct is_resourcemgr *rscmgr)
{
	unsigned int i;

	for (i = 0; i < rscmgr->itmon_port_num; i++) {
		if (desc && (strstr((char *)desc, rscmgr->itmon_port_name[i])))
			return 1;
	}

	return 0;
}

static int is_itmon_notifier(struct notifier_block *nb,
	unsigned long state, void *data)
{
	int i;
	struct is_core *core;
	struct is_resourcemgr *resourcemgr;
	struct itmon_notifier *itmon;

	info("%s: NOC info\n", __func__);

	resourcemgr = container_of(nb, struct is_resourcemgr, itmon_notifier);
	core = container_of(resourcemgr, struct is_core, resourcemgr);
	itmon = (struct itmon_notifier *)data;

	if (!itmon)
		return NOTIFY_DONE;

	if (due_to_is(itmon->port, resourcemgr)
			|| due_to_is(itmon->dest, resourcemgr)
			|| due_to_is(itmon->master, resourcemgr)) {
		info("%s: init description : %s\n", __func__, itmon->port);
		info("%s: target descrition: %s\n", __func__, itmon->dest);
		info("%s: user description : %s\n", __func__, itmon->master);
		info("%s: Transaction Type : %s\n", __func__, itmon->read ? "READ" : "WRITE");
		info("%s: target address   : %lx\n",__func__, itmon->target_addr);
		info("%s: Power Domain     : %s(%d)\n",__func__, itmon->pd_name, itmon->onoff);

		for (i = 0; i < IS_STREAM_COUNT; ++i) {
			if (!test_bit(IS_ISCHAIN_POWER_ON, &core->ischain[i].state))
				continue;

			is_debug_s2d(true, "ITMON error");
		}

		return NOTIFY_STOP;
	}

	return NOTIFY_DONE;
}
#endif

int is_resource_cdump(void)
{
	struct is_core *core;
	struct is_group *group;
	struct is_subdev *subdev;
	struct is_framemgr *framemgr;
	struct is_groupmgr *groupmgr;
	struct is_device_ischain *device = NULL;
	struct is_device_csi *csi;
	int i, j;

	core = is_get_is_core();

	if (atomic_read(&core->rsccount) == 0)
		goto exit;

	info("### %s dump start ###\n", __func__);
	cinfo("###########################\n");
	cinfo("###########################\n");
	cinfo("### %s dump start ###\n", __func__);

	groupmgr = &core->groupmgr;

#if IS_ENABLED(CONFIG_EXYNOS_MEMORY_LOGGER)
	/* dump all CR by using memlogger */
	is_debug_memlog_dump_cr_all(MEMLOG_LEVEL_EMERG);
#endif

	/* dump per core */
	for (i = 0; i < IS_STREAM_COUNT; ++i) {
		device = &core->ischain[i];
		if (!test_bit(IS_ISCHAIN_OPEN, &device->state))
			continue;

		if (test_bit(IS_ISCHAIN_CLOSING, &device->state))
			continue;

		/* clock & gpio dump */
		if (!in_interrupt()) {
			struct exynos_platform_is *pdata =
				dev_get_platdata(&core->pdev->dev);

			pdata->clk_dump(&core->pdev->dev);
		}

		cinfo("### 2. SFR DUMP ###\n");
		break;
	}

	/* dump per ischain */
	for (i = 0; i < IS_STREAM_COUNT; ++i) {
		device = &core->ischain[i];
		if (!test_bit(IS_ISCHAIN_OPEN, &device->state))
			continue;

		if (test_bit(IS_ISCHAIN_CLOSING, &device->state))
			continue;

		if (device->sensor && !test_bit(IS_ISCHAIN_REPROCESSING, &device->state)) {
			csi = (struct is_device_csi *)v4l2_get_subdevdata(device->sensor->subdev_csi);
			if (csi)
				csi_hw_cdump_all(csi);
		}

		cinfo("### 3. DUMP frame manager ###\n");

		/* dump all framemgr */
		group = groupmgr->leader[i];
		while (group) {
			if (!test_bit(IS_GROUP_OPEN, &group->state))
				break;

			for (j = 0; j < ENTRY_END; j++) {
				subdev = group->subdev[j];
				if (subdev && test_bit(IS_SUBDEV_START, &subdev->state)) {
					framemgr = GET_SUBDEV_FRAMEMGR(subdev);
					if (framemgr) {
						unsigned long flags;

						cinfo("[%d][%s] framemgr dump\n",
							subdev->instance, subdev->name);
						framemgr_e_barrier_irqs(framemgr, 0, flags);
						frame_manager_dump_queues(framemgr);
						framemgr_x_barrier_irqr(framemgr, 0, flags);
					}
				}
			}

			group = group->next;
		}
	}


	/* dump per core */
	for (i = 0; i < IS_STREAM_COUNT; ++i) {
		device = &core->ischain[i];
		if (!test_bit(IS_ISCHAIN_OPEN, &device->state))
			continue;

		if (test_bit(IS_ISCHAIN_CLOSING, &device->state))
			continue;

		is_hardware_sfr_dump(&core->hardware, DEV_HW_END, false);
		break;
	}
	cinfo("#####################\n");
	cinfo("#####################\n");
	cinfo("### %s dump end ###\n", __func__);
	info("### %s dump end (refer camera dump)###\n", __func__);
exit:
	return 0;
}

#ifdef ENABLE_KERNEL_LOG_DUMP
int is_kernel_log_dump(bool overwrite)
{
	static int dumped = 0;
	struct is_core *core;
	struct is_resourcemgr *resourcemgr;
	void *log_kernel = NULL;
	unsigned long long when;
	unsigned long usec;

	core = is_get_is_core();

	resourcemgr = &core->resourcemgr;

	if (dumped && !overwrite) {
		when = resourcemgr->kernel_log_time;
		usec = do_div(when, NSEC_PER_SEC) / NSEC_PER_USEC;
		info("kernel log was saved already at [%5lu.%06lu]\n",
				(unsigned long)when, usec);

		return -ENOSPC;
	}

#if IS_ENABLED(CONFIG_EXYNOS_SNAPSHOT)
	log_kernel = (void *)exynos_ss_get_item_vaddr("log_kernel");
#elif IS_ENABLED(CONFIG_DEBUG_SNAPSHOT)
	log_kernel = (void *)dbg_snapshot_get_item_vaddr("log_kernel");
#endif
	if (!log_kernel)
		return -EINVAL;

	if (resourcemgr->kernel_log_buf) {
		resourcemgr->kernel_log_time = local_clock();

		info("kernel log saved to %p(%p) from %p\n",
				resourcemgr->kernel_log_buf,
				(void *)virt_to_phys(resourcemgr->kernel_log_buf),
				log_kernel);
#if IS_ENABLED(CONFIG_EXYNOS_SNAPSHOT)
		memcpy(resourcemgr->kernel_log_buf, log_kernel,
				exynos_ss_get_item_size("log_kernel"));
#elif IS_ENABLED(CONFIG_DEBUG_SNAPSHOT)
		memcpy(resourcemgr->kernel_log_buf, log_kernel,
				dbg_snapshot_get_item_size("log_kernel"));
#endif

		dumped = 1;
	}

	return 0;
}
#endif

int is_resource_dump(void)
{
	struct is_core *core;
	struct is_group *group;
	struct is_subdev *subdev;
	struct is_framemgr *framemgr;
	struct is_groupmgr *groupmgr;
	struct is_device_ischain *device = NULL;
	struct is_device_csi *csi;
	int i, j;

	core = is_get_is_core();

	info("### %s dump start ###\n", __func__);
	groupmgr = &core->groupmgr;

	/* dump per core */
	for (i = 0; i < IS_STREAM_COUNT; ++i) {
		device = &core->ischain[i];
		if (!test_bit(IS_ISCHAIN_OPEN, &device->state))
			continue;

		if (test_bit(IS_ISCHAIN_CLOSING, &device->state))
			continue;

		/* clock & gpio dump */
		schedule_work(&core->wq_data_print_clk);
		/* ddk log dump */
		is_lib_logdump();
		is_hardware_sfr_dump(&core->hardware, DEV_HW_END, false);
		break;
	}

	/* dump per ischain */
	for (i = 0; i < IS_STREAM_COUNT; ++i) {
		device = &core->ischain[i];
		if (!test_bit(IS_ISCHAIN_OPEN, &device->state))
			continue;

		if (test_bit(IS_ISCHAIN_CLOSING, &device->state))
			continue;

		if (device->sensor && !test_bit(IS_ISCHAIN_REPROCESSING, &device->state)) {
			csi = (struct is_device_csi *)v4l2_get_subdevdata(device->sensor->subdev_csi);
			if (csi)
				csi_hw_dump_all(csi);
		}

		/* dump all framemgr */
		group = groupmgr->leader[i];
		while (group) {
			if (!test_bit(IS_GROUP_OPEN, &group->state))
				break;

			for (j = 0; j < ENTRY_END; j++) {
				subdev = group->subdev[j];
				if (subdev && test_bit(IS_SUBDEV_START, &subdev->state)) {
					framemgr = GET_SUBDEV_FRAMEMGR(subdev);
					if (framemgr) {
						unsigned long flags;
						mserr(" dump framemgr..", subdev, subdev);
						framemgr_e_barrier_irqs(framemgr, 0, flags);
						frame_manager_print_queues(framemgr);
						framemgr_x_barrier_irqr(framemgr, 0, flags);
					}
				}
			}

			group = group->next;
		}
	}

	info("### %s dump end ###\n", __func__);

	return 0;
}

#ifdef ENABLE_PANIC_HANDLER
static int is_panic_handler(struct notifier_block *nb, ulong l,
	void *buf)
{
	if (IS_ENABLED(CLOGGING))
		is_resource_cdump();
	else
		is_resource_dump();

	return 0;
}

static struct notifier_block notify_panic_block = {
	.notifier_call = is_panic_handler,
};
#endif

#if defined(ENABLE_REBOOT_HANDLER)
static int is_reboot_handler(struct notifier_block *nb, ulong l,
	void *buf)
{
	struct is_core *core;

	info("%s enter\n", __func__);

	core = is_get_is_core();
	is_cleanup(core);

	info("%s exit\n", __func__);

	return 0;
}

static struct notifier_block notify_reboot_block = {
	.notifier_call = is_reboot_handler,
};
#endif

#if defined(DISABLE_CORE_IDLE_STATE)
static void is_resourcemgr_c2_disable_work(struct work_struct *data)
{
	/* CPU idle on/off */
	info("%s: call cpuidle_pause()\n", __func__);
	cpuidle_pause_and_lock();
}
#endif

#if IS_ENABLED(CONFIG_SCHED_EMS_TUNE)
static struct emstune_mode_request emstune_req;
struct emstune_mode_request *is_get_emstune_req(void)
{
	return &emstune_req;
}
#endif

struct is_pm_qos_request *is_get_pm_qos(void)
{
	return exynos_isp_pm_qos;
}
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(5, 4, 0))
struct freq_qos_request *is_get_freq_qos(void)
{
	return exynos_isp_freq_qos;
}
#endif

int is_resourcemgr_probe(struct is_resourcemgr *resourcemgr,
	void *private_data, struct platform_device *pdev)
{
	int ret = 0;
	u32 i;
	struct device_node *np;

	FIMC_BUG(!resourcemgr);
	FIMC_BUG(!private_data);

	np = pdev->dev.of_node;
	resourcemgr->private_data = private_data;

	clear_bit(IS_RM_COM_POWER_ON, &resourcemgr->state);
	clear_bit(IS_RM_SS0_POWER_ON, &resourcemgr->state);
	clear_bit(IS_RM_SS1_POWER_ON, &resourcemgr->state);
	clear_bit(IS_RM_SS2_POWER_ON, &resourcemgr->state);
	clear_bit(IS_RM_SS3_POWER_ON, &resourcemgr->state);
	clear_bit(IS_RM_SS4_POWER_ON, &resourcemgr->state);
	clear_bit(IS_RM_SS5_POWER_ON, &resourcemgr->state);
	clear_bit(IS_RM_ISC_POWER_ON, &resourcemgr->state);
	clear_bit(IS_RM_POWER_ON, &resourcemgr->state);
	atomic_set(&resourcemgr->rsccount, 0);
	atomic_set(&resourcemgr->qos_refcount, 0);
	atomic_set(&resourcemgr->resource_sensor0.rsccount, 0);
	atomic_set(&resourcemgr->resource_sensor1.rsccount, 0);
	atomic_set(&resourcemgr->resource_sensor2.rsccount, 0);
	atomic_set(&resourcemgr->resource_sensor3.rsccount, 0);
	atomic_set(&resourcemgr->resource_ischain.rsccount, 0);
	atomic_set(&resourcemgr->resource_preproc.rsccount, 0);

	resourcemgr->cluster0 = 0;
	resourcemgr->cluster1 = 0;
	resourcemgr->cluster2 = 0;

	resourcemgr->minfo = is_get_is_minfo();

	/* rsc mutex init */
	mutex_init(&resourcemgr->rsc_lock);
	mutex_init(&resourcemgr->sysreg_lock);
	mutex_init(&resourcemgr->qos_lock);

#if IS_ENABLED(CONFIG_EXYNOS_ITMON)
	/* bus monitor unit */
	resourcemgr->itmon_notifier.notifier_call = is_itmon_notifier;
	resourcemgr->itmon_notifier.priority = 0;
	itmon_notifier_chain_register(&resourcemgr->itmon_notifier);
#endif

#if IS_ENABLED(CONFIG_EXYNOS_THERMAL) || IS_ENABLED(CONFIG_EXYNOS_THERMAL_V2)
	/* temperature monitor unit */
	resourcemgr->tmu_notifier.notifier_call = is_tmu_notifier;
	resourcemgr->tmu_notifier.priority = 0;
	resourcemgr->tmu_state = ISP_NORMAL;
	resourcemgr->limited_fps = 0;
	resourcemgr->streaming_cnt = 0;

	ret = exynos_tmu_isp_add_notifier(&resourcemgr->tmu_notifier);
	if (ret) {
		probe_err("exynos_tmu_isp_add_notifier is fail(%d)", ret);
		goto p_err;
	}
#endif

	ret = pablo_rscmgr_init_rmem(resourcemgr, np);
	if (ret) {
		probe_err("failed to init reserved memory(%d)", ret);
		goto p_err;
	}

	ret = is_resourcemgr_init_mem(resourcemgr);
	if (ret) {
		probe_err("is_resourcemgr_init_mem is fail(%d)", ret);
		goto p_err;
	}

	pablo_lib_init_mem(&resourcemgr->mem);
	if (ret) {
		probe_err("pablo_lib_init_mem is fail(%d)", ret);
		goto p_err;
	}

	for (i = 0 ; i < IS_STREAM_COUNT; i++)
		pablo_crta_bufmgr_probe(&resourcemgr->crta_bufmgr[i]);

	/* dvfs controller init */
	ret = is_dvfs_init(resourcemgr);
	if (ret) {
		probe_err("%s: is_dvfs_init failed!\n", __func__);
		goto p_err;
	}

#ifdef ENABLE_PANIC_HANDLER
	atomic_notifier_chain_register(&panic_notifier_list, &notify_panic_block);
#endif
#if defined(ENABLE_REBOOT_HANDLER)
	register_reboot_notifier(&notify_reboot_block);
#endif
#if IS_ENABLED(CONFIG_CMU_EWF)
	get_cmuewf_index(np, &idx_ewf);
#endif

#ifdef ENABLE_KERNEL_LOG_DUMP
#if IS_ENABLED(CONFIG_EXYNOS_SNAPSHOT)
	resourcemgr->kernel_log_buf = kzalloc(exynos_ss_get_item_size("log_kernel"),
						GFP_KERNEL);
#elif IS_ENABLED(CONFIG_DEBUG_SNAPSHOT)
	resourcemgr->kernel_log_buf = kzalloc(dbg_snapshot_get_item_size("log_kernel"),
						GFP_KERNEL);
#endif
#endif

	mutex_init(&resourcemgr->global_param.lock);
	resourcemgr->global_param.video_mode = 0;
	resourcemgr->global_param.state = 0;

#if defined(DISABLE_CORE_IDLE_STATE)
	INIT_WORK(&resourcemgr->c2_disable_work, is_resourcemgr_c2_disable_work);
#endif

	INIT_LIST_HEAD(&resourcemgr->regulator_list);

p_err:
	probe_info("[RSC] %s(%d)\n", __func__, ret);
	return ret;
}

int is_resource_open(struct is_resourcemgr *resourcemgr, u32 rsc_type, void **device)
{
	int ret = 0;
	u32 stream;
	void *result;
	struct is_resource *resource;
	struct is_core *core;
	struct is_device_ischain *ischain;

	FIMC_BUG(!resourcemgr);
	FIMC_BUG(!resourcemgr->private_data);
	FIMC_BUG(rsc_type >= RESOURCE_TYPE_MAX);

	result = NULL;
	core = (struct is_core *)resourcemgr->private_data;
	resource = GET_RESOURCE(resourcemgr, rsc_type);
	if (!resource) {
		err("[RSC] resource is NULL");
		ret = -EINVAL;
		goto p_err;
	}

	switch (rsc_type) {
	case RESOURCE_TYPE_SENSOR0:
		result = &core->sensor[RESOURCE_TYPE_SENSOR0];
		resource->pdev = core->sensor[RESOURCE_TYPE_SENSOR0].pdev;
		break;
	case RESOURCE_TYPE_SENSOR1:
		result = &core->sensor[RESOURCE_TYPE_SENSOR1];
		resource->pdev = core->sensor[RESOURCE_TYPE_SENSOR1].pdev;
		break;
	case RESOURCE_TYPE_SENSOR2:
		result = &core->sensor[RESOURCE_TYPE_SENSOR2];
		resource->pdev = core->sensor[RESOURCE_TYPE_SENSOR2].pdev;
		break;
	case RESOURCE_TYPE_SENSOR3:
		result = &core->sensor[RESOURCE_TYPE_SENSOR3];
		resource->pdev = core->sensor[RESOURCE_TYPE_SENSOR3].pdev;
		break;
#if (IS_SENSOR_COUNT > 4)
	case RESOURCE_TYPE_SENSOR4:
		result = &core->sensor[RESOURCE_TYPE_SENSOR4];
		resource->pdev = core->sensor[RESOURCE_TYPE_SENSOR4].pdev;
		break;
#if (IS_SENSOR_COUNT > 5)
	case RESOURCE_TYPE_SENSOR5:
		result = &core->sensor[RESOURCE_TYPE_SENSOR5];
		resource->pdev = core->sensor[RESOURCE_TYPE_SENSOR5].pdev;
		break;
#endif
#endif
	case RESOURCE_TYPE_ISCHAIN:
		for (stream = 0; stream < IS_STREAM_COUNT; ++stream) {
			ischain = &core->ischain[stream];
			if (!test_bit(IS_ISCHAIN_OPEN, &ischain->state)) {
				result = ischain;
				resource->pdev = ischain->pdev;
				break;
			}
		}
		break;
	}

	if (device)
		*device = result;

p_err:
	dbgd_resource("%s\n", __func__);
	return ret;
}
KUNIT_EXPORT_SYMBOL(is_resource_open);

static void is_resource_reset(struct is_resourcemgr *resourcemgr)
{
	struct is_lic_sram *lic_sram = &resourcemgr->lic_sram;

	memset((void *)lic_sram, 0x0, sizeof(struct is_lic_sram));

	resourcemgr->cluster0 = 0;
	resourcemgr->cluster1 = 0;
	resourcemgr->cluster2 = 0;

	resourcemgr->global_param.state = 0;
	resourcemgr->shot_timeout = SHOT_TIMEOUT;
	resourcemgr->shot_timeout_tick = 0;

#if IS_ENABLED(CONFIG_SCHED_EMS_TUNE)
	emstune_add_request(&emstune_req);
#endif

#if IS_ENABLED(CONFIG_CMU_EWF)
	set_cmuewf(idx_ewf, 1);
#endif
	is_debug_bcm(true, 0);

#if IS_ENABLED(CONFIG_EXYNOS_SCI_DBG_AUTO)
	smc_ppc_enable(1);
#endif

#ifdef DISABLE_BTS_CALC
	bts_calc_disable(1);
#endif

	resourcemgr->sfrdump_ptr = 0;
	votfitf_init();
}

static void is_resource_clear(struct is_resourcemgr *resourcemgr)
{
	u32 current_min, current_max;

	current_min = (resourcemgr->cluster0 & CLUSTER_MIN_MASK) >> CLUSTER_MIN_SHIFT;
	current_max = (resourcemgr->cluster0 & CLUSTER_MAX_MASK) >> CLUSTER_MAX_SHIFT;
	if (current_min) {
		C0MIN_QOS_DEL();
		warn("[RSC] cluster0 minfreq is not removed(%dMhz)\n", current_min);
	}

	if (current_max) {
		C0MAX_QOS_DEL();
		warn("[RSC] cluster0 maxfreq is not removed(%dMhz)\n", current_max);
	}

	current_min = (resourcemgr->cluster1 & CLUSTER_MIN_MASK) >> CLUSTER_MIN_SHIFT;
	current_max = (resourcemgr->cluster1 & CLUSTER_MAX_MASK) >> CLUSTER_MAX_SHIFT;
	if (current_min) {
		C1MIN_QOS_DEL();
		warn("[RSC] cluster1 minfreq is not removed(%dMhz)\n", current_min);
	}

	if (current_max) {
		C1MAX_QOS_DEL();
		warn("[RSC] cluster1 maxfreq is not removed(%dMhz)\n", current_max);
	}

	current_min = (resourcemgr->cluster2 & CLUSTER_MIN_MASK) >> CLUSTER_MIN_SHIFT;
	current_max = (resourcemgr->cluster2 & CLUSTER_MAX_MASK) >> CLUSTER_MAX_SHIFT;
	if (current_min) {
		C2MIN_QOS_DEL();
		warn("[RSC] cluster2 minfreq is not removed(%dMhz)\n", current_min);
	}

	if (current_max) {
		C2MAX_QOS_DEL();
		warn("[RSC] cluster2 maxfreq is not removed(%dMhz)\n", current_max);
	}

	resourcemgr->cluster0 = 0;
	resourcemgr->cluster1 = 0;
	resourcemgr->cluster2 = 0;

	is_mem_check_stats(&resourcemgr->mem);

#if IS_ENABLED(CONFIG_SCHED_EMS_TUNE)
	if (resourcemgr->dvfs_ctrl.cur_hmp_bst)
		emstune_boost(&emstune_req, 0);

	emstune_remove_request(&emstune_req);
#endif

#if IS_ENABLED(CONFIG_CMU_EWF)
	set_cmuewf(idx_ewf, 0);
#endif
	is_debug_bcm(false, CAMERA_DRIVER);

#if IS_ENABLED(CONFIG_EXYNOS_SCI_DBG_AUTO)
	smc_ppc_enable(0);
#endif

#ifdef DISABLE_BTS_CALC
	bts_calc_disable(0);
#endif
}

static int is_resource_cam_pwr_notifier(bool on)
{
	int ret;

	info("%s (%s)\n", __func__, on ? "ON" : "OFF");

	ret = blocking_notifier_call_chain(&cam_pwr_noti_chain, (unsigned long)on, NULL);

	return notifier_to_errno(ret);
}

int is_resource_get(struct is_resourcemgr *resourcemgr, u32 rsc_type)
{
	int ret = 0;
	u32 rsccount;
	struct is_resource *resource;
	struct is_core *core;
	int i, id;

	FIMC_BUG(!resourcemgr);
	FIMC_BUG(!resourcemgr->private_data);
	FIMC_BUG(rsc_type >= RESOURCE_TYPE_MAX);

	core = (struct is_core *)resourcemgr->private_data;

	mutex_lock(&resourcemgr->rsc_lock);

	rsccount = atomic_read(&core->rsccount);
	resource = GET_RESOURCE(resourcemgr, rsc_type);
	if (!resource) {
		err("[RSC] resource is NULL");
		ret = -EINVAL;
		goto rsc_err;
	}

	if (!core->pdev) {
		err("[RSC] pdev is NULL");
		ret = -EMFILE;
		goto rsc_err;
	}

	if (rsccount >= (IS_STREAM_COUNT + IS_VIDEO_SS5_NUM)) {
		err("[RSC] Invalid rsccount(%d)", rsccount);
		ret = -EMFILE;
		goto rsc_err;
	}

#ifdef ENABLE_KERNEL_LOG_DUMP
	/* to secure kernel log when there was an instance that remain open */
	{
		struct is_resource *resource_ischain;
		resource_ischain = GET_RESOURCE(resourcemgr, RESOURCE_TYPE_ISCHAIN);
		if ((rsc_type != RESOURCE_TYPE_ISCHAIN)	&& rsccount == 1) {
			if (atomic_read(&resource_ischain->rsccount) == 1)
				is_kernel_log_dump(false);
		}
	}
#endif

	if (rsccount == 0) {
		TIME_LAUNCH_STR(LAUNCH_TOTAL);
		pm_stay_awake(&core->pdev->dev);

		is_resource_reset(resourcemgr);

		ret = is_runtime_resume_pre(&resource->pdev->dev);
		if (ret)
			err("is_runtime_resume_pre is fail(%d)", ret);

		/* dvfs controller init */
		ret = is_dvfs_init(resourcemgr);
		if (ret) {
			err("%s: is_dvfs_init failed!\n", __func__);
			goto rsc_err;
		}

		if (IS_ENABLED(SECURE_CAMERA_FACE)) {
			mutex_init(&core->secure_state_lock);
			core->secure_state = IS_STATE_UNSECURE;
			core->scenario = 0;

			info("%s: is secure state has reset\n", __func__);
		}

		core->vender.opening_hint = IS_OPENING_HINT_NONE;
		core->vender.isp_max_width = 0;
		core->vender.isp_max_height = 0;

		for (i = 0; i < MAX_SENSOR_SHARED_RSC; i++) {
			spin_lock_init(&core->shared_rsc_slock[i]);
			atomic_set(&core->shared_rsc_count[i], 0);
		}

		for (i = 0; i < SENSOR_CONTROL_I2C_MAX; i++)
			atomic_set(&core->ixc_rsccount[i], 0);

#ifdef ENABLE_DYNAMIC_MEM
		ret = is_resourcemgr_init_dynamic_mem(resourcemgr);
		if (ret) {
			err("is_resourcemgr_init_dynamic_mem is fail(%d)\n", ret);
			goto p_err;
		}
#endif

		for (i = 0; i < resourcemgr->num_phy_ldos; i++) {
			ret = regulator_enable(resourcemgr->phy_ldos[i]);
			if (ret) {
				err("failed to enable PHY LDO[%d]", i);
				goto p_err;
			}
			usleep_range(100, 101);
		}
	}

	if (atomic_read(&resource->rsccount) == 0) {
		switch (rsc_type) {
		case RESOURCE_TYPE_SENSOR0:
		case RESOURCE_TYPE_SENSOR1:
		case RESOURCE_TYPE_SENSOR2:
		case RESOURCE_TYPE_SENSOR3:
		case RESOURCE_TYPE_SENSOR4:
		case RESOURCE_TYPE_SENSOR5:
			id = rsc_type - RESOURCE_TYPE_SENSOR0;
#ifdef CONFIG_PM
			pm_runtime_get_sync(&resource->pdev->dev);
#else
			is_sensor_runtime_resume(&resource->pdev->dev);
#endif
			set_bit(IS_RM_SS0_POWER_ON + id, &resourcemgr->state);
			break;
		case RESOURCE_TYPE_ISCHAIN:
			if (test_bit(IS_RM_POWER_ON, &resourcemgr->state)) {
				err("all resource is not power off(%lX)", resourcemgr->state);
				ret = -EINVAL;
				goto p_err;
			}

			ret = core->noti_register_state ? is_resource_cam_pwr_notifier(true) : 0;
			if (ret) {
				err("is_resource_cam_pwr_notifier is fail (%d)", ret);
				goto p_err;
			}

			ret = is_debug_open(resourcemgr->minfo);
			if (ret) {
				err("is_debug_open is fail(%d)", ret);
				goto p_err;
			}

			ret = is_interface_open(&core->interface);
			if (ret) {
				err("is_interface_open is fail(%d)", ret);
				goto p_err;
			}

			if (IS_ENABLED(SECURE_CAMERA_MEM_SHARE)) {
				ret = is_resourcemgr_init_secure_mem(resourcemgr);
				if (ret) {
					err("is_resourcemgr_init_secure_mem is fail(%d)\n",
							ret);
					goto p_err;
				}
			}

			ret = is_ischain_power(&core->ischain[0], 1);
			if (ret) {
				err("is_ischain_power is fail(%d)", ret);
				is_ischain_power(&core->ischain[0], 0);
				goto p_err;
			}

			set_bit(IS_RM_ISC_POWER_ON, &resourcemgr->state);
			set_bit(IS_RM_POWER_ON, &resourcemgr->state);

#if defined(DISABLE_CORE_IDLE_STATE)
			schedule_work(&resourcemgr->c2_disable_work);
#endif

			is_bts_scen(resourcemgr, 0, true);
			resourcemgr->cur_bts_scen_idx = 0;

			is_hw_ischain_qe_cfg();

			ret = is_load_bin();
			if (ret < 0) {
				err("is_load_bin() is fail(%d)", ret);
				goto p_err;
			}

			break;
		default:
			err("[RSC] resource type(%d) is invalid", rsc_type);
			BUG();
			break;
		}

		is_vender_resource_get(&core->vender, rsc_type);
	}

	if (rsccount == 0) {
#if defined(USE_OFFLINE_PROCESSING)
		ret = is_runtime_resume_post(&resource->pdev->dev);
		if (ret)
			err("is_runtime_resume_post is fail(%d)", ret);
#endif
#ifdef ENABLE_HWACG_CONTROL
		/* for CSIS HWACG */
		is_hw_csi_qchannel_enable_all(true);
#endif
		/* when the first sensor device be opened */
		if (rsc_type < RESOURCE_TYPE_ISCHAIN)
			is_hw_camif_init();

		/* It must be done after power on, because of accessing mux register */
		is_camif_wdma_init();
	}

p_err:
	atomic_inc(&resource->rsccount);
	atomic_inc(&core->rsccount);

	info("[RSC] rsctype: %d, rsccount: device[%d], core[%d]\n", rsc_type,
			atomic_read(&resource->rsccount), rsccount + 1);
rsc_err:
	mutex_unlock(&resourcemgr->rsc_lock);

	return ret;
}
PST_EXPORT_SYMBOL(is_resource_get);

int is_resource_put(struct is_resourcemgr *resourcemgr, u32 rsc_type)
{
	int i, id, ret = 0;
	u32 rsccount;
	struct is_resource *resource;
	struct is_core *core;

	FIMC_BUG(!resourcemgr);
	FIMC_BUG(!resourcemgr->private_data);
	FIMC_BUG(rsc_type >= RESOURCE_TYPE_MAX);

	core = (struct is_core *)resourcemgr->private_data;

	mutex_lock(&resourcemgr->rsc_lock);

	rsccount = atomic_read(&core->rsccount);
	resource = GET_RESOURCE(resourcemgr, rsc_type);
	if (!resource) {
		err("[RSC] resource is NULL");
		ret = -EINVAL;
		goto p_err;
	}

	if (!core->pdev) {
		err("[RSC] pdev is NULL");
		ret = -EMFILE;
		goto p_err;
	}

	if (rsccount == 0) {
		err("[RSC] Invalid rsccount(%d)\n", rsccount);
		ret = -EMFILE;
		goto p_err;
	}

	if (rsccount == 1) {
#if defined(USE_OFFLINE_PROCESSING)
		ret = is_runtime_suspend_pre(&resource->pdev->dev);
		if (ret)
			err("is_runtime_suspend_free is fail(%d)", ret);
#endif
#ifdef ENABLE_HWACG_CONTROL
		/* for CSIS HWACG */
		is_hw_csi_qchannel_enable_all(false);
#endif
	}

	/* local update */
	if (atomic_read(&resource->rsccount) == 1) {
		switch (rsc_type) {
		case RESOURCE_TYPE_SENSOR0:
		case RESOURCE_TYPE_SENSOR1:
		case RESOURCE_TYPE_SENSOR2:
		case RESOURCE_TYPE_SENSOR3:
		case RESOURCE_TYPE_SENSOR4:
		case RESOURCE_TYPE_SENSOR5:
			id = rsc_type - RESOURCE_TYPE_SENSOR0;
#if defined(CONFIG_PM)
			pm_runtime_put_sync(&resource->pdev->dev);
#else
			is_sensor_runtime_suspend(&resource->pdev->dev);
#endif
			clear_bit(IS_RM_SS0_POWER_ON + id, &resourcemgr->state);
			break;
		case RESOURCE_TYPE_ISCHAIN:
			is_load_clear();

			if (IS_ENABLED(SECURE_CAMERA_FACE)) {
				if (is_secure_func(core, NULL,
							IS_SECURE_CAMERA_FACE,
							core->scenario,
							SMC_SECCAM_UNPREPARE))
					err("Failed to is_secure_func(FACE, UNPREPARE)");
			}

			ret = is_itf_power_down_wrap(core);
			if (ret)
				err("power down cmd is fail(%d)", ret);

			ret = is_ischain_power(&core->ischain[0], 0);
			if (ret)
				err("is_ischain_power is fail(%d)", ret);

			ret = is_interface_close(&core->interface);
			if (ret)
				err("is_interface_close is fail(%d)", ret);

			if (IS_ENABLED(SECURE_CAMERA_MEM_SHARE)) {
				ret = is_resourcemgr_deinit_secure_mem(resourcemgr);
				if (ret)
					err("is_resourcemgr_deinit_secure_mem is fail(%d)",
							ret);
			}

			ret = is_debug_close();
			if (ret)
				err("is_debug_close is fail(%d)", ret);

			ret = core->noti_register_state ? is_resource_cam_pwr_notifier(false) : 0;
			if (ret)
				err("is_resource_cam_pwr_notifier is fail (%d)", ret);

			/* IS_BINARY_LOADED must be cleared after ddk shut down */
			clear_bit(IS_RM_ISC_POWER_ON, &resourcemgr->state);

#if defined(DISABLE_CORE_IDLE_STATE)
			/* CPU idle on/off */
			info("%s: call cpuidle_resume()\n", __func__);
			flush_work(&resourcemgr->c2_disable_work);

			cpuidle_resume_and_unlock();
#endif

			if (resourcemgr->cur_bts_scen_idx) {
				is_bts_scen(resourcemgr, resourcemgr->cur_bts_scen_idx, false);
				resourcemgr->cur_bts_scen_idx = 0;
			}
			is_bts_scen(resourcemgr, 0, false);

			break;
		}

		is_vender_resource_put(&core->vender, rsc_type);
	}

	/* global update */
	if (atomic_read(&core->rsccount) == 1) {
		for (i = resourcemgr->num_phy_ldos - 1; i >= 0; i--) {
			regulator_disable(resourcemgr->phy_ldos[i]);
			usleep_range(1000, 1001);
		}

		is_resource_clear(resourcemgr);

#ifdef ENABLE_DYNAMIC_MEM
		ret = is_resourcemgr_deinit_dynamic_mem(resourcemgr);
		if (ret)
			err("is_resourcemgr_deinit_dynamic_mem is fail(%d)", ret);
#endif

		is_vendor_resource_clean(core);

		core->vender.closing_hint = IS_CLOSING_HINT_NONE;

		ret = is_runtime_suspend_post(&resource->pdev->dev);
		if (ret)
			err("is_runtime_suspend_post is fail(%d)", ret);

		pm_relax(&core->pdev->dev);

		clear_bit(IS_RM_POWER_ON, &resourcemgr->state);
	}

	atomic_dec(&resource->rsccount);
	atomic_dec(&core->rsccount);
	info("[RSC] rsctype: %d, rsccount: device[%d], core[%d]\n", rsc_type,
			atomic_read(&resource->rsccount), rsccount - 1);
p_err:
	mutex_unlock(&resourcemgr->rsc_lock);

	return ret;
}
PST_EXPORT_SYMBOL(is_resource_put);

int is_resource_ioctl(struct is_resourcemgr *resourcemgr, struct v4l2_control *ctrl)
{
	int ret = 0;

	FIMC_BUG(!resourcemgr);
	FIMC_BUG(!ctrl);

	mutex_lock(&resourcemgr->qos_lock);
	switch (ctrl->id) {
	/* APOLLO CPU0~3 */
	case V4L2_CID_IS_DVFS_CLUSTER0:
		{
			u32 current_min, current_max;
			u32 request_min, request_max;

			current_min = (resourcemgr->cluster0 & CLUSTER_MIN_MASK) >> CLUSTER_MIN_SHIFT;
			current_max = (resourcemgr->cluster0 & CLUSTER_MAX_MASK) >> CLUSTER_MAX_SHIFT;
			request_min = (ctrl->value & CLUSTER_MIN_MASK) >> CLUSTER_MIN_SHIFT;
			request_max = (ctrl->value & CLUSTER_MAX_MASK) >> CLUSTER_MAX_SHIFT;

			if (current_min) {
				if (request_min)
					C0MIN_QOS_UPDATE(request_min);
				else
					C0MIN_QOS_DEL();
			} else {
				if (request_min)
					C0MIN_QOS_ADD(request_min);
			}

			if (current_max) {
				if (request_max)
					C0MAX_QOS_UPDATE(request_max);
				else
					C0MAX_QOS_DEL();
			} else {
				if (request_max)
					C0MAX_QOS_ADD(request_max);
			}

			info("[RSC] cluster0 minfreq : %dMhz\n", request_min);
			info("[RSC] cluster0 maxfreq : %dMhz\n", request_max);
			resourcemgr->cluster0 = (request_max << CLUSTER_MAX_SHIFT) | request_min;
		}
		break;
	/* ATLAS CPU4~5 */
	case V4L2_CID_IS_DVFS_CLUSTER1:
		{
			u32 current_min, current_max;
			u32 request_min, request_max;

			current_min = (resourcemgr->cluster1 & CLUSTER_MIN_MASK) >> CLUSTER_MIN_SHIFT;
			current_max = (resourcemgr->cluster1 & CLUSTER_MAX_MASK) >> CLUSTER_MAX_SHIFT;
			request_min = (ctrl->value & CLUSTER_MIN_MASK) >> CLUSTER_MIN_SHIFT;
			request_max = (ctrl->value & CLUSTER_MAX_MASK) >> CLUSTER_MAX_SHIFT;

			if (current_min) {
				if (request_min)
					C1MIN_QOS_UPDATE(request_min);
				else
					C1MIN_QOS_DEL();
			} else {
				if (request_min)
					C1MIN_QOS_ADD(request_min);
			}

			if (current_max) {
				if (request_max)
					C1MAX_QOS_UPDATE(request_max);
				else
					C1MAX_QOS_DEL();
			} else {
				if (request_max)
					C1MAX_QOS_ADD(request_max);
			}

			info("[RSC] cluster1 minfreq : %dMhz\n", request_min);
			info("[RSC] cluster1 maxfreq : %dMhz\n", request_max);
			resourcemgr->cluster1 = (request_max << CLUSTER_MAX_SHIFT) | request_min;
		}
		break;
	/* ATLAS CPU6~7 */
	case V4L2_CID_IS_DVFS_CLUSTER2:
		{
#if defined(PM_QOS_CLUSTER2_FREQ_MAX_DEFAULT_VALUE)
			u32 current_min, current_max;
			u32 request_min, request_max;

			current_min = (resourcemgr->cluster2 & CLUSTER_MIN_MASK) >> CLUSTER_MIN_SHIFT;
			current_max = (resourcemgr->cluster2 & CLUSTER_MAX_MASK) >> CLUSTER_MAX_SHIFT;
			request_min = (ctrl->value & CLUSTER_MIN_MASK) >> CLUSTER_MIN_SHIFT;
			request_max = (ctrl->value & CLUSTER_MAX_MASK) >> CLUSTER_MAX_SHIFT;

			if (current_min) {
				if (request_min)
					C2MIN_QOS_UPDATE(request_min);
				else
					C2MIN_QOS_DEL();
			} else {
				if (request_min)
					C2MIN_QOS_ADD(request_min);
			}

			if (current_max) {
				if (request_max)
					C2MAX_QOS_UPDATE(request_max);
				else
					C2MAX_QOS_DEL();
			} else {
				if (request_max)
					C2MAX_QOS_ADD(request_max);
			}

			info("[RSC] cluster2 minfreq : %dMhz\n", request_min);
			info("[RSC] cluster2 maxfreq : %dMhz\n", request_max);
			resourcemgr->cluster2 = (request_max << CLUSTER_MAX_SHIFT) | request_min;
#endif
		}
		break;
	}
	mutex_unlock(&resourcemgr->qos_lock);

	return ret;
}

void is_resource_set_global_param(struct is_resourcemgr *resourcemgr, void *device)
{
	bool video_mode;
	struct is_device_ischain *ischain = device;
	struct is_global_param *global_param = &resourcemgr->global_param;

	mutex_lock(&global_param->lock);

	atomic_inc(&resourcemgr->global_param.sensor_cnt);

	if (!global_param->state) {
		video_mode = IS_VIDEO_SCENARIO(ischain->setfile & IS_SETFILE_MASK);
		global_param->video_mode = video_mode;
		is_get_is_core()->hardware.video_mode = video_mode;
		minfo("video mode %d\n", ischain, video_mode);

		global_param->llc_state = 0;
		if (ischain->llc_mode == false)
			set_bit(LLC_DISABLE, &global_param->llc_state);

		is_hw_configure_llc(true, ischain, &global_param->llc_state);
	}

	set_bit(ischain->instance, &global_param->state);

	mutex_unlock(&global_param->lock);
}

void is_resource_clear_global_param(struct is_resourcemgr *resourcemgr, void *device)
{
	struct is_device_ischain *ischain = device;
	struct is_global_param *global_param = &resourcemgr->global_param;

	mutex_lock(&global_param->lock);

	clear_bit(ischain->instance, &global_param->state);

	if (!global_param->state) {
		global_param->video_mode = false;
		is_get_is_core()->hardware.video_mode = false;

		is_hw_configure_llc(false, ischain, &global_param->llc_state);
	}

	atomic_dec(&resourcemgr->global_param.sensor_cnt);

	mutex_unlock(&global_param->lock);
}

int is_resource_update_lic_sram(struct is_resourcemgr *resourcemgr, void *device, bool on)
{
	struct is_device_ischain *ischain = device;
	struct is_device_sensor *sensor;
	struct is_group *group;
	struct is_lic_sram *lic_sram;
	u32 taa_id;
	int taa_sram_sum;

	FIMC_BUG(!resourcemgr);
	FIMC_BUG(!device);

	/* TODO: DT flag for LIC use */

	sensor = ischain->sensor;
	FIMC_BUG(!sensor);

	lic_sram = &resourcemgr->lic_sram;
	group = &ischain->group_3aa;

	if (!test_bit(IS_GROUP_OPEN, &group->state)) {
		mwarn("[RSC] group_3aa is closed", ischain);
		return 0;
	}

	/* In case of remosic capture that is not use PDP & 3AA, it is returned. */
	if (sensor && !test_bit(IS_SENSOR_OTF_OUTPUT, &sensor->state))
		goto p_skip_update_sram;

	taa_id = group->id - GROUP_ID_3AA0;

	if (on) {
		lic_sram->taa_sram[taa_id] = sensor->sensor_width;
		atomic_add(sensor->sensor_width, &lic_sram->taa_sram_sum);
	} else {
		lic_sram->taa_sram[taa_id] = 0;
		taa_sram_sum = atomic_sub_return(sensor->sensor_width, &lic_sram->taa_sram_sum);
		if (taa_sram_sum < 0) {
			mwarn("[RSC] Invalid taa_sram_sum %d\n", ischain, taa_sram_sum);
			atomic_set(&lic_sram->taa_sram_sum, 0);
		}
	}

p_skip_update_sram:
	minfo("[RSC] LIC taa_sram([0]%zu, [1]%zu, [2]%zu, [3]%zu, [sum]%d)\n", ischain,
		lic_sram->taa_sram[0],
		lic_sram->taa_sram[1],
		lic_sram->taa_sram[2],
		lic_sram->taa_sram[3],
		atomic_read(&lic_sram->taa_sram_sum));
	return 0;
}

int is_logsync(u32 sync_id, u32 msg_test_id)
{
	/* print kernel sync log */
	log_sync(sync_id);

	return 0;
}

struct is_dbuf_q *is_init_dbuf_q(void)
{
	void *ret;
	int i_id, i_list;
	int num_list = MAX_DBUF_LIST;
	struct is_dbuf_q *dbuf_q;

	dbuf_q = vzalloc(sizeof(struct is_dbuf_q));
	if (!dbuf_q) {
		err("failed to allocate dbuf_q");
		return ERR_PTR(-ENOMEM);
	}

	for (i_id = 0; i_id < ID_DBUF_MAX; i_id++) {
		dbuf_q->dbuf_list[i_id] = vzalloc(sizeof(struct is_dbuf_list) * num_list);
		if (!dbuf_q->dbuf_list[i_id]) {
			err("failed to allocate dbuf_list");
			ret = ERR_PTR(-ENOMEM);
			goto err_alloc_list;
		}

		mutex_init(&dbuf_q->lock[i_id]);

		dbuf_q->queu_count[i_id] = 0;
		INIT_LIST_HEAD(&dbuf_q->queu_list[i_id]);

		dbuf_q->free_count[i_id] = 0;
		INIT_LIST_HEAD(&dbuf_q->free_list[i_id]);
	}

	/* set free list */
	for (i_id = 0; i_id < ID_DBUF_MAX; i_id++) {
		for (i_list = 0; i_list < num_list; i_list++) {
			list_add_tail(&dbuf_q->dbuf_list[i_id][i_list].list,
					&dbuf_q->free_list[i_id]);
			dbuf_q->free_count[i_id]++;
		}
	}

	return dbuf_q;

err_alloc_list:
	while (i_id-- > 0) {
		if (dbuf_q->dbuf_list[i_id])
			vfree(dbuf_q->dbuf_list[i_id]);
	}

	vfree(dbuf_q);

	return ret;
}

/* cache maintenance for user buffer */
void is_deinit_dbuf_q(struct is_dbuf_q *dbuf_q)
{
	int i_id;

	for (i_id = 0; i_id < ID_DBUF_MAX; i_id++)
		vfree(dbuf_q->dbuf_list[i_id]);

	vfree(dbuf_q);
}

static void is_flush_dma_buf(struct is_dbuf_q *dbuf_q, u32 dma_id, u32 qcnt)
{
	struct is_dbuf_list *dbuf_list;

	while (dbuf_q->queu_count[dma_id] > qcnt) {
		/* get queue list */
		dbuf_list = list_first_entry(&dbuf_q->queu_list[dma_id],
					struct is_dbuf_list, list);
		list_del(&dbuf_list->list);
		dbuf_q->queu_count[dma_id]--;

		/* put free list */
		list_add_tail(&dbuf_list->list, &dbuf_q->free_list[dma_id]);
		dbuf_q->free_count[dma_id]++;
	}
}

void is_q_dbuf_q(struct is_dbuf_q *dbuf_q, struct is_sub_dma_buf *sdbuf, u32 qcnt)
{
	int dma_id, num_planes, p;
	struct is_dbuf_list *dbuf_list;

	dma_id = is_get_dma_id(sdbuf->vid);
	if (dma_id < 0)
		return;

	mutex_lock(&dbuf_q->lock[dma_id]);

	if (dbuf_q->queu_count[dma_id] > qcnt) {
		warn("dma_id(%d) dbuf qcnt(%d) > vb2 qcnt(%d)", dma_id,
			dbuf_q->queu_count[dma_id], qcnt);

		is_flush_dma_buf(dbuf_q, dma_id, qcnt);
	}

	if (!dbuf_q->free_count[dma_id] || list_empty(&dbuf_q->free_list[dma_id])) {
		warn("dma_id(%d) free list is NULL[f(%d)/q(%d)]", dma_id,
			dbuf_q->free_count[dma_id], dbuf_q->queu_count[dma_id]);

		mutex_unlock(&dbuf_q->lock[dma_id]);
		return;
	}

	/* get free list */
	dbuf_list = list_first_entry(&dbuf_q->free_list[dma_id],
				struct is_dbuf_list, list);
	list_del(&dbuf_list->list);
	dbuf_q->free_count[dma_id]--;

	/* copy */
	num_planes = sdbuf->num_plane * sdbuf->num_buffer;
	for (p = 0; p < num_planes && p < IS_MAX_PLANES; p++)
		dbuf_list->dbuf[p] = sdbuf->dbuf[p];

	/* put queue list */
	list_add_tail(&dbuf_list->list, &dbuf_q->queu_list[dma_id]);
	dbuf_q->queu_count[dma_id]++;

	mutex_unlock(&dbuf_q->lock[dma_id]);
}

void is_dq_dbuf_q(struct is_dbuf_q *dbuf_q, u32 dma_id, enum dma_data_direction dir)
{
	u32 p;
	struct is_dbuf_list *dbuf_list;

	mutex_lock(&dbuf_q->lock[dma_id]);

	if (!dbuf_q->queu_count[dma_id] || list_empty(&dbuf_q->queu_list[dma_id])) {
		warn("dma_id(%d) queue list is NULL[f(%d)/q(%d)]", dma_id,
			dbuf_q->free_count[dma_id], dbuf_q->queu_count[dma_id]);

		mutex_unlock(&dbuf_q->lock[dma_id]);
		return;
	}

	/* get queue list */
	dbuf_list = list_first_entry(&dbuf_q->queu_list[dma_id],
				struct is_dbuf_list, list);
	list_del(&dbuf_list->list);
	dbuf_q->queu_count[dma_id]--;

	/* put free list */
	list_add_tail(&dbuf_list->list, &dbuf_q->free_list[dma_id]);
	dbuf_q->free_count[dma_id]++;

	mutex_unlock(&dbuf_q->lock[dma_id]);

	/* cache maintenance */
	for (p = 0; p < IS_MAX_PLANES && dbuf_list->dbuf[p]; p++) {
		if (dir == DMA_FROM_DEVICE)	/* cache inv */
			dma_buf_begin_cpu_access(dbuf_list->dbuf[p], DMA_FROM_DEVICE);
		else if (dir == DMA_TO_DEVICE)	/* cache clean */
			dma_buf_end_cpu_access(dbuf_list->dbuf[p], DMA_TO_DEVICE);
		else
			warn("invalid direction(%d), type(%d)", dir, dma_id);
	}

	if (!p)
		warn("dbuf is NULL. dma_id(%d)", dma_id);
}

u32 is_get_shot_timeout(void)
{
	struct is_core *core= is_get_is_core();

	return core->resourcemgr.shot_timeout;
}

int register_cam_pwr_notifier(struct notifier_block *nb)
{
	struct is_core *core = pablo_get_core_async();
	int ret;

	if (!core) {
		err("Camera core is NULL");
		return -ENODEV;
	}

	core->noti_register_state = true;

	ret = blocking_notifier_chain_register(&cam_pwr_noti_chain, nb);
	if (core && atomic_read(&core->rsccount)) {
		/* Currently blocking_notifier_chain_register function always returns zero
		 * If the Camera is on, return 1
		 */
		ret = 1;
	}

	info("%s ret(%d)\n", __func__, ret);

	return ret;
}
EXPORT_SYMBOL(register_cam_pwr_notifier);

int unregister_cam_pwr_notifier(struct notifier_block *nb)
{
	struct is_core *core = pablo_get_core_async();

	if (!core) {
		err("Camera core is NULL");
		return -ENODEV;
	}

	core->noti_register_state = false;

	info("%s\n", __func__);
	return blocking_notifier_chain_unregister(&cam_pwr_noti_chain, nb);
}
EXPORT_SYMBOL(unregister_cam_pwr_notifier);
