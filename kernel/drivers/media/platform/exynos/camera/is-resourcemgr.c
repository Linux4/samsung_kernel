// SPDX-License-Identifier: GPL-2.0
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

#include "exynos-is.h"
#include "is-resourcemgr.h"
#include "is-hw.h"
#include "is-hw-dvfs.h"
#include "pablo-debug.h"
#include "is-core.h"
#include "is-interface-library.h"
#include "votf/pablo-votf.h"
#include "is-device-camif-dma.h"
#include "pablo-dvfs.h"
#include "pablo-smc.h"
#include "pablo-lib.h"
#include "is-device-csi.h"
#include "is-interface-wrap.h"
#include "pablo-crta-bufmgr.h"

extern int is_sensor_runtime_suspend(struct device *dev);
extern int is_sensor_runtime_resume(struct device *dev);
extern void is_vendor_resource_clean(struct is_core *core);

static BLOCKING_NOTIFIER_HEAD(cam_pwr_noti_chain);

#if IS_ENABLED(CONFIG_EXYNOS_THERMAL) || IS_ENABLED(CONFIG_EXYNOS_THERMAL_V2)
static int is_tmu_notifier(struct notifier_block *nb, unsigned long state, void *data)
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

	if (!atomic_read(&resourcemgr->rsccount))
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
#if IS_ENABLED(CONFIG_EXYNOS_EA_DTM)
	dbg_snapshot_thermal(cooling_device_name, resourcemgr->limited_fps, 0, 0);
#else
	dbg_snapshot_thermal(NULL, 0, cooling_device_name, resourcemgr->limited_fps);
#endif
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

