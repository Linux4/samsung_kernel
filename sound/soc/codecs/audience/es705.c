/*
 * es705.c  --  Audience eS705 ALSA SoC Audio driver
 *
 * Copyright 2011 Audience, Inc.
 *
 * Author: Greg Clemson <gclemson@audience.com>
 *
 * Code Updates:
 *       Genisim Tsilker <gtsilker@audience.com>
 *            - Code refactoring
 *            - FW download functions update
 *            - Add optional UART VS FW download
 *            - Add external Clock support
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#define DEBUG
#define SAMSUNG_ES705_FEATURE

#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/init.h>
#include <linux/firmware.h>
#include <linux/delay.h>
#include <linux/pm.h>
#include <linux/completion.h>
#include <linux/i2c.h>
#include <linux/slimbus/slimbus.h>
#include <linux/platform_device.h>
#include <linux/regulator/consumer.h>
#include <linux/slab.h>
#include <linux/err.h>
#include <linux/version.h>
#include <sound/core.h>
#include <sound/pcm.h>
#include <sound/pcm_params.h>
#include <sound/soc.h>
#include <sound/soc-dai.h>
#include <sound/soc-dapm.h>
#include <sound/initval.h>
#include <sound/tlv.h>
#include <linux/kthread.h>
#include <linux/device.h>
#include <linux/pm_runtime.h>
#include <linux/mutex.h>
#include <linux/slimbus/slimbus.h>
#include <linux/spi/spi.h>
#include <linux/esxxx.h>
#include <linux/wait.h>
#include "es705.h"
#include "es705-platform.h"
#include "es705-routes.h"
#include "es705-profiles.h"
#include "es705-slim.h"
#include "es705-i2c.h"
#include "es705-i2s.h"
#include "es705-spi.h"
#include "es705-cdev.h"
#include "es705-uart.h"
#include "es705-uart-common.h"

#define ES705_CMD_ACCESS_WR_MAX 2
#define ES705_CMD_ACCESS_RD_MAX 2
struct es705_api_access {
	u32 read_msg[ES705_CMD_ACCESS_RD_MAX];
	unsigned int read_msg_len;
	u32 write_msg[ES705_CMD_ACCESS_WR_MAX];
	unsigned int write_msg_len;
	unsigned int val_shift;
	unsigned int val_max;
};

#include "es705-access.h"

#define NARROW_BAND	0
#define WIDE_BAND	1
#define NETWORK_OFFSET	21
static int network_type = NARROW_BAND;

/* Route state for Internal state management */
enum es705_power_state {
ES705_POWER_FW_LOAD,
ES705_POWER_SLEEP,
ES705_POWER_SLEEP_PENDING,
ES705_POWER_AWAKE
};

static const char *power_state[] = {
	"boot",
	"sleep",
	"sleep pending",
	"awake",
};

static const char *power_state_cmd[] = {
	"not defined",
	"sleep",
	"mp_sleep",
	"mp_cmd",
	"normal",
	"vs_overlay",
	"vs_lowpwr",
};

/* codec private data TODO: move to runtime init */
struct es705_priv es705_priv = {
	.pm_state = ES705_POWER_AWAKE,

	.rx1_route_enable = 0,
	.tx1_route_enable = 0,
	.rx2_route_enable = 0,

	.vs_enable = 0,
	.vs_wakeup_keyword = 0,

	.ap_tx1_ch_cnt = 2,

	.es705_power_state = ES705_SET_POWER_STATE_NORMAL,
	.streamdev.intf = -1,
	.ns = 1,

	.wakeup_method = 0,

#if defined(SAMSUNG_ES705_FEATURE)
	.voice_wakeup_enable = 0,
	.voice_lpm_enable = 0,
	/* gpio wakeup : 0, uart wakeup : 1 */
	.use_uart_for_wakeup_gpio = 0,
	/* for tuning : 1 */
	.change_uart_config = 0,
#endif
};

const char *esxxx_mode[] = {
	"SBL",
	"STANDARD",
	"VOICESENSE",
};

struct snd_soc_dai_driver es705_dai[];

/* indexed by ES705 INTF number */
u32 es705_streaming_cmds[4] = {
	0x90250200,		/* ES705_SLIM_INTF */
	0x90250000,		/* ES705_I2C_INTF  */
	0x90250300,		/* ES705_SPI_INTF  */
	0x90250100,		/* ES705_UART_INTF */
};

int es705_write_block(struct es705_priv *es705, const u32 *cmd_block)
{
	u32 api_word;
	u8 msg[4];
	int rc = 0;

	dev_dbg(es705->dev, "%s(): pm_runtime_get_sync()\n", __func__);
	dev_dbg(es705->dev, "%s(): mutex lock\n", __func__);
	mutex_lock(&es705->api_mutex);
	while (*cmd_block != 0xffffffff) {
		api_word = cpu_to_le32(*cmd_block);
		memcpy(msg, (char *)&api_word, 4);
		es705->dev_write(es705, msg, 4);
		usleep_range(1000, 1000);
		dev_dbg(es705->dev, "%s(): msg = %02x%02x%02x%02x\n",
			__func__, msg[0], msg[1], msg[2], msg[3]);
		cmd_block++;
	}
	dev_dbg(es705->dev, "%s(): mutex unlock\n", __func__);
	mutex_unlock(&es705->api_mutex);
	dev_dbg(es705->dev, "%s(): pm_runtime_put_autosuspend()\n", __func__);

	return rc;
}



/* Note: this may only end up being called in a api locked context. In
 * that case the mutex locks need to be removed.
 */
int es705_read_vs_data_block(struct es705_priv *es705)
{
	/* This function is not re-entrant so avoid stack bloat. */
	static u8 block[ES705_VS_KEYWORD_PARAM_MAX];

	u32 cmd;
	u32 resp;
	int ret;
	unsigned size;
	unsigned rdcnt;

	mutex_lock(&es705->api_mutex);

	/* Read voice sense keyword data block request. */
	cmd = cpu_to_le32(0x802e0006);
	es705->dev_write(es705, (char *)&cmd, 4);

	usleep_range(5000, 5000);

	ret = es705->dev_read(es705, (char *)&resp, 4);
	if (ret < 0) {
		dev_dbg(es705->dev, "%s(): error sending request = %d\n",
			__func__, ret);
		goto OUT;
	}

	le32_to_cpus(resp);
	size = resp & 0xffff;
	dev_dbg(es705->dev, "%s(): resp=0x%08x size=%d\n",
		__func__, resp, size);
	if ((resp & 0xffff0000) != 0x802e0000) {
		dev_err(es705->dev, "%s(): invalid read v-s data block response = 0x%08x\n",
			__func__, resp);
		goto OUT;
	}

	BUG_ON(size == 0);
	BUG_ON(size > ES705_VS_KEYWORD_PARAM_MAX);
	BUG_ON(size % 4 != 0);

	/* This assumes we need to transfer the block in 4 byte
	 * increments. This is true on slimbus, but may not hold true
	 * for other buses.
	 */
	for (rdcnt = 0; rdcnt < size; rdcnt += 4) {
		ret = es705->dev_read(es705, (char *)&resp, 4);
		if (ret < 0) {
			dev_dbg(es705->dev, "%s(): error reading data block at %d bytes\n",
				__func__, rdcnt);
			goto OUT;
		}
		memcpy(&block[rdcnt], &resp, 4);
	}

	memcpy(es705->vs_keyword_param, block, rdcnt);
	es705->vs_keyword_param_size = rdcnt;
	dev_dbg(es705->dev, "%s(): stored v-s keyword block of %d bytes\n",
		__func__, rdcnt);

OUT:
	mutex_unlock(&es705->api_mutex);
	if (ret)
		dev_err(es705->dev, "%s(): v-s read data block failure=%d\n",
			__func__, ret);
	return ret;
}

int es705_write_vs_data_block(struct es705_priv *es705)
{
	u32 cmd;
	u32 resp;
	int ret;
	u8 *dptr;
	u16 rem;
	u8 wdb[4];

	if (es705->vs_keyword_param_size == 0) {
		dev_warn(es705->dev, "%s(): attempt to write empty keyword data block\n",
			__func__);
		return -ENOENT;
	}

	BUG_ON(es705->vs_keyword_param_size % 4 != 0);

	mutex_lock(&es705->api_mutex);

	cmd = 0x802f0000 | (es705->vs_keyword_param_size & 0xffff);
	cmd = cpu_to_le32(cmd);
	ret = es705->dev_write(es705, (char *)&cmd, 4);
	if (ret < 0) {
		dev_err(es705->dev, "%s(): error writing cmd 0x%08x to device\n",
		    __func__, cmd);
		goto EXIT;
	}

	usleep_range(10000, 10000);
	ret = es705->dev_read(es705, (char *)&resp, 4);
	if (ret < 0) {
		dev_dbg(es705->dev, "%s(): error sending request = %d\n",
			__func__, ret);
		goto EXIT;
	}

	le32_to_cpus(resp);
	dev_dbg(es705->dev, "%s(): resp=0x%08x\n", __func__, resp);
	if ((resp & 0xffff0000) != 0x802f0000) {
		dev_err(es705->dev, "%s(): invalid write data block 0x%08x\n",
			__func__, resp);
		goto EXIT;
	}

	dptr = es705->vs_keyword_param;
	for (rem = es705->vs_keyword_param_size; rem > 0; rem -= 4, dptr += 4) {
		wdb[0] = dptr[3];
		wdb[1] = dptr[2];
		wdb[2] = dptr[1];
		wdb[3] = dptr[0];
		ret = es705->dev_write(es705, (char *)wdb, 4);
		if (ret < 0) {
			dev_err(es705->dev, "%s(): v-s wdb error offset=%hu\n",
			    __func__, dptr - es705->vs_keyword_param);
			goto EXIT;
		}
	}

	usleep_range(10000, 10000);
	memset(&resp, 0, 4);

	ret = es705->dev_read(es705, (char *)&resp, 4);
	if (ret < 0) {
		dev_dbg(es705->dev, "%s(): error sending request = %d\n",
			__func__, ret);
		goto EXIT;
	}

	le32_to_cpus(resp);
	dev_dbg(es705->dev, "%s(): resp=0x%08x\n", __func__, resp);
	if (resp & 0xffff) {
		dev_err(es705->dev, "%s(): write data block error 0x%08x\n",
			__func__, resp);
		goto EXIT;
	}

	dev_info(es705->dev, "%s(): v-s wdb success\n", __func__);
EXIT:
	mutex_unlock(&es705->api_mutex);
	if (ret != 0)
		dev_err(es705->dev, "%s(): v-s wdb failed ret=%d\n",
			__func__, ret);
	return ret;
}


#ifdef FIXED_CONFIG
static void es705_fixed_config(struct es705_priv *es705)
{
	int rc;

	rc = es705_write_block(es705, es705_route_config[ROUTE_OFF].route);
}
#endif

static void es705_switch_route(long route_index)
{
	struct es705_priv *es705 = &es705_priv;
	int rc;

	if (route_index >= ROUTE_MAX) {
		dev_dbg(es705->dev, "%s(): new es705_internal_route = %ld is out of range\n",
			 __func__, route_index);
		return;
	}

	dev_dbg(es705->dev, "%s(): switch current es705_internal_route = %ld to new route = %ld\n",
		__func__, es705->internal_route_num, route_index);
	es705->internal_route_num = route_index;
	rc = es705_write_block(es705,
			  es705_route_config[es705->internal_route_num].route);
}

static void es705_switch_route_config(long route_index)
{
	struct es705_priv *es705 = &es705_priv;
	int rc;

	if (route_index >= ROUTE_MAX) {
		dev_dbg(es705->dev, "%s(): new es705_internal_route = %ld is out of range\n",
			 __func__, route_index);
		return;
	}

	es705->internal_route_num = route_index;
	if (network_type == WIDE_BAND) {
		if (es705->internal_route_num >= 0 &&
			es705->internal_route_num < 5) {
			es705->internal_route_num += NETWORK_OFFSET;
			dev_dbg(es705->dev, "%s() adjust wideband offset\n",
				__func__);
		}
	}

	if (network_type == NARROW_BAND) {
		if (es705->internal_route_num >= 0 + NETWORK_OFFSET &&
			es705->internal_route_num < 5 + NETWORK_OFFSET) {
			es705->internal_route_num -= NETWORK_OFFSET;
			dev_dbg(es705->dev, "%s() adjust narrowband offset\n",
				__func__);
		}
	}


	dev_dbg(es705->dev, "%s(): switch current es705_internal_route = %ld to new route = %ld\n",
		__func__, es705->internal_route_num, route_index);

	rc = es705_write_block(es705,
			  &es705_route_configs[es705->internal_route_num][0]);
}

/* Send a single command to the chip.
 *
 * If the SR (suppress response bit) is NOT set, will read the
 * response and cache it the driver object retrieve with es705_resp().
 *
 * Returns:
 * 0 - on success.
 * EITIMEDOUT - if the chip did not respond in within the expected time.
 * E* - any value that can be returned by the underlying HAL.
 */
int es705_cmd(struct es705_priv *es705, u32 cmd)
{
	int sr;
	int err;
	u32 resp;

	BUG_ON(!es705);
	sr = cmd & BIT(28);

	err = es705->cmd(es705, cmd, sr, &resp);
	if (err || sr)
		return err;

	if (resp == 0) {
		err = -ETIMEDOUT;
		dev_err(es705->dev, "%s(): no response to command 0x%08x\n",
			__func__, cmd);
	} else {
		es705->last_response = resp;
		get_monotonic_boottime(&es705->last_resp_time);
	}
	return err;
}

static unsigned int es705_read(struct snd_soc_codec *codec,
			       unsigned int reg)
{
	struct es705_priv *es705 = &es705_priv;
	struct es705_api_access *api_access;
	u32 api_word[2] = {0};
	char req_msg[8];
	u32 ack_msg;
	char *msg_ptr;
	unsigned int msg_len;
	unsigned int value;
	int match = 0;
	int rc;

	if (reg > ES705_API_ADDR_MAX) {
		dev_err(es705->dev, "%s(): invalid address = 0x%04x\n",
			__func__, reg);
		return -EINVAL;
	}

	api_access = &es705_api_access[reg];
	msg_len = api_access->read_msg_len;
	memcpy((char *)api_word, (char *)api_access->read_msg, msg_len);
	switch (msg_len) {
	case 8:
		cpu_to_le32s(&api_word[1]);
	case 4:
		cpu_to_le32s(&api_word[0]);
	}
	memcpy(req_msg, (char *)api_word, msg_len);

	msg_ptr = req_msg;
	mutex_lock(&es705->api_mutex);
	rc = es705->dev_write_then_read(es705, msg_ptr, msg_len,
					&ack_msg, match);
	mutex_unlock(&es705->api_mutex);
	if (rc < 0) {
		dev_err(es705->dev, "%s(): es705_xxxx_write()", __func__);
		return rc;
	}
	memcpy((char *)&api_word[0], &ack_msg, 4);
	le32_to_cpus(&api_word[0]);
	value = api_word[0] & 0xffff;
	return value;
}

static int es705_write(struct snd_soc_codec *codec, unsigned int reg,
		       unsigned int value)
{
	struct es705_priv *es705 = &es705_priv;
	struct es705_api_access *api_access;
	u32 api_word[2] = {0};
	char msg[8];
	char *msg_ptr;
	int msg_len;
	unsigned int val_mask;
	int rc = 0;

	if (reg > ES705_API_ADDR_MAX) {
		dev_err(es705->dev, "%s(): invalid address = 0x%04x\n",
			__func__, reg);
		return -EINVAL;
	}

	api_access = &es705_api_access[reg];
	msg_len = api_access->write_msg_len;
	val_mask = (1 << get_bitmask_order(api_access->val_max)) - 1;
	dev_info(es705->dev, "%s(): reg=%d val=%d msg_len = %d val_mask = 0x%08x\n",
		__func__, reg, value, msg_len, val_mask);
	memcpy((char *)api_word, (char *)api_access->write_msg, msg_len);
	switch (msg_len) {
	case 8:
		api_word[1] |= (val_mask & value);
		cpu_to_le32s(&api_word[1]);
		cpu_to_le32s(&api_word[0]);
		break;
	case 4:
		api_word[0] |= (val_mask & value);
		cpu_to_le32s(&api_word[0]);
		break;
	}
	memcpy(msg, (char *)api_word, msg_len);

	msg_ptr = msg;
	mutex_lock(&es705->api_mutex);
	rc = es705->dev_write(es705, msg_ptr, msg_len);
	mutex_unlock(&es705->api_mutex);
	return rc;
}

