/*
 * Copyright (C) 2015 MediaTek Inc.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 */

#define pr_fmt(fmt) KBUILD_MODNAME ": %s: " fmt, __func__

#include <linux/types.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/device.h>
#include <linux/platform_device.h>
#include <linux/hrtimer.h>
#include <linux/ktime.h>
#include <linux/workqueue.h>
#include <linux/mutex.h>
#include <linux/of.h>
#include <linux/list.h>
#include <linux/delay.h>
#include <linux/pinctrl/consumer.h>

#include "flashlight-core.h"
#include "flashlight-dt.h"

#include "flashlight-boardid.h"

#ifdef CONFIG_MTK_PWM
#include <mt-plat/mtk_pwm.h>
#endif

#undef LOG_INF
#define FLASHLIGHTS_OCP8111_DEBUG 1
#if FLASHLIGHTS_OCP8111_DEBUG
#define LOG_INF(format, args...) pr_info("flashLight-ocp8111" "[%s]"  format, __func__,  ##args)
#else
#define LOG_INF(format, args...)
#endif
#define LOG_ERR(format, args...) pr_err("flashLight-ocp8111" "[%s]" format, __func__, ##args)

//+bug 682590, zhanghao2.wt, ADD, 2021/9/28, fix flashlight close screen and torch had closed.
#ifdef CONFIG_PM_WAKELOCKS
#include <linux/pm_wakeup.h>
#else
#include <linux/wakelock.h>
#endif

#ifdef CONFIG_PM_WAKELOCKS
static struct wakeup_source *torch_wake_lock;
#else
static struct wake_lock *torch_wake_lock;
#endif

static volatile int g_torchWaitLock;
//-bug 682590, zhanghao2.wt, ADD, 2021/9/28, fix flashlight close screen and torch had closed.

/* define device tree */
/* TODO: modify temp device tree name */
#if 0
#ifndef OCP8111_GPIO_DTNAME
#ifdef CONFIG_MTK_96516_CAMERA
#define OCP8111_GPIO_DTNAME "mediatek,flashlights_ocp8132a"
#else
#define OCP8111_GPIO_DTNAME "mediatek,flashlights_ocp8111"
#endif
#endif
#endif
#define OCP8111_GPIO_DTNAME "mediatek,flashlights_ocp8111"


/* TODO: define driver name */
#define OCP8111_NAME "flashlights-ocp8111"

/* define registers */
/* TODO: define register */

/* define mutex and work queue */
static DEFINE_MUTEX(ocp8111_mutex);
static struct work_struct ocp8111_work;
static int mainFlashFlag  = 0;
static int TorchFlag  = 1;


/* define pinctrl */
/* TODO: define pinctrl */
#define OCP8111_PINCTRL_PIN_XXX 0
#define OCP8111_PINCTRL_PINSTATE_LOW 0
#define OCP8111_PINCTRL_PINSTATE_HIGH 1
#define OCP8111_PINCTRL_STATE_XXX_HIGH "flashlights_on"
#define OCP8111_PINCTRL_STATE_XXX_LOW  "flashlights_off"
static struct pinctrl *ocp8111_pinctrl;
static struct pinctrl_state *ocp8111_xxx_high;
static struct pinctrl_state *ocp8111_xxx_low;

#define OCP8111_PINCTRL_PIN_MAIN 1
#define OCP8111_PINCTRL_MAIN_PINSTATE_LOW 0
#define OCP8111_PINCTRL_MAIN_PINSTATE_HIGH 1
#define OCP8111_PINCTRL_STATE_MAIN_HIGH "flashlights_on_main"
#define OCP8111_PINCTRL_STATE_MAIN_LOW  "flashlights_off_main"
static struct pinctrl_state *ocp8111_main_high;
static struct pinctrl_state *ocp8111_main_low;

#define OCP8111_PINCTRL_PIN_PWM_EN "flashlights_pwm_pin"
#define OCP8111_PINCTRL_PIN_PWM_AW_EN "flashlights_pwm_aw_pin"
static struct pinctrl_state *flashlight_pwm_en;
static struct pinctrl_state *flashlight_pwm_aw_en;
static int light_pwm_value;
#define GET_BOARDID_ERR 0
#define USE_AW3641 1
#define USE_OCP8111 2
#define USE_AW3641_PVT 3
#define USE_OCP8111_PVT 4
extern camera_switch;
extern hs_video_flag;
int pwm_flag = 0;
/* define usage count */
static int use_count;

/* platform data */
struct ocp8111_platform_data {
	int channel_num;
	struct flashlight_device_id *dev_id;
};

//+bug 682590, zhanghao2.wt, ADD, 2021/9/28, fix flashlight close screen and torch had closed.
static void enable_torch_wakelock(void)
{
	if (g_torchWaitLock == 0) {
#ifdef CONFIG_PM_WAKELOCKS
		__pm_stay_awake(torch_wake_lock);
#else
		wake_lock(torch_wake_lock);
#endif
		g_torchWaitLock = 1;
		LOG_INF("torch wakelock enable!!\n");
	}
}

static void diable_torch_wakelock(void)
{
	if (g_torchWaitLock == 1) {
#ifdef CONFIG_PM_WAKELOCKS
		__pm_relax(torch_wake_lock);
#else
		wake_unlock(torch_wake_lock);
#endif
		g_torchWaitLock = 0;
		LOG_INF("torch wakelock diable!!\n");
	}
}

//extern char* boardid_get();
//-bug 682590, zhanghao2.wt, ADD, 2021/9/28, fix flashlight close screen and torch had closed.

/******************************************************************************
 * Pinctrl configuration
 *****************************************************************************/
