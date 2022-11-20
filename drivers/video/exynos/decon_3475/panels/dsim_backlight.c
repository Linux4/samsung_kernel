/* linux/drivers/video/exynos_decon/panel/dsim_backlight.c
 *
 * Header file for Samsung MIPI-DSI Backlight driver.
 *
 * Copyright (c) 2013 Samsung Electronics
 * Minwoo Kim <minwoo7945.kim@samsung.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
*/


#include <linux/backlight.h>

#include "../dsim.h"
#include "../decon.h"
#include "dsim_backlight.h"
#include "panel_info.h"
#include <linux/gpio.h>
#include <linux/interrupt.h>

#ifdef CONFIG_PANEL_AID_DIMMING
#include "aid_dimming.h"
#endif

/*Control backlight*/
#define EW_DELAY (150)
#define EW_DETECT (270)
#define EW_WINDOW (1000 - (EW_DELAY + EW_DETECT))
#define DATA_START (5)
#define LOW_BIT_H (5)
#define LOW_BIT_L (4 * HIGH_BIT_L)
#define HIGH_BIT_L (5)
#define HIGH_BIT_H (4 * HIGH_BIT_L)
#define END_DATA_L (10)
#define END_DATA_H (350)

#ifdef CONFIG_PANEL_AID_DIMMING

static unsigned int get_actual_br_value(struct dsim_device *dsim, int index)
{
	struct panel_private *panel = &dsim->priv;
	struct SmtDimInfo *dimming_info = (struct SmtDimInfo *)panel->dim_info;

	if (dimming_info == NULL) {
		dsim_err("%s : dimming info is NULL\n", __func__);
		goto get_br_err;
	}

	if (index > MAX_BR_INFO)
		index = MAX_BR_INFO;

	return dimming_info[index].br;

get_br_err:
	return 0;
}
static unsigned char *get_gamma_from_index(struct dsim_device *dsim, int index)
{
	struct panel_private *panel = &dsim->priv;
	struct SmtDimInfo *dimming_info = (struct SmtDimInfo *)panel->dim_info;

	if (dimming_info == NULL) {
		dsim_err("%s : dimming info is NULL\n", __func__);
		goto get_gamma_err;
	}

	if (index > MAX_BR_INFO)
		index = MAX_BR_INFO;

	return (unsigned char *)dimming_info[index].gamma;

get_gamma_err:
	return NULL;
}

static unsigned char *get_aid_from_index(struct dsim_device *dsim, int index)
{
	struct panel_private *panel = &dsim->priv;
	struct SmtDimInfo *dimming_info = (struct SmtDimInfo *)panel->dim_info;

	if (dimming_info == NULL) {
		dsim_err("%s : dimming info is NULL\n", __func__);
		goto get_aid_err;
	}

	if (index > MAX_BR_INFO)
		index = MAX_BR_INFO;

	return (u8 *)dimming_info[index].aid;

get_aid_err:
	return NULL;
}

static unsigned char *get_elvss_from_index(struct dsim_device *dsim, int index, int acl)
{
	struct panel_private *panel = &dsim->priv;
	struct SmtDimInfo *dimming_info = (struct SmtDimInfo *)panel->dim_info;

	if (dimming_info == NULL) {
		dsim_err("%s : dimming info is NULL\n", __func__);
		goto get_elvess_err;
	}

	if(acl)
		return (unsigned char *)dimming_info[index].elvAcl;
	else
		return (unsigned char *)dimming_info[index].elv;

get_elvess_err:
	return NULL;
}

static void dsim_panel_gamma_ctrl(struct dsim_device *dsim)
{
	int level = LEVEL_IS_HBM(dsim->priv.auto_brightness), i = 0;
	u8 HBM_W[34] = {0xCA, };
	u8 *gamma = NULL;

	if (level) {
		memcpy(&HBM_W[1], dsim->priv.DB, 21);
		for ( i = 22 ; i <=30 ; i++ )
			HBM_W[i] = 0x80;
		HBM_W[31] = 0x00;
		HBM_W[32] = 0x00;
		HBM_W[33] = 0x00;

		if (dsim_write_hl_data(dsim, HBM_W, ARRAY_SIZE(HBM_W)) < 0)
			dsim_err("%s : failed to write gamma \n", __func__);

	} else {
		gamma = get_gamma_from_index(dsim, dsim->priv.br_index);
		if (gamma == NULL) {
			dsim_err("%s :faied to get gamma\n", __func__);
			return;
		}
		if (dsim_write_hl_data(dsim, gamma, GAMMA_CMD_CNT) < 0)
			dsim_err("%s : failed to write gamma \n", __func__);
	}

}

