// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2023 MediaTek Inc.
 */


#include <linux/module.h>
#include <linux/list.h>
#include <lpm.h>
#include <mtk_lpm_sysfs.h>
#include <mtk_lp_sysfs.h>
#include <lpm_sys_res.h>

#define LPM_SYS_RES_NAME "sys_res_ratio"
#define mtk_dbg_sys_res_log(fmt, args...)\
	do { \
		int l = scnprintf(p, sz, fmt, ##args); \
		p += l; \
		sz -= l; \
	} while (0)

static struct mtk_lp_sysfs_handle lpm_entry_sys_res;
static struct mtk_lp_sysfs_handle lpm_entry_sys_res_suspend;
static struct mtk_lp_sysfs_handle lpm_entry_sys_res_common;

static char *sys_res_scene_name[] = {
	[SYS_RES_SCENE_COMMON] = "common",
	[SYS_RES_SCENE_SUSPEND] = "suspend",
	[SYS_RES_SCENE_LAST_DIFF] = "last_diff",
	[SYS_RES_SCENE_LAST_SYNC] = "last_sync",
	[SYS_RES_SCENE_TEMP] = "temp",
};

static ssize_t sys_res_common_stat_read(char *ToUserBuf, size_t sz, void *priv)
{
	char *p = ToUserBuf;
	int i;
	unsigned long flag;
	struct lpm_sys_res_ops *sys_res_ops;
	struct sys_res_record *sys_res_record;

	sys_res_ops = get_lpm_sys_res_ops();

	if (!sys_res_ops || !sys_res_ops->get)
		return p - ToUserBuf;

	if (sys_res_ops->update)
		sys_res_ops->update();

	sys_res_record = sys_res_ops->get(SYS_RES_COMMON);

	if (!sys_res_record)
		return p - ToUserBuf;

	spin_lock_irqsave(sys_res_ops->lock, flag);
	mtk_dbg_sys_res_log("scene: %s\n", sys_res_scene_name[SYS_RES_SCENE_COMMON]);
	mtk_dbg_sys_res_log("suspend : %llu, duration: %llu\n",
		sys_res_record->spm_res_sig_stats_ptr->suspend_time,
		sys_res_record->spm_res_sig_stats_ptr->duration_time);

	for (i = 0; i < sys_res_record->spm_res_sig_stats_ptr->res_sig_num; i++)
		mtk_dbg_sys_res_log("index %d, grp_id %d, signal_id 0x%x : %llu\n", i,
			sys_res_record->spm_res_sig_stats_ptr->res_sig_tbl[i].grp_id,
			sys_res_record->spm_res_sig_stats_ptr->res_sig_tbl[i].sig_id,
			sys_res_record->spm_res_sig_stats_ptr->res_sig_tbl[i].time);
	spin_unlock_irqrestore(sys_res_ops->lock, flag);

	return p - ToUserBuf;
}
static const struct mtk_lp_sysfs_op lpm_sys_res_common_stat_fops = {
	.fs_read = sys_res_common_stat_read,
};

static ssize_t sys_res_common_log_read(char *ToUserBuf, size_t sz, void *priv)
{
	char *p = ToUserBuf;
	struct lpm_sys_res_ops *sys_res_ops;

	sys_res_ops = get_lpm_sys_res_ops();
	if (!sys_res_ops ||
	    !sys_res_ops->get_log_enable)
		return p - ToUserBuf;

	mtk_dbg_sys_res_log("%d\n",sys_res_ops->get_log_enable());

	return p - ToUserBuf;
}

static ssize_t sys_res_common_log_write(char *FromUserBuf, size_t sz, void *priv)
{
	unsigned int val;
	struct lpm_sys_res_ops *sys_res_ops;

	if (!kstrtouint(FromUserBuf, 10, &val)) {
		sys_res_ops = get_lpm_sys_res_ops();
		if (!sys_res_ops ||
		    !sys_res_ops->enable_common_log)
			return sz;

		sys_res_ops->enable_common_log(val);
	}

	return sz;
}
static const struct mtk_lp_sysfs_op lpm_sys_res_common_log_fops = {
	.fs_read = sys_res_common_log_read,
	.fs_write = sys_res_common_log_write,
};

static ssize_t sys_res_suspend_stat_read(char *ToUserBuf, size_t sz, void *priv)
{
	char *p = ToUserBuf;
	int i;
	unsigned long flag;
	struct lpm_sys_res_ops *sys_res_ops;
	struct sys_res_record *sys_res_record;

	sys_res_ops = get_lpm_sys_res_ops();

	if (!sys_res_ops || !sys_res_ops->get)
		return p - ToUserBuf;

	if (sys_res_ops->update)
		sys_res_ops->update();

	sys_res_record = sys_res_ops->get(SYS_RES_SUSPEND);

	if (!sys_res_record)
		return p - ToUserBuf;

	spin_lock_irqsave(sys_res_ops->lock, flag);
	mtk_dbg_sys_res_log("scene: %s\n", sys_res_scene_name[SYS_RES_SCENE_SUSPEND]);
	mtk_dbg_sys_res_log("suspend : %llu, duration: %llu\n",
		sys_res_record->spm_res_sig_stats_ptr->suspend_time,
		sys_res_record->spm_res_sig_stats_ptr->duration_time);


	for (i = 0; i < sys_res_record->spm_res_sig_stats_ptr->res_sig_num; i++)
		mtk_dbg_sys_res_log("index %d, grp_id %d, signal_id 0x%x : %llu\n", i,
			sys_res_record->spm_res_sig_stats_ptr->res_sig_tbl[i].grp_id,
			sys_res_record->spm_res_sig_stats_ptr->res_sig_tbl[i].sig_id,
			sys_res_record->spm_res_sig_stats_ptr->res_sig_tbl[i].time);
	spin_unlock_irqrestore(sys_res_ops->lock, flag);

	return p - ToUserBuf;
}
static const struct mtk_lp_sysfs_op lpm_sys_res_suspend_stat_fops = {
	.fs_read = sys_res_suspend_stat_read,
};

static ssize_t sys_res_last_suspend_stat_read(char *ToUserBuf, size_t sz, void *priv)
{
	char *p = ToUserBuf;
	int i;
	unsigned long flag;
	struct lpm_sys_res_ops *sys_res_ops;
	struct sys_res_record *sys_res_record;

	sys_res_ops = get_lpm_sys_res_ops();

	if (!sys_res_ops || !sys_res_ops->get)
		return p - ToUserBuf;

	if (sys_res_ops->update)
		sys_res_ops->update();

	sys_res_record = sys_res_ops->get(SYS_RES_LAST_SUSPEND);

	if (!sys_res_record)
		return p - ToUserBuf;

	spin_lock_irqsave(sys_res_ops->lock, flag);
	mtk_dbg_sys_res_log("scene: last suspend\n");
	mtk_dbg_sys_res_log("suspend : %llu, duration: %llu\n",
		sys_res_record->spm_res_sig_stats_ptr->suspend_time,
		sys_res_record->spm_res_sig_stats_ptr->duration_time);

	for (i = 0; i < sys_res_record->spm_res_sig_stats_ptr->res_sig_num; i++)
		mtk_dbg_sys_res_log("index %d, grp_id %d, signal_id 0x%x : %llu\n", i,
			sys_res_record->spm_res_sig_stats_ptr->res_sig_tbl[i].grp_id,
			sys_res_record->spm_res_sig_stats_ptr->res_sig_tbl[i].sig_id,
			sys_res_record->spm_res_sig_stats_ptr->res_sig_tbl[i].time);
	spin_unlock_irqrestore(sys_res_ops->lock, flag);

	return p - ToUserBuf;
}

static const struct mtk_lp_sysfs_op lpm_sys_res_last_suspend_stat_fops = {
	.fs_read = sys_res_last_suspend_stat_read,
};
static ssize_t sys_res_suspend_threshold_read(char *ToUserBuf, size_t sz, void *priv)
{
	char *p = ToUserBuf;
	struct lpm_sys_res_ops *sys_res_ops;

	sys_res_ops = get_lpm_sys_res_ops();

	if (!sys_res_ops || !sys_res_ops->get_detail)
		return p - ToUserBuf;

	mtk_dbg_sys_res_log("%d\n", sys_res_ops->get_threshold());

	return p - ToUserBuf;
}

static ssize_t sys_res_suspend_threshold_write(char *FromUserBuf, size_t sz, void *priv)
{
	unsigned int val;
	struct lpm_sys_res_ops *sys_res_ops;

	if (!kstrtouint(FromUserBuf, 10, &val)) {
		sys_res_ops = get_lpm_sys_res_ops();
		if (!sys_res_ops || !sys_res_ops->set_threshold)
			return sz;

		sys_res_ops->set_threshold(val);
	}

	return sz;
}

static const struct mtk_lp_sysfs_op lpm_sys_res_suspend_threshold_fops = {
	.fs_read = sys_res_suspend_threshold_read,
	.fs_write = sys_res_suspend_threshold_write,
};
int lpm_sys_res_fs_init(void)
{
	int ret;

	ret = mtk_lpm_sysfs_sub_entry_add(LPM_SYS_RES_NAME, 0644, NULL,
					&lpm_entry_sys_res);

	ret = mtk_lpm_sysfs_sub_entry_add("suspend", 0644, &lpm_entry_sys_res,
					&lpm_entry_sys_res_suspend);

	ret = mtk_lpm_sysfs_sub_entry_add("common", 0644, &lpm_entry_sys_res,
					&lpm_entry_sys_res_common);

	mtk_lpm_sysfs_sub_entry_node_add("stat", 0444, &lpm_sys_res_suspend_stat_fops,
						&lpm_entry_sys_res_suspend, NULL);
	mtk_lpm_sysfs_sub_entry_node_add("last_suspend", 0444, &lpm_sys_res_last_suspend_stat_fops,
						&lpm_entry_sys_res_suspend, NULL);
	mtk_lpm_sysfs_sub_entry_node_add("threshold", 0444, &lpm_sys_res_suspend_threshold_fops,
						&lpm_entry_sys_res_suspend, NULL);


	mtk_lpm_sysfs_sub_entry_node_add("stat", 0444, &lpm_sys_res_common_stat_fops,
						&lpm_entry_sys_res_common, NULL);
	mtk_lpm_sysfs_sub_entry_node_add("log_enable", 0444, &lpm_sys_res_common_log_fops,
						&lpm_entry_sys_res_common, NULL);

	return 0;
}
EXPORT_SYMBOL(lpm_sys_res_fs_init);

int lpm_sys_res_fs_deinit(void)
{
	return 0;
}
EXPORT_SYMBOL(lpm_sys_res_fs_deinit);
