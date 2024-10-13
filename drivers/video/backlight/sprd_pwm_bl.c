/*
 * Copyright (C) 2012 Spreadtrum Communications Inc.
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/backlight.h>
#include <linux/fb.h>
#include <linux/io.h>
#include <linux/clk.h>
#include <linux/earlysuspend.h>
#include <linux/gpio.h>
#include <linux/err.h>

#include <mach/hardware.h>
#include <mach/sci_glb_regs.h>
#include <mach/adi.h>
#include <mach/arch_misc.h>
#include <linux/of.h>

#include <linux/sprd_pwm_bl.h>

//#define SPRD_PWM_BL_DBG
#ifdef SPRD_PWM_BL_DBG
#define ENTER printk(KERN_INFO "[SPRD_PWM_BL_DBG] func: %s  line: %04d\n", __func__, __LINE__);
#define PRINT_DBG(x...)  printk(KERN_INFO "[SPRD_PWM_BL_DBG] " x)
#define PRINT_INFO(x...)  printk(KERN_INFO "[SPRD_PWM_BL_INFO] " x)
#define PRINT_WARN(x...)  printk(KERN_INFO "[SPRD_PWM_BL_WARN] " x)
#define PRINT_ERR(format,x...)  printk(KERN_ERR "[SPRD_PWM_BL_ERR] func: %s  line: %04d  info: " format, __func__, __LINE__, ## x)
#else
#define ENTER
#define PRINT_DBG(x...)
#define PRINT_INFO(x...)  printk(KERN_INFO "[SPRD_PWM_BL_INFO] " x)
#define PRINT_WARN(x...)  printk(KERN_INFO "[SPRD_PWM_BL_WARN] " x)
#define PRINT_ERR(format,x...)  printk(KERN_ERR "[SPRD_PWM_BL_ERR] func: %s  line: %04d  info: " format, __func__, __LINE__, ## x)
#endif

/* register definitions */
#define        PWM_PRESCALE    (0x0000)
#define        PWM_CNT         (0x0004)
#define        PWM_TONE_DIV    (0x0008)
#define        PWM_PAT_LOW     (0x000C)
#define        PWM_PAT_HIG     (0x0010)

#define        PWM_ENABLE      (1 << 8)
#define        PWM_DUTY            (1 << 8)
#define        PWM_MOD             0
#define        PWM_DIV             0x190
#define        PWM_LOW             0xffff
#define        PWM_HIG             0xffff
#define        PWM_SCALE       1
#define        PWM2_SCALE           0x0
#define        PWM_REG_MSK     0xffff
#define        PWM_MOD_MAX     0xff
#define        PWM_DUTY_MAX     0x7f

struct sprd_pwm_bl_device_data {
        int suspend;
        int brightness_max;
        int brightness_min;
        int pwm_index;
        int gpio_ctrl_pin;
        int gpio_active_level;
        struct clk *clk;
        struct backlight_device *bldev;
        struct early_suspend sprd_early_suspend_desc;
};

static struct sprd_pwm_bl_device_data sprd_pwm_bl = {
        .brightness_max = 0,
        .brightness_min = 0,
        .pwm_index = 0,
        .gpio_ctrl_pin = -1,
        .gpio_active_level = 0,
};

static DEFINE_SPINLOCK(clock_en_lock);
static void pwm_clk_en(int enable)
{
        unsigned long spin_lock_flags;
        static int current_state = 0;

        spin_lock_irqsave(&clock_en_lock, spin_lock_flags);
        if (1 == enable) {
                if (0 == current_state) {
                        clk_prepare_enable(sprd_pwm_bl.clk);
                        current_state = 1;
                }
        } else {
                if (1 == current_state) {
                        clk_disable_unprepare(sprd_pwm_bl.clk);
                        current_state = 0;
                }
        }
        spin_unlock_irqrestore(&clock_en_lock, spin_lock_flags);

        return;
}

