// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2020 Spreadtrum Communications Inc.
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#include <linux/delay.h>
#include <linux/device.h>
#include <linux/io.h>
/* HS03 code for P211026-03673 by gaochao at 20211028 start */
#include <linux/iio/consumer.h>
/* HS03 code for P211026-03673 by gaochao at 20211028 end */
#include <linux/mfd/syscon.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/of_platform.h>
#include <linux/platform_device.h>
#include <linux/regmap.h>
#include <linux/regulator/consumer.h>
#include <linux/slab.h>
#include <linux/soc/sprd/sprd_usbpinmux.h>
#include <linux/usb/phy.h>
#include <linux/usb/otg.h>
#include <uapi/linux/usb/charger.h>
#include <dt-bindings/soc/sprd,qogirl6-mask.h>
#include <dt-bindings/soc/sprd,qogirl6-regs.h>

#define SC2730_CHARGE_STATUS		0x1b9c
#define BIT_CHG_DET_DONE		BIT(11)
#define BIT_SDP_INT			BIT(7)
#define BIT_DCP_INT			BIT(6)
#define BIT_CDP_INT			BIT(5)
/* HS03 code for SR-SL6215-01-545 by shixuanxuan at 20210818 start */
#if !defined(HQ_FACTORY_BUILD)
#define BIT_CHGR_INT		BIT(2)
#endif
/* HS03 code forSR-SL6215-01-545 by shixuanxuan at 20210818 end */

/*analog2 defined*/
#define REG_ANLG_PHY_G2_ANALOG_USB20_USB20_BATTER_PLL			0x0008
#define REG_ANLG_PHY_G2_ANALOG_USB20_USB20_UTMI_CTL2			0x000c
#define REG_ANLG_PHY_G2_ANALOG_USB20_USB20_ISO_SW			0x001c
#define REG_ANLG_PHY_G2_ANALOG_USB20_USB20_UTMI_CTL1                    0x0004
#define REG_ANLG_PHY_G2_ANALOG_USB20_USB20_TRIMMING                     0x0010
#define REG_ANLG_PHY_G2_ANALOG_USB20_REG_SEL_CFG_0			0x0020

/*mask defined*/
#define MASK_ANLG_PHY_G2_ANALOG_USB20_USB20_PS_PD_S                     0x10
#define MASK_ANLG_PHY_G2_ANALOG_USB20_USB20_PS_PD_L                     0x8
#define MASK_ANLG_PHY_G2_ANALOG_USB20_USB20_ISO_SW_EN                   0x1
#define MASK_ANLG_PHY_G2_ANALOG_USB20_USB20_VBUSVLDEXT                  0x10000
#define MASK_ANLG_PHY_G2_ANALOG_USB20_USB20_DATABUS16_8                 0x10000000
#define MASK_ANLG_PHY_G2_ANALOG_USB20_USB20_TUNEHSAMP                   0x6000000
#define MASK_ANLG_PHY_G2_ANALOG_USB20_USB20_TFREGRES                    0x1f80000
#define MASK_ANLG_PHY_G2_DBG_SEL_ANALOG_USB20_USB20_DPPULLDOWN          0x4
#define MASK_ANLG_PHY_G2_DBG_SEL_ANALOG_USB20_USB20_DMPULLDOWN          0x2
#define MASK_ANLG_PHY_G2_ANALOG_USB20_USB20_DPPULLDOWN                  0x10
#define MASK_ANLG_PHY_G2_ANALOG_USB20_USB20_DMPULLDOWN                  0x8
#define MASK_ANLG_PHY_G2_ANALOG_USB20_USB20_RESERVED                    0xffff

#define TUNEHSAMP_2_6MA			(3 << 25)
#define TFREGRES_TUNE_VALUE		(0xe << 19)
#define DEFAULT_HOST_EYE_PATTERN					0x04f3d1c0
#define DEFAULT_DEVICE_EYE_PATTERN					0x04f3d1c0
/* HS03 code for P211026-03673 by gaochao at 20211028 start */
#define SC2730_CHARGE_DET_FGU_CTRL	0x3A0
#define SC2730_ADC_OFFSET		0x1800
#define BIT_DP_DM_AUX_EN		BIT(1)
#define BIT_DP_DM_BC_ENB		BIT(0)
#define VOLT_LO_LIMIT			1200
#define VOLT_HI_LIMIT			600
/* HS03 code for P211026-03673 by gaochao at 20211028 end */

