#ifndef _AFC_CHARGER_INTF
#define _AFC_CHARGER_INTF

#include <linux/kernel.h>
#include "mtk_charger_algorithm_class.h"
#include "mtk_charger.h"
#define AFC_COMM_CNT 3
#define AFC_RETRY_MAX 10
#define VBUS_RETRY_MAX 5
#define UI 160
#define WAIT_SPING_COUNT 5
#define RECV_SPING_RETRY 5
#define SPING_MIN_UI 5  //10
#define SPING_MAX_UI 20
#define HV_DISABLE 1
#define HV_ENABLE 0
#define ALGO_LOOP_MAX 3   //Bug 516173,zhaosidong.wt,ADD,20191205,afc detection
//define function macro
#define NO_COMM_CHECK 0

extern int g_afc_work_status;//Bug 518556,liuyong3.wt,ADD,20191128,Charging afc flag
/* AFC */
#define AFC_ICHG_LEAVE_THRESHOLD	1000000 /* uA */
#define AFC_START_BATTERY_SOC		0
#define AFC_STOP_BATTERY_SOC		85
#define AFC_PRE_INPUT_CURRENT		500000 /* uA */
#define AFC_CHARGER_INPUT_CURRENT	1500000 /* uA */
#define AFC_CHARGER_CURRENT			2800000 /* uA */
#define AFC_MIN_CHARGER_VOLTAGE		4200000
#define AFC_MAX_CHARGER_VOLTAGE		9000000


enum {
	CHECK_AFC	= 0,
	SET_VOLTAGE 	= 1,
};

enum {
	AFC_INIT,
	NOT_AFC,
	AFC_FAIL,
	AFC_DISABLE,
	NON_AFC_MAX
};

enum {
	SPING_ERR_1	= 1,
	SPING_ERR_2,
	SPING_ERR_3,
	SPING_ERR_4,
};

/* AFC detected */
enum {
	AFC_5V = NON_AFC_MAX,
	AFC_9V,
	AFC_12V,
};

enum {
	SET_5V	= 5000000,
	SET_9V	= 9000000,
};

enum afc_state_enum {
	AFC_HW_UNINIT = 0,
	AFC_HW_FAIL,
	AFC_HW_READY,
	AFC_TA_NOT_SUPPORT,
	AFC_RUN,
	AFC_TUNING,
	AFC_POSTCC,
};

struct afc_dev {
	struct platform_device *pdev;
	int afc_switch_gpio;
	struct chg_alg_device *alg;
	int afc_data_gpio;
	unsigned int vol;
	unsigned int afc_error;
	bool afc_init_ok;
	bool is_enable;
	bool pin_state; /* true: active, false: suspend */
	spinlock_t afc_spin_lock;
	struct pinctrl *pinctrl;
	struct pinctrl_state *pin_active;
	struct pinctrl_state *pin_suspend;
	bool is_connect;
	bool to_check_chr_type;
	struct wakeup_source *suspend_lock;
	int ta_vchr_org;
	struct mutex access_lock;
	int afc_loop_algo; //Bug 516173,zhaosidong.wt,ADD,20191205,afc detection
	int afc_start_battery_soc;
	int afc_stop_battery_soc;
	int afc_ichg_level_threshold;
	int afc_pre_input_current;
	int afc_charger_input_current;
	int afc_charger_current;
	int afc_min_charger_voltage;
	int afc_max_charger_voltage;
	struct mtk_charger *info;
	int afc_disable;
	struct power_supply *bat_psy;
	enum afc_state_enum state;

	int afc_limit_input_current;
	int afc_limit_charge_current;
	int afc_setting_input_current;
	int afc_setting_charge_current;
	int afc_limit_cv;
	struct mutex data_lock;
#if defined(CONFIG_DRV_SAMSUNG)
	struct device *switch_device;
#endif
};

#ifdef CONFIG_AFC_CHARGER

int afc_charge_init(struct mtk_charger *pinfo);
int afc_charge_init_alg(struct chg_alg_device *alg);
bool afc_get_is_connect(struct mtk_charger *pinfo);
void afc_set_is_cable_out_occur(struct chg_alg_device *alg, bool out);
bool afc_get_is_enable(struct mtk_charger *pinfo);
void afc_set_is_enable(struct mtk_charger *pinfo, bool enable);
int afc_set_charging_current(struct mtk_charger *pinfo, unsigned int *ichg, unsigned int *aicr);
void afc_set_to_check_chr_type(struct mtk_charger *pinfo, bool check);
int afc_check_charger(struct mtk_charger *pinfo);
int afc_set_ta_vchr(struct mtk_charger *pinfo, u32 chr_volt);
int afc_start_algorithm(struct chg_alg_device *alg);
int afc_charge_init(struct mtk_charger *pinfo);
int afc_reset_ta_vchr(struct mtk_charger *pinfo);
int afc_leave(struct mtk_charger *pinfo);
void afc_reset(struct afc_dev *afc);
#else /* NOT CONFIG_AFC_CHARGER */

static inline int afc_charge_init(struct charger_manager *pinfo)
{
   	return -ENOTSUPP;
}

static inline int afc_get_is_connect(struct charger_manager *pinfo)
{
	return -ENOTSUPP;
}

static inline int afc_reset_ta_vchr(struct charger_manager *pinfo)
{
	return -ENOTSUPP;
}

static inline int afc_check_charger(struct charger_manager *pinfo)
{
	return -ENOTSUPP;
}

static inline int afc_start_algorithm(struct charger_manager *pinfo)
{
	return -ENOTSUPP;
}

static inline int afc_set_charging_current(struct charger_manager *pinfo, unsigned int *ichg, unsigned int *aicr)
{
	return -ENOTSUPP;
}

static inline void afc_set_to_check_chr_type(struct charger_manager *pinfo, bool check)
{
}

static inline void afc_set_is_enable(struct charger_manager *pinfo, bool enable)
{
}

static inline void afc_set_is_cable_out_occur(struct charger_manager *pinfo, bool out)
{
}

static inline bool afc_get_is_enable(struct charger_manager *pinfo)
{
	return false;
}

static inline int afc_leave(struct charger_manager *pinfo)
{
    return -ENOTSUPP;
}

static inline void afc_reset(struct afc_dev *afc)
{
}
static inline int afc_plugout_reset(struct charger_manager *pinfo)
{
	return -ENOTSUPP;
}

#endif /* CONFIG_AFC_CHARGER */

#endif /* _AFC_CHSRGER_INTF */
