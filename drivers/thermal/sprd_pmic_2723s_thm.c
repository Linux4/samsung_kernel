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
#include <linux/kernel.h>
#include <linux/err.h>
#include <linux/interrupt.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/slab.h>
#include <linux/spi/spi.h>
#include <linux/of_device.h>
#include <linux/io.h>
#include <linux/sprd_thm.h>

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
#define THM_SENOR_NUM 8

//static const u32 temp_to_raw[TEMP_TO_RAM_DEGREE + 1] = { RAW_DATA_LOW, 676, 725, 777, 828, 878, 928, 980,RAW_DATA_HIGH };

static int pmic_sen_cal_offset = 0;

unsigned long SPRD_ADIE_THM_BASE = 0;
unsigned int SPRD_ADIE_THM_SIZE = 0;

extern int sprd_thermal_init(struct sprd_thermal_zone *pzone);
extern void sprd_thermal_remove(struct sprd_thermal_zone *pzone);

static inline int __thm_pmic_reg_write(unsigned long reg, u16 bits, u16 clear_msk);
static inline u32 __thm_pmic_reg_read(unsigned long reg);

static inline int __thm_pmic_reg_write(unsigned long reg, u16 bits, u16 clear_msk)
{
		sci_adi_write(reg, bits, clear_msk);
		return 0;
}

