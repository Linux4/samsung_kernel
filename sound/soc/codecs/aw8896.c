/*
 * aw8896.c   aw8896 codec module
 *
 * Version: v1.0.8
 *
 * Copyright (c) 2017 AWINIC Technology CO., LTD
 *
 *  Author: Nick Li <liweilei@awinic.com.cn>
 *
 * This program is free software; you can redistribute  it and/or modify it
 * under  the terms of  the GNU General  Public License as published by the
 * Free Software Foundation;  either version 2 of the  License, or (at your
 * option) any later version.
 */

#include <linux/module.h>
#include <linux/i2c.h>
#include <sound/core.h>
#include <sound/pcm.h>
#include <sound/pcm_params.h>
#include <sound/soc.h>
#include <linux/of_gpio.h>
#include <linux/delay.h>
#include <linux/device.h>
#include <linux/firmware.h>
#include <linux/i2c.h>
#include <linux/debugfs.h>
#include <linux/version.h>
#include <linux/input.h>
#include <sound/tlv.h>
#include "aw8896.h"
#include "aw8896_reg.h"

/******************************************************
 *
 * Marco
 *
 ******************************************************/
#define AW8896_I2C_NAME "aw889x_smartpa"

#define AW8896_VERSION "v1.0.8_19_1"

#define AW8896_RATES SNDRV_PCM_RATE_8000_48000
#define AW8896_FORMATS (SNDRV_PCM_FMTBIT_S16_LE | \
			SNDRV_PCM_FMTBIT_S24_LE | \
			SNDRV_PCM_FMTBIT_S32_LE)

#define AWINIC_I2C_REGMAP

#define AW_I2C_RETRIES 5
#define AW_I2C_RETRY_DELAY 5  /*5ms*/
#define AW_READ_CHIPID_RETRIES 5
#define AW_READ_CHIPID_RETRY_DELAY 5

#define AW8896_MAX_DSP_START_TRY_COUNT    20


static int aw8896_spk_control;
static int aw8896_rcv_control;

#define AW8896_FW_NAME_MAX     64
#define AW8896_MAX_FIRMWARE_LOAD_CNT 20
static char *aw8896_reg_name = "aw8896_reg.bin";
static char aw8896_fw_name[][AW8896_FW_NAME_MAX] = {
	{"aw8896_fw_d.bin"},
	{"aw8896_fw_e.bin"},
};
static unsigned char aw8896_dsp_fw_data[AW8896_DSP_FW_SIZE] = {0};

static int aw8896_i2c_write(struct aw8896 *aw8896,
			unsigned char reg_addr, unsigned int reg_data);
static int aw8896_i2c_read(struct aw8896 *aw8896,
			unsigned char reg_addr, unsigned int *reg_data);

static void aw8896_interrupt_setup(struct aw8896 *aw8896);
static void aw8896_interrupt_clear(struct aw8896 *aw8896);

/******************************************************
 *
 * aw8896 i2c write/read
 *
 ******************************************************/
#ifndef AWINIC_I2C_REGMAP
static int i2c_write(struct aw8896 *aw8896,
				unsigned char addr, unsigned int reg_data)
{
	int ret;
	u8 wbuf[512] = {0};

	struct i2c_msg msgs[] = {
		{
			.addr    = aw8896->i2c->addr,
			.flags    = 0,
			.len    = 3,
			.buf    = wbuf,
		},
	};

	wbuf[0] = addr;
	wbuf[1] = (unsigned char)((reg_data & 0xff00)>>8);
	wbuf[2] = (unsigned char)(reg_data & 0x00ff);

	ret = i2c_transfer(aw8896->i2c->adapter, msgs, 1);
	if (ret < 0)
		pr_err("%s: i2c write error: %d\n", __func__, ret);

	return ret;
}

static int i2c_read(struct aw8896 *aw8896,
		unsigned char addr, unsigned int *reg_data)
{
	int ret;
	unsigned char rbuf[512] = {0};
	unsigned int get_data;

	struct i2c_msg msgs[] = {
		{
			.addr    = aw8896->i2c->addr,
			.flags    = 0,
			.len    = 1,
			.buf    = &addr,
		},
		{
			.addr    = aw8896->i2c->addr,
			.flags    = I2C_M_RD,
			.len    = 2,
			.buf    = rbuf,
		},
	};

	ret = i2c_transfer(aw8896->i2c->adapter, msgs, 2);
	if (ret < 0) {
		pr_err("%s: i2c read error: %d\n", __func__, ret);
		return ret;
	}

	get_data = (unsigned int)(rbuf[0] & 0x00ff);
	get_data <<= 8;
	get_data |= (unsigned int)rbuf[1];

	*reg_data = get_data;

	return ret;
}
#endif

static int aw8896_i2c_write(struct aw8896 *aw8896,
			unsigned char reg_addr, unsigned int reg_data)
{
	int ret = -1;
	unsigned char cnt = 0;

	while (cnt < AW_I2C_RETRIES) {
#ifdef AWINIC_I2C_REGMAP
		ret = regmap_write(aw8896->regmap, reg_addr, reg_data);
		if (ret < 0) {
			pr_err("%s: regmap_write cnt=%d error=%d\n",
				__func__, cnt, ret);
		} else {
			break;
		}
#else
		ret = i2c_write(aw8896, reg_addr, reg_data);
		if (ret < 0) {
			pr_err("%s: i2c_write cnt=%d error=%d\n",
				__func__, cnt, ret);
		} else {
			break;
		}
#endif
		cnt++;
	}

	return ret;
}

static int aw8896_i2c_read(struct aw8896 *aw8896,
			unsigned char reg_addr, unsigned int *reg_data)
{
	int ret = -1;
	unsigned char cnt = 0;

	while (cnt < AW_I2C_RETRIES) {
#ifdef AWINIC_I2C_REGMAP
		ret = regmap_read(aw8896->regmap, reg_addr, reg_data);
		if (ret < 0) {
			pr_err("%s: regmap_read cnt=%d error=%d\n",
				__func__, cnt, ret);
		} else {
			break;
		}
#else
		ret = i2c_read(aw8896, reg_addr, reg_data);
		if (ret < 0) {
			pr_err("%s: i2c_read cnt=%d error=%d\n",
				__func__, cnt, ret);
		} else {
			break;
		}
#endif
		cnt++;
	}

	return ret;
}

static int aw8896_i2c_writes(struct aw8896 *aw8896,
			unsigned char reg_addr,
			unsigned char *buf,
			unsigned int len)
{
	int ret = -1;
	unsigned char *data;

	data = kmalloc(len+1, GFP_KERNEL);
	if (data == NULL)
		return  -ENOMEM;

	data[0] = reg_addr;
	memcpy(&data[1], buf, len);

	ret = i2c_master_send(aw8896->i2c, data, len+1);
	if (ret < 0)
		pr_err("%s: i2c send error\n", __func__);

	kfree(data);

	return ret;
}
/******************************************************
 *
 * aw8896 control
 *
 ******************************************************/
static void aw8896_run_mute(struct aw8896 *aw8896, bool mute)
{
	unsigned int reg_val = 0;

	pr_debug("%s enter\n", __func__);

	aw8896_i2c_read(aw8896, AW8896_REG_PWMCTRL, &reg_val);
	if (mute)
		reg_val |= AW8896_BIT_PWMCTRL_HMUTE;
	else
		reg_val &= (~AW8896_BIT_PWMCTRL_HMUTE);

	aw8896_i2c_write(aw8896, AW8896_REG_PWMCTRL, reg_val);
}

static void aw8896_run_pwd(struct aw8896 *aw8896, bool pwd)
{
	unsigned int reg_val = 0;

	pr_debug("%s enter\n", __func__);

	aw8896_i2c_read(aw8896, AW8896_REG_SYSCTRL, &reg_val);
	if (pwd)
		reg_val |= AW8896_BIT_SYSCTRL_PWDN;
	else
		reg_val &= (~AW8896_BIT_SYSCTRL_PWDN);

	aw8896_i2c_write(aw8896, AW8896_REG_SYSCTRL, reg_val);
}

