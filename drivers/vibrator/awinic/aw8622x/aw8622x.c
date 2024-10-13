// SPDX-License-Identifier: GPL-2.0
/*
 * File: aw8622x.c
 *
 * Author: <chelvming@awinic.com>
 *
 * Copyright (c) 2022 AWINIC Technology CO., LTD
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version.
 */
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/i2c.h>
#include <linux/of_gpio.h>
#include <linux/delay.h>
#include <linux/device.h>
#include <linux/firmware.h>
#include <linux/slab.h>
#include <linux/version.h>
#include <linux/input.h>
#include <linux/interrupt.h>
#include <linux/debugfs.h>
#include <linux/miscdevice.h>
#include <linux/syscalls.h>
#include <linux/power_supply.h>
#include <linux/pm_qos.h>
#include <sound/core.h>
#include <sound/pcm.h>
#include <sound/pcm_params.h>
#include <sound/control.h>
#include <sound/soc.h>
#include <linux/vmalloc.h>

#include "aw8622x_reg.h"
#include "aw8622x.h"

#define AW8622X_DRIVER_VERSION ("v0.0.0.8")

struct pm_qos_request aw8622x_pm_qos_req_vb;

static uint32_t aw8622x_f0_cali_data;
module_param(aw8622x_f0_cali_data, uint, 0644);
MODULE_PARM_DESC(aw8622x_f0_cali_data, "aw8622x f0 calibration value");

static uint32_t aw8622x_f0_data;
module_param(aw8622x_f0_data, uint, 0644);
MODULE_PARM_DESC(aw8622x_f0_data, "aw8622x f0 value");

static char *aw8622x_ram_name = "aw8622x_haptic.bin";
static char aw8622x_rtp_name[][AW8622X_RTP_NAME_MAX] = {
	/* 0 */		{"aw8622x_osc_rtp_12K_10s.bin"},
	/* 1 */		{"RTP1_RTP.bin"},
	/* 2 */		{"RTP2_RTP.bin"},
	/* 3 */		{"RTP3_RTP.bin"},
	/* 4 */		{"RTP4_RTP.bin"},
	/* 5 */		{"RTP5_RTP.bin"},
	/* 6 */		{"RTP6_RTP.bin"},
	/* 7 */		{"RTP7_RTP.bin"},
	/* 8 */		{"RTP8_RTP.bin"},
	/* 9 */		{"RTP9_RTP.bin"},
	/* 10 */	{"RTP10_RTP.bin"},
	/* 11 */	{"RTP11_RTP.bin"},
	/* 12 */	{"RTP12_RTP.bin"},
	/* 13 */	{"RTP13_RTP.bin"},
	/* 14 */	{"RTP14_RTP.bin"},
	/* 15 */	{"RTP15_RTP.bin"},
	/* 16 */	{"RTP16_RTP.bin"},
	/* 17 */	{"RTP17_RTP.bin"},
	/* 18 */	{"RESERVED_RTP.bin"},
	/* 19 */	{"RTP19_RTP.bin"},
	/* 20 */	{"RESERVED_RTP.bin"},
	/* 21 */	{"RESERVED_RTP.bin"},
	/* 22 */	{"RESERVED_RTP.bin"},
	/* 23 */	{"RESERVED_RTP.bin"},
	/* 24 */	{"RTP24_RTP.bin"},
	/* 25 */	{"RTP25_RTP.bin"},
	/* 26 */	{"RESERVED_RTP.bin"},
	/* 27 */	{"RESERVED_RTP.bin"},
	/* 28 */	{"RTP28_RTP.bin"},
	/* 29 */	{"RTP29_RTP.bin"},
	/* 30 */	{"RTP30_RTP.bin"},
	/* 31 */	{"RTP31_RTP.bin"},
	/* 32 */	{"RTP32_RTP.bin"},
	/* 33 */	{"RTP33_RTP.bin"},
	/* 34 */	{"RTP34_RTP.bin"},
	/* 35 */	{"RTP35_RTP.bin"},
	/* 36 */	{"RESERVED_RTP.bin"},
	/* 37 */	{"RESERVED_RTP.bin"},
	/* 38 */	{"RESERVED_RTP.bin"},
	/* 39 */	{"RTP39_RTP.bin"},
	/* 40 */	{"RTP40_RTP.bin"},
	/* 41 */	{"RTP41_RTP.bin"},
	/* 42 */	{"RTP42_RTP.bin"},
	/* 43 */	{"RESERVED_RTP.bin"},
	/* 44 */	{"RESERVED_RTP.bin"},
	/* 45 */	{"RESERVED_RTP.bin"},
	/* 46 */	{"RESERVED_RTP.bin"},
	/* 47 */	{"RTP47_RTP.bin"},
	/* 48 */	{"RTP48_RTP.bin"},
	/* 49 */	{"RESERVED_RTP.bin"},
	/* 50 */	{"RTP50_RTP.bin"},
	/* 51 */	{"RTP51_RTP.bin"},
	/* 52 */	{"RTP52_RTP.bin"},
	/* 53 */	{"RTP53_RTP.bin"},
	/* 54 */	{"RTP54_RTP.bin"},
	/* 55 */	{"RTP55_RTP.bin"},
	/* 56 */	{"RTP56_RTP.bin"},
	/* 57 */	{"RTP57_RTP.bin"},
	/* 58 */	{"RTP58_RTP.bin"},
	/* 59 */	{"RTP59_RTP.bin"},
	/* 60 */	{"RTP60_RTP.bin"},
	/* 61 */	{"RTP61_RTP.bin"},
	/* 62 */	{"RTP62_RTP.bin"},
	/* 63 */	{"RTP63_RTP.bin"},
	/* 64 */	{"RTP64_RTP.bin"},
	/* 65 */	{"RTP65_RTP.bin"},
	/* 66 */	{"RTP66_RTP.bin"},
	/* 67 */	{"RTP67_RTP.bin"},
	/* 68 */	{"RTP68_RTP.bin"},
	/* 69 */	{"RTP69_RTP.bin"},
	/* 70 */	{"RTP70_RTP.bin"},
	/* 71 */	{"RTP71_RTP.bin"},
	/* 72 */	{"RTP72_RTP.bin"},
	/* 73 */	{"RTP73_RTP.bin"},
	/* 74 */	{"RTP74_RTP.bin"},
	/* 75 */	{"RTP75_RTP.bin"},
	/* 76 */	{"RTP76_RTP.bin"},
	/* 77 */	{"RTP77_RTP.bin"},
	/* 78 */	{"RTP78_RTP.bin"},
	/* 79 */	{"RTP79_RTP.bin"},
	/* 80 */	{"RTP80_RTP.bin"},
	/* 81 */	{"RESERVED_RTP.bin"},
	/* 82 */	{"RTP82_RTP.bin"},
	/* 83 */	{"RTP83_RTP.bin"},
	/* 84 */	{"RTP84_RTP.bin"},
	/* 85 */	{"RTP85_RTP.bin"},
	/* 86 */	{"RTP86_RTP.bin"},
	/* 87 */	{"RTP87_RTP.bin"},
	/* 88 */	{"RTP88_RTP.bin"},
	/* 89 */	{"RTP89_RTP.bin"},
	/* 90 */	{"RTP90_RTP.bin"},
	/* 91 */	{"RTP91_RTP.bin"},
	/* 92 */	{"RTP92_RTP.bin"},
	/* 93 */	{"RTP93_RTP.bin"},
	/* 94 */	{"RTP94_RTP.bin"},
	/* 95 */	{"RTP95_RTP.bin"},
	/* 96 */	{"RTP96_RTP.bin"},
	/* 97 */	{"RTP97_RTP.bin"},
	/* 98 */	{"RTP98_RTP.bin"},
	/* 99 */	{"RTP99_RTP.bin"},
	/* 100 */	{"RTP100_RTP.bin"},
	/* 101 */	{"RTP101_RTP.bin"},
	/* 102 */	{"RTP102_RTP.bin"},
	/* 103 */	{"RTP103_RTP.bin"},
	/* 104 */	{"RTP104_RTP.bin"},
	/* 105 */	{"RTP105_RTP.bin"},
	/* 106 */	{"RTP106_RTP.bin"},
	/* 107 */	{"RTP107_RTP.bin"},
	/* 108 */	{"RTP108_RTP.bin"},
	/* 109 */	{"RTP109_RTP.bin"},
};

 /*
  * aw8622x i2c write/read
  */
static int aw8622x_i2c_write(struct aw8622x *aw8622x,
			     unsigned char reg_addr, unsigned char reg_data)
{
	int ret = -1;
	unsigned char cnt = 0;

	while (cnt < AW8622X_I2C_RETRIES) {
		ret = i2c_smbus_write_byte_data(aw8622x->i2c, reg_addr,
						reg_data);
		if (ret < 0) {
			aw_err("i2c_write addr=0x%02X, data=0x%02X, cnt=%d, error=%d",
				reg_addr, reg_data, cnt, ret);
		} else {
			break;
		}
		cnt++;
		usleep_range(AW8622X_I2C_RETRY_DELAY * 1000,
			     AW8622X_I2C_RETRY_DELAY * 1000 + 500);
	}
	return ret;
}

static int aw8622x_i2c_read(struct aw8622x *aw8622x,
			    unsigned char reg_addr, unsigned char *reg_data)
{
	int ret = -1;
	unsigned char cnt = 0;

	while (cnt < AW8622X_I2C_RETRIES) {
		ret = i2c_smbus_read_byte_data(aw8622x->i2c, reg_addr);
		if (ret < 0) {
			aw_err("i2c_read addr=0x%02X, cnt=%d error=%d",
				reg_addr, cnt, ret);
		} else {
			*reg_data = ret;
			break;
		}
		cnt++;
		usleep_range(AW8622X_I2C_RETRY_DELAY * 1000,
			     AW8622X_I2C_RETRY_DELAY * 1000 + 500);
	}
	return ret;
}

static int aw8622x_i2c_writes(struct aw8622x *aw8622x,
			      unsigned char reg_addr, unsigned char *buf,
			      unsigned int len)
{
	int ret = -1;
	unsigned char *data = NULL;

	data = kmalloc(len + 1, GFP_KERNEL);
	if (data == NULL)
		return -ENOMEM;

	data[0] = reg_addr;
	memcpy(&data[1], buf, len);
	ret = i2c_master_send(aw8622x->i2c, data, len + 1);
	if (ret < 0)
		aw_err("i2c master send error");
	kfree(data);
	return ret;
}

static int aw8622x_i2c_reads(struct aw8622x *aw8622x,
			     unsigned char reg_addr, unsigned char *buf,
			     unsigned int len)
{
	int ret;
	struct i2c_msg msg[] = {
		[0] = {
			.addr = aw8622x->i2c->addr,
			.flags = 0,
			.len = sizeof(unsigned char),
			.buf = &reg_addr,
		},
		[1] = {
			.addr = aw8622x->i2c->addr,
			.flags = I2C_M_RD,
			.len = len,
			.buf = buf,
		},
	};

	ret = i2c_transfer(aw8622x->i2c->adapter, msg, ARRAY_SIZE(msg));
	if (ret < 0) {
		aw_err("transfer failed.");
		return ret;
	} else if (ret != AW8622X_I2C_READ_MSG_NUM) {
		aw_err("transfer failed(size error).");
		return -ENXIO;
	}

	return ret;
}

/*
 * chip register continuous reading
 */
static ssize_t read_reg_array(struct aw8622x *aw8622x, char *buf, ssize_t len,
		       unsigned char head_reg_addr,
		       unsigned char tail_reg_addr)
{
	int reg_num = 0;
	int i = 0;
	unsigned char reg_array[AW8622X_REG_MAX] = {0};

	reg_num = tail_reg_addr - head_reg_addr + 1;
	aw8622x_i2c_reads(aw8622x, head_reg_addr, reg_array, reg_num);
	for (i = 0 ; i < reg_num; i++) {
		len += snprintf(buf + len, PAGE_SIZE - len,
				"reg:0x%02X=0x%02X\n",
				head_reg_addr + i, reg_array[i]);
	}
	return len;
}

static int aw8622x_i2c_write_bits(struct aw8622x *aw8622x,
				  unsigned char reg_addr, unsigned int mask,
				  unsigned char reg_data)
{
	int ret = -1;
	unsigned char reg_val = 0;

	ret = aw8622x_i2c_read(aw8622x, reg_addr, &reg_val);
	if (ret < 0) {
		aw_err("i2c read error, ret=%d", ret);
		return ret;
	}
	reg_val &= mask;
	reg_val |= (reg_data & (~mask));
	ret = aw8622x_i2c_write(aw8622x, reg_addr, reg_val);
	if (ret < 0) {
		aw_err("i2c write error, ret=%d", ret);
		return ret;
	}
	return 0;
}

/*
 * avoid CPU sleep to prevent RTP data transmission interruption
 */
static void pm_qos_enable(bool enable)
{
#ifdef KERNEL_OVER_5_10
	if (enable) {
		if (!cpu_latency_qos_request_active(&aw8622x_pm_qos_req_vb))
			cpu_latency_qos_add_request(&aw8622x_pm_qos_req_vb, CPU_LATENCY_QOC_VALUE);
	} else {
		if (cpu_latency_qos_request_active(&aw8622x_pm_qos_req_vb))
			cpu_latency_qos_remove_request(&aw8622x_pm_qos_req_vb);
	}
#else
	if (enable) {
		if (!pm_qos_request_active(&aw8622x_pm_qos_req_vb))
			pm_qos_add_request(&aw8622x_pm_qos_req_vb,
					   PM_QOS_CPU_DMA_LATENCY,
					   AW8622X_PM_QOS_VALUE_VB);
	} else {
		if (pm_qos_request_active(&aw8622x_pm_qos_req_vb))
			pm_qos_remove_request(&aw8622x_pm_qos_req_vb);
	}
#endif
}

/*
 * get fifo almost full int status
 */
static unsigned char aw8622x_haptic_rtp_get_fifo_afs(struct aw8622x *aw8622x)
{
	unsigned char ret = 0;
	unsigned char reg_val = 0;

	aw8622x_i2c_read(aw8622x, AW8622X_REG_SYSST, &reg_val);
	reg_val &= AW8622X_BIT_SYSST_FF_AFS;
	ret = reg_val >> 3;
	return ret;
}

/*
 * almost empty int config
 */
static void aw8622x_haptic_set_rtp_aei(struct aw8622x *aw8622x, bool flag)
{
	if (flag) {
		aw8622x_i2c_write_bits(aw8622x, AW8622X_REG_SYSINTM,
				       AW8622X_BIT_SYSINTM_FF_AEM_MASK,
				       AW8622X_BIT_SYSINTM_FF_AEM_ON);
	} else {
		aw8622x_i2c_write_bits(aw8622x, AW8622X_REG_SYSINTM,
				       AW8622X_BIT_SYSINTM_FF_AEM_MASK,
				       AW8622X_BIT_SYSINTM_FF_AEM_OFF);
	}
}

/*
 * parse device tree
 */
static int aw8622x_parse_dt(struct device *dev, struct aw8622x *aw8622x,
			    struct device_node *np)
{
	unsigned int val = 0;

	aw8622x->reset_gpio = of_get_named_gpio(np, "reset-gpio", 0);
	if (aw8622x->reset_gpio >= 0) {
		aw_info("reset gpio provided ok");
	} else {
		aw8622x->reset_gpio = -1;
		aw_err("reset gpio unavailable, hardware reset not supported");
		return -ERANGE;
	}

	aw8622x->irq_gpio = of_get_named_gpio(np, "irq-gpio", 0);
	if (aw8622x->irq_gpio < 0) {
		aw_err("no irq gpio provided.");
		aw8622x->isUsedIntn = false;
	} else {
		aw_info("irq gpio provided ok.");
		aw8622x->isUsedIntn = true;
	}

	val = of_property_read_u32(np, "vib_mode", &aw8622x->dts_info.mode);
	if (val != 0)
		aw_err("aw8622x_vib_mode not found");

	val = of_property_read_u32(np, "vib_f0_pre", &aw8622x->dts_info.f0_ref);
	if (val != 0)
		aw_err("vib_f0_ref not found");

	val = of_property_read_u32(np, "vib_f0_cali_percen",
				   &aw8622x->dts_info.f0_cali_percent);
	if (val != 0)
		aw_err("vib_f0_cali_percent not found");

	val = of_property_read_u32(np, "vib_cont_drv1_lvl",
				   &aw8622x->dts_info.cont_drv1_lvl_dt);
	if (val != 0)
		aw_err("vib_cont_drv1_lvl not found");

	val = of_property_read_u32(np, "vib_lra_vrms",
				   &aw8622x->dts_info.lra_vrms);
	if (val != 0)
		aw_err("vib_lra_vrms not found");

	val = of_property_read_u32(np, "vib_cont_drv1_time",
				   &aw8622x->dts_info.cont_drv1_time_dt);
	if (val != 0)
		aw_err("vib_cont_drv1_time not found");

	val = of_property_read_u32(np, "vib_cont_drv2_time",
				   &aw8622x->dts_info.cont_drv2_time_dt);
	if (val != 0)
		aw_err("vib_cont_drv2_time not found");

	val = of_property_read_u32(np, "vib_cont_brk_gain",
				   &aw8622x->dts_info.cont_brk_gain);
	if (val != 0)
		aw_err("vib_cont_brk_gain not found");

	val = of_property_read_u32(np, "vib_d2s_gain",
				   &aw8622x->dts_info.d2s_gain);
	if (val != 0)
		aw_err("vib_d2s_gain not found");

	val = of_property_read_u32(np, "vib_cont_brk_time",
				   &aw8622x->dts_info.cont_brk_time_dt);
	if (val != 0)
		aw_err("vib_cont_brk_time not found");

	val = of_property_read_u32(np, "vib_cont_track_margin",
				   &aw8622x->dts_info.cont_track_margin);
	if (val != 0)
		aw_err("vib_cont_track_margin not found");
#if defined(CONFIG_AW8622X_SAMSUNG_FEATURE)
	val = of_property_read_u32(np, "samsung,max_level_gain",
				   &aw8622x->dts_info.max_level_gain_val);
	if (val != 0)
		aw_err("Max Level gain Value not found");
#endif //CONFIG_AW8622X_SAMSUNG_FEATURE
	aw8622x->dts_info.is_enabled_auto_brk = of_property_read_bool(np,
						"vib_is_enabled_auto_brk");
	aw_info("aw8622x->info.is_enabled_auto_brk=%d",
		aw8622x->dts_info.is_enabled_auto_brk);

	aw8622x->dts_info.is_enabled_cont_f0 = of_property_read_bool(np,
						"vib_is_enabled_cont_f0");
	aw_info("aw8622x->info.is_enabled_cont_f0=%d",
		aw8622x->dts_info.is_enabled_cont_f0);

	return 0;
}

/*
 * gets the aw8622x drvier data
 */
inline struct aw8622x *aw8622x_get_drvdata(struct device *dev)
{
#if defined(CONFIG_AW8622X_SAMSUNG_FEATURE)
	struct sec_vib_inputff_drvdata *ddata = dev_get_drvdata(dev);
	struct aw8622x *aw8622x = ddata->private_data;
#else
	struct led_classdev *cdev = dev_get_drvdata(dev);
	struct aw8622x *aw8622x = container_of(cdev, struct aw8622x,
					       vib_dev);
#endif

	return aw8622x;
}

/*
 * gets the aw8622x drvier data from the input dev
 */
inline struct aw8622x *aw8622x_input_get_drvdata(struct input_dev *dev)
{
#if defined(CONFIG_AW8622X_SAMSUNG_FEATURE)
	struct sec_vib_inputff_drvdata *ddata = input_get_drvdata(dev);
	struct aw8622x *aw8622x = ddata->private_data;
#else
	struct aw8622x *aw8622x = input_get_drvdata(dev);
#endif

	return aw8622x;
}

#if defined(CONFIG_AW8622X_SAMSUNG_FEATURE)
#include "aw8622x_sep_map_table.h"

static unsigned short aw8622x_sep_index_mapping(struct aw8622x *aw8622x,
		unsigned short sep_index)
{
	if (aw8622x->use_sep_index && sep_index < ARRAY_SIZE(sep_to_aw8622x_index_mapping) && sep_index > 0) {
		aw_info("%s SEP index(%hu -> %hu)\n", __func__, sep_index,
			sep_to_aw8622x_index_mapping[sep_index]);
		return sep_to_aw8622x_index_mapping[sep_index];
	}

	aw_info("%s - No Mapping, so returning 0\n", __func__);
	return 0;
}

static int aw8622x_set_use_sep_index(struct input_dev *dev, bool use_sep_index)
{
	struct aw8622x *aw8622x = aw8622x_input_get_drvdata(dev);

	aw_info("%s use_sep_index=%d +\n", __func__, use_sep_index);

	mutex_lock(&aw8622x->lock);
	aw8622x->use_sep_index = use_sep_index;
	mutex_unlock(&aw8622x->lock);

	aw_info("%s -\n", __func__);
	return 0;
}
#endif //CONFIG_AW8622X_SAMSUNG_FEATURE

/*
 * Write calibration value to register(just update calibration value, not calibration process)
 * WRTIE_ZERO: no calibration
 * F0_CALI   : Calibrate all patterns of waveform to match the LRA's F0
 * OSC_CALI  : Calibrate the frequency of AW86224 to match audio frequency
 * F0_CALI and OSC_CALI can not exit at the same time
 */
