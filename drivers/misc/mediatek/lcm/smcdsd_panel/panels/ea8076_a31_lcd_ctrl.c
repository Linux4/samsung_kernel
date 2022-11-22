/*
 * Copyright (c) Samsung Electronics Co., Ltd.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/backlight.h>
#include <linux/ctype.h>
#include <linux/delay.h>
#include <linux/gpio.h>
#include <linux/lcd.h>
#include <linux/module.h>
#include <linux/of_device.h>
#include <linux/reboot.h>
#include <video/mipi_display.h>
#if defined(CONFIG_SMCDSD_DYNAMIC_MIPI)
#include <linux/dev_ril_bridge.h>
#endif

#include "../smcdsd_board.h"
#include "../smcdsd_panel.h"
#include "smcdsd_notify.h"

#if defined(CONFIG_SMCDSD_EA8076_A41_JPN)
#include "ea8076_a41_jpn_param.h"
#elif defined(CONFIG_SMCDSD_EA8076_A41)
#include "ea8076_a41_param.h"
#else
#include "ea8076_a31_param.h"
#endif
#if defined(CONFIG_SMCDSD_DYNAMIC_MIPI)
#include "ea8076_a31_freq.h"
#endif

#include "dd.h"
#include "timenval.h"

#if defined(CONFIG_SMCDSD_MDNIE)
#include "mdnie.h"
#if defined(CONFIG_SMCDSD_EA8076_A41) || defined(CONFIG_SMCDSD_EA8076_A41_JPN)
#include "ea8076_a41_mdnie.h"
#else
#include "ea8076_a31_mdnie.h"
#endif
#endif

#if defined(CONFIG_SMCDSD_DPUI)
#include "dpui.h"

#define	DPUI_VENDOR_NAME	"SDC"
#define DPUI_MODEL_NAME		"AMS638VL01"
#endif

#if defined(CONFIG_SMCDSD_DOZE)
#include "disp_helper.h"
#endif

#define dbg_info(fmt, ...)	pr_info(pr_fmt("%s: %3d: %s: " fmt), "lcd panel", __LINE__, __func__, ##__VA_ARGS__)

#define PANEL_STATE_SUSPENED	0
#define PANEL_STATE_RESUMED	1

#define LEVEL_IS_HBM(brightness)		(brightness > UI_MAX_BRIGHTNESS)

#define DSI_WRITE(cmd, size)		do {				\
{	\
	int tx_ret = 0;	\
	tx_ret = smcdsd_dsi_tx_data(lcd, cmd, size);			\
	if (tx_ret < 0)							\
		dev_info(&lcd->ld->dev, "%s: tx_ret(%d) failed to write %s\n", __func__, tx_ret, #cmd);	\
}	\
} while (0)

#define get_bit(value, shift, width)	((value >> shift) & (GENMASK(width - 1, 0)))

union wrctrld_info {
	u32 value;
	struct {
		u8 bl_reg2;
		u8 bl_reg1;
		u8 hbm;
		u8 reserved;
	};
};

union elvss_info {
	u32 value;
	struct {
		u8 offset;
		u8 tset;
		u16 reserved;
	};
};

union lpm_info {
	u32 value;
	struct {
		u8 state;
		u8 mode;	/* comes from sysfs. 0(off), 1(alpm 2nit), 2(hlpm 2nit), 3(alpm 60nit), 4(hlpm 60nit) or 1(alpm), 2(hlpm) */
		u8 ver;		/* comes from sysfs. 0(old), 1(new) */
		u16 reserved;
	};
};

struct lcd_info {
	unsigned int			connected;
	unsigned int			brightness;
	union elvss_info		current_elvss;
	unsigned int			current_acl;
	union wrctrld_info		current_wrctrld;
	unsigned int			state;
	unsigned int			enable;
	unsigned int			shutdown;

	struct lcd_device		*ld;
	struct backlight_device		*bd;
	struct device			svc_dev;

	unsigned char			**acl_table;
	unsigned char			**acl_dim_table;
#if defined(CONFIG_SMCDSD_DYNAMIC_MIPI)
	unsigned char			**ffc_table;
#endif
	unsigned char			 *((*hbm_table)[HBM_STATUS_MAX]);

	union {
		struct {
			u8		reserved;
			u8		id[LDI_LEN_ID];
		};
		u32			value;
	} id_info;
	unsigned char			code[LDI_LEN_CHIP_ID];
	unsigned char			date[LDI_LEN_DATE];
	unsigned int			coordinate[2];
	unsigned char			coordinates[20];
	unsigned char			manufacture_info[LDI_LEN_MANUFACTURE_INFO + LDI_LEN_MANUFACTURE_INFO_CELL_ID];
	unsigned char			rddpm;
	unsigned char			rddsm;

	int				temperature;
	unsigned int			temperature_index;

	unsigned int			adaptive_control;
	int				lux;
	struct class			*mdnie_class;

	struct mipi_dsi_lcd_parent	*pdata;
	struct mutex			lock;

	struct notifier_block		fb_notifier;
	struct notifier_block		reboot_notifier;
#if defined(CONFIG_SMCDSD_DYNAMIC_MIPI)
	struct notifier_block		df_notif;
#endif
#if defined(CONFIG_SEC_FACTORY)
	unsigned int			enable_fd;
#endif

#if defined(CONFIG_SMCDSD_DPUI)
	struct notifier_block		dpui_notif;
#endif

#if defined(CONFIG_SMCDSD_DOZE)
	union lpm_info			alpm;
	union lpm_info			current_alpm;

#if defined(CONFIG_SEC_FACTORY)
	unsigned int			prev_brightness;
	union lpm_info			prev_alpm;
#endif
#endif
	unsigned int			mask_brightness;
	unsigned int			actual_mask_brightness;
	bool				mask_state;

	unsigned int			trans_dimming;
	unsigned int			acl_dimming;
	unsigned int			acl_dimming_update_req;

	unsigned int			display_on_delay_req;
	unsigned int			mask_framedone_check_req;

	unsigned int			conn_init_done;
	unsigned int			conn_det_enable;
	unsigned int			conn_det_count;

#if defined(CONFIG_SMCDSD_DYNAMIC_MIPI)
	unsigned int			df_index_req;
	unsigned int			df_index_current;
#endif
	struct workqueue_struct		*conn_workqueue;
	struct work_struct		conn_work;

	struct timenval			tnv[2];
};

static struct lcd_info *get_lcd_info(struct platform_device *p)
{
	return platform_get_drvdata(p);
}

static int smcdsd_dsi_tx_data(struct lcd_info *lcd, u8 *cmd, u32 len)
{
	int ret = 0;
	int retry = 2;

	if (!lcd->connected)
		return ret;

try_write:
	if (len == 1)
		ret = lcd->pdata->tx(MIPI_DSI_DCS_SHORT_WRITE, cmd[0], 0);
	else if (len == 2)
		ret = lcd->pdata->tx(MIPI_DSI_DCS_SHORT_WRITE_PARAM, cmd[0], cmd[1]);
	else
		ret = lcd->pdata->tx(MIPI_DSI_DCS_LONG_WRITE, (unsigned long)cmd, len);

	if (ret < 0) {
		if (--retry)
			goto try_write;
		else
			dev_info(&lcd->ld->dev, "%s: fail. %02x, ret: %d\n", __func__, cmd[0], ret);
	}

	return ret;
}

static int _smcdsd_dsi_rx_data(struct lcd_info *lcd, u8 cmd, u32 size, u8 *buf, u32 offset)
{
	int ret = 0, rx_size = 0;
	int retry = 2;

	if (!lcd->connected)
		return ret;

try_read:
	rx_size = lcd->pdata->rx(MIPI_DSI_DCS_READ, offset, cmd, size, buf);
	dev_info(&lcd->ld->dev, "%s: %2d(%2d), %02x, %*ph%s\n", __func__, size, rx_size, cmd,
		min_t(u32, min_t(u32, size, rx_size), 5), buf, (rx_size > 5) ? "..." : "");
	if (rx_size != size) {
		if (--retry)
			goto try_read;
		else {
			dev_info(&lcd->ld->dev, "%s: fail. %02x, %d(%d)\n", __func__, cmd, size, rx_size);
			ret = -EPERM;
		}
	}

	return ret;
}

static int _smcdsd_dsi_rx_data_offset(struct lcd_info *lcd, u8 addr, u32 size, u8 *buf, u32 offset)
{
	int ret = 0;

	ret = _smcdsd_dsi_rx_data(lcd, addr, size, buf, offset);
	if (ret < 0)
		dev_info(&lcd->ld->dev, "%s: fail\n", __func__);

	return ret;
}

static int smcdsd_dsi_rx_data(struct lcd_info *lcd, u8 addr, u32 size, u8 *buf)
{
	int ret = 0, remain, limit, offset, slice;

	limit = 10;
	remain = size;

again:
	offset = size - remain;
	slice = (remain > limit) ? limit : remain;

	ret = _smcdsd_dsi_rx_data_offset(lcd, addr, slice, &buf[offset], offset);
	if (ret < 0) {
		dev_info(&lcd->ld->dev, "%s: fail\n", __func__);
		return ret;
	}

	remain -= limit;

	if (remain > 0)
		goto again;

	return ret;
}

static int smcdsd_dsi_rx_info(struct lcd_info *lcd, u8 reg, u32 len, u8 *buf)
{
	int ret = 0, i;

	ret = smcdsd_dsi_rx_data(lcd, reg, len, buf);
	if (ret < 0) {
		dev_info(&lcd->ld->dev, "%s: fail. %02x, ret: %d\n", __func__, reg, ret);
		goto exit;
	}

	dev_dbg(&lcd->ld->dev, "%s: %02xh\n", __func__, reg);
	for (i = 0; i < len; i++)
		dev_dbg(&lcd->ld->dev, "%02dth value is %02x, %3d\n", i + 1, buf[i], buf[i]);

exit:
	return ret;
}

#if defined(CONFIG_SMCDSD_MDNIE) || defined(CONFIG_SMCDSD_DOZE)
static int smcdsd_dsi_tx_set(struct lcd_info *lcd, struct lcd_seq_info *seq, u32 num)
{
	int ret = 0, i;

	for (i = 0; i < num; i++) {
		if (seq[i].cmd) {
			ret = smcdsd_dsi_tx_data(lcd, seq[i].cmd, seq[i].len);
			if (ret < 0) {
				dev_info(&lcd->ld->dev, "%s: %dth fail\n", __func__, i);
				return ret;
			}
		}
		if (seq[i].sleep)
			usleep_range(seq[i].sleep, seq[i].sleep + (seq[i].sleep >> 1));
	}
	return ret;
}
#endif

static int smcdsd_panel_set_elvss(struct lcd_info *lcd, u8 force)
{
	int ret = 0;
	union elvss_info elvss_value = {0, };
	unsigned char offset, tset = 0;

	offset = elvss_table[lcd->brightness];
	tset = ((lcd->temperature < 0) ? BIT(7) : 0) | abs(lcd->temperature);

	elvss_value.offset = SEQ_ELVSS_SET[LDI_OFFSET_ELVSS_1] = offset;
	elvss_value.tset = SEQ_ELVSS_SET[LDI_OFFSET_ELVSS_2] = tset;

	if (force)
		goto update;
	else if (lcd->current_elvss.value != elvss_value.value)
		goto update;
	else
		goto exit;

update:
	DSI_WRITE(SEQ_ELVSS_SET, ELVSS_CMD_CNT);
	lcd->current_elvss.value = elvss_value.value;
	dev_info(&lcd->ld->dev, "elvss: %x\n", lcd->current_elvss.value);

exit:
	return ret;
}

