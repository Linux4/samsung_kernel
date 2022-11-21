#ifndef __SEC_JACK_H__
#define __SEC_JACK_H__
/*
 *	drivers/switch/sec_jack.h
 *
 *	headset & hook detect driver for Spreadtrum (SAMSUNG KSND COMSTOM)
 *
 *	This program is free software; you can redistribute it and/or modify
 *	it under the terms of the GNU General Public License version 2 as
 *	published by the Free Software Foundation.
*/
#include <linux/switch.h>
#include <linux/input.h>
#include <linux/platform_device.h>
#include <linux/wakelock.h>

#define SEC_HEADSET_ADC_ADJUST

//#define ADPGAR_BYP_SELECT

#define PLUG_CONFIRM_COUNT (1)
#define NO_MIC_RETRY_COUNT (1)
#define ADC_READ_COUNT (1)
#define ADC_READ_LOOP (1)
#define ADC_GND (100)
#define SCI_ADC_GET_VALUE_COUNT (50)

#define EIC3_DBNC_CTRL (0x4C)
#define DBNC_CNT3_VALUE (30)
#define DBNC_CNT3_SHIFT (0)
#define DBNC_CNT3_MASK (0xFFF << DBNC_CNT3_SHIFT)

#define EIC8_DBNC_CTRL (0x60)
#define DBNC_CNT8_VALUE (25)
#define DBNC_CNT8_SHIFT (0)
#define DBNC_CNT8_MASK (0xFFF << DBNC_CNT8_SHIFT)

#define CLK_AUD_HID_EN (BIT(4))
#define CLK_AUD_HBD_EN (BIT(3))

#define HEADMIC_DETECT_BASE (ANA_AUDCFGA_INT_BASE)
#define HEADMIC_DETECT_REG(X) (HEADMIC_DETECT_BASE + (X))

#define HEADMIC_DETECT_GLB_REG(X) (ANA_REGS_GLB_BASE + (X))
#define HDT_EN (BIT(5))
#define AUD_EN (BIT(4))

//no use
#define HEADMIC_BUTTON_BASE (ANA_HDT_INT_BASE)
#define HEADMIC_BUTTON_REG(X) (HEADMIC_BUTTON_BASE + (X))
#define HID_CFG0 (0x0080)
#define HID_CFG2 (0x0088)
#define HID_CFG3 (0x008C)
#define HID_CFG4 (0x0090)
//no use

#define ANA_PMU0 (0x0000)
#define ANA_CDC1 (0x0020)
#define ANA_HDT0 (0x0068)
#define ANA_STS0 (0x0074)


#define AUDIO_PMUR1  (BIT(0))
#define ANA_PMU1 (0x0004)

#define AUDIO_HEADMIC_SLEEP_EN (BIT(6))

#define AUDIO_ADPGAR_BYP_SHIFT (10)
#define AUDIO_ADPGAR_BYP_MASK (0x3 << AUDIO_ADPGAR_BYP_SHIFT)
#define AUDIO_ADPGAR_BYP_NORMAL (0)
#define AUDIO_ADPGAR_BYP_ADC_PGAR1_2_ADCR (1)
#define AUDIO_ADPGAR_BYP_HEADMIC_2_ADCR (2)
#define AUDIO_ADPGAR_BYP_ALL_DISCONNECTED (3)

#define AUDIO_HEAD_SDET_SHIFT (5)
#define AUDIO_HEAD_SDET_MASK (0x3 << AUDIO_HEAD_SDET_SHIFT)
#define AUDIO_HEAD_SDET_2P1 (3)
#define AUDIO_HEAD_SDET_2P3 (2)
#define AUDIO_HEAD_SDET_2P5 (1)
#define AUDIO_HEAD_SDET_2P7 (0)

#define AUDIO_HEAD_INS_VREF_SHIFT (8)
#define AUDIO_HEAD_INS_VREF_MASK (0x3 << AUDIO_HEAD_INS_VREF_SHIFT)
#define AUDIO_HEAD_INS_VREF_2P1 (3)
#define AUDIO_HEAD_INS_VREF_2P3 (2)
#define AUDIO_HEAD_INS_VREF_2P5 (1)
#define AUDIO_HEAD_INS_VREF_2P7 (0)

#define AUDIO_HEAD_SBUT_SHIFT (1)
#define AUDIO_HEAD_SBUT_MASK (0xF << AUDIO_HEAD_SBUT_SHIFT)

#define AUDIO_HEAD_INS_PD (BIT(10))
#define AUDIO_HEADDETECT_PD (BIT(7))

#define AUDIO_V2ADC_EN (BIT(15))
#define AUDIO_HEAD2ADC_SEL_SHIFT (11)
#define AUDIO_HEAD2ADC_SEL_MASK (0xF << AUDIO_HEAD2ADC_SEL_SHIFT)
#define AUDIO_HEAD2ADC_SEL_DISABLE (0)
#define AUDIO_HEAD2ADC_SEL_HEADMIC_IN (1)
#define AUDIO_HEAD2ADC_SEL_HEADSET_L_INT (3)
#define AUDIO_HEAD2ADC_SEL_HEAD_DRO_L (4)

#define AUDIO_HEAD_BUTTON (BIT(11))
#define AUDIO_HEAD_INSERT (BIT(10))
#define AUDIO_HEAD_INSERT_2 (BIT(9))


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


static struct sprd_headset_power {
    struct regulator *head_mic;
    struct regulator *vcom_buf;
    struct regulator *vbo;
} sprd_hts_power;

enum {
	HOOK_RELEASED = 0,
	VOL_UP_RELEASED,
	VOL_DOWN_RELEASED,

	HOOK_VOL_ALL_RELEASED,

	HOOK_PRESSED,
	VOL_UP_PRESSED,
	VOL_DOWN_PRESSED,
};

enum {
    SEC_JACK_NO_DEVICE			   = 0x0,
    SEC_HEADSET_4POLE			   = 0x01 << 0,
    SEC_HEADSET_3POLE			   = 0x01 << 1,
    SEC_UNKNOWN_DEVICE             = 0x01 << 2,
};

struct sec_jack_zone {
    unsigned int adc_high;
    unsigned int delay_us;
    unsigned int check_count;
    unsigned int jack_type;
};

struct sec_jack_buttons_zone {
    unsigned int code;
    unsigned int adc_low;
    unsigned int adc_high;
};

struct sec_jack_platform_data {
    int gpio_detect;
    int gpio_button;

    struct sec_jack_zone jack_zones[4];
    struct sec_jack_buttons_zone *buttons_zones;

    bool det_active_high;
    bool button_active_high;

    int adc_threshold_3pole_detect;
    int adc_threshold_4pole_detect;

    int voltage_headmicbias;
    int nbuttons;

    int (*external_headmicbias_power_on)(int);
};

struct headset_info {
    int irq_detect;
    int irq_button;

    int hook_count;
    int detect_count;

    struct mutex hs_mutex;
    struct mutex dt_mutex;

    struct sec_jack_platform_data *platform_data;

    struct timer_list headset_timer;
    struct timer_list hook_timer;

    struct input_dev *idev;
    struct wake_lock det_wake_lock;

    struct work_struct button_work;
    //struct work_struct detect_work;
    struct delayed_work detect_work;

    struct workqueue_struct *queue;
    struct workqueue_struct *button_queue;

	int pressed_code;
	int hook_vol_status;

    unsigned int cur_jack_type;
    int hset_state;

};

#endif