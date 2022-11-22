/* drivers/video/decon_display/s6e3hf2_mipi_lcd.c
 *
 * Samsung SoC MIPI LCD driver.
 *
 * Copyright (c) 2014 Samsung Electronics
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

#include <video/mipi_display.h>
#include "../decon_display/decon_mipi_dsi.h"

#include "s6e3hf2_param.h"
#include "dynamic_aid_s6e3hf2.h"
#include <linux/syscalls.h>

#if defined(CONFIG_LCD_HMT) && defined(CONFIG_DECON_MIPI_DSI_PKTGO)
#include "../decon_display/decon_reg.h"
#include "../decon_display/dsim_reg.h"
#endif

#define CONFIG_ESD_FG

#if defined(CONFIG_DECON_MDNIE_LITE)
#include "mdnie.h"
#include "mdnie_lite_table_tb.h"
#endif

#define POWER_IS_ON(pwr)		(pwr <= FB_BLANK_NORMAL)
#define LEVEL_IS_HBM(brightness)	(brightness == EXTEND_BRIGHTNESS)
#define LEVEL_IS_CAPS_OFF(level)	(level <= 19)


#define NORMAL_TEMPERATURE		25	/* 25 degrees Celsius */

#define EXTEND_BRIGHTNESS		355
#define MIN_BRIGHTNESS		0
#define MAX_BRIGHTNESS		255
#define DEFAULT_BRIGHTNESS		134

#define MIN_GAMMA			2
#define MAX_GAMMA			350

#define HMT_MAX_GAMMA		150


#define DEFAULT_GAMMA_INDEX		IBRIGHTNESS_183NT

#define LDI_HBMGAMMA_REG		0xB4
#define LDI_HBMGAMMA_LEN		31

#define LDI_ID_REG			0x04
#define LDI_ID_LEN			3
#define LDI_CODE_REG			0xD6
#define LDI_CODE_LEN			5
#define LDI_MTP_REG			0xC8
#define LDI_MTP_LEN			44	/* MTP + Manufacture Date */
#define LDI_TSET_REG			0xB8 /* TSET: Global para 8th */
#define LDI_TSET_LEN			8
#define TSET_PARAM_SIZE			(LDI_TSET_LEN + 1)
#define LDI_ELVSS_REG			0xB6
#define LDI_ELVSS_LEN			(ELVSS_PARAM_SIZE - 1)
#define LDI_VDDMRVDD_REG		0xD7
#define LDI_VDDMRVDD_LEN		23
#define RVDD_NUM 21
#define VDDM_NUM 22


#define LDI_COORDINATE_REG		0xA1
#define LDI_COORDINATE_LEN		4

#define LDI_RDDPM_REG		0x0A
#define LDI_BOOSTER_VAL		0x9C
#define LDI_ESDERR_REG		0xEE

#ifdef SMART_DIMMING_DEBUG
#define smtd_dbg(format, arg...)	printk(format, ##arg)
#else
#define smtd_dbg(format, arg...)
#endif

#define GET_UPPER_4BIT(x)		((x >> 4) & 0xF)
#define GET_LOWER_4BIT(x)		(x & 0xF)

static const unsigned int DIM_TABLE[IBRIGHTNESS_MAX] = {
	2,	3,	4,	5,	6,	7,	8,	9,	10,	11,
	12,	13,	14,	15,	16,	17,	19,	20,	21,	22,
	24,	25,	27,	29,	30,	32,	34,	37,	39,	41,
	44,	47,	50,	53,	56,	60,	64,	68,	72,	77,
	82,	87,	93,	98,	105,	111,	119,	126,	134,	143,
	152,	162,	172,	183,	195,	207,	220,	234,	249,	265,
	282,	300,	316,	333,	MAX_GAMMA,	500
};

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
	struct dynamic_aid_param_t	daid;
	unsigned char			aor[IBRIGHTNESS_MAX][ARRAY_SIZE(SEQ_AOR_CONTROL)];
	unsigned int			connected;

	unsigned char			tset_table[TSET_PARAM_SIZE];
	int				temperature;
	unsigned int			coordinate[2];
	unsigned int			partial_range[2];
	unsigned char			date[4];

	int						lux;
	struct class			*mdnie_class;

	unsigned int			width;
	unsigned int			height;

#ifdef CONFIG_LCD_ALPM
	int				alpm;
	int				current_alpm;
	struct mutex			alpm_lock;
#endif
#ifdef CONFIG_LCD_HMT
	struct dynamic_aid_param_t	hmt_daid;
	unsigned int			hmt_on;
	unsigned int			hmt_brightness;
	unsigned int			current_hmt_on;
	unsigned int			hmt_bl;
	unsigned int			hmt_current_bl;
	unsigned char			hmt_current_acl;

	unsigned char			**hmt_gamma_table;
#endif

#if defined(CONFIG_ESD_FG)
		unsigned int			esd_fg_irq;
		unsigned int			esd_fg_gpio;
		struct delayed_work		esd_fg;
		bool				esd_fg_state;
		unsigned int			esd_fg_count;
		spinlock_t			esd_fg_lock;
#endif

		unsigned int			mcd_on;
		unsigned char			mcd_buf1bb[2];
		unsigned char			mcd_buf2cb[59];
		unsigned char			mcd_buf3f6[2];

#if defined(CONFIG_OLED_DET)
		unsigned int			oled_det_irq;
		unsigned int			oled_det_gpio;
#endif
	unsigned int			vddm_delta;
	unsigned int			rvdd_delta;

	struct mipi_dsim_device		*dsim;
};

#ifdef CONFIG_LCD_HMT
static int s6e3hf2_hmt_update(struct lcd_info *lcd, u8 forced);
#endif

#if defined(CONFIG_ESD_FG)
static void esd_fg_enable(struct lcd_info *lcd, int enable)
{
	struct device *dev = lcd->dsim->dev;

	if (!lcd->connected)
		return;

	dev_info(dev, "%s: %s, gpio: %d, state: %d\n", __func__, enable ? "enable" : "disable",
		gpio_get_value(lcd->esd_fg_gpio), lcd->esd_fg_state);

	if (!lcd->esd_fg_irq)
		return;

	if (enable)
		enable_irq(lcd->esd_fg_irq);
	else {
		if (!lcd->esd_fg_state)
			disable_irq(lcd->esd_fg_irq);
		if (IS_ERR(devm_pinctrl_get_select(dev, "errfg_off")))
			dev_info(dev, "%s err\n", __func__);
	}
}

static int esd_fg_blank(struct lcd_info *lcd)
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
}

static void esd_fg_work(struct work_struct *work)
{
	struct lcd_info *lcd =
		container_of(work, struct lcd_info, esd_fg.work);

	dev_info(&lcd->ld->dev, "%s: level: %d, state: %d, count: %d\n",
		__func__, gpio_get_value(lcd->esd_fg_gpio), lcd->esd_fg_state, lcd->esd_fg_count);

	esd_fg_is_detected(lcd);

	esd_fg_blank(lcd);
}

static irqreturn_t esd_fg_isr(int irq, void *dev_id)
{
	struct lcd_info *lcd = dev_id;
	unsigned int esd_fg_level = gpio_get_value(lcd->esd_fg_gpio);

	dev_info(&lcd->ld->dev, "%s: esd_fg(%d) is detected\n", __func__, esd_fg_level);

	lcd->esd_fg_count++;

	if (esd_fg_level && (!lcd->esd_fg_state)) {
		disable_irq_nosync(lcd->esd_fg_irq);
		schedule_delayed_work(&lcd->esd_fg, 0);
	}

	return IRQ_HANDLED;
}
#if defined(CONFIG_OLED_DET)
static irqreturn_t oled_det_isr(int irq, void *dev_id)
{
	struct lcd_info *lcd = dev_id;
	unsigned int oled_det_level = gpio_get_value(lcd->oled_det_gpio);

	dev_info(&lcd->ld->dev, "%s: oled_det(%d) is detected\n", __func__, oled_det_level);

	return IRQ_HANDLED;
}
#endif
static int esd_fg_init(struct lcd_info *lcd)
{
	struct device *dev = lcd->dsim->dev;
	int ret = 0;

	if (!lcd->connected)
		return ret;

	lcd->esd_fg_gpio = of_get_named_gpio(dev->of_node, "oled-errfg-gpio", 0);
	if (!lcd->esd_fg_gpio) {
		dev_err(&lcd->ld->dev, "failed to get proper gpio number\n");
		return -EINVAL;
	}

	if (gpio_get_value(lcd->esd_fg_gpio)) {
		dev_info(dev, "%s: gpio(%d) is already high\n", __func__, lcd->esd_fg_gpio);
		return ret;
	}

	lcd->esd_fg_irq = gpio_to_irq(lcd->esd_fg_gpio);
	if (!lcd->esd_fg_irq) {
		dev_err(&lcd->ld->dev, "failed to get proper irq number\n");
		return -EINVAL;
	}

	spin_lock_init(&lcd->esd_fg_lock);

	INIT_DELAYED_WORK(&lcd->esd_fg, esd_fg_work);

	ret = request_irq(lcd->esd_fg_irq, esd_fg_isr, IRQF_TRIGGER_RISING, "esd_fg", lcd);
	if (ret) {
		dev_err(&lcd->ld->dev, "esd_fg irq request failed\n");
		return ret;
	}

	dev_info(dev, "%s: esd_fg(%d)\n", __func__, gpio_get_value(lcd->esd_fg_gpio));

#if defined(CONFIG_OLED_DET)
	if(!of_find_property(dev->of_node, "oled-det-gpio", 0)) {
		dev_err(&lcd->ld->dev, "there is no oled-det-gpio property. check hw rev\n");
		lcd->oled_det_gpio = -1;
		return ret;
	}

	lcd->oled_det_gpio = of_get_named_gpio(dev->of_node, "oled-det-gpio", 0);
	if(lcd->oled_det_gpio == -ENOENT) {
		dev_err(&lcd->ld->dev, "failed to get oled-det-gpio number check hwrev\n");
		return -EINVAL;
	}
	dev_info(dev, "%s: oled_det_gpio = %d\n", __func__, gpio_get_value(lcd->oled_det_gpio));

	lcd->oled_det_irq = gpio_to_irq(lcd->oled_det_gpio);
	if (!lcd->oled_det_irq) {
		dev_err(&lcd->ld->dev, "failed to get proper oled_det irq number\n");
		return -EINVAL;
	}

	ret = request_irq(lcd->oled_det_irq, oled_det_isr, IRQF_TRIGGER_RISING | IRQF_TRIGGER_FALLING, "oled_det", lcd);
	if (ret) {
		dev_err(&lcd->ld->dev, "oled_det irq request failed\n");
		return ret;
	}
#endif
	return ret;
}
#endif

