// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2021 Unisoc, Inc.
 */

#include <linux/debugfs.h>
#include <linux/delay.h>
#include <linux/kernel.h>
#include <linux/mfd/syscon.h>
#include <linux/miscdevice.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/regmap.h>
#include "apdu_r3p0.h"
#include "qogirn6pro_ise_pd.h"

/* ise pd reg */
#define PD_ISE_CFG_0			(0x3f0)
#define PD_ISE_AUTO_SHUTDOWN_EN		(BIT(24))
#define PD_ISE_FORCE_SHUTDOWN		(BIT(25))

#define PWR_STATUS_DBG_23		(0x54c)
#define ISE_PD_STATUS(x)		(((x) & GENMASK(12, 8)) >> 8)
#define ISE_PD_POWER_ON			(0)
#define ISE_PD_POWER_DOWN		(7)

#define FORCE_DEEP_SLEEP_CFG_0		(0x818)
#define ISE_FORCE_DEEP_SLEEP_REG	(BIT(5))

#define ADM_SOFT_RESET			(0xb8c)
#define ISE_AON_SOFT_RESET		(BIT(5))

#define SYS_SOFT_RST_0			(0xb98)
#define SOFT_RST_SEL_0			(0xba8)
#define ISE_SOFT_RST			(BIT(27))

#define RCO150M_REL_CFG			(0x9f0)
#define RCO150M_RFC_OFF			(BIT(1))

/* ise aon clock */
#define ISE_AON_RAM_CFG			(0x080)
#define REG_RAM_PD_ISE_AON_BIT		(BIT(2))

#define GATE_EN_SEL6_CFG		(0x068)
#define CGM_XBUF_26M_ISE_AUTO_GATE_SEL	(BIT(22))
#define CGM_XBUF_2M_ISE_AUTO_GATE_SEL	(BIT(23))

#define GATE_EN_SW_CTL6_CFG		(0x08c)
#define CGM_XBUF_26M_ISE_FORCE_EN	(BIT(22))
#define CGM_XBUF_2M_ISE_FORCE_EN	(BIT(23))

/* ise aon */
#define RCO150_EN			(0x4)
#define RCO150_EN_BIT			(BIT(27))
#define RCO150_RST			(0xc)
#define RCO150_RST_BIT			(BIT(11))

#define RCO150M_CFG0			(0xca4)
#define RCO150M_CAL_DONE		(BIT(5))
#define RCO150M_CAL_START		(BIT(4))
#define RCO150M_CAL_TIMEOUT		(1000)

#ifdef pr_fmt
#undef pr_fmt
#endif
#define pr_fmt(fmt) "sprd-apdu: " fmt

long qogirn6pro_ise_pd_status_check(void *apdu_dev)
{
	struct sprd_apdu_device *apdu = (struct sprd_apdu_device *)apdu_dev;
	u32 ise_status = 0, timeout = 0;
	int ret;

	while (1) {
		ret = regmap_read(apdu->pd_ise->ise_pd_reg_base,
				  PWR_STATUS_DBG_23, &ise_status);
		if (ret)
			goto err;
		ise_status = ISE_PD_STATUS(ise_status);
		if (ise_status == ISE_PD_POWER_DOWN) {
			ise_status = ISE_PD_PWR_DOWN;
			break;
		} else if (ise_status == ISE_PD_POWER_ON) {
			ise_status = ISE_PD_PWR_ON;
			break;
		}

		usleep_range(10, 20);
		if (++timeout > APDU_PD_CHECK_TIMEOUT)
			return -ETIMEDOUT;
	}

	return ise_status;

err:
	dev_err(apdu->dev, "ise pd status check fail\n");
	return -EFAULT;
}

