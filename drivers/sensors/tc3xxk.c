/* tc3xxk.c -- Linux driver for coreriver chip as grip
 *
 * Copyright (C) 2013 Samsung Electronics Co.Ltd
 * Author: Junkyeong Kim <jk0430.kim@samsung.com>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2, or (at your option) any
 * later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 */

#include <linux/delay.h>
#include <linux/firmware.h>
#include <linux/of_gpio.h>
#include <linux/gpio.h>
#include <linux/i2c.h>
#include <linux/init.h>
#include <linux/input.h>
#include <linux/interrupt.h>
#include <linux/irq.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/mutex.h>
#include <linux/slab.h>
#include <linux/wakelock.h>
#include <linux/workqueue.h>
#include <linux/uaccess.h>
#include <linux/regulator/consumer.h>
#include <linux/sec_sysfs.h>
#include <linux/sensor/sensors_core.h>
#ifdef CONFIG_BATTERY_SAMSUNG
#include <linux/sec_batt.h>
#endif

#include "tc3xxk.h"

#if defined (CONFIG_VBUS_NOTIFIER)
#include <linux/muic/muic.h>
#include <linux/muic/muic_notifier.h>
#include <linux/vbus_notifier.h>
#endif

#define IDLE                     0
#define ACTIVE                   1

/* registers */
#define TC300K_KEYCODE		0x00
#define TC300K_FWVER		0x01
#define TC300K_MDVER		0x02
#define TC300K_MODE			0x03
#define TC300K_CHECKS_H		0x04
#define TC300K_CHECKS_L		0x05
#define TC300K_THRES_H		0x06
#define TC300K_THRES_L		0x07
#define TC300K_1KEY_DATA	0x08
#define TC300K_2KEY_DATA	0x0E
#define TC300K_3KEY_DATA	0x14
#define TC300K_4KEY_DATA	0x1A
#define TC300K_5KEY_DATA	0x20
#define TC300K_6KEY_DATA	0x26

#define TC300K_CH_PCK_H_OFFSET	0x00
#define TC300K_CH_PCK_L_OFFSET	0x01
#define TC300K_DIFF_H_OFFSET	0x02
#define TC300K_DIFF_L_OFFSET	0x03
#define TC300K_RAW_H_OFFSET		0x04
#define TC300K_RAW_L_OFFSET		0x05

/* registers for tabs2(tc350k) */
#define TC350K_1KEY		0x10	// recent inner
#define TC350K_2KEY		0x18	// back inner
#define TC350K_3KEY		0x20	// recent outer
#define TC350K_4KEY		0x28	// back outer

/* registers for grip sensor */
#define TC305K_GRIPCODE			0x0F
#define TC305K_GRIP_THD_PRESS		0x00
#define TC305K_GRIP_THD_RELEASE		0x02
#define TC305K_GRIP_THD_NOISE		0x04
#define TC305K_GRIP_CH_PERCENT		0x06
#define TC305K_GRIP_DIFF_DATA		0x08
#define TC305K_GRIP_RAW_DATA		0x0A
#define TC305K_GRIP_BASELINE		0x0C
#define TC305K_GRIP_TOTAL_CAP		0x0E
#define TC305K_GRIP_REF_CAP		0x70

#define TC305K_1GRIP			0x30
#define TC305K_2GRIP			0x40
#define TC305K_3GRIP			0x50
#define TC305K_4GRIP			0x60

#define TC350K_THRES_DATA_OFFSET	0x00
#define TC350K_CH_PER_DATA_OFFSET	0x02
#define TC350K_CH_DIFF_DATA_OFFSET	0x04
#define TC350K_CH_RAW_DATA_OFFSET	0x06

#define TC350K_DATA_SIZE		0x02
#define TC350K_DATA_H_OFFSET	0x00
#define TC350K_DATA_L_OFFSET	0x01

/* command */
#define TC300K_CMD_ADDR			0x00
#define TC300K_CMD_LED_ON		0x10
#define TC300K_CMD_LED_OFF		0x20
#define TC300K_CMD_GLOVE_ON		0x30
#define TC300K_CMD_GLOVE_OFF	0x40
#define TC300K_CMD_TA_ON		0x50
#define TC300K_CMD_TA_OFF		0x60
#define TC300K_CMD_CAL_CHECKSUM	0x70
#define TC300K_CMD_STOP_MODE		0x90
#define TC300K_CMD_NORMAL_MODE		0x91
#define TC300K_CMD_SAR_DISABLE		0xA0
#define TC300K_CMD_SAR_ENABLE		0xA1
#define TC300K_CMD_FLIP_OFF		0xB0
#define TC300K_CMD_FLIP_ON		0xB1
#define TC300K_CMD_GRIP_BASELINE_CAL	0xC0
#define TC300K_CMD_WAKE_UP		0xF0
#define TC300K_CMD_DELAY		50

/* mode status bit */
#define TC300K_MODE_TA_CONNECTED	(1 << 0)
#define TC300K_MODE_RUN			(1 << 1)
#define TC300K_MODE_SAR			(1 << 2)
#define TC300K_MODE_GLOVE		(1 << 4)

/* connecter check */
#define SUB_DET_DISABLE			0
#define SUB_DET_ENABLE_CON_OFF	1
#define SUB_DET_ENABLE_CON_ON	2

/* firmware */
#define TC300K_FW_PATH_SDCARD	"/sdcard/tc3xxk.bin"

#define TK_UPDATE_PASS		0
#define TK_UPDATE_DOWN		1
#define TK_UPDATE_FAIL		2

/* ISP command */
#define TC300K_CSYNC1			0xA3
#define TC300K_CSYNC2			0xAC
#define TC300K_CSYNC3			0xA5
#define TC300K_CCFG				0x92
#define TC300K_PRDATA			0x81
#define TC300K_PEDATA			0x82
#define TC300K_PWDATA			0x83
#define TC300K_PECHIP			0x8A
#define TC300K_PEDISC			0xB0
#define TC300K_LDDATA			0xB2
#define TC300K_LDMODE			0xB8
#define TC300K_RDDATA			0xB9
#define TC300K_PCRST			0xB4
#define TC300K_PCRED			0xB5
#define TC300K_PCINC			0xB6
#define TC300K_RDPCH			0xBD

/* ISP delay */
#define TC300K_TSYNC1			300	/* us */
#define TC300K_TSYNC2			50	/* 1ms~50ms */
#define TC300K_TSYNC3			100	/* us */
#define TC300K_TDLY1			1	/* us */
#define TC300K_TDLY2			2	/* us */
#define TC300K_TFERASE			10	/* ms */
#define TC300K_TPROG			20	/* us */

#define TC300K_CHECKSUM_DELAY	500

enum {
	FW_INKERNEL,
	FW_SDCARD,
};

struct fw_image {
	u8 hdr_ver;
	u8 hdr_len;
	u16 first_fw_ver;
	u16 second_fw_ver;
	u16 third_ver;
	u32 fw_len;
	u16 checksum;
	u16 alignment_dummy;
	u8 data[0];
} __attribute__ ((packed));

#define TSK_RELEASE			0x00
#define TSK_PRESS			0x01
#define GRIP_RELEASE			0x00
#define GRIP_PRESS			0x01

struct tsk_event_val {
	u16 tsk_bitmap;
	u8 tsk_status;
	int tsk_keycode;
	char* tsk_keyname;
};

struct grip_event_val {
	u16 grip_bitmap;
	u8 grip_status;
	int grip_code;
	char* grip_name;
};


struct grip_event_val grip_ev[4] =
{
	{0x01 << 0, GRIP_PRESS, KEY_CP_GRIP, "grip1"},
	{0x01 << 1, GRIP_PRESS, KEY_CP_GRIP, "grip2"},
	{0x01 << 4, GRIP_RELEASE, KEY_CP_GRIP, "grip1"},
	{0x01 << 5, GRIP_RELEASE, KEY_CP_GRIP, "grip2"},
};

struct tc3xxk_data {
	struct device *dev;
	struct i2c_client *client;
	struct input_dev *input_dev;
	struct tc3xxk_platform_data *pdata;
	struct mutex lock;
	struct mutex lock_fac;
	struct fw_image *fw_img;
	const struct firmware *fw;
	char phys[32];
	int irq;
	u16 checksum;
	u16 threhold;
	int mode;
	int (*power) (bool on);
	u8 fw_ver;
	u8 fw_ver_bin;
	u8 md_ver;
	u8 md_ver_bin;
	u8 fw_update_status;
	bool enabled;
	bool fw_downloding;

	struct pinctrl *pinctrl_i2c;
	struct pinctrl *pinctrl_irq;
	struct pinctrl_state *pin_state[4];

	struct wake_lock grip_wake_lock;
	u16 grip_p_thd;
	u16 grip_r_thd;
	u16 grip_n_thd;
	u16 grip_s1;
	u16 grip_s2;
	u16 grip_baseline;
	u16 grip_raw1;
	u16 grip_raw2;
	u16 grip_event;
	bool sar_mode;
	bool sar_enable;
	bool sar_enable_off;
	int grip_num;
	struct grip_event_val *grip_ev_val;
	int irq_count;
	int abnormal_mode;
	s32 diff;
	s32 max_diff;

#if defined (CONFIG_VBUS_NOTIFIER)
	struct notifier_block vbus_nb;
#endif
};

extern struct class *sec_class;

char *str_states[] = {"on_irq", "off_irq", "on_i2c", "off_i2c"};
enum {
	I_STATE_ON_IRQ = 0,
	I_STATE_OFF_IRQ,
	I_STATE_ON_I2C,
	I_STATE_OFF_I2C,
};

static bool tc3xxk_power_enabled;
const char *regulator_ic;

static int tc3xxk_pinctrl_init(struct tc3xxk_data *data);
static void tc3xxk_config_gpio_i2c(struct tc3xxk_data *data, int onoff);
static int tc3xxk_pinctrl(struct tc3xxk_data *data, int status);
static int read_tc350k_register_data(struct tc3xxk_data *data, int read_key_num, int read_offset);
static int tc3xxk_mode_enable(struct i2c_client *client, u8 cmd);

static int tc3xxk_mode_check(struct i2c_client *client)
{
	int mode = i2c_smbus_read_byte_data(client, TC300K_MODE);
	if (mode < 0)
		SENSOR_ERR("failed to read mode (%d)\n", mode);

	return mode;
}

static int tc3xxk_wake_up(struct i2c_client *client, u8 cmd)
{
	//If stop mode enabled, touch key IC need wake_up CMD
	//After wake_up CMD, IC need 10ms delay

	int ret;
	SENSOR_INFO("Send WAKE UP cmd: 0x%02x \n", cmd);
	ret = i2c_smbus_write_byte_data(client, TC300K_CMD_ADDR, TC300K_CMD_WAKE_UP);
	msleep(10);

	return ret;
}

static void tc3xxk_stop_mode(struct tc3xxk_data *data, bool on)
{
	struct i2c_client *client = data->client;
	int retry = 3;
	int ret;
	u8 cmd;
	int mode_retry = 1;
	bool mode;

	if (data->sar_mode == on){
		SENSOR_ERR("skip already %s\n", on ? "stop mode":"normal mode");
		return;
	}

	if (on)
		cmd = TC300K_CMD_STOP_MODE;
	else
		cmd = TC300K_CMD_NORMAL_MODE;

	SENSOR_INFO("%s, cmd=%x\n", on ? "stop mode" : "normal mode", cmd);
sar_mode:
	//Add wake up command before change mode from STOP mode to NORMAL mode
	if (on == 0 && data->sar_mode == 1) {
		ret = tc3xxk_wake_up(client, TC300K_CMD_WAKE_UP);
	}

	while (retry > 0) {
		ret = tc3xxk_mode_enable(client, cmd);
		if (ret < 0) {
			SENSOR_ERR("fail to write mode(%d), retry %d\n", ret, retry);
			retry--;
			msleep(20);
			continue;
		}
		break;
	}

	msleep(20);

	// Add wake up command before i2c read/write in STOP mode
	if (on == 1) {
		ret = tc3xxk_wake_up(client, TC300K_CMD_WAKE_UP);
	}

	retry = 3;
	while (retry > 0) {
		ret = tc3xxk_mode_check(client);
		if (ret < 0) {
			SENSOR_ERR("fail to read mode(%d), retry %d\n", ret, retry);
			retry--;
			msleep(20);
			continue;
		}
		break;
	}

	/*	RUN MODE
	  *	1 : NORMAL TOUCH MODE
	  *	0 : STOP MODE
	  */
	mode = !(ret & TC300K_MODE_RUN);

	SENSOR_INFO("run mode:%s reg:%x\n", mode ? "stop": "normal", ret);

	if((mode != on) && (mode_retry == 1)){
		SENSOR_ERR("change fail retry %d, %d\n", mode, on);
		mode_retry = 0;
		goto sar_mode;
	}

	data->sar_mode = mode;
}

static void grip_sar_sensing(struct tc3xxk_data *data, bool on)
{
	/* enable/disable sar sensing
	  * need to disable when earjack is connected (FM radio can't work normally)
	  */
}

static void tc3xxk_grip_cal_reset(struct tc3xxk_data *data)
{
	/* calibrate grip sensor chn */
	struct i2c_client *client = data->client;

	SENSOR_INFO("\n");
	i2c_smbus_write_byte_data(client, TC300K_CMD_ADDR, TC300K_CMD_GRIP_BASELINE_CAL);
	msleep(TC300K_CMD_DELAY);
}

