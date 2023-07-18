/*
 * Copyright (C) 2020 Spreadtrum Communications Inc.
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */
#include <linux/device.h>
#include <linux/module.h>
#include <linux/mfd/syscon.h>
#include <linux/of_platform.h>
#include <linux/platform_device.h>
#include <linux/regmap.h>
#include <linux/slab.h>
#include <linux/sprd_sip_svc.h>
#include "sprd_debuglog_core.h"
#include "sprd_debuglog_drv.h"

static int (*wakeup_source_match)(u32 m, u32 s, u32 t, void *data, int num);
static struct sprd_sip_svc_pwr_ops *power_ops;

static int disp_bits(struct reg_bit *bit_list, int list_num, u32 reg_val)
{
	u32 val;
	int i;

	if (!bit_list || !list_num) {
		pr_err("   #--Bit list is empty\n");
		return -EINVAL;
	}

	for (i = 0; i < list_num; ++i) {
		val = (reg_val >> bit_list[i].offset) & bit_list[i].mask;
		if (val == bit_list[i].expect)
			continue;
		pr_info("   #--%s: %u", bit_list[i].name, val);
	}

	return 0;
}

static int disp_regs(struct regmap *table_map, char *table_name,
					struct reg_info *reg_list, int list_num)
{
	int i, ret;
	u32 val;

	if (!table_map || !table_name) {
		pr_err("  ##--Register table is error\n");
		return -EINVAL;
	}

	if (!reg_list || !list_num) {
		pr_err("  ##--Register list is empty\n");
		return -EINVAL;
	}

	for (i = 0; i < list_num; ++i) {
		ret = regmap_read(table_map, reg_list[i].offset, &val);
		if (ret) {
			pr_err("  ##--Read %s.%s(0x%08x) error\n",
			      table_name, reg_list[i].name, reg_list[i].offset);
			continue;
		}

		pr_info("  ##--%s.%s: 0x%08x\n",
					     table_name, reg_list[i].name, val);

		ret = disp_bits(reg_list[i].bit, reg_list[i].num, val);
		if (ret)
			pr_err("  ##--Display %s error\n", reg_list[i].name);
	}

	return 0;
}

/**
 * ap_sleep_condition_check - Ap sleep condition check
 */
static int ap_sleep_condition_check(struct device *dev, struct debug_data *dat)
{
	struct reg_table *table;
	struct regmap *map;
	int ret, num, i;

	if (!dev || !dat) {
		pr_err("%s: Parameter is error\n", __func__);
		return -EINVAL;
	}

	table = (struct reg_table *)(dat->data);
	num = dat->num;

	pr_info(" ###--AP DEEP SLEEP CONDITION CHECK\n");

	for (i = 0; i < num; ++i) {
		map = syscon_regmap_lookup_by_phandle(dev->of_node, table[i].dts);
		if (IS_ERR(map)) {
			pr_info("  ##--Get %s regmap error\n", table[i].dts);
			continue;
		}
		ret = disp_regs(map, table[i].name, table[i].reg, table[i].num);
		if (ret)
			pr_info("  ##--Display %s error\n", table[i].name);
	}

	pr_info(" ###--END\n");

	return 0;
}

/**
 * ap_wakeup_source_get - Get ap wakeup source from intc
 */
static int ap_wakeup_source_get(struct device *dev, struct debug_data *dat)
{
	u32 major, second, third;
	int ret;

	if (!dev || !dat) {
		pr_err("%s: Parameter is error\n", __func__);
		return -EINVAL;
	}

	if (!wakeup_source_match) {
		dev_err(dev, "The wakeup source parse is error\n");
		return -EINVAL;
	}

	ret = power_ops->get_wakeup_source(&major, &second, &third);
	if (ret) {
		dev_err(dev, "Get wakeup source error\n");
		return -EINVAL;
	}

	/**
	 * The interface is called in the process of entering the suspend, and
	 * if the entry into the suspend fails, the wake-up source
	 * cannot be obtained.
	 */
	if (!major) {
		dev_warn(dev, "The system has not yet entered sleep mode\n");
		return 0;
	}

	ret = wakeup_source_match(major, second, third, dat->data, dat->num);
	if (ret) {
		dev_err(dev, "Match wakeup source error\n");
		return -EINVAL;
	}

	return 0;
}

