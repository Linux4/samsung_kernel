/* linux/drivers/video/backlight/hx8394_mipi_lcd.c
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

#include <linux/i2c.h>
#include <linux/slab.h>
#include <linux/version.h>
#include <linux/clk.h>
#include <linux/moduleparam.h>
#include <linux/vmalloc.h>
#include <linux/platform_device.h>
#include <linux/firmware.h>
#include <mach/regs-gpio.h>
#include <mach/regs-clock.h>
#include <plat/clock.h>

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
#include <linux/regmap.h>

#include <video/mipi_display.h>
#include <plat/dsim.h>
#include <plat/mipi_dsi.h>
#include <plat/gpio-cfg.h>

#include "hx8394_param.h"

#define LDI_ID_REG		0x04
#define LDI_ID_LEN		3

#define MAX_BRIGHTNESS 255
#define MIN_BRIGHTNESS 0
#define MIN_BRIGHTNESS_PLATFORM 4
#define DEFAULT_BRIGHTNESS 130
#define BRIGHTNESS_REG	0x51

#define POWER_IS_ON(pwr)		((pwr) <= FB_BLANK_NORMAL)

#ifdef CONFIG_MACH_MEGA2LTE_USA_ATT
#define OLD_LCD_REV 4
#else
#define OLD_LCD_REV 6
#endif

enum cabc_modes {
	OFF,
	UI,
	STILL,
	MOVING,
};

struct lcd_info {
	unsigned int			bl;
	unsigned int			current_bl;
	struct backlight_device		*bd;
	unsigned int			auto_brightness;
	unsigned int			ldi_enable;
	unsigned int			power;
	struct mutex			lock;
	struct mutex			bl_lock;
	struct mutex			cabc_lock;
	unsigned char			id[LDI_ID_LEN];

	struct device			*dev;
	struct lcd_device		*ld;
	struct mipi_dsim_lcd_device	*dsim_dev;
	unsigned int			irq;
	unsigned int			gpio;
	unsigned int			connected;
	unsigned int			cabc_mode;
	unsigned int			current_cabc_mode;
	unsigned int			cabc_test_mode;
	unsigned int			cabc_duty;
	unsigned int			current_cabc_duty;

	struct delayed_work		brightness_delay;
	unsigned int			brightness_updated;

	struct mipi_dsim_device		*dsim;
};

static struct lcd_info *g_lcd;
extern int system_rev;
extern void s5p_dynamic_clk_set(unsigned int mode);

/************************
REV00 1G : 0x0
REV00 2G : 0x1
REV01 2G : 0x2
REV01 1.5G :  0x3
*************************/

static int hx8394_write(struct lcd_info *lcd, const u8 *seq, u32 len)
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

static int hx8394_read(struct lcd_info *lcd, u8 addr, u8 *buf, u32 len)
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
		dev_err(&lcd->ld->dev, "%s failed: exceed retry count\n", __func__);
		goto read_err;
	}
	ret = s5p_mipi_dsi_rd_data(lcd->dsim, cmd, addr, len, buf, 0);
	if (ret != len) {
		dev_dbg(&lcd->ld->dev, "mipi_read failed retry ..\n");
		retry--;
		goto read_data;
	}
read_err:
	mutex_unlock(&lcd->lock);
	return ret;
}

static void hx8394_read_id(struct lcd_info *lcd, u8 *buf)
{
	int ret = 0;
	lcd->connected = 1;

	ret = hx8394_read(lcd, LDI_ID_REG, buf, LDI_ID_LEN);
	if (ret < 1) {
		lcd->connected = 0;
		dev_info(&lcd->ld->dev, "panel is not connected well\n");
	}
}

static int update_cabc_duty(struct lcd_info *lcd, u8 force)
{
	if (!lcd->cabc_mode)
		lcd->cabc_duty = CABC_DUTY_100;
	else
		lcd->cabc_duty = CABC_DUTY_82;

	if (force || (lcd->current_cabc_duty != lcd->cabc_duty)) {
		hx8394_write(lcd, SEQ_CABC_DUTY[lcd->cabc_duty], CABC_DUTY_SIZE);

		dev_info(&lcd->ld->dev, "%s : %d \n", __func__, lcd->cabc_duty);
	}
	lcd->current_cabc_duty = lcd->cabc_duty;

	return 0;
}

