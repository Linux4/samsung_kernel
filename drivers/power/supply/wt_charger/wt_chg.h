/* SPDX-License-Identifier: GPL-2.0 */
#ifndef __LINUX_WT_CHARGER_H__
#define __LINUX_WT_CHARGER_H__

#include <linux/module.h>

//+bug tankaikun.wt, add, 20220806. Add charger temp control strategy
#ifdef WT_COMPILE_FACTORY_VERSION
#define	DISABLE_BOARD_TEMP_CONTROL
#define DISABLE_CHG_TEMP_CONTROL
#define DISABLE_USB_PORT_TEMP_CONTROL
#endif /*WT_COMPILE_FACTORY_VERSION*/
//-bug tankaikun.wt, add, 20220806. Add charger temp control strategy

//+P86801AA1-3487, xubuchao1.wt, add, 2023/4/25, add ato uisoc control
#define ATO_BATT_SOC_MAX	78
#define ATO_BATT_SOC_MIN	60
//-P86801AA1-3487, xubuchao1.wt, add, 2023/4/25, add ato uisoc control

#define BATT_PROTECT_SOC_MAX	60
#define BATT_PROTECT_SOC_MIN	40

#define BATT_MAINTAIN_SOC_MAX	80
#define BATT_MAINTAIN_SOC_MIN	40

#ifdef CONFIG_AMERICA_VERSION
#define STORE_MODE_SOC_MAX	35
#define STORE_MODE_SOC_MIN	30
#else
#define STORE_MODE_SOC_MAX	70
#define STORE_MODE_SOC_MIN	60
#endif

#define BATT_SOC_ZERO	0
#define SOC_ZERO_MAX	3

#define BATT_TEMP_MIN	-200
#define BATT_TEMP_MAX	900
#define BATT_TEMP_0	0
#define TEMP_ZERO_MAX	3

#define BATT_VOLT_MIN 3100000
#define BATT_VOLT_MAX 4500000
#define VOLT_ZERO_MAX 3

#define FLOAT_RECHECK_TIME_MAX 10
#define USB_RECHECK_TIME_MAX 40
//P231018-03087 gudi.wt 20231019,add adb prot recheck time
//+P231018-03087 gudi.wt 20231025 fix eng adb prot
#ifdef CONFIG_CHARGER_ENG
#define SYS_BOOT_RECHECK_TIME_MAX 20000
#else
#define SYS_BOOT_RECHECK_TIME_MAX 3700
#endif
//-P231018-03087 gudi.wt 20231025 fix eng adb prot
#define RERUN_ADSP_COUNT_MAX 3

#define SLAVE_CHG_START_SOC_MAX 90

#define CHARGE_ONLINE_ATO_INTERVAL_MS 5000
#define CHARGE_ONLINE_INTERVAL_MS	10000
#define NORMAL_INTERVAL_MS	30000

//Jeita parameter
#define BATT_TEMP_MIN_THRESH	-200
#define BATT_TEMP_0_THRESH	10
#define BATT_TEMP_3_THRESH	50
#define BATT_TEMP_20_THRESH	120
#define BATT_TEMP_15_THRESH	160
#define BATT_TEMP_45_THRESH	450
#define BATT_TEMP_50_THRESH	500
#define BATT_TEMP_52_THRESH	520
#define BATT_TEMP_55_THRESH	550
#define BATT_TEMP_MAX_THRESH	600
#define BATT_TEMP_HYSTERESIS	20

#define BOARD_TEMP_40_THRESH	400
#define BOARD_TEMP_41_THRESH	410
#define BOARD_TEMP_42_THRESH	420
#define BOARD_TEMP_43_THRESH	430
#define BOARD_TEMP_44_THRESH	440
#define BOARD_TEMP_45_THRESH	450
#define BOARD_TEMP_46_THRESH	460
#define BOARD_TEMP_47_THRESH	470
#define BOARD_TEMP_48_THRESH	480
#define BOARD_TEMP_49_THRESH	490
#define BOARD_TEMP_50_THRESH	500
#define BOARD_TEMP_52_THRESH	520
#define BOARD_TEMP_54_THRESH	540
#define BOARD_TEMP_56_THRESH	560
#define BOARD_TEMP_58_THRESH	580

