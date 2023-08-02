/*
 *  Copyright (C) 2010,Imagis Technology Co. Ltd. All Rights Reserved.
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 */


#include <linux/mutex.h>
#include <linux/i2c.h>
#include <linux/delay.h>
#include <linux/slab.h>
#include <linux/input.h>
#include <linux/stat.h>
#include <linux/err.h>

#include "ist30xx.h"
#include "ist30xx_update.h"
#include "ist30xx_misc.h"
#if IST30XXC_CMCS_TEST 
#include "ist30xx_cmcs.h"
#endif
static char IsfwUpdate[20] = { 0 };

#define FW_DOWNLOADING "Downloading"
#define FW_DOWNLOAD_COMPLETE "Complete"
#define FW_DOWNLOAD_FAIL "FAIL"

#define FACTORY_BUF_SIZE    (2048)
#define BUILT_IN            (0)
#define UMS                 (1)

#define CMD_STATE_WAITING   (0)
#define CMD_STATE_RUNNING   (1)
#define CMD_STATE_OK        (2)
#define CMD_STATE_FAIL      (3)
#define CMD_STATE_NA        (4)

#define TSP_NODE_DEBUG      (0)
#define TSP_CM_DEBUG        (0)

#define TSP_CH_UNUSED       (0)
#define TSP_CH_SCREEN       (1)
#define TSP_CH_GTX          (2)
#define TSP_CH_KEY          (3)
#define TSP_CH_UNKNOWN      (-1)

#define TSP_CMD(name, func) .cmd_name = name, .cmd_func = func
struct tsp_cmd {
	struct list_head	list;
	const char *		cmd_name;
	void			(*cmd_func)(void *dev_data);
};

u32 ist30xxc_get_fw_ver(struct ist30xxc_data *data)
{
	u32 ver = 0;
	int ret = 0;

	ret = ist30xxc_read_cmd(data, eHCOM_GET_VER_FW, &ver);
	if (unlikely(ret))
    {
        ist30xxc_reset(data, false);
        ist30xxc_start(data);
		ts_info("%s(), ret=%d\n", __func__, ret);
		return ver;
    }

    ts_debug("Reg addr: %x, ver: %x\n", eHCOM_GET_VER_FW, ver);

	return ver;
}

#define KEY_SENSITIVITY_OFFSET  0x10
u32 tkey_sensitivity = 0;
int ist30xxc_get_key_sensitivity(struct ist30xxc_data *data, int id)
{
	u32 addr = IST30XXC_DA_ADDR(data->tags.algo_base) + KEY_SENSITIVITY_OFFSET;
	u32 val = 0;
    int ret = 0;

	if (unlikely(id >= data->tkey_info.key_num))
		return 0;

	if (ist30xxc_intr_wait(data, 30) < 0)
		return 0;

	ret = ist30xxc_read_cmd(data, addr, &val);
    if (ret) {
        ist30xxc_reset(data, false);
        ist30xxc_start(data);
		ts_info("%s(), ret=%d\n", __func__, ret);
        return 0;
    }

	if ((val & 0xFFF) == 0xFFF)
		return (tkey_sensitivity >> (16 * id)) & 0xFFFF;

	tkey_sensitivity = val;

	ts_debug("Reg addr: %x, val: %8x\n", addr, val);

	val >>= (16 * id);

	return (int)(val & 0xFFFF);
}


/* Factory CMD function */
static void set_default_result(struct sec_fac *sec)
{
	char delim = ':';

	memset(sec->cmd_result, 0, ARRAY_SIZE(sec->cmd_result));
	memcpy(sec->cmd_result, sec->cmd, strlen(sec->cmd));
	strncat(sec->cmd_result, &delim, CMD_STATE_RUNNING);
}

static void set_cmd_result(struct sec_fac *sec, char *buf, int len)
{
	strncat(sec->cmd_result, buf, len);
}

static void not_support_cmd(void *dev_data)
{
	char buf[16] = { 0 };

	struct ist30xxc_data *data = (struct ist30xxc_data *)dev_data;
	struct sec_fac *sec = (struct sec_fac *)&data->sec;

	set_default_result(sec);
	snprintf(buf, sizeof(buf), "%s", "NA");
	ts_info("%s(), %s\n", __func__, buf);

	set_cmd_result(sec, buf, strnlen(buf, sizeof(buf)));
	sec->cmd_state = CMD_STATE_NA;
	dev_info(&data->client->dev, "%s: \"%s(%d)\"\n", __func__,
		 buf, strnlen(buf, sizeof(buf)));
	return;
}

static void get_chip_vendor(void *dev_data)
{
	char buf[16] = { 0 };

	struct ist30xxc_data *data = (struct ist30xxc_data *)dev_data;
	struct sec_fac *sec = (struct sec_fac *)&data->sec;

	set_default_result(sec);
	snprintf(buf, sizeof(buf), "%s", TSP_CHIP_VENDOR);
	ts_info("%s(), %s\n", __func__, buf);

	set_cmd_result(sec, buf, strnlen(buf, sizeof(buf)));
	sec->cmd_state = CMD_STATE_OK;
	dev_info(&data->client->dev, "%s: %s(%d)\n", __func__,
		 buf, strnlen(buf, sizeof(buf)));
}

static void get_chip_name(void *dev_data)
{
	char buf[16] = { 0 };

	struct ist30xxc_data *data = (struct ist30xxc_data *)dev_data;
	struct sec_fac *sec = (struct sec_fac *)&data->sec;

	set_default_result(sec);
	snprintf(buf, sizeof(buf), "%s", TS_CHIP_NAME);
	ts_info("%s(), %s\n", __func__, buf);

	set_cmd_result(sec, buf, strnlen(buf, sizeof(buf)));
	sec->cmd_state = CMD_STATE_OK;
	dev_info(&data->client->dev, "%s: %s(%d)\n", __func__,
		 buf, strnlen(buf, sizeof(buf)));
}

