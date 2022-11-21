/* Copyright (c) 2018 The Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#ifndef __SMB5_CHARGER_H
#define __SMB5_CHARGER_H
#include <linux/types.h>
#include <linux/interrupt.h>
#include <linux/irqreturn.h>
#include <linux/regulator/driver.h>
#include <linux/regulator/consumer.h>
#include <linux/extcon.h>
#include <linux/alarmtimer.h>
#include "storm-watch.h"
/* HS60 add for SR-ZQL1695-01-405 by wangzikang at 2019/09/19 start */
#if !defined(HQ_FACTORY_BUILD)	//ss version
#if defined(CONFIG_BATT_CISD)
#include "batt-cisd.h"
#endif
#endif
/* HS60 add for SR-ZQL1695-01-405 by wangzikang at 2019/09/19 end */
#if defined(CONFIG_TYPEC)
#include <linux/usb/typec.h>
#endif
/* HS60 add for HS60-163 Set usb thermal by gaochao at 2019/07/30 start */
#include <linux/extcon.h>
#include <linux/qpnp/qpnp-adc.h>
#include <linux/of_gpio.h>
#include <linux/gpio.h>
/* HS60 add for HS60-163 Set usb thermal by gaochao at 2019/07/30 end */

/* HS60 add for HS60-293 Import main-source ATL battery profile by gaochao at 2019/07/31 start */
struct smbchg_info {
	int	cap;
	int	vbus;
	int	vbat;
	int	usb_c;
	int	bat_c;
	int	bat_t;
	int	icl_settled;
	int	sts;
	int	chg_type;
};
/* HS60 add for HS60-293 Import main-source ATL battery profile by gaochao at 2019/07/31 end */

enum print_reason {
	PR_INTERRUPT	= BIT(0),
	PR_REGISTER	= BIT(1),
	PR_MISC		= BIT(2),
	PR_PARALLEL	= BIT(3),
	PR_OTG		= BIT(4),
};

#define DEFAULT_VOTER			"DEFAULT_VOTER"
#define USER_VOTER			"USER_VOTER"
#define PD_VOTER			"PD_VOTER"
#define DCP_VOTER			"DCP_VOTER"
#define QC_VOTER			"QC_VOTER"
#define PL_USBIN_USBIN_VOTER		"PL_USBIN_USBIN_VOTER"
#define USB_PSY_VOTER			"USB_PSY_VOTER"
#define PL_TAPER_WORK_RUNNING_VOTER	"PL_TAPER_WORK_RUNNING_VOTER"
#define PL_QNOVO_VOTER			"PL_QNOVO_VOTER"
#define USBIN_V_VOTER			"USBIN_V_VOTER"
#define CHG_STATE_VOTER			"CHG_STATE_VOTER"
#define TYPEC_SRC_VOTER			"TYPEC_SRC_VOTER"
#define TAPER_END_VOTER			"TAPER_END_VOTER"
#define THERMAL_DAEMON_VOTER		"THERMAL_DAEMON_VOTER"
#define CC_DETACHED_VOTER		"CC_DETACHED_VOTER"
#define APSD_VOTER			"APSD_VOTER"
#define PD_DISALLOWED_INDIRECT_VOTER	"PD_DISALLOWED_INDIRECT_VOTER"
#define VBUS_CC_SHORT_VOTER		"VBUS_CC_SHORT_VOTER"
#define PD_INACTIVE_VOTER		"PD_INACTIVE_VOTER"
#define BOOST_BACK_VOTER		"BOOST_BACK_VOTER"
#define USBIN_USBIN_BOOST_VOTER		"USBIN_USBIN_BOOST_VOTER"
#define MICRO_USB_VOTER			"MICRO_USB_VOTER"
#define DEBUG_BOARD_VOTER		"DEBUG_BOARD_VOTER"
#define PD_SUSPEND_SUPPORTED_VOTER	"PD_SUSPEND_SUPPORTED_VOTER"
#define PL_DELAY_VOTER			"PL_DELAY_VOTER"
#define CTM_VOTER			"CTM_VOTER"
#define SW_QC3_VOTER			"SW_QC3_VOTER"
#define AICL_RERUN_VOTER		"AICL_RERUN_VOTER"
#define SW_ICL_MAX_VOTER		"SW_ICL_MAX_VOTER"
#define QNOVO_VOTER			"QNOVO_VOTER"
#define BATT_PROFILE_VOTER		"BATT_PROFILE_VOTER"
#define OTG_DELAY_VOTER			"OTG_DELAY_VOTER"
#define USBIN_I_VOTER			"USBIN_I_VOTER"
#define WEAK_CHARGER_VOTER		"WEAK_CHARGER_VOTER"
#define OTG_VOTER			"OTG_VOTER"
#define PL_FCC_LOW_VOTER		"PL_FCC_LOW_VOTER"
#define WBC_VOTER			"WBC_VOTER"
#define HW_LIMIT_VOTER			"HW_LIMIT_VOTER"
#define FORCE_RECHARGE_VOTER		"FORCE_RECHARGE_VOTER"
#define AICL_THRESHOLD_VOTER		"AICL_THRESHOLD_VOTER"
#define MOISTURE_VOTER			"MOISTURE_VOTER"
#define USBOV_DBC_VOTER			"USBOV_DBC_VOTER"
#define FCC_STEPPER_VOTER		"FCC_STEPPER_VOTER"
#define CHG_TERMINATION_VOTER		"CHG_TERMINATION_VOTER"
/*HS70 add for HS70-919 enable AFC function by qianyingdong at 2019/11/18 start*/
#if !defined(HQ_FACTORY_BUILD)	//ss version
#if defined(CONFIG_AFC)
#define SEC_BATTERY_AFC_VOTER		"SEC_BATTERY_AFC_VOTER"
#define SEC_BATTERY_DISABLE_HV_VOTER	"SEC_BATTERY_DISABLE_HV_VOTER"
#endif
#endif
/*HS70 add for HS70-919 enable AFC function by qianyingdong at 2019/11/18 end*/

#define BOOST_BACK_STORM_COUNT	3
#define WEAK_CHG_STORM_COUNT	8

#define VBAT_TO_VRAW_ADC(v)		div_u64((u64)v * 1000000UL, 194637UL)

#define SDP_100_MA			100000
#define SDP_CURRENT_UA			500000
#define CDP_CURRENT_UA			1500000
/* HS70 add for HS70-919 set DCP_ICL to 3000mA by qianyingdong at 2019/11/28 start */
#if defined(CONFIG_AFC)
/* HS70 add for P200417-04435  set DCP_ICL to 1800mA by wangzikang at 2020/04/23 start */
#define DCP_CURRENT_UA			1800000
/* HS70 add for P200417-04435  set DCP_ICL to 1800mA by wangzikang at 2020/04/23 end */
#else
#define DCP_CURRENT_UA			2000000
#endif
/* HS70 add for HS70-919 set DCP_ICL to 3000mA by qianyingdong at 2019/11/28 end */
/*HS70 add for HS70-919 enable AFC function by qianyingdong at 2019/11/18 start*/
#if !defined(HQ_FACTORY_BUILD)	//ss version
#if defined(CONFIG_AFC)
#define AFC_CURRENT_UA			1650000
#endif
#endif
/*HS70 add for HS70-919 enable AFC function by qianyingdong at 2019/11/18 end*/
/* HS70 add for P200417-04435 set QC2.0\3.0 to 9V1.67A by qianyingdong at 2020/04/24 start */
#define HVDCP_CURRENT_UA		1650000
/* HS70 add for P200417-04435 set QC2.0\3.0 to 9V1.67A by qianyingdong at 2020/04/24 end */
#define TYPEC_DEFAULT_CURRENT_UA	900000
#define TYPEC_MEDIUM_CURRENT_UA		1500000
#define TYPEC_HIGH_CURRENT_UA		3000000

