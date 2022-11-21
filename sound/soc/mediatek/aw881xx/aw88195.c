/*
 * aw88195.c   aw88195 codec module
 *
 * Copyright (c) 2018 AWINIC Technology CO., LTD
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
#include <linux/timer.h>
#include <linux/workqueue.h>
#include <linux/hrtimer.h>
#include <linux/syscalls.h>
#include <sound/tlv.h>
#include "aw881xx.h"
#include "aw88195.h"
#include "aw88195_reg.h"


static uint16_t aw88195_base_addr[AW88195_BASE_ADDR_MAX] = {
	AW88195_SPK_REG_ADDR,
	AW88195_SPK_DSP_FW_ADDR,
	AW88195_SPK_DSP_CFG_ADDR,
	AW88195_RCV_REG_ADDR,
	AW88195_RCV_DSP_FW_ADDR,
	AW88195_RCV_DSP_CFG_ADDR,
};

const unsigned char aw88195_reg_access[AW88195_REG_MAX] = {
	[AW88195_REG_ID] = AW88195_REG_RD_ACCESS,
	[AW88195_REG_SYSST] = AW88195_REG_RD_ACCESS,
	[AW88195_REG_SYSINT] = AW88195_REG_RD_ACCESS,
	[AW88195_REG_SYSINTM] = AW88195_REG_RD_ACCESS | AW88195_REG_WR_ACCESS,
	[AW88195_REG_SYSCTRL] = AW88195_REG_RD_ACCESS | AW88195_REG_WR_ACCESS,
	[AW88195_REG_I2SCTRL] = AW88195_REG_RD_ACCESS | AW88195_REG_WR_ACCESS,
	[AW88195_REG_I2SCFG1] = AW88195_REG_RD_ACCESS | AW88195_REG_WR_ACCESS,
	[AW88195_REG_PWMCTRL] = AW88195_REG_RD_ACCESS | AW88195_REG_WR_ACCESS,
	[AW88195_REG_HAGCCFG1] = AW88195_REG_RD_ACCESS | AW88195_REG_WR_ACCESS,
	[AW88195_REG_HAGCCFG2] = AW88195_REG_RD_ACCESS | AW88195_REG_WR_ACCESS,
	[AW88195_REG_HAGCCFG3] = AW88195_REG_RD_ACCESS | AW88195_REG_WR_ACCESS,
	[AW88195_REG_HAGCCFG4] = AW88195_REG_RD_ACCESS | AW88195_REG_WR_ACCESS,
	[AW88195_REG_HAGCCFG5] = AW88195_REG_RD_ACCESS | AW88195_REG_WR_ACCESS,
	[AW88195_REG_HAGCCFG6] = AW88195_REG_RD_ACCESS | AW88195_REG_WR_ACCESS,
	[AW88195_REG_HAGCCFG7] = AW88195_REG_RD_ACCESS | AW88195_REG_WR_ACCESS,
	[AW88195_REG_HAGCCFG8] = AW88195_REG_RD_ACCESS | AW88195_REG_WR_ACCESS,
	[AW88195_REG_SYSCTRL2] = AW88195_REG_RD_ACCESS | AW88195_REG_WR_ACCESS,
	[AW88195_REG_PRODID] = AW88195_REG_RD_ACCESS,
	[AW88195_REG_DBGCTRL] = AW88195_REG_RD_ACCESS | AW88195_REG_WR_ACCESS,
	[AW88195_REG_I2SCFG2] = AW88195_REG_RD_ACCESS | AW88195_REG_WR_ACCESS,
	[AW88195_REG_I2SSTAT] = AW88195_REG_RD_ACCESS,
	[AW88195_REG_I2SCAPCNT] = AW88195_REG_RD_ACCESS,
	[AW88195_REG_TM] = AW88195_REG_RD_ACCESS | AW88195_REG_WR_ACCESS,
	[AW88195_REG_CRCIN] = AW88195_REG_RD_ACCESS | AW88195_REG_WR_ACCESS,
	[AW88195_REG_CRCOUT] = AW88195_REG_RD_ACCESS,
	[AW88195_REG_DSPMADD] = AW88195_REG_RD_ACCESS | AW88195_REG_WR_ACCESS,
	[AW88195_REG_DSPMDAT] = AW88195_REG_RD_ACCESS | AW88195_REG_WR_ACCESS,
	[AW88195_REG_WDT] = AW88195_REG_RD_ACCESS,
	[AW88195_REG_ACR1] = AW88195_REG_RD_ACCESS | AW88195_REG_WR_ACCESS,
	[AW88195_REG_ACR2] = AW88195_REG_RD_ACCESS | AW88195_REG_WR_ACCESS,
	[AW88195_REG_ASR1] = AW88195_REG_RD_ACCESS,
	[AW88195_REG_ASR2] = AW88195_REG_RD_ACCESS,
	[AW88195_REG_DSPCFG] = AW88195_REG_RD_ACCESS | AW88195_REG_WR_ACCESS,
	[AW88195_REG_ASR3] = AW88195_REG_RD_ACCESS,
	[AW88195_REG_ASR4] = AW88195_REG_RD_ACCESS,
	[AW88195_REG_BSTCTRL1] = AW88195_REG_RD_ACCESS | AW88195_REG_WR_ACCESS,
	[AW88195_REG_BSTCTRL2] = AW88195_REG_RD_ACCESS | AW88195_REG_WR_ACCESS,
	[AW88195_REG_BSTCTRL3] = AW88195_REG_RD_ACCESS | AW88195_REG_WR_ACCESS,
	[AW88195_REG_BSTDBG1] = AW88195_REG_RD_ACCESS | AW88195_REG_WR_ACCESS,
	[AW88195_REG_BSTDBG2] = AW88195_REG_RD_ACCESS | AW88195_REG_WR_ACCESS,
	[AW88195_REG_PLLCTRL1] = AW88195_REG_RD_ACCESS | AW88195_REG_WR_ACCESS,
	[AW88195_REG_PLLCTRL2] = AW88195_REG_RD_ACCESS | AW88195_REG_WR_ACCESS,
	[AW88195_REG_AMPDBG1] = AW88195_REG_RD_ACCESS | AW88195_REG_WR_ACCESS,
	[AW88195_REG_AMPDBG2] = AW88195_REG_RD_ACCESS | AW88195_REG_WR_ACCESS,
	[AW88195_REG_CDACTRL1] = AW88195_REG_RD_ACCESS | AW88195_REG_WR_ACCESS,
	[AW88195_REG_CDACTRL2] = AW88195_REG_RD_ACCESS | AW88195_REG_WR_ACCESS,
	[AW88195_REG_ISECTRL1] = AW88195_REG_RD_ACCESS | AW88195_REG_WR_ACCESS,
	[AW88195_REG_SADCCTRL] = AW88195_REG_RD_ACCESS | AW88195_REG_WR_ACCESS,
	[AW88195_REG_VBAT] = AW88195_REG_RD_ACCESS,
	[AW88195_REG_TEMP] = REG_RD_ACCESS,
	[AW88195_REG_TEST] = REG_RD_ACCESS | REG_WR_ACCESS,
	[AW88195_REG_TEST2] = REG_RD_ACCESS | REG_WR_ACCESS,
	[AW88195_REG_EFCTR1] = REG_RD_ACCESS | REG_WR_ACCESS,
	[AW88195_REG_EFCTR2] = REG_RD_ACCESS | REG_WR_ACCESS,
	[AW88195_REG_EFWH] = REG_RD_ACCESS | REG_WR_ACCESS,
	[AW88195_REG_EFWM] = REG_RD_ACCESS | REG_WR_ACCESS,
	[AW88195_REG_EFWL] = REG_RD_ACCESS | REG_WR_ACCESS,
	[AW88195_REG_EFRH] = REG_RD_ACCESS,
	[AW88195_REG_EFRM] = REG_RD_ACCESS,
	[AW88195_REG_EFRL] = REG_RD_ACCESS,
	[AW88195_REG_TESTDET] = REG_RD_ACCESS,
};


/******************************************************
 *
 * aw88195 i2c write/read
 *
 ******************************************************/
