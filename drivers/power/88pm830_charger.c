/*
 * 88PM830 Battery Charger driver
 *
 * Copyright (c) 2013 Marvell Technology Ltd.
 * Yi Zhang<yizhang@marvell.com>
 * Fenghang Yin <yinfh@marvell.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */

#include <linux/module.h>
#include <linux/regmap.h>
#include <linux/interrupt.h>
#include <linux/slab.h>
#include <linux/power_supply.h>
#include <linux/platform_device.h>
#include <linux/regulator/machine.h>
#include <linux/jiffies.h>
#include <linux/notifier.h>
#include <linux/err.h>
#include <linux/mfd/88pm830.h>
#include <linux/of_device.h>
#include <linux/platform_data/mv_usb.h>

#define UPDATA_INTERVAL		(30*HZ)
#define CHG_RESTART_DELAY	(5) /* minutes */
#define OVERVOLTAGE_RECHARGE_LEVEL  (4150) /* mV */
#define RECHARGE_THRESHOLD	(98)

/* preregulator and charger register */
#define PM830_CHG_CTRL1			(0x3c)
#define VBUS_BOOSTER_EN			(1 << 7)
#define BATT_TEMP_MONITOR_EN		(1 << 4)
#define CHG_START			(1 << 0)

#define PM830_BSUP_CTRL			(0x3d)
#define SMTH_EN				(1 << 7)
#define SMTH_SET_MASK			(0x7 << 4)
#define BYPASS_SMTH_EN			(0x1 << 3)
#define OC_OUT_EN			(0x1 << 2)
#define OC_OUT_SET			(0x3 << 0)

#define PM830_BAT_CTRL			(0x3e)
#define BAT_REM_UV_EN			(0x1 << 7)
#define BAT_SHRT_SET_2			(0x0 << 4)
#define BAT_SHRT_SET_2_8		(0x1 << 4)
#define BAT_SHRT_SET_2_9		(0x2 << 4)
#define BAT_SHRT_SET_3			(0x3 << 4)
#define BAT_SHRT_SET_MASK		(0x3 << 4)
#define BAT_SHRT_EN			(0x1 << 3)
#define OV_BAT_SET_OFFSET		(1)
#define OV_VBAT_EN			(0x1 << 0)
#define SUPPL_PRE_DIS_MASK	(0x1 << 6)
#define SUPPL_PRE_DIS_SET(x)	((x) << 6)

#define PM830_BAT_CTRL2			(0x3f)
#define PM830_BAT_PRTY_EN		(0x1 << 1)

#define PM830_CHG_CTRL2			(0x41)
#define VBAT_PRE_TERM_MASK		(0x3 << 6)
#define VBAT_PRE_TERM_OFFSET		(6)
#define ICHG_PRE_SET_MASK		(0x1f << 0)
#define ICHG_PRE_SET_OFFSET		(0)

#define PM830_CHG_CTRL3			(0x42)
#define ICHG_FAST_TERM_MASK		(0x7 << 5)
#define ICHG_FAST_TERM_OFFSET		(5)
#define ICHG_FAST_SET_MASK		(0xf << 0)
#define ICHG_FAST_SET_OFFSET		(0)
/* for B0 revision and above */
#define ICHG_FAST_TERM_10MA		(0x0)
#define ICHG_FAST_TERM_20MA		(0x1)
#define ICHG_FAST_TERM_40MA		(0x2)
#define ICHG_FAST_TERM_60MA		(0x3)
#define ICHG_FAST_TERM_100MA	(0x4)
#define ICHG_FAST_TERM_150MA	(0x5)
#define ICHG_FAST_TERM_200MA	(0x6)
#define ICHG_FAST_TERM_300MA	(0x7)

#define PM830_CHG_CTRL4			(0x43)
#define FAST_TERM_NUM(x)		((x) << 6)
#define VBAT_FAST_SET_MASK_A1	(0x1f << 0)
#define VBAT_FAST_SET_MASK		(0x3f << 0)
#define VBAT_FAST_SET_OFFSET		(0)


#define PM830_CHG_CTRL5			(0x44)
#define FASTCHG_TIMEOUT_MASK		(0x7 << 4)
#define FASTCHG_TIMEOUT_SET(x)		((x - 1) << 4)
#define PRECHG_TIMEOUT_MASK		(0x7 << 0)
#define PRECHG_TIMEOUT_SET(x)		(((x - 16) >> 3))

#define PM830_CHG_CTRL6			(0x45)
#define VSYS_PREREG_SET(x)		(((x - 3400) / 50) - 1)

#define PM830_CHG_CTRL7			(0x46)
#define PM830_CHG_ILIM_10		(0x4)
#define PM830_CHG_ILIM_OFFSET		(4)
#define PM830_CHG_ILIM_MASK		(0x7)
#define PM830_CHG_CTRL8			(0x47)
#define PM830_CHG_CTRL9			(0x48)
#define PM830_CHG_CTRL10		(0x49)
#define PM830_CHG_CTRL11		(0x4a)
#define PM830_CHG_CTRL12		(0x4b)
#define PM830_CHG_CTRL13		(0x4c)

#define PM830_CHG_STATUS1_A1		(0x4d)
/* B0 change: charger status move to 0x57, 0x58 */
#define PM830_CHG_STATUS1		(0x57)
#define PM830_CHG_STATUS1_MASK		(0xff)
#define PM830_CHG_STATUS4		(0x58)
#define PM830_CHG_STATUS4_MASK		(0x3)
#define PM830_FAULT_VBAT_SHORT	(0x01 << 0)
#define PM830_FAULT_OV_VBAT		(0x01 << 1)
#define PM830_FAULT_BATTEMP_NOK	(0x01 << 2)
#define PM830_FAULT_VPWR_SHORT	(0x01 << 3)
#define PM830_FAULT_CHG_REMOVAL	(0x01 << 4)
#define PM830_FAULT_BAT_REMOVAL	(0x01 << 5)
#define PM830_FAULT_CHG_TIMEOUT	(0x01 << 6)
#define PM830_FAULT_OV_TEMP_INT	(0x01 << 7)
#define PM830_FAULT_CHGWDG_EXPIRED (0x01 << 8)
#define PM830_FAULT_PRE_SUPPL_STOP (0x01 << 9)

#define PM830_CHG_CTRL14		(0x4e)
#define PM830_CHG_CTRL15		(0x4f)
#define PM830_WDG_MASK			(0x1 << 4)
#define PM830_WDG_SET(x)		((x) << 4)
#define PREG_SMGO_EN			(0x1 << 7)

#define PM830_CHG_STATUS2		(0x50)
#define PM830_SMTH			(1 << 3)
#define PM830_VBAT_SHRT			(1 << 4)