static void aw8896_dsp_enable(struct aw8896 *aw8896, bool dsp)
{
	unsigned int reg_val = 0;

	pr_debug("%s enter\n", __func__);

	aw8896_i2c_read(aw8896, AW8896_REG_SYSCTRL, &reg_val);
	if (dsp)
		reg_val &= (~AW8896_BIT_SYSCTRL_DSPBY);
	else
		reg_val |= AW8896_BIT_SYSCTRL_DSPBY;

	aw8896_i2c_write(aw8896, AW8896_REG_SYSCTRL, reg_val);
}

static int aw8896_get_iis_status(struct aw8896 *aw8896)
{
	int ret = -1;
	unsigned int reg_val = 0;

	pr_debug("%s enter\n", __func__);

	aw8896_i2c_read(aw8896, AW8896_REG_SYSST, &reg_val);
	if (reg_val & AW8896_BIT_SYSST_PLLS)
		ret = 0;

	return ret;
}

static int aw8896_get_dsp_status(struct aw8896 *aw8896)
{
	int ret = -1;
	int retry = 50;
	unsigned int reg_val = 0;

	pr_debug("%s enter\n", __func__);
	while (retry-- > 0) {
		aw8896_i2c_read(aw8896, AW8896_REG_WDT, &reg_val);
		if (reg_val)
			return 0;
		usleep_range(1000, 2000);
	}
	return ret;
}

static void aw8896_spk_rcv_mode(struct aw8896 *aw8896)
{
	unsigned int reg_val = 0;

	pr_debug("%s spk_rcv=%d\n", __func__, aw8896->spk_rcv_mode);

	if (aw8896->spk_rcv_mode == AW8896_SPEAKER_MODE) {
		/*  RFB IDAC = 6V */
		aw8896_i2c_read(aw8896, AW8896_REG_AMPDBG1, &reg_val);
		reg_val |= AW8896_BIT_AMPDBG1_OPD;
		reg_val &= (~AW8896_BIT_AMPDBG1_IPWM_20UA);
		reg_val &= (~AW8896_BIT_AMPDBG1_RFB_MASK);
		reg_val |= 0x0002;
		aw8896_i2c_write(aw8896, AW8896_REG_AMPDBG1, reg_val);

		/*  Speaker Mode */
		aw8896_i2c_read(aw8896, AW8896_REG_SYSCTRL, &reg_val);
		reg_val &= (~AW8896_BIT_SYSCTRL_RCV_MODE);
		aw8896_i2c_write(aw8896, AW8896_REG_SYSCTRL, reg_val);

	} else if (aw8896->spk_rcv_mode == AW8896_RECEIVER_MODE) {
		/*  RFB IDAC = 4V */
		aw8896_i2c_read(aw8896, AW8896_REG_AMPDBG1, &reg_val);
		reg_val |= AW8896_BIT_AMPDBG1_OPD;
		reg_val |= (AW8896_BIT_AMPDBG1_IPWM_20UA);
		reg_val &= (~AW8896_BIT_AMPDBG1_RFB_MASK);
		reg_val |= 0x000B;
		aw8896_i2c_write(aw8896, AW8896_REG_AMPDBG1, reg_val);

		/*  Receiver Mode */
		aw8896_i2c_read(aw8896, AW8896_REG_SYSCTRL, &reg_val);
		reg_val |= AW8896_BIT_SYSCTRL_RCV_MODE;
		aw8896_i2c_write(aw8896, AW8896_REG_SYSCTRL, reg_val);
	} else {
	}
}

static void aw8896_tx_cfg(struct aw8896 *aw8896)
{
	unsigned int reg_val = 0;

	aw8896_i2c_read(aw8896, AW8896_REG_I2STXCFG, &reg_val);
	reg_val |= AW8896_BIT_I2STXCFG_TXEN;
	aw8896_i2c_write(aw8896, AW8896_REG_I2STXCFG, reg_val);

	if (aw8896->dsp_fw_ver == AW8896_DSP_FW_VER_D) {
		aw8896_i2c_read(aw8896, AW8896_REG_DBGCTRL, &reg_val);
		reg_val |= AW8896_BIT_DBGCTRL_LPBK_NEARE;
		aw8896_i2c_write(aw8896, AW8896_REG_DBGCTRL, reg_val);
	}
}

static int aw8896_get_sysint(struct aw8896 *aw8896, uint16_t *sysint)
{
	int ret = -1;
	unsigned int reg_val = 0;

	ret = aw8896_i2c_read(aw8896, AW8896_REG_SYSINT, &reg_val);
	if (ret < 0)
		pr_info("%s: read sysint fail, ret=%d\n",
				__func__, ret);
	else
		*sysint = reg_val;

	return ret;
}

static int aw8896_set_intmask(struct aw8896 *aw8896, bool flag)
{
	if (flag)
		aw8896_interrupt_setup(aw8896);
	else
		aw8896_i2c_write(aw8896, AW8896_REG_SYSINTM,
					AW8896_BIT_SYSINTM_MASK);

	return 0;
}

static void aw8896_start(struct aw8896 *aw8896, bool dsp_en)
{
	int ret = -1;
	uint16_t sysint = 0;
	int i;

	pr_debug("%s enter\n", __func__);

	aw8896_run_pwd(aw8896, false);
	for (i = 0; i < AW8896_MAX_DSP_START_TRY_COUNT; i++) {
		ret = aw8896_get_iis_status(aw8896);
		if (ret < 0) {
			pr_err("%s: get no iis signal, ret=%d\n",
				__func__, ret);
			usleep_range(2000, 2000 + 20);
			if (i == AW8896_MAX_DSP_START_TRY_COUNT - 1)
				break;
		} else {
			if (dsp_en) {
				aw8896_dsp_enable(aw8896, true);
			}
			break;
		}
	}
	aw8896_run_mute(aw8896, false);

	ret = aw8896_get_sysint(aw8896, &sysint);
	if (ret < 0)
		pr_err("%s: get_sysint fail, ret=%d\n",
			__func__, ret);
	else
		pr_info("%s: get_sysint=0x%04x\n",
			__func__, sysint);

	ret = aw8896_get_sysint(aw8896, &sysint);
	if (ret < 0)
		pr_err("%s: get_sysint fail, ret=%d\n",
			__func__, ret);
	else
		pr_info("%s: get_sysint=0x%04x\n",
			__func__, sysint);

	aw8896_set_intmask(aw8896, true);
	aw8896->status = AW8896_PW_ON;
}

static void aw8896_stop(struct aw8896 *aw8896)
{
	int ret = -1;
	uint16_t sysint = 0;

	ret = aw8896_get_sysint(aw8896, &sysint);
	if (ret < 0)
		pr_err("%s: get_sysint fail, ret=%d\n", __func__, ret);
	else
		pr_info("%s: get_sysint=0x%04x\n", __func__, sysint);

	aw8896_run_mute(aw8896, true);
	aw8896_set_intmask(aw8896, false);

	aw8896_dsp_enable(aw8896, false);
	usleep_range(1000, 2000);
	aw8896_run_pwd(aw8896, true);
}

static void aw8896_dsp_container_update(struct aw8896 *aw8896,
				struct aw8896_container *aw8896_cont,
				int base)
{
	int i = 0;
#ifdef AWINIC_DSP_I2C_WRITES
	unsigned char tmp_val = 0;
	unsigned int tmp_len = 0;
#else
	int tmp_val = 0;
	int addr = 0;
#endif

	pr_debug("%s enter\n", __func__);

#ifdef AWINIC_DSP_I2C_WRITES
	/* i2c writes */
	aw8896_i2c_write(aw8896, AW8896_REG_DSPMADD, base);
	for (i = 0; i < aw8896_cont->len; i += 2) {
		tmp_val = aw8896_cont->data[i+0];
		aw8896_cont->data[i+0] = aw8896_cont->data[i+1];
		aw8896_cont->data[i+1] = tmp_val;
	}
	for (i = 0; i < aw8896_cont->len; i += MAX_RAM_WRITE_BYTE_SIZE) {
		if ((aw8896_cont->len - i) < MAX_RAM_WRITE_BYTE_SIZE)
			tmp_len = aw8896_cont->len - i;
		else
			tmp_len = MAX_RAM_WRITE_BYTE_SIZE;

		aw8896_i2c_writes(aw8896, AW8896_REG_DSPMDAT,
			&aw8896_cont->data[i], tmp_len);
	}
#else
	/* i2c write */
	for (i = 0; i < aw8896_cont->len; i += 2) {
		tmp_val = (aw8896_cont->data[i+1]<<8) + aw8896_cont->data[i+0];
		aw8896_i2c_write(aw8896, AW8896_REG_DSPMADD, base+addr);
		aw8896_i2c_write(aw8896, AW8896_REG_DSPMDAT, tmp_val);
		addr++;
	}
#endif

	pr_debug("%s exit\n", __func__);
}