static void get_chip_id(void *dev_data)
{
	char buf[16] = { 0 };

	struct ist30xxc_data *data = (struct ist30xxc_data *)dev_data;
	struct sec_fac *sec = (struct sec_fac *)&data->sec;

	set_default_result(sec);
	snprintf(buf, sizeof(buf), "%#02x", data->chip_id);
	ts_info("%s(), %s\n", __func__, buf);

	set_cmd_result(sec, buf, strnlen(buf, sizeof(buf)));
	sec->cmd_state = CMD_STATE_OK;
	dev_info(&data->client->dev, "%s: %s(%d)\n", __func__,
		 buf, strnlen(buf, sizeof(buf)));
}
#include <linux/uaccess.h>
#define MAX_FW_PATH 255
static void fw_update(void *dev_data)
{
	int ret;
	char buf[16] = { 0 };
	mm_segment_t old_fs = {0};
	struct file *fp = NULL;
	long fsize = 0, nread = 0;
	u8 *fw;
	char fw_path[MAX_FW_PATH+1];

	struct ist30xxc_data *data = (struct ist30xxc_data *)dev_data;
	struct sec_fac *sec = (struct sec_fac *)&data->sec;

	set_default_result(sec);

	ts_info("%s(), %d\n", __func__, sec->cmd_param[0]);

	switch (sec->cmd_param[0]) {
	case BUILT_IN:
		sec->cmd_state = CMD_STATE_OK;
		ret = ist30xxc_fw_recovery(data);
		if (ret < 0)
			sec->cmd_state = CMD_STATE_FAIL;
		break;
	case UMS:
		sec->cmd_state = CMD_STATE_OK;
		old_fs = get_fs();
		set_fs(get_ds());

		snprintf(fw_path, MAX_FW_PATH, "/sdcard/%s", IST30XXC_FW_NAME);
		fp = filp_open(fw_path, O_RDONLY, 0);
		if (IS_ERR(fp)) {
			ts_warn("%s(), file %s open error:%d\n", __func__, 
                    fw_path, (s32)fp);
			sec->cmd_state= CMD_STATE_FAIL;
			set_fs(old_fs);
			break;
		}

		fsize = fp->f_path.dentry->d_inode->i_size;
		if (fsize != data->fw.buf_size) {
			ts_warn("%s(), invalid fw size!!\n", __func__);
			sec->cmd_state = CMD_STATE_FAIL;
			set_fs(old_fs);
			break;
		}
		fw = kzalloc((size_t)fsize, GFP_KERNEL);
		if (!fw) {
			ts_warn("%s(), failed to alloc buffer for fw\n", __func__);
			sec->cmd_state = CMD_STATE_FAIL;
			filp_close(fp, NULL);
			set_fs(old_fs);
			break;
		}
		nread = vfs_read(fp, (char __user *)fw, fsize, &fp->f_pos);
		if (nread != fsize) {
			ts_warn("%s(), failed to read fw\n", __func__);
			sec->cmd_state = CMD_STATE_FAIL;
			filp_close(fp, NULL);
			set_fs(old_fs);
			break;
		}

		filp_close(fp, current->files);
		set_fs(old_fs);
		ts_info("%s(), ums fw is loaded!!\n", __func__);

		ret = ist30xxc_get_update_info(data, fw, fsize);
        if (ret) {
            sec->cmd_state = CMD_STATE_FAIL;
            break;
        }
		data->fw.bin.core_ver = ist30xxc_parse_ver(data, FLAG_CORE, fw);
		data->fw.bin.fw_ver = ist30xxc_parse_ver(data, FLAG_FW, fw);
		data->fw.bin.test_ver = ist30xxc_parse_ver(data, FLAG_TEST, fw);

		mutex_lock(&ist30xxc_mutex);
		ret = ist30xxc_fw_update(data, fw, fsize);
        if (ret) {
            sec->cmd_state = CMD_STATE_FAIL;
            break;
        }

		mutex_unlock(&ist30xxc_mutex);

		ist30xxc_calibrate(data, 1);
		break;

	default:
		ts_warn("%s(), Invalid fw file type!\n", __func__);
		break;
	}

	if (sec->cmd_state == CMD_STATE_OK)
		snprintf(buf, sizeof(buf), "%s", "OK");
	else
		snprintf(buf, sizeof(buf), "%s", "NG");

	set_cmd_result(sec, buf, strnlen(buf, sizeof(buf)));
}


static void get_fw_ver_bin(void *dev_data)
{
	u32 ver = 0;
	char buf[16] = { 0 };

	struct ist30xxc_data *data = (struct ist30xxc_data *)dev_data;
	struct sec_fac *sec = (struct sec_fac *)&data->sec;

	set_default_result(sec);

	ver = ist30xxc_parse_ver(data, FLAG_FW, data->fw.buf);
	snprintf(buf, sizeof(buf), "IM00%04x", ver);
	ts_info("%s(), %s\n", __func__, buf);

	set_cmd_result(sec, buf, strnlen(buf, sizeof(buf)));
	sec->cmd_state = CMD_STATE_OK;
	dev_info(&data->client->dev, "%s: %s(%d)\n", __func__,
		 buf, strnlen(buf, sizeof(buf)));
}

static void get_fw_ver_ic(void *dev_data)
{
	u32 ver = 0;
	char msg[8];
	char buf[16] = { 0 };

	struct ist30xxc_data *data = (struct ist30xxc_data *)dev_data;
	struct sec_fac *sec = (struct sec_fac *)&data->sec;

	set_default_result(sec);

	if (data->status.power == 1)
		ver = ist30xxc_get_fw_ver(data);

	snprintf(buf, sizeof(buf), "IM00%04x", ver & 0xFFFF);

	if (data->fw.cur.test_ver > 0) {
		sprintf(msg, "(T%d)", data->fw.cur.test_ver);
		strcat(buf, msg);
	}

	ts_info("%s(), %s\n", __func__, buf);

	set_cmd_result(sec, buf, strnlen(buf, sizeof(buf)));
	sec->cmd_state = CMD_STATE_OK;
	dev_info(&data->client->dev, "%s: %s(%d)\n", __func__,
		 buf, strnlen(buf, sizeof(buf)));
}

static void get_threshold(void *dev_data)
{
	char buf[16] = { 0 };
	int threshold;

	struct ist30xxc_data *data = (struct ist30xxc_data *)dev_data;
	struct sec_fac *sec = (struct sec_fac *)&data->sec;

	set_default_result(sec);

    threshold = (int)(data->tsp_info.threshold);

	snprintf(buf, sizeof(buf), "%d", threshold);
	ts_info("%s(), %s\n", __func__, buf);

	set_cmd_result(sec, buf, strnlen(buf, sizeof(buf)));
	sec->cmd_state = CMD_STATE_OK;
	dev_info(&data->client->dev, "%s: %s(%d)\n", __func__,
		 buf, strnlen(buf, sizeof(buf)));
}

static void get_scr_x_num(void *dev_data)
{
	int val = -1;
	char buf[16] = { 0 };

	struct ist30xxc_data *data = (struct ist30xxc_data *)dev_data;
	struct sec_fac *sec = (struct sec_fac *)&data->sec;

	set_default_result(sec);

    if (data->tsp_info.dir.swap_xy)
        val = data->tsp_info.screen.tx;
    else
        val = data->tsp_info.screen.rx;

	if (val >= 0) {
		snprintf(buf, sizeof(buf), "%u", val);
		sec->cmd_state = CMD_STATE_OK;
		dev_info(&data->client->dev, "%s: %s(%d)\n", __func__, buf,
			 strnlen(buf, sizeof(buf)));
	} else {
		snprintf(buf, sizeof(buf), "%s", "NG");
		sec->cmd_state = CMD_STATE_FAIL;
		dev_info(&data->client->dev,
			 "%s: fail to read num of x (%d).\n", __func__, val);
	}
	set_cmd_result(sec, buf, strnlen(buf, sizeof(buf)));
	ts_info("%s(), %s\n", __func__, buf);
}

