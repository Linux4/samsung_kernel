/* drivers/input/touchscreen/ft5x06_sec.c
 *
 * FocalTech ft6x06 TouchScreen driver.
 *
 * Copyright (c) 2010  Focal tech Ltd.
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#include <linux/mutex.h>
#include <linux/i2c.h>
#include <linux/delay.h>
#include <linux/slab.h>
#include <linux/input.h>
#include <linux/stat.h>
#include <linux/err.h>
#include <linux/uaccess.h>
#include <linux/mutex.h>
#include <linux/interrupt.h>
#include <mach/irqs.h>

#include <asm/sec/sec_debug.h>

#include "ft6x06_sec.h"

enum {
	WAITING = 0,
	RUNNING,
	OK,
	FAIL,
	NOT_APPLICABLE,
};

struct tsp_cmd {
	struct list_head list;
	const char *cmd_name;
	void (*cmd_func)(void *device_data);
};

static void fw_update(void *device_data);
static void get_fw_ver_bin(void *device_data);
static void get_fw_ver_ic(void *device_data);
static void module_off_master(void *device_data);
static void module_on_master(void *device_data);
static void module_off_slave(void *device_data);
static void module_on_slave(void *device_data);
static void get_chip_vendor(void *device_data);
static void get_chip_name(void *device_data);
static void get_threshold(void *device_data);
static void get_x_num(void *device_data);
static void get_y_num(void *device_data);
static void not_support_cmd(void *device_data);

/* Vendor dependant command */
static void get_reference(void *device_data);
static void run_reference_read(void *device_data);
static void dead_zone_enable(void *device_data);


#define TSP_CMD(name, func) .cmd_name = name, .cmd_func = func
static struct tsp_cmd tsp_cmds[] = {
	{TSP_CMD("fw_update", fw_update),},
	{TSP_CMD("get_fw_ver_bin", get_fw_ver_bin),},
	{TSP_CMD("get_fw_ver_ic", get_fw_ver_ic),},
	{TSP_CMD("get_threshold", get_threshold),},
	{TSP_CMD("module_off_master", module_off_master),},
	{TSP_CMD("module_on_master", module_on_master),},
	{TSP_CMD("module_off_slave", module_off_slave),},
	{TSP_CMD("module_on_slave", module_on_slave),},
	{TSP_CMD("get_chip_vendor", get_chip_vendor),},
	{TSP_CMD("get_chip_name", get_chip_name),},
	{TSP_CMD("get_x_num", get_x_num),},
	{TSP_CMD("get_y_num", get_y_num),},
	{TSP_CMD("not_support_cmd", not_support_cmd),},

	/* vendor dependant command */
	{TSP_CMD("run_reference_read", run_reference_read),},
	{TSP_CMD("get_dnd_all_data", get_reference),},
	{TSP_CMD("dead_zone_enable", dead_zone_enable),},
};

static inline void set_cmd_result(struct ft6x06_ts_data *info, char *buff, int len)
{
	strncat(info->factory_info->cmd_result, buff, len);

	return;
}

static inline void set_default_result(struct ft6x06_ts_data *info)
{
	char delim = ':';
	memset(info->factory_info->cmd_result, 0x00, ARRAY_SIZE(info->factory_info->cmd_result));
	memcpy(info->factory_info->cmd_result, info->factory_info->cmd, strlen(info->factory_info->cmd));
	strncat(info->factory_info->cmd_result, &delim, 1);

	return;
}

#define MAX_FW_PATH 255
#define TSP_FW_FILENAME "focaltech_fw.bin"