static int es705_write_then_read(struct es705_priv *es705,
				const void *buf, int len,
				u32 *rspn, int match)
{
	int rc;
	rc = es705->dev_write_then_read(es705, buf, len, rspn, match);
	return rc;
}

#if defined(SAMSUNG_ES705_FEATURE)
#define POLY 0x8408
#define BLOCK_PAYLOAD_SIZE 508
static unsigned short crc_update(char *buf, int length, unsigned short crc)
{
	unsigned char i;
	unsigned short data;
	
	while (length--) {
		data = (unsigned short)(*buf++) & 0xff;
		for (i = 0; i < 8; i++) {
			if ((crc & 1) ^ (data & 1))
				crc = (crc >> 1) ^ POLY;
			else
				crc >>= 1;
			data >>= 1;
		}
	}
	return (crc);

}

static void es705_write_sensory_vs_data_block(int type)
{
	int size, size4, i, rc = 0;
	const u8 *data;
	char *buf = NULL;
	unsigned char preamble[4];
	u32 cmd[] = {0x9017e021,0x90180001, 0xffffffff};
	u32 cmd_confirm[] = {0x9017e002, 0x90180001, 0xffffffff};
	unsigned short crc;
	u32 check_cmd;
	u32 check_rspn = 0;

	/* check the type (0 = grammar, 1 = net) */
	if (!type) {
		size = es705_priv.vs_grammar->size;
		data = es705_priv.vs_grammar->data;
	}
	else {
		size = es705_priv.vs_net->size;
		data = es705_priv.vs_net->data;
		cmd[1] = 0x90180002;
		cmd_confirm[0] = 0x9017e003;
	}
	
	/* rdb/wdb mode = 1 for the grammar file */
	rc = es705_write_block(&es705_priv, cmd);

	/* Add packet data and CRC and then download */
	buf = kzalloc((size + 2 + 3), GFP_KERNEL);
	if (!buf) {
		dev_err(es705_priv.dev, "%s(): kzalloc fail\n", __func__);
		return;
	}
	memcpy(buf, data, size);

	size4 = size + 2;
	size4 = ((size4 + 3) >> 2) << 2;
	size4 -= 2;

	while (size < size4)
		buf[size++] = 0;
	
	crc = crc_update(buf, size, 0xffff);
	buf[size++] = (unsigned char)(crc & 0xff);
	crc >>= 8;
	buf[size++] = (unsigned char)(crc & 0xff);

	for (i = 0; i < size; i += BLOCK_PAYLOAD_SIZE) {
		es705_priv.vs_keyword_param_size  = size - i;
		if (es705_priv.vs_keyword_param_size > BLOCK_PAYLOAD_SIZE)
			es705_priv.vs_keyword_param_size = BLOCK_PAYLOAD_SIZE;

		preamble[0] = 1;
		preamble[1] = 8;
		preamble[2] = 0;
		preamble[3] = (i == 0) ? 0 : 1;
		memcpy(es705_priv.vs_keyword_param, preamble, 4);
		memcpy(&es705_priv.vs_keyword_param[4], &buf[i], es705_priv.vs_keyword_param_size);
		es705_priv.vs_keyword_param_size += 4;
		es705_write_vs_data_block(&es705_priv);
		memset(es705_priv.vs_keyword_param, 0, ES705_VS_KEYWORD_PARAM_MAX);
	}

	kfree(buf);

	/* verify the download count and the CRC */
	check_cmd = 0x8016e031;
	check_rspn = 0;
	rc = es705_write_then_read(&es705_priv, &check_cmd, sizeof(check_cmd),
					 &check_rspn, 0);
	pr_info("%s: size = %x\n", __func__, check_rspn);

	check_cmd = 0x8016e02a;
	check_rspn = 0x80160000;
	rc = es705_write_then_read(&es705_priv, &check_cmd, sizeof(check_cmd),
					 &check_rspn, 1);
	if (rc)
		dev_err(es705_priv.dev, "%s(): es705 CRC check fail\n", __func__);

}

static int es705_write_sensory_vs_keyword(void)
{
	int rc = 0;
	u32 grammar_confirm[] = {0x9017e002, 0x90180001, 0xffffffff};
	u32 net_confirm[] = {0x9017e003, 0x90180001, 0xffffffff};
	u32 start_confirm[] = {0x9017e000, 0x90180001, 0xffffffff};
	
	/* download keyword using WDB */
	es705_write_sensory_vs_data_block(0);
	es705_write_sensory_vs_data_block(1);

	/* mark the grammar and net as valid */
	rc = es705_write_block(&es705_priv, grammar_confirm);
	rc = es705_write_block(&es705_priv, net_confirm);
	rc = es705_write_block(&es705_priv, start_confirm);

	return rc;

}
#endif
static ssize_t es705_route_status_show(struct device *dev,
				       struct device_attribute *attr,
				       char *buf)
{
	int ret = 0;
	unsigned int value = 0;
	char *status_name = "Route Status";

	value = es705_read(NULL, ES705_CHANGE_STATUS);

	ret = snprintf(buf, PAGE_SIZE,
		       "%s=0x%04x\n",
		       status_name, value);

	return ret;
}

static DEVICE_ATTR(route_status, 0444, es705_route_status_show, NULL);
/* /sys/devices/platform/msm_slim_ctrl.1/es705-codec-gen0/route_status */

static ssize_t es705_route_show(struct device *dev,
				struct device_attribute *attr,
				char *buf)
{
	struct es705_priv *es705 = &es705_priv;

	dev_dbg(es705->dev, "%s(): route=%ld\n",
		__func__, es705->internal_route_num);
	return snprintf(buf, PAGE_SIZE, "route=%ld\n",
			es705->internal_route_num);
}

static DEVICE_ATTR(route, 0444, es705_route_show, NULL);
/* /sys/devices/platform/msm_slim_ctrl.1/es705-codec-gen0/route */

static ssize_t es705_rate_show(struct device *dev,
			       struct device_attribute *attr,
			       char *buf)
{
	struct es705_priv *es705 = &es705_priv;

	dev_dbg(es705->dev, "%s(): rate=%ld\n", __func__, es705->internal_rate);
	return snprintf(buf, PAGE_SIZE, "rate=%ld\n",
			es705->internal_rate);
}

static DEVICE_ATTR(rate, 0444, es705_rate_show, NULL);
/* /sys/devices/platform/msm_slim_ctrl.1/es705-codec-gen0/rate */

#define SIZE_OF_VERBUF 256
/* TODO: fix for new read/write. use es705_read() instead of BUS ops */
static ssize_t es705_fw_version_show(struct device *dev,
				     struct device_attribute *attr,
				     char *buf)
{
	int idx = 0;
	unsigned int value;
	char versionbuffer[SIZE_OF_VERBUF];
	char *verbuf = versionbuffer;

	memset(verbuf, 0, SIZE_OF_VERBUF);

	value = es705_read(NULL, ES705_FW_FIRST_CHAR);
	*verbuf++ = (value & 0x00ff);
	for (idx = 0; idx < (SIZE_OF_VERBUF-2); idx++) {
		value = es705_read(NULL, ES705_FW_NEXT_CHAR);
		*verbuf++ = (value & 0x00ff);
	}
	/* Null terminate the string*/
	*verbuf = '\0';
	dev_info(dev, "Audience fw ver %s\n", versionbuffer);
	return snprintf(buf, PAGE_SIZE, "FW Version = %s\n", versionbuffer);
}

static DEVICE_ATTR(fw_version, 0444, es705_fw_version_show, NULL);
/* /sys/devices/platform/msm_slim_ctrl.1/es705-codec-gen0/fw_version */

static ssize_t es705_clock_on_show(struct device *dev,
				   struct device_attribute *attr,
				   char *buf)
{
	int ret = 0;

	return ret;
}

static DEVICE_ATTR(clock_on, 0444, es705_clock_on_show, NULL);
/* /sys/devices/platform/msm_slim_ctrl.1/es705-codec-gen0/clock_on */

static ssize_t es705_vs_keyword_parameters_show(struct device *dev,
						struct device_attribute *attr,
						char *buf)
{
	int ret = 0;

	if (es705_priv.vs_keyword_param_size > 0) {
		memcpy(buf, es705_priv.vs_keyword_param,
		       es705_priv.vs_keyword_param_size);
		ret = es705_priv.vs_keyword_param_size;
		dev_dbg(dev, "%s(): keyword param size=%hu\n", __func__, ret);
	} else {
		dev_dbg(dev, "%s(): keyword param not set\n", __func__);
	}

	return ret;
}

static ssize_t es705_vs_keyword_parameters_set(struct device *dev,
					       struct device_attribute *attr,
					       const char *buf, size_t count)
{
	int ret = 0;

	if (count <= ES705_VS_KEYWORD_PARAM_MAX) {
		memcpy(es705_priv.vs_keyword_param, buf, count);
		es705_priv.vs_keyword_param_size = count;
		dev_dbg(dev, "%s(): keyword param block set size = %zi\n",
			 __func__, count);
		ret = count;
	} else {
		dev_dbg(dev, "%s(): keyword param block too big = %zi\n",
			 __func__, count);
		ret = -ENOMEM;
	}

	return ret;
}

static DEVICE_ATTR(vs_keyword_parameters, 0644,
		   es705_vs_keyword_parameters_show,
		   es705_vs_keyword_parameters_set);
/* /sys/devices/platform/msm_slim_ctrl.1/es705-codec-gen0/vs_keyword_parameters */

static ssize_t es705_vs_status_show(struct device *dev,
			            struct device_attribute *attr,
				    char *buf)
{
	int ret = 0;
	unsigned int value = 0;
	char *status_name = "Voice Sense Status";
	/* Disable vs status read for interrupt to work */
	struct es705_priv *es705 = &es705_priv;

	mutex_lock(&es705->api_mutex);
	value = es705->vs_get_event;
	/* Reset the detection status after read */
	es705->vs_get_event = NO_EVENT;
	mutex_unlock(&es705->api_mutex);

	ret = snprintf(buf, PAGE_SIZE,
		       "%s=0x%04x\n",
		       status_name, value);

	return ret;
}

static DEVICE_ATTR(vs_status, 0444, es705_vs_status_show, NULL);
/* /sys/devices/platform/msm_slim_ctrl.1/es705-codec-gen0/vs_status */

static ssize_t es705_ping_status_show(struct device *dev,
			            struct device_attribute *attr,
				    char *buf)
{
	struct es705_priv *es705 = &es705_priv;
	int rc = 0;
	u32 sync_cmd = (ES705_SYNC_CMD << 16) | ES705_SYNC_POLLING;
	u32 sync_ack;
	char msg[4];
	char *status_name = "Ping";

	cpu_to_le32s(&sync_cmd);
	memcpy(msg, (char *)&sync_cmd, 4);
	rc = es705->dev_write(es705, msg, 4);
	if (rc < 0) {
		dev_err(es705->dev, "%s(): firmware load failed sync write\n",
			__func__);
	}
	msleep(20);
	memset(msg, 0, 4);
	rc = es705->dev_read(es705, msg, 4);
	if (rc < 0) {
		dev_err(es705->dev, "%s(): error reading sync ack rc=%d\n",
		       __func__, rc);
	}
	memcpy((char *)&sync_ack, msg, 4);
	le32_to_cpus(&sync_ack);
	dev_dbg(es705->dev, "%s(): sync_ack = 0x%08x\n", __func__, sync_ack);

	rc = snprintf(buf, PAGE_SIZE,
		       "%s=0x%08x\n",
		       status_name, sync_ack);

	return rc;
}

static DEVICE_ATTR(ping_status, 0444, es705_ping_status_show, NULL);
/* /sys/devices/platform/msm_slim_ctrl.1/es705-codec-gen0/ping_status */

static ssize_t es705_gpio_reset_set(struct device *dev,
					struct device_attribute *attr,
					const char *buf, size_t count)
{
	struct es705_priv *es705 = &es705_priv;
	dev_dbg(es705->dev, "%s(): GPIO reset\n", __func__);
	es705->mode = SBL;
	es705_gpio_reset(es705);
	dev_dbg(es705->dev, "%s(): Ready for STANDARD download by proxy\n",
		__func__);
	return count;
}

static DEVICE_ATTR(gpio_reset, 0644, NULL, es705_gpio_reset_set);
/* /sys/devices/platform/msm_slim_ctrl.1/es705-codec-gen0/gpio_reset */


static ssize_t es705_overlay_mode_set(struct device *dev,
					struct device_attribute *attr,
					const char *buf, size_t count)
{
	struct es705_priv *es705 = &es705_priv;
	int rc;
	int value = ES705_SET_POWER_STATE_VS_OVERLAY;

	dev_dbg(es705->dev, "%s(): Set Overlay mode\n", __func__);

	es705->mode = SBL;
	rc = es705_write(NULL, ES705_POWER_STATE , value);
	if (rc) {
		dev_err(es705_priv.dev, "%s(): Set Overlay mode failed\n",
			__func__);
	} else {
		msleep(50);
		es705_priv.es705_power_state = ES705_SET_POWER_STATE_VS_OVERLAY;
		/* wait until es705 SBL mode activating */
		dev_info(es705->dev, "%s(): After successful VOICESENSE download,"
			"Enable Event Intr to Host\n",
			__func__);
	}
	return count;
}

static DEVICE_ATTR(overlay_mode, 0644, NULL, es705_overlay_mode_set);
/* /sys/devices/platform/msm_slim_ctrl.1/es705-codec-gen0/overlay_mode */

static ssize_t es705_vs_event_set(struct device *dev,
				struct device_attribute *attr,
				const char *buf, size_t count)
{
	struct es705_priv *es705 = &es705_priv;
	int rc;
	int value = ES705_SYNC_INTR_RISING_EDGE;

	dev_dbg(es705->dev, "%s(): Enable Voice Sense Event to Host\n",
		__func__);

	es705->mode = VOICESENSE;
	/* Enable Voice Sense Event INTR to Host */
	rc = es705_write(NULL, ES705_EVENT_RESPONSE, value);
	if (rc)
		dev_err(es705->dev, "%s(): Enable Event Intr fail\n",
			__func__);
	return count;
}

static DEVICE_ATTR(vs_event, 0644, NULL, es705_vs_event_set);
/* /sys/devices/platform/msm_slim_ctrl.1/es705-codec-gen0/vs_event */

static ssize_t es705_tuning_set(struct device *dev,
				struct device_attribute *attr,
				const char *buf, size_t count)
{
	struct es705_priv *es705 = &es705_priv;
	unsigned int value = 0;

	sscanf(buf, "%d", &value);
	pr_info("%s : [ES705] uart_pin_config = %d\n", __func__, value);

	if (value == 0)
		es705->change_uart_config = 0;
	else
		es705->change_uart_config = 1;

	return count;	
}

static DEVICE_ATTR(tuning, 0644, NULL, es705_tuning_set);
/* /sys/devices/platform/msm_slim_ctrl.1/es705-codec-gen0/tuning */

