/* linux/drivers/video/backlight/ea8061v_mipi_lcd.c
 *
 * Samsung SoC MIPI LCD driver.
 *
 * Copyright (c) 2012 Samsung Electronics
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

#undef SMART_DIMMING_DEBUG
#include "ea8061v_param.h"
#include "dynamic_aid_ea8061v.h"

#define MIN_BRIGHTNESS		0
#define MAX_BRIGHTNESS		255
#define DEFAULT_BRIGHTNESS		134

#define POWER_IS_ON(pwr)		(pwr <= FB_BLANK_NORMAL)
#define LEVEL_IS_HBM(level)		(level >= 6)
#define LEVEL_IS_PSRE(level)		(level >= 6)

#define NORMAL_TEMPERATURE	25	/* 25 ¡ÆC */

#define MIN_GAMMA			2
#define MAX_GAMMA			350
#define DEFAULT_GAMMA_INDEX		GAMMA_183CD

#define LDI_ID_REG			0x04
#define LDI_ID_LEN			3
#define LDI_MTP_REG			0xC8
#define LDI_MTP_LEN			57	/* MTP */
#define LDI_ELVSS_REG			0xB6
#define LDI_ELVSS_LEN			4
#define LDI_TSET_REG			0xB8
#define LDI_TSET_LEN			1

#define LDI_COORDINATE_REG		0xD8
#define LDI_COORDINATE_LEN		7

#ifdef SMART_DIMMING_DEBUG
#define smtd_dbg(format, arg...)	printk(format, ##arg)
#else
#define smtd_dbg(format, arg...)
#endif

struct lcd_info {
	unsigned int			bl;
	unsigned int			auto_brightness;
	unsigned int			acl_enable;
	unsigned int			siop_enable;
	unsigned int			current_acl;
	unsigned int			current_bl;
	unsigned int			current_elvss;
	unsigned int			current_psre;
	unsigned int			current_tset;
	unsigned int			elvss_compensation;
	unsigned int			elvss_status_max;
	unsigned int			elvss_status_hbm;
	unsigned int			elvss_status_350;
	unsigned int			ldi_enable;
	unsigned int			power;
	struct mutex			lock;
	struct mutex			bl_lock;

	struct device			*dev;
	struct lcd_device		*ld;
	struct mipi_dsim_lcd_device	*dsim_dev;
	struct backlight_device		*bd;
	unsigned char			id[LDI_ID_LEN];

	unsigned char			**gamma_table;
	unsigned char			**elvss_table[2];
	unsigned char			elvss_hbm[2][ELVSS_PARAM_SIZE + 1];
	struct dynamic_aid_param_t daid;
	unsigned char			aor[GAMMA_MAX][ARRAY_SIZE(SEQ_AID_SET)];
	unsigned int			connected;

	unsigned char			tset_table[LDI_TSET_LEN + 1];
	int				temperature;
	int				current_temp;

	unsigned int			coordinate[2];
	unsigned char			date[2];
	unsigned int			need_update;

	struct mipi_dsim_device		*dsim;
};

static struct lcd_info *g_lcd;
static int ea8061v_write(struct lcd_info *lcd, const u8 *seq, u32 len)
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
		dev_err(&lcd->ld->dev, "%s failed: exceed retry count\n", __func__);
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

static int ea8061v_read(struct lcd_info *lcd, u8 addr, u8 *buf, u32 len)
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
	ret = s5p_mipi_dsi_rd_data(lcd->dsim, cmd, addr, len, buf,0);
	if (ret < 0) {
		dev_dbg(&lcd->ld->dev, "mipi_read failed retry ..\n");
		retry--;
		goto read_data;
	}
read_err:
	mutex_unlock(&lcd->lock);
	return ret;
}

static void ea8061v_read_coordinate(struct lcd_info *lcd)
{
	int ret;
	unsigned char buf[LDI_COORDINATE_LEN] = {0,};

//	unsigned char SEQ_COORDINATE_POSITION[] = {0xB0, 3, 0x00};

//	ea8061v_write(lcd, SEQ_COORDINATE_POSITION, ARRAY_SIZE(SEQ_COORDINATE_POSITION));

	ret = ea8061v_read(lcd, LDI_COORDINATE_REG, buf, LDI_COORDINATE_LEN);

	if (ret < 1)
		dev_err(&lcd->ld->dev, "%s failed\n", __func__);

	lcd->coordinate[0] = buf[0] << 8 | buf[1];	/* X */
	lcd->coordinate[1] = buf[2] << 8 | buf[3];	/* Y */
}

