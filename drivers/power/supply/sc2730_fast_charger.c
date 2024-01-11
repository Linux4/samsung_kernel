// SPDX-License-Identifier: GPL-2.0
// Copyright (C) 2018 Spreadtrum Communications Inc.

#include <linux/module.h>
#include <linux/of_device.h>
#include <linux/platform_device.h>
#include <linux/power_supply.h>
#include <linux/usb/phy.h>
#include <linux/regmap.h>
#include <linux/notifier.h>
#include <linux/nvmem-consumer.h>
#include <linux/of.h>
#include <linux/slab.h>
#include <linux/power/charger-manager.h>
#include <linux/usb/tcpm.h>
#include <linux/usb/pd.h>

#define FCHG1_TIME1				0x0
#define FCHG1_TIME2				0x4
#define FCHG1_DELAY				0x8
#define FCHG2_DET_HIGH				0xc
#define FCHG2_DET_LOW				0x10
#define FCHG2_DET_LOW_CV			0x14
#define FCHG2_DET_HIGH_CV			0x18
#define FCHG2_DET_LOW_CC			0x1c
#define FCHG2_ADJ_TIME1				0x20
#define FCHG2_ADJ_TIME2				0x24
#define FCHG2_ADJ_TIME3				0x28
#define FCHG2_ADJ_TIME4				0x2c
#define FCHG_CTRL				0x30
#define FCHG_ADJ_CTRL				0x34
#define FCHG_INT_EN				0x38
#define FCHG_INT_CLR				0x3c
#define FCHG_INT_STS				0x40
#define FCHG_INT_STS0				0x44
#define FCHG_ERR_STS				0x48

#define SC2721_MODULE_EN0		0xC08
#define SC2721_CLK_EN0			0xC10
#define SC2721_IB_CTRL			0xEA4
#define SC2730_MODULE_EN0		0x1808
#define SC2730_CLK_EN0			0x1810
#define SC2730_IB_CTRL			0x1b84
#define UMP9620_MODULE_EN0		0x2008
#define UMP9620_CLK_EN0			0x2010
#define UMP9620_IB_CTRL			0x2384

#define ANA_REG_IB_TRIM_MASK			GENMASK(6, 0)
#define ANA_REG_IB_TRIM_SHIFT			2
#define ANA_REG_IB_TRIM_EM_SEL_BIT		BIT(1)
#define ANA_REG_IB_TRUM_OFFSET			0x1e

#define FAST_CHARGE_MODULE_EN0_BIT		BIT(11)
#define FAST_CHARGE_RTC_CLK_EN0_BIT		BIT(4)

#define FCHG_ENABLE_BIT				BIT(0)
#define FCHG_INT_EN_BIT				BIT(1)
#define FCHG_INT_CLR_MASK			BIT(1)
#define FCHG_TIME1_MASK				GENMASK(10, 0)
#define FCHG_TIME2_MASK				GENMASK(11, 0)
#define FCHG_DET_VOL_MASK			GENMASK(1, 0)
#define FCHG_DET_VOL_SHIFT			3
#define FCHG_DET_VOL_EXIT_SFCP			3
#define FCHG_CALI_MASK				GENMASK(15, 9)
#define FCHG_CALI_SHIFT				9

#define FCHG_ERR0_BIT				BIT(1)
#define FCHG_ERR1_BIT				BIT(2)
#define FCHG_ERR2_BIT				BIT(3)
#define FCHG_OUT_OK_BIT				BIT(0)

#define FCHG_INT_STS_DETDONE			BIT(5)

/* FCHG1_TIME1_VALUE is used for detect the time of V > VT1 */
#define FCHG1_TIME1_VALUE			0x514
/* FCHG1_TIME2_VALUE is used for detect the time of V > VT2 */
#define FCHG1_TIME2_VALUE			0x9c4

#define FCHG_VOLTAGE_5V				5000000
#define FCHG_VOLTAGE_9V				9000000
#define FCHG_VOLTAGE_12V			12000000
#define FCHG_VOLTAGE_20V			20000000
/* Tab A8_S code for AX6300SDEV-324 by wenyaqi at 20220613 start */
#ifdef CONFIG_AFC
#define AFC_VOLTAGE_9V				9000000
#endif
/* Tab A8_S code for AX6300SDEV-324 by wenyaqi at 20220613 end */

#define SC2730_FCHG_TIMEOUT			msecs_to_jiffies(5000)
#define SC2730_FAST_CHARGER_DETECT_MS		msecs_to_jiffies(1000)

#define SC2730_PD_DEFAULT_POWER_UW		10000000

#define SC2730_ENABLE_PPS			2
#define SC2730_DISABLE_PPS			1

struct sc27xx_fast_chg_data {
	u32 module_en;
	u32 clk_en;
	u32 ib_ctrl;
};

static const struct sc27xx_fast_chg_data sc2721_info = {
	.module_en = SC2721_MODULE_EN0,
	.clk_en = SC2721_CLK_EN0,
	.ib_ctrl = SC2721_IB_CTRL,
};

static const struct sc27xx_fast_chg_data sc2730_info = {
	.module_en = SC2730_MODULE_EN0,
	.clk_en = SC2730_CLK_EN0,
	.ib_ctrl = SC2730_IB_CTRL,
};

static const struct sc27xx_fast_chg_data ump9620_info = {
	.module_en = UMP9620_MODULE_EN0,
	.clk_en = UMP9620_CLK_EN0,
	.ib_ctrl = UMP9620_IB_CTRL,
};

