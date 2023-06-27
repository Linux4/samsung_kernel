/*
 * 2010 - 2012 Goodix Technology.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be a reference
 * to you, when you are integrating the GOODiX's CTP IC into your system,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * History:
 *	V1.0: 2012/08/31, first release
 *	V1.2: 2012/10/15
 *	V1.4: 2012/12/12
 *	V1.5: 2013/02/21, update format and style, add GTP_SLEEP
 *			  Modified by Albert Wang <twang13@marvell.com>
 *	V1.6: 2013/03/12, remove early suspend, add pm runtime
 *			  Modified by Albert Wang <twang13@marvell.com>
 *	V1.7: 2013/03/21, remove platform dependence from driver
 *			  Modified by Albert Wang <twang13@marvell.com>
 */

#ifndef _LINUX_I2C_GT9XX_TOUCH_H
#define	_LINUX_I2C_GT9XX_TOUCH_H

#include <linux/kernel.h>
#include <linux/hrtimer.h>
#include <linux/i2c.h>
#include <linux/input.h>
#include <linux/module.h>
#include <linux/delay.h>
#include <linux/i2c.h>
#include <linux/proc_fs.h>
#include <linux/string.h>
#include <linux/vmalloc.h>
#include <linux/interrupt.h>
#include <linux/uaccess.h>
#include <linux/io.h>
#include <mach/gpio.h>
#include <mach/mfp-pxa988.h>

struct goodix_ts_data {
	spinlock_t irq_lock;
	struct i2c_client *client;
	struct input_dev *input_dev;
	struct hrtimer timer;
	struct work_struct work;
	struct gtp_platform_data *data;
	s32 irq_is_disable;
	s32 use_irq;
	u16 abs_x_max;
	u16 abs_y_max;
	u8 max_touch_num;
	u8 int_trigger_type;
	u8 green_wake_mode;
	u8 chip_type;
	u8 enter_update;
	u8 gtp_is_suspend;
	u8 gtp_rawdiff_mode;
	u8 gtp_cfg_len;
};

/*
 * PART1: ON / OFF define
 */
#define GTP_CUSTOM_CFG		1
#define GTP_DRIVER_SEND_CFG	1
#define GTP_HAVE_TOUCH_KEY	1
#define GTP_POWER_CTRL_SLEEP	1
#define GTP_AUTO_UPDATE		0
#define GTP_UPDATE_FUC		1
#define GTP_CHANGE_X2Y		0
#define GTP_ESD_PROTECT		0
#define GTP_CREATE_WR_NODE	1
#define GTP_ICS_SLOT_REPORT	1
#define GTP_DEBUG_ON		0
#define GTP_DEBUG_ARRAY_ON	0
#define GTP_DEBUG_FUNC_ON	0

/*
 * PART2: TODO define
 */
/*
 * STEP_1(REQUIRED): Change config table
 * TODO: puts the config info corresponded to your TP here,
 *       the following is just a sample config,
 *       send this config should cause the chip cannot work normally
 */
/*
 * default or float
 */