static void fw_update(void *device_data)
{
	struct ft6x06_ts_data *info = (struct ft6x06_ts_data *)device_data;
	struct i2c_client *client = info->client;
	int ret = 0;
	char result[16] = {0};

	set_default_result(info);

	switch (info->factory_info->cmd_param[0]) {
	case BUILT_IN:
		mutex_lock(&g_device_mutex);
		disable_irq(client->irq);
		ret = fts_ctpm_fw_upgrade_with_i_file(client);
		if (ret != 0) {
			dev_err(&client->dev, "%s ERROR:[FTS] upgrade failed.\n",	__func__);
			info->factory_info->cmd_state = 3;
			enable_irq(client->irq);
			mutex_unlock(&g_device_mutex);
			goto update_fail;
		}

		enable_irq(client->irq);
		mutex_unlock(&g_device_mutex);
		break;
		
	case UMS:
		mutex_lock(&g_device_mutex);
		disable_irq(client->irq);
	
		ret = fts_ctpm_fw_upgrade_with_app_file(client, TSP_FW_FILENAME);
		if (ret != 0) {
			dev_err(&client->dev, "%s ERROR:[FTS] upgrade failed.\n", __func__);
			info->factory_info->cmd_state = 3;
			enable_irq(client->irq);
			mutex_unlock(&g_device_mutex);
			goto update_fail;
		}
		
		enable_irq(client->irq);
		mutex_unlock(&g_device_mutex);
		break;

	default:
		dev_err(&client->dev, "invalid fw file type!!\n");
		goto update_fail;
	}

	info->factory_info->cmd_state = 2;
	snprintf(result, sizeof(result) , "%s", "OK");
	set_cmd_result(info, result,
			strnlen(result, sizeof(result))); 

	return;

update_fail:
	snprintf(result, sizeof(result) , "%s", "NG");
	set_cmd_result(info, result, strnlen(result, sizeof(result)));

	return;
}

static void get_fw_ver_bin(void *device_data)
{
	struct ft6x06_ts_data *info = (struct ft6x06_ts_data *)device_data;
	struct i2c_client *client = info->client;
	struct tsp_factory_info *finfo = info->factory_info;
	u8 uc_host_fm_ver;

	set_default_result(info);

	uc_host_fm_ver = fts_ctpm_get_i_file_ver();

	snprintf(finfo->cmd_buff, sizeof(finfo->cmd_buff), "FT%06X", uc_host_fm_ver);

	set_cmd_result(info, finfo->cmd_buff,
			strnlen(finfo->cmd_buff, sizeof(finfo->cmd_buff)));
	finfo->cmd_state = OK;

	dev_info(&client->dev, "%s: %s(%d)\n", __func__, finfo->cmd_buff,
			(int)strnlen(finfo->cmd_buff, sizeof(finfo->cmd_buff)));

	return;
}

static void get_fw_ver_ic(void *device_data)
{
	struct ft6x06_ts_data *info = (struct ft6x06_ts_data *)device_data;
	struct i2c_client *client = info->client;
	struct tsp_factory_info *finfo = info->factory_info;
	
	set_default_result(info);

	snprintf(finfo->cmd_buff, sizeof(finfo->cmd_buff), "FT%06X", info->fw_version);

	set_cmd_result(info, finfo->cmd_buff,
			strnlen(finfo->cmd_buff, sizeof(finfo->cmd_buff)));
	finfo->cmd_state = OK;

	dev_info(&client->dev, "%s: %s(%d)\n", __func__, finfo->cmd_buff,
			(int)strnlen(finfo->cmd_buff, sizeof(finfo->cmd_buff)));

	return;
}

static void get_threshold(void *device_data)
{
	struct ft6x06_ts_data *info = (struct ft6x06_ts_data *)device_data;
	struct i2c_client *client = info->client;
	struct tsp_factory_info *finfo = info->factory_info;

	set_default_result(info);

	snprintf(finfo->cmd_buff, sizeof(finfo->cmd_buff),
			"%d", info->threshold);
	set_cmd_result(info, finfo->cmd_buff,
			strnlen(finfo->cmd_buff, sizeof(finfo->cmd_buff)));
	finfo->cmd_state = OK;

	dev_info(&client->dev, "%s: %s(%d)\n", __func__, finfo->cmd_buff,
			(int)strnlen(finfo->cmd_buff, sizeof(finfo->cmd_buff)));

	return;
}



static void module_off_master(void *device_data)
{
	return;
}

static void module_on_master(void *device_data)
{
	return;
}

static void module_off_slave(void *device_data)
{
	return;
}