/* Tab A8 code for SR-AX6300-01-5 by wenyaqi at 20210827 start */
#ifndef CONFIG_AFC
struct sc2730_fchg_info {
	struct device *dev;
	struct regmap *regmap;
	struct usb_phy *usb_phy;
	struct notifier_block usb_notify;
	struct notifier_block pd_notify;
	struct power_supply *psy_usb;
	struct power_supply *psy_tcpm;
	struct delayed_work work;
	struct work_struct pd_change_work;
	struct mutex lock;
	struct completion completion;
	struct adapter_power_cap pd_source_cap;
	u32 state;
	u32 base;
	int input_vol;
	int pd_fixed_max_uw;
	u32 limit;
	bool detected;
	bool pd_enable;
	bool sfcp_enable;
	bool pps_enable;
	bool pps_active;
	bool support_pd_pps;
	const struct sc27xx_fast_chg_data *pdata;
};
#endif
/* Tab A8 code for SR-AX6300-01-5 by wenyaqi at 20210827 end */

static int sc2730_fchg_internal_cur_calibration(struct sc2730_fchg_info *info)
{
	struct nvmem_cell *cell;
	int calib_data, calib_current, ret;
	void *buf;
	size_t len;
	const struct sc27xx_fast_chg_data *pdata = info->pdata;

	cell = nvmem_cell_get(info->dev, "fchg_cur_calib");
	if (IS_ERR(cell))
		return PTR_ERR(cell);

	buf = nvmem_cell_read(cell, &len);
	nvmem_cell_put(cell);

	if (IS_ERR(buf))
		return PTR_ERR(buf);

	memcpy(&calib_data, buf, min(len, sizeof(u32)));
	kfree(buf);

	/*
	 * In the handshake protocol behavior of sfcp, the current source
	 * of the fast charge internal module is small, we improve it
	 * by set the register ANA_REG_IB_CTRL. Now we add 30 level compensation.
	 */
	calib_current = (calib_data & FCHG_CALI_MASK) >> FCHG_CALI_SHIFT;
	calib_current += ANA_REG_IB_TRUM_OFFSET;

	ret = regmap_update_bits(info->regmap,
				 pdata->ib_ctrl,
				 ANA_REG_IB_TRIM_MASK << ANA_REG_IB_TRIM_SHIFT,
				 (calib_current & ANA_REG_IB_TRIM_MASK) << ANA_REG_IB_TRIM_SHIFT);
	if (ret) {
		dev_err(info->dev, "failed to calibrate fast charger current.\n");
		return ret;
	}

	/*
	 * Fast charge dm current source calibration mode, enable soft calibration mode.
	 */
	ret = regmap_update_bits(info->regmap, pdata->ib_ctrl,
				 ANA_REG_IB_TRIM_EM_SEL_BIT,
				 0);
	if (ret) {
		dev_err(info->dev, "failed to select ib trim mode.\n");
		return ret;
	}

	return 0;
}

static irqreturn_t sc2730_fchg_interrupt(int irq, void *dev_id)
{
	struct sc2730_fchg_info *info = dev_id;
	u32 int_sts, int_sts0;
	int ret;

	ret = regmap_read(info->regmap, info->base + FCHG_INT_STS, &int_sts);
	if (ret)
		return IRQ_RETVAL(ret);

	ret = regmap_read(info->regmap, info->base + FCHG_INT_STS0, &int_sts0);
	if (ret)
		return IRQ_RETVAL(ret);

	ret = regmap_update_bits(info->regmap, info->base + FCHG_INT_EN,
				 FCHG_INT_EN_BIT, 0);
	if (ret) {
		dev_err(info->dev, "failed to disable fast charger irq.\n");
		return IRQ_RETVAL(ret);
	}

	ret = regmap_update_bits(info->regmap, info->base + FCHG_INT_CLR,
				 FCHG_INT_CLR_MASK, FCHG_INT_CLR_MASK);
	if (ret) {
		dev_err(info->dev, "failed to clear fast charger interrupts\n");
		return IRQ_RETVAL(ret);
	}

	if ((int_sts & FCHG_INT_STS_DETDONE) && !(int_sts0 & FCHG_OUT_OK_BIT))
		dev_warn(info->dev,
			 "met some errors, now status = 0x%x, status0 = 0x%x\n",
			 int_sts, int_sts0);

	if (info->state == POWER_SUPPLY_USB_TYPE_PD)
		dev_info(info->dev, "Already PD, don't update SFCP\n");
	else if ((int_sts & FCHG_INT_STS_DETDONE) && (int_sts0 & FCHG_OUT_OK_BIT))
		info->state = POWER_SUPPLY_USB_TYPE_SFCP_1P0;
	else
		info->state = POWER_SUPPLY_USB_TYPE_UNKNOWN;

	complete(&info->completion);

	return IRQ_HANDLED;
}

static void sc2730_fchg_detect_status(struct sc2730_fchg_info *info)
{
	unsigned int min, max;

	/*
	 * If the USB charger status has been USB_CHARGER_PRESENT before
	 * registering the notifier, we should start to charge with getting
	 * the charge current.
	 */
	if (info->usb_phy->chg_state != USB_CHARGER_PRESENT)
		return;

	usb_phy_get_charger_current(info->usb_phy, &min, &max);

	info->limit = min;
	/*
	 * There is a confilt between charger detection and fast charger
	 * detection, and BC1.2 detection time consumption is <300ms,
	 * so we delay fast charger detection to avoid this issue.
	 */
	schedule_delayed_work(&info->work, SC2730_FAST_CHARGER_DETECT_MS);
}

static int sc2730_fchg_usb_change(struct notifier_block *nb,
				     unsigned long limit, void *data)
{
	struct sc2730_fchg_info *info =
		container_of(nb, struct sc2730_fchg_info, usb_notify);

	info->limit = limit;
	if (!info->limit) {
		cancel_delayed_work(&info->work);
		schedule_delayed_work(&info->work, 0);
	} else {
		/*
		 * There is a confilt between charger detection and fast charger
		 * detection, and BC1.2 detection time consumption is <300ms,
		 * so we delay fast charger detection to avoid this issue.
		 */
		schedule_delayed_work(&info->work, SC2730_FAST_CHARGER_DETECT_MS);
	}
	return NOTIFY_OK;
}

