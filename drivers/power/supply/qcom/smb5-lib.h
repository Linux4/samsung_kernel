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
#if defined(CONFIG_BATT_CISD)
#include "batt-cisd.h"
#endif
#if defined(CONFIG_TYPEC)
#include <linux/usb/typec.h>
#endif

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
//+Bug 600732,xushengjuan.wt,modify,20201118,S86117,charger bring up
//Bug 427130 caijiaqi.wt,ADD,20190121,P81081 charger bring up, add debug log
#define CHG_INSERT_VOTER			"CHG_INSERT_VOTER"
#define CHG_TERMINATION_VOTER		"CHG_TERMINATION_VOTER"
//+Bug 455539 gudi.wt, MODIFY, 20190806,s86119 control charging current in lcd on or off
#ifdef CONFIG_ARCH_MSM8953
#define BATT_TEMP_VOTER                "BATT_TEMP_VOTER"
#endif
//-Bug 455539 gudi.wt, MODIFY, 20190806,s86119 control charging current in lcd on or off
#if defined(CONFIG_AFC)
#define SEC_BATTERY_AFC_VOTER		"SEC_BATTERY_AFC_VOTER"
#define SEC_BATTERY_DISABLE_HV_VOTER	"SEC_BATTERY_DISABLE_HV_VOTER"
#endif

//+Bug 450044 caijiaqi.wt,MODIFIY,20190603,P81081 modifiy ato version control capacity max is 80%
#ifdef WT_COMPILE_FACTORY_VERSION
#define FACTORY_VOTER   "STORE_FACTOR_VOTER"
#endif
//-Bug 450044 caijiaqi.wt,MODIFIY,20190603,P81081 modifiy ato version control capacity max is 80%

#ifdef CONFIG_ARCH_MSM8953
//+Bug 465111 gudi.wtMODIFY,20190814,s86119 for accurate temp to limit current
#define POWER_OFF_FCC_VOTER "POWER_OFF_FCC_VOTER"
//+Bug 465111 gudi.wtMODIFY,20190814,s86119 for accurate temp to limit current
#endif
//-Bug 600732,xushengjuan.wt,modify,20201118,S86117,charger bring up
#define BOOST_BACK_STORM_COUNT	3
#define WEAK_CHG_STORM_COUNT	8

#define VBAT_TO_VRAW_ADC(v)		div_u64((u64)v * 1000000UL, 194637UL)

#define SDP_100_MA			100000
#define SDP_CURRENT_UA			500000
//+Bug 600732,xushengjuan.wt,modify,20201118,S86117,charger bring up
//+Bug 427130 caijiaqi.wt,MODIFIY,20190121,P81081 charger bring up
#define CDP_CURRENT_UA			900000
#define FLOAT_CURRENT_UA		900000
//+Bug 439935 gudi.wt,MODIFIY,20190628,S86119AA1 charge current set 2.0A max ,hardware support for bring up
#ifdef CONFIG_ARCH_MSM8953
//+Bug 439935 gudi.wt,MODIFIY,20190719,S86119 DCP_CURRENT_UA set 1.8A max for DCP from customer requirements
#define DCP_CURRENT_UA                  1800000
//-Bug 439935 gudi.wt,MODIFIY,20190719,S86119 DCP_CURRENT_UA set 1.8A max for DCP from customer requirements
#else

#define DCP_CURRENT_UA			1500000
#endif
//-Bug 439935 gudi.wt,MODIFIY,20190628,S86119AA1 charge current set 2.0A max ,hardware support for bring up
#if defined(CONFIG_AFC)
#define AFC_CURRENT_UA			1650000
#endif
//+Bug 427130 caijiaqi.wt,MODIFIY,20190121,P81081 charger bring up
//Bug 455539 caijiaqi.wt,MODIFIY,20190705,S86119 Modifiy open hvdcp for QC20
//Bug 455539 caijiaqi.wt,MODIFIY,20190705,S86119 Modifiy open hvdcp for QC20
#define HVDCP_CURRENT_UA                1650000
#define TYPEC_DEFAULT_CURRENT_UA	900000
#define TYPEC_MEDIUM_CURRENT_UA		1500000
//+Bug 455539 gudi.wt,MODIFIY,20190719,S86119 Modifiy open hvdcp for pd charger
#ifdef CONFIG_ARCH_MSM8953
#define TYPEC_HIGH_CURRENT_UA           1800000
#else
#define TYPEC_HIGH_CURRENT_UA           3000000
#endif
//-Bug 455539 gudi.wt,MODIFIY,20190719,S86119 Modifiy open hvdcp for pd charger
//+Bug 437351 caijiaqi.wt,MODIFIY,20190409,P81081 add battery online node
#define	BATTERY_ONLINE_INCOMPATIBLE_CHARGER	0
#define	BATTERY_ONLINE_NONE	1
#define	BATTERY_ONLINE_TA	3
#define	BATTERY_ONLINE_USB	4
#define	BATTERY_ONLINE_WIRELESS_CHARGER	10
#define	BATTERY_ONLINE_POGO	23
//Bug 472563 caijiaqi.wt,MODIFIY,20190823,S86119 add battery online node about PD
#define BATTERY_ONLINE_PD_DCP			44
#define	BATTERY_ONLINE_FAST_WIRELESS_CHARGER	100
//-Bug 437351 caijiaqi.wt,MODIFIY,20190409,P81081 add battery online node