static inline uint32_t pwm_read(int index, uint32_t reg)
{
        //this is the D-die PWM controller, here we use PWM2(index=3) for external backlight control.
        return __raw_readl(SPRD_PWM_BASE + index * 0x20 + reg);
}

static void pwm_write(int index, uint32_t value, uint32_t reg)
{
        //this is the D-die PWM controller, here we use PWM2(index=3) for external backlight control.
        __raw_writel(value, SPRD_PWM_BASE + index * 0x20 + reg);
}

//sprd used PWM2(mapped to gpio190) for external backlight control.
static int sprd_pwm_bl_update_status(struct backlight_device *bldev)
{
        u32 led_level;

        if ((bldev->props.state & (BL_CORE_SUSPENDED | BL_CORE_FBBLANK)) ||
            bldev->props.power != FB_BLANK_UNBLANK ||
            sprd_pwm_bl.suspend ||
            bldev->props.brightness == 0) {
                /* disable backlight */
                pwm_write(sprd_pwm_bl.pwm_index, 0, PWM_PRESCALE);
                pwm_clk_en(0);
                PRINT_INFO("disabled\n");
        } else {
                led_level = bldev->props.brightness & PWM_MOD_MAX;
                //led_level = (led_level * (PWM_DUTY_MAX+1) / (PWM_MOD_MAX+1)) + 10;
                led_level = led_level * 67 / 100;
                if(led_level < 8)
                        led_level = 8;
                PRINT_INFO("brightness = %d, led_level = %d\n", bldev->props.brightness, led_level);
                pwm_clk_en(1);
                pwm_write(sprd_pwm_bl.pwm_index, PWM2_SCALE, PWM_PRESCALE);
                pwm_write(sprd_pwm_bl.pwm_index, (led_level << 8) | PWM_MOD_MAX, PWM_CNT);
                pwm_write(sprd_pwm_bl.pwm_index, PWM_REG_MSK, PWM_PAT_LOW);
                pwm_write(sprd_pwm_bl.pwm_index, PWM_REG_MSK, PWM_PAT_HIG);
                pwm_write(sprd_pwm_bl.pwm_index, PWM_ENABLE, PWM_PRESCALE);
        }
        return 0;
}

static int sprd_pwm_bl_get_brightness(struct backlight_device *bldev)
{
        return (pwm_read(sprd_pwm_bl.pwm_index, PWM_CNT) >> 8) & PWM_MOD_MAX;
}

static const struct backlight_ops sprd_backlight_pwm_ops = {
        .update_status = sprd_pwm_bl_update_status,
        .get_brightness = sprd_pwm_bl_get_brightness,
};

void set_gpio_ctrll_pin_state(int on)
{
        on = on ? sprd_pwm_bl.gpio_active_level : !sprd_pwm_bl.gpio_active_level;
        gpio_direction_output(sprd_pwm_bl.gpio_ctrl_pin,on);
        PRINT_INFO("set_gpio_ctrll_pin_state=%d\n", on);
}

#ifdef CONFIG_EARLYSUSPEND
static void sprd_backlight_earlysuspend(struct early_suspend *h)
{
        if(-1 != sprd_pwm_bl.gpio_ctrl_pin)
                set_gpio_ctrll_pin_state(0);

        sprd_pwm_bl.suspend = 1;
        sprd_pwm_bl.bldev->ops->update_status(sprd_pwm_bl.bldev);
        PRINT_INFO("early suspend\n");
}

static void sprd_backlight_lateresume(struct early_suspend *h)
{
        if(-1 != sprd_pwm_bl.gpio_ctrl_pin)
                set_gpio_ctrll_pin_state(1);

        sprd_pwm_bl.suspend = 0;
        sprd_pwm_bl.bldev->ops->update_status(sprd_pwm_bl.bldev);
        PRINT_INFO("late resume\n");
}
#endif

#ifdef CONFIG_OF
static struct sprd_pwm_bl_platform_data *sprd_pwm_bl_parse_dt(struct platform_device *pdev)
{
        int ret = -1;
        struct device_node *np = pdev->dev.of_node;
        struct sprd_pwm_bl_platform_data* pdata = NULL;

