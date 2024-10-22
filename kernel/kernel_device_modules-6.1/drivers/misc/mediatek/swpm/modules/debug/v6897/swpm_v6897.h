/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2023 MediaTek Inc.
 */

#ifndef __SWPM_V6897_H__
#define __SWPM_V6897_H__

#define SWPM_TEST (0)
#define SWPM_DEPRECATED (0)
#include <swpm_v6897_subsys.h>

/* data shared w/ SSPM */

enum pmsr_cmd_action {
	PMSR_SET_EN,
	PMSR_SET_DBG_EN,
	PMSR_SET_LOG_INTERVAL,
};

struct share_wrap {
	/* regular update */
	unsigned int share_index_ext_addr;
	unsigned int share_index_sub_ext_addr;
	unsigned int share_ctrl_ext_addr;
	unsigned int share_swpm_pmsr_spm_addr;

	/* static data */
	unsigned int subsys_swpm_data_addr;
};


extern struct share_wrap *wrap_d;

extern void swpm_set_update_cnt(unsigned int type, unsigned int cnt);
extern void swpm_set_enable(unsigned int type, unsigned int enable);
extern int swpm_v6897_init(void);
extern void swpm_v6897_exit(void);

#endif

