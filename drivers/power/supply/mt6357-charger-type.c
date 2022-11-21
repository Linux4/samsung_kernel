// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2019 MediaTek Inc.
 * Author Wy Chuang<wy.chuang@mediatek.com>
 */

#include <linux/device.h>
#include <linux/iio/consumer.h>
#include <linux/interrupt.h>
#include <linux/mfd/mt6397/core.h>/* PMIC MFD core header */
#include <linux/module.h>
#include <linux/of.h>
#include <linux/platform_device.h>
#include <linux/regmap.h>
#include <linux/power_supply.h>
#include <mtk_musb.h>
#include <mt-plat/mtk_boot_common.h>
#if defined(CONFIG_VBUS_NOTIFIER)
#include <linux/vbus_notifier.h>
#endif

/* ============================================================ */
/* pmic control start*/
/* ============================================================ */
#define PMIC_RG_BC11_VREF_VTH_ADDR                         0xb98
#define PMIC_RG_BC11_VREF_VTH_MASK                         0x3
#define PMIC_RG_BC11_VREF_VTH_SHIFT                        0

#define PMIC_RG_BC11_CMP_EN_ADDR                           0xb98
#define PMIC_RG_BC11_CMP_EN_MASK                           0x3
#define PMIC_RG_BC11_CMP_EN_SHIFT                          2

#define PMIC_RG_BC11_IPD_EN_ADDR                           0xb98
#define PMIC_RG_BC11_IPD_EN_MASK                           0x3
#define PMIC_RG_BC11_IPD_EN_SHIFT                          4

#define PMIC_RG_BC11_IPU_EN_ADDR                           0xb98
#define PMIC_RG_BC11_IPU_EN_MASK                           0x3
#define PMIC_RG_BC11_IPU_EN_SHIFT                          6

#define PMIC_RG_BC11_BIAS_EN_ADDR                          0xb98
#define PMIC_RG_BC11_BIAS_EN_MASK                          0x1
#define PMIC_RG_BC11_BIAS_EN_SHIFT                         8

#define PMIC_RG_BC11_BB_CTRL_ADDR                          0xb98
#define PMIC_RG_BC11_BB_CTRL_MASK                          0x1
#define PMIC_RG_BC11_BB_CTRL_SHIFT                         9

#define PMIC_RG_BC11_RST_ADDR                              0xb98
#define PMIC_RG_BC11_RST_MASK                              0x1
#define PMIC_RG_BC11_RST_SHIFT                             10

#define PMIC_RG_BC11_VSRC_EN_ADDR                          0xb98
#define PMIC_RG_BC11_VSRC_EN_MASK                          0x3
#define PMIC_RG_BC11_VSRC_EN_SHIFT                         11

#define PMIC_RG_BC11_DCD_EN_ADDR                           0xb98
#define PMIC_RG_BC11_DCD_EN_MASK                           0x1
#define PMIC_RG_BC11_DCD_EN_SHIFT                          13

#define PMIC_RGS_BC11_CMP_OUT_ADDR                         0xb98
#define PMIC_RGS_BC11_CMP_OUT_MASK                         0x1
#define PMIC_RGS_BC11_CMP_OUT_SHIFT                        14

#define PMIC_RGS_CHRDET_ADDR                               0xa88
#define PMIC_RGS_CHRDET_MASK                               0x1
#define PMIC_RGS_CHRDET_SHIFT                              4

#define R_CHARGER_1	330
#define R_CHARGER_2	39

//+Bug 615302,xuejizhou.wt,ADD,20210113,battery Current event and slate mode
#if defined(CONFIG_CHARGER_BQ2415X) || defined(CONFIG_WT_PROJECT_S96717RA1)
static struct delayed_work batt_work;
static struct power_supply *batt_psy;
static int count = 0;
#endif
//-Bug 615302,xuejizhou.wt,ADD,20210113,battery Current event and slate mode

struct mtk_charger_type {
	struct mt6397_chip *chip;
	struct regmap *regmap;
	struct platform_device *pdev;
	struct mutex ops_lock;

	struct power_supply_desc psy_desc;
	struct power_supply_config psy_cfg;
	struct power_supply *psy;
	struct power_supply_desc ac_desc;
	struct power_supply_config ac_cfg;
	struct power_supply *ac_psy;
	struct power_supply_desc usb_desc;
	struct power_supply_config usb_cfg;
	struct power_supply *usb_psy;

	struct iio_channel *chan_vbus;
	struct work_struct chr_work;

	enum power_supply_usb_type type;

	int first_connect;
	int bc12_active;
#ifdef CONFIG_WT_PROJECT_S96717RA1
	int polarity_state;
#endif
};

static enum power_supply_property chr_type_properties[] = {
	POWER_SUPPLY_PROP_ONLINE,
	POWER_SUPPLY_PROP_USB_TYPE,
	POWER_SUPPLY_PROP_VOLTAGE_NOW,
};

static enum power_supply_property mt_ac_properties[] = {
	POWER_SUPPLY_PROP_ONLINE,
};

static enum power_supply_property mt_usb_properties[] = {
	POWER_SUPPLY_PROP_ONLINE,
	POWER_SUPPLY_PROP_CURRENT_MAX,
	POWER_SUPPLY_PROP_VOLTAGE_MAX,
	//+Bug 623286, yaocankun.wt,add, 20210202, Charging afc flag
#ifdef CONFIG_AFC_CHARGER
	POWER_SUPPLY_PROP_AFC_FLAG,
#endif
	//-Bug 623286, yaocankun.wt,add, 20210202, Charging afc flag
#ifdef CONFIG_WT_PROJECT_S96717RA1
	POWER_SUPPLY_PROP_TYPEC_POLARITY,
#endif
};
#ifdef CONFIG_AFC_CHARGER
extern int g_afc_work_status;//Bug 623286, yaocankun.wt,add, 20210202, Charging afc flag
#endif