static int update_cabc_mode(struct lcd_info *lcd, u8 force)
{
	mutex_lock(&lcd->cabc_lock);

	if (force || (lcd->current_cabc_mode != lcd->cabc_mode) ) {

		if(lcd->cabc_mode) {
			msleep(5);
			hx8394_write(lcd,SEQ_CABC_PWM[lcd->cabc_mode], CABC_PWM_SIZE);
			msleep(5);
		}

		hx8394_write(lcd,SEQ_CABC_MODE[lcd->cabc_mode], CABC_MODE_SIZE);
		msleep(5);

		lcd->current_cabc_mode = lcd->cabc_mode;

		dev_info(&lcd->ld->dev, "cabc_mode : %d", lcd->cabc_mode);
	}

	mutex_unlock(&lcd->cabc_lock);
	return 0;
}

static int update_brightness(struct lcd_info *lcd, u8 force)
{
	u32 brightness;
	unsigned char bl_reg[2];
	mutex_lock(&lcd->bl_lock);

	brightness = lcd->bd->props.brightness;

	lcd->bl = brightness;

	/*After CABC keysting app control CABC ON/OFF, All CABC control of platform will be rejected*/
	/*CABC on/off control will be under CABC keystring app untill reboot*/
	if (lcd->cabc_test_mode == 0 || lcd->cabc_test_mode == 1)
		lcd->cabc_mode = lcd->cabc_test_mode;

	if ((force) || ((lcd->ldi_enable) && (lcd->current_bl != lcd->bl))) {
		bl_reg[0] = BRIGHTNESS_REG;

		/*	platform		4~255			:	251	*/
		/*	DDI			0x2~0xE2(226)	:	224	*/
		bl_reg[1] = ((lcd->bl - MIN_BRIGHTNESS_PLATFORM) * 224 / 251) + 2 ;

		/*Under Platform brightness Min value (4) , Set LDI brightness value as 0*/
		if (lcd->bl < 4)
			bl_reg[1] = 0x0;

		hx8394_write(lcd ,bl_reg, ARRAY_SIZE(bl_reg));

		update_cabc_mode(lcd, force);

		update_cabc_duty(lcd, force);

		lcd->current_bl = lcd->bl;

		lcd->brightness_updated = 1;

		dev_info(&lcd->ld->dev, "brightness=%d, bl=%d \n", \
			brightness, bl_reg[1]);
	}

	mutex_unlock(&lcd->bl_lock);
	return 0;
}

static void brightness_delay_work(struct work_struct *work)
{
	struct lcd_info * lcd = g_lcd;

	if (lcd->ldi_enable && lcd->brightness_updated == 0) {
		dev_info(&lcd->ld->dev, "%s is called \n", __func__);
		update_brightness(lcd, 1);
	}
}