static void tc3xxk_reset(struct tc3xxk_data *data)
{
	SENSOR_INFO("\n");

	disable_irq_nosync(data->client->irq);

	data->pdata->power(data, false);

	msleep(50);

	data->pdata->power(data, true);
	msleep(200);

	if (data->sar_enable)
		tc3xxk_mode_enable(data->client, TC300K_CMD_SAR_ENABLE);

	enable_irq(data->client->irq);
}

static void tc3xxk_reset_probe(struct tc3xxk_data *data)
{
	data->pdata->power(data, false);

	msleep(50);

	data->pdata->power(data, true);
	msleep(200);
}

int tc3xxk_get_fw_version(struct tc3xxk_data *data, bool probe)
{
	struct i2c_client *client = data->client;
	int retry = 3;
	int buf;

	if ((!data->enabled) || data->fw_downloding) {
		SENSOR_ERR("can't excute\n");
		return -1;
	}

	buf = i2c_smbus_read_byte_data(client, TC300K_FWVER);
	if (buf < 0) {
		while (retry--) {
			SENSOR_ERR("read fail(%d)\n", retry);
			if (probe)
				tc3xxk_reset_probe(data);
			else
				tc3xxk_reset(data);
			buf = i2c_smbus_read_byte_data(client, TC300K_FWVER);
			if (buf > 0)
				break;
		}
		if (retry <= 0) {
			SENSOR_ERR("read fail\n");
			data->fw_ver = 0;
			return -1;
		}
	}
	data->fw_ver = (u8)buf;
	SENSOR_INFO( "fw_ver : 0x%x\n", data->fw_ver);

	return 0;
}

int tc3xxk_get_md_version(struct tc3xxk_data *data, bool probe)
{
	struct i2c_client *client = data->client;
	int retry = 3;
	int buf;

	if ((!data->enabled) || data->fw_downloding) {
		SENSOR_ERR("can't excute\n");
		return -1;
	}

	buf = i2c_smbus_read_byte_data(client, TC300K_MDVER);
	if (buf < 0) {
		while (retry--) {
			SENSOR_ERR("read fail(%d)\n", retry);
			if (probe)
				tc3xxk_reset_probe(data);
			else
				tc3xxk_reset(data);
			buf = i2c_smbus_read_byte_data(client, TC300K_MDVER);
			if (buf > 0)
				break;
		}
		if (retry <= 0) {
			SENSOR_ERR("read fail\n");
			data->md_ver = 0;
			return -1;
		}
	}
	data->md_ver = (u8)buf;
	SENSOR_INFO( "md_ver : 0x%x\n", data->md_ver);

	return 0;
}

static void tc3xxk_gpio_request(struct tc3xxk_data *data)
{
	int ret = 0;
	SENSOR_INFO("\n");

	if (!data->pdata->i2c_gpio) {
		ret = gpio_request(data->pdata->gpio_scl, "grip_scl");
		if (ret) {
			SENSOR_ERR("unable to request grip_scl [%d]\n",
					 data->pdata->gpio_scl);
		}

		ret = gpio_request(data->pdata->gpio_sda, "grip_sda");
		if (ret) {
			SENSOR_ERR("unable to request grip_sda [%d]\n",
					data->pdata->gpio_sda);
		}
	}

	ret = gpio_request(data->pdata->gpio_int, "grip_irq");
	if (ret) {
		SENSOR_ERR("unable to request grip_irq [%d]\n",
				data->pdata->gpio_int);
	}
}

static int tc3xxk_parse_dt(struct device *dev,
			struct tc3xxk_platform_data *pdata)
{
	struct device_node *np = dev->of_node;

	of_property_read_u32(np, "coreriver,use_bitmap", &pdata->use_bitmap);

	SENSOR_INFO("%s protocol.\n",
				pdata->use_bitmap ? "Use Bit-map" : "Use OLD");

	pdata->gpio_scl = of_get_named_gpio_flags(np, "coreriver,scl-gpio", 0, &pdata->scl_gpio_flags);
	pdata->gpio_sda = of_get_named_gpio_flags(np, "coreriver,sda-gpio", 0, &pdata->sda_gpio_flags);
	pdata->gpio_int = of_get_named_gpio_flags(np, "coreriver,irq-gpio", 0, &pdata->irq_gpio_flags);

	pdata->boot_on_ldo = of_property_read_bool(np, "coreriver,boot-on-ldo");

	pdata->i2c_gpio = of_property_read_bool(np, "coreriver,i2c-gpio");
	
	if (of_property_read_string(np, "coreriver,regulator_ic", &pdata->regulator_ic)) {
		input_err(true, dev, "%s Failed to get regulator_ic name property\n",__func__);
		return -EINVAL;
	}
	regulator_ic = pdata->regulator_ic;

	if (of_property_read_string(np, "coreriver,fw_name", &pdata->fw_name)) {
		SENSOR_ERR("Failed to get fw_name property\n");
		return -EINVAL;
	} else {
		SENSOR_INFO("fw_name %s\n", pdata->fw_name);
	}

	if (of_property_read_u32(np, "coreriver,sensing_ch_num", &pdata->sensing_ch_num) < 0){
		SENSOR_ERR("Failed to get sensing_ch_num property\n");
		return -EINVAL;
	}

	if (of_property_read_u32(np, "coreriver,tsk_ic_num", &pdata->tsk_ic_num) < 0){
		SENSOR_INFO("Failed to get tsk_ic_num, TSK IC is TC300K\n");
	} else {
		if (pdata->tsk_ic_num == TC350K_TSK_IC)
			SENSOR_INFO("TSK IC is TC350K_TSK_IC[%d]\n", pdata->tsk_ic_num);
		else
			SENSOR_ERR("TSK IC is unknown![%d]\n", pdata->tsk_ic_num);
	}

	pdata->bringup = of_property_read_bool(np, "coreriver,bringup");
	if (pdata->bringup  < 0)
		pdata->bringup = 0;

	return 0;
}

int tc3xxk_grip_power(void *info, bool on)
{
	struct regulator *regulator;
	int ret = 0;

	if (tc3xxk_power_enabled == on)
		return 0;

	SENSOR_INFO("%s\n", on ? "on" : "off");

	regulator = regulator_get(NULL, regulator_ic);
	if (IS_ERR(regulator)){
		SENSOR_ERR("regulator_ic get failed\n");
		return -EIO;
	}
	if (on) {
		ret = regulator_enable(regulator);
		if (ret) {
			SENSOR_ERR("regulator_ic enable failed\n");
			return ret;
		}
	} else {
		if (regulator_is_enabled(regulator)){
			regulator_disable(regulator);
			if (ret) {
				SENSOR_ERR("regulator_ic disable failed\n");
				return ret;
			}
		}
		else
			regulator_force_disable(regulator);
	}
	regulator_put(regulator);

	tc3xxk_power_enabled = on;

	return 0;
}

static irqreturn_t tc3xxk_interrupt(int irq, void *dev_id)
{
	struct tc3xxk_data *data = dev_id;
	struct i2c_client *client = data->client;
	int ret, retry;
	int i = 0;
	u8 grip_val;
	bool grip_handle_flag;

	wake_lock(&data->grip_wake_lock);

	SENSOR_INFO("\n");

	if ((!data->enabled) || data->fw_downloding) {
		SENSOR_ERR("can't excute\n");
		
		wake_unlock(&data->grip_wake_lock);
		
		return IRQ_HANDLED;
	}

	// if sar_mode is on => must send wake-up command
	if (data->sar_mode) {
		ret = tc3xxk_wake_up(client, TC300K_CMD_WAKE_UP);
	}
	ret = i2c_smbus_read_byte_data(client, TC305K_GRIPCODE);
	if (ret < 0) {
		retry = 3;
		while (retry--) {
			SENSOR_ERR("read fail ret=%d(retry:%d)\n", ret, retry);
			msleep(10);
			ret = i2c_smbus_read_byte_data(client, TC305K_GRIPCODE);
			if (ret > 0)
				break;
		}
		if (retry <= 0) {
			tc3xxk_reset(data);
			wake_unlock(&data->grip_wake_lock);
			return IRQ_HANDLED;
		}
	}
	grip_val = (u8)ret;
	
	for (i = 0 ; i < data->grip_num * 2 ; i++){
		if (data->pdata->use_bitmap)
			grip_handle_flag = (grip_val & data->grip_ev_val[i].grip_bitmap);
		else
			grip_handle_flag = (grip_val == data->grip_ev_val[i].grip_bitmap);

		if (grip_handle_flag){
			//need to check when using 2 grip channel
			//data->grip_event = data->tsk_ev_val[i].tsk_status;   //TSN :: Need to check Why?
			//input_report_key(data->input_dev,
			//	data->grip_ev_val[i].grip_code, data->grip_ev_val[i].grip_status);
			if(data->grip_ev_val[i].grip_status == ACTIVE){
				data->grip_event = ACTIVE;
				input_report_rel(data->input_dev, REL_MISC, 1);
			}
			else{
				data->grip_event = IDLE;
				input_report_rel(data->input_dev, REL_MISC, 2);
			}
			SENSOR_INFO(
				"grip %s : %s(0x%02X) ver0x%02x\n",
				data->grip_ev_val[i].grip_status? "P" : "R",
				data->grip_ev_val[i].grip_name, grip_val,
				data->fw_ver);

#ifdef CONFIG_SEC_FACTORY
			data->diff = read_tc350k_register_data(data, TC305K_1GRIP, TC305K_GRIP_DIFF_DATA); 
			if (data->abnormal_mode) { 
				/* if (data->grip_event) */ {  //TSN : Need to check why?
					if (data->max_diff < data->diff) 
						data->max_diff = data->diff; 
					data->irq_count++; 
				} 
			} 
#endif
		}
	}
	input_sync(data->input_dev);

	wake_unlock(&data->grip_wake_lock);
	return IRQ_HANDLED;
}

static ssize_t keycode_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct tc3xxk_data *data = dev_get_drvdata(dev);
	struct i2c_client *client = data->client;
	int ret;

	if ((!data->enabled) || data->fw_downloding) {
		SENSOR_ERR("can't excute\n");
		return -EPERM;
	}

	
	ret = i2c_smbus_read_byte_data(client, TC300K_KEYCODE);
	if (ret < 0) {
		SENSOR_ERR("failed to read threshold_h (%d)\n", ret);
		return ret;
	}
	
	return sprintf(buf, "%d\n", ret);
}

static ssize_t third_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct tc3xxk_data *data = dev_get_drvdata(dev);
	int value;

	if ((!data->enabled) || data->fw_downloding) {
		SENSOR_ERR("can't excute\n");
		return -EPERM;
	}

	if (data->pdata->tsk_ic_num == TC350K_TSK_IC) {
		value = read_tc350k_register_data(data, TC350K_3KEY, TC350K_CH_PER_DATA_OFFSET);
		return sprintf(buf, "%d\n", value);
	} 
	else {
		value = 0;
		return sprintf(buf, "%d\n", value);
	}
		
}

static ssize_t third_raw_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct tc3xxk_data *data = dev_get_drvdata(dev);
	int value;

	if ((!data->enabled) || data->fw_downloding) {
		SENSOR_ERR("can't excute\n");
		return -EPERM;
	}

	if (data->pdata->tsk_ic_num == TC350K_TSK_IC) {
		value = read_tc350k_register_data(data, TC350K_3KEY, TC350K_CH_RAW_DATA_OFFSET);
		return sprintf(buf, "%d\n", value);
	} 
	else {
		value = 0;
		return sprintf(buf, "%d\n", value);
	}
}

static ssize_t fourth_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct tc3xxk_data *data = dev_get_drvdata(dev);
	int value;

	if ((!data->enabled) || data->fw_downloding) {
		SENSOR_ERR("can't excute\n");
		return -EPERM;
	}

	if (data->pdata->tsk_ic_num == TC350K_TSK_IC) {
		value = read_tc350k_register_data(data, TC350K_4KEY, TC350K_CH_PER_DATA_OFFSET);
		return sprintf(buf, "%d\n", value);
	} 
	else {
		value = 0;
		return sprintf(buf, "%d\n", value);
	}
}

static ssize_t fourth_raw_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct tc3xxk_data *data = dev_get_drvdata(dev);
	int value;

	if ((!data->enabled) || data->fw_downloding) {
		SENSOR_ERR("can't excute\n");
		return -EPERM;
	}

	if (data->pdata->tsk_ic_num == TC350K_TSK_IC) {
		value = read_tc350k_register_data(data, TC350K_4KEY, TC350K_CH_RAW_DATA_OFFSET);
		return sprintf(buf, "%d\n", value);
	} 
	else {
		value = 0;
		return sprintf(buf, "%d\n", value);
	}
}

static ssize_t debug_c0_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct tc3xxk_data *data = dev_get_drvdata(dev);
	struct i2c_client *client = data->client;
	int ret;

	if ((!data->enabled) || data->fw_downloding) {
		SENSOR_ERR("can't excute\n");
		return -EPERM;
	}

	
	ret = i2c_smbus_read_byte_data(client, 0xC0);
	if (ret < 0) {
		SENSOR_ERR("failed to read 0xC0 Register (%d)\n", ret);
		return ret;
	}
	
	return sprintf(buf, "%d\n", ret);
}

static ssize_t debug_c1_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct tc3xxk_data *data = dev_get_drvdata(dev);
	struct i2c_client *client = data->client;
	int ret;

	if ((!data->enabled) || data->fw_downloding) {
		SENSOR_ERR("can't excute\n");
		return -EPERM;
	}

	
	ret = i2c_smbus_read_byte_data(client, 0xC1);
	if (ret < 0) {
		SENSOR_ERR("failed to read 0xC1 Register (%d)\n", ret);
		return ret;
	}
	
	return sprintf(buf, "%d\n", ret);
}


