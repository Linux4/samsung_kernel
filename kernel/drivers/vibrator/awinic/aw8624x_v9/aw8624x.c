// SPDX-License-Identifier: GPL-2.0
/*
 * File: aw8624x.c
 *
 * Author: <wangzhi@awinic.com>
 *
 * Copyright (c) 2023 AWINIC Technology CO., LTD
 *
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
#include <linux/uaccess.h>
#include <linux/power_supply.h>
#include <linux/vmalloc.h>
#include <linux/pm_qos.h>
#include <linux/fs.h>
#include <linux/soc/qcom/smem.h>

#include "aw8624x.h"
#include "aw8624x_reg.h"
#include "aw8624x_rtp_array.h"

#if IS_ENABLED(CONFIG_SEC_ABC)
#include <linux/sti/abc_common.h>
#endif

/******************************************************
 *
 * Marco
 *
 ******************************************************/
#define AW8624X_DRIVER_VERSION "v0.0.0.5"

static inline struct aw8624x *aw8624x_input_get_drvdata(struct input_dev *dev);
static uint32_t aw8624x_f0_cali_data;
module_param(aw8624x_f0_cali_data, uint, 0644);
MODULE_PARM_DESC(aw8624x_f0_cali_data, "aw8624x f0 calibration value");
static uint32_t aw8624x_f0_data;
module_param(aw8624x_f0_data, uint, 0644);
MODULE_PARM_DESC(aw8624x_f0_data, "aw8624x f0 value");
static uint32_t aw8624x_cont_f0_data;
module_param(aw8624x_cont_f0_data, uint, 0644);
MODULE_PARM_DESC(aw8624x_cont_f0_data, "aw8624x cont_f0 value");
static uint32_t bemf_peak[5];
module_param_array(bemf_peak, uint, NULL, 0644);
MODULE_PARM_DESC(bemf_peak, "aw8624x bemf peak value");

static char *aw8624x_ram_name = "aw8624x_haptic.bin";
static void aw8624x_sw_reset(struct aw8624x *aw8624x);
static int aw8624x_offset_cali(struct aw8624x *aw8624x);
static int aw8624x_ram_update(struct aw8624x *aw8624x);
static void aw8624x_interrupt_setup(struct aw8624x *aw8624x);
static void aw8624x_misc_para_init(struct aw8624x *aw8624x);
static void aw8624x_protect_config(struct aw8624x *aw8624x, uint8_t prtime, uint8_t prlvl);
static void aw8624x_vbat_mode_config(struct aw8624x *aw8624x, uint8_t flag);
static void aw8624x_raminit(struct aw8624x *aw8624x, bool flag);

bool standby_detected = false;
/*********************************************************
 *
 * I2C Read/Write
 *
 *********************************************************/
static int aw8624x_i2c_read(struct aw8624x *aw8624x, uint8_t reg_addr, uint8_t *reg_data)
{
	int ret = -1;
	uint8_t cnt = 0;

	while (cnt < AW8624X_I2C_RETRIES) {
		ret = i2c_smbus_read_byte_data(aw8624x->i2c, reg_addr);
		if (ret < 0) {
			aw_err("i2c_read addr=0x%02X, cnt=%u error=%d", reg_addr, cnt, ret);
		} else {
			*reg_data = ret;
			break;
		}
		cnt++;
		usleep_range(2000, 2500);
	}
	return ret;
}

static int aw8624x_i2c_reads(struct aw8624x *aw8624x, uint8_t reg_addr, uint8_t *buf, uint32_t len)
{
	int ret = -1;
	struct i2c_msg msg[] = {
		[0] = {
			.addr = aw8624x->i2c->addr,
			.flags = 0,
			.len = sizeof(uint8_t),
			.buf = &reg_addr,
			},
		[1] = {
			.addr = aw8624x->i2c->addr,
			.flags = I2C_M_RD,
			.len = len,
			.buf = buf,
			},
	};

	ret = i2c_transfer(aw8624x->i2c->adapter, msg, ARRAY_SIZE(msg));
	if (ret < 0) {
		aw_err("transfer failed.");
		return ret;
	} else if (ret != AW8624X_I2C_READ_MSG_NUM) {
		aw_err("transfer failed(size error).");
		return -ENXIO;
	}

	return ret;
}

static int aw8624x_i2c_write(struct aw8624x *aw8624x,
			uint8_t reg_addr, uint8_t reg_data)
{
	int ret = -1;
	uint8_t cnt = 0;

	while (cnt < AW8624X_I2C_RETRIES) {
		ret = i2c_smbus_write_byte_data(aw8624x->i2c, reg_addr,
						reg_data);
		if (ret < 0) {
			aw_err("i2c_write addr=0x%02X, data=0x%02X, cnt=%u, error=%d",
				reg_addr, reg_data, cnt, ret);
		} else {
			break;
		}
		cnt++;
		usleep_range(2000, 2500);
	}
	return ret;
}

static int aw8624x_i2c_writes(struct aw8624x *aw8624x, uint8_t reg_addr, uint8_t *buf, uint32_t len)
{
	uint8_t *data = NULL;
	int ret = -1;

	data = kmalloc(len + 1, GFP_KERNEL);
	if (data == NULL)
		return -ENOMEM;

	data[0] = reg_addr;
	memcpy(&data[1], buf, len);
	ret = i2c_master_send(aw8624x->i2c, data, len + 1);
	if (ret < 0)
		aw_err("i2c master send err.");
	kfree(data);

	return ret;
}

static int aw8624x_i2c_write_bits(struct aw8624x *aw8624x, uint8_t reg_addr, uint32_t mask,
				uint8_t reg_data)
{
	uint8_t reg_val = 0;
	int ret = -1;

	ret = aw8624x_i2c_read(aw8624x, reg_addr, &reg_val);
	if (ret < 0) {
		aw_err("i2c read error, ret=%d", ret);
		return ret;
	}
	reg_val &= mask;
	reg_val |= (reg_data & (~mask));
	ret = aw8624x_i2c_write(aw8624x, reg_addr, reg_val);
	if (ret < 0) {
		aw_err("i2c write error, ret=%d", ret);
		return ret;
	}

	return 0;
}

/*
 * avoid CPU sleep to prevent RTP data transmission interruption
 */
static void pm_qos_enable(struct aw8624x *aw8624x, bool enable)
{
#ifdef KERNEL_OVER_5_10
	if (enable) {
		if (!cpu_latency_qos_request_active(&aw8624x->pm_qos_req_vb))
			cpu_latency_qos_add_request(&aw8624x->pm_qos_req_vb, CPU_LATENCY_QOC_VALUE);
	} else {
		if (cpu_latency_qos_request_active(&aw8624x->pm_qos_req_vb))
			cpu_latency_qos_remove_request(&aw8624x->pm_qos_req_vb);
	}
#else
	if (enable) {
		if (!pm_qos_request_active(&aw8624x->pm_qos_req_vb))
			pm_qos_add_request(&aw8624x->pm_qos_req_vb, PM_QOS_CPU_DMA_LATENCY,
					   AW8624X_PM_QOS_VALUE_VB);
	} else {
		if (pm_qos_request_active(&aw8624x->pm_qos_req_vb))
			pm_qos_remove_request(&aw8624x->pm_qos_req_vb);
	}
#endif
}

/*
 * get fifo addr
 */
static void aw8624x_get_fifo_addr(struct aw8624x *aw8624x)
{
	uint8_t ae_addr_h = 0;
	uint8_t af_addr_h = 0;
	uint8_t ae_addr_l = 0;
	uint8_t af_addr_l = 0;
	uint8_t val[3] = {0};

	aw8624x_i2c_reads(aw8624x, AW8624X_REG_RTPCFG3, val, 3);
	ae_addr_h = ((val[0]) & AW8624X_BIT_RTPCFG3_FIFO_AEH) >> 4;
	ae_addr_l = val[1];
	af_addr_h = ((val[0]) & AW8624X_BIT_RTPCFG3_FIFO_AFH);
	af_addr_l = val[2];
	aw_info("almost_empty_threshold = %u,almost_full_threshold = %u",
		(uint16_t)((ae_addr_h << 8) | ae_addr_l),
		(uint16_t)((af_addr_h << 8) | af_addr_l));
}

/*
 * get the peak value of bemf after playback is stopped
 */
static void aw8624x_get_bemf_peak(struct aw8624x *aw8624x, uint32_t *peak)
{
	uint8_t reg[2] = {0};

	aw8624x_i2c_read(aw8624x, AW8624X_REG_CONTRD15, &reg[0]);
	aw8624x_i2c_read(aw8624x, AW8624X_REG_CONTRD20, &reg[1]);
	reg[1] = reg[1] & (~AW8624X_BIT_CONTRD20_BEMF_PEAK1_L_MASK);
	peak[0] = (reg[0] << 2) | reg[1];
	aw8624x_i2c_read(aw8624x, AW8624X_REG_CONTRD16, &reg[0]);
	aw8624x_i2c_read(aw8624x, AW8624X_REG_CONTRD20, &reg[1]);
	reg[1] = reg[1] & (~AW8624X_BIT_CONTRD20_BEMF_PEAK2_L_MASK);
	peak[1] = (reg[0] << 2) | (reg[1] >> 2);
	aw8624x_i2c_read(aw8624x, AW8624X_REG_CONTRD17, &reg[0]);
	aw8624x_i2c_read(aw8624x, AW8624X_REG_CONTRD20, &reg[1]);
	reg[1] = reg[1] & (~AW8624X_BIT_CONTRD20_BEMF_PEAK3_L_MASK);
	peak[2] = (reg[0] << 2) | (reg[1] >> 4);
	aw8624x_i2c_read(aw8624x, AW8624X_REG_CONTRD18, &reg[0]);
	aw8624x_i2c_read(aw8624x, AW8624X_REG_CONTRD20, &reg[1]);
	reg[1] = reg[1] & (~AW8624X_BIT_CONTRD20_BEMF_PEAK4_L_MASK);
	peak[3] = (reg[0] << 2) | (reg[1] >> 6);
	aw8624x_i2c_read(aw8624x, AW8624X_REG_CONTRD19, &reg[0]);
	aw8624x_i2c_read(aw8624x, AW8624X_REG_CONTRD21, &reg[1]);
	reg[1] = reg[1] & (~AW8624X_BIT_CONTRD21_BEMF_PEAK5_L_MASK);
	peak[4] = (reg[0] << 2) | reg[1];
}

/*
 * set fifo addr
 */
static void aw8624x_set_fifo_addr(struct aw8624x *aw8624x)
{
	uint8_t ae_addr_h = 0;
	uint8_t af_addr_h = 0;
	uint8_t val[3] = {0};

	aw_info("enter");
	ae_addr_h = AW8624X_SET_AEADDR_H(aw8624x->ram.base_addr);
	af_addr_h = AW8624X_SET_AFADDR_H(aw8624x->ram.base_addr);
	val[0] = ae_addr_h | af_addr_h;
	val[1] = AW8624X_SET_AEADDR_L(aw8624x->ram.base_addr);
	val[2] = AW8624X_SET_AFADDR_L(aw8624x->ram.base_addr);
	aw8624x_i2c_writes(aw8624x, AW8624X_REG_RTPCFG3, val, 3);
}

/*
 * set base addr: Critical address of FIFO area and Wareform area
 */
static void aw8624x_set_base_addr(struct aw8624x *aw8624x)
{
	uint8_t val = 0;
	uint32_t base_addr = 0;

	aw_info("enter");
	base_addr = aw8624x->ram.base_addr;
	aw8624x_i2c_write_bits(aw8624x, AW8624X_REG_RTPCFG1, AW8624X_BIT_RTPCFG1_ADDRH_MASK,
				 (uint8_t)AW8624X_SET_BASEADDR_H(base_addr));
	val = AW8624X_SET_BASEADDR_L(base_addr);
	aw8624x_i2c_write(aw8624x, AW8624X_REG_RTPCFG2, val);
}

/*
 * set ram addr pointer
 */
static void aw8624x_set_ram_addr(struct aw8624x *aw8624x)
{
	uint8_t val[2] = {0};
	uint32_t base_addr = aw8624x->ram.base_addr;

	aw_info("enter");
	val[0] = AW8624X_SET_RAMADDR_H(base_addr);
	val[1] = AW8624X_SET_RAMADDR_L(base_addr);
	aw8624x_i2c_writes(aw8624x, AW8624X_REG_RAMADDRH, val, 2);
}

/*
 * set sequence of RAM mode
 */
static void aw8624x_set_wav_seq(struct aw8624x *aw8624x, uint8_t wav, uint8_t seq)
{
	aw8624x_i2c_write(aw8624x, AW8624X_REG_WAVCFG1 + wav, seq);
}

/*
 * set loop times of each sequence
 */
static void aw8624x_set_wav_loop(struct aw8624x *aw8624x, uint8_t wav, uint8_t loop)
{
	uint8_t tmp = 0;

	if (wav % 2) {
		tmp = loop << 0;
		aw8624x_i2c_write_bits(aw8624x, AW8624X_REG_WAVCFG9 + (wav / 2),
					 AW8624X_BIT_WAVLOOP_SEQ_EVEN_MASK, tmp);
	} else {
		tmp = loop << 4;
		aw8624x_i2c_write_bits(aw8624x, AW8624X_REG_WAVCFG9 + (wav / 2),
					 AW8624X_BIT_WAVLOOP_SEQ_ODD_MASK, tmp);
	}
}

/*
 * set seq0 infinite loop playback
 */
static void aw8624x_set_repeat_seq(struct aw8624x *aw8624x, uint8_t seq)
{
	aw_info("repeat wav seq %u", seq);
	aw8624x_set_wav_seq(aw8624x, 0x00, seq);
	aw8624x_set_wav_seq(aw8624x, 0x01, 0x00);
	aw8624x_set_wav_loop(aw8624x, 0x00, 0x0f);
}

/*
 * get fifo almost full int status
 */
static uint8_t aw8624x_rtp_get_fifo_afs(struct aw8624x *aw8624x)
{
	uint8_t reg_val = 0;

	aw8624x_i2c_read(aw8624x, AW8624X_REG_SYSST, &reg_val);
	reg_val &= AW8624X_BIT_SYSST_FF_AFS;
	reg_val >>= 3;

	return reg_val;
}

/*
 * almost empty int config
 */
static void aw8624x_set_rtp_aei(struct aw8624x *aw8624x, bool flag)
{
	if (flag) {
		aw8624x_i2c_write_bits(aw8624x, AW8624X_REG_SYSINTM,
				       AW8624X_BIT_SYSINTM_FF_AEM_MASK,
				       AW8624X_BIT_SYSINTM_FF_AEM_ON);
	} else {
		aw8624x_i2c_write_bits(aw8624x, AW8624X_REG_SYSINTM,
				       AW8624X_BIT_SYSINTM_FF_AEM_MASK,
				       AW8624X_BIT_SYSINTM_FF_AEM_OFF);
	}
}

/*
 * stop and enter standby
 */
static void aw8624x_play_stop(struct aw8624x *aw8624x)
{
	bool force_flag = true;
	uint8_t val = 0, reg_val = 0;
	int cnt = 40;

	aw_info("enter");
	aw8624x->play_mode = AW8624X_STANDBY_MODE;
	aw8624x_raminit(aw8624x, true);				//internal CLK enable
	aw8624x_i2c_write_bits(aw8624x, AW8624X_REG_PLAYCFG3,
					AW8624X_BIT_PLAYCFG3_PLAY_MODE_MASK,
					AW8624X_BIT_PLAYCFG3_PLAY_MODE_STOP);
	val = AW8624X_BIT_PLAYCFG4_GO_ON;
	aw8624x_i2c_write(aw8624x, AW8624X_REG_PLAYCFG4, val);
	aw8624x_raminit(aw8624x, false);
	while (cnt) {
		aw8624x_i2c_read(aw8624x, AW8624X_REG_GLBRD5, &val);
		if ((val & AW8624X_BIT_GLBRD5_STATE_MASK) == AW8624X_BIT_GLBRD5_STATE_STANDBY) {
			force_flag = false;
			aw_info("entered standby! glb_state=0x%02X", val);
			break;
		}
		cnt--;
		usleep_range(2000, 2500);
	}
	if (force_flag) {
		aw_err("force to enter standby mode!");
		aw8624x_i2c_write_bits(aw8624x, AW8624X_REG_SYSCTRL2,
					 AW8624X_BIT_SYSCTRL2_STANDBY_MASK,
					 AW8624X_BIT_SYSCTRL2_STANDBY_ON);
		aw8624x_i2c_write_bits(aw8624x, AW8624X_REG_SYSCTRL2,
					 AW8624X_BIT_SYSCTRL2_STANDBY_MASK,
					 AW8624X_BIT_SYSCTRL2_STANDBY_OFF);
		aw8624x_i2c_read(aw8624x, AW8624X_REG_SYSCTRL2, &reg_val);
		aw_info("reg:0x44=0x%02X", reg_val);
	}
}

/*
 * set waveform sample rate
 */
static void aw8624x_set_sample_rate(struct aw8624x *aw8624x, uint8_t mode)
{
	switch (mode) {
	case AW8624X_PWM_48K:
		aw8624x_i2c_write_bits(aw8624x, AW8624X_REG_SYSCTRL2,
					 AW8624X_BIT_SYSCTRL2_WAVDAT_MODE_MASK,
					 AW8624X_BIT_SYSCTRL2_RATE_48K);
		break;
	case AW8624X_PWM_24K:
		aw8624x_i2c_write_bits(aw8624x, AW8624X_REG_SYSCTRL2,
					 AW8624X_BIT_SYSCTRL2_WAVDAT_MODE_MASK,
					 AW8624X_BIT_SYSCTRL2_RATE_24K);
		break;
	case AW8624X_PWM_12K:
		aw8624x_i2c_write_bits(aw8624x, AW8624X_REG_SYSCTRL2,
					 AW8624X_BIT_SYSCTRL2_WAVDAT_MODE_MASK,
					 AW8624X_BIT_SYSCTRL2_RATE_12K);
		break;
	default:
		break;
	}
}

/*
 * set auto brake
 */
static void aw8624x_auto_brk_config(struct aw8624x *aw8624x, uint8_t flag)
{
	if (flag) {
		aw8624x_i2c_write_bits(aw8624x, AW8624X_REG_PLAYCFG3,
					 AW8624X_BIT_PLAYCFG3_BRK_EN_MASK,
					 AW8624X_BIT_PLAYCFG3_BRK_ENABLE);
	} else {
		aw8624x_i2c_write_bits(aw8624x, AW8624X_REG_PLAYCFG3,
					 AW8624X_BIT_PLAYCFG3_BRK_EN_MASK,
					 AW8624X_BIT_PLAYCFG3_BRK_DISABLE);
	}
}

/*
 * wake up chip clock
 */
static void aw8624x_raminit(struct aw8624x *aw8624x, bool flag)
{
	if (flag) {
		aw8624x_i2c_write_bits(aw8624x, AW8624X_REG_SYSCTRL1,
					 AW8624X_BIT_SYSCTRL1_RAMINIT_MASK,
					 AW8624X_BIT_SYSCTRL1_RAMINIT_ON);
		usleep_range(1000, 1050);
	} else {
		aw8624x_i2c_write_bits(aw8624x, AW8624X_REG_SYSCTRL1,
					 AW8624X_BIT_SYSCTRL1_RAMINIT_MASK,
					 AW8624X_BIT_SYSCTRL1_RAMINIT_OFF);
	}
}

/*
 * set play mode
 */
static void aw8624x_play_mode(struct aw8624x *aw8624x, uint8_t play_mode)
{
	switch (play_mode) {
	case AW8624X_STANDBY_MODE:
		aw_info("enter standby mode");
		aw8624x->play_mode = AW8624X_STANDBY_MODE;
		aw8624x_play_stop(aw8624x);
		break;
	case AW8624X_RAM_MODE:
		aw_info("enter ram mode");
		aw8624x->play_mode = AW8624X_RAM_MODE;
		aw8624x_i2c_write_bits(aw8624x, AW8624X_REG_PLAYCFG3,
					 AW8624X_BIT_PLAYCFG3_PLAY_MODE_MASK,
					 AW8624X_BIT_PLAYCFG3_PLAY_MODE_RAM);
		break;
	case AW8624X_RAM_LOOP_MODE:
		aw_info("enter ram loop mode");
		aw8624x->play_mode = AW8624X_RAM_LOOP_MODE;
		aw8624x_i2c_write_bits(aw8624x, AW8624X_REG_PLAYCFG3,
					 AW8624X_BIT_PLAYCFG3_PLAY_MODE_MASK,
					 AW8624X_BIT_PLAYCFG3_PLAY_MODE_RAM);
		break;
	case AW8624X_CONT_MODE:
		aw_info("enter cont mode");
		aw8624x->play_mode = AW8624X_CONT_MODE;
		aw8624x_i2c_write_bits(aw8624x, AW8624X_REG_PLAYCFG3,
					 AW8624X_BIT_PLAYCFG3_PLAY_MODE_MASK,
					 AW8624X_BIT_PLAYCFG3_PLAY_MODE_CONT);
		break;
	case AW8624X_RTP_MODE:
		aw_info("enter rtp mode");
		aw8624x->play_mode = AW8624X_RTP_MODE;
		aw8624x_i2c_write_bits(aw8624x, AW8624X_REG_PLAYCFG3,
					 AW8624X_BIT_PLAYCFG3_PLAY_MODE_MASK,
					 AW8624X_BIT_PLAYCFG3_PLAY_MODE_RTP);
		break;
	case AW8624X_TRIG_MODE:
		aw_info("enter trig mode");
		aw8624x->play_mode = AW8624X_TRIG_MODE;
		aw8624x_i2c_write_bits(aw8624x, AW8624X_REG_PLAYCFG3,
					 AW8624X_BIT_PLAYCFG3_PLAY_MODE_MASK,
					 AW8624X_BIT_PLAYCFG3_PLAY_MODE_RAM);
		break;
	default:
		aw_err("play mode %u error", play_mode);
		break;
	}
}

/*
 * gain setting for waveform data
 */
static void aw8624x_set_gain(struct aw8624x *aw8624x, uint8_t gain)
{
	aw8624x_i2c_write(aw8624x, AW8624X_REG_PLAYCFG2, gain);
}

/*
 * get vbat value
 */
static void aw8624x_get_vbat(struct aw8624x *aw8624x)
{
	uint8_t reg_val = 0;
	uint32_t vbat_code = 0;

	aw_info("enter");
	aw8624x_play_stop(aw8624x);
	aw8624x_raminit(aw8624x, true);
	aw8624x_i2c_write_bits(aw8624x, AW8624X_REG_DETCFG2, AW8624X_BIT_DETCFG2_VBAT_GO_MASK,
				 AW8624X_BIT_DETCFG2_VABT_GO_ON);
	usleep_range(2000, 2500);
	aw8624x_i2c_read(aw8624x, AW8624X_REG_DET_VBAT, &reg_val);
	vbat_code = (vbat_code | reg_val) << 2;
	aw8624x_i2c_read(aw8624x, AW8624X_REG_DET_LO, &reg_val);
	vbat_code = vbat_code | ((reg_val & AW8624X_BIT_DET_LO_VBAT) >> 4);
	aw8624x_raminit(aw8624x, false);
	aw8624x->vbat = AW8624X_VBAT_FORMULA(vbat_code);
	if (aw8624x->vbat > AW8624X_VBAT_MAX) {
		aw8624x->vbat = AW8624X_VBAT_MAX;
		aw_info("vbat max limit = %dmV", aw8624x->vbat);
	}
	if (aw8624x->vbat < AW8624X_VBAT_MIN) {
		aw8624x->vbat = AW8624X_VBAT_MIN;
		aw_info("vbat min limit = %dmV", aw8624x->vbat);
	}
	aw_info("vbat = %umV, vbat_code = 0x%02X", aw8624x->vbat, vbat_code);
}