static u32 sc2730_fchg_get_detect_status(struct sc2730_fchg_info *info)
{
	unsigned long timeout;
	int value, ret;
	const struct sc27xx_fast_chg_data *pdata = info->pdata;

	/*
	 * In cold boot phase, system will detect fast charger status,
	 * if charger is not plugged in, it will cost another 2s
	 * to detect fast charger status, so we detect fast charger
	 * status only when DCP charger is plugged in
	 */
	if (info->usb_phy->chg_type != DCP_TYPE)
		return POWER_SUPPLY_USB_TYPE_UNKNOWN;

	reinit_completion(&info->completion);

	if (info->input_vol < FCHG_VOLTAGE_9V)
		value = 0;
	else if (info->input_vol < FCHG_VOLTAGE_12V)
		value = 1;
	else if (info->input_vol < FCHG_VOLTAGE_20V)
		value = 2;
	else
		value = 3;

	/*
	 * Due to the the current source of the fast charge internal module is small
	 * we need to dynamically calibrate it through the software during the process
	 * of identifying fast charge. After fast charge recognition is completed, we
	 * disable soft calibration compensate function, in order to prevent the dm current
	 * source from deviating in accuracy when used in other modules.
	 */
	ret = sc2730_fchg_internal_cur_calibration(info);
	if (ret) {
		dev_err(info->dev, "failed to set fast charger calibration.\n");
		return ret;
	}

	ret = regmap_update_bits(info->regmap, pdata->module_en,
				 FAST_CHARGE_MODULE_EN0_BIT,
				 FAST_CHARGE_MODULE_EN0_BIT);
	if (ret) {
		dev_err(info->dev, "failed to enable fast charger.\n");
		return ret;
	}

	ret = regmap_update_bits(info->regmap, pdata->clk_en,
				 FAST_CHARGE_RTC_CLK_EN0_BIT,
				 FAST_CHARGE_RTC_CLK_EN0_BIT);
	if (ret) {
		dev_err(info->dev,
			"failed to enable fast charger clock.\n");
		return ret;
	}

	ret = regmap_update_bits(info->regmap, info->base + FCHG1_TIME1,
				 FCHG_TIME1_MASK, FCHG1_TIME1_VALUE);
	if (ret) {
		dev_err(info->dev, "failed to set fast charge time1\n");
		return ret;
	}

	ret = regmap_update_bits(info->regmap, info->base + FCHG1_TIME2,
				 FCHG_TIME2_MASK, FCHG1_TIME2_VALUE);
	if (ret) {
		dev_err(info->dev, "failed to set fast charge time2\n");
		return ret;
	}

	ret = regmap_update_bits(info->regmap, info->base + FCHG_CTRL,
			FCHG_DET_VOL_MASK << FCHG_DET_VOL_SHIFT,
			(value & FCHG_DET_VOL_MASK) << FCHG_DET_VOL_SHIFT);
	if (ret) {
		dev_err(info->dev,
			"failed to set fast charger detect voltage.\n");
		return ret;
	}

	ret = regmap_update_bits(info->regmap, info->base + FCHG_CTRL,
				 FCHG_ENABLE_BIT, FCHG_ENABLE_BIT);
	if (ret) {
		dev_err(info->dev, "failed to enable fast charger.\n");
		return ret;
	}

	ret = regmap_update_bits(info->regmap, info->base + FCHG_INT_EN,
				 FCHG_INT_EN_BIT, FCHG_INT_EN_BIT);
	if (ret) {
		dev_err(info->dev, "failed to enable fast charger irq.\n");
		return ret;
	}

	timeout = wait_for_completion_timeout(&info->completion,
					      SC2730_FCHG_TIMEOUT);
	if (!timeout) {
		dev_err(info->dev, "timeout to get fast charger status\n");
		return POWER_SUPPLY_USB_TYPE_UNKNOWN;
	}

	/*
	 * Fast charge dm current source calibration mode, select efuse calibration
	 * as default.
	 */
	ret = regmap_update_bits(info->regmap, pdata->ib_ctrl,
				 ANA_REG_IB_TRIM_EM_SEL_BIT,
				 ANA_REG_IB_TRIM_EM_SEL_BIT);
	if (ret) {
		dev_err(info->dev, "failed to select ib trim mode.\n");
		return ret;
	}

	return info->state;
}

/* Tab A8 code for SR-AX6300-01-5 by wenyaqi at 20210824 start */
#ifdef CONFIG_AFC
static void ss_get_afc_detect_status(struct sc2730_fchg_info *info)
{
	static struct power_supply *psy_bat = NULL;
	union power_supply_propval val;
	int ret = 0;
	int hv_disable = HV_ENABLE;

	dev_err(info->dev, "%s: chg_type=%d\n", __func__, info->usb_phy->chg_type);
	if (info->usb_phy->chg_type != DCP_TYPE)
		return;

	if (info->pd_enable || info->pps_enable || info->sfcp_enable) {
		dev_err(info->dev, "%s: already in fast charge, not in afc\n", __func__);
		return;
	}

	if (info == NULL) {
		dev_err(info->dev, "%s: Failed to get sc2730_fchg_info\n", __func__);
		return;
	}

	if (psy_bat == NULL) {
		psy_bat = power_supply_get_by_name("battery");
		if (!psy_bat) {
			dev_err(info->dev, "%s: Failed to get psy_bat\n", __func__);
			return;
		}
	}

	ret = power_supply_get_property(psy_bat, POWER_SUPPLY_PROP_HV_DISABLE, &val);
	if (ret) {
		dev_err(info->dev, "%s: failed to get hv_disable\n", __func__);
		return;
	} else {
		hv_disable = val.intval;
	}

	if (hv_disable == HV_ENABLE) {
		dev_info(info->dev, "%s: Start AFC!!!\n", __func__);
		afc_set_voltage(info, SET_9V);
	} else {
		dev_err(info->dev, "%s: not start AFC, hv_disable(%d), chg_type(%d)\n",
			__func__, hv_disable, info->usb_phy->chg_type);
	}
	return;
}
#endif
/* Tab A8 code for SR-AX6300-01-5 by wenyaqi at 20210824 end */