static void module_on_slave(void *device_data)
{
	return;
}

#define FT6x06_VENDOR_NAME "FOCALTECH"

static void get_chip_vendor(void *device_data)
{
	struct ft6x06_ts_data *info = (struct ft6x06_ts_data *)device_data;
	struct i2c_client *client = info->client;
	struct tsp_factory_info *finfo = info->factory_info;

	set_default_result(info);

	snprintf(finfo->cmd_buff, sizeof(finfo->cmd_buff),
			"%s", FT6x06_VENDOR_NAME);
	set_cmd_result(info, finfo->cmd_buff,
			strnlen(finfo->cmd_buff, sizeof(finfo->cmd_buff)));
	finfo->cmd_state = OK;

	dev_info(&client->dev, "%s: %s(%d)\n", __func__, finfo->cmd_buff,
			(int)strnlen(finfo->cmd_buff, sizeof(finfo->cmd_buff)));

	return;
}

#define FT6X06_CHIP_NAME "FT6x06"

static void get_chip_name(void *device_data)
{
	struct ft6x06_ts_data *info = (struct ft6x06_ts_data *)device_data;
	struct i2c_client *client = info->client;
	struct tsp_factory_info *finfo = info->factory_info;

	set_default_result(info);

	snprintf(finfo->cmd_buff, sizeof(finfo->cmd_buff), "%s", FT6X06_CHIP_NAME);
	set_cmd_result(info, finfo->cmd_buff,
			strnlen(finfo->cmd_buff, sizeof(finfo->cmd_buff)));
	finfo->cmd_state = OK;

	dev_info(&client->dev, "%s: %s(%d)\n", __func__, finfo->cmd_buff,
			(int)strnlen(finfo->cmd_buff, sizeof(finfo->cmd_buff)));

	return;
}

static void get_x_num(void *device_data)
{
	struct ft6x06_ts_data *info = (struct ft6x06_ts_data *)device_data;
	struct i2c_client *client = info->client;
	struct tsp_factory_info *finfo = info->factory_info;

	set_default_result(info);

	snprintf(finfo->cmd_buff, sizeof(finfo->cmd_buff),
			"%u", info->x_max); /*노드개수를 의미하는지 ? ymsong */
	set_cmd_result(info, finfo->cmd_buff,
			strnlen(finfo->cmd_buff, sizeof(finfo->cmd_buff)));
	finfo->cmd_state = OK;

	dev_info(&client->dev, "%s: %s(%d)\n", __func__, finfo->cmd_buff,
			(int)strnlen(finfo->cmd_buff, sizeof(finfo->cmd_buff)));

	return;
}

static void get_y_num(void *device_data)
{
	struct ft6x06_ts_data *info = (struct ft6x06_ts_data *)device_data;
	struct i2c_client *client = info->client;
	struct tsp_factory_info *finfo = info->factory_info;

	set_default_result(info);

	snprintf(finfo->cmd_buff, sizeof(finfo->cmd_buff),
			"%u", info->y_max); /*노드개수를 의미하는지 ? ymsong */
	set_cmd_result(info, finfo->cmd_buff,
			strnlen(finfo->cmd_buff, sizeof(finfo->cmd_buff)));
	finfo->cmd_state = OK;

	dev_info(&client->dev, "%s: %s(%d)\n", __func__, finfo->cmd_buff,
			(int)strnlen(finfo->cmd_buff, sizeof(finfo->cmd_buff)));

	return;
}

static void not_support_cmd(void *device_data)
{
	struct ft6x06_ts_data *info = (struct ft6x06_ts_data *)device_data;
	struct i2c_client *client = info->client;
	struct tsp_factory_info *finfo = info->factory_info;

	set_default_result(info);

	sprintf(finfo->cmd_buff, "%s", "NA");
	set_cmd_result(info, finfo->cmd_buff,
			strnlen(finfo->cmd_buff, sizeof(finfo->cmd_buff)));
	info->factory_info->cmd_state = NOT_APPLICABLE;

	dev_info(&client->dev, "%s: \"%s(%d)\"\n", __func__, finfo->cmd_buff,
			(int)strnlen(finfo->cmd_buff, sizeof(finfo->cmd_buff)));

	return;
}

