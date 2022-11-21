/*
 * Regulators driver for Marvell 88PM800
 *
 * Copyright (C) 2012 Marvell International Ltd.
 * Joseph(Yossi) Hanin <yhanin@marvell.com>
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
#include <linux/gpio.h>
#include <linux/regulator/driver.h>
#include <linux/regulator/machine.h>
#include <linux/mfd/88pm80x.h>
#include <linux/delay.h>
#include <linux/io.h>

#ifdef CONFIG_DEBUG_FS
#include <linux/debugfs.h>
#include <plat/debugfs.h>	/* for "pxa" directory */
#endif

/* LDO1 with DVC[0..3] */
#define PM800_LDO1_VOUT		(0x08) /* VOUT1 */
#define PM800_LDO1_VOUT_2	(0x09)
#define PM800_LDO1_VOUT_3	(0x0A)
#define PM800_LDO2_VOUT		(0x0B)
#define PM800_LDO3_VOUT		(0x0C)
#define PM800_LDO4_VOUT		(0x0D)
#define PM800_LDO5_VOUT		(0x0E)
#define PM800_LDO6_VOUT		(0x0F)
#define PM800_LDO7_VOUT		(0x10)
#define PM800_LDO8_VOUT		(0x11)
#define PM800_LDO9_VOUT		(0x12)
#define PM800_LDO10_VOUT	(0x13)
#define PM800_LDO11_VOUT	(0x14)
#define PM800_LDO12_VOUT	(0x15)
#define PM800_LDO13_VOUT	(0x16)
#define PM800_LDO14_VOUT	(0x17)
#define PM800_LDO15_VOUT	(0x18)
#define PM800_LDO16_VOUT	(0x19)
#define PM800_LDO17_VOUT	(0x1A)
#define PM800_LDO18_VOUT	(0x1B)
#define PM800_LDO19_VOUT	(0x1C)

/* BUCK1 with DVC[0..3] */
#define PM800_BUCK1		(0x3C)
#define PM800_BUCK1_1		(0x3D)
#define PM800_BUCK1_2		(0x3E)
#define PM800_BUCK1_3		(0x3F)
#define PM800_BUCK2		(0x40)
#define PM800_BUCK3		(0x41)
#define PM800_BUCK3_DOUBLE	(1 << 6)

/* below 4 addresses are fake, only used in new dvc */
#define PM800_BUCK1_AP_ACTIVE	(0x3C)
#define PM800_BUCK1_AP_LPM	(0x3C)
#define PM800_BUCK1_APSUB_IDLE	(0x3C)
#define PM800_BUCK1_APSUB_SLEEP	(0x3C)

#define PM800_BUCK_ENA		(0x50)
#define PM800_LDO_ENA1_1	(0x51)
#define PM800_LDO_ENA1_2	(0x52)
#define PM800_LDO_ENA1_3	(0x53)

#define PM800_LDO_ENA2_1	(0x56)
#define PM800_LDO_ENA2_2	(0x57)
#define PM800_LDO_ENA2_3	(0x58)

#define PM800_BUCK1_MISC1	(0x78)
#define PM800_BUCK3_MISC1	(0x7E)
#define PM800_BUCK4_MISC1	(0x81)
#define PM800_BUCK5_MISC1	(0x84)
/*
 * Convert voltage value to reg value for BUCK1
 */
#define BUCK1_VOL2REG(vol)						\
	(((vol) > 1587500)						\
	 ? ((((vol)-1600000)/50000) + 0x50)		\
	 : (((vol)-600000)/12500))

struct pm800_regulator_info {
	struct regulator_desc desc;
	struct pm80x_chip *chip;
	struct regmap *map;
	struct regulator_dev *regulator;
	struct pm80x_dvc_pdata *dvc;

	unsigned int *vol_table;
	unsigned int *vol_suspend;