static int aw88195_i2c_writes(struct aw881xx *aw88195,
	uint8_t reg_addr, uint8_t *buf, uint16_t len)
{
	int ret = -1;

	ret = aw881xx_i2c_writes(aw88195, reg_addr, buf, len);
	if (ret < 0)
		pr_err("%s: aw881x_i2c_writes fail, ret=%d",
			__func__, ret);

	return ret;
}
/*
static int aw88195_i2c_reads(struct aw881xx *aw88195,
	uint8_t reg_addr, uint8_t *buf, uint16_t len)
{
	int ret = -1;

	ret = aw881xx_i2c_reads(aw88195, reg_addr, buf, len);
	if (ret < 0)
		pr_err("%s: aw881xx_i2c_reads fail, ret=%d",
			__func__, ret);

	return ret;
}
*/
static int aw88195_i2c_write(struct aw881xx *aw88195,
	uint8_t reg_addr, uint16_t reg_data)
{
	int ret = -1;

	ret = aw881xx_i2c_write(aw88195, reg_addr, reg_data);
	if (ret < 0)
		pr_err("%s: aw881xx_i2c_write fail, ret=%d",
			__func__, ret);

	return ret;

}

static int aw88195_i2c_read(struct aw881xx *aw88195,
	uint8_t reg_addr, uint16_t *reg_data)
{
	int ret = -1;

	ret = aw881xx_i2c_read(aw88195, reg_addr, reg_data);
	if (ret < 0)
		pr_err("%s: aw881xx_i2c_read fail, ret=%d",
			__func__, ret);

	return ret;
}

static int aw88195_i2c_write_bits(struct aw881xx *aw88195,
	uint8_t reg_addr, uint16_t mask, uint16_t reg_data)
{
	int ret = -1;

	ret = aw881xx_i2c_write_bits(aw88195, reg_addr, mask, reg_data);
	if (ret < 0)
		pr_err("%s: aw881xx_i2c_write_bits fail, ret=%d",
			__func__, ret);

	return ret;
}

static int aw88195_dsp_write(struct aw881xx *aw88195,
	uint16_t dsp_addr, uint16_t dsp_data)
{
	int ret = -1;

	ret = aw881xx_dsp_write(aw88195, dsp_addr, dsp_data);
	if (ret < 0)
		pr_err("%s: aw881xx_dsp_write fail, ret=%d",
			__func__, ret);

	return ret;
}

static int aw88195_dsp_read(struct aw881xx *aw88195,
	uint16_t dsp_addr, uint16_t *dsp_data)
{
	int ret = -1;

	ret = aw881xx_dsp_read(aw88195, dsp_addr, dsp_data);
	if (ret < 0)
		pr_err("%s: aw881xx_dsp_read fail, ret=%d",
			__func__, ret);

	return ret;
}

/******************************************************
 *
 * aw88195 cali store
 *
 ******************************************************/
void aw88195_set_cali_re(void *aw881xx)
{
	struct aw881xx *aw88195 = (struct aw881xx *)aw881xx;

	/* set cali re to aw88195 */
	aw88195_dsp_write(aw88195, AW88195_DSP_REG_CFG_ADPZ_RE,
			aw88195->cali_re);

}

/******************************************************
 *
 * aw88195 control
 *
 ******************************************************/
static void aw88195_run_mute(struct aw881xx *aw88195, bool mute)
{
	pr_debug("%s: enter\n", __func__);

	if (mute) {
		aw88195_i2c_write_bits(aw88195, AW88195_REG_PWMCTRL,
			AW88195_BIT_PWMCTRL_HMUTE_MASK,
			AW88195_BIT_PWMCTRL_HMUTE_ENABLE);
	} else {
		aw88195_i2c_write_bits(aw88195, AW88195_REG_PWMCTRL,
			AW88195_BIT_PWMCTRL_HMUTE_MASK,
			AW88195_BIT_PWMCTRL_HMUTE_DISABLE);
	}
}

void aw88195_run_pwd(void *aw881xx, bool pwd)
{
	struct aw881xx *aw88195 = (struct aw881xx *)aw881xx;

	pr_debug("%s: enter\n", __func__);

	if (pwd) {
		aw88195_i2c_write_bits(aw88195, AW88195_REG_SYSCTRL,
			AW88195_BIT_SYSCTRL_PW_MASK,
			AW88195_BIT_SYSCTRL_PW_PDN);
	} else {
		aw88195_i2c_write_bits(aw88195, AW88195_REG_SYSCTRL,
			AW88195_BIT_SYSCTRL_PW_MASK,
			AW88195_BIT_SYSCTRL_PW_ACTIVE);
	}
}

static void aw88195_dsp_enable(struct aw881xx *aw88195, bool dsp)
{
	pr_debug("%s: enter\n", __func__);

	if (dsp) {
		aw88195_i2c_write_bits(aw88195, AW88195_REG_SYSCTRL,
			AW88195_BIT_SYSCTRL_DSP_MASK,
			AW88195_BIT_SYSCTRL_DSP_WORK);
	} else {
		aw88195_i2c_write_bits(aw88195, AW88195_REG_SYSCTRL,
			AW88195_BIT_SYSCTRL_DSP_MASK,
			AW88195_BIT_SYSCTRL_DSP_BYPASS);
	}
}

static void aw88195_memclk_select(struct aw881xx *aw88195, unsigned char flag)
{
	pr_debug("%s: enter\n", __func__);

	if (flag == AW88195_MEMCLK_PLL) {
		aw88195_i2c_write_bits(aw88195, AW88195_REG_SYSCTRL2,
			AW88195_BIT_SYSCTRL2_MEMCLK_MASK,
			AW88195_BIT_SYSCTRL2_MEMCLK_PLL);
	} else if (flag == AW88195_MEMCLK_OSC) {
		aw88195_i2c_write_bits(aw88195, AW88195_REG_SYSCTRL2,
			AW88195_BIT_SYSCTRL2_MEMCLK_MASK,
			AW88195_BIT_SYSCTRL2_MEMCLK_OSC);
	} else {
		pr_err("%s: unknown memclk config, flag=0x%x\n",
			__func__, flag);
	}
}

static int aw88195_sysst_check(struct aw881xx *aw88195)
{
	int ret = -1;
	unsigned char i;
	uint16_t reg_val = 0;

	for (i = 0; i < AW88195_SYSST_CHECK_MAX; i++) {
		aw88195_i2c_read(aw88195, AW88195_REG_SYSST, &reg_val);
		if ((reg_val & (~AW88195_BIT_SYSST_CHECK_MASK)) ==
			AW88195_BIT_SYSST_CHECK) {
			ret = 0;
			return ret;
		} else {
			pr_debug("%s: check fail, cnt=%d, reg_val=0x%04x\n",
				__func__, i, reg_val);
			msleep(2);
		}
	}
	pr_info("%s: check fail\n", __func__);

	return ret;
}

