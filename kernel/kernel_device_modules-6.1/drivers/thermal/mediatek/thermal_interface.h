/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2020 MediaTek Inc.
 */

#ifndef __THERMAL_INTERFACE_H__
#define __THERMAL_INTERFACE_H__
/*SYSRAM offset*/
#define CPU_TEMP_OFFSET             (0)
#define CPU_HEADROOM_OFFSET         (0x20)
#define CPU_HEADROOM_RATIO_OFFSET   (0x40)
#define CPU_PREDICT_TEMP_OFFSET     (0x60)
#define AP_NTC_HEADROOM_OFFSET      (0x80)
#define TPCB_OFFSET                 (0x84)
#define TARGET_TPCB_OFFSET          (0x88)
#define SPORTS_MODE_ENABLE          (0x90)
#define VTSKIN                      (0x94)
#define DSU_AVG_TEMP_BASE_ADDR_OFFSET       (0x98)
#define CG_POLICY_MODE_OFFSET       (0x9C)
#define DSU_CEILING_FREQ_OFFSET     (0xC4)
#define TCM_BUF_OFFSET              (0xC8)
#define TTJ_OFFSET                  (0x100)
#define POWER_BUDGET_OFFSET         (0x110)
#define CPU_MIN_OPP_HINT_OFFSET     (0x120)
#define CPU_ACTIVE_BITMASK_OFFSET   (0x130)
#define CPU_JATM_SUSPEND_OFFSET     (0x140)
#define GPU_JATM_SUSPEND_OFFSET     (0x144)
#define MIN_THROTTLE_FREQ_OFFSET    (0x14C)

/*GPU ST0/ST2/ST3/ST4/ST5/ST6 = 0x15C/0x160/0x164/0x168/0x16C/0x170*/
#define GPU_ST0_FREQ_OFFSET         (0x15C)

#define GPU_TEMP_OFFSET             (0x180)
#define APU_TEMP_OFFSET             (0x190)
#define EMUL_TEMP_OFFSET            (0x1B0)
#define CPU_LIMIT_FREQ_OFFSET       (0x200)
#define CPU_CUR_FREQ_OFFSET         (0x210)
#define CPU_MAX_TEMP_OFFSET         (0x220)
#define CPU_LIMIT_OPP_OFFSET        (0x260)
#define CPU_ATC_OFFSET              (0x280)
#define GPU_ATC_OFFSET              (0x2C8)
#define APU_ATC_OFFSET              (0x2D4)
#define CPU_ATC_NUM                 (17)
#define GPU_ATC_NUM                 (3)
#define APU_ATC_NUM                 (3)
#define UTC_COUNT_OFFSET            (0x27C)
#define INFOB_OFFSET                (0x2C4)
#define REBOOT_TEMPERATURE_ADDR_OFFSET (0x39c)
#define GPU_COOLER_BASE             (0x3A0)
#define CPU_COOLER_BASE             (0x3D0)
#define COLD_INTERRUPT_ENABLE_OFFSET (0x398)

#define APU_MBOX_TTJ_OFFSET        (0x700)
#define APU_MBOX_PB_OFFSET         (0x704)
#define APU_MBOX_TEMP_OFFSET       (0x708)
#define APU_MBOX_LIMIT_OPP_OFFSET  (0x70C)
#define APU_MBOX_CUR_OPP_OFFSET    (0x710)
#define APU_MBOX_EMUL_TEMP_OFFSET  (0x714)
#define APU_MBOX_ATC_MAX_TTJ_ADDR  (0x718)


/*TCM offset*/
#define CPU_TEMP_TCM_OFFSET             (0)
#define CPU_HEADROOM_TCM_OFFSET         (0x20)
#define CPU_HEADROOM_RATIO_TCM_OFFSET   (0x40)
#define CPU_PREDICT_TEMP_TCM_OFFSET     (0x60)
#define AP_NTC_HEADROOM_TCM_OFFSET      (0x80)
#define TPCB_TCM_OFFSET                 (0x84)
#define TARGET_TPCB_TCM_OFFSET          (0x88)
#define SPORTS_MODE_TCM_ENABLE          (0x90)
#define VTSKIN_TCM                      (0x94)
#define DSU_AVG_TEMP_BASE_ADDR_TCM_OFFSET       (0x98)
#define CG_POLICY_MODE_TCM_OFFSET       (0x9C)
/*TTJ*/
#define TTJ_TCM_OFFSET                  (0xA0)

/*power budget*/
#define POWER_BUDGET_TCM_OFFSET         (0xA4)

/*min opp hint*/
#define CPU_MIN_OPP_HINT_TCM_OFFSET     (0xA8)
#define CPU_ACTIVE_BITMASK_TCM_OFFSET   (0xB8)
#define CPU_JATM_SUSPEND_TCM_OFFSET     (0xBC)
#define MIN_THROTTLE_FREQ_TCM_OFFSET    (0xC0)