static int ocp8111_pinctrl_init(struct platform_device *pdev)
{
	int ret = 0;

	/* get pinctrl */
	ocp8111_pinctrl = devm_pinctrl_get(&pdev->dev);
	if (IS_ERR(ocp8111_pinctrl)) {
		pr_err("Failed to get flashlight pinctrl.\n");
		ret = PTR_ERR(ocp8111_pinctrl);
		return ret;
	}

	/* TODO: Flashlight XXX pin initialization */
	ocp8111_xxx_high = pinctrl_lookup_state(
			ocp8111_pinctrl, OCP8111_PINCTRL_STATE_XXX_HIGH);
	if (IS_ERR(ocp8111_xxx_high)) {
		pr_err("Failed to init (%s)\n", OCP8111_PINCTRL_STATE_XXX_HIGH);
		ret = PTR_ERR(ocp8111_xxx_high);
	}
	ocp8111_xxx_low = pinctrl_lookup_state(
			ocp8111_pinctrl, OCP8111_PINCTRL_STATE_XXX_LOW);
	if (IS_ERR(ocp8111_xxx_low)) {
		pr_err("Failed to init (%s)\n", OCP8111_PINCTRL_STATE_XXX_LOW);
		ret = PTR_ERR(ocp8111_xxx_low);
	}

	ocp8111_main_high = pinctrl_lookup_state(
			ocp8111_pinctrl, OCP8111_PINCTRL_STATE_MAIN_HIGH);
	if (IS_ERR(ocp8111_main_high)) {
		pr_err("Failed to init (%s)\n", OCP8111_PINCTRL_STATE_MAIN_HIGH);
		ret = PTR_ERR(ocp8111_main_high);
	}
	ocp8111_main_low = pinctrl_lookup_state(
			ocp8111_pinctrl, OCP8111_PINCTRL_STATE_MAIN_LOW);
	if (IS_ERR(ocp8111_main_low)) {
		pr_err("Failed to init (%s)\n", OCP8111_PINCTRL_STATE_MAIN_LOW);
		ret = PTR_ERR(ocp8111_main_low);
	}

	flashlight_pwm_en = pinctrl_lookup_state(ocp8111_pinctrl, OCP8111_PINCTRL_PIN_PWM_EN);
	if (IS_ERR(flashlight_pwm_en)) {
		pr_err("Failed to init (%s)\n", OCP8111_PINCTRL_PIN_PWM_EN);
		ret = PTR_ERR(flashlight_pwm_en);
	}
	flashlight_pwm_aw_en = pinctrl_lookup_state(ocp8111_pinctrl, OCP8111_PINCTRL_PIN_PWM_AW_EN);
	if (IS_ERR(flashlight_pwm_aw_en)) {
		pr_err("Failed to init (%s)\n", OCP8111_PINCTRL_PIN_PWM_AW_EN);
		ret = PTR_ERR(flashlight_pwm_aw_en);
	}

	return ret;
}


static int mt_flashlight_led_set_pwm(int pwm_num, u32 level, u32 clk_src, u32 clk_div, u16 DATA_WIDTH)
{
    struct pwm_spec_config pwm_setting;
    memset(&pwm_setting, 0, sizeof(struct pwm_spec_config));
	/*pwm_no is by IC HW design, this value is different by different IC(MTK doc: PWM UserGuide) \u2014\u2014\u4ecedws\u91cc\u9762\u67e5\u770b\uff1f*/
    pwm_setting.pwm_no = pwm_num;
	/*OLD or FIFO*/
    pwm_setting.mode = PWM_MODE_OLD;
    /*use channel in pmic, or not, default is FALSE*/
    pwm_setting.pmic_pad = 0;
	/*\u5206\u9891\u7cfb\u6570*/
    pwm_setting.clk_div = clk_div;
	/*src clk is by IC HW design, this value is different by different IC(MTK doc: PWM UserGuide) \u2014\u2014\u672a\u627e\u5230\u67e5\u627e\u5bf9\u5e94src\u7684\u65b9\u6cd5*/
    pwm_setting.clk_src = clk_src;
    /*FALSE: after the PWM wave send finished, the voltage level of this GPIO is low
     * TRUE: after the PWM wave send finished, the voltage level of this GPIO is high*/
    pwm_setting.PWM_MODE_OLD_REGS.IDLE_VALUE = 0;
    /* FALSE: there is no interval time between 2 complete waveform
     * TRUE: interval time between 2 complete waveform, the interval time = DATA_WIDTH*/
    pwm_setting.PWM_MODE_OLD_REGS.GUARD_VALUE = 0;
    /* Guard\u7684\u957f\u5ea6\uff0cold mode\u4e0b\u662f\u65e0\u6548\u503c */
    pwm_setting.PWM_MODE_OLD_REGS.GDURATION = 0;
    /* waveform numbers. WAVE_NUM= 0 means PWM will send output always, until disable pwm, the waveform will stoped */
    pwm_setting.PWM_MODE_OLD_REGS.WAVE_NUM = 0;
    /* the time of onecomplete waveform, \u6b64\u5904\u542b\u4e49\u662f\u4e00\u4e2a\u5b8c\u6574\u6ce2\u5f62\u5305\u542b100\u4e2aclk */
    pwm_setting.PWM_MODE_OLD_REGS.DATA_WIDTH = DATA_WIDTH;
    /* the time of high voltage level in one complete waveform, it means duty cycle, \u6b64\u5904\u542b\u4e49\u662f\u4e00\u4e2a\u5b8c\u6574\u6ce2\u5f62\u4e2d\u9ad8\u7535\u5e73\u7684\u65f6\u957f\u4e3alevel\u4e2aclk */
    pwm_setting.PWM_MODE_OLD_REGS.THRESH = level;
    pwm_set_spec_config(&pwm_setting);
	printk("level = %d, clk_src = %d, clk_div = %d, DATA_WIDTH = %d", level, clk_src, clk_div, DATA_WIDTH);
    return 0;
}


static DEFINE_MUTEX(flashlight_gpio_lock);

