/* linux/drivers/video/exynos_decon/panel/lcd_sysfs.c
 *
 * Header file for Samsung MIPI-DSI Panel SYSFS driver.
 *
 * Copyright (c) 2013 Samsung Electronics
 * JiHoon Kim <jihoonn.kim@samsung.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
*/

#include <linux/lcd.h>

#include "../dsim.h"
#include "dsim_panel.h"
#include "panel_info.h"
#include "dsim_backlight.h"
#include "../decon.h"

#ifdef CONFIG_FB_DSU
#include <linux/sec_debug.h>
#endif

#if defined(CONFIG_EXYNOS_DECON_MDNIE_LITE)
#include "mdnie.h"
#endif

enum weakness_hbm_state {
	W_HBM_GALLERY = 0,
	W_HBM_OFF = 1
};
#define W_HBM_STEP	(5)

#if defined(CONFIG_SEC_FACTORY) && defined(CONFIG_EXYNOS_DECON_LCD_MCD)
#ifdef CONFIG_PANEL_S6E3HF3_DYNAMIC			// only edge panel

void mcd_mode_set(struct dsim_device *dsim)
{
	struct panel_private *panel = &dsim->priv;
	dsim_write_hl_data(dsim, SEQ_TEST_KEY_ON_F0, ARRAY_SIZE(SEQ_TEST_KEY_ON_F0));
	if(panel->mcd_on == 0)		// mcd_off
	{
		pr_info("%s MCD off : %d\n", __func__, panel->mcd_on);
		dsim_write_hl_data(dsim, SEQ_MCD_OFF_SET1, ARRAY_SIZE(SEQ_MCD_OFF_SET1));
		dsim_write_hl_data(dsim, SEQ_MCD_OFF_SET2, ARRAY_SIZE(SEQ_MCD_OFF_SET2));
		dsim_write_hl_data(dsim, SEQ_MCD_OFF_SET3, ARRAY_SIZE(SEQ_MCD_OFF_SET3));
		dsim_write_hl_data(dsim, SEQ_GAMMA_UPDATE, ARRAY_SIZE(SEQ_GAMMA_UPDATE));
		dsim_write_hl_data(dsim, SEQ_MCD_OFF_SET4, ARRAY_SIZE(SEQ_MCD_OFF_SET4));
	} else {					// mcd on
		pr_info("%s MCD on : %d\n", __func__, panel->mcd_on);
		dsim_write_hl_data(dsim, SEQ_MCD_ON_SET1, ARRAY_SIZE(SEQ_MCD_ON_SET1));
		dsim_write_hl_data(dsim, SEQ_MCD_ON_SET2, ARRAY_SIZE(SEQ_MCD_ON_SET2));
		dsim_write_hl_data(dsim, SEQ_MCD_ON_SET3, ARRAY_SIZE(SEQ_MCD_ON_SET3));
		dsim_write_hl_data(dsim, SEQ_GAMMA_UPDATE, ARRAY_SIZE(SEQ_GAMMA_UPDATE));
		dsim_write_hl_data(dsim, SEQ_MCD_ON_SET4, ARRAY_SIZE(SEQ_MCD_ON_SET4));
	}
	dsim_write_hl_data(dsim, SEQ_TEST_KEY_OFF_F0, ARRAY_SIZE(SEQ_TEST_KEY_OFF_F0));
}

#else
void mcd_mode_set(struct dsim_device *dsim)
{
	int i = 0;
	struct panel_private *panel = &dsim->priv;
	dsim_write_hl_data(dsim, SEQ_TEST_KEY_ON_F0, ARRAY_SIZE(SEQ_TEST_KEY_ON_F0));
	dsim_write_hl_data(dsim, SEQ_TEST_KEY_ON_F1, ARRAY_SIZE(SEQ_TEST_KEY_ON_F1));
	dsim_write_hl_data(dsim, SEQ_TEST_KEY_ON_FC, ARRAY_SIZE(SEQ_TEST_KEY_ON_FC));
	if(panel->mcd_on == 0)		// mcd_off
	{
		pr_info("%s MCD off : %d\n", __func__, panel->mcd_on);
		for(i = 0; i < (sizeof(SEQ_MCD_OFF_SET1) / sizeof(SEQ_MCD_OFF_SET1[0])); i++)
			dsim_write_hl_data(dsim, SEQ_MCD_OFF_SET1[i], ARRAY_SIZE(SEQ_MCD_OFF_SET1[i]));
		dsim_write_hl_data(dsim, SEQ_GAMMA_UPDATE, ARRAY_SIZE(SEQ_GAMMA_UPDATE));
		msleep(100);
		for(i = 0; i < (sizeof(SEQ_MCD_OFF_SET2) / sizeof(SEQ_MCD_OFF_SET2[0])); i++)
			dsim_write_hl_data(dsim, SEQ_MCD_OFF_SET2[i], ARRAY_SIZE(SEQ_MCD_OFF_SET2[i]));
	} else {					// mcd on
		pr_info("%s MCD on : %d\n", __func__, panel->mcd_on);
		for(i = 0; i < (sizeof(SEQ_MCD_ON_SET1) / sizeof(SEQ_MCD_ON_SET1[0])); i++)
			dsim_write_hl_data(dsim, SEQ_MCD_ON_SET1[i], ARRAY_SIZE(SEQ_MCD_ON_SET1[i]));
		dsim_write_hl_data(dsim, SEQ_GAMMA_UPDATE, ARRAY_SIZE(SEQ_GAMMA_UPDATE));
		msleep(100);
		for(i = 0; i < (sizeof(SEQ_MCD_ON_SET2) / sizeof(SEQ_MCD_ON_SET2[0])); i++)
			dsim_write_hl_data(dsim, SEQ_MCD_ON_SET2[i], ARRAY_SIZE(SEQ_MCD_ON_SET2[i]));
	}
	dsim_write_hl_data(dsim, SEQ_TEST_KEY_OFF_FC, ARRAY_SIZE(SEQ_TEST_KEY_OFF_FC));
	dsim_write_hl_data(dsim, SEQ_TEST_KEY_OFF_F1, ARRAY_SIZE(SEQ_TEST_KEY_OFF_F1));
	dsim_write_hl_data(dsim, SEQ_TEST_KEY_OFF_F0, ARRAY_SIZE(SEQ_TEST_KEY_OFF_F0));
}
#endif
static ssize_t mcd_mode_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct panel_private *priv = dev_get_drvdata(dev);

	sprintf(buf, "%u\n", priv->mcd_on);

	return strlen(buf);
}

