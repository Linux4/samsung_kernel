/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2021 MediaTek Inc.
*/

#ifndef __MTK_CHARGER_INIT_H__
#define __MTK_CHARGER_INIT_H__

/* hs14 code for SR-AL6528A-01-323|AL6528ADEU-580 by gaozhengwei at 2022/10/09 start */
#define BATTERY_CV 4400000
#define CV_HIGH_THRESHOLD 4500000
#define V_CHARGER_MAX 6300000 /* 6.3 V */
#define HV_CHARGER_MAX 10400000 /* 10.4 V */
#define V_CHARGER_DROP 700000 /* 0.7 V */
/* hs14 code for SR-AL6528A-01-323|AL6528ADEU-580 by gaozhengwei at 2022/10/09 end */
#define V_CHARGER_MIN 4600000 /* 4.6 V */
/* hs14 code for AL6528A-604 by gaozhengwei at 2022/11/07 start */
#define HV_CHARGER_MIN 8000000 /* 8.0 V */
/* hs14 code for AL6528A-604 by gaozhengwei at 2022/11/07 end */

#define USB_CHARGER_CURRENT_SUSPEND		0 /* def CONFIG_USB_IF */
#define USB_CHARGER_CURRENT_UNCONFIGURED	70000 /* 70mA */
#define USB_CHARGER_CURRENT_CONFIGURED		500000 /* 500mA */
#define USB_CHARGER_CURRENT			500000 /* 500mA */
/* hs14 code for SR-AL6528A-01-323 by gaozhengwei at 2022/09/22 start */
#define AC_CHARGER_CURRENT			2000000
#define AC_CHARGER_INPUT_CURRENT		1550000
/* hs14 code for SR-AL6528A-01-323 by gaozhengwei at 2022/09/22 end */
#define NON_STD_AC_CHARGER_CURRENT		500000
#define CHARGING_HOST_CHARGER_CURRENT		650000
#define APPLE_1_0A_CHARGER_CURRENT		650000
#define APPLE_2_1A_CHARGER_CURRENT		800000
#define TA_AC_CHARGING_CURRENT	3000000
#define USB_UNLIMITED_CURRENT	2000000
/* hs14 code for SR-AL6528A-01-322 by wenyaqi at 2022/09/20 start */
#define PD_CHARGER_CURRENT 2700000
#define PD_INPUT_CURRENT 1650000
#define PD_VOLTAGE_THR 8000
/* hs14 code for SR-AL6528A-01-322 by wenyaqi at 2022/09/20 end */

/* dynamic mivr */
#define V_CHARGER_MIN_1 4400000 /* 4.4 V */
#define V_CHARGER_MIN_2 4200000 /* 4.2 V */
#define MAX_DMIVR_CHARGER_CURRENT 1400000 /* 1.4 A */

/* hs14 code for SR-AL6528A-01-323 by gaozhengwei at 2022/09/22 start */
/* sw jeita */
#define JEITA_TEMP_ABOVE_T4_CV  4200000
#define JEITA_TEMP_T3_TO_T4_CV  4200000
#define JEITA_TEMP_T2_TO_T3_CV  4400000
#define JEITA_TEMP_T1_TO_T2_CV  4400000
#define JEITA_TEMP_T0_TO_T1_CV  4400000
#define JEITA_TEMP_BELOW_T0_CV  4400000
#define JEITA_TEMP_ABOVE_T4_CUR  0
#define JEITA_TEMP_T3_TO_T4_CUR  1750000
/* hs14 code for SR-AL6528A-01-322 by wenyaqi at 2022/09/23 start */
#define JEITA_TEMP_T2_TO_T3_CUR  2700000
/* hs14 code for SR-AL6528A-01-322 by wenyaqi at 2022/09/23 end */
#define JEITA_TEMP_T1_TO_T2_CUR  1500000
#define JEITA_TEMP_T0_TO_T1_CUR  500000
#define JEITA_TEMP_BELOW_T0_CUR  0
#define TEMP_T4_THRES  50
#define TEMP_T4_THRES_MINUS_X_DEGREE  48
#define TEMP_T3_THRES  45
#define TEMP_T3_THRES_MINUS_X_DEGREE  43
#define TEMP_T2_THRES  12
#define TEMP_T2_THRES_PLUS_X_DEGREE  14
#define TEMP_T1_THRES  5
#define TEMP_T1_THRES_PLUS_X_DEGREE  7
#define TEMP_T0_THRES  0
#define TEMP_T0_THRES_PLUS_X_DEGREE  2
#define TEMP_NEG_10_THRES  0
/* hs14 code for SR-AL6528A-01-323 by gaozhengwei at 2022/09/22 end */
/*A06 code for AL7160A-62 | AL7160A-67 by jiashixian at 2024/04/15 start*/
#if defined(CONFIG_HQ_PROJECT_O8)
// cp_jeita_cc
#define JEITA_TEMP_ABOV_T4_CP_CC  1000000
#define JEITA_TEMP_T3_TO_T4_CP_CC 3800000
#define JEITA_TEMP_T2_TO_T3_CP_CC 6000000
#define JEITA_TEMP_T1_TO_T2_CP_CC 1300000
#define JEITA_TEMP_T0_TO_T1_CP_CC 1000000
#define JEITA_TEMP_BELOW_T0_CP_CC 1000000

