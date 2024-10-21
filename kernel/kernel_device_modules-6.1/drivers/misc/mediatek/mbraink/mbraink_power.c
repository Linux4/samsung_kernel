// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2022 MediaTek Inc.
 */

#include <linux/module.h>
#include <linux/fs.h>
#include <linux/rtc.h>
#include <linux/sched/clock.h>
#include <swpm_module_ext.h>

#include "mbraink_power.h"

#include <scp_mbrain_dbg.h>


#if IS_ENABLED(CONFIG_MTK_LOW_POWER_MODULE) && \
	IS_ENABLED(CONFIG_MTK_SYS_RES_DBG_SUPPORT)

#include <lpm_sys_res_mbrain_dbg.h>

unsigned char *g_spm_raw;
unsigned int g_data_size;

#endif

#if IS_ENABLED(CONFIG_MTK_ECCCI_DRIVER)
#include "mtk_ccci_common.h"

unsigned int g_md_last_has_data_blk_idx;
unsigned int g_md_last_read_blk_idx;
unsigned int g_md_read_count;
#endif


#if IS_ENABLED(CONFIG_MTK_LPM_MT6985) && \
	IS_ENABLED(CONFIG_MTK_LOW_POWER_MODULE) && \
	IS_ENABLED(CONFIG_MTK_ECCCI_DRIVER)