static void run_reference_read(void *device_data)
{
	struct ft6x06_ts_data *info = (struct ft6x06_ts_data *)device_data;
	struct i2c_client *client = info->client;
	struct tsp_factory_info *finfo = info->factory_info;
	//struct tsp_raw_data *raw_data = info->raw_data;
	u16 min, max;
	//s32 i, j, offset;

	set_default_result(info);
#if 0
// tsp 칩셋으로부터 dnd_data를  읽어옴 
	ts_set_touchmode(TOUCH_DND_MODE);
	get_raw_data(info, (u8 *)raw_data->dnd_data, 5);
	ts_set_touchmode(TOUCH_POINT_MODE);

	min = 0xFFFF;
	max = 0x0000;

	// 읽어온 데이터의 min, max값을 구함 
	for (i = 0; i < info->cap_info.x_node_num; i++) {
		for (j = 0; j < info->cap_info.y_node_num; j++) {
			offset = i * info->cap_info.y_node_num + j;

			if (raw_data->dnd_data[offset] &&
				  raw_data->dnd_data[offset] < min)
				min = raw_data->dnd_data[offset];

			if (raw_data->dnd_data[offset] > max)
				max = raw_data->dnd_data[offset];
		}
	}
#endif
	min = 0x000;
	max = 0x0000;

// min, max값을 공정어플로 올려보내주면 spec in/out 여부 확인하도록 되어 잇음
	snprintf(finfo->cmd_buff, sizeof(finfo->cmd_buff), "%d,%d\n", min, max);
	set_cmd_result(info, finfo->cmd_buff,
			strnlen(finfo->cmd_buff, sizeof(finfo->cmd_buff)));
	finfo->cmd_state = OK;

	dev_info(&client->dev, "%s: %s\n", __func__, finfo->cmd_buff);

	return;
}

static void get_reference(void *device_data)
{
	struct ft6x06_ts_data *info = (struct ft6x06_ts_data *)device_data;
/*	struct i2c_client *client = info->client;
	struct tsp_factory_info *finfo = info->factory_info;
	struct tsp_raw_data *raw_data = info->raw_data;
	unsigned int val;
	int x_node, y_node;
	int node_num;*/

	set_default_result(info);
/*
	x_node = finfo->cmd_param[0];
	y_node = finfo->cmd_param[1];

	if (x_node < 0 || x_node >= info->cap_info.x_node_num ||
		y_node < 0 || y_node >= info->cap_info.y_node_num) {
		snprintf(finfo->cmd_buff, sizeof(finfo->cmd_buff), "%s", "abnormal");
		set_cmd_result(info, finfo->cmd_buff,
		strnlen(finfo->cmd_buff, sizeof(finfo->cmd_buff)));
		finfo->cmd_state = FAIL;
		return;
	}

	node_num = x_node * info->cap_info.y_node_num + y_node;

	val = raw_data->ref_data[node_num];
	snprintf(finfo->cmd_buff, sizeof(finfo->cmd_buff), "%u", val);
	set_cmd_result(info, finfo->cmd_buff,
	strnlen(finfo->cmd_buff, sizeof(finfo->cmd_buff)));
	finfo->cmd_state = OK;

	dev_info(&client->dev, "%s: %s(%d)\n", __func__, finfo->cmd_buff,
			(int)strnlen(finfo->cmd_buff, sizeof(finfo->cmd_buff)));
*/
	return;
}


static void dead_zone_enable(void *device_data)
{
	struct ft6x06_ts_data *info = (struct ft6x06_ts_data *)device_data;
	struct tsp_factory_info *finfo = info->factory_info;
	struct i2c_client *client = info->client;
	int val = finfo->cmd_param[0];

	set_default_result(info);

#if 0  // dead zone enable/disable register setting
	if(val) //disable
		zinitix_bit_clr(m_optional_mode, 3);
	else //enable
		zinitix_bit_set(m_optional_mode, 3);
#endif
	snprintf(finfo->cmd_buff, sizeof(finfo->cmd_buff),
			"dead_zone %s", val ? "disable" : "enable");
	set_cmd_result(info, finfo->cmd_buff,
			(int)strnlen(finfo->cmd_buff, sizeof(finfo->cmd_buff)));
	finfo->cmd_state = OK;

	dev_info(&client->dev, "%s(), %s\n", __func__, finfo->cmd_buff);

	return;
}