static ssize_t es705_keyword_grammar_path_set(struct device *dev,
				struct device_attribute *attr,
				const char *buf, size_t count)
{
	char path[100];
	int rc = 0;

	sscanf(buf, "%s", path);
	pr_info("%s : [ES705] keyword_grammar_path = %s\n", __func__, path);
	/* get the grammar file */
	rc = request_firmware((const struct firmware **)&es705_priv.vs_grammar,
			      path, es705_priv.dev);
	if (rc) {
		dev_err(es705_priv.dev, "%s(): request_firmware(%s) failed %d\n",
			__func__, path, rc);
		sprintf(path, "higalaxy_en_us_gram6.bin");
		dev_info(es705_priv.dev, "%s(): request default grammar\n", __func__);
		rc = request_firmware((const struct firmware **)&es705_priv.vs_grammar,
			      path, es705_priv.dev);
		if (rc)
			dev_err(es705_priv.dev, "%s(): request_firmware(%s) failed %d\n",
				__func__, path, rc);

	}

	return count;
}

static DEVICE_ATTR(keyword_grammar_path, 0664, NULL, es705_keyword_grammar_path_set);
/* /sys/devices/platform/msm_slim_ctrl.1/es705-codec-gen0/keyword_grammar_path */

static ssize_t es705_keyword_net_path_set(struct device *dev,
				struct device_attribute *attr,
				const char *buf, size_t count)
{
	char path[100];
	int rc = 0;

	sscanf(buf, "%s", path);
	pr_info("%s : [ES705] keyword_net_path = %s\n", __func__, path);

	rc = request_firmware((const struct firmware **)&es705_priv.vs_net,
			      path, es705_priv.dev);
	if (rc) {
		dev_err(es705_priv.dev, "%s(): request_firmware(%s) failed %d\n",
			__func__, path, rc);
		sprintf(path, "higalaxy_en_us_am.bin");
		dev_info(es705_priv.dev, "%s(): request default net\n", __func__);
		rc = request_firmware((const struct firmware **)&es705_priv.vs_net,
			      path, es705_priv.dev);
		if (rc)
			dev_err(es705_priv.dev, "%s(): request_firmware(%s) failed %d\n",
				__func__, path, rc);
	}

	return count;
}

static DEVICE_ATTR(keyword_net_path, 0664, NULL, es705_keyword_net_path_set);
/* /sys/devices/platform/msm_slim_ctrl.1/es705-codec-gen0/keyword_net_path */
static ssize_t es705_sleep_delay_show(struct device *dev,
					struct device_attribute *attr,
					char *buf)
{
	int ret = 0;
	ret = snprintf(buf, PAGE_SIZE, "%d\n", es705_priv.sleep_delay);
	return ret;
}

static ssize_t es705_sleep_delay_set(struct device *dev,
					struct device_attribute *attr,
					const char *buf, size_t count)
{
	int rc;
	rc = kstrtoint(buf, 0, &es705_priv.sleep_delay);
	dev_info(es705_priv.dev, "%s(): sleep delay = %d\n",
		__func__, es705_priv.sleep_delay);
	return count;
}
static DEVICE_ATTR(sleep_delay, 0666, es705_sleep_delay_show, es705_sleep_delay_set);
/* /sys/devices/platform/msm_slim_ctrl.1/es705-codec-gen0/sleep_delay */

static struct attribute *core_sysfs_attrs[] = {
	&dev_attr_route_status.attr,
	&dev_attr_route.attr,
	&dev_attr_rate.attr,
	&dev_attr_fw_version.attr,
	&dev_attr_clock_on.attr,
	&dev_attr_vs_keyword_parameters.attr,
	&dev_attr_vs_status.attr,
	&dev_attr_ping_status.attr,
	&dev_attr_gpio_reset.attr,
	&dev_attr_overlay_mode.attr,
	&dev_attr_vs_event.attr,
	&dev_attr_tuning.attr,
	&dev_attr_keyword_grammar_path.attr,
	&dev_attr_keyword_net_path.attr,
	&dev_attr_sleep_delay.attr,
	NULL
};

static struct attribute_group core_sysfs = {
	.attrs = core_sysfs_attrs
};
extern unsigned int system_rev;

#if defined(CONFIG_MACH_KLTE_JPN)
#define UART_DOWNLOAD_WAKEUP_HWREV 7
#else
#define UART_DOWNLOAD_WAKEUP_HWREV 6 /* HW rev0.7 */
#endif

static int es705_fw_download(struct es705_priv *es705, int fw_type)
{
	int rc = 0;

	mutex_lock(&es705->api_mutex);
	if (fw_type != VOICESENSE && fw_type != STANDARD) {
		dev_err(es705->dev, "%s(): Unexpected FW type\n", __func__);
		goto es705_fw_download_exit;
	}

	if (system_rev >= UART_DOWNLOAD_WAKEUP_HWREV || es705->uart_fw_download_rate)
		if (es705->uart_fw_download &&
				es705->uart_fw_download(es705, fw_type) >= 0) {
			es705->mode = fw_type;
			goto es705_fw_download_exit;
		}

	rc = es705->boot_setup(es705);
	if (rc) {
		dev_err(es705->dev, "%s(): fw download start error\n",
			__func__);
		goto es705_fw_download_exit;
	}

	dev_info(es705->dev, "%s(): write firmware image\n", __func__);
	if (fw_type == VOICESENSE)
		rc = es705->dev_write(es705, (char *)es705->vs->data,
					es705->vs->size);
	else
		rc = es705->dev_write(es705, (char *)es705->standard->data,
			es705->standard->size);
	if (rc) {
		dev_err(es705->dev, "%s(): firmware image write error\n",
			__func__);
		rc = -EIO;
		goto es705_fw_download_exit;
	}

	es705->mode = fw_type;
	rc = es705->boot_finish(es705);
	if (rc) {
		dev_err(es705->dev, "%s() fw download finish error\n",
			__func__);
			goto es705_fw_download_exit;
	}
	dev_info(es705->dev, "%s(): fw download done\n", __func__);

es705_fw_download_exit:
	//es705->uart_fw_download_rate = 0;
	mutex_unlock(&es705->api_mutex);
	return rc;
}

int es705_bootup(struct es705_priv *es705)
{
	int rc;
	BUG_ON(es705->standard->size == 0);

	mutex_lock(&es705->pm_mutex);
	es705->pm_state = ES705_POWER_FW_LOAD;
	mutex_unlock(&es705->pm_mutex);

	rc = es705_fw_download(es705, STANDARD);
	if (rc) {
		dev_err(es705->dev, "%s(): STANDARD fw download error\n",
			__func__);
	} else {
		mutex_lock(&es705->pm_mutex);
		es705->pm_state = ES705_POWER_AWAKE;
#if defined(SAMSUNG_ES705_FEATURE)
		es705->es705_power_state = ES705_SET_POWER_STATE_NORMAL;
#endif
		mutex_unlock(&es705->pm_mutex);
	}
	return rc;
}

static int es705_set_lpm(struct es705_priv *es705)
{
	int rc;
	const int max_retry_to_switch_to_lpm = 5;
	int retry = max_retry_to_switch_to_lpm;

	rc = es705_write(NULL, 	ES705_VS_INT_OSC_MEASURE_START, 0);
	if (rc) {
		dev_err(es705_priv.dev, "%s(): OSC Measure Start fail\n",
			__func__);
		goto es705_set_lpm_exit;
	}

	do {
		/* Wait 20ms before reading up to 5 times (total 100ms) */
		msleep(20);
		rc = es705_read(NULL, ES705_VS_INT_OSC_MEASURE_STATUS);
		if (rc < 0) {
			dev_err(es705_priv.dev, "%s(): OSC Measure Read Status fail\n",
				__func__);
			goto es705_set_lpm_exit;
		}
		dev_dbg(es705_priv.dev, "%s(): OSC Measure Status = 0x%04x\n",
			__func__, rc);
	} while (rc && --retry);

	dev_dbg(es705_priv.dev, "%s(): Activate Low Power Mode\n", __func__);
	rc = es705_write(NULL, ES705_POWER_STATE,
			 ES705_SET_POWER_STATE_VS_LOWPWR);
	if (rc) {
		dev_err(es705_priv.dev, "%s(): Write cmd fail\n", __func__);
		goto es705_set_lpm_exit;
	}

	es705_priv.es705_power_state = ES705_SET_POWER_STATE_VS_LOWPWR;

	if (es705_priv.pdata->esxxx_clk_cb)
		/* ext clock off */
		es705_priv.pdata->esxxx_clk_cb(0);

es705_set_lpm_exit:
	return rc;
}

static int es705_vs_load(struct es705_priv *es705)
{
	int rc;
	BUG_ON(es705->vs->size == 0);

	/* wait es705 SBL mode */
	msleep(50);

	es705->es705_power_state = ES705_SET_POWER_STATE_VS_OVERLAY;
	rc = es705_fw_download(es705, VOICESENSE);
	if (rc < 0) {
                dev_err(es705->dev, "%s(): FW download fail\n",
                        __func__);
		goto es705_vs_load_fail;
	}
	msleep(50);
	/* Enable Voice Sense Event INTR to Host */
	rc = es705_write(NULL, ES705_EVENT_RESPONSE,
			ES705_SYNC_INTR_RISING_EDGE);
	if (rc) {
		dev_err(es705->dev, "%s(): Enable Event Intr fail\n",
			__func__);
		goto es705_vs_load_fail;
	}

es705_vs_load_fail:
	return rc;
}

static int register_snd_soc(struct es705_priv *priv);

int fw_download(void *arg)
{
	struct es705_priv *priv = (struct es705_priv *)arg;
	int rc;

	dev_info(priv->dev, "%s(): fw download\n", __func__);
	rc = es705_bootup(priv);
	dev_info(priv->dev, "%s(): bootup rc=%d\n", __func__, rc);

	rc = register_snd_soc(priv);
	dev_info(priv->dev, "%s(): register_snd_soc rc=%d\n", __func__, rc);

#ifdef FIXED_CONFIG
	es705_fixed_config(priv);
#endif

	dev_info(priv->dev, "%s(): release module\n", __func__);
	module_put(THIS_MODULE);
	return 0;
}

/* Hold the pm_mutex before calling this function */
static int es705_sleep(struct es705_priv *es705)
{
	u32 cmd = (ES705_SET_SMOOTH << 16) | ES705_SET_SMOOTH_RATE;
	int rc;
	dev_info(es705->dev, "%s\n",__func__);

	/* Avoid smoothing time */
	rc = es705_cmd(es705, cmd);
	if (rc < 0) {
		dev_err(es705->dev, "%s(): Reset Smooth Rate Fail",
			__func__);
		goto es705_sleep_exit;
	}
	if (es705->voice_wakeup_enable) {
		u32 cmd = (ES705_SET_POWER_STATE << 16) |
				ES705_SET_POWER_STATE_VS_OVERLAY;
		rc = es705_cmd(es705, cmd);
		if (rc < 0) {
			dev_err(es705->dev, "%s(): Set VS SBL Fail",
			__func__);
			goto es705_sleep_exit;
		}
		msleep(20);
		dev_dbg(es705->dev, "%s(): VS Overlay Mode", __func__);
		rc = es705_vs_load(es705);
		if (rc) {
			dev_err(es705->dev, "%s(): Set VS Overlay FAIL", __func__);
		} else {
			/* Set PDM route */
			es705_switch_route(22);
			rc = es705_set_lpm(es705);
			if (rc) {
				dev_err(es705->dev, "%s(): Set LPM FAIL", __func__);
			} else {
				es705->pm_state = ES705_POWER_SLEEP;
				goto es705_sleep_exit;
			}
		}
	}
	/*
	 * write Set Power State Sleep - 0x9010_0001
	 * wait 20 ms, and then turn ext clock off
	 * There will not be any response after
	 * sleep command from chip
	 */
	cmd = (ES705_SET_POWER_STATE << 16) | ES705_SET_POWER_STATE_SLEEP;
	es705_cmd(es705, cmd);
	msleep(20);

	es705->es705_power_state = ES705_SET_POWER_STATE_SLEEP;
	es705->pm_state = ES705_POWER_SLEEP;

	if (es705->pdata->esxxx_clk_cb)
		es705->pdata->esxxx_clk_cb(0);

es705_sleep_exit:
	return rc;
}

static void es705_delayed_sleep(struct work_struct *w)
{
	int ch_tot;
	int ports_active = (es705_priv.rx1_route_enable ||
		es705_priv.rx2_route_enable || es705_priv.tx1_route_enable);

	/*
	 * If there are active streams we do not sleep.
	 * Count the front end (FE) streams ONLY.
	 */

	ch_tot = 0;
	ch_tot += es705_priv.dai[ES705_SLIM_1_PB].ch_tot;
	ch_tot += es705_priv.dai[ES705_SLIM_2_PB].ch_tot;

	ch_tot += es705_priv.dai[ES705_SLIM_1_CAP].ch_tot;
	dev_dbg(es705_priv.dev, "%s %d active channels, ports_active: %d\n",
		__func__, ch_tot, ports_active);

	mutex_lock(&es705_priv.pm_mutex);
	if ((ch_tot <= 0) && (ports_active == 0) &&
		(es705_priv.pm_state ==  ES705_POWER_SLEEP_PENDING))
		es705_sleep(&es705_priv);

	mutex_unlock(&es705_priv.pm_mutex);
}

static void es705_sleep_request(struct es705_priv *es705)
{
	dev_dbg(es705->dev, "%s internal es705_power_state = %d\n",
		__func__, es705_priv.pm_state);

	mutex_lock(&es705->pm_mutex);
	if (es705->uart_state == UART_OPEN)
		es705_uart_close(es705);
	if (es705->pm_state == ES705_POWER_AWAKE) {
		schedule_delayed_work(&es705->sleep_work,
			msecs_to_jiffies(es705->sleep_delay));
		es705->pm_state = ES705_POWER_SLEEP_PENDING;
	}
	mutex_unlock(&es705->pm_mutex);
}

#define SYNC_DELAY 30
static int es705_wakeup(struct es705_priv *es705)
{
	int rc = 0;
	u32 sync_cmd = (ES705_SYNC_CMD << 16) | ES705_SYNC_POLLING;
	u32 sync_rspn = sync_cmd;
	int match = 1;
	dev_info(es705->dev, "%s\n",__func__);
	/* 1 - clocks on
	 * 2 - wakeup 1 -> 0
	 * 3 - sleep 30 ms
	 * 4 - Send sync command (0x8000, 0x0001)
	 * 5 - Read sync ack
	 * 6 - wakeup 0 -> 1
	 */

	mutex_lock(&es705->pm_mutex);
#if defined(SAMSUNG_ES705_FEATURE)
	msm_slim_es705_func(es705_priv.gen0_client);
#endif
	if (delayed_work_pending(&es705->sleep_work) ||
		(es705->pm_state == ES705_POWER_SLEEP_PENDING)) {
		cancel_delayed_work_sync(&es705->sleep_work);
		dev_dbg(es705->dev, "%s(): cancel delayed work\n", __func__);
		es705->pm_state = ES705_POWER_AWAKE;
		goto es705_wakeup_exit;
	}

	/* Check if previous power state is not sleep then return */
	if (es705->pm_state != ES705_POWER_SLEEP) {
		dev_err(es705->dev, "%s(): no need to go to Normal Mode\n",
			__func__);
		goto es705_wakeup_exit;
	}

	if (es705->pdata->esxxx_clk_cb) {
		es705->pdata->esxxx_clk_cb(1);
		usleep_range(3000, 3100);
	}
	
#if defined(SAMSUNG_ES705_FEATURE)
	if (es705_priv.use_uart_for_wakeup_gpio) {
		dev_info(es705->dev, "%s(): begin uart wakeup\n",
			__func__);
#endif
		es705_uart_es705_wakeup(es705);

#if !defined(SAMSUNG_ES705_FEATURE)
		if (es705->wakeup_bus)
			es705->wakeup_bus(es705);
#endif
#if defined(SAMSUNG_ES705_FEATURE)
	} else {
		es705_gpio_wakeup(es705);
	}
#endif
	dev_dbg(es705->dev, "%s(): wait %dms wakeup, then ping SYNC to es705\n",
		__func__, SYNC_DELAY);
	msleep(SYNC_DELAY);

	rc = es705_write_then_read(es705, &sync_cmd, sizeof(sync_cmd),
					 &sync_rspn, match);
	if (rc) {
		dev_err(es705->dev, "%s(): es705 wakeup FAIL\n", __func__);
		if (es705->pdata->esxxx_clk_cb) {
			es705->pdata->esxxx_clk_cb(0);
			usleep_range(3000, 3100);
		}
		goto es705_wakeup_exit;
	}

	dev_dbg(es705->dev, "%s(): wakeup success, SYNC response 0x%08x\n",
		__func__, sync_rspn);
	if (es705->es705_power_state != ES705_SET_POWER_STATE_VS_LOWPWR &&
		es705->es705_power_state != ES705_SET_POWER_STATE_VS_OVERLAY) {
		u32 cmd = (ES705_SET_POWER_STATE << 16) | ES705_SET_POWER_STATE_NORMAL;

		es705_cmd(es705, cmd);
		msleep(20);
		es705->pm_state = ES705_POWER_AWAKE;
	}
	/* TODO use GPIO reset, if wakeup fail ? */

es705_wakeup_exit:
	mutex_unlock(&es705->pm_mutex);
	return rc;
}