static ssize_t debug_c2_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct tc3xxk_data *data = dev_get_drvdata(dev);
	struct i2c_client *client = data->client;
	int ret;

	if ((!data->enabled) || data->fw_downloding) {
		SENSOR_ERR("can't excute\n");
		return -EPERM;
	}
	
	ret = i2c_smbus_read_byte_data(client, 0xC2);
	if (ret < 0) {
		SENSOR_ERR("failed to read 0xC2 Register (%d)\n", ret);
		return ret;
	}
	
	return sprintf(buf, "%d\n", ret);
}

static ssize_t debug_c3_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct tc3xxk_data *data = dev_get_drvdata(dev);
	struct i2c_client *client = data->client;
	int ret;

	if ((!data->enabled) || data->fw_downloding) {
		SENSOR_ERR("can't excute\n");
		return -EPERM;
	}

	
	ret = i2c_smbus_read_byte_data(client, 0xC3);
	if (ret < 0) {
		SENSOR_ERR("failed to read 0xC3 Register (%d)\n", ret);
		return ret;
	}
	
	return sprintf(buf, "%d\n", ret);
}
static ssize_t tc3xxk_threshold_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct tc3xxk_data *data = dev_get_drvdata(dev);
	struct i2c_client *client = data->client;
	int ret;
	int value;
	u8 threshold_h, threshold_l;

	if ((!data->enabled) || data->fw_downloding) {
		SENSOR_ERR("can't excute\n");
		return -EPERM;
	}

	if (data->pdata->tsk_ic_num == TC350K_TSK_IC) {
		value = read_tc350k_register_data(data, TC350K_1KEY, TC350K_THRES_DATA_OFFSET);
		return sprintf(buf, "%d\n", value);
	} else {
		ret = i2c_smbus_read_byte_data(client, TC300K_THRES_H);
		if (ret < 0) {
			SENSOR_ERR("failed to read threshold_h (%d)\n", ret);
			return ret;
		}
		threshold_h = ret;

		ret = i2c_smbus_read_byte_data(client, TC300K_THRES_L);
		if (ret < 0) {
			SENSOR_ERR("failed to read threshold_l (%d)\n", ret);
			return ret;
		}
		threshold_l = ret;

		data->threhold = (threshold_h << 8) | threshold_l;
		return sprintf(buf, "%d\n", data->threhold);
	}
}

static int load_fw_in_kernel(struct tc3xxk_data *data)
{
	struct i2c_client *client = data->client;
	int ret;

	ret = request_firmware(&data->fw, data->pdata->fw_name, &client->dev);
	if (ret) {
		SENSOR_ERR("fail(%d)\n", ret);
		return -1;
	}
	data->fw_img = (struct fw_image *)data->fw->data;

	SENSOR_INFO( "0x%x 0x%x firm (size=%d)\n",
		data->fw_img->first_fw_ver, data->fw_img->second_fw_ver, data->fw_img->fw_len);
	SENSOR_INFO("done\n");

	return 0;
}

static int load_fw_sdcard(struct tc3xxk_data *data)
{
	struct file *fp;
	mm_segment_t old_fs;
	long fsize, nread;
	int ret = 0;

	old_fs = get_fs();
	set_fs(get_ds());
	fp = filp_open(TC300K_FW_PATH_SDCARD, O_RDONLY, S_IRUSR);
	if (IS_ERR(fp)) {
		SENSOR_ERR("%s open error\n", TC300K_FW_PATH_SDCARD);
		ret = -ENOENT;
		goto fail_sdcard_open;
	}

	fsize = fp->f_path.dentry->d_inode->i_size;

	data->fw_img = kzalloc((size_t)fsize, GFP_KERNEL);
	if (!data->fw_img) {
		SENSOR_ERR("fail to kzalloc for fw\n");
		filp_close(fp, current->files);
		ret = -ENOMEM;
		goto fail_sdcard_kzalloc;
	}

	nread = vfs_read(fp, (char __user *)data->fw_img, fsize, &fp->f_pos);
	if (nread != fsize) {
		SENSOR_ERR("fail to vfs_read file\n");
		ret = -EINVAL;
		goto fail_sdcard_size;
	}
	filp_close(fp, current->files);
	set_fs(old_fs);

	SENSOR_INFO("fw_size : %lu\n", nread);
	SENSOR_INFO("done\n");

	return ret;

fail_sdcard_size:
	kfree(&data->fw_img);
fail_sdcard_kzalloc:
	filp_close(fp, current->files);
fail_sdcard_open:
	set_fs(old_fs);

	return ret;
}

static inline void setsda(struct tc3xxk_data *data, int state)
{
	if (state)
		gpio_direction_output(data->pdata->gpio_sda, 1);
	else
		gpio_direction_output(data->pdata->gpio_sda, 0);
}

static inline void setscl(struct tc3xxk_data *data, int state)
{
	if (state)
		gpio_direction_output(data->pdata->gpio_scl, 1);
	else
		gpio_direction_output(data->pdata->gpio_scl, 0);
}

static inline int getsda(struct tc3xxk_data *data)
{
	return gpio_get_value(data->pdata->gpio_sda);
}

static inline int getscl(struct tc3xxk_data *data)
{
	return gpio_get_value(data->pdata->gpio_scl);
}

static void send_9bit(struct tc3xxk_data *data, u8 buff)
{
	int i;

	setscl(data, 1);
	ndelay(20);
	setsda(data, 0);
	ndelay(20);
	setscl(data, 0);
	ndelay(20);

	for (i = 0; i < 8; i++) {
		setscl(data, 1);
		ndelay(20);
		setsda(data, (buff >> i) & 0x01);
		ndelay(20);
		setscl(data, 0);
		ndelay(20);
	}

	setsda(data, 0);
}

static u8 wait_9bit(struct tc3xxk_data *data)
{
	int i;
	int buf;
	u8 send_buf = 0;

	gpio_direction_input(data->pdata->gpio_sda);

	getsda(data);
	ndelay(10);
	setscl(data, 1);
	ndelay(40);
	setscl(data, 0);
	ndelay(20);

	for (i = 0; i < 8; i++) {
		setscl(data, 1);
		ndelay(20);
		buf = getsda(data);
		ndelay(20);
		setscl(data, 0);
		ndelay(20);
		send_buf |= (buf & 0x01) << i;
	}
	setsda(data, 0);

	return send_buf;
}

static void tc3xxk_reset_for_isp(struct tc3xxk_data *data, bool start)
{
	if (start) {
		setscl(data, 0);
		setsda(data, 0);
		data->pdata->power(data, false);

		msleep(100);

		data->pdata->power(data, true);

		usleep_range(5000, 6000);
	} else {
		data->pdata->power(data, false);
		msleep(100);

		data->pdata->power(data, true);
		msleep(120);

		gpio_direction_input(data->pdata->gpio_sda);
		gpio_direction_input(data->pdata->gpio_scl);
	}
}

static void load(struct tc3xxk_data *data, u8 buff)
{
    send_9bit(data, TC300K_LDDATA);
    udelay(1);
    send_9bit(data, buff);
    udelay(1);
}

static void step(struct tc3xxk_data *data, u8 buff)
{
    send_9bit(data, TC300K_CCFG);
    udelay(1);
    send_9bit(data, buff);
    udelay(2);
}

static void setpc(struct tc3xxk_data *data, u16 addr)
{
    u8 buf[4];
    int i;

    buf[0] = 0x02;
    buf[1] = addr >> 8;
    buf[2] = addr & 0xff;
    buf[3] = 0x00;

    for (i = 0; i < 4; i++)
        step(data, buf[i]);
}

static void configure_isp(struct tc3xxk_data *data)
{
    u8 buf[7];
    int i;

    buf[0] = 0x75;    buf[1] = 0xFC;    buf[2] = 0xAC;
    buf[3] = 0x75;    buf[4] = 0xFC;    buf[5] = 0x35;
    buf[6] = 0x00;

    /* Step(cmd) */
    for (i = 0; i < 7; i++)
        step(data, buf[i]);
}

static int tc3xxk_erase_fw(struct tc3xxk_data *data)
{
	int i;
	u8 state = 0;

	tc3xxk_reset_for_isp(data, true);

	/* isp_enable_condition */
	send_9bit(data, TC300K_CSYNC1);
	udelay(9);
	send_9bit(data, TC300K_CSYNC2);
	udelay(9);
	send_9bit(data, TC300K_CSYNC3);
	usleep_range(150, 160);

	state = wait_9bit(data);
	if (state != 0x01) {
		SENSOR_ERR("isp enable error %d\n", state);
		return -1;
	}

	configure_isp(data);

	/* Full Chip Erase */
	send_9bit(data, TC300K_PCRST);
	udelay(1);
	send_9bit(data, TC300K_PECHIP);
	usleep_range(15000, 15500);


	state = 0;
	for (i = 0; i < 100; i++) {
		udelay(2);
		send_9bit(data, TC300K_CSYNC3);
		udelay(1);

		state = wait_9bit(data);
		if ((state & 0x04) == 0x00)
			break;
	}

	if (i == 100) {
		SENSOR_ERR("fail\n");
		return -1;
	}

	SENSOR_INFO("success\n");
	return 0;
}

static int tc3xxk_write_fw(struct tc3xxk_data *data)
{
	u16 addr = 0;
	u8 code_data;

	setpc(data, addr);
	load(data, TC300K_PWDATA);
	send_9bit(data, TC300K_LDMODE);
	udelay(1);

	while (addr < data->fw_img->fw_len) {
		code_data = data->fw_img->data[addr++];
		load(data, code_data);
		usleep_range(20, 21);
	}

	send_9bit(data, TC300K_PEDISC);
	udelay(1);

	return 0;
}

static int tc3xxk_verify_fw(struct tc3xxk_data *data)
{
	u16 addr = 0;
	u8 code_data;

	setpc(data, addr);

	SENSOR_INFO( "fw code size = %#x (%u)",
		data->fw_img->fw_len, data->fw_img->fw_len);
	while (addr < data->fw_img->fw_len) {
		if ((addr % 0x40) == 0)
			SENSOR_INFO("fw verify addr = %#x\n", addr);

		send_9bit(data, TC300K_PRDATA);
		udelay(2);
		code_data = wait_9bit(data);
		udelay(1);

		if (code_data != data->fw_img->data[addr++]) {
			SENSOR_ERR("addr : %#x data error (0x%2x)\n",
				addr - 1, code_data );
			return -1;
		}
	}
	SENSOR_INFO("success\n");

	return 0;
}

static void t300k_release_fw(struct tc3xxk_data *data, u8 fw_path)
{
	if (fw_path == FW_INKERNEL)
		release_firmware(data->fw);
	else if (fw_path == FW_SDCARD)
		kfree(data->fw_img);
}

static int tc3xxk_flash_fw(struct tc3xxk_data *data, u8 fw_path)
{
	int retry = 5;
	int ret;
	tc3xxk_config_gpio_i2c(data, 0);
	do {
		ret = tc3xxk_erase_fw(data);
		if (ret)
			SENSOR_ERR("erase fail(retry=%d)\n", retry);
		else
			break;
	} while (retry-- > 0);
	if (retry < 0)
		goto err_tc3xxk_flash_fw;

	retry = 5;
	do {
		tc3xxk_write_fw(data);

		ret = tc3xxk_verify_fw(data);
		if (ret)
			SENSOR_ERR("verify fail(retry=%d)\n", retry);
		else
			break;
	} while (retry-- > 0);

	tc3xxk_reset_for_isp(data, false);
	tc3xxk_config_gpio_i2c(data, 1);
	if (retry < 0)
		goto err_tc3xxk_flash_fw;

	return 0;

err_tc3xxk_flash_fw:

	return -1;
}

static int tc3xxk_crc_check(struct tc3xxk_data *data)
{
	struct i2c_client *client = data->client;
	int ret;
	u8 cmd;
	u8 checksum_h, checksum_l;

	if ((!data->enabled) || data->fw_downloding) {
		SENSOR_ERR("can't excute\n");
		return -1;
	}

	cmd = TC300K_CMD_CAL_CHECKSUM;
	ret = i2c_smbus_write_byte_data(client, TC300K_CMD_ADDR, cmd);
	if (ret) {
		SENSOR_ERR("command fail (%d)\n", ret);
		return ret;
	}

	msleep(TC300K_CHECKSUM_DELAY);

	ret = i2c_smbus_read_byte_data(client, TC300K_CHECKS_H);
	if (ret < 0) {
		SENSOR_ERR("failed to read checksum_h (%d)\n", ret);
		return ret;
	}
	checksum_h = ret;

	ret = i2c_smbus_read_byte_data(client, TC300K_CHECKS_L);
	if (ret < 0) {
		SENSOR_ERR("failed to read checksum_l (%d)\n", ret);
		return ret;
	}
	checksum_l = ret;

	data->checksum = (checksum_h << 8) | checksum_l;

	if (data->fw_img->checksum != data->checksum) {
		SENSOR_ERR("checksum fail - firm checksum(%d), compute checksum(%d)\n", 
			data->fw_img->checksum, data->checksum);
		return -1;
	}

	SENSOR_INFO("success (%d)\n", data->checksum);

	return 0;
}