static void ea8061v_read_id(struct lcd_info *lcd, u8 *buf)
{
	int ret;

	ret = ea8061v_read(lcd, LDI_ID_REG, buf, LDI_ID_LEN);
	if (ret < 0) {
		lcd->connected = 0;
		dev_info(&lcd->ld->dev, "panel is not connected well\n");
	}
}

static int ea8061v_read_mtp(struct lcd_info *lcd, u8 *buf)
{
	int ret, i;

	ret = ea8061v_read(lcd, LDI_MTP_REG, buf, LDI_MTP_LEN);

	if (ret < 0)
		dev_err(&lcd->ld->dev, "%s failed\n", __func__);

		/* manufacture date */
	lcd->date[0] = buf[40];
	lcd->date[1] = buf[41];

	for (i = 0; i < LDI_MTP_LEN ; i++)
		smtd_dbg("%02dth mtp value is %02x\n", i+1, (int)buf[i]);

	return ret;
}

static int ea8061v_read_elvss(struct lcd_info *lcd, u8 *buf)
{
	int ret, i;

	ret = ea8061v_read(lcd, LDI_ELVSS_REG, buf, LDI_ELVSS_LEN);

	smtd_dbg("%s: %02xh\n", __func__, LDI_ELVSS_REG);
	for (i = 0; i < LDI_ELVSS_LEN; i++)
		smtd_dbg("%02dth value is %02x\n", i+1, (int)buf[i]);

	return ret;
}

#if 0
static int ea8061v_read_tset(struct lcd_info *lcd)
{
	int ret, i;

	lcd->tset_table[0] = LDI_TSET_REG;

	ret = ea8061v_read(lcd, LDI_TSET_REG, &lcd->tset_table[1], LDI_TSET_LEN);

	smtd_dbg("%s: %02xh\n", __func__, LDI_TSET_REG);
	for (i = 1; i <= LDI_TSET_LEN; i++)
		smtd_dbg("%02dth value is %02x\n", i, lcd->tset_table[i]);

	return ret;
}
#endif

