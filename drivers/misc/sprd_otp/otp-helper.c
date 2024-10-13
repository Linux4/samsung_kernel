/*
 * Copyright (C) 2014 Spreadtrum Communications Inc.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/stat.h>
#include <linux/err.h>
#include "sprd_otp.h"

#define BLK_UID_HIGH                    ( 0 )
#define BLK_UID_LOW                     ( 1 )
#define BLK_ADC_DETA                    ( 7 )
#define BLK_CCCV_DETA                   ( 9 )

#define BLK_FGU_DETA_ABC                ( 8 )
#define BLK_FGU_DETA_D                  ( 9 )

#define BLK_WIDTH_OTP_EMEMORY			( 8 ) /* bit counts */
#define BLK_ADC_DETA_ABC_OTP			( 8 ) /* start block for ADC otp delta */

u32 __weak __ddie_efuse_read(int blk_index)
{
	return 0;
}
/*---------------------------------------------------------------------------*
 *				Efuse public API functions for other kernel modules			 *
 *---------------------------------------------------------------------------*/
u64 sci_efuse_get_uid(void)
{
	u64 uid_hi, uid_lo;

	uid_hi = __ddie_efuse_read(BLK_UID_HIGH);
	uid_lo = __ddie_efuse_read(BLK_UID_LOW);

	/* fill system serial number */
	return ((uid_hi << 32ull) | uid_lo);
}

EXPORT_SYMBOL_GPL(sci_efuse_get_uid);

#define BASE_ADC_P0				711	//3.6V
#define BASE_ADC_P1				830	//4.2V
#define VOL_P0					3600
#define VOL_P1					4200
#define ADC_DATA_OFFSET			128
int sci_efuse_calibration_get(unsigned int *p_cal_data)
{
	unsigned int deta = 0;
	unsigned short adc_temp = 0;

#if defined(CONFIG_ADIE_SC2723) || defined(CONFIG_ADIE_SC2723S)
	/* verify the otp data of ememory written or not */
	adc_temp = (__adie_efuse_read(0) & BIT(7));
	if (adc_temp)
		return 0;

	deta = __adie_efuse_read_bits(BLK_ADC_DETA_ABC_OTP * BLK_WIDTH_OTP_EMEMORY, 16);
#elif defined(CONFIG_ARCH_SCX30G) || defined(CONFIG_ARCH_SCX35L)
	deta = __ddie_efuse_read(BLK_ADC_DETA);
	WARN_ON(!(deta & BIT(31))); /* BIT 31 is protected bit */
#else
	#warning "AuxADC CAL DETA need fixing"
#endif

	pr_info("%s() get efuse block %u, deta: 0x%08x\n", __func__, BLK_ADC_DETA, deta);

	deta &= 0xFFFFFF; /* get BIT0 ~ BIT23) in block 7 */

	if ((!deta) || (p_cal_data == NULL)) {
		return 0;
	}
	//adc 3.6V
	adc_temp = ((deta >> 8) & 0x00FF) + BASE_ADC_P0 - ADC_DATA_OFFSET;
	p_cal_data[1] = (VOL_P0) | ((adc_temp << 2) << 16);

	//adc 4.2V
	adc_temp = (deta & 0x00FF) + BASE_ADC_P1 - ADC_DATA_OFFSET;
	p_cal_data[0] = (VOL_P1) | ((adc_temp << 2) << 16);

	return 1;
}

EXPORT_SYMBOL_GPL(sci_efuse_calibration_get);

int sci_efuse_cccv_cal_get(unsigned int *p_cal_data)
{
	unsigned int data;

	data = __ddie_efuse_read(BLK_CCCV_DETA);
	data &= ~(1 << 31);

	pr_info("sci_efuse_cccv_cal_get data:0x%x\n", data);

	if ((!data) || (p_cal_data == NULL)) {
		return 0;
	}

	*p_cal_data = (data >> 24) & 0x003F;	/*block 9, bit 29..24 */

	return 1;
}

EXPORT_SYMBOL_GPL(sci_efuse_cccv_cal_get);