static int tc3xxk_fw_update(struct tc3xxk_data *data, u8 fw_path, bool force)
{
	int retry = 4;
	int ret;

	if (fw_path == FW_INKERNEL) {
		ret = load_fw_in_kernel(data);
		if (ret)
			return -1;

		data->fw_ver_bin = data->fw_img->first_fw_ver;
		data->md_ver_bin = data->fw_img->second_fw_ver;

		/* read model ver */
		ret = tc3xxk_get_md_version(data, false);
		if (ret) {
			SENSOR_ERR("get md version fail\n");
			force = 1;
		}

		if (data->md_ver != data->md_ver_bin) {
			SENSOR_INFO( "fw model number = %x ic model number = %x \n", data->md_ver_bin, data->md_ver);
			force = 1;
		}

		if (!force && (data->fw_ver >= data->fw_ver_bin)) {
			SENSOR_INFO( "do not need firm update (IC:0x%x, BIN:0x%x)(MD IC:0x%x, BIN:0x%x)\n",
				data->fw_ver, data->fw_ver_bin, data->md_ver, data->md_ver_bin);
			t300k_release_fw(data, fw_path);
			return 0;
		}
	} else if (fw_path == FW_SDCARD) {
		ret = load_fw_sdcard(data);
		if (ret)
			return -1;
	}

	while (retry--) {
		data->fw_downloding = true;
		ret = tc3xxk_flash_fw(data, fw_path);
		data->fw_downloding = false;
		if (ret) {
			SENSOR_ERR("tc3xxk_flash_fw fail (%d)\n", retry);
			continue;
		}

		ret = tc3xxk_get_fw_version(data, false);
		if (ret) {
			SENSOR_ERR("tc3xxk_get_fw_version fail (%d)\n", retry);
			continue;
		}
		if (data->fw_ver != data->fw_img->first_fw_ver) {
			SENSOR_ERR("fw version fail (0x%x, 0x%x)(%d)\n",
				data->fw_ver, data->fw_img->first_fw_ver, retry);
			continue;
		}

		ret = tc3xxk_get_md_version(data, false);
		if (ret) {
			SENSOR_ERR("tc3xxk_get_md_version fail (%d)\n", retry);
			continue;
		}
		if (data->md_ver != data->fw_img->second_fw_ver) {
			SENSOR_ERR("md version fail (0x%x, 0x%x)(%d)\n", 
				data->md_ver, data->fw_img->second_fw_ver, retry);
			continue;
		}
		ret = tc3xxk_crc_check(data);
		if (ret) {
			SENSOR_ERR("crc check fail (%d)\n", retry);
			continue;
		}
		break;
	}

	if (retry > 0)
		SENSOR_INFO("success\n");

	t300k_release_fw(data, fw_path);

	return ret;
}

/*
 * Fw update by parameters:
 * s | S = TSK FW from kernel binary and compare fw version.
 * i | I = TSK FW from SD Card and Not compare fw version.
 * f | F = TSK FW from kernel binary and Not compare fw version.
 */
static ssize_t tc3xxk_update_store(struct device *dev,
	 struct device_attribute *attr, const char *buf, size_t count)
{
	struct tc3xxk_data *data = dev_get_drvdata(dev);
	struct i2c_client *client = data->client;
	int ret;
	u8 fw_path;
	bool fw_update_force = false;

	switch(*buf) {
	case 's':
	case 'S':
		fw_path = FW_INKERNEL;
		fw_update_force = true;
		break;
	case 'i':
	case 'I':
		fw_path = FW_SDCARD;
		break;
	case 'f':
	case 'F':
		fw_path = FW_INKERNEL;
		fw_update_force = true;
		break;
	default:
		SENSOR_ERR("wrong command fail\n");
		data->fw_update_status = TK_UPDATE_FAIL;
		return count;
	}

	data->fw_update_status = TK_UPDATE_DOWN;

	disable_irq(client->irq);
	ret = tc3xxk_fw_update(data, fw_path, fw_update_force);
	enable_irq(client->irq);
	if (ret < 0) {
		SENSOR_ERR("fail\n");
		data->fw_update_status = TK_UPDATE_FAIL;
	} else
		data->fw_update_status = TK_UPDATE_PASS;

	return count;
}

static ssize_t tc3xxk_firm_status_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct tc3xxk_data *data = dev_get_drvdata(dev);
	int ret;

	if (data->fw_update_status == TK_UPDATE_PASS)
		ret = sprintf(buf, "PASS\n");
	else if (data->fw_update_status == TK_UPDATE_DOWN)
		ret = sprintf(buf, "DOWNLOADING\n");
	else if (data->fw_update_status == TK_UPDATE_FAIL)
		ret = sprintf(buf, "FAIL\n");
	else
		ret = sprintf(buf, "NG\n");

	return ret;
}

static ssize_t tc3xxk_firm_version_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct tc3xxk_data *data = dev_get_drvdata(dev);

	return sprintf(buf, "0x%02x%02x\n", data->md_ver_bin, data->fw_ver_bin);
}

static ssize_t tc3xxk_md_version_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct tc3xxk_data *data = dev_get_drvdata(dev);

	return sprintf(buf, "0x%02x\n", data->md_ver_bin);
}

static ssize_t tc3xxk_firm_version_read_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct tc3xxk_data *data = dev_get_drvdata(dev);
	int ret;

	ret = tc3xxk_get_fw_version(data, false);
	if (ret < 0)
		SENSOR_ERR("failed to read firmware version (%d)\n", ret);

	ret = tc3xxk_get_md_version(data, false);
	if (ret < 0)
		SENSOR_ERR("failed to read md version (%d)\n", ret);

	return sprintf(buf, "0x%02x%02x\n", data->md_ver, data->fw_ver);
}

static ssize_t tc3xxk_md_version_read_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct tc3xxk_data *data = dev_get_drvdata(dev);
	int ret;

	ret = tc3xxk_get_md_version(data, false);
	if (ret < 0)
		SENSOR_ERR("failed to read md version (%d)\n", ret);

	return sprintf(buf, "0x%02x\n", data->md_ver);
}

static ssize_t recent_key_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct tc3xxk_data *data = dev_get_drvdata(dev);
	struct i2c_client *client = data->client;
	int ret;
	u8 buff[8];
	int value;

	if ((!data->enabled) || data->fw_downloding) {
		SENSOR_ERR("can't excute\n");
		return -1;
	}

	if (data->pdata->tsk_ic_num == TC350K_TSK_IC) {
		value = read_tc350k_register_data(data, TC350K_1KEY, TC350K_CH_PER_DATA_OFFSET);
	} else {
		ret = i2c_smbus_read_i2c_block_data(client, TC300K_2KEY_DATA, 6, buff);
		if (ret != 6) {
			SENSOR_ERR("read fail(%d)\n", ret);
			return -1;
		}
		value = (buff[TC300K_CH_PCK_H_OFFSET] << 8) |
			buff[TC300K_CH_PCK_L_OFFSET];
	}
	return sprintf(buf, "%d\n", value);
}

static ssize_t recent_key_ref_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct tc3xxk_data *data = dev_get_drvdata(dev);
	struct i2c_client *client = data->client;
	int ret;
	u8 buff[8];
	int value;

	if ((!data->enabled) || data->fw_downloding) {
		SENSOR_ERR("can't excute\n");
		return -1;
	}

	if (data->pdata->sensing_ch_num < 6)
		return sprintf(buf, "%d\n", 0);

	ret = i2c_smbus_read_i2c_block_data(client, TC300K_6KEY_DATA, 6, buff);
	if (ret != 6) {
		SENSOR_ERR("read fail(%d)\n", ret);
		return -1;
	}
	value = (buff[TC300K_CH_PCK_H_OFFSET] << 8) |
			buff[TC300K_CH_PCK_L_OFFSET];

	return sprintf(buf, "%d\n", value);
}

static ssize_t back_key_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct tc3xxk_data *data = dev_get_drvdata(dev);
	struct i2c_client *client = data->client;
	int ret;
	u8 buff[8];
	int value;

	if ((!data->enabled) || data->fw_downloding) {
		SENSOR_ERR("can't excute\n");
		return -1;
	}

	if (data->pdata->tsk_ic_num == TC350K_TSK_IC) {
		value = read_tc350k_register_data(data, TC350K_2KEY, TC350K_CH_PER_DATA_OFFSET);
	} else {
		ret = i2c_smbus_read_i2c_block_data(client, TC300K_1KEY_DATA, 6, buff);
		if (ret != 6) {
			SENSOR_ERR("read fail(%d)\n", ret);
			return -1;
		}
		value = (buff[TC300K_CH_PCK_H_OFFSET] << 8) |
			buff[TC300K_CH_PCK_L_OFFSET];
	}
	return sprintf(buf, "%d\n", value);
}

static ssize_t back_key_ref_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct tc3xxk_data *data = dev_get_drvdata(dev);
	struct i2c_client *client = data->client;
	int ret;
	u8 buff[8];
	int value;

	if ((!data->enabled) || data->fw_downloding) {
		SENSOR_ERR("can't excute\n");
		return -1;
	}

	if (data->pdata->sensing_ch_num < 6)
		return sprintf(buf, "%d\n", 0);

	ret = i2c_smbus_read_i2c_block_data(client, TC300K_5KEY_DATA, 6, buff);
	if (ret != 6) {
		SENSOR_ERR("read fail(%d)\n", ret);
		return -1;
	}
	value = (buff[TC300K_CH_PCK_H_OFFSET] << 8) |
		buff[TC300K_CH_PCK_L_OFFSET];

	return sprintf(buf, "%d\n", value);
}

static ssize_t dummy_recent_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct tc3xxk_data *data = dev_get_drvdata(dev);
	struct i2c_client *client = data->client;
	int ret;
	u8 buff[6];
	int value;

	if ((!data->enabled) || data->fw_downloding) {
		SENSOR_ERR("can't excute\n");
		return -1;
	}

	ret = i2c_smbus_read_i2c_block_data(client, TC300K_4KEY_DATA, 6, buff);
	if (ret != 6) {
		SENSOR_ERR("read fail(%d)\n", ret);
		return -1;
	}
	value = (buff[TC300K_CH_PCK_H_OFFSET] << 8) |
		buff[TC300K_CH_PCK_L_OFFSET];

	return sprintf(buf, "%d\n", value);
}

static ssize_t dummy_back_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct tc3xxk_data *data = dev_get_drvdata(dev);
	struct i2c_client *client = data->client;
	int ret;
	u8 buff[6];
	int value;

	if ((!data->enabled) || data->fw_downloding) {
		SENSOR_ERR("can't excute\n");
		return -1;
	}

	ret = i2c_smbus_read_i2c_block_data(client, TC300K_3KEY_DATA, 6, buff);
	if (ret != 6) {
		SENSOR_ERR("read fail(%d)\n", ret);
		return -1;
	}
	value = (buff[TC300K_CH_PCK_H_OFFSET] << 8) |
		buff[TC300K_CH_PCK_L_OFFSET];

	return sprintf(buf, "%d\n", value);
}

static ssize_t recent_key_raw(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct tc3xxk_data *data = dev_get_drvdata(dev);
	struct i2c_client *client = data->client;
	int ret;
	u8 buff[8];
	int value;

	SENSOR_INFO("called!\n");

	if ((!data->enabled) || data->fw_downloding) {
		SENSOR_ERR("can't excute\n");
		return -1;
	}

	if (data->pdata->tsk_ic_num == TC350K_TSK_IC) {
		value = read_tc350k_register_data(data, TC350K_1KEY, TC350K_CH_RAW_DATA_OFFSET);
	} else {
		ret = i2c_smbus_read_i2c_block_data(client, TC300K_2KEY_DATA, 6, buff);

		if (ret != 6) {
			SENSOR_ERR("read fail(%d)\n", ret);
			return -1;
		}
		value = (buff[TC300K_RAW_H_OFFSET] << 8) |
			buff[TC300K_RAW_L_OFFSET];
	}
	return sprintf(buf, "%d\n", value);
}

static ssize_t recent_key_raw_ref(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct tc3xxk_data *data = dev_get_drvdata(dev);
	struct i2c_client *client = data->client;
	int ret;
	u8 buff[8];
	int value;

	if ((!data->enabled) || data->fw_downloding) {
		SENSOR_ERR("can't excute\n");
		return -1;
	}

	if (data->pdata->sensing_ch_num < 6)
		return sprintf(buf, "%d\n", 0);

	ret = i2c_smbus_read_i2c_block_data(client, TC300K_6KEY_DATA, 6, buff);
	if (ret != 6) {
		SENSOR_ERR("read fail(%d)\n", ret);
		return -1;
	}
	value = (buff[TC300K_RAW_H_OFFSET] << 8) |
		buff[TC300K_RAW_L_OFFSET];

	return sprintf(buf, "%d\n", value);
}

static ssize_t back_key_raw(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct tc3xxk_data *data = dev_get_drvdata(dev);
	struct i2c_client *client = data->client;
	int ret;
	u8 buff[8];
	int value;

	SENSOR_INFO("called!\n");

	if ((!data->enabled) || data->fw_downloding) {
		SENSOR_ERR("can't excute\n");
		return -1;
	}

	if (data->pdata->tsk_ic_num == TC350K_TSK_IC) {
		value = read_tc350k_register_data(data, TC350K_2KEY, TC350K_CH_RAW_DATA_OFFSET);
	} else {
		ret = i2c_smbus_read_i2c_block_data(client, TC300K_1KEY_DATA, 6, buff);
		if (ret != 6) {
			SENSOR_ERR("read fail(%d)\n", ret);
			return -1;
		}
		value = (buff[TC300K_RAW_H_OFFSET] << 8) |
			buff[TC300K_RAW_L_OFFSET];
	}
	return sprintf(buf, "%d\n", value);
}