static int hx8394_ldi_init(struct lcd_info *lcd)
{
	int ret = 0;
	hx8394_read_id(lcd, lcd->id);
	dev_info(&lcd->ld->dev, "ID: %x, %x, %x\n", lcd->id[0], lcd->id[1], lcd->id[2]);

	hx8394_write(lcd, SEQ_HX8394_POWER_0, ARRAY_SIZE(SEQ_HX8394_POWER_0));
	hx8394_write(lcd, SEQ_HX8394_POWER_1, ARRAY_SIZE(SEQ_HX8394_POWER_1));
	hx8394_write(lcd, SEQ_HX8394_POWER_2, ARRAY_SIZE(SEQ_HX8394_POWER_2));

	hx8394_write(lcd, SEQ_HX8394_INIT_0, ARRAY_SIZE(SEQ_HX8394_INIT_0));
	hx8394_write(lcd, SEQ_HX8394_INIT_1, ARRAY_SIZE(SEQ_HX8394_INIT_1));
	hx8394_write(lcd, SEQ_HX8394_INIT_2, ARRAY_SIZE(SEQ_HX8394_INIT_2));
	hx8394_write(lcd, SEQ_HX8394_INIT_3, ARRAY_SIZE(SEQ_HX8394_INIT_3));
	hx8394_write(lcd, SEQ_HX8394_INIT_4, ARRAY_SIZE(SEQ_HX8394_INIT_4));
	hx8394_write(lcd, SEQ_HX8394_INIT_5, ARRAY_SIZE(SEQ_HX8394_INIT_5));
	hx8394_write(lcd, SEQ_HX8394_INIT_6, ARRAY_SIZE(SEQ_HX8394_INIT_6));
	hx8394_write(lcd, SEQ_HX8394_INIT_7, ARRAY_SIZE(SEQ_HX8394_INIT_7));
	hx8394_write(lcd, SEQ_HX8394_INIT_8, ARRAY_SIZE(SEQ_HX8394_INIT_8));
	hx8394_write(lcd, SEQ_HX8394_INIT_9, ARRAY_SIZE(SEQ_HX8394_INIT_9));
	hx8394_write(lcd, SEQ_HX8394_INIT_10, ARRAY_SIZE(SEQ_HX8394_INIT_10));
	if(system_rev <= OLD_LCD_REV)
		hx8394_write(lcd, SEQ_HX8394_INIT_11_old, ARRAY_SIZE(SEQ_HX8394_INIT_11_old));
	else
		hx8394_write(lcd, SEQ_HX8394_INIT_11, ARRAY_SIZE(SEQ_HX8394_INIT_11));

	hx8394_write(lcd, SEQ_HX8394_GAMMA_0, ARRAY_SIZE(SEQ_HX8394_GAMMA_0));
	hx8394_write(lcd, SEQ_HX8394_GAMMA_1, ARRAY_SIZE(SEQ_HX8394_GAMMA_1));
	hx8394_write(lcd, SEQ_HX8394_GAMMA_2, ARRAY_SIZE(SEQ_HX8394_GAMMA_2));
	hx8394_write(lcd, SEQ_HX8394_GAMMA_3, ARRAY_SIZE(SEQ_HX8394_GAMMA_3));
	hx8394_write(lcd, SEQ_HX8394_GAMMA_4, ARRAY_SIZE(SEQ_HX8394_GAMMA_4));
	hx8394_write(lcd, SEQ_HX8394_GAMMA_5, ARRAY_SIZE(SEQ_HX8394_GAMMA_5));
	hx8394_write(lcd, SEQ_HX8394_GAMMA_6, ARRAY_SIZE(SEQ_HX8394_GAMMA_6));
	hx8394_write(lcd, SEQ_HX8394_GAMMA_7, ARRAY_SIZE(SEQ_HX8394_GAMMA_7));

	if (system_rev < 0x07) /*For Under evt4 Panel*/
		msleep(120);

	hx8394_write(lcd, SEQ_DISPLAY_SLEEP_OUT, ARRAY_SIZE(SEQ_DISPLAY_SLEEP_OUT));
	msleep(180);

	if (system_rev <= 0x1) { /*for EVT2 panel*/
		msleep(30);
		hx8394_write(lcd ,SEQ_HX8394_INIT_8, ARRAY_SIZE(SEQ_HX8394_INIT_8));
		msleep(30);
		hx8394_write(lcd ,SEQ_HX8394_INIT_9, ARRAY_SIZE(SEQ_HX8394_INIT_9));
	}
	update_brightness(lcd, 1);

	hx8394_write(lcd,SEQ_BRIGHTNESS2, ARRAY_SIZE(SEQ_BRIGHTNESS2));

	return ret;
}

static int hx8394_ldi_enable(struct lcd_info *lcd)
{
	int ret = 0;

	hx8394_write(lcd,SEQ_DISPLAY_ON, ARRAY_SIZE(SEQ_DISPLAY_ON));

	return ret;
}

/*Function For CABC keystirng app*/
extern void set_cabc(unsigned int mode)
{
	struct lcd_info *lcd = g_lcd;

	/*After CABC keysting control CABC ON/OFF, All CABC control of platform will be rejected*/
	/*CABC on/off control will be under CABC keystring app untill reboot*/

	lcd->cabc_test_mode = mode;

	update_brightness(lcd, 1);
}