static ssize_t mcd_mode_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t size)
{
	struct dsim_device *dsim;
	struct panel_private *priv = dev_get_drvdata(dev);
	int value;
	int rc;

	dsim = container_of(priv, struct dsim_device, priv);

	rc = kstrtouint(buf, (unsigned int)0, &value);
	if (rc < 0)
		return rc;

	if ((priv->state == PANEL_STATE_RESUMED) && (priv->mcd_on != value)) {
		priv->mcd_on = value;
		mcd_mode_set(dsim);
	}

	dev_info(dev, "%s: %d\n", __func__, priv->mcd_on);

	return size;
}

static DEVICE_ATTR(mcd_mode, 0664, mcd_mode_show, mcd_mode_store);

#endif

#ifdef CONFIG_LCD_HMT
#if defined(CONFIG_PANEL_S6E3HF2_DYNAMIC)
static struct lcd_seq_info SEQ_HMT_AID_FORWARD_SET[] = {
	{(u8 *)SEQ_HMT_AID_FORWARD1, ARRAY_SIZE(SEQ_HMT_AID_FORWARD1), 0},
	{(u8 *)SEQ_HMT_AID_FORWARD2, ARRAY_SIZE(SEQ_HMT_AID_FORWARD2), 0},
};

static struct lcd_seq_info SEQ_HMT_REVERSE_SET[] = {
	{(u8 *)SEQ_HMT_AID_REVERSE1, ARRAY_SIZE(SEQ_HMT_AID_REVERSE1), 0},
	{(u8 *)SEQ_HMT_AID_REVERSE2, ARRAY_SIZE(SEQ_HMT_AID_REVERSE2), 0},
};

static struct lcd_seq_info SEQ_HMT_ON_SET[] = {
	{(u8 *)SEQ_HMT_ON1, ARRAY_SIZE(SEQ_HMT_ON1), 0},
	{(u8 *)SEQ_HMT_ON2, ARRAY_SIZE(SEQ_HMT_ON2), 0},
	{(u8 *)SEQ_HMT_ON3, ARRAY_SIZE(SEQ_HMT_ON3), 0},
};

static struct lcd_seq_info SEQ_HMT_OFF_SET[] = {
	{(u8 *)SEQ_HMT_OFF1, ARRAY_SIZE(SEQ_HMT_OFF1), 0},
};
#elif defined(CONFIG_PANEL_S6E3HA2_DYNAMIC)
static struct lcd_seq_info SEQ_HMT_AID_FORWARD_SET[] = {
	{(u8 *)SEQ_HMT_AID_FORWARD1, ARRAY_SIZE(SEQ_HMT_AID_FORWARD1), 0},
	{(u8 *)SEQ_HMT_AID_FORWARD2, ARRAY_SIZE(SEQ_HMT_AID_FORWARD2), 0},
};

static struct lcd_seq_info SEQ_HMT_REVERSE_SET[] = {
	{(u8 *)SEQ_HMT_AID_REVERSE1, ARRAY_SIZE(SEQ_HMT_AID_REVERSE1), 0},
	{(u8 *)SEQ_HMT_AID_REVERSE2, ARRAY_SIZE(SEQ_HMT_AID_REVERSE2), 0},
};

static struct lcd_seq_info SEQ_HMT_ON_SET[] = {
	{(u8 *)SEQ_HMT_ON1, ARRAY_SIZE(SEQ_HMT_ON1), 0},
	{(u8 *)SEQ_HMT_ON2, ARRAY_SIZE(SEQ_HMT_ON2), 0},
	{(u8 *)SEQ_HMT_ON3, ARRAY_SIZE(SEQ_HMT_ON3), 0},
};

static struct lcd_seq_info SEQ_HMT_OFF_SET[] = {
	{(u8 *)SEQ_HMT_OFF1, ARRAY_SIZE(SEQ_HMT_OFF1), 0},
	{(u8 *)SEQ_HMT_OFF2, ARRAY_SIZE(SEQ_HMT_OFF2), 0},
};
#elif defined(CONFIG_PANEL_S6E3HF3_DYNAMIC) || defined(CONFIG_PANEL_S6E3HA3_DYNAMIC)
static struct lcd_seq_info SEQ_HMT_AID_FORWARD_SET[] = {
	{(u8 *)SEQ_HMT_AID_FORWARD1, ARRAY_SIZE(SEQ_HMT_AID_FORWARD1), 0},
};

static struct lcd_seq_info SEQ_HMT_REVERSE_SET[] = {
	{(u8 *)SEQ_HMT_AID_REVERSE1, ARRAY_SIZE(SEQ_HMT_AID_REVERSE1), 0},
};

static struct lcd_seq_info SEQ_HMT_ON_SET[] = {
	{(u8 *)SEQ_HMT_ON1, ARRAY_SIZE(SEQ_HMT_ON1), 0},
};

static struct lcd_seq_info SEQ_HMT_OFF_SET[] = {
	{(u8 *)SEQ_HMT_OFF1, ARRAY_SIZE(SEQ_HMT_OFF1), 0},
};

#endif

static ssize_t hmt_brightness_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct panel_private *priv = dev_get_drvdata(dev);

	sprintf(buf, "index : %d, brightenss : %d\n", priv->hmt_br_index, priv->hmt_brightness);

	return strlen(buf);
}

static ssize_t hmt_brightness_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t size)
{
	struct dsim_device *dsim;
	struct panel_private *priv = dev_get_drvdata(dev);
	int value;
	int rc;

	dsim = container_of(priv, struct dsim_device, priv);

	rc = kstrtouint(buf, (unsigned int)0, &value);
	if (rc < 0)
		return rc;
	if (priv->state != PANEL_STATE_RESUMED) {
		dev_info(dev, "%s: panel is off\n", __func__);
		return -EINVAL;
	}

	if (priv->hmt_on == HMT_OFF) {
		dev_info(dev, "%s: hmt is not on\n", __func__);
		return -EINVAL;
	}

	if (priv->hmt_brightness != value) {
		mutex_lock(&priv->lock);
		priv->hmt_brightness = value;
		mutex_unlock(&priv->lock);
		dsim_panel_set_brightness_for_hmt(dsim, 0);
	}

	dev_info(dev, "%s: %d\n", __func__, value);
	return size;
}