//+Bug 437373 caijiaqi.wt, ADD,20190410,P81081 add battery node for customer
#define SEC_BAT_CURRENT_EVENT_NONE	0x00000
#define SEC_BAT_CURRENT_EVENT_AFC	0x00001
#define SEC_BAT_CURRENT_EVENT_CHARGE_DISABLE		0x00002
#define SEC_BAT_CURRENT_EVENT_SKIP_HEATING_CONTROL	0x00004
#define SEC_BAT_CURRENT_EVENT_LOW_TEMP_SWELLING		0x00010
#define SEC_BAT_CURRENT_EVENT_HIGH_TEMP_SWELLING	0x00020
#define SEC_BAT_CURRENT_EVENT_USB_100MA	0x00040
#define SEC_BAT_CURRENT_EVENT_SLATE	0x00800

#define BATT_MISC_EVENT_UNDEFINED_RANGE_TYPE	0x00000001
#define BATT_MISC_EVENT_TIMEOUT_OPEN_TYPE	0x00000004
#define BATT_MISC_EVENT_HICCUP_TYPE		0x00000020
//-Bug 437373 caijiaqi.wt, ADD,20190410,P81081 add battery node for customer
//-Bug 600732,xushengjuan.wt,modify,20201118,S86117,charger bring up
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

//+Bug 600732,xushengjuan.wt,modify,20201118,S86117,(Patch)Handle QC2.0 charger collapse.
#ifdef CONFIG_ARCH_MSM8953
enum qc2_non_comp_voltage {
	QC2_COMPLIANT,
	QC2_NON_COMPLIANT_9V,
	QC2_NON_COMPLIANT_12V
};
#endif
//-Bug 600732,xushengjuan.wt,modify,20201118,S86117,(Patch)Handle QC2.0 charger collapse.

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

static const unsigned int smblib_extcon_cable[] = {
	EXTCON_USB,
	EXTCON_USB_HOST,
	EXTCON_NONE,
};

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

//+Bug 603959, liangxiaoqin.wt, 20201216, add, add usb_notify node,start
#ifdef CONFIG_USB_NOTIFIER
	bool			otg_block;
	struct notifier_block	otg_nb;
#endif
//-Bug 603959, liangxiaoqin.wt, 20201216, add, add usb_notify node,end

	/* locks */
	struct mutex		lock;
	struct mutex		ps_change_lock;
	struct mutex		vadc_lock;

	/* power supplies */
	struct power_supply		*batt_psy;
	struct power_supply		*usb_psy;
	struct power_supply		*dc_psy;
	struct power_supply		*bms_psy;
	//Bug 600732,xushengjuan.wt,modify,20201118,S86117,add otg node
	struct power_supply		*otg_psy;
	struct power_supply		*usb_main_psy;
	struct power_supply		*usb_port_psy;
	enum power_supply_type		real_charger_type;

#if defined(CONFIG_BATT_CISD)
	struct cisd cisd;
#endif

	/* notifiers */
	struct notifier_block	nb;

	/* parallel charging */
	struct parallel_params	pl;

	/* regulators */
	struct smb_regulator	*vbus_vreg;
	struct smb_regulator	*vconn_vreg;
	struct regulator	*dpdm_reg;

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
	struct delayed_work	uusb_otg_work;
	struct delayed_work	bb_removal_work;
	struct delayed_work	usbov_dbc_work;
	//+Bug 600732,xushengjuan.wt,modify,20201118,S86117,charger bring up
	//Bug 427130 caijiaqi.wt,ADD,20190121,P81081 charger bring up, add debug log
	struct delayed_work	period_update_work;
	//Bug 437373 caijiaqi.wt, ADD,20190410,P81081 add battery node for customer
	struct delayed_work	usb_update_work;
	//-Bug 600732,xushengjuan.wt,modify,20201118,S86117,charger bring up
#if defined(CONFIG_AFC)
	struct delayed_work    compliant_check_work;
#endif

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
	bool			system_suspend_supported;
	int			boost_threshold_ua;
	int			system_temp_level;
	int			thermal_levels;
	int			*thermal_mitigation;
	int			dcp_icl_ua;
	int			fake_capacity;