static void get_scr_y_num(void *dev_data)
{
	int val = -1;
	char buf[16] = { 0 };

	struct ist30xxc_data *data = (struct ist30xxc_data *)dev_data;
	struct sec_fac *sec = (struct sec_fac *)&data->sec;

	set_default_result(sec);

    if (data->tsp_info.dir.swap_xy)
        val = data->tsp_info.screen.rx;
    else
        val = data->tsp_info.screen.tx;

	if (val >= 0) {
		snprintf(buf, sizeof(buf), "%u", val);
		sec->cmd_state = CMD_STATE_OK;
		dev_info(&data->client->dev, "%s: %s(%d)\n", __func__, buf,
			 strnlen(buf, sizeof(buf)));
	} else {
		snprintf(buf, sizeof(buf), "%s", "NG");
		sec->cmd_state = CMD_STATE_FAIL;
		dev_info(&data->client->dev,
			 "%s: fail to read num of y (%d).\n", __func__, val);
	}
	set_cmd_result(sec, buf, strnlen(buf, sizeof(buf)));
	ts_info("%s(), %s\n", __func__, buf);
}

static void get_all_x_num(void *dev_data)
{
	int val = - 1;
	char buf[16] = { 0 };

	struct ist30xxc_data *data = (struct ist30xxc_data *)dev_data;
	struct sec_fac *sec = (struct sec_fac *)&data->sec;

	set_default_result(sec);

    if (data->tsp_info.dir.swap_xy)
        val = data->tsp_info.ch_num.tx;
    else
        val = data->tsp_info.ch_num.rx;

	if (val >= 0) {
		snprintf(buf, sizeof(buf), "%u", val);
		sec->cmd_state = CMD_STATE_OK;
		dev_info(&data->client->dev, "%s: %s(%d)\n", __func__, buf,
			 strnlen(buf, sizeof(buf)));
	} else {
		snprintf(buf, sizeof(buf), "%s", "NG");
		sec->cmd_state = CMD_STATE_FAIL;
		dev_info(&data->client->dev,
			 "%s: fail to read all num of x (%d).\n", __func__, val);
	}
	set_cmd_result(sec, buf, strnlen(buf, sizeof(buf)));
	ts_info("%s(), %s\n", __func__, buf);
}

static void get_all_y_num(void *dev_data)
{
	int val = -1;
	char buf[16] = { 0 };

	struct ist30xxc_data *data = (struct ist30xxc_data *)dev_data;
	struct sec_fac *sec = (struct sec_fac *)&data->sec;

	set_default_result(sec);

    if (data->tsp_info.dir.swap_xy)
        val = data->tsp_info.ch_num.rx;
    else
        val = data->tsp_info.ch_num.tx;

	if (val >= 0) {
		snprintf(buf, sizeof(buf), "%u", val);
		sec->cmd_state = CMD_STATE_OK;
		dev_info(&data->client->dev, "%s: %s(%d)\n", __func__, buf,
			 strnlen(buf, sizeof(buf)));
	} else {
		snprintf(buf, sizeof(buf), "%s", "NG");
		sec->cmd_state = CMD_STATE_FAIL;
		dev_info(&data->client->dev,
			 "%s: fail to read all num of y (%d).\n", __func__, val);
	}
	set_cmd_result(sec, buf, strnlen(buf, sizeof(buf)));
	ts_info("%s(), %s\n", __func__, buf);
}

int chk_tsp_channel(void *dev_data, int width, int height)
{
	int node = -EPERM;

	struct ist30xxc_data *data = (struct ist30xxc_data *)dev_data;
	struct sec_fac *sec = (struct sec_fac *)&data->sec;

	if ((sec->cmd_param[0] < 0) || (sec->cmd_param[0] >= width) ||
	    (sec->cmd_param[1] < 0) || (sec->cmd_param[1] >= height)) {
		ts_info("%s: parameter error: %u,%u\n",
			 __func__, sec->cmd_param[0], sec->cmd_param[1]);
	} else {
		node = sec->cmd_param[0] + sec->cmd_param[1] * width;
		ts_info("%s: node = %d\n", __func__, node);
	}

	return node;
}

static u16 node_value[IST30XXC_NODE_TOTAL_NUM];
static u16 key_node_value[IST30XXC_MAX_KEYS];
void get_raw_array(void *dev_data)
{
	int i, ret;
    int count = 0;
	char *buf;
    const int msg_len = 16;
	char msg[msg_len];
	u8 flag = NODE_FLAG_RAW;

	struct ist30xxc_data *data = (struct ist30xxc_data *)dev_data;
	struct sec_fac *sec = (struct sec_fac *)&data->sec;
	TSP_INFO *tsp = &data->tsp_info;

	set_default_result(sec);

   	mutex_lock(&ist30xxc_mutex);
	ret = ist30xxc_read_touch_node(data, flag, &tsp->node);
	if (ret) {
       	mutex_unlock(&ist30xxc_mutex);
		sec->cmd_state = CMD_STATE_FAIL;
		ts_warn("%s(), tsp node read fail!\n", __func__);
		return;
	}
    mutex_unlock(&ist30xxc_mutex);
	ist30xxc_parse_touch_node(data, flag, &tsp->node);
    
    ret = parse_ts_node(data, flag, &tsp->node, node_value, TS_RAW_ALL);
	if (ret) {
		sec->cmd_state = CMD_STATE_FAIL;
		ts_warn("%s(), tsp node parse fail - flag: %d\n", __func__, flag);
		return;
	}

    buf = kmalloc(IST30XXC_NODE_TOTAL_NUM * 5, GFP_KERNEL);
    memset(buf, 0, sizeof(buf));

	for (i = 0; i < tsp->ch_num.rx * tsp->ch_num.tx; i++) {
#if TSP_NODE_DEBUG
		if ((i % tsp->ch_num.rx) == 0)
			printk("\n%s %4d: ", IST30XXC_DEBUG_TAG, i);

		printk("%4d ", node_value[i]);
#endif
        count += snprintf(msg, msg_len, "%d,", node_value[i]);
    	strncat(buf, msg, msg_len);
	}

    printk("\n");
    ts_info("%s: %d\n", __func__, count - 1);
	sec->cmd_state = CMD_STATE_OK;
	set_cmd_result(sec, buf, count - 1);
	dev_info(&data->client->dev, "%s: %s(%d)\n", __func__, buf, count);
    kfree(buf);
}