static int hx8394_ldi_disable(struct lcd_info *lcd)
{
	int ret = 0;

	dev_info(&lcd->ld->dev, "+ %s\n", __func__);

	hx8394_write(lcd, SEQ_DISPLAY_OFF, ARRAY_SIZE(SEQ_DISPLAY_OFF));

	msleep(50);

	hx8394_write(lcd, SEQ_DISPLAY_SLEEP_IN, ARRAY_SIZE(SEQ_DISPLAY_SLEEP_IN));

	msleep(50);

	dev_info(&lcd->ld->dev, "- %s\n", __func__);

	return ret;
}

static int hx8394_power_on(struct lcd_info *lcd)
{
	int ret = 0;

	dev_info(&lcd->ld->dev, "+ %s\n", __func__);

	if (!lcd->ldi_enable) {
	ret = hx8394_ldi_init(lcd);
	}
	if (ret) {
		dev_err(&lcd->ld->dev, "failed to initialize ldi.\n");
		goto err;
	}

	ret = hx8394_ldi_enable(lcd);
	if (ret) {
		dev_err(&lcd->ld->dev, "failed to enable ldi.\n");
		goto err;
	}

	lcd->ldi_enable = 1;

	dev_info(&lcd->ld->dev, "- %s\n", __func__);
err:
	return ret;
}

static int hx8394_power_off(struct lcd_info *lcd)
{
	int ret = 0;

	dev_info(&lcd->ld->dev, "+ %s\n", __func__);

	hx8394_write(lcd, SEQ_BRIGHTNESS_OFF, ARRAY_SIZE(SEQ_BRIGHTNESS_OFF));

	lcd->ldi_enable = 0;

	ret = hx8394_ldi_disable(lcd);

	dev_info(&lcd->ld->dev, "- %s\n", __func__);

	return ret;
}

static int hx8394_power(struct lcd_info *lcd, int power)
{
	int ret = 0;

	if (POWER_IS_ON(power) && !POWER_IS_ON(lcd->power))
		ret = hx8394_power_on(lcd);
	else if (!POWER_IS_ON(power) && POWER_IS_ON(lcd->power))
		ret = hx8394_power_off(lcd);

	if (!ret)
		lcd->power = power;


	return ret;
}

static int hx8394_set_power(struct lcd_device *ld, int power)
{
	struct lcd_info *lcd = lcd_get_data(ld);

	if (power != FB_BLANK_UNBLANK && power != FB_BLANK_POWERDOWN &&
		power != FB_BLANK_NORMAL) {
		dev_err(&lcd->ld->dev, "power value should be 0, 1 or 4.\n");
		return -EINVAL;
	}

	return hx8394_power(lcd, power);
}

static int hx8394_get_power(struct lcd_device *ld)
{
	struct lcd_info *lcd = lcd_get_data(ld);

	return lcd->power;
}

static int hx8394_check_fb(struct lcd_device *ld, struct fb_info *fb)
{
	/* struct s3cfb_window *win = fb->par;
	struct lcd_info *lcd = lcd_get_data(ld);

	dev_info(&lcd->ld->dev, "%s, fb%d\n", __func__, win->id);*/
	return 0;
}

static struct lcd_ops hx8394_lcd_ops = {
	.set_power = hx8394_set_power,
	.get_power = hx8394_get_power,
	.check_fb  = hx8394_check_fb,
};

static int hx8394_set_brightness(struct backlight_device *bd)
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

static int hx8394_get_brightness(struct backlight_device *bd)
{
	struct lcd_info *lcd = bl_get_data(bd);

	return lcd->current_bl;
}

static const struct backlight_ops hx8394_backlight_ops  = {
	.get_brightness = hx8394_get_brightness,
	.update_status = hx8394_set_brightness,
};

static ssize_t lcd_type_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct lcd_info *lcd = dev_get_drvdata(dev);
	char temp[15];

	sprintf(temp, "SDC_%02X%02X%02X\n", lcd->id[0], lcd->id[1], lcd->id[2]);

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

			if (lcd->auto_brightness > 0 && lcd->auto_brightness < 5)
				lcd->cabc_mode = 1;
			else
				lcd->cabc_mode = 0;

			if (lcd->ldi_enable) {
				if (lcd->auto_brightness > 0)
					update_brightness(lcd, 1);
				else {
					schedule_delayed_work(&lcd->brightness_delay, msecs_to_jiffies(250));
					lcd->brightness_updated = 0;
				}
			}

		}
	}
	return size;
}

