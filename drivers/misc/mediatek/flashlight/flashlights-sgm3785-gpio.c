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
#define SGM3785_GPIO_DTNAME "mediatek,flashlights_sgm3785_gpio"

/* Device name */
#define SGM3785_GPIO_NAME "flashlights-sgm3785-gpio"

#define LEVEL_DEFAULT  8
/* hs03s code for CAM-AL5625-01-247 by lisizhou at 2021/06/16 start */
#define LEVEL_MAX      31
/* hs03s code for CAM-AL5625-01-247 by lisizhou at 2021/06/16 end */

/* define mutex and work queue */
static DEFINE_MUTEX(sgm3785_gpio_mutex);
static struct work_struct sgm3785_gpio_work;
unsigned int current_level;

/* hs03s code for CAM-AL5625-01-247 by lisizhou at 2021/06/25 start */
static int pin_status = 0;
static int base = 73;
static int pin_id = 163;
/* hs03s code for CAM-AL5625-01-247 by lisizhou at 2021/06/25 end */

/* define pinctrl */
typedef enum{
    SGM3785_ENM_GPIO_MODE,
    SGM3785_ENM_PWM_MODE,
    SGM3785_ENF_GPIO_MODE,
    SGM3785_ENF_PWM__MODE,
}PinType;


typedef enum {
    MODE_SHUTOFF = 0,
    MODE_TORCH,
    MODE_PREFLASH,
    MODE_FLASH,
    MODE_MAX,
}flashlight_mode_t;

#define SGM3785_GPIO_PINCTRL_PINSTATE_LOW 0
#define SGM3785_GPIO_PINCTRL_PINSTATE_HIGH 1

flashlight_mode_t flashlight_mode = 0;

struct pinctrl *sgm_gpio_pinctrl = NULL;
struct pinctrl_state *sgm_gpio_default = NULL;
struct pinctrl_state *sgm3785_enm_gpio_h = NULL;
struct pinctrl_state *sgm3785_enm_gpio_l = NULL;
struct pinctrl_state *sgm3785_enm_pwm_h = NULL;
struct pinctrl_state *sgm3785_enm_pwm_l = NULL;

struct pinctrl_state *sgm3785_enf_gpio_h = NULL;
struct pinctrl_state *sgm3785_enf_gpio_l = NULL;

struct pwm_spec_config flashlight_config = {
    .pwm_no = PWM_MIN,
    .mode = PWM_MODE_OLD,
    .clk_div = CLK_DIV32,//CLK_DIV16: 147.7kHz    CLK_DIV32: 73.8kHz
    .clk_src = PWM_CLK_OLD_MODE_BLOCK,
    .pmic_pad = false,
    .PWM_MODE_OLD_REGS.IDLE_VALUE = IDLE_FALSE,
    .PWM_MODE_OLD_REGS.GUARD_VALUE = GUARD_FALSE,
    .PWM_MODE_OLD_REGS.GDURATION = 0,
    .PWM_MODE_OLD_REGS.WAVE_NUM = 0,
    .PWM_MODE_OLD_REGS.DATA_WIDTH = LEVEL_MAX,
    .PWM_MODE_OLD_REGS.THRESH = 5,
};

/* define usage count */
static int use_count;

/* platform data */
struct sgm3785_gpio_platform_data {
    int channel_num;
    struct flashlight_device_id *dev_id;
};

/* hs03s code for CAM-AL5625-01-247 by lisizhou at 2021/07/22 start */
struct wakeup_source *torch_suspend_lock = NULL;
struct mutex torch_access_lock;
/* hs03s code for CAM-AL5625-01-247 by lisizhou at 2021/07/22 end */

/******************************************************************************
 * Pinctrl configuration
 *****************************************************************************/