void run_raw_read(void *dev_data)
{
	int i, j;
    int ret = 0;
	int min_val, max_val;
	char buf[16] = { 0 };
    int type, idx;
	u8 flag = NODE_FLAG_RAW;

	struct ist30xxc_data *data = (struct ist30xxc_data *)dev_data;
	struct sec_fac *sec = (struct sec_fac *)&data->sec;
	TSP_INFO *tsp = &data->tsp_info;

	set_default_result(sec);

	ret = ist30xxc_read_touch_node(data, flag, &tsp->node);
	if (ret) {
		sec->cmd_state = CMD_STATE_FAIL;
		ts_warn("%s(), tsp node read fail!\n", __func__);
		return;
	}
	ist30xxc_parse_touch_node(data, flag, &tsp->node);

	ret = parse_ts_node(data, flag, &tsp->node, node_value, TS_RAW_SCREEN);
	if (ret) {
		sec->cmd_state = CMD_STATE_FAIL;
		ts_warn("%s(), tsp node parse fail - flag: %d\n", __func__, flag);
		return;
	}

    min_val = max_val = node_value[0];
	for (i = 0; i < tsp->screen.rx * tsp->screen.tx; i++) {
#if TSP_NODE_DEBUG
		if ((i % tsp->screen.rx) == 0)
			printk("\n%s %4d: ", IST30XXC_DEBUG_TAG, i);

		printk("%4d ", node_value[i]);
#endif        

		max_val = max(max_val, (int)node_value[i]);
		min_val = min(min_val, (int)node_value[i]);
	}

	snprintf(buf, sizeof(buf), "%d,%d", min_val, max_val);
	ts_info("%s(), %s\n", __func__, buf);

	sec->cmd_state = CMD_STATE_OK;
	set_cmd_result(sec, buf, strnlen(buf, sizeof(buf)));
	dev_info(&data->client->dev, "%s: %s(%d)\n", __func__, buf,
		 strnlen(buf, sizeof(buf)));
}

void run_raw_read_keys(void *dev_data)
{
	int i, j;
    int ret = 0;
	char buf[16] = { 0 };
    int type, idx;
	u8 flag = NODE_FLAG_RAW;

	struct ist30xxc_data *data = (struct ist30xxc_data *)dev_data;
	struct sec_fac *sec = (struct sec_fac *)&data->sec;
	TSP_INFO *tsp = &data->tsp_info;

	set_default_result(sec);

	ret = ist30xxc_read_touch_node(data, flag, &tsp->node);
	if (ret) {
		sec->cmd_state = CMD_STATE_FAIL;
		ts_warn("%s(), tsp node read fail!\n", __func__);
		return;
	}
	ist30xxc_parse_touch_node(data, flag, &tsp->node);

	ret = parse_ts_node(data, flag, &tsp->node, key_node_value, TS_RAW_KEY);
	if (ret) {
		sec->cmd_state = CMD_STATE_FAIL;
		ts_warn("%s(), tsp node parse fail - flag: %d\n", __func__, flag);
		return;
	}

	snprintf(buf, sizeof(buf), "%d,%d", key_node_value[0], key_node_value[1]);
	ts_info("%s(), %s\n", __func__, buf);

	sec->cmd_state = CMD_STATE_OK;
	set_cmd_result(sec, buf, strnlen(buf, sizeof(buf)));
	dev_info(&data->client->dev, "%s: %s(%d)\n", __func__, buf,
		 strnlen(buf, sizeof(buf)));
}

void get_raw_value(void *dev_data)
{
	int idx = 0;
	char buf[16] = { 0 };

	struct ist30xxc_data *data = (struct ist30xxc_data *)dev_data;
	struct sec_fac *sec = (struct sec_fac *)&data->sec;
    TSP_INFO *tsp = &data->tsp_info;

	set_default_result(sec);

	idx = chk_tsp_channel(data, tsp->screen.rx, tsp->screen.tx);
	if (idx < 0) { // Parameter parsing fail
		snprintf(buf, sizeof(buf), "%s", "NG");
		sec->cmd_state = CMD_STATE_FAIL;
	} else {
		snprintf(buf, sizeof(buf), "%d", node_value[idx]);
		sec->cmd_state = CMD_STATE_OK;
	}

	set_cmd_result(sec, buf, strnlen(buf, sizeof(buf)));
	ts_info("%s(), [%d][%d]: %s\n", __func__,
		 sec->cmd_param[0], sec->cmd_param[1], buf);
	dev_info(&data->client->dev, "%s: %s(%d)\n", __func__, buf,
		 strnlen(buf, sizeof(buf)));
}

#if IST30XXC_CMCS_TEST
extern u8 *tsp_cmcs_bin;
extern u32 tsp_cmcs_bin_size;
extern CMCS_BIN_INFO *tsp_cmcs;
extern CMCS_BUF *tsp_cmcs_buf;
void run_ts_cm_test(void *dev_data)
{
    int i, j;
    int ret = 0;
    char buf[16] = { 0 };
    int type, idx;
	int min_val, max_val;

    struct ist30xxc_data *data = (struct ist30xxc_data *)dev_data;
	struct sec_fac *sec = (struct sec_fac *)&data->sec;
    TSP_INFO *tsp = &data->tsp_info;

    set_default_result(sec);

	if ((tsp_cmcs_bin == NULL) || (tsp_cmcs_bin_size == 0)) {
       	sec->cmd_state = CMD_STATE_FAIL;
		ts_warn("%s(), Binary is not correct!\n", __func__);
		return;
	}

	ret = ist30xxc_get_cmcs_info(tsp_cmcs_bin, tsp_cmcs_bin_size);
    if (unlikely(ret)) {
        sec->cmd_state = CMD_STATE_FAIL;
		ts_warn("%s(), tsp get cmcs infomation fail!\n", __func__);
		return;
	}

	mutex_lock(&ist30xxc_mutex);
	ret = ist30xxc_cmcs_test(data, tsp_cmcs_bin, tsp_cmcs_bin_size);
    if (unlikely(ret)) {
		mutex_unlock(&ist30xxc_mutex);
        sec->cmd_state = CMD_STATE_FAIL;
		ts_warn("%s(), tsp cmcs test fail!\n", __func__);
		return;
	}        
	mutex_unlock(&ist30xxc_mutex);

    min_val = max_val = tsp_cmcs_buf->cm[0];
	for (i = 0; i < tsp->ch_num.tx; i++) {
#if TSP_CM_DEBUG
        printk("%s", IST30XXC_DEBUG_TAG);
#endif        
		for (j = 0; j < tsp->ch_num.rx; j++) {
            idx = (i * tsp->ch_num.rx) + j;
			type = check_node_type(data, i, j);

			if ((type == TSP_CH_SCREEN) || (type == TSP_CH_GTX)) {
       			max_val = max(max_val, (int)tsp_cmcs_buf->cm[idx]);
    		    min_val = min(min_val, (int)tsp_cmcs_buf->cm[idx]);
            }
#if TSP_CM_DEBUG            
            printk("%5d ", tsp_cmcs_buf->cm[idx]);
#endif            
		}
#if TSP_CM_DEBUG        
        printk("\n");
#endif        
	}

	snprintf(buf, sizeof(buf), "%d,%d", min_val, max_val);
	ts_info("%s(), %s\n", __func__, buf);

	sec->cmd_state = CMD_STATE_OK;
	set_cmd_result(sec, buf, strnlen(buf, sizeof(buf)));
	dev_info(&data->client->dev, "%s: %s(%d)\n", __func__, buf,
		 strnlen(buf, sizeof(buf)));
}

