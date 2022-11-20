/*
 * drivers/video/decon_7580/panels/s6e3aa2_a3xe_lcd_ctrl.c
 *
 * Samsung SoC MIPI LCD CONTROL functions
 *
 * Copyright (c) 2015 Samsung Electronics
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
*/

#include <linux/lcd.h>
#include <linux/backlight.h>
#include <linux/of_device.h>
#include <video/mipi_display.h>
#include "../dsim.h"
#include "dsim_panel.h"

#if defined(CONFIG_PANEL_S6E3AA2_A3XE)
#include "s6e3aa2_a3xe_param.h"
#elif defined(CONFIG_PANEL_S6E3FA3_A5XE)
#include "s6e3fa3_a5xe_param.h"
#elif defined(CONFIG_PANEL_S6E3FA3_A7XE)
#include "s6e3fa3_a7xe_param.h"
#endif

#if defined(CONFIG_EXYNOS_DECON_MDNIE_LITE)
#include "mdnie.h"
#if defined(CONFIG_PANEL_S6E3AA2_A3XE)
#include "mdnie_lite_table_a3xe.h"
#elif defined(CONFIG_PANEL_S6E3FA3_A5XE)
#include "mdnie_lite_table_a5xe.h"
#elif defined(CONFIG_PANEL_S6E3FA3_A7XE)
#include "mdnie_lite_table_a7xe.h"
#endif
#endif

#define POWER_IS_ON(pwr)					(pwr <= FB_BLANK_NORMAL)
#define LEVEL_IS_HBM(auto_brightness, brightness)		(auto_brightness >= 12 && brightness == UI_MAX_BRIGHTNESS)
#define LEVEL_IS_HBM_INTERPOLATION(auto_brightness, brightness)	(auto_brightness >= 6 && brightness == UI_MAX_BRIGHTNESS)
#define LEVEL_IS_ACL_OFF(auto_brightness, brightness)		(auto_brightness < 6 && brightness == UI_MAX_BRIGHTNESS)

#define DSI_WRITE(cmd, size)		do {				\
	ret = dsim_write_hl_data(dsim, cmd, size);			\
	if (ret < 0) {							\
		dev_err(&lcd->ld->dev, "%s: fail to write %s\n", __func__, #cmd);	\
		ret = -EPERM;						\
		goto exit;						\
	}								\
} while (0)

#ifdef SMART_DIMMING_DEBUG
#define smtd_dbg(format, arg...)	printk(format, ##arg)
#else
#define smtd_dbg(format, arg...)
#endif

#define get_bit(value, shift, size)	((value >> shift) & (0xffffffff >> (32 - size)))

union aor_info {
	u32 value;
	struct {
		u8 aor_1;
		u8 aor_2;
		u16 reserved;
	};
};

union elvss_info {
	u32 value;
	struct {
		u8 mpscon;
		u8 offset;
		u8 offset_base;
		u8 tset;
	};
};

struct hbm_interpolation_t {
	int		*hbm;
	const int	*gamma_default;

	const int	*ibr_tbl;
	int		idx_ref;
	int		idx_hbm;
};

struct lcd_info {
	unsigned int			bl;
	unsigned int			brightness;
	unsigned int			auto_brightness;
	unsigned int			auto_brightness_level;
	unsigned int			acl_enable;
	unsigned int			siop_enable;
	unsigned int			current_acl;
	unsigned int			current_bl;
	union elvss_info		current_elvss;
	union elvss_info		current_aor;
	unsigned int			current_hbm;
	unsigned int			ldi_enable;

	struct lcd_device		*ld;
	struct backlight_device		*bd;
	struct dynamic_aid_param_t	daid;

	unsigned char			elvss_table[IBRIGHTNESS_HBM_MAX][TEMP_MAX][ELVSS_CMD_CNT];
	unsigned char			gamma_table[IBRIGHTNESS_HBM_MAX][GAMMA_CMD_CNT];

	unsigned char			(*aor_table)[AID_CMD_CNT];
	unsigned char			**acl_table;
	unsigned char			**opr_table;
	unsigned char			**hbm_table;

	int				temperature;
	unsigned int			temperature_index;

	unsigned char			code[LDI_LEN_CHIP_ID];
	unsigned char			date[LDI_LEN_DATE];
	unsigned int			coordinate[2];

	unsigned char			dump_info[3];
	unsigned int			weakness_hbm_comp;

	struct dsim_device		*dsim;

	struct pinctrl			*pins;
	struct pinctrl_state		*pins_state[2];

	struct hbm_interpolation_t	hitp;

	unsigned int			ldu_index;
	unsigned char			(*aor_ldu_table)[AID_CMD_CNT];
};

static int pinctrl_enable(struct lcd_info *lcd, int enable)
{
	struct device *dev = &lcd->ld->dev;
	int ret = 0;

	if (!IS_ERR_OR_NULL(lcd->pins_state[enable])) {
		ret = pinctrl_select_state(lcd->pins, lcd->pins_state[enable]);
		if (ret) {
			dev_err(dev, "%s: pinctrl_select_state for %s\n", __func__, enable ? "on" : "off");
			return ret;
		}
	}

	return ret;
}

#ifdef CONFIG_PANEL_AID_DIMMING
static void dsim_panel_gamma_ctrl(struct dsim_device *dsim, u8 force)
{
	u8 *gamma = NULL;
	int ret = 0;
	struct lcd_info *lcd = dsim->priv.par;

	gamma = lcd->gamma_table[lcd->bl];
	if (gamma == NULL) {
		dev_err(&lcd->ld->dev, "%s: faied to get gamma\n", __func__);
		goto exit;
	}

	if (force)
		goto gamma_update;
	else if (lcd->current_bl != lcd->bl)
		goto gamma_update;
	else
		goto exit;

gamma_update:
	DSI_WRITE(gamma, GAMMA_CMD_CNT);

exit:
	return;
}

static void dsim_panel_aid_ctrl(struct dsim_device *dsim, u8 force)
{
	u8 *aid = NULL;
	int ret = 0;
	struct lcd_info *lcd = dsim->priv.par;
	union aor_info aor_value;

	aid = lcd->ldu_index ? lcd->aor_ldu_table[lcd->bl] : lcd->aor_table[lcd->brightness];
	if (aid == NULL) {
		dev_err(&lcd->ld->dev, "%s: faield to get aid value\n", __func__);
		goto exit;
	}

	aor_value.aor_1 = aid[LDI_OFFSET_AOR_1];
	aor_value.aor_2 = aid[LDI_OFFSET_AOR_2];

	DSI_WRITE(aid, AID_CMD_CNT);
	lcd->current_aor.value = aor_value.value;
	dev_info(&lcd->ld->dev, "aor: %x\n", lcd->current_aor.value);

exit:
	return;
}

static void dsim_panel_set_elvss(struct dsim_device *dsim, u8 force)
{
	u8 *elvss = NULL;
	int ret = 0;
	struct lcd_info *lcd = dsim->priv.par;
	union elvss_info elvss_value;
	unsigned char tset;

	elvss = lcd->elvss_table[lcd->bl][lcd->temperature_index];
	if (elvss == NULL) {
		dev_err(&lcd->ld->dev, "%s: failed to get elvss value\n", __func__);
		goto exit;
	}

	tset = ((lcd->temperature < 0) ? BIT(7) : 0) | abs(lcd->temperature);
	elvss_value.mpscon = elvss[LDI_OFFSET_ELVSS_1];
	elvss_value.offset = elvss[LDI_OFFSET_ELVSS_2];
	elvss_value.offset_base = elvss[LDI_OFFSET_ELVSS_3];
	elvss_value.tset = tset;

	if (force)
		goto elvss_update;
	else if (lcd->current_elvss.value != elvss_value.value)
		goto elvss_update;
	else
		goto exit;

elvss_update:
	elvss[LDI_OFFSET_ELVSS_4] = tset;
	DSI_WRITE(elvss, ELVSS_CMD_CNT);
	lcd->current_elvss.value = elvss_value.value;
	dev_info(&lcd->ld->dev, "elvss: %x\n", lcd->current_elvss.value);

exit:
	return;
}