struct sprd_hsphy {
	struct device		*dev;
	struct usb_phy		phy;
	struct regulator	*vdd;
	struct regmap           *hsphy_glb;
	struct regmap           *ana_g2;
	struct regmap           *pmic;
	u32			vdd_vol;
	u32			host_eye_pattern;
	u32			device_eye_pattern;
	atomic_t		reset;
	atomic_t		inited;
	bool			is_host;
	/* HS03 code for P211026-03673 by gaochao at 20211028 start */
	struct iio_channel	*dp;
	struct iio_channel	*dm;
	/* HS03 code for P211026-03673 by gaochao at 20211028 end */
};

/* HS03_s code(Unisoc Patch) for SSL6215DEV-6 by lina at 20220228 start */
#define FULLSPEED_USB33_TUNE		3300000
/* HS03_s code(Unisoc Patch) for SSL6215DEV-6 by lina at 20220228 end */

static int boot_cali;
static __init int sprd_hsphy_cali_mode(char *str)
{
	if (strcmp(str, "cali"))
		boot_cali = 0;
	else
		boot_cali = 1;

	return 0;
}
__setup("androidboot.mode=", sprd_hsphy_cali_mode);

/* HS03 code for SR-SL6215-01-545 by shixuanxuan at 20210818 start */
#if !defined(HQ_FACTORY_BUILD)
static enum usb_charger_type sc27xx_charger_detect(struct regmap *regmap)
{
	enum usb_charger_type type;
	u32 status = 0, val;
	int ret, cnt = 10;

	do {
		ret = regmap_read(regmap, SC2730_CHARGE_STATUS, &val);
		if (ret)
		{
			return UNKNOWN_TYPE;
		}
		if (val & BIT_CHG_DET_DONE) {
			status = val & (BIT_CDP_INT | BIT_DCP_INT | BIT_SDP_INT | BIT_CHGR_INT);
			break;
		} else {
		}

		msleep(200);
	} while (--cnt > 0);

	switch (status) {
	case (BIT_CDP_INT | BIT_CHGR_INT):
		type = CDP_TYPE;
		break;
	case (BIT_DCP_INT | BIT_CHGR_INT):
		type = DCP_TYPE;
		break;
	case (BIT_SDP_INT | BIT_CHGR_INT):
		type = SDP_TYPE;
		break;
	case BIT_CHGR_INT:
		type = FLOAT_TYPE;
		break;
	default:
		type = UNKNOWN_TYPE;
	}

	return type;
}
#else
static enum usb_charger_type sc27xx_charger_detect(struct regmap *regmap)
{
	enum usb_charger_type type;
	u32 status = 0, val;
	int ret, cnt = 10;

	do {
		ret = regmap_read(regmap, SC2730_CHARGE_STATUS, &val);
		if (ret)
			return UNKNOWN_TYPE;

		if (val & BIT_CHG_DET_DONE) {
			status = val & (BIT_CDP_INT | BIT_DCP_INT | BIT_SDP_INT);
			break;
		}

		msleep(200);
	} while (--cnt > 0);

	switch (status) {
	case BIT_CDP_INT:
		type = CDP_TYPE;
		break;
	case BIT_DCP_INT:
		type = DCP_TYPE;
		break;
	case BIT_SDP_INT:
		type = SDP_TYPE;
		break;
	default:
		type = UNKNOWN_TYPE;
	}

	return type;
}
#endif
/* HS03 code forSR-SL6215-01-545 by shixuanxuan at 20210818 end */

static inline void sprd_hsphy_reset_core(struct sprd_hsphy *phy)
{
	u32 reg, msk;

	/* Reset PHY */
	reg = msk = MASK_AON_APB_OTG_PHY_SOFT_RST |
				MASK_AON_APB_OTG_UTMI_SOFT_RST;

	regmap_update_bits(phy->hsphy_glb, REG_AON_APB_APB_RST1,
		msk, reg);

	/* USB PHY reset need to delay 20ms~30ms */
	usleep_range(20000, 30000);
	regmap_update_bits(phy->hsphy_glb, REG_AON_APB_APB_RST1,
		msk, 0);
}

