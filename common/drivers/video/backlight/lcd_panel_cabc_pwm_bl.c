#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#ifdef CONFIG_HAS_EARLYSUSPEND
#include <linux/earlysuspend.h>
#endif
#include <linux/platform_device.h>
#include <linux/fb.h>
#include <linux/backlight.h>
#include <linux/err.h>
#include <linux/gpio.h>
#include <linux/delay.h>
#include <linux/spinlock.h>
#include <linux/rtc.h>
#include <../sprdfb/sprdfb.h>

int current_intensity;

static int backlight_mode = 1;

#define DIMMING_VALUE		8
#define MAX_BRIGHTNESS_VALUE	255
#define MIN_BRIGHTNESS_VALUE	20
#define BACKLIGHT_DEBUG 1
#define BACKLIGHT_SUSPEND 0
#define BACKLIGHT_RESUME 1
#if defined(CONFIG_MACH_VIVALTO5MVE3G)
#define GPIO_BL_CTRL	234
#else
#define GPIO_BL_CTRL	214
#endif

#define EW_DELAY 200
#define EW_DETECT 300
#define EW_WINDOW 900
#define DATA_START 40
#define LOW_BIT_L 40
#define LOW_BIT_H 10
#define HIGH_BIT_L 10
#define HIGH_BIT_H 40
#define END_DATA_L 10
#define END_DATA_H 400

extern uint32_t lcd_id_from_uboot;
static DEFINE_SPINLOCK(bl_ctrl_lock);

#define BACKLIGHT_DEBUG 1

#if BACKLIGHT_DEBUG
#define BLDBG(fmt, args...) printk(fmt, ## args)
#else
#define BLDBG(fmt, args...)
#endif

struct lcd_panel_cabc_pwm_bl_data {
	struct platform_device *pdev;
	unsigned int ctrl_pin;
#ifdef CONFIG_HAS_EARLYSUSPEND
	struct early_suspend early_suspend_desc;
#endif
};

struct brt_value {
	int level;		/* Platform setting values */
	int tune_level;		/* Chip Setting values */
};

static void gpio_backlight_set_brightness(int level)
{
	int i = 0;
	unsigned char brightness;
	int bit_map[8];
	brightness = level;
	unsigned long flags;

	pr_info("%s, level(%d)\n", __func__, brightness);
	for (i = 0; i < 8; i++) {
		bit_map[i] = brightness & 0x01;
		brightness >>= 1;
	}
	spin_lock_irqsave(&bl_ctrl_lock, flags);

	gpio_set_value(GPIO_BL_CTRL, 1);
	udelay(DATA_START);
	for (i = 7; i >= 0; i--) {
		if (bit_map[i]) {
			gpio_set_value(GPIO_BL_CTRL, 0);
			udelay(HIGH_BIT_L);
			gpio_set_value(GPIO_BL_CTRL, 1);
			udelay(HIGH_BIT_H);
		} else {
			gpio_set_value(GPIO_BL_CTRL, 0);
			udelay(LOW_BIT_L);
			gpio_set_value(GPIO_BL_CTRL, 1);
			udelay(LOW_BIT_H);
		}
	}
	gpio_set_value(GPIO_BL_CTRL, 0);
	udelay(END_DATA_L);
	gpio_set_value(GPIO_BL_CTRL, 1);
	udelay(END_DATA_H);
	spin_unlock_irqrestore(&bl_ctrl_lock, flags);
	return;
}

/* input: intensity in percentage 0% - 100% */
static int lcd_panel_cabc_pwm_backlight_update_status(struct backlight_device
						      *bd)
{
	int user_intensity = bd->props.brightness;
	int tune_level = 0;
	int i;

	if (bd->props.power != FB_BLANK_UNBLANK)
		user_intensity = 0;

	if (bd->props.fb_blank != FB_BLANK_UNBLANK)
		user_intensity = 0;

	if (bd->props.state & BL_CORE_SUSPENDED)
		user_intensity = 0;

	if (backlight_mode != BACKLIGHT_RESUME) {
		pr_info("%s, backlight not enabled (mode : %d)\n",
				__func__, backlight_mode);
		return 0;
	}

	tune_level = user_intensity;
	gpio_backlight_set_brightness(tune_level);

	current_intensity = tune_level;

	return 0;
}

static int lcd_panel_cabc_pwm_backlight_get_brightness(struct backlight_device
						       *bl)
{
	pr_info("%s\n", __func__);

	return current_intensity;
}

static struct backlight_ops lcd_panel_cabc_pwm_backlight_ops = {
	.update_status = lcd_panel_cabc_pwm_backlight_update_status,
	.get_brightness = lcd_panel_cabc_pwm_backlight_get_brightness,
};