static int dsim_panel_set_acl(struct dsim_device *dsim, int force)
{
	int ret = 0, level = ACL_STATUS_8P;
	struct lcd_info *lcd = dsim->priv.par;

	if (lcd->siop_enable || lcd->acl_enable)
		goto acl_update;

	if (LEVEL_IS_ACL_OFF(lcd->auto_brightness, lcd->brightness) && lcd->weakness_hbm_comp == 2)
		level = ACL_STATUS_0P;

acl_update:
	if (force || lcd->current_acl != lcd->acl_table[level][LDI_OFFSET_ACL]) {
		DSI_WRITE(lcd->acl_table[level], ACL_CMD_CNT);
		DSI_WRITE(lcd->opr_table[level], OPR_CMD_CNT);
		lcd->current_acl = lcd->acl_table[level][LDI_OFFSET_ACL];
		dev_info(&lcd->ld->dev, "acl: %d, brightness: %d, auto: %d\n", lcd->current_acl, lcd->brightness, lcd->auto_brightness);
	}
exit:
	return ret;
}

static int dsim_panel_set_hbm(struct dsim_device *dsim, int force)
{
	struct lcd_info *lcd = dsim->priv.par;
	int ret = 0, level = LEVEL_IS_HBM(lcd->auto_brightness, lcd->brightness);

	if (force || lcd->current_hbm != lcd->hbm_table[level][LDI_OFFSET_HBM]) {
		DSI_WRITE(lcd->hbm_table[level], HBM_CMD_CNT);
		lcd->current_hbm = lcd->hbm_table[level][LDI_OFFSET_HBM];
		dev_info(&lcd->ld->dev, "hbm: %d, auto: %d\n", lcd->current_hbm, lcd->auto_brightness);
	}

exit:
	return ret;
}

static int low_level_set_brightness(struct dsim_device *dsim, int force)
{
	struct lcd_info *lcd = dsim->priv.par;
	int ret = 0;

	DSI_WRITE(SEQ_TEST_KEY_ON_F0, ARRAY_SIZE(SEQ_TEST_KEY_ON_F0));
	DSI_WRITE(SEQ_TEST_KEY_ON_FC, ARRAY_SIZE(SEQ_TEST_KEY_ON_FC));

	dsim_panel_gamma_ctrl(dsim, force);

	dsim_panel_aid_ctrl(dsim, force);

	dsim_panel_set_elvss(dsim, force);

	DSI_WRITE(SEQ_GAMMA_UPDATE, ARRAY_SIZE(SEQ_GAMMA_UPDATE));

	dsim_panel_set_acl(dsim, force);

	dsim_panel_set_hbm(dsim, force);

	DSI_WRITE(SEQ_TEST_KEY_OFF_F0, ARRAY_SIZE(SEQ_TEST_KEY_OFF_F0));
	DSI_WRITE(SEQ_TEST_KEY_OFF_FC, ARRAY_SIZE(SEQ_TEST_KEY_OFF_FC));

exit:
	return 0;
}

static int get_backlight_level_from_brightness(int brightness, int ldu)
{
	return brightness_table[brightness][ldu];
}
#endif

static int dsim_panel_set_brightness(struct dsim_device *dsim, int force)
{
	struct panel_private *priv = &dsim->priv;
	int ret = 0;
	struct lcd_info *lcd = dsim->priv.par;

	mutex_lock(&priv->lock);

	lcd->brightness = lcd->bd->props.brightness;

	lcd->bl = get_backlight_level_from_brightness(lcd->brightness, lcd->ldu_index);

	if (LEVEL_IS_HBM_INTERPOLATION(lcd->auto_brightness, lcd->brightness))
		lcd->bl = hbm_auto_brightness_table[lcd->auto_brightness];

	if (!force && priv->state != PANEL_STATE_RESUMED) {
		dev_info(&lcd->ld->dev, "%s: panel is not active state..\n", __func__);
		goto exit;
	}

	ret = low_level_set_brightness(dsim, force);
	if (ret)
		dev_err(&lcd->ld->dev, "%s failed to set brightness : %d\n", __func__, index_brightness_table[lcd->bl]);

	lcd->current_bl = lcd->bl;

	dev_info(&lcd->ld->dev, "brightness: %d, bl: %d, nit: %d\n", lcd->brightness, lcd->bl, index_brightness_table[lcd->bl]);
exit:
	mutex_unlock(&priv->lock);

	return ret;
}

static int panel_get_brightness(struct backlight_device *bd)
{
	struct panel_private *priv = bl_get_data(bd);
	struct lcd_info *lcd = priv->par;

	return index_brightness_table[lcd->bl];
}

static int panel_set_brightness(struct backlight_device *bd)
{
	int ret = 0;
	int brightness = bd->props.brightness;
	struct panel_private *priv = bl_get_data(bd);
	struct lcd_info *lcd = priv->par;
	struct dsim_device *dsim;

	dsim = container_of(priv, struct dsim_device, priv);

	if (brightness < UI_MIN_BRIGHTNESS || brightness > UI_MAX_BRIGHTNESS) {
		pr_alert("Brightness should be in the range of 0 ~ 255\n");
		ret = -EINVAL;
		goto exit;
	}

	if (priv->state == PANEL_STATE_RESUMED) {
		ret = dsim_panel_set_brightness(dsim, 0);
		if (ret) {
			dev_err(&lcd->ld->dev, "%s: fail to set brightness\n", __func__);
			goto exit;
		}
	}

exit:
	return ret;
}

static const struct backlight_ops panel_backlight_ops = {
	.get_brightness = panel_get_brightness,
	.update_status = panel_set_brightness,
};


#ifdef CONFIG_PANEL_AID_DIMMING
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

	lcd->daid.offset_color = (const struct rgb_t(*)[])offset_color;
	lcd->daid.iv_ref = index_voltage_reference;
	lcd->daid.m_gray = m_gray;
}

/* V255(msb is seperated) ~ VT -> VT ~ V255(msb is not seperated) and signed bit */
static void reorder_reg2mtp(u8 *reg, int *mtp)
{
	int j, c, v;

	for (c = 0, j = 0; c < CI_MAX; c++, j++) {
		if (reg[j++] & 0x01)
			mtp[(IV_MAX-1)*CI_MAX+c] = reg[j] * (-1);
		else
			mtp[(IV_MAX-1)*CI_MAX+c] = reg[j];
	}

	for (v = IV_MAX - 2; v >= 0; v--) {
		for (c = 0; c < CI_MAX; c++, j++) {
			if (reg[j] & 0x80)
				mtp[CI_MAX*v+c] = (reg[j] & 0x7F) * (-1);
			else
				mtp[CI_MAX*v+c] = reg[j];
		}
	}
}

/* V255(msb is seperated) ~ VT -> VT ~ V255(msb is not seperated) */
static void reorder_reg2gamma(u8 *reg, int *gamma)
{
	int j, c, v;

	for (c = 0, j = 0; c < CI_MAX; c++, j++) {
		if (reg[j++] & 0x01)
			gamma[(IV_MAX-1)*CI_MAX+c] = reg[j] | BIT(8);
		else
			gamma[(IV_MAX-1)*CI_MAX+c] = reg[j];
	}

	for (v = IV_MAX - 2; v >= 0; v--) {
		for (c = 0; c < CI_MAX; c++, j++) {
			if (reg[j] & 0x80)
				gamma[CI_MAX*v+c] = (reg[j] & 0x7F) | BIT(7);
			else
				gamma[CI_MAX*v+c] = reg[j];
		}
	}
}

/* VT ~ V255(msb is not seperated) -> V255(msb is seperated) ~ VT */
/* array idx 0 (zero) is reserved for gamma command address (0xCA) */
static void reorder_gamma2reg(int *gamma, u8 *reg)
{
	int j, c, v;
	int *pgamma;

	v = IV_MAX - 1;
	pgamma = &gamma[v * CI_MAX];
	for (c = 0, j = 1; c < CI_MAX; c++, pgamma++) {
		if (*pgamma & 0x100)
			reg[j++] = 1;
		else
			reg[j++] = 0;

		reg[j++] = *pgamma & 0xff;
	}

	for (v = IV_MAX - 2; v > IV_VT; v--) {
		pgamma = &gamma[v * CI_MAX];
		for (c = 0; c < CI_MAX; c++, pgamma++)
			reg[j++] = *pgamma;
	}

	v = IV_VT;
	pgamma = &gamma[v * CI_MAX];
	reg[j++] = pgamma[CI_RED] << 4 | pgamma[CI_GREEN];
	reg[j++] = pgamma[CI_BLUE];
}