//+Bug 600732,xushengjuan.wt,modify,20201118,S86117,charger bring up
	//Bug 437318 caijiaqi.wt, ADD,20190409,P81081 add store_mode node to control capacity
	int			store_mode;
	//+Bug 437373 caijiaqi.wt, ADD,20190410,P81081 add battery node for customer
	int			slate_mode;
	int			usb_suspend_mode;
	//-Bug 437373 caijiaqi.wt, ADD,20190410,P81081 add battery node for customer
//+ SS_charging, add battery_cycle node
	int			batt_cycle;
//- SS_charging, add battery_cycle node
	int			fake_batt_status;
//+bug 452108 ,caijiaqi.wt,Modify,20190618,some device use AFC charger ,charger error
#ifdef  CONFIG_ARCH_SDM429
	bool			dcp_dpdm_flag;
#endif
//-bug 452108 ,caijiaqi.wt,Modify,20190618,some device use AFC charger ,charger error
//-Bug 600732,xushengjuan.wt,modify,20201118,S86117,charger bring up
	bool			step_chg_enabled;
	bool			sw_jeita_enabled;
	bool			is_hdc;
	bool			chg_done;
	int			connector_type;
	bool			otg_en;
	bool			suspend_input_on_debug_batt;
	int			default_icl_ua;
	int			otg_cl_ua;
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

	/* workaround flag */
	u32			wa_flags;
	int			boost_current_ua;
//+Bug 600732,xushengjuan.wt,modify,20201118,S86117,charger bring up
//+Bug 455582,xujianbang.wt,Modify,20190706,(Patch)Handle QC2.0 charger collapse.
#ifdef CONFIG_ARCH_MSM8953
	int			qc2_max_pulses;
	enum qc2_non_comp_voltage qc2_unsupported_voltage;
#endif
//-Bug 455582,xujianbang.wt,Modify,20190706,(Patch)Handle QC2.0 charger collapse.
//-Bug 600732,xushengjuan.wt,modify,20201118,S86117,charger bring up
	bool			dbc_usbov;

	/* extcon for VBUS / ID notification to USB for uUSB */
	struct extcon_dev	*extcon;

	/* battery profile */
	int			batt_profile_fcc_ua;
	int			batt_profile_fv_uv;

	int			usb_icl_delta_ua;
	int			pulse_cnt;

	int			die_health;
	//+Bug 600732,xushengjuan.wt,modify,20201118,S86117,charger bring up
        //+Bug 455539 gudi.wt, MODIFY, 20190805,s86119 control charging current in lcd on or off
        #ifdef CONFIG_ARCH_MSM8953
        int                     last_bat_current;//bug 455539 gudi.wt, MODIFY, 20190717, control charging current in period update
        #endif
        //-Bug 455539 gudi.wt, MODIFY, 20190805,s86119 control charging current in lcd on or off

	/* flash */
	//+EXTB P190730-04079 caijiaqi.wt,add,20190815 Charging popup is coming  while camera with a flash
	#ifdef CONFIG_ARCH_MSM8953
	u32			old_charger_statue;
	#endif
	//-EXTB P190730-04079 caijiaqi.wt,add,20190815 Charging popup is coming  while camera with a flash
	//-Bug 600732,xushengjuan.wt,modify,20201118,S86117,charger bring up
	u32			flash_derating_soc;
	u32			flash_disable_soc;
	u32			headroom_mode;
	bool			flash_init_done;
	bool			flash_active;
#if defined(CONFIG_AFC)
	int			afc_sts;
	bool			hv_disable;
	struct delayed_work		flash_active_work;
#endif
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
//+Bug 600732,xushengjuan.wt,modify,20201118,S86117,charger bring up
//+Bug 437318 caijiaqi.wt, ADD,20190409,P81081 add store_mode node to control capacity
int smblib_get_prop_store_mode(struct smb_charger *chg,
				union power_supply_propval *val);
int smblib_set_prop_store_mode(struct smb_charger *chg,
				const union power_supply_propval *val);
//-Bug 437318 caijiaqi.wt, ADD,20190409,P81081 add store_mode node to control capacity
//-Bug 600732,xushengjuan.wt,modify,20201118,S86117,charger bring up
int smblib_init(struct smb_charger *chg);
int smblib_deinit(struct smb_charger *chg);
#if defined(CONFIG_AFC)
int is_afc_result(struct smb_charger *chg,int result);
#endif

//Bug 603959, liangxiaoqin.wt, 20201216, add, add usb_notify node
void smblib_notify_usb_host(struct smb_charger *chg, bool enable);
#endif /* __SMB5_CHARGER_H */