int s6e3hf2_write(struct lcd_info *lcd, const u8 *seq, u32 len)
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

int s6e3hf2_read(struct lcd_info *lcd, u8 addr, u8 *buf, u32 len)
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
		dev_err(&lcd->ld->dev, "mipi_read failed retry ..\n");
		retry--;
		goto read_data;
	}

read_err:
	mutex_unlock(&lcd->lock);
	return ret;
}

#if defined(CONFIG_LCD_ALPM) || defined(CONFIG_LCD_HMT) || defined(CONFIG_DECON_MDNIE_LITE)
static int s6e3hf2_write_set(struct lcd_info *lcd, struct lcd_seq_info *seq, u32 num)
{
	int ret = 0, i;

	for (i = 0; i < num; i++) {
		if (seq[i].cmd) {
			ret = s6e3hf2_write(lcd, seq[i].cmd, seq[i].len);
			if (ret != 0) {
				dev_info(&lcd->ld->dev, "%s failed.\n", __func__);
				return ret;
			}
		}
		if (seq[i].sleep)
			usleep_range(seq[i].sleep * 1000 , seq[i].sleep * 1000);
	}
	return ret;
}
#endif

#if defined(CONFIG_DECON_MDNIE_LITE)
static int s6e3hf2_send_seq(struct lcd_info *lcd, struct lcd_seq_info *seq, u32 num)
{
	int ret = 0;

	mutex_lock(&lcd->bl_lock);
	ret = s6e3hf2_write_set(lcd, seq, num);
	mutex_unlock(&lcd->bl_lock);

	return ret;
}
#endif

static int s6e3hf2_read_coordinate(struct lcd_info *lcd)
{
	int ret;
	unsigned char buf[LDI_COORDINATE_LEN] = {0,};

	ret = s6e3hf2_read(lcd, LDI_COORDINATE_REG, buf, LDI_COORDINATE_LEN);

	if (ret < 1)
		dev_err(&lcd->ld->dev, "%s failed\n", __func__);

	lcd->coordinate[0] = buf[0] << 8 | buf[1];	/* X */
	lcd->coordinate[1] = buf[2] << 8 | buf[3];	/* Y */

	return ret;
}

static int s6e3hf2_read_id(struct lcd_info *lcd, u8 *buf)
{
	int ret;

	ret = s6e3hf2_read(lcd, LDI_ID_REG, buf, LDI_ID_LEN);

	if (ret < 1) {
		lcd->connected = 0;
		dev_info(&lcd->ld->dev, "panel is not connected well\n");
	}

	return ret;
}

static int s6e3hf2_read_code(struct lcd_info *lcd, u8 *buf)
{
	int ret;

	ret = s6e3hf2_read(lcd, LDI_CODE_REG, buf, LDI_CODE_LEN);

	if (ret < 1)
		dev_info(&lcd->ld->dev, "%s failed\n", __func__);

	return ret;
}

static int s6e3hf2_read_mtp(struct lcd_info *lcd, u8 *buf)
{
	int ret, i;

	ret = s6e3hf2_read(lcd, LDI_MTP_REG, buf, LDI_MTP_LEN);

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

static int s6e3hf2_read_elvss(struct lcd_info *lcd, u8 *buf)
{
	int ret, i;

	ret = s6e3hf2_read(lcd, LDI_ELVSS_REG, buf, LDI_ELVSS_LEN);

	smtd_dbg("%s: %02xh\n", __func__, LDI_ELVSS_REG);
	for (i = 0; i < LDI_ELVSS_LEN; i++)
		smtd_dbg("%02dth value is %02x\n", i+1, (int)buf[i]);

	return ret;
}


static int s6e3hf2_read_tset(struct lcd_info *lcd)
{
	int ret, i;

	ret = s6e3hf2_read(lcd, LDI_TSET_REG, &lcd->tset_table[1], LDI_TSET_LEN);

	smtd_dbg("%s: %02xh\n", __func__, LDI_TSET_REG);
	for (i = 0; i < LDI_TSET_LEN; i++)
		smtd_dbg("%02dth value is %02x\n", i, lcd->tset_table[i]);

	lcd->tset_table[0] = LDI_TSET_REG;

	return ret;
}

static int s6e3hf2_read_vddmrvdd(struct lcd_info *lcd)
{
	int ret, i, rvdd_delta, vddm_delta;
	unsigned char buf[LDI_VDDMRVDD_LEN];

	ret = s6e3hf2_read(lcd, LDI_VDDMRVDD_REG, buf, LDI_VDDMRVDD_LEN);

	smtd_dbg("%s: %02xh\n", __func__, LDI_VDDMRVDD_REG);
	for (i = 0; i < LDI_VDDMRVDD_LEN; i++)
		smtd_dbg("%02dth value is %02x\n", i,buf[i]);

	rvdd_delta = buf[RVDD_NUM];
	vddm_delta = buf[VDDM_NUM];

	lcd->vddm_delta = VDDMRVDD_LUT[vddm_delta];
	lcd->rvdd_delta = VDDMRVDD_LUT[rvdd_delta];

	return ret;
}

static int get_backlight_level_from_brightness(int brightness)
{
	int backlightlevel = DEFAULT_GAMMA_INDEX;
	int i, gamma;

	gamma = (brightness * MAX_GAMMA) / MAX_BRIGHTNESS;
	for (i = 0; i < IBRIGHTNESS_500NT; i++) {
		if (brightness <= MIN_GAMMA) {
			backlightlevel = 0;
			break;
		}

		if (DIM_TABLE[i] > gamma)
			break;

		backlightlevel = i;
	}

	return backlightlevel;
}

static int s6e3hf2_gamma_ctl(struct lcd_info *lcd)
{
	int ret = 0;

	ret = s6e3hf2_write(lcd, lcd->gamma_table[lcd->bl], GAMMA_PARAM_SIZE);
	if (!ret)
		ret = -EPERM;

	return ret;
}

static int s6e3hf2_aid_parameter_ctl(struct lcd_info *lcd, u8 force)
{
	int ret = 0;

	if (force)
		goto aid_update;
	else if (lcd->aor[lcd->bl][1] !=  lcd->aor[lcd->current_bl][1])
		goto aid_update;
	else if (lcd->aor[lcd->bl][2] !=  lcd->aor[lcd->current_bl][2])
		goto aid_update;
	else
		goto exit;

aid_update:
	ret = s6e3hf2_write(lcd, lcd->aor[lcd->bl], AID_PARAM_SIZE);
	if (!ret)
		ret = -EPERM;

exit:
	return ret;
}

static int s6e3hf2_set_acl(struct lcd_info *lcd, u8 force)
{
	int ret = 0, level = ACL_STATUS_15P;

	if (lcd->siop_enable || LEVEL_IS_HBM(lcd->brightness))
		goto acl_update;

	if (!lcd->acl_enable)
		level = ACL_STATUS_0P;

acl_update:
	if (force || lcd->current_acl != ACL_CUTOFF_TABLE[level][1]) {
		ret = s6e3hf2_write(lcd, ACL_CUTOFF_TABLE[level], ACL_PARAM_SIZE);
		ret += s6e3hf2_write(lcd, ACL_OPR_TABLE[level], OPR_PARAM_SIZE);
		lcd->current_acl = ACL_CUTOFF_TABLE[level][1];
		dev_info(&lcd->ld->dev, "acl: %d, brightness: %d\n", lcd->current_acl, lcd->brightness);
	}

	if (!ret)
		ret = -EPERM;

	return ret;
}

/* CAPS enable flag should be decided before elvss setting */
static void s6e3hf2_set_caps(struct lcd_info *lcd)
{
	int level = CAPS_OFF;

	if (lcd->siop_enable || LEVEL_IS_HBM(lcd->brightness))		/* auto ACL ON = CAPS OFF */
		goto exit;

	if (LEVEL_IS_CAPS_OFF(index_brightness_table[lcd->bl]))		/* 2 ~ 19 nit = CAPS OFF */
		goto exit;

	if (!lcd->acl_enable)
		level = CAPS_ON;
exit:
	lcd->caps_enable = level;

	return;
}

static int s6e3hf2_set_elvss(struct lcd_info *lcd, u8 force)
{
	int ret = 0, i, elvss_level;
	u32 nit, temperature;
	union elvss_info elvss;

	nit = index_brightness_table[lcd->bl];
	elvss_level = ELVSS_STATUS_350;
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
	ret = s6e3hf2_write(lcd, lcd->elvss_table[lcd->caps_enable][temperature][elvss_level], ELVSS_PARAM_SIZE);
	dev_info(&lcd->ld->dev, "caps: %d, temperature: %d, elvss: %d, %x\n",
		lcd->caps_enable, temperature, elvss_level, elvss.value);
	lcd->current_elvss.value = elvss.value;
	if (!ret)
		ret = -EPERM;

exit:
	return ret;
}


static int s6e3hf2_set_tset(struct lcd_info *lcd, u8 force)
{
	int ret = 0;
	u8 tset;

	tset = ((lcd->temperature >= 0) ? 0 : BIT(7)) | abs(lcd->temperature);

	if (force || lcd->tset_table[LDI_TSET_LEN] != tset) {
		lcd->tset_table[LDI_TSET_LEN] = tset;
		ret = s6e3hf2_write(lcd, lcd->tset_table, TSET_PARAM_SIZE);
		dev_info(&lcd->ld->dev, "temperature: %d, tset: %d\n", lcd->temperature, tset);
	}

	if (!ret)
		ret = -EPERM;

	return ret;
}

static int s6e3hf2_set_hbm(struct lcd_info *lcd, u8 force)
{
	int ret = 0, level = LEVEL_IS_HBM(lcd->brightness);

	if (force || lcd->current_hbm != HBM_TABLE[level][1]) {
		ret = s6e3hf2_write(lcd, HBM_TABLE[level], HBM_PARAM_SIZE);
		lcd->current_hbm = HBM_TABLE[level][1];
		dev_info(&lcd->ld->dev, "hbm: %d, brightness: %d\n", lcd->current_hbm, lcd->brightness);
	}

	if (!ret)
		ret = -EPERM;

	return ret;
}

static int s6e3hf2_set_partial_display(struct lcd_info *lcd)
{
	if (lcd->partial_range[0] || lcd->partial_range[1]) {
		s6e3hf2_write(lcd, SEQ_PARTIAL_AREA, ARRAY_SIZE(SEQ_PARTIAL_AREA));
		s6e3hf2_write(lcd, SEQ_PARTIAL_MODE_ON, ARRAY_SIZE(SEQ_PARTIAL_MODE_ON));
	} else
		s6e3hf2_write(lcd, SEQ_NORMAL_MODE_ON, ARRAY_SIZE(SEQ_NORMAL_MODE_ON));

	return 0;
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

#if defined(IV_0)
	mtp_data[35] = mtp_data[34];				/* VT B */
	mtp_data[34] = GET_UPPER_4BIT(mtp_data[33]);	/* VT G */
	mtp_data[33] = GET_LOWER_4BIT(mtp_data[33]);	/* VT R */
#else
	mtp_data[32] = mtp_data[34];				/* VT B */
	mtp_data[31] = GET_UPPER_4BIT(mtp_data[33]);	/* VT G */
	mtp_data[30] = GET_LOWER_4BIT(mtp_data[33]);	/* VT R */
#endif

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
			smtd_dbg("mtp_data[%d] = %d\n", j, mtp_data[j]);

	for (i = 0, j = 0; i < IV_MAX; i++)
		for (c = 0; c < CI_MAX; c++, j++)
			smtd_dbg("mtp[%d] = %d\n", j, mtp[j]);

	for (i = 0, j = 0; i < IV_MAX; i++) {
		for (c = 0; c < CI_MAX; c++, j++)
			smtd_dbg("%04d ", mtp[j]);
		smtd_dbg("\n");
	}
}

static int init_gamma_table(struct lcd_info *lcd , u8 *mtp_data)
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

	for (i = 0; i < IBRIGHTNESS_MAX; i++)
		memcpy(lcd->aor[i], SEQ_AOR_CONTROL, ARRAY_SIZE(SEQ_AOR_CONTROL));

	for (i = 0; i < IBRIGHTNESS_MAX; i++) {
		lcd->aor[i][1] = aor_cmd[i][0];
		lcd->aor[i][2] = aor_cmd[i][1];
	}

	return 0;
}