static int sprd_hsphy_reset(struct usb_phy *x)
{
	struct sprd_hsphy *phy = container_of(x, struct sprd_hsphy, phy);

	sprd_hsphy_reset_core(phy);
	return 0;
}

static int sprd_hostphy_set(struct usb_phy *x, int on)
{
	struct sprd_hsphy *phy = container_of(x, struct sprd_hsphy, phy);
	u32 reg, msk;
	int ret = 0;

	if (on) {
		reg = phy->host_eye_pattern;
		regmap_write(phy->ana_g2,
			REG_ANLG_PHY_G2_ANALOG_USB20_USB20_TRIMMING, reg);

		msk = MASK_AON_APB_USB2_PHY_IDDIG;
		ret |= regmap_update_bits(phy->hsphy_glb,
			REG_AON_APB_OTG_PHY_CTRL, msk, 0);

		msk = MASK_ANLG_PHY_G2_DBG_SEL_ANALOG_USB20_USB20_DMPULLDOWN |
			MASK_ANLG_PHY_G2_DBG_SEL_ANALOG_USB20_USB20_DPPULLDOWN;
		ret |= regmap_update_bits(phy->ana_g2,
			REG_ANLG_PHY_G2_ANALOG_USB20_REG_SEL_CFG_0,
			msk, msk);

		msk = MASK_ANLG_PHY_G2_ANALOG_USB20_USB20_DMPULLDOWN |
			MASK_ANLG_PHY_G2_ANALOG_USB20_USB20_DPPULLDOWN;
		ret |= regmap_update_bits(phy->ana_g2,
			REG_ANLG_PHY_G2_ANALOG_USB20_USB20_UTMI_CTL2,
			msk, msk);

		reg = 0x200;
		msk = MASK_ANLG_PHY_G2_ANALOG_USB20_USB20_RESERVED;
		ret |= regmap_update_bits(phy->ana_g2,
			REG_ANLG_PHY_G2_ANALOG_USB20_USB20_UTMI_CTL1,
			msk, reg);
		phy->is_host = true;
	} else {
		reg = phy->device_eye_pattern;
		regmap_write(phy->ana_g2,
			REG_ANLG_PHY_G2_ANALOG_USB20_USB20_TRIMMING, reg);

		reg = msk = MASK_AON_APB_USB2_PHY_IDDIG;
		ret |= regmap_update_bits(phy->hsphy_glb,
			REG_AON_APB_OTG_PHY_CTRL, msk, reg);

		msk = MASK_ANLG_PHY_G2_DBG_SEL_ANALOG_USB20_USB20_DMPULLDOWN |
			MASK_ANLG_PHY_G2_DBG_SEL_ANALOG_USB20_USB20_DPPULLDOWN;
		ret |= regmap_update_bits(phy->ana_g2,
			REG_ANLG_PHY_G2_ANALOG_USB20_REG_SEL_CFG_0,
			msk, msk);

		msk = MASK_ANLG_PHY_G2_ANALOG_USB20_USB20_DMPULLDOWN |
			MASK_ANLG_PHY_G2_ANALOG_USB20_USB20_DPPULLDOWN;
		ret |= regmap_update_bits(phy->ana_g2,
			REG_ANLG_PHY_G2_ANALOG_USB20_USB20_UTMI_CTL2,
			msk, 0);

		msk = MASK_ANLG_PHY_G2_ANALOG_USB20_USB20_RESERVED;
		ret |= regmap_update_bits(phy->ana_g2,
			REG_ANLG_PHY_G2_ANALOG_USB20_USB20_UTMI_CTL1,
			msk, 0);
		phy->is_host = false;
	}

	return ret;
}

static void sprd_hsphy_emphasis_set(struct usb_phy *x, bool enabled)
{
}