static int sgm3785_gpio_pinctrl_init(struct platform_device *pdev)
{
    int ret = 0;

    /* get pinctrl */
    sgm_gpio_pinctrl = devm_pinctrl_get(&pdev->dev);
    if (IS_ERR(sgm_gpio_pinctrl)) {
        pr_err("Failed to get flashlight pinctrl.\n");
        ret = PTR_ERR(sgm_gpio_pinctrl);
        return ret;
    }

// get enm gpio mode
    sgm3785_enm_gpio_h = pinctrl_lookup_state(sgm_gpio_pinctrl, "flashlight_enm_gpio_high");
    if (IS_ERR(sgm3785_enm_gpio_h)) {
        //pr_err("Failed to init (%s)\n", SGM3785_ENM_GPIO_MODE_HIGH);
        ret = PTR_ERR(sgm3785_enm_gpio_h);
    }

     sgm3785_enm_gpio_l = pinctrl_lookup_state(sgm_gpio_pinctrl, "flashlight_enm_gpio_low");
      if (IS_ERR(sgm3785_enm_gpio_l)) {
        //pr_err("Failed to init (%s)\n", SGM3785_ENM_GPIO_MODE_LOW);
        ret = PTR_ERR(sgm3785_enm_gpio_l);
     }

//get enm pwm mode
       sgm3785_enm_pwm_h = pinctrl_lookup_state(sgm_gpio_pinctrl, "flashlight_enm_pwm_high");
      if (IS_ERR(sgm3785_enm_pwm_h)) {
        //pr_err("Failed to init (%s)\n", SGM3785_ENM_PWM_MODE_HIGH);
        ret = PTR_ERR(sgm3785_enm_pwm_h);
     }

      sgm3785_enm_pwm_l = pinctrl_lookup_state(sgm_gpio_pinctrl, "flashlight_enm_pwm_low");
      if (IS_ERR(sgm3785_enm_pwm_l)) {
        //pr_err("Failed to init (%s)\n", SGM3785_ENM_PWM_MODE_LOW);
        ret = PTR_ERR(sgm3785_enm_pwm_l);
     }

//get enf gpio mode
    sgm3785_enf_gpio_h = pinctrl_lookup_state(sgm_gpio_pinctrl, "flashlight_enf_gpio_high");
    if (IS_ERR(sgm3785_enf_gpio_h)) {
        //pr_err("Failed to init (%s)\n", SGM3785_ENF_GPIO_MODE_HIGH);
        ret = PTR_ERR(sgm3785_enf_gpio_h);
    }

     sgm3785_enf_gpio_l = pinctrl_lookup_state(sgm_gpio_pinctrl, "flashlight_enf_gpio_low");
     if (IS_ERR(sgm3785_enf_gpio_l)) {
        //pr_err("Failed to init (%s)\n", SGM3785_ENF_GPIO_MODE_LOW);
        ret = PTR_ERR(sgm3785_enf_gpio_l);
     }

    return ret;
}

