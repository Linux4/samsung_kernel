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
#include <soc/sprd/arch_misc.h>
#include "sprd_otp.h"

#define BLK_UID_HIGH                    ( 0 )
#define BLK_UID_LOW                     ( 1 )
#define BLK_ADC_DETA                    ( 7 )
#if defined(CONFIG_ARCH_SCX35L)
#define BLK_DHR_DETA					( 7 )
#endif
#define BLK_CCCV_DETA                   ( 9 )
#define BLK_TEMP_ADC_DETA		(9)
#define BLK_FGU_DETA_ABC                ( 8 )
#define BLK_FGU_DETA_D                  ( 9 )

#define BLK_WIDTH_OTP_EMEMORY			( 8 ) /* bit counts */
#define BLK_ADC_DETA_ABC_OTP			( 7 ) /* start block for ADC otp delta */
#define BLK_DHR_DETA_SHARKLC		( 13 )

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

#if defined(CONFIG_ARCH_SCX20L) || defined(CONFIG_ARCH_SCX35L64)\
|| defined(CONFIG_ARCH_SCX20)
#define BLK_THM_DETA		(13)
#define THM_CAL_BIT			(26)
#elif defined(CONFIG_ARCH_SCX35LT8) || defined(CONFIG_ARCH_WHALE)
#define BLK_THM_DETA		(15)
#define THM_CAL_BIT			(17)
#define THM_CAL_OFFSET		273020
#define THM_CAL_DEF		383
#define THM_BASE_DATA	765
#define THM_INT_OFF		28
#else
#define BLK_THM_DETA		(7)
#define THM_CAL_BIT			(24)
#endif

#if defined (CONFIG_ARCH_SCX35LT8) || defined(CONFIG_ARCH_WHALE) 
u32 thm_cal_algorithm(int bit_off)
{
   u32 k_real;
   int dec,t_ft,t,x_ft,dc;
   u32 data=__ddie_efuse_read(BLK_THM_DETA);
   int thm_sign = (data >> 2) & 0x0001;
   int thm_int = data & 0x0003;
   int thm_fra = (data >> 3) & 0x0003;
   int thm_zero = data & 0x3F;
   int vx_ft = (data >> bit_off)&0x1F;
   int thm_off = (data >> bit_off)& 0x3F;
   int x_ft_sign = (data >> (5+bit_off)) & 0x0001;
   if(x_ft_sign == 0){
		x_ft = THM_BASE_DATA + vx_ft ;
	}else{
		x_ft= THM_BASE_DATA - vx_ft ;
	}
   if(thm_sign==0)
	{
		t = THM_INT_OFF + thm_int ;
	}else{
		t = THM_INT_OFF - thm_int ;
	}
	if(thm_fra == 0){
        dec = 0;
	}else if(thm_fra == 1){
		dec = 250;
	}else if(thm_fra == 2){
		dec = 500;
	}else if(thm_fra == 3){
		dec = 750;
	}else{
		printk("this dec is error");
	}
	t_ft = t * 1000 + dec;
	dc = x_ft*1024;
	printk("the dc is =%d,X_FT=%d,T_FT=%d\n",dc,x_ft,t_ft);
	if(thm_zero == 0 && thm_off == 0){
		k_real = THM_CAL_DEF;
	}else{
		k_real = ((t_ft+THM_CAL_OFFSET)*1000/dc);
		if (k_real == 0){
			k_real = THM_CAL_DEF ;
		}
	}
	printk("the k_real =%d\n",k_real);
	return k_real;
}

int  sci_efuse_arm_thm_cal_get(int *cal,int *offset)
{
	u32 k_real;
	int bit_off=5;
	*offset = THM_CAL_OFFSET;
	k_real=thm_cal_algorithm(bit_off);
	if(k_real > 0){
		*cal = k_real;
		return 0;
	}
	return -1;
}
EXPORT_SYMBOL_GPL(sci_efuse_arm_thm_cal_get);

int  sci_efuse_bcore_thm_cal_get(int *cal,int *offset)
{
	u32 k_real;
	int bit_off=11;
	*offset = THM_CAL_OFFSET;
	k_real=thm_cal_algorithm(bit_off);
	if(k_real > 0){
		*cal = k_real;
		return 0;
	}
	return -1;
}


EXPORT_SYMBOL_GPL(sci_efuse_bcore_thm_cal_get);

