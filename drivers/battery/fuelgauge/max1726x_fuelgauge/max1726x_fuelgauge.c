/*
 * Maxim MAX1726x IC Fuel Gauge driver
 *
 * Author:  Mahir Ozturk <mahir.ozturk@maximintegrated.com>
 *          Jason Cole <jason.cole@maximintegrated.com>
 *          Kerem Sahin <kerem.sahin@maximintegrated.com>
 * Copyright (C) 2018 Maxim Integrated
 *
 * This program is free software; you can redistribute  it and/or modify it
 * under  the terms of  the GNU General  Public License as published by the
 * Free Software Foundation;  either version 2 of the  License, or (at your
 * option) any later version.
 */

 //Version 1.0.2 - updated model loading to include RCOMPSeg (0xAF)

#include <linux/delay.h>
#include <linux/err.h>
#include <linux/init.h>
#include <linux/interrupt.h>
#include <linux/i2c.h>
#include <linux/module.h>
#include <linux/mod_devicetable.h>
#include <linux/mutex.h>
#include <linux/of.h>
#include <linux/power_supply.h>
#include <linux/platform_device.h>
#include <linux/pm.h>
#include <linux/regmap.h>
#include <linux/slab.h>
#include "max1726x_fuelgauge.h"
#include "../../common/sec_charging_common.h"

#define DRV_NAME "max1726x"

/* CONFIG register bits */
#define MAX1726X_CONFIG_ALRT_EN         (1 << 2)
#define MAX1726X_CONFIG_TEX_EN          (1 << 8)

/* FILTERCFG register bits */
#define MAX1726X_FILTERCFG_CURR_SHIFT   0
#define MAX1726X_FILTERCFG_CURR_MASK    (0xF << 0)

/* CONFIG2 register bits */
#define MAX1726X_CONFIG2_LDMDL          (1 << 5)

/* STATUS register bits */
#define MAX1726X_STATUS_BST             (1 << 3)
#define MAX1726X_STATUS_POR             (1 << 1)

/* MODELCFG register bits */
#define MAX1726X_MODELCFG_REFRESH       (1 << 15)

/* TALRTTH register bits */
#define MIN_TEMP_ALERT                  0
#define MAX_TEMP_ALERT                  8

/* FSTAT register bits */
#define MAX1726X_FSTAT_DNR              (1)

#if defined(CONFIG_BATTERY_AGE_FORECAST)
/* FullSOCThr register bits */
#define MAX1726X_FULLSOC_MASK           (0xFF << 8)
#define MAX1726X_FULLSOC_SHIFT			8
#endif

/* STATUS interrupt status bits */
#define MAX1726X_STATUS_ALRT_CLR_MASK   (0x88BB)
#define MAX1726X_STATUS_SOC_MAX_ALRT    (1 << 14)
#define MAX1726X_STATUS_TEMP_MAX_ALRT   (1 << 13)
#define MAX1726X_STATUS_VOLT_MAX_ALRT   (1 << 12)
#define MAX1726X_STATUS_SOC_MIN_ALRT    (1 << 10)
#define MAX1726X_STATUS_TEMP_MIN_ALRT   (1 << 9)
#define MAX1726X_STATUS_VOLT_MIN_ALRT   (1 << 8)
#define MAX1726X_STATUS_CURR_MAX_ALRT   (1 << 6)
#define MAX1726X_STATUS_CURR_MIN_ALRT   (1 << 2)

#define MAX1726X_VMAX_TOLERANCE     50 /* 50 mV */

#define MAX1726X_FACTORY_MODE_SOC	50 /* 0DECIMAL */
#define MAX1726X_FACTORY_MODE_VOL	4321 /* mV */

enum max1726x_vempty_mode {
	VEMPTY_MODE_HW = 0,
	VEMPTY_MODE_SW,
	VEMPTY_MODE_SW_VALERT,
	VEMPTY_MODE_SW_RECOVERY,
};

enum chip_id {
	ID_MAX1726X,
};

struct lost_soc_data {
	/* data */
	bool ing;
	int prev_raw_soc;
	int prev_remcap;
	int prev_qh;
	int lost_cap;
	int weight;
};

struct max1726x_priv {
	struct i2c_client       *client;
	struct device           *dev;
	struct regmap           *regmap;
	struct power_supply	    *psy_fg;
	struct max1726x_platform_data   *pdata;
	struct attribute_group  *attr_grp;
	struct mutex		i2c_lock;

	int cable_type;
	bool is_charging;
	int raw_soc;
	int temperature;

	unsigned int capacity_old;	/* atomic, skip_abnormal calculation */
	unsigned int capacity_max;	/* dynamic calculation */
	unsigned int g_capacity_max;	/* dynamic calculation */
	bool capacity_max_conv;

	bool initial_update_of_soc;

	unsigned int vempty_mode;
	bool vempty_init_flag;
	unsigned long vempty_time;
#if defined(CONFIG_BATTERY_CISD)
	bool valert_count_flag;
#endif

	struct lost_soc_data lost_soc;

#if IS_ENABLED(CONFIG_USB_FACTORY_MODE)
	unsigned int f_mode;
#endif
	bool vbat_open;
};

static unsigned int __read_mostly lpcharge;
module_param(lpcharge, uint, 0444);
static unsigned int __read_mostly vbat_adc;
module_param(vbat_adc, uint, 0444);
#if IS_ENABLED(CONFIG_USB_FACTORY_MODE)
static char __read_mostly *f_mode;
module_param(f_mode, charp, 0444);
#endif

#define I2C_RETRY_CNT	3

static int max1726x_read_reg(struct max1726x_priv *priv, unsigned int reg, unsigned int *val)
{
	int i = 0, ret = 0;

#if IS_ENABLED(CONFIG_USB_FACTORY_MODE)
	if (priv->f_mode == OB_MODE && priv->vbat_open)
		return 0;
#endif

	mutex_lock(&priv->i2c_lock);
	for (i = 0; i < I2C_RETRY_CNT; ++i) {
		ret = regmap_read(priv->regmap, reg, val);
		if (ret >= 0)
			break;
		pr_info("%s: reg(0x%x), ret(%d), i2c_retry_cnt(%d/%d)\n",
			__func__, reg, ret, i + 1, I2C_RETRY_CNT);
	}
	mutex_unlock(&priv->i2c_lock);

	return ret;
}

static int max1726x_write_reg(struct max1726x_priv *priv, unsigned int reg, unsigned int val)
{
	int i = 0, ret = 0;

#if IS_ENABLED(CONFIG_USB_FACTORY_MODE)
	if (priv->f_mode == OB_MODE && priv->vbat_open)
		return 0;
#endif

	mutex_lock(&priv->i2c_lock);
	for (i = 0; i < I2C_RETRY_CNT; ++i) {
		ret = regmap_write(priv->regmap, reg, val);
		if (ret >= 0)
			break;
		pr_info("%s: reg(0x%x), ret(%d), i2c_retry_cnt(%d/%d)\n",
			__func__, reg, ret, i + 1, I2C_RETRY_CNT);
	}
	mutex_unlock(&priv->i2c_lock);

	return ret;
}

static int max1726x_update_bits(struct max1726x_priv *priv,
			unsigned int reg, unsigned int mask, unsigned int val)
{
	int i = 0, ret = 0;

#if IS_ENABLED(CONFIG_USB_FACTORY_MODE)
	if (priv->f_mode == OB_MODE && priv->vbat_open)
		return 0;
#endif

	mutex_lock(&priv->i2c_lock);
	for (i = 0; i < I2C_RETRY_CNT; ++i) {
		ret = regmap_update_bits(priv->regmap, reg, mask, val);
		if (ret >= 0)
			break;
		pr_info("%s: reg(0x%x), ret(%d), i2c_retry_cnt(%d/%d)\n",
			__func__, reg, ret, i + 1, I2C_RETRY_CNT);
	}
	mutex_unlock(&priv->i2c_lock);

	return ret;
}

static inline int max1726x_lsb_to_uvolts(struct max1726x_priv *priv, int lsb)
{
	return lsb * 625 / 8; /* 78.125uV per bit */
}

static inline int max1726x_lsb_to_mvolts(struct max1726x_priv *priv, int lsb)
{
	return max1726x_lsb_to_uvolts(priv, lsb) / 1000;
}

static int max1726x_raw_current_to_uamps(struct max1726x_priv *priv, u32 curr)
{
	int res = curr;

	/* Negative */
	if (res & 0x8000)
		res |= 0xFFFF0000;

	res *= 1562500 / (priv->pdata->rsense * 1000);
	return res;
}

static int max1726x_raw_current_to_mamps(struct max1726x_priv *priv, u32 curr)
{
	return max1726x_raw_current_to_uamps(priv, curr) / 1000;
}

static inline int max1726x_lsb_to_capacity(struct max1726x_priv *priv, int lsb)
{
	return lsb * 5 / priv->pdata->rsense; /* 0.5mAh per bit with a 10mohm */
}

static enum power_supply_property max1726x_fuelgauge_props[] = {
	POWER_SUPPLY_PROP_PRESENT,
};

static ssize_t max1726x_log_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct max1726x_priv *priv = dev_get_drvdata(dev);
	int rc = 0, reg = 0;
	u32 val = 0;

	for (reg = 0; reg < 0xE0; reg++) {
		max1726x_read_reg(priv, reg, &val);
		rc += (int)snprintf(buf+rc, PAGE_SIZE-rc, "0x%04X,", val);

		if (reg == 0x4F)
			reg += 0x60;

		if (reg == 0xBF)
			reg += 0x10;
	}

	rc += (int)snprintf(buf+rc, PAGE_SIZE-rc, "\n");

	return rc;
}

static DEVICE_ATTR(log, 0444, max1726x_log_show, NULL);

static struct attribute *max1726x_attr[] = {
	&dev_attr_log.attr,
	NULL
};

static struct attribute_group max1726x_attr_group = {
	.attrs = max1726x_attr,
};

