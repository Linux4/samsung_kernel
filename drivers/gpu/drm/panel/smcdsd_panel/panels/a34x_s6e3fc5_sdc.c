// SPDX-License-Identifier: GPL-2.0
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
#include "../smcdsd_dsi_msg.h"
#include "../smcdsd_notify.h"
#include "../smcdsd_panel.h"
#include "dd.h"

#if defined(CONFIG_SMCDSD_DPUI)
#include "dpui.h"
#endif

#include "a34x_s6e3fc5_sdc.h"
#if defined(CONFIG_MTK_ROUND_CORNER_SUPPORT)
#include "a34x_s6e3fc5_sdc_round.h"
#endif

#undef pr_fmt
#define pr_fmt(fmt) "lcd panel: %s: " fmt, __func__

#define dbg_info(fmt, ...)	pr_info(fmt, ##__VA_ARGS__)

#define get_shift_width(value, shift, width)	((value >> shift) & (GENMASK(width - 1, 0)))

#if defined(CONFIG_SMCDSD_DOZE)
#define IS_NORMAL_MODE(_lcd)		(((_lcd)->state) && !((_lcd)->doze_state))
#else
#define IS_NORMAL_MODE(_lcd)		(((_lcd)->state))
#endif

union lpm_info {
	u32 value;
	struct {
		u16 index;
		u8 mode;	/* comes from sysfs. 0(off), 1(alpm 2nit), 2(hlpm 2nit), 3(alpm 60nit), 4(hlpm 60nit) or 1(alpm), 2(hlpm) */
		u8 ver;		/* comes from sysfs. 0(old), 1(new) */
	};
};

struct lcd_info {
	unsigned int			connected;
	unsigned int			state;	/* 1: ON/DOZE, 0: OFF/DOZE_SUSPEND */
	unsigned int			shutdown;

	struct lcd_device		*ld;
	struct backlight_device		*bd;

	union {
		struct {
			u8		reserved;
			u8		id[LDI_LEN_ID];
		};
		u32			id_value;
	};

	int				lux;

	struct device			svc_dev;

	unsigned char			*code;
	unsigned char			*date;
	unsigned char			*manufacture_info;
	unsigned char			*coordinate;

	unsigned int			coordinate_xy[2];
	unsigned char			coordinates[20];

	struct mipi_dsi_lcd_common	*pdata;
	struct mutex			lock;

	struct notifier_block		panel_early_notifier;
	struct notifier_block		panel_after_notifier;
	struct notifier_block		reboot_notifier;

	unsigned int			fac_info;
	unsigned int			fac_done;
	unsigned int			mcd_mode;
	unsigned int			dsc_crc_state;

	unsigned int			conn_det_enable;
	unsigned int			conn_det_count;
	struct workqueue_struct		*conn_workqueue;
	struct work_struct		conn_work;

	unsigned int			brightness;
	unsigned int			adaptive_control;
	unsigned int			irc_mode;
	int				temperature;
	unsigned int			vrefresh;
	unsigned int			disp_mode_idx;

	unsigned int			current_brightness;	//todo: bit check
	unsigned int			current_adaptive_control;
	int				current_temperature;
	unsigned int			current_vrefresh;

	unsigned int			current_state;
	unsigned int			current_enable;

	unsigned int			doze_state;
#if defined(CONFIG_SMCDSD_DOZE)
	union lpm_info			alpm;
	union lpm_info			current_alpm;

	unsigned int			prev_brightness;
#endif
#if defined(CONFIG_SMCDSD_MDNIE)
	struct class			*mdnie_class;
	unsigned int			mdnie_inactive;
#endif
#if defined(CONFIG_SMCDSD_DYNAMIC_MIPI)
	struct notifier_block		df_notif;
	unsigned int			df_index_req;
	unsigned int			df_index_current;
#endif
#if defined(CONFIG_SMCDSD_DPUI)
	struct notifier_block		dpui_notif;
	struct notifier_block		dpci_notif;
#endif
//prevent undef compile error #if defined(CONFIG_SMCDSD_MASK_LAYER)
	unsigned int			te_cnt;
	unsigned int			mask_state;
	unsigned int			mask_brightness;
//#endif
#if defined(CONFIG_SMCDSD_MASK_LAYER)
	unsigned int			actual_mask_brightness;

	unsigned int			hbm_en;	//todo: merge with mask_state
	unsigned int			hbm_wait;
	u64				hbm_time_stamp;
	u64				hbm_interval;
#endif
};

#define get_shift_width(value, shift, width)	((value >> shift) & (GENMASK(width - 1, 0)))

static struct lcd_info *get_lcd_info(struct platform_device *p)
{
	return platform_get_drvdata(p);
}

static void set_lcd_info(struct platform_device *p, void *data)
{
	platform_set_drvdata(p, data);
}

static struct mipi_dsi_lcd_common *get_lcd_common_info(struct platform_device *p)
{
	struct device *parent = p->dev.parent;

	return dev_get_drvdata(parent);
}

static int smcdsd_commit_lock(struct lcd_info *lcd, bool lock)
{
	return lcd->pdata->commit_lock(NULL, lock);
}

static int smcdsd_dsi_tx_package(struct lcd_info *lcd, struct msg_package *package)
{
	struct msg_segment *segment[MAX_SEGMENT_NUM] = {0, };

	return send_msg_package(&package, 1, &segment);
}

static int _smcdsd_dsi_rx_data(struct lcd_info *lcd, u8 cmd, u32 size, u8 *buf, u32 offset)
{
	int ret = 0, rx_size = 0;
	int retry = 2;

	if (!lcd->connected)
		return ret;

try_read:
	rx_size = lcd->pdata->rx(lcd->pdata, MIPI_DSI_DCS_READ, offset, cmd, size, buf, 0);
	dev_info(&lcd->ld->dev, "%s: %2d(%2d), %02x, %*ph%s\n", __func__, size, rx_size, cmd,
		min_t(u32, min_t(u32, size, rx_size), 10), buf, (rx_size > 10) ? "..." : "");
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
	offset = 0;

again:
	slice = (remain > limit) ? limit : remain;
	ret = _smcdsd_dsi_rx_data_offset(lcd, addr, slice, &buf[offset], offset);
	if (ret < 0) {
		dev_info(&lcd->ld->dev, "%s: fail\n", __func__);
		return ret;
	}

	remain -= limit;
	offset += limit;

	if (remain > 0)
		goto again;

	return ret;
}

static int smcdsd_dsi_rx_info(struct lcd_info *lcd, u8 reg, u32 len, u8 *rxbuf)
{
	int ret = 0, i;
	u8 buf[MAX_RX_CMD_NUM] = {0, };

	ret = smcdsd_dsi_rx_data(lcd, reg, len, buf);
	if (ret < 0) {
		dev_info(&lcd->ld->dev, "%s: fail. %02x, ret: %d\n", __func__, reg, ret);
		goto exit;
	}

	dev_dbg(&lcd->ld->dev, "%s: %02xh\n", __func__, reg);
	for (i = 0; i < len; i++)
		dev_dbg(&lcd->ld->dev, "%02dth value is %02x, %3d\n", i + 1, buf[i], buf[i]);

	if (rxbuf)
		memcpy(rxbuf, buf, len);

exit:
	return ret;
}

#if defined(CONFIG_SMCDSD_MDNIE)
static int __mdnie_send(struct lcd_info *lcd, struct msg_segment *seq, u32 num)
{
	int ret = 0;

	struct msg_package package = { seq, num };

	ret = smcdsd_dsi_tx_package(lcd, &package);

	return ret;
}

static int mdnie_send(struct lcd_info *lcd, struct msg_segment *seq, u32 num)
{
	int ret = 0;

	if (lcd->mdnie_inactive) {
		dev_info(&lcd->ld->dev, "%s: mdnie_inactive(%u)\n", __func__, lcd->mdnie_inactive);
		return 0;
	}

	smcdsd_commit_lock(lcd, true);
	mutex_lock(&lcd->lock);

	if (lcd->state == 0) {
		dev_info(&lcd->ld->dev, "%s: panel state(%u)\n", __func__, lcd->state);
		ret = -EIO;
		goto exit;
	}

	ret = __mdnie_send(lcd, seq, num);
exit:
	mutex_unlock(&lcd->lock);
	smcdsd_commit_lock(lcd, false);

	return ret;
}

static int mdnie_read(struct lcd_info *lcd, u8 addr, u8 *buf, u32 size)
{
	int ret = 0;

	if (lcd->mdnie_inactive) {
		dev_info(&lcd->ld->dev, "%s: mdnie_inactive(%u)\n", __func__, lcd->mdnie_inactive);
		return 0;
	}

	mutex_lock(&lcd->lock);

	if (lcd->state == 0) {
		dev_info(&lcd->ld->dev, "%s: panel state(%u)\n", __func__, lcd->state);
		ret = -EIO;
		goto exit;
	}

	ret = smcdsd_dsi_rx_data(lcd, addr, size, buf);

exit:
	mutex_unlock(&lcd->lock);

	return ret;
}
#endif