static int init_elvss_table(struct lcd_info *lcd, u8* elvss_data)
{
	int i, temp, acl, caps, ret;

	for (caps = 0; caps < CAPS_MAX; caps++) {
		for (acl = 0; acl < ACL_STATUS_MAX; acl++) {
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
					lcd->elvss_table[caps][temp][i][22] = temp ? (elvss_data[21] - 3) : elvss_data[21];
				}
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
#ifdef CONFIG_LCD_HMT
	if (lcd->hmt_on) {
		dev_info(&lcd->ld->dev, "skip brightness control because HMT is enabled\n");
		return 0;
	}
#endif

	mutex_lock(&lcd->bl_lock);

	lcd->brightness = lcd->bd->props.brightness;

	lcd->bl = get_backlight_level_from_brightness(lcd->brightness > MAX_BRIGHTNESS ? MAX_BRIGHTNESS : lcd->brightness);

	if (LEVEL_IS_HBM(lcd->brightness))
		lcd->bl = IBRIGHTNESS_500NT;

	if (force || (lcd->ldi_enable && (lcd->current_bl != lcd->bl))) {
		s6e3hf2_write(lcd, SEQ_TEST_KEY_ON_F0, ARRAY_SIZE(SEQ_TEST_KEY_ON_F0));
		s6e3hf2_gamma_ctl(lcd);
		s6e3hf2_aid_parameter_ctl(lcd, force);
		s6e3hf2_set_caps(lcd);
		s6e3hf2_set_elvss(lcd, force);
		s6e3hf2_write(lcd, SEQ_GAMMA_UPDATE, ARRAY_SIZE(SEQ_GAMMA_UPDATE));
		s6e3hf2_write(lcd, SEQ_GAMMA_UPDATE_L, ARRAY_SIZE(SEQ_GAMMA_UPDATE_L));
		s6e3hf2_set_acl(lcd, force);
		s6e3hf2_set_tset(lcd, force);
		if (!(lcd->current_alpm && lcd->alpm))
			s6e3hf2_set_hbm(lcd, force);
		s6e3hf2_write(lcd, SEQ_TEST_KEY_OFF_F0, ARRAY_SIZE(SEQ_TEST_KEY_OFF_F0));

		lcd->current_bl = lcd->bl;

		dev_info(&lcd->ld->dev, "brightness=%d, bl=%d, candela=%d\n",
			lcd->brightness, lcd->bl, index_brightness_table[lcd->bl]);
	}

	mutex_unlock(&lcd->bl_lock);

	return 0;
}

static int s6e3hf2_set_fps(struct lcd_info *lcd)
{
	int ret = 0;
	s6e3hf2_write_set(lcd, SEQ_LDI_FPS_SET, ARRAY_SIZE(SEQ_LDI_FPS_SET));
	return ret;
}

static int s6e3hf2_set_rvddvddm(struct lcd_info *lcd)
{
	int ret = 0;
	unsigned char SEQ_VDDM_RVDD_SET[] = {
		0xD7, 0, 0
	};
	SEQ_VDDM_RVDD_SET[1] = lcd->rvdd_delta;
	SEQ_VDDM_RVDD_SET[2] = lcd->vddm_delta;

	dev_info(&lcd->ld->dev, "%s: rvdd_delta = %x, vddm_delta = %x\n", __func__, lcd->rvdd_delta, lcd->vddm_delta);

	s6e3hf2_write(lcd, SEQ_GLOBAL_PARAM_21, ARRAY_SIZE(SEQ_GLOBAL_PARAM_21));
	s6e3hf2_write(lcd, SEQ_VDDM_RVDD_SET, ARRAY_SIZE(SEQ_VDDM_RVDD_SET));
	s6e3hf2_write(lcd, SEQ_FREQ_CALIBRATION_SETTING4, ARRAY_SIZE(SEQ_FREQ_CALIBRATION_SETTING4));
	s6e3hf2_write(lcd, SEQ_FREQ_CALIBRATION_SETTING5, ARRAY_SIZE(SEQ_FREQ_CALIBRATION_SETTING5));

	return ret;
}

static int s6e3hf2_ldi_init(struct lcd_info *lcd)
{
	int ret = 0;

	lcd->connected = 1;

	/* 7. Sleep Out(11h) */
	s6e3hf2_write(lcd, SEQ_SLEEP_OUT, ARRAY_SIZE(SEQ_SLEEP_OUT));

	/* 8. Wait 5ms */
	msleep(5);

	/* 9. Interface Setting */
	s6e3hf2_write(lcd, SEQ_TEST_KEY_ON_F0, ARRAY_SIZE(SEQ_TEST_KEY_ON_F0));
	s6e3hf2_write(lcd, SEQ_SINGLE_DSI_SET1, ARRAY_SIZE(SEQ_SINGLE_DSI_SET1));
	s6e3hf2_write(lcd, SEQ_SINGLE_DSI_SET2, ARRAY_SIZE(SEQ_SINGLE_DSI_SET2));

	/* Calibration code */
	s6e3hf2_write(lcd, SEQ_TEST_KEY_ON_FC, ARRAY_SIZE(SEQ_TEST_KEY_ON_FC));
	s6e3hf2_set_rvddvddm(lcd);
	s6e3hf2_write(lcd, SEQ_FREQ_MEMCLOCK_SETTING, ARRAY_SIZE(SEQ_FREQ_MEMCLOCK_SETTING));

	s6e3hf2_set_fps(lcd);

	/* 10. Wait 120ms */
	msleep(120);

	/* 11. Module Information READ */
	s6e3hf2_read_id(lcd, lcd->id);

	/* 12. Common Setting */
	s6e3hf2_write(lcd, SEQ_TE_ON, ARRAY_SIZE(SEQ_TE_ON));
	s6e3hf2_write(lcd, SEQ_TOUCH_HSYNC_ON, ARRAY_SIZE(SEQ_TOUCH_HSYNC_ON));
	s6e3hf2_write(lcd, SEQ_PENTILE_CONTROL, ARRAY_SIZE(SEQ_PENTILE_CONTROL));

	s6e3hf2_write_set(lcd, SEQ_POC_SET, ARRAY_SIZE(SEQ_POC_SET));
	s6e3hf2_write(lcd, SEQ_TEST_KEY_OFF_FC, ARRAY_SIZE(SEQ_TEST_KEY_OFF_FC));

	/* PCD setting Off for TB */
	s6e3hf2_write(lcd, SEQ_PCD_SET_OFF, ARRAY_SIZE(SEQ_PCD_SET_OFF));

	s6e3hf2_write(lcd, SEQ_ERR_FG_SETTING, ARRAY_SIZE(SEQ_ERR_FG_SETTING));
	s6e3hf2_write(lcd, SEQ_TE_START_SETTING, ARRAY_SIZE(SEQ_TE_START_SETTING));

	s6e3hf2_write(lcd, SEQ_TEST_KEY_OFF_F0, ARRAY_SIZE(SEQ_TEST_KEY_OFF_F0));

	return ret;
}

static int s6e3hf2_check_panel(struct lcd_info *lcd)
{
	int ret = 0;
	u8 Booster_volt;

	ret = s6e3hf2_read(lcd, LDI_RDDPM_REG, &Booster_volt, 1);
	if(ret < 0 || Booster_volt != LDI_BOOSTER_VAL) {
		dev_err(&lcd->ld->dev, "BOOSTER_VAL : 0x%x, ret = %d\n", Booster_volt, ret);
#if defined(CONFIG_OLED_DET)
		if (gpio_is_valid(lcd->oled_det_gpio))
			dev_info(&lcd->ld->dev, "%s: oled_det val = (%d)\n", __func__, gpio_get_value(lcd->oled_det_gpio));
#endif
		s6e3hf2_write(lcd, SEQ_TEST_KEY_ON_F0, ARRAY_SIZE(SEQ_TEST_KEY_ON_F0));
		ret = s6e3hf2_read(lcd, LDI_ESDERR_REG, &Booster_volt, 1);
		dev_err(&lcd->ld->dev, "ESDERR : 0x%x, ret = %d\n", Booster_volt, ret);
		s6e3hf2_write(lcd, SEQ_TEST_KEY_OFF_F0, ARRAY_SIZE(SEQ_TEST_KEY_OFF_F0));
	}

	return 0;
}

static int s6e3hf2_ldi_enable(struct lcd_info *lcd)
{
	s6e3hf2_write(lcd, SEQ_DISPLAY_ON, ARRAY_SIZE(SEQ_DISPLAY_ON));

	s6e3hf2_check_panel(lcd);

	dev_info(&lcd->ld->dev, "DISPLAY_ON\n");

	return 0;
}

static int s6e3hf2_ldi_disable(struct lcd_info *lcd)
{
	int ret = 0;

	dev_info(&lcd->ld->dev, "+ %s\n", __func__);

	/* 2. Display Off (28h) */
	s6e3hf2_write(lcd, SEQ_DISPLAY_OFF, ARRAY_SIZE(SEQ_DISPLAY_OFF));

	dev_info(&lcd->ld->dev, "DISPLAY_OFF\n");

	/* 3. Sleep In (10h) */
	s6e3hf2_write(lcd, SEQ_SLEEP_IN, ARRAY_SIZE(SEQ_SLEEP_IN));

	/* 4. Wait 120ms */
	msleep(120);

	dev_info(&lcd->ld->dev, "- %s\n", __func__);

	return ret;
}

#ifdef CONFIG_LCD_ALPM

static int s6e3hf2_ldi_alpm_init(struct lcd_info *lcd)
{
	int ret = 0;
 	s6e3hf2_write_set(lcd, SEQ_ALPM_ON_SET, ARRAY_SIZE(SEQ_ALPM_ON_SET));

	/* ALPM MODE */
	lcd->current_alpm = lcd->dsim->lcd_alpm = 1;
	return ret;
}

static int s6e3hf2_ldi_alpm_exit(struct lcd_info *lcd)
{
	int ret = 0;
	s6e3hf2_write_set(lcd, SEQ_ALPM_OFF_SET, ARRAY_SIZE(SEQ_ALPM_OFF_SET));
	/* NORMAL MODE */
	lcd->current_alpm = lcd->dsim->lcd_alpm = 0;

	return ret;
}
#endif

static int s6e3hf2_power_on(struct lcd_info *lcd)
{
	int ret = 0;

	dev_info(&lcd->ld->dev, "+ %s\n", __func__);
#if defined(CONFIG_OLED_DET)
	if (gpio_is_valid(lcd->oled_det_gpio))
		enable_irq(lcd->oled_det_irq);
#endif

#ifdef CONFIG_LCD_ALPM
	mutex_lock(&lcd->alpm_lock);
	if (lcd->current_alpm && lcd->alpm) {
//		s6e3hf2_set_partial_display(lcd);
		dev_info(&lcd->ld->dev, "%s : ALPM mode\n", __func__);
	} else if (lcd->current_alpm) {
		ret = s6e3hf2_ldi_alpm_exit(lcd);
		if (ret) {
			dev_err(&lcd->ld->dev, "failed to exit alpm.\n");
			mutex_unlock(&lcd->alpm_lock);
			goto err;
		}
	} else {
		ret = s6e3hf2_ldi_init(lcd);
		if (ret) {
			dev_err(&lcd->ld->dev, "failed to initialize ldi.\n");
			mutex_unlock(&lcd->alpm_lock);
			goto err;
		}

		if (lcd->alpm) {
			ret = s6e3hf2_ldi_alpm_init(lcd);
			if (ret) {
				dev_err(&lcd->ld->dev, "failed to initialize alpm.\n");
				mutex_unlock(&lcd->alpm_lock);
				goto err;
			}
		} else {
			ret = s6e3hf2_ldi_enable(lcd);
			if (ret) {
				dev_err(&lcd->ld->dev, "failed to enable ldi.\n");
				mutex_unlock(&lcd->alpm_lock);
				goto err;
			}
		}
	}
	mutex_unlock(&lcd->alpm_lock);
#else
	ret = s6e3hf2_ldi_init(lcd);
	if (ret) {
		dev_err(&lcd->ld->dev, "failed to initialize ldi.\n");
		goto err;
	}

	ret = s6e3hf2_ldi_enable(lcd);
	if (ret) {
		dev_err(&lcd->ld->dev, "failed to enable ldi.\n");
		goto err;
	}
#endif

	mutex_lock(&lcd->bl_lock);
	lcd->ldi_enable = 1;
	mutex_unlock(&lcd->bl_lock);

	update_brightness(lcd, 1);
#ifdef CONFIG_LCD_HMT
	if(lcd->hmt_on)
		s6e3hf2_hmt_update(lcd, 1);
#endif

#if defined(CONFIG_ESD_FG)
		esd_fg_enable(lcd, 1);
#endif

	dev_info(&lcd->ld->dev, "- %s\n", __func__);
err:
	return ret;
}

static int s6e3hf2_power_off(struct lcd_info *lcd)
{
	int ret = 0;

	dev_info(&lcd->ld->dev, "+ %s\n", __func__);

#if defined(CONFIG_ESD_FG)
#if defined(CONFIG_OLED_DET)
	if (gpio_is_valid(lcd->oled_det_gpio))
		disable_irq(lcd->oled_det_irq);
#endif
		esd_fg_enable(lcd, 0);
#endif

	mutex_lock(&lcd->bl_lock);
	lcd->ldi_enable = 0;
	mutex_unlock(&lcd->bl_lock);

#ifdef CONFIG_LCD_ALPM
	mutex_lock(&lcd->alpm_lock);
	if (lcd->current_alpm && lcd->alpm) {
		lcd->dsim->lcd_alpm = 1;
		dev_info(&lcd->ld->dev, "%s : ALPM mode\n", __func__);
	} else
		ret = s6e3hf2_ldi_disable(lcd);
	mutex_unlock(&lcd->alpm_lock);
#else
	ret = s6e3hf2_ldi_disable(lcd);
#endif

	dev_info(&lcd->ld->dev, "- %s\n", __func__);

	return ret;
}

static int s6e3hf2_power(struct lcd_info *lcd, int power)
{
	int ret = 0;

	if (POWER_IS_ON(power) && !POWER_IS_ON(lcd->power))
		ret = s6e3hf2_power_on(lcd);
	else if (!POWER_IS_ON(power) && POWER_IS_ON(lcd->power))
		ret = s6e3hf2_power_off(lcd);

	if (!ret)
		lcd->power = power;

	return ret;
}

static int s6e3hf2_set_power(struct lcd_device *ld, int power)
{
	struct lcd_info *lcd = lcd_get_data(ld);

	if (power != FB_BLANK_UNBLANK && power != FB_BLANK_POWERDOWN &&
		power != FB_BLANK_NORMAL) {
		dev_err(&lcd->ld->dev, "power value should be 0, 1 or 4.\n");
		return -EINVAL;
	}

	return s6e3hf2_power(lcd, power);
}

static int s6e3hf2_get_power(struct lcd_device *ld)
{
	struct lcd_info *lcd = lcd_get_data(ld);

	return lcd->power;
}

static int s6e3hf2_check_fb(struct lcd_device *ld, struct fb_info *fb)
{
	return 0;
}

static int s6e3hf2_get_brightness(struct backlight_device *bd)
{
	struct lcd_info *lcd = bl_get_data(bd);

	return DIM_TABLE[lcd->bl];
}

static int s6e3hf2_set_brightness(struct backlight_device *bd)
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

static struct lcd_ops s6e3hf2_lcd_ops = {
	.set_power = s6e3hf2_set_power,
	.get_power = s6e3hf2_get_power,
	.check_fb  = s6e3hf2_check_fb,
};

static const struct backlight_ops s6e3hf2_backlight_ops = {
	.get_brightness = s6e3hf2_get_brightness,
	.update_status = s6e3hf2_set_brightness,
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
#ifdef CONFIG_LCD_HMT
			if (lcd->ldi_enable) {
				if (!lcd->hmt_on)
					update_brightness(lcd, 1);
				else
					s6e3hf2_hmt_update(lcd, 1);
			}
#else
			if (lcd->ldi_enable)
				update_brightness(lcd, 1);
#endif
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
		bl = get_backlight_level_from_brightness(i > MAX_BRIGHTNESS ? MAX_BRIGHTNESS : i);
		bl = LEVEL_IS_HBM(i) ? IBRIGHTNESS_500NT : bl;
		pos += sprintf(pos, "%3d %3d\n", i, DIM_TABLE[bl]);
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

static ssize_t partial_disp_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct lcd_info *lcd = dev_get_drvdata(dev);

	sprintf(buf, "%u, %u\n", lcd->partial_range[0], lcd->partial_range[1]);

	dev_info(dev, "%s: %d, %d\n", __func__, lcd->partial_range[0], lcd->partial_range[1]);

	return strlen(buf);
}

static ssize_t partial_disp_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t size)
{
	struct lcd_info *lcd = dev_get_drvdata(dev);
	u32 st, ed;
	u8 *seq_cmd = SEQ_PARTIAL_AREA;

	sscanf(buf, "%9d %9d" , &st, &ed);

	dev_info(dev, "%s: %d, %d\n", __func__, st, ed);

	if ((st >= lcd->height || ed >= lcd->height) || (st > ed)) {
		pr_err("%s:Invalid Input\n", __func__);
		return size;
	}

	lcd->partial_range[0] = st;
	lcd->partial_range[1] = ed;

	seq_cmd[1] = (st >> 8) & 0xFF;/*select msb 1byte*/
	seq_cmd[2] = st & 0xFF;
	seq_cmd[3] = (ed >> 8) & 0xFF;/*select msb 1byte*/
	seq_cmd[4] = ed & 0xFF;

	if (lcd->ldi_enable)
		s6e3hf2_set_partial_display(lcd);

	return size;
}