static int flash_distinguishes()
{
	if ((strcmp(boardid_get(), "S96818AA1") == 0) || (strcmp(boardid_get(), "S96818CA1") == 0)
		|| (strcmp(boardid_get(), "S96818FA1") == 0)) {
		printk("flashlight_ic is aw3641\n");
		return USE_AW3641;
	}
	else if ((strcmp(boardid_get(), "S96818BA1") == 0) || (strcmp(boardid_get(), "S96818EA1") == 0)) {
		printk("flashlight_ic is ocp8111\n");
		return USE_OCP8111;
	}
	else if ((strcmp(boardid_get(), "S96818GA1") == 0) || (strcmp(boardid_get(), "S96818JA1") == 0)) {
		printk("flashlight_ic is aw3641_pvt\n");
		return USE_AW3641_PVT;
	}
	else if ((strcmp(boardid_get(), "S96818HA1") == 0) || (strcmp(boardid_get(), "S96818IA1") == 0)
		|| (strcmp(boardid_get(), "S96818KA1") == 0)) {
		printk("flashlight_ic is ocp8111_pvt\n");
		return USE_OCP8111_PVT;
	}
	else
		return GET_BOARDID_ERR;
}
static int ocp8111_pinctrl_set(int pin, int state)
{
	int ret = 0;
	printk("boardid_get = %s, TorchFlag = %d.", boardid_get(), TorchFlag);
	LOG_INF("+++pin:%d, state:%d. \n", pin, state);
	if (IS_ERR(ocp8111_pinctrl)) {
		pr_err("pinctrl is not available\n");
		return -1;
	}

	mutex_lock(&flashlight_gpio_lock);
	switch (pin) {
	case OCP8111_PINCTRL_PIN_XXX:
		if (state == OCP8111_PINCTRL_PINSTATE_LOW &&
				!IS_ERR(ocp8111_xxx_low)){
			pinctrl_select_state(ocp8111_pinctrl, ocp8111_main_low);
			pinctrl_select_state(ocp8111_pinctrl, ocp8111_xxx_low);
			udelay(500);
		}
		else if (state == OCP8111_PINCTRL_PINSTATE_HIGH &&
				!IS_ERR(ocp8111_xxx_high) && !IS_ERR(flashlight_pwm_en) && (flash_distinguishes() == USE_OCP8111) && TorchFlag == 1){	//torch mode
			printk("flashlights_ocp8111 enter torch mode, light_pwm_value= %d\n", light_pwm_value);
			mt_flashlight_led_set_pwm(0, light_pwm_value, PWM_CLK_OLD_MODE_32K, CLK_DIV128, 100*20);
			ret = pinctrl_select_state(ocp8111_pinctrl, ocp8111_main_low);
			ret = pinctrl_select_state(ocp8111_pinctrl, ocp8111_xxx_high);
			mdelay(6);
			ret = pinctrl_select_state(ocp8111_pinctrl, flashlight_pwm_en); //ENM
		}
		else if (state == OCP8111_PINCTRL_PINSTATE_HIGH &&
				!IS_ERR(ocp8111_xxx_high) && !IS_ERR(flashlight_pwm_aw_en) && (flash_distinguishes() == USE_AW3641) && TorchFlag == 1){	//torch mode
			printk("flashlights_aw3641 enter torch mode, light_pwm_value= %d\n", light_pwm_value);
			mt_flashlight_led_set_pwm(2, light_pwm_value / 20, PWM_CLK_OLD_MODE_BLOCK, CLK_DIV32, 100);
			ret = pinctrl_select_state(ocp8111_pinctrl, ocp8111_main_low);
			ret = pinctrl_select_state(ocp8111_pinctrl, ocp8111_xxx_high);
			mdelay(6);
			ret = pinctrl_select_state(ocp8111_pinctrl, flashlight_pwm_aw_en); //ENM
		}
		else if (state == OCP8111_PINCTRL_PINSTATE_HIGH &&
				!IS_ERR(ocp8111_xxx_high) && !IS_ERR(flashlight_pwm_aw_en) && (flash_distinguishes() == USE_AW3641_PVT) && TorchFlag == 1){	//torch mode
			printk("flashlights_aw3641_PVT enter torch mode, light_pwm_value= %d, flashlight_pwm_en\n", light_pwm_value);
			mt_flashlight_led_set_pwm(0, light_pwm_value, PWM_CLK_OLD_MODE_32K, CLK_DIV128, 100*20);
			ret = pinctrl_select_state(ocp8111_pinctrl, ocp8111_main_low);
			ret = pinctrl_select_state(ocp8111_pinctrl, ocp8111_xxx_high);
			mdelay(6);
			ret = pinctrl_select_state(ocp8111_pinctrl, flashlight_pwm_en); //ENM
		}
		else if (state == OCP8111_PINCTRL_PINSTATE_HIGH &&
				!IS_ERR(ocp8111_xxx_high) && !IS_ERR(flashlight_pwm_aw_en) && (flash_distinguishes() == USE_OCP8111_PVT) && TorchFlag == 1){	//torch mode
			printk("flashlights_ocp8111_PVT enter torch mode, light_pwm_value= %d, flashlight_pwm_en\n", light_pwm_value);
			mt_flashlight_led_set_pwm(0, light_pwm_value, PWM_CLK_OLD_MODE_32K, CLK_DIV128, 100*20);
			ret = pinctrl_select_state(ocp8111_pinctrl, ocp8111_main_low);
			ret = pinctrl_select_state(ocp8111_pinctrl, ocp8111_xxx_high);
			mdelay(6);
			ret = pinctrl_select_state(ocp8111_pinctrl, flashlight_pwm_en); //ENM
		}

		else if (state == OCP8111_PINCTRL_PINSTATE_HIGH &&
				!IS_ERR(ocp8111_xxx_high) && !IS_ERR(flashlight_pwm_aw_en) && TorchFlag == 0){	//torch mode
			printk("flashlights_aw3641 enter torch mode, light_pwm_value= %d\n", light_pwm_value);
			ret = pinctrl_select_state(ocp8111_pinctrl, ocp8111_main_low);
			ret = pinctrl_select_state(ocp8111_pinctrl, ocp8111_xxx_high);
			mdelay(6);
		}	
		else
			pr_err("set err, pin(%d) state(%d) flash_distinguishes(%d)\n", pin, state, flash_distinguishes());
		pwm_flag = 1;
		break;
	case OCP8111_PINCTRL_PIN_MAIN:
		if (state == OCP8111_PINCTRL_MAIN_PINSTATE_LOW &&
				!IS_ERR(ocp8111_main_low)){
				pinctrl_select_state(ocp8111_pinctrl, ocp8111_main_low);
				pinctrl_select_state(ocp8111_pinctrl, ocp8111_xxx_low);
				udelay(500);
				pr_info("flashlights_ocp8111 pwn disable");
		}
		else if (state == OCP8111_PINCTRL_MAIN_PINSTATE_HIGH &&
				!IS_ERR(ocp8111_main_high)){	//flash mode
			printk("flashlights_ocp8111 enter flash mode");
			ret = pinctrl_select_state(ocp8111_pinctrl, ocp8111_main_high);
			ret = pinctrl_select_state(ocp8111_pinctrl, ocp8111_xxx_high);
			mdelay(5);
		}
		else
			pr_err("set err, pin(%d) state(%d)\n", pin, state);
		break;
	default:
		pr_err("set err, pin(%d) state(%d)\n", pin, state);
		break;
	}

	mutex_unlock(&flashlight_gpio_lock);

	pr_debug("pin(%d) state(%d)\n", pin, state);

	return ret;
}

