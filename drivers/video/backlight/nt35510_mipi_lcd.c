/* linux/drivers/video/backlight/nt35510_mipi_lcd.c
 *
 * Samsung SoC MIPI LCD driver.
 *
 * Copyright (c) 2013 Samsung Electronics
 *
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
*/

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/errno.h>
#include <linux/mutex.h>
#include <linux/wait.h>
#include <linux/ctype.h>
#include <linux/io.h>
#include <linux/delay.h>
#include <linux/irq.h>
#include <linux/interrupt.h>
#include <linux/gpio.h>
#include <linux/workqueue.h>
#include <linux/backlight.h>
#include <linux/lcd.h>
#include <linux/rtc.h>
#include <linux/reboot.h>
#include <linux/syscalls.h> /* sys_sync */

#include <video/mipi_display.h>
#include <plat/dsim.h>
#include <plat/mipi_dsi.h>
#include <plat/gpio-cfg.h>

#include "nt35510_param.h"

#define MAX_BRIGHTNESS 255
#define MIN_BRIGHTNESS 0
#define DEFAULT_BRIGHTNESS 130
#define KTD253_BACKLIGHT_OFF		0
#define KTD253_MIN_CURRENT_RATIO	1
#define KTD253_MAX_CURRENT_RATIO	32
#define T_LOW_NS       (200 + 10) /* Additional 10 as safety factor */
#define T_HIGH_NS      (200 + 10) /* Additional 10 as safety factor */
#define T_ON_MS		2 /* Start up time */
#define T_OFF_MS       3

#define POWER_IS_ON(pwr)		((pwr) <= FB_BLANK_NORMAL)

#define LDI_ID_REG			0x04
#define LDI_ID1_REG			0xDA	/* LCD module's manufacturer ID */
#define LDI_ID2_REG			0xDB	/* LCD module version ID */
#define LDI_ID3_REG			0xDC	/* LCD module driver ID */
#define LDI_ID_LEN			3

#define FEATURE_KTD283

struct lcd_info {
#ifdef FEATURE_KTD283
	unsigned int			bl;
	unsigned int			current_bl;
	struct backlight_device		*bd;
#endif
	unsigned int			auto_brightness;
	unsigned int			ldi_enable;
	unsigned int			power;
	struct mutex			lock;
	struct mutex			bl_lock;

	struct device			*dev;
	struct lcd_device		*ld;
	struct mipi_dsim_lcd_device	*dsim_dev;
	unsigned char			id[LDI_ID_LEN];
	unsigned int			irq;
	unsigned int			gpio;
	unsigned int			connected;

	struct mipi_dsim_device		*dsim;
};

static struct lcd_info *g_lcd;

static int nt35510_write(struct lcd_info *lcd, const u8 *seq, u32 len)
{
	int ret;
	int retry;
	u8 cmd;

	if (!lcd->connected)
		return -EINVAL;

	mutex_lock(&lcd->lock);

	if (len > 2)
		cmd = MIPI_DSI_DCS_LONG_WRITE;
	else if (len == 2)
		cmd = MIPI_DSI_DCS_SHORT_WRITE_PARAM;
	else if (len == 1)
		cmd = MIPI_DSI_DCS_SHORT_WRITE;
	else {
		ret = -EINVAL;
		goto write_err;
	}

	retry = 5;
write_data:
	if (!retry) {
		dev_err(&lcd->ld->dev, "%s failed - exceed retry count\n", __func__);
		goto write_err;
	}
	ret = s5p_mipi_dsi_wr_data(lcd->dsim, cmd, seq, len);
	if (ret < 0) {
		dev_dbg(&lcd->ld->dev, "mipi_write failed retry ..\n");
		retry--;
		goto write_data;
	}

write_err:
	mutex_unlock(&lcd->lock);
	return ret;
}

