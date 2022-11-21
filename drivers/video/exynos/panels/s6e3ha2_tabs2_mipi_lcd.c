/* drivers/video/decon_display/s6e3ha2_tabs2_mipi_lcd.c
 *
 * Samsung SoC MIPI LCD driver.
 *
 * Copyright (c) 2015 Samsung Electronics
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
#include <linux/interrupt.h>
#include <linux/workqueue.h>
#include <linux/backlight.h>
#include <linux/lcd.h>
#include <linux/rtc.h>
#include <linux/reboot.h>
#include <linux/of_gpio.h>
#include <linux/syscalls.h>
#include <linux/irq.h>
#include <linux/of_device.h>

#include <video/mipi_display.h>
#include "../decon_display/decon_mipi_dsi.h"

#include "s6e3ha2_tabs2_param.h"
#include "dynamic_aid_s6e3ha2_tabs2.h"

#if defined(CONFIG_DECON_MDNIE_LITE)
#include "mdnie.h"
#include "mdnie_lite_table_tabs28.h"
#endif

#define POWER_IS_ON(pwr)				(pwr <= FB_BLANK_NORMAL)
#define LEVEL_IS_HBM(brightness)			(brightness == EXTEND_BRIGHTNESS)
#define LEVEL_IS_CAPS_OFF(level)			(level <= IBRIGHTNESS_39NT)
#define LEVEL_IS_ACL_OFF(brightness)			(MAX_BRIGHTNESS <= brightness && brightness <= EXTEND_BRIGHTNESS - 1)

#define NORMAL_TEMPERATURE		25	/* 25 degrees Celsius */

#define EXTEND_BRIGHTNESS		355
#define MIN_BRIGHTNESS			0
#define MAX_BRIGHTNESS			255
#define DEFAULT_BRIGHTNESS		122

#define DEFAULT_GAMMA_INDEX		IBRIGHTNESS_162NT

#define LDI_ID_REG			0x04
#define LDI_ID_LEN			3
#define LDI_CODE_REG			0xD6
#define LDI_CODE_LEN			5
#define LDI_MTP_REG			0xC8
#define LDI_MTP_LEN			44	/* MTP + Manufacture Date */
#define LDI_TSET_REG			0xB8
#define LDI_TSET_LEN			8	/* TSET: Global para 8th */
#define TSET_PARAM_SIZE			(LDI_TSET_LEN + 1)
#define LDI_ELVSS_REG			0xB6
#define LDI_ELVSS_LEN			(ELVSS_PARAM_SIZE - 1)

#define LDI_COORDINATE_REG		0xA1
#define LDI_COORDINATE_LEN		4

#ifdef SMART_DIMMING_DEBUG
#define smtd_dbg(format, arg...)	printk(format, ##arg)
#else
#define smtd_dbg(format, arg...)
#endif

#define GET_UPPER_4BIT(x)		((x >> 4) & 0xF)
#define GET_LOWER_4BIT(x)		(x & 0xF)


union elvss_info {
	u32 value;
	struct {
		u8 mps;
		u8 offset;
		u8 elvss_base;
		u8 reserved;
	};
};

struct lcd_info {
	unsigned int			bl;
	unsigned int			brightness;
	unsigned int			acl_enable;
	unsigned int			siop_enable;
	unsigned int			caps_enable;
	unsigned int			current_acl;
	unsigned int			current_bl;
	union elvss_info		current_elvss;
	unsigned int			current_hbm;
	unsigned int			current_vint;
	unsigned int			current_brightness;
	unsigned int			ldi_enable;
	unsigned int			power;
	struct mutex			lock;
	struct mutex			bl_lock;

	struct device			*dev;
	struct lcd_device		*ld;
	struct backlight_device		*bd;
	unsigned char			id[LDI_ID_LEN];
	unsigned char			code[LDI_CODE_LEN];
	unsigned char			**gamma_table;
	unsigned char			**elvss_table[CAPS_MAX][TEMP_MAX];
	unsigned char			*vint_table[VINT_STATUS_MAX];
	struct dynamic_aid_param_t	daid;
	unsigned char			aor[IBRIGHTNESS_MAX][AID_PARAM_SIZE];
	unsigned char			inter_aor_info[EXTEND_BRIGHTNESS + 1][AID_PARAM_SIZE];
	unsigned int			connected;

	unsigned char			tset_table[TSET_PARAM_SIZE];
	int				temperature;
	unsigned int			coordinate[2];
	unsigned char			date[4];
	unsigned char			dump_info[3];

	int						lux;
	struct class			*mdnie_class;

	struct mipi_dsim_device		*dsim;

#define CONFIG_ESD_FG
#if defined(CONFIG_ESD_FG)
	int				esd_fg_irq;
	int				esd_fg_gpio;
	struct delayed_work		esd_fg;
	bool				esd_fg_state;
	unsigned int			esd_fg_count;
	spinlock_t			esd_fg_lock;
	struct pinctrl			*esd_fg_pins;
	struct pinctrl_state		*esd_fg_pins_state[2];
#endif
};

#if defined(CONFIG_ESD_FG)
static int irq_is_active(int irq, int level)
{
	struct irq_desc *desc = irq_to_desc(irq);

	if (!irq_has_action(irq))
		return 0;

	if (desc->action->flags & IRQF_TRIGGER_FALLING)
		return !level;
	else if (desc->action->flags & IRQF_TRIGGER_RISING)
		return level;
	else
		return 0;
}

static int esd_fg_pinctrl(struct lcd_info *lcd, int enable)
{
	struct device *dev = &lcd->ld->dev;
	int ret = 0;

	enable = !!enable;

	if (!IS_ERR(lcd->esd_fg_pins_state[enable])) {
		ret = pinctrl_select_state(lcd->esd_fg_pins, lcd->esd_fg_pins_state[enable]);
		if (ret) {
			dev_err(dev, "%s: failed to select state\n", __func__);
			return ret;
		}
	}

	return ret;
}

static int esd_fg_enable(struct lcd_info *lcd, int enable)
{
	struct device *dev = &lcd->ld->dev;
	int ret = 0;

	if (!lcd->connected)
		return ret;

	if (!gpio_is_valid(lcd->esd_fg_gpio) || !lcd->esd_fg_irq)
		return ret;

	if (enable) {
		esd_fg_pinctrl(lcd, enable);
		enable_irq(lcd->esd_fg_irq);
	} else {
		if (!lcd->esd_fg_state)
			disable_irq(lcd->esd_fg_irq);
		esd_fg_pinctrl(lcd, enable);
	}

	dev_info(dev, "%s: %s, gpio: %d, state: %d\n", __func__, enable ? "enable" : "disable",
		gpio_get_value(lcd->esd_fg_gpio), lcd->esd_fg_state);

	return ret;
}

static int esd_fg_refresh(struct lcd_info *lcd)
{
	struct fb_info *info = registered_fb[0];
	int ret = 0;

	dev_info(&lcd->ld->dev, "+%s\n", __func__);

	spin_lock(&lcd->esd_fg_lock);
	lcd->esd_fg_state = true;
	spin_unlock(&lcd->esd_fg_lock);

	if (!lock_fb_info(info))
		return -ENODEV;
	info->flags |= FBINFO_MISC_USEREVENT;
	ret = fb_blank(info, FB_BLANK_POWERDOWN);
	ret = fb_blank(info, FB_BLANK_UNBLANK);
	info->flags &= ~FBINFO_MISC_USEREVENT;
	unlock_fb_info(info);

	spin_lock(&lcd->esd_fg_lock);
	lcd->esd_fg_state = false;
	spin_unlock(&lcd->esd_fg_lock);

	dev_info(&lcd->ld->dev, "-%s: %d\n", __func__, gpio_get_value(lcd->esd_fg_gpio));

	return ret;
}