void bc11_set_register_value(struct regmap *map,
	unsigned int addr,
	unsigned int mask,
	unsigned int shift,
	unsigned int val)
{
	regmap_update_bits(map,
		addr,
		mask << shift,
		val << shift);
}

unsigned int bc11_get_register_value(struct regmap *map,
	unsigned int addr,
	unsigned int mask,
	unsigned int shift)
{
	unsigned int value = 0;

	regmap_read(map, addr, &value);
	value =
		(value &
		(mask << shift))
		>> shift;
	return value;
}

#if defined(CONFIG_CHARGER_BQ2415X) || defined(CONFIG_WT_PROJECT_S96717RA1)
extern int g_bootmode;
#endif
static void hw_bc11_init(struct mtk_charger_type *info)
{
#if IS_ENABLED(CONFIG_USB_MTK_HDRC)
	int timeout = 200;
#endif
	msleep(200);
//+EXTB P210404-01933,xuejizhou.wt,add,20210426,charge type detecion takes more than 20 sec in lpm mode
#if defined(CONFIG_CHARGER_BQ2415X) || defined(CONFIG_WT_PROJECT_S96717RA1)
	if (g_bootmode == KERNEL_POWER_OFF_CHARGING_BOOT ||
	    g_bootmode == LOW_POWER_OFF_CHARGING_BOOT) {
		pr_info("Skip usb_ready check in KPOC\n");
		goto skip;
	}
#endif
//-EXTB P210404-01933,xuejizhou.wt,add,20210426,charge type detecion takes more than 20 sec in lpm mode
	if (info->first_connect == true) {
#if IS_ENABLED(CONFIG_USB_MTK_HDRC)
		/* add make sure USB Ready */
		if (is_usb_rdy() == false) {
			pr_info("CDP, block\n");
			while (is_usb_rdy() == false && timeout > 0) {
				msleep(100);
				timeout--;
			}
			if (timeout == 0)
				pr_info("CDP, timeout\n");
			else
				pr_info("CDP, free\n");
		} else
			pr_info("CDP, PASS\n");
#endif
		info->first_connect = false;
	}
#if defined(CONFIG_CHARGER_BQ2415X) || defined(CONFIG_WT_PROJECT_S96717RA1)
skip:
#endif
	/* RG_bc11_BIAS_EN=1 */
	bc11_set_register_value(info->regmap,
		PMIC_RG_BC11_BIAS_EN_ADDR,
		PMIC_RG_BC11_BIAS_EN_MASK,
		PMIC_RG_BC11_BIAS_EN_SHIFT,
		1);
	/* RG_bc11_VSRC_EN[1:0]=00 */
	bc11_set_register_value(info->regmap,
		PMIC_RG_BC11_VSRC_EN_ADDR,
		PMIC_RG_BC11_VSRC_EN_MASK,
		PMIC_RG_BC11_VSRC_EN_SHIFT,
		0);
	/* RG_bc11_VREF_VTH = [1:0]=00 */
	bc11_set_register_value(info->regmap,
		PMIC_RG_BC11_VREF_VTH_ADDR,
		PMIC_RG_BC11_VREF_VTH_MASK,
		PMIC_RG_BC11_VREF_VTH_SHIFT,
		0);
	/* RG_bc11_CMP_EN[1.0] = 00 */
	bc11_set_register_value(info->regmap,
		PMIC_RG_BC11_CMP_EN_ADDR,
		PMIC_RG_BC11_CMP_EN_MASK,
		PMIC_RG_BC11_CMP_EN_SHIFT,
		0);
	/* RG_bc11_IPU_EN[1.0] = 00 */
	bc11_set_register_value(info->regmap,
		PMIC_RG_BC11_IPU_EN_ADDR,
		PMIC_RG_BC11_IPU_EN_MASK,
		PMIC_RG_BC11_IPU_EN_SHIFT,
		0);
	/* RG_bc11_IPD_EN[1.0] = 00 */
	bc11_set_register_value(info->regmap,
		PMIC_RG_BC11_IPD_EN_ADDR,
		PMIC_RG_BC11_IPD_EN_MASK,
		PMIC_RG_BC11_IPD_EN_SHIFT,
		0);
	/* bc11_RST=1 */
	bc11_set_register_value(info->regmap,
		PMIC_RG_BC11_RST_ADDR,
		PMIC_RG_BC11_RST_MASK,
		PMIC_RG_BC11_RST_SHIFT,
		1);
	/* bc11_BB_CTRL=1 */
	bc11_set_register_value(info->regmap,
		PMIC_RG_BC11_BB_CTRL_ADDR,
		PMIC_RG_BC11_BB_CTRL_MASK,
		PMIC_RG_BC11_BB_CTRL_SHIFT,
		1);
	/* add pull down to prevent PMIC leakage */
	bc11_set_register_value(info->regmap,
		PMIC_RG_BC11_IPD_EN_ADDR,
		PMIC_RG_BC11_IPD_EN_MASK,
		PMIC_RG_BC11_IPD_EN_SHIFT,
		0x1);
	msleep(50);

#if IS_ENABLED(CONFIG_USB_MTK_HDRC)
	Charger_Detect_Init();
#endif
}

