/* Synaptics Register Mapped Interface (RMI4) I2C Physical Layer Driver.
 * Copyright (c) 2007-2012, Synaptics Incorporated
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
#include <linux/kernel.h>
#include <linux/module.h>
#include <asm/unaligned.h>
#include <mach/cpufreq.h>
#include <linux/slab.h>
#include <linux/i2c.h>
#include <linux/interrupt.h>
#include <linux/delay.h>
#include <linux/input.h>
#include <linux/ctype.h>
#include <linux/hrtimer.h>
#include <linux/firmware.h>
#include <linux/sec_sysfs.h>

#include "sec_ts.h"

#define FACTORY_MODE
#define tostring(x) (#x)

#ifdef FACTORY_MODE
#include <linux/uaccess.h>

#define FT_CMD(name, func) .cmd_name = name, .cmd_func = func

extern struct class *sec_class;

enum {
	TYPE_RAW_DATA = 0,
	TYPE_SIGNAL_DATA = 1,
	TYPE_REMV_BASELINE_DATA = 4,
	TYPE_DECODED_DATA = 5,
	TYPE_REMV_AMB_DATA = 6,
};

enum CMD_STATUS {
	CMD_STATUS_WAITING = 0,
	CMD_STATUS_RUNNING,
	CMD_STATUS_OK,
	CMD_STATUS_FAIL,
	CMD_STATUS_NOT_APPLICABLE,
};

struct ft_cmd {
	struct list_head list;
	const char *cmd_name;
	void (*cmd_func)(void *device_data);
};

static ssize_t cmd_store(struct device *dev, struct device_attribute *attr,
		const char *buf, size_t count);

static ssize_t cmd_status_show(struct device *dev,
		struct device_attribute *attr, char *buf);

static ssize_t cmd_result_show(struct device *dev,
		struct device_attribute *attr, char *buf);

static ssize_t cmd_list_show(struct device *dev,
		struct device_attribute *attr, char *buf);


static DEVICE_ATTR(cmd, S_IWUSR | S_IWGRP, NULL, cmd_store);
static DEVICE_ATTR(cmd_status, S_IRUGO, cmd_status_show, NULL);
static DEVICE_ATTR(cmd_result, S_IRUGO, cmd_result_show, NULL);
static DEVICE_ATTR(cmd_list, S_IRUGO, cmd_list_show, NULL);

static void fw_update(void *device_data);
static void get_fw_ver_bin(void *device_data);
static void get_fw_ver_ic(void *device_data);
static void get_config_ver(void *device_data);
//static void get_threshold(void *device_data);
static void module_off_master(void *device_data);
static void module_on_master(void *device_data);
static void get_chip_vendor(void *device_data);
static void get_chip_name(void *device_data);
static void get_x_num(void *device_data);
static void get_y_num(void *device_data);
//static void get_checksum_data(void *device_data);
static void run_reference_read(void *device_data);
static void get_reference(void *device_data);
static void run_rawcap_read(void *device_data);
static void get_rawcap(void *device_data);
static void run_delta_read(void *device_data);
static void get_delta(void *device_data);
static void not_support_cmd(void *device_data);
static void glove_mode(void *device_data);
static void hover_enable(void *device_data);
static void clear_cover_mode(void *device_data);
#ifdef SMARTCOVER_COVER
static void smartcover_cmd(void *device_data);
#endif

struct ft_cmd ft_cmds[] = {
    {FT_CMD("fw_update", fw_update),},
    {FT_CMD("get_fw_ver_bin", get_fw_ver_bin),},
    {FT_CMD("get_fw_ver_ic", get_fw_ver_ic),},
    {FT_CMD("get_config_ver", get_config_ver),},
	//{FT_CMD("get_threshold", get_threshold),},
    {FT_CMD("module_off_master", module_off_master),},
    {FT_CMD("module_on_master", module_on_master),},
	//{FT_CMD("module_off_slave", not_support_cmd),},
	//{FT_CMD("module_on_slave", not_support_cmd),},
    {FT_CMD("get_chip_vendor", get_chip_vendor),},
    {FT_CMD("get_chip_name", get_chip_name),},
    {FT_CMD("get_x_num", get_x_num),},
    {FT_CMD("get_y_num", get_y_num),},
	//{FT_CMD("get_checksum_data", get_checksum_data),},
    {FT_CMD("run_reference_read", run_reference_read),},
    {FT_CMD("get_reference", get_reference),},
    {FT_CMD("run_rawcap_read", run_rawcap_read),},
    {FT_CMD("get_rawcap", get_rawcap),},
    {FT_CMD("run_delta_read", run_delta_read),},
    {FT_CMD("get_delta", get_delta),},
	{FT_CMD("glove_mode", glove_mode),},
	{FT_CMD("hover_enable", hover_enable),},
	{FT_CMD("clear_cover_mode", clear_cover_mode),},
	{FT_CMD("not_support_cmd", not_support_cmd),},
#ifdef SMARTCOVER_COVER
	{FT_CMD("smartcover_cmd", smartcover_cmd),},
#endif
};

static struct attribute *cmd_attributes[] = {
	&dev_attr_cmd.attr,
	&dev_attr_cmd_status.attr,
	&dev_attr_cmd_result.attr,
	&dev_attr_cmd_list.attr,
	NULL,
};

static struct attribute_group cmd_attr_group = {
	.attrs = cmd_attributes,
};

static int sec_ts_check_index(void *device_data)
{
	struct sec_ts_data *ts = (struct sec_ts_data *)device_data;
	char buff[CMD_STR_LEN] = { 0 };
	int node;

	if (ts->cmd_param[0] < 0
		|| ts->cmd_param[0] > ts->rx_count
		|| ts->cmd_param[1] < 0
		|| ts->cmd_param[1] > ts->tx_count) {
		snprintf(buff, sizeof(buff), "%s", "NG");
		strncat(ts->cmd_result, buff, strnlen(buff, sizeof(buff)));
		ts->cmd_state = CMD_STATUS_FAIL;
		tsp_debug_info(true, &ts->client->dev, "%s: parameter error: %u, %u\n",
				__func__, ts->cmd_param[0], ts->cmd_param[0]);
		node = -1;
		return node;
	}
	node = ts->cmd_param[1] * ts->rx_count + ts->cmd_param[0];
	tsp_debug_info(true, &ts->client->dev, "%s: node = %d\n", __func__, node);
	return node;
}

static void set_default_result(struct sec_ts_data *data)
{
	char delim = ':';

	memset(data->cmd_buff, 0x00, sizeof(data->cmd_buff));
	memset(data->cmd_result, 0x00, sizeof(data->cmd_result));
	memcpy(data->cmd_result, data->cmd, strlen(data->cmd));
	strncat(data->cmd_result, &delim, 1);

	return;
}

static void set_cmd_result(struct sec_ts_data *data, char *buf, int length)
{
	strncat(data->cmd_result, buf, length);

	return;
}

static void clear_cover_cmd_work(struct work_struct *work)
{
	struct sec_ts_data *ts = container_of(work, struct sec_ts_data,
													cover_cmd_work.work);
	if (ts->cmd_is_running) {
		schedule_delayed_work(&ts->cover_cmd_work, msecs_to_jiffies(5));
	} else {
		/* check lock   */
		mutex_lock(&ts->cmd_lock);
		ts->cmd_is_running = true;
		mutex_unlock(&ts->cmd_lock);

		ts->cmd_state = CMD_STATUS_RUNNING;
		tsp_debug_info(true, &ts->client->dev, "%s param = %d, %d\n", __func__,\
		ts->delayed_cmd_param[0], ts->delayed_cmd_param[1]);

		ts->cmd_param[0] = ts->delayed_cmd_param[0];
		if (ts->delayed_cmd_param[0] > 1)
			ts->cmd_param[1] = ts->delayed_cmd_param[1];
		strcpy(ts->cmd, "clear_cover_mode");
		clear_cover_mode(ts);
	}
}
EXPORT_SYMBOL(clear_cover_cmd_work);