static ssize_t cabc_mode_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t size)
{
	struct lcd_info *lcd = dev_get_drvdata(dev);
	unsigned int value;
	int rc;

	rc = kstrtoul(buf, (unsigned int)0, (unsigned long *)&value);
	if (rc < 0)
		return rc;
	else {
		lcd->cabc_mode = value;
		dev_info(&lcd->ld->dev, "%s : %d \n", \
			 __func__, lcd->cabc_mode);

		if( lcd->current_cabc_mode != lcd->cabc_mode ) {
				update_brightness(lcd, 1);
			}
		}

	return size;
}

static ssize_t dynamic_clk_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t size)
{
	unsigned int value;
	int rc;

	rc = kstrtoul(buf, (unsigned int)0, (unsigned long *)&value);
	if (rc < 0)
		return rc;
	else
		s5p_dynamic_clk_set(value);

	return size;
}

static DEVICE_ATTR(dynamic_clk, 0644, NULL, dynamic_clk_store);
static DEVICE_ATTR(auto_brightness, 0644, auto_brightness_show, auto_brightness_store);
static DEVICE_ATTR(cabc_mode, 0644, NULL, cabc_mode_store);

static int hx8394_probe(struct mipi_dsim_device *dsim)
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

	lcd->ld = lcd_device_register("panel", dsim->dev, lcd, &hx8394_lcd_ops);
	if (IS_ERR(lcd->ld)) {
		pr_err("failed to register lcd device\n");
		ret = PTR_ERR(lcd->ld);
		goto out_free_lcd;
	}

	lcd->bd = backlight_device_register("panel", dsim->dev, lcd, &hx8394_backlight_ops, NULL);
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
	lcd->cabc_mode = CABC_MODE_OFF;
	lcd->current_cabc_mode = CABC_MODE_OFF;
	lcd->cabc_duty = CABC_DUTY_MAX;
	lcd->current_cabc_duty = CABC_DUTY_MAX;
	lcd->cabc_test_mode = 2; /* 0 : force_off   1 : force_on   2: normal */

	ret = device_create_file(&lcd->ld->dev, &dev_attr_lcd_type);
	if (ret < 0)
		dev_err(&lcd->ld->dev, "failed to add sysfs entries, %d\n", __LINE__);

	ret = device_create_file(&lcd->bd->dev, &dev_attr_auto_brightness);
	if (ret < 0)
		dev_err(&lcd->ld->dev, "failed to add sysfs entries, %d\n", __LINE__);

	ret = device_create_file(&lcd->bd->dev, &dev_attr_cabc_mode);
	if (ret < 0)
		dev_err(&lcd->ld->dev, "failed to add sysfs entries, %d\n", __LINE__);

	ret = device_create_file(&lcd->bd->dev, &dev_attr_dynamic_clk);
	if (ret < 0)
		dev_err(&lcd->ld->dev, "failed to add sysfs entries, %d\n", __LINE__);

	mutex_init(&lcd->lock);
	mutex_init(&lcd->bl_lock);
	mutex_init(&lcd->cabc_lock);

	hx8394_read_id(lcd, lcd->id);

	INIT_DELAYED_WORK(&lcd->brightness_delay, brightness_delay_work);

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

static int hx8394_displayon(struct mipi_dsim_device *dsim)
{
	struct lcd_info *lcd = g_lcd;

	hx8394_power(lcd, FB_BLANK_UNBLANK);

	return 1;
}

static int hx8394_suspend(struct mipi_dsim_device *dsim)
{
	struct lcd_info *lcd = g_lcd;

	hx8394_power(lcd, FB_BLANK_POWERDOWN);

	/* TODO */

	return 1;
}

static int hx8394_resume(struct mipi_dsim_device *dsim)
{
	return 1;
}

struct mipi_dsim_lcd_driver hx8394_mipi_lcd_driver = {
	.probe		= hx8394_probe,
	.displayon	= hx8394_displayon,
	.suspend	= hx8394_suspend,
	.resume		= hx8394_resume,
};
