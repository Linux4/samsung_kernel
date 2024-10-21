// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2023 MediaTek Inc.
 */

#include <swpm_audio_v6897.h>

static struct audio_index_duration audio_idx_duration;
/* index snapshot */
DEFINE_SPINLOCK(audio_swpm_spinlock);

void audio_subsys_index_update(struct audio_index_ext *audio_idx_ext)
{
	struct timespec64 timestamp;
	int i = 0;
	unsigned long flags = 0;

	ktime_get_ts64(&timestamp);
	if (audio_idx_ext) {
		spin_lock_irqsave(&audio_swpm_spinlock, flags);
		audio_idx_duration.timestamp = timespec64_to_ns(&timestamp) / 1000000;

		audio_idx_duration.s0_time += audio_idx_ext->s0_time / 1000;
		audio_idx_duration.s1_time += audio_idx_ext->s1_time / 1000;

		audio_idx_duration.mcusys_active_time +=
			audio_idx_ext->mcusys_active_time / 1000;
		audio_idx_duration.mcusys_pd_time +=
			audio_idx_ext->mcusys_pd_time / 1000;

		audio_idx_duration.cluster_active_time +=
			audio_idx_ext->cluster_active_time / 1000;
		audio_idx_duration.cluster_pd_time +=
			audio_idx_ext->cluster_pd_time / 1000;

		audio_idx_duration.adsp_active_time +=
			audio_idx_ext->adsp_active_time / 1000;
		audio_idx_duration.adsp_wfi_time += audio_idx_ext->adsp_wfi_time / 1000;
		audio_idx_duration.adsp_core_rst_time +=
			audio_idx_ext->adsp_core_rst_time / 1000;
		audio_idx_duration.adsp_core_off_time +=
			audio_idx_ext->adsp_core_off_time / 1000;
		audio_idx_duration.adsp_pd_time += audio_idx_ext->adsp_pd_time / 1000;

		audio_idx_duration.scp_active_time +=
			audio_idx_ext->scp_active_time / 1000;
		audio_idx_duration.scp_wfi_time += audio_idx_ext->scp_wfi_time / 1000;
		audio_idx_duration.scp_cpu_off_time +=
			audio_idx_ext->scp_cpu_off_time / 1000;
		audio_idx_duration.scp_sleep_time +=
			audio_idx_ext->scp_sleep_time / 1000;
		audio_idx_duration.scp_pd_time +=
			audio_idx_ext->scp_pd_time / 1000;

		for (i = 0; i < NR_CPU_CORE; ++i) {
			audio_idx_duration.cpu_active_time[i] +=
				audio_idx_ext->cpu_active_time[i] / 1000;
			audio_idx_duration.cpu_wfi_time[i] +=
				audio_idx_ext->cpu_wfi_time[i] / 1000;
			audio_idx_duration.cpu_pd_time[i] +=
				audio_idx_ext->cpu_pd_time[i] / 1000;
		}
		spin_unlock_irqrestore(&audio_swpm_spinlock, flags);
	}
}
EXPORT_SYMBOL(audio_subsys_index_update);

void audio_subsys_index_init(void)
{
	unsigned long flags;

	spin_lock_irqsave(&audio_swpm_spinlock, flags);
	memset(&audio_idx_duration, 0, sizeof(struct audio_index_duration));
	spin_unlock_irqrestore(&audio_swpm_spinlock, flags);
}
EXPORT_SYMBOL(audio_subsys_index_init);

void get_audio_power_index(struct audio_index_duration *duration)
{
	unsigned long flags;

	sync_latest_data();
	if (duration) {
		spin_lock_irqsave(&audio_swpm_spinlock, flags);
		memcpy(duration, &audio_idx_duration, sizeof(struct audio_index_duration));
		spin_unlock_irqrestore(&audio_swpm_spinlock, flags);
	}
}
EXPORT_SYMBOL(get_audio_power_index);