static ssize_t cmd_store(struct device *dev, struct device_attribute *attr,
		const char *buf, size_t count)
{
	unsigned char param_cnt = 0;
	char *start;
	char *end;
	char *pos;
	char delim = ',';
	char buffer[CMD_STR_LEN];
	bool cmd_found = false;
	int *param;
	int length;
	struct ft_cmd *ft_cmd_ptr = NULL;
	struct sec_ts_data *ts = dev_get_drvdata(dev);
	//struct factory_data *data = ts->f_data;

	//if (data->cmd_is_running == true) {
	//	printk(KERN_ERR "%s: Still servicing previous command. Skip cmd :%s\n", __func__, buf);
	//	return count;
	//}
	
	/*if (!ts) {
                printk(KERN_ERR "%s: No platform data found\n",
                                __func__);
                return -EINVAL;
        }

        if (!ts->input_dev) {
                printk(KERN_ERR "%s: No input_dev data found\n",
                                __func__);
                return -EINVAL;
        }

        if (count > CMD_STR_LEN) {
                printk(KERN_ERR "%s: overflow command length\n",
                                __func__);
                return -EINVAL;
        }*/
#if 1
	if (ts->cmd_is_running == true) {
		//tsp_debug_err(true, &info->client->dev, "ft_cmd: other cmd is running.\n");
		printk(KERN_ERR "fts_cmd: other cmd is running.\n");
		if (strncmp("clear_cover_mode", buf, 16) == 0) {
				cancel_delayed_work(&ts->cover_cmd_work);
			//tsp_debug_err(true, &info->client->dev,
			//"[cmd is delayed] %d, param = %d, %d\n", __LINE__, buf[17]-'0', buf[19]-'0');
			printk(KERN_ERR "[cmd is delayed] %d, param = %d, %d\n", __LINE__, buf[17]-'0', buf[19]-'0');
				ts->delayed_cmd_param[0] = buf[17]-'0';
			if (ts->delayed_cmd_param[0] > 1)
				ts->delayed_cmd_param[1] = buf[19]-'0';
				schedule_delayed_work(&ts->cover_cmd_work, msecs_to_jiffies(10));
		}
		return -EBUSY;
	}
	else if (ts->reinit_done == false) {
		//tsp_debug_err(true, &info->client->dev, "ft_cmd: reinit is working\n");
		printk(KERN_ERR "fts_cmd: reinit is working\n");
		if (strncmp("clear_cover_mode", buf, 16) == 0) {
				cancel_delayed_work(&ts->cover_cmd_work);
			//tsp_debug_err(true, &info->client->dev,
			//"[cmd is delayed] %d, param = %d, %d\n", __LINE__, buf[17]-'0', buf[19]-'0');
			printk(KERN_ERR "[cmd is delayed] %d, param = %d, %d\n", __LINE__, buf[17]-'0', buf[19]-'0');
				ts->delayed_cmd_param[0] = buf[17]-'0';
			if (ts->delayed_cmd_param[0] > 1)
				ts->delayed_cmd_param[1] = buf[19]-'0';
			if(ts->delayed_cmd_param[0] == 0)
				schedule_delayed_work(&ts->cover_cmd_work, msecs_to_jiffies(300));
		}
	}
#endif
	mutex_lock(&ts->cmd_lock);
	ts->cmd_is_running = true;
	mutex_unlock(&ts->cmd_lock);

	ts->cmd_state = CMD_STATUS_RUNNING;

	length = (int)count;
	if (*(buf + length - 1) == '\n')
		length--;

	memset(ts->cmd, 0x00, sizeof(ts->cmd));
	memcpy(ts->cmd, buf, length);
	memset(ts->cmd_param, 0, sizeof(ts->cmd_param));

	memset(buffer, 0x00, sizeof(buffer));
	pos = strchr(buf, (int)delim);
	if (pos)
		memcpy(buffer, buf, pos - buf);
	else
		memcpy(buffer, buf, length);

	/* find command */
	list_for_each_entry(ft_cmd_ptr, &ts->cmd_list_head, list) {
		if (!strcmp(buffer, ft_cmd_ptr->cmd_name)) {
			cmd_found = true;
			break;
		}
	}

	/* set not_support_cmd */
	if (!cmd_found) {
		list_for_each_entry(ft_cmd_ptr,
			&ts->cmd_list_head, list) {
			if (!strcmp("not_support_cmd", ft_cmd_ptr->cmd_name))
				break;
		}
	}

	/* parsing parameters */
	if (cmd_found && pos) {
		pos++;
		start = pos;
		do {
			if ((*pos == delim) || (pos - buf == length)) {
				end = pos;
				memset(buffer, 0x00, sizeof(buffer));
				memcpy(buffer, start, end - start);
				*(buffer + strlen(buffer)) = '\0';
				param = ts->cmd_param + param_cnt;
				if (kstrtoint(buffer, 10, param) < 0)
					break;
				param_cnt++;
				start = pos + 1;
			}
			pos++;
		} while ((pos - buf <= length) && (param_cnt < CMD_PARAM_NUM));
	}

	printk(KERN_ERR "%s: Command = %s\n", __func__, buf);
	
	ft_cmd_ptr->cmd_func(ts);

	return count;
}

static ssize_t cmd_status_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct sec_ts_data *ts = dev_get_drvdata(dev);
	char buffer[16];

	printk(KERN_ERR "%s: Command status = %d\n", __func__, ts->cmd_state);

	switch (ts->cmd_state) {
	case CMD_STATUS_WAITING:
		sprintf(buffer, "%s", tostring(WAITING));
		break;
	case CMD_STATUS_RUNNING:
		sprintf(buffer, "%s", tostring(RUNNING));
		break;
	case CMD_STATUS_OK:
		sprintf(buffer, "%s", tostring(OK));
		break;
	case CMD_STATUS_FAIL:
		sprintf(buffer, "%s", tostring(FAIL));
		break;
	case CMD_STATUS_NOT_APPLICABLE:
		sprintf(buffer, "%s", tostring(NOT_APPLICABLE));
		break;
	default:
		sprintf(buffer, "%s", tostring(NOT_APPLICABLE));
		break;
	}

	return snprintf(buf, PAGE_SIZE, "%s\n", buffer);
}