static int sprd_hsphy_init(struct usb_phy *x)
{
	struct sprd_hsphy *phy = container_of(x, struct sprd_hsphy, phy);
	u32 reg, msk;
	int ret = 0;

	if (atomic_read(&phy->inited)) {
		dev_dbg(x->dev, "%s is already inited!\n", __func__);
		return 0;
	}

	/* Turn On VDD */
	regulator_set_voltage(phy->vdd, phy->vdd_vol, phy->vdd_vol);
	if (!regulator_is_enabled(phy->vdd)) {
		ret = regulator_enable(phy->vdd);
		if (ret)
			return ret;
	}

	/* usb enable */
	reg = msk = MASK_AON_APB_OTG_UTMI_EB;
	ret |= regmap_update_bits(phy->hsphy_glb,
		REG_AON_APB_APB_EB1, msk, reg);

	reg = msk = MASK_AON_APB_CGM_OTG_REF_EN |
		MASK_AON_APB_CGM_DPHY_REF_EN;
	ret |= regmap_update_bits(phy->hsphy_glb,
		REG_AON_APB_CGM_REG1, msk, reg);

	ret |= regmap_update_bits(phy->ana_g2,
		REG_ANLG_PHY_G2_ANALOG_USB20_USB20_ISO_SW,
		MASK_ANLG_PHY_G2_ANALOG_USB20_USB20_ISO_SW_EN, 0);

	/* usb phy power */
	msk = (MASK_ANLG_PHY_G2_ANALOG_USB20_USB20_PS_PD_L |
		MASK_ANLG_PHY_G2_ANALOG_USB20_USB20_PS_PD_S);
	ret |= regmap_update_bits(phy->ana_g2,
		REG_ANLG_PHY_G2_ANALOG_USB20_USB20_BATTER_PLL, msk, 0);

	/* usb vbus valid */
	reg = msk = MASK_AON_APB_OTG_VBUS_VALID_PHYREG;
	ret |= regmap_update_bits(phy->hsphy_glb,
		REG_AON_APB_OTG_PHY_TEST, msk, reg);

	reg = msk = MASK_ANLG_PHY_G2_ANALOG_USB20_USB20_VBUSVLDEXT;
	ret |= regmap_update_bits(phy->ana_g2,
		REG_ANLG_PHY_G2_ANALOG_USB20_USB20_UTMI_CTL1,	msk, reg);

	/* for SPRD phy utmi_width sel */
	reg = msk = MASK_AON_APB_UTMI_WIDTH_SEL;
	ret |= regmap_update_bits(phy->hsphy_glb,
		REG_AON_APB_OTG_PHY_CTRL, msk, reg);

	reg = msk = MASK_ANLG_PHY_G2_ANALOG_USB20_USB20_DATABUS16_8;
	ret |= regmap_update_bits(phy->ana_g2,
		REG_ANLG_PHY_G2_ANALOG_USB20_USB20_UTMI_CTL1,
		msk, reg);

	reg = phy->device_eye_pattern;
	regmap_write(phy->ana_g2,
		REG_ANLG_PHY_G2_ANALOG_USB20_USB20_TRIMMING, reg);

	if (!atomic_read(&phy->reset)) {
		sprd_hsphy_reset_core(phy);
		atomic_set(&phy->reset, 1);
	}

	atomic_set(&phy->inited, 1);

	return ret;
}

static void sprd_hsphy_shutdown(struct usb_phy *x)
{
	struct sprd_hsphy *phy = container_of(x, struct sprd_hsphy, phy);
	u32 reg, msk;

	if (!atomic_read(&phy->inited)) {
		dev_dbg(x->dev, "%s is already shut down\n", __func__);
		return;
	}

	/* usb vbus */
	msk = MASK_AON_APB_OTG_VBUS_VALID_PHYREG;
	regmap_update_bits(phy->hsphy_glb, REG_AON_APB_OTG_PHY_TEST, msk, 0);
	msk = MASK_ANLG_PHY_G2_ANALOG_USB20_USB20_VBUSVLDEXT;
	regmap_update_bits(phy->ana_g2,
		REG_ANLG_PHY_G2_ANALOG_USB20_USB20_UTMI_CTL1, msk, 0);

	/* usb power down */
	reg = msk = (MASK_ANLG_PHY_G2_ANALOG_USB20_USB20_PS_PD_L |
		MASK_ANLG_PHY_G2_ANALOG_USB20_USB20_PS_PD_S);
	regmap_update_bits(phy->ana_g2,
		REG_ANLG_PHY_G2_ANALOG_USB20_USB20_BATTER_PLL, msk, reg);
	reg = msk = MASK_ANLG_PHY_G2_ANALOG_USB20_USB20_ISO_SW_EN;
	regmap_update_bits(phy->ana_g2,
		REG_ANLG_PHY_G2_ANALOG_USB20_USB20_ISO_SW,
		msk, reg);

	/* usb cgm ref */
	msk = MASK_AON_APB_CGM_OTG_REF_EN |
		MASK_AON_APB_CGM_DPHY_REF_EN;
	regmap_update_bits(phy->hsphy_glb, REG_AON_APB_CGM_REG1, msk, 0);

	if (regulator_is_enabled(phy->vdd))
		regulator_disable(phy->vdd);

	atomic_set(&phy->inited, 0);
	atomic_set(&phy->reset, 0);
}

