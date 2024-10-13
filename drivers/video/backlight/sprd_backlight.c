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

//#define SPRD_BACKLIGHT_DBG
#ifdef SPRD_BACKLIGHT_DBG
#define ENTER printk(KERN_INFO "[SPRD_BACKLIGHT_DBG] func: %s  line: %04d\n", __func__, __LINE__);
#define PRINT_DBG(x...)  printk(KERN_INFO "[SPRD_BACKLIGHT_DBG] " x)
#define PRINT_INFO(x...)  printk(KERN_INFO "[SPRD_BACKLIGHT_INFO] " x)
#define PRINT_WARN(x...)  printk(KERN_INFO "[SPRD_BACKLIGHT_WARN] " x)
#define PRINT_ERR(format,x...)  printk(KERN_ERR "[SPRD_BACKLIGHT_ERR] func: %s  line: %04d  info: " format, __func__, __LINE__, ## x)
#else
#define ENTER
#define PRINT_DBG(x...)
#define PRINT_INFO(x...)  printk(KERN_INFO "[SPRD_BACKLIGHT_INFO] " x)
#define PRINT_WARN(x...)  printk(KERN_INFO "[SPRD_BACKLIGHT_WARN] " x)
#define PRINT_ERR(format,x...)  printk(KERN_ERR "[SPRD_BACKLIGHT_ERR] func: %s  line: %04d  info: " format, __func__, __LINE__, ## x)
#endif

#define DIMMING_PWD_BASE	(SPRD_MISC_BASE + 0x8020)
#define PD_PWM_BASE 	DIMMING_PWD_BASE

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
#ifdef         CONFIG_ARCH_SCX15
#define        PWM_SCALE           0xd
#else
#define        PWM_SCALE       1
#endif
#define        PWM2_SCALE           0x0
#define        PWM_REG_MSK     0xffff
#define        PWM_MOD_MAX     0xff
#define        PWM_DUTY_MAX     0x7f

/*****************************************
*  level              mV            step
*  0 ~  63      0  mV ~ 189 mV      3 mV
* 64 ~  95     192 mV ~ 378 mV      6 mV
* 96 ~ 127     384 mV ~ 756 mV     12 mV
******************************************/
#define        MAX_VOLTAGE_LEVEL    70
#define        MIN_VOLTAGE_LEVEL    4

/* sprdtrum backlight have two driven mode:
 * 1) pwm mode  //you need to define SPRD_BACKLIGHT_PWM
 * 2) white led mode //you need to deifine SPRD_BACKLIGHT_WHITELED
 *	2.1) series mode //whiteled driven by dimming PWM,
	you need to define the DIMMING_PWD_BASE
 *	2.2) parallel mode //whiteled driven by pd PWM,
 *	you need to define the PD_PWD_BASE
 */

#ifdef CONFIG_BACKLIGHT_SPRD_PWM_MODE
	/*if the backlight is driven by pwm, use this MACRO */
	#define SPRD_BACKLIGHT_PWM
#else
	/*the backlight is driven by whiteled default */
	#define SPRD_BACKLIGHT_WHITELED
	#define SPRD_DIM_PWM_MODE
#endif

enum bl_pwm_mode {
	normal_pwm,
	dim_pwm,
	pd_pwd,
};

struct sprd_bl_devdata {
	enum bl_pwm_mode	pwm_mode;
	u32		pwm_index;
	struct backlight_device *bldev;
	int             suspend;
	struct clk      *clk;
	struct early_suspend sprd_early_suspend_desc;
	int ctl_pin;
	int ctl_pin_level;
};

static struct sprd_bl_devdata sprdbl = {
	.ctl_pin = -1,
	.ctl_pin_level = -1,
	};
#if(defined(CONFIG_MACH_SP7715EA)||defined(CONFIG_MACH_SP7715EATRISIM))
static int pwm_whiteled_both = 0;
static int sprd_bl_whiteled_update_status(struct backlight_device *bldev);
#endif