#if !defined(CONFIG_SEC_FACTORY)
static void max1726x_periodic_read(struct max1726x_priv *priv)
{
	int i, reg, ret;
	u32 data[0x10];
	char *str = NULL;

#if IS_ENABLED(CONFIG_USB_FACTORY_MODE)
	if (priv->f_mode == OB_MODE && priv->vbat_open) {
		pr_debug("%s: %s with vbat_open.\n", __func__, BOOT_MODE_STRING[priv->f_mode]);
		return;
	}
#endif

	str = kzalloc(sizeof(char) * 1024, GFP_KERNEL);
	if (!str)
		return;

	for (i = 0; i < 16; i++) {
		if (i == 0x5)
			i = 0xB;
		if (i == 0xC)
			i = 0xD;

		for (reg = 0; reg < 0x10; reg++) {
			ret = max1726x_read_reg(priv, reg + i * 0x10, &data[reg]);
			if (ret < 0)
				goto out;
		}
		sprintf(str + strlen(str),
			"%04xh,%04xh,%04xh,%04xh,%04xh,%04xh,%04xh,%04xh,",
			data[0x00], data[0x01], data[0x02], data[0x03],
			data[0x04], data[0x05], data[0x06], data[0x07]);
		sprintf(str + strlen(str),
			"%04xh,%04xh,%04xh,%04xh,%04xh,%04xh,%04xh,%04xh,",
			data[0x08], data[0x09], data[0x0a], data[0x0b],
			data[0x0c], data[0x0d], data[0x0e], data[0x0f]);

		if (!priv->initial_update_of_soc)
			usleep_range(1000, 1100);
	}

	pr_info("max1726x: [FG] %s\n", str);
out:
	kfree(str);
}
#endif

static int max1726x_set_temperature(struct max1726x_priv *priv, int temp)
{ /* temp is 0.1DegreeC */
	int ret;
	u32 val;

#if IS_ENABLED(CONFIG_USB_FACTORY_MODE)
	if (priv->f_mode != NO_MODE && priv->vbat_open) {
		pr_debug("%s: %s with vbat_open.\n", __func__, BOOT_MODE_STRING[priv->f_mode]);
		return 0;
	}
#endif

	priv->temperature = temp;
	if (!priv->vempty_init_flag)
		priv->vempty_init_flag = true;

	val = (temp % 10) * 1000 / 39; /* Units of LSB = 1 / 256 degreeC */
	val |= (temp / 10) << 8; /* Units of upper byte = 1 degreeC */

	ret = max1726x_write_reg(priv, MAX1726X_TEMP_REG, val & 0xFFFF);
	if (ret < 0)
		return ret;

	pr_debug("%s: temp:%d, val(0x%04x)\n", __func__, temp, val);

	return 0;
}

static int max1726x_get_temperature(struct max1726x_priv *priv, int *temp)
{
	int ret;
	u32 data;

#if IS_ENABLED(CONFIG_USB_FACTORY_MODE)
	if (priv->f_mode != NO_MODE && priv->vbat_open) {
		pr_debug("%s: %s with vbat_open.\n", __func__, BOOT_MODE_STRING[priv->f_mode]);
		return 0;
	}
#endif

	ret = max1726x_read_reg(priv, MAX1726X_TEMP_REG, &data);
	if (ret < 0)
		return ret;

	*temp = data;
	/* The value is signed. */
	if (*temp & 0x8000)
		*temp |= 0xFFFF0000;

	/* The value is converted into centigrade scale */
	/* Units of LSB = 1 / 256 degree Celsius */
	*temp >>= 8;

	return 0;
}

static int max1726x_get_temperature_limit(struct max1726x_priv *priv, int *temp, int shift)
{
	int ret;
	u32 data;

	ret = max1726x_read_reg(priv, MAX1726X_TALRTTH_REG, &data);
	if (ret < 0)
		return ret;

	*temp = data >> shift;
	/* The value is signed */
	if (*temp & 0x80)
		*temp |= 0xFFFFFF00;

	/* LSB is 1DegreeC */
	return 0;
}

static int max1726x_get_battery_health(struct max1726x_priv *priv, int *health)
{
	int temp, vavg, vbatt, ret;
	u32 val;

	ret = max1726x_read_reg(priv, MAX1726X_AVGVCELL_REG, &val);
	if (ret < 0)
		goto health_error;

	/* bits [0-3] unused */
	vavg = max1726x_lsb_to_uvolts(priv, val);
	/* Convert to millivolts */
	vavg /= 1000;

	ret = max1726x_read_reg(priv, MAX1726X_VCELL_REG, &val);
	if (ret < 0)
		goto health_error;

	/* bits [0-3] unused */
	vbatt = max1726x_lsb_to_uvolts(priv, val);
	/* Convert to millivolts */
	vbatt /= 1000;

	if (vavg < priv->pdata->volt_min) {
		*health = POWER_SUPPLY_HEALTH_DEAD;
		goto out;
	}

	if (vbatt > priv->pdata->volt_max + MAX1726X_VMAX_TOLERANCE) {
		*health = POWER_SUPPLY_HEALTH_OVERVOLTAGE;
		goto out;
	}

	ret = max1726x_get_temperature(priv, &temp);
	if (ret < 0)
		goto health_error;

	if (temp <= priv->pdata->temp_min) {
		*health = POWER_SUPPLY_HEALTH_COLD;
		goto out;
	}

	if (temp >= priv->pdata->temp_max) {
		*health = POWER_SUPPLY_HEALTH_OVERHEAT;
		goto out;
	}

	*health = POWER_SUPPLY_HEALTH_GOOD;

out:
	return 0;

health_error:
	return ret;
}

static int max1726x_read_capacity(struct max1726x_priv *priv, unsigned int reg)
{
	unsigned int val;
	int ret = 0;

	ret = max1726x_read_reg(priv, reg, &val);
	if (ret < 0)
		return ret;

	return max1726x_lsb_to_capacity(priv, val);
}

static int max1726x_read_qh(struct max1726x_priv *priv)
{
	unsigned int val;
	int ret = 0, qh = 0;

	ret = max1726x_read_reg(priv, MAX1726X_QH_REG, &val);
	if (ret < 0)
		return ret;

	/* Negative */
	if (val & 0x8000)
		val |= 0xFFFF0000;

	qh = max1726x_lsb_to_capacity(priv, val);

	return qh; /* mAh */
}

static int max1726x_read_vcell(struct max1726x_priv *priv, int unit)
{
	unsigned int val;
	int ret = 0, vcell = 0;

#if IS_ENABLED(CONFIG_USB_FACTORY_MODE)
	if (priv->f_mode == OB_MODE && priv->vbat_open) {
		pr_debug("%s: %s with vbat_open.\n", __func__, BOOT_MODE_STRING[priv->f_mode]);
		return MAX1726X_FACTORY_MODE_VOL;
	}
#endif

	ret = max1726x_read_reg(priv, MAX1726X_VCELL_REG, &val);
	if (ret < 0)
		return ret;

	if (unit == SEC_BATTERY_VOLTAGE_UV) {
		vcell = max1726x_lsb_to_uvolts(priv, val);
	} else {
		vcell = max1726x_lsb_to_mvolts(priv, val);

		if (priv->pdata->using_temp_compensation &&
			(priv->vempty_mode == VEMPTY_MODE_SW_VALERT) &&
			(vcell >= priv->pdata->sw_vempty_recover_vol)) {
			priv->vempty_mode = VEMPTY_MODE_SW_RECOVERY;
			pr_info("%s: Recoverd from SW V EMPTY Activation\n", __func__);
#if defined(CONFIG_BATTERY_CISD)
			if (priv->valert_count_flag) {
				pr_info("%s: Vcell(%d) release CISD VALERT COUNT check\n",
						__func__, vcell);
				priv->valert_count_flag = false;
			}
#endif
		}
	}

	return vcell;
}

static int max1726x_read_avgvcell(struct max1726x_priv *priv, int unit)
{
	unsigned int val;
	int ret = 0, vcell = 0;

#if IS_ENABLED(CONFIG_USB_FACTORY_MODE)
	if (priv->f_mode != NO_MODE && priv->vbat_open) {
		pr_debug("%s: %s with vbat_open.\n", __func__, BOOT_MODE_STRING[priv->f_mode]);
		return MAX1726X_FACTORY_MODE_VOL;
	}
#endif

	ret = max1726x_read_reg(priv, MAX1726X_AVGVCELL_REG, &val);
	if (ret < 0)
		return ret;

	if (unit == SEC_BATTERY_VOLTAGE_UV)
		vcell = max1726x_lsb_to_uvolts(priv, val);
	else
		vcell = max1726x_lsb_to_mvolts(priv, val);

	return vcell;
}

static int max1726x_read_ocv(struct max1726x_priv *priv, int unit)
{
	unsigned int val;
	int ret = 0, ocv = 0;

#if IS_ENABLED(CONFIG_USB_FACTORY_MODE)
	if (priv->f_mode == OB_MODE && priv->vbat_open) {
		pr_debug("%s: %s with vbat_open.\n", __func__, BOOT_MODE_STRING[priv->f_mode]);
		return MAX1726X_FACTORY_MODE_VOL;
	}
#endif

	ret = max1726x_read_reg(priv, MAX1726X_OCV_REG, &val);
	if (ret < 0)
		return ret;

	if (unit == SEC_BATTERY_VOLTAGE_UV)
		ocv = max1726x_lsb_to_uvolts(priv, val);
	else
		ocv = max1726x_lsb_to_mvolts(priv, val);

	return ocv;
}

static int max1726x_read_current(struct max1726x_priv *priv, int unit)
{
	unsigned int val;
	int ret = 0, curr = 0;

#if IS_ENABLED(CONFIG_USB_FACTORY_MODE)
	if (priv->f_mode != NO_MODE && priv->vbat_open) {
		pr_debug("%s: %s with vbat_open.\n", __func__, BOOT_MODE_STRING[priv->f_mode]);
		return 0;
	}
#endif

	ret = max1726x_read_reg(priv, MAX1726X_CURRENT_REG, &val);
	if (ret < 0)
		return ret;

	if (unit == SEC_BATTERY_CURRENT_UA)
		curr = max1726x_raw_current_to_uamps(priv, val);
	else
		curr = max1726x_raw_current_to_mamps(priv, val);

	return curr;
}