static int aw88195_get_sysint(struct aw881xx *aw88195, uint16_t *sysint)
{
	int ret = -1;
	uint16_t reg_val = 0;

	ret = aw88195_i2c_read(aw88195, AW88195_REG_SYSINT, &reg_val);
	if (ret < 0)
		pr_info("%s: read sysint fail, ret=%d\n", __func__, ret);
	else
		*sysint = reg_val;

	return ret;
}

int aw88195_get_iis_status(void *aw881xx)
{
	struct aw881xx *aw88195 = (struct aw881xx *)aw881xx;
	int ret = -1;
	uint16_t reg_val = 0;

	pr_debug("%s: enter\n", __func__);

	aw88195_i2c_read(aw88195, AW88195_REG_SYSST, &reg_val);
	if (reg_val & AW88195_BIT_SYSST_PLLS)
		ret = 0;

	return ret;
}

static int aw88195_get_dsp_status(struct aw881xx *aw88195)
{
	int ret = -1;
	uint16_t reg_val = 0;

	pr_debug("%s: enter\n", __func__);

	aw88195_i2c_read(aw88195, AW88195_REG_WDT, &reg_val);
	if (reg_val)
		ret = 0;

	return ret;
}

int aw88195_get_dsp_config(void *aw881xx)
{
	struct aw881xx *aw88195 = (struct aw881xx *)aw881xx;
	int ret = -1;
	uint16_t reg_val = 0;

	pr_debug("%s: enter\n", __func__);

	aw88195_i2c_read(aw88195, AW88195_REG_SYSCTRL, &reg_val);
	if (reg_val & AW88195_BIT_SYSCTRL_DSP_BYPASS)
		aw88195->dsp_cfg = AW88195_DSP_BYPASS;
	else
		aw88195->dsp_cfg = AW88195_DSP_WORK;

	return ret;
}

int aw88195_get_hmute(void *aw881xx)
{
	struct aw881xx *aw88195 = (struct aw881xx *)aw881xx;
	int ret = -1;
	uint16_t reg_val = 0;

	pr_debug("%s: enter\n", __func__);

	aw88195_i2c_read(aw88195, AW88195_REG_PWMCTRL, &reg_val);
	if (reg_val & AW88195_BIT_PWMCTRL_HMUTE_ENABLE)
		ret = 1;
	else
		ret = 0;

	return ret;
}

static int aw88195_get_icalk(struct aw881xx *aw88195, int16_t *icalk)
{
	int ret = -1;
	uint16_t reg_val = 0;
	uint16_t reg_icalk = 0;

	ret = aw88195_i2c_read(aw88195, AW88195_REG_EFRM, &reg_val);
	reg_icalk = (uint16_t) reg_val & AW88195_EF_ISENSE_GAINERR_SLP_MASK;

	if ((reg_icalk & AW88195_EF_ISENSE_GAINERR_SLP_PROCT) !=
		AW88195_EF_ISENSE_GAINERR_SLP_PROCT) {
		reg_icalk &= AW88195_EF_ISENSE_GAINERR_SLP_PROCT_MASK;
	}

	if (reg_icalk & AW88195_EF_ISENSE_GAINERR_SLP_SIGN_MASK)
		reg_icalk = reg_icalk | AW88195_EF_ISENSE_GAINERR_SLP_NEG;

	*icalk = (int16_t) reg_icalk;

	return ret;
}

static int aw88195_get_vcalk(struct aw881xx *aw88195, int16_t *vcalk)
{
	int ret = -1;
	uint16_t reg_val = 0;
	uint16_t reg_vcalk = 0;

	ret = aw88195_i2c_read(aw88195, AW88195_REG_EFRL, &reg_val);
	reg_val = reg_val >> AW88195_EF_VSENSE_GAIN_SHIFT;

	if ((reg_val & AW88195_EF_VSENSE_GAIN_PROCT) !=
		AW88195_EF_VSENSE_GAIN_PROCT) {
		reg_val &= AW88195_EF_VSENSE_GAIN_PROCT_MASK;
	}

	reg_vcalk = (uint16_t) reg_val & AW88195_EF_VSENSE_GAIN_MASK;

	if (reg_vcalk & AW88195_EF_VSENSE_GAIN_SIGN_MASK)
		reg_vcalk = reg_vcalk | AW88195_EF_VSENSE_GAIN_NEG;

	*vcalk = (int16_t) reg_vcalk;

	return ret;
}

static int aw88195_dsp_set_vcalb(struct aw881xx *aw88195)
{
	int ret = -1;
	uint16_t reg_val = 0;
	int vcalb;
	int icalk;
	int vcalk;
	int16_t icalk_val = 0;
	int16_t vcalk_val = 0;

	ret = aw88195_get_icalk(aw88195, &icalk_val);
	ret = aw88195_get_vcalk(aw88195, &vcalk_val);

	icalk = AW88195_CABL_BASE_VALUE + AW88195_ICABLK_FACTOR * icalk_val;
	vcalk = AW88195_CABL_BASE_VALUE + AW88195_VCABLK_FACTOR * vcalk_val;

	vcalb = (AW88195_VCAL_FACTOR * AW88195_VSCAL_FACTOR /
		AW88195_ISCAL_FACTOR) * vcalk / icalk;

	reg_val = (uint16_t)vcalb;
	pr_debug("%s: icalk=%d, vcalk=%d, vcalb=%d, reg_val=%d\n",
		__func__, icalk, vcalk, vcalb, reg_val);

	ret = aw88195_dsp_write(aw88195, AW88195_DSP_REG_VCALB, reg_val);

	return ret;
}

static int aw88195_start(struct aw881xx *aw88195)
{
	int ret = -1;

	pr_debug("%s: enter\n", __func__);

	aw88195_run_pwd(aw88195, false);
	ret = aw88195_sysst_check(aw88195);
	if (ret < 0) {
		aw88195_run_mute(aw88195, true);
	} else {
		aw88195_run_mute(aw88195, false);

		if ((aw88195->monitor_flag) &&
			(aw88195->spk_rcv_mode == AW881XX_SPEAKER_MODE)) {
			aw881xx_monitor_start(aw88195);
		}
	}

	return ret;
}

static void aw88195_stop(struct aw881xx *aw88195)
{
	pr_debug("%s: enter\n", __func__);

	aw88195_run_mute(aw88195, true);
	aw88195_run_pwd(aw88195, true);

	if (aw88195->monitor_flag)
		aw881xx_monitor_stop(aw88195);
}

/******************************************************
 *
 * aw88195 dsp
 *
 ******************************************************/