#define CHG_IC_TEMP_41_THRESH	410
#define CHG_IC_TEMP_43_THRESH	430
#define CHG_IC_TEMP_45_THRESH	450
#define CHG_IC_TEMP_47_THRESH	470
#define CHG_IC_TEMP_48_THRESH	480
#define CHG_IC_TEMP_50_THRESH	500
#define CHG_IC_TEMP_52_THRESH	520
#define CHG_IC_TEMP_54_THRESH	540
#define CHG_IC_TEMP_56_THRESH	560
#define CHG_IC_TEMP_58_THRESH	580

 
#define USBIN_ADAPTER_ALLOW_5V		5000
#define USBIN_ADAPTER_ALLOW_9V		9000
#define USBIN_ADAPTER_ALLOW_12V		12000

#define PD_INPUT_CURRENT_3000_MA	3000000
#define PD_INPUT_CURRENT_2000_MA	2000000
#define PD_INPUT_CURRENT_1500_MA	1500000
#define PD_INPUT_CURRENT_1000_MA	1000000
#define PD_INPUT_CURRENT_500_MA		 500000
#define PD_INPUT_CURRENT_100_MA		 100000
#define PD_INPUT_CURRENT_0_MA		      0	


#define FCC_4000_MA	4000
#define FCC_3600_MA	3600
#define FCC_3000_MA	3000
#define FCC_2600_MA	2600
#define FCC_2500_MA	2500
#define FCC_2200_MA	2200
#define FCC_2000_MA	2000

#define FCC_1800_MA	1800
#define FCC_1700_MA	1700
#define FCC_1600_MA	1600
#define FCC_1550_MA	1550
#define FCC_1500_MA	1500

#define FCC_1250_MA	1250
#define FCC_1300_MA	1300
#define FCC_1200_MA	1200
#define FCC_1100_MA	1100
#define FCC_1000_MA	1000
#define FCC_800_MA	800
#define FCC_500_MA	500
#define FCC_300_MA	300
#define FCC_200_MA	200
#define FCC_100_MA	100
#define FCC_0_MA	0

#define BATT_NORMAL_CV		4400
#define BATT_HIGH_TEMP_CV	4200

#define DEFAULT_HVDCP_VOLT 5000

#define DEFAULT_BATT_TEMP	250
#define CHG_CURRENT_BYPASS_VALUE 2000

#define HVDCP20_VOLT_6V 6000
#define DEFAULT_USB_VOLT_DESIGN	5000
#define DEFAULT_HVDCP3_USB_VOLT_DESIGN	5600
#define DEFAULT_HVDCP_USB_VOLT_DESIGN	9000

//+bug tankaikun.wt, add, 20220709. Add charger current step control
#define DEFAULT_FCC_STEP_UPDATE_DELAY_MS 	500	 // 0.6s
#define DEFAULT_FCC_STEP_SIZE_MA			500  // 500mA
#define DEFAULT_ICL_STEP_UPDATE_DELAY_MS 	500  // 0.5s
#define DEFAULT_ICL_STEP_SIZE_MA			400  // 400mA
#define BPP_FCC_STEP_SIZE_MA				200  // 200mA
#define EPP_FCC_STEP_SIZE_MA				500  // 500mA
#define WIRELESS_WAIT_FOR_VBUS_MS			1000 // 1s

#define STEP_UP 1
#define STEP_DOWN -1
//-bug tankaikun.wt, add, 20220709. Add charger current step control

//bug 792983, tankaikun@wt, add 20220228, fix charge status is full but capacity is lower than 100%
#define MIN_SOC_CHANGE 1
#define VBUS_ONLINE_MIN_VOLT	3900000

#define SHUTDOWN_BATT_VOLT	3250000

