// SPDX-License-Identifier: GPL-2.0
/*
 * Samsung Exynos SoC series Pablo driver
 *
 * Copyright (c) 2023 Samsung Electronics Co., Ltd
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef PABLO_DVFS_H
#define PABLO_DVFS_H

#include <dt-bindings/camera/exynos_is_dt.h>
#include "pablo-kernel-variant.h"

#define IS_DVFS_STATIC 0
#define IS_DVFS_DYNAMIC 1

#define IS_DVFS_PATH_M2M 0
#define IS_DVFS_PATH_OTF 1

#define DVFS_NOT_MATCHED 0 /* not matched in this scenario */
#define DVFS_MATCHED 1 /* matched in this scenario */
#define DVFS_SKIP 2 /* matched, but do not anything. skip changing dvfs */

#define IS_DVFS_DEC_DTIME 50
#define IS_DVFS_THROTT_CTRL_OFF 0
#define IS_SCENARIO_DEFAULT 0

#define KEEP_FRAME_TICK_FAST_LAUNCH 10
#define KEEP_FRAME_TICK_THROTT 10
#define KEEP_FRAME_TICK_ITERATION 10
#define IS_DVFS_SKIP_DYNAMIC 1
#define DVFS_SN_STR(__SCENARIO) #__SCENARIO
#define GET_DVFS_CHK_FUNC(__SCENARIO) check_##__SCENARIO
#define DECLARE_DVFS_CHK_FUNC(__SCENARIO) \
	int check_##__SCENARIO(struct is_device_ischain *device, struct camera2_shot *shot, \
			       int position, int resol, int fps, int stream_cnt, \
			       int streaming_cnt, unsigned long sensor_map, \
			       struct is_dual_info *dual_info, ...)
#define DECLARE_EXT_DVFS_CHK_FUNC(__SCENARIO) \
	int check_##__SCENARIO(struct is_device_sensor *device, int position, int resol, int fps, \
			       int stream_cnt, int streaming_cnt, unsigned long sensor_map, \
			       struct is_dual_info *dual_info, ...)
#define GET_KEY_FOR_DVFS_TBL_IDX(__HAL_VER) (#__HAL_VER "_TBL_IDX")

#define SIZE_HD (720 * 480)
#define SIZE_FHD (1920 * 1080)
#define SIZE_WHD (2560 * 1440)
#define SIZE_UHD (3840 * 2160)
#define SIZE_8K (7680 * 4320)
#define SIZE_16MP_FHD_BDS (3072 * 2304) /* based a 4:3 */
#define SIZE_12MP_FHD_BDS (2688 * 2016) /* based a 4:3 */
#define SIZE_12MP_QHD_BDS (3072 * 2304)
#define SIZE_12MP_UHD_BDS (4032 * 3024)
#define SIZE_8MP_FHD_BDS (2176 * 1632)
#define SIZE_8MP_QHD_BDS (3264 * 1836)
#define SIZE_17MP_FHD_BDS (2432 * 1824)
#define SIZE_17MP (4864 * 3648)

#define GET_CLK_MARGIN(clk, margin) ((clk) * (100 + (margin)) / 100)

#if IS_ENABLED(CONFIG_EXYNOS_PM_QOS)
static inline void is_pm_qos_add_request(struct exynos_pm_qos_request *req, int exynos_pm_qos_class,
					 s32 value)
{
	exynos_pm_qos_add_request_trace((char *)__func__, __LINE__, req, exynos_pm_qos_class,
					value);
};
#define is_pm_qos_request exynos_pm_qos_request
#define is_pm_qos_update_request exynos_pm_qos_update_request
#define is_pm_qos_remove_request exynos_pm_qos_remove_request
#define is_pm_qos_request_active exynos_pm_qos_request_active
#else
#define is_pm_qos_request dev_pm_qos_request
#define is_pm_qos_add_request(arg...)
#define is_pm_qos_update_request(arg...)
#define is_pm_qos_remove_request(arg...)
#define is_pm_qos_request_active(arg...) 0
#endif

