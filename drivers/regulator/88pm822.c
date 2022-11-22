/*
 * Regulators driver for Marvell 88PM822
 *
 * Copyright (C) 2013 Marvell International Ltd.
 * Yipeng Yao <ypyao@marvell.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */
#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/init.h>
#include <linux/err.h>
#include <linux/regmap.h>
#include <linux/regulator/driver.h>
#include <linux/regulator/machine.h>
#include <linux/mfd/88pm822.h>
#include <linux/delay.h>
#include <linux/io.h>

#if defined(CONFIG_CPU_PXA1L88) || defined(CONFIG_CPU_PXA988) || defined(CONFIG_CPU_PXA1088)
/* below 4 addresses are fake, only used in new dvc */
#define PM822_BUCK1_AP_ACTIVE	(0x3C)
#define PM822_BUCK1_AP_LPM	(0x3C)
#define PM822_BUCK1_APSUB_IDLE	(0x3C)
#define PM822_BUCK1_APSUB_SLEEP	(0x3C)

#endif

struct pm822_regulator_info {
	struct regulator_desc desc;
	struct pm822_chip *chip;
	struct regmap *map;
	struct regulator_dev *regulator;

	int vol_reg;
	int vol_shift;
	int vol_nbits;
	int enable_reg;
	int enable_bit;
	int dvc_en;
	int dvc_val;
	int max_uA;
	unsigned int *vol_table;
	void __iomem *dummy_dvc_reg;
	unsigned int (*dvc_init_map)[4];

#if defined(CONFIG_CPU_PXA1L88) || defined(CONFIG_CPU_PXA988) || defined(CONFIG_CPU_PXA1088)
	struct pm822_dvc_pdata *dvc;
#endif
};

/* Copy from drivers/regulator/core.c */
struct regulator {
	struct device *dev;
	struct list_head list;
	int uA_load;
	int min_uV;
	int max_uV;
	char *supply_name;
	struct device_attribute dev_attr;
	struct regulator_dev *rdev;
	struct dentry *debugfs;
};

/* FIXME use 0b'10 as default dvc value. The thinking is that we do not
 * want user to see the dvc feature in PM822. If you want to change the
 * voltage setting according to different dvc settings, you may use the dedicated
 * driver API to do this.
 */
#define PM822_DEF_DVC_VAL		0x2

static const unsigned int BUCK1_table[] = {
	/* 0x00-0x4F: from 0.6 to 1.5875V with step 0.0125V */
	600000, 612500, 625000, 637500, 650000, 662500, 675000, 687500,
	700000, 712500, 725000, 737500, 750000, 762500, 775000, 787500,
	800000, 812500, 825000, 837500, 850000, 862500, 875000, 887500,
	900000, 912500, 925000, 937500, 950000, 962500, 975000, 987500,
	1000000, 1012500, 1025000, 1037500, 1050000, 1062500, 1075000, 1087500,
	1100000, 1112500, 1125000, 1137500, 1150000, 1162500, 1175000, 1187500,
	1200000, 1212500, 1225000, 1237500, 1250000, 1262500, 1275000, 1287500,
	1300000, 1312500, 1325000, 1337500, 1350000, 1362500, 1375000, 1387500,
	1400000, 1412500, 1425000, 1437500, 1450000, 1462500, 1475000, 1487500,
	1500000, 1512500, 1525000, 1537500, 1550000, 1562500, 1575000, 1587500,
	/* 0x50-0x7F: from 1.6 to 3.95V with step 0.05V */
	1600000, 1650000, 1700000, 1750000, 1800000,
};

static const unsigned int BUCK2_table[] = {
	/* 0x00-0x4F: from 0.6 to 1.5875V with step 0.0125V */
	600000, 612500, 625000, 637500, 650000, 662500, 675000, 687500,
	700000, 712500, 725000, 737500, 750000, 762500, 775000, 787500,
	800000, 812500, 825000, 837500, 850000, 862500, 875000, 887500,
	900000, 912500, 925000, 937500, 950000, 962500, 975000, 987500,
	1000000, 1012500, 1025000, 1037500, 1050000, 1062500, 1075000, 1087500,
	1100000, 1112500, 1125000, 1137500, 1150000, 1162500, 1175000, 1187500,
	1200000, 1212500, 1225000, 1237500, 1250000, 1262500, 1275000, 1287500,
	1300000, 1312500, 1325000, 1337500, 1350000, 1362500, 1375000, 1387500,
	1400000, 1412500, 1425000, 1437500, 1450000, 1462500, 1475000, 1487500,
	1500000, 1512500, 1525000, 1537500, 1550000, 1562500, 1575000, 1587500,
	/* 0x50-0x7F: from 1.6 to 3.95V with step 0.05V */
	1600000, 1650000, 1700000, 1750000, 1800000, 1850000, 1900000, 1950000,
	2000000, 2050000, 2100000, 2150000, 2200000, 2250000, 2300000, 2350000,
	2400000, 2450000, 2500000, 2550000, 2600000, 2650000, 2700000, 2750000,
	2800000, 2850000, 2900000, 2950000, 3000000, 3050000, 3100000, 3150000,
	3200000, 3250000, 3300000,
};