static int smcdsd_panel_send_msg(struct lcd_info *lcd, int force)
{
	int count = 0;
	struct msg_segment *segment[MAX_TX_CMD_NUM] = {0, };
	struct msg_package *package[MAX_TX_CMD_NUM];

	lcd->brightness = lcd->mask_state ? lcd->mask_brightness : lcd->bd->props.brightness;

	if (lcd->hbm_interval) {
		run_timer_to(NULL, "hbm_right_after_brightness", "hbm_interval");
		lcd->hbm_interval = 0;
	}

	dev_info(&lcd->ld->dev, "%s: brightness(%3d) acl(%d) refresh(%3d) degree(%3d) mode(%d)\n", __func__,
		lcd->brightness, lcd->adaptive_control, lcd->vrefresh, lcd->temperature, lcd->disp_mode_idx);

	/* KEY OPEN */
	package[count++] = GET_PACKAGE(&PACKAGE_LIST[MSG_IDX_BASE][GET_ENUM_WITH_NAME(MSG_S6E3FC5_SDC_KEY_ON)]);

	/* VRR */
	package[count++] = GET_PACKAGE(&PACKAGE_LIST[MSG_IDX_BASE][disp_mode_table[lcd->disp_mode_idx]], lcd->brightness);

	/* Sync. Control and Dimming Speed */
	package[count++] = GET_PACKAGE(&PACKAGE_LIST[MSG_IDX_BASE][GET_ENUM_WITH_NAME(MSG_S6E3FC5_SDC_DIMMING_SPEED)], !!(lcd->mask_state | lcd->hbm_en), lcd->brightness);

	/* BRIGHTNESS */
	package[count++] = GET_PACKAGE(&PACKAGE_LIST[MSG_IDX_BASE][GET_ENUM_WITH_NAME(MSG_S6E3FC5_SDC_BRIGHTNESS)], lcd->brightness);

	/* ACL */
	package[count++] = GET_PACKAGE(&PACKAGE_LIST[MSG_IDX_BASE][acl_table[lcd->adaptive_control][!!lcd->mask_state])];

	/* IRC */
	package[count++] = GET_PACKAGE(&PACKAGE_LIST[MSG_IDX_BASE][irc_table[lcd->irc_mode]]);

	/* TSET */
	package[count++] = GET_PACKAGE(&PACKAGE_LIST[MSG_IDX_BASE][tset_table[TEMPERATURE_UP(lcd->temperature)]]);

	/* UPDATE */
	package[count++] = GET_PACKAGE(&PACKAGE_LIST[MSG_IDX_BASE][GET_ENUM_WITH_NAME(MSG_S6E3FC5_SDC_PANEL_UPDATE)]);

	/* KEY CLOSE */
	if (lcd->mask_state | lcd->hbm_en)
		package[count++] = GET_PACKAGE(&PACKAGE_LIST[MSG_IDX_BASE][GET_ENUM_WITH_NAME(MSG_S6E3FC5_SDC_KEY_OFF_BLOCKING)]);
	else
		package[count++] = GET_PACKAGE(&PACKAGE_LIST[MSG_IDX_BASE][GET_ENUM_WITH_NAME(MSG_S6E3FC5_SDC_KEY_OFF)]);

	return send_msg_package(package, count, &segment);
}

#if defined(CONFIG_SMCDSD_DOZE)
static int smcdsd_setalpm(struct lcd_info *lcd)
{
	int ret = 0;
	union lpm_info lpm = {0, };

	dev_info(&lcd->ld->dev, "%s: brightness: %3d, lpm: %06x(%06x)\n", __func__,
		lcd->bd->props.brightness, lcd->current_alpm.value, lcd->alpm.value);

	lpm.value = lcd->alpm.value;
	lpm.index = (lpm.ver && lpm.mode) ? lpm_init_table[lcd->bd->props.brightness] : lpm_old_table[lpm.mode];

	smcdsd_dsi_tx_package(lcd, &PACKAGE_LIST[MSG_IDX_BASE][lpm.index]);

	lcd->current_alpm.value = lpm.value;

	dev_info(&lcd->ld->dev, "%s: %s\n", __func__, AOD_HLPM_STATE_NAME[lpm.index]);

	return ret;
}

static int s6e3fc5_sdc_doze_init(struct lcd_info *lcd)
{
	int ret = 0;
	union lpm_info lpm = {0, };

	dev_info(&lcd->ld->dev, "%s: brightness: %3d, lpm: %06x(%06x)\n", __func__,
		lcd->bd->props.brightness, lcd->current_alpm.value, lcd->alpm.value);

	lpm.value = lcd->alpm.value;
	lpm.index = (lpm.ver && lpm.mode) ? lpm_init_table[lcd->bd->props.brightness] : lpm_old_table[lpm.mode];

	if (lcd->current_alpm.value == lpm.value) {
		dev_info(&lcd->ld->dev, "%s: lpm: %06x(%06x) skip same value\n", __func__,
			lcd->current_alpm.value, lpm.value);
		//goto exit;
	}

	//s6e3fc5_sdc_disable_self_mask(lcd);

	ret = smcdsd_setalpm(lcd);	/* todo: move to enable for aod backlight */
	if (ret < 0)
		dev_info(&lcd->ld->dev, "%s: failed to set alpm\n", __func__);

	lcd->current_alpm.value = lpm.value;
//exit:
	return ret;
}

static int s6e3fc5_sdc_doze_exit(struct lcd_info *lcd)
{
	int ret = 0;
	union lpm_info lpm = {0, };

	dev_info(&lcd->ld->dev, "%s: brightness: %3d, lpm: %06x(%06x)\n", __func__,
		lcd->bd->props.brightness, lcd->current_alpm.value, lcd->alpm.value);

	/* 1. Update brightness value first */
	lcd->brightness = lcd->mask_state ? lcd->mask_brightness : lcd->bd->props.brightness;
	smcdsd_dsi_tx_package(lcd, GET_PACKAGE(&PACKAGE_LIST[MSG_IDX_BASE][GET_ENUM_WITH_NAME(MSG_S6E3FC5_SDC_BRIGHTNESS)], lcd->brightness));
	msleep(35);

	/* 2. Exit HLPM */
	lpm.value = lcd->alpm.value;
	lpm.index = (lpm.ver && lpm.mode) ? lpm_exit_table[lcd->bd->props.brightness] : lpm_old_table[lpm.mode];

	smcdsd_dsi_tx_package(lcd, &PACKAGE_LIST[MSG_IDX_BASE][lpm.index]);

	dev_info(&lcd->ld->dev, "%s: %s\n", __func__, AOD_HLPM_STATE_NAME[lpm.index]);

	//s6e3fc5_sdc_enable_self_mask(lcd);

	lcd->current_alpm.value = 0;

	return ret;
}

static int smcdsd_panel_send_msg_lpm(struct lcd_info *lcd, int force)
{
	return smcdsd_setalpm(lcd);
}
#else
#define smcdsd_panel_send_msg_lpm
#endif

static int smcdsd_panel_set_brightness(struct lcd_info *lcd, int force)
{
	int ret;

	if (lcd->doze_state)
		ret = smcdsd_panel_send_msg_lpm(lcd, force);
	else
		ret = smcdsd_panel_send_msg(lcd, force);

	return ret;
}

static int panel_get_brightness(struct backlight_device *bd)
{
	struct lcd_info *lcd = bl_get_data(bd);

	return lcd->brightness;
}

