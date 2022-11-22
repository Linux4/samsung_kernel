/* drivers/video/decon_display/ea8064g_mipi_lcd.c
 *
 * Samsung SoC MIPI LCD driver.
 *
 * Copyright (c) 2013 Samsung Electronics
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
#include <linux/irq.h>
#include <linux/syscalls.h>

#include <video/mipi_display.h>
#include "../decon_display/decon_mipi_dsi.h"

#include "ea8064g_param.h"
#include "dynamic_aid_ea8064g.h"

#if defined(CONFIG_DECON_MDNIE_LITE)
#include "mdnie.h"
#include "mdnie_lite_table_s.h"
#endif

#define POWER_IS_ON(pwr)		(pwr <= FB_BLANK_NORMAL)
#define LEVEL_IS_HBM(brightness)	(brightness == EXTEND_BRIGHTNESS)

#define NORMAL_TEMPERATURE		25	/* 25 degree */

#define EXTEND_BRIGHTNESS		355
#define MIN_BRIGHTNESS			0
#define MAX_BRIGHTNESS			255
#define DEFAULT_BRIGHTNESS		134

#define MIN_GAMMA			2
#define MAX_GAMMA			350	/* actually 360 */

#define DEFAULT_GAMMA_INDEX		IBRIGHTNESS_183NT

#define LDI_ID_REG			0x04
#define LDI_ID_LEN			3
#define LDI_ID2_REG			0xD6
#define LDI_ID2_LEN			5
#define LDI_MTP_REG			0xC8
#define LDI_MTP_LEN			33
#define LDI_TSET_REG			0xB8
#define LDI_TSET_LEN			1

#define LDI_COORDINATE_REG		0xA1
#define LDI_COORDINATE_LEN		6	/* color coordinate + manufacture date */

#define CONFIG_ESD_FG

#ifdef SMART_DIMMING_DEBUG
#define smtd_dbg(format, arg...)	printk(format, ##arg)
#else
#define smtd_dbg(format, arg...)
#endif

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
		u16 reserved;
	};
};

struct lcd_info {
	unsigned int			bl;
	unsigned int			brightness;
	unsigned int			acl_enable;
	unsigned int			siop_enable;
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
	unsigned char			ddi_id[LDI_ID2_LEN];
	unsigned char			**gamma_table;
	unsigned char			**elvss_table[ACL_STATUS_MAX];
	struct dynamic_aid_param_t	daid;
	unsigned char			aor[IBRIGHTNESS_MAX][ARRAY_SIZE(SEQ_AOR_CONTROL)];
	unsigned int			connected;

	unsigned char			tset_table[TSET_PARAM_SIZE];
	int				temperature;
	unsigned int			coordinate[2];
	unsigned int			partial_range[2];
	unsigned char			date[2];

	int						lux;
	struct class			*mdnie_class;

#if defined(CONFIG_ESD_FG)
	unsigned int			esd_fg_irq;
	unsigned int			esd_fg_gpio;
	struct delayed_work		esd_fg;
	bool				esd_fg_state;
	unsigned int			esd_fg_count;
	spinlock_t			esd_fg_lock;
	struct pinctrl			*pins;
	struct pinctrl_state		*pins_state;
#endif
	struct mipi_dsim_device		*dsim;
};