static void esd_fg_is_detected(struct lcd_info *lcd)
{
	struct file *fp;
	struct timespec ts;
	struct rtc_time tm;
	char name[NAME_MAX];

	getnstimeofday(&ts);
	rtc_time_to_tm(ts.tv_sec, &tm);
	sprintf(name, "%s%02d-%02d_%02d:%02d:%02d_%02d",
		"/sdcard/", tm.tm_mon + 1, tm.tm_mday, tm.tm_hour, tm.tm_min, tm.tm_sec, lcd->esd_fg_count);
	fp = filp_open(name, O_CREAT, S_IRWXU | S_IRWXG | S_IRWXO);
	sys_sync();

	if (IS_ERR_OR_NULL(fp))
		dev_info(&lcd->ld->dev, "fail to create %s log file, %s\n", __func__, name);
	else
		filp_close(fp, current->files);
}

static void esd_fg_work(struct work_struct *work)
{
	struct lcd_info *lcd =
		container_of(work, struct lcd_info, esd_fg.work);

	dev_info(&lcd->ld->dev, "%s: level: %d, state: %d, count: %d\n",
		__func__, gpio_get_value(lcd->esd_fg_gpio), lcd->esd_fg_state, lcd->esd_fg_count);

	return;

	esd_fg_is_detected(lcd);

	esd_fg_refresh(lcd);
}

static irqreturn_t esd_fg_isr(int irq, void *dev_id)
{
	struct lcd_info *lcd = dev_id;
	unsigned int esd_fg_level = gpio_get_value(lcd->esd_fg_gpio);

	dev_info(&lcd->ld->dev, "%s: esd_fg(%d) is detected\n", __func__, esd_fg_level);

	lcd->esd_fg_count++;

	if (irq_is_active(irq, esd_fg_level) && (!lcd->esd_fg_state)) {
		disable_irq_nosync(lcd->esd_fg_irq);
		schedule_delayed_work(&lcd->esd_fg, 0);
	}

	return IRQ_HANDLED;
}

static int esd_fg_init(struct lcd_info *lcd)
{
	struct device *dev;
	struct device_node *np;
	struct platform_device *pdev;
	enum of_gpio_flags flags;
	unsigned int irqf_type = IRQF_TRIGGER_RISING;
	unsigned int active_level = 1;
	int ret = 0;
	lcd->esd_fg_gpio = -EINVAL;

	if (!lcd->connected)
		return ret;

	np = of_parse_phandle(lcd->dsim->dev->of_node, "lcd_info", 0);
	pdev = of_platform_device_create(np, NULL, lcd->dsim->dev);
	dev = &pdev->dev;

	lcd->esd_fg_gpio = of_get_named_gpio_flags(np, "oled-errfg-gpio", 0, &flags);
	if (lcd->esd_fg_gpio < 0) {
		dev_err(&lcd->ld->dev, "failed to get proper gpio number\n");
		return -EINVAL;
	}

	if (flags & OF_GPIO_ACTIVE_LOW) {
		irqf_type = IRQF_TRIGGER_FALLING;
		active_level = 0;
	}

	if (gpio_get_value(lcd->esd_fg_gpio) == active_level) {
		dev_err(&lcd->ld->dev, "%s: gpio(%d) is already %s\n",
			__func__, lcd->esd_fg_gpio, active_level ? "high" : "low");
		return ret;
	}

	lcd->esd_fg_pins = devm_pinctrl_get(dev);
	if (IS_ERR(lcd->esd_fg_pins)) {
		dev_err(&lcd->ld->dev, "failed to get pinctrl\n");
		return -EINVAL;
	}

	lcd->esd_fg_pins_state[0] = pinctrl_lookup_state(lcd->esd_fg_pins, "errfg_off");
	if (IS_ERR(lcd->esd_fg_pins_state[0])) {
		dev_err(&lcd->ld->dev, "failed to get proper errfg off pinctrl\n");
		return -EINVAL;
	}

	lcd->esd_fg_pins_state[1] = pinctrl_lookup_state(lcd->esd_fg_pins, "errfg_on");
	if (IS_ERR(lcd->esd_fg_pins_state[1])) {
		dev_err(&lcd->ld->dev, "failed to get proper errfg on pinctrl\n");
		return -EINVAL;
	}

	lcd->esd_fg_irq = gpio_to_irq(lcd->esd_fg_gpio);
	if (!lcd->esd_fg_irq) {
		dev_err(&lcd->ld->dev, "failed to get proper irq number\n");
		return -EINVAL;
	}

	spin_lock_init(&lcd->esd_fg_lock);

	INIT_DELAYED_WORK(&lcd->esd_fg, esd_fg_work);

	irq_set_status_flags(lcd->esd_fg_irq, IRQ_NOAUTOEN);

	ret = request_irq(lcd->esd_fg_irq, esd_fg_isr, irqf_type, "esd_fg", lcd);
	if (ret) {
		dev_err(&lcd->ld->dev, "esd_fg irq request failed\n");
		return ret;
	}

	esd_fg_enable(lcd, 1);

	dev_info(&lcd->ld->dev, "%s: gpio(%d) is active %s(%d), %s edge\n",
		__func__, lcd->esd_fg_gpio, active_level ? "high" : "low",
		gpio_get_value(lcd->esd_fg_gpio),
		(irqf_type == IRQF_TRIGGER_RISING) ? "rising" : "falling");

	return ret;
}
#endif

int s6e3ha2_write(struct lcd_info *lcd, const u8 *seq, u32 len)
{
	int ret = 0;
	int retry;

	if (!lcd->connected)
		return -EINVAL;

	mutex_lock(&lcd->lock);

	retry = 3;
write_data:
	if (!retry) {
		dev_err(&lcd->ld->dev, "%s failed: exceed retry count\n", __func__);
		goto write_err;
	}

	if (len > 2)
		ret = s5p_mipi_dsi_wr_data(lcd->dsim,
					MIPI_DSI_DCS_LONG_WRITE, (u32)seq, len);
	else if (len == 2)
		ret = s5p_mipi_dsi_wr_data(lcd->dsim,
					MIPI_DSI_DCS_SHORT_WRITE_PARAM, seq[0], seq[1]);
	else if (len == 1)
		ret = s5p_mipi_dsi_wr_data(lcd->dsim,
					MIPI_DSI_DCS_SHORT_WRITE, seq[0], 0);
	else {
		ret = -EINVAL;
		goto write_err;
	}

	if (ret != 0) {
		dev_dbg(&lcd->ld->dev, "mipi_write failed retry ..\n");
		retry--;
		goto write_data;
	}

write_err:
	mutex_unlock(&lcd->lock);
	return ret;
}

int s6e3ha2_read(struct lcd_info *lcd, u8 addr, u8 *buf, u32 len)
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
	ret = s5p_mipi_dsi_rd_data(lcd->dsim, cmd, addr, len, buf, 1);
	if (ret != len) {
		dev_err(&lcd->ld->dev, "mipi_read failed retry ret = %d ..\n", ret);
		retry--;
		goto read_data;
	}

read_err:
	mutex_unlock(&lcd->lock);
	return ret;
}

static int s6e3ha2_read_offset(struct lcd_info *lcd, u8 addr, u8 *buf, u32 len, u32 offset)
{
	int ret;
	unsigned char wbuf[] = {0xB0, 0};

	wbuf[1] = offset;

	s6e3ha2_write(lcd, wbuf, ARRAY_SIZE(wbuf));

	ret = s6e3ha2_read(lcd, addr, buf, len);

	if (ret < 1)
		dev_err(&lcd->ld->dev, "%s failed\n", __func__);

	return ret;
}

#if defined(CONFIG_DECON_MDNIE_LITE)
static int s6e3ha2_write_set(struct lcd_info *lcd, struct lcd_seq_info *seq, u32 num)
{
	int ret = 0, i;

	mutex_lock(&lcd->bl_lock);

	for (i = 0; i < num; i++) {
		if (seq[i].cmd) {
			ret = s6e3ha2_write(lcd, seq[i].cmd, seq[i].len);
			if (ret != 0) {
				dev_info(&lcd->ld->dev, "%s failed.\n", __func__);
				goto exit;
			}
		}
		if (seq[i].sleep)
			msleep(seq[i].sleep);
	}

exit:
	mutex_unlock(&lcd->bl_lock);

	return ret;
}
#endif