static void aw8622x_haptic_upload_lra(struct aw8622x *aw8622x,
				      unsigned int flag)
{
	unsigned char reg_val = 0;

	switch (flag) {
	case AW8622X_WRITE_ZERO:
		aw_info("write zero to trim_lra!");
		reg_val = 0;
		break;
	case AW8622X_F0_CALI:
		aw_info("write f0_cali_data to trim_lra = 0x%02X",
			aw8622x->f0_cali_data);
		reg_val = (unsigned char)(aw8622x->f0_cali_data &
					  AW8622X_BIT_TRIMCFG3_TRIM_LRA);
		break;
	case AW8622X_OSC_CALI:
		aw_info("write osc_cali_data to trim_lra = 0x%02X",
			aw8622x->osc_cali_data);
		reg_val = (unsigned char)(aw8622x->osc_cali_data &
					  AW8622X_BIT_TRIMCFG3_TRIM_LRA);
		break;
	default:
		reg_val = 0;
		break;
	}
	aw8622x_i2c_write_bits(aw8622x, AW8622X_REG_TRIMCFG3,
			       AW8622X_BIT_TRIMCFG3_TRIM_LRA_MASK, reg_val);
}

/*
 * sram size, normally 3k(2k fifo, 1k ram)
 */
static int aw8622x_sram_size(struct aw8622x *aw8622x, int size_flag)
{
	if (size_flag == AW8622X_HAPTIC_SRAM_2K) {
		aw8622x_i2c_write_bits(aw8622x, AW8622X_REG_RTPCFG1,
				       AW8622X_BIT_RTPCFG1_SRAM_SIZE_2K_MASK,
				       AW8622X_BIT_RTPCFG1_SRAM_SIZE_2K_EN);
		aw8622x_i2c_write_bits(aw8622x, AW8622X_REG_RTPCFG1,
				       AW8622X_BIT_RTPCFG1_SRAM_SIZE_1K_MASK,
				       AW8622X_BIT_RTPCFG1_SRAM_SIZE_1K_DIS);
	} else if (size_flag == AW8622X_HAPTIC_SRAM_1K) {
		aw8622x_i2c_write_bits(aw8622x, AW8622X_REG_RTPCFG1,
				       AW8622X_BIT_RTPCFG1_SRAM_SIZE_2K_MASK,
				       AW8622X_BIT_RTPCFG1_SRAM_SIZE_2K_DIS);
		aw8622x_i2c_write_bits(aw8622x, AW8622X_REG_RTPCFG1,
				       AW8622X_BIT_RTPCFG1_SRAM_SIZE_1K_MASK,
				       AW8622X_BIT_RTPCFG1_SRAM_SIZE_1K_EN);
	} else if (size_flag == AW8622X_HAPTIC_SRAM_3K) {
		aw8622x_i2c_write_bits(aw8622x, AW8622X_REG_RTPCFG1,
				       AW8622X_BIT_RTPCFG1_SRAM_SIZE_1K_MASK,
				       AW8622X_BIT_RTPCFG1_SRAM_SIZE_1K_EN);
		aw8622x_i2c_write_bits(aw8622x, AW8622X_REG_RTPCFG1,
				       AW8622X_BIT_RTPCFG1_SRAM_SIZE_2K_MASK,
				       AW8622X_BIT_RTPCFG1_SRAM_SIZE_2K_EN);
	}
	return 0;
}

/*
 * stop the current playback, and chip will auto go
 * back to standby after the current waveform is over
 */
static int aw8622x_haptic_stop(struct aw8622x *aw8622x)
{
	unsigned char cnt = 40;
	unsigned char reg_val = 0;
	bool force_flag = true;

	aw_info("enter");
	aw8622x->play_mode = AW8622X_STANDBY_MODE;
	aw8622x_i2c_write(aw8622x, AW8622X_REG_PLAYCFG4,
			  AW8622X_BIT_PLAYCFG4_STOP_ON);
	while (cnt) {
		aw8622x_i2c_read(aw8622x, AW8622X_REG_GLBRD5, &reg_val);
		if ((reg_val & AW8622X_BIT_GLBRD_STATE_MASK) ==
		     AW8622X_BIT_STATE_STANDBY) {
			cnt = 0;
			force_flag = false;
			aw_info("entered standby! glb_state=0x%02X", reg_val);
		} else {
			cnt--;
		}
		usleep_range(AW8622X_STOP_DELAY_MIN, AW8622X_STOP_DELAY_MAX);
	}

	if (force_flag) {
		aw_err("force to enter standby mode!");
		aw8622x_i2c_write_bits(aw8622x, AW8622X_REG_SYSCTRL2,
				       AW8622X_BIT_SYSCTRL2_STANDBY_MASK,
				       AW8622X_BIT_SYSCTRL2_STANDBY_ON);
		aw8622x_i2c_write_bits(aw8622x, AW8622X_REG_SYSCTRL2,
				       AW8622X_BIT_SYSCTRL2_STANDBY_MASK,
				       AW8622X_BIT_SYSCTRL2_STANDBY_OFF);
	}
	return 0;
}

/*
 * wake up chip clock
 */
static void aw8622x_haptic_raminit(struct aw8622x *aw8622x, bool flag)
{
	if (flag) {
		aw8622x_i2c_write_bits(aw8622x, AW8622X_REG_SYSCTRL1,
				       AW8622X_BIT_SYSCTRL1_RAMINIT_MASK,
				       AW8622X_BIT_SYSCTRL1_RAMINIT_ON);
	} else {
		aw8622x_i2c_write_bits(aw8622x, AW8622X_REG_SYSCTRL1,
				       AW8622X_BIT_SYSCTRL1_RAMINIT_MASK,
				       AW8622X_BIT_SYSCTRL1_RAMINIT_OFF);
	}
}

/*
 * detect the chip supply voltage, mV
 */
static int aw8622x_haptic_get_vbat(struct aw8622x *aw8622x)
{
	unsigned char reg_val = 0;
	unsigned int vbat_code = 0;

	aw8622x_haptic_stop(aw8622x);
	aw8622x_haptic_raminit(aw8622x, true);
	aw8622x_i2c_write_bits(aw8622x, AW8622X_REG_DETCFG2,
			       AW8622X_BIT_DETCFG2_VBAT_GO_MASK,
			       AW8622X_BIT_DETCFG2_VABT_GO_ON);
	usleep_range(AW8622X_VBAT_DELAY_MIN, AW8622X_VBAT_DELAY_MAX);
	aw8622x_i2c_read(aw8622x, AW8622X_REG_DET_VBAT, &reg_val);
	vbat_code = (vbat_code | reg_val) << 2;
	aw8622x_i2c_read(aw8622x, AW8622X_REG_DET_LO, &reg_val);
	vbat_code = vbat_code | ((reg_val & 0x30) >> 4);
	aw8622x->vbat = 6100 * vbat_code / 1024;
	if (aw8622x->vbat > AW8622X_VBAT_MAX) {
		aw8622x->vbat = AW8622X_VBAT_MAX;
		aw_err("vbat max limit=%dmV", aw8622x->vbat);
	}
	if (aw8622x->vbat < AW8622X_VBAT_MIN) {
		aw8622x->vbat = AW8622X_VBAT_MIN;
		aw_err("vbat min limit=%dmV", aw8622x->vbat);
	}
	aw_info("aw8622x->vbat=%dmV, vbat_code=0x%02X", aw8622x->vbat,
		vbat_code);
	aw8622x_haptic_raminit(aw8622x, false);
	return 0;
}

/*
 * read clear interrupt
 */
static void aw8622x_interrupt_clear(struct aw8622x *aw8622x)
{
	unsigned char reg_val = 0;

	aw_info("enter");
	aw8622x_i2c_read(aw8622x, AW8622X_REG_SYSINT, &reg_val);
	aw_info("reg SYSINT=0x%02X", reg_val);
}

/*
 * gain setting for waveform data
 */
static int aw8622x_haptic_set_gain(struct aw8622x *aw8622x, unsigned char gain)
{
	aw8622x_i2c_write(aw8622x, AW8622X_REG_PLAYCFG2, gain);
	return 0;
}

/*
 * software gain compensation
 */
static int aw8622x_ram_vbat_comp(struct aw8622x *aw8622x, bool flag)
{
	int temp_gain = 0;

	if (flag) {
		if (aw8622x->ram_vbat_compensate ==
		    AW8622X_HAPTIC_RAM_VBAT_COMP_ENABLE) {
			aw8622x_haptic_get_vbat(aw8622x);
			temp_gain =
			    aw8622x->level * AW8622X_VBAT_REFER / aw8622x->vbat;
			if (temp_gain >
			    (128 * AW8622X_VBAT_REFER / AW8622X_VBAT_MIN)) {
				temp_gain =
				    128 * AW8622X_VBAT_REFER / AW8622X_VBAT_MIN;
				aw_info("gain limit=%d", temp_gain);
			}
			aw8622x_haptic_set_gain(aw8622x, temp_gain);
		} else {
			aw8622x_haptic_set_gain(aw8622x, aw8622x->level);
		}
	} else
		aw8622x_haptic_set_gain(aw8622x, aw8622x->level);
	return 0;
}

/*
 * chip working mode setting
 */
static int aw8622x_haptic_play_mode(struct aw8622x *aw8622x,
				    unsigned char play_mode)
{
	aw_info("enter");

	switch (play_mode) {
	case AW8622X_STANDBY_MODE:
		aw_info("enter standby mode");
		aw8622x->play_mode = AW8622X_STANDBY_MODE;
		aw8622x_haptic_stop(aw8622x);
		break;
	case AW8622X_RAM_MODE:
		aw_info("enter ram mode");
		aw8622x->play_mode = AW8622X_RAM_MODE;
		aw8622x_i2c_write_bits(aw8622x, AW8622X_REG_PLAYCFG3,
				       AW8622X_BIT_PLAYCFG3_PLAY_MODE_MASK,
				       AW8622X_BIT_PLAYCFG3_PLAY_MODE_RAM);
		break;
	case AW8622X_RAM_LOOP_MODE:
		aw_info("enter ram loop mode");
		aw8622x->play_mode = AW8622X_RAM_LOOP_MODE;
		aw8622x_i2c_write_bits(aw8622x, AW8622X_REG_PLAYCFG3,
				       AW8622X_BIT_PLAYCFG3_PLAY_MODE_MASK,
				       AW8622X_BIT_PLAYCFG3_PLAY_MODE_RAM);
		break;
	case AW8622X_RTP_MODE:
		aw_info("enter rtp mode");
		aw8622x->play_mode = AW8622X_RTP_MODE;
		aw8622x_i2c_write_bits(aw8622x, AW8622X_REG_PLAYCFG3,
				       AW8622X_BIT_PLAYCFG3_PLAY_MODE_MASK,
				       AW8622X_BIT_PLAYCFG3_PLAY_MODE_RTP);
		break;
	case AW8622X_TRIG_MODE:
		aw_info("enter trig mode");
		aw8622x->play_mode = AW8622X_TRIG_MODE;
		aw8622x_i2c_write_bits(aw8622x, AW8622X_REG_PLAYCFG3,
				       AW8622X_BIT_PLAYCFG3_PLAY_MODE_MASK,
				       AW8622X_BIT_PLAYCFG3_PLAY_MODE_RAM);
		break;
	case AW8622X_CONT_MODE:
		aw_info("enter cont mode");
		aw8622x->play_mode = AW8622X_CONT_MODE;
		aw8622x_i2c_write_bits(aw8622x, AW8622X_REG_PLAYCFG3,
				       AW8622X_BIT_PLAYCFG3_PLAY_MODE_MASK,
				       AW8622X_BIT_PLAYCFG3_PLAY_MODE_CONT);
		break;
	default:
		aw_err("play mode %d error", play_mode);
		break;
	}
	return 0;
}

/*
 * start a play
 */
static int aw8622x_haptic_play_go(struct aw8622x *aw8622x, bool flag)
{
	unsigned char reg_val = 0;

	aw_info("enter");
	if (flag == true) {
		aw8622x_i2c_write(aw8622x, AW8622X_REG_PLAYCFG4,
				  AW8622X_BIT_PLAYCFG4_GO_ON);
		usleep_range(2000, 2050);
	} else {
		aw8622x_i2c_write(aw8622x, AW8622X_REG_PLAYCFG4,
				  AW8622X_BIT_PLAYCFG4_STOP_ON);
	}

	aw8622x_i2c_read(aw8622x, AW8622X_REG_GLBRD5, &reg_val);
	return 0;
}

/*
 * set sequence of RAM mode
 */
static int aw8622x_haptic_set_wav_seq(struct aw8622x *aw8622x,
				      unsigned char wav, unsigned char seq)
{
	aw8622x_i2c_write(aw8622x, AW8622X_REG_WAVCFG1 + wav, seq);
	return 0;
}

/*
 * set loop times of each sequence
 */
static int aw8622x_haptic_set_wav_loop(struct aw8622x *aw8622x,
				       unsigned char wav, unsigned char loop)
{
	unsigned char tmp = 0;

	if (wav % 2) {
		tmp = loop << 0;
		aw8622x_i2c_write_bits(aw8622x, AW8622X_REG_WAVCFG9 + (wav / 2),
				       AW8622X_BIT_WAVLOOP_SEQ_EVEN_MASK, tmp);
	} else {
		tmp = loop << 4;
		aw8622x_i2c_write_bits(aw8622x, AW8622X_REG_WAVCFG9 + (wav / 2),
				       AW8622X_BIT_WAVLOOP_SEQ_ODD_MASK, tmp);
	}
	return 0;
}

/*
 * read lra_f0 register and calculate the frequency
 */
static int aw8622x_haptic_read_lra_f0(struct aw8622x *aw8622x)
{
	int ret = 0;
	unsigned char reg_val = 0;
	unsigned int f0_reg = 0;
	unsigned long f0_tmp = 0;

	aw_info("enter");
	ret = aw8622x_i2c_read(aw8622x, AW8622X_REG_CONTRD14, &reg_val);
	f0_reg = (f0_reg | reg_val) << 8;
	ret = aw8622x_i2c_read(aw8622x, AW8622X_REG_CONTRD15, &reg_val);
	f0_reg |= (reg_val << 0);
	if (!f0_reg) {
		aw_err("didn't get lra f0 because f0_reg value is 0!");
		aw8622x->f0 = 0;
		return -ERANGE;
	}
	f0_tmp = 384000 * 10 / f0_reg;
	aw8622x->f0 = (unsigned int)f0_tmp;
	aw_info("lra_f0=%d", aw8622x->f0);
	return 0;
}

/*
 * read cont_f0 register and calculate the frequency
 */
static int aw8622x_haptic_read_cont_f0(struct aw8622x *aw8622x)
{
	int ret = 0;
	unsigned char reg_val = 0;
	unsigned int f0_reg = 0;
	unsigned long f0_tmp = 0;

	aw_info("enter");
	ret = aw8622x_i2c_read(aw8622x, AW8622X_REG_CONTRD16, &reg_val);
	f0_reg = (f0_reg | reg_val) << 8;
	ret = aw8622x_i2c_read(aw8622x, AW8622X_REG_CONTRD17, &reg_val);
	f0_reg |= (reg_val << 0);
	if (!f0_reg) {
		aw_err("didn't get lra f0 because f0_reg value is 0!");
		aw8622x->f0 = 0;
		return -ERANGE;
	}

	f0_tmp = 384000 * 10 / f0_reg;
	aw8622x->f0 = (unsigned int)f0_tmp;
	aw_info("cont_f0=%d", aw8622x->f0);

	return 0;
}

/*
 * get frequency of LRA aftershock bemf
 */
static int aw8622x_haptic_get_f0(struct aw8622x *aw8622x)
{
	int ret = 0;
	unsigned char reg_val = 0;
	unsigned int cnt = 200;
	bool get_f0_flag = false;
	unsigned char brk_en_temp = 0;
	unsigned char cont_drv_width = 0;

	aw_info("enter");
	aw8622x->f0 = aw8622x->dts_info.f0_ref;
	/* enter standby mode */
	aw8622x_haptic_stop(aw8622x);
	/* f0 calibrate work mode */
	aw8622x_haptic_play_mode(aw8622x, AW8622X_CONT_MODE);
	/* enable f0 detect */
	aw8622x_i2c_write_bits(aw8622x, AW8622X_REG_CONTCFG1,
			       AW8622X_BIT_CONTCFG1_EN_F0_DET_MASK,
			       AW8622X_BIT_CONTCFG1_F0_DET_ENABLE);
	/* cont config */
	aw8622x_i2c_write_bits(aw8622x, AW8622X_REG_CONTCFG6,
			       AW8622X_BIT_CONTCFG6_TRACK_EN_MASK,
			       AW8622X_BIT_CONTCFG6_TRACK_ENABLE);
	/* enable auto brake */
	aw8622x_i2c_read(aw8622x, AW8622X_REG_PLAYCFG3, &reg_val);
	brk_en_temp = 0x04 & reg_val;
	aw8622x_i2c_write_bits(aw8622x, AW8622X_REG_PLAYCFG3,
			       AW8622X_BIT_PLAYCFG3_BRK_EN_MASK,
			       AW8622X_BIT_PLAYCFG3_BRK_ENABLE);
	/* f0 driver level */
	aw8622x_i2c_write_bits(aw8622x, AW8622X_REG_CONTCFG6,
			       AW8622X_BIT_CONTCFG6_DRV1_LVL_MASK,
			       aw8622x->dts_info.cont_drv1_lvl_dt);
	aw8622x_i2c_write(aw8622x, AW8622X_REG_CONTCFG7,
			  aw8622x->cont_drv2_lvl);
	/* DRV1_TIME */
	aw8622x_i2c_write(aw8622x, AW8622X_REG_CONTCFG8,
			  aw8622x->dts_info.cont_drv1_time_dt);
	/* DRV2_TIME */
	aw8622x_i2c_write(aw8622x, AW8622X_REG_CONTCFG9,
			  aw8622x->dts_info.cont_drv2_time_dt);
	/* TRACK_MARGIN */
	if (!aw8622x->dts_info.cont_track_margin) {
		aw_err("aw8622x->dts_info.cont_track_margin = 0!");
	} else {
		aw8622x_i2c_write(aw8622x, AW8622X_REG_CONTCFG11,
				  (unsigned char)aw8622x->dts_info.cont_track_margin);
	}
	/* DRV_WIDTH */
	if (aw8622x->dts_info.f0_ref)
		cont_drv_width = AW8622X_DRV_WIDTH_FORMULA(aw8622x->dts_info.f0_ref,
							   aw8622x->dts_info.cont_track_margin,
							   aw8622x->dts_info.cont_brk_gain);
	else
		cont_drv_width = AW8622X_BIT_DRV_WIDTH_DEF_VAL;
	aw_info("cont_drv_width=0x%02X", cont_drv_width);
	if (cont_drv_width < AW8622X_DRV_WIDTH_MIN ||
	    cont_drv_width > AW8622X_DRV_WIDTH_MAX) {
		aw_info("cont_drv_width[%d] is error, restore default vale[0x%02X]\n",
			cont_drv_width, AW8622X_BIT_DRV_WIDTH_DEF_VAL);
		cont_drv_width = AW8622X_BIT_DRV_WIDTH_DEF_VAL;
	}
	aw8622x_i2c_write(aw8622x, AW8622X_REG_CONTCFG3, cont_drv_width);
	/* cont play go */
	aw8622x_haptic_play_go(aw8622x, true);
	/* 300ms */
	while (cnt) {
		aw8622x_i2c_read(aw8622x, AW8622X_REG_GLBRD5, &reg_val);
		if ((reg_val & 0x0f) == 0x00) {
			cnt = 0;
			get_f0_flag = true;
			aw_info("entered standby mode! glb_state=0x%02X",
				reg_val);
		} else {
			cnt--;
		}
		usleep_range(AW8622X_F0_DELAY_MIN, AW8622X_F0_DELAY_MAX);
	}
	if (get_f0_flag) {
		if (aw8622x->dts_info.is_enabled_cont_f0)
			aw8622x_haptic_read_cont_f0(aw8622x);
		else
			aw8622x_haptic_read_lra_f0(aw8622x);
	} else {
		aw_err("enter standby mode failed, stop reading f0!");
	}

	/* restore default config */
	aw8622x_i2c_write_bits(aw8622x, AW8622X_REG_CONTCFG1,
			       AW8622X_BIT_CONTCFG1_EN_F0_DET_MASK,
			       AW8622X_BIT_CONTCFG1_F0_DET_DISABLE);
	/* recover auto break config */
	aw8622x_i2c_write_bits(aw8622x, AW8622X_REG_PLAYCFG3,
			       AW8622X_BIT_PLAYCFG3_BRK_EN_MASK,
			       brk_en_temp);
	return ret;
}

/*
 * write rtp waveform data to fifo until almost full
 */
static int aw8622x_haptic_rtp_init(struct aw8622x *aw8622x)
{
	unsigned int buf_len = 0;
	unsigned char glb_state_val = 0;

	aw_info("enter");
	pm_qos_enable(true);
	aw8622x->rtp_cnt = 0;
	mutex_lock(&aw8622x->rtp_lock);
	while ((!aw8622x_haptic_rtp_get_fifo_afs(aw8622x))
	       && (aw8622x->play_mode == AW8622X_RTP_MODE)
		   &&  !atomic_read(&aw8622x->exit_in_rtp_loop)) {
		aw_info("rtp cnt=%d", aw8622x->rtp_cnt);
		if (!aw8622x->rtp_container) {
			aw_err("aw8622x->rtp_container is null, break!");
			break;
		}
		if (aw8622x->rtp_cnt < (aw8622x->ram.base_addr)) {
			if ((aw8622x->rtp_container->len - aw8622x->rtp_cnt) <
			    (aw8622x->ram.base_addr)) {
				buf_len = aw8622x->rtp_container->len -
					  aw8622x->rtp_cnt;
			} else {
				buf_len = aw8622x->ram.base_addr;
			}
		} else if ((aw8622x->rtp_container->len - aw8622x->rtp_cnt) <
			   (aw8622x->ram.base_addr >> 2)) {
			buf_len = aw8622x->rtp_container->len -
				  aw8622x->rtp_cnt;
		} else {
			buf_len = aw8622x->ram.base_addr >> 2;
		}
		aw_info("buf_len=%d", buf_len);
		aw8622x_i2c_writes(aw8622x, AW8622X_REG_RTPDATA,
				   &aw8622x->rtp_container->data[aw8622x->rtp_cnt], buf_len);
		aw8622x->rtp_cnt += buf_len;
		aw8622x_i2c_read(aw8622x, AW8622X_REG_GLBRD5, &glb_state_val);
		if ((aw8622x->rtp_cnt == aw8622x->rtp_container->len)
		    || ((glb_state_val & 0x0f) == 0x00)) {
			if (aw8622x->rtp_cnt == aw8622x->rtp_container->len) {
				if (aw8622x->rtp_repeat) {
					aw8622x->rtp_cnt = 0;
					aw_info("rtp repeat!");
				} else {
					aw8622x->rtp_cnt = 0;
					pm_qos_enable(false);
					aw_info("rtp load completely! glb_state_val=0x%02x aw8622x->rtp_cnt=%d",
						glb_state_val, aw8622x->rtp_cnt);
					mutex_unlock(&aw8622x->rtp_lock);
					return 0;
				}
			} else {
				aw8622x->rtp_cnt = 0;
				pm_qos_enable(false);
				aw_err("rtp load failed!! glb_state_val=0x%02x aw8622x->rtp_cnt=%d",
					glb_state_val, aw8622x->rtp_cnt);
				mutex_unlock(&aw8622x->rtp_lock);
				return 0;
			}
		}
	}
	mutex_unlock(&aw8622x->rtp_lock);

	if (aw8622x->play_mode == AW8622X_RTP_MODE
	    && !atomic_read(&aw8622x->exit_in_rtp_loop))
		aw8622x_haptic_set_rtp_aei(aw8622x, true);

	aw_info("exit");
	pm_qos_enable(false);
	return 0;
}