static int smcdsd_panel_set_wrctrld(struct lcd_info *lcd, u8 force)
{
	int ret = 0;
	unsigned char bl_reg[3] = {0, };
	union wrctrld_info wrctrld = {0, };
	unsigned char hbm_level = 0;

	hbm_level = LEVEL_IS_HBM(lcd->brightness);

	bl_reg[0] = LDI_REG_BRIGHTNESS;
	wrctrld.bl_reg1 = bl_reg[1] = get_bit(brightness_table[lcd->brightness], 8, 2);
	wrctrld.bl_reg2 = bl_reg[2] = get_bit(brightness_table[lcd->brightness], 0, 8);
	wrctrld.hbm = lcd->hbm_table[lcd->trans_dimming][hbm_level][LDI_OFFSET_HBM];

	if (force || lcd->current_wrctrld.value != wrctrld.value)
		DSI_WRITE(lcd->hbm_table[lcd->trans_dimming][hbm_level], HBM_CMD_CNT);

	smcdsd_panel_set_elvss(lcd, force);

	if (force || lcd->current_wrctrld.value != wrctrld.value) {
		DSI_WRITE(bl_reg, ARRAY_SIZE(bl_reg));
		lcd->current_wrctrld.value = wrctrld.value;
	}

	return ret;
}

static int smcdsd_panel_set_acl(struct lcd_info *lcd, int force)
{
	int ret = 0, opr_status = ACL_STATUS_15P;
	unsigned int acl_value = 0;

	if (lcd->mask_state)
		opr_status = brightness_opr_table[ACL_STATUS_OFF][lcd->brightness];
	else
		opr_status = brightness_opr_table[!!lcd->adaptive_control][lcd->brightness];

	acl_value = lcd->acl_table[opr_status][LDI_OFFSET_ACL];

	if (force)
		goto update;
	else if (lcd->current_acl != acl_value)
		goto update;
	else if (lcd->acl_dimming_update_req)
		goto update;
	else
		goto exit;

update:
	if (lcd->acl_dimming_update_req) {
		DSI_WRITE(SEQ_ACL_DIM_OFFSET, ARRAY_SIZE(SEQ_ACL_DIM_OFFSET));
		DSI_WRITE(lcd->acl_dim_table[lcd->acl_dimming], ACL_DIM_CMD_CNT);
		lcd->acl_dimming_update_req = 0;
	}
	DSI_WRITE(lcd->acl_table[opr_status], ACL_CMD_CNT);
	lcd->current_acl = acl_value;
	dev_info(&lcd->ld->dev, "acl dim %x %x\n", lcd->acl_dim_table[lcd->acl_dimming][0],
			lcd->acl_dim_table[lcd->acl_dimming][ACL_DIM_FRAME_OFFSET]);
	dev_info(&lcd->ld->dev, "acl: %x, brightness: %d, adaptive_control: %d\n",
			lcd->current_acl, lcd->brightness, lcd->adaptive_control);

exit:
	return ret;
}

static int panel_bl_update_average(struct lcd_info *lcd, size_t index)
{
	struct timespec cur_ts;
	int cur_brt;

	if (index >= ARRAY_SIZE(lcd->tnv))
		return -EINVAL;

	ktime_get_ts(&cur_ts);
	cur_brt = nit_table[lcd->brightness];
	timenval_update_snapshot(&lcd->tnv[index], cur_brt, cur_ts);

	return 0;
}

static int panel_bl_clear_average(struct lcd_info *lcd, size_t index)
{
	if (index >= ARRAY_SIZE(lcd->tnv))
		return -EINVAL;

	timenval_clear_average(&lcd->tnv[index]);

	return 0;
}

static int panel_bl_get_average_and_clear(struct lcd_info *lcd, size_t index)
{
	int avg;

	if (index >= ARRAY_SIZE(lcd->tnv))
		return -EINVAL;

	mutex_lock(&lcd->lock);
	panel_bl_update_average(lcd, index);
	avg = lcd->tnv[index].avg;
	panel_bl_clear_average(lcd, index);
	mutex_unlock(&lcd->lock);

	return avg;
}

static int panel_bl_init_average(struct lcd_info *lcd, size_t index)
{
	struct timespec cur_ts;
	int cur_brt;

	if (index >= ARRAY_SIZE(lcd->tnv))
		return -EINVAL;

	ktime_get_ts(&cur_ts);
	cur_brt = nit_table[lcd->brightness];
	timenval_init(&lcd->tnv[index]);

	return 0;
}

static ssize_t brt_avg_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct lcd_info *lcd = dev_get_drvdata(dev);
	int brt_avg;

	if (lcd->state != PANEL_STATE_RESUMED) {
		dev_info(&lcd->ld->dev, "%s: panel state is %d\n", __func__, lcd->state);
		return snprintf(buf, PAGE_SIZE, "-1\n");
	}

	brt_avg = panel_bl_get_average_and_clear(lcd, 1);
	if (brt_avg < 0) {
		dev_info(&lcd->ld->dev, "%s: failed to get average brt1\n", __func__);
		return snprintf(buf, PAGE_SIZE, "-1\n");
	}

	dev_info(&lcd->ld->dev, "%s: %d\n", __func__, brt_avg);

	return snprintf(buf, PAGE_SIZE, "%d\n", brt_avg);
}

#if defined(CONFIG_SMCDSD_DOZE)
static int ea8076_setalpm(struct lcd_info *lcd)
{
	int ret = 0;
	union lpm_info lpm = {0, };

	dev_info(&lcd->ld->dev, "%s: brightness: %3d, lpm: %06x(%06x)\n", __func__,
		lcd->bd->props.brightness, lcd->current_alpm.value, lcd->alpm.value);

	lpm.value = lcd->alpm.value;
	lpm.state = (lpm.ver && lpm.mode) ? lpm_brightness_table[lcd->bd->props.brightness] : lpm_old_table[lpm.mode];

	switch (lpm.state) {
	case AOD_HLPM_02_NIT:
		smcdsd_dsi_tx_set(lcd, LCD_SEQ_HLPM_02_NIT, ARRAY_SIZE(LCD_SEQ_HLPM_02_NIT));
		break;
	case AOD_HLPM_10_NIT:
		smcdsd_dsi_tx_set(lcd, LCD_SEQ_HLPM_10_NIT, ARRAY_SIZE(LCD_SEQ_HLPM_10_NIT));
		break;
	case AOD_HLPM_30_NIT:
		smcdsd_dsi_tx_set(lcd, LCD_SEQ_HLPM_30_NIT, ARRAY_SIZE(LCD_SEQ_HLPM_30_NIT));
		break;
	case AOD_HLPM_60_NIT:
		smcdsd_dsi_tx_set(lcd, LCD_SEQ_HLPM_60_NIT, ARRAY_SIZE(LCD_SEQ_HLPM_60_NIT));
		break;
	}

	lcd->current_alpm.value = lpm.value;

	dev_info(&lcd->ld->dev, "%s: %s\n", __func__, (lpm.state < AOD_HLPM_STATE_MAX) ? AOD_HLPM_STATE_NAME[lpm.state] : "unknown");

	return ret;
}

static int ea8076_enteralpm(struct lcd_info *lcd)
{
	int ret = 0;
	union lpm_info lpm = {0, };

	dev_info(&lcd->ld->dev, "%s: brightness: %3d, lpm: %06x(%06x)\n", __func__,
		lcd->bd->props.brightness, lcd->current_alpm.value, lcd->alpm.value);

	mutex_lock(&lcd->lock);

	if (lcd->state == PANEL_STATE_SUSPENED) {
		dev_info(&lcd->ld->dev, "%s: panel state is %d\n", __func__, lcd->state);
		goto exit;
	}

	lpm.value = lcd->alpm.value;
	lpm.state = (lpm.ver && lpm.mode) ? lpm_brightness_table[lcd->bd->props.brightness] : lpm_old_table[lpm.mode];

	if (lcd->current_alpm.value == lpm.value) {
		dev_info(&lcd->ld->dev, "%s: lpm: %06x(%06x) skip same value\n", __func__,
			lcd->current_alpm.value, lpm.value);
		goto exit;
	}

	/* 2. Image Write for HLPM Mode */
	/* 3. HLPM/ALPM On Setting */
	ret = ea8076_setalpm(lcd);
	if (ret < 0)
		dev_info(&lcd->ld->dev, "%s: failed to set alpm\n", __func__);

	/* 4. Wait 16.7ms */
	msleep(20);

	/* 5. Display On(29h) */
	/* ea8076_displayon(lcd); */

	lcd->current_alpm.value = lpm.value;
exit:
	mutex_unlock(&lcd->lock);
	return ret;
}

static int ea8076_exitalpm(struct lcd_info *lcd)
{
	int ret = 0;

	dev_info(&lcd->ld->dev, "%s: brightness: %3d, lpm: %06x(%06x)\n", __func__,
		lcd->bd->props.brightness, lcd->current_alpm.value, lcd->alpm.value);

	mutex_lock(&lcd->lock);

	if (lcd->state == PANEL_STATE_SUSPENED) {
		dev_info(&lcd->ld->dev, "%s: panel state is %d\n", __func__, lcd->state);
		goto exit;
	}

	/* 5. HLPM/ALPM Off Setting */
	smcdsd_dsi_tx_set(lcd, LCD_SEQ_HLPM_OFF, ARRAY_SIZE(LCD_SEQ_HLPM_OFF));

	dev_info(&lcd->ld->dev, "%s: HLPM_OFF\n", __func__);

	msleep(34);

	lcd->current_alpm.value = 0;

	panel_bl_init_average(lcd, 1);
exit:
	mutex_unlock(&lcd->lock);
	return ret;
}
#endif

static int low_level_set_brightness(struct lcd_info *lcd, int force)
{
	DSI_WRITE(SEQ_TEST_KEY_ON_F0, ARRAY_SIZE(SEQ_TEST_KEY_ON_F0));

	smcdsd_panel_set_wrctrld(lcd, force);

	smcdsd_panel_set_acl(lcd, force);

	DSI_WRITE(SEQ_TEST_KEY_OFF_F0, ARRAY_SIZE(SEQ_TEST_KEY_OFF_F0));

	return 0;
}

static int _smcdsd_panel_set_brightness(struct lcd_info *lcd, int force, bool lock)
{
	int ret = 0;

	if (lock)
		mutex_lock(&lcd->lock);

#if defined(CONFIG_SMCDSD_DOZE)
	if (lcd->current_alpm.state && lcd->current_alpm.ver && lcd->enable && !force) {
		ea8076_setalpm(lcd);
		goto exit;
	} else if (lcd->current_alpm.state) {
		dev_info(&lcd->ld->dev, "%s: brightness: %3d, lpm: %06x(%06x)\n", __func__,
			lcd->bd->props.brightness, lcd->current_alpm.value, lcd->alpm.value);
		goto exit;
	}
#endif

	lcd->brightness = lcd->bd->props.brightness;

	if (lcd->mask_state) {
		dev_info(&lcd->ld->dev, "[SEC_MASK] %s: brightness: %d -> %d mask_layer: %d\n", __func__,
		lcd->brightness, lcd->mask_brightness, lcd->mask_state);
		lcd->brightness = lcd->mask_brightness;
	}

	if (!force && (lcd->state != PANEL_STATE_RESUMED || !lcd->enable)) {
		dev_info(&lcd->ld->dev, "%s: brightness: %d(%d), panel_state: %d, %d\n", __func__,
		lcd->brightness, lcd->bd->props.brightness, lcd->state, lcd->enable);
		goto exit;
	}

	low_level_set_brightness(lcd, force);

	dev_info(&lcd->ld->dev, "brightness: %3d, %4d, %6x, lx: %d\n", lcd->brightness,
		brightness_table[lcd->brightness], lcd->current_wrctrld.value, lcd->lux);

	if (!force)
		panel_bl_update_average(lcd, 1);
exit:
	if (lock)
		mutex_unlock(&lcd->lock);

	return ret;
}

static int smcdsd_panel_set_brightness(struct lcd_info *lcd, int force)
{
	int ret = 0;

	_smcdsd_panel_set_brightness(lcd, force, true);

	return ret;
}

static int panel_get_brightness(struct backlight_device *bd)
{
	struct lcd_info *lcd = bl_get_data(bd);

	return brightness_table[lcd->brightness];
}

static int panel_set_brightness(struct backlight_device *bd)
{
	int ret = 0;
	struct lcd_info *lcd = bl_get_data(bd);

	if (lcd->mask_state == true) {
		dev_info(&lcd->ld->dev, "%s: skip! MASK LAYER\n", __func__);
		return ret;
	}

	if (lcd->state == PANEL_STATE_RESUMED) {
		ret = smcdsd_panel_set_brightness(lcd, 0);
		if (ret < 0)
			dev_info(&lcd->ld->dev, "%s: failed to set brightness\n", __func__);
	}

	return ret;
}