	int vol_reg;
	int vol_shift;
	int vol_nbits;
	int update_reg;
	int update_bit;
	int enable_reg;
	int enable_bit;
	spinlock_t gpio_lock;
	int dvc_val;
	int buck1_set_index;
#ifdef CONFIG_DEBUG_FS
	struct dentry *debugfs;
#endif
	int max_uA;
};

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
	3200000, 3250000, 3300000,
};

static const unsigned int LDO1_table[] = {
	600000,  650000,  700000,  750000,  800000,  850000,  900000,  950000,
	1000000, 1050000, 1100000, 1150000, 1200000, 1300000, 1400000, 1500000,
};

static const unsigned int LDO2_table[] = {
	1700000, 1800000, 1900000, 2000000, 2100000, 2500000, 2700000, 2800000,
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
	1200000, 1250000, 1700000, 1800000, 1850000, 1900000, 2500000, 2600000,
	2700000, 2750000, 2800000, 2850000, 2900000, 3000000, 3100000, 3300000,
};

static const unsigned int LDO13_table[] = {
	1200000, 1250000, 1700000, 1800000, 1850000, 1900000, 2500000, 2600000,
	2700000, 2750000, 2800000, 2850000, 2900000, 3000000, 3100000, 3300000,
};

static const unsigned int LDO14_table[] = {
	1200000, 1250000, 1700000, 1800000, 1850000, 1900000, 2500000, 2600000,
	2700000, 2750000, 2800000, 2850000, 2900000, 3000000, 3100000, 3300000,
};

static const unsigned int LDO15_table[] = {
	1200000, 1250000, 1700000, 1800000, 1850000, 1900000, 2500000, 2600000,
	2700000, 2750000, 2800000, 2850000, 2900000, 3000000, 3100000, 3300000,
};

static const unsigned int LDO16_table[] = {
	1200000, 1250000, 1700000, 1800000, 1850000, 1900000, 2500000, 2600000,
	2700000, 2750000, 2800000, 2850000, 2900000, 3000000, 3100000, 3300000,
};

static const unsigned int LDO17_table[] = {
	1200000, 1250000, 1700000, 1800000, 1850000, 1900000, 2500000, 2600000,
	2700000, 2750000, 2800000, 2850000, 2900000, 3000000, 3100000, 3300000,
};

static const unsigned int LDO18_table[] = {
	1700000, 1800000, 1900000, 2500000, 2800000, 2900000, 3100000, 3300000,
};

static const unsigned int LDO19_table[] = {
	1700000, 1800000, 1900000, 2500000, 2800000, 2900000, 3100000, 3300000,
};

static const unsigned int BUCK1_DVC_SET[] = {
	195, 390, 780, 1560, 3120, 6250, 12500, 25000,
};

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

/*
 * Caculate how long dvc voltage change need
 */
static inline int buck1_delay(int index, unsigned vol1, unsigned vol2)
{
	int res;
	if ((vol2 > 1600000) && (vol1 > 1600000))
		res = ((vol2 - vol1) / BUCK1_DVC_SET[index] / 4);
	else if ((vol2 > 1600000) && (vol1 <= 1600000))
		res = (vol2 - 1600000) / BUCK1_DVC_SET[index] / 4 +
		      (1600000 - vol1) / BUCK1_DVC_SET[index];
	else
		res = (vol2 - vol1) / BUCK1_DVC_SET[index];
	return res;
}

#ifdef CONFIG_DEBUG_FS
static int buck1_dvc_open(struct inode *inode, struct file *file)
{
	file->private_data = inode->i_private;
	return 0;
}

static ssize_t buck1_dvc_read(struct file *file, char __user *user_buf,
			    size_t count, loff_t *ppos)
{
	struct pm800_regulator_info *info;
	int ret;
	char str[16];
	unsigned int val = 0;

	info = file->private_data;
	if (info->dvc && info->dvc->gpio_dvc)
		val = info->dvc->vol_val[0];
	else if (info->dvc && info->dvc->reg_dvc) {
		val = __raw_readl(info->dvc->read_reg);
		val = (val >> 1) && 0x7;
		regmap_read(info->map, PM800_BUCK1 + val, &val);
		val = (val * 125 + 6000) * 100;
	}

	ret = snprintf(str, sizeof(str) - 1, "%d\n", val);

	return simple_read_from_buffer(user_buf, count, ppos, str, ret);
}

