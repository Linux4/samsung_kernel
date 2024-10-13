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
 *
 ************************************************
 * Automatically generated C config: don't edit *
 ************************************************
 */

#ifndef __CTL_ADC_H__
#define __CTL_ADC_H__

/* adc channel definition */
#define ADC_CHANNEL_INVALID	-1
enum adc_channel {
	ADC_CHANNEL_0 = 0,  //ADCI0
	ADC_CHANNEL_1 = 1,  //ADCI1
	ADC_CHANNEL_2 = 2,  //ADCI2
	ADC_CHANNEL_3 = 3,  //ADCI3
	ADC_CHANNEL_PROG = 4,
	ADC_CHANNEL_VBAT = 5,
	ADC_CHANNEL_VCHGSEN = 6,
	ADC_CHANNEL_VCHGBG = 7,
	ADC_CHANNEL_ISENSE = 8,
	ADC_CHANNEL_TPYD = 9,
	ADC_CHANNEL_TPYU = 10,
	ADC_CHANNEL_TPXR = 11,
	ADC_CHANNEL_TPXL = 12,
	ADC_CHANNEL_DCDCCORE = 13,
	ADC_CHANNEL_DCDCARM = 14,
	ADC_CHANNEL_DCDCMEM = 15,
	ADC_CHANNEL_DCDCLDO = 16,
#if defined(CONFIG_ARCH_SC8825)
	ADC_CHANNEL_VBATBK = 17,
	ADC_CHANNEL_HEADMIC = 18,
	ADC_CHANNEL_LDO0 = 19,	/* ldo rf/abb/cama */
	ADC_CHANNEL_LDO1 = 20,	/* ldo v3v/v28/vsim0/vsim1/cammot/sd0/usb/dvdd18/v25 */
	ADC_CHANNEL_LDO2 = 21,	/* ldo camio/camcore/cmmb1v2/cmmb1v8/v18/sd1/sd3/ */
	ADC_MAX = 21,
#elif defined(CONFIG_ARCH_SCX35)
	ADC_CHANNEL_DCDCGPU = 17,
	ADC_CHANNEL_DCDCWRF = 18,
	ADC_CHANNEL_VBATBK = 19,
	ADC_CHANNEL_HEADMIC = 20,
	ADC_CHANNEL_LDO0 = 21,
	ADC_CHANNEL_LDO1 = 22,
	ADC_CHANNEL_LDO2 = 23,
	ADC_CHANNEL_WHTLED = 24,
	ADC_CHANNEL_OTP = 25,
	ADC_CHANNEL_LPLDO0 = 26,	/*SIM0/SIM1/SIM2/EMMCCORE/VDD28/VDD25/USB, Low power mode reference*/
	ADC_CHANNEL_LPLDO1 = 27,	/*CAMD/EMMCIO/VDD18/AVDD18/CAMIO/CLSG, Low power mode reference*/
	ADC_CHANNEL_LPLDO2 = 28,	/*RF0/RF1/RF2/CAMA/SD/CAMMOT, Low power mode reference*/
	ADC_CHANNEL_WHTLED_VFB  = 29,
	ADC_CHANNEL_USBDP = 30,
	ADC_CHANNEL_USBDM = 31,
	ADC_MAX = 31,
#endif
};

struct adc_sample_data {
	int sample_num;		/* from 1 to 15 */
	int sample_bits;	/*0: 10bits mode, 1:12 bits mode */
	int signal_mode;	/*0:resistance,1:capacitance */
	int sample_speed;	/*0:quick mode, 1: slow mode */
	int scale;		/*0:little scale, 1:big scale */
	int hw_channel_delay;	/*0:disable, 1:enable */
	int channel_id;		/*channel id of software, Or dedicatid hw channel number */
	int channel_type;	/*0: software, 1: slow hardware , 2: fast hardware */
	int *pbuf;
};

#ifdef CONFIG_SC_INTERNAL_ADC
extern void sci_adc_init(void __iomem * adc_base);
extern void sci_adc_dump_register(void);

/*
* Use this interface to get adc values and you can config adc sample behavior.
* The max number adc value is 16 now, Pls notice the return value;
*/
extern int sci_adc_get_values(struct adc_sample_data *adc);

/*
 * get adc channel voltage divider ratio.
 */
void sci_adc_get_vol_ratio(unsigned int channel_id, int scale, unsigned int* div_numerators,
			unsigned int* div_denominators );

/*
 * get adc channel voltage divider ratio for multi selected switch
 */
unsigned int sci_adc_get_ratio(unsigned int channel_id, int scale, int mux);

/*
 * Use this interface to get one adc value and this function have set default
 * adc sample behavior.
 */
static inline int sci_adc_get_value(unsigned int channel, int scale)
{
	struct adc_sample_data adc;
	int32_t result[1];

	adc.channel_id = channel;
	adc.channel_type = 0;
	adc.hw_channel_delay = 0;
	adc.pbuf = &result[0];
	adc.sample_bits = 1;
	adc.sample_num = 1;
	adc.sample_speed = 0;
	adc.scale = scale;
	adc.signal_mode = 0;

	if (0 != sci_adc_get_values(&adc)) {
		printk("sci_adc_get_value, return error\n");
		sci_adc_dump_register();
		BUG();
	}

	return result[0];
}


/*
 * get adc channel raw data
 */
int sci_get_adc_data(unsigned int channel, unsigned int scale);

/*
 * Use this interface to get adc value by current sense
 */
int sci_adc_get_value_by_isen(unsigned int channel, int scale, int isen);

#else
static inline void sci_adc_init(void __iomem * adc_base) {}

static inline void sci_adc_dump_register(void) {}

/*
* Use this interface to get adc values and you can config adc sample behavior.
* The max number adc value is 16 now, Pls notice the return value;
*/
static inline int sci_adc_get_values(struct adc_sample_data *adc)
{
	WARN(1, "Not implement\n");
	return 0; /*TODO:should return ENODEV*/
}

/*
 * get adc channel voltage divider ratio.
 */
static inline void sci_adc_get_vol_ratio(unsigned int channel_id, int scale, unsigned int* div_numerators,
			unsigned int* div_denominators ) {}

static inline int sci_adc_get_value(unsigned int channel, int scale)
{
	WARN(1, "Not implement\n");
	return 0; /*TODO:should return ENODEV*/
}

#endif
#endif