static int panel_check_fb_brightness(struct backlight_device *bd, struct fb_info *fb)
{
	return 0;
}

static const struct backlight_ops panel_backlight_ops = {
	.get_brightness = panel_get_brightness,
	.update_status = panel_set_brightness,
	.check_fb = panel_check_fb_brightness,
};

static int ea8076_read_id(struct lcd_info *lcd)
{
	int ret = 0;
	struct mipi_dsi_lcd_parent *pdata = lcd->pdata;
	static char *LDI_BIT_DESC_ID[BITS_PER_BYTE * LDI_LEN_ID] = {
		[0 ... 23] = "ID Read Fail",
	};

	lcd->id_info.value = 0;
	pdata->lcdconnected = lcd->connected = lcdtype ? 1 : 0;

	ret = smcdsd_dsi_rx_info(lcd, LDI_REG_ID, LDI_LEN_ID, lcd->id_info.id);
	if (ret < 0 || !lcd->id_info.value) {
		pdata->lcdconnected = lcd->connected = 0;
		dev_info(&lcd->ld->dev, "%s: connected lcd is invalid\n", __func__);

		if (lcdtype && pdata)
			smcdsd_abd_save_bit(&pdata->abd, BITS_PER_BYTE * LDI_LEN_ID, cpu_to_be32(lcd->id_info.value), LDI_BIT_DESC_ID);
	}

	dev_info(&lcd->ld->dev, "%s: %x\n", __func__, cpu_to_be32(lcd->id_info.value));

	return ret;
}

static int ea8076_read_coordinate(struct lcd_info *lcd)
{
	int ret = 0;
	unsigned char buf[LDI_GPARA_MANUFACTURE_INFO + LDI_LEN_MANUFACTURE_INFO] = {0, };

	ret = smcdsd_dsi_rx_info(lcd, LDI_REG_COORDINATE, ARRAY_SIZE(buf), buf);
	if (ret < 0)
		dev_info(&lcd->ld->dev, "%s: fail\n", __func__);

	lcd->coordinate[0] = buf[LDI_GPARA_COORDINATE + 0] << 8 | buf[LDI_GPARA_COORDINATE + 1];	/* X */
	lcd->coordinate[1] = buf[LDI_GPARA_COORDINATE + 2] << 8 | buf[LDI_GPARA_COORDINATE + 3];	/* Y */

	scnprintf(lcd->coordinates, sizeof(lcd->coordinates), "%d %d\n", lcd->coordinate[0], lcd->coordinate[1]);

	memcpy(lcd->date, &buf[LDI_GPARA_DATE], LDI_LEN_DATE);

	memcpy(lcd->manufacture_info, &buf[LDI_GPARA_MANUFACTURE_INFO], LDI_LEN_MANUFACTURE_INFO);

	return ret;
}

static int ea8076_read_manufacture_info(struct lcd_info *lcd)
{
	int ret = 0;
	unsigned char buf[LDI_GPARA_MANUFACTURE_INFO_CELL_ID + LDI_LEN_MANUFACTURE_INFO_CELL_ID] = {0, };

	ret = smcdsd_dsi_rx_info(lcd, LDI_REG_MANUFACTURE_INFO_CELL_ID, ARRAY_SIZE(buf), buf);
	if (ret < 0)
		dev_info(&lcd->ld->dev, "%s: fail\n", __func__);

	memcpy(&lcd->manufacture_info[LDI_LEN_MANUFACTURE_INFO], &buf[LDI_GPARA_MANUFACTURE_INFO_CELL_ID], LDI_LEN_MANUFACTURE_INFO_CELL_ID);

	return ret;
}

static int ea8076_read_chip_id(struct lcd_info *lcd)
{
	int ret = 0;
	unsigned char buf[LDI_LEN_CHIP_ID] = {0, };
	unsigned char wbuf[] = {0xB0, LDI_GPARA_CHIP_ID};

	DSI_WRITE(wbuf, ARRAY_SIZE(wbuf));
	ret = smcdsd_dsi_rx_info(lcd, LDI_REG_CHIP_ID, LDI_LEN_CHIP_ID, buf);
	if (ret < 0)
		dev_info(&lcd->ld->dev, "%s: fail\n", __func__);

	memcpy(lcd->code, buf, LDI_LEN_CHIP_ID);

	return ret;
}

static int ea8076_read_elvss(struct lcd_info *lcd)
{
	int ret = 0;
	unsigned char buf[LDI_LEN_ELVSS] = {0, };

	ret = smcdsd_dsi_rx_info(lcd, LDI_REG_ELVSS, LDI_LEN_ELVSS, buf);
	if (ret < 0)
		dev_info(&lcd->ld->dev, "%s: fail\n", __func__);

	SEQ_ELVSS_SET[LDI_GPARA_ELVSS_NORMAL + 1] = buf[LDI_GPARA_ELVSS_NORMAL];
	SEQ_ELVSS_SET[LDI_GPARA_ELVSS_HBM + 1] = buf[LDI_GPARA_ELVSS_HBM];

	return ret;
}

static int panel_read_bit_info(struct lcd_info *lcd, u32 index, u8 *rxbuf)
{
	int ret = 0;
	u8 buf[5] = {0, };
	struct bit_info *bit_info_list = ldi_bit_info_list;
	unsigned int reg, len, mask, expect, offset, invert, print_tag, bit, i, bit_max = 0, merged = 0;
	char **print_org = NULL;
	char *print_new[sizeof(u32) * BITS_PER_BYTE] = {0, };
	struct mipi_dsi_lcd_parent *pdata = lcd->pdata;

	if (!lcd->connected)
		return ret;

	if (index >= LDI_BIT_ENUM_MAX) {
		dev_info(&lcd->ld->dev, "%s: invalid index(%d)\n", __func__, index);
		ret = -EINVAL;
		return ret;
	}

	reg = bit_info_list[index].reg;
	len = bit_info_list[index].len;
	bit_max = len * BITS_PER_BYTE;
	print_org = bit_info_list[index].print;
	expect = bit_info_list[index].expect;
	offset = bit_info_list[index].offset;
	invert = bit_info_list[index].invert;
	mask = bit_info_list[index].mask;
	if (!mask) {
		for (bit = 0; bit < bit_max; bit++) {
			if (print_org[bit])
				mask |= BIT(bit);
		}
		bit_info_list[index].mask = mask;
	}

	if (offset + len > ARRAY_SIZE(buf)) {
		dev_info(&lcd->ld->dev, "%s: invalid length(%d) or offset(%d)\n", __func__, len, offset);
		ret = -EINVAL;
		return ret;
	}

	if (len > sizeof(u32)) {
		dev_info(&lcd->ld->dev, "%s: invalid length(%d)\n", __func__, len);
		ret = -EINVAL;
		return ret;
	}

	ret = smcdsd_dsi_rx_info(lcd, reg, offset + len, buf);
	if (ret < 0) {
		dev_info(&lcd->ld->dev, "%s: fail\n", __func__);
		return ret;
	}

	for (i = 0; i < len; i++)
		merged |= buf[offset + i] << (BITS_PER_BYTE * i);

	print_tag = merged & mask;
	print_tag = print_tag ^ invert;

	memcpy(&bit_info_list[index].result, &buf[offset], len);

	if (rxbuf)
		memcpy(rxbuf, &buf[offset], len);

	if (print_tag) {
		for_each_set_bit(bit, (unsigned long *)&print_tag, bit_max) {
			if (print_org[bit])
				print_new[bit] = print_org[bit];
		}

		if (likely(pdata)) {
			dev_info(&lcd->ld->dev, "==================================================\n");
			smcdsd_abd_save_bit(&pdata->abd, len * BITS_PER_BYTE, merged, print_new);
		}
		dev_info(&lcd->ld->dev, "==================================================\n");
		dev_info(&lcd->ld->dev, "%s: 0x%02X is invalid. 0x%0*X(expect %0*X)\n", __func__, reg, bit_max >> 2, merged, bit_max >> 2, expect);
		for (bit = 0; bit < bit_max; bit++) {
			if (print_new[bit]) {
				if (!bit || !print_new[bit - 1] || strcmp(print_new[bit - 1], print_new[bit]))
					dev_info(&lcd->ld->dev, "* %s (NG)\n", print_new[bit]);
			}
		}
		dev_info(&lcd->ld->dev, "==================================================\n");

	}

	return ret;
}

#if defined(CONFIG_SMCDSD_DPUI)
static int panel_inc_dpui_u32_field(struct lcd_info *lcd, enum dpui_key key, u32 value)
{
	if (lcd->connected) {
		inc_dpui_u32_field(key, value);
		if (value)
			dev_info(&lcd->ld->dev, "%s: key(%d) invalid\n", __func__, key);
	}

	return 0;
}

static int ea8076_read_rdnumed(struct lcd_info *lcd)
{
	int ret = 0;
	unsigned char buf[LDI_LEN_RDNUMED] = {0, };

	ret = panel_read_bit_info(lcd, LDI_BIT_ENUM_RDNUMED, buf);
	if (ret < 0)
		dev_info(&lcd->ld->dev, "%s: fail\n", __func__);
	else
		panel_inc_dpui_u32_field(lcd, DPUI_KEY_PNDSIE, (buf[0] & LDI_PNDSIE_MASK));

	return ret;
}

static int ea8076_read_rddsdr(struct lcd_info *lcd)
{
	int ret = 0;
	unsigned char buf[LDI_LEN_RDDSDR] = {0, };

	ret = panel_read_bit_info(lcd, LDI_BIT_ENUM_RDDSDR, buf);
	if (ret < 0)
		dev_info(&lcd->ld->dev, "%s: fail\n", __func__);
	else
		panel_inc_dpui_u32_field(lcd, DPUI_KEY_PNSDRE, !(buf[0] & LDI_PNSDRE_MASK));

	return ret;
}
#endif

static int ea8076_read_rddpm(struct lcd_info *lcd)
{
	int ret = 0;

	ret = panel_read_bit_info(lcd, LDI_BIT_ENUM_RDDPM, &lcd->rddpm);
	if (ret < 0)
		dev_info(&lcd->ld->dev, "%s: fail\n", __func__);

	return ret;
}

static int ea8076_read_rddsm(struct lcd_info *lcd)
{
	int ret = 0;

	ret = panel_read_bit_info(lcd, LDI_BIT_ENUM_RDDSM, &lcd->rddsm);
	if (ret < 0)
		dev_info(&lcd->ld->dev, "%s: fail\n", __func__);

	return ret;
}

static int ea8076_read_init_info(struct lcd_info *lcd)
{
	int ret = 0;

	ea8076_read_id(lcd);

	DSI_WRITE(SEQ_TEST_KEY_ON_F0, ARRAY_SIZE(SEQ_TEST_KEY_ON_F0));

	ea8076_read_coordinate(lcd);
	ea8076_read_manufacture_info(lcd);
	ea8076_read_chip_id(lcd);
	ea8076_read_elvss(lcd);

	DSI_WRITE(SEQ_TEST_KEY_OFF_F0, ARRAY_SIZE(SEQ_TEST_KEY_OFF_F0));

	return ret;
}

static int ea8076_exit(struct lcd_info *lcd)
{
	int ret = 0;

	dev_info(&lcd->ld->dev, "%s\n", __func__);

	ea8076_read_rddpm(lcd);
	ea8076_read_rddsm(lcd);

#if defined(CONFIG_SMCDSD_DPUI)
	ea8076_read_rdnumed(lcd);
#endif

	panel_bl_update_average(lcd, 1);

	/* 2. Display Off (28h) */
	DSI_WRITE(SEQ_DISPLAY_OFF, ARRAY_SIZE(SEQ_DISPLAY_OFF));

	/* 3. Wait 20ms */
	msleep(20);

	/* 4. Sleep In (10h) */
	DSI_WRITE(SEQ_SLEEP_IN, ARRAY_SIZE(SEQ_SLEEP_IN));

	/* 5. Wait 120ms */
	msleep(120);

#if defined(CONFIG_SMCDSD_DOZE)
	mutex_lock(&lcd->lock);
	lcd->current_alpm.value = 0;
	mutex_unlock(&lcd->lock);
#endif

	return ret;
}