        pdata = kzalloc(sizeof(*pdata), GFP_KERNEL);
        if (!pdata) {
                PRINT_ERR("failed to allocate pdata!\n");
                return NULL;
        }

        ret = of_property_read_u32(np, "pwm_index", &pdata->pwm_index);
        if(ret) {
                PRINT_ERR("failed to get pwm_index\n");
                goto fail;
        }

        ret = of_property_read_u32(np, "brightness_max", &pdata->brightness_max);
        if(ret) {
                pdata->brightness_max = 255;
                PRINT_WARN("failed to get brightness_max\n");
        }

        ret = of_property_read_u32(np, "brightness_min", &pdata->brightness_min);
        if(ret) {
                pdata->brightness_min = 0;
                PRINT_WARN("failed to get brightness_min\n");
        }

        ret = of_property_read_u32(np, "gpio_ctrl_pin", &pdata->gpio_ctrl_pin);
        if(ret) {
                pdata->gpio_ctrl_pin = -1;
                PRINT_WARN("failed to get gpio_ctrl_pin\n");
        }

        ret = of_property_read_u32(np, "gpio_active_level", &pdata->gpio_active_level);
        if(ret) {
                pdata->gpio_active_level = 0;
                PRINT_WARN("failed to get gpio_active_level\n");
        }

        return pdata;
fail:
        kfree(pdata);
        return NULL;
}
#endif

static int sprd_backlight_probe(struct platform_device *pdev)
{
        struct backlight_properties props;
        struct backlight_device *bldev;
        struct clk* ext_26m = NULL;
        char pwm_clk_name[32];
        struct sprd_pwm_bl_platform_data* pdata = NULL;
        int ret = -1;

#ifdef CONFIG_OF
        struct device_node *np = pdev->dev.of_node;
        if(np) {
                pdata = sprd_pwm_bl_parse_dt(pdev);
                if (pdata == NULL) {
                        PRINT_ERR("get dts data failed!\n");
                        return -ENODEV;
                }
        } else {
                PRINT_ERR("dev.of_node is NULL!\n");
                return -ENODEV;
        }
#else
        pdata = pdev->dev.platform_data;
        if (pdata == NULL) {
                PRINT_ERR("No platform data!\n");
                return -ENODEV;
        }
#endif

        if(pdata->gpio_ctrl_pin > 0) {
                sprd_pwm_bl.gpio_ctrl_pin = pdata->gpio_ctrl_pin;
                sprd_pwm_bl.gpio_active_level= pdata->gpio_active_level;
                ret = gpio_request(sprd_pwm_bl.gpio_ctrl_pin, "sprd_pwm_bl_gpio_ctrl_pin");
                if (ret) {
                        PRINT_ERR("request gpio_ctrl_pin error!\n");
                        return -EIO;
                }
                set_gpio_ctrll_pin_state(1);
        } else {
                sprd_pwm_bl.gpio_ctrl_pin = -1;
        }
        PRINT_INFO("gpio_ctrl_pin=%d\n", sprd_pwm_bl.gpio_ctrl_pin);

        sprd_pwm_bl.brightness_max= pdata->brightness_max;
        sprd_pwm_bl.brightness_min= pdata->brightness_min;
        sprd_pwm_bl.pwm_index = pdata->pwm_index;

        /*fixme, the pwm's clk name must like this:clk_pwmx*/
        sprintf(pwm_clk_name, "%s%d", "clk_pwm", sprd_pwm_bl.pwm_index);
        sprd_pwm_bl.clk = clk_get(&pdev->dev, pwm_clk_name);
        if (IS_ERR(sprd_pwm_bl.clk)) {
                PRINT_ERR("get pwm's clk failed!\n");
                return -ENODEV;
        }

        ext_26m = clk_get(NULL, "ext_26m");
        if (IS_ERR(ext_26m)) {
                PRINT_ERR("get pwm's ext_26m failed!\n");
                return -ENODEV;
        }

        clk_set_parent(sprd_pwm_bl.clk,ext_26m);

        PRINT_INFO("PWM%d is used for brightness control (external backlight controller)\n", sprd_pwm_bl.pwm_index);

        memset(&props, 0, sizeof(struct backlight_properties));
        props.max_brightness = PWM_MOD_MAX;
        props.type = BACKLIGHT_RAW;
        /*the default brightness = 1/2 max brightness */
        props.brightness = PWM_MOD_MAX >> 1;
        props.power = FB_BLANK_UNBLANK;

        bldev = backlight_device_register(
                        "sprd_backlight", &pdev->dev,
                        &sprd_pwm_bl, &sprd_backlight_pwm_ops, &props);

        if (IS_ERR(bldev)) {
                PRINT_ERR("regist backlight device failed!\n");
                return -ENOMEM;
        }

        sprd_pwm_bl.bldev = bldev;
        platform_set_drvdata(pdev, bldev);
        bldev->ops->update_status(bldev);

#ifdef CONFIG_EARLYSUSPEND
        sprd_pwm_bl.sprd_early_suspend_desc.level = EARLY_SUSPEND_LEVEL_BLANK_SCREEN;
        sprd_pwm_bl.sprd_early_suspend_desc.suspend = sprd_backlight_earlysuspend;
        sprd_pwm_bl.sprd_early_suspend_desc.resume = sprd_backlight_lateresume;
        register_early_suspend(&sprd_pwm_bl.sprd_early_suspend_desc);
#endif

        PRINT_INFO("probe success\n");
        return 0;
}

