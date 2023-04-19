/* SPDX-License-Identifier: GPL-2.0 */
/*
 *
 * Copyright (C) 2021 Samsung Electronics Co., Ltd.
 * http://www.samsungsemi.com/
 *
 * Core file for Samsung EUSB Repeater driver
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef __EUSB_REPEATER_H__
#define __EUSB_REPEATER_H__

#include <asm/unaligned.h>
#include <linux/completion.h>
#include <linux/ctype.h>
#include <linux/delay.h>
#include <linux/firmware.h>
#include <linux/gpio.h>
#include <linux/hrtimer.h>
#include <linux/i2c.h>
#include <linux/input.h>
#include <linux/input/mt.h>
#include <linux/interrupt.h>
#include <linux/io.h>
#include <linux/irq.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/of_gpio.h>
#include <linux/platform_device.h>
#include <linux/regulator/consumer.h>
#include <linux/slab.h>
#include <linux/timekeeping.h>
#include <linux/uaccess.h>
#include <linux/vmalloc.h>
//#include <linux/wakelock.h>
#include <linux/workqueue.h>

#if defined(CONFIG_TRUSTONIC_TRUSTED_UI)
#include <linux/t-base-tui.h>
#endif
#ifdef CONFIG_SEC_SYSFS
#include <linux/sec_sysfs.h>
#endif

#include "../../i2c/busses/i2c-exynos5.h"

#define I2C_WRITE_BUFFER_SIZE		(32 - 1)//10
#define TUSB_I2C_RETRY_CNT		3
#define EXYNOS_USB_TUNE_LAST		0xEF

struct eusb_repeater_tune_param {
	char name[32];
	unsigned int reg;
	unsigned int value;
	unsigned int shift;
	unsigned int mask;
};

struct eusb_repeater_data {
	struct device			*dev;
	struct i2c_client		*client;
	struct mutex			i2c_mutex;
	struct eusb_repeater_plat_data	*pdata;
	unsigned int comm_err_count;	/* i2c comm error count */

	/* Tune Parma list */
	struct eusb_repeater_tune_param *tune_param;
	u32 tune_cnt;
	int gpio_eusb_ctrl_sel;
	int ctrl_sel_status;
};

struct eusb_repeater_plat_data {
	int reserved;
};

#endif
