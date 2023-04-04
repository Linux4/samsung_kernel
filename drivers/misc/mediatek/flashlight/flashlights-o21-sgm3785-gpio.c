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
/* hs03s code for CAM-AL5625-01-247 by lisizhou at 2021/07/22 start */
#include <linux/spinlock.h>
/* hs03s code for CAM-AL5625-01-247 by lisizhou at 2021/07/22 end */
#include <linux/of.h>
#include <linux/list.h>
#include <linux/delay.h>
#include <linux/pinctrl/consumer.h>
/* hs03s code for CAM-AL5625-01-247 by lisizhou at 2021/06/25 start */
#include <linux/of_gpio.h>
#include <linux/gpio.h>
/* hs03s code for CAM-AL5625-01-247 by lisizhou at 2021/06/25 end */

#include "flashlight-core.h"
#include "flashlight-dt.h"

#include <mt-plat/mtk_pwm.h>

/* define device tree */
#define O21_SGM3785_GPIO_DTNAME "mediatek,flashlights_sgm3785_gpio"

/* Device name */
#define O21_SGM3785_GPIO_NAME "flashlights-o21-sgm3785-gpio"

#define O21_LEVEL_DEFAULT  8
/* hs03s code for CAM-AL5625-01-247 by lisizhou at 2021/06/16 start */
#define O21_LEVEL_MAX      31
/* hs03s code for CAM-AL5625-01-247 by lisizhou at 2021/06/16 end */

/* define mutex and work queue */
/* hs04 code for  P220826-01362 by chenjun at 2022/08/30 start */
static DEFINE_MUTEX(o21_sgm3785_gpio_mutex);
static DEFINE_MUTEX(o21_sgm3785_pinctrl_mutex);
static struct work_struct o21_sgm3785_gpio_work;
unsigned int o21_current_level;
/* hs04 code for  P220826-01362 by chenjun at 2022/08/30 end */

/* define pinctrl */
typedef enum{
    O21_SGM3785_ENM_GPIO_MODE,
    O21_SGM3785_ENM_PWM_MODE,
    O21_SGM3785_ENF_GPIO_MODE,
    O21_SGM3785_ENF_PWM__MODE,
}O21_PinType;


typedef enum {
    O21_MODE_SHUTOFF = 0,
    O21_MODE_TORCH,
    O21_MODE_PREFLASH,
    O21_MODE_FLASH,
    O21_MODE_MAX,
}o21_flashlight_mode_t;

#define O21_SGM3785_GPIO_PINCTRL_PINSTATE_LOW 0
#define O21_SGM3785_GPIO_PINCTRL_PINSTATE_HIGH 1

o21_flashlight_mode_t o21_flashlight_mode = 0;

struct pinctrl *o21_sgm_gpio_pinctrl = NULL;
struct pinctrl_state *o21_sgm_gpio_default = NULL;
struct pinctrl_state *o21_sgm3785_enm_gpio_h = NULL;
struct pinctrl_state *o21_sgm3785_enm_gpio_l = NULL;
struct pinctrl_state *o21_sgm3785_enm_pwm_h = NULL;
struct pinctrl_state *o21_sgm3785_enm_pwm_l = NULL;

struct pinctrl_state *o21_sgm3785_enf_gpio_h = NULL;
struct pinctrl_state *o21_sgm3785_enf_gpio_l = NULL;

struct pwm_spec_config o21_flashlight_config = {
    .pwm_no = PWM_MIN,
    .mode = PWM_MODE_OLD,
    .clk_div = CLK_DIV32,//CLK_DIV16: 147.7kHz    CLK_DIV32: 73.8kHz
    .clk_src = PWM_CLK_OLD_MODE_BLOCK,
    .pmic_pad = false,
    .PWM_MODE_OLD_REGS.IDLE_VALUE = IDLE_FALSE,
    .PWM_MODE_OLD_REGS.GUARD_VALUE = GUARD_FALSE,
    .PWM_MODE_OLD_REGS.GDURATION = 0,
    .PWM_MODE_OLD_REGS.WAVE_NUM = 0,
    .PWM_MODE_OLD_REGS.DATA_WIDTH = O21_LEVEL_MAX,
    .PWM_MODE_OLD_REGS.THRESH = 5,
};

