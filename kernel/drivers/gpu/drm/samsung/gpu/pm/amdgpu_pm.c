/*
 * Copyright 2017 Advanced Micro Devices, Inc.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
 * THE COPYRIGHT HOLDER(S) OR AUTHOR(S) BE LIABLE FOR ANY CLAIM, DAMAGES OR
 * OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
 * ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
 * OTHER DEALINGS IN THE SOFTWARE.
 *
 * Authors: Rafał Miłecki <zajec5@gmail.com>
 *          Alex Deucher <alexdeucher@gmail.com>
 */

#include <drm/drm_debugfs.h>

#include "amdgpu.h"
#include "amdgpu_drv.h"
#include "amdgpu_pm.h"
#include "amdgpu_dpm.h"
#include "atom.h"
#include <linux/pci.h>
#include <linux/hwmon.h>
#include <linux/hwmon-sysfs.h>
#include <linux/nospec.h>
#include <linux/pm_runtime.h>

static const struct cg_flag_name clocks[] = {
	{AMD_CG_SUPPORT_GFX_FGCG, "Graphics Fine Grain Clock Gating"},
	{AMD_CG_SUPPORT_GFX_MGCG, "Graphics Medium Grain Clock Gating"},
	{AMD_CG_SUPPORT_GFX_MGLS, "Graphics Medium Grain memory Light Sleep"},
	{AMD_CG_SUPPORT_GFX_CGCG, "Graphics Coarse Grain Clock Gating"},
	{AMD_CG_SUPPORT_GFX_CGLS, "Graphics Coarse Grain memory Light Sleep"},
	{AMD_CG_SUPPORT_GFX_CGTS, "Graphics Coarse Grain Tree Shader Clock Gating"},
	{AMD_CG_SUPPORT_GFX_CGTS_LS, "Graphics Coarse Grain Tree Shader Light Sleep"},
	{AMD_CG_SUPPORT_GFX_CP_LS, "Graphics Command Processor Light Sleep"},
	{AMD_CG_SUPPORT_GFX_RLC_LS, "Graphics Run List Controller Light Sleep"},
	{AMD_CG_SUPPORT_GFX_3D_CGCG, "Graphics 3D Coarse Grain Clock Gating"},
	{AMD_CG_SUPPORT_GFX_3D_CGLS, "Graphics 3D Coarse Grain memory Light Sleep"},
	{AMD_CG_SUPPORT_MC_LS, "Memory Controller Light Sleep"},
	{AMD_CG_SUPPORT_MC_MGCG, "Memory Controller Medium Grain Clock Gating"},
	{AMD_CG_SUPPORT_SDMA_LS, "System Direct Memory Access Light Sleep"},
	{AMD_CG_SUPPORT_SDMA_MGCG, "System Direct Memory Access Medium Grain Clock Gating"},
	{AMD_CG_SUPPORT_BIF_MGCG, "Bus Interface Medium Grain Clock Gating"},
	{AMD_CG_SUPPORT_BIF_LS, "Bus Interface Light Sleep"},
	{AMD_CG_SUPPORT_UVD_MGCG, "Unified Video Decoder Medium Grain Clock Gating"},
	{AMD_CG_SUPPORT_VCE_MGCG, "Video Compression Engine Medium Grain Clock Gating"},
	{AMD_CG_SUPPORT_HDP_LS, "Host Data Path Light Sleep"},
	{AMD_CG_SUPPORT_HDP_MGCG, "Host Data Path Medium Grain Clock Gating"},
	{AMD_CG_SUPPORT_DRM_MGCG, "Digital Right Management Medium Grain Clock Gating"},
	{AMD_CG_SUPPORT_DRM_LS, "Digital Right Management Light Sleep"},
	{AMD_CG_SUPPORT_ROM_MGCG, "Rom Medium Grain Clock Gating"},
	{AMD_CG_SUPPORT_DF_MGCG, "Data Fabric Medium Grain Clock Gating"},

	{AMD_CG_SUPPORT_ATHUB_MGCG, "Address Translation Hub Medium Grain Clock Gating"},
	{AMD_CG_SUPPORT_ATHUB_LS, "Address Translation Hub Light Sleep"},
	{0, NULL},
};

static const struct hwmon_temp_label {
	enum PP_HWMON_TEMP channel;
	const char *label;
} temp_label[] = {
	{PP_TEMP_EDGE, "edge"},
	{PP_TEMP_JUNCTION, "junction"},
	{PP_TEMP_MEM, "mem"},
};

/**
 * DOC: power_dpm_state
 *
 * The power_dpm_state file is a legacy interface and is only provided for
 * backwards compatibility. The amdgpu driver provides a sysfs API for adjusting
 * certain power related parameters.  The file power_dpm_state is used for this.
 * It accepts the following arguments:
 *
 * - battery
 *
 * - balanced
 *
 * - performance
 *
 * battery
 *
 * On older GPUs, the vbios provided a special power state for battery
 * operation.  Selecting battery switched to this state.  This is no
 * longer provided on newer GPUs so the option does nothing in that case.
 *
 * balanced
 *
 * On older GPUs, the vbios provided a special power state for balanced
 * operation.  Selecting balanced switched to this state.  This is no
 * longer provided on newer GPUs so the option does nothing in that case.
 *
 * performance
 *
 * On older GPUs, the vbios provided a special power state for performance
 * operation.  Selecting performance switched to this state.  This is no
 * longer provided on newer GPUs so the option does nothing in that case.
 *
 */

static ssize_t amdgpu_get_power_dpm_state(struct device *dev,
					  struct device_attribute *attr,
					  char *buf)
{
	struct drm_device *ddev = dev_get_drvdata(dev);
	struct amdgpu_device *adev = drm_to_adev(ddev);
	enum amd_pm_state_type pm;
	int ret;

	if (amdgpu_in_reset(adev))
		return -EPERM;

	ret = pm_runtime_get_sync(ddev->dev);
	if (ret < 0) {
		pm_runtime_put_autosuspend(ddev->dev);
		return ret;
	}

	pm = adev->pm.dpm.user_state;

	pm_runtime_mark_last_busy(ddev->dev);
	pm_runtime_put_autosuspend(ddev->dev);

	return snprintf(buf, PAGE_SIZE, "%s\n",
			(pm == POWER_STATE_TYPE_BATTERY) ? "battery" :
			(pm == POWER_STATE_TYPE_BALANCED) ? "balanced" : "performance");
}