enum smb_mode {
	PARALLEL_MASTER = 0,
	PARALLEL_SLAVE,
	NUM_MODES,
};

enum sink_src_mode {
	SINK_MODE,
	SRC_MODE,
	UNATTACHED_MODE,
};

/*HS70 add for HS70-919 import Handle QC2.0 charger collapse patch by qianyingdong at 2019/11/18 start*/
#ifdef CONFIG_ARCH_MSM8953
enum qc2_non_comp_voltage {
       QC2_COMPLIANT,
       QC2_NON_COMPLIANT_9V,
       QC2_NON_COMPLIANT_12V
};
#endif
/*HS70 add for HS70-919 import Handle QC2.0 charger collapse patch by qianyingdong at 2019/11/18 end*/

enum {
	BOOST_BACK_WA			= BIT(0),
	WEAK_ADAPTER_WA			= BIT(1),
	MOISTURE_PROTECTION_WA		= BIT(2),
	USBIN_OV_WA			= BIT(3),
	CHG_TERMINATION_WA		= BIT(4),
};

enum {
	RERUN_AICL			= BIT(0),
	RESTART_AICL			= BIT(1),
};

enum smb_irq_index {
	/* CHGR */
	CHGR_ERROR_IRQ = 0,
	CHG_STATE_CHANGE_IRQ,
	STEP_CHG_STATE_CHANGE_IRQ,
	STEP_CHG_SOC_UPDATE_FAIL_IRQ,
	STEP_CHG_SOC_UPDATE_REQ_IRQ,
	FG_FVCAL_QUALIFIED_IRQ,
	VPH_ALARM_IRQ,
	VPH_DROP_PRECHG_IRQ,
	/* DCDC */
	OTG_FAIL_IRQ,
	OTG_OC_DISABLE_SW_IRQ,
	OTG_OC_HICCUP_IRQ,
	BSM_ACTIVE_IRQ,
	HIGH_DUTY_CYCLE_IRQ,
	INPUT_CURRENT_LIMITING_IRQ,
	CONCURRENT_MODE_DISABLE_IRQ,
	SWITCHER_POWER_OK_IRQ,
	/* BATIF */
	BAT_TEMP_IRQ,
	ALL_CHNL_CONV_DONE_IRQ,
	BAT_OV_IRQ,
	BAT_LOW_IRQ,
	BAT_THERM_OR_ID_MISSING_IRQ,
	BAT_TERMINAL_MISSING_IRQ,
	BUCK_OC_IRQ,
	VPH_OV_IRQ,
	/* USB */
	USBIN_COLLAPSE_IRQ,
	USBIN_VASHDN_IRQ,
	USBIN_UV_IRQ,
	USBIN_OV_IRQ,
	USBIN_PLUGIN_IRQ,
	USBIN_REVI_CHANGE_IRQ,
	USBIN_SRC_CHANGE_IRQ,
	USBIN_ICL_CHANGE_IRQ,
	/* DC */
	DCIN_VASHDN_IRQ,
	DCIN_UV_IRQ,
	DCIN_OV_IRQ,
	DCIN_PLUGIN_IRQ,
	DCIN_REVI_IRQ,
	DCIN_PON_IRQ,
	DCIN_EN_IRQ,
	/* TYPEC */
	TYPEC_OR_RID_DETECTION_CHANGE_IRQ,
	TYPEC_VPD_DETECT_IRQ,
	TYPEC_CC_STATE_CHANGE_IRQ,
	TYPEC_VCONN_OC_IRQ,
	TYPEC_VBUS_CHANGE_IRQ,
	TYPEC_ATTACH_DETACH_IRQ,
	TYPEC_LEGACY_CABLE_DETECT_IRQ,
	TYPEC_TRY_SNK_SRC_DETECT_IRQ,
	/* MISC */
	WDOG_SNARL_IRQ,
	WDOG_BARK_IRQ,
	AICL_FAIL_IRQ,
	AICL_DONE_IRQ,
	SMB_EN_IRQ,
	IMP_TRIGGER_IRQ,
	TEMP_CHANGE_IRQ,
	TEMP_CHANGE_SMB_IRQ,
	/* FLASH */
	VREG_OK_IRQ,
	ILIM_S2_IRQ,
	ILIM_S1_IRQ,
	VOUT_DOWN_IRQ,
	VOUT_UP_IRQ,
	FLASH_STATE_CHANGE_IRQ,
	TORCH_REQ_IRQ,
	FLASH_EN_IRQ,
	/* END */
	SMB_IRQ_MAX,
};

enum float_options {
	FLOAT_DCP		= 1,
	FLOAT_SDP		= 2,
	DISABLE_CHARGING	= 3,
	SUSPEND_INPUT		= 4,
};

enum chg_term_config_src {
	ITERM_SRC_UNSPECIFIED,
	ITERM_SRC_ADC,
	ITERM_SRC_ANALOG
};

struct smb_irq_info {
	const char			*name;
	const irq_handler_t		handler;
	const bool			wake;
	const struct storm_watch	storm_data;
	struct smb_irq_data		*irq_data;
	int				irq;
};

/* HS60 add for HS60-163 Set usb thermal by gaochao at 2019/07/30 start */
enum {
	/* ZQL1695 */
	HQ_PCBA_ROW_EVB = 0x11,
	HQ_PCBA_ROW_EVT = 0x12,
	HQ_PCBA_ROW_DVT1 = 0x13,
	HQ_PCBA_ROW_DVT2 = 0x14,
	HQ_PCBA_ROW_PVT = 0x15,
	HQ_PCBA_ROW_MP1 = 0x16,
	HQ_PCBA_ROW_MP2 = 0x17,
	HQ_PCBA_ROW_MP3 = 0x18,
	HQ_PCBA_ROW_MP4 = 0x19,
	/* ZQL1695 */
	HQ_PCBA_PRC_IN_ID_EVB = 0x21,
	HQ_PCBA_PRC_IN_ID_EVT = 0x22,
	HQ_PCBA_PRC_IN_ID_DVT1 = 0x23,
	HQ_PCBA_PRC_IN_ID_DVT2 = 0x24,
	HQ_PCBA_PRC_IN_ID_PVT = 0x25,
	HQ_PCBA_PRC_IN_ID_MP1 = 0x26,
	HQ_PCBA_PRC_IN_ID_MP2 = 0x27,
	HQ_PCBA_PRC_IN_ID_MP3 = 0x28,
	HQ_PCBA_PRC_IN_ID_MP4 = 0x29,
	/* ZQL1695 */
	HQ_PCBA_LATAM_EVB = 0x31,
	HQ_PCBA_LATAM_EVT = 0x32,
	HQ_PCBA_LATAM_DVT1 = 0x33,
	HQ_PCBA_LATAM_DVT2 = 0x34,
	HQ_PCBA_LATAM_PVT = 0x35,
	HQ_PCBA_LATAM_MP1 = 0x36,
	HQ_PCBA_LATAM_MP2 = 0x37,
	HQ_PCBA_LATAM_MP3 = 0x38,
	HQ_PCBA_LATAM_MP4 = 0x39,