int mbraink_get_power_info(char *buffer, unsigned int size, int datatype)
{
	int idx = 0, n = 0;
	struct mbraink_26m mbraink_26m_stat;
	struct lpm_dbg_lp_info mbraink_lpm_dbg_lp_info;
	struct md_sleep_status mbraink_md_data;
	struct timespec64 tv = { 0 };
	const char * const mbraink_lp_state_name[NUM_SPM_STAT] = {
		"AP",
		"26M",
		"VCORE",
	};

	memset(&mbraink_26m_stat, 0, sizeof(mbraink_26m_stat));
	memset(&mbraink_lpm_dbg_lp_info, 0, sizeof(mbraink_lpm_dbg_lp_info));
	memset(&mbraink_md_data, 0, sizeof(mbraink_md_data));

	ktime_get_real_ts64(&tv);
	n += snprintf(buffer + n, size, "systime:%lld\n", tv.tv_sec);

	n += snprintf(buffer + n, size, "datatype:%d\n", datatype);

	for (idx = 0; idx < NUM_SPM_STAT; idx++) {
		n += snprintf(buffer + n, size, "Idle_count %s:%lld\n",
			mbraink_lp_state_name[idx], mbraink_lpm_dbg_lp_info.record[idx].count);
	}
	for (idx = 0; idx < NUM_SPM_STAT; idx++) {
		n += snprintf(buffer + n, size, "Idle_period %s:%lld.%03lld\n",
			mbraink_lp_state_name[idx],
			PCM_TICK_TO_SEC(mbraink_lpm_dbg_lp_info.record[idx].duration),
			PCM_TICK_TO_SEC((mbraink_lpm_dbg_lp_info.record[idx].duration%
				PCM_32K_TICKS_PER_SEC)*
				1000));
	}

	for (idx = 0; idx < NUM_SPM_STAT; idx++) {
		n += snprintf(buffer + n, size, "Suspend_count %s:%lld\n",
			mbraink_lp_state_name[idx], mbraink_lpm_dbg_lp_info.record[idx].count);
	}
	for (idx = 0; idx < NUM_SPM_STAT; idx++) {
		n += snprintf(buffer + n, size, "Suspend_period %s:%lld.%03lld\n",
			mbraink_lp_state_name[idx],
			PCM_TICK_TO_SEC(mbraink_lpm_dbg_lp_info.record[idx].duration),
			PCM_TICK_TO_SEC((mbraink_lpm_dbg_lp_info.record[idx].duration%
				PCM_32K_TICKS_PER_SEC)*
				1000));
	}

	get_md_sleep_time(&mbraink_md_data);
	if (!is_md_sleep_info_valid(&mbraink_md_data))
		pr_notice("mbraink_md_data is not valid!\n");

	n += snprintf(buffer + n, size, "MD:%lld.%03lld\nMD_2G:%lld.%03lld\nMD_3G:%lld.%03lld\n",
		mbraink_md_data.md_sleep_time / 1000000,
		(mbraink_md_data.md_sleep_time % 1000000) / 1000,
		mbraink_md_data.gsm_sleep_time / 1000000,
		(mbraink_md_data.gsm_sleep_time % 1000000) / 1000,
		mbraink_md_data.wcdma_sleep_time / 1000000,
		(mbraink_md_data.wcdma_sleep_time % 1000000) / 1000);

	n += snprintf(buffer + n, size, "MD_4G:%lld.%03lld\nMD_5G:%lld.%03lld\n",
		mbraink_md_data.lte_sleep_time / 1000000,
		(mbraink_md_data.lte_sleep_time % 1000000) / 1000,
		mbraink_md_data.nr_sleep_time / 1000000,
		(mbraink_md_data.nr_sleep_time % 1000000) / 1000);

	mbraink_26m_stat.req_sta_0 = plat_mmio_read(SPM_REQ_STA_0);
	mbraink_26m_stat.req_sta_1 = plat_mmio_read(SPM_REQ_STA_1);
	mbraink_26m_stat.req_sta_2 = plat_mmio_read(SPM_REQ_STA_2);
	mbraink_26m_stat.req_sta_3 = plat_mmio_read(SPM_REQ_STA_3);
	mbraink_26m_stat.req_sta_4 = plat_mmio_read(SPM_REQ_STA_4);
	mbraink_26m_stat.req_sta_5 = plat_mmio_read(SPM_REQ_STA_5);
	mbraink_26m_stat.req_sta_6 = plat_mmio_read(SPM_REQ_STA_6);
	mbraink_26m_stat.req_sta_7 = plat_mmio_read(SPM_REQ_STA_7);
	mbraink_26m_stat.req_sta_8 = plat_mmio_read(SPM_REQ_STA_8);
	mbraink_26m_stat.req_sta_9 = plat_mmio_read(SPM_REQ_STA_9);
	mbraink_26m_stat.req_sta_10 = plat_mmio_read(SPM_REQ_STA_10);
	mbraink_26m_stat.src_req = plat_mmio_read(SPM_SRC_REQ);

	n += snprintf(buffer + n, size, "req_sta_0:%u\nreq_sta_1:%u\nreq_sta_2:%u\nreq_sta_3:%u\n",
		mbraink_26m_stat.req_sta_0, mbraink_26m_stat.req_sta_1,
		mbraink_26m_stat.req_sta_2, mbraink_26m_stat.req_sta_3);
	n += snprintf(buffer + n, size, "req_sta_4:%u\nreq_sta_5:%u\nreq_sta_6:%u\nreq_sta_7:%u\n",
		mbraink_26m_stat.req_sta_4, mbraink_26m_stat.req_sta_5,
		mbraink_26m_stat.req_sta_6, mbraink_26m_stat.req_sta_7);
	n += snprintf(buffer + n, size, "req_sta_8:%u\nreq_sta_9:%u\nreq_sta_10:%u\nsrc_req:%u\n",
		mbraink_26m_stat.req_sta_8, mbraink_26m_stat.req_sta_9,
		mbraink_26m_stat.req_sta_10, mbraink_26m_stat.src_req);
	buffer[n] = '\0';

	return n;
}
#else
int mbraink_get_power_info(char *buffer, unsigned int size, int datatype)
{
	pr_info("%s: Do not support ioctl power query.\n", __func__);
	return 0;
}
#endif