static int aw88195_dsp_check(struct aw881xx *aw88195)
{
	int ret = -1;
	uint16_t reg_val = 0;
	uint16_t iis_check_max = 5;
	uint16_t i = 0;

	pr_debug("%s: enter\n", __func__);

	aw88195_run_pwd(aw88195, false);
	aw88195_memclk_select(aw88195, AW88195_MEMCLK_PLL);
	for (i = 0; i < iis_check_max; i++) {
		ret = aw88195_get_iis_status(aw88195);
		if (ret < 0) {
			pr_err("%s: iis signal check error, reg=0x%x\n",
				__func__, reg_val);
			msleep(2);
		} else {
			if (aw88195->dsp_cfg == AW88195_DSP_WORK) {
				aw88195_dsp_enable(aw88195, false);
				aw88195_dsp_enable(aw88195, true);
				msleep(1);
				ret = aw88195_get_dsp_status(aw88195);
				if (ret < 0) {
					pr_err("%s: dsp wdt status error=%d\n",
						__func__, ret);
				} else {
					return 0;
				}
			} else if (aw88195->dsp_cfg == AW88195_DSP_BYPASS) {
				return 0;
			} else {
				pr_err("%s: unknown dsp cfg=%d\n",
					__func__, aw88195->dsp_cfg);
				return -EINVAL;
			}
		}
	}
	return -EINVAL;
}

static void aw88195_dsp_container_update(struct aw881xx *aw88195,
	struct aw88195_container *aw88195_cont, uint16_t base)
{
	int i = 0;
#ifdef AW88195_DSP_I2C_WRITES
	uint8_t tmp_val = 0;
	uint32_t tmp_len = 0;
#else
	uint16_t reg_val = 0;
#endif

	pr_debug("%s enter\n", __func__);

#ifdef AW88195_DSP_I2C_WRITES
	/* i2c writes */
	aw88195_i2c_write(aw88195, AW88195_REG_DSPMADD, base);
	for (i = 0; i < aw88195_cont->len; i += 2) {
		tmp_val = aw88195_cont->data[i + 0];
		aw88195_cont->data[i + 0] = aw88195_cont->data[i + 1];
		aw88195_cont->data[i + 1] = tmp_val;
	}
	for (i = 0; i < aw88195_cont->len;
		i += AW88195_MAX_RAM_WRITE_BYTE_SIZE) {
		if ((aw88195_cont->len - i) < AW88195_MAX_RAM_WRITE_BYTE_SIZE)
			tmp_len = aw88195_cont->len - i;
		else
			tmp_len = AW88195_MAX_RAM_WRITE_BYTE_SIZE;
		aw88195_i2c_writes(aw88195, AW88195_REG_DSPMDAT,
			&aw88195_cont->data[i], tmp_len);
	}
#else
	/* i2c write */
	aw88195_i2c_write(aw88195, AW88195_REG_DSPMADD, base);
	for (i = 0; i < aw88195_cont->len; i += 2) {
		reg_val = (aw88195_cont->data[i + 1] << 8) +
			aw88195_cont->data[i + 0];
		aw88195_i2c_write(aw88195, AW88195_REG_DSPMDAT, reg_val);
	}
#endif
	pr_debug("%s: exit\n", __func__);
}

static int aw88195_dsp_update_cali_re(struct aw881xx *aw88195)
{
	return aw881xx_dsp_update_cali_re(aw88195);
};

static void aw88195_dsp_cfg_loaded(const struct firmware *cont, void *context)
{
	struct aw881xx *aw88195 = context;
	struct aw88195_container *aw88195_cfg;
	int ret = -1;

	if (!cont) {
		pr_err("%s: failed to read %s\n",
			__func__, aw88195->cfg_name[aw88195->cfg_num]);
		release_firmware(cont);
		return;
	}

	pr_info("%s: loaded %s - size: %zu\n", __func__,
		aw88195->cfg_name[aw88195->cfg_num], cont ? cont->size : 0);

	aw88195_cfg = kzalloc(cont->size + sizeof(int), GFP_KERNEL);
	if (!aw88195_cfg) {
		release_firmware(cont);
		pr_err("%s: error allocating memory\n", __func__);
		return;
	}
	aw88195->dsp_cfg_len = cont->size;
	aw88195_cfg->len = cont->size;
	memcpy(aw88195_cfg->data, cont->data, cont->size);
	release_firmware(cont);

	if (aw88195->work_flag == false) {
		kfree(aw88195_cfg);
		pr_info("%s: mode_flag = %d\n", __func__, aw88195->work_flag);
		return;
	}

	mutex_lock(&aw88195->lock);
	aw88195_dsp_container_update(aw88195, aw88195_cfg,
		aw88195_base_addr[aw88195->cfg_num]);

	kfree(aw88195_cfg);

	aw88195_dsp_update_cali_re(aw88195);
	aw88195_dsp_set_vcalb(aw88195);

	ret = aw88195_dsp_check(aw88195);
	if (ret < 0) {
		aw88195->init = AW88195_INIT_NG;
		aw88195_run_mute(aw88195, true);
		pr_info("%s: fw/cfg update error\n", __func__);
	} else {
		pr_info("%s: fw/cfg update complete\n", __func__);

        if (aw88195->clk_div2_flag == 1){
            aw88195_i2c_write(aw88195, 0x65, 0x3f07);
        } else {
            aw88195_i2c_write(aw88195, 0x65, 0x7f07);
        }

		ret = aw88195_start(aw88195);
		if (ret < 0) {
			pr_err("%s: start fail, ret=%d\n", __func__, ret);
		} else {
			pr_info("%s: start success\n", __func__);
			aw88195->init = AW88195_INIT_OK;
		}
	}
	mutex_unlock(&aw88195->lock);
}

static int aw88195_load_dsp_cfg(struct aw881xx *aw88195)
{
	pr_info("%s enter\n", __func__);

	aw88195->cfg_num++;

	return request_firmware_nowait(THIS_MODULE, FW_ACTION_HOTPLUG,
			aw88195->cfg_name[aw88195->cfg_num],
			aw88195->dev, GFP_KERNEL, aw88195,
			aw88195_dsp_cfg_loaded);
}

static void aw88195_dsp_fw_loaded(const struct firmware *cont, void *context)
{
	struct aw881xx *aw88195 = context;
	struct aw88195_container *aw88195_cfg;
	int ret = -1;

	if (!cont) {
		pr_err("%s: failed to read %s\n",
			__func__, aw88195->cfg_name[aw88195->cfg_num]);
		release_firmware(cont);
		return;
	}

	pr_info("%s: loaded %s - size: %zu\n",
		__func__, aw88195->cfg_name[aw88195->cfg_num],
		cont ? cont->size : 0);

	aw88195_cfg = kzalloc(cont->size + sizeof(int), GFP_KERNEL);
	if (!aw88195_cfg) {
		release_firmware(cont);
		pr_err("%s: error allocating memory\n", __func__);
		return;
	}
	aw88195->dsp_fw_len = cont->size;
	aw88195_cfg->len = cont->size;
	memcpy(aw88195_cfg->data, cont->data, cont->size);
	release_firmware(cont);

	if (aw88195->work_flag == true) {
		mutex_lock(&aw88195->lock);
		aw88195_dsp_container_update(aw88195, aw88195_cfg,
			aw88195_base_addr[aw88195->cfg_num]);
		mutex_unlock(&aw88195->lock);
	}
	kfree(aw88195_cfg);

	if (aw88195->work_flag == false) {
		pr_info("%s: mode_flag = %d\n",
			__func__, aw88195->work_flag);
		return;
	}

	ret = aw88195_load_dsp_cfg(aw88195);
	if (ret < 0) {
		pr_err("%s: cfg loading requested failed: %d\n",
			__func__, ret);
	}
}