	/* ZQL1693 */
	HQ_PCBA_Verizen_EVB = 0x41,
	HQ_PCBA_Verizen_EVT = 0x42,
	HQ_PCBA_Verizen_DVT1 = 0x43,
	HQ_PCBA_Verizen_DVT2 = 0x44,
	HQ_PCBA_Verizen_PVT = 0x45,
	HQ_PCBA_Verizen_MP1 = 0x46,
	HQ_PCBA_Verizen_MP2 = 0x47,
	HQ_PCBA_Verizen_MP3 = 0x48,
	HQ_PCBA_Verizen_MP4 = 0x49,
	/* ZQL1693 */
	HQ_PCBA_AT_T_EVB = 0x51,
	HQ_PCBA_AT_T_EVT = 0x52,
	HQ_PCBA_AT_T_DVT1 = 0x53,
	HQ_PCBA_AT_T_DVT2 = 0x54,
	HQ_PCBA_AT_T_PVT = 0x55,
	HQ_PCBA_AT_T_MP1 = 0x56,
	HQ_PCBA_AT_T_MP2 = 0x57,
	HQ_PCBA_AT_T_MP3 = 0x58,
	HQ_PCBA_AT_T_MP4 = 0x59,
	/* ZQL1693 */
	HQ_PCBA_T_Mobile_EVB = 0x61,
	HQ_PCBA_T_Mobile_EVT = 0x62,
	HQ_PCBA_T_Mobile_DVT1 = 0x63,
	HQ_PCBA_T_Mobile_DVT2 = 0x64,
	HQ_PCBA_T_Mobile_PVT = 0x65,
	HQ_PCBA_T_Mobile_MP1 = 0x66,
	HQ_PCBA_T_Mobile_MP2 = 0x67,
	HQ_PCBA_T_Mobile_MP3 = 0x68,
	HQ_PCBA_T_Mobile_MP4 = 0x68,
	/* ZQL1693 */
	HQ_PCBA_TRF_EVB = 0x71,
	HQ_PCBA_TRF_EVT = 0x72,
	HQ_PCBA_TRF_DVT1 = 0x73,
	HQ_PCBA_TRF_DVT2 = 0x74,
	HQ_PCBA_TRF_PVT = 0x75,
	HQ_PCBA_TRF_MP1 = 0x76,
	HQ_PCBA_TRF_MP2 = 0x77,
	HQ_PCBA_TRF_MP3 = 0x78,
	HQ_PCBA_TRF_MP4 = 0x79,
	/* ZQL1693 */
	HQ_PCBA_Canada_EVB = 0x81,
	HQ_PCBA_Canada_EVT = 0x82,
	HQ_PCBA_Canada_DVT1 = 0x83,
	HQ_PCBA_Canada_DVT2 = 0x84,
	HQ_PCBA_Canada_PVT = 0x85,
	HQ_PCBA_Canada_MP1 = 0x86,
	HQ_PCBA_Canada_MP2 = 0x87,
	HQ_PCBA_Canada_MP3 = 0x88,
	HQ_PCBA_Canada_MP4 = 0x89,

	/* ZQL1690 */
	HQ_PCBA_Sprint_EVB = 0x91,
	HQ_PCBA_Sprint_EVT = 0x92,
	HQ_PCBA_Sprint_DVT1 = 0x93,
	HQ_PCBA_Sprint_DVT2 = 0x94,
	HQ_PCBA_Sprint_PVT = 0x95,
	HQ_PCBA_Sprint_MP1 = 0x96,
	HQ_PCBA_Sprint_MP2 = 0x97,
	HQ_PCBA_Sprint_MP3 = 0x98,
	HQ_PCBA_Sprint_MP4 = 0x99,
	/* HS60 code added by wangzikang for SR-ZQL1695-01-387 at 20190917 start*/
	/*ZQL1693*/
	HQ_PCBA_AIO_EVB = 0xA1,
	HQ_PCBA_AIO_EVT = 0xA2,
	HQ_PCBA_AIO_DVT1 = 0xA3,
	HQ_PCBA_AIO_DVT2 = 0xA4,
	HQ_PCBA_AIO_PVT = 0xA5,
	HQ_PCBA_AIO_MP1 = 0xA6,
	HQ_PCBA_AIO_MP2 = 0xA7,
	HQ_PCBA_AIO_MP3 = 0xA8,
	HQ_PCBA_AIO_MP4 = 0xA9,
	/* HS60 code added by wangzikang for SR-ZQL1695-01-387 at 20190917 end*/
};

#define CHG_ALERT_HOT_NTC_VOLTAFE  237284 		//70degC
#define CHG_ALERT_WARM_NTC_VOLTAGE 327297 	//60degC
enum {
	DRAW_VBUS_GPIO59_LOW,	/* disconnect VBUS to GND */
	DRAW_VBUS_GPIO59_HIGH,	/* connect VBUS to GND */
};

/* HS60 add for HS60-2198  Increase work delay time in case of deadlock resulting in hold on power-on logo by gaochao at 2019/10/02 start */
#define USB_TERMAL_START_DETECT_TIME		20000//2000
/* HS60 add for HS60-2198  Increase work delay time in case of deadlock resulting in hold on power-on logo by gaochao at 2019/10/02 end */
//#define USB_TERMAL_DETECT_TIMER		60000
/*HS60 add for ZQL169XFAC-45 by wangzikang at 2019/11/30 start*/
#define USB_TERMAL_DETECT_TIMER		30000
/*HS60 add for ZQL169XFAC-45 by wangzikang at 2019/11/30 end*/
/* HS60 add for HS60-163 Set usb thermal by gaochao at 2019/07/30 end */

/* HS70 add for HS70-135 Distinguish HS60 and HS70 charging by gaochao at 2019/10/08 start */
enum {
	/* ZQL1871 Global */
	HQ_PCBA_CHG_ROW_EVB = 0x11,
	HQ_PCBA_CHG_ROW_EVT = 0x12,
	HQ_PCBA_CHG_ROW_DVT1 = 0x13,
	HQ_PCBA_CHG_ROW_DVT2 = 0x14,
	HQ_PCBA_CHG_ROW_PVT = 0x15,
	HQ_PCBA_CHG_ROW_MP1 = 0x16,
	HQ_PCBA_CHG_ROW_MP2 = 0x17,
	HQ_PCBA_CHG_ROW_MP3 = 0x18,
	HQ_PCBA_CHG_ROW_MP4 = 0x19,

	/* ZQL1871 Global */
	HQ_PCBA_CHG_INDIA_EVB = 0x21,
	HQ_PCBA_CHG_INDIA_EVT = 0x22,
	HQ_PCBA_CHG_INDIA_DVT1 = 0x23,
	HQ_PCBA_CHG_INDIA_DVT2 = 0x24,
	HQ_PCBA_CHG_INDIA_PVT = 0x25,
	HQ_PCBA_CHG_INDIA_MP1 = 0x26,
	HQ_PCBA_CHG_INDIA_MP2 = 0x27,
	HQ_PCBA_CHG_INDIA_MP3 = 0x28,
	HQ_PCBA_CHG_INDIA_MP4 = 0x29,

