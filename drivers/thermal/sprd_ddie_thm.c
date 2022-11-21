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
#include <soc/sprd/sci.h>
#include <soc/sprd/sci_glb_regs.h>
#include <linux/io.h>
#include <soc/sprd/adi.h>
#include "thm.h"
#include <linux/sprd_thm.h>
#include <soc/sprd/arch_misc.h>
#include <soc/sprd/hardware.h>

#define SPRD_THM_DEBUG
#ifdef SPRD_THM_DEBUG
#define THM_DEBUG(format, arg...) printk( "sprd thm: " "@@@" format, ## arg)
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
#define SENSOR_OVERHEAT_THRES   (0X0040)
#define SENSOR_HOT_THRES   (0X0044)
#define SENSOR_HOT2NOR_THRES   (0X0048)
#define SENSOR_HIGHOFF_THRES   (0X004C)
#define SENSOR_LOWOFF_THRES (0X0050)
#define SENSOR_MON_PERI		(0X0058)
#define SENSOR_MON_CTL		(0X005c)
#define SENSOR_TEMPER0_READ	(0x0060)
#define SENSOR_READ_STATUS	(0x0070)


#define A_SEN_OVERHEAT_INT_BIT (1 << 5)
#define A_SEN_HOT_INT_BIT      (1 << 4)
#define A_SEN_HOT2NOR_INT_BIT  (1 << 3)
#define A_SEN_HIGHOFF_BIT  (1 << 2)
#define A_SEN_LOWOFF_INT_BIT  (1 << 1)

#define A_RAW_TEMP_OFFSET 8
#define A_RAW_TEMP_RANGE_MSK  0x7F

#define A_HIGH_BITS_OFFSET   4

#define A_HIGH_TAB_SZ 8
#define A_LOW_TAB_SZ  16

#define A_HOT2NOR_RANGE   15
#define A_LOCAL_SENSOR_ADDR_OFF 0x100
#define A_DELAY_TEMPERATURE 3
#define INTOFFSET 3

#define A_TSMC_DOLPHINW4T_CHIP_ID_1  0x7715A001
#define A_TSMC_DOLPHINW4T_CHIP_ID_2  0x7715A003
#define A_TSMC_DOLPHINWT4T_CHIP_ID_1 0x8815A001

static const short a_temp_search_high_152nm[A_HIGH_TAB_SZ] =
    { -56, -29, -2, 26, 53, 79, 106, 133 };
static const short a_temp_search_low_152nm[A_LOW_TAB_SZ] =
    { 0, 2, 3, 5, 7, 8, 10, 11, 13, 15, 17, 18, 20, 21, 23, 25 };

#define SEN_OVERHEAT_INT_BIT (1 << 5)
#define SEN_HOT_INT_BIT      (1 << 4)
#define SEN_HOT2NOR_INT_BIT  (1 << 3)
#define SEN_HIGHOFF_BIT  (1 << 2)
#define SEN_LOWOFF_INT_BIT  (1 << 1)

#define RAW_TEMP_RANGE_MSK  0x3FFF
#define RAW_READ_RANGE_MSK  0x7FFF

#define HIGH_BITS_OFFSET   4

#define HOT2NOR_RANGE   15
#define LOCAL_SENSOR_ADDR_OFF  0x100
#define DELAY_TEMPERATURE 3

#define SEN_DET_PRECISION  (0x50)

#define TSMC_DOLPHINW4T_CHIP_ID_1  0x7715A001
#define TSMC_DOLPHINW4T_CHIP_ID_2  0x7715A003
#define TSMC_DOLPHINWT4T_CHIP_ID_1 0x8815A001

#define TEMP_TO_RAM_DEGREE 8
#define TEMP_LOW         (-40)
#define TEMP_HIGH        (120)
#ifdef CONFIG_ARCH_SCX35LT8
#define RAW_DATA_LOW         (627)
#define RAW_DATA_HIGH        (1075)
#else
#define RAW_DATA_LOW         (623)
#define RAW_DATA_HIGH        (1030)
#endif

//static const u32 temp_to_raw[TEMP_TO_RAM_DEGREE + 1] = { RAW_DATA_LOW, 676, 725, 777, 828, 878, 928, 980,RAW_DATA_HIGH };

static u32 current_trip_num = 0;
static int arm_sen_cal_offset = 0;
static int pmic_sen_cal_offset = 0;