static void sc2730_fchg_disable(struct sc2730_fchg_info *info)
{
	const struct sc27xx_fast_chg_data *pdata = info->pdata;
	int ret;

	ret = regmap_update_bits(info->regmap, info->base + FCHG_CTRL,
				 FCHG_ENABLE_BIT, 0);
	if (ret)
		dev_err(info->dev, "failed to disable fast charger.\n");

	/*
	 * Adding delay is to make sure writing the the control register
	 * successfully firstly, then disable the module and clock.
	 */
	msleep(100);

	ret = regmap_update_bits(info->regmap, pdata->module_en,
				 FAST_CHARGE_MODULE_EN0_BIT, 0);
	if (ret)
		dev_err(info->dev, "failed to disable fast charger module.\n");

	ret = regmap_update_bits(info->regmap, pdata->clk_en,
				 FAST_CHARGE_RTC_CLK_EN0_BIT, 0);
	if (ret)
		dev_err(info->dev, "failed to disable charger clock.\n");
}

static int sc2730_fchg_sfcp_adjust_voltage(struct sc2730_fchg_info *info,
					   u32 input_vol)
{
	int ret, value;

	if (input_vol < FCHG_VOLTAGE_9V)
		value = 0;
	else if (input_vol < FCHG_VOLTAGE_12V)
		value = 1;
	else if (input_vol < FCHG_VOLTAGE_20V)
		value = 2;
	else
		value = 3;

	ret = regmap_update_bits(info->regmap, info->base + FCHG_CTRL,
				 FCHG_DET_VOL_MASK << FCHG_DET_VOL_SHIFT,
				 (value & FCHG_DET_VOL_MASK) << FCHG_DET_VOL_SHIFT);
	if (ret) {
		dev_err(info->dev,
			"failed to set fast charger detect voltage.\n");
		return ret;
	}

	return 0;
}

#ifdef CONFIG_TYPEC_TCPM
static int sc2730_get_pd_fixed_voltage_max(struct sc2730_fchg_info *info, u32 *max_vol)
{
	struct tcpm_port *port;
	int i, adptor_max_vbus = 0;

	if (!info->psy_tcpm) {
		dev_err(info->dev, "psy_tcpm is NULL !!!\n");
		return -EINVAL;
	}

	port = power_supply_get_drvdata(info->psy_tcpm);
	if (!port) {
		dev_err(info->dev, "failed to get tcpm port\n");
		return -EINVAL;
	}

	tcpm_get_source_capabilities(port, &info->pd_source_cap);
	if (!info->pd_source_cap.nr_source_caps) {
		dev_err(info->dev, "failed to obtain the PD power supply capacity\n");
		return -EINVAL;
	}

	for (i = 0; i < info->pd_source_cap.nr_source_caps; i++) {
		if (info->pd_source_cap.type[i] == PDO_TYPE_FIXED &&
		    adptor_max_vbus < info->pd_source_cap.max_mv[i])
			adptor_max_vbus = info->pd_source_cap.max_mv[i];
	}

	*max_vol = adptor_max_vbus * 1000;

	return 0;
}


static int sc2730_get_pps_voltage_max(struct sc2730_fchg_info *info, u32 *max_vol)
{
	union power_supply_propval val;
	int ret;

	if (!info->psy_tcpm) {
		dev_err(info->dev, "psy_tcpm is NULL !!!\n");
		return -EINVAL;
	}

	ret = power_supply_get_property(info->psy_tcpm,
					POWER_SUPPLY_PROP_VOLTAGE_MAX,
					&val);
	if (ret) {
		dev_err(info->dev, "failed to set online property\n");
		return ret;
	}

	*max_vol = val.intval;

	return ret;
}

static int sc2730_get_pps_current_max(struct sc2730_fchg_info *info, u32
				      *max_cur)
{
	union power_supply_propval val;
	int ret;

	if (!info->psy_tcpm) {
		dev_err(info->dev, "psy_tcpm is NULL !!!\n");
		return -EINVAL;
	}

	ret = power_supply_get_property(info->psy_tcpm,
					POWER_SUPPLY_PROP_CURRENT_MAX,
					&val);
	if (ret) {
		dev_err(info->dev, "failed to set online property\n");
		return ret;
	}

	*max_cur = val.intval;

	return ret;
}

static int sc2730_fchg_pd_adjust_voltage(struct sc2730_fchg_info *info,
					 u32 input_vol)
{
	struct tcpm_port *port;
	int ret, i, index = -1;
	u32 pdo[PDO_MAX_OBJECTS];
	unsigned int snk_uw;

	if (!info->psy_tcpm) {
		dev_err(info->dev, "psy_tcpm is NULL !!!\n");
		return -EINVAL;
	}

	port = power_supply_get_drvdata(info->psy_tcpm);
	if (!port) {
		dev_err(info->dev, "failed to get tcpm port\n");
		return -EINVAL;
	}

	tcpm_get_source_capabilities(port, &info->pd_source_cap);
	if (!info->pd_source_cap.nr_source_caps) {
		pdo[0] = PDO_FIXED(5000, 2000, 0);
		snk_uw = SC2730_PD_DEFAULT_POWER_UW;
		index = 0;
		goto done;
	}

	for (i = 0; i < info->pd_source_cap.nr_source_caps; i++) {
		if ((info->pd_source_cap.max_mv[i] <= input_vol / 1000) &&
		    (info->pd_source_cap.type[i] == PDO_TYPE_FIXED))
			index = i;
	}

	/*
	 * Ensure that index is within a valid range to prevent arrays
	 * from crossing bounds.
	 */
	if (index < 0 || index >= info->pd_source_cap.nr_source_caps) {
		dev_err(info->dev, "Index is invalid!!!\n");
		return -EINVAL;
	}

	snk_uw = info->pd_source_cap.max_mv[index] * info->pd_source_cap.ma[index];
	if (snk_uw > info->pd_fixed_max_uw)
		snk_uw = info->pd_fixed_max_uw;

	for (i = 0; i < index + 1; i++) {
		pdo[i] = PDO_FIXED(info->pd_source_cap.max_mv[i], info->pd_source_cap.ma[i], 0);
		if (info->pd_source_cap.max_mv[i] * info->pd_source_cap.ma[i] > snk_uw)
			pdo[i] = PDO_FIXED(info->pd_source_cap.max_mv[i],
					   snk_uw / info->pd_source_cap.max_mv[i],
					   0);
	}

done:
	ret = tcpm_update_sink_capabilities(port, pdo,
					    index + 1,
					    snk_uw / 1000);
	if (ret) {
		dev_err(info->dev, "failed to set pd, ret = %d\n", ret);
		return ret;
	}

	return 0;
}