static void aw8896_fw_loaded(const struct firmware *cont, void *context)
{
	struct aw8896 *aw8896 = context;
	struct aw8896_container *aw8896_fw;
	/*  int i; */
	int ret = -1;

	pr_debug("%s enter\n", __func__);

	aw8896->dsp_fw_state = AW8896_DSP_FW_FAIL;

	if (!cont) {
		pr_err("%s: failed to read %s\n", __func__,
			aw8896_fw_name[aw8896->dsp_fw_ver]);
		release_firmware(cont);
		return;
	}

	pr_info("%s: loaded %s - size: %zu\n", __func__,
		aw8896_fw_name[aw8896->dsp_fw_ver],
			cont ? cont->size : 0);
	if (cont->size > AW8896_DSP_FW_SIZE) {
		pr_err("%s: Error:Size exceeds the limit of 512\n", __func__);
		release_firmware(cont);
		return;
	}

	aw8896_fw = kzalloc(cont->size+sizeof(int), GFP_KERNEL);
	if (!aw8896_fw) {
		release_firmware(cont);
		pr_err("%s: Error allocating memory\n", __func__);
		return;
	}
	aw8896_fw->len = cont->size;
	memcpy(aw8896_fw->data, cont->data, cont->size);

	mutex_lock(&aw8896->lock);
	aw8896_dsp_container_update(aw8896, aw8896_fw,
		AW8896_DSP_FW_BASE);
	memcpy(aw8896_fw->data, cont->data, cont->size);
	aw8896_dsp_container_update(aw8896, aw8896_fw,
		AW8896_DSP_FW_BACKUP_BASE);

	memcpy(aw8896_dsp_fw_data, cont->data, cont->size);

	release_firmware(cont);
	aw8896->dsp_fw_len = aw8896_fw->len;
	kfree(aw8896_fw);

	pr_info("%s: fw update complete\n", __func__);

	aw8896_dsp_enable(aw8896, true);

	usleep_range(1000, 2000);

	ret = aw8896_get_dsp_status(aw8896);
	if (ret) {
		aw8896->init = AW8896_INIT_NG;
		aw8896_dsp_enable(aw8896, false);
		pr_info("%s: dsp working wdt, dsp fw update failed\n",
			__func__);
	} else {
		aw8896->init = AW8896_INIT_OK;
		aw8896->dsp_fw_state = AW8896_DSP_FW_OK;
		pr_info("%s: init ok\n", __func__);
	}

	aw8896_tx_cfg(aw8896);
	aw8896_spk_rcv_mode(aw8896);
	aw8896_start(aw8896, false);
	if (!(aw8896->flags & AW8896_FLAG_SKIP_INTERRUPTS)) {
		aw8896_interrupt_clear(aw8896);
		aw8896_interrupt_setup(aw8896);
	}
	mutex_unlock(&aw8896->lock);
}

static int aw8896_load_fw(struct aw8896 *aw8896)
{
	unsigned int reg_val = 0;
	unsigned int tmp_val = 0;

	pr_info("%s enter\n", __func__);

	if (aw8896->dsp_fw_state == AW8896_DSP_FW_OK) {
		pr_info("%s: dsp fw ok\n", __func__);
		return 0;
	}

	aw8896->dsp_fw_state = AW8896_DSP_FW_PENDING;

	aw8896_i2c_write(aw8896, AW8896_REG_DSPMADD, AW8896_DSP_FW_VER_BASE);
	aw8896_i2c_read(aw8896, AW8896_REG_DSPMDAT, &reg_val);
		tmp_val |= reg_val;
	aw8896_i2c_write(aw8896, AW8896_REG_DSPMADD, AW8896_DSP_FW_VER_BASE+1);
	aw8896_i2c_read(aw8896, AW8896_REG_DSPMDAT, &reg_val);
		tmp_val |= (reg_val << 16);

	if (tmp_val == 0xdeac97d6) {
		aw8896->dsp_fw_ver = AW8896_DSP_FW_VER_E;
		pr_info("%s: dsp fw read version: AW8896_DSP_VER_FW_E: 0x%x\n",
			__func__, tmp_val);
	} else if (tmp_val == 0) {
		aw8896->dsp_fw_ver = AW8896_DSP_FW_VER_D;
		pr_info("%s: dsp fw read version: AW8896_DSP_FW_VER_D: 0x%x\n",
			__func__, tmp_val);
	} else {
		pr_err("%s: dsp fw read version err: 0x%x\n",
			__func__, tmp_val);
		return -EINVAL;
	}

	aw8896_run_mute(aw8896, true);
	aw8896_dsp_enable(aw8896, false);

	return request_firmware_nowait(THIS_MODULE, FW_ACTION_HOTPLUG,
			aw8896_fw_name[aw8896->dsp_fw_ver],
			aw8896->dev, GFP_KERNEL,
			aw8896, aw8896_fw_loaded);
}

static void aw8896_reg_container_update(struct aw8896 *aw8896,
					struct aw8896_container *aw8896_cont)
{
	int i = 0;
	int reg_addr = 0;
	int reg_val = 0;

	pr_debug("%s enter\n", __func__);

	mutex_lock(&aw8896->lock);
	for (i = 0; i < aw8896_cont->len; i += 4) {
		reg_addr = (aw8896_cont->data[i+1]<<8) + aw8896_cont->data[i+0];
		reg_val = (aw8896_cont->data[i+3]<<8) + aw8896_cont->data[i+2];
		pr_debug("%s: reg=0x%04x, val = 0x%04x\n",
				__func__, reg_addr, reg_val);
		aw8896_i2c_write(aw8896, (unsigned char)reg_addr,
						(unsigned int)reg_val);
	}
	mutex_unlock(&aw8896->lock);

	pr_debug("%s exit\n", __func__);
}

static void aw8896_reg_loaded(const struct firmware *cont, void *context)
{
	struct aw8896 *aw8896 = context;
	struct aw8896_container *aw8896_reg;
	int ret = -1;
	int i = 0;
	int iis_check_max = 5;
	unsigned int reg_val = 0;

	if (!cont) {
		pr_err("%s: failed to read %s\n", __func__, aw8896_reg_name);
		release_firmware(cont);
		return;
	}

	pr_info("%s: loaded %s - size: %zu\n", __func__, aw8896_reg_name,
			cont ? cont->size : 0);

	aw8896_reg = kzalloc(cont->size+sizeof(int), GFP_KERNEL);
	if (!aw8896_reg) {
		release_firmware(cont);
		pr_err("%s: error allocating memory\n", __func__);
		return;
	}
	aw8896_reg->len = cont->size;
	memcpy(aw8896_reg->data, cont->data, cont->size);
	release_firmware(cont);

	aw8896_reg_container_update(aw8896, aw8896_reg);
	aw8896_i2c_read(aw8896, AW8896_REG_DBGCTRL, &reg_val);
	aw8896_i2c_write(aw8896, AW8896_REG_DBGCTRL,
		reg_val | AW8896_BIT_DBGCTRL_DISNCKRST);

	aw8896_i2c_read(aw8896, AW8896_REG_I2SCTRL, &reg_val);
	aw8896_i2c_write(aw8896, AW8896_REG_I2SCTRL,
		reg_val & (~AW8896_BIT_I2SCTRL_INPLEV));

	kfree(aw8896_reg);

	for (i = 0; i < iis_check_max; i++) {
		ret = aw8896_get_iis_status(aw8896);
		if (ret < 0) {
			pr_err("%s: get no iis signal, ret=%d\n",
				__func__, ret);
		} else {
			usleep_range(5000, 6000);
			ret = aw8896_load_fw(aw8896);
			if (ret) {
				pr_err("%s: fw loading requested failed: %d\n",
					__func__, ret);
			}
			break;
		}
		usleep_range(2000, 3000);
	}
}

static int aw8896_load_reg(struct aw8896 *aw8896)
{

	pr_info("%s enter\n", __func__);

	return request_firmware_nowait(THIS_MODULE, FW_ACTION_HOTPLUG,
		aw8896_reg_name, aw8896->dev, GFP_KERNEL,
		aw8896, aw8896_reg_loaded);
}