unsigned long SPRD_THM_BASE = 0;
unsigned int SPRD_THM_SIZE = 0;

static inline int __thm_reg_write(unsigned long reg, u16 bits, u16 clear_msk);
static inline u32 __thm_reg_read(unsigned long reg);
int sprd_thm_set_active_trip(struct sprd_thermal_zone *pzone, int trip );
u32 sprd_thm_temp2rawdata( int temp);

static inline int __thm_reg_write(unsigned long reg, u16 bits, u16 clear_msk)
{
	if (reg >= SPRD_THM_BASE && reg <= (SPRD_THM_BASE + SPRD_THM_SIZE)) {
		__raw_writel(((__raw_readl((volatile void *)reg) & ~clear_msk) | bits), ((volatile void *)reg));
	 }else {
		printk(KERN_ERR "error thm reg0x:%lx \n", reg);
	}
	return 0;
}

static inline u32 __thm_reg_read(unsigned long reg)
{
	if (reg >= SPRD_THM_BASE && reg <= (SPRD_THM_BASE + SPRD_THM_SIZE)) {
		return __raw_readl((volatile void *)reg);
	} else {
		printk(KERN_ERR "error thm reg0x:%lx \n", reg);
	}
	return 0;
}

static int sprd_thm_get_reg_base(struct sprd_thermal_zone *pzone ,struct resource *regs)
{
	if(SPRD_THM_BASE ==0)
	{
		pzone->reg_base = (void __iomem *)ioremap_nocache(regs->start,
			resource_size(regs));
		if (!pzone->reg_base)
			return -ENOMEM;
		SPRD_THM_BASE = (unsigned long) pzone->reg_base;
		SPRD_THM_SIZE= resource_size(regs);
	}else{
		pzone->reg_base = (void __iomem *)SPRD_THM_BASE;
	}
	return 0;
}