static int panel_set_brightness(struct backlight_device *bd)
{
	int ret = 0;
	struct lcd_info *lcd = bl_get_data(bd);

	smcdsd_commit_lock(lcd, true);
	mutex_lock(&lcd->lock);
	if (lcd->state) {
		ret = smcdsd_panel_set_brightness(lcd, 0);
		if (ret < 0)
			dev_info(&lcd->ld->dev, "%s: failed to set brightness\n", __func__);
	}
	mutex_unlock(&lcd->lock);
	smcdsd_commit_lock(lcd, false);

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

static int smcdsd_panel_mask_bit_info(struct lcd_info *lcd, u32 index)
{
	int ret = 0;
	u8 *buf;
	struct abd_bit_info *bit_info_list = ldi_bit_info_list;
	unsigned int reg, len, mask, offset, invert;
	char **print_org = NULL;
	struct mipi_dsi_lcd_common *pdata = lcd->pdata;

	if (!lcd->connected)
		return ret;

	if (index >= LDI_BIT_ENUM_MAX) {
		dev_info(&lcd->ld->dev, "%s: invalid index(%d)\n", __func__, index);
		ret = -EINVAL;
		return ret;
	}

	reg = bit_info_list[index].reg;
	len = bit_info_list[index].len;
	print_org = bit_info_list[index].print;
	offset = bit_info_list[index].offset;
	invert = bit_info_list[index].invert;
	buf = bit_info_list[index].result_buf;

	smcdsd_abd_mask_bit(&pdata->abd, len * BITS_PER_BYTE, get_merged_value(buf, len), print_org, invert);

#if defined(CONFIG_SMCDSD_DPUI)
{
	struct abd_bit_info *bit_dpui_list = ldi_bit_dpui_list;
	unsigned int i, key, value;

	for (i = 0; i < ARRAY_SIZE(ldi_bit_dpui_list); i++) {
		if (reg != bit_dpui_list[i].reg)
			continue;

		key = bit_dpui_list[i].dpui_key;
		invert = bit_dpui_list[i].invert;
		mask = bit_dpui_list[i].mask;

		value = (buf[offset] & mask) ^ invert;

		inc_dpui_u32_field(key, value);
		if (value)
			dev_info(&lcd->ld->dev, "%s: key(%d) invalid\n", __func__, key);
	}
}
#endif

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

	size = snprintf(tbuf, MAX_DPUI_VAL_LEN, "%d", lcd->id[0]);
	set_dpui_field(DPUI_KEY_LCDID1, tbuf, size);
	size = snprintf(tbuf, MAX_DPUI_VAL_LEN, "%d", lcd->id[1]);
	set_dpui_field(DPUI_KEY_LCDID2, tbuf, size);
	size = snprintf(tbuf, MAX_DPUI_VAL_LEN, "%d", lcd->id[2]);
	set_dpui_field(DPUI_KEY_LCDID3, tbuf, size);

	size = snprintf(tbuf, MAX_DPUI_VAL_LEN, "0x%02X%02X%02X%02X%02X",
		lcd->code[0], lcd->code[1], lcd->code[2], lcd->code[3], lcd->code[4]);
	set_dpui_field(DPUI_KEY_CHIPID, tbuf, size);

	size = snprintf(tbuf, MAX_DPUI_VAL_LEN, "%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X",
		lcd->date[0], lcd->date[1], lcd->date[2], lcd->date[3], lcd->date[4], lcd->date[5], lcd->date[6],
		lcd->coordinate[0], lcd->coordinate[1], lcd->coordinate[2], lcd->coordinate[3]);
	set_dpui_field(DPUI_KEY_CELLID, tbuf, size);

	m_info = lcd->manufacture_info;
	site = get_shift_width(m_info[0], 4, 4);
	rework = get_shift_width(m_info[0], 0, 4);
	poc = get_shift_width(m_info[1], 0, 4);
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

static int panel_dpci_notifier_callback(struct notifier_block *self,
				unsigned long event, void *data)
{
	struct lcd_info *lcd = NULL;

	lcd = container_of(self, struct lcd_info, dpci_notif);

	return NOTIFY_DONE;
}
#endif /* CONFIG_SMCDSD_DPUI */

#if defined(CONFIG_SMCDSD_DYNAMIC_MIPI)
static int s6e3fc5_sdc_change_ffc(struct lcd_info *lcd, unsigned int ffc_on)
{
	int ret = 0;

	dev_info(&lcd->ld->dev, "%s: %d->%d\n", __func__, lcd->df_index_current, lcd->df_index_req);

	smcdsd_dsi_tx_package(lcd, &PACKAGE_LIST[MSG_IDX_BASE][ffc_table[lcd->df_index_req]]);

	lcd->df_index_current = lcd->df_index_req;

	return ret;
}

static int search_dynamic_freq_idx(struct lcd_info *lcd,
				struct df_freq_tbl_info *df_freq_tbl, int band_idx, int freq)
{
	int i, ret = 0;
	int min, max, array_idx;
	struct dynamic_freq_range *array;
	struct df_freq_tbl_info *df_tbl;

	if (band_idx >= FREQ_RANGE_MAX) {
		dev_err(&lcd->ld->dev, "%s: exceed max band idx(%d)\n", __func__, band_idx);
		ret = -EINVAL;
		goto search_exit;
	}

	df_tbl = &df_freq_tbl[band_idx];

	if (df_tbl == NULL) {
		dev_err(&lcd->ld->dev, "%s: failed to find band_idx(%d)\n", __func__, band_idx);
		ret = -EPERM;
		goto search_exit;
	}
	array_idx = df_tbl->size;

	if (array_idx == 1) {
		array = &df_tbl->array[0];
		dev_info(&lcd->ld->dev, "%s: Found adap_freq idx(0): %d\n",
			__func__, array->freq_idx);
		ret = array->freq_idx;
	} else {
		for (i = 0; i < array_idx; i++) {
			array = &df_tbl->array[i];
			dev_info(&lcd->ld->dev, "%s: min(%d) max(%d)\n", __func__, array->min, array->max);

			min = (int)freq - array->min;
			max = (int)freq - array->max;

			if ((min >= 0) && (max <= 0)) {
				dev_info(&lcd->ld->dev, "%s: Found adap_freq idx(%d)\n",
					__func__, array->freq_idx);
				ret = array->freq_idx;
				break;
			}
		}

		if (i >= array_idx) {
			dev_err(&lcd->ld->dev, "%s: Can't found freq idx(%d)\n", __func__, i);
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

		df_idx = search_dynamic_freq_idx(lcd, a34_dynamic_freq_set,
						ch_info->band, ch_info->channel);

		dev_info(&lcd->ld->dev, "%s: (b:%d, c:%d) df:%d\n",
				__func__, ch_info->band, ch_info->channel, df_idx);

		if (clamp_val(df_idx, DYNAMIC_MIPI_INDEX_0, DYNAMIC_MIPI_INDEX_MAX - 1) != df_idx)
			return NOTIFY_DONE;

		if (lcd->df_index_current == df_idx)
			return NOTIFY_DONE;

		//smcdsd_commit_lock(lcd, true);
		//mutex_lock(&lcd->lock);
		//if (lcd->state)
		//	s6e3fc5_sdc_change_ffc(lcd, FFC_OFF);
		//mutex_unlock(&lcd->lock);
		//smcdsd_commit_lock(lcd, false);

		smcdsd_panel_dsi_clk_change(lcd->pdata, df_idx);

		mutex_lock(&lcd->lock);
		lcd->df_index_req = df_idx;
		mutex_unlock(&lcd->lock);
	}

	return NOTIFY_DONE;
}

static int smcdsd_panel_framedone_dynamic_mipi(struct platform_device *p)
{
	struct lcd_info *lcd = get_lcd_info(p);

	if (!lcd)
		return 0;

	smcdsd_commit_lock(lcd, true);
	mutex_lock(&lcd->lock);
	if (lcd->df_index_req != lcd->df_index_current && lcd->state)
		s6e3fc5_sdc_change_ffc(lcd, 1);
	mutex_unlock(&lcd->lock);
	smcdsd_commit_lock(lcd, false);

	return 0;
}
#endif

static int s6e3fc5_sdc_read_id(struct lcd_info *lcd)
{
	int i = 0, ret = 0;
	struct mipi_dsi_lcd_common *pdata = lcd->pdata;

	lcd->id_value = 0;
	pdata->lcdconnected = lcd->connected = lcdtype ? 1 : 0;

	for (i = 0; i < LDI_LEN_ID; i++) {
		ret = smcdsd_dsi_rx_info(lcd, LDI_REG_ID + i, 1, &lcd->id[i]);
		if (ret < 0)
			break;
	}

	if (ret < 0 || !lcd->id_value) {
		pdata->lcdconnected = lcd->connected = 0;
		dev_info(&lcd->ld->dev, "%s: connected lcd is invalid\n", __func__);

		if (lcdtype)
			smcdsd_abd_save_str(&pdata->abd, "ID Read Fail");
	}

	dev_info(&lcd->ld->dev, "%s: %06x\n", __func__, cpu_to_be32(lcd->id_value));

	return ret;
}

static int s6e3fc5_sdc_read_init_info(struct lcd_info *lcd)
{
	int ret = 0;
	struct mipi_dsi_lcd_common *pdata = lcd->pdata;

	pdata->lcdconnected = lcd->connected = lcdtype ? 1 : 0;

	lcd->id[0] = (lcdtype & 0xFF0000) >> 16;
	lcd->id[1] = (lcdtype & 0x00FF00) >> 8;
	lcd->id[2] = (lcdtype & 0x0000FF) >> 0;

	dev_info(&lcd->ld->dev, "%s: %x\n", __func__, cpu_to_be32(lcd->id_value));

	s6e3fc5_sdc_read_id(lcd);

	if (!lcd->fac_info)
		return ret;

	lcd->coordinate = BUF_COORDINATE;
	lcd->date = BUF_DATE;
	lcd->manufacture_info = BUF_MANUFACTURE_INFO;
	lcd->code = BUF_CODE;

	send_msg_segment(MSG_S6E3FC5_SDC_KEY_ON, ARRAY_SIZE(MSG_S6E3FC5_SDC_KEY_ON));
	send_msg_segment(MSG_S6E3FC5_SDC_FAC_INFO, ARRAY_SIZE(MSG_S6E3FC5_SDC_FAC_INFO));
	send_msg_segment(MSG_S6E3FC5_SDC_GAMMA_OFFSET, ARRAY_SIZE(MSG_S6E3FC5_SDC_GAMMA_OFFSET));
	send_msg_segment(MSG_S6E3FC5_SDC_KEY_OFF, ARRAY_SIZE(MSG_S6E3FC5_SDC_KEY_OFF));

	lcd->coordinate_xy[0] = lcd->coordinate[0] << 8 | lcd->coordinate[1];	/* X */
	lcd->coordinate_xy[1] = lcd->coordinate[2] << 8 | lcd->coordinate[3];	/* Y */
	scnprintf(lcd->coordinates, sizeof(lcd->coordinates), "%d %d\n", lcd->coordinate_xy[0], lcd->coordinate_xy[1]);

#if defined(CONFIG_SMCDSD_MDNIE)
	attr_store_for_each(lcd->mdnie_class, "color_coordinate", lcd->coordinates, strlen(lcd->coordinates));
#endif

	return ret;
}

static int s6e3fc5_sdc_exit(struct lcd_info *lcd)
{
	int ret = 0;

	dev_info(&lcd->ld->dev, "%s\n", __func__);

	smcdsd_dsi_tx_package(lcd, &PACKAGE_LIST[MSG_IDX_BASE][GET_ENUM_WITH_NAME(MSG_S6E3FC5_SDC_EXIT)]);

	smcdsd_panel_mask_bit_info(lcd, LDI_BIT_ENUM_05);
	smcdsd_panel_mask_bit_info(lcd, LDI_BIT_ENUM_0A);
	smcdsd_panel_mask_bit_info(lcd, LDI_BIT_ENUM_0E);
	smcdsd_panel_mask_bit_info(lcd, LDI_BIT_ENUM_E9);
	smcdsd_panel_mask_bit_info(lcd, LDI_BIT_ENUM_EE);

#if defined(CONFIG_SMCDSD_DOZE)
	lcd->current_alpm.value = lcd->doze_state = 0;
#endif

	return ret;
}

static int s6e3fc5_sdc_init(struct lcd_info *lcd)
{
	int ret = 0;

	dev_info(&lcd->ld->dev, "%s\n", __func__);

	s6e3fc5_sdc_read_id(lcd);

	smcdsd_dsi_tx_package(lcd, &PACKAGE_LIST[MSG_IDX_BASE][GET_ENUM_WITH_NAME(MSG_S6E3FC5_SDC_INIT)]);
#if defined(CONFIG_SMCDSD_DYNAMIC_MIPI)
	s6e3fc5_sdc_change_ffc(lcd, 1);
#endif

	s6e3fc5_sdc_read_init_info(lcd);

	smcdsd_panel_mask_bit_info(lcd, LDI_BIT_ENUM_0F);

	smcdsd_panel_set_brightness(lcd, 1);

	return ret;
}

static int s6e3fc5_sdc_displayon(struct lcd_info *lcd)
{
	int ret = 0;

	dev_info(&lcd->ld->dev, "%s\n", __func__);

	/* 12. Display On(29h) */
	/* Display on cmd will be sent .set_dispon_cmdq */

	smcdsd_dsi_tx_package(lcd, &PACKAGE_LIST[MSG_IDX_BASE][GET_ENUM_WITH_NAME(MSG_S6E3FC5_SDC_DISPLAY_ON)]);

	return ret;
}

static int panel_early_notifier_callback(struct notifier_block *self,
			unsigned long event, void *data)
{
	struct fb_event *evdata = data;
	struct lcd_info *lcd = NULL;
	int fb_blank;

	switch (event) {
	case SMCDSD_EARLY_EVENT_BLANK:
		break;
	default:
		return NOTIFY_DONE;
	}

	lcd = container_of(self, struct lcd_info, panel_early_notifier);

	fb_blank = *(int *)evdata->data;

	dev_info(&lcd->ld->dev, "%s: %02lx, %d\n", __func__, event, fb_blank);

	if (evdata->info->node)
		return NOTIFY_DONE;

	mutex_lock(&lcd->lock);
	lcd->state = 0;
	mutex_unlock(&lcd->lock);

	return NOTIFY_DONE;
}

static int panel_after_notifier_callback(struct notifier_block *self,
			unsigned long event, void *data)
{
	struct fb_event *evdata = data;
	struct lcd_info *lcd = NULL;
	int fb_blank;

	switch (event) {
	case SMCDSD_EVENT_BLANK:
		break;
	default:
		return NOTIFY_DONE;
	}

	lcd = container_of(self, struct lcd_info, panel_after_notifier);

	fb_blank = *(int *)evdata->data;

	dev_info(&lcd->ld->dev, "%s: %02lx, %d\n", __func__, event, fb_blank);

	if (evdata->info->node)
		return NOTIFY_DONE;

	if (fb_blank != FB_BLANK_UNBLANK)
		return NOTIFY_DONE;

	if (lcd->shutdown)
		return NOTIFY_DONE;

	mutex_lock(&lcd->lock);

	//move to drm_panel_enable class_for_each_device(lcd->mdnie_class, NULL, __mdnie_send, mdnie_force_update);

	lcd->state = 1;

	mutex_unlock(&lcd->lock);

	return NOTIFY_DONE;
}

static int panel_reboot_notify(struct notifier_block *self,
			unsigned long event, void *unused)
{
	struct lcd_info *lcd = container_of(self, struct lcd_info, reboot_notifier);

	dev_info(&lcd->ld->dev, "%s\n", __func__);

	mutex_lock(&lcd->lock);
	lcd->state = 0;
	lcd->shutdown = 1;
	mutex_unlock(&lcd->lock);

	return NOTIFY_DONE;
}

static int s6e3fc5_sdc_register_notifier(struct lcd_info *lcd)
{
	lcd->panel_early_notifier.notifier_call = panel_early_notifier_callback;
	lcd->panel_early_notifier.priority = smcdsd_nb_priority_max.priority - 2;
	smcdsd_register_notifier(&lcd->panel_early_notifier);

	lcd->panel_after_notifier.notifier_call = panel_after_notifier_callback;
	lcd->panel_after_notifier.priority = smcdsd_nb_priority_min.priority + 2;
	smcdsd_register_notifier(&lcd->panel_after_notifier);

	lcd->reboot_notifier.notifier_call = panel_reboot_notify;
	register_reboot_notifier(&lcd->reboot_notifier);

#if defined(CONFIG_SMCDSD_DPUI)
	lcd->dpui_notif.notifier_call = panel_dpui_notifier_callback;
	lcd->dpci_notif.notifier_call = panel_dpci_notifier_callback;

	if (lcd->connected) {
		dpui_logging_register(&lcd->dpui_notif, DPUI_TYPE_PANEL);
		dpui_logging_register(&lcd->dpci_notif, DPUI_TYPE_CTRL);
	}
#endif
#if defined(CONFIG_SMCDSD_DYNAMIC_MIPI)
	lcd->df_notif.notifier_call = panel_dynamic_freq_notifier_callback;
	if (lcd->connected)
		register_dev_ril_bridge_event_notifier(&lcd->df_notif);
#endif

	dev_info(&lcd->ld->dev, "%s\n", __func__);

	return 0;
}

static int s6e3fc5_sdc_probe(struct lcd_info *lcd)
{
	dev_info(&lcd->ld->dev, "+ %s\n", __func__);

	lcd->bd->props.max_brightness = EXTEND_BRIGHTNESS;
	lcd->bd->props.brightness = UI_DEFAULT_BRIGHTNESS;

	lcd->state = 1;
	lcd->temperature = NORMAL_TEMPERATURE;
	lcd->adaptive_control = 1;
	lcd->lux = -1;

	lcd->vrefresh = 60;

	lcd->fac_info = 1;

	smcdsd_commit_lock(lcd, true);
	mutex_lock(&lcd->lock);
	s6e3fc5_sdc_read_init_info(lcd);
	smcdsd_panel_set_brightness(lcd, 1);
	mutex_unlock(&lcd->lock);
	smcdsd_commit_lock(lcd, false);

	lcd->fac_info = lcd->fac_done = IS_ENABLED(CONFIG_SEC_FACTORY) ? 1 : 0;

	dev_info(&lcd->ld->dev, "- %s\n", __func__);

	return 0;
}

static ssize_t lcd_type_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct lcd_info *lcd = dev_get_drvdata(dev);

	sprintf(buf, "%s_%02X%02X%02X\n", LCD_TYPE_VENDOR, lcd->id[0], lcd->id[1], lcd->id[2]);

	return strlen(buf);
}

static ssize_t window_type_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct lcd_info *lcd = dev_get_drvdata(dev);

	sprintf(buf, "%02x %02x %02x\n", lcd->id[0], lcd->id[1], lcd->id[2]);

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

	if (lcd->lux == value)
		return size;

	mutex_lock(&lcd->lock);
	lcd->lux = value;
	mutex_unlock(&lcd->lock);

#if defined(CONFIG_SMCDSD_MDNIE)
	attr_store_for_each(lcd->mdnie_class, attr->attr.name, buf, size);
#endif

	return size;
}

static void panel_conn_uevent(struct lcd_info *lcd)
{
	char *uevent_conn_str[3] = {"CONNECTOR_NAME=UB_CONNECT", "CONNECTOR_TYPE=HIGH_LEVEL", NULL};

	if (!lcd->fac_info)
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

	dev_info(&lcd->ld->dev, "%s: %u->%u\n", __func__, lcd->conn_det_enable, value);

	if (lcd->conn_det_enable == value)
		return size;

	mutex_lock(&lcd->lock);
	lcd->conn_det_enable = value;
	mutex_unlock(&lcd->lock);

	dev_info(&lcd->ld->dev, "%s: %s\n", __func__, gpio_active ? "disconnected" : "connected");
	if (lcd->conn_det_enable && gpio_active)
		panel_conn_uevent(lcd);

	return size;
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
	unsigned int value;
	int rc;

	rc = kstrtouint(buf, 0, &value);
	if (rc < 0)
		return rc;

	dev_info(&lcd->ld->dev, "%s: %u->%u\n", __func__, lcd->adaptive_control, value);

	if (clamp_val(value, 0, ARRAY_SIZE(acl_table) - 1) != value)
		return size;

	if (lcd->adaptive_control == value)
		return size;

	smcdsd_commit_lock(lcd, true);
	mutex_lock(&lcd->lock);
	lcd->adaptive_control = value;

	if (IS_NORMAL_MODE(lcd))
		smcdsd_panel_set_brightness(lcd, 1);

	mutex_unlock(&lcd->lock);
	smcdsd_commit_lock(lcd, false);

	return size;
}

static ssize_t irc_mode_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct lcd_info *lcd = dev_get_drvdata(dev);

	sprintf(buf, "%u\n", lcd->irc_mode);

	return strlen(buf);
}

