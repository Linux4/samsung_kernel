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
#include <linux/input/mt.h>
#include <linux/interrupt.h>
#include <linux/irq.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/mutex.h>
#include <linux/slab.h>
#include <asm/unaligned.h>
#include <linux/wakelock.h>
#include <linux/workqueue.h>
#include <linux/uaccess.h>
#include <linux/io.h>
#include <linux/regulator/consumer.h>
#include <linux/sensor/sensors_core.h>
#include <linux/pinctrl/consumer.h>


#include "tc3xxk.h"


#define IDLE                     0
#define ACTIVE                   1

/* registers */
#define TC300K_FWVER		0x01
#define TC300K_MDVER		0x02
#define TC300K_MODE			0x03
#define TC300K_CHECKS_H		0x04
#define TC300K_CHECKS_L		0x05

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

#define TC350K_DATA_SIZE		0x02
#define TC350K_DATA_H_OFFSET	0x00
#define TC350K_DATA_L_OFFSET	0x01

/* command */
#define TC300K_CMD_ADDR			0x00
#define TC300K_CMD_TA_ON		0x50
#define TC300K_CMD_TA_OFF		0x60
#define TC300K_CMD_CAL_CHECKSUM	0x70
#define TC300K_CMD_STOP_MODE		0x90
#define TC300K_CMD_NORMAL_MODE		0x91
#define TC300K_CMD_SAR_DISABLE		0xA0
#define TC300K_CMD_SAR_ENABLE		0xA1
#define TC300K_CMD_IC_STOP_ON		0xA2
#define TC300K_CMD_IC_STOP_OFF		0xA3
#define TC300K_CMD_GRIP_BASELINE_CAL	0xC0
#define TC300K_CMD_WAKE_UP		0xF0
#define TC300K_CMD_DELAY		50

/* mode status bit */
#define TC300K_MODE_RUN			(1 << 1)
#define TC300K_MODE_SAR			(1 << 2)

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

#define GRIP_RELEASE		0x00
#define GRIP_PRESS			0x01

struct grip_event_val {
	u16 grip_bitmap;
	u8 grip_status;
	char* grip_name;
};


struct grip_event_val grip_ev[4] =
{
	{0x01 << 0, GRIP_PRESS, "grip1"},
	{0x01 << 1, GRIP_PRESS, "grip2"},
	{0x01 << 4, GRIP_RELEASE, "grip1"},
	{0x01 << 5, GRIP_RELEASE, "grip2"},
};

struct tc3xxk_data {
	struct device *dev;
	struct i2c_client *client;
	struct tc3xxk_platform_data *pdata;
	struct fw_image *fw_img;
	const struct firmware *fw;
	char phys[32];
	int irq;
	bool irq_check;
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
	bool fw_downloading;

	struct pinctrl *pinctrl_i2c;
	struct pinctrl *pinctrl_irq;
	struct pinctrl_state *pin_state[4];

	u16 grip_p_thd;
	u16 grip_r_thd;
	u16 grip_n_thd;
	u16 grip_s1;
	u16 grip_s2;
	u16 grip_baseline;
	u16 grip_raw1;
	u16 grip_raw2;
	u16 grip_event;
	bool sar_enable;
	bool sar_enable_off;
	int grip_num;
	struct grip_event_val *grip_ev_val;
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
	usleep_range(10000, 10000);

	return ret;
}



