/*
 * Marvell 88PM830 Interface
 *
 * Copyright (C) 2013 Marvell International Ltd.
 * Yi Zhang <yizhang@marvell.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef __88PM830_H__
#define __88PM830_H__

#include <linux/platform_device.h>
#include <linux/interrupt.h>
#include <linux/regmap.h>
#include <linux/atomic.h>
#include <linux/regulator/machine.h>

#define PM830_VERSION_MASK		(0xFF)	/* 830 chip ID mask */
/*88PM830 registers*/

/* chip id register */
#define PM830_CHIP_ID			(0x00)
#define PM830_GEN_ID_MASK		(0x3 << 6)
#define PM830_MJ_ID_MASK		(0x7 << 3)
#define PM830_MI_ID_MASK		(0x7 << 0)

/* status register */
#define PM830_STATUS			(0x01)
#define PM830_VBUS_STATUS		(0x1 << 2)
#define PM830_BAT_DET			(0x1 << 1)
#define PM830_CHG_DET			(0x1 << 0)

/* interrupt status register */
#define PM830_INT_STATUS1		(0x04)
#define PM830_SLP_INT_STS1		(1 << 7)
#define PM830_CHG_FAULT_STS1		(1 << 6)
#define PM830_CHG_TOUT_STS1		(1 << 5)
#define PM830_CHG_DONE_STS1		(1 << 4)
#define PM830_OV_TEMP_INT_STS1		(1 << 3)
#define PM830_CC_INT_STS1		(1 << 2)
#define PM830_BAT_INT_STS1		(1 << 1)
#define PM830_CHG_INT_STS1		(1 << 0)

/* for gpadc */
#define PM830_INT_STATUS2		(0x05)
#define PM830_CFOUT_INT_STS2		(1 << 7)
#define PM830_GPADC1_INT_STS2		(1 << 6)
#define PM830_GPADC0_INT_STS2		(1 << 5)
#define PM830_ITEMP_INT_STS2		(1 << 4)
#define PM830_VPWR_INT_STS2		(1 << 3)
#define PM830_VCHG_INT_STS2		(1 << 2)
#define PM830_VSYS_INT_STS2		(1 << 1)
#define PM830_VBAT_INT_STS2		(1 << 0)

/* interrupt enable register */
#define PM830_INT_EN1			(0x08)
#define PM830_SLP_INT_EN1		(1 << 7)
#define PM830_CHG_FAULT_EN1		(1 << 6)
#define PM830_CHG_TOUT_EN1		(1 << 5)
#define PM830_CHG_DONE_EN1		(1 << 4)
#define PM830_OV_TEMP_INT_EN1		(1 << 3)
#define PM830_CC_INT_EN1		(1 << 2)
#define PM830_BAT_INT_EN1		(1 << 1)
#define PM830_CHG_INT_EN1		(1 << 0)

#define PM830_INT_EN2			(0x05)
#define PM830_CFOUT_INT_EN2		(1 << 7)
#define PM830_GPADC1_INT_EN2		(1 << 6)
#define PM830_GPADC0_INT_EN2		(1 << 5)
#define PM830_ITEMP_INT_EN2		(1 << 4)
#define PM830_VPWR_INT_EN2		(1 << 3)
#define PM830_VCHG_INT_EN2		(1 << 2)
#define PM830_VSYS_INT_EN2		(1 << 1)
#define PM830_VBAT_INT_EN2		(1 << 0)

#define PM830_MISC1			(0x0f)
#define CF_EN_PU_EN			(1 << 7)
#define CF_EN_PD_EN			(1 << 6)
#define CF_MASK_PU_EN			(1 << 5)
#define CF_MASK_PD_EN			(1 << 4)

/* for interrupt control */
#define PM830_INT_MASK			(1 << 2)
#define PM830_INT_CLEAR			(1 << 1)
#define PM830_INV_INT		(1 << 0)

#define PM830_MISC2			(0x10)
#define FGC_CLK_EN			(1 << 0)

/*TODO: add more definition */
#define PM830_MISC3			(0x11)

#define PM830_MISC4			(0x13)
#define I2CPAD_PU_EN			(1 << 6)
#define I2CPAD_SR_MASK			(0x3 << 4)
#define DIGPAD_SR_MASK			(0x3 << 2)
#define VIO1_V18EN			(0x1 << 1)
#define VIO1_PWDN_N			(0x1 << 0)

/*TODO: add more definition */
#define PM830_MISC5			(0x14)

#define PM830_MISC6			(0x16)
#define REF_TSDN_FORCE_EN		(1 << 4)
#define REF_VDDSEL_MASK			(0xf << 0)

/* charger fll register */
#define PM830_CHG_FLL1			(0x1c)
/*TODO: add more definition */

/*
 * coulomb counter register,
 * maybe used in charger & fuel-gauge
 */
#define PM830_CC_CTRL1			(0x24)
#define PM830_CC_CTRL2			(0x25)
#define PM830_CC_IBAT			(0x32)

/*TODO: add more definition */

