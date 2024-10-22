/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2022 MediaTek Inc.
 */

#ifndef __SWPM_V6985_H__
#define __SWPM_V6985_H__

#define SWPM_TEST (0)
#define SWPM_DEPRECATED (0)

#include <swpm_v6985_subsys.h>

/* data shared w/ SSPM */
enum profile_point {
	MON_TIME,
	CALC_TIME,
	REC_TIME,
	TOTAL_TIME,

	NR_PROFILE_POINT
};

#define PMSR_MAX_SIG_SEL (48)
struct swpm_pmsr_data {
	unsigned int sig_record[PMSR_MAX_SIG_SEL];
};

enum pmsr_cmd_action {
	PMSR_SET_EN,
	PMSR_SET_SIG_SEL,
	PMSR_SET_DBG_EN,
	PMSR_SET_LOG_INTERVAL,
	PMSR_GET_SIG_SEL,
};

struct share_wrap {
	/* regular update */
	unsigned int share_index_ext_addr;
	unsigned int share_index_sub_ext_addr;
	unsigned int share_ctrl_ext_addr;

	/* static data */
	unsigned int subsys_swpm_data_addr;
};

struct swpm_common_rec_data {
	/* 8 bytes */
	unsigned int cur_idx;
	unsigned int profile_enable;

	/* 8(long) * 5(prof_pt) * 3 = 120 bytes */
	unsigned long long avg_latency[NR_PROFILE_POINT];
	unsigned long long max_latency[NR_PROFILE_POINT];
	unsigned long long prof_cnt[NR_PROFILE_POINT];

	/* 4(int) * 64(rec_cnt) * 7 = 1792 bytes */
	unsigned int pwr[NR_POWER_RAIL][MAX_RECORD_CNT];
};

struct swpm_rec_data {
	struct swpm_common_rec_data common_rec_data;
	/* total size = 8192 bytes */
} __aligned(8);


extern struct share_wrap *wrap_d;

extern void swpm_set_update_cnt(unsigned int type, unsigned int cnt);
extern void swpm_set_enable(unsigned int type, unsigned int enable);
extern int swpm_v6985_init(void);
extern void swpm_v6985_exit(void);

#endif