static int sgm3785_gpio_pinctrl_set(int pinType, int state)
{
    int ret = 0;

    if (IS_ERR(sgm_gpio_pinctrl)) {
        pr_err("pinctrl is not available\n");
        return -1;
    }

    switch (pinType) {
    case SGM3785_ENM_GPIO_MODE:
        if (state == SGM3785_GPIO_PINCTRL_PINSTATE_LOW &&
                !IS_ERR(sgm3785_enm_gpio_l))
            pinctrl_select_state(sgm_gpio_pinctrl, sgm3785_enm_gpio_l);
        else if (state == SGM3785_GPIO_PINCTRL_PINSTATE_HIGH &&
                !IS_ERR(sgm3785_enm_gpio_h))
            pinctrl_select_state(sgm_gpio_pinctrl, sgm3785_enm_gpio_h);
        else
            pr_err("set err, pin(%d) state(%d)\n", pinType, state);
        break;

    case SGM3785_ENM_PWM_MODE:
        if (state == SGM3785_GPIO_PINCTRL_PINSTATE_LOW &&
                !IS_ERR(sgm3785_enm_pwm_l))
            pinctrl_select_state(sgm_gpio_pinctrl, sgm3785_enm_pwm_l);
        else if (state == SGM3785_GPIO_PINCTRL_PINSTATE_HIGH &&
                !IS_ERR(sgm3785_enm_pwm_h))
            pinctrl_select_state(sgm_gpio_pinctrl, sgm3785_enm_pwm_h);
        else
            pr_err("set err, pin(%d) state(%d)\n", pinType, state);
        break;
    case SGM3785_ENF_GPIO_MODE:
        if (state == SGM3785_GPIO_PINCTRL_PINSTATE_LOW &&
                !IS_ERR(sgm3785_enf_gpio_l))
            pinctrl_select_state(sgm_gpio_pinctrl, sgm3785_enf_gpio_l);
        else if (state == SGM3785_GPIO_PINCTRL_PINSTATE_HIGH &&
                !IS_ERR(sgm3785_enf_gpio_h))
            pinctrl_select_state(sgm_gpio_pinctrl, sgm3785_enf_gpio_h);
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

/******************************************************************************
 * sgm3785_set_torch_mode
 *****************************************************************************/
/* hs03s code for CAM-AL5625-01-247 by lisizhou at 2021/07/22 start */
/* HS03s code for DEVAL5625-1829 by chenjun at 2021/07/01 start */
static int sgm3785_set_torch_mode(int level)
{
    int pinType = 0, state = 0;
    int ret;
    /* TODO: wrap enable function */
    pr_info("torch_mode enter \n");

    if(NULL == torch_suspend_lock){
        torch_suspend_lock = wakeup_source_register(NULL, "Torch suspend wakelock");
        //spin_lock_init(&afc->afc_spin_lock);
        mutex_init(&torch_access_lock);

        mutex_lock(&torch_access_lock);
        __pm_stay_awake(torch_suspend_lock);

        pr_info("torch wake_lock init and locked!");
    }else{
        pr_info("torch wake_lock already init and locked!");
    }
    pinType = SGM3785_ENF_GPIO_MODE;
    state = SGM3785_GPIO_PINCTRL_PINSTATE_LOW;
    ret = sgm3785_gpio_pinctrl_set(pinType, state);

    pinType = SGM3785_ENM_GPIO_MODE;
    state = SGM3785_GPIO_PINCTRL_PINSTATE_HIGH;
    ret = sgm3785_gpio_pinctrl_set(pinType, state);

    mdelay(5);

    //pwm mode can not work when system suspend
    pinType = SGM3785_ENM_PWM_MODE;
    state = SGM3785_GPIO_PINCTRL_PINSTATE_HIGH;
    ret = sgm3785_gpio_pinctrl_set(pinType, state);
    flashlight_config.PWM_MODE_OLD_REGS.THRESH = level * 5;
    ret = pwm_set_spec_config(&flashlight_config);

    return ret;
}
/* HS03s code for DEVAL5625-1829 by chenjun at 2021/07/22 end */
/* hs03s code for CAM-AL5625-01-247 by lisizhou at 2021/06/16 end */

/* hs03s code for CAM-AL5625-01-247 by lisizhou at 2021/06/25 start */
/******************************************************************************
 * sgm3785_set_preflash_mode
 *****************************************************************************/
/* flashlight enable function */
static int sgm3785_set_preflash_mode(int level)
{
    int pinType = 0, state = 0;
    int ret;
    /* TODO: wrap enable function */
    pr_info("preflash_mode enter \n");

    pinType = SGM3785_ENF_GPIO_MODE;
    state = SGM3785_GPIO_PINCTRL_PINSTATE_LOW;
    ret = sgm3785_gpio_pinctrl_set(pinType, state);

    pinType = SGM3785_ENM_GPIO_MODE;
    state = SGM3785_GPIO_PINCTRL_PINSTATE_HIGH;
    ret = sgm3785_gpio_pinctrl_set(pinType, state);

    mdelay(5);

    //pwm mode can not work when system suspend
    pinType = SGM3785_ENM_PWM_MODE;
    state = SGM3785_GPIO_PINCTRL_PINSTATE_HIGH;
    ret = sgm3785_gpio_pinctrl_set(pinType, state);

    flashlight_config.PWM_MODE_OLD_REGS.THRESH = 29;
    ret = pwm_set_spec_config(&flashlight_config);

    return ret;
}
/* hs03s code for CAM-AL5625-01-247 by lisizhou at 2021/06/25 end */

/******************************************************************************
 * sgm3785_set_flash_mode
 *****************************************************************************/
/* flashlight enable function */
static int sgm3785_set_flash_mode(int level)
{
    int pinType = 0, state = 0;
        int ret;
    /* TODO: wrap enable function */
    pr_info("flash_mode enter \n");

    pinType = SGM3785_ENM_PWM_MODE;
    state = SGM3785_GPIO_PINCTRL_PINSTATE_HIGH;
    ret = sgm3785_gpio_pinctrl_set(pinType, state);

    flashlight_config.PWM_MODE_OLD_REGS.THRESH = level;
    ret = pwm_set_spec_config(&flashlight_config);
   // mdelay(2);

    pinType = SGM3785_ENF_GPIO_MODE;
    state = SGM3785_GPIO_PINCTRL_PINSTATE_HIGH;
    ret = sgm3785_gpio_pinctrl_set(pinType, state);

    //mdelay(5);

    return ret;
}

/* hs03s code for CAM-AL5625-01-247 by lisizhou at 2021/07/22 start */
/* HS03s code for DEVAL5625-1829 by chenjun at 2021/07/01 start */
/******************************************************************************
 * as6585_set_torch_mode
 *****************************************************************************/
static int as6585_set_torch_mode(int level)
{
    int pinType = 0, state = 0;
    int ret;
    /* TODO: wrap enable function */
    pr_info("torch_mode enter \n");

    if(NULL == torch_suspend_lock){
        torch_suspend_lock = wakeup_source_register(NULL, "Torch suspend wakelock");
        //spin_lock_init(&afc->afc_spin_lock);
        mutex_init(&torch_access_lock);

        mutex_lock(&torch_access_lock);
        __pm_stay_awake(torch_suspend_lock);

        pr_info("torch wake_lock init and locked!");
    }else{
        pr_info("torch wake_lock already init and locked!");
    }

    pinType = SGM3785_ENF_GPIO_MODE;
    state = SGM3785_GPIO_PINCTRL_PINSTATE_LOW;
    ret = sgm3785_gpio_pinctrl_set(pinType, state);

    pinType = SGM3785_ENM_GPIO_MODE;
    state = SGM3785_GPIO_PINCTRL_PINSTATE_HIGH;
    ret = sgm3785_gpio_pinctrl_set(pinType, state);

    mdelay(5);

    //pwm mode can not work when system suspend
    pinType = SGM3785_ENM_PWM_MODE;
    state = SGM3785_GPIO_PINCTRL_PINSTATE_HIGH;
    ret = sgm3785_gpio_pinctrl_set(pinType, state);
    flashlight_config.PWM_MODE_OLD_REGS.THRESH = level * 5;
    ret = pwm_set_spec_config(&flashlight_config);

    return ret;
}
/* HS03s code for DEVAL5625-1829 by chenjun at 2021/07/22 end */

/******************************************************************************
 * as6585_set_preflash_mode
 *****************************************************************************/
static int as6585_set_preflash_mode(int level)
{
    int pinType = 0, state = 0;
    int ret;
    /* TODO: wrap enable function */
    pr_info("preflash_mode enter \n");

    pinType = SGM3785_ENF_GPIO_MODE;
    state = SGM3785_GPIO_PINCTRL_PINSTATE_LOW;
    ret = sgm3785_gpio_pinctrl_set(pinType, state);

    pinType = SGM3785_ENM_GPIO_MODE;
    state = SGM3785_GPIO_PINCTRL_PINSTATE_HIGH;
    ret = sgm3785_gpio_pinctrl_set(pinType, state);

    mdelay(5);

    //pwm mode can not work when system suspend
    pinType = SGM3785_ENM_PWM_MODE;
    state = SGM3785_GPIO_PINCTRL_PINSTATE_HIGH;
    ret = sgm3785_gpio_pinctrl_set(pinType, state);

    flashlight_config.PWM_MODE_OLD_REGS.THRESH = LEVEL_MAX - 4;
    ret = pwm_set_spec_config(&flashlight_config);

    return ret;
}

/******************************************************************************
 * as6585_set_flash_mode
 *****************************************************************************/
static int as6585_set_flash_mode(int level)
{
    int pinType = 0, state = 0;
        int ret;
    /* TODO: wrap enable function */
    pr_info("flash_mode enter \n");

    pinType = SGM3785_ENM_PWM_MODE;
    state = SGM3785_GPIO_PINCTRL_PINSTATE_HIGH;
    ret = sgm3785_gpio_pinctrl_set(pinType, state);

    /* hs03s code for CAM-AL5625-01-247 by lisizhou at 2021/06/30 start */
    flashlight_config.PWM_MODE_OLD_REGS.THRESH = LEVEL_MAX - level - 1;
    /* hs03s code for CAM-AL5625-01-247 by lisizhou at 2021/06/30 end */
    ret = pwm_set_spec_config(&flashlight_config);
   // mdelay(2);

    pinType = SGM3785_ENF_GPIO_MODE;
    state = SGM3785_GPIO_PINCTRL_PINSTATE_HIGH;
    ret = sgm3785_gpio_pinctrl_set(pinType, state);

    //mdelay(5);

    return ret;
}
/* hs03s code for CAM-AL5625-01-247 by lisizhou at 2021/06/25 end */

/* hs03s code for CAM-AL5625-01-247 by lisizhou at 2021/06/16 start */
/* set flashlight level */
static void sgm3785_gpio_set_level(int level)
{
   // int pinType = 0, state = 0;
    /* TODO: wrap set level function */
    pr_info("[flashlight] level = %d",level);

    if (level > LEVEL_MAX) {
        level = LEVEL_MAX;
    }
    current_level = level;
}
/* hs03s code for CAM-AL5625-01-247 by lisizhou at 2021/06/16 end */


/* HS03s code for DEVAL5625-1829 by chenjun at 2021/07/22 start */
/* flashlight disable function */
static int sgm3785_set_shutoff(void)
{
    int pinType = 0, state = 0;
    int ret = 0;
    /* TODO: wrap disable function */
    pr_info("[flashlight] sgm3785_set_shutoff enter\n");

    if(NULL != torch_suspend_lock){
        __pm_relax(torch_suspend_lock);
        mutex_unlock(&torch_access_lock);
        torch_suspend_lock = NULL;
        pr_info("torch wake_lock relax!");
    }else{
        pr_info("torch wake_lock already relax!");
    }

    pinType = SGM3785_ENM_GPIO_MODE;
    state = SGM3785_GPIO_PINCTRL_PINSTATE_LOW;
    ret = sgm3785_gpio_pinctrl_set(pinType, state);

    pinType = SGM3785_ENF_GPIO_MODE;
    state = SGM3785_GPIO_PINCTRL_PINSTATE_LOW;
    ret = sgm3785_gpio_pinctrl_set(pinType, state);

    flashlight_mode = MODE_SHUTOFF;

    return ret;
}
/* HS03s code for DEVAL5625-1829 by chenjun at 2021/07/22 end */

/* flashlight init */
static int sgm3785_gpio_init(void)
{
    /* TODO: wrap init function */
    sgm3785_set_shutoff();
    return 0;
}

/* flashlight uninit */
static int sgm3785_gpio_uninit(void)
{
    /* TODO: wrap uninit function */
    //sgm3785_set_shutoff();
    return 0;
}

/******************************************************************************
 * Timer and work queue
 *****************************************************************************/
static struct hrtimer sgm3785_gpio_timer;
static unsigned int sgm3785_gpio_timeout_ms;


static void sgm3785_gpio_work_disable(struct work_struct *data)
{
    pr_debug("work queue callback\n");
    sgm3785_set_shutoff();
}

static enum hrtimer_restart sgm3785_gpio_timer_func(struct hrtimer *timer)
{
    schedule_work(&sgm3785_gpio_work);
    return HRTIMER_NORESTART;
}


/******************************************************************************
 * Flashlight operations
 *****************************************************************************/
/* hs03s code for CAM-AL5625-01-247 by lisizhou at 2021/06/25 start */
static int sgm3785_gpio_ioctl(unsigned int cmd, unsigned long arg)
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
        sgm3785_gpio_timeout_ms = fl_arg->arg;
        break;
    case FLASH_IOC_SET_DUTY:  //110
        sgm3785_gpio_set_level(fl_arg->arg);
        break;
    case FLASH_IOC_GET_PRE_ON_TIME_MS: //130
        sgm3785_gpio_timeout_ms = fl_arg->arg;
        break;
    case FLASH_IOC_GET_PRE_ON_TIME_MS_DUTY: //131
        sgm3785_gpio_set_level(fl_arg->arg);
        break;
    case FLASH_IOC_PRE_ON:   //125  flash
        flashlight_mode = MODE_FLASH;
        break;
    case FLASH_IOC_SET_SCENARIO :  //215
        flashlight_mode = MODE_TORCH;
        break;
    case FLASH_IOC_IS_CHARGER_READY:  //210 preflash
        flashlight_mode = MODE_PREFLASH;
        break;
    case FLASH_IOC_SET_ONOFF:              //115
        pr_info("timeout_ms = %d, current_level = %d, flashlight_mode= %d, pin status = %d\n",
                sgm3785_gpio_timeout_ms,current_level,flashlight_mode, pin_status);
        if (fl_arg->arg == 1 && pin_status == 0) {
            switch(flashlight_mode) {
                case MODE_FLASH: {
                    if (sgm3785_gpio_timeout_ms == 0) {
                        sgm3785_gpio_timeout_ms = 100;
                    }
                    s = sgm3785_gpio_timeout_ms / 1000;
                    ns = sgm3785_gpio_timeout_ms % 1000 * 1000000;
                    ktime = ktime_set(s, ns);
                    hrtimer_start(&sgm3785_gpio_timer, ktime,
                            HRTIMER_MODE_REL);

                    as6585_set_flash_mode(current_level);
                }
                    break;
                case MODE_PREFLASH:
                    as6585_set_preflash_mode(current_level);
                    break;
                case MODE_SHUTOFF:
                case MODE_TORCH:
                    as6585_set_torch_mode(current_level);
                    break;
                default:
                    break;
            }
        }else if(fl_arg->arg == 1 && pin_status == 1){
            switch(flashlight_mode) {
                case MODE_FLASH: {
                    if (sgm3785_gpio_timeout_ms == 0) {
                        sgm3785_gpio_timeout_ms = 100;
                    }
                    s = sgm3785_gpio_timeout_ms / 1000;
                    ns = sgm3785_gpio_timeout_ms % 1000 * 1000000;
                    ktime = ktime_set(s, ns);
                    hrtimer_start(&sgm3785_gpio_timer, ktime,
                            HRTIMER_MODE_REL);

                    sgm3785_set_flash_mode(current_level);
                }
                    break;
                /* hs03s code for CAM-AL5625-01-247 by lisizhou at 2021/06/16 start */
                case MODE_PREFLASH:
                    sgm3785_set_preflash_mode(current_level);
                    break;
                /* hs03s code for CAM-AL5625-01-247 by lisizhou at 2021/06/16 end */
                case MODE_SHUTOFF:
                case MODE_TORCH:
                    sgm3785_set_torch_mode(current_level);
                    break;
                default:
                    break;
            }
        }
        else {
            pr_info("fl_arg->arg == %d",fl_arg->arg);
            sgm3785_set_shutoff();
            hrtimer_cancel(&sgm3785_gpio_timer);
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

static int sgm3785_gpio_open(void)
{
    /* Move to set driver for saving power */
    return 0;
}

static int sgm3785_gpio_release(void)
{
    /* Move to set driver for saving power */
    return 0;
}

static int sgm3785_gpio_set_driver(int set)
{
    int ret = 0;
    /* set chip and usage count */
    pr_info("sgm3785_gpio_set_driver enter \n");
    mutex_lock(&sgm3785_gpio_mutex);
    if (set) {
        if (!use_count)
            ret = sgm3785_gpio_init();
        use_count++;
        pr_debug("Set driver: %d\n", use_count);
    } else {
        use_count--;
        if (!use_count)
            ret = sgm3785_gpio_uninit();
        if (use_count < 0)
            use_count = 0;
        pr_debug("Unset driver: %d\n", use_count);
    }
    mutex_unlock(&sgm3785_gpio_mutex);

    return ret;
}

static ssize_t sgm3785_gpio_strobe_store(struct flashlight_arg arg)
{
    pr_info("sgm3785_gpio_strobe_store enter \n");
    sgm3785_gpio_set_driver(1);
    sgm3785_gpio_set_level(arg.level);
    sgm3785_gpio_timeout_ms = 0;
    sgm3785_set_torch_mode(0);
    msleep(arg.dur);
    sgm3785_set_shutoff();
    sgm3785_gpio_set_driver(0);

    return 0;
}

static struct flashlight_operations sgm3785_gpio_ops = {
    sgm3785_gpio_open,
    sgm3785_gpio_release,
    sgm3785_gpio_ioctl,
    sgm3785_gpio_strobe_store,
    sgm3785_gpio_set_driver
};


/******************************************************************************
 * Platform device and driver
 *****************************************************************************/
static int sgm3785_gpio_chip_init(void)
{
    /* NOTE: Chip initialication move to "set driver" for power saving.
     * sgm3785_gpio_init();
     */

    return 0;
}

static int sgm3785_gpio_parse_dt(struct device *dev,
        struct sgm3785_gpio_platform_data *pdata)
{
    struct device_node *np, *cnp;
    u32 decouple = 0;
    int i = 0;

    if (!dev || !dev->of_node || !pdata)
        return -ENODEV;

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
                SGM3785_GPIO_NAME);
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

static int sgm3785_gpio_probe(struct platform_device *pdev)
{
    struct sgm3785_gpio_platform_data *pdata = dev_get_platdata(&pdev->dev);
    int err;
    int i;

    pr_info("Probe start.\n");

    /* hs03s code for CAM-AL5625-01-247 by lisizhou at 2021/06/25 start */
    gpio_request(pin_id + base,"id_pin");
    gpio_direction_input(pin_id + base);
    pin_status = gpio_get_value(pin_id + base);
    pr_info("flash pin status = %d", pin_status);
    /* hs03s code for CAM-AL5625-01-247 by lisizhou at 2021/06/25 end */

    /* init pinctrl */
    if (sgm3785_gpio_pinctrl_init(pdev)) {
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
        err = sgm3785_gpio_parse_dt(&pdev->dev, pdata);
        if (err)
            goto err;
    }

    /* init work queue */
    INIT_WORK(&sgm3785_gpio_work, sgm3785_gpio_work_disable);

    /* init timer */
    hrtimer_init(&sgm3785_gpio_timer, CLOCK_MONOTONIC, HRTIMER_MODE_REL);
    sgm3785_gpio_timer.function = sgm3785_gpio_timer_func;
    sgm3785_gpio_timeout_ms = 100;

    /* init chip hw */
    sgm3785_gpio_chip_init();

    /* clear usage count */
    use_count = 0;

    /* register flashlight device */
    if (pdata->channel_num) {
        for (i = 0; i < pdata->channel_num; i++)
            if (flashlight_dev_register_by_device_id(
                        &pdata->dev_id[i],
                        &sgm3785_gpio_ops)) {
                err = -EFAULT;
                goto err;
            }
    } else {
        if (flashlight_dev_register(SGM3785_GPIO_NAME, &sgm3785_gpio_ops)) {
            err = -EFAULT;
            goto err;
        }
    }

    pr_info("Probe done.\n");

    return 0;
err:
    return err;
}

static int sgm3785_gpio_remove(struct platform_device *pdev)
{
    struct sgm3785_gpio_platform_data *pdata = dev_get_platdata(&pdev->dev);
    int i;

    pr_debug("Remove start.\n");

    pdev->dev.platform_data = NULL;

    /* unregister flashlight device */
    if (pdata && pdata->channel_num)
        for (i = 0; i < pdata->channel_num; i++)
            flashlight_dev_unregister_by_device_id(
                    &pdata->dev_id[i]);
    else
        flashlight_dev_unregister(SGM3785_GPIO_NAME);

    /* flush work queue */
    flush_work(&sgm3785_gpio_work);

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
        .name = SGM3785_GPIO_NAME,
        .id = 0,
        .dev = {}
    },
    {}
};
MODULE_DEVICE_TABLE(platform, sgm3785_gpio_platform_device);
#endif

static struct platform_driver sgm3785_gpio_platform_driver = {
    .probe = sgm3785_gpio_probe,
    .remove = sgm3785_gpio_remove,
    .driver = {
        .name = SGM3785_GPIO_NAME,
        .owner = THIS_MODULE,
#ifdef CONFIG_OF
        .of_match_table = sgm3785_gpio_of_match,
#endif
    },
};

static int __init flashlight_sgm3785_gpio_init(void)
{
    int ret;

    pr_debug("Init start.\n");

#ifndef CONFIG_OF
    ret = platform_device_register(&sgm3785_gpio_platform_device);
    if (ret) {
        pr_err("Failed to register platform device\n");
        return ret;
    }
#endif

    ret = platform_driver_register(&sgm3785_gpio_platform_driver);
    if (ret) {
        pr_err("Failed to register platform driver\n");
        return ret;
    }

    pr_debug("Init done.\n");

    return 0;
}

static void __exit flashlight_sgm3785_gpio_exit(void)
{
    pr_debug("Exit start.\n");

    platform_driver_unregister(&sgm3785_gpio_platform_driver);

    pr_debug("Exit done.\n");
}

module_init(flashlight_sgm3785_gpio_init);
module_exit(flashlight_sgm3785_gpio_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Focus.li <lisizhou@huaqin.com>");
MODULE_DESCRIPTION("HQ SGM3785 GPIO Driver");