/*
 * the d2s_gain value corresponding to the d2s_gain register value
 */
static int aw8624x_select_d2s_gain(uint8_t reg)
{
	int d2s_gain = 0;

	switch (reg) {
	case AW8624X_BIT_SYSCTRL5_D2S_GAIN_1:
		d2s_gain = 1;
		break;
	case AW8624X_BIT_SYSCTRL5_D2S_GAIN_2:
		d2s_gain = 2;
		break;
	case AW8624X_BIT_SYSCTRL5_D2S_GAIN_4:
		d2s_gain = 4;
		break;
	case AW8624X_BIT_SYSCTRL5_D2S_GAIN_5:
		d2s_gain = 5;
		break;
	case AW8624X_BIT_SYSCTRL5_D2S_GAIN_8:
		d2s_gain = 8;
		break;
	case AW8624X_BIT_SYSCTRL5_D2S_GAIN_10:
		d2s_gain = 10;
		break;
	case AW8624X_BIT_SYSCTRL5_D2S_GAIN_20:
		d2s_gain = 20;
		break;
	case AW8624X_BIT_SYSCTRL5_D2S_GAIN_40:
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
static void aw8624x_get_lra_resistance(struct aw8624x *aw8624x)
{
	uint8_t reg_val = 0;
	uint8_t d2s_gain_code = 0x05;
	uint8_t d2s_gain_bak = 0;
	uint32_t lra_code = 0;
	int d2s_gain = 0;

	aw8624x_i2c_read(aw8624x, AW8624X_REG_SYSCTRL5, &reg_val);
	d2s_gain_bak = (~AW8624X_BIT_SYSCTRL5_D2S_GAIN_MASK) & reg_val;
	aw8624x_i2c_write_bits(aw8624x, AW8624X_REG_SYSCTRL5,
					AW8624X_BIT_SYSCTRL5_D2S_GAIN_MASK,
					d2s_gain_code);
	d2s_gain = aw8624x_select_d2s_gain(d2s_gain_code);
	if (d2s_gain < 0) {
		aw_err("d2s_gain is error");
		return;
	}
	aw8624x_raminit(aw8624x, true);
	/* enter standby mode */
	aw8624x_play_stop(aw8624x);
	usleep_range(2000, 2500);
	aw8624x_i2c_write_bits(aw8624x, AW8624X_REG_SYSCTRL2, AW8624X_BIT_SYSCTRL2_STANDBY_MASK,
				 AW8624X_BIT_SYSCTRL2_STANDBY_OFF);
	aw8624x_i2c_write_bits(aw8624x, AW8624X_REG_DETCFG1, AW8624X_BIT_DETCFG1_RL_OS_MASK,
				 AW8624X_BIT_DETCFG1_RL);
	aw8624x_i2c_write_bits(aw8624x, AW8624X_REG_DETCFG2, AW8624X_BIT_DETCFG2_DIAG_GO_MASK,
				 AW8624X_BIT_DETCFG2_DIAG_GO_ON);
	usleep_range(30000, 30500);
	aw8624x_i2c_read(aw8624x, AW8624X_REG_DET_RL, &reg_val);
	lra_code = (lra_code | reg_val) << 2;
	aw8624x_i2c_read(aw8624x, AW8624X_REG_DET_LO, &reg_val);
	lra_code = lra_code | (reg_val & AW8624X_BIT_DET_LO_RL);
	aw8624x_raminit(aw8624x, false);
	aw8624x->lra = AW8624X_RL_FORMULA(lra_code, d2s_gain);
	aw_info("aw8624x->lra = %u", aw8624x->lra);
	/* restore d2s_gain */
	aw8624x_i2c_write_bits(aw8624x, AW8624X_REG_SYSCTRL5,
					AW8624X_BIT_SYSCTRL5_D2S_GAIN_MASK,
					d2s_gain_bak);
}

/*
 * set vbat compensation mode
 */
static void aw8624x_ram_vbat_comp(struct aw8624x *aw8624x, bool flag)
{
	int temp_gain = 0;

	aw_info("enter");
	if (flag) {
		if (aw8624x->ram_vbat_comp == AW8624X_RAM_VBAT_COMP_ENABLE) {
			aw8624x_get_vbat(aw8624x);
			temp_gain = aw8624x->gain * AW8624X_VBAT_REFER / aw8624x->vbat;
			if (temp_gain > (AW8624X_DEFAULT_GAIN *
					AW8624X_VBAT_REFER / AW8624X_VBAT_MIN)) {
				temp_gain = AW8624X_DEFAULT_GAIN *
					AW8624X_VBAT_REFER / AW8624X_VBAT_MIN;
				aw_info("gain limit=%d", temp_gain);
			}
			aw8624x_set_gain(aw8624x, (uint8_t)temp_gain);
			aw_info("ram vbat comp open, set_gain 0x%02X", temp_gain);
		} else {
			aw8624x_set_gain(aw8624x, aw8624x->gain);
			aw_info("ram vbat comp close, set_gain 0x%02X", aw8624x->gain);
		}
	} else {
		aw8624x_set_gain(aw8624x, aw8624x->gain);
		aw_info("ram vbat comp close, set_gain 0x%02X", aw8624x->gain);
	}
}

void aw_reg_read(struct aw8624x *aw8624x)
{
	uint8_t reg_array[AW8624X_REG_MAX] = {0};
	int i = 0;

	aw8624x_i2c_reads(aw8624x, AW8624X_REG_ID, reg_array, AW8624X_REG_RTPDATA);
	aw8624x_i2c_reads(aw8624x, AW8624X_REG_RTPDATA + 1, &reg_array[AW8624X_REG_RTPDATA + 1],
					AW8624X_REG_RAMDATA - AW8624X_REG_RTPDATA - 1);
	aw8624x_i2c_reads(aw8624x, AW8624X_REG_RAMDATA + 1, &reg_array[AW8624X_REG_RAMDATA + 1],
					AW8624X_REG_ANACFG8 - AW8624X_REG_RAMDATA + 1);
	for (i = 0 ; i < AW8624X_REG_ANACFG8 + 2; i++) {
		aw_info("reg:0x%02X=0x%02X", AW8624X_REG_ID + i, reg_array[i]);
	}
}

void aw_reinit(struct aw8624x *aw8624x)
{
	int ret = 0;

	aw_info("enter");

	ret = aw8624x_offset_cali(aw8624x);
	if (ret < 0)
		aw8624x_sw_reset(aw8624x);

	/* aw8624x irq */
	aw8624x_interrupt_setup(aw8624x);
	aw8624x_play_mode(aw8624x, AW8624X_STANDBY_MODE);
	aw8624x_set_sample_rate(aw8624x, AW8624X_PWM_12K);
	/* misc value init */
	aw8624x_misc_para_init(aw8624x);
	/* set motor protect */
	aw8624x_protect_config(aw8624x, AW8624X_PWMCFG4_PRTIME_DEFAULT_VALUE,
			       AW8624X_BIT_PWMCFG3_PRLVL_DEFAULT_VALUE);
	/* vbat compensation */
	aw8624x_vbat_mode_config(aw8624x, AW8624X_CONT_VBAT_HW_COMP_MODE);
	aw8624x_set_base_addr(aw8624x);
	aw8624x_set_fifo_addr(aw8624x);
}

/*
 * start or stop playback wareform
 */
static void aw8624x_play_go(struct aw8624x *aw8624x, bool flag)
{
	uint8_t val = 0;

	aw_info("enter flag: %d", flag);
	if (flag == true) {
		val = AW8624X_BIT_PLAYCFG4_GO_ON;
		aw8624x_i2c_write(aw8624x, AW8624X_REG_PLAYCFG4, val);
	} else {
		val = AW8624X_BIT_PLAYCFG4_STOP_ON;
		aw8624x_i2c_write(aw8624x, AW8624X_REG_PLAYCFG4, val);
	}
	usleep_range(500, 600);
	aw8624x_i2c_read(aw8624x, AW8624X_REG_GLBRD5, &val);
	aw_info("reg:0x%02X=0x%02X", AW8624X_REG_GLBRD5, val);
	if (val == 0) {
		aw8624x_i2c_write_bits(aw8624x, AW8624X_REG_SYSCTRL2,
						AW8624X_BIT_SYSCTRL2_WAKE_MASK,
						AW8624X_BIT_SYSCTRL2_WAKE_ON);
		usleep_range(500, 600);
		aw8624x_i2c_read(aw8624x, AW8624X_REG_GLBRD5, &val);
		aw_info("reg:0x%02X=0x%02X", AW8624X_REG_GLBRD5, val);
		aw8624x_i2c_write_bits(aw8624x, AW8624X_REG_SYSCTRL2,
						AW8624X_BIT_SYSCTRL2_WAKE_MASK,
						AW8624X_BIT_SYSCTRL2_WAKE_OFF);
		if (val == 0) {
			aw_reg_read(aw8624x);
			aw8624x_sw_reset(aw8624x);
			aw_reinit(aw8624x);
			standby_detected = true;
#if IS_ENABLED(CONFIG_USB_USING_ADVANCED_USBLOG)
			printk_usb(NOTIFY_PRINTK_USB_SNAPSHOT, "Motor IC Standby detected\n");
#endif
		}
	} else if (standby_detected) {
		standby_detected = false;
#if IS_ENABLED(CONFIG_SEC_ABC)
		sec_abc_send_event("MODULE=vib@WARN=standby_error_recovered");
#endif
	}
}

#ifdef AW_CHECK_RAM_DATA
static int aw8624x_check_ram_data(struct aw8624x *aw8624x, struct aw8624x_container *aw8624x_fw)
{
	int i = 0;
	int j = 0;
	int len = 0;
	uint8_t ram_data[AW8624X_RAMDATA_RD_BUFFER_SIZE] = {0};

	aw8624x_set_ram_addr(aw8624x);
	i = aw8624x->ram.ram_shift;
	while (i < aw8624x_fw->len) {
		if ((aw8624x_fw->len - i) < AW8624X_RAMDATA_RD_BUFFER_SIZE)
			len = aw8624x_fw->len - i;
		else
			len = AW8624X_RAMDATA_RD_BUFFER_SIZE;

		aw8624x_i2c_reads(aw8624x, AW8624X_REG_RAMDATA, ram_data, len);
		for (j = 0; j < len; j++) {
			if (ram_data[j] != aw8624x_fw->data[i + j]) {
				aw_err("check ramdata error, addr=0x%04X, ram_data=0x%02X, file_data=0x%02X",
					j, ram_data[j], aw8624x_fw->data[i + j]);
				return -ERANGE;
			}
		}
		i += len;
	}

	return 0;
}
#endif

/*
 * Write calibration value to register(just update calibration value, not calibration process)
 * WRTIE_ZERO: no calibration
 * F0_CALI   : Calibrate all patterns of waveform to match the LRA's F0
 * OSC_CALI  : Calibrate the frequency of AW8624X to match rtp frequency
 * F0_CALI and OSC_CALI can not exit at the same time
 */
static void aw8624x_upload_lra(struct aw8624x *aw8624x, uint8_t flag)
{
	uint8_t reg_val;

	switch (flag) {
	case AW8624X_WRITE_ZERO:
		aw_info("write zero to trim_lra!");
		reg_val = 0x00;
		break;
	case AW8624X_F0_CALI_LRA:
		aw_info("write f0_cali_data to trim_lra = 0x%02X", aw8624x->f0_cali_data);
		reg_val = aw8624x->f0_cali_data;
		break;
	case AW8624X_OSC_CALI_LRA:
		aw_info("write osc_cali_data to trim_lra = 0x%02X", aw8624x->osc_cali_data);
		reg_val = aw8624x->osc_cali_data;
		break;
	default:
		aw_err("flag is error");
		reg_val = 0x00;
		break;
	}
	aw8624x_i2c_write_bits(aw8624x, AW8624X_REG_TRIMCFG3, AW8624X_BIT_TRIMCFG3_TRIM_LRA_MASK,
					reg_val);
}

/*
 * judge real f0 whether is out of calibrate range
 */
static int aw8624x_judge_cali_range(struct aw8624x *aw8624x)
{
	uint32_t f0_cali_min = 0;
	uint32_t f0_cali_max = 0;

	aw_info("enter");
	f0_cali_min = aw8624x->dts_info.f0_pre * (100 - aw8624x->dts_info.f0_cali_percent) / 100;
	f0_cali_max = aw8624x->dts_info.f0_pre * (100 + aw8624x->dts_info.f0_cali_percent) / 100;

	aw_info("f0_pre = %u, f0_cali_min = %u, f0_cali_max = %u, f0 = %u",
		aw8624x->dts_info.f0_pre, f0_cali_min, f0_cali_max, aw8624x->f0);

	if (aw8624x->f0 < f0_cali_min) {
		aw_err("f0 is too small, fitting_f0 = %u!", aw8624x->f0);
		return -ERANGE;
	}

	if (aw8624x->f0 > f0_cali_max) {
		aw_err("f0 is too large, fitting_f0 = %u!", aw8624x->f0);
		return -ERANGE;
	}

	return 0;
}

static void aw8624x_calculate_cali_data(struct aw8624x *aw8624x)
{
	char f0_cali_lra = 0;
	int f0_cali_step = 0;

	f0_cali_step = 100000 * ((int)aw8624x->f0 - (int)aw8624x->dts_info.f0_pre) /
				((int)aw8624x->dts_info.f0_pre * 24);
	aw_info("f0_cali_step=%d", f0_cali_step);
	if (f0_cali_step >= 0) {	/*f0_cali_step >= 0 */
		if (f0_cali_step % 10 >= 5)
			f0_cali_step = 64 + (f0_cali_step / 10 + 1);
		else
			f0_cali_step = 64 + f0_cali_step / 10;
	} else {	/* f0_cali_step < 0 */
		if (f0_cali_step % 10 <= -5)
			f0_cali_step = 64 + (f0_cali_step / 10 - 1);
		else
			f0_cali_step = 64 + f0_cali_step / 10;
	}
	if (f0_cali_step >= 64)
		f0_cali_lra = (char)f0_cali_step - 64;
	else
		f0_cali_lra = (char)f0_cali_step + 64;
	/* update cali step */
	aw8624x->f0_cali_data = f0_cali_lra;
	aw_info("f0_cali_data = 0x%02X", aw8624x->f0_cali_data);
}

#ifdef AW_READ_CONT_F0
/*
 * read cont_f0 register and calculate the frequency
 */
static int aw8624x_read_cont_f0(struct aw8624x *aw8624x)
{
	uint8_t val[2] = {0};
	uint32_t f0_reg = 0;

	aw_info("enter");
	aw8624x_i2c_reads(aw8624x, AW8624X_REG_CONTCFG13, val, 2);
	f0_reg = (f0_reg | val[0]) << 8;
	f0_reg |= (val[1] << 0);
	if (!f0_reg) {
		aw_err("didn't get cont f0 because f0_reg value is 0!");
		aw8624x->cont_f0 = 0;
		return -ERANGE;
	}
	aw8624x->cont_f0 = AW8624X_F0_FORMULA(f0_reg);
	aw_info("cont_f0 = %u", aw8624x->cont_f0);

	return 0;
}
#endif

/*
 * read lra_f0 register and calculate the frequency
 */
static int aw8624x_read_lra_f0(struct aw8624x *aw8624x)
{
	uint8_t val[2] = {0};
	uint32_t f0_reg = 0;

	aw_info("enter");
	/* F_LRA_F0 */
	aw8624x_i2c_reads(aw8624x, AW8624X_REG_CONTCFG11, val, 2);
	f0_reg = (f0_reg | val[0]) << 8;
	f0_reg |= (val[1] << 0);
	if (!f0_reg) {
		aw8624x->lra_f0 = 0;
		aw_err("didn't get lra f0 because f0_reg value is 0!");
		return -ERANGE;
	}
	aw8624x->lra_f0 = AW8624X_F0_FORMULA(f0_reg);
	aw_info("lra_f0 = %u", aw8624x->lra_f0);

	return 0;
}

/*
 * waiting for enter standby after start f0 det
 * read f0 after standby
 */
static int aw8624x_wait_standby_and_read_f0(struct aw8624x *aw8624x, int cnt)
{
	bool get_f0_flag = false;
	uint8_t val = 0;
	int ret = 0;

	while (cnt) {
		aw8624x_i2c_read(aw8624x, AW8624X_REG_GLBRD5, &val);
		if ((val & AW8624X_BIT_GLBRD5_STATE_MASK) == AW8624X_BIT_GLBRD5_STATE_STANDBY) {
			get_f0_flag = true;
			aw_info("entered standby! glb_state=0x%02X", val);
			break;
		}
		cnt--;
		aw_dbg("waitting for standby,glb_state=0x%02X", val);
		usleep_range(10000, 10500);
	}
	if (get_f0_flag) {
#ifdef AW_READ_CONT_F0
		ret = aw8624x_read_cont_f0(aw8624x);
		if (ret < 0)
			aw_err("read cont f0 is failed");
#endif
		ret = aw8624x_read_lra_f0(aw8624x);
		if (ret < 0)
			aw_err("read lra f0 is failed");

		aw8624x->f0 = (aw8624x->fitting_coeff * aw8624x->cont_f0 +
					(10 - aw8624x->fitting_coeff) * aw8624x->lra_f0) / 10;
		aw_info("fitting f0 = %u", aw8624x->f0);
	} else {
		ret = -ERANGE;
		aw_err("enter standby mode failed, stop reading f0!");
	}

	return ret;
}

/*
 * get frequency of LRA aftershock bemf
 */
static int aw8624x_get_f0(struct aw8624x *aw8624x)
{
	uint8_t brk_en_temp = 0;
	uint8_t state = 0;
	uint8_t val[3] = {0};
	int ret = 0;

	/* add VDD POR verify */
	aw8624x_i2c_read(aw8624x, AW8624X_REG_SYSST2, &state);
	if (!(state & AW8624X_BIT_SYSST2_DVDD_PORN)) {
		aw_err("VDD is NOT POWER ON!!!");
		return -ERANGE;
	}

	aw_info("enter");
	/* enter standby mode */
	aw8624x_play_stop(aw8624x);
	/* f0 calibrate work mode */
	aw8624x_play_mode(aw8624x, AW8624X_CONT_MODE);
	aw8624x_i2c_write_bits(aw8624x, AW8624X_REG_CONTCFG7, AW8624X_BIT_CONTCFG7_DRV_SWITCH_MASK,
			       AW8624X_BIT_CONTCFG7_DRV_SWITCH_DISABLE);
	/* enable f0 detect */
	aw8624x_i2c_write_bits(aw8624x, AW8624X_REG_CONTCFG1, AW8624X_BIT_CONTCFG1_PEAK_NUM_MASK,
			AW8624X_BIT_CONTCFG1_PEAK_NUM_3);
	/* cont config */
	aw8624x_i2c_write_bits(aw8624x, AW8624X_REG_CONTCFG6, AW8624X_BIT_CONTCFG6_TRACK_EN_MASK,
			AW8624X_BIT_CONTCFG6_TRACK_ENABLE);
	/* enable auto brake */
	aw8624x_i2c_read(aw8624x, AW8624X_REG_PLAYCFG3, &val[0]);
	brk_en_temp = AW8624X_BIT_PLAYCFG3_BRK & val[0];
	if (aw8624x->dts_info.is_enabled_auto_brk)
		aw8624x_auto_brk_config(aw8624x, true);
	else
		aw8624x_auto_brk_config(aw8624x, false);
	/* f0 driver level */
	aw8624x_i2c_write_bits(aw8624x, AW8624X_REG_CONTCFG6, AW8624X_BIT_CONTCFG6_DRV1_LVL_MASK,
			aw8624x->dts_info.cont_drv1_lvl);
	val[0] = aw8624x->dts_info.cont_drv2_lvl;
	aw8624x_i2c_write_bits(aw8624x, AW8624X_REG_CONTCFG7, AW8624X_BIT_CONTCFG7_DRV2_LVL_MASK,
			val[0]);
	val[0] = aw8624x->dts_info.cont_drv1_time;
	val[1] = aw8624x->dts_info.cont_drv2_time;
	aw8624x_i2c_writes(aw8624x, AW8624X_REG_CONTCFG8, val, 2);
	/* TRACK_MARGIN */
	if (!aw8624x->dts_info.cont_track_margin) {
		aw_err("dts_info.cont_track_margin = 0!");
	} else {
		val[0] = aw8624x->dts_info.cont_track_margin;
		aw8624x_i2c_write(aw8624x, AW8624X_REG_CONTCFG4, val[0]);
	}

	/* cont play go */
	aw8624x_play_go(aw8624x, true);
	usleep_range(20000, 20500);
	ret = aw8624x_wait_standby_and_read_f0(aw8624x, 200);
	/* disable f0 detect */
	aw8624x_i2c_write_bits(aw8624x, AW8624X_REG_CONTCFG1, AW8624X_BIT_CONTCFG1_PEAK_NUM_MASK,
			       AW8624X_BIT_CONTCFG1_PEAK_NUM_NO_F0_DET);
	/* recover auto break config */
	aw8624x_i2c_write_bits(aw8624x, AW8624X_REG_PLAYCFG3, AW8624X_BIT_PLAYCFG3_BRK_EN_MASK,
			       brk_en_temp);

	return ret;
}

/*
 * calibrate all patterns of waveform to match the LRA's F0,
 * and update it to the chip register.
 * The calibration value changes the OSC frequency of the chip.
 */
static int aw8624x_f0_cali(struct aw8624x *aw8624x)
{
	int ret = 0;

	aw8624x_upload_lra(aw8624x, AW8624X_WRITE_ZERO);
	if (aw8624x_get_f0(aw8624x)) {
		aw_err("get f0 error");
	} else {
		ret = aw8624x_judge_cali_range(aw8624x);
		if (ret < 0)
			aw8624x->f0 = aw8624x->dts_info.f0_pre;
		aw8624x_calculate_cali_data(aw8624x);
		aw8624x_upload_lra(aw8624x, AW8624X_F0_CALI_LRA);
	}
	/* restore standby work mode */
	aw8624x_play_stop(aw8624x);

	return ret;
}

/*
 * parse device tree
 */
static int aw8624x_parse_dt(struct device *dev, struct aw8624x *aw8624x, struct device_node *np)
{
	uint32_t val = 0;

	aw8624x->irq_gpio = of_get_named_gpio(np, "irq-gpio", 0);
	if (aw8624x->irq_gpio < 0)
		aw_err("no irq gpio provided.");
	else
		aw_info("irq gpio provide ok irq.");

	val = of_property_read_u32(np, "vib_f0_pre", &aw8624x->dts_info.f0_pre);
	if (val != 0)
		aw_err("aw8624x_vib_f0_pre not found");

	val = of_property_read_u32(np, "vib_f0_cali_percen", &aw8624x->dts_info.f0_cali_percent);
	if (val != 0)
		aw_err("aw8624x_vib_f0_cali_percent not found");

	val = of_property_read_u32(np, "vib_cont_drv1_lvl", &aw8624x->dts_info.cont_drv1_lvl);
	if (val != 0)
		aw_err("aw8624x_vib_cont_drv1_lvl not found");

	val = of_property_read_u32(np, "vib_cont_drv2_lvl", &aw8624x->dts_info.cont_drv2_lvl);
	if (val != 0)
		aw_err("aw8624x_vib_cont_drv2_lvl not found");

	val = of_property_read_u32(np, "vib_lra_vrms", &aw8624x->dts_info.lra_vrms);
	if (val != 0)
		aw_err("aw8624x_vib_lra_vrms not found");

	val = of_property_read_u32(np, "vib_cont_drv1_time", &aw8624x->dts_info.cont_drv1_time);
	if (val != 0)
		aw_err("aw8624x_vib_cont_drv1_time not found");

	val = of_property_read_u32(np, "vib_cont_drv2_time", &aw8624x->dts_info.cont_drv2_time);
	if (val != 0)
		aw_err("aw8624x_vib_cont_drv2_time not found");

	val = of_property_read_u32(np, "vib_cont_brk_gain", &aw8624x->dts_info.cont_brk_gain);
	if (val != 0)
		aw_err("aw8624x_vib_cont_brk_gain not found");

	val = of_property_read_u32(np, "vib_d2s_gain", &aw8624x->dts_info.d2s_gain);
	if (val != 0)
		aw_err("aw8624x_vib_d2s_gain not found");

	val = of_property_read_u32(np, "vib_f0_d2s_gain", &aw8624x->dts_info.f0_d2s_gain);
	if (val != 0)
		aw_err("aw8624x_vib_f0_d2s_gain not found");

	val = of_property_read_u32(np, "vib_cont_brk_time", &aw8624x->dts_info.cont_brk_time);
	if (val != 0)
		aw_err("aw8624x_vib_cont_brk_time not found");

	val = of_property_read_u32(np, "vib_cont_track_margin",
				   &aw8624x->dts_info.cont_track_margin);
	if (val != 0)
		aw_err("aw8624x_vib_cont_track_margin not found");
#if defined(CONFIG_AW8624X_SAMSUNG_FEATURE)
	val = of_property_read_u32(np, "samsung,max_level_gain",
				   &aw8624x->dts_info.max_level_gain_val);
	if (val != 0) {
		aw_err("Max Level gain Value not found");
		aw8624x->dts_info.max_level_gain_val = 0x80;
	} else
		aw_info("aw8624x->dts_info.max_level_gain_val = %d",
						aw8624x->dts_info.max_level_gain_val);
	/* True for manual f0 Calibration */
	aw8624x->dts_info.is_f0_tracking = of_property_read_bool(np,
					"samsung,f0-tracking");
	aw_info("aw8624x->dts_info.is_f0_tracking = %d",
					aw8624x->dts_info.is_f0_tracking);
	if (aw8624x->dts_info.is_f0_tracking) {
		val = of_property_read_u32(np, "samsung,f0-tracking-offset",
					   &aw8624x->dts_info.f0_offset);
		if (val != 0) {
			aw_err("f0 offset value not found");
			aw8624x->dts_info.f0_offset = 0;
		} else
			aw_info("aw8624x->dts_info.f0_offset = %d",
							aw8624x->dts_info.f0_offset);
	}
#endif //CONFIG_AW8624X_SAMSUNG_FEATURE
	aw8624x->dts_info.is_enabled_smart_loop = of_property_read_bool(np,
					"vib_is_enabled_smart_loop");
	aw_info("aw8624x->dts_info.is_enabled_smart_loop = %d",
					aw8624x->dts_info.is_enabled_smart_loop);
	aw8624x->dts_info.is_enabled_inter_brake =
					of_property_read_bool(np, "vib_is_enabled_inter_brake");
	aw_info("aw8624x->dts_info.is_enabled_inter_brake = %d",
					aw8624x->dts_info.is_enabled_inter_brake);
	aw8624x->dts_info.is_enabled_auto_brk = of_property_read_bool(np,
					"vib_is_enabled_auto_brk");
	aw_info("aw8624x->dts_info.is_enabled_auto_brk=%d", aw8624x->dts_info.is_enabled_auto_brk);

	return 0;
}

/*
 * software reset function
 */
static void aw8624x_sw_reset(struct aw8624x *aw8624x)
{
	aw_info("enter");
	aw8624x_i2c_write(aw8624x, AW8624X_REG_ID, AW8624X_BIT_RESET);
	usleep_range(2000, 2500);
}

/*
 * parse chip id
 */
static int aw8624x_parse_chipid(struct aw8624x *aw8624x)
{
	int ret = -1;
	uint8_t reg[2] = {0};
	uint8_t cnt = 0;

	while (cnt < AW8624X_READ_CHIPID_RETRIES) {
		ret = aw8624x_i2c_reads(aw8624x, AW8624X_REG_CHIPIDH, reg, 2);
		if (ret < 0) {
			aw_err("failed to read AW8624X_REG_CHIPID: %d", ret);
			break;
		}
		if (reg[0] == AW8624X_CHIP_ID) {
			switch (reg[1]) {
			case AW86243_CHIP_ID:
				aw_info("detected aw86243.");
				return 0;
			case AW86245_CHIP_ID:
				aw_info("detected aw86245.");
				return 0;
			default:
				aw_info("the chip is not aw8624X");
			}
		}
		aw_info("unsupport device revision reg[0]:0x%02X reg[1]:0x%02X", reg[0], reg[1]);
		cnt++;
		usleep_range(2000, 3000);
	}

	return -EINVAL;
}

#ifdef CONFIG_AW8624X_SAMSUNG_FEATURE
#include "aw8624x_sep_map_table.h"

static unsigned short aw8624x_sep_index_mapping(struct aw8624x *aw8624x,
		unsigned short sep_index)
{
	if (aw8624x->use_sep_index && sep_index < ARRAY_SIZE(sep_to_aw8624x_index_mapping) && sep_index > 0) {
		aw_info("%s SEP index(%hu -> %hu)\n", __func__, sep_index,
		sep_to_aw8624x_index_mapping[sep_index]);
		return sep_to_aw8624x_index_mapping[sep_index];
	}
#ifdef CONFIG_AW8624X_SAMSUNG_FEATURE
	aw_info("%s - No Mapping, so returning 0, sep_index is %hu, use_sep_index is %d\n",
		__func__, sep_index, aw8624x->use_sep_index);
#else
	aw_info("%s - No Mapping, so returning 0\n", __func__);
#endif
	return 0;
}
#endif //CONFIG_AW8624X_SAMSUNG_FEATURE

/*
 * check mass production bit, usually done during initialization,
 * modification is not recommended
 */
static int aw8624x_check_qualify(struct aw8624x *aw8624x)
{
	unsigned char reg = 0;
	int ret = 0;

	ret = aw8624x_i2c_read(aw8624x, AW8624X_REG_EFRD9, &reg);
	if (ret < 0) {
		aw_err("failed to read register 0x64: %d", ret);
		return ret;
	}
	if ((reg & 0x80) == 0x80)
		return 0;
	aw_err("register 0x64 error: 0x%02x", reg);

	return -ERANGE;
}

/*
 * analysis of upper layer parameters of input architecture
 */
static int aw8624x_input_upload_effect(struct input_dev *dev, struct ff_effect *effect,
				struct ff_effect *old)
{
	struct aw8624x *aw8624x = aw8624x_input_get_drvdata(dev);
	unsigned short data[2] = {0};
	int ret;
	int effect_id_max = 0;

	aw_info("enter, effect->type=0x%X, FF_CONSTANT=0x%X, FF_PERIODIC=0x%X",
		effect->type, FF_CONSTANT, FF_PERIODIC);
	effect_id_max = sizeof(aw8624x_rtp_name) / sizeof(*aw8624x_rtp_name) +
			AW_RAM_MAX_INDEX - 1;
	if (!aw8624x->ram_init) {
		aw_info("ram init failed, not allow to play!");
		return -ERANGE;
	}
	cancel_work_sync(&aw8624x->vibrator_work);
	/* stop ram loop */
	if (hrtimer_active(&aw8624x->timer)) {
		if (aw8624x->wk_lock_flag == 1) {
			pm_relax(aw8624x->dev);
			aw8624x->wk_lock_flag = 0;
		}
		hrtimer_cancel(&aw8624x->timer);
		aw8624x_play_stop(aw8624x);
	}
	mutex_lock(&aw8624x->lock);
	aw8624x->effect_type = effect->type;
	aw8624x->rtp_repeat = 0;
	/* wait for rtp to exit */
	aw8624x->play_mode = AW8624X_NULL;
	if (aw8624x->rtp_init) {
		aw8624x->rtp_init = false;
		while (atomic_read(&aw8624x->is_in_rtp_loop)) {
			aw_info("goint to waiting rtp play exit");
			mutex_unlock(&aw8624x->lock);
			ret = wait_event_interruptible(aw8624x->stop_wait_q,
					atomic_read(&aw8624x->is_in_rtp_loop) == 0);
			aw_info("wakeup");
			if (ret == -ERESTARTSYS) {
				mutex_unlock(&aw8624x->lock);
				aw_info("wake up by signal return erro");
				return ret;
			}
			mutex_lock(&aw8624x->lock);
		}
		aw8624x->rtp_cnt = 0;
		aw8624x_set_rtp_aei(aw8624x, false);

		if (aw8624x->wk_lock_flag == 1) {
			pm_relax(aw8624x->dev);
			aw8624x->wk_lock_flag = 0;
		}
	}
	mutex_unlock(&aw8624x->lock);
	cancel_work_sync(&aw8624x->rtp_work);

	mutex_lock(&aw8624x->lock);
	if (aw8624x->effect_type == FF_CONSTANT) {
		aw_info("effect_type: FF_CONSTANT!");
		aw8624x->duration = effect->replay.length;
		aw8624x->activate_mode = AW8624X_RAM_LOOP_MODE;
		aw8624x->effect_id = AW_RAM_LONG_INDEX;
	} else if (aw8624x->effect_type == FF_PERIODIC) {
		aw_info("effect_type: FF_PERIODIC!");
		if (effect->u.periodic.waveform == FF_CUSTOM) {
			ret = copy_from_user(data, effect->u.periodic.custom_data, sizeof(short) * 2);
			if (ret) {
				mutex_unlock(&aw8624x->lock);
				return -EFAULT;
			}
		} else {
			data[0] = effect->u.periodic.offset;
			data[1] = 0;
		}
#ifdef CONFIG_AW8624X_SAMSUNG_FEATURE
		data[0] = aw8624x_sep_index_mapping(aw8624x, (unsigned short)data[0]);
#endif
		if (data[0] < 1 || data[0] > effect_id_max) {
			aw_err("data[0] out of range :%u !!", data[0]);
			mutex_unlock(&aw8624x->lock);
			return -ERANGE;
		}
		if ((data[0] == AW_RAM_LONG_INDEX) ||
			(data[0] > aw8624x->ram.ram_num && data[0] <= AW_RAM_MAX_INDEX)) {
			aw_err("data[0]:%u is neither RAM or RTP seq!!", data[0]);
			mutex_unlock(&aw8624x->lock);
			return -ERANGE;
		}
		aw8624x->effect_id = data[0];
		if (aw8624x->effect_id <= aw8624x->ram.ram_num) {
			aw8624x->activate_mode = AW8624X_RAM_MODE;
			aw_info("aw8624x->effect_id=%d, activate_mode=%u", aw8624x->effect_id,
				aw8624x->activate_mode);
		}
		if (aw8624x->effect_id > AW_RAM_MAX_INDEX) {
			aw8624x->activate_mode = AW8624X_RTP_MODE;
			aw8624x->rtp_repeat = data[1];
			aw_info("effect_id=%d, activate_mode=%u, rtp_repeat=%u", aw8624x->effect_id,
				aw8624x->activate_mode, aw8624x->rtp_repeat);
		}

	} else {
		aw_info("Unsupported effect type: %u", effect->type);
	}
	mutex_unlock(&aw8624x->lock);

	return 0;
}

/*
 * input architecture playback function
 */
static int aw8624x_input_playback(struct input_dev *dev, int effect_id,
				int val)
{
	struct aw8624x *aw8624x = aw8624x_input_get_drvdata(dev);

	aw_info("enter, effect_id=%d, val=%d, aw8624x->effect_id=%d, aw8624x->activate_mode=%u",
		effect_id, val, aw8624x->effect_id, aw8624x->activate_mode);
	aw8624x->input_flag = true;

	if (!aw8624x->ram_init) {
		aw_info("ram init failed, not allow to play!");
		return -ERANGE;
	}
	if (val > 0) {
		aw8624x->state = 1;
	} else {
		queue_work(aw8624x->work_queue, &aw8624x->stop_work);
		return 0;
	}
	hrtimer_cancel(&aw8624x->timer);

	if (aw8624x->effect_type == FF_CONSTANT &&
			aw8624x->activate_mode == AW8624X_RAM_LOOP_MODE) {
		aw_info("enter ram_loop_mode ");
		queue_work(aw8624x->work_queue, &aw8624x->vibrator_work);
	} else if (aw8624x->effect_type == FF_PERIODIC &&
			aw8624x->activate_mode == AW8624X_RAM_MODE) {
		aw_info("enter ram_mode");
		queue_work(aw8624x->work_queue, &aw8624x->vibrator_work);
	} else if (aw8624x->effect_type == FF_PERIODIC &&
			aw8624x->activate_mode == AW8624X_RTP_MODE) {
		aw_info("enter rtp_mode");
		queue_work(aw8624x->work_queue, &aw8624x->rtp_work);
	} else {
		aw_err("Unsupported mode: %u or Unsupported effect_type: 0x%X",
			aw8624x->activate_mode, aw8624x->effect_type);
	}

	return 0;
}

static int aw8624x_input_erase(struct input_dev *dev, int effect_id)
{
	struct aw8624x *aw8624x = aw8624x_input_get_drvdata(dev);

	aw_info("enter");
	aw8624x->effect_type = 0;
	aw8624x->duration = 0;

	return 0;
}

static void aw8624x_stop_work_routine(struct work_struct *work)
{
	struct aw8624x *aw8624x = container_of(work, struct aw8624x, stop_work);

	mutex_lock(&aw8624x->lock);
	if (aw8624x->wk_lock_flag == 1) {
		pm_relax(aw8624x->dev);
		aw8624x->wk_lock_flag = 0;
	}
	hrtimer_cancel(&aw8624x->timer);
	aw8624x_play_stop(aw8624x);
	mutex_unlock(&aw8624x->lock);
}

/*
 * Samsung Gain values : 0 ~ 10000
 * Awinic Gain Register Values : 30 ~ 128
 *
 * Samsung mapping formula:
 *   gain = (ss_gain * max_level_gain) / 10000
 */
static void aw8624x_input_set_gain_work_routine(struct work_struct *work)
{
	struct aw8624x *aw8624x = container_of(work, struct aw8624x, set_gain_work);
#if defined(CONFIG_AW8624X_SAMSUNG_FEATURE)
	if (aw8624x->level >= 10000)
		aw8624x->gain = aw8624x->dts_info.max_level_gain_val;
	else
		aw8624x->gain = aw8624x->level * aw8624x->dts_info.max_level_gain_val / 10000;
#endif
	aw_info("set_gain queue work, gain: 0x%04X, level: 0x%04X", aw8624x->gain, aw8624x->level);
	aw8624x_set_gain(aw8624x, aw8624x->gain);
}

/*
 * input architecture gain setting for waveform data
 */
static void aw8624x_input_set_gain(struct input_dev *dev, u16 level)
{
	struct aw8624x *aw8624x = aw8624x_input_get_drvdata(dev);

	aw_info("enter, level: 0x%04X", level);
	aw8624x->level = level;
#if defined(CONFIG_AW8624X_SAMSUNG_FEATURE)
	aw_info("before temp check for level adjustment, level: 0x%04x", aw8624x->level);
	aw8624x->level = sec_vib_inputff_tune_gain(&aw8624x->sec_vib_ddata, aw8624x->level);
	aw_info("after temp check for level adjustment, level: 0x%04x", aw8624x->level);
#endif
	queue_work(aw8624x->work_queue, &aw8624x->set_gain_work);
}

/*
 * offset calibration for bemf detection,
 * it is recommended not to modify
 */
static int aw8624x_offset_cali(struct aw8624x *aw8624x)
{
	uint8_t reg_val[2] = {0};
	int os_code = 0;
	int d2s_gain = 0;

	/* d2s_gain */
	if (!aw8624x->dts_info.d2s_gain) {
		aw_err("dts_info.d2s_gain = 0!");
	} else {
		aw8624x_i2c_write_bits(aw8624x, AW8624X_REG_SYSCTRL5,
			AW8624X_BIT_SYSCTRL5_D2S_GAIN_MASK,
			aw8624x->dts_info.d2s_gain);
	}

	/* f0_d2s_gain */
	if (!aw8624x->dts_info.f0_d2s_gain) {
		aw_err("dts_info.f0_d2s_gain = 0!");
	} else {
		aw8624x_i2c_write_bits(aw8624x, AW8624X_REG_SYSCTRL6,
			AW8624X_BIT_SYSCTRL6_F0_D2S_GAIN_MASK,
			aw8624x->dts_info.f0_d2s_gain);
	}

	d2s_gain = aw8624x_select_d2s_gain(aw8624x->dts_info.f0_d2s_gain);
	if (d2s_gain < 0) {
		aw_err("d2s_gain is error");
		return -ERANGE;
	}
	aw8624x_raminit(aw8624x, true);
	aw8624x_i2c_write_bits(aw8624x, AW8624X_REG_DETCFG1, AW8624X_BIT_DETCFG1_RL_OS_MASK,
			AW8624X_BIT_DETCFG1_OS);
	aw8624x_i2c_write_bits(aw8624x, AW8624X_REG_DETCFG2, AW8624X_BIT_DETCFG2_DIAG_GO_MASK,
			AW8624X_BIT_DETCFG2_DIAG_GO_ON);
	usleep_range(3000, 3500);
	aw8624x_i2c_read(aw8624x, AW8624X_REG_DET_OS, &reg_val[0]);
	aw8624x_i2c_read(aw8624x, AW8624X_REG_DET_LO, &reg_val[1]);
	aw8624x_raminit(aw8624x, false);
	os_code = (reg_val[1] & (~AW8624X_BIT_DET_LO_OS_MASK)) >> 2;
	os_code = (reg_val[0] << 2) | os_code;
	os_code = AW8624X_OS_FORMULA(os_code, d2s_gain);
	aw_info("os_code is %d mV", os_code);
	if (os_code > 15 || os_code < -15)
		return -ERANGE;

	return 0;
}

/*
 * vbat mode for cont mode, the output waveform will
 * keep stable against VDD voltage drop
 */
static void aw8624x_vbat_mode_config(struct aw8624x *aw8624x, uint8_t flag)
{
	uint8_t val = 0;

	aw_info("enter");
	if (flag == AW8624X_CONT_VBAT_HW_COMP_MODE) {
		val = AW8624X_BIT_GLBCFG2_START_DLY_250US;
		aw8624x_i2c_write(aw8624x, AW8624X_REG_GLBCFG2, val);
		aw8624x_i2c_write_bits(aw8624x, AW8624X_REG_SYSCTRL1,
				AW8624X_BIT_SYSCTRL1_VBAT_MODE_MASK,
				AW8624X_BIT_SYSCTRL1_VBAT_MODE_HW);
	} else {
		aw8624x_i2c_write_bits(aw8624x, AW8624X_REG_SYSCTRL1,
				AW8624X_BIT_SYSCTRL1_VBAT_MODE_MASK,
				AW8624X_BIT_SYSCTRL1_VBAT_MODE_SW);
	}
}

/*
 * get theory playback time in OSC calibration
 */
static uint32_t aw8624x_get_theory_time(struct aw8624x *aw8624x)
{
	uint8_t reg_val = 0;
	uint32_t fre_val = 0;
	uint32_t theory_time = 0;

	aw8624x_i2c_read(aw8624x, AW8624X_REG_SYSCTRL2, &reg_val);
	fre_val = reg_val & AW8624X_BIT_SYSCTRL2_RATE;
	if (fre_val == AW8624X_BIT_SYSCTRL2_RATE_48K)
		theory_time = aw8624x->rtp_len * 1000 / 48;	/*48K*/
	else if (fre_val == AW8624X_BIT_SYSCTRL2_RATE_24K)
		theory_time = aw8624x->rtp_len * 1000 / 24;	/*24K*/
	else
		theory_time = aw8624x->rtp_len * 1000 / 12;	/*12K*/
	aw_info("microsecond: %u theory_time: %u", aw8624x->microsecond, theory_time);

	return theory_time;
}

/************ RTP F0 Cali Start ****************/
static uint8_t aw8624x_rtp_vib_data_des(uint8_t *data, int lens, int cnt_set)
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

static uint8_t aw8624x_rtp_zero_data_des(uint8_t *data, int lens, int cnt_set)
{
	uint8_t ret = 0;
	int i = 0;
	int cnt = 0;

	for (i = 0; i < cnt_set; i++) {
		if ((i + 1) >= lens)
			return ret;
		if (data[i] == 0 || data[i] == 0x80)
			cnt++;
	}
	if (cnt == cnt_set)
		ret = 1;

	return ret;
}

static int aw8624x_rtp_f0_cali(struct aw8624x *aw8624x, uint8_t *src_data,
			int file_lens, int rtp_cali_len)
{
#ifdef AW_OVERRIDE_EN
	int zero_cnt = 0;
	int max_val = -1000;
	int two_cycle_stop_pos = 0;
	int last_pos = 0;
	int div = 1000;
	int l = 0;
#endif
	int temp_cnt = 0;
	int ret = 0;
	int i = 0;
	int j = 0;
	int k = 0;
	int index = 0;
	int *start_pos = NULL;
	int *stop_pos = NULL;
	int vib_judg_cnt = 0;
	int zero_judg_cnt = 300;
	int seg_src_len = 0;
	int seg_dst_len = 0;
	int next_start_pos = 0;
	int temp_len = 0;
	int zero_len = 0;
	int brk_wave_len = 0;
	int8_t data_value = 0;
	int left = 0;
	int right = 0;
	int8_t *seg_src_data = NULL;
	int cnt = 0;
	int src_f0 = aw8624x->dts_info.f0_pre;
	int dst_f0 = aw8624x->f0;
#ifdef AW_ADD_BRAKE_WAVE
	bool en_replace = true;
#endif

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
		if (src_data[i + 1] == 0 || src_data[i + 1] == 0x80)
			continue;
		ret = aw8624x_rtp_vib_data_des(&src_data[i + 1], file_lens, vib_judg_cnt);
		if (!ret)
			continue;
		if (index >= 1000) {
			aw_err("index overflow : %d ", index);
			vfree(start_pos);
			vfree(stop_pos);
			return -EFBIG;
		}
		start_pos[index] = i;
		brk_wave_len = 0;
		for (j = i + 1; j < file_lens; j++) {
			if ((j + 1) >= file_lens) {
				stop_pos[index] = file_lens - 1;
				break;
			}
			if (src_data[j] == 0 || src_data[j] == 0x80) {
				if (j >= file_lens - zero_judg_cnt)
					continue;
				ret = aw8624x_rtp_zero_data_des(&src_data[j + 1],
						file_lens, zero_judg_cnt);
#ifdef AW_ADD_BRAKE_WAVE
				if (ret == 1 && en_replace) {
					aw_info("index : %d ", index);
					en_replace = false;
					memset(&src_data[j], 0x80, sizeof(uint8_t) * zero_judg_cnt);
				}
#endif
				if (!ret)
					continue;
				ret = 0;
				for (k = j + 1; k < file_lens; k++) {
					if (src_data[k] == 0)
						continue;
					if (src_data[k] == 0x80) {
						brk_wave_len++;
						continue;
					}
					ret = aw8624x_rtp_vib_data_des(&src_data[k],
						file_lens, vib_judg_cnt);
					if (ret) {
						next_start_pos = k;
						break;
					}
				}
				if (!ret) {
					next_start_pos = j;
					stop_pos[index] = next_start_pos;
					break;
				}
				seg_src_len = j - start_pos[index] + 1 + brk_wave_len;
				seg_dst_len = seg_src_len * src_f0 / dst_f0;
				if (seg_dst_len >= (next_start_pos - start_pos[index] + 1)) {
					j = next_start_pos;
				} else {
					stop_pos[index] = j;
					break;
				}
			}
		}
		if (index >= 1)
			zero_len = start_pos[index] - (start_pos[index - 1] + temp_len);
		else
			zero_len = start_pos[index];

		for (j = 0; j < zero_len; j++)
			aw8624x->aw_rtp->data[cnt++] = 0;
		if (stop_pos[index] == file_lens - 1)
			brk_wave_len = 0;
		seg_src_len = stop_pos[index] - start_pos[index] + 1 + brk_wave_len;
		seg_src_data = &src_data[i];
		seg_dst_len = (seg_src_len * src_f0) / dst_f0;
		temp_cnt = cnt;
		temp_len = (dst_f0 / src_f0) >= 1 ? seg_src_len : seg_dst_len;
		aw_info("seg_src_len = %d, seg_dst_len = %d, temp_len = %d", seg_src_len, seg_dst_len, temp_len);
		aw_info("brk_wave_len = %d", brk_wave_len);
		for (j = 0; j < seg_dst_len; j++) {
			left = (int)(j * dst_f0 / src_f0);
			right = (int)(j * dst_f0 / src_f0 + 1.0);
			data_value = ((seg_src_data[right] - seg_src_data[left]) *
			((j * dst_f0 / src_f0) - left)) + seg_src_data[left];
			aw8624x->aw_rtp->data[cnt++] = data_value;
		}
		for (j = 0; j < temp_len - seg_dst_len; j++)
			aw8624x->aw_rtp->data[cnt++] = 0;

#ifdef AW_OVERRIDE_EN
		zero_cnt = 0;
		max_val = -1000;
		last_pos = 0;
		aw_info("temp_len - brk_wave_len = %d", temp_len - brk_wave_len);
		if ((temp_len - brk_wave_len) >= AW_LONG_VIB_POINTS) {
			for (l = 5; l < temp_len; l++) {
				if (abs((int8_t)aw8624x->aw_rtp->data[l + temp_cnt]) >= max_val)
					max_val = abs((int8_t)aw8624x->aw_rtp->data[l + temp_cnt]);

				if ((int8_t)aw8624x->aw_rtp->data[l + temp_cnt] *
					(int8_t)aw8624x->aw_rtp->data[l + temp_cnt + 1] <= 0) {
					if ((l - last_pos) >= 5) {
						zero_cnt++;
						last_pos = l;
					}
					if (zero_cnt == 5) {
						two_cycle_stop_pos = l;
						break;
					}
				}
			}
			div = 127000 / max_val;
			div = div > 2000 ? 2000 : div;
			for (l = 0; l < two_cycle_stop_pos; l++) {
				aw8624x->aw_rtp->data[l + temp_cnt] =
					(int8_t)aw8624x->aw_rtp->data[l + temp_cnt] * div / 1000;
			}
		}
#endif
		index++;
		i += temp_len;
#ifdef AW_ADD_BRAKE_WAVE
		en_replace = true;
#endif
	}
	if (index == 0) {
		vfree(start_pos);
		vfree(stop_pos);
		aw_err("No vibration data identified!");
		return -EINVAL;
	}
	zero_len = (file_lens - stop_pos[index - 1] - brk_wave_len - 1) - (temp_len - seg_src_len);
	for (j = 0; j < zero_len; j++) {
		if (cnt >= rtp_cali_len) {
			cnt = rtp_cali_len;
			break;
		}
		aw8624x->aw_rtp->data[cnt++] = 0;
	}
	aw8624x->aw_rtp->len = cnt;
	vfree(start_pos);
	vfree(stop_pos);
	aw_info("RTP F0 cali succeed. cnt = %d", cnt);

	return 0;
}
/************ RTP F0 Cali End ****************/

/*
 * calculation of real playback duration in OSC calibration
 */
static int aw8624x_rtp_osc_calibration(struct aw8624x *aw8624x)
{
	const struct firmware *rtp_file;
	uint8_t state = 0;
	uint32_t base_addr = aw8624x->ram.base_addr;
	uint32_t buf_len = 0;
	int ret = -1;

	aw8624x->rtp_cnt = 0;
	aw8624x->timeval_flags = 1;

	/* add VDD POR verify */
	aw8624x_i2c_read(aw8624x, AW8624X_REG_SYSST2, &state);
	if (!(state & AW8624X_BIT_SYSST2_DVDD_PORN)) {
		aw_err("VDD is NOT POWER ON!!!");
		return -ERANGE;
	}

	aw_info("enter");
	/* fw loaded */
	ret = request_firmware(&rtp_file, aw8624x_rtp_name[0], aw8624x->dev);
	if (ret < 0) {
		aw_err("failed to read %s", aw8624x_rtp_name[0]);
		return ret;
	}
	/* for irq interrupt during calibrate */
	aw8624x_play_stop(aw8624x);
	aw8624x->rtp_init = false;
	mutex_lock(&aw8624x->rtp_lock);
	vfree(aw8624x->aw_rtp);
	aw8624x->aw_rtp = vmalloc(rtp_file->size + sizeof(int));
	if (!aw8624x->aw_rtp) {
		release_firmware(rtp_file);
		mutex_unlock(&aw8624x->rtp_lock);
		aw_err("error allocating memory");
		return -ENOMEM;
	}
	aw8624x->aw_rtp->len = rtp_file->size;
	aw8624x->rtp_len = rtp_file->size;
	aw_info("rtp file:[%s] size=%dbytes", aw8624x_rtp_name[0],
			aw8624x->aw_rtp->len);
	memcpy(aw8624x->aw_rtp->data, rtp_file->data, rtp_file->size);
	release_firmware(rtp_file);
	mutex_unlock(&aw8624x->rtp_lock);
	/* set gain */
	aw8624x_set_gain(aw8624x, aw8624x->gain);
	/* rtp mode config */
	aw8624x_play_mode(aw8624x, AW8624X_RTP_MODE);
	disable_irq(gpio_to_irq(aw8624x->irq_gpio));
	/* haptic go */
	aw8624x_play_go(aw8624x, true);
	/* require latency of CPU & DMA not more then PM_QOS_VALUE_VB us */
	pm_qos_enable(aw8624x, true);
	while (1) {
		if (!aw8624x_rtp_get_fifo_afs(aw8624x)) {
			aw_dbg("aw8624x_rtp_get_fifo_afi, rtp_cnt= %u", aw8624x->rtp_cnt);
			mutex_lock(&aw8624x->rtp_lock);
			if ((aw8624x->aw_rtp->len > aw8624x->rtp_cnt) &&
				((aw8624x->aw_rtp->len - aw8624x->rtp_cnt) < (base_addr >> 2)))
				buf_len = aw8624x->aw_rtp->len - aw8624x->rtp_cnt;
			else
				buf_len = (base_addr >> 2);

			if (aw8624x->rtp_cnt != aw8624x->aw_rtp->len) {
				if (aw8624x->timeval_flags == 1) {
					aw8624x->kstart = ktime_get();
					aw8624x->timeval_flags = 0;
				}
				aw8624x_i2c_writes(aw8624x, AW8624X_REG_RTPDATA,
						&aw8624x->aw_rtp->data[aw8624x->rtp_cnt], buf_len);
				aw8624x->rtp_cnt += buf_len;
			}
			mutex_unlock(&aw8624x->rtp_lock);
		}
		aw8624x_i2c_read(aw8624x, AW8624X_REG_SYSST2, &state);
		if (state & AW8624X_BIT_SYSST2_FF_EMPTY) {
			aw8624x->kend = ktime_get();
			aw_info("osc trim playback done aw8624x->rtp_cnt= %u", aw8624x->rtp_cnt);
			break;
		}
		aw8624x->kend = ktime_get();
		aw8624x->microsecond = ktime_to_us(ktime_sub(aw8624x->kend, aw8624x->kstart));
		if (aw8624x->microsecond > AW8624X_OSC_CALI_MAX_LENGTH) {
			aw_info("osc trim time out! aw8624x->rtp_cnt %u", aw8624x->rtp_cnt);
			break;
		}
	}
	pm_qos_enable(aw8624x, false);
	enable_irq(gpio_to_irq(aw8624x->irq_gpio));
	aw8624x->microsecond = ktime_to_us(ktime_sub(aw8624x->kend, aw8624x->kstart));
	/* calibration osc */
	aw_info("awinic_microsecond: %u", aw8624x->microsecond);

	return 0;
}

/*
 * OSC calibration value calculation
 */
static int aw8624x_osc_trim_calculation(uint32_t theory_time, uint32_t real_time)
{
	uint32_t real_code = 0;
	uint32_t lra_code = 0;
	uint32_t DFT_LRA_TRIM_CODE = 0;
	/* 0.1 percent below no need to calibrate */
	uint32_t osc_cali_threshold = 10;

	aw_info("enter");
	if (theory_time == real_time) {
		aw_info("theory_time == real_time: %u, no need to calibrate!", real_time);
		return 0;
	} else if (theory_time < real_time) {
		if ((real_time - theory_time) > (theory_time / 50)) {
			aw_info("(real_time - theory_time) > (theory_time / 50), can't calibrate!");
			return DFT_LRA_TRIM_CODE;
		}
		if ((real_time - theory_time) < (osc_cali_threshold * theory_time / 10000)) {
			aw_info(" real_time: %u, theory_time: %u, no need to calibrate!",
				 real_time, theory_time);
			return DFT_LRA_TRIM_CODE;
		}
		real_code = ((real_time - theory_time) * 4000) / theory_time;
		real_code = ((real_code % 10 < 5) ? 0 : 1) + real_code / 10;
		real_code = 64 + real_code;
	} else if (theory_time > real_time) {
		if ((theory_time - real_time) > (theory_time / 50)) {
			aw_info("(theory_time - real_time) > (theory_time / 50), can't calibrate!");
			return DFT_LRA_TRIM_CODE;
		}
		if ((theory_time - real_time) < (osc_cali_threshold * theory_time / 10000)) {
			aw_info("real_time: %u, theory_time: %u, no need to calibrate!",
				real_time, theory_time);
			return DFT_LRA_TRIM_CODE;
		}
		real_code = ((theory_time - real_time) * 4000) / theory_time;
		real_code = ((real_code % 10 < 5) ? 0 : 1) + real_code / 10;
		if (real_code < 64)
			real_code = 64 - real_code;
		else
			real_code = 0;
	}
	if (real_code >= 64)
		lra_code = real_code - 64;
	else
		lra_code = real_code + 64;
	aw_info("real_time: %u, theory_time: %u", real_time, theory_time);
	aw_info("real_code: %02X, trim_lra: 0x%02X", real_code, lra_code);

	return lra_code;
}

/*
 * calculate OSC calibration theoretical duration and OSC calibration
 */
static int aw8624x_rtp_trim_lra_calibration(struct aw8624x *aw8624x)
{
	int lra_trim_code = 0;
	uint32_t theory_time = 0;

	theory_time = aw8624x_get_theory_time(aw8624x);

	if (theory_time == 0) {
		aw_info("theory_time is zero, please check rtp_len!!!");
		return -1;
	}

	lra_trim_code = aw8624x_osc_trim_calculation(theory_time, aw8624x->microsecond);
	if (lra_trim_code >= 0) {
		aw8624x->osc_cali_data = lra_trim_code;
		aw8624x_upload_lra(aw8624x, AW8624X_OSC_CALI_LRA);
	}

	return 0;
}

/*
 * interrupt register configuration
 */
static void aw8624x_interrupt_setup(struct aw8624x *aw8624x)
{
	uint8_t reg_val = 0;

	aw_info("enter");
	aw8624x_i2c_read(aw8624x, AW8624X_REG_SYSINT, &reg_val);
	aw_info("reg SYSINT=0x%02X", reg_val);

	/* History mode of interrupt */
	aw8624x_i2c_write_bits(aw8624x, AW8624X_REG_SYSCTRL5, AW8624X_BIT_SYSCTRL5_INT_MODE_MASK,
			AW8624X_BIT_SYSCTRL5_INT_MODE_EDGE);
	aw8624x_i2c_write_bits(aw8624x, AW8624X_REG_SYSCTRL5,
			AW8624X_BIT_SYSCTRL5_INT_EDGE_MODE_MASK,
			AW8624X_BIT_SYSCTRL5_INT_EDGE_MODE_POS);
	/* Enable undervoltage int */
	aw8624x_i2c_write_bits(aw8624x, AW8624X_REG_SYSINTM, AW8624X_BIT_SYSINTM_UVLM_MASK,
			AW8624X_BIT_SYSINTM_UVLM_ON);
	/* Enable over current int */
	aw8624x_i2c_write_bits(aw8624x, AW8624X_REG_SYSINTM, AW8624X_BIT_SYSINTM_OCDM_MASK,
			AW8624X_BIT_SYSINTM_OCDM_ON);
	/* Enable over temperature int */
	aw8624x_i2c_write_bits(aw8624x, AW8624X_REG_SYSINTM, AW8624X_BIT_SYSINTM_OTM_MASK,
			AW8624X_BIT_SYSINTM_OTM_ON);
}

/*
 * rtp play
 */
static void aw8624x_rtp_play(struct aw8624x *aw8624x)
{
	uint8_t glb_st = 0;
	uint32_t buf_len = 0;
	int ret = 0;
	struct aw8624x_container *aw_rtp = aw8624x->aw_rtp;

	aw_info("enter");
	if (!aw_rtp) {
		aw_info("aw_rtp is null, break!");
		return;
	}
	atomic_set(&aw8624x->is_in_rtp_loop, 1);
	while ((!aw8624x_rtp_get_fifo_afs(aw8624x))
			&& (aw8624x->play_mode == AW8624X_RTP_MODE)) {
		aw_dbg("rtp cnt = %u", aw8624x->rtp_cnt);

		if (aw8624x->rtp_cnt < (aw8624x->ram.base_addr)) {
			if ((aw_rtp->len > aw8624x->rtp_cnt) && ((aw_rtp->len - aw8624x->rtp_cnt) <
				(aw8624x->ram.base_addr))) {
				buf_len = aw_rtp->len - aw8624x->rtp_cnt;
			} else {
				if (aw8624x->rtp_repeat)
					buf_len = aw8624x->ram.base_addr >> 2;
				else
					buf_len = aw8624x->ram.base_addr;
			}
		} else if ((aw_rtp->len > aw8624x->rtp_cnt) && ((aw_rtp->len - aw8624x->rtp_cnt) <
				(aw8624x->ram.base_addr >> 2))) {
			buf_len = aw_rtp->len - aw8624x->rtp_cnt;
		} else {
			buf_len = aw8624x->ram.base_addr >> 2;
		}
		ret = aw8624x_i2c_writes(aw8624x, AW8624X_REG_RTPDATA, &aw_rtp->data[aw8624x->rtp_cnt],
							buf_len);
		if (ret < 0) {
			aw_err("write rtp_data failed!");
			aw8624x->rtp_cnt = 0;
			aw8624x->rtp_init = false;
			break;
		}
		aw8624x->rtp_cnt += buf_len;
		aw8624x_i2c_read(aw8624x, AW8624X_REG_GLBRD5, &glb_st);
		if ((aw8624x->rtp_cnt == aw_rtp->len) ||
		((glb_st & AW8624X_BIT_GLBRD5_STATE_MASK) == AW8624X_BIT_GLBRD5_STATE_STANDBY)) {
			if (aw8624x->rtp_cnt == aw_rtp->len) {
				if (aw8624x->rtp_repeat) {
					aw8624x->rtp_cnt = 0;
					aw_info("rtp repeat!");
				} else {
					aw_info("rtp load completely! glb_state_val=0x%02x aw8624x->rtp_cnt=%u",
							glb_st, aw8624x->rtp_cnt);
					aw8624x->rtp_cnt = 0;
					aw8624x->rtp_init = false;
					aw8624x_set_rtp_aei(aw8624x, false);
					break;
				}
			} else {
				aw_err("rtp load failed!! glb_state_val=0x%02x aw8624x->rtp_cnt=%u",
							glb_st, aw8624x->rtp_cnt);
				aw8624x->rtp_cnt = 0;
				aw8624x->rtp_init = false;
				aw8624x_set_rtp_aei(aw8624x, false);
				break;
			}
		}
	}

	atomic_set(&aw8624x->is_in_rtp_loop, 0);
	wake_up_interruptible(&aw8624x->stop_wait_q);
	if (!(aw8624x->rtp_init) && aw8624x->wk_lock_flag == 1) {
		pm_relax(aw8624x->dev);
		aw8624x->wk_lock_flag = 0;
	}
}

/*
 * interrupt callback function
 */
static irqreturn_t aw8624x_irq_handle(int irq, void *data)
{
	uint8_t reg_val = 0;
	struct aw8624x *aw8624x = data;

	aw_info("enter");
	/* acquire irq state */
	aw8624x_i2c_read(aw8624x, AW8624X_REG_SYSINT, &reg_val);
	aw_dbg("reg SYSINT=0x%02X", reg_val);
	if (reg_val & AW8624X_BIT_SYSINT_UVLI)
		aw_err("chip uvlo int error");
	if (reg_val & AW8624X_BIT_SYSINT_OCDI)
		aw_err("chip over current int error");
	if (reg_val & AW8624X_BIT_SYSINT_OTI)
		aw_err("chip over temperature int error");
	if (reg_val & AW8624X_BIT_SYSINT_DONEI)
		aw_info("chip playback done");
	if (reg_val & AW8624X_BIT_SYSINT_FF_AFI)
		aw_info("rtp mode fifo almost full!");
	if ((reg_val & AW8624X_BIT_SYSINT_FF_AEI) && aw8624x->rtp_init) {
		mutex_lock(&aw8624x->rtp_lock);
		aw8624x_rtp_play(aw8624x);
		mutex_unlock(&aw8624x->rtp_lock);
	} else if (reg_val & AW8624X_BIT_SYSINT_FF_AEI) {
		aw_info("aw8624x rtp fifo almost empty");
		if (!aw8624x->rtp_init)
			aw_err("aw8624x rtp init = %d", aw8624x->rtp_init);
	}

	if (aw8624x->play_mode != AW8624X_RTP_MODE)
		aw8624x_set_rtp_aei(aw8624x, false);

	aw_info("exit");

	return IRQ_HANDLED;
}

/*
 * irq_config
 */
static int aw8624x_irq_config(struct device *dev, struct aw8624x *aw8624x)
{
	int ret = -1;
	int irq_flags = 0;

	aw_info("enter");
	if (gpio_is_valid(aw8624x->irq_gpio)) {
		/* register irq handler */
		aw8624x_interrupt_setup(aw8624x);
		irq_flags = IRQF_TRIGGER_FALLING | IRQF_ONESHOT;
		ret = devm_request_threaded_irq(dev, gpio_to_irq(aw8624x->irq_gpio),
							NULL, aw8624x_irq_handle, irq_flags,
							"aw8624x", aw8624x);
		if (ret != 0) {
			aw_err("failed to request IRQ %d: %d", gpio_to_irq(aw8624x->irq_gpio), ret);
			return ret;
		}
	} else {
		aw_info("irq_gpio is invalid!");
	}

	return 0;
}

static int aw8624x_get_ram_num(struct aw8624x *aw8624x)
{
	uint8_t val[3] = {0};
	uint32_t first_wave_addr = 0;

	aw_info("enter");
	if (!aw8624x->ram_init) {
		aw_err("ram init failed, ram_num = 0!");
		return -EPERM;
	}
	aw8624x_play_stop(aw8624x);
	aw8624x_raminit(aw8624x, true);
	aw8624x_set_ram_addr(aw8624x);
	aw8624x_i2c_reads(aw8624x, AW8624X_REG_RAMDATA, val, 3);
	first_wave_addr = (val[1] << 8 | val[2]);
	aw8624x->ram.ram_num = (first_wave_addr - aw8624x->ram.base_addr - 1) / 4;
	aw_info("first wave addr = 0x%04X, ram_num = %u", first_wave_addr, aw8624x->ram.ram_num);
	aw8624x_raminit(aw8624x, false);

	return 0;
}

static int aw8624x_container_update(struct aw8624x *aw8624x, struct aw8624x_container *aw8624x_fw)
{
	uint32_t shift = 0;
	int i = 0;
	int len = 0;
	int ret = 0;

	aw_info("enter");
	mutex_lock(&aw8624x->lock);
	aw8624x->ram.baseaddr_shift = 2;
	aw8624x->ram.ram_shift = 4;
	aw8624x_play_stop(aw8624x);
	aw8624x_raminit(aw8624x, true);
	shift = aw8624x->ram.baseaddr_shift;
	aw8624x->ram.base_addr = (uint32_t)((aw8624x_fw->data[0 + shift] << 8) |
					(aw8624x_fw->data[1 + shift]));
	aw_info("base_addr = %u", aw8624x->ram.base_addr);
	if (!aw8624x->ram.base_addr)
		aw_info("no set FIFO space for rtp play!");
	aw8624x_set_base_addr(aw8624x);
	aw8624x_set_fifo_addr(aw8624x);
	aw8624x_get_fifo_addr(aw8624x);
	aw8624x_set_ram_addr(aw8624x);
	i = aw8624x->ram.ram_shift;
	while (i < aw8624x_fw->len) {
		if ((aw8624x_fw->len - i) < AW8624X_RAMDATA_WR_BUFFER_SIZE)
			len = aw8624x_fw->len - i;
		else
			len = AW8624X_RAMDATA_WR_BUFFER_SIZE;

		aw8624x_i2c_writes(aw8624x, AW8624X_REG_RAMDATA, &aw8624x_fw->data[i], len);
		i += len;
	}

#ifdef AW_CHECK_RAM_DATA
	ret = aw8624x_check_ram_data(aw8624x, aw8624x_fw);
	if (ret)
		aw_err("ram data check sum error");
	else
		aw_info("ram data check sum pass");
#endif

	aw8624x_raminit(aw8624x, false);
	mutex_unlock(&aw8624x->lock);

	aw_info("exit");

	return ret;
}

/*
 * ram bin file loading and update to chip sram
 */
static void aw8624x_ram_load(const struct firmware *cont, void *context)
{
	uint16_t check_sum = 0;
	int i = 0;
	int ret = 0;
	struct aw8624x *aw8624x = context;
	struct aw8624x_container *aw8624x_fw;
#ifdef AW_READ_BIN_FLEXBALLY
	static uint8_t load_cont;
	int ram_timer_val = 1000;

	load_cont++;
#endif

	if (!cont) {
		aw_err("failed to read %s", aw8624x_ram_name);
		release_firmware(cont);

#ifdef AW_READ_BIN_FLEXBALLY
		if (load_cont <= 20) {
			schedule_delayed_work(&aw8624x->ram_work,
					msecs_to_jiffies(ram_timer_val));
			aw_info("start hrtimer:load_cont%u", load_cont);
		}
#endif
		return;
	}

	aw_info("loaded %s - size: %zu", aw8624x_ram_name, cont ? cont->size : 0);
	/* check sum */
	for (i = 2; i < cont->size; i++)
		check_sum += cont->data[i];

	if (check_sum != (uint16_t)((cont->data[0] << 8) | (cont->data[1]))) {
		aw_err("check sum err: check_sum=0x%04X", check_sum);
		release_firmware(cont);
		return;
	}
	aw_info("check sum pass: 0x%04x", check_sum);
	aw8624x->ram.check_sum = check_sum;

	/* aw ram update */
	aw8624x_fw = kzalloc(cont->size + sizeof(int), GFP_KERNEL);
	if (!aw8624x_fw) {
		release_firmware(cont);
		aw_err("Error allocating memory");
		return;
	}
	aw8624x_fw->len = cont->size;
	memcpy(aw8624x_fw->data, cont->data, cont->size);
	release_firmware(cont);
	ret = aw8624x_container_update(aw8624x, aw8624x_fw);
	if (ret) {
		aw_err("ram firmware update failed!");
	} else {
		aw8624x->ram_init = true;
		aw8624x->ram.len = aw8624x_fw->len - aw8624x->ram.ram_shift;
		aw_info("ram firmware update complete!");
		mutex_lock(&aw8624x->lock);
		aw8624x_get_ram_num(aw8624x);
		mutex_unlock(&aw8624x->lock);
	}
	kfree(aw8624x_fw);
	aw8624x_fw = NULL;
}

static int aw8624x_ram_update(struct aw8624x *aw8624x)
{
	aw8624x->ram_init = false;
	aw8624x->rtp_init = false;

	return request_firmware_nowait(THIS_MODULE, 1, aw8624x_ram_name,
				aw8624x->dev, GFP_KERNEL,
				aw8624x, aw8624x_ram_load);
}

static void aw8624x_ram_play(struct aw8624x *aw8624x, uint8_t type)
{
	aw_info("index=%d", aw8624x->index);

	if (type == AW8624X_RAM_LOOP_MODE)
		aw8624x_set_repeat_seq(aw8624x, aw8624x->index);

	if (type == AW8624X_RAM_MODE) {
		aw8624x_set_wav_seq(aw8624x, 0x00, aw8624x->index);
		aw8624x_set_wav_seq(aw8624x, 0x01, 0x00);
		aw8624x_set_wav_loop(aw8624x, 0x00, 0x00);
	}
	aw8624x_play_mode(aw8624x, aw8624x->activate_mode);
	aw8624x_play_go(aw8624x, true);
}

/*
 * cont mode long vibration
 */
static void aw8624x_cont_play(struct aw8624x *aw8624x)
{
	uint8_t reg_val = 0;
	uint8_t state = 0;

	/* add VDD POR verify */
	aw8624x_i2c_read(aw8624x, AW8624X_REG_SYSST2, &state);
	if (!(state & AW8624X_BIT_SYSST2_DVDD_PORN)) {
		aw_err("VDD is NOT POWER ON!!!");
		return;
	}

	aw_info("enter");
	/* work mode */
	aw8624x_play_mode(aw8624x, AW8624X_CONT_MODE);
	/* cont config */
	aw8624x_i2c_write_bits(aw8624x, AW8624X_REG_CONTCFG6,
				(AW8624X_BIT_CONTCFG6_TRACK_EN_MASK &
				AW8624X_BIT_CONTCFG6_DRV1_LVL_MASK),
				(AW8624X_BIT_CONTCFG6_TRACK_ENABLE |
				(uint8_t)aw8624x->dts_info.cont_drv1_lvl));
	reg_val = aw8624x->dts_info.cont_drv2_lvl;
	aw8624x_i2c_write_bits(aw8624x, AW8624X_REG_CONTCFG7, AW8624X_BIT_CONTCFG7_DRV2_LVL_MASK,
				 reg_val);
	/* Half cycle number of RMS drive process */
	reg_val = 0xFF;
	aw8624x_i2c_write(aw8624x, AW8624X_REG_CONTCFG9, reg_val);
	/* cont play go */
	aw8624x_play_go(aw8624x, true);
}

/*
 * motor protection
 * The waveform data larger than prlvl and last
 * more than prtime/3k senonds, chip will stop output
 */
static void aw8624x_protect_config(struct aw8624x *aw8624x, uint8_t prtime, uint8_t prlvl)
{
	uint8_t reg_val = 0;

	aw8624x_i2c_write_bits(aw8624x, AW8624X_REG_PWMCFG1, AW8624X_BIT_PWMCFG1_PRC_EN_MASK,
			AW8624X_BIT_PWMCFG1_PRC_DISABLE);
	if (prlvl != 0) {
		/* Enable protection mode */
		aw_info("enable protection mode");
		reg_val = AW8624X_BIT_PWMCFG3_PR_ENABLE |
			(prlvl & (~AW8624X_BIT_PWMCFG3_PRLVL_MASK));
		aw8624x_i2c_write(aw8624x, AW8624X_REG_PWMCFG3, reg_val);
		aw8624x_i2c_write(aw8624x, AW8624X_REG_PWMCFG4, prtime);
	} else {
		/* Disable */
		aw_info("disable protection mode");
		aw8624x_i2c_write_bits(aw8624x, AW8624X_REG_PWMCFG3,
				AW8624X_BIT_PWMCFG3_PR_EN_MASK,
				AW8624X_BIT_PWMCFG3_PR_DISABLE);
	}
}

/*
 * gets the aw8624x drvier data
 */
inline struct aw8624x *aw8624x_get_drvdata(struct device *dev)
{
#ifdef CONFIG_AW8624X_SAMSUNG_FEATURE
	struct sec_vib_inputff_drvdata *ddata = dev_get_drvdata(dev);
	struct aw8624x *aw8624x = ddata->private_data;
#else
	struct led_classdev *cdev = dev_get_drvdata(dev);
	struct aw8624x *aw8624x = container_of(cdev, struct aw8624x,
					       vib_dev);
#endif

	return aw8624x;
}

/*
 * gets the aw8624x drvier data from the input dev
 */
inline struct aw8624x *aw8624x_input_get_drvdata(struct input_dev *dev)
{
#if defined(CONFIG_AW8624X_SAMSUNG_FEATURE)
	struct sec_vib_inputff_drvdata *ddata = input_get_drvdata(dev);
	struct aw8624x *aw8624x = ddata->private_data;
#else
	struct aw8624x *aw8624x = input_get_drvdata(dev);
#endif

	return aw8624x;
}

/*
 * to do f0 calibration in factory line
 */
void aw8624x_f0_factory_store(struct aw8624x *aw8624x)
{
	int i = 0;

	mutex_lock(&aw8624x->lock);
	/* get lra resistance */
	aw8624x_get_lra_resistance(aw8624x);
	aw8624x->sample_param[7] = aw8624x->lra;
	msleep(300);
	/* f0 cali and get bemf_peak */
	aw8624x_f0_cali(aw8624x);
	msleep(300);
	aw8624x_get_bemf_peak(aw8624x, bemf_peak);
	for (i = 0; i < 5; i++)
		aw8624x->sample_param[i] = bemf_peak[i];
	aw8624x->sample_param[5] = aw8624x->f0;
	aw8624x->sample_param[6] = aw8624x->cont_f0;
	aw8624x->sample_param[8] = aw8624x->f0_cali_data;
	for (i = 0; i < 9; i++)
		aw_info("sample_param[%d]=0x%04x", i, aw8624x->sample_param[i]);

	mutex_unlock(&aw8624x->lock);
}

/*
 * return f0 value in factory line
 */
uint32_t aw8624x_f0_factory_get(struct aw8624x *aw8624x)
{
	return aw8624x->sample_param[5];
}

/*
 * set nv_f0 in factory line and update relevant registers
 */
int aw8624x_nv_f0_store(struct aw8624x *aw8624x, uint32_t nv_f0)
{
	int ret = 0;

	mutex_lock(&aw8624x->lock);
	aw8624x->f0 = nv_f0;
	aw_info("aw8624x->f0 = 0x%02X", aw8624x->f0);
	ret = aw8624x_judge_cali_range(aw8624x);
	if (ret < 0) {
		aw8624x->f0 = aw8624x->sample_param[5];
	}

	aw8624x_calculate_cali_data(aw8624x);
	aw8624x->sample_param[5] = aw8624x->f0;
	aw8624x->sample_param[8] = aw8624x->f0_cali_data;
	/* calculate rtp_drv_width */
	aw8624x->rtp_drv_width = AW8624X_DRV_WIDTH_FORMULA(aw8624x->f0,
					aw8624x->dts_info.cont_track_margin,
					aw8624x->dts_info.cont_brk_gain);
	if (aw8624x->rtp_drv_width < AW8624X_DRV_WIDTH_MIN)
		aw8624x->rtp_drv_width = AW8624X_DRV_WIDTH_MIN;
	if (aw8624x->rtp_drv_width > AW8624X_DRV_WIDTH_MAX)
		aw8624x->rtp_drv_width = AW8624X_DRV_WIDTH_MAX;

	aw_info("rtp_drv_width=0x%02X", aw8624x->rtp_drv_width);
	aw8624x_upload_lra(aw8624x, AW8624X_F0_CALI_LRA);
	mutex_unlock(&aw8624x->lock);

	return 0;
}

static ssize_t state_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct aw8624x *aw8624x = aw8624x_get_drvdata(dev);

	return snprintf(buf, PAGE_SIZE, "%d\n", aw8624x->state);
}