static int max1726x_read_avgcurrent(struct max1726x_priv *priv, int unit)
{
	unsigned int val;
	int ret = 0, curr = 0;

#if IS_ENABLED(CONFIG_USB_FACTORY_MODE)
	if (priv->f_mode != NO_MODE && priv->vbat_open) {
		pr_debug("%s: %s with vbat_open.\n", __func__, BOOT_MODE_STRING[priv->f_mode]);
		return 0;
	}
#endif

	ret = max1726x_read_reg(priv, MAX1726X_AVGCURRENT_REG, &val);
	if (ret < 0)
		return ret;

	if (unit == SEC_BATTERY_CURRENT_UA)
		curr = max1726x_raw_current_to_uamps(priv, val);
	else
		curr = max1726x_raw_current_to_mamps(priv, val);

	return curr;
}

#define MAX1726X_0DECIMAL 0
#define MAX1726X_1DECIMAL 1
#define MAX1726X_2DECIMAL 2

static int max1726x_read_rawsoc(struct max1726x_priv *priv, int decimal)
{
	unsigned int val;
	int soc;
	int ret;

#if IS_ENABLED(CONFIG_USB_FACTORY_MODE)
	if (priv->f_mode == OB_MODE && priv->vbat_open) {
		pr_debug("%s: %s with vbat_open.\n", __func__, BOOT_MODE_STRING[priv->f_mode]);
		if (decimal == MAX1726X_2DECIMAL)
			return MAX1726X_FACTORY_MODE_SOC * 100;
		else if (decimal == MAX1726X_1DECIMAL)
			return MAX1726X_FACTORY_MODE_SOC * 10;
		else
			return MAX1726X_FACTORY_MODE_SOC;
	}
#endif

	ret = max1726x_read_reg(priv, MAX1726X_REPSOC_REG, &val);
	if (ret < 0)
		return ret;

	/* RepSOC LSB: 1/256 % */
	switch (decimal) {
	case MAX1726X_2DECIMAL:
		soc = (val >> 8) * 100 + (val & 0xFF) * 100 / 256;
		ret = min(soc, 10000);
		break;
	case MAX1726X_1DECIMAL:
		soc = (val >> 8) * 10 + (val & 0xFF) * 10 / 256;
		ret = min(soc, 1000);
		break;
	default:
		soc = (val >> 8);
		ret = min(soc, 100);
		break;
	};
	pr_debug("%s: rawsoc:%d (decimal:%d), val(0x%04x)\n", __func__, soc, decimal, val);

	return ret;
}

static int max1726x_read_vfsoc(struct max1726x_priv *priv, int decimal)
{
	unsigned int val;
	int soc;
	int ret;

#if IS_ENABLED(CONFIG_USB_FACTORY_MODE)
	if (priv->f_mode == OB_MODE && priv->vbat_open) {
		pr_debug("%s: %s with vbat_open.\n", __func__, BOOT_MODE_STRING[priv->f_mode]);
		if (decimal == MAX1726X_2DECIMAL)
			return MAX1726X_FACTORY_MODE_SOC * 100;
		else if (decimal == MAX1726X_1DECIMAL)
			return MAX1726X_FACTORY_MODE_SOC * 10;
		else
			return MAX1726X_FACTORY_MODE_SOC;
	}
#endif

	ret = max1726x_read_reg(priv, MAX1726X_VFSOC_REG, &val);
	if (ret < 0)
		return ret;

	/* RepSOC LSB: 1/256 % */
	switch (decimal) {
	case MAX1726X_2DECIMAL:
		soc = (val >> 8) * 100 + (val & 0xFF) * 100 / 256;
		ret = min(soc, 10000);
		break;
	case MAX1726X_1DECIMAL:
		soc = (val >> 8) * 10 + (val & 0xFF) * 10 / 256;
		ret = min(soc, 1000);
		break;
	default:
		soc = (val >> 8);
		ret = min(soc, 100);
		break;
	};

	return ret;
}

static void max1726x_set_vempty(struct max1726x_priv *priv, int vempty_mode)
{
	struct max1726x_platform_data *pdata = priv->pdata;
	u32 val;
	int ret = 0;

#if IS_ENABLED(CONFIG_USB_FACTORY_MODE)
	if (priv->f_mode != NO_MODE && priv->vbat_open) {
		pr_debug("%s: %s with vbat_open.\n", __func__, BOOT_MODE_STRING[priv->f_mode]);
		return;
	}
#endif

	if (!priv->pdata->using_temp_compensation) {
		pr_info("%s: does not use temp compensation, default hw vempty\n",
			__func__);
		vempty_mode = VEMPTY_MODE_HW;
	}

	priv->vempty_mode = vempty_mode;
	switch (vempty_mode) {
	case VEMPTY_MODE_SW:
		/* HW Vempty Disable */
		max1726x_write_reg(priv, MAX1726X_VEMPTY_REG, pdata->vempty_sw_mode);
		/* Reset VALRT Threshold setting (enable) */

		/* Set VAlrtTh */
		val = (pdata->sw_vempty_valrt / 20);
		val |= ((pdata->volt_max / 20) << 8);
		ret = max1726x_write_reg(priv, MAX1726X_VALRTTH_REG, val);
		if (ret < 0) {
			pr_info("%s: Failed to write VALRTTH_REG\n", __func__);
			return;
		}
		ret = max1726x_read_reg(priv, MAX1726X_VALRTTH_REG, &val);
		pr_info("%s: HW V EMPTY Disable, SW V EMPTY Enable. valrt %dmV\n",
			__func__, (val & 0x00ff) * 20);
		break;
	default:
		/* HW Vempty Enable */
		regmap_write(priv->regmap, MAX1726X_VEMPTY_REG, pdata->vempty);
		/* Reset VALRT Threshold setting (disable) */
		val = (pdata->sw_vempty_valrt_cisd / 20);
		val |= ((pdata->volt_max / 20) << 8);
		ret = max1726x_write_reg(priv, MAX1726X_VALRTTH_REG, val);
		if (ret < 0) {
			pr_info("%s: Failed to write VALRTTH_REG\n", __func__);
			return;
		}
		ret = max1726x_read_reg(priv, MAX1726X_VALRTTH_REG, &val);
		pr_info("%s: HW V EMPTY Enable, SW V EMPTY Disable. valrt %dmV\n",
			__func__, (val & 0x00ff) * 20);
		break;
	}
}

static bool max1726x_check_vempty_recover_time(struct max1726x_priv *priv)
{
	struct timespec64 c_ts = {0, };
	unsigned long vempty_time = 0;

	if (!priv->pdata->using_temp_compensation)
		return false;

	c_ts = ktime_to_timespec64(ktime_get_boottime());
	if (!priv->vempty_time)
		priv->vempty_time = (c_ts.tv_sec) ? c_ts.tv_sec : 1;
	else if (c_ts.tv_sec >= priv->vempty_time)
		vempty_time = c_ts.tv_sec - priv->vempty_time;
	else
		vempty_time = 0xFFFFFFFF - priv->vempty_time + c_ts.tv_sec;

	pr_info("%s: check vempty time(%ld, %ld)\n",
		__func__, priv->vempty_time, vempty_time);

	return (vempty_time >= priv->pdata->vempty_recover_time);
}

static unsigned int max1726x_check_vempty_status(struct max1726x_priv *priv, int soc)
{
	if (!priv->pdata->using_temp_compensation)
		return soc;

	/* SW/HW V Empty setting */
	if (priv->vempty_init_flag) {
		if (priv->temperature <= priv->pdata->low_temp_limit) {
			if (priv->raw_soc <= 50 &&
				(priv->vempty_mode != VEMPTY_MODE_HW) &&
				max1726x_check_vempty_recover_time(priv)) {
				max1726x_set_vempty(priv, VEMPTY_MODE_HW);
				priv->vempty_time = 0;
			} else if (priv->raw_soc > 50 &&
				priv->vempty_mode == VEMPTY_MODE_HW) {
				max1726x_set_vempty(priv, VEMPTY_MODE_SW);
				priv->vempty_time = 0;
			}
		} else if (priv->vempty_mode != VEMPTY_MODE_HW &&
			max1726x_check_vempty_recover_time(priv)) {
			max1726x_set_vempty(priv, VEMPTY_MODE_HW);
			priv->vempty_time = 0;
		}
	} else {
		priv->vempty_time = 0;
	}

	if (!priv->is_charging && !lpcharge &&
		priv->vempty_mode == VEMPTY_MODE_SW_VALERT) {
		if (!priv->vempty_time) {
			pr_info("%s : SW V EMPTY. Decrease SOC\n", __func__);
			if (priv->capacity_old > 0)
				soc = priv->capacity_old - 1;
			else
				soc =  0;
		}
	} else if ((priv->vempty_mode == VEMPTY_MODE_SW_RECOVERY)
		&& (soc == priv->capacity_old)) {
		priv->vempty_mode = VEMPTY_MODE_SW;
	}

	return soc;
}

static void max1726x_lost_soc_reset(struct max1726x_priv *priv)
{
	priv->lost_soc.ing = false;
	priv->lost_soc.prev_raw_soc = -1;
	priv->lost_soc.prev_remcap = 0;
	priv->lost_soc.prev_qh = 0;
	priv->lost_soc.lost_cap = 0;
	priv->lost_soc.weight = 0;
}

static void max1726x_lost_soc_check_trigger_cond(
	struct max1726x_priv *priv, int raw_soc, int d_raw_soc, int d_remcap, int d_qh)
{
	struct max1726x_platform_data *pdata = priv->pdata;

	if (priv->lost_soc.prev_raw_soc >= pdata->lost_soc.trig_soc ||
		d_raw_soc <= 0 || d_qh < 0)
		return;

