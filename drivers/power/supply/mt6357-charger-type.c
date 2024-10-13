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
/* HS03s code for SR-AL5625-01-55 by wenyaqi at 20210419 start */
#include "charger_class.h"
/* HS03s code for SR-AL5625-01-55 by wenyaqi at 20210419 end */
/*HS03s for SR-AL5625-01-261 by wenyaqi at 20210428 start*/
#ifndef HQ_FACTORY_BUILD	//ss version
#include <mt-plat/mtk_boot_common.h>
#endif
/*HS03s for SR-AL5625-01-261 by wenyaqi at 20210428 end*/
#include <linux/reboot.h>

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

/*HS03s for SR-AL5625-01-513 by wenyaqi at 20210526 start*/
#ifdef CONFIG_HQ_PROJECT_HS03S
enum CHG_STATUS {
	CHG_STATUS_NOT_CHARGING = 0,
	CHG_STATUS_PRE_CHARGE,
	CHG_STATUS_FAST_CHARGING,
	CHG_STATUS_DONE,
};
#endif
#ifdef CONFIG_HQ_PROJECT_HS04
enum CHG_STATUS {
	CHG_STATUS_NOT_CHARGING = 0,
	CHG_STATUS_PRE_CHARGE,
	CHG_STATUS_FAST_CHARGING,
	CHG_STATUS_DONE,
};
#endif
/*HS03s for SR-AL5625-01-513 by wenyaqi at 20210526 end*/

struct mtk_charger_type {
	struct mt6397_chip *chip;
	struct regmap *regmap;
	struct platform_device *pdev;
	struct mutex ops_lock;
	/* HS03s for SR-AL5625-01-55 by wenyaqi at 20210419 start */
	struct charger_device *chg_dev;
	/* HS03s for SR-AL5625-01-55 by wenyaqi at 20210419 end */

	struct power_supply_desc psy_desc;
	struct power_supply_config psy_cfg;
	struct power_supply *psy;
	struct power_supply_desc ac_desc;
	struct power_supply_config ac_cfg;
	struct power_supply *ac_psy;
	struct power_supply_desc usb_desc;
	struct power_supply_config usb_cfg;
	struct power_supply *usb_psy;
	/*HS03s for SR-AL5625-01-267 by wenyaqi at 20210427 start*/
	#ifndef HQ_FACTORY_BUILD	//ss version
	struct power_supply_desc otg_desc;
	struct power_supply_config otg_cfg;
	struct power_supply *otg_psy;
	#endif
	/*HS03s for SR-AL5625-01-267 by wenyaqi at 20210427 end*/

	struct iio_channel *chan_vbus;
	struct work_struct chr_work;

	enum power_supply_usb_type type;

	int first_connect;
	int bc12_active;
	/*HS03s for bugid by wangzikang at 20210731 start*/
	//spinlock_t slock;
	/*HS03s for bugid by wangzikang at 20210731 end*/
	#ifndef HQ_FACTORY_BUILD	//ss version
	struct delayed_work		dwork;
	int power_off_flag;
	#endif
	u32 bootmode;
	u32 boottype;
};

struct tag_bootmode {
	u32 size;
	u32 tag;
	u32 bootmode;
	u32 boottype;
};

static enum power_supply_property chr_type_properties[] = {
	POWER_SUPPLY_PROP_ONLINE,
	POWER_SUPPLY_PROP_TYPE,
	POWER_SUPPLY_PROP_USB_TYPE,
	POWER_SUPPLY_PROP_VOLTAGE_NOW,
/*HS03s for SR-AL5625-01-278 by wenyaqi at 20210427 start*/
	#ifndef HQ_FACTORY_BUILD	//ss version
	POWER_SUPPLY_PROP_CHARGE_TYPE,
	#endif
/*HS03s for SR-AL5625-01-278 by wenyaqi at 20210427 end*/
/*HS03s for SR-AL5625-01-513 by wenyaqi at 20210526 start*/
#ifdef CONFIG_HQ_PROJECT_HS03S
	POWER_SUPPLY_PROP_STATUS,
#endif
#ifdef CONFIG_HQ_PROJECT_HS04
	POWER_SUPPLY_PROP_STATUS,
#endif
/*HS03s for SR-AL5625-01-513 by wenyaqi at 20210526 end*/
};

static enum power_supply_property mt_ac_properties[] = {
	POWER_SUPPLY_PROP_ONLINE,
};

static enum power_supply_property mt_usb_properties[] = {
	POWER_SUPPLY_PROP_ONLINE,
	POWER_SUPPLY_PROP_CURRENT_MAX,
	POWER_SUPPLY_PROP_VOLTAGE_MAX,
	/* HS03s for SR-AL5625-01-55 by wenyaqi at 20210419 start */
	POWER_SUPPLY_PROP_VOLTAGE_NOW,
	POWER_SUPPLY_PROP_INPUT_CURRENT_NOW,
	/* HS03s for SR-AL5625-01-55 by wenyaqi at 20210419 end */
	/* HS03s for SR-AL5625-01-52 by wenyaqi at 20210419 start */
	POWER_SUPPLY_PROP_TYPEC_CC_ORIENTATION,
	/* HS03s for SR-AL5625-01-52 by wenyaqi at 20210419 end */
};

/*HS03s for SR-AL5625-01-267 by wenyaqi at 20210427 start*/
#ifndef HQ_FACTORY_BUILD	//ss version
static enum power_supply_property mt_otg_properties[] = {
	POWER_SUPPLY_PROP_ONLINE,
};
#endif
/*HS03s for SR-AL5625-01-267 by wenyaqi at 20210427 end*/

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

