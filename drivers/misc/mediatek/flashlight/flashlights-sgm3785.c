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

#ifdef CONFIG_MTK_PWM
#include <mt-plat/mtk_pwm.h>
#endif

#undef LOG_INF
#define FLASHLIGHTS_SGM3785_DEBUG 1
#if FLASHLIGHTS_SGM3785_DEBUG
#define LOG_INF(format, args...) pr_info("flashLight-sgm3785" "[%s]"  format, __func__,  ##args)
#else
#define LOG_INF(format, args...)
#endif
#define LOG_ERR(format, args...) pr_err("flashLight-sgm3785" "[%s]" format, __func__, ##args)

//+bug 682590, zhanghao2.wt, ADD, 2021/9/28, fix flashlight close screen and torch had closed.
#ifdef CONFIG_PM_WAKELOCKS
#include <linux/pm_wakeup.h>
#else
#include <linux/wakelock.h>
#endif

#ifdef CONFIG_PM_WAKELOCKS
struct wakeup_source *torch_wake_lock;
#else
struct wake_lock *torch_wake_lock;
#endif

static volatile int g_torchWaitLock;
//-bug 682590, zhanghao2.wt, ADD, 2021/9/28, fix flashlight close screen and torch had closed.

/* define device tree */
/* TODO: modify temp device tree name */
#ifndef SGM3785_GPIO_DTNAME
#ifdef CONFIG_MTK_96516_CAMERA
#define SGM3785_GPIO_DTNAME "mediatek,flashlights_ocp8132a"
#else
#define SGM3785_GPIO_DTNAME "mediatek,flashlights_sgm3785"
#endif
#endif

/* TODO: define driver name */
#define SGM3785_NAME "flashlights-sgm3785"

/* define registers */
/* TODO: define register */

/* define mutex and work queue */
static DEFINE_MUTEX(sgm3785_mutex);
static struct work_struct sgm3785_work;
static int mainFlashFlag  = 0;

/* define pinctrl */
/* TODO: define pinctrl */
#define SGM3785_PINCTRL_PIN_XXX 0
#define SGM3785_PINCTRL_PINSTATE_LOW 0
#define SGM3785_PINCTRL_PINSTATE_HIGH 1
#define SGM3785_PINCTRL_STATE_XXX_HIGH "flashlights_on"
#define SGM3785_PINCTRL_STATE_XXX_LOW  "flashlights_off"
static struct pinctrl *sgm3785_pinctrl;
static struct pinctrl_state *sgm3785_xxx_high;
static struct pinctrl_state *sgm3785_xxx_low;

#define SGM3785_PINCTRL_PIN_MAIN 1
#define SGM3785_PINCTRL_MAIN_PINSTATE_LOW 0
#define SGM3785_PINCTRL_MAIN_PINSTATE_HIGH 1
#define SGM3785_PINCTRL_STATE_MAIN_HIGH "flashlights_on_main"
#define SGM3785_PINCTRL_STATE_MAIN_LOW  "flashlights_off_main"
static struct pinctrl_state *sgm3785_main_high;
static struct pinctrl_state *sgm3785_main_low;

#define SGM3785_PINCTRL_PIN_PWM_EN "flashlights_pwm_pin"
static struct pinctrl_state *flashlight_pwm_en;
static int light_pwm_value;




/* define usage count */
static int use_count;

/* platform data */
struct sgm3785_platform_data {
	int channel_num;
	struct flashlight_device_id *dev_id;
};

//+bug 682590, zhanghao2.wt, ADD, 2021/9/28, fix flashlight close screen and torch had closed.
void enable_torch_wakelock(void)
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

void diable_torch_wakelock(void)
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
//-bug 682590, zhanghao2.wt, ADD, 2021/9/28, fix flashlight close screen and torch had closed.

/******************************************************************************
 * Pinctrl configuration
 *****************************************************************************/