#ifdef CONFIG_HAS_EARLYSUSPEND
static void lcd_panel_cabc_pwm_backlight_earlysuspend(struct early_suspend
						      *desc)
{
	struct timespec ts;
	struct rtc_time tm;
#ifndef CONFIG_FB_BL_EVENT_CTRL
	backlight_mode = BACKLIGHT_SUSPEND;
#endif
	gpio_set_value(GPIO_BL_CTRL, 0);
	getnstimeofday(&ts);
	rtc_time_to_tm(ts.tv_sec, &tm);
	printk("[%02d:%02d:%02d.%03lu][BACKLIGHT] earlysuspend\n", tm.tm_hour,
	       tm.tm_min, tm.tm_sec, ts.tv_nsec);
}

static void lcd_panel_cabc_pwm_backlight_lateresume(struct early_suspend *desc)
{
	struct lcd_panel_cabc_pwm_bl_data *lcd_panel_cabc_pwm =
	    container_of(desc, struct lcd_panel_cabc_pwm_bl_data,
			 early_suspend_desc);
	struct backlight_device *bl =
	    platform_get_drvdata(lcd_panel_cabc_pwm->pdev);
	struct timespec ts;
	struct rtc_time tm;
	unsigned long flags;

	getnstimeofday(&ts);
	rtc_time_to_tm(ts.tv_sec, &tm);
	backlight_mode = BACKLIGHT_RESUME;
	printk("[%02d:%02d:%02d.%03lu][BACKLIGHT] late resume\n", tm.tm_hour,
	       tm.tm_min, tm.tm_sec, ts.tv_nsec);

	spin_lock_irqsave(&bl_ctrl_lock, flags);
	gpio_set_value(GPIO_BL_CTRL, 1);
	udelay(200);
	gpio_set_value(GPIO_BL_CTRL, 0);
	udelay(300);
	gpio_set_value(GPIO_BL_CTRL, 1);
	udelay(400);
	spin_unlock_irqrestore(&bl_ctrl_lock, flags);
	backlight_update_status(bl);
}
#else
#ifdef CONFIG_PM
static int lcd_panel_cabc_pwm_backlight_suspend(struct platform_device *pdev,
						pm_message_t state)
{
	struct backlight_device *bl = platform_get_drvdata(pdev);
	struct lcd_panel_cabc_pwm_bl_data *lcd_panel_cabc_pwm =
	    dev_get_drvdata(&bl->dev);

	pr_info("%s\n", __func__);

	return 0;
}

static int lcd_panel_cabc_pwm_backlight_resume(struct platform_device *pdev)
{
	struct backlight_device *bl = platform_get_drvdata(pdev);
	pr_info("%s\n", __func__);

	backlight_update_status(bl);

	return 0;
}
#else
#define lcd_panel_cabc_pwm_backlight_suspend  NULL
#define lcd_panel_cabc_pwm_backlight_resume   NULL
#endif /* CONFIG_PM */
#endif /* CONFIG_HAS_EARLYSUSPEND */

#ifdef CONFIG_FB_BL_EVENT_CTRL
static int fb_bl_notifier_callback(struct notifier_block *self,
				unsigned long event, void *data)
{
	struct backlight_device *bl;
	struct fb_bl_event *evdata = data;

	if (event != FB_BL_EVENT_RESUME && event != FB_BL_EVENT_SUSPEND)
		return 0;

	bl = container_of(self, struct backlight_device, fb_notif);

	printk("fb_bl_notifier event=[0x%x]\n",event);

	switch (event) {
	case FB_BL_EVENT_RESUME:
		backlight_mode = BACKLIGHT_RESUME;
		backlight_update_status(bl);
		break;

	case FB_BL_EVENT_SUSPEND:
		backlight_mode = BACKLIGHT_SUSPEND;
		gpio_set_value(GPIO_BL_CTRL, 0);
		mdelay(3);
		break;
	}
	return 0;
}

static int fb_backlight_register_fb(struct backlight_device *bd)
{
	memset(&bd->fb_notif, 0, sizeof(bd->fb_notif));
	bd->fb_notif.notifier_call = fb_bl_notifier_callback;

	return fb_bl_register_client(&bd->fb_notif);
}

static void fb_backlight_unregister_fb(struct backlight_device *bd)
{
	fb_bl_unregister_client(&bd->fb_notif);
}
#endif