static ssize_t state_store(struct device *dev, struct device_attribute *attr, const char *buf,
			size_t count)
{
	return count;
}

static ssize_t duration_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct aw8624x *aw8624x = aw8624x_get_drvdata(dev);
	ktime_t time_rem;
	uint64_t time_ms = 0;

	if (hrtimer_active(&aw8624x->timer)) {
		time_rem = hrtimer_get_remaining(&aw8624x->timer);
		time_ms = ktime_to_ms(time_rem);
	}

	return snprintf(buf, PAGE_SIZE, "%llu\n", time_ms);
}

static ssize_t duration_store(struct device *dev, struct device_attribute *attr, const char *buf,
			size_t count)
{
	struct aw8624x *aw8624x = aw8624x_get_drvdata(dev);
	uint32_t val = 0;
	int rc = 0;

	rc = kstrtouint(buf, 0, &val);
	if (rc < 0)
		return rc;

	/* setting 0 on duration is NOP for now */
	if (val <= 0)
		return count;

	mutex_lock(&aw8624x->lock);
	aw8624x->duration = val;
	mutex_unlock(&aw8624x->lock);
	aw_info("duration=%d", aw8624x->duration);

	return count;
}

static ssize_t activate_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct aw8624x *aw8624x = aw8624x_get_drvdata(dev);

	/* For now nothing to show */
	return snprintf(buf, PAGE_SIZE, "activate = %d\n", aw8624x->state);
}

