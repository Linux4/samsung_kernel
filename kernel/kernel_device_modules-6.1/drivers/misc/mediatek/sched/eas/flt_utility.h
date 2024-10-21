/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2021 MediaTek Inc.
 */
 #ifndef _FLT_UTILITY_H
#define _FLT_UTILITY_H

/* API Function pointer*/
extern int (*flt_get_ws_api)(void);
extern int (*flt_set_ws_api)(int ws);
extern int (*flt_sched_set_group_policy_eas_api)(int grp_id, int ws, int wp, int wc);
extern int (*flt_sched_get_group_policy_eas_api)(int grp_id, int *ws, int *wp, int *wc);
extern int (*flt_sched_set_cpu_policy_eas_api)(int cpu, int ws, int wp, int wc);
extern int (*flt_sched_get_cpu_policy_eas_api)(int cpu, int *ws, int *wp, int *wc);
extern int (*flt_get_sum_group_api)(int grp_id);
extern int (*flt_get_gear_sum_pelt_group_api)(unsigned int gear_id, int grp_id);
extern int (*flt_sched_get_gear_sum_group_eas_api)(int gear_id, int grp_id);
extern int (*flt_get_cpu_by_wp_api)(int cpu);
extern int (*flt_get_task_by_wp_api)(struct task_struct *p, int wc, int task_wp);
extern int (*flt_sched_get_cpu_group_eas_api)(int cpu, int grp_id);
extern int (*flt_get_grp_h_eas_api)(int grp_id);
extern int (*flt_get_cpu_r_api)(int cpu);
extern int (*flt_get_cpu_o_eas_api)(int grp_id);
extern int (*flt_get_total_gp_api)(void);
extern int (*flt_get_grp_r_eas_api)(int grp_id);
extern void (*flt_ctl_api)(int set);
#define CPU_NUM		8
#define PER_ENTRY	4
#define RESERVED_LEN	96
#define WP_LEN		16
#define WC_LEN		16
#define WC_MASK	0xffff
#define CPU_S		(0)
#define GP_NIDS		(CPU_S + (PER_ENTRY * CPU_NUM))
#define GP_NIDWP	(GP_NIDS + (PER_ENTRY))
#define CPU_R		(GP_NIDWP + (PER_ENTRY * GROUP_ID_RECORD_MAX))
#define GP_R		(CPU_R + (PER_ENTRY * CPU_NUM))
#define GP_H		(GP_R + (PER_ENTRY * GROUP_ID_RECORD_MAX))
#define FLT_VALID	(GP_H + (PER_ENTRY *GROUP_ID_RECORD_MAX))
#define GP_RWP		(FLT_VALID + PER_ENTRY)
#define AP_CPU_SETTING_ADDR (GP_RWP + (PER_ENTRY * GROUP_ID_RECORD_MAX) + RESERVED_LEN)
#define AP_GP_SETTING_STA_ADDR (AP_CPU_SETTING_ADDR + (PER_ENTRY * CPU_NUM))
#define AP_FLT_CTL (AP_GP_SETTING_STA_ADDR + (PER_ENTRY * GROUP_ID_RECORD_MAX))
#define AP_WS_CTL (AP_FLT_CTL + PER_ENTRY)

#define FLT_MODE2_EN 2
#define DEFAULT_WS 4
#define CPU_DEFAULT_WC GROUP_RAVG_HIST_SIZE_MAX
#define CPU_DEFAULT_WP WP_MODE_4

/* NID mode */
enum _flt_nid_mode {
	FLT_GP_NID = 0,
	FLT_GP_ID = 1,
	FLT_GP_NUM,
};

void flt_mode2_register_api_hooks(void);
void flt_mode2_init_res(void);

/* interface api */
void flt_update_data(unsigned int data, unsigned int offset);
unsigned int flt_get_data(unsigned int offset);
int flt_setnid(u32 mode);
u32 flt_getnid(void);
#endif /* _FLT_UTILITY_H */