#if IS_ENABLED(CONFIG_MTK_SWPM_MODULE)
int mbraink_power_getVcoreInfo(struct mbraink_power_vcoreInfo *pmbrainkPowerVcoreInfo)
{
	int ret = 0;
	int i = 0;
	int32_t core_vol_num = 0, core_ip_num = 0;
	struct ip_stats *core_ip_stats_ptr = NULL;
	struct vol_duration *core_duration_ptr = NULL;
	int32_t core_vol_check = 0, core_ip_check = 0;

	core_vol_num = get_vcore_vol_num();
	core_ip_num = get_vcore_ip_num();

	core_duration_ptr = kcalloc(core_vol_num, sizeof(struct vol_duration), GFP_KERNEL);
	if (core_duration_ptr == NULL) {
		pr_notice("core_duration_idx failure\n");
		ret = -1;
		goto End;
	}

	core_ip_stats_ptr = kcalloc(core_ip_num, sizeof(struct ip_stats), GFP_KERNEL);
	if (core_ip_stats_ptr == NULL) {
		pr_notice("core_ip_stats_idx failure\n");
		ret = -1;
		goto End;
	}

	for (i = 0; i < core_ip_num; i++) {
		core_ip_stats_ptr[i].times =
		kzalloc(sizeof(struct ip_times), GFP_KERNEL);
		if (core_ip_stats_ptr[i].times == NULL) {
			pr_notice("times failure\n");
			goto End;
		}
	}

	sync_latest_data();

	get_vcore_vol_duration(core_vol_num, core_duration_ptr);
	get_vcore_ip_vol_stats(core_ip_num, core_vol_num, core_ip_stats_ptr);

	if (core_vol_num > MAX_VCORE_NUM) {
		pr_notice("core vol num over (%d)", MAX_VCORE_NUM);
		core_vol_check = MAX_VCORE_NUM;
	} else
		core_vol_check = core_vol_num;

	if (core_ip_num > MAX_VCORE_IP_NUM) {
		pr_notice("core_ip_num over (%d)", MAX_VCORE_IP_NUM);
		core_ip_check = MAX_VCORE_IP_NUM;
	} else
		core_ip_check = core_ip_num;

	pmbrainkPowerVcoreInfo->totalVCNum = core_vol_check;
	pmbrainkPowerVcoreInfo->totalVCIpNum = core_ip_check;

	for (i = 0; i < core_vol_check; i++) {
		pmbrainkPowerVcoreInfo->vcoreDurationInfo[i].vol =
			core_duration_ptr[i].vol;
		pmbrainkPowerVcoreInfo->vcoreDurationInfo[i].duration =
			core_duration_ptr[i].duration;
	}

	for (i = 0; i < core_ip_check; i++) {
		strncpy(pmbrainkPowerVcoreInfo->vcoreIpDurationInfo[i].ip_name,
			core_ip_stats_ptr[i].ip_name,
			MAX_IP_NAME_LENGTH - 1);
		pmbrainkPowerVcoreInfo->vcoreIpDurationInfo[i].times.active_time =
			core_ip_stats_ptr[i].times->active_time;
		pmbrainkPowerVcoreInfo->vcoreIpDurationInfo[i].times.idle_time =
			core_ip_stats_ptr[i].times->idle_time;
		pmbrainkPowerVcoreInfo->vcoreIpDurationInfo[i].times.off_time =
			core_ip_stats_ptr[i].times->off_time;
	}

End:
	if (core_duration_ptr != NULL)
		kfree(core_duration_ptr);

	if (core_ip_stats_ptr != NULL) {
		for (i = 0; i < core_ip_num; i++) {
			if (core_ip_stats_ptr[i].times != NULL)
				kfree(core_ip_stats_ptr[i].times);
		}
		kfree(core_ip_stats_ptr);
	}

	return ret;
}

#else
int mbraink_power_getVcoreInfo(struct mbraink_audio_idleRatioInfo *pmbrainkAudioIdleRatioInfo)
{
	pr_info("%s: Do not support ioctl vcore info query.\n", __func__);
	return 0;
}
#endif //#if IS_ENABLED(CONFIG_MTK_SWPM_MODULE)

void mbraink_get_power_wakeup_info(struct mbraink_power_wakeup_data *wakeup_info_buffer)
{
	struct wakeup_source *ws = NULL;
	ktime_t total_time;
	ktime_t max_time;
	unsigned long active_count;
	ktime_t active_time;
	ktime_t prevent_sleep_time;
	int lockid = 0;
	unsigned short i = 0;
	unsigned short pos = 0;
	unsigned short count = 5000;

	if (wakeup_info_buffer == NULL)
		return;

	lockid = wakeup_sources_read_lock();

	ws = wakeup_sources_walk_start();
	pos = wakeup_info_buffer->next_pos;
	while (ws && (pos > i) && (count > 0)) {
		ws = wakeup_sources_walk_next(ws);
		i++;
		count--;
	}

	for (i = 0; i < MAX_WAKEUP_SOURCE_NUM; i++) {
		if (ws != NULL) {
			wakeup_info_buffer->next_pos++;

			total_time = ws->total_time;
			max_time = ws->max_time;
			prevent_sleep_time = ws->prevent_sleep_time;
			active_count = ws->active_count;
			if (ws->active) {
				ktime_t now = ktime_get();

				active_time = ktime_sub(now, ws->last_time);
				total_time = ktime_add(total_time, active_time);
				if (active_time > max_time)
					max_time = active_time;

				if (ws->autosleep_enabled) {
					prevent_sleep_time = ktime_add(prevent_sleep_time,
						ktime_sub(now, ws->start_prevent_time));
				}
			} else {
				active_time = 0;
			}

			if (ws->name != NULL) {
				if ((strlen(ws->name) > 0) && (strlen(ws->name) < MAX_NAME_SZ)) {
					memcpy(wakeup_info_buffer->drv_data[i].name,
					ws->name, strlen(ws->name));
				}
			}

			wakeup_info_buffer->drv_data[i].active_count = active_count;
			wakeup_info_buffer->drv_data[i].event_count = ws->event_count;
			wakeup_info_buffer->drv_data[i].wakeup_count = ws->wakeup_count;
			wakeup_info_buffer->drv_data[i].expire_count = ws->expire_count;
			wakeup_info_buffer->drv_data[i].active_time = ktime_to_ms(active_time);
			wakeup_info_buffer->drv_data[i].total_time = ktime_to_ms(total_time);
			wakeup_info_buffer->drv_data[i].max_time = ktime_to_ms(max_time);
			wakeup_info_buffer->drv_data[i].last_time = ktime_to_ms(ws->last_time);
			wakeup_info_buffer->drv_data[i].prevent_sleep_time = ktime_to_ms(
				prevent_sleep_time);

			ws = wakeup_sources_walk_next(ws);
		}
	}
	wakeup_sources_read_unlock(lockid);

	if (ws != NULL)
		wakeup_info_buffer->is_has_data = 1;
	else
		wakeup_info_buffer->is_has_data = 0;
}