static int ocp8111_pinctrl_video_set(int pin, int state)
{
	int ret = 0;
	printk("boardid_get = %s, TorchFlag = %d.", boardid_get(), TorchFlag);
	LOG_INF("+++pin:%d, state:%d. \n", pin, state);
	if (IS_ERR(ocp8111_pinctrl)) {
		pr_err("pinctrl is not available\n");
		return -1;
	}

	mutex_lock(&flashlight_gpio_lock);
	switch (pin) {
	case OCP8111_PINCTRL_PIN_XXX:
		if (state == OCP8111_PINCTRL_PINSTATE_LOW &&
				!IS_ERR(ocp8111_xxx_low)){
			pinctrl_select_state(ocp8111_pinctrl, ocp8111_main_low);
			pinctrl_select_state(ocp8111_pinctrl, ocp8111_xxx_low);
			udelay(500);
		}
		else if (state == OCP8111_PINCTRL_PINSTATE_HIGH &&
				!IS_ERR(ocp8111_xxx_high) && !IS_ERR(flashlight_pwm_en) && (flash_distinguishes() == USE_OCP8111) && TorchFlag == 1){	//torch mode
			printk("flashlights_ocp8111 enter video mode, light_pwm_value= %d\n", light_pwm_value);
			mt_flashlight_led_set_pwm(0, light_pwm_value, PWM_CLK_OLD_MODE_32K, CLK_DIV128, 100);
			ret = pinctrl_select_state(ocp8111_pinctrl, ocp8111_main_low);
			ret = pinctrl_select_state(ocp8111_pinctrl, ocp8111_xxx_high);
			mdelay(6);
			ret = pinctrl_select_state(ocp8111_pinctrl, flashlight_pwm_en); //ENM
		}
		else if (state == OCP8111_PINCTRL_PINSTATE_HIGH &&
				!IS_ERR(ocp8111_xxx_high) && !IS_ERR(flashlight_pwm_aw_en) && (flash_distinguishes() == USE_AW3641) && TorchFlag == 1){	//torch mode
			printk("flashlights_aw3641 enter video mode, light_pwm_value= %d\n", light_pwm_value);
			mt_flashlight_led_set_pwm(2, light_pwm_value, PWM_CLK_OLD_MODE_BLOCK, CLK_DIV32, 100);
			ret = pinctrl_select_state(ocp8111_pinctrl, ocp8111_main_low);
			ret = pinctrl_select_state(ocp8111_pinctrl, ocp8111_xxx_high);
			mdelay(6);
			ret = pinctrl_select_state(ocp8111_pinctrl, flashlight_pwm_aw_en); //ENM
		}
		else if (state == OCP8111_PINCTRL_PINSTATE_HIGH &&
				!IS_ERR(ocp8111_xxx_high) && !IS_ERR(flashlight_pwm_aw_en) && (flash_distinguishes() == USE_AW3641_PVT) && TorchFlag == 1){	//torch mode
			printk("flashlights_aw3641_PVT enter video mode, light_pwm_value= %d, flashlight_pwm_en\n", light_pwm_value);
			mt_flashlight_led_set_pwm(0, light_pwm_value, PWM_CLK_OLD_MODE_32K, CLK_DIV128, 150);
			ret = pinctrl_select_state(ocp8111_pinctrl, ocp8111_main_low);
			ret = pinctrl_select_state(ocp8111_pinctrl, ocp8111_xxx_high);
			mdelay(6);
			ret = pinctrl_select_state(ocp8111_pinctrl, flashlight_pwm_en); //ENM
		}
		else if (state == OCP8111_PINCTRL_PINSTATE_HIGH &&
				!IS_ERR(ocp8111_xxx_high) && !IS_ERR(flashlight_pwm_aw_en) && (flash_distinguishes() == USE_OCP8111_PVT) && TorchFlag == 1){	//torch mode
			printk("flashlights_ocp8111_PVT enter video mode, light_pwm_value= %d, flashlight_pwm_en\n", light_pwm_value);
			mt_flashlight_led_set_pwm(0, light_pwm_value, PWM_CLK_OLD_MODE_32K, CLK_DIV128, 100);
			ret = pinctrl_select_state(ocp8111_pinctrl, ocp8111_main_low);
			ret = pinctrl_select_state(ocp8111_pinctrl, ocp8111_xxx_high);
			mdelay(6);
			ret = pinctrl_select_state(ocp8111_pinctrl, flashlight_pwm_en); //ENM
		}
		else if (state == OCP8111_PINCTRL_PINSTATE_HIGH &&
				!IS_ERR(ocp8111_xxx_high) && !IS_ERR(flashlight_pwm_aw_en) && TorchFlag == 0){	//torch mode
			printk("flashlights_aw3641 enter video mode, light_pwm_value= %d\n", light_pwm_value);
			ret = pinctrl_select_state(ocp8111_pinctrl, ocp8111_main_low);
			ret = pinctrl_select_state(ocp8111_pinctrl, ocp8111_xxx_high);
			mdelay(6);
		}
		else
			pr_err("set err, pin(%d) state(%d) flash_distinguishes(%d)\n", pin, state, flash_distinguishes());
		break;
	case OCP8111_PINCTRL_PIN_MAIN:
		if (state == OCP8111_PINCTRL_MAIN_PINSTATE_LOW &&
				!IS_ERR(ocp8111_main_low)){
				pinctrl_select_state(ocp8111_pinctrl, ocp8111_main_low);
				pinctrl_select_state(ocp8111_pinctrl, ocp8111_xxx_low);
				udelay(500);
				pr_info("flashlights_ocp8111 pwn disable");
		}
		else if (state == OCP8111_PINCTRL_MAIN_PINSTATE_HIGH &&
				!IS_ERR(ocp8111_main_high)){	//flash mode
			printk("flashlights_ocp8111 enter flash mode");
			ret = pinctrl_select_state(ocp8111_pinctrl, ocp8111_main_high);
			ret = pinctrl_select_state(ocp8111_pinctrl, ocp8111_xxx_high);
			mdelay(5);
		}
		else
			pr_err("set err, pin(%d) state(%d)\n", pin, state);
		break;
	default:
		pr_err("set err, pin(%d) state(%d)\n", pin, state);
		break;
	}

	mutex_unlock(&flashlight_gpio_lock);

	pr_debug("pin(%d) state(%d)\n", pin, state);

	return ret;
}


/******************************************************************************
 * dummy operations
 *****************************************************************************/
/* flashlight enable function */
static int ocp8111_enable(void)
{
	//int pin = 0, state = 1;

	/* TODO: wrap enable function */
	LOG_ERR("mainFlashFlag:%d.\n", mainFlashFlag);
	if (mainFlashFlag) {
		ocp8111_pinctrl_set(OCP8111_PINCTRL_PIN_MAIN, OCP8111_PINCTRL_MAIN_PINSTATE_HIGH);		//flash mode
		//ocp8111_pinctrl_set(OCP8111_PINCTRL_PIN_XXX, OCP8111_PINCTRL_PINSTATE_LOW);
		TorchFlag = 1;
	} else {
		//ocp8111_pinctrl_set(OCP8111_PINCTRL_PIN_MAIN, OCP8111_PINCTRL_MAIN_PINSTATE_LOW);
		//+EXTB P211011-02914,zhanghao2.wt,modify,2021/10/12,fix press home key torch has flash
		//+S96818AA1-1936,chenming01.wt,ADD,2023/08/02, camera switch distinguishes the flash PWM frequency
		if ((g_torchWaitLock == 0 || hs_video_flag) && camera_switch == 1) {
			enable_torch_wakelock(); //bug 682590, zhanghao2.wt, ADD, 2021/9/28, fix flashlight close screen and torch had closed.
			if (flash_distinguishes() == USE_AW3641) {
				light_pwm_value = 0;
				printk("flashlights_aw3641 set torch mode light_pwm_value = %d\n", light_pwm_value);
			}
			else if (flash_distinguishes() == USE_OCP8111){
				light_pwm_value = 70;
				printk("flashlights_ocp8111 set torch mode light_pwm_value = %d\n", light_pwm_value);
			}
			else if (flash_distinguishes() == USE_OCP8111_PVT){
				light_pwm_value = 70;
				printk("flashlights_ocp8111_pvt set torch mode light_pwm_value = %d\n", light_pwm_value);
			}
			else if (flash_distinguishes() == USE_AW3641_PVT){
				light_pwm_value = 120;
				printk("flashlights_ocp8111_pvt set torch mode light_pwm_value = %d\n", light_pwm_value);
			}
			if (!pwm_flag)
				ocp8111_pinctrl_video_set(OCP8111_PINCTRL_PIN_XXX, OCP8111_PINCTRL_PINSTATE_HIGH);//torch mode
		} else {
			enable_torch_wakelock(); //bug 682590, zhanghao2.wt, ADD, 2021/9/28, fix flashlight close screen and torch had closed.
			if (flash_distinguishes() == USE_AW3641) {
				light_pwm_value = 0*20;
				printk("flashlights_aw3641 set torch mode light_pwm_value = %d\n", light_pwm_value);
			}
			else if (flash_distinguishes() == USE_OCP8111){
				light_pwm_value = 44*20;
				printk("flashlights_ocp8111 set torch mode light_pwm_value = %d\n", light_pwm_value);
			}
			else if (flash_distinguishes() == USE_OCP8111_PVT){
				light_pwm_value = 48*20;
				printk("flashlights_ocp8111_pvt set torch mode light_pwm_value = %d\n", light_pwm_value);
			}
			else if (flash_distinguishes() == USE_AW3641_PVT){
				light_pwm_value = 50*20;
				printk("flashlights_ocp8111_pvt set torch mode light_pwm_value = %d\n", light_pwm_value);
			}
				ocp8111_pinctrl_set(OCP8111_PINCTRL_PIN_XXX, OCP8111_PINCTRL_PINSTATE_HIGH);

		}
		//-S96818AA1-1936,chenming01.wt,ADD,2023/08/02, camera switch distinguishes the flash PWM frequency
		//-EXTB P211011-02914,zhanghao2.wt,modify,2021/10/12,fix press home key torch has flash
	}
	pwm_flag = 1;
	return 0;
}