void run_ts_cm_test_key(void *dev_data)
{
    int i, j;
    int ret = 0;
    char buf[16] = { 0 };
    int type, idx;
    int cm_key[IST30XXC_MAX_KEYS] = { 0 };
    int key_count = 0;

    struct ist30xxc_data *data = (struct ist30xxc_data *)dev_data;
	struct sec_fac *sec = (struct sec_fac *)&data->sec;
    TSP_INFO *tsp = &data->tsp_info;

    set_default_result(sec);

	if ((tsp_cmcs_bin == NULL) || (tsp_cmcs_bin_size == 0)) {
       	sec->cmd_state = CMD_STATE_FAIL;
		ts_warn("%s(), Binary is not correct!\n", __func__);
		return;
	}

	ret = ist30xxc_get_cmcs_info(tsp_cmcs_bin, tsp_cmcs_bin_size);
    if (unlikely(ret)) {
        sec->cmd_state = CMD_STATE_FAIL;
		ts_warn("%s(), tsp get cmcs infomation fail!\n", __func__);
		return;
	}

	mutex_lock(&ist30xxc_mutex);
	ret = ist30xxc_cmcs_test(data, tsp_cmcs_bin, tsp_cmcs_bin_size);
    if (unlikely(ret)) {
		mutex_unlock(&ist30xxc_mutex);
        sec->cmd_state = CMD_STATE_FAIL;
		ts_warn("%s(), tsp cmcs test fail!\n", __func__);
		return;
	}        
	mutex_unlock(&ist30xxc_mutex);

	for (i = 0; i < tsp->ch_num.tx; i++) {
#if TSP_CM_DEBUG
        printk("%s", IST30XXC_DEBUG_TAG);
#endif        
		for (j = 0; j < tsp->ch_num.rx; j++) {
            idx = (i * tsp->ch_num.rx) + j;
			type = check_node_type(data, i, j);

			if (type == TSP_CH_KEY) {
                cm_key[key_count++] = (int)tsp_cmcs_buf->cm[idx];
            }
#if TSP_CM_DEBUG            
            printk("%5d ", tsp_cmcs_buf->cm[idx]);
#endif            
		}
#if TSP_CM_DEBUG        
        printk("\n");
#endif        
	}

	snprintf(buf, sizeof(buf), "%d,%d", cm_key[0], cm_key[1]);
	ts_info("%s(), %s\n", __func__, buf);

	sec->cmd_state = CMD_STATE_OK;
	set_cmd_result(sec, buf, strnlen(buf, sizeof(buf)));
	dev_info(&data->client->dev, "%s: %s(%d)\n", __func__, buf,
		 strnlen(buf, sizeof(buf)));
}

void get_ts_cm_value(void *dev_data)
{
	int idx = 0;
	char buf[16] = { 0 };

	struct ist30xxc_data *data = (struct ist30xxc_data *)dev_data;
	struct sec_fac *sec = (struct sec_fac *)&data->sec;
    TSP_INFO *tsp = &data->tsp_info;

	set_default_result(sec);

	idx = chk_tsp_channel(data, tsp->ch_num.rx, tsp->ch_num.tx);
	if (idx < 0) { // Parameter parsing fail
		snprintf(buf, sizeof(buf), "%s", "NG");
		sec->cmd_state = CMD_STATE_FAIL;
	} else {
		snprintf(buf, sizeof(buf), "%d", tsp_cmcs_buf->cm[idx]);
		sec->cmd_state = CMD_STATE_OK;
	}

	set_cmd_result(sec, buf, strnlen(buf, sizeof(buf)));
	ts_info("%s(), [%d][%d]: %s\n", __func__,
		 sec->cmd_param[0], sec->cmd_param[1], buf);
	dev_info(&data->client->dev, "%s: %s(%d)\n", __func__, buf,
		 strnlen(buf, sizeof(buf)));
}

void run_cmcs_test(void *dev_data)
{
    int ret = 0;
    char buf[16] = { 0 };

    struct ist30xxc_data *data = (struct ist30xxc_data *)dev_data;
	struct sec_fac *sec = (struct sec_fac *)&data->sec;

    set_default_result(sec);

	if ((tsp_cmcs_bin == NULL) || (tsp_cmcs_bin_size == 0)) {
       	sec->cmd_state = CMD_STATE_FAIL;
		ts_warn("%s(), Binary is not correct!\n", __func__);
		return;
	}

	ret = ist30xxc_get_cmcs_info(tsp_cmcs_bin, tsp_cmcs_bin_size);
    if (unlikely(ret)) {
        sec->cmd_state = CMD_STATE_FAIL;
		ts_warn("%s(), tsp get cmcs infomation fail!\n", __func__);
		return;
	}

	mutex_lock(&ist30xxc_mutex);
	ret = ist30xxc_cmcs_test(data, tsp_cmcs_bin, tsp_cmcs_bin_size);
    if (unlikely(ret)) {
        mutex_unlock(&ist30xxc_mutex);
        sec->cmd_state = CMD_STATE_FAIL;
		ts_warn("%s(), tsp cmcs test fail!\n", __func__);
		return;
	}        
	mutex_unlock(&ist30xxc_mutex);

	snprintf(buf, sizeof(buf), "%s", "OK");
	ts_info("%s(), %s\n", __func__, buf);

	sec->cmd_state = CMD_STATE_OK;
	set_cmd_result(sec, buf, strnlen(buf, sizeof(buf)));
	dev_info(&data->client->dev, "%s: %s(%d)\n", __func__, buf,
		 strnlen(buf, sizeof(buf)));
}

void get_cm_array(void *dev_data)
{
    int i;
    int count = 0;
    int type;
    char *buf;
    const int msg_len = 16;
	char msg[msg_len];

	struct ist30xxc_data *data = (struct ist30xxc_data *)dev_data;
	struct sec_fac *sec = (struct sec_fac *)&data->sec;
	TSP_INFO *tsp = &data->tsp_info;

	set_default_result(sec);
   	
    buf = kmalloc(IST30XXC_NODE_TOTAL_NUM * 5, GFP_KERNEL);
    memset(buf, 0, sizeof(buf));

	for (i = 0; i < tsp->ch_num.rx * tsp->ch_num.tx; i++) {
#if TSP_CM_DEBUG
		if ((i % tsp->ch_num.rx) == 0)
			printk("\n%s %4d: ", IST30XXC_DEBUG_TAG, i);

		printk("%4d ", tsp_cmcs_buf->cm[i]);
#endif
        type = check_node_type(data, i / tsp->ch_num.rx, i % tsp->ch_num.rx);
        if ((type == TSP_CH_UNKNOWN) || (type == TSP_CH_UNUSED))
            count += snprintf(msg, msg_len, "%d,", 0);
        else
            count += snprintf(msg, msg_len, "%d,", tsp_cmcs_buf->cm[i]);
    	strncat(buf, msg, msg_len);
	}

    printk("\n");
    ts_info("%s: %d\n", __func__, count - 1);
	sec->cmd_state = CMD_STATE_OK;
	set_cmd_result(sec, buf, count - 1);
	dev_info(&data->client->dev, "%s: %s(%d)\n", __func__, buf, count);
    kfree(buf);
}

