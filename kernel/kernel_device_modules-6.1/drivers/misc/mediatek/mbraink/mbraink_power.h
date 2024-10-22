/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2022 MediaTek Inc.
 */
#ifndef MBRAINK_POWER_H
#define MBRAINK_POWER_H

#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/io.h>
#include "mbraink_ioctl_struct_def.h"

#if IS_ENABLED(CONFIG_MTK_LOW_POWER_MODULE) && \
		IS_ENABLED(CONFIG_MTK_SYS_RES_DBG_SUPPORT)

#include <lpm_dbg_common_v2.h>

#endif


#define MAX_POWER_HD_SZ 8
#define SPM_DATA_SZ (5640)
#define SPM_TOTAL_SZ (MAX_POWER_HD_SZ+SPM_DATA_SZ)


#if IS_ENABLED(CONFIG_MTK_LPM_MT6985) && \
	IS_ENABLED(CONFIG_MTK_LOW_POWER_MODULE) && \
	IS_ENABLED(CONFIG_MTK_ECCCI_DRIVER)

#define plat_mmio_read(offset)        __raw_readl(lpm_spm_base + offset)

#define PCM_32K_TICKS_PER_SEC           (32768)
#define PCM_TICK_TO_SEC(TICK)   (TICK / PCM_32K_TICKS_PER_SEC)

struct mbraink_26m {
#define SPM_BASE                        (0)
#define SPM_REQ_STA_0                  (SPM_BASE + 0x848)
#define SPM_REQ_STA_1                  (SPM_BASE + 0x84C)
#define SPM_REQ_STA_2                  (SPM_BASE + 0x850)
#define SPM_REQ_STA_3                  (SPM_BASE + 0x854)
#define SPM_REQ_STA_4                  (SPM_BASE + 0x858)
#define SPM_REQ_STA_5                  (SPM_BASE + 0x85C)
#define SPM_REQ_STA_6                  (SPM_BASE + 0x860)
#define SPM_REQ_STA_7                  (SPM_BASE + 0x864)
#define SPM_REQ_STA_8                  (SPM_BASE + 0x868)
#define SPM_REQ_STA_9                  (SPM_BASE + 0x86C)
#define SPM_REQ_STA_10                 (SPM_BASE + 0x870)
#define SPM_SRC_REQ                    (SPM_BASE + 0x818)
	u32 req_sta_0, req_sta_1, req_sta_2;
	u32 req_sta_3, req_sta_4, req_sta_5;
	u32 req_sta_6, req_sta_7, req_sta_8;
	u32 req_sta_9, req_sta_10;
	u32 src_req;
};

extern void __iomem *lpm_spm_base;


extern struct md_sleep_status before_md_sleep_status;
int is_md_sleep_info_valid(struct md_sleep_status *md_data);
void get_md_sleep_time(struct md_sleep_status *md_data);
#endif /*end of CONFIG_MTK_LPM_MT6985 && CONFIG_MTK_LOW_POWER_MODULE && CONFIG_MTK_ECCCI_DRIVER*/


#if IS_ENABLED(CONFIG_MTK_ECCCI_DRIVER)

#define MD_BLK_MAX_NUM 108
#define MD_DATA_TOTAL_SZ (MD_MDHD_SZ+MD_BLK_SZ*MD_BLK_MAX_NUM)
#define MD_MAX_SZ (MD_HD_SZ+MD_DATA_TOTAL_SZ)

#define MD_STATUS_W_DONE 0xEDEDEDED
#define MD_STATUS_W_ING  0xEEEEEEEE
#define MD_STATUS_R_DONE 0xFFFFFFFF

#endif

int vcorefs_get_src_req_num(void);
unsigned int *vcorefs_get_src_req(void);
void
mbraink_power_get_voting_info(struct mbraink_voting_struct_data *mbraink_vcorefs_src);

int mbraink_get_power_info(char *buffer, unsigned int size, int datatype);
int mbraink_power_getVcoreInfo(struct mbraink_power_vcoreInfo *pmbrainkPowerVcoreInfo);

void mbraink_get_power_wakeup_info(struct mbraink_power_wakeup_data *wakeup_info_buffer);

int mbraink_power_get_spm_info(struct mbraink_power_spm_raw *power_spm_buffer);
int mbraink_power_get_spm_l1_info(long long *spm_l1_array, int spm_l1_size);
int mbraink_power_get_spm_l2_info(struct mbraink_power_spm_l2_info *spm_l2_info);

int mbraink_power_get_scp_info(struct mbraink_power_scp_info *scp_info);

int mbraink_power_get_modem_info(struct mbraink_modem_raw *modem_buffer);

#endif /*end of MBRAINK_POWER_H*/