static int sc2730_fchg_pps_adjust_voltage(struct sc2730_fchg_info *info,
					 u32 input_vol)
{
	union power_supply_propval val, vol;
	int ret;

	if (!info->psy_tcpm) {
		dev_err(info->dev, "psy_tcpm is NULL !!!\n");
		return -EINVAL;
	}

	if (!info->pps_active) {
		val.intval = SC2730_ENABLE_PPS;
		ret = power_supply_set_property(info->psy_tcpm, POWER_SUPPLY_PROP_ONLINE, &val);
		if (ret) {
			dev_err(info->dev, "failed to set online property ret = %d\n", ret);
			return ret;
		}
		info->pps_active = true;
	}

	vol.intval = input_vol;
	ret = power_supply_set_property(info->psy_tcpm, POWER_SUPPLY_PROP_VOLTAGE_NOW, &vol);
	if (ret) {
		dev_err(info->dev, "failed to set vol property\n");
		return ret;
	}

	return 0;
}

static int sc2730_fchg_pps_adjust_current(struct sc2730_fchg_info *info,
					 u32 input_current)
{
	union power_supply_propval val;
	int ret;

	if (!info->psy_tcpm) {
		dev_err(info->dev, "psy_tcpm is NULL !!!\n");
		return -EINVAL;
	}

	if (!info->pps_active) {
		val.intval = SC2730_ENABLE_PPS;
		ret = power_supply_set_property(info->psy_tcpm, POWER_SUPPLY_PROP_ONLINE, &val);
		if (ret) {
			dev_err(info->dev, "failed to set online property\n");
			return ret;
		}
		info->pps_active = true;
	}

	val.intval = input_current;
	ret = power_supply_set_property(info->psy_tcpm, POWER_SUPPLY_PROP_CURRENT_NOW, &val);
	if (ret) {
		dev_err(info->dev, "failed to set current property\n");
		return ret;
	}

	return 0;
}

static int sc2730_fchg_enable_pps(struct sc2730_fchg_info *info, bool enable)
{
	union power_supply_propval val;
	int ret;

	if (!info->psy_tcpm) {
		dev_err(info->dev, "psy_tcpm is NULL !!!\n");
		return -EINVAL;
	}

	if (info->pps_active && !enable) {
		val.intval = SC2730_DISABLE_PPS;
		ret = power_supply_set_property(info->psy_tcpm, POWER_SUPPLY_PROP_ONLINE, &val);
		if (ret) {
			dev_err(info->dev, "failed to disbale pps, ret = %d\n", ret);
			return ret;
		}
		info->pps_active = false;
	} else if (!info->pps_active && enable) {
		val.intval = SC2730_ENABLE_PPS;
		ret = power_supply_set_property(info->psy_tcpm, POWER_SUPPLY_PROP_ONLINE, &val);
		if (ret) {
			dev_err(info->dev, "failed to enable pps, ret = %d\n", ret);
			return ret;
		}
		info->pps_active = true;
	}

	return 0;
}

static int sc2730_fchg_pd_change(struct notifier_block *nb,
				 unsigned long event, void *data)
{
	struct sc2730_fchg_info *info =
		container_of(nb, struct sc2730_fchg_info, pd_notify);
	struct power_supply *psy = data;

	if (strcmp(psy->desc->name, "tcpm-source-psy-sc27xx-pd") != 0)
		goto out;

	if (event != PSY_EVENT_PROP_CHANGED)
		goto out;

	info->psy_tcpm = data;

	schedule_work(&info->pd_change_work);

out:
	return NOTIFY_OK;
}

static void sc2730_fchg_pd_change_work(struct work_struct *data)
{
	struct sc2730_fchg_info *info =
		container_of(data, struct sc2730_fchg_info, pd_change_work);
	union power_supply_propval val;
	struct tcpm_port *port;
	/* Tab A8 code for AX6300DEV-3976 by zhaichao at 20220118 start */
	int pd_type, ret;
	/* Tab A8 code for AX6300DEV-3976 by zhaichao at 20220118 end */

	mutex_lock(&info->lock);

	if (!info->psy_tcpm) {
		dev_err(info->dev, "psy_tcpm NULL !!!\n");
		goto out;
	}

	port = power_supply_get_drvdata(info->psy_tcpm);
	if (!port) {
		dev_err(info->dev, "failed to get tcpm port!\n");
		goto out;
	}

	ret = power_supply_get_property(info->psy_tcpm,
					POWER_SUPPLY_PROP_USB_TYPE,
					&val);
	if (ret) {
		dev_err(info->dev, "failed to get pd type\n");
		goto out;
	}

	/* Tab A8 code for AX6300DEV-3976 by zhaichao at 20220118 start */
	pd_type = val.intval;

	if (pd_type == POWER_SUPPLY_USB_TYPE_PD ||
           (!info->support_pd_pps && pd_type == POWER_SUPPLY_USB_TYPE_PD_PPS)) {
	/* Tab A8 code for AX6300DEV-3976 by zhaichao at 20220118 end */
		info->pd_enable = true;
		info->pps_enable = false;
		info->pps_active = false;
		info->state = POWER_SUPPLY_USB_TYPE_PD;
		mutex_unlock(&info->lock);
		cm_notify_event(info->psy_usb, CM_EVENT_FAST_CHARGE, NULL);
		goto out1;
	/* Tab A8 code for AX6300DEV-3976 by zhaichao at 20220118 start */
	} else if (pd_type == POWER_SUPPLY_USB_TYPE_PD_PPS) {
	/* Tab A8 code for AX6300DEV-3976 by zhaichao at 20220118 end */
		info->pps_enable = true;
		info->pd_enable = false;
		info->state = POWER_SUPPLY_USB_TYPE_PD_PPS;
		mutex_unlock(&info->lock);
		cm_notify_event(info->psy_usb, CM_EVENT_FAST_CHARGE, NULL);
		goto out1;
	} else if (pd_type == POWER_SUPPLY_USB_TYPE_C) {
		if (info->pd_enable)
			sc2730_fchg_pd_adjust_voltage(info, FCHG_VOLTAGE_5V);

		info->pd_enable = false;
		info->pps_enable = false;
		info->pps_active = false;
		if (info->state != POWER_SUPPLY_USB_TYPE_SFCP_1P0)
			info->state = POWER_SUPPLY_USB_TYPE_C;
	}

out:
	mutex_unlock(&info->lock);

out1:
	dev_info(info->dev, "pd type = %d\n", pd_type);
}
#else
static int sc2730_get_pd_fixed_voltage_max(struct sc2730_fchg_info *info, u32
				      *max_vol)
{
	return 0;
}