	/* ZQL1871 Global */
	HQ_PCBA_CHG_LATAM_EVB = 0x31,
	HQ_PCBA_CHG_LATAM_EVT = 0x32,
	HQ_PCBA_CHG_LATAM_DVT1 = 0x33,
	HQ_PCBA_CHG_LATAM_DVT2 = 0x34,
	HQ_PCBA_CHG_LATAM_PVT = 0x35,
	HQ_PCBA_CHG_LATAM_MP1 = 0x36,
	HQ_PCBA_CHG_LATAM_MP2 = 0x37,
	HQ_PCBA_CHG_LATAM_MP3 = 0x38,
	HQ_PCBA_CHG_LATAM_MP4 = 0x39,

	HQ_PCBA_CHG_VZW_EVB = 0x41,
	HQ_PCBA_CHG_VZW_EVT = 0x42,
	HQ_PCBA_CHG_VZW_DVT1 = 0x43,
	HQ_PCBA_CHG_VZW_DVT2 = 0x44,
	HQ_PCBA_CHG_VZW_PVT = 0x45,
	HQ_PCBA_CHG_VZW_MP1 = 0x46,
	HQ_PCBA_CHG_VZW_MP2 = 0x47,
	HQ_PCBA_CHG_VZW_MP3 = 0x48,
	HQ_PCBA_CHG_VZW_MP4 = 0x49,

	HQ_PCBA_CHG_AT_T_EVB = 0x51,
	HQ_PCBA_CHG_AT_T_EVT = 0x52,
	HQ_PCBA_CHG_AT_T_DVT1 = 0x53,
	HQ_PCBA_CHG_AT_T_DVT2 = 0x54,
	HQ_PCBA_CHG_AT_T_PVT = 0x55,
	HQ_PCBA_CHG_AT_T_MP1 = 0x56,
	HQ_PCBA_CHG_AT_T_MP2 = 0x57,
	HQ_PCBA_CHG_AT_T_MP3 = 0x58,
	HQ_PCBA_CHG_AT_T_MP4 = 0x59,

	HQ_PCBA_CHG_T_Mobile_EVB = 0x61,
	HQ_PCBA_CHG_T_Mobile_EVT = 0x62,
	HQ_PCBA_CHG_T_Mobile_DVT1 = 0x63,
	HQ_PCBA_CHG_T_Mobile_DVT2 = 0x64,
	HQ_PCBA_CHG_T_Mobile_PVT = 0x65,
	HQ_PCBA_CHG_T_Mobile_MP1 = 0x66,
	HQ_PCBA_CHG_T_Mobile_MP2 = 0x67,
	HQ_PCBA_CHG_T_Mobile_MP3 = 0x68,
	HQ_PCBA_CHG_T_Mobile_MP4 = 0x69,

	HQ_PCBA_CHG_TFN_EVB = 0x71,
	HQ_PCBA_CHG_TFN_EVT = 0x72,
	HQ_PCBA_CHG_TFN_DVT1 = 0x73,
	HQ_PCBA_CHG_TFN_DVT2 = 0x74,
	HQ_PCBA_CHG_TFN_PVT = 0x75,
	HQ_PCBA_CHG_TFN_MP1 = 0x76,
	HQ_PCBA_CHG_TFN_MP2 = 0x77,
	HQ_PCBA_CHG_TFN_MP3 = 0x78,
	HQ_PCBA_CHG_TFN_MP4 = 0x79,

	HQ_PCBA_CHG_Canada_EVB = 0x81,
	HQ_PCBA_CHG_Canada_EVT = 0x82,
	HQ_PCBA_CHG_Canada_DVT1 = 0x83,
	HQ_PCBA_CHG_Canada_DVT2 = 0x84,
	HQ_PCBA_CHG_Canada_PVT = 0x85,
	HQ_PCBA_CHG_Canada_MP1 = 0x86,
	HQ_PCBA_CHG_Canada_MP2 = 0x87,
	HQ_PCBA_CHG_Canada_MP3 = 0x88,
	HQ_PCBA_CHG_Canada_MP4 = 0x89,

	HQ_PCBA_CHG_UNLOCK_EVB = 0x91,
	HQ_PCBA_CHG_UNLOCK_EVT = 0x92,
	HQ_PCBA_CHG_UNLOCK_DVT1 = 0x93,
	HQ_PCBA_CHG_UNLOCK_DVT2 = 0x94,
	HQ_PCBA_CHG_UNLOCK_PVT = 0x95,
	HQ_PCBA_CHG_UNLOCK_MP1 = 0x96,
	HQ_PCBA_CHG_UNLOCK_MP2 = 0x97,
	HQ_PCBA_CHG_UNLOCK_MP3 = 0x98,
	HQ_PCBA_CHG_UNLOCK_MP4 = 0x99,
};
/* HS70 add for HS70-135 Distinguish HS60 and HS70 charging by gaochao at 2019/10/08 end */

/* HS70 add for HS70-135 Distinguish HS60 and HS70 charging by gaochao at 2019/10/10 start */
enum {
	DETECT_SDM439_PLATFORM,
	DETECT_SDM450_PLATFORM,
	DETECT_SDM450_HS50,
};
/* HS70 add for HS70-135 Distinguish HS60 and HS70 charging by gaochao at 2019/10/10 end */

static const unsigned int smblib_extcon_cable[] = {
	EXTCON_USB,
	EXTCON_USB_HOST,
	EXTCON_CHG_USB_DCP,
	EXTCON_NONE,
};

/* HS60 add for SR-ZQL1695-01-315 Provide sysFS node named /sys/class/power_supply/battery/store_mode for retail APP by gaochao at 2019/08/18 start */
#if !defined(HQ_FACTORY_BUILD)	//ss version
//Huaqin added for SR-ZQL1871-01-248 & SR-ZQL1695-01-486 by wangzikang at 2019/10/18 start
#define STORE_MODE_VOTER		"STORE_MODE_VOTER"
//Huaqin added for SR-ZQL1871-01-248 & SR-ZQL1695-01-486 by wangzikang at 2019/10/18 start
/* HS60 add for HS60-2198  Increase work delay time in case of deadlock resulting in hold on power-on logo by gaochao at 2019/10/02 start */
#define RETAIL_APP_START_DETECT_TIME		6000//3000
/* HS60 add for HS60-2198  Increase work delay time in case of deadlock resulting in hold on power-on logo by gaochao at 2019/10/02 end */
#define RETAIL_APP_DETECT_TIMER		60000

#define STORE_MODE_ENABLE_SS	1
#define STORE_MODE_DISABLE_SS	0

#define SS_RETAIL_APP_DISCHG_THRESHOLD	70
#define SS_RETAIL_APP_CHG_THRESHOLD	60
/* HS60 add for SR-ZQL1695-01-495 by wangzikang at 2019/10/28 start */
#define SS_RETAIL_APP_DISCHG_THRESHOLD_VZW	35
#define SS_RETAIL_APP_CHG_THRESHOLD_VZW		  30
/* HS60 add for SR-ZQL1695-01-495 by wangzikang at 2019/10/28 end */

#endif
/* HS60 add for SR-ZQL1695-01-315 Provide sysFS node named /sys/class/power_supply/battery/store_mode for retail APP by gaochao at 2019/08/18 end */