#if IS_ENABLED(CONFIG_MTK_LOW_POWER_MODULE) && \
	IS_ENABLED(CONFIG_MTK_SYS_RES_DBG_SUPPORT)

int mbraink_power_get_spm_l1_info(long long *out_spm_l1_array, int spm_l1_size)
{
	int ret = -1;
	int i = 0;
	long long value = 0;
	struct lpm_sys_res_mbrain_dbg_ops *sys_res_mbrain_ops = NULL;
	unsigned char *spm_l1_data = NULL;
	unsigned int data_size = 0;
	int size = 0;

	if (out_spm_l1_array == NULL)
		return -1;

	if (spm_l1_size != SPM_L1_DATA_NUM)
		return -1;

	sys_res_mbrain_ops = get_lpm_mbrain_dbg_ops();
	if (sys_res_mbrain_ops &&
		sys_res_mbrain_ops->get_last_suspend_res_data) {

		data_size = SPM_L1_SZ;
		spm_l1_data = kmalloc(data_size, GFP_KERNEL);
		if (spm_l1_data != NULL) {
			memset(spm_l1_data, 0, data_size);
			if (sys_res_mbrain_ops->get_last_suspend_res_data(spm_l1_data,
					data_size) == 0) {
				size = sizeof(value);
				for (i = 0; i < SPM_L1_DATA_NUM; i++) {
					if ((i*size + size) <= SPM_L1_SZ)
						memcpy(&value, (const void *)(spm_l1_data
							+ i*size), size);
					out_spm_l1_array[i] = value;
				}
				ret = 0;
			}
		}
	}

	if (spm_l1_data != NULL) {
		kfree(spm_l1_data);
		spm_l1_data = NULL;
	}

	return ret;
}

int mbraink_power_get_spm_l2_info(struct mbraink_power_spm_l2_info *spm_l2_info)
{
	struct lpm_sys_res_mbrain_dbg_ops *sys_res_mbrain_ops = NULL;
	unsigned char *ptr = NULL, *sig_tbl_ptr = NULL;
	unsigned int sig_num = 0;
	uint32_t thr[4];

	if (spm_l2_info == NULL)
		return 0;

	sys_res_mbrain_ops = get_lpm_mbrain_dbg_ops();

	if (sys_res_mbrain_ops &&
		sys_res_mbrain_ops->get_over_threshold_num &&
		sys_res_mbrain_ops->get_over_threshold_data) {

		thr[0] = spm_l2_info->value[0];
		thr[1] = spm_l2_info->value[1];
		thr[2] = spm_l2_info->value[2];
		thr[3] = spm_l2_info->value[3];

		ptr = kmalloc(SPM_L2_LS_SZ, GFP_KERNEL);
		if (ptr == NULL)
			goto End;

		if (sys_res_mbrain_ops->get_over_threshold_num(ptr,
			SPM_L2_LS_SZ, thr, 4) == 0) {
			memcpy(&sig_num, ptr+24, sizeof(sig_num));

			if (sig_num > SPM_L2_MAX_RES_NUM)
				goto End;

			sig_tbl_ptr = kmalloc(sig_num*SPM_L2_RES_SIZE, GFP_KERNEL);
			if (sig_tbl_ptr == NULL)
				goto End;

			if (sys_res_mbrain_ops->get_over_threshold_data(sig_tbl_ptr
				, sig_num*SPM_L2_RES_SIZE) == 0) {
				memcpy(spm_l2_info->spm_data, ptr, SPM_L2_LS_SZ);
				if ((SPM_L2_LS_SZ + sig_num*SPM_L2_RES_SIZE) <= SPM_L2_SZ)
					memcpy(spm_l2_info->spm_data + SPM_L2_LS_SZ, sig_tbl_ptr,
					sig_num*SPM_L2_RES_SIZE);
			}
		}
	}
End:

	if (ptr != NULL) {
		kfree(ptr);
		ptr = NULL;
	}

	if (sig_tbl_ptr != NULL) {
		kfree(sig_tbl_ptr);
		sig_tbl_ptr = NULL;
	}

	return 0;
}