static int is_itmon_notifier(struct notifier_block *nb, unsigned long state, void *data)
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

	if (due_to_is(itmon->port, resourcemgr) || due_to_is(itmon->dest, resourcemgr) ||
	    due_to_is(itmon->master, resourcemgr)) {
		info("%s: init description : %s\n", __func__, itmon->port);
		info("%s: target descrition: %s\n", __func__, itmon->dest);
		info("%s: user description : %s\n", __func__, itmon->master);
		info("%s: Transaction Type : %s\n", __func__, itmon->read ? "READ" : "WRITE");
		info("%s: target address   : %lx\n", __func__, itmon->target_addr);
		info("%s: Power Domain     : %s(%d)\n", __func__, itmon->pd_name, itmon->onoff);

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

static int pablo_resource_cdump_per_core(void)
{
	struct is_device_ischain *device;
	struct is_core *core;
	int i;

	core = is_get_is_core();

	for (i = 0; i < IS_STREAM_COUNT; ++i) {
		device = &core->ischain[i];
		if (!test_bit(IS_ISCHAIN_OPEN, &device->state))
			continue;

		if (test_bit(IS_ISCHAIN_CLOSING, &device->state))
			continue;

		/* clock & gpio dump */
		if (!in_interrupt()) {
			struct exynos_platform_is *pdata = dev_get_platdata(&core->pdev->dev);

			pdata->clk_dump(&core->pdev->dev);
		}

		cinfo("### 2. SFR DUMP ###\n");
		break;
	}

	return 0;
}

static int pablo_resource_dump_per_core(bool clk_gpio_ddk_dump)
{
	struct is_device_ischain *device;
	struct is_core *core;
	int i;

	core = is_get_is_core();

	for (i = 0; i < IS_STREAM_COUNT; ++i) {
		device = &core->ischain[i];
		if (!test_bit(IS_ISCHAIN_OPEN, &device->state))
			continue;

		if (test_bit(IS_ISCHAIN_CLOSING, &device->state))
			continue;

		if (clk_gpio_ddk_dump) {
			/* clock & gpio dump */
			schedule_work(&core->wq_data_print_clk);
			/* ddk log dump */
			is_lib_logdump();
		}
		is_hardware_sfr_dump(&core->hardware, DEV_HW_END, false);
		break;
	}

	return 0;
}

static int pablo_resource_dump_per_ischain(bool dump_all)
{
	struct is_device_ischain *device;
	struct is_device_csi *csi;
	struct is_group *group;
	struct is_subdev *subdev;
	struct is_framemgr *framemgr;
	struct is_core *core;
	int i, j;

	core = is_get_is_core();

	for (i = 0; i < IS_STREAM_COUNT; ++i) {
		device = &core->ischain[i];
		if (!test_bit(IS_ISCHAIN_OPEN, &device->state))
			continue;

		if (test_bit(IS_ISCHAIN_CLOSING, &device->state))
			continue;

		if (device->sensor && !test_bit(IS_ISCHAIN_REPROCESSING, &device->state)) {
			csi = (struct is_device_csi *)v4l2_get_subdevdata(
				device->sensor->subdev_csi);
			if (csi) {
				if (dump_all)
					csi_hw_dump_all(csi);
				else
					csi_hw_cdump_all(csi);
			}
		}

		if (!dump_all)
			cinfo("### 3. frame manager ###\n");

		/* dump all framemgr */
		group = get_leader_group(i);
		while (group) {
			if (!test_bit(IS_GROUP_OPEN, &group->state))
				break;

			for (j = 0; j < ENTRY_END; j++) {
				subdev = group->subdev[j];
				if (subdev && test_bit(IS_SUBDEV_START, &subdev->state)) {
					framemgr = GET_SUBDEV_FRAMEMGR(subdev);
					if (framemgr) {
						unsigned long flags;

						if (dump_all) {
							mserr(" dump framemgr..", subdev, subdev);
							framemgr_e_barrier_irqs(framemgr, 0, flags);
							frame_manager_dump_queues(framemgr);
						} else {
							cinfo("[%d][%s] framemgr dump\n",
							      subdev->instance, subdev->name);
							framemgr_e_barrier_irqs(framemgr, 0, flags);
							frame_manager_print_queues(framemgr);
						}
						framemgr_x_barrier_irqr(framemgr, 0, flags);
					}
				}
			}

			group = group->next;
		}
	}

	return 0;
}

static int pablo_resource_check_rsccount(void)
{
	struct is_core *core;

	core = is_get_is_core();

	return atomic_read(&core->resourcemgr.rsccount);
}

int is_resource_cdump(void)
{
	if (!pablo_resource_check_rsccount())
		goto exit;

	info("### %s dump start ###\n", __func__);
	cinfo("###########################\n");
	cinfo("###########################\n");
	cinfo("### %s dump start ###\n", __func__);

#if IS_ENABLED(CONFIG_EXYNOS_MEMORY_LOGGER)
	/* dump all CR by using memlogger */
	is_debug_memlog_dump_cr_all(MEMLOG_LEVEL_EMERG);
#endif

	/* dump per core */
	pablo_resource_cdump_per_core();

	/* dump per ischain */
	pablo_resource_dump_per_ischain(false);

	/* dump per core */
	pablo_resource_dump_per_core(false);

	cinfo("#####################\n");
	cinfo("#####################\n");
	cinfo("### %s dump end ###\n", __func__);
	info("### %s dump end (refer camera dump)###\n", __func__);
exit:
	return 0;
}
KUNIT_EXPORT_SYMBOL(is_resource_cdump);

int is_resource_dump(void)
{
	info("### %s dump start ###\n", __func__);

	/* dump per core */
	pablo_resource_dump_per_core(true);

	/* dump per ischain */
	pablo_resource_dump_per_ischain(true);

	info("### %s dump end ###\n", __func__);

	return 0;
}
KUNIT_EXPORT_SYMBOL(is_resource_dump);

static int is_panic_handler(struct notifier_block *nb, ulong l, void *buf)
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

static int is_reboot_handler(struct notifier_block *nb, ulong l, void *buf)
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

#if defined(DISABLE_CORE_IDLE_STATE)
static void is_resourcemgr_c2_disable_work(struct work_struct *data)
{
	/* CPU idle on/off */
	info("%s: call cpuidle_pause()\n", __func__);
	cpuidle_pause_and_lock();
}
#endif

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
		else if (!strcmp(i.node->name, "camera_ddk") || !strcmp(i.node->name, "camera-bin"))
			pablo_lib_init_reserved_mem(rmem);
		else
			probe_warn("callback not found for [%s], skipping", i.node->name);
	}

	return 0;
}

static void pablo_rscmgr_register_ops(void)
{
	pablo_get_rscmgr_ops()->configure_llc = is_hw_configure_llc;
}