static ssize_t amdgpu_set_power_dpm_state(struct device *dev,
					  struct device_attribute *attr,
					  const char *buf,
					  size_t count)
{
	struct drm_device *ddev = dev_get_drvdata(dev);
	struct amdgpu_device *adev = drm_to_adev(ddev);
	enum amd_pm_state_type  state;
	int ret;

	if (amdgpu_in_reset(adev))
		return -EPERM;

	if (strncmp("battery", buf, strlen("battery")) == 0)
		state = POWER_STATE_TYPE_BATTERY;
	else if (strncmp("balanced", buf, strlen("balanced")) == 0)
		state = POWER_STATE_TYPE_BALANCED;
	else if (strncmp("performance", buf, strlen("performance")) == 0)
		state = POWER_STATE_TYPE_PERFORMANCE;
	else
		return -EINVAL;

	ret = pm_runtime_get_sync(ddev->dev);
	if (ret < 0) {
		pm_runtime_put_autosuspend(ddev->dev);
		return ret;
	}

	mutex_lock(&adev->pm.mutex);
	adev->pm.dpm.user_state = state;
	mutex_unlock(&adev->pm.mutex);

	amdgpu_pm_compute_clocks(adev);

	pm_runtime_mark_last_busy(ddev->dev);
	pm_runtime_put_autosuspend(ddev->dev);

	return count;
}


/**
 * DOC: power_dpm_force_performance_level
 *
 * The amdgpu driver provides a sysfs API for adjusting certain power
 * related parameters.  The file power_dpm_force_performance_level is
 * used for this.  It accepts the following arguments:
 *
 * - auto
 *
 * - low
 *
 * - high
 *
 * - manual
 *
 * - profile_standard
 *
 * - profile_min_sclk
 *
 * - profile_min_mclk
 *
 * - profile_peak
 *
 * auto
 *
 * When auto is selected, the driver will attempt to dynamically select
 * the optimal power profile for current conditions in the driver.
 *
 * low
 *
 * When low is selected, the clocks are forced to the lowest power state.
 *
 * high
 *
 * When high is selected, the clocks are forced to the highest power state.
 *
 * manual
 *
 * When manual is selected, the user can manually adjust which power states
 * are enabled for each clock domain via the sysfs pp_dpm_mclk, pp_dpm_sclk,
 * and pp_dpm_pcie files and adjust the power state transition heuristics
 * via the pp_power_profile_mode sysfs file.
 *
 * profile_standard
 * profile_min_sclk
 * profile_min_mclk
 * profile_peak
 *
 * When the profiling modes are selected, clock and power gating are
 * disabled and the clocks are set for different profiling cases. This
 * mode is recommended for profiling specific work loads where you do
 * not want clock or power gating for clock fluctuation to interfere
 * with your results. profile_standard sets the clocks to a fixed clock
 * level which varies from asic to asic.  profile_min_sclk forces the sclk
 * to the lowest level.  profile_min_mclk forces the mclk to the lowest level.
 * profile_peak sets all clocks (mclk, sclk, pcie) to the highest levels.
 *
 */

static ssize_t amdgpu_get_power_dpm_force_performance_level(struct device *dev,
							    struct device_attribute *attr,
							    char *buf)
{
	struct drm_device *ddev = dev_get_drvdata(dev);
	struct amdgpu_device *adev = drm_to_adev(ddev);
	enum amd_dpm_forced_level level = 0xff;
	int ret;

	if (amdgpu_in_reset(adev))
		return -EPERM;

	ret = pm_runtime_get_sync(ddev->dev);
	if (ret < 0) {
		pm_runtime_put_autosuspend(ddev->dev);
		return ret;
	}

	level = adev->pm.dpm.forced_level;

	pm_runtime_mark_last_busy(ddev->dev);
	pm_runtime_put_autosuspend(ddev->dev);

	return snprintf(buf, PAGE_SIZE, "%s\n",
			(level == AMD_DPM_FORCED_LEVEL_AUTO) ? "auto" :
			(level == AMD_DPM_FORCED_LEVEL_LOW) ? "low" :
			(level == AMD_DPM_FORCED_LEVEL_HIGH) ? "high" :
			(level == AMD_DPM_FORCED_LEVEL_MANUAL) ? "manual" :
			(level == AMD_DPM_FORCED_LEVEL_PROFILE_STANDARD) ? "profile_standard" :
			(level == AMD_DPM_FORCED_LEVEL_PROFILE_MIN_SCLK) ? "profile_min_sclk" :
			(level == AMD_DPM_FORCED_LEVEL_PROFILE_MIN_MCLK) ? "profile_min_mclk" :
			(level == AMD_DPM_FORCED_LEVEL_PROFILE_PEAK) ? "profile_peak" :
			"unknown");
}

static ssize_t amdgpu_set_power_dpm_force_performance_level(struct device *dev,
							    struct device_attribute *attr,
							    const char *buf,
							    size_t count)
{
	struct drm_device *ddev = dev_get_drvdata(dev);
	struct amdgpu_device *adev = drm_to_adev(ddev);
	enum amd_dpm_forced_level level;
	enum amd_dpm_forced_level current_level = 0xff;
	int ret = 0;

	if (amdgpu_in_reset(adev))
		return -EPERM;

	if (strncmp("low", buf, strlen("low")) == 0) {
		level = AMD_DPM_FORCED_LEVEL_LOW;
	} else if (strncmp("high", buf, strlen("high")) == 0) {
		level = AMD_DPM_FORCED_LEVEL_HIGH;
	} else if (strncmp("auto", buf, strlen("auto")) == 0) {
		level = AMD_DPM_FORCED_LEVEL_AUTO;
	} else if (strncmp("manual", buf, strlen("manual")) == 0) {
		level = AMD_DPM_FORCED_LEVEL_MANUAL;
	} else if (strncmp("profile_exit", buf, strlen("profile_exit")) == 0) {
		level = AMD_DPM_FORCED_LEVEL_PROFILE_EXIT;
	} else if (strncmp("profile_standard", buf, strlen("profile_standard")) == 0) {
		level = AMD_DPM_FORCED_LEVEL_PROFILE_STANDARD;
	} else if (strncmp("profile_min_sclk", buf, strlen("profile_min_sclk")) == 0) {
		level = AMD_DPM_FORCED_LEVEL_PROFILE_MIN_SCLK;
	} else if (strncmp("profile_min_mclk", buf, strlen("profile_min_mclk")) == 0) {
		level = AMD_DPM_FORCED_LEVEL_PROFILE_MIN_MCLK;
	} else if (strncmp("profile_peak", buf, strlen("profile_peak")) == 0) {
		level = AMD_DPM_FORCED_LEVEL_PROFILE_PEAK;
	}  else {
		return -EINVAL;
	}

	ret = pm_runtime_get_sync(ddev->dev);
	if (ret < 0) {
		pm_runtime_put_autosuspend(ddev->dev);
		return ret;
	}

	if (current_level == level) {
		pm_runtime_mark_last_busy(ddev->dev);
		pm_runtime_put_autosuspend(ddev->dev);
		return count;
	}

	/* profile_exit setting is valid only when current mode is in profile mode */
	if (!(current_level & (AMD_DPM_FORCED_LEVEL_PROFILE_STANDARD |
	    AMD_DPM_FORCED_LEVEL_PROFILE_MIN_SCLK |
	    AMD_DPM_FORCED_LEVEL_PROFILE_MIN_MCLK |
	    AMD_DPM_FORCED_LEVEL_PROFILE_PEAK)) &&
	    (level == AMD_DPM_FORCED_LEVEL_PROFILE_EXIT)) {
		pr_err("Currently not in any profile mode!\n");
		pm_runtime_mark_last_busy(ddev->dev);
		pm_runtime_put_autosuspend(ddev->dev);
		return -EINVAL;
	}

	pm_runtime_mark_last_busy(ddev->dev);
	pm_runtime_put_autosuspend(ddev->dev);

	return count;
}

