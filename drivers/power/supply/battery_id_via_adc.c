/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (C) 2021.
 */

/* HS03 code for SR-SL6215-01-181 by gaochao at 20210713 start */
#include <linux/device.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/of.h>
#include <linux/iio/consumer.h>
#include <linux/iio/iio.h>
#include <linux/platform_device.h>	/* platform device */
#include "battery_id_via_adc.h"

static struct platform_device *g_md_adc_pdev = NULL;
static int g_adc_num = 0;
static int g_adc_val = 0;

static int bat_id_get_adc_info(struct device *dev)
{
	int ret = 0;
	int val = 0;
	struct iio_channel *md_channel = NULL;

	md_channel = iio_channel_get(dev, "customer_bat_id"); // devm_iio_channel_get
	if (IS_ERR(md_channel)) {
		if (PTR_ERR(md_channel) == -EPROBE_DEFER) {
			pr_err("[%s]EPROBE_DEFER\n", __FUNCTION__);
			return PTR_ERR(md_channel);
		}
		pr_err("fail to get iio channel (%d)\n", ret);
		goto Fail;
	}

	g_adc_num = md_channel->channel->channel;
	ret = iio_read_channel_raw(md_channel, &val); // iio_read_channel_processed
	if (ret < 0) {
		pr_err("iio_read_channel_raw fail\n");
		goto Fail;
	}
	iio_channel_release(md_channel);

	g_adc_val = val;
	pr_info("md_ch=%d, val=%d\n", g_adc_num, g_adc_val);

	return ret;
Fail:
	return -1;
}

int bat_id_get_adc_num(void)
{
	return g_adc_num;
}
EXPORT_SYMBOL(bat_id_get_adc_num);

int bat_id_get_adc_val(void)
{
	return g_adc_val;
}
EXPORT_SYMBOL(bat_id_get_adc_val);

int battery_get_bat_id_voltage(void)
{
	struct iio_channel *channel = NULL;
	int ret = 0;
	int val = 0;
	int number = 0;

	if (!g_md_adc_pdev) {
        pr_err("fail to get g_md_adc_pdev\n");
		goto BAT_Fail;
    }

	channel = devm_iio_channel_get(&g_md_adc_pdev->dev, "customer_bat_id"); // iio_channel_get
	if (IS_ERR(channel)) {
		pr_err("fail to get iio channel 3 (%d)\n", ret);
		goto BAT_Fail;
	}

	number = channel->channel->channel;
	g_adc_num = number;
	ret = iio_read_channel_processed(channel, &val);
	if (ret < 0) {
		pr_err("iio_read_channel_processed fail\n");
		goto BAT_Fail;
	}

	iio_channel_release(channel);
	pr_debug("[%s]bat_id_adc_ch=%d, val=%d\n", __FUNCTION__, number, val);

	return val;
BAT_Fail:
    pr_err("BAT_Fail, load default battery profile\n");

	return 0;

}
EXPORT_SYMBOL(battery_get_bat_id_voltage);

int battery_get_bat_id(void)
{
	static int id_voltage = 0;
	static int id = 0;
	static int detect_cnt = 0;

	if (!detect_cnt) {
		id_voltage = battery_get_bat_id_voltage();
		/* Tab A7 T618 code for SR-AX6189A-01-108 by shixuanxuan at 20211223 start */
#ifdef CONFIG_UMS512_25C10_CHARGER
		if (id_voltage >= BATTERY_SCUD_BYD_ID_VOLTAGE_LOW && id_voltage <= BATTERY_SCUD_BYD_ID_VOLTAGE_UP) {
			id = BATTERY_SCUD_BYD;
		} else if (id_voltage >= BATTERY_ATL_NVT_ID_VOLTAGE_LOW && id_voltage <= BATTERY_ATL_NVT_ID_VOLTAGE_UP) {
			id = BATTERY_ATL_NVT;
		} else {
			id = BATTERY_SCUD_BYD;
		}

		pr_info("[%s]Tab A7 bat_id_adc_ch=%d, id_voltage=%d, bat_id=%d\n",
			__FUNCTION__, bat_id_get_adc_num(), id_voltage, id);

		return id;
#endif
		/* Tab A7 T618 code for SR-AX6189A-01-108 by shixuanxuan at 20211223 end */

		if (id_voltage >= BATTERY_SCUD_BYD_ID_VOLTAGE_LOW && id_voltage <= BATTERY_SCUD_BYD_ID_VOLTAGE_UP) {
			id = BATTERY_SCUD_BYD;
		} else if (id_voltage >= BATTERY_ATL_NVT_ID_VOLTAGE_LOW && id_voltage <= BATTERY_ATL_NVT_ID_VOLTAGE_UP) {
			id = BATTERY_ATL_NVT;
		/* Tab A8 code for SR-AX6300-01-181 by zhangyanlong at 20210817 start */
		#ifdef CONFIG_TARGET_UMS9230_4H10
		} else if (id_voltage >= BATTERY_SCUD_SDI_ID_VOLTAGE_LOW && id_voltage <= BATTERY_SCUD_SDI_ID_VOLTAGE_UP) {
			id = BATTERY_SCUD_SDI;
		} else {
			id = BATTERY_SCUD_BYD;
			pr_err("bat_id get fail, load default battery profile BATTERY_SCUD_BYD via default id\n");
		#elif  CONFIG_TARGET_UMS512_1H10
		} else if (id_voltage >= BATTERY_SCUD_ATL_ID_VOLTAGE_LOW && id_voltage <= BATTERY_SCUD_ATL_ID_VOLTAGE_UP) {
			id = BATTERY_SCUD_ATL;
		} else {
			id = BATTERY_ATL_NVT;
			pr_err("bat_id get fail, load default battery profile BATTERY_ATL_NVT via default id\n");
		#endif
		/* Tab A8 code for SR-AX6300-01-181 by zhangyanlong at 20210817 end */
		}
		// detect_cnt = 1;

		pr_info("[%s]bat_id_adc_ch=%d, id_voltage=%d, bat_id=%d\n",
			__FUNCTION__, bat_id_get_adc_num(), id_voltage, id);
	}

	return id;
}
/* HS03 code for SR-SL6215-01-606 by gaochao at 20210813 end */
EXPORT_SYMBOL(battery_get_bat_id);

int battery_id_via_adc_probe(struct platform_device *pdev)
{
	int ret;

    pr_info("[%s] start\n", __FUNCTION__);
	ret = bat_id_get_adc_info(&pdev->dev);
	if (ret < 0) {
		pr_err("[bat_id_get_adc_info] fail\n");
		// return ret;
	}
	g_md_adc_pdev = pdev;

    pr_info("[%s] end\n", __FUNCTION__);

	return 0;
}

static const struct of_device_id bat_id_auxadc_of_ids[] = {
	{.compatible = "bat_id_via_adc"},
	{}
};


static struct platform_driver bat_id_adc_driver = {

	.driver = {
			.name = "bat_id_via_adc",
			.of_match_table = bat_id_auxadc_of_ids,
	},

	.probe = battery_id_via_adc_probe,
};

static int __init battery_id_via_adc_init(void)
{
	int ret;

	ret = platform_driver_register(&bat_id_adc_driver);
	if (ret) {
		pr_err("bat_id adc driver init fail %d\n", ret);
		return ret;
	}
	return 0;
}

module_init(battery_id_via_adc_init);

MODULE_AUTHOR("zh");
MODULE_DESCRIPTION("battery adc driver");
MODULE_LICENSE("GPL");
/* HS03 code for SR-SL6215-01-181 by gaochao at 20210713 end */
