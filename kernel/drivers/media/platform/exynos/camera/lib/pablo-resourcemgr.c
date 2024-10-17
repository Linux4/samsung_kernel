// SPDX-License-Identifier: GPL
/*
 * Samsung Exynos SoC series Pablo driver
 *
 * Exynos Pablo image subsystem functions
 *
 * Copyright (c) 2023 Samsung Electronics Co., Ltd
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/version.h>
#include <videodev2_exynos_camera.h>
#include <soc/samsung/freq-qos-tracer.h>
#include <linux/of_reserved_mem.h>

#include "is-device-ischain.h"
#include "is-device-sensor.h"
#include "pablo-binary.h"
#include "pablo-resourcemgr.h"
#include "is-hw.h"

static struct freq_qos_request exynos_isp_freq_qos[IS_FREQ_QOS_MAX];
static inline unsigned long pablo_set_addr(struct is_priv_buf *pb, enum is_addr_type type)
{
	if (type == KVADDR_TYPE)
		return CALL_BUFOP(pb, kvaddr, pb);
	else if (type == PHADDR_TYPE)
		return CALL_BUFOP(pb, phaddr, pb);

	return CALL_BUFOP(pb, dvaddr, pb);
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

	if (IS_ENABLED(CONFIG_PABLO_HW_HELPER_V1)) {
		for (i = 0; i < SENSOR_POSITION_MAX; i++) {
			/* calibration data for each sensor postion */
			minfo->pb_cal[i] =
				CALL_PTR_MEMOP(mem, alloc, mem->priv, TOTAL_CAL_DATA_SIZE, NULL, 0);
			if (IS_ERR_OR_NULL(minfo->pb_cal[i])) {
				err("failed to allocate buffer for TOTAL_CAL_DATA");
				return -ENOMEM;
			}
			minfo->total_size += minfo->pb_cal[i]->size;
			info("[RSC]memory_alloc(TOTAL_CAL_DATA_SIZE[%d]): 0x%08lx\n", i,
			     minfo->pb_cal[i]->size);
		}
	}

	/* library logging */
	if (!IS_ENABLED(CLOG_RESERVED_MEM) && IS_ENABLED(CONFIG_PABLO_HW_HELPER_V1)) {
		minfo->pb_debug = mem->contig_alloc(DEBUG_REGION_SIZE + 0x10);
		if (IS_ERR_OR_NULL(minfo->pb_debug)) {
			/* retry by ION */
			minfo->pb_debug = CALL_PTR_MEMOP(mem, alloc, mem->priv,
							 DEBUG_REGION_SIZE + 0x10, NULL, 0);
			if (IS_ERR_OR_NULL(minfo->pb_debug)) {
				err("failed to allocate buffer for DEBUG_REGION");
				return -ENOMEM;
			}
		}
		minfo->total_size += minfo->pb_debug->size;
		info("[RSC]memory_alloc(DEBUG_REGION_SIZE): 0x%08lx\n", minfo->pb_debug->size);
	}

	/* parameter region */
	minfo->pb_pregion = CALL_PTR_MEMOP(mem, alloc, mem->priv,
					   (IS_STREAM_COUNT * PARAM_REGION_SIZE), NULL, 0);
	if (IS_ERR_OR_NULL(minfo->pb_pregion)) {
		err("failed to allocate buffer for PARAM_REGION");
		return -ENOMEM;
	}
	minfo->total_size += minfo->pb_pregion->size;
	info("[RSC]memory_alloc(PARAM_REGION_SIZE x %d): 0x%08lx\n", IS_STREAM_COUNT,
	     minfo->pb_pregion->size);

	if (IS_ENABLED(CONFIG_PABLO_HW_HELPER_V1)) {
		/* sfr dump addr region */
		minfo->pb_sfr_dump_addr =
			CALL_PTR_MEMOP(mem, alloc, mem->priv, SFR_DUMP_SIZE, NULL, 0);
		if (IS_ERR_OR_NULL(minfo->pb_sfr_dump_addr)) {
			err("failed to allocate buffer for SFR_DUMP_ADDR");
			return -ENOMEM;
		}
		minfo->total_size += minfo->pb_sfr_dump_addr->size;
		info("[RSC]memory_alloc(SFR_DUMP_ADDR_SIZE): 0x%08lx\n",
		     minfo->pb_sfr_dump_addr->size);

		/* sfr dump value region */
		minfo->pb_sfr_dump_value =
			CALL_PTR_MEMOP(mem, alloc, mem->priv, SFR_DUMP_SIZE, NULL, 0);
		if (IS_ERR_OR_NULL(minfo->pb_sfr_dump_value)) {
			err("failed to allocate buffer for SFR_DUMP_VALUE");
			return -ENOMEM;
		}
		minfo->total_size += minfo->pb_sfr_dump_value->size;
		info("[RSC]memory_alloc(SFR_DUMP_VALUE_SIZE): 0x%08lx\n",
		     minfo->pb_sfr_dump_value->size);
	}