static ssize_t cmd_result_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct sec_ts_data *ts = dev_get_drvdata(dev);

	tsp_debug_info(true, &ts->client->dev, "%s: Command result = %s\n", __func__, ts->cmd_result);

	mutex_lock(&ts->cmd_lock);
	ts->cmd_is_running = false;
	mutex_unlock(&ts->cmd_lock);

	ts->cmd_state = CMD_STATUS_WAITING;

	return snprintf(buf, PAGE_SIZE, "%s\n", ts->cmd_result);
}

static ssize_t cmd_list_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	int ii = 0;
	char buffer[CMD_RESULT_STR_LEN];
	char buffer_name[CMD_STR_LEN];

	snprintf(buffer, 30, "++factory command list++\n");
	while (strncmp(ft_cmds[ii].cmd_name, "not_support_cmd", 16) != 0) {
		snprintf(buffer_name, CMD_STR_LEN, "%s\n", ft_cmds[ii].cmd_name);
		strcat(buffer, buffer_name);
		ii++;
	}

	return snprintf(buf, PAGE_SIZE, "%s\n", buffer);

}

static void fw_update(void *device_data)
{
	struct sec_ts_data *ts = (struct sec_ts_data *)device_data;
	char buff[64] = { 0 };
	int retval = 0;

	set_default_result(ts);
	if (ts->touch_stopped) {
		tsp_debug_info(true, &ts->client->dev, "%s: [ERROR] Touch is stopped\n",
			 __func__);
		snprintf(buff, sizeof(buff), "%s", "TSP turned off");
		set_cmd_result(ts, buff, strnlen(buff, sizeof(buff)));
		ts->cmd_state = CMD_STATUS_NOT_APPLICABLE;
		return;
	}

	retval = sec_ts_firmware_update_on_hidden_menu(ts, ts->cmd_param[0]);
	if (retval < 0) {
		sprintf(buff, "%s", "NA");
		set_cmd_result(ts, buff, strnlen(buff, sizeof(buff)));
		ts->cmd_state = CMD_STATUS_FAIL;
		tsp_debug_info(true, &ts->client->dev, "%s: failed [%d]\n", __func__, retval);
	} else {
		sprintf(buff, "%s", "OK");
		set_cmd_result(ts, buff, strnlen(buff, sizeof(buff)));
		ts->cmd_state = CMD_STATUS_OK;
		tsp_debug_info(true, &ts->client->dev, "%s: success [%d]\n", __func__, retval);
	}
	return;
}

static int sec_ts_fix_tmode(struct sec_ts_data *ts, u8 mode, u8 state)
{
	int ret;
	u8 onoff[1] = {STATE_MANAGE_OFF};
	u8 tBuff[2] = { mode, state };

	ret = ts->sec_ts_i2c_write(ts, SEC_TS_CMD_STATEMANAGE_ON, onoff, 1);

	ret = ts->sec_ts_i2c_write(ts, SEC_TS_CMD_CHG_SYSMODE , tBuff, sizeof(tBuff));

	return ret;
}

static int sec_ts_release_tmode(struct sec_ts_data *ts)
{
	int ret;
	u8 onoff[1] = {STATE_MANAGE_ON};

	ret = ts->sec_ts_i2c_write(ts, SEC_TS_CMD_STATEMANAGE_ON, onoff, 1);

	return ret;
}

void sec_ts_print_frame(struct sec_ts_data *ts, short *min, short *max)
{
	int i = 0;
	int j = 0;
	unsigned char *pStr = NULL;
	unsigned char pTmp[16] = { 0 };

	pStr = kzalloc(6 * (ts->rx_count + 1), GFP_KERNEL);
	if (pStr == NULL) {
		tsp_debug_info(true, &ts->client->dev, "SEC_TS pStr kzalloc failed\n");
		return;
	}

	memset(pStr, 0x0, 6 * (ts->rx_count + 1));
	snprintf(pTmp, sizeof(pTmp), "    ");
	strncat(pStr, pTmp, 6 * ts->rx_count);

	for (i = 0; i < ts->rx_count; i++) {
		snprintf(pTmp, sizeof(pTmp), "Rx%02d  ", i);
		strncat(pStr, pTmp, 6 * ts->rx_count);
	}

	tsp_debug_info(true, &ts->client->dev, "SEC_TS %s\n", pStr);
	memset(pStr, 0x0, 6 * (ts->rx_count + 1));
	snprintf(pTmp, sizeof(pTmp), " +");
	strncat(pStr, pTmp, 6 * ts->rx_count);

	for (i = 0; i < ts->rx_count; i++) {
		snprintf(pTmp, sizeof(pTmp), "------");
		strncat(pStr, pTmp, 6 * ts->rx_count);
	}

	tsp_debug_info(true, &ts->client->dev, "SEC_TS %s\n", pStr);

	for (i = 0; i < ts->tx_count; i++) {
		memset(pStr, 0x0, 6 * (ts->rx_count + 1));
		snprintf(pTmp, sizeof(pTmp), "Tx%02d | ", i);
		strncat(pStr, pTmp, 6 * ts->rx_count);

		for (j = 0; j < ts->rx_count; j++) {
			snprintf(pTmp, sizeof(pTmp), "%5d ", ts->pFrame[(i * ts->rx_count) + j]);

			if (i > 0) {
				if (ts->pFrame[(i * ts->rx_count) + j] < *min)
					*min = ts->pFrame[(i * ts->rx_count) + j];

				if (ts->pFrame[(i * ts->rx_count) + j] > *max)
					*max = ts->pFrame[(i * ts->rx_count) + j];
			}
			strncat(pStr, pTmp, 6 * ts->rx_count);
		}
	tsp_debug_info(true, &ts->client->dev, "SEC_TS %s\n", pStr);
	}
	kfree(pStr);
}

int sec_ts_read_frame(struct sec_ts_data *ts, u8 type, short *min,
                 short *max)
{
	unsigned int readbytes = 0xFF;
	unsigned int storebytes = 0xFF;
	unsigned char *pRead = NULL;
	unsigned int dataposition = 0;
	int rc = 0;
	int ret = 0;
	int i = 0;

	//read rawdata
	readbytes = ts->rx_count * (ts->tx_count) * 2;

	pRead = kzalloc(readbytes, GFP_KERNEL);

	if (pRead == NULL) {
		tsp_debug_info(true, &ts->client->dev, "Read frame kzalloc failed\n");
		rc = 1;
		goto ErrorExit;
	}

	//Select rawdata type
	ret = ts->sec_ts_i2c_write(ts, 0xf4, &type, 1); //SEC_TS_CMD_MUTU_RAWDATA_TYPE 
	if (ret < 0) {
		tsp_debug_info(true, &ts->client->dev, "Set rawdata type failed\n");
		rc = 2;
		goto ErrorExit;
	}

	sec_ts_delay(10);	
	ret = ts->sec_ts_i2c_read(ts, SEC_TS_READ_TOUCH_RAWDATA, pRead, readbytes);
	if (ret < 0) {
		tsp_debug_err(true, &ts->client->dev, "%s: read rawdata failed!\n", __func__);
		rc = 3;
		goto ErrorExit;
	}

	storebytes = ts->rx_count * ts->tx_count * 2;
	for (i = 0; i < storebytes; i += 2)
		ts->pFrame[dataposition++] = pRead[i+1] + (pRead[i] << 8);

#ifdef DEBUG_MSG
	tsp_debug_info(true, &ts->client->dev, "SEC_TS 02X%02X%02X readbytes=%d, storebytes=%d\n",
	pRead[0], pRead[1], pRead[2], readbytes, storebytes);
#endif
	kfree(pRead);

	sec_ts_print_frame(ts, min, max);

	ErrorExit:
	return rc;
}

