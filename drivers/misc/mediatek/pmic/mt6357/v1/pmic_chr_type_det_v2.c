/*
 * Copyright (C) 2016 MediaTek Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See http://www.gnu.org/licenses/gpl-2.0.html for more details.
 */

#include <linux/delay.h>
#include <linux/device.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/mutex.h>
#include <linux/power_supply.h>
#include <linux/reboot.h>
#include <linux/workqueue.h>

#include <mt-plat/charger_type.h>
#include <mt-plat/mtk_boot.h>
#include <mt-plat/upmu_common.h>
#include <mach/upmu_sw.h>
#include <mach/upmu_hw.h>

#ifdef CONFIG_MTK_USB2JTAG_SUPPORT
#include <mt-plat/mtk_usb2jtag.h>
#endif
#if defined(CONFIG_VBUS_NOTIFIER)
#include <linux/vbus_notifier.h>
#endif

#ifdef CONFIG_MTK_BQ24157_SUPPORT
#include <mt-plat/charger_class.h>
#endif

/* #define __FORCE_USB_TYPE__ */
#ifndef CONFIG_TCPC_CLASS
#define __SW_CHRDET_IN_PROBE_PHASE__
#endif

static enum charger_type g_chr_type;
#ifdef __SW_CHRDET_IN_PROBE_PHASE__
static struct work_struct chr_work;
#endif
static DEFINE_MUTEX(chrdet_lock);

static struct power_supply *chrdet_psy;
//+Bug492299,lili5.wt,ADD,20191021,battery Current event and slate mode
static struct delayed_work batt_work;
static struct power_supply *batt_psy;
static int count = 0;
//-Bug492299,lili5.wt,ADD,20191021,battery Current event and slate mode
//+Bug 510049, liuyong3, wt, ADD, 20191121, Add charger secdet for Non-standard charger
static struct delayed_work charger_secdet_work;
static bool need_rerun_det = false;
#define SECOND_DETECT_DELAY_TIME	1500
#ifdef CONFIG_TCPC_CLASS
bool ignore_usb_check = 0;
#endif
//-Bug 510049, liuyong3, wt, ADD, 20191121, Add charger secdet for Non-standard charger

#if !defined(CONFIG_FPGA_EARLY_PORTING)
static bool first_connect = true;
#endif

static int chrdet_inform_psy_changed(enum charger_type chg_type,
				bool chg_online)
{
	int ret = 0;
	union power_supply_propval propval;
#ifdef CONFIG_MTK_BQ24157_SUPPORT
	struct charger_device *chg_dev;
#endif

	pr_info("charger type: %s: online = %d, type = %d\n", __func__,
		chg_online, chg_type);

#ifdef CONFIG_MTK_BQ24157_SUPPORT
	chg_dev = get_charger_by_name("primary_chg");
	if(chg_dev==NULL){
	    pr_info(" %s: get charger dev err\n", __func__);
	}
	if (chg_dev != NULL && chg_dev->ops != NULL && chg_dev->ops->charger_set_wakelock){
	   pr_info(" %s: charger_set_wakelock\n", __func__);
	    chg_dev->ops->charger_set_wakelock(chg_dev, chg_online);
	}
#endif

	/* Inform chg det power supply */
	if (chg_online) {
		propval.intval = chg_online;
		ret = power_supply_set_property(chrdet_psy,
				POWER_SUPPLY_PROP_ONLINE, &propval);
		if (ret < 0)
			pr_notice("%s: psy online failed, ret = %d\n",
				__func__, ret);

		propval.intval = chg_type;
		ret = power_supply_set_property(chrdet_psy,
				POWER_SUPPLY_PROP_CHARGE_TYPE, &propval);
		if (ret < 0)
			pr_notice("%s: psy type failed, ret = %d\n",
				__func__, ret);

		return ret;
	}

	propval.intval = chg_type;
	ret = power_supply_set_property(chrdet_psy,
				POWER_SUPPLY_PROP_CHARGE_TYPE, &propval);
	if (ret < 0)
		pr_notice("%s: psy type failed, ret(%d)\n", __func__, ret);

	propval.intval = chg_online;
	ret = power_supply_set_property(chrdet_psy, POWER_SUPPLY_PROP_ONLINE,
				&propval);
	if (ret < 0)
		pr_notice("%s: psy online failed, ret(%d)\n", __func__, ret);
	return ret;
}

