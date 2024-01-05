// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2020 MediaTek Inc.
 */
#include <mt-plat/mtk_gpu_utility.h>
#include <gpufreq_v2.h>
#include <ged_gpufreq_v2.h>
#include <ged_ski.h>
#include <ged_sysfs.h>
#include <ged_base.h>
#include "ged_log.h"

static struct kobject *g_gpu_kobj;

ssize_t gpu_available_governor_show(struct kobject *kobj,
		struct kobj_attribute *attr, char *buf)
{
	return scnprintf(buf, PAGE_SIZE, "Default\n");
}
static KOBJ_ATTR_RO(gpu_available_governor);

ssize_t gpu_busy_show(struct kobject *kobj,
		struct kobj_attribute *attr, char *buf)
{
	unsigned int gpu_loading = 0;

	mtk_get_gpu_loading(&gpu_loading);

	return scnprintf(buf, PAGE_SIZE, "%u %%\n", gpu_loading);
}
static KOBJ_ATTR_RO(gpu_busy);

ssize_t gpu_clock_show(struct kobject *kobj,
		struct kobj_attribute *attr, char *buf)
{
	unsigned int gpu_freq = 0;

	gpu_freq = gpufreq_get_cur_volt(TARGET_DEFAULT) ? gpufreq_get_cur_freq(TARGET_DEFAULT) : 0;

	return scnprintf(buf, PAGE_SIZE, "%u\n", gpu_freq);
}
static KOBJ_ATTR_RO(gpu_clock);

ssize_t gpu_freq_table_show(struct kobject *kobj,
		struct kobj_attribute *attr, char *buf)
{
	const struct gpufreq_opp_info *power_table = NULL;
	int table_num = 0;
	char temp[1024] = {0};
	int idx;
	int count = 0;
	int pos = 0;
	int length;

	power_table = gpufreq_get_working_table(TARGET_DEFAULT);
	table_num = ged_get_opp_num();

	for (idx = 0; table_num>0 && count < table_num; count++) {
		length = scnprintf(temp + pos, 1024 - pos,
				"%u ", power_table[idx + count].freq);
		pos += length;
	}
	return scnprintf(buf, PAGE_SIZE, "%s\n", temp);
}
static KOBJ_ATTR_RO(gpu_freq_table);

ssize_t gpu_governor_show(struct kobject *kobj,
		struct kobj_attribute *attr, char *buf)
{
	return scnprintf(buf, PAGE_SIZE, "Default\n");
}
static KOBJ_ATTR_RO(gpu_governor);

ssize_t gpu_max_clock_show(struct kobject *kobj,
		struct kobj_attribute *attr, char *buf)
{
	unsigned int max_clock = 0;
	unsigned long max_clock_custom = 0;
	int limit_idx = -1;

	mtk_get_gpu_ceiling_index(&limit_idx);
	max_clock =  gpufreq_get_freq_by_idx(TARGET_DEFAULT, limit_idx);
	mtk_get_custom_upbound_gpu_freq(&max_clock_custom);
	max_clock = (max_clock_custom < max_clock) ?
			max_clock_custom : max_clock;
	return scnprintf(buf, PAGE_SIZE, "%u\n", max_clock);
}

static ssize_t gpu_max_clock_store(struct kobject *kobj,
		struct kobj_attribute *attr, const char *buf, size_t count)
{
	int max_freq = 0;
	const struct gpufreq_opp_info *power_table = NULL;
	int table_num = 0;
	int idx = 0;
	int index_count = 0;

	char acBuffer[GED_SYSFS_MAX_BUFF_SIZE];

	if ((count > 0) && (count < GED_SYSFS_MAX_BUFF_SIZE)) {
		if (scnprintf(acBuffer, GED_SYSFS_MAX_BUFF_SIZE, "%s", buf)) {
			if (kstrtoint(acBuffer, 0, &max_freq) == 0) {
				if (max_freq <= 0)
					return -EINVAL;

				power_table = gpufreq_get_working_table(TARGET_DEFAULT);
				table_num = ged_get_opp_num();

				for (idx = 0; table_num>0 && index_count < table_num; index_count++) {
					if (max_freq ==
					    power_table[idx + index_count].freq) {
						mtk_custom_upbound_gpu_freq(index_count);
						return count;
					}
				}

				GED_LOGE("SKI: set max clock failed (%d not support)!\n", max_freq);
			}
		}
	}

	return -EINVAL;
}
static KOBJ_ATTR_RW(gpu_max_clock);

ssize_t gpu_min_clock_show(struct kobject *kobj,
		struct kobj_attribute *attr, char *buf)
{
	unsigned long min_clock = 0;
	unsigned long min_clock_custom = 0;

	mtk_get_gpu_bottom_freq(&min_clock);
	mtk_get_custom_boost_gpu_freq(&min_clock_custom);
	min_clock = (min_clock_custom > min_clock) ?
			min_clock_custom : min_clock;
	return scnprintf(buf, PAGE_SIZE, "%lu\n", min_clock);
}

static ssize_t gpu_min_clock_store(struct kobject *kobj,
		struct kobj_attribute *attr, const char *buf, size_t count)
{
	int min_freq = 0;
	const struct gpufreq_opp_info *power_table = NULL;
	int table_num = 0;
	int idx = 0;
	int index_count = 0;

	char acBuffer[GED_SYSFS_MAX_BUFF_SIZE];

