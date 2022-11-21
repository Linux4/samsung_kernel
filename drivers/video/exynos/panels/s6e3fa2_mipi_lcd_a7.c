/* drivers/video/decon_display/s6e3fa2_mipi_lcd.c
 *
 * Samsung SoC MIPI LCD driver.
 *
 * Copyright (c) 2013 Samsung Electronics
 *
 * Haowei Li, <haowei.li@samsung.com>
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
#include <linux/of.h>

#include <video/mipi_display.h>
#include "../decon_display/decon_mipi_dsi.h"

#include "s6e3fa2_param_a7.h"
#include "dynamic_aid_s6e3fa2_a7.h"
#include <linux/syscalls.h>

#if defined(CONFIG_DECON_MDNIE_LITE)
#include "mdnie.h"
#include "mdnie_lite_table_a7.h"
#endif

#include <linux/completion.h>
#include <linux/gpio.h>

/*#define PRINT_CMD*/
#define POWER_IS_ON(pwr)		(pwr <= FB_BLANK_NORMAL)
#define LEVEL_IS_HBM(brightness)	(brightness == EXTEND_BRIGHTNESS)

#define NORMAL_TEMPERATURE	25	/* 25 C */

#define EXTEND_BRIGHTNESS		355
#define MIN_BRIGHTNESS		0
#define MAX_BRIGHTNESS		255
#define DEFAULT_BRIGHTNESS		133

#define MIN_GAMMA			2
#define MAX_GAMMA			360
#define DEFAULT_GAMMA			183
#define DEFAULT_GAMMA_INDEX		GAMMA_183CD

#define LDI_ID_REG			0x04
#define LDI_ID_LEN			3
#define LDI_ID2_REG			0xD6
#define LDI_ID2_LEN			5
#define LDI_MTP_REG			0xC8
#define LDI_MTP_LEN			87	/* MTP */
#define LDI_ELVSS_REG			0xB6
#define LDI_ELVSS_LEN			21
#define LDI_TSET_REG			0xB8
#define LDI_TSET_LEN			6

#define LDI_COORDINATE_REG		0xA1
#define LDI_COORDINATE_LEN		4

#ifdef SMART_DIMMING_DEBUG
#define smtd_dbg(format, arg...)	printk(format, ##arg)
#else
#define smtd_dbg(format, arg...)
#endif

union elvss_info {
	u32 value;
	struct {
		u8 mps;
		u8 offset;
		u8 hbm;
		u8 reserved;
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
	unsigned int			current_psre;
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
	unsigned char			elvss[ELVSS_PARAM_SIZE];
	unsigned char			elvss_hbm[2];
	struct dynamic_aid_param_t daid;
	unsigned char			aor[GAMMA_MAX][ARRAY_SIZE(SEQ_AOR_CONTROL)];
	unsigned int			connected;
	unsigned char			tset_table[LDI_TSET_LEN + 1];
	int				temperature;
	int				current_temp;
	unsigned int			coordinate[2];
	unsigned int			partial_range[2];
	unsigned char			date[2];

	int						lux;
	struct class			*mdnie_class;

	unsigned int			width;
	unsigned int			height;

	unsigned int			need_update;

#if defined(CONFIG_LCD_PCD)
	unsigned int			err_fg_irq;
	unsigned int			err_fg_gpio;
	struct delayed_work		err_fg;
	bool				err_fg_state;
	unsigned int			err_fg_count;
	spinlock_t			err_fg_lock;
#endif