	/*
	 * raw soc is jumped over gap_soc
	 * and remcap is decreased more than trig_scale of qh
	 */
	if (d_raw_soc >= pdata->lost_soc.trig_d_soc &&
		d_remcap >= (d_qh * pdata->lost_soc.trig_scale)) {
		priv->lost_soc.ing = true;
		priv->lost_soc.lost_cap += d_remcap;

		/* calc weight */
		priv->lost_soc.weight += d_raw_soc * 10 / pdata->lost_soc.guarantee_soc;
		if (priv->lost_soc.weight < pdata->lost_soc.min_weight)
			priv->lost_soc.weight = pdata->lost_soc.min_weight;

		pr_info("%s: trigger: raw_soc(%d->%d), d_raw_soc(%d), d_remcap(%d), d_qh(%d), weight(%d.%d)\n",
			__func__, priv->lost_soc.prev_raw_soc, raw_soc, d_raw_soc, d_remcap,
			d_qh, priv->lost_soc.weight / 10, priv->lost_soc.weight % 10);
	}
}

static int max1726x_lost_soc_calc_soc(
	struct max1726x_priv *priv, int request_soc, int d_qh, int d_remcap)
{
	struct max1726x_platform_data *pdata = priv->pdata;
	int lost_soc = 0, gap_cap = 0;
	int vavg = 0, fullcaprep = 0, onecap = 0;

	vavg = max1726x_read_avgvcell(priv, SEC_BATTERY_VOLTAGE_MV);
	fullcaprep = max1726x_read_capacity(priv, MAX1726X_FULLCAPREP_REG);
	if (fullcaprep < 0) {
		fullcaprep = max1726x_lsb_to_capacity(priv, pdata->designcap);
		pr_info("%s: ing: fullcaprep is replaced\n", __func__);
	}
	onecap = (fullcaprep / 100) + 1;

	if (d_qh < 0) {
		/* charging status, recover capacity is delta of remcap */
		if (d_remcap < 0)
			gap_cap = d_remcap * (-1);
		else
			gap_cap = d_remcap;
	} else if (d_qh == 0) {
		gap_cap = 1;
	} else {
		gap_cap = (d_qh * priv->lost_soc.weight / 10);
	}

	if ((vavg < pdata->lost_soc.min_vol) && (vavg > 0) && (gap_cap < onecap)) {
		gap_cap = onecap; /* reduce 1% */
		pr_info("%s: ing: vavg(%d) is under min_vol(%d), reduce cap more(%d)\n",
			__func__, vavg, pdata->lost_soc.min_vol, (fullcaprep / 100));
	}

	priv->lost_soc.lost_cap -= gap_cap;

	if (priv->lost_soc.lost_cap > 0) {
		lost_soc = (priv->lost_soc.lost_cap * 1000) / fullcaprep;
		pr_info("%s: ing: calc_soc(%d), lost_soc(%d), lost_cap(%d), d_qh(%d), d_remcap(%d), weight(%d.%d)\n",
			__func__, request_soc + lost_soc, lost_soc, priv->lost_soc.lost_cap,
			d_qh, d_remcap, priv->lost_soc.weight / 10, priv->lost_soc.weight % 10);
	} else {
		lost_soc = 0;
		max1726x_lost_soc_reset(priv);
		pr_info("%s: done: request_soc(%d), lost_soc(%d), lost_cap(%d)\n",
			__func__, request_soc, lost_soc, priv->lost_soc.lost_cap);
	}

	return lost_soc;
}

static int max1726x_lost_soc_get(struct max1726x_priv *priv, int request_soc)
{
	int raw_soc, remcap, qh; /* now values */
	int d_raw_soc, d_remcap, d_qh; /* delta between prev values */
	int report_soc;

	/* get current values */
	raw_soc = max1726x_read_rawsoc(priv, MAX1726X_1DECIMAL); /* 0.1% */
	remcap = max1726x_read_capacity(priv, MAX1726X_REPCAP_REG);
	qh = max1726x_read_qh(priv);

	if (priv->lost_soc.prev_raw_soc < 0) {
		priv->lost_soc.prev_raw_soc = raw_soc;
		priv->lost_soc.prev_remcap = remcap;
		priv->lost_soc.prev_qh = qh;
		priv->lost_soc.lost_cap = 0;
		pr_info("%s: init: raw_soc(%d), remcap(%d), qh(%d)\n",
			__func__, raw_soc, remcap, qh);

		return request_soc;
	}

	/* get diff values with prev */
	d_raw_soc = priv->lost_soc.prev_raw_soc - raw_soc;
	d_remcap = priv->lost_soc.prev_remcap - remcap;
	d_qh = priv->lost_soc.prev_qh - qh;

	max1726x_lost_soc_check_trigger_cond(priv, raw_soc, d_raw_soc, d_remcap, d_qh);

	/* backup prev values */
	priv->lost_soc.prev_raw_soc = raw_soc;
	priv->lost_soc.prev_remcap = remcap;
	priv->lost_soc.prev_qh = qh;

	if (!priv->lost_soc.ing)
		return request_soc;

	report_soc = request_soc + max1726x_lost_soc_calc_soc(priv, request_soc, d_qh, d_remcap);

	if (report_soc > 1000)
		report_soc = 1000;
	if (report_soc < 0)
		report_soc = 0;

	return report_soc;
}

static void max1726x_check_temperature_wa(struct max1726x_priv *priv, int temp)
{ /* temp is 0.1DegreeC */
	static bool low_temp_wa;
	unsigned int val;
	int ret;

	if (!priv->pdata->using_temp_compensation)
		return;

	if (temp < 0) {
		ret = max1726x_read_reg(priv, MAX1726X_DESIGNCAP_REG, &val);
		if (ret < 0)
			return;

		if (val == priv->pdata->designcap) {
			regmap_write(priv->regmap, MAX1726X_DESIGNCAP_REG, priv->pdata->designcap + 3);
			pr_info("%s: set the low temp reset! temp:%d, set cap:0x%x, orig cap:0x%x\n",
				__func__, temp, priv->pdata->designcap + 3, priv->pdata->designcap);
		}
	}

	if (temp < 0 && !low_temp_wa) {
		low_temp_wa = true;
		max1726x_update_bits(priv, MAX1726X_FILTERCFG_REG,
			MAX1726X_FILTERCFG_CURR_MASK, (0x7 << MAX1726X_FILTERCFG_CURR_SHIFT));
		max1726x_read_reg(priv, MAX1726X_FILTERCFG_REG, &val);
		pr_info("%s: temp(%d), FilterCFG(0x%04x)\n", __func__, temp, val);
	} else if (temp > 30 && low_temp_wa) {
		low_temp_wa = false;
		max1726x_update_bits(priv, MAX1726X_FILTERCFG_REG,
			MAX1726X_FILTERCFG_CURR_MASK, (0x4 << MAX1726X_FILTERCFG_CURR_SHIFT));
		max1726x_read_reg(priv, MAX1726X_FILTERCFG_REG, &val);
		pr_info("%s: temp(%d), FilterCFG(0x%04x)\n", __func__, temp, val);
	}
}

static void max1726x_fg_adjust_capacity_max(
	struct max1726x_priv *priv)
{
	struct timespec64 c_ts = {0, };
	static struct timespec64 old_ts = {0, };

	if (priv->capacity_max_conv) {
		c_ts = ktime_to_timespec64(ktime_get_boottime());
		pr_info("%s: capacit max conv time(%llu)\n",
			__func__, c_ts.tv_sec - old_ts.tv_sec);

		if ((priv->capacity_max < priv->g_capacity_max) &&
			((unsigned long)(c_ts.tv_sec - old_ts.tv_sec) >= 60)) {
			priv->capacity_max++;
			old_ts = c_ts;
		} else if (priv->capacity_max >= priv->g_capacity_max) {
			priv->g_capacity_max = 0;
			priv->capacity_max_conv = false;
		}
		pr_info("%s: capacity_max_conv(%d) Capacity Max(%d | %d)\n",
			__func__, priv->capacity_max_conv,
			priv->capacity_max, priv->g_capacity_max);
	}
}

static unsigned int max1726x_get_scaled_capacity(
	struct max1726x_priv *priv, unsigned int soc)
{
	struct max1726x_platform_data *pdata = priv->pdata;

	soc = (soc < pdata->capacity_min) ?
		0 : ((soc - pdata->capacity_min) * 1000 /
		(priv->capacity_max - pdata->capacity_min));

	pr_info("%s : capacity_max (%d) scaled capacity(%d.%d), raw_soc(%d.%d)\n",
		__func__, priv->capacity_max, soc / 10, soc % 10,
		soc / 10, soc % 10);

	return soc;
}

/* capacity is integer */
static unsigned int max1726x_get_atomic_capacity(
	struct max1726x_priv *priv, unsigned int soc)
{
	if (priv->capacity_old < soc) {
		pr_info("%s: capacity (old %d : new %d)\n",
			__func__, priv->capacity_old, soc);
		soc = priv->capacity_old + 1;
	} else if (priv->capacity_old > soc) {
		pr_info("%s: capacity (old %d : new %d)\n",
			__func__, priv->capacity_old, soc);
		soc = priv->capacity_old - 1;
	}

	/* updated old capacity */
	priv->capacity_old = soc;

	return soc;
}

static unsigned int max1726x_get_skip_abnormal_capacity(
	struct max1726x_priv *priv, unsigned int soc)
{
	/* keep SOC stable in abnormal status */
	if (!priv->is_charging && priv->capacity_old < soc) {
		pr_info("%s: capacity (old %d : new %d)\n",
			__func__, priv->capacity_old, soc);
		soc = priv->capacity_old;
	}

	/* updated old capacity */
	priv->capacity_old = soc;

	return soc;
}

static int max1726x_check_capacity_max(
	struct max1726x_priv *priv, int capacity_max)
{
	int cap_max, cap_min;

	cap_max = priv->pdata->capacity_max;
	cap_min = priv->pdata->capacity_max - priv->pdata->capacity_max_margin;

	return (capacity_max < cap_min) ? cap_min :
		((capacity_max >= cap_max) ? cap_max : capacity_max);
}

static void max1726x_calculate_dynamic_scale(
	struct max1726x_priv *priv, int capacity, bool scale_by_full)
{
	struct max1726x_platform_data *pdata = priv->pdata;
	int min_cap = pdata->capacity_max - pdata->capacity_max_margin;
	int raw_soc;