void get_slope0_array(void *dev_data)
{
    int i;
    int count = 0;
    char *buf;
    const int msg_len = 16;
	char msg[msg_len];

	struct ist30xxc_data *data = (struct ist30xxc_data *)dev_data;
	struct sec_fac *sec = (struct sec_fac *)&data->sec;
	TSP_INFO *tsp = &data->tsp_info;

	set_default_result(sec);
   	
    buf = kmalloc(IST30XXC_NODE_TOTAL_NUM * 5, GFP_KERNEL);
    memset(buf, 0, sizeof(buf));

	for (i = 0; i < tsp->ch_num.rx * tsp->ch_num.tx; i++) {
#if TSP_CM_DEBUG
		if ((i % tsp->ch_num.rx) == 0)
			printk("\n%s %4d: ", IST30XXC_DEBUG_TAG, i);

		printk("%4d ", tsp_cmcs_buf->slope0[i]);
#endif
        count += snprintf(msg, msg_len, "%d,", tsp_cmcs_buf->slope0[i]);
    	strncat(buf, msg, msg_len);
	}

    printk("\n");
    ts_info("%s: %d\n", __func__, count - 1);
	sec->cmd_state = CMD_STATE_OK;
	set_cmd_result(sec, buf, count - 1);
	dev_info(&data->client->dev, "%s: %s(%d)\n", __func__, buf, count);
    kfree(buf);
}

void get_slope1_array(void *dev_data)
{
    int i;
    int count = 0;
    char *buf;
    const int msg_len = 16;
	char msg[msg_len];

	struct ist30xxc_data *data = (struct ist30xxc_data *)dev_data;
	struct sec_fac *sec = (struct sec_fac *)&data->sec;
	TSP_INFO *tsp = &data->tsp_info;

	set_default_result(sec);
   	
    buf = kmalloc(IST30XXC_NODE_TOTAL_NUM * 5, GFP_KERNEL);
    memset(buf, 0, sizeof(buf));

	for (i = 0; i < tsp->ch_num.rx * tsp->ch_num.tx; i++) {
#if TSP_CM_DEBUG
		if ((i % tsp->ch_num.rx) == 0)
			printk("\n%s %4d: ", IST30XXC_DEBUG_TAG, i);

		printk("%4d ", tsp_cmcs_buf->slope1[i]);
#endif
        count += snprintf(msg, msg_len, "%d,", tsp_cmcs_buf->slope1[i]);
    	strncat(buf, msg, msg_len);
	}

    printk("\n");
    ts_info("%s: %d\n", __func__, count - 1);
	sec->cmd_state = CMD_STATE_OK;
	set_cmd_result(sec, buf, count - 1);
	dev_info(&data->client->dev, "%s: %s(%d)\n", __func__, buf, count);
    kfree(buf);
}

void get_cs_array(void *dev_data)
{
    int i;
    int count = 0;
    int type;
    char *buf;
    const int msg_len = 16;
	char msg[msg_len];

	struct ist30xxc_data *data = (struct ist30xxc_data *)dev_data;
	struct sec_fac *sec = (struct sec_fac *)&data->sec;
	TSP_INFO *tsp = &data->tsp_info;

	set_default_result(sec);
   	
    buf = kmalloc(IST30XXC_NODE_TOTAL_NUM * 5, GFP_KERNEL);
    memset(buf, 0, sizeof(buf));

	for (i = 0; i < tsp->ch_num.rx * tsp->ch_num.tx; i++) {
#if TSP_CM_DEBUG
		if ((i % tsp->ch_num.rx) == 0)
			printk("\n%s %4d: ", IST30XXC_DEBUG_TAG, i);

		printk("%4d ", tsp_cmcs_buf->cs0[i]);
#endif
        type = check_node_type(data, i / tsp->ch_num.rx, i % tsp->ch_num.rx);
        if ((type == TSP_CH_UNKNOWN) || (type == TSP_CH_UNUSED))
            count += snprintf(msg, msg_len, "%d,", 0);
        else
            count += snprintf(msg, msg_len, "%d,", tsp_cmcs_buf->cs[i]);
    	strncat(buf, msg, msg_len);
	}

    printk("\n");
    ts_info("%s: %d\n", __func__, count - 1);
	sec->cmd_state = CMD_STATE_OK;
	set_cmd_result(sec, buf, count - 1);
	dev_info(&data->client->dev, "%s: %s(%d)\n", __func__, buf, count);
    kfree(buf);
}
#endif
/* sysfs: /sys/class/sec/tsp/close_tsp_test */
static ssize_t show_close_tsp_test(struct device *dev,
				   struct device_attribute *attr, char *buf)
{
	return snprintf(buf, FACTORY_BUF_SIZE, "%u\n", 0);
}

/* sysfs: /sys/class/sec/tsp/cmd */
static ssize_t store_cmd(struct device *dev, struct device_attribute
			 *devattr, const char *buf, size_t count)
{
	struct ist30xxc_data *data = dev_get_drvdata(dev);
	struct sec_fac *sec = (struct sec_fac *)&data->sec;
	struct i2c_client *client = data->client;

	char *cur, *start, *end;
	char msg[SEC_CMD_STR_LEN] = { 0 };
	int len, i;
	struct tsp_cmd *tsp_cmd_ptr = NULL;
	char delim = ',';
	bool cmd_found = false;
	int param_cnt = 0;
	int ret;

	if (sec->cmd_is_running == true) {
		dev_err(&client->dev, "tsp_cmd: other cmd is running.\n");
		ts_err("tsp_cmd: other cmd is running.\n");
		goto err_out;
	}

	/* check lock  */
	mutex_lock(&sec->cmd_lock);
	sec->cmd_is_running = true;
	mutex_unlock(&sec->cmd_lock);

	sec->cmd_state = CMD_STATE_RUNNING;

	for (i = 0; i < ARRAY_SIZE(sec->cmd_param); i++)
		sec->cmd_param[i] = 0;

	len = (int)count;
	if (*(buf + len - 1) == '\n')
		len--;
	memset(sec->cmd, 0, ARRAY_SIZE(sec->cmd));
	memcpy(sec->cmd, buf, len);

	cur = strchr(buf, (int)delim);
	if (cur)
		memcpy(msg, buf, cur - buf);
	else
		memcpy(msg, buf, len);
	/* find command */
	list_for_each_entry(tsp_cmd_ptr, &sec->cmd_list_head, list) {
		if (!strcmp(msg, tsp_cmd_ptr->cmd_name)) {
			cmd_found = true;
			break;
		}
	}

	/* set not_support_cmd */
	if (!cmd_found) {
		list_for_each_entry(tsp_cmd_ptr, &sec->cmd_list_head, list) {
			if (!strcmp("not_support_cmd", tsp_cmd_ptr->cmd_name))
				break;
		}
	}

	/* parsing parameters */
	if (cur && cmd_found) {
		cur++;
		start = cur;
		memset(msg, 0, ARRAY_SIZE(msg));
		do {
			if (*cur == delim || cur - buf == len) {
				end = cur;
				memcpy(msg, start, end - start);
				*(msg + strlen(msg)) = '\0';
				ret = kstrtoint(msg, 10, \
						sec->cmd_param + param_cnt);
				start = cur + 1;
				memset(msg, 0, ARRAY_SIZE(msg));
				param_cnt++;
			}
			cur++;
		} while (cur - buf <= len);
	}
	ts_info("SEC CMD = %s\n", tsp_cmd_ptr->cmd_name);

	for (i = 0; i < param_cnt; i++)
		ts_info("SEC CMD Param %d= %d\n", i, sec->cmd_param[i]);

	tsp_cmd_ptr->cmd_func(data);

err_out:
	return count;
}