/* flashlight disable function */
static int ocp8111_disable(void)
{
	//int pin = 0, state = 0;

	/* TODO: wrap disable function */
	//+bug 682590, zhanghao2.wt, ADD, 2021/9/28, fix flashlight close screen and torch had closed.
	LOG_ERR("disable.\n");
	diable_torch_wakelock();
	//-bug 682590, zhanghao2.wt, ADD, 2021/9/28, fix flashlight close screen and torch had closed.
	ocp8111_pinctrl_set(OCP8111_PINCTRL_PIN_MAIN, OCP8111_PINCTRL_MAIN_PINSTATE_LOW);
	pwm_flag = 0;
	return 0;
}

/* set flashlight level */
static int ocp8111_set_level(int level)
{
	//int pin = 0, state = 0;

	/* TODO: wrap set level function */

	//return ocp8111_pinctrl_set(pin, state);
	mainFlashFlag  = 1;
    if (level == 1) {
        mainFlashFlag = 1;
		TorchFlag = 0;
    }
    else if (level == 0) {
		mainFlashFlag  = 0;
		TorchFlag = 0;
	} else if (level == 6) {
		mainFlashFlag  = 0;
		TorchFlag = 1;
	}

    printk("level:%d, mainFlashFlag:%d,light_pwm_value=%d.\n", level, mainFlashFlag,light_pwm_value);
	return 0;
}

/* flashlight init */
static int ocp8111_init(void)
{
	//int pin = 0, state = 0;

	/* TODO: wrap init function */

	//return ocp8111_pinctrl_set(pin, state);
	return 0;
}

/* flashlight uninit */
static int ocp8111_uninit(void)
{
	//int pin = 0, state = 0;

	/* TODO: wrap uninit function */

	//return ocp8111_pinctrl_set(pin, state);
	return 0;
}

/******************************************************************************
 * Timer and work queue
 *****************************************************************************/
static struct hrtimer ocp8111_timer;
static unsigned int ocp8111_timeout_ms;

static void ocp8111_work_disable(struct work_struct *data)
{
	pr_debug("work queue callback\n");
	ocp8111_disable();
}

static enum hrtimer_restart ocp8111_timer_func(struct hrtimer *timer)
{
	schedule_work(&ocp8111_work);
	return HRTIMER_NORESTART;
}


/******************************************************************************
 * Flashlight operations
 *****************************************************************************/
static int ocp8111_ioctl(unsigned int cmd, unsigned long arg)
{
	struct flashlight_dev_arg *fl_arg;
	int channel;
	//ktime_t ktime;
	//unsigned int s;
	//unsigned int ns;

	fl_arg = (struct flashlight_dev_arg *)arg;
	channel = fl_arg->channel;

	switch (cmd) {
	case FLASH_IOC_SET_TIME_OUT_TIME_MS:
		LOG_INF("FLASH_IOC_SET_TIME_OUT_TIME_MS(%d): %d\n",
				channel, (int)fl_arg->arg);
		ocp8111_timeout_ms = fl_arg->arg;
		break;

	case FLASH_IOC_SET_DUTY:
		LOG_INF("FLASH_IOC_SET_DUTY(%d): %d\n",
				channel, (int)fl_arg->arg);
		ocp8111_set_level(fl_arg->arg);
		break;

	case FLASH_IOC_SET_ONOFF:
		LOG_INF("FLASH_IOC_SET_ONOFF(%d): %d\n",
				channel, (int)fl_arg->arg);
		if (fl_arg->arg == 1) {
			/*
			if (ocp8111_timeout_ms) {
				s = ocp8111_timeout_ms / 1000;
				ns = ocp8111_timeout_ms % 1000 * 1000000;
				ktime = ktime_set(s, ns);
				hrtimer_start(&ocp8111_timer, ktime,
						HRTIMER_MODE_REL);
			}*/
			ocp8111_enable();
		} else {
			ocp8111_disable();
			//hrtimer_cancel(&ocp8111_timer);
		}
		break;
	default:
		LOG_INF("No such command and arg(%d): (%d, %d)\n",
				channel, _IOC_NR(cmd), (int)fl_arg->arg);
		return -ENOTTY;
	}

	return 0;
}

static int ocp8111_open(void)
{
	/* Move to set driver for saving power */
	return 0;
}

static int ocp8111_release(void)
{
	/* Move to set driver for saving power */
	return 0;
}

static int ocp8111_set_driver(int set)
{
	int ret = 0;

	/* set chip and usage count */
	mutex_lock(&ocp8111_mutex);
	if (set) {
		if (!use_count)
			ret = ocp8111_init();
		use_count++;
		pr_debug("Set driver: %d\n", use_count);
	} else {
		use_count--;
		if (!use_count)
			ret = ocp8111_uninit();
		if (use_count < 0)
			use_count = 0;
		pr_debug("Unset driver: %d\n", use_count);
	}
	mutex_unlock(&ocp8111_mutex);

	return ret;
}