/* define usage count */
static int o21_use_count;

/* platform data */
struct o21_sgm3785_gpio_platform_data {
    int channel_num;
    struct flashlight_device_id *dev_id;
};

/* hs03s code for CAM-AL5625-01-247 by lisizhou at 2021/07/22 start */
struct wakeup_source *o21_torch_suspend_lock = NULL;
struct mutex o21_torch_access_lock;
/* hs03s code for CAM-AL5625-01-247 by lisizhou at 2021/07/22 end */

/******************************************************************************
 * Pinctrl configuration
 *****************************************************************************/
static int o21_sgm3785_gpio_pinctrl_init(struct platform_device *pdev)
{
    int ret = 0;

    /* get pinctrl */
    o21_sgm_gpio_pinctrl = devm_pinctrl_get(&pdev->dev);
    if (IS_ERR(o21_sgm_gpio_pinctrl)) {
        pr_err("Failed to get flashlight pinctrl.\n");
        ret = PTR_ERR(o21_sgm_gpio_pinctrl);
        return ret;
    }

// get enm gpio mode
    o21_sgm3785_enm_gpio_h = pinctrl_lookup_state(o21_sgm_gpio_pinctrl, "flashlight_enm_gpio_high");
    if (IS_ERR(o21_sgm3785_enm_gpio_h)) {
        //pr_err("Failed to init (%s)\n", O21_SGM3785_ENM_GPIO_MODE_HIGH);
        ret = PTR_ERR(o21_sgm3785_enm_gpio_h);
    }

     o21_sgm3785_enm_gpio_l = pinctrl_lookup_state(o21_sgm_gpio_pinctrl, "flashlight_enm_gpio_low");
      if (IS_ERR(o21_sgm3785_enm_gpio_l)) {
        //pr_err("Failed to init (%s)\n", O21_SGM3785_ENM_GPIO_MODE_LOW);
        ret = PTR_ERR(o21_sgm3785_enm_gpio_l);
     }

//get enm pwm mode
       o21_sgm3785_enm_pwm_h = pinctrl_lookup_state(o21_sgm_gpio_pinctrl, "flashlight_enm_pwm_high");
      if (IS_ERR(o21_sgm3785_enm_pwm_h)) {
        //pr_err("Failed to init (%s)\n", O21_SGM3785_ENM_PWM_MODE_HIGH);
        ret = PTR_ERR(o21_sgm3785_enm_pwm_h);
     }

      o21_sgm3785_enm_pwm_l = pinctrl_lookup_state(o21_sgm_gpio_pinctrl, "flashlight_enm_pwm_low");
      if (IS_ERR(o21_sgm3785_enm_pwm_l)) {
        //pr_err("Failed to init (%s)\n", O21_SGM3785_ENM_PWM_MODE_LOW);
        ret = PTR_ERR(o21_sgm3785_enm_pwm_l);
     }

//get enf gpio mode
    o21_sgm3785_enf_gpio_h = pinctrl_lookup_state(o21_sgm_gpio_pinctrl, "flashlight_enf_gpio_high");
    if (IS_ERR(o21_sgm3785_enf_gpio_h)) {
        //pr_err("Failed to init (%s)\n", O21_SGM3785_ENF_GPIO_MODE_HIGH);
        ret = PTR_ERR(o21_sgm3785_enf_gpio_h);
    }

     o21_sgm3785_enf_gpio_l = pinctrl_lookup_state(o21_sgm_gpio_pinctrl, "flashlight_enf_gpio_low");
     if (IS_ERR(o21_sgm3785_enf_gpio_l)) {
        //pr_err("Failed to init (%s)\n", O21_SGM3785_ENF_GPIO_MODE_LOW);
        ret = PTR_ERR(o21_sgm3785_enf_gpio_l);
     }

    return ret;
}