static void aw8896_cold_start(struct aw8896 *aw8896)
{
	int ret = -1;

	pr_info("%s enter\n", __func__);

	ret = aw8896_load_reg(aw8896);
	if (ret)
		pr_err("%s: reg/fw loading requested failed: %d\n",
			__func__, ret);
}

static void aw8896_smartpa_cfg(struct aw8896 *aw8896, bool flag)
{
	pr_info("%s, flag = %d\n", __func__, flag);

	if (flag == true) {
		if ((aw8896->init == AW8896_INIT_ST) ||
			(aw8896->init == AW8896_INIT_NG)) {
			pr_info("%s, init = %d\n", __func__, aw8896->init);
			aw8896_cold_start(aw8896);
		} else {
			aw8896_spk_rcv_mode(aw8896);
			aw8896_start(aw8896, true);
		}
	} else {
		aw8896_stop(aw8896);
	}
}

static void aw8896_start_work(struct work_struct *work)
{
	struct aw8896 *aw8896 = container_of(work, struct aw8896, start_work);

	pr_info("%s enter\n", __func__);
	mutex_lock(&aw8896->lock);
	aw8896_smartpa_cfg(aw8896, true);
	mutex_unlock(&aw8896->lock);
}
/******************************************************
 *
 * kcontrol
 *
 ******************************************************/
static const char *const spk_function[] = { "Off", "On" };
static const char *const rcv_function[] = { "Off", "On" };
static const DECLARE_TLV_DB_SCALE(digital_gain, 0, 50, 0);

struct soc_mixer_control aw8896_mixer = {
	.reg    = AW8896_REG_DSPCFG,
	.shift  = AW8896_VOL_REG_SHIFT,
	.max    = AW8896_VOLUME_MAX,
	.min    = AW8896_VOLUME_MIN,
};

static int aw8896_volume_info(struct snd_kcontrol *kcontrol,
			struct snd_ctl_elem_info *uinfo)
{
	struct soc_mixer_control *mc =
			(struct soc_mixer_control *) kcontrol->private_value;

	/* set kcontrol info */
	uinfo->type = SNDRV_CTL_ELEM_TYPE_INTEGER;
	uinfo->count = 1;
	uinfo->value.integer.min = 0;
	uinfo->value.integer.max = mc->max - mc->min;
	return 0;
}

static int aw8896_volume_get(struct snd_kcontrol *kcontrol,
			struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *codec = snd_soc_kcontrol_component(kcontrol);
	struct aw8896 *aw8896 = snd_soc_component_get_drvdata(codec);
	int value;
	struct soc_mixer_control *mc =
			(struct soc_mixer_control *) kcontrol->private_value;

	aw8896_i2c_read(aw8896, AW8896_REG_DSPCFG, &value);
	ucontrol->value.integer.value[0] = (value >> mc->shift)
					&(~AW8896_BIT_DSPCFG_VOL_MASK);
	return 0;
}

static int aw8896_volume_put(struct snd_kcontrol *kcontrol,
			struct snd_ctl_elem_value *ucontrol)
{
	struct soc_mixer_control *mc =
		(struct soc_mixer_control *)kcontrol->private_value;
	struct snd_soc_component *codec = snd_soc_kcontrol_component(kcontrol);
	struct aw8896 *aw8896 = snd_soc_component_get_drvdata(codec);
	unsigned int value, reg_value;

	/* value is right */
	value = ucontrol->value.integer.value[0];
	if (value > (mc->max-mc->min) || value < 0) {
		pr_err("%s:value over range\n", __func__);
		return -EPERM;
	}

	/* smartpa have clk */
	aw8896_i2c_read(aw8896, AW8896_REG_SYSST, &reg_value);
	if (!(reg_value&AW8896_BIT_SYSST_PLLS)) {
		pr_err("%s: NO I2S CLK ,cat not write reg\n", __func__);
		return 0;
	}
	/* cal real value */
	value = value << mc->shift&AW8896_BIT_DSPCFG_VOL_MASK;
	aw8896_i2c_read(aw8896, AW8896_REG_DSPCFG, &reg_value);
	value = value | (reg_value&0x00ff);

	/* write value */
	aw8896_i2c_read(aw8896, AW8896_REG_SYSCTRL, &reg_value);
	if (reg_value&AW8896_BIT_SYSCTRL_DSPBY) {
		aw8896_i2c_write(aw8896, AW8896_REG_DSPCFG, value);
	} else {
		aw8896_dsp_enable(aw8896, false);
		aw8896_i2c_write(aw8896, AW8896_REG_DSPCFG, value);
		aw8896_dsp_enable(aw8896, true);
	}

	return 0;
}

static struct snd_kcontrol_new aw8896_volume = {
	.iface = SNDRV_CTL_ELEM_IFACE_MIXER,
	.name  = "aw8896_rx_volume",
	.access = SNDRV_CTL_ELEM_ACCESS_TLV_READ |
		SNDRV_CTL_ELEM_ACCESS_READWRITE,
	.tlv.p = (digital_gain),
	.info = aw8896_volume_info,
	.get =  aw8896_volume_get,
	.put =  aw8896_volume_put,
	.private_value = (unsigned long)&aw8896_mixer,
};

static int aw8896_spk_get(struct snd_kcontrol *kcontrol,
			struct snd_ctl_elem_value *ucontrol)
{
	pr_debug("%s: aw8896_spk_control=%d\n", __func__, aw8896_spk_control);
	ucontrol->value.integer.value[0] = aw8896_spk_control;
	return 0;
}

static int aw8896_spk_set(struct snd_kcontrol *kcontrol,
			struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *codec = snd_soc_kcontrol_component(kcontrol);
	struct aw8896 *aw8896 = snd_soc_component_get_drvdata(codec);

	pr_debug("%s: ucontrol->value.integer.value[0]=%ld\n",
				__func__, ucontrol->value.integer.value[0]);
	if (ucontrol->value.integer.value[0] == aw8896_spk_control)
		return 1;

	aw8896_spk_control = ucontrol->value.integer.value[0];

	aw8896->spk_rcv_mode = AW8896_SPEAKER_MODE;


	return 0;
}

static int aw8896_rcv_get(struct snd_kcontrol *kcontrol,
			struct snd_ctl_elem_value *ucontrol)
{
	pr_debug("%s: aw8896_rcv_control=%d\n", __func__, aw8896_rcv_control);
	ucontrol->value.integer.value[0] = aw8896_rcv_control;
	return 0;
}
static int aw8896_rcv_set(struct snd_kcontrol *kcontrol,
					struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *codec = snd_soc_kcontrol_component(kcontrol);
	struct aw8896 *aw8896 = snd_soc_component_get_drvdata(codec);

	pr_debug("%s: ucontrol->value.integer.value[0]=%ld\n",
			__func__, ucontrol->value.integer.value[0]);
	if (ucontrol->value.integer.value[0] == aw8896_rcv_control)
		return 1;

	aw8896_rcv_control = ucontrol->value.integer.value[0];

	aw8896->spk_rcv_mode = AW8896_RECEIVER_MODE;
	aw8896->init = AW8896_INIT_ST;

	return 0;
}


static const struct soc_enum aw8896_snd_enum[] = {
	SOC_ENUM_SINGLE_EXT(ARRAY_SIZE(spk_function), spk_function),
	SOC_ENUM_SINGLE_EXT(ARRAY_SIZE(rcv_function), rcv_function),
};

static struct snd_kcontrol_new aw8896_controls[] = {
	SOC_ENUM_EXT("aw8896_speaker_switch", aw8896_snd_enum[0],
					aw8896_spk_get, aw8896_spk_set),
	SOC_ENUM_EXT("aw8896_receiver_switch", aw8896_snd_enum[1],
					aw8896_rcv_get, aw8896_rcv_set),
};

static void aw8896_add_codec_controls(struct aw8896 *aw8896)
{
	pr_info("%s enter\n", __func__);

	snd_soc_add_component_controls(aw8896->component, aw8896_controls,
		ARRAY_SIZE(aw8896_controls));

	snd_soc_add_component_controls(aw8896->component, &aw8896_volume, 1);
}