#define CTP_CFG_GROUP1 { \
	0x42, 0x38, 0x04, 0x80, 0x07, 0x05, 0x05, 0x03, 0x02, 0xC8, 0x28, 0x08,\
	0x64, 0x3C, 0x03, 0x05, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,\
	0x00, 0x00, 0x00, 0x8B, 0x2B, 0x0D, 0x20, 0x22, 0xE6, 0x1B, 0x00, 0x00,\
	0x00, 0x9A, 0x03, 0x2D, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,\
	0x00, 0x00, 0x00, 0x64, 0xD2, 0x94, 0x85, 0x02, 0x08, 0x00, 0x00, 0x1B,\
	0x18, 0x23, 0x58, 0x1B, 0x24, 0x53, 0x22, 0x21, 0xD5, 0x26, 0x22, 0x30,\
	0x2A, 0x24, 0x00, 0x00, 0x30, 0x88, 0xE0, 0x00, 0x57, 0x50, 0x32, 0xCC,\
	0xCC, 0x06, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x1B, 0x14, 0x0D, 0x14,\
	0x00, 0x00, 0x01, 0x00, 0x00, 0x03, 0x14, 0x50, 0x00, 0x00, 0x00, 0x00,\
	0x00, 0x00, 0x00, 0x00, 0x1A, 0x18, 0x16, 0x14, 0x12, 0x10, 0x0E, 0x0C,\
	0x0A, 0x08, 0x06, 0x04, 0x02, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,\
	0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0x00, 0x01,\
	0x02, 0x04, 0x06, 0x07, 0x08, 0x09, 0x0A, 0x0C, 0x0E, 0x1D, 0x1E, 0x1F,\
	0x20, 0x22, 0x24, 0x25, 0x26, 0x28, 0x29, 0x2A, 0xFF, 0xFF, 0xFF, 0xFF,\
	0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,\
	0xFF, 0xFF, 0xFF, 0xFF, 0xCE, 0x01\
}

/*
 * TODO puts your group2 config info here, if need.
 * VDDIO
 */
#define CTP_CFG_GROUP2 { \
}

/*
 * TODO puts your group3 config info here, if need.
 * GND
 */
#define CTP_CFG_GROUP3 { \
}

/*
 * STEP_2(optional): Change I/O define & I/O operation mode.
 */
#define GTP_RST_PORT(pin)	mfp_to_gpio(pin)
#define GTP_INT_PORT(pin)	mfp_to_gpio(pin)

#define GTP_GPIO_AS_INPUT(pin)		gpio_direction_input(pin)
#define GTP_GPIO_OUTPUT(pin, level)	gpio_direction_output(pin, level)
#define GTP_GPIO_AS_INT(pin)		GTP_GPIO_AS_INPUT(pin)
#define GTP_GPIO_REQUEST(pin, label)	gpio_request(pin, label)
#define GTP_GPIO_FREE(pin)		gpio_free(pin)

#define GTP_IRQ_TAB	{ \
				IRQ_TYPE_EDGE_RISING,\
				IRQ_TYPE_EDGE_FALLING,\
				IRQ_TYPE_LEVEL_LOW,\
				IRQ_TYPE_LEVEL_HIGH\
			}

/*
 * STEP_3(optional): Custom set some config by themself, if need.
 */
#if GTP_CUSTOM_CFG
#define GTP_MAX_HEIGHT	1920
#define GTP_MAX_WIDTH	1080
#define GTP_INT_TRIGGER 0	/* 0: Rising; 1: Falling */
#else
#define GTP_MAX_HEIGHT	4096
#define GTP_MAX_WIDTH	4096
#define GTP_INT_TRIGGER 1
#endif

#define GTP_MAX_TOUCH		5
#define GTP_ESD_CHECK_CIRCLE	2000

/*
 * STEP_4(optional): If this project have touch key, Set touch key config.
 */
#if GTP_HAVE_TOUCH_KEY
#define GTP_KEY_TAB	{ KEY_MENU, KEY_HOME, KEY_BACK }
#endif

/*
 * PART3: OTHER define
 */
#define GTP_DRIVER_VERSION	"V1.7<2013/03/21>"
#define GTP_I2C_NAME		"Goodix-TS-9XX"
#define GTP_POLL_TIME		10
#define GTP_ADDR_LENGTH		2
#define GTP_CONFIG_MAX_LENGTH	240
#define FAIL			0
#define SUCCESS			1

/*
 * Register define
 */
#define GTP_READ_COOR_ADDR	0x814E
#define GTP_REG_SLEEP		0x8040
#define GTP_REG_SENSOR_ID	0x814A
#define GTP_REG_CONFIG_DATA	0x8047
#define GTP_REG_VERSION		0x8140

#define RESOLUTION_LOC		3
#define TRIGGER_LOC		8

/*
 * Log define
 */