#if defined(CONFIG_FPGA_EARLY_PORTING)
/* FPGA */
int hw_charging_get_charger_type(void)
{
	/* Force Standard USB Host */
	g_chr_type = STANDARD_HOST;
	chrdet_inform_psy_changed(g_chr_type, 1);
	return g_chr_type;
}

#else
/* EVB / Phone */
static void hw_bc11_init(void)
{
	int timeout = 200;

	msleep(200);

	if (get_boot_mode() == KERNEL_POWER_OFF_CHARGING_BOOT ||
	    get_boot_mode() == LOW_POWER_OFF_CHARGING_BOOT) {
		pr_info("Skip usb_ready check in KPOC\n");
		goto skip;
	}

	if (first_connect == true) {
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
		first_connect = false;
	}

skip:
	/* RG_bc11_BIAS_EN=1 */
	bc11_set_register_value(PMIC_RG_BC11_BIAS_EN, 1);
	/* RG_bc11_VSRC_EN[1:0]=00 */
	bc11_set_register_value(PMIC_RG_BC11_VSRC_EN, 0);
	/* RG_bc11_VREF_VTH = [1:0]=00 */
	bc11_set_register_value(PMIC_RG_BC11_VREF_VTH, 0);
	/* RG_bc11_CMP_EN[1.0] = 00 */
	bc11_set_register_value(PMIC_RG_BC11_CMP_EN, 0);
	/* RG_bc11_IPU_EN[1.0] = 00 */
	bc11_set_register_value(PMIC_RG_BC11_IPU_EN, 0);
	/* RG_bc11_IPD_EN[1.0] = 00 */
	bc11_set_register_value(PMIC_RG_BC11_IPD_EN, 0);
	/* bc11_RST=1 */
	bc11_set_register_value(PMIC_RG_BC11_RST, 1);
	/* bc11_BB_CTRL=1 */
	bc11_set_register_value(PMIC_RG_BC11_BB_CTRL, 1);
	/* add pull down to prevent PMIC leakage */
	bc11_set_register_value(PMIC_RG_BC11_IPD_EN, 0x1);
	msleep(50);

	Charger_Detect_Init();
}

static unsigned int hw_bc11_DCD(void)
{
	unsigned int wChargerAvail = 0;
	/* RG_bc11_IPU_EN[1.0] = 10 */
	bc11_set_register_value(PMIC_RG_BC11_IPU_EN, 0x2);
	/* RG_bc11_IPD_EN[1.0] = 01 */
	bc11_set_register_value(PMIC_RG_BC11_IPD_EN, 0x1);
	/* RG_bc11_VREF_VTH = [1:0]=01 */
	bc11_set_register_value(PMIC_RG_BC11_VREF_VTH, 0x1);
	/* RG_bc11_CMP_EN[1.0] = 10 */
	bc11_set_register_value(PMIC_RG_BC11_CMP_EN, 0x2);
	msleep(80);
	/* mdelay(80); */
	wChargerAvail = bc11_get_register_value(PMIC_RGS_BC11_CMP_OUT);

	/* RG_bc11_IPU_EN[1.0] = 00 */
	bc11_set_register_value(PMIC_RG_BC11_IPU_EN, 0x0);
	/* RG_bc11_IPD_EN[1.0] = 00 */
	bc11_set_register_value(PMIC_RG_BC11_IPD_EN, 0x0);
	/* RG_bc11_CMP_EN[1.0] = 00 */
	bc11_set_register_value(PMIC_RG_BC11_CMP_EN, 0x0);
	/* RG_bc11_VREF_VTH = [1:0]=00 */
	bc11_set_register_value(PMIC_RG_BC11_VREF_VTH, 0x0);
	return wChargerAvail;
}

