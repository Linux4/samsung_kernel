#if !defined(__NOPMI_CHG_H__)
#define __NOPMI_CHG_H__

#include "nopmi_chg_jeita.h"
#include <linux/qti_power_supply.h>
#include "nopmi_chg_iio.h"
#include <linux/iio/consumer.h>
#include <dt-bindings/iio/qti_power_supply_iio.h>

#define STEP_TABLE_MAX			2
#define STEP_DOWN_CURR_MA		150
#define CV_BATT_VOLT_HYSTERESIS		10

#define CC_CV_STEP_VOTER		"CC_CV_STEP_VOTER"

//longcheer nielianjie10 2022.10.13 add battery verify to limit charge current and modify battery verify logic
#define BAT_VERIFY_VOTER		"BAT_VERIFY_VOTER"  //battery verify vote to FCC
#define UNVEIRFY_BAT			2000     //FCC limit to 2A
#define VERIFY_BAT			5000	 //FCC limit to 5A

//longcheer nielianjie10 2022.12.05 Set CV according to circle count
#define CYCLE_COUNT			100	//battery cycle count

#define PROP_SIZE_LEN 20

#define BAT_CURRENT_EVENT_AFC_MASK                      0x01
#define BAT_CURRENT_EVENT_CHARGE_DISABLE_MASK           0x02
#define BAT_CURRENT_EVENT_LOW_TEMP_SWELLING_MASK        0x10
#define BAT_CURRENT_EVENT_HIGH_TEMP_SWELLING_MASK       0x20
#define BAT_CURRENT_EVENT_USB_100MA_MASK                0x04

#define BAT_CURRENT_EVENT_AFC_SHIFT                      0
#define BAT_CURRENT_EVENT_CHARGE_DISABLE_SHIFT           1
#define BAT_CURRENT_EVENT_LOW_TEMP_SWELLING_SHIFT        4
#define BAT_CURRENT_EVENT_HIGH_TEMP_SWELLING_SHIFT       5
#define BAT_CURRENT_EVENT_USB_100MA_SHIFT                2

//#define BAT_CURRENT_EVENT_TIMEOUT_OPEN_TYPE_MASK           0x04

//#define BAT_CURRENT_EVENT_TIMEOUT_OPEN_TYPE_SHIFT           2
#define BATT_MISC_EVENT_TIMEOUT_OPEN_TYPE  0x00000004 // dcd_timeout
#define BATT_MISC_EVENT_FULLCAPACITY_TYPE  0x01000000 // soc 85, protected

#define NOPMI_FULL_IN_ADVANCE

#ifdef NOPMI_FULL_IN_ADVANCE
#define FULL_IN_ADVANCE_COUNT  10
#endif