/* HS70 add for HS70-565 Set ICL of float charger as 500mA by gaochao at 2019/12/20 start */
#if !defined(HQ_FACTORY_BUILD)	//ss version
#define BOOT_TO_DETECT_FLOAT_CHARGER_START_DETECT_TIME		20000
#define BOOT_TO_DETECT_INIT		0
#define BOOT_TO_DETECT_START		1
#define BOOT_TO_DETECT_SECOND		2
#define BOOT_TO_DETECT_STEP		1
#define BOOT_TO_DETECT_MAX		6
#endif
/* HS70 add for HS70-565 Set ICL of float charger as 500mA by gaochao at 2019/12/20 end */

/* HS60 add for HS60-811 Set float charger by gaochao at 2019/08/27 start */
#define FLOAT_CHARGER_START_DETECT_TIME		6000
/* HS60 add for HS60-811 Set float charger by gaochao at 2019/08/27 end */
/* HS60 add for ZQL1693-8 rerun apsd to identify charger type by gaochao at 2019/09/04 start */
#define RERUN_APSD_DETECT_TIME		3000
/* HS60 add for ZQL1693-8 rerun apsd to identify charger type by gaochao at 2019/09/04 end */

/* EXTCON_USB and EXTCON_USB_HOST are mutually exclusive */
static const u32 smblib_extcon_exclusive[] = {0x3, 0};

struct smb_regulator {
	struct regulator_dev	*rdev;
	struct regulator_desc	rdesc;
};

struct smb_irq_data {
	void			*parent_data;
	const char		*name;
	struct storm_watch	storm_data;
};

struct smb_chg_param {
	const char	*name;
	u16		reg;
	int		min_u;
	int		max_u;
	int		step_u;
	int		(*get_proc)(struct smb_chg_param *param,
				    u8 val_raw);
	int		(*set_proc)(struct smb_chg_param *param,
				    int val_u,
				    u8 *val_raw);
};

struct buck_boost_freq {
	int freq_khz;
	u8 val;
};

struct smb_chg_freq {
	unsigned int		freq_5V;
	unsigned int		freq_6V_8V;
	unsigned int		freq_9V;
	unsigned int		freq_12V;
	unsigned int		freq_removal;
	unsigned int		freq_below_otg_threshold;
	unsigned int		freq_above_otg_threshold;
};

struct smb_params {
	struct smb_chg_param	fcc;
	struct smb_chg_param	fv;
	struct smb_chg_param	usb_icl;
	struct smb_chg_param	icl_max_stat;
	struct smb_chg_param	icl_stat;
	struct smb_chg_param	otg_cl;
	struct smb_chg_param	jeita_cc_comp_hot;
	struct smb_chg_param	jeita_cc_comp_cold;
	struct smb_chg_param	freq_switcher;
	struct smb_chg_param	aicl_5v_threshold;
	struct smb_chg_param	aicl_cont_threshold;
};

struct parallel_params {
	struct power_supply	*psy;
};

struct smb_iio {
	struct iio_channel	*temp_chan;
	struct iio_channel	*temp_max_chan;
	struct iio_channel	*usbin_i_chan;
	struct iio_channel	*usbin_v_chan;
	struct iio_channel	*batt_i_chan;
	struct iio_channel	*connector_temp_chan;
	struct iio_channel	*connector_temp_thr1_chan;
	struct iio_channel	*connector_temp_thr2_chan;
	struct iio_channel	*connector_temp_thr3_chan;
};

/* Huaqin add for HS60-139 Set VREG_L8 as 2.9V by gaochao at 2019/07/18 start */
#define CUSTOM_VREG_L8_2P95
/* Huaqin add for HS60-139 Set VREG_L8 as 2.9V by gaochao at 2019/07/18 end */

struct smb_charger {
	struct device		*dev;
	char			*name;
	struct regmap		*regmap;
	struct smb_irq_info	*irq_info;
	struct smb_params	param;
	struct smb_iio		iio;
	int			*debug_mask;
	int			*pd_disabled;
	enum smb_mode		mode;
	struct smb_chg_freq	chg_freq;
	int			smb_version;
	int			otg_delay_ms;
	int			*weak_chg_icl_ua;
	struct qpnp_vadc_chip	*vadc_dev;
	bool			pd_not_supported;
	/* HS60 add for HS60-163 Set usb thermal by gaochao at 2019/07/30 start */
	struct qpnp_vadc_chip	*vadc_usb_alert;
	u32 vbus_control_gpio;
	/* HS60 add for HS60-163 Set usb thermal by gaochao at 2019/07/30 end */
	/* HS70 add for HS70-135 Distinguish HS60 and HS70 charging by gaochao at 2019/10/08 start */
	int vbus_control_gpio_enable;
	/* HS70 add for HS70-135 Distinguish HS60 and HS70 charging by gaochao at 2019/10/08 end */

	/* locks */
	struct mutex		lock;
	struct mutex		ps_change_lock;
	struct mutex		vadc_lock;

	/* power supplies */
	struct power_supply		*batt_psy;
	struct power_supply		*usb_psy;
	struct power_supply		*dc_psy;
	struct power_supply		*bms_psy;
	struct power_supply		*usb_main_psy;
	struct power_supply		*usb_port_psy;
	/* HS60 add for SR-ZQL1871-01-299 OTG psy by wangzikang at 2019/10/29 start */
	#if !defined(HQ_FACTORY_BUILD)	//ss version
	struct power_supply		*otg_psy;
	#endif
	/* HS60 add for SR-ZQL1871-01-299 OTG psy by wangzikang at 2019/10/29 end */
	enum power_supply_type		real_charger_type;

/* HS60 add for SR-ZQL1695-01-405 by wangzikang at 2019/09/19 start */
#if !defined(HQ_FACTORY_BUILD)	//ss version
#if defined(CONFIG_BATT_CISD)
	struct cisd cisd;
#endif
#endif
/* HS60 add for SR-ZQL1695-01-405 by wangzikang at 2019/09/19 end */
	/* notifiers */
	struct notifier_block	nb;

	/* parallel charging */
	struct parallel_params	pl;

	/* regulators */
	struct smb_regulator	*vbus_vreg;
	struct smb_regulator	*vconn_vreg;
	struct regulator	*dpdm_reg;

	/* Huaqin add for HS60-139 Set VREG_L8 as 2.9V by gaochao at 2019/07/18 start */
	#if defined(CUSTOM_VREG_L8_2P95)
	struct regulator	*VREG_L8_2P95;
	#endif
	/* Huaqin add for HS60-139 Set VREG_L8 as 2.9V by gaochao at 2019/07/18 end */

	/* votables */
	struct votable		*dc_suspend_votable;
	struct votable		*fcc_votable;
	struct votable		*fv_votable;
	struct votable		*usb_icl_votable;
	struct votable		*awake_votable;
	struct votable		*pl_disable_votable;
	struct votable		*chg_disable_votable;
	struct votable		*pl_enable_votable_indirect;
	struct votable		*usb_irq_enable_votable;