/**
 * DOC: pcie_bw
 *
 * The amdgpu driver provides a sysfs API for estimating how much data
 * has been received and sent by the GPU in the last second through PCIe.
 * The file pcie_bw is used for this.
 * The Perf counters count the number of received and sent messages and return
 * those values, as well as the maximum payload size of a PCIe packet (mps).
 * Note that it is not possible to easily and quickly obtain the size of each
 * packet transmitted, so we output the max payload size (mps) to allow for
 * quick estimation of the PCIe bandwidth usage
 */
static ssize_t amdgpu_get_pcie_bw(struct device *dev,
		struct device_attribute *attr,
		char *buf)
{
	struct drm_device *ddev = dev_get_drvdata(dev);
	struct amdgpu_device *adev = drm_to_adev(ddev);
	uint64_t count0 = 0, count1 = 0;
	int ret;

	if (amdgpu_in_reset(adev))
		return -EPERM;

	if (adev->flags & AMD_IS_APU)
		return -ENODATA;

	if (!adev->asic_funcs->get_pcie_usage)
		return -ENODATA;

	ret = pm_runtime_get_sync(ddev->dev);
	if (ret < 0) {
		pm_runtime_put_autosuspend(ddev->dev);
		return ret;
	}

	amdgpu_asic_get_pcie_usage(adev, &count0, &count1);

	pm_runtime_mark_last_busy(ddev->dev);
	pm_runtime_put_autosuspend(ddev->dev);

	return snprintf(buf, PAGE_SIZE,	"%llu %llu %i\n",
			count0, count1, pcie_get_mps(adev->pdev));
}

/**
 * DOC: unique_id
 *
 * The amdgpu driver provides a sysfs API for providing a unique ID for the GPU
 * The file unique_id is used for this.
 * This will provide a Unique ID that will persist from machine to machine
 *
 * NOTE: This will only work for GFX9 and newer. This file will be absent
 * on unsupported ASICs (GFX8 and older)
 */
static ssize_t amdgpu_get_unique_id(struct device *dev,
		struct device_attribute *attr,
		char *buf)
{
	struct drm_device *ddev = dev_get_drvdata(dev);
	struct amdgpu_device *adev = drm_to_adev(ddev);

	if (amdgpu_in_reset(adev))
		return -EPERM;

	if (adev->unique_id)
		return snprintf(buf, PAGE_SIZE, "%016llx\n", adev->unique_id);

	return 0;
}

/**
 * DOC: thermal_throttling_logging
 *
 * Thermal throttling pulls down the clock frequency and thus the performance.
 * It's an useful mechanism to protect the chip from overheating. Since it
 * impacts performance, the user controls whether it is enabled and if so,
 * the log frequency.
 *
 * Reading back the file shows you the status(enabled or disabled) and
 * the interval(in seconds) between each thermal logging.
 *
 * Writing an integer to the file, sets a new logging interval, in seconds.
 * The value should be between 1 and 3600. If the value is less than 1,
 * thermal logging is disabled. Values greater than 3600 are ignored.
 */
static ssize_t amdgpu_get_thermal_throttling_logging(struct device *dev,
						     struct device_attribute *attr,
						     char *buf)
{
	struct drm_device *ddev = dev_get_drvdata(dev);
	struct amdgpu_device *adev = drm_to_adev(ddev);

	return snprintf(buf, PAGE_SIZE, "%s: thermal throttling logging %s, with interval %d seconds\n",
			adev_to_drm(adev)->unique,
			atomic_read(&adev->throttling_logging_enabled) ? "enabled" : "disabled",
			adev->throttling_logging_rs.interval / HZ + 1);
}

static ssize_t amdgpu_set_thermal_throttling_logging(struct device *dev,
						     struct device_attribute *attr,
						     const char *buf,
						     size_t count)
{
	struct drm_device *ddev = dev_get_drvdata(dev);
	struct amdgpu_device *adev = drm_to_adev(ddev);
	long throttling_logging_interval;
	unsigned long flags;
	int ret = 0;

	ret = kstrtol(buf, 0, &throttling_logging_interval);
	if (ret)
		return ret;

	if (throttling_logging_interval > 3600)
		return -EINVAL;

	if (throttling_logging_interval > 0) {
		raw_spin_lock_irqsave(&adev->throttling_logging_rs.lock, flags);
		/*
		 * Reset the ratelimit timer internals.
		 * This can effectively restart the timer.
		 */
		adev->throttling_logging_rs.interval =
			(throttling_logging_interval - 1) * HZ;
		adev->throttling_logging_rs.begin = 0;
		adev->throttling_logging_rs.printed = 0;
		adev->throttling_logging_rs.missed = 0;
		raw_spin_unlock_irqrestore(&adev->throttling_logging_rs.lock, flags);

		atomic_set(&adev->throttling_logging_enabled, 1);
	} else {
		atomic_set(&adev->throttling_logging_enabled, 0);
	}

	return count;
}