static int s6e3ha2_read_coordinate(struct lcd_info *lcd)
{
	int ret;
	unsigned char buf[LDI_COORDINATE_LEN] = {0,};

	ret = s6e3ha2_read(lcd, LDI_COORDINATE_REG, buf, LDI_COORDINATE_LEN);

	if (ret < 1)
		dev_err(&lcd->ld->dev, "%s failed\n", __func__);

	lcd->coordinate[0] = buf[0] << 8 | buf[1];	/* X */
	lcd->coordinate[1] = buf[2] << 8 | buf[3];	/* Y */

	return ret;
}

static int s6e3ha2_read_id(struct lcd_info *lcd, u8 *buf)
{
	int ret;

	ret = s6e3ha2_read(lcd, LDI_ID_REG, buf, LDI_ID_LEN);

	if (ret < 1) {
		lcd->connected = 0;
		dev_info(&lcd->ld->dev, "panel is not connected well\n");
	}

	return ret;
}

static int s6e3ha2_read_code(struct lcd_info *lcd, u8 *buf)
{
	int ret;

	ret = s6e3ha2_read(lcd, LDI_CODE_REG, buf, LDI_CODE_LEN);

	if (ret < 1)
		dev_info(&lcd->ld->dev, "%s failed\n", __func__);

	return ret;
}

static int s6e3ha2_read_mtp(struct lcd_info *lcd, u8 *buf)
{
	int ret, i;

	ret = s6e3ha2_read(lcd, LDI_MTP_REG, buf, LDI_MTP_LEN);

	if (ret < 1)
		dev_err(&lcd->ld->dev, "%s failed\n", __func__);

	smtd_dbg("%s: %02xh\n", __func__, LDI_MTP_REG);
	for (i = 0; i < LDI_MTP_LEN; i++)
		smtd_dbg("%02dth value is %02x\n", i+1, (int)buf[i]);

	/* manufacture date */
	lcd->date[0] = buf[40];		/* 41th */
	lcd->date[1] = buf[41];		/* 42th */
	lcd->date[2] = buf[42];		/* 43th */
	lcd->date[3] = buf[43];		/* 44th */

	return ret;
}

static int s6e3ha2_read_elvss(struct lcd_info *lcd, u8 *buf)
{
	int ret, i;

	ret = s6e3ha2_read(lcd, LDI_ELVSS_REG, buf, LDI_ELVSS_LEN);

	smtd_dbg("%s: %02xh\n", __func__, LDI_ELVSS_REG);
	for (i = 0; i < LDI_ELVSS_LEN; i++)
		smtd_dbg("%02dth value is %02x\n", i+1, (int)buf[i]);

	return ret;
}

static int s6e3ha2_read_tset(struct lcd_info *lcd)
{
	int ret, i;

	ret = s6e3ha2_read(lcd, LDI_TSET_REG, &lcd->tset_table[1], LDI_TSET_LEN);

	smtd_dbg("%s: %02xh\n", __func__, LDI_TSET_REG);
	for (i = 0; i < LDI_TSET_LEN; i++)
		smtd_dbg("%02dth value is %02x\n", i, lcd->tset_table[i]);

	lcd->tset_table[0] = LDI_TSET_REG;

	return ret;
}

static int get_backlight_level_from_brightness(int brightness)
{
	int backlightlevel = DEFAULT_GAMMA_INDEX;

	backlightlevel = brightness_table[brightness];

	return backlightlevel;
}

static int s6e3ha2_gamma_ctl(struct lcd_info *lcd, u8 force)
{
	int ret = 0;

	if (force)
		goto gamma_update;
	else if (lcd->current_bl != lcd->bl)
		goto gamma_update;
	else
		goto exit;

gamma_update:
	ret = s6e3ha2_write(lcd, lcd->gamma_table[lcd->bl], GAMMA_PARAM_SIZE);
	if (!ret)
		ret = -EPERM;

exit:
	return ret;
}

static int s6e3ha2_aid_parameter_ctl(struct lcd_info *lcd, u8 force)
{
	int ret = 0;

	if (force)
		goto aid_update;
	else if (lcd->inter_aor_info[lcd->current_brightness][1] != lcd->inter_aor_info[lcd->brightness][1])
		goto aid_update;
	else if (lcd->inter_aor_info[lcd->current_brightness][2] != lcd->inter_aor_info[lcd->brightness][2])
		goto aid_update;
	else
		goto exit;

aid_update:
	ret = s6e3ha2_write(lcd, lcd->inter_aor_info[lcd->brightness], AID_PARAM_SIZE);
	if (!ret)
		ret = -EPERM;

exit:
	return ret;
}

static int s6e3ha2_set_acl(struct lcd_info *lcd, u8 force)
{
	int ret = 0, level = ACL_STATUS_15P;

	if (lcd->siop_enable)
		goto acl_update;

	if (!lcd->acl_enable && LEVEL_IS_ACL_OFF(lcd->brightness))
		level = ACL_STATUS_0P;

acl_update:
	if (force || lcd->current_acl != ACL_CUTOFF_TABLE[level][1]) {
		ret = s6e3ha2_write(lcd, ACL_CUTOFF_TABLE[level], ACL_PARAM_SIZE);
		ret += s6e3ha2_write(lcd, ACL_OPR_TABLE[level], OPR_PARAM_SIZE);
		lcd->current_acl = ACL_CUTOFF_TABLE[level][1];
		dev_info(&lcd->ld->dev, "acl: %d, brightness: %d\n", lcd->current_acl, lcd->brightness);
	}

	if (!ret)
		ret = -EPERM;

	return ret;
}

/* CAPS enable flag should be decided before elvss setting */
static void s6e3ha2_set_caps(struct lcd_info *lcd)
{
	int level = CAPS_OFF;

	if (LEVEL_IS_CAPS_OFF(lcd->bl))
		goto exit;

	level = CAPS_ON;
exit:
	lcd->caps_enable = level;

	return;
}

static int s6e3ha2_set_elvss(struct lcd_info *lcd, u8 force)
{
	int ret = 0, i, elvss_level;
	u32 nit, temperature;
	union elvss_info elvss;

	nit = index_brightness_table[lcd->bl];
	elvss_level = ELVSS_STATUS_360;
	for (i = 0; i < ELVSS_STATUS_MAX; i++) {
		if (nit <= ELVSS_DIM_TABLE[i]) {
			elvss_level = i;
			break;
		}
	}

	temperature = (lcd->temperature <= -20) ? TEMP_BELOW_MINUS_20_DEGREE : TEMP_ABOVE_MINUS_20_DEGREE;
	elvss.mps = lcd->elvss_table[lcd->caps_enable][temperature][elvss_level][1];
	elvss.offset = lcd->elvss_table[lcd->caps_enable][temperature][elvss_level][2];
	elvss.elvss_base = lcd->elvss_table[lcd->caps_enable][temperature][elvss_level][22];

	if (force)
		goto elvss_update;
	else if (lcd->current_elvss.value != elvss.value)
		goto elvss_update;
	else
		goto exit;

elvss_update:
	ret = s6e3ha2_write(lcd, lcd->elvss_table[lcd->caps_enable][temperature][elvss_level], ELVSS_PARAM_SIZE);
	dev_info(&lcd->ld->dev, "caps: %d, temperature: %d, elvss: %d, %x\n",
		lcd->caps_enable, temperature, elvss_level, elvss.value);
	lcd->current_elvss.value = elvss.value;
	if (!ret)
		ret = -EPERM;

exit:
	return ret;
}

static int s6e3ha2_set_tset(struct lcd_info *lcd, u8 force)
{
	int ret = 0;
	u8 tset;

	tset = ((lcd->temperature >= 0) ? 0 : BIT(7)) | abs(lcd->temperature);

	if (force || lcd->tset_table[LDI_TSET_LEN] != tset) {
		lcd->tset_table[LDI_TSET_LEN] = tset;
		ret = s6e3ha2_write(lcd, lcd->tset_table, TSET_PARAM_SIZE);
		dev_info(&lcd->ld->dev, "temperature: %d, tset: %d\n", lcd->temperature, tset);
	}

	if (!ret)
		ret = -EPERM;

	return ret;
}