static int aw88195_load_dsp_fw(struct aw881xx *aw88195)
{
	int ret = -1;

	pr_info("%s enter\n", __func__);

	aw88195->cfg_num++;

	if (aw88195->work_flag == true) {
		mutex_lock(&aw88195->lock);
		aw88195_run_mute(aw88195, true);
		aw88195_dsp_enable(aw88195, false);
		aw88195_memclk_select(aw88195, AW88195_MEMCLK_PLL);
		mutex_unlock(&aw88195->lock);
	} else {
		pr_info("%s: mode_flag = %d\n",
			__func__, aw88195->work_flag);
		return ret;
	}

	return request_firmware_nowait(THIS_MODULE, FW_ACTION_HOTPLUG,
			aw88195->cfg_name[aw88195->cfg_num],
			aw88195->dev, GFP_KERNEL, aw88195,
			aw88195_dsp_fw_loaded);
}

static void aw88195_update_dsp(void *aw881xx)
{
	struct aw881xx *aw88195 = (struct aw881xx *)aw881xx;
	uint16_t iis_check_max = 5;
	int i = 0;
	int ret = -1;

	pr_info("%s: enter\n", __func__);

	aw881xx_get_cfg_shift(aw88195);
	aw88195_get_dsp_config(aw88195);

	for (i = 0; i < iis_check_max; i++) {
		mutex_lock(&aw88195->lock);
		ret = aw88195_get_iis_status(aw88195);
		mutex_unlock(&aw88195->lock);
		if (ret < 0) {
			pr_err("%s: get no iis signal, ret=%d\n",
				__func__, ret);
			msleep(2);
		} else {
			ret = aw88195_load_dsp_fw(aw88195);
			if (ret < 0) {
				pr_err("%s: cfg loading requested failed: %d\n",
					__func__, ret);
			}
			break;
		}
	}
}

static void aw88195_reg_container_update(struct aw881xx *aw88195,
	struct aw88195_container *aw88195_cont)
{
	int i = 0;
	uint16_t reg_addr = 0;
	uint16_t reg_val = 0;

	pr_debug("%s: enter\n", __func__);

	for (i = 0; i < aw88195_cont->len; i += 4) {
		reg_addr = (aw88195_cont->data[i + 1] << 8) +
			aw88195_cont->data[i + 0];
		reg_val = (aw88195_cont->data[i + 3] << 8) +
			aw88195_cont->data[i + 2];
		pr_debug("%s: reg=0x%04x, val = 0x%04x\n",
			__func__, reg_addr, reg_val);
		aw88195_i2c_write(aw88195,
			(uint8_t)reg_addr, (uint16_t)reg_val);
	}

	pr_debug("%s: exit\n", __func__);
}

static void aw88195_reg_loaded(const struct firmware *cont, void *context)
{
	struct aw881xx *aw88195 = context;
	struct aw88195_container *aw88195_cfg;
	int ret = -1;
	uint16_t iis_check_max = 5;
	int i = 0;

	if (!cont) {
		pr_err("%s: failed to read %s\n",
			__func__, aw88195->cfg_name[aw88195->cfg_num]);
		release_firmware(cont);
		return;
	}

	pr_info("%s: loaded %s - size: %zu\n",
		__func__, aw88195->cfg_name[aw88195->cfg_num],
		cont ? cont->size : 0);

	aw88195_cfg = kzalloc(cont->size + sizeof(int), GFP_KERNEL);
	if (!aw88195_cfg) {
		release_firmware(cont);
		pr_err("%s: error allocating memory\n", __func__);
		return;
	}
	aw88195_cfg->len = cont->size;
	memcpy(aw88195_cfg->data, cont->data, cont->size);
	release_firmware(cont);

	if (aw88195->work_flag == true) {
		mutex_lock(&aw88195->lock);
		aw88195_reg_container_update(aw88195, aw88195_cfg);
		aw88195_get_dsp_config(aw88195);
		mutex_unlock(&aw88195->lock);
	}
	kfree(aw88195_cfg);

	if (aw88195->work_flag == false) {
		pr_info("%s: mode_flag = %d\n",
			__func__, aw88195->work_flag);
		return;
	}

	for (i = 0; i < iis_check_max; i++) {
		mutex_lock(&aw88195->lock);
		ret = aw88195_get_iis_status(aw88195);
		mutex_unlock(&aw88195->lock);
		if (ret < 0) {
			pr_err("%s: get no iis signal, ret=%d\n",
				__func__, ret);
			msleep(2);
		} else {
			ret = aw88195_load_dsp_fw(aw88195);
			if (ret < 0) {
				pr_err("%s: cfg loading requested failed: %d\n",
					__func__, ret);
			}
			break;
		}
	}
}

static int aw88195_load_reg(struct aw881xx *aw88195)
{
	pr_info("%s: enter\n", __func__);

	return request_firmware_nowait(THIS_MODULE, FW_ACTION_HOTPLUG,
			aw88195->cfg_name[aw88195->cfg_num],
			aw88195->dev, GFP_KERNEL, aw88195,
			aw88195_reg_loaded);
}


static void aw88195_cold_start(struct aw881xx *aw88195)
{
	int ret = -1;

	pr_info("%s: enter\n", __func__);

	aw881xx_get_cfg_shift(aw88195);

	ret = aw88195_load_reg(aw88195);
	if (ret < 0) {
		pr_err("%s: cfg loading requested failed: %d\n",
			__func__, ret);
	}
}

void aw88195_smartpa_cfg(void *aw881xx, bool flag)
{
	struct aw881xx *aw88195 = (struct aw881xx *)aw881xx;
	int ret = -1;

	pr_info("%s: flag = %d\n", __func__, flag);

	aw88195->work_flag = flag;
	if (flag == true) {
		if ((aw88195->init == AW88195_INIT_ST)
			|| (aw88195->init == AW88195_INIT_NG)) {
			pr_info("%s: init = %d\n", __func__, aw88195->init);
			aw88195_cold_start(aw88195);
		} else {
			ret = aw88195_start(aw88195);
			if (ret < 0)
				pr_err("%s: start fail, ret=%d\n",
					__func__, ret);
			else
				pr_info("%s: start success\n", __func__);
		}
	} else {
		aw88195_stop(aw88195);
	}
}

/******************************************************
 *
 * asoc interface
 *
 ******************************************************/
void aw88195_get_volume(void *aw881xx, unsigned int *vol)
{
	struct aw881xx *aw88195 = (struct aw881xx *)aw881xx;
	uint16_t reg_val = 0;

	aw88195_i2c_read(aw88195, AW88195_REG_HAGCCFG7, &reg_val);
	*vol = (reg_val >> AW88195_VOL_REG_SHIFT);
}

void aw88195_set_volume(void *aw881xx, unsigned int vol)
{
	struct aw881xx *aw88195 = (struct aw881xx *)aw881xx;
	uint16_t reg_val = 0;

	/* cal real value */
	reg_val = (vol << AW88195_VOL_REG_SHIFT);

	/* write value */
	aw88195_i2c_write_bits(aw88195, AW88195_REG_HAGCCFG7,
		(uint16_t)AW88195_BIT_HAGCCFG7_VOL_MASK, reg_val);
}