static void  ea8061v_update_seq(struct lcd_info *lcd)
{
	if (lcd->id[2] == 0x83) {  /* Panel rev.A  */
		pSEQ_AID_SET = SEQ_AID_SET_RevA;
		paor_cmd = aor_cmd_RevA;
	} else  if ((lcd->id[2] == 0x84)||(lcd->id[2] == 0x95) ) { /* Panel rev.BC */
		paor_cmd = aor_cmd_RevBC;
	} else if (lcd->id[2] == 0x96) { /* Panel rev.D */
		paor_cmd = aor_cmd_RevD;
		pELVSS_DIM_TABLE = ELVSS_DIM_TABLE_RevDE;
		pELVSS_TABLE = ELVSS_TABLE_RevD;
	} else if (lcd->id[2] == 0x97) { /* Panel rev.E */
		paor_cmd = aor_cmd_RevE;
		pELVSS_DIM_TABLE = ELVSS_DIM_TABLE_RevDE;
		pELVSS_TABLE = ELVSS_TABLE_RevE;
	}
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


#if 0
static int ea8061v_set_psre(struct lcd_info *lcd, u8 force)
{
	int ret = 0;

	if (force || lcd->current_psre != LEVEL_IS_PSRE(lcd->auto_brightness)) {
		if (LEVEL_IS_PSRE(lcd->auto_brightness))
			ret = ea8061v_write(lcd, SEQ_PSRE_MODE_ON, ARRAY_SIZE(SEQ_PSRE_MODE_ON));
		else
			ret = ea8061v_write(lcd, SEQ_PSRE_MODE_OFF, ARRAY_SIZE(SEQ_PSRE_MODE_OFF));

		lcd->current_psre = LEVEL_IS_PSRE(lcd->auto_brightness);

		dev_info(&lcd->ld->dev, "psre: %d, auto_brightness: %d\n", lcd->current_psre, lcd->auto_brightness);
	}

	return ret;
}
#endif

static int ea8061v_gamma_ctl(struct lcd_info *lcd)
{
	ea8061v_write(lcd, lcd->gamma_table[lcd->bl], GAMMA_PARAM_SIZE);

	return 0;
}

static int ea8061v_aid_parameter_ctl(struct lcd_info *lcd, u8 force)
{
	if (force)
		goto aid_update;
	else if (lcd->aor[lcd->bl][3] !=  lcd->aor[lcd->current_bl][3])
		goto aid_update;
	else if (lcd->aor[lcd->bl][4] !=  lcd->aor[lcd->current_bl][4])
		goto aid_update;
	else
		goto exit;

aid_update:
	ea8061v_write(lcd, lcd->aor[lcd->bl], AID_PARAM_SIZE);

exit:

	return 0;
}

static int ea8061v_set_acl(struct lcd_info *lcd, u8 force)
{
	int ret = 0, level = ACL_STATUS_15P;

	if (lcd->siop_enable || LEVEL_IS_HBM(lcd->auto_brightness))
		goto acl_update;

	if (!lcd->acl_enable)
		level = ACL_STATUS_0P;

acl_update:
	if (force || lcd->current_acl != ACL_CUTOFF_TABLE[level][1]) {
		if (level == ACL_STATUS_15P)
			ea8061v_write(lcd, SEQ_ACL_OPR_32FRAME, ARRAY_SIZE(SEQ_ACL_OPR_32FRAME));
		else
			ea8061v_write(lcd, SEQ_ACL_OPR_16FRAME, ARRAY_SIZE(SEQ_ACL_OPR_16FRAME));
		ret = ea8061v_write(lcd, ACL_CUTOFF_TABLE[level], ACL_PARAM_SIZE);

		lcd->current_acl = ACL_CUTOFF_TABLE[level][1];
		dev_info(&lcd->ld->dev, "acl: %d, auto_brightness: %d\n", lcd->current_acl, lcd->auto_brightness);
	}


	if (!ret)
		ret = -EPERM;

	return ret;
}

static int ea8061v_set_elvss(struct lcd_info *lcd, u8 force)
{
	int ret = 0,i, elvss_level;
	u32 nit;
	u8 update_hbm = 0;
	u8 elvss, cur_elvss;
	//u8 cur_temp, temp;

	nit = DIM_TABLE[lcd->bl];
	elvss_level = lcd->elvss_status_350;

	for (i = 0; i < lcd->elvss_status_max ; i++) {
		if (nit <= pELVSS_DIM_TABLE[i]) {
			elvss_level = i;
			break;
		}
	}

	if ((lcd->current_elvss != elvss_level) &&
		(lcd->current_elvss == lcd->elvss_status_hbm || elvss_level == lcd->elvss_status_hbm))
		update_hbm = 1;
/*
	if (lcd->acl_enable)
		temp = (lcd->temperature > 0) ? 0x88 : 0x8C;
	else
		temp = (lcd->temperature > 0) ? 0x98 : 0x9C;

	cur_temp = lcd->elvss_table[lcd->acl_enable][lcd->current_elvss][1];*/
	cur_elvss = lcd->elvss_table[lcd->acl_enable][lcd->current_elvss][2];
//	lcd->elvss_table[lcd->acl_enable][elvss_level][1] = temp;
	elvss = lcd->elvss_table[lcd->acl_enable][elvss_level][2];

	if (cur_elvss != elvss)
		force = 1;

	if (force) {
		if (update_hbm) {
	//		lcd->elvss_hbm[0][1] = temp;
	//		lcd->elvss_hbm[1][1] = temp;
			lcd->elvss_hbm[0][2] = elvss;

			if (elvss_level == lcd->elvss_status_hbm)
				ret = ea8061v_write(lcd, lcd->elvss_hbm[1], ARRAY_SIZE(lcd->elvss_hbm[1]));
			else
				ret = ea8061v_write(lcd, lcd->elvss_hbm[0], ARRAY_SIZE(lcd->elvss_hbm[0]));
		} else
			ret = ea8061v_write(lcd, lcd->elvss_table[lcd->acl_enable][elvss_level], ELVSS_PARAM_SIZE);

		lcd->current_elvss = elvss_level;

		dev_dbg(&lcd->ld->dev, "elvss: %d, %d, %x\n", lcd->acl_enable, lcd->current_elvss,
			lcd->elvss_table[lcd->acl_enable][lcd->current_elvss][2]);
	}

	if (!ret) {
		ret = -EPERM;
		goto elvss_err;
	}

elvss_err:
	return ret;
}


static int ea8061v_set_tset(struct lcd_info *lcd, u8 force)
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
		lcd->tset_table[0]= LDI_TSET_REG;
		ret = ea8061v_write(lcd, lcd->tset_table, LDI_TSET_LEN + 1);
		lcd->current_temp = lcd->temperature;
		dev_info(&lcd->ld->dev, "temperature: %d, tset: %d %02x %02x\n",
				lcd->temperature, tset, lcd->tset_table[0], lcd->tset_table[1]);
	}

	if (!ret) {
		ret = -EPERM;
		goto err;
	}