static int s6e3ha2_set_hbm(struct lcd_info *lcd, u8 force)
{
	int ret = 0, level = LEVEL_IS_HBM(lcd->brightness);

	if (force || lcd->current_hbm != HBM_TABLE[level][1]) {
		ret = s6e3ha2_write(lcd, HBM_TABLE[level], HBM_PARAM_SIZE);
		lcd->current_hbm = HBM_TABLE[level][1];
		dev_info(&lcd->ld->dev, "hbm: %d, brightness: %d\n", lcd->current_hbm, lcd->brightness);
	}

	if (!ret)
		ret = -EPERM;

	return ret;
}

static int s6e3ha2_set_vint(struct lcd_info *lcd, u8 force)
{
	int ret = 0, i, level;
	u32 nit;

	nit = index_brightness_table[lcd->bl];
	level = VINT_STATUS_014;
	for (i = 0; i < VINT_STATUS_MAX; i++) {
		if (nit <= VINT_DIM_TABLE[i]) {
			level = i;
			break;
		}
	}

	if (force || lcd->current_vint != VINT_TABLE[level]) {
		ret = s6e3ha2_write(lcd, lcd->vint_table[level], VINT_PARAM_SIZE);
		lcd->current_vint = VINT_TABLE[level];
		dev_info(&lcd->ld->dev, "vint: %x\n", lcd->current_vint);
	}

	if (!ret)
		ret = -EPERM;

	return ret;
}

static void init_dynamic_aid(struct lcd_info *lcd)
{
	lcd->daid.vreg = VREG_OUT_X1000;
	lcd->daid.iv_tbl = index_voltage_table;
	lcd->daid.iv_max = IV_MAX;
	lcd->daid.mtp = kzalloc(IV_MAX * CI_MAX * sizeof(int), GFP_KERNEL);
	lcd->daid.gamma_default = gamma_default;
	lcd->daid.formular = gamma_formula;
	lcd->daid.vt_voltage_value = vt_voltage_value;

	lcd->daid.ibr_tbl = index_brightness_table;
	lcd->daid.ibr_max = IBRIGHTNESS_MAX;
	lcd->daid.gc_tbls = gamma_curve_tables;
	lcd->daid.gc_lut = gamma_curve_lut;

	lcd->daid.br_base = brightness_base_table;
	lcd->daid.offset_gra = offset_gradation;
	lcd->daid.offset_color = (const struct rgb_t(*)[])offset_color;
}

static void init_mtp_data(struct lcd_info *lcd, u8 *mtp_data)
{
	int i, c, j;

	int *mtp;

	mtp = lcd->daid.mtp;

	mtp_data[32] = mtp_data[34];			/* VT B */
	mtp_data[31] = GET_UPPER_4BIT(mtp_data[33]);	/* VT G */
	mtp_data[30] = GET_LOWER_4BIT(mtp_data[33]);	/* VT R */

	for (c = 0, j = 0; c < CI_MAX; c++, j++) {
		if (mtp_data[j++] & 0x01)
			mtp[(IV_MAX-1)*CI_MAX+c] = mtp_data[j] * (-1);
		else
			mtp[(IV_MAX-1)*CI_MAX+c] = mtp_data[j];
	}

	for (i = IV_MAX - 2; i >= 0; i--) {
		for (c = 0; c < CI_MAX; c++, j++) {
			if (mtp_data[j] & 0x80)
				mtp[CI_MAX*i+c] = (mtp_data[j] & 0x7F) * (-1);
			else
				mtp[CI_MAX*i+c] = mtp_data[j];
		}
	}

	for (i = 0, j = 0; i <= IV_MAX; i++)
		for (c = 0; c < CI_MAX; c++, j++)
			smtd_dbg("mtp_data[%02d] = %d\n", j, mtp_data[j]);

	for (i = 0, j = 0; i < IV_MAX; i++)
		for (c = 0; c < CI_MAX; c++, j++)
			smtd_dbg("mtp[%02d] = %d\n", j, mtp[j]);

	smtd_dbg("MTP_Offset_Value\n");
	for (i = 0; i < IV_MAX; i++) {
		for (c = 0; c < CI_MAX; c++)
			smtd_dbg("%4d ", lcd->daid.mtp[i*CI_MAX+c]);
		smtd_dbg("\n");
	}
}

static int init_gamma_table(struct lcd_info *lcd, u8 *mtp_data)
{
	int i, c, j, v;
	int ret = 0;
	int *pgamma;
	int **gamma;

	/* allocate memory for local gamma table */
	gamma = kzalloc(IBRIGHTNESS_MAX * sizeof(int *), GFP_KERNEL);
	if (!gamma) {
		pr_err("failed to allocate gamma table\n");
		ret = -ENOMEM;
		goto err_alloc_gamma_table;
	}

	for (i = 0; i < IBRIGHTNESS_MAX; i++) {
		gamma[i] = kzalloc(IV_MAX*CI_MAX * sizeof(int), GFP_KERNEL);
		if (!gamma[i]) {
			pr_err("failed to allocate gamma\n");
			ret = -ENOMEM;
			goto err_alloc_gamma;
		}
	}

	/* allocate memory for gamma table */
	lcd->gamma_table = kzalloc(IBRIGHTNESS_MAX * sizeof(u8 *), GFP_KERNEL);
	if (!lcd->gamma_table) {
		pr_err("failed to allocate gamma table 2\n");
		ret = -ENOMEM;
		goto err_alloc_gamma_table2;
	}

	for (i = 0; i < IBRIGHTNESS_MAX; i++) {
		lcd->gamma_table[i] = kzalloc(GAMMA_PARAM_SIZE * sizeof(u8), GFP_KERNEL);
		if (!lcd->gamma_table[i]) {
			pr_err("failed to allocate gamma 2\n");
			ret = -ENOMEM;
			goto err_alloc_gamma2;
		}
		lcd->gamma_table[i][0] = 0xCA;
	}

	/* calculate gamma table */
	init_mtp_data(lcd, mtp_data);
	dynamic_aid(lcd->daid, gamma);

	for (i = 0; i < IBRIGHTNESS_MAX; i++) {
		smtd_dbg("Gamma [%03d] = ", index_brightness_table[i]);
		for (j = 0; j < IV_MAX; j++) {
			for (c = 0; c < CI_MAX; c++)
				smtd_dbg("%04d ", gamma[i][j*CI_MAX+c]);
		}
		smtd_dbg("\n");
	}

	/* relocate gamma order */
	for (i = 0; i < IBRIGHTNESS_MAX; i++) {
		/* Brightness table */
		v = IV_MAX - 1;
		pgamma = &gamma[i][v * CI_MAX];
		for (c = 0, j = 1; c < CI_MAX; c++, pgamma++) {
			if (*pgamma & 0x100)
				lcd->gamma_table[i][j++] = 1;
			else
				lcd->gamma_table[i][j++] = 0;

			lcd->gamma_table[i][j++] = *pgamma & 0xff;
		}

		for (v = IV_MAX - 2; v >= 0; v--) {
			pgamma = &gamma[i][v * CI_MAX];
			for (c = 0; c < CI_MAX; c++, pgamma++)
				lcd->gamma_table[i][j++] = *pgamma;
		}

		for (v = 0; v < GAMMA_PARAM_SIZE; v++)
			smtd_dbg("%d ", lcd->gamma_table[i][v]);
		smtd_dbg("\n");
	}

	/* free local gamma table */
	for (i = 0; i < IBRIGHTNESS_MAX; i++)
		kfree(gamma[i]);
	kfree(gamma);

	return 0;

err_alloc_gamma2:
	while (i > 0) {
		kfree(lcd->gamma_table[i-1]);
		i--;
	}
	kfree(lcd->gamma_table);
err_alloc_gamma_table2:
	i = IBRIGHTNESS_MAX;
err_alloc_gamma:
	while (i > 0) {
		kfree(gamma[i-1]);
		i--;
	}
	kfree(gamma);
err_alloc_gamma_table:
	return ret;
}