static const unsigned int BUCK3_table[] = {
	/* 0x00-0x4F: from 0.6 to 1.5875V with step 0.0125V */
	600000, 612500, 625000, 637500, 650000, 662500, 675000, 687500,
	700000, 712500, 725000, 737500, 750000, 762500, 775000, 787500,
	800000, 812500, 825000, 837500, 850000, 862500, 875000, 887500,
	900000, 912500, 925000, 937500, 950000, 962500, 975000, 987500,
	1000000, 1012500, 1025000, 1037500, 1050000, 1062500, 1075000, 1087500,
	1100000, 1112500, 1125000, 1137500, 1150000, 1162500, 1175000, 1187500,
	1200000, 1212500, 1225000, 1237500, 1250000, 1262500, 1275000, 1287500,
	1300000, 1312500, 1325000, 1337500, 1350000, 1362500, 1375000, 1387500,
	1400000, 1412500, 1425000, 1437500, 1450000, 1462500, 1475000, 1487500,
	1500000, 1512500, 1525000, 1537500, 1550000, 1562500, 1575000, 1587500,
	/* 0x50-0x7F: from 1.6 to 3.95V with step 0.05V */
	1600000, 1650000, 1700000, 1750000, 1800000, 1850000, 1900000, 1950000,
	2000000, 2050000, 2100000, 2150000, 2200000, 2250000, 2300000, 2350000,
	2400000, 2450000, 2500000, 2550000, 2600000, 2650000, 2700000, 2750000,
	2800000, 2850000, 2900000, 2950000, 3000000, 3050000, 3100000, 3150000,
	3200000, 3250000, 3300000,
};

static const unsigned int BUCK4_table[] = {
	/* 0x00-0x4F: from 0.6 to 1.5875V with step 0.0125V */
	600000, 612500, 625000, 637500, 650000, 662500, 675000, 687500,
	700000, 712500, 725000, 737500, 750000, 762500, 775000, 787500,
	800000, 812500, 825000, 837500, 850000, 862500, 875000, 887500,
	900000, 912500, 925000, 937500, 950000, 962500, 975000, 987500,
	1000000, 1012500, 1025000, 1037500, 1050000, 1062500, 1075000, 1087500,
	1100000, 1112500, 1125000, 1137500, 1150000, 1162500, 1175000, 1187500,
	1200000, 1212500, 1225000, 1237500, 1250000, 1262500, 1275000, 1287500,
	1300000, 1312500, 1325000, 1337500, 1350000, 1362500, 1375000, 1387500,
	1400000, 1412500, 1425000, 1437500, 1450000, 1462500, 1475000, 1487500,
	1500000, 1512500, 1525000, 1537500, 1550000, 1562500, 1575000, 1587500,
	/* 0x50-0x7F: from 1.6 to 3.95V with step 0.05V */
	1600000, 1650000, 1700000, 1750000, 1800000, 1850000, 1900000, 1950000,
	2000000, 2050000, 2100000, 2150000, 2200000, 2250000, 2300000, 2350000,
	2400000, 2450000, 2500000, 2550000, 2600000, 2650000, 2700000, 2750000,
	2800000, 2850000, 2900000, 2950000, 3000000, 3050000, 3100000, 3150000,
	3200000, 3250000, 3300000,
};