/*HS03s for SR-AL5625-01-261 by wenyaqi at 20210428 start*/
#ifndef HQ_FACTORY_BUILD	//ss version
extern int hq_get_boot_mode(void);
#endif
static void hw_bc11_init(struct mtk_charger_type *info)
{
#if IS_ENABLED(CONFIG_USB_MTK_HDRC)
	int timeout = 200;
#endif
	/*HS03s for bugid by wangzikang at 20210731 start*/
/* hs04 code for AL6398ADEU-199 by shixuanxuan at 20221206 start */
#ifdef CONFIG_HQ_PROJECT_HS04
	msleep(300);
#else
	msleep(200);
#endif
/* hs04 code for AL6398ADEU-199 by shixuanxuan at 20221206 end*/
	/*HS03s for bugid by wangzikang at 20210731 end*/

	#ifndef HQ_FACTORY_BUILD	//ss version
	if (hq_get_boot_mode() == KERNEL_POWER_OFF_CHARGING_BOOT ||
		hq_get_boot_mode() == LOW_POWER_OFF_CHARGING_BOOT) {
		pr_info("Skip usb_ready check in KPOC\n");
		goto skip;
	}
	#endif

	if (info->first_connect == true) {
#if IS_ENABLED(CONFIG_USB_MTK_HDRC)
		/* add make sure USB Ready */
		if (is_usb_rdy() == false) {
			pr_info("CDP, block\n");
			while (is_usb_rdy() == false && timeout > 0) {
				/*HS03s for bugid by wangzikang at 20210731 start*/
				msleep(100);
				/*HS03s for bugid by wangzikang at 20210731 end*/
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

#ifndef HQ_FACTORY_BUILD	//ss version
skip:
#endif
/*HS03s for SR-AL5625-01-261 by wenyaqi at 20210428 end*/
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
	/*HS03s for bugid by wangzikang at 20210731 start*/
	msleep(50);
	/*HS03s for bugid by wangzikang at 20210731 end*/

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
	/*HS03s for bugid by wangzikang at 20210731 start*/
	msleep(80);
	/*HS03s for bugid by wangzikang at 20210731 end*/
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
	/*HS03s for bugid by wangzikang at 20210731 start*/
	msleep(80);
	/*HS03s for bugid by wangzikang at 20210731 end*/
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
	/*HS03s for bugid by wangzikang at 20210731 start*/
	msleep(80);
	/*HS03s for bugid by wangzikang at 20210731 end*/
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

#ifdef CONFIG_HQ_PROJECT_HS03S
//wangtao for o8
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

/*HS03s for SR-AL5625-01-261 by wenyaqi at 20210428 start*/
	#ifndef HQ_FACTORY_BUILD	//ss version
	if (type == POWER_SUPPLY_USB_TYPE_CDP) {
		if (hq_get_boot_mode() == KERNEL_POWER_OFF_CHARGING_BOOT ||
		    hq_get_boot_mode() == LOW_POWER_OFF_CHARGING_BOOT) {
			pr_info("Pull up D+ to 0.6V for CDP in KPOC\n");
			/*HS03s for bugid by wangzikang at 20210731 start*/
			msleep(100);
			/*HS03s for bugid by wangzikang at 20210731 end*/
			/* RG_bc11_VSRC_EN[1:0] = 10 */
			bc11_set_register_value(info->regmap,
				PMIC_RG_BC11_VSRC_EN_ADDR,
				PMIC_RG_BC11_VSRC_EN_MASK,
				PMIC_RG_BC11_VSRC_EN_SHIFT,
				0x2);
			/* RG_bc11_IPU_EN[1.0] = 10 */
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
/*HS03s for SR-AL5625-01-261 by wenyaqi at 20210428 end*/
		hw_bc11_done(info);
	else
		pr_info("charger type: skip bc11 release for BC12 DCP SPEC\n");

	dump_charger_name(info->psy_desc.type);

	return type;
}
#endif
#ifdef CONFIG_HQ_PROJECT_HS04
//wangtao for o8
static int get_charger_type(struct mtk_charger_type *info)
{
	enum power_supply_usb_type type;
	/* HS04_T for DEAL6398A-1879 by shixuanxuan at 20221012 start */
	int retry_num = 5;
	int curr_num = 6;
	bool bc11_flag = 0;

	hw_bc11_init(info);
	if (hw_bc11_DCD(info)) {
		do {
			msleep(100);
			pr_err("bc1.1 detect no type, it will retry 5 times: retry = %d\n",
                               (curr_num - retry_num));
			bc11_flag = hw_bc11_DCD(info);
		} while ( bc11_flag && retry_num--);
	}

	if (bc11_flag) {
		pr_err("bc1.1 detect no type!\n");
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
/* HS04_T for DEAL6398A-1879 by shixuanxuan at 20221012 end*/

/*HS03s for SR-AL5625-01-261 by wenyaqi at 20210428 start*/
	#ifndef HQ_FACTORY_BUILD	//ss version
	if (type == POWER_SUPPLY_USB_TYPE_CDP) {
		if (hq_get_boot_mode() == KERNEL_POWER_OFF_CHARGING_BOOT ||
		    hq_get_boot_mode() == LOW_POWER_OFF_CHARGING_BOOT) {
			pr_info("Pull up D+ to 0.6V for CDP in KPOC\n");
			/*HS03s for bugid by wangzikang at 20210731 start*/
			msleep(100);
			/*HS03s for bugid by wangzikang at 20210731 end*/
			/* RG_bc11_VSRC_EN[1:0] = 10 */
			bc11_set_register_value(info->regmap,
				PMIC_RG_BC11_VSRC_EN_ADDR,
				PMIC_RG_BC11_VSRC_EN_MASK,
				PMIC_RG_BC11_VSRC_EN_SHIFT,
				0x2);
			/* RG_bc11_IPU_EN[1.0] = 10 */
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
/*HS03s for SR-AL5625-01-261 by wenyaqi at 20210428 end*/
		hw_bc11_done(info);
	else
		pr_info("charger type: skip bc11 release for BC12 DCP SPEC\n");

	dump_charger_name(info->psy_desc.type);

	return type;
}
#endif
//for o8
#ifdef CONFIG_HQ_PROJECT_OT8
extern bool pd_hub_flag;
/* Tab A7 lite_T for P221008-02933 by duanweiping at 20221029 start */
bool bc12_done = false;
EXPORT_SYMBOL(bc12_done);
/* Tab A7 lite_T for P221008-02933 by duanweiping at 20221029 end */
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
	/* Tab A7 lite_T for P221008-02933 by duanweiping at 20221029 start */
	bc12_done = true;
	pr_err("get_charger_type BC12 done\n");
	/* Tab A7 lite_T for P221008-02933 by duanweiping at 20221029 end */
	/*TabA7 Lite code for P201215-02072 samsung lpm animation in POWER_OFF_CHARGING by wenyaqi at 20201224 start*/
	#ifndef HQ_FACTORY_BUILD	//ss version
	if (type == POWER_SUPPLY_USB_TYPE_CDP) {
		if (hq_get_boot_mode() == KERNEL_POWER_OFF_CHARGING_BOOT ||
		    hq_get_boot_mode() == LOW_POWER_OFF_CHARGING_BOOT) {
			pr_info("Pull up D+ to 0.6V for CDP in KPOC\n");
			msleep(100);
			/* RG_bc11_VSRC_EN[1:0] = 10 */
			bc11_set_register_value(info->regmap,
				PMIC_RG_BC11_VSRC_EN_ADDR,
				PMIC_RG_BC11_VSRC_EN_MASK,
				PMIC_RG_BC11_VSRC_EN_SHIFT,
				0x2);
			/* RG_bc11_IPU_EN[1.0] = 10 */
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
	/*TabA7 Lite code for P201215-02072 samsung lpm animation in POWER_OFF_CHARGING by wenyaqi at 20201224 end*/
		hw_bc11_done(info);
	/*TabA7 Lite code for HQ00001 by wenyaqi at 20210506 start*/
	else if (info->psy_desc.type == POWER_SUPPLY_TYPE_USB &&
		type == POWER_SUPPLY_USB_TYPE_DCP) {
			pr_info("still run bc11 release for USB hub\n");
			hw_bc11_done(info);
	}
	/*TabA7 Lite code for HQ00001 by wenyaqi at 20210506 end*/
	else
		pr_info("charger type: skip bc11 release for BC12 DCP SPEC\n");

	dump_charger_name(info->psy_desc.type);

	return type;
}

#endif //for 08

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


#ifdef CONFIG_HQ_PROJECT_HS03S
void do_charger_detect(struct mtk_charger_type *info, bool en)
{
	union power_supply_propval prop_online, prop_type, prop_usb_type;
	int ret = 0;

	/*HS03s for bugid by wangzikang at 20210731 start*/
	//unsigned long flags;
	//spin_lock_irqsave(&info->slock, flags);
	pr_info("%s enter \n",__func__);
	/*HS03s for bugid by wangzikang at 20210731 end*/
/* HS03s for SR-AL5625-01-515 by wangzikang at 21210610 start*/
//#ifndef CONFIG_TCPC_CLASS
	if (!mt_usb_is_device()) {
		pr_info("charger type: UNKNOWN, Now is usb host mode. Skip detection\n");
		return;
	}
//#endif
/* HS03s for SR-AL5625-01-515 by wangzikang at 21210610 end*/

	prop_online.intval = en;
	if (en) {
		ret = power_supply_set_property(info->psy,
				POWER_SUPPLY_PROP_ONLINE, &prop_online);
		ret = power_supply_get_property(info->psy,
				POWER_SUPPLY_PROP_TYPE, &prop_type);
		ret = power_supply_get_property(info->psy,
				POWER_SUPPLY_PROP_USB_TYPE, &prop_usb_type);
		pr_notice("type:%d usb_type:%d\n", prop_type.intval, prop_usb_type.intval);
	} else {
		info->psy_desc.type = POWER_SUPPLY_TYPE_UNKNOWN;
		info->type = POWER_SUPPLY_USB_TYPE_UNKNOWN;
		pr_notice("%s type:0 usb_type:0\n", __func__);
	}

	power_supply_changed(info->psy);
	/*HS03s for bugid by wangzikang at 20210731 start*/
	//spin_unlock_irqrestore(&info->slock, flags);
	/*HS03s for bugid by wangzikang at 20210731 end*/
}
#endif
#ifdef CONFIG_HQ_PROJECT_HS04
void do_charger_detect(struct mtk_charger_type *info, bool en)
{
	union power_supply_propval prop_online, prop_type, prop_usb_type;
	int ret = 0;

	/*HS03s for bugid by wangzikang at 20210731 start*/
	//unsigned long flags;
	//spin_lock_irqsave(&info->slock, flags);
	pr_info("%s enter \n",__func__);
	/*HS03s for bugid by wangzikang at 20210731 end*/
/* HS03s for SR-AL5625-01-515 by wangzikang at 21210610 start*/
//#ifndef CONFIG_TCPC_CLASS
	if (!mt_usb_is_device()) {
		pr_info("charger type: UNKNOWN, Now is usb host mode. Skip detection\n");
		return;
	}
//#endif
/* HS03s for SR-AL5625-01-515 by wangzikang at 21210610 end*/

	prop_online.intval = en;
	if (en) {
		ret = power_supply_set_property(info->psy,
				POWER_SUPPLY_PROP_ONLINE, &prop_online);
		ret = power_supply_get_property(info->psy,
				POWER_SUPPLY_PROP_TYPE, &prop_type);
		ret = power_supply_get_property(info->psy,
				POWER_SUPPLY_PROP_USB_TYPE, &prop_usb_type);
		pr_notice("type:%d usb_type:%d\n", prop_type.intval, prop_usb_type.intval);
	} else {
		info->psy_desc.type = POWER_SUPPLY_TYPE_UNKNOWN;
		info->type = POWER_SUPPLY_USB_TYPE_UNKNOWN;
		pr_notice("%s type:0 usb_type:0\n", __func__);
	}

	power_supply_changed(info->psy);
	/*HS03s for bugid by wangzikang at 20210731 start*/
	//spin_unlock_irqrestore(&info->slock, flags);
	/*HS03s for bugid by wangzikang at 20210731 end*/
}
#endif
#ifdef CONFIG_HQ_PROJECT_OT8
/* modify code for O8 */
/*TabA7 Lite code for SR-AX3565-01-97 enable otg by wenyaqi at 20201124 start*/
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
	} else {
		prop2.intval = POWER_SUPPLY_TYPE_UNKNOWN;
		prop3.intval = POWER_SUPPLY_USB_TYPE_UNKNOWN;
		info->psy_desc.type = POWER_SUPPLY_TYPE_UNKNOWN;
		info->type = POWER_SUPPLY_USB_TYPE_UNKNOWN;
	}

	pr_notice("%s type:%d usb_type:%d\n", __func__, prop2.intval, prop3.intval);

	power_supply_changed(info->psy);
}
#endif//for o8

#ifndef HQ_FACTORY_BUILD	//ss version
#define POWER_OFF_CHECK_TIME_MS 1000
#define FLAG_POWER_OFF 0
#define FLAG_POWER_ON  1
static void check_off_delay_work(struct work_struct *work)
{
	struct mtk_charger_type *info = (struct mtk_charger_type *)container_of(
				     work, struct mtk_charger_type, dwork.work);
	pr_info("%s: ENTER info->power_off_flag=%d\n", __func__,info->power_off_flag);
	
	if(info->power_off_flag == FLAG_POWER_OFF) {
		pr_info("%s: shutdown!\n", __func__);
		kernel_power_off();
	}
}
#endif

static void do_charger_detection_work(struct work_struct *data)
{
	struct mtk_charger_type *info = (struct mtk_charger_type *)container_of(
				     data, struct mtk_charger_type, chr_work);
	unsigned int chrdet = 0;

	/*HS03s for P210730-04606 by wangzikang at 20210816 start*/
	pr_notice("%s: enter\n", __func__);
	/*HS03s for P210730-04606 by wangzikang at 20210816 end*/
	chrdet = bc11_get_register_value(info->regmap,
		PMIC_RGS_CHRDET_ADDR,
		PMIC_RGS_CHRDET_MASK,
		PMIC_RGS_CHRDET_SHIFT);

	pr_notice("%s: chrdet:%d\n", __func__, chrdet);
	if (chrdet)
		do_charger_detect(info, chrdet);
	/*HS03s for P210730-04606 by wangzikang at 20210816 start*/
	#ifndef HQ_FACTORY_BUILD	//ss version
	else {
		hw_bc11_done(info);
		/* 8 = KERNEL_POWER_OFF_CHARGING_BOOT */
		/* 9 = LOW_POWER_OFF_CHARGING_BOOT */
		pr_info("%s: bootmode=%d\n", __func__,hq_get_boot_mode());
		if (hq_get_boot_mode() == KERNEL_POWER_OFF_CHARGING_BOOT ||
		hq_get_boot_mode() == LOW_POWER_OFF_CHARGING_BOOT) {
			pr_info("%s: Unplug Charger/USB\n", __func__);

/* HS04_T for DEAL6398A-1879 by shixuanxuan at 20221012 start */
/* HS03s_T for AL5626TDEV-715 by duanweiping at 20221102 start */
#if defined(CONFIG_HQ_PROJECT_HS04)||defined(CONFIG_HQ_PROJECT_HS03S)
/* HS03s_T for AL5626TDEV-715 by duanweiping at 20221102 end */
			pr_info("%s: system_state=%d\n", __func__,
				system_state);
			if (system_state != SYSTEM_POWER_OFF)
				kernel_power_off();
#else
#ifndef CONFIG_TCPC_CLASS
			pr_info("%s: system_state=%d\n", __func__,
				system_state);
			if (system_state != SYSTEM_POWER_OFF)
				kernel_power_off();
#endif
#endif
/* HS04_T for DEAL6398A-1879 by shixuanxuan at 20221012 end */
		}
	}
	#endif
	/*HS03s for P210730-04606 by wangzikang at 20210816 end*/
}


irqreturn_t chrdet_int_handler(int irq, void *data)
{
	struct mtk_charger_type *info = data;
	unsigned int chrdet = 0;

	/*HS03s for P210730-04606 by wangzikang at 20210816 start*/
	pr_notice("%s: enter\n", __func__);
	/*HS03s for P210730-04606 by wangzikang at 20210816 end*/

	chrdet = bc11_get_register_value(info->regmap,
		PMIC_RGS_CHRDET_ADDR,
		PMIC_RGS_CHRDET_MASK,
		PMIC_RGS_CHRDET_SHIFT);
	/*HS03s for P210730-04606 by wangzikang at 20210816 start*/
	#ifndef HQ_FACTORY_BUILD	//ss version
	if (!chrdet) {
		hw_bc11_done(info);
		/* 8 = KERNEL_POWER_OFF_CHARGING_BOOT */
		/* 9 = LOW_POWER_OFF_CHARGING_BOOT */
		pr_info("%s: bootmode=%d\n", __func__,hq_get_boot_mode());
		if (hq_get_boot_mode() == KERNEL_POWER_OFF_CHARGING_BOOT ||
		    hq_get_boot_mode() == LOW_POWER_OFF_CHARGING_BOOT) {
			pr_info("%s: Unplug Charger/USB\n", __func__);

/* HS04_T for DEAL6398A-1879 by shixuanxuan at 20221012 start */
/* HS03s_T for AL5626TDEV-715 by duanweiping at 20221102 start */
#if defined(CONFIG_HQ_PROJECT_HS04)||defined(CONFIG_HQ_PROJECT_HS03S)
/* HS03s_T for AL5626TDEV-715 by duanweiping at 20221102 end */
			pr_info("%s: system_state=%d\n", __func__,
				system_state);
			if (system_state != SYSTEM_POWER_OFF) {
				info->power_off_flag = FLAG_POWER_OFF;
				schedule_delayed_work(&info->dwork,msecs_to_jiffies(POWER_OFF_CHECK_TIME_MS));
			}
#else
#ifndef CONFIG_TCPC_CLASS
			pr_info("%s: system_state=%d\n", __func__,
				system_state);
			if (system_state != SYSTEM_POWER_OFF) {
				info->power_off_flag = FLAG_POWER_OFF;
				schedule_delayed_work(&info->dwork,msecs_to_jiffies(POWER_OFF_CHECK_TIME_MS));
			}
#endif
#endif
/* HS04_T for DEAL6398A-1879 by shixuanxuan at 20221012 end*/
		}
	}
	else {
		info->power_off_flag = FLAG_POWER_ON;
		pr_info("%s: Vbus recovery!\n", __func__);
	}
	#endif
	/*HS03s for P210730-04606 by wangzikang at 20210816 end*/
	pr_notice("%s: chrdet:%d\n", __func__, chrdet);
	do_charger_detect(info, chrdet);

	return IRQ_HANDLED;
}

/*HS03s for SR-AL5625-01-278 by wenyaqi at 20210701 start*/
#ifndef HQ_FACTORY_BUILD	//ss version
#ifdef CONFIG_HQ_PROJECT_HS03S
enum ss_batt_chr_status {
	SS_NOT_CHARGE = 0,
	SS_PRE_CHARGE,
	SS_FAST_CHARGE,
	SS_CHARGE_DONE,
};

static int ss_get_chr_type(struct charger_device *chg_dev, int *chr_type)
{
	int ret = 0;
	int chg_stat = 0;
	struct power_supply *psys = NULL;
	union power_supply_propval vbat;

	if (chg_dev == NULL || chr_type == NULL) {
		pr_err("%s: get chg_dev or chr_type failed\n", __func__);
		return -EINVAL;
	}
	ret = charger_dev_get_chr_status(chg_dev, &chg_stat);
	if (ret < 0) {
		pr_err("%s: get chg_stat failed\n", __func__);
		*chr_type = POWER_SUPPLY_CHARGE_TYPE_UNKNOWN;
		return ret;
	}

	switch(chg_stat)
	{
	case SS_NOT_CHARGE:
		*chr_type = POWER_SUPPLY_CHARGE_TYPE_NONE;
		break;
	case SS_PRE_CHARGE:
		*chr_type = POWER_SUPPLY_CHARGE_TYPE_TRICKLE;
		break;
	case SS_FAST_CHARGE:
	case SS_CHARGE_DONE:
		*chr_type = POWER_SUPPLY_CHARGE_TYPE_FAST;
		break;
	default:
		*chr_type = POWER_SUPPLY_CHARGE_TYPE_NONE;
		break;
	}

	psys = power_supply_get_by_name("battery");
	if (!IS_ERR_OR_NULL(psys)) {
		vbat.intval = 0;
		ret = power_supply_get_property(psys,
			POWER_SUPPLY_PROP_VOLTAGE_NOW, &vbat);
		if (ret < 0) {
			pr_err("%s: get vbat failed\n", __func__);
			return ret;
		}

		if (vbat.intval > 4350000)
			*chr_type = POWER_SUPPLY_CHARGE_TYPE_TAPER;
	}
	pr_debug("%s:charger_type=%d\n", __func__, *chr_type);
	return 0;
}
#endif
#ifdef CONFIG_HQ_PROJECT_HS04
enum ss_batt_chr_status {
	SS_NOT_CHARGE = 0,
	SS_PRE_CHARGE,
	SS_FAST_CHARGE,
	SS_CHARGE_DONE,
};

static int ss_get_chr_type(struct charger_device *chg_dev, int *chr_type)
{
	int ret = 0;
	int chg_stat = 0;
	struct power_supply *psys = NULL;
	union power_supply_propval vbat;

	if (chg_dev == NULL || chr_type == NULL) {
		pr_err("%s: get chg_dev or chr_type failed\n", __func__);
		return -EINVAL;
	}
	ret = charger_dev_get_chr_status(chg_dev, &chg_stat);
	if (ret < 0) {
		pr_err("%s: get chg_stat failed\n", __func__);
		*chr_type = POWER_SUPPLY_CHARGE_TYPE_UNKNOWN;
		return ret;
	}

	switch(chg_stat)
	{
	case SS_NOT_CHARGE:
		*chr_type = POWER_SUPPLY_CHARGE_TYPE_NONE;
		break;
	case SS_PRE_CHARGE:
		*chr_type = POWER_SUPPLY_CHARGE_TYPE_TRICKLE;
		break;
	case SS_FAST_CHARGE:
	case SS_CHARGE_DONE:
		*chr_type = POWER_SUPPLY_CHARGE_TYPE_FAST;
		break;
	default:
		*chr_type = POWER_SUPPLY_CHARGE_TYPE_NONE;
		break;
	}

	psys = power_supply_get_by_name("battery");
	if (!IS_ERR_OR_NULL(psys)) {
		vbat.intval = 0;
		ret = power_supply_get_property(psys,
			POWER_SUPPLY_PROP_VOLTAGE_NOW, &vbat);
		if (ret < 0) {
			pr_err("%s: get vbat failed\n", __func__);
			return ret;
		}

		if (vbat.intval > 4350000)
			*chr_type = POWER_SUPPLY_CHARGE_TYPE_TAPER;
	}
	pr_debug("%s:charger_type=%d\n", __func__, *chr_type);
	return 0;
}
#endif
#endif

static int psy_chr_type_get_property(struct power_supply *psy,
	enum power_supply_property psp, union power_supply_propval *val)
{
	struct mtk_charger_type *info = NULL;
	int vbus = 0;
	/*HS03s for SR-AL5625-01-278 by wenyaqi at 20210427 start*/
	/*HS03s for SR-AL5625-01-513 by wenyaqi at 20210608 start*/
	struct charger_device *chg = NULL;
	#ifndef HQ_FACTORY_BUILD	//ss version
	int chr_type = 0;
	#endif
#ifdef CONFIG_HQ_PROJECT_HS03S
	int chr_status = 0, ret = 0;
	bool chg_en = false;
	/*HS03s for P210623-03592 by wenyaqi at 20210625 start*/
	struct power_supply *psys = NULL;
	static int chr_status_old = 0;
#endif
#ifdef CONFIG_HQ_PROJECT_HS04
	int chr_status = 0, ret = 0;
	bool chg_en = false;
	/*HS03s for P210623-03592 by wenyaqi at 20210625 start*/
	struct power_supply *psys = NULL;
	static int chr_status_old = 0;
#endif
	/*HS03s for SR-AL5625-01-513 by wenyaqi at 20210608 end*/

	pr_notice("%s: prop:%d\n", __func__, psp);
	info = (struct mtk_charger_type *)power_supply_get_drvdata(psy);
	if (info->psy != NULL && info->psy == psy)
		chg = info->chg_dev;
	else {
		pr_err("%s fail\n", __func__);
	}

#ifdef CONFIG_HQ_PROJECT_OT8
	if (pd_hub_flag) {
		info->type = POWER_SUPPLY_USB_TYPE_SDP;
		info->psy_desc.type = POWER_SUPPLY_TYPE_USB;

		power_supply_changed(info->psy);
		pr_err("%s SXX info->type = %d, info->psy_desc.type = %d\n",
			__func__, info->type, info->psy_desc.type);
	}
#endif
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
	#ifndef HQ_FACTORY_BUILD	//ss version
	case POWER_SUPPLY_PROP_CHARGE_TYPE:
		if (chg != NULL) {
#ifdef CONFIG_HQ_PROJECT_HS03S
			ss_get_chr_type(chg, &chr_type);
#endif
#ifdef CONFIG_HQ_PROJECT_HS04
			ss_get_chr_type(chg, &chr_type);
#endif
#ifdef CONFIG_HQ_PROJECT_OT8
			charger_dev_get_chr_type(chg, &chr_type);
#endif
			/*HS03s for SR-AL5625-01-278 by wenyaqi at 20210701 end*/
			val->intval = chr_type;
		} else {
			val->intval = 0;
			pr_err("%s get_ibus fail\n", __func__);
		}
		break;
	/*HS03s for SR-AL5625-01-278 by wenyaqi at 20210427 end*/
	#endif
	/*HS03s for SR-AL5625-01-513 by wenyaqi at 20210608 start*/
#ifdef CONFIG_HQ_PROJECT_HS03S
	case POWER_SUPPLY_PROP_STATUS:
		if (info->type == POWER_SUPPLY_USB_TYPE_UNKNOWN)
		{
			val->intval = POWER_SUPPLY_STATUS_DISCHARGING;
			goto update_status;
		}
		/*HS03s for P210610-03373 by wenyaqi at 20210610 start*/
		if (chg == NULL) {
			pr_err("%s get chg fail\n", __func__);
			val->intval = POWER_SUPPLY_STATUS_DISCHARGING;
			goto update_status;
		}
		ret = charger_dev_get_chr_status(chg, &chr_status);
		if (ret < 0) {
			val->intval = POWER_SUPPLY_STATUS_DISCHARGING;
			goto update_status;
		}
		/*HS03s for P210610-03373 by wenyaqi at 20210610 end*/
		switch(chr_status) {
		case CHG_STATUS_NOT_CHARGING:
			val->intval = POWER_SUPPLY_STATUS_DISCHARGING;
			break;
		case CHG_STATUS_PRE_CHARGE:
		case CHG_STATUS_FAST_CHARGING:
			ret = charger_dev_is_enabled(chg, &chg_en);
			if(chg_en)
				val->intval = POWER_SUPPLY_STATUS_CHARGING;
			else
				val->intval = POWER_SUPPLY_STATUS_NOT_CHARGING;
			break;
		case CHG_STATUS_DONE:
			val->intval = POWER_SUPPLY_STATUS_FULL;
			break;
		default:
			ret = -ENODATA;
			break;
		}
	update_status:
		if (chr_status_old != val->intval) {
			chr_status_old = val->intval;
			power_supply_changed(info->psy);
			psys = power_supply_get_by_name("battery");
			if (!IS_ERR_OR_NULL(psys))
				power_supply_changed(psys);
		}
		/*HS03s for P210623-03592 by wenyaqi at 20210625 end*/
		break;
#endif
#ifdef CONFIG_HQ_PROJECT_HS04
	case POWER_SUPPLY_PROP_STATUS:
		if (info->type == POWER_SUPPLY_USB_TYPE_UNKNOWN)
		{
			val->intval = POWER_SUPPLY_STATUS_DISCHARGING;
			goto update_status;
		}
		/*HS03s for P210610-03373 by wenyaqi at 20210610 start*/
		if (chg == NULL) {
			pr_err("%s get chg fail\n", __func__);
			val->intval = POWER_SUPPLY_STATUS_DISCHARGING;
			goto update_status;
		}
		ret = charger_dev_get_chr_status(chg, &chr_status);
		if (ret < 0) {
			val->intval = POWER_SUPPLY_STATUS_DISCHARGING;
			goto update_status;
		}
		/*HS03s for P210610-03373 by wenyaqi at 20210610 end*/
		switch(chr_status) {
		case CHG_STATUS_NOT_CHARGING:
			val->intval = POWER_SUPPLY_STATUS_DISCHARGING;
			break;
		case CHG_STATUS_PRE_CHARGE:
		case CHG_STATUS_FAST_CHARGING:
			ret = charger_dev_is_enabled(chg, &chg_en);
			if(chg_en)
				val->intval = POWER_SUPPLY_STATUS_CHARGING;
			else
				val->intval = POWER_SUPPLY_STATUS_NOT_CHARGING;
			break;
		case CHG_STATUS_DONE:
			val->intval = POWER_SUPPLY_STATUS_FULL;
			break;
		default:
			ret = -ENODATA;
			break;
		}
	update_status:
		if (chr_status_old != val->intval) {
			chr_status_old = val->intval;
			power_supply_changed(info->psy);
			psys = power_supply_get_by_name("battery");
			if (!IS_ERR_OR_NULL(psys))
				power_supply_changed(psys);
		}
		/*HS03s for P210623-03592 by wenyaqi at 20210625 end*/
		break;
#endif
	/*HS03s for SR-AL5625-01-513 by wenyaqi at 20210608 end*/
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
		break;
	/*HS03s for SR-AL5625-01-369 by wenyaqi at 20210428 start*/
	case POWER_SUPPLY_PROP_TYPE:
		info->psy_desc.type = val->intval;
		break;
	case POWER_SUPPLY_PROP_USB_TYPE:
		info->type = val->intval;
		break;
	/*HS03s for SR-AL5625-01-369 by wenyaqi at 20210428 end*/
	default:
		return -EINVAL;
	}

	return 0;
}

/*HS03s for SR-AL5625-01-267 by wenyaqi at 20210427 start*/
#ifndef HQ_FACTORY_BUILD	//ss version
extern bool ss_musb_is_host(void);
static int mt_otg_get_property(struct power_supply *psy,
	enum power_supply_property psp, union power_supply_propval *val)
{
	struct mtk_charger_type *info;

	info = (struct mtk_charger_type *)power_supply_get_drvdata(psy);
	switch (psp) {
	case POWER_SUPPLY_PROP_ONLINE:
		val->intval = ss_musb_is_host();
		break;
	default:
		return -EINVAL;
	}

	return 0;
}
#endif
/*HS03s for SR-AL5625-01-267 by wenyaqi at 20210427 end*/

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

#ifdef CONFIG_HQ_PROJECT_HS03S
/* HS03s for SR-AL5625-01-52 by wenyaqi at 20210419 start */
extern int usb_cc_flag;
/* HS03s for SR-AL5625-01-52 by wenyaqi at 20210419 end */
#endif /*CONFIG_HQ_PROJECT_HS03S*/
#ifdef CONFIG_HQ_PROJECT_HS04
/* HS03s for SR-AL5625-01-52 by wenyaqi at 20210419 start */
extern int usb_cc_flag;
/* HS03s for SR-AL5625-01-52 by wenyaqi at 20210419 end */
#endif /*CONFIG_HQ_PROJECT_HS04*/

#ifdef CONFIG_HQ_PROJECT_HS03S
//wangtao for 06
static int mt_usb_get_property(struct power_supply *psy,
	enum power_supply_property psp, union power_supply_propval *val)
{
	struct mtk_charger_type *info;
	/* HS03s for SR-AL5625-01-55 by wenyaqi at 20210419 start */
	struct charger_device *chg = NULL;
	int vbus = 0;
	int ibus = 0;

	info = (struct mtk_charger_type *)power_supply_get_drvdata(psy);
	if (info->usb_psy != NULL && info->usb_psy == psy)
		chg = info->chg_dev;
	else {
		pr_err("%s fail\n", __func__);
	}

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
	case POWER_SUPPLY_PROP_VOLTAGE_NOW:
		get_vbus_voltage(info, &vbus);
		val->intval = vbus;
		break;
	case POWER_SUPPLY_PROP_INPUT_CURRENT_NOW:
		if (chg != NULL) {
			charger_dev_get_ibus(chg, &ibus);
			val->intval = ibus / 1000;
		} else {
			pr_err("%s get_ibus fail\n", __func__);
		}
		break;
	/* HS03s for SR-AL5625-01-55 by wenyaqi at 20210419 end */
	/* HS03s for SR-AL5625-01-52 by wenyaqi at 20210419 start */
	case POWER_SUPPLY_PROP_TYPEC_CC_ORIENTATION:
#ifdef CONFIG_HQ_PROJECT_HS03S
		val->intval = usb_cc_flag;
#endif /*CONFIG_HQ_PROJECT_HS03S*/
#ifdef CONFIG_HQ_PROJECT_HS04
		val->intval = usb_cc_flag;
#endif /*CONFIG_HQ_PROJECT_HS04*/
		val->intval = 0;
		break;
	/* HS03s for SR-AL5625-01-52 by wenyaqi at 20210419 end */
	default:
		return -EINVAL;
	}

	return 0;
}
#endif
#ifdef CONFIG_HQ_PROJECT_HS04
//wangtao for 06
static int mt_usb_get_property(struct power_supply *psy,
	enum power_supply_property psp, union power_supply_propval *val)
{
	struct mtk_charger_type *info;
	/* HS03s for SR-AL5625-01-55 by wenyaqi at 20210419 start */
	struct charger_device *chg = NULL;
	int vbus = 0;
	int ibus = 0;

	info = (struct mtk_charger_type *)power_supply_get_drvdata(psy);
	if (info->usb_psy != NULL && info->usb_psy == psy)
		chg = info->chg_dev;
	else {
		pr_err("%s fail\n", __func__);
	}

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
	case POWER_SUPPLY_PROP_VOLTAGE_NOW:
		get_vbus_voltage(info, &vbus);
		val->intval = vbus;
		break;
	case POWER_SUPPLY_PROP_INPUT_CURRENT_NOW:
		if (chg != NULL) {
			charger_dev_get_ibus(chg, &ibus);
			val->intval = ibus / 1000;
		} else {
			pr_err("%s get_ibus fail\n", __func__);
		}
		break;
	/* HS03s for SR-AL5625-01-55 by wenyaqi at 20210419 end */
	/* HS03s for SR-AL5625-01-52 by wenyaqi at 20210419 start */
	case POWER_SUPPLY_PROP_TYPEC_CC_ORIENTATION:
#ifdef CONFIG_HQ_PROJECT_HS03S
		val->intval = usb_cc_flag;
#endif /*CONFIG_HQ_PROJECT_HS03S*/
#ifdef CONFIG_HQ_PROJECT_HS04
		val->intval = usb_cc_flag;
#endif /*CONFIG_HQ_PROJECT_HS04*/
		val->intval = 0;
		break;
	/* HS03s for SR-AL5625-01-52 by wenyaqi at 20210419 end */
	default:
		return -EINVAL;
	}

	return 0;
}
#endif
#ifdef CONFIG_HQ_PROJECT_OT8
//for o8
//wangtao
extern int usb_cc_flag;
/* OT8 add for SR-AX3565-01-18 Add typec cc orientation by shixuanxuan at 2020/11/28 end */
static int mt_usb_get_property(struct power_supply *psy,
	enum power_supply_property psp, union power_supply_propval *val)
{
	struct mtk_charger_type *info;
	/*TabA7 Lite code for SR-AX3565-01-21 add sysFS node named usb/voltage_now by wenyaqi at 20201130 start*/
	struct power_supply *psys = NULL;
	/*TabA7 Lite code for SR-AX3565-01-21 add sysFS node named usb/input_current_now by wenyaqi at 20201130 start*/
	struct power_supply *psys2 = NULL;
	/*TabA7 Lite code for SR-AX3565-01-21 add sysFS node named usb/input_current_now by wenyaqi at 20201130 end*/

	psys = power_supply_get_by_name("mtk-master-charger");
	/*TabA7 Lite code for SR-AX3565-01-21 add sysFS node named usb/voltage_now by wenyaqi at 20201130 end*/
	/*TabA7 Lite code for SR-AX3565-01-21 add sysFS node named usb/input_current_now by wenyaqi at 20201130 start*/
	psys2 = power_supply_get_by_name("mt6370_pmu_charger");
	/*TabA7 Lite code for SR-AX3565-01-21 add sysFS node named usb/input_current_now by wenyaqi at 20201130 end*/
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
	/*TabA7 Lite code for SR-AX3565-01-21 add sysFS node named usb/voltage_now by wenyaqi at 20201130 start*/
	case POWER_SUPPLY_PROP_VOLTAGE_NOW:
		if (psys == NULL) {
			pr_err("[%s]psys is not rdy\n", __func__);
			return -EINVAL;
		}
		power_supply_get_property(psys,
			POWER_SUPPLY_PROP_VOLTAGE_NOW, val);
		break;
	/*TabA7 Lite code for SR-AX3565-01-21 add sysFS node named usb/voltage_now by wenyaqi at 20201130 end*/
	/*TabA7 Lite code for SR-AX3565-01-21 add sysFS node named usb/input_current_now by wenyaqi at 20201130 start*/
	case POWER_SUPPLY_PROP_INPUT_CURRENT_NOW:
		if (psys2 == NULL) {
			pr_err("[%s]psys2 is not rdy\n", __func__);
			return -EINVAL;
		}
		power_supply_get_property(psys2,
			POWER_SUPPLY_PROP_INPUT_CURRENT_NOW, val);
		break;
	/*TabA7 Lite code for SR-AX3565-01-21 add sysFS node named usb/input_current_now by wenyaqi at 20201130 end*/
	/* OT8 add for SR-AX3565-01-18 Add typec cc orientation by shixuanxuan at 2020/11/28 start */
	case POWER_SUPPLY_PROP_TYPEC_CC_ORIENTATION:
		val->intval = usb_cc_flag;
		break;
	/* OT8 add for SR-AX3565-01-18 Add typec cc orientation by shixuanxuan at 2020/11/28 end */
	default:
		return -EINVAL;
	}

	return 0;
}
#endif//for 08
static int psy_charger_type_property_is_writeable(struct power_supply *psy,
					       enum power_supply_property psp)
{
	switch (psp) {
	case POWER_SUPPLY_PROP_ONLINE:
	/*HS03s for SR-AL5625-01-369 by wenyaqi at 20210428 start*/
	case POWER_SUPPLY_PROP_TYPE:
	case POWER_SUPPLY_PROP_USB_TYPE:
	/*HS03s for SR-AL5625-01-369 by wenyaqi at 20210428 end*/
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

static int check_boot_mode(struct mtk_charger_type *info, struct device *dev)
{
	struct device_node *boot_node = NULL;
	struct tag_bootmode *tag = NULL;

	boot_node = of_parse_phandle(dev->of_node, "bootmode", 0);
	if (!boot_node)
		pr_notice("%s: failed to get boot mode phandle\n", __func__);
	else {
		tag = (struct tag_bootmode *)of_get_property(boot_node,
							"atag,boot", NULL);
		if (!tag)
			pr_notice("%s: failed to get atag,boot\n", __func__);
		else {
			pr_notice("%s: size:0x%x tag:0x%x bootmode:0x%x boottype:0x%x\n",
				__func__, tag->size, tag->tag,
				tag->bootmode, tag->boottype);
			info->bootmode = tag->bootmode;
			info->boottype = tag->boottype;
		}
	}
	return 0;
}

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

	/*HS03s for bugid by wangzikang at 20210731 start*/
	//spin_lock_init(&info->slock);
	/*HS03s for bugid by wangzikang at 20210731 end*/

	check_boot_mode(info, &pdev->dev);

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

	/*HS03s for SR-AL5625-01-267 by wenyaqi at 20210427 start*/
	#ifndef HQ_FACTORY_BUILD	//ss version
	info->otg_desc.name = "otg";
	info->otg_desc.type = POWER_SUPPLY_TYPE_DFP;
	info->otg_desc.properties = mt_otg_properties;
	info->otg_desc.num_properties = ARRAY_SIZE(mt_otg_properties);
	info->otg_desc.get_property = mt_otg_get_property;
	info->otg_cfg.drv_data = info;
	#endif
	/*HS03s for SR-AL5625-01-267 by wenyaqi at 20210427 end*/
	info->psy = power_supply_register(&pdev->dev, &info->psy_desc,
			&info->psy_cfg);

	if (IS_ERR(info->psy)) {
		pr_notice("%s Failed to register power supply: %ld\n",
			__func__, PTR_ERR(info->psy));
		return PTR_ERR(info->psy);
	}
	pr_notice("%s register psy success\n", __func__);

	/*HS03s for SR-AL5625-01-267 by wenyaqi at 20210427 start*/
	#ifndef HQ_FACTORY_BUILD	//ss version
	info->otg_psy = power_supply_register(&pdev->dev, &info->otg_desc,
			&info->otg_cfg);

	if (IS_ERR(info->otg_psy)) {
		pr_notice("%s Failed to register power supply: %ld\n",
			__func__, PTR_ERR(info->otg_psy));
		return PTR_ERR(info->otg_psy);
	}
	#endif
	/*HS03s for SR-AL5625-01-267 by wenyaqi at 20210427 end*/

	info->chan_vbus = devm_iio_channel_get(
		&pdev->dev, "pmic_vbus");
	if (IS_ERR(info->chan_vbus)) {
		pr_notice("chan_vbus auxadc get fail, ret=%d\n",
			PTR_ERR(info->chan_vbus));
	}

	if (of_property_read_u32(np, "bc12_active", &info->bc12_active) < 0)
		pr_notice("%s: no bc12_active\n", __func__);

	pr_notice("%s: bc12_active:%d\n", __func__, info->bc12_active);

	/* HS03s for SR-AL5625-01-55 by wenyaqi at 20210419 start */
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

	if (info->bc12_active) {
		INIT_WORK(&info->chr_work, do_charger_detection_work);
		#ifndef HQ_FACTORY_BUILD	//ss version
		INIT_DELAYED_WORK(&info->dwork,check_off_delay_work);
		#endif
		schedule_work(&info->chr_work);

		ret = devm_request_threaded_irq(&pdev->dev,
			platform_get_irq_byname(pdev, "chrdet"), NULL,
			chrdet_int_handler, IRQF_TRIGGER_HIGH, "chrdet", info);
		if (ret < 0)
			pr_notice("%s request chrdet irq fail\n", __func__);
	}

	info->first_connect = true;

	info->chg_dev = get_charger_by_name("primary_chg");
	if (info->chg_dev)
		pr_err("%s: Found primary charger\n", __func__);
	else {
		pr_err("%s: Error : can't find primary charger ***\n", __func__);
	}
	/* HS03s for SR-AL5625-01-55 by wenyaqi at 20210419 end */

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