long qogirn6pro_ise_cold_power_on(void *apdu_dev)
{
	struct sprd_apdu_device *apdu = (struct sprd_apdu_device *)apdu_dev;
	u32 reg_mask, reg_value = 0, timeout = 0;
	long ret;

	if (apdu->pd_ise->ise_pd_status_check(apdu) != ISE_PD_PWR_DOWN) {
		dev_err(apdu->dev, "ise isn't power down status before this OP\n");
		return -EFAULT;
	}

	/* keep reset ise high before clock and such other operation */
	ret = regmap_update_bits(apdu->pd_ise->ise_pd_reg_base,
				 ADM_SOFT_RESET, ISE_AON_SOFT_RESET,
				 ISE_AON_SOFT_RESET);
	if (ret)
		goto err;
	usleep_range(10, 20);

	/* close ise force deep sleep */
	ret = regmap_update_bits(apdu->pd_ise->ise_pd_reg_base,
				 FORCE_DEEP_SLEEP_CFG_0,
				 ISE_FORCE_DEEP_SLEEP_REG, 0);
	if (ret)
		goto err;
	usleep_range(10, 20);

	/* close ise ram/aon force power down */
	ret = regmap_update_bits(apdu->pd_ise->ise_pd_reg_base,
				 ISE_AON_RAM_CFG, REG_RAM_PD_ISE_AON_BIT, 0);
	if (ret)
		goto err;
	usleep_range(10, 20);

	reg_mask =
		CGM_XBUF_26M_ISE_AUTO_GATE_SEL | CGM_XBUF_2M_ISE_AUTO_GATE_SEL;
	ret = regmap_read(apdu->pd_ise->ise_aon_clk_reg_base,
			  GATE_EN_SEL6_CFG, &reg_value);
	if (ret)
		goto err;
	reg_value &= (~reg_mask);
	ret = regmap_write(apdu->pd_ise->ise_aon_clk_reg_base,
			   GATE_EN_SEL6_CFG, reg_value);
	if (ret)
		goto err;
	usleep_range(10, 20);

	reg_mask = CGM_XBUF_26M_ISE_FORCE_EN | CGM_XBUF_2M_ISE_FORCE_EN;
	ret = regmap_read(apdu->pd_ise->ise_aon_clk_reg_base,
			  GATE_EN_SW_CTL6_CFG, &reg_value);
	if (ret)
		goto err;
	reg_value &= (~reg_mask);
	ret = regmap_write(apdu->pd_ise->ise_aon_clk_reg_base,
			   GATE_EN_SW_CTL6_CFG, reg_value);
	if (ret)
		goto err;
	usleep_range(10, 20);

	/*
	 * force close rco 150M PD for timing violation
	 * before release reset hard reset signal
	 * 0:RCO150M not force OFF  1:RCO150M force OFF
	 */
	ret = regmap_update_bits(apdu->pd_ise->ise_pd_reg_base,
				 RCO150M_REL_CFG, RCO150M_RFC_OFF,
				 RCO150M_RFC_OFF);
	if (ret)
		goto err;
	usleep_range(10, 20);

	/* release ise hard  reset signal */
	ret = regmap_update_bits(apdu->pd_ise->ise_pd_reg_base,
				 ADM_SOFT_RESET, ISE_AON_SOFT_RESET, 0);
	if (ret)
		goto err;
	usleep_range(10, 20);

	/* RCO150M not force OFF */
	ret = regmap_update_bits(apdu->pd_ise->ise_pd_reg_base,
				 RCO150M_REL_CFG, RCO150M_RFC_OFF, 0);
	if (ret)
		goto err;
	usleep_range(10, 20);

	reg_mask =
		CGM_XBUF_26M_ISE_AUTO_GATE_SEL | CGM_XBUF_2M_ISE_AUTO_GATE_SEL;
	ret = regmap_read(apdu->pd_ise->ise_aon_clk_reg_base,
			  GATE_EN_SEL6_CFG, &reg_value);
	if (ret)
		goto err;
	reg_value |= reg_mask;
	ret = regmap_write(apdu->pd_ise->ise_aon_clk_reg_base,
			   GATE_EN_SEL6_CFG, reg_value);
	if (ret)
		goto err;
	usleep_range(10, 20);

	reg_mask = CGM_XBUF_26M_ISE_FORCE_EN | CGM_XBUF_2M_ISE_FORCE_EN;
	ret = regmap_read(apdu->pd_ise->ise_aon_clk_reg_base,
			  GATE_EN_SW_CTL6_CFG, &reg_value);
	if (ret)
		goto err;
	reg_value |= reg_mask;
	ret = regmap_write(apdu->pd_ise->ise_aon_clk_reg_base,
			   GATE_EN_SW_CTL6_CFG, reg_value);
	if (ret)
		goto err;
	usleep_range(10, 20);

	/* after this operation, ise will power on */
	ret = regmap_update_bits(apdu->pd_ise->ise_pd_reg_base, PD_ISE_CFG_0,
				 PD_ISE_FORCE_SHUTDOWN, 0);
	if (ret)
		goto err;
	usleep_range(10, 20);

	/* trigger rco150m (ise clock source) calibrating after ise power on */
	ret = regmap_update_bits(apdu->pd_ise->ise_aon_reg_base, RCO150M_CFG0,
				 RCO150M_CAL_START, RCO150M_CAL_START);
	if (ret)
		goto err;
	usleep_range(10, 20);

	/* wait for rco150m calibrating done */
	ret = regmap_read(apdu->pd_ise->ise_aon_reg_base,
			  RCO150M_CFG0, &reg_value);
	if (ret)
		goto err;
	while (!(reg_value & RCO150M_CAL_DONE)) {
		ret = regmap_read(apdu->pd_ise->ise_aon_reg_base,
				  RCO150M_CFG0, &reg_value);
		if (ret)
			goto err;

		usleep_range(10, 20);
		timeout++;
		if (timeout >= RCO150M_CAL_TIMEOUT) {
			dev_err(apdu->dev, "ise rco150m cal timeout\n");
			goto err;
		}
	}

	/* when ise deep sleep, this config will let ise auto power down */
	ret = regmap_update_bits(apdu->pd_ise->ise_pd_reg_base,
				 PD_ISE_CFG_0, PD_ISE_AUTO_SHUTDOWN_EN,
				 PD_ISE_AUTO_SHUTDOWN_EN);
	if (ret)
		goto err;

	return 0;

err:
	dev_err(apdu->dev, "ise cold power on fail\n");
	return ret;
}