static const unsigned int BUCK5_table[] = {
	/* 0x00-0x4F: from 0.6 to 1.5875V with step 0.0125V */
	600000, 612500, 625000, 637500, 650000, 662500, 675000, 687500,
	700000, 712500, 725000, 737500, 750000, 762500, 775000, 787500,
	800000, 812500, 825000, 837500, 850000, 862500, 875000, 887500,
	900000, 912500, 925000, 937500, 950000, 962500, 975000, 987500,
	1000000, 1012500, 1025000, 1037500, 1050000, 1062500, 1075000, 1087500,
	1100000, 1112500, 1125000, 1137500, 1150000, 1162500, 1175000, 1187500,
	1200000, 1212500, 1225000, 1237500, 1250000, 1262500, 1275000, 1287500,
	1300000, 1312500, 1325000, 1337500, 1350000, 1362500, 1375000, 1387500,
	1400000, 1412500, 1425000, 1437500, 1450000, 1462500, 1475000, 1487500,
	1500000, 1512500, 1525000, 1537500, 1550000, 1562500, 1575000, 1587500,
	/* 0x50-0x7F: from 1.6 to 3.95V with step 0.05V */
	1600000, 1650000, 1700000, 1750000, 1800000, 1850000, 1900000, 1950000,
	2000000, 2050000, 2100000, 2150000, 2200000, 2250000, 2300000, 2350000,
	2400000, 2450000, 2500000, 2550000, 2600000, 2650000, 2700000, 2750000,
	2800000, 2850000, 2900000, 2950000, 3000000, 3050000, 3100000, 3150000,
	3200000, 3250000, 3300000, 3350000, 3400000, 3450000, 3500000, 3550000,
	3600000, 3650000, 3700000, 3750000, 3800000, 3850000, 3900000, 3950000,
};

static const unsigned int LDO1_table[] = {
	1700000, 1800000, 1900000, 2500000, 2800000, 2900000, 3100000, 3300000,
};

static const unsigned int LDO2_table[] = {
	1700000, 1800000, 1900000, 2500000, 2800000, 2900000, 3100000, 3300000,
};

static const unsigned int LDO3_table[] = {
	1200000, 1250000, 1700000, 1800000, 1850000, 1900000, 2500000, 2600000,
	2700000, 2750000, 2800000, 2850000, 2900000, 3000000, 3100000, 3300000,
};

static const unsigned int LDO4_table[] = {
	1200000, 1250000, 1700000, 1800000, 1850000, 1900000, 2500000, 2600000,
	2700000, 2750000, 2800000, 2850000, 2900000, 3000000, 3100000, 3300000,
};

static const unsigned int LDO5_table[] = {
	1200000, 1250000, 1700000, 1800000, 1850000, 1900000, 2500000, 2600000,
	2700000, 2750000, 2800000, 2850000, 2900000, 3000000, 3100000, 3300000,
};

static const unsigned int LDO6_table[] = {
	1200000, 1250000, 1700000, 1800000, 1850000, 1900000, 2500000, 2600000,
	2700000, 2750000, 2800000, 2850000, 2900000, 3000000, 3100000, 3300000,
};

static const unsigned int LDO7_table[] = {
	1200000, 1250000, 1700000, 1800000, 1850000, 1900000, 2500000, 2600000,
	2700000, 2750000, 2800000, 2850000, 2900000, 3000000, 3100000, 3300000,
};

static const unsigned int LDO8_table[] = {
	1200000, 1250000, 1700000, 1800000, 1850000, 1900000, 2500000, 2600000,
	2700000, 2750000, 2800000, 2850000, 2900000, 3000000, 3100000, 3300000,
};

static const unsigned int LDO9_table[] = {
	1200000, 1250000, 1700000, 1800000, 1850000, 1900000, 2500000, 2600000,
	2700000, 2750000, 2800000, 2850000, 2900000, 3000000, 3100000, 3300000,
};

static const unsigned int LDO10_table[] = {
	1200000, 1250000, 1700000, 1800000, 1850000, 1900000, 2500000, 2600000,
	2700000, 2750000, 2800000, 2850000, 2900000, 3000000, 3100000, 3300000,
};

static const unsigned int LDO11_table[] = {
	1200000, 1250000, 1700000, 1800000, 1850000, 1900000, 2500000, 2600000,
	2700000, 2750000, 2800000, 2850000, 2900000, 3000000, 3100000, 3300000,
};

static const unsigned int LDO12_table[] = {
	600000,  650000,  700000,  750000,  800000,  850000,  900000,  950000,
	1000000, 1050000, 1100000, 1150000, 1200000, 1300000, 1400000, 1500000,
};

static const unsigned int LDO13_table[] = {
	1700000, 1800000, 1900000, 2500000, 2800000, 2900000, 3100000, 3300000,
};

