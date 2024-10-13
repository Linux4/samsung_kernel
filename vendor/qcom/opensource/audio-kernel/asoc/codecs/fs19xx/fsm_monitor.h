/* SPDX-License-Identifier: GPL-2.0+ */
/**
 * Copyright (C) Shanghai FourSemi Semiconductor Co.,Ltd 2016-2023. All rights reserved.
 * 2023-08-10 File created.
 */

#ifndef __FSM_MONITOR_H__
#define __FSM_MONITOR_H__

#include "fsm_public.h"
#include <linux/mutex.h>
#include <linux/workqueue.h>
#include <linux/power_supply.h>
#include <linux/of.h>

struct fsm_mntr_cfg_tbl {
	int threshold;
	int volume;
};

struct fsm_mntr {
	struct device *dev;
	struct workqueue_struct *thread_wq;
	struct delayed_work delay_work;
	int mntr_mode;
	int mntr_scene;
	int mntr_period;
	int mntr_avg_count;
	int bsg_cfg_count;
	int *bsg_tbl;
	int csg_cfg_count;
	int *csg_tbl;
	int bat_vol;
	int bat_cap;
	int bat_temp;
	int mix_volume;
	int cur_volume;
	bool monitor_on;
};

static struct fsm_mntr *g_fsm_mntr;

static int fsm_monitor_parse_bsg_dts(struct fsm_mntr *mntr)
{
	struct property *prop;
	const __be32 *data;
	int size;
	int i;

	if (mntr == NULL)
		return -EINVAL;

	if (!(mntr->mntr_mode & 0x3))
		return 0;

	prop = of_find_property(mntr->dev->of_node, "fsm,mntr-bsg-cfg", &size);
	if (prop == NULL || !prop->value)
		return -ENODATA;

	if (size & 0x7) { // size = 2*4N bytes
		dev_err(mntr->dev, "Invalid size: %d\n", size);
		return -EINVAL;
	}

	mntr->bsg_tbl = devm_kzalloc(mntr->dev, size, GFP_KERNEL);
	if (mntr->bsg_tbl == NULL)
		return -ENOMEM;

	mntr->bsg_cfg_count = size / sizeof(struct fsm_mntr_cfg_tbl);
	data = prop->value;
	for (i = 0; i < size / sizeof(int); i++)
		mntr->bsg_tbl[i] = be32_to_cpup(data++);

	dev_info(mntr->dev, "bsg count: %d\n", mntr->bsg_cfg_count);

	return 0;
}

static int fsm_monitor_parse_csg_dts(struct fsm_mntr *mntr)
{
	struct property *prop;
	const __be32 *data;
	int size;
	int i;

	if (mntr == NULL)
		return -EINVAL;

	if (!(mntr->mntr_mode & 0x4))
		return 0;

	prop = of_find_property(mntr->dev->of_node, "fsm,mntr-csg-cfg", &size);
	if (prop == NULL || !prop->value)
		return -ENODATA;

	if (size & 0x7) { // size = 2*4N bytes
		dev_err(mntr->dev, "Invalid size: %d\n", size);
		return -EINVAL;
	}

	mntr->csg_tbl = devm_kzalloc(mntr->dev, size, GFP_KERNEL);
	if (mntr->bsg_tbl == NULL)
		return -ENOMEM;

	mntr->csg_cfg_count = size / sizeof(struct fsm_mntr_cfg_tbl);
	data = prop->value;
	for (i = 0; i < size / sizeof(int); i++)
		mntr->csg_tbl[i] = be32_to_cpup(data++);

	dev_info(mntr->dev, "csg count: %d\n", mntr->csg_cfg_count);

	return 0;
}