#define PM830_CHG_CTRL16		(0x52)
#define PM830_CHG_CTRL17		(0x53)
#define PM830_CHG_CTRL18		(0x54)
#define PM830_CHG_MPPT_BIDIR_EN	(0x1 << 4)
#define PM830_CHG_CTRL19		(0x55)
#define PM830_CHG_MPPT_EN	(0x1 << 7)
#define PM830_CHG_CTRL20		(0x56)

struct pm830_charger_info {
	struct pm830_chg_pdata *pdata;
	struct device *dev;
	struct power_supply ac;
	struct power_supply usb;
	struct notifier_block chg_notif;
	int ac_chg_online;
	int usb_chg_online;
	struct mutex lock;

	struct delayed_work restart_chg_work;
	struct delayed_work bat_short_work;

	unsigned int prechg_cur;	/* precharge current limit */
	unsigned int prechg_vol;	/* precharge voltage limit */
	unsigned int prechg_timeout;	/* precharge time limit: min */

	unsigned int fastchg_eoc;	/* fastcharge end current */
	unsigned int fastchg_cur;	/* fastcharge current */
	unsigned int fastchg_vol;	/* fastcharge voltage */
	unsigned int fastchg_timeout;	/* fastcharge time limit: hr */
	unsigned int over_vol;
	unsigned int vbat_short;
	unsigned int limit_cur;

	unsigned int thermal_dis;
	unsigned int *thermal_thr;

	unsigned int temp_cfg;
	unsigned int temp_thr;

	unsigned int mppt_wght;
	unsigned int mppt_per;
	unsigned int mppt_max_cur;

	unsigned int allow_basic_chg;
	unsigned int allow_recharge;
	unsigned int allow_chg_after_tout;
	unsigned int allow_chg_after_overvoltage;

	int irq_nums;
	int irq[7];

	struct pm830_chip *chip;
	struct regmap *regmap;
	int pm830_status;
};

/*
 * mppt status variable - used to speed up by avoiding operating i2c bus,
 * it's disable by default
 */
static atomic_t mppt_enable = ATOMIC_INIT(0);

static int pm830_stop_charging(struct pm830_charger_info *info);

static enum power_supply_property pm830_props[] = {
	POWER_SUPPLY_PROP_STATUS, /* Charger status output */
	POWER_SUPPLY_PROP_ONLINE, /* External power source */
};

static inline int get_prechg_cur(struct pm830_charger_info *info)
{
	static int ret;
	ret = info->prechg_cur / 10 - 1;
	dev_dbg(info->dev, "%s: precharge current = 0x%x\n",
		__func__, ret);
	return (ret < 0) ? 0 : ret;
}

static inline int get_prechg_vol(struct pm830_charger_info *info)
{
	static int ret;
	ret = (info->prechg_vol - 3300) / 100;
	ret = (ret < 0) ? 0 : ((ret > 0x3) ? 0x3 : ret);
	dev_dbg(info->dev, "%s: precharge voltage = 0x%x\n",
		__func__, ret);
	return ret;
}

static inline int get_fastchg_eoc(struct pm830_charger_info *info)
{
	static int ret;
	/* B0 change */
	unsigned int eoc = info->fastchg_eoc;
	if (info->chip->version > PM830_A1_VERSION) {
		if (eoc >= 300)
			ret = ICHG_FAST_TERM_300MA;
		else if (eoc >= 200)
			ret = ICHG_FAST_TERM_200MA;
		else if (eoc >= 150)
			ret = ICHG_FAST_TERM_150MA;
		else if (eoc >= 100)
			ret = ICHG_FAST_TERM_100MA;
		else if (eoc >= 60)
			ret = ICHG_FAST_TERM_60MA;
		else if (eoc >= 40)
			ret = ICHG_FAST_TERM_40MA;
		else if (eoc >= 20)
			ret = ICHG_FAST_TERM_20MA;
		else
			ret = ICHG_FAST_TERM_10MA;
	} else
		ret = (eoc - 10) / 10;

	dev_dbg(info->dev, "%s: fastcharge eoc = 0x%x\n",
		__func__, ret);
	return (ret < 0) ? 0 : ret;
}

static inline int get_fastchg_cur(struct pm830_charger_info *info)
{
	static int ret;
	ret = (info->fastchg_cur - 500) / 100;
	dev_dbg(info->dev, "%s: fastcharge current = 0x%x\n",
		__func__, ret);
	return (ret < 0) ? 0 : ret;
}

static inline int get_fastchg_vol(struct pm830_charger_info *info)
{
	static int ret;
	if (info->chip->version > PM830_A1_VERSION) {
		/* B0 change */
		ret = (info->fastchg_vol - 3600) / 25;
		if (ret > 0x24)
			ret = 0x24;
	} else
		ret = (info->fastchg_vol - 3600) / 50;

	dev_dbg(info->dev, "%s: fastcharge voltage = 0x%x\n",
		__func__, ret);
	return (ret < 0) ? 0 : ret;
}

static inline int get_limit_cur(struct pm830_charger_info *info)
{
	unsigned int ret = 0;
	if (info->limit_cur > 2400)
		info->limit_cur = 2400;

	if (info->limit_cur > 1600) {
		info->limit_cur *= 2;
		info->limit_cur /= 3;
		ret = 1 << 7;
	}

	ret |= ((info->limit_cur / 100) - 1);
	dev_dbg(info->dev, "%s: current limitation = 0x%x\n",
		__func__, ret);
	return ret;
}

static inline int get_temp_thr(struct pm830_charger_info *info)
{
	static int ret;
	ret = (info->temp_thr * 1000 - 46488) / 832;
	dev_dbg(info->dev, "%s: temperature threshold = 0x%x\n",
		__func__, ret);
	return (ret < 0) ? 0 : ret;
}

static char *pm830_supplied_to[] = {
	"battery",
};