static unsigned int hw_bc11_DCD(struct mtk_charger_type *info)
{
	unsigned int wChargerAvail = 0;
	/* RG_bc11_IPU_EN[1.0] = 10 */
	bc11_set_register_value(info->regmap,
		PMIC_RG_BC11_IPU_EN_ADDR,
		PMIC_RG_BC11_IPU_EN_MASK,
		PMIC_RG_BC11_IPU_EN_SHIFT,
		0x2);
	/* RG_bc11_IPD_EN[1.0] = 01 */
	bc11_set_register_value(info->regmap,
		PMIC_RG_BC11_IPD_EN_ADDR,
		PMIC_RG_BC11_IPD_EN_MASK,
		PMIC_RG_BC11_IPD_EN_SHIFT,
		0x1);
	/* RG_bc11_VREF_VTH = [1:0]=01 */
	bc11_set_register_value(info->regmap,
		PMIC_RG_BC11_VREF_VTH_ADDR,
		PMIC_RG_BC11_VREF_VTH_MASK,
		PMIC_RG_BC11_VREF_VTH_SHIFT,
		0x1);
	/* RG_bc11_CMP_EN[1.0] = 10 */
	bc11_set_register_value(info->regmap,
		PMIC_RG_BC11_CMP_EN_ADDR,
		PMIC_RG_BC11_CMP_EN_MASK,
		PMIC_RG_BC11_CMP_EN_SHIFT,
		0x2);
	msleep(80);
	/* mdelay(80); */
	wChargerAvail = bc11_get_register_value(info->regmap,
		PMIC_RGS_BC11_CMP_OUT_ADDR,
		PMIC_RGS_BC11_CMP_OUT_MASK,
		PMIC_RGS_BC11_CMP_OUT_SHIFT);

	/* RG_bc11_IPU_EN[1.0] = 00 */
	bc11_set_register_value(info->regmap,
		PMIC_RG_BC11_IPU_EN_ADDR,
		PMIC_RG_BC11_IPU_EN_MASK,
		PMIC_RG_BC11_IPU_EN_SHIFT,
		0x0);
	/* RG_bc11_IPD_EN[1.0] = 00 */
	bc11_set_register_value(info->regmap,
		PMIC_RG_BC11_IPD_EN_ADDR,
		PMIC_RG_BC11_IPD_EN_MASK,
		PMIC_RG_BC11_IPD_EN_SHIFT,
		0x0);
	/* RG_bc11_CMP_EN[1.0] = 00 */
	bc11_set_register_value(info->regmap,
		PMIC_RG_BC11_CMP_EN_ADDR,
		PMIC_RG_BC11_CMP_EN_MASK,
		PMIC_RG_BC11_CMP_EN_SHIFT,
		0x0);
	/* RG_bc11_VREF_VTH = [1:0]=00 */
	bc11_set_register_value(info->regmap,
		PMIC_RG_BC11_VREF_VTH_ADDR,
		PMIC_RG_BC11_VREF_VTH_MASK,
		PMIC_RG_BC11_VREF_VTH_SHIFT,
		0x0);
	return wChargerAvail;
}

static unsigned int hw_bc11_stepA2(struct mtk_charger_type *info)
{
	unsigned int wChargerAvail = 0;

	/* RG_bc11_VSRC_EN[1.0] = 10 */
	bc11_set_register_value(info->regmap,
		PMIC_RG_BC11_VSRC_EN_ADDR,
		PMIC_RG_BC11_VSRC_EN_MASK,
		PMIC_RG_BC11_VSRC_EN_SHIFT,
		0x2);
	/* RG_bc11_IPD_EN[1:0] = 01 */
	bc11_set_register_value(info->regmap,
		PMIC_RG_BC11_IPD_EN_ADDR,
		PMIC_RG_BC11_IPD_EN_MASK,
		PMIC_RG_BC11_IPD_EN_SHIFT,
		0x1);
	/* RG_bc11_VREF_VTH = [1:0]=00 */
	bc11_set_register_value(info->regmap,
		PMIC_RG_BC11_VREF_VTH_ADDR,
		PMIC_RG_BC11_VREF_VTH_MASK,
		PMIC_RG_BC11_VREF_VTH_SHIFT,
		0x0);
	/* RG_bc11_CMP_EN[1.0] = 01 */
	bc11_set_register_value(info->regmap,
		PMIC_RG_BC11_CMP_EN_ADDR,
		PMIC_RG_BC11_CMP_EN_MASK,
		PMIC_RG_BC11_CMP_EN_SHIFT,
		0x1);
	msleep(80);
	/* mdelay(80); */
	wChargerAvail = bc11_get_register_value(info->regmap,
					PMIC_RGS_BC11_CMP_OUT_ADDR,
					PMIC_RGS_BC11_CMP_OUT_MASK,
					PMIC_RGS_BC11_CMP_OUT_SHIFT);

	/* RG_bc11_VSRC_EN[1:0]=00 */
	bc11_set_register_value(info->regmap,
		PMIC_RG_BC11_VSRC_EN_ADDR,
		PMIC_RG_BC11_VSRC_EN_MASK,
		PMIC_RG_BC11_VSRC_EN_SHIFT,
		0x0);
	/* RG_bc11_IPD_EN[1.0] = 00 */
	bc11_set_register_value(info->regmap,
		PMIC_RG_BC11_IPD_EN_ADDR,
		PMIC_RG_BC11_IPD_EN_MASK,
		PMIC_RG_BC11_IPD_EN_SHIFT,
		0x0);
	/* RG_bc11_CMP_EN[1.0] = 00 */
	bc11_set_register_value(info->regmap,
		PMIC_RG_BC11_CMP_EN_ADDR,
		PMIC_RG_BC11_CMP_EN_MASK,
		PMIC_RG_BC11_CMP_EN_SHIFT,
		0x0);
	return wChargerAvail;
}