//bug761884, tankaikun@wt, add 20220831, fix iic read/write return -13 when system suspend
#define WT_CHARGER_ALARM_TIME				30000 // 30s
#define WT_CHARGER_ALARM_OFF_MODE_TIME		90000 // 90s

//+bug761884, tankaikun@wt, add 20220831, add wireless step charge logic
#define WIRELESS_ADAPER_VOLT_5000_MV	5000
#define WIRELESS_ADAPER_VOLT_9000_MV	9000
#define WIRELESS_ADAPER_VOLT_12000_MV	12000
//-bug761884, tankaikun@wt, add 20220831, add wireless step charge logic

//bug 806590, tankaikun.wt, mod, 2022/11/9, limit report 100% capacity term current
#define BATTERY_FULL_TERM_CURRENT 600
#define BATTERY_FULL_MAX_CNT_DESIGN 3

#define DYNAMIC_UPDATE_WIRELESS_FOD_CAPACITY 95

#define SHUTDOWN_CNT_CHECK_VBAT	3

enum batt_jeita_status {
	BATT_TEMP_COLD = 0,
	BATT_TEMP_COOL,
	BATT_TEMP_COOL_2,
	BATT_TEMP_NORMAL,
	BATT_TEMP_WARM,
	BATT_TEMP_HOT,
};

enum wt_hvdcp_mode {
	WT_HVDCP20_5V = 1, 
	WT_HVDCP20_9V, 
	WT_HVDCP20_12V, 
	WT_HVDCP30_5V, 
};

/*hvdcp30*/
struct hvdcp30_profile_t {
	unsigned int vbat;
	unsigned int vchr;
};

enum wt_wireless_type {
	WIRELESS_VBUS_NONE,
	WIRELESS_VBUS_WL_BPP,
	WIRELESS_VBUS_WL_10W_MODE,
	WIRELESS_VBUS_WL_EPP,
};

enum{
	STEPPER_STATUS_DISABLE = 0,
	STEPPER_STATUS_ENABLE,
	STEPPER_STATUS_UP,
	STEPPER_STATUS_DOWN,
	STEPPER_STATUS_COMPLETE,
	STEPPER_STATUS_UNKNOW,
};

//+bug761884, tankaikun@wt, add 20220811, thermal step charge strategy
enum thermal_engine_types {
	USB_THERMAL_ENGINE_BC12 = 0,
	USB_THERMAL_ENGINE_FCHG,
	USB_THERMAL_ENGINE_WL_BPP,
	USB_THERMAL_ENGINE_WL_EPP,
	USB_THERMAL_ENGINE_UNKNOW,
	BOARD_THERMAL_ENGINE_BC12,
	BOARD_THERMAL_ENGINE_FCHG,
	BOARD_THERMAL_ENGINE_WL_BPP,
	BOARD_THERMAL_ENGINE_WL_EPP,
	BOARD_THERMAL_ENGINE_UNKNOW,
	CHG_THERMAL_ENGINE_BC12,
	CHG_THERMAL_ENGINE_FCHG,
	CHG_THERMAL_ENGINE_WL_BPP,
	CHG_THERMAL_ENGINE_WL_EPP,
	THERMAL_ENGINE_MAX,
};

struct wtchg_thermal_engine_table {
	int temp;
	int clr_temp;
	int lcd_on_ibat;
	int lcd_off_ibat;
};
//-bug761884, tankaikun@wt, add 20220811, thermal step charge strategy

//+P231218-05362  guhan01.wt 20231223,Add batt_ Slate_ Mode node value
enum sec_battery_slate_mode {
  SEC_SLATE_OFF = 0,
  SEC_SLATE_MODE,
  SEC_SMART_SWITCH_SLATE,
  SEC_SMART_SWITCH_SRC,
};
//-P231218-05362  guhan01.wt 20231223,Add batt_ Slate_ Mode node value

typedef enum{
	CHRG_STATE_STOP,
	CHRG_STATE_FAST,
	CHRG_STATE_TAPER,
	CHRG_STATE_FULL,
} wtchg_state;