#if 0
static int nt35510_read(struct lcd_info *lcd, u8 addr, u8 *buf, u32 len)
{
	int ret = 0;
	u8 cmd;
	int retry;

	if (!lcd->connected)
		return -EINVAL;

	mutex_lock(&lcd->lock);

	if (len > 2)
		cmd = MIPI_DSI_DCS_READ;
	else if (len == 2)
		cmd = MIPI_DSI_GENERIC_READ_REQUEST_2_PARAM;
	else if (len == 1)
		cmd = MIPI_DSI_GENERIC_READ_REQUEST_1_PARAM;
	else {
		ret = -EINVAL;
		goto read_err;
	}
	retry = 5;
read_data:
	if (!retry) {
		dev_err(&lcd->ld->dev, "%s failed - exceed retry count\n", __func__);
		goto read_err;
	}
	ret = s5p_mipi_dsi_rd_data(lcd->dsim, cmd, addr, len, buf,1);
	if (ret < 0) {
		dev_dbg(&lcd->ld->dev, "mipi_read failed retry ..\n");
		retry--;
		goto read_data;
	}
read_err:
	mutex_unlock(&lcd->lock);
	return ret;
}

static void nt35510_read_id(struct lcd_info *lcd, u8 *buf)
{
	int ret = 0;

	ret = nt35510_read(lcd, LDI_ID_REG, buf, LDI_ID_LEN);
	if (ret < 0) {
		lcd->connected = 0;
		dev_info(&lcd->ld->dev, "panel is not connected well\n");
	}
}
#endif

#ifdef FEATURE_KTD283
static int update_brightness(struct lcd_info *lcd, u8 force)
{
	int step_count = 0;
	unsigned long irqFlags;
	int brightness = lcd->bd->props.brightness;
	mutex_lock(&lcd->bl_lock);

	lcd->bl = ((32 * brightness * 210) / MAX_BRIGHTNESS) / MAX_BRIGHTNESS;

	if ((force) || ((lcd->ldi_enable) && (lcd->current_bl != lcd->bl))) {

		if (lcd->bl == KTD253_BACKLIGHT_OFF) {
			/* Switch off backlight.	*/
			dev_info(&lcd->ld->dev, "%s: switching backlight off\n", __func__);
			gpio_set_value(EXYNOS4_GPD0(1), 0);
			msleep(T_OFF_MS);
		} else {

			local_irq_save(irqFlags);

		 	if (lcd->current_bl == KTD253_BACKLIGHT_OFF) {
				/* Switch on backlight. */
				dev_dbg(&lcd->ld->dev, "%s: switching backlight on\n", __func__);
				gpio_set_value(EXYNOS4_GPD0(1), 1);
				mdelay(T_ON_MS);
				/* Backlight is always at full intensity when switched on. */
				lcd ->current_bl = KTD253_MAX_CURRENT_RATIO;
			}

			while (lcd ->current_bl!= lcd ->bl) {
				gpio_set_value(EXYNOS4_GPD0(1), 0);
				ndelay(T_LOW_NS);
				gpio_set_value(EXYNOS4_GPD0(1), 1);
				ndelay(T_HIGH_NS);

				if (lcd ->current_bl == KTD253_MIN_CURRENT_RATIO) {
					lcd ->current_bl = KTD253_MAX_CURRENT_RATIO;
				} else {
					lcd ->current_bl--;
				}
				step_count++;
			}

			local_irq_restore(irqFlags);

		dev_info(&lcd->ld->dev, "%s: after_ lcd ->current_bl = %d\n", __func__,lcd ->current_bl);

		}

			lcd->current_bl = lcd->bl;

	}

	mutex_unlock(&lcd->bl_lock);

	return 0;
}
#endif

