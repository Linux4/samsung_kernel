#ifndef _AFC_H
#define _AFC_H

#include <linux/kernel.h>

/* F52 code for QN3979-302 Add AFC feature by gaochao at 2021/02/26 start */
// #define AFC_DETECT_RESTRICTION
/* F52 code for QN3979-302 Add AFC feature by gaochao at 2021/02/26 end */

#define AFC_COMM_CNT 3
#define AFC_RETRY_MAX 10
#define VBUS_RETRY_MAX 5
#if !defined(AFC_DETECT_RESTRICTION)
#define UI 160
#endif
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
// #undef AFC_DETECT_RESTRICTION

#if defined(AFC_DETECT_RESTRICTION)
#include <linux/hrtimer.h>

#define AFC_START_TIME_NS				10 * 1000 * 1000
#define BASE 			(40  * 1000 ) 		/* 40 us */
#define UI				(BASE * 4) 			/* 160 us */
#define AFC_MS		(1000 * 1000)		// 1ms

typedef enum {
	AFC_IDLE,  // 0
	AFC_MPING_START, // 1
	AFC_MPING_END, // 2
	AFC_WAIT_SPING, // 3
	AFC_SPING_START, // 4
	AFC_SPING_END, // 5
	AFC_RQUEST_CTRL, // 6
	AFC_START_TRANSFER, // 7
	AFC_SEND_DATA, // 8
	AFC_END_TRANSFER, // 9
	AFC_SEND_END_MPING, // 10
	AFC_WAIT_DATA,  // 11
	AFC_RECV_DATA_START, // 12
	AFC_RECV_DATA_END, // 13
	AFC_SEND_CFM_MPING_START, // 14
	AFC_SEND_CFM_MPING_END, // 15
	AFC_WAIT_CMF_SPING, // 16
	AFC_RECV_CMF_SPING_START, // 17
	AFC_RECV_CMF_SPING_END, // 18
	AFC_FINISH,  // 19
	AFC_OK, // 20
	AFC_SM_IDLE, // 21
	AFC_ERROR, // 22
} afc_state_t;

typedef struct {
	afc_state_t state;
	unsigned char data[10];
	unsigned char parity;
	int dataLen;
	int dataOffset;
	int dataSendCount;
	int count;
	int retry_count;
} afc_t;

enum afc_data_gpio {
    AFC_DATA_GPIO_OUT_LOW = 0,
    AFC_DATA_GPIO_OUT_HIGH,
};

#else

/* F52 code for QN3979-302 Add AFC feature by gaochao at 2021/02/26 start */
// #define UI (160)
/* F52 code for QN3979-302 Add AFC feature by gaochao at 2021/02/26 end */

#endif		/* AFC_DETECT_RESTRICTION */


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
	#if defined(AFC_DETECT_RESTRICTION)
	struct hrtimer afc_detection_timer;
	afc_t afc;
	struct work_struct afc_ta_set_work;
	#else
	struct work_struct afc_set_voltage_work;
	#endif
};

extern int afc_set_voltage(int vol);
extern void detach_afc(void);
/* F52 code for SR-QN3979-01-805 SEC_FLOATING_FEATURE_BATTERY_SUPPORT_HV_DURING_CHARGING by gaochao at 2021/04/05 start */
void detach_afc_hv_charge(void);
/* F52 code for SR-QN3979-01-805 SEC_FLOATING_FEATURE_BATTERY_SUPPORT_HV_DURING_CHARGING by gaochao at 2021/04/05 end */
extern int get_afc_mode(void);
#endif /* _AFC_H */