static ssize_t activate_store(struct device *dev, struct device_attribute *attr, const char *buf,
			size_t count)
{
	int rc = 0;
	uint32_t val = 0;
	struct aw8624x *aw8624x = aw8624x_get_drvdata(dev);

	if (!aw8624x->ram_init) {
		aw_err("ram init failed, not allow to play!");
		return count;
	}
	rc = kstrtouint(buf, 0, &val);
	if (rc < 0)
		return rc;

	if (val < 0)
		return count;

	aw_info("value=%u", val);

	mutex_lock(&aw8624x->lock);
	hrtimer_cancel(&aw8624x->timer);
	aw8624x->state = val;
	mutex_unlock(&aw8624x->lock);
	schedule_work(&aw8624x->vibrator_work);

	return count;
}

static ssize_t activate_mode_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct aw8624x *aw8624x = aw8624x_get_drvdata(dev);

	return snprintf(buf, PAGE_SIZE, "%u\n", aw8624x->activate_mode);
}

static ssize_t activate_mode_store(struct device *dev, struct device_attribute *attr,
				const char *buf, size_t count)
{
	uint32_t val = 0;
	int rc = 0;
	struct aw8624x *aw8624x = aw8624x_get_drvdata(dev);

	rc = kstrtouint(buf, 0, &val);
	if (rc < 0)
		return rc;

	mutex_lock(&aw8624x->lock);
	aw8624x->activate_mode = val;
	mutex_unlock(&aw8624x->lock);

	return count;
}

