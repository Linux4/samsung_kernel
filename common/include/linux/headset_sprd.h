/*
 * Copyright (C) 2012 Spreadtrum Communications Inc.
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

#ifndef __HEADSET_SPRD_H__
#define __HEADSET_SPRD_H__
#include <linux/switch.h>
#include <linux/input.h>
#include <linux/platform_device.h>

enum {
        BIT_HEADSET_OUT = 0,
        BIT_HEADSET_MIC = (1 << 0),
        BIT_HEADSET_NO_MIC = (1 << 1),
};

struct headset_buttons {
        int adc_min;
        int adc_max;
        int code;
        unsigned int type;	/* input event type (EV_KEY, EV_SW, EV_ABS) */
};

/*
* Modified based on 1.2.0
* sejong123.park, 14/03/05
*/
typedef enum sprd_headset_type {
        HEADSET_NORMAL,
        HEADSET_NO_MIC,
        HEADSET_NORTH_AMERICA,
        HEADSET_APPLE,
        HEADSET_TYPE_MAX,
        HEADSET_TYPE_ERR = -1,
} SPRD_HEADSET_TYPE;

struct sprd_headset_platform_data {
        int gpio_switch;
        int gpio_detect;
        int gpio_button;
        int irq_trigger_level_detect;
        int irq_trigger_level_button;
        struct headset_buttons *headset_buttons;
        int nbuttons;
};

struct sprd_headset {
        int headphone;
        int type;
        int irq_detect;
        int irq_button;
        struct sprd_headset_platform_data *platform_data;
        struct switch_dev sdev;
        struct input_dev *input_dev;
        struct delayed_work work_detect;
        struct workqueue_struct *detect_work_queue;
        struct delayed_work work_button;
        struct workqueue_struct *button_work_queue;
};
#endif