static const unsigned int LDO14_table[] = {
	1700000, 1800000, 1900000, 2000000, 2100000, 2500000, 2700000, 2800000,
};

static const unsigned int VOUTSW_table[] = {
};

#if defined(CONFIG_CPU_PXA1L88) || defined(CONFIG_CPU_PXA988) || defined(CONFIG_CPU_PXA1088)
/* below 4 tables are fake table, only used in new dvc
 * the voltage is logical value, not real value. and the
 * smallest logical value is LEVEL0(int value is 1), and
 * as the unit is uv, so smallest uV is 1000, others are x1000
 */
static const unsigned int BUCK1_AP_ACTIVE_table[] = {
	1000, 2000, 3000, 4000, 5000, 6000, 7000, 8000, 9000, 10000,
};

static const unsigned int BUCK1_AP_LPM_table[] = {
	1000, 2000, 3000, 4000, 5000, 6000, 7000, 8000, 9000, 10000,
};

static const unsigned int BUCK1_APSUB_IDLE_table[] = {
	1000, 2000, 3000, 4000, 5000, 6000, 7000, 8000, 9000, 10000,
};

static const unsigned int BUCK1_APSUB_SLEEP_table[] = {
	1000, 2000, 3000, 4000, 5000, 6000, 7000, 8000, 9000, 10000,
};
#endif

static int pm822_list_voltage(struct regulator_dev *rdev, unsigned index)
{
	struct pm822_regulator_info *info = rdev_get_drvdata(rdev);
	int ret = -EINVAL;

	if (info->vol_table && (index < info->desc.n_voltages))
		ret = info->vol_table[index];

	return ret;
}

static int choose_voltage(struct pm822_regulator_info *info,
			  int min_uV, int max_uV)
{
	int i, ret = -ENOENT;

	if (info->vol_table) {
		for (i = 0; i < info->desc.n_voltages; i++) {
			if (!info->vol_table[i])
				break;
			if ((min_uV <= info->vol_table[i])
			    && (max_uV >= info->vol_table[i])) {
				ret = i;
				break;
			}
		}
	}
	if (ret < 0)
		dev_err(info->chip->dev,
			"invalid voltage range (%d %d) uV\n", min_uV, max_uV);

	return ret;
}

static int pm822_set_voltage(struct regulator_dev *rdev,
			     int min_uV, int max_uV, unsigned *selector)
{
	struct pm822_regulator_info *info = rdev_get_drvdata(rdev);
	uint8_t mask;
	int ret;
	unsigned int value;

	if (info->desc.id == PM822_ID_VOUTSW)
		return 0;

	if (min_uV > max_uV) {
		dev_err(info->chip->dev,
			"invalid voltage range (%d, %d) uV\n", min_uV, max_uV);
		return -EINVAL;
	}

#if defined(CONFIG_CPU_PXA1L88) || defined(CONFIG_CPU_PXA988) || defined(CONFIG_CPU_PXA1088)
	if ((info->dvc != NULL) && (info->dvc->reg_dvc)
	    && (info->desc.id >= PM822_ID_BUCK1_AP_ACTIVE)
	    && (info->desc.id <= PM822_ID_BUCK1_APSUB_SLEEP)) {
		info->dvc->set_dvc(info->desc.id, min_uV / 1000);
		return 0;
	}

	ret = choose_voltage(info, min_uV, max_uV);
	if (ret < 0)
		return -EINVAL;
	*selector = ret;
	value = (ret << info->vol_shift);
	mask = ((1 << info->vol_nbits) - 1) << info->vol_shift;
	if ((info->dvc != NULL) && (info->dvc->reg_dvc)) {
		/* BUCK3 and BUCK5 */
		if (info->desc.id == PM822_ID_BUCK3) {
			regmap_update_bits(info->map, info->vol_reg + 1, mask,
					   value);
			regmap_update_bits(info->map, info->vol_reg + 2, mask,
					   value);
			regmap_update_bits(info->map, info->vol_reg + 3, mask,
					   value);
		}

		if (info->desc.id == PM822_ID_BUCK5) {
			regmap_update_bits(info->map, info->vol_reg + 1, mask,
					   value);
			regmap_update_bits(info->map, info->vol_reg + 2, mask,
					   value);
			regmap_update_bits(info->map, info->vol_reg + 3, mask,
					   value);
		}
	}

	return regmap_update_bits(info->map, info->vol_reg, mask, value);
#endif
}