static ssize_t index_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct aw8624x *aw8624x = aw8624x_get_drvdata(dev);

	mutex_lock(&aw8624x->lock);
	aw8624x_i2c_read(aw8624x, AW8624X_REG_WAVCFG1, aw8624x->seq);
	aw8624x->index = aw8624x->seq[0];
	mutex_unlock(&aw8624x->lock);

	return snprintf(buf, PAGE_SIZE, "index = %d\n", aw8624x->index);
}

static ssize_t index_store(struct device *dev, struct device_attribute *attr, const char *buf,
			size_t count)
{
	uint32_t val = 0;
	int rc = 0;
	struct aw8624x *aw8624x = aw8624x_get_drvdata(dev);

	rc = kstrtouint(buf, 0, &val);
	if (rc < 0)
		return rc;

	if (val > aw8624x->ram.ram_num) {
		aw_err("input value out of range!");
		return count;
	}
	aw_info("value=%u", val);

	mutex_lock(&aw8624x->lock);
	aw8624x->index = val;
	aw8624x->input_flag = false;
	//aw8624x_set_repeat_seq(aw8624x, aw8624x->index);
	mutex_unlock(&aw8624x->lock);

	return count;
}

static ssize_t gain_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct aw8624x *aw8624x = aw8624x_get_drvdata(dev);
	uint8_t gain = 0;

	mutex_lock(&aw8624x->lock);
	aw8624x_i2c_read(aw8624x, AW8624X_REG_PLAYCFG2, &gain);
	mutex_unlock(&aw8624x->lock);

	return snprintf(buf, PAGE_SIZE, "gain = 0x%02X\n", gain);
}

static ssize_t gain_store(struct device *dev, struct device_attribute *attr, const char *buf,
			size_t count)
{
	struct aw8624x *aw8624x = aw8624x_get_drvdata(dev);
	uint32_t val = 0;
	int rc = 0;

	rc = kstrtouint(buf, 0, &val);
	if (rc < 0)
		return rc;

	aw_info("value=0x%02X", val);
	mutex_lock(&aw8624x->lock);
	aw8624x->gain = val;
	aw8624x_set_gain(aw8624x, aw8624x->gain);
	mutex_unlock(&aw8624x->lock);

	return count;
}

static ssize_t seq_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	uint8_t i = 0;
	size_t count = 0;
	struct aw8624x *aw8624x = aw8624x_get_drvdata(dev);

	mutex_lock(&aw8624x->lock);
	aw8624x_i2c_reads(aw8624x, AW8624X_REG_WAVCFG1, aw8624x->seq, AW8624X_SEQUENCER_SIZE);
	for (i = 0; i < AW8624X_SEQUENCER_SIZE; i++)
		count += snprintf(buf + count, PAGE_SIZE - count, "seq%d = %u\n", i + 1,
				aw8624x->seq[i]);
	mutex_unlock(&aw8624x->lock);

	return count;
}

static ssize_t seq_store(struct device *dev, struct device_attribute *attr, const char *buf,
			size_t count)
{
	struct aw8624x *aw8624x = aw8624x_get_drvdata(dev);
	uint32_t databuf[2] = { 0, 0 };

	if (sscanf(buf, "%X %X", &databuf[0], &databuf[1]) == 2) {
		if (databuf[0] >= AW8624X_SEQUENCER_SIZE ||
				databuf[1] > aw8624x->ram.ram_num) {
			aw_err("input value out of range!");
			return count;
		}
		aw_info("seq%u=0x%02X", databuf[0], databuf[1]);
		mutex_lock(&aw8624x->lock);
		aw8624x->seq[databuf[0]] = databuf[1];
		aw8624x_set_wav_seq(aw8624x, (uint8_t)databuf[0],
					aw8624x->seq[databuf[0]]);
		mutex_unlock(&aw8624x->lock);
	}

	return count;
}

static ssize_t loop_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	uint8_t i = 0;
	size_t count = 0;
	uint8_t reg_val[AW8624X_SEQUENCER_LOOP_SIZE] = {0};
	struct aw8624x *aw8624x = aw8624x_get_drvdata(dev);

	mutex_lock(&aw8624x->lock);
	aw8624x_i2c_reads(aw8624x, AW8624X_REG_WAVCFG9, reg_val, AW8624X_SEQUENCER_LOOP_SIZE);
	for (i = 0; i < AW8624X_SEQUENCER_LOOP_SIZE; i++) {
		aw8624x->loop[i * 2 + 0] = (reg_val[i] >> 4) & 0x0F;
		aw8624x->loop[i * 2 + 1] = (reg_val[i] >> 0) & 0x0F;
		count += snprintf(buf + count, PAGE_SIZE - count, "seq%d loop: 0x%02X\n", i * 2 + 1,
				aw8624x->loop[i * 2 + 0]);
		count += snprintf(buf + count, PAGE_SIZE - count, "seq%d loop: 0x%02X\n", i * 2 + 2,
				aw8624x->loop[i * 2 + 1]);
	}
	mutex_unlock(&aw8624x->lock);

	return count;
}

static ssize_t loop_store(struct device *dev, struct device_attribute *attr, const char *buf,
			size_t count)
{
	uint32_t databuf[2] = { 0, 0 };
	struct aw8624x *aw8624x = aw8624x_get_drvdata(dev);

	if (sscanf(buf, "%x %x", &databuf[0], &databuf[1]) == 2) {
		aw_info("seq%u loop=0x%02X", databuf[0], databuf[1]);

		if (databuf[0] >= AW8624X_SEQUENCER_SIZE || databuf[1] > 0x0f) {
			aw_err("input loop param error, please check!");

			return count;
		}

		mutex_lock(&aw8624x->lock);
		aw8624x->loop[databuf[0]] = databuf[1];
		aw8624x_set_wav_loop(aw8624x, (uint8_t)databuf[0],
					aw8624x->loop[databuf[0]]);
		mutex_unlock(&aw8624x->lock);
	}

	return count;
}

static ssize_t reg_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	ssize_t len = 0;
	uint8_t reg_array[AW8624X_REG_MAX] = {0};
	int i = 0;
	struct aw8624x *aw8624x = aw8624x_get_drvdata(dev);

	mutex_lock(&aw8624x->lock);
	aw8624x_i2c_reads(aw8624x, AW8624X_REG_ID, reg_array, AW8624X_REG_RTPDATA);
	aw8624x_i2c_reads(aw8624x, AW8624X_REG_RTPDATA + 1, &reg_array[AW8624X_REG_RTPDATA + 1],
					AW8624X_REG_RAMDATA - AW8624X_REG_RTPDATA - 1);
	aw8624x_i2c_reads(aw8624x, AW8624X_REG_RAMDATA + 1, &reg_array[AW8624X_REG_RAMDATA + 1],
					AW8624X_REG_ANACFG8 - AW8624X_REG_RAMDATA);
	for (i = 0 ; i < AW8624X_REG_ANACFG8 + 1; i++) {
		len += snprintf(buf + len, PAGE_SIZE - len, "reg:0x%02X=0x%02X\n",
				AW8624X_REG_ID + i, reg_array[i]);
	}
	mutex_unlock(&aw8624x->lock);

	return len;
}

static ssize_t reg_store(struct device *dev, struct device_attribute *attr, const char *buf,
			size_t count)
{
	uint8_t val = 0;
	uint32_t databuf[2] = { 0, 0 };
	struct aw8624x *aw8624x = aw8624x_get_drvdata(dev);

	if (sscanf(buf, "%X %X", &databuf[0], &databuf[1]) == 2) {
		val = databuf[1];
		mutex_lock(&aw8624x->lock);
		aw8624x_i2c_write(aw8624x, (uint8_t)databuf[0], val);
		mutex_unlock(&aw8624x->lock);
	}

	return count;
}

static ssize_t ram_update_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	int i = 0;
	int j = 0;
	int size = 0;
	ssize_t len = 0;
	uint8_t ram_data[AW8624X_RAMDATA_RD_BUFFER_SIZE] = {0};
	uint8_t buffer[AW8624X_RAMDATA_SHOW_LINE_BUFFER_SZIE] = {0};
	uint8_t unit[AW8624X_RAMDATA_SHOW_UINT_SIZE] = {0};
	struct aw8624x *aw8624x = aw8624x_get_drvdata(dev);

	mutex_lock(&aw8624x->lock);
	len += snprintf(buf + len, PAGE_SIZE - len, "Please cat log\n");
	/* RAMINIT Enable */
	aw8624x_raminit(aw8624x, true);
	aw8624x_play_stop(aw8624x);
	aw8624x_set_ram_addr(aw8624x);
	while (i < aw8624x->ram.len) {
		if ((aw8624x->ram.len - i) < AW8624X_RAMDATA_RD_BUFFER_SIZE)
			size = aw8624x->ram.len - i;
		else
			size = AW8624X_RAMDATA_RD_BUFFER_SIZE;

		aw8624x_i2c_reads(aw8624x, AW8624X_REG_RAMDATA, ram_data, size);
		for (j = 0; j < size; j++) {
			sprintf(unit, "0x%02X ", ram_data[j]);
			strcat(buffer, unit);
			if ((j + 1) % AW8624X_RAMDATA_SHOW_COLUMN == 0 || j == size - 1) {
				aw_info("[ram_data]:%s", buffer);
				memset(buffer, 0, AW8624X_RAMDATA_SHOW_LINE_BUFFER_SZIE);
			}
		}
		i += size;
	}
	/* RAMINIT Disable */
	aw8624x_raminit(aw8624x, false);
	mutex_unlock(&aw8624x->lock);

	return len;
}

static ssize_t ram_update_store(struct device *dev, struct device_attribute *attr, const char *buf,
				size_t count)
{
	uint32_t val = 0;
	int rc = 0;
	struct aw8624x *aw8624x = aw8624x_get_drvdata(dev);

	rc = kstrtouint(buf, 0, &val);
	if (rc < 0)
		return rc;
	if (val)
		aw8624x_ram_update(aw8624x);

	return count;
}

/*
 * get real lra_f0
 */
static ssize_t f0_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	ssize_t len = 0;
	struct aw8624x *aw8624x = aw8624x_get_drvdata(dev);

	mutex_lock(&aw8624x->lock);
	aw8624x_upload_lra(aw8624x, AW8624X_WRITE_ZERO);
	aw8624x_get_f0(aw8624x);
	aw8624x_upload_lra(aw8624x, AW8624X_F0_CALI_LRA);
	mutex_unlock(&aw8624x->lock);
	len += snprintf(buf + len, PAGE_SIZE - len, "%u\n", aw8624x->f0);

	return len;
}

static ssize_t f0_store(struct device *dev, struct device_attribute *attr, const char *buf,
			size_t count)
{
	return count;
}

static ssize_t cali_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	ssize_t len = 0;
	uint8_t reg = 0;
	struct aw8624x *aw8624x = aw8624x_get_drvdata(dev);

	mutex_lock(&aw8624x->lock);
	aw8624x_i2c_read(aw8624x, AW8624X_REG_TRIMCFG3, &reg);
	aw8624x_upload_lra(aw8624x, AW8624X_F0_CALI_LRA);
	aw8624x_get_f0(aw8624x);
	aw8624x_i2c_write(aw8624x, AW8624X_REG_TRIMCFG3, reg);
	mutex_unlock(&aw8624x->lock);
	len += snprintf(buf + len, PAGE_SIZE - len, "%u\n", aw8624x->f0);

	return len;
}

static ssize_t cali_store(struct device *dev, struct device_attribute *attr, const char *buf,
			size_t count)
{
	uint32_t val = 0;
	int rc = 0;
	struct aw8624x *aw8624x = aw8624x_get_drvdata(dev);

	rc = kstrtouint(buf, 0, &val);
	if (rc < 0)
		return rc;

	if (val) {
		mutex_lock(&aw8624x->lock);
		aw8624x_f0_cali(aw8624x);
		aw8624x->sample_param[5] = aw8624x->f0;
		aw8624x->sample_param[8] = aw8624x->f0_cali_data;
		mutex_unlock(&aw8624x->lock);
	}

	return count;
}

static ssize_t fitting_coeff_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	ssize_t len = 0;
	struct aw8624x *aw8624x = aw8624x_get_drvdata(dev);

	len += snprintf(buf + len, PAGE_SIZE - len, "fitting_coeff = %u\n",
			aw8624x->fitting_coeff);

	return len;
}

static ssize_t fitting_coeff_store(struct device *dev, struct device_attribute *attr, const char *buf,
			size_t count)
{
	uint32_t val = 0;
	int rc = 0;
	struct aw8624x *aw8624x = aw8624x_get_drvdata(dev);

	rc = kstrtouint(buf, 0, &val);
	if (rc < 0)
		return rc;

	aw_info("update fitting_coeff = %u", val);
	aw8624x->fitting_coeff = val;

	return count;
}

