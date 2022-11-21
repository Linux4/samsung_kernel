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
#include "thermal_core.h"

#define SPRD_THM_PMIC_DEBUG
#ifdef SPRD_THM_PMIC_DEBUG
#define THM_PMIC_DEBUG(format, arg...) printk("sprd PMIC thm: " "@@@" format, ## arg)
#else
#define THM_PMIC_DEBUG(format, arg...)
#endif


#define A_THM_CTRL            (0x0000)
#define A_THM_INT_CTRL       (0x0004)
#define A_SENSOR_CTRL         (0x0020)
#define A_SENSOR_DET_PERI     (0x0024)
#define A_SENSOR_INT_CTRL     (0x0028)
#define A_SENSOR_INT_STS      (0x002C)
#define A_SENSOR_INT_RAW_STS      (0x0030)
#define A_SENSOR_INT_CLR      (0x0034)
#define A_SENSOR_OVERHEAT_HOT_THRES   (0X0040)
#define A_SENSOR_HOT2NOR__HIGHOFF_THRES   (0X0044)
#define A_SENSOR_LOWOFF__COLD_THRES   (0X0048)
#define A_SENSOR_TEMPER0_READ	(0x0058)

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
#define LOCAL_SENSOR_ADDR_OFF 0x100
#define DELAY_TEMPERATURE 3

#define SEN_DET_PRECISION  (0x50)

#define TSMC_DOLPHINW4T_CHIP_ID_1  0x7715A001
#define TSMC_DOLPHINW4T_CHIP_ID_2  0x7715A003
#define TSMC_DOLPHINWT4T_CHIP_ID_1 0x8815A001

#define TEMP_TO_RAM_DEGREE 8
#define TEMP_LOW         (-40)
#define TEMP_HIGH        (120)
#define RAW_DATA_LOW         (623)
#define RAW_DATA_HIGH        (1030)


//static const u32 temp_to_raw[TEMP_TO_RAM_DEGREE + 1] = { RAW_DATA_LOW, 676, 725, 777, 828, 878, 928, 980,RAW_DATA_HIGH };

static u32 current_trip_num = 0;
static int pmic_sen_cal_offset = 0;

unsigned long SPRD_ADIE_THM_BASE = 0;
unsigned int SPRD_ADIE_THM_SIZE = 0;
static inline int __thm_pmic_reg_write(unsigned long reg, u16 bits, u16 clear_msk);
static inline u32 __thm_pmic_reg_read(unsigned long reg);
int sprd_thm_pmic_set_active_trip(struct sprd_thermal_zone *pzone, int trip );
u32 sprd_thm_temp2rawdata_a(u32 sensor, int temp);
int sprd_thm_rawdata2temp_a(u32 sensor, int rawdata);

static inline int __thm_pmic_reg_write(unsigned long reg, u16 bits, u16 clear_msk)
{
	THM_PMIC_DEBUG("cxk reg=%lx ANA_THM_BASE=%lx,SPRD_THM_BASE=%lx SPRD_THM_SIZE=%8x\n",
		reg,ANA_THM_BASE,SPRD_ADIE_THM_BASE,SPRD_ADIE_THM_SIZE);
	 if (reg >= ANA_THM_BASE && reg <= (ANA_THM_BASE + SPRD_ADIE_THM_SIZE)) {
		sci_adi_write(reg, bits, clear_msk);
	} else {
		printk(KERN_ERR "error thm reg0x:%lx \n", reg);
	}
	return 0;
}

static inline u32 __thm_pmic_reg_read(unsigned long reg)
{
	 if (reg >= ANA_THM_BASE && reg <= (ANA_THM_BASE + SPRD_ADIE_THM_SIZE)) {
		return sci_adi_read(reg);
	} else {
		printk(KERN_ERR "error thm reg0x:%lx \n", reg);
	}
	return 0;
}