static struct amdgpu_device_attr amdgpu_device_attrs[] = {
	AMDGPU_DEVICE_ATTR_RW(power_dpm_state,				ATTR_FLAG_BASIC|ATTR_FLAG_ONEVF),
	AMDGPU_DEVICE_ATTR_RW(power_dpm_force_performance_level,	ATTR_FLAG_BASIC),
	AMDGPU_DEVICE_ATTR_RO(pcie_bw,					ATTR_FLAG_BASIC),
	AMDGPU_DEVICE_ATTR_RO(unique_id,				ATTR_FLAG_BASIC),
	AMDGPU_DEVICE_ATTR_RW(thermal_throttling_logging,		ATTR_FLAG_BASIC),
};

static int default_attr_update(struct amdgpu_device *adev, struct amdgpu_device_attr *attr,
			       uint32_t mask, enum amdgpu_device_attr_states *states)
{
	struct device_attribute *dev_attr = &attr->dev_attr;
	const char *attr_name = dev_attr->attr.name;
	enum amd_asic_type asic_type = adev->asic_type;

	if (!(attr->flags & mask)) {
		*states = ATTR_STATE_UNSUPPORTED;
		return 0;
	}

#define DEVICE_ATTR_IS(_name)	(!strcmp(attr_name, #_name))

	if (DEVICE_ATTR_IS(pp_dpm_socclk)) {
		if (asic_type < CHIP_VEGA10)
			*states = ATTR_STATE_UNSUPPORTED;
	} else if (DEVICE_ATTR_IS(pp_dpm_dcefclk)) {
		if (asic_type < CHIP_VEGA10 || asic_type == CHIP_ARCTURUS)
			*states = ATTR_STATE_UNSUPPORTED;
	} else if (DEVICE_ATTR_IS(pp_dpm_fclk)) {
		if (asic_type < CHIP_VEGA20)
			*states = ATTR_STATE_UNSUPPORTED;
	} else if (DEVICE_ATTR_IS(pp_dpm_pcie)) {
		if (asic_type == CHIP_ARCTURUS)
			*states = ATTR_STATE_UNSUPPORTED;
	} else if (DEVICE_ATTR_IS(pp_od_clk_voltage)) {
		*states = ATTR_STATE_UNSUPPORTED;
	} else if (DEVICE_ATTR_IS(mem_busy_percent)) {
		if (adev->flags & AMD_IS_APU || asic_type == CHIP_VEGA10)
			*states = ATTR_STATE_UNSUPPORTED;
	} else if (DEVICE_ATTR_IS(pcie_bw)) {
		/* PCIe Perf counters won't work on APU nodes */
		if (adev->flags & AMD_IS_APU)
			*states = ATTR_STATE_UNSUPPORTED;
	} else if (DEVICE_ATTR_IS(unique_id)) {
		if (asic_type != CHIP_VEGA10 &&
		    asic_type != CHIP_VEGA20 &&
		    asic_type != CHIP_ARCTURUS)
			*states = ATTR_STATE_UNSUPPORTED;
	} else if (DEVICE_ATTR_IS(pp_features)) {
		if (adev->flags & AMD_IS_APU || asic_type < CHIP_VEGA10)
			*states = ATTR_STATE_UNSUPPORTED;
	} else if (DEVICE_ATTR_IS(gpu_metrics)) {
		if (asic_type < CHIP_VEGA12)
			*states = ATTR_STATE_UNSUPPORTED;
	}

#undef DEVICE_ATTR_IS

	return 0;
}


static int amdgpu_device_attr_create(struct amdgpu_device *adev,
				     struct amdgpu_device_attr *attr,
				     uint32_t mask, struct list_head *attr_list)
{
	int ret = 0;
	struct device_attribute *dev_attr = &attr->dev_attr;
	const char *name = dev_attr->attr.name;
	enum amdgpu_device_attr_states attr_states = ATTR_STATE_SUPPORTED;
	struct amdgpu_device_attr_entry *attr_entry;

	int (*attr_update)(struct amdgpu_device *adev, struct amdgpu_device_attr *attr,
			   uint32_t mask, enum amdgpu_device_attr_states *states) = default_attr_update;

	BUG_ON(!attr);

	attr_update = attr->attr_update ? attr_update : default_attr_update;

	ret = attr_update(adev, attr, mask, &attr_states);
	if (ret) {
		dev_err(adev->dev, "failed to update device file %s, ret = %d\n",
			name, ret);
		return ret;
	}

	if (attr_states == ATTR_STATE_UNSUPPORTED)
		return 0;

	ret = device_create_file(adev->dev, dev_attr);
	if (ret) {
		dev_err(adev->dev, "failed to create device file %s, ret = %d\n",
			name, ret);
	}

	attr_entry = kmalloc(sizeof(*attr_entry), GFP_KERNEL);
	if (!attr_entry)
		return -ENOMEM;

	attr_entry->attr = attr;
	INIT_LIST_HEAD(&attr_entry->entry);

	list_add_tail(&attr_entry->entry, attr_list);

	return ret;
}

static void amdgpu_device_attr_remove(struct amdgpu_device *adev, struct amdgpu_device_attr *attr)
{
	struct device_attribute *dev_attr = &attr->dev_attr;

	device_remove_file(adev->dev, dev_attr);
}

static void amdgpu_device_attr_remove_groups(struct amdgpu_device *adev,
					     struct list_head *attr_list);

static int amdgpu_device_attr_create_groups(struct amdgpu_device *adev,
					    struct amdgpu_device_attr *attrs,
					    uint32_t counts,
					    uint32_t mask,
					    struct list_head *attr_list)
{
	int ret = 0;
	uint32_t i = 0;

	for (i = 0; i < counts; i++) {
		ret = amdgpu_device_attr_create(adev, &attrs[i], mask, attr_list);
		if (ret)
			goto failed;
	}

	return 0;

failed:
	amdgpu_device_attr_remove_groups(adev, attr_list);

	return ret;
}

static void amdgpu_device_attr_remove_groups(struct amdgpu_device *adev,
					     struct list_head *attr_list)
{
	struct amdgpu_device_attr_entry *entry, *entry_tmp;

	if (list_empty(attr_list))
		return ;

