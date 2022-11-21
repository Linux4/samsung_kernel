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
#include <soc/sprd/adc.h>
#include <linux/module.h>
#include <linux/of_device.h>
#include <linux/platform_device.h>
#include <linux/thermal.h>
#include <linux/slab.h>

//#define SPRD_THM_BOARD_DEBUG
#ifdef SPRD_THM_BOARD_DEBUG
#define THM_BOARD_DEBUG(format, arg...) printk("sprd board thm: " "@@@" format, ## arg)
#else
#define THM_BOARD_DEBUG(format, arg...)
#endif

#define THM_SENOR_NUM 8
extern int sprd_thermal_init(struct sprd_thermal_zone *pzone);
extern void sprd_thermal_remove(struct sprd_thermal_zone *pzone);


struct sprd_board_sensor_config *pthm_config;
//extern uint16_t sprdchg_bat_adc_to_vol(uint16_t adcvalue);

static int sprdthm_read_temp_adc(void)
{
#define SAMPLE_NUM  15
	int cnt = pthm_config->temp_adc_sample_cnt;

	if (cnt > SAMPLE_NUM) {
		cnt = SAMPLE_NUM;
	} else if (cnt < 1) {
		cnt = 1;
	}

	if (pthm_config->temp_support) {
		int ret, i, j, temp;
		int adc_val[cnt];
		struct adc_sample_data data = {
			.channel_id = pthm_config->temp_adc_ch,
			.channel_type = 0,	/*sw */
			.hw_channel_delay = 0,	/*reserved */
			.scale = pthm_config->temp_adc_scale,	/*small scale */
			.pbuf = &adc_val[0],
			.sample_num = cnt,
			.sample_bits = 1,
			.sample_speed = 0,	/*quick mode */
			.signal_mode = 0,	/*resistance path */
		};

		ret = sci_adc_get_values(&data);
		WARN_ON(0 != ret);

		for (j = 1; j <= cnt - 1; j++) {
			for (i = 0; i < cnt - j; i++) {
				if (adc_val[i] > adc_val[i + 1]) {
					temp = adc_val[i];
					adc_val[i] = adc_val[i + 1];
					adc_val[i + 1] = temp;
				}
			}
		}
		THM_BOARD_DEBUG("sprdthm: channel:%d,sprdthm_read_temp_adc:%d\n",
		       data.channel_id, adc_val[cnt / 2]);
		return adc_val[cnt / 2];
	} else {
		return 3000;
	}
}

static int sprdthm_interpolate(int x, int n, struct sprdboard_table_data *tab)
{
	int index;
	int y;

	if (x >= tab[0].x)
		y = tab[0].y;
	else if (x <= tab[n - 1].x)
		y = tab[n - 1].y;
	else {
		/*  find interval */
		for (index = 1; index < n; index++)
			if (x > tab[index].x)
				break;
		/*  interpolate */
		y = (tab[index - 1].y - tab[index].y) * (x - tab[index].x)
		    * 2 / (tab[index - 1].x - tab[index].x);
		y = (y + 1) / 2;
		y += tab[index].y;
	}
	return y;
}
static uint16_t sprdthm_adc_to_vol(uint16_t channel, int scale,
				   uint16_t adcvalue)
{
	uint32_t result;
	uint32_t vthm_vol = adc2vbat(adcvalue,scale);
	uint32_t m, n;
	uint32_t thm_numerators, thm_denominators;
	uint32_t numerators, denominators;

	sci_adc_get_vol_ratio(ADC_CHANNEL_VBAT, 0, &thm_numerators,
			      &thm_denominators);
	sci_adc_get_vol_ratio(channel, scale, &numerators, &denominators);

	///v1 = vbat_vol*0.268 = vol_bat_m * r2 /(r1+r2)
	n = thm_denominators * numerators;
	m = vthm_vol * thm_numerators * (denominators);
	result = (m + n / 2) / n;
	return result;
}

static int sprdthm_search_temp_tab(int val)
{
	return sprdthm_interpolate(val, pthm_config->temp_tab_size,
				   pthm_config->temp_tab);
}

static int sprd_board_thm_get_reg_base(struct sprd_thermal_zone *pzone ,struct resource *regs)
{
	return 0;
}
static int sprd_board_thm_set_active_trip(struct sprd_thermal_zone *pzone, int trip )
{
	return 0;
}

static unsigned long  sprd_board_thm_temp_read(struct sprd_thermal_zone *pzone)
{
    int temp;
	if (pthm_config->temp_support) {
		int val = sprdthm_read_temp_adc();
		//voltage mode
		if (pthm_config->temp_table_mode) {
			val =sprdthm_adc_to_vol(pthm_config->temp_adc_ch,
					       pthm_config->temp_adc_scale, val);
			THM_BOARD_DEBUG("sprdthm: sprdthm_read_temp voltage:%d\n", val);
		}
        THM_BOARD_DEBUG("sprd_board_thm_temp_read y=%d\n",sprdthm_search_temp_tab(val));
		temp = sprdthm_search_temp_tab(val);
		printk("sensor id:%d,rawdata:0x%x, temp:%lu\n", pzone->sensor_id, val, temp*1000);
		return temp*1000;
	} else {
		return -35000;
	}
	
}

