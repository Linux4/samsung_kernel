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

#ifndef __HEADSET_SPRD_SC2723_H__
#define __HEADSET_SPRD_SC2723_H__
#include <linux/switch.h>
#include <linux/input.h>
#include <linux/platform_device.h>

#define SEC_SYSFS_FOR_FACTORY_TEST
#define SEC_SYSFS_ADC_EARJACK

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

struct sprd_headset_platform_data {
        int gpio_switch;
        int gpio_detect;
        int gpio_button;
        int irq_trigger_level_detect;
        int irq_trigger_level_button;
        int adc_threshold_3pole_detect;
        int adc_threshold_4pole_detect;
        int irq_threshold_buttont;
        int voltage_headmicbias;
        struct headset_buttons *headset_buttons;
        int nbuttons;
        int (*external_headmicbias_power_on)(int);
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
        struct work_struct work_button;
        struct workqueue_struct *button_work_queue;
};
#endif