	list_for_each_entry_safe(entry, entry_tmp, attr_list, entry) {
		amdgpu_device_attr_remove(adev, entry->attr);
		list_del(&entry->entry);
		kfree(entry);
	}
}

static ssize_t amdgpu_hwmon_show_temp(struct device *dev,
				      struct device_attribute *attr,
				      char *buf)
{
	struct amdgpu_device *adev = dev_get_drvdata(dev);
	int channel = to_sensor_dev_attr(attr)->index;
	int r, temp = 0, size = sizeof(temp);

	if (amdgpu_in_reset(adev))
		return -EPERM;

	if (channel >= PP_TEMP_MAX)
		return -EINVAL;

	r = pm_runtime_get_sync(adev_to_drm(adev)->dev);
	if (r < 0) {
		pm_runtime_put_autosuspend(adev_to_drm(adev)->dev);
		return r;
	}

	switch (channel) {
	case PP_TEMP_JUNCTION:
		/* get current junction temperature */
		r = amdgpu_dpm_read_sensor(adev, AMDGPU_PP_SENSOR_HOTSPOT_TEMP,
					   (void *)&temp, &size);
		break;
	case PP_TEMP_EDGE:
		/* get current edge temperature */
		r = amdgpu_dpm_read_sensor(adev, AMDGPU_PP_SENSOR_EDGE_TEMP,
					   (void *)&temp, &size);
		break;
	case PP_TEMP_MEM:
		/* get current memory temperature */
		r = amdgpu_dpm_read_sensor(adev, AMDGPU_PP_SENSOR_MEM_TEMP,
					   (void *)&temp, &size);
		break;
	default:
		r = -EINVAL;
		break;
	}

	pm_runtime_mark_last_busy(adev_to_drm(adev)->dev);
	pm_runtime_put_autosuspend(adev_to_drm(adev)->dev);

	if (r)
		return r;

	return snprintf(buf, PAGE_SIZE, "%d\n", temp);
}

static ssize_t amdgpu_hwmon_show_temp_thresh(struct device *dev,
					     struct device_attribute *attr,
					     char *buf)
{
	struct amdgpu_device *adev = dev_get_drvdata(dev);
	int hyst = to_sensor_dev_attr(attr)->index;
	int temp;

	if (hyst)
		temp = adev->pm.dpm.thermal.min_temp;
	else
		temp = adev->pm.dpm.thermal.max_temp;

	return snprintf(buf, PAGE_SIZE, "%d\n", temp);
}

static ssize_t amdgpu_hwmon_show_hotspot_temp_thresh(struct device *dev,
					     struct device_attribute *attr,
					     char *buf)
{
	struct amdgpu_device *adev = dev_get_drvdata(dev);
	int hyst = to_sensor_dev_attr(attr)->index;
	int temp;

	if (hyst)
		temp = adev->pm.dpm.thermal.min_hotspot_temp;
	else
		temp = adev->pm.dpm.thermal.max_hotspot_crit_temp;

	return snprintf(buf, PAGE_SIZE, "%d\n", temp);
}

static ssize_t amdgpu_hwmon_show_mem_temp_thresh(struct device *dev,
					     struct device_attribute *attr,
					     char *buf)
{
	struct amdgpu_device *adev = dev_get_drvdata(dev);
	int hyst = to_sensor_dev_attr(attr)->index;
	int temp;

	if (hyst)
		temp = adev->pm.dpm.thermal.min_mem_temp;
	else
		temp = adev->pm.dpm.thermal.max_mem_crit_temp;

	return snprintf(buf, PAGE_SIZE, "%d\n", temp);
}

static ssize_t amdgpu_hwmon_show_temp_label(struct device *dev,
					     struct device_attribute *attr,
					     char *buf)
{
	int channel = to_sensor_dev_attr(attr)->index;

	if (channel >= PP_TEMP_MAX)
		return -EINVAL;

	return snprintf(buf, PAGE_SIZE, "%s\n", temp_label[channel].label);
}

static ssize_t amdgpu_hwmon_show_temp_emergency(struct device *dev,
					     struct device_attribute *attr,
					     char *buf)
{
	struct amdgpu_device *adev = dev_get_drvdata(dev);
	int channel = to_sensor_dev_attr(attr)->index;
	int temp = 0;

	if (channel >= PP_TEMP_MAX)
		return -EINVAL;

	switch (channel) {
	case PP_TEMP_JUNCTION:
		temp = adev->pm.dpm.thermal.max_hotspot_emergency_temp;
		break;
	case PP_TEMP_EDGE:
		temp = adev->pm.dpm.thermal.max_edge_emergency_temp;
		break;
	case PP_TEMP_MEM:
		temp = adev->pm.dpm.thermal.max_mem_emergency_temp;
		break;
	}

	return snprintf(buf, PAGE_SIZE, "%d\n", temp);
}

static ssize_t amdgpu_hwmon_show_vddgfx(struct device *dev,
					struct device_attribute *attr,
					char *buf)
{
	struct amdgpu_device *adev = dev_get_drvdata(dev);
	u32 vddgfx;
	int r, size = sizeof(vddgfx);

	if (amdgpu_in_reset(adev))
		return -EPERM;

	r = pm_runtime_get_sync(adev_to_drm(adev)->dev);
	if (r < 0) {
		pm_runtime_put_autosuspend(adev_to_drm(adev)->dev);
		return r;
	}

	/* get the voltage */
	r = amdgpu_dpm_read_sensor(adev, AMDGPU_PP_SENSOR_VDDGFX,
				   (void *)&vddgfx, &size);

	pm_runtime_mark_last_busy(adev_to_drm(adev)->dev);
	pm_runtime_put_autosuspend(adev_to_drm(adev)->dev);

	if (r)
		return r;

	return snprintf(buf, PAGE_SIZE, "%d\n", vddgfx);
}

static ssize_t amdgpu_hwmon_show_vddgfx_label(struct device *dev,
					      struct device_attribute *attr,
					      char *buf)
{
	return snprintf(buf, PAGE_SIZE, "vddgfx\n");
}