int is_resourcemgr_probe(struct is_resourcemgr *resourcemgr, void *private_data,
			 struct platform_device *pdev)
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
	for (i = 0; i < RESOURCE_TYPE_MAX; i++)
		atomic_set(&resourcemgr->resource_device[i].rsccount, 0);
	atomic_set(&resourcemgr->resource_preproc.rsccount, 0);
	memset(resourcemgr->cluster, 0x0, sizeof(u32) * PABLO_CPU_CLUSTER_MAX);

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

	pablo_crta_bufmgr_probe();

	/* dvfs controller init */
	ret = is_dvfs_init(&resourcemgr->dvfs_ctrl, is_dvfs_dt_arr, is_hw_dvfs_get_lv);
	if (ret) {
		probe_err("%s: is_dvfs_init failed!\n", __func__);
		goto p_err;
	}

	atomic_notifier_chain_register(&panic_notifier_list, &notify_panic_block);
	register_reboot_notifier(&notify_reboot_block);
#if IS_ENABLED(CONFIG_CMU_EWF)
	get_cmuewf_index(np, &resourcemgr->idx_ewf);
#endif

#ifdef ENABLE_KERNEL_LOG_DUMP
#if IS_ENABLED(CONFIG_EXYNOS_SNAPSHOT)
	resourcemgr->kernel_log_buf = kzalloc(exynos_ss_get_item_size("log_kernel"), GFP_KERNEL);
#elif IS_ENABLED(CONFIG_DEBUG_SNAPSHOT)
	resourcemgr->kernel_log_buf = kzalloc(dbg_snapshot_get_item_size("log_kernel"), GFP_KERNEL);
#endif
#endif

	mutex_init(&resourcemgr->global_param.lock);
	resourcemgr->global_param.state = 0;

	pablo_rscmgr_register_ops();

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

	if (rsc_type < RESOURCE_TYPE_ISCHAIN) {
		result = &core->sensor[rsc_type];
		resource->pdev = core->sensor[rsc_type].pdev;
	} else {
		for (stream = 0; stream < IS_STREAM_COUNT; ++stream) {
			ischain = &core->ischain[stream];
			if (!test_bit(IS_ISCHAIN_OPEN, &ischain->state)) {
				result = ischain;
				resource->pdev = ischain->pdev;
				break;
			}
		}
	}

	if (device)
		*device = result;

p_err:
	dbgd_resource("%s\n", __func__);
	return ret;
}
EXPORT_SYMBOL_GPL(is_resource_open);