/*
 * used to check if fifo is empty during osc calibration
 */
static unsigned char aw8622x_haptic_osc_read_status(struct aw8622x *aw8622x)
{
	unsigned char reg_val = 0;

	aw8622x_i2c_read(aw8622x, AW8622X_REG_SYSST2, &reg_val);
	return reg_val;
}

/*
 * set seq0 infinite loop playback
 */
static int aw8622x_haptic_set_repeat_wav_seq(struct aw8622x *aw8622x,
					     unsigned char seq)
{
	aw_info("repeat wav seq %d", seq);
	aw8622x_haptic_set_wav_seq(aw8622x, 0x00, seq);
	aw8622x_haptic_set_wav_loop(aw8622x, 0x00,
				    AW8622X_BIT_WAVLOOP_INIFINITELY);
	return 0;
}

/*
 * config sampling rate of the waveform
 */
static int aw8622x_haptic_set_pwm(struct aw8622x *aw8622x, unsigned char mode)
{
	switch (mode) {
	case AW8622X_PWM_48K:
		aw8622x_i2c_write_bits(aw8622x, AW8622X_REG_SYSCTRL2,
				       AW8622X_BIT_SYSCTRL2_WAVDAT_MODE_MASK,
				       AW8622X_BIT_SYSCTRL2_RATE_48K);
		break;
	case AW8622X_PWM_24K:
		aw8622x_i2c_write_bits(aw8622x, AW8622X_REG_SYSCTRL2,
				       AW8622X_BIT_SYSCTRL2_WAVDAT_MODE_MASK,
				       AW8622X_BIT_SYSCTRL2_RATE_24K);
		break;
	case AW8622X_PWM_12K:
		aw8622x_i2c_write_bits(aw8622x, AW8622X_REG_SYSCTRL2,
				       AW8622X_BIT_SYSCTRL2_WAVDAT_MODE_MASK,
				       AW8622X_BIT_SYSCTRL2_RATE_12K);
		break;
	default:
		break;
	}
	return 0;
}

/*
 * analysis of the gain value delivered by the upper layer
 * of the input architecture
 */
static int16_t aw8622x_haptic_effect_strength(struct aw8622x *aw8622x)
{
	aw_info("enter, aw8622x->gain = 0x%X", aw8622x->gain);

#if defined(CONFIG_AW8622X_SAMSUNG_FEATURE)
	if (aw8622x->gain >= 10000)
		aw8622x->level = aw8622x->dts_info.max_level_gain_val;
	else
		aw8622x->level = aw8622x->gain * aw8622x->dts_info.max_level_gain_val / 10000;
#else
	if (aw8622x->gain >= 0x7FFF)
		aw8622x->level = 0x80; /* 128 */
	else if (aw8622x->gain <= 0x3FFF)
		aw8622x->level = 0x1E; /* 30 */
	else
		aw8622x->level = (aw8622x->gain - 16383) / 128;
	if (aw8622x->level < 0x1E)
		aw8622x->level = 0x1E; /* 30 */
#endif

	aw_info("aw8622x->level =0x%X", aw8622x->level);
	return 0;
}

/************ RTP F0 Cali Start ****************/
static uint8_t aw8622x_rtp_vib_data_des(int8_t *data, int lens, int cnt_set)
{
	uint8_t ret = 0;
	int i = 0;
	int cnt = 0;

	for (i = 0; i < cnt_set; i++) {
		if ((i + 1) >= lens)
			return ret;
		if (abs(data[i]) <= abs(data[i + 1]))
			cnt++;
	}
	if (cnt == cnt_set)
		ret = 1;

	return ret;
}

static uint8_t aw8622x_rtp_zero_data_des(int8_t *data, int lens, int cnt_set)
{
	uint8_t ret = 0;
	int i = 0;
	int cnt = 0;

	for (i = 0; i < cnt_set; i++) {
		if ((i + 1) >= lens)
			return ret;
		if (data[i] == 0)
			cnt++;
	}
	if (cnt == cnt_set)
		ret = 1;

	return ret;
}

static int aw8622x_rtp_f0_cali(struct aw8622x *aw8622x, uint8_t *src_data, int file_lens)
{
	int ret = 0;
	int i = 0;
	int j = 0;
	int k = 0;
	int index = 0;
	int *start_pos = NULL;
	int *stop_pos = NULL;
	int vib_judg_cnt = 0;
	int zero_judg_cnt = 50;
	int seg_src_len = 0;
	int seg_dst_len = 0;
	int next_start_pos = 0;
	int temp_len = 0;
	int zero_len = 0;
	int8_t data_value = 0;
	int left = 0;
	int right = 0;
	int8_t *seg_src_data = NULL;
	int cnt = 0;
	int src_f0 = aw8622x->dts_info.f0_ref;
	int dst_f0 = aw8622x->f0;

	if (dst_f0 == src_f0) {
		aw_info("No calibration required!");
		return -EINVAL;
	}
	if (dst_f0 == 0) {
		aw_err("dst_f0 invalid %d", dst_f0);
		return -EINVAL;
	}
	if (src_f0 == 0) {
		aw_err("src_f0 invalid %d", src_f0);
		return -EINVAL;
	}
	start_pos = vmalloc(1000 * sizeof(int));
	stop_pos = vmalloc(1000 * sizeof(int));

	for (i = 0; i < file_lens; i++) {
		if ((i + 1) >= file_lens)
			break;

		if (abs(src_data[i + 1]) > 0) {
			ret = aw8622x_rtp_vib_data_des(&src_data[i + 1], file_lens, vib_judg_cnt);
			if (ret) {
				if (index >= 1000) {
					aw_err("index overflow : %d ", index);
					vfree(start_pos);
					vfree(stop_pos);
					return -EFBIG;
				}
				start_pos[index] = i;
				for (j = i + 1; j < file_lens; j++) {
					if ((j + 1) >= file_lens) {
						stop_pos[index] = file_lens - 1;
						break;
					}
					if (src_data[j] == 0) {
						ret = aw8622x_rtp_zero_data_des(&src_data[j + 1],
								file_lens, zero_judg_cnt);
						if (ret) {
							ret = 0;
							for (k = j + 1; k < file_lens; k++) {
								if (abs(src_data[k]) > 0) {
									ret = aw8622x_rtp_vib_data_des(&src_data[k],
											file_lens, vib_judg_cnt);
									if (ret) {
										next_start_pos = k;
										break;
									}
								}
							}
							if (!ret) {
								next_start_pos = j;
								stop_pos[index] = next_start_pos;
								break;
							}
							seg_src_len = j - start_pos[index] + 1;
							seg_dst_len = seg_src_len * src_f0 / dst_f0;
							if (seg_dst_len >= (next_start_pos -
										start_pos[index] + 1))
								j = next_start_pos;
							else {
								stop_pos[index] = j;
								break;
							}
						}
					}
				}

				if (index >= 1)
					zero_len = start_pos[index] - (start_pos[index - 1] + temp_len);
				else
					zero_len = start_pos[index];
				for (j = 0; j < zero_len; j++)
					aw8622x->rtp_container->data[cnt++] = 0;

				seg_src_len = stop_pos[index] - start_pos[index] + 1;
				seg_src_data = &src_data[i];
				seg_dst_len = (seg_src_len * src_f0) / dst_f0;
				temp_len = (dst_f0 / src_f0) >= 1 ? seg_src_len : seg_dst_len;
				{
					for (j = 0; j < seg_dst_len; j++) {
						left = (int)(j * dst_f0 / src_f0);
						right = (int)(j * dst_f0 / src_f0 + 1.0);
						data_value = ((seg_src_data[right] - seg_src_data[left])
								* ((j * dst_f0 / src_f0) - left)) + seg_src_data[left];
						aw8622x->rtp_container->data[cnt++] = data_value;
					}
					for (j = 0; j < temp_len - seg_dst_len; j++)
						aw8622x->rtp_container->data[cnt++] = 0;
				}
				index++;
				i += temp_len;
			}
		}
	}
	if (index == 0) {
		vfree(start_pos);
		vfree(stop_pos);
		aw_err("No vibration data identified!");
		return -EINVAL;
	}
	zero_len = (file_lens - stop_pos[index - 1] - 1) - (temp_len - seg_src_len);
	for (j = 0; j < zero_len; j++)
		aw8622x->rtp_container->data[cnt++] = 0;

	aw8622x->rtp_container->len = cnt;
	vfree(start_pos);
	vfree(stop_pos);

	aw_info("RTP F0 cali succeed. cnt = %d", cnt);

	return 0;
}
/************ RTP F0 Cali End ****************/

/*
 * rtp playback configuration
 */
static void aw8622x_rtp_work_routine(struct work_struct *work)
{
	const struct firmware *rtp_file;
	int ret = -1;
	int effect_id_max = 0;
	unsigned int cnt = 200;
	unsigned char reg_val = 0;
	bool rtp_work_flag = false;
	struct aw8622x *aw8622x = container_of(work, struct aw8622x, rtp_work);
	int rtp_cali_len = 0;

	mutex_lock(&aw8622x->lock);
	aw_info("enter");

	effect_id_max = sizeof(aw8622x_rtp_name) / sizeof(*aw8622x_rtp_name) +
			aw8622x->ram.ram_num - 1;
	if ((aw8622x->effect_id <= aw8622x->ram.ram_num) ||
	    (aw8622x->effect_id > effect_id_max)) {
		mutex_unlock(&aw8622x->lock);
		return;
	}
	aw_info("effect_id=%d state=%d", aw8622x->effect_id, aw8622x->state);

	aw8622x_haptic_upload_lra(aw8622x, aw8622x->rtp_cali_select);
	aw8622x_haptic_set_rtp_aei(aw8622x, false);
	aw8622x_interrupt_clear(aw8622x);
	/* wait for irq to exit */
	atomic_set(&aw8622x->exit_in_rtp_loop, 1);
	while (atomic_read(&aw8622x->is_in_rtp_loop)) {
		aw_info("goint to waiting irq exit");
		ret = wait_event_interruptible(aw8622x->wait_q,
					       atomic_read(&aw8622x->is_in_rtp_loop) == 0);
		aw_info("wakeup ");
		if (ret == -ERESTARTSYS) {
			atomic_set(&aw8622x->exit_in_rtp_loop, 0);
			wake_up_interruptible(&aw8622x->stop_wait_q);
			mutex_unlock(&aw8622x->lock);
			aw_err("wake up by signal return erro");
			return;
		}
	}
	atomic_set(&aw8622x->exit_in_rtp_loop, 0);
	wake_up_interruptible(&aw8622x->stop_wait_q);
	aw8622x_haptic_stop(aw8622x);

	if (aw8622x->state) {
		pm_stay_awake(aw8622x->dev);
		aw8622x->wk_lock_flag = 1;
		if (aw8622x->input_flag)
			aw8622x_haptic_effect_strength(aw8622x);
		aw8622x->rtp_file_num = aw8622x->effect_id - aw8622x->ram.ram_num;
		aw_info("aw8622x->rtp_file_num=%d", aw8622x->rtp_file_num);
		if (aw8622x->rtp_file_num < 0)
			aw8622x->rtp_file_num = 0;
		if (aw8622x->rtp_file_num >
		    ((sizeof(aw8622x_rtp_name) / AW8622X_RTP_NAME_MAX) - 1))
			aw8622x->rtp_file_num = (sizeof(aw8622x_rtp_name) /
						 AW8622X_RTP_NAME_MAX) - 1;

		/* fw loaded */
		ret = request_firmware(&rtp_file,
				       aw8622x_rtp_name[aw8622x->rtp_file_num],
				       aw8622x->dev);
		if (ret < 0) {
			aw_err("failed to read %s",
			       aw8622x_rtp_name[aw8622x->rtp_file_num]);
			if (aw8622x->wk_lock_flag == 1) {
				pm_relax(aw8622x->dev);
				aw8622x->wk_lock_flag = 0;
			}
			mutex_unlock(&aw8622x->lock);
			return;
		}
		aw8622x->rtp_init = 0;
		vfree(aw8622x->rtp_container);
		if (aw8622x->f0 == 0 || aw8622x->dts_info.f0_ref == 0) {
			aw_err("f0 or f0_ref invalid!");
			release_firmware(rtp_file);
			if (aw8622x->wk_lock_flag == 1) {
				pm_relax(aw8622x->dev);
				aw8622x->wk_lock_flag = 0;
			}
			mutex_unlock(&aw8622x->lock);
			return;
		}
		rtp_cali_len = rtp_file->size;
		if (aw8622x->f0 < aw8622x->dts_info.f0_ref)
			rtp_cali_len = rtp_file->size * aw8622x->dts_info.f0_ref / aw8622x->f0;
		aw8622x->rtp_container = vmalloc(rtp_cali_len + sizeof(int));
		if (!aw8622x->rtp_container) {
			release_firmware(rtp_file);
			aw_err("error allocating memory, f0:%d f0_cali=%d", aw8622x->f0, aw8622x->f0_cali_data);
			if (aw8622x->wk_lock_flag == 1) {
				pm_relax(aw8622x->dev);
				aw8622x->wk_lock_flag = 0;
			}
			mutex_unlock(&aw8622x->lock);
			return;
		}
		aw_info("rtp file:[%s] size=%dbytes, rtp_cali_len = %d",
			aw8622x_rtp_name[aw8622x->rtp_file_num],
			rtp_file->size, rtp_cali_len);
		ret = aw8622x_rtp_f0_cali(aw8622x, (uint8_t *)rtp_file->data, rtp_file->size);
		if (ret) {
			aw_err("Play uncalibrated data! f0:%d f0_cali=%d", aw8622x->f0, aw8622x->f0_cali_data);
			aw8622x->rtp_container->len = rtp_file->size;
			memcpy(aw8622x->rtp_container->data, rtp_file->data,
		       rtp_file->size);
		}
		release_firmware(rtp_file);
		aw8622x->rtp_init = 1;
		/* gain */
		aw8622x_ram_vbat_comp(aw8622x, false);
		/* rtp mode config */
		aw8622x_haptic_play_mode(aw8622x, AW8622X_RTP_MODE);
		/* haptic go */
		aw8622x_haptic_play_go(aw8622x, true);
		mutex_unlock(&aw8622x->lock);
		usleep_range(AW8622X_RTP_DELAY_MIN, AW8622X_RTP_DELAY_MAX);
		while (cnt) {
			aw8622x_i2c_read(aw8622x, AW8622X_REG_GLBRD5, &reg_val);
			if ((reg_val & 0x0f) == 0x08) {
				cnt = 0;
				rtp_work_flag = true;
				aw_info("RTP_GO! glb_state=0x08");
			} else {
				cnt--;
				aw_dbg("wait for RTP_GO, glb_state=0x%02X",
					reg_val);
			}
			usleep_range(AW8622X_RTP_DELAY_MIN,
				     AW8622X_RTP_DELAY_MAX);
		}
		if (rtp_work_flag) {
			aw8622x_haptic_rtp_init(aw8622x);
		} else {
			/* enter standby mode */
			aw8622x_haptic_stop(aw8622x);
			aw_err("failed to enter RTP_GO status!");
		}
	} else {
		if (aw8622x->wk_lock_flag == 1) {
			pm_relax(aw8622x->dev);
			aw8622x->wk_lock_flag = 0;
		}
		aw8622x->rtp_cnt = 0;
		aw8622x->rtp_init = 0;
		mutex_unlock(&aw8622x->lock);
	}

}

/*
 * calculation of real playback duration in OSC calibration
 */
static int aw8622x_rtp_osc_calibration(struct aw8622x *aw8622x)
{
	const struct firmware *rtp_file;
	int ret = -1;
	unsigned int buf_len = 0;
	unsigned char osc_int_state = 0;

	aw8622x->rtp_cnt = 0;
	aw8622x->timeval_flags = 1;

	aw_info("enter");
	/* fw loaded */
	ret = request_firmware(&rtp_file, aw8622x_rtp_name[0], aw8622x->dev);
	if (ret < 0) {
		aw_err("failed to read %s", aw8622x_rtp_name[0]);
		return ret;
	}
	/* for irq interrupt during calibrate */
	aw8622x_haptic_stop(aw8622x);
	aw8622x->rtp_init = 0;
	mutex_lock(&aw8622x->rtp_lock);
	vfree(aw8622x->rtp_container);
	aw8622x->rtp_container = vmalloc(rtp_file->size + sizeof(int));
	if (!aw8622x->rtp_container) {
		release_firmware(rtp_file);
		mutex_unlock(&aw8622x->rtp_lock);
		aw_err("error allocating memory");
		return -ENOMEM;
	}
	aw8622x->rtp_container->len = rtp_file->size;
	aw8622x->rtp_len = rtp_file->size;
	aw_info("rtp file:[%s] size=%dbytes", aw8622x_rtp_name[0],
	       aw8622x->rtp_container->len);
	memcpy(aw8622x->rtp_container->data, rtp_file->data, rtp_file->size);
	release_firmware(rtp_file);
	mutex_unlock(&aw8622x->rtp_lock);
	/* gain */
	aw8622x_ram_vbat_comp(aw8622x, false);
	/* rtp mode config */
	aw8622x_haptic_play_mode(aw8622x, AW8622X_RTP_MODE);
	aw8622x_i2c_write_bits(aw8622x, AW8622X_REG_SYSCTRL7,
			       AW8622X_BIT_SYSCTRL7_INT_MODE_MASK,
			       AW8622X_BIT_SYSCTRL7_INT_MODE_EDGE);
	disable_irq(gpio_to_irq(aw8622x->irq_gpio));
	/* haptic go */
	aw8622x_haptic_play_go(aw8622x, true);
	/* require latency of CPU & DMA not more then PM_QOS_VALUE_VB us */
	pm_qos_enable(true);
	while (1) {
		if (!aw8622x_haptic_rtp_get_fifo_afs(aw8622x)) {
			mutex_lock(&aw8622x->rtp_lock);
			if ((aw8622x->rtp_container->len - aw8622x->rtp_cnt) <
			    (aw8622x->ram.base_addr >> 2))
				buf_len = aw8622x->rtp_container->len -
					  aw8622x->rtp_cnt;
			else
				buf_len = (aw8622x->ram.base_addr >> 2);

			if (aw8622x->rtp_cnt != aw8622x->rtp_container->len) {
				if (aw8622x->timeval_flags == 1) {
					ktime_get_real_ts64(&aw8622x->start);
					aw8622x->timeval_flags = 0;
				}
				aw8622x->rtp_update_flag =
				    aw8622x_i2c_writes(aw8622x,
						       AW8622X_REG_RTPDATA,
						       &aw8622x->rtp_container->data[aw8622x->rtp_cnt],
						       buf_len);
				aw8622x->rtp_cnt += buf_len;
			}
			mutex_unlock(&aw8622x->rtp_lock);
		}
		osc_int_state = aw8622x_haptic_osc_read_status(aw8622x);
		if (osc_int_state & AW8622X_BIT_SYSST2_FF_EMPTY) {
			ktime_get_real_ts64(&aw8622x->end);
			aw_info("osc trim playback done aw8622x->rtp_cnt= %d",
				aw8622x->rtp_cnt);
			break;
		}
		ktime_get_real_ts64(&aw8622x->end);
		aw8622x->microsecond =
		    (aw8622x->end.tv_sec - aw8622x->start.tv_sec) * 1000000 +
		    (aw8622x->end.tv_nsec - aw8622x->start.tv_nsec) / 1000000;
		if (aw8622x->microsecond > AW8622X_OSC_CALI_MAX_LENGTH) {
			aw_info("osc trim time out! aw8622x->rtp_cnt %d osc_int_state %02x",
				aw8622x->rtp_cnt, osc_int_state);
			break;
		}
	}
	pm_qos_enable(false);
	enable_irq(gpio_to_irq(aw8622x->irq_gpio));

	aw8622x->microsecond =
	    (aw8622x->end.tv_sec - aw8622x->start.tv_sec) * 1000000 +
	    (aw8622x->end.tv_nsec - aw8622x->start.tv_nsec)  / 1000000;
	/* calibration osc */
	aw_info("awinic_microsecond: %ld", aw8622x->microsecond);
	aw_info("exit");
	return 0;
}

/*
 * OSC calibration value calculation
 */
