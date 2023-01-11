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

#ifdef CONFIG_DEBUG_FS
#include <linux/debugfs.h>
#endif

#define PM830_A1_VERSION (0x09)
#define PM830_B0_VERSION (0x10)
#define PM830_B1_VERSION (0x11)
#define PM830_B2_VERSION (0x12)

enum {
	PM830_NO_GPADC = -1,
	PM830_GPADC0 = 0,
	PM830_GPADC1,
};

enum {
	PM830_NO_LED = -1,
	PM830_FLASH_LED = 0,
	PM830_TORCH_LED,
};

enum {
	PM830_GPADC_VBAT,
	PM830_GPADC_VSYS,
	PM830_GPADC_VCHG,
	PM830_GPADC_VPWR,
	PM830_GPADC_GPADC0,
	PM830_GPADC_GPADC1,
};

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

#define PM830_INT_EN2			(0x09)
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
#define PM830_SD_PWRUP			(1 << 3)
#define PM830_CC_CTRL2			(0x25)
#define PM830_IBAT_CC1			(0x2B)
#define PM830_CC_IBAT			(0x32)

#define PM830_IBAT_EOC1			(0x33)
#define PM830_IBAT_EOC2			(0x34)

#define PM830_PRE_REGULATOR		(0x3C)
#define PM830_USB_OTG_EN		(1 << 7)

#define PM830_CHG_STATUS3		(0x51)
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
#define PM830_GPADC0_BIAS_EN		(1 << 4)
#define PM830_GPADC1_BIAS_EN		(1 << 5)
#define PM830_GPADC0_GP_BIAS_OUT	(1 << 6)
#define PM830_GPADC1_GP_BIAS_OUT	(1 << 7)

#define PM830_GPADC0_BIAS		(0x70)
#define BIAS_GP0_MASK			(0xF)
#define BIAS_GP0_SET(x)			((x - 1) / 5)

#define PM830_GPADC1_BIAS		(0x71)
#define GP_BIAS_SET(x)			(x << 0)
#define GP_PREBIAS(x)			(x << 4)
#define PM830_GP_BIAS_MASK		(0xf << 0)

#define GP_BIAS_SET(x)			(x << 0)
#define GP_PREBIAS(x)			(x << 4)

#define PM830_VBAT_LOW_TH		(0x76)
#define PM830_GPADC0_LOW_TH		(0x7B)
#define PM830_GPADC1_LOW_TH		(0x7C)
#define PM830_VBAT_UPP_TH		(0x80)
#define PM830_GPADC0_UPP_TH		(0x85)
#define PM830_GPADC1_UPP_TH		(0x86)

#define PM830_VBAT_MEAS1		(0x8A)
#define PM830_VSYS_MEAS1		(0x8C)
#define PM830_VCHG_MEAS1		(0x8E)
#define PM830_VPWR_MEAS1		(0x90)
#define PM830_VCHG_MEAS1		(0x8E)
#define PM830_ITEMP_HI			(0x92)
#define PM830_ITEMP_LO			(0x93)
#define PM830_GPADC0_MEAS1		(0x94)
#define PM830_GPADC0_MEAS2		(0x95)
#define PM830_GPADC1_MEAS1		(0x96)
#define PM830_GPADC1_MEAS2		(0x97)

#define PM830_VBAT_MIN			(0xA2)
#define PM830_VSYS_MIN			(0xA4)
#define PM830_VCHG_MIN			(0xA6)
#define PM830_VPWR_MIN			(0xA8)

#define PM830_VBAT_MAX			(0xAE)
#define PM830_VSYS_MAX			(0xB0)
#define PM830_VCHG_MAX			(0xB2)
#define PM830_VPWR_MAX			(0xBA)

#define PM830_VBAT_AVG			(0xBA)
#define PM830_VSYS_AVG			(0xBC)
#define PM830_VCHG_AVG			(0xBE)
#define PM830_VPWR_AVG			(0xC0)

/*TODO: add more definition */
struct pm830_usb_pdata {
	int	vbus_gpio;
	int	id_gpadc;
};