static void init_mtp_data(struct lcd_info *lcd, u8 *mtp_data)
{
	int i, c;
	int *mtp = lcd->daid.mtp;
	u8 tmp[IV_MAX * CI_MAX + CI_MAX] = {0, };

	memcpy(tmp, mtp_data, LDI_LEN_MTP);

	/* C8h 34th Para: VT R / VT G */
	/* C8h 35th Para: VT B */
	tmp[33] = get_bit(mtp_data[33], 4, 4);
	tmp[34] = get_bit(mtp_data[33], 0, 4);
	tmp[35] = get_bit(mtp_data[34], 0, 4);

	reorder_reg2mtp(tmp, mtp);

	smtd_dbg("MTP_Offset_Value\n");
	for (i = 0; i < IV_MAX; i++) {
		for (c = 0; c < CI_MAX; c++)
			smtd_dbg("%4d ", mtp[i*CI_MAX+c]);
		smtd_dbg("\n");
	}
}

static int init_gamma(struct lcd_info *lcd, u8 *mtp_data)
{
	int i, j;
	int ret = 0;
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

	/* pre-allocate memory for gamma table */
	for (i = 0; i < IBRIGHTNESS_MAX; i++)
		memcpy(&lcd->gamma_table[i], SEQ_GAMMA_CONDITION_SET, GAMMA_CMD_CNT);

	/* calculate gamma table */
	init_mtp_data(lcd, mtp_data);
	dynamic_aid(lcd->daid, gamma);

	/* relocate gamma order */
	for (i = 0; i < IBRIGHTNESS_MAX; i++)
		reorder_gamma2reg(gamma[i], lcd->gamma_table[i]);

	for (i = 0; i < IBRIGHTNESS_MAX; i++) {
		smtd_dbg("Gamma [%3d] = ", lcd->daid.ibr_tbl[i]);
		for (j = 0; j < GAMMA_CMD_CNT; j++)
			smtd_dbg("%4d ", lcd->gamma_table[i][j]);
		smtd_dbg("\n");
	}

	/* free local gamma table */
	for (i = 0; i < IBRIGHTNESS_MAX; i++)
		kfree(gamma[i]);
	kfree(gamma);

	return 0;

err_alloc_gamma:
	while (i > 0) {
		kfree(gamma[i-1]);
		i--;
	}
	kfree(gamma);
err_alloc_gamma_table:
	return ret;
}
#endif

static int s6e3aa2_read_info(struct dsim_device *dsim, u8 reg, u32 len, u8 *buf)
{
	int ret = 0, i;
	struct lcd_info *lcd = dsim->priv.par;

	ret = dsim_read_hl_data(dsim, reg, len, buf);

	if (ret != len) {
		dev_err(&lcd->ld->dev, "%s: fail: %02xh\n", __func__, reg);
		ret = -EINVAL;
		goto exit;
	}

	smtd_dbg("%s: %02xh\n", __func__, reg);
	for (i = 0; i < len; i++)
		smtd_dbg("%02dth value is %02x, %3d\n", i + 1, buf[i], buf[i]);

exit:
	return ret;
}

static int s6e3aa2_read_id(struct dsim_device *dsim)
{
	int ret = 0;
	struct panel_private *priv = &dsim->priv;
	struct lcd_info *lcd = dsim->priv.par;

	ret = s6e3aa2_read_info(dsim, LDI_REG_ID, LDI_LEN_ID, priv->id);
	if (ret < 0) {
		priv->lcdConnected = PANEL_DISCONNEDTED;
		dev_err(&lcd->ld->dev, "%s: fail\n", __func__);
		goto exit;
	}

exit:
	return ret;
}

static int s6e3aa2_read_mtp(struct dsim_device *dsim, unsigned char *buf)
{
	int ret = 0;
	struct lcd_info *lcd = dsim->priv.par;

	ret = s6e3aa2_read_info(dsim, LDI_REG_MTP, LDI_LEN_MTP, buf);
	if (ret < 0) {
		dev_err(&lcd->ld->dev, "%s: fail\n", __func__);
		goto exit;
	}

exit:
	return ret;
}

static int s6e3aa2_read_coordinate(struct dsim_device *dsim)
{
	int ret = 0;
	struct lcd_info *lcd = dsim->priv.par;
	unsigned char buf[LDI_GPARA_DATE + LDI_LEN_DATE] = {0,};

	ret = s6e3aa2_read_info(dsim, LDI_REG_COORDINATE, ARRAY_SIZE(buf), buf);
	if (ret < 0) {
		dev_err(&lcd->ld->dev, "%s: fail\n", __func__);
		goto exit;
	}

	lcd->coordinate[0] = buf[0] << 8 | buf[1];	/* X */
	lcd->coordinate[1] = buf[2] << 8 | buf[3];	/* Y */

	memcpy(lcd->date, &buf[LDI_GPARA_DATE], LDI_LEN_DATE);

exit:
	return ret;
}

static int s6e3aa2_read_chip_id(struct dsim_device *dsim)
{
	int ret = 0;
	struct lcd_info *lcd = dsim->priv.par;

	ret = s6e3aa2_read_info(dsim, LDI_REG_CHIP_ID, LDI_LEN_CHIP_ID, lcd->code);
	if (ret < 0) {
		dev_err(&lcd->ld->dev, "%s: fail\n", __func__);
		goto exit;
	}

exit:
	return ret;
}

static int s6e3aa2_read_elvss(struct dsim_device *dsim, unsigned char *buf)
{
	int ret = 0;
	struct lcd_info *lcd = dsim->priv.par;

	ret = s6e3aa2_read_info(dsim, LDI_REG_ELVSS, LDI_LEN_ELVSS, buf);
	if (ret < 0) {
		dev_err(&lcd->ld->dev, "%s: fail\n", __func__);
		goto exit;
	}

exit:
	return ret;
}

static int s6e3aa2_init_elvss(struct dsim_device *dsim, u8 *elvss_data)
{
	int i, temp, ret = 0;
	struct lcd_info *lcd = dsim->priv.par;

	for (i = 0; i < IBRIGHTNESS_MAX; i++) {
		for (temp = 0; temp < TEMP_MAX; temp++) {
			/* Duplicate with reading value from DDI */
			memcpy(&lcd->elvss_table[i][temp][1], elvss_data, LDI_LEN_ELVSS);

			lcd->elvss_table[i][temp][0] = elvss_mpscon_offset_data[i][0];
			lcd->elvss_table[i][temp][1] = elvss_mpscon_offset_data[i][1];
			lcd->elvss_table[i][temp][LDI_OFFSET_ELVSS_1] = elvss_mpscon_offset_data[i][LDI_OFFSET_ELVSS_1];
			lcd->elvss_table[i][temp][LDI_OFFSET_ELVSS_2] = elvss_mpscon_offset_data[i][LDI_OFFSET_ELVSS_2];

			if (elvss_otp_data[i].nit && elvss_otp_data[i].not_otp[temp] != -1)
				lcd->elvss_table[i][temp][LDI_OFFSET_ELVSS_3] = elvss_otp_data[i].not_otp[temp];
		}
	}

	return ret;
}

static int s6e3aa2_read_hbm(struct dsim_device *dsim, unsigned char *buf)
{
	int ret = 0;
	struct lcd_info *lcd = dsim->priv.par;

	ret = s6e3aa2_read_info(dsim, LDI_REG_HBM, LDI_LEN_HBM, buf);
	if (ret < 0) {
		dev_err(&lcd->ld->dev, "%s: fail\n", __func__);
		goto exit;
	}

exit:
	return ret;
}