static int hmt_write_set(struct dsim_device *dsim, struct lcd_seq_info *seq, u32 num)
{
	int ret = 0, i;

	for (i = 0; i < num; i++) {
		if (seq[i].cmd) {
			ret = dsim_write_hl_data(dsim, seq[i].cmd, seq[i].len);
			if (ret != 0) {
				dsim_err("%s failed.\n", __func__);
				return ret;
			}
		}
		if (seq[i].sleep)
			usleep_range(seq[i].sleep * 1000 , seq[i].sleep * 1000);
	}
	return ret;
}

int hmt_set_mode(struct dsim_device *dsim, bool wakeup)
{
	struct panel_private *priv = &(dsim->priv);

	mutex_lock(&priv->lock);
	dsim_write_hl_data(dsim, SEQ_TEST_KEY_ON_F0, ARRAY_SIZE(SEQ_TEST_KEY_ON_F0));
	dsim_write_hl_data(dsim, SEQ_TEST_KEY_ON_FC, ARRAY_SIZE(SEQ_TEST_KEY_ON_FC));

	if(priv->hmt_on == HMT_ON) {
		// on set
		hmt_write_set(dsim, SEQ_HMT_ON_SET, ARRAY_SIZE(SEQ_HMT_ON_SET));
		hmt_write_set(dsim, SEQ_HMT_REVERSE_SET, ARRAY_SIZE(SEQ_HMT_REVERSE_SET));
		if(wakeup) {
			dsim_write_hl_data(dsim, SEQ_GAMMA_UPDATE, ARRAY_SIZE(SEQ_GAMMA_UPDATE));
			dsim_write_hl_data(dsim, SEQ_GAMMA_UPDATE_L, ARRAY_SIZE(SEQ_GAMMA_UPDATE_L));
			msleep(120);
		}
	} else if(priv->hmt_on == HMT_OFF) {
		// off set
		hmt_write_set(dsim, SEQ_HMT_OFF_SET, ARRAY_SIZE(SEQ_HMT_OFF_SET));
		hmt_write_set(dsim, SEQ_HMT_AID_FORWARD_SET, ARRAY_SIZE(SEQ_HMT_AID_FORWARD_SET));

	} else {
		pr_info("hmt state is invalid %d !\n", priv->hmt_on);
	}

	dsim_write_hl_data(dsim, SEQ_TEST_KEY_OFF_F0, ARRAY_SIZE(SEQ_TEST_KEY_OFF_F0));
	dsim_write_hl_data(dsim, SEQ_TEST_KEY_OFF_FC, ARRAY_SIZE(SEQ_TEST_KEY_OFF_FC));
	mutex_unlock(&priv->lock);

	if(priv->hmt_on == HMT_ON)
		dsim_panel_set_brightness_for_hmt(dsim, 1);
	else if (priv->hmt_on == HMT_OFF)
		dsim_panel_set_brightness(dsim, 1);
	else ;

	return 0;
}

static ssize_t hmt_on_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct panel_private *priv = dev_get_drvdata(dev);

	sprintf(buf, "%u\n", priv->hmt_on);

	return strlen(buf);
}

static ssize_t hmt_on_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t size)
{
	struct dsim_device *dsim;
	struct panel_private *priv = dev_get_drvdata(dev);
	int value;
	int rc;
	dsim = container_of(priv, struct dsim_device, priv);

	rc = kstrtoint(buf, 0, &value);
	if (rc < 0)
		return rc;

	if (priv->state != PANEL_STATE_RESUMED) {
		dev_info(dev, "%s: panel is off\n", __func__);
		return -EINVAL;
	}

	if (priv->hmt_on != value) {
		mutex_lock(&priv->lock);
		priv->hmt_on = value;
		mutex_unlock(&priv->lock);
		dev_info(dev, "++%s: %d\n", __func__, priv->hmt_on);
		hmt_set_mode(dsim, false);
		dev_info(dev, "--%s: %d\n", __func__, priv->hmt_on);
	} else
		dev_info(dev, "%s: hmt already %s\n", __func__, value ? "on" : "off");

	return size;
}

static DEVICE_ATTR(hmt_bright, 0664, hmt_brightness_show, hmt_brightness_store);
static DEVICE_ATTR(hmt_on, 0664, hmt_on_show, hmt_on_store);

#endif