#define IS_DVFS_SCENARIO_COMMON_MODE_SHIFT 0
#define IS_DVFS_SCENARIO_COMMON_MODE_MASK 0x3
#define IS_DVFS_SCENARIO_VENDOR_SHIFT 16
#define IS_DVFS_SCENARIO_VENDOR_MASK 0xffff

#define IS_DVFS_SCENARIO_WIDTH_SHIFT 0
#define IS_DVFS_SCENARIO_HEIGHT_SHIFT 16

enum is_dvfs_scenario_common_mode {
	IS_DVFS_SCENARIO_COMMON_MODE_PHOTO = 1,
	IS_DVFS_SCENARIO_COMMON_MODE_VIDEO = 2,
};

enum is_dvfs_scenario_vendor {
	IS_DVFS_SCENARIO_VENDOR_NONE = 0,
	IS_DVFS_SCENARIO_VENDOR_VT = 1,
	IS_DVFS_SCENARIO_VENDOR_SSM = 2,
	IS_DVFS_SCENARIO_VENDOR_VIDEO_HDR = 3,
	IS_DVFS_SCENARIO_VENDOR_SUPER_STEADY = 4,
	IS_DVFS_SCENARIO_VENDOR_LOW_POWER = 5,
	IS_DVFS_SCENARIO_VENDOR_PIP = 6,
	IS_DVFS_SCENARIO_VENDOR_SENSING_CAMERA = 7,
};

enum is_dvfs_state {
	IS_DVFS_TMU_THROTT,
	IS_DVFS_TICK_THROTT,
};

enum is_dvfs_val {
	IS_DVFS_VAL_LEV,
	IS_DVFS_VAL_FRQ,
	IS_DVFS_VAL_QOS,
	IS_DVFS_VAL_END,
};

enum is_dvfs_lv {
	IS_DVFS_LV_END = 20,
};

struct is_dvfs_dt_t {
	const char *parse_scenario_nm; /* string for parsing from DTS */
	u32 scenario_id; /* scenario_id */
	int keep_frame_tick; /* keep qos lock during specific frames when dynamic scenario */
};

struct is_dvfs_scenario_ctrl {
	unsigned int cur_scenario_id; /* selected scenario idx */
	int cur_frame_tick; /* remained frame tick to keep qos lock in dynamic scenario */
	char *scenario_nm; /* string of scenario_id */
};

struct is_dvfs_dt_info {
	/* These fields are to return qos value for dvfs scenario */
	u32 (*dvfs_data)[IS_DVFS_END];
	u32 qos_tb[IS_DVFS_END][IS_DVFS_LV_END][IS_DVFS_VAL_END];
	bool qos_otf[IS_DVFS_END];
	const char *qos_name[IS_DVFS_END];
	const char **dvfs_cpu;
	u32 scenario_count;
};

struct is_pm_qos_ops {
	void (*add_request)(struct is_pm_qos_request *req, int exynos_pm_qos_class, s32 value);
	void (*update_request)(struct is_pm_qos_request *req, s32 new_value);
	void (*remove_request)(struct is_pm_qos_request *req);
	int (*request_active)(struct is_pm_qos_request *req);
};

struct is_emstune_ops {
	void (*add_request)(void);
	void (*remove_request)(void);
	void (*boost)(int enable);
};

struct is_dvfs_ext_func {
	struct is_pm_qos_ops *pm_qos_ops;
	struct is_emstune_ops *emstune_ops;
};

struct is_dvfs_iteration_mode {
	u32 changed;
	u32 iter_svhist;
};

struct is_dvfs_rnr_mode {
	u32 changed;
	u32 rnr;
};

struct is_dvfs_ctrl {
	struct mutex lock;
	u32 cur_lv[IS_DVFS_END];
	int cur_hpg_qos;
	int cur_hmp_bst;
	u32 dvfs_scenario;
	u32 dvfs_rec_size;
	ulong state;
	const char *cur_cpus;