/**
 * ap_wakeup_source_print - Print wakeup source informaiton
 */
static int ap_wakeup_source_print(struct device *dev, struct debug_data *dat)
{
	char *wakeup_name, *wakeup_device;
	char **str_list;

	if (!dev || !dat) {
		pr_err("%s: Parameter is error\n", __func__);
		return -EINVAL;
	}

	str_list = (char **)(dat->data);

	wakeup_name = str_list[0];
	wakeup_device = str_list[1];

	if (!wakeup_name) {
		dev_warn(dev, "The wakeup source is empty\n");
		return 0;
	}

	pr_info(" ###--WAKEUP SOURCE\n");
	pr_info("  ##--%s\n", wakeup_name);
	if (wakeup_device)
		pr_info("   #--%s\n", wakeup_device);
	pr_info(" ###--END\n");

	str_list[0] = NULL;
	str_list[1] = NULL;

	return 0;
}

/**
 * subsys_state_monitor - display power state
 */
static int subsys_state_monitor(struct device *dev, struct debug_data *dat)
{
	struct reg_table *table;
	struct regmap *map;
	int ret, num, i;

	if (!dev || !dat) {
		pr_err("%s: Parameter is error\n", __func__);
		return -EINVAL;
	}

	table = (struct reg_table *)(dat->data);
	num = dat->num;

	pr_info(" ###--POWER DOMAIN STATE\n");

	for (i = 0; i < num; ++i) {
		map = syscon_regmap_lookup_by_phandle(dev->of_node, table[i].dts);
		if (IS_ERR(map)) {
			pr_info("  ##--Get %s regmap error\n", table[i].dts);
			continue;
		}
		ret = disp_regs(map, table[i].name, table[i].reg, table[i].num);
		if (ret)
			pr_info("  ##--Display %s error\n", table[i].name);
	}

	pr_info(" ###--END\n");

	return 0;
}

/**
 * debug_driver_probe - add the debug log driver
 */