static int aw8622x_osc_trim_calculation(unsigned long theory_time,
					unsigned long real_time)
{
	unsigned int real_code = 0;
	unsigned int lra_code = 0;
	unsigned int DFT_LRA_TRIM_CODE = 0;
	/* 0.1 percent below no need to calibrate */
	unsigned int osc_cali_threshold = 10;

	aw_info("enter");
	if (theory_time == real_time) {
		aw_info("theory_time == real_time: %ld, no need to calibrate!",
			real_time);
		return 0;
	} else if (theory_time < real_time) {
		if ((real_time - theory_time) > (theory_time / 50)) {
			aw_info("(real_time - theory_time) > (theory_time/50), can't calibrate!");
			return DFT_LRA_TRIM_CODE;
		}

		if ((real_time - theory_time) <
		    (osc_cali_threshold * theory_time / 10000)) {
			aw_info(" real_time: %ld, theory_time: %ld, no need to calibrate!",
				 real_time, theory_time);
			return DFT_LRA_TRIM_CODE;
		}

		real_code = ((real_time - theory_time) * 4000) / theory_time;
		real_code = ((real_code % 10 < 5) ? 0 : 1) + real_code / 10;
		real_code = 32 + real_code;
	} else if (theory_time > real_time) {
		if ((theory_time - real_time) > (theory_time / 50)) {
			aw_info("(theory_time - real_time) > (theory_time / 50), can't calibrate!");
			return DFT_LRA_TRIM_CODE;
		}
		if ((theory_time - real_time) <
		    (osc_cali_threshold * theory_time / 10000)) {
			aw_info("real_time: %ld, theory_time: %ld, no need to calibrate!",
				real_time, theory_time);
			return DFT_LRA_TRIM_CODE;
		}
		real_code = ((theory_time - real_time) * 4000) / theory_time;
		real_code = ((real_code % 10 < 5) ? 0 : 1) + real_code / 10;
		if (real_code < 32)
			real_code = 32 - real_code;
		else
			real_code = 0;
	}
	if (real_code > 31)
		lra_code = real_code - 32;
	else
		lra_code = real_code + 32;
	aw_info("real_time: %ld, theory_time: %ld", real_time, theory_time);
	aw_info("real_code: %02X, trim_lra: 0x%02X", real_code, lra_code);
	return lra_code;
}

/*
 * the d2s_gain value corresponding to the d2s_gain register value
 */
static int aw8622x_select_d2s_gain(unsigned char reg)
{
	int d2s_gain = 0;

	switch (reg) {
	case AW8622X_BIT_SYSCTRL7_D2S_GAIN_1:
		d2s_gain = 1;
		break;
	case AW8622X_BIT_SYSCTRL7_D2S_GAIN_2:
		d2s_gain = 2;
		break;
	case AW8622X_BIT_SYSCTRL7_D2S_GAIN_4:
		d2s_gain = 4;
		break;
	case AW8622X_BIT_SYSCTRL7_D2S_GAIN_5:
		d2s_gain = 5;
		break;
	case AW8622X_BIT_SYSCTRL7_D2S_GAIN_8:
		d2s_gain = 8;
		break;
	case AW8622X_BIT_SYSCTRL7_D2S_GAIN_10:
		d2s_gain = 10;
		break;
	case AW8622X_BIT_SYSCTRL7_D2S_GAIN_20:
		d2s_gain = 20;
		break;
	case AW8622X_BIT_SYSCTRL7_D2S_GAIN_40:
		d2s_gain = 40;
		break;
	default:
		d2s_gain = -1;
		break;
	}
	return d2s_gain;
}

/*
 * detect the motor impedance value, the unit is mohm
 */
static int aw8622x_haptic_get_lra_resistance(struct aw8622x *aw8622x)
{
	unsigned char reg_val = 0;
	unsigned char d2s_gain_temp = 0;
	unsigned char d2s_gain = 0;
	unsigned int lra_code = 0;
	unsigned int lra = 0;

	mutex_lock(&aw8622x->lock);
	aw8622x_haptic_stop(aw8622x);
	aw8622x_i2c_read(aw8622x, AW8622X_REG_SYSCTRL7, &reg_val);
	d2s_gain_temp = 0x07 & reg_val;
	aw8622x_i2c_write_bits(aw8622x, AW8622X_REG_SYSCTRL7,
			       AW8622X_BIT_SYSCTRL7_D2S_GAIN_MASK,
			       aw8622x->dts_info.d2s_gain);
	d2s_gain = aw8622x_select_d2s_gain(aw8622x->dts_info.d2s_gain);
	if (d2s_gain < 0) {
		aw_err("d2s_gain is error");
		mutex_unlock(&aw8622x->lock);
		return -ERANGE;
	}
	aw8622x_haptic_raminit(aw8622x, true);
	/* enter standby mode */
	aw8622x_haptic_stop(aw8622x);
	usleep_range(AW8622X_STOP_DELAY_MIN, AW8622X_STOP_DELAY_MAX);
	aw8622x_i2c_write_bits(aw8622x, AW8622X_REG_SYSCTRL2,
			       AW8622X_BIT_SYSCTRL2_STANDBY_MASK,
			       AW8622X_BIT_SYSCTRL2_STANDBY_OFF);
	aw8622x_i2c_write_bits(aw8622x, AW8622X_REG_DETCFG1,
			       AW8622X_BIT_DETCFG1_RL_OS_MASK,
			       AW8622X_BIT_DETCFG1_RL);
	aw8622x_i2c_write_bits(aw8622x, AW8622X_REG_DETCFG2,
			       AW8622X_BIT_DETCFG2_DIAG_GO_MASK,
			       AW8622X_BIT_DETCFG2_DIAG_GO_ON);
	usleep_range(AW8622X_RL_DELAY_MIN, AW8622X_RL_DELAY_MAX);
	aw8622x_i2c_read(aw8622x, AW8622X_REG_DET_RL, &reg_val);
	lra_code = (lra_code | reg_val) << 2;
	aw8622x_i2c_read(aw8622x, AW8622X_REG_DET_LO, &reg_val);
	lra_code = lra_code | (reg_val & 0x03);
	/* 2num */
	lra = (lra_code * 678 * 100) / (1024 * d2s_gain);
	/* The unit is mohm */
	aw8622x->lra = lra * 10;
	aw8622x_haptic_raminit(aw8622x, false);
	aw8622x_i2c_write_bits(aw8622x, AW8622X_REG_SYSCTRL7,
			       AW8622X_BIT_SYSCTRL7_D2S_GAIN_MASK,
			       d2s_gain_temp);
	mutex_unlock(&aw8622x->lock);
	return 0;
}

#ifdef AW_CHECK_RAM_DATA
/*
 * data comparison
 */
static int aw8622x_check_ram_data(struct aw8622x *aw8622x,
				  unsigned char *cont_data,
				  unsigned char *ram_data,
				  unsigned int len)
{
	int i = 0;

	for (i = 0; i < len; i++) {
		if (ram_data[i] != cont_data[i]) {
			aw_err("check ramdata error, addr=0x%04x, ram_data=0x%02x, file_data=0x%02x",
				i, ram_data[i], cont_data[i]);
			return -ERANGE;
		}
	}
	return 0;
}
#endif

/*
 * update chip sram data, read back and verify
 */
static int aw8622x_container_update(struct aw8622x *aw8622x,
				     struct aw8622x_container *aw8622x_cont)
{
	unsigned char reg_val = 0;
	unsigned int shift = 0;
	unsigned int temp = 0;
	int i = 0;
	int ret = 0;
	int len = 0;
#ifdef AW_CHECK_RAM_DATA
	unsigned char ram_data[AW8622X_RAMDATA_RD_BUFFER_SIZE] = { 0 };
#endif

	aw_info("enter");
	mutex_lock(&aw8622x->lock);
	aw8622x->ram.baseaddr_shift = 2;
	aw8622x->ram.ram_shift = 4;
	/* RAMINIT Enable */
	aw8622x_haptic_raminit(aw8622x, true);
	/* Enter standby mode */
	aw8622x_haptic_stop(aw8622x);
	/* base addr */
	shift = aw8622x->ram.baseaddr_shift;
	aw8622x->ram.base_addr =
	    (unsigned int)((aw8622x_cont->data[0 + shift] << 8) |
			   (aw8622x_cont->data[1 + shift]));

	/* chip SRAM default config 3k */
	aw8622x_sram_size(aw8622x, AW8622X_HAPTIC_SRAM_3K);

	/* ram base addr */
	aw8622x_i2c_write_bits(aw8622x, AW8622X_REG_RTPCFG1, /* ADDRH */
			AW8622X_BIT_RTPCFG1_ADDRH_MASK,
			aw8622x_cont->data[0 + shift]);
	aw8622x_i2c_write(aw8622x, AW8622X_REG_RTPCFG2, /* ADDRL */
			aw8622x_cont->data[1 + shift]);

	/* FIFO almost empty threshold high 4 bits */
	aw8622x_i2c_write_bits(aw8622x, AW8622X_REG_RTPCFG3,
			AW8622X_BIT_RTPCFG3_FIFO_AEH_MASK,
			(unsigned char)
				(((aw8622x->ram.base_addr >> 1) >> 4) & 0xF0));
	/* FIFO almost empty threshold low 8 bits */
	aw8622x_i2c_write(aw8622x, AW8622X_REG_RTPCFG4,
			(unsigned char)
				(((aw8622x->ram.base_addr >> 1) & 0x00FF)));
	/* FIFO almost full threshold high 4 bits */
	aw8622x_i2c_write_bits(aw8622x, AW8622X_REG_RTPCFG3,
				AW8622X_BIT_RTPCFG3_FIFO_AFH_MASK,
				(unsigned char)(((aw8622x->ram.base_addr -
				(aw8622x->ram.base_addr >> 2)) >> 8) & 0x0F));
	/* FIFO almost full threshold low 8 bits */
	aw8622x_i2c_write(aw8622x, AW8622X_REG_RTPCFG5,
			(unsigned char)(((aw8622x->ram.base_addr -
				(aw8622x->ram.base_addr >> 2)) & 0x00FF)));

	aw8622x_i2c_read(aw8622x, AW8622X_REG_RTPCFG3, &reg_val);
	temp = ((reg_val & 0x0f) << 24) | ((reg_val & 0xf0) << 4);
	aw8622x_i2c_read(aw8622x, AW8622X_REG_RTPCFG4, &reg_val);
	temp = temp | reg_val;
	aw_info("almost_empty_threshold=%d", (unsigned short)temp);
	aw8622x_i2c_read(aw8622x, AW8622X_REG_RTPCFG5, &reg_val);
	temp = temp | (reg_val << 16);
	aw_info("almost_full_threshold=%d", temp >> 16);

	shift = aw8622x->ram.baseaddr_shift;

	/* ram data entrance */
	aw8622x_i2c_write_bits(aw8622x, AW8622X_REG_RAMADDRH,
			       AW8622X_BIT_RAMADDRH_MASK,
			       aw8622x_cont->data[0 + shift]);
	aw8622x_i2c_write(aw8622x, AW8622X_REG_RAMADDRL,
			  aw8622x_cont->data[1 + shift]);
	i = aw8622x->ram.ram_shift;
	aw_info("ram_len=%d", aw8622x_cont->len - shift);

	/* write ram data to SRAM */
	while (i < aw8622x_cont->len) {
		if ((aw8622x_cont->len - i) < AW8622X_RAMDATA_WR_BUFFER_SIZE)
			len = aw8622x_cont->len - i;
		else
			len = AW8622X_RAMDATA_WR_BUFFER_SIZE;

		aw8622x_i2c_writes(aw8622x, AW8622X_REG_RAMDATA,
				   &aw8622x_cont->data[i], len);
		i += len;
	}
#ifdef	AW_CHECK_RAM_DATA
	/* ram data read back and verify */
	shift = aw8622x->ram.baseaddr_shift;
	aw8622x_i2c_write_bits(aw8622x, AW8622X_REG_RAMADDRH,
			       AW8622X_BIT_RAMADDRH_MASK,
			       aw8622x_cont->data[0 + shift]);
	aw8622x_i2c_write(aw8622x, AW8622X_REG_RAMADDRL,
			  aw8622x_cont->data[1 + shift]);
	i = aw8622x->ram.ram_shift;
	while (i < aw8622x_cont->len) {
		if ((aw8622x_cont->len - i) < AW8622X_RAMDATA_RD_BUFFER_SIZE)
			len = aw8622x_cont->len - i;
		else
			len = AW8622X_RAMDATA_RD_BUFFER_SIZE;

		aw8622x_i2c_reads(aw8622x, AW8622X_REG_RAMDATA, ram_data, len);
		ret = aw8622x_check_ram_data(aw8622x, &aw8622x_cont->data[i],
					     ram_data, len);
		if (ret < 0)
			break;
		i += len;
	}
	if (ret)
		aw_err("ram data check sum error");
	else
		aw_info("ram data check sum pass");
#endif
	/* RAMINIT Disable */
	aw8622x_haptic_raminit(aw8622x, false);
	mutex_unlock(&aw8622x->lock);
	aw_info("exit");
	return ret;
}

/*
 * get the number of ram waveforms
 */
static int aw8622x_get_ram_num(struct aw8622x *aw8622x)
{
	unsigned char val[3] = { 0 };
	unsigned int first_wave_addr = 0;
	unsigned int base_addr = aw8622x->ram.base_addr;
	int i = 0;

	aw_info("enter");
	if (!aw8622x->ram_init) {
		aw_err("ram init failed, ram_num = 0!");
		return -ERANGE;
	}
	mutex_lock(&aw8622x->lock);
	/* RAMINIT Enable */
	aw8622x_haptic_stop(aw8622x);
	aw8622x_haptic_raminit(aw8622x, true);

	/* ram data entrance */
	val[0] = (unsigned char)(base_addr >> 8);
	val[1] = (unsigned char)(base_addr & 0x00FF);
	aw8622x_i2c_write_bits(aw8622x, AW8622X_REG_RAMADDRH,
			       AW8622X_BIT_RAMADDRH_MASK, val[0]);
	aw8622x_i2c_write(aw8622x, AW8622X_REG_RAMADDRL, val[1]);

	for (i = 0; i < 3; i++)
		aw8622x_i2c_read(aw8622x, AW8622X_REG_RAMDATA, &val[i]);
	first_wave_addr = val[1] << 8 | val[2];

	aw8622x->ram.ram_num =
			(first_wave_addr - aw8622x->ram.base_addr - 1) / 4;
	aw_info("first wave addr = 0x%04X, ram_num=%d",
		first_wave_addr, aw8622x->ram.ram_num);

	/* RAMINIT Disable */
	aw8622x_haptic_raminit(aw8622x, false);
	mutex_unlock(&aw8622x->lock);
	return 0;
}

/*
 * ram bin file loading and update to chip sram
 */
static void aw8622x_ram_loaded(const struct firmware *cont, void *context)
{
	struct aw8622x *aw8622x = context;
	struct aw8622x_container *aw8622x_fw;
	unsigned short check_sum = 0;
	int i = 0;
	int ret = 0;
#ifdef AW_READ_BIN_FLEXBALLY
	static unsigned char load_cont;
	int ram_timer_val = 1000;

	load_cont++;
#endif
	aw_info("enter");
	if (!cont) {
		aw_err("failed to read %s", aw8622x_ram_name);
		release_firmware(cont);
#ifdef AW_READ_BIN_FLEXBALLY
		if (load_cont <= 20) {
			schedule_delayed_work(&aw8622x->ram_work,
					      msecs_to_jiffies(ram_timer_val));
			aw_info("start hrtimer: load_cont=%d", load_cont);
		}
#endif
		return;
	}
	aw_info("loaded %s - size: %zu bytes", aw8622x_ram_name,
		cont ? cont->size : 0);

	/* check sum */
	for (i = 2; i < cont->size; i++)
		check_sum += cont->data[i];
	if (check_sum !=
	    (unsigned short)((cont->data[0] << 8) | (cont->data[1]))) {
		aw_err("check sum err: check_sum=0x%04x", check_sum);
		return;
	}
	aw_info("check sum pass: 0x%04x", check_sum);
	aw8622x->ram.check_sum = check_sum;

	aw8622x_fw = kzalloc(cont->size + sizeof(int), GFP_KERNEL);
	if (!aw8622x_fw) {
		release_firmware(cont);
		aw_err("Error allocating memory");
		return;
	}
	aw8622x_fw->len = cont->size;
	memcpy(aw8622x_fw->data, cont->data, cont->size);
	release_firmware(cont);
	ret = aw8622x_container_update(aw8622x, aw8622x_fw);
	if (ret) {
		aw8622x->ram.len = 0;
		aw_err("ram firmware update failed!");
	} else {
		aw8622x->ram_init = 1;
		aw8622x->ram.len = aw8622x_fw->len;
		aw_info("ram firmware update complete!");
		aw8622x_get_ram_num(aw8622x);
	}
	kfree(aw8622x_fw);
	aw8622x_fw = NULL;

}

/*
 * load ram bin file
 */
static int aw8622x_ram_update(struct aw8622x *aw8622x)
{
	aw8622x->ram_init = 0;
	aw8622x->rtp_init = 0;

#ifdef KERNEL_OVER_5_14
	return request_firmware_nowait(THIS_MODULE, FW_ACTION_UEVENT,
				       aw8622x_ram_name, aw8622x->dev,
				       GFP_KERNEL, aw8622x, aw8622x_ram_loaded);
#else
	return request_firmware_nowait(THIS_MODULE, FW_ACTION_HOTPLUG,
				       aw8622x_ram_name, aw8622x->dev,
				       GFP_KERNEL, aw8622x, aw8622x_ram_loaded);
#endif
}

/*
 * calculate OSC calibration theoretical duration and OSC calibration
 */
static int aw8622x_rtp_trim_lra_calibration(struct aw8622x *aw8622x)
{
	unsigned char reg_val = 0;
	unsigned int fre_val = 0;
	unsigned int theory_time = 0;
	unsigned int lra_trim_code = 0;

	aw8622x_i2c_read(aw8622x, AW8622X_REG_SYSCTRL2, &reg_val);
	fre_val = (reg_val & 0x03) >> 0;

	if (fre_val == 2 || fre_val == 3)
		theory_time = (aw8622x->rtp_len / 12000) * 1000000; /* 12K */
	if (fre_val == 0)
		theory_time = (aw8622x->rtp_len / 24000) * 1000000; /* 24K */
	if (fre_val == 1)
		theory_time = (aw8622x->rtp_len / 48000) * 1000000; /* 48K */

	aw_info("microsecond:%ld  theory_time=%d", aw8622x->microsecond,
		theory_time);
	if (theory_time == 0) {
		aw_info("theory_time is zero, please check rtp_len!!!");
		return -1;
	}

	lra_trim_code = aw8622x_osc_trim_calculation(theory_time,
						     aw8622x->microsecond);
	if (lra_trim_code >= 0) {
		aw8622x->osc_cali_data = lra_trim_code;
		aw8622x_haptic_upload_lra(aw8622x, AW8622X_OSC_CALI);
	}
	return 0;
}

/*
 * ram long vibration timer callback function
 */
static enum hrtimer_restart aw8622x_vibrator_timer_func(struct hrtimer *timer)
{
	struct aw8622x *aw8622x = container_of(timer, struct aw8622x, timer);

	aw_info("enter");
	queue_work(aw8622x->work_queue, &aw8622x->stop_work);

	return HRTIMER_NORESTART;
}

/*
 * set the ram loop mode and start a play
 */
static int aw8622x_haptic_play_repeat_seq(struct aw8622x *aw8622x,
					  unsigned char flag)
{
	aw_info("enter");

	if (flag) {
		aw8622x_haptic_play_mode(aw8622x, AW8622X_RAM_LOOP_MODE);
		aw8622x_haptic_play_go(aw8622x, true);
	}
	return 0;
}

/*
 * pin mode config
 * false: INTN PIN used as TRIG, and RTP can not be used
 * true : INTN PIN used as interrupt pin, and RTP is accessible
 */
static int aw8622x_haptic_trig_config(struct aw8622x *aw8622x)
{

	aw_info("enter");

	if (aw8622x->isUsedIntn == false) {
		aw8622x_i2c_write_bits(aw8622x, AW8622X_REG_SYSCTRL2,
				AW8622X_BIT_SYSCTRL2_INTN_PIN_MASK,
				AW8622X_BIT_SYSCTRL2_TRIG1);
	}

	return 0;
}

/*
 * motor protection
 * The waveform data larger than prlvl and last
 * more than prtime/3k senonds, chip will stop output
 */
static int aw8622x_haptic_swicth_motor_protect_config(struct aw8622x *aw8622x,
						      unsigned char prtime,
						      unsigned char prlvl)
{
	aw_info("enter");
	aw8622x_i2c_write_bits(aw8622x, AW8622X_REG_PWMCFG1,
			       AW8622X_BIT_PWMCFG1_PRC_EN_MASK,
			       AW8622X_BIT_PWMCFG1_PRC_DISABLE);
	if (prlvl != 0) {
		/* Enable protection mode */
		aw8622x_i2c_write(aw8622x, AW8622X_REG_PWMCFG3,
				  (AW8622X_BIT_PWMCFG3_PR_ENABLE |
				  (prlvl & (~AW8622X_BIT_PWMCFG3_PRLVL_MASK))));
		aw8622x_i2c_write(aw8622x, AW8622X_REG_PWMCFG4, prtime);
	} else {
		/* Disable */
		aw8622x_i2c_write(aw8622x, AW8622X_REG_PWMCFG3,
				  (AW8622X_BIT_PWMCFG3_PR_DISABLE |
				  (prlvl & (~AW8622X_BIT_PWMCFG3_PRLVL_MASK))));
		aw8622x_i2c_write(aw8622x, AW8622X_REG_PWMCFG4, prtime);
	}
	return 0;
}

/*
 * calibrate all patterns of waveform to match the LRA's F0,
 * and update it to the chip register.
 * The calibration value changes the OSC frequency of the chip.
 */