static unsigned int hw_bc11_stepB2(struct mtk_charger_type *info)
{
	unsigned int wChargerAvail = 0;

	/*enable the voltage source to DM*/
	bc11_set_register_value(info->regmap,
		PMIC_RG_BC11_VSRC_EN_ADDR,
		PMIC_RG_BC11_VSRC_EN_MASK,
		PMIC_RG_BC11_VSRC_EN_SHIFT,
		0x1);
	/* enable the pull-down current to DP */
	bc11_set_register_value(info->regmap,
		PMIC_RG_BC11_IPD_EN_ADDR,
		PMIC_RG_BC11_IPD_EN_MASK,
		PMIC_RG_BC11_IPD_EN_SHIFT,
		0x2);
	/* VREF threshold voltage for comparator  =0.325V */
	bc11_set_register_value(info->regmap,
		PMIC_RG_BC11_VREF_VTH_ADDR,
		PMIC_RG_BC11_VREF_VTH_MASK,
		PMIC_RG_BC11_VREF_VTH_SHIFT,
		0x0);
	/* enable the comparator to DP */
	bc11_set_register_value(info->regmap,
		PMIC_RG_BC11_CMP_EN_ADDR,
		PMIC_RG_BC11_CMP_EN_MASK,
		PMIC_RG_BC11_CMP_EN_SHIFT,
		0x2);
	msleep(80);
	wChargerAvail = bc11_get_register_value(info->regmap,
		PMIC_RGS_BC11_CMP_OUT_ADDR,
		PMIC_RGS_BC11_CMP_OUT_MASK,
		PMIC_RGS_BC11_CMP_OUT_SHIFT);
	/*reset to default value*/
	bc11_set_register_value(info->regmap,
		PMIC_RG_BC11_VSRC_EN_ADDR,
		PMIC_RG_BC11_VSRC_EN_MASK,
		PMIC_RG_BC11_VSRC_EN_SHIFT,
		0x0);
	bc11_set_register_value(info->regmap,
		PMIC_RG_BC11_IPD_EN_ADDR,
		PMIC_RG_BC11_IPD_EN_MASK,
		PMIC_RG_BC11_IPD_EN_SHIFT,
		0x0);
	bc11_set_register_value(info->regmap,
		PMIC_RG_BC11_CMP_EN_ADDR,
		PMIC_RG_BC11_CMP_EN_MASK,
		PMIC_RG_BC11_CMP_EN_SHIFT,
		0x0);
	if (wChargerAvail == 1) {
		bc11_set_register_value(info->regmap,
			PMIC_RG_BC11_VSRC_EN_ADDR,
			PMIC_RG_BC11_VSRC_EN_MASK,
			PMIC_RG_BC11_VSRC_EN_SHIFT,
			0x2);
		pr_info("charger type: DCP, keep DM voltage source in stepB2\n");
	}
	return wChargerAvail;

}

static void hw_bc11_done(struct mtk_charger_type *info)
{
	/* RG_bc11_VSRC_EN[1:0]=00 */
	bc11_set_register_value(info->regmap,
		PMIC_RG_BC11_VSRC_EN_ADDR,
		PMIC_RG_BC11_VSRC_EN_MASK,
		PMIC_RG_BC11_VSRC_EN_SHIFT,
		0x0);
	/* RG_bc11_VREF_VTH = [1:0]=0 */
	bc11_set_register_value(info->regmap,
		PMIC_RG_BC11_VREF_VTH_ADDR,
		PMIC_RG_BC11_VREF_VTH_MASK,
		PMIC_RG_BC11_VREF_VTH_SHIFT,
		0x0);
	/* RG_bc11_CMP_EN[1.0] = 00 */
	bc11_set_register_value(info->regmap,
		PMIC_RG_BC11_CMP_EN_ADDR,
		PMIC_RG_BC11_CMP_EN_MASK,
		PMIC_RG_BC11_CMP_EN_SHIFT,
		0x0);
	/* RG_bc11_IPU_EN[1.0] = 00 */
	bc11_set_register_value(info->regmap,
		PMIC_RG_BC11_IPU_EN_ADDR,
		PMIC_RG_BC11_IPU_EN_MASK,
		PMIC_RG_BC11_IPU_EN_SHIFT,
		0x0);
	/* RG_bc11_IPD_EN[1.0] = 00 */
	bc11_set_register_value(info->regmap,
		PMIC_RG_BC11_IPD_EN_ADDR,
		PMIC_RG_BC11_IPD_EN_MASK,
		PMIC_RG_BC11_IPD_EN_SHIFT,
		0x0);
	/* RG_bc11_BIAS_EN=0 */
	bc11_set_register_value(info->regmap,
		PMIC_RG_BC11_BIAS_EN_ADDR,
		PMIC_RG_BC11_BIAS_EN_MASK,
		PMIC_RG_BC11_BIAS_EN_SHIFT,
		0x0);

#if IS_ENABLED(CONFIG_USB_MTK_HDRC)
	Charger_Detect_Release();
#endif
}

static void dump_charger_name(int type)
{
	switch (type) {
	case POWER_SUPPLY_TYPE_UNKNOWN:
		pr_err("charger type: %d, CHARGER_UNKNOWN\n", type);
		break;
	case POWER_SUPPLY_TYPE_USB:
		pr_err("charger type: %d, Standard USB Host\n", type);
		break;
	case POWER_SUPPLY_TYPE_USB_CDP:
		pr_err("charger type: %d, Charging USB Host\n", type);
		break;
#ifdef FIXME
	case POWER_SUPPLY_TYPE_USB_FLOAT:
		pr_err("charger type: %d, Non-standard Charger\n", type);
		break;
#endif
	case POWER_SUPPLY_TYPE_USB_DCP:
		pr_err("charger type: %d, Standard Charger\n", type);
		break;
	default:
		pr_err("charger type: %d, Not Defined!!!\n", type);
		break;
	}
}

