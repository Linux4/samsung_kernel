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
#include "charger_class.h"
#include <tcpm.h>
#include <tcpci.h>

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

#if defined (CONFIG_CHARGER_BQ2560X) ||  defined (CONFIG_CHARGER_BQ2589X)
bool otg_enabled;
extern int mtk_chg_status;
extern bool is_bc12_done;
#define CHR_WORK_DELAY 1000
#endif

//+Bug 682591,wangmingyuan.wt,ADD,20210816,battery Current event and slate mode
#if defined (CONFIG_CHARGER_BQ2560X) ||  defined (CONFIG_CHARGER_BQ2589X)
static struct delayed_work batt_work;
static struct power_supply *batt_psy;
static int count = 0;
extern int mtk_chg_status;
#endif
//-Bug 682591,wangmingyuan.wt,ADD,20210816,battery Current event and slate mode
bool is_nonstd_chg = false;

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
	struct delayed_work chr_work;
#ifdef CONFIG_CHARGER_BQ2560X
	struct delayed_work typec_det_work;
#endif
	enum power_supply_usb_type type;

	int first_connect;
	int bc12_active;
	int polarity_state;//Bug682956,yangyuhang.wt ,20210813, add cc polarity node

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
	POWER_SUPPLY_PROP_TYPEC_POLARITY,//Bug682956,yangyuhang.wt ,20210813, add cc polarity node
	POWER_SUPPLY_PROP_REAL_TYPE,
	POWER_SUPPLY_PROP_AFC_FLAG,//Bug685833,yangyuhang.wt ,20210826, add Charging afc flag node
};

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

#if defined CONFIG_CHARGER_BQ2589X || defined CONFIG_CHARGER_BQ2560X
extern int g_bootmode;
#endif