int aw88195_update_hw_params(void *aw881xx)
{
	struct aw881xx *aw88195 = (struct aw881xx *)aw881xx;
	uint16_t reg_val = 0;

	/* match rate */
	switch (aw88195->rate) {
	case 8000:
		reg_val = AW88195_BIT_I2SCTRL_SR_8K;
		break;
	case 16000:
		reg_val = AW88195_BIT_I2SCTRL_SR_16K;
		break;
	case 32000:
		reg_val = AW88195_BIT_I2SCTRL_SR_32K;
		break;
	case 44100:
		reg_val = AW88195_BIT_I2SCTRL_SR_44P1K;
		break;
	case 48000:
		reg_val = AW88195_BIT_I2SCTRL_SR_48K;
		break;
	case 96000:
		reg_val = AW88195_BIT_I2SCTRL_SR_96K;
		break;
	case 192000:
		reg_val = AW88195_BIT_I2SCTRL_SR_192K;
		break;
	default:
		reg_val = AW88195_BIT_I2SCTRL_SR_48K;
		pr_err("%s: rate can not support\n", __func__);
		break;
	}
	/* set chip rate */
	aw88195_i2c_write_bits(aw88195, AW88195_REG_I2SCTRL,
		AW88195_BIT_I2SCTRL_SR_MASK, reg_val);

    pr_info("%s: rate:[%d],width:[%d]\n", __func__,\
            aw88195->rate,aw88195->rate);
    if ((aw88195->rate == 32000) || (aw88195->rate == 16000) || \
        (aw88195->rate == 8000)){
            aw88195->clk_div2_flag = 1;
            aw88195_i2c_write(aw88195, 0x65, 0x3f07);
        } else {
            aw88195->clk_div2_flag = 0;
            aw88195_i2c_write(aw88195, 0x65, 0x7f07);
        }

	/* match bit width */
	switch (aw88195->width) {
	case 16:
		reg_val = AW88195_BIT_I2SCTRL_FMS_16BIT;
		break;
	case 20:
		reg_val = AW88195_BIT_I2SCTRL_FMS_20BIT;
		break;
	case 24:
		reg_val = AW88195_BIT_I2SCTRL_FMS_24BIT;
		break;
	case 32:
		reg_val = AW88195_BIT_I2SCTRL_FMS_32BIT;
		break;
	default:
		reg_val = AW88195_BIT_I2SCTRL_FMS_16BIT;
		pr_err("%s: width can not support\n", __func__);
		break;
	}
	/* set width */
	aw88195_i2c_write_bits(aw88195, AW88195_REG_I2SCTRL,
		AW88195_BIT_I2SCTRL_FMS_MASK, reg_val);

	return 0;
}

/******************************************************
 *
 * irq
 *
 ******************************************************/
void aw88195_interrupt_setup(void *aw881xx)
{
	struct aw881xx *aw88195 = (struct aw881xx *)aw881xx;
	uint16_t reg_val = 0;

	pr_info("%s: enter\n", __func__);

	aw88195_i2c_read(aw88195, AW88195_REG_SYSINTM, &reg_val);
	reg_val &= (~AW88195_BIT_SYSINTM_PLLM);
	reg_val &= (~AW88195_BIT_SYSINTM_OTHM);
	reg_val &= (~AW88195_BIT_SYSINTM_OCDM);
	aw88195_i2c_write(aw88195, AW88195_REG_SYSINTM, reg_val);
}

void aw88195_interrupt_handle(void *aw881xx)
{
	struct aw881xx *aw88195 = (struct aw881xx *)aw881xx;
	uint16_t reg_val = 0;

	pr_info("%s: enter\n", __func__);

	aw88195_i2c_read(aw88195, AW88195_REG_SYSST, &reg_val);
	pr_info("%s: reg SYSST=0x%x\n", __func__, reg_val);

	aw88195_i2c_read(aw88195, AW88195_REG_SYSINT, &reg_val);
	pr_info("%s: reg SYSINT=0x%x\n", __func__, reg_val);

	aw88195_i2c_read(aw88195, AW88195_REG_SYSINTM, &reg_val);
	pr_info("%s: reg SYSINTM=0x%x\n", __func__, reg_val);
}

/*****************************************************
 *
 * monitor
 *
 *****************************************************/
/*
static int aw88195_monitor_get_gain_value(
	struct aw881xx *aw88195, uint16_t *reg_val)
{
	int ret = -1;

	if ((aw88195 == NULL) || (reg_val == NULL)) {
		pr_err("%s: aw88195 is %p, reg_val=%p\n",
			__func__, aw88195, reg_val);
		return ret;
	}

	ret = aw88195_i2c_read(aw88195, AW88195_REG_HAGCCFG7, reg_val);
	*reg_val = *reg_val >> AW88195_HAGC_GAIN_SHIFT;

	return ret;
}
*/
static int aw88195_monitor_set_gain_value(struct aw881xx *aw88195,
	uint16_t gain_value)
{
	int ret = -1;
	uint16_t reg_val = 0;
	uint16_t read_reg_val;

	if (aw88195 == NULL) {
		pr_err("%s: aw88195 is %p\n", __func__, aw88195);
		return ret;
	}

	ret = aw88195_i2c_read(aw88195, AW88195_REG_HAGCCFG7, &reg_val);
	if (ret)
		return ret;

	/* check gain */
	read_reg_val = reg_val;
	read_reg_val = read_reg_val >> AW88195_HAGC_GAIN_SHIFT;
	if (read_reg_val == gain_value) {
		pr_debug("%s: gain_value=0x%x, no change\n",
			__func__, gain_value);
		return ret;
	}

	reg_val &= AW88195_HAGC_GAIN_MASK;
	reg_val |= gain_value << AW88195_HAGC_GAIN_SHIFT;

	ret = aw88195_i2c_write(aw88195, AW88195_REG_HAGCCFG7, reg_val);
	if (ret < 0)
		return ret;

	pr_debug("%s: set reg_val=0x%x, gain_value=0x%x\n",
		__func__, reg_val, gain_value);

	return ret;
}

/*
static int aw88195_monitor_get_bst_ipeak(
	struct aw881xx *aw88195, uint16_t *reg_val)
{
	int ret = -1;

	if ((aw88195 == NULL) || (reg_val == NULL)) {
		pr_err("%s: aw88195 is %p, reg_val=%p\n",
			__func__, aw88195, reg_val);
		return ret;
	}

	ret = aw88195_i2c_read(aw88195, AW88195_REG_SYSCTRL2, reg_val);
	*reg_val = *reg_val & (AW88195_BIT_SYSCTRL2_BST_IPEAK_MASK);

	return ret;
}
*/
static int aw88195_monitor_set_bst_ipeak(struct aw881xx *aw88195,
	uint16_t bst_ipeak)
{
	int ret = -1;
	uint16_t reg_val = 0;
	uint16_t read_reg_val;

	if (aw88195 == NULL) {
		pr_err("%s: aw88195 is %p\n", __func__, aw88195);
		return ret;
	}

	ret = aw88195_i2c_read(aw88195, AW88195_REG_SYSCTRL2, &reg_val);
	if (ret < 0)
		return ret;

	/* check ipeak */
	read_reg_val = reg_val;
	read_reg_val = read_reg_val & (~AW88195_BIT_SYSCTRL2_BST_IPEAK_MASK);
	if (read_reg_val == bst_ipeak) {
		pr_debug("%s: ipeak=0x%x, no change\n", __func__, bst_ipeak);
		return ret;
	}

	reg_val &= AW88195_BIT_SYSCTRL2_BST_IPEAK_MASK;
	reg_val |= bst_ipeak;

	ret = aw88195_i2c_write(aw88195, AW88195_REG_SYSCTRL2, reg_val);
	if (ret < 0)
		return ret;

	pr_debug("%s: set reg_val=0x%x, bst_ipeak=0x%x\n",
		__func__, reg_val, bst_ipeak);

	return ret;
}

