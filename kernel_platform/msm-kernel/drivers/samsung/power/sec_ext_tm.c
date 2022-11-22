// SPDX-License-Identifier: GPL-2.0
/*
 *  ss_thermal_log.c - SAMSUNG Thermal logging.
 *
 */

#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/device.h>
#include <linux/thermal.h>

#define MAX_CHANS	2
#define MAX_NAME_SZ	40

typedef struct sec_temp_table_data {
	int adc;
	int temp;
} sec_temp_table_data_t;

typedef struct sec_adc_tm_data {
	unsigned int chans[MAX_CHANS];
	unsigned int types[MAX_CHANS];
	unsigned int temp_table_size[MAX_CHANS];
	sec_temp_table_data_t *temp_table[MAX_CHANS];
} sec_adc_tm_data_t;

typedef struct adc_table_info {
	const char adc[MAX_NAME_SZ];
	const char data[MAX_NAME_SZ];
} adc_table_info_t;

static adc_table_info_t adc_tbl_dt[] = {
	{"battery,temp_table_adc",		"battery,temp_table_data"},		/* BAT_THM */
	{"battery,usb_temp_table_adc",	"battery,usb_temp_table_data"},	/* USB_THM */
	{"battery,wpc_temp_table_adc",	"battery,wpc_temp_table_data"}	/* WPC_THM */
};

static sec_adc_tm_data_t *sec_data;

int sec_convert_adc_to_temp(unsigned int adc_ch, int temp_adc)
{
	int temp = 25000;
	int low = 0;
	int high = 0;
	int mid = 0;
	const sec_temp_table_data_t *temp_adc_table = {0, };
	unsigned int temp_adc_table_size = 0;

	if (!sec_data) {
		pr_info("%s: battery data is not ready yet\n", __func__);
		goto temp_to_adc_goto;
	}

	if (adc_ch == sec_data->chans[0] && sec_data->temp_table[0]) {
		temp_adc_table = sec_data->temp_table[0];
		temp_adc_table_size = sec_data->temp_table_size[0];
	} else if (adc_ch == sec_data->chans[1] && sec_data->temp_table[1]) {
		temp_adc_table = sec_data->temp_table[1];
		temp_adc_table_size = sec_data->temp_table_size[1];
	} else {
		pr_err("%s: Invalid channel\n", __func__);
		goto temp_to_adc_goto;
	}

	if (temp_adc_table[0].adc >= temp_adc) {
		temp = temp_adc_table[0].temp;
		goto temp_to_adc_goto;
	} else if (temp_adc_table[temp_adc_table_size-1].adc <= temp_adc) {
		temp = temp_adc_table[temp_adc_table_size-1].temp;
		goto temp_to_adc_goto;
	}

	high = temp_adc_table_size - 1;

	while (low <= high) {
		mid = (low + high) / 2;
		if (temp_adc_table[mid].adc > temp_adc)
			high = mid - 1;
		else if (temp_adc_table[mid].adc < temp_adc)
			low = mid + 1;
		else {
			temp = temp_adc_table[mid].temp;
			goto temp_to_adc_goto;
		}
	}

	temp = temp_adc_table[high].temp;
	temp += ((temp_adc_table[low].temp - temp_adc_table[high].temp) *
		 (temp_adc - temp_adc_table[high].adc)) /
		(temp_adc_table[low].adc - temp_adc_table[high].adc);

temp_to_adc_goto:
	return (temp == 25000 ? temp : temp * 100);
}
EXPORT_SYMBOL_GPL(sec_convert_adc_to_temp);

int sec_get_thr_voltage(unsigned int adc_ch, int temp)
{
	int volt = 0;
	int low = 0;
	int high = 0;
	int mid = 0;
	const sec_temp_table_data_t *temp_adc_table = {0, };
	unsigned int temp_adc_table_size = 0;

	if (!sec_data) {
		pr_info("%s: battery data is not ready yet\n", __func__);
		goto get_thr_voltage_goto;
	}

	if (adc_ch == sec_data->chans[0] && sec_data->temp_table[0]) {
		temp_adc_table = sec_data->temp_table[0];
		temp_adc_table_size = sec_data->temp_table_size[0];
	} else if (adc_ch == sec_data->chans[1] && sec_data->temp_table[1]) {
		temp_adc_table = sec_data->temp_table[1];
		temp_adc_table_size = sec_data->temp_table_size[1];
	} else {
		pr_err("%s: Invalid channel\n", __func__);
		goto get_thr_voltage_goto;
	}

	if (temp > 900 || temp < -200) {
		pr_err("%s: out of temperature\n", __func__);
		goto get_thr_voltage_goto;
	}

	high = temp_adc_table_size - 1;

	while (low <= high) {
		mid = (low + high) / 2;
		if (temp_adc_table[mid].temp < temp)
			high = mid - 1;
		else if (temp_adc_table[mid].temp > temp)
			low = mid + 1;
		else {
			volt = temp_adc_table[mid].adc;
			goto get_thr_voltage_goto;
		}
	}

	volt = temp_adc_table[mid].adc;
get_thr_voltage_goto:
	return volt;
}
EXPORT_SYMBOL_GPL(sec_get_thr_voltage);