	struct is_dvfs_dt_info dvfs_info;
	struct is_dvfs_iteration_mode *iter_mode;
	struct is_dvfs_rnr_mode rnr_mode;
	struct is_dvfs_scenario_ctrl *static_ctrl;
	struct is_dvfs_scenario_ctrl *dynamic_ctrl;

	u32 dec_dtime;
	u32 thrott_ctrl;
	struct delayed_work dec_dwork;
	struct is_pm_qos_request user_qos;
	int *scenario_idx;
};

typedef u32 (*hw_dvfs_lv_getter_t)(struct is_dvfs_ctrl *dvfs_ctrl, u32 type);
typedef void (*pablo_clock_change_cb)(u32 clock);

#ifdef CONFIG_PM_DEVFREQ
int is_dvfs_init(struct is_dvfs_ctrl *dvfs_ctrl, struct is_dvfs_dt_t *dvfs_dt_arr,
		 hw_dvfs_lv_getter_t hw_dvfs_lv_get_func);
int is_dvfs_sel_static(struct is_dvfs_ctrl *dvfs_ctrl, int scenario_id);
int is_dvfs_sel_dynamic(struct is_dvfs_ctrl *dvfs_ctrl, bool reprocessing_mode);
bool is_dvfs_update_dynamic(struct is_dvfs_ctrl *dvfs_ctrl, int scenario_id);
int is_dvfs_get_freq(struct is_dvfs_ctrl *dvfs_ctrl, u32 *qos_freq);
int is_add_dvfs(struct is_dvfs_ctrl *dvfs_ctrl, int scenario_id, u32 *qos_thput);
int is_remove_dvfs(struct is_dvfs_ctrl *dvfs_ctrl, int scenario_id);
int is_set_dvfs_m2m(struct is_dvfs_ctrl *dvfs_ctrl, int scenario_id, bool reprocessing_mode);
int is_set_dvfs_otf(struct is_dvfs_ctrl *dvfs_ctrl, int scenario_id);
int pablo_set_static_dvfs(struct is_dvfs_ctrl *dvfs_ctrl, const char *suffix, int scenario_id,
			  int path, bool reprocessing_mode);
int is_set_static_dvfs(
	struct is_dvfs_ctrl *dvfs_ctrl, int scenario_id, bool stream_on, bool fast_launch);
unsigned int is_get_bit_count(unsigned long bits);
int is_dvfs_ems_init(void);
int is_dvfs_ems_reset(struct is_dvfs_ctrl *dvfs_ctrl);
int is_dvfs_set_ext_func(struct is_dvfs_ext_func *ext_func);
int pablo_register_change_dvfs_cb(u32 dvfs_t, pablo_clock_change_cb cb);
#define pablo_devfreq_g_domain_freq exynos_devfreq_get_domain_freq
#else
#define is_dvfs_init(d, a, f) ({ 0; })
#define is_dvfs_sel_static(d, i) ({ 0; })
#define is_dvfs_sel_dynamic(d, r) ({ 0; })
#define is_dvfs_update_dynamic(d, i) ({ 0; })
#define is_dvfs_get_freq(d, q) ({ 0; })
#define is_add_dvfs(d, i, q) ({ 0; })
#define is_remove_dvfs(d, i) ({ 0; })
#define is_set_dvfs_m2m(d, i, r) ({ 0; })
#define is_set_dvfs_otf(d, i) ({ 0; })
#define pablo_set_static_dvfs(d, s, i, p, r) ({ 0; })
#define is_set_static_dvfs(d, i, s, f) ({ 0; })
#define is_get_bit_count(b) ({ 0; })
#define is_dvfs_ems_init() ({ 0; })
#define is_dvfs_ems_reset(d) ({ 0; })
#define is_dvfs_set_ext_func(e) ({ 0; })
#define pablo_register_change_dvfs_cb(d, c) ({ 0; })
#define pablo_devfreq_g_domain_freq(arg...)
#endif /* CONFIG_PM_DEVFREQ */
#endif /* PABLO_DVFS_H */