static ssize_t amdgpu_hwmon_show_vddnb(struct device *dev,
				       struct device_attribute *attr,
				       char *buf)
{
	struct amdgpu_device *adev = dev_get_drvdata(dev);
	u32 vddnb;
	int r, size = sizeof(vddnb);

	if (amdgpu_in_reset(adev))
		return -EPERM;

	/* only APUs have vddnb */
	if  (!(adev->flags & AMD_IS_APU))
		return -EINVAL;

	r = pm_runtime_get_sync(adev_to_drm(adev)->dev);
	if (r < 0) {
		pm_runtime_put_autosuspend(adev_to_drm(adev)->dev);
		return r;
	}

	/* get the voltage */
	r = amdgpu_dpm_read_sensor(adev, AMDGPU_PP_SENSOR_VDDNB,
				   (void *)&vddnb, &size);

	pm_runtime_mark_last_busy(adev_to_drm(adev)->dev);
	pm_runtime_put_autosuspend(adev_to_drm(adev)->dev);

	if (r)
		return r;

	return snprintf(buf, PAGE_SIZE, "%d\n", vddnb);
}

static ssize_t amdgpu_hwmon_show_vddnb_label(struct device *dev,
					      struct device_attribute *attr,
					      char *buf)
{
	return snprintf(buf, PAGE_SIZE, "vddnb\n");
}

static ssize_t amdgpu_hwmon_show_sclk(struct device *dev,
				      struct device_attribute *attr,
				      char *buf)
{
	struct amdgpu_device *adev = dev_get_drvdata(dev);
	uint32_t sclk;
	int r, size = sizeof(sclk);

	if (amdgpu_in_reset(adev))
		return -EPERM;

	r = pm_runtime_get_sync(adev_to_drm(adev)->dev);
	if (r < 0) {
		pm_runtime_put_autosuspend(adev_to_drm(adev)->dev);
		return r;
	}

	/* get the sclk */
	r = amdgpu_dpm_read_sensor(adev, AMDGPU_PP_SENSOR_GFX_SCLK,
				   (void *)&sclk, &size);

	pm_runtime_mark_last_busy(adev_to_drm(adev)->dev);
	pm_runtime_put_autosuspend(adev_to_drm(adev)->dev);

	if (r)
		return r;

	return snprintf(buf, PAGE_SIZE, "%u\n", sclk * 10 * 1000);
}

static ssize_t amdgpu_hwmon_show_sclk_label(struct device *dev,
					    struct device_attribute *attr,
					    char *buf)
{
	return snprintf(buf, PAGE_SIZE, "sclk\n");
}

static ssize_t amdgpu_hwmon_show_mclk(struct device *dev,
				      struct device_attribute *attr,
				      char *buf)
{
	struct amdgpu_device *adev = dev_get_drvdata(dev);
	uint32_t mclk;
	int r, size = sizeof(mclk);

	if (amdgpu_in_reset(adev))
		return -EPERM;

	r = pm_runtime_get_sync(adev_to_drm(adev)->dev);
	if (r < 0) {
		pm_runtime_put_autosuspend(adev_to_drm(adev)->dev);
		return r;
	}

	/* get the sclk */
	r = amdgpu_dpm_read_sensor(adev, AMDGPU_PP_SENSOR_GFX_MCLK,
				   (void *)&mclk, &size);

	pm_runtime_mark_last_busy(adev_to_drm(adev)->dev);
	pm_runtime_put_autosuspend(adev_to_drm(adev)->dev);

	if (r)
		return r;

	return snprintf(buf, PAGE_SIZE, "%u\n", mclk * 10 * 1000);
}

static ssize_t amdgpu_hwmon_show_mclk_label(struct device *dev,
					    struct device_attribute *attr,
					    char *buf)
{
	return snprintf(buf, PAGE_SIZE, "mclk\n");
}

/**
 * DOC: hwmon
 *
 * The amdgpu driver exposes the following sensor interfaces:
 *
 * - GPU temperature (via the on-die sensor)
 *
 * - GPU voltage
 *
 * - Northbridge voltage (APUs only)
 *
 * - GPU power
 *
 * - GPU fan
 *
 * - GPU gfx/compute engine clock
 *
 * - GPU memory clock (dGPU only)
 *
 * hwmon interfaces for GPU temperature:
 *
 * - temp[1-3]_input: the on die GPU temperature in millidegrees Celsius
 *   - temp2_input and temp3_input are supported on SOC15 dGPUs only
 *
 * - temp[1-3]_label: temperature channel label
 *   - temp2_label and temp3_label are supported on SOC15 dGPUs only
 *
 * - temp[1-3]_crit: temperature critical max value in millidegrees Celsius
 *   - temp2_crit and temp3_crit are supported on SOC15 dGPUs only
 *
 * - temp[1-3]_crit_hyst: temperature hysteresis for critical limit in millidegrees Celsius
 *   - temp2_crit_hyst and temp3_crit_hyst are supported on SOC15 dGPUs only
 *
 * - temp[1-3]_emergency: temperature emergency max value(asic shutdown) in millidegrees Celsius
 *   - these are supported on SOC15 dGPUs only
 *
 * hwmon interfaces for GPU voltage:
 *
 * - in0_input: the voltage on the GPU in millivolts
 *
 * - in1_input: the voltage on the Northbridge in millivolts
 *
 * hwmon interfaces for GPU fan:
 *
 * - pwm1: pulse width modulation fan level (0-255)
 *
 * - pwm1_min: pulse width modulation fan control minimum level (0)
 *
 * - pwm1_max: pulse width modulation fan control maximum level (255)
 *
 * hwmon interfaces for GPU clocks:
 *
 * - freq1_input: the gfx/compute clock in hertz
 *
 * - freq2_input: the memory clock in hertz
 *
 * You can use hwmon tools like sensors to view this information on your system.
 *
 */