static int get_charger_type(struct mtk_charger_type *info)
{
	enum power_supply_usb_type type;

	hw_bc11_init(info);
	if (hw_bc11_DCD(info)) {
		info->psy_desc.type = POWER_SUPPLY_TYPE_USB;
		type = POWER_SUPPLY_USB_TYPE_DCP;
	} else {
		if (hw_bc11_stepA2(info)) {
			if (hw_bc11_stepB2(info)) {
				info->psy_desc.type = POWER_SUPPLY_TYPE_USB_DCP;
				type = POWER_SUPPLY_USB_TYPE_DCP;
			} else {
				info->psy_desc.type = POWER_SUPPLY_TYPE_USB_CDP;
				type = POWER_SUPPLY_USB_TYPE_CDP;
			}
		} else {
			info->psy_desc.type = POWER_SUPPLY_TYPE_USB;
			type = POWER_SUPPLY_USB_TYPE_SDP;
		}
	}
//+EXTB P210404-01933,xuejizhou.wt,add,20210426,charge type detecion takes more than 20 sec in lpm mode
#if defined(CONFIG_CHARGER_BQ2415X) || defined(CONFIG_WT_PROJECT_S96717RA1)
	if (type == POWER_SUPPLY_USB_TYPE_CDP) {
		if (g_bootmode == KERNEL_POWER_OFF_CHARGING_BOOT ||
		    g_bootmode == LOW_POWER_OFF_CHARGING_BOOT) {
			pr_info("Pull up D+ to 0.6V for CDP in KPOC\n");
			msleep(100);
			bc11_set_register_value(info->regmap,
				PMIC_RG_BC11_VSRC_EN_ADDR,
				PMIC_RG_BC11_VSRC_EN_MASK,
				PMIC_RG_BC11_VSRC_EN_SHIFT,
				0x2);
			bc11_set_register_value(info->regmap,
				PMIC_RG_BC11_IPU_EN_ADDR,
				PMIC_RG_BC11_IPU_EN_MASK,
				PMIC_RG_BC11_IPU_EN_SHIFT,
				0x2);
		} else
			hw_bc11_done(info);
	} else if (type != POWER_SUPPLY_USB_TYPE_DCP)
#else
	if (type != POWER_SUPPLY_USB_TYPE_DCP)
#endif
//-EXTB P210404-01933,xuejizhou.wt,add,20210426,charge type detecion takes more than 20 sec in lpm mode
		hw_bc11_done(info);
	else
		pr_info("charger type: skip bc11 release for BC12 DCP SPEC\n");

	dump_charger_name(info->psy_desc.type);

	return type;
}

static int get_vbus_voltage(struct mtk_charger_type *info,
	int *val)
{
	int ret;

	if (!IS_ERR(info->chan_vbus)) {
		ret = iio_read_channel_processed(info->chan_vbus, val);
		if (ret < 0)
			pr_notice("[%s]read fail,ret=%d\n", __func__, ret);
	} else {
		pr_notice("[%s]chan error %d\n", __func__, info->chan_vbus);
		ret = -ENOTSUPP;
	}

	*val = (((R_CHARGER_1 +
			R_CHARGER_2) * 100 * *val) /
			R_CHARGER_2) / 100;

	return ret;
}


void do_charger_detect(struct mtk_charger_type *info, bool en)
{
	union power_supply_propval prop, prop2, prop3;
	int ret = 0;

#ifndef CONFIG_TCPC_CLASS
	if (!mt_usb_is_device()) {
		pr_info("charger type: UNKNOWN, Now is usb host mode. Skip detection\n");
		return;
	}
#endif

	prop.intval = en;
	if (en) {
		ret = power_supply_set_property(info->psy,
				POWER_SUPPLY_PROP_ONLINE, &prop);
		ret = power_supply_get_property(info->psy,
				POWER_SUPPLY_PROP_TYPE, &prop2);
		ret = power_supply_get_property(info->psy,
				POWER_SUPPLY_PROP_USB_TYPE, &prop3);
		#ifdef CONFIG_CHARGER_BQ2415X
		schedule_delayed_work(&batt_work, msecs_to_jiffies(0));//Bug 615302,xuejizhou.wt,ADD,20210113,battery Current event and slate mode
		#endif
	} else {
		prop2.intval = POWER_SUPPLY_TYPE_UNKNOWN;
		prop3.intval = POWER_SUPPLY_USB_TYPE_UNKNOWN;
		info->psy_desc.type = POWER_SUPPLY_TYPE_UNKNOWN;
		info->type = POWER_SUPPLY_USB_TYPE_UNKNOWN;
		#ifdef CONFIG_CHARGER_BQ2415X
		count = 0;
		#endif
	}

	pr_notice("%s type:%d usb_type:%d\n", __func__, prop2.intval, prop3.intval);

	power_supply_changed(info->psy);
}

static void do_charger_detection_work(struct work_struct *data)
{
	struct mtk_charger_type *info = (struct mtk_charger_type *)container_of(
				     data, struct mtk_charger_type, chr_work);
	unsigned int chrdet = 0;
#if defined(CONFIG_VBUS_NOTIFIER)
	vbus_status_t status = 0;
#endif
	chrdet = bc11_get_register_value(info->regmap,
		PMIC_RGS_CHRDET_ADDR,
		PMIC_RGS_CHRDET_MASK,
		PMIC_RGS_CHRDET_SHIFT);

	pr_notice("%s: chrdet:%d\n", __func__, chrdet);

#if defined(CONFIG_VBUS_NOTIFIER)
	status = (chrdet > 0) ?
		STATUS_VBUS_HIGH : STATUS_VBUS_LOW;
	
	pr_info("usb: %s: chrdet:%d\n", __func__, chrdet);
	vbus_notifier_handle(status);
#endif

	if (chrdet)
		do_charger_detect(info, chrdet);
}


irqreturn_t chrdet_int_handler(int irq, void *data)
{
	struct mtk_charger_type *info = data;
	unsigned int chrdet = 0;
#if defined(CONFIG_VBUS_NOTIFIER)
	vbus_status_t status = 0;
#endif
	chrdet = bc11_get_register_value(info->regmap,
		PMIC_RGS_CHRDET_ADDR,
		PMIC_RGS_CHRDET_MASK,
		PMIC_RGS_CHRDET_SHIFT);

	pr_notice("%s: chrdet:%d\n", __func__, chrdet);

#if defined(CONFIG_VBUS_NOTIFIER)
	status = (chrdet > 0) ?
		STATUS_VBUS_HIGH : STATUS_VBUS_LOW;

	pr_info("usb: %s: chrdet:%d\n", __func__, chrdet);
	vbus_notifier_handle(status);
#endif

	do_charger_detect(info, chrdet);

	return IRQ_HANDLED;
}


static int psy_chr_type_get_property(struct power_supply *psy,
	enum power_supply_property psp, union power_supply_propval *val)
{
	struct mtk_charger_type *info;
	int vbus = 0;

