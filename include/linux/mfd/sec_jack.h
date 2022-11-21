/*
 * Copyright (C) 2008 Samsung Electronics, Inc.
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

#define SEC_SYSFS_FOR_FACTORY_TEST

#if defined(CONFIG_MACH_LT02) || defined(CONFIG_MACH_COCOA7)
/* Enable the Analog Deck Dock device */
#define SEC_USE_ANLOGDOCK_DEVICE
#endif

//#define SEC_HEADSET_ADC_ADJUST  //KSND added

enum {
	SEC_JACK_NO_DEVICE		= 0x0,
	SEC_HEADSET_4POLE		= 0x01 << 0,
	SEC_HEADSET_3POLE		= 0x01 << 1,
	SEC_UNKNOWN_DEVICE		= 0x01 << 2,
};

struct sec_jack_zone {
	unsigned int adc_high;
	unsigned int delay_ms;
	unsigned int check_count;
	unsigned int jack_type;
};

struct sec_jack_buttons_zone {
	unsigned int code;
	unsigned int adc_low;
	unsigned int adc_high;
};

struct sec_jack_platform_data {
	int headset_flag;
	void (*mic_set_power)(int on);

	struct 	sec_jack_zone	*zones;
	struct 	sec_jack_buttons_zone	*buttons_zones;
	int	num_zones;
	int	num_buttons_zones;

	int 	press_release_th;
};

#endif