/* GPADC */
#define PM830_GPADC_MEAS_EN		(0x6A)
#define PM830_CFOUT_MEAS_EN		(1 << 7)
#define PM830_GPADC1_MEAS_EN		(1 << 6)
#define PM830_GPADC0_MEAS_EN		(1 << 5)
#define PM830_ITEMP_MEAS_EN		(1 << 4)
#define PM830_VPWR_MEAS_EN		(1 << 3)
#define PM830_VCHG_MEAS_EN		(1 << 2)
#define PM830_VSYS_MEAS_EN		(1 << 1)
#define PM830_VBAT_MEAS_EN		(1 << 0)

#define PM830_GPADC_CONFIG1		(0x6C)
#define PM830_GPADC_SLOW_MODE(x)	(x << 3)
#define PM830_GPADC_GPFSM_EN		(1 << 0)

#define PM830_GPADC_MEASURE_OFF1	(0x6D)
#define PM830_GPADC_CONFIG2		(0x6E)
#define PM830_BD_GP_SEL_MASK		(1 << 6)
#define PM830_BD_GP_SEL(x)		(x << 6)
#define PM830_BD_EN			(1 << 5)
#define PM830_BD_PREBIAS		(1 << 4)

#define PM830_GPADC_BIAS_ENA		(0x6F)
#define PM830_GPADC_GP_BIAS_OUT(x)	(x << 6)
#define PM830_GPADC_GP_BIAS_EN(x)	(x << 4)
#define PM830_GPADC_BIAS0		(0x70)
#define PM830_GPADC_BIAS1		(0x71)
#define GP_BIAS_SET(x)			(x << 0)
#define GP_PREBIAS(x)			(x << 4)
#define PM830_GP_BIAS_MASK		(0xf << 0)

#define GP_BIAS_SET(x)			(x << 0)
#define GP_PREBIAS(x)			(x << 4)

#define PM830_ITEMP_HI			(0x92)
#define PM830_ITEMP_LO			(0x93)
#define PM830_GPADC0_MEAS1		(0x94)
#define PM830_GPADC1_MEAS1		(0x96)

/*TODO: add more definition */
struct pm830_usb_pdata {
	int	vbus_gpio;
	int	id_gpadc;
};

struct pm830_bat_pdata {
	int bat_ntc; /* bat det by GPADC1 */
	int capacity; /* mAh */
	int r_int; /* mOhm */
	int slp_con; /* slp count in second */
	int range_low_th;
	int range_high_th;
};

struct pm830_chg_pdata {
	unsigned int prechg_cur;	/* precharge current limit */
	unsigned int prechg_vol;	/* precharge voltage limit */
	unsigned int prechg_timeout;	/* precharge timeout: min*/
	unsigned int fastchg_eoc;	/* fastcharge end current */
	unsigned int fastchg_cur;	/* fastcharge current */
	unsigned int fastchg_vol;	/* fastcharge voltage */
	unsigned int fastchg_timeout;	/* fastcharge timeout: hour */
	unsigned int limit_cur;

	unsigned int thermal_thr[4];	/* thermal loop threshold */
	unsigned int thermal_en;	/* thermal loop enable */

	unsigned int temp_cfg;;		/* internal temp config */
	unsigned int temp_thr;		/* internal temp threshold */

	unsigned int mppt_wght;		/* MPPT weight percentage */
	unsigned int mppt_per;		/* MPPT clock period */
	unsigned int mppt_max_cur;	/* MPPT max current value */

	bool	gpadc_nr;

	char **supplied_to;
	size_t num_supplicants;
};

struct pm830_chip {
	struct device *dev;
	struct i2c_client *client;
	struct regmap *regmap;
	struct regmap_irq_chip *regmap_irq_chip;
	struct regmap_irq_chip_data *irq_data;
	unsigned char version;
	int id;
	int irq;
	int irq_mode;
	int irq_base;
	unsigned long wu_flag;
	spinlock_t lock;
#ifdef CONFIG_DEBUG_FS
	struct dentry *debugfs;
#endif
};

struct pm830_platform_data {
	struct pm830_usb_pdata *usb;
	struct pm830_bat_pdata *bat;
	struct pm830_chg_pdata *chg;
	int irq_mode;		/* clear interrupt by read/write(0/1) */
	int batt_det;		/* enable/disable */
	int (*plat_config)(struct pm830_chip *chip,
				struct pm830_platform_data *pdata);
};

#ifdef CONFIG_PM
static inline int pm830_dev_suspend(struct device *dev)
{
	struct platform_device *pdev = to_platform_device(dev);
	struct pm830_chip *chip = dev_get_drvdata(pdev->dev.parent);
	int irq = platform_get_irq(pdev, 0);

	if (device_may_wakeup(dev))
		set_bit(irq, &chip->wu_flag);

	return 0;
}

static inline int pm830_dev_resume(struct device *dev)
{
	struct platform_device *pdev = to_platform_device(dev);
	struct pm830_chip *chip = dev_get_drvdata(pdev->dev.parent);
	int irq = platform_get_irq(pdev, 0);

	if (device_may_wakeup(dev))
		clear_bit(irq, &chip->wu_flag);

	return 0;
}
#endif

#endif /* __88PM830_H__*/