#ifdef CONFIG_LCD_ALPM
#if defined(CONFIG_PANEL_S6E3HF3_DYNAMIC)
int alpm_set_mode(struct dsim_device *dsim, int enable)
{
	struct panel_private *priv = &(dsim->priv);
	if(priv->alpm_support != 1) {
		pr_info("%s this panel do not support alpm %d!\n", __func__, priv->alpm_support);
		return 0;
	}
	if((enable != ALPM_ON) && (enable != ALPM_OFF)) {
		pr_info("alpm state is invalid %d !\n", priv->alpm);
		return 0;
	}
	dsim_write_hl_data(dsim, SEQ_DISPLAY_OFF, ARRAY_SIZE(SEQ_DISPLAY_OFF));
	usleep_range(17000, 17000);
	dsim_write_hl_data(dsim, SEQ_TEST_KEY_ON_F0, ARRAY_SIZE(SEQ_TEST_KEY_ON_F0));
	if(enable == ALPM_ON) {
		dsim_write_hl_data(dsim, SEQ_ALPM2NIT_MODE_ON, ARRAY_SIZE(SEQ_ALPM2NIT_MODE_ON));
	} else if(enable == ALPM_OFF) {
		dsim_write_hl_data(dsim, SEQ_NORMAL_MODE_ON, ARRAY_SIZE(SEQ_NORMAL_MODE_ON));
	}
	dsim_write_hl_data(dsim, SEQ_GAMMA_UPDATE, ARRAY_SIZE(SEQ_GAMMA_UPDATE));
	dsim_write_hl_data(dsim, SEQ_GAMMA_UPDATE_L, ARRAY_SIZE(SEQ_GAMMA_UPDATE_L));
	dsim_write_hl_data(dsim, SEQ_TEST_KEY_OFF_F0, ARRAY_SIZE(SEQ_TEST_KEY_OFF_F0));
	dsim_write_hl_data(dsim, SEQ_DISPLAY_ON, ARRAY_SIZE(SEQ_DISPLAY_ON)); /* (workaround) DDI 0x0A register : DISP_ON bit not upset */
	usleep_range(17000, 17000);
	dsim_write_hl_data(dsim, SEQ_DISPLAY_ON, ARRAY_SIZE(SEQ_DISPLAY_ON));

	priv->current_alpm = dsim->alpm = enable;

	return 0;
}
#else
int alpm_set_mode(struct dsim_device *dsim, int enable)
{
	struct panel_private *priv = &(dsim->priv);
	if((enable != ALPM_ON) && (enable != ALPM_OFF)) {
		pr_info("alpm state is invalid %d !\n", priv->alpm);
		return 0;
	}
	dsim_write_hl_data(dsim, SEQ_DISPLAY_OFF, ARRAY_SIZE(SEQ_DISPLAY_OFF));
	usleep_range(17000, 17000);
	dsim_write_hl_data(dsim, SEQ_TEST_KEY_ON_F0, ARRAY_SIZE(SEQ_TEST_KEY_ON_F0));
	if(enable == ALPM_ON) {
		priv->mtpForALPM[34] = priv->mtpForALPM[35]= 0;
		dsim_write_hl_data(dsim, priv->mtpForALPM, ARRAY_SIZE(priv->mtpForALPM));
		dsim_write_hl_data(dsim, SEQ_ALPM2NIT_MODE_ON, ARRAY_SIZE(SEQ_ALPM2NIT_MODE_ON));
	} else if(enable == ALPM_OFF) {
		priv->mtpForALPM[34] = priv->prev_VT[0];
		priv->mtpForALPM[35] = priv->prev_VT[1];
		dsim_write_hl_data(dsim, priv->mtpForALPM, ARRAY_SIZE(priv->mtpForALPM));
		dsim_write_hl_data(dsim, SEQ_NORMAL_MODE_ON, ARRAY_SIZE(SEQ_NORMAL_MODE_ON));
	}
	dsim_write_hl_data(dsim, SEQ_GAMMA_UPDATE, ARRAY_SIZE(SEQ_GAMMA_UPDATE));
	dsim_write_hl_data(dsim, SEQ_GAMMA_UPDATE_L, ARRAY_SIZE(SEQ_GAMMA_UPDATE_L));
	dsim_write_hl_data(dsim, SEQ_TEST_KEY_OFF_F0, ARRAY_SIZE(SEQ_TEST_KEY_OFF_F0));
	usleep_range(17000, 17000);
	dsim_write_hl_data(dsim, SEQ_DISPLAY_ON, ARRAY_SIZE(SEQ_DISPLAY_ON));

	priv->current_alpm = dsim->alpm = enable;

	return 0;
}
#endif
static ssize_t alpm_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct panel_private *priv = dev_get_drvdata(dev);

	sprintf(buf, "%d\n", priv->alpm);

	dev_info(dev, "%s: %d\n", __func__, priv->alpm);

	return strlen(buf);
}

#if defined (CONFIG_SEC_FACTORY)
static int prev_brightness = 0;
#endif

static ssize_t alpm_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t size)
{
	struct dsim_device *dsim;
	struct panel_private *priv = dev_get_drvdata(dev);
	int value;

	dsim = container_of(priv, struct dsim_device, priv);

	sscanf(buf, "%9d", &value);
	dev_info(dev, "%s: %d \n", __func__, value);

	mutex_lock(&priv->alpm_lock);
#if defined (CONFIG_SEC_FACTORY)
	if (value) {
		if ((priv->state == PANEL_STATE_RESUMED) && !priv->current_alpm) {
			prev_brightness = priv->bd->props.brightness;
			priv->bd->props.brightness = UI_MIN_BRIGHTNESS;
			dsim_panel_set_brightness(dsim, 1);
			alpm_set_mode(dsim, ALPM_ON);
		}
	} else {
		if ((priv->state == PANEL_STATE_RESUMED) && priv->current_alpm) {
			priv->bd->props.brightness = prev_brightness;
			alpm_set_mode(dsim, ALPM_OFF);
			dsim_panel_set_brightness(dsim, 1);
		}
	}
#else
	if (value) {
		if ((priv->state == PANEL_STATE_RESUMED) && !priv->current_alpm)
			alpm_set_mode(dsim, ALPM_ON);
	} else {
		if ((priv->state == PANEL_STATE_RESUMED)&& priv->current_alpm)
			alpm_set_mode(dsim, ALPM_OFF);
#if defined(CONFIG_PANEL_S6E3HF3_DYNAMIC)
		usleep_range(17000, 17000);
		dsim_panel_set_brightness(dsim, 1);
#endif
	}
#endif
	priv->alpm = value;

	mutex_unlock(&priv->alpm_lock);

	dev_info(dev, "%s: %d %d\n", __func__, priv->alpm, dsim->alpm);

	return size;
}

static DEVICE_ATTR(alpm, 0664, alpm_show, alpm_store);
#endif

#ifdef CONFIG_LCD_DOZE_MODE

#if defined (CONFIG_SEC_FACTORY)
static int prev_brightness = 0;
static int current_alpm_mode = 0;

#endif

