/* tc305k.c -- Linux driver for coreriver chip as touchkey
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
#include <linux/leds.h>
#include <linux/module.h>
#include <linux/mutex.h>
#include <linux/slab.h>
#include <linux/wakelock.h>
#include <linux/workqueue.h>
#include <linux/uaccess.h>
#include <linux/sensor/sensors_core.h>
#include <linux/wakelock.h>
#include <linux/completion.h>
#include <linux/irqchip/mt-eic.h>
#include <linux/regulator/consumer.h>
#include <linux/muic/muic.h>
#include <linux/muic/muic_notifier.h>

/*#ifdef CONFIG_TOUCHKEY_GRIP*/
#define MODULE_NAME              "grip_sensor"

/* TSK IC */
#define tc305k_TSK_IC	0x00

/* registers */
#define tc305k_KEYCODE		0x00
#define tc305k_FWVER		0x01
#define tc305k_MDVER		0x02
#define tc305k_MODE		0x03
#define tc305k_CHECKS_H		0x04
#define tc305k_CHECKS_L		0x05
#define tc305k_THRES_H		0x06
#define tc305k_THRES_L		0x07

#define tc305k_CH_PCK_H_OFFSET	0x00
#define tc305k_CH_PCK_L_OFFSET	0x01
#define tc305k_DIFF_H_OFFSET	0x02
#define tc305k_DIFF_L_OFFSET	0x03
#define tc305k_RAW_H_OFFSET	0x04
#define tc305k_RAW_L_OFFSET	0x05

/* registers for grip sensor */
#define TC305K_GRIP_THD_PRESS		0x30
#define TC305K_GRIP_THD_RELEASE		0x32
#define TC305K_GRIP_THD_NOISE		0x34
#define TC305K_GRIP_CH_PERCENT		0x36
#define TC305K_GRIP_DIFF_DATA		0x38
#define TC305K_GRIP_RAW_DATA		0x3A
#define TC305K_GRIP_BASELINE		0x3C
#define TC305K_GRIP_TOTAL_CAP		0x3E

#define TC350K_THRES_DATA_OFFSET	0x00
#define TC350K_CH_PER_DATA_OFFSET	0x02
#define TC350K_CH_DIFF_DATA_OFFSET	0x04
#define TC350K_CH_RAW_DATA_OFFSET	0x06

#define TC350K_DATA_SIZE		0x02
#define TC350K_DATA_H_OFFSET		0x00
#define TC350K_DATA_L_OFFSET		0x01

/* command */
#define tc305k_CMD_ADDR			0x00
#define tc305k_CMD_LED_ON		0x10
#define tc305k_CMD_LED_OFF		0x20
#define tc305k_CMD_GLOVE_ON		0x30
#define tc305k_CMD_GLOVE_OFF		0x40
#define tc305k_CMD_TA_ON		0x50
#define tc305k_CMD_TA_OFF		0x60
#define tc305k_CMD_CAL_CHECKSUM		0x70
#define tc305k_CMD_KEY_THD_ADJUST	0x80
#define tc305k_CMD_ALLKEY_THD_ADJUST	0x8F
#define tc305k_CMD_STOP_MODE		0x90
#define tc305k_CMD_NORMAL_MODE		0x91
#define tc305k_CMD_SAR_DISABLE		0xA0
#define tc305k_CMD_SAR_ENABLE		0xA1
#define tc305k_CMD_WAKE_UP		0xF0
#define tc305k_CMD_SW_RESET             0xC0

#define tc305k_CMD_DELAY		50

/* mode status bit */
#define tc305k_MODE_TA_CONNECTED	(1 << 0)
#define tc305k_MODE_RUN			(1 << 1)
#define tc305k_MODE_SAR			(1 << 2)
#define tc305k_MODE_GLOVE		(1 << 4)

/* connecter check */
#define SUB_DET_DISABLE			0
#define SUB_DET_ENABLE_CON_OFF		1
#define SUB_DET_ENABLE_CON_ON		2

/* firmware */
#define tc305k_FW_PATH_SDCARD	"/sdcard/Firmware/grip/tc305k.bin"

#define TK_UPDATE_PASS		0
#define TK_UPDATE_DOWN		1
#define TK_UPDATE_FAIL		2

/* ISP command */
#define tc305k_CSYNC1			0xA3
#define tc305k_CSYNC2			0xAC
#define tc305k_CSYNC3			0xA5
#define tc305k_CCFG			0x92
#define tc305k_PRDATA			0x81
#define tc305k_PEDATA			0x82
#define tc305k_PWDATA			0x83
#define tc305k_PECHIP			0x8A
#define tc305k_PEDISC			0xB0
#define tc305k_LDDATA			0xB2
#define tc305k_LDMODE			0xB8
#define tc305k_RDDATA			0xB9
#define tc305k_PCRST			0xB4
#define tc305k_PCRED			0xB5
#define tc305k_PCINC			0xB6
#define tc305k_RDPCH			0xBD

/* ISP delay */
#define tc305k_TSYNC1			300	/* us */
#define tc305k_TSYNC2			50	/* 1ms~50ms */
#define tc305k_TSYNC3			100	/* us */
#define tc305k_TDLY1			1	/* us */
#define tc305k_TDLY2			2	/* us */
#define tc305k_TFERASE			10	/* ms */
#define tc305k_TPROG			20	/* us */

#define tc305k_CHECKSUM_DELAY		500
#define tc305k_POWERON_DELAY		120
#define tc305k_POWEROFF_DELAY		50

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

#define	TSK_RELEASE	0x00
#define	TSK_PRESS	0x01

struct tsk_event_val {
	u16	tsk_bitmap;
	u8	tsk_status;
	int	tsk_keycode;
	char*	tsk_keyname;
};

#define KEY_CP_GRIP		0x2f1	/* grip sensor for CP ,old keycode*/
struct tsk_event_val tsk_ev[6] =
{
        {0x01 << 2, TSK_PRESS, KEY_CP_GRIP, "grip"},
	{0x01 << 6, TSK_RELEASE, KEY_CP_GRIP, "grip"}
};

#define TC305K_NAME "tc305k"

struct tc305k_devicetree_data {
	int gpio_int;
	int gpio_sda;
	int gpio_scl;
	int gpio_sub_det;
	int gpio_en_flag;
        int gpio_en;
	int i2c_gpio;
	u32 irq_gpio_flags;
	u32 sda_gpio_flags;
	u32 scl_gpio_flags;
	bool boot_on_ldo;
	u32 bringup;

	int *keycode;
	int key_num;
	const char *fw_name;
	u32 sensing_ch_num;
	u32 use_bitmap;
	u32 tsk_ic_num;
        struct regulator *vreg_vio;
};

struct tc305k_data {
	struct device *factory_device;
	struct i2c_client *client;
        struct notifier_block tc305k_nb;
	struct input_dev *input_dev;
	struct tc305k_devicetree_data *dtdata;
	struct mutex lock;
	struct mutex lock_fac;
	struct fw_image *fw_img;
	const struct firmware *fw;
	char phys[32];
	int irq;
	u16 checksum;
	u16 threhold;
	int mode;
	u8 fw_ver;
	u8 fw_ver_bin;
	u8 md_ver;
	u8 md_ver_bin;
	u8 fw_update_status;
	bool enabled;
	bool fw_downloding;
	bool glove_mode;
	bool led_on;

	int key_num;
	struct tsk_event_val *tsk_ev_val;
	int (*power) (struct tc305k_data *data, bool on);

	struct pinctrl *pinctrl;
	struct wake_lock touchkey_wake_lock;
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
	struct completion resume_done;
	bool is_lpm_suspend;
};

extern struct class *sec_class;

static void tc305k_input_close(struct input_dev *dev);
static int tc305k_input_open(struct input_dev *dev);
static int tc305k_pinctrl_configure(struct tc305k_data *data, bool active);
static int read_tc350k_register_data(struct tc305k_data *data, int read_key_num, int read_offset);

static int tc305k_mode_enable(struct i2c_client *client, u8 cmd)
{
	int ret;

	ret = i2c_smbus_write_byte_data(client, tc305k_CMD_ADDR, cmd);
	msleep(tc305k_CMD_DELAY);

	return ret;
}

static int tc305k_mode_check(struct i2c_client *client)
{
	int mode = i2c_smbus_read_byte_data(client, tc305k_MODE);
	if (mode < 0)
		input_err(true, &client->dev, "[TK] %s: failed to read mode (%d)\n",
			__func__, mode);

	return mode;
}

