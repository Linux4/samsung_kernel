#ifndef __WINGTECH_CHARGER_H
#define __WINGTECH_CHARGER_H
#include <linux/gpio.h>
#include <linux/i2c.h>
#include <linux/init.h>
#include <linux/interrupt.h>
#include <linux/module.h>
#include <linux/power_supply.h>
#include <linux/slab.h>
#include <linux/kernel.h>
#include <linux/sched.h>
#include <linux/kthread.h>
#include <linux/delay.h>
#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/of_gpio.h>
#include <linux/err.h>
#include <linux/bitops.h>
#include <linux/math64.h>
#include <charger_class.h>
#include <mt-plat/mtk_boot.h>
#include <linux/regmap.h>
#include <linux/alarmtimer.h>

enum register_bits  {
	B_EN_HIZ, B_EN_ILIM, B_IILIM, B_EN_ICHG_MON, B_BHOT,
	B_BCOLD, B_VINDPM_OFS, B_CONV_START, B_CONV_RATE, B_BOOSTF,
	B_ICO_EN, B_HVDCP_EN, B_MAXC_EN, B_FORCE_DPM, B_AUTO_DPDM_EN,
	B_FORCE_DSEL, B_BAT_LOAD_EN, B_WD_RST, B_OTG_CFG, B_CHG_CFG,
	B_SYSVMIN, B_PUMPX_EN, B_ICHG, B_IPRECHG, B_ITERM,
	B_CV, B_BATLOWV, B_VRECHG, B_TERM_EN, B_STAT_DIS,
	B_WD, B_TMR_EN, B_CHG_TMR, B_JEITA_ISET, B_BATCMP,
	B_VCLAMP, B_TREG, B_FORCE_ICO, B_TMR2X_EN, B_BATFET_DIS,
	B_JEITA_VSET, B_BATFET_DLY, B_BATFET_RST_EN, B_PUMPX_UP, B_PUMPX_DN,
	B_BOOSTV, B_BOOSTI,	B_VBUS_STAT, B_CHG_STAT, B_PG_STAT,
	B_SDP_STAT, B_VSYS_STAT, B_WD_FAULT, B_BOOST_FAULT, B_CHG_FAULT,
	B_BAT_FAULT, B_NTC_FAULT, B_FORCE_VINDPM, B_VINDPM,	B_THERM_STAT,
	B_BATV,	B_SYSV,	B_TSPCT, B_VBUS_GD, B_VBUSV,
	B_ICHGR, B_VDPM_STAT, B_IDPM_STAT, B_IDPM_LIM, B_REG_RST,
	B_ICO_OPTIMIZED, B_PN, B_TS_PROFILE, B_DEV_REV, B_PFM_DIS,
	B_BOOST_LIM, B_Q1_FULLON, B_TOPOFF_TIMER, B_ARDCEN_ITS, B_VBUS_OVP,
	B_CHG_OTG, B_IINLMTSEL, B_ADP_DIS,
	B_GEG00, B_GEG01, B_GEG02, B_GEG03, B_GEG04, B_GEG05, B_GEG06, B_GEG07, B_GEG08, B_GEG09,
	B_GEG0A, B_GEG0B, B_GEG0C, B_GEG0D, B_GEG0E, B_GEG0F, B_GEG10, B_GEG11, B_GEG12, B_GEG13,
	B_GEG14, B_GEG15, B_GEG16, B_GEG17, B_GEG18, B_GEG19, B_GEG1A, B_GEG1B, B_GEG1C, B_GEG1D,
	B_GEG1E, B_GEG1F, B_GEG40, B_GEG41, B_GEG42, B_GEG43, B_GEG44, B_GEG45, B_GEG50, B_GEG51,
	B_GEG52, B_GEG53, B_GEG54, B_GEG55, B_GEG56, B_GEG60, B_GEG61, B_GEG62, B_GEG63, B_GEG64,
	B_GEG65, B_GEG66,
	B_MAX_FIELDS
};
struct bits_field {
	unsigned int reg;
	unsigned int lsb;
	unsigned int msb;
	bool is_exist;
};

#define BIT_FIELD(_reg, _lsb, _msb) {		\
		.reg = _reg,	\
		.lsb = _lsb,	\
		.msb = _msb,	\
		.is_exist = true, \
}

#define BIT_END {		\
		.reg = 0xFF,	\
		.lsb = 0,	\
		.msb = 0,	\
		.is_exist = false, \
}

struct chg_prop {
	int ichg_step;
	int ichg_min;
	int ichg_max;
	int ichg_offset;
	int iterm_step;
	int iterm_min;
	int iterm_max;
	int iterm_offset;
	int input_limit_step;
	int input_limit_min;
	int input_limit_max;
	int input_limit_offset;
	int last_limit_current;
	int actual_limit_current;
	int cv_step;
	int cv_min;
	int cv_max;
	int cv_offset;
	int vindpm_min;
	int vindpm_max;
	int vindpm_step;
	int vindpm_offset;
	int boost_limit_step;
	int boost_limit_min;
	int boost_limit_max;
	int boost_limit_offset;
	int boost_v_step;
	int boost_v_min;
	int boost_v_max;
	int boost_v_offset;
};

struct charger_ic_t {
	u8 i2c_addr;
	const char *name;
	u8 chip_id;
	u8 match_id[2];
	unsigned int max_register;
	struct chg_prop chg_prop;
	struct bits_field *bits_field;
	
};