static int init_aid_dimming_table(struct lcd_info *lcd)
{
	int i;

	for (i = 0; i <= lcd->bd->props.max_brightness; i++) {
		lcd->inter_aor_info[i][0] = SEQ_AID_SET[0];
		lcd->inter_aor_info[i][1] = inter_aor_cmd[i][0];
		lcd->inter_aor_info[i][2] = inter_aor_cmd[i][1];
	}

	return 0;
}

static int init_elvss_table(struct lcd_info *lcd, u8 *elvss_data)
{
	int i, temp, caps, ret;

	for (caps = 0; caps < CAPS_MAX; caps++) {
		for (temp = 0; temp < TEMP_MAX; temp++) {
			lcd->elvss_table[caps][temp] = kzalloc(ELVSS_STATUS_MAX * sizeof(u8 *), GFP_KERNEL);

			if (IS_ERR_OR_NULL(lcd->elvss_table[caps][temp])) {
				pr_err("failed to allocate elvss table\n");
				ret = -ENOMEM;
				goto err_alloc_elvss_table;
			}

			for (i = 0; i < ELVSS_STATUS_MAX; i++) {
				lcd->elvss_table[caps][temp][i] = kzalloc(ELVSS_PARAM_SIZE * sizeof(u8), GFP_KERNEL);
				if (IS_ERR_OR_NULL(lcd->elvss_table[caps][temp][i])) {
					pr_err("failed to allocate elvss\n");
					ret = -ENOMEM;
					goto err_alloc_elvss;
				}

				/* Duplicate with reading value from DDI */
				memcpy(&lcd->elvss_table[caps][temp][i][1], elvss_data, LDI_ELVSS_LEN);

				lcd->elvss_table[caps][temp][i][0] = LDI_ELVSS_REG;
				lcd->elvss_table[caps][temp][i][1] = MPS_TABLE[caps];
				lcd->elvss_table[caps][temp][i][2] = ELVSS_TABLE[i];
				/* lcd->elvss_table[caps][temp][i][22] = temp ? (elvss_data[21] - 3) : elvss_data[21]; */
			}
		}
	}

	return 0;

err_alloc_elvss:
	/* should be kfree elvss with caps */
	while (temp >= 0) {
		while (i > 0)
			kfree(lcd->elvss_table[caps][temp][--i]);

		i = ELVSS_STATUS_MAX;
		temp--;
	}
	temp = TEMP_MAX;
err_alloc_elvss_table:
	while (temp > 0)
		kfree(lcd->elvss_table[caps][--temp]);

	return ret;
}

static int init_vint_table(struct lcd_info *lcd)
{
	int i, j, ret;

	for (i = 0; i < VINT_STATUS_MAX; i++) {
		lcd->vint_table[i] = kzalloc(VINT_PARAM_SIZE * sizeof(u8), GFP_KERNEL);
		if (!lcd->vint_table[i]) {
			pr_err("failed to allocate vint\n");
			ret = -ENOMEM;
			goto err_alloc_vint;
		}

		/* Duplicate same value with default one */
		memcpy(lcd->vint_table[i], SEQ_VINT_SET, ARRAY_SIZE(SEQ_VINT_SET));

		lcd->vint_table[i][2] = VINT_TABLE[i];
	}

	for (i = 0; i < VINT_STATUS_MAX; i++) {
		for (j = 0; j < ARRAY_SIZE(SEQ_VINT_SET); j++)
			smtd_dbg("0x%02x, ", lcd->vint_table[i][j]);
		smtd_dbg("\n");
	}

	return 0;

err_alloc_vint:
	while (i > 0)
		kfree(lcd->vint_table[--i]);

	return ret;
}

static void show_lcd_table(struct lcd_info *lcd)
{
	int i, j, caps, temp;

	for (i = 0; i < IBRIGHTNESS_MAX; i++) {
		smtd_dbg("%03d: ", index_brightness_table[i]);
		for (j = 0; j < GAMMA_PARAM_SIZE; j++)
			smtd_dbg("%02X ", lcd->gamma_table[i][j]);
		smtd_dbg("\n");
	}
	smtd_dbg("\n");

	for (i = 0; i < IBRIGHTNESS_MAX; i++) {
		smtd_dbg("%03d: ", index_brightness_table[i]);
		for (j = 0; j < GAMMA_PARAM_SIZE; j++)
			smtd_dbg("%03d ", lcd->gamma_table[i][j]);
		smtd_dbg("\n");
	}
	smtd_dbg("\n");

	for (i = 0; i < IBRIGHTNESS_MAX; i++) {
		smtd_dbg("%03d: ", index_brightness_table[i]);
		for (j = 0; j < AID_PARAM_SIZE; j++)
			smtd_dbg("%02X ", lcd->aor[i][j]);
		smtd_dbg("\n");
	}
	smtd_dbg("\n");

	for (caps = 0; caps < CAPS_MAX; caps++) {
		for (temp = 0; temp < TEMP_MAX; temp++) {
			smtd_dbg("caps: %d, temp: %d\n", caps, temp);
			for (i = 0; i < ELVSS_STATUS_MAX; i++) {
				smtd_dbg("%03d: ", ELVSS_DIM_TABLE[i]);
				for (j = 0; j < ELVSS_PARAM_SIZE; j++)
					smtd_dbg("%02X ", lcd->elvss_table[caps][temp][i][j]);
				smtd_dbg("\n");
			}
			smtd_dbg("\n");
		}
		smtd_dbg("\n");
	}
}

static int update_brightness(struct lcd_info *lcd, u8 force)
{
	mutex_lock(&lcd->bl_lock);

	lcd->brightness = lcd->bd->props.brightness;

	lcd->bl = get_backlight_level_from_brightness(lcd->brightness);

	if (LEVEL_IS_HBM(lcd->brightness))
		lcd->bl = IBRIGHTNESS_500NT;

	if (force || lcd->ldi_enable) {
		s6e3ha2_write(lcd, SEQ_TEST_KEY_ON_F0, ARRAY_SIZE(SEQ_TEST_KEY_ON_F0));
		s6e3ha2_gamma_ctl(lcd, force);
		s6e3ha2_aid_parameter_ctl(lcd, force);
		s6e3ha2_set_caps(lcd);
		s6e3ha2_set_elvss(lcd, force);
		s6e3ha2_set_vint(lcd, force);
		s6e3ha2_write(lcd, SEQ_GAMMA_UPDATE, ARRAY_SIZE(SEQ_GAMMA_UPDATE));
		s6e3ha2_write(lcd, SEQ_GAMMA_UPDATE_L, ARRAY_SIZE(SEQ_GAMMA_UPDATE_L));
		s6e3ha2_set_acl(lcd, force);
		s6e3ha2_set_tset(lcd, force);
		s6e3ha2_set_hbm(lcd, force);
		s6e3ha2_write(lcd, SEQ_TEST_KEY_OFF_F0, ARRAY_SIZE(SEQ_TEST_KEY_OFF_F0));

		lcd->current_bl = lcd->bl;
		lcd->current_brightness = lcd->brightness;

		dev_info(&lcd->ld->dev, "brightness=%d, bl=%d, candela=%d\n",
			lcd->brightness, lcd->bl, index_brightness_table[lcd->bl]);
	}

	mutex_unlock(&lcd->bl_lock);

	return 0;
}