int sci_efuse_fgu_cal_get(unsigned int *p_cal_data)
{
#if defined(CONFIG_ADIE_SC2723) || defined(CONFIG_ADIE_SC2723S)
	unsigned int data,blk0;

	blk0 = __adie_efuse_read(0);

	if (blk0 & (1 << 7)) {
		return 0;
	}

	data = __adie_efuse_read_bits(BITSINDEX(12, 0), 9);
	printk("sci_efuse_fgu_cal_get 4.2 data data:0x%x\n", data);
	p_cal_data[0] = (data + 6963) - 4096 - 256;	//4.0V or 4.2V

	data = __adie_efuse_read_bits(BITSINDEX(14, 2), 6);
	printk("sci_efuse_fgu_cal_get 3.6 data data:0x%x\n", data);
	p_cal_data[1] = p_cal_data[0] - 410 - data + 16;	//3.6V

	data = __adie_efuse_read_bits(BITSINDEX(13, 1), 9);
	printk("sci_efuse_fgu_cal_get 0 data data:0x%x\n", data);
	p_cal_data[2] = (((data) & 0x1FF) + 8192) - 256;

	return 1;
#else
	unsigned int data;

	data = __ddie_efuse_read(BLK_FGU_DETA_ABC);
	data &= ~(1 << 31);

	pr_info("sci_efuse_fgu_cal_get ABC data:0x%x\n", data);

	if ((!data) || (p_cal_data == NULL)) {
		return 0;
	}

	p_cal_data[0] = (((data >> 16) & 0x1FF) + 6963) - 4096 - 256;   //4.0V or 4.2V
	p_cal_data[1] = p_cal_data[0] - 410 - ((data >> 25) & 0x3F) + 16;  //3.6V
	p_cal_data[2] = (((data) & 0x1FF) + 8192) - 256;

	return 1;
#endif
}

EXPORT_SYMBOL_GPL(sci_efuse_fgu_cal_get);

/*
 * sci_efuse_get_apt_cal - read apt data saved in efuse
 * @pdata: apt data pointer for dcdcwpa (unit: uV)
 * pdata[0] -> 3v
 * pdata[1] -> 0.8v
 * @num: the length of apt data
 *
 * retruns 0 if success, else
 * returns negative number.
 */
#define APT_CAL_DATA_BLK		( 10 )
#define OFFSET2VOL(p, uv)		( ((p) - 128) * (uv) / 256)
int sci_efuse_get_apt_cal(unsigned int *pdata, int num)
{
	int i = 0;
	u32 efuse_data = 0;
	u8 *offset = (u8 *) & efuse_data;
	const unsigned int vol_ref[2] = {
		3000000,	//3v
		800000,		//0.8v
	};

	/* Fixme: dcdcwpa only support two points(3.6v / 0.8v) voltage calibration */
	BUG_ON(num > 2);

	efuse_data = __ddie_efuse_read(APT_CAL_DATA_BLK);

	pr_info("%s efuse data: 0x%08x\n", __func__, efuse_data);

	WARN_ON(!(efuse_data & BIT(31))); /* BIT 31 is protected bit */
	efuse_data &= 0xFFFF; /* get BIT0 ~ BIT15 in block 10 */

	if (!(efuse_data) || (!pdata)) {
		return -1;
	}

	printk("%s: ", __func__);
	for (i = 0; i < num; i++) {
		pdata[i] = (unsigned int)OFFSET2VOL(offset[i], vol_ref[i]);
		printk("(%duV : %08d), ", vol_ref[i], pdata[i]);
	}
	printk("\n");

	return 0;
}

EXPORT_SYMBOL_GPL(sci_efuse_get_apt_cal);

/*
 * sci_efuse_get_cal - read 4 point adc calibration data saved in efuse
 * @pdata: adc data pointer
 * pdata[0] -> 4.2v
 * pdata[1] -> 3.6v
 * pdata[2] -> 0.4v
 * @num: the length of adc data
 *
 * retruns 0 if success, else
 * returns negative number.
 */
#define BLK_ADC_DETA_ABC					( 7 )
#define BLK_ADC_DETA_D						( 9 )
#define DELTA2ADC(_delta_, _Ideal_) 		( ((_delta_) + (_Ideal_) - 128) << 2 )
#define ADC_VOL(_delta_, _Ideal_, vol)		( (DELTA2ADC(_delta_, _Ideal_) << 16) | (vol) )