static DEFINE_SPINLOCK(clock_en_lock);
static void bl_pwm_clk_en(int enable)
{
        unsigned long spin_lock_flags;
        static int current_state = 0;

        spin_lock_irqsave(&clock_en_lock, spin_lock_flags);
        if (1 == enable) {
                if (0 == current_state) {
                        clk_prepare_enable(sprdbl.clk);
                        current_state = 1;
                }
        } else {
                if (1 == current_state) {
                        clk_disable_unprepare(sprdbl.clk);
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
static int sprd_bl_pwm_update_status(struct backlight_device *bldev)
{
	u32 led_level;

	if ((bldev->props.state & (BL_CORE_SUSPENDED | BL_CORE_FBBLANK)) ||
			bldev->props.power != FB_BLANK_UNBLANK ||
			sprdbl.suspend ||
			bldev->props.brightness == 0) {
		/* disable backlight */
		pwm_write(sprdbl.pwm_index, 0, PWM_PRESCALE);
		bl_pwm_clk_en(0);
		PRINT_INFO("[pwm] disabled\n");
	} else {
		led_level = bldev->props.brightness & PWM_MOD_MAX;
		//led_level = (led_level * (PWM_DUTY_MAX+1) / (PWM_MOD_MAX+1)) + 10;
		led_level = led_level * 67 / 100;
		if(led_level < 8)
			led_level = 8;
		PRINT_INFO("[pwm] brightness = %d, led_level = %d\n", bldev->props.brightness, led_level);
		bl_pwm_clk_en(1);
		pwm_write(sprdbl.pwm_index, PWM2_SCALE, PWM_PRESCALE);
		pwm_write(sprdbl.pwm_index, (led_level << 8) | PWM_MOD_MAX, PWM_CNT);
		pwm_write(sprdbl.pwm_index, PWM_REG_MSK, PWM_PAT_LOW);
		pwm_write(sprdbl.pwm_index, PWM_REG_MSK, PWM_PAT_HIG);
		pwm_write(sprdbl.pwm_index, PWM_ENABLE, PWM_PRESCALE);
	}

#if(defined(CONFIG_MACH_SP7715EA)||defined(CONFIG_MACH_SP7715EATRISIM))
	if(1 == pwm_whiteled_both)
		sprd_bl_whiteled_update_status(bldev);
#endif
	return 0;
}

static int sprd_bl_pwm_get_brightness(struct backlight_device *bldev)
{
	return (pwm_read(sprdbl.pwm_index, PWM_CNT) >> 8) & PWM_MOD_MAX;
}

#ifdef CONFIG_ARCH_SCX15
static void srpd_backlight_init(void)
{
	PRINT_INFO("srpd_backlight_init\n");
	sci_adi_set(ANA_REG_GLB_WHTLED_CTRL1, BIT_PWM0_EN );

	//this is the A-die PWM controller, it is used for white_led.
	sci_adi_raw_write(PD_PWM_BASE+PWM_CNT, PWM_DUTY | PWM_MOD);
	sci_adi_raw_write(PD_PWM_BASE+PWM_TONE_DIV, PWM_DIV);
	sci_adi_raw_write(PD_PWM_BASE+PWM_PRESCALE, PWM_ENABLE | PWM_SCALE);
	sci_adi_raw_write(PD_PWM_BASE+PWM_PAT_LOW, PWM_LOW);
	sci_adi_raw_write(PD_PWM_BASE+PWM_PAT_HIG, PWM_HIG);

	sci_adi_set(ANA_REG_GLB_WHTLED_CTRL0, BIT_WHTLED_SERIES_EN);
	sci_adi_set(ANA_REG_GLB_WHTLED_CTRL0, BIT_WHTLED_BOOST_EN);
	sci_adi_set(ANA_REG_GLB_WHTLED_CTRL0, BIT_WHTLED_PD);

	sci_adi_set(ANA_REG_GLB_WHTLED_CTRL1, BIT_RTC_PWM0_EN);
	sci_adi_set(ANA_REG_GLB_WHTLED_CTRL1, BIT_PWM0_EN);
}
#endif

static int sprd_bl_whiteled_update_status(struct backlight_device *bldev)
{
	u32 bl_brightness, led_level, pwm_level;
	u32 reg_val;

	if ((bldev->props.state & (BL_CORE_SUSPENDED | BL_CORE_FBBLANK)) ||
			bldev->props.power != FB_BLANK_UNBLANK ||
			sprdbl.suspend ||
			bldev->props.brightness == 0) {
		/* disable backlight */
		if (sprdbl.pwm_mode == dim_pwm) {
			sci_adi_clr(ANA_REG_GLB_WHTLED_CTRL0, BIT_WHTLED_BOOST_EN);
		} else {
			/*yes, 1 is disbale and 0 is enable*/
			sci_adi_set(ANA_REG_GLB_WHTLED_CTRL0, BIT_WHTLED_PD);
		}
		PRINT_INFO("[white led] disabled\n");
	} else {
		bl_brightness = bldev->props.brightness & PWM_MOD_MAX;
		pwm_level = bl_brightness & 0x3;
		/*duty ratio = 25% 50% 75% or 100%*/
		pwm_level = (PWM_MOD_MAX >> 2) * (pwm_level + 1) & PWM_MOD_MAX;

		if (sprdbl.pwm_mode == dim_pwm) {
			/*series mode*/
			/*whiteled config*/
			led_level = (((MAX_VOLTAGE_LEVEL - MIN_VOLTAGE_LEVEL + 1) * bl_brightness) / (PWM_MOD_MAX + 1)) + MIN_VOLTAGE_LEVEL;
			PRINT_INFO("[white led] brightness = %d, led_level = %d\n", bldev->props.brightness, led_level);
			reg_val = sci_adi_read(ANA_REG_GLB_WHTLED_CTRL1);

#ifdef CONFIG_ARCH_SCX15
			reg_val &= ~(0x7f << 6);
			reg_val |= led_level << 6;
#else
			reg_val &= ~(0x7f << 5);
			reg_val |= led_level << 5;
#endif

			sci_adi_raw_write(ANA_REG_GLB_WHTLED_CTRL1, reg_val);
			#if 0
			/*dimming pwm config*/
			sci_adi_raw_write(DIMMING_PWD_BASE + PWM_SCALE, PWM_PRESCALE);
			sci_adi_raw_write(DIMMING_PWD_BASE + PWM_CNT,
					(pwm_level << 8) | PWM_MOD_MAX);
			sci_adi_raw_write(DIMMING_PWD_BASE + PWM_SCALE,
					PWM_SCALE | PWM_ENABLE);
			#endif
			/*enable the whiteled bootst output*/
			sci_adi_set(ANA_REG_GLB_WHTLED_CTRL0, BIT_WHTLED_SERIES_EN);
			sci_adi_set(ANA_REG_GLB_WHTLED_CTRL0, BIT_WHTLED_BOOST_EN);
		} else {
			/*parallel mode*/
			led_level = (bl_brightness >> 2) & 0x3f;
			reg_val = sci_adi_read(ANA_REG_GLB_WHTLED_CTRL0);
			reg_val &= ~(0x3f << 1);
			reg_val |= led_level << 1;
			sci_adi_raw_write(ANA_REG_GLB_WHTLED_CTRL0, reg_val);

			sci_adi_raw_write(PD_PWM_BASE + PWM_SCALE, PWM_PRESCALE);
			sci_adi_raw_write(PD_PWM_BASE + PWM_CNT,
					(pwm_level << 8) | PWM_MOD_MAX);

			sci_adi_raw_write(PD_PWM_BASE + PWM_SCALE,
					PWM_SCALE | PWM_ENABLE);

			sci_adi_clr(ANA_REG_GLB_WHTLED_CTRL0, BIT_WHTLED_SERIES_EN);
			sci_adi_clr(ANA_REG_GLB_WHTLED_CTRL0, BIT_WHTLED_PD);
		}
	}

	return 0;
}

static int sprd_bl_whiteled_get_brightness(struct backlight_device *bldev)
{
	/*fixme*/
	return 0;
}

static const struct backlight_ops sprd_backlight_pwm_ops = {
       .update_status = sprd_bl_pwm_update_status,
       .get_brightness = sprd_bl_pwm_get_brightness,
};

static const struct backlight_ops sprd_backlight_whiteled_ops = {
       .update_status = sprd_bl_whiteled_update_status,
       .get_brightness = sprd_bl_whiteled_get_brightness,
};

#ifdef CONFIG_EARLYSUSPEND
void set_ctl_pin_state(int on){
        static int ret = 0;
	
	if (sprdbl.ctl_pin == -1)
		return;
	on = on ? sprdbl.ctl_pin_level : !sprdbl.ctl_pin_level;
	if (ret == 0) {
		ret = gpio_request(sprdbl.ctl_pin, "backlight_ctl_pin");
		if (ret) {
			printk("%s: request gpio error\n", __func__);
			return -EIO;
		}
		gpio_direction_output(sprdbl.ctl_pin, on);
		ret = 1;
	}
	gpio_set_value(sprdbl.ctl_pin,on);
}
static void sprd_backlight_earlysuspend(struct early_suspend *h)
{
	set_ctl_pin_state(0);
	sprdbl.suspend = 1;
	sprdbl.bldev->ops->update_status(sprdbl.bldev);
	PRINT_INFO("early suspend\n");
}

static void sprd_backlight_lateresume(struct early_suspend *h)
{
	set_ctl_pin_state(1);
	sprdbl.suspend = 0;
	sprdbl.bldev->ops->update_status(sprdbl.bldev);
	PRINT_INFO("late resume\n");
}
#endif

#ifdef CONFIG_OF
static struct resource *sprd_backlight_parse_dt(struct platform_device *pdev)
{
	int ret;
	struct device_node *np = pdev->dev.of_node;
	struct resource *pwm_res;
	pwm_res = kzalloc(sizeof(*pwm_res), GFP_KERNEL);
	if (!pwm_res) {
		dev_err(pdev, "sprd_backlight Could not allocate struct resource");
		return NULL;
	}
	ret = of_property_read_u32(np, "start", &pwm_res->start);
	if(ret){
		dev_err(pdev, "fail to get resource.start\n");
		goto fail;
	}
	ret = of_property_read_u32(np, "end", &pwm_res->end);
	if(ret){
		dev_err(pdev, "fail to get resource.end\n");
		goto fail;
	}
	ret = of_property_read_u32(np, "ctl_pin", &sprdbl.ctl_pin);
	ret = of_property_read_u32(np, "ctl_pin_level", &sprdbl.ctl_pin_level);
	ret = of_property_read_u32(np, "flags", &pwm_res->flags);
	if(ret){
		dev_err(pdev, "fail to get resource.flags\n");
		goto fail;
	}
	return pwm_res;
fail:
	kfree(pwm_res);
	return NULL;
}
#endif

static int sprd_backlight_probe(struct platform_device *pdev)
{
	struct backlight_properties props;
	struct backlight_device *bldev;
	int use_pwm = 0;

#ifdef SPRD_BACKLIGHT_PWM
	struct clk* ext_26m = NULL;
	struct resource *pwm_res = NULL;
	struct resource *pwm_res1 = NULL;
	char pwm_clk_name[32];

#ifdef CONFIG_OF
	struct device_node *np = pdev->dev.of_node;
	if(np){
		pwm_res = sprd_backlight_parse_dt(pdev);
		if (pwm_res){
                   pdev->resource = pwm_res;
		}else{
			printk("Can't get pwm resource");
			return -ENODEV;
		}
	}else{
		printk("dev.of_node is NULL");
		return -ENODEV;
	}
#else
	pwm_res = platform_get_resource(pdev, IORESOURCE_IO, 0);
	if (pwm_res == NULL) {
		printk("Can't get pwm resource");
		return -ENODEV;
	}
	pwm_res1 = platform_get_resource(pdev, IORESOURCE_IRQ, 0);
	if (pwm_res1 != NULL) {
		sprdbl.ctl_pin = pwm_res1->start;
		sprdbl.ctl_pin_level = !!pwm_res1->end;
	}
#endif

	sprdbl.pwm_index = pwm_res->start;
	/*fixme, the pwm's clk name must like this:clk_pwmx*/
	sprintf(pwm_clk_name, "%s%d", "clk_pwm", sprdbl.pwm_index);
	sprdbl.clk = clk_get(&pdev->dev, pwm_clk_name);
	if (IS_ERR(sprdbl.clk)) {
		printk("Can't get pwm's clk");
		return -ENODEV;
	}

	ext_26m = clk_get(NULL, "ext_26m");
	if (IS_ERR(ext_26m)) {
		printk("Can't get pwm's ext_26m");
		return -ENODEV;
	}

	clk_set_parent(sprdbl.clk,ext_26m);

	sprdbl.pwm_mode = normal_pwm;
	use_pwm = 1;
	PRINT_INFO("PWM%d is used for brightness control (external backlight controller)\n", sprdbl.pwm_index);
#else

#if(defined(CONFIG_MACH_SP7715EA)||defined(CONFIG_MACH_SP7715EATRISIM))
	struct clk* ext_26m = NULL;
	struct resource *pwm_res = NULL;
	char pwm_clk_name[32];
	int adie_chip_ver = 0;

	adie_chip_ver = sci_get_ana_chip_ver();
	PRINT_INFO("adie_chip_ver = 0x%08X\n", adie_chip_ver);

	//if(0x00000000 != adie_chip_ver) {
	if(1) {
#ifdef CONFIG_OF
		struct device_node *np = pdev->dev.of_node;
		if(np){
			pwm_res = sprd_backlight_parse_dt(pdev);
			if (pwm_res){
				pdev->resource = pwm_res;
			}else{
				printk("Can't get pwm resource");
				return -ENODEV;
			}
		}else{
			printk("dev.of_node is NULL");
			return -ENODEV;
		}
#else
		pwm_res = platform_get_resource(pdev, IORESOURCE_IO, 0);
		if (IS_ERR(pwm_res)) {
			printk("Can't get pwm resource");
			return -ENODEV;
		}
#endif

		sprdbl.pwm_index = pwm_res->start;
		/*fixme, the pwm's clk name must like this:clk_pwmx*/
		sprintf(pwm_clk_name, "%s%d", "clk_pwm", sprdbl.pwm_index);
		sprdbl.clk = clk_get(&pdev->dev, pwm_clk_name);
		if (IS_ERR(sprdbl.clk)) {
			printk("Can't get pwm's clk");
			return -ENODEV;
		}

		ext_26m = clk_get(NULL, "ext_26m");
		if (IS_ERR(ext_26m)) {
			printk("Can't get pwm's ext_26m");
			return -ENODEV;
		}

		clk_set_parent(sprdbl.clk,ext_26m);

		sprdbl.pwm_mode = normal_pwm;
		PRINT_INFO("PWM%d is used for brightness control (external backlight controller)\n", sprdbl.pwm_index);
		use_pwm = 1;
		goto pwm_7715ea;
	}
#endif

#ifdef SPRD_DIM_PWM_MODE
	sprdbl.pwm_mode = dim_pwm;
#else
	sprdbl.pwm_mode = pd_pwd;
#endif

#ifdef CONFIG_ARCH_SCX15
	srpd_backlight_init();
#endif
#endif

pwm_7715ea:

	memset(&props, 0, sizeof(struct backlight_properties));
	props.max_brightness = PWM_MOD_MAX;
	props.type = BACKLIGHT_RAW;
	/*the default brightness = 1/2 max brightness */
	props.brightness = PWM_MOD_MAX >> 1;
	props.power = FB_BLANK_UNBLANK;

	if(1 == use_pwm) {
		bldev = backlight_device_register(
				pdev->name, &pdev->dev,
				&sprdbl, &sprd_backlight_pwm_ops, &props);
		if (IS_ERR(bldev)) {
			printk(KERN_ERR "Failed to register backlight device\n");
			return -ENOMEM;
		}
		#if(defined(CONFIG_MACH_SP7715EA)||defined(CONFIG_MACH_SP7715EATRISIM))
			if(0x00000000 == adie_chip_ver) {
				pwm_whiteled_both = 1;
				#ifdef SPRD_DIM_PWM_MODE
					sprdbl.pwm_mode = dim_pwm;
				#else
					sprdbl.pwm_mode = pd_pwd;
				#endif
				#ifdef CONFIG_ARCH_SCX15
					srpd_backlight_init();
				#endif
			}
		#endif
	}
	else {
		bldev = backlight_device_register(
				pdev->name, &pdev->dev,
				&sprdbl, &sprd_backlight_whiteled_ops, &props);
		if (IS_ERR(bldev)) {
			printk(KERN_ERR "Failed to register backlight device\n");
			return -ENOMEM;
		}
	}

	sprdbl.bldev = bldev;
	platform_set_drvdata(pdev, bldev);
	bldev->ops->update_status(bldev);

#ifdef CONFIG_EARLYSUSPEND
	sprdbl.sprd_early_suspend_desc.level	= EARLY_SUSPEND_LEVEL_BLANK_SCREEN;
	sprdbl.sprd_early_suspend_desc.suspend	= sprd_backlight_earlysuspend;
	sprdbl.sprd_early_suspend_desc.resume	= sprd_backlight_lateresume;
	register_early_suspend(&sprdbl.sprd_early_suspend_desc);
#endif

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
	unregister_early_suspend(&sprdbl.sprd_early_suspend_desc);
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
	{ .compatible = "sprd,sprd_backlight", },
	{ }
};
static struct platform_driver sprd_backlight_driver = {
	.probe = sprd_backlight_probe,
	.remove = sprd_backlight_remove,
	.suspend = sprd_backlight_suspend,
	.resume = sprd_backlight_resume,
	.shutdown = sprd_backlight_shutdown,
	.driver = {
		.name = "sprd_backlight",
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