	pr_notice("%s: prop:%d\n", __func__, psp);
	info = (struct mtk_charger_type *)power_supply_get_drvdata(psy);

	switch (psp) {
	case POWER_SUPPLY_PROP_ONLINE:
		if (info->type == POWER_SUPPLY_USB_TYPE_UNKNOWN)
			val->intval = 0;
		else
			val->intval = 1;
		break;
	case POWER_SUPPLY_PROP_TYPE:
		 val->intval = info->psy_desc.type;
		break;
	case POWER_SUPPLY_PROP_USB_TYPE:
		val->intval = info->type;
		break;
	case POWER_SUPPLY_PROP_VOLTAGE_NOW:
		get_vbus_voltage(info, &vbus);
		val->intval = vbus;
		break;
	default:
		return -EINVAL;
	}

	return 0;
}

int psy_chr_type_set_property(struct power_supply *psy,
			enum power_supply_property psp,
			const union power_supply_propval *val)
{
	struct mtk_charger_type *info;

	pr_notice("%s: prop:%d %d\n", __func__, psp, val->intval);

	info = (struct mtk_charger_type *)power_supply_get_drvdata(psy);
	switch (psp) {
	case POWER_SUPPLY_PROP_ONLINE:
		info->type = get_charger_type(info);
//+bug 621775,yaocankun.wt,add,20210115,add for n21 charger bring up
#ifdef CONFIG_MT6370_PMU_CHARGER
		if(val->intval == 0)
		{
			info->type = POWER_SUPPLY_USB_TYPE_UNKNOWN;
			info->psy_desc.type = POWER_SUPPLY_TYPE_UNKNOWN;
		}
#endif
//+Bug 623221, yaocankun.wt,add, 20210225, add charge current event
#ifdef CONFIG_WT_PROJECT_S96717RA1
	if(val->intval == 0)
		count = 0;
	else
		schedule_delayed_work(&batt_work, msecs_to_jiffies(0));
#endif
//-Bug 623221, yaocankun.wt,add, 20210225, add charge current event
//-bug 621775,yaocankun.wt,add,20210115,add for n21 charger bring up
#ifdef CONFIG_CHARGER_BQ2415X
	if (val->intval == true) {
		if (info->type == POWER_SUPPLY_USB_TYPE_DCP &&
				info->psy_desc.type == POWER_SUPPLY_TYPE_USB) {
					pr_err("%s non_std retry bc1.2\n", __func__);
				info->type = get_charger_type(info);
				if (info->type == POWER_SUPPLY_USB_TYPE_DCP &&
						info->psy_desc.type == POWER_SUPPLY_TYPE_USB) {
					info->type = POWER_SUPPLY_USB_TYPE_SDP;
					info->psy_desc.type = POWER_SUPPLY_TYPE_USB;
				}
		}
	}
#endif
		break;
#ifdef CONFIG_MT6370_PMU_CHARGER
	case POWER_SUPPLY_PROP_USB_TYPE:
		info->type = val->intval;
		pr_err("force charger type to %d\n", val->intval);
#endif
	default:
		return -EINVAL;
	}

	return 0;
}

static int mt_ac_get_property(struct power_supply *psy,
	enum power_supply_property psp, union power_supply_propval *val)
{
	struct mtk_charger_type *info;

	info = (struct mtk_charger_type *)power_supply_get_drvdata(psy);
	switch (psp) {
	case POWER_SUPPLY_PROP_ONLINE:
		val->intval = 0;
		/* Force to 1 in all charger type */
		if (info->type != POWER_SUPPLY_USB_TYPE_UNKNOWN)
			val->intval = 1;
		/* Reset to 0 if charger type is USB */
		if ((info->type == POWER_SUPPLY_USB_TYPE_SDP) ||
			(info->type == POWER_SUPPLY_USB_TYPE_CDP))
			val->intval = 0;
		break;
	default:
		return -EINVAL;
	}

	return 0;
}
//+Bug 627659, yaocankun.wt, add, 20210219, add typec polarity node
#ifdef CONFIG_WT_PROJECT_S96717RA1
int wt_set_cc_polarity_state(int state)
{
	static struct power_supply *chg_psy;
	struct mtk_charger_type *info;

	if (chg_psy == NULL)
			chg_psy = power_supply_get_by_name("mtk_charger_type");

	if (IS_ERR_OR_NULL(chg_psy)){
		pr_notice("%s Couldn't get chg_psy\n", __func__);
	} else {
		info = (struct mtk_charger_type *)power_supply_get_drvdata(chg_psy);
		info->polarity_state = state;
	}
	return 0;
}
#endif
//-Bug 627659, yaocankun.wt, add, 20210219, add typec polarity node

static int mt_usb_get_property(struct power_supply *psy,
	enum power_supply_property psp, union power_supply_propval *val)
{
	struct mtk_charger_type *info;

	info = (struct mtk_charger_type *)power_supply_get_drvdata(psy);

	switch (psp) {
	case POWER_SUPPLY_PROP_ONLINE:
		if ((info->type == POWER_SUPPLY_USB_TYPE_SDP) ||
			(info->type == POWER_SUPPLY_USB_TYPE_CDP))
			val->intval = 1;
		else
			val->intval = 0;
		break;
	case POWER_SUPPLY_PROP_CURRENT_MAX:
		val->intval = 500000;
		break;
	case POWER_SUPPLY_PROP_VOLTAGE_MAX:
		val->intval = 5000000;
		break;
	//+Bug 623286, yaocankun.wt,add, 20210202, Charging afc flag
#ifdef CONFIG_AFC_CHARGER
	case POWER_SUPPLY_PROP_AFC_FLAG:
		val->intval = g_afc_work_status;
		break;
#endif
	//-Bug 623286, yaocankun.wt,add, 20210202, Charging afc flag
//+Bug 627659, yaocankun.wt, add, 20210219, add typec polarity node
#ifdef CONFIG_WT_PROJECT_S96717RA1
	case POWER_SUPPLY_PROP_TYPEC_POLARITY:
		val->intval = info->polarity_state;
		break;
#endif
//-Bug 627659, yaocankun.wt, add, 20210219, add typec polarity node
	default:
		return -EINVAL;
	}