/* hs04 code for  P220826-01362 by chenjun at 2022/08/30 start */
static int o21_sgm3785_gpio_pinctrl_set(int pinType, int state)
{
    int ret = 0;

    if (IS_ERR(o21_sgm_gpio_pinctrl)) {
        pr_err("pinctrl is not available\n");
        return -1;
    }

    switch (pinType) {
    case O21_SGM3785_ENM_GPIO_MODE:
        if (state == O21_SGM3785_GPIO_PINCTRL_PINSTATE_LOW &&
                !IS_ERR(o21_sgm3785_enm_gpio_l)) {
            mutex_lock(&o21_sgm3785_pinctrl_mutex);
            pinctrl_select_state(o21_sgm_gpio_pinctrl, o21_sgm3785_enm_gpio_l);
            mutex_unlock(&o21_sgm3785_pinctrl_mutex);
        }
        else if (state == O21_SGM3785_GPIO_PINCTRL_PINSTATE_HIGH &&
                !IS_ERR(o21_sgm3785_enm_gpio_h)) {
            mutex_lock(&o21_sgm3785_pinctrl_mutex);
            pinctrl_select_state(o21_sgm_gpio_pinctrl, o21_sgm3785_enm_gpio_h);
            mutex_unlock(&o21_sgm3785_pinctrl_mutex);
        }
        else
            pr_err("set err, pin(%d) state(%d)\n", pinType, state);
        break;

    case O21_SGM3785_ENM_PWM_MODE:
        if (state == O21_SGM3785_GPIO_PINCTRL_PINSTATE_LOW &&
                !IS_ERR(o21_sgm3785_enm_pwm_l)) {
            mutex_lock(&o21_sgm3785_pinctrl_mutex);
            pinctrl_select_state(o21_sgm_gpio_pinctrl, o21_sgm3785_enm_pwm_l);
            mutex_unlock(&o21_sgm3785_pinctrl_mutex);
        }
        else if (state == O21_SGM3785_GPIO_PINCTRL_PINSTATE_HIGH &&
                !IS_ERR(o21_sgm3785_enm_pwm_h)) {
            mutex_lock(&o21_sgm3785_pinctrl_mutex);
            pinctrl_select_state(o21_sgm_gpio_pinctrl, o21_sgm3785_enm_pwm_h);
            mutex_unlock(&o21_sgm3785_pinctrl_mutex);
        }
        else
            pr_err("set err, pin(%d) state(%d)\n", pinType, state);
        break;
    case O21_SGM3785_ENF_GPIO_MODE:
        if (state == O21_SGM3785_GPIO_PINCTRL_PINSTATE_LOW &&
                !IS_ERR(o21_sgm3785_enf_gpio_l)) {
            mutex_lock(&o21_sgm3785_pinctrl_mutex);
            pinctrl_select_state(o21_sgm_gpio_pinctrl, o21_sgm3785_enf_gpio_l);
            mutex_unlock(&o21_sgm3785_pinctrl_mutex);
        }
        else if (state == O21_SGM3785_GPIO_PINCTRL_PINSTATE_HIGH &&
                !IS_ERR(o21_sgm3785_enf_gpio_h)) {
            mutex_lock(&o21_sgm3785_pinctrl_mutex);
            pinctrl_select_state(o21_sgm_gpio_pinctrl, o21_sgm3785_enf_gpio_h);
            mutex_unlock(&o21_sgm3785_pinctrl_mutex);
        }
        else
            pr_err("set err, pin(%d) state(%d)\n", pinType, state);
        break;

    default:
        pr_err("set err, pin(%d) state(%d)\n", pinType, state);
        break;
    }
    pr_info("pin(%d) state(%d)\n", pinType, state);

    return ret;
}
/* hs04 code for  P220826-01362 by chenjun at 2022/08/30 end */


/******************************************************************************
 * o21_sgm3785_set_torch_mode
 *****************************************************************************/