static int nt35510_ldi_init(struct lcd_info *lcd)
{
	int ret = 0;

	nt35510_write(lcd, SEQ_SLEEP_OUT, ARRAY_SIZE(SEQ_SLEEP_OUT));
	msleep(120);

	/* power setting */
	nt35510_write(lcd, SEQ_POWER_INIT0, ARRAY_SIZE(SEQ_POWER_INIT0));
	nt35510_write(lcd, SEQ_POWER_INIT1, ARRAY_SIZE(SEQ_POWER_INIT1));
	nt35510_write(lcd, SEQ_POWER_INIT2, ARRAY_SIZE(SEQ_POWER_INIT2));
	nt35510_write(lcd, SEQ_POWER_INIT3, ARRAY_SIZE(SEQ_POWER_INIT3));
	nt35510_write(lcd, SEQ_POWER_INIT4, ARRAY_SIZE(SEQ_POWER_INIT4));
	nt35510_write(lcd, SEQ_POWER_INIT5, ARRAY_SIZE(SEQ_POWER_INIT5));
	nt35510_write(lcd, SEQ_POWER_INIT6, ARRAY_SIZE(SEQ_POWER_INIT6));
	nt35510_write(lcd, SEQ_POWER_INIT7, ARRAY_SIZE(SEQ_POWER_INIT7));
	nt35510_write(lcd, SEQ_POWER_INIT8, ARRAY_SIZE(SEQ_POWER_INIT8));
	nt35510_write(lcd, SEQ_POWER_INIT9, ARRAY_SIZE(SEQ_POWER_INIT9));
	nt35510_write(lcd, SEQ_POWER_INIT10, ARRAY_SIZE(SEQ_POWER_INIT10));
	nt35510_write(lcd, SEQ_POWER_INIT11, ARRAY_SIZE(SEQ_POWER_INIT11));
	/* gamma setting */
	nt35510_write(lcd, SEQ_GAMMA_INIT1, ARRAY_SIZE(SEQ_GAMMA_INIT1));
	nt35510_write(lcd, SEQ_GAMMA_INIT2, ARRAY_SIZE(SEQ_GAMMA_INIT2));
	nt35510_write(lcd, SEQ_GAMMA_INIT3, ARRAY_SIZE(SEQ_GAMMA_INIT3));
	nt35510_write(lcd, SEQ_GAMMA_INIT4, ARRAY_SIZE(SEQ_GAMMA_INIT4));
	nt35510_write(lcd, SEQ_GAMMA_INIT5, ARRAY_SIZE(SEQ_GAMMA_INIT5));
	nt35510_write(lcd, SEQ_GAMMA_INIT6, ARRAY_SIZE(SEQ_GAMMA_INIT6));
	/* display parameter */
	nt35510_write(lcd, SEQ_DISPLAY_INIT0, ARRAY_SIZE(SEQ_DISPLAY_INIT0));
	nt35510_write(lcd, SEQ_DISPLAY_INIT1, ARRAY_SIZE(SEQ_DISPLAY_INIT1));
	nt35510_write(lcd, SEQ_DISPLAY_INIT2, ARRAY_SIZE(SEQ_DISPLAY_INIT2));
	nt35510_write(lcd, SEQ_DISPLAY_INIT3, ARRAY_SIZE(SEQ_DISPLAY_INIT3));
	nt35510_write(lcd, SEQ_DISPLAY_INIT4, ARRAY_SIZE(SEQ_DISPLAY_INIT4));
	nt35510_write(lcd, SEQ_DISPLAY_INIT5, ARRAY_SIZE(SEQ_DISPLAY_INIT5));
	nt35510_write(lcd, SEQ_DISPLAY_INIT6, ARRAY_SIZE(SEQ_DISPLAY_INIT6));
	nt35510_write(lcd, SEQ_DISPLAY_INIT7, ARRAY_SIZE(SEQ_DISPLAY_INIT7));
	nt35510_write(lcd, SEQ_DISPLAY_INIT8, ARRAY_SIZE(SEQ_DISPLAY_INIT8));
	nt35510_write(lcd, SEQ_DISPLAY_INIT9, ARRAY_SIZE(SEQ_DISPLAY_INIT9));
	nt35510_write(lcd, SEQ_DISPLAY_INIT10, ARRAY_SIZE(SEQ_DISPLAY_INIT10));
	nt35510_write(lcd, SEQ_DISPLAY_INIT11, ARRAY_SIZE(SEQ_DISPLAY_INIT11));

	nt35510_write(lcd, SEQ_DISPLAY_ON, ARRAY_SIZE(SEQ_DISPLAY_ON));
	msleep(10);

	return ret;
}

static int nt35510_ldi_enable(struct lcd_info *lcd)
{
	int ret = 0;

 	return ret;
}

static int nt35510_ldi_disable(struct lcd_info *lcd)
{
	int ret = 0;

	dev_info(&lcd->ld->dev, "+ %s\n", __func__);

	nt35510_write(lcd, SEQ_DISPLAY_OFF, ARRAY_SIZE(SEQ_DISPLAY_OFF));

	msleep(10);

	nt35510_write(lcd, SEQ_SLEEP_IN, ARRAY_SIZE(SEQ_SLEEP_IN));

	msleep(120);

	dev_info(&lcd->ld->dev, "- %s\n", __func__);

	return ret;
}