static ssize_t ocp8111_strobe_store(struct flashlight_arg arg)
{
	ocp8111_set_driver(1);
	ocp8111_set_level(arg.level);
	ocp8111_timeout_ms = 0;
	ocp8111_enable();
	msleep(arg.dur);
	ocp8111_disable();
	ocp8111_set_driver(0);

	return 0;
}

static struct flashlight_operations ocp8111_ops = {
	ocp8111_open,
	ocp8111_release,
	ocp8111_ioctl,
	ocp8111_strobe_store,
	ocp8111_set_driver
};


/******************************************************************************
 * Platform device and driver
 *****************************************************************************/
static int ocp8111_chip_init(void)
{
	/* NOTE: Chip initialication move to "set driver" for power saving.
	 * ocp8111_init();
	 */

	return 0;
}

static int ocp8111_parse_dt(struct device *dev,
		struct ocp8111_platform_data *pdata)
{
	return 0;
}

//+bug 682590, zhanghao2.wt, ADD, 2021/9/24, add ss flashlight-node pwm light control.
#include <linux/cdev.h>

#define WT_FLASHLIGHT_DEVNAME            "flash"
static dev_t wt_flashlight_devno;
static struct cdev *wt_flashlight_cdev;
static struct class *wt_flashlight_class;
static struct device *wt_flashlight_device;
static unsigned long flashduty1;

static const struct file_operations wt_flashlight_fops = {
    .owner = THIS_MODULE,
    .unlocked_ioctl = NULL,
    .open = NULL,
    .release = NULL,
#ifdef CONFIG_COMPAT
    .compat_ioctl = NULL,
#endif
};

static ssize_t show_flashduty1(struct device *dev, struct device_attribute *attr, char *buf)
{
    pr_info("[LED]get backlight duty value is:%ld\n", flashduty1); //bug 694907, zhanghao2.wt, ADD, 2021/9/29, fix Invalid type in argument to printf format specifier .
    return sprintf(buf, "%d\n", flashduty1);
}