static ssize_t aid_log_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct lcd_info *lcd = dev_get_drvdata(dev);
	u8 temp[256];
	int i, j, k;
	int *mtp;

	mtp = lcd->daid.mtp;
	for (i = 0, j = 0; i < IV_MAX; i++, j += 3) {
		if (i == 0)
			dev_info(dev, "MTP Offset VT R:%d G:%d B:%d\n",
					mtp[j], mtp[j + 1], mtp[j + 2]);
		else
			dev_info(dev, "MTP Offset V%3d R:%04d G:%04d B:%04d\n",
					lcd->daid.iv_tbl[i], mtp[j], mtp[j + 1], mtp[j + 2]);
	}

	for (i = 0; i < IBRIGHTNESS_MAX; i++) {
		memset(temp, 0, sizeof(temp));
		for (j = 1; j < GAMMA_PARAM_SIZE; j++) {
			if (j == 1 || j == 3 || j == 5)
				k = lcd->gamma_table[i][j++] * 256;
			else
				k = 0;
			snprintf(temp + strnlen(temp, 256), 256, " %d",
				lcd->gamma_table[i][j] + k);
		}

		dev_info(dev, "nit : %3d  %s\n", lcd->daid.ibr_tbl[i], temp);
	}

	dev_info(dev, "%s\n", __func__);

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