#if defined(SAMSUNG_ES705_FEATURE)
static void es705_read_write_power_control(int read_write)
{
	if (read_write)
		es705_priv.power_control(ES705_SET_POWER_STATE_NORMAL, ES705_POWER_STATE);
	else if (!(es705_priv.rx1_route_enable || es705_priv.rx2_route_enable || es705_priv.tx1_route_enable || es705_priv.voice_wakeup_enable))
		es705_priv.power_control(ES705_SET_POWER_STATE_SLEEP, ES705_POWER_STATE);
}
#endif

static int es705_put_control_value(struct snd_kcontrol *kcontrol,
				   struct snd_ctl_elem_value *ucontrol)
{
	/* struct snd_soc_codec *codec = es705_priv.codec; */
	struct soc_mixer_control *mc =
		(struct soc_mixer_control *)kcontrol->private_value;
	unsigned int reg = mc->reg;
	unsigned int value;
	int rc = 0;

	value = ucontrol->value.integer.value[0];
	rc = es705_write(NULL, reg, value);

	return 0;
}

static int es705_get_control_value(struct snd_kcontrol *kcontrol,
				   struct snd_ctl_elem_value *ucontrol)
{
	/* struct snd_soc_codec *codec = es705_priv.codec; */
	struct soc_mixer_control *mc =
		(struct soc_mixer_control *)kcontrol->private_value;
	unsigned int reg = mc->reg;
	unsigned int value;

#if defined(SAMSUNG_ES705_FEATURE)
	es705_read_write_power_control(1);
#endif
	value = es705_read(NULL, reg);
	ucontrol->value.integer.value[0] = value;
#if defined(SAMSUNG_ES705_FEATURE)
	es705_read_write_power_control(0);
#endif

	return 0;
}

static int es705_put_control_enum(struct snd_kcontrol *kcontrol,
				  struct snd_ctl_elem_value *ucontrol)
{
	struct soc_enum *e =
		(struct soc_enum *)kcontrol->private_value;
	unsigned int reg = e->reg;
	unsigned int value;
	int rc = 0;

	value = ucontrol->value.enumerated.item[0];
	rc = es705_write(NULL, reg, value);

	return 0;
}

static int es705_get_control_enum(struct snd_kcontrol *kcontrol,
				  struct snd_ctl_elem_value *ucontrol)
{
	struct soc_enum *e =
		(struct soc_enum *)kcontrol->private_value;
	unsigned int reg = e->reg;
	unsigned int value;

#if defined(SAMSUNG_ES705_FEATURE)
	es705_read_write_power_control(1);
#endif
	value = es705_read(NULL, reg);

	ucontrol->value.enumerated.item[0] = value;
#if defined(SAMSUNG_ES705_FEATURE)
	es705_read_write_power_control(0);
#endif

	return 0;
}

static int es705_get_power_control_enum(struct snd_kcontrol *kcontrol,
				  struct snd_ctl_elem_value *ucontrol)
{
	struct soc_enum *e =
		(struct soc_enum *)kcontrol->private_value;
	unsigned int reg = e->reg;
	unsigned int value;

	/* Don't read if already in Sleep Mode */
	if (es705_priv.pm_state == ES705_POWER_SLEEP)
		value = es705_priv.es705_power_state;
	else
		value = es705_read(NULL, reg);

	ucontrol->value.enumerated.item[0] = value;

	return 0;
}

static int es705_get_uart_fw_download_rate(struct snd_kcontrol *kcontrol,
					struct snd_ctl_elem_value *ucontrol)
{
	ucontrol->value.enumerated.item[0] = es705_priv.uart_fw_download_rate;
	return 0;
}

static int es705_put_uart_fw_download_rate(struct snd_kcontrol *kcontrol,
					struct snd_ctl_elem_value *ucontrol)
{
	/*
	 * 0 - no use UART
	 * 1 - use UART for FW download. Baud Rate 4.608 KBps
	 * 2 - use UART for FW download. Baud Rate 1 MBps
	 * 3 - use UART for FW download. Baud Rate 3 Mbps
	 */
	es705_priv.uart_fw_download_rate = ucontrol->value.enumerated.item[0];
	return 0;
}

static int es705_get_vs_stream_enable(struct snd_kcontrol *kcontrol,
				   struct snd_ctl_elem_value *ucontrol)
{
	ucontrol->value.enumerated.item[0] = es705_priv.vs_stream_enable;
	return 0;
}

static int es705_put_vs_stream_enable(struct snd_kcontrol *kcontrol,
				   struct snd_ctl_elem_value *ucontrol)
{
	es705_priv.vs_stream_enable = ucontrol->value.enumerated.item[0];
	return 0;
}

#if defined(SAMSUNG_ES705_FEATURE)
static int es705_sleep_power_control(unsigned int value, unsigned int reg)
{
	int rc = 0;

	switch (es705_priv.es705_power_state) {
	case ES705_SET_POWER_STATE_SLEEP :
		if (value == ES705_SET_POWER_STATE_NORMAL) {
			rc = es705_wakeup(&es705_priv);
			if (rc < 0) {
				dev_err(es705_priv.dev, "%s(): es705_wakeup failed\n",
					__func__);
				return rc;
			}
			rc = es705_write(NULL, reg, value);
			if (rc < 0) {
				dev_err(es705_priv.dev, "%s(): Power state command write failed\n",
					__func__);
				return rc;
			}
			/* Wait for 100ms to switch from Overlay mode */
			msleep(100);
			es705_priv.es705_power_state =
				ES705_SET_POWER_STATE_NORMAL;
			es705_priv.mode = STANDARD;
			es705_priv.pm_state = ES705_POWER_AWAKE;
		} else if (value == ES705_SET_POWER_STATE_VS_OVERLAY) {
			rc = es705_wakeup(&es705_priv);
			if (rc < 0) {
				dev_err(es705_priv.dev, "%s(): es705_wakeup failed\n",
					__func__);
				return rc;
			}
			rc = es705_write(NULL, reg, value);
			if (rc < 0) {
				dev_err(es705_priv.dev, "%s(): Power state command write failed\n",
					__func__);
				return rc;
			} else {
				es705_vs_load(&es705_priv);
			}
		}
		break;

	case ES705_SET_POWER_STATE_NORMAL :
	case ES705_SET_POWER_STATE_VS_OVERLAY :
		return -EINVAL;

	case ES705_SET_POWER_STATE_VS_LOWPWR :
		if (value == ES705_SET_POWER_STATE_VS_OVERLAY) {
			rc = es705_wakeup(&es705_priv);
			if (rc < 0) {
				dev_err(es705_priv.dev, "%s(): es705_wakeup failed\n",
					__func__);
				return rc;
			}
			es705_priv.es705_power_state =
				ES705_SET_POWER_STATE_VS_OVERLAY;
			es705_priv.pm_state = ES705_POWER_AWAKE;
		}
		break;

	default :
		return -EINVAL;
	}
	return rc;
}

static int es705_sleep_pending_power_control(unsigned int value,
						unsigned int reg)
{
	int rc = 0;
	int retry = 10;

	switch (es705_priv.es705_power_state) {
	case ES705_SET_POWER_STATE_SLEEP :
		return -EINVAL;

	case ES705_SET_POWER_STATE_NORMAL :
		if (value == ES705_SET_POWER_STATE_SLEEP)
			es705_sleep_request(&es705_priv);
		else if (value == ES705_SET_POWER_STATE_NORMAL) {
			rc = es705_wakeup(&es705_priv);
			if (rc < 0) {
				dev_err(es705_priv.dev, "%s(): es705_wakeup failed\n",
					__func__);
				return rc;
			}
			es705_priv.es705_power_state = \
				ES705_SET_POWER_STATE_NORMAL;
			es705_priv.mode = STANDARD;
			es705_priv.pm_state = ES705_POWER_AWAKE;
		} else if (value == ES705_SET_POWER_STATE_VS_OVERLAY) {
			rc = es705_wakeup(&es705_priv);
			if (rc < 0) {
				dev_err(es705_priv.dev, "%s(): es705_wakeup failed\n",
					__func__);
				return rc;
			}
			rc = es705_write(NULL, reg, value);
			if (rc < 0) {
				dev_err(es705_priv.dev, "%s(): Power state command write failed\n",
					__func__);
				return rc;
			} else {
				es705_vs_load(&es705_priv);
			}
		}
		break;

	case ES705_SET_POWER_STATE_VS_OVERLAY :
		if (value == ES705_SET_POWER_STATE_SLEEP)
			es705_sleep_request(&es705_priv);
		else if (value == ES705_SET_POWER_STATE_NORMAL) {
			rc = es705_wakeup(&es705_priv);
			if (rc < 0) {
				dev_err(es705_priv.dev, "%s(): es705_wakeup failed\n",
					__func__);
				return rc;
			}
			rc = es705_write(NULL, reg, value);
			if (rc < 0) {
				dev_err(es705_priv.dev, "%s(): Power state command write failed\n",
					__func__);
				return rc;
			}
			/* Wait for 100ms to switch from Overlay mode */
			msleep(100);
			es705_priv.es705_power_state = \
				ES705_SET_POWER_STATE_NORMAL;
			es705_priv.mode = STANDARD;
		} else if (value == ES705_SET_POWER_STATE_VS_LOWPWR) {
			rc = es705_write(NULL,
				ES705_VS_INT_OSC_MEASURE_START, 0);
			do {
				/* Wait 20ms each time before reading,
				Status may take 100ms to be done
				added retries */
				msleep(20);
				rc = es705_read(NULL,
				ES705_VS_INT_OSC_MEASURE_STATUS);
				if (rc == 0) {
					dev_dbg(es705_priv.dev, "%s(): Activate Low Power Mode\n",
						__func__);
					es705_write(NULL, reg, value);
					es705_priv.es705_power_state =\
					ES705_SET_POWER_STATE_VS_LOWPWR;
					es705_priv.pm_state =\
						ES705_POWER_SLEEP;
					return rc;
				}
			} while (retry--);
		}
		break;

	case ES705_SET_POWER_STATE_VS_LOWPWR :
	default :
		return -EINVAL;
	}

	return rc;
}

static int es705_awake_power_control(unsigned int value, unsigned int reg)
{
	int rc = 0;
	int retry = 10;

	switch (es705_priv.es705_power_state) {
	case ES705_SET_POWER_STATE_SLEEP :
		return -EINVAL;

	case ES705_SET_POWER_STATE_NORMAL :
		if (value == ES705_SET_POWER_STATE_SLEEP)
			es705_sleep_request(&es705_priv);
		else if (value == ES705_SET_POWER_STATE_VS_OVERLAY) {
			rc = es705_write(NULL, reg, value);
			if (rc < 0) {
				dev_err(es705_priv.dev, "%s(): Power state command write failed\n",
					__func__);
				return rc;
			} else {
				es705_vs_load(&es705_priv);
			}
		} else
			return -EINVAL;
		break;

	case ES705_SET_POWER_STATE_VS_OVERLAY :
		if (value == ES705_SET_POWER_STATE_SLEEP)
			es705_sleep_request(&es705_priv);
		else if (value == ES705_SET_POWER_STATE_NORMAL) {
			rc = es705_write(NULL, reg, value);
			if (rc < 0) {
				dev_err(es705_priv.dev, "%s(): Power state command write failed\n",
					__func__);
				return rc;
			}
			/* Wait for 100ms to switch from Overlay mode */
			msleep(100);
			es705_priv.es705_power_state =
				ES705_SET_POWER_STATE_NORMAL;
			es705_priv.mode = STANDARD;
		} else if (value == ES705_SET_POWER_STATE_VS_LOWPWR) {
			rc = es705_write(NULL,
				ES705_VS_INT_OSC_MEASURE_START, 0);
			do {
				/* Wait 20ms each time before reading,
				Status may take 100ms to be done
				added retries */
				msleep(20);
				rc = es705_read(NULL,
				ES705_VS_INT_OSC_MEASURE_STATUS);
				if (rc == 0) {
					dev_dbg(es705_priv.dev, "%s(): Activate Low Power Mode\n",
						__func__);
					es705_write(NULL, reg, value);

					/* Disable external clock */
					msleep(20);
					/* clocks off */
					if (es705_priv.pdata->esxxx_clk_cb)
						es705_priv.
							pdata->esxxx_clk_cb(0);

					es705_priv.es705_power_state =
					ES705_SET_POWER_STATE_VS_LOWPWR;
					es705_priv.pm_state =
						ES705_POWER_SLEEP;
					return rc;
				}
			} while (retry--);
		}
		break;

	case ES705_SET_POWER_STATE_VS_LOWPWR :
		dev_dbg(es705_priv.dev, "%s(): Set Overlay mode\n", __func__);
		es705_priv.mode = VOICESENSE;
		es705_priv.es705_power_state = ES705_SET_POWER_STATE_VS_OVERLAY;
		/* wait until es705 SBL mode activating */
		dev_dbg(es705_priv.dev, "%s(): Ready for VOICESENSE download by proxy\n",
			__func__);
		dev_info(es705_priv.dev, "%s(): After successful VOICESENSE download,"
			"Enable Event Intr to Host\n", __func__);
	default :
		return -EINVAL;
	}
	return rc;
}

static int es705_power_control(unsigned int value, unsigned int reg)
{
	int rc = 0;

	dev_info(es705_priv.dev, "%s(): entry pm state %d es705 state %d value %d\n",
		__func__, es705_priv.pm_state,
		es705_priv.es705_power_state, value);

	if (value == 0 || value == ES705_SET_POWER_STATE_MP_SLEEP ||
		value == ES705_SET_POWER_STATE_MP_CMD) {
		dev_err(es705_priv.dev, "%s(): Unsupported value in es705\n",
			__func__);
		return -EINVAL;
	}
	switch (es705_priv.pm_state) {
	case ES705_POWER_FW_LOAD :
		return -EINVAL;
 
	case ES705_POWER_SLEEP :
		rc = es705_sleep_power_control(value, reg);
		break;

	case ES705_POWER_SLEEP_PENDING :
		rc = es705_sleep_pending_power_control(value, reg);
		break;


	case ES705_POWER_AWAKE :
		rc = es705_awake_power_control(value, reg);
		break;

	default :
		dev_err(es705_priv.dev, "%s(): Unsupported pm state [%d] in es705\n",
			__func__, es705_priv.pm_state);
		break;
	}

	dev_info(es705_priv.dev, "%s(): exit pm state %d es705 state %d value %d\n",
		__func__, es705_priv.pm_state,
		es705_priv.es705_power_state, value);

	return rc;
}
#endif