static int ea8076_displayon(struct lcd_info *lcd)
{
	int ret = 0;

	dev_info(&lcd->ld->dev, "%s\n", __func__);

	/* 12. Display On(29h) */
	DSI_WRITE(SEQ_DISPLAY_ON, ARRAY_SIZE(SEQ_DISPLAY_ON));

	if (lcd->display_on_delay_req) {
		dev_info(&lcd->ld->dev, "%s: sleep 34ms for HLPM\n", __func__);
		msleep(34);
		lcd->display_on_delay_req = 0;
	}

	return ret;
}

#if defined(CONFIG_SMCDSD_DYNAMIC_MIPI)
static int ea8076_change_ffc(struct lcd_info *lcd, unsigned int ffc_on)
{
	int ret = 0;

	if (ffc_on) {
		DSI_WRITE(lcd->ffc_table[lcd->df_index_req], FFC_CMD_CNT);
		lcd->df_index_current = lcd->df_index_req;
		dev_info(&lcd->ld->dev, "%s: [OFF] => [%d]\n", __func__, lcd->df_index_current);
	} else {
		DSI_WRITE(SEQ_FFC_SET_OFF, ARRAY_SIZE(SEQ_FFC_SET_OFF));
		dev_info(&lcd->ld->dev, "%s: [%d] => [OFF]\n", __func__, lcd->df_index_current);
	}

	return ret;
}
#endif

static int ea8076_init(struct lcd_info *lcd)
{
	int ret = 0;

	dev_info(&lcd->ld->dev, "%s\n", __func__);

	/* 6. Sleep Out(11h) */
	DSI_WRITE(SEQ_SLEEP_OUT, ARRAY_SIZE(SEQ_SLEEP_OUT));

	/* 7. Wait 10ms */
	usleep_range(10000, 11000);

#if defined(CONFIG_SEC_FACTORY)
	ea8076_read_init_info(lcd);
#if defined(CONFIG_SMCDSD_MDNIE)
	attr_store_for_each(lcd->mdnie_class, "color_coordinate", lcd->coordinates, strlen(lcd->coordinates));
#endif
#else
	ea8076_read_id(lcd);
#endif

#if defined(CONFIG_SMCDSD_DPUI)
	ea8076_read_rddsdr(lcd);
#endif

	/* Common Setting */
	/* Testkey Enable */
	DSI_WRITE(SEQ_TEST_KEY_ON_F0, ARRAY_SIZE(SEQ_TEST_KEY_ON_F0));
	DSI_WRITE(SEQ_TEST_KEY_ON_FC, ARRAY_SIZE(SEQ_TEST_KEY_ON_FC));

#if defined(CONFIG_SEC_FACTORY)
	dev_info(&lcd->ld->dev, "%s: enable_fd: %d\n", __func__, lcd->enable_fd);
	if (lcd->enable_fd)
		DSI_WRITE(SEQ_ASWIRE_OFF, ARRAY_SIZE(SEQ_ASWIRE_OFF));
	else
		DSI_WRITE(SEQ_ASWIRE_OFF_FD_OFF, ARRAY_SIZE(SEQ_ASWIRE_OFF_FD_OFF));
#endif
	smcdsd_dsi_tx_set(lcd, LCD_SEQ_INIT_1, ARRAY_SIZE(LCD_SEQ_INIT_1));
#if defined(CONFIG_SMCDSD_DYNAMIC_MIPI)
	ea8076_change_ffc(lcd, FFC_UPDATE);
#endif
	smcdsd_dsi_tx_set(lcd, LCD_SEQ_INIT_2, ARRAY_SIZE(LCD_SEQ_INIT_2));

	/* Testkey Disable */
	DSI_WRITE(SEQ_TEST_KEY_OFF_F0, ARRAY_SIZE(SEQ_TEST_KEY_OFF_F0));
	DSI_WRITE(SEQ_TEST_KEY_OFF_FC, ARRAY_SIZE(SEQ_TEST_KEY_OFF_FC));

	/* Brightness Setting */

	lcd->acl_dimming_update_req = 1;
	smcdsd_panel_set_brightness(lcd, 1);

	/* TE(Vsync) ON/OFF */
	DSI_WRITE(SEQ_TEST_KEY_ON_F0, ARRAY_SIZE(SEQ_TEST_KEY_ON_F0));
	DSI_WRITE(SEQ_TE_ON, ARRAY_SIZE(SEQ_TE_ON));
	DSI_WRITE(SEQ_TEST_KEY_OFF_F0, ARRAY_SIZE(SEQ_TEST_KEY_OFF_F0));

	/* 10. Wait 110ms */
	msleep(110);

	panel_bl_init_average(lcd, 1);

	return ret;
}

#if defined(CONFIG_SMCDSD_DPUI)
static int panel_dpui_notifier_callback(struct notifier_block *self,
				unsigned long event, void *data)
{
	struct lcd_info *lcd = NULL;
	struct dpui_info *dpui = data;
	char tbuf[MAX_DPUI_VAL_LEN];
	int size;
	unsigned int site, rework, poc, i, invalid = 0;
	unsigned char *m_info;

	struct seq_file m = {
		.buf = tbuf,
		.size = sizeof(tbuf) - 1,
	};

	if (dpui == NULL) {
		pr_err("%s: dpui is null\n", __func__);
		return NOTIFY_DONE;
	}

	lcd = container_of(self, struct lcd_info, dpui_notif);

	size = snprintf(tbuf, MAX_DPUI_VAL_LEN, "%04d%02d%02d %02d%02d%02d",
			((lcd->date[0] & 0xF0) >> 4) + 2011, lcd->date[0] & 0xF, lcd->date[1] & 0x1F,
			lcd->date[2] & 0x1F, lcd->date[3] & 0x3F, lcd->date[4] & 0x3F);
	set_dpui_field(DPUI_KEY_MAID_DATE, tbuf, size);

	size = snprintf(tbuf, MAX_DPUI_VAL_LEN, "%d", lcd->id_info.id[0]);
	set_dpui_field(DPUI_KEY_LCDID1, tbuf, size);
	size = snprintf(tbuf, MAX_DPUI_VAL_LEN, "%d", lcd->id_info.id[1]);
	set_dpui_field(DPUI_KEY_LCDID2, tbuf, size);
	size = snprintf(tbuf, MAX_DPUI_VAL_LEN, "%d", lcd->id_info.id[2]);
	set_dpui_field(DPUI_KEY_LCDID3, tbuf, size);
	size = snprintf(tbuf, MAX_DPUI_VAL_LEN, "%s_%s", DPUI_VENDOR_NAME, DPUI_MODEL_NAME);
	set_dpui_field(DPUI_KEY_DISP_MODEL, tbuf, size);

	size = snprintf(tbuf, MAX_DPUI_VAL_LEN, "0x%02X%02X%02X%02X%02X%02X",
		lcd->code[0], lcd->code[1], lcd->code[2], lcd->code[3], lcd->code[4], lcd->code[5]);
	set_dpui_field(DPUI_KEY_CHIPID, tbuf, size);

	size = snprintf(tbuf, MAX_DPUI_VAL_LEN, "%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X",
		lcd->date[0], lcd->date[1], lcd->date[2], lcd->date[3], lcd->date[4],
		lcd->date[5], lcd->date[6], (lcd->coordinate[0] & 0xFF00) >> 8, lcd->coordinate[0] & 0x00FF,
		(lcd->coordinate[1] & 0xFF00) >> 8, lcd->coordinate[1] & 0x00FF);
	set_dpui_field(DPUI_KEY_CELLID, tbuf, size);

	m_info = lcd->manufacture_info;
	site = get_bit(m_info[0], 4, 4);
	rework = get_bit(m_info[0], 0, 4);
	poc = get_bit(m_info[1], 0, 4);
	seq_printf(&m, "%d%d%d%02x%02x", site, rework, poc, m_info[2], m_info[3]);

	for (i = 4; i < LDI_LEN_MANUFACTURE_INFO + LDI_LEN_MANUFACTURE_INFO_CELL_ID; i++) {
		if (!isdigit(m_info[i]) && !isupper(m_info[i])) {
			invalid = 1;
			break;
		}
	}
	for (i = 4; !invalid && i < LDI_LEN_MANUFACTURE_INFO + LDI_LEN_MANUFACTURE_INFO_CELL_ID; i++)
		seq_printf(&m, "%c", m_info[i]);

	set_dpui_field(DPUI_KEY_OCTAID, tbuf, m.count);

	inc_dpui_u32_field(DPUI_KEY_UB_CON, lcd->conn_det_count);
	lcd->conn_det_count = 0;

	return NOTIFY_DONE;
}
#endif /* CONFIG_SMCDSD_DPUI */

static int fb_notifier_callback(struct notifier_block *self,
			unsigned long event, void *data)
{
	struct fb_event *evdata = data;
	struct lcd_info *lcd = NULL;
	int fb_blank;

	switch (event) {
	case FB_EVENT_BLANK:
	case FB_EARLY_EVENT_BLANK:
	case SMCDSD_EVENT_DOZE:
	case SMCDSD_EARLY_EVENT_DOZE:
		break;
	default:
		return NOTIFY_DONE;
	}

	lcd = container_of(self, struct lcd_info, fb_notifier);

	fb_blank = *(int *)evdata->data;

	dev_info(&lcd->ld->dev, "%s: %02lx, %d\n", __func__, event, fb_blank);

	if (evdata->info->node)
		return NOTIFY_DONE;

	if (IS_EARLY(event) && fb_blank == FB_BLANK_POWERDOWN) {
		mutex_lock(&lcd->lock);
		lcd->enable = 0;
		mutex_unlock(&lcd->lock);
	} else if (IS_AFTER(event) && fb_blank == FB_BLANK_UNBLANK) {
		mutex_lock(&lcd->lock);
		if (lcd->shutdown) {
			dev_info(&lcd->ld->dev,
				"%s: Now system going to shutdown, enable request will be ignored\n", __func__);
		} else {
			lcd->enable = 1;
		}
#if defined(CONFIG_SMCDSD_DYNAMIC_MIPI)
		/* defence code for req come during resume */
		if (lcd->df_index_req != lcd->df_index_current) {
			DSI_WRITE(SEQ_TEST_KEY_ON_F0, ARRAY_SIZE(SEQ_TEST_KEY_ON_F0));
			DSI_WRITE(SEQ_TEST_KEY_ON_FC, ARRAY_SIZE(SEQ_TEST_KEY_ON_FC));
			ea8076_change_ffc(lcd, FFC_UPDATE);
			DSI_WRITE(SEQ_TEST_KEY_OFF_F0, ARRAY_SIZE(SEQ_TEST_KEY_OFF_F0));
			DSI_WRITE(SEQ_TEST_KEY_OFF_FC, ARRAY_SIZE(SEQ_TEST_KEY_OFF_FC));
		}
#endif
		ea8076_displayon(lcd);
		mutex_unlock(&lcd->lock);
		smcdsd_panel_set_brightness(lcd, 1);
	}

	if (fb_blank == FB_BLANK_UNBLANK && event == FB_EVENT_BLANK)
		panel_bl_init_average(lcd, 1);

	return NOTIFY_DONE;
}

#if defined(CONFIG_SMCDSD_DYNAMIC_MIPI)
static int search_dynamic_freq_idx(struct lcd_info *lcd,
				struct df_freq_tbl_info *df_freq_tbl, int band_idx, int freq)
{
	int i, ret = 0;
	int min, max, array_idx;
	struct dynamic_freq_range *array;
	struct df_freq_tbl_info *df_tbl;

	if (band_idx >= FREQ_RANGE_MAX) {
		dev_err(&lcd->ld->dev, "%s: exceed max band idx : %d\n", __func__, band_idx);
		ret = -EINVAL;
		goto search_exit;
	}

	df_tbl = &df_freq_tbl[band_idx];