/******************************************************
 *
 * Digital Audio Interface
 *
 ******************************************************/
static int aw8896_startup(struct snd_pcm_substream *substream,
			struct snd_soc_dai *dai)
{
	struct snd_soc_component *component = dai->component;
	struct aw8896 *aw8896 = snd_soc_component_get_drvdata(component);

	pr_info("%s: enter\n", __func__);
	if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK) {
		mutex_lock(&aw8896->lock);
		aw8896_run_pwd(aw8896, false);
		mutex_unlock(&aw8896->lock);
	} else {
		pr_info("%s: stream capute. do nothing\n", __func__);
	}
	return 0;
}

static int aw8896_set_fmt(struct snd_soc_dai *dai, unsigned int fmt)
{
	/*struct aw8896 *aw8896 = snd_soc_component_get_drvdata(dai->component);*/
	struct snd_soc_component *component = dai->component;

	pr_info("%s: fmt=0x%x\n", __func__, fmt);

	/* Supported mode: regular I2S, secondary, or PDM */
	switch (fmt & SND_SOC_DAIFMT_FORMAT_MASK) {
	case SND_SOC_DAIFMT_I2S:
		if ((fmt & SND_SOC_DAIFMT_MASTER_MASK) !=
			SND_SOC_DAIFMT_CBS_CFS) {
			dev_err(component->dev,
			"%s: invalid codec mode\n", __func__);
			return -EINVAL;
		}
		break;
	default:
		dev_err(component->dev, "%s: unsupported DAI format %d\n",
				__func__, (fmt & SND_SOC_DAIFMT_FORMAT_MASK));
		return -EINVAL;
	}
	return 0;
}

static int aw8896_set_dai_sysclk(struct snd_soc_dai *codec_dai,
				int clk_id, unsigned int freq, int dir)
{
	struct aw8896 *aw8896 =
		snd_soc_component_get_drvdata(codec_dai->component);

	pr_info("%s: freq=%d\n", __func__, freq);

	aw8896->sysclk = freq;
	return 0;
}

static int aw8896_hw_params(struct snd_pcm_substream *substream,
			struct snd_pcm_hw_params *params,
			struct snd_soc_dai *dai)
{
	struct snd_soc_component *component = dai->component;
	struct aw8896 *aw8896 = snd_soc_component_get_drvdata(component);
	unsigned int rate;
	int reg_value, tmp_value;
	int width;
	/* Supported */


	mutex_lock(&aw8896->lock);
	/* get rate param */
	aw8896->rate = rate = params_rate(params);
	pr_debug("%s: requested rate: %d, sample size: %d\n", __func__, rate,
			snd_pcm_format_width(params_format(params)));
	/* match rate */
	switch (rate) {
	case 8000:
		reg_value = AW8896_BIT_I2SCTRL_SR_8K;
		break;
	case 16000:
		reg_value = AW8896_BIT_I2SCTRL_SR_16K;
		break;
	case 32000:
		reg_value = AW8896_BIT_I2SCTRL_SR_32K;
		break;
	case 44100:
		reg_value = AW8896_BIT_I2SCTRL_SR_44K;
		break;
	case 48000:
		reg_value = AW8896_BIT_I2SCTRL_SR_48K;
		break;
	default:
		reg_value = AW8896_BIT_I2SCTRL_SR_48K;
		pr_err("%s: rate can not support\n", __func__);
		break;
	}
	/* set chip rate */
	if (-1 != reg_value) {
		aw8896_i2c_read(aw8896, AW8896_REG_I2SCTRL, &tmp_value);
		reg_value = reg_value |
		(tmp_value & (~AW8896_BIT_I2SCTRL_SR_MASK));
		aw8896_i2c_write(aw8896, AW8896_REG_I2SCTRL, reg_value);
	}

	/* get bit width */
	width = params_width(params);
	pr_debug("%s: width = %d\n", __func__, width);
	switch (width) {
	case 16:
		reg_value = AW8896_BIT_I2SCTRL_FMS_16BIT;
		break;
	case 20:
		reg_value = AW8896_BIT_I2SCTRL_FMS_20BIT;
		break;
	case 24:
		reg_value = AW8896_BIT_I2SCTRL_FMS_24BIT;
		break;
	case 32:
		reg_value = AW8896_BIT_I2SCTRL_FMS_32BIT;
		break;
	default:
		reg_value = AW8896_BIT_I2SCTRL_FMS_16BIT;
		pr_err("%s: width can not support\n", __func__);
		break;
	}
	/* set width */
	if (-1 != reg_value) {
		aw8896_i2c_read(aw8896, AW8896_REG_I2SCTRL, &tmp_value);
		reg_value = reg_value |
		(tmp_value&(~AW8896_BIT_I2SCTRL_FMS_MASK));
		aw8896_i2c_write(aw8896, AW8896_REG_I2SCTRL, reg_value);
	}
	mutex_unlock(&aw8896->lock);

	return 0;
}

static int aw8896_mute(struct snd_soc_dai *dai, int mute, int stream)
{
	struct snd_soc_component *component = dai->component;
	struct aw8896 *aw8896 = snd_soc_component_get_drvdata(component);
	unsigned int reg_val = 0;

	pr_info("%s: mute state=%d\n", __func__, mute);

	if (!(aw8896->flags & AW8896_FLAG_DSP_START_ON_MUTE))
		return 0;

	if (stream != SNDRV_PCM_STREAM_PLAYBACK) {
		pr_info("%s: not playback\n", __func__);
		return 0;
	}
	if (mute) {
		/* Stop DSP */
		mutex_lock(&aw8896->lock);
		aw8896->pstream = 0;

		aw8896_i2c_read(aw8896, AW8896_REG_SYSCTRL, &reg_val);

		if ((aw8896->status == AW8896_PW_OFF) &&
			(reg_val & AW8896_BIT_SYSCTRL_PWDN)) {
			mutex_unlock(&aw8896->lock);
			pr_info("%s: already power off", __func__);
			return 0;
		}
		aw8896->status = AW8896_PW_OFF;
		aw8896_smartpa_cfg(aw8896, false);
		mutex_unlock(&aw8896->lock);
	} else {
		/* Start DSP */
		mutex_lock(&aw8896->lock);
		if (aw8896->pstream == 1) {
			mutex_unlock(&aw8896->lock);
			pr_info("%s: already power on enter\n", __func__);
			return 0;
		}

		aw8896->pstream = 1;

		if (aw8896->status == AW8896_PW_ON) {
			mutex_unlock(&aw8896->lock);
			pr_info("%s: already power on", __func__);
			return 0;
		}
		/* check i2s in start */
		schedule_work(&aw8896->start_work);
		mutex_unlock(&aw8896->lock);
	}

	return 0;
}

static void aw8896_shutdown(struct snd_pcm_substream *substream,
	struct snd_soc_dai *dai)
{
	struct snd_soc_component *component = dai->component;
	struct aw8896 *aw8896 = snd_soc_component_get_drvdata(component);

	if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK) {
		aw8896->rate = 0;
		aw8896_run_pwd(aw8896, true);
	} else {
		pr_info("%s: stream capute. do nothing\n", __func__);
	}
}

static const struct snd_soc_dai_ops aw8896_dai_ops = {
	.startup = aw8896_startup,
	.set_fmt = aw8896_set_fmt,
	.set_sysclk = aw8896_set_dai_sysclk,
	.hw_params = aw8896_hw_params,
	.mute_stream = aw8896_mute,
	.shutdown = aw8896_shutdown,
};

static struct snd_soc_dai_driver aw8896_dai[] = {
	{
		.name = "aw8896-aif",
		.id = 1,
		.playback = {
			.stream_name = "Speaker_Playback",
			.channels_min = 1,
			.channels_max = 2,
			.rates = AW8896_RATES,
			.formats = AW8896_FORMATS,
		},
		.capture = {
			.stream_name = "Speaker_Capture",
			.channels_min = 1,
			.channels_max = 2,
			.rates = AW8896_RATES,
			.formats = AW8896_FORMATS,
		},
		.ops = &aw8896_dai_ops,
		.symmetric_rates = 1,
		.symmetric_channels = 1,
		.symmetric_samplebits = 1,
	},
};


/*****************************************************
 *
 * codec driver
 *
 *****************************************************/