// lcm off param
/*A06 code for AL7160A-837 by jiashixian at 2024/05/09 start*/
/*A06 code for AL7160A-1488 by jiashixian at 2024/05/23 start*/
#define AP_TEMP_ABOVE_T4_CP_CC    2500000
/*A06 code for AL7160A-2611 by jiashixian at 2024/06/05 start*/
#define AP_TEMP_T3_TO_T4_CP_CC    3400000
/*A06 code for AL7160A-2611 by jiashixian at 2024/06/05 end*/
#define AP_TEMP_T2_TO_T3_CP_CC    3500000
/*A06 code for AL7160A-3634 by jiashixian at 2024/06/26 start*/
#define AP_TEMP_T1_TO_T2_CP_CC    3900000
/*A06 code for AL7160A-3634 by jiashixian at 2024/06/26 end*/
#define AP_TEMP_BELOW_T1_CP_CC    6000000

// lcm on param
/*A06 code for AL7160A-1526 by jiashixian at 2024/05/30 start*/
/*A06 code for AL7160A-2611 by jiashixian at 2024/06/05 start*/
#ifdef HQ_FACTORY_BUILD
#define AP_TEMP_T1_LCMON_CP_CC  3500000
#define AP_TEMP_T2_LCMON_CP_CC  2500000
#define AP_TEMP_T3_LCMON_CP_CC  750000
#else
/*A06 code for AL7160A-3634 by jiashixian at 2024/06/26 start*/
#define AP_TEMP_T1_LCMON_CP_CC  700000
#define AP_TEMP_T2_LCMON_CP_CC  600000
#define AP_TEMP_T3_LCMON_CP_CC  500000
/*A06 code for AL7160A-3634 by jiashixian at 2024/06/26 end*/
#endif
/*A06 code for AL7160A-2611 by jiashixian at 2024/06/05 end*/
/*A06 code for AL7160A-1526 by jiashixian at 2024/05/30 end*/
#define AP_TEMP_NORMAL_LCMON_CP_CC 6000000
/*A06 code for AL7160A-837 by jiashixian at 2024/05/09 end*/

// lcm off temp thresh
#define AP_TEMP_T4_CP_THRES  44
#define AP_TEMP_T3_CP_THRES  42
/*A06 code for AL7160A-2611 by jiashixian at 2024/06/05 start*/
#define AP_TEMP_T2_CP_THRES  40
#define AP_TEMP_T1_CP_THRES  39
/*A06 code for AL7160A-2611 by jiashixian at 2024/06/05 end*/
// lcm on temp thresh
/*A06 code for AL7160A-83 by jiashixian at 2024/04/19 start*/
/*A06 code for AL7160A-1526 by jiashixian at 2024/05/30 start*/
/*A06 code for AL7160A-2611 by jiashixian at 2024/06/05 start*/
#ifdef HQ_FACTORY_BUILD
#define AP_TEMP_T1_CP_THRES_LCMON 41
#define AP_TEMP_T2_CP_THRES_LCMON 43
#define AP_TEMP_T3_CP_THRES_LCMON 45
#else
/*A06 code for AL7160A-3634 by jiashixian at 2024/06/26 start*/
#define AP_TEMP_T1_CP_THRES_LCMON 37
/*A06 code for AL7160A-3634 by jiashixian at 2024/06/26 end*/
#define AP_TEMP_T2_CP_THRES_LCMON 43
#define AP_TEMP_T3_CP_THRES_LCMON 44
#endif
/*A06 code for AL7160A-2611 by jiashixian at 2024/06/05 end*/
/*A06 code for AL7160A-1526 by jiashixian at 2024/05/30 end*/
/*A06 code for AL7160A-83 by jiashixian at 2024/04/19 end*/
/*A06 code for AL7160A-1488 by jiashixian at 2024/05/23 end*/
#endif
/*A06 code for AL7160A-62 | AL7160A-67 by jiashixian at 2024/04/15 end*/
/* Battery Temperature Protection */
#define MIN_CHARGE_TEMP  0
#define MIN_CHARGE_TEMP_PLUS_X_DEGREE	6
#define MAX_CHARGE_TEMP  50
#define MAX_CHARGE_TEMP_MINUS_X_DEGREE	47

/* pe */
#define PE_ICHG_LEAVE_THRESHOLD 1000000 /* uA */
#define TA_AC_12V_INPUT_CURRENT 3200000
#define TA_AC_9V_INPUT_CURRENT	3200000
#define TA_AC_7V_INPUT_CURRENT	3200000
#define TA_9V_SUPPORT
#define TA_12V_SUPPORT

/* pe2.0 */
#define PE20_ICHG_LEAVE_THRESHOLD 1000000 /* uA */
#define TA_START_BATTERY_SOC	0
#define TA_STOP_BATTERY_SOC	85