	if (df_tbl == NULL) {
		dev_err(&lcd->ld->dev, "%s:failed to find band_idx : %d\n", __func__, band_idx);
		ret = -EPERM;
		goto search_exit;
	}
	array_idx = df_tbl->size;

	if (array_idx == 1) {
		array = &df_tbl->array[0];
		dev_info(&lcd->ld->dev, "%s:Found adap_freq idx(0) : %d\n",
			__func__, array->freq_idx);
		ret = array->freq_idx;
	} else {
		for (i = 0; i < array_idx; i++) {
			array = &df_tbl->array[i];
			dev_info(&lcd->ld->dev, "%s: min: %d, max: %d\n", __func__, array->min, array->max);

			min = (int)freq - array->min;
			max = (int)freq - array->max;

			if ((min >= 0) && (max <= 0)) {
				dev_info(&lcd->ld->dev, "%s:Found adap_freq idx : %d\n",
					__func__, array->freq_idx);
				ret = array->freq_idx;
				break;
			}
		}

		if (i >= array_idx) {
			dev_err(&lcd->ld->dev, "%s:Can't found freq idx (%d)\n", __func__, i);
			ret = -EINVAL;
			goto search_exit;
		}
	}
search_exit:
	return ret;
}

static int panel_dynamic_freq_notifier_callback(struct notifier_block *self,
				unsigned long size, void *buf)
{
	struct lcd_info *lcd = NULL;
	struct dev_ril_bridge_msg *msg;
	struct ril_noti_info *ch_info;
	int df_idx;

	lcd = container_of(self, struct lcd_info, df_notif);
	msg = (struct dev_ril_bridge_msg *)buf;

	if (msg == NULL) {
		dev_err(&lcd->ld->dev, "%s: msg null.\n", __func__);
		return NOTIFY_DONE;
	}

	if (msg->dev_id == IPC_SYSTEM_CP_CHANNEL_INFO &&
		msg->data_len == sizeof(struct ril_noti_info)) {

		ch_info = (struct ril_noti_info *)msg->data;
		if (ch_info == NULL) {
			dev_err(&lcd->ld->dev, "%s: ch_info null.\n", __func__);
			return NOTIFY_DONE;
		}

		df_idx = search_dynamic_freq_idx(lcd, a31_dynamic_freq_set,
			ch_info->band, ch_info->channel);

		dev_info(&lcd->ld->dev, "%s: (b:%d, c:%d) df:%d\n",
			__func__, ch_info->band, ch_info->channel, df_idx);

		if (df_idx < 0)
			return NOTIFY_DONE;

		if (lcd->df_index_current == df_idx)
			return NOTIFY_DONE;

		mutex_lock(&lcd->lock);
		if (lcd->enable) {
			DSI_WRITE(SEQ_TEST_KEY_ON_F0, ARRAY_SIZE(SEQ_TEST_KEY_ON_F0));
			DSI_WRITE(SEQ_TEST_KEY_ON_FC, ARRAY_SIZE(SEQ_TEST_KEY_ON_FC));
			ea8076_change_ffc(lcd, FFC_OFF);
			DSI_WRITE(SEQ_TEST_KEY_OFF_F0, ARRAY_SIZE(SEQ_TEST_KEY_OFF_F0));
			DSI_WRITE(SEQ_TEST_KEY_OFF_FC, ARRAY_SIZE(SEQ_TEST_KEY_OFF_FC));
		}
		mutex_unlock(&lcd->lock);

		smcdsd_panel_dsi_clk_change(df_idx);

		mutex_lock(&lcd->lock);
		lcd->df_index_req = df_idx;
		mutex_unlock(&lcd->lock);

	}

	return NOTIFY_DONE;
}
#endif

static int panel_reboot_notify(struct notifier_block *self,
			unsigned long event, void *unused)
{
	struct lcd_info *lcd = container_of(self, struct lcd_info, reboot_notifier);

	dev_info(&lcd->ld->dev, "%s\n", __func__);

	mutex_lock(&lcd->lock);
	lcd->enable = 0;
	lcd->shutdown = 1;
	mutex_unlock(&lcd->lock);

	return NOTIFY_DONE;
}

static int ea8076_register_notifier(struct lcd_info *lcd)
{
	lcd->fb_notifier.notifier_call = fb_notifier_callback;
	smcdsd_register_notifier(&lcd->fb_notifier);

	lcd->reboot_notifier.notifier_call = panel_reboot_notify;
	register_reboot_notifier(&lcd->reboot_notifier);

#if defined(CONFIG_SMCDSD_DPUI)
	lcd->dpui_notif.notifier_call = panel_dpui_notifier_callback;
	if (lcd->connected)
		dpui_logging_register(&lcd->dpui_notif, DPUI_TYPE_PANEL);
#endif

#if defined(CONFIG_SMCDSD_DYNAMIC_MIPI)
	lcd->df_notif.notifier_call = panel_dynamic_freq_notifier_callback;
	if (lcd->connected)
		register_dev_ril_bridge_event_notifier(&lcd->df_notif);
#endif

	dev_info(&lcd->ld->dev, "%s\n", __func__);

	return 0;
}

static int ea8076_probe(struct lcd_info *lcd)
{
	int ret = 0;

	dev_info(&lcd->ld->dev, "+ %s\n", __func__);

	lcd->bd->props.max_brightness = EXTEND_BRIGHTNESS;
	lcd->bd->props.brightness = UI_DEFAULT_BRIGHTNESS;
	lcd->mask_brightness = UI_MAX_BRIGHTNESS;

	lcd->state = PANEL_STATE_RESUMED;
	lcd->enable = 1;

	lcd->temperature = NORMAL_TEMPERATURE;
	lcd->adaptive_control = !!ACL_STATUS_15P;
	lcd->lux = -1;

	lcd->acl_table = ACL_TABLE;
	lcd->acl_dim_table = ACL_DIM_TABLE;
#if defined(CONFIG_SMCDSD_DYNAMIC_MIPI)
	lcd->ffc_table = FFC_TABLE;
#endif
	lcd->hbm_table = HBM_TABLE;
	lcd->trans_dimming = TRANS_DIMMING_ON;

	lcd->acl_dimming_update_req = 0;
	lcd->display_on_delay_req = 0;
	lcd->acl_dimming = ACL_DIMMING_ON;
	lcd->mask_framedone_check_req = 0;

#if defined(CONFIG_SEC_FACTORY)
	lcd->enable_fd = 1;
#endif

	ret = ea8076_read_init_info(lcd);
	if (ret < 0)
		dev_info(&lcd->ld->dev, "%s: failed to init information\n", __func__);

	smcdsd_panel_set_brightness(lcd, 1);

	dev_info(&lcd->ld->dev, "- %s\n", __func__);

	return 0;
}

static ssize_t lcd_type_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct lcd_info *lcd = dev_get_drvdata(dev);

	sprintf(buf, "SDC_%02X%02X%02X\n", lcd->id_info.id[0], lcd->id_info.id[1], lcd->id_info.id[2]);

	return strlen(buf);
}

static ssize_t window_type_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct lcd_info *lcd = dev_get_drvdata(dev);

	sprintf(buf, "%02x %02x %02x\n", lcd->id_info.id[0], lcd->id_info.id[1], lcd->id_info.id[2]);

	return strlen(buf);
}

static ssize_t brightness_table_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	int i;
	char *pos = buf;

	for (i = 0; i <= EXTEND_BRIGHTNESS; i++)
		pos += sprintf(pos, "%3d %4d\n", i, brightness_table[i]);

	return pos - buf;
}

static ssize_t temperature_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	char temp[] = "-15, -14, 0, 1\n";

	strcat(buf, temp);
	return strlen(buf);
}

static ssize_t temperature_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t size)
{
	struct lcd_info *lcd = dev_get_drvdata(dev);
	int value, rc, temperature_index = 0;

	rc = kstrtoint(buf, 0, &value);
	if (rc < 0)
		return rc;

	switch (value) {
	case 1:
		temperature_index = TEMP_ABOVE_MINUS_00_DEGREE;
		value = NORMAL_TEMPERATURE;
		break;
	case 0:
	case -14:
		temperature_index = TEMP_ABOVE_MINUS_15_DEGREE;
		break;
	case -15:
		temperature_index = TEMP_BELOW_MINUS_15_DEGREE;
		break;
	default:
		dev_info(&lcd->ld->dev, "%s: %d is invalid\n", __func__, value);
		return -EINVAL;
	}

	mutex_lock(&lcd->lock);
	lcd->temperature = value;
	lcd->temperature_index = temperature_index;
	mutex_unlock(&lcd->lock);

	if (lcd->state == PANEL_STATE_RESUMED)
		smcdsd_panel_set_brightness(lcd, 0);

	dev_info(&lcd->ld->dev, "%s: %d, %d, %d\n", __func__, value, lcd->temperature, lcd->temperature_index);

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
	struct lcd_info *lcd = dev_get_drvdata(dev);

	sprintf(buf, "%02X%02X%02X%02X%02X%02X\n",
		lcd->code[0], lcd->code[1], lcd->code[2], lcd->code[3], lcd->code[4], lcd->code[5]);

	return strlen(buf);
}

static ssize_t cell_id_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct lcd_info *lcd = dev_get_drvdata(dev);

	sprintf(buf, "%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X\n",
		lcd->date[0], lcd->date[1], lcd->date[2], lcd->date[3], lcd->date[4],
		lcd->date[5], lcd->date[6], (lcd->coordinate[0] & 0xFF00) >> 8, lcd->coordinate[0] & 0x00FF,
		(lcd->coordinate[1] & 0xFF00) >> 8, lcd->coordinate[1] & 0x00FF);

	return strlen(buf);
}

static ssize_t adaptive_control_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct lcd_info *lcd = dev_get_drvdata(dev);

	sprintf(buf, "%d\n", lcd->adaptive_control);

	return strlen(buf);
}

static ssize_t adaptive_control_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t size)
{
	struct lcd_info *lcd = dev_get_drvdata(dev);
	int rc;
	unsigned int value;

	rc = kstrtouint(buf, 0, &value);
	if (rc < 0)
		return rc;

	if (lcd->adaptive_control != value) {
		dev_info(&lcd->ld->dev, "%s: %d, %d\n", __func__, lcd->adaptive_control, value);
		mutex_lock(&lcd->lock);
		lcd->adaptive_control = value;
		mutex_unlock(&lcd->lock);
		if (lcd->state == PANEL_STATE_RESUMED)
			smcdsd_panel_set_brightness(lcd, 0);
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

#if defined(CONFIG_SMCDSD_MDNIE)
		attr_store_for_each(lcd->mdnie_class, attr->attr.name, buf, size);
#endif
	}

	return size;
}

static ssize_t octa_id_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct lcd_info *lcd = dev_get_drvdata(dev);
	unsigned int site, rework, poc, i, invalid = 0;
	unsigned char *m_info;

	struct seq_file m = {
		.buf = buf,
		.size = PAGE_SIZE - 1,
	};

	m_info = lcd->manufacture_info;
	site = get_bit(m_info[0], 4, 4);
	rework = get_bit(m_info[0], 0, 4);
	poc = get_bit(m_info[1], 0, 4);
	seq_printf(&m, "%d%d%d%02x%02x", site, rework, poc, m_info[2], m_info[3]);

	for (i = 4; i < LDI_LEN_MANUFACTURE_INFO + LDI_LEN_MANUFACTURE_INFO_CELL_ID; i++) {
		if (!isdigit(m_info[i]) && !isupper(m_info[i])) {
			invalid = 1;
			break;
		}
	}
	for (i = 4; !invalid && i < LDI_LEN_MANUFACTURE_INFO + LDI_LEN_MANUFACTURE_INFO_CELL_ID; i++)
		seq_printf(&m, "%c", m_info[i]);

	seq_puts(&m, "\n");

	return strlen(buf);
}

