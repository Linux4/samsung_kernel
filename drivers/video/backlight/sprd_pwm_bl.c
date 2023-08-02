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
#include <linux/platform_data/gen-panel-bl.h>
#include <linux/delay.h>

//#define SPRD_PWM_BL_DBG
#ifdef SPRD_PWM_BL_DBG
#define ENTER printk(KERN_INFO "[BACKLIGHT] func: %s  line: %04d\n", __func__, __LINE__);
#define PRINT_DBG(x...)  printk(KERN_INFO "[BACKLIGHT] " x)
#define PRINT_INFO(x...)  printk(KERN_INFO "[BACKLIGHT] " x)
#define PRINT_WARN(x...)  printk(KERN_INFO "[BACKLIGHT] " x)
#define PRINT_ERR(format,x...)  printk(KERN_ERR "[BACKLIGHT] func: %s  line: %04d  info: " format, __func__, __LINE__, ## x)
#else
#define ENTER
#define PRINT_DBG(x...)
#define PRINT_INFO(x...)  printk(KERN_INFO "[BACKLIGHT] " x)
#define PRINT_WARN(x...)  printk(KERN_INFO "[BACKLIGHT] " x)
#define PRINT_ERR(format,x...)  printk(KERN_ERR "[BACKLIGHT] func: %s  line: %04d  info: " format, __func__, __LINE__, ## x)
#endif

/* register definitions */
#define        PWM_PRESCALE    (0x0000)
#define        PWM_CNT         (0x0004)
#define        PWM_TONE_DIV    (0x0008)
#define        PWM_PAT_LOW     (0x000C)
#define        PWM_PAT_HIG     (0x0010)

#define        PWM_ENABLE      ((1 << 8)|(1 << 2))
#define        PWM_DUTY            (1 << 8)
#define        PWM_MOD             0
#define        PWM_DIV             0x190
#define        PWM_LOW             0xffff
#define        PWM_HIG             0xffff
#define        PWM_SCALE       1
#define        PWM2_SCALE           0x0
#define        PWM_REG_MSK     0xffff
#define        PWM_MOD_MAX     0xff
#define        PWM_DUTY_MAX     0x30
#define        DEFAULT_BRIHTNESS     143

struct sprd_pwm_bl_platform_data {
		const char *name;
        int pwm_index;
        int gpio_ctrl_pin;
        int gpio_active_level;
	struct brt_value range[MAX_BRT_VALUE_IDX];
};

struct sprd_pwm_bl_device_data {
	const char *name;
        int suspend;
        int brightness_max;
        int brightness_min;
        int pwm_index;
        int gpio_ctrl_pin;
        int gpio_active_level;
        struct clk *clk;
        struct backlight_device *bldev;
	struct brt_value range[MAX_BRT_VALUE_IDX];
	int current_brightness;
	int prev_tune_level;
};

static struct sprd_pwm_bl_device_data* sprd_pwm_bl;