static int aw8622x_haptic_f0_calibration(struct aw8622x *aw8622x)
{
	int ret = 0;
	unsigned char reg_val = 0;
	unsigned int f0_limit = 0;
	char f0_cali_lra = 0;
	int f0_cali_step = 0;
	unsigned int f0_cali_min = aw8622x->dts_info.f0_ref *
				(100 - aw8622x->dts_info.f0_cali_percent) / 100;
	unsigned int f0_cali_max =  aw8622x->dts_info.f0_ref *
				(100 + aw8622x->dts_info.f0_cali_percent) / 100;

	aw_info("enter");

	if (aw8622x_haptic_get_f0(aw8622x)) {
		aw_err("get f0 error, user defafult f0");
	} else {
		/* max and min limit */
		f0_limit = aw8622x->f0;
		aw_info("f0_ref=%d, f0_cali_min=%d, f0_cali_max=%d, f0=%d",
			aw8622x->dts_info.f0_ref, f0_cali_min, f0_cali_max,
			aw8622x->f0);

		if ((aw8622x->f0 < f0_cali_min) || aw8622x->f0 > f0_cali_max) {
			aw_err("f0 calibration out of range=%d", aw8622x->f0);
			f0_limit = aw8622x->dts_info.f0_ref;
			return -ERANGE;
		}
		aw_info("f0_limit=%d", (int)f0_limit);
		/* calculate cali step */
		f0_cali_step = 100000 * ((int)f0_limit -
					 (int)aw8622x->dts_info.f0_ref) /
					((int)f0_limit * 24);
		aw_info("f0_cali_step=%d", f0_cali_step);
		if (f0_cali_step >= 0) { /* f0_cali_step >= 0 */
			if (f0_cali_step % 10 >= 5)
				f0_cali_step = 32 + (f0_cali_step / 10 + 1);
			else
				f0_cali_step = 32 + f0_cali_step / 10;
		} else {	/* f0_cali_step < 0 */
			if (f0_cali_step % 10 <= -5)
				f0_cali_step = 32 + (f0_cali_step / 10 - 1);
			else
				f0_cali_step = 32 + f0_cali_step / 10;
		}
		if (f0_cali_step > 31)
			f0_cali_lra = (char)f0_cali_step - 32;
		else
			f0_cali_lra = (char)f0_cali_step + 32;
		/* update cali data */
		aw8622x->f0_cali_data = (int)f0_cali_lra;
		aw8622x_haptic_upload_lra(aw8622x, AW8622X_F0_CALI);

		/* read back */
		aw8622x_i2c_read(aw8622x, AW8622X_REG_TRIMCFG3, &reg_val);
		aw_info("final trim_lra=0x%02x", reg_val);
	}
	/* restore standby work mode */
	aw8622x_haptic_stop(aw8622x);
	return ret;
}

/*
 * cont mode long vibration
 */
static int aw8622x_haptic_cont_config(struct aw8622x *aw8622x)
{
	aw_info("enter");

	/* work mode */
	aw8622x_haptic_play_mode(aw8622x, AW8622X_CONT_MODE);
	/* Fix cont waveform width to the register DRV_WIDTH */
	aw8622x_i2c_write_bits(aw8622x, AW8622X_REG_CONTCFG6,
			       AW8622X_BIT_CONTCFG6_TRACK_EN_MASK,
			       AW8622X_BIT_CONTCFG6_TRACK_ENABLE);
	/* f0 driver level */
	aw8622x_i2c_write_bits(aw8622x, AW8622X_REG_CONTCFG6,
			       AW8622X_BIT_CONTCFG6_DRV1_LVL_MASK,
			       aw8622x->cont_drv1_lvl);
	aw8622x_i2c_write(aw8622x, AW8622X_REG_CONTCFG7,
			  aw8622x->cont_drv2_lvl);
	/* Half cycle number of RMS drive process */
	aw8622x_i2c_write(aw8622x, AW8622X_REG_CONTCFG9,
			  AW8622X_CONTCFG9_INIT_VAL);
	/* cont play go */
	aw8622x_haptic_play_go(aw8622x, true);
	return 0;
}

/*
 * set the ram mode and start a play
 */
static int aw8622x_haptic_play_wav_seq(struct aw8622x *aw8622x,
				       unsigned char flag)
{
	aw_info("enter");
	if (flag) {
		aw8622x_haptic_play_mode(aw8622x, AW8622X_RAM_MODE);
		aw8622x_haptic_play_go(aw8622x, true);
	}
	return 0;
}

static enum led_brightness aw8622x_haptic_brightness_get(struct led_classdev
							 *cdev)
{
	struct aw8622x *aw8622x = container_of(cdev, struct aw8622x, vib_dev);

	return aw8622x->amplitude;
}

/*
 * leds schema registration callback function,
 * used to start and stop vibration in RAM mode
 */
static void aw8622x_haptic_brightness_set(struct led_classdev *cdev,
					  enum led_brightness level)
{
	struct aw8622x *aw8622x = container_of(cdev, struct aw8622x, vib_dev);

	aw_info("enter");
	if (!aw8622x->ram_init) {
		aw_err("ram init failed, not allow to play!");
		return;
	}
	if (aw8622x->ram_update_flag < 0)
		return;

	aw8622x->amplitude = level;

	mutex_lock(&aw8622x->lock);
	aw8622x_haptic_stop(aw8622x);
	if (aw8622x->amplitude > 0) {
		aw8622x_haptic_upload_lra(aw8622x, AW8622X_F0_CALI);
		aw8622x_ram_vbat_comp(aw8622x, false);
		aw8622x_haptic_set_wav_seq(aw8622x, 0x01, 0x00);
		aw8622x_haptic_set_wav_loop(aw8622x, 0x00, 0x00);
		aw8622x_haptic_play_wav_seq(aw8622x, aw8622x->amplitude);
	}
	mutex_unlock(&aw8622x->lock);
}

/*
 * start a ram or ram_loop play
 */
static void aw8622x_ram_play(struct aw8622x *aw8622x, unsigned int type)
{
	aw_info("index=%d", aw8622x->index);

	if (type == AW8622X_RAM_LOOP_MODE)
		aw8622x_haptic_set_repeat_wav_seq(aw8622x, aw8622x->index);

	if (type == AW8622X_RAM_MODE) {
		aw8622x_haptic_set_wav_seq(aw8622x, 0x00, aw8622x->index);
		aw8622x_haptic_set_wav_seq(aw8622x, 0x01, 0x00);
		aw8622x_haptic_set_wav_loop(aw8622x, 0x00, 0x00);
	}
	aw8622x_haptic_play_mode(aw8622x, type);
	aw8622x_haptic_play_go(aw8622x, true);
}

/*
 * cont mode vibration
 */
static ssize_t cont_store(struct device *dev,
				  struct device_attribute *attr,
				  const char *buf, size_t count)
{
	struct aw8622x *aw8622x = aw8622x_get_drvdata(dev);

	unsigned int val = 0;
	int rc = 0;

	rc = kstrtouint(buf, 0, &val);
	if (rc < 0)
		return rc;

	aw8622x_haptic_stop(aw8622x);
	if (val)
		aw8622x_haptic_cont_config(aw8622x);
	return count;
}
static DEVICE_ATTR_WO(cont);

/*
 * get real lra_f0
 */
static ssize_t f0_show(struct device *dev,
			       struct device_attribute *attr, char *buf)
{
	struct aw8622x *aw8622x = aw8622x_get_drvdata(dev);
	ssize_t len = 0;
	unsigned char reg = 0;

	mutex_lock(&aw8622x->lock);
	aw8622x_i2c_read(aw8622x, AW8622X_REG_TRIMCFG3, &reg);
	aw8622x_haptic_upload_lra(aw8622x, AW8622X_WRITE_ZERO);
	aw8622x_haptic_get_f0(aw8622x);
	aw8622x_i2c_write(aw8622x, AW8622X_REG_TRIMCFG3, reg);
	mutex_unlock(&aw8622x->lock);
	len += snprintf(buf + len, PAGE_SIZE - len, "%d\n", aw8622x->f0);
	return len;
}

static ssize_t f0_store(struct device *dev,
				struct device_attribute *attr, const char *buf,
				size_t count)
{
	unsigned int val = 0;
	int rc = 0;

	rc = kstrtouint(buf, 0, &val);
	if (rc < 0)
		return rc;

	return count;
}
static DEVICE_ATTR_RW(f0);

static ssize_t reg_show(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	struct aw8622x *aw8622x = aw8622x_get_drvdata(dev);
	ssize_t len = 0;

	mutex_lock(&aw8622x->lock);
	len = read_reg_array(aw8622x, buf, len, AW8622X_REG_ID,
			     AW8622X_REG_RTPDATA - 1);
	if (!len)
		return len;
	len = read_reg_array(aw8622x, buf, len, AW8622X_REG_RTPDATA + 1,
			     AW8622X_REG_RAMDATA - 1);
	if (!len)
		return len;
	len = read_reg_array(aw8622x, buf, len, AW8622X_REG_RAMDATA + 1,
			     AW8622X_REG_ANACFG8);
	mutex_unlock(&aw8622x->lock);
	return len;
}

static ssize_t reg_store(struct device *dev,
				 struct device_attribute *attr, const char *buf,
				 size_t count)
{
	struct aw8622x *aw8622x = aw8622x_get_drvdata(dev);
	unsigned int databuf[2] = { 0, 0 };

	mutex_lock(&aw8622x->lock);
	if (sscanf(buf, "%x %x", &databuf[0], &databuf[1]) == 2) {
		aw8622x_i2c_write(aw8622x, (unsigned char)databuf[0],
				  (unsigned char)databuf[1]);
	}
	mutex_unlock(&aw8622x->lock);

	return count;
}
static DEVICE_ATTR_RW(reg);

static ssize_t duration_show(struct device *dev,
				     struct device_attribute *attr, char *buf)
{
	struct aw8622x *aw8622x = aw8622x_get_drvdata(dev);
	ktime_t time_rem;
	unsigned long long time_ms = 0;

	if (hrtimer_active(&aw8622x->timer)) {
		time_rem = hrtimer_get_remaining(&aw8622x->timer);
		time_ms = ktime_to_ms(time_rem);
	}
	return snprintf(buf, PAGE_SIZE, "%lld\n", time_ms);
}

static ssize_t duration_store(struct device *dev,
				      struct device_attribute *attr,
				      const char *buf, size_t count)
{
	struct aw8622x *aw8622x = aw8622x_get_drvdata(dev);
	unsigned int val = 0;
	int rc = 0;

	rc = kstrtouint(buf, 0, &val);
	if (rc < 0)
		return rc;
	/* setting 0 on duration is NOP for now */
	if (val <= 0)
		return count;
	mutex_lock(&aw8622x->lock);
	aw8622x->duration = val;
	mutex_unlock(&aw8622x->lock);
	return count;
}
static DEVICE_ATTR_RW(duration);

static ssize_t activate_show(struct device *dev,
				     struct device_attribute *attr, char *buf)
{
	struct aw8622x *aw8622x = aw8622x_get_drvdata(dev);

	/* For now nothing to show */
	return snprintf(buf, PAGE_SIZE, "activate = %d\n", aw8622x->state);
}

static ssize_t activate_store(struct device *dev,
				      struct device_attribute *attr,
				      const char *buf, size_t count)
{
	struct aw8622x *aw8622x = aw8622x_get_drvdata(dev);
	unsigned int val = 0;
	int rc = 0;

	if (!aw8622x->ram_init) {
		aw_err("ram init failed, not allow to play!");
		return count;
	}
	rc = kstrtouint(buf, 0, &val);
	if (rc < 0)
		return rc;
	if (val < 0)
		return count;
	aw_info("value=%d", val);

	mutex_lock(&aw8622x->lock);
	hrtimer_cancel(&aw8622x->timer);
	aw8622x->state = val;
	mutex_unlock(&aw8622x->lock);
	schedule_work(&aw8622x->vibrator_work);
	return count;
}
static DEVICE_ATTR_RW(activate);

static ssize_t seq_show(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	struct aw8622x *aw8622x = aw8622x_get_drvdata(dev);
	size_t count = 0;
	unsigned char i = 0;
	unsigned char reg_val = 0;

	mutex_lock(&aw8622x->lock);
	for (i = 0; i < AW8622X_SEQUENCER_SIZE; i++) {
		aw8622x_i2c_read(aw8622x, AW8622X_REG_WAVCFG1 + i, &reg_val);
		count += snprintf(buf + count, PAGE_SIZE - count,
				  "seq%d: 0x%02x\n", i + 1, reg_val);
		aw8622x->seq[i] |= reg_val;
	}
	mutex_unlock(&aw8622x->lock);
	return count;
}

static ssize_t seq_store(struct device *dev,
				 struct device_attribute *attr, const char *buf,
				 size_t count)
{
	struct aw8622x *aw8622x = aw8622x_get_drvdata(dev);
	unsigned int databuf[2] = { 0, 0 };

	if (sscanf(buf, "%x %x", &databuf[0], &databuf[1]) == 2) {
		if (databuf[0] > AW8622X_SEQUENCER_SIZE ||
		    databuf[1] > aw8622x->ram.ram_num) {
			aw_err("input value out of range");
			return count;
		}
		aw_info("seq%d=0x%02X", databuf[0], databuf[1]);
		mutex_lock(&aw8622x->lock);
		aw8622x->seq[databuf[0]] = (unsigned char)databuf[1];
		aw8622x_haptic_set_wav_seq(aw8622x, (unsigned char)databuf[0],
					   aw8622x->seq[databuf[0]]);
		mutex_unlock(&aw8622x->lock);
	}
	return count;
}
static DEVICE_ATTR_RW(seq);

static ssize_t loop_show(struct device *dev,
				 struct device_attribute *attr, char *buf)
{
	struct aw8622x *aw8622x = aw8622x_get_drvdata(dev);
	size_t count = 0;
	unsigned char i = 0;
	unsigned char reg_val = 0;

	for (i = 0; i < AW8622X_SEQUENCER_LOOP_SIZE; i++) {
		aw8622x_i2c_read(aw8622x, AW8622X_REG_WAVCFG9 + i, &reg_val);
		aw8622x->loop[i * 2 + 0] = (reg_val >> 4) & 0x0F;
		aw8622x->loop[i * 2 + 1] = (reg_val >> 0) & 0x0F;

		count += snprintf(buf + count, PAGE_SIZE - count,
				  "seq%d_loop = 0x%02x\n", i * 2 + 1,
				  aw8622x->loop[i * 2 + 0]);
		count += snprintf(buf + count, PAGE_SIZE - count,
				  "seq%d_loop = 0x%02x\n", i * 2 + 2,
				  aw8622x->loop[i * 2 + 1]);
	}
	return count;
}

static ssize_t loop_store(struct device *dev,
				  struct device_attribute *attr,
				  const char *buf, size_t count)
{
	struct aw8622x *aw8622x = aw8622x_get_drvdata(dev);
	unsigned int databuf[2] = { 0, 0 };

	if (sscanf(buf, "%x %x", &databuf[0], &databuf[1]) == 2) {
		aw_info("seq%d loop=0x%02X", databuf[0], databuf[1]);
		mutex_lock(&aw8622x->lock);
		aw8622x->loop[databuf[0]] = (unsigned char)databuf[1];
		aw8622x_haptic_set_wav_loop(aw8622x, (unsigned char)databuf[0],
					    aw8622x->loop[databuf[0]]);
		mutex_unlock(&aw8622x->lock);
	}
	return count;
}
static DEVICE_ATTR_RW(loop);

static ssize_t rtp_show(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	struct aw8622x *aw8622x = aw8622x_get_drvdata(dev);
	ssize_t len = 0;

	len += snprintf(buf + len, PAGE_SIZE - len,
			"rtp_cnt = %d\n", aw8622x->rtp_cnt);
	return len;
}

static ssize_t rtp_store(struct device *dev,
				 struct device_attribute *attr, const char *buf,
				 size_t count)
{
	struct aw8622x *aw8622x = aw8622x_get_drvdata(dev);
	unsigned int val = 0;
	unsigned int rtp_num_max = sizeof(aw8622x_rtp_name) /
				   AW8622X_RTP_NAME_MAX;
	int rc = 0;

	rc = kstrtouint(buf, 0, &val);
	if (rc < 0) {
		aw_err("kstrtouint fail");
		return rc;
	}
	mutex_lock(&aw8622x->lock);
	aw8622x_haptic_stop(aw8622x);
	aw8622x_haptic_set_rtp_aei(aw8622x, false);
	aw8622x_interrupt_clear(aw8622x);
	if (val > 0 && val < rtp_num_max) {
		aw8622x->effect_id = val + aw8622x->ram.ram_num;
		aw8622x->state = 1;
		schedule_work(&aw8622x->rtp_work);
	} else if (val == 0) {
		aw_info("rtp play stop");
	} else {
		aw_err("rtp_file_num over max value");
	}
	mutex_unlock(&aw8622x->lock);
	return count;
}
static DEVICE_ATTR_RW(rtp);

static ssize_t state_show(struct device *dev,
				  struct device_attribute *attr, char *buf)
{
	struct aw8622x *aw8622x = aw8622x_get_drvdata(dev);

	return snprintf(buf, PAGE_SIZE, "%d\n", aw8622x->state);
}

static ssize_t state_store(struct device *dev,
				   struct device_attribute *attr,
				   const char *buf, size_t count)
{

	return count;
}
static DEVICE_ATTR_RW(state);

static ssize_t activate_mode_show(struct device *dev,
					  struct device_attribute *attr,
					  char *buf)
{
	struct aw8622x *aw8622x = aw8622x_get_drvdata(dev);

	return snprintf(buf, PAGE_SIZE, "%d\n",
			aw8622x->activate_mode);
}

static ssize_t activate_mode_store(struct device *dev,
					   struct device_attribute *attr,
					   const char *buf, size_t count)
{
	struct aw8622x *aw8622x = aw8622x_get_drvdata(dev);
	unsigned int val = 0;
	int rc = 0;

	rc = kstrtouint(buf, 0, &val);
	if (rc < 0)
		return rc;
	mutex_lock(&aw8622x->lock);
	aw8622x->activate_mode = val;
	mutex_unlock(&aw8622x->lock);
	return count;
}
static DEVICE_ATTR_RW(activate_mode);

static ssize_t index_show(struct device *dev,
				  struct device_attribute *attr, char *buf)
{
	struct aw8622x *aw8622x = aw8622x_get_drvdata(dev);
	unsigned char reg_val = 0;

	mutex_lock(&aw8622x->lock);
	aw8622x_i2c_read(aw8622x, AW8622X_REG_WAVCFG1, &reg_val);
	aw8622x->index = reg_val;
	mutex_unlock(&aw8622x->lock);
	return snprintf(buf, PAGE_SIZE, "index = %d\n", aw8622x->index);
}

static ssize_t index_store(struct device *dev,
				   struct device_attribute *attr,
				   const char *buf, size_t count)
{
	struct aw8622x *aw8622x = aw8622x_get_drvdata(dev);
	unsigned int val = 0;
	int rc = 0;

	rc = kstrtouint(buf, 0, &val);
	if (rc < 0)
		return rc;
	aw_info("value=%d", val);
	if (val > aw8622x->ram.ram_num) {
		aw_err("input value out of range!");
		return count;
	}
	mutex_lock(&aw8622x->lock);
	aw8622x->input_flag = false;
	aw8622x->index = val;
	aw8622x_haptic_set_repeat_wav_seq(aw8622x, aw8622x->index);
	mutex_unlock(&aw8622x->lock);
	return count;
}
static DEVICE_ATTR_RW(index);

static ssize_t osc_cali_show(struct device *dev,
				     struct device_attribute *attr, char *buf)
{
	struct aw8622x *aw8622x = aw8622x_get_drvdata(dev);
	ssize_t len = 0;

	len += snprintf(buf + len, PAGE_SIZE - len, "osc_cali_data = 0x%02X\n",
			aw8622x->osc_cali_data);

	return len;
}

static ssize_t osc_cali_store(struct device *dev,
				      struct device_attribute *attr,
				      const char *buf, size_t count)
{
	struct aw8622x *aw8622x = aw8622x_get_drvdata(dev);
	unsigned int val = 0;
	int rc = 0;

	rc = kstrtouint(buf, 0, &val);
	if (rc < 0)
		return count;
	mutex_lock(&aw8622x->lock);
	aw8622x->osc_cali_run = 1;
	if (val == 1) {
		aw8622x_haptic_upload_lra(aw8622x, AW8622X_WRITE_ZERO);
		aw8622x_rtp_osc_calibration(aw8622x);
		aw8622x_rtp_trim_lra_calibration(aw8622x);
	} else if (val == 2) {
		aw8622x_haptic_upload_lra(aw8622x, AW8622X_OSC_CALI);
		aw8622x_rtp_osc_calibration(aw8622x);
	} else {
		aw_err("input value out of range");
	}
	aw8622x->osc_cali_run = 0;
	/* osc calibration flag end, other behaviors are permitted */
	mutex_unlock(&aw8622x->lock);

	return count;
}
static DEVICE_ATTR_RW(osc_cali);

static ssize_t gain_show(struct device *dev,
				 struct device_attribute *attr, char *buf)
{
	unsigned char reg = 0;
	struct aw8622x *aw8622x = aw8622x_get_drvdata(dev);

	mutex_lock(&aw8622x->lock);
	aw8622x_i2c_read(aw8622x, AW8622X_REG_PLAYCFG2, &reg);
	mutex_unlock(&aw8622x->lock);

	return snprintf(buf, PAGE_SIZE, "0x%02X\n", reg);
}

static ssize_t gain_store(struct device *dev,
				  struct device_attribute *attr,
				  const char *buf, size_t count)
{
	struct aw8622x *aw8622x = aw8622x_get_drvdata(dev);
	unsigned int val = 0;
	int rc = 0;

	rc = kstrtouint(buf, 0, &val);
	if (rc < 0)
		return rc;

	aw_info("value=%d", val);
	if (val >= 0x80)
		val = 0x80;
	mutex_lock(&aw8622x->lock);
	aw8622x->level = val;
	aw8622x_haptic_set_gain(aw8622x, aw8622x->level);
	mutex_unlock(&aw8622x->lock);
	return count;
}
static DEVICE_ATTR_RW(gain);