static ssize_t alpm_doze_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t size)
{
	int value;
	struct dsim_device *dsim;
	struct panel_private *priv = dev_get_drvdata(dev);
	struct decon_device *decon = NULL;
	struct mutex *output_lock = NULL;

	dsim = container_of(priv, struct dsim_device, priv);

	sscanf(buf, "%9d", &value);

	dsim_err("%s: sysfs alpm value: %d", __func__, value);

	decon = (struct decon_device *)dsim->decon;
	if (decon != NULL)
		output_lock = &decon->output_lock;
#ifdef CONFIG_SEC_FACTORY
	dsim_info("%s: %d\n", __func__, value);
	current_alpm_mode = priv->alpm_mode;
	switch (value) {
		case ALPM_OFF:
			priv->alpm_mode = value;
			if ((dsim->dsim_doze == DSIM_DOZE_STATE_DOZE) ||
				(dsim->dsim_doze == DSIM_DOZE_STATE_NORMAL)) {

				call_panel_ops(dsim, exitalpm, dsim);
				usleep_range(17000, 17000);
				if (prev_brightness) {
					priv->bd->props.brightness = prev_brightness - 1;
					dsim_panel_set_brightness(dsim, 1);
					prev_brightness = 0;
				}
				call_panel_ops(dsim, displayon, dsim);
			}
			break;
		case ALPM_ON_LOW:
		case HLPM_ON_LOW:
		case ALPM_ON_HIGH:
		case HLPM_ON_HIGH:
			priv->alpm_mode = value;
			if ((dsim->dsim_doze == DSIM_DOZE_STATE_DOZE) ||
				(dsim->dsim_doze == DSIM_DOZE_STATE_NORMAL)) {
				if(current_alpm_mode == ALPM_OFF) {
					prev_brightness = priv->bd->props.brightness + 1;
					priv->bd->props.brightness = UI_MIN_BRIGHTNESS;
					dsim_panel_set_brightness(dsim, 1);
				}
				call_panel_ops(dsim, enteralpm, dsim);
				usleep_range(17000, 17000);
				dsim_write_hl_data(dsim, SEQ_DISPLAY_ON, ARRAY_SIZE(SEQ_DISPLAY_ON));
			}
			break;
		default:
			dsim_err("ERR:%s:undefined alpm mode : %d\n", __func__, value);
			break;
	}
#else
	switch (value) {
		case ALPM_ON_LOW:
		case HLPM_ON_LOW:
		case ALPM_ON_HIGH:
		case HLPM_ON_HIGH:
			if (output_lock != NULL)
				mutex_lock(output_lock);

			dsim_info("%s: %d\n", __func__, priv->alpm_mode);
			if (value != priv->alpm_mode) {
				priv->alpm_mode = value;
				if (dsim->dsim_doze == DSIM_DOZE_STATE_DOZE) {
					call_panel_ops(dsim, enteralpm, dsim);
				}
			}
			if (output_lock != NULL)
				mutex_unlock(output_lock);

			break;
		default:
			dsim_err("ERR:%s:undefined alpm mode : %d\n", __func__, value);
			break;
	}
#endif
	return size;
}

static ssize_t alpm_doze_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct panel_private *priv = dev_get_drvdata(dev);

	dsim_info("%s: %d\n", __func__, priv->alpm_mode);
	sprintf(buf, "%d\n", priv->alpm_mode);
	return strlen(buf);
}

static DEVICE_ATTR(alpm, 0664, alpm_doze_show, alpm_doze_store);
#endif
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

	sprintf(buf, "%02x %02x %02x\n", priv->id[0], priv->id[1], priv->id[2]);

	return strlen(buf);
}

static ssize_t brightness_table_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct panel_private *panel = dev_get_drvdata(dev);
	char *pos = buf;
	int nit, i, br_index;
	struct dsim_device *dsim;

	dsim = container_of(panel, struct dsim_device, priv);

	for (i = 0; i <= EXTEND_BRIGHTNESS; i++) {
		nit = (panel->gallery_br_tbl && !panel->adaptive_control) ? panel->gallery_br_tbl[i] : panel->br_tbl[i];
		br_index = get_acutal_br_index(dsim, nit);
		nit = get_actual_br_value(dsim, br_index);
		pos += sprintf(pos, "%3d %3d\n", i, nit);
	}
	return pos - buf;
}

static ssize_t siop_enable_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct panel_private *priv = dev_get_drvdata(dev);

	sprintf(buf, "%u\n", priv->siop_enable);

	return strlen(buf);
}

static ssize_t siop_enable_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t size)
{
	struct dsim_device *dsim;
	struct panel_private *priv = dev_get_drvdata(dev);
	int value;
	int rc;

	dsim = container_of(priv, struct dsim_device, priv);

	rc = kstrtouint(buf, (unsigned int)0, &value);
	if (rc < 0)
		return rc;
	else {
		if (priv->siop_enable != value) {
			dev_info(dev, "%s: %d, %d\n", __func__, priv->siop_enable, value);
			mutex_lock(&priv->lock);
			priv->siop_enable = value;
			mutex_unlock(&priv->lock);
#ifdef CONFIG_LCD_HMT
			if(priv->hmt_on == HMT_ON)
				dsim_panel_set_brightness_for_hmt(dsim, 1);
			else
#endif
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
	int value;
	int rc;

	dsim = container_of(priv, struct dsim_device, priv);

	rc = kstrtoint(buf, 10, &value);

	if (rc < 0)
		return rc;
	else {
		mutex_lock(&priv->lock);
		priv->temperature = value;
		mutex_unlock(&priv->lock);
#ifdef CONFIG_LCD_HMT
		if(priv->hmt_on == HMT_ON)
			dsim_panel_set_brightness_for_hmt(dsim, 1);
		else
#endif
			dsim_panel_set_brightness(dsim, 1);
		dev_info(dev, "%s: %d, %d\n", __func__, value, priv->temperature);
	}

	return size;
}

static ssize_t color_coordinate_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct panel_private *priv = dev_get_drvdata(dev);

	sprintf(buf, "%u, %u\n", priv->coordinate[0], priv->coordinate[1]);
	return strlen(buf);
}

static ssize_t manufacture_date_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct panel_private *priv = dev_get_drvdata(dev);
	u16 year;
	u8 month, day, hour, min;

	year = ((priv->date[0] & 0xF0) >> 4) + 2011;
	month = priv->date[0] & 0xF;
	day = priv->date[1] & 0x1F;
	hour = priv->date[2] & 0x1F;
	min = priv->date[3] & 0x3F;

	sprintf(buf, "%d, %d, %d, %d:%d\n", year, month, day, hour, min);
	return strlen(buf);
}

static ssize_t read_mtp_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	return strlen(buf);
}