static void dsim_panel_aid_ctrl(struct dsim_device *dsim)
{
	u8 *aid = NULL;
	aid = get_aid_from_index(dsim, dsim->priv.br_index);
	if (aid == NULL) {
		dsim_err("%s : faield to get aid value\n", __func__);
		return;
	}
	if (dsim_write_hl_data(dsim, aid, AID_CMD_CNT) < 0)
		dsim_err("%s : failed to write gamma \n", __func__);
}

static void dsim_panel_set_elvss(struct dsim_device *dsim)
{
	u8 *elvss = NULL;
	u8 elvss_val[3] = {0, };
	u8 B6_W[18] = {0xB6,
					0x2C, 0x0B, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
					0x00, 0x00, 0x0A, 0xAA, 0xAF, 0x0F, 0x00};
	u8 B6_GW[2] = {0xB6, };
	int level = LEVEL_IS_HBM(dsim->priv.auto_brightness);
	int real_br = 0;

	real_br = get_actual_br_value(dsim, dsim->priv.br_index);
	elvss = get_elvss_from_index(dsim, dsim->priv.br_index, dsim->priv.acl_enable);

	if (elvss == NULL) {
		dsim_err("%s : failed to get elvss value\n", __func__);
		return;
	}

	if (level) {
		B6_W[17] = dsim->priv.elvss_hbm;

		if (dsim_write_hl_data(dsim, B6_W, ARRAY_SIZE(B6_W)) < 0)
			dsim_err("%s : failed to write elvss \n", __func__);

	} else {

		B6_GW[1] = dsim->priv.elvss_hbm_default;

		if (real_br <= 29 ) {
			if (dsim->priv.temperature <= -20) {
				B6_GW[1] = elvss[5];
			} else if (dsim->priv.temperature > -20 && dsim->priv.temperature <= 0) {
				B6_GW[1] = elvss[4];
			} else {
				B6_GW[1] = elvss[3];
			}
		}

		elvss_val[0] = elvss[0];
		elvss_val[1] = elvss[1];
		elvss_val[2] = elvss[2];

		if (dsim_write_hl_data(dsim, elvss_val, ELVSS_LEN) < 0)
			dsim_err("%s : failed to write elvss \n", __func__);

		if (dsim_write_hl_data(dsim, SEQ_ELVSS_GLOBAL, ARRAY_SIZE(SEQ_ELVSS_GLOBAL)) < 0)
		dsim_err("%s : failed to SEQ_ELVSS_GLOBAL \n", __func__);

		if (dsim_write_hl_data(dsim, B6_GW, 2) < 0)
		dsim_err("%s : failed to write elvss \n", __func__);

	}
}

static int dsim_panel_set_acl(struct dsim_device *dsim, int force)
{
	int ret = 0, level = ACL_STATUS_15P;
	struct panel_private *panel = &dsim->priv;

	if (panel == NULL) {
			dsim_err("%s : panel is NULL\n", __func__);
			goto exit;
	}

	if (dsim->priv.siop_enable || LEVEL_IS_HBM(dsim->priv.auto_brightness))  // auto acl or hbm is acl on
		goto acl_update;

	if (!dsim->priv.acl_enable)
		level = ACL_STATUS_0P;

acl_update:
	if(force || dsim->priv.current_acl != panel->acl_cutoff_tbl[level][1]) {
		if((ret = dsim_write_hl_data(dsim, panel->acl_cutoff_tbl[level], 2)) < 0) {
			dsim_err("fail to write acl command.\n");
			goto exit;
		}
		dsim->priv.current_acl = panel->acl_cutoff_tbl[level][1];
		dsim_info("acl: %d, auto_brightness: %d\n", dsim->priv.current_acl, dsim->priv.auto_brightness);
	}
exit:
	if (!ret)
		ret = -EPERM;
	return ret;
}

static int dsim_panel_set_tset(struct dsim_device *dsim, int force)
{
	int ret = 0;
	int tset = 0;
	unsigned char SEQ_TSET[TSET_LEN] = {TSET_REG, 0x38, 0x0B, 0x40, 0x00, 0xA8, 0x00};

	tset = ((dsim->priv.temperature >= 0) ? 0 : BIT(7)) | abs(dsim->priv.temperature);

	if(force || dsim->priv.tset[0] != tset) {
		dsim->priv.tset[0] = SEQ_TSET[6] = tset;

		if ((ret = dsim_write_hl_data(dsim, SEQ_TSET, ARRAY_SIZE(SEQ_TSET))) < 0) {
			dsim_err("fail to write tset command.\n");
			ret = -EPERM;
		}

		dsim_info("temperature: %d, tset: %d\n", dsim->priv.temperature, SEQ_TSET[6]);
	}
	return ret;
}