static int sgm3785_pinctrl_init(struct platform_device *pdev)
{
	int ret = 0;

	/* get pinctrl */
	sgm3785_pinctrl = devm_pinctrl_get(&pdev->dev);
	if (IS_ERR(sgm3785_pinctrl)) {
		pr_err("Failed to get flashlight pinctrl.\n");
		ret = PTR_ERR(sgm3785_pinctrl);
		return ret;
	}

	/* TODO: Flashlight XXX pin initialization */
	sgm3785_xxx_high = pinctrl_lookup_state(
			sgm3785_pinctrl, SGM3785_PINCTRL_STATE_XXX_HIGH);
	if (IS_ERR(sgm3785_xxx_high)) {
		pr_err("Failed to init (%s)\n", SGM3785_PINCTRL_STATE_XXX_HIGH);
		ret = PTR_ERR(sgm3785_xxx_high);
	}
	sgm3785_xxx_low = pinctrl_lookup_state(
			sgm3785_pinctrl, SGM3785_PINCTRL_STATE_XXX_LOW);
	if (IS_ERR(sgm3785_xxx_low)) {
		pr_err("Failed to init (%s)\n", SGM3785_PINCTRL_STATE_XXX_LOW);
		ret = PTR_ERR(sgm3785_xxx_low);
	}

	sgm3785_main_high = pinctrl_lookup_state(
			sgm3785_pinctrl, SGM3785_PINCTRL_STATE_MAIN_HIGH);
	if (IS_ERR(sgm3785_main_high)) {
		pr_err("Failed to init (%s)\n", SGM3785_PINCTRL_STATE_MAIN_HIGH);
		ret = PTR_ERR(sgm3785_main_high);
	}
	sgm3785_main_low = pinctrl_lookup_state(
			sgm3785_pinctrl, SGM3785_PINCTRL_STATE_MAIN_LOW);
	if (IS_ERR(sgm3785_main_low)) {
		pr_err("Failed to init (%s)\n", SGM3785_PINCTRL_STATE_MAIN_LOW);
		ret = PTR_ERR(sgm3785_main_low);
	}

	flashlight_pwm_en = pinctrl_lookup_state(sgm3785_pinctrl, SGM3785_PINCTRL_PIN_PWM_EN);
	if (IS_ERR(flashlight_pwm_en)) {
		pr_err("Failed to init (%s)\n", SGM3785_PINCTRL_PIN_PWM_EN);
		ret = PTR_ERR(flashlight_pwm_en);
	}

	return ret;
}


int mt_flashlight_led_set_pwm(int pwm_num, u32 level)
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
    pwm_setting.clk_div = CLK_DIV32;
	/*src clk is by IC HW design, this value is different by different IC(MTK doc: PWM UserGuide) \u2014\u2014\u672a\u627e\u5230\u67e5\u627e\u5bf9\u5e94src\u7684\u65b9\u6cd5*/
    pwm_setting.clk_src = PWM_CLK_OLD_MODE_BLOCK;
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
    pwm_setting.PWM_MODE_OLD_REGS.DATA_WIDTH = 100;
    /* the time of high voltage level in one complete waveform, it means duty cycle, \u6b64\u5904\u542b\u4e49\u662f\u4e00\u4e2a\u5b8c\u6574\u6ce2\u5f62\u4e2d\u9ad8\u7535\u5e73\u7684\u65f6\u957f\u4e3alevel\u4e2aclk */
    pwm_setting.PWM_MODE_OLD_REGS.THRESH = level;
    pwm_set_spec_config(&pwm_setting);
    return 0;
}




static DEFINE_MUTEX(flashlight_gpio_lock);