static ssize_t ram_update_show(struct device *dev,
				       struct device_attribute *attr, char *buf)
{
	struct aw8622x *aw8622x = aw8622x_get_drvdata(dev);
	ssize_t len = 0;
	unsigned int i = 0;
	unsigned char reg_val = 0;

	/* RAMINIT Enable */
	mutex_lock(&aw8622x->lock);
	aw8622x_haptic_raminit(aw8622x, true);
	aw8622x_haptic_stop(aw8622x);
	aw8622x_i2c_write(aw8622x, AW8622X_REG_RAMADDRH,
			  (unsigned char)(aw8622x->ram.base_addr >> 8));
	aw8622x_i2c_write(aw8622x, AW8622X_REG_RAMADDRL,
			  (unsigned char)(aw8622x->ram.base_addr & 0x00ff));
	len += snprintf(buf + len, PAGE_SIZE - len,
			"haptic_ram len = %d\n", aw8622x->ram.len);
	for (i = 0; i < aw8622x->ram.len; i++) {
		aw8622x_i2c_read(aw8622x, AW8622X_REG_RAMDATA, &reg_val);
		if (i % 5 == 0)
			len += snprintf(buf + len,
				PAGE_SIZE - len, "0x%02X\n", reg_val);
		else
			len += snprintf(buf + len,
				PAGE_SIZE - len, "0x%02X,", reg_val);
	}
	len += snprintf(buf + len, PAGE_SIZE - len, "\n");
	/* RAMINIT Disable */
	aw8622x_haptic_raminit(aw8622x, false);
	mutex_unlock(&aw8622x->lock);
	return len;
}

static ssize_t ram_update_store(struct device *dev,
					struct device_attribute *attr,
					const char *buf, size_t count)
{
	struct aw8622x *aw8622x = aw8622x_get_drvdata(dev);
	unsigned int val = 0;
	int rc = 0;

	rc = kstrtouint(buf, 0, &val);
	if (rc < 0)
		return rc;
	if (val)
		aw8622x_ram_update(aw8622x);
	return count;
}
static DEVICE_ATTR_RW(ram_update);

static ssize_t f0_save_show(struct device *dev,
				    struct device_attribute *attr, char *buf)
{
	struct aw8622x *aw8622x = aw8622x_get_drvdata(dev);
	ssize_t len = 0;

	len += snprintf(buf + len, PAGE_SIZE - len, "f0_cali_data = 0x%02X\n",
			aw8622x->f0_cali_data);

	return len;
}

static ssize_t f0_save_store(struct device *dev,
				     struct device_attribute *attr,
				     const char *buf, size_t count)
{
	struct aw8622x *aw8622x = aw8622x_get_drvdata(dev);
	unsigned int val = 0;
	int rc = 0;

	rc = kstrtouint(buf, 0, &val);
	if (rc < 0)
		return rc;
	aw8622x->f0_cali_data = val;
	return count;
}
static DEVICE_ATTR_RW(f0_save);

static ssize_t osc_save_show(struct device *dev,
				     struct device_attribute *attr, char *buf)
{
	struct aw8622x *aw8622x = aw8622x_get_drvdata(dev);
	ssize_t len = 0;

	len += snprintf(buf + len, PAGE_SIZE - len, "osc_cali_data = 0x%02X\n",
			aw8622x->osc_cali_data);

	return len;
}

static ssize_t osc_save_store(struct device *dev,
				      struct device_attribute *attr,
				      const char *buf, size_t count)
{
	struct aw8622x *aw8622x = aw8622x_get_drvdata(dev);
	unsigned int val = 0;
	int rc = 0;

	rc = kstrtouint(buf, 0, &val);
	if (rc < 0)
		return rc;
	aw8622x->osc_cali_data = val;
	return count;
}
static DEVICE_ATTR_RW(osc_save);

static ssize_t ram_vbat_comp_show(struct device *dev,
						struct device_attribute *attr,
						char *buf)
{
	struct aw8622x *aw8622x = aw8622x_get_drvdata(dev);
	ssize_t len = 0;

	len += snprintf(buf + len, PAGE_SIZE - len, "ram_vbat_comp = %d\n",
			aw8622x->ram_vbat_compensate);

	return len;
}

static ssize_t ram_vbat_comp_store(struct device *dev,
						 struct device_attribute *attr,
						 const char *buf, size_t count)
{
	struct aw8622x *aw8622x = aw8622x_get_drvdata(dev);
	unsigned int val = 0;
	int rc = 0;

	rc = kstrtouint(buf, 0, &val);
	if (rc < 0)
		return rc;

	mutex_lock(&aw8622x->lock);
	if (val)
		aw8622x->ram_vbat_compensate =
		    AW8622X_HAPTIC_RAM_VBAT_COMP_ENABLE;
	else
		aw8622x->ram_vbat_compensate =
		    AW8622X_HAPTIC_RAM_VBAT_COMP_DISABLE;
	mutex_unlock(&aw8622x->lock);

	return count;
}
static DEVICE_ATTR_RW(ram_vbat_comp);

static ssize_t cali_show(struct device *dev,
				 struct device_attribute *attr, char *buf)
{
	struct aw8622x *aw8622x = aw8622x_get_drvdata(dev);
	ssize_t len = 0;
	unsigned char reg = 0;

	mutex_lock(&aw8622x->lock);
	aw8622x_i2c_read(aw8622x, AW8622X_REG_TRIMCFG3, &reg);
	aw8622x_haptic_upload_lra(aw8622x, AW8622X_F0_CALI);
	aw8622x_haptic_get_f0(aw8622x);
	aw8622x_i2c_write(aw8622x, AW8622X_REG_TRIMCFG3, reg);
	mutex_unlock(&aw8622x->lock);
	len += snprintf(buf + len, PAGE_SIZE - len, "%d\n", aw8622x->f0);
	return len;
}

static ssize_t cali_store(struct device *dev,
				  struct device_attribute *attr,
				  const char *buf, size_t count)
{
	struct aw8622x *aw8622x = aw8622x_get_drvdata(dev);
	unsigned int val = 0;
	int rc = 0;

	rc = kstrtouint(buf, 0, &val);
	if (rc < 0)
		return rc;
	if (val) {
		mutex_lock(&aw8622x->lock);
		aw8622x_haptic_upload_lra(aw8622x, AW8622X_WRITE_ZERO);
		aw8622x_haptic_f0_calibration(aw8622x);
		mutex_unlock(&aw8622x->lock);
	}
	return count;
}
static DEVICE_ATTR_RW(cali);

static ssize_t cont_drv_lvl_show(struct device *dev,
					 struct device_attribute *attr,
					 char *buf)
{
	struct aw8622x *aw8622x = aw8622x_get_drvdata(dev);
	ssize_t len = 0;

	len += snprintf(buf + len, PAGE_SIZE - len, "cont_drv1_lvl = 0x%02X\n",
			aw8622x->cont_drv1_lvl);
	len += snprintf(buf + len, PAGE_SIZE - len, "cont_drv2_lvl = 0x%02X\n",
			aw8622x->cont_drv2_lvl);
	return len;
}

static ssize_t cont_drv_lvl_store(struct device *dev,
					  struct device_attribute *attr,
					  const char *buf, size_t count)
{
	struct aw8622x *aw8622x = aw8622x_get_drvdata(dev);
	unsigned int databuf[2] = { 0, 0 };

	if (sscanf(buf, "%x %x", &databuf[0], &databuf[1]) == 2) {
		aw8622x->cont_drv1_lvl = databuf[0];
		aw8622x->cont_drv2_lvl = databuf[1];
		aw8622x_i2c_write_bits(aw8622x, AW8622X_REG_CONTCFG6,
				       AW8622X_BIT_CONTCFG6_DRV1_LVL_MASK,
				       aw8622x->cont_drv1_lvl);
		aw8622x_i2c_write(aw8622x, AW8622X_REG_CONTCFG7,
				  aw8622x->cont_drv2_lvl);
	}
	return count;
}
static DEVICE_ATTR_RW(cont_drv_lvl);

static ssize_t cont_drv_time_show(struct device *dev,
					  struct device_attribute *attr,
					  char *buf)
{
	struct aw8622x *aw8622x = aw8622x_get_drvdata(dev);
	ssize_t len = 0;

	len += snprintf(buf + len, PAGE_SIZE - len, "cont_drv1_time = 0x%02X\n",
			aw8622x->cont_drv1_time);
	len += snprintf(buf + len, PAGE_SIZE - len, "cont_drv2_time = 0x%02X\n",
			aw8622x->cont_drv2_time);
	return len;
}

static ssize_t cont_drv_time_store(struct device *dev,
					   struct device_attribute *attr,
					   const char *buf, size_t count)
{
	struct aw8622x *aw8622x = aw8622x_get_drvdata(dev);
	unsigned int databuf[2] = { 0, 0 };

	if (sscanf(buf, "%x %x", &databuf[0], &databuf[1]) == 2) {
		aw8622x->cont_drv1_time = databuf[0];
		aw8622x->cont_drv2_time = databuf[1];
		aw8622x_i2c_write(aw8622x, AW8622X_REG_CONTCFG8,
				  aw8622x->cont_drv1_time);
		aw8622x_i2c_write(aw8622x, AW8622X_REG_CONTCFG9,
				  aw8622x->cont_drv2_time);
	}
	return count;
}
static DEVICE_ATTR_RW(cont_drv_time);

static ssize_t cont_brk_time_show(struct device *dev,
					  struct device_attribute *attr,
					  char *buf)
{
	struct aw8622x *aw8622x = aw8622x_get_drvdata(dev);
	ssize_t len = 0;

	len += snprintf(buf + len, PAGE_SIZE - len, "cont_brk_time = 0x%02X\n",
			aw8622x->cont_brk_time);
	return len;
}

static ssize_t cont_brk_time_store(struct device *dev,
					   struct device_attribute *attr,
					   const char *buf, size_t count)
{
	struct aw8622x *aw8622x = aw8622x_get_drvdata(dev);
	int err = 0;
	int val = 0;

	err = kstrtoint(buf, 16, &val);
	if (err != 0) {
		aw_err("format not match!");
		return count;
	}
	mutex_lock(&aw8622x->lock);
	aw8622x->cont_brk_time = val;
	aw8622x_i2c_write(aw8622x, AW8622X_REG_CONTCFG10,
			  aw8622x->cont_brk_time);
	mutex_unlock(&aw8622x->lock);

	return count;
}
static DEVICE_ATTR_RW(cont_brk_time);

static ssize_t vbat_monitor_show(struct device *dev,
					 struct device_attribute *attr,
					 char *buf)
{
	struct aw8622x *aw8622x = aw8622x_get_drvdata(dev);
	ssize_t len = 0;

	mutex_lock(&aw8622x->lock);
	aw8622x_haptic_get_vbat(aw8622x);
	len += snprintf(buf + len, PAGE_SIZE - len, "vbat_monitor = %d\n",
			aw8622x->vbat);
	mutex_unlock(&aw8622x->lock);

	return len;
}

static ssize_t vbat_monitor_store(struct device *dev,
					  struct device_attribute *attr,
					  const char *buf, size_t count)
{
	return count;
}
static DEVICE_ATTR_RW(vbat_monitor);

static ssize_t lra_resistance_show(struct device *dev,
					   struct device_attribute *attr,
					   char *buf)
{
	struct aw8622x *aw8622x = aw8622x_get_drvdata(dev);
	ssize_t len = 0;

	aw8622x_haptic_get_lra_resistance(aw8622x);
	len += snprintf(buf + len, PAGE_SIZE - len, "%d\n",
			aw8622x->lra);
	return len;
}

static ssize_t lra_resistance_store(struct device *dev,
					    struct device_attribute *attr,
					    const char *buf, size_t count)
{
	return count;
}
static DEVICE_ATTR_RW(lra_resistance);

static ssize_t prctmode_show(struct device *dev,
				     struct device_attribute *attr, char *buf)
{
	struct aw8622x *aw8622x = aw8622x_get_drvdata(dev);
	ssize_t len = 0;
	unsigned char reg_val = 0;

	aw8622x_i2c_read(aw8622x, AW8622X_REG_DETCFG1, &reg_val);

	len += snprintf(buf + len, PAGE_SIZE - len, "prctmode = %d\n",
			reg_val & 0x08);
	return len;
}

static ssize_t prctmode_store(struct device *dev,
				      struct device_attribute *attr,
				      const char *buf, size_t count)
{
	struct aw8622x *aw8622x = aw8622x_get_drvdata(dev);
	unsigned int databuf[2] = { 0, 0 };
	unsigned int prtime = 0;
	unsigned int prlvl = 0;

	if (sscanf(buf, "%x %x", &databuf[0], &databuf[1]) == 2) {
		prtime = databuf[0];
		prlvl = databuf[1];
		mutex_lock(&aw8622x->lock);
		aw8622x_haptic_swicth_motor_protect_config(aw8622x, prtime, prlvl);
		mutex_unlock(&aw8622x->lock);
	}
	return count;
}
static DEVICE_ATTR_RW(prctmode);

static ssize_t effect_id_show(struct device *dev,
				     struct device_attribute *attr, char *buf)
{
	struct aw8622x *aw8622x = aw8622x_get_drvdata(dev);

	return snprintf(buf, PAGE_SIZE, "0x%02x\n", aw8622x->effect_id);
}

static ssize_t effect_id_store(struct device *dev,
				      struct device_attribute *attr,
				      const char *buf, size_t count)
{
	struct aw8622x *aw8622x = aw8622x_get_drvdata(dev);
	unsigned int val = 0;
	int rc = 0;

	rc = kstrtouint(buf, 0, &val);
	if (rc < 0)
		return rc;
	aw_info("value=%d", val);

	mutex_lock(&aw8622x->lock);
	aw8622x->input_flag = true;
	if (val > 0 && val < aw8622x->ram.ram_num) {
		aw8622x->activate_mode = AW8622X_RAM_MODE;
		aw8622x->effect_id = val;
	} else if (val == aw8622x->ram.ram_num) {
		aw8622x->activate_mode = AW8622X_RAM_LOOP_MODE;
		aw8622x->effect_id = val;
	} else {
		aw_err("val is error");
	}
	mutex_unlock(&aw8622x->lock);
	return count;
}
static DEVICE_ATTR_RW(effect_id);

static ssize_t rtp_cali_select_show(struct device *dev,
				    struct device_attribute *attr, char *buf)
{
	struct aw8622x *aw8622x = aw8622x_get_drvdata(dev);

	switch(aw8622x->rtp_cali_select) {
	case AW8622X_WRITE_ZERO:
		return snprintf(buf, PAGE_SIZE, "rtp mode use zero cali data\n");
	case AW8622X_F0_CALI:
		return snprintf(buf, PAGE_SIZE, "rtp mode use f0 cali data\n");
	case AW8622X_OSC_CALI:
		return snprintf(buf, PAGE_SIZE, "rtp mode use osc cali data\n");
	default :
		return 0;
	}
}

static ssize_t rtp_cali_select_store(struct device *dev,
				     struct device_attribute *attr,
				     const char *buf, size_t count)
{
	struct aw8622x *aw8622x = aw8622x_get_drvdata(dev);
	unsigned int val = 0;
	int rc = 0;

	rc = kstrtouint(buf, 0, &val);
	if (rc < 0)
		return rc;
	switch(val) {
	case AW8622X_WRITE_ZERO:
		aw_info("rtp mode use zero cali data");
		break;
	case AW8622X_F0_CALI:
		aw_info("rtp mode use f0 cali data");
		break;
	case AW8622X_OSC_CALI:
		aw_info("rtp mode use osc cali data");
		break;
	default :
		val = AW8622X_OSC_CALI;
		aw_err("input error, osc cali data is used by default");
		break;
	}
	aw8622x->rtp_cali_select = val;

	return count;
}
static DEVICE_ATTR_RW(rtp_cali_select);

static ssize_t f0_cali_confirm_show(struct device *dev,
				    struct device_attribute *attr, char *buf)
{
	if (aw8622x_f0_cali_data <= 0x3F)
		return snprintf(buf, PAGE_SIZE, "Yes\n");
	else
		return snprintf(buf, PAGE_SIZE, "No\n");
}
static DEVICE_ATTR_RO(f0_cali_confirm);

static ssize_t output_f0_show(struct device *dev,
				  struct device_attribute *attr, char *buf)
{
	struct aw8622x *aw8622x = aw8622x_get_drvdata(dev);
	unsigned int val = 0;

	if (aw8622x->f0_cali_data <= 0x1F)
		val = aw8622x->dts_info.f0_ref * (10000 + aw8622x->f0_cali_data * 24) / 10000;
	else
		val = aw8622x->dts_info.f0_ref * (10000 - (0x40 - aw8622x->f0_cali_data) * 24) / 10000;

	return snprintf(buf, PAGE_SIZE, "%u\n", val);
}
static DEVICE_ATTR_RO(output_f0);

static struct attribute *aw8622x_vibrator_attributes[] = {
	&dev_attr_state.attr,
	&dev_attr_duration.attr,
	&dev_attr_activate.attr,
	&dev_attr_activate_mode.attr,
	&dev_attr_index.attr,
	&dev_attr_gain.attr,
	&dev_attr_seq.attr,
	&dev_attr_loop.attr,
	&dev_attr_reg.attr,
	&dev_attr_rtp.attr,
	&dev_attr_ram_update.attr,
	&dev_attr_f0.attr,
	&dev_attr_cali.attr,
	&dev_attr_f0_save.attr,
	&dev_attr_osc_save.attr,
	&dev_attr_cont.attr,
	&dev_attr_cont_drv_lvl.attr,
	&dev_attr_cont_drv_time.attr,
	&dev_attr_cont_brk_time.attr,
	&dev_attr_vbat_monitor.attr,
	&dev_attr_lra_resistance.attr,
	&dev_attr_prctmode.attr,
	&dev_attr_ram_vbat_comp.attr,
	&dev_attr_osc_cali.attr,
	&dev_attr_effect_id.attr,
	&dev_attr_rtp_cali_select.attr,
	&dev_attr_f0_cali_confirm.attr,
	&dev_attr_output_f0.attr,
	NULL
};

struct attribute_group aw8622x_vibrator_attribute_group = {
	.attrs = aw8622x_vibrator_attributes
};

/*
 * under the input architecture, start a ram or ram_loop play
 */
static int aw8622x_haptic_play_effect_seq(struct aw8622x *aw8622x,
					  unsigned char flag)
{
	aw_info("enter, effect_id=%d, activate_mode=%d", aw8622x->effect_id,
		aw8622x->activate_mode);
	if (aw8622x->effect_id > aw8622x->ram.ram_num)
		return 0;
	if (flag) {
		if (aw8622x->activate_mode == AW8622X_RAM_MODE) {
			aw8622x_haptic_set_wav_seq(aw8622x, 0x00,
						  (char)aw8622x->effect_id);
			aw8622x_haptic_set_wav_seq(aw8622x, 0x01, 0x00);
			aw8622x_haptic_set_wav_loop(aw8622x, 0x00, 0x00);
			aw8622x_haptic_play_mode(aw8622x,
						 AW8622X_RAM_MODE);
			aw8622x_haptic_effect_strength(aw8622x);
			aw8622x_haptic_set_gain(aw8622x, aw8622x->level);
			aw8622x_haptic_play_go(aw8622x, true);
		}
		if (aw8622x->activate_mode == AW8622X_RAM_LOOP_MODE) {
			aw8622x_haptic_set_repeat_wav_seq(aw8622x,
							  aw8622x->ram.ram_num);
			aw8622x_haptic_play_repeat_seq(aw8622x, true);
		}
	}
	aw_info("exit");
	return 0;
}

/*
 * the callback function of the workqueue
 */
static void aw8622x_vibrator_work_routine(struct work_struct *work)
{
	struct aw8622x *aw8622x = container_of(work, struct aw8622x,
					       vibrator_work);

	mutex_lock(&aw8622x->lock);
	aw_info("enter, state=%d activate_mode=%d duration=%d f0:%d f0_cali=%d",
		aw8622x->state, aw8622x->activate_mode, aw8622x->duration, aw8622x->f0, aw8622x->f0_cali_data);
	/* Enter standby mode */
	aw8622x_haptic_stop(aw8622x);
	aw8622x_haptic_upload_lra(aw8622x, AW8622X_F0_CALI);
	if (aw8622x->state) {
		if (aw8622x->activate_mode == AW8622X_RAM_MODE) {
			aw8622x_ram_vbat_comp(aw8622x, false);
			if (aw8622x->input_flag)
				aw8622x_haptic_play_effect_seq(aw8622x, true);
			else
				aw8622x_ram_play(aw8622x,
						 AW8622X_RAM_MODE);
		} else if (aw8622x->activate_mode == AW8622X_RAM_LOOP_MODE) {
			aw8622x_ram_vbat_comp(aw8622x, false);
			if (aw8622x->input_flag)
				aw8622x_haptic_play_effect_seq(aw8622x, true);
			else
				aw8622x_ram_play(aw8622x,
						 AW8622X_RAM_LOOP_MODE);
			/* run ms timer */
			hrtimer_start(&aw8622x->timer,
				      ktime_set(aw8622x->duration / 1000,
				      (aw8622x->duration % 1000) * 1000000),
				      HRTIMER_MODE_REL);
			pm_stay_awake(aw8622x->dev);
			aw8622x->wk_lock_flag = 1;
		} else if (aw8622x->activate_mode == AW8622X_CONT_MODE) {
			aw_info("enter cont mode");
			aw8622x_haptic_cont_config(aw8622x);
			/* run ms timer */
			hrtimer_start(&aw8622x->timer,
				      ktime_set(aw8622x->duration / 1000,
				      (aw8622x->duration % 1000) * 1000000),
				      HRTIMER_MODE_REL);
		} else {
			aw_err("activate_mode error");
		}
	} else {
		if (aw8622x->wk_lock_flag == 1) {
			pm_relax(aw8622x->dev);
			aw8622x->wk_lock_flag = 0;
		}
	}
	mutex_unlock(&aw8622x->lock);
}