#define MAX	1400000
#define MIN	600000
static ssize_t buck1_dvc_write(struct file *file, const char __user *user_buf,
			    size_t count, loff_t *ppos)
{
	struct pm800_regulator_info *info;
	unsigned int exp_vol;
	int ret;
	info = file->private_data;

	ret = kstrtou32_from_user(user_buf, count, 10, &exp_vol);
	if (ret < 0)
		return ret;
	/*
	 * check the input whether it's out of range when
	 * DVC function is enabled;
	 * no need to check when DVC is disabled
	 */
	if (info->dvc != NULL) {
		if ((exp_vol > MAX) || (exp_vol < MIN)) {
			pr_err("input out of range!\n");
			return -1;
		}
		if (info->dvc->gpio_dvc != 0) {

			gpio_set_value(info->dvc->dvc1, 0);
			gpio_set_value(info->dvc->dvc2, 0);
			/*
			* there is a corner case here:
			* after using this debugfs to set voltage,
			* then vol_val[0] may be bigger than the other
			* values, calling set_voltage may has a
			* middle status
			*/
			info->dvc->vol_val[0] = exp_vol;
		} else if (info->dvc->reg_dvc != 0) {
			ret = __raw_readl(info->dvc->write_reg);
			ret &= ~(0x7 << 4); /* Switch to level 0 */
			ret |= 1 << 7;
			__raw_writel(ret, info->dvc->write_reg);
		}
	}
	/*
	 * when DVC function is disabled,
	 * the dvc is [00], set by u-boot
	 */
	regmap_update_bits(info->map, PM800_BUCK1, 0x7f,
				   BUCK1_VOL2REG(exp_vol));

	return count;
}

static const struct file_operations buck1_dvc_ops = {
	.owner		= THIS_MODULE,
	.open		= buck1_dvc_open,
	.read		= buck1_dvc_read,
	.write		= buck1_dvc_write,
};

static inline int pm800_buck1_debugfs_init(struct pm800_regulator_info *info)
{
	struct dentry *dump_vol;
	if (pxa == NULL) {
		pr_err("debugfs parent dir doesn't exist!\n");
		return -ENOENT;
	}

	dump_vol = debugfs_create_file("vcc_main", S_IRUGO | S_IFREG,
			    pxa, (void *)info, &buck1_dvc_ops);
	if (dump_vol == NULL) {
		debugfs_remove(dump_vol);
		pr_err("create vcc_main debugfs error!\n");
		return -ENOENT;
	}
	return 0;
}

static void pm800_buck1_debugfs_remove(struct pm800_regulator_info *info)
{
	if (info->debugfs)
		debugfs_remove_recursive(info->debugfs);
}
#endif

static int pm800_list_voltage(struct regulator_dev *rdev, unsigned index)
{
	struct pm800_regulator_info *info = rdev_get_drvdata(rdev);
	int ret = -EINVAL;

	if (info->vol_table && (index < info->desc.n_voltages))
		ret = info->vol_table[index];

	return ret;
}