static int sprd_backlight_remove(struct platform_device *pdev)
{
        struct backlight_device *bldev;

        bldev = platform_get_drvdata(pdev);
        bldev->props.power = FB_BLANK_UNBLANK;
        bldev->props.brightness = PWM_MOD_MAX;

        backlight_update_status(bldev);
        backlight_device_unregister(bldev);
        platform_set_drvdata(pdev, NULL);

#ifdef CONFIG_EARLYSUSPEND
        unregister_early_suspend(&sprd_pwm_bl.sprd_early_suspend_desc);
#endif
        return 0;
}

static void sprd_backlight_shutdown(struct platform_device *pdev)
{
        struct backlight_device *bldev;

        bldev = platform_get_drvdata(pdev);
        bldev->props.brightness = 0;
        backlight_update_status(bldev);
}


#ifdef CONFIG_PM
static int sprd_backlight_suspend(struct platform_device *pdev,
                                  pm_message_t state)
{
        return 0;
}

static int sprd_backlight_resume(struct platform_device *pdev)
{
        return 0;
}
#else
#define sprd_backlight_suspend NULL
#define sprd_backlight_resume NULL
#endif

static const struct of_device_id backlight_of_match[] = {
        { .compatible = "sprd,sprd_pwm_bl", },
        { }
};
static struct platform_driver sprd_backlight_driver = {
        .probe = sprd_backlight_probe,
        .remove = sprd_backlight_remove,
        .suspend = sprd_backlight_suspend,
        .resume = sprd_backlight_resume,
        .shutdown = sprd_backlight_shutdown,
        .driver = {
                .name = "sprd_pwm_bl",
                .owner = THIS_MODULE,
                .of_match_table = backlight_of_match,
        },
};

extern int lcd_panel_cabc_pwm_backlight;
static int __init sprd_backlight_init(void)
{
	if (!lcd_panel_cabc_pwm_backlight)
        return platform_driver_register(&sprd_backlight_driver);
	else
		return 0;
}

static void __exit sprd_backlight_exit(void)
{
        platform_driver_unregister(&sprd_backlight_driver);
}

module_init(sprd_backlight_init);
module_exit(sprd_backlight_exit);

MODULE_DESCRIPTION("Spreadtrum backlight Driver");