static void get_fw_ver_bin(void *device_data)
{
	struct sec_ts_data *ts = (struct sec_ts_data *)device_data;
	char buff[16] = { 0 };
	int ret;
	u8 fw_ver[4];

	set_default_result(ts);

	ret = ts->sec_ts_i2c_read(ts, SEC_TS_READ_FW_VERSION, fw_ver, 4);
	if(ret < 0) {
		tsp_debug_info(true, &ts->client->dev, "%s: firmware version read error\n ", __func__);
		return;
	}
	if (strncmp(ts->plat_data->model_name, "G920", 4) == 0)
		sprintf(buff, "SECTS%01X%01X", fw_ver[2], fw_ver[3]);
	else
		tsp_debug_info(true, &ts->client->dev, "%s: Check model name[%s]!\n",
			__func__, ts->plat_data->model_name);

	set_cmd_result(ts, buff, strnlen(buff, sizeof(buff)));
	ts->cmd_state = CMD_STATUS_OK;
	tsp_debug_info(true, &ts->client->dev, "%s: %s\n", __func__, buff);
}

static void get_fw_ver_ic(void *device_data)
{
	struct sec_ts_data *ts = (struct sec_ts_data *)device_data;
	char buff[16] = { 0 };
	int ret;
	u8 fw_ver[4];

	set_default_result(ts);

	ret = ts->sec_ts_i2c_read(ts, SEC_TS_READ_FW_VERSION, fw_ver, 4);
	if (ret < 0)	{
		tsp_debug_info(true, &ts->client->dev, "%s: firmware version read error\n ", __func__);
		return;
	}

	if (strncmp(ts->plat_data->model_name, "G920", 4) == 0)
		sprintf(buff, "SECTS%01X%01X", fw_ver[0], fw_ver[1]);
	else
		tsp_debug_info(true, &ts->client->dev, "%s: Check model name[%s]!\n",
			__func__, ts->plat_data->model_name);

	set_cmd_result(ts, buff, strnlen(buff, sizeof(buff)));
	ts->cmd_state = CMD_STATUS_OK;
	tsp_debug_info(true, &ts->client->dev, "%s: %s\n", __func__, buff);
}

static void get_config_ver(void *device_data)
{
	struct sec_ts_data *ts = (struct sec_ts_data *)device_data;
	char buff[20] = { 0 };
	int ret;
	u8 para_ver[4];

	ret = ts->sec_ts_i2c_read(ts, SEC_TS_READ_PARA_VERSION, para_ver, 4);
	if (ret < 0) {
		tsp_debug_info(true, &ts->client->dev, "%s: parameter version read error\n ", __func__);
		return;
	}

	snprintf(buff, sizeof(buff), "%s_%X%X%X%X",
		ts->plat_data->model_name ?: ts->plat_data->project_name ?: SEC_TS_DEVICE_NAME,
		para_ver[0], para_ver[1], para_ver[2], para_ver[3]);

	set_default_result(ts);
	set_cmd_result(ts, buff, strnlen(buff, sizeof(buff)));
	ts->cmd_state = CMD_STATUS_OK;
	tsp_debug_info(true, &ts->client->dev, "%s: %s\n", __func__, buff);
}

static void module_off_master(void *device_data)
{
	struct sec_ts_data *ts = (struct sec_ts_data *)device_data;
	char buff[3] = { 0 };
	int ret = 0;

	mutex_lock(&ts->lock);
	if (ts->enabled) {
		disable_irq(ts->irq);
		ts->enabled = false;
	}
	mutex_unlock(&ts->lock);

	if (ts->plat_data->power)
		ts->plat_data->power(ts, false);
	else
		ret = 1;

	if (ret == 0)
		snprintf(buff, sizeof(buff), "%s", "OK");
	else
		snprintf(buff, sizeof(buff), "%s", "NG");

    set_default_result(ts);
    set_cmd_result(ts, buff, strnlen(buff, sizeof(buff)));
    if (strncmp(buff, "OK", 2) == 0)
    	ts->cmd_state = CMD_STATUS_OK;
    else
    	ts->cmd_state = CMD_STATUS_FAIL;
    tsp_debug_info(true, &ts->client->dev, "%s: %s\n", __func__, buff);
}

static void module_on_master(void *device_data)
{
	struct sec_ts_data *ts = (struct sec_ts_data *)device_data;
    char buff[3] = { 0 };
    int ret = 0;

    mutex_lock(&ts->lock);
    if (!ts->enabled) {
		enable_irq(ts->irq);
		ts->enabled = true;
    }
    mutex_unlock(&ts->lock);

    if (ts->plat_data->power)
    	ts->plat_data->power(ts, true);
    else
		ret = 1;

    if (ret == 0)
		snprintf(buff, sizeof(buff), "%s", "OK");
    else
		snprintf(buff, sizeof(buff), "%s", "NG");

	set_default_result(ts);
	set_cmd_result(ts, buff, strnlen(buff, sizeof(buff)));
	if (strncmp(buff, "OK", 2) == 0)
		ts->cmd_state = CMD_STATUS_OK;
	else
		ts->cmd_state = CMD_STATUS_FAIL;

	tsp_debug_info(true, &ts->client->dev, "%s: %s\n", __func__, buff);
}

static void get_chip_vendor(void *device_data)
{
	struct sec_ts_data *ts = (struct sec_ts_data *)device_data;
	char buff[16] = { 0 };
	strncpy(buff, "SEC", sizeof(buff));
	set_default_result(ts);
	set_cmd_result(ts, buff, strnlen(buff, sizeof(buff)));
	ts->cmd_state = CMD_STATUS_OK;
	tsp_debug_info(true, &ts->client->dev, "%s: %s\n", __func__, buff);
}

static void get_chip_name(void *device_data)
{
	struct sec_ts_data *ts = (struct sec_ts_data *)device_data;
	char buff[16] = { 0 };
	char chipid[3] = { 0 };
	int ret;

	ret = ts->sec_ts_i2c_read(ts, SEC_TS_READ_DEVICE_ID, chipid, 3 );
	snprintf(buff, sizeof(buff), "%X%X%X", chipid[0], chipid[1], chipid[2]);

	set_default_result(ts);
	set_cmd_result(ts, buff, strnlen(buff, sizeof(buff)));
	ts->cmd_state = CMD_STATUS_OK;
	tsp_debug_info(true, &ts->client->dev, "%s: %s\n", __func__, buff);
}