static ssize_t read_mtp_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t size)
{
	struct dsim_device *dsim;
	struct panel_private *priv = dev_get_drvdata(dev);
	unsigned int reg, pos, length, i;
	unsigned char readbuf[256] = {0xff, };
	unsigned char writebuf[2] = {0xB0, };

	dsim = container_of(priv, struct dsim_device, priv);

	sscanf(buf, "%x %d %d", &reg, &pos, &length);

	if (!reg || !length || pos < 0 || reg > 0xff || length > 255 || pos > 255)
		return -EINVAL;
	if (priv->state != PANEL_STATE_RESUMED)
		return -EINVAL;

	writebuf[1] = pos;
	if (dsim_write_hl_data(dsim, SEQ_TEST_KEY_ON_F0, ARRAY_SIZE(SEQ_TEST_KEY_ON_F0)) < 0)
		dsim_err("fail to write F0 on command.\n");

	if (dsim_write_hl_data(dsim, SEQ_TEST_KEY_ON_FC, ARRAY_SIZE(SEQ_TEST_KEY_ON_FC)) < 0)
		dsim_err("fail to write test on fc command.\n");

	if (dsim_write_hl_data(dsim, writebuf, ARRAY_SIZE(writebuf)) < 0)
		dsim_err("fail to write global command.\n");

	if (dsim_read_hl_data(dsim, reg, length, readbuf) < 0)
		dsim_err("fail to read id on command.\n");

	if (dsim_write_hl_data(dsim, SEQ_TEST_KEY_OFF_F0, ARRAY_SIZE(SEQ_TEST_KEY_OFF_F0)) < 0)
		dsim_err("fail to write F0 on command.\n");

	if (dsim_write_hl_data(dsim, SEQ_TEST_KEY_OFF_FC, ARRAY_SIZE(SEQ_TEST_KEY_OFF_FC)) < 0)
		dsim_err("fail to write test on fc command.\n");

	dsim_info("READ_Reg addr : %02x, start pos : %d len : %d \n", reg, pos, length);
	for (i = 0; i < length; i++)
		dsim_info("READ_Reg %dth : %02x \n", i + 1, readbuf[i]);

	return size;
}

#ifdef CONFIG_LCD_BURNIN_CORRECTION
static ssize_t ldu_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct panel_private *priv = dev_get_drvdata(dev);

	sprintf(buf, "%u\n", priv->ldu_correction_state);

	return strlen(buf);
}

static ssize_t ldu_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t size)
{
	struct dsim_device *dsim;
	struct panel_private *priv = dev_get_drvdata(dev);
	int rc;
	int value;

	dsim = container_of(priv, struct dsim_device, priv);

	rc = kstrtouint(buf, 0, &value);

	if (rc < 0)
		return rc;

	if (priv->ldu_correction_state != value) {
		dev_info(dev, "%s: %d, %d\n", __func__, priv->ldu_correction_state, value);
		priv->ldu_correction_state = value;
	}

	return size;
}
static DEVICE_ATTR(ldu, 0000, ldu_show, ldu_store);
#endif

#ifdef CONFIG_PANEL_AID_DIMMING
static void show_aid_log(struct dsim_device *dsim)
{
	int i, j, k;
	struct dim_data *dim_data = NULL;
	struct SmtDimInfo *dimming_info = NULL;
#ifdef CONFIG_LCD_HMT
	struct SmtDimInfo *hmt_dimming_info = NULL;
#endif
	struct panel_private *panel = &dsim->priv;
	u8 temp[256];


	if (panel == NULL) {
		dsim_err("%s:panel is NULL\n", __func__);
		return;
	}

	dim_data = (struct dim_data*)(panel->dim_data);
	if (dim_data == NULL) {
		dsim_info("%s:dimming is NULL\n", __func__);
		return;
	}

	dimming_info = (struct SmtDimInfo*)(panel->dim_info);
	if (dimming_info == NULL) {
		dsim_info("%s:dimming is NULL\n", __func__);
		return;
	}

	dsim_info("MTP VT : %d %d %d\n",
			dim_data->vt_mtp[CI_RED], dim_data->vt_mtp[CI_GREEN], dim_data->vt_mtp[CI_BLUE] );

	for(i = 0; i < VMAX; i++) {
		dsim_info("MTP V%d : %4d %4d %4d \n",
			vref_index[i], dim_data->mtp[i][CI_RED], dim_data->mtp[i][CI_GREEN], dim_data->mtp[i][CI_BLUE] );
	}

	for(i = 0; i < MAX_BR_INFO; i++) {
		memset(temp, 0, sizeof(temp));
		for(j = 1; j < OLED_CMD_GAMMA_CNT; j++) {
			if (j == 1 || j == 3 || j == 5)
				k = dimming_info[i].gamma[j++] * 256;
			else
				k = 0;
			snprintf(temp + strnlen(temp, 256), 256, " %d", dimming_info[i].gamma[j] + k);
		}
		dsim_info("nit :%3d %s\n", dimming_info[i].br, temp);
	}
#ifdef CONFIG_LCD_HMT
	hmt_dimming_info = (struct SmtDimInfo*)(panel->hmt_dim_info);
	if (hmt_dimming_info == NULL) {
		dsim_info("%s:dimming is NULL\n", __func__);
		return;
	}
	for(i = 0; i < HMT_MAX_BR_INFO; i++) {
		memset(temp, 0, sizeof(temp));
		for(j = 1; j < OLED_CMD_GAMMA_CNT; j++) {
			if (j == 1 || j == 3 || j == 5)
				k = hmt_dimming_info[i].gamma[j++] * 256;
			else
				k = 0;
			snprintf(temp + strnlen(temp, 256), 256, " %d", hmt_dimming_info[i].gamma[j] + k);
		}
		dsim_info("nit :%3d %s\n", hmt_dimming_info[i].br, temp);
	}
#endif
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

static ssize_t manufacture_code_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct panel_private *priv = dev_get_drvdata(dev);

	sprintf(buf, "%02X%02X%02X%02X%02X\n",
		priv->code[0], priv->code[1], priv->code[2], priv->code[3], priv->code[4]);

	return strlen(buf);
}

static ssize_t cell_id_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct panel_private *priv = dev_get_drvdata(dev);

	sprintf(buf, "%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X\n",
		priv->date[0] , priv->date[1], priv->date[2], priv->date[3], priv->date[4],
		priv->date[5],priv->date[6], (priv->coordinate[0] &0xFF00)>>8,priv->coordinate[0] &0x00FF,
		(priv->coordinate[1]&0xFF00)>>8,priv->coordinate[1]&0x00FF);

	return strlen(buf);
}

