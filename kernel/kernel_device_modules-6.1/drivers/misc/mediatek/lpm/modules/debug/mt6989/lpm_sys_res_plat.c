// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2023 MediaTek Inc.
 */


#include <linux/kernel.h>
#include <linux/errno.h>
#include <linux/of.h>
#include <linux/of_irq.h>
#include <linux/of_address.h>
#include <linux/slab.h>
#include <linux/io.h>
#include <linux/rtc.h>
#include <linux/wakeup_reason.h>
#include <linux/syscore_ops.h>
#include <linux/suspend.h>
#include <linux/spinlock.h>

#include <lpm.h>
#include <lpm_module.h>
#include <lpm_spm_comm.h>
#include <lpm_dbg_common_v2.h>
#include <lpm_dbg_fs_common.h>
#include <lpm_dbg_trace_event.h>
#include <lpm_dbg_logger.h>
#include <lpm_trace_event/lpm_trace_event.h>
#include <spm_reg.h>
#include <pwr_ctrl.h>
#include <mt-plat/mtk_ccci_common.h>
#include <lpm_timer.h>
#include <mtk_lpm_sysfs.h>
#include <mtk_cpupm_dbg.h>
#include <lpm_sys_res.h>
#include <lpm_sys_res_plat.h>

#include <swpm_module_ext.h>
#include <swpm_v6989_ext.h>

static struct sys_res_record sys_res_record[SYS_RES_SCENE_NUM];
static spinlock_t sys_res_lock;
static int common_log_enable;
static unsigned int sys_res_last_buffer_index;
static unsigned int sys_res_temp_buffer_index;
static unsigned int sys_res_last_suspend_diff_buffer_index;
static unsigned int sys_res_last_diff_buffer_index;

struct sys_res_group_info sys_res_group_info[NR_SPM_GRP] = {
	{289,   0,  32, 30},
	{290,  32,  33, 30},
	{291,  65,  33, 30},
	{292,  98,  34, 30},
	{293, 132,  35, 30},
	{294, 167,  36, 30},
	{295, 203,  33, 30},
	{296, 236,  11, 30},
	{0, 247,  39, 30},
	{0, 286,  11, 30},
};


static struct sys_res_mapping sys_res_mapping[] = {
	{246, "md"},
	{245, "conn"},
	{244, "scp"},
	{243, "adsp"},
	{242, "pcie"},
	{240, "uarthub"},
};

static int lpm_sys_res_alloc(struct sys_res_record *record)
{
	struct res_sig_stats *spm_res_sig_stats_ptr;

	if (!record)
		return -1;

	spm_res_sig_stats_ptr =
	kcalloc(1, sizeof(struct res_sig_stats), GFP_KERNEL);
	if (!spm_res_sig_stats_ptr)
		goto RES_SIG_ALLOC_ERROR;

	get_res_sig_stats(spm_res_sig_stats_ptr);
	spm_res_sig_stats_ptr->res_sig_tbl =
	kcalloc(spm_res_sig_stats_ptr->res_sig_num,
			sizeof(struct res_sig), GFP_KERNEL);
	if (!spm_res_sig_stats_ptr->res_sig_tbl)
		goto RES_SIG_ALLOC_TABLE_ERROR;

	get_res_sig_stats(spm_res_sig_stats_ptr);
	record->spm_res_sig_stats_ptr = spm_res_sig_stats_ptr;

	return 0;

RES_SIG_ALLOC_TABLE_ERROR:
	kfree(spm_res_sig_stats_ptr);
	record->spm_res_sig_stats_ptr = NULL;
RES_SIG_ALLOC_ERROR:
	return -1;

}

static void lpm_sys_res_free(struct sys_res_record *record)
{
	if(record && record->spm_res_sig_stats_ptr) {
		kfree(record->spm_res_sig_stats_ptr->res_sig_tbl);
		kfree(record->spm_res_sig_stats_ptr);
		record->spm_res_sig_stats_ptr = NULL;
	}
}


