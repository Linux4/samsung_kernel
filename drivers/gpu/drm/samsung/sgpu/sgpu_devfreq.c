/*
 * @file sgpu_devfreq.c
 *
 * Copyright (c) 2019 Samsung Electronics Co., Ltd.
 *              http://www.samsung.com
 *
 * @brief Contains the implementaton of main devfreq handler.
 *  Devfreq handler manages devfreq feature.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published
 * by the Free Software Foundation, either version 2 of the License,
 * or (at your option) any later version.
 */

#include "amdgpu.h"
#include "sgpu_governor.h"
#include "sgpu_user_interface.h"
#include "sgpu_utilization.h"
#include "sgpu_profiler.h"
#include "amdgpu_trace.h"

#ifdef CONFIG_DRM_SGPU_EXYNOS
#include <soc/samsung/cal-if.h>
#include <soc/samsung/tmu.h>
#include "exynos_gpu_interface.h"

#if IS_ENABLED(CONFIG_EXYNOS_ESCA_DVFS_MANAGER)
#include <soc/samsung/exynos-dm.h>
#endif
#endif /* CONFIG_DRM_SGPU_EXYNOS */

static int sgpu_devfreq_target(struct device *dev, unsigned long *target_freq, u32 flags)
{
	struct drm_device *ddev = dev_get_drvdata(dev);
	struct amdgpu_device *adev = ddev->dev_private;
	struct devfreq *df = adev->devfreq;
	struct sgpu_governor_data *data = df->data;
	unsigned long cur_freq, qos_min_freq, qos_max_freq;
	struct dev_pm_opp *target_opp;
	int i;
#if defined(CONFIG_DRM_SGPU_EXYNOS) && !defined(CONFIG_SOC_S5E9925_EVT0)
	uint32_t cnt_value;
#endif /* CONFIG_DRM_SGPU_EXYNOS && !CONFIG_SOC_S5E9925_EVT0 */
	if (df->profile->get_cur_freq)
		df->profile->get_cur_freq(df->dev.parent, &cur_freq);
	else
		cur_freq = df->previous_freq;

	qos_min_freq = dev_pm_qos_read_value(dev, DEV_PM_QOS_MIN_FREQUENCY);
	qos_max_freq = dev_pm_qos_read_value(dev, DEV_PM_QOS_MAX_FREQUENCY);
	if (qos_min_freq >= qos_max_freq)
		flags = 1;

	target_opp = devfreq_recommended_opp(dev, target_freq, flags);
	if (IS_ERR(target_opp)) {
		dev_err(dev, "target_freq: not found valid OPP table\n");
		return PTR_ERR(target_opp);
	}
	dev_pm_opp_put(target_opp);

	if (cur_freq == *target_freq)
		return 0;

	/* in suspend or power_off*/
	if (atomic_read(&df->suspend_count) > 0) {
		*target_freq = df->previous_freq;
		return 0;
	}

#ifdef CONFIG_DRM_SGPU_EXYNOS
#ifndef CONFIG_SOC_S5E9925_EVT0
	cnt_value = readl(data->bg3d_dvfs + 0x5c);
	DRM_DEBUG("[SWAT CNT1] CUR_2AD Count : 0x%x, CUR_1AD Count : 0x%x",
	       (cnt_value >> 16) & 0xffff, cnt_value & 0xffff);
	cnt_value = readl(data->bg3d_dvfs + 0x60);
	DRM_DEBUG("[SWAT CNT2] CUR_2BD Count : 0x%x, CUR_1BD Count : 0x%x",
	       (cnt_value >> 16) & 0xffff, cnt_value & 0xffff);

	/* BG3D_DVFS_CTL (EVT1) 0x058 disable */
	writel(0x0, data->bg3d_dvfs + 0x58);

	/* Polling busy bit (BG3D_DVFS_CTL[12])  */
	while(readl(data->bg3d_dvfs + 0x58) & 0x1000) ;
#endif

#if IS_ENABLED(CONFIG_DEBUG_SNAPSHOT)
	if (adev->gpu_dss_freq_id)
		dbg_snapshot_freq(adev->gpu_dss_freq_id, cur_freq, *target_freq, DSS_FLAG_IN);
#endif
	mutex_lock(&adev->ifpo_mutex);
	/* cal i/f for change clock */
#if IS_ENABLED(CONFIG_EXYNOS_ESCA_DVFS_MANAGER)
	DM_CALL(data->dm_type, target_freq);
#elif IS_ENABLED(CONFIG_CAL_IF)
	cal_dfs_set_rate(adev->cal_id, *target_freq);
#endif /*CONFIG_CAL_IF*/
	mutex_unlock(&adev->ifpo_mutex);
#if IS_ENABLED(CONFIG_DEBUG_SNAPSHOT)
	if (adev->gpu_dss_freq_id)
		dbg_snapshot_freq(adev->gpu_dss_freq_id, cur_freq, *target_freq, DSS_FLAG_OUT);
#endif

#ifndef CONFIG_SOC_S5E9925_EVT0
	/* BG3D_DVFS_CTL (EVT1) 0x058 enable */
	writel(0x1, data->bg3d_dvfs + 0x58);
#endif
#endif /* CONFIG_DRM_SGPU_EXYNOS */

	for (i = 0; i < df->profile->max_state; i++) {
		if (df->profile->freq_table[i] == *target_freq)
			data->current_level = i;
	}

	trace_sgpu_devfreq_set_target(cur_freq, *target_freq);
	SGPU_LOG(adev, DMSG_INFO, DMSG_DVFS, "set_freq:%8lu", *target_freq);

	return 0;
}