static int s6e3ha2_ldi_init(struct lcd_info *lcd)
{
	int ret = 0;

	lcd->connected = 1;

	/* 7. Sleep Out(11h) */
	s6e3ha2_write(lcd, SEQ_SLEEP_OUT, ARRAY_SIZE(SEQ_SLEEP_OUT));

	/* 8. Wait 5ms */
	usleep_range(5000, 6000);

	s6e3ha2_write(lcd, SEQ_TEST_KEY_ON_F0, ARRAY_SIZE(SEQ_TEST_KEY_ON_F0));
	s6e3ha2_write(lcd, SEQ_TEST_KEY_ON_FC, ARRAY_SIZE(SEQ_TEST_KEY_ON_FC));

	/* 9. Interface Setting */
	s6e3ha2_write(lcd, SEQ_SINGLE_DSI_1, ARRAY_SIZE(SEQ_SINGLE_DSI_1));
	s6e3ha2_write(lcd, SEQ_SINGLE_DSI_2, ARRAY_SIZE(SEQ_SINGLE_DSI_2));

	/* 10. Wait 120ms */
	msleep(120);

	/* 11. Module Information READ */
	s6e3ha2_read_id(lcd, lcd->id);

	/* 12. Common Setting */
	/* 4.2.1 TE(Vsync) ON/OFF */
	/* 12. Common Setting */
	/* 4.2.1 TE(Vsync) ON/OFF */
	s6e3ha2_write(lcd, SEQ_TE_ON, ARRAY_SIZE(SEQ_TE_ON));
	/* 4.2.2 TSP TE(Hsync) Control */
	s6e3ha2_write(lcd, SEQ_TSP_HSYNC_ON, ARRAY_SIZE(SEQ_TSP_HSYNC_ON));
	/* 4.2.3 PenTile Setting */
	s6e3ha2_write(lcd, SEQ_PENTILE_SETTING, ARRAY_SIZE(SEQ_PENTILE_SETTING));
	/* 4.2.4 POC Setting */
	s6e3ha2_write(lcd, SEQ_POC_SETTING_1, ARRAY_SIZE(SEQ_POC_SETTING_1));
	s6e3ha2_write(lcd, SEQ_POC_SETTING_2, ARRAY_SIZE(SEQ_POC_SETTING_2));
	s6e3ha2_write(lcd, SEQ_PCD_SET_DET_LOW, ARRAY_SIZE(SEQ_PCD_SET_DET_LOW));
	/* 4.2.5 ERR_FG Setting */
	s6e3ha2_write(lcd, SEQ_ERR_FG_SETTING, ARRAY_SIZE(SEQ_ERR_FG_SETTING));
	s6e3ha2_write(lcd, SEQ_TE_START_SETTING, ARRAY_SIZE(SEQ_TE_START_SETTING));
	/* 4.2.6 Memory Address Set */
	s6e3ha2_write(lcd, SEQ_COLUMN_ADDRESS, ARRAY_SIZE(SEQ_COLUMN_ADDRESS));
	s6e3ha2_write(lcd, SEQ_PAGE_ADDRESS, ARRAY_SIZE(SEQ_PAGE_ADDRESS));

	/* 13. Brightness Setting */
	update_brightness(lcd, 1);

	s6e3ha2_write(lcd, SEQ_TEST_KEY_OFF_FC, ARRAY_SIZE(SEQ_TEST_KEY_OFF_FC));
	s6e3ha2_write(lcd, SEQ_TEST_KEY_OFF_F0, ARRAY_SIZE(SEQ_TEST_KEY_OFF_F0));

	return ret;
}

static int s6e3ha2_ldi_enable(struct lcd_info *lcd)
{
	/* 16. Display On(29h) */
	s6e3ha2_write(lcd, SEQ_DISPLAY_ON, ARRAY_SIZE(SEQ_DISPLAY_ON));

	dev_info(&lcd->ld->dev, "DISPLAY_ON\n");

	return 0;
}

static int s6e3ha2_ldi_disable(struct lcd_info *lcd)
{
	int ret = 0;

	dev_info(&lcd->ld->dev, "+ %s\n", __func__);

	/* 2. Display Off (28h) */
	s6e3ha2_write(lcd, SEQ_DISPLAY_OFF, ARRAY_SIZE(SEQ_DISPLAY_OFF));

	dev_info(&lcd->ld->dev, "DISPLAY_OFF\n");

	/* 3. Sleep In (10h) */
	s6e3ha2_write(lcd, SEQ_SLEEP_IN, ARRAY_SIZE(SEQ_SLEEP_IN));

	/* 4. Wait 120ms */
	msleep(120);

	dev_info(&lcd->ld->dev, "- %s\n", __func__);

	return ret;
}

static int s6e3ha2_power_on(struct lcd_info *lcd)
{
	int ret = 0;

	dev_info(&lcd->ld->dev, "+ %s\n", __func__);

	ret = s6e3ha2_ldi_init(lcd);
	if (ret) {
		dev_err(&lcd->ld->dev, "failed to initialize ldi.\n");
		goto err;
	}

	ret = s6e3ha2_ldi_enable(lcd);
	if (ret) {
		dev_err(&lcd->ld->dev, "failed to enable ldi.\n");
		goto err;
	}

	mutex_lock(&lcd->bl_lock);
	lcd->ldi_enable = 1;
	mutex_unlock(&lcd->bl_lock);

	update_brightness(lcd, 1);

#if defined(CONFIG_ESD_FG)
	esd_fg_enable(lcd, 1);
#endif

	dev_info(&lcd->ld->dev, "- %s\n", __func__);
err:
	return ret;
}

static int s6e3ha2_power_off(struct lcd_info *lcd)
{
	int ret = 0;

	dev_info(&lcd->ld->dev, "+ %s\n", __func__);

#if defined(CONFIG_ESD_FG)
	esd_fg_enable(lcd, 0);
#endif

	mutex_lock(&lcd->bl_lock);
	lcd->ldi_enable = 0;
	mutex_unlock(&lcd->bl_lock);

	ret = s6e3ha2_ldi_disable(lcd);

	dev_info(&lcd->ld->dev, "- %s\n", __func__);

	return ret;
}

static int s6e3ha2_power(struct lcd_info *lcd, int power)
{
	int ret = 0;

	if (POWER_IS_ON(power) && !POWER_IS_ON(lcd->power))
		ret = s6e3ha2_power_on(lcd);
	else if (!POWER_IS_ON(power) && POWER_IS_ON(lcd->power))
		ret = s6e3ha2_power_off(lcd);

	if (!ret)
		lcd->power = power;

	return ret;
}

static int s6e3ha2_set_power(struct lcd_device *ld, int power)
{
	struct lcd_info *lcd = lcd_get_data(ld);

	if (power != FB_BLANK_UNBLANK && power != FB_BLANK_POWERDOWN &&
		power != FB_BLANK_NORMAL) {
		dev_err(&lcd->ld->dev, "power value should be 0, 1 or 4.\n");
		return -EINVAL;
	}

	return s6e3ha2_power(lcd, power);
}

static int s6e3ha2_get_power(struct lcd_device *ld)
{
	struct lcd_info *lcd = lcd_get_data(ld);

	return lcd->power;
}

static int s6e3ha2_check_fb(struct lcd_device *ld, struct fb_info *fb)
{
	return 0;
}

static int s6e3ha2_get_brightness(struct backlight_device *bd)
{
	struct lcd_info *lcd = bl_get_data(bd);

	return index_brightness_table[lcd->bl];
}

static int s6e3ha2_set_brightness(struct backlight_device *bd)
{
	int ret = 0;
	int brightness = bd->props.brightness;
	struct lcd_info *lcd = bl_get_data(bd);

	if (brightness < MIN_BRIGHTNESS ||
		brightness > bd->props.max_brightness) {
		dev_err(&bd->dev, "lcd brightness should be %d to %d. now %d\n",
			MIN_BRIGHTNESS, lcd->bd->props.max_brightness, brightness);
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

static int check_fb_brightness(struct backlight_device *bd, struct fb_info *fb)
{
	return 0;
}

static struct lcd_ops s6e3ha2_lcd_ops = {
	.set_power = s6e3ha2_set_power,
	.get_power = s6e3ha2_get_power,
	.check_fb  = s6e3ha2_check_fb,
};

static const struct backlight_ops s6e3ha2_backlight_ops = {
	.get_brightness = s6e3ha2_get_brightness,
	.update_status = s6e3ha2_set_brightness,
	.check_fb = check_fb_brightness,
};

static ssize_t power_reduce_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct lcd_info *lcd = dev_get_drvdata(dev);

	sprintf(buf, "%u\n", lcd->acl_enable);

	return strlen(buf);
}

static ssize_t power_reduce_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t size)
{
	struct lcd_info *lcd = dev_get_drvdata(dev);
	int value;
	int rc;

	rc = kstrtoul(buf, (unsigned int)0, (unsigned long *)&value);
	if (rc < 0)
		return rc;
	else {
		if (lcd->acl_enable != value) {
			dev_info(dev, "%s: %d, %d\n", __func__, lcd->acl_enable, value);
			mutex_lock(&lcd->bl_lock);
			lcd->acl_enable = value;
			mutex_unlock(&lcd->bl_lock);
		}
	}
	return size;
}