static int __sync_lastest_lpm_sys_res_record(struct sys_res_record *record)
{
	int ret = 0;
#if IS_ENABLED(CONFIG_MTK_SWPM_MODULE)
	if (!record ||
	    !record->spm_res_sig_stats_ptr)
		return ret;

	ret = sync_latest_data();
	get_res_sig_stats(record->spm_res_sig_stats_ptr);
#endif
	return ret;
}
static void __lpm_sys_res_record_diff(struct sys_res_record *result,
				   struct sys_res_record *prev,
				   struct sys_res_record *cur)
{
	int i;

	if (!result || !prev || !cur)
		return;

	result->spm_res_sig_stats_ptr->suspend_time =
		prev->spm_res_sig_stats_ptr->suspend_time -
		cur->spm_res_sig_stats_ptr->suspend_time;

	result->spm_res_sig_stats_ptr->duration_time =
		prev->spm_res_sig_stats_ptr->duration_time -
		cur->spm_res_sig_stats_ptr->duration_time;

	for (i = 0; i < result->spm_res_sig_stats_ptr->res_sig_num; i++) {
		result->spm_res_sig_stats_ptr->res_sig_tbl[i].time =
		prev->spm_res_sig_stats_ptr->res_sig_tbl[i].time -
		cur->spm_res_sig_stats_ptr->res_sig_tbl[i].time;
	}

}

static void __lpm_sys_res_record_add(struct sys_res_record *result,
				   struct sys_res_record *delta)
{
	int i;

	if (!result || !delta)
		return;

	result->spm_res_sig_stats_ptr->suspend_time +=
		delta->spm_res_sig_stats_ptr->suspend_time;

	result->spm_res_sig_stats_ptr->duration_time +=
		delta->spm_res_sig_stats_ptr->duration_time;

	for (i = 0; i < result->spm_res_sig_stats_ptr->res_sig_num; i++) {
		result->spm_res_sig_stats_ptr->res_sig_tbl[i].time +=
		delta->spm_res_sig_stats_ptr->res_sig_tbl[i].time;
	}
}

static int update_lpm_sys_res_record(void)
{
	unsigned int temp;
	unsigned long flag;
	int ret;

	spin_lock_irqsave(&sys_res_lock, flag);
	ret = __sync_lastest_lpm_sys_res_record(&sys_res_record[sys_res_temp_buffer_index]);

	__lpm_sys_res_record_diff(&sys_res_record[sys_res_last_diff_buffer_index],
			&sys_res_record[sys_res_temp_buffer_index],
			&sys_res_record[sys_res_last_buffer_index]);

	if (sys_res_record[sys_res_last_diff_buffer_index].spm_res_sig_stats_ptr->suspend_time > 0) {
		__lpm_sys_res_record_add(&sys_res_record[SYS_RES_SCENE_SUSPEND],
				      &sys_res_record[sys_res_last_diff_buffer_index]);

		temp = sys_res_last_diff_buffer_index;
		sys_res_last_diff_buffer_index = sys_res_last_suspend_diff_buffer_index;
		sys_res_last_suspend_diff_buffer_index = temp;

	} else {
		__lpm_sys_res_record_add(&sys_res_record[SYS_RES_SCENE_COMMON],
				      &sys_res_record[sys_res_last_diff_buffer_index]);
	}

	temp = sys_res_temp_buffer_index;
	sys_res_temp_buffer_index = sys_res_last_buffer_index;
	sys_res_last_buffer_index = temp;
	spin_unlock_irqrestore(&sys_res_lock, flag);

	return ret;
}

static struct sys_res_record *get_lpm_sys_res_record(unsigned int scene)
{
	if (scene >= SYS_RES_GET_SCENE_NUM)
		return NULL;