static int nt35510_power_on(struct lcd_info *lcd)
{
	int ret = 0;

	dev_info(&lcd->ld->dev, "+ %s\n", __func__);

	if (!lcd->ldi_enable) {
	ret = nt35510_ldi_init(lcd);
	}
	if (ret) {
		dev_err(&lcd->ld->dev, "failed to initialize ldi.\n");
		goto err;
	}

 	ret = nt35510_ldi_enable(lcd);
	if (ret) {
		dev_err(&lcd->ld->dev, "failed to enable ldi.\n");
		goto err;
	}

	lcd->ldi_enable = 1;

#ifdef FEATURE_KTD283
	update_brightness(lcd, 1);
#endif

	dev_info(&lcd->ld->dev, "- %s\n", __func__);
err:
	return ret;
}

static int nt35510_power_off(struct lcd_info *lcd)
{
	int ret = 0;

	dev_info(&lcd->ld->dev, "+ %s\n", __func__);

	lcd->ldi_enable = 0;

	ret = nt35510_ldi_disable(lcd);

	dev_info(&lcd->ld->dev, "- %s\n", __func__);

	return ret;
}

static int nt35510_power(struct lcd_info *lcd, int power)
{
	int ret = 0;

	lcd->dsim->cmd_transfer = 1;

	if (POWER_IS_ON(power) && !POWER_IS_ON(lcd->power))
		ret = nt35510_power_on(lcd);
	else if (!POWER_IS_ON(power) && POWER_IS_ON(lcd->power))
		ret = nt35510_power_off(lcd);

	if (!ret)
		lcd->power = power;

	lcd->dsim->cmd_transfer = 0;

	return ret;
}

static int nt35510_set_power(struct lcd_device *ld, int power)
{
	struct lcd_info *lcd = lcd_get_data(ld);

	if (power != FB_BLANK_UNBLANK && power != FB_BLANK_POWERDOWN &&
		power != FB_BLANK_NORMAL) {
		dev_err(&lcd->ld->dev, "power value should be 0, 1 or 4.\n");
		return -EINVAL;
	}

	return nt35510_power(lcd, power);
}

static int nt35510_get_power(struct lcd_device *ld)
{
	struct lcd_info *lcd = lcd_get_data(ld);

	return lcd->power;
}

static int nt35510_check_fb(struct lcd_device *ld, struct fb_info *fb)
{
	/* struct s3cfb_window *win = fb->par;
	struct lcd_info *lcd = lcd_get_data(ld);

	dev_info(&lcd->ld->dev, "%s, fb%d\n", __func__, win->id);*/
	return 0;
}

static struct lcd_ops nt35510_lcd_ops = {
	.set_power = nt35510_set_power,
	.get_power = nt35510_get_power,
	.check_fb  = nt35510_check_fb,
};

#ifdef FEATURE_KTD283
static int ktd283_set_brightness(struct backlight_device *bd)
{
	int ret = 0;
	int brightness = bd->props.brightness;
	struct lcd_info *lcd = bl_get_data(bd);

	if (brightness < MIN_BRIGHTNESS ||
		brightness > bd->props.max_brightness) {
		dev_err(&bd->dev, "lcd brightness should be %d to %d. now %d\n",
			MIN_BRIGHTNESS, MAX_BRIGHTNESS, brightness);
		return -EINVAL;
	}

	if (lcd->ldi_enable) {
		ret = update_brightness(lcd, 0);
		if (ret < 0) {
			dev_err(&lcd->ld->dev, "err in %s\n", __func__);
			return -EINVAL;
		}
	}

	return ret;
}

static int ktd283_get_brightness(struct backlight_device *bd)
{
	struct lcd_info *lcd = bl_get_data(bd);

	return lcd->current_bl;
}

static const struct backlight_ops ktd283_backlight_ops  = {
	.get_brightness = ktd283_get_brightness,
	.update_status = ktd283_set_brightness,
};
#endif

static ssize_t lcd_type_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	char temp[] = "BOE_NT35510\n";

	strcat(buf, temp);
	return strlen(buf);
}

static DEVICE_ATTR(lcd_type, 0444, lcd_type_show, NULL);

static ssize_t auto_brightness_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct lcd_info *lcd = dev_get_drvdata(dev);
	char temp[3];

	sprintf(temp, "%d\n", lcd->auto_brightness);
	strcpy(buf, temp);

	return strlen(buf);
}