static SENSOR_DEVICE_ATTR(temp1_input, S_IRUGO, amdgpu_hwmon_show_temp, NULL, PP_TEMP_EDGE);
static SENSOR_DEVICE_ATTR(temp1_crit, S_IRUGO, amdgpu_hwmon_show_temp_thresh, NULL, 0);
static SENSOR_DEVICE_ATTR(temp1_crit_hyst, S_IRUGO, amdgpu_hwmon_show_temp_thresh, NULL, 1);
static SENSOR_DEVICE_ATTR(temp1_emergency, S_IRUGO, amdgpu_hwmon_show_temp_emergency, NULL, PP_TEMP_EDGE);
static SENSOR_DEVICE_ATTR(temp2_input, S_IRUGO, amdgpu_hwmon_show_temp, NULL, PP_TEMP_JUNCTION);
static SENSOR_DEVICE_ATTR(temp2_crit, S_IRUGO, amdgpu_hwmon_show_hotspot_temp_thresh, NULL, 0);
static SENSOR_DEVICE_ATTR(temp2_crit_hyst, S_IRUGO, amdgpu_hwmon_show_hotspot_temp_thresh, NULL, 1);
static SENSOR_DEVICE_ATTR(temp2_emergency, S_IRUGO, amdgpu_hwmon_show_temp_emergency, NULL, PP_TEMP_JUNCTION);
static SENSOR_DEVICE_ATTR(temp3_input, S_IRUGO, amdgpu_hwmon_show_temp, NULL, PP_TEMP_MEM);
static SENSOR_DEVICE_ATTR(temp3_crit, S_IRUGO, amdgpu_hwmon_show_mem_temp_thresh, NULL, 0);
static SENSOR_DEVICE_ATTR(temp3_crit_hyst, S_IRUGO, amdgpu_hwmon_show_mem_temp_thresh, NULL, 1);
static SENSOR_DEVICE_ATTR(temp3_emergency, S_IRUGO, amdgpu_hwmon_show_temp_emergency, NULL, PP_TEMP_MEM);
static SENSOR_DEVICE_ATTR(temp1_label, S_IRUGO, amdgpu_hwmon_show_temp_label, NULL, PP_TEMP_EDGE);
static SENSOR_DEVICE_ATTR(temp2_label, S_IRUGO, amdgpu_hwmon_show_temp_label, NULL, PP_TEMP_JUNCTION);
static SENSOR_DEVICE_ATTR(temp3_label, S_IRUGO, amdgpu_hwmon_show_temp_label, NULL, PP_TEMP_MEM);
static SENSOR_DEVICE_ATTR(in0_input, S_IRUGO, amdgpu_hwmon_show_vddgfx, NULL, 0);
static SENSOR_DEVICE_ATTR(in0_label, S_IRUGO, amdgpu_hwmon_show_vddgfx_label, NULL, 0);
static SENSOR_DEVICE_ATTR(in1_input, S_IRUGO, amdgpu_hwmon_show_vddnb, NULL, 0);
static SENSOR_DEVICE_ATTR(in1_label, S_IRUGO, amdgpu_hwmon_show_vddnb_label, NULL, 0);
static SENSOR_DEVICE_ATTR(freq1_input, S_IRUGO, amdgpu_hwmon_show_sclk, NULL, 0);
static SENSOR_DEVICE_ATTR(freq1_label, S_IRUGO, amdgpu_hwmon_show_sclk_label, NULL, 0);
static SENSOR_DEVICE_ATTR(freq2_input, S_IRUGO, amdgpu_hwmon_show_mclk, NULL, 0);
static SENSOR_DEVICE_ATTR(freq2_label, S_IRUGO, amdgpu_hwmon_show_mclk_label, NULL, 0);

static struct attribute *hwmon_attributes[] = {
	&sensor_dev_attr_temp1_input.dev_attr.attr,
	&sensor_dev_attr_temp1_crit.dev_attr.attr,
	&sensor_dev_attr_temp1_crit_hyst.dev_attr.attr,
	&sensor_dev_attr_temp2_input.dev_attr.attr,
	&sensor_dev_attr_temp2_crit.dev_attr.attr,
	&sensor_dev_attr_temp2_crit_hyst.dev_attr.attr,
	&sensor_dev_attr_temp3_input.dev_attr.attr,
	&sensor_dev_attr_temp3_crit.dev_attr.attr,
	&sensor_dev_attr_temp3_crit_hyst.dev_attr.attr,
	&sensor_dev_attr_temp1_emergency.dev_attr.attr,
	&sensor_dev_attr_temp2_emergency.dev_attr.attr,
	&sensor_dev_attr_temp3_emergency.dev_attr.attr,
	&sensor_dev_attr_temp1_label.dev_attr.attr,
	&sensor_dev_attr_temp2_label.dev_attr.attr,
	&sensor_dev_attr_temp3_label.dev_attr.attr,
	&sensor_dev_attr_in0_input.dev_attr.attr,
	&sensor_dev_attr_in0_label.dev_attr.attr,
	&sensor_dev_attr_in1_input.dev_attr.attr,
	&sensor_dev_attr_in1_label.dev_attr.attr,
	&sensor_dev_attr_freq1_input.dev_attr.attr,
	&sensor_dev_attr_freq1_label.dev_attr.attr,
	&sensor_dev_attr_freq2_input.dev_attr.attr,
	&sensor_dev_attr_freq2_label.dev_attr.attr,
	NULL
};

static umode_t hwmon_attributes_visible(struct kobject *kobj,
					struct attribute *attr, int index)
{
	struct device *dev = kobj_to_dev(kobj);
	struct amdgpu_device *adev = dev_get_drvdata(dev);
	umode_t effective_mode = attr->mode;

	/* under multi-vf mode, the hwmon attributes are all not supported */
	if (amdgpu_sriov_vf(adev) && !amdgpu_sriov_is_pp_one_vf(adev))
		return 0;

	/* Skip crit temp on APU */
	if ((adev->flags & AMD_IS_APU) && (adev->family >= AMDGPU_FAMILY_CZ) &&
	    (attr == &sensor_dev_attr_temp1_crit.dev_attr.attr ||
	     attr == &sensor_dev_attr_temp1_crit_hyst.dev_attr.attr))
		return 0;

	/* Skip limit attributes if DPM is not enabled */
	if (!adev->pm.dpm_enabled &&
	    (attr == &sensor_dev_attr_temp1_crit.dev_attr.attr ||
	     attr == &sensor_dev_attr_temp1_crit_hyst.dev_attr.attr))
		return 0;

	if (adev->family == AMDGPU_FAMILY_SI)
	      /* not implemented yet */
		return 0;

	if ((adev->family == AMDGPU_FAMILY_SI ||	/* not implemented yet */
	     adev->family == AMDGPU_FAMILY_KV) &&	/* not implemented yet */
	    (attr == &sensor_dev_attr_in0_input.dev_attr.attr ||
	     attr == &sensor_dev_attr_in0_label.dev_attr.attr))
		return 0;

	/* only APUs have vddnb */
	if (!(adev->flags & AMD_IS_APU) &&
	    (attr == &sensor_dev_attr_in1_input.dev_attr.attr ||
	     attr == &sensor_dev_attr_in1_label.dev_attr.attr))
		return 0;

	/* no mclk on APUs */
	if ((adev->flags & AMD_IS_APU) &&
	    (attr == &sensor_dev_attr_freq2_input.dev_attr.attr ||
	     attr == &sensor_dev_attr_freq2_label.dev_attr.attr))
		return 0;