static int sprd_hsphy_post_init(struct usb_phy *x)
{
	struct sprd_hsphy *phy = container_of(x, struct sprd_hsphy, phy);

	if (regulator_is_enabled(phy->vdd))
		regulator_disable(phy->vdd);

	return 0;
}

static ssize_t vdd_voltage_show(struct device *dev,
				struct device_attribute *attr,
				char *buf)
{
	struct sprd_hsphy *x = dev_get_drvdata(dev);

	if (!x)
		return -EINVAL;

	return sprintf(buf, "%d\n", x->vdd_vol);
}

static ssize_t vdd_voltage_store(struct device *dev,
				 struct device_attribute *attr,
				 const char *buf, size_t size)
{
	struct sprd_hsphy *x = dev_get_drvdata(dev);
	u32 vol;

	if (!x)
		return -EINVAL;

	if (kstrtouint(buf, 16, &vol) < 0)
		return -EINVAL;

	if (vol < 1200000 || vol > 3750000) {
		dev_err(dev, "Invalid voltage value %d\n", vol);
		return -EINVAL;
	}
	x->vdd_vol = vol;

	return size;
}
static DEVICE_ATTR_RW(vdd_voltage);

static ssize_t hsphy_device_eye_pattern_show(struct device *dev,
			struct device_attribute *attr,
			char *buf)
{
	struct sprd_hsphy *x = dev_get_drvdata(dev);

	if (!x)
		return -EINVAL;


	return sprintf(buf, "0x%x\n", x->device_eye_pattern);
}

static ssize_t hsphy_device_eye_pattern_store(struct device *dev,
				struct device_attribute *attr,
				const char *buf, size_t size)
{
	struct sprd_hsphy *x = dev_get_drvdata(dev);

	if (!x)
		return -EINVAL;

	if (kstrtouint(buf, 16, &x->device_eye_pattern) < 0)
		return -EINVAL;

	return size;
}
static DEVICE_ATTR_RW(hsphy_device_eye_pattern);

static ssize_t hsphy_host_eye_pattern_show(struct device *dev,
			struct device_attribute *attr,
			char *buf)
{
	struct sprd_hsphy *x = dev_get_drvdata(dev);

	if (!x)
		return -EINVAL;


	return sprintf(buf, "0x%x\n", x->host_eye_pattern);
}

static ssize_t hsphy_host_eye_pattern_store(struct device *dev,
				struct device_attribute *attr,
				const char *buf, size_t size)
{
	struct sprd_hsphy *x = dev_get_drvdata(dev);

	if (!x)
		return -EINVAL;

	if (kstrtouint(buf, 16, &x->host_eye_pattern) < 0)
		return -EINVAL;

	return size;
}

static DEVICE_ATTR_RW(hsphy_host_eye_pattern);

static struct attribute *usb_hsphy_attrs[] = {
	&dev_attr_vdd_voltage.attr,
	&dev_attr_hsphy_device_eye_pattern.attr,
	&dev_attr_hsphy_host_eye_pattern.attr,
	NULL
};
ATTRIBUTE_GROUPS(usb_hsphy);