/* hs03s code for CAM-AL5625-01-247 by lisizhou at 2021/07/22 start */
/* HS03s code for DEVAL5625-1829 by chenjun at 2021/07/01 start */
static int o21_sgm3785_set_torch_mode(int level)
{
    int pinType = 0, state = 0;
    int ret;
    /* TODO: wrap enable function */
    pr_info("torch_mode enter \n");

    if(NULL == o21_torch_suspend_lock){
        o21_torch_suspend_lock = wakeup_source_register(NULL, "Torch suspend wakelock");
        //spin_lock_init(&afc->afc_spin_lock);
        mutex_init(&o21_torch_access_lock);

        mutex_lock(&o21_torch_access_lock);
        __pm_stay_awake(o21_torch_suspend_lock);

        pr_info("torch wake_lock init and locked!");
    }else{
        pr_info("torch wake_lock already init and locked!");
    }
    pinType = O21_SGM3785_ENF_GPIO_MODE;
    state = O21_SGM3785_GPIO_PINCTRL_PINSTATE_LOW;
    ret = o21_sgm3785_gpio_pinctrl_set(pinType, state);

    pinType = O21_SGM3785_ENM_GPIO_MODE;
    state = O21_SGM3785_GPIO_PINCTRL_PINSTATE_HIGH;
    ret = o21_sgm3785_gpio_pinctrl_set(pinType, state);

    mdelay(5);

    //pwm mode can not work when system suspend
    pinType = O21_SGM3785_ENM_PWM_MODE;
    state = O21_SGM3785_GPIO_PINCTRL_PINSTATE_HIGH;
    ret = o21_sgm3785_gpio_pinctrl_set(pinType, state);
    o21_flashlight_config.PWM_MODE_OLD_REGS.THRESH = level * 5;
    ret = pwm_set_spec_config(&o21_flashlight_config);

    return ret;
}
/* HS03s code for DEVAL5625-1829 by chenjun at 2021/07/22 end */
/* hs03s code for CAM-AL5625-01-247 by lisizhou at 2021/06/16 end */

/* hs03s code for CAM-AL5625-01-247 by lisizhou at 2021/06/25 start */
/******************************************************************************
 * o21_sgm3785_set_preflash_mode
 *****************************************************************************/
/* flashlight enable function */
static int o21_sgm3785_set_preflash_mode(int level)
{
    int pinType = 0, state = 0;
    int ret;
    /* TODO: wrap enable function */
    pr_info("preflash_mode enter \n");

    pinType = O21_SGM3785_ENF_GPIO_MODE;
    state = O21_SGM3785_GPIO_PINCTRL_PINSTATE_LOW;
    ret = o21_sgm3785_gpio_pinctrl_set(pinType, state);

    pinType = O21_SGM3785_ENM_GPIO_MODE;
    state = O21_SGM3785_GPIO_PINCTRL_PINSTATE_HIGH;
    ret = o21_sgm3785_gpio_pinctrl_set(pinType, state);

    mdelay(5);

    //pwm mode can not work when system suspend
    pinType = O21_SGM3785_ENM_PWM_MODE;
    state = O21_SGM3785_GPIO_PINCTRL_PINSTATE_HIGH;
    ret = o21_sgm3785_gpio_pinctrl_set(pinType, state);

    o21_flashlight_config.PWM_MODE_OLD_REGS.THRESH = 29;
    ret = pwm_set_spec_config(&o21_flashlight_config);

    return ret;
}
/* hs03s code for CAM-AL5625-01-247 by lisizhou at 2021/06/25 end */

/******************************************************************************
 * o21_sgm3785_set_flash_mode
 *****************************************************************************/