#if 0
static int s6e3aa2_read_status(struct dsim_device *dsim)
{
	int ret = 0;
	struct lcd_info *lcd = dsim->priv.par;
	unsigned char rddpm[LDI_LEN_RDDPM] = {0,};
	unsigned char rddsm[LDI_LEN_RDDSM] = {0,};
	unsigned char esderr[LDI_LEN_ESDERR] = {0,};

	DSI_WRITE(SEQ_TEST_KEY_ON_F0, ARRAY_SIZE(SEQ_TEST_KEY_ON_F0));

	ret = s6e3aa2_read_info(dsim, LDI_REG_RDDPM, LDI_LEN_RDDPM, rddpm);
	if (ret < 0) {
		dev_err(&lcd->ld->dev, "%s: fail\n", __func__);
		goto exit;
	}

	ret = s6e3aa2_read_info(dsim, LDI_REG_RDDSM, LDI_LEN_RDDSM, rddsm);
	if (ret < 0) {
		dev_err(&lcd->ld->dev, "%s: fail\n", __func__);
		goto exit;
	}
	ret = s6e3aa2_read_info(dsim, LDI_REG_ESDERR, LDI_LEN_ESDERR, esderr);
	if (ret < 0) {
		dev_err(&lcd->ld->dev, "%s: fail\n", __func__);
		goto exit;
	}

	dev_info(&lcd->ld->dev, "%s: dpm: %2x dsm: %2x err: %2x\n", __func__, rddpm[0], rddsm[0], esderr[0]);

	DSI_WRITE(SEQ_TEST_KEY_OFF_F0, ARRAY_SIZE(SEQ_TEST_KEY_OFF_F0));

exit:
	return ret;
}
#endif

static int s6e3aa2_init_hbm_elvss(struct dsim_device *dsim, u8 *elvss_data)
{
	int i, temp, ret = 0;
	struct lcd_info *lcd = dsim->priv.par;

	for (i = IBRIGHTNESS_MAX; i < IBRIGHTNESS_HBM_MAX; i++) {
		for (temp = 0; temp < TEMP_MAX; temp++) {
			/* Duplicate with reading value from DDI */
			memcpy(&lcd->elvss_table[i][temp][1], elvss_data, LDI_LEN_ELVSS);

			lcd->elvss_table[i][temp][0] = elvss_mpscon_offset_data[i][0];
			lcd->elvss_table[i][temp][1] = elvss_mpscon_offset_data[i][1];
			lcd->elvss_table[i][temp][LDI_OFFSET_ELVSS_1] = elvss_mpscon_offset_data[i][LDI_OFFSET_ELVSS_1];
			lcd->elvss_table[i][temp][LDI_OFFSET_ELVSS_2] = elvss_mpscon_offset_data[i][LDI_OFFSET_ELVSS_2];
			if (i != IBRIGHTNESS_600NIT)
				lcd->elvss_table[i][temp][LDI_OFFSET_ELVSS_3] = elvss_data[LDI_GPARA_HBM_ELVSS];
		}
	}

	return ret;
}

static void init_hbm_interpolation(struct lcd_info *lcd)
{
	lcd->hitp.hbm = kzalloc(IV_MAX * CI_MAX * sizeof(int), GFP_KERNEL);
	lcd->hitp.gamma_default = gamma_default;

	lcd->hitp.ibr_tbl = index_brightness_table;
	lcd->hitp.idx_ref = IBRIGHTNESS_420NIT;
	lcd->hitp.idx_hbm = IBRIGHTNESS_600NIT;
}

static void init_hbm_data(struct lcd_info *lcd, u8 *hbm_data)
{
	int i, c;
	int *hbm = lcd->hitp.hbm;
	u8 tmp[IV_MAX * CI_MAX + CI_MAX] = {0, };
	u8 v255[CI_MAX][2] = {{0,}, };

	memcpy(&tmp[2], hbm_data, LDI_LEN_HBM);

	/* V255 */
	/* 1st: 0x0 & B3h 1st para D[2] */
	/* 2nd: B3h 2nd para */
	/* 3rd: 0x0 & B3h 1st para D[1] */
	/* 4th: B3h 3rd para */
	/* 5th: 0x0 & B3h 1st para D[0] */
	/* 6th: B3h 4th para */
	v255[CI_RED][0] = get_bit(hbm_data[0], 2, 1);
	v255[CI_RED][1] = hbm_data[1];

	v255[CI_GREEN][0] = get_bit(hbm_data[0], 1, 1);
	v255[CI_GREEN][1] = hbm_data[2];

	v255[CI_BLUE][0] = get_bit(hbm_data[0], 0, 1);
	v255[CI_BLUE][1] = hbm_data[3];

	tmp[0] = v255[CI_RED][0];
	tmp[1] = v255[CI_RED][1];
	tmp[2] = v255[CI_GREEN][0];
	tmp[3] = v255[CI_GREEN][1];
	tmp[4] = v255[CI_BLUE][0];
	tmp[5] = v255[CI_BLUE][1];

	for (i = 0; i < ARRAY_SIZE(tmp); i++)
		smtd_dbg("%02dth value is %02x, %3d\n", i + 1, tmp[i], tmp[i]);

	reorder_reg2gamma(tmp, hbm);

	smtd_dbg("HBM_Gamma_Value\n");
	for (i = 0; i < IV_MAX; i++) {
		for (c = 0; c < CI_MAX; c++)
			smtd_dbg("%4d ", hbm[i*CI_MAX+c]);
		smtd_dbg("\n");
	}
}

static int init_hbm_gamma(struct lcd_info *lcd)
{
	int i, v, c, ret = 0;
	int *pgamma_def, *pgamma_hbm, *pgamma;
	s64 t1, t2, ratio;
	int gamma[IV_MAX * CI_MAX] = {0, };
	struct hbm_interpolation_t *hitp = &lcd->hitp;

	for (i = IBRIGHTNESS_MAX; i < IBRIGHTNESS_HBM_MAX; i++)
		memcpy(&lcd->gamma_table[i], SEQ_GAMMA_CONDITION_SET, GAMMA_CMD_CNT);

/*
	V255 of 443 nit
	ratio = (443-420) / (600-420) = 0.127778
	target gamma = 256 + (281 - 256) * 0.127778 = 259.1944
*/
	for (i = IBRIGHTNESS_MAX; i < IBRIGHTNESS_HBM_MAX; i++) {
		t1 = hitp->ibr_tbl[i] - hitp->ibr_tbl[hitp->idx_ref];
		t2 = hitp->ibr_tbl[hitp->idx_hbm] - hitp->ibr_tbl[hitp->idx_ref];

		ratio = (t1 << 10) / t2;

		for (v = 0; v < IV_MAX; v++) {
			pgamma_def = (int *)&hitp->gamma_default[v*CI_MAX];
			pgamma_hbm = &hitp->hbm[v*CI_MAX];
			pgamma = &gamma[v*CI_MAX];

			for (c = 0; c < CI_MAX; c++) {
				t1 = pgamma_def[c] << 10;
				t2 = pgamma_hbm[c] - pgamma_def[c];
				pgamma[c] = (t1 + (t2 * ratio)) >> 10;
			}
		}

		reorder_gamma2reg(gamma, lcd->gamma_table[i]);
	}

	for (i = IBRIGHTNESS_MAX; i < IBRIGHTNESS_HBM_MAX; i++) {
		smtd_dbg("Gamma [%3d] = ", lcd->hitp.ibr_tbl[i]);
		for (v = 0; v < GAMMA_CMD_CNT; v++)
			smtd_dbg("%4d ", lcd->gamma_table[i][v]);
		smtd_dbg("\n");
	}

	return ret;
}

static int s6e3aa2_init_hbm_interpolation(struct dsim_device *dsim)
{
	struct lcd_info *lcd = dsim->priv.par;
	unsigned char hbm_data[LDI_LEN_HBM] = {0,};
	int ret = 0;

	ret |= s6e3aa2_read_hbm(dsim, hbm_data);

	init_hbm_interpolation(lcd);
	init_hbm_data(lcd, hbm_data);
	init_hbm_gamma(lcd);

	return ret;
}