long qogirn6pro_ise_full_power_down(void *apdu_dev)
{
	struct sprd_apdu_device *apdu = (struct sprd_apdu_device *)apdu_dev;
	long ret;

	/* if want to full power down, need make sure power on first */
	if (sprd_apdu_power_on_check(apdu, APDU_POWER_ON_CHECK_TIMES) < 0) {
		dev_err(apdu->dev, "power on check fail\n");
		return -ENXIO;
	}

	ret = sprd_apdu_normal_pd_ise_req(apdu);
	if (ret < 0) {
		dev_err(apdu->dev, "normal pwr down fail\n");
		return ret;
	}

	ret = med_rewrite_post_process_check(apdu);
	if (ret < 0) {
		dev_err(apdu->dev, "wait med wr done for ise pd fail\n");
		return ret;
	}

	/* save med info(cnt/lv1) to flash and power down ise using pmu reg */
	sprd_apdu_normal_pd_sync_cnt_lv1(apdu);

	return 0;
}

long qogirn6pro_ise_hard_reset(void *apdu_dev)
{
	struct sprd_apdu_device *apdu = (struct sprd_apdu_device *)apdu_dev;
	int ret;

	ret = regmap_update_bits(apdu->pd_ise->ise_pd_reg_base, ADM_SOFT_RESET,
				 ISE_AON_SOFT_RESET, ISE_AON_SOFT_RESET);
	if (ret)
		goto err;

	usleep_range(800, 1000);
	ret = regmap_update_bits(apdu->pd_ise->ise_pd_reg_base, ADM_SOFT_RESET,
				 ISE_AON_SOFT_RESET, 0);
	if (ret)
		goto err;

	return 0;

err:
	dev_err(apdu->dev, "ise hard reset fail\n");
	return ret;
}

long qogirn6pro_ise_soft_reset(void *apdu_dev)
{
	struct sprd_apdu_device *apdu = (struct sprd_apdu_device *)apdu_dev;
	int ret;

	/*
	 * clear ise soft reset sel--if soft reset done,
	 * ise will auto release reset signal
	 */
	ret = regmap_update_bits(apdu->pd_ise->ise_pd_reg_base, SOFT_RST_SEL_0,
				 ISE_SOFT_RST, 0);
	if (ret)
		goto err;

	/* ensure the initial value is 0 */
	ret = regmap_update_bits(apdu->pd_ise->ise_pd_reg_base, SYS_SOFT_RST_0,
				 ISE_SOFT_RST, 0);
	if (ret)
		goto err;
	ret = regmap_update_bits(apdu->pd_ise->ise_pd_reg_base, SYS_SOFT_RST_0,
				 ISE_SOFT_RST, ISE_SOFT_RST);
	if (ret)
		goto err;

	usleep_range(800, 1000);
	/* clear for next time trigger */
	ret = regmap_update_bits(apdu->pd_ise->ise_pd_reg_base, SYS_SOFT_RST_0,
				 ISE_SOFT_RST, 0);
	if (ret)
		goto err;

	return 0;

err:
	dev_err(apdu->dev, "ise soft reset fail\n");
	return ret;
}

long qogirn6pro_ise_hard_reset_set(void *apdu_dev)
{
	struct sprd_apdu_device *apdu = (struct sprd_apdu_device *)apdu_dev;
	int ret;

	ret = regmap_update_bits(apdu->pd_ise->ise_pd_reg_base,
				 ADM_SOFT_RESET, ISE_AON_SOFT_RESET,
				 ISE_AON_SOFT_RESET);
	if (ret)
		goto err;

	return 0;

err:
	dev_err(apdu->dev, "ise hard reset set high fail\n");
	return ret;
}

long qogirn6pro_ise_hard_reset_clr(void *apdu_dev)
{
	struct sprd_apdu_device *apdu = (struct sprd_apdu_device *)apdu_dev;
	int ret;

	ret = regmap_update_bits(apdu->pd_ise->ise_pd_reg_base,
				 ADM_SOFT_RESET, ISE_AON_SOFT_RESET, 0);

	if (ret)
		goto err;

	return 0;

err:
	dev_err(apdu->dev, "ise hard reset clear low fail\n");
	return ret;
}

MODULE_DESCRIPTION("Spreadtrum APDU driver");
MODULE_LICENSE("GPL v2");
