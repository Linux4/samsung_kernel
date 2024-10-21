// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2023 MediaTek Inc.
 */

#include <linux/cpu.h>
#include <linux/fs.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/notifier.h>
#include <linux/of_device.h>
#include <linux/slab.h>
#include <linux/timer.h>
#include <linux/types.h>

#if IS_ENABLED(CONFIG_MTK_QOS_FRAMEWORK)
#include <mtk_qos_ipi.h>
#endif

#if IS_ENABLED(CONFIG_MTK_TINYSYS_SSPM_SUPPORT)
#include <sspm_reservedmem.h>
#endif

#include <swpm_dbg_fs_common.h>
#include <swpm_dbg_common_v1.h>
#include <swpm_module.h>
#include <swpm_module_ext.h>
#include <swpm_v6989.h>
#include <swpm_v6989_ext.h>
#include <swpm_v6989_subsys.h>

#undef swpm_dbg_log
#define swpm_dbg_log(fmt, args...) \
	do { \
		int l = scnprintf(p, sz, fmt, ##args); \
		p += l; \
		sz -= l; \
	} while (0)


#define SWPM_EXT_DBG (1)


static ssize_t enable_read(char *ToUser, size_t sz, void *priv)
{
	char *p = ToUser;

	if (!ToUser)
		return -EINVAL;

	swpm_dbg_log("echo <type or 65535> <0 or 1> > /proc/swpm/enable\n");
	swpm_dbg_log("SWPM status = 0x%x\n", swpm_status);

	return p - ToUser;
}

static ssize_t enable_write(char *FromUser, size_t sz, void *priv)
{
	int type, enable;
	int ret;

	ret = -EINVAL;

	if (!FromUser)
		goto out;

	if (sz >= MTK_SWPM_SYSFS_BUF_WRITESZ)
		goto out;

	ret = -EPERM;
	if (sscanf(FromUser, "%d %d", &type, &enable) == 2) {
		swpm_lock(&swpm_mutex);
		swpm_set_enable(type, enable);
		if (swpm_status)
			mod_timer(&swpm_timer, jiffies +
				msecs_to_jiffies(swpm_log_interval_ms));
		else
			del_timer(&swpm_timer);
		swpm_unlock(&swpm_mutex);
		ret = sz;
	}

out:
	return ret;
}

static const struct mtk_swpm_sysfs_op enable_fops = {
	.fs_read = enable_read,
	.fs_write = enable_write,
};

static ssize_t dump_power_read(char *ToUser, size_t sz, void *priv)
{
	char *p = ToUser;
	size_t mSize = 0;
	int i;

	for (i = 0; i < NR_POWER_RAIL; i++) {
		p += scnprintf(p + mSize, sz - mSize, "%s",
			       swpm_power_rail[i].name);
		if (i != NR_POWER_RAIL - 1)
			p += sprintf(p, "/");
		else
			p += sprintf(p, " = ");
	}

	for (i = 0; i < NR_POWER_RAIL; i++) {
		p += scnprintf(p + mSize, sz - mSize, "%d",
			       swpm_power_rail[i].avg_power);
		if (i != NR_POWER_RAIL - 1)
			p += sprintf(p, "/");
		else
			p += sprintf(p, " uA\n");
	}

	WARN_ON(sz - mSize <= 0);

	return p - ToUser;
}

static const struct mtk_swpm_sysfs_op dump_power_fops = {
	.fs_read = dump_power_read,
};

static unsigned int swpm_pmsr_en = 1;
static ssize_t swpm_pmsr_en_read(char *ToUser, size_t sz, void *priv)
{
	char *p = ToUser;

	if (!ToUser)
		return -EINVAL;

	swpm_dbg_log("%d\n", swpm_pmsr_en);

	return p - ToUser;
}

static ssize_t swpm_pmsr_en_write(char *FromUser, size_t sz, void *priv)
{
	unsigned int enable = 0;
	int ret;

	ret = -EINVAL;

	if (!FromUser)
		goto out;

	if (sz >= MTK_SWPM_SYSFS_BUF_WRITESZ)
		goto out;

	ret = -EPERM;
	if (!kstrtouint(FromUser, 0, &enable)) {
		swpm_pmsr_en = !!enable;
		swpm_set_only_cmd(0, swpm_pmsr_en,
				  PMSR_SET_EN, PMSR_CMD_TYPE);
		ret = sz;
	}

out:
	return ret;
}

static const struct mtk_swpm_sysfs_op swpm_pmsr_en_fops = {
	.fs_read = swpm_pmsr_en_read,
	.fs_write = swpm_pmsr_en_write,
};

static ssize_t swpm_pmsr_dbg_en_write(char *FromUser, size_t sz, void *priv)
{
	unsigned int type = 0, val = 0;
	int ret;

	ret = -EINVAL;

	if (!FromUser)
		goto out;

	if (sz >= MTK_SWPM_SYSFS_BUF_WRITESZ)
		goto out;

	ret = -EPERM;
	if (sscanf(FromUser, "%x %x", &type, &val) == 2) {
		swpm_set_only_cmd(type, val,
						  PMSR_SET_DBG_EN, PMSR_CMD_TYPE);
		ret = sz;
	}

out:
	return ret;
}

static const struct mtk_swpm_sysfs_op swpm_pmsr_dbg_en_fops = {
	.fs_write = swpm_pmsr_dbg_en_write,
};

static ssize_t swpm_pmsr_log_interval_write(char *FromUser,
					    size_t sz, void *priv)
{
	unsigned int val = 0;
	int ret;

	ret = -EINVAL;

	if (!FromUser)
		goto out;

	if (sz >= MTK_SWPM_SYSFS_BUF_WRITESZ)
		goto out;

	ret = -EPERM;

	if (!kstrtouint(FromUser, 0, &val)) {
		swpm_set_only_cmd(0, val,
				  PMSR_SET_LOG_INTERVAL, PMSR_CMD_TYPE);
		ret = sz;
	}

out:
	return ret;
}

static const struct mtk_swpm_sysfs_op swpm_pmsr_log_interval_fops = {
	.fs_write = swpm_pmsr_log_interval_write,
};

#if SWPM_EXT_DBG
static ssize_t swpm_sp_test_read(char *ToUser, size_t sz, void *priv)
{
	char *p = ToUser;
	int i;
	int32_t core_vol_num, xpu_ip_num;

	struct ip_stats *xpu_ip_stats_ptr = NULL;
	struct vol_duration *core_duration_ptr = NULL;
	struct res_sig_stats *spm_res_sig_stats_ptr = NULL;

	if (!ToUser)
		return -EINVAL;

	core_vol_num = get_vcore_vol_num();
	xpu_ip_num = get_xpu_ip_num();

	core_duration_ptr =
	kcalloc(core_vol_num, sizeof(struct vol_duration), GFP_KERNEL);
	xpu_ip_stats_ptr =
	kcalloc(xpu_ip_num, sizeof(struct ip_stats), GFP_KERNEL);
	spm_res_sig_stats_ptr =
	kzalloc(sizeof(struct res_sig_stats), GFP_KERNEL);

	if (!core_duration_ptr) {
		swpm_dbg_log("core_duration_idx failure\n");
		goto End;
	}
	if (!xpu_ip_stats_ptr) {
		swpm_dbg_log("xpu_ip_stats_idx failure\n");
		goto End;
	}
	if (!spm_res_sig_stats_ptr) {
		swpm_dbg_log("spm_res_sig_stats_idx failure\n");
		goto End;
	}

	for (i = 0; i < xpu_ip_num; i++) {
		xpu_ip_stats_ptr[i].times =
		kzalloc(sizeof(struct ip_times), GFP_KERNEL);
		if (!xpu_ip_stats_ptr[i].times)
			goto End;
	}

	sync_latest_data();

	get_vcore_vol_duration(core_vol_num, core_duration_ptr);
	get_xpu_ip_stats(xpu_ip_num, xpu_ip_stats_ptr);


	get_res_sig_stats(spm_res_sig_stats_ptr);

	swpm_dbg_log("suspend_time (ms) : %llu\n",
		     spm_res_sig_stats_ptr->suspend_time);
	swpm_dbg_log("duration_time (ms) : %llu\n",
		     spm_res_sig_stats_ptr->duration_time);

	for (i = 0; i < xpu_ip_num; i++) {
		swpm_dbg_log("%s ",
			     xpu_ip_stats_ptr[i].ip_name);
		swpm_dbg_log("active/idle/off (ms) : %lld/%lld/%lld\n",
		xpu_ip_stats_ptr[i].times->active_time,
		xpu_ip_stats_ptr[i].times->idle_time,
		xpu_ip_stats_ptr[i].times->off_time);
	}
End:
	kfree(core_duration_ptr);

	if (xpu_ip_stats_ptr) {
		for (i = 0; i < xpu_ip_num; i++)
			kfree(xpu_ip_stats_ptr[i].times);
		kfree(xpu_ip_stats_ptr);
	}

	kfree(spm_res_sig_stats_ptr);

	return p - ToUser;
}

static const struct mtk_swpm_sysfs_op swpm_sp_test_fops = {
	.fs_read = swpm_sp_test_read,
};

static ssize_t swpm_sp_ddr_idx_read(char *ToUser, size_t sz, void *priv)
{
	char *p = ToUser;
	int i, j;
	int32_t ddr_freq_num, ddr_bc_ip_num;

	struct ddr_act_times *ddr_act_times_ptr = NULL;
	struct ddr_sr_pd_times *ddr_sr_pd_times_ptr = NULL;
	struct ddr_ip_bc_stats *ddr_ip_stats_ptr = NULL;

	if (!ToUser)
		return -EINVAL;

	ddr_freq_num = get_ddr_freq_num();
	ddr_bc_ip_num = get_ddr_data_ip_num();

	ddr_act_times_ptr =
	kcalloc(ddr_freq_num, sizeof(struct ddr_act_times), GFP_KERNEL);
	ddr_sr_pd_times_ptr =
	kzalloc(sizeof(struct ddr_sr_pd_times), GFP_KERNEL);
	ddr_ip_stats_ptr =
	kcalloc(ddr_bc_ip_num,
		sizeof(struct ddr_ip_bc_stats), GFP_KERNEL);

	if (!ddr_act_times_ptr)
		goto End;
	if (!ddr_sr_pd_times_ptr)
		goto End;
	if (!ddr_ip_stats_ptr)
		goto End;

	for (i = 0; i < ddr_bc_ip_num; i++) {
		ddr_ip_stats_ptr[i].bc_stats =
		kcalloc(ddr_freq_num, sizeof(struct ddr_bc_stats), GFP_KERNEL);
		if (!ddr_ip_stats_ptr[i].bc_stats)
			goto End;
	}

	sync_latest_data();

	get_ddr_act_times(ddr_freq_num, ddr_act_times_ptr);
	get_ddr_sr_pd_times(ddr_sr_pd_times_ptr);
	get_ddr_freq_data_ip_stats(ddr_bc_ip_num,
				   ddr_freq_num,
				   ddr_ip_stats_ptr);

	swpm_dbg_log("SR time(msec): %lld\n",
		   ddr_sr_pd_times_ptr->sr_time);
	swpm_dbg_log("PD time(msec): %lld\n",
		   ddr_sr_pd_times_ptr->pd_time);

	for (i = 0; i < ddr_freq_num; i++) {
		swpm_dbg_log("Freq %dMhz: ",
			     ddr_act_times_ptr[i].freq);
		swpm_dbg_log("Time(msec):%lld ",
			     ddr_act_times_ptr[i].active_time);
		for (j = 0; j < ddr_bc_ip_num; j++) {
			swpm_dbg_log("%llu/",
				     ddr_ip_stats_ptr[j].bc_stats[i].value);
		}
		swpm_dbg_log("\n");
	}
End:
	kfree(ddr_act_times_ptr);
	kfree(ddr_sr_pd_times_ptr);

	if (ddr_ip_stats_ptr) {
		for (i = 0; i < ddr_bc_ip_num; i++)
			kfree(ddr_ip_stats_ptr[i].bc_stats);
		kfree(ddr_ip_stats_ptr);
	}

	return p - ToUser;
}

static const struct mtk_swpm_sysfs_op swpm_sp_ddr_idx_fops = {
	.fs_read = swpm_sp_ddr_idx_read,
};

static ssize_t swpm_sp_spm_sig_read(char *ToUser, size_t sz, void *priv)
{
	char *p = ToUser;
	int i, grp_id = 0, prev_id;

	struct res_sig_stats *spm_res_sig_stats_ptr;
	struct res_sig *spm_res_sig_ptr;

	if (!ToUser)
		return -EINVAL;

	spm_res_sig_stats_ptr =
	kzalloc(sizeof(struct res_sig_stats), GFP_KERNEL);

	if (!spm_res_sig_stats_ptr)
		goto END;

	get_res_sig_stats(spm_res_sig_stats_ptr);

	spm_res_sig_stats_ptr->res_sig_tbl =
	kcalloc(spm_res_sig_stats_ptr->res_sig_num,
			sizeof(struct res_sig), GFP_KERNEL);
	if (!spm_res_sig_stats_ptr->res_sig_tbl)
		goto END;
	spm_res_sig_ptr = spm_res_sig_stats_ptr->res_sig_tbl;

	sync_latest_data();

	get_res_sig_stats(spm_res_sig_stats_ptr);

	swpm_dbg_log("spm resource signal time: (ms)\n");
	swpm_dbg_log("resource signal number: %u\n", spm_res_sig_stats_ptr->res_sig_num);
	swpm_dbg_log("group 0\n");
	for (i = 0; i < spm_res_sig_stats_ptr->res_sig_num; i++) {
		prev_id = grp_id;
		grp_id = spm_res_sig_stats_ptr->res_sig_tbl[i].grp_id;

		if (prev_id != grp_id)
			swpm_dbg_log("group %d\n", grp_id);
		swpm_dbg_log("(%d)%x: %llu\n", i,
			spm_res_sig_ptr[i].sig_id,
			spm_res_sig_ptr[i].time);
	}

END:
	if (spm_res_sig_stats_ptr) {
		kfree(spm_res_sig_stats_ptr->res_sig_tbl);
		kfree(spm_res_sig_stats_ptr);
	}
	return p - ToUser;
}

static const struct mtk_swpm_sysfs_op swpm_sp_spm_sig_fops = {
	.fs_read = swpm_sp_spm_sig_read,
};
#endif

static void swpm_v6989_dbg_fs_init(void)
{
	mtk_swpm_sysfs_entry_func_node_add("enable"
			, 0644, &enable_fops, NULL, NULL);
	mtk_swpm_sysfs_entry_func_node_add("dump_power"
			, 0444, &dump_power_fops, NULL, NULL);
	mtk_swpm_sysfs_entry_func_node_add("swpm_pmsr_en"
			, 0644, &swpm_pmsr_en_fops, NULL, NULL);
	mtk_swpm_sysfs_entry_func_node_add("swpm_pmsr_dbg_en"
			, 0644, &swpm_pmsr_dbg_en_fops, NULL, NULL);
	mtk_swpm_sysfs_entry_func_node_add("swpm_pmsr_log_interval"
			, 0644, &swpm_pmsr_log_interval_fops, NULL, NULL);

#if SWPM_EXT_DBG
	mtk_swpm_sysfs_entry_func_node_add("swpm_sp_ddr_idx"
			, 0444, &swpm_sp_ddr_idx_fops, NULL, NULL);
	mtk_swpm_sysfs_entry_func_node_add("swpm_sp_test"
			, 0444, &swpm_sp_test_fops, NULL, NULL);
	mtk_swpm_sysfs_entry_func_node_add("swpm_sp_spm_sig"
			, 0444, &swpm_sp_spm_sig_fops, NULL, NULL);
#endif
}

static int __init swpm_v6989_dbg_early_initcall(void)
{
	return 0;
}
#ifndef MTK_SWPM_KERNEL_MODULE
subsys_initcall(swpm_v6989_dbg_early_initcall);
#endif

static int __init swpm_v6989_dbg_device_initcall(void)
{
	return 0;
}

static int __init swpm_v6989_dbg_late_initcall(void)
{
	/*
	 * use late init call sync to
	 * ensure qos module is ready
	 */
	swpm_dbg_common_fs_init();
	swpm_v6989_init();
	swpm_v6989_ext_init();
	swpm_v6989_dbg_fs_init();

	pr_notice("swpm init success\n");

	return 0;
}
#ifndef MTK_SWPM_KERNEL_MODULE
late_initcall_sync(swpm_v6989_dbg_late_initcall);
#endif

int __init swpm_v6989_dbg_init(void)
{
	int ret = 0;
#ifdef MTK_SWPM_KERNEL_MODULE
	ret = swpm_v6989_dbg_early_initcall();
#endif
	if (ret)
		goto swpm_v6989_dbg_init_fail;

	ret = swpm_v6989_dbg_device_initcall();

	if (ret)
		goto swpm_v6989_dbg_init_fail;

#ifdef MTK_SWPM_KERNEL_MODULE
	ret = swpm_v6989_dbg_late_initcall();
#endif

	if (ret)
		goto swpm_v6989_dbg_init_fail;

	return 0;
swpm_v6989_dbg_init_fail:
	return -EAGAIN;
}

void __exit swpm_v6989_dbg_exit(void)
{
	swpm_v6989_exit();
	swpm_v6989_ext_exit();
}

module_init(swpm_v6989_dbg_init);
module_exit(swpm_v6989_dbg_exit);

MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("v6989 software power model debug module");
MODULE_AUTHOR("MediaTek Inc.");