	/* work */
	struct work_struct	bms_update_work;
	struct work_struct	pl_update_work;
	struct work_struct	jeita_update_work;
	struct work_struct	moisture_protection_work;
	struct work_struct	chg_termination_work;
	struct delayed_work	ps_change_timeout_work;
	struct delayed_work	clear_hdc_work;
	struct delayed_work	icl_change_work;
	struct delayed_work	pl_enable_work;
#ifdef CONFIG_USB_NOTIFY_LAYER
	struct delayed_work	microb_otg_work;
#endif
	struct delayed_work	uusb_otg_work;
	struct delayed_work	bb_removal_work;
	struct delayed_work	usbov_dbc_work;
	/* HS60 add for HS60-163 Set usb thermal by gaochao at 2019/07/30 start */
	struct delayed_work	usb_thermal_status_change_work;
	/* HS60 add for HS60-163 Set usb thermal by gaochao at 2019/07/30 end */
	/* HS60 add for SR-ZQL1695-01-315 Provide sysFS node named /sys/class/power_supply/battery/store_mode for retail APP by gaochao at 2019/08/18 start */
	#if !defined(HQ_FACTORY_BUILD)	//ss version
	struct delayed_work	retail_app_status_change_work;
	#endif
	/* HS60 add for SR-ZQL1695-01-315 Provide sysFS node named /sys/class/power_supply/battery/store_mode for retail APP by gaochao at 2019/08/18 end */
	/* HS60 add for HS60-811 Set float charger by gaochao at 2019/08/27 start */
	struct delayed_work	float_charger_detect_work;
	/* HS60 add for HS60-811 Set float charger by gaochao at 2019/08/27 end */
	/* HS60 add for ZQL1693-8 rerun apsd to identify charger type by gaochao at 2019/09/04 start */
	struct delayed_work	rerun_apsd_work;
	/* HS60 add for ZQL1693-8 rerun apsd to identify charger type by gaochao at 2019/09/04 end */
	/*HS70 add for HS70-919 enable AFC function by qianyingdong at 2019/11/18 start*/
	#if !defined(HQ_FACTORY_BUILD)	//ss version
	#if defined(CONFIG_AFC)
	struct delayed_work    compliant_check_work;
	#endif
	#endif
	/*HS70 add for HS70-919 enable AFC function by qianyingdong at 2019/11/18 end*/

	/* alarm */
	struct alarm		moisture_protection_alarm;
	struct alarm		chg_termination_alarm;

	/* pd */
	int			voltage_min_uv;
	int			voltage_max_uv;
	int			pd_active;
	bool			pd_hard_reset;
	bool			pr_swap_in_progress;
	bool			early_usb_attach;
	bool			ok_to_pd;
	bool			typec_legacy;

	/* cached status */
	/* HS60 add for SR-ZQL1695-01-358 Provide sysFS node named xxx/battery/batt_slate_mode by gaochao at 2019/08/29 start */
	#if !defined(HQ_FACTORY_BUILD)	//ss version
	int			slate_mode;
	#endif
	/* HS60 add for SR-ZQL1695-01-358 Provide sysFS node named xxx/battery/batt_slate_mode by gaochao at 2019/08/29 end */
	/* HS60 add for SR-ZQL1695-01-315 Provide sysFS node named /sys/class/power_supply/battery/store_mode for retail APP by gaochao at 2019/08/18 start */
	#if !defined(HQ_FACTORY_BUILD)	//ss version
	int			store_mode_ss;
	#endif
	/* HS60 add for SR-ZQL1695-01-315 Provide sysFS node named /sys/class/power_supply/battery/store_mode for retail APP by gaochao at 2019/08/18 end */
	bool			system_suspend_supported;
	int			boost_threshold_ua;
	int			system_temp_level;
	int			thermal_levels;
	int			*thermal_mitigation;
	int			dcp_icl_ua;
	int			fake_capacity;
	/* HS70 add for HS70-135 Distinguish HS60 and HS70 charging by gaochao at 2019/10/10 start */
	int			typec_interface_protection_enable;
	int			typec_interface_protection_length;
	int			*typec_interface_protection;
	int			usb_connector_thermal_enable;
	int			usb_connector_thermal_length;
	int			*usb_connector_thermal;
	/* HS70 add for HS70-135 Distinguish HS60 and HS70 charging by gaochao at 2019/10/10 end */
	/* HS60 add for SR-ZQL1695-01-405 by wangzikang at 2019/09/19 start */
	#if !defined(HQ_FACTORY_BUILD)	//ss version
	//+ SS_charging, add battery_cycle node
	int			batt_cycle;
	int			battery_health;
	//- SS_charging, add battery_cycle node
	#endif
	/* HS60 add for SR-ZQL1695-01-405 by wangzikang at 2019/09/19 end */
	int			fake_batt_status;
	/* HS70 add for HS70-135 Distinguish HS60 and HS70 charging by gaochao at 2019/10/08 start */
	int			distinguish_sdm439_sdm450_others;
	/* HS70 add for HS70-135 Distinguish HS60 and HS70 charging by gaochao at 2019/10/08 end */
	bool			step_chg_enabled;
	bool			sw_jeita_enabled;
	bool			is_hdc;
	bool			chg_done;
	int			connector_type;
	bool			otg_en;
	bool			suspend_input_on_debug_batt;
	int			default_icl_ua;
	int			otg_cl_ua;
#ifdef CONFIG_USB_NOTIFY_LAYER
	bool		otg_enable;
#endif
	bool			uusb_apsd_rerun_done;
	int			fake_input_current_limited;
	int			typec_mode;
	int			usb_icl_change_irq_enabled;
	u32			jeita_status;
	u8			float_cfg;
	bool			use_extcon;
	bool			otg_present;
	int			hw_max_icl_ua;
	int			auto_recharge_soc;
	/* HS60 add for SR-ZQL1695-01-357 Import battery aging by gaochao at 2019/08/29 start */
	#if !defined(HQ_FACTORY_BUILD)	//ss version
	int			auto_recharge_vbat_mv;
	#endif
	/* HS60 add for SR-ZQL1695-01-357 Import battery aging by gaochao at 2019/08/29 end */
	bool			jeita_configured;
	enum sink_src_mode	sink_src_mode;
	bool			hw_die_temp_mitigation;
	bool			hw_connector_mitigation;
	int			connector_pull_up;
	int			aicl_5v_threshold_mv;
	int			default_aicl_5v_threshold_mv;
	int			aicl_cont_threshold_mv;
	int			default_aicl_cont_threshold_mv;
	bool			aicl_max_reached;
	bool			moisture_present;
	bool			moisture_protection_enabled;
	bool			fcc_stepper_enable;
	int			charge_full_cc;
	int			cc_soc_ref;
	int			last_cc_soc;
	/* HS70 add for HS70-565 Set ICL of float charger as 500mA by gaochao at 2019/12/20 start */
	#if !defined(HQ_FACTORY_BUILD)	//ss version
	int			boot_to_detect_charger;
	#endif
	/* HS70 add for HS70-565 Set ICL of float charger as 500mA by gaochao at 2019/12/20 end */

	/* workaround flag */
	u32			wa_flags;
	int			boost_current_ua;
	/*HS60 add for P200213-04659 Slow Charging Optimize by wangzikang at 2020/02/14 start*/
	#if !defined(HQ_FACTORY_BUILD)	//ss version
	int			slow_charging_count;
	#endif
	/*HS60 add for P200213-04659 Slow Charging Optimize by wangzikang at 2020/02/14 start*/
/*HS70 add for HS70-919 import Handle QC2.0 charger collapse patch by qianyingdong at 2019/11/18 start*/
#ifdef CONFIG_ARCH_MSM8953
	int			qc2_max_pulses;
	enum qc2_non_comp_voltage qc2_unsupported_voltage;
#endif
/*HS70 add for HS70-919 import Handle QC2.0 charger collapse patch by qianyingdong at 2019/11/18 end*/
	bool			dbc_usbov;

	/* extcon for VBUS / ID notification to USB for uUSB */
	struct extcon_dev	*extcon;