#define GTP_INFO(fmt, arg...)	printk(KERN_INFO "<<-GTP-INFO->> "fmt"\n",\
									##arg)
#define GTP_ERROR(fmt, arg...)	printk(KERN_ERR "<<-GTP-ERROR->> "fmt"\n",\
									##arg)
#define GTP_DEBUG(fmt, arg...)\
	do { \
		if (GTP_DEBUG_ON)\
			printk(KERN_DEBUG "<<-GTP-DEBUG->> [%d]"fmt"\n",\
				__LINE__, ##arg);\
	} while (0)
#define GTP_DEBUG_ARRAY(array, num)\
	do { \
		s32 i;\
		u8 *a = (array);\
		if (GTP_DEBUG_ARRAY_ON) { \
			printk(KERN_DEBUG "<<-GTP-DEBUG-ARRAY->>\n");\
			for (i = 0; i < (num); i++) { \
				printk(KERN_INFO "%02x   ", (a)[i]);\
				if ((i + 1) % 10 == 0)\
					printk(KERN_INFO "\n");\
			} \
			printk(KERN_INFO "\n");\
		} \
	} while (0)
#define GTP_DEBUG_FUNC()\
	do { \
		if (GTP_DEBUG_FUNC_ON)\
			printk(KERN_DEBUG \
				"<<-GTP-FUNC->> Func: %s @Line: %d\n",\
				__func__, __LINE__);\
	} while (0)
#define GTP_SWAP(x, y)\
	do { \
		typeof(x) (z) = (x);\
		(x) = (y);\
		(y) = (z);\
	} while (0)

/*
 * According to Documentation/timers/timers-howto.txt
 * we should choose sleep family functions according to the time:
 * <~10us	udelay
 * 10us ~ 20ms	usleep_range
 * >~20ms	msleep
 */
#define GTP_MSLEEP(x)\
	do { \
		unsigned int ms = (x);\
		if (ms >= 20)\
			msleep(ms);\
		else\
			usleep_range(ms * 1000,	ms * 1000 + 100);\
	} while (0)

/*
 * PART4: UPDATE define
 */
/*
 * Error number
 */
#define ERROR_NO_FILE		2	/* ENOENT */
#define ERROR_FILE_READ		23	/* ENFILE */
#define ERROR_FILE_TYPE		21	/* EISDIR */
#define ERROR_GPIO_REQUEST	4	/* EINTR */
#define ERROR_I2C_TRANSFER	5	/* EIO */
#define ERROR_NO_RESPONSE	16	/* EBUSY */
#define ERROR_TIMEOUT		110	/* ETIMEDOUT */

/*
 * Declare gt9xx_platform data
 */
struct gtp_platform_data {
	int reset;	/* define GTP reset pin */
	int irq;	/* define GTP irq pin */
};

/*
 * Declare extern variables and functions
 */
extern u16 show_len;
extern u16 total_len;
extern struct i2c_client *i2c_connect_client;

s32 gtp_i2c_read(struct i2c_client *client, u8 *buf, s32 len);
s32 gtp_i2c_write(struct i2c_client *client, u8 *buf, s32 len);
void gtp_reset_guitar(struct i2c_client *client, s32 ms);
s32 gtp_send_cfg(struct i2c_client *client);
void gtp_irq_enable(struct goodix_ts_data *ts);
void gtp_irq_disable(struct goodix_ts_data *ts);
#if GTP_UPDATE_FUC
s32 gup_enter_update_mode(struct i2c_client *client);
void gup_leave_update_mode(struct i2c_client *client);
s32 gup_update_proc(void *dir);
#endif
#if GTP_AUTO_UPDATE
u8 gup_init_update_proc(struct goodix_ts_data *ts);
#endif
#if GTP_CREATE_WR_NODE
s32 init_wr_node(struct i2c_client *client);
void uninit_wr_node(void);
#endif

#endif /* _LINUX_I2C_GT9XX_TOUCH_H */