static int pm830_is_chg_allowed(struct pm830_charger_info *info)
{
	union power_supply_propval val;
	int ret, charging;
	struct power_supply *psy;

	if (!info->allow_basic_chg)
		return 0;

	if (!info->allow_chg_after_tout)
		return 0;

	psy = power_supply_get_by_name(info->usb.supplied_to[0]);
	if (!psy || !psy->get_property) {
		dev_err(info->dev, "get battery property failed.\n");
		return 0;
	}

	/* check battery driver initialization firstly */
	ret = psy->get_property(psy, POWER_SUPPLY_PROP_STATUS, &val);
	if (ret) {
		dev_err(info->dev, "get battery property failed\n");
		return 0;
	}

	if (val.intval == POWER_SUPPLY_STATUS_UNKNOWN) {
		dev_dbg(info->dev, "battery driver init still not finish\n");
		return 0;
	}

	/* check if there is a battery present */
	ret = psy->get_property(psy, POWER_SUPPLY_PROP_PRESENT, &val);
	if (ret) {
		dev_err(info->dev, "get battery property failed.\n");
		return 0;
	}

	if (val.intval == 0) {
		dev_dbg(info->dev, "battery not present.\n");
		return 0;
	}

	/* check if battery is OK */
	ret = psy->get_property(psy, POWER_SUPPLY_PROP_HEALTH, &val);
	if (ret) {
		dev_err(info->dev, "get battery property failed.\n");
		return 0;
	}

	if (val.intval != POWER_SUPPLY_HEALTH_GOOD) {
		dev_info(info->dev, "battery health not good.\n");
		return 0;
	}

	/*
	 * allow_rechage should be set to 0 when EOC comes, and recharge is't allowed before battery
	 * driver status change to FULL
	 */
	if (!info->allow_recharge) {
		/*
		 * does internal SOC lower than recharge threshold?
		 * if there is no way to get internal soc, need to always start recharge, otherwise
		 * compare internal soc and the recharge threshold
		 */
		if (info->chip->get_fg_internal_soc) {
			ret = info->chip->get_fg_internal_soc();
			if (ret <= RECHARGE_THRESHOLD)
				info->allow_recharge = 1;
			else
				return 0;
		} else {
			dev_dbg(info->dev, "there is no interface to get internal soc!\n");
			info->allow_recharge = 1;
		}
	} else {
		/*
		 * handle the special case: internal status if FULL, but pm830 device
		 * status is charging
		 */
		regmap_read(info->regmap, PM830_CHG_CTRL1, &charging);
		if ((info->pm830_status == POWER_SUPPLY_STATUS_FULL) && (charging & CHG_START)) {
			dev_dbg(info->dev, "Already start charging\n");
			return 0;
		}
	}

	if (!info->allow_chg_after_overvoltage) {
		/* check battery voltage */
		ret = psy->get_property(psy,
				POWER_SUPPLY_PROP_VOLTAGE_NOW, &val);
		if (ret) {
			dev_err(info->dev, "get battery property failed.\n");
			return 0;
		}

		if (val.intval >= OVERVOLTAGE_RECHARGE_LEVEL) {
			dev_dbg(info->dev, "voltage not low enough.\n");
			return 0;
		} else
			info->allow_chg_after_overvoltage = 1;
	}

	return 1;
}

static int pm830_get_property(struct power_supply *psy,
			      enum power_supply_property psp,
			      union power_supply_propval *val)
{
	unsigned int value;
	struct pm830_charger_info *info =
		dev_get_drvdata(psy->dev->parent);
	switch (psp) {
	case POWER_SUPPLY_PROP_ONLINE:
		regmap_read(info->regmap, PM830_STATUS, &value);
		if ((value & 0x1) == PM830_CHG_DET) {
			if (!strncmp(psy->name, "ac", 2))
				val->intval = info->ac_chg_online;
			else
				val->intval = info->usb_chg_online;

		} else
			val->intval = 0;
		break;
	case POWER_SUPPLY_PROP_STATUS:
		val->intval = info->pm830_status;
		break;
	default:
		return -EINVAL;
	}
	return 0;
}

/*
 * set WA_TH according to VBAT to avoid wrong charger removal detection, have
 * confirmed with silicon design team
 */
static int vbat_array_b1[3] = {3600, 3800, 3900};
static int vbat_array_above_b1[2] = {3600, 3800};
static int pm830_set_wa_th(struct pm830_charger_info *info)
{
	int data, ret, volt, val;
	unsigned char buf[2];
	int i, array_size;
	int *vbat_array = NULL, offset;

	ret = regmap_bulk_read(info->regmap, PM830_VBAT_AVG, buf, 2);
	if (ret < 0)
		return ret;
	data = ((buf[0] & 0xff) << 4) | (buf[1] & 0x0f);
	volt = pm830_get_adc_volt(PM830_GPADC_VBAT, data);

	/*
	 * B1 version:
	 * set WA_TH(0xDE[3:2] according to VBAT:
	 * VBAT < 3.6V, WA_TH = 3.8V
	 * VBAT < 3.8V, WA_TH = 4.0V
	 * VBAT < 3.9V, WA_TH = 4.1V
	 * VBAT >= 3.9V, WA_TH = 4.3V
	 * above B1 version:
	 * set WA_TH(0xD3[3:2] according to VBAT:
	 * VBAT < 3.6V, WA_TH = 4.1V
	 * VBAT < 3.8V, WA_TH = 4.3V
	 * VBAT >= 3.8V, WA_TH = 4.5V
	 */
	if (info->chip->version >= PM830_B2_VERSION) {
		array_size = ARRAY_SIZE(vbat_array_above_b1);
		vbat_array = vbat_array_above_b1;
		offset = 1;
	} else if (info->chip->version == PM830_B1_VERSION) {
		array_size = ARRAY_SIZE(vbat_array_b1);
		vbat_array = vbat_array_b1;
		offset = 0;
	} else {
		/* never run to here */
		return 0;
	}

	dev_dbg(info->dev, "%s: get vbat %d\n", __func__, volt);
	regmap_write(info->regmap, 0xf0, 0x1);
	for (i = 0; i < array_size; i++) {
		if (volt < vbat_array[i])
			break;
	}
	regmap_update_bits(info->regmap, 0xD3, (0x3 << 2), ((i + offset) << 2));
	regmap_write(info->regmap, 0xf0, 0x0);

	/* set VBAT upp threshold according to VBAT */
	if (i >= array_size)
		regmap_write(info->regmap, PM830_VBAT_UPP_TH, 0xff);
	else {
		/* get VBAT upp threshold */
		val = pm830_get_adc_value(PM830_GPADC_VBAT, vbat_array[i]);
		regmap_write(info->regmap, PM830_VBAT_UPP_TH, val >> 4);
	}
	return 0;
}

/*
 * mppt feature:
 * try to get maximum possible power from wall adapter
 */
static void pm830_mppt_enable(struct pm830_charger_info *info, int enable)
{
	if (enable) {
		/*
		 * above B0 version:
		 * set WA_TH to avoid wrong charger removal detection, when mppt
		 * enabled
		 */
		if (info->chip->version >= PM830_B1_VERSION)
			pm830_set_wa_th(info);

		/* enable MPPT bidirectional */
		regmap_update_bits(info->regmap, PM830_CHG_CTRL18,
				PM830_CHG_MPPT_BIDIR_EN,
				PM830_CHG_MPPT_BIDIR_EN);

		regmap_update_bits(info->regmap, PM830_CHG_CTRL19,
				   0xf, info->mppt_max_cur);
		regmap_update_bits(info->regmap, PM830_CHG_CTRL19,
				   0x7 << 4, (info->mppt_per) << 4);
		regmap_update_bits(info->regmap, PM830_CHG_CTRL18,
				   0xf, info->mppt_wght);
		regmap_update_bits(info->regmap, PM830_CHG_CTRL19,
				   PM830_CHG_MPPT_EN, PM830_CHG_MPPT_EN);

		/* set mppt status as enabled */
		atomic_set(&mppt_enable, 1);
	} else {
		regmap_update_bits(info->regmap, PM830_CHG_CTRL19,
				   1 << 7, 0);

		/* set mppt status to disabled */
		atomic_set(&mppt_enable, 0);
	}
}