static void hw_bc11_init(struct mtk_charger_type *info)
{
#if IS_ENABLED(CONFIG_USB_MTK_HDRC)
	int timeout = 200;
#endif
	msleep(200);
//+bug 682591,xuejizhou.wt,add,20210426,charge type detecion takes more than 20 sec in lpm mode
#if defined CONFIG_CHARGER_BQ2589X || defined CONFIG_CHARGER_BQ2560X
	if (g_bootmode == KERNEL_POWER_OFF_CHARGING_BOOT ||
	    g_bootmode == LOW_POWER_OFF_CHARGING_BOOT) {
		pr_info("Skip usb_ready check in KPOC\n");
		goto skip;
	}
#endif
//-bug 682591,xuejizhou.wt,add,20210426,charge type detecion takes more than 20 sec in lpm mode
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
#if defined CONFIG_CHARGER_BQ2589X || defined CONFIG_CHARGER_BQ2560X
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
		pr_info("charger type: %d, CHARGER_UNKNOWN\n", type);
		break;
	case POWER_SUPPLY_TYPE_USB:
		pr_info("charger type: %d, Standard USB Host\n", type);
		break;
	case POWER_SUPPLY_TYPE_USB_CDP:
		pr_info("charger type: %d, Charging USB Host\n", type);
		break;
#ifdef FIXME
	case POWER_SUPPLY_TYPE_USB_FLOAT:
		pr_info("charger type: %d, Non-standard Charger\n", type);
		break;
#endif
	case POWER_SUPPLY_TYPE_USB_DCP:
		pr_info("charger type: %d, Standard Charger\n", type);
		break;
	default:
		pr_info("charger type: %d, Not Defined!!!\n", type);
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
//+bug 682591,xuejizhou.wt,add,20210426,charge type detecion takes more than 20 sec in lpm mode
#if defined CONFIG_CHARGER_BQ2589X || defined CONFIG_CHARGER_BQ2560X
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
//-bug 682591,xuejizhou.wt,add,20210426,charge type detecion takes more than 20 sec in lpm mode
		hw_bc11_done(info);
	else
		pr_info("charger type: skip bc11 release for BC12 DCP SPEC\n");
#ifdef CONFIG_CHARGER_BQ2560X
	is_bc12_done = true;
#endif
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

static void typec_detect_work(struct work_struct *data)
{
	struct tcpc_device *tcpc_dev;
	unsigned int chrdet = 0;
	struct mtk_charger_type *info = (struct mtk_charger_type *)container_of(
				     data, struct mtk_charger_type, typec_det_work.work);

	tcpc_dev = tcpc_dev_get_by_name("type_c_port0");
	if (!tcpc_dev) {
		pr_err("%s get tcpc device type_c_port0 fail\n", __func__);
		return;
	}

	chrdet = bc11_get_register_value(info->regmap,
		PMIC_RGS_CHRDET_ADDR,
		PMIC_RGS_CHRDET_MASK,
		PMIC_RGS_CHRDET_SHIFT);

	pr_info("%s: chrdet:%d\n", __func__, chrdet);

	if (!chrdet) {
		if (tcpc_dev->typec_role != TYPEC_ROLE_DRP)
			tcpci_set_cc(tcpc_dev, TYPEC_CC_DRP);
	}
}

void do_charger_detect(struct mtk_charger_type *info, bool en)
{
	union power_supply_propval prop, prop2, prop3;
	int ret = 0;
#if defined (CONFIG_CHARGER_BQ2560X) ||  defined (CONFIG_CHARGER_BQ2589X)
	struct charger_device *primary_charger;
	bool is_hz_mode = false;
	unsigned int chrdet = 0;//Bug 704797,yangyuhang.wt,ADD,20211123,If no charger is connected, "Charger is connected" is displayed

	primary_charger = get_charger_by_name("primary_chg");
	if (primary_charger)
		pr_err("Found primary charger\n");
	else {
		pr_err("*** Error : can't find primary charger ***\n");
		return;
	}
#endif

#ifndef CONFIG_TCPC_CLASS
	if (!mt_usb_is_device()) {
		pr_info("charger type: UNKNOWN, Now is usb host mode. Skip detection\n");
		return;
	}
#endif

#if defined (CONFIG_CHARGER_BQ2560X) ||  defined (CONFIG_CHARGER_BQ2589X)
	if (otg_enabled == true) {
		pr_err("%s otg enabled, return\n", __func__);
		return;
	}

	if (!en) {
		charger_dev_is_hz_mode(primary_charger, &is_hz_mode);
		if (is_hz_mode)
			charger_dev_hz_mode(primary_charger, 0);
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
		#if defined (CONFIG_CHARGER_BQ2560X) ||  defined (CONFIG_CHARGER_BQ2589X)
		//+Bug 704797,yangyuhang.wt,ADD,20211123,If no charger is connected, "Charger is connected" is displayed
		chrdet = bc11_get_register_value(info->regmap,
					PMIC_RGS_CHRDET_ADDR,
					PMIC_RGS_CHRDET_MASK,
					PMIC_RGS_CHRDET_SHIFT);
		pr_err("%s: chrdet:%d\n", __func__, chrdet);
		if (!chrdet) {
			do_charger_detect(info, chrdet);
			return;
		}
		//-Bug 704797,yangyuhang.wt,ADD,20211123,If no charger is connected, "Charger is connected" is displayed
		schedule_delayed_work(&batt_work, msecs_to_jiffies(0));//Bug 682591,wangmingyuan.wt,ADD,20210816,battery Current event and slate mode
		#endif
	} else {
		cancel_delayed_work(&info->chr_work);//Bug 704797,yangyuhang.wt,ADD,20211123,If no charger is connected, "Charger is connected" is displayed
		prop2.intval = POWER_SUPPLY_TYPE_UNKNOWN;
		prop3.intval = POWER_SUPPLY_USB_TYPE_UNKNOWN;
		info->psy_desc.type = POWER_SUPPLY_TYPE_UNKNOWN;
		info->type = POWER_SUPPLY_USB_TYPE_UNKNOWN;
		#if defined (CONFIG_CHARGER_BQ2560X) ||  defined (CONFIG_CHARGER_BQ2589X)
		mtk_chg_status = 0;
		count = 0;
		is_bc12_done = false;
		mtk_chg_status = 0;
		pr_info("%s work\n", __func__);
		schedule_delayed_work(&info->typec_det_work, msecs_to_jiffies(5000));
		#endif
		is_nonstd_chg = false;
	}

	pr_notice("%s type:%d usb_type:%d\n", __func__, prop2.intval, prop3.intval);

	power_supply_changed(info->psy);
}

static void do_charger_detection_work(struct work_struct *data)
{
	struct mtk_charger_type *info = (struct mtk_charger_type *)container_of(
				     data, struct mtk_charger_type, chr_work.work);
	unsigned int chrdet = 0;

	if (g_bootmode == 0xab) {
		pr_err("%s g_bootmode = %d\n", __func__, g_bootmode);
		schedule_delayed_work(&info->chr_work, msecs_to_jiffies(CHR_WORK_DELAY));
		return;
	}

	chrdet = bc11_get_register_value(info->regmap,
		PMIC_RGS_CHRDET_ADDR,
		PMIC_RGS_CHRDET_MASK,
		PMIC_RGS_CHRDET_SHIFT);

	pr_notice("%s: chrdet:%d\n", __func__, chrdet);
	if (chrdet)
		do_charger_detect(info, chrdet);
}


irqreturn_t chrdet_int_handler(int irq, void *data)
{
	struct mtk_charger_type *info = data;
	unsigned int chrdet = 0;

	chrdet = bc11_get_register_value(info->regmap,
		PMIC_RGS_CHRDET_ADDR,
		PMIC_RGS_CHRDET_MASK,
		PMIC_RGS_CHRDET_SHIFT);

	pr_notice("%s: chrdet:%d\n", __func__, chrdet);
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
		if (val->intval == true) {
			info->type = get_charger_type(info);
#if defined (CONFIG_CHARGER_BQ2560X) || defined (CONFIG_CHARGER_BQ2589X)
			if (info->type == POWER_SUPPLY_USB_TYPE_DCP &&
					info->psy_desc.type == POWER_SUPPLY_TYPE_USB) {
				pr_err("%s non_std retry bc1.2\n", __func__);
				info->type = get_charger_type(info);
				if (info->type == POWER_SUPPLY_USB_TYPE_DCP &&
						info->psy_desc.type == POWER_SUPPLY_TYPE_USB) {
					is_nonstd_chg = true;
					info->type = POWER_SUPPLY_USB_TYPE_SDP;
					info->psy_desc.type = POWER_SUPPLY_TYPE_USB;
				}
			}
#endif
		} else {
			info->type = POWER_SUPPLY_USB_TYPE_UNKNOWN;
			info->psy_desc.type = POWER_SUPPLY_TYPE_UNKNOWN;
		}
		break;
	case POWER_SUPPLY_PROP_USB_TYPE:
		info->type = val->intval;
		switch (val->intval) {
			case POWER_SUPPLY_USB_TYPE_UNKNOWN:
				info->psy_desc.type = POWER_SUPPLY_TYPE_UNKNOWN;
				break;
        	case POWER_SUPPLY_USB_TYPE_SDP:
				info->psy_desc.type = POWER_SUPPLY_TYPE_USB;
				break;
        	case POWER_SUPPLY_USB_TYPE_DCP:
				info->psy_desc.type = POWER_SUPPLY_TYPE_USB_DCP;
				break;
        	case POWER_SUPPLY_USB_TYPE_CDP:
				info->psy_desc.type = POWER_SUPPLY_TYPE_USB_CDP;
				break;
			default:
				info->psy_desc.type = POWER_SUPPLY_TYPE_USB_FLOAT;
				break;
		}
		pr_err("force charger type to %d\n", val->intval);
		break;
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
//+Bug682956,yangyuhang.wt ,20210813, add cc polarity node
int tcpc_set_cc_polarity_state(int state)
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
//-Bug682956,yangyuhang.wt ,20210813, add cc polarity node
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
//+Bug682956,yangyuhang.wt ,20210813, add cc polarity node
	case POWER_SUPPLY_PROP_TYPEC_POLARITY:
		val->intval = info->polarity_state;
		break;
//-Bug682956,yangyuhang.wt ,20210813, add cc polarity node
//+Bug686637,yangyuhang.wt ,20210819, add real type node
	case POWER_SUPPLY_PROP_REAL_TYPE:
		if (info->type == POWER_SUPPLY_USB_TYPE_SDP) {
			val->strval = "USB";
		} else if (info->type == POWER_SUPPLY_USB_TYPE_CDP) {
			val->strval = "USB_CDP";
		} else if (info->type == POWER_SUPPLY_USB_TYPE_DCP) {
				val->strval = "USB_DCP";
		} else {
			val->strval = "UNKNOWN";
		}
		break;
//-Bug686637,yangyuhang.wt ,20210819, add real type node
//+Bug685833,yangyuhang.wt ,20210826, add Charging afc flag node
	case POWER_SUPPLY_PROP_AFC_FLAG:
		val->intval = 0;
		break;
//-Bug685833,yangyuhang.wt ,20210826, add Charging afc flag node
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

//+Bug 682591,wangmingyuan.wt,ADD,20210816,battery Current event and slate mode
#define low_current_level 100
#if defined (CONFIG_CHARGER_BQ2560X) ||  defined (CONFIG_CHARGER_BQ2589X)
static void battery_current_monitoring_work(struct work_struct *data)
{
	struct mtk_charger_type *info;
	union power_supply_propval val;
	
	int ret= 0;

	if(info->type == POWER_SUPPLY_USB_TYPE_SDP){
		if(count > 2) {
			count = 0;
			val.intval = 64;
			ret = power_supply_set_property(batt_psy, POWER_SUPPLY_PROP_BATT_CURRENT_EVENT, &val);
			if (ret < 0)
				pr_debug("%s: psy set property failed, ret = %d\n", __func__, ret);
		} else {
			ret = power_supply_get_property(batt_psy, POWER_SUPPLY_PROP_INPUT_CURRENT_LIMIT, &val);
			if (ret < 0)
				pr_debug("%s: psy get property failed, ret = %d\n", __func__, ret);

			if (low_current_level == val.intval) {
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
//-Bug 682591,wangmingyuan.wt,ADD,20210816,battery Current event and slate mode
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

		//+Bug682591,wangmingyuan.wt,ADD,20210816,battery Current event and slate mode
		#if defined (CONFIG_CHARGER_BQ2560X) ||  defined (CONFIG_CHARGER_BQ2589X)
		batt_psy = power_supply_get_by_name("battery");
		if (!batt_psy) {
		pr_notice("%s: get power supply failed\n", __func__);
		return -EINVAL;
		}
		#endif
		//-Bug682591,wangmingyuan.wt,ADD,20210816,battery Current event and slate mode

		INIT_DELAYED_WORK(&info->chr_work, do_charger_detection_work);
		schedule_delayed_work(&info->chr_work, msecs_to_jiffies(CHR_WORK_DELAY));

		//+Bug 682591,wangmingyuan.wt,ADD,20210816,battery Current event and slate mode
		#if defined (CONFIG_CHARGER_BQ2560X) ||  defined (CONFIG_CHARGER_BQ2589X)
		INIT_DELAYED_WORK(&batt_work, battery_current_monitoring_work);
		INIT_DELAYED_WORK(&info->typec_det_work, typec_detect_work);
		#endif
		//-Bug 682591,wangmingyuan.wt,ADD,20210816,battery Current event and slate mode

		ret = devm_request_threaded_irq(&pdev->dev,
			platform_get_irq_byname(pdev, "chrdet"), NULL,
			chrdet_int_handler, IRQF_TRIGGER_HIGH, "chrdet", info);
		if (ret < 0)
			pr_notice("%s request chrdet irq fail\n", __func__);
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

