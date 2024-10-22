/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2021 MediaTek Inc.
 */
#ifndef __GED_DCS_H__
#define __GED_DCS_H__

#include "ged_type.h"

#define DCS_DEFAULT_MIN_CORE 0
#define DCS_DEBUG_MAX_CORE 12


struct dcs_core_mask {
	unsigned int core_mask;
	unsigned int core_num;
};

struct dcs_virtual_opp {
	int idx;
	unsigned int freq;
	unsigned int freq_real;
	int core_num;
};

GED_ERROR ged_dcs_init_platform_info(void);
void ged_dcs_exit(void);
struct gpufreq_core_mask_info *dcs_get_avail_mask_table(void);

int dcs_get_dcs_opp_setting(void);
int dcs_get_cur_core_num(void);
int dcs_get_max_core_num(void);
int dcs_get_avail_mask_num(void);
int dcs_set_core_mask(unsigned int core_mask, unsigned int core_num);
int dcs_restore_max_core_mask(void);
int is_dcs_enable(void);
void dcs_enable(int enable);
int dcs_set_fix_num(unsigned int core_num);
void dcs_fix_reset(void);
unsigned int dcs_get_fix_num(void);
unsigned int dcs_get_fix_mask(void);
bool dcs_get_setting_dirty(void);

// for dcs_stress
int dcs_get_dcs_stress(void);
void dcs_set_dcs_stress(int enable);

// for dcs adjust
void dcs_set_adjust_support(unsigned int val);
void dcs_set_adjust_ratio_th(unsigned int val);
void dcs_set_adjust_fr_cnt(unsigned int val);
void dcs_set_adjust_non_dcs_th(unsigned int val);

unsigned int dcs_get_adjust_support(void);
unsigned int dcs_get_adjust_ratio_th(void);
unsigned int dcs_get_adjust_fr_cnt(void);
unsigned int dcs_get_adjust_non_dcs_th(void);


#endif /* __GED_DCS_H__ */