#if defined(CONFIG_PANEL_S6E3HF2_DYNAMIC) || defined(CONFIG_PANEL_S6E3HA2_DYNAMIC)	//only zero
static int find_hbm_table(struct dsim_device *dsim, int nit)
{
	int retVal = 0;
	int i;
	int current_gap;
	int minVal = 20000;

	for (i = 0; i < 256; i++) {
		current_gap = nit - dsim->priv.gallery_br_tbl[i];

		if (current_gap < 0)
			current_gap *= -1;
		if (minVal > current_gap) {
			minVal = current_gap;
			retVal = i;
		}
	}
	return retVal;
}
#endif

static ssize_t adaptive_control_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct panel_private *priv = dev_get_drvdata(dev);

	sprintf(buf, "%d\n", priv->adaptive_control);

	return strlen(buf);
}

#if defined(CONFIG_PANEL_S6E3HF2_DYNAMIC) || defined(CONFIG_PANEL_S6E3HA2_DYNAMIC)	//only zero
static ssize_t adaptive_control_store(struct device *dev, struct device_attribute *attr, const char *buf, size_t size)
{
	int rc;
	int value, i;
	int hbm_nit, origin_nit, p_br, gap_nit;
	int index_array[W_HBM_STEP];
	int update_br;
	int final_br;
	struct dsim_device *dsim;
	struct panel_private *priv = dev_get_drvdata(dev);

	dsim = container_of(priv, struct dsim_device, priv);

	rc = kstrtouint(buf, (unsigned int)0, &value);
	if (rc < 0)
		return rc;

	if (priv->adaptive_control != value) {
		dev_info(dev, "%s: %d, %d\n", __func__, priv->adaptive_control, value);

		if (priv->accessibility || priv->ldu_correction_state) {
			pr_info("%s: don't support gallery change if color blind(%d) or ldu(%d) is already enabled\n", __func__, priv->accessibility, priv->ldu_correction_state);
			return size;
		}

		p_br = priv->bd->props.brightness;
		final_br = priv->bd->props.brightness;

		/* OFF->Gallery or Gallery->OFF */
		hbm_nit = priv->gallery_br_tbl[p_br];

		// index_array[0](=normal_br) ~ index_array[high](=hbm_br) //
		origin_nit = priv->br_tbl[p_br];
		gap_nit = (hbm_nit - origin_nit) / W_HBM_STEP;
		for (i = 0; i < W_HBM_STEP; i++) {
			index_array[i] = find_hbm_table(dsim, origin_nit);
			origin_nit += gap_nit;
		}

		if (value != W_HBM_OFF) {
			mutex_lock(&priv->lock);
			priv->adaptive_control = value;
			mutex_unlock(&priv->lock);
		}

		priv->is_br_override = true;
		for (i = 1; i < W_HBM_STEP; i++) {

			if (value != W_HBM_OFF)
				update_br = index_array[i];
			else
				update_br = index_array[W_HBM_STEP - i];

			priv->override_br_value = update_br;
			priv->bd->props.brightness = update_br;
			dsim_panel_set_brightness(dsim, 0);

			msleep(20);

			if (priv->bd->props.brightness != update_br)
				final_br = priv->bd->props.brightness;
		}
		priv->is_br_override = false;

		if (value == W_HBM_OFF) {
			mutex_lock(&priv->lock);
			priv->adaptive_control = value;
			mutex_unlock(&priv->lock);
		}

		priv->bd->props.brightness = final_br;
		dsim_panel_set_brightness(dsim, 0);
	}

	return size;
}
#else
static ssize_t adaptive_control_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t size)
{
	int rc;
	int value;
	struct dsim_device *dsim;
	struct panel_private *priv = dev_get_drvdata(dev);

	dsim = container_of(priv, struct dsim_device, priv);

	rc = kstrtouint(buf, 0, &value);

	if (rc < 0)
		return rc;
	else {
		if (priv->adaptive_control != value) {
			dev_info(dev, "%s: %d, %d\n", __func__, priv->adaptive_control, value);
			mutex_lock(&priv->lock);
			priv->adaptive_control = value;
			mutex_unlock(&priv->lock);
			dsim_panel_set_brightness(dsim, 1);
		}
	}
	return size;
}
#endif

static ssize_t lux_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct panel_private *priv = dev_get_drvdata(dev);

	sprintf(buf, "%d\n", priv->lux);

	return strlen(buf);
}

static ssize_t lux_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t size)
{
	int rc;
	int value;
	struct panel_private *priv = dev_get_drvdata(dev);

	rc = kstrtoint(buf, 0, &value);

	if (rc < 0)
		return rc;
	else {
		dev_info(dev, "%s: %d, %d\n", __func__, priv->lux, value);
		if (priv->lux != value) {
			mutex_lock(&priv->lock);
			priv->lux = value;
			mutex_unlock(&priv->lock);

			attr_store_for_each(priv->mdnie_class, attr->attr.name, buf, size);
		}
	}
	return size;
}

static ssize_t accessibility_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct panel_private *priv = dev_get_drvdata(dev);

	return sprintf(buf, "%d\n", priv->accessibility);
}

static ssize_t accessibility_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	int rc;
	int value;
	struct dsim_device *dsim;
	struct panel_private *priv = dev_get_drvdata(dev);

	dsim = container_of(priv, struct dsim_device, priv);

	rc = kstrtouint(buf, 0, &value);

	if (rc < 0)
		return rc;
	else {
		if (priv->accessibility != value) {
			dev_info(dev, "%s: %d, %d\n", __func__, priv->accessibility, value);
			priv->accessibility = value;
		}
	}
	return count;
}