static ssize_t xtalk_mode_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t size)
{
	struct lcd_info *lcd = dev_get_drvdata(dev);
	unsigned int value;
	int ret;

	ret = kstrtouint(buf, 0, &value);
	if (ret < 0)
		return ret;

	dev_info(&lcd->ld->dev, "%s: %d\n", __func__, value);

	if (lcd->state != PANEL_STATE_RESUMED || !lcd->enable) {
		dev_info(&lcd->ld->dev, "%s: panel state is %d, %d\n", __func__, lcd->state, lcd->enable);
		return -EINVAL;
	}

	mutex_lock(&lcd->lock);
	if (value == 1) {
		DSI_WRITE(SEQ_TEST_KEY_ON_F0, ARRAY_SIZE(SEQ_TEST_KEY_ON_F0));
		DSI_WRITE(SEQ_XTALK_B0, ARRAY_SIZE(SEQ_XTALK_B0));
		DSI_WRITE(SEQ_XTALK_ON, ARRAY_SIZE(SEQ_XTALK_ON));
		DSI_WRITE(SEQ_TEST_KEY_OFF_F0, ARRAY_SIZE(SEQ_TEST_KEY_OFF_F0));
	} else {
		DSI_WRITE(SEQ_TEST_KEY_ON_F0, ARRAY_SIZE(SEQ_TEST_KEY_ON_F0));
		DSI_WRITE(SEQ_XTALK_B0, ARRAY_SIZE(SEQ_XTALK_B0));
		DSI_WRITE(SEQ_XTALK_OFF, ARRAY_SIZE(SEQ_XTALK_OFF));
		DSI_WRITE(SEQ_TEST_KEY_OFF_F0, ARRAY_SIZE(SEQ_TEST_KEY_OFF_F0));
	}
	mutex_unlock(&lcd->lock);

	return size;
}

#if defined(CONFIG_SEC_FACTORY)
static ssize_t enable_fd_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct lcd_info *lcd = dev_get_drvdata(dev);

	sprintf(buf, "%d\n", lcd->enable_fd);

	return strlen(buf);
}

static ssize_t enable_fd_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t size)
{
	struct lcd_info *lcd = dev_get_drvdata(dev);
	unsigned int value;
	int ret;

	ret = kstrtouint(buf, 0, &value);
	if (ret < 0)
		return ret;

	dev_info(&lcd->ld->dev, "%s: %d\n", __func__, lcd->enable_fd, value);

	if (lcd->state != PANEL_STATE_RESUMED) {
		dev_info(&lcd->ld->dev, "%s: panel state is %d\n", __func__, lcd->state);
		return -EINVAL;
	}

	mutex_lock(&lcd->lock);
	lcd->enable_fd = value;
	mutex_unlock(&lcd->lock);

	return size;
}
#endif

#if defined(CONFIG_SMCDSD_DPUI)
/*
 * HW PARAM LOGGING SYSFS NODE
 */
static ssize_t dpui_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	int ret;

	update_dpui_log(DPUI_LOG_LEVEL_INFO, DPUI_TYPE_PANEL);
	ret = get_dpui_log(buf, DPUI_LOG_LEVEL_INFO, DPUI_TYPE_PANEL);
	if (ret < 0) {
		pr_err("%s failed to get log %d\n", __func__, ret);
		return ret;
	}

	pr_info("%s\n", buf);
	return ret;
}

static ssize_t dpui_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t size)
{
	if (buf[0] == 'C' || buf[0] == 'c')
		clear_dpui_log(DPUI_LOG_LEVEL_INFO, DPUI_TYPE_PANEL);

	return size;
}

/*
 * [DEV ONLY]
 * HW PARAM LOGGING SYSFS NODE
 */
static ssize_t dpui_dbg_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	int ret;

	update_dpui_log(DPUI_LOG_LEVEL_DEBUG, DPUI_TYPE_PANEL);
	ret = get_dpui_log(buf, DPUI_LOG_LEVEL_DEBUG, DPUI_TYPE_PANEL);
	if (ret < 0) {
		pr_err("%s failed to get log %d\n", __func__, ret);
		return ret;
	}

	pr_info("%s\n", buf);
	return ret;
}

static ssize_t dpui_dbg_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t size)
{
	if (buf[0] == 'C' || buf[0] == 'c')
		clear_dpui_log(DPUI_LOG_LEVEL_DEBUG, DPUI_TYPE_PANEL);

	return size;
}

static DEVICE_ATTR(dpui, 0660, dpui_show, dpui_store);
static DEVICE_ATTR(dpui_dbg, 0660, dpui_dbg_show, dpui_dbg_store);
#endif

#if defined(CONFIG_SMCDSD_DOZE)
#if defined(CONFIG_SEC_FACTORY)
static ssize_t alpm_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t size)
{
	struct lcd_info *lcd = dev_get_drvdata(dev);
	struct mipi_dsi_lcd_parent *pdata = lcd->pdata;
	struct fb_info *fbinfo = registered_fb[0];
	struct abd_protect *abd = &pdata->abd;
	union lpm_info lpm = {0, };
	unsigned int value = 0;
	int ret;

	ret = kstrtouint(buf, 0, &value);
	if (ret < 0)
		return ret;

	dev_info(&lcd->ld->dev, "%s: %06x, lpm: %06x(%06x)\n", __func__,
		value, lcd->current_alpm.value, lcd->alpm.value);

	if (lcd->state != PANEL_STATE_RESUMED || !lcd->enable) {
		dev_info(&lcd->ld->dev, "%s: panel state is %d, %d\n", __func__, lcd->state, lcd->enable);
		return -EINVAL;
	}

	if (!lock_fb_info(fbinfo)) {
		dev_info(&lcd->ld->dev, "%s: fblock is failed\n", __func__);
		return -EINVAL;
	}

	lpm.ver = get_bit(value, 16, 8);
	lpm.mode = get_bit(value, 0, 8);

	if (!lpm.ver && lpm.mode >= ALPM_MODE_MAX) {
		dev_info(&lcd->ld->dev, "%s: undefined lpm value: %x\n", __func__, value);
		unlock_fb_info(fbinfo);
		return -EINVAL;
	}

	if (lpm.ver && lpm.mode >= AOD_MODE_MAX) {
		dev_info(&lcd->ld->dev, "%s: undefined lpm value: %x\n", __func__, value);
		unlock_fb_info(fbinfo);
		return -EINVAL;
	}

	lpm.state = (lpm.ver && lpm.mode) ? lpm_brightness_table[lcd->bd->props.brightness] : lpm_old_table[lpm.mode];

	mutex_lock(&lcd->lock);
	lcd->prev_alpm = lcd->alpm;
	lcd->alpm = lpm;
	mutex_unlock(&lcd->lock);

	smcdsd_abd_enable(abd, 0);
	switch (lpm.mode) {
	case ALPM_OFF:
		if (lcd->prev_brightness) {
			mutex_lock(&lcd->lock);
			lcd->bd->props.brightness = lcd->prev_brightness;
			lcd->prev_brightness = 0;
			mutex_unlock(&lcd->lock);
		}
		call_drv_ops(doze_exit);
		usleep_range(17000, 18000);
		mutex_lock(&lcd->lock);
		ea8076_displayon(lcd);
		mutex_unlock(&lcd->lock);
		break;
	case ALPM_ON_LOW:
	case HLPM_ON_LOW:
	case ALPM_ON_HIGH:
	case HLPM_ON_HIGH:
		if (lcd->prev_alpm.mode == ALPM_OFF) {
			mutex_lock(&lcd->lock);
			lcd->prev_brightness = lcd->bd->props.brightness;
			mutex_unlock(&lcd->lock);
		}
		lcd->bd->props.brightness = (value <= HLPM_ON_LOW) ? 0 : UI_MAX_BRIGHTNESS;
		call_drv_ops(doze_init);
		usleep_range(17000, 18000);
		mutex_lock(&lcd->lock);
		ea8076_displayon(lcd);
		mutex_unlock(&lcd->lock);
		break;
	}
	smcdsd_abd_enable(abd, 1);

	unlock_fb_info(fbinfo);

	return size;
}
#else
static ssize_t alpm_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t size)
{
	struct lcd_info *lcd = dev_get_drvdata(dev);
	union lpm_info lpm = {0, };
	unsigned int value = 0;
	int ret;

	ret = kstrtouint(buf, 0, &value);
	if (ret < 0)
		return ret;

	dev_info(&lcd->ld->dev, "%s: %06x, lpm: %06x(%06x)\n", __func__,
		value, lcd->current_alpm.value, lcd->alpm.value);

	lpm.ver = get_bit(value, 16, 8);
	lpm.mode = get_bit(value, 0, 8);

	if (!lpm.ver && lpm.mode >= ALPM_MODE_MAX) {
		dev_info(&lcd->ld->dev, "%s: undefined lpm value: %x\n", __func__, value);
		return -EINVAL;
	}

	if (lpm.ver && lpm.mode >= AOD_MODE_MAX) {
		dev_info(&lcd->ld->dev, "%s: undefined lpm value: %x\n", __func__, value);
		return -EINVAL;
	}

	lpm.state = (lpm.ver && lpm.mode) ? lpm_brightness_table[lcd->bd->props.brightness] : lpm_old_table[lpm.mode];

	if (lcd->alpm.value == lpm.value && lcd->current_alpm.value == lpm.value) {
		dev_info(&lcd->ld->dev, "%s: unchanged lpm value: %x\n", __func__, lpm.value);
		return size;
	}

	mutex_lock(&lcd->lock);
	lcd->alpm = lpm;
	mutex_unlock(&lcd->lock);

	if (lcd->current_alpm.state && lcd->enable)
		call_drv_ops(doze_init);

	return size;
}
#endif

static ssize_t alpm_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct lcd_info *lcd = dev_get_drvdata(dev);

	sprintf(buf, "%06x\n", lcd->current_alpm.value);

	return strlen(buf);
}

static DEVICE_ATTR(alpm, 0664, alpm_show, alpm_store);
#endif

#if defined(CONFIG_SMCDSD_DYNAMIC_MIPI)
static ssize_t dynamic_mipi_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t size)
{
	struct lcd_info *lcd = dev_get_drvdata(dev);
	unsigned int value = 0;
	int ret;
	int df_idx;

	ret = kstrtouint(buf, 0, &value);
	if (ret < 0)
		return ret;

	if (lcd->enable) {
		DSI_WRITE(SEQ_TEST_KEY_ON_F0, ARRAY_SIZE(SEQ_TEST_KEY_ON_F0));
		DSI_WRITE(SEQ_TEST_KEY_ON_FC, ARRAY_SIZE(SEQ_TEST_KEY_ON_FC));
		ea8076_change_ffc(lcd, FFC_OFF);
		DSI_WRITE(SEQ_TEST_KEY_OFF_F0, ARRAY_SIZE(SEQ_TEST_KEY_OFF_F0));
		DSI_WRITE(SEQ_TEST_KEY_OFF_FC, ARRAY_SIZE(SEQ_TEST_KEY_OFF_FC));
	}

	df_idx = value;

	if (df_idx < 0 || df_idx > 3)
		df_idx = 0;

	smcdsd_panel_dsi_clk_change(df_idx);

	mutex_lock(&lcd->lock);
	lcd->df_index_req = df_idx;
	mutex_unlock(&lcd->lock);

	dev_info(&lcd->ld->dev, "%s: (%04d)\n", __func__, value);

	return size;
}

static ssize_t dynamic_mipi_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct lcd_info *lcd = dev_get_drvdata(dev);

	sprintf(buf, "%u\n", lcd->df_index_req);

	return strlen(buf);
}

static ssize_t virtual_dynamic_mipi_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t size)
{
	struct lcd_info *lcd = dev_get_drvdata(dev);
	int ret;

	u8 rat;
	u32 band;
	u32 channel;

	struct dev_ril_bridge_msg dummy_ril_bridge_msg;
	struct ril_noti_info  dummy_ril_noti_info;

	memset(&dummy_ril_bridge_msg, 0x00, sizeof(dummy_ril_bridge_msg));
	memset(&dummy_ril_noti_info, 0x00, sizeof(dummy_ril_noti_info));

	ret = sscanf(buf, "%d %d %d\n", &rat, &band, &channel);
	if (ret < 0)
		return ret;

	dummy_ril_noti_info.rat = 0;
	dummy_ril_noti_info.band = band;
	dummy_ril_noti_info.channel = channel;

	dummy_ril_bridge_msg.data = &dummy_ril_noti_info;
	dummy_ril_bridge_msg.dev_id = IPC_SYSTEM_CP_CHANNEL_INFO;
	dummy_ril_bridge_msg.data_len = sizeof(struct ril_noti_info);

	panel_dynamic_freq_notifier_callback(&lcd->df_notif, sizeof(dummy_ril_bridge_msg), &dummy_ril_bridge_msg);

	return size;
}