struct wtchg_wakeup_source {
	struct wakeup_source	*source;
	unsigned long		disabled;
};

struct wt_chg {
	struct platform_device *pdev;
	struct device *dev;
	struct charger_device *master_dev;
	struct charger_device *slave_dev;
	struct charger_device *bbc_dev;

	struct iio_channel *main_chg_therm;
	struct iio_channel *usb_port_therm;
	struct iio_channel *board_pcb_therm;
	struct iio_channel *vbus_value;

	struct iio_dev *indio_dev;
	struct iio_chan_spec *iio_chan;
	struct iio_channel *int_iio_chans;

	struct iio_channel	**gq_ext_iio_chans;
	struct iio_channel	**main_chg_ext_iio_chans;
	struct iio_channel	**wl_chg_ext_iio_chans;
	struct iio_channel	**afc_chg_ext_iio_chans;

	struct power_supply *main_psy;
	struct power_supply *slave_psy;
	struct power_supply *batt_psy;
	struct power_supply *ac_psy;
	struct power_supply *otg_psy;
	struct power_supply *wl_psy;
	struct power_supply *usb_psy;
	struct power_supply *bms_psy;
	struct power_supply *hvdcp_psy;

	struct wtchg_wakeup_source	wtchg_wl_wake_source;
	struct wtchg_wakeup_source	wtchg_ato_soc_wake_source;
	struct wtchg_wakeup_source	wtchg_alarm_wake_source;
	struct wtchg_wakeup_source	wtchg_iterm_wake_source;
	struct wtchg_wakeup_source	wtchg_off_charger_wake_source;
	struct notifier_block wtchg_fb_notifier;

	struct class battery_class;
	struct device batt_device;

	struct timer_list hvdcp_timer;
	struct delayed_work wt_chg_work;
	struct delayed_work fcc_stepper_work;
	struct delayed_work icl_stepper_work;
	struct delayed_work ato_soc_user_control_work;
	struct delayed_work wt_chg_post_init_work;
	struct delayed_work wt_float_recheck_work;
	struct work_struct	wl_chg_stepper_work;
	struct work_struct wtchg_alarm_work;

	int pd_active;
	int real_type;
	int pd_min_vol;
	int pd_max_vol;
	int pd_allow_vol;
	int pd_cur_max;
	int pd_usb_suspend;
    int pd_in_hard_reset;

	int usb_load_finish;
	int usb_connect;
	int vbus_online;
	int pre_vbus_online;
	int vbus_volt;
	int chg_status;
	int usb_online;
	int ac_online;
	int otg_enable;
	int batt_volt;
	int rerun_adsp_count;
	//P86801AA1-3622,gudi.wt,20230705,battery protect func
	int batt_cycle;
	int batt_time;
	int chg_current;
	int batt_capacity;
	int batt_capacity_max;
	int batt_capacity_last;
	int batt_capacity_pre;
	int batt_capacity_recharge;
	int batt_capacity_level;
	int batt_capacity_fake;
	int batt_temp;
	int batt_health;
	int board_temp;
	int board_temp_pre;
	int main_chg_temp;
	int main_chg_temp_pre;
	int usb_port_temp;
	int usb_port_temp_pre;
	int lcd_on;
	const char *batt_id_string;

	int batt_cv_max;
	int jeita_batt_cv;
	int jeita_batt_cv_pre;

	int batt_fcc_max;
	int jeita_ibatt;
	int chg_ibatt;
	int chg_ibatt_pre;

	int main_chg_ibatt;
	int slave_chg_ibatt;
	int slave_chg_enable;

	int chg_type_ibatt;
	int chg_type_input_curr;
	int chg_type_input_curr_pre;
	int chg_type_input_curr_max;
	int chg_type_input_voltage_max;

	int main_chg_input_curr;
	int slave_chg_input_curr;

	int batt_jeita_stat_prev;
	int chg_iterm;
	int batt_iterm;
	int thermal_levels;
	int system_temp_level;