/*
 * if this handler happened first,
 * it means that battery is out of protection mode
 */
static irqreturn_t pm830_vbat_handler(int irq, void *data)
{
	struct pm830_charger_info *info = data;

	/* battery is not really shoretd, so continue to charge it */
	mutex_lock(&info->lock);
	if (info->vbat_short) {
		info->vbat_short = 0;
		mutex_unlock(&info->lock);
		dev_err(info->dev, "battery not shorted, continue charging!\n");
		regmap_update_bits(info->regmap, PM830_PRE_REGULATOR,
				CHG_START, 0);
		regmap_update_bits(info->regmap, PM830_BAT_CTRL,
				BAT_SHRT_EN, BAT_SHRT_EN);
		regmap_update_bits(info->regmap, PM830_PRE_REGULATOR,
				CHG_START, CHG_START);
	} else
		mutex_unlock(&info->lock);

	/*
	 * above B0 version:
	 * when MPPT enabled, need to set WA_TH according to VBAT
	 */
	if ((info->chip->version >= PM830_B1_VERSION) && atomic_read(&mppt_enable))
		pm830_set_wa_th(info);

	return IRQ_HANDLED;
}

/*
 * if this work happened first,
 * it means that battery is really shorted and cannot be charged
 */
static void pm830_bat_short_work(struct work_struct *work)
{
	struct pm830_charger_info *info =
		container_of(work, struct pm830_charger_info,
			bat_short_work.work);

	mutex_lock(&info->lock);
	if (info->vbat_short) {
		info->vbat_short = 0;
		mutex_unlock(&info->lock);
		dev_err(info->dev, "battery is shorted, cannot charge!\n");
		pm830_stop_charging(info);
		power_supply_changed(&info->ac);
		power_supply_changed(&info->usb);
	} else
		mutex_unlock(&info->lock);
}

static int pm830_handle_short(struct pm830_charger_info *info)
{
	int val;

	regmap_read(info->regmap, PM830_CHG_STATUS2, &val);
	if (val < 0)
		return val;

	/* check if battery is shorted */
	if (!(val & PM830_VBAT_SHRT)) {
		dev_dbg(info->dev, "battery is not shorted\n");
		return 0;
	}

	dev_err(info->dev, "battery might to be shorted!\n");
	info->vbat_short = 1;
	/* disable battery short */
	regmap_update_bits(info->regmap, PM830_BAT_CTRL, BAT_SHRT_EN, 0);
	/* set VBAT upp threshold to 2.2V */
	regmap_write(info->regmap, PM830_VBAT_UPP_TH,
			(pm830_get_adc_value(PM830_GPADC_VBAT, 2200)) >> 4);
	schedule_delayed_work(&info->bat_short_work, 2 * HZ);

	return 0;
}

static int pm830_start_charging(struct pm830_charger_info *info)
{
	int chg_allowed;

	/* Clear CHG_START bit */
	regmap_update_bits(info->regmap, PM830_CHG_CTRL1,
			   CHG_START, 0);

	/* clear charger status registers, otherwise can't start charging */
	regmap_write(info->regmap, PM830_CHG_STATUS1, PM830_CHG_STATUS1_MASK);
	regmap_update_bits(info->regmap, PM830_CHG_STATUS4, PM830_CHG_STATUS4_MASK,
			PM830_CHG_STATUS4_MASK);

	/* set chg current limit even charging is not allowed */
	regmap_write(info->regmap, PM830_CHG_CTRL7,
			get_limit_cur(info));

	chg_allowed = pm830_is_chg_allowed(info);
	if (!chg_allowed) {
		dev_dbg(info->dev, "charging not allowed...!\n");
		if (info->pm830_status != POWER_SUPPLY_STATUS_FULL)
			info->pm830_status = POWER_SUPPLY_STATUS_NOT_CHARGING;
		return 0;
	}

	/* Precharge config */
	regmap_update_bits(info->regmap, PM830_CHG_CTRL2,
			   VBAT_PRE_TERM_MASK,
			   get_prechg_vol(info) << VBAT_PRE_TERM_OFFSET);
	regmap_update_bits(info->regmap, PM830_CHG_CTRL2,
			   ICHG_PRE_SET_MASK,
			   get_prechg_cur(info) << ICHG_PRE_SET_OFFSET);
	regmap_update_bits(info->regmap, PM830_CHG_CTRL5,
			   PRECHG_TIMEOUT_MASK,
			   PRECHG_TIMEOUT_SET(info->prechg_timeout));
	/* Fastcharge config */
	regmap_update_bits(info->regmap, PM830_CHG_CTRL3,
			   ICHG_FAST_TERM_MASK,
			   get_fastchg_eoc(info) << ICHG_FAST_TERM_OFFSET);
	regmap_update_bits(info->regmap, PM830_CHG_CTRL3,
			   ICHG_FAST_SET_MASK,
			   get_fastchg_cur(info) << ICHG_FAST_SET_OFFSET);
	regmap_update_bits(info->regmap, PM830_CHG_CTRL4,
			   (info->chip->version > PM830_A1_VERSION) ?
			   VBAT_FAST_SET_MASK : VBAT_FAST_SET_MASK_A1,
			   get_fastchg_vol(info) << VBAT_FAST_SET_OFFSET);
	regmap_update_bits(info->regmap, PM830_CHG_CTRL5,
			   FASTCHG_TIMEOUT_MASK,
			   FASTCHG_TIMEOUT_SET(info->fastchg_timeout));

	/* current limit fine setting on USB: 0x4->10% */
	regmap_update_bits(info->regmap, PM830_CHG_CTRL7,
			   (PM830_CHG_ILIM_MASK << PM830_CHG_ILIM_OFFSET),
			   (PM830_CHG_ILIM_10 << PM830_CHG_ILIM_OFFSET));

	/* check for battery short */
	pm830_handle_short(info);

	dev_info(info->dev, "start charging...!\n");

	/* Start charging... */
	regmap_update_bits(info->regmap, PM830_CHG_CTRL1,
			   CHG_START, CHG_START);

	/*
	 * if it's not recharge - clear battery priority and set status as charging
	 * if it's recharge - clear battery priority in previous versions of PM830 B2
	 */
	if (info->pm830_status != POWER_SUPPLY_STATUS_FULL) {
		regmap_update_bits(info->regmap, PM830_BAT_CTRL2, PM830_BAT_PRTY_EN, 0);
		info->pm830_status = POWER_SUPPLY_STATUS_CHARGING;
	} else if (info->chip->version < PM830_B2_VERSION)
		regmap_update_bits(info->regmap, PM830_BAT_CTRL2, PM830_BAT_PRTY_EN, 0);

	return 0;
}

