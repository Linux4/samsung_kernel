/* drivers/video/decon_display/s6tnmr7_mipi_lcd.c
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

#include "s6tnmr7_param.h"
#include "dynamic_aid_s6tnmr7.h"

#if defined(CONFIG_DECON_MDNIE_LITE_CHAGALL)
#include "mdnie_chagall.h"
#endif

#define POWER_IS_ON(pwr)		(pwr <= FB_BLANK_NORMAL)
#define LEVEL_IS_HBM(brightness)	(brightness == EXTEND_BRIGHTNESS)
#define LEVEL_IS_CAPS_OFF(level)	(level <= 19)

#define NORMAL_TEMPERATURE		25	/* 25 degrees Celsius */

#define EXTEND_BRIGHTNESS		355
#define MIN_BRIGHTNESS			0
#define MAX_BRIGHTNESS			255
#define DEFAULT_BRIGHTNESS		134

#define MIN_GAMMA			2
#define MAX_GAMMA			350
#define LINEAR_MIN_GAMMA			30

#define DEFAULT_GAMMA_INDEX		IBRIGHTNESS_162NT

#define LDI_HBM_LEN			11
#define LDI_HBM_MAX			(LDI_HBM_LEN * 3)
#define LDI_HBMR_REG			0xCC10
#define LDI_HBMG_REG			0xCE10
#define LDI_HBMB_REG			0xD010
#define LDI_HBMELVSSON_REG		0xB395
#define LDI_HBMELVSSOFF_REG		0xBB35

#define LDI_ID_REG			0x04
#define LDI_ID_LEN			3
#define LDI_CODE_REG			0xD6
#define LDI_CODE_LEN			5

#define LDI_MTPR_REG			0xD200
#define LDI_MTPG_REG			0xD280
#define LDI_MTPB_REG			0xD300
#define MTP_VMAX			11
#define LDI_MTP_LEN			(MTP_VMAX * 3)
#define LDI_ELVSS_REG			0xB6
#define LDI_ELVSS_LEN			17

#define LDI_COORDINATE_REG		0xB38A
#define LDI_COORDINATE_LEN		4

#define LDI_BURST_SIZE		128
#define LDI_PARAM_MSB		0xB1
#define LDI_PARAM_LSB		0xB0
#define LDI_PARAM_LSB_SIZE	2

#define LDI_MDNIE_SIZE		136
#define MDNIE_FIRST_SIZE		82
#define MDNIE_SECOND_SIZE	54

#define LDI_GAMMA_REG		0x83

#ifdef SMART_DIMMING_DEBUG
#define smtd_dbg(format, arg...)	printk(format, ##arg)
#else
#define smtd_dbg(format, arg...)
#endif

#define GET_UPPER_4BIT(x)		((x >> 4) & 0xF)
#define GET_LOWER_4BIT(x)		(x & 0xF)

static const unsigned int DIM_TABLE[IBRIGHTNESS_MAX] = {
	2,	3,	4,	5,	6,	7,	8,	9,	10,	11,	12,	13,	14,	15,	16,	17,
	19,	20,	21,	22,	24,	25,	27,	29,	30,	32,	34,	37,	39,	41,	44,	47,
	50,	53,	56,	60,	64,	68,	72,	77,	82,	87,	93,	98,	105, 111, 119,
	126, 134, 143, 152,	162,	172,	183,	195,	207,	220,
	234, 249, 265, 282, 300, 400
};

struct lcd_info {
	unsigned int			bl;
	unsigned int			brightness;
	unsigned int			acl_enable;
	unsigned int			siop_enable;
	unsigned int			caps_enable;
	unsigned int			current_acl;
	unsigned int			current_bl;
	unsigned char			current_elvss;
	unsigned int			current_hbm;
	unsigned int			current_vint;
	unsigned int			ldi_enable;
	unsigned int			power;
	struct mutex			lock;
	struct mutex			bl_lock;
#if defined(CONFIG_DECON_MDNIE_LITE_CHAGALL)
	struct mdnie_device		*md;
	int				mdnie_addr;
#endif

	struct device			*dev;
	struct lcd_device		*ld;
	struct backlight_device		*bd;
	unsigned char			id[LDI_ID_LEN];
	unsigned char			code[LDI_CODE_LEN];
	unsigned int				*gamma_level;
	unsigned char			**gamma_table;
	unsigned char			elvss_hbm[2];
	unsigned char			**elvss_table;
	int				elvss_delta;
	struct dynamic_aid_param_t	daid;
	unsigned char			aor[IBRIGHTNESS_MAX][ARRAY_SIZE(SEQ_AOR_CONTROL)];
	unsigned int			connected;
	unsigned int			err_count;

	int				temperature;
	unsigned int			coordinate[2];
	unsigned int			partial_range[2];
	unsigned char			date[2];

	int						lux;
	struct class			*mdnie_class;

	unsigned int			width;
	unsigned int			height;

#ifdef CONFIG_LCD_ALPM
	int				alpm;
	int				current_alpm;
#endif
	struct mipi_dsim_device		*dsim;
};

int s6tnmr7_write(struct lcd_info *lcd, const u8 *seq, u32 len)
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

int s6tnmr7_read(struct lcd_info *lcd, u8 addr, u8 *buf, u32 len)
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