static int s6e3aa2_exit(struct dsim_device *dsim)
{
	int ret = 0;
	struct lcd_info *lcd = dsim->priv.par;

	dev_info(&lcd->ld->dev, "%s\n", __func__);

	/* 2. Display Off (28h) */
	DSI_WRITE(SEQ_DISPLAY_OFF, ARRAY_SIZE(SEQ_DISPLAY_OFF));

	/* 3. Wait 10ms */
	usleep_range(10000, 11000);

	/* 4. Sleep In (10h) */
	DSI_WRITE(SEQ_SLEEP_IN, ARRAY_SIZE(SEQ_SLEEP_IN));

	/* 5. Wait 150ms */
	msleep(150);

exit:
	return ret;
}

static int s6e3aa2_displayon(struct dsim_device *dsim)
{
	int ret = 0;
	struct lcd_info *lcd = dsim->priv.par;

	dev_info(&lcd->ld->dev, "%s\n", __func__);

	/* 14. Display On(29h) */
	DSI_WRITE(SEQ_DISPLAY_ON, ARRAY_SIZE(SEQ_DISPLAY_ON));

exit:
	return ret;
}

static int s6e3aa2_init(struct dsim_device *dsim)
{
	int ret = 0;
	struct lcd_info *lcd = dsim->priv.par;

	dev_info(&lcd->ld->dev, "%s\n", __func__);

	/* 7. Sleep Out(11h) */
	DSI_WRITE(SEQ_SLEEP_OUT, ARRAY_SIZE(SEQ_SLEEP_OUT));

	/* 8. Wait 20ms */
	msleep(20);

	/* 9. ID READ */
	s6e3aa2_read_id(dsim);

	/* Test Key Enable */
	DSI_WRITE(SEQ_TEST_KEY_ON_F0, ARRAY_SIZE(SEQ_TEST_KEY_ON_F0));
	DSI_WRITE(SEQ_TEST_KEY_ON_FC, ARRAY_SIZE(SEQ_TEST_KEY_ON_FC));

	/* 10. Common Setting */
	/* 4.1.2 PCD Setting */
	DSI_WRITE(SEQ_PCD_SET_DET_LOW, ARRAY_SIZE(SEQ_PCD_SET_DET_LOW));
	/* 4.1.3 ERR_FG Setting */
	DSI_WRITE(SEQ_ERR_FG_SETTING, ARRAY_SIZE(SEQ_ERR_FG_SETTING));

	dsim_panel_set_brightness(dsim, 1);

	/* Test Key Disable */
	DSI_WRITE(SEQ_TEST_KEY_OFF_F0, ARRAY_SIZE(SEQ_TEST_KEY_OFF_F0));
	DSI_WRITE(SEQ_TEST_KEY_OFF_FC, ARRAY_SIZE(SEQ_TEST_KEY_OFF_FC));

	/* 4.1.1 TE(Vsync) ON/OFF */
	DSI_WRITE(SEQ_TE_ON, ARRAY_SIZE(SEQ_TE_ON));

	/* 12. Wait 120ms */
	msleep(120);

exit:
	return ret;
}

static int s6e3aa2_read_init_info(struct dsim_device *dsim, unsigned char *mtp)
{
	int ret = 0;
	struct lcd_info *lcd = dsim->priv.par;
	unsigned char elvss_data[LDI_LEN_ELVSS] = {0,};

	DSI_WRITE(SEQ_TEST_KEY_ON_F0, ARRAY_SIZE(SEQ_TEST_KEY_ON_F0));

	ret |= s6e3aa2_read_id(dsim);
	ret |= s6e3aa2_read_mtp(dsim, mtp);
	ret |= s6e3aa2_read_coordinate(dsim);
	ret |= s6e3aa2_read_chip_id(dsim);
	ret |= s6e3aa2_read_elvss(dsim, elvss_data);

	ret |= s6e3aa2_init_elvss(dsim, elvss_data);
	ret |= s6e3aa2_init_hbm_elvss(dsim, elvss_data);
	ret |= s6e3aa2_init_hbm_interpolation(dsim);

	DSI_WRITE(SEQ_TEST_KEY_OFF_F0, ARRAY_SIZE(SEQ_TEST_KEY_OFF_F0));

exit:
	return ret;
}

static int s6e3aa2_probe(struct dsim_device *dsim)
{
	int ret = 0;
	struct panel_private *priv = &dsim->priv;
	unsigned char mtp[LDI_LEN_MTP] = {0, };
	struct lcd_info *lcd = dsim->priv.par;
	struct device_node *np;
	struct platform_device *pdev;

	pr_info("%s: was called\n", __func__);

	priv->lcdConnected = PANEL_CONNECTED;

	lcd->bd->props.max_brightness = UI_MAX_BRIGHTNESS;
	lcd->bd->props.brightness = UI_DEFAULT_BRIGHTNESS;

	lcd->temperature = NORMAL_TEMPERATURE;
	lcd->acl_enable = 0;
	lcd->current_acl = 0;
	lcd->auto_brightness = 0;
	lcd->siop_enable = 0;
	lcd->current_hbm = 0;
	lcd->auto_brightness_level = 12;

	lcd->acl_table = ACL_TABLE;
	lcd->opr_table = OPR_TABLE;
	lcd->hbm_table = HBM_TABLE;
	lcd->aor_table = AOR_TABLE;
	lcd->aor_ldu_table = AOR_LDU_TABLE;

	ret = s6e3aa2_read_init_info(dsim, mtp);
	if (priv->lcdConnected == PANEL_DISCONNEDTED) {
		pr_err("%s: lcd was not connected\n", __func__);
		goto exit;
	}

#ifdef CONFIG_PANEL_AID_DIMMING
	init_dynamic_aid(lcd);
	init_gamma(lcd, mtp);
#endif

	dsim_panel_set_brightness(dsim, 1);

	np = of_find_node_with_property(NULL, "lcd_info");
	np = of_parse_phandle(np, "lcd_info", 0);
	pdev = of_platform_device_create(np, NULL, dsim->dev);

	lcd->pins = devm_pinctrl_get(&pdev->dev);
	if (IS_ERR(lcd->pins)) {
		pr_err("%s: devm_pinctrl_get fail\n", __func__);
		goto exit;
	}

	lcd->pins_state[0] = pinctrl_lookup_state(lcd->pins, "off");
	lcd->pins_state[1] = pinctrl_lookup_state(lcd->pins, "on");
	if (IS_ERR_OR_NULL(lcd->pins_state[0]) || IS_ERR_OR_NULL(lcd->pins_state[1])) {
		pr_err("%s: pinctrl_lookup_state fail\n", __func__);
		goto exit;
	}

	dev_err(&lcd->ld->dev, "%s: done\n", __func__);
exit:
	return ret;
}

struct dsim_panel_ops s6e3aa2_panel_ops = {
	.probe		= s6e3aa2_probe,
	.displayon	= s6e3aa2_displayon,
	.exit		= s6e3aa2_exit,
	.init		= s6e3aa2_init,
};


static ssize_t lcd_type_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct panel_private *priv = dev_get_drvdata(dev);

	sprintf(buf, "SDC_%02X%02X%02X\n", priv->id[0], priv->id[1], priv->id[2]);

	return strlen(buf);
}

static ssize_t window_type_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct panel_private *priv = dev_get_drvdata(dev);

	sprintf(buf, "%x %x %x\n", priv->id[0], priv->id[1], priv->id[2]);

	return strlen(buf);
}

static ssize_t brightness_table_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	int i, bl;
	char *pos = buf;
	struct panel_private *priv = dev_get_drvdata(dev);
	struct lcd_info *lcd = priv->par;

	for (i = 0; i <= UI_MAX_BRIGHTNESS; i++) {
		bl = get_backlight_level_from_brightness(i, lcd->ldu_index);
		pos += sprintf(pos, "%3d %3d\n", i, index_brightness_table[bl]);
	}

	return pos - buf;
}

static ssize_t auto_brightness_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct panel_private *priv = dev_get_drvdata(dev);
	struct lcd_info *lcd = priv->par;

	sprintf(buf, "%u\n", lcd->auto_brightness);

	return strlen(buf);
}