static int choose_voltage(struct regulator_dev *rdev, int min_uV, int max_uV)
{
	struct pm800_regulator_info *info = rdev_get_drvdata(rdev);
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

static int pm800_set_voltage(struct regulator_dev *rdev,
			     int min_uV, int max_uV, unsigned *selector)
{
	struct pm800_regulator_info *info = rdev_get_drvdata(rdev);
	uint8_t mask;
	int ret, i, cur_dvc1, cur_dvc2, cur_vol, exp_vol, vol1, vol2;
	unsigned long flags;
	unsigned int value;

	if (min_uV > max_uV) {
		dev_err(info->chip->dev,
			"invalid voltage range (%d, %d) uV\n", min_uV, max_uV);
		return -EINVAL;
	}
	if ((info->dvc != NULL)
	    && (info->dvc->gpio_dvc != 0)
	    && (info->desc.id == PM800_ID_BUCK1)) {
		cur_vol = info->dvc->vol_val[info->dvc_val];
		exp_vol = min_uV;
		vol1 = (cur_vol > exp_vol) ? exp_vol : cur_vol;
		vol2 = (cur_vol > exp_vol) ? cur_vol : exp_vol;
		for (i = 0; i < 4; i++) {
			/* Get current dvc pin value */
			if (exp_vol == info->dvc->vol_val[i]) {
				spin_lock_irqsave(&info->gpio_lock, flags);
				/* 10 -> 01 case */
				if ((info->dvc_val == 0x2) && (i == 0x1)) {
					gpio_set_value(info->dvc->dvc1,
							      1);
					gpio_set_value(info->dvc->dvc2,
							      0);
				} else if ((info->dvc_val == 0x1)
					   && (i == 0x2)) {
					/* 01 -> 10 case */
					gpio_set_value(info->dvc->dvc2,
							      1);
					gpio_set_value(info->dvc->dvc1,
							      0);
				} else {
					/* Get the unchanged value */
					gpio_set_value(info->dvc->dvc2,
							      i >> 1);
					gpio_set_value(info->dvc->dvc1,
							      i % 2);
				}
				spin_unlock_irqrestore(&info->gpio_lock, flags);
				/*
				 * gpio sync: 4 periods of 3Mhz,
				 * 8us: test by oscilloscope
				 * +4us redundancy
				 */
				udelay(buck1_delay(info->buck1_set_index,
						   vol1, vol2) + 12);
				dev_dbg(info->chip->dev, "dvc pins: %d\n",
					info->dvc_val);
				break;
			}
		}
		if (i != 4) {	/* Have found in the table */
			dev_dbg(info->chip->dev, "new vol: %d i: %d\n",
				exp_vol, i);
			cur_dvc1 = !!gpio_get_value(info->dvc->dvc1);
			cur_dvc2 = !!gpio_get_value(info->dvc->dvc2);
			/* Cache dvc value */
			info->dvc_val = (cur_dvc2 << 1) | cur_dvc1;
			return 0;
		}
		/* Not found in the table */
		info->dvc_val = 0;
		cur_vol = info->dvc->vol_val[0];
		info->dvc->vol_val[0] = exp_vol;
		regmap_update_bits(info->map, PM800_BUCK1, 0x7f,
				   BUCK1_VOL2REG(exp_vol));

		spin_lock_irqsave(&info->gpio_lock, flags);
		gpio_set_value(info->dvc->dvc2, 0);
		gpio_set_value(info->dvc->dvc1, 0);
		spin_unlock_irqrestore(&info->gpio_lock, flags);

		/* exp_vol is sure to smaller than cur_vol */
		vol1 = (cur_vol > exp_vol) ? exp_vol : cur_vol;
		vol2 = (cur_vol > exp_vol) ? cur_vol : exp_vol;
		/*
		 * gpio sync: 4 periods of 3Mhz,
		 * 8us: test by oscilloscope
		 * +4us redundancy
		 */
		udelay(buck1_delay(info->buck1_set_index,
				   vol1, vol2) + 12);
		dev_dbg(info->chip->dev, "new vol: %d\n", exp_vol);

		return 0;
	}
	if ((info->dvc != NULL) && (info->dvc->reg_dvc)
	    && (info->desc.id >= PM800_ID_BUCK1_AP_ACTIVE)
	    && (info->desc.id <= PM800_ID_BUCK1_APSUB_SLEEP)) {
		info->dvc->set_dvc(info->desc.id, min_uV / 1000);
		return 0;
	}

	ret = choose_voltage(rdev, min_uV, max_uV);
	if (ret < 0)
		return -EINVAL;
	*selector = ret;
	value = (ret << info->vol_shift);
	mask = ((1 << info->vol_nbits) - 1) << info->vol_shift;
	if ((info->dvc != NULL)
	    && (info->dvc->gpio_dvc | info->dvc->reg_dvc)) {
		/* BUCK4 and LDO1 */
		if (info->desc.id == PM800_ID_BUCK4) {
			regmap_update_bits(info->map, info->vol_reg + 1, mask,
					   value);
			regmap_update_bits(info->map, info->vol_reg + 2, mask,
					   value);
			regmap_update_bits(info->map, info->vol_reg + 3, mask,
					   value);
		}

		if (info->desc.id == PM800_ID_LDO1) {
			regmap_update_bits(info->map, info->vol_reg + 1, 0xff,
					   (value << 4) | value);
			regmap_update_bits(info->map, info->vol_reg + 2, mask,
					   value);
		}
	}

	return regmap_update_bits(info->map, info->vol_reg, mask, value);
}

static int pm800_get_voltage(struct regulator_dev *rdev)
{
	struct pm800_regulator_info *info = rdev_get_drvdata(rdev);
	uint8_t val, mask;
	int ret, dvc1, dvc2, dvc;
	unsigned int value;

	ret = regmap_read(info->map, info->vol_reg, &value);
	if (ret < 0)
		return ret;
	if ((info->dvc != NULL)
	    && (info->dvc->gpio_dvc != 0)
	    && (info->desc.id == PM800_ID_BUCK1)) {
		dvc1 = !!gpio_get_value(info->dvc->dvc1);
		dvc2 = !!gpio_get_value(info->dvc->dvc2);
		dvc = (dvc2 << 1) | dvc1;
		ret = regmap_read(info->map, (info->vol_reg + dvc), &value);
		if (ret < 0)
			return ret;
	}

	mask = ((1 << info->vol_nbits) - 1) << info->vol_shift;
	val = (value & mask) >> info->vol_shift;

	return pm800_list_voltage(rdev, val);
}

static int pm800_enable(struct regulator_dev *rdev)
{
	struct pm800_regulator_info *info = rdev_get_drvdata(rdev);

	return regmap_update_bits(info->map, info->enable_reg,
				  1 << info->enable_bit, 1 << info->enable_bit);
}

static int pm800_disable(struct regulator_dev *rdev)
{
	struct pm800_regulator_info *info = rdev_get_drvdata(rdev);

	return regmap_update_bits(info->map, info->enable_reg,
				  1 << info->enable_bit, 0);
}

static int pm800_is_enabled(struct regulator_dev *rdev)
{
	struct pm800_regulator_info *info = rdev_get_drvdata(rdev);
	int ret;
	unsigned int val;

	ret = regmap_read(info->map, info->enable_reg, &val);
	if (ret < 0)
		return ret;

	return !!(val & (1 << info->enable_bit));
}

static int pm800_get_current_limit(struct regulator_dev *rdev)
{
	struct pm800_regulator_info *info = rdev_get_drvdata(rdev);
	return info->max_uA;
}

static struct regulator_ops pm800_regulator_ops = {
	.set_voltage = pm800_set_voltage,
	.get_voltage = pm800_get_voltage,
	.list_voltage = pm800_list_voltage,
	.enable = pm800_enable,
	.disable = pm800_disable,
	.is_enabled = pm800_is_enabled,
	.get_current_limit = pm800_get_current_limit,
};
/*
 * vreg - the buck regs string.
 * nbits - the number of bits that used to set the buck voltage.
 * ureg - the string for the register that is control by PI2c for sleep,
 * for exmp:GO_REGISTER[0x20] in PM8607)
 * ubit - enable bit for the buck similar to ebit.
 * ereg -  the string for the enable register.
 * ebit - the bit number in the enable register.
 */
#define PM800_DVC(vreg, nbits, ureg, ubit, ereg, ebit, amax)	\
{									\
	.desc	= {							\
		.name	= #vreg,					\
		.ops	= &pm800_regulator_ops,			\
		.type	= REGULATOR_VOLTAGE,				\
		.id	= PM800_ID_##vreg,				\
		.owner	= THIS_MODULE,					\
		.n_voltages = ARRAY_SIZE(vreg##_table),	\
	},								\
	.vol_reg	= PM800_##vreg,				\
	.vol_shift	= (0),						\
	.vol_nbits	= (nbits),					\
	.update_reg	= PM800_##ureg,				\
	.update_bit	= (ubit),					\
	.enable_reg	= PM800_##ereg,				\
	.enable_bit	= (ebit),					\
	.max_uA		= (amax),						\
	.vol_table	= (unsigned int *)&vreg##_table,		\
	.vol_suspend	= (unsigned int *)&vreg##_table,	\
}

/*
 * _id - the LDO number.
 * vreg - the LDO regs string
 * shift - the number of bits we need to shift left for setting LDO voltage.
 * nbits - the number of bits that used to set the LDO voltage.
 * ereg -  the string for the enable register.
 * ebit - the bit number in the enable register.
 */
#define PM800_LDO(_id, vreg, shift, nbits, ereg, ebit, amax)	\
{									\
	.desc	= {							\
		.name	= "LDO" #_id,					\
		.ops	= &pm800_regulator_ops,			\
		.type	= REGULATOR_VOLTAGE,				\
		.id	= PM800_ID_LDO##_id,				\
		.owner	= THIS_MODULE,					\
		.n_voltages = ARRAY_SIZE(LDO##_id##_table),	\
	},								\
	.vol_reg	= PM800_##vreg##_VOUT,				\
	.vol_shift	= (shift),					\
	.vol_nbits	= (nbits),					\
	.enable_reg	= PM800_##ereg,				\
	.enable_bit	= (ebit),					\
	.max_uA		= (amax),						\
	.vol_table	= (unsigned int *)&LDO##_id##_table,		\
	.vol_suspend	= (unsigned int *)&LDO##_id##_table,	\
}
/*
 * the GO register in the PM800_DVC table need to be consider
 * it might be we need to remove this filed from the MACRO
 */
static struct pm800_regulator_info pm800_regulator_info[] = {
	PM800_DVC(BUCK1, 7, BUCK_ENA, 0, BUCK_ENA, 0, 3000000),
	PM800_DVC(BUCK2, 7, BUCK_ENA, 1, BUCK_ENA, 1, 1200000),
	PM800_DVC(BUCK3, 7, BUCK_ENA, 2, BUCK_ENA, 2, 1200000),
	PM800_DVC(BUCK4, 7, BUCK_ENA, 3, BUCK_ENA, 3, 1200000),
	PM800_DVC(BUCK5, 7, BUCK_ENA, 4, BUCK_ENA, 4, 1200000),

	PM800_LDO(1,	LDO1,	0, 4,	LDO_ENA1_1, 0, 200000),
	PM800_LDO(2,	LDO2,	0, 3,	LDO_ENA1_1, 1, 10000),
	PM800_LDO(3,	LDO3,	0, 4,	LDO_ENA1_1, 2, 300000),
	PM800_LDO(4,	LDO4,	0, 4,	LDO_ENA1_1, 3, 300000),
	PM800_LDO(5,	LDO5,	0, 4,	LDO_ENA1_1, 4, 300000),
	PM800_LDO(6,	LDO6,	0, 4,	LDO_ENA1_1, 5, 300000),
	PM800_LDO(7,	LDO7,	0, 4,	LDO_ENA1_1, 6, 300000),
	PM800_LDO(8,	LDO8,	0, 4,	LDO_ENA1_1, 7, 300000),
	PM800_LDO(9,	LDO9,	0, 4,	LDO_ENA1_2, 0, 300000),
	PM800_LDO(10,	LDO10,	0, 4,	LDO_ENA1_2, 1, 300000),
	PM800_LDO(11,	LDO11,	0, 4,	LDO_ENA1_2, 2, 300000),
	PM800_LDO(12,	LDO12,	0, 4,	LDO_ENA1_2, 3, 300000),
	PM800_LDO(13,	LDO13,	0, 4,	LDO_ENA1_2, 4, 300000),
	PM800_LDO(14,	LDO14,	0, 4,	LDO_ENA1_2, 5, 300000),
	PM800_LDO(15,	LDO15,	0, 4,	LDO_ENA1_2, 6, 300000),
	PM800_LDO(16,	LDO16,	0, 4,	LDO_ENA1_2, 7, 300000),
	PM800_LDO(17,	LDO17,	0, 4,	LDO_ENA1_3, 0, 300000),
	PM800_LDO(18,	LDO18,	0, 3,	LDO_ENA1_3, 1, 200000),
	PM800_LDO(19,	LDO19,	0, 3,	LDO_ENA1_3, 2, 200000),

	/* below 4 dvcs are fake, only used in new dvc */
	PM800_DVC(BUCK1_AP_ACTIVE, 7, BUCK_ENA, 0, BUCK_ENA, 0, 300000),
	PM800_DVC(BUCK1_AP_LPM, 7, BUCK_ENA, 0, BUCK_ENA, 0, 300000),
	PM800_DVC(BUCK1_APSUB_IDLE, 7, BUCK_ENA, 0, BUCK_ENA, 0, 300000),
	PM800_DVC(BUCK1_APSUB_SLEEP, 7, BUCK_ENA, 0, BUCK_ENA, 0, 300000),
};

static int __devinit pm800_regulator_probe(struct platform_device *pdev)
{
	struct pm80x_chip *chip = dev_get_drvdata(pdev->dev.parent);
	struct pm800_regulator_info *info = NULL;
	struct regulator_init_data *pdata = pdev->dev.platform_data;
	struct pm80x_platform_data *ppdata = (pdev->dev.parent)->platform_data;
	struct resource *res;
	int i, ret, dvc1, dvc2, vol;
	unsigned int val, max1 = 0, max2 = 0, max3 = 0;
	unsigned long flags;

	res = platform_get_resource(pdev, IORESOURCE_IO, 0);
	if (res == NULL) {
		dev_err(&pdev->dev, "No I/O resource!\n");
		return -EINVAL;
	}

	for (i = 0; i < ARRAY_SIZE(pm800_regulator_info); i++) {
		info = &pm800_regulator_info[i];
		if (info->desc.id == res->start)
			break;
	}
	if ((i < 0) || (i > PM800_ID_RG_MAX)) {
		dev_err(&pdev->dev, "Failed to find regulator %d\n",
			res->start);
		return -EINVAL;
	}

	info->map = chip->subchip->regmap_power;
	info->chip = chip;
	info->dvc = ppdata->dvc;
	spin_lock_init(&info->gpio_lock);

	info->regulator = regulator_register(&info->desc, &pdev->dev,
					     pdata, info, NULL);
	if (IS_ERR(info->regulator)) {
		dev_err(&pdev->dev, "failed to register regulator %s\n",
			info->desc.name);
		return PTR_ERR(info->regulator);
	}
	/* dvc may not be used on other PMIC */
	if ((info->dvc != NULL)
	    && (info->dvc->gpio_dvc != 0)
	    && (info->desc.id == PM800_ID_BUCK1)) {
		ret = regmap_read(info->map, PM800_BUCK1_MISC1, &val);
		if (ret < 0) {
			dev_err(&pdev->dev, "Failed to read buck1 misc1: %d\n",
				ret);
			goto out1;
		}

		info->buck1_set_index = (val >> 3) & 0x07;
		/* DVC gpio configuration */
		if (gpio_request(info->dvc->dvc1, "DVC1")) {
			dev_err(&pdev->dev,
				"Failed to request GPIO for DVC1!\n");
			goto out1;
		}
		if (gpio_request(info->dvc->dvc2, "DVC2")) {
			dev_err(&pdev->dev,
				"Failed to request GPIO for DVC2!\n");
			goto out2;
		}
		/* Read the original voltage value set by U-boot */
		dvc1 = !!gpio_get_value(info->dvc->dvc1);
		dvc2 = !!gpio_get_value(info->dvc->dvc2);
		info->dvc_val = (dvc2 << 1) | dvc1;
		ret =
		    regmap_read(info->map, PM800_BUCK1 + (info->dvc_val), &vol);
		if (ret < 0) {
			dev_err(&pdev->dev, "Failed to read dvc_val: %d\n",
				ret);
			goto out3;
		}

		/* Set the uboot voltage to reg of dvc */
		for (i = 0; i < 4; i++)
			if (i != info->dvc_val)
				regmap_update_bits(info->map, PM800_BUCK1 + i,
						   0x7f, vol);

		spin_lock_irqsave(&info->gpio_lock, flags);
		gpio_direction_output(info->dvc->dvc2, 0);
		gpio_direction_output(info->dvc->dvc1, 0);
		spin_unlock_irqrestore(&info->gpio_lock, flags);
		/* Cache the dvc pin value */
		info->dvc_val = 0;

		/* Sort the voltage */
		for (i = 0; i < info->dvc->size; i++)
			if (max1 < info->dvc->vol_val[i])
				max1 = info->dvc->vol_val[i];

		for (i = 0; i < info->dvc->size; i++)
			if ((max2 < info->dvc->vol_val[i]) &&
			    (info->dvc->vol_val[i] != max1))
				max2 = info->dvc->vol_val[i];

		for (i = 0; i < info->dvc->size; i++)
			if ((max3 < info->dvc->vol_val[i]) &&
			    (info->dvc->vol_val[i] != max1) &&
			    (info->dvc->vol_val[i] != max2))
				max3 = info->dvc->vol_val[i];

		info->dvc->vol_val[3] = max1;
		info->dvc->vol_val[2] = max2;
		info->dvc->vol_val[1] = max3;
		info->dvc->vol_val[0] = BUCK1_table[vol];
		/* Set value to the related regs */
		for (i = 1; i < 4; i++) {
			vol = info->dvc->vol_val[i];
			regmap_update_bits(info->map, PM800_BUCK1 + i, 0x7f,
					   BUCK1_VOL2REG(vol));
		}
	}

	platform_set_drvdata(pdev, info);
#ifdef CONFIG_DEBUG_FS
	if (info->desc.id == PM800_ID_BUCK1)
		pm800_buck1_debugfs_init(info);
#endif
	return 0;
out3:
	gpio_free(info->dvc->dvc2);
out2:
	gpio_free(info->dvc->dvc1);
out1:
	regulator_unregister(info->regulator);
	return -EINVAL;

}

static int __devexit pm800_regulator_remove(struct platform_device *pdev)
{
	struct pm800_regulator_info *info = platform_get_drvdata(pdev);
	if (info->dvc) {
		gpio_free(info->dvc->dvc1);
		gpio_free(info->dvc->dvc2);
	}

	platform_set_drvdata(pdev, NULL);
#ifdef CONFIG_DEBUG_FS
	if (info->debugfs)
		pm800_buck1_debugfs_remove(info);
#endif
	regulator_unregister(info->regulator);
	return 0;
}

static struct platform_driver pm800_regulator_driver = {
	.driver		= {
		.name	= "88pm80x-regulator",
		.owner	= THIS_MODULE,
	},
	.probe		= pm800_regulator_probe,
	.remove		= __devexit_p(pm800_regulator_remove),
};

static int __init pm800_regulator_init(void)
{
	return platform_driver_register(&pm800_regulator_driver);
}
subsys_initcall(pm800_regulator_init);

static void __exit pm800_regulator_exit(void)
{
	platform_driver_unregister(&pm800_regulator_driver);
}
module_exit(pm800_regulator_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Joseph(Yossi) Hanin <yhanin@marvell.com>");
MODULE_DESCRIPTION("Regulator Driver for Marvell 88PM800 PMIC");
MODULE_ALIAS("platform:88pm800-regulator");
