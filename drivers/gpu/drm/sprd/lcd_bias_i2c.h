/*:
 * Copyright (C) 2019 Spreadtrum Communications Inc.
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
*/

#ifndef _LCD_BIAS_I2C_H_
#define _LCD_BIAS_I2C_H_

#include <linux/delay.h>
#include <linux/i2c.h>
#include <linux/module.h>
#include <linux/mutex.h>
#include <linux/types.h>
#include <linux/stat.h>

#define TAG "[LCD_BIAS]"

#define SM_ERR(fmt, ...) \
		pr_err(TAG " %s() " fmt "\n", __func__, ##__VA_ARGS__)
#define SM_WARN(fmt, ...) \
		pr_warn(TAG " %s() " fmt "\n", __func__, ##__VA_ARGS__)
#define SM_INFO(fmt, ...) \
		pr_info(TAG " %s() " fmt "\n", __func__, ##__VA_ARGS__)



extern int lcd_bias_set_avdd_avee(void);

#endif