#if 0 /* If need to detect apple adapter, please enable this code section */
static unsigned int hw_bc11_stepA1(void)
{
	unsigned int wChargerAvail = 0;
	/* RG_bc11_IPD_EN[1.0] = 01 */
	bc11_set_register_value(PMIC_RG_BC11_IPD_EN, 0x1);
	/* RG_bc11_VREF_VTH = [1:0]=00 */
	bc11_set_register_value(PMIC_RG_BC11_VREF_VTH, 0x0);
	/* RG_bc11_CMP_EN[1.0] = 01 */
	bc11_set_register_value(PMIC_RG_BC11_CMP_EN, 0x1);
	msleep(80);
	/* mdelay(80); */
	wChargerAvail = bc11_get_register_value(PMIC_RGS_BC11_CMP_OUT);

	/* RG_bc11_IPD_EN[1.0] = 00 */
	bc11_set_register_value(PMIC_RG_BC11_IPD_EN, 0x0);
	/* RG_bc11_CMP_EN[1.0] = 00 */
	bc11_set_register_value(PMIC_RG_BC11_CMP_EN, 0x0);
	return wChargerAvail;
}
#endif

static unsigned int hw_bc11_stepA2(void)
{
	unsigned int wChargerAvail = 0;
	/* RG_bc11_VSRC_EN[1.0] = 10 */
	bc11_set_register_value(PMIC_RG_BC11_VSRC_EN, 0x2);
	/* RG_bc11_IPD_EN[1:0] = 01 */
	bc11_set_register_value(PMIC_RG_BC11_IPD_EN, 0x1);
	/* RG_bc11_VREF_VTH = [1:0]=00 */
	bc11_set_register_value(PMIC_RG_BC11_VREF_VTH, 0x0);
	/* RG_bc11_CMP_EN[1.0] = 01 */
	bc11_set_register_value(PMIC_RG_BC11_CMP_EN, 0x1);
	msleep(80);
	/* mdelay(80); */
	wChargerAvail = bc11_get_register_value(PMIC_RGS_BC11_CMP_OUT);

	/* RG_bc11_VSRC_EN[1:0]=00 */
	bc11_set_register_value(PMIC_RG_BC11_VSRC_EN, 0x0);
	/* RG_bc11_IPD_EN[1.0] = 00 */
	bc11_set_register_value(PMIC_RG_BC11_IPD_EN, 0x0);
	/* RG_bc11_CMP_EN[1.0] = 00 */
	bc11_set_register_value(PMIC_RG_BC11_CMP_EN, 0x0);
	return wChargerAvail;
}

static unsigned int hw_bc11_stepB2(void)
{
	unsigned int wChargerAvail = 0;

	/*enable the voltage source to DM*/
	bc11_set_register_value(PMIC_RG_BC11_VSRC_EN, 0x1);
	/* enable the pull-down current to DP */
	bc11_set_register_value(PMIC_RG_BC11_IPD_EN, 0x2);
	/* VREF threshold voltage for comparator  =0.325V */
	bc11_set_register_value(PMIC_RG_BC11_VREF_VTH, 0x0);
	/* enable the comparator to DP */
	bc11_set_register_value(PMIC_RG_BC11_CMP_EN, 0x2);
	msleep(80);
	wChargerAvail = bc11_get_register_value(PMIC_RGS_BC11_CMP_OUT);
	/*reset to default value*/
	bc11_set_register_value(PMIC_RG_BC11_VSRC_EN, 0x0);
	bc11_set_register_value(PMIC_RG_BC11_IPD_EN, 0x0);
	bc11_set_register_value(PMIC_RG_BC11_CMP_EN, 0x0);
	if (wChargerAvail == 1) {
		bc11_set_register_value(PMIC_RG_BC11_VSRC_EN, 0x2);
		pr_info("charger type: DCP, keep DM voltage source in stepB2\n");
	}
	return wChargerAvail;

}