static ssize_t f0_save_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	ssize_t len = 0;
	struct aw8624x *aw8624x = aw8624x_get_drvdata(dev);

	len += snprintf(buf + len, PAGE_SIZE - len, "f0_cali_data = 0x%02X\n",
			aw8624x->f0_cali_data);

	return len;
}

static ssize_t f0_save_store(struct device *dev, struct device_attribute *attr, const char *buf,
			size_t count)
{
	uint32_t val = 0;
	int rc = 0;
	struct aw8624x *aw8624x = aw8624x_get_drvdata(dev);

	rc = kstrtouint(buf, 0, &val);
	if (rc < 0)
		return rc;

	aw_info("update f0_cali_data = %u", val);
	aw8624x->f0_cali_data = val;

	return count;
}

/*
 * cont mode vibration
 */
static ssize_t cont_store(struct device *dev, struct device_attribute *attr, const char *buf,
			size_t count)
{
	uint32_t val = 0;
	int rc = 0;
	struct aw8624x *aw8624x = aw8624x_get_drvdata(dev);

	rc = kstrtouint(buf, 0, &val);
	if (rc < 0)
		return rc;

	mutex_lock(&aw8624x->lock);
	aw8624x_play_stop(aw8624x);
	if (val)
		aw8624x_cont_play(aw8624x);
	mutex_unlock(&aw8624x->lock);

	return count;
}

static ssize_t cont_drv_lvl_show(struct device *dev,
					struct device_attribute *attr,
					char *buf)
{
	struct aw8624x *aw8624x = aw8624x_get_drvdata(dev);
	ssize_t len = 0;

	len += snprintf(buf + len, PAGE_SIZE - len, "cont_drv1_lvl = 0x%02X\n",
			aw8624x->dts_info.cont_drv1_lvl);
	len += snprintf(buf + len, PAGE_SIZE - len, "cont_drv2_lvl = 0x%02X\n",
			aw8624x->dts_info.cont_drv2_lvl);
	return len;
}

static ssize_t cont_drv_lvl_store(struct device *dev,
					struct device_attribute *attr,
					const char *buf, size_t count)
{
	struct aw8624x *aw8624x = aw8624x_get_drvdata(dev);
	uint32_t databuf[2] = { 0, 0 };

	if (sscanf(buf, "%x %x", &databuf[0], &databuf[1]) == 2) {
		aw8624x->dts_info.cont_drv1_lvl = databuf[0];
		aw8624x->dts_info.cont_drv2_lvl = databuf[1];
		aw8624x_i2c_write_bits(aw8624x, AW8624X_REG_CONTCFG6,
				AW8624X_BIT_CONTCFG6_DRV1_LVL_MASK,
				(uint8_t)aw8624x->dts_info.cont_drv1_lvl);
		aw8624x_i2c_write_bits(aw8624x, AW8624X_REG_CONTCFG7,
				AW8624X_BIT_CONTCFG7_DRV2_LVL_MASK,
				(uint8_t)aw8624x->dts_info.cont_drv2_lvl);
	}
	return count;
}

static ssize_t cont_drv_time_show(struct device *dev,
					struct device_attribute *attr,
					char *buf)
{
	struct aw8624x *aw8624x = aw8624x_get_drvdata(dev);
	ssize_t len = 0;

	len += snprintf(buf + len, PAGE_SIZE - len, "cont_drv1_time = 0x%02X\n",
			aw8624x->dts_info.cont_drv1_time);
	len += snprintf(buf + len, PAGE_SIZE - len, "cont_drv2_time = 0x%02X\n",
			aw8624x->dts_info.cont_drv2_time);
	return len;
}

static ssize_t cont_drv_time_store(struct device *dev,
					struct device_attribute *attr,
					const char *buf, size_t count)
{
	struct aw8624x *aw8624x = aw8624x_get_drvdata(dev);
	uint32_t databuf[2] = { 0, 0 };

	if (sscanf(buf, "%x %x", &databuf[0], &databuf[1]) == 2) {
		aw8624x->dts_info.cont_drv1_time = databuf[0];
		aw8624x->dts_info.cont_drv2_time = databuf[1];
		aw8624x_i2c_write(aw8624x, AW8624X_REG_CONTCFG8,
				(uint8_t)aw8624x->dts_info.cont_drv1_time);
		aw8624x_i2c_write(aw8624x, AW8624X_REG_CONTCFG9,
				(uint8_t)aw8624x->dts_info.cont_drv2_time);
	}
	return count;
}

static ssize_t cont_brk_time_show(struct device *dev,
					struct device_attribute *attr,
					char *buf)
{
	struct aw8624x *aw8624x = aw8624x_get_drvdata(dev);
	ssize_t len = 0;

	len += snprintf(buf + len, PAGE_SIZE - len, "cont_brk_time = 0x%02X\n",
			aw8624x->dts_info.cont_brk_time);
	return len;
}

static ssize_t cont_brk_time_store(struct device *dev,
					struct device_attribute *attr,
					const char *buf, size_t count)
{
	struct aw8624x *aw8624x = aw8624x_get_drvdata(dev);
	int err = 0;
	int val = 0;

	err = kstrtoint(buf, 16, &val);
	if (err != 0) {
		aw_err("format not match!");
		return count;
	}
	mutex_lock(&aw8624x->lock);
	aw8624x->dts_info.cont_brk_time = val;
	aw8624x_i2c_write(aw8624x, AW8624X_REG_CONTCFG10,
			(uint8_t)aw8624x->dts_info.cont_brk_time);
	mutex_unlock(&aw8624x->lock);

	return count;
}

static ssize_t vbat_monitor_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	ssize_t len = 0;
	struct aw8624x *aw8624x = aw8624x_get_drvdata(dev);

	mutex_lock(&aw8624x->lock);
	aw8624x_get_vbat(aw8624x);
	len += snprintf(buf + len, PAGE_SIZE - len, "vbat_monitor = %u\n", aw8624x->vbat);
	mutex_unlock(&aw8624x->lock);

	return len;
}

static ssize_t vbat_monitor_store(struct device *dev, struct device_attribute *attr,
				const char *buf, size_t count)
{
	return count;
}

static ssize_t lra_resistance_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	ssize_t len = 0;
	struct aw8624x *aw8624x = aw8624x_get_drvdata(dev);

	mutex_lock(&aw8624x->lock);
	aw8624x_get_lra_resistance(aw8624x);
	len += snprintf(buf + len, PAGE_SIZE - len, "lra_resistance = %u\n", aw8624x->lra);
	mutex_unlock(&aw8624x->lock);

	return len;
}

static ssize_t lra_resistance_store(struct device *dev, struct device_attribute *attr,
				const char *buf, size_t count)
{
	return count;
}

static ssize_t prctmode_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	ssize_t len = 0;
	uint8_t reg_val = 0;
	struct aw8624x *aw8624x = aw8624x_get_drvdata(dev);

	mutex_lock(&aw8624x->lock);
	aw8624x_i2c_read(aw8624x, AW8624X_REG_PWMCFG3, &reg_val);
	reg_val >>= 7;
	len += snprintf(buf + len, PAGE_SIZE - len, "prctmode = %u\n", reg_val);
	mutex_unlock(&aw8624x->lock);

	return len;
}

static ssize_t prctmode_store(struct device *dev, struct device_attribute *attr, const char *buf,
			size_t count)
{
	uint32_t databuf[2] = { 0, 0 };
	uint32_t prtime = 0;
	uint32_t prlvl = 0;
	struct aw8624x *aw8624x = aw8624x_get_drvdata(dev);

	if (sscanf(buf, "%x %x", &databuf[0], &databuf[1]) == 2) {
		prtime = databuf[0];
		prlvl = databuf[1];
		mutex_lock(&aw8624x->lock);
		aw8624x_protect_config(aw8624x, prtime, prlvl);
		mutex_unlock(&aw8624x->lock);
	}

	return count;
}

static ssize_t ram_vbat_comp_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	ssize_t len = 0;
	struct aw8624x *aw8624x = aw8624x_get_drvdata(dev);

	len += snprintf(buf + len, PAGE_SIZE - len, "ram_vbat_comp = %u\n",
			aw8624x->ram_vbat_comp);

	return len;
}

static ssize_t ram_vbat_comp_store(struct device *dev, struct device_attribute *attr,
				const char *buf, size_t count)
{
	uint32_t val = 0;
	int rc = 0;
	struct aw8624x *aw8624x = aw8624x_get_drvdata(dev);

	rc = kstrtouint(buf, 0, &val);
	if (rc < 0)
		return rc;

	mutex_lock(&aw8624x->lock);
	if (val)
		aw8624x->ram_vbat_comp = AW8624X_RAM_VBAT_COMP_ENABLE;
	else
		aw8624x->ram_vbat_comp = AW8624X_RAM_VBAT_COMP_DISABLE;
	mutex_unlock(&aw8624x->lock);

	return count;
}

static ssize_t ram_num_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	ssize_t len = 0;
	struct aw8624x *aw8624x = aw8624x_get_drvdata(dev);

	mutex_lock(&aw8624x->lock);
	aw8624x_get_ram_num(aw8624x);
	mutex_unlock(&aw8624x->lock);
	len += snprintf(buf + len, PAGE_SIZE - len, "ram_num = %u\n", aw8624x->ram.ram_num);

	return len;
}

static ssize_t rtp_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	ssize_t len = 0;
	struct aw8624x *aw8624x = aw8624x_get_drvdata(dev);

	len += snprintf(buf + len, PAGE_SIZE - len, "rtp_cnt = %u", aw8624x->rtp_cnt);

	return len;
}

static ssize_t rtp_store(struct device *dev, struct device_attribute *attr, const char *buf,
			 size_t count)
{
	uint32_t val = 0;
	int rc = 0;
	struct aw8624x *aw8624x = aw8624x_get_drvdata(dev);

	rc = kstrtouint(buf, 0, &val);
	if (rc < 0) {
		aw_err("kstrtouint fail");
		return count;
	}
	if (!aw8624x->ram.base_addr) {
		aw_err("no set FIFO space for rtp play!");
		return count;
	}
	mutex_lock(&aw8624x->lock);
	if ((val > 0) && (val < aw8624x->rtp_num_max)) {
		aw8624x->state = 1;
		aw8624x->effect_id = val + AW_RAM_MAX_INDEX;
		schedule_work(&aw8624x->rtp_work);
	} else if (val == 0) {
		aw_info("rtp play stop");
		aw8624x->state = 0;
		schedule_work(&aw8624x->rtp_work);
	} else {
		aw_err("input number error:%u", val);
	}
	mutex_unlock(&aw8624x->lock);

	return count;
}

static ssize_t osc_cali_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	ssize_t len = 0;
	struct aw8624x *aw8624x = aw8624x_get_drvdata(dev);

	len += snprintf(buf + len, PAGE_SIZE - len, "osc_cali_data = 0x%02X\n",
			aw8624x->osc_cali_data);

	return len;
}

static ssize_t osc_cali_store(struct device *dev, struct device_attribute *attr, const char *buf,
			size_t count)
{
	uint32_t val = 0;
	int rc = 0;
	struct aw8624x *aw8624x = aw8624x_get_drvdata(dev);

	rc = kstrtouint(buf, 0, &val);
	if (rc < 0)
		return rc;

	mutex_lock(&aw8624x->lock);
	if (val == 1) {
		aw8624x_upload_lra(aw8624x, AW8624X_WRITE_ZERO);
		aw8624x_rtp_osc_calibration(aw8624x);
		aw8624x_rtp_trim_lra_calibration(aw8624x);
	} else if (val == 2) {
		aw8624x_upload_lra(aw8624x, AW8624X_OSC_CALI_LRA);
		aw8624x_rtp_osc_calibration(aw8624x);
	} else {
		aw_err("input value out of range");
	}
	/* osc calibration flag end, other behaviors are permitted */
	mutex_unlock(&aw8624x->lock);

	return count;
}

static ssize_t osc_save_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	ssize_t len = 0;
	struct aw8624x *aw8624x = aw8624x_get_drvdata(dev);

	len += snprintf(buf + len, PAGE_SIZE - len, "osc_cali_data = 0x%02X\n",
			aw8624x->osc_cali_data);

	return len;
}

static ssize_t osc_save_store(struct device *dev, struct device_attribute *attr, const char *buf,
			size_t count)
{
	uint32_t val = 0;
	int rc = 0;
	struct aw8624x *aw8624x = aw8624x_get_drvdata(dev);

	rc = kstrtouint(buf, 0, &val);
	if (rc < 0)
		return count;
	aw8624x->osc_cali_data = val;

	return count;
}

static ssize_t f0_cali_confirm_show(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	if (aw8624x_f0_cali_data <= 0x7F)
		return snprintf(buf, PAGE_SIZE, "Yes\n");
	else
		return snprintf(buf, PAGE_SIZE, "No\n");
}

static ssize_t effect_id_show(struct device *dev,
			struct device_attribute *attr, char *buf)
{
	struct aw8624x *aw8624x = aw8624x_get_drvdata(dev);

	return snprintf(buf, PAGE_SIZE, "0x%02x\n", aw8624x->effect_id);
}

static ssize_t effect_id_store(struct device *dev,
				struct device_attribute *attr,
				const char *buf, size_t count)
{
	uint32_t val = 0;
	int rc = 0;
	struct aw8624x *aw8624x = aw8624x_get_drvdata(dev);

	rc = kstrtouint(buf, 0, &val);
	if (rc < 0)
		return rc;
	aw_info("value=%u", val);
	mutex_lock(&aw8624x->lock);
	aw8624x->input_flag = true;
	if (val == AW_RAM_LONG_INDEX) {
		aw8624x->activate_mode = AW8624X_RAM_LOOP_MODE;
		aw8624x->effect_id = val;
	} else if (val > 0 && val <= aw8624x->ram.ram_num) {
		aw8624x->activate_mode = AW8624X_RAM_MODE;
		aw8624x->effect_id = val;
	} else {
		aw_err("val is error");
	}
	mutex_unlock(&aw8624x->lock);
	return count;
}

static ssize_t rtp_cali_select_show(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	struct aw8624x *aw8624x = aw8624x_get_drvdata(dev);

	switch (aw8624x->rtp_cali_select) {
	case AW8624X_WRITE_ZERO:
		return snprintf(buf, PAGE_SIZE, "rtp mode use zero cali data\n");
	case AW8624X_F0_CALI_LRA:
		return snprintf(buf, PAGE_SIZE, "rtp mode use f0 cali data\n");
	case AW8624X_OSC_CALI_LRA:
		return snprintf(buf, PAGE_SIZE, "rtp mode use osc cali data\n");
	default:
		return 0;
	}
}

static ssize_t rtp_cali_select_store(struct device *dev,
				struct device_attribute *attr,
				const char *buf, size_t count)
{
	struct aw8624x *aw8624x = aw8624x_get_drvdata(dev);
	uint32_t val = 0;
	int rc = 0;

	rc = kstrtouint(buf, 0, &val);
	if (rc < 0)
		return rc;

	switch (val) {
	case AW8624X_WRITE_ZERO:
		aw_info("rtp mode use zero cali data");
		break;
	case AW8624X_F0_CALI_LRA:
		aw_info("rtp mode use f0 cali data");
		break;
	case AW8624X_OSC_CALI_LRA:
		aw_info("rtp mode use osc cali data");
		break;
	default:
		val = AW8624X_OSC_CALI_LRA;
		aw_err("input error, osc cali data is used by default");
		break;
	}
	aw8624x->rtp_cali_select = val;

	return count;
}

static ssize_t smart_loop_store(struct device *dev, struct device_attribute *attr,
				const char *buf, size_t count)
{
	uint32_t val = 0;
	int rc = 0;
	struct aw8624x *aw8624x = aw8624x_get_drvdata(dev);

	rc = kstrtouint(buf, 0, &val);
	if (rc < 0)
		return rc;

	mutex_lock(&aw8624x->lock);
	if (val) {
		aw8624x->dts_info.is_enabled_smart_loop = 1;
		aw8624x_i2c_write_bits(aw8624x, AW8624X_REG_CONTCFG5,
				AW8624X_BIT_CONTCFG5_SMART_LOOP_MASK,
				aw8624x->dts_info.is_enabled_smart_loop << 4);
	} else {
		aw8624x->dts_info.is_enabled_smart_loop = 0;
		aw8624x_i2c_write_bits(aw8624x, AW8624X_REG_CONTCFG5,
				AW8624X_BIT_CONTCFG5_SMART_LOOP_MASK,
				aw8624x->dts_info.is_enabled_smart_loop << 4);
	}
	mutex_unlock(&aw8624x->lock);

	return count;
}

static ssize_t smart_loop_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	ssize_t len = 0;
	struct aw8624x *aw8624x = aw8624x_get_drvdata(dev);

	len += snprintf(buf + len, PAGE_SIZE - len, "is_enabled_smart_loop = %d\n",
							aw8624x->dts_info.is_enabled_smart_loop);

	return len;
}

static ssize_t inter_brake_store(struct device *dev, struct device_attribute *attr,
				const char *buf, size_t count)
{
	uint32_t val = 0;
	int rc = 0;
	struct aw8624x *aw8624x = aw8624x_get_drvdata(dev);

	rc = kstrtouint(buf, 0, &val);
	if (rc < 0)
		return rc;

	mutex_lock(&aw8624x->lock);
	if (val) {
		aw8624x->dts_info.is_enabled_inter_brake = 1;
		aw8624x_i2c_write_bits(aw8624x, AW8624X_REG_SYSCTRL1,
				AW8624X_BIT_SYSCTRL1_INT_BRK_MASK,
				aw8624x->dts_info.is_enabled_inter_brake << 6);
	} else {
		aw8624x->dts_info.is_enabled_inter_brake = 0;
		aw8624x_i2c_write_bits(aw8624x, AW8624X_REG_SYSCTRL1,
				AW8624X_BIT_SYSCTRL1_INT_BRK_MASK,
				aw8624x->dts_info.is_enabled_inter_brake << 6);
	}
	mutex_unlock(&aw8624x->lock);

	return count;
}

static ssize_t inter_brake_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	ssize_t len = 0;
	struct aw8624x *aw8624x = aw8624x_get_drvdata(dev);

	len += snprintf(buf + len, PAGE_SIZE - len, "is_enabled_inter_brake = %d\n",
							aw8624x->dts_info.is_enabled_inter_brake);

	return len;
}

static ssize_t auto_brk_store(struct device *dev, struct device_attribute *attr,
				const char *buf, size_t count)
{
	uint32_t val = 0;
	int rc = 0;
	struct aw8624x *aw8624x = aw8624x_get_drvdata(dev);

	rc = kstrtouint(buf, 0, &val);
	if (rc < 0)
		return rc;

	mutex_lock(&aw8624x->lock);
	if (val) {
		aw8624x->dts_info.is_enabled_auto_brk = 1;
		aw8624x_auto_brk_config(aw8624x, aw8624x->dts_info.is_enabled_auto_brk);
	} else {
		aw8624x->dts_info.is_enabled_auto_brk = 0;
		aw8624x_auto_brk_config(aw8624x, aw8624x->dts_info.is_enabled_auto_brk);
	}
	mutex_unlock(&aw8624x->lock);

	return count;
}

static ssize_t auto_brk_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	ssize_t len = 0;
	struct aw8624x *aw8624x = aw8624x_get_drvdata(dev);

	len += snprintf(buf + len, PAGE_SIZE - len, "is_enabled_auto_brk = %d\n",
							aw8624x->dts_info.is_enabled_auto_brk);

	return len;
}

static ssize_t d2s_gain_store(struct device *dev, struct device_attribute *attr,
				const char *buf, size_t count)
{
	uint32_t val = 0;
	int rc = 0;
	struct aw8624x *aw8624x = aw8624x_get_drvdata(dev);

	rc = kstrtouint(buf, 0, &val);
	if (rc < 0)
		return rc;

	mutex_lock(&aw8624x->lock);
	switch (val) {
	case 1:
		aw8624x->dts_info.d2s_gain = 0;
		break;
	case 2:
		aw8624x->dts_info.d2s_gain = 1;
		break;
	case 4:
		aw8624x->dts_info.d2s_gain = 2;
		break;
	case 5:
		aw8624x->dts_info.d2s_gain = 3;
		break;
	case 8:
		aw8624x->dts_info.d2s_gain = 4;
		break;
	case 10:
		aw8624x->dts_info.d2s_gain = 5;
		break;
	case 20:
		aw8624x->dts_info.d2s_gain = 6;
		break;
	case 40:
		aw8624x->dts_info.d2s_gain = 7;
		break;
	default:
		aw_err("err d2s_gain param");
		break;
	}
	aw8624x_i2c_write_bits(aw8624x, AW8624X_REG_SYSCTRL5,
					AW8624X_BIT_SYSCTRL5_D2S_GAIN_MASK,
					aw8624x->dts_info.d2s_gain);
	mutex_unlock(&aw8624x->lock);

	return count;
}

static ssize_t d2s_gain_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	ssize_t len = 0;
	int d2s_gain = 0;
	struct aw8624x *aw8624x = aw8624x_get_drvdata(dev);

	d2s_gain = aw8624x_select_d2s_gain(aw8624x->dts_info.d2s_gain);
	len += snprintf(buf + len, PAGE_SIZE - len, "d2s_gain = %d\n", d2s_gain);

	return len;
}

static ssize_t f0_d2s_gain_store(struct device *dev, struct device_attribute *attr,
				const char *buf, size_t count)
{
	uint32_t val = 0;
	int rc = 0;
	struct aw8624x *aw8624x = aw8624x_get_drvdata(dev);

	rc = kstrtouint(buf, 0, &val);
	if (rc < 0)
		return rc;

	mutex_lock(&aw8624x->lock);
	switch (val) {
	case 1:
		aw8624x->dts_info.f0_d2s_gain = 0;
		break;
	case 2:
		aw8624x->dts_info.f0_d2s_gain = 1;
		break;
	case 4:
		aw8624x->dts_info.f0_d2s_gain = 2;
		break;
	case 5:
		aw8624x->dts_info.f0_d2s_gain = 3;
		break;
	case 8:
		aw8624x->dts_info.f0_d2s_gain = 4;
		break;
	case 10:
		aw8624x->dts_info.f0_d2s_gain = 5;
		break;
	case 20:
		aw8624x->dts_info.f0_d2s_gain = 6;
		break;
	case 40:
		aw8624x->dts_info.f0_d2s_gain = 7;
		break;
	default:
		aw_err("err f0_d2s_gain param");
	}
	aw8624x_i2c_write_bits(aw8624x, AW8624X_REG_SYSCTRL6,
				AW8624X_BIT_SYSCTRL6_F0_D2S_GAIN_MASK,
				aw8624x->dts_info.f0_d2s_gain);
	mutex_unlock(&aw8624x->lock);

	return count;
}

static ssize_t f0_d2s_gain_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	ssize_t len = 0;
	int f0_d2s_gain = 0;
	struct aw8624x *aw8624x = aw8624x_get_drvdata(dev);

	f0_d2s_gain = aw8624x_select_d2s_gain(aw8624x->dts_info.f0_d2s_gain);
	len += snprintf(buf + len, PAGE_SIZE - len, "f0_d2s_gain = %d\n",
					f0_d2s_gain);

	return len;
}

static ssize_t output_f0_show(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	struct aw8624x *aw8624x = aw8624x_get_drvdata(dev);
	uint32_t val = 0;

	if (aw8624x->f0_cali_data <= 0x3F)
		val = aw8624x->dts_info.f0_pre * (10000 + aw8624x->f0_cali_data * 24) / 10000;
	else
		val = aw8624x->dts_info.f0_pre * (10000 - (0x80 - aw8624x->f0_cali_data) * 24) / 10000;

	return snprintf(buf, PAGE_SIZE, "%u\n", val);
}