static void get_x_num(void *device_data)
{
	struct sec_ts_data *ts = (struct sec_ts_data *)device_data;
	char buff[16] = { 0 };

	set_default_result(ts);
	snprintf(buff, sizeof(buff), "%d", ts->rx_count);
	set_cmd_result(ts, buff, strnlen(buff, sizeof(buff)));
	ts->cmd_state = 2;
	tsp_debug_info(true, &ts->client->dev, "%s: %s\n", __func__, buff);
}

static void get_y_num(void *device_data)
{
	struct sec_ts_data *ts = (struct sec_ts_data *)device_data;
	char buff[16] = { 0 };

	set_default_result(ts);
	snprintf(buff, sizeof(buff), "%d", ts->tx_count);
	set_cmd_result(ts, buff, strnlen(buff, sizeof(buff)));
	ts->cmd_state = CMD_STATUS_OK;
	tsp_debug_info(true, &ts->client->dev, "%s: %s\n", __func__, buff);
}

static void run_reference_read(void *device_data)
{
	struct sec_ts_data *ts = (struct sec_ts_data *)device_data;
	char buff[CMD_STR_LEN] = { 0 };
	short min = 0x7FFF;
	short max = 0x8000;

	set_default_result(ts);
	if (ts->touch_stopped) {
		char buff[CMD_STR_LEN] = { 0 };
		tsp_debug_info(true, &ts->client->dev, "%s: [ERROR] Touch is stopped\n",
		__func__);
		snprintf(buff, sizeof(buff), "%s", "TSP turned off");
		set_cmd_result(ts, buff, strnlen(buff, sizeof(buff)));
		ts->cmd_state = CMD_STATUS_NOT_APPLICABLE;
		return;
	}

	sec_ts_fix_tmode(ts, TOUCH_MODE_TOUCH, TOUCH_STATE_TOUCH);

	sec_ts_read_frame(ts, TYPE_REMV_AMB_DATA, &min, &max);
	snprintf(buff, sizeof(buff), "%d,%d", min, max);
	set_cmd_result(ts, buff, strnlen(buff, sizeof(buff)));
	ts->cmd_state = CMD_STATUS_OK;
	tsp_debug_info(true, &ts->client->dev, "%s: %s\n", __func__, buff);

	sec_ts_release_tmode(ts);
}

static void get_reference(void *device_data)
{
	struct sec_ts_data *ts = (struct sec_ts_data *)device_data;
	char buff[CMD_STR_LEN] = { 0 };
	short val = 0;
	int node = 0;

	set_default_result(ts);
	if (ts->touch_stopped) {
		tsp_debug_info(true, &ts->client->dev, "%s: [ERROR] Touch is stopped\n",
		__func__);
		snprintf(buff, sizeof(buff), "%s", "TSP turned off");
		set_cmd_result(ts, buff, strnlen(buff, sizeof(buff)));
		ts->cmd_state = CMD_STATUS_NOT_APPLICABLE;
		return;
	}

	node = sec_ts_check_index(ts);
	if (node < 0)
		return;

	val = ts->pFrame[node];
	snprintf(buff, sizeof(buff), "%d", val);
	set_cmd_result(ts, buff, strnlen(buff, sizeof(buff)));
	ts->cmd_state = CMD_STATUS_OK;
	tsp_debug_info(true, &ts->client->dev, "%s: %s\n", __func__, buff);
}

static void run_rawcap_read(void *device_data)
{
	struct sec_ts_data *ts = (struct sec_ts_data *)device_data;
	char buff[CMD_STR_LEN] = { 0 };
	short min = 0x7FFF;
	short max = 0x8000;
	int ret;

	set_default_result(ts);

	if (ts->touch_stopped) {
		tsp_debug_info(true, &ts->client->dev, "%s: [ERROR] Touch is stopped\n",
		__func__);
		snprintf(buff, sizeof(buff), "%s", "TSP turned off");
		set_cmd_result(ts, buff, strnlen(buff, sizeof(buff)));
		ts->cmd_state = CMD_STATUS_NOT_APPLICABLE;
		return;
	}

	sec_ts_fix_tmode(ts, TOUCH_MODE_TOUCH, TOUCH_STATE_TOUCH);

	ret = sec_ts_read_frame(ts, TYPE_RAW_DATA, &min, &max);
	snprintf(buff, sizeof(buff), "%d,%d", min, max);
	set_cmd_result(ts, buff, strnlen(buff, sizeof(buff)));
	ts->cmd_state = CMD_STATUS_OK;
	tsp_debug_info(true, &ts->client->dev, "%s: %s\n", __func__, buff);

	sec_ts_release_tmode(ts);
}

static void get_rawcap(void *device_data)
{
	struct sec_ts_data *ts = (struct sec_ts_data *)device_data;
	char buff[CMD_STR_LEN] = { 0 };
	short val = 0;
	int node = 0;

	set_default_result(ts);
	if (ts->touch_stopped) {
		tsp_debug_info(true, &ts->client->dev, "%s: [ERROR] Touch is stopped\n",
		__func__);
		snprintf(buff, sizeof(buff), "%s", "TSP turned off");
		set_cmd_result(ts, buff, strnlen(buff, sizeof(buff)));
		ts->cmd_state = CMD_STATUS_NOT_APPLICABLE;
		return;
	}

	node = sec_ts_check_index(ts);
	if (node < 0)
		return;

	val = ts->pFrame[node];
	snprintf(buff, sizeof(buff), "%d", val);
	set_cmd_result(ts, buff, strnlen(buff, sizeof(buff)));
	ts->cmd_state = CMD_STATUS_OK;
	tsp_debug_info(true, &ts->client->dev, "%s: %s\n", __func__, buff);
}

static void run_delta_read(void *device_data)
{
	struct sec_ts_data *ts = (struct sec_ts_data *)device_data;
	char buff[CMD_STR_LEN] = { 0 };
	short min = 0x7FFF;
	short max = 0x8000;
	int ret;

	set_default_result(ts);

	if (ts->touch_stopped) {
		tsp_debug_info(true, &ts->client->dev, "%s: [ERROR] Touch is stopped\n",
		__func__);
		snprintf(buff, sizeof(buff), "%s", "TSP turned off");
		set_cmd_result(ts, buff, strnlen(buff, sizeof(buff)));
		ts->cmd_state = CMD_STATUS_NOT_APPLICABLE;
		return;
	}

	sec_ts_fix_tmode(ts, TOUCH_MODE_TOUCH, TOUCH_STATE_TOUCH);

	ret = sec_ts_read_frame(ts, TYPE_SIGNAL_DATA, &min, &max);
	snprintf(buff, sizeof(buff), "%d,%d", min, max);
	set_cmd_result(ts, buff, strnlen(buff, sizeof(buff)));
	ts->cmd_state = CMD_STATUS_OK;
	tsp_debug_info(true, &ts->client->dev, "%s: %s\n", __func__, buff);

	sec_ts_release_tmode(ts);
}