#ifdef CONFIG_PANEL_S6E3HA2_DYNAMIC
static int dsim_panel_set_vint(struct dsim_device *dsim, int force)
{
	int ret = 0;
	int nit = 0;
	int i, level;
	int arraySize = ARRAY_SIZE(VINT_DIM_TABLE);
	unsigned char SEQ_VINT[VINT_LEN] = {VINT_REG, 0x8B, 0x00};

	nit = dsim->priv.br_tbl[dsim->priv.bd->props.brightness];

	level = arraySize - 1;

	for (i = 0; i < arraySize; i++) {
		if (nit <= VINT_DIM_TABLE[i]) {
			level = i;
			goto set_vint;
		}
	}
set_vint:
	if(force || dsim->priv.current_vint != VINT_TABLE[level]) {
		SEQ_VINT[VINT_LEN - 1] = VINT_TABLE[level];
		if ((ret = dsim_write_hl_data(dsim, SEQ_VINT, ARRAY_SIZE(SEQ_VINT))) < 0) {
			dsim_err("fail to write vint command.\n");
			ret = -EPERM;
		}
		dsim->priv.current_vint = VINT_TABLE[level];
		dsim_info("vint: %02x\n", dsim->priv.current_vint);
	}
	return ret;
}
#endif

static int dsim_panel_set_hbm(struct dsim_device *dsim, int force)
{
	int ret = 0, level = LEVEL_IS_HBM(dsim->priv.auto_brightness);
	struct panel_private *panel = &dsim->priv;

	if (panel == NULL) {
		dsim_err("%s : panel is NULL\n", __func__);
		goto exit;
	}

	if(force || dsim->priv.current_hbm != panel->hbm_tbl[level][1]) {
		dsim->priv.current_hbm = panel->hbm_tbl[level][1];
		if((ret = dsim_write_hl_data(dsim, panel->hbm_tbl[level], ARRAY_SIZE(SEQ_HBM_OFF))) < 0) {
			dsim_err("fail to write hbm command.\n");
			ret = -EPERM;
		}
		dsim_info("hbm: %d, auto_brightness: %d\n", dsim->priv.current_hbm, dsim->priv.auto_brightness);
	}
exit:
	return ret;
}

static int low_level_set_brightness(struct dsim_device *dsim ,int force)
{
	if (dsim_write_hl_data(dsim, SEQ_TEST_KEY_ON_F0, ARRAY_SIZE(SEQ_TEST_KEY_ON_F0)) < 0)
		dsim_err("%s : fail to write F0 on command.\n", __func__);

	dsim_panel_gamma_ctrl(dsim);

	dsim_panel_aid_ctrl(dsim);

	dsim_panel_set_elvss(dsim);

	dsim_panel_set_acl(dsim, force);

	if (dsim_write_hl_data(dsim, SEQ_GAMMA_UPDATE_S6E88A0, ARRAY_SIZE(SEQ_GAMMA_UPDATE_S6E88A0)) < 0)
			dsim_err("%s : fail to write SEQ_GAMMA_UPDATE_S6E88A0 on command.\n", __func__);

	dsim_panel_set_tset(dsim, force);

	if (dsim_write_hl_data(dsim, SEQ_TEST_KEY_OFF_F0, ARRAY_SIZE(SEQ_TEST_KEY_OFF_F0)) < 0)
		dsim_err("%s : fail to write F0 on command\n", __func__);


	return 0;

	dsim_panel_set_hbm(dsim, force);

	if (dsim_write_hl_data(dsim, SEQ_TEST_KEY_OFF_FC, ARRAY_SIZE(SEQ_TEST_KEY_OFF_FC)) < 0)
			dsim_err("%s : fail to write F0 on command.\n", __func__);
	if (dsim_write_hl_data(dsim, SEQ_TEST_KEY_OFF_F0, ARRAY_SIZE(SEQ_TEST_KEY_OFF_F0)) < 0)
		dsim_err("%s : fail to write F0 on command\n", __func__);

	return 0;
}