	if ((capacity > 100) || ((capacity * 10) < min_cap)) {
		pr_err("%s: invalid capacity(%d)\n", __func__, capacity);
		return;
	}

	raw_soc = max1726x_read_rawsoc(priv, MAX1726X_1DECIMAL); /* 0.1% */
	if ((raw_soc < min_cap) || priv->capacity_max_conv) {
		pr_info("%s: skip routine - raw_soc(%d), min_cap(%d), capacity_max_conv(%d)\n",
			__func__, raw_soc, min_cap, priv->capacity_max_conv);
		return;
	}

	priv->capacity_max = (raw_soc * 100 / (capacity + 1));
	priv->capacity_old = capacity;

	priv->capacity_max = max1726x_check_capacity_max(priv, priv->capacity_max);

	pr_info("%s: %d is used for capacity_max, capacity(%d)\n",
		__func__, priv->capacity_max, capacity);
	if ((capacity == 100) && !priv->capacity_max_conv && scale_by_full) {
		priv->capacity_max_conv = true;
		priv->g_capacity_max = raw_soc;
		pr_info("%s: Goal capacity max %d\n", __func__, priv->g_capacity_max);
	}
}

static int max1726x_reset_capacity(struct max1726x_priv *priv)
{
	struct max1726x_platform_data *pdata = priv->pdata;
	int ret = 0;
	int vcell, ocv, soc, vfsoc, inow, iavg;
	u32 val = 0;
	int por = 0;

#if IS_ENABLED(CONFIG_USB_FACTORY_MODE)
	if (priv->f_mode == NO_MODE) {
		pr_info("%s: skip. %s\n", __func__, BOOT_MODE_STRING[priv->f_mode]);
		return -1;
	}
#else
	if (priv->is_charging) {
		/* TODO: check condition more */
		pr_info("%s : skip. is_charging\n", __func__);
		return 0;
	}
#endif

	ret = max1726x_read_reg(priv, MAX1726X_STATUS_REG, &val);
	if (ret < 0) {
		pr_err("%s: failed to read STATUS_REG\n", __func__);
	} else {
		por = !!(val & 0x2);
		pr_info("%s : status(0x%04x), por(%d)\n", __func__, val, por);
	}

	msleep(500);

	vcell = max1726x_read_vcell(priv, SEC_BATTERY_VOLTAGE_MV);
	ocv = max1726x_read_ocv(priv, SEC_BATTERY_VOLTAGE_MV);
	soc = max1726x_read_rawsoc(priv, MAX1726X_1DECIMAL); /* 0.1% */
	vfsoc = max1726x_read_vfsoc(priv, MAX1726X_1DECIMAL); /* 0.1% */
	inow = max1726x_read_current(priv, SEC_BATTERY_CURRENT_MA);
	iavg = max1726x_read_avgcurrent(priv, SEC_BATTERY_CURRENT_MA);

	pr_info("%s: Before quick-start - vcell(%d), ocv(%d), soc(%d), vfsoc(%d), inow(%d), iavg(%d)\n",
		__func__, vcell, ocv, soc, vfsoc, inow, iavg);

	regmap_write(priv->regmap, MAX1726X_CYCLES_REG, 0);

	ret = max1726x_update_bits(priv, MAX1726X_MISCCFG_REG,
				(0x1 << 10), (0x1 << 10));
	if (ret < 0) {
		pr_err("%s: failed to write MISCCFG_REG\n", __func__);
		return ret;
	}

	msleep(250);
	if (!por) {
		regmap_write(priv->regmap, MAX1726X_FULLCAPREP_REG, pdata->designcap);
		regmap_write(priv->regmap, MAX1726X_FULLCAP_REG, pdata->designcap);
		msleep(500);
	}

	vcell = max1726x_read_vcell(priv, SEC_BATTERY_VOLTAGE_MV);
	ocv = max1726x_read_ocv(priv, SEC_BATTERY_VOLTAGE_MV);
	soc = max1726x_read_rawsoc(priv, MAX1726X_1DECIMAL); /* 0.1% */
	vfsoc = max1726x_read_vfsoc(priv, MAX1726X_1DECIMAL); /* 0.1% */
	inow = max1726x_read_current(priv, SEC_BATTERY_CURRENT_MA);
	iavg = max1726x_read_avgcurrent(priv, SEC_BATTERY_CURRENT_MA);

	pr_info("%s: After quick-start - vcell(%d), ocv(%d), soc(%d), vfsoc(%d), inow(%d), iavg(%d)\n",
		__func__, vcell, ocv, soc, vfsoc, inow, iavg);

	regmap_write(priv->regmap, MAX1726X_CYCLES_REG, 0x00A0);

	return 0;
}

static int max1726x_get_ui_soc(struct max1726x_priv *priv, int type)
{
	int soc;

	switch (type) {
	case SEC_FUELGAUGE_CAPACITY_TYPE_RAW:
		soc = max1726x_read_rawsoc(priv, MAX1726X_2DECIMAL); /* 0.01% */
		break;
	case SEC_FUELGAUGE_CAPACITY_TYPE_CAPACITY_POINT:
		soc = priv->raw_soc % 10;
		break;
	case SEC_FUELGAUGE_CAPACITY_TYPE_DYNAMIC_SCALE:
		soc = priv->raw_soc;
		break;
	default:
		soc = max1726x_read_rawsoc(priv, MAX1726X_1DECIMAL); /* 0.1% */

		if (priv->pdata->capacity_calculation_type & SEC_FUELGAUGE_CAPACITY_TYPE_DYNAMIC_SCALE) {
			max1726x_fg_adjust_capacity_max(priv);
			soc = max1726x_get_scaled_capacity(priv, soc);

			if (soc > 1010) {
				pr_info("%s: scaled capacity (%d)\n", __func__, soc);
				max1726x_calculate_dynamic_scale(priv, 100, false);
			}
		}

		/* capacity should be between 0% and 100% (0.1% degree) */
		soc = min(soc, 1000);
		soc = max(soc, 0);
		priv->raw_soc = soc;

		if (priv->pdata->capacity_calculation_type & SEC_FUELGAUGE_CAPACITY_TYPE_LOST_SOC)
			soc = max1726x_lost_soc_get(priv, priv->raw_soc);

		/* get only integer part */
		soc /= 10;

		soc = max1726x_check_vempty_status(priv, soc);

		/* (Only for atomic capacity)
		 * In initial time, capacity_old is 0.
		 * and in resume from sleep,
		 * capacity_old is too different from actual soc.
		 * should update capacity_old
		 * by soc in booting or resume.
		 */
		if (priv->initial_update_of_soc) {
			priv->initial_update_of_soc = false;
			if (priv->vempty_mode != VEMPTY_MODE_SW_VALERT) {
				/* updated old capacity */
				priv->capacity_old = soc;

				return soc;
			}
		}

		if (priv->pdata->capacity_calculation_type & SEC_FUELGAUGE_CAPACITY_TYPE_ATOMIC)
			soc = max1726x_get_atomic_capacity(priv, soc);

		if (priv->pdata->capacity_calculation_type & SEC_FUELGAUGE_CAPACITY_TYPE_SKIP_ABNORMAL)
			soc = max1726x_get_skip_abnormal_capacity(priv, soc);

		break;
	};

	return soc;
}

static int max1726x_get_asoc(struct max1726x_priv *priv)
{
	int fullcapnom, designcap;
	int asoc;

#if IS_ENABLED(CONFIG_USB_FACTORY_MODE)
	if (priv->f_mode != NO_MODE && priv->vbat_open) {
		pr_debug("%s: %s with vbat_open.\n", __func__, BOOT_MODE_STRING[priv->f_mode]);
		return 100;
	}
#endif

	designcap = max1726x_lsb_to_capacity(priv, priv->pdata->designcap);
	fullcapnom = max1726x_read_capacity(priv, MAX1726X_FULLCAPNOM_REG);
	asoc = designcap * 100 / fullcapnom;
	pr_info("%s: asoc(%d), fullcap(%d)\n", __func__, asoc, fullcapnom);

	return asoc;
}

static int max1726x_get_property(struct power_supply *psy,
			enum power_supply_property psp,
			union power_supply_propval *val)
{
	struct max1726x_priv *priv = power_supply_get_drvdata(psy);
	struct max1726x_platform_data *pdata = priv->pdata;
	enum power_supply_ext_property ext_psp = (enum power_supply_ext_property) psp;
	unsigned int reg;
	int ret, unit;