static ssize_t store_flashduty1(struct device *dev, struct device_attribute *attr, const char *buf, size_t count)
{
	int err;
	pr_info("Enter!\n");
	err = kstrtoul(buf, 10, &flashduty1);
	if(err != 0){
		return err;
	}
    LOG_INF("ss-torch:set torch level,flashduty1= %d\n", flashduty1);
	switch(flashduty1){
		case 1001:
			if (flash_distinguishes() == USE_AW3641) {
				light_pwm_value = 2*20;
				mt_flashlight_led_set_pwm(2, light_pwm_value / 20, PWM_CLK_OLD_MODE_BLOCK, CLK_DIV32, 100);
				printk("flashlights_aw3641 set SS torch mode light_pwm_value = %d\n", light_pwm_value);
			}
			else if (flash_distinguishes() == USE_OCP8111) {
				light_pwm_value = 50*20;
				mt_flashlight_led_set_pwm(0, light_pwm_value, PWM_CLK_OLD_MODE_32K, CLK_DIV128, 100*20);
				printk("flashlights_ocp8111 set SS torch mode light_pwm_value = %d\n", light_pwm_value);
			}
			else if (flash_distinguishes() == USE_AW3641_PVT) {
				light_pwm_value = 29*20;
				mt_flashlight_led_set_pwm(0, light_pwm_value, PWM_CLK_OLD_MODE_32K, CLK_DIV128, 100*20);
				printk("PVT_flashlights_aw3641 set SS torch mode light_pwm_value = %d\n", light_pwm_value);
			}
			else if (flash_distinguishes() == USE_OCP8111_PVT) {
				light_pwm_value = 25*20;
				mt_flashlight_led_set_pwm(0, light_pwm_value, PWM_CLK_OLD_MODE_32K, CLK_DIV128, 100*20);
				printk("PVT_flashlights_ocp8111 set SS torch mode light_pwm_value = %d\n", light_pwm_value);
			}

			if (!pwm_flag)
				ocp8111_pinctrl_set(OCP8111_PINCTRL_PIN_XXX, OCP8111_PINCTRL_PINSTATE_HIGH);
			break;

		case 1002:
			if (flash_distinguishes() == USE_AW3641) {
				light_pwm_value = 4*20;
				mt_flashlight_led_set_pwm(2, light_pwm_value / 20, PWM_CLK_OLD_MODE_BLOCK, CLK_DIV32, 100);
				printk("flashlights_aw3641 set SS torch mode light_pwm_value = %d\n", light_pwm_value);
			}
			else if (flash_distinguishes() == USE_OCP8111) {
				light_pwm_value = 58*20;
				mt_flashlight_led_set_pwm(0, light_pwm_value, PWM_CLK_OLD_MODE_32K, CLK_DIV128, 100*20);
				printk("flashlights_ocp8111 set SS torch mode light_pwm_value = %d\n", light_pwm_value);
			}
			else if (flash_distinguishes() == USE_AW3641_PVT) {
				light_pwm_value = 39*20;
				mt_flashlight_led_set_pwm(0, light_pwm_value, PWM_CLK_OLD_MODE_32K, CLK_DIV128, 100*20);
				printk("PVT_flashlights_aw3641 set SS torch mode light_pwm_value = %d\n", light_pwm_value);
			}
			else if (flash_distinguishes() == USE_OCP8111_PVT) {
				light_pwm_value = 34*20;
				mt_flashlight_led_set_pwm(0, light_pwm_value, PWM_CLK_OLD_MODE_32K, CLK_DIV128, 100*20);
				printk("PVT_flashlights_ocp8111 set SS torch mode light_pwm_value = %d\n", light_pwm_value);
			}

			if (!pwm_flag)
				ocp8111_pinctrl_set(OCP8111_PINCTRL_PIN_XXX, OCP8111_PINCTRL_PINSTATE_HIGH);
			break;

		case 1003:
			if (flash_distinguishes() == USE_AW3641) {
				light_pwm_value = 6*20;
				mt_flashlight_led_set_pwm(2, light_pwm_value / 20, PWM_CLK_OLD_MODE_BLOCK, CLK_DIV32, 100);
				printk("flashlights_aw3641 set SS torch mode light_pwm_value = %d\n", light_pwm_value);
			}
			else if (flash_distinguishes() == USE_OCP8111) {
				light_pwm_value = 67*20;
				mt_flashlight_led_set_pwm(0, light_pwm_value, PWM_CLK_OLD_MODE_32K, CLK_DIV128, 100*20);
				printk("flashlights_ocp8111 set SS torch mode light_pwm_value = %d\n", light_pwm_value);
			}
			else if (flash_distinguishes() == USE_AW3641_PVT) {
				light_pwm_value = 49*20;
				mt_flashlight_led_set_pwm(0, light_pwm_value, PWM_CLK_OLD_MODE_32K, CLK_DIV128, 100*20);
				printk("PVT_flashlights_aw3641 set SS torch mode light_pwm_value = %d\n", light_pwm_value);
			}
			else if (flash_distinguishes() == USE_OCP8111_PVT) {
				light_pwm_value = 42*20;
				mt_flashlight_led_set_pwm(0, light_pwm_value, PWM_CLK_OLD_MODE_32K, CLK_DIV128, 100*20);
				printk("PVT_flashlights_ocp8111 set SS torch mode light_pwm_value = %d\n", light_pwm_value);
			}

			if (!pwm_flag)
				ocp8111_pinctrl_set(OCP8111_PINCTRL_PIN_XXX, OCP8111_PINCTRL_PINSTATE_HIGH);
			break;

		case 1005:
			if (flash_distinguishes() == USE_AW3641) {
				light_pwm_value = 8*20;
				mt_flashlight_led_set_pwm(2, light_pwm_value / 20, PWM_CLK_OLD_MODE_BLOCK, CLK_DIV32, 100);
				printk("flashlights_aw3641 set SS torch mode light_pwm_value = %d\n", light_pwm_value);
			}
			else if (flash_distinguishes() == USE_OCP8111) {
				light_pwm_value = 76*20;
				mt_flashlight_led_set_pwm(0, light_pwm_value, PWM_CLK_OLD_MODE_32K, CLK_DIV128, 100*20);
				printk("flashlights_ocp8111 set SS torch mode light_pwm_value = %d\n", light_pwm_value);
			}
			else if (flash_distinguishes() == USE_AW3641_PVT) {
				light_pwm_value = 59*20;
				mt_flashlight_led_set_pwm(0, light_pwm_value, PWM_CLK_OLD_MODE_32K, CLK_DIV128, 100*20);
				printk("PVT_flashlights_aw3641 set SS torch mode light_pwm_value = %d\n", light_pwm_value);
			}
			else if (flash_distinguishes() == USE_OCP8111_PVT) {
				light_pwm_value = 52*20;
				mt_flashlight_led_set_pwm(0, light_pwm_value, PWM_CLK_OLD_MODE_32K, CLK_DIV128, 100*20);
				printk("PVT_flashlights_ocp8111 set SS torch mode light_pwm_value = %d\n", light_pwm_value);
			}

			if (!pwm_flag)
				ocp8111_pinctrl_set(OCP8111_PINCTRL_PIN_XXX, OCP8111_PINCTRL_PINSTATE_HIGH);
			break;

		case 1007:
			if (flash_distinguishes() == USE_AW3641) {
				light_pwm_value = 10*20;
				mt_flashlight_led_set_pwm(2, light_pwm_value / 20, PWM_CLK_OLD_MODE_BLOCK, CLK_DIV32, 100);
				printk("flashlights_aw3641 set SS torch mode light_pwm_value = %d\n", light_pwm_value);
			}
			else if (flash_distinguishes() == USE_OCP8111) {
				light_pwm_value = 85*20;
				mt_flashlight_led_set_pwm(0, light_pwm_value, PWM_CLK_OLD_MODE_32K, CLK_DIV128, 100*20);
				printk("flashlights_ocp8111 set SS torch mode light_pwm_value = %d\n", light_pwm_value);
			}
			else if (flash_distinguishes() == USE_AW3641_PVT) {
				light_pwm_value = 69*20;
				mt_flashlight_led_set_pwm(0, light_pwm_value, PWM_CLK_OLD_MODE_32K, CLK_DIV128, 100*20);
				printk("PVT_flashlights_aw3641 set SS torch mode light_pwm_value = %d\n", light_pwm_value);
			}
			else if (flash_distinguishes() == USE_OCP8111_PVT) {
				light_pwm_value = 62*20;
				mt_flashlight_led_set_pwm(0, light_pwm_value, PWM_CLK_OLD_MODE_32K, CLK_DIV128, 100*20);
				printk("PVT_flashlights_ocp8111 set SS torch mode light_pwm_value = %d\n", light_pwm_value);
			}

			if (!pwm_flag)
				ocp8111_pinctrl_set(OCP8111_PINCTRL_PIN_XXX, OCP8111_PINCTRL_PINSTATE_HIGH);
			break;

		default:
			if (flash_distinguishes() == USE_AW3641) {
				light_pwm_value = 5*20;
				mt_flashlight_led_set_pwm(2, light_pwm_value / 20, PWM_CLK_OLD_MODE_BLOCK, CLK_DIV32, 100);
				printk("flashlights_aw3641 set SS torch mode light_pwm_value = %d\n", light_pwm_value);
			}
			else if (flash_distinguishes() == USE_OCP8111) {
				light_pwm_value = 63*20;
				mt_flashlight_led_set_pwm(0, light_pwm_value, PWM_CLK_OLD_MODE_32K, CLK_DIV128, 100*20);
				printk("flashlights_ocp8111 set SS torch mode light_pwm_value = %d\n", light_pwm_value);
			}
			else if (flash_distinguishes() == USE_AW3641_PVT) {
				light_pwm_value = 50*20;
				mt_flashlight_led_set_pwm(0, light_pwm_value, PWM_CLK_OLD_MODE_32K, CLK_DIV128, 100*20);
				printk("PVT_flashlights_aw3641 set SS torch mode light_pwm_value = %d\n", light_pwm_value);
			}
			else if (flash_distinguishes() == USE_OCP8111_PVT) {
				light_pwm_value = 41*20;
				mt_flashlight_led_set_pwm(0, light_pwm_value, PWM_CLK_OLD_MODE_32K, CLK_DIV128, 100*20);
				printk("PVT_flashlights_ocp8111 set SS torch mode light_pwm_value = %d\n", light_pwm_value);
			}

			if (!pwm_flag)
				ocp8111_pinctrl_set(OCP8111_PINCTRL_PIN_XXX, OCP8111_PINCTRL_PINSTATE_HIGH);
			break;
		}
	pr_info("Exit!\n");
	return count;
}

static DEVICE_ATTR(rear_flash, 0664, show_flashduty1, store_flashduty1);


static int ss_flashlight_node_create(void)
{
	// create node /sys/class/camera/flash/rear_flash
	if (alloc_chrdev_region(&wt_flashlight_devno, 0, 1, WT_FLASHLIGHT_DEVNAME)) {
		pr_err("[flashlight_probe] alloc_chrdev_region fail~");
	} else {
		pr_err("[flashlight_probe] major: %d, minor: %d ~", MAJOR(wt_flashlight_devno),
			MINOR(wt_flashlight_devno));
	}

	wt_flashlight_cdev = cdev_alloc();
	if (!wt_flashlight_cdev) {
		pr_err("[flashlight_probe] Failed to allcoate cdev\n");
	}
	wt_flashlight_cdev->ops = &wt_flashlight_fops;
	wt_flashlight_cdev->owner = THIS_MODULE;
	if (cdev_add(wt_flashlight_cdev, wt_flashlight_devno, 1)) {
		pr_err("[flashlight_probe] cdev_add fail ~" );
	}
	wt_flashlight_class = class_create(THIS_MODULE, "camera");   //  /sys/class/camera
	if (IS_ERR(wt_flashlight_class)) {
		pr_err("[flashlight_probe] Unable to create class, err = %d ~",
			(int)PTR_ERR(wt_flashlight_class));
		return -1 ;
	}
	wt_flashlight_device =
		device_create(wt_flashlight_class, NULL, wt_flashlight_devno, NULL, WT_FLASHLIGHT_DEVNAME);  //   /sys/class/camera/flash
	if (NULL == wt_flashlight_device) {
		pr_err("[flashlight_probe] device_create fail ~");
	}
	if (device_create_file(wt_flashlight_device,&dev_attr_rear_flash)) { // /sys/class/camera/flash/rear_flash
		pr_err("[flashlight_probe]device_create_file flash1 fail!\n");
	}
	return 0;
}
//-bug 682590, zhanghao2.wt, ADD, 2021/9/24, add ss flashlight-node pwm light control.