struct pm830_bat_pdata {
	int ext_storage;
	int bat_ntc; /* bat det by GPADC1 */
	int capacity; /* mAh */
	int r_int; /* mOhm */
	int slp_con; /* slp count in second */
	int range_low_th;
	int range_high_th;
	int power_off_threshold;
	int safe_power_off_threshold;

	unsigned int temp_range_num; /* enable battery temperature feature or not, 3 in max */
	unsigned int gp0_bias_curr[3];
	int switch_thr[2];
	int r_tbat_thr[4];
	unsigned int ntc_table_size;
	int *ntc_table;
	struct i2c_client *client;

	unsigned int ocv_table[100];	/* soc-ocv table */
};

struct pm830_chg_pdata {
	unsigned int prechg_cur;	/* precharge current limit */
	unsigned int prechg_vol;	/* precharge voltage limit */
	unsigned int prechg_timeout;	/* precharge timeout: min*/
	unsigned int fastchg_eoc;	/* fastcharge end current */
	unsigned int fastchg_vol;	/* fastcharge voltage */
	unsigned int fastchg_timeout;	/* fastcharge timeout: hour */
	unsigned int over_vol;

	unsigned int thermal_thr[4];	/* thermal loop threshold */
	unsigned int thermal_dis;	/* thermal loop disable */

	unsigned int temp_cfg;;		/* internal temp config */
	unsigned int temp_thr;		/* internal temp threshold */

	unsigned int mppt_wght;		/* MPPT weight percentage */
	unsigned int mppt_per;		/* MPPT clock period */
	unsigned int mppt_max_cur;	/* MPPT max current value */

	bool charger_control_interface;

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
	unsigned int irq_flags;
	spinlock_t lock;
#ifdef CONFIG_DEBUG_FS
	struct dentry *debugfs;
#endif
	bool obm_config_bat_det;
	int edge_wakeup_gpio;

	int (*get_fg_internal_soc)(void);
};

struct pm830_led_pdata {
	unsigned int cf_en;
	unsigned int cf_txmsk;
};

struct pm830_platform_data {
	struct pm830_usb_pdata *usb;
	struct pm830_bat_pdata *bat;
	struct pm830_chg_pdata *chg;
	struct pm830_led_pdata *led;
	int irq_flags;		/* interrupt flags */
	bool obm_config_bat_det;
	int edge_wakeup_gpio;
};

#ifdef CONFIG_PM
static inline int pm830_dev_suspend(struct device *dev)
{
	return 0;
}

static inline int pm830_dev_resume(struct device *dev)
{
	return 0;
}
#endif

/*
 * GPADC value transformation
 * VBAT, VSYS: 1.367 mV/LSB
 * VCHG, VPWR: 1.709 mV/LSB
 * GPADC0/1: 0.342 mV/LSB
 */
static inline int pm830_get_adc_volt(int type, int val)
{
	int ret;
	switch (type) {
	case PM830_GPADC_VBAT:
	case PM830_GPADC_VSYS:
		ret = (((val) & 0xfff) * 700) >> 9;
		break;
	case PM830_GPADC_VCHG:
	case PM830_GPADC_VPWR:
		ret = (((val) & 0xfff) * 875) >> 9;
		break;
	case PM830_GPADC_GPADC0:
	case PM830_GPADC_GPADC1:
		ret = (((val) & 0xfff) * 175) >> 9;
		break;
	default:
		ret = -EINVAL;
		break;
	}

	return ret;
}

static inline int pm830_get_adc_value(int type, int volt)
{
	int ret;
	switch (type) {
	case PM830_GPADC_VBAT:
	case PM830_GPADC_VSYS:
		ret = ((volt) << 9) / 700;
		break;
	case PM830_GPADC_VCHG:
	case PM830_GPADC_VPWR:
		ret = ((volt) << 9) / 875;
		break;
	case PM830_GPADC_GPADC0:
	case PM830_GPADC_GPADC1:
		ret = ((volt) << 9) / 175;
		break;
	default:
		ret = -EINVAL;
		break;
	}

	if (ret < 0)
		return ret;

	return ret & 0xfff;
}

#endif /* __88PM830_H__*/