	if ((count > 0) && (count < GED_SYSFS_MAX_BUFF_SIZE)) {
		if (scnprintf(acBuffer, GED_SYSFS_MAX_BUFF_SIZE, "%s", buf)) {
			if (kstrtoint(acBuffer, 0, &min_freq) == 0) {
				if (min_freq <= 0)
					return -EINVAL;

				power_table = gpufreq_get_working_table(TARGET_DEFAULT);
				table_num = ged_get_opp_num();

				for (idx = 0; table_num>0 && index_count < table_num; index_count++) {
					if (min_freq ==
					    power_table[idx + index_count].freq) {
						mtk_custom_boost_gpu_freq(index_count);
						return count;
					}
				}

				GED_LOGE("SKI: set min clock failed (%d not support)!\n", min_freq);
			}
		}
	}

	return -EINVAL;
}
static KOBJ_ATTR_RW(gpu_min_clock);

ssize_t gpu_model_show(struct kobject *kobj,
		struct kobj_attribute *attr, char *buf)
{
	return scnprintf(buf, PAGE_SIZE, "Mali-G57 MC2\n");
}
static KOBJ_ATTR_RO(gpu_model);

ssize_t gpu_tmu_show(struct kobject *kobj,
		struct kobj_attribute *attr, char *buf)
{
	int temperature = 0;

	temperature = __gpufreq_get_gpu_temp();

	return scnprintf(buf, PAGE_SIZE, "%d\n", temperature);
}
static KOBJ_ATTR_RO(gpu_tmu);

GED_ERROR ged_ski_init(void)
{
	int ret = GED_OK;

	g_gpu_kobj = kobject_create_and_add("gpu", kernel_kobj);

	if (!g_gpu_kobj) {
		ret = GED_ERROR_OOM;

		GED_LOGE("SKI: failed to create g_gpu_kobj!\n");
		goto EXIT;
	}
	ret = ged_sysfs_create_file(g_gpu_kobj, &kobj_attr_gpu_available_governor);
	if (ret) {
		ret = GED_ERROR_OOM;
		GED_LOGE("SKI: failed to create gpu_available_governor!\n");
		goto EXIT;
	}
	ret = ged_sysfs_create_file(g_gpu_kobj, &kobj_attr_gpu_busy);
	if (ret) {
		ret = GED_ERROR_OOM;
		GED_LOGE("SKI: failed to create gpu_busy!\n");
		goto EXIT;
	}
	ret = ged_sysfs_create_file(g_gpu_kobj, &kobj_attr_gpu_clock);
	if (ret) {
		ret = GED_ERROR_OOM;
		GED_LOGE("SKI: failed to create gpu_clock!\n");
		goto EXIT;
	}
	ret = ged_sysfs_create_file(g_gpu_kobj, &kobj_attr_gpu_freq_table);
	if (ret) {
		ret = GED_ERROR_OOM;
		GED_LOGE("SKI: failed to create gpu_freq_table!\n");
		goto EXIT;
	}
	ret = ged_sysfs_create_file(g_gpu_kobj, &kobj_attr_gpu_governor);
	if (ret) {
		ret = GED_ERROR_OOM;
		GED_LOGE("SKI: failed to create gpu_governor!\n");
		goto EXIT;
	}
	ret = ged_sysfs_create_file(g_gpu_kobj, &kobj_attr_gpu_max_clock);
	if (ret) {
		ret = GED_ERROR_OOM;
		GED_LOGE("SKI: failed to create gpu_max_clock!\n");
		goto EXIT;
	}
	ret = ged_sysfs_create_file(g_gpu_kobj, &kobj_attr_gpu_min_clock);
	if (ret) {
		ret = GED_ERROR_OOM;
		GED_LOGE("SKI: failed to create gpu_min_clock!\n");
		goto EXIT;
	}
	ret = ged_sysfs_create_file(g_gpu_kobj, &kobj_attr_gpu_model);
	if (ret) {
		ret = GED_ERROR_OOM;
		GED_LOGE("SKI: failed to create gpu_model!\n");
		goto EXIT;
	}
	ret = ged_sysfs_create_file(g_gpu_kobj, &kobj_attr_gpu_tmu);
	if (ret) {
		ret = GED_ERROR_OOM;
		GED_LOGE("SKI: failed to create gpu_tmu!\n");
		goto EXIT;
	}
	return ret;
EXIT:
	ged_ski_exit();
	return ret;
}
void ged_ski_exit(void)
{
	ged_sysfs_remove_file(g_gpu_kobj, &kobj_attr_gpu_available_governor);
	ged_sysfs_remove_file(g_gpu_kobj, &kobj_attr_gpu_busy);
	ged_sysfs_remove_file(g_gpu_kobj, &kobj_attr_gpu_clock);
	ged_sysfs_remove_file(g_gpu_kobj, &kobj_attr_gpu_freq_table);
	ged_sysfs_remove_file(g_gpu_kobj, &kobj_attr_gpu_governor);
	ged_sysfs_remove_file(g_gpu_kobj, &kobj_attr_gpu_max_clock);
	ged_sysfs_remove_file(g_gpu_kobj, &kobj_attr_gpu_min_clock);
	ged_sysfs_remove_file(g_gpu_kobj, &kobj_attr_gpu_model);
	ged_sysfs_remove_file(g_gpu_kobj, &kobj_attr_gpu_tmu);
	kobject_put(g_gpu_kobj);
	g_gpu_kobj = NULL;
}