static void hw_bc11_done(void)
{
	/* RG_bc11_VSRC_EN[1:0]=00 */
	bc11_set_register_value(PMIC_RG_BC11_VSRC_EN, 0x0);
	/* RG_bc11_VREF_VTH = [1:0]=0 */
	bc11_set_register_value(PMIC_RG_BC11_VREF_VTH, 0x0);
	/* RG_bc11_CMP_EN[1.0] = 00 */
	bc11_set_register_value(PMIC_RG_BC11_CMP_EN, 0x0);
	/* RG_bc11_IPU_EN[1.0] = 00 */
	bc11_set_register_value(PMIC_RG_BC11_IPU_EN, 0x0);
	/* RG_bc11_IPD_EN[1.0] = 00 */
	bc11_set_register_value(PMIC_RG_BC11_IPD_EN, 0x0);
	/* RG_bc11_BIAS_EN=0 */
	bc11_set_register_value(PMIC_RG_BC11_BIAS_EN, 0x0);
	Charger_Detect_Release();
}

static void dump_charger_name(enum charger_type type)
{
	switch (type) {
	case CHARGER_UNKNOWN:
		pr_info("charger type: %d, CHARGER_UNKNOWN\n", type);
		break;
	case STANDARD_HOST:
		pr_info("charger type: %d, Standard USB Host\n", type);
		break;
	case CHARGING_HOST:
		pr_info("charger type: %d, Charging USB Host\n", type);
		break;
	case NONSTANDARD_CHARGER:
		pr_info("charger type: %d, Non-standard Charger\n", type);
		break;
	case STANDARD_CHARGER:
		pr_info("charger type: %d, Standard Charger\n", type);
		break;
	case APPLE_2_1A_CHARGER:
		pr_info("charger type: %d, APPLE_2_1A_CHARGER\n", type);
		break;
	case APPLE_1_0A_CHARGER:
		pr_info("charger type: %d, APPLE_1_0A_CHARGER\n", type);
		break;
	case APPLE_0_5A_CHARGER:
		pr_info("charger type: %d, APPLE_0_5A_CHARGER\n", type);
		break;
	default:
		pr_info("charger type: %d, Not Defined!!!\n", type);
		break;
	}
}

int hw_charging_get_charger_type(void)
{
	enum charger_type CHR_Type_num = CHARGER_UNKNOWN;

#ifdef CONFIG_MTK_USB2JTAG_SUPPORT
	if (usb2jtag_mode()) {
		pr_info("[USB2JTAG] in usb2jtag mode, skip charger detection\n");
		return STANDARD_HOST;
	}
#endif

	hw_bc11_init();

	if (hw_bc11_DCD()) {
#if 0 /* If need to detect apple adapter, please enable this code section */
		if (hw_bc11_stepA1())
			CHR_Type_num = APPLE_2_1A_CHARGER;
		else
			CHR_Type_num = NONSTANDARD_CHARGER;
#else
		CHR_Type_num = NONSTANDARD_CHARGER;
#endif
	} else {
		if (hw_bc11_stepA2()) {
			if (hw_bc11_stepB2())
				CHR_Type_num = STANDARD_CHARGER;
			else
				CHR_Type_num = CHARGING_HOST;
		} else
			CHR_Type_num = STANDARD_HOST;
	}


	if (CHR_Type_num == CHARGING_HOST) {
		if (get_boot_mode() == KERNEL_POWER_OFF_CHARGING_BOOT ||
		    get_boot_mode() == LOW_POWER_OFF_CHARGING_BOOT) {
			pr_info("Pull up D+ to 0.6V for CDP in KPOC\n");
			msleep(100);
			bc11_set_register_value(PMIC_RG_BC11_VSRC_EN, 0x2);
			bc11_set_register_value(PMIC_RG_BC11_IPU_EN, 0x2);
		} else
			hw_bc11_done();
	} else if (CHR_Type_num != STANDARD_CHARGER)
		hw_bc11_done();
	else
		pr_info("charger type: skip bc11 release for BC12 DCP SPEC\n");

	dump_charger_name(CHR_Type_num);


#ifdef __FORCE_USB_TYPE__
	CHR_Type_num = STANDARD_HOST;
	pr_info("charger type: Froce to STANDARD_HOST\n");
#endif

	return CHR_Type_num;
}

