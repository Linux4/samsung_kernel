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
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/err.h>
#include<linux/string.h>
#include <soc/sprd/arch_misc.h>
#include <soc/sprd/hardware.h>
#include <linux/thermal.h>
#include <linux/sprd_thm.h>
#include "thm.h"

#ifdef CONFIG_OF
#include <linux/slab.h>
#include <linux/of_device.h>
#include <linux/of_irq.h>
#endif

#define THM_SENOR_NUM 8
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
#define SEN_OVERHEAT_ALARM_EN (1 << 7)
#define SEN0_OVERHEAT_ALARM_EN (1 << 8)


#define RAW_TEMP_RANGE_MSK  0x3FFF
#define RAW_READ_RANGE_MSK  0x7FFF

#define HIGH_BITS_OFFSET   4

#define HOT2NOR_RANGE   15
#define LOCAL_SENSOR_ADDR_OFF  0x100
#define DELAY_TEMPERATURE 3

#define LOCAL_THM_ADDR_OFF  0x200


#define SEN_DET_PRECISION  (0x50)

#define TSMC_DOLPHINW4T_CHIP_ID_1  0x7715A001
#define TSMC_DOLPHINW4T_CHIP_ID_2  0x7715A003
#define TSMC_DOLPHINWT4T_CHIP_ID_1 0x8815A001

#define TEMP_TO_RAM_DEGREE 8
#define TEMP_LOW         (-40000)
#define TEMP_HIGH        (120000)
#ifdef CONFIG_ARCH_SCX20
#define RAW_DATA_LOW         (627)
#define RAW_DATA_HIGH        (1075)
#else
#define RAW_DATA_LOW         (623)
#define RAW_DATA_HIGH        (1030)
#endif
#define  CRITIC_TEMP	80000

#define RAW_ADC_LOW         (550)
#define RAW_ADC_HIGH        (1200)

//static const u32 temp_to_raw[TEMP_TO_RAM_DEGREE + 1] = { RAW_DATA_LOW, 676, 725, 777, 828, 878, 928, 980,RAW_DATA_HIGH };

unsigned long SPRD_THM_BASE = 0;
unsigned int SPRD_THM_SIZE = 0;
extern int sprd_thermal_init(struct sprd_thermal_zone *pzone);
extern void sprd_thermal_remove(struct sprd_thermal_zone *pzone);

static inline void __thm_reg_write(unsigned long reg, u16 bits, u16 clear_msk);
static inline u32 __thm_reg_read(unsigned long reg);
int sprd_thm_set_active_trip(struct sprd_thermal_zone *pzone, int trip );
u32 sprd_thm_temp2rawdata( int temp);

static inline void __thm_reg_write(unsigned long reg, u16 bits, u16 clear_msk)
{
		__raw_writel(((__raw_readl((volatile void *)reg) & ~clear_msk) | bits), ((volatile void *)reg));
}

static inline u32 __thm_reg_read(unsigned long reg)
{
		return __raw_readl((volatile void *)reg);
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
	if ((rawdata < RAW_ADC_LOW) || (rawdata > RAW_ADC_HIGH))
		return 0;
	temp_result = TEMP_LOW +
		(rawdata - RAW_DATA_LOW) * (TEMP_HIGH - TEMP_LOW) / (RAW_DATA_HIGH - RAW_DATA_LOW);

	return temp_result;
}