static int pm830_stop_charging(struct pm830_charger_info *info)
{
	int bat_det, bat_shrt;
	struct power_supply *psy;
	union power_supply_propval val;
	int ret;
	dev_info(info->dev, "stop charging...!\n");

	/* if it's power supply mode, just return */
	psy = power_supply_get_by_name(info->usb.supplied_to[0]);
	if (!psy || !psy->get_property) {
		dev_err(info->dev, "get battery property failed.\n");
		return 0;
	}

	ret = psy->get_property(psy, POWER_SUPPLY_PROP_TECHNOLOGY, &val);
	if (ret)
		dev_err(info->dev, "get battery property failed\n");
	else if (val.intval == POWER_SUPPLY_TECHNOLOGY_UNKNOWN)
		return 0;

	/*
	 * if charger plug out - clear battery priority and PREG_SMGO_EN, and report discharging
	 * else if battery is present and not short - set battery priority and report discharging
	 * otherwise report not charging
	 */
	regmap_read(info->regmap, PM830_STATUS, &bat_det);
	regmap_read(info->regmap, PM830_CHG_STATUS2, &bat_shrt);
	if (!(info->ac_chg_online || info->usb_chg_online)) {
		regmap_update_bits(info->regmap, PM830_BAT_CTRL2, PM830_BAT_PRTY_EN, 0);
		if (info->chip->version >= PM830_B2_VERSION)
			regmap_update_bits(info->regmap, PM830_CHG_CTRL15, PREG_SMGO_EN, 0);
		info->pm830_status = POWER_SUPPLY_STATUS_DISCHARGING;
	} else if ((bat_det & PM830_BAT_DET) && !(bat_shrt & PM830_VBAT_SHRT)) {
		regmap_update_bits(info->regmap, PM830_BAT_CTRL2, PM830_BAT_PRTY_EN,
				PM830_BAT_PRTY_EN);
		info->pm830_status = POWER_SUPPLY_STATUS_DISCHARGING;
	} else {
		regmap_update_bits(info->regmap, PM830_BAT_CTRL2, PM830_BAT_PRTY_EN, 0);
		info->pm830_status = POWER_SUPPLY_STATUS_NOT_CHARGING;
	}

	/* stop MPPT algorithm and stop charging */
	pm830_mppt_enable(info, 0);
	regmap_update_bits(info->regmap, PM830_CHG_CTRL1,
			   CHG_START, 0);

	return 0;
}

static void pm830_chg_ext_power_changed(struct power_supply *psy)
{
	struct pm830_charger_info *info = dev_get_drvdata(psy->dev->parent);
	int chg_allowed;

	if (!strncmp(psy->name, "ac", 2) && (info->ac_chg_online))
		dev_dbg(info->dev, "%s: AC\n", __func__);
	else if (!strncmp(psy->name, "usb", 3) && (info->usb_chg_online))
		dev_dbg(info->dev, "%s: USB\n", __func__);
	else
		return;

	chg_allowed = pm830_is_chg_allowed(info);

	if (chg_allowed) {
		if (info->pm830_status != POWER_SUPPLY_STATUS_CHARGING) {
			dev_info(info->dev, "%s: Battery OK...\n", __func__);
			pm830_start_charging(info);
			power_supply_changed(&info->ac);
			power_supply_changed(&info->usb);
		}
	} else {
		if (info->pm830_status == POWER_SUPPLY_STATUS_CHARGING) {
			dev_info(info->dev,
				"%s: Battery not OK...!\n", __func__);
			pm830_stop_charging(info);
			power_supply_changed(&info->ac);
			power_supply_changed(&info->usb);
		}
	}
}

static int pm830_powersupply_init(struct pm830_charger_info *info,
				  struct pm830_chg_pdata *pdata)
{
	int ret = 0;

	if (pdata->supplied_to) {
		info->ac.supplied_to = pdata->supplied_to;
		info->ac.num_supplicants = pdata->num_supplicants;
		info->usb.supplied_to = pdata->supplied_to;
		info->usb.num_supplicants = pdata->num_supplicants;
	} else {
		info->ac.supplied_to = pm830_supplied_to;
		info->ac.num_supplicants = ARRAY_SIZE(pm830_supplied_to);
		info->usb.supplied_to = pm830_supplied_to;
		info->usb.num_supplicants = ARRAY_SIZE(pm830_supplied_to);
	}
	/* register ac charger props */
	info->ac.name = "ac";
	info->ac.type = POWER_SUPPLY_TYPE_MAINS;
	info->ac.properties = pm830_props;
	info->ac.num_properties = ARRAY_SIZE(pm830_props);
	info->ac.get_property = pm830_get_property;
	info->ac.external_power_changed = pm830_chg_ext_power_changed;

	ret = power_supply_register(info->dev, &info->ac);
	if (ret)
		goto err_reg_ac;
	/* register usb charger props */
	info->usb.name = "usb";
	info->usb.type = POWER_SUPPLY_TYPE_USB;
	info->usb.properties = pm830_props;
	info->usb.num_properties = ARRAY_SIZE(pm830_props);
	info->usb.get_property = pm830_get_property;
	info->usb.external_power_changed = pm830_chg_ext_power_changed;

	ret = power_supply_register(info->dev, &info->usb);
	if (ret)
		goto err_reg_usb;

	return ret;

err_reg_usb:
	power_supply_unregister(&info->ac);
err_reg_ac:
	return ret;
}