static int sc2730_get_pps_voltage_max(struct sc2730_fchg_info *info, u32
				      *max_vol)
{
	return 0;
}

static int sc2730_get_pps_current_max(struct sc2730_fchg_info *info, u32
				      *max_cur)
{
	return 0;
}

static int sc2730_fchg_pd_adjust_voltage(struct sc2730_fchg_info *info,
					 u32 input_vol)
{
	return 0;
}

static int sc2730_fchg_pps_adjust_voltage(struct sc2730_fchg_info *info,
					 u32 input_vol)
{
	return 0;
}

static int sc2730_fchg_pps_adjust_current(struct sc2730_fchg_info *info,
					 u32 input_current)
{
	return 0;
}

static int sc2730_fchg_enable_pps(struct sc2730_fchg_info *info, bool enable)
{
	return 0;
}
static int sc2730_fchg_pd_change(struct notifier_block *nb,
				 unsigned long event, void *data)
{
	return NOTIFY_OK;
}

static void sc2730_fchg_pd_change_work(struct work_struct *data)
{

}
#endif

static int sc2730_fchg_usb_get_property(struct power_supply *psy,
					enum power_supply_property psp,
					union power_supply_propval *val)
{
	struct sc2730_fchg_info *info = power_supply_get_drvdata(psy);
	int ret = 0;

	mutex_lock(&info->lock);

	switch (psp) {
	case POWER_SUPPLY_PROP_USB_TYPE:
		val->intval = info->state;
		break;
	case POWER_SUPPLY_PROP_VOLTAGE_MAX:
		val->intval = 0;
		if (info->pd_enable)
			sc2730_get_pd_fixed_voltage_max(info, &val->intval);
		else if (info->pps_enable)
			sc2730_get_pps_voltage_max(info, &val->intval);
		else if (info->sfcp_enable)
			val->intval = FCHG_VOLTAGE_9V;
		/* Tab A8_S code for AX6300SDEV-324 by wenyaqi at 20220613 start */
		#ifdef CONFIG_AFC
		else if (info->afc_enable)
			val->intval = AFC_VOLTAGE_9V;
		#endif
		/* Tab A8_S code for AX6300SDEV-324 by wenyaqi at 20220613 end */
		break;
	case POWER_SUPPLY_PROP_CURRENT_MAX:
		if (info->pps_enable)
			sc2730_get_pps_current_max(info, &val->intval);
		break;
	default:
		ret = -EINVAL;
	}

	mutex_unlock(&info->lock);
	return ret;
}

static int sc2730_fchg_usb_set_property(struct power_supply *psy,
					enum power_supply_property psp,
					const union power_supply_propval *val)
{
	struct sc2730_fchg_info *info = power_supply_get_drvdata(psy);
	int ret = 0;

	mutex_lock(&info->lock);

	switch (psp) {
	case POWER_SUPPLY_PROP_ONLINE:
		if (val->intval == CM_PPS_CHARGE_DISABLE_CMD) {
			if (sc2730_fchg_enable_pps(info, false)) {
				ret = -EINVAL;
				dev_err(info->dev, "failed to disable pps\n");
			}
			break;
		} else if (val->intval == CM_PPS_CHARGE_ENABLE_CMD) {
			if (sc2730_fchg_enable_pps(info, true)) {
				ret = -EINVAL;
				dev_err(info->dev, "failed to enable pps\n");
			}
			break;
		}

		break;
	case POWER_SUPPLY_PROP_VOLTAGE_MAX:
		if (info->pd_enable) {
			if (sc2730_fchg_enable_pps(info, false))
				dev_err(info->dev, "failed to disable pps\n");

			ret = sc2730_fchg_pd_adjust_voltage(info, val->intval);
			if (ret)
				dev_err(info->dev, "failed to adjust pd vol\n");
		} else if (info->pps_enable) {
			ret = sc2730_fchg_pps_adjust_voltage(info, val->intval);
			if (ret)
				dev_err(info->dev, "failed to adjust pd vol\n");
		} else if (info->sfcp_enable) {
			ret = sc2730_fchg_sfcp_adjust_voltage(info, val->intval);
			if (ret)
				dev_err(info->dev, "failed to adjust sfcp vol\n");
		}
		break;
	case POWER_SUPPLY_PROP_CURRENT_MAX:
		sc2730_fchg_pps_adjust_current(info, val->intval);
		break;
	/* Tab A8 code for SR-AX6300-01-5 by wenyaqi at 20210824 start */
	case POWER_SUPPLY_PROP_USB_TYPE:
		info->state = val->intval;
		break;
	/* Tab A8 code for SR-AX6300-01-5 by wenyaqi at 20210824 end */
	default:
		ret = -EINVAL;
	}

	mutex_unlock(&info->lock);
	return ret;
}