static unsigned long sprd_thm_temp_read(struct sprd_thermal_zone *pzone)
{
	u32 rawdata = 0;
	int cal_offset = 0;
	u32 sensor = pzone->sensor_id, local_sen_id = 0;
	unsigned long  local_sensor_addr;
	unsigned long  temp;
	if(pzone->sensor_id == SPRD_GPU_SENSOR)
			local_sen_id = 1;
	local_sensor_addr = pzone->reg_base + local_sen_id * LOCAL_SENSOR_ADDR_OFF;
	rawdata = __thm_reg_read(local_sensor_addr + SENSOR_TEMPER0_READ);
	rawdata = rawdata & RAW_READ_RANGE_MSK;
	cal_offset = pzone->thm_cal;
	temp = sprd_thm_rawdata2temp( rawdata) -cal_offset;
	printk("sensor id:%d, rawdata:0x%x, temp:%lu\n", sensor, rawdata,temp);
	return temp;

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

static int sprd_thm_trip_set(struct sprd_thermal_zone *pzone,int trip)
{
	THM_DEBUG("sprd_thm_trip_set trip=%d, temp=%ld,lowoff =%ld\n",
		trip,pzone->trip_tab->trip_points[trip].temp,pzone->trip_tab->trip_points[trip].lowoff);
	return 0;
}

#endif

int sprd_thm_get_trend(struct sprd_thermal_zone *pzone, int trip, enum thermal_trend *ptrend)
{
	*ptrend = pzone->trend_val;
	return 0;
}

int sprd_thm_get_hyst(struct sprd_thermal_zone *pzone, int trip, unsigned long *physt)
{

	struct sprd_thm_platform_data *trip_tab = pzone->trip_tab;
	if (trip >= trip_tab->num_trips - 2){
	*physt = 0;
	}else{
	*physt = trip_tab->trip_points[trip].temp - trip_tab->trip_points[trip + 1].lowoff;
	}
	return 0;
}

int init_flag=0;

static int sprd_thm_hw_init(struct sprd_thermal_zone *pzone)
{
	unsigned long  local_sensor_addr, base_addr = 0;
	u32 local_sen_id = 0;
	u32 raw_temp = 0;
	int ret = 0;
	int i;
	struct sprd_thm_platform_data *trip_tab = pzone->trip_tab;
	ret = sci_efuse_thermal_cal_get(&pzone->thm_cal) ;
	THM_DEBUG("arm_sen_cal_offset =%d,ret =%d\n",pzone->thm_cal,ret);
	if(pzone->sensor_id == SPRD_GPU_SENSOR)
			local_sen_id = 1;
	base_addr = (unsigned long) pzone->reg_base ;
	local_sensor_addr = base_addr + local_sen_id * LOCAL_SENSOR_ADDR_OFF;

	printk(KERN_NOTICE "sprd_thm_hw_init 2713s_thm id:%d,base 0x%lx \n",
		pzone->sensor_id, base_addr);
	if(init_flag ==0)
		{
			sci_glb_set(REG_AON_APB_APB_EB1, BIT_THM_EB);
			sci_glb_set(REG_AON_APB_APB_RTC_EB,
			(BIT_THM_RTC_EB | BIT_GPU_THMA_RTC_EB |
			BIT_GPU_THMA_RTC_AUTO_EN | BIT_ARM_THMA_RTC_EB |BIT_ARM_THMA_RTC_AUTO_EN ) );
			sci_glb_set(REG_AON_APB_APB_RST1,BIT_THM_SOFT_RST);
			for(i=0; i < 10000; i++);
			sci_glb_clr(REG_AON_APB_APB_RST1,BIT_THM_SOFT_RST);
			__thm_reg_write((base_addr + THM_CTRL), 0x3, 0);
			__thm_reg_write((base_addr + THM_INT_CTRL), 0x1, 0);
			init_flag =1;
		}
			__thm_reg_write((local_sensor_addr + SENSOR_INT_CTRL), 0, ~0);	//disable all int
			__thm_reg_write((local_sensor_addr + SENSOR_INT_CLR), ~0, 0);	//clr all int
#if 0
	if (SPRD_ARM_SENSOR== pzone->sensor_id){
		if (trip_tab->trip_points[trip_tab->num_trips - 1].type ==
			THERMAL_TRIP_CRITICAL) {
			raw_temp =
			sprd_thm_temp2rawdata( trip_tab->trip_points[trip_tab->num_trips - 1].temp - cal_offset);
			}
//		raw_temp = sprd_thm_temp2rawdata(CRITIC_TEMP - cal_offset);
		__thm_reg_write((local_sensor_addr + SENSOR_OVERHEAT_THRES),raw_temp, RAW_TEMP_RANGE_MSK);//set overheat  temp value
		__thm_reg_write((base_addr + THM_INT_CTRL), SEN0_OVERHEAT_ALARM_EN, 0);
		__thm_reg_write((local_sensor_addr + SENSOR_INT_CTRL),SEN_OVERHEAT_ALARM_EN, 0);
	}
#endif
	printk(KERN_NOTICE "sprd_thm_hw_init addr 0x:%lx,int ctrl 0x%x\n",
			local_sensor_addr,
			__thm_reg_read((local_sensor_addr + SENSOR_INT_CTRL)));

	__thm_reg_write((local_sensor_addr + SENSOR_DET_PERI), 0x4000, 0x4000);
	__thm_reg_write((local_sensor_addr + SENSOR_MON_CTL), 0x21, 0X21);
	__thm_reg_write((local_sensor_addr + SENSOR_MON_PERI), 0x400, 0x100);
	if (SPRD_ARM_SENSOR== pzone->sensor_id)
		__thm_reg_write((base_addr + SENSOR_CTRL + LOCAL_SENSOR_ADDR_OFF), 0x030, 0x030);
	__thm_reg_write((local_sensor_addr + SENSOR_CTRL), 0x39, 0x030);
	return 0;
}

int sprd_thm_hw_disable_sensor(struct sprd_thermal_zone *pzone)
{
	int ret = 0;
	/* Sensor minitor disable */
	if (SPRD_ARM_SENSOR == pzone->sensor_id) {
		/*__thm_reg_write((u32)(pzone->reg_base + SENSOR_CTRL), 0x00, 0x01);*/
		__thm_reg_write((pzone->reg_base + SENSOR_CTRL), 0x8, 0x01);
	} else if (SPRD_GPU_SENSOR == pzone->sensor_id) {
		/*__thm_reg_write((u32)(pzone->reg_base + SENSOR_CTRL+ LOCAL_SENSOR_ADDR_OFF), 0x00, 0x01);*/
		__thm_reg_write((pzone->reg_base + SENSOR_CTRL + LOCAL_SENSOR_ADDR_OFF), 0x8, 0x01);
	} else {
		THM_DEBUG("the sensor id is error \n");
	}
	return ret;
}

int sprd_thm_hw_enable_sensor(struct sprd_thermal_zone *pzone)
{
	int ret = 0;
	// Sensor minitor enable
	THM_DEBUG("sprd_2713S_thm enable sensor sensor_ID:0x%x \n",pzone->sensor_id);
	if (SPRD_ARM_SENSOR == pzone->sensor_id) {
		sci_glb_set(REG_AON_APB_APB_RTC_EB,
				(BIT_THM_RTC_EB | BIT_GPU_THMA_RTC_EB |
				BIT_GPU_THMA_RTC_AUTO_EN | BIT_ARM_THMA_RTC_EB | BIT_ARM_THMA_RTC_AUTO_EN));
		__thm_reg_write((pzone->reg_base + SENSOR_CTRL + LOCAL_SENSOR_ADDR_OFF), 0x030, 0x0);
		__thm_reg_write((pzone->reg_base + SENSOR_CTRL), 0x31, 0x0);
	} else if (SPRD_GPU_SENSOR == pzone->sensor_id) {
		/*sci_glb_set(REG_AON_APB_APB_RTC_EB,
				(BIT_THM_RTC_EB | BIT_GPU_THMA_RTC_EB |
				BIT_GPU_THMA_RTC_AUTO_EN | BIT_ARM_THMA_RTC_EB |BIT_ARM_THMA_RTC_AUTO_EN ) );*/
		__thm_reg_write((pzone->reg_base + SENSOR_CTRL+LOCAL_SENSOR_ADDR_OFF), 0x31, 0x0);
	} else {
		THM_DEBUG("the sensor id is error \n");
	}

	return ret;
}


u16 int_ctrl_reg[SPRD_MAX_SENSOR];
int sprd_thm_hw_suspend(struct sprd_thermal_zone *pzone)
{
	unsigned long local_sensor_addr;
	int ret = 0;

	local_sensor_addr = pzone->reg_base ;
	/*int_ctrl_reg[pzone->sensor_id] = __thm_reg_read((local_sensor_addr + SENSOR_INT_CTRL));*/

	sprd_thm_hw_disable_sensor(pzone);
	if (SPRD_ARM_SENSOR == pzone->sensor_id) {
		int_ctrl_reg[pzone->sensor_id] = __thm_reg_read((local_sensor_addr + SENSOR_INT_CTRL));
		__thm_reg_write((local_sensor_addr + SENSOR_INT_CTRL), 0, ~0);	//disable all int
		__thm_reg_write((local_sensor_addr + SENSOR_INT_CLR), ~0, 0);	//clr all int

	} else if (SPRD_GPU_SENSOR == pzone->sensor_id) {
		int_ctrl_reg[pzone->sensor_id] =
			__thm_reg_read((local_sensor_addr + SENSOR_INT_CTRL+LOCAL_SENSOR_ADDR_OFF));
		__thm_reg_write((local_sensor_addr + SENSOR_INT_CTRL+LOCAL_SENSOR_ADDR_OFF), 0, ~0);	/*disable all int*/
		__thm_reg_write((local_sensor_addr + SENSOR_INT_CTRL+LOCAL_SENSOR_ADDR_OFF), ~0, 0);	/*clr all int*/

	} else {
		THM_DEBUG("the sensor id is error \n");
	}
	return ret;
}
int sprd_thm_hw_resume(struct sprd_thermal_zone *pzone)
{
	unsigned long local_sensor_addr;
	int ret = 0;

	local_sensor_addr = pzone->reg_base ;

	sprd_thm_hw_enable_sensor(pzone);
	if (SPRD_ARM_SENSOR == pzone->sensor_id) {
		__thm_reg_write((local_sensor_addr + SENSOR_INT_CLR), ~0, 0);	//clr all int
		__thm_reg_write((local_sensor_addr + SENSOR_INT_CTRL), int_ctrl_reg[pzone->sensor_id], ~0);	//enable int of saved
		__thm_reg_write((local_sensor_addr + SENSOR_CTRL+LOCAL_SENSOR_ADDR_OFF), 0x30, 0);
		__thm_reg_write((local_sensor_addr + SENSOR_CTRL), 0x39, 0);
	} else if (SPRD_GPU_SENSOR == pzone->sensor_id) {
		__thm_reg_write((local_sensor_addr + SENSOR_INT_CLR+LOCAL_SENSOR_ADDR_OFF), ~0, 0);	/*clr all int*/
		__thm_reg_write((local_sensor_addr + SENSOR_INT_CTRL+LOCAL_SENSOR_ADDR_OFF), int_ctrl_reg[pzone->sensor_id], ~0);	/*enable int of saved*/
		__thm_reg_write((local_sensor_addr + SENSOR_CTRL+LOCAL_SENSOR_ADDR_OFF), 0x39, 0);
	} else {
		THM_DEBUG("the sensor id is error \n");
	}
	return ret;
}

struct thm_handle_ops sprd_ddie_ops =
{
		.hw_init = sprd_thm_hw_init,
		.read_temp = sprd_thm_temp_read,
		.get_trend = sprd_thm_get_trend,
		.get_hyst = sprd_thm_get_hyst,
		.suspend = sprd_thm_hw_suspend,
		.resume = sprd_thm_hw_resume,
		.trip_debug_set = sprd_thm_trip_set,
		.reg_debug_get = sprd_thm_regs_read,
		.reg_debug_set = sprd_thm_regs_set,
};

#ifdef CONFIG_OF
static struct sprd_thm *thermal_detect_parse_dt(
                         struct device *dev)
{
	struct sprd_thm_platform_data *pdata;
	struct sprd_thm *thm_data;
	struct device_node *np = dev->of_node,*sensor_child = NULL;
	int nsensor,sensor_id;
	int ret;
	u32 trip_points_critical,trip_num;
	int i = 0,j = 0,temp_interval;
	char prop_name[32];
	const char *tmp_str;
	const char *thm_name;
	u32 tmp_data,tmp_lowoff;
	nsensor = of_get_child_count(np);
	if (nsensor == 0) {
		dev_err(dev, "fail to get child node \n");
		goto fail;
	}
	printk("the nubutton %d\n",nsensor);
	thm_data = kzalloc(sizeof(*thm_data), GFP_KERNEL);
	if (!thm_data) {
		dev_err(dev, "could not allocate memory for platform sprd_thm\n");
		return NULL;
	}
	thm_data->nsensor = nsensor;
	pdata = kzalloc(thm_data->nsensor*sizeof(*pdata), GFP_KERNEL);
	if (!pdata) {
		dev_err(dev, "could not allocate memory for platform data\n");
		goto fail;
	}
	thm_data->sprd_data = pdata;
	for_each_child_of_node(np,sensor_child) {
	ret = of_property_read_u32(sensor_child, "trip-points-critical", &trip_points_critical);
	if(ret){
		dev_err(dev, "fail to get trip_points_critical\n");
		goto fail;
	}
	ret = of_property_read_u32(sensor_child, "trip-num", &trip_num);
	if(ret){
		dev_err(dev, "fail to get trip_num\n");
		goto fail;
	}
	ret = of_property_read_u32(sensor_child, "id", &sensor_id);
	if (ret) {
		dev_err(dev, "fail to get id\n");
		goto fail;
	}
	ret = of_property_read_u32(sensor_child, "temp-inteval", &temp_interval);
	if(ret){
		dev_err(dev, "fail to get temp-inteval\n");
		goto fail;
	}
	of_property_read_string(sensor_child, "thermal-name",
					&thm_name);
	strcpy(pdata->thm_name, thm_name);
	for (i = 0; i <trip_num-1; ++i) {
		sprintf(prop_name, "trip%d-temp-active", i);
		if (of_property_read_u32(sensor_child, prop_name, &tmp_data)){
			dev_err(dev, "fail to get trip%d-temp-active\n",i);
			goto fail;
		}
		pdata->trip_points[i].temp = tmp_data;
        sprintf(prop_name, "trip%d-temp-lowoff", i);
		if (of_property_read_u32(sensor_child, prop_name, &tmp_lowoff)){
			dev_err(dev, "fail to get trip%d-temp-lowoff\n",i);
			goto fail;
		}
		pdata->trip_points[i].lowoff = tmp_lowoff;
		sprintf(prop_name, "trip%d-type", i);
		if (of_property_read_string(sensor_child, prop_name, &tmp_str))
			goto  fail;

		if (!strcmp(tmp_str, "active"))
			pdata->trip_points[i].type = THERMAL_TRIP_ACTIVE;
		else if (!strcmp(tmp_str, "passive"))
			pdata->trip_points[i].type = THERMAL_TRIP_PASSIVE;
		else if (!strcmp(tmp_str, "hot"))
			pdata->trip_points[i].type = THERMAL_TRIP_HOT;
		else if (!strcmp(tmp_str, "critical"))
			pdata->trip_points[i].type = THERMAL_TRIP_CRITICAL;
		else
			goto  fail;

		sprintf(prop_name, "trip%d-cdev-num", i);
		if (of_property_read_u32(sensor_child, prop_name, &tmp_data))
			goto  fail;
		for (j = 0; j < tmp_data; j++) {
			sprintf(prop_name, "trip%d-cdev-name%d", i, j);
			if (of_property_read_string(sensor_child, prop_name, &tmp_str))
				goto  fail;
			strcpy(pdata->trip_points[i].cdev_name[j], tmp_str);
			dev_info(dev,"cdev name: %s \n", pdata->trip_points[i].cdev_name[j]);
		}
		dev_info(dev, "trip[%d] temp: %lu lowoff: %lu\n",
					i, pdata->trip_points[i].temp, pdata->trip_points[i].lowoff);
	}
	pdata->trip_points[i].temp = trip_points_critical;
	pdata->trip_points[i].type = THERMAL_TRIP_CRITICAL;
	dev_info(dev, "trip[%d] temp: %lu \n",
					i, pdata->trip_points[i].temp);
	pdata->num_trips = trip_num;
	pdata->sensor_id = sensor_id;
	pdata->temp_interval = temp_interval;
	printk("the trip_num=%d,sensor_id =%d\n",pdata->num_trips,pdata->sensor_id);
	pdata++;
	};
	return thm_data;

fail:
	kfree(pdata);
	kfree(thm_data);
	return NULL;

}
#endif

static int sprd_ddie_thm_probe(struct platform_device *pdev)
{
	struct sprd_thm_platform_data *ptrips = NULL;
	struct sprd_thermal_zone *pzone = NULL;
	struct resource *res;
	struct sprd_thm *pthm = NULL;
	unsigned long ddie_thm_base;
	int ret,i;
#ifdef CONFIG_OF
	struct device_node *np = pdev->dev.of_node;
	if (!np) {
		dev_err(&pdev->dev, "device node not found\n");
		return -EINVAL;
	}
#endif
	printk("sprd_ddie_thm_probe start\n");
	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	ddie_thm_base = (unsigned long)devm_ioremap_resource(&pdev->dev, res);
	if (!ddie_thm_base) {
		pr_err("thermal ioremap failed!\n");
		return -ENOMEM;
	}
#ifdef CONFIG_OF
	pthm = thermal_detect_parse_dt(&pdev->dev);
#else
	pthm = dev_get_platdata(&pdev->dev);
#endif
	if (!pthm){
	dev_err(&pdev->dev, "not found pthm\n");
		return -EINVAL;
	}
	for (i = 0; i < pthm->nsensor; i++) {
	pzone = devm_kzalloc(&pdev->dev, sizeof(*pzone), GFP_KERNEL);
	mutex_init(&pzone->th_lock);
	mutex_lock(&pzone->th_lock);
	if (!pzone)
		return -ENOMEM;
	pzone->reg_base= ddie_thm_base;
	ptrips = &pthm->sprd_data[i];
	pzone->trip_tab = ptrips;
	pzone->temp_inteval = ptrips->temp_interval;
	pzone->sensor_id = ptrips->sensor_id;
	memcpy(pzone->thermal_zone_name,
							ptrips->thm_name, strlen(ptrips->thm_name)+1);
	printk("the thermal-name1 is %s\n",pzone->thermal_zone_name);
	pzone->ops = &sprd_ddie_ops;
	ret = sprd_thm_hw_init(pzone);
	if(ret){
		dev_err(&pdev->dev, " pzone hw init error id =%d\n",pzone->sensor_id);
		return -ENODEV;
	}
    ret = sprd_thermal_init(pzone);
	if(ret){
		dev_err(&pdev->dev, " pzone sw init error id =%d\n",pzone->sensor_id);
		return -ENODEV;
	}
	platform_set_drvdata(pdev, pzone);
	printk("sprd_ddie_thm_probe---end\n");
	mutex_unlock(&pzone->th_lock);
	pzone++;
	}
	return 0;
}

static int sprd_ddie_thm_remove(struct platform_device *pdev)
{

	struct sprd_thermal_zone *pzone = platform_get_drvdata(pdev);
	sprd_thermal_remove(pzone);
	return 0;
}

static int sprd_ddie_thm_suspend(struct platform_device *pdev,
				pm_message_t state)
{
	struct sprd_thermal_zone *pzone = platform_get_drvdata(pdev);
	flush_delayed_work(&pzone->thm_read_work);
	//flush_delayed_work(&pzone->thm_logtime_work);
	flush_delayed_work(&pzone->resume_delay_work);
	pzone->ops->suspend(pzone);
	return 0;
}

static int sprd_ddie_thm_resume(struct platform_device *pdev)
{

	struct sprd_thermal_zone *pzone = platform_get_drvdata(pdev);
	schedule_delayed_work(&pzone->resume_delay_work, (HZ * 1));
	schedule_delayed_work(&pzone->thm_read_work, (HZ * 5));
	//schedule_delayed_work(&pzone->thm_logtime_work, (HZ * 7));
	//pzone->ops->resume(pzone);
	return 0;
}

static const struct of_device_id thermal_of_match[] = {
	{ .compatible = "sprd,ddie-thermal", },
       {}
};

static struct platform_driver sprd_thermal_driver = {
	.probe = sprd_ddie_thm_probe,
	.suspend = sprd_ddie_thm_suspend,
	.resume = sprd_ddie_thm_resume,
	.remove = sprd_ddie_thm_remove,
	.driver = {
		   .owner = THIS_MODULE,
		   .name = "ddie-thermal",
		   .of_match_table = of_match_ptr(thermal_of_match),
		   },
};
static int __init sprd_ddie_thermal_init(void)
{
	return platform_driver_register(&sprd_thermal_driver);
}

static void __exit sprd_ddie_thermal_exit(void)
{
	platform_driver_unregister(&sprd_thermal_driver);
}


device_initcall_sync(sprd_ddie_thermal_init);
module_exit(sprd_ddie_thermal_exit);
MODULE_AUTHOR("Freeman Liu <freeman.liu@spreadtrum.com>");
MODULE_DESCRIPTION("sprd thermal driver");
MODULE_LICENSE("GPL");