err:
	return ret;
}

void init_dynamic_aid(struct lcd_info *lcd)
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
	lcd->daid.br_base = brightness_base_table;
	lcd->daid.gc_tbls = gamma_curve_tables;
	lcd->daid.gc_lut = gamma_curve_lut;

	 if (lcd->id[2] == 0x83) { /* Panel rev.A  */
		lcd->daid.offset_gra = offset_gradation_revA;
		lcd->daid.offset_color = offset_color_revA;
	} else if ((lcd->id[2] == 0x84)||(lcd->id[2] == 0x95)){ /* Panel rev.B.C  */
		lcd->daid.offset_gra = offset_gradation_revBC;
		lcd->daid.offset_color = offset_color_revBC;
	} else if (lcd->id[2] == 0x96){ /* Panel rev.D  */
		lcd->daid.offset_gra = offset_gradation_revD;
		lcd->daid.offset_color = offset_color_revD;
	} else if (lcd->id[2] == 0x97){ /* Panel rev.E  */
		lcd->daid.offset_gra = offset_gradation_revE;
		lcd->daid.offset_color = offset_color_revE;
	}
}

static void init_mtp_data(struct lcd_info *lcd, const u8 *mtp_data)
{
	int i, c, j;
	int *mtp;

	mtp = lcd->daid.mtp;

	for (c = 0, j = 0; c < CI_MAX ; c++, j++) {
		if (mtp_data[j++] & 0x01)
			mtp[(IV_MAX-1)*CI_MAX+c] = mtp_data[j] * (-1);
		else
			mtp[(IV_MAX-1)*CI_MAX+c] = mtp_data[j];
	}

	for (i = IV_MAX - 2; i >= 0; i--) {
		for (c=0; c<CI_MAX ; c++, j++) {
			if (mtp_data[j] & 0x80)
				mtp[CI_MAX*i+c] = (mtp_data[j] & 0x7F) * (-1);
			else
				mtp[CI_MAX*i+c] = mtp_data[j];
		}
	}

	for (i = 0, j = 0; i <= IV_MAX; i++)
		for (c=0; c<CI_MAX ; c++, j++)
			smtd_dbg("mtp_data[%d] = %d\n",j, mtp_data[j]);

	for (i = 0, j = 0; i < IV_MAX; i++)
		for (c=0; c<CI_MAX ; c++, j++)
			smtd_dbg("mtp[%d] = %d\n",j, mtp[j]);
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
		for (c = 0, j = 1; c < CI_MAX ; c++, pgamma++) {
			if (*pgamma & 0x100)
				lcd->gamma_table[i][j++] = 1;
			else
				lcd->gamma_table[i][j++] = 0;

			lcd->gamma_table[i][j++] = *pgamma & 0xff;
		}

		for (v = IV_MAX - 2; v >= 0; v--) {
			pgamma = &gamma[i][v * CI_MAX];
			for (c = 0; c < CI_MAX ; c++, pgamma++)
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

static int init_aid_dimming_table(struct lcd_info *lcd, const u8 *mtp_data)
{
	int i, j;

	for (i = 0; i < GAMMA_MAX -1; i++) {
		memcpy(lcd->aor[i], pSEQ_AID_SET, ARRAY_SIZE(SEQ_AID_SET));
		lcd->aor[i][3] = paor_cmd[i][0];
		lcd->aor[i][4] = paor_cmd[i][1];


		for (j = 0; j < ARRAY_SIZE(SEQ_AID_SET); j++)
			smtd_dbg("%02X ", lcd->aor[i][j]);
		smtd_dbg("\n");
	}

	return 0;
}

static int init_elvss_table(struct lcd_info *lcd, u8 *elvss_data)
{
	int i, j, k, ret;

	if (lcd->id[2] >= 0x96)
		lcd->elvss_status_max = ELVSS_STATUS_MAX+1; /* Rev D,E Panel */
	 else
		lcd->elvss_status_max = ELVSS_STATUS_MAX; /* Rev A,B,C Panel */

	 lcd->elvss_status_hbm = lcd->elvss_status_max -1;
	 lcd->elvss_status_350 = lcd->elvss_status_max -2;

	for (k = 0; k < 2; k++) {
		lcd->elvss_table[k] = kzalloc(lcd->elvss_status_max * sizeof(u8 *), GFP_KERNEL);

		if (IS_ERR_OR_NULL(lcd->elvss_table[k])) {
			pr_err("failed to allocate elvss table\n");
			ret = -ENOMEM;
			goto err_alloc_elvss_table;
		}

		for (i = 0; i < lcd->elvss_status_max; i++) {
			lcd->elvss_table[k][i] = kzalloc(ELVSS_PARAM_SIZE * sizeof(u8), GFP_KERNEL);
			if (IS_ERR_OR_NULL(lcd->elvss_table[k][i])) {
				pr_err("failed to allocate elvss\n");
				ret = -ENOMEM;
				goto err_alloc_elvss;
			}

			lcd->elvss_table[k][i][0] = 0xB6;
			if (k==0)
				lcd->elvss_table[k][i][1] = 0x5c;
			else
				lcd->elvss_table[k][i][1] = 0x4C;
			lcd->elvss_table[k][i][2] += pELVSS_TABLE[i];
		}

		for (i = 0; i < lcd->elvss_status_max; i++) {
			for (j = 0; j < ELVSS_PARAM_SIZE; j++)
				smtd_dbg("0x%02x, ", lcd->elvss_table[k][i][j]);
			smtd_dbg("\n");
		}
	}

	return 0;

err_alloc_elvss:
	/* should be kfree elvss with k */
	while (i > 0) {
		kfree(lcd->elvss_table[k][i-1]);
		i--;
	}
	kfree(lcd->elvss_table[k]);
err_alloc_elvss_table:
	return ret;
}

static int init_hbm_parameter(struct lcd_info *lcd, const u8 *mtp_data, const u8 *elvss_data)
{
	int i;

	for (i = 0; i < GAMMA_PARAM_SIZE; i++)
		lcd->gamma_table[GAMMA_HBM][i] = lcd->gamma_table[GAMMA_350CD][i];

	/* C8 : 34~39 -> CA : 1~6 */
	for (i = 0; i < 6; i++)
		lcd->gamma_table[GAMMA_HBM][1 + i] = mtp_data[33 + i];

	/* C8 : 43~57 -> CA : 7~21 */
	for (i = 0; i < 15; i++)
		lcd->gamma_table[GAMMA_HBM][7 + i] = mtp_data[42 + i];

	lcd->elvss_table[0][lcd->elvss_status_hbm][2] = 0x8A;
	lcd->elvss_table[1][lcd->elvss_status_hbm][2] = 0x8A;

	lcd->elvss_hbm[1][ELVSS_PARAM_SIZE] = mtp_data[39];
	lcd->elvss_hbm[0][ELVSS_PARAM_SIZE] = elvss_data[LDI_ELVSS_LEN - 1];

	return 0;

}

static int update_brightness(struct lcd_info *lcd, u8 force)
{
	u32 brightness;

	mutex_lock(&lcd->bl_lock);

	brightness = lcd->bd->props.brightness;

	lcd->bl = get_backlight_level_from_brightness(brightness);

	if (LEVEL_IS_HBM(lcd->auto_brightness) && (brightness == lcd->bd->props.max_brightness))
		lcd->bl = GAMMA_HBM;

	if ((force) || ((lcd->ldi_enable) && (lcd->current_bl != lcd->bl))) {
		ea8061v_write(lcd, SEQ_APPLY_MTP_KEY_UNLOCK, ARRAY_SIZE(SEQ_APPLY_MTP_KEY_UNLOCK));

		ea8061v_gamma_ctl(lcd);
		ea8061v_aid_parameter_ctl(lcd, force);
		ea8061v_set_tset(lcd, force);
		ea8061v_set_elvss(lcd, force);
		ea8061v_set_acl(lcd, force);
		ea8061v_write(lcd, SEQ_GAMMA_UPDATE, ARRAY_SIZE(SEQ_GAMMA_UPDATE));

		ea8061v_write(lcd, SEQ_APPLY_MTP_KEY_LOCK, ARRAY_SIZE(SEQ_APPLY_MTP_KEY_LOCK));

		/*ea8061v_set_psre(lcd, force);*/

		lcd->current_bl = lcd->bl;

		dev_info(&lcd->ld->dev, "brightness=%d, bl=%d, candela=%d\n", \
			brightness, lcd->bl, DIM_TABLE[lcd->bl]);
	}

	mutex_unlock(&lcd->bl_lock);

	return 0;
}

static int ea8061v_ldi_init(struct lcd_info *lcd)
{
	int ret = 0;
	lcd->need_update = 0;

	ea8061v_write(lcd, SEQ_APPLY_LEVEL_2_KEY_UNLOCK, ARRAY_SIZE(SEQ_APPLY_LEVEL_2_KEY_UNLOCK));
	ea8061v_write(lcd, SEQ_DCDC_SET, ARRAY_SIZE(SEQ_DCDC_SET));
	ea8061v_write(lcd, SEQ_SOURCE_CONTROL, ARRAY_SIZE(SEQ_SOURCE_CONTROL));
	ea8061v_write(lcd, SEQ_APPLY_LEVEL_3_KEY_UNLOCK, ARRAY_SIZE(SEQ_APPLY_LEVEL_3_KEY_UNLOCK));
	ea8061v_write(lcd, SEQ_GLOBAL_PARA_12TH, ARRAY_SIZE(SEQ_GLOBAL_PARA_12TH));
	ea8061v_write(lcd, SEQ_ERR_FG_OUTPUT_SET1, ARRAY_SIZE(SEQ_ERR_FG_OUTPUT_SET1));
	ea8061v_write(lcd, SEQ_GLOBAL_PARA_6TH, ARRAY_SIZE(SEQ_GLOBAL_PARA_6TH));
	ea8061v_write(lcd, SEQ_ERR_FG_OUTPUT_SET2, ARRAY_SIZE(SEQ_ERR_FG_OUTPUT_SET2));
	ea8061v_write(lcd, SEQ_DSI_ERR_OUT, ARRAY_SIZE(SEQ_DSI_ERR_OUT));
	ea8061v_write(lcd, SEQ_APPLY_LEVEL_3_KEY_LOCK, ARRAY_SIZE(SEQ_APPLY_LEVEL_3_KEY_LOCK));
	ea8061v_write(lcd, SEQ_SLEEP_OUT, ARRAY_SIZE(SEQ_SLEEP_OUT));
	mdelay(120);

	return ret;
}

static int ea8061v_ldi_enable(struct lcd_info *lcd)
{
	int ret = 0;
	ea8061v_write(lcd, SEQ_DISPLAY_ON, ARRAY_SIZE(SEQ_DISPLAY_ON));
	return ret;
}

static int ea8061v_ldi_disable(struct lcd_info *lcd)
{
	int ret = 0;

	dev_info(&lcd->ld->dev, "+ %s\n", __func__);

	ea8061v_write(lcd, SEQ_DISPLAY_OFF, ARRAY_SIZE(SEQ_DISPLAY_OFF));
	msleep(35);
	ea8061v_write(lcd, SEQ_SLEEP_IN, ARRAY_SIZE(SEQ_SLEEP_IN));
	msleep(120);

	dev_info(&lcd->ld->dev, "- %s\n", __func__);

	return ret;
}

static int ea8061v_power_on(struct lcd_info *lcd)
{
	int ret;

	dev_info(&lcd->ld->dev, "+ %s\n", __func__);

	ret = ea8061v_ldi_init(lcd);
	if (ret) {
		dev_err(&lcd->ld->dev, "failed to initialize ldi.\n");
		goto err;
	}

	ret = ea8061v_ldi_enable(lcd);
	if (ret) {
		dev_err(&lcd->ld->dev, "failed to enable ldi.\n");
		goto err;
	}

	mutex_lock(&lcd->bl_lock);
	lcd->ldi_enable = 1;
	mutex_unlock(&lcd->bl_lock);

	if (lcd->bl == 0&&lcd->auto_brightness!=6) /* Update_brightness not in HBM wakeup */ 
	   lcd->need_update |= 1;

	if (lcd->need_update)
	update_brightness(lcd, 1);

	dev_info(&lcd->ld->dev, "- %s\n", __func__);
err:
	return ret;
}

static int ea8061v_power_off(struct lcd_info *lcd)
{
	int ret;

	dev_info(&lcd->ld->dev, "+ %s\n", __func__);

	mutex_lock(&lcd->bl_lock);
	lcd->ldi_enable = 0;
	mutex_unlock(&lcd->bl_lock);

	ret = ea8061v_ldi_disable(lcd);

	dev_info(&lcd->ld->dev, "- %s\n", __func__);

	return ret;
}

static int ea8061v_power(struct lcd_info *lcd, int power)
{
	int ret = 0;

	if (POWER_IS_ON(power) && !POWER_IS_ON(lcd->power))
		ret = ea8061v_power_on(lcd);
	else if (!POWER_IS_ON(power) && POWER_IS_ON(lcd->power))
		ret = ea8061v_power_off(lcd);

	if (!ret)
		lcd->power = power;

	return ret;
}

static int ea8061v_set_power(struct lcd_device *ld, int power)
{
	struct lcd_info *lcd = lcd_get_data(ld);

	if (power != FB_BLANK_UNBLANK && power != FB_BLANK_POWERDOWN &&
		power != FB_BLANK_NORMAL) {
		dev_err(&lcd->ld->dev, "power value should be 0, 1 or 4.\n");
		return -EINVAL;
	}

	return ea8061v_power(lcd, power);
}

static int ea8061v_get_power(struct lcd_device *ld)
{
	struct lcd_info *lcd = lcd_get_data(ld);

	return lcd->power;
}


static int ea8061v_set_brightness(struct backlight_device *bd)
{
	int ret = 0;
	int brightness = bd->props.brightness;
	struct lcd_info *lcd = bl_get_data(bd);

	/* dev_info(&lcd->ld->dev, "%s: brightness=%d\n", __func__, brightness); */

	if (brightness < MIN_BRIGHTNESS ||
		brightness > bd->props.max_brightness) {
		dev_err(&bd->dev, "lcd brightness should be %d to %d. now %d\n",
			MIN_BRIGHTNESS, lcd->bd->props.max_brightness, brightness);
		return -EINVAL;
	}

	if (lcd->ldi_enable) {
		ret = update_brightness(lcd, 0);
		if (ret < 0) {
			dev_err(lcd->dev, "err in %s\n", __func__);
			return -EINVAL;
		}
	}
	else
		lcd->need_update = 1;

	return ret;
}

static int ea8061v_get_brightness(struct backlight_device *bd)
{
	struct lcd_info *lcd = bl_get_data(bd);

	return DIM_TABLE[lcd->bl];
}

static int ea8061v_check_fb(struct lcd_device *ld, struct fb_info *fb)
{
	return 0;
}

static struct lcd_ops ea8061v_lcd_ops = {
	.set_power = ea8061v_set_power,
	.get_power = ea8061v_get_power,
	.check_fb  = ea8061v_check_fb,
};

static const struct backlight_ops ea8061v_backlight_ops  = {
	.get_brightness = ea8061v_get_brightness,
	.update_status = ea8061v_set_brightness,
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

	sprintf(buf, "SDC_%02X%02X%02X\n", lcd->id[0], lcd->id[1], lcd->id[2]);

	return strlen(buf);
}

static ssize_t window_type_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct lcd_info *lcd = dev_get_drvdata(dev);
	char temp[15];

	if (lcd->ldi_enable)
		ea8061v_read_id(lcd, lcd->id);

	sprintf(temp, "%x %x %x\n", lcd->id[0], lcd->id[1], lcd->id[2]);

	strcat(buf, temp);
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

	for (i = 0; i < lcd->elvss_status_max; i++) {
		for (j = 0; j < ELVSS_PARAM_SIZE; j++)
			printk("0x%02x, ", lcd->elvss_table[0][i][j]);
		printk("\n");
	}

	return strlen(buf);
}

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
			dev_info(dev, "%s: %d, %d\n", __func__, lcd->auto_brightness, value);
			mutex_lock(&lcd->bl_lock);
			lcd->auto_brightness = value;
			mutex_unlock(&lcd->bl_lock);
			if (lcd->ldi_enable)
				update_brightness(lcd, 0);
		}
	}
	return size;
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


