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
#include <linux/spinlock.h>

#include <swpm_dbg_fs_common.h>
#include <swpm_dbg_common_v1.h>
#include <swpm_module.h>
#include <swpm_module_ext.h>
#include <swpm_audio_v6897.h>

#undef swpm_dbg_log
#define swpm_dbg_log(fmt, args...) \
	do { \
		int l = scnprintf(p, sz, fmt, ##args); \
		p += l; \
		sz -= l; \
	} while (0)

static ssize_t audio_power_index_read(char *ToUser, size_t sz, void *priv)
{
	char *p = ToUser;
	struct audio_index_duration *audio_idx_duration;
	int i = 0;

	if (!ToUser)
		return -EINVAL;

	audio_idx_duration = kmalloc(sizeof(struct audio_index_duration), GFP_KERNEL);
	get_audio_power_index(audio_idx_duration);
	if (!audio_idx_duration)
		goto End;

	swpm_dbg_log("Timestamp (msec): %llu\n", audio_idx_duration->timestamp);
	swpm_dbg_log("S0 time(msec): %lld\n", audio_idx_duration->s0_time);
	swpm_dbg_log("S1 time(msec): %lld\n", audio_idx_duration->s1_time);
	swpm_dbg_log("MCUSYS active time(msec): %lld\n", audio_idx_duration->mcusys_active_time);
	swpm_dbg_log("MCUSYS power off time(msec): %lld\n", audio_idx_duration->mcusys_pd_time);
	swpm_dbg_log("CLUSTER active time(msec): %lld\n", audio_idx_duration->cluster_active_time);
	swpm_dbg_log("CLUSTER power off time(msec): %lld\n", audio_idx_duration->cluster_pd_time);
	swpm_dbg_log("ADSP active time(msec): %lld\n", audio_idx_duration->adsp_active_time);
	swpm_dbg_log("ADSP wfi time(msec): %lld\n", audio_idx_duration->adsp_wfi_time);
	swpm_dbg_log("ADSP core rst time(msec): %lld\n", audio_idx_duration->adsp_core_rst_time);
	swpm_dbg_log("ADSP core off time(msec): %lld\n", audio_idx_duration->adsp_core_off_time);
	swpm_dbg_log("ADSP power off time(msec): %lld\n", audio_idx_duration->adsp_pd_time);
	swpm_dbg_log("SCP active time(msec): %lld\n", audio_idx_duration->scp_active_time);
	swpm_dbg_log("SCP wfi time(msec): %lld\n", audio_idx_duration->scp_wfi_time);
	swpm_dbg_log("SCP cpu off time(msec): %lld\n", audio_idx_duration->scp_cpu_off_time);
	swpm_dbg_log("SCP sleep time(msec): %lld\n", audio_idx_duration->scp_sleep_time);
	swpm_dbg_log("SCP power off time(msec): %lld\n", audio_idx_duration->scp_pd_time);

	for (i = 0; i < NR_CPU_CORE; ++i) {
		swpm_dbg_log("CPU[%d] time(msec) active: %lld, wfi: %lld, off: %lld\n",
			     i, audio_idx_duration->cpu_active_time[i],
			     audio_idx_duration->cpu_wfi_time[i],
			     audio_idx_duration->cpu_pd_time[i]);
	}
	swpm_dbg_log("\n");

End:
	kfree(audio_idx_duration);
	return p - ToUser;
}

static ssize_t audio_power_index_write(char *FromUser, size_t sz, void *priv)
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
		if (val == 1)
			audio_subsys_index_init();
		ret = sz;
	}

out:
	return ret;
}

static const struct mtk_swpm_sysfs_op audio_power_index_fops = {
	.fs_read = audio_power_index_read,
	.fs_write = audio_power_index_write,
};

int __init swpm_audio_v6897_dbg_init(void)
{
	mtk_swpm_sysfs_entry_func_node_add("audio_power_index"
			, 0644, &audio_power_index_fops, NULL, NULL);

	// swpm_audio_v6897_init();

	pr_notice("swpm audio install success\n");

	return 0;
}

void __exit swpm_audio_v6897_dbg_exit(void)
{
	// swpm_audio_v6897_exit();
}

module_init(swpm_audio_v6897_dbg_init);
module_exit(swpm_audio_v6897_dbg_exit);

MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("v6897 SWPM audio debug module");
MODULE_AUTHOR("MediaTek Inc.");