	struct mipi_dsim_device		*dsim;
};

static int s6e3fa2_write(struct lcd_info *lcd, const u8 *seq, u32 len)
{
	int ret = 0;
	int retry;
#if defined(PRINT_CMD)
	int i=0;
#endif

	if (!lcd->connected)
		return -EINVAL;

	mutex_lock(&lcd->lock);

	retry = 3;
write_data:
	if (!retry) {
		dev_err(&lcd->ld->dev, "%s failed: exceed retry count\n", __func__);
		goto write_err;
	}
#if defined(PRINT_CMD)
	printk("\n %s:: REG[0x%02x] ",__func__, seq[0]);

	for(i=1;i<len;i++)
		printk("0x%02x ", seq[i]);
	printk("\n");
#endif
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

static int s6e3fa2_read(struct lcd_info *lcd, u8 addr, u8 *buf, u32 len)
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

static int s6e3fa2_write_set(struct lcd_info *lcd, struct lcd_seq_info *seq, u32 num)
{
	int ret = 0, i;

	for (i = 0; i < num ; i++) {
		if (seq[i].cmd) {
			ret = s6e3fa2_write(lcd, seq[i].cmd, seq[i].len);
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

#if defined(CONFIG_DECON_MDNIE_LITE)
static int s6e3fa2_send_seq(struct lcd_info *lcd, struct lcd_seq_info *seq, u32 num)
{
	int ret = 0;

	mutex_lock(&lcd->bl_lock);
	ret = s6e3fa2_write_set(lcd, seq, num);
	mutex_unlock(&lcd->bl_lock);

	return ret;
}
#endif

static void s6e3fa2_read_coordinate(struct lcd_info *lcd)
{
	int ret;
	unsigned char buf[LDI_COORDINATE_LEN] = {0,};

	ret = s6e3fa2_read(lcd, LDI_COORDINATE_REG, buf, LDI_COORDINATE_LEN);

	if (ret < 1)
		dev_err(&lcd->ld->dev, "%s failed\n", __func__);

	lcd->coordinate[0] = buf[0] << 8 | buf[1];	/* X */
	lcd->coordinate[1] = buf[2] << 8 | buf[3];	/* Y */
}

static void s6e3fa2_read_id(struct lcd_info *lcd, u8 *buf)
{
	int ret;

	ret = s6e3fa2_read(lcd, LDI_ID_REG, buf, LDI_ID_LEN);
	if (ret < 1) {
		lcd->connected = 0;
		dev_info(&lcd->ld->dev, "panel is not connected well\n");
	}
}

static void s6e3fa2_read_ddi_id(struct lcd_info *lcd, u8 *buf)
{
	int ret;

	ret = s6e3fa2_read(lcd, LDI_ID2_REG, buf, LDI_ID2_LEN);

	if (ret < 1)
		dev_info(&lcd->ld->dev, "%s failed\n", __func__);
}

static int s6e3fa2_read_mtp(struct lcd_info *lcd, u8 *buf)
{
	int ret, i;

	ret = s6e3fa2_read(lcd, LDI_MTP_REG, buf, LDI_MTP_LEN);

	if (ret < 1)
		dev_err(&lcd->ld->dev, "%s failed\n", __func__);

	/* manufacture date */
	lcd->date[0] = buf[40];
	lcd->date[1] = buf[41];

	smtd_dbg("%s: %02xh\n", __func__, LDI_MTP_REG);
	for (i = 0; i < LDI_MTP_LEN; i++)
		smtd_dbg("%02dth value is %02x\n", i+1, (int)buf[i]);

	return ret;
}

static int s6e3fa2_read_elvss(struct lcd_info *lcd, u8 *buf)
{
	int ret, i;

	ret = s6e3fa2_read(lcd, LDI_ELVSS_REG, buf, LDI_ELVSS_LEN);

	smtd_dbg("%s: %02xh\n", __func__, LDI_ELVSS_REG);
	for (i = 0; i < LDI_ELVSS_LEN; i++)
		smtd_dbg("%02dth value is %02x\n", i+1, (int)buf[i]);

	return ret;
}

static int s6e3fa2_read_tset(struct lcd_info *lcd)
{
	int ret, i;

	lcd->tset_table[0] = LDI_TSET_REG;

	ret = s6e3fa2_read(lcd, LDI_TSET_REG, &lcd->tset_table[1], LDI_TSET_LEN);

	smtd_dbg("%s: %02xh\n", __func__, LDI_TSET_REG);
	for (i = 1; i <= LDI_TSET_LEN; i++)
		smtd_dbg("%02dth value is %02x\n", i, lcd->tset_table[i]);

	return ret;
}

static void s6e3fa2_update_seq(struct lcd_info *lcd)
{
#if 0
	u8 id = lcd->id[2];

	/* Panel revision history
		0x00: Rev A, B, C
		0x01: Rev D, E, F
		0x02: Rev G,
		0x12: Rev H
		0x13: Rev I, J */

	if (id < 0x02) { /* Panel rev.A ~ rev.F */
		/* Panel Command */
		pSEQ_AOR_CONTROL = SEQ_AOR_CONTROL_RevF;
		pELVSS_DIM_TABLE = ELVSS_DIM_TABLE_RevF;
		ELVSS_STATUS_MAX = ARRAY_SIZE(ELVSS_DIM_TABLE_RevF);
		pELVSS_TABLE = ELVSS_TABLE_RevF;

		/* Dynamic AID parameta */
		paor_cmd = aor_cmd_revF;
	}

	if (id < 0x12) { /* Panel rev.A ~ rev.G */
		pSEQ_TOUCH_HSYNC_ON = SEQ_TOUCH_HSYNC_ON_RevG;
	}
#endif
}

static int get_backlight_level_from_brightness(int brightness)
{
	int backlightlevel = DEFAULT_GAMMA_INDEX;
	int i, gamma;

	gamma = (brightness * MAX_GAMMA) / MAX_BRIGHTNESS;
	for (i = 0; i < GAMMA_MAX-1; i++) {
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

static int s6e3fa2_gamma_ctl(struct lcd_info *lcd)
{
	s6e3fa2_write(lcd, lcd->gamma_table[lcd->bl], GAMMA_PARAM_SIZE);

	return 0;
}

static int s6e3fa2_aid_parameter_ctl(struct lcd_info *lcd, u8 force)
{
	if (force)
		goto aid_update;
	else if (lcd->aor[lcd->bl][1] !=  lcd->aor[lcd->current_bl][1])
		goto aid_update;
	else if (lcd->aor[lcd->bl][2] !=  lcd->aor[lcd->current_bl][2])
		goto aid_update;
	else
		goto exit;

aid_update:
	s6e3fa2_write(lcd, lcd->aor[lcd->bl], AID_PARAM_SIZE);

exit:
	return 0;
}

static int s6e3fa2_set_acl(struct lcd_info *lcd, u8 force)
{
	int ret = 0, level = ACL_STATUS_15P;

	if (lcd->siop_enable || LEVEL_IS_HBM(lcd->brightness))
		goto acl_update;

	if (!lcd->acl_enable)
		level = ACL_STATUS_0P;

acl_update:
	if (force || lcd->current_acl != ACL_CUTOFF_TABLE[level][1]) {
		ret = s6e3fa2_write(lcd, ACL_CUTOFF_TABLE[level], ACL_PARAM_SIZE);
		lcd->current_acl = ACL_CUTOFF_TABLE[level][1];
		if (level || LEVEL_IS_HBM(lcd->brightness))
			ret = s6e3fa2_write(lcd, SEQ_ACL_ON_OPR_AVR, ARRAY_SIZE(SEQ_ACL_ON_OPR_AVR));
		else
			ret = s6e3fa2_write(lcd, SEQ_ACL_OFF_OPR_AVR, ARRAY_SIZE(SEQ_ACL_OFF_OPR_AVR));

		dev_info(&lcd->ld->dev, "acl: %d, brightness: %d\n", lcd->current_acl, lcd->brightness);
	}

	if (!ret)
		ret = -EPERM;

	return ret;
}

static int s6e3fa2_set_elvss(struct lcd_info *lcd, u8 force)
{
	int ret = 0, i;
	u32 elvss_level, nit;
	union elvss_info elvss;

	nit = DIM_TABLE[lcd->bl];

	elvss_level = ELVSS_STATUS_360;

	for (i = 0; i < ELVSS_STATUS_MAX; i++) {
		if (nit <= pELVSS_DIM_TABLE[i]) {
			elvss_level = i;
			break;
		}
	}

	if (!(lcd->acl_enable) && nit >= 20) elvss.mps = 0x9C;
	else elvss.mps = 0x8C;

	elvss.offset = pELVSS_TABLE[elvss_level][lcd->acl_enable];

	if ((elvss_level == ELVSS_STATUS_HBM))
		elvss.hbm = lcd->elvss_hbm[1];
	else {
		elvss.hbm = lcd->elvss_hbm[0];	// normal brightness

		if(lcd->temperature <= -20 && nit >= 30) elvss.hbm -= 0x03;

		if( nit < 30) {
			if (lcd->temperature > 0) elvss.hbm = 0x12;
			else if (lcd->temperature <= 0 && lcd->temperature > -20 ){
				switch(nit) {
					case 21:
						elvss.hbm = 0x1A;
						break;
					case 22:
						elvss.hbm = 0x18;
						break;
					case 24:
						elvss.hbm = 0x16;
						break;
					case 25:
					case 27:
						elvss.hbm = 0x14;
						break;
					case 29:
						elvss.hbm = 0x12;
						break;
					default:
						elvss.hbm = 0x1C;
						break;
				}
			} else {
				switch(nit) {
					case 21:
						elvss.hbm = 0x1D;
						break;
					case 22:
						elvss.hbm = 0x1B;
						break;
					case 24:
						elvss.hbm = 0x19;
						break;
					case 25:
						elvss.hbm = 0x17;
						break;
					case 27:
						elvss.hbm = 0x15;
						break;
					case 29:
						elvss.hbm = 0x12;
						break;
					default :
						elvss.hbm = 0x1F;
					break;
				}
			}
		}
	}

	if (force)
		goto elvss_update;
	else if (lcd->current_elvss.value != elvss.value)
		goto elvss_update;
	else
		goto exit;

elvss_update:
	lcd->elvss[1] = elvss.mps;
	lcd->elvss[2] = elvss.offset;
	lcd->elvss[21] = elvss.hbm;

	ret = s6e3fa2_write(lcd, lcd->elvss, ELVSS_PARAM_SIZE);
	lcd->current_elvss = elvss;

	if (!ret)
		ret = -EPERM;
exit:
	return 0;
}

static int s6e3fa2_set_tset(struct lcd_info *lcd, u8 force)
{
	int ret = 0;
	u8 tset;

	if (lcd->temperature >= 0) {
		tset = (u8)lcd->temperature;
		tset &= 0x7f;
	} else {
		tset = (u8)(lcd->temperature * (-1));
		tset |= 0x80;
	}

	if (force || lcd->temperature != lcd->current_temp) {
		lcd->tset_table[LDI_TSET_LEN] = tset;
		ret = s6e3fa2_write(lcd, lcd->tset_table, LDI_TSET_LEN + 1);
		lcd->current_temp = lcd->temperature;
		dev_info(&lcd->ld->dev, "temperature: %d, tset: %d\n",
				lcd->temperature, tset);
	}

	if (!ret) {
		ret = -EPERM;
		goto err;
	}

err:
	return ret;
}

static int s6e3fa2_set_partial_display(struct lcd_info *lcd)
{
	if (lcd->partial_range[0] || lcd->partial_range[1]) {
		s6e3fa2_write(lcd, SEQ_PARTIAL_AREA, ARRAY_SIZE(SEQ_PARTIAL_AREA));
		s6e3fa2_write(lcd, SEQ_PARTIAL_DISP, ARRAY_SIZE(SEQ_PARTIAL_DISP));
	} else
		s6e3fa2_write(lcd, SEQ_NORMAL_DISP, ARRAY_SIZE(SEQ_NORMAL_DISP));

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
	lcd->daid.offset_color = offset_color;
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
	lcd->gamma_table = kzalloc(GAMMA_MAX * sizeof(u8 *), GFP_KERNEL);
	if (!lcd->gamma_table) {
		pr_err("failed to allocate gamma table 2\n");
		ret = -ENOMEM;
		goto err_alloc_gamma_table2;
	}

	for (i = 0; i < GAMMA_MAX; i++) {
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
	for (i = 0; i < GAMMA_MAX - 1; i++) {
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
	int i, j;

	for (i = 0; i < GAMMA_MAX; i++)
		memcpy(lcd->aor[i], pSEQ_AOR_CONTROL, ARRAY_SIZE(SEQ_AOR_CONTROL));

	for (i = 0; i < GAMMA_MAX - 1; i++) {
		lcd->aor[i][1] = paor_cmd[i][0];
		lcd->aor[i][2] = paor_cmd[i][1];
	}

	for (i = 0; i < GAMMA_MAX; i++) {
		for (j = 0; j < ARRAY_SIZE(SEQ_AOR_CONTROL); j++)
			smtd_dbg("%02X ", lcd->aor[i][j]);
		smtd_dbg("\n");
	}

	return 0;
}

static int init_elvss_table(struct lcd_info *lcd, const u8 *elvss_data)
{
	memcpy(lcd->elvss, SEQ_ELVSS_SET, ELVSS_PARAM_SIZE);
	lcd->elvss_hbm[0] = elvss_data[20];

	return 0;
}

static int init_hbm_parameter(struct lcd_info *lcd, const u8 *mtp_data)
{
	int i;

	for (i = 0; i < GAMMA_PARAM_SIZE; i++)
		lcd->gamma_table[GAMMA_HBM][i] = lcd->gamma_table[GAMMA_360CD][i];

	/* C8 : 34~39 -> CA : 1~6 */
	for (i = 0; i < 6; i++)
		lcd->gamma_table[GAMMA_HBM][1 + i] = mtp_data[33 + i];

	/* C8 : 73~87 -> CA : 7~21 */
	for (i = 0; i < 15; i++)
		lcd->gamma_table[GAMMA_HBM][7 + i] = mtp_data[72 + i];

	lcd->elvss_hbm[1] = mtp_data[39];

	return 0;

}

static void show_lcd_table(struct lcd_info *lcd)
{
	int i, j;

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
}

#if defined(CONFIG_LCD_PCD)
static void s6e3fa2_err_fg_enable(struct lcd_info *lcd, int enable)
{
	struct device *dev = lcd->dsim->dev;

	if (!lcd->connected)
		return;

	dev_info(dev, "%s: %s, gpio: %d, state: %d\n", __func__, enable ? "enable" : "disable",
		gpio_get_value(lcd->err_fg_gpio), lcd->err_fg_state);

	if (!lcd->err_fg_irq)
		return;

	if (enable)
		enable_irq(lcd->err_fg_irq);
	else {
		if (!lcd->err_fg_state)
			disable_irq(lcd->err_fg_irq);
		if (IS_ERR(devm_pinctrl_get_select(dev, "errfg_off")))
			dev_info(dev, "%s err\n", __func__);
	}
}

static int s6e3fa2_err_fg_blank(struct lcd_info *lcd)
{
	struct fb_info *info = registered_fb[0];
	int ret = 0;

	dev_info(&lcd->ld->dev, "+%s\n", __func__);

	spin_lock(&lcd->err_fg_lock);
	lcd->err_fg_state = true;
	spin_unlock(&lcd->err_fg_lock);

	if (!lock_fb_info(info))
		return -ENODEV;
	info->flags |= FBINFO_MISC_USEREVENT;
	info->flags |= FBINFO_MISC_ESD_DETECTED;
	ret = fb_blank(info, FB_BLANK_POWERDOWN);
	ret = fb_blank(info, FB_BLANK_UNBLANK);
	info->flags &= ~FBINFO_MISC_ESD_DETECTED;
	info->flags &= ~FBINFO_MISC_USEREVENT;
	unlock_fb_info(info);

	spin_lock(&lcd->err_fg_lock);
	lcd->err_fg_state = false;
	spin_unlock(&lcd->err_fg_lock);

	dev_info(&lcd->ld->dev, "-%s: %d\n", __func__, gpio_get_value(lcd->err_fg_gpio));

	return ret;
}

static void s6e3fa2_err_fg_is_detected(struct lcd_info *lcd)
{
	struct file *fp;
	struct timespec ts;
	struct rtc_time tm;
	char name[NAME_MAX];

	getnstimeofday(&ts);
	rtc_time_to_tm(ts.tv_sec, &tm);
	sprintf(name, "%s%02d-%02d_%02d:%02d:%02d_%02d",
		"/sdcard/", tm.tm_mon + 1, tm.tm_mday, tm.tm_hour, tm.tm_min, tm.tm_sec, lcd->err_fg_count);
	fp = filp_open(name, O_CREAT, S_IRWXU | S_IRWXG | S_IRWXO);
	sys_sync();

	if (IS_ERR_OR_NULL(fp))
		dev_info(&lcd->ld->dev, "fail to create %s log file, %s\n", __func__, name);
}

static void s6e3fa2_err_fg_work(struct work_struct *work)
{
	struct lcd_info *lcd =
		container_of(work, struct lcd_info, err_fg.work);

	dev_info(&lcd->ld->dev, "%s: level: %d, state: %d, count: %d\n",
		__func__, gpio_get_value(lcd->err_fg_gpio), lcd->err_fg_state, lcd->err_fg_count);

	s6e3fa2_err_fg_is_detected(lcd);

	s6e3fa2_err_fg_blank(lcd);
}
static irqreturn_t s6e3fa2_err_fg_isr(int irq, void *dev_id)
{
	struct lcd_info *lcd = dev_id;
	struct device *dev = lcd->dsim->dev;
//	unsigned int err_fg_level = gpio_get_value(lcd->err_fg_gpio);

	dev_info(dev, "%s : err_fg is detected!!\n", __func__);

	lcd->err_fg_count++;

	if (!lcd->err_fg_state && lcd->err_fg_count > 1) {
		disable_irq_nosync(lcd->err_fg_irq);
		schedule_delayed_work(&lcd->err_fg, 0);
	}

	return IRQ_HANDLED;
}
static int s6e3fa2_err_fg_init(struct lcd_info *lcd)
{

	struct device *dev = lcd->dsim->dev;
	int ret = 0;

	if (!lcd->connected)
		return ret;

	lcd->err_fg_gpio = of_get_named_gpio(dev->of_node, "oled-errfg-gpio", 0);
	if (!lcd->err_fg_gpio) {
		dev_err(&lcd->ld->dev, "failed to get proper gpio number\n");
		return -EINVAL;
	}

	if (gpio_get_value(lcd->err_fg_gpio)) {
		dev_info(dev, "%s: gpio(%d) is already High\n", __func__, lcd->err_fg_gpio);
		return ret;
	}

	lcd->err_fg_irq = gpio_to_irq(lcd->err_fg_gpio);
	if (!lcd->err_fg_irq) {
		dev_err(&lcd->ld->dev, "failed to get proper irq number\n");
		return -EINVAL;
	}

	spin_lock_init(&lcd->err_fg_lock);

	INIT_DELAYED_WORK(&lcd->err_fg, s6e3fa2_err_fg_work);

	ret = request_irq(lcd->err_fg_irq, s6e3fa2_err_fg_isr, IRQF_TRIGGER_RISING |
            IRQF_ONESHOT, "err_fg", lcd);
	if (ret) {
		dev_err(&lcd->ld->dev, "err_fg irq request failed\n");
		return ret;
	}

	dev_info(dev, "%s: err_fg(%d)\n", __func__, gpio_get_value(lcd->err_fg_gpio));
	return ret;
}
#endif

static int update_brightness(struct lcd_info *lcd, u8 force)
{
	mutex_lock(&lcd->bl_lock);

	lcd->brightness = lcd->bd->props.brightness;

	lcd->bl = get_backlight_level_from_brightness(lcd->brightness > MAX_BRIGHTNESS ? MAX_BRIGHTNESS : lcd->brightness);

	if (LEVEL_IS_HBM(lcd->brightness))
		lcd->bl = GAMMA_HBM;

	if ((force) || ((lcd->ldi_enable) && (lcd->current_bl != lcd->bl))) {
		s6e3fa2_write(lcd, SEQ_TEST_KEY_ON_F0, ARRAY_SIZE(SEQ_TEST_KEY_ON_F0));
		s6e3fa2_gamma_ctl(lcd);
		s6e3fa2_aid_parameter_ctl(lcd, force);
		s6e3fa2_set_elvss(lcd, force);

		s6e3fa2_set_acl(lcd, force);
		s6e3fa2_set_tset(lcd, force);

		s6e3fa2_write(lcd, SEQ_GAMMA_UPDATE, ARRAY_SIZE(SEQ_GAMMA_UPDATE));
		if(lcd->brightness == MIN_GAMMA || lcd->bl == GAMMA_HBM)
			s6e3fa2_write(lcd, SEQ_GAMMA_UPDATE_L, ARRAY_SIZE(SEQ_GAMMA_UPDATE_L));
		s6e3fa2_write(lcd, SEQ_TEST_KEY_OFF_F0, ARRAY_SIZE(SEQ_TEST_KEY_OFF_F0));

		lcd->current_bl = lcd->bl;

		dev_info(&lcd->ld->dev, "brightness=%d, bl=%d, candela=%d\n", \
		lcd->brightness, lcd->bl, DIM_TABLE[lcd->bl]);

	}

	mutex_unlock(&lcd->bl_lock);
	return 0;
}

static int s6e3fa2_ldi_init(struct lcd_info *lcd)
{
	int ret = 0;

	lcd->connected = 1;
	lcd->need_update = 0;

	msleep(10);
	s6e3fa2_write(lcd, SEQ_TEST_KEY_ON_F0, ARRAY_SIZE(SEQ_TEST_KEY_ON_F0));
	s6e3fa2_write(lcd, SEQ_TEST_KEY_ON_FC, ARRAY_SIZE(SEQ_TEST_KEY_ON_FC));
	s6e3fa2_write(lcd, SEQ_SLEEP_OUT, ARRAY_SIZE(SEQ_SLEEP_OUT));
	msleep(20);

#ifdef CONFIG_DECON_MIC
	s6e3fa2_write(lcd, SEQ_MIC_ENABLE, ARRAY_SIZE(SEQ_MIC_ENABLE));
#endif

	s6e3fa2_read_id(lcd, lcd->id);

#if 0
	/* Brightness Control : 350nit */
	s6e3fa2_write(lcd, SEQ_GAMMA_CONTROL_SET_350CD, ARRAY_SIZE(SEQ_GAMMA_CONTROL_SET_350CD));
	s6e3fa2_write(lcd, SEQ_AOR_CONTROL, ARRAY_SIZE(SEQ_AOR_CONTROL));
	s6e3fa2_write(lcd, SEQ_ELVSS_SET, ARRAY_SIZE(SEQ_ELVSS_SET));
	s6e3fa2_write(lcd, SEQ_GAMMA_UPDATE, ARRAY_SIZE(SEQ_GAMMA_UPDATE));
	s6e3fa2_write(lcd, SEQ_GAMMA_UPDATE_L, ARRAY_SIZE(SEQ_GAMMA_UPDATE_L));
	s6e3fa2_write(lcd, SEQ_ACL_OFF, ARRAY_SIZE(SEQ_ACL_OFF));

	/* ELVSS Control : ACL OFF */
	s6e3fa2_write(lcd, SEQ_GPARAM_TSET, ARRAY_SIZE(SEQ_GPARAM_TSET));
	s6e3fa2_write(lcd, SEQ_TSET, ARRAY_SIZE(SEQ_TSET));
	s6e3fa2_write(lcd, SEQ_ACL_OFF_OPR_AVR, ARRAY_SIZE(SEQ_ACL_OFF_OPR_AVR));
	s6e3fa2_write(lcd, SEQ_ELVSS_SET, ARRAY_SIZE(SEQ_ELVSS_SET));
#endif
	/* Brightness Control */
	update_brightness(lcd, 1);

	s6e3fa2_write(lcd, SEQ_TEST_KEY_OFF_FC, ARRAY_SIZE(SEQ_TEST_KEY_OFF_FC));

	s6e3fa2_write(lcd, SEQ_TE_ON, ARRAY_SIZE(SEQ_TE_ON));

	msleep(120);

	return ret;
}

static int s6e3fa2_ldi_enable(struct lcd_info *lcd)
{
	int ret = 0;

	s6e3fa2_write(lcd, SEQ_DISPLAY_ON, ARRAY_SIZE(SEQ_DISPLAY_ON));
	dev_info(&lcd->ld->dev, "DISPLAY_ON\n");

	return ret;
}

static int s6e3fa2_ldi_disable(struct lcd_info *lcd)
{
	int ret = 0;

	dev_info(&lcd->ld->dev, "+ %s\n", __func__);

	/* Display Off (28h) */
	s6e3fa2_write(lcd, SEQ_DISPLAY_OFF, ARRAY_SIZE(SEQ_DISPLAY_OFF));
	dev_info(&lcd->ld->dev, "DISPLAY_OFF\n");

	/* Wait 10ms */
	usleep_range(10000, 11000);

	/* Sleep In (10h) */
	s6e3fa2_write(lcd, SEQ_SLEEP_IN, ARRAY_SIZE(SEQ_SLEEP_IN));

	/* Wait 120ms */
	msleep(125);
	dev_info(&lcd->ld->dev, "- %s\n", __func__);

	return ret;
}

static int s6e3fa2_power_on(struct lcd_info *lcd)
{
	int ret = 0;

	dev_info(&lcd->ld->dev, "+ %s\n", __func__);

	ret = s6e3fa2_ldi_init(lcd);
	if (ret) {
		dev_err(&lcd->ld->dev, "failed to initialize ldi.\n");
		goto err;
	}

	ret = s6e3fa2_ldi_enable(lcd);
	if (ret) {
		dev_err(&lcd->ld->dev, "failed to enable ldi.\n");
		goto err;
	}

	mutex_lock(&lcd->bl_lock);
	lcd->ldi_enable = 1;
	mutex_unlock(&lcd->bl_lock);

	if (lcd->need_update)
		update_brightness(lcd, 1);

#if defined(CONFIG_LCD_PCD)
	s6e3fa2_err_fg_enable(lcd, 1);
#endif

	dev_info(&lcd->ld->dev, "- %s\n", __func__);
err:
	return ret;
}

static int s6e3fa2_power_off(struct lcd_info *lcd)
{
	int ret = 0;

	dev_info(&lcd->ld->dev, "+ %s\n", __func__);

#if defined(CONFIG_LCD_PCD)
	s6e3fa2_err_fg_enable(lcd, 0);
#endif

	mutex_lock(&lcd->bl_lock);
	lcd->ldi_enable = 0;
	mutex_unlock(&lcd->bl_lock);

	ret = s6e3fa2_ldi_disable(lcd);

	dev_info(&lcd->ld->dev, "- %s\n", __func__);

	return ret;
}

static int s6e3fa2_power(struct lcd_info *lcd, int power)
{
	int ret = 0;

	if (POWER_IS_ON(power) && !POWER_IS_ON(lcd->power))
		ret = s6e3fa2_power_on(lcd);
	else if (!POWER_IS_ON(power) && POWER_IS_ON(lcd->power))
		ret = s6e3fa2_power_off(lcd);

	if (!ret)
		lcd->power = power;

	return ret;
}

static int s6e3fa2_set_power(struct lcd_device *ld, int power)
{
	struct lcd_info *lcd = lcd_get_data(ld);

	if (power != FB_BLANK_UNBLANK && power != FB_BLANK_POWERDOWN &&
		power != FB_BLANK_NORMAL) {
		dev_err(&lcd->ld->dev, "power value should be 0, 1 or 4.\n");
		return -EINVAL;
	}

	return s6e3fa2_power(lcd, power);
}

static int s6e3fa2_get_power(struct lcd_device *ld)
{
	struct lcd_info *lcd = lcd_get_data(ld);

	return lcd->power;
}

static int s6e3fa2_check_fb(struct lcd_device *ld, struct fb_info *fb)
{
	return 0;
}

static int s6e3fa2_get_brightness(struct backlight_device *bd)
{
	struct lcd_info *lcd = bl_get_data(bd);

	return DIM_TABLE[lcd->bl];
}

static int s6e3fa2_set_brightness(struct backlight_device *bd)
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
	else
		lcd->need_update = 1;

	return ret;
}

static struct lcd_ops s6e3fa2_lcd_ops = {
	.set_power = s6e3fa2_set_power,
	.get_power = s6e3fa2_get_power,
	.check_fb  = s6e3fa2_check_fb,
};

static const struct backlight_ops s6e3fa2_backlight_ops = {
	.get_brightness = s6e3fa2_get_brightness,
	.update_status = s6e3fa2_set_brightness,
};

static ssize_t power_reduce_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct lcd_info *lcd = dev_get_drvdata(dev);
	char temp[3];

	sprintf(temp, "%d\n", lcd->acl_enable);
	strcpy(buf, temp);

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
	char temp[15];

	sprintf(temp, "SDC_%02X%02X%02X\n", lcd->id[0], lcd->id[1], lcd->id[2]);

	strcat(buf, temp);
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
		bl = LEVEL_IS_HBM(i) ? GAMMA_HBM : bl;
		pos += sprintf(pos, "%3d %3d\n", i, DIM_TABLE[bl]);
	}

	return pos - buf;
}

static ssize_t ddi_id_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct lcd_info *lcd = dev_get_drvdata(dev);
	char temp[30];

	sprintf(temp, "ddi id : %02X %02X %02X %02X %02X\n",
		lcd->ddi_id[0], lcd->ddi_id[1], lcd->ddi_id[2], lcd->ddi_id[3], lcd->ddi_id[4]);

	strcat(buf, temp);
	return strlen(buf);
}

static ssize_t manufacture_code_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct lcd_info *lcd = dev_get_drvdata(dev);

	sprintf(buf, "%02X%02X%02X%02X%02X\n",
		lcd->ddi_id[0], lcd->ddi_id[1], lcd->ddi_id[2], lcd->ddi_id[3], lcd->ddi_id[4]);

	return strlen(buf);
}

static ssize_t gamma_table_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct lcd_info *lcd = dev_get_drvdata(dev);
	int i, j;

	for (i = 0; i < GAMMA_MAX; i++) {
		for (j = 0; j < GAMMA_PARAM_SIZE; j++)
			printk("0x%02x, ", lcd->gamma_table[i][j]);
		printk("\n");
	}

	return strlen(buf);
}

static ssize_t siop_enable_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct lcd_info *lcd = dev_get_drvdata(dev);
	char temp[3];

	sprintf(temp, "%d\n", lcd->siop_enable);
	strcpy(buf, temp);

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
		if (value >= 1)
			lcd->temperature = NORMAL_TEMPERATURE;
		else
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

	sprintf(buf, "%d, %d\n", lcd->coordinate[0], lcd->coordinate[1]);

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

static ssize_t parameter_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
#if 0
	struct lcd_info *lcd = dev_get_drvdata(dev);
	char *pos = buf;
	unsigned char temp[50] = {0,};
	int i;

	if (!lcd->ldi_enable)
		return -EINVAL;

	/* ID */
	s6e3fa0_read(lcd, LDI_ID_REG, temp, LDI_ID_LEN);
	pos += sprintf(pos, "ID    [04]: %02x, %02x, %02x\n", temp[0], temp[1], temp[2]);

#if 0
	/* PSRE */
	s6e3fa0_read(lcd, 0xB5, temp, 3);
	pos += sprintf(pos, "PSRE  [B5]: %02x, %02x, %02x\n", temp[0], temp[1], temp[2]);
#endif

	/* ACL */
	s6e3fa0_read(lcd, 0x56, temp, 1);
	pos += sprintf(pos, "ACL   [56]: %02x\n", temp[0]);

	/* ELVSS */
	s6e3fa0_read(lcd, LDI_ELVSS_REG, temp, ELVSS_PARAM_SIZE - 1);
	pos += sprintf(pos, "ELVSS [B6]: %02x, %02x\n", temp[0], temp[1]);

	/* GAMMA */
	s6e3fa0_read(lcd, 0xCA, temp, GAMMA_PARAM_SIZE - 1);
	pos += sprintf(pos, "GAMMA [CA]: ");
	for (i = 0; i < GAMMA_PARAM_SIZE - 1; i++)
		pos += sprintf(pos, "%02x, ", temp[i]);
	pos += sprintf(pos, "\n");

	return pos - buf;
#endif
	return 0;
}

static ssize_t partial_disp_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct lcd_info *lcd = dev_get_drvdata(dev);

	sprintf((char *)buf, "%d, %d\n", lcd->partial_range[0], lcd->partial_range[1]);

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
		s6e3fa2_set_partial_display(lcd);

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
			dev_info(dev, "MTP Offset V%d R:%d G:%d B:%d\n",
					lcd->daid.iv_tbl[i], mtp[j], mtp[j + 1], mtp[j + 2]);
	}