static ssize_t led_flash_show(struct device *dev, struct device_attribute *attr, char *buf){
    return sprintf(buf, "%d\n", 0);
}
static ssize_t led_flash_store(struct device *dev, struct device_attribute *attr, const char *buf, size_t size){
	unsigned long value;
	int err;
	err = kstrtoul(buf, 10, &value);
	if(err != 0){
		return err;
	}
        LOG_INF("value:%ld. \n", value);
	switch(value){
		case 0: //off
                        err = ocp8111_disable();
			if(err < 0)
				LOG_ERR("AAA - error1 - AAA\n");
			break;
		case 1: //on
			mainFlashFlag  = 0;
                        err = ocp8111_enable();
			if(err < 0)
				LOG_ERR("AAA - error2 - AAA\n");
			break;
		default :
			LOG_ERR("AAA - error3 - AAA\n");
			break;
	}
	return 1;
}
static DEVICE_ATTR(led_flash, 0664, led_flash_show, led_flash_store);


static int ocp8111_probe(struct platform_device *pdev)
{
	struct ocp8111_platform_data *pdata = dev_get_platdata(&pdev->dev);
	int err, ret;
	int i;

	LOG_INF("Probe start.\n");

	/* init pinctrl */
	if (ocp8111_pinctrl_init(pdev)) {
		pr_debug("Failed to init pinctrl.\n");
		err = -EFAULT;
		goto err;
	}

	/* init platform data */
	if (!pdata) {
		pdata = devm_kzalloc(&pdev->dev, sizeof(*pdata), GFP_KERNEL);
		if (!pdata) {
			err = -ENOMEM;
			goto err;
		}
		pdev->dev.platform_data = pdata;
		err = ocp8111_parse_dt(&pdev->dev, pdata);
		if (err)
			goto err;
	}

	/* init work queue */
	INIT_WORK(&ocp8111_work, ocp8111_work_disable);

	/* init timer */
	hrtimer_init(&ocp8111_timer, CLOCK_MONOTONIC, HRTIMER_MODE_REL);
	ocp8111_timer.function = ocp8111_timer_func;
	ocp8111_timeout_ms = 100;

	/* init chip hw */
	ocp8111_chip_init();

	/* clear usage count */
	use_count = 0;

	/* register flashlight device */
	if (pdata->channel_num) {
		for (i = 0; i < pdata->channel_num; i++)
			if (flashlight_dev_register_by_device_id(
						&pdata->dev_id[i],
						&ocp8111_ops)) {
				err = -EFAULT;
				goto err;
			}
	} else {
		if (flashlight_dev_register(OCP8111_NAME, &ocp8111_ops)) {
			err = -EFAULT;
			goto err;
		}
	}
	//add file node.
	ret = device_create_file(&pdev->dev, &dev_attr_led_flash);
	if(ret < 0){
		pr_err("=== create led_flash_node file failed ===\n");
	}

	if (ss_flashlight_node_create() < 0){
		pr_err( "ss_flashlight_node_create failed!\n");
	}

//+bug 682590, zhanghao2.wt, ADD, 2021/9/28, fix flashlight close screen and torch had closed.
#ifdef CONFIG_PM_WAKELOCKS
	torch_wake_lock = wakeup_source_register(NULL, "torch_lock_wakelock");
#else
	wake_lock_init(torch_wake_lock, WAKE_LOCK_SUSPEND, "torch_lock_wakelock");
#endif
//-bug 682590, zhanghao2.wt, ADD, 2021/9/28, fix flashlight close screen and torch had closed.

	LOG_INF("Probe done.\n");

	return 0;
err:
	return err;
}

static int ocp8111_remove(struct platform_device *pdev)
{
	struct ocp8111_platform_data *pdata = dev_get_platdata(&pdev->dev);
	int i;

	pr_debug("Remove start.\n");

	pdev->dev.platform_data = NULL;

	/* unregister flashlight device */
	if (pdata && pdata->channel_num)
		for (i = 0; i < pdata->channel_num; i++)
			flashlight_dev_unregister_by_device_id(
					&pdata->dev_id[i]);
	else
		flashlight_dev_unregister(OCP8111_NAME);

	/* flush work queue */
	flush_work(&ocp8111_work);

	pr_debug("Remove done.\n");

	return 0;
}

#ifdef CONFIG_OF
static const struct of_device_id ocp8111_gpio_of_match[] = {
	{.compatible = OCP8111_GPIO_DTNAME},
	{},
};
MODULE_DEVICE_TABLE(of, ocp8111_gpio_of_match);
#else
static struct platform_device ocp8111_gpio_platform_device[] = {
	{
		.name = OCP8111_NAME,
		.id = 0,
		.dev = {}
	},
	{}
};
MODULE_DEVICE_TABLE(platform, ocp8111_gpio_platform_device);
#endif

static struct platform_driver ocp8111_platform_driver = {
	.probe = ocp8111_probe,
	.remove = ocp8111_remove,
	.driver = {
		.name = OCP8111_NAME,
		.owner = THIS_MODULE,
#ifdef CONFIG_OF
		.of_match_table = ocp8111_gpio_of_match,
#endif
	},
};

static int __init flashlight_ocp8111_init(void)
{
	int ret;

	LOG_INF("Init start.\n");

#ifndef CONFIG_OF
	ret = platform_device_register(&ocp8111_gpio_platform_device);
	if (ret) {
		pr_err("Failed to register platform device\n");
		return ret;
	}
#endif

	ret = platform_driver_register(&ocp8111_platform_driver);
	if (ret) {
		pr_err("Failed to register platform driver\n");
		return ret;
	}

	LOG_INF("Init done.\n");

	return 0;
}

static void __exit flashlight_ocp8111_exit(void)
{
	pr_debug("Exit start.\n");

	platform_driver_unregister(&ocp8111_platform_driver);

	pr_debug("Exit done.\n");
}

late_initcall(flashlight_ocp8111_init);
module_exit(flashlight_ocp8111_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Chris<chenshiqiang@wingtechcom>");
MODULE_DESCRIPTION("MTK Flashlight OCP8111 Driver");