extern bool is_pd_active(void);
void mtk_pmic_enable_chr_type_det(bool en)
{
#ifndef CONFIG_TCPC_CLASS
	if (!mt_usb_is_device()) {
		g_chr_type = CHARGER_UNKNOWN;
		pr_info("charger type: UNKNOWN, Now is usb host mode. Skip detection\n");
		return;
	}
#endif

	mutex_lock(&chrdet_lock);

	if (en) {
		if (is_meta_mode()) {
			/* Skip charger type detection to speed up meta boot */
			pr_notice("charger type: force Standard USB Host in meta\n");
			g_chr_type = STANDARD_HOST;
			chrdet_inform_psy_changed(g_chr_type, 1);
//+Extb 200325-03787,chenxu,WT,Add,,20200410,power swap without usb disconnect 
		} else if (g_ignore_usb) {
			/* Skip charger type detection for pr_swap */
			pr_notice("charger type: force Standard USB Host for pr_swap\n");
			g_chr_type = STANDARD_HOST;
			chrdet_inform_psy_changed(g_chr_type, 1);
//-Extb 200325-03787,chenxu,WT,Add,,20200410,power swap without usb disconnect
		} else {
//+EXTB P200309-06181,zhaosidong.wt,ADD,20200401,multiport adapter detection
			if(is_pd_active() && need_rerun_det) {
				pr_info("force charger type: STANDARD_HOST\n");
				g_chr_type = STANDARD_HOST;
			} else {
				pr_info("charger type: charger IN\n");
				g_chr_type = hw_charging_get_charger_type();
			}
#ifdef CONFIG_TCPC_CLASS
			if((!ignore_usb_check) && (NONSTANDARD_CHARGER == g_chr_type) && (!need_rerun_det)) {
#else
		    if((NONSTANDARD_CHARGER == g_chr_type) && (!need_rerun_det)) {
#endif
				schedule_delayed_work(&charger_secdet_work, msecs_to_jiffies(SECOND_DETECT_DELAY_TIME));
				need_rerun_det = true;
				pr_info("ignore chrdet changed,start second check\n");
			} else {
				chrdet_inform_psy_changed(g_chr_type, 1);
			}
//-EXTB P200309-06181,zhaosidong.wt,ADD,20200401,multiport adapter detection
			schedule_delayed_work(&batt_work, msecs_to_jiffies(0));//Bug492299,lili5.wt,ADD,20191021,battery Current event and slate mode
		}
	} else {
		pr_info("charger type: charger OUT\n");
		//+Bug 510049, liuyong3, wt, ADD, 20191121, Add charger secdet for Non-standard charger
		cancel_delayed_work(&charger_secdet_work);
		need_rerun_det = false;
		//-Bug 510049, liuyong3, wt, ADD, 20191121, Add charger secdet for Non-standard charger
		g_chr_type = CHARGER_UNKNOWN;
		chrdet_inform_psy_changed(g_chr_type, 0);
		count = 0;//Bug492299,lili5.wt,ADD,20191021,battery Current event and slate mode
	}

	mutex_unlock(&chrdet_lock);
}

/* Charger Detection */
void do_charger_detect(void)
{
	if (pmic_get_register_value(PMIC_RGS_CHRDET))
		mtk_pmic_enable_chr_type_det(true);
	else
		mtk_pmic_enable_chr_type_det(false);
}

//+Bug 510049, liuyong3, wt, ADD, 20191121, Add charger secdet for Non-standard charger
static void do_charger_secdet_work()
{
	if (pmic_get_register_value(PMIC_RGS_CHRDET)){
		mtk_pmic_enable_chr_type_det(true);
	}
}
//-Bug 510049, liuyong3, wt, ADD, 20191121, Add charger secdet for Non-standard charger

/* PMIC Int Handler */
void chrdet_int_handler(void)
{

#if defined(CONFIG_VBUS_NOTIFIER)
	unsigned short vbus = pmic_get_register_value(PMIC_RGS_CHRDET);
	vbus_status_t status = (vbus > 0) ?
		STATUS_VBUS_HIGH : STATUS_VBUS_LOW;
	
	vbus_notifier_handle(status);
#endif
	do_charger_detect(); //Bug 522914,zhaosidong.wt,MODIFY,20191217, in LPM plug out TA will trigger reboot
	/*
	 * pr_notice("[chrdet_int_handler]CHRDET status = %d....\n",
	 *	pmic_get_register_value(PMIC_RGS_CHRDET));
	 */
	if (!pmic_get_register_value(PMIC_RGS_CHRDET)) {
		int boot_mode = 0;

		hw_bc11_done();
		boot_mode = get_boot_mode();

		if (boot_mode == KERNEL_POWER_OFF_CHARGING_BOOT
		    || boot_mode == LOW_POWER_OFF_CHARGING_BOOT) {
			pr_info("[chrdet_int_handler] Unplug Charger/USB\n");

#ifndef CONFIG_TCPC_CLASS
			pr_info("%s: system_state=%d\n", __func__,
				system_state);
			if (system_state != SYSTEM_POWER_OFF)
				kernel_power_off();
#else
			return;
#endif
		}
	}
}


/* Charger Probe Related */
#ifdef __SW_CHRDET_IN_PROBE_PHASE__
static void do_charger_detection_work(struct work_struct *data)
{
#if defined(CONFIG_VBUS_NOTIFIER)
	unsigned short vbus = pmic_get_register_value(PMIC_RGS_CHRDET);
	vbus_status_t status = (vbus > 0) ?
		STATUS_VBUS_HIGH : STATUS_VBUS_LOW;
	
	vbus_notifier_handle(status);
#endif
	if (pmic_get_register_value(PMIC_RGS_CHRDET))
		do_charger_detect();
}
#endif

//+Bug492299,lili5.wt,ADD,20191021,battery Current event and slate mode
#define low_current_level 100

static void battery_current_monitoring_work(struct work_struct *data)
{
	union power_supply_propval val;
	int ret= 0;

	if(g_chr_type == STANDARD_HOST){
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
//-Bug492299,lili5.wt,ADD,20191021,battery Current event and slate mode

static int __init pmic_chrdet_init(void)
{
	mutex_init(&chrdet_lock);
	chrdet_psy = power_supply_get_by_name("charger");
	if (!chrdet_psy) {
		pr_notice("%s: get power supply failed\n", __func__);
		return -EINVAL;
	}

	//+Bug492299,lili5.wt,ADD,20191021,battery Current event and slate mode
	batt_psy = power_supply_get_by_name("battery");
	if (!batt_psy) {
		pr_notice("%s: get power supply failed\n", __func__);
		return -EINVAL;
	}
	//-Bug492299,lili5.wt,ADD,20191021,battery Current event and slate mode

#ifdef __SW_CHRDET_IN_PROBE_PHASE__
	/* do charger detect here to prevent HW miss interrupt*/
	INIT_WORK(&chr_work, do_charger_detection_work);
	schedule_work(&chr_work);
#endif
	//+Bug492299,lili5.wt,ADD,20191021,battery Current event and slate mode
	INIT_DELAYED_WORK(&batt_work, battery_current_monitoring_work);
	//-Bug492299,lili5.wt,ADD,20191021,battery Current event and slate mode
	//+Bug 510049, liuyong3, wt, ADD, 20191121, Add charger secdet for Non-standard charger
	INIT_DELAYED_WORK(&charger_secdet_work, do_charger_secdet_work);
	//-Bug 510049, liuyong3, wt, ADD, 20191121, Add charger secdet for Non-standard charger

#ifndef CONFIG_TCPC_CLASS
	pmic_register_interrupt_callback(INT_CHRDET_EDGE, chrdet_int_handler);
	pmic_enable_interrupt(INT_CHRDET_EDGE, 1, "PMIC");
#endif

	return 0;
}

late_initcall(pmic_chrdet_init);

#endif /* CONFIG_FPGA_EARLY_PORTING */