/* flashlight enable function */
static int o21_sgm3785_set_flash_mode(int level)
{
    int pinType = 0, state = 0;
        int ret;
    /* TODO: wrap enable function */
    pr_info("o21_sgm3785 flash_mode enter set\n");

    pinType = O21_SGM3785_ENM_PWM_MODE;
    state = O21_SGM3785_GPIO_PINCTRL_PINSTATE_HIGH;
    ret = o21_sgm3785_gpio_pinctrl_set(pinType, state);

    o21_flashlight_config.PWM_MODE_OLD_REGS.THRESH = level;
    ret = pwm_set_spec_config(&o21_flashlight_config);
   // mdelay(2);

    pinType = O21_SGM3785_ENF_GPIO_MODE;
    state = O21_SGM3785_GPIO_PINCTRL_PINSTATE_HIGH;
    ret = o21_sgm3785_gpio_pinctrl_set(pinType, state);

    //mdelay(5);

    return ret;
}

/* hs03s code for CAM-AL5625-01-247 by lisizhou at 2021/06/16 start */
/* set flashlight level */
static void o21_sgm3785_gpio_set_level(int level)
{
   // int pinType = 0, state = 0;
    /* TODO: wrap set level function */
    pr_info("[flashlight] level = %d",level);

    if (level > O21_LEVEL_MAX) {
        level = O21_LEVEL_MAX;
    }
    o21_current_level = level;
}
/* hs03s code for CAM-AL5625-01-247 by lisizhou at 2021/06/16 end */


/* HS03s code for DEVAL5625-1829 by chenjun at 2021/07/22 start */
/* flashlight disable function */
static int o21_sgm3785_set_shutoff(void)
{
    int pinType = 0, state = 0;
    int ret = 0;
    /* TODO: wrap disable function */
    pr_info("[flashlight] o21_sgm3785_set_shutoff enter\n");

    if(NULL != o21_torch_suspend_lock){
        __pm_relax(o21_torch_suspend_lock);
        mutex_unlock(&o21_torch_access_lock);
        o21_torch_suspend_lock = NULL;
        pr_info("torch wake_lock relax!");
    }else{
        pr_info("torch wake_lock already relax!");
    }

    pinType = O21_SGM3785_ENM_GPIO_MODE;
    state = O21_SGM3785_GPIO_PINCTRL_PINSTATE_LOW;
    ret = o21_sgm3785_gpio_pinctrl_set(pinType, state);

    pinType = O21_SGM3785_ENF_GPIO_MODE;
    state = O21_SGM3785_GPIO_PINCTRL_PINSTATE_LOW;
    ret = o21_sgm3785_gpio_pinctrl_set(pinType, state);

    o21_flashlight_mode = O21_MODE_SHUTOFF;

    return ret;
}
/* HS03s code for DEVAL5625-1829 by chenjun at 2021/07/22 end */

/* flashlight init */
static int o21_sgm3785_gpio_init(void)
{
    /* TODO: wrap init function */
    o21_sgm3785_set_shutoff();
    return 0;
}

/* flashlight uninit */
static int o21_sgm3785_gpio_uninit(void)
{
    /* TODO: wrap uninit function */
    //o21_sgm3785_set_shutoff();
    return 0;
}

/******************************************************************************
 * Timer and work queue
 *****************************************************************************/
static struct hrtimer o21_sgm3785_gpio_timer;
static unsigned int o21_sgm3785_gpio_timeout_ms;


static void o21_sgm3785_gpio_work_disable(struct work_struct *data)
{
    pr_debug("work queue callback\n");
    o21_sgm3785_set_shutoff();
}

static enum hrtimer_restart o21_sgm3785_gpio_timer_func(struct hrtimer *timer)
{
    schedule_work(&o21_sgm3785_gpio_work);
    return HRTIMER_NORESTART;
}


/******************************************************************************
 * Flashlight operations
 *****************************************************************************/