static ssize_t auto_brightness_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t size)
{
	struct dsim_device *dsim;
	struct panel_private *priv = dev_get_drvdata(dev);
	int value;
	int rc;
	struct lcd_info *lcd = priv->par;

	dsim = container_of(priv, struct dsim_device, priv);

	rc = kstrtoul(buf, (unsigned int)0, (unsigned long *)&value);
	if (rc < 0)
		return rc;
	else {
		if (lcd->auto_brightness != value) {
			dev_info(&lcd->ld->dev, "%s: %d, %d\n", __func__, lcd->auto_brightness, value);
			mutex_lock(&priv->lock);
			lcd->auto_brightness = value;
			mutex_unlock(&priv->lock);
			if (priv->state == PANEL_STATE_RESUMED)
				dsim_panel_set_brightness(dsim, 0);
		}
	}
	return size;
}

static ssize_t siop_enable_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct panel_private *priv = dev_get_drvdata(dev);
	struct lcd_info *lcd = priv->par;

	sprintf(buf, "%u\n", lcd->siop_enable);

	return strlen(buf);
}

static ssize_t siop_enable_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t size)
{
	struct dsim_device *dsim;
	struct panel_private *priv = dev_get_drvdata(dev);
	int value;
	int rc;
	struct lcd_info *lcd = priv->par;

	dsim = container_of(priv, struct dsim_device, priv);

	rc = kstrtoul(buf, (unsigned int)0, (unsigned long *)&value);
	if (rc < 0)
		return rc;
	else {
		if (lcd->siop_enable != value) {
			dev_info(dev, "%s: %d, %d\n", __func__, lcd->siop_enable, value);
			mutex_lock(&priv->lock);
			lcd->siop_enable = value;
			mutex_unlock(&priv->lock);
			if (priv->state == PANEL_STATE_RESUMED)
				dsim_panel_set_brightness(dsim, 1);
		}
	}
	return size;
}

static ssize_t power_reduce_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct panel_private *priv = dev_get_drvdata(dev);
	struct lcd_info *lcd = priv->par;

	sprintf(buf, "%u\n", lcd->acl_enable);

	return strlen(buf);
}

static ssize_t power_reduce_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t size)
{
	struct dsim_device *dsim;
	struct panel_private *priv = dev_get_drvdata(dev);
	int value;
	int rc;
	struct lcd_info *lcd = priv->par;

	dsim = container_of(priv, struct dsim_device, priv);
	rc = kstrtoul(buf, (unsigned int)0, (unsigned long *)&value);

	if (rc < 0)
		return rc;
	else {
		if (lcd->acl_enable != value) {
			dev_info(dev, "%s: %d, %d\n", __func__, lcd->acl_enable, value);
			mutex_lock(&priv->lock);
			lcd->acl_enable = value;
			mutex_unlock(&priv->lock);
			if (priv->state == PANEL_STATE_RESUMED)
				dsim_panel_set_brightness(dsim, 1);
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
	struct dsim_device *dsim;
	struct panel_private *priv = dev_get_drvdata(dev);
	int value, rc, temperature_index = 0;
	struct lcd_info *lcd = priv->par;

	dsim = container_of(priv, struct dsim_device, priv);

	rc = kstrtoint(buf, 10, &value);

	if (rc < 0)
		return rc;
	else {
		switch (value) {
		case 1:
			temperature_index = TEMP_ABOVE_MINUS_00_DEGREE;
			break;
		case 0:
		case -19:
			temperature_index = TEMP_ABOVE_MINUS_20_DEGREE;
			break;
		case -20:
			temperature_index = TEMP_BELOW_MINUS_20_DEGREE;
			break;
		}

		mutex_lock(&priv->lock);
		lcd->temperature = value;
		lcd->temperature_index = temperature_index;
		mutex_unlock(&priv->lock);

		if (priv->state == PANEL_STATE_RESUMED)
			dsim_panel_set_brightness(dsim, 1);

		dev_info(dev, "%s: %d, %d, %d\n", __func__, value, lcd->temperature, lcd->temperature_index);
	}

	return size;
}

static ssize_t color_coordinate_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct panel_private *priv = dev_get_drvdata(dev);
	struct lcd_info *lcd = priv->par;

	sprintf(buf, "%u, %u\n", lcd->coordinate[0], lcd->coordinate[1]);

	return strlen(buf);
}

static ssize_t manufacture_date_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct panel_private *priv = dev_get_drvdata(dev);
	struct lcd_info *lcd = priv->par;
	u16 year;
	u8 month, day, hour, min, sec;
	u16 ms;

	year = ((lcd->date[0] & 0xF0) >> 4) + 2011;
	month = lcd->date[0] & 0xF;
	day = lcd->date[1] & 0x1F;
	hour = lcd->date[2] & 0x1F;
	min = lcd->date[3] & 0x3F;
	sec = lcd->date[4];
	ms = (lcd->date[5] << 8) | lcd->date[6];

	sprintf(buf, "%04d, %02d, %02d, %02d:%02d:%02d.%04d\n", year, month, day, hour, min, sec, ms);

	return strlen(buf);
}

static ssize_t manufacture_code_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct panel_private *priv = dev_get_drvdata(dev);
	struct lcd_info *lcd = priv->par;

	sprintf(buf, "%02X%02X%02X%02X%02X\n",
		lcd->code[0], lcd->code[1], lcd->code[2], lcd->code[3], lcd->code[4]);

	return strlen(buf);
}

static ssize_t cell_id_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct panel_private *priv = dev_get_drvdata(dev);
	struct lcd_info *lcd = priv->par;

	sprintf(buf, "%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X\n",
		lcd->date[0], lcd->date[1], lcd->date[2], lcd->date[3], lcd->date[4],
		lcd->date[5], lcd->date[6], (lcd->coordinate[0] & 0xFF00) >> 8, lcd->coordinate[0] & 0x00FF,
		(lcd->coordinate[1] & 0xFF00) >> 8, lcd->coordinate[1] & 0x00FF);

	return strlen(buf);
}

#ifdef CONFIG_PANEL_AID_DIMMING
static void show_aid_log(struct dsim_device *dsim)
{
	struct lcd_info *lcd = dsim->priv.par;
	u8 temp[256];
	int i, j, k;
	int *mtp;

	mtp = lcd->daid.mtp;
	for (i = 0, j = 0; i < IV_MAX; i++, j += CI_MAX) {
		if (i == 0)
			dev_info(&lcd->ld->dev, "MTP Offset VT   : %4d %4d %4d\n",
				mtp[j + CI_RED], mtp[j + CI_GREEN], mtp[j + CI_BLUE]);
		else
			dev_info(&lcd->ld->dev, "MTP Offset V%3d : %4d %4d %4d\n",
				lcd->daid.iv_tbl[i], mtp[j + CI_RED], mtp[j + CI_GREEN], mtp[j + CI_BLUE]);
	}

	for (i = 0; i < IBRIGHTNESS_MAX; i++) {
		memset(temp, 0, sizeof(temp));
		for (j = 1; j < GAMMA_CMD_CNT; j++) {
			if (j == 1 || j == 3 || j == 5)
				k = lcd->gamma_table[i][j++] * 256;
			else
				k = 0;
			snprintf(temp + strnlen(temp, 256), 256, " %3d", lcd->gamma_table[i][j] + k);
		}

		dev_info(&lcd->ld->dev, "nit : %3d  %s\n", lcd->daid.ibr_tbl[i], temp);
	}

	mtp = lcd->hitp.hbm;
	for (i = 0, j = 0; i < IV_MAX; i++, j += CI_MAX) {
		if (i == 0)
			dev_info(&lcd->ld->dev, "HBM Gamma VT   : %4d %4d %4d\n",
				mtp[j + CI_RED], mtp[j + CI_GREEN], mtp[j + CI_BLUE]);
		else
			dev_info(&lcd->ld->dev, "HBM Gamma V%3d : %4d %4d %4d\n",
				lcd->daid.iv_tbl[i], mtp[j + CI_RED], mtp[j + CI_GREEN], mtp[j + CI_BLUE]);
	}

	for (i = IBRIGHTNESS_MAX; i < IBRIGHTNESS_HBM_MAX; i++) {
		memset(temp, 0, sizeof(temp));
		for (j = 1; j < GAMMA_CMD_CNT; j++) {
			if (j == 1 || j == 3 || j == 5)
				k = lcd->gamma_table[i][j++] * 256;
			else
				k = 0;
			snprintf(temp + strnlen(temp, 256), 256, " %3d", lcd->gamma_table[i][j] + k);
		}

		dev_info(&lcd->ld->dev, "nit : %3d  %s\n", lcd->daid.ibr_tbl[i], temp);
	}

	dev_info(&lcd->ld->dev, "%s\n", __func__);
}