static int pm822_get_voltage(struct regulator_dev *rdev)
{
	struct pm822_regulator_info *info = rdev_get_drvdata(rdev);
	uint8_t val, mask;
	int ret;
	unsigned int value;

	if (info->desc.id == PM822_ID_VOUTSW)
		return 0;

#if defined(CONFIG_CPU_PXA1L88) || defined(CONFIG_CPU_PXA988) || defined(CONFIG_CPU_PXA1088)
	ret = regmap_read(info->map, info->vol_reg, &value);
#endif
	if (ret < 0)
		return ret;

	mask = ((1 << info->vol_nbits) - 1) << info->vol_shift;
	val = (value & mask) >> info->vol_shift;

	return pm822_list_voltage(rdev, val);
}

static int pm822_enable(struct regulator_dev *rdev)
{
	struct pm822_regulator_info *info = rdev_get_drvdata(rdev);

	return regmap_update_bits(info->map, info->enable_reg,
			1 << info->enable_bit, 1 << info->enable_bit);
}

static int pm822_disable(struct regulator_dev *rdev)
{
	struct pm822_regulator_info *info = rdev_get_drvdata(rdev);

	return regmap_update_bits(info->map, info->enable_reg,
				  1 << info->enable_bit, 0);
}

static int pm822_is_enabled(struct regulator_dev *rdev)
{
	struct pm822_regulator_info *info = rdev_get_drvdata(rdev);
	int ret;
	unsigned int val;

	ret = regmap_read(info->map, info->enable_reg, &val);
	if (ret < 0)
		return ret;

	return !!(val & (1 << info->enable_bit));
}

static int pm822_get_current_limit(struct regulator_dev *rdev)
{
	struct pm822_regulator_info *info = rdev_get_drvdata(rdev);
	return info->max_uA;
}

static struct regulator_ops pm822_regulator_ops = {
	.set_voltage = pm822_set_voltage,
	.get_voltage = pm822_get_voltage,
	.list_voltage = pm822_list_voltage,
	.enable = pm822_enable,
	.disable = pm822_disable,
	.is_enabled = pm822_is_enabled,
	.get_current_limit = pm822_get_current_limit,
};