static int fsm_monitor_parse_dts(struct fsm_mntr *mntr)
{
	struct device_node *np;
	int ret;

	np = mntr->dev->of_node;
	ret = of_property_read_bool(np, "fsm,mntr-mode");
	if (!ret) {
		dev_info(mntr->dev, "Not found property: fsm,mntr-mode\n");
		return -EINVAL;
	}

	ret = of_property_read_u32(np, "fsm,mntr-mode", &mntr->mntr_mode);
	if (ret) {
		dev_err(mntr->dev, "Failed to read fsm,mntr-mode\n");
		return ret;
	}

	ret = of_property_read_u32(np, "fsm,mntr-scene", &mntr->mntr_scene);
	if (ret)
		mntr->mntr_scene = 0x0001; // Music as default

	ret = of_property_read_u32(np, "fsm,mntr-period", &mntr->mntr_period);
	if (ret)
		mntr->mntr_period = 1000; // 1000ms

	ret = of_property_read_u32(np,
			"fsm,mntr-avg-count", &mntr->mntr_avg_count);
	if (ret)
		mntr->mntr_avg_count = 5; // 5 times

	ret = fsm_monitor_parse_bsg_dts(mntr);
	if (ret) {
		dev_err(mntr->dev, "Failed to parse bsg dts: %d\n", ret);
		return ret;
	}

	ret = fsm_monitor_parse_csg_dts(mntr);
	if (ret) {
		dev_err(mntr->dev, "Failed to parse csg dts: %d\n", ret);
		return ret;
	}

	return 0;
}

static int fsm_get_ambient_info(struct fsm_mntr *mntr)
{
	union power_supply_propval prop;
	struct power_supply *pspy;
	int ret;

	if (mntr == NULL || mntr->dev == NULL)
		return -EINVAL;

	pspy = power_supply_get_by_name("battery");
	if (pspy == NULL) {
		dev_err(mntr->dev, "Failed to get power supply!\n");
		return -EINVAL;
	}

	ret = power_supply_get_property(pspy,
			POWER_SUPPLY_PROP_VOLTAGE_NOW, &prop);
	mntr->bat_vol = DIV_ROUND_CLOSEST(prop.intval, 1000);

	ret |= power_supply_get_property(pspy,
			POWER_SUPPLY_PROP_CAPACITY, &prop);
	mntr->bat_cap = prop.intval;

	ret |= power_supply_get_property(pspy,
			POWER_SUPPLY_PROP_TEMP, &prop);
	mntr->bat_temp = DIV_ROUND_CLOSEST(prop.intval, 10);

	power_supply_put(pspy);
	dev_dbg(mntr->dev, "bat vol:%d cap:%d temp:%d\n",
			mntr->bat_vol,
			mntr->bat_cap,
			mntr->bat_temp);

	return ret;
}

static int fsm_monitor_get_volume(struct fsm_mntr *mntr, int *volume)
{
	struct fsm_mntr_cfg_tbl *tbl;
	int new_volume;
	int index;
	int i;

	if (mntr == NULL || volume == NULL)
		return -EINVAL;

	new_volume = mntr->mix_volume;
	if (mntr->mntr_mode & 0x3) { // BSG_VOL|BSG_CAP
		tbl = (struct fsm_mntr_cfg_tbl *)mntr->bsg_tbl;
		for (index = -1, i = 0; i < mntr->bsg_cfg_count; i++) {
			if (mntr->bat_vol > tbl[i].threshold)
				continue;
			if (index < 0 || tbl[i].threshold < tbl[index].threshold)
				index = i;
		}
		if (index >= 0 && new_volume > tbl[index].volume)
			new_volume = tbl[index].volume;
		dev_dbg(mntr->dev, "get bsg volume: %d\n", new_volume);
	}

	if (mntr->mntr_mode & 0x4) { // CSG
		tbl = (struct fsm_mntr_cfg_tbl *)mntr->csg_tbl;
		for (index = -1, i = 0; i < mntr->csg_cfg_count; i++) {
			if (mntr->bat_temp > tbl[i].threshold)
				continue;
			if (index < 0 || tbl[i].threshold < tbl[index].threshold)
				index = i;
		}
		if (index >= 0 && new_volume > tbl[index].volume)
			new_volume = tbl[index].volume;
		dev_dbg(mntr->dev, "get csg volume: %d\n", new_volume);
	}

	*volume = new_volume;
	dev_dbg(mntr->dev, "get volume: %d\n", *volume);

	return 0;
}