static void get_delta(void *device_data)
{
	struct sec_ts_data *ts = (struct sec_ts_data *)device_data;
	char buff[CMD_STR_LEN] = { 0 };
	short val = 0;
	int node = 0;

	set_default_result(ts);

	if (ts->touch_stopped) {
		tsp_debug_info(true, &ts->client->dev, "%s: [ERROR] Touch is stopped\n",
		__func__);
		snprintf(buff, sizeof(buff), "%s", "TSP turned off");
		set_cmd_result(ts, buff, strnlen(buff, sizeof(buff)));
		ts->cmd_state = CMD_STATUS_NOT_APPLICABLE;
		return;
	}

	node = sec_ts_check_index(ts);
	if (node < 0)
		return;

	val = ts->pFrame[node];
	snprintf(buff, sizeof(buff), "%d", val);
	set_cmd_result(ts, buff, strnlen(buff, sizeof(buff)));
	ts->cmd_state = CMD_STATUS_OK;
	tsp_debug_info(true, &ts->client->dev, "%s: %s\n", __func__, buff);
}

#define GLOVE_MODE_EN (1 << 0)
#define CLEAR_COVER_EN (1 << 1)
#define FAST_GLOVE_MODE_EN (1 << 2)

static void glove_mode(void *device_data)
{
	struct sec_ts_data *ts = (struct sec_ts_data *)device_data;
	int glove_mode_enables = 0;
	set_default_result(ts);

	if (ts->cmd_param[0] < 0 || ts->cmd_param[0] > 1) {
		snprintf(ts->cmd_buff, sizeof(ts->cmd_buff), "NG");
		ts->cmd_state = CMD_STATUS_FAIL;
	} else {
		int retval;

		if (ts->cmd_param[0])
			glove_mode_enables |= GLOVE_MODE_EN;
		else
			glove_mode_enables &= ~(GLOVE_MODE_EN);

		retval = sec_ts_glove_mode_enables(ts, glove_mode_enables);

		if (retval < 0) {
			tsp_debug_err(true, &ts->client->dev, "%s failed, retval = %d\n", __func__, retval);
			snprintf(ts->cmd_buff, sizeof(ts->cmd_buff), "NG");
			ts->cmd_state = CMD_STATUS_FAIL;
		} else {
			snprintf(ts->cmd_buff, sizeof(ts->cmd_buff), "OK");
			ts->cmd_state = CMD_STATUS_OK;
		}
	}

	set_cmd_result(ts, ts->cmd_buff, strlen(ts->cmd_buff));

	mutex_lock(&ts->cmd_lock);
	ts->cmd_is_running = false;
	mutex_unlock(&ts->cmd_lock);

	ts->cmd_state = CMD_STATUS_WAITING;

	return;
}

static void hover_enable(void *device_data)
{
	int enables;
	int retval;
	struct sec_ts_data *ts = (struct sec_ts_data *)device_data;

	tsp_debug_info(true, &ts->client->dev, "%s: enter hover enable, param = %d\n", __func__, ts->cmd_param[0]);

	set_default_result(ts);

	if (ts->cmd_param[0] < 0 || ts->cmd_param[0] > 1) {
		snprintf(ts->cmd_buff, sizeof(ts->cmd_buff), "NG");
		ts->cmd_state = CMD_STATUS_FAIL;
	} else {
		enables = ts->cmd_param[0];
		retval = sec_ts_hover_enables(ts, enables);

		if (retval < 0) {
			printk(KERN_ERR "%s failed, retval = %d\n", __func__, retval);
			snprintf(ts->cmd_buff, sizeof(ts->cmd_buff), "NG");
			ts->cmd_state = CMD_STATUS_FAIL;
		} else {
			snprintf(ts->cmd_buff, sizeof(ts->cmd_buff), "OK");
			ts->cmd_state = CMD_STATUS_OK;
		}
	}

	set_cmd_result(ts, ts->cmd_buff, strlen(ts->cmd_buff));
	mutex_lock(&ts->cmd_lock);
	ts->cmd_is_running = false;
	mutex_unlock(&ts->cmd_lock);

	ts->cmd_state = CMD_STATUS_WAITING;

	return;
}

static void clear_cover_mode(void *device_data)
{
	struct sec_ts_data *ts = (struct sec_ts_data *)device_data;
	char buff[CMD_STR_LEN] = { 0 };

	tsp_debug_info(true, &ts->client->dev, "%s: start clear_cover_mode %s\n", __func__, buff);
	set_default_result(ts);

	if (ts->cmd_param[0] < 0 || ts->cmd_param[0] > 3) {
		snprintf(buff, sizeof(buff), "%s", "NG");
		ts->cmd_state = CMD_STATUS_FAIL;
	} else {
		if (ts->cmd_param[0] > 1) {
			ts->flip_enable = true;
			ts->cover_type = ts->cmd_param[1];
		} else {
			ts->flip_enable = false;
		}

		if (!ts->touch_stopped && ts->reinit_done) {
			if (ts->flip_enable)
				sec_ts_set_cover_type(ts, true);
			else
				sec_ts_set_cover_type(ts, false);
		}

		snprintf(buff, sizeof(buff), "%s", "OK");
		ts->cmd_state = CMD_STATUS_OK;
	}
	set_cmd_result(ts, buff, strnlen(buff, sizeof(buff)));
	ts->cmd_state = CMD_STATUS_WAITING;

	mutex_lock(&ts->cmd_lock);
	ts->cmd_is_running = false;
	mutex_unlock(&ts->cmd_lock);

	tsp_debug_info(true, &ts->client->dev, "%s: %s\n", __func__, buff);
};
#endif