	return 0;
}

static int psy_charger_type_property_is_writeable(struct power_supply *psy,
					       enum power_supply_property psp)
{
	switch (psp) {
	case POWER_SUPPLY_PROP_ONLINE:
#ifdef CONFIG_MT6370_PMU_CHARGER
	case POWER_SUPPLY_PROP_USB_TYPE:
#endif
		return 1;
	default:
		return 0;
	}
}

static enum power_supply_usb_type mt6357_charger_usb_types[] = {
	POWER_SUPPLY_USB_TYPE_UNKNOWN,
	POWER_SUPPLY_USB_TYPE_SDP,
	POWER_SUPPLY_USB_TYPE_DCP,
	POWER_SUPPLY_USB_TYPE_CDP,
};

static char *mt6357_charger_supplied_to[] = {
	"battery",
	"mtk-master-charger"
};

//+Bug 615302,xuejizhou.wt,ADD,20210113,battery Current event and slate mode
#define low_current_level 100
#if defined(CONFIG_CHARGER_BQ2415X) || defined(CONFIG_WT_PROJECT_S96717RA1)
static void battery_current_monitoring_work(struct work_struct *data)
{
	struct mtk_charger_type *info;
	union power_supply_propval val;
	
	int ret= 0;

	//+Bug 623221, yaocankun.wt,add, 20210225, add charge current event
	#ifndef CONFIG_WT_PROJECT_S96717RA1
	info = (struct mtk_charger_type *)container_of(
				     data, struct mtk_charger_type, chr_work);

	#else
	struct power_supply *psy, *psy_master;

	psy_master = power_supply_get_by_name("mtk-master-charger");
	if(psy_master == NULL || IS_ERR(psy_master))
	{
		pr_err("mtk-master-charger hasn't register\n");
		return;
	}
	psy = power_supply_get_by_name("mtk_charger_type");
	if(psy == NULL || IS_ERR(psy))
	{
		pr_err("mtk_charger_type hasn't register\n");
		return;
	}
	info = (struct mtk_charger_type *)power_supply_get_drvdata(psy);
	if(!info){
		pr_err("mtk_charger_type hasn't init\n");
		return;
	}
	if(!batt_psy){
		batt_psy = power_supply_get_by_name("battery");
		if (!batt_psy) {
			pr_notice("%s: get power supply failed\n", __func__);
			schedule_delayed_work(&batt_work, msecs_to_jiffies(1000));
		return;
		}
	}
	pr_err("battery_current_monitoring_work working\n");
	#endif
	//-Bug 623221, yaocankun.wt,add, 20210225, add charge current event

	if(info->type == POWER_SUPPLY_USB_TYPE_SDP){
		if(count > 2) {
			count = 0;
			val.intval = 64;
			ret = power_supply_set_property(batt_psy, POWER_SUPPLY_PROP_BATT_CURRENT_EVENT, &val);
			if (ret < 0)
				pr_debug("%s: psy set property failed, ret = %d\n", __func__, ret);
		} else {
	//+Bug 623221, yaocankun.wt,add, 20210225, add charge current event
			#ifdef CONFIG_WT_PROJECT_S96717RA1
			ret = power_supply_get_property(psy_master, POWER_SUPPLY_PROP_INPUT_CURRENT_LIMIT, &val);
			#else
			ret = power_supply_get_property(batt_psy, POWER_SUPPLY_PROP_INPUT_CURRENT_LIMIT, &val);
			#endif
			if (ret < 0)
				pr_debug("%s: psy get property failed, ret = %d\n", __func__, ret);

			#ifdef CONFIG_WT_PROJECT_S96717RA1
			if (100000 == val.intval)
			#else
			if (low_current_level == val.intval)
			#endif
	//-Bug 623221, yaocankun.wt,add, 20210225, add charge current event
			{
				count ++;
				schedule_delayed_work(&batt_work, msecs_to_jiffies(1000));
			}
		}
	}
	else
	{
		count = 0;// reset counter when charger type changed.
	}
}
#endif
//-Bug 615302,xuejizhou.wt,ADD,20210113,battery Current event and slate mode