/* sysfs: /sys/class/sec/tsp/cmd_status */
static ssize_t show_cmd_status(struct device *dev,
			       struct device_attribute *devattr, char *buf)
{
	struct ist30xxc_data *data = dev_get_drvdata(dev);
	struct sec_fac *sec = (struct sec_fac *)&data->sec;
	char msg[16] = { 0 };

	dev_info(&data->client->dev, "tsp cmd: status:%d\n", sec->cmd_state);

	if (sec->cmd_state == CMD_STATE_WAITING)
		snprintf(msg, sizeof(msg), "WAITING");
	else if (sec->cmd_state == CMD_STATE_RUNNING)
		snprintf(msg, sizeof(msg), "RUNNING");
	else if (sec->cmd_state == CMD_STATE_OK)
		snprintf(msg, sizeof(msg), "OK");
	else if (sec->cmd_state == CMD_STATE_FAIL)
		snprintf(msg, sizeof(msg), "FAIL");
	else if (sec->cmd_state == CMD_STATE_NA)
		snprintf(msg, sizeof(msg), "NOT_APPLICABLE");

	return snprintf(buf, FACTORY_BUF_SIZE, "%s\n", msg);
}

/* sysfs: /sys/class/sec/tsp/cmd_result */
static ssize_t show_cmd_result(struct device *dev, struct device_attribute
			       *devattr, char *buf)
{
	struct ist30xxc_data *data = dev_get_drvdata(dev);
	struct sec_fac *sec = (struct sec_fac *)&data->sec;

	dev_info(&data->client->dev, "tsp cmd: result: %s\n", sec->cmd_result);

	mutex_lock(&sec->cmd_lock);
	sec->cmd_is_running = false;
	mutex_unlock(&sec->cmd_lock);

	sec->cmd_state = CMD_STATE_WAITING;

	return snprintf(buf, FACTORY_BUF_SIZE, "%s\n", sec->cmd_result);
}



/* sysfs: /sys/class/sec/sec_touchscreen/tsp_firm_version_phone */
static ssize_t phone_firmware_show(struct device *dev,
				   struct device_attribute *attr, char *buf)
{
    struct ist30xxc_data *data = dev_get_drvdata(dev);

	u32 ver = ist30xxc_parse_ver(data, FLAG_FW, data->fw.buf);

	ts_info("%s(), IM00%04x\n", __func__, ver);

	return sprintf(buf, "IM00%04x", ver);
}

/* sysfs: /sys/class/sec/sec_touchscreen/tsp_firm_version_panel */
static ssize_t part_firmware_show(struct device *dev,
				  struct device_attribute *attr, char *buf)
{
	u32 ver = 0;
    struct ist30xxc_data *data = dev_get_drvdata(dev);

	if (data->status.power == 1)
		ver = ist30xxc_get_fw_ver(data);

	ts_info("%s(), IM00%04x\n", __func__, ver);

	return sprintf(buf, "IM00%04x", ver);
}

/* sysfs: /sys/class/sec/sec_touchscreen/tsp_threshold */
static ssize_t threshold_firmware_show(struct device *dev,
				       struct device_attribute *attr, char *buf)
{
	int threshold;
    struct ist30xxc_data *data = dev_get_drvdata(dev);

	threshold = (int)(data->tsp_info.threshold);

	ts_info("%s(), %d\n", __func__, threshold);

	return sprintf(buf, "%d", threshold);
}

/* sysfs: /sys/class/sec/sec_touchscreen/tsp_firm_update */
static ssize_t firmware_update(struct device *dev,
			       struct device_attribute *attr, char *buf)
{
	bool ret;
    struct ist30xxc_data *data = dev_get_drvdata(dev);

	sprintf(IsfwUpdate, "%s\n", FW_DOWNLOADING);
	ts_info("%s(), %s\n", __func__, IsfwUpdate);

	ret = ist30xxc_fw_recovery(data);
	ret = (ret == 0 ? 1 : -1);

	sprintf(IsfwUpdate, "%s\n",
		(ret > 0 ? FW_DOWNLOAD_COMPLETE : FW_DOWNLOAD_FAIL));
	ts_info("%s(), %s\n", __func__, IsfwUpdate);

	return sprintf(buf, "%d", ret);
}

/* sysfs: /sys/class/sec/sec_touchscreen/tsp_firm_update_status */
static ssize_t firmware_update_status(struct device *dev,
				      struct device_attribute *attr, char *buf)
{
	ts_info("%s(), %s\n", __func__, IsfwUpdate);

	return sprintf(buf, "%s\n", IsfwUpdate);
}

/* sysfs: /sys/class/sec/sec_touchkey/touchkey_recent */
static ssize_t recent_sensitivity_show(struct device *dev,
				       struct device_attribute *attr, char *buf)
{
	int sensitivity = 0;
    struct ist30xxc_data *data = dev_get_drvdata(dev);

    sensitivity = ist30xxc_get_key_sensitivity(data, 0);

	ts_info("%s(), %d\n", __func__, sensitivity);

	return sprintf(buf, "%d", sensitivity);
}

/* sysfs: /sys/class/sec/sec_touchkey/touchkey_back */
static ssize_t back_sensitivity_show(struct device *dev,
				     struct device_attribute *attr, char *buf)
{
	int sensitivity = 0;
    struct ist30xxc_data *data = dev_get_drvdata(dev);

    sensitivity = ist30xxc_get_key_sensitivity(data, 1);

	ts_info("%s(), %d\n", __func__, sensitivity);

	return sprintf(buf, "%d", sensitivity);
}

/* sysfs: /sys/class/sec/sec_touchkey/touchkey_threshold */
static ssize_t touchkey_threshold_show(struct device *dev,
				       struct device_attribute *attr, char *buf)
{
	int threshold;
    struct ist30xxc_data *data = dev_get_drvdata(dev);

	threshold = (int)(data->tkey_info.threshold);

	ts_info("%s(), %d\n", __func__, threshold);

	return sprintf(buf, "%d", threshold);
}