#if IS_ENABLED(CONFIG_EXYNOS_ESCA_DVFS_MANAGER)
int sgpu_dm_freq_scaler(int dm_type, void *devdata, u32 target_freq, unsigned int relation)
{
	struct sgpu_governor_data *data = devdata;
	struct devfreq *df = data->devfreq;
	int i;

	if (!df || !df->profile)
		return 0;

	for (i = 0; i < df->profile->max_state; i++) {
		if (df->profile->freq_table[i] == target_freq)
			data->current_level = i;
	}

	data->old_freq = target_freq;

	return 0;
}
#endif

static int sgpu_devfreq_status(struct device *dev, struct devfreq_dev_status *stat)
{
	struct drm_device *ddev = dev_get_drvdata(dev);
	struct amdgpu_device *adev = ddev->dev_private;

	stat->current_frequency = adev->devfreq->previous_freq;
	return sgpu_utilization_capture(stat);
}

static int sgpu_register_pm_qos(struct devfreq *df)
{
	struct sgpu_governor_data *data = df->data;

	data->devfreq = df;

	data->sys_min_freq = df->scaling_min_freq;
	data->sys_max_freq = df->scaling_max_freq;

	/* Round down to kHz for PM QoS */
	dev_pm_qos_add_request(df->dev.parent, &data->sys_pm_qos_min,
			       DEV_PM_QOS_MIN_FREQUENCY,
			       data->sys_min_freq / HZ_PER_KHZ);
	dev_pm_qos_add_request(df->dev.parent, &data->sys_pm_qos_max,
			       DEV_PM_QOS_MAX_FREQUENCY,
			       data->sys_max_freq / HZ_PER_KHZ);

	return 0;
}

static int sgpu_unregister_pm_qos(struct devfreq *df)
{
	struct sgpu_governor_data *data = df->data;

	if (!data)
		return -EINVAL;

	dev_pm_qos_remove_request(&data->sys_pm_qos_min);
	dev_pm_qos_remove_request(&data->sys_pm_qos_max);

	return 0;
}