int sprd_pmic_thm_get_reg_base(struct sprd_thermal_zone *pzone ,struct resource *regs)
{
	pzone->reg_base = (void __iomem *) ANA_THM_BASE;
	SPRD_ADIE_THM_BASE = (unsigned long) pzone->reg_base;
	SPRD_ADIE_THM_SIZE= resource_size(regs);
	return 0;
}
int sprd_thm_pmic_set_active_trip(struct sprd_thermal_zone *pzone, int trip )
{
#if 0
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
	THM_DEBUG("thm sensor trip:%d, temperature:%d \n", trip, trip_tab->trip_points[trip].temp);
	raw_temp = sprd_thm_temp2rawdata(pzone->sensor_id,
		trip_tab->trip_points[trip].temp - pmic_sen_cal_offset);
	if (raw_temp < RAW_TEMP_RANGE_MSK) {
	raw_temp++;
	}
	__thm_reg_write((local_sensor_addr + SENSOR_HOT_THRES),
						raw_temp, RAW_TEMP_RANGE_MSK);

	//set Hot2Normal int temp value
	raw_temp =
	sprd_thm_temp2rawdata(pzone->sensor_id, trip_tab->trip_points[trip].lowoff  + 2 - pmic_sen_cal_offset);
	__thm_reg_write((local_sensor_addr + SENSOR_HOT2NOR_THRES),
		raw_temp, RAW_TEMP_RANGE_MSK);

	raw_temp =
	sprd_thm_temp2rawdata(pzone->sensor_id, trip_tab->trip_points[trip].lowoff  + 1 - pmic_sen_cal_offset);
	__thm_reg_write((local_sensor_addr + SENSOR_HIGHOFF_THRES),
		raw_temp, RAW_TEMP_RANGE_MSK);

	//set cold int temp value
	raw_temp =
	sprd_thm_temp2rawdata(pzone->sensor_id, trip_tab->trip_points[trip].lowoff - pmic_sen_cal_offset);
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
#endif
	return 0;
}

u32 sprd_thm_temp2rawdata_a(u32 sensor, int temp)
{
	u32 high_bits, low_bits;
	int i;
	const short *high_tab;
	const short *low_tab;

	if (SPRD_PMIC_SENSOR != sensor){
		printk(KERN_ERR "Only for A sensor, error sensor id:%d \n", sensor);
		return 0;
	}
	high_tab = a_temp_search_high_152nm;
	low_tab = a_temp_search_low_152nm;

	if (temp < high_tab[0]) {
		printk(KERN_ERR "temp over low temp:%d \n", temp);
		return 0;
	}

	for (i = A_HIGH_TAB_SZ - 1; i >= 0; i--) {
		if (high_tab[i] <= temp)
			break;
	}
	if (i < 0) {
		i = 0;
	}
	temp -= high_tab[i];
	high_bits = i;

	for (i = A_LOW_TAB_SZ - 1; i >= 0; i--) {
		if (low_tab[i] <= temp)
			break;
	}
	if (i < 0) {
		i = 0;
	}
	low_bits = i;
	return ((high_bits << HIGH_BITS_OFFSET) | low_bits);

}
int sprd_thm_rawdata2temp_a(u32 sensor, int rawdata)
{
	const short *high_tab;
	const short *low_tab;

	if (SPRD_PMIC_SENSOR != sensor){
		printk(KERN_ERR "Only for A sensor, error sensor id:%d \n", sensor);
		return 0;
	}

	high_tab = a_temp_search_high_152nm;
	low_tab = a_temp_search_low_152nm;

	return high_tab[(rawdata >> HIGH_BITS_OFFSET) & 0x07] +
	    low_tab[rawdata & 0x0F];
}

int sprd_pmic_thm_temp_read(struct sprd_thermal_zone *pzone)
{
	u32 rawdata = 0;
	int cal_offset = 0;
	u32 sensor = pzone->sensor_id;

	rawdata = __thm_pmic_reg_read((unsigned long )pzone->reg_base + A_SENSOR_TEMPER0_READ);
	cal_offset = pmic_sen_cal_offset;
	THM_PMIC_DEBUG("A thm sensor id:%d, cal_offset:%d, rawdata:0x%x\n", sensor,
			  cal_offset, rawdata);
	return (sprd_thm_rawdata2temp_a(sensor, rawdata) + cal_offset);
}