#if !defined(ENABLE_DYNAMIC_MEM)
	/* 3aa/isp internal DMA buffer */
	minfo->pb_taaisp = CALL_PTR_MEMOP(mem, alloc, mem->priv, TAAISP_DMA_SIZE, NULL, 0);
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
		minfo->pb_dummy =
			CALL_PTR_MEMOP(mem, alloc, mem->priv, DUMMY_DMA_SIZE, "camera_heap", 0);
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

static int is_resourcemgr_alloc_dynamic_mem(struct is_resourcemgr *resourcemgr)
{
	struct is_mem *mem = &resourcemgr->mem;
	struct is_minfo *minfo = resourcemgr->minfo;
	int ret;

	if (TAAISP_DMA_SIZE > 0) {
		/* 3aa/isp internal DMA buffer */
		minfo->pb_taaisp = CALL_PTR_MEMOP(mem, alloc, mem->priv, TAAISP_DMA_SIZE, NULL, 0);
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
#if defined(USE_CAMERA_HEAP)
		minfo->pb_tnr = CALL_PTR_MEMOP(mem, alloc, mem->priv, TNR_DMA_SIZE, CAMERA_HEAP_NAME, 0);
#else
		minfo->pb_tnr = CALL_PTR_MEMOP(mem, alloc, mem->priv, TNR_DMA_SIZE, NULL, 0);
#endif
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
		minfo->pb_orbmch = CALL_PTR_MEMOP(mem, alloc, mem->priv, ORBMCH_DMA_SIZE, NULL, 0);
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

static struct pablo_rscmgr_ops rscmgr_func;
struct pablo_rscmgr_ops *pablo_get_rscmgr_ops(void)
{
	return &rscmgr_func;
}
EXPORT_SYMBOL_GPL(pablo_get_rscmgr_ops);

#ifdef ENABLE_KERNEL_LOG_DUMP
static void *is_get_log_kernel(void)
{
#if IS_ENABLED(CONFIG_EXYNOS_SNAPSHOT)
	return (void *)exynos_ss_get_item_vaddr("log_kernel");
#elif IS_ENABLED(CONFIG_DEBUG_SNAPSHOT)
	return (void *)dbg_snapshot_get_item_vaddr("log_kernel");
#endif
	return NULL;
}
static struct pablo_rscmgr_sys_ops rscmgr_sys_ops = {
#ifdef ENABLE_KERNEL_LOG_DUMP
	.get_log_kernel = is_get_log_kernel,
#endif
#if IS_ENABLED(CONFIG_EXYNOS_BTS)
	.bts_scenidx = bts_get_scenindex,
	.bts_add_scen = bts_add_scenario,
	.bts_del_scen = bts_del_scenario,
#endif
	.vmap = vmap,
};
int is_kernel_log_dump(struct is_resourcemgr *resourcemgr, bool overwrite)
{
	static int dumped;
	void *log_kernel = NULL;
	unsigned long long when;
	unsigned long usec;

	if (dumped && !overwrite) {
		when = resourcemgr->kernel_log_time;
		usec = do_div(when, NSEC_PER_SEC) / NSEC_PER_USEC;
		info("kernel log was saved already at [%5lu.%06lu]\n", (unsigned long)when, usec);

		return -ENOSPC;
	}

	log_kernel = rscmgr_sys_ops.get_log_kernel();
	if (!log_kernel)
		return -EINVAL;

	if (resourcemgr->kernel_log_buf) {
		resourcemgr->kernel_log_time = local_clock();

		info("kernel log saved to %pK(%pK) from %pK\n", resourcemgr->kernel_log_buf,
			(void *)virt_to_phys(resourcemgr->kernel_log_buf), log_kernel);
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
EXPORT_SYMBOL(is_kernel_log_dump);
#endif

struct pablo_rscmgr_sys_ops *pablo_get_rscmgr_sys_ops(void)
{
	return &rscmgr_sys_ops;
}
EXPORT_SYMBOL_GPL(pablo_get_rscmgr_sys_ops);

static unsigned long pablo_vmap(unsigned long addr, unsigned int size)
{
	int i;
	unsigned int npages = size >> PAGE_SHIFT;
	pgprot_t prot = pgprot_writecombine(PAGE_KERNEL);
	struct page **pages;
	void *vaddr;

	pages = kmalloc_array(npages, sizeof(struct page *), GFP_ATOMIC);
	if (!pages)
		return 0;

	for (i = 0; i < npages; i++) {
		pages[i] = phys_to_page(addr);
		addr += PAGE_SIZE;
	}

	vaddr = rscmgr_sys_ops.vmap(pages, npages, VM_MAP, prot);
	kfree(pages);

	return (unsigned long)vaddr;
}

static void is_resourcemgr_dynamic_mem(struct is_priv_buf *ip, bool init, char *name)
{
	unsigned long kva;
	dma_addr_t dva;

	if (ip) {
		if (init) {
			kva = pablo_set_addr(ip, KVADDR_TYPE);
			dva = pablo_set_addr(ip, DVADDR_TYPE);
			info("[RSC] %s_DMA memory kva:0x%pK, dva: %pad\n", name, (void *)kva, &dva);
		} else {
			CALL_VOID_BUFOP(ip, free, ip);
			ip = NULL;
		}
	}
}

int pablo_rscmgr_init_log_rmem(struct is_resourcemgr *resourcemgr, struct reserved_mem *rmem)
{
	resourcemgr->minfo->phaddr_debug = rmem->base;
	resourcemgr->minfo->kvaddr_debug = pablo_vmap(rmem->base, rmem->size);
	if (!resourcemgr->minfo->kvaddr_debug) {
		probe_err("failed to map [%s]", rmem->name);
		return -EINVAL;
	}

	memset((void *)resourcemgr->minfo->kvaddr_debug, 0x0, rmem->size - 1);
#if IS_ENABLED(CONFIG_DEBUG_SNAPSHOT)
	dbg_snapshot_add_bl_item_info("log_camera", rmem->base, rmem->size);
#endif

	probe_info("[RSC]log rmem(V/P/S): %pK/%pap/%pap\n", (void *)resourcemgr->minfo->kvaddr_debug,
		   &rmem->base, &rmem->size);

	return 0;
}
EXPORT_SYMBOL(pablo_rscmgr_init_log_rmem);

int is_resourcemgr_init_mem(struct is_resourcemgr *resourcemgr)
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
	if (minfo->pb_debug && !minfo->kvaddr_debug) {
		minfo->kvaddr_debug = pablo_set_addr(minfo->pb_debug, KVADDR_TYPE);
		minfo->phaddr_debug = pablo_set_addr(minfo->pb_debug, PHADDR_TYPE);
		minfo->kvaddr_debug_cnt = minfo->kvaddr_debug + DEBUG_REGION_SIZE;
	}

	if (minfo->pb_sfr_dump_addr)
		minfo->kvaddr_sfr_dump_addr = pablo_set_addr(minfo->pb_sfr_dump_addr, KVADDR_TYPE);

	if (minfo->pb_sfr_dump_value)
		minfo->kvaddr_sfr_dump_value =
			pablo_set_addr(minfo->pb_sfr_dump_value, KVADDR_TYPE);

	minfo->kvaddr_region = pablo_set_addr(minfo->pb_pregion, KVADDR_TYPE);

	for (i = 0; i < SENSOR_POSITION_MAX; i++)
		if (minfo->pb_cal[i])
			minfo->kvaddr_cal[i] = pablo_set_addr(minfo->pb_cal[i], KVADDR_TYPE);

	if (DUMMY_DMA_SIZE) {
		minfo->dvaddr_dummy = pablo_set_addr(minfo->pb_dummy, DVADDR_TYPE);
		minfo->kvaddr_dummy = pablo_set_addr(minfo->pb_dummy, KVADDR_TYPE);
		minfo->phaddr_dummy = pablo_set_addr(minfo->pb_dummy, PHADDR_TYPE);

		probe_info("[RSC] Dummy buffer: dva(%pad) kva(0x%pK) pha(%pa)\n",
			   &minfo->dvaddr_dummy, (void *)minfo->kvaddr_dummy, &minfo->phaddr_dummy);
	}

	probe_info("[RSC] Kernel virtual for debug: 0x%pK\n", (void *)minfo->kvaddr_debug);
	probe_info("[RSC] is_init_mem done\n");
p_err:
	return ret;
}
EXPORT_SYMBOL(is_resourcemgr_init_mem);

#ifdef ENABLE_DYNAMIC_MEM
int is_resourcemgr_init_dynamic_mem(struct is_resourcemgr *resourcemgr)
{
	struct is_minfo *minfo = resourcemgr->minfo;
	int ret = 0;

	ret = is_resourcemgr_alloc_dynamic_mem(resourcemgr);
	if (ret) {
		err("Couldn't alloc for FIMC-IS\n");
		ret = -ENOMEM;
		goto p_err;
	}

	is_resourcemgr_dynamic_mem(minfo->pb_taaisp, true, "TAAISP");
	is_resourcemgr_dynamic_mem(minfo->pb_tnr, true, "TNR_DMA");
	is_resourcemgr_dynamic_mem(minfo->pb_orbmch, true, "ORBMCH_DMA");

	info("[RSC] %s done\n", __func__);

p_err:
	return ret;
}
EXPORT_SYMBOL(is_resourcemgr_init_dynamic_mem);

int is_resourcemgr_deinit_dynamic_mem(struct is_resourcemgr *resourcemgr)
{
	struct is_minfo *minfo = resourcemgr->minfo;

	is_resourcemgr_dynamic_mem(minfo->pb_orbmch, false, "ORBMCH_DMA");
	is_resourcemgr_dynamic_mem(minfo->pb_tnr, false, "TNR_DMA");
	is_resourcemgr_dynamic_mem(minfo->pb_taaisp, false, "TAAISP");

	return 0;
}
EXPORT_SYMBOL(is_resourcemgr_deinit_dynamic_mem);
#endif /* #ifdef ENABLE_DYNAMIC_MEM */

int is_logsync(u32 sync_id)
{
	/* print kernel sync log */
	log_sync(sync_id);

	return 0;
}
EXPORT_SYMBOL(is_logsync);

u32 is_get_shot_timeout(struct is_resourcemgr *resourcemgr)
{
	return resourcemgr->shot_timeout;
}
EXPORT_SYMBOL(is_get_shot_timeout);

static int is_resourcemgr_alloc_secure_mem(struct is_resourcemgr *resourcemgr)
{
	struct is_mem *mem = &resourcemgr->mem;
	struct is_minfo *minfo = resourcemgr->minfo;
	int ret;

	if (IS_ENABLED(SECURE_CAMERA_TAAISP)) {
		if (TAAISP_DMA_SIZE > 0) {
			/* 3aa/isp internal DMA buffer */
			minfo->pb_taaisp_s =
				CALL_PTR_MEMOP(mem, alloc, mem->priv, TAAISP_DMA_SIZE,
					       "camera_heap", ION_EXYNOS_FLAG_PROTECTED);
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
			minfo->pb_tnr_s =
				CALL_PTR_MEMOP(mem, alloc, mem->priv, TNR_S_DMA_SIZE,
					       "secure_camera_heap", ION_EXYNOS_FLAG_PROTECTED);
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

int is_resourcemgr_init_secure_mem(struct is_resourcemgr *resourcemgr)
{
	int ret = 0;

	if (resourcemgr->scenario != IS_SCENARIO_SECURE)
		return ret;

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
EXPORT_SYMBOL(is_resourcemgr_init_secure_mem);

int is_resourcemgr_deinit_secure_mem(struct is_resourcemgr *resourcemgr)
{
	struct is_minfo *minfo = resourcemgr->minfo;
	int ret = 0;

	if (minfo->pb_taaisp_s)
		CALL_VOID_BUFOP(minfo->pb_taaisp_s, free, minfo->pb_taaisp_s);
	if (minfo->pb_tnr_s)
		CALL_VOID_BUFOP(minfo->pb_tnr_s, free, minfo->pb_tnr_s);

	minfo->pb_taaisp_s = NULL;
	minfo->pb_tnr_s = NULL;

	info("[RSC] %s done\n", __func__);

	return ret;
}
EXPORT_SYMBOL(is_resourcemgr_deinit_secure_mem);

void is_resource_set_global_param(struct is_resourcemgr *resourcemgr, void *device)
{
	struct is_global_param *global_param = &resourcemgr->global_param;

	mutex_lock(&global_param->lock);
	atomic_inc(&resourcemgr->global_param.sensor_cnt);

	if (!global_param->state) {
		global_param->llc_state = 0;
		if (resourcemgr->dev_resource.llc_mode == false)
			set_bit(LLC_DISABLE, &global_param->llc_state);

		rscmgr_func.configure_llc(true, device, &global_param->llc_state);
	}

	set_bit(resourcemgr->dev_resource.instance, &global_param->state);

	mutex_unlock(&global_param->lock);
}
EXPORT_SYMBOL(is_resource_set_global_param);

void is_resource_clear_global_param(struct is_resourcemgr *resourcemgr, void *device)
{
	struct is_global_param *global_param = &resourcemgr->global_param;

	mutex_lock(&global_param->lock);

	clear_bit(resourcemgr->dev_resource.instance, &global_param->state);

	if (!global_param->state)
		rscmgr_func.configure_llc(false, device, &global_param->llc_state);

	atomic_dec(&resourcemgr->global_param.sensor_cnt);

	mutex_unlock(&global_param->lock);
}
EXPORT_SYMBOL(is_resource_clear_global_param);

void is_bts_scen(struct is_resourcemgr *resourcemgr, unsigned int index, bool enable)
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

	scen_idx = rscmgr_sys_ops.bts_scenidx(name);
	if (scen_idx) {
		if (enable)
			ret = rscmgr_sys_ops.bts_add_scen(scen_idx);
		else
			ret = rscmgr_sys_ops.bts_del_scen(scen_idx);

		if (ret) {
			err("call bts_%s_scenario is fail (%d:%s)\n", enable ? "add" : "del",
			    scen_idx, name);
		} else {
			info("call bts_%s_scenario (%d:%s)\n", enable ? "add" : "del", scen_idx,
			     name);
		}
	}
#endif
}
EXPORT_SYMBOL(is_bts_scen);

static int pablo_cluster_min_qos_add(s32 freq, enum pablo_lib_cluster_id id)
{
	struct pablo_lib_platform_data *plpd = pablo_lib_get_platform_data();
	struct cpufreq_policy *policy = cpufreq_cpu_get(plpd->cpu_cluster[id]);

	if (policy != NULL)
		return freq_qos_tracer_add_request(&policy->constraints,
			&exynos_isp_freq_qos[id * 2], FREQ_QOS_MIN, freq * 1000);

	return -ENODEV;
}

static int pablo_cluster_min_qos_del(enum pablo_lib_cluster_id id)
{
	return freq_qos_tracer_remove_request(&exynos_isp_freq_qos[id * 2]);
}

static int pablo_cluster_min_qos_update(u32 freq, enum pablo_lib_cluster_id id)
{
	return freq_qos_update_request(&exynos_isp_freq_qos[id * 2], freq * 1000);
}

static int pablo_cluster_max_qos_add(u32 freq, enum pablo_lib_cluster_id id)
{
	struct pablo_lib_platform_data *plpd = pablo_lib_get_platform_data();
	struct cpufreq_policy *policy = cpufreq_cpu_get(plpd->cpu_cluster[id]);

	if (policy != NULL)
		return freq_qos_tracer_add_request(&policy->constraints,
			&exynos_isp_freq_qos[id * 2 + 1], FREQ_QOS_MAX, freq * 1000);

	return -ENODEV;
}

static int pablo_cluster_max_qos_del(enum pablo_lib_cluster_id id)
{
	return freq_qos_tracer_remove_request(&exynos_isp_freq_qos[id * 2 + 1]);
}

static int pablo_cluster_max_qos_update(u32 freq, enum pablo_lib_cluster_id id)
{
	return freq_qos_update_request(&exynos_isp_freq_qos[id * 2 + 1], freq * 1000);
}

static const char *cluster_name[PABLO_CPU_CLUSTER_MAX] = {
	"Cluster LIT",
	"Cluster MID",
	"Cluster MID_HF",
	"Cluster BIG",
};

static void is_set_cluster_qos(struct is_resourcemgr *resourcemgr, struct v4l2_control *ctrl,
			       enum pablo_lib_cluster_id id)
{
	u32 current_min, current_max;
	u32 request_min, request_max;

#if !defined(PM_QOS_CLUSTER2_FREQ_MAX_DEFAULT_VALUE)
	if (id == PABLO_CPU_CLUSTER_BIG)
		return;
#endif
	current_min = (resourcemgr->cluster[id] & CLUSTER_MIN_MASK) >> CLUSTER_MIN_SHIFT;
	current_max = (resourcemgr->cluster[id] & CLUSTER_MAX_MASK) >> CLUSTER_MAX_SHIFT;
	request_min = (ctrl->value & CLUSTER_MIN_MASK) >> CLUSTER_MIN_SHIFT;
	request_max = (ctrl->value & CLUSTER_MAX_MASK) >> CLUSTER_MAX_SHIFT;

	if (current_min) {
		if (request_min)
			pablo_cluster_min_qos_update(request_min, id);
		else
			pablo_cluster_min_qos_del(id);
	} else {
		if (request_min)
			pablo_cluster_min_qos_add(request_min, id);
	}

	if (current_max) {
		if (request_max)
			pablo_cluster_max_qos_update(request_max, id);
		else
			pablo_cluster_max_qos_del(id);
	} else {
		if (request_max)
			pablo_cluster_max_qos_add(request_max, id);
	}

	info("[RSC] %s minfreq : %dMHz  maxfreq : %dMHz\n",
		cluster_name[id], request_min, request_max);

	resourcemgr->cluster[id] = (request_max << CLUSTER_MAX_SHIFT) | request_min;
}

void pablo_resource_clear_cluster_qos(struct is_resourcemgr *resourcemgr)
{
	u32 current_min, current_max;
	int i;

	for (i = 0; i < PABLO_CPU_CLUSTER_MAX; i++) {
		current_min = (resourcemgr->cluster[i] & CLUSTER_MIN_MASK) >> CLUSTER_MIN_SHIFT;
		current_max = (resourcemgr->cluster[i] & CLUSTER_MAX_MASK) >> CLUSTER_MAX_SHIFT;
		if (current_min) {
			pablo_cluster_min_qos_del(i);
			warn("[RSC] cluster%d minfreq is not removed(%dMhz)\n", i, current_min);
		}

		if (current_max) {
			pablo_cluster_max_qos_del(i);
			warn("[RSC] cluster%d maxfreq is not removed(%dMhz)\n", i, current_max);
		}
		resourcemgr->cluster[i] = 0;
	}
}
EXPORT_SYMBOL(pablo_resource_clear_cluster_qos);

int is_resource_ioctl(struct is_resourcemgr *resourcemgr, struct v4l2_control *ctrl)
{
	struct pablo_lib_platform_data *plpd = pablo_lib_get_platform_data();
	int ret = 0;
	u32 cluster_id = 0;

	FIMC_BUG(!resourcemgr);
	FIMC_BUG(!ctrl);

	mutex_lock(&resourcemgr->qos_lock);
	switch (ctrl->id) {
	case V4L2_CID_IS_DVFS_CLUSTER0:
		cluster_id = PABLO_CPU_CLUSTER_LIT;
		break;
	case V4L2_CID_IS_DVFS_CLUSTER1:
		cluster_id = PABLO_CPU_CLUSTER_MID;
		break;
	case V4L2_CID_IS_DVFS_CLUSTER1_HF:
		cluster_id = PABLO_CPU_CLUSTER_MID_HF;
		break;
	case V4L2_CID_IS_DVFS_CLUSTER2:
		cluster_id = PABLO_CPU_CLUSTER_BIG;
		break;
	default:
		goto p_err;
	}

	if (plpd->cpu_cluster[cluster_id] == PABLO_CPU_MAX) {
		ret = -ENODEV;
		goto p_err;
	}
	is_set_cluster_qos(resourcemgr, ctrl, cluster_id);

p_err:
	mutex_unlock(&resourcemgr->qos_lock);

	return ret;
}
EXPORT_SYMBOL(is_resource_ioctl);

void is_resource_update_lic_sram(struct is_resourcemgr *resourcemgr, bool on)
{
	struct is_lic_sram *lic_sram;
	u32 taa_id;
	int taa_sram_sum;

	/* TODO: DT flag for LIC use */
	lic_sram = &resourcemgr->lic_sram;
	taa_id = resourcemgr->dev_resource.taa_id;

	if (on) {
		lic_sram->taa_sram[taa_id] = resourcemgr->dev_resource.width;
		atomic_add(resourcemgr->dev_resource.width, &lic_sram->taa_sram_sum);
	} else {
		lic_sram->taa_sram[taa_id] = 0;
		taa_sram_sum = atomic_sub_return(resourcemgr->dev_resource.width, &lic_sram->taa_sram_sum);
		if (taa_sram_sum < 0) {
			warn("[RSC] Invalid taa_sram_sum %d\n", taa_sram_sum);
			atomic_set(&lic_sram->taa_sram_sum, 0);
		}
	}
}
EXPORT_SYMBOL(is_resource_update_lic_sram);
