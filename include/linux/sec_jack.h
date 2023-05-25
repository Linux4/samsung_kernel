/*
 * Copyright (C) 2012 Samsung Electronics, Inc.
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#ifndef __ASM_ARCH_SEC_HEADSET_H
#define __ASM_ARCH_SEC_HEADSET_H
#include <sound/soc.h>
#include <sound/jack.h>

#ifdef __KERNEL__

enum {
	SEC_JACK_NO_DEVICE				= 0x0,
	SEC_HEADSET_4POLE				= 0x01 << 0,
	SEC_HEADSET_3POLE				= 0x01 << 1,
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
	int	det_gpio;
	int	send_end_gpio;
	int	ear_micbias_gpio;
	int	fsa_en_gpio;
	bool	det_active_high;
	bool	send_end_active_high;
	struct qpnp_vadc_chip		*vadc_dev;
	struct sec_jack_zone jack_zones[4];
	struct sec_jack_buttons_zone jack_buttons_zones[4];
	int mpp_ch_scale[3];
	struct pinctrl *jack_pinctrl;
	int btn_adc_read_count;
};

#if defined (SEC_USE_SOC_JACK_API)
#define SEC_JACK_BUTTON_MASK (SND_JACK_BTN_0 | SND_JACK_BTN_1 | SND_JACK_BTN_2)
extern int sec_jack_soc_init(struct snd_soc_card *card);
#endif

#ifdef CONFIG_DYNAMIC_MICBIAS_CONTROL
#define MIC_BIAS_V2P20V		0
#define MIC_BIAS_V2P80V		1
/* Dynamic Control Ear Mic Bias of WCD Codec */
extern void msm8x16_wcd_dynamic_control_micbias(int voltage);
extern int is_mic_enable(void);
#endif

#endif

#endif