/* dual charger */
#define TA_AC_MASTER_CHARGING_CURRENT 1500000
#define TA_AC_SLAVE_CHARGING_CURRENT 1500000
#define SLAVE_MIVR_DIFF 100000

/* slave charger */
#define CHG2_EFF 90

/* cable measurement impedance */
#define CABLE_IMP_THRESHOLD 699
#define VBAT_CABLE_IMP_THRESHOLD 3900000 /* uV */

/* bif */
#define BIF_THRESHOLD1 4250000	/* UV */
#define BIF_THRESHOLD2 4300000	/* UV */
#define BIF_CV_UNDER_THRESHOLD2 4450000	/* UV */
#define BIF_CV BATTERY_CV /* UV */

#define R_SENSE 56 /* mohm */

#define MAX_CHARGING_TIME (12 * 60 * 60) /* 12 hours */

#define DEFAULT_BC12_CHARGER 0 /* MAIN_CHARGER */

/* battery warning */
#define BATTERY_NOTIFY_CASE_0001_VCHARGER
#define BATTERY_NOTIFY_CASE_0002_VBATTEMP

/* pe4 */
#define PE40_MAX_VBUS 11000
#define PE40_MAX_IBUS 3000
#define HIGH_TEMP_TO_LEAVE_PE40 46
#define HIGH_TEMP_TO_ENTER_PE40 39
#define LOW_TEMP_TO_LEAVE_PE40 10
#define LOW_TEMP_TO_ENTER_PE40 16

/* pd */
#define PD_VBUS_UPPER_BOUND 10000000	/* uv */
#define PD_VBUS_LOW_BOUND 5000000	/* uv */
#define PD_ICHG_LEAVE_THRESHOLD 1000000 /* uA */
#define PD_STOP_BATTERY_SOC 80

#define VSYS_WATT 5000000
#define IBUS_ERR 14

#define SC_BATTERY_SIZE 3000
#define SC_CV_TIME 3600
#define SC_CURRENT_LIMIT 2000

#endif /*__MTK_CHARGER_INIT_H__*/

/* hs14 code for SR-AL6528A-01-336 by shanxinkai at 2022/09/15 start */
/*D85 setting */
#ifdef HQ_D85_BUILD
#define D85_BATTERY_CV 4000000
#define D85_JEITA_TEMP_CV 4000000
#endif
/* hs14 code for SR-AL6528A-01-336 by shanxinkai at 2022/09/15 end */

/* A06 code for SR-AL7160A-01-358 by zhangziyi at 20240409 start */
#if defined(CONFIG_HQ_PROJECT_O8)
#undef BATTERY_CV
#define BATTERY_CV                   4450000

#undef JEITA_TEMP_ABOVE_T4_CV
#define JEITA_TEMP_ABOVE_T4_CV       4230000

#undef JEITA_TEMP_T3_TO_T4_CV
#define JEITA_TEMP_T3_TO_T4_CV       4230000

#undef JEITA_TEMP_T2_TO_T3_CV
#define JEITA_TEMP_T2_TO_T3_CV       4450000

#undef JEITA_TEMP_T1_TO_T2_CV
#define JEITA_TEMP_T1_TO_T2_CV       4450000

#undef JEITA_TEMP_T0_TO_T1_CV
#define JEITA_TEMP_T0_TO_T1_CV       4450000

#undef JEITA_TEMP_BELOW_T0_CV
#define  JEITA_TEMP_BELOW_T0_CV      4450000

#undef JEITA_TEMP_T3_TO_T4_CUR
#define JEITA_TEMP_T3_TO_T4_CUR      3000000

#undef JEITA_TEMP_T2_TO_T3_CUR
#define JEITA_TEMP_T2_TO_T3_CUR      3000000

#undef JEITA_TEMP_T1_TO_T2_CUR
#define JEITA_TEMP_T1_TO_T2_CUR      1320000

#undef JEITA_TEMP_T0_TO_T1_CUR
#define JEITA_TEMP_T0_TO_T1_CUR      420000

#undef TEMP_T4_THRES
#define TEMP_T4_THRES                52

#undef TEMP_T4_THRES_MINUS_X_DEGREE
#define TEMP_T4_THRES_MINUS_X_DEGREE 50

#undef TEMP_T3_THRES
#define TEMP_T3_THRES                42

#undef TEMP_T3_THRES_MINUS_X_DEGREE
#define TEMP_T3_THRES_MINUS_X_DEGREE 40

#undef TEMP_T2_THRES
#define TEMP_T2_THRES                18

#undef TEMP_T2_THRES_PLUS_X_DEGREE
#define TEMP_T2_THRES_PLUS_X_DEGREE  20

#undef TEMP_T1_THRES
#define TEMP_T1_THRES                8

#undef TEMP_T1_THRES_PLUS_X_DEGREE
#define TEMP_T1_THRES_PLUS_X_DEGREE  10
#endif //CONFIG_HQ_PROJECT_O8
/* A06 code for SR-AL7160A-01-358 by zhangziyi at 20240409 end */