#ifdef CONFIG_LCD_ALPM
static ssize_t alpm_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct lcd_info *lcd = dev_get_drvdata(dev);

	sprintf(buf, "%d\n", lcd->alpm);

	dev_info(dev, "%s: %d\n", __func__, lcd->alpm);

	return strlen(buf);
}

static ssize_t alpm_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t size)
{
	struct lcd_info *lcd = dev_get_drvdata(dev);
	int value;

	sscanf(buf, "%9d", &value);
	dev_info(dev, "%s: %d \n", __func__, value);

	mutex_lock(&lcd->alpm_lock);
	if (value) {
		if (lcd->ldi_enable && !lcd->current_alpm)
			s6e3hf2_ldi_alpm_init(lcd);
	} else {
		if (lcd->ldi_enable && lcd->current_alpm)
			s6e3hf2_ldi_alpm_exit(lcd);
	}

	lcd->alpm = value;
	mutex_unlock(&lcd->alpm_lock);

	dev_info(dev, "%s: %d\n", __func__, lcd->alpm);

	return size;
}

static DEVICE_ATTR(alpm, 0664, alpm_show, alpm_store);
#endif

static int s6e3hf2_read_mcd(struct lcd_info *lcd)
{
	int ret;
	unsigned char buf1, buf3;
	unsigned char buf2[58];

	s6e3hf2_write(lcd, SEQ_MCD_ON1, ARRAY_SIZE(SEQ_MCD_ON1));
	ret = s6e3hf2_read(lcd, 0xBB, &(buf1), sizeof(unsigned char));
	lcd->mcd_buf1bb[0] = 0xBB;
	lcd->mcd_buf1bb[1] = buf1;
	if (ret < 1)
		dev_info(&lcd->ld->dev, "%s failed\n", __func__);

	ret += s6e3hf2_read(lcd, 0xCB, buf2, ARRAY_SIZE(buf2));
	lcd->mcd_buf2cb[0] = 0xCB;
	memcpy(&lcd->mcd_buf2cb[1], buf2, sizeof(buf2));
	if (ret < 58)
		dev_info(&lcd->ld->dev, "%s failed\n", __func__);

	s6e3hf2_write(lcd, SEQ_MCD_ON29, ARRAY_SIZE(SEQ_MCD_ON29));
	ret += s6e3hf2_read(lcd, 0xF6, &(buf3), sizeof(unsigned char));
	lcd->mcd_buf3f6[0] = 0xF6;
	lcd->mcd_buf3f6[1] = buf3;
	if (ret < 1)
		dev_info(&lcd->ld->dev, "%s failed\n", __func__);

	return ret;
}

static int mcd_on(struct lcd_info *lcd)
{
	dev_info(&lcd->ld->dev, "mcd_on\n");

	s6e3hf2_write(lcd, SEQ_TEST_KEY_ON_F0, ARRAY_SIZE(SEQ_TEST_KEY_ON_F0));
	s6e3hf2_write_set(lcd, SEQ_MCD_ON_SET, ARRAY_SIZE(SEQ_MCD_ON_SET));
	s6e3hf2_write(lcd, SEQ_GAMMA_UPDATE, ARRAY_SIZE(SEQ_GAMMA_UPDATE));
	s6e3hf2_write(lcd, SEQ_TEST_KEY_OFF_F0, ARRAY_SIZE(SEQ_TEST_KEY_OFF_F0));
	return 0;
}