#define BLK_BIG_OSC_DETA	13
#define BLK_LITTLE_OSC_DETA	3

int sci_efuse_bigcpu_osc_get(u32 *p_cal_data)
{
	u32 data=__ddie_efuse_read(BLK_BIG_OSC_DETA);
	u32 bigcpu_osc = data & 0x3F;
	if (bigcpu_osc ==0) {
		*p_cal_data  = 0;
		return -1;
	}
	*p_cal_data = bigcpu_osc ;
	return 0;
}
EXPORT_SYMBOL_GPL(sci_efuse_bigcpu_osc_get);

int sci_efuse_litcpu_osc_get(u32 *p_cal_data)
{
	u32 data=__ddie_efuse_read(BLK_LITTLE_OSC_DETA);
	u32 litcpu_osc = data & 0x3F;
	if (litcpu_osc ==0) {
		*p_cal_data  = 0;
		return -1;
	}
	*p_cal_data = litcpu_osc ;
	return 0;
}

EXPORT_SYMBOL_GPL(sci_efuse_litcpu_osc_get);
#endif

int  sci_efuse_thermal_cal_get(int *cal)
{
	u32 data=__ddie_efuse_read(BLK_THM_DETA);
	int thm_cal = (data >> THM_CAL_BIT) & 0x001F;
	if (thm_cal ==0) {
		*cal  = 0;
		return -1;
	}
	*cal = thm_cal * 1000 -16000;
	return 0;
}
EXPORT_SYMBOL_GPL(sci_efuse_thermal_cal_get);

#ifdef CONFIG_ARCH_SCX20
#define BLK_BINNING_DETA	13
#define BINNING_CAL_BIT			(15)

int sci_efuse_binning_result_get(u32 *p_binning_data)
{
	u32 data=__ddie_efuse_read(BLK_BINNING_DETA);
	u32 binning = (data >> BINNING_CAL_BIT) & 0xF;
	if (binning ==0) {
		*p_binning_data  = 0;
		return -1;
	}
	*p_binning_data = binning ;
	return 0;
}
EXPORT_SYMBOL_GPL(sci_efuse_binning_result_get);
#endif

#if defined(CONFIG_ARCH_SCX35L)
int  sci_efuse_Dhryst_binning_get(int *cal)
{
	u32 data = 0;
	int Dhry_binning = 0;
	if(soc_is_scx9832a_v0()){
		data=__ddie_efuse_read(BLK_DHR_DETA_SHARKLC);
		Dhry_binning = (data >> 10) & 0x003F;
		pr_info("%s() get efuse block %u, deta: 0x%08x\n", __func__, BLK_DHR_DETA_SHARKLC, Dhry_binning);
	}else{
		data=__ddie_efuse_read(BLK_DHR_DETA);
		Dhry_binning = (data >> 4) & 0x003F;
		pr_info("%s() get efuse block %u, deta: 0x%08x\n", __func__, BLK_DHR_DETA, Dhry_binning);
	}

	*cal = Dhry_binning;
	return 0;
}
EXPORT_SYMBOL_GPL(sci_efuse_Dhryst_binning_get);
#endif


#define BASE_TEMP_ADC_P0				819	//1.0V
#define BASE_TEMP_ADC_P1				82	//0.1V
#define VOL_TEMP_P0					1000
#define VOL_TEMP_P1					100
#define ADC_DATA_OFFSET			128
int sci_temp_efuse_calibration_get(unsigned int *p_cal_data)
{
#if defined(CONFIG_ADIE_SC2723) || defined(CONFIG_ADIE_SC2723S)
	unsigned int deta = 0;
	unsigned short adc_temp = 0;

	/* verify the otp data of ememory written or not */
	adc_temp = (__adie_efuse_read(0) & BIT(7));
	if (adc_temp)
		return 0;

	deta = __adie_efuse_read_bits(BLK_TEMP_ADC_DETA * BLK_WIDTH_OTP_EMEMORY, 16);
	pr_info("%s() get efuse block %u, deta: 0x%08x\n", __func__, BLK_TEMP_ADC_DETA, deta);

	deta &= 0xFFFFFF; /* get BIT0 ~ BIT23) in block 7 */

	if ((!deta) || (p_cal_data == NULL)) {
		return 0;
	}
	//adc 0.1V
	adc_temp = ((deta & 0x00FF) + BASE_TEMP_ADC_P1 - ADC_DATA_OFFSET) * 4;
	pr_info("0.1V adc_temp =%d/0x%x\n",adc_temp,adc_temp);
	p_cal_data[1] = (VOL_TEMP_P1) | (adc_temp << 16);

	//adc 1.0V
	adc_temp =(( (deta >> 8) & 0x00FF) + BASE_TEMP_ADC_P0 - ADC_DATA_OFFSET ) * 4;
	pr_info("1.0V adc_temp =%d/0x%x\n",adc_temp,adc_temp);
	p_cal_data[0] = (VOL_TEMP_P0) | (adc_temp << 16);

	return 1;
#else
	return 0;
#endif
}
EXPORT_SYMBOL_GPL(sci_temp_efuse_calibration_get);