#define MAX_RETRY_TO_SWITCH_TO_LOW_POWER_MODE	5
static int es705_put_power_control_enum(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol)
{
	struct soc_enum *e = (struct soc_enum *)kcontrol->private_value;
	unsigned int reg = e->reg;
	unsigned int value;
	int rc = 0;

	value = ucontrol->value.enumerated.item[0];
	dev_dbg(es705_priv.dev, "%s(): Previous power state = %s, power set cmd = %s\n",
		__func__, power_state[es705_priv.pm_state],
		power_state_cmd[es705_priv.es705_power_state]);
	dev_dbg(es705_priv.dev, "%s(): Requested power set cmd = %s\n",
		__func__, power_state_cmd[value]);

	if (value == 0 || value == ES705_SET_POWER_STATE_MP_SLEEP ||
		value == ES705_SET_POWER_STATE_MP_CMD) {
		dev_err(es705_priv.dev, "%s(): Unsupported state in es705\n",
			__func__);
		rc = -EINVAL;
		goto es705_put_power_control_enum_exit;
	} else {
		if ((es705_priv.pm_state == ES705_POWER_SLEEP) &&
			(value != ES705_SET_POWER_STATE_NORMAL) &&
			(value != ES705_SET_POWER_STATE_VS_OVERLAY)) {
			dev_err(es705_priv.dev, "%s(): ES705 is in sleep mode."
				" Select the Normal Mode or Overlay"
				" if in Low Power mode.\n", __func__);
			rc = -EPERM;
			goto  es705_put_power_control_enum_exit;
		}

		if (value == ES705_SET_POWER_STATE_SLEEP) {
			dev_dbg(es705_priv.dev, "%s(): Activate Sleep Request\n",
						__func__);
			es705_sleep_request(&es705_priv);
		} else if (value == ES705_SET_POWER_STATE_NORMAL) {
			/* Overlay mode doesn't need wakeup */
			if (es705_priv.es705_power_state !=
				ES705_SET_POWER_STATE_VS_OVERLAY) {
				rc = es705_wakeup(&es705_priv);
				if (rc)
					goto es705_put_power_control_enum_exit;
			} else {
				rc = es705_write(NULL, ES705_POWER_STATE,
						 value);
				if (rc) {
					dev_err(es705_priv.dev, "%s(): Power state command write failed\n",
						__func__);
					goto es705_put_power_control_enum_exit;
				}
				/* Wait for 100ms to switch from Overlay mode */
				msleep(100);
			}
			es705_priv.es705_power_state =
				ES705_SET_POWER_STATE_NORMAL;
			es705_priv.mode = STANDARD;

		} else if (value == ES705_SET_POWER_STATE_VS_LOWPWR) {
			if (es705_priv.es705_power_state ==
					ES705_SET_POWER_STATE_VS_OVERLAY) {
				rc = es705_set_lpm(&es705_priv);
				if (rc)
					dev_err(es705_priv.dev, "%s(): Can't switch to Low Power Mode\n",
						__func__);
				else
					es705_priv.pm_state = ES705_POWER_SLEEP;
			} else {
				dev_err(es705_priv.dev, "%s(): ES705 should be in VS Overlay"
					"mode. Select the VS Overlay Mode.\n",
					__func__);
				rc = -EINVAL;
			}
			goto es705_put_power_control_enum_exit;
		} else if (value == ES705_SET_POWER_STATE_VS_OVERLAY) {
			if (es705_priv.es705_power_state ==
					ES705_SET_POWER_STATE_VS_LOWPWR) {
				rc = es705_wakeup(&es705_priv);
				if (rc)
					goto es705_put_power_control_enum_exit;
				es705_priv.es705_power_state =
					     ES705_SET_POWER_STATE_VS_OVERLAY;
			} else {
				rc = es705_write(NULL, reg, value);
				if (rc) {
					dev_err(es705_priv.dev, "%s(): Power state command write failed\n",
						__func__);
					goto es705_put_power_control_enum_exit;
				} else {
					es705_vs_load(&es705_priv);
				}
			}
		}
	}

es705_put_power_control_enum_exit:
	dev_dbg(es705_priv.dev, "%s(): Current power state = %s, power set cmd = %s\n",
		__func__, power_state[es705_priv.pm_state],
		power_state_cmd[es705_priv.es705_power_state]);
	return rc;
}

static int es705_get_rx1_route_enable_value(struct snd_kcontrol *kcontrol,
				       struct snd_ctl_elem_value *ucontrol)
{
	ucontrol->value.integer.value[0] = es705_priv.rx1_route_enable;
	dev_dbg(es705_priv.dev, "%s(): rx1_route_enable = %d\n",
		__func__, es705_priv.rx1_route_enable);

	return 0;
}

static int es705_put_rx1_route_enable_value(struct snd_kcontrol *kcontrol,
				       struct snd_ctl_elem_value *ucontrol)
{
	es705_priv.rx1_route_enable = ucontrol->value.integer.value[0];
	dev_dbg(es705_priv.dev, "%s(): rx1_route_enable = %d\n",
		__func__, es705_priv.rx1_route_enable);
#if defined(SAMSUNG_ES705_FEATURE)
	if (es705_priv.power_control) {
		if(es705_priv.rx1_route_enable)
			es705_priv.power_control(ES705_SET_POWER_STATE_NORMAL,
						 ES705_POWER_STATE);
		else if (!(es705_priv.rx1_route_enable ||
				es705_priv.rx2_route_enable ||
				es705_priv.tx1_route_enable))
			es705_priv.power_control(ES705_SET_POWER_STATE_SLEEP,
						ES705_POWER_STATE);
	}
#endif
	return 0;
}

static int es705_get_tx1_route_enable_value(struct snd_kcontrol *kcontrol,
				       struct snd_ctl_elem_value *ucontrol)
{
	ucontrol->value.integer.value[0] = es705_priv.tx1_route_enable;
	dev_dbg(es705_priv.dev, "%s(): tx1_route_enable = %d\n",
		__func__, es705_priv.tx1_route_enable);

	return 0;
}

static int es705_put_tx1_route_enable_value(struct snd_kcontrol *kcontrol,
				       struct snd_ctl_elem_value *ucontrol)
{
	es705_priv.tx1_route_enable = ucontrol->value.integer.value[0];
	dev_dbg(es705_priv.dev, "%s(): tx1_route_enable = %d\n",
		__func__, es705_priv.tx1_route_enable);

#if defined(SAMSUNG_ES705_FEATURE)
	if (es705_priv.power_control) {
		if(es705_priv.tx1_route_enable)
			es705_priv.power_control(ES705_SET_POWER_STATE_NORMAL,
						 ES705_POWER_STATE);
		else if (!(es705_priv.rx1_route_enable ||
				es705_priv.rx2_route_enable ||
				es705_priv.tx1_route_enable))
			es705_priv.power_control(ES705_SET_POWER_STATE_SLEEP,
						ES705_POWER_STATE);
	}
#endif
return 0;
}

static int es705_get_rx2_route_enable_value(struct snd_kcontrol *kcontrol,
				       struct snd_ctl_elem_value *ucontrol)
{
	ucontrol->value.integer.value[0] = es705_priv.rx2_route_enable;
	dev_dbg(es705_priv.dev, "%s(): rx2_route_enable = %d\n",
		__func__, es705_priv.rx2_route_enable);

	return 0;
}

static int es705_put_rx2_route_enable_value(struct snd_kcontrol *kcontrol,
				       struct snd_ctl_elem_value *ucontrol)
{
	es705_priv.rx2_route_enable = ucontrol->value.integer.value[0];
	dev_dbg(es705_priv.dev, "%s(): rx2_route_enable = %d\n",
		__func__, es705_priv.rx2_route_enable);

#if defined(SAMSUNG_ES705_FEATURE)
	if (es705_priv.power_control) {
		if(es705_priv.rx2_route_enable)
			es705_priv.power_control(ES705_SET_POWER_STATE_NORMAL,
						ES705_POWER_STATE);
		else if (!(es705_priv.rx1_route_enable ||
				es705_priv.rx2_route_enable ||
				es705_priv.tx1_route_enable))
			es705_priv.power_control(ES705_SET_POWER_STATE_SLEEP,
						ES705_POWER_STATE);
	}
#endif
return 0;
}

#if defined(SAMSUNG_ES705_FEATURE)
static int es705_get_voice_wakeup_enable_value(struct snd_kcontrol *kcontrol,
				       struct snd_ctl_elem_value *ucontrol)
{
	ucontrol->value.integer.value[0] = es705_priv.voice_wakeup_enable;
	dev_dbg(es705_priv.dev, "%s(): voice_wakeup_enable = %d\n",
		__func__, es705_priv.voice_wakeup_enable);

	return 0;
}

static int es705_put_voice_wakeup_enable_value(struct snd_kcontrol *kcontrol,
				       struct snd_ctl_elem_value *ucontrol)
{
	int rc = 0;
	u32 sync_cmd = (ES705_SYNC_CMD << 16) | ES705_SYNC_POLLING;
	u32 sync_rspn = sync_cmd;
	int match = 1;

	es705_priv.voice_wakeup_enable = ucontrol->value.integer.value[0];
	dev_dbg(es705_priv.dev, "%s(): voice_wakeup_enable = %d\n",
		__func__, es705_priv.voice_wakeup_enable);

	if (es705_priv.power_control) {
		if(es705_priv.voice_wakeup_enable) {
			es705_priv.power_control(ES705_SET_POWER_STATE_VS_OVERLAY, ES705_POWER_STATE);
			es705_switch_route_config(27);
			rc = es705_write_then_read(&es705_priv, &sync_cmd, sizeof(sync_cmd),
					 &sync_rspn, match);
			if (rc) {
				dev_err(es705_priv.dev, "%s(): es705 Sync FAIL\n", __func__);
				return rc;
			}
			rc = es705_write_sensory_vs_keyword();
			if (rc) {
				dev_err(es705_priv.dev, "%s(): es705 keyword download FAIL\n", __func__);
				return rc;
			}
		}
		else {
			if (es705_priv.es705_power_state == ES705_SET_POWER_STATE_VS_LOWPWR)
				es705_priv.power_control(ES705_SET_POWER_STATE_VS_OVERLAY, ES705_POWER_STATE);
			es705_switch_route_config(5);
			es705_priv.power_control(ES705_SET_POWER_STATE_NORMAL, ES705_POWER_STATE);
			es705_read_write_power_control(0);

		}
	}
	return rc;
}

static int es705_get_voice_lpm_enable_value(struct snd_kcontrol *kcontrol,
				       struct snd_ctl_elem_value *ucontrol)
{
	ucontrol->value.integer.value[0] = es705_priv.voice_lpm_enable;
	dev_dbg(es705_priv.dev, "%s(): voice_lpm_enable = %d\n",
		__func__, es705_priv.voice_lpm_enable);

	return 0;
}

static int es705_put_voice_lpm_enable_value(struct snd_kcontrol *kcontrol,
				       struct snd_ctl_elem_value *ucontrol)
{
	es705_priv.voice_lpm_enable = ucontrol->value.integer.value[0];

	dev_dbg(es705_priv.dev, "%s(): voice_lpm_enable = %d\n",
		__func__, es705_priv.voice_lpm_enable);

	if (!es705_priv.voice_lpm_enable)
		return 0;

	if (es705_priv.power_control)
		es705_priv.power_control(ES705_SET_POWER_STATE_VS_LOWPWR, ES705_POWER_STATE);
	return 0;
}
#endif

#if !defined(SAMSUNG_ES705_FEATURE)
static int es705_get_wakeup_method_value(struct snd_kcontrol *kcontrol,
				       struct snd_ctl_elem_value *ucontrol)
{
	ucontrol->value.integer.value[0] = es705_priv.wakeup_method;
	dev_dbg(es705_priv.dev, "%s(): es705 wakeup method by %s\n",
		__func__, es705_priv.wakeup_method ?
		"UART" : "wakeup GPIO");
	return 0;
}

static int es705_put_wakeup_method_value(struct snd_kcontrol *kcontrol,
				       struct snd_ctl_elem_value *ucontrol)
{
	es705_priv.wakeup_method = ucontrol->value.integer.value[0];
	dev_dbg(es705_priv.dev, "%s(): enable es705 wakeup by %s\n",
		__func__, es705_priv.wakeup_method ?
		"UART" : "wakeup GPIO");
	return 0;
}
#endif

static int es705_get_ns_value(struct snd_kcontrol *kcontrol,
				       struct snd_ctl_elem_value *ucontrol)
{
	dev_dbg(es705_priv.dev, "%s(): NS = %d\n",
		__func__, es705_priv.ns);
	ucontrol->value.enumerated.item[0] = es705_priv.ns;

	return 0;
}

static int es705_put_ns_value(struct snd_kcontrol *kcontrol,
				       struct snd_ctl_elem_value *ucontrol)
{
	int value = ucontrol->value.enumerated.item[0];
	int rc = 0;
	dev_dbg(es705_priv.dev, "%s(): NS = %d\n", __func__, value);

	es705_priv.ns = value;

	/* 0 = NS off, 1 = NS on*/
	if (value)
		rc = es705_write(NULL, ES705_PRESET,
			ES705_NS_ON_PRESET);
	else
		rc = es705_write(NULL, ES705_PRESET,
			ES705_NS_OFF_PRESET);

	return rc;
}

static int es705_get_aud_zoom(struct snd_kcontrol *kcontrol,
				       struct snd_ctl_elem_value *ucontrol)
{
	dev_dbg(es705_priv.dev, "%s(): Zoom = %d\n",
		__func__, es705_priv.zoom);
	ucontrol->value.enumerated.item[0] = es705_priv.zoom;

	return 0;
}

static int es705_put_aud_zoom(struct snd_kcontrol *kcontrol,
				       struct snd_ctl_elem_value *ucontrol)
{
	int value = ucontrol->value.enumerated.item[0];
	int rc = 0;
	dev_dbg(es705_priv.dev, "%s(): Zoom = %d\n", __func__, value);

	es705_priv.zoom = value;

	if (value == ES705_AUD_ZOOM_NARRATOR) {
		rc = es705_write(NULL, ES705_PRESET,
			ES705_AUD_ZOOM_PRESET);
		rc = es705_write(NULL, ES705_PRESET,
			ES705_AUD_ZOOM_NARRATOR_PRESET);
	} else if (value == ES705_AUD_ZOOM_SCENE) {
		rc = es705_write(NULL, ES705_PRESET,
			ES705_AUD_ZOOM_PRESET);
		rc = es705_write(NULL, ES705_PRESET,
			ES705_AUD_ZOOM_SCENE_PRESET);
	} else if (value == ES705_AUD_ZOOM_NARRATION) {
		rc = es705_write(NULL, ES705_PRESET,
			ES705_AUD_ZOOM_PRESET);
		rc = es705_write(NULL, ES705_PRESET,
			ES705_AUD_ZOOM_NARRATION_PRESET);
	} else
		rc = es705_write(NULL, ES705_PRESET, 0);

	return rc;
}

int es705_remote_route_enable(struct snd_soc_dai *dai)
{
	dev_dbg(es705_priv.dev, "%s():dai->name = %s dai->id = %d\n",
		__func__, dai->name, dai->id);

	switch (dai->id) {
	case ES705_SLIM_1_PB:
		return es705_priv.rx1_route_enable;
	case ES705_SLIM_1_CAP:
		return es705_priv.tx1_route_enable;
	case ES705_SLIM_2_PB:
		return es705_priv.rx2_route_enable;
	default:
		return 0;
	}
}
EXPORT_SYMBOL_GPL(es705_remote_route_enable);

static int es705_put_internal_route_config(struct snd_kcontrol *kcontrol,
					   struct snd_ctl_elem_value *ucontrol)
{
	es705_switch_route_config(ucontrol->value.integer.value[0]);

	return 0;
}

static int es705_get_internal_route_config(struct snd_kcontrol *kcontrol,
					   struct snd_ctl_elem_value *ucontrol)
{
	struct es705_priv *es705 = &es705_priv;

	ucontrol->value.integer.value[0] = es705->internal_route_num;

	return 0;
}

