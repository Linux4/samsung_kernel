/* linux/drivers/video/backlight/s6d7aa0_mipi_lcd.c
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
#if defined(CONFIG_FB_S5P_MDNIE_LITE)
#include <linux/mdnie.h>
#endif

#include "s6d7aa0_param.h"

#define MIN_BRIGHTNESS 0
#define MAX_BRIGHTNESS 255
#define DEFAULT_BRIGHTNESS 119

#define MIN_GAMMA			2
#define MAX_GAMMA			175


#define POWER_IS_ON(pwr)		((pwr) <= FB_BLANK_NORMAL)

#define LDI_ID_REG			0xD8	/* LCD ID1,ID2,ID3 */
#define LDI_ID_LEN			3

struct lcd_info {
	unsigned int			bl;
	unsigned int			current_bl;
	struct backlight_device		*bd;
#if defined(CONFIG_FB_S5P_MDNIE_LITE)
	struct mdnie_device	*md;
	int	mdnie_addr;
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

static int s6d7aa0_write(struct lcd_info *lcd, const u8 *seq, u32 len)
{
	int ret;
	int retry;
	u8 cmd;

	if (!lcd->dsim->pd->lcd_connected)
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

static int s6d7aa0_read(struct lcd_info *lcd, u8 addr, u8 *buf, u32 len)
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
	ret = s5p_mipi_dsi_rd_data(lcd->dsim, cmd, addr, len, buf, 1);
	if (ret < 0) {
		dev_dbg(&lcd->ld->dev, "mipi_read failed retry ..\n");
		retry--;
		goto read_data;
	}
read_err:
	mutex_unlock(&lcd->lock);
	return ret;
}

static int get_backlight_level_from_brightness(int brightness)
{
	int backlightlevel;

	backlightlevel = (brightness * MAX_GAMMA) / MAX_BRIGHTNESS;

	return backlightlevel;
}


static int update_brightness(struct lcd_info *lcd, u8 force)
{
	u32 brightness;

	mutex_lock(&lcd->bl_lock);

	brightness = lcd->bd->props.brightness;

	lcd->bl = get_backlight_level_from_brightness(brightness);

	if ((force) || ((lcd->ldi_enable) && (lcd->current_bl != lcd->bl))) {

		lcd->current_bl = lcd->bl;
		SEQ_BRIGHTNESS_SELECT[1] = lcd->bl;
		s6d7aa0_write(lcd, SEQ_BRIGHTNESS_SELECT, \
			ARRAY_SIZE(SEQ_BRIGHTNESS_SELECT));

		dev_info(&lcd->ld->dev, "brightness=%d lcd->bl=%d\n", brightness, lcd->bl);
	}

	mutex_unlock(&lcd->bl_lock);

	return 0;
}

int s6d7aa0_mdnie_read(struct device *dev, u8 addr, u8 *buf, u32 len)
{
	struct lcd_info *lcd = dev_get_drvdata(dev);
	int ret;

	dev_info(&lcd->ld->dev, "%s  addr = %x\n", __func__, addr);

	/* Base addr & read data */
	ret = s6d7aa0_read(lcd, addr, buf, len);
	if (ret < 1) {
		dev_info(&lcd->ld->dev, "panel is not connected well\n");
	}

	return ret;
}

int s6d7aa0_mdnie_write(struct device *dev, const u8 *seq, u32 len)
{
	struct lcd_info *lcd = dev_get_drvdata(dev);
	int ret;

	/* lock/unlock key */
	if (!len)
		return 0;

	/* Base addr & read data */
	ret = s6d7aa0_write(lcd, seq, len);
	if (ret < len)
		return -EINVAL;

	/* msleep(17*2); */ /* wait 1 frame */

	return len;
}



static int s6d7aa0_ldi_init(struct lcd_info *lcd)
{
	int ret = 0;
	s6d7aa0_write(lcd, SEQ_PASSWD1, ARRAY_SIZE(SEQ_PASSWD1));
	s6d7aa0_write(lcd, SEQ_PASSWD2, ARRAY_SIZE(SEQ_PASSWD2));
	s6d7aa0_write(lcd, SEQ_PASSWD3, ARRAY_SIZE(SEQ_PASSWD3));
	s6d7aa0_write(lcd, SEQ_OTP_RELOAD, ARRAY_SIZE(SEQ_OTP_RELOAD));
	s6d7aa0_write(lcd, SEQ_BL_ON_CTL, ARRAY_SIZE(SEQ_BL_ON_CTL));
	s6d7aa0_write(lcd, SEQ_BC_PARAM_MDNIE, ARRAY_SIZE(SEQ_BC_PARAM_MDNIE));
	s6d7aa0_write(lcd, SEQ_FD_PARAM_MDNIE, ARRAY_SIZE(SEQ_FD_PARAM_MDNIE));
	s6d7aa0_write(lcd, SEQ_FE_PARAM_MDNIE, ARRAY_SIZE(SEQ_FE_PARAM_MDNIE));
	s6d7aa0_write(lcd, SEQ_B3_PARAM, ARRAY_SIZE(SEQ_B3_PARAM));
	s6d7aa0_write(lcd, SEQ_BACKLIGHT_CTL, ARRAY_SIZE(SEQ_BACKLIGHT_CTL));
	s6d7aa0_write(lcd, SEQ_PORCH_CTL, ARRAY_SIZE(SEQ_PORCH_CTL));
	msleep(10);
	s6d7aa0_write(lcd, SEQ_SLEEP_OUT, ARRAY_SIZE(SEQ_SLEEP_OUT));
	msleep(120);
	s6d7aa0_write(lcd, SEQ_PASSWD1_LOCK, ARRAY_SIZE(SEQ_PASSWD1_LOCK));
	s6d7aa0_write(lcd, SEQ_PASSWD2_LOCK, ARRAY_SIZE(SEQ_PASSWD2_LOCK));
	s6d7aa0_write(lcd, SEQ_PASSWD3_LOCK, ARRAY_SIZE(SEQ_PASSWD3_LOCK));
	s6d7aa0_write(lcd, SEQ_TEON_CTL, ARRAY_SIZE(SEQ_TEON_CTL));

	return ret;
}

static int s6d7aa0_ldi_enable(struct lcd_info *lcd)
{
	int ret = 0;

	s6d7aa0_write(lcd, SEQ_DISPLAY_ON, ARRAY_SIZE(SEQ_DISPLAY_ON));
	msleep(10);

	return ret;
}

static int s6d7aa0_ldi_disable(struct lcd_info *lcd)
{
	int ret = 0;

	dev_info(&lcd->ld->dev, "+ %s\n", __func__);

	s6d7aa0_write(lcd, SEQ_BRIGHTNESS_OFF, ARRAY_SIZE(SEQ_BRIGHTNESS_OFF));
	s6d7aa0_write(lcd, SEQ_DISPLAY_OFF, ARRAY_SIZE(SEQ_DISPLAY_OFF));
	msleep(30);
	s6d7aa0_write(lcd, SEQ_PASSWD1, ARRAY_SIZE(SEQ_PASSWD1));
	s6d7aa0_write(lcd, SEQ_BL_OFF_CTL, ARRAY_SIZE(SEQ_BL_OFF_CTL));
	msleep(1);
	s6d7aa0_write(lcd, SEQ_SLEEP_IN, ARRAY_SIZE(SEQ_SLEEP_IN));

	dev_info(&lcd->ld->dev, "- %s\n", __func__);

	return ret;
}

static int s6d7aa0_power_on(struct lcd_info *lcd)
{
	int ret = 0;

	dev_info(&lcd->ld->dev, "+ %s\n", __func__);

	if (!lcd->ldi_enable) {
	ret = s6d7aa0_ldi_init(lcd);
	}
	if (ret) {
		dev_err(&lcd->ld->dev, "failed to initialize ldi.\n");
		goto err;
	}

 	ret = s6d7aa0_ldi_enable(lcd);
	if (ret) {
		dev_err(&lcd->ld->dev, "failed to enable ldi.\n");
		goto err;
	}

	lcd->ldi_enable = 1;

	update_brightness(lcd, 1);

	dev_info(&lcd->ld->dev, "- %s\n", __func__);
err:
	return ret;
}

static int s6d7aa0_power_off(struct lcd_info *lcd)
{
	int ret = 0;

	dev_info(&lcd->ld->dev, "+ %s\n", __func__);

	lcd->ldi_enable = 0;

	ret = s6d7aa0_ldi_disable(lcd);

	dev_info(&lcd->ld->dev, "- %s\n", __func__);

	return ret;
}

static int s6d7aa0_power(struct lcd_info *lcd, int power)
{
	int ret = 0;

	if (POWER_IS_ON(power) && !POWER_IS_ON(lcd->power))
		ret = s6d7aa0_power_on(lcd);
	else if (!POWER_IS_ON(power) && POWER_IS_ON(lcd->power))
		ret = s6d7aa0_power_off(lcd);

	if (!ret)
		lcd->power = power;

	return ret;
}

static int s6d7aa0_set_power(struct lcd_device *ld, int power)
{
	struct lcd_info *lcd = lcd_get_data(ld);

	if (power != FB_BLANK_UNBLANK && power != FB_BLANK_POWERDOWN &&
		power != FB_BLANK_NORMAL) {
		dev_err(&lcd->ld->dev, "power value should be 0, 1 or 4.\n");
		return -EINVAL;
	}

	return s6d7aa0_power(lcd, power);
}

static int s6d7aa0_get_power(struct lcd_device *ld)
{
	struct lcd_info *lcd = lcd_get_data(ld);

	return lcd->power;
}

static int s6d7aa0_check_fb(struct lcd_device *ld, struct fb_info *fb)
{
	/* struct s3cfb_window *win = fb->par;
	struct lcd_info *lcd = lcd_get_data(ld);

	dev_info(&lcd->ld->dev, "%s, fb%d\n", __func__, win->id);*/
	return 0;
}

static struct lcd_ops s6d7aa0_lcd_ops = {
	.set_power = s6d7aa0_set_power,
	.get_power = s6d7aa0_get_power,
	.check_fb  = s6d7aa0_check_fb,
};

static int s6d7aa0_set_brightness(struct backlight_device *bd)
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

static int s6d7aa0_get_brightness(struct backlight_device *bd)
{
	struct lcd_info *lcd = bl_get_data(bd);

	return lcd->current_bl;
}

static const struct backlight_ops s6d7aa0_backlight_ops  = {
	.get_brightness = s6d7aa0_get_brightness,
	.update_status = s6d7aa0_set_brightness,
};

#if defined(CONFIG_FB_S5P_MDNIE_LITE)
static struct mdnie_ops s6d7aa0_mdnie_ops = {
	.write = s6d7aa0_mdnie_write,
	.read = s6d7aa0_mdnie_read,
};
#endif

static ssize_t lcd_type_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	char temp[] = "BOE_BP070WX1\n";

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

static int s6d7aa0_probe(struct mipi_dsim_device *dsim)
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