int sprd_pmic_thm_hw_init(struct sprd_thermal_zone *pzone)
{
	unsigned long  local_sensor_addr, base_addr = 0;
	u32 local_sen_id = 0;
	u32 raw_temp = 0;
	int cal_offset = 0;
	int i;
	struct sprd_thm_platform_data *trip_tab = pzone->trip_tab;

	base_addr = (unsigned long) pzone->reg_base;

	THM_PMIC_DEBUG( "sprd_adie_thm_hw_init 2713s_thm id:%d,base 0x%lx \n",
		pzone->sensor_id, base_addr);

	 if (SPRD_PMIC_SENSOR == pzone->sensor_id) {
		// adie thm en
		sci_adi_set(ANA_REG_GLB_ARM_MODULE_EN, BIT_ANA_THM_EN);
		sci_adi_set(ANA_REG_GLB_RTC_CLK_EN,
			    BIT_RTC_THMA_AUTO_EN | BIT_RTC_THMA_EN |
			    BIT_RTC_THM_EN);

		// Reset Thermal
		sci_adi_set(ANA_REG_GLB_ARM_RST,
			    BIT_ANA_THM_SOFT_RST | BIT_ANA_THMA_SOFT_RST);
		for(i=0; i < 10000; i++);
		sci_adi_clr(ANA_REG_GLB_ARM_RST,
			    BIT_ANA_THM_SOFT_RST | BIT_ANA_THMA_SOFT_RST);

		cal_offset = pmic_sen_cal_offset = 0;
		local_sen_id = 0;

		__thm_pmic_reg_write((base_addr + A_THM_CTRL), 0x1 >> local_sen_id, 0);
		__thm_pmic_reg_write((base_addr + A_THM_INT_CTRL), 0x3, 0);	//enable top int

		local_sensor_addr = base_addr + local_sen_id * LOCAL_SENSOR_ADDR_OFF;

		__thm_pmic_reg_write((local_sensor_addr + A_SENSOR_INT_CTRL), 0, ~0);	//disable all int
		__thm_pmic_reg_write((local_sensor_addr + A_SENSOR_INT_CLR), ~0, 0);	//clr all int

		if (trip_tab->num_trips > 0) {
			//set hot
			if (trip_tab->trip_points[0].type == THERMAL_TRIP_ACTIVE) {
				raw_temp =
					sprd_thm_temp2rawdata_a(pzone->sensor_id,
							    trip_tab->trip_points[0].
							    temp - cal_offset);
				if (raw_temp < A_RAW_TEMP_RANGE_MSK) {
					raw_temp++;
				}
				//set hot int temp value
				__thm_pmic_reg_write((local_sensor_addr +
						 A_SENSOR_OVERHEAT_HOT_THRES), raw_temp,
						 A_RAW_TEMP_RANGE_MSK);
				raw_temp =
					sprd_thm_temp2rawdata_a(pzone->sensor_id,
							  trip_tab->trip_points[0].
							  temp - HOT2NOR_RANGE -
							  cal_offset);
				//set hot2nor int temp value
				__thm_pmic_reg_write((local_sensor_addr +
						 A_SENSOR_HOT2NOR__HIGHOFF_THRES),
						raw_temp << A_RAW_TEMP_OFFSET,
						A_RAW_TEMP_RANGE_MSK << A_RAW_TEMP_OFFSET);
				__thm_pmic_reg_write((local_sensor_addr + A_SENSOR_INT_CTRL),	//enable int
						A_SEN_HOT2NOR_INT_BIT | A_SEN_HOT_INT_BIT,
						0);
			}
		}
		//set overheat
		if (trip_tab->trip_points[trip_tab->num_trips - 1].type ==
		    THERMAL_TRIP_CRITICAL) {
			raw_temp =
			    sprd_thm_temp2rawdata_a(pzone->sensor_id,
						  trip_tab->trip_points
						  [trip_tab->num_trips -
						   1].temp - cal_offset);
			//set overheat int temp value
			__thm_pmic_reg_write((local_sensor_addr +
					 A_SENSOR_OVERHEAT_HOT_THRES),
					 raw_temp << A_RAW_TEMP_OFFSET,
					 A_RAW_TEMP_RANGE_MSK << A_RAW_TEMP_OFFSET);

			__thm_pmic_reg_write((local_sensor_addr + A_SENSOR_INT_CTRL),	//enable int
					 A_SEN_OVERHEAT_INT_BIT, 0);
		}

		THM_PMIC_DEBUG( "sprd_thm_hw_init addr 0x:%lx,int ctrl 0x%x\n",
			   	local_sensor_addr,
			   	__thm_pmic_reg_read((local_sensor_addr + A_SENSOR_INT_CTRL)));

		// Set sensor det period is 0.25S
		__thm_pmic_reg_write((local_sensor_addr + A_SENSOR_DET_PERI), 0x2000, 0x2000);
		__thm_pmic_reg_write((local_sensor_addr + A_SENSOR_CTRL), 0x01, 0x01);
		// Start the sensor by set Sen_set_rdy(bit3)
		__thm_pmic_reg_write((local_sensor_addr + A_SENSOR_CTRL), 0x8, 0x8);
	}
	else {
		printk(KERN_ERR "error thm sensor id:%d \n", pzone->sensor_id);
		return -EINVAL;
	}

	return 0;
}

int sprd_pmic_thm_hw_disable_sensor(struct sprd_thermal_zone *pzone)
{
	int ret = 0;
	THM_PMIC_DEBUG("sprd_2713S_thm disable PMIC sensor \n" );
	__thm_pmic_reg_write((unsigned long )(pzone->reg_base + A_SENSOR_CTRL), 0x0, 0x8);
	__thm_pmic_reg_write((unsigned long )(pzone->reg_base + A_SENSOR_CTRL), 0x00, 0x01);

	return ret;
}