	/* only SOC15 dGPUs support hotspot and mem temperatures */
	if (((adev->flags & AMD_IS_APU) ||
	     adev->asic_type < CHIP_VEGA10) &&
	    (attr == &sensor_dev_attr_temp2_crit.dev_attr.attr ||
	     attr == &sensor_dev_attr_temp2_crit_hyst.dev_attr.attr ||
	     attr == &sensor_dev_attr_temp3_crit.dev_attr.attr ||
	     attr == &sensor_dev_attr_temp3_crit_hyst.dev_attr.attr ||
	     attr == &sensor_dev_attr_temp1_emergency.dev_attr.attr ||
	     attr == &sensor_dev_attr_temp2_emergency.dev_attr.attr ||
	     attr == &sensor_dev_attr_temp3_emergency.dev_attr.attr ||
	     attr == &sensor_dev_attr_temp2_input.dev_attr.attr ||
	     attr == &sensor_dev_attr_temp3_input.dev_attr.attr ||
	     attr == &sensor_dev_attr_temp2_label.dev_attr.attr ||
	     attr == &sensor_dev_attr_temp3_label.dev_attr.attr))
		return 0;

	return effective_mode;
}

static const struct attribute_group hwmon_attrgroup = {
	.attrs = hwmon_attributes,
	.is_visible = hwmon_attributes_visible,
};

static const struct attribute_group *hwmon_groups[] = {
	&hwmon_attrgroup,
	NULL
};

static ssize_t amdgpu_show_power_state(struct device *dev,
						struct device_attribute *attr,
						char *buf)
{
	uint32_t power_status = 0;
	struct drm_device *ddev = dev_get_drvdata(dev);
	struct amdgpu_device *adev = ddev->dev_private;

#ifdef CONFIG_DRM_SGPU_EXYNOS
	if (sgpu_exynos_pm)
		power_status = readl(adev->pm.pmu_mmio);
	else
#endif
		power_status = adev->in_suspend;

	return snprintf(buf, PAGE_SIZE, "%d\n", power_status);
}

static ssize_t amdgpu_set_power_state(struct device *dev,
						struct device_attribute *attr,
						const char *buf,
						size_t count)
{
#ifdef CONFIG_DRM_SGPU_EXYNOS
	struct drm_device *ddev = dev_get_drvdata(dev);
	struct amdgpu_device *adev = ddev->dev_private;
	int err;
	u32 value;

	err = kstrtou32(buf, 10, &value);
	if (err)
		return err;

	if (sgpu_exynos_pm)
		writel(value, adev->pm.pmu_mmio);
#endif

	return count;
}
static DEVICE_ATTR(amdgpu_power_state, S_IRUGO | S_IWUSR,
		amdgpu_show_power_state, amdgpu_set_power_state);


int amdgpu_pm_sysfs_init(struct amdgpu_device *adev)
{
	int ret;
	uint32_t mask = 0;

	if (adev->pm.sysfs_initialized)
		return 0;

	if (adev->pm.dpm_enabled == 0)
		return 0;

	INIT_LIST_HEAD(&adev->pm.pm_attr_list);

	adev->pm.int_hwmon_dev = hwmon_device_register_with_groups(adev->dev,
								   DRIVER_NAME, adev,
								   hwmon_groups);
	if (IS_ERR(adev->pm.int_hwmon_dev)) {
		ret = PTR_ERR(adev->pm.int_hwmon_dev);
		dev_err(adev->dev,
			"Unable to register hwmon device: %d\n", ret);
		return ret;
	}

	switch (amdgpu_virt_get_sriov_vf_mode(adev)) {
	case SRIOV_VF_MODE_ONE_VF:
		mask = ATTR_FLAG_ONEVF;
		break;
	case SRIOV_VF_MODE_MULTI_VF:
		mask = 0;
		break;
	case SRIOV_VF_MODE_BARE_METAL:
	default:
		mask = ATTR_FLAG_MASK_ALL;
		break;
	}

	ret = device_create_file(adev->dev, &dev_attr_amdgpu_power_state);
	if (ret) {
		DRM_ERROR("failed to create device file for power_status\n");
		return ret;
	}

	ret = amdgpu_device_attr_create_groups(adev,
					       amdgpu_device_attrs,
					       ARRAY_SIZE(amdgpu_device_attrs),
					       mask,
					       &adev->pm.pm_attr_list);
	if (ret)
		return ret;

	adev->pm.sysfs_initialized = true;

	return 0;
}

void amdgpu_pm_sysfs_fini(struct amdgpu_device *adev)
{
	if (adev->pm.dpm_enabled == 0)
		return;

	if (adev->pm.int_hwmon_dev)
		hwmon_device_unregister(adev->pm.int_hwmon_dev);

	amdgpu_device_attr_remove_groups(adev, &adev->pm.pm_attr_list);
}

/*
 * Debugfs info
 */
#if defined(CONFIG_DEBUG_FS)

static int amdgpu_debugfs_pm_info(struct seq_file *m, void *data)
{
	struct drm_info_node *node = (struct drm_info_node *) m->private;
	struct drm_device *dev = node->minor->dev;
	struct amdgpu_device *adev = drm_to_adev(dev);
	int r;

	if (amdgpu_in_reset(adev))
		return -EPERM;

	r = pm_runtime_get_sync(dev->dev);
	if (r < 0) {
		pm_runtime_put_autosuspend(dev->dev);
		return r;
	}

	if (!adev->pm.dpm_enabled) {
		seq_printf(m, "dpm not enabled\n");
		pm_runtime_mark_last_busy(dev->dev);
		pm_runtime_put_autosuspend(dev->dev);
		return 0;
	}

	pm_runtime_mark_last_busy(dev->dev);
	pm_runtime_put_autosuspend(dev->dev);

	return -EINVAL;
}

static const struct drm_info_list amdgpu_pm_info_list[] = {
	{"amdgpu_pm_info", amdgpu_debugfs_pm_info, 0, NULL},
};
#endif

int amdgpu_debugfs_pm_init(struct amdgpu_device *adev)
{
#if defined(CONFIG_DEBUG_FS)
	return amdgpu_debugfs_add_files(adev, amdgpu_pm_info_list, ARRAY_SIZE(amdgpu_pm_info_list));
#else
	return 0;
#endif
}