static DEVICE_ATTR(power_reduce, 0664, power_reduce_show, power_reduce_store);
static DEVICE_ATTR(lcd_type, 0444, lcd_type_show, NULL);
static DEVICE_ATTR(window_type, 0444, window_type_show, NULL);
static DEVICE_ATTR(gamma_table, 0444, gamma_table_show, NULL);
static DEVICE_ATTR(auto_brightness, 0644, auto_brightness_show, auto_brightness_store);
static DEVICE_ATTR(siop_enable, 0664, siop_enable_show, siop_enable_store);
static DEVICE_ATTR(temperature, 0664, temperature_show, temperature_store);
static DEVICE_ATTR(color_coordinate, 0444, color_coordinate_show, NULL);
static DEVICE_ATTR(manufacture_date, 0444, manufacture_date_show, NULL);
static DEVICE_ATTR(aid_log, 0444, aid_log_show, NULL);

static int ea8061v_probe(struct mipi_dsim_device *dsim)
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

	g_lcd = lcd;

	lcd->ld = lcd_device_register("panel", dsim->dev, lcd, &ea8061v_lcd_ops);
	if (IS_ERR(lcd->ld)) {
		pr_err("failed to register lcd device\n");
		ret = PTR_ERR(lcd->ld);
		goto out_free_lcd;
	}

	lcd->bd = backlight_device_register("panel", dsim->dev, lcd, &ea8061v_backlight_ops, NULL);
	if (IS_ERR(lcd->bd)) {
		pr_err("failed to register backlight device\n");
		ret = PTR_ERR(lcd->bd);
		goto out_free_backlight;
	}

	lcd->dev = dsim->dev;
	lcd->dsim = dsim;
	lcd->bd->props.max_brightness = MAX_BRIGHTNESS;
	lcd->bd->props.brightness = DEFAULT_BRIGHTNESS;
	lcd->bl = DEFAULT_GAMMA_INDEX;
	lcd->current_bl = lcd->bl;
	lcd->acl_enable = 0;
	lcd->current_acl = 0;
	lcd->power = FB_BLANK_UNBLANK;
	lcd->ldi_enable = 1;
	lcd->auto_brightness = 0;
	lcd->connected = 1;
	lcd->siop_enable = 0;
	lcd->temperature = 1;
	lcd->current_tset = TSET_25_DEGREES;

	ret = device_create_file(&lcd->ld->dev, &dev_attr_power_reduce);
	if (ret < 0)
		dev_err(&lcd->ld->dev, "failed to add sysfs entries, %d\n", __LINE__);

	ret = device_create_file(&lcd->ld->dev, &dev_attr_lcd_type);
	if (ret < 0)
		dev_err(&lcd->ld->dev, "failed to add sysfs entries, %d\n", __LINE__);

	ret = device_create_file(&lcd->ld->dev, &dev_attr_window_type);
	if (ret < 0)
		dev_err(&lcd->ld->dev, "failed to add sysfs entries, %d\n", __LINE__);

	ret = device_create_file(&lcd->ld->dev, &dev_attr_gamma_table);
	if (ret < 0)
		dev_err(&lcd->ld->dev, "failed to add sysfs entries, %d\n", __LINE__);

	ret = device_create_file(&lcd->bd->dev, &dev_attr_auto_brightness);
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

	ret = device_create_file(&lcd->ld->dev, &dev_attr_aid_log);
	if (ret < 0)
		dev_err(&lcd->ld->dev, "failed to add sysfs entries, %d\n", __LINE__);

	/* dev_set_drvdata(dsim->dev, lcd); */

	mutex_init(&lcd->lock);
	mutex_init(&lcd->bl_lock);

	ea8061v_read_id(lcd, lcd->id);
	msleep(20);
	ea8061v_update_seq(lcd);
	ea8061v_read_coordinate(lcd);
	msleep(20);
	ea8061v_read_mtp(lcd, mtp_data);
	msleep(20);
	ea8061v_read_elvss(lcd, elvss_data);

	pr_info("[OCTA] Panel ID : %x, %x, %x\n", lcd->id[0], lcd->id[1], lcd->id[2]);

	init_dynamic_aid(lcd);

	ret = init_gamma_table(lcd, mtp_data);
	ret += init_aid_dimming_table(lcd, mtp_data);
	ret += init_elvss_table(lcd, elvss_data);
	ret += init_hbm_parameter(lcd, mtp_data, elvss_data);

	printk("ea8061v_probe ret value =%d\n", ret);

	if (ret)
		dev_info(&lcd->ld->dev, "gamma table generation is failed\n");

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