#if defined(CONFIG_ESD_FG)
static int esd_fg_enable(struct lcd_info *lcd, int enable)
{
	struct device *dev = lcd->dsim->dev;
	int ret = 0;

	if (!lcd->connected)
		return ret;

	dev_info(dev, "%s: %s, gpio: %d, state: %d\n", __func__, enable ? "enable" : "disable",
		gpio_get_value(lcd->esd_fg_gpio), lcd->esd_fg_state);

	if (!lcd->esd_fg_irq)
		return ret;

	if (enable)
		enable_irq(lcd->esd_fg_irq);
	else {
		if (!lcd->esd_fg_state)
			disable_irq(lcd->esd_fg_irq);
		if (!IS_ERR(lcd->pins_state)) {
			ret = pinctrl_select_state(lcd->pins, lcd->pins_state);
			if (ret) {
				dev_err(dev, "%s: failed to select state\n", __func__);
				return ret;
			}
		}
	}

	return ret;
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

	if (!esd_fg_level && (!lcd->esd_fg_state)) {
		disable_irq_nosync(lcd->esd_fg_irq);
		schedule_delayed_work(&lcd->esd_fg, 0);
	}

	return IRQ_HANDLED;
}

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

	lcd->pins = devm_pinctrl_get(dev);
	if (!lcd->pins) {
		dev_err(&lcd->ld->dev, "failed to get pinctrl\n");
		return -EINVAL;
	}

	lcd->pins_state = pinctrl_lookup_state(lcd->pins, "errfg_off");
	if (!lcd->pins_state) {
		dev_err(&lcd->ld->dev, "failed to get proper errfg off pinctrl\n");
		return -EINVAL;
	}

	if (!gpio_get_value(lcd->esd_fg_gpio)) {
		dev_info(dev, "%s: gpio(%d) is already low\n", __func__, lcd->esd_fg_gpio);
		return ret;
	}

	lcd->esd_fg_irq = gpio_to_irq(lcd->esd_fg_gpio);
	if (!lcd->esd_fg_irq) {
		dev_err(&lcd->ld->dev, "failed to get proper irq number\n");
		return -EINVAL;
	}

	spin_lock_init(&lcd->esd_fg_lock);

	INIT_DELAYED_WORK(&lcd->esd_fg, esd_fg_work);

	ret = request_irq(lcd->esd_fg_irq, esd_fg_isr, IRQF_TRIGGER_FALLING, "esd_fg", lcd);
	if (ret) {
		dev_err(&lcd->ld->dev, "esd_fg irq request failed\n");
		return ret;
	}

	dev_info(dev, "%s: esd_fg(%d)\n", __func__, gpio_get_value(lcd->esd_fg_gpio));

	return ret;
}
#endif

int ea8064g_write(struct lcd_info *lcd, const u8 *seq, u32 len)
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

int ea8064g_read(struct lcd_info *lcd, u8 addr, u8 *buf, u32 len)
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
		dev_err(&lcd->ld->dev, "mipi_read failed retry ..\n");
		retry--;
		goto read_data;
	}

read_err:
	mutex_unlock(&lcd->lock);
	return ret;
}

#if defined(CONFIG_LCD_ALPM) || defined(CONFIG_DECON_MDNIE_LITE)
static int ea8064g_write_set(struct lcd_info *lcd, struct lcd_seq_info *seq, u32 num)
{
	int ret = 0, i;

	for (i = 0; i < num; i++) {
		if (seq[i].cmd) {
			ret = ea8064g_write(lcd, seq[i].cmd, seq[i].len);
			if (ret != 0) {
				dev_info(&lcd->ld->dev, "%s failed.\n", __func__);
				return ret;
			}
		}
		if (seq[i].sleep)
			msleep(seq[i].sleep);
	}
	return ret;
}
#endif

#if defined(CONFIG_DECON_MDNIE_LITE)
static int ea8064g_send_seq(struct lcd_info *lcd, struct lcd_seq_info *seq, u32 num)
{
	int ret = 0;

	mutex_lock(&lcd->bl_lock);
	ret = ea8064g_write_set(lcd, seq, num);
	mutex_unlock(&lcd->bl_lock);

	return ret;
}
#endif

static int ea8064g_read_coordinate(struct lcd_info *lcd)
{
	int ret;
	unsigned char buf[LDI_COORDINATE_LEN] = {0,};

	ret = ea8064g_read(lcd, LDI_COORDINATE_REG, buf, LDI_COORDINATE_LEN);

	if (ret < 1)
		dev_err(&lcd->ld->dev, "%s failed\n", __func__);

	lcd->coordinate[0] = buf[0] << 8 | buf[1];	/* X */
	lcd->coordinate[1] = buf[2] << 8 | buf[3];	/* Y */

	/* manufacture date */
	lcd->date[0] = buf[4];
	lcd->date[1] = buf[5];

	return ret;
}