static ssize_t virtual_dynamic_mipi_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct lcd_info *lcd = dev_get_drvdata(dev);

	sprintf(buf, "%u\n", lcd->df_index_req);

	return strlen(buf);
}

static DEVICE_ATTR(dynamic_mipi, 0664, dynamic_mipi_show, dynamic_mipi_store);
static DEVICE_ATTR(virtual_dynamic_mipi, 0664, virtual_dynamic_mipi_show, virtual_dynamic_mipi_store);
#endif

static ssize_t mask_brightness_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct lcd_info *lcd = dev_get_drvdata(dev);

	sprintf(buf, "%d\n", lcd->mask_brightness);

	return strlen(buf);
}

static ssize_t mask_brightness_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t size)
{
	struct lcd_info *lcd = dev_get_drvdata(dev);
	unsigned int value;
	int rc;

	rc = kstrtouint(buf, 0, &value);
	if (rc < 0)
		return rc;

	dev_info(&lcd->ld->dev, "%s: %d, %d\n", __func__, lcd->mask_brightness, value);

	if (value > lcd->bd->props.max_brightness)
		return -EINVAL;

	mutex_lock(&lcd->lock);
	lcd->mask_brightness = value;
	mutex_unlock(&lcd->lock);

	return size;
}

static DEVICE_ATTR(mask_brightness, 0644, mask_brightness_show, mask_brightness_store);

static ssize_t actual_mask_brightness_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct lcd_info *lcd = dev_get_drvdata(dev);

	sprintf(buf, "%d\n", lcd->actual_mask_brightness);

	return strlen(buf);
}

static DEVICE_ATTR(actual_mask_brightness, 0444, actual_mask_brightness_show, NULL);

static DEVICE_ATTR(lcd_type, 0444, lcd_type_show, NULL);
static DEVICE_ATTR(window_type, 0444, window_type_show, NULL);
static DEVICE_ATTR(manufacture_code, 0444, manufacture_code_show, NULL);
static DEVICE_ATTR(cell_id, 0444, cell_id_show, NULL);
static DEVICE_ATTR(brightness_table, 0444, brightness_table_show, NULL);
static DEVICE_ATTR(temperature, 0664, temperature_show, temperature_store);
static DEVICE_ATTR(color_coordinate, 0444, color_coordinate_show, NULL);
static DEVICE_ATTR(manufacture_date, 0444, manufacture_date_show, NULL);
static DEVICE_ATTR(adaptive_control, 0664, adaptive_control_show, adaptive_control_store);
static DEVICE_ATTR(lux, 0644, lux_show, lux_store);
static DEVICE_ATTR(octa_id, 0444, octa_id_show, NULL);
static DEVICE_ATTR(SVC_OCTA, 0444, cell_id_show, NULL);
static DEVICE_ATTR(SVC_OCTA_CHIPID, 0444, octa_id_show, NULL);
static DEVICE_ATTR(SVC_OCTA_DDI_CHIPID, 0444, manufacture_code_show, NULL);
static DEVICE_ATTR(xtalk_mode, 0220, NULL, xtalk_mode_store);
#if defined(CONFIG_SEC_FACTORY)
static DEVICE_ATTR(enable_fd, 0644, enable_fd_show, enable_fd_store);
#endif
static DEVICE_ATTR(brt_avg, 0444, brt_avg_show, NULL);

static struct attribute *lcd_sysfs_attributes[] = {
	&dev_attr_lcd_type.attr,
	&dev_attr_window_type.attr,
	&dev_attr_manufacture_code.attr,
	&dev_attr_cell_id.attr,
	&dev_attr_temperature.attr,
	&dev_attr_color_coordinate.attr,
	&dev_attr_manufacture_date.attr,
	&dev_attr_brightness_table.attr,
	&dev_attr_adaptive_control.attr,
	&dev_attr_lux.attr,
	&dev_attr_octa_id.attr,
#if defined(CONFIG_SMCDSD_DOZE)
	&dev_attr_alpm.attr,
#endif
#if defined(CONFIG_SMCDSD_DYNAMIC_MIPI)
	&dev_attr_dynamic_mipi.attr,
	&dev_attr_virtual_dynamic_mipi.attr,
#endif
	&dev_attr_xtalk_mode.attr,
#if defined(CONFIG_SEC_FACTORY)
	&dev_attr_enable_fd.attr,
#endif
#if defined(CONFIG_SMCDSD_DPUI)
	&dev_attr_dpui.attr,
	&dev_attr_dpui_dbg.attr,
#endif
	&dev_attr_mask_brightness.attr,
	&dev_attr_actual_mask_brightness.attr,
	&dev_attr_brt_avg.attr,
	NULL,
};

static const struct attribute_group lcd_sysfs_attr_group = {
	.attrs = lcd_sysfs_attributes,
};

static void lcd_init_svc(struct lcd_info *lcd)
{
	struct device *dev = &lcd->svc_dev;
	struct kobject *top_kobj = &lcd->ld->dev.kobj.kset->kobj;
	struct kernfs_node *kn = kernfs_find_and_get(top_kobj->sd, "svc");
	struct kobject *svc_kobj = NULL;
	char *buf = NULL;
	int ret = 0;

	svc_kobj = kn ? kn->priv : kobject_create_and_add("svc", top_kobj);
	if (!svc_kobj)
		return;

	buf = kzalloc(PATH_MAX, GFP_KERNEL);
	if (buf) {
		kernfs_path(svc_kobj->sd, buf, PATH_MAX);
		dev_info(&lcd->ld->dev, "%s: %s %s\n", __func__, buf, !kn ? "create" : "");
		kfree(buf);
	}

	dev->kobj.parent = svc_kobj;
	dev_set_name(dev, "OCTA");
	dev_set_drvdata(dev, lcd);
	ret = device_register(dev);
	if (ret < 0) {
		dev_info(&lcd->ld->dev, "%s: device_register fail\n", __func__);
		return;
	}

	device_create_file(dev, &dev_attr_SVC_OCTA);
	device_create_file(dev, &dev_attr_SVC_OCTA_CHIPID);
	device_create_file(dev, &dev_attr_SVC_OCTA_DDI_CHIPID);

	if (kn)
		kernfs_put(kn);
}

static void lcd_init_sysfs(struct lcd_info *lcd)
{
	int ret = 0;

	ret = sysfs_create_group(&lcd->ld->dev.kobj, &lcd_sysfs_attr_group);
	if (ret < 0)
		dev_info(&lcd->ld->dev, "failed to add lcd sysfs\n");

	lcd_init_svc(lcd);

	init_debugfs_backlight(lcd->bd, brightness_table, NULL);
}

#if defined(CONFIG_SMCDSD_MDNIE)
static int mdnie_send(struct lcd_info *lcd, struct lcd_seq_info *seq, u32 num)
{
	int ret = 0;

	mutex_lock(&lcd->lock);

	if (lcd->state != PANEL_STATE_RESUMED || !lcd->enable) {
		dev_info(&lcd->ld->dev, "%s: panel state is %d, %d\n", __func__, lcd->state, lcd->enable);
		ret = -EIO;
		goto exit;
	}

	ret = smcdsd_dsi_tx_set(lcd, seq, num);

exit:
	mutex_unlock(&lcd->lock);

	return ret;
}

static int mdnie_read(struct lcd_info *lcd, u8 addr, u8 *buf, u32 size)
{
	int ret = 0;

	mutex_lock(&lcd->lock);

	if (lcd->state != PANEL_STATE_RESUMED || !lcd->enable) {
		dev_info(&lcd->ld->dev, "%s: panel state is %d, %d\n", __func__, lcd->state, lcd->enable);
		ret = -EIO;
		goto exit;
	}

	ret = smcdsd_dsi_rx_data(lcd, addr, size, buf);

exit:
	mutex_unlock(&lcd->lock);

	return ret;
}
#endif

static int smcdsd_panel_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	int ret = 0;
	struct lcd_info *lcd;

	lcd = kzalloc(sizeof(struct lcd_info), GFP_KERNEL);
	if (!lcd) {
		pr_err("%s: failed to allocate for lcd\n", __func__);
		ret = -ENOMEM;
		goto exit;
	}

	lcd->ld = lcd_device_register("panel", dev, lcd, NULL);
	if (IS_ERR(lcd->ld)) {
		pr_err("%s: failed to register lcd device\n", __func__);
		ret = PTR_ERR(lcd->ld);
		goto exit;
	}

	lcd->bd = backlight_device_register("panel", dev, lcd, &panel_backlight_ops, NULL);
	if (IS_ERR(lcd->bd)) {
		pr_err("%s: failed to register backlight device\n", __func__);
		ret = PTR_ERR(lcd->bd);
		goto exit;
	}

	lcd->pdata = dev_get_platdata(dev);

	platform_set_drvdata(pdev, lcd);

	mutex_init(&lcd->lock);

	ret = ea8076_probe(lcd);
	if (ret < 0)
		dev_info(&lcd->ld->dev, "%s: failed to probe panel\n", __func__);

	lcd_init_sysfs(lcd);

	ea8076_register_notifier(lcd);

#if defined(CONFIG_SMCDSD_MDNIE)
	mdnie_register(&lcd->ld->dev, lcd, (mdnie_w)mdnie_send, (mdnie_r)mdnie_read, lcd->coordinate, &tune_info);
	lcd->mdnie_class = get_mdnie_class();
#endif

#if defined(CONFIG_SMCDSD_DOZE)
	disp_helper_set_option(DISP_OPT_AOD, 1);
#endif

	dev_info(&lcd->ld->dev, "%s: %s: done\n", kbasename(__FILE__), __func__);

exit:
	return ret;
}

static int smcdsd_panel_init(struct platform_device *p)
{
	struct lcd_info *lcd = get_lcd_info(p);

	dev_info(&lcd->ld->dev, "+ %s: %d\n", __func__, lcd->state);

	if (lcd->state == PANEL_STATE_SUSPENED) {
		ea8076_init(lcd);

		mutex_lock(&lcd->lock);
		lcd->state = PANEL_STATE_RESUMED;
		mutex_unlock(&lcd->lock);
	}

	dev_info(&lcd->ld->dev, "- %s: %d, %d\n", __func__, lcd->state, lcd->connected);

	return 0;
}

static int smcdsd_panel_displayon_late(struct platform_device *p)
{
	struct lcd_info *lcd = get_lcd_info(p);

	dev_info(&lcd->ld->dev, "+ %s: %d\n", __func__, lcd->state);

	mutex_lock(&lcd->lock);
	ea8076_displayon(lcd);
	mutex_unlock(&lcd->lock);

	dev_info(&lcd->ld->dev, "- %s: %d, %d\n", __func__, lcd->state, lcd->connected);

	return 0;
}

static int smcdsd_panel_exit(struct platform_device *pdev)
{
	struct lcd_info *lcd = platform_get_drvdata(pdev);

	dev_info(&lcd->ld->dev, "+ %s: %d\n", __func__, lcd->state);

	if (lcd->state == PANEL_STATE_SUSPENED)
		goto exit;

	ea8076_exit(lcd);

	mutex_lock(&lcd->lock);
	lcd->state = PANEL_STATE_SUSPENED;
	mutex_unlock(&lcd->lock);

	dev_info(&lcd->ld->dev, "- %s: %d, %d\n", __func__, lcd->state, lcd->connected);

exit:
	return 0;
}