static ssize_t store_cmd(struct device *dev, struct device_attribute
		*devattr, const char *buf, size_t count)
{
	struct ft6x06_ts_data *info = dev_get_drvdata(dev);
	struct i2c_client *client = info->client;
	struct tsp_factory_info *finfo = info->factory_info;
	char *cur, *start, *end;
	char buff[TSP_CMD_STR_LEN] = {0};
	int len, i;
	struct tsp_cmd *tsp_cmd_ptr = NULL;
	char delim = ',';
	bool cmd_found = false;
	int param_cnt = 0;

	if (finfo->cmd_is_running == true) {
		dev_err(&client->dev, "%s: other cmd is running\n", __func__);
		goto err_out;
	}

	/* check lock  */
	mutex_lock(&finfo->cmd_lock);
	finfo->cmd_is_running = true;
	mutex_unlock(&finfo->cmd_lock);

	finfo->cmd_state = RUNNING;

	for (i = 0; i < ARRAY_SIZE(finfo->cmd_param); i++)
		finfo->cmd_param[i] = 0;

	len = (int)count;
	if (*(buf + len - 1) == '\n')
		len--;

	memset(finfo->cmd, 0x00, ARRAY_SIZE(finfo->cmd));
	memcpy(finfo->cmd, buf, len);

	cur = strchr(buf, (int)delim);
	if (cur)
		memcpy(buff, buf, cur - buf);
	else
		memcpy(buff, buf, len);

	/* find command */
	list_for_each_entry(tsp_cmd_ptr, &finfo->cmd_list_head, list) {
		if (!strcmp(buff, tsp_cmd_ptr->cmd_name)) {
			cmd_found = true;
			break;
		}
	}

	/* set not_support_cmd */
	if (!cmd_found) {
		list_for_each_entry(tsp_cmd_ptr, &finfo->cmd_list_head, list) {
			if (!strcmp("not_support_cmd", tsp_cmd_ptr->cmd_name))
				break;
		}
	}

	/* parsing parameters */
	if (cur && cmd_found) {
		cur++;
		start = cur;
		memset(buff, 0x00, ARRAY_SIZE(buff));
		do {
			if (*cur == delim || cur - buf == len) {
				end = cur;
				memcpy(buff, start, end - start);
				*(buff + strlen(buff)) = '\0';
				finfo->cmd_param[param_cnt] =
					(int)simple_strtol(buff, NULL, 10);
				start = cur + 1;
				memset(buff, 0x00, ARRAY_SIZE(buff));
				param_cnt++;
			}
			cur++;
		} while (cur - buf <= len);
	}

	dev_info(&client->dev, "cmd = %s\n", tsp_cmd_ptr->cmd_name);
	/*	for (i = 0; i < param_cnt; i++)
		dev_info(&client->dev, "cmd param %d= %d\n", i, finfo->cmd_param[i]);*/

	tsp_cmd_ptr->cmd_func(info);

	mutex_lock(&finfo->cmd_lock);
	finfo->cmd_is_running = false;
	mutex_unlock(&finfo->cmd_lock);

err_out:
	return count;
}

static ssize_t show_cmd_status(struct device *dev,
		struct device_attribute *devattr, char *buf)
{
	struct ft6x06_ts_data *info = dev_get_drvdata(dev);
	struct i2c_client *client = info->client;
	struct tsp_factory_info *finfo = info->factory_info;

	dev_info(&client->dev, "tsp cmd: status:%d\n", finfo->cmd_state);

	if (finfo->cmd_state == WAITING)
		snprintf(finfo->cmd_buff, sizeof(finfo->cmd_buff), "WAITING");