	lcd->ld = lcd_device_register("panel", dsim->dev, lcd, &s6d7aa0_lcd_ops);
	if (IS_ERR(lcd->ld)) {
		pr_err("failed to register lcd device\n");
		ret = PTR_ERR(lcd->ld);
		goto out_free_lcd;
	}

	lcd->bd = backlight_device_register("panel", dsim->dev, lcd, &s6d7aa0_backlight_ops, NULL);
	if (IS_ERR(lcd->bd)) {
		pr_err("failed to register backlight device\n");
		ret = PTR_ERR(lcd->bd);
		goto out_free_backlight;
	}

	lcd->bd->props.max_brightness = MAX_BRIGHTNESS;
	lcd->bd->props.brightness = DEFAULT_BRIGHTNESS;
	lcd->bl = 0;
	lcd->current_bl = lcd->bl;
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

#if defined(CONFIG_FB_S5P_MDNIE_LITE)
		lcd->md = mdnie_device_register("mdnie", &lcd->ld->dev, &s6d7aa0_mdnie_ops);
#endif

	dev_info(&lcd->ld->dev, "%s lcd panel driver has been probed.\n", dev_name(dsim->dev));

	return 0;

out_free_backlight:
	lcd_device_unregister(lcd->ld);
	kfree(lcd);
	return ret;

out_free_lcd:
	kfree(lcd);
	return ret;

err_alloc:
	return ret;
}