static void pm830_chg_init(struct pm830_charger_info *info)
{
	int val;

	/* allow charging by default */
	info->allow_basic_chg = 1;

	/* Clear CHG_START bit */
	regmap_update_bits(info->regmap, PM830_CHG_CTRL1,
			   CHG_START, 0);
	regmap_update_bits(info->regmap, PM830_CHG_CTRL1,
			   (1 << 1), (1 << 1));
	/* Enable bat detection */
	regmap_update_bits(info->regmap, PM830_GPADC_MEAS_EN,
			   PM830_VBAT_MEAS_EN, PM830_VBAT_MEAS_EN);
	/* enable GPADC with non-stop mode */
	regmap_update_bits(info->regmap, PM830_GPADC_CONFIG1, 0x3, 0x3);

	/* Enable detection of EOC */
	regmap_update_bits(info->regmap, PM830_CC_IBAT, 0x1, 0x1);

	/*
	 * take some margin on OV_VBAT, because some spikes on the VBAT voltage
	 * are possible during current transient, have confirmed with silicon
	 * design team
	 */
	if (info->over_vol < info->fastchg_vol)
		info->over_vol = info->fastchg_vol + 400;
	/* set over-vol threshold*/
	if (info->over_vol >= 5000)
		val = 0x3;
	else if (info->over_vol >= 4800)
		val = 0x2;
	else if (info->over_vol >= 4600)
		val = 0x1;
	else
		val = 0x0;

	regmap_update_bits(info->regmap, PM830_BAT_CTRL,
			   (0x3 << OV_BAT_SET_OFFSET),
			   (val << OV_BAT_SET_OFFSET));

	/* Enable over-vol/short-detection detectors */
	regmap_update_bits(info->regmap, PM830_BAT_CTRL,
			   OV_VBAT_EN, OV_VBAT_EN);
	regmap_update_bits(info->regmap, PM830_BAT_CTRL,
			   BAT_SHRT_EN | BAT_SHRT_SET_MASK,
			   BAT_SHRT_EN | BAT_SHRT_SET_2);

	/* thermal loop */
	if (!info->thermal_dis) {
		/* battery temperature measurement enable */
		regmap_update_bits(info->regmap, PM830_GPADC_MEAS_EN,
				   PM830_GPADC0_MEAS_EN, PM830_GPADC0_MEAS_EN);
		/* set temperature threshold */
		regmap_write(info->regmap, PM830_CHG_CTRL10,
			     info->thermal_thr[0]);
		regmap_write(info->regmap, PM830_CHG_CTRL11,
			     info->thermal_thr[1]);
		regmap_write(info->regmap, PM830_CHG_CTRL12,
			     info->thermal_thr[2]);
		regmap_write(info->regmap, PM830_CHG_CTRL13,
			     info->thermal_thr[3]);
		/* enable thermal loop */
		regmap_update_bits(info->regmap, PM830_CHG_CTRL1,
				   BATT_TEMP_MONITOR_EN,
				   BATT_TEMP_MONITOR_EN);
	} else {
		/* clear thermal enable bit */
		regmap_update_bits(info->regmap, PM830_CHG_CTRL1,
				BATT_TEMP_MONITOR_EN, 0);
	}

	/* Internal temperature protection */
	regmap_write(info->regmap, PM830_CHG_CTRL8, get_temp_thr(info));
	regmap_write(info->regmap, PM830_CHG_CTRL9, info->temp_cfg);

	if (info->chip->version > PM830_A1_VERSION) {
		/* disable watch dog now */
		regmap_update_bits(info->regmap, PM830_CHG_CTRL15,
				PM830_WDG_MASK, PM830_WDG_SET(1));

		/* disable precharge when the device enter supplement mode */
		regmap_update_bits(info->regmap, PM830_BAT_CTRL,
				SUPPL_PRE_DIS_MASK, SUPPL_PRE_DIS_SET(0));
	}

	/*
	 * set VBAT low threshold to 0V, it will be used by battery short detect and WA_TH dynamic
	 * setting
	 */
	regmap_write(info->regmap, PM830_VBAT_LOW_TH, 0);

	/* init power supply status */
	info->pm830_status = POWER_SUPPLY_STATUS_UNKNOWN;
}

static ssize_t pm830_control_charging(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	struct pm830_charger_info *info = dev_get_drvdata(dev);
	int enable;

	if (!info)
		return strnlen(buf, PAGE_SIZE);

	if (sscanf(buf, "%d", &enable) < 0)
		enable = 1;

	if (enable) {
		dev_info(info->dev, "enable charging by manual\n");
		info->allow_basic_chg = 1;
		pm830_start_charging(info);
	} else {
		dev_info(info->dev, "disable charging by manual\n");
		info->allow_basic_chg = 0;
		pm830_stop_charging(info);
	}

	power_supply_changed(&info->ac);
	power_supply_changed(&info->usb);

	return strnlen(buf, PAGE_SIZE);
}

static DEVICE_ATTR(control, S_IWUSR | S_IWGRP, NULL, pm830_control_charging);

static int pm830_chg_notifier_callback(struct notifier_block *nb,
				       unsigned long type, void *chg_event)
{
	struct pm830_charger_info *info =
		container_of(nb, struct pm830_charger_info, chg_notif);
	static unsigned long chg_type = NULL_CHARGER;

	/* no change in charger type - nothing to do */
	if (type == chg_type)
		return 0;

	chg_type = type;

	switch (type) {
	case NULL_CHARGER:
		info->ac_chg_online = 0;
		info->usb_chg_online = 0;
		cancel_delayed_work(&info->restart_chg_work);
		break;
	case SDP_CHARGER:
	case NONE_STANDARD_CHARGER:
	case DEFAULT_CHARGER:
		info->ac_chg_online = 0;
		info->usb_chg_online = 1;
		info->fastchg_cur = 500;
		info->limit_cur = 500;
		break;
	case CDP_CHARGER:
		info->ac_chg_online = 1;
		info->usb_chg_online = 0;
		/* the max current for CDP should be 1.5A */
		info->fastchg_cur = 1500;
		info->limit_cur = 1500;
		break;
	case DCP_CHARGER:
		info->ac_chg_online = 1;
		info->usb_chg_online = 0;
		/* the max value according to spec */
		info->fastchg_cur = 2000;
		info->limit_cur = 2400;
		pm830_mppt_enable(info, 1);
		break;
	default:
		info->ac_chg_online = 0;
		info->usb_chg_online = 1;
		info->fastchg_cur = 500;
		info->limit_cur = 500;
		pm830_mppt_enable(info, 1);
		break;
	}

	info->allow_recharge = 1;
	info->allow_chg_after_tout = 1;
	info->allow_chg_after_overvoltage = 1;

	if (info->usb_chg_online || info->ac_chg_online)
		pm830_start_charging(info);
	else
		pm830_stop_charging(info);

	dev_dbg(info->dev, "usb inserted: ac = %d, usb = %d\n",
		info->ac_chg_online, info->usb_chg_online);
	power_supply_changed(&info->ac);
	power_supply_changed(&info->usb);
	return 0;
}

static irqreturn_t pm830_ov_temp_handler(int irq, void *data)
{
	struct pm830_charger_info *info = data;
	unsigned char buf[2];
	int val, ret;

	ret = regmap_bulk_read(info->regmap, PM830_ITEMP_HI, buf, 2);
	if (ret < 0)
		dev_err(info->dev, "failed to read internal temperature!\n");
	else {
		val = ((buf[0] & 0xff) << 4) | (buf[1] & 0x0f);
		/* temp(C) = val * 0.104 - 273 */
		val = ((val * 213) >> 11) - 273;
		dev_err(info->dev, "over internal temperature: %d(C)!\n", val);
	}

	/*
	 * should never happen.
	 * IRQ is triggered in 150(C) - system should be dead already.
	 */
	BUG();

	return IRQ_HANDLED;
}