	switch(scene) {
	case SYS_RES_COMMON:
		return &sys_res_record[SYS_RES_SCENE_COMMON];
	case SYS_RES_SUSPEND:
		return &sys_res_record[SYS_RES_SCENE_SUSPEND];
	case SYS_RES_LAST_SUSPEND:
		return &sys_res_record[sys_res_last_suspend_diff_buffer_index];
	case SYS_RES_LAST:
		return &sys_res_record[sys_res_last_buffer_index];
	}
	return &sys_res_record[scene];
}

static uint64_t lpm_sys_res_get_detail(struct sys_res_record *record, int op, unsigned int val)
{
	uint64_t ret = 0;
	uint64_t total_time = 0, sig_time = 0;

	if (!record)
		return 0;

	switch (op) {
	case SYS_RES_DURATION:
		ret = record->spm_res_sig_stats_ptr->duration_time;
		break;
	case SYS_RES_SUSPEND_TIME:
		ret = record->spm_res_sig_stats_ptr->suspend_time;
		break;
	case SYS_RES_SIG_TIME:
		if (val >= record->spm_res_sig_stats_ptr->res_sig_num)
			return 0;
		ret = record->spm_res_sig_stats_ptr->res_sig_tbl[val].time;
		break;
	case SYS_RES_SIG_ID:
		if (val >= record->spm_res_sig_stats_ptr->res_sig_num)
			return 0;
		ret = record->spm_res_sig_stats_ptr->res_sig_tbl[val].sig_id;
		break;
	case SYS_RES_SIG_GROUP_ID:
		if (val >= record->spm_res_sig_stats_ptr->res_sig_num)
			return 0;
		ret = record->spm_res_sig_stats_ptr->res_sig_tbl[val].grp_id;
		break;
	case SYS_RES_SIG_OVERALL_RATIO:
		if (val >= record->spm_res_sig_stats_ptr->res_sig_num)
			return 0;
		total_time = record->spm_res_sig_stats_ptr->duration_time;
		sig_time = record->spm_res_sig_stats_ptr->res_sig_tbl[val].time;
		ret = sig_time < total_time ?
			(sig_time * 100) / total_time : 100;
		break;
	case SYS_RES_SIG_SUSPEND_RATIO:
		if (val >= record->spm_res_sig_stats_ptr->res_sig_num)
			return 0;
		total_time = record->spm_res_sig_stats_ptr->suspend_time;
		sig_time = record->spm_res_sig_stats_ptr->res_sig_tbl[val].time;
		ret = sig_time < total_time ?
			(sig_time * 100) / total_time : 100;
		break;
	case SYS_RES_SIG_ADDR:
		if (val >= record->spm_res_sig_stats_ptr->res_sig_num)
			return 0;
		ret = (uint64_t)(&record->spm_res_sig_stats_ptr->res_sig_tbl[val]);
		break;
	default:
		break;
	};
	return ret;
}

static unsigned int  lpm_sys_res_get_threshold(void)
{
	return sys_res_group_info[0].threshold;
}

static void lpm_sys_res_set_threshold(unsigned int val)
{
	int i;

	if (val > 100)
		val = 100;

	for (i = 0; i < NR_SPM_GRP; i++)
		sys_res_group_info[i].threshold = val;
}

static void lpm_sys_res_enable_common_log(int en)
{
	common_log_enable = (en)? 1 : 0;
}

static int lpm_sys_res_get_log_enable(void)
{
	return common_log_enable;
}

static int lpm_sys_res_get_id_name(struct sys_res_mapping **map, unsigned int *size)
{
	unsigned int res_mapping_len;

	if (!map || !size)
		return -1;

	res_mapping_len = sizeof(sys_res_mapping) / sizeof(struct sys_res_mapping);

	*size = res_mapping_len;
	*map = (struct sys_res_mapping *)&sys_res_mapping;

	return 0;
}