int sci_efuse_get_cal(unsigned int *pdata, int num)
{
	int i;
	u32 efuse_data = 0;
	u8 *delta = (u8 *) & efuse_data;
	const u16 ideal[3][2] = {
		{4200, 830},	/* FIXME: efuse only support 10bits */
		{3600, 711},
		{400, 79},
	};

#if defined(CONFIG_ADIE_SC2723) || defined(CONFIG_ADIE_SC2723S)
	/* verify the otp data of ememory written or not */
	efuse_data = (__adie_efuse_read(0) & BIT(7));
	if (efuse_data)
		return -2;

	efuse_data = __adie_efuse_read_bits(BLK_ADC_DETA_ABC_OTP * BLK_WIDTH_OTP_EMEMORY, 16);
#elif defined(CONFIG_ARCH_SCX30G) || defined(CONFIG_ARCH_SCX35L)
	efuse_data = __ddie_efuse_read(BLK_ADC_DETA_ABC);
	WARN_ON(!(efuse_data & BIT(31))); /* BIT 31 is protected bit */
#else
	#warning "AuxADC CAL DETA need fixing"
#endif

	pr_info("%s efuse data: 0x%08x\n", __func__, efuse_data);

	efuse_data &= 0xFFFFFF; /* get BIT0 ~ BIT23 in block 7 */

	if (!(efuse_data) || (!pdata)) {
		return -1;
	}

	WARN_ON(!((delta[0] > delta[1]) && (delta[1] > delta[2])));

	for (i = 0; i < num; i++) {
		pdata[i] =
		    (unsigned int)ADC_VOL(delta[i], ideal[i][1], ideal[i][0]);
	}

	/* dump adc calibration parameters */
	printk("%s adc_cal: ", __func__);
	for (i = 0; i < num; i++) {
		printk("%d -- %d; \n", (pdata[i] & 0xFFFF),
		       (pdata[i] >> 16) & 0xFFFF);
	}
	printk("\n");

	return 0;
}

EXPORT_SYMBOL_GPL(sci_efuse_get_cal);

/*
 * sci_efuse_get_cal_v2 - read 4 point adc calibration data saved in efuse
 * @pdata: adc data pointer
 * pdata[0] -> 4.2v
 * pdata[1] -> 3.6v
 * pdata[3] -> 0.9v
 * pdata[4] -> 0.3v
 * @num: the length of adc data
 *
 * retruns 0 if success, else
 * returns negative number.
 */
int sci_efuse_get_cal_v2(unsigned int *pdata, int num)
{
	int i;
	u32 efuse_data;
	u8 *delta = (u8 *) & efuse_data;
	const u16 ideal[4][2] = {
		{4200, 830},	/* FIXME: efuse only support 10bits */
		{3600, 711},
		{900, 737},
		{300, 255},
	};

	efuse_data = __ddie_efuse_read(BLK_ADC_DETA_ABC);

	pr_info("%s efuse block(%d) data: 0x%08x\n", __func__, BLK_ADC_DETA_ABC, efuse_data);

	WARN_ON(!(efuse_data & BIT(31))); /* BIT 31 is protected bit */
	efuse_data &= 0xFFFFFF; /* get BIT0 ~ BIT23 in block 7 */

	if (!(efuse_data) || (!pdata)) {
		return -1;
	}

	WARN_ON(!((delta[0] > delta[1]) && (delta[1] > delta[2])));

	for (i = 0; i < num; i++) {
		pdata[i] =
		    (unsigned int)ADC_VOL(delta[i], ideal[i][1], ideal[i][0]);
		if (3 == i) {
			efuse_data = __ddie_efuse_read(BLK_ADC_DETA_D);
			pr_info("efuse block(%d) data: 0x%08x\n", BLK_ADC_DETA_D, efuse_data);
			WARN_ON(!(efuse_data & BIT(31)));
			efuse_data &= 0xFF0000; /* get BIT16 ~ BIT23 in block 9 */
			if (!(efuse_data)) {
				return -2;
			}
			pdata[i] =
			    (unsigned int)ADC_VOL(delta[2], ideal[i][1],
						  ideal[i][0]);
		}
	}

	/* dump adc calibration parameters */
	printk("%s adc_cal: ", __func__);
	for (i = 0; i < num; i++) {
		printk("%d -- %d; \n", (pdata[i] & 0xFFFF),
		       (pdata[i] >> 16) & 0xFFFF);
	}
	printk("\n");

	return 0;
}

EXPORT_SYMBOL_GPL(sci_efuse_get_cal_v2);