	switch ((int)psp) {
	case POWER_SUPPLY_PROP_PRESENT:
		ret = max1726x_read_reg(priv, MAX1726X_STATUS_REG, &reg);
		if (ret < 0)
			return ret;
		if (reg & MAX1726X_STATUS_BST)
			val->intval = 0;
		else
			val->intval = 1;
		break;

	case POWER_SUPPLY_PROP_CYCLE_COUNT:
		ret = max1726x_read_reg(priv, MAX1726X_CYCLES_REG, &reg);
		if (ret < 0)
			return ret;

		val->intval = reg;
		break;

	case POWER_SUPPLY_PROP_VOLTAGE_MAX:
		ret = max1726x_read_reg(priv, MAX1726X_MAXMINVOLT_REG, &reg);
		if (ret < 0)
			return ret;

		val->intval = reg >> 8;
		val->intval *= 20000; /* Units of LSB = 20mV */
		break;

	case POWER_SUPPLY_PROP_VOLTAGE_MIN_DESIGN:
		ret = max1726x_read_reg(priv, MAX1726X_VEMPTY_REG, &reg);
		if (ret < 0)
			return ret;

		val->intval = reg >> 7;
		val->intval *= 10000; /* Units of LSB = 10mV */
		break;

	case POWER_SUPPLY_PROP_STATUS:
		if (pdata && pdata->get_charging_status)
			val->intval = pdata->get_charging_status();
		else
			val->intval = POWER_SUPPLY_STATUS_UNKNOWN;
		break;

	case POWER_SUPPLY_PROP_VOLTAGE_NOW:
		unit = val->intval;
		val->intval = max1726x_read_vcell(priv, unit);
		break;

	case POWER_SUPPLY_PROP_VOLTAGE_AVG:
		unit = val->intval;
		val->intval = max1726x_read_avgvcell(priv, unit);
		break;

	case POWER_SUPPLY_PROP_VOLTAGE_OCV:
		unit = val->intval;
		val->intval = max1726x_read_ocv(priv, unit);
		break;

	case POWER_SUPPLY_PROP_CAPACITY:
		val->intval = max1726x_get_ui_soc(priv, val->intval);
		break;

	case POWER_SUPPLY_PROP_CHARGE_FULL:
		ret = max1726x_read_reg(priv, MAX1726X_FULLCAPREP_REG, &reg);
		if (ret < 0)
			return ret;

		val->intval = (reg * 1000) >> 1; /* FullCAPRep LSB: 0.5 mAh */
		break;

	case POWER_SUPPLY_PROP_CHARGE_COUNTER:
		val->intval = priv->raw_soc *
			max1726x_lsb_to_capacity(priv, priv->pdata->designcap);
		break;

	case POWER_SUPPLY_PROP_CHARGE_NOW:
		ret = max1726x_read_reg(priv, MAX1726X_REPCAP_REG, &reg);
		if (ret < 0)
			return ret;

		val->intval = (reg * 1000) >> 1; /* RepCAP LSB: 0.5 mAh */
		break;

	case POWER_SUPPLY_PROP_TEMP:
		ret = max1726x_get_temperature(priv, &val->intval);
		if (ret < 0)
			return ret;

		val->intval *= 10; /* Convert 1DegreeC LSB to 0.1DegreeC LSB */
		break;

	case POWER_SUPPLY_PROP_TEMP_ALERT_MIN:
		ret = max1726x_get_temperature_limit(priv, &val->intval, MIN_TEMP_ALERT);
		if (ret < 0)
			return ret;

		val->intval *= 10; /* Convert 1DegreeC LSB to 0.1DegreeC LSB */
		break;

	case POWER_SUPPLY_PROP_TEMP_ALERT_MAX:
		ret = max1726x_get_temperature_limit(priv, &val->intval, MAX_TEMP_ALERT);
		if (ret < 0)
			return ret;

		val->intval *= 10; /* Convert 1DegreeC LSB to 0.1DegreeC LSB */
		break;

	case POWER_SUPPLY_PROP_HEALTH:
		ret = max1726x_get_battery_health(priv, &val->intval);
		if (ret < 0)
			return ret;
		break;

	case POWER_SUPPLY_PROP_CURRENT_NOW:
		unit = val->intval;
		val->intval = max1726x_read_current(priv, unit);
		break;

	case POWER_SUPPLY_PROP_CURRENT_AVG:
		unit = val->intval;
		val->intval = max1726x_read_avgcurrent(priv, unit);
		break;

	case POWER_SUPPLY_PROP_TIME_TO_EMPTY_AVG:
		ret = max1726x_read_reg(priv, MAX1726X_TTE_REG, &reg);
		if (ret < 0)
			return ret;

		val->intval = (reg * 45) >> 3; /* TTE LSB: 5.625 sec */
		break;

	case POWER_SUPPLY_PROP_TIME_TO_FULL_NOW:
		ret = max1726x_read_reg(priv, MAX1726X_TTF_REG, &reg);
		if (ret < 0)
			return ret;

		val->intval = (reg * 45) >> 3; /* TTE LSB: 5.625 sec */
		break;

	case POWER_SUPPLY_PROP_ENERGY_FULL:
		val->intval = max1726x_get_asoc(priv);
#if !defined(CONFIG_SEC_FACTORY)
		max1726x_periodic_read(priv);
#endif
		break;

	case POWER_SUPPLY_PROP_ENERGY_FULL_DESIGN:
		val->intval = priv->capacity_max;
		break;

	case POWER_SUPPLY_PROP_ENERGY_NOW:
		switch (val->intval) {
		case SEC_BATTERY_CAPACITY_QH:
			val->intval = max1726x_read_qh(priv) * 1000; /* uAh */
			break;
		case SEC_BATTERY_CAPACITY_AGEDCELL:
			val->intval = max1726x_read_capacity(priv, MAX1726X_FULLCAPNOM_REG);
			break;
		case SEC_BATTERY_CAPACITY_FULL:
			val->intval = max1726x_read_capacity(priv, MAX1726X_FULLCAPREP_REG);
			break;
		default:
			return -EINVAL;
		}
		break;
#if defined(CONFIG_BATTERY_AGE_FORECAST)
	case POWER_SUPPLY_PROP_CAPACITY_LEVEL:
		return -ENODATA;
#endif
	case POWER_SUPPLY_EXT_PROP_MIN ... POWER_SUPPLY_EXT_PROP_MAX:
		switch (ext_psp) {
		case POWER_SUPPLY_EXT_PROP_MONITOR_WORK:
#if !defined(CONFIG_SEC_FACTORY)
			max1726x_periodic_read(priv);
#endif
			break;

		case POWER_SUPPLY_EXT_PROP_CHARGING_ENABLED:
			val->intval = priv->is_charging;
			break;
		case POWER_SUPPLY_EXT_PROP_CHECK_INIT:
			val->intval = priv->vbat_open ? 1 : 0;
			break;
		case POWER_SUPPLY_EXT_PROP_BATT_DUMP:
			val->strval = "FG LOG";
			break;
		default:
			return -EINVAL;
		}
		break;
	default:
		return -EINVAL;
	}
	return 0;
}

static int max1726x_set_property(struct power_supply *psy,
			enum power_supply_property psp,
			const union power_supply_propval *val)
{
	struct max1726x_priv *priv = power_supply_get_drvdata(psy);
	struct max1726x_platform_data *pdata = priv->pdata;
	enum power_supply_ext_property ext_psp = (enum power_supply_ext_property) psp;
	//unsigned int reg;
	int ret = 0;

	switch ((int)psp) {
	case POWER_SUPPLY_PROP_CHARGE_FULL:
		if (pdata->capacity_calculation_type & SEC_FUELGAUGE_CAPACITY_TYPE_DYNAMIC_SCALE)
			max1726x_calculate_dynamic_scale(priv, val->intval, true);
		break;


	case POWER_SUPPLY_PROP_ONLINE:
		priv->cable_type = val->intval;
		if (!is_nocharge_type(priv->cable_type)) {
			/* enable alert */
			if (priv->vempty_mode >= VEMPTY_MODE_SW_VALERT) {
				max1726x_set_vempty(priv, VEMPTY_MODE_HW);
				priv->initial_update_of_soc = true;
			}
		}
		break;

	case POWER_SUPPLY_PROP_CAPACITY:
		if (val->intval == SEC_FUELGAUGE_CAPACITY_TYPE_RESET) {
			if (max1726x_reset_capacity(priv))
				return -EINVAL;
			priv->initial_update_of_soc = true;
		}
		break;

	case POWER_SUPPLY_PROP_TEMP:
		max1726x_check_temperature_wa(priv, val->intval);
		max1726x_set_temperature(priv, val->intval);
		break;

	case POWER_SUPPLY_PROP_TEMP_AMBIENT:
		break;

	case POWER_SUPPLY_PROP_ENERGY_FULL_DESIGN:
		if (priv->pdata->capacity_calculation_type & SEC_FUELGAUGE_CAPACITY_TYPE_DYNAMIC_SCALE) {
			pr_info("%s: capacity_max changed, %d -> %d\n",
				__func__, priv->capacity_max, val->intval);
			priv->capacity_max =
				max1726x_check_capacity_max(priv, val->intval);
			priv->initial_update_of_soc = true;
		}
		break;

#if defined(CONFIG_BATTERY_AGE_FORECAST)
	case POWER_SUPPLY_PROP_CAPACITY_LEVEL:
	{
		u32 reg_fullsocthr;
		int val_soc = val->intval;

		if (val->intval > priv->pdata->full_condition_soc
		    || val->intval <= (priv->pdata->full_condition_soc - 10)) {
			pr_info("%s: abnormal value(%d). so thr is changed to default(%d)\n",
			     __func__, val->intval, priv->pdata->full_condition_soc);
			val_soc = priv->pdata->full_condition_soc;
		}

		reg_fullsocthr = (val_soc << MAX1726X_FULLSOC_SHIFT);
			/* As per Ted (Maxim) : You don't need to write lower byte for FullSOCTHR */
		if (max1726x_update_bits(priv, MAX1726X_FULLSOCTHR_REG, MAX1726X_FULLSOC_MASK, reg_fullsocthr) < 0) {
			pr_info("%s: Failed to write FULLSOCTHR_REG\n", __func__);
		} else {
			max1726x_read_reg(priv, MAX1726X_FULLSOCTHR_REG, &reg_fullsocthr);
			pr_info("%s: FullSOCThr %d%%(0x%04X)\n",
				__func__, val_soc, reg_fullsocthr);
		}
	}
		break;
#endif

	case POWER_SUPPLY_EXT_PROP_MIN ... POWER_SUPPLY_EXT_PROP_MAX:
		switch (ext_psp) {
		case POWER_SUPPLY_EXT_PROP_CHARGING_ENABLED:
			switch (val->intval) {
			case SEC_BAT_CHG_MODE_BUCK_OFF:
			case SEC_BAT_CHG_MODE_CHARGING_OFF:
				priv->is_charging = false;
				break;
			case SEC_BAT_CHG_MODE_CHARGING:
				priv->is_charging = true;
				break;
			};
			break;

#if IS_ENABLED(CONFIG_USB_FACTORY_MODE)
		case POWER_SUPPLY_EXT_PROP_BATT_F_MODE:
			priv->f_mode = val->intval;
			break;
#endif

		default:
			return -EINVAL;
		}
		break;
	default:
		return -EINVAL;
	}
	return ret;
}

static void max1726x_volt_min_alrt(struct max1726x_priv *priv)
{
	if (lpcharge || priv->is_charging)
		return;

	pr_info("%s: Battery Voltage is Very Low!! V EMPTY(%d)\n",
		__func__, priv->vempty_mode);

	if (priv->vempty_mode != VEMPTY_MODE_HW)
		priv->vempty_mode = VEMPTY_MODE_SW_VALERT;
#if defined(CONFIG_BATTERY_CISD)
	else {
		if (!priv->valert_count_flag) {
			union power_supply_propval value;

			value.intval = priv->vempty_mode;
			psy_do_property("battery", set,
					POWER_SUPPLY_PROP_VOLTAGE_MIN, value);
			priv->valert_count_flag = true;
		}
	}
#endif
}