static int s6d7aa0_displayon(struct mipi_dsim_device *dsim)
{
	struct lcd_info *lcd = g_lcd;

	s6d7aa0_power(lcd, FB_BLANK_UNBLANK);

	return 1;
}

static int s6d7aa0_suspend(struct mipi_dsim_device *dsim)
{
	struct lcd_info *lcd = g_lcd;

	s6d7aa0_power(lcd, FB_BLANK_POWERDOWN);

	/* TODO */

	return 1;
}

static int s6d7aa0_resume(struct mipi_dsim_device *dsim)
{
	return 1;
}

struct mipi_dsim_lcd_driver s6d7aa0_mipi_lcd_driver = {
	.probe		= s6d7aa0_probe,
	.displayon	= s6d7aa0_displayon,
	.suspend	= s6d7aa0_suspend,
	.resume		= s6d7aa0_resume,
};

static int s6d7aa0_init(void)
{
	return 0 ;
#if 0
	s5p_mipi_dsi_register_lcd_driver(&ea8061v_mipi_lcd_driver);
	exynos_mipi_dsi_register_lcd_driver
#endif
}

static void s6d7aa0_exit(void)
{
	return;
}

module_init(s6d7aa0_init);
module_exit(s6d7aa0_exit);

MODULE_DESCRIPTION("MIPI-DSI s6d7aa0 (800*1280) Panel Driver");
MODULE_LICENSE("GPL");