static ssize_t irc_mode_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t size)
{
	struct lcd_info *lcd = dev_get_drvdata(dev);
	unsigned int value;
	int rc;

	rc = kstrtouint(buf, 0, &value);
	if (rc < 0)
		return rc;

	dev_info(&lcd->ld->dev, "%s: %u->%u\n", __func__, lcd->irc_mode, value);

	if (clamp_val(value, 0, ARRAY_SIZE(irc_table) - 1) != value)
		return size;

	if (lcd->irc_mode == value)
		return size;

	smcdsd_commit_lock(lcd, true);
	mutex_lock(&lcd->lock);
	lcd->irc_mode = value;

	if (IS_NORMAL_MODE(lcd))
		smcdsd_panel_set_brightness(lcd, 1);

	mutex_unlock(&lcd->lock);
	smcdsd_commit_lock(lcd, false);

	return size;
}

static ssize_t temperature_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	char temp[] = "-16, -15, 0, 1\n";

	strcat(buf, temp);
	return strlen(buf);
}

static ssize_t temperature_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t size)
{
	struct lcd_info *lcd = dev_get_drvdata(dev);
	int value;
	int rc;

	rc = kstrtoint(buf, 0, &value);
	if (rc < 0)
		return rc;

	dev_info(&lcd->ld->dev, "%s: %d->%d\n", __func__, lcd->temperature, value);

	if (clamp_val(value, -16, NORMAL_TEMPERATURE) != value)
		return size;

	smcdsd_commit_lock(lcd, true);
	mutex_lock(&lcd->lock);
	lcd->temperature = value;

	if (IS_NORMAL_MODE(lcd))
		smcdsd_panel_set_brightness(lcd, 1);

	mutex_unlock(&lcd->lock);
	smcdsd_commit_lock(lcd, false);

	return size;
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

	sprintf(buf, "%02X%02X%02X%02X%02X\n",
		lcd->code[0], lcd->code[1], lcd->code[2], lcd->code[3], lcd->code[4]);

	return strlen(buf);
}

static ssize_t cell_id_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct lcd_info *lcd = dev_get_drvdata(dev);

	sprintf(buf, "%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X\n",
		lcd->date[0], lcd->date[1], lcd->date[2], lcd->date[3], lcd->date[4], lcd->date[5], lcd->date[6],
		lcd->coordinate[0], lcd->coordinate[1], lcd->coordinate[2], lcd->coordinate[3]);

	return strlen(buf);
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
	site = get_shift_width(m_info[0], 4, 4);
	rework = get_shift_width(m_info[0], 0, 4);
	poc = get_shift_width(m_info[1], 0, 4);
	seq_printf(&m, "%d%d%d%02x%02x", site, rework, poc, m_info[2], m_info[3]);

	for (i = LDI_LEN_MANUFACTURE_INFO; i < LDI_LEN_MANUFACTURE_INFO + LDI_LEN_MANUFACTURE_INFO_CELL_ID; i++) {
		if (!isdigit(m_info[i]) && !isupper(m_info[i])) {
			dev_info(&lcd->ld->dev, "%s invalid, %dth : %x\n", __func__, i, m_info[i]);
			invalid = 1;
			break;
		}
	}
	for (i = LDI_LEN_MANUFACTURE_INFO; !invalid && i < LDI_LEN_MANUFACTURE_INFO + LDI_LEN_MANUFACTURE_INFO_CELL_ID; i++)
		seq_printf(&m, "%c", m_info[i]);

	seq_puts(&m, "\n");

	return strlen(buf);
}

