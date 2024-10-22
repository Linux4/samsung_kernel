// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

#include <linux/module.h>
#include <linux/fs.h>
#include <linux/slab.h>
#include <swpm_module_ext.h>
#include "mbraink_memory.h"
#if IS_ENABLED(CONFIG_MTK_DVFSRC_MB)
#include <dvfsrc-mb.h>
#endif

#if IS_ENABLED(CONFIG_MTK_SWPM_MODULE)
int mbraink_memory_getDdrInfo(struct mbraink_memory_ddrInfo *pMemoryDdrInfo)
{
	int ret = 0;
	int i, j;
	int32_t ddr_freq_num = 0, ddr_bc_ip_num = 0;
	int32_t ddr_freq_check = 0, ddr_bc_ip_check = 0;

	struct ddr_act_times *ddr_act_times_ptr = NULL;
	struct ddr_sr_pd_times *ddr_sr_pd_times_ptr = NULL;
	struct ddr_ip_bc_stats *ddr_ip_stats_ptr = NULL;

	if (pMemoryDdrInfo == NULL) {
		pr_notice("pMemoryDdrInfo is NULL");
		ret = -1;
		goto End;
	}

	ddr_freq_num = get_ddr_freq_num();
	ddr_bc_ip_num = get_ddr_data_ip_num();

	ddr_act_times_ptr = kmalloc_array(ddr_freq_num, sizeof(struct ddr_act_times), GFP_KERNEL);
	if (!ddr_act_times_ptr) {
		ret = -1;
		goto End;
	}

	ddr_sr_pd_times_ptr = kmalloc(sizeof(struct ddr_sr_pd_times), GFP_KERNEL);
	if (!ddr_sr_pd_times_ptr) {
		ret = -1;
		goto End;
	}

	ddr_ip_stats_ptr = kmalloc_array(ddr_bc_ip_num,
								sizeof(struct ddr_ip_bc_stats),
								GFP_KERNEL);
	if (!ddr_ip_stats_ptr) {
		ret = -1;
		goto End;
	}

	for (i = 0; i < ddr_bc_ip_num; i++)
		ddr_ip_stats_ptr[i].bc_stats = kmalloc_array(ddr_freq_num,
								sizeof(struct ddr_bc_stats),
								GFP_KERNEL);

	sync_latest_data();

	get_ddr_act_times(ddr_freq_num, ddr_act_times_ptr);
	get_ddr_sr_pd_times(ddr_sr_pd_times_ptr);
	get_ddr_freq_data_ip_stats(ddr_bc_ip_num, ddr_freq_num, ddr_ip_stats_ptr);

	if (ddr_freq_num > MAX_DDR_FREQ_NUM) {
		pr_notice("ddr_freq_num over (%d)", MAX_DDR_FREQ_NUM);
		ddr_freq_check = MAX_DDR_FREQ_NUM;
	} else
		ddr_freq_check = ddr_freq_num;

	if (ddr_bc_ip_num > MAX_DDR_IP_NUM) {
		pr_notice("ddr_bc_ip_num over (%d)", MAX_DDR_IP_NUM);
		ddr_bc_ip_check = MAX_DDR_IP_NUM;
	} else
		ddr_bc_ip_check = ddr_bc_ip_num;

	pMemoryDdrInfo->srTimeInMs = ddr_sr_pd_times_ptr->sr_time;
	pMemoryDdrInfo->pdTimeInMs = ddr_sr_pd_times_ptr->pd_time;
	pMemoryDdrInfo->totalDdrFreqNum = ddr_freq_check;
	pMemoryDdrInfo->totalDdrIpNum = ddr_bc_ip_check;

	for (i = 0; i < ddr_freq_check; i++) {
		pMemoryDdrInfo->ddrActiveInfo[i].freqInMhz =
			ddr_act_times_ptr[i].freq;
		pMemoryDdrInfo->ddrActiveInfo[i].totalActiveTimeInMs =
			ddr_act_times_ptr[i].active_time;
		for (j = 0; j < ddr_bc_ip_check; j++) {
			pMemoryDdrInfo->ddrActiveInfo[i].totalIPActiveTimeInMs[j] =
				ddr_ip_stats_ptr[j].bc_stats[i].value;
		}
	}

End:
	if (ddr_act_times_ptr != NULL)
		kfree(ddr_act_times_ptr);

	if (ddr_sr_pd_times_ptr != NULL)
		kfree(ddr_sr_pd_times_ptr);

	if (ddr_ip_stats_ptr != NULL) {
		for (i = 0; i < ddr_bc_ip_num; i++) {
			if (ddr_ip_stats_ptr[i].bc_stats != NULL)
				kfree(ddr_ip_stats_ptr[i].bc_stats);
		}
		kfree(ddr_ip_stats_ptr);
	}

	return ret;
}

#else
int mbraink_memory_getDdrInfo(struct mbraink_memory_ddrInfo *pMemoryDdrInfo)
{
	pr_info("%s: Do not support ioctl getDdrInfo query.\n", __func__);
	if (pMemoryDdrInfo != NULL)
		pMemoryDdrInfo->totalDdrFreqNum = 0;

	return 0;
}
#endif

#if IS_ENABLED(CONFIG_MTK_DVFSRC_MB)
int mbraink_memory_getMdvInfo(struct mbraink_memory_mdvInfo  *pMemoryMdv)
{
	int ret = 0;
	int i = 0;
	struct mtk_dvfsrc_header srcHeader;

	if (pMemoryMdv == NULL) {
		ret = -1;
		goto End;
	}

	if (MAX_MDV_SZ != MAX_DATA_SIZE) {
		pr_notice("mdv data sz mis-match");
		ret = -1;
		goto End;
	}

	memset(&srcHeader, 0, sizeof(struct mtk_dvfsrc_header));
	dvfsrc_get_data(&srcHeader);
	pMemoryMdv->mid = srcHeader.module_id;
	pMemoryMdv->ver = srcHeader.version;
	pMemoryMdv->pos = srcHeader.data_offset;
	pMemoryMdv->size = srcHeader.data_length;
	for (i = 0; i<MAX_MDV_SZ; i++)
		pMemoryMdv->raw[i] = srcHeader.data[i];

End:
	return ret;
}

#else
int mbraink_memory_getMdvInfo(struct mbraink_memory_mdvInfo  *pMemoryMdv)
{
	pr_info("%s: Do not support ioctl getMdv query.\n", __func__);
	return 0;
}
#endif


