#ifndef __LINUX_MSG2138_TS_H__
#define __LINUX_MSG2138_TS_H__

#include <linux/i2c.h>
#include <linux/input.h>

#define TS_DEBUG_MSG			0
#define MSG2138_UPDATE			1

#define TS_WIDTH_MAX			480
#define	TS_HEIGHT_MAX			854

#define MSG2138_BUS_NUM			2
#define MSG2138_TS_NAME			"msg2138"
#define MSG2138_TS_ADDR			0x26
#define MSG2138_FW_ADDR			0x62
#define MSG2138_FW_UPDATE_ADDR		0x49


#define TPD_OK				0
#define TPD_REG_BASE			0x00
#define TPD_SOFT_RESET_MODE		0x01
#define TPD_OP_MODE			0x00
#define TPD_LOW_PWR_MODE		0x04
#define TPD_SYSINFO_MODE		0x10

#define GET_HSTMODE(reg)  ((reg & 0x70) >> 4)  /* in op mode or not */
#define GET_BOOTLOADERMODE(reg) ((reg & 0x10) >> 4)  /* in bl mode */


struct ts_event {
	u16	x1;
	u16	y1;
	u16	x2;
	u16	y2;
	u8  touch_point;
};

struct msg2138_ts_platform_data {
	int irq_gpio_number;
	int reset_gpio_number;
	int (*set_power)(int on);
	void (*set_reset)(void);
	int abs_x_max;
	int abs_y_max;
	int abs_flag;
	int virtual_key;
	int (*set_virtual_key)(struct input_dev *);
};

struct msg2138_ts_data {
	struct input_dev	*input_dev;
	struct i2c_client	*client;
	struct ts_event		event;
	struct work_struct	pen_event_work;
	struct workqueue_struct	*ts_workqueue;
	struct msg2138_ts_platform_data *platform_data;
	struct regulator *v_tsp;
};

struct TouchScreenInfo_t {
	unsigned char nTouchKeyMode;
	unsigned char nTouchKeyCode;
};

#endif