static int aw8896_probe(struct snd_soc_component *component)
{
	struct aw8896 *aw8896 = snd_soc_component_get_drvdata(component);
	int ret = 0;

	pr_info("%s enter\n", __func__);

	aw8896->component = component;

	aw8896_add_codec_controls(aw8896);

	if (component->dev->of_node)
		dev_set_name(component->dev, "%s", "aw8896_smartpa");

	pr_info("%s exit\n", __func__);

	return ret;
}

static void aw8896_remove(struct snd_soc_component *component)
{
	/*struct aw8896 *aw8896 = snd_soc_component_get_drvdata(codec);*/
	pr_info("%s enter\n", __func__);
}

struct regmap *aw8896_get_regmap(struct device *dev)
{
	struct aw8896 *aw8896 = dev_get_drvdata(dev);

	return aw8896->regmap;
}

static unsigned int aw8896_codec_read(struct snd_soc_component *component,
	unsigned int reg)
{
	struct aw8896 *aw8896 = snd_soc_component_get_drvdata(component);
	unsigned int value = 0;
	int ret;

	pr_debug("%s:enter\n", __func__);
	if (aw8896_reg_access[reg]&REG_RD_ACCESS) {
		ret = aw8896_i2c_read(aw8896, reg, &value);
		if (ret < 0) {
			pr_debug("%s: read register failed\n", __func__);
			return ret;
		}
	} else {
		pr_debug("%s:Register 0x%x NO read access\n", __func__, reg);
		return -EPERM;
	}
	return value;
}

static int aw8896_codec_write(struct snd_soc_component *component,
	unsigned int reg, unsigned int value)
{
	int ret;
	struct aw8896 *aw8896 = snd_soc_component_get_drvdata(component);

	pr_debug("%s:enter ,reg is 0x%x value is 0x%x\n", __func__, reg, value);

	if (aw8896_reg_access[reg]&REG_WR_ACCESS) {
		ret = aw8896_i2c_write(aw8896, reg, value);
		return ret;
	}
	pr_debug("%s: Register 0x%x NO write access\n", __func__, reg);


	return -EPERM;
}



static const struct snd_soc_dapm_widget aw8896_dapm_widgets[] = {
	SND_SOC_DAPM_DAC_E("DAC", "Speaker_Playback", SND_SOC_NOPM, 0, 0, NULL,
				SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_POST_PMU |
				SND_SOC_DAPM_PRE_PMD | SND_SOC_DAPM_POST_PMD),
	SND_SOC_DAPM_OUTPUT("SPK"),
};

static const struct snd_soc_dapm_route aw8896_audio_map[] = {
	/* sink, control, source */
	{"SPK", NULL, "DAC"},
};


static const struct snd_soc_component_driver soc_component_dev_aw8896 = {
	.probe = aw8896_probe,
	.remove = aw8896_remove,
	/*.get_regmap = aw8896_get_regmap,*/
	.read = aw8896_codec_read,
	.write = aw8896_codec_write,
	/*.readable_register= aw8896_readable,*/
	.dapm_widgets = aw8896_dapm_widgets,
	.num_dapm_widgets = ARRAY_SIZE(aw8896_dapm_widgets),
	.dapm_routes = aw8896_audio_map,
	.num_dapm_routes = ARRAY_SIZE(aw8896_audio_map),
};
/*****************************************************
 *
 * regmap
 *
 *****************************************************/
bool aw8896_writeable_register(struct device *dev, unsigned int reg)
{
	/* enable read access for all registers */
	return 1;
}

bool aw8896_readable_register(struct device *dev, unsigned int reg)
{
	/* enable read access for all registers */
	return 1;
}

bool aw8896_volatile_register(struct device *dev, unsigned int reg)
{
	/* enable read access for all registers */
	return 1;
}

static const struct regmap_config aw8896_regmap = {
	.reg_bits = 8,
	.val_bits = 16,

	.max_register = AW8896_MAX_REGISTER,
	.writeable_reg = aw8896_writeable_register,
	.readable_reg = aw8896_readable_register,
	.volatile_reg = aw8896_volatile_register,
	.cache_type = REGCACHE_RBTREE,
};

/******************************************************
 *
 * irq
 *
 ******************************************************/
static void aw8896_interrupt_setup(struct aw8896 *aw8896)
{
	unsigned int reg_val;

	pr_info("%s enter\n", __func__);

	aw8896_i2c_read(aw8896, AW8896_REG_SYSINTM, &reg_val);

	/* i2s status interrupt unmask */
	reg_val &= (~AW8896_BIT_SYSINTM_PLLM);
	reg_val &= (~AW8896_BIT_SYSINTM_CLKM);
	reg_val &= (~AW8896_BIT_SYSINTM_NOCLKM);

	/* uvlo interrupt unmask */
	reg_val &= (~AW8896_BIT_SYSINTM_UVLOM);

	/* over temperature interrupt unmask */
	reg_val &= (~AW8896_BIT_SYSINTM_OTHM);

	/* dsp watchdog status interrupt unmask */
	reg_val &= (~AW8896_BIT_SYSINTM_WDM);

	aw8896_i2c_write(aw8896, AW8896_REG_SYSINTM, reg_val);
}

static void aw8896_interrupt_clear(struct aw8896 *aw8896)
{
	unsigned int reg_val;

	pr_info("%s enter\n", __func__);

	aw8896_i2c_read(aw8896, AW8896_REG_SYSST, &reg_val);
	pr_info("%s: reg SYSST=0x%x\n", __func__, reg_val);

	aw8896_i2c_read(aw8896, AW8896_REG_SYSINT, &reg_val);
	pr_info("%s: reg SYSINT=0x%x\n", __func__, reg_val);

	aw8896_i2c_read(aw8896, AW8896_REG_SYSINTM, &reg_val);
	pr_info("%s: reg SYSINTM=0x%x\n", __func__, reg_val);
}

static irqreturn_t aw8896_irq(int irq, void *data)
{
	struct aw8896 *aw8896 = data;

	pr_info("%s enter\n", __func__);

	aw8896_interrupt_clear(aw8896);

	pr_info("%s exit\n", __func__);

	return IRQ_HANDLED;
}

/*****************************************************
 *
 * device tree
 *
 *****************************************************/
static int aw8896_parse_dt(struct device *dev, struct aw8896 *aw8896,
			struct device_node *np)
{

	aw8896->reset_gpio = of_get_named_gpio(np, "reset-gpio", 0);
	if (aw8896->reset_gpio < 0) {
		dev_err(dev,
		"%s: no reset gpio provided, will not HW reset device\n",
		__func__);

		return -EPERM;
	}

	dev_info(dev, "%s: reset gpio provided ok\n", __func__);

	aw8896->irq_gpio =  of_get_named_gpio(np, "irq-gpio", 0);
	if (aw8896->irq_gpio < 0)
		dev_info(dev, "%s: no irq gpio provided.\n", __func__);
	else
		dev_info(dev, "%s: irq gpio provided ok.\n", __func__);

	return 0;
}

int aw8896_hw_reset(struct aw8896 *aw8896)
{
	pr_info("%s enter\n", __func__);

	if (aw8896 && gpio_is_valid(aw8896->reset_gpio)) {
		gpio_set_value_cansleep(aw8896->reset_gpio, 1);
		usleep_range(1000, 2000);
		gpio_set_value_cansleep(aw8896->reset_gpio, 0);
		usleep_range(1000, 2000);
	} else {
		dev_err(aw8896->dev, "%s:  failed\n", __func__);
	}
	return 0;
}

/*****************************************************
 *
 * check chip id
 *
 *****************************************************/
int aw8896_read_chipid(struct aw8896 *aw8896)
{
	int ret = -1;
	unsigned int cnt = 0;
	unsigned int reg = 0;

	while (cnt < AW_READ_CHIPID_RETRIES) {
		ret = aw8896_i2c_read(aw8896, AW8896_REG_ID, &reg);
		if (ret < 0) {
			dev_err(aw8896->dev,
			"%s: failed to read register AW8896_REG_ID: %d\n",
			__func__, ret);

			return -EIO;
		}
		switch (reg) {
		case 0x0310:
			pr_info("%s aw8896 detected\n", __func__);
			aw8896->flags |= AW8896_FLAG_SKIP_INTERRUPTS;
			aw8896->flags |= AW8896_FLAG_DSP_START_ON_MUTE;
			aw8896->flags |= AW8896_FLAG_DSP_START_ON;
			aw8896->chipid = AW8990_ID;
			pr_info("%s aw8896->flags=0x%x\n",
				__func__, aw8896->flags);
			return 0;
		default:
			pr_info("%s unsupported device revision (0x%x)\n",
				__func__, reg);
			break;
		}
		cnt++;

		usleep_range(AW_READ_CHIPID_RETRY_DELAY * 1000,
			AW_READ_CHIPID_RETRY_DELAY * 1000 + 1000);
	}

	return -EINVAL;
}