static inline u32 __thm_pmic_reg_read(unsigned long reg)
{
		return sci_adi_read(reg);
}
static u32 sprd_thm_temp2rawdata_a(u32 sensor, int temp)
{
	u32 high_bits, low_bits;
	int i;
	const short *high_tab;
	const short *low_tab;
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
static int sprd_thm_rawdata2temp_a(u32 sensor, int rawdata)
{
	const short *high_tab;
	const short *low_tab;
	high_tab = a_temp_search_high_152nm;
	low_tab = a_temp_search_low_152nm;

	return high_tab[(rawdata >> HIGH_BITS_OFFSET) & 0x07] +
	    low_tab[rawdata & 0x0F];
}

static unsigned long sprd_pmic_thm_temp_read(struct sprd_thermal_zone *pzone)
{
	u32 rawdata = 0;
	int cal_offset = 0;
	u32 sensor = pzone->sensor_id;

	rawdata = __thm_pmic_reg_read(pzone->reg_base + A_SENSOR_TEMPER0_READ);
	cal_offset = pmic_sen_cal_offset;
	THM_PMIC_DEBUG("A thm sensor id:%d, cal_offset:%d, rawdata:0x%x\n", sensor,
			  cal_offset, rawdata);
	return (sprd_thm_rawdata2temp_a(sensor, rawdata)*1000 + cal_offset*1000);
}

static int sprd_pmic_thm_hw_init(struct sprd_thermal_zone *pzone)
{
	unsigned long  local_sensor_addr, base_addr = 0;
	u32 local_sen_id = 0;
	int cal_offset = 0;
	int i;
	//struct sprd_thm_platform_data *trip_tab = pzone->trip_tab;
	base_addr = pzone->reg_base;
	local_sensor_addr = pzone->reg_base;

	THM_PMIC_DEBUG( "sprd_adie_thm_hw_init 2713s_thm id:%d,base 0x%lx \n",
		pzone->sensor_id, base_addr);
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
		__thm_pmic_reg_write((local_sensor_addr + A_SENSOR_INT_CTRL), 0, ~0);	//disable all int
		__thm_pmic_reg_write((local_sensor_addr + A_SENSOR_INT_CLR), ~0, 0);	//clr all int
		THM_PMIC_DEBUG( "sprd_thm_hw_init addr 0x:%lx,int ctrl 0x%x\n",
			   	local_sensor_addr,
			   	__thm_pmic_reg_read((local_sensor_addr + A_SENSOR_INT_CTRL)));

		// Set sensor det period is 0.25S
		__thm_pmic_reg_write((local_sensor_addr + A_SENSOR_DET_PERI), 0x2000, 0x2000);
		__thm_pmic_reg_write((local_sensor_addr + A_SENSOR_CTRL), 0x01, 0x01);
		// Start the sensor by set Sen_set_rdy(bit3)
		__thm_pmic_reg_write((local_sensor_addr + A_SENSOR_CTRL), 0x8, 0x8);

	return 0;
}

static int sprd_pmic_thm_hw_disable_sensor(struct sprd_thermal_zone *pzone)
{
	int ret = 0;
	THM_PMIC_DEBUG("sprd_2713S_thm disable PMIC sensor \n" );
	__thm_pmic_reg_write((unsigned long )(pzone->reg_base + A_SENSOR_CTRL), 0x0, 0x8);
	__thm_pmic_reg_write((unsigned long )(pzone->reg_base + A_SENSOR_CTRL), 0x00, 0x01);

	return ret;
}

static int sprd_pmic_thm_hw_enable_sensor(struct sprd_thermal_zone *pzone)
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
static int sprd_pmic_thm_hw_suspend(struct sprd_thermal_zone *pzone)
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
static int sprd_pmic_thm_hw_resume(struct sprd_thermal_zone *pzone)
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

static int sprd_pmic_thm_get_trend(struct sprd_thermal_zone *pzone, int trip, enum thermal_trend *ptrend)
{
	*ptrend = pzone->trend_val;
	return 0;
}

static int sprd_pmic_thm_get_hyst(struct sprd_thermal_zone *pzone, int trip, unsigned long *physt)
{
	struct sprd_thm_platform_data *trip_tab = pzone->trip_tab;
	if (trip >= trip_tab->num_trips - 2){
	*physt = 0;
	}else{
	*physt = trip_tab->trip_points[trip].temp - trip_tab->trip_points[trip + 1].lowoff;
	}
	return 0;
}

struct thm_handle_ops sprd_pmic_ops =
{
	.hw_init = sprd_pmic_thm_hw_init,
	.read_temp = sprd_pmic_thm_temp_read,
	.get_trend = sprd_pmic_thm_get_trend,
	.get_hyst = sprd_pmic_thm_get_hyst,
	.suspend = sprd_pmic_thm_hw_suspend,
	.resume = sprd_pmic_thm_hw_resume,
};

static struct sprd_thm_platform_data *thermal_detect_parse_dt(
                         struct device *dev)
{
	struct sprd_thm_platform_data *pdata;
	struct device_node *np = dev->of_node;
	u32 trip_points_critical,trip_num;
	char prop_name[32];
	const char *tmp_str;
	u32 tmp_data,tmp_lowoff;
	int ret,i ,j = 0;
	pdata = kzalloc(sizeof(*pdata), GFP_KERNEL);
	if (!pdata) {
		dev_err(dev, "could not allocate memory for platform data\n");
		return NULL;
	}
	ret = of_property_read_u32(np, "trip-points-critical", &trip_points_critical);
	if(ret){
		dev_err(dev, "fail to get trip_points_critical\n");
		goto fail;
	}
	ret = of_property_read_u32(np, "trip-num", &trip_num);
	if(ret){
		dev_err(dev, "fail to get trip_num\n");
		goto fail;
	}
	for (i = 0; i <trip_num-1; ++i) {
		sprintf(prop_name, "trip%d-temp-active", i);
		if (of_property_read_u32(np, prop_name, &tmp_data)){
			dev_err(dev, "fail to get trip%d-temp-active\n",i);
			goto fail;
		}

		pdata->trip_points[i].temp = tmp_data;
        sprintf(prop_name, "trip%d-temp-lowoff", i);
		if (of_property_read_u32(np, prop_name, &tmp_lowoff)){
			dev_err(dev, "fail to get trip%d-temp-lowoff\n",i);
			goto fail;
		}
		pdata->trip_points[i].lowoff = tmp_lowoff;
		sprintf(prop_name, "trip%d-type", i);
		if (of_property_read_string(np, prop_name, &tmp_str))
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
		if (of_property_read_u32(np, prop_name, &tmp_data))
			goto  fail;
		for (j = 0; j < tmp_data; j++) {
			sprintf(prop_name, "trip%d-cdev-name%d", i, j);
			if (of_property_read_string(np, prop_name, &tmp_str))
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

	return pdata;

fail:
	kfree(pdata);
	return NULL;

}


static int sprd_adi_thm_probe(struct platform_device *pdev)
{
	struct sprd_thermal_zone *pzone = NULL;
	struct sprd_thm_platform_data *ptrips = NULL;
	struct resource *res;
	const char *thm_name;
	int ret,temp_interval,sensor_id;
	unsigned long adie_thm_base;
	struct device_node *np = pdev->dev.of_node;
	if (!np) {
		dev_err(&pdev->dev, "device node not found\n");
		return -EINVAL;
	}
	printk("sprd_adi_thm_probe start\n");
	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	adie_thm_base = (unsigned long)devm_ioremap_resource(&pdev->dev, res);
	if (!adie_thm_base) {
		pr_err("thermal ioremap failed!\n");
		return -ENOMEM;
	}
#if 0
	pdev->id = of_alias_get_id(np, "thmzone");
	printk(KERN_INFO " sprd_thermal_probe id:%d\n", pdev->id);
	if (unlikely(pdev->id < 0 || pdev->id >= THM_SENOR_NUM)) {
		dev_err(&pdev->dev, "does not support id %d\n", pdev->id);
		return -ENXIO;
	}
#endif
	ret = of_property_read_u32(np, "temp-inteval", &temp_interval);
	if(ret){
		dev_err(&pdev->dev, "fail to get temp-inteval\n");
		return -EINVAL;
	}
	ret = of_property_read_u32(np, "id", &sensor_id);
	if (ret) {
		dev_err(&pdev->dev, "fail to get id\n");
		return -EINVAL;
	}
	ptrips = thermal_detect_parse_dt(&pdev->dev);
	if (!ptrips){
	dev_err(&pdev->dev, "not found ptrips\n");
		return -EINVAL;
	}
	pzone = devm_kzalloc(&pdev->dev, sizeof(*pzone), GFP_KERNEL);
	mutex_init(&pzone->th_lock);
	mutex_lock(&pzone->th_lock);
	if (!pzone)
		return -ENOMEM;
	of_property_read_string(np, "thermal-name",&thm_name);
	strcpy(pzone->thermal_zone_name, thm_name);
	printk("the thermal-name1 is %s\n",pzone->thermal_zone_name);
	pzone->reg_base = (unsigned long) ANA_THM_BASE;
	pzone->trip_tab = ptrips;
	pzone->temp_inteval = temp_interval;
	pzone->sensor_id = sensor_id;
	pzone->ops = &sprd_pmic_ops;
	ret = sprd_pmic_thm_hw_init(pzone);
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
	mutex_unlock(&pzone->th_lock);
	printk("sprd_adi_thm_probe end\n");
	return 0;
}

static int sprd_adi_thm_remove(struct platform_device *pdev)
{
	struct sprd_thermal_zone *pzone = platform_get_drvdata(pdev);
	sprd_thermal_remove(pzone);
	return 0;
}

static int sprd_adi_thm_suspend(struct platform_device *pdev,
				pm_message_t state)
{
	struct sprd_thermal_zone *pzone = platform_get_drvdata(pdev);
	//flush_delayed_work(&pzone->thm_logtime_work);
	flush_delayed_work(&pzone->thm_read_work);
	flush_delayed_work(&pzone->resume_delay_work);
	pzone->ops->suspend(pzone);
	return 0;
}

static int sprd_adi_thm_resume(struct platform_device *pdev)
{

	struct sprd_thermal_zone *pzone = platform_get_drvdata(pdev);
	schedule_delayed_work(&pzone->resume_delay_work, (HZ * 1));
	schedule_delayed_work(&pzone->thm_read_work, (HZ * 5));
	//schedule_delayed_work(&pzone->thm_logtime_work, (HZ * 7));
	//pzone->ops->resume(pzone);
	return 0;
}

static const struct of_device_id thermal_of_match[] = {
	{ .compatible = "sprd,sc2723-thermal", },
       {}
};

static struct platform_driver sprd_thermal_driver = {
	.probe = sprd_adi_thm_probe,
	.suspend = sprd_adi_thm_suspend,
	.resume = sprd_adi_thm_resume,
	.remove = sprd_adi_thm_remove,
	.driver = {
		   .owner = THIS_MODULE,
		   .name = "sc2723-thermal",
#ifdef CONFIG_OF
			.of_match_table = of_match_ptr(thermal_of_match),
#endif
		   },
};
static int __init sprd_adi_thermal_init(void)
{
	return platform_driver_register(&sprd_thermal_driver);
}

static void __exit sprd_adi_thermal_exit(void)
{
	platform_driver_unregister(&sprd_thermal_driver);
}

device_initcall_sync(sprd_adi_thermal_init);
module_exit(sprd_adi_thermal_exit);

MODULE_AUTHOR("Freeman Liu <freeman.liu@spreadtrum.com>");
MODULE_DESCRIPTION("sprd thermal driver");
MODULE_LICENSE("GPL");