static int ext_tm_parse_dt(struct platform_device *pdev, struct device_node *np, int idx)
{
	adc_table_info_t *table;
	const u32 *p;
	int i, value, len = 0;
	int err;

	pr_info("%s: temp_table[%d] type=%u\n", __func__, idx, sec_data->types[idx]);

	table = &adc_tbl_dt[sec_data->types[idx]];
	p = of_get_property(np, table->adc, &len);
	if (!p)
		return -ENOENT;

	sec_data->temp_table_size[idx] = len / sizeof(u32);
	sec_data->temp_table[idx] =
			devm_kzalloc(&pdev->dev, sizeof(sec_temp_table_data_t) *
			sec_data->temp_table_size[idx], GFP_KERNEL);
	if (!sec_data->temp_table[idx])
		return -ENOMEM;

	for (i = 0; i < sec_data->temp_table_size[idx]; i++) {
		err = of_property_read_u32_index(np, table->adc, i, &value);
		sec_data->temp_table[idx][i].adc = (int)value;
		if (err)
			pr_err("%s: .adc[%d] is empty\n", __func__, i);

		err = of_property_read_u32_index(np, table->data, i, &value);
		sec_data->temp_table[idx][i].temp = (int)value;
		if (err)
			pr_err("%s: .data[%d] is empty\n", __func__, i);
	}
	return 0;
}


static int ext_tm_probe(struct platform_device *pdev)
{
	struct device_node *node = pdev->dev.of_node;
	struct device_node *np;
	int i, err;
	int adc_table_cnt = sizeof(adc_tbl_dt) / sizeof(adc_table_info_t);

	pr_info("%s\n", __func__);

	np = of_parse_phandle(node, "batt_node", 0);
	if (!np) {
		pr_err("%s: np is NULL\n", __func__);
		return -ENOENT;
	}

	sec_data = devm_kzalloc(&pdev->dev, sizeof(*sec_data), GFP_KERNEL);
	if (!sec_data)
		return -ENOMEM;

	err = of_property_read_u32_array(node, "types",
			sec_data->types, MAX_CHANS);
	if (err) {
		pr_err("%s: no tm types\n", __func__);
		return -ENOENT;
	}
	err = of_property_read_u32_array(node, "chans",
			sec_data->chans, MAX_CHANS);
	if (err) {
		pr_err("%s: no tm chans\n", __func__);
		return -ENOENT;
	}

	for (i = 0; i < MAX_CHANS; i++) {
		sec_data->chans[i] &= 0x0FF;
		if (sec_data->chans[i] == 0U || sec_data->types[i] >= adc_table_cnt) {
			pr_err("%s: chans[%d] chan or type is invalid\n", __func__, i);
			sec_data->temp_table[i] = NULL;
			continue;
		}
		err = ext_tm_parse_dt(pdev, np, i);
		if (err) {
			pr_err("%s: chans[%d] parsing error:%d\n", __func__, i, err);
			sec_data->temp_table[i] = NULL;
		}
	}

	pr_info("%s: done\n", __func__);
	return 0;
}

static const struct of_device_id ext_tm_match_table[] = {
	{ .compatible = "samsung,ext-tm", },
	{}
};

static struct platform_driver ext_tm_driver = {
	.driver = {
		.name = "samsung,ext-tm",
		.of_match_table	= ext_tm_match_table,
	},
	.probe = ext_tm_probe,
};

module_platform_driver(ext_tm_driver);

MODULE_ALIAS("platform:sec_ext_tm");
MODULE_DESCRIPTION("Samsung Ext Thermal Monitor Driver");
MODULE_LICENSE("GPL v2");