/******************************************************
 *
 * sys group attribute: reg
 *
 ******************************************************/
static ssize_t aw8896_reg_store(struct device *dev,
				struct device_attribute *attr,
				const char *buf, size_t count)
{
	struct aw8896 *aw8896 = dev_get_drvdata(dev);

	unsigned int databuf[2] = {0};

	if (sscanf(buf, "%x %x", &databuf[0], &databuf[1]) == 2)
		aw8896_i2c_write(aw8896, databuf[0], databuf[1]);

	return count;
}

static ssize_t aw8896_reg_show(struct device *dev,
			struct device_attribute *attr,
			char *buf)
{
	struct aw8896 *aw8896 = dev_get_drvdata(dev);
	ssize_t len = 0;
	unsigned char i = 0;
	unsigned int reg_val = 0;

	for (i = 0; i < AW8896_REG_MAX; i++) {
		if (!(aw8896_reg_access[i]&REG_RD_ACCESS))
			continue;
		aw8896_i2c_read(aw8896, i, &reg_val);
		len += snprintf(buf+len, PAGE_SIZE-len, "reg:0x%02x=0x%04x\n",
						i, reg_val);
	}
	return len;
}

static ssize_t aw8896_dsp_store(struct device *dev,
				struct device_attribute *attr,
				const char *buf, size_t count)
{
	struct aw8896 *aw8896 = dev_get_drvdata(dev);

	unsigned int databuf[16] = {0};
	unsigned int reg_val = 0;
	int ret;

	ret = kstrtoint(buf, 0, &databuf[0]);
	if (ret < 0) {
		dev_err(dev, "%s, read buf %s failed\n",
			__func__, buf);
		return ret;
	}

	if (databuf[0] == 1) {
		aw8896_i2c_read(aw8896, AW8896_REG_SYSST, &reg_val);
		if (reg_val & AW8896_BIT_SYSST_PLLS) {
			aw8896->init = 0;
			aw8896->dsp_fw_state = AW8896_DSP_FW_FAIL;
			aw8896_smartpa_cfg(aw8896, false);
			aw8896_smartpa_cfg(aw8896, true);
		} else {
			count += snprintf((char *)(buf+count),
				PAGE_SIZE-count,
				"aw8896 plls=%d, no iis signal\n",
				reg_val&AW8896_BIT_SYSST_PLLS);
		}
	} else {
		aw8896_dsp_enable(aw8896, false);
	}

	return count;
}

static ssize_t aw8896_dsp_show(struct device *dev,
			struct device_attribute *attr,
			char *buf)
{
	struct aw8896 *aw8896 = dev_get_drvdata(dev);
	ssize_t len = 0;
	unsigned int i = 0;
	unsigned int addr = 0;
	unsigned int reg_val = 0;

	aw8896_i2c_read(aw8896, AW8896_REG_SYSCTRL, &reg_val);

	if (reg_val & AW8896_BIT_SYSCTRL_DSPBY) {
		len += snprintf(buf+len, PAGE_SIZE-len, "aw8896 dsp bypass\n");
	} else {
		aw8896_i2c_read(aw8896, AW8896_REG_SYSST, &reg_val);
		if (reg_val & AW8896_BIT_SYSST_PLLS) {
			len += snprintf(buf+len, PAGE_SIZE-len,
					"aw8896 dsp working\n");

			aw8896_dsp_enable(aw8896, false);

			len += snprintf(buf+len, PAGE_SIZE-len,
						"aw8896 dsp firmware:\n");
			addr = 0;
			for (i = 0; i < aw8896->dsp_fw_len; i += 2) {
				aw8896_i2c_write(aw8896, AW8896_REG_DSPMADD,
						AW8896_DSP_FW_BASE+addr);
				aw8896_i2c_read(aw8896, AW8896_REG_DSPMDAT,
						&reg_val);
				len += snprintf(buf+len, PAGE_SIZE-len,
						"0x%02x,0x%02x,",
						reg_val&0x00FF, (reg_val>>8));
				if ((i/2+1)%4 == 0)
					len += snprintf(buf+len,
						PAGE_SIZE-len, "\n");
				addr++;
			}
			len += snprintf(buf+len, PAGE_SIZE-len, "\n");

			len += snprintf(buf+len, PAGE_SIZE-len,
						"aw8896 dsp firmware backup:\n");
			addr = 0;
			for (i = 0; i < aw8896->dsp_fw_len; i += 2) {
				aw8896_i2c_write(aw8896, AW8896_REG_DSPMADD,
						AW8896_DSP_FW_BACKUP_BASE+addr);
				aw8896_i2c_read(aw8896, AW8896_REG_DSPMDAT,
						&reg_val);
				len += snprintf(buf+len, PAGE_SIZE-len,
						"0x%02x,0x%02x,",
						reg_val&0x00FF, (reg_val>>8));
				if ((i/2+1)%4 == 0)
					len += snprintf(buf+len,
							PAGE_SIZE-len, "\n");
				addr++;
			}
			len += snprintf(buf+len, PAGE_SIZE-len, "\n");

			aw8896_dsp_enable(aw8896, true);
		} else {
			len += snprintf((char *)(buf+len), PAGE_SIZE-len,
					"aw8896 plls=%d, no iis signal\n",
					reg_val&AW8896_BIT_SYSST_PLLS);
		}
	}

	return len;
}

static ssize_t aw8896_spk_rcv_store(struct device *dev,
				struct device_attribute *attr,
				const char *buf, size_t count)
{
	struct aw8896 *aw8896 = dev_get_drvdata(dev);
	int ret;
	unsigned int databuf[2] = {0};

	ret = kstrtoint(buf, 0, &databuf[0]);
	if (ret < 0) {
		dev_err(dev, "%s, read buf %s failed\n",
			__func__, buf);
		return ret;
	}

	aw8896->spk_rcv_mode = databuf[0];

	return count;
}

static ssize_t aw8896_spk_rcv_show(struct device *dev,
				struct device_attribute *attr,
				char *buf)
{
	struct aw8896 *aw8896 = dev_get_drvdata(dev);
	ssize_t len = 0;

	if (aw8896->spk_rcv_mode == AW8896_SPEAKER_MODE) {
		len += snprintf(buf+len, PAGE_SIZE-len,
				"aw8896 spk_rcv: %d, speaker mode\n",
				aw8896->spk_rcv_mode);
	} else if (aw8896->spk_rcv_mode == AW8896_RECEIVER_MODE) {
		len += snprintf(buf+len, PAGE_SIZE-len,
					"aw8896 spk_rcv: %d, receiver mode\n",
					aw8896->spk_rcv_mode);
	} else {
		len += snprintf(buf+len, PAGE_SIZE-len,
					"aw8896 spk_rcv: %d, unknown mode\n",
					aw8896->spk_rcv_mode);
	}

	return len;
}

static DEVICE_ATTR(reg, 0644, aw8896_reg_show, aw8896_reg_store);
static DEVICE_ATTR(dsp, 0644, aw8896_dsp_show, aw8896_dsp_store);
static DEVICE_ATTR(spk_rcv, 0644,
		aw8896_spk_rcv_show, aw8896_spk_rcv_store);

static struct attribute *aw8896_attributes[] = {
	&dev_attr_reg.attr,
	&dev_attr_dsp.attr,
	&dev_attr_spk_rcv.attr,
	NULL
};

static struct attribute_group aw8896_attribute_group = {
	.attrs = aw8896_attributes
};


/******************************************************
 *
 * i2c driver
 *
 ******************************************************/