static DEFINE_SPINLOCK(clock_en_lock);
static void pwm_clk_en(int enable)
{
        static int current_state = 0;

        if (1 == enable) {
                if (0 == current_state) {
                        clk_prepare_enable(sprd_pwm_bl->clk);
                        current_state = 1;
                }
        } else {
                if (1 == current_state) {
                        clk_disable_unprepare(sprd_pwm_bl->clk);
                        current_state = 0;
                }
        }
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

static int sprd_pwm_get_tune_level(struct sprd_pwm_bl_device_data *bl_info,
		int brightness)
{
	int tune_level = 0, idx;
	struct brt_value *range = bl_info->range;

	if (unlikely(!range)) {
		pr_err("[BACKLIGHT] %s: brightness range not exist!\n", __func__);
		return -EINVAL;
	}

	if (brightness > range[BRT_VALUE_MAX].brightness ||
			brightness < 0) {
		pr_err("[BACKLIGHT] %s: out of range (%d)\n", __func__, brightness);
		return -EINVAL;
	}

	for (idx = 0; idx < MAX_BRT_VALUE_IDX; idx++)
		if (brightness <= range[idx].brightness)
			break;

	if (idx == MAX_BRT_VALUE_IDX) {
		pr_err("[BACKLIGHT] %s: out of brt_value table (%d)\n",
				__func__, brightness);
		return -EINVAL;
	}

	if (idx <= BRT_VALUE_MIN)
		tune_level = range[idx].tune_level;
	else
		tune_level = range[idx].tune_level -
			(range[idx].brightness - brightness) *
			(range[idx].tune_level - range[idx - 1].tune_level) /
			(range[idx].brightness - range[idx - 1].brightness);

	return tune_level;
}

//sprd used PWM2(mapped to gpio190) for external backlight control.
static int sprd_pwm_bl_update_status(struct backlight_device *bldev)
{
        u32 tune_level;
        unsigned long spin_lock_flags;
	struct brt_value *range = sprd_pwm_bl->range;

	tune_level = sprd_pwm_get_tune_level(sprd_pwm_bl, bldev->props.brightness);

	if((range[BRT_VALUE_DIM].brightness) == (range[BRT_VALUE_MIN].brightness)){
		if (sprd_pwm_bl->prev_tune_level == tune_level) {
			PRINT_INFO("%s, already set tune_level(%d)\n",
					__func__, tune_level);
		}
	}
	else{
		if ((sprd_pwm_bl->prev_tune_level < range[BRT_VALUE_DIM].tune_level)&&(tune_level==range[BRT_VALUE_DIM].tune_level)){
			PRINT_INFO("%s, already set tune_level(%d)\n",
				__func__, tune_level);
		}
	}

        if ((bldev->props.state & (BL_CORE_SUSPENDED | BL_CORE_FBBLANK)) ||
            bldev->props.power != FB_BLANK_UNBLANK ||
            sprd_pwm_bl->suspend ||
            bldev->props.brightness == 0) {
                /* disable backlight */
                pwm_write(sprd_pwm_bl->pwm_index, 0, PWM_PRESCALE);
                PRINT_INFO("disabled\n");
        } else {
		spin_lock_irqsave(&clock_en_lock, spin_lock_flags);
                PRINT_INFO("tune_level : %d\n", tune_level);

                pwm_write(sprd_pwm_bl->pwm_index, (tune_level << 8) | PWM_MOD_MAX, PWM_CNT);
                pwm_write(sprd_pwm_bl->pwm_index, PWM_REG_MSK, PWM_PAT_LOW);
                pwm_write(sprd_pwm_bl->pwm_index, PWM_REG_MSK, PWM_PAT_HIG);
                pwm_write(sprd_pwm_bl->pwm_index, PWM_ENABLE, PWM_PRESCALE);

		spin_unlock_irqrestore(&clock_en_lock, spin_lock_flags);

        }
        return 0;
}

static int sprd_pwm_bl_get_brightness(struct backlight_device *bldev)
{
        return (pwm_read(sprd_pwm_bl->pwm_index, PWM_CNT) >> 8) & PWM_MOD_MAX;
}

static const struct backlight_ops sprd_backlight_pwm_ops = {
        .update_status = sprd_pwm_bl_update_status,
        .get_brightness = sprd_pwm_bl_get_brightness,
};

void set_gpio_ctrll_pin_state(int on)
{
        on = on ? sprd_pwm_bl->gpio_active_level : !sprd_pwm_bl->gpio_active_level;
        gpio_direction_output(sprd_pwm_bl->gpio_ctrl_pin,on);
        PRINT_INFO("set_gpio_ctrll_pin_state=%d\n", on);
}

#ifdef CONFIG_OF
static struct sprd_pwm_bl_device_data *sprd_pwm_bl_parse_dt(struct platform_device *pdev)
{
        int ret = -1;
        struct device_node *np = pdev->dev.of_node;
        struct sprd_pwm_bl_device_data* pdata = NULL;
	int arr[MAX_BRT_VALUE_IDX * 2], i;

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

	ret = of_property_read_string(np, "backlight-name", &pdata->name);
        if(ret) {
                PRINT_ERR("failed to get backlight-name\n");
                goto fail;
        }

	ret = of_property_read_u32_array(np, "backlight-brt-range", arr, MAX_BRT_VALUE_IDX * 2);
	for (i = 0; i < MAX_BRT_VALUE_IDX; i++) {
		pdata->range[i].brightness = arr[i * 2];
		pdata->range[i].tune_level = arr[i * 2 + 1];
	}

	PRINT_INFO("backlight device : %s\n", pdata->name);
	PRINT_INFO("[BRT_VALUE_OFF] brightness(%d), tune_level(%d)\n",
				pdata->range[BRT_VALUE_OFF].brightness,
				pdata->range[BRT_VALUE_OFF].tune_level);
	PRINT_INFO("[BRT_VALUE_MIN] brightness(%d), tune_level(%d)\n",
				pdata->range[BRT_VALUE_MIN].brightness,
				pdata->range[BRT_VALUE_MIN].tune_level);
	PRINT_INFO("[BRT_VALUE_DIM] brightness(%d), tune_level(%d)\n",
				pdata->range[BRT_VALUE_DIM].brightness,
				pdata->range[BRT_VALUE_DIM].tune_level);
	PRINT_INFO("[BRT_VALUE_DEF] brightness(%d), tune_level(%d)\n",
				pdata->range[BRT_VALUE_DEF].brightness,
				pdata->range[BRT_VALUE_DEF].tune_level);
	PRINT_INFO("[BRT_VALUE_MAX] brightness(%d), tune_level(%d)\n",
				pdata->range[BRT_VALUE_MAX].brightness,
				pdata->range[BRT_VALUE_MAX].tune_level);

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
        struct sprd_pwm_bl_device_data* pdata;
        int ret = -1, err=0;
	PRINT_INFO("%s : %d\n", __func__, __LINE__);

#ifdef CONFIG_OF
        struct device_node *np = pdev->dev.of_node;
        if(np) {
                pdata = sprd_pwm_bl_parse_dt(pdev);
                if (pdata == NULL) {
                        PRINT_ERR("get dts data failed!\n");
                        err = -ENODEV;
			goto fail;
                }
        } else {
                PRINT_ERR("dev.of_node is NULL!\n");
                err = -ENODEV;
		goto fail;
        }
#else
        pdata = pdev->dev.platform_data;
        if (pdata == NULL) {
                PRINT_ERR("No platform data!\n");
                err = -ENODEV;
		goto fail;
        }
#endif

        /*fixme, the pwm's clk name must like this:clk_pwmx*/
        sprintf(pwm_clk_name, "%s%d", "clk_pwm", pdata->pwm_index);
        pdata->clk = clk_get(&pdev->dev, pwm_clk_name);
        if (IS_ERR(pdata->clk)) {
                PRINT_ERR("get pwm's clk failed!\n");
                err = -ENODEV;
		goto fail;
        }

        ext_26m = clk_get(NULL, "ext_26m");
        if (IS_ERR(ext_26m)) {
                PRINT_ERR("get pwm's ext_26m failed!\n");
                err = -ENODEV;
		goto fail;
        }

        clk_set_parent(pdata->clk,ext_26m);

        PRINT_INFO("PWM%d is used for brightness control (external backlight controller)\n", pdata->pwm_index);

        memset(&props, 0, sizeof(struct backlight_properties));
        props.max_brightness = PWM_MOD_MAX;
        props.type = BACKLIGHT_RAW;
        props.brightness = DEFAULT_BRIHTNESS;
        props.power = FB_BLANK_UNBLANK;

        bldev = backlight_device_register(
                        "panel", &pdev->dev,
                        &pdata, &sprd_backlight_pwm_ops, &props);

        if (IS_ERR(bldev)) {
                PRINT_ERR("regist backlight device failed!\n");
                err = -ENOMEM;
		goto fail;
        }

        pdata->bldev = bldev;

	sprd_pwm_bl = pdata;

	pwm_clk_en(1);

        if(pdata->gpio_ctrl_pin > 0) {
            ret = gpio_request(pdata->gpio_ctrl_pin, "sprd_pwm_bl_gpio_ctrl_pin");
                if (ret) {
                        PRINT_ERR("request gpio_ctrl_pin error!\n");
			kfree(pdata);
                        err = -EIO;
			goto fail;
                }
            set_gpio_ctrll_pin_state(1);
        } else {
            pdata->gpio_ctrl_pin = -1;
        }
	PRINT_INFO("gpio_ctrl_pin=%d\n", pdata->gpio_ctrl_pin);

	pm_runtime_enable(&pdev->dev);
	platform_set_drvdata(pdev, bldev);
	pm_runtime_get_sync(&pdev->dev);

        PRINT_INFO("probe success\n");
        return 0;

fail:
	kfree(pdata);
	return err;
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

        return 0;
}

static void sprd_backlight_shutdown(struct platform_device *pdev)
{
        struct backlight_device *bldev;

        bldev = platform_get_drvdata(pdev);
        bldev->props.brightness = 0;
        backlight_update_status(bldev);
}

#if defined(CONFIG_PM_RUNTIME) || defined(CONFIG_PM_SLEEP)
static int gen_panel_backlight_runtime_suspend(struct device *dev)
{
	pwm_clk_en(0);

	sprd_pwm_bl->suspend = 1;
	sprd_pwm_bl->bldev->ops->update_status(sprd_pwm_bl->bldev);

	PRINT_INFO("early suspend\n");

	return 0;
}

static int gen_panel_backlight_runtime_resume(struct device *dev)
{
	sprd_pwm_bl->suspend = 0;
	sprd_pwm_bl->bldev->ops->update_status(sprd_pwm_bl->bldev);
	pwm_clk_en(1);

	PRINT_INFO("late resume\n");

        return 0;
}
#endif

static const struct of_device_id backlight_of_match[] = {
        { .compatible = "sprd,sprd_pwm_bl", },
        { }
};
static struct platform_driver sprd_backlight_driver = {
        .probe = sprd_backlight_probe,
        .remove = sprd_backlight_remove,
        .shutdown = sprd_backlight_shutdown,
        .driver = {
                .name = "sprd_pwm_bl",
                .owner = THIS_MODULE,
                .of_match_table = backlight_of_match,
        },
};

static int __init sprd_backlight_init(void)
{
        return platform_driver_register(&sprd_backlight_driver);
}

static void __exit sprd_backlight_exit(void)
{
        platform_driver_unregister(&sprd_backlight_driver);
}

module_init(sprd_backlight_init);
module_exit(sprd_backlight_exit);

MODULE_DESCRIPTION("Spreadtrum backlight Driver");