static int es705_put_network_type(struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol)
{
	struct es705_priv *es705 = &es705_priv;

	dev_info(es705->dev, "%s():network type = %ld pm_state = %d\n",
		__func__, ucontrol->value.integer.value[0], es705->pm_state);

	if (ucontrol->value.integer.value[0] == WIDE_BAND)
		network_type = WIDE_BAND;
	else
		network_type = NARROW_BAND;

	mutex_lock(&es705->pm_mutex);
	if (es705->pm_state == ES705_POWER_AWAKE)
		es705_switch_route_config(es705->internal_route_num);

	mutex_unlock(&es705->pm_mutex);

	return 0;
}

static int es705_get_network_type(struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol)
{
	ucontrol->value.integer.value[0] = network_type;

	return 0;
}

static int es705_put_internal_route(struct snd_kcontrol *kcontrol,
				    struct snd_ctl_elem_value *ucontrol)
{
	pr_info("%s : put internal route = %ld\n", __func__, ucontrol->value.integer.value[0]);
	es705_switch_route(ucontrol->value.integer.value[0]);
	return 0;
}

static int es705_get_internal_route(struct snd_kcontrol *kcontrol,
				    struct snd_ctl_elem_value *ucontrol)
{
	struct es705_priv *es705 = &es705_priv;

	ucontrol->value.integer.value[0] = es705->internal_route_num;

	return 0;
}

static int es705_put_internal_rate(struct snd_kcontrol *kcontrol,
				   struct snd_ctl_elem_value *ucontrol)
{
	struct es705_priv *es705 = &es705_priv;
	const u32 *rate_macro = NULL;
	int rc = 0;

	dev_dbg(es705->dev, "%s():es705->internal_rate = %d ucontrol = %d\n",
		__func__, (int)es705->internal_rate,
		(int)ucontrol->value.enumerated.item[0]);

	switch (ucontrol->value.enumerated.item[0]) {
	case RATE_NB:
		rate_macro = es705_route_config[es705->internal_route_num].nb;
		break;
	case RATE_WB:
		rate_macro = es705_route_config[es705->internal_route_num].wb;
		break;
	case RATE_SWB:
		rate_macro = es705_route_config[es705->internal_route_num].swb;
		break;
	case RATE_FB:
		rate_macro = es705_route_config[es705->internal_route_num].fb;
		break;
	default:
		break;
	}

	if (!rate_macro) {
		dev_err(es705->dev, "%s(): internal rate, %d, out of range\n",
			__func__, ucontrol->value.enumerated.item[0]);
		return -EINVAL;
	}

	es705->internal_rate = ucontrol->value.enumerated.item[0];
	rc = es705_write_block(es705, rate_macro);

	return rc;
}

static int es705_get_internal_rate(struct snd_kcontrol *kcontrol,
				   struct snd_ctl_elem_value *ucontrol)
{
	struct es705_priv *es705 = &es705_priv;

	ucontrol->value.enumerated.item[0] = es705->internal_rate;
	dev_dbg(es705->dev, "%s():es705->internal_rate = %d ucontrol = %d\n",
		__func__, (int)es705->internal_rate,
		(int)ucontrol->value.enumerated.item[0]);

	return 0;
}

static int es705_get_audio_custom_profile(struct snd_kcontrol *kcontrol,
					  struct snd_ctl_elem_value *ucontrol)
{
	return 0;
}

static int es705_put_audio_custom_profile(struct snd_kcontrol *kcontrol,
					  struct snd_ctl_elem_value *ucontrol)
{
	int index = ucontrol->value.integer.value[0];

	if (index < ES705_CUSTOMER_PROFILE_MAX)
		es705_write_block(&es705_priv,
				  &es705_audio_custom_profiles[index][0]);
	return 0;
}

static int es705_ap_put_tx1_ch_cnt(struct snd_kcontrol *kcontrol,
				   struct snd_ctl_elem_value *ucontrol)
{
	es705_priv.ap_tx1_ch_cnt = ucontrol->value.enumerated.item[0] + 1;
	return 0;
}

static int es705_ap_get_tx1_ch_cnt(struct snd_kcontrol *kcontrol,
				   struct snd_ctl_elem_value *ucontrol)
{
	struct es705_priv *es705 = &es705_priv;

	ucontrol->value.enumerated.item[0] = es705->ap_tx1_ch_cnt - 1;

	return 0;
}

static const char * const es705_ap_tx1_ch_cnt_texts[] = {
	"One", "Two"
};
static const struct soc_enum es705_ap_tx1_ch_cnt_enum =
	SOC_ENUM_SINGLE(SND_SOC_NOPM, 0,
			ARRAY_SIZE(es705_ap_tx1_ch_cnt_texts),
			es705_ap_tx1_ch_cnt_texts);

static const char * const es705_vs_power_state_texts[] = {
	"None", "Sleep", "MP_Sleep", "MP_Cmd", "Normal", "Overlay", "Low_Power"
};
static const struct soc_enum es705_vs_power_state_enum =
	SOC_ENUM_SINGLE(ES705_POWER_STATE, 0,
			ARRAY_SIZE(es705_vs_power_state_texts),
			es705_vs_power_state_texts);

/* generic gain translation */
static int es705_index_to_gain(int min, int step, int index)
{
	return	min + (step * index);
}
static int es705_gain_to_index(int min, int step, int gain)
{
	return	(gain - min) / step;
}

/* dereverb gain */
static int es705_put_dereverb_gain_value(struct snd_kcontrol *kcontrol,
					 struct snd_ctl_elem_value *ucontrol)
{
	struct soc_mixer_control *mc =
		(struct soc_mixer_control *)kcontrol->private_value;
	unsigned int reg = mc->reg;
	unsigned int value;
	int rc = 0;

	if (ucontrol->value.integer.value[0] <= 12) {
		value = es705_index_to_gain(-12, 1, ucontrol->value.integer.value[0]);
		rc = es705_write(NULL, reg, value);
	}

	return rc;
}

static int es705_get_dereverb_gain_value(struct snd_kcontrol *kcontrol,
					 struct snd_ctl_elem_value *ucontrol)
{
	struct soc_mixer_control *mc =
		(struct soc_mixer_control *)kcontrol->private_value;
	unsigned int reg = mc->reg;
	unsigned int value;

#if defined(SAMSUNG_ES705_FEATURE)
	es705_read_write_power_control(1);
#endif
	value = es705_read(NULL, reg);
	ucontrol->value.integer.value[0] = es705_gain_to_index(-12, 1, value);
#if defined(SAMSUNG_ES705_FEATURE)
	es705_read_write_power_control(0);
#endif
	return 0;
}

/* bwe high band gain */
static int es705_put_bwe_high_band_gain_value(struct snd_kcontrol *kcontrol,
					      struct snd_ctl_elem_value *ucontrol)
{
	struct soc_mixer_control *mc =
		(struct soc_mixer_control *)kcontrol->private_value;
	unsigned int reg = mc->reg;
	unsigned int value;
	int rc = 0;

	if (ucontrol->value.integer.value[0] <= 30) {
		value = es705_index_to_gain(-10, 1, ucontrol->value.integer.value[0]);
		rc = es705_write(NULL, reg, value);
	}

	return 0;
}

static int es705_get_bwe_high_band_gain_value(struct snd_kcontrol *kcontrol,
					      struct snd_ctl_elem_value *ucontrol)
{
	struct soc_mixer_control *mc =
		(struct soc_mixer_control *)kcontrol->private_value;
	unsigned int reg = mc->reg;
	unsigned int value;

#if defined(SAMSUNG_ES705_FEATURE)
	es705_read_write_power_control(1);
#endif
	value = es705_read(NULL, reg);
	ucontrol->value.integer.value[0] = es705_gain_to_index(-10, 1, value);
#if defined(SAMSUNG_ES705_FEATURE)
	es705_read_write_power_control(0);
#endif

	return 0;
}

/* bwe max snr */
static int es705_put_bwe_max_snr_value(struct snd_kcontrol *kcontrol,
				       struct snd_ctl_elem_value *ucontrol)
{
	struct soc_mixer_control *mc =
		(struct soc_mixer_control *)kcontrol->private_value;
	unsigned int reg = mc->reg;
	unsigned int value;
	int rc = 0;

	if (ucontrol->value.integer.value[0] <= 70) {
		value = es705_index_to_gain(-20, 1, ucontrol->value.integer.value[0]);
		rc = es705_write(NULL, reg, value);
	}

	return 0;
}

static int es705_get_bwe_max_snr_value(struct snd_kcontrol *kcontrol,
				       struct snd_ctl_elem_value *ucontrol)
{
	struct soc_mixer_control *mc =
		(struct soc_mixer_control *)kcontrol->private_value;
	unsigned int reg = mc->reg;
	unsigned int value;

#if defined(SAMSUNG_ES705_FEATURE)
	es705_read_write_power_control(1);
#endif
	value = es705_read(NULL, reg);
	ucontrol->value.integer.value[0] = es705_gain_to_index(-20, 1, value);
#if defined(SAMSUNG_ES705_FEATURE)
	es705_read_write_power_control(0);
#endif

	return 0;
}

static const char * const es705_mic_config_texts[] = {
	"CT 2-mic", "FT 2-mic", "DV 1-mic", "EXT 1-mic", "BT 1-mic",
	"CT ASR 2-mic", "FT ASR 2-mic", "EXT ASR 1-mic", "FT ASR 1-mic",
};
static const struct soc_enum es705_mic_config_enum =
	SOC_ENUM_SINGLE(ES705_MIC_CONFIG, 0,
			ARRAY_SIZE(es705_mic_config_texts),
			es705_mic_config_texts);

static const char * const es705_aec_mode_texts[] = {
	"Off", "On", "rsvrd2", "rsvrd3", "rsvrd4", "On half-duplex"
};
static const struct soc_enum es705_aec_mode_enum =
	SOC_ENUM_SINGLE(ES705_AEC_MODE, 0, ARRAY_SIZE(es705_aec_mode_texts),
			es705_aec_mode_texts);

static const char * const es705_algo_rates_text[] = {
	"fs=8khz", "fs=16khz", "fs=24khz", "fs=48khz", "fs=96khz", "fs=192khz"
};
static const struct soc_enum es705_algo_sample_rate_enum =
	SOC_ENUM_SINGLE(ES705_ALGO_SAMPLE_RATE, 0,
			ARRAY_SIZE(es705_algo_rates_text),
			es705_algo_rates_text);
static const struct soc_enum es705_algo_mix_rate_enum =
	SOC_ENUM_SINGLE(ES705_MIX_SAMPLE_RATE, 0,
			ARRAY_SIZE(es705_algo_rates_text),
			es705_algo_rates_text);

static const char * const es705_internal_rate_text[] = {
	"NB", "WB", "SWB", "FB"
};
static const struct soc_enum es705_internal_rate_enum =
	SOC_ENUM_SINGLE(SND_SOC_NOPM, 0,
			ARRAY_SIZE(es705_internal_rate_text),
			es705_internal_rate_text);

static const char * const es705_algorithms_text[] = {
	"None", "VP", "Two CHREC", "AUDIO", "Four CHPASS"
};
static const struct soc_enum es705_algorithms_enum =
	SOC_ENUM_SINGLE(ES705_ALGO_SAMPLE_RATE, 0,
			ARRAY_SIZE(es705_algorithms_text),
			es705_algorithms_text);
static const char * const es705_off_on_texts[] = {
	"Off", "On"
};
static const char * const es705_audio_zoom_texts[] = {
	"disabled", "Narrator", "Scene", "Narration"
};
static const struct soc_enum es705_veq_enable_enum =
	SOC_ENUM_SINGLE(ES705_VEQ_ENABLE, 0, ARRAY_SIZE(es705_off_on_texts),
			es705_off_on_texts);
static const struct soc_enum es705_dereverb_enable_enum =
	SOC_ENUM_SINGLE(ES705_DEREVERB_ENABLE, 0,
			ARRAY_SIZE(es705_off_on_texts),
			es705_off_on_texts);
static const struct soc_enum es705_bwe_enable_enum =
	SOC_ENUM_SINGLE(ES705_BWE_ENABLE, 0, ARRAY_SIZE(es705_off_on_texts),
			es705_off_on_texts);
static const struct soc_enum es705_bwe_post_eq_enable_enum =
	SOC_ENUM_SINGLE(ES705_BWE_POST_EQ_ENABLE, 0,
			ARRAY_SIZE(es705_off_on_texts),
			es705_off_on_texts);
static const struct soc_enum es705_algo_processing_enable_enum =
	SOC_ENUM_SINGLE(ES705_ALGO_PROCESSING, 0,
			ARRAY_SIZE(es705_off_on_texts),
			es705_off_on_texts);
static const struct soc_enum es705_ns_enable_enum =
	SOC_ENUM_SINGLE(ES705_PRESET, 0, ARRAY_SIZE(es705_off_on_texts),
			es705_off_on_texts);
static const struct soc_enum es705_audio_zoom_enum =
	SOC_ENUM_SINGLE(ES705_PRESET, 0, ARRAY_SIZE(es705_audio_zoom_texts),
			es705_audio_zoom_texts);

static int es705_put_power_state_enum(struct snd_kcontrol *kcontrol,
				      struct snd_ctl_elem_value *ucontrol)
{
	return 0;
}

static int es705_get_power_state_enum(struct snd_kcontrol *kcontrol,
				      struct snd_ctl_elem_value *ucontrol)
{
	return 0;
}
static const char * const es705_power_state_texts[] = {
	"Sleep", "Active"
};
static const struct soc_enum es705_power_state_enum =
	SOC_ENUM_SINGLE(SND_SOC_NOPM, 0,
			ARRAY_SIZE(es705_power_state_texts),
			es705_power_state_texts);

static int es705_get_vs_enable(struct snd_kcontrol *kcontrol,
			       struct snd_ctl_elem_value *ucontrol)
{
	ucontrol->value.enumerated.item[0] = es705_priv.vs_enable;
	return 0;
}
static int es705_put_vs_enable(struct snd_kcontrol *kcontrol,
			       struct snd_ctl_elem_value *ucontrol)
{
	es705_priv.vs_enable = ucontrol->value.enumerated.item[0];
	es705_priv.vs_streaming(&es705_priv);
	return 0;
}

static int es705_get_vs_wakeup_keyword(struct snd_kcontrol *kcontrol,
				       struct snd_ctl_elem_value *ucontrol)
{
	ucontrol->value.enumerated.item[0] = es705_priv.vs_wakeup_keyword;
	return 0;
}

static int es705_put_vs_wakeup_keyword(struct snd_kcontrol *kcontrol,
				       struct snd_ctl_elem_value *ucontrol)
{
	es705_priv.vs_wakeup_keyword = ucontrol->value.enumerated.item[0];
	return 0;
}


static int es705_put_vs_stored_keyword(struct snd_kcontrol *kcontrol,
				      struct snd_ctl_elem_value *ucontrol)
{
	int op;
	int ret;

	op = ucontrol->value.enumerated.item[0];
	dev_dbg(es705_priv.dev, "%s(): op=%d\n", __func__, op);

	ret = 0;
	switch (op) {
	case 0:
		dev_dbg(es705_priv.dev, "%s(): keyword params put...\n",
			__func__);
		ret = es705_write_vs_data_block(&es705_priv);
		break;
	case 1:
		dev_dbg(es705_priv.dev, "%s(): keyword params get...\n",
			__func__);
		ret = es705_read_vs_data_block(&es705_priv);
		break;
	case 2:
		dev_dbg(es705_priv.dev, "%s(): keyword params clear...\n",
			__func__);
		es705_priv.vs_keyword_param_size = 0;
		break;
	default:
		BUG_ON(0);
	};

	return ret;
}

/* Voice Sense Detection Sensitivity */
static
int es705_put_vs_detection_sensitivity(struct snd_kcontrol *kcontrol,
					struct snd_ctl_elem_value *ucontrol)
{
	struct soc_mixer_control *mc =
		(struct soc_mixer_control *)kcontrol->private_value;
	unsigned int reg = mc->reg;
	unsigned int value;
	int rc = 0;