static void is_resource_reset(struct is_resourcemgr *resourcemgr)
{
	struct is_lic_sram *lic_sram = &resourcemgr->lic_sram;

	memset((void *)lic_sram, 0x0, sizeof(struct is_lic_sram));
	memset(resourcemgr->cluster, 0x0, sizeof(u32) * PABLO_CPU_CLUSTER_MAX);

	resourcemgr->global_param.state = 0;
	resourcemgr->shot_timeout = SHOT_TIMEOUT;
	resourcemgr->shot_timeout_tick = 0;

	is_dvfs_ems_init();

#if IS_ENABLED(CONFIG_CMU_EWF)
	set_cmuewf(resourcemgr->idx_ewf, 1);
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
	pablo_resource_clear_cluster_qos(resourcemgr);
	is_mem_check_stats(&resourcemgr->mem);

	is_dvfs_ems_reset(&resourcemgr->dvfs_ctrl);

#if IS_ENABLED(CONFIG_CMU_EWF)
	set_cmuewf(resourcemgr->idx_ewf, 0);
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

static void pablo_resource_get_sensor_type(struct is_resourcemgr *resourcemgr, u32 rsc_type,
					   struct is_resource *resource)
{
	int id = rsc_type - RESOURCE_TYPE_SENSOR0;

#ifdef CONFIG_PM
	pm_runtime_get_sync(&resource->pdev->dev);
#else
	is_sensor_runtime_resume(&resource->pdev->dev);
#endif
	set_bit(IS_RM_SS0_POWER_ON + id, &resourcemgr->state);
}

static void pablo_resource_put_sensor_type(struct is_resourcemgr *resourcemgr, u32 rsc_type,
					   struct is_resource *resource)
{
	int id = rsc_type - RESOURCE_TYPE_SENSOR0;

#if defined(CONFIG_PM)
	pm_runtime_put_sync(&resource->pdev->dev);
#else
	is_sensor_runtime_suspend(&resource->pdev->dev);
#endif
	clear_bit(IS_RM_SS0_POWER_ON + id, &resourcemgr->state);
}

static int pablo_resource_get_ischain_type(struct is_resourcemgr *resourcemgr)
{
	int ret;
	struct is_core *core = (struct is_core *)resourcemgr->private_data;

	if (test_bit(IS_RM_ISC_POWER_ON, &resourcemgr->state)) {
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

	ret = CALL_INTERFACE_OPS(&core->interface, open);
	if (ret) {
		err("is_interface_open is fail(%d)", ret);
		goto p_err;
	}

	if (IS_ENABLED(SECURE_CAMERA_MEM_SHARE)) {
		ret = is_resourcemgr_init_secure_mem(resourcemgr);
		if (ret) {
			err("is_resourcemgr_init_secure_mem is fail(%d)\n", ret);
			goto p_err;
		}
	}

	ret = CALL_DEVICE_ISCHAIN_OPS(&core->ischain[0], power, 1);
	if (ret) {
		err("is_ischain_power is fail(%d)", ret);
		CALL_DEVICE_ISCHAIN_OPS(&core->ischain[0], power, 0);
		goto p_err;
	}

	set_bit(IS_RM_ISC_POWER_ON, &resourcemgr->state);
	set_bit(IS_RM_POWER_ON, &resourcemgr->state);

#if defined(DISABLE_CORE_IDLE_STATE)
	schedule_work(&resourcemgr->c2_disable_work);
#endif
	is_bts_scen(resourcemgr, 0, true);
	resourcemgr->cur_bts_scen_idx = 0;

	ret = is_load_bin();
	if (ret < 0) {
		err("is_load_bin() is fail(%d)", ret);
		goto p_err;
	}

p_err:
	return ret;
}

static void pablo_resource_put_ischain_type(struct is_resourcemgr *resourcemgr)
{
	int ret;
	struct is_core *core = (struct is_core *)resourcemgr->private_data;

	is_load_clear();

	if (IS_ENABLED(SECURE_CAMERA_FACE)) {
		if (is_secure_func(core, NULL, IS_SECURE_CAMERA_FACE, resourcemgr->scenario,
				   SMC_SECCAM_UNPREPARE))
			err("Failed to is_secure_func(FACE, UNPREPARE)");
	}

	ret = is_itf_power_down_wrap(core);
	if (ret)
		err("power down cmd is fail(%d)", ret);

	ret = CALL_DEVICE_ISCHAIN_OPS(&core->ischain[0], power, 0);
	if (ret)
		err("is_ischain_power is fail(%d)", ret);

	ret = CALL_INTERFACE_OPS(&core->interface, close);
	if (ret)
		err("is_interface_close is fail(%d)", ret);

	if (IS_ENABLED(SECURE_CAMERA_MEM_SHARE)) {
		ret = is_resourcemgr_deinit_secure_mem(resourcemgr);
		if (ret)
			err("is_resourcemgr_deinit_secure_mem is fail(%d)", ret);
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
}

static int pablo_resource_get_by_type(struct is_resourcemgr *resourcemgr, u32 rsc_type,
				      struct is_resource *resource)
{
	int ret = 0;

	if (rsc_type == RESOURCE_TYPE_ISCHAIN)
		ret = pablo_resource_get_ischain_type(resourcemgr);
	else
		pablo_resource_get_sensor_type(resourcemgr, rsc_type, resource);

	return ret;
}

static void pablo_resource_put_by_type(struct is_resourcemgr *resourcemgr, u32 rsc_type,
				       struct is_resource *resource)
{
	if (rsc_type == RESOURCE_TYPE_ISCHAIN)
		pablo_resource_put_ischain_type(resourcemgr);
	else
		pablo_resource_put_sensor_type(resourcemgr, rsc_type, resource);
}

int is_resource_get(struct is_resourcemgr *resourcemgr, u32 rsc_type)
{
	int ret = 0;
	u32 rsccount;
	struct is_resource *resource;
	struct is_core *core;
	int i;

	FIMC_BUG(!resourcemgr);
	FIMC_BUG(!resourcemgr->private_data);
	FIMC_BUG(rsc_type >= RESOURCE_TYPE_MAX);

	core = (struct is_core *)resourcemgr->private_data;

	mutex_lock(&resourcemgr->rsc_lock);

	rsccount = atomic_read(&resourcemgr->rsccount);
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
		if ((rsc_type != RESOURCE_TYPE_ISCHAIN) && rsccount == 1) {
			if (atomic_read(&resource_ischain->rsccount) == 1)
				is_kernel_log_dump(resourcemgr, false);
		}
	}
#endif

	if (rsccount == 0) {
		TIME_LAUNCH_STR(LAUNCH_TOTAL);
		pm_stay_awake(&core->pdev->dev);

		/* dvfs controller init */
		ret = is_dvfs_init(&resourcemgr->dvfs_ctrl, is_dvfs_dt_arr, is_hw_dvfs_get_lv);
		if (ret) {
			err("%s: is_dvfs_init failed!\n", __func__);
			goto rsc_err;
		}

		is_resource_reset(resourcemgr);

		if (IS_ENABLED(SECURE_CAMERA_FACE)) {
			mutex_init(&core->secure_state_lock);
			core->secure_state = IS_STATE_UNSECURE;
			resourcemgr->scenario = 0;

			info("%s: is secure state has reset\n", __func__);
		}

		core->vendor.opening_hint = IS_OPENING_HINT_NONE;
		core->vendor.isp_max_width = 0;
		core->vendor.isp_max_height = 0;

		for (i = 0; i < MAX_SENSOR_SHARED_RSC; i++) {
			mutex_init(&core->shared_rsc_lock[i]);
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
		ret = pablo_resource_get_by_type(resourcemgr, rsc_type, resource);
		if (ret)
			goto p_err;

		is_vendor_resource_get(&core->vendor, rsc_type);
	}

	if (rsccount == 0) {
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
	atomic_inc(&resourcemgr->rsccount);

	info("[RSC] rsctype: %d, rsccount: device[%d], core[%d]\n", rsc_type,
	     atomic_read(&resource->rsccount), rsccount + 1);
rsc_err:
	mutex_unlock(&resourcemgr->rsc_lock);

	return ret;
}
PST_EXPORT_SYMBOL(is_resource_get);

int is_resource_put(struct is_resourcemgr *resourcemgr, u32 rsc_type)
{
	int i, ret = 0;
	u32 rsccount;
	struct is_resource *resource;
	struct is_core *core;

	FIMC_BUG(!resourcemgr);
	FIMC_BUG(!resourcemgr->private_data);
	FIMC_BUG(rsc_type >= RESOURCE_TYPE_MAX);

	core = (struct is_core *)resourcemgr->private_data;

	mutex_lock(&resourcemgr->rsc_lock);

	rsccount = atomic_read(&resourcemgr->rsccount);
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
#ifdef ENABLE_HWACG_CONTROL
		/* for CSIS HWACG */
		is_hw_csi_qchannel_enable_all(false);
#endif
	}

	/* local update */
	if (atomic_read(&resource->rsccount) == 1) {
		pablo_resource_put_by_type(resourcemgr, rsc_type, resource);
		is_vendor_resource_put(&core->vendor, rsc_type);
	}

	/* global update */
	if (atomic_read(&resourcemgr->rsccount) == 1) {
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

		core->vendor.closing_hint = IS_CLOSING_HINT_NONE;

		ret = is_runtime_suspend_post(&resource->pdev->dev);
		if (ret)
			err("is_runtime_suspend_post is fail(%d)", ret);

		pm_relax(&core->pdev->dev);

		clear_bit(IS_RM_POWER_ON, &resourcemgr->state);
	}

	atomic_dec(&resource->rsccount);
	atomic_dec(&resourcemgr->rsccount);
	info("[RSC] rsctype: %d, rsccount: device[%d], core[%d]\n", rsc_type,
	     atomic_read(&resource->rsccount), rsccount - 1);
p_err:
	mutex_unlock(&resourcemgr->rsc_lock);

	return ret;
}
PST_EXPORT_SYMBOL(is_resource_put);

int register_cam_pwr_notifier(struct notifier_block *nb)
{
	struct is_core *core = pablo_get_core_async();
	struct is_resourcemgr *resourcemgr;
	int ret;

	if (!core) {
		err("Camera core is NULL");
		return -ENODEV;
	}

	core->noti_register_state = true;
	resourcemgr = &core->resourcemgr;

	ret = blocking_notifier_chain_register(&cam_pwr_noti_chain, nb);
	if (core && atomic_read(&resourcemgr->rsccount)) {
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

#if IS_ENABLED(CONFIG_PABLO_KUNIT_TEST)
static struct pkt_rscmgr_ops rscmgr_ops = {
	.get_sensor = pablo_resource_get_sensor_type,
	.put_sensor = pablo_resource_put_sensor_type,
	.get_ischain = pablo_resource_get_ischain_type,
	.put_ischain = pablo_resource_put_ischain_type,
};

struct pkt_rscmgr_ops *pablo_kunit_rscmgr(void)
{
	return &rscmgr_ops;
}
KUNIT_EXPORT_SYMBOL(pablo_kunit_rscmgr);
#endif