	int start_charging_test;
	int wt_discharging_state;
	int ato_soc_user_control;
	int interval_time;
	int soc_zero_count;
	int temp_zero_count;
	int volt_zero_count;

	bool chg_init_flag;
	bool safety_timeout;
	int max_charging_time;
	int batt_maintain_mode;
	int batt_protected_mode;
	int store_mode;
	int batt_slate_mode;
	int batt_full_capacity;

	int pre_vbus;
	int ship_mode;

	bool main_chg_temp_debug_flag;
	int	main_chg_debug_temp;
	bool usb_temp_debug_flag;
	int	usb_debug_temp;
	bool batt_temp_debug_flag;
	int	batt_debug_temp;
	bool board_temp_debug_flag;
	int	board_debug_temp;

	int typec_cc_orientation;

	//+bug761884, tankaikun@wt, add 20220711, wireless step charge
	int main_step_fcc_dir;
	int main_step_fcc_count;
	int main_step_fcc_residual;
	int main_fcc_ma;
	int main_step_wl_icl_dir;
	int main_step_wl_icl_count;
	int main_step_wl_icl_residual;
	int main_icl_ma;
	int fcc_step_ma;

	long fcc_stepper_status;
	long icl_stepper_status;

	int wireless_fcc_design_max;
	int wireless_icl_design_max;

	int wireless_adapter_volt_design;

	int wl_online;
	int wl_type;
	//-bug761884, tankaikun@wt, add 20220711, wireless step charge

	//+bug761884, tankaikun@wt, add 20220811, thermal step charge strategy
	struct wtchg_thermal_engine_table *usb_thermal_engine_tab;
	struct wtchg_thermal_engine_table *board_thermal_engine_tab;
	struct wtchg_thermal_engine_table *chg_thermal_engine_tab;
	struct wtchg_thermal_engine_table *thermal_engine_tab_array[THERMAL_ENGINE_MAX];
	u32 thermal_engine_tab_size[THERMAL_ENGINE_MAX];

	int usb_thermal_status;
	int board_thermal_status;
	int chg_thermal_status;

	int last_usb_thermal_status;
	int last_board_thermal_status;
	int last_chg_thermal_status;

	int thermal_ibatt;

	bool disable_usb_port_therm;
	bool disable_main_chg_therm;
	bool disable_board_pcb_therm;
	//-bug761884, tankaikun@wt, add 20220811, thermal step charge strategy

	//+chk3593, liyiying.wt, 2022/7/21, add, judge the vbat if over the cv value
	unsigned long last_vbat_update;
	int vbat_update_time;
	int vbat_over_count;
	int vbat_over_flag;
	//-chk3593, liyiying.wt, 2022/7/21, add, judge the vbat if over the cv value

	//bug792983, tankaikun@wt, add 20220831, fix device recharge but no charging icon
	int eoc_reported;

	//+bug761884, tankaikun@wt, add 20220831, fix iic read/write return -13 when system suspend
	struct alarm wtchg_alarm;
	struct mutex resume_complete_lock;
	int wtchg_alarm_init;

	bool wtchg_alarm_status;

	int resume_completed;
	//-bug761884, tankaikun@wt, add 20220831, fix iic read/write return -13 when system suspend

	//+bug 806590, tankaikun.wt, mod, 2022/11/9, limit report 100% capacity term current
	unsigned long batt_full_check_timer;
	int batt_full_check_counter;
	//-bug 806590, tankaikun.wt, mod, 2022/11/9, limit report 100% capacity term current

	//+bug825533, tankaikun@wt, add 20230216, add software control iterm
	unsigned long chg_state_update_timer;
	wtchg_state chg_state_step;
	int chg_status_hyst_cnt;
	int chg_iterm_cnt;
	int batt_recharge_offset_vol;
	int chg_status_target_fcc;
	//-bug825533, tankaikun@wt, add 20230216, add software control iterm

	int shutdown_cnt;
	bool shutdown_check_ok;
};

MODULE_LICENSE("GPL v2");

#endif