static int mcd_off(struct lcd_info *lcd)
{
	dev_info(&lcd->ld->dev, "mcd_off\n");

	s6e3hf2_write(lcd, SEQ_TEST_KEY_ON_F0, ARRAY_SIZE(SEQ_TEST_KEY_ON_F0));
	s6e3hf2_write(lcd, SEQ_MCD_ON1, ARRAY_SIZE(SEQ_MCD_ON1));
	s6e3hf2_write(lcd, lcd->mcd_buf1bb, ARRAY_SIZE(lcd->mcd_buf1bb));
	s6e3hf2_write(lcd, lcd->mcd_buf2cb, ARRAY_SIZE(lcd->mcd_buf2cb));
	s6e3hf2_write(lcd, SEQ_MCD_ON29, ARRAY_SIZE(SEQ_MCD_ON29));
	s6e3hf2_write(lcd, lcd->mcd_buf3f6, ARRAY_SIZE(lcd->mcd_buf3f6));
	s6e3hf2_write(lcd, SEQ_GAMMA_UPDATE, ARRAY_SIZE(SEQ_GAMMA_UPDATE));
	s6e3hf2_write(lcd, SEQ_TEST_KEY_OFF_F0, ARRAY_SIZE(SEQ_TEST_KEY_OFF_F0));

	return 0;
}

static ssize_t mcd_mode_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct lcd_info *lcd = dev_get_drvdata(dev);

	sprintf(buf, "%d\n", lcd->mcd_on);

	dev_info(dev, "%s: %d\n", __func__, lcd->mcd_on);

	return strlen(buf);
}

static ssize_t mcd_mode_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t size)
{
	struct lcd_info *lcd = dev_get_drvdata(dev);
	int value;

	sscanf(buf, "%9d", &value);
	dev_info(dev, "%s: %d \n", __func__, value);

	if (lcd->ldi_enable && (lcd->mcd_on != value)) {
		if(value)
			mcd_on(lcd);
		else
			mcd_off(lcd);
		lcd->mcd_on = value;
	}

	dev_info(dev, "%s: %d\n", __func__, lcd->mcd_on);

	return size;
}
static DEVICE_ATTR(mcd_mode, 0664, mcd_mode_show, mcd_mode_store);

#ifdef CONFIG_LCD_HMT
static const unsigned int HMT_DIM_TABLE[HMT_IBRIGHTNESS_MAX] = {
	20,	21,	22,	23,	25,	27,	29,	31,	33,	35,
	37,	39,	41,	44,	47,	50,	53,	56,	60,	64,
	68,	72,	77,	82,	87,	93,	99,	105, 360
};

static int hmt_init(struct lcd_info *lcd)
{
	mutex_lock(&lcd->bl_lock);
	lcd->hmt_on = lcd->current_hmt_on = 0;
	lcd->hmt_bl = lcd->hmt_current_bl = 0;
	lcd->hmt_brightness = DEFAULT_BRIGHTNESS;
	lcd->hmt_current_acl = 0;
	mutex_unlock(&lcd->bl_lock);

	return 0;
}

static int hmt_on_set(struct lcd_info *lcd, u8 forced)
{
	int ret = 0;

	if (forced || (lcd->current_hmt_on != lcd->hmt_on)) {
		if (lcd->hmt_on) {
			s6e3hf2_write_set(lcd, SEQ_HMT_SINGLE_SET, ARRAY_SIZE(SEQ_HMT_SINGLE_SET));
			s6e3hf2_write_set(lcd, SEQ_HMT_REVERSE_SET, ARRAY_SIZE(SEQ_HMT_REVERSE_SET));
		} else {
			s6e3hf2_write_set(lcd, SEQ_HMT_OFF_SET, ARRAY_SIZE(SEQ_HMT_OFF_SET));
			s6e3hf2_write_set(lcd, SEQ_HMT_AID_FORWARD_SET, ARRAY_SIZE(SEQ_HMT_AID_FORWARD_SET));
		}
	}

	lcd->current_hmt_on = lcd->hmt_on;
	dev_info(&lcd->ld->dev, "%s : lcd->current_hmt_on = %d\n", __func__, lcd->current_hmt_on);

	return ret;
}

static int get_hmt_level_from_brightness(int brightness)
{
	int backlightlevel = 0;
	int i, gamma;

	gamma = (brightness * HMT_MAX_GAMMA) / MAX_BRIGHTNESS;
	for (i = 0; i < HMT_IBRIGHTNESS_360NT; i++) {
		if (brightness <= MIN_GAMMA) {
			backlightlevel = 0;
			break;
		}

		if (HMT_DIM_TABLE[i] > gamma)
			break;
		backlightlevel = i;
	}

	return backlightlevel;
}

static int hmt_gamma_ctl(struct lcd_info *lcd)
{
	s6e3hf2_write(lcd, lcd->hmt_gamma_table[lcd->hmt_bl], GAMMA_PARAM_SIZE);

	return 0;
}

static int hmt_aid_parameter_ctl(struct lcd_info *lcd, u8 forced)
{
	unsigned char hmt_aor[4] = {0xB2, 0x00, 0x00, 0x00};
	if(forced)
		goto aid_update;
	if (hmt_aor_cmd[lcd->hmt_bl][1] != hmt_aor_cmd[lcd->hmt_current_bl][1])
		goto aid_update;
	else if (hmt_aor_cmd[lcd->hmt_bl][2] != hmt_aor_cmd[lcd->hmt_current_bl][2])
		goto aid_update;

	return 0;
aid_update:
	memcpy(&hmt_aor[1], hmt_aor_cmd[lcd->hmt_bl], ARRAY_SIZE(hmt_aor_cmd[lcd->hmt_bl]));
	s6e3hf2_write(lcd, hmt_aor, ARRAY_SIZE(hmt_aor));

	return 0;
}

static int hmt_set_elvss(struct lcd_info *lcd, u8 forced)
{
	unsigned char buf[3] = {0, };
	if(forced)
		goto elvss_update;

	if(lcd->hmt_current_acl != lcd->acl_enable)
		goto elvss_update;

	return 0;
elvss_update:
	if(lcd->hmt_brightness == HMT_MAX_GAMMA) {
		memcpy(buf, HMT_ELVSS_TABLE[lcd->acl_enable], ARRAY_SIZE(HMT_ELVSS_TABLE[lcd->acl_enable]));
		buf[2] = 0x00;
		s6e3hf2_write(lcd, buf, ARRAY_SIZE(buf));
	}
	else
 		s6e3hf2_write(lcd, HMT_ELVSS_TABLE[lcd->acl_enable], ARRAY_SIZE(HMT_ELVSS_TABLE[lcd->acl_enable]));
 	lcd->hmt_current_acl = lcd->acl_enable;
	return 0;
}

static int hmt_read_hbmgamma(struct lcd_info *lcd, u8 *buf)
{
	int ret, i;

	ret = s6e3hf2_read(lcd, LDI_HBMGAMMA_REG, buf, LDI_HBMGAMMA_LEN);

	if (ret < 1)
		dev_err(&lcd->ld->dev, "%s failed\n", __func__);

	smtd_dbg("%s: %02xh\n", __func__, LDI_HBMGAMMA_REG);
	for (i = 0; i < LDI_HBMGAMMA_LEN; i++)
		smtd_dbg("%02dth value is %02x\n", i+1, (int)buf[i]);
	return ret;
}

static int hmt_set_selected_gamma(struct lcd_info *lcd, int ibrightness_index)
{
	int ret, i, j, k;
	u8 hbm_gamma[LDI_HBMGAMMA_LEN] = {0,};
	short hbm_buf16[LDI_HBMGAMMA_LEN - 3] = {0, };
	short hbm_buf16_temp[LDI_HBMGAMMA_LEN - 3] = {0, };

	/* read hbm gamma & hex->dec */
	s6e3hf2_write(lcd, SEQ_TEST_KEY_ON_F0, ARRAY_SIZE(SEQ_TEST_KEY_ON_F0));
	ret = hmt_read_hbmgamma(lcd, hbm_gamma);
	s6e3hf2_write(lcd, SEQ_TEST_KEY_OFF_F0, ARRAY_SIZE(SEQ_TEST_KEY_OFF_F0));
	if (ret < 1)
		dev_err(&lcd->ld->dev, "%s failed\n", __func__);

	for(i = 1, j = 1; i < LDI_HBMGAMMA_LEN; i++, j++) {
		if(i <= 6) {
			hbm_buf16[j] = hbm_gamma[i++] & 0x01;
			hbm_buf16[j] <<= 8;
			hbm_buf16[j] += hbm_gamma[i];
		} else {
			hbm_buf16[j] = hbm_gamma[i];
		}
	}

	smtd_dbg("read hbm gamma\n");
	for (i = 1; i < LDI_HBMGAMMA_LEN - 3; i++) {
		smtd_dbg("%d ", hbm_buf16[i]);
		if(i % CI_MAX == 0)
			smtd_dbg("\n");
	}
	smtd_dbg("\n");

	/* color offset */
	for(j = 1, k = CI_MAX * IV_MAX - CI_MAX; j < LDI_HBMGAMMA_LEN - 3; j++) {
		hbm_buf16_temp[j] = hbm_buf16[j] + (short)(hmt_offset_color[ibrightness_index][k++]);
		if(j % CI_MAX == 0)
			k -= 6;
	}
	for(j = 1, k = 1; j < LDI_HBMGAMMA_LEN - 3; j++)
	{
		if(j <= 3) {
			lcd->hmt_gamma_table[ibrightness_index][k++] = (unsigned char)((hbm_buf16_temp[j] & 0x0100) >> 8);
			lcd->hmt_gamma_table[ibrightness_index][k++] = (unsigned char)(hbm_buf16_temp[j]);
		} else {
			lcd->hmt_gamma_table[ibrightness_index][k++] = (unsigned char)(hbm_buf16_temp[j]);
		}
	}
	smtd_dbg("after color offset\n");
	for (j = 1; j < LDI_HBMGAMMA_LEN - 3; j++)
		smtd_dbg("%d ", hbm_buf16_temp[j]);
	smtd_dbg("\n");
	smtd_dbg("final gamma\n");
	for (j = 1; j < LDI_HBMGAMMA_LEN + 6; j++) {
		smtd_dbg("%d(%02x)", lcd->hmt_gamma_table[ibrightness_index][j], lcd->hmt_gamma_table[ibrightness_index][j]);
		if(j % CI_MAX == 0)
			smtd_dbg("\n");
	}
	smtd_dbg("\n");
	return ret;
}