static int sc2730_fchg_property_is_writeable(struct power_supply *psy,
					     enum power_supply_property psp)
{
	int ret;

	switch (psp) {
	case POWER_SUPPLY_PROP_ONLINE:
	case POWER_SUPPLY_PROP_VOLTAGE_MAX:
	case POWER_SUPPLY_PROP_CURRENT_MAX:
	/* Tab A8 code for SR-AX6300-01-5 by wenyaqi at 20210824 start */
	case POWER_SUPPLY_PROP_USB_TYPE:
	/* Tab A8 code for SR-AX6300-01-5 by wenyaqi at 20210824 end */
		ret = 1;
		break;
	default:
		ret = 0;
	}

	return ret;
}

static enum power_supply_usb_type sc2730_fchg_usb_types[] = {
	POWER_SUPPLY_USB_TYPE_UNKNOWN,
	POWER_SUPPLY_USB_TYPE_C,
	POWER_SUPPLY_USB_TYPE_PD,
	POWER_SUPPLY_USB_TYPE_PD_PPS,
	POWER_SUPPLY_USB_TYPE_PD_DRP,
	POWER_SUPPLY_USB_TYPE_SFCP_1P0,
	POWER_SUPPLY_USB_TYPE_SFCP_2P0,
	/* Tab A8 code for SR-AX6300-01-5 by wenyaqi at 20210824 start */
	#ifdef CONFIG_AFC
	POWER_SUPPLY_USB_TYPE_AFC,
	#endif
	/* Tab A8 code for SR-AX6300-01-5 by wenyaqi at 20210824 end */
};

static enum power_supply_property sc2730_fchg_usb_props[] = {
	POWER_SUPPLY_PROP_VOLTAGE_MAX,
	POWER_SUPPLY_PROP_USB_TYPE,
};

static const struct power_supply_desc sc2730_fchg_desc = {
	.name			= "sc2730_fast_charger",
	.type			= POWER_SUPPLY_TYPE_USB,
	.properties		= sc2730_fchg_usb_props,
	.num_properties		= ARRAY_SIZE(sc2730_fchg_usb_props),
	.get_property		= sc2730_fchg_usb_get_property,
	.set_property		= sc2730_fchg_usb_set_property,
	.property_is_writeable	= sc2730_fchg_property_is_writeable,
	.usb_types              = sc2730_fchg_usb_types,
	.num_usb_types          = ARRAY_SIZE(sc2730_fchg_usb_types),
};

static void sc2730_fchg_work(struct work_struct *data)
{
	struct delayed_work *dwork = to_delayed_work(data);
	struct sc2730_fchg_info *info =
		container_of(dwork, struct sc2730_fchg_info, work);

	mutex_lock(&info->lock);
	if (!info->limit) {
		if (!info->pps_enable || info->state != POWER_SUPPLY_USB_TYPE_PD_PPS)
			info->state = POWER_SUPPLY_USB_TYPE_UNKNOWN;

		info->detected = false;
		info->sfcp_enable = false;
		/* Tab A8 code for SR-AX6300-01-5 by wenyaqi at 20210824 start */
		#ifdef CONFIG_AFC
		info->afc_enable = false;
		#endif
		/* Tab A8 code for SR-AX6300-01-5 by wenyaqi at 20210824 end */
		sc2730_fchg_disable(info);
	} else if (!info->detected) {
		info->detected = true;
		/* Tab A8 code for AX6300DEV-2595 by wenyaqi at 20211109 start */
		if (info->pd_enable || info->pps_enable || !info->support_sfcp) {
		/* Tab A8 code for AX6300DEV-2595 by wenyaqi at 20211109 end */
			sc2730_fchg_disable(info);
			/* Tab A8 code for SR-AX6300-01-5 by wenyaqi at 20210824 start */
			#ifdef CONFIG_AFC
			info->afc_enable = false;
			#endif
			/* Tab A8 code for SR-AX6300-01-5 by wenyaqi at 20210824 end */
		} else if (sc2730_fchg_get_detect_status(info) ==
		    POWER_SUPPLY_USB_TYPE_SFCP_1P0) {
			/*
			 * Must release info->lock before send fast charge event
			 * to charger manager, otherwise it will cause deadlock.
			 */
			info->sfcp_enable = true;
			mutex_unlock(&info->lock);
			cm_notify_event(info->psy_usb, CM_EVENT_FAST_CHARGE, NULL);
			dev_info(info->dev, "pd_enable = %d, sfcp_enable = %d\n",
				 info->pd_enable, info->sfcp_enable);
			return;
		} else {
			sc2730_fchg_disable(info);
		}
	}

	mutex_unlock(&info->lock);
	/* Tab A8 code for SR-AX6300-01-5 by wenyaqi at 20210824 start */
	#ifdef CONFIG_AFC
	ss_get_afc_detect_status(info);
	#endif
	/* Tab A8 code for SR-AX6300-01-5 by wenyaqi at 20210824 end */

	dev_info(info->dev, "pd_enable = %d, pps_enable = %d, sfcp_enable = %d\n",
		 info->pd_enable, info->pps_enable, info->sfcp_enable);
}