static void tc305k_stop_mode(struct tc305k_data *data, bool on)
{
	struct i2c_client *client = data->client;
	int retry = 3;
	int ret;
	u8 cmd;
	int mode_retry = 1;
	bool mode;

	if (data->sar_mode == on){
		input_err(true, &client->dev, "[TK] %s : skip already %s\n",
				__func__, on ? "stop mode":"normal mode");
		return;
	}

	if (on)
		cmd = tc305k_CMD_STOP_MODE;
	else
		cmd = tc305k_CMD_NORMAL_MODE;

	input_info(true, &client->dev, "[TK] %s: %s, cmd=%x\n",
			__func__, on ? "stop mode" : "normal mode", cmd);
sar_mode:
	while (retry > 0) {
		ret = tc305k_mode_enable(client, cmd);
		if (ret < 0) {
			input_err(true, &client->dev, "%s fail to write mode(%d), retry %d\n",
					__func__, ret, retry);
			retry--;
			msleep(20);
			continue;
		}
		break;
	}

	msleep(40);

	retry = 3;
	while (retry > 0) {
		ret = tc305k_mode_check(client);
		if (ret < 0) {
			input_err(true, &client->dev, "%s fail to read mode(%d), retry %d\n",
					__func__, ret, retry);
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
	mode = !(ret & tc305k_MODE_RUN);

	input_info(true, &client->dev, "%s: run mode:%s reg:%x\n",
			__func__, mode ? "stop": "normal", ret);

	if((mode != on) && (mode_retry == 1)){
		input_err(true, &client->dev, "%s change fail retry %d, %d\n", __func__, mode, on);
		mode_retry = 0;
		goto sar_mode;
	}

	data->sar_mode = mode;
}

static void touchkey_sar_sensing(struct tc305k_data *data, bool on)
{
	/* enable/disable sar sensing
	  * need to disable when earjack is connected (FM radio can't work normally)
	  */
}

static void tc305k_release_all_fingers(struct tc305k_data *data)
{
	struct i2c_client *client = data->client;
	int i;

	input_info(true, &client->dev, "[TK] %s\n", __func__);

	for (i = 0; i < data->key_num ; i++) {
		input_report_key(data->input_dev,
			data->tsk_ev_val[i].tsk_keycode, 0);
	}
	input_sync(data->input_dev);
}

static void tc305k_reset(struct tc305k_data *data)
{
	int ret;
	input_info(true, &data->client->dev, "%s", __func__);
	tc305k_release_all_fingers(data);

	disable_irq(data->client->irq);
	data->power(data, false);
	msleep(tc305k_POWEROFF_DELAY);

	data->power(data, true);
	msleep(tc305k_POWERON_DELAY);

	if (data->glove_mode) {
		ret = tc305k_mode_enable(data->client, tc305k_CMD_GLOVE_ON);
		if (ret < 0)
			input_err(true, &data->client->dev, "[TK] %s glovemode fail(%d)\n", __func__, ret);
	}

	if (data->sar_enable) {
                tc305k_mode_enable(data->client, tc305k_CMD_WAKE_UP);
                mdelay(10);
                tc305k_mode_enable(data->client, tc305k_CMD_SAR_ENABLE);
	}

	enable_irq(data->client->irq);

}

static void tc305k_reset_probe(struct tc305k_data *data)
{
	input_info(true, &data->client->dev, "%s", __func__);
	data->power(data, false);
	msleep(tc305k_POWEROFF_DELAY);

	data->power(data, true);
	msleep(tc305k_POWERON_DELAY);
}

int tc305k_get_fw_version(struct tc305k_data *data, bool probe)
{
	struct i2c_client *client = data->client;
	int retry = 3;
	int buf;

	if ((!data->enabled) || data->fw_downloding) {
		input_err(true, &client->dev, "[TK]can't excute %s\n", __func__);
		return -1;
	}

	buf = i2c_smbus_read_byte_data(client, tc305k_FWVER);
	if (buf < 0) {
		while (retry--) {
			input_err(true, &client->dev, "[TK]%s read fail(%d)\n",
				__func__, retry);
			if (probe)
				tc305k_reset_probe(data);
			else
				tc305k_reset(data);
			buf = i2c_smbus_read_byte_data(client, tc305k_FWVER);
			if (buf > 0)
				break;
		}
		if (retry <= 0) {
			input_err(true, &client->dev, "[TK]%s read fail\n", __func__);
			data->fw_ver = 0;
			return -1;
		}
	}
	data->fw_ver = (u8)buf;

	buf = i2c_smbus_read_byte_data(client, tc305k_MDVER);
	if (buf < 0) {
		input_err(true, &client->dev, "[TK] %s: fail to read model ID", __func__);
		data->md_ver = 0;
	} else {
		data->md_ver = (u8)buf;
	}

	input_info(true, &client->dev, "[TK] %s:[IC] fw ver : 0x%x, model id:0x%x\n",
		__func__, data->fw_ver, data->md_ver);

	return 0;
}
static void tc305k_gpio_request(struct tc305k_data *data)
{
	int ret = 0;
	input_info(true, &data->client->dev, "%s: enter \n",__func__);

	ret = gpio_request(data->dtdata->gpio_int, "touchkey_irq");
	if (ret) {
		input_err(true, &data->client->dev, "%s: unable to request touchkey_irq [%d]\n",
				__func__, data->dtdata->gpio_int);
	}

	if (gpio_is_valid(data->dtdata->gpio_en)) {
		ret = gpio_request(data->dtdata->gpio_en, "touchkey_en");
		if (ret) {
			input_err(true, &data->client->dev, "%s: unable to request touchkey_irq [%d]\n",
					__func__, data->dtdata->gpio_int);
		}
	}
}

#ifdef CONFIG_OF
static int tc305k_parse_dt(struct device *dev,
			struct tc305k_devicetree_data *dtdata)
{
        struct device_node *np = dev->of_node;
        int ret = 0;
        of_property_read_u32(np, "coreriver,use_bitmap", &dtdata->use_bitmap);

	dtdata->gpio_scl = of_get_named_gpio(np, "coreriver,scl-gpio", 0);
	if (!gpio_is_valid(dtdata->gpio_scl)) {
		input_err(true, dev, "[TK] %s Failed to get scl %d\n", __func__, dtdata->gpio_scl);
		return -EINVAL;
	}

	dtdata->gpio_sda = of_get_named_gpio(np, "coreriver,sda-gpio", 0);
	if (!gpio_is_valid(dtdata->gpio_sda)) {
		input_err(true, dev, "[TK] %s Failed to get sda %d\n", __func__, dtdata->gpio_sda);
		return -EINVAL;
	}

	dtdata->gpio_int = of_get_named_gpio(np, "coreriver,irq-gpio", 0);
	if (dtdata->gpio_int < 0) {
		input_err(true, dev, "[TK] %s Failed to get int %d\n", __func__, dtdata->gpio_int);
		dtdata->gpio_int = 102;
	}

	dtdata->gpio_en = of_get_named_gpio(np, "coreriver,tkey_en-gpio", 0);
	if (dtdata->gpio_en < 0) {
                dtdata->gpio_en_flag = 0;
		input_err(true, dev, "[TK] %s Failed to get gpio en, gpio_en_flag %d\n", __func__, dtdata->gpio_en_flag);
	} else {
                dtdata->gpio_en_flag = 1;
                input_err(true, dev, "[TK] %s success to get gpio en, gpio_en_flag %d\n", __func__, dtdata->gpio_en_flag);
        }

	dtdata->gpio_sub_det = of_get_named_gpio(np, "coreriver,sub-det-gpio", 0);
        if (dtdata->gpio_sub_det < 0)
		input_info(true, dev, "[TK] %s Failed to get sub-det-gpio[%d] property\n", __func__, dtdata->gpio_sub_det);

	if (of_property_read_string(np, "coreriver,fw_name", &dtdata->fw_name)) {
                dtdata->fw_name = "coreriver/tc305k_GP.fw";
                input_err(true, dev, "[TK] %s Failed to get fw_name property\n",__func__);
        } else {
		input_info(true, dev, "[TK] %s fw_name %s\n", __func__, dtdata->fw_name);
	}

	if (of_property_read_u32(np, "coreriver,sensing_ch_num", &dtdata->sensing_ch_num) < 0){
		input_err(true, dev, "[TK] %s Failed to get sensing_ch_num property\n",__func__);
	}

	if (of_property_read_u32(np, "coreriver,tsk_ic_num", &dtdata->tsk_ic_num) < 0)
		input_info(true, dev, "[TK] %s Failed to get tsk_ic_num, TSK IC is tc305k\n", __func__);

	of_property_read_u32(np, "coreriver,bringup", &dtdata->bringup);
        input_info(true, dev, "%s: get tc305k regulator start\n");
        dtdata->vreg_vio = regulator_get(dev, "avdd");
        if (IS_ERR(dtdata->vreg_vio)) {
		dtdata->vreg_vio = NULL;
		input_info(true, dev, "%s: get touch_regulator error\n",
				__func__);
		if (dtdata->gpio_en_flag) {
			input_info(true, dev, "%s: touch vdd control use gpio_en\n", __func__);
		} else {
			input_info(true, dev, "%s: touch vdd control do not use gpio_en"
				"and don't get regulator error\n", __func__);
			return -EIO;
		}

	}
        ret = regulator_set_voltage(dtdata->vreg_vio, 2800000, 2800000);
	if (ret < 0)
		input_info(true, dev, "%s: set voltage error(%d)\n", __func__, ret);
        
        
	input_info(true, dev, "[TK] %s : %s, scl:%d, sda:%d, irq:%d, gpio-en:%d, sub-det:%d, "
			"fw_name:%s, sensing_ch:%d, tsk_ic:%d,%s\n",
			__func__, dtdata->use_bitmap ? "Use Bit-map" : "Use OLD",
			dtdata->gpio_scl, dtdata->gpio_sda, dtdata->gpio_int, dtdata->gpio_en,
			dtdata->gpio_sub_det, dtdata->fw_name, dtdata->sensing_ch_num,
			dtdata->tsk_ic_num, TC305K_NAME);
	return 0;
}
#else
static int tc305k_parse_dt(struct device *dev,
			struct tc305k_devicetree_data *dtdata)
{
	return -ENODEV;
}
#endif


int tc305k_touchkey_power(struct tc305k_data *data, bool enable)
{
	struct i2c_client *client = data->client;
	int ret = 0;

	if (data->dtdata->gpio_en_flag) {
		gpio_direction_output(data->dtdata->gpio_en, enable);
                
		input_info(true, &client->dev, "%s gpio_direction_ouput:%d\n", __func__, enable);
	}

	if (!IS_ERR_OR_NULL(data->dtdata->vreg_vio)) {
		if (enable) {
			if (!regulator_is_enabled(data->dtdata->vreg_vio)) {
				ret = regulator_enable(data->dtdata->vreg_vio);
				if (ret) {
					input_err(true, &client->dev, "%s [ERROR] tc305k enable "
						"failed  (%d)\n", __func__, ret);
					return -EIO;
				}
				input_info(true, &client->dev, "%s power on\n", __func__);

			} else {
				input_info(true, &client->dev, "%s already power on\n", __func__);
			}
		} else {
			if (regulator_is_enabled(data->dtdata->vreg_vio)) {
				ret = regulator_disable(data->dtdata->vreg_vio);
				if(ret) {
					input_err(true, &client->dev, "%s [ERROR] touch_regulator disable "
						"failed (%d)\n", __func__, ret);
					return -EIO;
				}
				input_info(true, &client->dev, "%s power off\n", __func__);

			} else {
				input_info(true, &client->dev, "%s already power off\n", __func__);
			}
		}

	}

	return 0;
}

static irqreturn_t tc305k_interrupt(int irq, void *dev_id)
{
	struct tc305k_data *data = dev_id;
	struct i2c_client *client = data->client;
	int ret, retry;
	u8 key_val;
	int i = 0;
	bool key_handle_flag;

	if (data->is_lpm_suspend) {
		wake_lock_timeout(&data->touchkey_wake_lock,
				msecs_to_jiffies(3 * MSEC_PER_SEC));
		/* waiting for blsp block resuming, if not occurs i2c error */
		wait_for_completion_interruptible(&data->resume_done);
	}

	input_info(true, &client->dev, "[TK] %s\n",__func__);

	if ((!data->enabled) || data->fw_downloding) {
		input_err(true, &client->dev, "[TK] can't excute %s\n", __func__);
		return IRQ_HANDLED;
	}

	ret = i2c_smbus_read_byte_data(client, tc305k_KEYCODE);
	if (ret < 0) {
		retry = 3;
		while (retry--) {
			input_err(true, &client->dev, "[TK] %s read fail ret=%d(retry:%d)\n",
				__func__, ret, retry);
			msleep(10);
			ret = i2c_smbus_read_byte_data(client, tc305k_KEYCODE);
			if (ret > 0)
				break;
		}
		if (retry <= 0) {
			tc305k_reset(data);
			return IRQ_HANDLED;
		}
	}
	key_val = (u8)ret;

	for (i = 0 ; i < data->key_num * 2 ; i++){
		if (data->dtdata->use_bitmap)
			key_handle_flag = (key_val & data->tsk_ev_val[i].tsk_bitmap);
		else
			key_handle_flag = (key_val == data->tsk_ev_val[i].tsk_bitmap);

		if (key_handle_flag){
			input_report_key(data->input_dev,
				data->tsk_ev_val[i].tsk_keycode, data->tsk_ev_val[i].tsk_status);
                        input_info(true , &client->dev, "[tk] grip %d", data->tsk_ev_val[i].tsk_status);  
			if (data->tsk_ev_val[i].tsk_keycode == KEY_CP_GRIP) {
				data->grip_event = data->tsk_ev_val[i].tsk_status;
			}
		}
	}
        
	input_sync(data->input_dev);
	return IRQ_HANDLED;
}

static int load_fw_in_kernel(struct tc305k_data *data)
{
	struct i2c_client *client = data->client;
	int ret;

	ret = request_firmware(&data->fw, data->dtdata->fw_name, &client->dev);
	if (ret) {
		input_err(true, &client->dev, "[TK] %s fail(%d)\n", __func__, ret);
		return -1;
	}
	data->fw_img = (struct fw_image *)data->fw->data;

	input_info(true, &client->dev, "[TK] [BIN] fw ver : 0x%x (size=%d), model id : 0x%x\n",
			data->fw_img->first_fw_ver, data->fw_img->fw_len,
			data->fw_img->second_fw_ver);

	data->fw_ver_bin = data->fw_img->first_fw_ver;
	data->md_ver_bin = data->fw_img->second_fw_ver;
	return 0;
}

static int load_fw_sdcard(struct tc305k_data *data)
{
	struct i2c_client *client = data->client;
	struct file *fp;
	mm_segment_t old_fs;
	long fsize, nread;
	int ret = 0;

	old_fs = get_fs();
	set_fs(get_ds());
	fp = filp_open(tc305k_FW_PATH_SDCARD, O_RDONLY, S_IRUSR);
	if (IS_ERR(fp)) {
		input_err(true, &client->dev, "[TK] %s %s open error\n",
			__func__, tc305k_FW_PATH_SDCARD);
		ret = -ENOENT;
		goto fail_sdcard_open;
	}

	fsize = fp->f_path.dentry->d_inode->i_size;

	data->fw_img = kzalloc((size_t)fsize, GFP_KERNEL);
	if (!data->fw_img) {
		input_err(true, &client->dev, "[TK] %s fail to kzalloc for fw\n", __func__);
		filp_close(fp, current->files);
		ret = -ENOMEM;
		goto fail_sdcard_kzalloc;
	}

	nread = vfs_read(fp, (char __user *)data->fw_img, fsize, &fp->f_pos);
	if (nread != fsize) {
		input_err(true, &client->dev,
				"[TK] %s fail to vfs_read file\n", __func__);
		ret = -EINVAL;
		goto fail_sdcard_size;
	}
	filp_close(fp, current->files);
	set_fs(old_fs);

	input_info(true, &client->dev, "[TK] fw_size : %lu\n", nread);
	input_info(true, &client->dev, "[TK] %s done\n", __func__);

	return ret;

fail_sdcard_size:
	kfree(&data->fw_img);
fail_sdcard_kzalloc:
	filp_close(fp, current->files);
fail_sdcard_open:
	set_fs(old_fs);

	return ret;
}

static inline void setsda(struct tc305k_data *data, int state)
{
	if (state)
		gpio_direction_output(data->dtdata->gpio_sda, 1);
	else
		gpio_direction_output(data->dtdata->gpio_sda, 0);
}

static inline void setscl(struct tc305k_data *data, int state)
{
	if (state)
		gpio_direction_output(data->dtdata->gpio_scl, 1);
	else
		gpio_direction_output(data->dtdata->gpio_scl, 0);
}

static inline int getsda(struct tc305k_data *data)
{
	return gpio_get_value(data->dtdata->gpio_sda);
}

static inline int getscl(struct tc305k_data *data)
{
	return gpio_get_value(data->dtdata->gpio_scl);
}

static void send_9bit(struct tc305k_data *data, u8 buff)
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

static u8 wait_9bit(struct tc305k_data *data)
{
	int i;
	int buf;
	u8 send_buf = 0;

	gpio_direction_input(data->dtdata->gpio_sda);

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

static void tc305k_reset_for_isp(struct tc305k_data *data, bool start)
{
	if (start) {
		setscl(data, 0);
		setsda(data, 0);
		data->power(data, false);

		msleep(tc305k_POWEROFF_DELAY * 2);

		data->power(data, true);

		usleep_range(5000, 6000);
	} else {
		data->power(data, false);
		msleep(tc305k_POWEROFF_DELAY * 2);

		data->power(data, true);
		msleep(tc305k_POWERON_DELAY);

		gpio_direction_input(data->dtdata->gpio_sda);
		gpio_direction_input(data->dtdata->gpio_scl);
	}
}

static void load(struct tc305k_data *data, u8 buff)
{
    send_9bit(data, tc305k_LDDATA);
    udelay(1);
    send_9bit(data, buff);
    udelay(1);
}

static void step(struct tc305k_data *data, u8 buff)
{
    send_9bit(data, tc305k_CCFG);
    udelay(1);
    send_9bit(data, buff);
    udelay(2);
}

static void setpc(struct tc305k_data *data, u16 addr)
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

static void configure_isp(struct tc305k_data *data)
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

static int tc305k_erase_fw(struct tc305k_data *data)
{
	struct i2c_client *client = data->client;
	int i;
	u8 state = 0;

	tc305k_reset_for_isp(data, true);

	/* isp_enable_condition */
	send_9bit(data, tc305k_CSYNC1);
	udelay(9);
	send_9bit(data, tc305k_CSYNC2);
	udelay(9);
	send_9bit(data, tc305k_CSYNC3);
	usleep_range(150, 160);

	state = wait_9bit(data);
	if (state != 0x01) {
		input_err(true, &client->dev, "[TK] %s isp enable error %d\n", __func__, state);
		return -1;
	}

	configure_isp(data);

	/* Full Chip Erase */
	send_9bit(data, tc305k_PCRST);
	udelay(1);
	send_9bit(data, tc305k_PECHIP);
	usleep_range(15000, 15500);


	state = 0;
	for (i = 0; i < 100; i++) {
		udelay(2);
		send_9bit(data, tc305k_CSYNC3);
		udelay(1);

		state = wait_9bit(data);
		if ((state & 0x04) == 0x00)
			break;
	}

	if (i == 100) {
		input_err(true, &client->dev, "[TK] %s fail\n", __func__);
		return -1;
	}

	input_info(true, &client->dev, "[TK] %s success\n", __func__);
	return 0;
}

static int tc305k_write_fw(struct tc305k_data *data)
{
	u16 addr = 0;
	u8 code_data;

	setpc(data, addr);
	load(data, tc305k_PWDATA);
	send_9bit(data, tc305k_LDMODE);
	udelay(1);

	while (addr < data->fw_img->fw_len) {
		code_data = data->fw_img->data[addr++];
		load(data, code_data);
		usleep_range(20, 21);
	}

	send_9bit(data, tc305k_PEDISC);
	udelay(1);

	return 0;
}

static int tc305k_verify_fw(struct tc305k_data *data)
{
	struct i2c_client *client = data->client;
	u16 addr = 0;
	u8 code_data;

	setpc(data, addr);

	input_info(true, &client->dev, "[TK] fw code size = %#x (%u)",
		data->fw_img->fw_len, data->fw_img->fw_len);
	while (addr < data->fw_img->fw_len) {
		if ((addr % 0x40) == 0)
			input_dbg(true, &client->dev, "[TK] fw verify addr = %#x\n", addr);

		send_9bit(data, tc305k_PRDATA);
		udelay(2);
		code_data = wait_9bit(data);
		udelay(1);

		if (code_data != data->fw_img->data[addr++]) {
			input_err(true, &client->dev,
				"%s addr : %#x data error (0x%2x)\n",
				__func__, addr - 1, code_data );
			return -1;
		}
	}
	input_info(true, &client->dev, "[TK] %s success\n", __func__);

	return 0;
}

static void t300k_release_fw(struct tc305k_data *data, u8 fw_path)
{
	if (fw_path == FW_INKERNEL)
		release_firmware(data->fw);
	else if (fw_path == FW_SDCARD)
		kfree(data->fw_img);
}

static int tc305k_flash_fw(struct tc305k_data *data, u8 fw_path)
{
	struct i2c_client *client = data->client;
	int retry = 5;
	int ret;

	do {
		ret = tc305k_erase_fw(data);
		if (ret)
			input_err(true, &client->dev, "[TK] %s erase fail(retry=%d)\n",
				__func__, retry);
		else
			break;
	} while (retry-- > 0);
	if (retry < 0)
		goto err_tc305k_flash_fw;

	retry = 5;
	do {
		tc305k_write_fw(data);

		ret = tc305k_verify_fw(data);
		if (ret)
			input_err(true, &client->dev, "[TK] %s verify fail(retry=%d)\n",
				__func__, retry);
		else
			break;
	} while (retry-- > 0);

	tc305k_reset_for_isp(data, false);

	if (retry < 0)
		goto err_tc305k_flash_fw;

	return 0;

err_tc305k_flash_fw:

	return -1;
}

static int tc305k_crc_check(struct tc305k_data *data)
{
	struct i2c_client *client = data->client;
	int ret;
	u16 checksum;
	u8 cmd;
	u8 checksum_h, checksum_l;

	if ((!data->enabled) || data->fw_downloding) {
		input_err(true, &client->dev, "[TK] %s can't excute\n", __func__);
		return -1;
	}

	cmd = tc305k_CMD_CAL_CHECKSUM;
	ret = tc305k_mode_enable(client, cmd);
	if (ret) {
		input_err(true, &client->dev, "[TK] %s command fail (%d)\n", __func__, ret);
		return ret;
	}

	msleep(tc305k_CHECKSUM_DELAY);

	ret = i2c_smbus_read_byte_data(client, tc305k_CHECKS_H);
	if (ret < 0) {
		input_err(true, &client->dev, "[TK] %s: failed to read checksum_h (%d)\n",
			__func__, ret);
		return ret;
	}
	checksum_h = ret;

	ret = i2c_smbus_read_byte_data(client, tc305k_CHECKS_L);
	if (ret < 0) {
		input_err(true, &client->dev, "[TK] %s: failed to read checksum_l (%d)\n",
			__func__, ret);
		return ret;
	}
	checksum_l = ret;

	checksum = (checksum_h << 8) | checksum_l;

	if (data->fw_img->checksum != checksum) {
		input_err(true, &client->dev,
			"%s checksum fail - firm checksum(%d), compute checksum(%d)\n",
			__func__, data->fw_img->checksum, checksum);
		return -1;
	}

	input_info(true, &client->dev, "[TK] %s success (%d)\n", __func__, checksum);

	return 0;
}

static int tc305k_fw_update(struct tc305k_data *data, u8 fw_path, bool force)
{
	struct i2c_client *client = data->client;
	int retry = 4;
	int ret;

	if (fw_path == FW_INKERNEL) {
		if (!force) {
			ret = tc305k_get_fw_version(data, false);
			if (ret)
				return -1;
		}

		ret = load_fw_in_kernel(data);
		if (ret)
			return -1;

		if (data->md_ver != data->md_ver_bin) {
			input_err(true, &client->dev,
					"[TK] model id is different(IC:0x%x, BIN:0x%x)."
					" do force firm up\n",
					data->md_ver, data->md_ver_bin);
			force = true;
		}

		//if ((!force && (data->fw_ver >= data->fw_ver_bin)) || data->dtdata->bringup) {
		if ((!force && (data->fw_ver >= data->fw_ver_bin))) {	//modify by wxj in 2016/3/18
			input_info(true, &client->dev, "[TK] do not need firm update (IC:0x%x, BIN:0x%x)\n",
				data->fw_ver, data->fw_ver_bin);
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
		ret = tc305k_flash_fw(data, fw_path);
		data->fw_downloding = false;
		if (ret) {
			input_err(true, &client->dev, "[TK] %s tc305k_flash_fw fail (%d)\n",
				__func__, retry);
			continue;
		}

		ret = tc305k_get_fw_version(data, false);
		if (ret) {
			input_err(true, &client->dev, "[TK] %s tc305k_get_fw_version fail (%d)\n",
				__func__, retry);
			continue;
		}
		if (data->fw_ver != data->fw_img->first_fw_ver) {
			input_err(true, &client->dev, "[TK] %s fw version fail (0x%x, 0x%x)(%d)\n",
				__func__, data->fw_ver, data->fw_img->first_fw_ver, retry);
			continue;
		}

		ret = tc305k_crc_check(data);
		if (ret) {
			input_err(true, &client->dev, "[TK] %s crc check fail (%d)\n",
				__func__, retry);
			continue;
		}
		break;
	}

	if (retry > 0)
		input_info(true, &client->dev, "%s success\n", __func__);

	t300k_release_fw(data, fw_path);

	return ret;
}

/*
 * Fw update by parameters:
 * s | S = TSK FW from kernel binary and compare fw version.
 * i | I = TSK FW from SD Card and Not compare fw version.
 * f | F = TSK FW from kernel binary and Not compare fw version.
 */
static ssize_t tc305k_update_store(struct device *dev,
	 struct device_attribute *attr, const char *buf, size_t count)
{
	struct tc305k_data *data = dev_get_drvdata(dev);
	struct i2c_client *client = data->client;
	int ret;
	u8 fw_path;
	bool fw_update_force = false;

	switch(*buf) {
	case 's':
	case 'S':
		fw_path = FW_INKERNEL;
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
		input_err(true, &client->dev, "[TK] %s wrong command fail\n", __func__);
		data->fw_update_status = TK_UPDATE_FAIL;
		return count;
	}

	data->fw_update_status = TK_UPDATE_DOWN;

	disable_irq(client->irq);
	ret = tc305k_fw_update(data, fw_path, fw_update_force);
	enable_irq(client->irq);
	if (ret < 0) {
		input_err(true, &client->dev, "[TK] %s fail\n", __func__);
		data->fw_update_status = TK_UPDATE_FAIL;
	} else
		data->fw_update_status = TK_UPDATE_PASS;

	return count;
}

static int read_tc350k_register_data(struct tc305k_data *data, int read_key_num, int read_offset)
{
	struct i2c_client *client = data->client;
	int ret;
	u8 buff[2];
	int value;

	ret = i2c_smbus_read_i2c_block_data(client, read_key_num + read_offset, TC350K_DATA_SIZE, buff);
	if (ret != TC350K_DATA_SIZE) {
		dev_err(&client->dev, "[TK] %s read fail(%d)\n", __func__, ret);
		value = 0;
		goto exit;
	}
	value = (buff[TC350K_DATA_H_OFFSET] << 8) | buff[TC350K_DATA_L_OFFSET];

	dev_info(&client->dev, "[TK] %s : read key num/offset = [0x%X/0x%X], value : [%d]\n",
								__func__, read_key_num, read_offset, value);

exit:
	return value;
}

static ssize_t tc305k_firm_version_read_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct tc305k_data *data = dev_get_drvdata(dev);
	struct i2c_client *client = data->client;
	int ret;

	ret = tc305k_get_fw_version(data, false);
	if (ret < 0)
		input_err(true, &client->dev, "[TK] %s: failed to read firmware version (%d)\n",
			__func__, ret);

	return sprintf(buf, "0x%02x\n", data->fw_ver);
}
static ssize_t touchkey_keycode(struct device *dev,
		struct device_attribute *attr, char *buf)
{
        struct tc305k_data *data = dev_get_drvdata(dev);
	struct i2c_client *client = data->client;
	int ret;

        ret = i2c_smbus_read_byte_data(client, tc305k_KEYCODE);
	if (ret < 0) 
	        input_err(true, &client->dev, "[TK] %s: failed to read keycode (%d)\n",
			__func__, ret);
        return sprintf(buf, "%d\n", (u8)ret);
}
static ssize_t touchkey_sar_enable(struct device *dev,
		 struct device_attribute *attr, const char *buf,
		 size_t count)
{
	struct tc305k_data *data = dev_get_drvdata(dev);
	struct i2c_client *client = data->client;
	int buff;
	int ret;
	bool on;
	int cmd;

	ret = sscanf(buf, "%d", &buff);
	if (ret != 1) {
		input_err(true, &client->dev, "%s: cmd read err\n", __func__);
		return count;
	}

	if (!(buff >= 0 && buff <= 3)) {
		input_err(true, &client->dev, "%s: wrong command(%d)\n",
				__func__, buff);
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
		input_info(true, &data->client->dev,
				"%s : Power back off _ force off -> on (%d)\n",
				__func__, data->sar_enable);
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
		input_info(true, &data->client->dev,
				"%s skip, Power back off _ force off mode (%d)\n",
				__func__, data->sar_enable);
		return count;
	}

	if (buff == 1) {
		on = true;
		cmd = tc305k_CMD_SAR_ENABLE;
	} else if (buff == 2) {
		on = false;
		data->sar_enable_off = 1;
		cmd = tc305k_CMD_SAR_DISABLE;
	} else {
		on = false;
		cmd = tc305k_CMD_SAR_DISABLE;
	}

        if ( cmd == tc305k_CMD_SAR_ENABLE) {
                ret = tc305k_mode_enable(data->client, tc305k_CMD_WAKE_UP);
                if (ret < 0) {
		        input_err(true, &data->client->dev, "%s wake_up fail(%d)\n", __func__, ret);
	        }

                mdelay(10);

                ret = tc305k_mode_enable(data->client, cmd);
	        if (ret < 0) {
		        input_err(true, &data->client->dev, "%s fail(%d)\n", __func__, ret);
		        return count;
	        }                
        } else {
	        ret = tc305k_mode_enable(data->client, cmd);
	        if (ret < 0) {
		        input_err(true, &data->client->dev, "%s fail(%d)\n", __func__, ret);
		        return count;
	        }
        }
        
	if (buff == 1) {
		data->sar_enable = true;
	} else {
		input_report_key(data->input_dev, KEY_CP_GRIP, TSK_RELEASE);
		data->grip_event = 0;
		data->sar_enable = false;
	}

	input_info(true, &data->client->dev, "%s data:%d on:%d\n",__func__, buff, on);
	return count;
}

static ssize_t touchkey_grip_threshold_show(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	struct tc305k_data *data = dev_get_drvdata(dev);
	int ret;

	mutex_lock(&data->lock_fac);
	ret = read_tc350k_register_data(data, TC305K_GRIP_THD_PRESS, 0);
	mutex_unlock(&data->lock_fac);
	if (ret < 0) {
		input_err(true, &data->client->dev, "%s fail to read press thd(%d)\n", __func__, ret);
		data->grip_p_thd = 0;
		return sprintf(buf, "%d\n", 0);
	}
	data->grip_p_thd = ret;

	mutex_lock(&data->lock_fac);
	ret = read_tc350k_register_data(data, TC305K_GRIP_THD_RELEASE, 0);
	mutex_unlock(&data->lock_fac);
	if (ret < 0) {
		input_err(true, &data->client->dev, "%s fail to read release thd(%d)\n", __func__, ret);
		data->grip_r_thd = 0;
		return sprintf(buf, "%d\n", 0);
	}

	data->grip_r_thd = ret;

	mutex_lock(&data->lock_fac);
	ret = read_tc350k_register_data(data, TC305K_GRIP_THD_NOISE, 0);
	mutex_unlock(&data->lock_fac);
	if (ret < 0) {
		input_err(true, &data->client->dev, "%s fail to read noise thd(%d)\n", __func__, ret);
		data->grip_n_thd = 0;
		return sprintf(buf, "%d\n", 0);
	}
	data->grip_n_thd = ret;

	return sprintf(buf, "%d,%d,%d\n",
			data->grip_p_thd, data->grip_r_thd, data->grip_n_thd );
}

static ssize_t touchkey_total_cap_show(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	struct tc305k_data *data = dev_get_drvdata(dev);
	int ret;

	ret = i2c_smbus_read_byte_data(data->client, TC305K_GRIP_TOTAL_CAP);
	if (ret < 0) {
		input_err(true, &data->client->dev, "%s fail(%d)\n", __func__, ret);
		return sprintf(buf, "%d\n", 0);
	}

	return sprintf(buf, "%d\n", ret / 100);
}

static ssize_t touchkey_grip_show(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	struct tc305k_data *data = dev_get_drvdata(dev);
	int ret;

	mutex_lock(&data->lock_fac);
	ret = read_tc350k_register_data(data, TC305K_GRIP_DIFF_DATA, 0);
	mutex_unlock(&data->lock_fac);
	if (ret < 0) {
		input_err(true, &data->client->dev, "%s fail(%d)\n", __func__, ret);
		data->grip_s1 = 0;
		return sprintf(buf, "%d\n", 0);
	}
	data->grip_s1 = ret;

	return sprintf(buf, "%d\n", data->grip_s1);
}

static ssize_t touchkey_grip_baseline_show(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	struct tc305k_data *data = dev_get_drvdata(dev);
	int ret;

	mutex_lock(&data->lock_fac);
	ret = read_tc350k_register_data(data, TC305K_GRIP_BASELINE, 0);
	mutex_unlock(&data->lock_fac);
	if (ret < 0) {
		input_err(true, &data->client->dev, "%s fail(%d)\n", __func__, ret);
		data->grip_baseline = 0;
		return sprintf(buf, "%d\n", 0);
	}
	data->grip_baseline = ret;

	return sprintf(buf, "%d\n", data->grip_baseline);

}

static ssize_t touchkey_grip_raw_show(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	struct tc305k_data *data = dev_get_drvdata(dev);
	int ret;

	mutex_lock(&data->lock_fac);
	ret = read_tc350k_register_data(data, TC305K_GRIP_RAW_DATA, 0);
	mutex_unlock(&data->lock_fac);
	if (ret < 0) {
		input_err(true, &data->client->dev, "%s fail(%d)\n", __func__, ret);
		data->grip_raw1 = 0;
		data->grip_raw2 = 0;
		return sprintf(buf, "%d\n", 0);
	}
	data->grip_raw1 = ret;
	data->grip_raw2 = 0;

	return sprintf(buf, "%d,%d\n", data->grip_raw1, data->grip_raw2);
}

static ssize_t touchkey_grip_gain_show(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	return sprintf(buf, "%d,%d,%d,%d\n", 0, 0, 0, 0);
}

static ssize_t touchkey_grip_check_show(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	struct tc305k_data *data = dev_get_drvdata(dev);

	input_info(true, &data->client->dev, "%s event:%d\n", __func__, data->grip_event);

	return sprintf(buf, "%d\n", data->grip_event);
}

static ssize_t touchkey_grip_sw_reset(struct device *dev,
		 struct device_attribute *attr, const char *buf,
		 size_t count)
{
	struct tc305k_data *data = dev_get_drvdata(dev);
	struct i2c_client *client = data->client;
	int buff;
	int ret;

	ret = sscanf(buf, "%d", &buff);
	if (ret != 1) {
		input_err(true, &client->dev, "%s: cmd read err\n", __func__);
		return count;
	}

	if (!(buff == 1)) {
		input_err(true, &client->dev, "%s: wrong command(%d)\n",
			__func__, buff);
		return count;
	}

	data->grip_event = 0;
        
        ret = tc305k_mode_enable(data->client, tc305k_CMD_SW_RESET);
        if (ret < 0) {
                input_err(true, &data->client->dev, "%s sw_reset fail(%d)\n", __func__, ret);
                return ret;
        }

        input_info(true, &data->client->dev, "%s data(%d) sw_reset success\n", __func__, buff);

        return count;
}

static ssize_t touchkey_sensing_change(struct device *dev,
		 struct device_attribute *attr, const char *buf,
		 size_t count)
{
	struct tc305k_data *data = dev_get_drvdata(dev);
	struct i2c_client *client = data->client;
	int ret, buff;

	ret = sscanf(buf, "%d", &buff);
	if (ret != 1) {
		input_err(true, &client->dev, "%s: cmd read err\n", __func__);
		return count;
	}

	if (!(buff == 0 || buff == 1)) {
		input_err(true, &client->dev, "%s: wrong command(%d)\n",
				__func__, buff);
		return count;
	}

	touchkey_sar_sensing(data, buff);

	input_info(true, &data->client->dev, "%s earjack (%d)\n", __func__, buff);

	return count;
}

static ssize_t touchkey_chip_name(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	struct tc305k_data *data = dev_get_drvdata(dev);
	struct i2c_client *client = data->client;

	input_info(true, &client->dev, "%s\n", __func__);

	return sprintf(buf, "TC305K\n");
}

static ssize_t touchkey_mode_change(struct device *dev,
		 struct device_attribute *attr, const char *buf,
		 size_t count)
{
	struct tc305k_data *data = dev_get_drvdata(dev);
	struct i2c_client *client = data->client;
	int ret, buff;

	ret = sscanf(buf, "%d", &buff);
	if (ret != 1) {
		input_err(true, &client->dev, "%s: cmd read err\n", __func__);
		return count;
	}

	if (!(buff == 0 || buff == 1)) {
		input_err(true, &client->dev, "%s: wrong command(%d)\n", __func__, buff);
		return count;
	}

	input_info(true, &data->client->dev, "%s data(%d)\n", __func__, buff);

	tc305k_stop_mode(data, buff);

	return count;
}

static DEVICE_ATTR(touchkey_firm_update, S_IRUGO | S_IWUSR | S_IWGRP, NULL, tc305k_update_store);
static DEVICE_ATTR(touchkey_fw_version, S_IRUGO, tc305k_firm_version_read_show, NULL);
static DEVICE_ATTR(touchkey_keycode, S_IRUGO, touchkey_keycode, NULL);
static DEVICE_ATTR(touchkey_grip_threshold, S_IRUGO, touchkey_grip_threshold_show, NULL);
static DEVICE_ATTR(touchkey_total_cap, S_IRUGO, touchkey_total_cap_show, NULL);
static DEVICE_ATTR(sar_enable, S_IRUGO | S_IWUSR | S_IWGRP, NULL, touchkey_sar_enable);
static DEVICE_ATTR(sw_reset, S_IRUGO | S_IWUSR | S_IWGRP, NULL, touchkey_grip_sw_reset);
static DEVICE_ATTR(touchkey_earjack, S_IRUGO | S_IWUSR | S_IWGRP, NULL, touchkey_sensing_change);
static DEVICE_ATTR(touchkey_grip, S_IRUGO, touchkey_grip_show, NULL);
static DEVICE_ATTR(touchkey_grip_baseline, S_IRUGO, touchkey_grip_baseline_show, NULL);
static DEVICE_ATTR(touchkey_grip_raw, S_IRUGO, touchkey_grip_raw_show, NULL);
static DEVICE_ATTR(touchkey_grip_gain, S_IRUGO, touchkey_grip_gain_show, NULL);
static DEVICE_ATTR(touchkey_grip_check, S_IRUGO, touchkey_grip_check_show, NULL);
static DEVICE_ATTR(touchkey_sar_only_mode, S_IRUGO | S_IWUSR | S_IWGRP,
			NULL, touchkey_mode_change);
#if 0
static DEVICE_ATTR(touchkey_sar_press_threshold,  S_IRUGO | S_IWUSR | S_IWGRP | S_IWOTH,
			NULL, touchkey_sar_press_threshold_store);
static DEVICE_ATTR(touchkey_sar_release_threshold,  S_IRUGO | S_IWUSR | S_IWGRP | S_IWOTH,
			NULL, touchkey_sar_release_threshold_store);
#endif

static DEVICE_ATTR(touchkey_chip_name, S_IRUGO, touchkey_chip_name, NULL);


static struct attribute *grip_attributes[] = {
        &dev_attr_touchkey_firm_update.attr,
        &dev_attr_touchkey_fw_version.attr,
        &dev_attr_touchkey_keycode.attr,
	&dev_attr_touchkey_grip_threshold.attr,
	&dev_attr_touchkey_total_cap.attr,
	&dev_attr_sar_enable.attr,
	&dev_attr_sw_reset.attr,
	&dev_attr_touchkey_earjack.attr,
	&dev_attr_touchkey_grip.attr,
	&dev_attr_touchkey_grip_baseline.attr,
	&dev_attr_touchkey_grip_raw.attr,
	&dev_attr_touchkey_grip_gain.attr,
	&dev_attr_touchkey_grip_check.attr,
	&dev_attr_touchkey_sar_only_mode.attr,
#if 0
	&dev_attr_touchkey_sar_press_threshold.attr,
	&dev_attr_touchkey_sar_release_threshold.attr,
#endif
	&dev_attr_touchkey_chip_name.attr,
	NULL,
};

static struct attribute_group grip_attr_group = {
	.attrs = grip_attributes,
};

static int tc305k_connecter_check(struct tc305k_data *data)
{
	struct i2c_client *client = data->client;

	if (!gpio_is_valid(data->dtdata->gpio_sub_det)) {
		input_err(true, &client->dev, "%s: Not use sub_det pin\n", __func__);
		return SUB_DET_DISABLE;

	} else {
		if (gpio_get_value(data->dtdata->gpio_sub_det)) {
			return SUB_DET_ENABLE_CON_OFF;
		} else {
			return SUB_DET_ENABLE_CON_ON;
		}

	}

}
static int tc305k_fw_check(struct tc305k_data *data)
{
	struct i2c_client *client = data->client;
	int ret;
	int tsk_connecter_status;
	bool force_update = false;

	tsk_connecter_status = tc305k_connecter_check(data);

	if (tsk_connecter_status == SUB_DET_ENABLE_CON_OFF) {
		input_err(true, &client->dev, "%s : TSK IC is disconnected! skip probe(%d)\n",
						__func__, gpio_get_value(data->dtdata->gpio_sub_det));
		return -1;
	}

	ret = tc305k_get_fw_version(data, true);
	if (ret < 0) {
		input_err(true, &client->dev,
			"[TK] %s: i2c fail...[%d], addr[%d]\n",
			__func__, ret, data->client->addr);
		data->fw_ver = 0xFF;
	}

	if (data->fw_ver == 0xFF) {
		input_info(true, &client->dev,
			"[TK] fw version 0xFF, Excute firmware update!\n");
		force_update = true;
	} else {
		force_update = false;
	}

	ret = tc305k_fw_update(data, FW_INKERNEL, force_update);
	if (ret)
		input_err(true, &client->dev, "%s: fail to fw update\n", __func__);

	return ret;
}

static int tc305k_pinctrl_configure(struct tc305k_data *data, bool active)
{
	struct pinctrl_state *set_state_i2c;
	int retval;

	input_info(true, &data->client->dev, "%s %s\n", __func__, active? "ACTIVE" : "SUSPEND");

	if (active) {
		set_state_i2c =
			pinctrl_lookup_state(data->pinctrl,
						"grip_active");
		if (IS_ERR(set_state_i2c)) {
			input_err(true,  &data->client->dev, "%s: cannot get pinctrl(i2c) active state\n", __func__);
			return PTR_ERR(set_state_i2c);
		}
	} else {
		set_state_i2c =
			pinctrl_lookup_state(data->pinctrl,
						"grip_suspend");
		if (IS_ERR(set_state_i2c)) {
			input_err(true, &data->client->dev, "%s: cannot get pinctrl(i2c) sleep state\n", __func__);
			return PTR_ERR(set_state_i2c);
		}
	}

	retval = pinctrl_select_state(data->pinctrl, set_state_i2c);
	if (retval) {
		input_err(true, &data->client->dev, "%s: cannot set pinctrl(i2c) %s state\n",
				__func__, active ? "active" : "suspend");
		return retval;
	}

	if (active) {
		gpio_direction_input(data->dtdata->gpio_sda);
		gpio_direction_input(data->dtdata->gpio_scl);
		gpio_direction_input(data->dtdata->gpio_int);
	}

	return 0;
}

#ifdef CONFIG_MUIC_NOTIFIER
static int ta_notification(struct notifier_block *nb,
		unsigned long action, void *data)
{
        muic_attached_dev_t attached_dev = *(muic_attached_dev_t *)data;
        struct tc305k_data *tkdata = container_of(nb, struct tc305k_data, tc305k_nb);
        int ta_state = 0;
        int ret = 0;

	switch (action) {
	case MUIC_NOTIFY_CMD_DETACH:
	case MUIC_NOTIFY_CMD_LOGICALLY_DETACH:
                ta_state = false;
                break;
        case MUIC_NOTIFY_CMD_ATTACH:
	case MUIC_NOTIFY_CMD_LOGICALLY_ATTACH:
                ta_state = true;
                break;
        }

        input_info(true, &tkdata->client->dev, "%s ta_state %d \n", __func__, ta_state);

        if (ta_state == true) {
                ret = tc305k_mode_enable(tkdata->client, tc305k_CMD_TA_ON);
                if (ret < 0) {
		input_err(true, &tkdata->client->dev, "[TK] %s ta_on fail(%d)\n", __func__, ret);
                }
        } else if (ta_state == false) {
                ret = tc305k_mode_enable(tkdata->client, tc305k_CMD_TA_OFF);
                if (ret < 0) {
		input_err(true, &tkdata->client->dev, "[TK] %s ta_off fail(%d)\n", __func__, ret);
                }
        }
        
        return 0;           
}
#endif

static int tc305k_probe(struct i2c_client *client,
		const struct i2c_device_id *id)
{
	struct i2c_adapter *adapter = to_i2c_adapter(client->dev.parent);
	struct tc305k_devicetree_data *dtdata;
	struct input_dev *input_dev;
	struct tc305k_data *data;
	int ret = 0;
	int i = 0;
	int err = 0;
        
	input_info(true, &client->dev, "[TK] %s\n",__func__);

	if (!i2c_check_functionality(adapter, I2C_FUNC_I2C)) {
		input_err(true, &client->dev, "[TK] i2c_check_functionality fail\n");
		return -EIO;
	}

	if (client->dev.of_node) {
		dtdata = devm_kzalloc(&client->dev, sizeof(struct tc305k_devicetree_data), GFP_KERNEL);
		if (!dtdata) {
			input_info(true, &client->dev, "[TK] Failed to allocate memory\n");
			goto err_alloc_data;
		}

		err = tc305k_parse_dt(&client->dev, dtdata);
		if (err)
			goto err_alloc_data;
	} else {
		dtdata = client->dev.platform_data;
	}

	data = kzalloc(sizeof(struct tc305k_data), GFP_KERNEL);
	if (!data) {
		input_err(true, &client->dev, "[TK] Failed to allocate memory\n");
		ret = -ENOMEM;
		goto err_alloc_data;
	}

	data->dtdata = dtdata;
        
	input_dev = input_allocate_device();
	if (!input_dev) {
		input_err(true, &client->dev,
			"[TK] Failed to allocate memory for input device\n");
		ret = -ENOMEM;
		goto err_alloc_input;
	}

	data->client = client;
	data->input_dev = input_dev;

	if (data->dtdata == NULL) {
		input_err(true, &client->dev, "[TK] failed to get platform data\n");
		ret = -EINVAL;
		goto err_platform_data;
	}
	data->irq = -1;
	mutex_init(&data->lock);
	mutex_init(&data->lock_fac);

	init_completion(&data->resume_done);
	wake_lock_init(&data->touchkey_wake_lock, WAKE_LOCK_SUSPEND, "touchkey wake lock");

	data->power = tc305k_touchkey_power;

	i2c_set_clientdata(client, data);
        data->client = client;
        data->factory_device = &client->dev;
	tc305k_gpio_request(data);
        //gpio_direction_input(dtdata->gpio_int);
	data->pinctrl = devm_pinctrl_get(&client->dev);
	if (IS_ERR(data->pinctrl)) {
		if (PTR_ERR(data->pinctrl) == -EPROBE_DEFER)
			input_err(true, &client->dev, "%s: pinctrl is EPROBE_DEFER\n", __func__);

		input_err(true, &client->dev, "%s: Target does not use pinctrl\n", __func__);
		data->pinctrl = NULL;
	}

	if (data->pinctrl) {
		ret = tc305k_pinctrl_configure(data, true);
		if (ret < 0) {
			input_err(true, &client->dev,
				"%s: Failed to init pinctrl: %d\n", __func__, ret);
			goto err_pinctrl_config;
		}
	}

	if(dtdata->boot_on_ldo){
		data->power(data, true);
	} else {
		data->power(data, true);
		msleep(tc305k_POWERON_DELAY);
	}
        
	data->enabled = true;
	client->irq = gpio_to_irq(dtdata->gpio_int);

	ret = tc305k_fw_check(data);
	if (ret) {
		input_err(true, &client->dev,
			"[TK] failed to firmware check(%d)\n", ret);
		goto err_fw_check;
	}

	snprintf(data->phys, sizeof(data->phys),
		"%s/input0", dev_name(&client->dev));
	input_dev->name = MODULE_NAME;
	input_dev->phys = data->phys;
	input_dev->id.bustype = BUS_I2C;
	input_dev->dev.parent = &client->dev;
	input_dev->open = tc305k_input_open;
	input_dev->close = tc305k_input_close;

	data->tsk_ev_val = tsk_ev;
	data->key_num = ARRAY_SIZE(tsk_ev)/2;
	input_info(true, &client->dev, "[TK] number of keys = %d\n", data->key_num);

	set_bit(EV_KEY, input_dev->evbit);
	for (i = 0; i < data->key_num; i++) {
		set_bit(data->tsk_ev_val[i].tsk_keycode, input_dev->keybit);
#if !defined(CONFIG_SAMSUNG_PRODUCT_SHIP)
		input_info(true, &client->dev, "[TK] keycode[%d]= %3d\n",
						i, data->tsk_ev_val[i].tsk_keycode);
#endif
	}
	input_set_drvdata(input_dev, data);

	ret = input_register_device(input_dev);
	if (ret) {
		input_err(true, &client->dev, "[TK] fail to register input_dev (%d)\n",
			ret);
		goto err_register_input_dev;
	}

	ret = request_threaded_irq(client->irq, NULL, tc305k_interrupt,
				IRQF_DISABLED | IRQF_TRIGGER_FALLING |
				IRQF_ONESHOT, TC305K_NAME, data);
	if (ret < 0) {
		input_err(true, &client->dev, "[TK] fail to request irq (%d).\n",
			dtdata->gpio_int);
		goto err_request_irq;
	}
	data->irq = dtdata->gpio_int;

        ret = sensors_create_symlink(&input_dev->dev.kobj, input_dev->name);
	if (ret < 0) {
		input_err(true, &client->dev, "%s [ERROR] sensors_create_symlink\n", __func__);
		goto err_create_attr_group;
	}

	ret = sysfs_create_group(&input_dev->dev.kobj, &grip_attr_group);
	if (ret < 0) {
                input_err(true, &client->dev, "%s [ERROR] sysfs_create_group\n", __func__);
		sensors_remove_symlink(&input_dev->dev.kobj, input_dev->name);
		goto err_create_attr_group;
	}

        ret = sensors_register(data->factory_device,
		data, grip_attributes, MODULE_NAME);
	if (ret) {
		input_err(true, &client->dev, "%s [ERROR] sensors_register\n", __func__);
                sensors_remove_symlink(&input_dev->dev.kobj, input_dev->name);
		ret = -EAGAIN;
		goto err_create_attr_group;
	}

        ret = tc305k_mode_enable(data->client, tc305k_CMD_SAR_DISABLE);
	if (ret < 0) {
		input_err(true, &data->client->dev, "[TK] %s sar_disable fail(%d)\n", __func__, ret);
        }

#ifdef CONFIG_MUIC_NOTIFIER
        muic_notifier_register(&data->tc305k_nb, ta_notification, MUIC_NOTIFY_DEV_TSP);
#endif

        return 0;

err_create_attr_group:
err_request_irq:
	input_unregister_device(input_dev);
err_register_input_dev:
err_fw_check:
err_pinctrl_config:
	wake_lock_destroy(&data->touchkey_wake_lock);

	mutex_destroy(&data->lock);
	mutex_destroy(&data->lock_fac);
	data->power(data, false);
err_platform_data:
	input_free_device(input_dev);
err_alloc_input:
	kfree(data);
err_alloc_data:
	return ret;
}

static int tc305k_remove(struct i2c_client *client)
{
	struct tc305k_data *data = i2c_get_clientdata(client);

	wake_lock_destroy(&data->touchkey_wake_lock);
	free_irq(client->irq, data);
	input_unregister_device(data->input_dev);
	input_free_device(data->input_dev);
	mutex_destroy(&data->lock);
	mutex_destroy(&data->lock_fac);
	data->power(data, false);
	gpio_free(data->dtdata->gpio_int);
	gpio_free(data->dtdata->gpio_sda);
	gpio_free(data->dtdata->gpio_scl);
	kfree(data);

	return 0;
}

static void tc305k_shutdown(struct i2c_client *client)
{
	struct tc305k_data *data = i2c_get_clientdata(client);

	wake_lock_destroy(&data->touchkey_wake_lock);
	data->power(data, false);
}

static int tc305k_stop(struct device *dev)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct tc305k_data *data = i2c_get_clientdata(client);
        struct irq_desc *desc= irq_to_desc(client->irq);

	mutex_lock(&data->lock);

	if (!data->enabled) {
		mutex_unlock(&data->lock);
		return 0;
	}

	input_info(true, &data->client->dev, "[TK] %s: users=%d\n",
		__func__, data->input_dev->users);

	disable_irq(client->irq);
        mt_eint_mask(desc->irq_data.hwirq);
	data->enabled = false;
	tc305k_release_all_fingers(data);
	data->power(data, false);
	data->led_on = false;

	mutex_unlock(&data->lock);

	return 0;
}

static int tc305k_start(struct device *dev)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct tc305k_data *data = i2c_get_clientdata(client);
        struct irq_desc *desc= irq_to_desc(client->irq);
	int ret;
	u8 cmd;

	mutex_lock(&data->lock);
	if (data->enabled) {
		mutex_unlock(&data->lock);
		return 0;
	}
	input_info(true, &data->client->dev, "[TK] %s: users=%d\n", __func__, data->input_dev->users);

	data->power(data, true);
	msleep(tc305k_POWERON_DELAY);
	enable_irq(client->irq);
        mt_eint_unmask(desc->irq_data.hwirq);

	data->enabled = true;
	if (data->led_on == true) {
		data->led_on = false;
		input_info(true, &client->dev, "[TK] led on(resume)\n");
		cmd = tc305k_CMD_LED_ON;
		ret = tc305k_mode_enable(client, cmd);
		if (ret < 0)
			input_err(true, &client->dev, "%s led on fail(%d)\n", __func__, ret);
	}

	if (data->glove_mode) {
		ret = tc305k_mode_enable(client, tc305k_CMD_GLOVE_ON);
		if (ret < 0)
			input_err(true, &client->dev, "[TK] %s glovemode fail(%d)\n", __func__, ret);
	}

	mutex_unlock(&data->lock);

	return 0;
}

static void tc305k_input_close(struct input_dev *dev)
{
	struct tc305k_data *data = input_get_drvdata(dev);

	input_info(true, &data->client->dev,
			"%s: sar_enable(%d)\n", __func__, data->sar_enable);
        /* this device is only grip sensors (project : grandprime plus)*/
	//tc305k_stop_mode(data, 1);

	if (device_may_wakeup(&data->client->dev))
		enable_irq_wake(data->irq);
}

static int tc305k_input_open(struct input_dev *dev)
{
	struct tc305k_data *data = input_get_drvdata(dev);

	input_info(true, &data->client->dev,
			"%s: sar_enable(%d)\n", __func__, data->sar_enable);
        /* this device is only grip sensors (project : grandprime plus)*/
        //tc305k_stop_mode(data, 0);

	if (device_may_wakeup(&data->client->dev))
		disable_irq_wake(data->irq);

	return 0;
}

#ifdef CONFIG_PM

static int tc305k_suspend(struct device *dev)
{
	struct tc305k_data *data = dev_get_drvdata(dev);

	data->is_lpm_suspend = true;

        reinit_completion(&data->resume_done);
	return 0;
}

static int tc305k_resume(struct device *dev)
{
	struct tc305k_data *data = dev_get_drvdata(dev);

	data->is_lpm_suspend = false;
	complete_all(&data->resume_done);
        
	return 0;
}

static const struct dev_pm_ops tc305k_pm_ops = {
	.suspend = tc305k_suspend,
	.resume = tc305k_resume,
};
#endif

static const struct i2c_device_id tc305k_id[] = {
	{TC305K_NAME, 0},
	{ }
};

#ifdef CONFIG_OF
static struct of_device_id coreriver_match_table[] = {
	{ .compatible = "coreriver,tc305k-grip",},
	{ },
};
#else
#define coreriver_match_table	NULL
#endif
MODULE_DEVICE_TABLE(i2c, tc305k_id);

static struct i2c_driver tc305k_driver = {
	.probe = tc305k_probe,
	.remove = tc305k_remove,
	.shutdown = tc305k_shutdown,
	.driver = {
		.name = TC305K_NAME,
		.owner = THIS_MODULE,
#ifdef CONFIG_OF
		.of_match_table = of_match_ptr(coreriver_match_table),
#endif
#if defined(CONFIG_PM)
		.pm	= &tc305k_pm_ops,
#endif
	},
	.id_table = tc305k_id,
};

static int __init tc305k_init(void)
{

#ifdef CONFIG_SAMSUNG_LPM_MODE
	if (poweroff_charging) {
		pr_notice("%s %s : LPM Charging Mode!!\n", SECLOG, __func__);
		return 0;
	}
#endif

	 return i2c_add_driver(&tc305k_driver);
}

static void __exit tc305k_exit(void)
{
	i2c_del_driver(&tc305k_driver);
}

module_init(tc305k_init);
module_exit(tc305k_exit);

MODULE_AUTHOR("Samsung Electronics");
MODULE_DESCRIPTION("Touchkey driver for Coreriver tc305k");
MODULE_LICENSE("GPL");