static ssize_t back_key_raw_ref(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct tc3xxk_data *data = dev_get_drvdata(dev);
	struct i2c_client *client = data->client;
	int ret;
	u8 buff[8];
	int value;

	if ((!data->enabled) || data->fw_downloding) {
		SENSOR_ERR("can't excute\n");
		return -1;
	}


	if (data->pdata->sensing_ch_num < 6)
		return sprintf(buf, "%d\n", 0);

	ret = i2c_smbus_read_i2c_block_data(client, TC300K_5KEY_DATA, 6, buff);
	if (ret != 6) {
		SENSOR_ERR("read fail(%d)\n", ret);
		return -1;
	}
	value = (buff[TC300K_RAW_H_OFFSET] << 8) |
		buff[TC300K_RAW_L_OFFSET];

	return sprintf(buf, "%d\n", value);
}

static ssize_t dummy_recent_raw(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct tc3xxk_data *data = dev_get_drvdata(dev);
	struct i2c_client *client = data->client;
	int ret;
	u8 buff[6];
	int value;

	if ((!data->enabled) || data->fw_downloding) {
		SENSOR_ERR("can't excute\n");
		return -1;
	}

	ret = i2c_smbus_read_i2c_block_data(client, TC300K_4KEY_DATA, 6, buff);
	if (ret != 6) {
		SENSOR_ERR("read fail(%d)\n", ret);
		return -1;
	}
	value = (buff[TC300K_RAW_H_OFFSET] << 8) |
		buff[TC300K_RAW_L_OFFSET];

	return sprintf(buf, "%d\n", value);
}

static ssize_t dummy_back_raw(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct tc3xxk_data *data = dev_get_drvdata(dev);
	struct i2c_client *client = data->client;
	int ret;
	u8 buff[6];
	int value;

	if ((!data->enabled) || data->fw_downloding) {
		SENSOR_ERR("can't excute\n");
		return -1;
	}

	ret = i2c_smbus_read_i2c_block_data(client, TC300K_3KEY_DATA, 6, buff);
	if (ret != 6) {
		SENSOR_ERR("read fail(%d)\n", ret);
		return -1;
	}
	value = (buff[TC300K_RAW_H_OFFSET] << 8) |
		buff[TC300K_RAW_L_OFFSET];

	return sprintf(buf, "%d\n", value);
}

static int read_tc350k_register_data(struct tc3xxk_data *data, int read_key_num, int read_offset)
{
	struct i2c_client *client = data->client;
	int ret;
	u8 buff[2];
	int value;

	mutex_lock(&data->lock_fac);
	ret = i2c_smbus_read_i2c_block_data(client, read_key_num + read_offset, TC350K_DATA_SIZE, buff);
	if (ret != TC350K_DATA_SIZE) {
		SENSOR_ERR("read fail(%d)\n", ret);
		value = 0;
		goto exit;
	}
	value = (buff[TC350K_DATA_H_OFFSET] << 8) | buff[TC350K_DATA_L_OFFSET];
	mutex_unlock(&data->lock_fac);

	SENSOR_INFO("read key num/offset = [0x%X/0x%X], value : [%d]\n",
								read_key_num, read_offset, value);

exit:
	return value;
}

static ssize_t back_raw_inner(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct tc3xxk_data *data = dev_get_drvdata(dev);
	int value;

	if ((!data->enabled) || data->fw_downloding) {
		SENSOR_ERR("can't excute\n");
		return -1;
	}

	value = read_tc350k_register_data(data, TC350K_2KEY, TC350K_CH_RAW_DATA_OFFSET);

	return sprintf(buf, "%d\n", value);
}
static ssize_t back_raw_outer(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct tc3xxk_data *data = dev_get_drvdata(dev);
	int value;

	if ((!data->enabled) || data->fw_downloding) {
		SENSOR_ERR("can't excute\n");
		return -1;
	}

	value = read_tc350k_register_data(data, TC350K_4KEY, TC350K_CH_RAW_DATA_OFFSET);

	return sprintf(buf, "%d\n", value);
}
static ssize_t recent_raw_inner(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct tc3xxk_data *data = dev_get_drvdata(dev);
	int value;

	if ((!data->enabled) || data->fw_downloding) {
		SENSOR_ERR("can't excute\n");
		return -1;
	}

	value = read_tc350k_register_data(data, TC350K_1KEY, TC350K_CH_RAW_DATA_OFFSET);

	return sprintf(buf, "%d\n", value);
}
static ssize_t recent_raw_outer(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct tc3xxk_data *data = dev_get_drvdata(dev);
	int value;

	if ((!data->enabled) || data->fw_downloding) {
		SENSOR_ERR("can't excute\n");
		return -1;
	}

	value = read_tc350k_register_data(data, TC350K_3KEY, TC350K_CH_RAW_DATA_OFFSET);

	return sprintf(buf, "%d\n", value);
}
static ssize_t back_idac_inner(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct tc3xxk_data *data = dev_get_drvdata(dev);
	int value;

	if ((!data->enabled) || data->fw_downloding) {
		SENSOR_ERR("can't excute\n");
		return -1;
	}

	value = read_tc350k_register_data(data, TC350K_2KEY, TC350K_CH_DIFF_DATA_OFFSET);

	return sprintf(buf, "%d\n", value);
}
static ssize_t back_idac_outer(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct tc3xxk_data *data = dev_get_drvdata(dev);
	int value;

	if ((!data->enabled) || data->fw_downloding) {
		SENSOR_ERR("can't excute\n");
		return -1;
	}

	value = read_tc350k_register_data(data, TC350K_4KEY, TC350K_CH_DIFF_DATA_OFFSET);

	return sprintf(buf, "%d\n", value);
}
static ssize_t recent_idac_inner(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct tc3xxk_data *data = dev_get_drvdata(dev);
	int value;

	if ((!data->enabled) || data->fw_downloding) {
		SENSOR_ERR("can't excute\n");
		return -1;
	}

	value = read_tc350k_register_data(data, TC350K_1KEY, TC350K_CH_DIFF_DATA_OFFSET);

	return sprintf(buf, "%d\n", value);
}
static ssize_t recent_idac_outer(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct tc3xxk_data *data = dev_get_drvdata(dev);
	int value;

	if ((!data->enabled) || data->fw_downloding) {
		SENSOR_ERR("can't excute\n");
		return -1;
	}

	value = read_tc350k_register_data(data, TC350K_3KEY, TC350K_CH_DIFF_DATA_OFFSET);

	return sprintf(buf, "%d\n", value);
}
static ssize_t back_inner(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct tc3xxk_data *data = dev_get_drvdata(dev);
	int value;

	if ((!data->enabled) || data->fw_downloding) {
		SENSOR_ERR("can't excute\n");
		return -1;
	}

	value = read_tc350k_register_data(data, TC350K_2KEY, TC350K_CH_PER_DATA_OFFSET);

	return sprintf(buf, "%d\n", value);
}
static ssize_t back_outer(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct tc3xxk_data *data = dev_get_drvdata(dev);
	int value;

	if ((!data->enabled) || data->fw_downloding) {
		SENSOR_ERR("can't excute\n");
		return -1;
	}

	value = read_tc350k_register_data(data, TC350K_4KEY, TC350K_CH_PER_DATA_OFFSET);

	return sprintf(buf, "%d\n", value);
}
static ssize_t recent_inner(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct tc3xxk_data *data = dev_get_drvdata(dev);
	int value;

	if ((!data->enabled) || data->fw_downloding) {
		SENSOR_ERR("can't excute\n");
		return -1;
	}

	value = read_tc350k_register_data(data, TC350K_1KEY, TC350K_CH_PER_DATA_OFFSET);

	return sprintf(buf, "%d\n", value);
}
static ssize_t recent_outer(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct tc3xxk_data *data = dev_get_drvdata(dev);
	int value;

	if ((!data->enabled) || data->fw_downloding) {
		SENSOR_ERR("can't excute\n");
		return -1;
	}

	value = read_tc350k_register_data(data, TC350K_3KEY, TC350K_CH_PER_DATA_OFFSET);

	return sprintf(buf, "%d\n", value);
}
static ssize_t back_threshold_inner(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct tc3xxk_data *data = dev_get_drvdata(dev);
	int value;

	if ((!data->enabled) || data->fw_downloding) {
		SENSOR_ERR("can't excute\n");
		return -1;
	}

	value = read_tc350k_register_data(data, TC350K_2KEY, TC350K_THRES_DATA_OFFSET);

	return sprintf(buf, "%d\n", value);
}
static ssize_t back_threshold_outer(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct tc3xxk_data *data = dev_get_drvdata(dev);
	int value;

	if ((!data->enabled) || data->fw_downloding) {
		SENSOR_ERR("can't excute\n");
		return -1;
	}

	value = read_tc350k_register_data(data, TC350K_4KEY, TC350K_THRES_DATA_OFFSET);

	return sprintf(buf, "%d\n", value);
}
static ssize_t recent_threshold_inner(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct tc3xxk_data *data = dev_get_drvdata(dev);
	int value;

	if ((!data->enabled) || data->fw_downloding) {
		SENSOR_ERR("can't excute\n");
		return -1;
	}

	value = read_tc350k_register_data(data, TC350K_1KEY, TC350K_THRES_DATA_OFFSET);

	return sprintf(buf, "%d\n", value);
}
static ssize_t recent_threshold_outer(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct tc3xxk_data *data = dev_get_drvdata(dev);
	int value;

	if ((!data->enabled) || data->fw_downloding) {
		SENSOR_ERR("can't excute\n");
		return -1;
	}

	value = read_tc350k_register_data(data, TC350K_3KEY, TC350K_THRES_DATA_OFFSET);

	return sprintf(buf, "%d\n", value);
}

static int tc3xxk_mode_enable(struct i2c_client *client, u8 cmd)
{
	int ret;

	ret = i2c_smbus_write_byte_data(client, TC300K_CMD_ADDR, cmd);
	msleep(30);

	return ret;
}

static ssize_t grip_sar_enable_show(struct device *dev,
			struct device_attribute *attr, char *buf)
{
	struct tc3xxk_data *data = dev_get_drvdata(dev);

	return snprintf(buf, PAGE_SIZE, "%u\n", !data->sar_enable_off);
}

static ssize_t grip_sar_enable(struct device *dev,
		 struct device_attribute *attr, const char *buf,
		 size_t count)
{
	struct tc3xxk_data *data = dev_get_drvdata(dev);
	int buff;
	int ret;
	bool on;
	int cmd;

	ret = sscanf(buf, "%d", &buff);
	if (ret != 1) {
		SENSOR_ERR("cmd read err\n");
		return count;
	}

	SENSOR_INFO(" (%d) \n", buff);
//return count;	//temp

	if (!(buff >= 0 && buff <= 3)) {
		SENSOR_ERR("wrong command(%d)\n", buff);
		return count;
	}

	/*	sar enable param
	  *	0	off
	  *	1	on
	  *	2	force off
	  *	3	force off -> on
	  */

	if (buff == 3) {
		data->sar_enable_off = 0;
		SENSOR_INFO("Power back off _ force off -> on (%d)\n", 
			data->sar_enable);
		if (data->sar_enable)
			buff = 1;
		else
			return count;
	}

	if (data->sar_enable_off) {
		if (buff == 1)
			data->sar_enable = true;
		else
			data->sar_enable = false;
		SENSOR_INFO("skip, Power back off _ force off mode (%d)\n", 
			data->sar_enable);
		return count;
	}

	if (buff == 1) {
		on = true;
		cmd = TC300K_CMD_SAR_ENABLE;
	} else if (buff == 2) {
		on = false;
		data->sar_enable_off = 1;
		cmd = TC300K_CMD_SAR_DISABLE;
	} else {
		on = false;
		cmd = TC300K_CMD_SAR_DISABLE;
	}

	// if sar_mode is on => must send wake-up command
	if (data->sar_mode) {
		ret = tc3xxk_wake_up(data->client, TC300K_CMD_WAKE_UP);
	}

	ret = tc3xxk_mode_enable(data->client, cmd);
	if (ret < 0) {
		SENSOR_ERR("fail(%d)\n", ret);
		return count;
	}


	if (buff == 1) {
		data->sar_enable = true;
	} else {
		input_report_key(data->input_dev, KEY_CP_GRIP, TSK_RELEASE);
		data->grip_event = 0;
		data->sar_enable = false;
	}

	SENSOR_INFO("data:%d on:%d\n", buff, on);
	return count;
}

static ssize_t tc3xxk_grip1_threshold_show(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	struct tc3xxk_data *data = dev_get_drvdata(dev);
	int ret;

	ret = read_tc350k_register_data(data, TC305K_1GRIP, TC305K_GRIP_THD_PRESS);
	if (ret < 0) {
		SENSOR_ERR("fail to read press thd(%d)\n", ret);
		data->grip_p_thd = 0;
		return sprintf(buf, "%d\n", 0);
	}
	data->grip_p_thd = ret;

	ret = read_tc350k_register_data(data, TC305K_1GRIP, TC305K_GRIP_THD_RELEASE);
	if (ret < 0) {
		SENSOR_ERR("fail to read release thd(%d)\n", ret);
		data->grip_r_thd = 0;
		return sprintf(buf, "%d\n", 0);
	}

	data->grip_r_thd = ret;

	ret = read_tc350k_register_data(data, TC305K_1GRIP, TC305K_GRIP_THD_NOISE);
	if (ret < 0) {
		SENSOR_ERR("fail to read noise thd(%d)\n", ret);
		data->grip_n_thd = 0;
		return sprintf(buf, "%d\n", 0);
	}
	data->grip_n_thd = ret;

	return sprintf(buf, "%d,%d,%d\n",
			data->grip_p_thd, data->grip_r_thd, data->grip_n_thd );
}