static irqreturn_t pm830_done_handler(int irq, void *data)
{
	static int value;
	int ret;
	struct pm830_charger_info *info = data;
	dev_dbg(info->dev, "charge done interrupt is served:\n");

	pm830_stop_charging(info);

	/*
	 * check charging done reason by reading charger status
	 * register
	 */
	if (info->chip->version > PM830_A1_VERSION)
		ret = regmap_read(info->regmap, PM830_CHG_STATUS1, &value);
	else
		ret = regmap_read(info->regmap, PM830_CHG_STATUS1_A1, &value);

	if (value & PM830_FAULT_CHG_REMOVAL) {
		/*
		 * handled in USB driver, don't need to update power supply,
		 * power supply will be updated in usb notifier function
		 */
		dev_dbg(info->dev, "usb plugged out case\n");
	} else if (value & PM830_FAULT_BAT_REMOVAL) {
		/* handled in battery driver, update power supply */
		dev_info(info->dev, "battery plugged out case\n");
	} else if (value & PM830_FAULT_OV_TEMP_INT) {
		/* handled in a dedicated interrupt, update power supply */
		dev_info(info->dev, "internal over temperature case\n");
	} else {
		/* battery is full - update power supply */
		dev_info(info->dev, "Battery Full, Charging done!");
		/* need to change power supply status to full */
		info->pm830_status = POWER_SUPPLY_STATUS_FULL;
		info->allow_recharge = 0;
		/*
		 * B2 version:
		 * set PREG_SMGO_EN bit when EOC trigger, it's from silicon design team, used to
		 * improve battery priority mode
		 */
		if (info->chip->version >= PM830_B2_VERSION)
			regmap_update_bits(info->regmap, PM830_CHG_CTRL15, PREG_SMGO_EN,
					PREG_SMGO_EN);

	}

	if (info->chip->version > PM830_A1_VERSION)
		regmap_write(info->regmap, PM830_CHG_STATUS1, value);
	else
		regmap_write(info->regmap, PM830_CHG_STATUS1_A1, value);

	power_supply_changed(&info->ac);
	power_supply_changed(&info->usb);

	return IRQ_HANDLED;
}

static void pm830_restart_chg_work(struct work_struct *work)
{
	struct pm830_charger_info *info =
		container_of(work, struct pm830_charger_info,
				restart_chg_work.work);

	info->allow_chg_after_tout = 1;
	/* verify that charger is still present */
	if (info->ac_chg_online || info->usb_chg_online) {
		pm830_start_charging(info);
		power_supply_changed(&info->ac);
		power_supply_changed(&info->usb);
	}
}

static irqreturn_t pm830_exception_handler(int irq, void *data)
{
	int status = 0;
	struct pm830_charger_info *info = data;
	char buf[2];

	if (info->chip->version < PM830_B0_VERSION) {
		/* report interrupt reason according to register 0x4d */
		regmap_read(info->regmap, PM830_CHG_STATUS1_A1, &status);
	} else {
		/*
		* report interrupt reason according to register 0x57,
		* 0x58(B0 change)
		*/
		regmap_bulk_read(info->regmap, PM830_CHG_STATUS1, buf, 2);
		status = buf[0] | (buf[1] << 8);
	}
	/* PM830_IRQ_CHG_FAULT */
	if (status & PM830_FAULT_VBAT_SHORT)
		dev_err(info->dev, "battery short detect!\n");
	if (status & PM830_FAULT_OV_VBAT) {
		/* battery overvoltage, try to recharge later */
		dev_err(info->dev, "battery over voltage!\n");
		info->allow_chg_after_overvoltage = 0;
	}
	if (status & PM830_FAULT_BATTEMP_NOK)
		/* TODO: polling temperature to reenable charge */
		dev_err(info->dev, "battery over temperature!\n");
	/* PM830_IRQ_CHG_TOUT */
	if (status & PM830_FAULT_CHG_TIMEOUT) {
		/* charger timeout, try to recharger later */
		dev_err(info->dev, "charge timeout!\n");
		info->allow_chg_after_tout = 0;
		schedule_delayed_work(&info->restart_chg_work,
				CHG_RESTART_DELAY * 60 * HZ);
	}
	if (status & PM830_FAULT_PRE_SUPPL_STOP)
		dev_err(info->dev, "charger supplement stop!\n");
	if (status & PM830_FAULT_CHGWDG_EXPIRED)
		dev_err(info->dev, "charger watchdog timeout!\n");

	/* write clear, only clear fault flag bits */
	if (info->chip->version < PM830_B0_VERSION)
		regmap_write(info->regmap, PM830_CHG_STATUS1_A1, status);
	else
		regmap_bulk_write(info->regmap, PM830_CHG_STATUS1, buf, 2);

	pm830_stop_charging(info);

	/* power supply status changed */
	power_supply_changed(&info->ac);
	power_supply_changed(&info->usb);

	return IRQ_HANDLED;
}

static struct pm830_irq_desc {
	const char *name;
	irqreturn_t (*handler)(int irq, void *data);
} pm830_irq_descs[] = {
	{"charger internal temp", pm830_ov_temp_handler},
	{"charge done", pm830_done_handler},
	{"charge timeout", pm830_exception_handler},
	{"charge fault", pm830_exception_handler},
	{"battery voltage", pm830_vbat_handler},
};

static int pm830_chg_dt_init(struct device_node *np,
			 struct device *dev,
			 struct pm830_chg_pdata *pdata)
{
	int ret;
	ret = of_property_read_u32(np, "prechg-current", &pdata->prechg_cur);
	if (ret)
		return ret;
	ret = of_property_read_u32(np, "prechg-voltage", &pdata->prechg_vol);
	if (ret)
		return ret;
	ret = of_property_read_u32(np, "prechg-timeout",
				   &pdata->prechg_timeout);
	if (ret)
		return ret;
	ret = of_property_read_u32(np, "fastchg-eoc", &pdata->fastchg_eoc);
	if (ret)
		return ret;
	ret = of_property_read_u32(np, "fastchg-voltage", &pdata->fastchg_vol);
	if (ret)
		return ret;
	ret = of_property_read_u32(np, "fastchg-timeout",
				   &pdata->fastchg_timeout);
	if (ret)
		return ret;
	ret = of_property_read_u32(np, "over-voltage", &pdata->over_vol);
	if (ret)
		return ret;