static ssize_t ccd_state_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct lcd_info *lcd = dev_get_drvdata(dev);
	struct mipi_dsi_msg *dsi_msg;
	int i;
	int valid = 0;
	int len = 0;
	u8 result_buf[MAX_RX_READ_NUM] = {0, };
	struct abd_protect *abd = &lcd->pdata->abd;

	dev_info(&lcd->ld->dev, "%s: state(%d)\n", __func__, IS_NORMAL_MODE(lcd));

	if (!IS_NORMAL_MODE(lcd))
		return 0;

	smcdsd_commit_lock(lcd, true);
	mutex_lock(&lcd->lock);

	smcdsd_abd_enable(abd, 0);

	send_msg_segment(MSG_S6E3FC5_SDC_CCD, ARRAY_SIZE(MSG_S6E3FC5_SDC_CCD));
	for_each_rx_msg_in_segment(i, MSG_S6E3FC5_SDC_CCD, ARRAY_SIZE(MSG_S6E3FC5_SDC_CCD), dsi_msg) {
		dbg_info("%2x: %*ph\n", ((u8 *)dsi_msg->tx_buf)[0], dsi_msg->rx_len, (u8 *)dsi_msg->rx_buf);
		memcpy(&result_buf[len], dsi_msg->rx_buf, dsi_msg->rx_len);
		len += dsi_msg->rx_len;
	}

	for (i = 0; i < ARRAY_SIZE(fac_check_list); i++) {
		if (fac_check_list[i].name != attr->attr.name)
			continue;

		valid += (get_merged_value(result_buf, len) ^ fac_check_list[i].invert) ? 0 : 1;
	}

	sprintf(buf, "%d\n", valid ? 1 : 0);	/* or */

	dbg_info("len(%d) buf(%*ph) merge(%llx) valid(%d)\n", len, len, result_buf, get_merged_value(result_buf, len), valid);

	for_each_rx_msg_in_segment(i, MSG_S6E3FC5_SDC_CCD, ARRAY_SIZE(MSG_S6E3FC5_SDC_CCD), dsi_msg) {
		memset(dsi_msg->rx_buf, 0, dsi_msg->rx_len);
	}

	smcdsd_abd_enable(abd, 1);

	mutex_unlock(&lcd->lock);
	smcdsd_commit_lock(lcd, false);

	return strlen(buf);
}

static ssize_t dsc_crc_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct lcd_info *lcd = dev_get_drvdata(dev);
	struct mipi_dsi_msg *dsi_msg;
	int i;
	int valid = 0;
	int len = 0;
	u8 result_buf[MAX_RX_READ_NUM] = {0, };
	struct abd_protect *abd = &lcd->pdata->abd;

	dev_info(&lcd->ld->dev, "%s: state(%d) dsc_crc_state(%d)%s\n", __func__, IS_NORMAL_MODE(lcd),
		lcd->dsc_crc_state, lcd->dsc_crc_state ? "" : "invalid");

	if (!IS_NORMAL_MODE(lcd)) {
		smcdsd_abd_save_str(abd, "dsc_crc_show LCD is not on state");
		return 0;
	}

	smcdsd_commit_lock(lcd, true);
	mutex_lock(&lcd->lock);

	for_each_rx_msg_in_segment(i, MSG_S6E3FC5_SDC_CRC, ARRAY_SIZE(MSG_S6E3FC5_SDC_CRC), dsi_msg) {
		dbg_info("%2x: %*ph\n", ((u8 *)dsi_msg->tx_buf)[0], dsi_msg->rx_len, (u8 *)dsi_msg->rx_buf);
		memcpy(&result_buf[len], dsi_msg->rx_buf, dsi_msg->rx_len);
		len += dsi_msg->rx_len;
	}

	for (i = 0; i < ARRAY_SIZE(fac_check_list); i++) {
		if (fac_check_list[i].name != attr->attr.name)
			continue;

		valid += (get_merged_value(result_buf, len) ^ fac_check_list[i].invert) ? 0 : 1;
	}

	sprintf(buf, "%d %*ph\n", valid ? 1 : -1, len, result_buf);	/* and */

	dbg_info("len(%d) buf(%*ph) merge(%llx) valid(%d)\n", len, len, result_buf, get_merged_value(result_buf, len), valid);

	if (!valid && !lcd->dsc_crc_state)
		smcdsd_abd_save_str(abd, "invalid because of unblanced dsc_crc_state");

	for_each_rx_msg_in_segment(i, MSG_S6E3FC5_SDC_CRC, ARRAY_SIZE(MSG_S6E3FC5_SDC_CRC), dsi_msg) {
		memset(dsi_msg->rx_buf, 0, dsi_msg->rx_len);
	}

	lcd->dsc_crc_state = 0;

	mutex_unlock(&lcd->lock);
	smcdsd_commit_lock(lcd, false);

	return strlen(buf);
}

static ssize_t dsc_crc_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t size)
{
	struct lcd_info *lcd = dev_get_drvdata(dev);
	int value;
	int rc;
	struct abd_protect *abd = &lcd->pdata->abd;

	rc = kstrtoint(buf, 0, &value);
	if (rc < 0)
		return rc;

	dev_info(&lcd->ld->dev, "%s: state(%d) dsc_crc_state(%d->%d)%s\n", __func__, IS_NORMAL_MODE(lcd),
		lcd->dsc_crc_state, value, (lcd->dsc_crc_state != value) ? "" : "invalid");

	if (!IS_NORMAL_MODE(lcd)) {
		smcdsd_abd_save_str(abd, "dsc_crc_store LCD is not on state");
		return size;
	}

	smcdsd_commit_lock(lcd, true);
	mutex_lock(&lcd->lock);

	msleep(40);
	if (lcd->fac_info)
		send_msg_segment(MSG_S6E3FC5_SDC_CRC, ARRAY_SIZE(MSG_S6E3FC5_SDC_CRC));
	else
		send_msg_segment(MSG_S6E3FC5_SDC_CRC_USER, ARRAY_SIZE(MSG_S6E3FC5_SDC_CRC_USER));

	lcd->dsc_crc_state = 1;

	mutex_unlock(&lcd->lock);
	smcdsd_commit_lock(lcd, false);

	return size;
}

static ssize_t ecc_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct lcd_info *lcd = dev_get_drvdata(dev);
	struct mipi_dsi_msg *dsi_msg;
	int i;
	int valid = 0;
	int len = 0;
	u8 result_buf[MAX_RX_READ_NUM] = {0, };

	dev_info(&lcd->ld->dev, "%s: state(%d)\n", __func__, IS_NORMAL_MODE(lcd));

	if (!IS_NORMAL_MODE(lcd))
		return 0;

	smcdsd_commit_lock(lcd, true);
	mutex_lock(&lcd->lock);

	send_msg_segment(MSG_S6E3FC5_SDC_ECC, ARRAY_SIZE(MSG_S6E3FC5_SDC_ECC));
	for_each_rx_msg_in_segment(i, MSG_S6E3FC5_SDC_ECC, ARRAY_SIZE(MSG_S6E3FC5_SDC_ECC), dsi_msg) {
		dbg_info("%2x: %*ph\n", ((u8 *)dsi_msg->tx_buf)[0], dsi_msg->rx_len, (u8 *)dsi_msg->rx_buf);
		memcpy(&result_buf[len], dsi_msg->rx_buf, dsi_msg->rx_len);
		len += dsi_msg->rx_len;
	}

	for (i = 0; i < ARRAY_SIZE(fac_check_list); i++) {
		if (fac_check_list[i].name != attr->attr.name)
			continue;

		valid += (get_merged_value(result_buf, len) ^ fac_check_list[i].invert) ? 0 : 1;
	}

	sprintf(buf, "%d\n", valid ? 1 : 0);	/* or */

	dbg_info("len(%d) buf(%*ph) merge(%llx) valid(%d)\n", len, len, result_buf, get_merged_value(result_buf, len), valid);

	for_each_rx_msg_in_segment(i, MSG_S6E3FC5_SDC_ECC, ARRAY_SIZE(MSG_S6E3FC5_SDC_ECC), dsi_msg) {
		memset(dsi_msg->rx_buf, 0, dsi_msg->rx_len);
	}

	mutex_unlock(&lcd->lock);
	smcdsd_commit_lock(lcd, false);

	return strlen(buf);
}

static DEVICE_ATTR_RO(ccd_state);
static DEVICE_ATTR_RW(dsc_crc);
static DEVICE_ATTR_RO(ecc);

#if defined(CONFIG_SMCDSD_DOZE)
static ssize_t alpm_fac_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t size)
{
	struct lcd_info *lcd = dev_get_drvdata(dev);
	struct mipi_dsi_lcd_common *pdata = lcd->pdata;
	struct fb_info *fbinfo = registered_fb[0];
	struct abd_protect *abd = &pdata->abd;
	union lpm_info lpm = {0, };
	unsigned int value = 0;
	int ret;

	ret = kstrtouint(buf, 0, &value);
	if (ret < 0)
		return ret;

	dev_info(&lcd->ld->dev, "%s: %06x, lpm: %06x(%06x) state(%u) doze(%u)\n", __func__,
		value, lcd->current_alpm.value, lcd->alpm.value, lcd->state, lcd->doze_state);

	if (lcd->state == 0) {
		dev_info(&lcd->ld->dev, "%s: panel state is %d\n", __func__, lcd->state);
		return -EINVAL;
	}

	if (!lock_fb_info(fbinfo)) {	/* todo */
		dev_info(&lcd->ld->dev, "%s: fblock is failed\n", __func__);
		return -EINVAL;
	}

	lpm.ver = get_shift_width(value, 16, 8);
	lpm.mode = get_shift_width(value, 0, 8);

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

	lpm.index = (lpm.ver && lpm.mode) ? lpm_init_table[lcd->bd->props.brightness] : lpm_old_table[lpm.mode];

	mutex_lock(&lcd->lock);
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

		smcdsd_commit_lock(lcd, true);
		mutex_lock(&lcd->lock);
		call_drv_ops(lcd->pdata, doze, 0);
		mutex_unlock(&lcd->lock);
		smcdsd_commit_lock(lcd, false);

		break;
	case ALPM_ON_LOW:
	case HLPM_ON_LOW:
	case ALPM_ON_HIGH:
	case HLPM_ON_HIGH:
		smcdsd_commit_lock(lcd, true);
		mutex_lock(&lcd->lock);
		if (lcd->prev_brightness == 0)
			lcd->prev_brightness = lcd->bd->props.brightness;
		lcd->bd->props.brightness = (value <= HLPM_ON_LOW) ? 0 : UI_MAX_BRIGHTNESS;

		call_drv_ops(lcd->pdata, doze, 1);
		mutex_unlock(&lcd->lock);
		smcdsd_commit_lock(lcd, false);
		break;
	}
	smcdsd_abd_enable(abd, 1);

	unlock_fb_info(fbinfo);

	return size;
}