static irqreturn_t max1726x_irq_handler(int id, void *dev)
{
	struct max1726x_priv *priv = dev;
	u32 val;

	/* Check alert type */
	max1726x_read_reg(priv, MAX1726X_STATUS_REG, &val);

	if (val & MAX1726X_STATUS_SOC_MAX_ALRT)
		dev_info(priv->dev, "Alert: SOC MAX!\n");
	if (val & MAX1726X_STATUS_SOC_MIN_ALRT)
		dev_info(priv->dev, "Alert: SOC MIN!\n");
	if (val & MAX1726X_STATUS_TEMP_MAX_ALRT)
		dev_info(priv->dev, "Alert: TEMP MAX!\n");
	if (val & MAX1726X_STATUS_TEMP_MIN_ALRT)
		dev_info(priv->dev, "Alert: TEMP MIN!\n");
	if (val & MAX1726X_STATUS_VOLT_MAX_ALRT)
		dev_info(priv->dev, "Alert: VOLT MAX!\n");
	if (val & MAX1726X_STATUS_VOLT_MIN_ALRT) {
		dev_info(priv->dev, "Alert: VOLT MIN!\n");
		max1726x_volt_min_alrt(priv);
	}
	if (val & MAX1726X_STATUS_CURR_MAX_ALRT)
		dev_info(priv->dev, "Alert: CURR MAX!\n");
	if (val & MAX1726X_STATUS_CURR_MIN_ALRT)
		dev_info(priv->dev, "Alert: CURR MIN!\n");

	/* Clear alerts */
	regmap_write(priv->regmap, MAX1726X_STATUS_REG,
				val & MAX1726X_STATUS_ALRT_CLR_MASK);

	/* power_supply_changed(priv->psy_fg); */
	return IRQ_HANDLED;
}

static void max1726x_set_alert_thresholds(struct max1726x_priv *priv)
{
	struct max1726x_platform_data *pdata = priv->pdata;
	u32 val;

	/* Set VAlrtTh */
	val = (pdata->volt_min / 20);
	val |= ((pdata->volt_max / 20) << 8);
	max1726x_write_reg(priv, MAX1726X_VALRTTH_REG, val);

	/* Set TAlrtTh */
	val = pdata->temp_min & 0xFF;
	val |= ((pdata->temp_max & 0xFF) << 8);
	max1726x_write_reg(priv, MAX1726X_TALRTTH_REG, val);

	/* Set SAlrtTh */
	val = pdata->soc_min;
	val |= (pdata->soc_max << 8);
	max1726x_write_reg(priv, MAX1726X_SALRTTH_REG, val);

	/* Set IAlrtTh */
	val = (pdata->curr_min * pdata->rsense / 400) & 0xFF;
	val |= (((pdata->curr_max * pdata->rsense / 400) & 0xFF) << 8);
	max1726x_write_reg(priv, MAX1726X_IALRTTH_REG, val);
}

static int max1726x_init(struct max1726x_priv *priv)
{
	int ret = 0;
	u32 fgrev;

	ret = max1726x_read_reg(priv, MAX1726X_VERSION_REG, &fgrev);
	if (ret < 0)
		return -ENODEV;

	dev_info(priv->dev, "IC Version: 0x%04x\n", fgrev);

	/* Optional step - alert threshold initialization */
	max1726x_set_alert_thresholds(priv);

	/* external temperature measurement */
	max1726x_update_bits(priv, MAX1726X_CONFIG_REG,
		MAX1726X_CONFIG_TEX_EN, MAX1726X_CONFIG_TEX_EN);

	return ret;
}

static void max1726x_priv_init(struct max1726x_priv *priv)
{
	int raw_soc;

	if (priv->pdata->capacity_calculation_type & SEC_FUELGAUGE_CAPACITY_TYPE_LOST_SOC)
		max1726x_lost_soc_reset(priv);

	if (priv->pdata->capacity_calculation_type & SEC_FUELGAUGE_CAPACITY_TYPE_DYNAMIC_SCALE) {
		priv->capacity_max = priv->pdata->capacity_max;
		priv->g_capacity_max = 0;
		priv->capacity_max_conv = false;

		raw_soc = max1726x_read_rawsoc(priv, MAX1726X_1DECIMAL); /* 0.1% */
		if (raw_soc > priv->capacity_max)
			max1726x_calculate_dynamic_scale(priv, 100, false);
	}

	/* SW/HW init code. SW/HW V Empty mode must be opposite ! */
	priv->vempty_init_flag = false;	/* default value */
	pr_info("%s: SW/HW V empty init\n", __func__);
	max1726x_set_vempty(priv, VEMPTY_MODE_SW);
#if defined(CONFIG_BATTERY_CISD)
	priv->valert_count_flag = false;
#endif
	priv->initial_update_of_soc = true;
}

static void max1726x_parse_dt_lost_soc(
	struct max1726x_platform_data *pdata, struct device_node *np)
{
	int ret;

	ret = of_property_read_u32(np, "fuelgauge,lost_soc_trig_soc",
				 &pdata->lost_soc.trig_soc);
	if (ret < 0)
		pdata->lost_soc.trig_soc = 1000; /* 100% */

	ret = of_property_read_u32(np, "fuelgauge,lost_soc_trig_d_soc",
				 &pdata->lost_soc.trig_d_soc);
	if (ret < 0)
		pdata->lost_soc.trig_d_soc = 20; /* 2% */

	ret = of_property_read_u32(np, "fuelgauge,lost_soc_trig_scale",
				 &pdata->lost_soc.trig_scale);
	if (ret < 0)
		pdata->lost_soc.trig_scale = 2; /* 2x */

	ret = of_property_read_u32(np, "fuelgauge,lost_soc_guarantee_soc",
				 &pdata->lost_soc.guarantee_soc);
	if (ret < 0)
		pdata->lost_soc.guarantee_soc = 20; /* 2% */

	ret = of_property_read_u32(np, "fuelgauge,lost_soc_min_vol",
				 &pdata->lost_soc.min_vol);
	if (ret < 0)
		pdata->lost_soc.min_vol = 3200; /* 3.2V */

	ret = of_property_read_u32(np, "fuelgauge,lost_soc_min_weight",
				 &pdata->lost_soc.min_weight);
	if (ret < 0 || pdata->lost_soc.min_weight <= 10)
		pdata->lost_soc.min_weight = 20; /* 2.0 */

	pr_info("%s: trigger soc(%d), d_soc(%d), scale(%d), guarantee_soc(%d), min_vol(%d), min_weight(%d)\n",
		__func__, pdata->lost_soc.trig_soc, pdata->lost_soc.trig_d_soc,
		pdata->lost_soc.trig_scale, pdata->lost_soc.guarantee_soc,
		pdata->lost_soc.min_vol, pdata->lost_soc.min_weight);
}

static struct max1726x_platform_data *max1726x_parse_dt(struct device *dev)
{
	struct device_node *np = dev->of_node;
	struct max1726x_platform_data *pdata;
	int ret;

	pdata = devm_kzalloc(dev, sizeof(*pdata), GFP_KERNEL);
	if (!pdata)
		return NULL;

	ret = of_property_read_u32(np, "fuelgauge,talrt-min", &pdata->temp_min);
	if (ret)
		pdata->temp_min = -128; /* DegreeC */ /* Disable alert */

	ret = of_property_read_u32(np, "fuelgauge,talrt-max", &pdata->temp_max);
	if (ret)
		pdata->temp_max = 127; /* DegreeC */ /* Disable alert */

	ret = of_property_read_u32(np, "fuelgauge,valrt-min", &pdata->volt_min);
	if (ret)
		pdata->volt_min = 0; /* mV */ /* Disable alert */

	ret = of_property_read_u32(np, "fuelgauge,valrt-max", &pdata->volt_max);
	if (ret)
		pdata->volt_max = 5100; /* mV */ /* Disable alert */

	ret = of_property_read_u32(np, "fuelgauge,ialrt-min", &pdata->curr_min);
	if (ret)
		pdata->curr_min = -5120; /* mA */ /* Disable alert */

	ret = of_property_read_u32(np, "fuelgauge,ialrt-max", &pdata->curr_max);
	if (ret)
		pdata->curr_max = 5080; /* mA */ /* Disable alert */

	ret = of_property_read_u32(np, "fuelgauge,salrt-min", &pdata->soc_min);
	if (ret)
		pdata->soc_min = 0; /* Percent */ /* Disable alert */

	ret = of_property_read_u32(np, "fuelgauge,salrt-max", &pdata->soc_max);
	if (ret)
		pdata->soc_max = 255; /* Percent */ /* Disable alert */


	ret = of_property_read_u32(np, "fuelgauge,rsense", &pdata->rsense);
	if (ret)
		pdata->rsense = 10;

	ret = of_property_read_u16(np, "fuelgauge,vempty", &pdata->vempty);
	if (ret)
		pdata->vempty = 0xA561;

	ret = of_property_read_u16(np, "fuelgauge,designcap", &pdata->designcap);
	if (ret)
		pdata->designcap = 0x0BB8;

	pr_info("%s: rsense:%d, designcap:0x%04x\n",
		__func__, pdata->rsense, pdata->designcap);

	/*** SEC ***/
	ret = of_property_read_u32(np, "fuelgauge,capacity_calculation_type",
				&pdata->capacity_calculation_type);
	if (ret)
		pdata->capacity_calculation_type = 0;

	ret = of_property_read_u32(np, "fuelgauge,capacity_max",
				&pdata->capacity_max);
	if (ret)
		pdata->capacity_max = 1000;

	ret = of_property_read_u32(np, "fuelgauge,capacity_max_margin",
				&pdata->capacity_max_margin);
	if (ret)
		pdata->capacity_max_margin = 300;