static int sc2730_fchg_probe(struct platform_device *pdev)
{
	struct device_node *np = pdev->dev.of_node;
	struct sc2730_fchg_info *info;
	struct power_supply_config charger_cfg = { };
	int irq, ret;

	info = devm_kzalloc(&pdev->dev, sizeof(*info), GFP_KERNEL);
	if (!info)
		return -ENOMEM;

	mutex_init(&info->lock);
	info->dev = &pdev->dev;
	info->state = POWER_SUPPLY_USB_TYPE_UNKNOWN;
	info->pdata = of_device_get_match_data(info->dev);
	if (!info->pdata) {
		dev_err(info->dev, "no matching driver data found\n");
		return -EINVAL;
	}

	INIT_DELAYED_WORK(&info->work, sc2730_fchg_work);
	INIT_WORK(&info->pd_change_work, sc2730_fchg_pd_change_work);
	init_completion(&info->completion);

	info->regmap = dev_get_regmap(pdev->dev.parent, NULL);
	if (!info->regmap) {
		dev_err(&pdev->dev, "failed to get charger regmap\n");
		return -ENODEV;
	}

	ret = of_property_read_u32(np, "reg", &info->base);
	if (ret) {
		dev_err(&pdev->dev, "failed to get register address\n");
		return -ENODEV;
	}

	ret = device_property_read_u32(&pdev->dev,
				       "sprd,input-voltage-microvolt",
				       &info->input_vol);
	if (ret) {
		dev_err(&pdev->dev, "failed to get fast charger voltage.\n");
		return ret;
	}

	ret = device_property_read_u32(&pdev->dev,
				       "sprd,pd-fixed-max-microwatt",
				       &info->pd_fixed_max_uw);
	if (ret) {
		dev_info(&pdev->dev, "failed to get pd fixed max uw.\n");
		/* If this parameter is not defined in DTS, the default power is 10W */
		info->pd_fixed_max_uw = SC2730_PD_DEFAULT_POWER_UW;
	}

	info->support_pd_pps = device_property_read_bool(&pdev->dev,
							 "sprd,support-pd-pps");
	/* Tab A8 code for AX6300DEV-2595 by wenyaqi at 20211109 start */
	info->support_sfcp = device_property_read_bool(&pdev->dev,
							 "sprd,support-sfcp");
	/* Tab A8 code for AX6300DEV-2595 by wenyaqi at 20211109 end */
	info->pps_active = false;
	platform_set_drvdata(pdev, info);

	info->usb_phy = devm_usb_get_phy_by_phandle(&pdev->dev, "phys", 0);
	if (IS_ERR(info->usb_phy)) {
		dev_err(&pdev->dev, "failed to find USB phy\n");
		return PTR_ERR(info->usb_phy);
	}

	info->usb_notify.notifier_call = sc2730_fchg_usb_change;
	ret = usb_register_notifier(info->usb_phy, &info->usb_notify);
	if (ret) {
		dev_err(&pdev->dev, "failed to register notifier:%d\n", ret);
		return ret;
	}

	irq = platform_get_irq(pdev, 0);
	if (irq < 0) {
		dev_err(&pdev->dev, "no irq resource specified\n");
		usb_unregister_notifier(info->usb_phy, &info->usb_notify);
		return irq;
	}
	ret = devm_request_threaded_irq(info->dev, irq, NULL,
					sc2730_fchg_interrupt,
					IRQF_NO_SUSPEND | IRQF_ONESHOT,
					pdev->name, info);
	if (ret) {
		dev_err(&pdev->dev, "failed to request irq.\n");
		usb_unregister_notifier(info->usb_phy, &info->usb_notify);
		return ret;
	}

	info->pd_notify.notifier_call = sc2730_fchg_pd_change;
	ret = power_supply_reg_notifier(&info->pd_notify);
	if (ret) {
		dev_err(info->dev, "failed to register pd notifier:%d\n", ret);
		usb_unregister_notifier(info->usb_phy, &info->usb_notify);
		return ret;
	}

	charger_cfg.drv_data = info;
	charger_cfg.of_node = np;

	info->psy_usb = devm_power_supply_register(&pdev->dev,
						   &sc2730_fchg_desc,
						   &charger_cfg);
	if (IS_ERR(info->psy_usb)) {
		dev_err(&pdev->dev, "failed to register power supply\n");
		usb_unregister_notifier(info->usb_phy, &info->usb_notify);
		power_supply_unreg_notifier(&info->pd_notify);
		return PTR_ERR(info->psy_usb);
	}

	sc2730_fchg_detect_status(info);

	return 0;
}

static int sc2730_fchg_remove(struct platform_device *pdev)
{
	struct sc2730_fchg_info *info = platform_get_drvdata(pdev);

	usb_unregister_notifier(info->usb_phy, &info->usb_notify);

	return 0;
}

static void sc2730_fchg_shutdown(struct platform_device *pdev)
{
	struct sc2730_fchg_info *info = platform_get_drvdata(pdev);
	int ret;
	u32 value = FCHG_DET_VOL_EXIT_SFCP;

	/*
	 * SFCP will handsharke failed from charging in shut down
	 * to charging in power up, because SFCP is not exit before
	 * shut down. Set bit3:4 to 2b'11 to exit SFCP.
	 */

	ret = regmap_update_bits(info->regmap, info->base + FCHG_CTRL,
				 FCHG_DET_VOL_MASK << FCHG_DET_VOL_SHIFT,
				 (value & FCHG_DET_VOL_MASK) << FCHG_DET_VOL_SHIFT);
	if (ret)
		dev_err(info->dev,
			"failed to set fast charger detect voltage.\n");
}

static const struct of_device_id sc2730_fchg_of_match[] = {
	{ .compatible = "sprd,sc2730-fast-charger", .data = &sc2730_info },
	{ .compatible = "sprd,ump9620-fast-chg", .data = &ump9620_info },
	{ .compatible = "sprd,sc2721-fast-charger", .data = &sc2721_info },
	{ }
};

static struct platform_driver sc2730_fchg_driver = {
	.driver = {
		.name = "sc2730-fast-charger",
		.of_match_table = sc2730_fchg_of_match,
	},
	.probe = sc2730_fchg_probe,
	.remove = sc2730_fchg_remove,
	.shutdown = sc2730_fchg_shutdown,
};

module_platform_driver(sc2730_fchg_driver);

MODULE_DESCRIPTION("Spreadtrum SC2730 Fast Charger Driver");
MODULE_LICENSE("GPL v2");