#if defined(CONFIG_SMCDSD_DOZE)
static int smcdsd_panel_initalpm(struct platform_device *p)
{
	struct lcd_info *lcd = get_lcd_info(p);

	dev_info(&lcd->ld->dev, "+ %s: %d\n", __func__, lcd->state);

	if (lcd->state == PANEL_STATE_SUSPENED) {
		ea8076_init(lcd);

		mutex_lock(&lcd->lock);
		lcd->state = PANEL_STATE_RESUMED;
		lcd->display_on_delay_req = 1;
		mutex_unlock(&lcd->lock);
	}

	ea8076_enteralpm(lcd);

	dev_info(&lcd->ld->dev, "- %s: %d, %d\n", __func__, lcd->state, lcd->connected);

	return 0;
}

static int smcdsd_panel_exitalpm(struct platform_device *p)
{
	struct lcd_info *lcd = get_lcd_info(p);

	dev_info(&lcd->ld->dev, "+ %s: %d\n", __func__, lcd->state);

	if (lcd->state == PANEL_STATE_SUSPENED) {
		ea8076_init(lcd);

		mutex_lock(&lcd->lock);
		lcd->state = PANEL_STATE_RESUMED;
		mutex_unlock(&lcd->lock);
	}

	ea8076_exitalpm(lcd);

	smcdsd_panel_set_brightness(lcd, 1);

	dev_info(&lcd->ld->dev, "- %s: %d, %d\n", __func__, lcd->state, lcd->connected);

	return 0;
}
#endif

static int smcdsd_panel_set_mask(struct platform_device *pdev, int on)
{
	struct lcd_info *lcd = platform_get_drvdata(pdev);
	struct mipi_dsi_lcd_parent *pdata = dev_get_platdata(&pdev->dev);
	struct abd_protect *abd = &pdata->abd;

	/* update HBM state */
	lcd->mask_state = !!on;

	/* dimming off */
	lcd->trans_dimming = TRANS_DIMMING_OFF;

	if (lcd->mask_state)
		lcd->acl_dimming = ACL_DIMMING_OFF;
	else
		lcd->acl_dimming = ACL_DIMMING_ON;

	lcd->acl_dimming_update_req = 1;

	/*
	 * 2ms for wait for VFP period
	 * This delay make cmdq flush wait for 2ms.
	 */
	mdelay(2);

	/* Update BR with NO LCD mutex */
	_smcdsd_panel_set_brightness(lcd, 1, false);

	lcd->mask_framedone_check_req = true;

	if (on)
		smcdsd_abd_save_str(abd, "mask_brightness");
	else
		smcdsd_abd_save_str(abd, "prev_brightness");

	return 0;
}

static int smcdsd_panel_get_mask(struct platform_device *pdev)
{
	struct lcd_info *lcd = platform_get_drvdata(pdev);

	return lcd->mask_state;
}

static int smcdsd_panel_set_lock(struct platform_device *pdev)
{
	struct lcd_info *lcd = platform_get_drvdata(pdev);
	struct mipi_dsi_lcd_parent *pdata = dev_get_platdata(&pdev->dev);

	if (pdata->lcm_path_lock)
		mutex_lock(&lcd->lock);
	else
		mutex_unlock(&lcd->lock);

	dbg_info("%s: %d\n", __func__, pdata->lcm_path_lock);

	return 0;
}

static int smcdsd_panel_framedone_mask(struct platform_device *pdev)
{
	struct lcd_info *lcd = platform_get_drvdata(pdev);
	struct mipi_dsi_lcd_parent *pdata = dev_get_platdata(&pdev->dev);
	struct abd_protect *abd = &pdata->abd;

	if (lcd->mask_framedone_check_req) {
		mutex_lock(&lcd->lock);
		if (lcd->mask_state == 0) {
			lcd->trans_dimming = TRANS_DIMMING_ON;
			lcd->actual_mask_brightness = 0;
		} else {
			lcd->actual_mask_brightness = lcd->mask_brightness;
		}

		if (lcd->mask_state)
			smcdsd_abd_save_str(abd, "mask_done");
		else
			smcdsd_abd_save_str(abd, "prev_done");

		sysfs_notify(&lcd->ld->dev.kobj, NULL, "actual_mask_brightness");

		dbg_info("[SEC_MASK] %s:actual_mask_brightness: %d\n", __func__, lcd->actual_mask_brightness);

		lcd->mask_framedone_check_req = false;

		mutex_unlock(&lcd->lock);
	}
	return 0;
}

#if defined(CONFIG_SMCDSD_DYNAMIC_MIPI)
static int smcdsd_panel_framedone_dynamic_mipi(struct platform_device *pdev)
{
	struct lcd_info *lcd = platform_get_drvdata(pdev);
	struct mipi_dsi_lcd_parent *smcdsd = dev_get_platdata(&pdev->dev);

	/* lcd->enable => primary display  resume done ~ suspend start */
	/* In mask layer scenario, FFC TX will be delayed */
	if (smcdsd->lcm_path_lock == 0) {
		mutex_lock(&lcd->lock);
		if ((lcd->df_index_req != lcd->df_index_current) && lcd->enable) {
			DSI_WRITE(SEQ_TEST_KEY_ON_F0, ARRAY_SIZE(SEQ_TEST_KEY_ON_F0));
			DSI_WRITE(SEQ_TEST_KEY_ON_FC, ARRAY_SIZE(SEQ_TEST_KEY_ON_FC));
			ea8076_change_ffc(lcd, FFC_UPDATE);
			DSI_WRITE(SEQ_TEST_KEY_OFF_F0, ARRAY_SIZE(SEQ_TEST_KEY_OFF_F0));
			DSI_WRITE(SEQ_TEST_KEY_OFF_FC, ARRAY_SIZE(SEQ_TEST_KEY_OFF_FC));
		}
		mutex_unlock(&lcd->lock);
	}

	return 0;
}
#endif

static int smcdsd_panel_framedone(struct platform_device *pdev)
{
	smcdsd_panel_framedone_mask(pdev);
#if defined(CONFIG_SMCDSD_DYNAMIC_MIPI)
	smcdsd_panel_framedone_dynamic_mipi(pdev);
#endif

	return 0;
}

struct mipi_dsi_lcd_driver ea8076_mipi_lcd_driver = {
	.driver = {
		.name = "ea8076",
	},
	.probe		= smcdsd_panel_probe,
	.init		= smcdsd_panel_init,
	.exit		= smcdsd_panel_exit,
	.displayon_late = smcdsd_panel_displayon_late,
#if defined(CONFIG_SMCDSD_DOZE)
	.doze_init	= smcdsd_panel_initalpm,
	.doze_exit	= smcdsd_panel_exitalpm,
#endif
	.set_mask	= smcdsd_panel_set_mask,
	.get_mask	= smcdsd_panel_get_mask,
	.framedone_notify	= smcdsd_panel_framedone,
	.lock		= smcdsd_panel_set_lock,
};
__XX_ADD_LCD_DRIVER(ea8076_mipi_lcd_driver);

static void panel_conn_uevent(struct lcd_info *lcd)
{
	char *uevent_conn_str[3] = {"CONNECTOR_NAME=UB_CONNECT", "CONNECTOR_TYPE=HIGH_LEVEL", NULL};

	if (!IS_ENABLED(CONFIG_SEC_FACTORY))
		return;

	if (!lcd->conn_det_enable)
		return;

	kobject_uevent_env(&lcd->ld->dev.kobj, KOBJ_CHANGE, uevent_conn_str);

	dev_info(&lcd->ld->dev, "%s: %s, %s\n", __func__, uevent_conn_str[0], uevent_conn_str[1]);
}

static void panel_conn_work(struct work_struct *work)
{
	struct lcd_info *lcd = container_of(work, struct lcd_info, conn_work);

	dev_info(&lcd->ld->dev, "%s\n", __func__);

	panel_conn_uevent(lcd);
}

static irqreturn_t panel_conn_det_handler(int irq, void *dev_id)
{
	struct lcd_info *lcd = (struct lcd_info *)dev_id;

	dev_info(&lcd->ld->dev, "%s\n", __func__);

	queue_work(lcd->conn_workqueue, &lcd->conn_work);

	lcd->conn_det_count++;

	return IRQ_HANDLED;
}

static ssize_t conn_det_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct lcd_info *lcd = dev_get_drvdata(dev);
	int gpio_active = of_gpio_get_active("gpio_con");

	if (gpio_active < 0)
		sprintf(buf, "%d\n", -1);
	else
		sprintf(buf, "%s\n", gpio_active ? "disconnected" : "connected");

	dev_info(&lcd->ld->dev, "%s: %s\n", __func__, buf);

	return strlen(buf);
}

static ssize_t conn_det_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t size)
{
	struct lcd_info *lcd = dev_get_drvdata(dev);
	unsigned int value;
	int rc;
	int gpio_active = of_gpio_get_active("gpio_con");

	rc = kstrtouint(buf, 0, &value);
	if (rc < 0)
		return rc;

	if (gpio_active < 0)
		return -EINVAL;

	dev_info(&lcd->ld->dev, "%s: %u, %u\n", __func__, lcd->conn_det_enable, value);

	if (lcd->conn_det_enable != value) {
		mutex_lock(&lcd->lock);
		lcd->conn_det_enable = value;
		mutex_unlock(&lcd->lock);

		dev_info(&lcd->ld->dev, "%s: %s\n", __func__, gpio_active ? "disconnected" : "connected");
		if (lcd->conn_det_enable && gpio_active)
			panel_conn_uevent(lcd);
	}

	return size;
}

static DEVICE_ATTR(conn_det, 0644, conn_det_show, conn_det_store);

static void panel_conn_register(struct lcd_info *lcd)
{
	struct mipi_dsi_lcd_parent *pdata = lcd->pdata;
	struct abd_protect *abd = &pdata->abd;
	int gpio = 0, gpio_active = 0;

	if (!pdata) {
		dev_info(&lcd->ld->dev, "%s: pdata is invalid\n", __func__);
		return;
	}

	if (!lcd->connected) {
		dev_info(&lcd->ld->dev, "%s: lcd connected: %d\n", __func__, lcd->connected);
		return;
	}

	gpio = of_get_gpio_with_name("gpio_con");
	if (gpio < 0) {
		dev_info(&lcd->ld->dev, "%s: gpio_con is %d\n", __func__, gpio);
		return;
	}

	gpio_active = of_gpio_get_active("gpio_con");
	if (gpio_active) {
		dev_info(&lcd->ld->dev, "%s: gpio_con_active is %d\n", __func__, gpio_active);
		return;
	}

	INIT_WORK(&lcd->conn_work, panel_conn_work);

	lcd->conn_workqueue = create_singlethread_workqueue("lcd_conn_workqueue");
	if (!lcd->conn_workqueue) {
		dev_info(&lcd->ld->dev, "%s: create_singlethread_workqueue fail\n", __func__);
		return;
	}

	smcdsd_abd_pin_register_handler(abd, gpio_to_irq(gpio), panel_conn_det_handler, lcd);

	if (!IS_ENABLED(CONFIG_SEC_FACTORY))
		return;

	smcdsd_abd_con_register(abd);
	device_create_file(&lcd->ld->dev, &dev_attr_conn_det);
}

static int match_dev_name(struct device *dev, void *data)
{
	const char *keyword = data;

	return dev_name(dev) ? !!strstr(dev_name(dev), keyword) : 0;
}

static struct device *find_lcd_device(void)
{
	struct platform_device *pdev = NULL;
	struct device *dev = NULL;

	pdev = of_find_abd_dt_parent_platform_device();
	if (!pdev) {
		dbg_info("%s: of_find_device_by_node fail\n", __func__);
		return NULL;
	}

	dev = device_find_child(&pdev->dev, "panel", match_dev_name);
	if (!dev) {
		dbg_info("%s: device_find_child fail\n", __func__);
		return NULL;
	}

	if (dev)
		put_device(dev);

	return dev;
}

static int __init panel_conn_init(void)
{
	struct device *dev = find_lcd_device();
	struct lcd_info *lcd = NULL;

	if (!dev) {
		dbg_info("find_lcd_device fail\n");
		return 0;
	}

	lcd = dev_get_drvdata(dev);
	if (!lcd) {
		dbg_info("lcd_info invalid\n");
		return 0;
	}

	if (unlikely(!lcd->conn_init_done)) {
		lcd->conn_init_done = 1;
		panel_conn_register(lcd);
	}

	return 0;
}

late_initcall_sync(panel_conn_init);