	/* battery profile */
	int			batt_profile_fcc_ua;
	int			batt_profile_fv_uv;

	int			usb_icl_delta_ua;
	int			pulse_cnt;

	int			die_health;

	/* flash */
	/* HS60 add for P191025-06620 Charging popup is coming while camera takes a photo with flash light by gaochao at 2019/11/21 start */
	#if !defined(HQ_FACTORY_BUILD)	//ss version
	int			previous_charger_status;
	#endif
	/* HS60 add for P191025-06620 Charging popup is coming while camera takes a photo with flash light by gaochao at 2019/11/21 end */
	u32			flash_derating_soc;
	u32			flash_disable_soc;
	u32			headroom_mode;
	bool			flash_init_done;
	bool			flash_active;
	/* HS60 add for P191025-06620 by wangzikang at 2019/12/04 start */
	#if !defined(HQ_FACTORY_BUILD)	//ss version
	bool			online_state_last;
	#endif
	/* HS60 add for P191025-06620 by wangzikang at 2019/12/04 end */
	/*HS70 add for HS70-919 enable AFC function by qianyingdong at 2019/11/18 start*/
	#if !defined(HQ_FACTORY_BUILD)	//ss version
	#if defined(CONFIG_AFC)
	int			afc_sts;
	bool			hv_disable;
	struct delayed_work		flash_active_work;
	#endif
	#endif
	/*HS70 add for HS70-919 enable AFC function by qianyingdong at 2019/11/18 end*/
	/* HS50 add for SR-QL3095-01-67 Import default charger profile by wenyaqi at 2020/08/03 start */
	bool			is_dcp;
	/* HS50 add for SR-QL3095-01-67 Import default charger profile by wenyaqi at 2020/08/03 end */
#if defined(CONFIG_TYPEC)
	struct typec_port 		*port;
	struct typec_partner 	*partner;
	struct usb_pd_identity 	partner_identity;
	struct typec_capability 	typec_cap;
	struct completion 		typec_reverse_completion;
	int		typec_power_role;
	int 	typec_data_role;
	int 	typec_power_role_flag;
	int 	typec_try_state_change;
	int 	pwr_opmode;
	int		requested_port_type;
	struct delayed_work		role_reversal_check;
	struct mutex			typec_lock;
#endif
};

int smblib_read(struct smb_charger *chg, u16 addr, u8 *val);
int smblib_masked_write(struct smb_charger *chg, u16 addr, u8 mask, u8 val);
int smblib_write(struct smb_charger *chg, u16 addr, u8 val);
int smblib_batch_write(struct smb_charger *chg, u16 addr, u8 *val, int count);
int smblib_batch_read(struct smb_charger *chg, u16 addr, u8 *val, int count);

int smblib_get_charge_param(struct smb_charger *chg,
			    struct smb_chg_param *param, int *val_u);
int smblib_get_usb_suspend(struct smb_charger *chg, int *suspend);
int smblib_get_aicl_cont_threshold(struct smb_chg_param *param, u8 val_raw);
int smblib_enable_charging(struct smb_charger *chg, bool enable);
int smblib_set_charge_param(struct smb_charger *chg,
			    struct smb_chg_param *param, int val_u);
int smblib_set_usb_suspend(struct smb_charger *chg, bool suspend);
int smblib_set_dc_suspend(struct smb_charger *chg, bool suspend);

int smblib_mapping_soc_from_field_value(struct smb_chg_param *param,
					     int val_u, u8 *val_raw);
int smblib_mapping_cc_delta_to_field_value(struct smb_chg_param *param,
					   u8 val_raw);
int smblib_mapping_cc_delta_from_field_value(struct smb_chg_param *param,
					     int val_u, u8 *val_raw);
int smblib_set_chg_freq(struct smb_chg_param *param,
				int val_u, u8 *val_raw);
int smblib_set_prop_boost_current(struct smb_charger *chg,
				const union power_supply_propval *val);
int smblib_set_aicl_cont_threshold(struct smb_chg_param *param,
				int val_u, u8 *val_raw);
int smblib_vbus_regulator_enable(struct regulator_dev *rdev);
int smblib_vbus_regulator_disable(struct regulator_dev *rdev);
int smblib_vbus_regulator_is_enabled(struct regulator_dev *rdev);

int smblib_vconn_regulator_enable(struct regulator_dev *rdev);
int smblib_vconn_regulator_disable(struct regulator_dev *rdev);
int smblib_vconn_regulator_is_enabled(struct regulator_dev *rdev);

irqreturn_t default_irq_handler(int irq, void *data);
irqreturn_t chg_state_change_irq_handler(int irq, void *data);
irqreturn_t batt_temp_changed_irq_handler(int irq, void *data);
irqreturn_t batt_psy_changed_irq_handler(int irq, void *data);
irqreturn_t usbin_uv_irq_handler(int irq, void *data);
irqreturn_t usb_plugin_irq_handler(int irq, void *data);
irqreturn_t usb_source_change_irq_handler(int irq, void *data);
irqreturn_t icl_change_irq_handler(int irq, void *data);
irqreturn_t typec_state_change_irq_handler(int irq, void *data);
irqreturn_t typec_attach_detach_irq_handler(int irq, void *data);
irqreturn_t dc_plugin_irq_handler(int irq, void *data);
irqreturn_t high_duty_cycle_irq_handler(int irq, void *data);
irqreturn_t switcher_power_ok_irq_handler(int irq, void *data);
irqreturn_t wdog_bark_irq_handler(int irq, void *data);
irqreturn_t typec_or_rid_detection_change_irq_handler(int irq, void *data);
irqreturn_t usbin_ov_irq_handler(int irq, void *data);

int smblib_get_prop_input_suspend(struct smb_charger *chg,
				union power_supply_propval *val);
int smblib_get_prop_batt_present(struct smb_charger *chg,
				union power_supply_propval *val);

/* HS60 add for SR-ZQL1695-01000000466 Provide sysFS node named /sys/class/power_supply/battery/online by gaochao at 2019/08/08 start */
#if !defined(HQ_FACTORY_BUILD)	//ss version
void smblib_get_prop_batt_online_samsung(struct smb_charger *chg,
				union power_supply_propval *val);
#endif
/* HS60 add for SR-ZQL1695-01000000466 Provide sysFS node named /sys/class/power_supply/battery/online by gaochao at 2019/08/08 end */

/* HS60 add for SR-ZQL1695-01000000455 Provide sysFS node named /sys/class/power_supply/battery/batt_current_event by gaochao at 2019/08/08 start */
#if !defined(HQ_FACTORY_BUILD)	//ss version
void smblib_get_prop_batt_batt_current_event_samsung(struct smb_charger *chg,
				union power_supply_propval *val);
#endif
/* HS60 add for SR-ZQL1695-01000000455 Provide sysFS node named /sys/class/power_supply/battery/batt_current_event by gaochao at 2019/08/08 end */

/* HS60 add for SR-ZQL1695-01000000460 Provide sysFS node named /sys/class/power_supply/battery/batt_misc_event by gaochao at 2019/08/11 start */
#if !defined(HQ_FACTORY_BUILD)	//ss version
void smblib_get_prop_batt_batt_misc_event_samsung(struct smb_charger *chg,
				union power_supply_propval *val);
#endif
/* HS60 add for SR-ZQL1695-01000000460 Provide sysFS node named /sys/class/power_supply/battery/batt_misc_event by gaochao at 2019/08/11 end */