static int mt6357_charger_type_probe(struct platform_device *pdev)
{
	struct mtk_charger_type *info;
	struct iio_channel *chan_vbus;
	struct device *dev = &pdev->dev;
	struct device_node *np = dev->of_node;
	int ret = 0;

	pr_notice("%s: starts\n", __func__);

	chan_vbus = devm_iio_channel_get(
		&pdev->dev, "pmic_vbus");
	if (IS_ERR(chan_vbus)) {
		pr_notice("mt6357 charger type requests probe deferral ret:%d\n",
			chan_vbus);
		return -EPROBE_DEFER;
	}

	info = devm_kzalloc(&pdev->dev, sizeof(*info),
		GFP_KERNEL);
	if (!info)
		return -ENOMEM;

	info->chip = (struct mt6397_chip *)dev_get_drvdata(
		pdev->dev.parent);
	info->regmap = info->chip->regmap;

	dev_set_drvdata(&pdev->dev, info);
	info->pdev = pdev;
	mutex_init(&info->ops_lock);

	info->psy_desc.name = "mtk_charger_type";
	info->psy_desc.type = POWER_SUPPLY_TYPE_UNKNOWN;
	info->psy_desc.properties = chr_type_properties;
	info->psy_desc.num_properties = ARRAY_SIZE(chr_type_properties);
	info->psy_desc.get_property = psy_chr_type_get_property;
	info->psy_desc.set_property = psy_chr_type_set_property;
	info->psy_desc.property_is_writeable =
			psy_charger_type_property_is_writeable;
	info->psy_desc.usb_types = mt6357_charger_usb_types,
	info->psy_desc.num_usb_types = ARRAY_SIZE(mt6357_charger_usb_types),
	info->psy_cfg.drv_data = info;

	info->psy_cfg.of_node = np;
	info->psy_cfg.supplied_to = mt6357_charger_supplied_to;
	info->psy_cfg.num_supplicants = ARRAY_SIZE(mt6357_charger_supplied_to);

	info->ac_desc.name = "ac";
	info->ac_desc.type = POWER_SUPPLY_TYPE_MAINS;
	info->ac_desc.properties = mt_ac_properties;
	info->ac_desc.num_properties = ARRAY_SIZE(mt_ac_properties);
	info->ac_desc.get_property = mt_ac_get_property;
	info->ac_cfg.drv_data = info;

	info->usb_desc.name = "usb";
	info->usb_desc.type = POWER_SUPPLY_TYPE_USB;
	info->usb_desc.properties = mt_usb_properties;
	info->usb_desc.num_properties = ARRAY_SIZE(mt_usb_properties);
	info->usb_desc.get_property = mt_usb_get_property;
	info->usb_cfg.drv_data = info;

	info->psy = power_supply_register(&pdev->dev, &info->psy_desc,
			&info->psy_cfg);

	if (IS_ERR(info->psy)) {
		pr_notice("%s Failed to register power supply: %ld\n",
			__func__, PTR_ERR(info->psy));
		return PTR_ERR(info->psy);
	}
	pr_notice("%s register psy success\n", __func__);

	info->chan_vbus = devm_iio_channel_get(
		&pdev->dev, "pmic_vbus");
	if (IS_ERR(info->chan_vbus)) {
		pr_notice("chan_vbus auxadc get fail, ret=%d\n",
			PTR_ERR(info->chan_vbus));
	}

	if (of_property_read_u32(np, "bc12_active", &info->bc12_active) < 0)
		pr_notice("%s: no bc12_active\n", __func__);

	pr_notice("%s: bc12_active:%d\n", __func__, info->bc12_active);

	if (info->bc12_active) {
		info->ac_psy = power_supply_register(&pdev->dev,
				&info->ac_desc, &info->ac_cfg);

		if (IS_ERR(info->ac_psy)) {
			pr_notice("%s Failed to register power supply: %ld\n",
				__func__, PTR_ERR(info->ac_psy));
			return PTR_ERR(info->ac_psy);
		}

		info->usb_psy = power_supply_register(&pdev->dev,
				&info->usb_desc, &info->usb_cfg);

		if (IS_ERR(info->usb_psy)) {
			pr_notice("%s Failed to register power supply: %ld\n",
				__func__, PTR_ERR(info->usb_psy));
			return PTR_ERR(info->usb_psy);
		}

		//+Bug492299,lili5.wt,ADD,20191021,battery Current event and slate mode
		#ifdef CONFIG_CHARGER_BQ2415X
		batt_psy = power_supply_get_by_name("battery");
		if (!batt_psy) {
		pr_notice("%s: get power supply failed\n", __func__);
		return -EINVAL;
		}
		#endif
		//-Bug492299,lili5.wt,ADD,20191021,battery Current event and slate mode

		INIT_WORK(&info->chr_work, do_charger_detection_work);
//+Bug 623221, yaocankun.wt,add, 20210225, add charge current event
		#ifdef CONFIG_WT_PROJECT_S96717RA1
		INIT_DELAYED_WORK(&batt_work, battery_current_monitoring_work);
		#endif
//-Bug 623221, yaocankun.wt,add, 20210225, add charge current event
//+bug 621775,yaocankun.wt,add,20210115,add for n21 charger bring up
#ifndef CONFIG_MT6370_PMU_CHARGER
		schedule_work(&info->chr_work);

		//+Bug 615302,xuejizhou.wt,ADD,20210113,battery Current event and slate mode
		#ifdef CONFIG_CHARGER_BQ2415X
		INIT_DELAYED_WORK(&batt_work, battery_current_monitoring_work);
		#endif
		//-Bug 615302,xuejizhou.wt,ADD,20210113,battery Current event and slate mode

		ret = devm_request_threaded_irq(&pdev->dev,
			platform_get_irq_byname(pdev, "chrdet"), NULL,
			chrdet_int_handler, IRQF_TRIGGER_HIGH, "chrdet", info);
		if (ret < 0)
			pr_notice("%s request chrdet irq fail\n", __func__);
#else
		ret = 0;
#endif
//-bug 621775,yaocankun.wt,add,20210115,add for n21 charger bring up
	}

	info->first_connect = true;

	pr_notice("%s: done\n", __func__);

	return 0;
}

static const struct of_device_id mt6357_charger_type_of_match[] = {
	{.compatible = "mediatek,mt6357-charger-type",},
	{},
};

static int mt6357_charger_type_remove(struct platform_device *pdev)
{
	struct mtk_charger_type *info = platform_get_drvdata(pdev);

	if (info)
		devm_kfree(&pdev->dev, info);
	return 0;
}

MODULE_DEVICE_TABLE(of, mt6357_charger_type_of_match);

static struct platform_driver mt6357_charger_type_driver = {
	.probe = mt6357_charger_type_probe,
	.remove = mt6357_charger_type_remove,
	//.shutdown = mt6357_charger_type_shutdown,
	.driver = {
		.name = "mt6357-charger-type-detection",
		.of_match_table = mt6357_charger_type_of_match,
		},
};

static int __init mt6357_charger_type_init(void)
{
	return platform_driver_register(&mt6357_charger_type_driver);
}
module_init(mt6357_charger_type_init);

static void __exit mt6357_charger_type_exit(void)
{
	platform_driver_unregister(&mt6357_charger_type_driver);
}
module_exit(mt6357_charger_type_exit);

MODULE_AUTHOR("wy.chuang <wy.chuang@mediatek.com>");
MODULE_DESCRIPTION("MTK Charger Type Detection Driver");
MODULE_LICENSE("GPL");