struct charger_init_data {
	int ichg;	/* charge current		*/
	int vreg;	/* regulation voltage		*/
	int iterm;	/* termination current		*/
	int iprechg;	/* precharge current		*/
	int sysvmin;	/* minimum system voltage limit */
	int boostv;	/* boost regulation voltage	*/
	int boosti;	/* boost current limit		*/
	int boostf;	/* boost frequency		*/
	int ilim_en;	/* enable ILIM pin		*/
	int treg;	/* thermal regulation threshold */
};

struct charger_state {
	u8 online;
	u8 chrg_status;
	u8 chrg_fault;
	u8 vsys_status;
	u8 boost_fault;
	u8 bat_fault;
};



#define AFC_COMM_CNT 3
#define WAIT_SPING_COUNT 5
#define SPING_MIN_UI 5  //10
#define SPING_MAX_UI 20
#define UI 160

enum {
	SPING_ERR_1	= 1,
	SPING_ERR_2,
	SPING_ERR_3,
	SPING_ERR_4,
};

enum {
	SET_5V	= 5000,
	SET_9V	= 9000,
};

enum {
	OVP_5V	= 6500000,
	OVP_9V	= 10500000,
};

enum {
	VINDPM_5V	= 4200000,
	VINDPM_9V	= 8200000,
};
//+P240305-05569  guhan01.wt 20240318,one ui 6.1 charging protection
enum {
	NORMAL_100,
	HIGHSOC_80,
	SLEEP_80,
	OPTION_80,
	DOWN_80,
};
//-P240305-05569  guhan01.wt 20240318,one ui 6.1 charging protection
struct afc_dev {
	int afc_switch_gpio;
	int afc_data_gpio;
	unsigned int vol;
	unsigned int afc_error;
	spinlock_t afc_spin_lock;
	struct device *switch_dev;

	int afc_charger_input_current;
	int afc_charger_current;
	int afc_min_charger_voltage;
	int afc_max_charger_voltage;
};

struct wtchg_info {
	struct device *dev;
	struct i2c_client *client;

	const char *chg_dev_name;
	const char *eint_name;

	int status;
	int irq;
	int irq_gpio;
	int chg_stat;
	int pd_stat;
	struct mutex i2c_rw_lock;
	bool charge_enabled;	/* Register bit status */

	struct charger_device *chg_dev;
	struct power_supply *psy;
	struct power_supply *otg_psy;
	struct power_supply *chg_psy;
	struct power_supply *usb_psy;
	struct power_supply *bat_psy;
	struct power_supply_desc psy_desc;
	struct power_supply_desc psy_otg_desc;
	
	struct bits_field *cur_field;
	struct charger_ic_t *cur_chg;
	struct charger_init_data init_data;
	struct charger_state state;
	enum power_supply_usb_type usb_type;
	struct notifier_block psy_nb;

	void *mtk_charger;
	/*AFC*/
	struct afc_dev afc;
	struct delayed_work afc_check_work;
	bool is_afc_charger;
	bool afc_enable;
	bool force_dis_afc;
	int cc_polarity;
	bool ato_discharging;
	int afc_start_battery_soc;
	int afc_stop_battery_soc;
	bool lcm_on;
	/*OTHER*/
	u8 wt_discharging_state;
	bool can_charging;
	struct delayed_work afc_5v_to_9v_work;
	struct delayed_work afc_9v_to_5v_work;
	bool is_ato_versions;
	struct delayed_work wtchg_lateinit_work;
	bool store_mode;
	bool dischg_limit;
	struct delayed_work wtchg_set_current_work;
	u32 chg_cur_val;
	u8 slate_mode;
	struct notifier_block pm_notifier;
	u32 batt_full_capacity;
//+P240305-05569  guhan01.wt 20240318,one ui 6.1 charging protection
	int batt_soc_rechg;
	int batt_mode;
//-P240305-05569  guhan01.wt 20240318,one ui 6.1 charging protection
	bool otg_enabled;
	bool batsys_created;
	bool usbsys_created;
	struct wakeup_source *wtchg_wakelock;
	bool shipmode;
	spinlock_t slock;
	wait_queue_head_t  wait_que;
	struct alarm charger_timer;
	struct timespec endtime;
	bool charger_thread_timeout;
	struct mutex charger_lock;
	bool charger_thread_polling;
	bool is_hiz;
	struct tcpc_device *tcpc_dev;
	struct notifier_block pd_nb;
	int pd_type;
	int bat_temp;
};

enum wt_discharging_status {
	WT_CHARGE_SOC_LIMIT_DISCHG = BIT(0),
	WT_CHARGE_DEMO_MODE_DISCHG = BIT(1),
	WT_CHARGE_SLATE_MODE_DISCHG = BIT(2),
	WT_CHARGE_BATOVP_DISCHG = BIT(3),
	WT_CHARGE_FULL_CAP_DISCHG = BIT(4),
};

#define ATO_LIMIT_SOC_MAX	79
#define ATO_LIMIT_SOC_MIN	60

#define STORE_MODE_SOC_MAX	69
#define STORE_MODE_SOC_MIN	60

#define BATT_MISC_EVENT_FULL_CAPACITY  0x01000000

extern struct wtchg_info *wt_get_wtchg_info(void); 


#endif