#if defined(CONFIG_ADIE_SC2731)
#define BASE_ADC_P0				728	//3.6V
#define BASE_ADC_P1				850	//4.2V
#else
#define BASE_ADC_P0				711	//3.6V
#define BASE_ADC_P1				830	//4.2V
#endif
#define VOL_P0					3600
#define VOL_P1					4200
//#define ADC_DATA_OFFSET			128
int sci_efuse_calibration_get(unsigned int *p_cal_data)
{
	unsigned int deta = 0;
	unsigned short adc_temp = 0;
#if defined(CONFIG_ADIE_SC2731)
#if 0
	adc_temp = (__adie_efuse_read(0) & BIT(0));
	if (adc_temp)
		return 0;
#endif
	deta = __adie_efuse_read_bits(BITSINDEX(18, 0), 16);

	pr_info("%s() get efuse block %u, deta: 0x%08x\n", __func__, BLK_ADC_DETA, deta);
	if ((!deta) || (p_cal_data == NULL)) {
		return 0;
	}
	adc_temp = ((deta >> 8) & 0x00FF) + BASE_ADC_P0 - ADC_DATA_OFFSET;
	p_cal_data[1] = (VOL_P0) | ((adc_temp << 2) << 16);
	adc_temp = (deta & 0x00FF) + BASE_ADC_P1 - ADC_DATA_OFFSET;
	p_cal_data[0] = (VOL_P1) | ((adc_temp << 2) << 16);
	return 1;
#else
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
#endif
}

EXPORT_SYMBOL_GPL(sci_efuse_calibration_get);