/* hs03s code for CAM-AL5625-01-247 by lisizhou at 2021/06/25 start */
static int o21_sgm3785_gpio_ioctl(unsigned int cmd, unsigned long arg)
{
    struct flashlight_dev_arg *fl_arg;
    int channel;
    ktime_t ktime;
    unsigned int s;
    unsigned int ns;

    fl_arg = (struct flashlight_dev_arg *)arg;
    channel = fl_arg->channel;

    pr_info("cmd = %d,arg = %d\n",_IOC_NR(cmd),(int)fl_arg->arg);
    switch (cmd) {
    case FLASH_IOC_SET_TIME_OUT_TIME_MS:  //100
        o21_sgm3785_gpio_timeout_ms = fl_arg->arg;
        break;
    case FLASH_IOC_SET_DUTY:  //110
        o21_sgm3785_gpio_set_level(fl_arg->arg);
        break;
    case FLASH_IOC_GET_PRE_ON_TIME_MS: //130
        o21_sgm3785_gpio_timeout_ms = fl_arg->arg;
        break;
    case FLASH_IOC_GET_PRE_ON_TIME_MS_DUTY: //131
        o21_sgm3785_gpio_set_level(fl_arg->arg);
        break;
    case FLASH_IOC_PRE_ON:   //125  flash
        o21_flashlight_mode = O21_MODE_FLASH;
        break;
    case FLASH_IOC_SET_SCENARIO :  //215
        o21_flashlight_mode = O21_MODE_TORCH;
        break;
    case FLASH_IOC_IS_CHARGER_READY:  //210 preflash
        o21_flashlight_mode = O21_MODE_PREFLASH;
        break;
    case FLASH_IOC_SET_ONOFF:              //115
        pr_info("timeout_ms = %d, o21_current_level = %d, o21_flashlight_mode= %d\n",
                o21_sgm3785_gpio_timeout_ms,o21_current_level,o21_flashlight_mode);
        if (fl_arg->arg == 1) {
            switch(o21_flashlight_mode) {
                case O21_MODE_FLASH: {
                    if (o21_sgm3785_gpio_timeout_ms == 0) {
                        o21_sgm3785_gpio_timeout_ms = 100;
                    }
                    s = o21_sgm3785_gpio_timeout_ms / 1000;
                    ns = o21_sgm3785_gpio_timeout_ms % 1000 * 1000000;
                    ktime = ktime_set(s, ns);
                    hrtimer_start(&o21_sgm3785_gpio_timer, ktime,
                            HRTIMER_MODE_REL);

                    o21_sgm3785_set_flash_mode(o21_current_level);
                }
                    break;
                /* hs03s code for CAM-AL5625-01-247 by lisizhou at 2021/06/16 start */
                case O21_MODE_PREFLASH:
                    o21_sgm3785_set_preflash_mode(o21_current_level);
                    break;
                /* hs03s code for CAM-AL5625-01-247 by lisizhou at 2021/06/16 end */
                case O21_MODE_SHUTOFF:
                case O21_MODE_TORCH:
                    o21_sgm3785_set_torch_mode(o21_current_level);
                    break;
                default:
                    break;
            }
        }
        else {
            pr_info("fl_arg->arg == %d",fl_arg->arg);
            o21_sgm3785_set_shutoff();
            hrtimer_cancel(&o21_sgm3785_gpio_timer);
        }
        break;
    case FLASH_IOC_GET_HW_FAULT2:
        break;
    default:
        pr_info("No such command and arg(%d): (%d, %d)\n",
                channel, _IOC_NR(cmd), (int)fl_arg->arg);
        return -ENOTTY;
    }

    return 0;
}
/* hs03s code for CAM-AL5625-01-247 by lisizhou at 2021/06/25 end */

static int o21_sgm3785_gpio_open(void)
{
    /* Move to set driver for saving power */
    return 0;
}

static int o21_sgm3785_gpio_release(void)
{
    /* Move to set driver for saving power */
    return 0;
}

static int o21_sgm3785_gpio_set_driver(int set)
{
    int ret = 0;
    /* set chip and usage count */
    pr_info("o21_sgm3785_gpio_set_driver enter \n");
    mutex_lock(&o21_sgm3785_gpio_mutex);
    if (set) {
        if (!o21_use_count)
            ret = o21_sgm3785_gpio_init();
        o21_use_count++;
        pr_debug("Set driver: %d\n", o21_use_count);
    } else {
        o21_use_count--;
        if (!o21_use_count)
            ret = o21_sgm3785_gpio_uninit();
        if (o21_use_count < 0)
            o21_use_count = 0;
        pr_debug("Unset driver: %d\n", o21_use_count);
    }
    mutex_unlock(&o21_sgm3785_gpio_mutex);

    return ret;
}