static ssize_t lcd_type_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct lcd_info *lcd = dev_get_drvdata(dev);

	sprintf(buf, "SDC_%02X%02X%02X\n", lcd->id[0], lcd->id[1], lcd->id[2]);

	return strlen(buf);
}

static ssize_t window_type_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct lcd_info *lcd = dev_get_drvdata(dev);

	sprintf(buf, "%x %x %x\n", lcd->id[0], lcd->id[1], lcd->id[2]);

	return strlen(buf);
}

static ssize_t brightness_table_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	int i, bl;
	char *pos = buf;

	for (i = 0; i <= EXTEND_BRIGHTNESS; i++) {
		bl = get_backlight_level_from_brightness(i);
		bl = LEVEL_IS_HBM(i) ? IBRIGHTNESS_500NT : bl;
		pos += sprintf(pos, "%3d %3d\n", i, index_brightness_table[bl]);
	}

	return pos - buf;
}

static ssize_t siop_enable_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct lcd_info *lcd = dev_get_drvdata(dev);

	sprintf(buf, "%u\n", lcd->siop_enable);

	return strlen(buf);
}

static ssize_t siop_enable_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t size)
{
	struct lcd_info *lcd = dev_get_drvdata(dev);
	int value;
	int rc;

	rc = kstrtoul(buf, (unsigned int)0, (unsigned long *)&value);
	if (rc < 0)
		return rc;
	else {
		if (lcd->siop_enable != value) {
			dev_info(dev, "%s: %d, %d\n", __func__, lcd->siop_enable, value);
			mutex_lock(&lcd->bl_lock);
			lcd->siop_enable = value;
			mutex_unlock(&lcd->bl_lock);
			if (lcd->ldi_enable)
				update_brightness(lcd, 1);
		}
	}
	return size;
}

static ssize_t temperature_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	char temp[] = "-20, -19, 0, 1\n";

	strcat(buf, temp);
	return strlen(buf);
}

static ssize_t temperature_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t size)
{
	struct lcd_info *lcd = dev_get_drvdata(dev);
	int value, rc;

	rc = kstrtoint(buf, 10, &value);

	if (rc < 0)
		return rc;
	else {
		mutex_lock(&lcd->bl_lock);
		lcd->temperature = value;
		mutex_unlock(&lcd->bl_lock);

		if (lcd->ldi_enable)
			update_brightness(lcd, 1);

		dev_info(dev, "%s: %d, %d\n", __func__, value, lcd->temperature);
	}

	return size;
}

static ssize_t color_coordinate_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct lcd_info *lcd = dev_get_drvdata(dev);

	sprintf(buf, "%u, %u\n", lcd->coordinate[0], lcd->coordinate[1]);

	return strlen(buf);
}

static ssize_t manufacture_date_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct lcd_info *lcd = dev_get_drvdata(dev);
	u16 year;
	u8 month, day, hour, min;

	year = ((lcd->date[0] & 0xF0) >> 4) + 2011;
	month = lcd->date[0] & 0xF;
	day = lcd->date[1] & 0x1F;
	hour = lcd->date[2] & 0x1F;
	min = lcd->date[3] & 0x3F;

	sprintf(buf, "%d, %d, %d, %d:%d\n", year, month, day, hour, min);
	return strlen(buf);
}

static ssize_t manufacture_code_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct lcd_info *lcd = dev_get_drvdata(dev);

	sprintf(buf, "%02X%02X%02X%02X%02X\n",
		lcd->code[0], lcd->code[1], lcd->code[2], lcd->code[3], lcd->code[4]);

	return strlen(buf);
}

static ssize_t dump_register_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct lcd_info *lcd = dev_get_drvdata(dev);
	char *pos = buf;
	u8 reg, len, offset;
	int ret, i;
	u8 *dump = NULL;

	reg = lcd->dump_info[0];
	len = lcd->dump_info[1];
	offset = lcd->dump_info[2];

	if (!reg || !len || reg > 0xff || len > 255 || offset > 255)
		goto exit;

	dump = kzalloc(len * sizeof(u8), GFP_KERNEL);

	if (lcd->ldi_enable) {
		s6e3ha2_write(lcd, SEQ_TEST_KEY_ON_F0, ARRAY_SIZE(SEQ_TEST_KEY_ON_F0));
		s6e3ha2_write(lcd, SEQ_TEST_KEY_ON_F1, ARRAY_SIZE(SEQ_TEST_KEY_ON_F1));
		s6e3ha2_write(lcd, SEQ_TEST_KEY_ON_FC, ARRAY_SIZE(SEQ_TEST_KEY_ON_FC));

		if (offset)
			ret = s6e3ha2_read_offset(lcd, reg, dump, len, offset);
		else
			ret = s6e3ha2_read(lcd, reg, dump, len);

		s6e3ha2_write(lcd, SEQ_TEST_KEY_OFF_FC, ARRAY_SIZE(SEQ_TEST_KEY_OFF_FC));
		s6e3ha2_write(lcd, SEQ_TEST_KEY_OFF_F1, ARRAY_SIZE(SEQ_TEST_KEY_OFF_F1));
		s6e3ha2_write(lcd, SEQ_TEST_KEY_OFF_F0, ARRAY_SIZE(SEQ_TEST_KEY_OFF_F0));
	}

	pos += sprintf(pos, "+ [%02X]\n", reg);
	for (i = 0; i < len; i++)
		pos += sprintf(pos, "%2d: %02x\n", i + offset + 1, dump[i]);
	pos += sprintf(pos, "- [%02X]\n", reg);

	kfree(dump);
exit:
	return pos - buf;
}

static ssize_t dump_register_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t size)
{
	struct lcd_info *lcd = dev_get_drvdata(dev);
	unsigned int reg, len, offset;
	int ret;

	ret = sscanf(buf, "%x %d %d", &reg, &len, &offset);

	dev_info(dev, "%s: %x %d %d\n", __func__, reg, len, offset);

	if (ret == 2)
		offset = 0;

	if (ret < 0)
		return ret;
	else {
		if (!reg || !len || reg > 0xff || len > 255 || offset > 255)
			return -EINVAL;

		lcd->dump_info[0] = reg;
		lcd->dump_info[1] = len;
		lcd->dump_info[2] = offset;
	}

	return size;
}

static ssize_t lux_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct lcd_info *lcd = dev_get_drvdata(dev);

	sprintf(buf, "%d\n", lcd->lux);

	return strlen(buf);
}

static ssize_t lux_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t size)
{
	struct lcd_info *lcd = dev_get_drvdata(dev);
	int value;
	int rc;

	rc = kstrtoint(buf, 0, &value);

	if (rc < 0)
		return rc;

	if (lcd->lux != value) {
		mutex_lock(&lcd->lock);
		lcd->lux = value;
		mutex_unlock(&lcd->lock);

		attr_store_for_each(lcd->mdnie_class, attr->attr.name, buf, size);
	}

	return size;
}

static DEVICE_ATTR(power_reduce, 0664, power_reduce_show, power_reduce_store);
static DEVICE_ATTR(lcd_type, 0444, lcd_type_show, NULL);
static DEVICE_ATTR(window_type, 0444, window_type_show, NULL);
static DEVICE_ATTR(manufacture_code, 0444, manufacture_code_show, NULL);
static DEVICE_ATTR(brightness_table, 0444, brightness_table_show, NULL);
static DEVICE_ATTR(siop_enable, 0664, siop_enable_show, siop_enable_store);
static DEVICE_ATTR(temperature, 0664, temperature_show, temperature_store);
static DEVICE_ATTR(color_coordinate, 0444, color_coordinate_show, NULL);
static DEVICE_ATTR(manufacture_date, 0444, manufacture_date_show, NULL);
static DEVICE_ATTR(dump_register, 0664, dump_register_show, dump_register_store);
static DEVICE_ATTR(lux, 0644, lux_show, lux_store);