static int aw88195_monitor_vmax_check(struct aw881xx *aw88195)
{
	int ret = -1;

	ret = aw88195_get_iis_status(aw88195);
	if (ret < 0) {
		pr_err("%s: no iis signal\n", __func__);
		return ret;
	}

	ret = aw88195_get_dsp_status(aw88195);
	if (ret < 0) {
		pr_err("%s: dsp not work\n", __func__);
		return ret;
	}

	return 0;
}

static int aw88195_monitor_set_vmax(
	struct aw881xx *aw88195, uint16_t vmax)
{
	int ret = -1;
	uint16_t reg_val = 0;

	ret = aw88195_monitor_vmax_check(aw88195);
	if (ret) {
		pr_err("%s: vamx_check fail, ret=%d\n", __func__, ret);
		return ret;
	}

	ret = aw88195_dsp_read(aw88195, AW88195_DSP_REG_VMAX, &reg_val);
	if (ret)
		return ret;

	if (reg_val == vmax) {
		pr_info("%s: read vmax=0x%x\n", __func__, reg_val);
		return ret;
	}

	ret = aw88195_dsp_write(aw88195, AW88195_DSP_REG_VMAX, vmax);
	if (ret)
		return ret;

	return ret;
}

static int aw88195_monitor_get_voltage(struct aw881xx *aw88195,
	uint16_t *voltage)
{
	int ret = -1;

	if ((aw88195 == NULL) || (voltage == NULL)) {
		pr_err("%s aw88195 is %p, voltage=%p\n",
			__func__, aw88195, voltage);
		return ret;
	}

	ret = aw88195_i2c_read(aw88195, AW88195_REG_VBAT, voltage);
	if (ret < 0)
		return ret;

	*voltage = (*voltage) * AW88195_VBAT_RANGE /
		AW88195_VBAT_COEFF_INT_10BIT;

	pr_debug("%s: chip voltage=%dmV\n", __func__, *voltage);

	return ret;
}

static int aw88195_monitor_get_temperature(
	struct aw881xx *aw88195, int16_t *temp)
{
	int ret = -1;
	uint16_t reg_val = 0;

	if ((aw88195 == NULL) || (temp == NULL)) {
		pr_err("%s aw88195= %p, temp=%p\n",
			__func__, aw88195, temp);
		return ret;
	}

	ret = aw88195_i2c_read(aw88195, AW88195_REG_TEMP, &reg_val);
	if (ret < 0)
		return ret;

	if (reg_val & AW88195_TEMP_SIGN_MASK)
		reg_val |= AW88195_TEMP_NEG_MASK;

	*temp = (int)reg_val;

	pr_debug("%s: chip_temperature=%d\n", __func__, *temp);

	return ret;
}

static int aw88195_monitor_check_voltage(struct aw881xx *aw88195,
	uint16_t *bst_ipeak, uint16_t *gain_db)
{
	int ret = -1;
	uint16_t voltage = 0;

	if ((aw88195 == NULL) || (bst_ipeak == NULL) || (gain_db == NULL)) {
		pr_err("aw88195 is %p, bst_ipeak is %p, gain_db is %p\n",
			aw88195, bst_ipeak, gain_db);
		return ret;
	}

	ret = aw88195_monitor_get_voltage(aw88195, &voltage);

	if (voltage > AW88195_VOL_LIMIT_40) {
		*bst_ipeak = AW88195_IPEAK_350;
		*gain_db = AW88195_GAIN_00DB;
	} else if (voltage < AW88195_VOL_LIMIT_39 &&
		voltage > AW88195_VOL_LIMIT_38) {
		*bst_ipeak = AW88195_IPEAK_300;
		*gain_db = AW88195_GAIN_NEG_05DB;
	} else if (voltage < AW88195_VOL_LIMIT_37 &&
		voltage > AW88195_VOL_LIMIT_36) {
		*bst_ipeak = AW88195_IPEAK_275;
		*gain_db = AW88195_GAIN_NEG_10DB;
	} else if (voltage < AW88195_VOL_LIMIT_35) {
		*bst_ipeak = AW88195_IPEAK_250;
		*gain_db = AW88195_GAIN_NEG_15DB;
	} else {
		*bst_ipeak = AW88195_IPEAK_NONE;
		*gain_db = AW88195_GAIN_NONE;
	}
	pr_info("%s: bst_ipeak=0x%x, gain_db=0x%x\n",
		__func__, *bst_ipeak, *gain_db);

	return ret;
}

static void aw88195_monitor_check_temperature_deglitch(
	uint16_t *bst_ipeak, uint16_t *gain_db,
	uint16_t *vmax, int16_t temperature, int16_t pre_temp)
{
	if (temperature <= pre_temp) {
		if (temperature <= AW88195_TEMP_LIMIT_7 &&
			temperature >= AW88195_TEMP_LIMIT_5) {
			*bst_ipeak = AW88195_IPEAK_350;
			*gain_db = AW88195_GAIN_00DB;
			*vmax = AW88195_VMAX_80;
		} else if (temperature <= AW88195_TEMP_LIMIT_2 &&
			temperature >= AW88195_TEMP_LIMIT_0) {
			*bst_ipeak = AW88195_IPEAK_300;
			*gain_db = AW88195_GAIN_NEG_30DB;
			*vmax = AW88195_VMAX_70;
		} else if (temperature <= AW88195_TEMP_LIMIT_NEG_2 &&
			temperature >= AW88195_TEMP_LIMIT_NEG_5) {
			*bst_ipeak = AW88195_IPEAK_275;
			*gain_db = AW88195_GAIN_NEG_45DB;
			*vmax = AW88195_VMAX_60;
		}
	} else {
		if (temperature <= AW88195_TEMP_LIMIT_7 &&
			temperature >= AW88195_TEMP_LIMIT_5) {
			*bst_ipeak = AW88195_IPEAK_300;
			*gain_db = AW88195_GAIN_NEG_30DB;
			*vmax = AW88195_VMAX_70;
		} else if (temperature <= AW88195_TEMP_LIMIT_2 &&
			temperature >= AW88195_TEMP_LIMIT_0) {
			*bst_ipeak = AW88195_IPEAK_275;
			*gain_db = AW88195_GAIN_NEG_45DB;
			*vmax = AW88195_VMAX_60;
		} else if (temperature <= AW88195_TEMP_LIMIT_NEG_2 &&
			temperature >= AW88195_TEMP_LIMIT_NEG_5) {
			*bst_ipeak = AW88195_IPEAK_250;
			*gain_db = AW88195_GAIN_NEG_60DB;
			*vmax = AW88195_VMAX_50;
		}
	}
}