static int sprd_board_thm_hw_init(struct sprd_thermal_zone *pzone)
{
    THM_BOARD_DEBUG("sprd_board_thm_hw_init\n");
	pthm_config=pzone->sensor_config;

	return 0;
}


static int sprd_board_thm_hw_suspend(struct sprd_thermal_zone *pzone)
{
     THM_BOARD_DEBUG("sprd_board_thm_hw_suspend\n");
	
	return 0;
}
static int sprd_board_thm_hw_resume(struct sprd_thermal_zone *pzone)
{

    THM_BOARD_DEBUG("sprd_board_thm_hw_resume\n");

	return 0;
}
#ifdef  THM_TEST
static int sprd_board_thm_trip_set(struct sprd_thermal_zone *pzone,int trip)
{
	THM_BOARD_DEBUG("sprd_thm_trip_set trip=%d, temp=%ld,lowoff =%ld\n",
		trip,pzone->trip_tab->trip_points[trip].temp,pzone->trip_tab->trip_points[trip].lowoff);
	return 0;
}
#endif

static int sprd_board_thm_get_trend(struct sprd_thermal_zone *pzone, int trip, enum thermal_trend *ptrend)
{
	*ptrend = pzone->trend_val;
	return 0;
}

static int sprd_board_thm_get_hyst(struct sprd_thermal_zone *pzone, int trip, unsigned long *physt)
{
	struct sprd_thm_platform_data *trip_tab = pzone->trip_tab;
	if (trip >= trip_tab->num_trips - 2){
	*physt = 0;
	}else{
	*physt = trip_tab->trip_points[trip].temp - trip_tab->trip_points[trip + 1].lowoff;
	}
	return 0;
}

struct thm_handle_ops sprd_boardthm_ops =
{
	.hw_init = sprd_board_thm_hw_init,
	.get_reg_base = sprd_board_thm_get_reg_base,
	.read_temp = sprd_board_thm_temp_read,
	.get_trend = sprd_board_thm_get_trend,
	.get_hyst = sprd_board_thm_get_hyst,
	.suspend = sprd_board_thm_hw_suspend,
	.resume = sprd_board_thm_hw_resume,
	.trip_debug_set = sprd_board_thm_trip_set,
};

#ifdef CONFIG_OF
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

static struct sprd_board_sensor_config *sprdboard_thermal_parse_dt(
							   struct device *dev)
{
    struct sprd_board_sensor_config *pconfig;
	struct device_node *np = dev->of_node;
	int ret,i;
	int temp;
	int temp_adc_ch,temp_adc_scale,temp_adc_sample_cnt;
	int temp_table_mode,temp_tab_size,temp_support;
	pconfig = kzalloc(sizeof(*pconfig), GFP_KERNEL);
	if (!pconfig) {
		dev_err(dev, "could not allocate memory for platform data\n");
		return NULL;
	}
	ret =of_property_read_u32(np, "temp-adc-ch",&temp_adc_ch);
	if(ret){
		dev_err(dev, "sprd_thermal_probe No temp-adc-ch\n");
		goto fail;
	}
	ret =of_property_read_u32(np, "temp-adc-scale",&temp_adc_scale);
	if(ret){
		dev_err(dev, "sprd_thermal_probe No temp_adc_scale\n");
		goto fail;
	}
	ret =of_property_read_u32(np, "temp-adc-sample-cnt",&temp_adc_sample_cnt);
	if(ret){
		dev_err(dev, "fail to get temp_adc_sample_cnt\n");
		goto fail;
	}
	ret =of_property_read_u32(np, "temp-table-mode",&temp_table_mode);
	if(ret){
		dev_err(dev, "fail to get temp_table_mode\n");
		goto fail;
	}
	ret =of_property_read_u32(np, "temp-tab-size",&temp_tab_size);
	if(ret){
		dev_err(dev, "fail to get  temp_tab_size\n");
		goto fail;
	}
	ret =of_property_read_u32(np, "temp-support",&temp_support);
	if(ret){
		dev_err(dev, "fail to get temp_support\n");
		goto fail;
	}
	pconfig->temp_adc_ch=temp_adc_ch;
	pconfig->temp_adc_sample_cnt=temp_adc_sample_cnt;
	pconfig->temp_adc_scale=temp_adc_scale;
	pconfig->temp_table_mode=temp_table_mode;
	pconfig->temp_support=temp_support;
	pconfig->temp_tab_size=temp_tab_size;
	pconfig->temp_tab = kzalloc(sizeof(struct sprdboard_table_data) *
				  pconfig->temp_tab_size-1, GFP_KERNEL);
	for (i = 0; i < pconfig->temp_tab_size-1; i++) {
		ret = of_property_read_u32_index(np, "temp-tab-val", i,
						 &pconfig->temp_tab[i].x);
		if(ret){
			dev_err(dev, "fail to get temp-tab-va\n");
			goto fail;
		}
		ret = of_property_read_u32_index(np, "temp-tab-temp", i, &temp);
		if(ret){
			dev_err(dev, "fail to get temp-tab-temp\n");
			goto fail;
		}
		pconfig->temp_tab[i].y = temp - 1000;
	}
	return pconfig;
fail:
	kfree(pconfig);
	return NULL;
}