int aw8896_i2c_probe(struct i2c_client *i2c, const struct i2c_device_id *id)
{
	struct snd_soc_dai_driver *dai;
	struct aw8896 *aw8896;
	struct device_node *np = i2c->dev.of_node;
	int irq_flags = 0;
	int ret = -1;

	pr_info("%s enter\n", __func__);
	pr_info("%s: driver version %s\n", __func__, AW8896_VERSION);

	if (!i2c_check_functionality(i2c->adapter, I2C_FUNC_I2C)) {
		dev_err(&i2c->dev, "check_functionality failed\n");
		return -EIO;
	}

	aw8896 = devm_kzalloc(&i2c->dev, sizeof(struct aw8896), GFP_KERNEL);
	if (aw8896 == NULL)
		return -ENOMEM;

	aw8896->dev = &i2c->dev;
	aw8896->i2c = i2c;

	/* aw8896 regmap */
	aw8896->regmap = devm_regmap_init_i2c(i2c, &aw8896_regmap);
	if (IS_ERR(aw8896->regmap)) {
		ret = PTR_ERR(aw8896->regmap);
		dev_err(&i2c->dev,
				"%s: failed to allocate register map: %d\n",
				__func__, ret);
		goto err_regmap;
	}

	i2c_set_clientdata(i2c, aw8896);
	mutex_init(&aw8896->lock);

	/* aw8896 rst & int */
	if (np) {
		ret = aw8896_parse_dt(&i2c->dev, aw8896, np);
		if (ret) {
			dev_err(&i2c->dev,
				"%s: failed to parse device tree node\n",
				__func__);
			goto err_parse_dt;
		}
	} else {
		aw8896->reset_gpio = -1;
		aw8896->irq_gpio = -1;
	}

	if (gpio_is_valid(aw8896->reset_gpio)) {
		ret = devm_gpio_request_one(&i2c->dev, aw8896->reset_gpio,
				GPIOF_OUT_INIT_LOW, "aw8896_rst");
		if (ret) {
			dev_err(&i2c->dev,
				"%s: rst request failed\n", __func__);
			goto err_reset_gpio_request;
		}
	}

	if (gpio_is_valid(aw8896->irq_gpio)) {
		ret = devm_gpio_request_one(&i2c->dev, aw8896->irq_gpio,
					GPIOF_DIR_IN, "aw8896_int");
		if (ret) {
			dev_err(&i2c->dev,
				"%s: int request failed\n", __func__);
			goto err_irq_gpio_request;
		}
	}

	/* hardware reset */
	aw8896_hw_reset(aw8896);

	/* aw8896 chip id */
	ret = aw8896_read_chipid(aw8896);
	if (ret < 0) {
		dev_err(&i2c->dev,
				"%s: aw8896_read_chipid failed ret=%d\n",
				__func__, ret);
		goto err_id;
	}

	/* aw8896 device name */
	if (i2c->dev.of_node) {
		dev_set_name(&i2c->dev, "%s", "aw8896_smartpa");
	} else {
		dev_err(&i2c->dev,
				"%s failed to set device name: %d\n",
				__func__, ret);
	}

	/* register codec */
	dai = devm_kzalloc(&i2c->dev, sizeof(aw8896_dai), GFP_KERNEL);
	if (!dai)
		goto err_dai_kzalloc;

	memcpy(dai, aw8896_dai, sizeof(aw8896_dai));
	pr_info("%s dai->name(%s)\n", __func__, dai->name);

	ret = snd_soc_register_component(&i2c->dev, &soc_component_dev_aw8896,
		dai, ARRAY_SIZE(aw8896_dai));
	if (ret < 0) {
		dev_err(&i2c->dev,
				"%s failed to register aw8896: %d\n",
				__func__, ret);
		goto err_register_codec;
	}

	/* aw8896 irq */
	if (gpio_is_valid(aw8896->irq_gpio) &&
			!(aw8896->flags & AW8896_FLAG_SKIP_INTERRUPTS)) {
		/* register irq handler */
		irq_flags = IRQF_TRIGGER_FALLING | IRQF_ONESHOT;
		ret = devm_request_threaded_irq(&i2c->dev,
						gpio_to_irq(aw8896->irq_gpio),
						NULL, aw8896_irq, irq_flags,
						"aw8896", aw8896);
		if (ret != 0) {
			dev_err(&i2c->dev, "failed to request IRQ %d: %d\n",
					gpio_to_irq(aw8896->irq_gpio), ret);
			goto err_irq;
		}
	} else {
		dev_info(&i2c->dev, "%s skipping IRQ registration\n", __func__);
		/* disable feature support if gpio was invalid */
		aw8896->flags |= AW8896_FLAG_SKIP_INTERRUPTS;
	}

	dev_set_drvdata(&i2c->dev, aw8896);
	INIT_WORK(&aw8896->start_work, aw8896_start_work);
	ret = sysfs_create_group(&i2c->dev.kobj, &aw8896_attribute_group);
	if (ret < 0) {
		dev_info(&i2c->dev,
			"%s error creating sysfs attr files\n", __func__);
		goto err_sysfs;
	}
	aw8896->status = AW8896_PW_OFF;
	pr_info("%s probe completed successfully!\n", __func__);

	return 0;

err_sysfs:
	devm_free_irq(&i2c->dev, gpio_to_irq(aw8896->irq_gpio), aw8896);
err_irq:
	snd_soc_unregister_component(&i2c->dev);
err_register_codec:
	devm_kfree(&i2c->dev, dai);
	dai = NULL;
err_dai_kzalloc:
err_id:
	if (gpio_is_valid(aw8896->irq_gpio))
		devm_gpio_free(&i2c->dev, aw8896->irq_gpio);
err_irq_gpio_request:
	if (gpio_is_valid(aw8896->reset_gpio))
		devm_gpio_free(&i2c->dev, aw8896->reset_gpio);
err_reset_gpio_request:
err_parse_dt:
err_regmap:
	devm_kfree(&i2c->dev, aw8896);
	aw8896 = NULL;
	return ret;
}

void aw8896_i2c_shutdown(struct i2c_client *i2c)
{
	struct aw8896 *aw8896 = i2c_get_clientdata(i2c);

	pr_info("%s enter\n", __func__);

	if (aw8896 && gpio_is_valid(aw8896->reset_gpio))
		gpio_set_value_cansleep(aw8896->reset_gpio, 1);
}

int aw8896_i2c_remove(struct i2c_client *i2c)
{
	struct aw8896 *aw8896 = i2c_get_clientdata(i2c);

	pr_info("%s enter\n", __func__);

	if (gpio_to_irq(aw8896->irq_gpio))
		devm_free_irq(&i2c->dev, gpio_to_irq(aw8896->irq_gpio), aw8896);

	snd_soc_unregister_component(&i2c->dev);

	if (gpio_is_valid(aw8896->irq_gpio))
		devm_gpio_free(&i2c->dev, aw8896->irq_gpio);
	if (gpio_is_valid(aw8896->reset_gpio))
		devm_gpio_free(&i2c->dev, aw8896->reset_gpio);

	devm_kfree(&i2c->dev, aw8896);
	aw8896 = NULL;

	return 0;
}

static const struct i2c_device_id aw8896_i2c_id[] = {
	{ AW8896_I2C_NAME, 0 },
	{ }
};
MODULE_DEVICE_TABLE(i2c, aw8896_i2c_id);

static const struct of_device_id aw8896_dt_match[] = {
	{ .compatible = "awinic,aw8896_smartpa" },
	{ },
};

static struct i2c_driver aw8896_i2c_driver = {
	.driver = {
		.name = AW8896_I2C_NAME,
		.owner = THIS_MODULE,
		.of_match_table = of_match_ptr(aw8896_dt_match),
	},
	.probe = aw8896_i2c_probe,
	.remove = aw8896_i2c_remove,
	.shutdown = aw8896_i2c_shutdown,
	.id_table = aw8896_i2c_id,
};


static int __init aw8896_i2c_init(void)
{
	int ret = 0;

	pr_info("aw8896 driver version %s\n", AW8896_VERSION);

	ret = i2c_add_driver(&aw8896_i2c_driver);
	if (ret) {
		pr_err("fail to add aw8896 device into i2c\n");
		return ret;
	}

	return 0;
}
module_init(aw8896_i2c_init);


static void __exit aw8896_i2c_exit(void)
{
	i2c_del_driver(&aw8896_i2c_driver);
}
module_exit(aw8896_i2c_exit);


MODULE_DESCRIPTION("ASoC AW8896 Smart PA Driver");
MODULE_LICENSE("GPL v2");