#define EMUL_TEMP_TCM_OFFSET            (0xD0)
#define CPU_LIMIT_FREQ_TCM_OFFSET       (0xD4)
#define CPU_CUR_FREQ_TCM_OFFSET         (0xE4)
#define CPU_MAX_TEMP_TCM_OFFSET         (0xF4)
#define CPU_LIMIT_OPP_TCM_OFFSET        (0x124)
#define UTC_COUNT_TCM_OFFSET            (0x134)

/*monitor Tj*/
/*LVTS1~LVTS16, 0x178~0x184 reserved*/
#define MONITOR_TJ_LVTS_TCM_OFFSET      (0x138)

/*CORE0~CORE7, 0x1A8,0x1AC reserved*/
#define MONITOR_TJ_CORE_TEMP_TCM_OFFSET (0x188)

#define CPU_COOLER_TCM_BASE             (0x1B0)


#define DSU_CEILING_FREQ_TCM_OFFSET     (0x220)


/*monitor Tj*/
/*LVTS1~LVTS16,0x138~0x174; 0x178~0x184 reserved*/
#define CPU_LVTS_TEMP_MA_TCM_ADDR                 (0x138)

/*CORE0~CORE7,0x188~0x1A4, 0x1A8,0x1AC reserved*/
#define CPU_CORE_TEMP_MA_TCM_ADDR                 (0x188)

struct headroom_info {
	int temp;
	int predict_temp;
	int headroom;
	int ratio;
};
enum headroom_id {
	/* SoC Tj */
	SOC_CPU0,
	SOC_CPU1,
	SOC_CPU2,
	SOC_CPU3,
	SOC_CPU4,
	SOC_CPU5,
	SOC_CPU6,
	SOC_CPU7,
	/* PCB */
	PCB_AP,

	NR_HEADROOM_ID
};

enum ttj_user {
	JATM_OFF = -1,
	CATM,
	JATM_ON,
	NR_TTJ_USER
};

struct ttj_info {
	int jatm_on;
	unsigned int catm_cpu_ttj;
	unsigned int catm_gpu_ttj;
	unsigned int catm_apu_ttj;
	unsigned int cpu_max_ttj;
	unsigned int gpu_max_ttj;
	unsigned int apu_max_ttj;
	unsigned int min_ttj;
};

struct frs_info {
	int enable;
	int activated;
	int pid;
	int target_fps;
	int diff;
	int tpcb;
	int tpcb_slope;
	int ap_headroom;
	int n_sec_to_ttpcb;
	int frs_target_fps;
	int real_fps;
	int target_tpcb;
	int ptime;
};

#define MAX_MD_NAME_LENGTH  (20)

struct md_thermal_sensor_t {
	int id;
	char sensor_name[MAX_MD_NAME_LENGTH];
	int cur_temp;
};

struct md_thermal_actuator_t {
	int id;
	char actuator_name[MAX_MD_NAME_LENGTH];
	int cur_status;
	int max_status;
};

struct md_info {
	int sensor_num;
	struct md_thermal_sensor_t *sensor_info;
	int md_autonomous_ctrl;
	int actuator_num;
	struct md_thermal_actuator_t *actuator_info;
};

struct pid_term_info {
	int limit_state;
	int p;
	int i;
	int d;
};

struct pid_info {
	int pid_num;
	struct pid_term_info *pid_term_data;
};

#define USER_VSENSOR_NAME 32

struct user_vsensor_info {
	int temp;
	char user_vsensor_name[USER_VSENSOR_NAME];
};

extern void update_ap_ntc_headroom(int temp, int polling_interval);
extern int get_thermal_headroom(enum headroom_id id);
extern int set_cpu_min_opp(int gear, int opp);
extern int set_cpu_active_bitmask(int mask);
extern int get_cpu_temp(int cpu_id);
extern void set_ttj(int user);
extern void write_jatm_suspend(int jatm_suspend);
extern int get_jatm_suspend(void);
extern int get_catm_ttj(void);
extern int get_catm_min_ttj(void);
extern int get_dsu_temp(void);
extern int set_reboot_temperature(int temp);
extern int set_cold_interrupt_enable_addr(int val);
extern int get_dsu_ceiling_freq(void);
extern int get_cpu_ceiling_freq (int cluster_id);

extern struct user_vsensor_info *get_u_vsensor0_info(void);
extern struct user_vsensor_info *get_u_vsensor1_info(void);
extern struct user_vsensor_info *get_u_vsensor2_info(void);
extern struct user_vsensor_info *get_u_vsensor3_info(void);
extern struct user_vsensor_info *get_u_vsensor4_info(void);
extern struct user_vsensor_info *get_u_vsensor5_info(void);

#if IS_ENABLED(CONFIG_MTK_THERMAL_INTERFACE)
extern void __iomem *thermal_csram_base;
extern void __iomem *thermal_cputcm_base;
extern void __iomem *thermal_apu_mbox_base;
extern struct frs_info frs_data;
#else
void __iomem *thermal_csram_base;
void __iomem *thermal_cputcm_base;
void __iomem *thermal_apu_mbox_base;
struct frs_info frs_data;
#endif
#endif