	value = ucontrol->value.integer.value[0];
	dev_dbg(es705_priv.dev, "%s(): ucontrol = %ld value = %d\n",
		__func__, ucontrol->value.integer.value[0], value);

	rc = es705_write(NULL, reg, value);

	return rc;
}

static
int es705_get_vs_detection_sensitivity(struct snd_kcontrol *kcontrol,
					struct snd_ctl_elem_value *ucontrol)
{
	struct soc_mixer_control *mc =
		(struct soc_mixer_control *)kcontrol->private_value;
	unsigned int reg = mc->reg;
	unsigned int value;

#if defined(SAMSUNG_ES705_FEATURE)
	es705_read_write_power_control(1);
#endif
	value = es705_read(NULL, reg);
	ucontrol->value.integer.value[0] = value;

	dev_dbg(es705_priv.dev, "%s(): value = %d ucontrol = %ld\n",
		__func__, value, ucontrol->value.integer.value[0]);
#if defined(SAMSUNG_ES705_FEATURE)
	es705_read_write_power_control(0);
#endif

	return 0;
}

static
int es705_put_vad_sensitivity(struct snd_kcontrol *kcontrol,
					struct snd_ctl_elem_value *ucontrol)
{
	struct soc_mixer_control *mc =
		(struct soc_mixer_control *)kcontrol->private_value;
	unsigned int reg = mc->reg;
	unsigned int value;
	int rc = 0;

	value = ucontrol->value.integer.value[0];

	dev_dbg(es705_priv.dev, "%s(): ucontrol = %ld value = %d\n",
		__func__, ucontrol->value.integer.value[0], value);

	rc = es705_write(NULL, reg, value);

	return rc;
}

static
int es705_get_vad_sensitivity(struct snd_kcontrol *kcontrol,
					struct snd_ctl_elem_value *ucontrol)
{
	struct soc_mixer_control *mc =
		(struct soc_mixer_control *)kcontrol->private_value;
	unsigned int reg = mc->reg;
	unsigned int value;

#if defined(SAMSUNG_ES705_FEATURE)
	es705_read_write_power_control(1);
#endif
	value = es705_read(NULL, reg);
	ucontrol->value.integer.value[0] = value;

	dev_dbg(es705_priv.dev, "%s(): value = %d ucontrol = %ld\n",
		__func__, value, ucontrol->value.integer.value[0]);
#if defined(SAMSUNG_ES705_FEATURE)
	es705_read_write_power_control(0);
#endif

	return 0;
}

static const char * const es705_vs_wakeup_keyword_texts[] = {
	"Default", "One", "Two", "Three", "Four"
};
static const struct soc_enum es705_vs_wakeup_keyword_enum =
	SOC_ENUM_SINGLE(ES705_VOICE_SENSE_SET_KEYWORD, 0,
			ARRAY_SIZE(es705_vs_wakeup_keyword_texts),
			es705_vs_wakeup_keyword_texts);

static const char * const es705_vs_event_texts[] = {
	"No Event", "Codec Event", "VS Keyword Event",
};
static const struct soc_enum es705_vs_event_enum =
	SOC_ENUM_SINGLE(ES705_VOICE_SENSE_EVENT, 0,
			ARRAY_SIZE(es705_vs_event_texts),
			es705_vs_event_texts);

static const char * const es705_vs_training_status_texts[] = {
	"Training", "Done",
};
static const struct soc_enum es705_vs_training_status_enum =
	SOC_ENUM_SINGLE(ES705_VOICE_SENSE_TRAINING_STATUS, 0,
			ARRAY_SIZE(es705_vs_training_status_texts),
			es705_vs_training_status_texts);

static const char * const es705_vs_training_record_texts[] = {
	"Stop", "Start",
};


static const char * const es705_vs_stored_keyword_texts[] = {
	"Put", "Get", "Clear"
};

static const struct soc_enum es705_vs_stored_keyword_enum =
	SOC_ENUM_SINGLE(ES705_VS_STORED_KEYWORD, 0,
			ARRAY_SIZE(es705_vs_stored_keyword_texts),
			es705_vs_stored_keyword_texts);

static const struct soc_enum es705_vs_training_record_enum =
	SOC_ENUM_SINGLE(ES705_VOICE_SENSE_TRAINING_RECORD, 0,
			ARRAY_SIZE(es705_vs_training_record_texts),
			es705_vs_training_record_texts);

static const char * const es705_vs_training_mode_texts[] = {
	"Detect builtin keyword", "Train keyword", "Detect user keyword"
};

static const struct soc_enum es705_vs_training_mode_enum =
	SOC_ENUM_SINGLE(ES705_VOICE_SENSE_TRAINING_MODE, 0,
			ARRAY_SIZE(es705_vs_training_mode_texts),
			es705_vs_training_mode_texts);


static struct snd_kcontrol_new es705_digital_ext_snd_controls[] = {
	SOC_SINGLE_EXT("ES705 RX1 Enable", SND_SOC_NOPM, 0, 1, 0,
		       es705_get_rx1_route_enable_value,
		       es705_put_rx1_route_enable_value),
	SOC_SINGLE_EXT("ES705 TX1 Enable", SND_SOC_NOPM, 0, 1, 0,
		       es705_get_tx1_route_enable_value,
		       es705_put_tx1_route_enable_value),
	SOC_SINGLE_EXT("ES705 RX2 Enable", SND_SOC_NOPM, 0, 1, 0,
		       es705_get_rx2_route_enable_value,
		       es705_put_rx2_route_enable_value),
	SOC_ENUM_EXT("Mic Config", es705_mic_config_enum,
		     es705_get_control_enum, es705_put_control_enum),
	SOC_ENUM_EXT("AEC Mode", es705_aec_mode_enum,
		     es705_get_control_enum, es705_put_control_enum),
	SOC_ENUM_EXT("VEQ Enable", es705_veq_enable_enum,
		     es705_get_control_enum, es705_put_control_enum),
	SOC_ENUM_EXT("Dereverb Enable", es705_dereverb_enable_enum,
		     es705_get_control_enum, es705_put_control_enum),
	SOC_SINGLE_EXT("Dereverb Gain",
		       ES705_DEREVERB_GAIN, 0, 100, 0,
		       es705_get_dereverb_gain_value, es705_put_dereverb_gain_value),
	SOC_ENUM_EXT("BWE Enable", es705_bwe_enable_enum,
		     es705_get_control_enum, es705_put_control_enum),
	SOC_SINGLE_EXT("BWE High Band Gain",
		       ES705_BWE_HIGH_BAND_GAIN, 0, 100, 0,
		       es705_get_bwe_high_band_gain_value,
		       es705_put_bwe_high_band_gain_value),
	SOC_SINGLE_EXT("BWE Max SNR",
		       ES705_BWE_MAX_SNR, 0, 100, 0,
		       es705_get_bwe_max_snr_value, es705_put_bwe_max_snr_value),
	SOC_ENUM_EXT("BWE Post EQ Enable", es705_bwe_post_eq_enable_enum,
		     es705_get_control_enum, es705_put_control_enum),
	SOC_SINGLE_EXT("SLIMbus Link Multi Channel",
		       ES705_SLIMBUS_LINK_MULTI_CHANNEL, 0, 65535, 0,
		       es705_get_control_value, es705_put_control_value),
	SOC_ENUM_EXT("Set Power State", es705_power_state_enum,
		       es705_get_power_state_enum, es705_put_power_state_enum),
	SOC_ENUM_EXT("Algorithm Processing", es705_algo_processing_enable_enum,
		     es705_get_control_enum, es705_put_control_enum),
	SOC_ENUM_EXT("Algorithm Sample Rate", es705_algo_sample_rate_enum,
		     es705_get_control_enum, es705_put_control_enum),
	SOC_ENUM_EXT("Algorithm", es705_algorithms_enum,
		     es705_get_control_enum, es705_put_control_enum),
	SOC_ENUM_EXT("Mix Sample Rate", es705_algo_mix_rate_enum,
		     es705_get_control_enum, es705_put_control_enum),
	SOC_SINGLE_EXT("Internal Route",
		       SND_SOC_NOPM, 0, 100, 0, es705_get_internal_route,
		       es705_put_internal_route),
	SOC_ENUM_EXT("Internal Rate", es705_internal_rate_enum,
		      es705_get_internal_rate,
		      es705_put_internal_rate),
	SOC_SINGLE_EXT("Preset",
		       ES705_PRESET, 0, 2047, 0, es705_get_control_value,
		       es705_put_control_value),
	SOC_SINGLE_EXT("Audio Custom Profile",
		       SND_SOC_NOPM, 0, 100, 0, es705_get_audio_custom_profile,
		       es705_put_audio_custom_profile),
	SOC_ENUM_EXT("ES705-AP Tx Channels", es705_ap_tx1_ch_cnt_enum,
		     es705_ap_get_tx1_ch_cnt, es705_ap_put_tx1_ch_cnt),
	SOC_SINGLE_EXT("Voice Sense Enable",
		       ES705_VOICE_SENSE_ENABLE, 0, 1, 0,
		       es705_get_vs_enable, es705_put_vs_enable),
	SOC_ENUM_EXT("Voice Sense Set Wakeup Word",
		     es705_vs_wakeup_keyword_enum,
		     es705_get_vs_wakeup_keyword, es705_put_vs_wakeup_keyword),
	SOC_ENUM_EXT("Voice Sense Status",
		     es705_vs_event_enum,
		     es705_get_control_enum, NULL),
	SOC_ENUM_EXT("Voice Sense Training Mode",
			 es705_vs_training_mode_enum,
			 es705_get_control_enum, es705_put_control_enum),
	SOC_ENUM_EXT("Voice Sense Training Status",
		     es705_vs_training_status_enum,
		     es705_get_control_enum, NULL),
	SOC_ENUM_EXT("Voice Sense Training Record",
		     es705_vs_training_record_enum,
		     NULL, es705_put_control_enum),
	SOC_ENUM_EXT("Voice Sense Stored Keyword",
		     es705_vs_stored_keyword_enum,
		     NULL, es705_put_vs_stored_keyword),
	SOC_SINGLE_EXT("Voice Sense Detect Sensitivity",
			ES705_VOICE_SENSE_DETECTION_SENSITIVITY, 0, 10, 0,
			es705_get_vs_detection_sensitivity,
			es705_put_vs_detection_sensitivity),
	SOC_SINGLE_EXT("Voice Activity Detect Sensitivity",
			ES705_VOICE_ACTIVITY_DETECTION_SENSITIVITY, 0, 10, 0,
			es705_get_vad_sensitivity,
			es705_put_vad_sensitivity),
	SOC_ENUM_EXT("ES705 Power State", es705_vs_power_state_enum,
		     es705_get_power_control_enum,
		     es705_put_power_control_enum),
	SOC_ENUM_EXT("Noise Suppression", es705_ns_enable_enum,
		       es705_get_ns_value,
		       es705_put_ns_value),
	SOC_ENUM_EXT("Audio Zoom", es705_audio_zoom_enum,
		       es705_get_aud_zoom,
		       es705_put_aud_zoom),
	SOC_SINGLE_EXT("UART FW Download Rate", SND_SOC_NOPM,
			0, 3, 0,
			es705_get_uart_fw_download_rate,
			es705_put_uart_fw_download_rate),
	SOC_SINGLE_EXT("Voice Sense Stream Enable", ES705_VS_STREAM_ENABLE,
		       0, 1, 0,
		       es705_get_vs_stream_enable, es705_put_vs_stream_enable),
#if defined(SAMSUNG_ES705_FEATURE)
	SOC_SINGLE_EXT("ES705 Voice Wakeup Enable", SND_SOC_NOPM, 0, 1, 0,
		       es705_get_voice_wakeup_enable_value,
		       es705_put_voice_wakeup_enable_value),
	SOC_SINGLE_EXT("ES705 Voice LPM Enable", SND_SOC_NOPM, 0, 1, 0,
		       es705_get_voice_lpm_enable_value,
		       es705_put_voice_lpm_enable_value),
	SOC_SINGLE_EXT("Internal Route Config", SND_SOC_NOPM, 0, 100, 0,
				es705_get_internal_route_config,
				es705_put_internal_route_config),
	SOC_SINGLE_EXT("Current Network Type", SND_SOC_NOPM, 0, 1, 0,
				es705_get_network_type,
				es705_put_network_type)
#endif
#if !defined(SAMSUNG_ES705_FEATURE)
	SOC_SINGLE_EXT("ES705 Wakeup Method", SND_SOC_NOPM, 0, 1, 0,
			es705_get_wakeup_method_value,
			es705_put_wakeup_method_value),
#endif
};

static int es705_set_bias_level(struct snd_soc_codec *codec,
				enum snd_soc_bias_level level)
{
	int rc = 0;

	switch (level) {
	case SND_SOC_BIAS_ON:
		break;

	case SND_SOC_BIAS_PREPARE:
		break;

	case SND_SOC_BIAS_STANDBY:
		break;

	case SND_SOC_BIAS_OFF:
		break;
	}
	codec->dapm.bias_level = level;

	return rc;
}

#define ES705_RATES (SNDRV_PCM_RATE_8000 | SNDRV_PCM_RATE_11025 |\
			SNDRV_PCM_RATE_16000 | SNDRV_PCM_RATE_22050 |\
			SNDRV_PCM_RATE_32000 | SNDRV_PCM_RATE_48000 |\
			SNDRV_PCM_RATE_96000 | SNDRV_PCM_RATE_192000)
#define ES705_SLIMBUS_RATES (SNDRV_PCM_RATE_48000)

#define ES705_FORMATS (SNDRV_PCM_FMTBIT_S16_LE | SNDRV_PCM_FMTBIT_S16_BE |\
			SNDRV_PCM_FMTBIT_S20_3LE | SNDRV_PCM_FMTBIT_S20_3BE |\
			SNDRV_PCM_FMTBIT_S24_LE | SNDRV_PCM_FMTBIT_S24_BE |\
			SNDRV_PCM_FMTBIT_S32_LE | SNDRV_PCM_FMTBIT_S32_BE)
#define ES705_SLIMBUS_FORMATS (SNDRV_PCM_FMTBIT_S16_LE |\
			SNDRV_PCM_FMTBIT_S16_BE)

