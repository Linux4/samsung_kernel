#ifndef _AFC_H
#define _AFC_H

#include <linux/kernel.h>

#define AFC_COMM_CNT 3
#define AFC_RETRY_MAX 10
#define VBUS_RETRY_MAX 5
#define UI 160
#define WAIT_SPING_COUNT 5
#define RECV_SPING_RETRY 5
#define SPING_MIN_UI 10
#define SPING_MAX_UI 20
#define HV_DISABLE 1
#define HV_ENABLE 0

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
	SET_5V	= 5,
	SET_9V	= 9,
};

struct afc_dev {
	struct device *dev;
	int irq;
	int afc_switch_gpio;
	int afc_data_gpio;
	unsigned int vol;
	unsigned int purpose;
	unsigned int afc_cnt;
	unsigned int afc_error;
	bool afc_disable;
	bool pin_state; /* 1: active, 2: suspend */
	bool afc_used;
	spinlock_t afc_spin_lock;
	struct pinctrl *pinctrl;
	struct pinctrl_state *pin_active;
	struct pinctrl_state *pin_suspend;
	struct work_struct afc_set_voltage_work;
};

extern int afc_set_voltage(int vol);
extern void detach_afc(void);
extern int get_afc_mode(void);
#endif /* _AFC_H */