static int s6e3hf2_hmt_update(struct lcd_info *lcd, u8 forced)
{
#ifdef CONFIG_DECON_MIPI_DSI_PKTGO
	int dsim_mpkt;
#endif

	mutex_lock(&lcd->bl_lock);

	s6e3hf2_write(lcd, SEQ_TEST_KEY_ON_F0, ARRAY_SIZE(SEQ_TEST_KEY_ON_F0));
	s6e3hf2_write(lcd, SEQ_TEST_KEY_ON_FC, ARRAY_SIZE(SEQ_TEST_KEY_ON_FC));
	s6e3hf2_write(lcd, SEQ_TE_OFF, ARRAY_SIZE(SEQ_TE_OFF));

	usleep_range(17000, 20000);

#ifdef CONFIG_DECON_MIPI_DSI_PKTGO
	dsim_mpkt = dsim_reg_get_pkt_go_status();
	if (dsim_mpkt)
		dsim_reg_set_pkt_go_enable(false);
#endif

	hmt_on_set(lcd, forced);

	lcd->hmt_bl = get_hmt_level_from_brightness(lcd->hmt_brightness > MAX_BRIGHTNESS ? MAX_BRIGHTNESS : lcd->hmt_brightness);

	if (lcd->hmt_on && (forced || (lcd->hmt_current_bl != lcd->hmt_bl))) {
		hmt_gamma_ctl(lcd);
		hmt_aid_parameter_ctl(lcd, forced);
		hmt_set_elvss(lcd, forced);
		lcd->hmt_current_bl = lcd->hmt_bl;

		dev_info(&lcd->ld->dev, "brightness=%d, bl=%d, candela=%d\n",
			lcd->hmt_brightness, lcd->hmt_bl, hmt_index_brightness_table[lcd->hmt_bl]);
		s6e3hf2_write(lcd, SEQ_TE_ON, ARRAY_SIZE(SEQ_TE_ON));
		s6e3hf2_write(lcd, SEQ_GAMMA_UPDATE, ARRAY_SIZE(SEQ_GAMMA_UPDATE));
		s6e3hf2_write(lcd, SEQ_GAMMA_UPDATE_L, ARRAY_SIZE(SEQ_GAMMA_UPDATE_L));
		s6e3hf2_write(lcd, SEQ_TEST_KEY_OFF_FC, ARRAY_SIZE(SEQ_TEST_KEY_OFF_FC));
		s6e3hf2_write(lcd, SEQ_TEST_KEY_OFF_F0, ARRAY_SIZE(SEQ_TEST_KEY_OFF_F0));

		mutex_unlock(&lcd->bl_lock);
	} else {
		s6e3hf2_write(lcd, SEQ_TE_ON, ARRAY_SIZE(SEQ_TE_ON));
		s6e3hf2_write(lcd, SEQ_TEST_KEY_OFF_FC, ARRAY_SIZE(SEQ_TEST_KEY_OFF_FC));
		s6e3hf2_write(lcd, SEQ_TEST_KEY_OFF_F0, ARRAY_SIZE(SEQ_TEST_KEY_OFF_F0));
		mutex_unlock(&lcd->bl_lock);
		update_brightness(lcd, 1);
	}
#ifdef CONFIG_DECON_MIPI_DSI_PKTGO
	if (dsim_mpkt)
		dsim_reg_set_pkt_go_enable(true);
#endif

	return 0;
}

static void init_hmt_dynamic_aid(struct lcd_info *lcd)
{
	lcd->hmt_daid.vreg = VREG_OUT_X1000;
	lcd->hmt_daid.iv_tbl = index_voltage_table;
	lcd->hmt_daid.iv_max = IV_MAX;
	lcd->hmt_daid.mtp = lcd->daid.mtp;
	lcd->hmt_daid.gamma_default = gamma_default;
	lcd->hmt_daid.formular = gamma_formula;
	lcd->hmt_daid.vt_voltage_value = vt_voltage_value;

	lcd->hmt_daid.ibr_tbl = hmt_index_brightness_table;
	lcd->hmt_daid.ibr_max = HMT_IBRIGHTNESS_MAX;
	lcd->hmt_daid.gc_tbls = hmt_gamma_curve_tables;
	lcd->hmt_daid.gc_lut = gamma_curve_lut;

	lcd->hmt_daid.br_base = hmt_brightness_base_table;
	lcd->hmt_daid.offset_gra = hmt_offset_gradation;
	lcd->hmt_daid.offset_color = (const struct rgb_t(*)[])hmt_offset_color;
}

static int init_hmt_gamma_table(struct lcd_info *lcd , const u8 *mtp_data)
{
	int i, c, j, v;
	int ret = 0;
	int *pgamma;
	int **gamma;

	/* allocate memory for local gamma table */
	gamma = kzalloc(HMT_IBRIGHTNESS_MAX * sizeof(int *), GFP_KERNEL);
	if (!gamma) {
		pr_err("failed to allocate gamma table\n");
		ret = -ENOMEM;
		goto err_alloc_gamma_table;
	}

	for (i = 0; i < HMT_IBRIGHTNESS_MAX; i++) {
		gamma[i] = kzalloc(IV_MAX*CI_MAX * sizeof(int), GFP_KERNEL);
		if (!gamma[i]) {
			pr_err("failed to allocate gamma\n");
			ret = -ENOMEM;
			goto err_alloc_gamma;
		}
	}

	/* allocate memory for gamma table */
	lcd->hmt_gamma_table = kzalloc(HMT_IBRIGHTNESS_MAX * sizeof(u8 *), GFP_KERNEL);
	if (!lcd->hmt_gamma_table) {
		pr_err("failed to allocate gamma table 2\n");
		ret = -ENOMEM;
		goto err_alloc_gamma_table2;
	}

	for (i = 0; i < HMT_IBRIGHTNESS_MAX; i++) {
		lcd->hmt_gamma_table[i] = kzalloc(GAMMA_PARAM_SIZE * sizeof(u8), GFP_KERNEL);
		if (!lcd->hmt_gamma_table[i]) {
			pr_err("failed to allocate gamma 2\n");
			ret = -ENOMEM;
			goto err_alloc_gamma2;
		}
		lcd->hmt_gamma_table[i][0] = 0xCA;
	}

	/* calculate gamma table */
	/* init_mtp_data(lcd, mtp_data); */
	lcd->hmt_daid.mtp = lcd->daid.mtp;
	dynamic_aid(lcd->hmt_daid, gamma);

	/* relocate gamma order */
	for (i = 0; i < HMT_IBRIGHTNESS_MAX; i++) {
		/* Brightness table */
		v = IV_MAX - 1;
		pgamma = &gamma[i][v * CI_MAX];
		for (c = 0, j = 1; c < CI_MAX; c++, pgamma++) {
			if (*pgamma & 0x100)
				lcd->hmt_gamma_table[i][j++] = 1;
			else
				lcd->hmt_gamma_table[i][j++] = 0;

			lcd->hmt_gamma_table[i][j++] = *pgamma & 0xff;
		}

		for (v = IV_MAX - 2; v >= 0; v--) {
			pgamma = &gamma[i][v * CI_MAX];
			for (c = 0; c < CI_MAX; c++, pgamma++)
				lcd->hmt_gamma_table[i][j++] = *pgamma;
		}
		for (v = 0; v < GAMMA_PARAM_SIZE; v++)
			smtd_dbg("%d ", lcd->hmt_gamma_table[i][v]);
		smtd_dbg("\n");
	}
	hmt_set_selected_gamma(lcd, HMT_IBRIGHTNESS_150NT);
	/* free local gamma table */
	for (i = 0; i < HMT_IBRIGHTNESS_MAX; i++)
		kfree(gamma[i]);
	kfree(gamma);
	return 0;

err_alloc_gamma2:
	while (i > 0) {
		kfree(lcd->hmt_gamma_table[i-1]);
		i--;
	}
	kfree(lcd->hmt_gamma_table);
	i = HMT_IBRIGHTNESS_MAX;
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
static ssize_t hmt_brightness_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct lcd_info *lcd = dev_get_drvdata(dev);

	sprintf(buf, "%u\n", lcd->hmt_bl);

	return strlen(buf);
}