static int ea8061v_displayon(struct mipi_dsim_device *dsim)
{
	struct lcd_info *lcd = g_lcd;

	ea8061v_power(lcd, FB_BLANK_UNBLANK);

	return 0;
}

static int ea8061v_suspend(struct mipi_dsim_device *dsim)
{
	struct lcd_info *lcd = g_lcd;

	ea8061v_power(lcd, FB_BLANK_POWERDOWN);

	return 0;
}

static int ea8061v_resume(struct mipi_dsim_device *dsim)
{
	return 0;
}

struct mipi_dsim_lcd_driver ea8061v_mipi_lcd_driver = {
	.probe		= ea8061v_probe,
	.displayon	= ea8061v_displayon,
	.suspend	= ea8061v_suspend,
	.resume		= ea8061v_resume,
};

static int ea8061v_init(void)
{
	return 0 ;
#if 0
	s5p_mipi_dsi_register_lcd_driver(&ea8061v_mipi_lcd_driver);
	exynos_mipi_dsi_register_lcd_driver
#endif
}

static void ea8061v_exit(void)
{
	return;
}

module_init(ea8061v_init);
module_exit(ea8061v_exit);

MODULE_DESCRIPTION("MIPI-DSI ea8061v (720*1280) Panel Driver");
MODULE_LICENSE("GPL");