static ssize_t tc3xxk_grip2_threshold_show(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	struct tc3xxk_data *data = dev_get_drvdata(dev);
	int ret;

	ret = read_tc350k_register_data(data, TC305K_2GRIP, TC305K_GRIP_THD_PRESS);
	if (ret < 0) {
		SENSOR_ERR("fail to read press thd(%d)\n", ret);
		data->grip_p_thd = 0;
		return sprintf(buf, "%d\n", 0);
	}
	data->grip_p_thd = ret;

	ret = read_tc350k_register_data(data, TC305K_2GRIP, TC305K_GRIP_THD_RELEASE);
	if (ret < 0) {
		SENSOR_ERR("fail to read release thd(%d)\n", ret);
		data->grip_r_thd = 0;
		return sprintf(buf, "%d\n", 0);
	}

	data->grip_r_thd = ret;

	ret = read_tc350k_register_data(data, TC305K_2GRIP, TC305K_GRIP_THD_NOISE);
	if (ret < 0) {
		SENSOR_ERR("fail to read noise thd(%d)\n", ret);
		data->grip_n_thd = 0;
		return sprintf(buf, "%d\n", 0);
	}
	data->grip_n_thd = ret;

	return sprintf(buf, "%d,%d,%d\n",
			data->grip_p_thd, data->grip_r_thd, data->grip_n_thd );
}

static ssize_t tc3xxk_total_cap1_show(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	struct tc3xxk_data *data = dev_get_drvdata(dev);
	struct i2c_client *client = data->client;
	int ret;

	ret = i2c_smbus_read_byte_data(client, TC305K_1GRIP + TC305K_GRIP_TOTAL_CAP);
	if (ret < 0) {
		SENSOR_ERR("fail(%d)\n", ret);
		return sprintf(buf, "%d\n", 0);
	}

	return sprintf(buf, "%d\n", ret);
}

static ssize_t tc3xxk_total_cap2_show(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	struct tc3xxk_data *data = dev_get_drvdata(dev);
	struct i2c_client *client = data->client;
	int ret;

	ret = i2c_smbus_read_byte_data(client, TC305K_2GRIP + TC305K_GRIP_TOTAL_CAP);
	if (ret < 0) {
		SENSOR_ERR("fail(%d)\n", ret);
		return sprintf(buf, "%d\n", 0);
	}

	return sprintf(buf, "%d\n", ret);
}

static ssize_t grip_ref_cap_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct tc3xxk_data *data = dev_get_drvdata(dev);
	struct i2c_client *client = data->client;
	int ret;

	ret = i2c_smbus_read_byte_data(client, TC305K_GRIP_REF_CAP);
	if (ret < 0) {
		SENSOR_ERR("fail(%d)\n", ret);
		return sprintf(buf, "%d\n", 0);
	}

	return sprintf(buf, "%d\n", ret);
}

static ssize_t tc3xxk_grip1_show(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	struct tc3xxk_data *data = dev_get_drvdata(dev);
	int ret;

	ret = read_tc350k_register_data(data, TC305K_1GRIP, TC305K_GRIP_DIFF_DATA);
	if (ret < 0) {
		SENSOR_ERR("fail(%d)\n", ret);
		data->grip_s1 = 0;
		return sprintf(buf, "%d\n", 0);
	}
	data->grip_s1 = ret;

	return sprintf(buf, "%d\n", data->grip_s1);
}

static ssize_t tc3xxk_grip2_show(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	struct tc3xxk_data *data = dev_get_drvdata(dev);
	int ret;

	ret = read_tc350k_register_data(data, TC305K_2GRIP, TC305K_GRIP_DIFF_DATA);
	if (ret < 0) {
		SENSOR_ERR("fail(%d)\n", ret);
		data->grip_s1 = 0;
		return sprintf(buf, "%d\n", 0);
	}
	data->grip_s1 = ret;

	return sprintf(buf, "%d\n", data->grip_s1);
}

static ssize_t tc3xxk_grip1_baseline_show(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	struct tc3xxk_data *data = dev_get_drvdata(dev);
	int ret;

	ret = read_tc350k_register_data(data, TC305K_1GRIP, TC305K_GRIP_BASELINE);
	if (ret < 0) {
		SENSOR_ERR("fail(%d)\n", ret);
		data->grip_baseline = 0;
		return sprintf(buf, "%d\n", 0);
	}
	data->grip_baseline = ret;

	return sprintf(buf, "%d\n", data->grip_baseline);
}

static ssize_t tc3xxk_grip2_baseline_show(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	struct tc3xxk_data *data = dev_get_drvdata(dev);
	int ret;

	ret = read_tc350k_register_data(data, TC305K_2GRIP, TC305K_GRIP_BASELINE);
	if (ret < 0) {
		SENSOR_ERR("fail(%d)\n", ret);
		data->grip_baseline = 0;
		return sprintf(buf, "%d\n", 0);
	}
	data->grip_baseline = ret;

	return sprintf(buf, "%d\n", data->grip_baseline);
}

static ssize_t tc3xxk_grip1_raw_show(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	struct tc3xxk_data *data = dev_get_drvdata(dev);
	int ret;

	ret = read_tc350k_register_data(data, TC305K_1GRIP, TC305K_GRIP_RAW_DATA);
	if (ret < 0) {
		SENSOR_ERR("fail(%d)\n", ret);
		data->grip_raw1 = 0;
		data->grip_raw2 = 0;
		return sprintf(buf, "%d\n", 0);
	}
	data->grip_raw1 = ret;
	data->grip_raw2 = 0;

	return sprintf(buf, "%d,%d\n", data->grip_raw1, data->grip_raw2);
}

static ssize_t tc3xxk_grip2_raw_show(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	struct tc3xxk_data *data = dev_get_drvdata(dev);
	int ret;

	ret = read_tc350k_register_data(data, TC305K_2GRIP, TC305K_GRIP_RAW_DATA);
	if (ret < 0) {
		SENSOR_ERR("fail(%d)\n", ret);
		data->grip_raw1 = 0;
		data->grip_raw2 = 0;
		return sprintf(buf, "%d\n", 0);
	}
	data->grip_raw1 = ret;
	data->grip_raw2 = 0;

	return sprintf(buf, "%d,%d\n", data->grip_raw1, data->grip_raw2);
}

static ssize_t tc3xxk_grip_gain_show(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	return sprintf(buf, "%d,%d,%d,%d\n", 0, 0, 0, 0);
}

static ssize_t tc3xxk_grip_check_show(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	struct tc3xxk_data *data = dev_get_drvdata(dev);

	SENSOR_ERR("event:%d\n", data->grip_event);

	return sprintf(buf, "%d\n", data->grip_event);
}

static ssize_t tc3xxk_grip_sw_reset(struct device *dev,
		 struct device_attribute *attr, const char *buf,
		 size_t count)
{
	struct tc3xxk_data *data = dev_get_drvdata(dev);
	int buff;
	int ret;

	ret = sscanf(buf, "%d", &buff);
	if (ret != 1) {
		SENSOR_ERR("cmd read err\n");
		return count;
	}

	if (!(buff == 1)) {
		SENSOR_ERR("wrong command(%d)\n", buff);
		return count;
	}

	data->grip_event = 0;

	SENSOR_INFO("data(%d)\n", buff);

	tc3xxk_grip_cal_reset(data);

	return count;
}

static ssize_t grip_sensing_change(struct device *dev,
		 struct device_attribute *attr, const char *buf,
		 size_t count)
{
	struct tc3xxk_data *data = dev_get_drvdata(dev);
	int ret, buff;

	ret = sscanf(buf, "%d", &buff);
	if (ret != 1) {
		SENSOR_ERR("cmd read err\n");
		return count;
	}

	if (!(buff == 0 || buff == 1)) {
		SENSOR_ERR("wrong command(%d)\n", buff);
		return count;
	}

	grip_sar_sensing(data, buff);

	SENSOR_INFO("earjack (%d)\n", buff);

	return count;
}

#ifndef CONFIG_SAMSUNG_PRODUCT_SHIP
static ssize_t grip_mode_change(struct device *dev,
		 struct device_attribute *attr, const char *buf,
		 size_t count)
{
	struct tc3xxk_data *data = dev_get_drvdata(dev);
	int ret, buff;

	ret = sscanf(buf, "%d", &buff);
	if (ret != 1) {
		SENSOR_ERR("cmd read err\n");
		return count;
	}

	if (!(buff == 0 || buff == 1)) {
		SENSOR_ERR("wrong command(%d)\n", buff);
		return count;
	}

	SENSOR_INFO("data(%d)\n", buff);

	tc3xxk_stop_mode(data, buff);

	return count;
}
#endif

#ifdef CONFIG_SEC_FACTORY
static ssize_t tc3xxk_grip_irq_count_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct tc3xxk_data *data = dev_get_drvdata(dev);

	int result = 0;

	if (data->irq_count)
		result = -1;

	SENSOR_INFO("\n");

	return snprintf(buf, PAGE_SIZE, "%d,%d,%d\n",
		result, data->irq_count, data->max_diff);
}

static ssize_t tc3xxk_grip_irq_count_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	struct tc3xxk_data *data = dev_get_drvdata(dev);

	u8 onoff;
	int ret;

	ret = kstrtou8(buf, 10, &onoff);
	if (ret < 0) {
		SENSOR_ERR("kstrtou8 failed.(%d)\n", ret);
		return count;
	}

	mutex_lock(&data->lock_fac);

	if (onoff == 0) {
		data->abnormal_mode = 0;
	} else if (onoff == 1) {
		data->abnormal_mode = 1;
		data->irq_count = 0;
		data->max_diff = 0;
	} else {
		SENSOR_ERR("unknown value %d\n", onoff);
	}

	mutex_unlock(&data->lock_fac);

	SENSOR_INFO("%d\n", onoff);
	
	return count;
}
#endif //#ifdef CONFIG_SEC_FACTORY

static ssize_t tc3xxk_modecheck_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct tc3xxk_data *data = dev_get_drvdata(dev);
	struct i2c_client *client = data->client;
	int ret;
	u8 mode, glove, run, sar, ta;

	if ((!data->enabled) || data->fw_downloding) {
		SENSOR_ERR("can't excute\n");
		return -EPERM;
	}

	ret = tc3xxk_mode_check(client);
	if (ret < 0)
		return ret;
	else
		mode = ret;

	glove = !!(mode & TC300K_MODE_GLOVE);
	run = !!(mode & TC300K_MODE_RUN);
	sar = !!(mode & TC300K_MODE_SAR);
	ta = !!(mode & TC300K_MODE_TA_CONNECTED);

	SENSOR_INFO("bit:%x, glove:%d, run:%d, sar:%d, ta:%d\n",
			mode, glove, run, sar, ta);
	return sprintf(buf, "bit:%x, glove:%d, run:%d, sar:%d, ta:%d\n",
			mode, glove, run, sar, ta);
}

static ssize_t grip_chip_name(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	return sprintf(buf, "%s\n", MODEL_NAME);
}

static ssize_t grip_vendor_show(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	return sprintf(buf, "%s\n", VENDOR_NAME);
}

static ssize_t grip_crc_check_show(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	struct tc3xxk_data *data = dev_get_drvdata(dev);
	int ret;

	ret = tc3xxk_crc_check(data);

	return sprintf(buf, (ret == 0) ? "OK,%x\n" : "NG,%x\n", data->checksum);
}

static DEVICE_ATTR(touchkey_threshold, S_IRUGO, tc3xxk_threshold_show, NULL);
static DEVICE_ATTR(grip_firm_update, S_IRUGO | S_IWUSR | S_IWGRP,
		NULL, tc3xxk_update_store);
static DEVICE_ATTR(grip_firm_update_status, S_IRUGO,
		tc3xxk_firm_status_show, NULL);
static DEVICE_ATTR(grip_firm_version_phone, S_IRUGO,
		tc3xxk_firm_version_show, NULL);
static DEVICE_ATTR(grip_firm_version_panel, S_IRUGO,
		tc3xxk_firm_version_read_show, NULL);
static DEVICE_ATTR(grip_md_version_phone, S_IRUGO,
		tc3xxk_md_version_show, NULL);
static DEVICE_ATTR(grip_md_version_panel, S_IRUGO,
		tc3xxk_md_version_read_show, NULL);
static DEVICE_ATTR(grip_recent, S_IRUGO, recent_key_show, NULL);
static DEVICE_ATTR(grip_recent_ref, S_IRUGO, recent_key_ref_show, NULL);
static DEVICE_ATTR(grip_back, S_IRUGO, back_key_show, NULL);
static DEVICE_ATTR(grip_back_ref, S_IRUGO, back_key_ref_show, NULL);
static DEVICE_ATTR(grip_d_menu, S_IRUGO, dummy_recent_show, NULL);
static DEVICE_ATTR(grip_d_back, S_IRUGO, dummy_back_show, NULL);
static DEVICE_ATTR(grip_recent_raw, S_IRUGO, recent_key_raw, NULL);
static DEVICE_ATTR(grip_recent_raw_ref, S_IRUGO, recent_key_raw_ref, NULL);
static DEVICE_ATTR(grip_back_raw, S_IRUGO, back_key_raw, NULL);
static DEVICE_ATTR(grip_back_raw_ref, S_IRUGO, back_key_raw_ref, NULL);
static DEVICE_ATTR(grip_d_menu_raw, S_IRUGO, dummy_recent_raw, NULL);
static DEVICE_ATTR(grip_d_back_raw, S_IRUGO, dummy_back_raw, NULL);