static ssize_t o21_sgm3785_gpio_strobe_store(struct flashlight_arg arg)
{
    pr_info("o21_sgm3785_gpio_strobe_store enter \n");
    o21_sgm3785_gpio_set_driver(1);
    o21_sgm3785_gpio_set_level(arg.level);
    o21_sgm3785_gpio_timeout_ms = 0;
    o21_sgm3785_set_torch_mode(0);
    msleep(arg.dur);
    o21_sgm3785_set_shutoff();
    o21_sgm3785_gpio_set_driver(0);

    return 0;
}

static struct flashlight_operations o21_sgm3785_gpio_ops = {
    o21_sgm3785_gpio_open,
    o21_sgm3785_gpio_release,
    o21_sgm3785_gpio_ioctl,
    o21_sgm3785_gpio_strobe_store,
    o21_sgm3785_gpio_set_driver
};


/******************************************************************************
 * Platform device and driver
 *****************************************************************************/
static int o21_sgm3785_gpio_chip_init(void)
{
    /* NOTE: Chip initialication move to "set driver" for power saving.
     * o21_sgm3785_gpio_init();
     */

    return 0;
}

static int o21_sgm3785_gpio_parse_dt(struct device *dev,
        struct o21_sgm3785_gpio_platform_data *pdata)
{
    struct device_node *np, *cnp;
    u32 decouple = 0;
    int i = 0;

    if (!dev || !dev->of_node || !pdata)
        return -ENODEV;

    cnp = NULL;
    np = dev->of_node;

    pdata->channel_num = of_get_child_count(np);
    if (!pdata->channel_num) {
        pr_info("Parse no dt, node.\n");
        return 0;
    }
    pr_info("Channel number(%d).\n", pdata->channel_num);

    if (of_property_read_u32(np, "decouple", &decouple))
        pr_info("Parse no dt, decouple.\n");

    pdata->dev_id = devm_kzalloc(dev,
            pdata->channel_num *
            sizeof(struct flashlight_device_id),
            GFP_KERNEL);
    if (!pdata->dev_id)
        return -ENOMEM;

    for_each_child_of_node(np, cnp) {
        if (of_property_read_u32(cnp, "type", &pdata->dev_id[i].type))
            goto err_node_put;
        if (of_property_read_u32(cnp, "ct", &pdata->dev_id[i].ct))
            goto err_node_put;
        if (of_property_read_u32(cnp, "part", &pdata->dev_id[i].part))
            goto err_node_put;
        snprintf(pdata->dev_id[i].name, FLASHLIGHT_NAME_SIZE,
                O21_SGM3785_GPIO_NAME);
        pdata->dev_id[i].channel = i;
        pdata->dev_id[i].decouple = decouple;

        pr_info("Parse dt (type,ct,part,name,channel,decouple)=(%d,%d,%d,%s,%d,%d).\n",
                pdata->dev_id[i].type, pdata->dev_id[i].ct,
                pdata->dev_id[i].part, pdata->dev_id[i].name,
                pdata->dev_id[i].channel,
                pdata->dev_id[i].decouple);
        i++;
    }

    return 0;

err_node_put:
    of_node_put(cnp);
    return -EINVAL;
}