	ret = of_property_read_u32(np, "no-thermal-support",
			&pdata->thermal_dis);
	if (ret)
		return ret;

	ret = of_property_read_u32_array(np, "thermal-threshold",
					 pdata->thermal_thr, 4);
	if (ret)
		return ret;
	ret = of_property_read_u32(np, "temp-configure", &pdata->temp_cfg);
	if (ret)
		return ret;
	ret = of_property_read_u32(np, "temp-threshold", &pdata->temp_thr);
	if (ret)
		return ret;

	ret = of_property_read_u32(np, "mppt-weight", &pdata->mppt_wght);
	if (ret)
		return ret;
	ret = of_property_read_u32(np, "mppt-period", &pdata->mppt_per);
	if (ret)
		return ret;
	ret = of_property_read_u32(np, "mppt-start-current",
				&pdata->mppt_max_cur);
	if (ret)
		return ret;

	pdata->charger_control_interface = of_property_read_bool(np,
			"charger-control-interface");

	return 0;
}

static int pm830_charger_probe(struct platform_device *pdev)
{
	struct pm830_charger_info *info;
	struct pm830_chg_pdata *pdata = pdev->dev.platform_data;
	struct pm830_chip *chip = dev_get_drvdata(pdev->dev.parent);
	struct device_node *node = pdev->dev.of_node;
	int ret = 0;
	int i;
	int j;

	if (IS_ENABLED(CONFIG_OF)) {
		if (!pdata) {
			pdata = devm_kzalloc(&pdev->dev,
					     sizeof(*pdata), GFP_KERNEL);
			if (!pdata)
				return -ENOMEM;
		}
		ret = pm830_chg_dt_init(node, &pdev->dev, pdata);
		if (ret)
			goto out;
	} else if (!pdata) {
		return -EINVAL;
	}

	info = devm_kzalloc(&pdev->dev, sizeof(struct pm830_charger_info),
			GFP_KERNEL);
	if (!info) {
		dev_err(&pdev->dev, "Cannot allocate memory.\n");
		return -ENOMEM;
	}

	if (pdata->charger_control_interface) {
		ret = device_create_file(&pdev->dev, &dev_attr_control);
		if (ret < 0)
			goto out;
	}

	info->pdata = pdata;
	info->dev = &pdev->dev;

	info->prechg_cur = pdata->prechg_cur;
	info->prechg_vol = pdata->prechg_vol;
	info->prechg_timeout = pdata->prechg_timeout;

	info->fastchg_eoc = pdata->fastchg_eoc;
	info->fastchg_vol = pdata->fastchg_vol;
	info->fastchg_timeout = pdata->fastchg_timeout;

	info->over_vol = pdata->over_vol;

	info->thermal_dis = pdata->thermal_dis;
	info->thermal_thr = pdata->thermal_thr;

	info->temp_cfg = pdata->temp_cfg;
	info->temp_thr = pdata->temp_thr;

	info->mppt_wght = pdata->mppt_wght;
	info->mppt_per = pdata->mppt_per;
	info->mppt_max_cur = pdata->mppt_max_cur;

	info->chip = chip;
	info->regmap = chip->regmap;

	mutex_init(&info->lock);

	platform_set_drvdata(pdev, info);

	for (i = 0, j = 0; i < pdev->num_resources; i++) {
		info->irq[j] = platform_get_irq(pdev, i);
		if (info->irq[j] < 0)
			continue;
		j++;
	}
	info->irq_nums = j;

	/* register powersupply */
	ret = pm830_powersupply_init(info, pdata);
	if (ret) {
		dev_err(info->dev, "register powersupply fail!\n");
		goto out;
	}

	/* register charger event notifier */
	info->chg_notif.notifier_call = pm830_chg_notifier_callback;

#ifdef CONFIG_USB_MV_UDC
	ret = mv_udc_register_client(&info->chg_notif);
	if (ret < 0) {
		dev_err(info->dev, "failed to register client!\n");
		goto err_psy;
	}
#endif

	pm830_chg_init(info);
	INIT_DELAYED_WORK(&info->restart_chg_work, pm830_restart_chg_work);
	INIT_DELAYED_WORK(&info->bat_short_work, pm830_bat_short_work);

	/* interrupt should be request in the last stage */
	for (i = 0; i < info->irq_nums; i++) {
		ret = devm_request_threaded_irq(info->dev, info->irq[i], NULL,
					   pm830_irq_descs[i].handler,
					   IRQF_ONESHOT | IRQF_NO_SUSPEND,
					   pm830_irq_descs[i].name, info);
		if (ret < 0) {
			dev_err(info->dev, "failed to request IRQ: #%d: %d\n",
				info->irq[i], ret);
			goto out_irq;
		}
	}

	dev_dbg(info->dev, "%s is successful!\n", __func__);

	return 0;
out_irq:
	while (--i >= 0)
		devm_free_irq(info->dev, info->irq[i], info);
#ifdef CONFIG_USB_MV_UDC
err_psy:
#endif
	power_supply_unregister(&info->ac);
	power_supply_unregister(&info->usb);
out:
	return ret;
}

static int pm830_charger_remove(struct platform_device *pdev)
{
	int i;
	struct pm830_charger_info *info = platform_get_drvdata(pdev);
	if (!info)
		return 0;

	for (i = 0; i < info->irq_nums; i++) {
		if (pm830_irq_descs[i].handler != NULL)
			devm_free_irq(info->dev, info->irq[i], info);
	}

	cancel_delayed_work(&info->restart_chg_work);
	cancel_delayed_work(&info->bat_short_work);
	pm830_stop_charging(info);
#ifdef CONFIG_USB_MV_UDC
	mv_udc_unregister_client(&info->chg_notif);
#endif
	power_supply_unregister(&info->ac);
	power_supply_unregister(&info->usb);

	platform_set_drvdata(pdev, NULL);
	return 0;
}

static void pm830_charger_shutdown(struct platform_device *pdev)
{
	pm830_charger_remove(pdev);
	return;
}

static const struct of_device_id pm830_chg_dt_match[] = {
	{ .compatible = "marvell,88pm830-chg", },
	{ },
};
MODULE_DEVICE_TABLE(of, pm830_chg_dt_match);

static struct platform_driver pm830_charger_driver = {
	.probe = pm830_charger_probe,
	.remove = pm830_charger_remove,
	.shutdown = pm830_charger_shutdown,
	.driver = {
		.owner = THIS_MODULE,
		.name = "88pm830-chg",
		.of_match_table = of_match_ptr(pm830_chg_dt_match),
	},
};

module_platform_driver(pm830_charger_driver);

MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("88pm830 Charger Driver");
MODULE_AUTHOR("Yi Zhang<yizhang@marvell.com>");