#ifdef SMARTCOVER_COVER
void change_smartcover_table(struct sec_ts_data *ts)
{
	struct factory_data *info = ts->f_data;
	u8 i, j, k, h, temp, temp_sum;

	for (i = 0; i < MAX_BYTE ; i++)
		for (j = 0; j < MAX_TX; j++)
			info->changed_table[j][i] = info->smart_cover[i][j];

#if 1 // debug
	printk(KERN_ERR "%s smart_cover value\n", __func__);
	//tsp_debug_info(true, &info->client->dev, "%s smart_cover value\n", __func__);
	for (i = 0; i < MAX_BYTE; i++){
		pr_cont("[sec_ts] ");
		for (j = 0; j < MAX_TX; j++)
			pr_cont("%d ",info->smart_cover[i][j]);
		pr_cont("\n");
	}

	//tsp_debug_info(true, &info->client->dev, "%s changed_table value\n", __func__);
	printk(KERN_INFO "%s changed_table value\n", __func__);
	for (j = 0; j < MAX_TX; j++){
		pr_cont("[sec_ts] ");
		for (i = 0; i < MAX_BYTE; i++)
			pr_cont("%d ",info->changed_table[j][i]);
		pr_cont("\n");
	}
#endif

	//tsp_debug_info(true, &info->client->dev, "%s %d\n", __func__, __LINE__);
	printk(KERN_INFO "%s %d\n", __func__, __LINE__);

	for (i = 0; i < MAX_TX; i++)
		for (j = 0; j < 4; j++)
			info->send_table[i][j] = 0;
	//tsp_debug_info(true, &info->client->dev, "%s %d\n", __func__, __LINE__);
	printk(KERN_INFO "%s %d\n", __func__, __LINE__);

	for (i = 0; i < MAX_TX; i++){
		temp = 0;
		for (j = 0; j < MAX_BYTE; j++)
			temp += info->changed_table[i][j];
		if (temp == 0) continue;

		for(k = 0; k < 4; k++) {
			temp_sum = 0;
			for(h = 0; h < 8; h++)
				temp_sum += ((u8)(info->changed_table[i][h+8*k])) << (7-h);

			info->send_table[i][k] = temp_sum;
		}

		//tsp_debug_info(true, &info->client->dev, "i:%2d, %2X %2X %2X %2X \n",
		//i,info->send_table[i][0],info->send_table[i][1],info->send_table[i][2],info->send_table[i][3]);
		printk(KERN_INFO "i:%2d, %2X %2X %2X %2X \n", \
		i,info->send_table[i][0],info->send_table[i][1],info->send_table[i][2],info->send_table[i][3]);
	}
	//tsp_debug_info(true, &info->client->dev, "%s %d\n", __func__, __LINE__);
	printk(KERN_INFO "%s %d\n", __func__, __LINE__);

}
void set_smartcover_mode(struct sec_ts_data *ts, bool on)
{
	struct factory_data *info = ts->f_data;
	int ret;
	unsigned char regMon[2] = {0xC1, 0x0A};
	unsigned char regMoff[2] = {0xC2, 0x0A};

	if (on == 1){
		//ts->sec_ts_i2c_write(info,);
		ret = fts_write_reg(info, regMon, 2);
		if (ret < 0)
			//tsp_debug_err(true, &info->client->dev, "%s mode on failed. ret: %d\n", __func__, ret);
			printk(KERN_ERR "%s mode on failed. ret: %d\n", __func__, ret);
	} else {
		ret = fts_write_reg(info, regMoff, 2);
		if (ret < 0)
			//tsp_debug_err(true, &info->client->dev, "%s mode off failed. ret: %d\n", __func__, ret);
			printk(KERN_ERR "%s mode off failed. ret: %d\n", __func__, ret);
	}
}
void set_smartcover_clear(struct sec_ts_ts *ts)
{
	//struct factory_ts *info = ts->f_ts;
	int ret;
	unsigned char regClr[6] = {0xC5, 0xFF, 0x00, 0x00, 0x00, 0x00};

	ret = fts_write_reg(ts, regClr, 6);
	if (ret < 0)
		//tsp_debug_err(true, &info->client->dev, "%s ts clear failed. ret: %d\n", __func__, ret);
		printk(KERN_ERR "%s ts clear failed. ret: %d\n", __func__, ret);
}

void set_smartcover_data(struct sec_ts_data *ts)
{
	struct factory_data *info = ts->f_data;
	int ret;
	u8 i, j;
	u8 temp=0;
	unsigned char regData[6] = {0xC5, 0x00, 0x00, 0x00, 0x00, 0x00};


	for (i = 0; i < MAX_TX; i++) {
		temp = 0;
		for (j = 0; j < 4; j++)
			temp += info->send_table[i][j];
		if (temp == 0) continue;

		regData[1] = i;

		for (j = 0; j < 4; j++)
			regData[2+j] = info->send_table[i][j];

		//tsp_debug_info(true, &info->client->dev, "i:%2d, %2X %2X %2X %2X \n",
		//regData[1],regData[2],regData[3],regData[4], regData[5]);
		printk(KERN_INFO "i:%2d, %2X %2X %2X %2X \n", \
		regData[1],regData[2],regData[3],regData[4], regData[5]);

		// data write
		ret = fts_write_reg(info, regData, 6);
		if (ret < 0)
			//tsp_debug_err(true, &info->client->dev, "%s data write[%d] failed. ret: %d\n", __func__,i, ret);
			printk(KERN_ERR "%s data write[%d] failed. ret: %d\n", __func__,i, ret);
	}
}