/* for tc350k */
static DEVICE_ATTR(grip_back_raw_inner, S_IRUGO, back_raw_inner, NULL);
static DEVICE_ATTR(grip_back_raw_outer, S_IRUGO, back_raw_outer, NULL);
static DEVICE_ATTR(grip_recent_raw_inner, S_IRUGO, recent_raw_inner, NULL);
static DEVICE_ATTR(grip_recent_raw_outer, S_IRUGO, recent_raw_outer, NULL);

static DEVICE_ATTR(grip_back_idac_inner, S_IRUGO, back_idac_inner, NULL);
static DEVICE_ATTR(grip_back_idac_outer, S_IRUGO, back_idac_outer, NULL);
static DEVICE_ATTR(grip_recent_idac_inner, S_IRUGO, recent_idac_inner, NULL);
static DEVICE_ATTR(grip_recent_idac_outer, S_IRUGO, recent_idac_outer, NULL);

static DEVICE_ATTR(grip_back_idac, S_IRUGO, back_idac_inner, NULL);
static DEVICE_ATTR(grip_recent_idac, S_IRUGO, recent_idac_inner, NULL);

static DEVICE_ATTR(grip_back_inner, S_IRUGO, back_inner, NULL);
static DEVICE_ATTR(grip_back_outer, S_IRUGO, back_outer, NULL);
static DEVICE_ATTR(grip_recent_inner, S_IRUGO, recent_inner, NULL);
static DEVICE_ATTR(grip_recent_outer, S_IRUGO, recent_outer, NULL);

static DEVICE_ATTR(grip_recent_threshold_inner, S_IRUGO, recent_threshold_inner, NULL);
static DEVICE_ATTR(grip_back_threshold_inner, S_IRUGO, back_threshold_inner, NULL);
static DEVICE_ATTR(grip_recent_threshold_outer, S_IRUGO, recent_threshold_outer, NULL);
static DEVICE_ATTR(grip_back_threshold_outer, S_IRUGO, back_threshold_outer, NULL);
/* end 350k */

static DEVICE_ATTR(modecheck, S_IRUGO, tc3xxk_modecheck_show, NULL);


static DEVICE_ATTR(grip_keycode, S_IRUGO, keycode_show, NULL);
static DEVICE_ATTR(grip_3rd, S_IRUGO, third_show, NULL);
static DEVICE_ATTR(grip_3rd_raw, S_IRUGO, third_raw_show, NULL);
static DEVICE_ATTR(grip_4th, S_IRUGO, fourth_show, NULL);
static DEVICE_ATTR(grip_4th_raw, S_IRUGO, fourth_raw_show, NULL);
static DEVICE_ATTR(grip_debug0, S_IRUGO, debug_c0_show, NULL);
static DEVICE_ATTR(grip_debug1, S_IRUGO, debug_c1_show, NULL);
static DEVICE_ATTR(grip_debug2, S_IRUGO, debug_c2_show, NULL);
static DEVICE_ATTR(grip_debug3, S_IRUGO, debug_c3_show, NULL);

static DEVICE_ATTR(grip_threshold, S_IRUGO, tc3xxk_grip1_threshold_show, NULL);
static DEVICE_ATTR(grip2ch_threshold, S_IRUGO, tc3xxk_grip2_threshold_show, NULL);
static DEVICE_ATTR(grip_total_cap, S_IRUGO, tc3xxk_total_cap1_show, NULL);
static DEVICE_ATTR(grip_total_cap2ch, S_IRUGO, tc3xxk_total_cap2_show, NULL);
static DEVICE_ATTR(grip_sar_enable, S_IWUSR | S_IWGRP, NULL, grip_sar_enable);
static DEVICE_ATTR(grip_sw_reset, S_IWUSR | S_IWGRP, NULL, tc3xxk_grip_sw_reset);
static DEVICE_ATTR(grip_earjack, S_IWUSR | S_IWGRP, NULL, grip_sensing_change);
static DEVICE_ATTR(grip, S_IRUGO, tc3xxk_grip1_show, NULL);
static DEVICE_ATTR(grip2ch, S_IRUGO, tc3xxk_grip2_show, NULL);
static DEVICE_ATTR(grip_baseline, S_IRUGO, tc3xxk_grip1_baseline_show, NULL);
static DEVICE_ATTR(grip2ch_baseline, S_IRUGO, tc3xxk_grip2_baseline_show, NULL);
static DEVICE_ATTR(grip_raw, S_IRUGO, tc3xxk_grip1_raw_show, NULL);
static DEVICE_ATTR(grip2ch_raw, S_IRUGO, tc3xxk_grip2_raw_show, NULL);
static DEVICE_ATTR(grip_gain, S_IRUGO, tc3xxk_grip_gain_show, NULL);
static DEVICE_ATTR(grip_check, S_IRUGO, tc3xxk_grip_check_show, NULL);
#ifndef CONFIG_SAMSUNG_PRODUCT_SHIP
static DEVICE_ATTR(grip_sar_only_mode,  S_IWUSR | S_IWGRP, NULL, grip_mode_change);
#if 0 // T387V Ref
static DEVICE_ATTR(grip_sar_press_threshold, 0220,
		NULL, grip_sar_press_threshold_store);
static DEVICE_ATTR(grip_sar_release_threshold, 0220,
		NULL, grip_sar_release_threshold_store);
#endif
#endif
#ifdef CONFIG_SEC_FACTORY
static DEVICE_ATTR(grip_irq_count, S_IRUGO | S_IWUSR | S_IWGRP, tc3xxk_grip_irq_count_show, tc3xxk_grip_irq_count_store);
#endif
static DEVICE_ATTR(grip_ref_cap, S_IRUGO, grip_ref_cap_show, NULL);

static DEVICE_ATTR(name, S_IRUGO, grip_chip_name, NULL);
static DEVICE_ATTR(vendor, S_IRUGO, grip_vendor_show, NULL);
static DEVICE_ATTR(grip_crc_check, S_IRUGO, grip_crc_check_show, NULL);

static struct device_attribute *sec_grip_attributes[] = {
	&dev_attr_touchkey_threshold,
	&dev_attr_grip_firm_update,
	&dev_attr_grip_firm_update_status,
	&dev_attr_grip_firm_version_phone,
	&dev_attr_grip_firm_version_panel,
	&dev_attr_grip_md_version_phone,
	&dev_attr_grip_md_version_panel,
	&dev_attr_grip_recent,
	&dev_attr_grip_recent_ref,
	&dev_attr_grip_back,
	&dev_attr_grip_back_ref,
	&dev_attr_grip_d_menu,
	&dev_attr_grip_d_back,
	&dev_attr_grip_recent_raw,
	&dev_attr_grip_recent_raw_ref,
	&dev_attr_grip_back_raw,
	&dev_attr_grip_back_raw_ref,
	&dev_attr_grip_d_menu_raw,
	&dev_attr_grip_d_back_raw,
	&dev_attr_modecheck,
	
	&dev_attr_grip_keycode,
	&dev_attr_grip_3rd,
	&dev_attr_grip_3rd_raw,
	&dev_attr_grip_4th,
	&dev_attr_grip_4th_raw,
	&dev_attr_grip_debug0,
	&dev_attr_grip_debug1,
	&dev_attr_grip_debug2,
	&dev_attr_grip_debug3,
	
	&dev_attr_grip_threshold,
	&dev_attr_grip2ch_threshold,
	&dev_attr_grip_total_cap,
	&dev_attr_grip_total_cap2ch,
	&dev_attr_grip_sar_enable,
	&dev_attr_grip_sw_reset,
	&dev_attr_grip_earjack,
	&dev_attr_grip,
	&dev_attr_grip2ch,
	&dev_attr_grip_baseline,
	&dev_attr_grip2ch_baseline,
	&dev_attr_grip_raw,
	&dev_attr_grip2ch_raw,
	&dev_attr_grip_gain,
	&dev_attr_grip_check,
#ifndef CONFIG_SAMSUNG_PRODUCT_SHIP
	&dev_attr_grip_sar_only_mode,
#endif
#ifdef CONFIG_SEC_FACTORY	
	&dev_attr_grip_irq_count,
#endif	
	&dev_attr_grip_ref_cap,
	
	&dev_attr_name,
	&dev_attr_vendor,
	&dev_attr_grip_crc_check,
	NULL,
};

static DEVICE_ATTR(enable, 0664, grip_sar_enable_show, grip_sar_enable);

static struct attribute *tc3xxk_attributes[] = {
	&dev_attr_enable.attr,
	NULL
};

static struct attribute_group tc3xxk_attribute_group = {
	.attrs = tc3xxk_attributes
};

static struct device_attribute *sec_grip_attributes_350k[] = {
	&dev_attr_grip_firm_update,
	&dev_attr_grip_firm_update_status,
	&dev_attr_grip_firm_version_phone,
	&dev_attr_grip_firm_version_panel,
	&dev_attr_grip_md_version_phone,
	&dev_attr_grip_md_version_panel,

	&dev_attr_grip_back_raw_inner,
	&dev_attr_grip_back_raw_outer,
	&dev_attr_grip_recent_raw_inner,
	&dev_attr_grip_recent_raw_outer,

	&dev_attr_grip_back_idac_inner,
	&dev_attr_grip_back_idac_outer,
	&dev_attr_grip_recent_idac_inner,
	&dev_attr_grip_recent_idac_outer,

	&dev_attr_grip_back_inner,
	&dev_attr_grip_back_outer,
	&dev_attr_grip_recent_inner,
	&dev_attr_grip_recent_outer,

	&dev_attr_grip_recent_threshold_inner,
	&dev_attr_grip_back_threshold_inner,
	&dev_attr_grip_recent_threshold_outer,
	&dev_attr_grip_back_threshold_outer,

	&dev_attr_grip_recent,
	&dev_attr_grip_back,
	&dev_attr_grip_recent_raw,
	&dev_attr_grip_back_raw,
	&dev_attr_grip_recent_idac,
	&dev_attr_grip_back_idac,
	&dev_attr_touchkey_threshold,
	
	&dev_attr_modecheck,

	&dev_attr_grip_keycode,
	&dev_attr_grip_3rd,
	&dev_attr_grip_3rd_raw,
	&dev_attr_grip_4th,
	&dev_attr_grip_4th_raw,
	&dev_attr_grip_debug0,
	&dev_attr_grip_debug1,
	&dev_attr_grip_debug2,
	&dev_attr_grip_debug3,
	
	&dev_attr_grip_threshold,
	&dev_attr_grip2ch_threshold,
	&dev_attr_grip_total_cap,
	&dev_attr_grip_total_cap2ch,
	&dev_attr_grip_sar_enable,
	&dev_attr_grip_sw_reset,
	&dev_attr_grip_earjack,
	&dev_attr_grip,
	&dev_attr_grip2ch,
	&dev_attr_grip_baseline,
	&dev_attr_grip2ch_baseline,
	&dev_attr_grip_raw,
	&dev_attr_grip2ch_raw,
	&dev_attr_grip_gain,
	&dev_attr_grip_check,
#ifndef CONFIG_SAMSUNG_PRODUCT_SHIP
	&dev_attr_grip_sar_only_mode,
#endif
#ifdef CONFIG_SEC_FACTORY	
	&dev_attr_grip_irq_count,
#endif	
	&dev_attr_grip_ref_cap,

	&dev_attr_name,
	&dev_attr_vendor,
	&dev_attr_grip_crc_check,
	NULL,
};

#if defined (CONFIG_VBUS_NOTIFIER)
static int grip_vbus_notification(struct notifier_block *nb,
		unsigned long cmd, void *data)
{
	struct tc3xxk_data *tkey_data = container_of(nb, struct tc3xxk_data, vbus_nb);
	struct i2c_client *client = tkey_data->client;
	vbus_status_t vbus_type = *(vbus_status_t *)data;
	int ret;

	SENSOR_INFO("cmd=%lu, vbus_type=%d\n", cmd, vbus_type);

	switch (vbus_type) {
	case STATUS_VBUS_HIGH:
		SENSOR_INFO("attach\n");
		// if sar_mode is on => must send wake-up command
		if (tkey_data->sar_mode)
			ret = tc3xxk_wake_up(client, TC300K_CMD_WAKE_UP);

		ret = tc3xxk_mode_enable(client, TC300K_CMD_TA_ON);
		if (ret < 0)
			SENSOR_ERR("TA mode ON fail(%d)\n", ret);
		break;
	case STATUS_VBUS_LOW:
		SENSOR_INFO("detach\n");
		// if sar_mode is on => must send wake-up command
		if (tkey_data->sar_mode)
			ret = tc3xxk_wake_up(client, TC300K_CMD_WAKE_UP);

		ret = tc3xxk_mode_enable(client, TC300K_CMD_TA_OFF);
		if (ret < 0)
			SENSOR_ERR("TA mode OFF fail(%d)\n", ret);
		break;
	default:
		break;
	}

	return 0;
}

#endif


