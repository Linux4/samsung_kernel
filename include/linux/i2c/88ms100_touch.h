 /*
 * Marvell 88MS100 Touch Controller Driver.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

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
#include <linux/gpio.h>
#include <linux/seq_file.h>

struct ms100_ts_data {
	spinlock_t irq_lock;
	struct i2c_client *client;
	struct input_dev *input_dev;
	struct hrtimer timer;
	struct work_struct work;
	struct ms100_platform_data *data;
	struct proc_dir_entry *proc_file;
	s32 irq_is_disable;
	s32 use_irq;
	u16 abs_x_max;
	u16 abs_y_max;
	u8 max_touch_num;
	u8 int_trigger_type;
	u8 green_wake_mode;
	u8 chip_type;
	u8 enter_update;
	u8 ms100_is_suspend;
};

struct ms100_platform_data {
	int reset;      /* define MS100 reset pin */
	int irq;        /* define MS100 irq pin */
	int (*set_power)(int on);
	int ms100_max_height;
	int ms100_max_width;
	int (*set_virtual_key)(struct input_dev *);
};

void ms100_reset_guitar(struct i2c_client *client, s32 ms);

#define MS100_HAVE_TOUCH_KEY 1
#define MS100_USE_FW_HEX 1
#define MS100_USE_FW_BIN 0
#define MS100_DEBUG_ON 1
#define MS100_INT_TRIGGER 1
#define MS100_LOSE_POWER_SLEEP 1
#define MS100_ICS_SLOT_REPORT 1
#define MS100_DEBUG_FS 0
#define MS100_DRIVER_SEND_CFG 0

#define READ_ONEBYTE 0
#define READ_ALL 0
#define READ 0

#define MS100_ADDR_LENGTH 2
#define MS100_POLL_TIME	10

#define MS100_MAX_TOUCH	10

#define MS100_READ_COOR_ADDR      0x0000
#define MS100_REG_RAWDATA         0x2000
#define MS100_REG_SLEEP           0x0080


#if MS100_HAVE_TOUCH_KEY
#define MS100_KEY_TAB     { KEY_BACK, KEY_HOMEPAGE, KEY_MENU }
#endif


#define MS100_INFO(fmt, arg...)   pr_info("<<-MS100-INFO->> "fmt"\n", ##arg)
#define MS100_ERROR(fmt, arg...)  pr_err("<<-MS100-ERROR->> "fmt"\n", ##arg)
#define MS100_DEBUG(fmt, arg...)\
	do { \
		if (MS100_DEBUG_ON)\
			pr_debug("<<-MS100-DEBUG->> [%d]"fmt"\n",\
			   __LINE__, ##arg);\
	} while (0)

#define MS100_SWAP(x, y)\
	do { \
		typeof(x) (z) = (x);\
		(x) = (y);\
		(y) = (z);\
	} while (0)


#define MS100_MSLEEP(x)\
	do { \
		unsigned int vms = (x);\
		if (vms >= 20)\
			msleep(vms);\
		else\
			usleep_range(vms * 1000, vms * 1000 + 100);\
	} while (0)

#define MS100_IRQ_TAB     { \
	IRQ_TYPE_EDGE_RISING,\
	IRQ_TYPE_EDGE_FALLING,\
	IRQ_TYPE_LEVEL_LOW,\
	IRQ_TYPE_LEVEL_HIGH\
}

#define MS100_PROC_ENABLE
#define MS100_PROC_ENTRY "driver/88ms100_reg"
#define MS100_I2C_NAME	"88MS100_TOUCH"