static ssize_t alpm_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t size)
{
	struct lcd_info *lcd = dev_get_drvdata(dev);
	union lpm_info lpm = {0, };
	unsigned int value = 0;
	int ret;

	if (lcd->fac_info) {
		dev_info(&lcd->ld->dev, "%s: fac_info(%d)\n", __func__, lcd->fac_info);
		return alpm_fac_store(dev, attr, buf, size);
	}

	ret = kstrtouint(buf, 0, &value);
	if (ret < 0)
		return ret;

	dev_info(&lcd->ld->dev, "%s: %06x, lpm: %06x(%06x) state(%u) doze(%u)\n", __func__,
		value, lcd->current_alpm.value, lcd->alpm.value, lcd->state, lcd->doze_state);

	lpm.ver = get_shift_width(value, 16, 8);
	lpm.mode = get_shift_width(value, 0, 8);

	if (!lpm.ver && lpm.mode >= ALPM_MODE_MAX) {
		dev_info(&lcd->ld->dev, "%s: undefined lpm value: %x\n", __func__, value);
		return -EINVAL;
	}

	if (lpm.ver && lpm.mode >= AOD_MODE_MAX) {
		dev_info(&lcd->ld->dev, "%s: undefined lpm value: %x\n", __func__, value);
		return -EINVAL;
	}

	lpm.index = (lpm.ver && lpm.mode) ? lpm_init_table[lcd->bd->props.brightness] : lpm_old_table[lpm.mode];

	if (lcd->alpm.value == lpm.value && lcd->current_alpm.value == lpm.value) {
		dev_info(&lcd->ld->dev, "%s: unchanged lpm value: %x\n", __func__, lpm.value);
		return size;
	}

	smcdsd_commit_lock(lcd, true);
	mutex_lock(&lcd->lock);
	lcd->alpm = lpm;

	if (lcd->current_alpm.index && lcd->state)
		call_drv_ops(lcd->pdata, doze, 1);

	mutex_unlock(&lcd->lock);
	smcdsd_commit_lock(lcd, false);

	return size;
}

static ssize_t alpm_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct lcd_info *lcd = dev_get_drvdata(dev);

	sprintf(buf, "%06x\n", lcd->current_alpm.value);

	return strlen(buf);
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
	if (buf[0] == 'C' || buf[0] == 'c') {
		clear_dpui_log(DPUI_LOG_LEVEL_INFO, DPUI_TYPE_PANEL);
	}

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
	if (buf[0] == 'C' || buf[0] == 'c') {
		clear_dpui_log(DPUI_LOG_LEVEL_DEBUG, DPUI_TYPE_PANEL);
	}

	return size;
}

static DEVICE_ATTR(dpui, 0660, dpui_show, dpui_store);
static DEVICE_ATTR(dpui_dbg, 0660, dpui_dbg_show, dpui_dbg_store);

static ssize_t dpci_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	int ret;

	update_dpui_log(DPUI_LOG_LEVEL_INFO, DPUI_TYPE_CTRL);
	ret = get_dpui_log(buf, DPUI_LOG_LEVEL_INFO, DPUI_TYPE_CTRL);
	if (ret < 0) {
		pr_err("%s failed to get log %d\n", __func__, ret);
		return ret;
	}
	pr_info("%s\n", buf);
	return ret;
}

static ssize_t dpci_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t size)
{
	if (buf[0] == 'C' || buf[0] == 'c') {
		clear_dpui_log(DPUI_LOG_LEVEL_INFO, DPUI_TYPE_CTRL);
	}

	return size;
}

/*
 * [DEV ONLY]
 * HW PARAM LOGGING SYSFS NODE
 */
static ssize_t dpci_dbg_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	int ret;

	update_dpui_log(DPUI_LOG_LEVEL_DEBUG, DPUI_TYPE_CTRL);
	ret = get_dpui_log(buf, DPUI_LOG_LEVEL_DEBUG, DPUI_TYPE_CTRL);
	if (ret < 0) {
		pr_err("%s failed to get log %d\n", __func__, ret);
		return ret;
	}

	pr_info("%s\n", buf);
	return ret;
}

static ssize_t dpci_dbg_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t size)
{
	if (buf[0] == 'C' || buf[0] == 'c') {
		clear_dpui_log(DPUI_LOG_LEVEL_DEBUG, DPUI_TYPE_CTRL);
	}

	return size;
}

static DEVICE_ATTR(dpci, 0660, dpci_show, dpci_store);
static DEVICE_ATTR(dpci_dbg, 0660, dpci_dbg_show, dpci_dbg_store);
#endif

#if defined(CONFIG_SMCDSD_MASK_LAYER)
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

	dev_info(&lcd->ld->dev, "%s: %u->%u\n", __func__, lcd->mask_brightness, value);

	if (value > lcd->bd->props.max_brightness)
		return -EINVAL;

	mutex_lock(&lcd->lock);
	lcd->mask_brightness = value;
	mutex_unlock(&lcd->lock);

	return size;
}

static ssize_t actual_mask_brightness_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct lcd_info *lcd = dev_get_drvdata(dev);

	sprintf(buf, "%d\n", lcd->actual_mask_brightness);

	return strlen(buf);
}

static DEVICE_ATTR_RO(actual_mask_brightness);
static DEVICE_ATTR_RW(mask_brightness);
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

	dev_info(&lcd->ld->dev, "%s: %04u->%04u\n", __func__, lcd->df_index_req, value);

	smcdsd_commit_lock(lcd, true);
	mutex_lock(&lcd->lock);
	//if (lcd->state)
	//	s6e3fc5_sdc_change_ffc(lcd, FFC_OFF);

	df_idx = value;

	if (clamp_val(df_idx, DYNAMIC_MIPI_INDEX_0, DYNAMIC_MIPI_INDEX_MAX - 1) != df_idx)
		df_idx = DYNAMIC_MIPI_INDEX_0;

	smcdsd_panel_dsi_clk_change(lcd->pdata, df_idx);

	lcd->df_index_req = df_idx;
	mutex_unlock(&lcd->lock);
	smcdsd_commit_lock(lcd, false);

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

	dev_info(&lcd->ld->dev, "%s: rat(%u) band(%u) channel(%u)\n", __func__, rat, band, channel);

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

static DEVICE_ATTR_RW(dynamic_mipi);
static DEVICE_ATTR_RW(virtual_dynamic_mipi);
#endif

static void panel_conn_register(struct lcd_info *lcd)
{
	struct mipi_dsi_lcd_common *pdata = lcd->pdata;
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

	if (lcd->fac_info)
		smcdsd_abd_con_register(abd);
}

static ssize_t fac_info_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct lcd_info *lcd = dev_get_drvdata(dev);

	sprintf(buf, "%u\n", lcd->fac_info);

	return strlen(buf);
}

static const struct attribute_group lcd_sysfs_attr_group;

static ssize_t fac_info_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t size)
{
	struct lcd_info *lcd = dev_get_drvdata(dev);
	struct mipi_dsi_lcd_common *pdata = lcd->pdata;
	struct abd_protect *abd = &pdata->abd;
	unsigned int value;
	int rc;

	rc = kstrtouint(buf, 0, &value);
	if (rc < 0)
		return rc;

	dev_info(&lcd->ld->dev, "%s: %u->%u\n", __func__, lcd->fac_info, value);

	mutex_lock(&lcd->lock);
	lcd->fac_info = value;
	mutex_unlock(&lcd->lock);

	if (lcd->fac_done)
		return size;

	lcd->fac_done = 1;
	smcdsd_abd_con_register(abd);
	sysfs_update_group(&lcd->ld->dev.kobj, &lcd_sysfs_attr_group);

	return size;
}

#if defined(CONFIG_SMCDSD_MDNIE)
static ssize_t mdnie_inactive_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct lcd_info *lcd = dev_get_drvdata(dev);

	sprintf(buf, "%u\n", lcd->mdnie_inactive);

	return strlen(buf);
}

static ssize_t mdnie_inactive_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t size)
{
	struct lcd_info *lcd = dev_get_drvdata(dev);
	unsigned int value;
	int rc;

	rc = kstrtouint(buf, 0, &value);
	if (rc < 0)
		return rc;

	dev_info(&lcd->ld->dev, "%s: %u->%u\n", __func__, lcd->mdnie_inactive, value);

	mutex_lock(&lcd->lock);
	lcd->mdnie_inactive = value;
	mutex_unlock(&lcd->lock);

	return size;
}
#endif

static DEVICE_ATTR_RW(fac_info);
static DEVICE_ATTR_RO(lcd_type);
static DEVICE_ATTR_RO(window_type);
static DEVICE_ATTR_RW(lux);
static DEVICE_ATTR_RW(conn_det);
static DEVICE_ATTR_RW(adaptive_control);
static DEVICE_ATTR_RW(irc_mode);
static DEVICE_ATTR_RW(temperature);
static DEVICE_ATTR_RO(manufacture_date);
static DEVICE_ATTR_RO(manufacture_code);
static DEVICE_ATTR_RO(cell_id);
static DEVICE_ATTR_RO(octa_id);