#ifdef CONFIG_CHECK_OCTA_CHIP_ID
static ssize_t octa_id_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct panel_private *priv = dev_get_drvdata(dev);
	int site, rework, poc;
	char cell_id[16];
	int i;
	unsigned char* octa_id;

	octa_id = &(priv->octa_id[1]);

	site = octa_id[0] & 0xf0;
	site >>= 4;
	rework = octa_id[0] & 0x0f;
	poc = octa_id[1] & 0x0f;

	dsim_info("site (%d), rework (%d), poc (%d)\n",
			site, rework, poc);

	dsim_info("<CELL ID>\n");
	for(i = 0; i < 16; i++) {
		cell_id[i] = octa_id[i+4];
		dsim_info("%x -> %c\n",octa_id[i+4], cell_id[i]);
	}

	sprintf(buf, "%d%d%d%02x%02x%c%c%c%c%c%c%c%c%c%c%c%c%c%c%c%c\n",
		site, rework, poc, octa_id[2], octa_id[3],
		cell_id[0], cell_id[1], cell_id[2], cell_id[3],
		cell_id[4], cell_id[5], cell_id[6], cell_id[7],
		cell_id[8], cell_id[9], cell_id[10], cell_id[11],
		cell_id[12], cell_id[13], cell_id[14], cell_id[15]);

	return strlen(buf);
}
static DEVICE_ATTR(octa_id, 0444, octa_id_show, NULL);
#endif


#ifdef CONFIG_FB_DSU
static ssize_t resolution_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	int i;
	struct dsim_device *dsim;
	struct panel_private *priv = dev_get_drvdata(dev);

	dsim = container_of(priv, struct dsim_device, priv);
	for( i=0; i<ARRAY_SIZE(dsu_config); i++ ) {
		if( dsu_config[i].value == dsim->dsu_param_value ) {
			strcpy( buf, dsu_config[i].id_str );
			pr_info( "%s:%s,%d\n", __func__, dsu_config[i].id_str, dsu_config[i].value );
			return strlen(buf);
			break;
		}
	}

	strcpy(buf, "WQHD" );
	pr_err( "%s:(default)%s,%d\n", __func__, buf, DSU_CONFIG_WQHD );
	return strlen(buf);
}

static ssize_t resolution_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t size)
{
	struct dsim_device *dsim;
	struct panel_private *priv = dev_get_drvdata(dev);
	int ret;
	int i;

	dsim = container_of(priv, struct dsim_device, priv);
	if( dsim->dsu_param_offset==0 ) {
		pr_err( "%s: failed. offset not exist\n", __func__ );
	}

	for( i=0; i<ARRAY_SIZE(dsu_config); i++ ) {
		if( !strncmp(dsu_config[i].id_str, buf, strlen(dsu_config[i].id_str)) ) {
			dsim->dsu_param_value = dsu_config[i].value;
			ret = sec_set_param((unsigned long) dsim->dsu_param_offset, dsim->dsu_param_value);
			pr_info( "%s:%s,%d,%d\n", __func__, dsu_config[i].id_str, dsim->dsu_param_value, ret );
			return size;
		}
	}

	pr_err( "%s: failed (%s)\n", __func__, buf );
	return size;
}
static DEVICE_ATTR(resolution, 0664, resolution_show, resolution_store);
#endif

static DEVICE_ATTR(accessibility, 0000, accessibility_show, accessibility_store);
static DEVICE_ATTR(adaptive_control, 0664, adaptive_control_show, adaptive_control_store);
static DEVICE_ATTR(lcd_type, 0444, lcd_type_show, NULL);
static DEVICE_ATTR(window_type, 0444, window_type_show, NULL);
static DEVICE_ATTR(manufacture_code, 0444, manufacture_code_show, NULL);
static DEVICE_ATTR(cell_id, 0444, cell_id_show, NULL);
static DEVICE_ATTR(brightness_table, 0444, brightness_table_show, NULL);
static DEVICE_ATTR(siop_enable, 0664, siop_enable_show, siop_enable_store);
static DEVICE_ATTR(temperature, 0664, temperature_show, temperature_store);
static DEVICE_ATTR(color_coordinate, 0444, color_coordinate_show, NULL);
static DEVICE_ATTR(manufacture_date, 0444, manufacture_date_show, NULL);
static DEVICE_ATTR(read_mtp, 0664, read_mtp_show, read_mtp_store);
#ifdef CONFIG_PANEL_AID_DIMMING
static DEVICE_ATTR(aid_log, 0444, aid_log_show, NULL);
#endif
static DEVICE_ATTR(lux, 0644, lux_show, lux_store);

static struct attribute *lcd_sysfs_attributes[] = {
	&dev_attr_lcd_type.attr,
	&dev_attr_window_type.attr,
	&dev_attr_manufacture_code.attr,
	&dev_attr_cell_id.attr,
	&dev_attr_brightness_table.attr,
	&dev_attr_siop_enable.attr,
	&dev_attr_temperature.attr,
	&dev_attr_color_coordinate.attr,
	&dev_attr_manufacture_date.attr,
#ifdef CONFIG_PANEL_AID_DIMMING
	&dev_attr_aid_log.attr,
#endif
	&dev_attr_read_mtp.attr,
#if defined(CONFIG_SEC_FACTORY) && defined(CONFIG_EXYNOS_DECON_LCD_MCD)
	&dev_attr_mcd_mode.attr,
#endif
#ifdef CONFIG_LCD_HMT
#ifdef CONFIG_PANEL_AID_DIMMING
	&dev_attr_hmt_bright.attr,
#endif
	&dev_attr_hmt_on.attr,
#endif
#if defined(CONFIG_LCD_ALPM) || defined(CONFIG_LCD_DOZE_MODE)
	&dev_attr_alpm.attr,
#endif
	&dev_attr_adaptive_control.attr,
	&dev_attr_lux.attr,
#ifdef CONFIG_LCD_BURNIN_CORRECTION
	&dev_attr_ldu.attr,
#endif
	&dev_attr_accessibility.attr,
#ifdef CONFIG_CHECK_OCTA_CHIP_ID
	&dev_attr_octa_id.attr,
#endif
#ifdef CONFIG_FB_DSU
	&dev_attr_resolution.attr,
#endif
	NULL,
};


static const struct attribute_group lcd_sysfs_attr_group = {
	.attrs = lcd_sysfs_attributes,
};

static const struct attribute_group *lcd_sysfs_attr_groups[] = {
	&lcd_sysfs_attr_group,
	NULL,
};

void lcd_init_sysfs(struct dsim_device *dsim)
{
	int ret = -1;

	ret = sysfs_create_group(&dsim->lcd->dev.kobj, &lcd_sysfs_attr_group);
	if (ret < 0)
		dev_err(&dsim->lcd->dev, "failed to add lcd sysfs\n");

	dsim->lcd->dev.groups = lcd_sysfs_attr_groups;
}