static void lpm_sys_res_log(unsigned int scene)
{
	#define LOG_BUF_OUT_SZ		(768)

	unsigned long flag;
	struct sys_res_record *sys_res_record;
	struct sys_res_mapping *map = NULL;
	uint64_t time, sys_index, sig_tbl_index;
	uint64_t threshold, ratio;
	unsigned int res_mapping_len, tmp_active_time, tmp_id;
	int time_type, ratio_type;
	char scene_name[15];
	char sys_res_log_buf[LOG_BUF_OUT_SZ] = { 0 };
	int i, j = 0, sys_res_log_size = 0, sys_res_update = 0;

	if (scene == SYS_RES_LAST &&
	    !common_log_enable)
		return;

	sys_res_update = update_lpm_sys_res_record();

	lpm_sys_res_get_id_name(&map, &res_mapping_len);

	if (sys_res_update) {
		pr_info("[name:spm&][SPM] SWPM data is invalid\n");
		return;
	}

	spin_lock_irqsave(&sys_res_lock, flag);

	sys_res_record = get_lpm_sys_res_record(scene);
	if (scene == SYS_RES_LAST_SUSPEND) {
		time_type = SYS_RES_SUSPEND_TIME;
		strncpy(scene_name, "suspend", 10);
		ratio_type = SYS_RES_SIG_SUSPEND_RATIO;
	} else if (scene == SYS_RES_LAST) {
		time_type = SYS_RES_DURATION;
		strncpy(scene_name, "common", 10);
		ratio_type = SYS_RES_SIG_OVERALL_RATIO;
	} else {
		spin_unlock_irqrestore(&sys_res_lock, flag);
		return;
	}

	time = lpm_sys_res_get_detail(sys_res_record, time_type, 0);
	sys_res_log_size += scnprintf(
				sys_res_log_buf + sys_res_log_size,
				LOG_BUF_OUT_SZ - sys_res_log_size,
				"[name:spm&][SPM] %s %llu ms; ", scene_name, time);

	for (i = 0; i < SYS_RES_SYS_RESOURCE_NUM; i++){
		sys_index = sys_res_group_info[i].sys_index;
		sig_tbl_index = sys_res_group_info[i].sig_table_index;
		threshold = sys_res_group_info[i].threshold;
		ratio = lpm_sys_res_get_detail(sys_res_record,
					       ratio_type,
					       sys_index);

		sys_res_log_size += scnprintf(
				sys_res_log_buf + sys_res_log_size,
				LOG_BUF_OUT_SZ - sys_res_log_size,
				"group %d ratio %llu", i, ratio);

		if (ratio < threshold) {
			sys_res_log_size += scnprintf(
					sys_res_log_buf + sys_res_log_size,
					LOG_BUF_OUT_SZ - sys_res_log_size,
					"; ");
			continue;
		}

		sys_res_log_size += scnprintf(
				sys_res_log_buf + sys_res_log_size,
				LOG_BUF_OUT_SZ - sys_res_log_size,
				" (> %llu) [", threshold);

		for (j = 0; j < sys_res_group_info[i].group_num; j++) {
			ratio = lpm_sys_res_get_detail(sys_res_record,
						       ratio_type,
						       j + sig_tbl_index);

			if (ratio < threshold)
				continue;

			if (sys_res_log_size > LOG_BUF_OUT_SZ - 45) {
				pr_info("[name:spm&][SPM] %s", sys_res_log_buf);
				sys_res_log_size = 0;
			}

			sys_res_log_size += scnprintf(
				sys_res_log_buf + sys_res_log_size,
				LOG_BUF_OUT_SZ - sys_res_log_size,
				"0x%llx(%llu%%),",
				lpm_sys_res_get_detail(sys_res_record,
					SYS_RES_SIG_ID, j + sig_tbl_index),
				ratio);
		}

		sys_res_log_size += scnprintf(
				sys_res_log_buf + sys_res_log_size,
				LOG_BUF_OUT_SZ - sys_res_log_size,
				"]; ");
	}
	pr_info("[name:spm&][SPM] %s", sys_res_log_buf);

	sys_res_log_size = 0;
	for (i = 0; i < res_mapping_len; i++) {
		tmp_id = map[i].id;
		tmp_active_time = lpm_sys_res_get_detail(sys_res_record,
					SYS_RES_SIG_TIME, tmp_id);
		sys_res_log_size += scnprintf(
				sys_res_log_buf + sys_res_log_size,
				LOG_BUF_OUT_SZ - sys_res_log_size,
				"%s: %u.%03u%s",
				map[i].name, tmp_active_time / 1000, tmp_active_time % 1000,
				i < res_mapping_len - 1 ? "," : "\n");
	}
	pr_info("[name:spm&][SPM] %s", sys_res_log_buf);

	spin_unlock_irqrestore(&sys_res_lock, flag);
}

