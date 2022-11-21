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
#include <linux/timer.h> //KSND
#include <linux/time.h>  //KSND

//KSND
#define SEC_USE_HS_SYSFS
//#define SEC_SYSFS_ADC_EARJACK

//====================  debug  ====================
#define ENTER \
do{ if(debug_level >= 1) printk(KERN_INFO "[SPRD_HEADSET_DBG][%d] func: %s  line: %04d\n", adie_type, __func__, __LINE__); }while(0)

#define PRINT_DBG(format,x...)  \
do{ if(debug_level >= 1) printk(KERN_INFO "[SPRD_HEADSET_DBG][%d] " format, adie_type, ## x); }while(0)

#define PRINT_INFO(format,x...)  \
do{ printk(KERN_INFO "[SPRD_HEADSET_INFO][%d] " format, adie_type, ## x); }while(0)

#define PRINT_WARN(format,x...)  \
do{ printk(KERN_INFO "[SPRD_HEADSET_WARN][%d] " format, adie_type, ## x); }while(0)

#define PRINT_ERR(format,x...)  \
do{ printk(KERN_ERR "[SPRD_HEADSET_ERR][%d] func: %s  line: %04d  info: " format, adie_type, __func__, __LINE__, ## x); }while(0)
//====================  debug  ====================

#define headset_reg_read(addr)	\
    do {	\
	sci_adi_read(addr);	\
} while(0)

#define headset_reg_set_val(addr, val, mask, shift)  \
    do {    \
        uint32_t temp;    \
        temp = sci_adi_read(addr);  \
        temp = (temp & (~mask)) | ((val) << (shift));   \
        sci_adi_raw_write(addr, temp);    \
    } while(0)

#define headset_reg_clr_bit(addr, bit)   \
    do {    \
        uint32_t temp;    \
        temp = sci_adi_read(addr);  \
        temp = temp & (~bit);   \
        sci_adi_raw_write(addr, temp);  \
    } while(0)

#define headset_reg_set_bit(addr, bit)   \
    do {    \
        uint32_t temp;    \
        temp = sci_adi_read(addr);  \
        temp = temp | bit;  \
        sci_adi_raw_write(addr, temp);  \
    } while(0)


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
        int adc_threshold_gnd_average;
        int adc_threshold_left_average;
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
        //struct delayed_work work_detect;
        struct work_struct work_detect;
        struct workqueue_struct *detect_work_queue;
        struct work_struct work_button;
        struct workqueue_struct *button_work_queue;

        struct timespec ts;             /* Get Current time for KSND */
        struct timespec ts_after;       /* Get Current time After Event */
};
#endif