static int sprd_hsphy_vbus_notify(struct notifier_block *nb,
				  unsigned long event, void *data)
{
	struct usb_phy *usb_phy = container_of(nb, struct usb_phy, vbus_nb);
	struct sprd_hsphy *phy = container_of(usb_phy, struct sprd_hsphy, phy);
	u32 reg, msk;
	u32 ret = 0;

	if (phy->is_host || usb_phy->last_event == USB_EVENT_ID) {
		dev_info(phy->dev, "USB PHY is host mode\n");
		return 0;
	}

	if (event) {
		/* usb vbus valid */
		reg = msk = MASK_AON_APB_OTG_VBUS_VALID_PHYREG;
		regmap_update_bits(phy->hsphy_glb, REG_AON_APB_OTG_PHY_TEST, msk, reg);

		reg = msk = MASK_ANLG_PHY_G2_ANALOG_USB20_USB20_VBUSVLDEXT;
		regmap_update_bits(phy->ana_g2,
			REG_ANLG_PHY_G2_ANALOG_USB20_USB20_UTMI_CTL1, msk, reg);

		reg = msk = MASK_ANLG_PHY_G2_ANALOG_USB20_USB20_DATABUS16_8;
		ret |= regmap_update_bits(phy->ana_g2,
			REG_ANLG_PHY_G2_ANALOG_USB20_USB20_UTMI_CTL1,
			msk, reg);

		usb_phy_set_charger_state(usb_phy, USB_CHARGER_PRESENT);
	} else {
		/* usb vbus invalid */
		msk = MASK_AON_APB_OTG_VBUS_VALID_PHYREG;
		regmap_update_bits(phy->hsphy_glb, REG_AON_APB_OTG_PHY_TEST, msk, 0);

		msk = MASK_ANLG_PHY_G2_ANALOG_USB20_USB20_VBUSVLDEXT;
		regmap_update_bits(phy->ana_g2,
			REG_ANLG_PHY_G2_ANALOG_USB20_USB20_UTMI_CTL1, msk, 0);

		usb_phy_set_charger_state(usb_phy, USB_CHARGER_ABSENT);
	}

	return 0;
}

static enum usb_charger_type sprd_hsphy_charger_detect(struct usb_phy *x)
{
	struct sprd_hsphy *phy = container_of(x, struct sprd_hsphy, phy);

	return sc27xx_charger_detect(phy->pmic);
}

/* HS03 code for P211026-03673 by gaochao at 20211028 start */
static struct device *second_detect_dev = NULL;

static int sc2730_voltage_cali(int voltage)
{
	return voltage * 3 / 2;
}

static enum usb_charger_type sprd_hsphy_retry_charger_detect(struct usb_phy *x)
{
	struct sprd_hsphy *phy = container_of(x, struct sprd_hsphy, phy);
	enum usb_charger_type type = UNKNOWN_TYPE;
	int dm_voltage;
	int dp_voltage;
	int cnt = 20;

	/* Tab A8 code for AX6300DEV-1831 by zhaichao at 20211029 start */
	if (!phy->dm || !phy->dp) {
		dev_info(x->dev, "iio resource is not ready, try again\n");
		phy->dp = devm_iio_channel_get(x->dev, "dp");
		phy->dm = devm_iio_channel_get(x->dev, "dm");
		if (!phy->dm || !phy->dp) {
			dev_err(x->dev, "phy->dp:%p, phy->dm:%p\n", phy->dp, phy->dm);
			return UNKNOWN_TYPE;
		}
	}
	/* Tab A8 code for AX6300DEV-1831 by zhaichao at 20211029 end */

	regmap_update_bits(phy->pmic,
		SC2730_ADC_OFFSET | SC2730_CHARGE_DET_FGU_CTRL,
		BIT_DP_DM_AUX_EN | BIT_DP_DM_BC_ENB,
		BIT_DP_DM_AUX_EN);

	msleep(300);
	iio_read_channel_processed(phy->dp, &dp_voltage);
	dp_voltage = sc2730_voltage_cali(dp_voltage);
	if (dp_voltage > VOLT_LO_LIMIT) {
		do {
			iio_read_channel_processed(phy->dm, &dm_voltage);
			dm_voltage = sc2730_voltage_cali(dm_voltage);
			if (dm_voltage > VOLT_LO_LIMIT) {
				type = DCP_TYPE;
				break;
			}
			msleep(100);
			cnt--;
			iio_read_channel_processed(phy->dp, &dp_voltage);
			dp_voltage = sc2730_voltage_cali(dp_voltage);
			if (dp_voltage  < VOLT_HI_LIMIT) {
				type = SDP_TYPE;
				break;
			}
		} while ((x->chg_state == USB_CHARGER_PRESENT) && cnt > 0);
	}

	regmap_update_bits(phy->pmic,
		SC2730_ADC_OFFSET | SC2730_CHARGE_DET_FGU_CTRL,
		BIT_DP_DM_AUX_EN | BIT_DP_DM_BC_ENB, 0);