	for (i = 0; i < GAMMA_MAX - 1; i++) {
		memset(temp, 0, 256);
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
static DEVICE_ATTR(manufacture_code, 0444, manufacture_code_show, NULL);
static DEVICE_ATTR(gamma_table, 0444, gamma_table_show, NULL);
static DEVICE_ATTR(siop_enable, 0664, siop_enable_show, siop_enable_store);
static DEVICE_ATTR(temperature, 0664, temperature_show, temperature_store);
static DEVICE_ATTR(color_coordinate, 0444, color_coordinate_show, NULL);
static DEVICE_ATTR(manufacture_date, 0444, manufacture_date_show, NULL);
static DEVICE_ATTR(parameter, 0444, parameter_show, NULL);
static DEVICE_ATTR(partial_disp, 0664, partial_disp_show, partial_disp_store);
static DEVICE_ATTR(aid_log, 0444, aid_log_show, NULL);
static DEVICE_ATTR(lux, 0644, lux_show, lux_store);
static DEVICE_ATTR(brightness_table, 0444, brightness_table_show, NULL);

static int s6e3fa2_probe(struct mipi_dsim_device *dsim)
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

	lcd->ld = lcd_device_register("panel", dsim->dev, lcd, &s6e3fa2_lcd_ops);
	if (IS_ERR(lcd->ld)) {
		pr_err("failed to register lcd device\n");
		ret = PTR_ERR(lcd->ld);
		goto out_free_lcd;
	}
	dsim->lcd = lcd->ld;

	lcd->bd = backlight_device_register("panel", dsim->dev, lcd, &s6e3fa2_backlight_ops, NULL);
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

	ret = device_create_file(&lcd->ld->dev, &dev_attr_manufacture_code);
	if (ret < 0)
		dev_err(&lcd->ld->dev, "failed to add sysfs entries, %d\n", __LINE__);

	ret = device_create_file(&lcd->ld->dev, &dev_attr_gamma_table);
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

	ret = device_create_file(&lcd->ld->dev, &dev_attr_parameter);
	if (ret < 0)
		dev_err(&lcd->ld->dev, "failed to add sysfs entries, %d\n", __LINE__);

	ret = device_create_file(&lcd->ld->dev, &dev_attr_partial_disp);
	if (ret < 0)
		dev_err(&lcd->ld->dev, "failed to add sysfs entries, %d\n", __LINE__);

	ret = device_create_file(&lcd->ld->dev, &dev_attr_aid_log);
	if (ret < 0)
		dev_err(&lcd->ld->dev, "failed to add sysfs entries, %d\n", __LINE__);

	ret = device_create_file(&lcd->ld->dev, &dev_attr_lux);
	if (ret < 0)
		dev_err(&lcd->ld->dev, "failed to add sysfs entries, %d\n", __LINE__);

	ret = device_create_file(&lcd->bd->dev, &dev_attr_brightness_table);
	if (ret < 0)
		dev_err(&lcd->ld->dev, "failed to add sysfs entries, %d\n", __LINE__);

	/* dev_set_drvdata(dsim->dev, lcd); */

	mutex_init(&lcd->lock);
	mutex_init(&lcd->bl_lock);

	s6e3fa2_write(lcd, SEQ_TEST_KEY_ON_F0, ARRAY_SIZE(SEQ_TEST_KEY_ON_F0));
	s6e3fa2_read_id(lcd, lcd->id);
	s6e3fa2_update_seq(lcd);
	s6e3fa2_read_ddi_id(lcd, lcd->ddi_id);
	s6e3fa2_read_coordinate(lcd);
	s6e3fa2_read_mtp(lcd, mtp_data);
	s6e3fa2_read_elvss(lcd, elvss_data);
	s6e3fa2_read_tset(lcd);
	s6e3fa2_write(lcd, SEQ_TEST_KEY_OFF_F0, ARRAY_SIZE(SEQ_TEST_KEY_OFF_F0));

	dev_info(&lcd->ld->dev, "ID: %x, %x, %x\n", lcd->id[0], lcd->id[1], lcd->id[2]);

	init_dynamic_aid(lcd);

	ret = init_gamma_table(lcd, mtp_data);
	ret += init_aid_dimming_table(lcd);
	ret += init_elvss_table(lcd, elvss_data);
	ret += init_hbm_parameter(lcd, mtp_data);

	if (ret)
		dev_info(&lcd->ld->dev, "gamma table generation is failed\n");

	if (lcd->power == FB_BLANK_POWERDOWN)
		s6e3fa2_power(lcd, FB_BLANK_UNBLANK);
	else
		update_brightness(lcd, 1);

	show_lcd_table(lcd);

	lcd->ldi_enable = 1;

#if defined(CONFIG_DECON_MDNIE_LITE)
	mdnie_register(&lcd->ld->dev, lcd, (mdnie_w)s6e3fa2_send_seq, (mdnie_r)s6e3fa2_read, &tune_info);
	lcd->mdnie_class = get_mdnie_class();
#endif

#if defined(CONFIG_LCD_PCD)
	s6e3fa2_err_fg_init(lcd);
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

static int s6e3fa2_displayon(struct mipi_dsim_device *dsim)
{
	struct lcd_info *lcd = dev_get_drvdata(&dsim->lcd->dev);

	s6e3fa2_power(lcd, FB_BLANK_UNBLANK);

	return 0;
}

static int s6e3fa2_suspend(struct mipi_dsim_device *dsim)
{
	struct lcd_info *lcd = dev_get_drvdata(&dsim->lcd->dev);

	s6e3fa2_power(lcd, FB_BLANK_POWERDOWN);

	return 0;
}

static int s6e3fa2_resume(struct mipi_dsim_device *dsim)
{
	return 0;
}

struct mipi_dsim_lcd_driver s6e3fa2_mipi_lcd_driver = {
	.probe		= s6e3fa2_probe,
	.displayon	= s6e3fa2_displayon,
	.suspend	= s6e3fa2_suspend,
	.resume		= s6e3fa2_resume,
};

static int s6e3fa2_init(void)
{
	return 0;
}

static void s6e3fa2_exit(void)
{
	return;
}

module_init(s6e3fa2_init);
module_exit(s6e3fa2_exit);

MODULE_DESCRIPTION("MIPI-DSI s6e3fa2 (1080*1920) Panel Driver");
MODULE_LICENSE("GPL");