static ssize_t aid_log_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct dsim_device *dsim;
	struct panel_private *priv = dev_get_drvdata(dev);

	dsim = container_of(priv, struct dsim_device, priv);

	show_aid_log(dsim);

	return strlen(buf);
}
#endif

static int dsim_read_hl_data_offset(struct dsim_device *dsim, u8 addr, u32 size, u8 *buf, u32 offset)
{
	int ret;
	unsigned char wbuf[] = {0xB0, 0};
	struct lcd_info *lcd = dsim->priv.par;

	wbuf[1] = offset;

	DSI_WRITE(wbuf, ARRAY_SIZE(wbuf));

	ret = dsim_read_hl_data(dsim, addr, size, buf);

	if (ret < 1)
		dev_err(&lcd->ld->dev, "%s failed\n", __func__);

exit:
	return ret;
}

static ssize_t dump_register_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct dsim_device *dsim;
	struct panel_private *priv = dev_get_drvdata(dev);
	struct lcd_info *lcd = priv->par;
	char *pos = buf;
	u8 reg, len, offset;
	int ret, i;
	u8 *dump = NULL;

	dsim = container_of(priv, struct dsim_device, priv);

	reg = lcd->dump_info[0];
	len = lcd->dump_info[1];
	offset = lcd->dump_info[2];

	if (!reg || !len || reg > 0xff || len > 255 || offset > 255)
		goto exit;

	dump = kzalloc(len * sizeof(u8), GFP_KERNEL);

	if (priv->state == PANEL_STATE_RESUMED) {
		DSI_WRITE(SEQ_TEST_KEY_ON_F0, ARRAY_SIZE(SEQ_TEST_KEY_ON_F0));
		DSI_WRITE(SEQ_TEST_KEY_ON_FC, ARRAY_SIZE(SEQ_TEST_KEY_ON_FC));

		if (offset)
			ret = dsim_read_hl_data_offset(dsim, reg, len, dump, offset);
		else
			ret = dsim_read_hl_data(dsim, reg, len, dump);

		DSI_WRITE(SEQ_TEST_KEY_OFF_F0, ARRAY_SIZE(SEQ_TEST_KEY_OFF_F0));
		DSI_WRITE(SEQ_TEST_KEY_OFF_FC, ARRAY_SIZE(SEQ_TEST_KEY_OFF_FC));
	}

	pos += sprintf(pos, "+ [%02X]\n", reg);
	for (i = 0; i < len; i++)
		pos += sprintf(pos, "%2d: %02x\n", i + offset + 1, dump[i]);
	pos += sprintf(pos, "- [%02X]\n", reg);

	dev_info(&lcd->ld->dev, "+ [%02X]\n", reg);
	for (i = 0; i < len; i++)
		dev_info(&lcd->ld->dev, "%2d: %02x\n", i + offset + 1, dump[i]);
	dev_info(&lcd->ld->dev, "- [%02X]\n", reg);

	kfree(dump);
exit:
	return pos - buf;
}

static ssize_t dump_register_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t size)
{
	struct panel_private *priv = dev_get_drvdata(dev);
	unsigned int reg, len, offset;
	int ret;
	struct lcd_info *lcd = priv->par;

	ret = sscanf(buf, "%x %d %d", &reg, &len, &offset);

	if (ret == 2)
		offset = 0;

	dev_info(dev, "%s: %x %d %d\n", __func__, reg, len, offset);

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

static ssize_t weakness_hbm_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct panel_private *priv = dev_get_drvdata(dev);
	struct lcd_info *lcd = priv->par;

	sprintf(buf, "%d\n", lcd->weakness_hbm_comp);

	return strlen(buf);
}

static ssize_t weakness_hbm_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t size)
{
	int rc, value;
	struct dsim_device *dsim;
	struct panel_private *priv = dev_get_drvdata(dev);
	struct lcd_info *lcd = priv->par;

	dsim = container_of(priv, struct dsim_device, priv);

	rc = kstrtouint(buf, (unsigned int)0, &value);

	if (rc < 0)
		return rc;
	else {
		if (lcd->weakness_hbm_comp != value) {
			if ((lcd->weakness_hbm_comp == 1) && (value == 2)) {
				dev_info(&lcd->ld->dev, "%s: don't support hbm interpolation\n", __func__);
				return size;
			}
			if ((lcd->weakness_hbm_comp == 2) || (value == 2)) {
				lcd->weakness_hbm_comp = value;
				if (priv->state == PANEL_STATE_RESUMED)
					dsim_panel_set_brightness(dsim, 1);
				dev_info(&lcd->ld->dev, "%s: %d, %d\n", __func__, lcd->weakness_hbm_comp, value);
			}
		}
	}
	return size;
}

static ssize_t auto_brightness_level_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct panel_private *priv = dev_get_drvdata(dev);
	struct lcd_info *lcd = priv->par;

	sprintf(buf, "%u\n", lcd->auto_brightness_level);

	return strlen(buf);
}

static ssize_t ldu_correction_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct panel_private *priv = dev_get_drvdata(dev);
	struct lcd_info *lcd = priv->par;

	sprintf(buf, "%u\n", lcd->ldu_index);

	return strlen(buf);
}

static ssize_t ldu_correction_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t size)
{
	struct dsim_device *dsim;
	struct panel_private *priv = dev_get_drvdata(dev);
	int value;
	int rc;
	struct lcd_info *lcd = priv->par;

	dsim = container_of(priv, struct dsim_device, priv);
	rc = kstrtoul(buf, (unsigned int)0, (unsigned long *)&value);

	if (rc < 0)
		return rc;
	else {
		if (value > 7)
			return -EINVAL;

		if (lcd->ldu_index != value) {
			dev_info(dev, "%s: %d, %d\n", __func__, lcd->ldu_index, value);
			mutex_lock(&priv->lock);
			lcd->ldu_index = value;
			mutex_unlock(&priv->lock);
			if (priv->state == PANEL_STATE_RESUMED)
				dsim_panel_set_brightness(dsim, 1);
		}
	}
	return size;
}

static DEVICE_ATTR(lcd_type, 0444, lcd_type_show, NULL);
static DEVICE_ATTR(window_type, 0444, window_type_show, NULL);
static DEVICE_ATTR(manufacture_code, 0444, manufacture_code_show, NULL);
static DEVICE_ATTR(cell_id, 0444, cell_id_show, NULL);
static DEVICE_ATTR(brightness_table, 0444, brightness_table_show, NULL);
static DEVICE_ATTR(auto_brightness, 0644, auto_brightness_show, auto_brightness_store);
static DEVICE_ATTR(siop_enable, 0664, siop_enable_show, siop_enable_store);
static DEVICE_ATTR(power_reduce, 0664, power_reduce_show, power_reduce_store);
static DEVICE_ATTR(temperature, 0664, temperature_show, temperature_store);
static DEVICE_ATTR(color_coordinate, 0444, color_coordinate_show, NULL);
static DEVICE_ATTR(manufacture_date, 0444, manufacture_date_show, NULL);
static DEVICE_ATTR(aid_log, 0444, aid_log_show, NULL);
static DEVICE_ATTR(dump_register, 0644, dump_register_show, dump_register_store);
static DEVICE_ATTR(weakness_hbm_comp, 0664, weakness_hbm_show, weakness_hbm_store);
static DEVICE_ATTR(auto_brightness_level, 0444, auto_brightness_level_show, NULL);
static DEVICE_ATTR(ldu_correction, 0664, ldu_correction_show, ldu_correction_store);

static struct attribute *lcd_sysfs_attributes[] = {
	&dev_attr_lcd_type.attr,
	&dev_attr_window_type.attr,
	&dev_attr_manufacture_code.attr,
	&dev_attr_cell_id.attr,
	&dev_attr_siop_enable.attr,
	&dev_attr_power_reduce.attr,
	&dev_attr_temperature.attr,
	&dev_attr_color_coordinate.attr,
	&dev_attr_manufacture_date.attr,
	&dev_attr_aid_log.attr,
	&dev_attr_dump_register.attr,
	&dev_attr_ldu_correction.attr,
	NULL,
};

