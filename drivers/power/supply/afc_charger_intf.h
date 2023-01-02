/*HS03s for SR-AL5625-01-249 by wenyaqi at 20210425 start*/
#ifndef _AFC_CHARGER_INTF
#define _AFC_CHARGER_INTF

#include <linux/kernel.h>

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
#define ALGO_LOOP_MAX 20
//define function macro
#define NO_COMM_CHECK 0

/* afc parameter */
#define AFC_START_BATTERY_SOC 0
#define AFC_STOP_BATTERY_SOC 95
#define AFC_PRE_INPUT_CURRENT 500000
#define AFC_CHARGER_INPUT_CURRENT 1600000
#define AFC_CHARGER_CURRENT 2700000
#define AFC_ICHG_LEVEL_THRESHOLD 1000000
#define AFC_MIN_CHARGER_VOLTAGE 4200000
#define AFC_MAX_CHARGER_VOLTAGE 10050000


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

struct afc_dev {
	int afc_switch_gpio;
	int afc_data_gpio;
	unsigned int vol;
	unsigned int afc_error;
	bool afc_init_ok;
	bool is_enable;
	bool pin_state; /* true: active, false: suspend */
#if defined(CONFIG_DRV_SAMSUNG)
	int afc_disable;
	struct device *switch_device;
#endif
	spinlock_t afc_spin_lock;
	struct pinctrl *pinctrl;
	struct pinctrl_state *pin_active;
	struct pinctrl_state *pin_suspend;
	bool is_connect;
	bool to_check_chr_type;
	struct wakeup_source *suspend_lock;
	int ta_vchr_org;
	struct mutex access_lock;
	int afc_loop_algo;
	bool afc_retry_flag;
};

#endif /* _AFC_CHSRGER_INTF */
/*HS03s for SR-AL5625-01-249 by wenyaqi at 20210425 end*/