static void tc3xxk_reset(struct tc3xxk_data *data)
{
	SENSOR_INFO("\n");

	data->pdata->power(data, false);

	msleep(50);

	data->pdata->power(data, true);
	msleep(200);

	if (data->sar_enable)
		tc3xxk_mode_enable(data->client, TC300K_CMD_SAR_ENABLE);
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

	if ((!data->enabled) || data->fw_downloading) {
		SENSOR_ERR("can't excute\n");
		return -1;
	}

	buf = i2c_smbus_read_byte_data(client, TC300K_FWVER);
	if (buf < 0) {
		while (retry--) {
			SENSOR_ERR("read fail(%d, %d)\n", buf, retry);
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

	if ((!data->enabled) || data->fw_downloading) {
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
	int ret;
	enum of_gpio_flags flags;

	of_property_read_u32(np, "coreriver,use_bitmap", &pdata->use_bitmap);

	SENSOR_INFO("%s protocol.\n",
				pdata->use_bitmap ? "Use Bit-map" : "Use OLD");

	pdata->gpio_scl = of_get_named_gpio_flags(np, "coreriver,scl-gpio", 0, &pdata->scl_gpio_flags);
	pdata->gpio_sda = of_get_named_gpio_flags(np, "coreriver,sda-gpio", 0, &pdata->sda_gpio_flags);
	pdata->gpio_int = of_get_named_gpio_flags(np, "coreriver,irq-gpio", 0, &pdata->irq_gpio_flags);
	
	pdata->ldo_en = of_get_named_gpio_flags(np, "coreriver,ldo_en", 0, &flags);
	if (pdata->ldo_en < 0) {
		SENSOR_ERR("fail to get ldo_en\n");
		pdata->ldo_en = 0;
        if (of_property_read_string(np, "coreriver,regulator_ic", &pdata->regulator_ic)) {
            SENSOR_ERR("Failed to get regulator_ic name property\n");
            return -EINVAL;
        }
        regulator_ic = pdata->regulator_ic;
	} else {
		ret = gpio_request(pdata->ldo_en, "grip_ldo_en");
		if (ret < 0) {
			SENSOR_ERR("gpio %d request failed %d\n", pdata->ldo_en, ret);
			return ret;
		}
		gpio_direction_output(pdata->ldo_en, 0);
	}

	pdata->boot_on_ldo = of_property_read_bool(np, "coreriver,boot-on-ldo");

	pdata->i2c_gpio = of_property_read_bool(np, "coreriver,i2c-gpio");

	if (of_property_read_string(np, "coreriver,fw_name", &pdata->fw_name)) {
		SENSOR_ERR("Failed to get fw_name property\n");
		return -EINVAL;
	} else {
		SENSOR_INFO("fw_name %s\n", pdata->fw_name);
	}

	pdata->bringup = of_property_read_bool(np, "coreriver,bringup");
	if (pdata->bringup  < 0)
		pdata->bringup = 0;
	SENSOR_INFO("grip_int:%d, ldo_en:%d\n", pdata->gpio_int, pdata->ldo_en);

	return 0;
}

int tc3xxk_grip_power(void *info, bool on)
{
	struct tc3xxk_data *data = (struct tc3xxk_data *)info;
	struct regulator *regulator;
	int ret = 0;

	if (tc3xxk_power_enabled == on)
		return 0;

	SENSOR_INFO("%s\n", on ? "on" : "off");
	
    /* ldo control*/
	if (data->pdata->ldo_en) {
		gpio_set_value(data->pdata->ldo_en, on);
		SENSOR_INFO("ldo_en power %d\n", on);
        tc3xxk_power_enabled = on;
        return 0;
	}

    /*regulator control*/
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

	if ((!data->enabled) || data->fw_downloading) {
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
		data->fw_downloading = true;
		ret = tc3xxk_flash_fw(data, fw_path);
		data->fw_downloading = false;
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



static int tc3xxk_mode_enable(struct i2c_client *client, u8 cmd)
{
	int ret;

	ret = i2c_smbus_write_byte_data(client, TC300K_CMD_ADDR, cmd);
	msleep(15);

	return ret;
}


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
		data->fw_ver = 0xFF;
        SENSOR_INFO("firmware is blank or i2c fail, try flash firmware to grip\n");
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

static int tc3xxk_probe(struct i2c_client *client,
		const struct i2c_device_id *id)
{
	struct tc3xxk_platform_data *pdata;
	struct tc3xxk_data *data;
	int ret = 0;
	
	SENSOR_INFO("\n");

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
			ret = -ENOMEM;
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
	data->client = client;

	if (data->pdata == NULL) {
		SENSOR_ERR("failed to get platform data\n");

		ret = -EINVAL;
		goto err_platform_data;
	}
	data->irq = -1;

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

	data->dev = &client->dev;
	data->irq = pdata->gpio_int;
	data->irq_check = false;
	
	// default working on stop mode
	ret = tc3xxk_wake_up(data->client, TC300K_CMD_WAKE_UP);
	ret = tc3xxk_mode_enable(data->client, TC300K_CMD_IC_STOP_ON);
	if (ret < 0) {
		SENSOR_ERR("Change mode fail(%d)\n", ret);
	}

	ret = tc3xxk_mode_check(client);
	if (ret >= 0) {
		data->sar_enable = !!(ret & TC300K_MODE_SAR);
		SENSOR_INFO("mode %d, sar %d\n", ret, data->sar_enable);
	}
	device_init_wakeup(&client->dev, true);

	SENSOR_INFO("done\n");
	return 0;

err_fw_check:
	data->pdata->power(data, false);
err_pinctrl_init:
err_platform_data:
	kfree(data);
err_alloc_data:
	SENSOR_ERR("failed\n");
	return ret;
}

static int tc3xxk_remove(struct i2c_client *client)
{
	struct tc3xxk_data *data = i2c_get_clientdata(client);

	device_init_wakeup(&client->dev, false);
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

	device_init_wakeup(&client->dev, false);;
	data->pdata->power(data, false);
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

static struct i2c_driver tc3xxk_driver = {
	.probe = tc3xxk_probe,
	.remove = tc3xxk_remove,
	.shutdown = tc3xxk_shutdown,
	.driver = {
		.name = MODEL_NAME,
		.owner = THIS_MODULE,
		.pm = NULL,
		.of_match_table = coreriver_match_table,
	},
	.id_table = tc3xxk_id,
};

static int __init tc3xxk_init(void)
{
	return i2c_add_driver(&tc3xxk_driver);
}

static void __exit tc3xxk_exit(void)
{
	i2c_del_driver(&tc3xxk_driver);
}

module_init(tc3xxk_init);
module_exit(tc3xxk_exit);

MODULE_AUTHOR("Samsung Electronics");
MODULE_DESCRIPTION("Grip Sensor driver for Coreriver TC3XXK");
MODULE_LICENSE("GPL");