#define SEC_BATTERY_CABLE_UNKNOWN                0
#define SEC_BATTERY_CABLE_NONE                   1
#define SEC_BATTERY_CABLE_PREPARE_TA             2
#define SEC_BATTERY_CABLE_TA                     3
#define SEC_BATTERY_CABLE_USB                    4
#define SEC_BATTERY_CABLE_USB_CDP                5
#define SEC_BATTERY_CABLE_9V_TA                  6
#define SEC_BATTERY_CABLE_9V_ERR                 7
#define SEC_BATTERY_CABLE_9V_UNKNOWN             8
#define SEC_BATTERY_CABLE_12V_TA                 9
#define SEC_BATTERY_CABLE_WIRELESS               10
#define SEC_BATTERY_CABLE_HV_WIRELESS            11
#define SEC_BATTERY_CABLE_PMA_WIRELESS           12
#define SEC_BATTERY_CABLE_WIRELESS_PACK          13
#define SEC_BATTERY_CABLE_WIRELESS_HV_PACK       14
#define SEC_BATTERY_CABLE_WIRELESS_STAND         15
#define SEC_BATTERY_CABLE_WIRELESS_HV_STAND      16
#define SEC_BATTERY_CABLE_QC20                   17
#define SEC_BATTERY_CABLE_QC30                   18
#define SEC_BATTERY_CABLE_PDIC                   19
#define SEC_BATTERY_CABLE_UARTOFF                20
#define SEC_BATTERY_CABLE_OTG                    21
#define SEC_BATTERY_CABLE_LAN_HUB                22
#define SEC_BATTERY_CABLE_POWER_SHARING          23
#define SEC_BATTERY_CABLE_HMT_CONNECTED          24
#define SEC_BATTERY_CABLE_HMT_CHARGE             25
#define SEC_BATTERY_CABLE_HV_TA_CHG_LIMIT        26
#define SEC_BATTERY_CABLE_WIRELESS_VEHICLE       27
#define SEC_BATTERY_CABLE_WIRELESS_HV_VEHICLE    28
#define SEC_BATTERY_CABLE_PREPARE_WIRELESS_HV    29
#define SEC_BATTERY_CABLE_TIMEOUT                30
#define SEC_BATTERY_CABLE_SMART_OTG              31
#define SEC_BATTERY_CABLE_SMART_NOTG             32
#define SEC_BATTERY_CABLE_WIRELESS_TX            33
#define SEC_BATTERY_CABLE_HV_WIRELESS_20         34
#define SEC_BATTERY_CABLE_HV_WIRELESS_20_LIMIT   35
#define SEC_BATTERY_CABLE_WIRELESS_FAKE          36
#define SEC_BATTERY_CABLE_PREPARE_WIRELESS_20    37
#define SEC_BATTERY_CABLE_PDIC_APDO              38
#define SEC_BATTERY_CABLE_POGO                   39
#define SEC_BATTERY_CABLE_POGO_9V                40
#define SEC_BATTERY_CABLE_FPDO_DC                41
#define SEC_BATTERY_CABLE_MAX                    42


struct step_config {
	int volt_lim;
	int curr_lim;
};

struct nopmi_dt_props {
	int	usb_icl_ua;
	int	chg_inhibit_thr_mv;
	bool	no_battery;
	bool	hvdcp_disable;
	bool	hvdcp_autonomous;
	bool	adc_based_aicl;
	int	sec_charger_config;
	int	auto_recharge_soc;
	int	auto_recharge_vbat_mv;
	int	wd_bark_time;
	int	wd_snarl_time_cfg;
	int	batt_profile_fcc_ua;
	int	batt_profile_fv_uv;
};