static DEVICE_ATTR(SVC_OCTA, 0444, cell_id_show, NULL);
static DEVICE_ATTR(SVC_OCTA_DDI_CHIPID, 0444, manufacture_code_show, NULL);

#if defined(CONFIG_SMCDSD_MDNIE)
static DEVICE_ATTR_RW(mdnie_inactive);
#endif

#if defined(CONFIG_SMCDSD_DOZE)
static DEVICE_ATTR_RW(alpm);
#endif

static struct attribute *lcd_sysfs_attributes[] = {
	&dev_attr_lcd_type.attr,
	&dev_attr_window_type.attr,
	&dev_attr_lux.attr,
	&dev_attr_conn_det.attr,
	&dev_attr_adaptive_control.attr,
	&dev_attr_irc_mode.attr,
	&dev_attr_temperature.attr,
	&dev_attr_manufacture_date.attr,
	&dev_attr_manufacture_code.attr,
	&dev_attr_cell_id.attr,
	&dev_attr_octa_id.attr,
#if defined(CONFIG_SMCDSD_MDNIE)
	&dev_attr_mdnie_inactive.attr,
#endif
#if defined(CONFIG_SMCDSD_DOZE)
	&dev_attr_alpm.attr,
#endif
#if defined(CONFIG_SMCDSD_DYNAMIC_MIPI)
	&dev_attr_dynamic_mipi.attr,
	&dev_attr_virtual_dynamic_mipi.attr,
#endif
#if defined(CONFIG_SMCDSD_DPUI)
	&dev_attr_dpui.attr,
	&dev_attr_dpui_dbg.attr,
	&dev_attr_dpci.attr,
	&dev_attr_dpci_dbg.attr,
#endif
#if defined(CONFIG_SMCDSD_MASK_LAYER)
	&dev_attr_mask_brightness.attr,
	&dev_attr_actual_mask_brightness.attr,
#endif
	&dev_attr_ccd_state.attr,
	&dev_attr_ecc.attr,
	&dev_attr_dsc_crc.attr,
	NULL,
};

static umode_t lcd_sysfs_is_visible(struct kobject *kobj,
	struct attribute *attr, int index)
{
	struct device *dev = container_of(kobj, struct device, kobj);
	struct lcd_info *lcd = dev_get_drvdata(dev);
	umode_t mode = attr->mode;

	if (!lcd->fac_info && !lcd->fac_done) {
		if (attr == &dev_attr_ccd_state.attr)
			mode = 0;

		if (attr == &dev_attr_ecc.attr)
			mode = 0;
	}

	return mode;
}

static const struct attribute_group lcd_sysfs_attr_group = {
	.is_visible = lcd_sysfs_is_visible,
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

	ret = device_create_file(dev, &dev_attr_SVC_OCTA);
	ret = device_create_file(dev, &dev_attr_SVC_OCTA_DDI_CHIPID);

	if (kn)
		kernfs_put(kn);
}

static void lcd_init_sysfs(struct lcd_info *lcd)
{
	int ret = 0;
	unsigned int i;

	ret = sysfs_create_group(&lcd->ld->dev.kobj, &lcd_sysfs_attr_group);
	if (ret < 0)
		dev_info(&lcd->ld->dev, "sysfs_create_group fail\n");

	ret = sysfs_create_file(&lcd->bd->dev.kobj, &dev_attr_fac_info.attr);
	if (ret < 0)
		dev_info(&lcd->ld->dev, "sysfs_create_file fail\n");

	lcd_init_svc(lcd);

	for (i = 0; i < (u16)ARRAY_SIZE(MSG_S6E3FC5_SDC_INIT); i++)
		init_debugfs_param("lcd_init", (void *)MSG_S6E3FC5_SDC_INIT[i].dsi_msg.tx_buf, U8_MAX, MSG_S6E3FC5_SDC_INIT[i].dsi_msg.tx_len, 0);
	for (i = 0; i < (u16)ARRAY_SIZE(MSG_S6E3FC5_SDC_EXIT); i++)
		init_debugfs_param("lcd_exit", (void *)MSG_S6E3FC5_SDC_EXIT[i].dsi_msg.tx_buf, U8_MAX, MSG_S6E3FC5_SDC_EXIT[i].dsi_msg.tx_len, 0);

	init_debugfs_param("gamma_offset", (void *)gamma_offset, S32_MAX, sizeof(gamma_offset)  / sizeof(int), GS_MAX * CI_MAX);

	init_debugfs_param("gamma_reg_rx", (void *)&RX_BUF_67[167], U8_MAX, GAMMA_LEGNTH, 0);
	init_debugfs_param("gamma_set_rx", (void *)gamma_set_rx, U32_MAX, sizeof(gamma_set_rx) / sizeof(u32), 0);

	init_debugfs_param("gamma_set_tx", (void *)gamma_set_tx, S32_MAX, sizeof(gamma_set_tx) / sizeof(s32), GS_MAX * CI_MAX);

	init_debugfs_param("gamma_reg_tx", (void *)S6E3FC5_SDC_GAMMA_NDARRAY_DATA, U8_MAX, (EXTEND_BRIGHTNESS + 1) * (1 + GAMMA_LEGNTH), 1 + GAMMA_LEGNTH);
}

static int smcdsd_panel_framedone_notify(struct platform_device *p)
{
#if defined(CONFIG_SMCDSD_DYNAMIC_MIPI)
	smcdsd_panel_framedone_dynamic_mipi(p);
#endif
	return 0;
}

static int smcdsd_panel_power(struct platform_device *p, unsigned int on)
{
	struct lcd_info *lcd = get_lcd_info(p);

	dev_info(&lcd->ld->dev, "+ %s: on(%u)\n", __func__, on);

	if (on)
		run_list(&p->dev, "panel_power_enable");
	else
		run_list(&p->dev, "panel_power_disable");

	dev_info(&lcd->ld->dev, "- %s: on(%u) state(%u) connected(%u)\n", __func__, on, lcd->state, lcd->connected);

	return 0;
}

static int smcdsd_panel_reset(struct platform_device *p, unsigned int on)
{
	struct lcd_info *lcd = get_lcd_info(p);

	dev_info(&lcd->ld->dev, "+ %s: on(%u)\n", __func__, on);

	if (on)
		run_list(&p->dev, "panel_reset_enable");
	else
		run_list(&p->dev, "panel_reset_disable");

	dev_info(&lcd->ld->dev, "- %s: on(%u) state(%u) connected(%u)\n", __func__, on, lcd->state, lcd->connected);

	return 0;
}

static int smcdsd_panel_init(struct platform_device *p)
{
	struct lcd_info *lcd = get_lcd_info(p);

	dev_info(&lcd->ld->dev, "+ %s: state(%u)\n", __func__, lcd->state);

	s6e3fc5_sdc_init(lcd);

	dev_info(&lcd->ld->dev, "- %s: state(%u) connected(%u)\n", __func__, lcd->state, lcd->connected);

	return 0;
}

static int smcdsd_panel_enable(struct platform_device *p)
{
	struct lcd_info *lcd = get_lcd_info(p);

	dev_info(&lcd->ld->dev, "%s: state(%u) doze(%u)\n", __func__, lcd->state, lcd->doze_state);

	smcdsd_panel_set_brightness(lcd, 1);

#if defined(CONFIG_SMCDSD_MDNIE)
	class_for_each_device(lcd->mdnie_class, NULL, __mdnie_send, mdnie_force_update);
#endif

	//move to .set_dispon_cmdq s6e3fc5_sdc_displayon(lcd);	/* OFF,DOZE_SUSPEND-> ON, DOZE */

	return 0;
}

static int smcdsd_panel_displayon_late(struct platform_device *p)
{
	struct lcd_info *lcd = get_lcd_info(p);

	dev_info(&lcd->ld->dev, "%s: state(%u) doze(%u)\n", __func__, lcd->state, lcd->doze_state);

	s6e3fc5_sdc_displayon(lcd);

	return 0;
}

static int smcdsd_panel_exit(struct platform_device *p)
{
	struct lcd_info *lcd = get_lcd_info(p);

	dev_info(&lcd->ld->dev, "+ %s: state(%u)\n", __func__, lcd->state);

	s6e3fc5_sdc_exit(lcd);

	dev_info(&lcd->ld->dev, "- %s: state(%u) connected(%u)\n", __func__, lcd->state, lcd->connected);

	return 0;
}

static int smcdsd_panel_set_mode(struct platform_device *p, struct drm_display_mode *m, unsigned int dst_mode, int stage)
{
	struct lcd_info *lcd = get_lcd_info(p);

	if (stage == BEFORE_DSI_POWERDOWN)	/* todo */
		return 0;

	dev_info(&lcd->ld->dev, "%s: refresh(%3u) mode(%d)(%s) state(%u/%u)\n", __func__, m->vrefresh, dst_mode, m->name, lcd->state, lcd->doze_state);

	if (m->vrefresh != 60 && m->vrefresh != 120)	/* todo */
		return 0;

	lcd->vrefresh = m->vrefresh;

	lcd->disp_mode_idx = dst_mode;

	if (IS_NORMAL_MODE(lcd) == 0)
		return 0;

	return smcdsd_panel_set_brightness(lcd, 1);
}

#if defined(CONFIG_SMCDSD_DOZE)
static int smcdsd_panel_doze(struct platform_device *p, unsigned int on)
{
	struct lcd_info *lcd = get_lcd_info(p);

	dev_info(&lcd->ld->dev, "+ %s: on(%u) state(%u) doze(%u)\n", __func__, on, lcd->state, lcd->doze_state);

	if (on) {
		s6e3fc5_sdc_doze_init(lcd);
		if (!lcd->doze_state)
			smcdsd_dsi_tx_package(lcd, GET_PACKAGE(&PACKAGE_LIST[MSG_IDX_BASE][GET_ENUM_WITH_NAME(MSG_S6E3FC5_SDC_AOD_INIT)]));

		lcd->doze_state = 1;
	} else {
		s6e3fc5_sdc_doze_exit(lcd);

		lcd->doze_state = 0;

		smcdsd_panel_set_brightness(lcd, 1);
	}

	dev_info(&lcd->ld->dev, "- %s: on(%u) state(%u) doze(%u) connected(%u)\n", __func__, on, lcd->state, lcd->doze_state, lcd->connected);

	return 0;
}
#endif