/* ####################################################
        func : smartcover_cmd [0] [1] [2] [3]
        index 0
                vlaue 0 : off (normal)
                vlaue 1 : off (globe mode)
                vlaue 2 :  X
                vlaue 3 : on
                                clear -> data send(send_table value) ->  mode on
                vlaue 4 : clear smart_cover value
                vlaue 5 : data save to smart_cover value
                        index 1 : tx channel num
                        index 2 : data 0xFF
                        index 3 : data 0xFF
                value 6 : table value change, smart_cover -> changed_table -> send_table

        ex)
        // clear
        echo smartcover_cmd,4 > cmd
        // data write (hart)
        echo smartcover_cmd,5,3,16,16 > cmd
        echo smartcover_cmd,5,4,56,56 > cmd
        echo smartcover_cmd,5,5,124,124 > cmd
        echo smartcover_cmd,5,6,126,252 > cmd
        echo smartcover_cmd,5,7,127,252 > cmd
        echo smartcover_cmd,5,8,63,248 > cmd
        echo smartcover_cmd,5,9,31,240 > cmd
        echo smartcover_cmd,5,10,15,224 > cmd
        echo smartcover_cmd,5,11,7,192 > cmd
        echo smartcover_cmd,5,12,3,128 > cmd
        // data change
        echo smartcover_cmd,6 > cmd
        // mode on
        echo smartcover_cmd,3 > cmd

###################################################### */
void smartcover_cmd(void *device_data)
{
	//struct fts_ts_info *info = (struct fts_ts_info *)device_data;
	struct sec_ts_data *ts = (struct sec_ts_data *)device_data;
	struct factory_data *info = ts->f_data;
	int retval;
	char buff[CMD_STR_LEN] = { 0 };
	u8 i, j, t;

	set_default_result(info);

	if (info->cmd_param[0] < 0 || info->cmd_param[0] > 6) {
		snprintf(buff, sizeof(buff), "%s", "NG");
		info->cmd_state = CMD_STATUS_FAIL;
	} else {
		if (info->cmd_param[0] == 0) {                      // off
			set_smartcover_mode(ts, 0);
			//tsp_debug_info(true, &info->client->dev, "%s mode off, normal\n", __func__);
			printk(KERN_INFO "%s mode off, normal\n", __func__);
		} else if(info->cmd_param[0] == 1) {       // off, globe mode
			set_smartcover_mode(ts, 0);
			//tsp_debug_info(true, &info->client->dev, "%s mode off, globe mode\n", __func__);
			printk(KERN_INFO "%s mode off, globe mode\n", __func__);

			if(ts->glove_enables & (0x1 << 3)) {//SEC_TS_BIT_SETFUNC_GLOVE )
				retval = sec_ts_glove_mode_enables(ts, 1);
				if (retval < 0) {
					printk(KERN_ERR "%s failed, retval = %d\n", __func__, retval);
					snprintf(info->cmd_buff, sizeof(info->cmd_buff), "NG");
					info->cmd_state = CMD_STATUS_FAIL;
				} else {
					snprintf(info->cmd_buff, sizeof(info->cmd_buff), "OK");
					info->cmd_state = CMD_STATUS_OK;
				}
			} else if (ts->glove_enables & (0x1 << 1)) {//SEC_TS_BIT_SETFUNC_HOVER )
				retval = sec_ts_hover_enables(ts, 1);
				if (retval < 0)	{
					printk(KERN_ERR "%s failed, retval = %d\n", __func__, retval);
					snprintf(info->cmd_buff, sizeof(info->cmd_buff), "NG");
					info->cmd_state = CMD_STATUS_FAIL;
				} else {
					snprintf(info->cmd_buff, sizeof(info->cmd_buff), "OK");
					info->cmd_state = CMD_STATUS_OK;
				}
			}
			/*
			if (info->fast_mshover_enabled)
			fts_command(info, FTS_CMD_SET_FAST_GLOVE_MODE);
			else if (info->mshover_enabled)
			fts_command(info, FTS_CMD_MSHOVER_ON);
			*/
		} else if (info->cmd_param[0] == 3) {       // on
			set_smartcover_clear(ts);
			set_smartcover_data(ts);
			//tsp_debug_info(true, &info->client->dev, "%s data send\n", __func__);
			printk(KERN_INFO "%s data send\n", __func__);
			set_smartcover_mode(ts, 1);
			//tsp_debug_info(true, &info->client->dev, "%s mode on\n", __func__);
			printk(KERN_INFO "%s mode on\n", __func__);

		} else if (info->cmd_param[0] == 4) {       // clear
			for (i = 0; i < MAX_BYTE; i++)
				for (j = 0; j < MAX_TX; j++)
					info->smart_cover[i][j] = 0;
			//tsp_debug_info(true, &info->client->dev, "%s data clear\n", __func__);
			printk(KERN_INFO "%s data clear\n", __func__);
		} else if(info->cmd_param[0] == 5) {       // data write
			if (info->cmd_param[1] < 0 ||  info->cmd_param[1] >= 32) {
			//tsp_debug_info(true, &info->client->dev, "%s data tx size is over[%d]\n",
			//__func__,info->cmd_param[1]);
				printk(KERN_INFO "%s data tx size is over[%d]\n", \
				__func__,info->cmd_param[1]);
				snprintf(buff, sizeof(buff), "%s", "NG");
				info->cmd_state = CMD_STATUS_FAIL;
				goto fail;
			}
			//tsp_debug_info(true, &info->client->dev, "%s data %2X, %2X, %2X\n", __func__, 
			//info->cmd_param[1],info->cmd_param[2],info->cmd_param[3] );
			printk(KERN_INFO "%s data %2X, %2X, %2X\n", __func__, \
			info->cmd_param[1],info->cmd_param[2],info->cmd_param[3] );

			t = info->cmd_param[1];
			info->smart_cover[t][0] = (info->cmd_param[2]&0x80)>>7;
			info->smart_cover[t][1] = (info->cmd_param[2]&0x40)>>6;
			info->smart_cover[t][2] = (info->cmd_param[2]&0x20)>>5;
			info->smart_cover[t][3] = (info->cmd_param[2]&0x10)>>4;
			info->smart_cover[t][4] = (info->cmd_param[2]&0x08)>>3;
			info->smart_cover[t][5] = (info->cmd_param[2]&0x04)>>2;
			info->smart_cover[t][6] = (info->cmd_param[2]&0x02)>>1;
			info->smart_cover[t][7] = (info->cmd_param[2]&0x01);
			info->smart_cover[t][8] = (info->cmd_param[3]&0x80)>>7;
			info->smart_cover[t][9] = (info->cmd_param[3]&0x40)>>6;
			info->smart_cover[t][10] = (info->cmd_param[3]&0x20)>>5;
			info->smart_cover[t][11] = (info->cmd_param[3]&0x10)>>4;
			info->smart_cover[t][12] = (info->cmd_param[3]&0x08)>>3;
			info->smart_cover[t][13] = (info->cmd_param[3]&0x04)>>2;
			info->smart_cover[t][14] = (info->cmd_param[3]&0x02)>>1;
			info->smart_cover[t][15] = (info->cmd_param[3]&0x01);

		} else if (info->cmd_param[0] == 6) {         // data change
			change_smartcover_table(ts);
			//tsp_debug_info(true, &info->client->dev, "%s data change\n", __func__);
			printk(KERN_INFO "%s data change\n", __func__);
		} else {
			//tsp_debug_info(true, &info->client->dev, "%s cmd[%d] not use\n", __func__, info->cmd_param[0] );
			printk(KERN_INFO "%s cmd[%d] not use\n", __func__, info->cmd_param[0]);
			snprintf(buff, sizeof(buff), "%s", "NG");
			info->cmd_state = CMD_STATUS_FAIL;
			goto fail;
		}

		snprintf(buff, sizeof(buff), "%s", "OK");
		info->cmd_state = CMD_STATUS_OK;
	}
fail:
	set_cmd_result(info, buff, strnlen(buff, sizeof(buff)));

	mutex_lock(&info->cmd_lock);
	info->cmd_is_running = false;
	mutex_unlock(&info->cmd_lock);
	info->cmd_state = CMD_STATUS_WAITING;

	//tsp_debug_info(true, &info->client->dev, "%s: %s\n", __func__, buff);
	printk(KERN_INFO "%s: %s\n", __func__, buff);
};
#endif

static void not_support_cmd(void *device_data)
{
	struct sec_ts_data *ts = (struct sec_ts_data *)device_data;

	set_default_result(ts);
	sprintf(ts->cmd_buff, "%s", tostring(NA));

	set_cmd_result(ts, ts->cmd_buff, strlen(ts->cmd_buff));
	ts->cmd_state = CMD_STATUS_NOT_APPLICABLE;
}

int sec_ts_fn_init(struct sec_ts_data *ts)
{
	int retval;
	unsigned short ii;

	INIT_LIST_HEAD(&ts->cmd_list_head);
	for (ii = 0; ii < ARRAY_SIZE(ft_cmds); ii++)
		list_add_tail(&ft_cmds[ii].list, &ts->cmd_list_head);

	mutex_init(&ts->cmd_lock);
	ts->cmd_is_running = false;

	ts->fac_dev_ts = sec_device_create(ts, "tsp");

	retval = IS_ERR(ts->fac_dev_ts);
	if (retval) {
		tsp_debug_err(true, &ts->client->dev, "%s: Failed to create device for the sysfs\n", __func__);
		retval = IS_ERR(ts->fac_dev_ts);
		goto exit;
	}

	dev_set_drvdata(ts->fac_dev_ts, ts);

	retval = sysfs_create_group(&ts->fac_dev_ts->kobj,
            &cmd_attr_group);
        if (retval < 0) {
		tsp_debug_err(true, &ts->client->dev, "%s: Failed to create sysfs attributes\n", __func__);
                goto exit;
        }

	retval = sysfs_create_link(&ts->fac_dev_ts->kobj,
				&ts->input_dev->dev.kobj, "input");

	if (retval < 0)
	{
		tsp_debug_err(true, &ts->client->dev, "%s: fail - sysfs_create_link\n", __func__);
		goto exit;
	}
	ts->reinit_done = true;
	ts->touch_stopped = false;
	
	INIT_DELAYED_WORK(&ts->cover_cmd_work, clear_cover_cmd_work);

	return 0;

exit:
	//kfree(factory_data);
	return retval;
}