static int tc3xxk_fw_check(struct tc3xxk_data *data)
{
	int ret;

	if (data->pdata->bringup) {
		SENSOR_INFO("firmware update skip, bring up\n");
		return 0;
	}

	ret = tc3xxk_get_fw_version(data, true);

	if (ret < 0) {
		SENSOR_ERR("i2c fail...[%d], addr[%d]\n",
			ret, data->client->addr);
		SENSOR_ERR("grip driver unload\n");
		return ret;
	}

	if (data->fw_ver == 0xFF) {
		SENSOR_INFO(
			"fw version 0xFF, Excute firmware update!\n");
		ret = tc3xxk_fw_update(data, FW_INKERNEL, true);
		if (ret)
			return -1;
	} else {
		ret = tc3xxk_fw_update(data, FW_INKERNEL, false);
		if (ret)
			return -1;
	}

	return 0;
}

static int tc3xxk_pinctrl_init(struct tc3xxk_data *data)
{
	struct device *dev = &data->client->dev;
	int i;
	SENSOR_INFO("\n");
	// IRQ
	data->pinctrl_irq = devm_pinctrl_get(dev);
	if (IS_ERR(data->pinctrl_irq)) {
		SENSOR_INFO("Failed to get irq pinctrl\n");
		data->pinctrl_irq = NULL;
		goto i2c_pinctrl_get;
	}
	for (i = 0; i < 2; ++i) {
		data->pin_state[i] = pinctrl_lookup_state(data->pinctrl_irq, str_states[i]);
		if (IS_ERR(data->pin_state[i])) {
			SENSOR_INFO("Failed to get irq pinctrl state\n");
			devm_pinctrl_put(data->pinctrl_irq);
			data->pinctrl_irq = NULL;
			goto i2c_pinctrl_get;
		}
	}

i2c_pinctrl_get:
	/* for h/w i2c */
	dev = data->client->dev.parent->parent;
	SENSOR_INFO("use dev's parent\n");

	// I2C
	data->pinctrl_i2c = devm_pinctrl_get(dev);
	if (IS_ERR(data->pinctrl_i2c)) {
		SENSOR_ERR("Failed to get i2c pinctrl\n");
		goto err_pinctrl_get_i2c;
	}
	for (i = 2; i < 4; ++i) {
		data->pin_state[i] = pinctrl_lookup_state(data->pinctrl_i2c, str_states[i]);
		if (IS_ERR(data->pin_state[i])) {
			SENSOR_ERR("Failed to get i2c pinctrl state\n");
			goto err_pinctrl_get_state_i2c;
		}
	}
	return 0;

err_pinctrl_get_state_i2c:
	devm_pinctrl_put(data->pinctrl_i2c);
err_pinctrl_get_i2c:
	return -ENODEV;
}

static int tc3xxk_pinctrl(struct tc3xxk_data *data, int state)
{
	struct pinctrl *pinctrl_i2c = data->pinctrl_i2c;
	struct pinctrl *pinctrl_irq = data->pinctrl_irq;
	int ret=0;

	switch (state) {
		case I_STATE_ON_IRQ:
		case I_STATE_OFF_IRQ:
			if (pinctrl_irq)
				ret = pinctrl_select_state(pinctrl_irq, data->pin_state[state]);
			break;
		case I_STATE_ON_I2C:
		case I_STATE_OFF_I2C:
			if (pinctrl_i2c)
				ret = pinctrl_select_state(pinctrl_i2c, data->pin_state[state]);
			break;
	}
	if (ret < 0) {
		SENSOR_ERR(
			"Failed to configure tc3xxk_pinctrl state[%d]\n", state);
		return ret;
	}

	return 0;
}

static void tc3xxk_config_gpio_i2c(struct tc3xxk_data *data, int onoff)
{
	SENSOR_ERR("\n");

	tc3xxk_pinctrl(data, onoff ? I_STATE_ON_I2C : I_STATE_OFF_I2C);
	mdelay(100);
}

static int __devinit tc3xxk_probe(struct i2c_client *client,
		const struct i2c_device_id *id)
{
	struct tc3xxk_platform_data *pdata;
	struct tc3xxk_data *data;
	struct input_dev *input_dev;
	int ret=0;
	
	SENSOR_INFO("\n");

#ifdef CONFIG_BATTERY_SAMSUNG
	if (lpcharge == 1) {
		SENSOR_ERR("Do not load driver due to : lpm %d\n", lpcharge);
		return -ENODEV;
	}
#endif

	if (!i2c_check_functionality(client->adapter, I2C_FUNC_I2C)) {
		SENSOR_ERR("i2c_check_functionality fail\n");
		return -EIO;
	}

	if (client->dev.of_node) {
		pdata = devm_kzalloc(&client->dev,
			sizeof(struct tc3xxk_platform_data),
				GFP_KERNEL);
		if (!pdata) {
			SENSOR_ERR("Failed to allocate memory\n");
			goto err_alloc_data;
		}

		ret = tc3xxk_parse_dt(&client->dev, pdata);
		if (ret)
			goto err_alloc_data;
	}else
		pdata = client->dev.platform_data;

	data = kzalloc(sizeof(struct tc3xxk_data), GFP_KERNEL);
	if (!data) {
		SENSOR_ERR("Failed to allocate memory\n");
		ret = -ENOMEM;
		goto err_alloc_data;
	}

	data->pdata = pdata;

	input_dev = input_allocate_device();
	if (!input_dev) {
		SENSOR_ERR("Failed to allocate memory for input device\n");
		ret = -ENOMEM;
		goto err_alloc_input;
	}

	data->input_dev = input_dev;
	data->client = client;

	if (data->pdata == NULL) {
		SENSOR_ERR("failed to get platform data\n");
		ret = -EINVAL;
		goto err_platform_data;
	}
	data->irq = -1;
	mutex_init(&data->lock);
	mutex_init(&data->lock_fac);

	wake_lock_init(&data->grip_wake_lock, WAKE_LOCK_SUSPEND, "grip wake lock");

	pdata->power = tc3xxk_grip_power;

	i2c_set_clientdata(client, data);
	tc3xxk_gpio_request(data);

	ret = tc3xxk_pinctrl_init(data);
	if (ret < 0) {
		SENSOR_ERR(
			"Failed to init pinctrl: %d\n", ret);
		goto err_pinctrl_init;
	}

	if(pdata->boot_on_ldo){
		data->pdata->power(data, true);
	} else {
		data->pdata->power(data, true);
		msleep(200);
	}
	data->enabled = true;

	client->irq = gpio_to_irq(pdata->gpio_int);

	ret = tc3xxk_fw_check(data);
	if (ret) {
		SENSOR_ERR(
			"failed to firmware check(%d)\n", ret);
		goto err_fw_check;
	}

	snprintf(data->phys, sizeof(data->phys),
		"%s/input0", dev_name(&client->dev));
	input_dev->name = MODULE_NAME;    // TSN : TRY to parse form Match table
	input_dev->phys = data->phys;
	input_dev->id.bustype = BUS_I2C;
	input_dev->dev.parent = &client->dev;

	data->grip_ev_val = grip_ev;
	data->grip_num = ARRAY_SIZE(grip_ev)/2;
	SENSOR_INFO( "number of grips = %d\n", data->grip_num);

#if 0
	set_bit(EV_KEY, input_dev->evbit);
	for (i = 0; i < data->grip_num; i++) {
		set_bit(data->grip_ev_val[i].grip_code, input_dev->keybit);
#if !defined(CONFIG_SAMSUNG_PRODUCT_SHIP)
		SENSOR_INFO("gripcode[%d]= %3d\n",
						i, data->grip_ev_val[i].grip_code);
	}
#endif
#else
	input_set_capability(input_dev, EV_REL, REL_MISC);
#endif
	input_set_drvdata(input_dev, data);

	ret = input_register_device(input_dev);
	if (ret) {
		SENSOR_ERR("fail to register input_dev (%d)\n", ret);
		goto err_register_input_dev;
	}
	
	ret = sensors_create_symlink(&data->input_dev->dev.kobj,
					data->input_dev->name);
	if (ret < 0) {
		SENSOR_ERR("Failed to create sysfs symlink\n");
		goto err_sysfs_symlink;
	}
	
	ret = sysfs_create_group(&data->input_dev->dev.kobj,
				&tc3xxk_attribute_group);
	if (ret < 0) {
		SENSOR_ERR("Failed to create sysfs group\n");
		goto err_sysfs_group;
	}
	
	if (data->pdata->tsk_ic_num == TC350K_TSK_IC) {
		ret = sensors_register(&data->dev, data, 
				sec_grip_attributes_350k, MODULE_NAME);
	} else {
		ret = sensors_register(&data->dev, data, sec_grip_attributes,
				MODULE_NAME);
	}
	if (ret) {
		SENSOR_ERR("could not register grip_sensor(%d)\n", ret);
		goto err_sensor_register;
	}
	dev_set_drvdata(data->dev, data);

#if defined (CONFIG_VBUS_NOTIFIER)
	vbus_notifier_register(&data->vbus_nb, grip_vbus_notification,
			       VBUS_NOTIFY_DEV_CHARGER);
#endif

	ret = request_threaded_irq(client->irq, NULL, tc3xxk_interrupt,
				IRQF_DISABLED | IRQF_TRIGGER_FALLING |
				IRQF_ONESHOT, MODEL_NAME, data);
	if (ret < 0) {
		SENSOR_ERR("fail to request irq (%d).\n",
			pdata->gpio_int);
		goto err_request_irq;
	}
	data->irq = pdata->gpio_int;

	ret = tc3xxk_mode_check(client);
	if (ret >= 0) {
		data->sar_enable = !!(ret & TC300K_MODE_SAR);
		SENSOR_INFO("mode %d, sar %d\n", ret, data->sar_enable);
	}
	device_init_wakeup(&client->dev, true);

	SENSOR_INFO("done\n");
	return 0;

err_request_irq:
err_sysfs_group:
	if (data->pdata->tsk_ic_num == TC350K_TSK_IC) {
		sensors_unregister(data->dev, sec_grip_attributes_350k);
	} else {
		sensors_unregister(data->dev, sec_grip_attributes);
	}
err_sensor_register:
	sensors_remove_symlink(&data->input_dev->dev.kobj, input_dev->name);
err_sysfs_symlink:
	input_unregister_device(input_dev);
	input_dev = NULL;
err_register_input_dev:
err_fw_check:
	data->pdata->power(data, false);
err_pinctrl_init:
	mutex_destroy(&data->lock);
	mutex_destroy(&data->lock_fac);
	wake_lock_destroy(&data->grip_wake_lock);
err_platform_data:
	input_free_device(input_dev);
err_alloc_input:
	kfree(data);
err_alloc_data:
	return ret;
}

static int __devexit tc3xxk_remove(struct i2c_client *client)
{
	struct tc3xxk_data *data = i2c_get_clientdata(client);

	device_init_wakeup(&client->dev, false);
	wake_lock_destroy(&data->grip_wake_lock);
	free_irq(client->irq, data);
	input_unregister_device(data->input_dev);
	mutex_destroy(&data->lock);
	mutex_destroy(&data->lock_fac);
	data->pdata->power(data, false);
	gpio_free(data->pdata->gpio_int);
	gpio_free(data->pdata->gpio_sda);
	gpio_free(data->pdata->gpio_scl);
	kfree(data);

	return 0;
}

static void tc3xxk_shutdown(struct i2c_client *client)
{
	struct tc3xxk_data *data = i2c_get_clientdata(client);

	SENSOR_INFO("\n");

	device_init_wakeup(&client->dev, false);
	wake_lock_destroy(&data->grip_wake_lock);
	disable_irq(client->irq);
	data->pdata->power(data, false);
}

static int tc3xxk_suspend(struct device *dev)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct tc3xxk_data *data = i2c_get_clientdata(client);

	SENSOR_INFO("sar_enable(%d)\n", data->sar_enable);
	
	mutex_lock(&data->lock);
	
	tc3xxk_stop_mode(data, 1);

	if (device_may_wakeup(&data->client->dev))
		enable_irq_wake(data->client->irq);

	disable_irq(client->irq);
	
	tc3xxk_pinctrl(data, I_STATE_OFF_IRQ);

	mutex_unlock(&data->lock);

	return 0;
}

static int tc3xxk_resume(struct device *dev)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct tc3xxk_data *data = i2c_get_clientdata(client);

	mutex_lock(&data->lock);

	SENSOR_INFO("sar_enable(%d)\n", data->sar_enable);

	if (device_may_wakeup(&data->client->dev))
		disable_irq_wake(data->client->irq);

	tc3xxk_pinctrl(data, I_STATE_ON_IRQ);
	enable_irq(client->irq);

	tc3xxk_stop_mode(data, 0);

	mutex_unlock(&data->lock);
	return 0;
}

static const struct i2c_device_id tc3xxk_id[] = {
	{MODEL_NAME, 0},
	{ }
};
MODULE_DEVICE_TABLE(i2c, tc3xxk_id);


static struct of_device_id coreriver_match_table[] = {
	{ .compatible = "coreriver,tc3xx-grip",},
	{ },
};

static const struct dev_pm_ops tc3xxk_pm_ops = {
	.suspend = tc3xxk_suspend,
	.resume = tc3xxk_resume,
};

static struct i2c_driver tc3xxk_driver = {
	.probe = tc3xxk_probe,
	.remove = __devexit_p(tc3xxk_remove),
	.shutdown = tc3xxk_shutdown,
	.driver = {
		.name = MODEL_NAME,
		.owner = THIS_MODULE,
		.pm = &tc3xxk_pm_ops,
		.of_match_table = of_match_ptr(coreriver_match_table),
	},
	.id_table = tc3xxk_id,
};

module_i2c_driver(tc3xxk_driver);

MODULE_AUTHOR("Samsung Electronics");
MODULE_DESCRIPTION("Grip Sensor driver for Coreriver TC3XXK");
MODULE_LICENSE("GPL");