static int aw88195_monitor_check_temperature(
	struct aw881xx *aw88195, uint16_t *bst_ipeak,
	uint16_t *gain_db, uint16_t *vmax)
{
	int ret = -1;
	int16_t temperature = 0;
	int16_t pre_temp;

	if ((aw88195 == NULL) || (bst_ipeak == NULL) ||
		(gain_db == NULL) || (vmax == NULL)) {
		pr_err("%s: aw88195=%p, bst_ipeak=%p, gain_db=%p, vmax=%p\n",
			__func__, aw88195, bst_ipeak, gain_db, vmax);
		return ret;
	}
	pre_temp = aw88195->pre_temp;

	ret = aw88195_monitor_get_temperature(aw88195, &temperature);
	aw88195->pre_temp = temperature;
	if (temperature > AW88195_TEMP_LIMIT_7) {
		*bst_ipeak = AW88195_IPEAK_350;
		*gain_db = AW88195_GAIN_00DB;
		*vmax = AW88195_VMAX_80;
	} else if (temperature < AW88195_TEMP_LIMIT_5 &&
		temperature > AW88195_TEMP_LIMIT_2) {
		*bst_ipeak = AW88195_IPEAK_300;
		*gain_db = AW88195_GAIN_NEG_30DB;
		*vmax = AW88195_VMAX_70;
	} else if (temperature < AW88195_TEMP_LIMIT_0 &&
		temperature > AW88195_TEMP_LIMIT_NEG_2) {
		*bst_ipeak = AW88195_IPEAK_275;
		*gain_db = AW88195_GAIN_NEG_45DB;
		*vmax = AW88195_VMAX_60;
	} else if (temperature < AW88195_TEMP_LIMIT_NEG_5) {
		*bst_ipeak = AW88195_IPEAK_250;
		*gain_db = AW88195_GAIN_NEG_60DB;
		*vmax = AW88195_VMAX_50;
	} else {
		aw88195_monitor_check_temperature_deglitch(
			bst_ipeak, gain_db, vmax,
			temperature, pre_temp);
	}
	pr_info("%s: bst_ipeak=0x%x, gain_db=0x%x\n",
		__func__, *bst_ipeak, *gain_db);
	return ret;
}

static int aw88195_monitor_check_sysint(struct aw881xx *aw88195)
{
	int ret = -1;
	uint16_t sysint = 0;

	if (aw88195 == NULL) {
		pr_err("%s: aw88195 is NULL\n", __func__);
		return ret;
	}

	ret = aw88195_get_sysint(aw88195, &sysint);
	if (ret < 0)
		pr_err("%s: get_sysint fail, ret=%d\n", __func__, ret);
	else
		pr_info("%s: get_sysint=0x%04x\n", __func__, ret);

	return ret;
}

void aw88195_monitor_cal_ipeak(uint16_t *real_ipeak,
	uint16_t vol_ipeak, uint16_t temp_ipeak)
{
	if (real_ipeak == NULL) {
		pr_err("%s: real_ipeak=%p\n", __func__, real_ipeak);
		return;
	}

	if (vol_ipeak == AW88195_IPEAK_NONE &&
		temp_ipeak == AW88195_IPEAK_NONE)
		*real_ipeak = AW88195_IPEAK_NONE;
	else
		*real_ipeak = (vol_ipeak < temp_ipeak ? vol_ipeak : temp_ipeak);
}

void aw88195_monitor_cal_gain(uint16_t *real_gain,
	uint16_t vol_gain, uint16_t temp_gain)
{
	if (real_gain == NULL) {
		pr_err("%s: real_gain=%p\n", __func__, real_gain);
		return;
	}

	if (vol_gain == AW88195_GAIN_NONE ||
		temp_gain == AW88195_GAIN_NONE) {
		if (vol_gain == AW88195_GAIN_NONE &&
			temp_gain == AW88195_GAIN_NONE)
			*real_gain = AW88195_GAIN_NONE;
		else if (vol_gain == AW88195_GAIN_NONE)
			*real_gain = temp_gain;
		else
			*real_gain = vol_gain;
	} else {
		*real_gain = (vol_gain > temp_gain ? vol_gain : temp_gain);
	}
}

void aw88195_monitor(void *aw881xx)
{
	struct aw881xx *aw88195 = (struct aw881xx *)aw881xx;
	int ret;
	uint16_t vol_ipeak = 0;
	uint16_t vol_gain = 0;
	uint16_t temp_ipeak = 0;
	uint16_t temp_gain = 0;
	uint16_t vmax = 0;
	uint16_t real_ipeak;
	uint16_t real_gain;

	/* get ipeak and gain value by voltage and temperature */
	ret = aw88195_monitor_check_voltage(aw88195, &vol_ipeak, &vol_gain);
	if (ret < 0) {
		pr_err("%s: check voltage failed\n", __func__);
		return;
	}
	ret =
		aw88195_monitor_check_temperature(aw88195,
			&temp_ipeak, &temp_gain, &vmax);
	if (ret < 0) {
		pr_err("%s: check temperature failed\n", __func__);
		return;
	}
	/* get min Ipeak */
	if (vol_ipeak == AW88195_IPEAK_NONE &&
		temp_ipeak == AW88195_IPEAK_NONE)
		real_ipeak = AW88195_IPEAK_NONE;
	else
		real_ipeak = (vol_ipeak < temp_ipeak ? vol_ipeak : temp_ipeak);

	/* get min gain */
	if (vol_gain == AW88195_GAIN_NONE ||
		temp_gain == AW88195_GAIN_NONE) {
		if (vol_gain == AW88195_GAIN_NONE &&
			temp_gain == AW88195_GAIN_NONE)
			real_gain = AW88195_GAIN_NONE;
		else if (vol_gain == AW88195_GAIN_NONE)
			real_gain = temp_gain;
		else
			real_gain = vol_gain;
	} else {
		real_gain = (vol_gain > temp_gain ? vol_gain : temp_gain);
	}

	if (real_ipeak != AW88195_IPEAK_NONE)
		aw88195_monitor_set_bst_ipeak(aw88195, real_ipeak);

	if (real_gain != AW88195_GAIN_NONE)
		aw88195_monitor_set_gain_value(aw88195, real_gain);

	if (vmax != AW88195_VMAX_NONE)
		aw88195_monitor_set_vmax(aw88195, vmax);

	aw88195_monitor_check_sysint(aw88195);
}


unsigned char aw88195_get_reg_access(uint8_t reg_addr)
{
	return aw88195_reg_access[reg_addr];
}

int aw88195_get_dsp_mem_reg(void *aw881xx,
	uint8_t *mem_addr_reg, uint8_t *msm_data_reg)
{
	struct aw881xx *aw88195 = (struct aw881xx *)aw881xx;

	pr_debug("%s: pid=%d\n", __func__, aw88195->pid);
	*mem_addr_reg = AW88195_REG_DSPMADD;
	*msm_data_reg = AW88195_REG_DSPMDAT;

	return 0;
}

struct device_ops aw88195_dev_ops = {
	aw88195_update_hw_params,
	aw88195_get_volume,
	aw88195_set_volume,

	aw88195_smartpa_cfg,
	aw88195_update_dsp,
	aw88195_run_pwd,
	aw88195_monitor,
	aw88195_set_cali_re,

	aw88195_interrupt_setup,
	aw88195_interrupt_handle,

	aw88195_get_hmute,
	aw88195_get_dsp_config,
	aw88195_get_iis_status,
	aw88195_get_reg_access,
	aw88195_get_dsp_mem_reg,
};

void aw88195_ops(void *aw881xx)
{
	struct aw881xx *aw88195 = (struct aw881xx *)aw881xx;

	aw88195->dev_ops = aw88195_dev_ops;
}