struct snd_soc_dai_driver es705_dai[] = {
	{
		.name = "es705-slim-rx1",
		.id = ES705_SLIM_1_PB,
		.playback = {
			.stream_name = "SLIM_PORT-1 Playback",
			.channels_min = 1,
			.channels_max = 2,
			.rates = ES705_SLIMBUS_RATES,
			.formats = ES705_SLIMBUS_FORMATS,
		},
		.ops = &es705_slim_port_dai_ops,
	},
	{
		.name = "es705-slim-tx1",
		.id = ES705_SLIM_1_CAP,
		.capture = {
			.stream_name = "SLIM_PORT-1 Capture",
			.channels_min = 1,
			.channels_max = 2,
			.rates = ES705_SLIMBUS_RATES,
			.formats = ES705_SLIMBUS_FORMATS,
		},
		.ops = &es705_slim_port_dai_ops,
	},
	{
		.name = "es705-slim-rx2",
		.id = ES705_SLIM_2_PB,
		.playback = {
			.stream_name = "SLIM_PORT-2 Playback",
			.channels_min = 1,
			.channels_max = 2,
			.rates = ES705_SLIMBUS_RATES,
			.formats = ES705_SLIMBUS_FORMATS,
		},
		.ops = &es705_slim_port_dai_ops,
	},
	{
		.name = "es705-slim-tx2",
		.id = ES705_SLIM_2_CAP,
		.capture = {
			.stream_name = "SLIM_PORT-2 Capture",
			.channels_min = 1,
			.channels_max = 2,
			.rates = ES705_SLIMBUS_RATES,
			.formats = ES705_SLIMBUS_FORMATS,
		},
		.ops = &es705_slim_port_dai_ops,
	},
	{
		.name = "es705-slim-rx3",
		.id = ES705_SLIM_3_PB,
		.playback = {
			.stream_name = "SLIM_PORT-3 Playback",
			.channels_min = 1,
			.channels_max = 4,
			.rates = ES705_SLIMBUS_RATES,
			.formats = ES705_SLIMBUS_FORMATS,
		},
		.ops = &es705_slim_port_dai_ops,
	},
	{
		.name = "es705-slim-tx3",
		.id = ES705_SLIM_3_CAP,
		.capture = {
			.stream_name = "SLIM_PORT-3 Capture",
			.channels_min = 1,
			.channels_max = 2,
			.rates = ES705_SLIMBUS_RATES,
			.formats = ES705_SLIMBUS_FORMATS,
		},
		.ops = &es705_slim_port_dai_ops,
	},
	{
		.name = "es705-porta",
		.id = ES705_I2S_PORTA,
		.playback = {
			.stream_name = "PORTA Playback",
			.channels_min = 2,
			.channels_max = 2,
			.rates = ES705_RATES,
			.formats = ES705_FORMATS,
		},
		.capture = {
			.stream_name = "PORTA Capture",
			.channels_min = 2,
			.channels_max = 2,
			.rates = ES705_RATES,
			.formats = ES705_FORMATS,
		},
		.ops = &es705_i2s_port_dai_ops,
	},
	{
		.name = "es705-portb",
		.id = ES705_I2S_PORTB,
		.playback = {
			.stream_name = "PORTB Playback",
			.channels_min = 2,
			.channels_max = 2,
			.rates = ES705_RATES,
			.formats = ES705_FORMATS,
		},
		.capture = {
			.stream_name = "PORTB Capture",
			.channels_min = 2,
			.channels_max = 2,
			.rates = ES705_RATES,
			.formats = ES705_FORMATS,
		},
		.ops = &es705_i2s_port_dai_ops,
	},
	{
		.name = "es705-portc",
		.id = ES705_I2S_PORTC,
		.playback = {
			.stream_name = "PORTC Playback",
			.channels_min = 2,
			.channels_max = 2,
			.rates = ES705_RATES,
			.formats = ES705_FORMATS,
		},
		.capture = {
			.stream_name = "PORTC Capture",
			.channels_min = 2,
			.channels_max = 2,
			.rates = ES705_RATES,
			.formats = ES705_FORMATS,
		},
		.ops = &es705_i2s_port_dai_ops,
	},
	{
		.name = "es705-portd",
		.id = ES705_I2S_PORTD,
		.playback = {
			.stream_name = "PORTD Playback",
			.channels_min = 2,
			.channels_max = 2,
			.rates = ES705_RATES,
			.formats = ES705_FORMATS,
		},
		.capture = {
			.stream_name = "PORTD Capture",
			.channels_min = 2,
			.channels_max = 2,
			.rates = ES705_RATES,
			.formats = ES705_FORMATS,
		},
		.ops = &es705_i2s_port_dai_ops,
	},
};

#ifdef CONFIG_PM
#if LINUX_VERSION_CODE >= KERNEL_VERSION(3, 4, 0)
static int es705_codec_suspend(struct snd_soc_codec *codec)
#else
static int es705_codec_suspend(struct snd_soc_codec *codec, pm_message_t state)
#endif
{
	struct es705_priv *es705 = snd_soc_codec_get_drvdata(codec);

	dev_dbg(es705->dev, "%s(): eS705 goes to sleep\n", __func__);
	es705_set_bias_level(codec, SND_SOC_BIAS_OFF);

	es705_sleep(es705);

	return 0;
}

static int es705_codec_resume(struct snd_soc_codec *codec)
{
	struct es705_priv *es705 = snd_soc_codec_get_drvdata(codec);

	dev_dbg(es705->dev, "%s(): eS705 waking up\n", __func__);
	es705_wakeup(es705);

	es705_set_bias_level(codec, SND_SOC_BIAS_STANDBY);

	return 0;
}
#else
#define es705_codec_suspend NULL
#define es705_codec_resume NULL
#endif

int es705_remote_add_codec_controls(struct snd_soc_codec *codec)
{
	int rc;

#if LINUX_VERSION_CODE >= KERNEL_VERSION(3, 4, 0)
	rc = snd_soc_add_codec_controls(codec, es705_digital_ext_snd_controls,
				ARRAY_SIZE(es705_digital_ext_snd_controls));
#else
	rc = snd_soc_add_controls(codec, es705_digital_ext_snd_controls,
				ARRAY_SIZE(es705_digital_ext_snd_controls));
#endif
	if (rc)
		dev_err(codec->dev, "%s(): es705_digital_ext_snd_controls failed\n", __func__);

	return rc;
}

static int es705_codec_probe(struct snd_soc_codec *codec)
{
	struct es705_priv *es705 = snd_soc_codec_get_drvdata(codec);

	dev_dbg(codec->dev, "%s()\n", __func__);
	es705->codec = codec;

	codec->control_data = snd_soc_codec_get_drvdata(codec);

	es705_set_bias_level(codec, SND_SOC_BIAS_STANDBY);

	/*
	rc = snd_soc_add_controls(codec, es705_digital_snd_controls,
					ARRAY_SIZE(es705_digital_snd_controls));
	if (rc)
		dev_err(codec->dev, "%s(): es705_digital_snd_controls failed\n", __func__);
	*/

	return 0;
}

static int  es705_codec_remove(struct snd_soc_codec *codec)
{
	struct es705_priv *es705 = snd_soc_codec_get_drvdata(codec);

	es705_set_bias_level(codec, SND_SOC_BIAS_OFF);

	kfree(es705);

	return 0;
}

struct snd_soc_codec_driver soc_codec_dev_es705 = {
	.probe =	es705_codec_probe,
	.remove =	es705_codec_remove,
	.suspend =	es705_codec_suspend,
	.resume =	es705_codec_resume,
	.read =		es705_read,
	.write =	es705_write,
	.set_bias_level =	es705_set_bias_level,
};

static void es705_event_status(struct es705_priv *es705)
{
	int rc = 0;
	const u32 vs_get_key_word_status = 0x806D0000;
	u32 rspn;
	int match = 0;

	if (es705->es705_power_state == ES705_SET_POWER_STATE_VS_LOWPWR) {
		rc = es705_wakeup(es705);
		if (rc) {
			dev_err(es705->dev, "%s(): Get VS Status Fail\n",
				__func__);
			return;
	} else {
		/* wait VS status readiness */
		msleep(50);
	}
	}

	cpu_to_le32s(&vs_get_key_word_status);
	rc = es705_write_then_read(es705, &vs_get_key_word_status,
					sizeof(vs_get_key_word_status),
					&rspn, match);
	if (rc) {
		dev_err(es705->dev, "%s(): Get VS Status Fail\n", __func__);
		return;
	}

	if (es705->mode == VOICESENSE) {
		u32 cmd = (ES705_SET_POWER_STATE << 16) |
				ES705_SET_POWER_STATE_NORMAL;
		le32_to_cpus(&rspn);
		rspn = rspn & 0xFFFF;
		/* Check VS detection status. */
		dev_info(es705->dev, "%s(): VS status 0x%04x\n",
			__func__, rspn);
		if ((rspn & 0x00FF) == KW_DETECTED) {
			dev_info(es705->dev, "%s(): Generate VS keyword detected event to User space\n",
				__func__);
			es705->vs_get_event = KW_DETECTED;
			es705_vs_event(es705);
			dev_info(es705_priv.dev, "%s(): VS keyword detected\n",
				__func__);
			/* Switch FW to STANDARD */
			es705_cmd(es705, cmd);
			msleep(20);
			es705->pm_state = ES705_POWER_AWAKE;
#if defined(SAMSUNG_ES705_FEATURE)
			es705_priv.power_control(ES705_SET_POWER_STATE_VS_OVERLAY, ES705_POWER_STATE);
#endif
		}
	}
}

irqreturn_t es705_irq_event(int irq, void *irq_data)
{
	struct es705_priv *es705 = (struct es705_priv *)irq_data;

	mutex_lock(&es705->api_mutex);
	dev_info(es705->dev, "%s(): %s mode, Interrupt event",
		__func__, esxxx_mode[es705->mode]);
	/* Get Event status, reset Interrupt */
	es705_event_status(es705);
	mutex_unlock(&es705->api_mutex);
	return IRQ_HANDLED;
}

static int register_snd_soc(struct es705_priv *priv)
{
	int rc;
	int i;
	int ch_cnt;
	struct slim_device *sbdev = priv->gen0_client;

	es705_init_slim_slave(sbdev);

	dev_dbg(&sbdev->dev, "%s(): name = %s\n", __func__, sbdev->name);
	rc = snd_soc_register_codec(&sbdev->dev, &soc_codec_dev_es705,
				    es705_dai, ARRAY_SIZE(es705_dai));
	dev_dbg(&sbdev->dev, "%s(): rc = snd_soc_regsiter_codec() = %d\n",
		__func__, rc);

	/* allocate ch_num array for each DAI */
	for (i = 0; i < ARRAY_SIZE(es705_dai); i++) {
		switch (es705_dai[i].id) {
		case ES705_SLIM_1_PB:
		case ES705_SLIM_2_PB:
		case ES705_SLIM_3_PB:
			ch_cnt = es705_dai[i].playback.channels_max;
			break;
		case ES705_SLIM_1_CAP:
		case ES705_SLIM_2_CAP:
		case ES705_SLIM_3_CAP:
			ch_cnt = es705_dai[i].capture.channels_max;
			break;
		default:
			continue;
		}
		es705_priv.dai[i].ch_num =
			kzalloc((ch_cnt * sizeof(unsigned int)), GFP_KERNEL);
	}

	es705_slim_map_channels(priv);
	return rc;
}

int es705_core_probe(struct device *dev)
{
	struct esxxx_platform_data *pdata = dev->platform_data;
	int rc = 0;
	const char *fw_filename = "audience-es705-fw.bin";
	const char *vs_filename = "audience-es705-vs.bin";

	if (pdata == NULL) {
		dev_err(dev, "%s(): pdata is NULL", __func__);
		rc = -EIO;
		goto pdata_error;
	}

	es705_priv.dev = dev;
	es705_priv.pdata = pdata;
	es705_priv.fw_requested = 0;

	es705_clk_init(&es705_priv);
#if defined(SAMSUNG_ES705_FEATURE)
	/*
	 * Set UART interface as a default for STANDARD FW
	 * download at boot time. And select Baud Rate 3MBPs
	 */
	es705_priv.uart_fw_download = es705_uart_fw_download;
	if (system_rev >= UART_DOWNLOAD_WAKEUP_HWREV) {
		es705_priv.uart_fw_download_rate = 3;
		es705_priv.use_uart_for_wakeup_gpio = 1;
	} else {
		es705_priv.uart_fw_download_rate = 0;
		es705_priv.use_uart_for_wakeup_gpio = 0;
	}
	/* Select UART eS705 Wakeup mechanism */
	es705_priv.uart_es705_wakeup = es705_uart_es705_wakeup;
	es705_priv.power_control = es705_power_control;
#else
        /*
         * Set default interface for STANDARD FW
         * download at boot time.
         */
        es705_priv.uart_fw_download = NULL;
        es705_priv.uart_fw_download_rate = 0;
        /* Select GPIO eS705 Wakeup mechanism */
	es705_priv.uart_es705_wakeup = NULL;
#endif
	es705_priv.uart_state = UART_CLOSE;
	es705_priv.sleep_delay = ES705_SLEEP_DEFAULT_DELAY;

	mutex_init(&es705_priv.api_mutex);
	mutex_init(&es705_priv.pm_mutex);
	mutex_init(&es705_priv.streaming_mutex);

	init_waitqueue_head(&es705_priv.stream_in_q);

	/* No keyword parameters available until set. */
	es705_priv.vs_keyword_param_size = 0;

	rc = sysfs_create_group(&es705_priv.dev->kobj, &core_sysfs);
	if (rc) {
		dev_err(es705_priv.dev, "%s(): failed to create core sysfs entries: %d\n",
			__func__, rc);
	}

	rc = es705_gpio_init(&es705_priv);
	if (rc)
		goto gpio_init_error;

	if (pdata->esxxx_clk_cb)
		pdata->esxxx_clk_cb(1);

	msleep(10);
	es705_gpio_reset(&es705_priv);

	rc = request_firmware((const struct firmware **)&es705_priv.standard,
			      fw_filename, es705_priv.dev);
	if (rc) {
		dev_err(es705_priv.dev, "%s(): request_firmware(%s) failed %d\n",
			__func__, fw_filename, rc);
		goto request_firmware_error;
	}

	rc = request_firmware((const struct firmware **)&es705_priv.vs,
			      vs_filename, es705_priv.dev);
	if (rc) {
		dev_err(es705_priv.dev, "%s(): request_firmware(%s) failed %d\n",
			__func__, vs_filename, rc);
		goto request_vs_firmware_error;
	}

	INIT_DELAYED_WORK(&es705_priv.sleep_work, es705_delayed_sleep);

	es705_priv.pm_state = ES705_POWER_FW_LOAD;
	es705_priv.fw_requested = 1;

#if defined(SAMSUNG_ES705_FEATURE)
	rc = es705_init_input_device(&es705_priv);
	if (rc < 0)
		goto init_input_device_error;
#endif
	return rc;

#if defined(SAMSUNG_ES705_FEATURE)
init_input_device_error:
#endif
request_vs_firmware_error:
	release_firmware(es705_priv.standard);
request_firmware_error:
gpio_init_error:
pdata_error:
	dev_dbg(es705_priv.dev, "%s(): exit with error\n", __func__);
	return rc;
}
EXPORT_SYMBOL_GPL(es705_core_probe);

static __init int es705_init(void)
{
	int rc = 0;

#if defined(CONFIG_SND_SOC_ES705_I2C)

	rc = es705_i2c_init(&es705_priv);

#elif defined(CONFIG_SND_SOC_ES705_SPI)

	rc = spi_register_driver(&es705_spi_driver);
	if (!rc) {
		pr_debug("%s() registered as SPI", __func__);
		es705_priv.intf = ES705_SPI_INTF;
	}
#else
	/* Bus specifc registration */
	/* FIXME: Temporary kludge until es705_bus_init abstraction
	 * is worked out */
#if !defined(CONFIG_SND_SOC_ES705_UART)
	rc = es705_bus_init(&es705_priv);
#else
	rc = es705_uart_bus_init(&es705_priv);
#endif

#endif

	if (rc) {
		pr_debug("Error registering Audience eS705 driver: %d\n", rc);
		goto INIT_ERR;
	}

#if !defined(CONFIG_SND_SOC_ES705_UART)
/* If CONFIG_SND_SOC_ES705_UART, UART probe will initialize char device
 * if a es705 is found */
	rc = es705_init_cdev(&es705_priv);
	if (rc) {
		pr_err("failed to initialize char device = %d\n", rc);
		goto INIT_ERR;
	}
#endif

INIT_ERR:
	return rc;
}
module_init(es705_init);

static __exit void es705_exit(void)
{
#if defined(SAMSUNG_ES705_FEATURE)
	es705_unregister_input_device(&es705_priv);
#endif
	if (es705_priv.fw_requested) {
		release_firmware(es705_priv.standard);
		release_firmware(es705_priv.vs);
	}
	es705_cleanup_cdev(&es705_priv);

#if defined(CONFIG_SND_SOC_ES705_I2C)
	i2c_del_driver(&es705_i2c_driver);
#else
	/* no support from QCOM to unregister
	 * slim_driver_unregister(&es705_slim_driver);
	 */
#endif
}
module_exit(es705_exit);


MODULE_DESCRIPTION("ASoC ES705 driver");
MODULE_AUTHOR("Greg Clemson <gclemson@audience.com>");
MODULE_AUTHOR("Genisim Tsilker <gtsilker@audience.com>");
MODULE_LICENSE("GPL v2");
MODULE_ALIAS("platform:es705-codec");
MODULE_FIRMWARE("audience-es705-fw.bin");