#define PM822_REGULATOR(_id, shift, nbits, ereg, ebit, _dvc, amax)	\
{									\
	.desc	= {							\
		.name	= #_id,						\
		.ops	= &pm822_regulator_ops,				\
		.type	= REGULATOR_VOLTAGE,				\
		.id	= PM822_ID_##_id,				\
		.owner	= THIS_MODULE,					\
		.n_voltages = ARRAY_SIZE(_id##_table),			\
	},								\
	.vol_reg	= PM822_##_id,					\
	.vol_shift	= (shift),					\
	.vol_nbits	= (nbits),					\
	.enable_reg	= PM822_##ereg,					\
	.enable_bit	= (ebit),					\
	.dvc_en		= _dvc,						\
	.dvc_val	= PM822_DEF_DVC_VAL,				\
	.max_uA		= (amax),					\
	.vol_table	= (unsigned int *)&_id##_table,			\
}

static struct pm822_regulator_info pm822_regulator_info[] = {

	PM822_REGULATOR(BUCK1,	0,	7,	BUCK_EN1,	0,	1,	3500000),
	PM822_REGULATOR(BUCK2,	0,	7,	BUCK_EN1,	1,	0,	750000),
	PM822_REGULATOR(BUCK3,	0,	7,	BUCK_EN1,	2,	1,	1500000),
	PM822_REGULATOR(BUCK4,	0,	7,	BUCK_EN1,	3,	0,	750000),
	PM822_REGULATOR(BUCK5,	0,	7,	BUCK_EN1,	4,	1,	1500000),

	PM822_REGULATOR(LDO1,	0,	4,	LDO1_8_EN1,	0,	0,	100000),
	PM822_REGULATOR(LDO2,	0,	4,	LDO1_8_EN1,	1,	0,	100000),
	PM822_REGULATOR(LDO3,	0,	4,	LDO1_8_EN1,	2,	0,	400000),
	PM822_REGULATOR(LDO4,	0,	4,	LDO1_8_EN1,	3,	0,	400000),
	PM822_REGULATOR(LDO5,	0,	4,	LDO1_8_EN1,	4,	0,	200000),
	PM822_REGULATOR(LDO6,	0,	4,	LDO1_8_EN1,	5,	0,	200000),
	PM822_REGULATOR(LDO7,	0,	4,	LDO1_8_EN1,	6,	0,	100000),
	PM822_REGULATOR(LDO8,	0,	4,	LDO1_8_EN1,	7,	0,	100000),
	PM822_REGULATOR(LDO9,	0,	4,	LDO9_14_EN1,	0,	0,	200000),
	PM822_REGULATOR(LDO10,	0,	4,	LDO9_14_EN1,	1,	0,	400000),
	PM822_REGULATOR(LDO11,	0,	4,	LDO9_14_EN1,	2,	0,	200000),
	PM822_REGULATOR(LDO12,	0,	4,	LDO9_14_EN1,	3,	0,	400000),
	PM822_REGULATOR(LDO13,	0,	4,	LDO9_14_EN1,	4,	0,	100000),
	PM822_REGULATOR(LDO14,	0,	3,	LDO9_14_EN1,	5,	0,	8000),
	PM822_REGULATOR(VOUTSW,	0,	0,	MISC_EN1,	4,	0,	0),

#if defined(CONFIG_CPU_PXA1L88) || defined(CONFIG_CPU_PXA988) || defined(CONFIG_CPU_PXA1088)
	/* below 4 dvcs are fake, only used in new dvc */
	PM822_REGULATOR(BUCK1_AP_ACTIVE, 0, 7, BUCK_EN1, 0, 0, 300000),
	PM822_REGULATOR(BUCK1_AP_LPM, 0, 7, BUCK_EN1, 0, 0, 300000),
	PM822_REGULATOR(BUCK1_APSUB_IDLE, 0, 7, BUCK_EN1, 0, 0, 300000),
	PM822_REGULATOR(BUCK1_APSUB_SLEEP, 0, 7, BUCK_EN1, 0, 0, 300000),
#endif
};

static int __devinit pm822_regulator_probe(struct platform_device *pdev)
{
	struct pm822_chip *chip = dev_get_drvdata(pdev->dev.parent);
	struct pm822_regulator_info *info = NULL;
	struct regulator_init_data *pdata = pdev->dev.platform_data;
	struct pm822_platform_data *ppdata = (pdev->dev.parent)->platform_data;
	struct resource *res;
	int i;

	res = platform_get_resource(pdev, IORESOURCE_IO, 0);
	if (res == NULL) {
		dev_err(&pdev->dev, "No I/O resource!\n");
		return -EINVAL;
	}

	for (i = 0; i < ARRAY_SIZE(pm822_regulator_info); i++) {
		info = &pm822_regulator_info[i];
		if (info->desc.id == res->start)
			break;
	}
	if ((i < 0) || (i > PM822_ID_RG_MAX)) {
		dev_err(&pdev->dev, "Failed to find regulator %d\n",
			res->start);
		return -EINVAL;
	}
#if defined(CONFIG_CPU_PXA1L88) || defined(CONFIG_CPU_PXA988) || defined(CONFIG_CPU_PXA1088)
	info->dvc = ppdata->dvc;
#endif

	info->map = chip->subchip->regmap_power;
	info->chip = chip;
	info->regulator = regulator_register(&info->desc, &pdev->dev,
					     pdata, info, NULL);
	if (IS_ERR(info->regulator)) {
		dev_err(&pdev->dev, "failed to register regulator %s\n",
			info->desc.name);
		return PTR_ERR(info->regulator);
	}

	platform_set_drvdata(pdev, info);
	return 0;
}

static int __devexit pm822_regulator_remove(struct platform_device *pdev)
{
	struct pm822_regulator_info *info = platform_get_drvdata(pdev);

	platform_set_drvdata(pdev, NULL);
	regulator_unregister(info->regulator);
	return 0;
}

static struct platform_driver pm822_regulator_driver = {
	.driver		= {
		.name	= "88pm822-regulator",
		.owner	= THIS_MODULE,
	},
	.probe		= pm822_regulator_probe,
	.remove		= __devexit_p(pm822_regulator_remove),
};

static int __init pm822_regulator_init(void)
{
	return platform_driver_register(&pm822_regulator_driver);
}
subsys_initcall(pm822_regulator_init);

static void __exit pm822_regulator_exit(void)
{
	platform_driver_unregister(&pm822_regulator_driver);
}
module_exit(pm822_regulator_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Yipeng Yao <ypyao@marvell.com>");
MODULE_DESCRIPTION("Regulator Driver for Marvell 88PM822 PMIC");
MODULE_ALIAS("platform:88pm822-regulator");