static ssize_t golden_param_store(struct device *dev, struct device_attribute *attr,
				const char *buf, size_t count)
{
	uint32_t param[8] = {0};
	struct aw8624x *aw8624x = aw8624x_get_drvdata(dev);

	if (sscanf(buf, "%x %x %x %x %x %x %x %x", &param[0], &param[1], &param[2],
						&param[3], &param[4], &param[5],
						&param[6], &param[7]) == 8) {
		aw8624x->golden_param[0] = param[0];
		aw8624x->golden_param[1] = param[1];
		aw8624x->golden_param[2] = param[2];
		aw8624x->golden_param[3] = param[3];
		aw8624x->golden_param[4] = param[4];
		aw8624x->golden_param[5] = param[5];
		aw8624x->golden_param[6] = param[6];
		aw8624x->golden_param[7] = param[7];
	}

	return count;
}

static ssize_t golden_param_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	ssize_t len = 0;
	struct aw8624x *aw8624x = aw8624x_get_drvdata(dev);

	return snprintf(buf, PAGE_SIZE, "0x%04X,0x%04X,0x%04X,0x%04X,0x%04X,0x%04X,0x%04X,0x%04X\n",
			aw8624x->golden_param[0], aw8624x->golden_param[1],
			aw8624x->golden_param[2], aw8624x->golden_param[3],
			aw8624x->golden_param[4], aw8624x->golden_param[5],
			aw8624x->golden_param[6], aw8624x->golden_param[7]);

	return len;
}

static ssize_t sample_param_store(struct device *dev, struct device_attribute *attr,
				   const char *buf, size_t count)
{
	uint32_t param[9] = {0};
	struct aw8624x *aw8624x = aw8624x_get_drvdata(dev);

	if (sscanf(buf, "%x %x %x %x %x %x %x %x %x", &param[0], &param[1], &param[2],
						&param[3], &param[4], &param[5],
						&param[6], &param[7], &param[8]) == 9) {
		aw8624x->sample_param[0] = param[0];
		aw8624x->sample_param[1] = param[1];
		aw8624x->sample_param[2] = param[2];
		aw8624x->sample_param[3] = param[3];
		aw8624x->sample_param[4] = param[4];
		aw8624x->sample_param[5] = param[5];
		aw8624x->sample_param[6] = param[6];
		aw8624x->sample_param[7] = param[7];
		aw8624x->sample_param[8] = param[8];
	}

	return count;
}

static ssize_t sample_param_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct aw8624x *aw8624x = aw8624x_get_drvdata(dev);

	return snprintf(buf, PAGE_SIZE, "0x%04X,0x%04X,0x%04X,0x%04X,0x%04X,0x%04X,0x%04X,0x%04X,0x%04X\n",
			aw8624x->sample_param[0], aw8624x->sample_param[1],
			aw8624x->sample_param[2], aw8624x->sample_param[3],
			aw8624x->sample_param[4], aw8624x->sample_param[5],
			aw8624x->sample_param[6], aw8624x->sample_param[7],
			aw8624x->sample_param[8]);
}

static DEVICE_ATTR_RW(state);
static DEVICE_ATTR_RW(reg);
static DEVICE_ATTR_RW(duration);
static DEVICE_ATTR_RW(activate);
static DEVICE_ATTR_RW(activate_mode);
static DEVICE_ATTR_RW(index);
static DEVICE_ATTR_RW(gain);
static DEVICE_ATTR_RW(seq);
static DEVICE_ATTR_RW(loop);
static DEVICE_ATTR_RW(ram_update);
static DEVICE_ATTR_RW(f0);
static DEVICE_ATTR_RW(cali);
static DEVICE_ATTR_RW(f0_save);
static DEVICE_ATTR_RW(fitting_coeff);
static DEVICE_ATTR_WO(cont);
static DEVICE_ATTR_RW(cont_drv_lvl);
static DEVICE_ATTR_RW(cont_drv_time);
static DEVICE_ATTR_RW(cont_brk_time);
static DEVICE_ATTR_RW(vbat_monitor);
static DEVICE_ATTR_RW(lra_resistance);
static DEVICE_ATTR_RW(prctmode);
static DEVICE_ATTR_RW(ram_vbat_comp);
static DEVICE_ATTR_RO(ram_num);
static DEVICE_ATTR_RW(rtp);
static DEVICE_ATTR_RW(osc_cali);
static DEVICE_ATTR_RW(osc_save);
static DEVICE_ATTR_RO(f0_cali_confirm);
static DEVICE_ATTR_RW(effect_id);
static DEVICE_ATTR_RW(rtp_cali_select);
static DEVICE_ATTR_RW(smart_loop);
static DEVICE_ATTR_RW(inter_brake);
static DEVICE_ATTR_RW(auto_brk);
static DEVICE_ATTR_RW(d2s_gain);
static DEVICE_ATTR_RW(f0_d2s_gain);
static DEVICE_ATTR_RO(output_f0);
static DEVICE_ATTR_RW(golden_param);
static DEVICE_ATTR_RW(sample_param);

static struct attribute *vibrator_attributes[] = {
	&dev_attr_state.attr,
	&dev_attr_duration.attr,
	&dev_attr_activate.attr,
	&dev_attr_activate_mode.attr,
	&dev_attr_index.attr,
	&dev_attr_gain.attr,
	&dev_attr_seq.attr,
	&dev_attr_loop.attr,
	&dev_attr_reg.attr,
	&dev_attr_ram_update.attr,
	&dev_attr_f0.attr,
	&dev_attr_cali.attr,
	&dev_attr_f0_save.attr,
	&dev_attr_fitting_coeff.attr,
	&dev_attr_cont.attr,
	&dev_attr_cont_drv_lvl.attr,
	&dev_attr_cont_drv_time.attr,
	&dev_attr_cont_brk_time.attr,
	&dev_attr_vbat_monitor.attr,
	&dev_attr_lra_resistance.attr,
	&dev_attr_prctmode.attr,
	&dev_attr_ram_vbat_comp.attr,
	&dev_attr_ram_num.attr,
	&dev_attr_rtp.attr,
	&dev_attr_osc_cali.attr,
	&dev_attr_osc_save.attr,
	&dev_attr_f0_cali_confirm.attr,
	&dev_attr_effect_id.attr,
	&dev_attr_rtp_cali_select.attr,
	&dev_attr_smart_loop.attr,
	&dev_attr_inter_brake.attr,
	&dev_attr_auto_brk.attr,
	&dev_attr_d2s_gain.attr,
	&dev_attr_f0_d2s_gain.attr,
	&dev_attr_output_f0.attr,
	&dev_attr_golden_param.attr,
	&dev_attr_sample_param.attr,
	NULL
};

static struct attribute_group aw8624x_vibrator_attribute_group = {
	.attrs = vibrator_attributes
};

/*
 * judge whether haptic is in rtp going state
 */
static uint8_t aw8624x_judge_rtp_going(struct aw8624x *aw8624x)
{
	uint8_t glb_state = 0;

	aw8624x_i2c_read(aw8624x, AW8624X_REG_GLBRD5, &glb_state);
	if (glb_state == AW8624X_BIT_GLBRD5_STATE_RTP_GO) {
		aw_info("rtp_routine_on");
		return 1; /*is going on */
	}

	return 0;
}

/*
 * ram long vibration timer callback function
 */
static enum hrtimer_restart aw8624x_vibrator_timer_func(struct hrtimer *timer)
{
	struct aw8624x *aw8624x = container_of(timer, struct aw8624x, timer);

	aw_info("enter");
	queue_work(aw8624x->work_queue, &aw8624x->stop_work);

	return HRTIMER_NORESTART;
}

/*
 * under the input architecture, start a ram or ram_loop play
 */
static int aw8624x_haptic_play_effect_seq(struct aw8624x *aw8624x, unsigned char flag)
{
	aw_info("enter, aw8624x->effect_id=%d, activate_mode=%u", aw8624x->effect_id,
		aw8624x->activate_mode);

	if (aw8624x->effect_id > aw8624x->ram.ram_num) {
		aw_err("aw8624x->effect_id out of range.");
		return 0;
	}

	if (flag) {
		if (aw8624x->activate_mode == AW8624X_RAM_MODE) {
			aw8624x_set_wav_seq(aw8624x, 0x00, (uint8_t)aw8624x->effect_id);
			aw8624x_set_wav_seq(aw8624x, 0x01, 0x00);
			aw8624x_set_wav_loop(aw8624x, 0x00, 0x00);
			aw8624x_play_mode(aw8624x, AW8624X_RAM_MODE);
			aw8624x_set_gain(aw8624x, aw8624x->gain);
			aw8624x_play_go(aw8624x, true);
		}
		if (aw8624x->activate_mode == AW8624X_RAM_LOOP_MODE) {
			/* aw8624x_set_combined_seq(aw8624x, 1); */ /* overdrive one cycle */
			aw8624x_set_wav_seq(aw8624x, 0x00, AW_RAM_LONG_INDEX);
			aw8624x_set_wav_loop(aw8624x, 0x00,
					AW8624X_BIT_WAVLOOP_SEQ_EVEN_INIFINITELY);
			aw8624x_play_mode(aw8624x, AW8624X_RAM_LOOP_MODE);
			aw8624x_play_go(aw8624x, true);
		}
	}
	aw_info("exit");
	return 0;
}

/*
 * the callback function of the workqueue
 */
static void aw8624x_vibrator_work_routine(struct work_struct *work)
{
	uint8_t state = 0;
	uint8_t val = 0;
	struct aw8624x *aw8624x = container_of(work, struct aw8624x, vibrator_work);

	mutex_lock(&aw8624x->lock);
	aw_info("enter, state=%d activate_mode=%d duration=%d f0:%d f0_cali=%d",
		aw8624x->state, aw8624x->activate_mode, aw8624x->duration, aw8624x->f0, aw8624x->f0_cali_data);

	/* add VDD POR verify */
	aw8624x_i2c_read(aw8624x, AW8624X_REG_SYSST2, &state);
	if (!(state & AW8624X_BIT_SYSST2_DVDD_PORN)) {
		aw_err("VDD is NOT POWER ON!!!");
		mutex_unlock(&aw8624x->lock);
		return;
	}

	/* update cont_drv_width */
	val = aw8624x->drv_width;
	aw_info("update drv_width=0x%02X", aw8624x->drv_width);
	aw8624x_i2c_write(aw8624x, AW8624X_REG_CONTCFG3, val);

	/* Enter standby mode */
	aw8624x_play_stop(aw8624x);
	if (aw8624x->state) {
		aw8624x_upload_lra(aw8624x, AW8624X_F0_CALI_LRA);
		if (aw8624x->activate_mode == AW8624X_RAM_MODE) {
			aw8624x_ram_vbat_comp(aw8624x, false);
			if (aw8624x->input_flag)
				aw8624x_haptic_play_effect_seq(aw8624x, true);
			else
				aw8624x_ram_play(aw8624x, AW8624X_RAM_MODE);
		} else if (aw8624x->activate_mode == AW8624X_RAM_LOOP_MODE) {
			aw8624x_ram_vbat_comp(aw8624x, false);
			if (aw8624x->input_flag)
				aw8624x_haptic_play_effect_seq(aw8624x, true);
			else
				aw8624x_ram_play(aw8624x, AW8624X_RAM_LOOP_MODE);
			/* run ms timer */
			hrtimer_start(&aw8624x->timer, ktime_set(aw8624x->duration / 1000,
				(aw8624x->duration % 1000) * 1000000), HRTIMER_MODE_REL);
			pm_stay_awake(aw8624x->dev);
			aw8624x->wk_lock_flag = 1;
		} else if (aw8624x->activate_mode == AW8624X_CONT_MODE) {
			aw_info("enter cont mode");
			aw8624x_cont_play(aw8624x);
			/* run ms timer */
			hrtimer_start(&aw8624x->timer, ktime_set(aw8624x->duration / 1000,
				(aw8624x->duration % 1000) * 1000000), HRTIMER_MODE_REL);
		} else {
			aw_err("activate_mode error");
		}
	} else {
		if (aw8624x->wk_lock_flag == 1) {
			pm_relax(aw8624x->dev);
			aw8624x->wk_lock_flag = 0;
		}
	}
	mutex_unlock(&aw8624x->lock);
}

/*
 * wait haptic enter rtp mode
 */
static int aw8624x_wait_enter_rtp_mode(struct aw8624x *aw8624x, int cnt)
{
	bool rtp_work_flag = false;
	uint8_t ret = 0;

	while (cnt) {
		ret = aw8624x_judge_rtp_going(aw8624x);
		if (ret) {
			rtp_work_flag = true;
			aw_info("RTP_GO!");
			break;
		}
		cnt--;
		usleep_range(2000, 2500);
	}
	if (!rtp_work_flag) {
		aw8624x_play_stop(aw8624x);
		aw_err("failed to enter RTP_GO status!");
		return -ERANGE;
	}

	return 0;
}

/*
 * rtp playback configuration
 */
static void aw8624x_rtp_work_routine(struct work_struct *work)
{
	const struct firmware *rtp_file;
	uint8_t val = 0;
	uint8_t state = 0;
	int ret = 0;
	int effect_id_max = 0;
	struct aw8624x *aw8624x = container_of(work, struct aw8624x, rtp_work);
	int rtp_cali_len = 0;

	mutex_lock(&aw8624x->lock);
	aw_info("enter");

	/* add VDD POR verify */
	aw8624x_i2c_read(aw8624x, AW8624X_REG_SYSST2, &state);
	if (!(state & AW8624X_BIT_SYSST2_DVDD_PORN)) {
		aw_err("VDD is NOT POWER ON!!!");
		mutex_unlock(&aw8624x->lock);
		return;
	}

	effect_id_max = sizeof(aw8624x_rtp_name) / sizeof(*aw8624x_rtp_name) +
			AW_RAM_MAX_INDEX - 1;
	if ((aw8624x->effect_id <= AW_RAM_MAX_INDEX) ||
		(aw8624x->effect_id > effect_id_max)) {
		mutex_unlock(&aw8624x->lock);
		return;
	}
	aw_info("effect_id=%d state=%d", aw8624x->effect_id, aw8624x->state);
	if (hrtimer_active(&aw8624x->timer))
		hrtimer_cancel(&aw8624x->timer);
	aw8624x_play_stop(aw8624x);
	aw8624x_upload_lra(aw8624x, aw8624x->rtp_cali_select);
	aw8624x_set_rtp_aei(aw8624x, false);
	/* clear irq state */
	aw8624x_i2c_read(aw8624x, AW8624X_REG_SYSINT, &val);
	aw_info("SYSINT=0x%02X", val);

	if (aw8624x->state) {
		pm_stay_awake(aw8624x->dev);
		aw8624x->wk_lock_flag = 1;
		aw8624x->rtp_file_num = aw8624x->effect_id - AW_RAM_MAX_INDEX;
		aw_info("aw8624x->rtp_file_num=%u", aw8624x->rtp_file_num);
		if (aw8624x->rtp_file_num < 0)
			aw8624x->rtp_file_num = 0;
		if (aw8624x->rtp_file_num >
			((sizeof(aw8624x_rtp_name) / AW8624X_NAME_MAX) - 1))
			aw8624x->rtp_file_num = (sizeof(aw8624x_rtp_name) /
							AW8624X_NAME_MAX) - 1;
		/* fw loaded */
		ret = request_firmware(&rtp_file,
				aw8624x_rtp_name[aw8624x->rtp_file_num],
				aw8624x->dev);
		if (ret < 0) {
			aw_err("failed to read %s",
			aw8624x_rtp_name[aw8624x->rtp_file_num]);
			if (aw8624x->wk_lock_flag == 1) {
				pm_relax(aw8624x->dev);
				aw8624x->wk_lock_flag = 0;
			}
			mutex_unlock(&aw8624x->lock);
			return;
		}
		aw8624x->rtp_init = false;
		vfree(aw8624x->aw_rtp);
		aw8624x->aw_rtp = NULL;
		if (aw8624x->f0 == 0 || aw8624x->dts_info.f0_pre == 0) {
			aw_err("f0 or f0_pre invalid!");
			release_firmware(rtp_file);
			if (aw8624x->wk_lock_flag == 1) {
				pm_relax(aw8624x->dev);
				aw8624x->wk_lock_flag = 0;
			}
			mutex_unlock(&aw8624x->lock);
			return;
		}
		rtp_cali_len = rtp_file->size;
		if (aw8624x->f0 < aw8624x->dts_info.f0_pre)
			rtp_cali_len = rtp_file->size * aw8624x->dts_info.f0_pre / aw8624x->f0;
		aw8624x->aw_rtp = vmalloc(rtp_cali_len + sizeof(int));
		if (!aw8624x->aw_rtp) {
			release_firmware(rtp_file);
			aw_err("error allocating memory, f0:%d f0_cali=%d", aw8624x->f0, aw8624x->f0_cali_data);
			if (aw8624x->wk_lock_flag == 1) {
				pm_relax(aw8624x->dev);
				aw8624x->wk_lock_flag = 0;
			}
			mutex_unlock(&aw8624x->lock);
			return;
		}
		aw_info("rtp file:[%s] size = %zu bytes, rtp_cali_len = %d",
			aw8624x_rtp_name[aw8624x->rtp_file_num],
			rtp_file->size, rtp_cali_len);
		ret = aw8624x_rtp_f0_cali(aw8624x, (uint8_t *)rtp_file->data,
							rtp_file->size, rtp_cali_len);
		if (ret) {
			aw_err("Play uncalibrated data! f0:%d f0_cali=%d", aw8624x->f0, aw8624x->f0_cali_data);
			aw8624x->aw_rtp->len = rtp_file->size;
			memcpy(aw8624x->aw_rtp->data, rtp_file->data, rtp_file->size);
		}
		release_firmware(rtp_file);
		aw8624x->rtp_init = true;

		val = aw8624x->rtp_drv_width;
		aw_info("update rtp_drv_width=0x%02X", aw8624x->rtp_drv_width);
		aw8624x_i2c_write(aw8624x, AW8624X_REG_CONTCFG3, val);
		/* gain */
		aw8624x_ram_vbat_comp(aw8624x, false);
		/* rtp mode config */
		aw8624x_play_mode(aw8624x, AW8624X_RTP_MODE);
		/* haptic go */
		aw8624x_play_go(aw8624x, true);
		mutex_unlock(&aw8624x->lock);
		usleep_range(2000, 2500);
		ret = aw8624x_wait_enter_rtp_mode(aw8624x, 200);
		if (ret < 0)
			return;

		/* rtp init */
		pm_qos_enable(aw8624x, true);
		aw8624x->rtp_cnt = 0;
		mutex_lock(&aw8624x->rtp_lock);
		aw8624x_rtp_play(aw8624x);
		mutex_unlock(&aw8624x->rtp_lock);
		if (aw8624x->play_mode == AW8624X_RTP_MODE && aw8624x->rtp_init)
			aw8624x_set_rtp_aei(aw8624x, true);
		aw_info("exit");
		pm_qos_enable(aw8624x, false);
	} else {
		if (aw8624x->wk_lock_flag == 1) {
			pm_relax(aw8624x->dev);
			aw8624x->wk_lock_flag = 0;
		}
		aw8624x->rtp_cnt = 0;
		aw8624x->rtp_init = false;
		mutex_unlock(&aw8624x->lock);
	}
}

static enum led_brightness aw8624x_brightness_get(struct led_classdev *cdev)
{
	struct aw8624x *aw8624x = container_of(cdev, struct aw8624x, vib_dev);

	return aw8624x->amplitude;
}

static void aw8624x_brightness_set(struct led_classdev *cdev, enum led_brightness level)
{
	uint8_t state = 0;
	struct aw8624x *aw8624x = container_of(cdev, struct aw8624x, vib_dev);

	aw_info("enter");
	if (!aw8624x->ram_init) {
		aw_err("ram init failed, not allow to play!");
		return;
	}
	aw8624x->amplitude = level;
	mutex_lock(&aw8624x->lock);
	/* add VDD POR verify */
	aw8624x_i2c_read(aw8624x, AW8624X_REG_SYSST2, &state);
	if (!(state & AW8624X_BIT_SYSST2_DVDD_PORN)) {
		aw_err("VDD is NOT POWER ON!!!");
		mutex_unlock(&aw8624x->lock);
		return;
	}

	aw8624x_play_stop(aw8624x);
	if (aw8624x->amplitude > 0) {
		aw8624x_upload_lra(aw8624x, AW8624X_F0_CALI_LRA);
		aw8624x_ram_vbat_comp(aw8624x, false);
		aw8624x_set_wav_seq(aw8624x, 0x01, 0x00);
		aw8624x_set_wav_loop(aw8624x, 0x00, 0x00);
		aw8624x_play_mode(aw8624x, AW8624X_RAM_MODE);
		aw8624x_play_go(aw8624x, true);
	}
	mutex_unlock(&aw8624x->lock);
}

static int aw8624x_vibrator_init(struct aw8624x *aw8624x)
{
#ifndef CONFIG_AW8624X_SAMSUNG_FEATURE  // Dont Register LED
	int ret = 0;
#endif
	aw_info("enter, loaded in leds_cdev framework!");
#ifdef KERNEL_OVER_5_10
	aw8624x->vib_dev.name = "aw_vibrator";
#else
	aw8624x->vib_dev.name = "vibrator";
#endif
	aw8624x->vib_dev.brightness_get = aw8624x_brightness_get;
	aw8624x->vib_dev.brightness_set = aw8624x_brightness_set;

#ifndef CONFIG_AW8624X_SAMSUNG_FEATURE  // Dont Register LED
	ret = devm_led_classdev_register(&aw8624x->i2c->dev, &aw8624x->vib_dev);
	if (ret < 0) {
		aw_err("fail to create led dev");
		return ret;
	}
	ret = sysfs_create_group(&aw8624x->vib_dev.dev->kobj, &aw8624x_vibrator_attribute_group);
	if (ret < 0) {
		aw_err("error creating sysfs attr files");
		return ret;
	}
#endif
	hrtimer_init(&aw8624x->timer, CLOCK_MONOTONIC, HRTIMER_MODE_REL);
	aw8624x->timer.function = aw8624x_vibrator_timer_func;
	INIT_WORK(&aw8624x->vibrator_work, aw8624x_vibrator_work_routine);
	INIT_WORK(&aw8624x->rtp_work, aw8624x_rtp_work_routine);
	mutex_init(&aw8624x->lock);
	mutex_init(&aw8624x->rtp_lock);
	atomic_set(&aw8624x->is_in_rtp_loop, 0);
	init_waitqueue_head(&aw8624x->stop_wait_q);

	return 0;
}