static ssize_t hmt_brightness_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t size)
{
	struct lcd_info *lcd = dev_get_drvdata(dev);
	int value;
	int rc;

	rc = kstrtoul(buf, (unsigned int)0, (unsigned long *)&value);
	if (rc < 0)
		return rc;
	else {
		if (!lcd->ldi_enable) {
			dev_info(dev, "%s: panel is off\n", __func__);
			return -EINVAL;
		}

		if (!lcd->hmt_on) {
			dev_info(dev, "%s: hmt is not on\n", __func__);
			return -EINVAL;
		}

		if (lcd->hmt_brightness != value) {
			mutex_lock(&lcd->bl_lock);
			lcd->hmt_brightness = value;
			mutex_unlock(&lcd->bl_lock);
			s6e3hf2_hmt_update(lcd, 0);
		}

		dev_info(dev, "%s: %d\n", __func__, value);
	}
	return size;
}

static ssize_t hmt_on_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct lcd_info *lcd = dev_get_drvdata(dev);

	sprintf(buf, "%u\n", lcd->hmt_on);

	return strlen(buf);
}

static ssize_t hmt_on_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t size)
{
	struct lcd_info *lcd = dev_get_drvdata(dev);
	int value;
	int rc;

	rc = kstrtoint(buf, 0, &value);
	if (rc < 0)
		return rc;

	if (!lcd->ldi_enable) {
		dev_info(dev, "%s: panel is off\n", __func__);
		return -EINVAL;
	}

	if (lcd->hmt_on != value) {
		dev_info(dev, "%s: %d\n", __func__, lcd->hmt_on);
		mutex_lock(&lcd->bl_lock);
		lcd->hmt_on = value;
		mutex_unlock(&lcd->bl_lock);
		s6e3hf2_hmt_update(lcd, 1);
	} else
		dev_info(dev, "%s: hmt already %s\n", __func__, value ? "single" : "off");

	return size;
}

static DEVICE_ATTR(hmt_bright, 0664, hmt_brightness_show, hmt_brightness_store);
static DEVICE_ATTR(hmt_on, 0664, hmt_on_show, hmt_on_store);
#endif


static DEVICE_ATTR(power_reduce, 0664, power_reduce_show, power_reduce_store);
static DEVICE_ATTR(lcd_type, 0444, lcd_type_show, NULL);
static DEVICE_ATTR(window_type, 0444, window_type_show, NULL);
static DEVICE_ATTR(manufacture_code, 0444, manufacture_code_show, NULL);
static DEVICE_ATTR(brightness_table, 0444, brightness_table_show, NULL);
static DEVICE_ATTR(siop_enable, 0664, siop_enable_show, siop_enable_store);
static DEVICE_ATTR(temperature, 0664, temperature_show, temperature_store);
static DEVICE_ATTR(color_coordinate, 0444, color_coordinate_show, NULL);
static DEVICE_ATTR(manufacture_date, 0444, manufacture_date_show, NULL);
static DEVICE_ATTR(partial_disp, 0664, partial_disp_show, partial_disp_store);
static DEVICE_ATTR(aid_log, 0444, aid_log_show, NULL);
static DEVICE_ATTR(lux, 0644, lux_show, lux_store);

static int s6e3hf2_probe(struct mipi_dsim_device *dsim)
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
 	lcd->ld = lcd_device_register("panel", dsim->dev, lcd, &s6e3hf2_lcd_ops);
	if (IS_ERR(lcd->ld)) {
		pr_err("failed to register lcd device\n");
		ret = PTR_ERR(lcd->ld);
		goto out_free_lcd;
	}
	dsim->lcd = lcd->ld;

	lcd->bd = backlight_device_register("panel", dsim->dev, lcd, &s6e3hf2_backlight_ops, NULL);
	if (IS_ERR(lcd->bd)) {
		pr_err("failed to register backlight device\n");
		ret = PTR_ERR(lcd->bd);
		goto out_free_backlight;
	}

	lcd->dev = dsim->dev;
	lcd->dsim = dsim;
	lcd->bd->props.max_brightness = EXTEND_BRIGHTNESS;
	lcd->bd->props.brightness = DEFAULT_BRIGHTNESS;
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
	lcd->width = dsim->lcd_info->xres;
	lcd->height = dsim->lcd_info->yres;
#ifdef CONFIG_LCD_ALPM
	lcd->alpm = 0;
	lcd->current_alpm = 0;
#endif
	lcd->lux = -1;

	lcd->mcd_on = 0;

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

	ret = device_create_file(&lcd->ld->dev, &dev_attr_partial_disp);
	if (ret < 0)
		dev_err(&lcd->ld->dev, "failed to add sysfs entries, %d\n", __LINE__);

	ret = device_create_file(&lcd->ld->dev, &dev_attr_aid_log);
	if (ret < 0)
		dev_err(&lcd->ld->dev, "failed to add sysfs entries, %d\n", __LINE__);

#ifdef CONFIG_LCD_ALPM
	ret = device_create_file(&lcd->ld->dev, &dev_attr_alpm);
	if (ret < 0)
		dev_err(&lcd->ld->dev, "failed to add sysfs entries, %d\n", __LINE__);
#endif

#ifdef CONFIG_LCD_HMT
	ret = device_create_file(&lcd->ld->dev, &dev_attr_hmt_bright);
	if (ret < 0)
		dev_err(&lcd->ld->dev, "failed to add sysfs entries, %d\n", __LINE__);

	ret = device_create_file(&lcd->ld->dev, &dev_attr_hmt_on);
	if (ret < 0)
		dev_err(&lcd->ld->dev, "failed to add sysfs entries, %d\n", __LINE__);
#endif

	ret = device_create_file(&lcd->ld->dev, &dev_attr_mcd_mode);
	if (ret < 0)
		dev_err(&lcd->ld->dev, "failed to add sysfs entries, %d\n", __LINE__);

	ret = device_create_file(&lcd->ld->dev, &dev_attr_lux);
	if (ret < 0)
		dev_err(&lcd->ld->dev, "failed to add sysfs entries, %d\n", __LINE__);

	mutex_init(&lcd->lock);
	mutex_init(&lcd->bl_lock);
	mutex_init(&lcd->alpm_lock);

	s6e3hf2_write(lcd, SEQ_TEST_KEY_ON_F0, ARRAY_SIZE(SEQ_TEST_KEY_ON_F0));
	s6e3hf2_read_id(lcd, lcd->id);
	s6e3hf2_read_code(lcd, lcd->code);
	s6e3hf2_read_coordinate(lcd);
	s6e3hf2_read_mtp(lcd, mtp_data);
	s6e3hf2_read_elvss(lcd, elvss_data);
	s6e3hf2_read_tset(lcd);
	s6e3hf2_read_vddmrvdd(lcd);
	s6e3hf2_read_mcd(lcd);

	s6e3hf2_write(lcd, SEQ_TEST_KEY_OFF_F0, ARRAY_SIZE(SEQ_TEST_KEY_OFF_F0));

	dev_info(&lcd->ld->dev, "ID: %x, %x, %x\n", lcd->id[0], lcd->id[1], lcd->id[2]);

	init_dynamic_aid(lcd);

	ret = init_gamma_table(lcd, mtp_data);
	ret += init_aid_dimming_table(lcd);
	ret += init_elvss_table(lcd, elvss_data);

	if (ret)
		dev_info(&lcd->ld->dev, "gamma table generation is failed\n");

#ifdef CONFIG_LCD_HMT
	init_hmt_dynamic_aid(lcd);

	ret = init_hmt_gamma_table(lcd, mtp_data);

	hmt_init(lcd);

	if (ret)
		dev_info(&lcd->ld->dev, "hmt gamma table generation is failed\n");
#endif


	show_lcd_table(lcd);

	lcd->ldi_enable = 1;

#if defined(CONFIG_DECON_MDNIE_LITE)
	mdnie_register(&lcd->ld->dev, lcd, (mdnie_w)s6e3hf2_send_seq, (mdnie_r)s6e3hf2_read, &tune_info);
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

static int s6e3hf2_displayon(struct mipi_dsim_device *dsim)
{
	struct lcd_info *lcd = dev_get_drvdata(&dsim->lcd->dev);

	s6e3hf2_power(lcd, FB_BLANK_UNBLANK);

	return 0;
}

static int s6e3hf2_suspend(struct mipi_dsim_device *dsim)
{
	struct lcd_info *lcd = dev_get_drvdata(&dsim->lcd->dev);

	s6e3hf2_power(lcd, FB_BLANK_POWERDOWN);

	return 0;
}

static int s6e3hf2_resume(struct mipi_dsim_device *dsim)
{
	return 0;
}

struct mipi_dsim_lcd_driver s6e3hf2_mipi_lcd_driver = {
	.probe		= s6e3hf2_probe,
	.displayon	= s6e3hf2_displayon,
	.suspend	= s6e3hf2_suspend,
	.resume		= s6e3hf2_resume,
};

static int s6e3hf2_init(void)
{
	return 0;
}

static void s6e3hf2_exit(void)
{
	return;
}

module_init(s6e3hf2_init);
module_exit(s6e3hf2_exit);

MODULE_DESCRIPTION("MIPI-DSI S6E3hf2 (1440*2560) Panel Driver");
MODULE_LICENSE("GPL");