static int get_acutal_br_index(struct dsim_device *dsim, int br)
{
	int i;
	int min;
	int gap;
	int index = 0;
	struct panel_private *panel = &dsim->priv;
	struct SmtDimInfo *dimming_info = panel->dim_info;

	if (dimming_info == NULL) {
		dsim_err("%s : dimming_info is NULL\n", __func__);
		return 0;
	}

	min = MAX_BRIGHTNESS;

	for (i = 0; i < MAX_BR_INFO; i++) {
		if (br > dimming_info[i].br)
			gap = br - dimming_info[i].br;
		else
			gap = dimming_info[i].br - br;

		if (gap == 0) {
			index = i;
			break;
		}

		if (gap < min) {
			min = gap;
			index = i;
		}
	}
	return index;
}

#endif

#ifndef CONFIG_PANEL_AID_DIMMING
#if defined(CONFIG_EXYNOS3475_DECON_LCD_SC7798D) || defined(CONFIG_EXYNOS3475_DECON_LCD_SC7798D_J2)

static int dsim_panel_set_cabc(struct dsim_device *dsim, int force) {
	int ret = 0;
	struct panel_private *panel = &dsim->priv;
	int bl = 0;

	if (panel == NULL) {
		dsim_err("%s : panel is NULL\n", __func__);
		goto exit;
	}

	bl = panel->bd->props.brightness;

	if ((panel->auto_brightness > 0 && panel->auto_brightness < 6) && (bl != 0xFF))
		panel->acl_enable = 1;
	else
		panel->acl_enable = 0;

	if (panel->acl_enable_force != ACL_TEST_DO_NOT_WORK)
		panel->acl_enable = panel->acl_enable_force - 1;

	if ((panel->acl_enable == panel->current_acl) && !force)
		return 0;

	if (panel->acl_enable) { //cabc on
		if (dsim_write_hl_data(dsim, SEQ_INIT1_1, ARRAY_SIZE(SEQ_INIT1_1)) < 0)
			dsim_err("%s : failed to write SEQ_INIT1_1 \n", __func__);
		if (dsim_write_hl_data(dsim, SEQ_CABC_ON, ARRAY_SIZE(SEQ_CABC_ON)) < 0)
			dsim_err("%s : failed to write SEQ_CD_ON \n", __func__);

	} else { //cabc off
		if (dsim_write_hl_data(dsim, SEQ_INIT1_1, ARRAY_SIZE(SEQ_INIT1_1)) < 0)
			dsim_err("%s : failed to write SEQ_INIT1_1 \n", __func__);

		if (dsim_write_hl_data(dsim, SEQ_CABC_OFF, ARRAY_SIZE(SEQ_CABC_OFF)) < 0)
			dsim_err("%s : failed to write SEQ_CD_ON \n", __func__);
	}

	panel->current_acl = panel->acl_enable;
	dsim_info("cabc: %d, auto_brightness: %d\n", dsim->priv.current_acl, dsim->priv.auto_brightness);

exit:
	if (!ret)
		ret = -EPERM;
	return ret;
}
#endif
#endif