	ret = of_property_read_u32(np, "fuelgauge,capacity_min",
				&pdata->capacity_min);
	if (ret)
		pdata->capacity_min = 0;

	pr_info("%s: calculation_type: 0x%x\n",
		__func__, pdata->capacity_calculation_type);

	if (pdata->capacity_calculation_type & SEC_FUELGAUGE_CAPACITY_TYPE_DYNAMIC_SCALE)
		pr_info("%s: capacity_max:%d, capacity_min:%d, capacity_max_margin:%d\n",
			__func__, pdata->capacity_max, pdata->capacity_min, pdata->capacity_max_margin);

	pdata->using_temp_compensation = of_property_read_bool(np,
				"fuelgauge,using_temp_compensation");
	if (pdata->using_temp_compensation) {
		ret = of_property_read_u32(np, "fuelgauge,low_temp_limit",
					 &pdata->low_temp_limit);
		if (ret)
			pdata->low_temp_limit = 0; /* Default: 0'C */

		ret = of_property_read_u32(np, "fuelgauge,vempty_recover_time",
					&pdata->vempty_recover_time);
		if (ret)
			pdata->vempty_recover_time = 0; /* Default: 0 */

		ret = of_property_read_u16(np, "fuelgauge,vempty_sw_mode", &pdata->vempty_sw_mode);
		if (ret)
			pdata->vempty_sw_mode = 0x7D54;

		ret = of_property_read_u32(np, "fuelgauge,sw_vempty_valrt", &pdata->sw_vempty_valrt);
		if (ret)
			pdata->sw_vempty_valrt = pdata->volt_min;

		ret = of_property_read_u32(np, "fuelgauge,sw_vempty_valrt_cisd", &pdata->sw_vempty_valrt_cisd);
		if (ret)
			pdata->sw_vempty_valrt_cisd = pdata->volt_min;

		ret = of_property_read_u32(np, "fuelgauge,sw_vempty_recover_vol", &pdata->sw_vempty_recover_vol);
		if (ret)
			pdata->sw_vempty_recover_vol = 0;

		pr_info("%s: low_temp_limit:%d, vempty_recover_time:%d, vempty_sw_mode:0x%04x, sw_vempty_valrt:%d/recover:%d\n",
			__func__, pdata->low_temp_limit, pdata->vempty_recover_time, pdata->vempty_sw_mode,
			pdata->sw_vempty_valrt, pdata->sw_vempty_recover_vol);
	}

	if (pdata->capacity_calculation_type & SEC_FUELGAUGE_CAPACITY_TYPE_LOST_SOC)
		max1726x_parse_dt_lost_soc(pdata, np);

	ret = of_property_read_u32(np, "fuelgauge,vbat_open_adc", &pdata->vbat_open_adc);
	if (ret)
		pdata->vbat_open_adc = 0;

#if defined(CONFIG_BATTERY_AGE_FORECAST)
	np = of_find_node_by_name(NULL, "battery");
		ret = of_property_read_u32(np, "battery,full_condition_soc",
					   &pdata->full_condition_soc);
		if (ret) {
			pdata->full_condition_soc = 93;
			pr_info("%s: Full condition soc is Empty\n", __func__);
		}
#endif

	return pdata;
}

static void max1726x_parse_param_value(struct max1726x_priv *priv)
{
#if IS_ENABLED(CONFIG_USB_FACTORY_MODE)
	pr_info("%s: f_mode: %s\n", __func__, f_mode);

	if (!f_mode) {
		priv->f_mode = NO_MODE;
	} else if ((strncmp(f_mode, "OB", 2) == 0) ||
			(strncmp(f_mode, "DL", 2) == 0)) {
		/* Set factory mode variables in OB mode */
		priv->f_mode = OB_MODE;
	} else if (strncmp(f_mode, "IB", 2) == 0) {
		priv->f_mode = IB_MODE;
	} else {
		priv->f_mode = NO_MODE;
	}

	pr_info("%s: f_mode: %s\n", __func__, BOOT_MODE_STRING[priv->f_mode]);
#endif

	priv->vbat_open = vbat_adc < priv->pdata->vbat_open_adc ? true : false;
	pr_info("%s: vbat_adc: %d (open_adc: %d), vbat_open: %d\n",
		__func__, vbat_adc, priv->pdata->vbat_open_adc, priv->vbat_open);
}

static const struct regmap_config max1726x_regmap = {
	.reg_bits   = 8,
	.val_bits   = 16,
	.val_format_endian = REGMAP_ENDIAN_NATIVE,
};

static const struct power_supply_desc max1726x_fg_desc = {
	.name           = "max1726x-fuelgauge",
	.type           = POWER_SUPPLY_TYPE_UNKNOWN,
	.properties     = max1726x_fuelgauge_props,
	.num_properties = ARRAY_SIZE(max1726x_fuelgauge_props),
	.get_property   = max1726x_get_property,
	.set_property	= max1726x_set_property,
};

static int max1726x_probe(struct i2c_client *client,
				const struct i2c_device_id *id)
{
	struct max1726x_priv *priv;
	struct power_supply_config psy_cfg = {};
	int ret;

	pr_info("%s: max1726x Fuelgauge Driver Loading\n", __func__);

	priv = devm_kzalloc(&client->dev, sizeof(*priv), GFP_KERNEL);
	if (!priv)
		return -ENOMEM;

	if (client->dev.of_node)
		priv->pdata = max1726x_parse_dt(&client->dev);
	else
		priv->pdata = client->dev.platform_data;

	priv->dev   = &client->dev;

	i2c_set_clientdata(client, priv);

	priv->client = client;
	priv->regmap = devm_regmap_init_i2c(client, &max1726x_regmap);
	if (IS_ERR(priv->regmap))
		return PTR_ERR(priv->regmap);
	mutex_init(&priv->i2c_lock);
	max1726x_parse_param_value(priv);

	ret = max1726x_init(priv);
	if (ret) {
		pr_err("%s: Failed to Initialize Fuelgauge\n", __func__);
		return ret;
	}

	max1726x_priv_init(priv);

	psy_cfg.drv_data = priv;
	priv->psy_fg = power_supply_register(&client->dev,
					&max1726x_fg_desc, &psy_cfg);

	if (IS_ERR(priv->psy_fg)) {
		ret = PTR_ERR(priv->psy_fg);
		dev_err(&client->dev, "failed to register psy_fg: %d\n", ret);
		return ret;
	}

	if (client->irq) {
		ret = devm_request_threaded_irq(priv->dev, client->irq,
						NULL,
						max1726x_irq_handler,
						IRQF_TRIGGER_FALLING | IRQF_ONESHOT,
						priv->psy_fg->desc->name,
						priv);
		if (ret) {
			dev_err(priv->dev, "Failed to request irq %d\n", client->irq);
			goto err_irq;
		} else {
			max1726x_update_bits(priv, MAX1726X_CONFIG_REG,
					MAX1726X_CONFIG_ALRT_EN, MAX1726X_CONFIG_ALRT_EN);
		}
	}

	/* Create max1726x sysfs attributes */
	priv->attr_grp = &max1726x_attr_group;
	ret = sysfs_create_group(&priv->dev->kobj, priv->attr_grp);
	if (ret) {
		dev_err(priv->dev, "Failed to create attribute group [%d]\n", ret);
		priv->attr_grp = NULL;
		goto err_attr;
	}

#if IS_MODULE(CONFIG_BATTERY_SAMSUNG)
	sec_chg_set_dev_init(SC_DEV_FG);
#endif

	pr_info("%s: max1726x Fuelgauge Driver Loaded\n", __func__);
	return 0;

err_irq:
	power_supply_unregister(priv->psy_fg);
err_attr:
	sysfs_remove_group(&priv->dev->kobj, priv->attr_grp);
	return ret;
}

static int max1726x_remove(struct i2c_client *client)
{
	struct max1726x_priv *priv = i2c_get_clientdata(client);

	sysfs_remove_group(&priv->dev->kobj, priv->attr_grp);
	power_supply_unregister(priv->psy_fg);
	return 0;
}

static void max1726x_shutdown(struct i2c_client *client)
{
	struct max1726x_priv *priv = i2c_get_clientdata(client);

	pr_info("%s: ++\n", __func__);

	if (client->irq)
		free_irq(client->irq, priv);

	pr_info("%s: --\n", __func__);
}

#ifdef CONFIG_PM_SLEEP
static int max1726x_suspend(struct device *dev)
{
	struct i2c_client *client = to_i2c_client(dev);

	if (client->irq) {
		disable_irq(client->irq);
		enable_irq_wake(client->irq);
	}

	return 0;
}

static int max1726x_resume(struct device *dev)
{
	struct i2c_client *client = to_i2c_client(dev);

	if (client->irq) {
		disable_irq_wake(client->irq);
		enable_irq(client->irq);
	}

	return 0;
}

static SIMPLE_DEV_PM_OPS(max1726x_pm_ops,
		max1726x_suspend, max1726x_resume);
#define MAX1726X_PM_OPS (&max1726x_pm_ops)
#else
#define MAX1726X_PM_OPS NULL
#endif /* CONFIG_PM_SLEEP */

static const struct of_device_id max1726x_match[] = {
	{ .compatible = "maxim,max1726x", },
	{ },
};
MODULE_DEVICE_TABLE(of, max1726x_match);

static const struct i2c_device_id max1726x_id[] = {
	{ "max1726x", ID_MAX1726X },
	{ },
};
MODULE_DEVICE_TABLE(i2c, max1726x_id);

static struct i2c_driver max1726x_i2c_driver = {
	.driver = {
		.name = DRV_NAME,
		.of_match_table = of_match_ptr(max1726x_match),
		.pm = MAX1726X_PM_OPS,
	},
	.probe = max1726x_probe,
	.remove = max1726x_remove,
	.shutdown = max1726x_shutdown,
	.id_table = max1726x_id,
};
module_i2c_driver(max1726x_i2c_driver);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Jason Cole <jason.cole@maximintegrated.com>");
MODULE_AUTHOR("Mahir Ozturk <mahir.ozturk@maximintegrated.com>");
MODULE_DESCRIPTION("Maxim Max1726x Fuel Gauge driver");