static int debug_driver_probe(struct platform_device *pdev)
{
	struct sprd_sip_svc_handle *sip;
	const struct plat_data *pdata;
	struct debug_log *log;
	struct device *dev;
	int ret;

	pr_info("%s: Init debug log driver\n", __func__);

	dev = &pdev->dev;
	if (!dev) {
		pr_err("%s: Get debug device error\n", __func__);
		return -EINVAL;
	}

	sip = sprd_sip_svc_get_handle();
	if (!sip) {
		dev_err(dev, "Get wakeup sip error.\n");
		return -EINVAL;
	}

	power_ops = &sip->pwr_ops;

	log = devm_kzalloc(dev, sizeof(struct debug_log), GFP_KERNEL);
	if (!log) {
		dev_err(dev, "%s: Debug memory alloc error\n", __func__);
		return -ENOMEM;
	}

	log->dev = dev;

	pdata = of_device_get_match_data(dev);
	if (!pdata) {
		dev_err(dev, "No matched private driver data found\n");
		return -EINVAL;
	}

	if (pdata->sleep_condition_table && pdata->sleep_condition_table_num) {
		log->sleep_condition_check =
		      devm_kzalloc(dev, sizeof(struct debug_event), GFP_KERNEL);
		if (!log->sleep_condition_check) {
			dev_err(dev, "Check event memory alloc error\n");
			return -ENOMEM;
		}

		log->sleep_condition_check->data.data =
					pdata->sleep_condition_table;
		log->sleep_condition_check->data.num =
					pdata->sleep_condition_table_num;
		log->sleep_condition_check->handle =
					ap_sleep_condition_check;
	}

	if (pdata->wakeup_source_info && pdata->wakeup_source_info_num) {
		log->wakeup_soruce_get =
		      devm_kzalloc(dev, sizeof(struct debug_event), GFP_KERNEL);
		if (!log->wakeup_soruce_get) {
			dev_err(dev, "Get event memory alloc error\n");
			return -ENOMEM;
		}

		log->wakeup_soruce_get->data.data =
					pdata->wakeup_source_info;
		log->wakeup_soruce_get->data.num =
					pdata->wakeup_source_info_num;
		log->wakeup_soruce_get->handle =
					ap_wakeup_source_get;
	}

	if (pdata->wakeup_source_info && pdata->wakeup_source_info_num) {
		log->wakeup_soruce_print =
		      devm_kzalloc(dev, sizeof(struct debug_event), GFP_KERNEL);
		if (!log->wakeup_soruce_print) {
			dev_err(dev, "Print event memory alloc error\n");
			return -ENOMEM;
		}

		log->wakeup_soruce_print->data.data =
					pdata->wakeup_source_info;
		log->wakeup_soruce_print->data.num =
					pdata->wakeup_source_info_num;
		log->wakeup_soruce_print->handle =
					ap_wakeup_source_print;
	}

	if (pdata->subsys_state_table && pdata->subsys_state_table_num) {
		log->subsys_state_monitor =
		      devm_kzalloc(dev, sizeof(struct debug_event), GFP_KERNEL);
		if (!log->subsys_state_monitor) {
			dev_err(dev, "Monitor event memory alloc error\n");
			return -ENOMEM;
		}

		log->subsys_state_monitor->data.data =
					pdata->subsys_state_table;
		log->subsys_state_monitor->data.num =
					pdata->subsys_state_table_num;
		log->subsys_state_monitor->handle =
					subsys_state_monitor;
	}

	wakeup_source_match = pdata->wakeup_source_match;

	ret = sprd_debug_log_register(log);
	if (ret) {
		dev_err(dev, "Register debug log error\n");
		return -EINVAL;
	}

	return 0;
}

/**
 * debug_driver_remove - remove the debug log driver
 */
static int debug_driver_remove(struct platform_device *pdev)
{
	return sprd_debug_log_unregister();
}

/* Platform debug data */
const struct debug_data __weak sprd_sharkl6pro_debug_data;
const struct debug_data __weak sprd_sharkl6_debug_data;
const struct debug_data __weak sprd_sharkl5pro_debug_data;
const struct debug_data __weak sprd_sharkl5_debug_data;
const struct debug_data __weak sprd_sharkl3_debug_data;
const struct debug_data __weak sprd_sharkle_debug_data;

static const struct of_device_id debuglog_of_match[] = {
	{
		.compatible = "sprd,debuglog-sharkl6pro",
		.data = &sprd_sharkl6pro_debug_data,
	},
	{
		.compatible = "sprd,debuglog-sharkl6",
		.data = &sprd_sharkl6_debug_data,
	},
	{
		.compatible = "sprd,debuglog-sharkl5pro",
		.data = &sprd_sharkl5pro_debug_data,
	},
	{
		.compatible = "sprd,debuglog-sharkl5",
		.data = &sprd_sharkl5_debug_data,
	},
	{
		.compatible = "sprd,debuglog-sharkl3",
		.data = &sprd_sharkl3_debug_data,
	},
	{
		.compatible = "sprd,debuglog-sharkle",
		.data = &sprd_sharkle_debug_data,
	},
	{
	},
};
MODULE_DEVICE_TABLE(of, debuglog_of_match);

static struct platform_driver sprd_debuglog_driver = {
	.driver = {
		.name = "sprd-debuglog",
		.of_match_table = debuglog_of_match,
	},
	.probe = debug_driver_probe,
	.remove = debug_driver_remove,
};

module_platform_driver(sprd_debuglog_driver);

MODULE_LICENSE("GPL v2");
MODULE_DESCRIPTION("sprd debug log driver");