static int sgpu_devfreq_cur_freq(struct device *dev, unsigned long *freq)
{
	struct drm_device *ddev = dev_get_drvdata(dev);
	struct amdgpu_device *adev = ddev->dev_private;
	struct devfreq *df = adev->devfreq;
#if IS_ENABLED(CONFIG_EXYNOS_ESCA_DVFS_MANAGER)
	struct sgpu_governor_data *data = df->data;
#endif

	if (atomic_read(&df->suspend_count) > 0) {
		*freq = 0;
		return 0;
	}

#ifdef CONFIG_DRM_SGPU_EXYNOS
#if IS_ENABLED(CONFIG_EXYNOS_ESCA_DVFS_MANAGER)
	*freq = data->old_freq;
#elif IS_ENABLED(CONFIG_CAL_IF)
	*freq = cal_dfs_cached_get_rate(adev->cal_id);
#endif /*CONFIG_CAL_IF*/
#else
	*freq = adev->devfreq->previous_freq;
#endif /* CONFIG_DRM_SGPU_EXYNOS */

	return 0;
}

static void sgpu_devfreq_exit(struct device *dev)
{
	struct drm_device *ddev = dev_get_drvdata(dev);
	struct amdgpu_device *adev = ddev->dev_private;

#ifdef CONFIG_DRM_SGPU_EXYNOS
	exynos_interface_deinit(adev->devfreq);
#endif /* CONFIG_DRM_SGPU_EXYNOS */
	sgpu_profiler_deinit(adev->devfreq);
	sgpu_unregister_pm_qos(adev->devfreq);
	sgpu_governor_deinit(adev->devfreq);

	kfree(adev->devfreq->profile);

	return;
}

int sgpu_devfreq_init(struct amdgpu_device *adev)
{
	struct devfreq_dev_profile *dp;
	struct sgpu_governor_data *data;
	int ret = 0;

	dp = kzalloc(sizeof(struct devfreq_dev_profile), GFP_KERNEL);
	if (!dp)
		return -ENOMEM;

	dp->target = sgpu_devfreq_target;
	dp->get_dev_status = sgpu_devfreq_status;
	dp->get_cur_freq = sgpu_devfreq_cur_freq;
	dp->exit = sgpu_devfreq_exit;
	dp->timer = DEVFREQ_TIMER_DELAYED;

	ret = sgpu_governor_init(adev->dev, dp, &data);
	if (ret)
		goto err_gov;

	adev->devfreq = devfreq_add_device(adev->dev, dp, "sgpu_governor", data);
	if (IS_ERR(adev->devfreq)) {
		dev_err(adev->dev, "Unable to register with devfreq %d\n", ret);
		ret = PTR_ERR(adev->devfreq);
		goto err_devfreq;
	}

	adev->devfreq->suspend_freq = dp->initial_freq;

	ret = sgpu_register_pm_qos(adev->devfreq);
	if (ret) {
		dev_err(adev->dev, "Unable to register pm QoS requests of devfreq %d\n", ret);
		goto err_noti;
	}
	ret = sgpu_create_sysfs_file(adev->devfreq);
	if (ret) {
		dev_err(adev->dev, "Unable to create sysfs node %d\n", ret);
		goto err_sysfs;
	}

	ret = sgpu_profiler_init(adev->devfreq);
	if (ret) {
		dev_err(adev->dev, "Unable to create profiler module %d\n", ret);
		goto err_sysfs;
	}

#ifdef CONFIG_DRM_SGPU_EXYNOS
	ret = exynos_interface_init(adev->devfreq);
	if (ret) {
		dev_err(adev->dev, "Unable to create exynos interface %d\n", ret);
		goto err_sysfs;
	}

	/* AFM_WARN trim PMIC is enabled at EVT1 time */
	if (sgpu_enable_afm && AMDGPU_IS_EVT1(adev->grbm_chip_rev)) {
		ret = sgpu_afm_init(adev);
		if (ret) {
			DRM_ERROR("Creating sgpu_afm_init failed (%d).\n", ret);
			adev->enable_afm = false;
		} else {
			DRM_INFO("sgpu afm driver init done\n");
			adev->enable_afm = true;
		}
	} else
		adev->enable_afm = false;
#endif /* CONFIG_DRM_SGPU_EXYNOS */
	return ret;

err_sysfs:
	sgpu_unregister_pm_qos(adev->devfreq);
err_noti:
	devfreq_remove_device(adev->devfreq);
err_devfreq:
	sgpu_governor_deinit(adev->devfreq);
err_gov:
	kfree(dp);
	return ret;
}