#endif
static int sprd_board_thm_probe(struct platform_device *pdev)
{
	struct sprd_thermal_zone *pzone = NULL;
	struct sprd_thm_platform_data *ptrips = NULL;
	struct sprd_board_sensor_config *pconfig = NULL;
	const char *thm_name;
	int ret,temp_interval,sensor_id;
	printk("sprd_thermal_probe---------start\n");
#ifdef CONFIG_OF
	struct device_node *np = pdev->dev.of_node;
#endif

#ifdef CONFIG_OF
	if (!np) {
		dev_err(&pdev->dev, "device node not found\n");
		return -EINVAL;
	}
	pconfig = sprdboard_thermal_parse_dt(&pdev->dev);
	if (!pconfig){
		dev_err(&pdev->dev, "not found ptrips\n");
		return -EINVAL;
	}
	ptrips = thermal_detect_parse_dt(&pdev->dev);
#else
	ptrips = dev_get_platdata(&pdev->dev);
#endif
    if (!ptrips){
		dev_err(&pdev->dev, "not found ptrips\n");
		return -EINVAL;
	}
	pzone = devm_kzalloc(&pdev->dev, sizeof(*pzone), GFP_KERNEL);
	if (!pzone){
		kfree(pconfig);
		kfree(ptrips);
		return -ENOMEM;
		}
	mutex_init(&pzone->th_lock);
	mutex_lock(&pzone->th_lock);
	printk("sprd_ddie_thm_probe start\n");
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
	of_property_read_string(np, "thermal-name",&thm_name);
	strcpy(pzone->thermal_zone_name, thm_name);
	pzone->trip_tab = ptrips;
	pzone->mode = THERMAL_DEVICE_DISABLED;
	pzone->trip_tab = ptrips;
	pzone->sensor_config= pconfig;
	pzone->trend_val = THERMAL_TREND_STABLE;
	pzone->temp_inteval = temp_interval;
	pzone->sensor_id = sensor_id;
	pzone->ops = &sprd_boardthm_ops;
	ret = sprd_board_thm_hw_init(pzone);
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
	printk("sprd_ddie_thm_probe end\n");
	mutex_unlock(&pzone->th_lock);
	return 0;
}

static int sprd_board_thm_remove(struct platform_device *pdev)
{

	struct sprd_thermal_zone *pzone = platform_get_drvdata(pdev);
	sprd_thermal_remove(pzone);
	return 0;
}
static int sprd_board_thm_suspend(struct platform_device *pdev,
				pm_message_t state)
{
	struct sprd_thermal_zone *pzone = platform_get_drvdata(pdev);
	//flush_delayed_work(&pzone->thm_logtime_work);
	flush_delayed_work(&pzone->thm_read_work);
	flush_delayed_work(&pzone->resume_delay_work);
	pzone->ops->suspend(pzone);
	return 0;
}
static int sprd_board_thm_resume(struct platform_device *pdev)
{

	struct sprd_thermal_zone *pzone = platform_get_drvdata(pdev);
	schedule_delayed_work(&pzone->resume_delay_work, (HZ * 1));
	schedule_delayed_work(&pzone->thm_read_work, (HZ * 5));
	//schedule_delayed_work(&pzone->thm_logtime_work, (HZ * 7));
	//pzone->ops->resume(pzone);
	return 0;
}

#ifdef CONFIG_OF
static const struct of_device_id thermal_of_match[] = {
	{ .compatible = "sprd,board-thermal", },
       {}
};
#endif

static struct platform_driver sprd_thermal_driver = {
	.probe = sprd_board_thm_probe,
	.suspend = sprd_board_thm_suspend,
	.resume = sprd_board_thm_resume,
	.remove = sprd_board_thm_remove,
	.driver = {
		   .owner = THIS_MODULE,
		   .name = "board-thermal",
#ifdef CONFIG_OF
			.of_match_table = of_match_ptr(thermal_of_match),
#endif
		   },
};
static int __init sprd_board_thermal_init(void)
{
	return platform_driver_register(&sprd_thermal_driver);
}

static void __exit sprd_board_thermal_exit(void)
{
	platform_driver_unregister(&sprd_thermal_driver);
}

device_initcall_sync(sprd_board_thermal_init);
module_exit(sprd_board_thermal_exit);
MODULE_AUTHOR("Freeman Liu <freeman.liu@spreadtrum.com>");
MODULE_DESCRIPTION("sprd thermal driver");
MODULE_LICENSE("GPL");