static int sgm3785_pinctrl_set(int pin, int state)
{
	int ret = 0;

	LOG_INF("+++pin:%d, state:%d. \n", pin, state);
	if (IS_ERR(sgm3785_pinctrl)) {
		pr_err("pinctrl is not available\n");
		return -1;
	}

	mutex_lock(&flashlight_gpio_lock);

	switch (pin) {
	case SGM3785_PINCTRL_PIN_XXX:
		if (state == SGM3785_PINCTRL_PINSTATE_LOW &&
				!IS_ERR(sgm3785_xxx_low)){
			pinctrl_select_state(sgm3785_pinctrl, sgm3785_main_low);
			pinctrl_select_state(sgm3785_pinctrl, sgm3785_xxx_low);
			mdelay(6);
		}
		else if (state == SGM3785_PINCTRL_PINSTATE_HIGH &&
				!IS_ERR(sgm3785_xxx_high) && !IS_ERR(flashlight_pwm_en)){	//torch mode
			pr_info("flashlights_sgm3785 enter torch mode, light_pwm_value= %d\n", light_pwm_value);
			ret = pinctrl_select_state(sgm3785_pinctrl, sgm3785_main_low);
			ret = pinctrl_select_state(sgm3785_pinctrl, sgm3785_xxx_high);
			mdelay(6);
			ret = pinctrl_select_state(sgm3785_pinctrl, flashlight_pwm_en); //ENM
			mt_flashlight_led_set_pwm(5, light_pwm_value);
		}

		else
			pr_err("set err, pin(%d) state(%d)\n", pin, state);
		break;
	case SGM3785_PINCTRL_PIN_MAIN:
		if (state == SGM3785_PINCTRL_MAIN_PINSTATE_LOW &&
				!IS_ERR(sgm3785_main_low)){
				pinctrl_select_state(sgm3785_pinctrl, sgm3785_main_low);
				pinctrl_select_state(sgm3785_pinctrl, sgm3785_xxx_low);
				mdelay(6);
				pr_info("flashlights_sgm3785 pwn disable");
		}
		else if (state == SGM3785_PINCTRL_MAIN_PINSTATE_HIGH &&
				!IS_ERR(sgm3785_main_high) && !IS_ERR(flashlight_pwm_en)){	//flash mode
					pr_info("flashlights_sgm3785 enter flash mode");
					ret = pinctrl_select_state(sgm3785_pinctrl, flashlight_pwm_en);
					mt_flashlight_led_set_pwm(5, 89);
					ret = pinctrl_select_state(sgm3785_pinctrl, sgm3785_main_low); //ENF
					ret = pinctrl_select_state(sgm3785_pinctrl, sgm3785_main_high);
					udelay(500);
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
static int sgm3785_enable(void)
{
	//int pin = 0, state = 1;

	/* TODO: wrap enable function */
	LOG_ERR("mainFlashFlag:%d.\n", mainFlashFlag);
	if (mainFlashFlag) {
		sgm3785_pinctrl_set(SGM3785_PINCTRL_PIN_MAIN, SGM3785_PINCTRL_MAIN_PINSTATE_HIGH);		//flash mode
		//sgm3785_pinctrl_set(SGM3785_PINCTRL_PIN_XXX, SGM3785_PINCTRL_PINSTATE_LOW);

	} else {
		//sgm3785_pinctrl_set(SGM3785_PINCTRL_PIN_MAIN, SGM3785_PINCTRL_MAIN_PINSTATE_LOW);
		//+EXTB P211011-02914,zhanghao2.wt,modify,2021/10/12,fix press home key torch has flash
		if (g_torchWaitLock == 0) {
			enable_torch_wakelock(); //bug 682590, zhanghao2.wt, ADD, 2021/9/28, fix flashlight close screen and torch had closed.
			light_pwm_value = 48;
			sgm3785_pinctrl_set(SGM3785_PINCTRL_PIN_XXX, SGM3785_PINCTRL_PINSTATE_HIGH);		//torch mode
		}
		//-EXTB P211011-02914,zhanghao2.wt,modify,2021/10/12,fix press home key torch has flash
	}
	return 0;
}

/* flashlight disable function */
static int sgm3785_disable(void)
{
	//int pin = 0, state = 0;

	/* TODO: wrap disable function */
	//+bug 682590, zhanghao2.wt, ADD, 2021/9/28, fix flashlight close screen and torch had closed.
	LOG_ERR("disable.\n");
	diable_torch_wakelock();
	//-bug 682590, zhanghao2.wt, ADD, 2021/9/28, fix flashlight close screen and torch had closed.
	sgm3785_pinctrl_set(SGM3785_PINCTRL_PIN_MAIN, SGM3785_PINCTRL_MAIN_PINSTATE_LOW);
	return 0;
}

/* set flashlight level */
static int sgm3785_set_level(int level)
{
	//int pin = 0, state = 0;

	/* TODO: wrap set level function */

	//return sgm3785_pinctrl_set(pin, state);
        if (level == 1)
            mainFlashFlag = 1;
        else if (level == 0 || level == 6){
		light_pwm_value = 48;
		mainFlashFlag  = 0;
	}

    LOG_INF("level:%d, mainFlashFlag:%d,light_pwm_value=%d.\n", level, mainFlashFlag,light_pwm_value);
	return 0;
}

/* flashlight init */
static int sgm3785_init(void)
{
	//int pin = 0, state = 0;

	/* TODO: wrap init function */

	//return sgm3785_pinctrl_set(pin, state);
	return 0;
}

/* flashlight uninit */
static int sgm3785_uninit(void)
{
	//int pin = 0, state = 0;

	/* TODO: wrap uninit function */

	//return sgm3785_pinctrl_set(pin, state);
	return 0;
}

/******************************************************************************
 * Timer and work queue
 *****************************************************************************/
static struct hrtimer sgm3785_timer;
static unsigned int sgm3785_timeout_ms;

static void sgm3785_work_disable(struct work_struct *data)
{
	pr_debug("work queue callback\n");
	sgm3785_disable();
}

static enum hrtimer_restart sgm3785_timer_func(struct hrtimer *timer)
{
	schedule_work(&sgm3785_work);
	return HRTIMER_NORESTART;
}


/******************************************************************************
 * Flashlight operations
 *****************************************************************************/
static int sgm3785_ioctl(unsigned int cmd, unsigned long arg)
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
		sgm3785_timeout_ms = fl_arg->arg;
		break;

	case FLASH_IOC_SET_DUTY:
		LOG_INF("FLASH_IOC_SET_DUTY(%d): %d\n",
				channel, (int)fl_arg->arg);
		sgm3785_set_level(fl_arg->arg);
		break;

	case FLASH_IOC_SET_ONOFF:
		LOG_INF("FLASH_IOC_SET_ONOFF(%d): %d\n",
				channel, (int)fl_arg->arg);
		if (fl_arg->arg == 1) {
			/*
			if (sgm3785_timeout_ms) {
				s = sgm3785_timeout_ms / 1000;
				ns = sgm3785_timeout_ms % 1000 * 1000000;
				ktime = ktime_set(s, ns);
				hrtimer_start(&sgm3785_timer, ktime,
						HRTIMER_MODE_REL);
			}*/
			sgm3785_enable();
		} else {
			sgm3785_disable();
			//hrtimer_cancel(&sgm3785_timer);
		}
		break;
	default:
		LOG_INF("No such command and arg(%d): (%d, %d)\n",
				channel, _IOC_NR(cmd), (int)fl_arg->arg);
		return -ENOTTY;
	}

	return 0;
}

static int sgm3785_open(void)
{
	/* Move to set driver for saving power */
	return 0;
}

static int sgm3785_release(void)
{
	/* Move to set driver for saving power */
	return 0;
}

static int sgm3785_set_driver(int set)
{
	int ret = 0;

	/* set chip and usage count */
	mutex_lock(&sgm3785_mutex);
	if (set) {
		if (!use_count)
			ret = sgm3785_init();
		use_count++;
		pr_debug("Set driver: %d\n", use_count);
	} else {
		use_count--;
		if (!use_count)
			ret = sgm3785_uninit();
		if (use_count < 0)
			use_count = 0;
		pr_debug("Unset driver: %d\n", use_count);
	}
	mutex_unlock(&sgm3785_mutex);

	return ret;
}

static ssize_t sgm3785_strobe_store(struct flashlight_arg arg)
{
	sgm3785_set_driver(1);
	sgm3785_set_level(arg.level);
	sgm3785_timeout_ms = 0;
	sgm3785_enable();
	msleep(arg.dur);
	sgm3785_disable();
	sgm3785_set_driver(0);

	return 0;
}

static struct flashlight_operations sgm3785_ops = {
	sgm3785_open,
	sgm3785_release,
	sgm3785_ioctl,
	sgm3785_strobe_store,
	sgm3785_set_driver
};


/******************************************************************************
 * Platform device and driver
 *****************************************************************************/
static int sgm3785_chip_init(void)
{
	/* NOTE: Chip initialication move to "set driver" for power saving.
	 * sgm3785_init();
	 */

	return 0;
}

static int sgm3785_parse_dt(struct device *dev,
		struct sgm3785_platform_data *pdata)
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
			light_pwm_value = 28;
			sgm3785_pinctrl_set(SGM3785_PINCTRL_PIN_XXX, SGM3785_PINCTRL_PINSTATE_HIGH);
			break;

		case 1002:
			light_pwm_value = 38;
			sgm3785_pinctrl_set(SGM3785_PINCTRL_PIN_XXX, SGM3785_PINCTRL_PINSTATE_HIGH);
			break;

		case 1003:
			light_pwm_value = 48;
			sgm3785_pinctrl_set(SGM3785_PINCTRL_PIN_XXX, SGM3785_PINCTRL_PINSTATE_HIGH);
			break;

		case 1005:
			light_pwm_value = 58;
			sgm3785_pinctrl_set(SGM3785_PINCTRL_PIN_XXX, SGM3785_PINCTRL_PINSTATE_HIGH);
			break;

		case 1007:
			light_pwm_value = 68;
			sgm3785_pinctrl_set(SGM3785_PINCTRL_PIN_XXX, SGM3785_PINCTRL_PINSTATE_HIGH);
			break;

		default:
			light_pwm_value = 45;
			sgm3785_pinctrl_set(SGM3785_PINCTRL_PIN_XXX, SGM3785_PINCTRL_PINSTATE_HIGH);
			break;
	}
	pr_info("Exit!\n");
	return count;
}

