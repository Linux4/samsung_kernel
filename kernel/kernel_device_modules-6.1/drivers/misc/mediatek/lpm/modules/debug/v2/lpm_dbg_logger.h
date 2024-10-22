/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

#ifndef __LPM_DBG_LOGGER_H__
#define __LPM_DBG_LOGGER_H__

#include <lpm_dbg_common_v2.h>

extern void __iomem *lpm_spm_base;

struct spm_req_sta_list {
	struct subsys_req *spm_req;
	unsigned int spm_req_num;
	unsigned int spm_req_sta_addr;
	unsigned int spm_req_sta_num;
	unsigned int lp_scenario_sta;
	unsigned int is_blocked;
	struct rtc_time *suspend_tm;
};

struct lpm_logger_mbrain_dbg_ops {
	unsigned int (*get_last_suspend_wakesrc)(void);
};

int lpm_logger_init(void);
void lpm_logger_deinit(void);
struct spm_req_sta_list *spm_get_req_sta_list(void);
char *get_spm_resource_str(unsigned int index);
char *get_spm_scenario_str(unsigned int index);
struct lpm_logger_mbrain_dbg_ops *get_lpm_logger_mbrain_dbg_ops(void);
int register_lpm_logger_mbrain_dbg_ops(struct lpm_logger_mbrain_dbg_ops *ops);
void unregister_lpm_logger_mbrain_dbg_ops(void);

#endif