int sprd_thm_set_active_trip(struct sprd_thermal_zone *pzone, int trip )
{
	u32 raw_temp = 0;
	u32 local_sen_id = 0;
	unsigned long  local_sensor_addr = 0;
	struct sprd_thm_platform_data *trip_tab = pzone->trip_tab;

	THM_DEBUG("thm sensor id:%d, trip:%d \n", pzone->sensor_id, trip);
	if (trip < 0 || trip > (trip_tab->num_trips - 1))
		return -1;
	if (trip_tab->trip_points[trip].type != THERMAL_TRIP_ACTIVE)
		return -1;

	local_sen_id = pzone->sensor_id;
	local_sensor_addr =
		(unsigned long ) pzone->reg_base + local_sen_id * LOCAL_SENSOR_ADDR_OFF;

	//Disable sensor int except OVERHEAT
	__thm_reg_write((local_sensor_addr + SENSOR_INT_CTRL), 0, (0x7F & (~SEN_OVERHEAT_INT_BIT)));

	//set hot int temp value
	THM_DEBUG("thm sensor trip:%d, temperature:%ld \n", trip, trip_tab->trip_points[trip].temp);
	raw_temp = sprd_thm_temp2rawdata(trip_tab->trip_points[trip].temp + arm_sen_cal_offset);
	if (raw_temp < RAW_TEMP_RANGE_MSK) {
		raw_temp++;
	}
	__thm_reg_write((local_sensor_addr + SENSOR_HOT_THRES),
						raw_temp, RAW_TEMP_RANGE_MSK);

	//set Hot2Normal int temp value
	raw_temp =
	sprd_thm_temp2rawdata(trip_tab->trip_points[trip].lowoff  + 2 + pmic_sen_cal_offset);
	__thm_reg_write((local_sensor_addr + SENSOR_HOT2NOR_THRES),
		raw_temp, RAW_TEMP_RANGE_MSK);

	raw_temp =
	sprd_thm_temp2rawdata( trip_tab->trip_points[trip].lowoff  + 1 + pmic_sen_cal_offset);
	__thm_reg_write((local_sensor_addr + SENSOR_HIGHOFF_THRES),
		raw_temp, RAW_TEMP_RANGE_MSK);

	//set cold int temp value
	raw_temp =
	sprd_thm_temp2rawdata( trip_tab->trip_points[trip].lowoff + pmic_sen_cal_offset);
	__thm_reg_write((local_sensor_addr + SENSOR_LOWOFF_THRES),
		raw_temp,RAW_TEMP_RANGE_MSK);

	THM_DEBUG("thm set HOT:0x%x, HOT2NOR:0x%x, LOWOFF:0x%x\n", __thm_reg_read(local_sensor_addr + SENSOR_HOT_THRES),
	 __thm_reg_read(local_sensor_addr + SENSOR_HOT2NOR_THRES), __thm_reg_read(local_sensor_addr + SENSOR_LOWOFF_THRES));

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


u32 sprd_thm_temp2rawdata( int temp)
{
	u32 raw_result;
	 if ((temp < TEMP_LOW) || (temp > TEMP_HIGH))
		return 0;
	 raw_result = RAW_DATA_LOW +
	 	(temp - TEMP_LOW) * (RAW_DATA_HIGH - RAW_DATA_LOW) / (TEMP_HIGH - TEMP_LOW);

	return raw_result;
}

int sprd_thm_rawdata2temp( int rawdata)
{
	int temp_result;

	temp_result = TEMP_LOW +
		(rawdata - RAW_DATA_LOW) * (TEMP_HIGH - TEMP_LOW) / (RAW_DATA_HIGH - RAW_DATA_LOW);

	return temp_result;
}
#ifdef THM_TEST
static int sprd_thm_regs_read(struct sprd_thermal_zone *pzone,unsigned int *regs)
{
	unsigned long base_addr = 0;
	printk(" sprd_thm_regs_read\n");
	if(pzone->sensor_id == SPRD_ARM_SENSOR){
		base_addr = (unsigned long) pzone->reg_base;
		*regs =  __thm_reg_read((base_addr + SENSOR_DET_PERI));
		*(regs + 1) =  __thm_reg_read((base_addr + SENSOR_MON_CTL));
		*(regs + 2) =  __thm_reg_read((base_addr + SENSOR_MON_PERI));
		*(regs + 3) =  __thm_reg_read((base_addr + SENSOR_CTRL));
	}
	return 0;
}

static int sprd_thm_regs_set(struct sprd_thermal_zone *pzone,unsigned int*regs)
{
	u32 pre_data[4] = {0};
	unsigned long  gpu_sensor_addr,base_addr;
	gpu_sensor_addr =
	    (unsigned long ) pzone->reg_base +LOCAL_SENSOR_ADDR_OFF;
	printk("sprd_thm_regs_set\n");
	base_addr = (unsigned long) pzone->reg_base;
	pre_data[0] = __thm_reg_read(base_addr + SENSOR_DET_PERI);
	pre_data[1] = __thm_reg_read(base_addr + SENSOR_MON_CTL);
	pre_data[2] = __thm_reg_read(base_addr + SENSOR_MON_PERI);
	pre_data[3] = __thm_reg_read(base_addr + SENSOR_CTRL);
	__thm_reg_write((base_addr + SENSOR_CTRL), 0x00, 0x01);
	__thm_reg_write((base_addr+ SENSOR_CTRL), 0x8, 0x08);
	__thm_reg_write((base_addr + SENSOR_DET_PERI), regs[0], pre_data[0]);
	__thm_reg_write((base_addr + SENSOR_MON_CTL), regs[1], pre_data[1]);
	__thm_reg_write((base_addr + SENSOR_MON_PERI), regs[2], pre_data[2] |0x100 );
	__thm_reg_write((base_addr + SENSOR_CTRL), regs[3], pre_data[3]);
	__thm_reg_write((base_addr + SENSOR_CTRL), 0x8, 0x8);

	pre_data[0] = __thm_reg_read(gpu_sensor_addr + SENSOR_DET_PERI);
	pre_data[1] = __thm_reg_read(gpu_sensor_addr + SENSOR_MON_CTL);
	pre_data[2] = __thm_reg_read(gpu_sensor_addr + SENSOR_MON_PERI);
	pre_data[3] = __thm_reg_read(gpu_sensor_addr + SENSOR_CTRL);
	__thm_reg_write((gpu_sensor_addr + SENSOR_CTRL), 0x00, 0x01);
	__thm_reg_write((gpu_sensor_addr+ SENSOR_CTRL), 0x8, 0x08);	
	__thm_reg_write((gpu_sensor_addr + SENSOR_DET_PERI), regs[0], pre_data[0]);
	__thm_reg_write((gpu_sensor_addr + SENSOR_MON_CTL), regs[1], pre_data[1]);
	__thm_reg_write((gpu_sensor_addr + SENSOR_MON_PERI), regs[2], pre_data[2] |0x100 );
	__thm_reg_write((gpu_sensor_addr + SENSOR_CTRL), regs[3], pre_data[3]);
	__thm_reg_write((gpu_sensor_addr + SENSOR_CTRL), 0x8, 0x8);
	return 0;
}
#endif
static int sprd_thm_temp_read(struct sprd_thermal_zone *pzone)
{
	u32 rawdata = 0;
	int cal_offset = 0;
	u32 sensor = pzone->sensor_id, local_sen_id = 0;
	unsigned long  local_sensor_addr;
	if(pzone->sensor_id == SPRD_GPU_SENSOR)
		local_sen_id = 1;
	local_sensor_addr =
	    (unsigned long ) pzone->reg_base + local_sen_id * LOCAL_SENSOR_ADDR_OFF;

	rawdata = __thm_reg_read(local_sensor_addr + SENSOR_TEMPER0_READ);
	rawdata = rawdata & RAW_READ_RANGE_MSK;
	cal_offset = arm_sen_cal_offset;

	THM_DEBUG("D thm sensor id:%d, cal_offset:%d, rawdata:0x%x\n", sensor,
			cal_offset, rawdata);
	return (sprd_thm_rawdata2temp( rawdata) -cal_offset);
}
#ifdef  THM_TEST
static int sprd_thm_trip_set(struct sprd_thermal_zone *pzone,int trip)
{
	THM_DEBUG("sprd_thm_trip_set trip=%d, temp=%ld,lowoff =%ld\n",
		trip,pzone->trip_tab->trip_points[trip].temp,pzone->trip_tab->trip_points[trip].lowoff);
	return sprd_thm_set_active_trip(pzone,current_trip_num);
}
#endif
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
static int sprd_thm_hw_init(struct sprd_thermal_zone *pzone)
{
	unsigned long  local_sensor_addr, base_addr = 0;
	u32 local_sen_id = 0;
	u32 raw_temp = 0;
	int cal_offset = 0,ret = 0;
	struct sprd_thm_platform_data *trip_tab = pzone->trip_tab;
	ret = sci_efuse_thermal_cal_get(&arm_sen_cal_offset) ;

	THM_DEBUG("arm_sen_cal_offset =%d,ret =%d\n",arm_sen_cal_offset,ret);
	base_addr = (unsigned long) pzone->reg_base;
	local_sen_id = pzone->sensor_id;
	local_sensor_addr = base_addr + local_sen_id * LOCAL_SENSOR_ADDR_OFF;

	printk(KERN_NOTICE "sprd_thm_hw_init 2713s_thm id:%d,base 0x%lx \n",
		pzone->sensor_id, base_addr);
	sci_glb_set(REG_AON_APB_APB_EB1, BIT_THM_EB);
#ifdef CONFIG_ARCH_SCX35LT8
	sci_glb_set(REG_AON_APB_APB_RTC_EB,
			(BIT_THM_RTC_EB | BIT_GPU_THMA_RTC_EB |
			BIT_GPU_THMA_RTC_AUTO_EN | BIT_CA53_LIT_THMA_RTC_EB |
			BIT_CA53_BIG_THMA_RTC_EB |BIT_ARM_THMA_RTC_AUTO_EN ) );
#else
	sci_glb_set(REG_AON_APB_APB_RTC_EB,
			(BIT_THM_RTC_EB | BIT_GPU_THMA_RTC_EB |
			BIT_GPU_THMA_RTC_AUTO_EN | BIT_ARM_THMA_RTC_EB |BIT_ARM_THMA_RTC_AUTO_EN ) );
#endif
	__thm_reg_write((base_addr + THM_CTRL), 0x3, 0);
	__thm_reg_write((base_addr + THM_INT_CTRL), 0x3, 0);

	__thm_reg_write((local_sensor_addr + SENSOR_INT_CTRL), 0, ~0);	//disable all int
	__thm_reg_write((local_sensor_addr + SENSOR_INT_CLR), ~0, 0);	//clr all int

		//set int
	if (trip_tab->num_trips > 0) {
		current_trip_num = 0;
		sprd_thm_set_active_trip(pzone,current_trip_num);
			//set overheat
		if (trip_tab->trip_points[trip_tab->num_trips - 1].type ==
			THERMAL_TRIP_CRITICAL) {
			raw_temp =
				sprd_thm_temp2rawdata( trip_tab->trip_points[trip_tab->num_trips - 1].temp - cal_offset);
				//set overheat int temp value
			__thm_reg_write((local_sensor_addr +
						 SENSOR_OVERHEAT_THRES),
						 raw_temp, RAW_TEMP_RANGE_MSK);
			__thm_reg_write((local_sensor_addr + SENSOR_INT_CTRL),	//enable int
						SEN_OVERHEAT_INT_BIT, 0);
		}
	}
	printk(KERN_NOTICE "sprd_thm_hw_init addr 0x:%lx,int ctrl 0x%x\n",
			local_sensor_addr,
			__thm_reg_read((local_sensor_addr + SENSOR_INT_CTRL)));

	__thm_reg_write((local_sensor_addr + SENSOR_DET_PERI), 0x4000, 0x4000);
	__thm_reg_write((local_sensor_addr + SENSOR_MON_CTL), 0x21, 0X21);	
	__thm_reg_write((local_sensor_addr + SENSOR_MON_PERI), 0x400, 0x100);
	__thm_reg_write((local_sensor_addr + SENSOR_CTRL), 0x031, 0x131);


	__thm_reg_write((local_sensor_addr + SENSOR_CTRL), 0x8, 0x8);	
	return 0;
}

static int sprd_thm_hw_disable_sensor(struct sprd_thermal_zone *pzone)
{
	u32 local_sen_id = 0;
	unsigned long  local_sensor_addr;
	if(pzone->sensor_id == SPRD_GPU_SENSOR)
		local_sen_id = 1;
	local_sensor_addr =
	    (unsigned long ) pzone->reg_base + local_sen_id * LOCAL_SENSOR_ADDR_OFF;
	// Sensor minitor disable
	__thm_reg_write((local_sensor_addr + SENSOR_CTRL), 0x00, 0x01);
	__thm_reg_write((local_sensor_addr + SENSOR_CTRL), 0x8, 0x8);

	return 0;
}

static int sprd_thm_hw_enable_sensor(struct sprd_thermal_zone *pzone)
{
	u32 local_sen_id = 0;
	unsigned long  local_sensor_addr;
	int ret = 0;
	if(pzone->sensor_id == SPRD_GPU_SENSOR)
		local_sen_id = 1;
	local_sensor_addr =
	    (unsigned long ) pzone->reg_base + local_sen_id * LOCAL_SENSOR_ADDR_OFF;
	// Sensor minitor enable
	THM_DEBUG("sprd_2713S_thm enable sensor sensor_ID:0x%x \n",pzone->sensor_id);
#ifdef CONFIG_ARCH_SCX35LT8
	sci_glb_set(REG_AON_APB_APB_RTC_EB,
			(BIT_THM_RTC_EB | BIT_GPU_THMA_RTC_EB |
			BIT_GPU_THMA_RTC_AUTO_EN | BIT_CA53_LIT_THMA_RTC_EB |
			BIT_CA53_BIG_THMA_RTC_EB |BIT_ARM_THMA_RTC_AUTO_EN ) );
#else
	sci_glb_set(REG_AON_APB_APB_RTC_EB,
			(BIT_THM_RTC_EB | BIT_GPU_THMA_RTC_EB |
			BIT_GPU_THMA_RTC_AUTO_EN | BIT_ARM_THMA_RTC_EB |BIT_ARM_THMA_RTC_AUTO_EN ) );
#endif

	__thm_reg_write((local_sensor_addr+ SENSOR_CTRL), 0x01, 0x01);
	__thm_reg_write((local_sensor_addr+ SENSOR_CTRL), 0x8, 0x8);

	return ret;
}


u16 int_ctrl_reg[SPRD_MAX_SENSOR];
static int sprd_thm_hw_suspend(struct sprd_thermal_zone *pzone)
{
	u32 local_sen_id = 0;
	unsigned long  local_sensor_addr;
	int ret = 0;
	if(pzone->sensor_id == SPRD_GPU_SENSOR)
		local_sen_id = 1;
	local_sensor_addr =
	    (unsigned long ) pzone->reg_base + local_sen_id * LOCAL_SENSOR_ADDR_OFF;
	int_ctrl_reg[pzone->sensor_id] = __thm_reg_read((local_sensor_addr + SENSOR_INT_CTRL));

	sprd_thm_hw_disable_sensor(pzone);

	__thm_reg_write((local_sensor_addr + SENSOR_INT_CTRL), 0, ~0);	//disable all int
	__thm_reg_write((local_sensor_addr + SENSOR_INT_CLR), ~0, 0);	//clr all int

	return ret;
}
static int sprd_thm_hw_resume(struct sprd_thermal_zone *pzone)
{
	u32 local_sen_id = 0;
	unsigned long  local_sensor_addr;
	int ret = 0;
	if(pzone->sensor_id == SPRD_GPU_SENSOR)
		local_sen_id = 1;
	local_sensor_addr =
	    (unsigned long ) pzone->reg_base + local_sen_id * LOCAL_SENSOR_ADDR_OFF;
	sprd_thm_hw_enable_sensor(pzone);
	__thm_reg_write((local_sensor_addr + SENSOR_INT_CLR), ~0, 0);	//clr all int
	__thm_reg_write((local_sensor_addr + SENSOR_INT_CTRL), int_ctrl_reg[pzone->sensor_id], ~0);	//enable int of saved
	__thm_reg_write((local_sensor_addr + SENSOR_CTRL), 0x9, 0);

	return ret;
}

static int sprd_thm_hw_irq_handle(struct sprd_thermal_zone *pzone)
{
	u32 local_sen_id = 0;
	unsigned long  local_sensor_addr;
	u32 int_sts;
	int ret = 0;
	u32 overhead_hot_tem_cur = 0;
	struct sprd_thm_platform_data *trip_tab = pzone->trip_tab;
	int temp;
	if(pzone->sensor_id == SPRD_GPU_SENSOR)
		local_sen_id = 1;
	local_sensor_addr =
	    (unsigned long ) pzone->reg_base + local_sen_id * LOCAL_SENSOR_ADDR_OFF;
	int_sts = __thm_reg_read(local_sensor_addr + SENSOR_INT_STS);

	__thm_reg_write((local_sensor_addr + SENSOR_INT_CLR), int_sts, ~0);	//CLR INT

	printk("sprd_thm_hw_irq_handle --------@@@------id:%d, int_sts :0x%x \n",
			pzone->sensor_id, int_sts);
	temp = sprd_thm_temp_read(pzone);
	printk("sprd_thm_hw_irq_handle ------$$$--------temp:%d\n", temp);

	overhead_hot_tem_cur = __thm_reg_read((local_sensor_addr + SENSOR_HOT_THRES))
											& RAW_TEMP_RANGE_MSK;

	if(SPRD_ARM_SENSOR != pzone->sensor_id){
		return ret;
	}
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

struct thm_handle_ops sprd_ddie_ops[2] =
{
	{
		.hw_init = sprd_thm_hw_init,
		.get_reg_base = sprd_thm_get_reg_base,
		.read_temp = sprd_thm_temp_read,
		.irq_handle = sprd_thm_hw_irq_handle,
		.suspend = sprd_thm_hw_suspend,
		.resume = sprd_thm_hw_resume,
		.trip_debug_set = sprd_thm_trip_set,
		.reg_debug_get = sprd_thm_regs_read,
		.reg_debug_set = sprd_thm_regs_set,
	},
	{
		.hw_init = sprd_thm_hw_init,
		.get_reg_base = sprd_thm_get_reg_base,
		.read_temp = sprd_thm_temp_read,
		.irq_handle = sprd_thm_hw_irq_handle,
		.suspend = sprd_thm_hw_suspend,
		.resume = sprd_thm_hw_resume,
		.trip_debug_set = sprd_thm_trip_set,
		.reg_debug_get = sprd_thm_regs_read,
		.reg_debug_set = sprd_thm_regs_set,
	}
};

static int __init sprd_ddie_thermal_init(void)
{
	if (0 == sprd_thm_chip_id_check()) {
		sprd_thm_add(&sprd_ddie_ops[0],"sprd_arm_thm",SPRD_ARM_SENSOR);
		sprd_thm_add(&sprd_ddie_ops[1],"sprd_gpu_thm",SPRD_GPU_SENSOR);
	}
	return 0;
}

static void __exit sprd_ddie_thermal_exit(void)
{
	sprd_thm_delete(SPRD_ARM_SENSOR);
	sprd_thm_delete(SPRD_GPU_SENSOR);
}

module_init(sprd_ddie_thermal_init);
module_exit(sprd_ddie_thermal_exit);