int dsim_panel_set_brightness(struct dsim_device *dsim, int force)
{
	int ret = 0;
#ifndef CONFIG_PANEL_AID_DIMMING
#ifdef CONFIG_EXYNOS3475_DECON_LCD_SC7798D
#define BRIGHTNESS_REG 0x51
#define LEVEL_IS_HBM(level)			(level >= 6)

	struct panel_private *panel = &dsim->priv;
	unsigned char bl_reg[2];
	int bl = panel->bd->props.brightness;

	if (panel->state != PANEL_STATE_RESUMED) {
		dsim_info("%s : panel is not active state..\n", __func__);
		goto set_br_exit;
	}

	bl_reg[0] = BRIGHTNESS_REG;
	if (bl >= 8)
		bl_reg[1] = (bl - 3) * 181 / 252 + 3;
	else if ( bl > 0)
		bl_reg[1] = 0x06;
	else
		bl_reg[1] = 0x00;

	if (LEVEL_IS_HBM(panel->auto_brightness) && (panel->bd->props.brightness == 255))
		bl_reg[1] = 0xEA;

	if (panel->state != PANEL_STATE_RESUMED) {
		dsim_info("%s : panel is not active state..\n", __func__);
		return ret;
	}

	dsim_info("%s: platform BL : %d panel BL reg : %d \n", __func__, panel->bd->props.brightness, bl_reg[1]);

	if (dsim_write_hl_data(dsim, bl_reg, ARRAY_SIZE(bl_reg)) < 0)
		dsim_err("%s : fail to write brightness cmd.\n", __func__);

	dsim_panel_set_cabc(dsim, force);

set_br_exit:
	return ret;

#elif defined(CONFIG_PANEL_S6D7AA0_DYNAMIC)
#define BRIGHTNESS_REG 0x51
#define LEVEL_IS_HBM(level)			(level >= 6)

		struct panel_private *panel = &dsim->priv;
		unsigned char bl_reg[2];
		int bl = panel->bd->props.brightness;

		if (panel->state != PANEL_STATE_RESUMED) {
			dsim_info("%s : panel is not active state..\n", __func__);
			goto set_br_exit;
		}

		bl_reg[0] = BRIGHTNESS_REG;
		if (bl >= 3)
			bl_reg[1] = bl;
		else
			bl_reg[1] = 0x03;

		if (panel->state != PANEL_STATE_RESUMED) {
			dsim_info("%s : panel is not active state..\n", __func__);
			return ret;
		}

		dsim_info("%s: platform BL : %d panel BL reg : %d \n", __func__, panel->bd->props.brightness, bl_reg[1]);

		if (dsim_write_hl_data(dsim, bl_reg, ARRAY_SIZE(bl_reg)) < 0)
			dsim_err("%s : fail to write brightness cmd.\n", __func__);

	set_br_exit:
		return ret;

#elif defined(CONFIG_EXYNOS3475_DECON_LCD_SC7798D_J2)

	struct panel_private *panel = &dsim->priv;
	unsigned char brightness = panel->bd->props.brightness;
	struct dsim_resources *res = &dsim->res;
	unsigned long irqFlags;
	int i;
	u8 aor_temp[6] = {0xB2, 0x40, 0x0A, 0x17, 0x00, 0x0A};

	if (panel->state != PANEL_STATE_RESUMED) {
		dsim_info("%s : panel is not active state..\n", __func__);
		goto set_br_exit;
	}

	if ( dsim->rev >= 1) { /* OCTA temp aor*/

		aor_temp[4] =	( 1024 * ( panel->bd->props.brightness / 255 ) ) >> 2;
		aor_temp[5] =	( 1024 * ( panel->bd->props.brightness / 255 ) ) / 256;

		if (dsim_write_hl_data(dsim, SEQ_TEST_KEY_ON_F0, ARRAY_SIZE(SEQ_TEST_KEY_ON_F0)) < 0)
			dsim_err("%s : fail to write F0 on command.\n", __func__);

		if (dsim_write_hl_data(dsim, aor_temp, ARRAY_SIZE(aor_temp)) < 0)
			dsim_err("%s : failed to write gamma \n", __func__);

		if (dsim_write_hl_data(dsim, SEQ_GAMMA_UPDATE, ARRAY_SIZE(SEQ_GAMMA_UPDATE)) < 0)
			dsim_err("%s : failed to write gamma \n", __func__);

		if (dsim_write_hl_data(dsim, SEQ_TEST_KEY_OFF_F0, ARRAY_SIZE(SEQ_TEST_KEY_OFF_F0)) < 0)
		dsim_err("%s : fail to write F0 on command\n", __func__);

	} else {
		if (brightness == 0) {
			gpio_set_value(res->lcd_power[1], 0);
			usleep_range(10, 20);
			panel->bd->props.state = brightness;
			goto set_br_exit;
		}

		/*previous backlight state*/
		if (panel->bd->props.state != brightness){

			dsim_info("%s:Platform BL : %d, previous value : %d \n", __func__, brightness, panel->bd->props.state);

			local_irq_save(irqFlags);

			/* Set ExpressWire Mode */

			if (panel->bd->props.state == 0) {
				dsim_info("%s, set expresswire mode\n", __func__);
				gpio_set_value(res->lcd_power[1], 0);
				msleep(3);
				gpio_set_value(res->lcd_power[1], 1);
				udelay(EW_DELAY);
				gpio_set_value(res->lcd_power[1], 0);
				udelay(EW_DETECT);
				gpio_set_value(res->lcd_power[1], 1);
				udelay(EW_WINDOW);
			}

			gpio_set_value(res->lcd_power[1], 1);
			udelay(DATA_START);

			/* Transfer 8 bits data*/
			for (i = 0; i < 8; i++) {
				if (brightness & (0x80 >> i)) {
					gpio_set_value(res->lcd_power[1], 0);
					udelay(HIGH_BIT_L);
					gpio_set_value(res->lcd_power[1], 1);
					udelay(HIGH_BIT_H);
				} else {
					gpio_set_value(res->lcd_power[1], 0);
					udelay(LOW_BIT_L);
					gpio_set_value(res->lcd_power[1], 1);
					udelay(LOW_BIT_H);
				}
			}

			gpio_set_value(res->lcd_power[1], 0);
			udelay(END_DATA_L);
			gpio_set_value(res->lcd_power[1], 1);

			local_irq_restore(irqFlags);

			udelay(END_DATA_H);


		}

		panel->bd->props.state = brightness;

		dsim_panel_set_cabc(dsim, force);
}
set_br_exit:

	return ret;

#else
	dsim_info("%s:this panel does not support dimming \n", __func__);
#endif
#else
	struct dim_data *dimming;
	struct panel_private *panel = &dsim->priv;
	int p_br = panel->bd->props.brightness;
	int acutal_br = 0;
	int real_br = 0;
	int prev_index = panel->br_index;

	dimming = (struct dim_data *)panel->dim_data;
	if ((dimming == NULL) || (panel->br_tbl == NULL)) {
		dsim_info("%s : this panel does not support dimming\n", __func__);
		return ret;
	}

	acutal_br = panel->br_tbl[p_br];
	panel->br_index = get_acutal_br_index(dsim, acutal_br);
	real_br = get_actual_br_value(dsim, panel->br_index);
	panel->acl_enable = ACL_IS_ON(real_br);

	if (LEVEL_IS_HBM(panel->auto_brightness) && (p_br == panel->bd->props.max_brightness)) {
		panel->br_index = HBM_INDEX;
		panel->acl_enable = 1;				// hbm is acl on
	}
	if(panel->siop_enable)					// check auto acl
		panel->acl_enable = 1;

	if (panel->state != PANEL_STATE_RESUMED) {
		dsim_info("%s : panel is not active state..\n", __func__);
		goto set_br_exit;
	}

	dsim_info("%s : platform : %d, : mapping : %d, real : %d, index : %d\n",
		__func__, p_br, acutal_br, real_br, panel->br_index);

	if (!force && panel->br_index == prev_index)
		goto set_br_exit;

	if ((acutal_br == 0) || (real_br == 0))
		goto set_br_exit;

	mutex_lock(&panel->lock);

	ret = low_level_set_brightness(dsim, force);
	if (ret) {
		dsim_err("%s failed to set brightness : %d\n", __func__, acutal_br);
	}
	mutex_unlock(&panel->lock);

set_br_exit:
#endif
	return ret;
}

