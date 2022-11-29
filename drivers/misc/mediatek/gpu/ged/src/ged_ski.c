/*
 * Copyright (C) 2020 MediaTek Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See http://www.gnu.org/licenses/gpl-2.0.html for more details.
 */

#include <mt-plat/mtk_gpu_utility.h>
#include <mtk_gpufreq.h>
#include "ged_ski.h"

static struct kobject *gpu_kobj;

define_one_global_ro(gpu_available_governor);
define_one_global_ro(gpu_busy);
define_one_global_ro(gpu_clock);
define_one_global_ro(gpu_freq_table);
define_one_global_ro(gpu_governor);
define_one_global_ro(gpu_max_clock);
define_one_global_ro(gpu_min_clock);
define_one_global_ro(gpu_model);
define_one_global_ro(gpu_tmu);

ssize_t show_gpu_available_governor(struct kobject *kobj,
		struct kobj_attribute *attr, char *buf)
{
	return scnprintf(buf, PAGE_SIZE, "Default\n");
}

ssize_t show_gpu_busy(struct kobject *kobj,
		struct kobj_attribute *attr, char *buf)
{
	unsigned int gpu_loading = 0;

	mtk_get_gpu_loading(&gpu_loading);

	return scnprintf(buf, PAGE_SIZE, "%u %%\n", gpu_loading);
}

ssize_t show_gpu_clock(struct kobject *kobj,
		struct kobj_attribute *attr, char *buf)
{
	unsigned int gpu_freq = 0;

	gpu_freq = mt_gpufreq_get_cur_volt() ? mt_gpufreq_get_cur_freq() : 0;

	return scnprintf(buf, PAGE_SIZE, "%u\n", gpu_freq);
}

ssize_t show_gpu_freq_table(struct kobject *kobj,
		struct kobj_attribute *attr, char *buf)
{
	struct mt_gpufreq_power_table_info *power_table = NULL;
	unsigned int table_num = 0;
	unsigned int max_opp_idx = 0;
	char temp[1024] = {0};
	int idx;
	int count = 0;
	int pos = 0;
	int length;

	power_table = pass_gpu_table_to_eara();
	table_num = mt_gpufreq_get_dvfs_table_num();
#ifdef CONFIG_MACH_MT6739
	max_opp_idx = 0;
#else
	max_opp_idx = mt_gpufreq_get_seg_max_opp_index();
#endif

	for (idx = max_opp_idx; count < table_num; count++) {
		length = scnprintf(temp + pos, 1024 - pos,
				"%u ", power_table[idx + count].gpufreq_khz);
		pos += length;
	}

	return scnprintf(buf, PAGE_SIZE, "%s\n", temp);
}

ssize_t show_gpu_governor(struct kobject *kobj,
		struct kobj_attribute *attr, char *buf)
{
	return scnprintf(buf, PAGE_SIZE, "Default\n");
}

ssize_t show_gpu_max_clock(struct kobject *kobj,
		struct kobj_attribute *attr, char *buf)
{
	unsigned int max_clock = 0;
	unsigned long max_clock_custom = 0;

	max_clock = mt_gpufreq_get_thermal_limit_freq();
	mtk_get_gpu_custom_upbound_freq(&max_clock_custom);
	max_clock = (max_clock_custom < max_clock) ?
			max_clock_custom : max_clock;

	return scnprintf(buf, PAGE_SIZE, "%u\n", max_clock);
}

ssize_t show_gpu_min_clock(struct kobject *kobj,
		struct kobj_attribute *attr, char *buf)
{
	unsigned long min_clock = 0;
	unsigned long min_clock_custom = 0;

	mtk_get_gpu_bottom_freq(&min_clock);
	mtk_get_gpu_custom_boost_freq(&min_clock_custom);
	min_clock = (min_clock_custom > min_clock) ?
			min_clock_custom : min_clock;

	return scnprintf(buf, PAGE_SIZE, "%lu\n", min_clock);
}

