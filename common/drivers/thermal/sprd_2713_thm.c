/*
 * Copyright (C) 2013 Spreadtrum Communications Inc.
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

#include <linux/kernel.h>
#include <linux/err.h>
#include <mach/hardware.h>
#include <mach/sci.h>
#include <mach/sci_glb_regs.h>
#include <linux/io.h>
#include <mach/adi.h>
#include "thm.h"
#include <linux/sprd_thm.h>
#include <mach/arch_misc.h>

#define SPRD_THM_DEBUG
#ifdef SPRD_THM_DEBUG
#define THM_DEBUG(format, arg...) printk("sprd thm: " "@@@" format, ## arg)
#else
#define THM_DEBUG(format, arg...)
#endif

#define THM_CTRL            (0x0000)
#define THM_INT_CTRL       (0x0004)
#define SENSOR_CTRL         (0x0020)
#define SENSOR_DET_PERI     (0x0024)
#define SENSOR_INT_CTRL     (0x0028)
#define SENSOR_INT_STS      (0x002C)
#define SENSOR_INT_RAW_STS      (0x0030)
#define SENSOR_INT_CLR      (0x0034)
#define SENSOR_OVERHEAT_HOT_THRES   (0X0040)
#define SENSOR_HOT2NOR__HIGHOFF_THRES   (0X0044)
#define SENSOR_LOWOFF__COLD_THRES   (0X0048)
#define SENSOR_TEMPER0_READ	(0x0058)

#define SEN_OVERHEAT_INT_BIT (1 << 5)
#define SEN_HOT_INT_BIT      (1 << 4)
#define SEN_HOT2NOR_INT_BIT  (1 << 3)
#define SEN_HIGHOFF_BIT  (1 << 2)
#define SEN_LOWOFF_INT_BIT  (1 << 1)

#define RAW_TEMP_OFFSET 8
#define RAW_TEMP_RANGE_MSK  0x7F

#define HIGH_BITS_OFFSET   4

#define HIGH_TAB_SZ 8
#define LOW_TAB_SZ  16

#define HOT2NOR_RANGE   15
#define LOCAL_SENSOR_ADDR_OFF 0x100
#define DELAY_TEMPERATURE 3
#define INTOFFSET 3

#define TSMC_DOLPHINW4T_CHIP_ID_1  0x7715A001
#define TSMC_DOLPHINW4T_CHIP_ID_2  0x7715A003
#define TSMC_DOLPHINWT4T_CHIP_ID_1 0x8815A001

static const short temp_search_high_40nm[HIGH_TAB_SZ] =
    { -41, -14, 14, 41, 68, 95, 122, 150 };
static const short temp_search_low_40nm[LOW_TAB_SZ] =
    { 0, 2, 4, 5, 7, 8, 10, 12, 14, 15, 17, 19, 20, 22, 24, 26 };
static const short temp_search_high_152nm[HIGH_TAB_SZ] =
    { -45, -19, 7, 33, 59, 85, 111, 137 };
static const short temp_search_low_152nm[LOW_TAB_SZ] =
    { 0, 2, 3, 5, 6, 8, 10, 11, 13, 14, 16, 18, 19, 21, 22, 24 };

static u32 current_trip_num = 0;
static int arm_sen_cal_offset = 0;
static int pmic_sen_cal_offset = 0;

static inline int __thm_reg_write(u32 reg, u16 bits, u16 clear_msk);
static inline u32 __thm_reg_read(u32 reg);
int sprd_thm_set_active_trip(struct sprd_thermal_zone *pzone, int trip );
u32 sprd_thm_temp2rawdata(u32 sensor, int temp);

static inline int __thm_reg_write(u32 reg, u16 bits, u16 clear_msk)
{
	if (reg >= SPRD_THM_BASE && reg <= (SPRD_THM_BASE + SPRD_THM_SIZE)) {
		__raw_writel(((__raw_readl((volatile void *)reg) & ~clear_msk) | bits), (volatile void *)(reg));
	} else if (reg >= ANA_THM_BASE && reg <= (ANA_THM_BASE + SPRD_THM_SIZE)) {
		sci_adi_write(reg, bits, clear_msk);
	} else {
		printk(KERN_ERR "error thm reg0x:%x \n", reg);
	}
	return 0;
}

static inline u32 __thm_reg_read(u32 reg)
{
	if (reg >= SPRD_THM_BASE && reg <= (SPRD_THM_BASE + SPRD_THM_SIZE)) {
		return __raw_readl((volatile void *)reg);
	} else if (reg >= ANA_THM_BASE && reg <= (ANA_THM_BASE + SPRD_THM_SIZE)) {
		return sci_adi_read(reg);
	} else {
		printk(KERN_ERR "error thm reg0x:%x \n", reg);
	}
	return 0;
}

int sprd_thm_set_active_trip(struct sprd_thermal_zone *pzone, int trip )
{
	u32 raw_temp = 0;
	u32 local_sen_id = 0;
	u32 local_sensor_addr = 0;
	struct sprd_thm_platform_data *trip_tab = pzone->trip_tab;

	THM_DEBUG("thm sensor id:%d, trip:%d  \n", pzone->sensor_id, trip);
	if (trip < 0 || trip > (trip_tab->num_trips - 1))
		return -1;
	if (trip_tab->trip_points[trip].type != THERMAL_TRIP_ACTIVE)
		return -1;

	local_sen_id = pzone->sensor_id;
	local_sensor_addr =
		(u32) pzone->reg_base + local_sen_id * LOCAL_SENSOR_ADDR_OFF;

	//Disable sensor int
	__thm_reg_write((local_sensor_addr + SENSOR_INT_CTRL),0,(0x7F & (~SEN_OVERHEAT_INT_BIT)));

	//set hot int temp value
	THM_DEBUG("thm sensor trip:%d, temperature:%d  \n", trip, trip_tab->trip_points[trip].temp);
	raw_temp =
		sprd_thm_temp2rawdata(pzone->sensor_id,
				              trip_tab->trip_points[trip].temp - pmic_sen_cal_offset - INTOFFSET);
	if (raw_temp < RAW_TEMP_RANGE_MSK) {
		raw_temp++;
	}
	__thm_reg_write((local_sensor_addr + SENSOR_OVERHEAT_HOT_THRES),
				     raw_temp,
					 RAW_TEMP_RANGE_MSK);

	//set Hot2Normal int temp value
	raw_temp =
		sprd_thm_temp2rawdata(pzone->sensor_id, trip_tab->trip_points[trip].lowoff - INTOFFSET - pmic_sen_cal_offset);
	__thm_reg_write((local_sensor_addr + SENSOR_HOT2NOR__HIGHOFF_THRES),
					raw_temp << RAW_TEMP_OFFSET,
					RAW_TEMP_RANGE_MSK << RAW_TEMP_OFFSET);

	//set cold int temp value
	raw_temp =
		sprd_thm_temp2rawdata(pzone->sensor_id, trip_tab->trip_points[trip].lowoff - INTOFFSET - pmic_sen_cal_offset);
	__thm_reg_write((local_sensor_addr + SENSOR_LOWOFF__COLD_THRES),
					raw_temp << RAW_TEMP_OFFSET,
					RAW_TEMP_RANGE_MSK << RAW_TEMP_OFFSET);

	THM_DEBUG("thm OVERHEAT_HOT:0x%x, HOT2NOR__HIGHOFF:0x%x, LOWOFF__COLD:0x%x\n", __thm_reg_read(local_sensor_addr + SENSOR_OVERHEAT_HOT_THRES),
		  __thm_reg_read(local_sensor_addr + SENSOR_HOT2NOR__HIGHOFF_THRES), __thm_reg_read(local_sensor_addr + SENSOR_LOWOFF__COLD_THRES));

    // Restart sensor to enable new paramter
	__thm_reg_write((local_sensor_addr + SENSOR_CTRL), 0x9, 0x9);

	if (trip > 0)
	{
		//enable Hot int and Lowoff int
		__thm_reg_write((local_sensor_addr + SENSOR_INT_CTRL),
				SEN_HOT_INT_BIT | SEN_LOWOFF_INT_BIT,
				SEN_HOT_INT_BIT | SEN_LOWOFF_INT_BIT);
	}
	else
	{
		//enable Hot int and disable LOWOFF int
		__thm_reg_write((local_sensor_addr + SENSOR_INT_CTRL),
				SEN_HOT_INT_BIT,
				SEN_HOT_INT_BIT | SEN_LOWOFF_INT_BIT);
	}

	return 0;
}

u32 sprd_thm_temp2rawdata(u32 sensor, int temp)
{
	u32 high_bits, low_bits;
	int i;
	const short *high_tab;
	const short *low_tab;

	if (SPRD_ARM_SENSOR == sensor) {
		high_tab = temp_search_high_40nm;
		low_tab = temp_search_low_40nm;
	} else if (SPRD_PMIC_SENSOR == sensor) {
		high_tab = temp_search_high_152nm;
		low_tab = temp_search_low_152nm;
	} else {
		printk(KERN_ERR "error thm sensor id:%d \n", sensor);
		return 0;
	}

	if (temp < high_tab[0]) {
		printk(KERN_ERR "temp over low temp:%d \n", temp);
		return 0;
	}

	for (i = HIGH_TAB_SZ - 1; i >= 0; i--) {
		if (high_tab[i] <= temp)
			break;
	}
	if (i < 0) {
		i = 0;
	}
	temp -= high_tab[i];
	high_bits = i;

	for (i = LOW_TAB_SZ - 1; i >= 0; i--) {
		if (low_tab[i] <= temp)
			break;
	}
	if (i < 0) {
		i = 0;
	}
	low_bits = i;
	return ((high_bits << HIGH_BITS_OFFSET) | low_bits);

}

int sprd_thm_rawdata2temp(u32 sensor, int rawdata)
{
	const short *high_tab;
	const short *low_tab;

	if (SPRD_ARM_SENSOR == sensor) {
		high_tab = temp_search_high_40nm;
		low_tab = temp_search_low_40nm;
	} else if (SPRD_PMIC_SENSOR == sensor) {
		high_tab = temp_search_high_152nm;
		low_tab = temp_search_low_152nm;
	} else {
		printk(KERN_ERR "error thm sensor id:%d \n", sensor);
		return 0;
	}

	return high_tab[(rawdata >> HIGH_BITS_OFFSET) & 0x07] +
	    low_tab[rawdata & 0x0F];
}

int sprd_thm_temp_read(struct sprd_thermal_zone *pzone)
{
	u32 rawdata = 0;
	int cal_offset = 0;
	u32 sensor = pzone->sensor_id;

	if (SPRD_ARM_SENSOR == sensor) {
		rawdata = __thm_reg_read((SPRD_THM_BASE + SENSOR_TEMPER0_READ));
		cal_offset = arm_sen_cal_offset;
	} else if (SPRD_PMIC_SENSOR == sensor) {
		rawdata = __thm_reg_read(ANA_THM_BASE + SENSOR_TEMPER0_READ);
		cal_offset = pmic_sen_cal_offset;
	} else {
		printk(KERN_ERR "error thm sensor id:%d \n", sensor);
		return 0;
	}
	THM_DEBUG("thm sensor id:%d, cal_offset:%d, rawdata:0x%x\n", sensor,
		  cal_offset, rawdata);
	return sprd_thm_rawdata2temp(sensor, rawdata) + cal_offset;
}
int sprd_thm_chip_id_check(void)
{
	u32 chip_id_tmp;

	chip_id_tmp = sci_get_chip_id();

#if !defined(CONFIG_ARCH_SCX15)
	return 0;
#else
	if ((TSMC_DOLPHINW4T_CHIP_ID_1 == chip_id_tmp) || (TSMC_DOLPHINW4T_CHIP_ID_2 == chip_id_tmp) ||
	   (TSMC_DOLPHINWT4T_CHIP_ID_1 == chip_id_tmp)) {
		//printk("Sprd thm the chip support thermal CHIP_ID:0x%x \n",chip_id_tmp);
		return 0;
	} else {
		printk("Sprd thm the chip don't support thermal CHIP_ID:0x%x \n",chip_id_tmp);
		return -1;
	}
#endif
}
int sprd_thm_hw_init(struct sprd_thermal_zone *pzone)
{
	u32 local_sensor_addr, base_addr = 0;
	u32 local_sen_id = 0;
	u32 raw_temp = 0;
	int cal_offset = 0;
	struct sprd_thm_platform_data *trip_tab = pzone->trip_tab;

	base_addr = (u32) pzone->reg_base;

	printk(KERN_NOTICE "sprd_thm_hw_init 2713_thm id:%d,base 0x%x\n",
	       pzone->sensor_id, base_addr);

	if (SPRD_ARM_SENSOR == pzone->sensor_id) {
		//ddie thm en
		sci_glb_set(REG_AON_APB_APB_EB1, BIT_THM_EB);
		sci_glb_set(REG_AON_APB_APB_RTC_EB,
			    (BIT_THM_RTC_EB | BIT_ARM_THMA_RTC_EB |
			     BIT_ARM_THMA_RTC_AUTO_EN));
		cal_offset = arm_sen_cal_offset = 0;
		local_sen_id = 0;
	} else if (SPRD_PMIC_SENSOR == pzone->sensor_id) {
		//adie thm en
		sci_adi_set(ANA_REG_GLB_ARM_MODULE_EN, BIT_ANA_THM_EN);
		sci_adi_set(ANA_REG_GLB_RTC_CLK_EN,
			    BIT_RTC_THMA_AUTO_EN | BIT_RTC_THMA_EN |
			    BIT_RTC_THM_EN);
		cal_offset = pmic_sen_cal_offset = 0;
		local_sen_id = 0;
	} else {
		printk(KERN_ERR "error thm sensor id:%d \n", pzone->sensor_id);
		return -EINVAL;
	}
	__thm_reg_write((base_addr + THM_CTRL), 0x1 >> local_sen_id, 0);
	__thm_reg_write((base_addr + THM_INT_CTRL), 0x3, 0);	//enable top int

	local_sensor_addr = base_addr + local_sen_id * LOCAL_SENSOR_ADDR_OFF;

	__thm_reg_write((local_sensor_addr + SENSOR_INT_CTRL), 0, ~0);	//disable all int
	__thm_reg_write((local_sensor_addr + SENSOR_INT_CLR), ~0, 0);	//clr all int

	//set int
	if (trip_tab->num_trips > 0) {
#if 0
		//set hot
		if (trip_tab->trip_points[0].type == THERMAL_TRIP_ACTIVE) {
			raw_temp =
			    sprd_thm_temp2rawdata(pzone->sensor_id,
						  trip_tab->trip_points[0].
						  temp - cal_offset);
			if (raw_temp < RAW_TEMP_RANGE_MSK) {
				raw_temp++;
			}
			//set hot int temp value
			__thm_reg_write((local_sensor_addr +
					 SENSOR_OVERHEAD_HOT_THRES), raw_temp,
					RAW_TEMP_RANGE_MSK);
			raw_temp =
			    sprd_thm_temp2rawdata(pzone->sensor_id,
						  trip_tab->trip_points[0].
						  temp - HOT2NOR_RANGE -
						  cal_offset);
			//set hot2nor int temp value
			__thm_reg_write((local_sensor_addr +
					 SENSOR_HOT2NOR__HIGHOFF_THRES),
					raw_temp << RAW_TEMP_OFFSET,
					RAW_TEMP_RANGE_MSK << RAW_TEMP_OFFSET);
			__thm_reg_write((local_sensor_addr + SENSOR_INT_CTRL),	//enable int
					SEN_HOT2NOR_INT_BIT | SEN_HOT_INT_BIT,
					0);
		}
#else
		current_trip_num = 0;
		sprd_thm_set_active_trip(pzone,current_trip_num);
#endif
		//set overheat
		if (trip_tab->trip_points[trip_tab->num_trips - 1].type ==
		    THERMAL_TRIP_CRITICAL) {
			raw_temp =
			    sprd_thm_temp2rawdata(pzone->sensor_id,
						  trip_tab->trip_points
						  [trip_tab->num_trips -
						   1].temp - cal_offset);
			//set overheat int temp value
			__thm_reg_write((local_sensor_addr +
					 SENSOR_OVERHEAT_HOT_THRES),
					raw_temp << RAW_TEMP_OFFSET,
					RAW_TEMP_RANGE_MSK << RAW_TEMP_OFFSET);
			__thm_reg_write((local_sensor_addr + SENSOR_INT_CTRL),	//enable int
					SEN_OVERHEAT_INT_BIT, 0);
		}
	}

	printk(KERN_NOTICE "sprd_thm_hw_init addr 0x:%x,int ctrl 0x%x\n",
	       local_sensor_addr,
	       __thm_reg_read((local_sensor_addr + SENSOR_INT_CTRL)));

	// Set sensor det period is 2.25S
	__thm_reg_write((local_sensor_addr + SENSOR_DET_PERI), 0x2000, 0x2000);
	__thm_reg_write((local_sensor_addr + SENSOR_CTRL), 0x101, 0x101);

	// Start the sensor by set Sen_set_rdy(bit3)
	__thm_reg_write((local_sensor_addr + SENSOR_CTRL), 0x8, 0x8);

	return 0;

}

int sprd_thm_hw_disable_sensor(struct sprd_thermal_zone *pzone)
{
	int ret = 0;
	// Sensor minitor disable
	if(SPRD_ARM_SENSOR == pzone->sensor_id){
		__thm_reg_write((u32)(pzone->reg_base + SENSOR_CTRL), 0x0, 0x8);
		__thm_reg_write((u32)(pzone->reg_base + SENSOR_CTRL), 0x00, 0x01);
	}
	return ret;

}

int sprd_thm_hw_enable_sensor(struct sprd_thermal_zone *pzone)
{
	int ret = 0;
	// Sensor minitor enable
	THM_DEBUG("sprd_2713_thm enable sensor sensor_ID:0x%x \n",pzone->sensor_id);
	if(SPRD_ARM_SENSOR == pzone->sensor_id){
		__thm_reg_write((u32)(pzone->reg_base + SENSOR_CTRL), 0x01, 0x01);
		__thm_reg_write((u32)(pzone->reg_base + SENSOR_CTRL), 0x8, 0x8);
	}
	return ret;

}


u16 int_ctrl_reg[SPRD_MAX_SENSOR];
int sprd_thm_hw_suspend(struct sprd_thermal_zone *pzone)
{
	u32 local_sen_id = 0;
	u32 local_sensor_addr;
	int ret = 0;

	local_sensor_addr =
	    (u32) pzone->reg_base + local_sen_id * LOCAL_SENSOR_ADDR_OFF;
	int_ctrl_reg[pzone->sensor_id] = __thm_reg_read((local_sensor_addr + SENSOR_INT_CTRL));

	sprd_thm_hw_disable_sensor(pzone);
	__thm_reg_write((local_sensor_addr + SENSOR_INT_CTRL), 0, ~0);	//disable all int
	__thm_reg_write((local_sensor_addr + SENSOR_INT_CLR), ~0, 0);	//clr all int
	return ret;
}
int sprd_thm_hw_resume(struct sprd_thermal_zone *pzone)
{
	u32 local_sen_id = 0;
	u32 local_sensor_addr;
	int ret = 0;

	local_sensor_addr =
	    (u32) pzone->reg_base + local_sen_id * LOCAL_SENSOR_ADDR_OFF;

	sprd_thm_hw_enable_sensor(pzone);
	if(SPRD_ARM_SENSOR == pzone->sensor_id){
		__thm_reg_write((local_sensor_addr + SENSOR_INT_CLR), ~0, 0);	//clr all int
		__thm_reg_write((local_sensor_addr + SENSOR_INT_CTRL), int_ctrl_reg[pzone->sensor_id], ~0);	//enable int of saved
		__thm_reg_write((local_sensor_addr + SENSOR_CTRL), 0x9, 0);
	}
	return ret;
}

int sprd_thm_hw_irq_handle(struct sprd_thermal_zone *pzone)
{
	u32 local_sen_id = 0;
	u32 local_sensor_addr;
	u32 int_sts;
	int ret = 0;
	u32 overhead_hot_tem_cur = 0;
	struct sprd_thm_platform_data *trip_tab = pzone->trip_tab;
	int temp;

	local_sensor_addr =
	    (u32) pzone->reg_base + local_sen_id * LOCAL_SENSOR_ADDR_OFF;
	int_sts = __thm_reg_read(local_sensor_addr + SENSOR_INT_STS);

	__thm_reg_write((local_sensor_addr + SENSOR_INT_CLR), int_sts, ~0);	//CLR INT
	temp = sprd_thm_temp_read(pzone->sensor_id);

	printk("sprd_thm_hw_irq_handle --------@@@------id:%d, int_sts :0x%x \n",
	     pzone->sensor_id, int_sts);
	printk("sprd_thm_hw_irq_handle ------$$$--------temp:%d\n", temp);

	overhead_hot_tem_cur = __thm_reg_read((local_sensor_addr + SENSOR_OVERHEAT_HOT_THRES))
								& RAW_TEMP_RANGE_MSK;

	if (int_sts & SEN_HOT_INT_BIT){
		if ((current_trip_num) >= (trip_tab->num_trips - 2)){
			current_trip_num = trip_tab->num_trips - 2;
			return ret;
		}
		if (temp >= trip_tab->trip_points[current_trip_num].temp - INTOFFSET){
			current_trip_num++;
			sprd_thm_set_active_trip(pzone,current_trip_num);
		}
	}else if (int_sts & SEN_LOWOFF_INT_BIT){
		if (temp < trip_tab->trip_points[current_trip_num].lowoff + INTOFFSET){
			current_trip_num--;
			sprd_thm_set_active_trip(pzone,current_trip_num);
		}
	}else{
		THM_DEBUG("sprd_thm_hw_irq_handle NOT a HOT or LOWOFF interrupt \n");
		return ret;
	}
	return ret;
}