struct nopmi_chg {
	struct platform_device *pdev;
	struct device *dev;
    struct iio_dev	*indio_dev;
	struct iio_chan_spec	*iio_chan;
	struct iio_channel	*int_iio_chans;
    struct iio_channel	**fg_ext_iio_chans;
    struct iio_channel	**cp_ext_iio_chans;
    struct iio_channel	**main_chg_ext_iio_chans;
    struct iio_channel	**cc_ext_iio_chans;
    struct iio_channel	**ds_ext_iio_chans;
	struct charger_device *master_dev;
	struct charger_device *slave_dev;
	struct charger_device *bbc_dev;
    struct class battery_class;
    struct device batt_device;
	struct tcpc_device *tcpc_dev;
	struct notifier_block pd_nb;
	//int typec_mode;
	enum power_supply_typec_mode typec_mode;
	int  cc_orientation;
	struct power_supply *main_psy;
	struct power_supply *master_psy;
	struct power_supply *slave_psy;
	struct power_supply *batt_psy;
	struct power_supply *usb_psy;
	struct power_supply *bms_psy;
	struct power_supply *bbc_psy;
	struct power_supply *ac_psy;
	bool ac_hold_online;
	bool main_chgdet;
	//longcheer nielianjie10 2022.12.05 Set CV according to circle count
	struct step_config *select_cc_cv_step_config;
	int cycle_count;
	//2021.09.21 wsy edit reomve vote to jeita
#if 1
	struct votable		*fcc_votable;
	struct votable		*fv_votable;
	struct votable		*usb_icl_votable;
	struct votable		*chg_dis_votable;
#endif
	struct nopmi_dt_props	dt;
	struct delayed_work nopmi_chg_work;
	struct delayed_work 	cvstep_monitor_work;
	struct delayed_work	xm_prop_change_work;
	struct delayed_work     fv_vote_monitor_work;
	struct delayed_work     batt_capacity_check_work;
	struct delayed_work     thermal_check_work;
	struct delayed_work     batt_slate_mode_check_work;
	struct delayed_work     store_mode_check_work;
	int online;
	int hv_charger_status;
	int batt_current_event;
	int batt_misc_event;
	int batt_full_capacity_soc;
	int battery_cycle;
	int batt_charging_source;
	int batt_current_ua_now;
	int direct_charging_status;
	int batt_charging_type;
	int batt_store_mode;
	int batt_soc_rechg;
	char batt_full_capacity[32];
	bool is_store_mode_stop_charge;
#ifdef NOPMI_FULL_IN_ADVANCE
	int batt_capacity_store;
	int batt_full_count;
#endif
	int time_to_full_now;
	int batt_slate_mode;
	struct device_attribute attr_online;
	struct device_attribute attr_hv_charger_status;
	struct device_attribute attr_batt_current_event;
	struct device_attribute attr_batt_misc_event;
	struct device_attribute attr_batt_full_capacity;
	struct device_attribute attr_battery_cycle;
	struct device_attribute attr_time_to_full_now;
	struct device_attribute attr_batt_slate_mode;
	struct device_attribute attr_batt_charging_source;
	struct device_attribute attr_batt_current_ua_now;
	struct device_attribute attr_direct_charging_status;
	struct device_attribute attr_charging_type;
	struct device_attribute attr_store_mode;
	struct device_attribute attr_batt_soc_rechg;
	int pd_active;
	int onlypd_active;
	int in_verified;
	int real_type;
	int pd_min_vol;
	int pd_max_vol;
	int pd_cur_max;
	int pd_usb_suspend;
	int pd_in_hard_reset;
	int usb_online;
	int batt_health;
	int input_suspend;
	int mtbf_cur;
	int update_cont;
	/*jeita config*/
	struct nopmi_chg_jeita_st jeita_ctl;
	/* thermal */
	int *thermal_mitigation;
	int thermal_levels;
	int system_temp_level;
        /*apdo*/
	int apdo_volt;
	int apdo_curr;
	int cp_type;
	NOPMI_CHARGER_IC_TYPE charge_ic_type;
	const char * const * chip_fg_ext_iio_chan_name;
	const char * const * chip_main_chg_ext_iio_chan_name;

	/*LCD sysfs*/
	int lcd_status;
	bool disable_quick_charge;
	bool disable_pps_charge;
	struct device_attribute attr_lcd_status;
	bool afc_flag;
};

enum nopmi_chg_iio_type {
	NOPMI_MAIN,
	NOPMI_BMS,
	NOPMI_CC,
	NOPMI_CP,
};

enum nopmi_cp_type {
	CP_SP2130,
	CP_UPM6720,
	CP_UNKOWN
};

struct nopmi_charging_type{
	int charging_type;
	char *ta_type;
};

extern int nopmi_chg_get_iio_channel(struct nopmi_chg *chg,
			enum nopmi_chg_iio_type type, int channel, int *val);
extern int nopmi_chg_set_iio_channel(struct nopmi_chg *chg,
			enum nopmi_chg_iio_type type, int channel, int val);
//extern bool is_cp_chan_valid(struct nopmi_chg *chip,
//			enum cp_ext_iio_channels chan);
extern bool is_cc_chan_valid(struct nopmi_chg *chip,
			enum cc_ext_iio_channels chan);
//extern bool is_ds_chan_valid(struct nopmi_chg *chip,
//			enum ds_ext_iio_channels chan);
extern bool first_power_on;

#endif