	dev_info(x->dev, "correct type is %x\n", type);
	if (type != UNKNOWN_TYPE) {
		x->chg_type = type;
		schedule_work(&x->chg_work);
	}
	return type;
}
/* HS03 code for P211026-03673 by gaochao at 20211028 end */

static int sprd_hsphy_probe(struct platform_device *pdev)
{
	struct device_node *regmap_np;
	struct platform_device *regmap_pdev;
	struct sprd_hsphy *phy;
	struct device *dev = &pdev->dev;
	int ret;
	u32 reg, msk;
	struct usb_otg *otg;

	/* HS03 code for P211026-03673 by gaochao at 20211028 start */
	second_detect_dev = dev;
	/* HS03 code for P211026-03673 by gaochao at 20211028 end */
	phy = devm_kzalloc(dev, sizeof(*phy), GFP_KERNEL);
	if (!phy)
		return -ENOMEM;

	regmap_np = of_find_compatible_node(NULL, NULL, "sprd,sc27xx-syscon");
	if (!regmap_np) {
		dev_err(dev, "unable to get syscon node\n");
		return -ENODEV;
	}

	regmap_pdev = of_find_device_by_node(regmap_np);
	if (!regmap_pdev) {
		of_node_put(regmap_np);
		dev_err(dev, "unable to get syscon platform device\n");
		return -ENODEV;
	}

	phy->pmic = dev_get_regmap(regmap_pdev->dev.parent, NULL);
	if (!phy->pmic) {
		dev_err(dev, "unable to get pmic regmap device\n");
		return -ENODEV;
	}

	ret = of_property_read_u32(dev->of_node, "sprd,vdd-voltage",
				   &phy->vdd_vol);
	if (ret < 0) {
		dev_err(dev, "unable to read ssphy vdd voltage\n");
		return ret;
	}

	if (boot_cali) {
		phy->vdd_vol = FULLSPEED_USB33_TUNE;
		dev_info(dev, "calimode vdd_vol:%d\n", phy->vdd_vol);
	}

	phy->vdd = devm_regulator_get(dev, "vdd");
	if (IS_ERR(phy->vdd)) {
		dev_err(dev, "unable to get ssphy vdd supply\n");
		return PTR_ERR(phy->vdd);
	}

	ret = regulator_set_voltage(phy->vdd, phy->vdd_vol, phy->vdd_vol);
	if (ret < 0) {
		dev_err(dev, "fail to set ssphy vdd voltage at %dmV\n",
			phy->vdd_vol);
		return ret;
	}

	otg = devm_kzalloc(&pdev->dev, sizeof(*otg), GFP_KERNEL);
	if (!otg)
		return -ENOMEM;

	phy->ana_g2 = syscon_regmap_lookup_by_phandle(dev->of_node,
				 "sprd,syscon-anag2");
	if (IS_ERR(phy->ana_g2)) {
		dev_err(&pdev->dev, "ap USB anag2 syscon failed!\n");
		return PTR_ERR(phy->ana_g2);
	}

	phy->hsphy_glb = syscon_regmap_lookup_by_phandle(dev->of_node,
				 "sprd,syscon-enable");
	if (IS_ERR(phy->hsphy_glb)) {
		dev_err(&pdev->dev, "ap USB aon apb syscon failed!\n");
		return PTR_ERR(phy->hsphy_glb);
	}
	/* HS03 code for SL6215DEV-36 by shixuanxuan at 20210810 start */
	ret = of_property_read_u32(dev->of_node, "sprd,hsphy-device-eye-pattern",
					&phy->device_eye_pattern);
	/* HS03 code for SL6215DEV-36 by shixuanxuan at 20210810 end */
	if (ret < 0) {
		dev_err(dev, "unable to get hsphy-device-eye-pattern node\n");
		phy->device_eye_pattern = DEFAULT_DEVICE_EYE_PATTERN;
	}

	/* HS03 code for SL6215DEV-36 by shixuanxuan at 20210810 start */
	ret = of_property_read_u32(dev->of_node, "sprd,hsphy-host-eye-pattern",
					&phy->host_eye_pattern);
	/* HS03 code for SL6215DEV-36 by shixuanxuan at 20210810 end */
	if (ret < 0) {
		dev_err(dev, "unable to get hsphy-host-eye-pattern node\n");
		phy->host_eye_pattern = DEFAULT_HOST_EYE_PATTERN;
	}

	/* HS03 code for P211026-03673 by gaochao at 20211028 start */
	phy->dp = devm_iio_channel_get(dev, "dp");
	phy->dm = devm_iio_channel_get(dev, "dm");
	if (IS_ERR(phy->dp)) {
		phy->dp = NULL;
		dev_warn(dev, "failed to get dp or dm channel\n");
	}
	if (IS_ERR(phy->dm)) {
		phy->dm = NULL;
		dev_warn(dev, "failed to get dp or dm channel\n");
	}
	/* HS03 code for P211026-03673 by gaochao at 20211028 end */

	/* enable usb module */
	reg = msk = (MASK_AON_APB_OTG_UTMI_EB | MASK_AON_APB_ANA_EB);
	regmap_update_bits(phy->hsphy_glb, REG_AON_APB_APB_EB1, msk, reg);

	reg = msk = MASK_AON_APB_CGM_OTG_REF_EN | MASK_AON_APB_CGM_DPHY_REF_EN;
	regmap_update_bits(phy->hsphy_glb, REG_AON_APB_CGM_REG1, msk, reg);

	/* usb power down */
	if (sprd_usbmux_check_mode() != MUX_MODE) {
		reg = msk = (MASK_ANLG_PHY_G2_ANALOG_USB20_USB20_PS_PD_L |
				MASK_ANLG_PHY_G2_ANALOG_USB20_USB20_PS_PD_S);

		regmap_update_bits(phy->ana_g2,
				REG_ANLG_PHY_G2_ANALOG_USB20_USB20_BATTER_PLL, msk, reg);
	}

	phy->phy.dev = dev;
	phy->phy.label = "sprd-hsphy";
	phy->phy.otg = otg;
	phy->phy.init = sprd_hsphy_init;
	phy->phy.shutdown = sprd_hsphy_shutdown;
	phy->phy.post_init = sprd_hsphy_post_init;
	phy->phy.reset_phy = sprd_hsphy_reset;
	phy->phy.set_vbus = sprd_hostphy_set;
	phy->phy.set_emphasis = sprd_hsphy_emphasis_set;
	phy->phy.type = USB_PHY_TYPE_USB2;
	phy->phy.vbus_nb.notifier_call = sprd_hsphy_vbus_notify;
	phy->phy.charger_detect = sprd_hsphy_charger_detect;
	/* HS03 code for P211026-03673 by gaochao at 20211028 start */
	phy->phy.retry_charger_detect = sprd_hsphy_retry_charger_detect;
	/* HS03 code for P211026-03673 by gaochao at 20211028 end */
	otg->usb_phy = &phy->phy;

	platform_set_drvdata(pdev, phy);

	ret = usb_add_phy_dev(&phy->phy);
	if (ret) {
		dev_err(dev, "fail to add phy\n");
		return ret;
	}

	ret = sysfs_create_groups(&dev->kobj, usb_hsphy_groups);
	if (ret)
		dev_err(dev, "failed to create usb hsphy attributes\n");

	if (extcon_get_state(phy->phy.edev, EXTCON_USB) > 0)
		usb_phy_set_charger_state(&phy->phy, USB_CHARGER_PRESENT);

	dev_info(dev, "sprd usb phy probe ok\n");

	return 0;
}

static int sprd_hsphy_remove(struct platform_device *pdev)
{
	struct sprd_hsphy *phy = platform_get_drvdata(pdev);

	sysfs_remove_groups(&pdev->dev.kobj, usb_hsphy_groups);
	usb_remove_phy(&phy->phy);
	regulator_disable(phy->vdd);

	return 0;
}

static const struct of_device_id sprd_hsphy_match[] = {
	{ .compatible = "sprd,qogirl6-phy" },
	{},
};
MODULE_DEVICE_TABLE(of, sprd_hsphy_match);

static struct platform_driver sprd_hsphy_driver = {
	.probe = sprd_hsphy_probe,
	.remove = sprd_hsphy_remove,
	.driver = {
		.name = "sprd-hsphy",
		.of_match_table = sprd_hsphy_match,
	},
};
module_platform_driver(sprd_hsphy_driver);

MODULE_DESCRIPTION("UNISOC Qogirl6 USB PHY driver");
MODULE_LICENSE("GPL v2");