static void aw8624x_misc_para_init(struct aw8624x *aw8624x)
{
	uint8_t val = 0;

	aw_info("enter");
	/* Config rise/fall time of HDP/HDN */
	aw8624x_i2c_write_bits(aw8624x, AW8624X_REG_ANACFG8,
					AW8624X_BIT_ANACFG8_TRTF_CTRL_HDRV_MASK,
					AW8624X_BIT_ANACFG8_TRTF_CTRL_HDRV);
	/* GAIN_BYPASS config */
	aw8624x_i2c_write_bits(aw8624x, AW8624X_REG_SYSCTRL5,
				AW8624X_BIT_SYSCTRL5_GAIN_BYPASS_MASK,
				AW8624X_BIT_SYSCTRL5_GAIN_CHANGEABLE);
	/* CONT SMART_LOOP config */
	aw8624x_i2c_write_bits(aw8624x, AW8624X_REG_CONTCFG5,
				AW8624X_BIT_CONTCFG5_SMART_LOOP_MASK,
				aw8624x->dts_info.is_enabled_smart_loop << 4);
	/* RTP INT_BRK config */
	aw8624x_i2c_write_bits(aw8624x, AW8624X_REG_SYSCTRL1,
				AW8624X_BIT_SYSCTRL1_INT_BRK_MASK,
				aw8624x->dts_info.is_enabled_inter_brake << 6);
	/* AUTO BRK config */
	aw8624x_auto_brk_config(aw8624x, aw8624x->dts_info.is_enabled_auto_brk);

	/* cont_brk_time */
	if (aw8624x->dts_info.cont_brk_time) {
		aw8624x_i2c_write(aw8624x, AW8624X_REG_CONTCFG10,
				(uint8_t)aw8624x->dts_info.cont_brk_time);
	} else {
		aw_err("dts_info.cont_brk_time=0");
	}
	/* cont_brk_gain */
	if (aw8624x->dts_info.cont_brk_gain) {
		aw8624x_i2c_write_bits(aw8624x, AW8624X_REG_CONTCFG5,
					AW8624X_BIT_CONTCFG5_BRK_GAIN_MASK,
					aw8624x->dts_info.cont_brk_gain);
	} else {
		aw_err("dts_info.cont_brk_gain=0");
	}
	/* DRV_WIDTH */
	if (!aw8624x->dts_info.f0_pre)
		return;
	aw8624x->drv_width = AW8624X_DRV_WIDTH_FORMULA(aw8624x->dts_info.f0_pre,
					      aw8624x->dts_info.cont_track_margin,
					      aw8624x->dts_info.cont_brk_gain);
	if (aw8624x->drv_width < AW8624X_DRV_WIDTH_MIN)
		aw8624x->drv_width = AW8624X_DRV_WIDTH_MIN;
	if (aw8624x->drv_width > AW8624X_DRV_WIDTH_MAX)
		aw8624x->drv_width = AW8624X_DRV_WIDTH_MAX;
	val = aw8624x->drv_width;
	aw8624x->rtp_drv_width = aw8624x->drv_width;
	aw_info("cont_drv_width=0x%02X", val);
	aw8624x_i2c_write(aw8624x, AW8624X_REG_CONTCFG3, val);

/* init CONTCFG1 */
	aw8624x_i2c_write(aw8624x, AW8624X_REG_CONTCFG1, AW8624X_REG_CONTCFG1_DEFAULT);
}

#ifdef AW_UEFI_F0_CALIBRATION
/*
 * get uefi params data include f0 f0_cali_data and bemf_peak
 */
static int aw8624x_get_uefi_params_data(struct aw8624x *aw8624x)
{
	uint32_t *param_data = NULL;
	size_t buf_size = 0;
	int i = 0;

	param_data = qcom_smem_get(QCOM_SMEM_HOST_ANY, SMEM_AWINIC_PARAMS, &buf_size);
	if (IS_ERR(param_data) || !buf_size) {
		aw_err("smem_get_entry failed!!");
		return -ERANGE;
	}
	aw8624x->f0 = param_data[0];
	aw8624x->cont_f0 = param_data[1];
	aw8624x->f0_cali_data = param_data[2];
	aw8624x->sample_param[0] = param_data[3];
	aw8624x->sample_param[1] = param_data[4];
	aw8624x->sample_param[2] = param_data[5];
	aw8624x->sample_param[3] = param_data[6];
	aw8624x->sample_param[4] = param_data[7];
	aw8624x->sample_param[5] = aw8624x->f0;
	aw8624x->sample_param[6] = aw8624x->cont_f0;
	aw8624x->sample_param[8] = aw8624x->f0_cali_data;

	aw_info("f0 = 0x%04x, cont_f0 = 0x%04x", aw8624x->f0, aw8624x->cont_f0);
	aw_info("f0_cali_data = 0x%04x", aw8624x->f0_cali_data);
	for (i = 0; i < 7; i++)
		aw_info("uefi_sample_param[%d] = 0x%04x", i, aw8624x->sample_param[i]);

	return 0;
}
#endif

static void aw8624x_haptic_init(struct aw8624x *aw8624x)
{
	int ret = 0;
	int i = 0;
	uint8_t reg_val = 0;
	//uint32_t *uefi_params = NULL;

	aw_info("enter");
	mutex_lock(&aw8624x->lock);
	/* haptic init */
#ifdef CONFIG_AW8624X_SAMSUNG_FEATURE
	aw8624x->level = 10000;
#else
	aw8624x->level = 0x7FFF;
#endif
	aw8624x->input_flag = false;
	aw8624x->rtp_cali_select = AW8624X_OSC_CALI_LRA;
	aw8624x->activate_mode = AW8624X_RAM_LOOP_MODE;
	aw8624x->f0_pre = aw8624x->dts_info.f0_pre;
	aw8624x->f0 = aw8624x->dts_info.f0_pre;
	aw8624x->sample_param[5] = aw8624x->dts_info.f0_pre;
	aw8624x->fitting_coeff = AW8624X_FITTING_COEFF;
	aw8624x->rtp_num_max = sizeof(aw8624x_rtp_name) / AW8624X_NAME_MAX;
	ret = aw8624x_i2c_read(aw8624x, AW8624X_REG_WAVCFG1, &reg_val);
	aw8624x->index = reg_val & AW8624X_BIT_WAVCFG_SEQ;
	ret = aw8624x_i2c_read(aw8624x, AW8624X_REG_PLAYCFG2, &reg_val);
	aw8624x->gain = reg_val;
	aw_info("gain=0x%02X, index=0x%02X", aw8624x->gain, aw8624x->index);
	for (i = 0; i < AW8624X_SEQUENCER_SIZE; i++) {
		ret = aw8624x_i2c_read(aw8624x, AW8624X_REG_WAVCFG1 + i,
				&reg_val);
		aw8624x->seq[i] = reg_val;
	}
	aw8624x_play_mode(aw8624x, AW8624X_STANDBY_MODE);
	aw8624x_set_sample_rate(aw8624x, AW8624X_PWM_12K);
	/* misc value init */
	aw8624x_misc_para_init(aw8624x);
	/* set motor protect */
	aw8624x_protect_config(aw8624x, AW8624X_PWMCFG4_PRTIME_DEFAULT_VALUE,
			AW8624X_BIT_PWMCFG3_PRLVL_DEFAULT_VALUE);
	/* vbat compensation */
	aw8624x_vbat_mode_config(aw8624x, AW8624X_CONT_VBAT_HW_COMP_MODE);
	aw8624x->ram_vbat_comp = AW8624X_RAM_VBAT_COMP_DISABLE;
	/* f0 calibration */

	if (!aw8624x->dts_info.is_f0_tracking) {
#ifdef AW_UEFI_F0_CALIBRATION
		aw8624x_get_uefi_params_data(aw8624x);
		aw8624x_upload_lra(aw8624x, AW8624X_F0_CALI_LRA);
#else
#ifdef AW_LK_F0_CALIBRATION
#ifdef CONFIG_AW8624X_SAMSUNG_FEATURE
		aw_info("aw8624x_f0_cali_data=%u", aw8624x_f0_cali_data);
		aw_info("aw8624x_f0_data=%u, aw8624x_cont_f0_data=%u", aw8624x_f0_data, aw8624x_cont_f0_data);
		for (i = 0; i < 5; i++)
			aw_info("bemf_peak[%d]=%u", i, bemf_peak[i]);
#endif
		aw8624x->f0 = aw8624x_f0_data;
		aw8624x->cont_f0 = aw8624x_cont_f0_data;
		aw8624x->f0_cali_data = aw8624x_f0_cali_data;
		for (i = 0; i < 5; i++)
			aw8624x->sample_param[i] = bemf_peak[i];
		aw8624x->sample_param[5] = aw8624x->f0;
		aw8624x->sample_param[6] = aw8624x->cont_f0;
		aw8624x->sample_param[8] = aw8624x->f0_cali_data;
		for (i = 0; i < 7; i++)
			aw_info("lk_sample_param[%d]=0x%04x", i, aw8624x->sample_param[i]);
		aw8624x_upload_lra(aw8624x, AW8624X_F0_CALI_LRA);
#endif
#endif
	/* calculate rtp_drv_width */
		aw8624x->rtp_drv_width = AW8624X_DRV_WIDTH_FORMULA(aw8624x->f0,
						aw8624x->dts_info.cont_track_margin,
						aw8624x->dts_info.cont_brk_gain);
		if (aw8624x->rtp_drv_width < AW8624X_DRV_WIDTH_MIN)
			aw8624x->rtp_drv_width = AW8624X_DRV_WIDTH_MIN;
		if (aw8624x->rtp_drv_width > AW8624X_DRV_WIDTH_MAX)
			aw8624x->rtp_drv_width = AW8624X_DRV_WIDTH_MAX;

		aw_info("rtp_drv_width=0x%02X", aw8624x->rtp_drv_width);
		/* get lra resistance */
		aw8624x_get_lra_resistance(aw8624x);
		aw8624x->sample_param[7] = aw8624x->lra;
	}
	mutex_unlock(&aw8624x->lock);
	aw_info("exit");
}

/*
 * ram data update workqueue callback function
 */
static void aw8624x_ram_work_routine(struct work_struct *work)
{
	struct aw8624x *aw8624x = container_of(work, struct aw8624x, ram_work.work);

	aw_info("enter");
	aw8624x_ram_update(aw8624x);
}

static void aw8624x_ram_work_init(struct aw8624x *aw8624x)
{
	int ram_timer_val = 1000;

	aw_info("enter");
	INIT_DELAYED_WORK(&aw8624x->ram_work, aw8624x_ram_work_routine);
	schedule_delayed_work(&aw8624x->ram_work, msecs_to_jiffies(ram_timer_val));
}

#ifdef CONFIG_AW8624X_SAMSUNG_FEATURE
static int aw8624x_set_use_sep_index(struct input_dev *dev, bool use_sep_index)
{
	struct aw8624x *aw8624x = aw8624x_input_get_drvdata(dev);

	aw_info("%s +\n", __func__);

	mutex_lock(&aw8624x->lock);
	aw8624x->use_sep_index = use_sep_index;
	mutex_unlock(&aw8624x->lock);

	aw_info("%s -\n", __func__);

	return 0;
}

u32 aw8624x_haptics_get_f0_stored(struct input_dev *dev)
{
	struct aw8624x *aw8624x = aw8624x_input_get_drvdata(dev);

	aw_info("enter, get f0 stored\n");

	return aw8624x->f0;
}

int aw8624x_haptics_get_f0_offset(struct input_dev *dev)
{
	struct aw8624x *aw8624x = aw8624x_input_get_drvdata(dev);

	if (aw8624x->dts_info.is_f0_tracking) {
		aw_info("%s: f0_offset : 0x%08X", __func__, aw8624x->dts_info.f0_offset);
		return aw8624x->dts_info.f0_offset;
	}

	aw_info("enter, get f0 offset\n");
	return aw8624x->f0_cali_data;
}

int aw8624x_haptic_get_lra_resistance_stored(struct input_dev *dev)
{
	struct aw8624x *aw8624x = aw8624x_input_get_drvdata(dev);

	aw_info("enter, get lra_resistance stored\n");
	aw8624x_get_lra_resistance(aw8624x);

	return aw8624x->lra;
}

static int samsung_set_trigger_cal(struct input_dev *dev, u32 val)
{
	struct aw8624x *aw8624x = aw8624x_input_get_drvdata(dev);

	if (!aw8624x->dts_info.is_f0_tracking)
		return 0;
	aw8624x_f0_factory_store(aw8624x);
	aw8624x->sec_vib_ddata.trigger_calibration = 0;
	return 0;
}

u32 samsung_get_f0_measured(struct input_dev *dev)
{
	struct aw8624x *aw8624x = aw8624x_input_get_drvdata(dev);

	if (aw8624x->dts_info.is_f0_tracking)
		return aw8624x_f0_factory_get(aw8624x);

	aw_info("enter, get f0 measured\n");
	mutex_lock(&aw8624x->lock);
	aw8624x_upload_lra(aw8624x, AW8624X_WRITE_ZERO);
	aw8624x_f0_cali(aw8624x);
	mutex_unlock(&aw8624x->lock);

	return aw8624x->f0;
}

u32 samsung_set_f0_stored(struct input_dev *dev, u32 val)
{
	struct aw8624x *aw8624x = aw8624x_input_get_drvdata(dev);

	if (aw8624x->dts_info.is_f0_tracking)
		return aw8624x_nv_f0_store(aw8624x, val);

	aw_info("enter, set f0 stored\n");
	mutex_lock(&aw8624x->lock);
	aw8624x->f0 = val;
	aw8624x_upload_lra(aw8624x, AW8624X_F0_CALI_LRA);
	mutex_unlock(&aw8624x->lock);

	return 0;
}

int aw8624x_haptics_set_f0_offset(struct input_dev *dev, u32 val)
{
	struct aw8624x *aw8624x = aw8624x_input_get_drvdata(dev);

	if (aw8624x->dts_info.is_f0_tracking)
		return 0;

	aw_info("enter, set f0 offset\n");
	mutex_lock(&aw8624x->lock);
	aw8624x->f0_cali_data = (uint8_t)val;
	aw8624x_upload_lra(aw8624x, AW8624X_F0_CALI_LRA);
	mutex_unlock(&aw8624x->lock);

	return 0;
}

static const struct sec_vib_inputff_ops aw8624x_vib_ops = {
	.upload = aw8624x_input_upload_effect,
	.playback = aw8624x_input_playback,
	.set_gain = aw8624x_input_set_gain,
	.erase = aw8624x_input_erase,
	.set_use_sep_index = aw8624x_set_use_sep_index,
	.set_trigger_cal = samsung_set_trigger_cal,
	.get_f0_measured = samsung_get_f0_measured,
	.set_f0_offset = aw8624x_haptics_set_f0_offset,
	.get_f0_offset = aw8624x_haptics_get_f0_offset,
	.get_f0_stored = aw8624x_haptics_get_f0_stored,
	.set_f0_stored = samsung_set_f0_stored,
	.get_lra_resistance = aw8624x_haptic_get_lra_resistance_stored,
};

static struct attribute_group *aw8624x_dev_attr_groups[] = {
	&aw8624x_vibrator_attribute_group,
	NULL
};

static int samsung_aw8624x_input_init(struct aw8624x *aw8624x)
{
	aw8624x->sec_vib_ddata.dev = aw8624x->dev;
	aw8624x->sec_vib_ddata.vib_ops = &aw8624x_vib_ops;
	aw8624x->sec_vib_ddata.vendor_dev_attr_groups = (struct attribute_group **)aw8624x_dev_attr_groups;
	aw8624x->sec_vib_ddata.private_data = (void *)aw8624x;
	aw8624x->sec_vib_ddata.ff_val = 0;
	aw8624x->sec_vib_ddata.is_f0_tracking = 1;
	aw8624x->sec_vib_ddata.use_common_inputff = true;
	sec_vib_inputff_setbit(&aw8624x->sec_vib_ddata, FF_CONSTANT);
	sec_vib_inputff_setbit(&aw8624x->sec_vib_ddata, FF_PERIODIC);
	sec_vib_inputff_setbit(&aw8624x->sec_vib_ddata, FF_CUSTOM);
	sec_vib_inputff_setbit(&aw8624x->sec_vib_ddata, FF_GAIN);

	return sec_vib_inputff_register(&aw8624x->sec_vib_ddata);
}
#else
static int aw8624x_input_init(struct aw8624x *aw8624x)
{
	int rc;
	struct input_dev *input_dev;
	struct ff_device *ff;

	input_dev = devm_input_allocate_device(aw8624x->dev);
	if (!input_dev)
		return -ENOMEM;

	/* aw8624x input config */
	input_dev->name = "aw8624x_haptic";
	input_set_drvdata(input_dev, aw8624x);
	aw8624x->input_dev = input_dev;
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
	ff->upload = aw8624x_input_upload_effect;
	ff->playback = aw8624x_input_playback;
	ff->erase = aw8624x_input_erase;
	ff->set_gain = aw8624x_input_set_gain;
	rc = input_register_device(input_dev);
	if (rc < 0) {
		aw_err("register input device failed, rc=%d", rc);
		input_ff_destroy(input_dev);
	}

	return rc;
}
#endif

static int aw8624x_i2c_probe(struct i2c_client *i2c, const struct i2c_device_id *id)
{
	int ret = -1;
	struct aw8624x *aw8624x;
	struct device_node *np = i2c->dev.of_node;

	aw_info("enter");
	if (!i2c_check_functionality(i2c->adapter, I2C_FUNC_I2C)) {
		aw_err("check_functionality failed");
		return -EIO;
	}
	aw8624x = devm_kzalloc(&i2c->dev, sizeof(struct aw8624x), GFP_KERNEL);
	if (aw8624x == NULL)
		return -ENOMEM;

	aw8624x->dev = &i2c->dev;
	aw8624x->i2c = i2c;
	i2c_set_clientdata(i2c, aw8624x);
	/* aw8624x rst & int */
	if (np) {
		ret = aw8624x_parse_dt(&i2c->dev, aw8624x, np);
		if (ret) {
			aw_err("failed to parse device tree node");
			goto err_parse_dt;
		}
	} else {
		aw8624x->irq_gpio = -1;
	}
	if (gpio_is_valid(aw8624x->irq_gpio)) {
		ret = devm_gpio_request_one(&i2c->dev, aw8624x->irq_gpio,
					GPIOF_DIR_IN, "aw8624x_int");
		if (ret) {
			aw_err("int request failed");
			goto err_irq_gpio_request;
		}
	}
	/* aw8624x chip id */
	ret = aw8624x_parse_chipid(aw8624x);
	if (ret < 0) {
		aw_err("aw8624x parse chipid failed.");
		goto err_id;
	}
	/* chip qualify */
	ret = aw8624x_check_qualify(aw8624x);
	if (ret < 0) {
		aw_err("unqualified chip!");
		goto err_check_qualify;
	}
	aw8624x_sw_reset(aw8624x);
	ret = aw8624x_offset_cali(aw8624x);
	if (ret < 0)
		aw8624x_sw_reset(aw8624x);
	/* aw8624x irq */
	ret = aw8624x_irq_config(&i2c->dev, aw8624x);
	if (ret != 0) {
		aw_err("irq_config failed ret=%d", ret);
		goto err_irq_config;
	}
	aw8624x_vibrator_init(aw8624x);
	aw8624x_haptic_init(aw8624x);
	aw8624x_ram_work_init(aw8624x);
	aw8624x->work_queue = create_singlethread_workqueue("aw8624x_vibrator_work_queue");
	if (!aw8624x->work_queue) {
		aw_err("Error creating aw8624x_vibrator_work_queue");
		goto err_create_workqueue;
	}
	INIT_WORK(&aw8624x->set_gain_work,
		aw8624x_input_set_gain_work_routine);
	INIT_WORK(&aw8624x->stop_work,
		aw8624x_stop_work_routine);

#ifdef CONFIG_AW8624X_SAMSUNG_FEATURE
	ret = samsung_aw8624x_input_init(aw8624x);
#else
	ret = aw8624x_input_init(aw8624x);
#endif
	if (ret < 0)
		goto destroy_ff;
	dev_set_drvdata(&i2c->dev, aw8624x);
	standby_detected = false;
	aw_info("probe completed successfully!");
	return 0;

destroy_ff:
err_create_workqueue:
err_irq_config:
err_check_qualify:
err_id:
err_irq_gpio_request:
err_parse_dt:
	devm_kfree(&i2c->dev, aw8624x);
	aw8624x = NULL;
	return ret;
}

#ifdef KERNEL_OVER_6_1
static void aw8624x_i2c_remove(struct i2c_client *i2c)
{
	struct aw8624x *aw8624x = i2c_get_clientdata(i2c);

	aw_info("enter");
#ifdef CONFIG_AW8624X_SAMSUNG_FEATURE
	sec_vib_inputff_unregister(&aw8624x->sec_vib_ddata);
#else
	sysfs_remove_group(&aw8624x->vib_dev.dev->kobj, &aw8624x_vibrator_attribute_group);
	devm_led_classdev_unregister(&aw8624x->i2c->dev, &aw8624x->vib_dev);
	input_unregister_device(aw8624x->input_dev);
	input_ff_destroy(aw8624x->input_dev);
#endif
	cancel_work_sync(&aw8624x->set_gain_work);
	cancel_work_sync(&aw8624x->stop_work);
	cancel_delayed_work_sync(&aw8624x->ram_work);
	cancel_work_sync(&aw8624x->rtp_work);
	cancel_work_sync(&aw8624x->vibrator_work);
	hrtimer_cancel(&aw8624x->timer);
	mutex_destroy(&aw8624x->lock);
	mutex_destroy(&aw8624x->rtp_lock);
	destroy_workqueue(aw8624x->work_queue);
	devm_free_irq(&i2c->dev, gpio_to_irq(aw8624x->irq_gpio), aw8624x);
}
#else
static int aw8624x_i2c_remove(struct i2c_client *i2c)
{
	struct aw8624x *aw8624x = i2c_get_clientdata(i2c);

	aw_info("enter");
#ifdef CONFIG_AW8624X_SAMSUNG_FEATURE
	sec_vib_inputff_unregister(&aw8624x->sec_vib_ddata);
#else
	sysfs_remove_group(&aw8624x->vib_dev.dev->kobj, &aw8624x_vibrator_attribute_group);
	devm_led_classdev_unregister(&aw8624x->i2c->dev, &aw8624x->vib_dev);
	input_unregister_device(aw8624x->input_dev);
	input_ff_destroy(aw8624x->input_dev);
#endif
	cancel_work_sync(&aw8624x->set_gain_work);
	cancel_work_sync(&aw8624x->stop_work);
	cancel_delayed_work_sync(&aw8624x->ram_work);
	cancel_work_sync(&aw8624x->rtp_work);
	cancel_work_sync(&aw8624x->vibrator_work);
	hrtimer_cancel(&aw8624x->timer);
	mutex_destroy(&aw8624x->lock);
	mutex_destroy(&aw8624x->rtp_lock);
	destroy_workqueue(aw8624x->work_queue);
	devm_free_irq(&i2c->dev, gpio_to_irq(aw8624x->irq_gpio), aw8624x);

	return 0;
}
#endif

static const struct i2c_device_id aw8624x_i2c_id[] = {
	{AW8624X_I2C_NAME, 0},
	{}
};

MODULE_DEVICE_TABLE(i2c, aw8624x_i2c_id);

static const struct of_device_id aw8624x_dt_match[] = {
	{.compatible = "awinic,aw8624x_haptic"},
	{},
};

static struct i2c_driver aw8624x_i2c_driver = {
	.driver = {
		.name = AW8624X_I2C_NAME,
		.owner = THIS_MODULE,
		.of_match_table = of_match_ptr(aw8624x_dt_match),
	},
	.probe = aw8624x_i2c_probe,
	.remove = aw8624x_i2c_remove,
	.id_table = aw8624x_i2c_id,
};

static int __init aw8624x_i2c_init(void)
{
	int ret = 0;

	pr_info("%s aw8624x driver version %s\n", __func__, AW8624X_DRIVER_VERSION);
	ret = i2c_add_driver(&aw8624x_i2c_driver);
	if (ret) {
		pr_err("%s fail to add aw8624x device into i2c\n", __func__);
		return ret;
	}
	pr_info("%s success", __func__);
	return 0;
}

late_initcall(aw8624x_i2c_init);

static void __exit aw8624x_i2c_exit(void)
{
	i2c_del_driver(&aw8624x_i2c_driver);
}

module_exit(aw8624x_i2c_exit);

MODULE_DESCRIPTION("AW8624X Input Haptic Driver");
MODULE_LICENSE("GPL v2");