int sprd_pmic_thm_hw_enable_sensor(struct sprd_thermal_zone *pzone)
{
	int ret = 0;
	// Sensor minitor enable
	THM_PMIC_DEBUG("sprd_2713S_thm enable PMIC sensor  \n");
	sci_adi_set(ANA_REG_GLB_RTC_CLK_EN,
		BIT_RTC_THMA_AUTO_EN | BIT_RTC_THMA_EN |
		BIT_RTC_THM_EN);
	__thm_pmic_reg_write((unsigned long )(pzone->reg_base + A_SENSOR_CTRL), 0x01, 0x01);
	__thm_pmic_reg_write((unsigned long )(pzone->reg_base + A_SENSOR_CTRL), 0x8, 0x8);


	return ret;
}


u16 int_pmic_ctrl_reg[SPRD_MAX_SENSOR];
int sprd_pmic_thm_hw_suspend(struct sprd_thermal_zone *pzone)
{
	u32 local_sen_id = 0;
	unsigned long  local_sensor_addr;
	int ret = 0;

	local_sensor_addr =
	    (unsigned long ) pzone->reg_base + local_sen_id * LOCAL_SENSOR_ADDR_OFF;
	int_pmic_ctrl_reg[pzone->sensor_id] = __thm_pmic_reg_read((local_sensor_addr + A_SENSOR_INT_CTRL));

	sprd_pmic_thm_hw_disable_sensor(pzone);
	__thm_pmic_reg_write((local_sensor_addr + A_SENSOR_INT_CTRL), 0, ~0);	//disable all int
	__thm_pmic_reg_write((local_sensor_addr + A_SENSOR_INT_CLR), ~0, 0);	//clr all int
	return ret;
}
int sprd_pmic_thm_hw_resume(struct sprd_thermal_zone *pzone)
{
	u32 local_sen_id = 0;
	unsigned long  local_sensor_addr;
	int ret = 0;

	local_sensor_addr =
	    (unsigned long ) pzone->reg_base + local_sen_id * LOCAL_SENSOR_ADDR_OFF;

	sprd_pmic_thm_hw_enable_sensor(pzone);
	__thm_pmic_reg_write((local_sensor_addr + A_SENSOR_INT_CLR), ~0, 0);	//clr all int
	__thm_pmic_reg_write((local_sensor_addr + A_SENSOR_INT_CTRL), int_pmic_ctrl_reg[pzone->sensor_id], ~0);	//enable int of saved
	__thm_pmic_reg_write((local_sensor_addr + A_SENSOR_CTRL), 0x9, 0);

	return ret;
}

int sprd_pmic_thm_hw_irq_handle(struct sprd_thermal_zone *pzone)
{
	#if 0
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
		pzone->trend_val = THERMAL_TREND_RAISING;
		if ((current_trip_num) >= (trip_tab->num_trips - 2)){
			current_trip_num = trip_tab->num_trips - 2;
			return ret;
		}
		if (temp >= trip_tab->trip_points[current_trip_num].temp - TRIP_TEMP_OFFSET){
			current_trip_num++;
			sprd_thm_set_active_trip(pzone,current_trip_num);
		}
	}else if (int_sts & SEN_LOWOFF_INT_BIT){
		if (temp < trip_tab->trip_points[current_trip_num].lowoff + TRIP_TEMP_OFFSET){
			current_trip_num--;
			sprd_thm_set_active_trip(pzone,current_trip_num);
		}
	}else{
		THM_DEBUG("sprd_thm_hw_irq_handle NOT a HOT or LOWOFF interrupt \n");
		return ret;
	}

	return ret;
#endif
	return 0;
}
struct thm_handle_ops sprd_pmic_ops =
{
	.hw_init = sprd_pmic_thm_hw_init,
	.get_reg_base = sprd_pmic_thm_get_reg_base,
	.read_temp = sprd_pmic_thm_temp_read,
	.irq_handle = sprd_pmic_thm_hw_irq_handle,
	.suspend = sprd_pmic_thm_hw_suspend,
	.resume = sprd_pmic_thm_hw_resume,
};
static int __init sprd_pmic_thermal_init(void)
{
	sprd_thm_add(&sprd_pmic_ops,"sprd_pmic_thm",SPRD_PMIC_SENSOR);
	return 0;
}

static void __exit sprd_pmic_thermal_exit(void)
{
	sprd_thm_delete(SPRD_PMIC_SENSOR);
}

module_init(sprd_pmic_thermal_init);
module_exit(sprd_pmic_thermal_exit);