static int lcd_panel_cabc_pwm_backlight_probe(struct platform_device *pdev)
{
	struct backlight_device *bl;
	struct lcd_panel_cabc_pwm_bl_data *lcd_panel_cabc_pwm;
	struct backlight_properties props;
	int ret;
	int max_brightness = 255;
	int default_brightness = 160;

	pr_info("%s\n", __func__);

	lcd_panel_cabc_pwm = kzalloc(sizeof(*lcd_panel_cabc_pwm), GFP_KERNEL);
	if (!lcd_panel_cabc_pwm) {
		dev_err(&pdev->dev, "no memory for state\n");
		ret = -ENOMEM;
		goto err_alloc;
	}

	memset(&props, 0, sizeof(struct backlight_properties));
	props.max_brightness = max_brightness;
	props.type = BACKLIGHT_PLATFORM;
	bl = backlight_device_register("panel", &pdev->dev,
				       lcd_panel_cabc_pwm,
				       &lcd_panel_cabc_pwm_backlight_ops,
				       &props);
	if (IS_ERR(bl)) {
		dev_err(&pdev->dev, "failed to register backlight\n");
		ret = PTR_ERR(bl);
		goto err_bl;
	}

	if (gpio_request(GPIO_BL_CTRL, "BL_CTRL")) {
		pr_err("%s, request gpio(%d) failed\n",
				__func__, GPIO_BL_CTRL);
		goto err_bl;
	}
#ifdef CONFIG_FB_BL_EVENT_CTRL
	ret = fb_backlight_register_fb(bl);
	if (ret) {
		device_unregister(&bl->dev);
		ret = ERR_PTR(ret);
		goto err_bl;
	}
#endif
#ifdef CONFIG_HAS_EARLYSUSPEND
	lcd_panel_cabc_pwm->pdev = pdev;
	lcd_panel_cabc_pwm->early_suspend_desc.level =
	    EARLY_SUSPEND_LEVEL_BLANK_SCREEN;
	lcd_panel_cabc_pwm->early_suspend_desc.suspend =
	    lcd_panel_cabc_pwm_backlight_earlysuspend;
	lcd_panel_cabc_pwm->early_suspend_desc.resume =
	    lcd_panel_cabc_pwm_backlight_lateresume;
	register_early_suspend(&lcd_panel_cabc_pwm->early_suspend_desc);
#endif

	bl->props.max_brightness = max_brightness;
	bl->props.brightness = default_brightness;
	platform_set_drvdata(pdev, bl);
	/* lcd_panel_cabc_pwm_backlight_update_status(bl); */

	return 0;

err_bl:
	kfree(lcd_panel_cabc_pwm);
err_alloc:
	return ret;
}

static int lcd_panel_cabc_pwm_backlight_remove(struct platform_device *pdev)
{
	struct backlight_device *bl = platform_get_drvdata(pdev);
	struct lcd_panel_cabc_pwm_bl_data *lcd_panel_cabc_pwm =
	    dev_get_drvdata(&bl->dev);

	backlight_device_unregister(bl);

#ifdef CONFIG_HAS_EARLYSUSPEND
	unregister_early_suspend(&lcd_panel_cabc_pwm->early_suspend_desc);
#endif

	kfree(lcd_panel_cabc_pwm);

	return 0;
}

static int lcd_panel_cabc_pwm_backlight_shutdown(struct platform_device *pdev)
{
	pr_info("%s\n", __func__);

	return 0;
}

static const struct of_device_id backlight_of_match[] = {
	{.compatible = "sprd,panel_cabc_bl",},
	{}
};

static struct platform_driver lcd_panel_cabc_pwm_backlight_driver = {
	.driver = {
		   .name = "panel",
		   .owner = THIS_MODULE,
		   .of_match_table = backlight_of_match,
		   },
	.probe = lcd_panel_cabc_pwm_backlight_probe,
	.remove = lcd_panel_cabc_pwm_backlight_remove,
	.shutdown = lcd_panel_cabc_pwm_backlight_shutdown,
#ifndef CONFIG_HAS_EARLYSUSPEND
	.suspend = lcd_panel_cabc_pwm_backlight_suspend,
	.resume = lcd_panel_cabc_pwm_backlight_resume,
#endif
};

static int __init lcd_panel_cabc_pwm_backlight_init(void)
{
	return platform_driver_register(&lcd_panel_cabc_pwm_backlight_driver);
}
module_init(lcd_panel_cabc_pwm_backlight_init);

static void __exit lcd_panel_cabc_pwm_backlight_exit(void)
{
	platform_driver_unregister(&lcd_panel_cabc_pwm_backlight_driver);
}
module_exit(lcd_panel_cabc_pwm_backlight_exit);

MODULE_DESCRIPTION("lcd_panel_cabc_pwm_bl based Backlight Driver");
MODULE_LICENSE("GPL");
MODULE_ALIAS("platform:lcd_panel_cabc_pwm_bl");
