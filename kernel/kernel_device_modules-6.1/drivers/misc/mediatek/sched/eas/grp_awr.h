/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2023 MediaTek Inc.
 */
#if IS_ENABLED(CONFIG_MTK_SCHED_GROUP_AWARE)
struct gas_margin_thr {
	int gear_id;
	int group_id;
	int margin;
	int converge_thr;
};

extern int flt_sched_get_cpu_group(int cpu, int grp_id);
extern int flt_get_cpu_o(int cpu);
extern int flt_get_gp_hint(int grp_id);
extern void (*sugov_grp_awr_update_cpu_tar_util_hook)(int cpu);
extern void set_group_target_active_ratio_pct(int grp_idx, int val);
extern void set_group_target_active_ratio_cap(int grp_idx, int val);
extern void set_cpu_group_active_ratio_pct(int cpu, int grp_idx, int val);
extern void set_cpu_group_active_ratio_cap(int cpu, int grp_idx, int val);
extern void set_group_active_ratio_pct(int grp_idx, int val);
extern void set_group_active_ratio_cap(int grp_idx, int val);
extern void set_grp_awr_marg_ctrl(int val);
extern int get_grp_awr_marg_ctrl(void);
int  grp_awr_get_grp_tar_util(int grp_idx);
extern void set_grp_awr_tar_marg_ctrl(int grp_id, int val);
extern int get_grp_awr_tar_ctrl(int grp_id);
extern void set_grp_awr_thr(int gear_id, int group_id, int freq);
extern int get_grp_awr_thr(int gear_id, int group_id);
extern int get_grp_awr_thr_freq(int gear_id, int group_id);
extern void set_grp_awr_min_opp_margin(int gear_id, int group_id, int val);
extern int get_grp_awr_min_opp_margin(int gear_id, int group_id);
extern int reset_grp_awr_margin(void);
extern void set_top_grp_aware(int val, int force_ctrl);
extern int get_top_grp_aware(void);
extern int get_top_grp_aware_refcnt(void);
void grp_awr_update_grp_awr_util(void);
int grp_awr_init(void);
#endif