static int smcdsd_panel_setup(struct platform_device *p)
{
	struct mipi_dsi_lcd_common *plcd = get_lcd_common_info(p);
	int i;

#if defined(CONFIG_MTK_ROUND_CORNER_SUPPORT) && !defined(CONFIG_SEC_FACTORY)
	for (i = 0; i < LCD_CONFIG_MAX; i++) {
		if (!plcd->config[i].drm.vrefresh)
			continue;

		plcd->config[i].ext.round_corner_en = 1;
		plcd->config[i].ext.corner_pattern_height = ROUND_CORNER_H_TOP;
		plcd->config[i].ext.corner_pattern_height_bot = ROUND_CORNER_H_BOT;
		plcd->config[i].ext.corner_pattern_tp_size = sizeof(top_rc_pattern);
		plcd->config[i].ext.corner_pattern_lt_addr = (void *)top_rc_pattern;
	}
#endif

	plcd->config[LCD_CONFIG_INDEX_60PHS].drm.type |= DRM_MODE_TYPE_PREFERRED;

	for (i = 0; i < LCD_CONFIG_MAX; i++) {
		if (!plcd->config[i].drm.vrefresh)
			continue;

		plcd->config[i].drm.vscan = 1;

		if (i == LCD_CONFIG_INDEX_60PHS)
			plcd->config[i].drm.vscan = 2;

		snprintf(plcd->config[i].drm.name, DRM_DISPLAY_MODE_LEN, "%dx%d@%d%shs",
			plcd->config[i].drm.hdisplay, plcd->config[i].drm.vdisplay,
			plcd->config[i].drm.vrefresh, (plcd->config[i].drm.vscan != 1) ? "p" : "");
	}

	return 0;
}

static int smcdsd_panel_probe(struct platform_device *p)
{
	struct device *dev = &p->dev;
	int ret = 0;
	struct lcd_info *lcd;

	lcd = kzalloc(sizeof(struct lcd_info), GFP_KERNEL);
	if (!lcd) {
		dbg_info("failed to allocate for lcd\n");
		ret = -ENOMEM;
		goto exit;
	}

	lcd->ld = lcd_device_register("panel", dev, lcd, NULL);
	if (IS_ERR(lcd->ld)) {
		dbg_info("failed to register lcd device\n");
		ret = PTR_ERR(lcd->ld);
		goto exit;
	}

	lcd->bd = backlight_device_register("panel", dev, lcd, &panel_backlight_ops, NULL);
	if (IS_ERR(lcd->bd)) {
		dbg_info("failed to register backlight device\n");
		ret = PTR_ERR(lcd->bd);
		goto exit;
	}

	mutex_init(&lcd->lock);

	lcd->pdata = get_lcd_common_info(p);

	set_lcd_info(p, lcd);

	s6e3fc5_sdc_probe(lcd);

	s6e3fc5_sdc_register_notifier(lcd);

	lcd_init_sysfs(lcd);

#if defined(CONFIG_SMCDSD_MDNIE)
	lcd->mdnie_class = mdnie_register(&lcd->ld->dev, lcd, (mdnie_w)mdnie_send, (mdnie_r)mdnie_read, lcd->coordinate_xy, &tune_info);
#endif

	dev_info(&lcd->ld->dev, "%s: %s: done\n", kbasename(__FILE__), __func__);
exit:
	return ret;
}

#if defined(CONFIG_SMCDSD_MASK_LAYER)
static int smcdsd_panel_hbm_set_lcm_cmdq(struct platform_device *p, bool en)
{
	struct lcd_info *lcd = get_lcd_info(p);
	int hbm_done = 0;

	lcd->mask_state = en;

	dev_info(&lcd->ld->dev, "%s: en(%d->%d) te_cnt(%d) refresh(%3u) %lu\n", __func__,
		lcd->hbm_en, en, lcd->te_cnt, lcd->vrefresh,
		lcd->hbm_time_stamp ? div_u64(local_clock() - lcd->hbm_time_stamp, NSEC_PER_USEC) : 0);

	lcd->hbm_time_stamp = local_clock();

	if (en == true) {
		if (lcd->te_cnt == 1 + 0) {
			smcdsd_dsi_tx_package(lcd, GET_PACKAGE(&PACKAGE_LIST[MSG_IDX_BASE][GET_ENUM_WITH_NAME(MSG_S6E3FC5_SDC_PREPARE_FINGER_PRINT)], !!(lcd->mask_state | lcd->hbm_en), lcd->brightness));
		} else if (lcd->te_cnt == 1 + 2) {
			smcdsd_panel_set_brightness(lcd, 1);
			hbm_done = lcd->vrefresh == 60 ? 1 : hbm_done;	/* 60: No Delay after command */
		} else if (lcd->te_cnt == 1 + 3) {
			hbm_done = lcd->vrefresh == 120 ? 1 : hbm_done;	/* 120: 1TE delay after command  */
		}
	} else if (en == false) {
		if (lcd->te_cnt == 1 + 0) {
			smcdsd_panel_set_brightness(lcd, 1);
			hbm_done = lcd->vrefresh == 60 ? 1 : hbm_done;	/* 60: No Delay after command  */
		} else if (lcd->te_cnt == 1 + 1) {
			hbm_done = lcd->vrefresh == 120 ? 1 : hbm_done;	/* 120: 1TE delay after command  */
		}
	}

	BUG_ON(lcd->te_cnt >= 10);

	/* if panel's wait state is changed(0->1) = no more TE(VSYNC) */
	lcd->hbm_wait = hbm_done;
	lcd->hbm_en = hbm_done ? en : lcd->hbm_en;
	lcd->te_cnt = hbm_done ? 0 : lcd->te_cnt + 1;

	return hbm_done;
}

static int smcdsd_panel_hbm_get_state(struct platform_device *p, bool *state)
{
	struct lcd_info *lcd = get_lcd_info(p);

	*state = lcd->hbm_en;

	dev_dbg(&lcd->ld->dev, "%s: en_state(%d)\n", __func__, *state);

	return 0;
}

static int smcdsd_panel_hbm_get_wait_state(struct platform_device *p, bool *wait)
{
	struct lcd_info *lcd = get_lcd_info(p);

	*wait = lcd->hbm_wait;

	dev_dbg(&lcd->ld->dev, "%s: wait_state(%d)\n", __func__, *wait);

	return 0;
}

static int smcdsd_panel_hbm_set_wait_state(struct platform_device *p, bool wait)
{
	struct lcd_info *lcd = get_lcd_info(p);

	bool old = lcd->hbm_wait;

	lcd->hbm_wait = wait;

	//mask off and right after 1st cmd(including brightness) should have delay
	if (!lcd->hbm_en && old && lcd->vrefresh == 60) {
		lcd->hbm_interval = 1;
		run_timer_from(NULL, "hbm_exit", "hbm_interval", 34);
	}

	dev_info(&lcd->ld->dev, "%s: wait_state(%d->%d) refresh(%3u) hbm_interval(%u) %lu\n", __func__,
		old, lcd->hbm_wait, lcd->vrefresh, lcd->hbm_interval,
		div_u64(local_clock() - lcd->hbm_time_stamp, NSEC_PER_USEC));

	lcd->actual_mask_brightness = lcd->mask_state ? lcd->mask_brightness : 0;

	sysfs_notify(&lcd->ld->dev.kobj, NULL, "actual_mask_brightness");

	lcd->hbm_time_stamp = 0;

	//todo: sysfs notify in this position is proper? or after return? check mtk_drm_crtc_hbm_wait and ddp_cmdq_cb

	return old;
}
#endif

struct mipi_dsi_lcd_driver s6e3fc5_sdc_mipi_lcd_driver = {
	.driver = {
		.name = "s6e3fc5_sdc",
	},
	.setup		= smcdsd_panel_setup,
	.probe		= smcdsd_panel_probe,
	.init		= smcdsd_panel_init,
	.exit		= smcdsd_panel_exit,
	.panel_reset	= smcdsd_panel_reset,
	.panel_power	= smcdsd_panel_power,
	.set_mode	= smcdsd_panel_set_mode,
#if defined(CONFIG_SMCDSD_DOZE)
	.doze		= smcdsd_panel_doze,
#endif
	.enable		= smcdsd_panel_enable,
	.framedone_notify = smcdsd_panel_framedone_notify,
	.displayon_late = smcdsd_panel_displayon_late,

#if defined(CONFIG_SMCDSD_MASK_LAYER)
	.hbm_set_lcm_cmdq = smcdsd_panel_hbm_set_lcm_cmdq,
	.hbm_get_state	= smcdsd_panel_hbm_get_state,
	.hbm_get_wait_state	= smcdsd_panel_hbm_get_wait_state,
	.hbm_set_wait_state	= smcdsd_panel_hbm_set_wait_state,
#endif
};
__XX_ADD_LCD_DRIVER(s6e3fc5_sdc_mipi_lcd_driver);

static int __init panel_late_init(void)
{
	struct lcd_info *lcd = NULL;
	struct mipi_dsi_lcd_common *pdata = NULL;
	struct platform_device *pdev = NULL;

	pdev = of_find_device_by_path("/panel");
	if (!pdev) {
		dbg_info("of_find_device_by_path fail\n");
		return 0;
	}

	pdata = platform_get_drvdata(pdev);
	if (!pdata) {
		dbg_info("platform_get_drvdata fail\n");
		return 0;
	}

	if (!pdata->drv) {
		dbg_info("lcd_driver invalid\n");
		return 0;
	}

	if (!pdata->drv->pdev) {
		dbg_info("lcd_driver pdev invalid\n");
		return 0;
	}

	if (pdata->drv != this_driver)
		return 0;

	lcd = get_lcd_info(pdata->drv->pdev);
	if (!lcd) {
		dbg_info("get_lcd_info invalid\n");
		return 0;
	}

	panel_conn_register(lcd);

	dev_info(&lcd->ld->dev, "%s: %s: done\n", kbasename(__FILE__), __func__);

	return 0;
}
late_initcall_sync(panel_late_init);