/* HS60 add for SR-ZQL1695-01-315 Provide sysFS node named /sys/class/power_supply/battery/store_mode for retail APP by gaochao at 2019/08/18 start */
#if !defined(HQ_FACTORY_BUILD)	//ss version
void smblib_get_prop_batt_store_mode_samsung(struct smb_charger *chg,
				union power_supply_propval *val);

void smblib_set_prop_batt_store_mode_samsung(struct smb_charger *chg,
				const union power_supply_propval *val);
#endif
/* HS60 add for SR-ZQL1695-01-315 Provide sysFS node named /sys/class/power_supply/battery/store_mode for retail APP by gaochao at 2019/08/18 end */

int smblib_get_prop_batt_capacity(struct smb_charger *chg,
				union power_supply_propval *val);
int smblib_get_prop_batt_status(struct smb_charger *chg,
				union power_supply_propval *val);
int smblib_get_prop_batt_charge_type(struct smb_charger *chg,
				union power_supply_propval *val);
int smblib_get_prop_batt_charge_done(struct smb_charger *chg,
				union power_supply_propval *val);
int smblib_get_prop_batt_health(struct smb_charger *chg,
				union power_supply_propval *val);
int smblib_get_prop_system_temp_level(struct smb_charger *chg,
				union power_supply_propval *val);
int smblib_get_prop_system_temp_level_max(struct smb_charger *chg,
				union power_supply_propval *val);
int smblib_get_prop_input_current_limited(struct smb_charger *chg,
				union power_supply_propval *val);
int smblib_set_prop_input_suspend(struct smb_charger *chg,
				const union power_supply_propval *val);
int smblib_set_prop_batt_capacity(struct smb_charger *chg,
				const union power_supply_propval *val);
int smblib_set_prop_batt_status(struct smb_charger *chg,
				const union power_supply_propval *val);
int smblib_set_prop_system_temp_level(struct smb_charger *chg,
				const union power_supply_propval *val);
int smblib_set_prop_input_current_limited(struct smb_charger *chg,
				const union power_supply_propval *val);
int smblib_get_prop_dc_present(struct smb_charger *chg,
				union power_supply_propval *val);
int smblib_get_prop_dc_online(struct smb_charger *chg,
				union power_supply_propval *val);
int smblib_get_prop_dc_current_max(struct smb_charger *chg,
				union power_supply_propval *val);
int smblib_set_prop_dc_current_max(struct smb_charger *chg,
				const union power_supply_propval *val);
int smblib_get_prop_usb_present(struct smb_charger *chg,
				union power_supply_propval *val);
int smblib_get_prop_usb_online(struct smb_charger *chg,
				union power_supply_propval *val);
int smblib_get_prop_usb_suspend(struct smb_charger *chg,
				union power_supply_propval *val);
int smblib_get_prop_usb_voltage_max(struct smb_charger *chg,
				union power_supply_propval *val);
int smblib_get_prop_usb_voltage_max_design(struct smb_charger *chg,
				union power_supply_propval *val);
int smblib_get_prop_typec_cc_orientation(struct smb_charger *chg,
				union power_supply_propval *val);
int smblib_get_prop_typec_power_role(struct smb_charger *chg,
				union power_supply_propval *val);
int smblib_get_prop_input_current_settled(struct smb_charger *chg,
				union power_supply_propval *val);
int smblib_get_prop_input_voltage_settled(struct smb_charger *chg,
				union power_supply_propval *val);
int smblib_get_prop_pd_in_hard_reset(struct smb_charger *chg,
			       union power_supply_propval *val);
int smblib_get_pe_start(struct smb_charger *chg,
			       union power_supply_propval *val);
int smblib_get_prop_die_health(struct smb_charger *chg,
			       union power_supply_propval *val);
int smblib_set_prop_pd_current_max(struct smb_charger *chg,
				const union power_supply_propval *val);
int smblib_set_prop_sdp_current_max(struct smb_charger *chg,
				const union power_supply_propval *val);
int smblib_set_prop_pd_voltage_max(struct smb_charger *chg,
				const union power_supply_propval *val);
int smblib_set_prop_pd_voltage_min(struct smb_charger *chg,
				const union power_supply_propval *val);
int smblib_set_prop_typec_power_role(struct smb_charger *chg,
				const union power_supply_propval *val);
int smblib_set_prop_pd_active(struct smb_charger *chg,
				const union power_supply_propval *val);
int smblib_set_prop_pd_in_hard_reset(struct smb_charger *chg,
				const union power_supply_propval *val);
int smblib_set_prop_ship_mode(struct smb_charger *chg,
				const union power_supply_propval *val);
void smblib_suspend_on_debug_battery(struct smb_charger *chg);
int smblib_rerun_apsd_if_required(struct smb_charger *chg);
int smblib_get_prop_fcc_delta(struct smb_charger *chg,
				union power_supply_propval *val);
int smblib_dp_dm(struct smb_charger *chg, int val);
int smblib_disable_hw_jeita(struct smb_charger *chg, bool disable);
int smblib_run_aicl(struct smb_charger *chg, int type);
int smblib_set_icl_current(struct smb_charger *chg, int icl_ua);
int smblib_get_icl_current(struct smb_charger *chg, int *icl_ua);
int smblib_get_charge_current(struct smb_charger *chg, int *total_current_ua);
int smblib_get_prop_pr_swap_in_progress(struct smb_charger *chg,
				union power_supply_propval *val);
int smblib_set_prop_pr_swap_in_progress(struct smb_charger *chg,
				const union power_supply_propval *val);
int smblib_get_prop_from_bms(struct smb_charger *chg,
				enum power_supply_property psp,
				union power_supply_propval *val);
int smblib_stat_sw_override_cfg(struct smb_charger *chg, bool override);
int smblib_configure_wdog(struct smb_charger *chg, bool enable);
int smblib_force_vbus_voltage(struct smb_charger *chg, u8 val);
int smblib_configure_hvdcp_apsd(struct smb_charger *chg, bool enable);
int smblib_icl_override(struct smb_charger *chg, bool override);
/* HS60 add for SR-ZQL1695-01-357 Import battery aging by gaochao at 2019/08/29 start */
#if !defined(HQ_FACTORY_BUILD)	//ss version
int smblib_set_prop_rechg_vbat_thresh(struct smb_charger *chg,
				const union power_supply_propval *val);
#endif
/* HS60 add for SR-ZQL1695-01-357 Import battery aging by gaochao at 2019/08/29 end */

int smblib_init(struct smb_charger *chg);
int smblib_deinit(struct smb_charger *chg);
/*HS70 add for HS70-919 enable AFC function by qianyingdong at 2019/11/18 start*/
#if !defined(HQ_FACTORY_BUILD)	//ss version
#if defined(CONFIG_AFC)
int is_afc_result(struct smb_charger *chg,int result);
/* HS50 add for P201023-04559 re-connect vbus when shutdown with afc TA by wenyaqi at 2020/10/28 start */
void ss_vbus_control_gpio_set(struct smb_charger *chg, int set_gpio_val);
/* HS50 add for P201023-04559 re-connect vbus when shutdown with afc TA by wenyaqi at 2020/10/28 end */
#endif
#endif
/*HS70 add for HS70-919 enable AFC function by qianyingdong at 2019/11/18 end*/
#endif /* __SMB5_CHARGER_H */