static struct attribute *backlight_sysfs_attributes[] = {
	&dev_attr_brightness_table.attr,
	&dev_attr_auto_brightness.attr,
	&dev_attr_weakness_hbm_comp.attr,
	&dev_attr_auto_brightness_level.attr,
	NULL,
};

static const struct attribute_group lcd_sysfs_attr_group = {
	.attrs = lcd_sysfs_attributes,
};

static const struct attribute_group backlight_sysfs_attr_group = {
	.attrs = backlight_sysfs_attributes,
};

static void lcd_init_sysfs(struct dsim_device *dsim)
{
	struct lcd_info *lcd = dsim->priv.par;
	int ret = 0;

	ret = sysfs_create_group(&lcd->ld->dev.kobj, &lcd_sysfs_attr_group);
	if (ret < 0)
		dev_err(&lcd->ld->dev, "failed to add lcd sysfs\n");

	ret = sysfs_create_group(&lcd->bd->dev.kobj, &backlight_sysfs_attr_group);
	if (ret < 0)
		dev_err(&lcd->bd->dev, "failed to add backlight sysfs\n");
}


#if defined(CONFIG_EXYNOS_DECON_MDNIE_LITE)
static int mdnie_lite_write_set(struct dsim_device *dsim, struct lcd_seq_info *seq, u32 num)
{
	int ret = 0, i;
	struct lcd_info *lcd = dsim->priv.par;

	for (i = 0; i < num; i++) {
		if (seq[i].cmd) {
			ret = dsim_write_hl_data(dsim, seq[i].cmd, seq[i].len);
			if (ret != 0) {
				dev_info(&lcd->ld->dev, "%s failed.\n", __func__);
				return ret;
			}
		}
		if (seq[i].sleep)
			usleep_range(seq[i].sleep * 1000, seq[i].sleep * 1000);
	}
	return ret;
}

int mdnie_lite_send_seq(struct dsim_device *dsim, struct lcd_seq_info *seq, u32 num)
{
	int ret = 0;
	struct panel_private *priv = &dsim->priv;
	struct lcd_info *lcd = dsim->priv.par;

	mutex_lock(&priv->lock);

	if (priv->state != PANEL_STATE_RESUMED) {
		dev_info(&lcd->ld->dev, "%s: panel is not active\n", __func__);
		ret = -EIO;
		goto exit;
	}

	ret = mdnie_lite_write_set(dsim, seq, num);

exit:
	mutex_unlock(&priv->lock);

	return ret;
}

int mdnie_lite_read(struct dsim_device *dsim, u8 addr, u8 *buf, u32 size)
{
	int ret = 0;
	struct panel_private *priv = &dsim->priv;
	struct lcd_info *lcd = dsim->priv.par;

	mutex_lock(&priv->lock);

	if (priv->state != PANEL_STATE_RESUMED) {
		dev_info(&lcd->ld->dev, "%s: panel is not active\n", __func__);
		ret = -EIO;
		goto exit;
	}

	ret = dsim_read_hl_data(dsim, addr, size, buf);

exit:
	mutex_unlock(&priv->lock);

	return ret;
}
#endif

static int dsim_panel_early_probe(struct dsim_device *dsim)
{
	int ret = 0;
	struct panel_private *priv = &dsim->priv;

	priv->ops = dsim_panel_get_priv_ops(dsim);

	if (priv->ops->early_probe)
		ret = priv->ops->early_probe(dsim);

	return ret;
}

static int dsim_panel_probe(struct dsim_device *dsim)
{
	int ret = 0;
	struct panel_private *priv = &dsim->priv;
	struct lcd_info *lcd = dsim->priv.par;
#if defined(CONFIG_EXYNOS_DECON_MDNIE_LITE)
	u16 coordinate[2] = {0, };
#endif
	priv->par = lcd = kzalloc(sizeof(struct lcd_info), GFP_KERNEL);
	if (!lcd) {
		pr_err("failed to allocate for lcd\n");
		ret = -ENOMEM;
		goto probe_err;
	}

	dsim->lcd = lcd->ld = lcd_device_register("panel", dsim->dev, &dsim->priv, NULL);
	if (IS_ERR(lcd->ld)) {
		pr_err("%s: faield to register lcd device\n", __func__);
		ret = PTR_ERR(lcd->ld);
		goto probe_err;
	}

	lcd->bd = backlight_device_register("panel", dsim->dev, &dsim->priv, &panel_backlight_ops, NULL);
	if (IS_ERR(lcd->bd)) {
		pr_err("%s: failed register backlight device\n", __func__);
		ret = PTR_ERR(lcd->bd);
		goto probe_err;
	}

	priv->lcdConnected = PANEL_CONNECTED;
	priv->state = PANEL_STATE_RESUMED;

	mutex_init(&priv->lock);

	if (priv->ops->probe) {
		ret = priv->ops->probe(dsim);
		if (ret) {
			dev_err(&lcd->ld->dev, "%s: failed to probe panel\n", __func__);
			goto probe_err;
		}
	}

#if defined(CONFIG_EXYNOS_DECON_LCD_SYSFS)
	lcd_init_sysfs(dsim);
#endif

#if defined(CONFIG_EXYNOS_DECON_MDNIE_LITE)
	coordinate[0] = (u16)lcd->coordinate[0];
	coordinate[1] = (u16)lcd->coordinate[1];
	mdnie_register(&lcd->ld->dev, dsim, (mdnie_w)mdnie_lite_send_seq, (mdnie_r)mdnie_lite_read, coordinate, &tune_info);
#endif

	dev_info(&lcd->ld->dev, "%s probe done\n", __FILE__);
probe_err:
	return ret;
}

static int dsim_panel_displayon(struct dsim_device *dsim)
{
	int ret = 0;
	struct panel_private *priv = &dsim->priv;
	struct lcd_info *lcd = dsim->priv.par;

	dev_info(&lcd->ld->dev, "+%s\n", __func__);

	if (priv->state == PANEL_STATE_SUSPENED) {
		if (priv->ops->init) {
			ret = priv->ops->init(dsim);
			if (ret) {
				dev_err(&lcd->ld->dev, "%s: failed to panel init\n", __func__);
				goto displayon_err;
			}
		}
	}

	if (priv->ops->displayon) {
		ret = priv->ops->displayon(dsim);
		if (ret) {
			dev_err(&lcd->ld->dev, "%s: failed to panel display on\n", __func__);
			goto displayon_err;
		}
	}

displayon_err:
	mutex_lock(&priv->lock);
	priv->state = PANEL_STATE_RESUMED;
	mutex_unlock(&priv->lock);

	dev_info(&lcd->ld->dev, "-%s: %d\n", __func__, priv->lcdConnected);

	pinctrl_enable(lcd, 1);

	return ret;
}

static int dsim_panel_suspend(struct dsim_device *dsim)
{
	int ret = 0;
	struct panel_private *priv = &dsim->priv;
	struct lcd_info *lcd = dsim->priv.par;

	dev_info(&lcd->ld->dev, "+%s\n", __func__);

	if (priv->state == PANEL_STATE_SUSPENED)
		goto suspend_err;

	priv->state = PANEL_STATE_SUSPENDING;

	if (priv->ops->exit) {
		ret = priv->ops->exit(dsim);
		if (ret) {
			dev_err(&lcd->ld->dev, "%s: failed to panel exit\n", __func__);
			goto suspend_err;
		}
	}

suspend_err:
	mutex_lock(&priv->lock);
	priv->state = PANEL_STATE_SUSPENED;
	mutex_unlock(&priv->lock);

	pinctrl_enable(lcd, 0);

	dev_info(&lcd->ld->dev, "-%s: %d\n", __func__, priv->lcdConnected);

	return ret;
}

struct mipi_dsim_lcd_driver s6e3aa2_mipi_lcd_driver = {
	.early_probe = dsim_panel_early_probe,
	.probe		= dsim_panel_probe,
	.displayon	= dsim_panel_displayon,
	.suspend	= dsim_panel_suspend,
};