	else if (finfo->cmd_state == RUNNING)
		snprintf(finfo->cmd_buff, sizeof(finfo->cmd_buff), "RUNNING");

	else if (finfo->cmd_state == OK)
		snprintf(finfo->cmd_buff, sizeof(finfo->cmd_buff), "OK");

	else if (finfo->cmd_state == FAIL)
		snprintf(finfo->cmd_buff, sizeof(finfo->cmd_buff), "FAIL");

	else if (finfo->cmd_state == NOT_APPLICABLE)
		snprintf(finfo->cmd_buff, sizeof(finfo->cmd_buff), "NOT_APPLICABLE");

	return snprintf(buf, sizeof(finfo->cmd_buff),
			"%s\n", finfo->cmd_buff);
}

static ssize_t show_cmd_result(struct device *dev, struct device_attribute
		*devattr, char *buf)
{
	struct ft6x06_ts_data *info = dev_get_drvdata(dev);
	struct i2c_client *client = info->client;
	struct tsp_factory_info *finfo = info->factory_info;

	dev_info(&client->dev, "tsp cmd: result: %s\n", finfo->cmd_result);

	finfo->cmd_state = WAITING;

	return snprintf(buf, sizeof(finfo->cmd_result),
			"%s\n", finfo->cmd_result);
}

/* sysfs: /sys/class/sec/tsp/input/enabled */
static ssize_t show_enabled(struct device *dev, struct device_attribute
		*devattr, char *buf)

{
	struct ft6x06_ts_data *info = dev_get_drvdata(dev);
	struct i2c_client *client = info->client;

	int val =0;
	
	if(info->work_state == EALRY_SUSPEND)
		val =0;
	else
		val =1;

	dev_info(&client->dev, " enabled is %d\n", info->work_state);

	return snprintf(buf, sizeof(val),"%d\n", val);
}

static DEVICE_ATTR(cmd, S_IWUSR | S_IWGRP, NULL, store_cmd);
static DEVICE_ATTR(cmd_status, S_IRUGO, show_cmd_status, NULL);
static DEVICE_ATTR(cmd_result, S_IRUGO, show_cmd_result, NULL);
static DEVICE_ATTR(enabled, S_IRUGO, show_enabled, NULL);


static struct attribute *touchscreen_attributes[] = {
	&dev_attr_cmd.attr,
	&dev_attr_cmd_status.attr,
	&dev_attr_cmd_result.attr,
	NULL,
};

static struct attribute *sec_touch_pretest_attributes[] = {
	&dev_attr_enabled.attr,
	NULL,
};

static struct attribute_group touchscreen_attr_group = {
	.attrs = touchscreen_attributes,
};

static struct attribute_group sec_touch_pertest_attr_group = {
	.attrs	= sec_touch_pretest_attributes,
};


static ssize_t show_touchkey_threshold(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct ft6x06_ts_data *info = dev_get_drvdata(dev);
	struct i2c_client *client = info->client;

	dev_info(&client->dev, "%s: key threshold = %d\n", __func__,
			info->key_threshold);

	return snprintf(buf, 41, "%d", info->key_threshold);
}

static ssize_t show_touchkey_sensitivity(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct ft6x06_ts_data *info = dev_get_drvdata(dev);
	struct i2c_client *client = info->client;
	u16 val = 0;
	int ret = 0;
	int i;

	if (!strcmp(attr->attr.name, "touchkey_recent"))
		i = 0;
	else if (!strcmp(attr->attr.name, "touchkey_back"))
		i = 1;
	else {
		dev_err(&client->dev, "%s: Invalid attribute\n", __func__);

		goto err_out;
	}

	//ret = read_data(client, BT541_BTN_WIDTH + i, (u8 *)&val, 2);
	ret= 100; // ymsong 임시 
	if (ret < 0) {
		dev_err(&client->dev, "%s: Failed to read %d's key sensitivity\n",
				__func__, i);

		goto err_out;
	}
	dev_info(&client->dev, "%s: %d's key sensitivity = %d\n",
			__func__, i, val);

	return snprintf(buf, 6, "%d", val);

err_out:
	return sprintf(buf, "NG");
}