static int s6e3ha2_probe(struct mipi_dsim_device *dsim)
{
	int ret;
	struct lcd_info *lcd;

	u8 mtp_data[LDI_MTP_LEN] = {0,};
	u8 elvss_data[LDI_ELVSS_LEN] = {0,};

	lcd = kzalloc(sizeof(struct lcd_info), GFP_KERNEL);
	if (!lcd) {
		pr_err("failed to allocate for lcd\n");
		ret = -ENOMEM;
		goto err_alloc;
	}

	lcd->ld = lcd_device_register("panel", dsim->dev, lcd, &s6e3ha2_lcd_ops);
	if (IS_ERR(lcd->ld)) {
		pr_err("failed to register lcd device\n");
		ret = PTR_ERR(lcd->ld);
		goto out_free_lcd;
	}
	dsim->lcd = lcd->ld;

	lcd->bd = backlight_device_register("panel", dsim->dev, lcd, &s6e3ha2_backlight_ops, NULL);
	if (IS_ERR(lcd->bd)) {
		pr_err("failed to register backlight device\n");
		ret = PTR_ERR(lcd->bd);
		goto out_free_backlight;
	}

	lcd->dev = dsim->dev;
	lcd->dsim = dsim;
	lcd->bd->props.max_brightness = EXTEND_BRIGHTNESS;
	lcd->bd->props.brightness = DEFAULT_BRIGHTNESS;
	lcd->brightness = lcd->current_brightness = DEFAULT_BRIGHTNESS;
	lcd->bl = DEFAULT_GAMMA_INDEX;
	lcd->current_bl = lcd->bl;
	lcd->acl_enable = 0;
	lcd->current_acl = 0;
#ifdef CONFIG_S5P_LCD_INIT
	lcd->power = FB_BLANK_POWERDOWN;
#else
	lcd->power = FB_BLANK_UNBLANK;
#endif
	lcd->connected = 1;
	lcd->siop_enable = 0;
	lcd->temperature = NORMAL_TEMPERATURE;
	lcd->lux = -1;

	ret = device_create_file(&lcd->ld->dev, &dev_attr_power_reduce);
	if (ret < 0)
		dev_err(&lcd->ld->dev, "failed to add sysfs entries, %d\n", __LINE__);

	ret = device_create_file(&lcd->ld->dev, &dev_attr_lcd_type);
	if (ret < 0)
		dev_err(&lcd->ld->dev, "failed to add sysfs entries, %d\n", __LINE__);

	ret = device_create_file(&lcd->ld->dev, &dev_attr_window_type);
	if (ret < 0)
		dev_err(&lcd->ld->dev, "failed to add sysfs entries, %d\n", __LINE__);

	ret = device_create_file(&lcd->ld->dev, &dev_attr_manufacture_code);
	if (ret < 0)
		dev_err(&lcd->ld->dev, "failed to add sysfs entries, %d\n", __LINE__);

	ret = device_create_file(&lcd->bd->dev, &dev_attr_brightness_table);
	if (ret < 0)
		dev_err(&lcd->ld->dev, "failed to add sysfs entries, %d\n", __LINE__);

	ret = device_create_file(&lcd->ld->dev, &dev_attr_siop_enable);
	if (ret < 0)
		dev_err(&lcd->ld->dev, "failed to add sysfs entries, %d\n", __LINE__);

	ret = device_create_file(&lcd->ld->dev, &dev_attr_temperature);
	if (ret < 0)
		dev_err(&lcd->ld->dev, "failed to add sysfs entries, %d\n", __LINE__);

	ret = device_create_file(&lcd->ld->dev, &dev_attr_color_coordinate);
	if (ret < 0)
		dev_err(&lcd->ld->dev, "failed to add sysfs entries, %d\n", __LINE__);

	ret = device_create_file(&lcd->ld->dev, &dev_attr_manufacture_date);
	if (ret < 0)
		dev_err(&lcd->ld->dev, "failed to add sysfs entries, %d\n", __LINE__);

	ret = device_create_file(&lcd->ld->dev, &dev_attr_dump_register);
	if (ret < 0)
		dev_err(&lcd->ld->dev, "failed to add sysfs entries, %d\n", __LINE__);

	ret = device_create_file(&lcd->ld->dev, &dev_attr_lux);
	if (ret < 0)
		dev_err(&lcd->ld->dev, "failed to add sysfs entries, %d\n", __LINE__);

	mutex_init(&lcd->lock);
	mutex_init(&lcd->bl_lock);

	s6e3ha2_write(lcd, SEQ_TEST_KEY_ON_F0, ARRAY_SIZE(SEQ_TEST_KEY_ON_F0));
	s6e3ha2_read_id(lcd, lcd->id);
	s6e3ha2_read_code(lcd, lcd->code);
	s6e3ha2_read_coordinate(lcd);
	s6e3ha2_read_mtp(lcd, mtp_data);
	s6e3ha2_read_elvss(lcd, elvss_data);
	s6e3ha2_read_tset(lcd);
	s6e3ha2_write(lcd, SEQ_TEST_KEY_OFF_F0, ARRAY_SIZE(SEQ_TEST_KEY_OFF_F0));

	dev_info(&lcd->ld->dev, "ID: %x, %x, %x\n", lcd->id[0], lcd->id[1], lcd->id[2]);

	init_dynamic_aid(lcd);

	ret = init_gamma_table(lcd, mtp_data);
	ret += init_aid_dimming_table(lcd);
	ret += init_elvss_table(lcd, elvss_data);
	ret += init_vint_table(lcd);

	if (ret)
		dev_info(&lcd->ld->dev, "gamma table generation is failed\n");

	show_lcd_table(lcd);

	lcd->ldi_enable = 1;

#if defined(CONFIG_DECON_MDNIE_LITE)
	mdnie_register(&lcd->ld->dev, lcd, (mdnie_w)s6e3ha2_write_set, (mdnie_r)s6e3ha2_read, &tune_info);
	lcd->mdnie_class = get_mdnie_class();
#endif

	update_brightness(lcd, 1);

#if defined(CONFIG_ESD_FG)
	esd_fg_init(lcd);
#endif

	dev_info(&lcd->ld->dev, "%s lcd panel driver has been probed.\n", __FILE__);
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

static int s6e3ha2_displayon(struct mipi_dsim_device *dsim)
{
	struct lcd_info *lcd = dev_get_drvdata(&dsim->lcd->dev);

	s6e3ha2_power(lcd, FB_BLANK_UNBLANK);

	return 0;
}

static int s6e3ha2_suspend(struct mipi_dsim_device *dsim)
{
	struct lcd_info *lcd = dev_get_drvdata(&dsim->lcd->dev);

	s6e3ha2_power(lcd, FB_BLANK_POWERDOWN);

	return 0;
}

static int s6e3ha2_resume(struct mipi_dsim_device *dsim)
{
	return 0;
}

struct mipi_dsim_lcd_driver s6e3ha2_mipi_lcd_driver = {
	.probe		= s6e3ha2_probe,
	.displayon	= s6e3ha2_displayon,
	.suspend	= s6e3ha2_suspend,
	.resume		= s6e3ha2_resume,
};

static int s6e3ha2_init(void)
{
#if defined(CONFIG_LCD_PCD)
	extern unsigned int lcdtype;
	struct device_node *np;
	struct property *prop;

	if ((lcdtype & 0xf) < 2) {
		np = of_find_node_by_path("/decon_fb");
		prop = of_find_property(np, "oled-pcd-gpio", NULL);
		if (prop) {
			of_remove_property(np, prop);
			pr_err("%s: lcdtype: %x so remove oled-pcd-gpio property\n", __func__, lcdtype);
		}
	}
#endif
	return 0;
}

static void s6e3ha2_exit(void)
{
	return;
}

module_init(s6e3ha2_init);
module_exit(s6e3ha2_exit);

MODULE_DESCRIPTION("MIPI-DSI S6E3HA2 (1536*2048) Panel Driver");
MODULE_LICENSE("GPL");