static int aw8622x_vibrator_init(struct aw8622x *aw8622x)
{
#if !defined(CONFIG_AW8622X_SAMSUNG_FEATURE)  // Dont Register LED
	int ret = 0;
#endif

	aw_info("enter, loaded in leds_cdev framework!");
	aw8622x->vib_dev.name = "vibrator";
	aw8622x->vib_dev.brightness_get = aw8622x_haptic_brightness_get;
	aw8622x->vib_dev.brightness_set = aw8622x_haptic_brightness_set;

#if !defined(CONFIG_AW8622X_SAMSUNG_FEATURE)  // Dont Register LED
	ret = devm_led_classdev_register(&aw8622x->i2c->dev, &aw8622x->vib_dev);
	if (ret < 0) {
		aw_err("fail to create led dev");
		return ret;
	}

	ret = sysfs_create_group(&aw8622x->vib_dev.dev->kobj,
				 &aw8622x_vibrator_attribute_group);
	if (ret < 0) {
		aw_err("error creating sysfs attr files");
		return ret;
	}
#endif

	hrtimer_init(&aw8622x->timer, CLOCK_MONOTONIC, HRTIMER_MODE_REL);
	aw8622x->timer.function = aw8622x_vibrator_timer_func;
	INIT_WORK(&aw8622x->vibrator_work,
		  aw8622x_vibrator_work_routine);
	INIT_WORK(&aw8622x->rtp_work, aw8622x_rtp_work_routine);
	mutex_init(&aw8622x->lock);
	mutex_init(&aw8622x->rtp_lock);
	atomic_set(&aw8622x->is_in_rtp_loop, 0);
	atomic_set(&aw8622x->exit_in_rtp_loop, 0);
	init_waitqueue_head(&aw8622x->wait_q);
	init_waitqueue_head(&aw8622x->stop_wait_q);

	return 0;
}

/*
 * chip register initialization
 */
static void aw8622x_haptic_misc_para_init(struct aw8622x *aw8622x)
{

	aw_info("enter");
	aw8622x->cont_drv1_lvl = aw8622x->dts_info.cont_drv1_lvl_dt;
	aw8622x->cont_drv1_time = aw8622x->dts_info.cont_drv1_time_dt;
	aw8622x->cont_drv2_time = aw8622x->dts_info.cont_drv2_time_dt;
	aw8622x->cont_brk_time = aw8622x->dts_info.cont_brk_time_dt;

	/* Config TRIG function, modification is not recommended */
	aw8622x_i2c_write_bits(aw8622x, AW8622X_REG_TRGCFG8,
				       AW8622X_BIT_TRGCFG8_TRG_TRIG1_MODE_MASK,
				       AW8622X_BIT_TRGCFG8_TRIG1);

	/* Config rise/fall time of HDP/HDN */
	aw8622x_i2c_write_bits(aw8622x, AW8622X_REG_ANACFG8,
					AW8622X_BIT_ANACFG8_TRTF_CTRL_HDRV_MASK,
					AW8622X_BIT_ANACFG8_TRTF_CTRL_HDRV);

	/* d2s_gain */
	if (!aw8622x->dts_info.d2s_gain) {
		aw_err("aw8622x->dts_info.d2s_gain = 0!");
	} else {
		aw8622x_i2c_write_bits(aw8622x, AW8622X_REG_SYSCTRL7,
				       AW8622X_BIT_SYSCTRL7_D2S_GAIN_MASK,
				       aw8622x->dts_info.d2s_gain);
	}

	/* cont_brk_time */
	if (!aw8622x->cont_brk_time) {
		aw_err("aw8622x->cont_brk_time = 0!");
	} else {
		aw8622x_i2c_write(aw8622x, AW8622X_REG_CONTCFG10,
				  aw8622x->cont_brk_time);
	}

	/* cont_brk_gain */
	if (!aw8622x->dts_info.cont_brk_gain) {
		aw_err("aw8622x->dts_info.cont_brk_gain = 0!");
	} else {
		aw8622x_i2c_write_bits(aw8622x, AW8622X_REG_CONTCFG5,
				       AW8622X_BIT_CONTCFG5_BRK_GAIN_MASK,
				       aw8622x->dts_info.cont_brk_gain);
	}

	/* cont_drv2_lvl for rms voltage of LRA */
	aw8622x->cont_drv2_lvl = AW8622X_DRV2_LVL_FORMULA(aw8622x->dts_info.f0_ref, aw8622x->dts_info.lra_vrms);
	aw_info("lra_vrms=%d, cont_drv2_lvl=0x%02X", aw8622x->dts_info.lra_vrms,
		aw8622x->cont_drv2_lvl);
	if (aw8622x->cont_drv2_lvl > AW8622X_DRV2_LVL_MAX) {
		aw_err("cont_drv2_lvl[0x%02X] is error, restore max vale[0x%02X]",
		       aw8622x->cont_drv2_lvl, AW8622X_DRV2_LVL_MAX);
		aw8622x->cont_drv2_lvl = AW8622X_DRV2_LVL_MAX;
	}
}

/*
 * offset calibration for bemf detection,
 * it is recommended not to modify
 */
static int aw8622x_haptic_offset_calibration(struct aw8622x *aw8622x)
{
	unsigned int cont = 2000;
	unsigned char reg_val = 0;

	aw_info("enter");

	aw8622x_haptic_raminit(aw8622x, true);
	aw8622x_i2c_write_bits(aw8622x, AW8622X_REG_DETCFG2,
			       AW8622X_BIT_DETCFG2_DIAG_GO_MASK,
			       AW8622X_BIT_DETCFG2_DIAG_GO_ON);
	while (1) {
		aw8622x_i2c_read(aw8622x, AW8622X_REG_DETCFG2, &reg_val);
		if ((reg_val & 0x01) == 0 || cont == 0)
			break;
		cont--;
	}
	if (cont == 0)
		aw_err("calibration offset failed!");
	aw8622x_haptic_raminit(aw8622x, false);
	return 0;
}

/*
 * vbat mode for cont mode, the output waveform will
 * keep stable against VDD voltage drop
 */
static int aw8622x_haptic_vbat_mode_config(struct aw8622x *aw8622x,
					   unsigned char flag)
{
	if (flag == AW8622X_HAPTIC_CONT_VBAT_HW_ADJUST_MODE) {
		aw8622x_i2c_write(aw8622x, AW8622X_REG_GLBCFG2,
				  AW8622X_GLBCFG2_START_DLY_INIT_VALUE);
		aw8622x_i2c_write_bits(aw8622x, AW8622X_REG_SYSCTRL1,
				       AW8622X_BIT_SYSCTRL1_VBAT_MODE_MASK,
				       AW8622X_BIT_SYSCTRL1_VBAT_MODE_HW);
	} else {
		aw8622x_i2c_write_bits(aw8622x, AW8622X_REG_SYSCTRL1,
				       AW8622X_BIT_SYSCTRL1_VBAT_MODE_MASK,
				       AW8622X_BIT_SYSCTRL1_VBAT_MODE_SW);
	}
	return 0;
}

/*
 * ram data update workqueue callback function
 */
static void aw8622x_ram_work_routine(struct work_struct *work)
{
	struct aw8622x *aw8622x = container_of(work, struct aw8622x,
						ram_work.work);

	aw_info("enter");
	aw8622x_ram_update(aw8622x);
}

static int aw8622x_ram_work_init(struct aw8622x *aw8622x)
{
	int ram_timer_val = 1000;

	aw_info("enter");
	INIT_DELAYED_WORK(&aw8622x->ram_work, aw8622x_ram_work_routine);
	schedule_delayed_work(&aw8622x->ram_work,
				msecs_to_jiffies(ram_timer_val));
	return 0;
}

/*
 * auto brake config
 */
static void aw8622x_haptic_auto_brk_enable(struct aw8622x *aw8622x,
					   unsigned char flag)
{
	if (flag) {
		aw8622x_i2c_write_bits(aw8622x, AW8622X_REG_PLAYCFG3,
				AW8622X_BIT_PLAYCFG3_BRK_EN_MASK,
				AW8622X_BIT_PLAYCFG3_BRK_ENABLE);
	} else {
		aw8622x_i2c_write_bits(aw8622x, AW8622X_REG_PLAYCFG3,
				AW8622X_BIT_PLAYCFG3_BRK_EN_MASK,
				AW8622X_BIT_PLAYCFG3_BRK_DISABLE);
	}
}

static int aw8622x_haptic_init(struct aw8622x *aw8622x)
{
	int ret = 0;
	unsigned char i = 0;
	unsigned char reg_val = 0;

	aw_info("enter");
	mutex_lock(&aw8622x->lock);
	/* haptic init */
	aw8622x->ram_state = 0;
#if defined(CONFIG_AW8622X_SAMSUNG_FEATURE)
	aw8622x->gain = 10000;
#else
	aw8622x->gain = 0x7FFF;
#endif
	aw8622x->input_flag = false;
	aw8622x->rtp_cali_select = AW8622X_OSC_CALI;
	aw8622x->activate_mode = aw8622x->dts_info.mode;
	ret = aw8622x_i2c_read(aw8622x, AW8622X_REG_WAVCFG1, &reg_val);
	aw8622x->index = reg_val & AW8622X_BIT_WAVCFG_SEQ;
	ret = aw8622x_i2c_read(aw8622x, AW8622X_REG_PLAYCFG2, &reg_val);
	aw8622x->level = reg_val;
	aw_info("aw8622x->level =0x%02X", aw8622x->level);
	for (i = 0; i < AW8622X_SEQUENCER_SIZE; i++) {
		ret = aw8622x_i2c_read(aw8622x, AW8622X_REG_WAVCFG1 + i,
				       &reg_val);
		aw8622x->seq[i] = reg_val;
	}
	aw8622x_haptic_play_mode(aw8622x, AW8622X_STANDBY_MODE);
	aw8622x_haptic_set_pwm(aw8622x, AW8622X_PWM_12K);
	/* misc value init */
	aw8622x_haptic_misc_para_init(aw8622x);
	/* set motor protect */
	aw8622x_haptic_swicth_motor_protect_config(aw8622x, AW8622X_PWMCFG4_PRTIME_DEFAULT_VALUE,
						   AW8622X_BIT_PWMCFG3_PRLVL_DEFAULT_VALUE);
	aw8622x_haptic_trig_config(aw8622x);
	aw8622x_haptic_offset_calibration(aw8622x);
	/* config auto_brake */
	aw8622x_haptic_auto_brk_enable(aw8622x,
				       aw8622x->dts_info.is_enabled_auto_brk);
	/* vbat compensation */
	aw8622x_haptic_vbat_mode_config(aw8622x,
				AW8622X_HAPTIC_CONT_VBAT_HW_ADJUST_MODE);
	aw8622x->ram_vbat_compensate = AW8622X_HAPTIC_RAM_VBAT_COMP_DISABLE;

	/* f0 calibration */
#ifdef AW_KERNEL_F0_CALIBRATION
	aw8622x_haptic_upload_lra(aw8622x, AW8622X_WRITE_ZERO);
	aw8622x_haptic_f0_calibration(aw8622x);
#else
#if defined(CONFIG_AW8622X_SAMSUNG_FEATURE)
	aw_info("aw8622x_f0_cali_data=%u", aw8622x_f0_cali_data);
	aw_info("aw8622x_f0_data=%u", aw8622x_f0_data);
#endif
	aw8622x->f0 = aw8622x_f0_data;
	aw8622x->f0_cali_data = (uint8_t)aw8622x_f0_cali_data;
	aw8622x_haptic_upload_lra(aw8622x, AW8622X_F0_CALI);
#endif
	mutex_unlock(&aw8622x->lock);
	return ret;
}

/*
 * interrupt register configuration
 */
static void aw8622x_interrupt_setup(struct aw8622x *aw8622x)
{
	unsigned char reg_val = 0;

	aw_info("enter");

	aw8622x_i2c_read(aw8622x, AW8622X_REG_SYSINT, &reg_val);

	aw_info("reg SYSINT=0x%02X", reg_val);

	/* History mode of interrupt */
	aw8622x_i2c_write_bits(aw8622x, AW8622X_REG_SYSCTRL7,
			       AW8622X_BIT_SYSCTRL7_INT_MODE_MASK,
			       AW8622X_BIT_SYSCTRL7_INT_MODE_EDGE);
	aw8622x_i2c_write_bits(aw8622x, AW8622X_REG_SYSCTRL7,
			       AW8622X_BIT_SYSCTRL7_INT_EDGE_MODE_MASK,
			       AW8622X_BIT_SYSCTRL7_INT_EDGE_MODE_POS);
	/* Enable undervoltage int */
	aw8622x_i2c_write_bits(aw8622x, AW8622X_REG_SYSINTM,
			       AW8622X_BIT_SYSINTM_UVLM_MASK,
			       AW8622X_BIT_SYSINTM_UVLM_ON);
	/* Enable over current int */
	aw8622x_i2c_write_bits(aw8622x, AW8622X_REG_SYSINTM,
			       AW8622X_BIT_SYSINTM_OCDM_MASK,
			       AW8622X_BIT_SYSINTM_OCDM_ON);
	/* Enable over temperature int */
	aw8622x_i2c_write_bits(aw8622x, AW8622X_REG_SYSINTM,
			       AW8622X_BIT_SYSINTM_OTM_MASK,
			       AW8622X_BIT_SYSINTM_OTM_ON);
}

/*
 * interrupt callback function
 */
static irqreturn_t aw8622x_irq(int irq, void *data)
{
	struct aw8622x *aw8622x = data;
	unsigned char reg_val = 0;
	unsigned int buf_len = 0;
	unsigned char glb_state_val = 0;
	struct aw8622x_container *rtp_container = aw8622x->rtp_container;

	aw_info("enter");
	atomic_set(&aw8622x->is_in_rtp_loop, 1);
	aw8622x_i2c_read(aw8622x, AW8622X_REG_SYSINT, &reg_val);
	aw_info("reg SYSINT=0x%02X", reg_val);
	if (reg_val & AW8622X_BIT_SYSINT_UVLI)
		aw_err("chip uvlo int error");
	if (reg_val & AW8622X_BIT_SYSINT_OCDI)
		aw_err("chip over current int error");
	if (reg_val & AW8622X_BIT_SYSINT_OTI)
		aw_err("chip over temperature int error");
	if (reg_val & AW8622X_BIT_SYSINT_DONEI)
		aw_info("chip playback done");

	if (reg_val & AW8622X_BIT_SYSINT_FF_AEI) {
		aw_info("aw8622x rtp fifo almost empty");
		if (aw8622x->rtp_init) {
			while ((!aw8622x_haptic_rtp_get_fifo_afs(aw8622x)) &&
			       (aw8622x->play_mode == AW8622X_RTP_MODE)
				  && !atomic_read(&aw8622x->exit_in_rtp_loop)) {
				mutex_lock(&aw8622x->rtp_lock);
				aw_info("aw8622x rtp mode fifo update, cnt=%d",
					aw8622x->rtp_cnt);
				if (!rtp_container) {
					aw_info("rtp_container is null, break!");
					mutex_unlock(&aw8622x->rtp_lock);
					break;
				}
				if ((rtp_container->len - aw8622x->rtp_cnt) <
				    (aw8622x->ram.base_addr >> 2)) {
					buf_len = rtp_container->len -
							   aw8622x->rtp_cnt;
				} else {
					buf_len = (aw8622x->ram.base_addr >> 2);
				}
				aw8622x->rtp_update_flag =
				    aw8622x_i2c_writes(aw8622x,
						       AW8622X_REG_RTPDATA,
						       &rtp_container->data
						       [aw8622x->rtp_cnt],
						       buf_len);
				aw8622x->rtp_cnt += buf_len;
				aw8622x_i2c_read(aw8622x, AW8622X_REG_GLBRD5,
						 &glb_state_val);
				if (aw8622x->rtp_cnt == rtp_container->len) {
					if (aw8622x->rtp_repeat) {
						aw8622x->rtp_cnt = 0;
						aw8622x->rtp_init = 1;
						aw_info("rtp repeat!");
					} else {
						aw8622x->rtp_cnt = 0;
						aw8622x->rtp_init = 0;
						aw8622x_haptic_set_rtp_aei(aw8622x, false);
						aw_info("rtp load completely! glb_state_val=%02x aw8622x->rtp_cnt=%d",
							glb_state_val, aw8622x->rtp_cnt);
						mutex_unlock(&aw8622x->rtp_lock);
						break;
					}
				} else if ((glb_state_val & 0x0f) == 0) {
					aw8622x->rtp_cnt = 0;
					aw8622x->rtp_init = 0;
					aw8622x_haptic_set_rtp_aei(aw8622x, false);
					aw_err("rtp load failed!! glb_state_val=%02x aw8622x->rtp_cnt=%d",
						glb_state_val, aw8622x->rtp_cnt);
					mutex_unlock(&aw8622x->rtp_lock);
					break;
				}
				mutex_unlock(&aw8622x->rtp_lock);
			}
		} else {
			aw_err("aw8622x rtp init=%d, init error",
				aw8622x->rtp_init);
		}
	}

	if (reg_val & AW8622X_BIT_SYSINT_FF_AFI)
		aw_info("aw8622x rtp mode fifo almost full!");

	if (aw8622x->play_mode != AW8622X_RTP_MODE ||
	    atomic_read(&aw8622x->exit_in_rtp_loop))
		aw8622x_haptic_set_rtp_aei(aw8622x, false);

	atomic_set(&aw8622x->is_in_rtp_loop, 0);
	wake_up_interruptible(&aw8622x->wait_q);
	aw_info("exit");

	return IRQ_HANDLED;
}

/*
 * check mass production bit, usually done during initialization,
 * modification is not recommended
 */
static char aw8622x_check_qualify(struct aw8622x *aw8622x)
{
	unsigned char reg = 0;
	int ret = 0;

	ret = aw8622x_i2c_read(aw8622x, AW8622X_REG_EFRD9, &reg);
	if (ret < 0) {
		aw_err("failed to read register 0x64: %d", ret);
		return ret;
	}
	if ((reg & 0x80) == 0x80)
		return 1;
	aw_err("register 0x64 error: 0x%02x", reg);
	return 0;
}

/*
 * analysis of upper layer parameters of input architecture
 */
static int aw8622x_haptics_upload_effect(struct input_dev *dev,
					 struct ff_effect *effect,
					 struct ff_effect *old)
{
	struct aw8622x *aw8622x = aw8622x_input_get_drvdata(dev);
	unsigned short data[2] = {0};
	ktime_t rem;
	unsigned long long time_us;
	int ret;
	int effect_id_max = 0;

	aw_info("enter, effect->type=0x%X, FF_CONSTANT=0x%X, FF_PERIODIC=0x%X",
		effect->type, FF_CONSTANT, FF_PERIODIC);
	aw8622x->input_flag = true;
	effect_id_max = sizeof(aw8622x_rtp_name) / sizeof(*aw8622x_rtp_name) +
			aw8622x->ram.ram_num - 1;
	if (!aw8622x->ram_init) {
		aw_info("ram init failed, not allow to play!");
		return -ERANGE;
	}
	if (aw8622x->osc_cali_run != 0)
		return -ERANGE;
	if (hrtimer_active(&aw8622x->timer)) {
		rem = hrtimer_get_remaining(&aw8622x->timer);
		time_us = ktime_to_us(rem);
		aw_info("waiting for playing clear sequence: %llu us, f0:%d, f0_cali=%d",
			time_us, aw8622x->f0, aw8622x->f0_cali_data);
		usleep_range(time_us, time_us + 100);
	}

	aw8622x->effect_type = effect->type;
	cancel_work_sync(&aw8622x->vibrator_work);
	cancel_work_sync(&aw8622x->rtp_work);
	mutex_lock(&aw8622x->lock);
	while (atomic_read(&aw8622x->exit_in_rtp_loop)) {
		aw_info("goint to waiting rtp exit, f0:%d, f0_cali=%d", aw8622x->f0, aw8622x->f0_cali_data);
		mutex_unlock(&aw8622x->lock);
		ret = wait_event_interruptible(aw8622x->stop_wait_q,
				atomic_read(&aw8622x->exit_in_rtp_loop) == 0);
		aw_info("wakeup");
		if (ret == -ERESTARTSYS) {
			mutex_unlock(&aw8622x->lock);
			aw_info(": wake up by signal return erro : %d, f0:%d, f0_cali=%d",
				ret, aw8622x->f0, aw8622x->f0_cali_data);
			return ret;
		}
		mutex_lock(&aw8622x->lock);
	}

