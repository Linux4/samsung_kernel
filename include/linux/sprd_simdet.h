/*
 * Copyright (C) 2014 Spreadtrum Communications Inc.
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

#ifndef _SPRD_SIMDET_H
#define _SPRD_SIMDET_H

#include <linux/switch.h>
#include <linux/platform_device.h>

enum {
	SIMDET_OFF = 0,
	SIMDET_ON = 1,
	SIMDET_NR,
};

struct sprd_simdet_platform_data {
        int gpio_detect;
        int irq_trigger_level_detect;
        int delay_time;
};

struct sprd_simdet {
        int irq_detect;
        int status;
        struct sprd_simdet_platform_data *platform_data;
        struct switch_dev sdev;
        struct delayed_work work_detect;
        struct workqueue_struct *detect_work_queue;
};

#endif