static int panel_get_brightness(struct backlight_device *bd)
{
	return bd->props.brightness;
}


static int panel_set_brightness(struct backlight_device *bd)
{
	int ret = 0;
	int brightness = bd->props.brightness;
	struct panel_private *priv = bl_get_data(bd);
	struct dsim_device *dsim;

	dsim = container_of(priv, struct dsim_device, priv);

	if (brightness < UI_MIN_BRIGHTNESS || brightness > UI_MAX_BRIGHTNESS) {
		printk(KERN_ALERT "Brightness should be in the range of 0 ~ 255\n");
		ret = -EINVAL;
		goto exit_set;
	}

	ret = dsim_panel_set_brightness(dsim, 0);
	if (ret) {
		dsim_err("%s : fail to set brightness\n", __func__);
		goto exit_set;
	}
exit_set:
	return ret;

}

static const struct backlight_ops panel_backlight_ops = {
	.get_brightness = panel_get_brightness,
	.update_status = panel_set_brightness,
};


int dsim_backlight_probe(struct dsim_device *dsim)
{
	int ret = 0;
	struct panel_private *panel = &dsim->priv;

	panel->bd = backlight_device_register("panel", dsim->dev, &dsim->priv,
					&panel_backlight_ops, NULL);
	if (IS_ERR(panel->bd)) {
		dsim_err("%s:failed register backlight\n", __func__);
		ret = PTR_ERR(panel->bd);
	}

	panel->bd->props.max_brightness = UI_MAX_BRIGHTNESS;
	panel->bd->props.brightness = UI_DEFAULT_BRIGHTNESS;
	panel->bd->props.state = 0;

#ifdef CONFIG_LCD_HMT
	panel->hmt_on = HMT_OFF;
	panel->hmt_brightness = DEFAULT_HMT_BRIGHTNESS;
	panel->hmt_br_index = 0;
#endif
	return ret;
}