struct tsp_cmd ts_cmds[] = {
	{ TSP_CMD("get_chip_vendor", get_chip_vendor), },
	{ TSP_CMD("get_chip_name",   get_chip_name),   },
	{ TSP_CMD("get_chip_id",     get_chip_id),     },
	{ TSP_CMD("fw_update",	     fw_update),       },
	{ TSP_CMD("get_fw_ver_bin",  get_fw_ver_bin),  },
	{ TSP_CMD("get_fw_ver_ic",   get_fw_ver_ic),   },
	{ TSP_CMD("get_threshold",   get_threshold),   },
	{ TSP_CMD("get_x_num",	     get_scr_x_num),   },
	{ TSP_CMD("get_y_num",	     get_scr_y_num),   },
    { TSP_CMD("get_all_x_num",	 get_all_x_num),   },
	{ TSP_CMD("get_all_y_num",	 get_all_y_num),   },
    { TSP_CMD("run_reference_read",run_raw_read),},
	{ TSP_CMD("get_reference",   get_raw_value),   },    
	{ TSP_CMD("run_raw_read",    run_raw_read),    },
    { TSP_CMD("run_raw_read_key",run_raw_read_keys),},
	{ TSP_CMD("get_raw_value",   get_raw_value),},
#if IST30XXC_CMCS_TEST
    { TSP_CMD("run_cm_test",     run_ts_cm_test),  },
    { TSP_CMD("get_cm_value",    get_ts_cm_value), },
    { TSP_CMD("run_cm_test_key", run_ts_cm_test_key),},    
#if IST30XXC_JIG_MODE
    { TSP_CMD("get_raw_array",   get_raw_array),   },
    { TSP_CMD("run_cmcs_test",   run_cmcs_test),   },
    { TSP_CMD("get_cm_array",    get_cm_array),    },
    { TSP_CMD("get_slope0_array",get_slope0_array),},
    { TSP_CMD("get_slope1_array",get_slope1_array),},
    { TSP_CMD("get_cs_array",    get_cs_array),    },
    { TSP_CMD("get_cs0_array",   get_cs_array),    },
    { TSP_CMD("get_cs1_array",   get_cs_array),    },
#endif
#endif
	{ TSP_CMD("not_support_cmd", not_support_cmd), },
};

#define SEC_DEFAULT_ATTR    (S_IRUGO | S_IWUSR | S_IWOTH | S_IXOTH)
/* sysfs - touchscreen */
static DEVICE_ATTR(tsp_firm_version_phone, S_IRUGO,
		   phone_firmware_show, NULL);
static DEVICE_ATTR(tsp_firm_version_panel, S_IRUGO,
		   part_firmware_show, NULL);
static DEVICE_ATTR(tsp_threshold, S_IRUGO,
		   threshold_firmware_show, NULL);
static DEVICE_ATTR(tsp_firm_update, S_IRUGO,
		   firmware_update, NULL);
static DEVICE_ATTR(tsp_firm_update_status, S_IRUGO,
		   firmware_update_status, NULL);

/* sysfs - touchkey */
static DEVICE_ATTR(touchkey_recent, S_IRUGO,
		   recent_sensitivity_show, NULL);
static DEVICE_ATTR(touchkey_back, S_IRUGO,
		   back_sensitivity_show, NULL);
static DEVICE_ATTR(touchkey_threshold, S_IRUGO,
		   touchkey_threshold_show, NULL);

/* sysfs - tsp */
static DEVICE_ATTR(close_tsp_test, S_IRUGO, show_close_tsp_test, NULL);
static DEVICE_ATTR(cmd, S_IWUSR | S_IWGRP, NULL, store_cmd);
static DEVICE_ATTR(cmd_status, S_IRUGO, show_cmd_status, NULL);
static DEVICE_ATTR(cmd_result, S_IRUGO, show_cmd_result, NULL);

static struct attribute *sec_tsp_attributes[] = {
	&dev_attr_tsp_firm_version_phone.attr,
	&dev_attr_tsp_firm_version_panel.attr,
	&dev_attr_tsp_threshold.attr,
	&dev_attr_tsp_firm_update.attr,
	&dev_attr_tsp_firm_update_status.attr,
	NULL,
};

static struct attribute *sec_tkey_attributes[] = {
	&dev_attr_touchkey_recent.attr,
	&dev_attr_touchkey_back.attr,
	&dev_attr_touchkey_threshold.attr,
	NULL,
};

static struct attribute *sec_touch_facotry_attributes[] = {
	&dev_attr_close_tsp_test.attr,
	&dev_attr_cmd.attr,
	&dev_attr_cmd_status.attr,
	&dev_attr_cmd_result.attr,
	NULL,
};

static struct attribute_group sec_tsp_attr_group = {
	.attrs	= sec_tsp_attributes,
};

static struct attribute_group sec_tkey_attr_group = {
	.attrs	= sec_tkey_attributes,
};

static struct attribute_group sec_touch_factory_attr_group = {
	.attrs	= sec_touch_facotry_attributes,
};

extern struct class *sec_class;
struct device *sec_ts;
struct device *sec_tkey;
struct device *sec_factory_dev;

int sec_ts_sysfs(struct ist30xxc_data *data)
{
	/* /sys/class/sec/sec_touchscreen */
	sec_ts = device_create(sec_class, NULL, 0, data,
					"sec_touchscreen");
	if (IS_ERR(sec_ts)) {
		ts_err("Failed to create device (%s)!\n", "sec_touchscreen");
		return -ENODEV;
	}
	/* /sys/class/sec/sec_touchscreen/... */
	if (sysfs_create_group(&sec_ts->kobj, &sec_tsp_attr_group))
		ts_err("Failed to create sysfs group(%s)!\n", "sec_touchscreen");

	/* /sys/class/sec/sec_touchkey */
	sec_tkey = device_create(sec_class, NULL, 0, data, "sec_touchkey");
	if (IS_ERR(sec_tkey)) {
		ts_err("Failed to create device (%s)!\n", "sec_touchkey");
		return -ENODEV;
	}
	/* /sys/class/sec/sec_touchkey/... */
	if (sysfs_create_group(&sec_tkey->kobj, &sec_tkey_attr_group))
		ts_err("Failed to create sysfs group(%s)!\n", "sec_touchkey");

	/* /sys/class/sec/tsp */
	sec_factory_dev = device_create(sec_class, NULL, 0, data, "tsp");
	if (IS_ERR(sec_factory_dev)) {
		ts_err("Failed to create device (%s)!\n", "tsp");
		return -ENODEV;
	}
	/* /sys/class/sec/tsp/... */
	if (sysfs_create_group(&sec_factory_dev->kobj, 
                &sec_touch_factory_attr_group))
		ts_err("Failed to create sysfs group(%s)!\n", "tsp");

	return 0;
}
EXPORT_SYMBOL(sec_ts_sysfs);
int sec_factory_cmd_init(struct ist30xxc_data *data)
{
	int i;
	struct sec_fac *sec = (struct sec_fac *)&data->sec;

	INIT_LIST_HEAD(&sec->cmd_list_head);
	for (i = 0; i < ARRAY_SIZE(ts_cmds); i++)
		list_add_tail(&ts_cmds[i].list, &sec->cmd_list_head);

	mutex_init(&sec->cmd_lock);
	sec->cmd_is_running = false;

	return 0;
}
EXPORT_SYMBOL(sec_factory_cmd_init);