#if defined(CONFIG_LCD_ALPM)
static int s6tnmr7_write_set(struct lcd_info *lcd, struct lcd_seq_info *seq, u32 num)
{
	int ret = 0, i;

	for (i = 0; i < num; i++) {
		if (seq[i].cmd) {
			ret = s6tnmr7_write(lcd, seq[i].cmd, seq[i].len);
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

static int s6tnmr7_read_id(struct lcd_info *lcd, u8 *buf)
{
	int ret;
	ret = s6tnmr7_read(lcd, LDI_ID_REG, buf, LDI_ID_LEN);

	if (ret < 1) {
		lcd->connected = 0;
		dev_info(&lcd->ld->dev, "panel is not connected well\n");
	}

	return ret;
}

static void s6tnmr7_read_coordinate(struct lcd_info *lcd)
{
	u8 coordinate_offset[2] = {0xB0, };
	unsigned char buf[4] = {0,};
	int coordinate_addr;

	coordinate_addr = LDI_COORDINATE_REG;
	coordinate_offset[1] = (u8) (coordinate_addr & 0xff);
	s6tnmr7_write(lcd, coordinate_offset, 2);
	s6tnmr7_read(lcd, (u8) (coordinate_addr >> 8), buf, 4);

	lcd->coordinate[0] = buf[0] << 8 | buf[1];	/* X */
	lcd->coordinate[1] = buf[2] << 8 | buf[3];	/* Y */
	smtd_dbg("coordinate value is %d,%d \n", lcd->coordinate[0], lcd->coordinate[1]);
}

static int s6tnmr7_read_code(struct lcd_info *lcd, u8 *buf)
{
	int ret;

	ret = s6tnmr7_read(lcd, LDI_CODE_REG, buf, LDI_CODE_LEN);

	if (ret < 1)
		dev_info(&lcd->ld->dev, "%s failed\n", __func__);

	return ret;
}

static int s6tnmr7_read_mtp(struct lcd_info *lcd, u8 *buf)
{
	int ret = 0, i;
	int mtp_addr[3];
	u8 mtp_offset[2] = {0xB0,};

	mtp_addr[0] = LDI_MTPR_REG;
	mtp_addr[1] = LDI_MTPG_REG;
	mtp_addr[2] = LDI_MTPB_REG;

	for (i = 0; i < CI_MAX; i++) {
		mtp_offset[1] = (u8) (mtp_addr[i] & 0xff);
		s6tnmr7_write(lcd, mtp_offset, 2);

		ret = s6tnmr7_read(lcd, (u8) (mtp_addr[i] >> 8),
			buf + i*MTP_VMAX, MTP_VMAX);

		if (ret < 1) {
		dev_err(&lcd->ld->dev, "%s failed\n", __func__);
			return ret;
		}
	}

	for (i = 0; i < LDI_MTP_LEN; i++)
		smtd_dbg("%02dth mtp value is %02x\n", i+1, (int)buf[i]);

	return LDI_MTP_LEN;
}

static int s6tnmr7_read_hbm(struct lcd_info *lcd, u8 *buf)
{
	int ret = 0, i;
	int hbm_addr[3];
	unsigned char hbm_offset[2] = {0xB0,};

	hbm_addr[0] = LDI_HBMR_REG;
	hbm_addr[1] = LDI_HBMG_REG;
	hbm_addr[2] = LDI_HBMB_REG;

	for (i = 0; i < CI_MAX; i++) {
		hbm_offset[1] = (u8) (hbm_addr[i] & 0xff);
		s6tnmr7_write(lcd, hbm_offset, 2);

		ret = s6tnmr7_read(lcd, (u8) (hbm_addr[i] >> 8),
			buf + i*LDI_HBM_LEN, LDI_HBM_LEN);

		if (ret < 1) {
			dev_err(&lcd->ld->dev, "%s failed\n", __func__);
			return ret;
		}
	}

/*VT set 0 */
	buf[10] = 0;
	buf[21] = 0;
	buf[32] = 0;

	for (i = 0; i < LDI_HBM_MAX; i++)
		smtd_dbg("%02dth hbm value is %02x\n", i+1, (int)buf[i]);

	return ret;
}

static int s6tnmr7_read_hbmelvss(struct lcd_info *lcd, u8 *buf)
{
	int ret, i;
	int elvss_addr;
	u8 elvss_offset[2] = {0xB0, };

	elvss_addr = LDI_HBMELVSSOFF_REG;
	elvss_offset[1] = (u8) (elvss_addr & 0xff);
	s6tnmr7_write(lcd, elvss_offset, 2);
	ret = s6tnmr7_read(lcd, (u8) (elvss_addr >> 8), buf, 1);

	elvss_addr = LDI_HBMELVSSON_REG;
	elvss_offset[1] = (u8) (elvss_addr & 0xff);
	s6tnmr7_write(lcd, elvss_offset, 2);
	ret += s6tnmr7_read(lcd, (u8) (elvss_addr >> 8), buf+1, 1);

	for (i = 0; i < 2; i++)
		smtd_dbg("%02dth hbmelvss value is %02x\n", i+1, (int)buf[i]);

	return ret;
}

static int s6tnmr7_read_elvss(struct lcd_info *lcd, u8 *buf)
{
	int ret, i;

	ret = s6tnmr7_read(lcd, LDI_ELVSS_REG, buf, LDI_ELVSS_LEN);

	smtd_dbg("%s: %02xh\n", __func__, LDI_ELVSS_REG);
	for (i = 0; i < LDI_ELVSS_LEN; i++)
		smtd_dbg("%02dth value is %02x\n", i+1, (int)buf[i]);

	return ret;
}
static int init_backlight_level_from_brightness(struct lcd_info *lcd)
{
	int i, j, gamma;

	lcd->gamma_level = kzalloc((MAX_BRIGHTNESS+1) * sizeof(int), GFP_KERNEL); //0~255 + HBM
	if (!lcd->gamma_level) {
		pr_err("failed to allocate gamma_level table\n");
		return -1;
		}

	/* 0~19 */
	i = 0;
	lcd->gamma_level[i++] = IBRIGHTNESS_2NT; // 0
	lcd->gamma_level[i++] = IBRIGHTNESS_2NT; // 1
	lcd->gamma_level[i++] = IBRIGHTNESS_2NT; // 2
	lcd->gamma_level[i++] = IBRIGHTNESS_3NT; // 3
	lcd->gamma_level[i++] = IBRIGHTNESS_4NT;
	lcd->gamma_level[i++] = IBRIGHTNESS_5NT;
	lcd->gamma_level[i++] = IBRIGHTNESS_6NT;
	lcd->gamma_level[i++] = IBRIGHTNESS_7NT;
	lcd->gamma_level[i++] = IBRIGHTNESS_8NT;
	lcd->gamma_level[i++] = IBRIGHTNESS_9NT;
	lcd->gamma_level[i++] = IBRIGHTNESS_10NT;
	lcd->gamma_level[i++] = IBRIGHTNESS_11NT;
	lcd->gamma_level[i++] = IBRIGHTNESS_12NT;
	lcd->gamma_level[i++] = IBRIGHTNESS_13NT;
	lcd->gamma_level[i++] = IBRIGHTNESS_14NT;
	lcd->gamma_level[i++] = IBRIGHTNESS_15NT;
	lcd->gamma_level[i++] = IBRIGHTNESS_16NT;
	lcd->gamma_level[i++] = IBRIGHTNESS_17NT;
	lcd->gamma_level[i++] = IBRIGHTNESS_17NT; //17~18 : 17NT
	lcd->gamma_level[i++] = IBRIGHTNESS_19NT; // 19

	lcd->gamma_level[i++] = IBRIGHTNESS_20NT;
	lcd->gamma_level[i++] = IBRIGHTNESS_21NT;
	lcd->gamma_level[i++] = IBRIGHTNESS_22NT;
	lcd->gamma_level[i++] = IBRIGHTNESS_22NT;
	lcd->gamma_level[i++] = IBRIGHTNESS_24NT;
	lcd->gamma_level[i++] = IBRIGHTNESS_25NT;
	lcd->gamma_level[i++] = IBRIGHTNESS_27NT;
	lcd->gamma_level[i++] = IBRIGHTNESS_27NT;
	lcd->gamma_level[i++] = IBRIGHTNESS_29NT;
	lcd->gamma_level[i++] = IBRIGHTNESS_30NT;
	lcd->gamma_level[i++] = IBRIGHTNESS_30NT;

	/* 255~20*/
	for(i = MAX_BRIGHTNESS; i >= LINEAR_MIN_GAMMA; i--) {
		gamma = ((i - LINEAR_MIN_GAMMA) * (300 - LINEAR_MIN_GAMMA) / (255 - LINEAR_MIN_GAMMA)) + LINEAR_MIN_GAMMA;
		for (j = IBRIGHTNESS_300NT; j >= 0; j--) {
			if (DIM_TABLE[j] < gamma)
			break;
			lcd->gamma_level[i] =j;
		}
	}
	return 0;
}

static int s6tnmr7_gamma_ctl(struct lcd_info *lcd)
{
	int ret = 0;

	ret = s6tnmr7_write(lcd, lcd->gamma_table[lcd->bl], GAMMA_PARAM_SIZE);
	if (!ret)
		ret = -EPERM;

	return ret;
}

static int s6tnmr7_aid_parameter_ctl(struct lcd_info *lcd, u8 force)
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
	ret = s6tnmr7_write(lcd, lcd->aor[lcd->bl], AID_PARAM_SIZE);
	if (!ret)
		ret = -EPERM;

exit:
	return ret;
}

static int s6tnmr7_set_acl(struct lcd_info *lcd, u8 force)
{
	int ret = 0, level = 0;

	level = ACL_STATUS_15P;

	if (lcd->siop_enable)
		goto acl_update;

	if ((!lcd->acl_enable) ||  LEVEL_IS_HBM(lcd->brightness))
		level = ACL_STATUS_0P;

acl_update:
	if (force || lcd->current_acl != ACL_CUTOFF_TABLE[level][1]) {
		s6tnmr7_write(lcd, SEQ_GLOBAL_PARAM_ACL, ARRAY_SIZE(SEQ_GLOBAL_PARAM_ACL));
		ret = s6tnmr7_write(lcd, ACL_CUTOFF_TABLE[level], ACL_PARAM_SIZE);

		s6tnmr7_write(lcd, SEQ_GLOBAL_PARAM_OPRAVR_CAL, 2);
		ret += s6tnmr7_write(lcd, SEQ_ACL_OPR_AVR_CAL, 2);

		s6tnmr7_write(lcd, SEQ_GLOBAL_PARAM_ACLUPDATE, 2);
		ret += s6tnmr7_write(lcd, SEQ_ACL_UPDATE, 2);

		lcd->current_acl = ACL_CUTOFF_TABLE[level][1];
		dev_info(&lcd->ld->dev, "acl: %d, brightness: %d\n", lcd->current_acl, lcd->brightness);
	}

	if (!ret)
		ret = -EPERM;

	return ret;
}

static int s6tnmr7_set_elvss(struct lcd_info *lcd, u8 force)
{
	int ret = 0, i, elvss_level;
	u32 nit;
	unsigned char SEQ_ELVSS_HBM[2] = {0xBB, };
	unsigned char SEQ_ELVSS[2] = {0xBB, };

	nit = DIM_TABLE[lcd->bl];
	elvss_level = ELVSS_STATUS_300;
	for (i = 0; i < ELVSS_STATUS_MAX; i++) {
		if (nit <= ELVSS_DIM_TABLE[i]) {
			elvss_level = i;
			break;
		}
	}

	if (lcd->temperature == TSET_MINUS_0_DEGREES)
		SEQ_ELVSS[1] = lcd->elvss_table[elvss_level][lcd->acl_enable];
	else
		SEQ_ELVSS[1] = lcd->elvss_table[elvss_level][lcd->acl_enable] - lcd->elvss_delta;

	if((force) || (lcd->current_elvss != SEQ_ELVSS[1])) {
		ret = s6tnmr7_write(lcd, SEQ_GLOBAL_PARAM_53RD, ARRAY_SIZE(SEQ_GLOBAL_PARAM_53RD));
		if (ret < 0)
			goto elvss_err;

		ret = s6tnmr7_write(lcd, SEQ_ELVSS, ELVSS_PARAM_SIZE);
		if (ret < 0)
			goto elvss_err;
		lcd->current_elvss = SEQ_ELVSS[1];
		dev_dbg(&lcd->ld->dev, "elvss_level = %d, SEQ_ELVSS_HBM = {%x, %x}\n", lcd->current_elvss, SEQ_ELVSS[0], SEQ_ELVSS[1]);
	}
	//HBM setting
	if (elvss_level == ELVSS_STATUS_HBM)
		SEQ_ELVSS_HBM[1] = lcd->elvss_hbm[1];
	else
		SEQ_ELVSS_HBM[1] = lcd->elvss_hbm[0];

	if ((force) || (lcd->current_hbm != SEQ_ELVSS_HBM[1])) {

		ret = s6tnmr7_write(lcd, SEQ_GLOBAL_PARAM_ELVSSHBM, ARRAY_SIZE(SEQ_GLOBAL_PARAM_ELVSSHBM));
		if (ret < 0)
			goto elvss_err;

		ret = s6tnmr7_write(lcd, SEQ_ELVSS_HBM, ARRAY_SIZE(SEQ_ELVSS_HBM));
		if (ret < 0)
			goto elvss_err;

		lcd->current_hbm = SEQ_ELVSS_HBM[1];
		dev_dbg(&lcd->ld->dev, "hbm elvss_level = %d, SEQ_ELVSS_HBM = {%x, %x}\n", elvss_level, SEQ_ELVSS_HBM[0], SEQ_ELVSS_HBM[1]);
	}

	return 0;

elvss_err:
	dev_err(&lcd->ld->dev, "elvss write error\n");
	return ret;
}

static void init_dynamic_aid(struct lcd_info *lcd)
{
	lcd->daid.vreg = pvregout_voltage_table[0];
	lcd->daid.vref_h = pvregout_voltage_table[1];
	lcd->daid.br_base = pbrightness_base_table;
	lcd->daid.gc_tbls = gamma_curve_tables;
	lcd->daid.offset_gra = poffset_gradation;
	lcd->daid.offset_color = poffset_color;

	lcd->daid.iv_tbl = index_voltage_table;
	lcd->daid.iv_max = IV_MAX;
	lcd->daid.mtp = kzalloc(IV_MAX * CI_MAX * sizeof(int), GFP_KERNEL);
	lcd->daid.gamma_default = gamma_default;
	lcd->daid.formular = gamma_formula;
	lcd->daid.vt_voltage_value = vt_voltage_value;

	lcd->daid.ibr_tbl = index_brightness_table;
	lcd->daid.ibr_max = IBRIGHTNESS_MAX -1; //except hbm state

	lcd->daid.gc_lut = gamma_curve_lut;
}

static void init_mtp_data(struct lcd_info *lcd, u8 *mtp_data)
{
	int i, c, j;
	int *mtp;
	int mtp_v0[3];

	mtp = lcd->daid.mtp;


	for (c = 0; c < CI_MAX ; c++) {
		for (i = IV_11, j = 0; i < IV_MAX; i++, j++)
			mtp[i*CI_MAX + c] = mtp_data[MTP_VMAX*c + j];

		mtp[IV_3*CI_MAX + c] = mtp_data[MTP_VMAX*c + j++];
		mtp_v0[c] = mtp_data[MTP_VMAX*c + j++];
		mtp[IV_VT*CI_MAX + c] = mtp_data[MTP_VMAX*c + j++];
	}

	for (c = 0; c < CI_MAX ; c++) {
		for (i = IV_3, j = 0; i <= IV_203; i++, j++)
			if (mtp[i*CI_MAX + c] & 0x80) {
				mtp[i*CI_MAX + c] = mtp[i*CI_MAX + c] & 0x7f;
				mtp[i*CI_MAX + c] *= (-1);
		}
		if (mtp_v0[c] & 0x80)
			mtp[IV_255*CI_MAX + c] *= (-1);
	}

	for (i = 0, j = 0; i <= IV_MAX; i++)
		for (c = 0; c < CI_MAX; c++, j++)
			smtd_dbg("mtp_data[%d] = %d\n", j, mtp_data[j]);

	for (i = 0, j = 0; i < IV_MAX; i++)
		for (c = 0; c < CI_MAX; c++, j++)
			smtd_dbg("mtp[%d] = %d\n", j, mtp[j]);
}

static int init_gamma_table(struct lcd_info *lcd , u8 *mtp_data)
{
	int i, c, j, v;
	int ret = 0;
	int *pgamma;
	int **gamma;
	unsigned char	value;

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
		lcd->gamma_table[i][0] = LDI_GAMMA_REG;
	}

	/* calculate gamma table */
	init_mtp_data(lcd, mtp_data);
	dynamic_aid(lcd->daid, gamma);

	/* relocate gamma order */
	for (i = 0; i < IBRIGHTNESS_MAX - 1; i++) {
		/* Brightness table */
		for (c = 0, j = 1; c < CI_MAX; c++, pgamma++) {
			for (v = IV_11; v < IV_MAX; v++) {
				pgamma = &gamma[i][v * CI_MAX + c];
				value = (char)((*pgamma) & 0xff);
				lcd->gamma_table[i][j++] = value;
			}
			pgamma = &gamma[i][IV_3 * CI_MAX + c];
			value = (char)((*pgamma) & 0xff);
			lcd->gamma_table[i][j++] = value;

			pgamma = &gamma[i][IV_255 * CI_MAX + c];
			value = (*pgamma & 0x100) ? 0x80 : 0x00;
			lcd->gamma_table[i][j++] = value;

			pgamma = &gamma[i][IV_VT * CI_MAX + c];
			value = (char)((*pgamma) & 0xff);
			lcd->gamma_table[i][j++] = value;
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

static int init_hbm_parameter(struct lcd_info *lcd,
	const u8 *mtp_data, const u8 *hbm_data, const u8 *hbmelvss_data)
{
	int i;

	lcd->gamma_table[IBRIGHTNESS_HBM][0] = LDI_GAMMA_REG;

	/* C8 34~39, 73~87 -> CA 1~21 */
	for (i = 0; i < LDI_HBM_MAX; i++)
		lcd->gamma_table[IBRIGHTNESS_HBM][i + 1] = hbm_data[i];

	lcd->elvss_hbm[0] = hbmelvss_data[0];
	lcd->elvss_hbm[1] = hbmelvss_data[1];
	pr_info("%s: elvss_hbm[0] = %d, elvss_hbm[1] = %d\n", __func__, lcd->elvss_hbm[0], lcd->elvss_hbm[1]);

	return 0;

}

static int init_aid_dimming_table(struct lcd_info *lcd)
{
	int i, j;
	pSEQ_AOR_CONTROL = SEQ_AOR_CONTROL;

	for (i = 0; i < IBRIGHTNESS_MAX; i++)
		memcpy(lcd->aor[i], pSEQ_AOR_CONTROL, ARRAY_SIZE(SEQ_AOR_CONTROL));

	for (i = 0; i < IBRIGHTNESS_MAX -1; i++) {
		lcd->aor[i][1] = paor_cmd[i][1];
		lcd->aor[i][2] = paor_cmd[i][2];
	}

	for (i = 0; i < IBRIGHTNESS_MAX; i++) {
		for (j = 0; j < ARRAY_SIZE(SEQ_AOR_CONTROL); j++)
			smtd_dbg("%02X ", lcd->aor[i][j]);
		smtd_dbg("\n");
	}

	return 0;
}

static int init_elvss_table(struct lcd_info *lcd, u8* elvss_data)
{
	int i, j, ret = 0;

	lcd->elvss_table = kzalloc(ELVSS_STATUS_MAX * sizeof(u8 *), GFP_KERNEL);

	if (IS_ERR_OR_NULL(lcd->elvss_table)) {
		pr_err("failed to allocate elvss table\n");
		ret = -ENOMEM;
		goto err_alloc_elvss_table;
	}

	for (i = 0; i < ELVSS_STATUS_MAX; i++) {
		lcd->elvss_table[i] = kzalloc(ELVSS_TABLE_NUM * sizeof(u8), GFP_KERNEL);
		if (IS_ERR_OR_NULL(lcd->elvss_table[i])) {
			pr_err("failed to allocate elvss\n");
			ret = -ENOMEM;
			goto err_alloc_elvss;
		}
		lcd->elvss_table[i][0] = pELVSS_TABLE[i][0];
		lcd->elvss_table[i][1] = pELVSS_TABLE[i][1];
	}

	for (i = 0; i < ELVSS_STATUS_MAX; i++) {
		for (j = 0; j < ELVSS_TABLE_NUM; j++)
			smtd_dbg("0x%02x, ", lcd->elvss_table[i][j]);
		smtd_dbg("\n");
	}
	lcd->elvss_delta = *pelvss_delta;

	return 0;

err_alloc_elvss:
	while (i > 0) {
		kfree(lcd->elvss_table[i-1]);
		i--;
	}
	kfree(lcd->elvss_table);
err_alloc_elvss_table:
	return ret;
}


static int update_brightness(struct lcd_info *lcd, u8 force)
{
	mutex_lock(&lcd->bl_lock);

	lcd->brightness = lcd->bd->props.brightness;

	lcd->bl = lcd->gamma_level[lcd->brightness > MAX_BRIGHTNESS ? MAX_BRIGHTNESS : lcd->brightness];

	if (LEVEL_IS_HBM(lcd->brightness))
		lcd->bl = IBRIGHTNESS_HBM;

	if (force || (lcd->ldi_enable && (lcd->current_bl != lcd->bl))) {
		s6tnmr7_gamma_ctl(lcd);
		s6tnmr7_aid_parameter_ctl(lcd, force);
		s6tnmr7_set_elvss(lcd, force);
		s6tnmr7_set_acl(lcd, force);
		s6tnmr7_write(lcd, SEQ_GLOBAL_PARAM_47RD, ARRAY_SIZE(SEQ_GLOBAL_PARAM_47RD));
		s6tnmr7_write(lcd, SEQ_GAMMA_UPDATE, ARRAY_SIZE(SEQ_GAMMA_UPDATE));

		lcd->current_bl = lcd->bl;

		dev_info(&lcd->ld->dev, "brightness=%d, bl=%d, candela=%d\n",
			lcd->brightness, lcd->bl, index_brightness_table[lcd->bl]);
	}

	mutex_unlock(&lcd->bl_lock);

	return 0;
}

static int s6tnmr7_tsp_te_enable(struct lcd_info *lcd, int onoff)
{
	int ret;

	ret = s6tnmr7_write(lcd, SEQ_TSP_TE_B0, ARRAY_SIZE(SEQ_TSP_TE_B0));
	if (ret < 0)
		goto te_err;

	ret = s6tnmr7_write(lcd, SEQ_TSP_TE_EN, ARRAY_SIZE(SEQ_TSP_TE_EN));
	if (ret < 0)
		goto te_err;

	return ret;

te_err:
	dev_err(&lcd->ld->dev, "%s onoff=%d fail\n", __func__, onoff);
	return ret;
}

#if defined(CONFIG_DECON_MDNIE_LITE_CHAGALL)
static int s6tnmr7_mdnie_enable(struct lcd_info *lcd, int onoff)
{
	int ret;

	ret = s6tnmr7_write(lcd, SEQ_MDNIE_EN_B0, ARRAY_SIZE(SEQ_MDNIE_EN_B0));
	if (ret < 0)
		goto enable_err;

	ret = s6tnmr7_write(lcd, SEQ_MDNIE_EN, ARRAY_SIZE(SEQ_MDNIE_EN));
	if (ret < 0)
		goto enable_err;

	return ret;

enable_err:
	dev_err(&lcd->ld->dev, "%s onoff=%d fail\n", __func__, onoff);
	return ret;
}

int s6tnmr7_mdnie_read(struct device *dev, u8 addr, u8 *buf, u32 len)
{
	struct lcd_info *lcd = dev_get_drvdata(dev);
	unsigned char addr_buf[LDI_PARAM_LSB_SIZE];
	int ret;

	dev_info(&lcd->ld->dev, "%s\n", __func__);

	/* Offset addr */
	addr_buf[0] = LDI_PARAM_LSB;
	addr_buf[1] = (unsigned char) (lcd->mdnie_addr & 0xff);
	ret = s6tnmr7_write(lcd, addr_buf, LDI_PARAM_LSB_SIZE);
	if (ret < 2)
		return -EINVAL;
	msleep(120);

	/* Base addr & read data */
	ret = s6tnmr7_read(lcd, addr, buf, len);
	if (ret < 1) {
		dev_info(&lcd->ld->dev, "panel is not connected well\n");
	}

	return ret;
}

int s6tnmr7_mdnie_write(struct device *dev, const u8 *seq, u32 len)
{
	struct lcd_info *lcd = dev_get_drvdata(dev);
	unsigned char addr_buf[LDI_PARAM_LSB_SIZE];
	int ret;
	u8 start_addr;

	/* lock/unlock key */
	if (!len)
		return 0;

	/* check base address */
	start_addr = ((lcd->mdnie_addr >> 8) & 0xff) + LDI_PARAM_MSB;
	if (seq[0] != start_addr) {
		dev_err(&lcd->ld->dev, "Invalid mdnie address (%x, %x)\n",
				start_addr, seq[0]);
		return -EFAULT;
	}
	mutex_lock(&lcd->bl_lock);

	/* Offset addr */
	addr_buf[0] = LDI_PARAM_LSB;
	addr_buf[1] = (unsigned char) (lcd->mdnie_addr & 0xff);
	ret = s6tnmr7_write(lcd, addr_buf, LDI_PARAM_LSB_SIZE);
	if (ret < 0) {
		dev_err(&lcd->ld->dev, "%s failed LDI_PARAM_LSB\n", __func__);
		mutex_unlock(&lcd->bl_lock);
		return -EINVAL;
	}

	/* Base addr & read data */
	ret = s6tnmr7_write(lcd, seq, len);
	if (ret < 0) {
		dev_err(&lcd->ld->dev, "%s failed data\n", __func__);
		mutex_unlock(&lcd->bl_lock);
		return -EINVAL;
	}
	mutex_unlock(&lcd->bl_lock);

	msleep(17);  /* wait 1 frame */

	return len;
}

int s6tnmr7_mdnie_set_addr(struct device *dev, int mdnie_addr)
{
	struct lcd_info *lcd = dev_get_drvdata(dev);

	lcd->mdnie_addr = mdnie_addr;

	return 0;
}
#endif

/*To convert from video+psr to command mode */
static void mipi_mode_conversion(struct lcd_info *lcd)
{
	s6tnmr7_write(lcd,SEQ_GLOBAL_PARAM_87RD, ARRAY_SIZE(SEQ_GLOBAL_PARAM_87RD));
	s6tnmr7_write(lcd,SEQ_MODE_SET, ARRAY_SIZE(SEQ_MODE_SET));
	s6tnmr7_write(lcd,SEQ_GLOBAL_PARAM_5TH, ARRAY_SIZE(SEQ_GLOBAL_PARAM_5TH));
	s6tnmr7_write(lcd,SEQ_VSYNC_SET, ARRAY_SIZE(SEQ_VSYNC_SET));
}

static int s6tnmr7_ldi_init(struct lcd_info *lcd)
{
	int ret = 0;

	lcd->connected = 1;

	usleep_range(5000, 10000);

	s6tnmr7_read_id(lcd, lcd->id);
	dev_info(&lcd->ld->dev," %s : id [%x] [%x] [%x] \n", __func__,
		lcd->id[0], lcd->id[1], lcd->id[2]);

	update_brightness(lcd, 1);

#if defined(CONFIG_DECON_MDNIE_LITE_CHAGALL)
	if (!lcd->err_count)
		s6tnmr7_mdnie_enable(lcd, 1);
#endif
	s6tnmr7_tsp_te_enable(lcd, 1);
/*To convert from video+psr to command mode */
	mipi_mode_conversion(lcd);

	return ret;
}

static int s6tnmr7_ldi_enable(struct lcd_info *lcd)
{
	int ret = 0;

	s6tnmr7_write(lcd, SEQ_DISPLAY_ON, ARRAY_SIZE(SEQ_DISPLAY_ON));

	dev_info(&lcd->ld->dev, "DISPLAY_ON\n");

	return ret;
}

static int s6tnmr7_ldi_disable(struct lcd_info *lcd)
{
	int ret = 0;

	dev_info(&lcd->ld->dev, "+ %s\n", __func__);

	/* 2. Display Off (28h) */
	s6tnmr7_write(lcd, SEQ_DISPLAY_OFF, ARRAY_SIZE(SEQ_DISPLAY_OFF));

	dev_info(&lcd->ld->dev, "DISPLAY_OFF\n");

	/* 3. Sleep In (10h) */
	s6tnmr7_write(lcd, SEQ_SLEEP_IN, ARRAY_SIZE(SEQ_SLEEP_IN));

	/* 4. Wait 120ms */
	msleep(120);

	dev_info(&lcd->ld->dev, "- %s\n", __func__);

	return ret;
}

#ifdef CONFIG_LCD_ALPM
static int s6tnmr7_ldi_alpm_init(struct lcd_info *lcd)
{
	int ret = 0;

	s6tnmr7_write_set(lcd, SEQ_ALPM_ON_SET, ARRAY_SIZE(SEQ_ALPM_ON_SET));

	/* ALPM MODE */
	lcd->current_alpm = 1;

	return ret;
}

static int s6tnmr7_ldi_alpm_exit(struct lcd_info *lcd)
{
	int ret = 0;

	s6tnmr7_write_set(lcd, SEQ_ALPM_OFF_SET, ARRAY_SIZE(SEQ_ALPM_OFF_SET));

	/* NORMAL MODE */
	lcd->current_alpm = lcd->dsim->lcd_alpm = 0;

	return ret;
}
#endif

static int s6tnmr7_power_on(struct lcd_info *lcd)
{
	int ret = 0;

	dev_info(&lcd->ld->dev, "+ %s\n", __func__);

	ret = s6tnmr7_ldi_init(lcd);
	if (ret) {
		dev_err(&lcd->ld->dev, "failed to initialize ldi.\n");
		goto err;
	}

	ret = s6tnmr7_ldi_enable(lcd);
	if (ret) {
		dev_err(&lcd->ld->dev, "failed to enable ldi.\n");
		goto err;
	}

	mutex_lock(&lcd->bl_lock);
	lcd->ldi_enable = 1;
	mutex_unlock(&lcd->bl_lock);

	update_brightness(lcd, 1);

	dev_info(&lcd->ld->dev, "- %s\n", __func__);
err:
	return ret;
}

static int s6tnmr7_power_off(struct lcd_info *lcd)
{
	int ret = 0;

	dev_info(&lcd->ld->dev, "+ %s\n", __func__);

	mutex_lock(&lcd->bl_lock);
	lcd->ldi_enable = 0;
	mutex_unlock(&lcd->bl_lock);

#ifdef CONFIG_LCD_ALPM
	if (lcd->current_alpm && lcd->alpm) {
		lcd->dsim->lcd_alpm = 1;
		dev_info(&lcd->ld->dev, "%s : ALPM mode\n", __func__);
	} else
		ret = s6tnmr7_ldi_disable(lcd);
#else
	ret = s6tnmr7_ldi_disable(lcd);
#endif

	dev_info(&lcd->ld->dev, "- %s\n", __func__);

	return ret;
}

static int s6tnmr7_power(struct lcd_info *lcd, int power)
{
	int ret = 0;

	if (POWER_IS_ON(power) && !POWER_IS_ON(lcd->power))
		ret = s6tnmr7_power_on(lcd);
	else if (!POWER_IS_ON(power) && POWER_IS_ON(lcd->power))
		ret = s6tnmr7_power_off(lcd);

	if (!ret)
		lcd->power = power;

	return ret;
}

static int s6tnmr7_set_power(struct lcd_device *ld, int power)
{
	struct lcd_info *lcd = lcd_get_data(ld);

	if (power != FB_BLANK_UNBLANK && power != FB_BLANK_POWERDOWN &&
		power != FB_BLANK_NORMAL) {
		dev_err(&lcd->ld->dev, "power value should be 0, 1 or 4.\n");
		return -EINVAL;
	}

	return s6tnmr7_power(lcd, power);
}

static int s6tnmr7_get_power(struct lcd_device *ld)
{
	struct lcd_info *lcd = lcd_get_data(ld);

	return lcd->power;
}

static int s6tnmr7_check_fb(struct lcd_device *ld, struct fb_info *fb)
{
	return 0;
}

static int s6tnmr7_get_brightness(struct backlight_device *bd)
{
	struct lcd_info *lcd = bl_get_data(bd);

	return DIM_TABLE[lcd->bl];
}

static int s6tnmr7_set_brightness(struct backlight_device *bd)
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

static struct lcd_ops s6tnmr7_lcd_ops = {
	.set_power = s6tnmr7_set_power,
	.get_power = s6tnmr7_get_power,
	.check_fb  = s6tnmr7_check_fb,
};

static const struct backlight_ops s6tnmr7_backlight_ops = {
	.get_brightness = s6tnmr7_get_brightness,
	.update_status = s6tnmr7_set_brightness,
	.check_fb = check_fb_brightness,
};

#if defined(CONFIG_DECON_MDNIE_LITE_CHAGALL)
static struct mdnie_ops s6tnmr7_mdnie_ops = {
	.write = s6tnmr7_mdnie_write,
	.read = s6tnmr7_mdnie_read,
	.set_addr = s6tnmr7_mdnie_set_addr,
};
#endif

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

static ssize_t brightness_table_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	int i, bl;
	char *pos = buf;
	struct lcd_info *lcd = dev_get_drvdata(dev);

	for (i = 0; i <= EXTEND_BRIGHTNESS; i++) {
		bl = lcd->gamma_level[i > MAX_BRIGHTNESS ? MAX_BRIGHTNESS : i];
		bl = LEVEL_IS_HBM(i) ? IBRIGHTNESS_HBM : bl;
		pos += sprintf(pos, "%3d %3d\n", i, DIM_TABLE[bl]);
	}

	return pos - buf;
}

static ssize_t gamma_table_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct lcd_info *lcd = dev_get_drvdata(dev);
	int i, j;

	for (i = 0; i < IBRIGHTNESS_MAX; i++) {
		for (j = 0; j < GAMMA_PARAM_SIZE; j++)
			printk("0x%02x, ", lcd->gamma_table[i][j]);
		printk("\n");
	}

	printk("ELVSS Table\n acl_off: ");
	for (i = 0; i < ELVSS_STATUS_MAX; i++)
		printk("0x%02x, ", lcd->elvss_table[i][0]);
	printk("\n acl_on: ");
	for (i = 0; i < ELVSS_STATUS_MAX; i++)
		printk("0x%02x, ", lcd->elvss_table[i][1]);
	printk("\n");

	return strlen(buf);
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
	int value, rc, temperature = 0 ;

	rc = kstrtoint(buf, 10, &value);

	if (rc < 0)
		return rc;
	else {
		switch (value) {
		case 1:
		case 0:
		case -19:
			temperature = value;
			break;
		case -20:
			temperature = value;
			break;
		}

		mutex_lock(&lcd->bl_lock);
		lcd->temperature = temperature;
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

static DEVICE_ATTR(power_reduce, 0664, power_reduce_show, power_reduce_store);
static DEVICE_ATTR(lcd_type, 0444, lcd_type_show, NULL);
static DEVICE_ATTR(window_type, 0444, window_type_show, NULL);
static DEVICE_ATTR(manufacture_code, 0444, manufacture_code_show, NULL);
static DEVICE_ATTR(gamma_table, 0444, gamma_table_show, NULL);
static DEVICE_ATTR(siop_enable, 0664, siop_enable_show, siop_enable_store);
static DEVICE_ATTR(temperature, 0664, temperature_show, temperature_store);
static DEVICE_ATTR(color_coordinate, 0444, color_coordinate_show, NULL);
static DEVICE_ATTR(manufacture_date, 0444, manufacture_date_show, NULL);
static DEVICE_ATTR(aid_log, 0444, aid_log_show, NULL);
static DEVICE_ATTR(lux, 0644, lux_show, lux_store);
static DEVICE_ATTR(brightness_table, 0444, brightness_table_show, NULL);

static int s6tnmr7_probe(struct mipi_dsim_device *dsim)
{
	int ret;
	struct lcd_info *lcd;

	u8 mtp_data[LDI_MTP_LEN] = {0,};
	u8 elvss_data[LDI_ELVSS_LEN] = {0,};
	u8 hbmelvss_data[2] = {0, 0};
	u8 hbm_data[LDI_HBM_MAX] = {0,};

	lcd = kzalloc(sizeof(struct lcd_info), GFP_KERNEL);
	if (!lcd) {
		pr_err("failed to allocate for lcd\n");
		ret = -ENOMEM;
		goto err_alloc;
	}

	lcd->ld = lcd_device_register("panel", dsim->dev, lcd, &s6tnmr7_lcd_ops);
	if (IS_ERR(lcd->ld)) {
		pr_err("failed to register lcd device\n");
		ret = PTR_ERR(lcd->ld);
		goto out_free_lcd;
	}
	dsim->lcd = lcd->ld;

	lcd->bd = backlight_device_register("panel", dsim->dev, lcd, &s6tnmr7_backlight_ops, NULL);
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
	lcd->temperature = 1;
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

/*	ret = device_create_file(&lcd->ld->dev, &dev_attr_partial_disp);
	if (ret < 0)
		dev_err(&lcd->ld->dev, "failed to add sysfs entries, %d\n", __LINE__);
*/
	ret = device_create_file(&lcd->ld->dev, &dev_attr_aid_log);
	if (ret < 0)
		dev_err(&lcd->ld->dev, "failed to add sysfs entries, %d\n", __LINE__);

	ret = device_create_file(&lcd->ld->dev, &dev_attr_lux);
	if (ret < 0)
		dev_err(&lcd->ld->dev, "failed to add sysfs entries, %d\n", __LINE__);

	ret = device_create_file(&lcd->bd->dev, &dev_attr_brightness_table);
	if (ret < 0)
		dev_err(&lcd->ld->dev, "failed to add sysfs entries, %d\n", __LINE__);

	mutex_init(&lcd->lock);
	mutex_init(&lcd->bl_lock);
	s6tnmr7_read_id(lcd, lcd->id);
	s6tnmr7_read_code(lcd, lcd->code);
	s6tnmr7_read_coordinate(lcd);
	s6tnmr7_read_mtp(lcd, mtp_data);
	s6tnmr7_read_elvss(lcd, elvss_data);

	dev_info(&lcd->ld->dev, "ID: %x, %x, %x\n", lcd->id[0], lcd->id[1], lcd->id[2]);

	ret = init_backlight_level_from_brightness(lcd);
	if(ret < 0)
		dev_info(&lcd->ld->dev, "gamma level generation is failed\n");
	init_dynamic_aid(lcd);

	ret = init_gamma_table(lcd, mtp_data);
	ret += init_aid_dimming_table(lcd);
	ret += init_elvss_table(lcd, elvss_data);

	s6tnmr7_read_hbmelvss(lcd, hbmelvss_data);
	s6tnmr7_read_hbm(lcd, hbm_data);
	ret += init_hbm_parameter(lcd, mtp_data, hbm_data, hbmelvss_data);

	if (ret)
		dev_info(&lcd->ld->dev, "gamma table generation is failed\n");

	lcd->ldi_enable = 1;

#if defined(CONFIG_DECON_MDNIE_LITE_CHAGALL)
	lcd->md = mdnie_device_register("mdnie", &lcd->ld->dev, &s6tnmr7_mdnie_ops);
	lcd->mdnie_class = get_mdnie_class();
#endif

	update_brightness(lcd, 1);

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

static int s6tnmr7_displayon(struct mipi_dsim_device *dsim)
{
	struct lcd_info *lcd = dev_get_drvdata(&dsim->lcd->dev);

	s6tnmr7_power(lcd, FB_BLANK_UNBLANK);

	return 0;
}

static int s6tnmr7_suspend(struct mipi_dsim_device *dsim)
{
	struct lcd_info *lcd = dev_get_drvdata(&dsim->lcd->dev);

	s6tnmr7_power(lcd, FB_BLANK_POWERDOWN);

	return 0;
}

static int s6tnmr7_resume(struct mipi_dsim_device *dsim)
{
	return 0;
}

struct mipi_dsim_lcd_driver s6tnmr7_mipi_lcd_driver = {
	.probe		= s6tnmr7_probe,
	.displayon	= s6tnmr7_displayon,
	.suspend	= s6tnmr7_suspend,
	.resume		= s6tnmr7_resume,
};

static int s6tnmr7_init(void)
{
	return 0;
}

static void s6tnmr7_exit(void)
{
	return;
}

module_init(s6tnmr7_init);
module_exit(s6tnmr7_exit);

MODULE_DESCRIPTION("MIPI-DSI s6tnmr7 (2560#1600) Panel Driver");
MODULE_LICENSE("GPL");