static DEVICE_ATTR(rear_flash, 0664, show_flashduty1, store_flashduty1);


int ss_flashlight_node_create(void)
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
                        err = sgm3785_disable();
			if(err < 0)
				LOG_ERR("AAA - error1 - AAA\n");
			break;
		case 1: //on
			mainFlashFlag  = 0;
                        err = sgm3785_enable();
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


static int sgm3785_probe(struct platform_device *pdev)
{
	struct sgm3785_platform_data *pdata = dev_get_platdata(&pdev->dev);
	int err, ret;
	int i;

	LOG_INF("Probe start.\n");

	/* init pinctrl */
	if (sgm3785_pinctrl_init(pdev)) {
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
		err = sgm3785_parse_dt(&pdev->dev, pdata);
		if (err)
			goto err;
	}

	/* init work queue */
	INIT_WORK(&sgm3785_work, sgm3785_work_disable);

	/* init timer */
	hrtimer_init(&sgm3785_timer, CLOCK_MONOTONIC, HRTIMER_MODE_REL);
	sgm3785_timer.function = sgm3785_timer_func;
	sgm3785_timeout_ms = 100;

	/* init chip hw */
	sgm3785_chip_init();

	/* clear usage count */
	use_count = 0;

	/* register flashlight device */
	if (pdata->channel_num) {
		for (i = 0; i < pdata->channel_num; i++)
			if (flashlight_dev_register_by_device_id(
						&pdata->dev_id[i],
						&sgm3785_ops)) {
				err = -EFAULT;
				goto err;
			}
	} else {
		if (flashlight_dev_register(SGM3785_NAME, &sgm3785_ops)) {
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

static int sgm3785_remove(struct platform_device *pdev)
{
	struct sgm3785_platform_data *pdata = dev_get_platdata(&pdev->dev);
	int i;

	pr_debug("Remove start.\n");

	pdev->dev.platform_data = NULL;

	/* unregister flashlight device */
	if (pdata && pdata->channel_num)
		for (i = 0; i < pdata->channel_num; i++)
			flashlight_dev_unregister_by_device_id(
					&pdata->dev_id[i]);
	else
		flashlight_dev_unregister(SGM3785_NAME);

	/* flush work queue */
	flush_work(&sgm3785_work);

	pr_debug("Remove done.\n");

	return 0;
}

#ifdef CONFIG_OF
static const struct of_device_id sgm3785_gpio_of_match[] = {
	{.compatible = SGM3785_GPIO_DTNAME},
	{},
};
MODULE_DEVICE_TABLE(of, sgm3785_gpio_of_match);
#else
static struct platform_device sgm3785_gpio_platform_device[] = {
	{
		.name = SGM3785_NAME,
		.id = 0,
		.dev = {}
	},
	{}
};
MODULE_DEVICE_TABLE(platform, sgm3785_gpio_platform_device);
#endif

static struct platform_driver sgm3785_platform_driver = {
	.probe = sgm3785_probe,
	.remove = sgm3785_remove,
	.driver = {
		.name = SGM3785_NAME,
		.owner = THIS_MODULE,
#ifdef CONFIG_OF
		.of_match_table = sgm3785_gpio_of_match,
#endif
	},
};

static int __init flashlight_sgm3785_init(void)
{
	int ret;

	LOG_INF("Init start.\n");

#ifndef CONFIG_OF
	ret = platform_device_register(&sgm3785_gpio_platform_device);
	if (ret) {
		pr_err("Failed to register platform device\n");
		return ret;
	}
#endif

	ret = platform_driver_register(&sgm3785_platform_driver);
	if (ret) {
		pr_err("Failed to register platform driver\n");
		return ret;
	}

	LOG_INF("Init done.\n");

	return 0;
}

static void __exit flashlight_sgm3785_exit(void)
{
	pr_debug("Exit start.\n");

	platform_driver_unregister(&sgm3785_platform_driver);

	pr_debug("Exit done.\n");
}

late_initcall(flashlight_sgm3785_init);
module_exit(flashlight_sgm3785_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Chris<chenshiqiang@wingtechcom>");
MODULE_DESCRIPTION("MTK Flashlight SGM3785 Driver");