ssize_t show_gpu_model(struct kobject *kobj,
		struct kobj_attribute *attr, char *buf)
{
#ifdef CONFIG_MACH_MT6739
	return scnprintf(buf, PAGE_SIZE, "IMG PowerVR GE8100\n");
#else
	return scnprintf(buf, PAGE_SIZE, "Mali-G52 MC2\n");
#endif
}

ssize_t show_gpu_tmu(struct kobject *kobj,
		struct kobj_attribute *attr, char *buf)
{
	int temperature;

	temperature = mt_gpufreq_get_immed_gpu_temp();

	return scnprintf(buf, PAGE_SIZE, "%d\n", temperature);
}

GED_ERROR ged_ski_init(void)
{
	int ret = GED_OK;

	gpu_kobj = kobject_create_and_add("gpu", kernel_kobj);
	if (!gpu_kobj) {
		ret = GED_ERROR_OOM;
		GED_LOGE("ged: failed to create gpu_kobj!\n");
		goto EXIT;
	}

	ret = sysfs_create_file(gpu_kobj, &gpu_available_governor.attr);
	if (ret) {
		ret = GED_ERROR_OOM;
		GED_LOGE("ged: failed to create gpu_available_governor!\n");
		goto EXIT;
	}
	ret = sysfs_create_file(gpu_kobj, &gpu_busy.attr);
	if (ret) {
		ret = GED_ERROR_OOM;
		GED_LOGE("ged: failed to create gpu_busy!\n");
		goto EXIT;
	}
	ret = sysfs_create_file(gpu_kobj, &gpu_clock.attr);
	if (ret) {
		ret = GED_ERROR_OOM;
		GED_LOGE("ged: failed to create gpu_clock!\n");
		goto EXIT;
	}
	ret = sysfs_create_file(gpu_kobj, &gpu_freq_table.attr);
	if (ret) {
		ret = GED_ERROR_OOM;
		GED_LOGE("ged: failed to create gpu_freq_table!\n");
		goto EXIT;
	}
	ret = sysfs_create_file(gpu_kobj, &gpu_governor.attr);
	if (ret) {
		ret = GED_ERROR_OOM;
		GED_LOGE("ged: failed to create gpu_governor!\n");
		goto EXIT;
	}
	ret = sysfs_create_file(gpu_kobj, &gpu_max_clock.attr);
	if (ret) {
		ret = GED_ERROR_OOM;
		GED_LOGE("ged: failed to create gpu_max_clock!\n");
		goto EXIT;
	}
	ret = sysfs_create_file(gpu_kobj, &gpu_min_clock.attr);
	if (ret) {
		ret = GED_ERROR_OOM;
		GED_LOGE("ged: failed to create gpu_min_clock!\n");
		goto EXIT;
	}
	ret = sysfs_create_file(gpu_kobj, &gpu_model.attr);
	if (ret) {
		ret = GED_ERROR_OOM;
		GED_LOGE("ged: failed to create gpu_model!\n");
		goto EXIT;
	}
	ret = sysfs_create_file(gpu_kobj, &gpu_tmu.attr);
	if (ret) {
		ret = GED_ERROR_OOM;
		GED_LOGE("ged: failed to create gpu_tmu!\n");
		goto EXIT;
	}

	return ret;

EXIT:
	ged_ski_exit();
	return ret;
}

void ged_ski_exit(void)
{
	sysfs_remove_file(gpu_kobj, &gpu_available_governor.attr);
	sysfs_remove_file(gpu_kobj, &gpu_busy.attr);
	sysfs_remove_file(gpu_kobj, &gpu_clock.attr);
	sysfs_remove_file(gpu_kobj, &gpu_freq_table.attr);
	sysfs_remove_file(gpu_kobj, &gpu_governor.attr);
	sysfs_remove_file(gpu_kobj, &gpu_max_clock.attr);
	sysfs_remove_file(gpu_kobj, &gpu_min_clock.attr);
	sysfs_remove_file(gpu_kobj, &gpu_model.attr);
	sysfs_remove_file(gpu_kobj, &gpu_tmu.attr);
	kobject_put(gpu_kobj);
	gpu_kobj = NULL;
}