static struct lpm_sys_res_ops sys_res_ops = {
	.get = get_lpm_sys_res_record,
	.update = update_lpm_sys_res_record,
	.get_detail = lpm_sys_res_get_detail,
	.get_threshold = lpm_sys_res_get_threshold,
	.set_threshold = lpm_sys_res_set_threshold,
	.enable_common_log = lpm_sys_res_enable_common_log,
	.get_log_enable = lpm_sys_res_get_log_enable,
	.log = lpm_sys_res_log,
	.lock = &sys_res_lock,
	.get_id_name = lpm_sys_res_get_id_name,
};

static int lpm_sys_res_pm_event(struct notifier_block *notifier,
			unsigned long pm_event, void *unused)
{
	int sys_res_update = 0;

	switch (pm_event) {
	case PM_SUSPEND_PREPARE:
		sys_res_update = sys_res_ops.update();
		if (sys_res_update)
			pr_info("[name:spm&][SPM] SWPM data is invalid\n");

		return NOTIFY_DONE;
	case PM_POST_SUSPEND:
		sys_res_ops.log(SYS_RES_LAST_SUSPEND);
		return NOTIFY_DONE;
	default:
		return NOTIFY_DONE;
	}
	return NOTIFY_OK;
}

static struct notifier_block lpm_sys_res_pm_notifier_func = {
	.notifier_call = lpm_sys_res_pm_event,
	.priority = 0,
};

int lpm_sys_res_plat_init(void)
{
	int ret, i, j;

	for (i = 0; i < SYS_RES_SCENE_NUM; i++) {
		ret = lpm_sys_res_alloc(&sys_res_record[i]);
		if(ret) {
			for (j = i - 1; j >= 0; j--)
				lpm_sys_res_free(&sys_res_record[i]);
			pr_info("[name:spm&][SPM] sys_res alloc fail\n");
			return ret;
		}
	}

	sys_res_last_buffer_index = SYS_RES_SCENE_LAST_SYNC;
	sys_res_temp_buffer_index = SYS_RES_SCENE_TEMP;

	sys_res_last_suspend_diff_buffer_index = SYS_RES_SCENE_LAST_SUSPEND_DIFF;
	sys_res_last_diff_buffer_index = SYS_RES_SCENE_LAST_DIFF;

	spin_lock_init(&sys_res_lock);

	ret = register_lpm_sys_res_ops(&sys_res_ops);
	if (ret) {
		pr_debug("[name:spm&][SPM] Failed to register LPM sys_res operations.\n");
		return ret;
	}

	ret = register_pm_notifier(&lpm_sys_res_pm_notifier_func);
	if (ret) {
		pr_debug("[name:spm&][SPM] Failed to register PM notifier.\n");
		return ret;
	}

	return 0;
}


void lpm_sys_res_plat_deinit(void)
{
	int i;

	for (i = 0; i < SYS_RES_SCENE_NUM; i++)
		lpm_sys_res_free(&sys_res_record[i]);

	unregister_lpm_sys_res_ops();
}