int mbraink_power_get_spm_info(struct mbraink_power_spm_raw *spm_buffer)
{
	bool bfree = false;
	struct lpm_sys_res_mbrain_dbg_ops *sys_res_mbrain_ops = NULL;

	if (spm_buffer == NULL) {
		bfree = true;
		goto End;
	}

	if (spm_buffer->type == 1) {

		sys_res_mbrain_ops = get_lpm_mbrain_dbg_ops();

		if (sys_res_mbrain_ops && sys_res_mbrain_ops->get_length) {
			g_data_size = sys_res_mbrain_ops->get_length();
			g_data_size += MAX_POWER_HD_SZ;
			pr_notice("g_data_size(%d)\n", g_data_size);
		}

		if (g_data_size == 0) {
			bfree = true;
			goto End;
		}

		if (g_spm_raw != NULL) {
			vfree(g_spm_raw);
			g_spm_raw = NULL;
		}

		if (g_data_size <= SPM_TOTAL_SZ) {
			g_spm_raw = vmalloc(g_data_size);
			if (g_spm_raw != NULL) {
				memset(g_spm_raw, 0, g_data_size);

				if (sys_res_mbrain_ops &&
				   sys_res_mbrain_ops->get_data &&
				   sys_res_mbrain_ops->get_data(g_spm_raw, g_data_size) != 0) {
					bfree = true;
					goto End;
				}
			}
		}
	}

	if (g_spm_raw != NULL) {
		if (((spm_buffer->pos+spm_buffer->size) > (g_data_size)) ||
			spm_buffer->size > sizeof(spm_buffer->spm_data)) {
			bfree = true;
			goto End;
		}
		memcpy(spm_buffer->spm_data, g_spm_raw+spm_buffer->pos, spm_buffer->size);

		if (spm_buffer->type == 0) {
			bfree = true;
			goto End;
		}
	}

End:
	if (bfree == true) {
		if (g_spm_raw != NULL) {
			vfree(g_spm_raw);
			g_spm_raw = NULL;
		}
		g_data_size = 0;
	}

	return 0;
}

#else

int mbraink_power_get_spm_info(struct mbraink_power_spm_raw *spm_buffer)
{
	pr_notice("not support spm info\n");
	return 0;
}

#endif


int mbraink_power_get_scp_info(struct mbraink_power_scp_info *scp_info)
{
	struct scp_res_mbrain_dbg_ops *scp_res_mbrain_ops = NULL;
	unsigned int data_size = 0;
	unsigned char *ptr = NULL;

	if (scp_info == NULL)
		return -1;

	scp_res_mbrain_ops = get_scp_mbrain_dbg_ops();

	if (scp_res_mbrain_ops &&
		scp_res_mbrain_ops->get_length &&
		scp_res_mbrain_ops->get_data) {

		data_size = scp_res_mbrain_ops->get_length();

		if (data_size > 0 && data_size <= sizeof(scp_info->scp_data)) {
			ptr = kmalloc(data_size, GFP_KERNEL);
			if (ptr == NULL)
				goto End;

			scp_res_mbrain_ops->get_data(ptr, data_size);
			if (data_size <= sizeof(scp_info->scp_data))
				memcpy(scp_info->scp_data, ptr, data_size);

		} else {
			goto End;
		}
	}

End:
	if (ptr != NULL) {
		kfree(ptr);
		ptr = NULL;
	}

	return 0;
}