static int fsm_monitor_update_volume(struct fsm_mntr *mntr)
{
	static int vol, temp, count;
	int volume;
	int ret;

	if (mntr == NULL)
		return -EINVAL;

	if (!mntr->mntr_mode)
		return 0;

	if (mntr->mntr_mode & 0x1) // BSG_VOL
		vol += mntr->bat_vol;
	else if (mntr->mntr_mode & 0x2) // BSG_CAP
		vol += mntr->bat_cap;
	temp += mntr->bat_temp; // CSG
	count++;

	if (mntr->monitor_on && count < mntr->mntr_avg_count)
		return 0;

	mntr->bat_vol = vol / count;
	mntr->bat_temp = temp / count;

	ret = fsm_monitor_get_volume(mntr, &volume);
	if (!ret && mntr->cur_volume != volume) {
		fsm_set_volume(volume);
		mntr->cur_volume = volume;
		dev_info(mntr->dev, "bat avg vol:%d temp:%d volume: %d\n",
				mntr->bat_vol, mntr->bat_temp, volume);
	}

	vol = 0;
	temp = 0;
	count = 0;

	return 0;
}

static void fsm_vbat_monitor_work(struct work_struct *work)
{
	struct fsm_mntr *mntr;
	int ret;

	mntr = container_of(work, struct fsm_mntr, delay_work.work);

	ret = fsm_get_ambient_info(mntr);
	if (ret) {
		dev_err(mntr->dev, "Failed to get ambient info: %d\n", ret);
		return;
	}

	ret = fsm_monitor_update_volume(mntr);
	if (ret)
		dev_err(mntr->dev, "Failed to update volume: %d\n", ret);

	queue_delayed_work(mntr->thread_wq,
			&mntr->delay_work,
			msecs_to_jiffies(mntr->mntr_period));
}

static int fsm_monitor_switch(uint16_t scene, bool on)
{
	struct fsm_mntr *mntr = g_fsm_mntr;
	int ret;

	if (mntr == NULL)
		return -EINVAL;

	dev_dbg(mntr->dev, "mode: %d scene: %x-%x state: %d\n",
			mntr->mntr_mode,
			mntr->mntr_scene, scene,
			mntr->monitor_on);

	if (!mntr->mntr_mode)
		return 0;

	if (on && !(mntr->mntr_scene & scene))
		return 0;

	dev_info(mntr->dev, "bat monitor switch %s\n", on ? "ON" : "OFF");

	if (on) {
		if (mntr->monitor_on)
			return 0;
		mntr->mix_volume = fsm_get_config()->volume;
		mntr->cur_volume = mntr->mix_volume;
		ret  = fsm_get_ambient_info(mntr);
		ret |= fsm_monitor_update_volume(mntr);
		if (ret)
			return ret;
		mntr->monitor_on = true;
		queue_delayed_work(mntr->thread_wq,
				&mntr->delay_work,
				msecs_to_jiffies(mntr->mntr_period));
	} else {
		if (!mntr->monitor_on)
			return 0;
		mntr->monitor_on = false;
		cancel_delayed_work_sync(&mntr->delay_work);
		fsm_set_volume(mntr->mix_volume);
	}

	return 0;
}

static int fsm_monitor_init(struct device *dev)
{
	struct fsm_mntr *mntr;
	int ret;

	if (dev == NULL)
		return -EINVAL;

	mntr = devm_kzalloc(dev, sizeof(struct fsm_mntr), GFP_KERNEL);
	if (mntr == NULL)
		return -ENOMEM;

	mntr->dev = dev;
	ret = fsm_monitor_parse_dts(mntr);
	if (ret) {
		dev_info(dev, "Not use vbat monitor?\n");
		ret = 0;
		goto err_exit;
	}

	mntr->thread_wq = create_singlethread_workqueue("fsm-monitor");
	INIT_DELAYED_WORK(&mntr->delay_work, fsm_vbat_monitor_work);

	g_fsm_mntr = mntr;

	return 0;

err_exit:
	if (mntr->bsg_tbl)
		devm_kfree(dev, mntr->bsg_tbl);
	if (mntr->csg_tbl)
		devm_kfree(dev, mntr->csg_tbl);
	devm_kfree(dev, mntr);
	g_fsm_mntr = NULL;

	return ret;
}

static void fsm_monitor_deinit(struct device *dev)
{
	struct fsm_mntr *mntr = g_fsm_mntr;

	if (mntr == NULL)
		return;

	if (mntr->bsg_tbl)
		devm_kfree(dev, mntr->bsg_tbl);
	if (mntr->csg_tbl)
		devm_kfree(dev, mntr->csg_tbl);
	devm_kfree(dev, mntr);
	g_fsm_mntr = NULL;
}

#endif // __FSM_MONITOR_H__