static DEVICE_ATTR(touchkey_threshold, S_IRUGO, show_touchkey_threshold, NULL);
static DEVICE_ATTR(touchkey_recent, S_IRUGO, show_touchkey_sensitivity, NULL);
static DEVICE_ATTR(touchkey_back, S_IRUGO, show_touchkey_sensitivity, NULL);

static struct attribute *touchkey_attributes[] = {
	&dev_attr_touchkey_threshold.attr,
	&dev_attr_touchkey_back.attr,
	&dev_attr_touchkey_recent.attr,
	NULL,
};
static struct attribute_group touchkey_attr_group = {
	.attrs = touchkey_attributes,
};

int init_sec_factory(struct ft6x06_ts_data *info)
{
	struct device *factory_ts_dev;
	struct device *factory_tk_dev;
	struct device *sec_pretest_dev;
	struct tsp_factory_info *factory_info;
	struct tsp_raw_data *raw_data;
	int ret = 0;
	int i;

	factory_info = kzalloc(sizeof(struct tsp_factory_info), GFP_KERNEL);
	if (unlikely(!factory_info)) {
		dev_err(&info->client->dev, "%s: Failed to allocate memory\n",
				__func__);
		ret = -ENOMEM;

		goto err_alloc1;
	}
	raw_data = kzalloc(sizeof(struct tsp_raw_data), GFP_KERNEL);
	if (unlikely(!raw_data)) {
		dev_err(&info->client->dev, "%s: Failed to allocate memory\n",
				__func__);
		ret = -ENOMEM;

		goto err_alloc2;
	}

	INIT_LIST_HEAD(&factory_info->cmd_list_head);
	for (i = 0; i < ARRAY_SIZE(tsp_cmds); i++)
		list_add_tail(&tsp_cmds[i].list, &factory_info->cmd_list_head);

	factory_ts_dev = device_create(sec_class, NULL, 0, info, "tsp");
	if (unlikely(!factory_ts_dev)) {
		dev_err(&info->client->dev, "Failed to create factory dev\n");
		ret = -ENODEV;
		goto err_create_device;
	}

	factory_tk_dev = device_create(sec_class, NULL, 0, info, "sec_touchkey");
	if (IS_ERR(factory_tk_dev)) {
		dev_err(&info->client->dev, "Failed to create factory dev\n");
		ret = -ENODEV;
		goto err_create_device;
	}

	/* /sys/class/sec/tsp/input/ */
	sec_pretest_dev = device_create(sec_class, factory_ts_dev, 4, info, "input");
	if (IS_ERR(sec_pretest_dev)) {
		dev_err(&info->client->dev, "Failed to create device (%s)!\n", "tsp");
		goto err_create_device;
	}

	ret = sysfs_create_group(&factory_ts_dev->kobj, &touchscreen_attr_group);
	if (unlikely(ret)) {
		dev_err(&info->client->dev, "Failed to create touchscreen sysfs group\n");
		goto err_create_sysfs;
	}

	ret = sysfs_create_group(&factory_tk_dev->kobj, &touchkey_attr_group);
	if (unlikely(ret)) {
		dev_err(&info->client->dev, "Failed to create touchkey sysfs group\n");
		goto err_create_sysfs;
	}

	/* /sys/class/sec/tsp/... */
	if (sysfs_create_group(&sec_pretest_dev->kobj, &sec_touch_pertest_attr_group)) {
		dev_err(&info->client->dev, "Failed to create sysfs group(%s)!\n", "input");
		goto err_create_sysfs;
	}

	mutex_init(&factory_info->cmd_lock);
	factory_info->cmd_is_running = false;

	info->factory_info = factory_info;
	info->raw_data = raw_data;

	return ret;

err_create_sysfs:
err_create_device:
	kfree(raw_data);
err_alloc2:
	kfree(factory_info);
err_alloc1:

	return ret;
}
//EXPORT_SYMBOL(init_sec_factory);