#if IS_ENABLED(CONFIG_MTK_ECCCI_DRIVER)

int mbraink_power_get_modem_info(struct mbraink_modem_raw *modem_buffer)
{
	int shm_size = 0;
	void __iomem *shm_addr = NULL;
	unsigned char *base_addr = NULL;
	unsigned char *read_addr = NULL;
	int i = 0;
	unsigned int mem_status = 0;
	unsigned int read_blk_idx = 0;
	unsigned int offset = 0;
	bool ret = true;

	if (modem_buffer == NULL)
		return 0;

	shm_addr = get_smem_start_addr(SMEM_USER_32K_LOW_POWER, &shm_size);
	if (shm_addr == NULL) {
		pr_notice("get_smem_start_addr addr is null\n");
		return 0;
	}

	if (shm_size == 0 || MD_MAX_SZ > shm_size) {
		pr_notice("get_smem_start_addr size(%d) is incorrect\n", shm_size);
		return 0;
	}

	base_addr = (unsigned char *)shm_addr;

	if (modem_buffer->type == 0) {
		read_addr = base_addr;
		memcpy(modem_buffer->data1, read_addr, MD_HD_SZ);
		read_addr = base_addr + MD_HD_SZ;
		memcpy(modem_buffer->data2, read_addr, MD_MDHD_SZ);

		if (modem_buffer->data1[0] != 1 ||  modem_buffer->data1[2] != 8) {
			modem_buffer->is_has_data = 0;
			modem_buffer->count = 0;
			return 0;
		}

		g_md_read_count = 0;
		g_md_last_read_blk_idx = g_md_last_has_data_blk_idx;

		pr_notice("g_md_last_read_blk_idx(%d)", g_md_last_read_blk_idx);
	}

	read_blk_idx = g_md_last_read_blk_idx;
	i = 0;
	ret = true;
	while ((g_md_read_count < MD_BLK_MAX_NUM) && (i < MD_SECBLK_NUM)) {
		offset = MD_HD_SZ + MD_MDHD_SZ + read_blk_idx*MD_BLK_SZ;
		if (offset > shm_size) {
			ret = false;
			break;
		}
		read_addr = base_addr + offset;
		memcpy(&mem_status, read_addr, sizeof(mem_status));

		read_blk_idx = (read_blk_idx + 1) % MD_BLK_MAX_NUM;
		g_md_read_count++;

		if (mem_status == MD_STATUS_W_DONE) {
			offset = i*MD_BLK_SZ;
			if ((offset + MD_BLK_SZ) > sizeof(modem_buffer->data3)) {
				ret = false;
				break;
			}

			memcpy(modem_buffer->data3 + offset, read_addr, MD_BLK_SZ);
			//reset mem_status mem_count after read data
			mem_status = MD_STATUS_R_DONE;
			memcpy(read_addr, &mem_status, sizeof(mem_status));
			memset(read_addr + sizeof(mem_status), 0, 4); //mem_count
			i++;
			g_md_last_has_data_blk_idx = read_blk_idx;
		}
	}

	if ((g_md_read_count < MD_BLK_MAX_NUM) && (ret == true))
		modem_buffer->is_has_data = 1;
	else
		modem_buffer->is_has_data = 0;

	g_md_last_read_blk_idx = read_blk_idx;
	modem_buffer->count = i;

	return 0;
}

#else

int mbraink_power_get_modem_info(struct mbraink_modem_raw *modem_buffer)
{
	pr_notice("not support eccci modem interface\n");
	return 0;
}

#endif

void
mbraink_power_get_voting_info(struct mbraink_voting_struct_data *mbraink_vcorefs_src)
{
	unsigned int *mbraink_voting_ret = NULL;
	int idx = 0;
	int voting_num = 0;

	mbraink_voting_ret = vcorefs_get_src_req();
	voting_num = vcorefs_get_src_req_num();

	if (voting_num > MAX_STRUCT_SZ)
		voting_num = MAX_STRUCT_SZ;

	memset(mbraink_vcorefs_src, 0, sizeof(struct mbraink_voting_struct_data));

	if (mbraink_voting_ret) {
		mbraink_vcorefs_src->voting_num = voting_num;
		for (idx = 0; idx < voting_num; idx++) {
			mbraink_vcorefs_src->mbraink_voting_data[idx] =
				mbraink_voting_ret[idx];
		}
	} else {
		mbraink_vcorefs_src->voting_num = -1;
		pr_info("vcore voting system is not support on kernel space !\n");
	}
}