static int ea8064g_read_id(struct lcd_info *lcd, u8 *buf)
{
	int ret;

	ret = ea8064g_read(lcd, LDI_ID_REG, buf, LDI_ID_LEN);

	if (ret < 1) {
		lcd->connected = 0;
		dev_info(&lcd->ld->dev, "panel is not connected well\n");
	}

	return ret;
}

static int ea8064g_read_ddi_id(struct lcd_info *lcd, u8 *buf)
{
	int ret;

	ret = ea8064g_read(lcd, LDI_ID2_REG, buf, LDI_ID2_LEN);

	if (ret < 1)
		dev_info(&lcd->ld->dev, "%s failed\n", __func__);

	return ret;
}

static int ea8064g_read_mtp(struct lcd_info *lcd, u8 *buf)
{
	int ret, i;

	ret = ea8064g_read(lcd, LDI_MTP_REG, buf, LDI_MTP_LEN);

	if (ret < 1)
		dev_err(&lcd->ld->dev, "%s failed\n", __func__);

	smtd_dbg("%s: %02xh\n", __func__, LDI_MTP_REG);
	for (i = 0; i < LDI_MTP_LEN; i++)
		smtd_dbg("%02dth value is %02x\n", i+1, (int)buf[i]);

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

static int ea8064g_gamma_ctl(struct lcd_info *lcd)
{
	int ret = 0;

	ret = ea8064g_write(lcd, lcd->gamma_table[lcd->bl], GAMMA_PARAM_SIZE);
	if (!ret)
		ret = -EPERM;

	return ret;
}

static int ea8064g_aid_parameter_ctl(struct lcd_info *lcd, u8 force)
{
	int ret = 0;

	if (force)
		goto aid_update;
	else if (lcd->aor[lcd->bl][3] !=  lcd->aor[lcd->current_bl][3])
		goto aid_update;
	else if (lcd->aor[lcd->bl][4] !=  lcd->aor[lcd->current_bl][4])
		goto aid_update;
	else
		goto exit;

aid_update:
	ret = ea8064g_write(lcd, lcd->aor[lcd->bl], AID_PARAM_SIZE);
	if (!ret)
		ret = -EPERM;

exit:
	return ret;
}

static int ea8064g_set_acl(struct lcd_info *lcd, u8 force)
{
	int ret = 0, level = ACL_STATUS_15P;

	if (lcd->siop_enable || LEVEL_IS_HBM(lcd->brightness))
		goto acl_update;

	if (!lcd->acl_enable)
		level = ACL_STATUS_0P;

acl_update:
	if (force || lcd->current_acl != ACL_CUTOFF_TABLE[level][1]) {
		ret = ea8064g_write(lcd, ACL_OPR_TABLE[level], OPR_PARAM_SIZE);
		ret += ea8064g_write(lcd, ACL_CUTOFF_TABLE[level], ACL_PARAM_SIZE);
		lcd->current_acl = ACL_CUTOFF_TABLE[level][1];
		dev_info(&lcd->ld->dev, "acl: %d, brightness: %d\n", lcd->current_acl, lcd->brightness);
	}

	if (!ret)
		ret = -EPERM;

	return ret;
}

static int ea8064g_set_elvss(struct lcd_info *lcd, u8 force)
{
	int ret = 0, i, elvss_level;
	u32 nit;
	union elvss_info elvss;

	nit = DIM_TABLE[lcd->bl];
	elvss_level = ELVSS_STATUS_360;
	for (i = 0; i < ELVSS_STATUS_MAX; i++) {
		if (nit <= ELVSS_DIM_TABLE[i]) {
			elvss_level = i;
			break;
		}
	}

	elvss.mps = lcd->elvss_table[lcd->acl_enable][elvss_level][1];
	elvss.offset = lcd->elvss_table[lcd->acl_enable][elvss_level][2];

	if (force)
		goto elvss_update;
	else if (lcd->current_elvss.value != elvss.value)
		goto elvss_update;
	else
		goto exit;

elvss_update:
	ret = ea8064g_write(lcd, lcd->elvss_table[lcd->acl_enable][elvss_level], ELVSS_PARAM_SIZE);
	dev_info(&lcd->ld->dev, "acl: %d, elvss: %d, %x\n", lcd->acl_enable, elvss_level, elvss.value);
	lcd->current_elvss.value = elvss.value;

	if (!ret)
		ret = -EPERM;

exit:
	return ret;
}

static int ea8064g_set_tset(struct lcd_info *lcd, u8 force)
{
	int ret = 0;
	u8 tset;

	tset = ((lcd->temperature > 0) ? 0 : BIT(7)) | abs(lcd->temperature);

	if (force || lcd->tset_table[LDI_TSET_LEN] != tset) {
		lcd->tset_table[LDI_TSET_LEN] = tset;
		ret = ea8064g_write(lcd, lcd->tset_table, TSET_PARAM_SIZE);
		dev_info(&lcd->ld->dev, "temperature: %d, tset: %d\n", lcd->temperature, tset);
	}

	if (!ret)
		ret = -EPERM;

	return ret;
}

static int ea8064g_set_hbm(struct lcd_info *lcd, u8 force)
{
	int ret = 0, level = LEVEL_IS_HBM(lcd->brightness);

	if (force || lcd->current_hbm != HBM_TABLE[level][1]) {
		ret = ea8064g_write(lcd, HBM_TABLE[level], HBM_PARAM_SIZE);
		lcd->current_hbm = HBM_TABLE[level][1];
		dev_info(&lcd->ld->dev, "hbm: %d, brightness: %d\n", lcd->current_hbm, lcd->brightness);
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

static void init_mtp_data(struct lcd_info *lcd, const u8 *mtp_data)
{
	int i, c, j;

	int *mtp;

	mtp = lcd->daid.mtp;

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

static int init_gamma_table(struct lcd_info *lcd , const u8 *mtp_data)
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
		lcd->aor[i][3] = aor_cmd[i][0];
		lcd->aor[i][4] = aor_cmd[i][1];
	}

	return 0;
}

static int init_elvss_table(struct lcd_info *lcd)
{
	int i, acl, ret;

	for (acl = 0; acl < ACL_STATUS_MAX; acl++) {
		lcd->elvss_table[acl] = kzalloc(ELVSS_STATUS_MAX * sizeof(u8 *), GFP_KERNEL);

		if (IS_ERR_OR_NULL(lcd->elvss_table[acl])) {
			pr_err("failed to allocate elvss table\n");
			ret = -ENOMEM;
			goto err_alloc_elvss_table;
		}

		for (i = 0; i < ELVSS_STATUS_MAX; i++) {
			lcd->elvss_table[acl][i] = kzalloc(ELVSS_PARAM_SIZE * sizeof(u8), GFP_KERNEL);
			if (IS_ERR_OR_NULL(lcd->elvss_table[acl][i])) {
				pr_err("failed to allocate elvss\n");
				ret = -ENOMEM;
				goto err_alloc_elvss;
			}

			lcd->elvss_table[acl][i][0] = SEQ_ELVSS_SET[0];
			lcd->elvss_table[acl][i][1] = MPS_TABLE[acl];
			lcd->elvss_table[acl][i][2] = ELVSS_TABLE[acl][i];
		}
	}

	return 0;

err_alloc_elvss:
	while (i > 0)
		kfree(lcd->elvss_table[acl][--i]);

err_alloc_elvss_table:
	kfree(lcd->elvss_table[acl]);

	return ret;
}

static int init_tset_table(struct lcd_info *lcd)
{
	memcpy(lcd->tset_table, SEQ_TSET, ARRAY_SIZE(SEQ_TSET));

	return 0;
}

static void show_lcd_table(struct lcd_info *lcd)
{
	int i, j, acl;

	for (i = 0; i < IBRIGHTNESS_MAX; i++) {
		smtd_dbg("%03d: ", index_brightness_table[i]);
		for (j = 0; j < GAMMA_PARAM_SIZE; j++)
			smtd_dbg("%02X ", lcd->gamma_table[i][j]);
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


	for (acl = 0; acl < ACL_STATUS_MAX; acl++) {
		smtd_dbg("acl: %d\n", acl);
		for (i = 0; i < ELVSS_STATUS_MAX; i++) {
			smtd_dbg("%03d: ", ELVSS_DIM_TABLE[i]);
			for (j = 0; j < ELVSS_PARAM_SIZE; j++)
				smtd_dbg("%02X, ", lcd->elvss_table[acl][i][j]);
			smtd_dbg("\n");
		}
		smtd_dbg("\n");
	}
}

static int update_brightness(struct lcd_info *lcd, u8 force)
{
	mutex_lock(&lcd->bl_lock);

	lcd->brightness = lcd->bd->props.brightness;

	lcd->bl = get_backlight_level_from_brightness(lcd->brightness > MAX_BRIGHTNESS ? MAX_BRIGHTNESS : lcd->brightness);

	if (LEVEL_IS_HBM(lcd->brightness))
		lcd->bl = IBRIGHTNESS_500NT;

	if (force || (lcd->ldi_enable && (lcd->current_bl != lcd->bl))) {
		ea8064g_write(lcd, SEQ_TEST_KEY_ON_F0, ARRAY_SIZE(SEQ_TEST_KEY_ON_F0));
		ea8064g_write(lcd, SEQ_TEST_KEY_ON_F1, ARRAY_SIZE(SEQ_TEST_KEY_ON_F1));
		ea8064g_gamma_ctl(lcd);
		ea8064g_aid_parameter_ctl(lcd, force);
		ea8064g_set_elvss(lcd, force);
		ea8064g_write(lcd, SEQ_GAMMA_UPDATE, ARRAY_SIZE(SEQ_GAMMA_UPDATE));
		ea8064g_set_acl(lcd, force);
		ea8064g_set_tset(lcd, force);
		ea8064g_set_hbm(lcd, force);
		ea8064g_write(lcd, SEQ_TEST_KEY_OFF_F0, ARRAY_SIZE(SEQ_TEST_KEY_OFF_F0));
		ea8064g_write(lcd, SEQ_TEST_KEY_OFF_F1, ARRAY_SIZE(SEQ_TEST_KEY_OFF_F1));

		lcd->current_bl = lcd->bl;

		dev_info(&lcd->ld->dev, "brightness=%d, bl=%d, candela=%d\n",
			lcd->brightness, lcd->bl, index_brightness_table[lcd->bl]);
	}

	mutex_unlock(&lcd->bl_lock);

	return 0;
}

#if defined(CONFIG_LCD_PCD)
static int ea8064g_get_pcd(struct lcd_info *lcd)
{
	struct device *dev = lcd->dev;
	int gpio, pcd;

	gpio = of_get_named_gpio(dev->of_node, "oled-pcd-gpio", 0);
	if (gpio < 0) {
		dev_err(dev, "failed to get proper gpio number\n");
		return -EINVAL;
	}

	pcd = gpio_get_value(gpio);

	dev_info(dev, "%s : pcd %d\n", __func__, pcd);

	return pcd;
}
#endif

static int ea8064g_ldi_init(struct lcd_info *lcd)
{
	int ret = 0;

	lcd->connected = 1;

#if defined(CONFIG_LCD_PCD)
	ea8064g_get_pcd(lcd);
#endif
	ea8064g_read_id(lcd, lcd->id);

	/* 8. Brightness Setting */
	/* 9. ELVSS Temp Compensation */
	update_brightness(lcd, 1);

	/* We should re-enable test key because update_brightness function disable it */
	ea8064g_write(lcd, SEQ_TEST_KEY_ON_F0, ARRAY_SIZE(SEQ_TEST_KEY_ON_F0));
	ea8064g_write(lcd, SEQ_TEST_KEY_ON_F1, ARRAY_SIZE(SEQ_TEST_KEY_ON_F1));
	ea8064g_write(lcd, SEQ_TEST_KEY_ON_FC, ARRAY_SIZE(SEQ_TEST_KEY_ON_FC));

	/* 10. Sleep Out Command (11h) */
	ea8064g_write(lcd, SEQ_SLEEP_OUT, ARRAY_SIZE(SEQ_SLEEP_OUT));

	/* 11. Wait 20ms */
	msleep(20);

	/* ESD: with FC (including F1) unlock */
	ea8064g_write(lcd, SEQ_ESD_FG_ENABLE, ARRAY_SIZE(SEQ_ESD_FG_ENABLE));
	ea8064g_write(lcd, SEQ_GPARA_ESD_FG_POLARITY, ARRAY_SIZE(SEQ_GPARA_ESD_FG_POLARITY));
	ea8064g_write(lcd, SEQ_ESD_FG_POLARITY, ARRAY_SIZE(SEQ_ESD_FG_POLARITY));

	/* 12. Common Setting */
	/* 12.1 ETC & TE OUT (Vsync) */
	/* dsi_write(SEQ_TSET, ARRAY_SIZE(SEQ_TSET)); */
	ea8064g_write(lcd, SEQ_SOURCE_CONTROL, ARRAY_SIZE(SEQ_SOURCE_CONTROL));
	ea8064g_write(lcd, SEQ_TE_ON, ARRAY_SIZE(SEQ_TE_ON));
	ea8064g_write(lcd, SEQ_COLUMN_ADDRESS, ARRAY_SIZE(SEQ_COLUMN_ADDRESS));
	ea8064g_write(lcd, SEQ_PAGE_ADDRESS, ARRAY_SIZE(SEQ_PAGE_ADDRESS));

	/* 12.2 Touch Hsync */
	ea8064g_write(lcd, SEQ_TOUCH_HSYNC, ARRAY_SIZE(SEQ_TOUCH_HSYNC));
#if defined(CONFIG_LCD_PCD)
	ea8064g_write(lcd, SEQ_PCD_SET_DET_LOW, ARRAY_SIZE(SEQ_PCD_SET_DET_LOW));
#endif

	ea8064g_write(lcd, SEQ_TEST_KEY_OFF_F0, ARRAY_SIZE(SEQ_TEST_KEY_OFF_F0));
	ea8064g_write(lcd, SEQ_TEST_KEY_OFF_F1, ARRAY_SIZE(SEQ_TEST_KEY_OFF_F1));
	ea8064g_write(lcd, SEQ_TEST_KEY_OFF_FC, ARRAY_SIZE(SEQ_TEST_KEY_OFF_FC));

	/* 13. Wait 100ms */
	msleep(100);

	return ret;
}

static int ea8064g_ldi_enable(struct lcd_info *lcd)
{
	int ret = 0;

	ea8064g_write(lcd, SEQ_DISPLAY_ON, ARRAY_SIZE(SEQ_DISPLAY_ON));

	dev_info(&lcd->ld->dev, "DISPLAY_ON\n");

	return ret;
}

static int ea8064g_ldi_disable(struct lcd_info *lcd)
{
	int ret = 0;

	dev_info(&lcd->ld->dev, "+ %s\n", __func__);

	/* 2. Display Off (28h) */
	ea8064g_write(lcd, SEQ_DISPLAY_OFF, ARRAY_SIZE(SEQ_DISPLAY_OFF));

	dev_info(&lcd->ld->dev, "DISPLAY_OFF\n");

	/* 3. Wait 35ms */
	msleep(35);

	/* 4. Sleep In (10h) */
	ea8064g_write(lcd, SEQ_SLEEP_IN, ARRAY_SIZE(SEQ_SLEEP_IN));

	/* 5. Wait 120ms */
	msleep(120);

	dev_info(&lcd->ld->dev, "- %s\n", __func__);

	return ret;
}

static int ea8064g_power_on(struct lcd_info *lcd)
{
	int ret = 0;

	dev_info(&lcd->ld->dev, "+ %s\n", __func__);

	ret = ea8064g_ldi_init(lcd);
	if (ret) {
		dev_err(&lcd->ld->dev, "failed to initialize ldi.\n");
		goto err;
	}

	ret = ea8064g_ldi_enable(lcd);
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

static int ea8064g_power_off(struct lcd_info *lcd)
{
	int ret = 0;

	dev_info(&lcd->ld->dev, "+ %s\n", __func__);

#if defined(CONFIG_ESD_FG)
	esd_fg_enable(lcd, 0);
#endif

	mutex_lock(&lcd->bl_lock);
	lcd->ldi_enable = 0;
	mutex_unlock(&lcd->bl_lock);

	ret = ea8064g_ldi_disable(lcd);

	dev_info(&lcd->ld->dev, "- %s\n", __func__);

	return ret;
}

static int ea8064g_power(struct lcd_info *lcd, int power)
{
	int ret = 0;

	if (POWER_IS_ON(power) && !POWER_IS_ON(lcd->power))
		ret = ea8064g_power_on(lcd);
	else if (!POWER_IS_ON(power) && POWER_IS_ON(lcd->power))
		ret = ea8064g_power_off(lcd);

	if (!ret)
		lcd->power = power;

	return ret;
}

static int ea8064g_set_power(struct lcd_device *ld, int power)
{
	struct lcd_info *lcd = lcd_get_data(ld);

	if (power != FB_BLANK_UNBLANK && power != FB_BLANK_POWERDOWN &&
		power != FB_BLANK_NORMAL) {
		dev_err(&lcd->ld->dev, "power value should be 0, 1 or 4.\n");
		return -EINVAL;
	}

	return ea8064g_power(lcd, power);
}

static int ea8064g_get_power(struct lcd_device *ld)
{
	struct lcd_info *lcd = lcd_get_data(ld);

	return lcd->power;
}

static int ea8064g_check_fb(struct lcd_device *ld, struct fb_info *fb)
{
	return 0;
}

static int ea8064g_get_brightness(struct backlight_device *bd)
{
	struct lcd_info *lcd = bl_get_data(bd);

	return DIM_TABLE[lcd->bl];
}

static int ea8064g_set_brightness(struct backlight_device *bd)
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

static struct lcd_ops ea8064g_lcd_ops = {
	.set_power = ea8064g_set_power,
	.get_power = ea8064g_get_power,
	.check_fb  = ea8064g_check_fb,
};

static const struct backlight_ops ea8064g_backlight_ops = {
	.get_brightness = ea8064g_get_brightness,
	.update_status = ea8064g_set_brightness,
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
			if (lcd->ldi_enable)
				update_brightness(lcd, 1);
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

	sprintf(buf, "%02X %02X %02X\n", lcd->id[0], lcd->id[1], lcd->id[2]);

	return strlen(buf);
}

static ssize_t ddi_id_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct lcd_info *lcd = dev_get_drvdata(dev);

	sprintf(buf, "ddi id : %02X %02X %02X %02X %02X\n",
		lcd->ddi_id[0], lcd->ddi_id[1], lcd->ddi_id[2], lcd->ddi_id[3], lcd->ddi_id[4]);

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
	u8 month;

	year = ((lcd->date[0] & 0xF0) >> 4) + 2011;
	month = lcd->date[0] & 0xF;

	sprintf(buf, "%d, %d, %d\n", year, month, lcd->date[1]);

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


static DEVICE_ATTR(power_reduce, 0664, power_reduce_show, power_reduce_store);
static DEVICE_ATTR(lcd_type, 0444, lcd_type_show, NULL);
static DEVICE_ATTR(window_type, 0444, window_type_show, NULL);
static DEVICE_ATTR(ddi_id, 0444, ddi_id_show, NULL);
static DEVICE_ATTR(brightness_table, 0444, brightness_table_show, NULL);
static DEVICE_ATTR(siop_enable, 0664, siop_enable_show, siop_enable_store);
static DEVICE_ATTR(temperature, 0664, temperature_show, temperature_store);
static DEVICE_ATTR(color_coordinate, 0444, color_coordinate_show, NULL);
static DEVICE_ATTR(manufacture_date, 0444, manufacture_date_show, NULL);
static DEVICE_ATTR(lux, 0644, lux_show, lux_store);

static int ea8064g_probe(struct mipi_dsim_device *dsim)
{
	int ret;
	struct lcd_info *lcd;

	u8 mtp_data[LDI_MTP_LEN] = {0,};

	lcd = kzalloc(sizeof(struct lcd_info), GFP_KERNEL);
	if (!lcd) {
		pr_err("failed to allocate for lcd\n");
		ret = -ENOMEM;
		goto err_alloc;
	}

	lcd->ld = lcd_device_register("panel", dsim->dev, lcd, &ea8064g_lcd_ops);
	if (IS_ERR(lcd->ld)) {
		pr_err("failed to register lcd device\n");
		ret = PTR_ERR(lcd->ld);
		goto out_free_lcd;
	}
	dsim->lcd = lcd->ld;

	lcd->bd = backlight_device_register("panel", dsim->dev, lcd, &ea8064g_backlight_ops, NULL);
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

	ret = device_create_file(&lcd->ld->dev, &dev_attr_ddi_id);
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

	ret = device_create_file(&lcd->ld->dev, &dev_attr_lux);
	if (ret < 0)
		dev_err(&lcd->ld->dev, "failed to add sysfs entries, %d\n", __LINE__);

	mutex_init(&lcd->lock);
	mutex_init(&lcd->bl_lock);

	ea8064g_write(lcd, SEQ_TEST_KEY_ON_F0, ARRAY_SIZE(SEQ_TEST_KEY_ON_F0));
	ea8064g_read_id(lcd, lcd->id);
	ea8064g_read_ddi_id(lcd, lcd->ddi_id);
	ea8064g_read_coordinate(lcd);
	ea8064g_read_mtp(lcd, mtp_data);
	ea8064g_write(lcd, SEQ_TEST_KEY_OFF_F0, ARRAY_SIZE(SEQ_TEST_KEY_OFF_F0));

	dev_info(&lcd->ld->dev, "ID: %x, %x, %x\n", lcd->id[0], lcd->id[1], lcd->id[2]);

	init_dynamic_aid(lcd);

	ret = init_gamma_table(lcd, mtp_data);
	ret += init_aid_dimming_table(lcd);
	ret += init_elvss_table(lcd);
	ret += init_tset_table(lcd);

	if (ret)
		dev_info(&lcd->ld->dev, "gamma table generation is failed\n");

	show_lcd_table(lcd);

	lcd->ldi_enable = 1;

#if defined(CONFIG_DECON_MDNIE_LITE)
	mdnie_register(&lcd->ld->dev, lcd, (mdnie_w)ea8064g_send_seq, (mdnie_r)ea8064g_read, &tune_info);
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

static int ea8064g_displayon(struct mipi_dsim_device *dsim)
{
	struct lcd_info *lcd = dev_get_drvdata(&dsim->lcd->dev);

	ea8064g_power(lcd, FB_BLANK_UNBLANK);

	return 0;
}

static int ea8064g_suspend(struct mipi_dsim_device *dsim)
{
	struct lcd_info *lcd = dev_get_drvdata(&dsim->lcd->dev);

	ea8064g_power(lcd, FB_BLANK_POWERDOWN);

	return 0;
}

static int ea8064g_resume(struct mipi_dsim_device *dsim)
{
	return 0;
}

struct mipi_dsim_lcd_driver ea8064g_mipi_lcd_driver = {
	.probe		= ea8064g_probe,
	.displayon	= ea8064g_displayon,
	.suspend	= ea8064g_suspend,
	.resume		= ea8064g_resume,
};

static int ea8064g_init(void)
{
	return 0;
}

static void ea8064g_exit(void)
{
	return;
}

module_init(ea8064g_init);
module_exit(ea8064g_exit);

MODULE_DESCRIPTION("MIPI-DSI EA8064G (720*1280) Panel Driver");
MODULE_LICENSE("GPL");