static ssize_t auto_brightness_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t size)
{
	struct lcd_info *lcd = dev_get_drvdata(dev);
	int value;
	int rc;

	rc = kstrtoul(buf, (unsigned int)0, (unsigned long *)&value);
	if (rc < 0)
		return rc;
	else {
		if (lcd->auto_brightness != value) {
			dev_info(dev, "%s - %d, %d\n", __func__, lcd->auto_brightness, value);
			mutex_lock(&lcd->bl_lock);
			lcd->auto_brightness = value;
			mutex_unlock(&lcd->bl_lock);
		}
	}
	return size;
}

static DEVICE_ATTR(auto_brightness, 0644, auto_brightness_show, auto_brightness_store);

static int nt35510_probe(struct mipi_dsim_device *dsim)
{
	int ret = 0;
	struct lcd_info *lcd;

	lcd = kzalloc(sizeof(struct lcd_info), GFP_KERNEL);
	if (!lcd) {
		pr_err("failed to allocate for lcd\n");
		ret = -ENOMEM;
		goto err_alloc;
	}

	g_lcd = lcd;

	lcd->ld = lcd_device_register("panel", dsim->dev, lcd, &nt35510_lcd_ops);
	if (IS_ERR(lcd->ld)) {
		pr_err("failed to register lcd device\n");
		ret = PTR_ERR(lcd->ld);
		goto out_free_lcd;
	}

#ifdef FEATURE_KTD283
	lcd->bd = backlight_device_register("panel", dsim->dev, lcd, &ktd283_backlight_ops, NULL);
	if (IS_ERR(lcd->bd)) {
		pr_err("failed to register backlight device\n");
		ret = PTR_ERR(lcd->bd);
		goto out_free_backlight;
	}

	lcd->bd->props.max_brightness = MAX_BRIGHTNESS;
	lcd->bd->props.brightness = DEFAULT_BRIGHTNESS;
	lcd->bl = 0;
	lcd->current_bl = lcd->bl;
#endif
	lcd->auto_brightness = 0;
	lcd->dev = dsim->dev;
	lcd->dsim = dsim;
	lcd->power = FB_BLANK_POWERDOWN; /* FB_BLANK_UNBLANK; */
	lcd->ldi_enable = 1;
	lcd->connected = 1;

	ret = device_create_file(&lcd->ld->dev, &dev_attr_lcd_type);
	if (ret < 0)
		dev_err(&lcd->ld->dev, "failed to add sysfs entries, %d\n", __LINE__);

	ret = device_create_file(&lcd->bd->dev, &dev_attr_auto_brightness);
	if (ret < 0)
		dev_err(&lcd->ld->dev, "failed to add sysfs entries, %d\n", __LINE__);

	mutex_init(&lcd->lock);
	mutex_init(&lcd->bl_lock);

	 /*nt35510_read_id(lcd, lcd->id);*/
	/*lcd->id[1] = NEW_LCD_ID;*/

	/*dev_info(&lcd->ld->dev, "ID: %x, %x, %x\n", lcd->id[0], lcd->id[1], lcd->id[2]);*/

	dev_info(&lcd->ld->dev, "%s lcd panel driver has been probed.\n", dev_name(dsim->dev));

	return 0;

#ifdef FEATURE_KTD283
out_free_backlight:
	lcd_device_unregister(lcd->ld);
	kfree(lcd);
	return ret;
#endif

out_free_lcd:
	kfree(lcd);
	return ret;

err_alloc:
	return ret;
}

static int nt35510_displayon(struct mipi_dsim_device *dsim)
{
	struct lcd_info *lcd = g_lcd;

	nt35510_power(lcd, FB_BLANK_UNBLANK);

	return 1;
}

static int nt35510_suspend(struct mipi_dsim_device *dsim)
{
	struct lcd_info *lcd = g_lcd;

	nt35510_power(lcd, FB_BLANK_POWERDOWN);

	/* TODO */

	return 1;
}

static int nt35510_resume(struct mipi_dsim_device *dsim)
{
	return 1;
}

struct mipi_dsim_lcd_driver nt35510_mipi_lcd_driver = {
	.probe		= nt35510_probe,
	.displayon	= nt35510_displayon,
	.suspend	= nt35510_suspend,
	.resume		= nt35510_resume,
};