static int o21_sgm3785_gpio_probe(struct platform_device *pdev)
{
    struct o21_sgm3785_gpio_platform_data *pdata = dev_get_platdata(&pdev->dev);
    int err;
    int i;

    pr_info("Probe start.\n");

    /* init pinctrl */
    if (o21_sgm3785_gpio_pinctrl_init(pdev)) {
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
        err = o21_sgm3785_gpio_parse_dt(&pdev->dev, pdata);
        if (err)
            goto err;
    }

    /* init work queue */
    INIT_WORK(&o21_sgm3785_gpio_work, o21_sgm3785_gpio_work_disable);

    /* init timer */
    hrtimer_init(&o21_sgm3785_gpio_timer, CLOCK_MONOTONIC, HRTIMER_MODE_REL);
    o21_sgm3785_gpio_timer.function = o21_sgm3785_gpio_timer_func;
    o21_sgm3785_gpio_timeout_ms = 100;

    /* init chip hw */
    o21_sgm3785_gpio_chip_init();

    /* clear usage count */
    o21_use_count = 0;

    /* register flashlight device */
    if (pdata->channel_num) {
        for (i = 0; i < pdata->channel_num; i++)
            if (flashlight_dev_register_by_device_id(
                        &pdata->dev_id[i],
                        &o21_sgm3785_gpio_ops)) {
                err = -EFAULT;
                goto err;
            }
    } else {
        if (flashlight_dev_register(O21_SGM3785_GPIO_NAME, &o21_sgm3785_gpio_ops)) {
            err = -EFAULT;
            goto err;
        }
    }

    pr_info("Probe done.\n");

    return 0;
err:
    return err;
}

static int o21_sgm3785_gpio_remove(struct platform_device *pdev)
{
    struct o21_sgm3785_gpio_platform_data *pdata = dev_get_platdata(&pdev->dev);
    int i;

    pr_debug("Remove start.\n");

    pdev->dev.platform_data = NULL;

    /* unregister flashlight device */
    if (pdata && pdata->channel_num)
        for (i = 0; i < pdata->channel_num; i++)
            flashlight_dev_unregister_by_device_id(
                    &pdata->dev_id[i]);
    else
        flashlight_dev_unregister(O21_SGM3785_GPIO_NAME);

    /* flush work queue */
    flush_work(&o21_sgm3785_gpio_work);

    pr_debug("Remove done.\n");

    return 0;
}

#ifdef CONFIG_OF
static const struct of_device_id o21_sgm3785_gpio_of_match[] = {
    {.compatible = O21_SGM3785_GPIO_DTNAME},
    {},
};
MODULE_DEVICE_TABLE(of, o21_sgm3785_gpio_of_match);
#else
static struct platform_device o21_sgm3785_gpio_platform_device[] = {
    {
        .name = O21_SGM3785_GPIO_NAME,
        .id = 0,
        .dev = {}
    },
    {}
};
MODULE_DEVICE_TABLE(platform, o21_sgm3785_gpio_platform_device);
#endif

static struct platform_driver o21_sgm3785_gpio_platform_driver = {
    .probe = o21_sgm3785_gpio_probe,
    .remove = o21_sgm3785_gpio_remove,
    .driver = {
        .name = O21_SGM3785_GPIO_NAME,
        .owner = THIS_MODULE,
#ifdef CONFIG_OF
        .of_match_table = o21_sgm3785_gpio_of_match,
#endif
    },
};

static int __init flashlight_o21_sgm3785_gpio_init(void)
{
    int ret;

    pr_debug("Init start.\n");

#ifndef CONFIG_OF
    ret = platform_device_register(&o21_sgm3785_gpio_platform_device);
    if (ret) {
        pr_err("Failed to register platform device\n");
        return ret;
    }
#endif

    ret = platform_driver_register(&o21_sgm3785_gpio_platform_driver);
    if (ret) {
        pr_err("Failed to register platform driver\n");
        return ret;
    }

    pr_debug("Init done.\n");

    return 0;
}

static void __exit flashlight_o21_sgm3785_gpio_exit(void)
{
    pr_debug("Exit start.\n");

    platform_driver_unregister(&o21_sgm3785_gpio_platform_driver);

    pr_debug("Exit done.\n");
}

module_init(flashlight_o21_sgm3785_gpio_init);
module_exit(flashlight_o21_sgm3785_gpio_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Focus.li <lisizhou@huaqin.com>");
MODULE_DESCRIPTION("HQ O21_SGM3785 GPIO Driver");