	if (aw8622x->effect_type == FF_CONSTANT) {
		aw_info("effect_type: FF_CONSTANT!");
		aw8622x->duration = effect->replay.length;
		aw8622x->activate_mode = AW8622X_RAM_LOOP_MODE;
		aw8622x->effect_id = aw8622x->ram.ram_num;
	} else if (aw8622x->effect_type == FF_PERIODIC) {
		aw_info("effect_type: FF_PERIODIC!");
		if (effect->u.periodic.waveform == FF_CUSTOM) {
			ret = copy_from_user(data, effect->u.periodic.custom_data,
					     sizeof(short) * 2);
			if (ret) {
				mutex_unlock(&aw8622x->lock);
				return -EFAULT;
			}
		} else {
			data[0] = effect->u.periodic.offset;
			data[1] = 0;
		}
#if defined(CONFIG_AW8622X_SAMSUNG_FEATURE)
		data[0] = aw8622x_sep_index_mapping(aw8622x, (unsigned short)data[0]);
#endif
		if (data[0] < 1 || data[0] > effect_id_max) {
#if defined(CONFIG_AW8622X_SAMSUNG_FEATURE)
			aw_err("data[0] out of range :%d !!", data[0]);
#else
			aw_err("data[0] out of range !!");
#endif
			mutex_unlock(&aw8622x->lock);
			return -ERANGE;
		}
		aw8622x->effect_id = data[0];
		if (aw8622x->effect_id < aw8622x->ram.ram_num) {
			aw8622x->activate_mode = AW8622X_RAM_MODE;
			aw_info("effect_id=%d, activate_mode=%d",
				aw8622x->effect_id, aw8622x->activate_mode);
		}
		if (aw8622x->effect_id > aw8622x->ram.ram_num) {
			aw8622x->activate_mode = AW8622X_RTP_MODE;
			aw8622x->rtp_repeat = data[1];
#if defined(CONFIG_AW8622X_SAMSUNG_FEATURE)
			aw_info("effect_id=%d, activate_mode=%d, rtp_repeat=%d",
				aw8622x->effect_id, aw8622x->activate_mode, aw8622x->rtp_repeat);
#else
			aw_info("effect_id=%d, activate_mode=%d",
				aw8622x->effect_id, aw8622x->activate_mode);
#endif
		}

	} else {
		aw_info("Unsupported effect type: %d", effect->type);
	}
	mutex_unlock(&aw8622x->lock);
	aw_info("aw8622x->effect_type= 0x%X", aw8622x->effect_type);
	return 0;
}

/*
 * input architecture playback function
 */
static int aw8622x_haptics_playback(struct input_dev *dev, int effect_id,
				    int val)
{
	struct aw8622x *aw8622x = aw8622x_input_get_drvdata(dev);
	int rc = 0;

	aw_info("enter, effect_id=%d, val=%d, aw8622x->effect_id=%d, aw8622x->activate_mode=%d",
		effect_id, val, aw8622x->effect_id, aw8622x->activate_mode);
	aw8622x->input_flag = true;

	/* for osc calibration */
	if (aw8622x->osc_cali_run != 0)
		return 0;
	if (val > 0) {
		aw8622x->state = 1;
	} else {
		queue_work(aw8622x->work_queue, &aw8622x->stop_work);
		return 0;
	}
	hrtimer_cancel(&aw8622x->timer);

	if (aw8622x->effect_type == FF_CONSTANT &&
	    aw8622x->activate_mode == AW8622X_RAM_LOOP_MODE) {
		aw_info("enter ram_loop_mode ");
		queue_work(aw8622x->work_queue, &aw8622x->vibrator_work);
	} else if (aw8622x->effect_type == FF_PERIODIC &&
		   aw8622x->activate_mode == AW8622X_RAM_MODE) {
		aw_info("enter ram_mode");
		queue_work(aw8622x->work_queue, &aw8622x->vibrator_work);
	} else if (aw8622x->effect_type == FF_PERIODIC &&
		   aw8622x->activate_mode == AW8622X_RTP_MODE) {
		aw_info("enter rtp_mode");
		queue_work(aw8622x->work_queue, &aw8622x->rtp_work);
	} else {
		/*other mode */
	}

	return rc;
}

static int aw8622x_haptics_erase(struct input_dev *dev, int effect_id)
{
	struct aw8622x *aw8622x = aw8622x_input_get_drvdata(dev);
	int rc = 0;

	aw_info("enter");
	aw8622x->input_flag = true;
	/* for osc calibration */
	if (aw8622x->osc_cali_run != 0)
		return 0;
	aw8622x->effect_type = 0;
	aw8622x->duration = 0;
	return rc;
}

static void aw8622x_haptics_stop_work_routine(struct work_struct *work)
{
	struct aw8622x *aw8622x = container_of(work, struct aw8622x, stop_work);

	mutex_lock(&aw8622x->lock);
	hrtimer_cancel(&aw8622x->timer);
	aw8622x_haptic_stop(aw8622x);
	mutex_unlock(&aw8622x->lock);
}

/*
 * Samsung Gain values : 0 ~ 10000
 * Awinic Gain Register Values : 30 ~ 128
 *
 * Samsung mapping formula:
 *   gain = (ss_gain * max_level_gain)/ 10000
 */
static void aw8622x_haptics_set_gain_work_routine(struct work_struct *work)
{
	unsigned char comp_level = 0;
	struct aw8622x *aw8622x =
	    container_of(work, struct aw8622x, set_gain_work);

#if defined(CONFIG_AW8622X_SAMSUNG_FEATURE)
	if (aw8622x->gain >= 10000)
		aw8622x->level = aw8622x->dts_info.max_level_gain_val;
	else
		aw8622x->level = aw8622x->gain * aw8622x->dts_info.max_level_gain_val / 10000;
#else
	if (aw8622x->gain >= 0x7FFF)
		aw8622x->level = 0x80; /* 128 */
	else if (aw8622x->gain <= 0x3FFF)
		aw8622x->level = 0x1E; /* 30 */
	else
		aw8622x->level = (aw8622x->gain - 16383) / 128;

	if (aw8622x->level < 0x1E)
		aw8622x->level = 0x1E; /* 30 */
#endif //CONFIG_AW8622X_SAMSUNG_FEATURE

	aw_info("set_gain queue work, vmax=0x%04X, level=0x%04X",
		aw8622x->gain, aw8622x->level);

	if (aw8622x->ram_vbat_compensate == AW8622X_HAPTIC_RAM_VBAT_COMP_ENABLE &&
	    aw8622x->vbat) {
		aw_info("ref %d vbat %d", AW8622X_VBAT_REFER, aw8622x->vbat);
		comp_level = aw8622x->level * AW8622X_VBAT_REFER /
			     aw8622x->vbat;
		if (comp_level > (128 * AW8622X_VBAT_REFER /
				  AW8622X_VBAT_MIN)) {
			comp_level = 128 * AW8622X_VBAT_REFER /
				     AW8622X_VBAT_MIN;
			aw_info("comp_level: 0x%X", comp_level);
		}
		aw_info("enable vbat comp, level=0x%X, comp_level=0x%X",
			aw8622x->level, comp_level);
		aw8622x_i2c_write(aw8622x, AW8622X_REG_PLAYCFG2, comp_level);
	} else {
		aw_info("disable vbat comp, vbat=%dmv, vbat_min=%dmv, vbat_ref=%dmv",
			aw8622x->vbat, AW8622X_VBAT_MIN, AW8622X_VBAT_REFER);
		aw8622x_i2c_write(aw8622x, AW8622X_REG_PLAYCFG2,
				  aw8622x->level);
	}
}

/*
 * input architecture gain setting for waveform data
 */
static void aw8622x_haptics_set_gain(struct input_dev *dev, u16 gain)
{
	struct aw8622x *aw8622x = aw8622x_input_get_drvdata(dev);

	aw_info("enter, gain: 0x%04X", gain);
	aw8622x->input_flag = true;
	aw8622x->gain = gain;
	aw_info("before temp check for gain adjustment, gain: 0x%04X", aw8622x->gain);
	aw8622x->gain = sec_vib_inputff_tune_gain(&aw8622x->sec_vib_ddata, aw8622x->gain);
	aw_info("after temp check for gain adjustment, gain: 0x%04X", aw8622x->gain);
	queue_work(aw8622x->work_queue, &aw8622x->set_gain_work);
}

/*
 * hardware reset function
 */
static int aw8622x_hw_reset(struct aw8622x *aw8622x)
{
	aw_info("enter");
	if (aw8622x && gpio_is_valid(aw8622x->reset_gpio)) {
		gpio_set_value_cansleep(aw8622x->reset_gpio, 0);
		usleep_range(1000, 2000);
		gpio_set_value_cansleep(aw8622x->reset_gpio, 1);
		usleep_range(8000, 8500);
	} else {
		aw_err("failed");
	}
	return 0;
}

/*
 * software reset function
 */
static int aw8622x_haptic_softreset(struct aw8622x *aw8622x)
{
	aw_info("enter");
	aw8622x_i2c_write(aw8622x, AW8622X_REG_ID, AW8622X_BIT_RESET);
	usleep_range(2000, 2500);
	return 0;
}

/*
 * parse chip id
 */
static int aw8622x_parse_chipid(struct aw8622x *aw8622x)
{
	int ret = -1;
	unsigned char cnt = 0;
	unsigned char reg = 0;
	unsigned char ef_id = 0xff;


	while (cnt < AW8622x_READ_CHIPID_RETRIES) {
		/* hardware reset */
		aw8622x_hw_reset(aw8622x);

		ret = aw8622x_i2c_read(aw8622x, AW8622X_REG_ID, &reg);
		if (ret < 0) {
			aw_err("failed to read AW8622X_REG_ID: %d", ret);
			break;
		}

		switch (reg) {
		case AW8622X_CHIP_ID:
			aw8622x_i2c_read(aw8622x, AW8622X_REG_EFRD9, &ef_id);
			if ((ef_id & 0x41) == AW86224_5_EF_ID) {
				aw_info("aw86224 or aw86225 detected");
				aw8622x_haptic_softreset(aw8622x);
				return 0;
			}
			if ((ef_id & 0x41) == AW86223_EF_ID) {
				aw_info("aw86223 detected");
				aw8622x_haptic_softreset(aw8622x);
				return 0;
			}
			aw_info("unsupported ef_id = (0x%02X)", ef_id);
			break;
		default:
			aw_info("unsupported device revision (0x%X)", reg);
			break;
		}
		cnt++;

		usleep_range(2000, 3000);
	}

	return -EINVAL;
}

#if defined(CONFIG_AW8622X_SAMSUNG_FEATURE)
u32 aw8622x_haptics_get_f0_measured(struct input_dev *dev)
{
	struct aw8622x *aw8622x = aw8622x_input_get_drvdata(dev);

	aw_info("enter, get f0 measured\n");

	mutex_lock(&aw8622x->lock);
	aw8622x_haptic_upload_lra(aw8622x, AW8622X_WRITE_ZERO);
	aw8622x_haptic_f0_calibration(aw8622x);
	mutex_unlock(&aw8622x->lock);

	return aw8622x->f0;
}

u32 aw8622x_haptics_set_f0_stored(struct input_dev *dev, u32 val)
{
	struct aw8622x *aw8622x = aw8622x_input_get_drvdata(dev);

	aw_info("enter, set f0 stored\n");

	mutex_lock(&aw8622x->lock);
	aw8622x->f0 = val;
	aw8622x_haptic_upload_lra(aw8622x, AW8622X_F0_CALI);
	mutex_unlock(&aw8622x->lock);

	return 0;
}

u32 aw8622x_haptics_get_f0_stored(struct input_dev *dev)
{
	struct aw8622x *aw8622x = aw8622x_input_get_drvdata(dev);

	aw_info("enter, get f0 stored\n");

	return aw8622x->f0;
}

int aw8622x_haptics_set_f0_offset(struct input_dev *dev, u32 val)
{
	struct aw8622x *aw8622x = aw8622x_input_get_drvdata(dev);

	aw_info("enter, set f0 offset\n");

	mutex_lock(&aw8622x->lock);
	aw8622x->f0_cali_data = (uint8_t)val;
	aw8622x_haptic_upload_lra(aw8622x, AW8622X_F0_CALI);
	mutex_unlock(&aw8622x->lock);

	return 0;
}

int aw8622x_haptics_get_f0_offset(struct input_dev *dev)
{
	struct aw8622x *aw8622x = aw8622x_input_get_drvdata(dev);

	aw_info("enter, get f0 offset\n");

	return aw8622x->f0_cali_data;
}

int aw8622x_haptic_get_lra_resistance_stored(struct input_dev *dev)
{
	struct aw8622x *aw8622x = aw8622x_input_get_drvdata(dev);

	aw_info("enter, get lra_resistance stored\n");
	aw8622x_haptic_get_lra_resistance(aw8622x);

	return aw8622x->lra;
}

static const struct sec_vib_inputff_ops aw8622x_vib_ops = {
	.upload = aw8622x_haptics_upload_effect,
	.playback = aw8622x_haptics_playback,
	.set_gain = aw8622x_haptics_set_gain,
	.erase = aw8622x_haptics_erase,
	.set_use_sep_index = aw8622x_set_use_sep_index,
	.get_f0_measured = aw8622x_haptics_get_f0_measured,
	.set_f0_offset = aw8622x_haptics_set_f0_offset,
	.get_f0_offset = aw8622x_haptics_get_f0_offset,
	.get_f0_stored = aw8622x_haptics_get_f0_stored,
	.set_f0_stored = aw8622x_haptics_set_f0_stored,
	.get_lra_resistance = aw8622x_haptic_get_lra_resistance_stored,
};

static struct attribute_group *aw8622x_dev_attr_groups[] = {
	&aw8622x_vibrator_attribute_group,
	NULL
};


static int samsung_aw8622x_input_init(struct aw8622x *aw8622x)
{
	aw8622x->sec_vib_ddata.dev = aw8622x->dev;
	aw8622x->sec_vib_ddata.vib_ops = &aw8622x_vib_ops;
	aw8622x->sec_vib_ddata.vendor_dev_attr_groups = (struct attribute_group **)aw8622x_dev_attr_groups;
	aw8622x->sec_vib_ddata.private_data = (void *)aw8622x;
	aw8622x->sec_vib_ddata.ff_val = 0;
	aw8622x->sec_vib_ddata.is_f0_tracking = 1;
	aw8622x->sec_vib_ddata.use_common_inputff = true;
	sec_vib_inputff_setbit(&aw8622x->sec_vib_ddata, FF_CONSTANT);
	sec_vib_inputff_setbit(&aw8622x->sec_vib_ddata, FF_PERIODIC);
	sec_vib_inputff_setbit(&aw8622x->sec_vib_ddata, FF_CUSTOM);
	sec_vib_inputff_setbit(&aw8622x->sec_vib_ddata, FF_GAIN);

	return sec_vib_inputff_register(&aw8622x->sec_vib_ddata);
}
#else
static int aw8622x_input_init(struct aw8622x *aw8622x)
{
	int rc;
	struct input_dev *input_dev;
	struct ff_device *ff;

	input_dev = devm_input_allocate_device(aw8622x->dev);
	if (!input_dev)
		return -ENOMEM;

	/* aw8622x input config */
	input_dev->name = "aw8622x_haptic";
	input_set_drvdata(input_dev, aw8622x);
	aw8622x->input_dev = input_dev;
	input_set_capability(input_dev, EV_FF, FF_CONSTANT);
	input_set_capability(input_dev, EV_FF, FF_GAIN);
	input_set_capability(input_dev, EV_FF, FF_PERIODIC);
	input_set_capability(input_dev, EV_FF, FF_CUSTOM);
	rc = input_ff_create(input_dev, 4);
	if (rc < 0) {
		aw_err("create FF input device failed, rc=%d", rc);
		return rc;
	}

	ff = input_dev->ff;
	ff->upload = aw8622x_haptics_upload_effect;
	ff->playback = aw8622x_haptics_playback;
	ff->erase = aw8622x_haptics_erase;
	ff->set_gain = aw8622x_haptics_set_gain;
	rc = input_register_device(input_dev);
	if (rc < 0) {
		aw_err("register input device failed, rc=%d", rc);
		input_ff_destroy(input_dev);
	}

	return rc;
}
#endif

static int aw8622x_i2c_probe(struct i2c_client *i2c,
			     const struct i2c_device_id *id)
{
	struct aw8622x *aw8622x;
	struct device_node *np = i2c->dev.of_node;
	int ret = -1;
	int irq_flags = 0;

	aw_info("enter");
	if (!i2c_check_functionality(i2c->adapter, I2C_FUNC_I2C)) {
		aw_err("check_functionality failed");
		return -EIO;
	}

	aw8622x = devm_kzalloc(&i2c->dev, sizeof(struct aw8622x), GFP_KERNEL);
	if (aw8622x == NULL)
		return -ENOMEM;

	aw8622x->dev = &i2c->dev;
	aw8622x->i2c = i2c;

	i2c_set_clientdata(i2c, aw8622x);
	if (np) {
		ret = aw8622x_parse_dt(&i2c->dev, aw8622x, np);
		if (ret) {
			aw_err("failed to parse device tree node");
			goto err_parse_dt;
		}
	} else {
		aw8622x->reset_gpio = -1;
		aw8622x->irq_gpio = -1;
	}
	if (gpio_is_valid(aw8622x->reset_gpio)) {
		ret = devm_gpio_request_one(&i2c->dev,
				aw8622x->reset_gpio, GPIOF_OUT_INIT_LOW,
				"aw8622x_rst");
		if (ret) {
			aw_err("rst request failed");
			goto err_reset_gpio_request;
		}
	}
	if (gpio_is_valid(aw8622x->irq_gpio)) {
		ret = devm_gpio_request_one(&i2c->dev, aw8622x->irq_gpio,
			GPIOF_DIR_IN, "aw8622x_int");
		if (ret) {
			aw_err("int request failed");
			goto err_irq_gpio_request;
		}
	}

	/* parse chip id */
	ret = aw8622x_parse_chipid(aw8622x);
	if (ret < 0) {
		aw_err("aw8622x parse chipid failed");
		goto err_id;
	}

	/* chip qualify */
	if (!aw8622x_check_qualify(aw8622x)) {
		aw_err("unqualified chip!");
		goto err_check_qualify;
	}

	aw8622x_vibrator_init(aw8622x);
	aw8622x_haptic_init(aw8622x);
	aw8622x_ram_work_init(aw8622x);

	aw8622x->work_queue = create_singlethread_workqueue("aw8622x_vibrator_work_queue");
	if (!aw8622x->work_queue) {
		aw_err("Error creating aw8622x_vibrator_work_queue");
		goto err_sysfs;
	}

	INIT_WORK(&aw8622x->set_gain_work,
		  aw8622x_haptics_set_gain_work_routine);
	INIT_WORK(&aw8622x->stop_work,
		  aw8622x_haptics_stop_work_routine);

	/* aw8622x irq */
	if (gpio_is_valid(aw8622x->irq_gpio) &&
	    !(aw8622x->flags & AW8622X_FLAG_SKIP_INTERRUPTS)) {
		/* register irq handler */
		aw8622x_interrupt_setup(aw8622x);
		irq_flags = IRQF_TRIGGER_FALLING | IRQF_ONESHOT;
		ret = devm_request_threaded_irq(&i2c->dev,
				gpio_to_irq(aw8622x->irq_gpio),
				NULL, aw8622x_irq, irq_flags,
				"aw8622x", aw8622x);
		if (ret != 0) {
			aw_err("failed to request IRQ %d: %d",
				gpio_to_irq(aw8622x->irq_gpio), ret);
			goto err_irq;
		}
	} else {
		aw_info("skipping IRQ registration");
		/* disable feature support if gpio was invalid */
		aw8622x->flags |= AW8622X_FLAG_SKIP_INTERRUPTS;
	}

#if defined(CONFIG_AW8622X_SAMSUNG_FEATURE)
	ret = samsung_aw8622x_input_init(aw8622x);
#else
	ret = aw8622x_input_init(aw8622x);
#endif
	if (ret < 0)
		goto destroy_ff;

	dev_set_drvdata(&i2c->dev, aw8622x);
	aw_info("probe completed successfully!");
	return 0;

destroy_ff:
err_sysfs:
err_irq:
err_check_qualify:
err_id:
	if (gpio_is_valid(aw8622x->irq_gpio))
		devm_gpio_free(&i2c->dev, aw8622x->irq_gpio);
err_irq_gpio_request:
	if (gpio_is_valid(aw8622x->reset_gpio))
		devm_gpio_free(&i2c->dev, aw8622x->reset_gpio);

err_reset_gpio_request:
err_parse_dt:
	devm_kfree(&i2c->dev, aw8622x);
	aw8622x = NULL;
	return ret;

}

static int aw8622x_i2c_remove(struct i2c_client *i2c)
{
	struct aw8622x *aw8622x = i2c_get_clientdata(i2c);

	aw_info("enter");
#ifdef CONFIG_CS40L26_SAMSUNG_FEATURE
	sec_vib_inputff_unregister(&aw8622x->sec_vib_ddata);
#endif
	cancel_delayed_work_sync(&aw8622x->ram_work);
	if (aw8622x->isUsedIntn)
		cancel_work_sync(&aw8622x->rtp_work);
	cancel_work_sync(&aw8622x->vibrator_work);
	hrtimer_cancel(&aw8622x->timer);
	mutex_destroy(&aw8622x->lock);
	mutex_destroy(&aw8622x->rtp_lock);
	devm_free_irq(&i2c->dev, gpio_to_irq(aw8622x->irq_gpio), aw8622x);
	aw_info("exit");
	return 0;
}

static const struct i2c_device_id aw8622x_i2c_id[] = {
	{ AW8622X_I2C_NAME, 0 },
	{ }
};
MODULE_DEVICE_TABLE(i2c, aw8622x_i2c_id);

static const struct of_device_id aw8622x_dt_match[] = {
	{ .compatible = "awinic,aw8622x_haptic" },
	{ },
};

static struct i2c_driver aw8622x_i2c_driver = {
	.driver = {
		.name = AW8622X_I2C_NAME,
		.owner = THIS_MODULE,
		.of_match_table = of_match_ptr(aw8622x_dt_match),
	},
	.probe = aw8622x_i2c_probe,
	.remove = aw8622x_i2c_remove,
	.id_table = aw8622x_i2c_id,
};

static int __init aw8622x_i2c_init(void)
{
	int ret = 0;

	aw_info("aw8622x driver version %s", AW8622X_DRIVER_VERSION);

	ret = i2c_add_driver(&aw8622x_i2c_driver);
	if (ret) {
		aw_err("fail to add aw8622x device into i2c");
		return ret;
	}

	return 0;
}

late_initcall(aw8622x_i2c_init);

static void __exit aw8622x_i2c_exit(void)
{
	i2c_del_driver(&aw8622x_i2c_driver);
}
module_exit(aw8622x_i2c_exit);

MODULE_DESCRIPTION("AW8622X Input Haptic Driver");
MODULE_LICENSE("GPL v2");