int sci_efuse_cccv_cal_get(unsigned int *p_cal_data)
{
#if defined(CONFIG_ADIE_SC2723) || defined(CONFIG_ADIE_SC2723S)
	unsigned int data,blk0;

	blk0 = __adie_efuse_read(0);

	if (blk0 & (1 << 7)) {
		return 0;
	}

	data = __adie_efuse_read_bits(BITSINDEX(14, 0), 6);
	pr_info("sci_efuse_cccv_cal_get data:0x%x\n", data);
	*p_cal_data = (data) & 0x003F;
#else
	unsigned int data;

	data = __ddie_efuse_read(BLK_CCCV_DETA);
	data &= ~(1 << 31);

	pr_info("sci_efuse_cccv_cal_get data:0x%x\n", data);

	if ((!data) || (p_cal_data == NULL)) {
		return 0;
	}

	*p_cal_data = (data >> 24) & 0x003F;	/*block 9, bit 29..24 */
#endif
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
#elif defined(CONFIG_ADIE_SC2731)
	unsigned int data,blk0;
#if 0
	blk0 = __adie_efuse_read(0);
	if (!(blk0 & (1))) {
		return 0;
	}
#endif
	data = __adie_efuse_read_bits(BITSINDEX(3, 0), 9);
	printk("sci_efuse_fgu_cal_get 4.2 data data:0x%x\n", data);
	p_cal_data[0] = (data + 6963) - 4096 - 256-8;	//4.0V or 4.2V
	data = __adie_efuse_read_bits(BITSINDEX(4, 9), 6);
	printk("sci_efuse_fgu_cal_get 3.6 data data:0x%x\n", data);
	p_cal_data[1] = p_cal_data[0] - 410 - data + 32;	//3.6V
	data = __adie_efuse_read_bits(BITSINDEX(4, 0), 9);
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

	//WARN_ON(!((delta[0] > delta[1]) && (delta[1] > delta[2])));

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

/*
* sci_efuse_get_cal_v3 - read 2 point adc calibration data saved in efuse
* @pdata: adc data pointer
* pdata[0] -> 0.9v
* pdata[1] -> 0.3v
* @num: the length of adc data
*
* retruns 0 if success, else
* returns negative number.
*/
int sci_efuse_get_cal_v3(unsigned int *pdata, int num)
{
	int i;
	u32 efuse_data;
	u8 *delta = (u8 *) &efuse_data;
	const u16 ideal[2][2] = {
		{900, 737},
		{300, 245},
	};
	if (!pdata || num < 2) {
		return -1;
	}
	efuse_data = __ddie_efuse_read(BLK_ADC_DETA_ABC);
	pr_info("%s efuse block(%d) data: 0x%08x\n", __func__, BLK_ADC_DETA_ABC, efuse_data);
	WARN_ON(!(efuse_data & BIT(31)));
	/* BIT 31 is protected bit */
	efuse_data &= 0xFF0000;
	/* get BIT16 ~ BIT23 in block 7 */
	if (!efuse_data) {
		return -2;
	}
	pdata[0] = (unsigned int)ADC_VOL(delta[2], ideal[0][1], ideal[0][0]);
	efuse_data = __ddie_efuse_read(BLK_ADC_DETA_D);
	pr_info("efuse block(%d) data: 0x%08x\n", BLK_ADC_DETA_D, efuse_data);
	WARN_ON(!(efuse_data & BIT(31)));
	efuse_data &= 0xFF0000;
	/* get BIT16 ~ BIT23 in block 9 */
	if (!(efuse_data)) {
		return -3;
	}
	pdata[1] = (unsigned int)ADC_VOL(delta[2], ideal[1][1], ideal[1][0]);
	/* dump adc calibration parameters */
	printk("%s adc_cal: \n", __func__);
	for (i = 0; i < num; i++) {
		printk("%d -- %d; \n", (pdata[i] & 0xFFFF),
			(pdata[i] >> 16) & 0xFFFF);
	}
	printk("\n");
	return 0;
}
EXPORT_SYMBOL_GPL(sci_efuse_get_cal_v3);

#if defined(CONFIG_ARCH_SCX20)
#define BLK_USB_PHY_TUNE 12
int sci_efuse_usb_phy_tune_get(unsigned int *p_cal_data)
{
	unsigned int data, ret;

	data = __ddie_efuse_read(BLK_USB_PHY_TUNE);

	*p_cal_data = (data >> 16) & 0x1f;
	ret = !!(data & 1 << 30);
	pr_info("%s: usb phy tune is %s, tfhres: 0x%x\n",
		__func__, ret ? "OK" : "NOT OK", *p_cal_data);

	return ret;
}
EXPORT_SYMBOL_GPL(sci_efuse_usb_phy_tune_get);
#endif

#if defined(CONFIG_ADIE_SC2723) || defined(CONFIG_ADIE_SC2723S)

int sci_efuse_ib_trim_get(unsigned int *p_cal_data)
{
	unsigned int data,blk0;

	blk0 = __adie_efuse_read(0);
	if (blk0 & (1 << 7)) {
		return 0;
	}

	data = __adie_efuse_read_bits(BITSINDEX(15, 0), 7);
	*p_cal_data = data;
	return 1;
}
EXPORT_SYMBOL_GPL(sci_efuse_ib_trim_get);

#endif
#if defined(CONFIG_ADIE_SC2731)
int sci_efuse_typec_cal_get(unsigned int *p_cal_data)
{
		unsigned int data, blk0;
#if 0
		blk0 = __adie_efuse_read(0);
		if (blk0 & (1)) {
			return 0;
		}
#endif
		if ((p_cal_data == NULL)) {
			return 0;
		}

		data = __adie_efuse_read_bits(BITSINDEX(13, 11), 5);
		printk("sci_efuse_typec_cal_get data:0x%x\n", data);
		p_cal_data[0] = data;	/*type-c cc1*/

		data = __adie_efuse_read_bits(BITSINDEX(13, 6), 5);
		printk("sci_efuse_typec_cal_get data:0x%x\n", data);
		p_cal_data[1] = data;	/*type-c cc2*/

		return 1;

}
EXPORT_SYMBOL_GPL(sci_efuse_typec_cal_get);


#endif
