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

#include "../smcdsd_board.h"
#include "../smcdsd_panel.h"
#include "smcdsd_notify.h"
#include "lcm_drv.h"

#if defined(CONFIG_SMCDSD_MDNIE)
#include "mdnie.h"
#if defined(CONFIG_SMCDSD_PANEL_A41) || defined(CONFIG_SMCDSD_PANEL_A41_JPN)
#include "ea8076_a41_mdnie.h"
#else
#include "ea8076_a31_mdnie.h"
#endif
#endif

#if defined(CONFIG_SMCDSD_PANEL_A41_JPN)
#include "ea8076_a41_jpn_param.h"
#elif defined(CONFIG_SMCDSD_PANEL_A41)
#include "ea8076_a41_param.h"
#else
#include "ea8076_a31_param.h"
#endif

#include "dd.h"
#include "timenval.h"

#if defined(CONFIG_SMCDSD_DPUI)
#include "dpui.h"

#define	DPUI_VENDOR_NAME	"SDC"
#define	DPUI_MODEL_NAME		"AMS638VL01"
#endif

#if defined(CONFIG_SMCDSD_DOZE)
#include "disp_helper.h"
#endif

#define dbg_info(fmt, ...)	pr_info(pr_fmt("%s: %3d: %s: " fmt), "lcd panel", __LINE__, __func__, ##__VA_ARGS__)

#define PANEL_STATE_SUSPENED	0
#define PANEL_STATE_RESUMED	1

#define LEVEL_IS_HBM(brightness)		(brightness > UI_MAX_BRIGHTNESS)

#define get_bit(value, shift, width)	((value >> shift) & (GENMASK(width - 1, 0)))


#define DSI_WRITE(cmd, size)	do {				\
{	\
	int tx_ret = 0; \
	tx_ret = smcdsd_dsi_tx_data(lcd, cmd, size);		\
	if (tx_ret < 0)						\
		dev_info(&lcd->ld->dev, "%s: tx_ret(%d) failed to write %02x %s\n", __func__, tx_ret, cmd[0], #cmd);	\
}	\
} while (0)

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
		u8 reserved;
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

	unsigned char			**acl_table;
	unsigned char			**acl_dim_table;
	unsigned char			*((*hbm_table)[HBM_STATUS_MAX]);
	unsigned int			*brightness_table;
	struct device			svc_dev;

	union {
		struct {
			u8		reserved;
			u8		id[LDI_LEN_ID];
		};
		u32			value;
	} id_info;

	int				lux;

	unsigned char			code[LDI_LEN_CHIP_ID];
	unsigned char			date[LDI_LEN_DATE];
	unsigned int			coordinate[2];
	unsigned char			coordinates[20];
	unsigned char			manufacture_info[LDI_LEN_MANUFACTURE_INFO + LDI_LEN_MANUFACTURE_INFO_CELL_ID];
	unsigned int			temperature_index;
	unsigned char			rddpm;
	unsigned char			rddsm;


	struct mipi_dsi_lcd_common	*pdata;
	struct mutex			lock;
	unsigned int			need_primary_lock;

	struct notifier_block		fb_notifier_panel;
	struct notifier_block		reboot_notifier;

	unsigned int			fac_info;
	unsigned int			fac_done;

	unsigned int			enable_fd;

	unsigned int			conn_det_enable;
	unsigned int			conn_det_count;
	struct workqueue_struct		*conn_workqueue;
	struct work_struct		conn_work;

	int				temperature;
	unsigned int			trans_dimming;
	unsigned int			adaptive_control;
	unsigned int			display_on_delay_req;

	unsigned int			mask_brightness;
	unsigned int			actual_mask_brightness;
	bool					mask_state;
	bool					force_normal_transition;
	int					mask_delay;

	unsigned int			mask_framedone_check_req;

	unsigned int			acl_dimming_update_req;
	unsigned int			acl_dimming;

#if defined(CONFIG_SMCDSD_DOZE)
	union lpm_info			alpm;
	union lpm_info			current_alpm;

	unsigned int			prev_brightness;
	union lpm_info			prev_alpm;
#endif
#if defined(CONFIG_SMCDSD_MDNIE)
	struct class			*mdnie_class;
#endif

#if defined(CONFIG_SMCDSD_DPUI)
	struct notifier_block		dpui_notif;
#endif

	struct timenval			tnv[2];
};

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

static int smcdsd_dsi_tx_data(struct lcd_info *lcd, u8 *cmd, u32 len)
{
	int ret = 0;
	int retry = 2;

	if (!lcd->connected)
		return ret;

	/*
	 * We assume that all the TX function will be called in lcd->lock
	 * If not, Stop here for debug.
	 */
	if (IS_ENABLED(CONFIG_MTK_FB) && !mutex_is_locked(&lcd->lock)) {
		dev_info(&lcd->ld->dev, "%s: fail. lcd->lock should be locked.\n", __func__);
		BUG();
	}

try_write:
	if (len == 1)
		ret = lcd->pdata->tx(lcd->pdata, MIPI_DSI_DCS_SHORT_WRITE, (unsigned long)cmd, len, lcd->need_primary_lock);
	else if (len == 2)
		ret = lcd->pdata->tx(lcd->pdata, MIPI_DSI_DCS_SHORT_WRITE_PARAM, (unsigned long)cmd, len, lcd->need_primary_lock);
	else
		ret = lcd->pdata->tx(lcd->pdata, MIPI_DSI_DCS_LONG_WRITE, (unsigned long)cmd, len, lcd->need_primary_lock);

	if (ret < 0) {
		if (--retry)
			goto try_write;
		else
			dev_info(&lcd->ld->dev, "%s: fail. %02x, ret: %d\n", __func__, cmd[0], ret);
	}

	return ret;
}

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

static int _smcdsd_dsi_rx_data(struct lcd_info *lcd, u8 cmd, u32 size, u8 *buf, u32 offset)
{
	int ret = 0, rx_size = 0;
	int retry = 2;

	if (!lcd->connected)
		return ret;

try_read:
	rx_size = lcd->pdata->rx(lcd->pdata, MIPI_DSI_DCS_READ, offset, cmd, size, buf, lcd->need_primary_lock);
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

static int smcdsd_panel_read_bit_info(struct lcd_info *lcd, u32 index, u8 *rxbuf)
{
	int ret = 0;
	u8 buf[5] = {0, };
	struct bit_info *bit_info_list = ldi_bit_info_list;
	unsigned int reg, len, mask, expect, offset, invert, print_tag, bit, i, bit_max = 0, merged = 0;
	char **print_org = NULL;
	char *print_new[sizeof(u32) * BITS_PER_BYTE] = {0, };
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

		if (likely(&pdata->abd)) {
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
	return ret;
}

static int ea8076_exitalpm(struct lcd_info *lcd)
{
	int ret = 0;

	dev_info(&lcd->ld->dev, "%s: brightness: %3d, lpm: %06x(%06x)\n", __func__,
		lcd->bd->props.brightness, lcd->current_alpm.value, lcd->alpm.value);

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
	return ret;
}
#endif

static int get_mask_brightness_delay(struct lcd_info *lcd)
{
	int retVal = 0;

	/* 2msec delay for VFP */
	retVal = 2;

	return retVal;
}

static int low_level_set_brightness(struct lcd_info *lcd, int force)
{
	int ret = 0;

	if (lcd->mask_delay) {
		mdelay(lcd->mask_delay);
		dev_info(&lcd->ld->dev, "%s: mask delay : %d\n", __func__, lcd->mask_delay);
		lcd->mask_delay = 0;
	}

	DSI_WRITE(SEQ_TEST_KEY_ON_F0, ARRAY_SIZE(SEQ_TEST_KEY_ON_F0));

	smcdsd_panel_set_wrctrld(lcd, force);

	smcdsd_panel_set_acl(lcd, force);

	DSI_WRITE(SEQ_TEST_KEY_OFF_F0, ARRAY_SIZE(SEQ_TEST_KEY_OFF_F0));

	return ret;
}

static int smcdsd_panel_set_brightness(struct lcd_info *lcd, int force)
{
	int ret = 0;

	if (lcd->mask_state == true) {
		dev_info(&lcd->ld->dev, "%s: skip! MASK LAYER\n", __func__);
		return ret;
	}

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

	if (!force && lcd->state != PANEL_STATE_RESUMED) {
		dev_info(&lcd->ld->dev, "%s: brightness: %d, panel_state: %d\n", __func__, lcd->brightness, lcd->state);
		goto exit;
	}

	low_level_set_brightness(lcd, force);

	dev_info(&lcd->ld->dev, "brightness: %3d, %4d, %6x, lx: %d\n", lcd->brightness,
		lcd->brightness_table[lcd->brightness], lcd->current_wrctrld.value, lcd->lux);

	if (!force)
		panel_bl_update_average(lcd, 1);

exit:
	return ret;
}

static int smcdsd_panel_set_mask_brightness(struct lcd_info *lcd)
{
	int ret = 0;

#if defined(CONFIG_SMCDSD_DOZE)
	if (lcd->current_alpm.state) {
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

	if (lcd->state != PANEL_STATE_RESUMED) {
		dev_info(&lcd->ld->dev, "%s: brightness: %d, panel_state: %d\n", __func__, lcd->brightness, lcd->state);
		goto exit;
	}
	lcd->force_normal_transition = true;
	low_level_set_brightness(lcd, 1);
	lcd->force_normal_transition = false;

	dev_info(&lcd->ld->dev, "brightness: %3d, %4d, %6x, lx: %d\n", lcd->brightness,
		lcd->brightness_table[lcd->brightness], lcd->current_wrctrld.value, lcd->lux);

exit:
	return ret;
}

static int panel_get_brightness(struct backlight_device *bd)
{
	struct lcd_info *lcd = bl_get_data(bd);

	return lcd->brightness_table[lcd->brightness];
}

static int panel_set_brightness(struct backlight_device *bd)
{
	int ret = 0;
	struct lcd_info *lcd = bl_get_data(bd);

	mutex_lock(&lcd->lock);
	if (lcd->state == PANEL_STATE_RESUMED) {
		ret = smcdsd_panel_set_brightness(lcd, 0);
		if (ret < 0)
			dev_info(&lcd->ld->dev, "%s: failed to set brightness\n", __func__);
	}
	mutex_unlock(&lcd->lock);

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

	ret = smcdsd_panel_read_bit_info(lcd, LDI_BIT_ENUM_RDNUMED, buf);
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

	ret = smcdsd_panel_read_bit_info(lcd, LDI_BIT_ENUM_RDDSDR, buf);
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

	ret = smcdsd_panel_read_bit_info(lcd, LDI_BIT_ENUM_RDDPM, &lcd->rddpm);
	if (ret < 0)
		dev_info(&lcd->ld->dev, "%s: fail\n", __func__);

	return ret;
}

static int ea8076_read_rddsm(struct lcd_info *lcd)
{
	int ret = 0;

	ret = smcdsd_panel_read_bit_info(lcd, LDI_BIT_ENUM_RDDSM, &lcd->rddsm);
	if (ret < 0)
		dev_info(&lcd->ld->dev, "%s: fail\n", __func__);

	return ret;
}

static int ea8076_read_id(struct lcd_info *lcd)
{
	int ret = 0;
	struct mipi_dsi_lcd_common *pdata = lcd->pdata;
	static char *LDI_BIT_DESC_ID[BITS_PER_BYTE * LDI_LEN_ID] = {
		[0 ... 23] = "ID Read Fail",
	};

	lcd->id_info.value = 0;
	pdata->lcdconnected = lcd->connected = lcdtype ? 1 : 0;

	ret = smcdsd_dsi_rx_info(lcd, LDI_REG_ID, LDI_LEN_ID, lcd->id_info.id);

	if (ret < 0 || !lcd->id_info.value) {
		pdata->lcdconnected = lcd->connected = 0;
		dev_info(&lcd->ld->dev, "%s: connected lcd is invalid\n", __func__);

		if (lcdtype)
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

	dev_info(&lcd->ld->dev, "%s: %*ph\n", __func__, ARRAY_SIZE(lcd->date), lcd->date);

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

	dev_info(&lcd->ld->dev, "%s: %*ph\n", __func__, ARRAY_SIZE(lcd->manufacture_info), lcd->manufacture_info);

	return ret;
}

static int ea8076_read_chip_id(struct lcd_info *lcd)
{
	int ret = 0;
	unsigned char buf[LDI_LEN_CHIP_ID] = {0, };

	ret = _smcdsd_dsi_rx_data_offset(lcd, LDI_REG_CHIP_ID, LDI_LEN_CHIP_ID, buf, LDI_GPARA_CHIP_ID);
	if (ret < 0)
		dev_info(&lcd->ld->dev, "%s: fail\n", __func__);

	memcpy(lcd->code, buf, LDI_LEN_CHIP_ID);

	dev_info(&lcd->ld->dev, "%s: %*ph\n", __func__, ARRAY_SIZE(lcd->code), lcd->code);

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

static int ea8076_read_init_info(struct lcd_info *lcd)
{
	int ret = 0;
	struct mipi_dsi_lcd_common *pdata = lcd->pdata;

	pdata->lcdconnected = lcd->connected = lcdtype ? 1 : 0;

	lcd->id_info.id[0] = (lcdtype & 0xFF0000) >> 16;
	lcd->id_info.id[1] = (lcdtype & 0x00FF00) >> 8;
	lcd->id_info.id[2] = (lcdtype & 0x0000FF) >> 0;

	dev_info(&lcd->ld->dev, "%s: %x\n", __func__, cpu_to_be32(lcd->id_info.value));

	if (!lcd->fac_info)
		return ret;

	ea8076_read_id(lcd);

	DSI_WRITE(SEQ_TEST_KEY_ON_F0, ARRAY_SIZE(SEQ_TEST_KEY_ON_F0));

	ea8076_read_coordinate(lcd);
	ea8076_read_manufacture_info(lcd);
	ea8076_read_chip_id(lcd);
	ea8076_read_elvss(lcd);

	DSI_WRITE(SEQ_TEST_KEY_OFF_F0, ARRAY_SIZE(SEQ_TEST_KEY_OFF_F0));

#if defined(CONFIG_SMCDSD_MDNIE)
	attr_store_for_each(lcd->mdnie_class, "color_coordinate", lcd->coordinates, strlen(lcd->coordinates));
#endif

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
	lcd->current_alpm.value = 0;
#endif

	return ret;
}

static int ea8076_init(struct lcd_info *lcd)
{
	int ret = 0;

	dev_info(&lcd->ld->dev, "%s\n", __func__);

	/* 6. Sleep Out(11h) */
	DSI_WRITE(SEQ_SLEEP_OUT, ARRAY_SIZE(SEQ_SLEEP_OUT));

	/* 7. Wait 10ms */
	usleep_range(10000, 11000);

	ea8076_read_init_info(lcd);

#if defined(CONFIG_SMCDSD_DPUI)
	ea8076_read_rddsdr(lcd);
#endif
	/* Common Setting */
	/* Testkey Enable */
	DSI_WRITE(SEQ_TEST_KEY_ON_F0, ARRAY_SIZE(SEQ_TEST_KEY_ON_F0));
	DSI_WRITE(SEQ_TEST_KEY_ON_FC, ARRAY_SIZE(SEQ_TEST_KEY_ON_FC));

	if (lcd->fac_info) {
		dev_info(&lcd->ld->dev, "%s: enable_fd: %d\n", __func__, lcd->enable_fd);

		if (lcd->enable_fd)
			DSI_WRITE(SEQ_ASWIRE_OFF, ARRAY_SIZE(SEQ_ASWIRE_OFF));
		else
			DSI_WRITE(SEQ_ASWIRE_OFF_FD_OFF, ARRAY_SIZE(SEQ_ASWIRE_OFF_FD_OFF));
	}

	smcdsd_dsi_tx_set(lcd, LCD_SEQ_INIT_1, ARRAY_SIZE(LCD_SEQ_INIT_1));
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

	lcd = container_of(self, struct lcd_info, fb_notifier_panel);

	fb_blank = *(int *)evdata->data;

	dev_info(&lcd->ld->dev, "%s: %02lx, %d, brightness(%3d,%3d)\n", __func__, event, fb_blank, lcd->brightness, lcd->bd->props.brightness);

	if (evdata->info->node)
		return NOTIFY_DONE;

	if (IS_ENABLED(CONFIG_MTK_FB))
		mutex_lock(&lcd->lock);

	if (IS_EARLY(event) && fb_blank == FB_BLANK_POWERDOWN) {
		lcd->enable = 0;
	} else if (IS_AFTER(event) && fb_blank == FB_BLANK_UNBLANK && !lcd->shutdown) {
		lcd->enable = 1;

		ea8076_displayon(lcd);
	}

	if (fb_blank == FB_BLANK_UNBLANK && event == FB_EVENT_BLANK)
		panel_bl_init_average(lcd, 1);

	if (IS_ENABLED(CONFIG_MTK_FB))
		mutex_unlock(&lcd->lock);

	return NOTIFY_DONE;
}

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
	lcd->fb_notifier_panel.notifier_call = fb_notifier_callback;
	smcdsd_register_notifier(&lcd->fb_notifier_panel);

	lcd->reboot_notifier.notifier_call = panel_reboot_notify;
	register_reboot_notifier(&lcd->reboot_notifier);

#if defined(CONFIG_SMCDSD_DPUI)
	lcd->dpui_notif.notifier_call = panel_dpui_notifier_callback;
	if (lcd->connected)
		dpui_logging_register(&lcd->dpui_notif, DPUI_TYPE_PANEL);
#endif

	dev_info(&lcd->ld->dev, "%s\n", __func__);

	return 0;
}

static int ea8076_probe(struct lcd_info *lcd)
{
	dev_info(&lcd->ld->dev, "+ %s\n", __func__);

	lcd->bd->props.max_brightness = EXTEND_BRIGHTNESS;
	lcd->bd->props.brightness = UI_DEFAULT_BRIGHTNESS;
	lcd->mask_brightness = UI_MAX_BRIGHTNESS;
	lcd->force_normal_transition = false;

	lcd->state = PANEL_STATE_RESUMED;
	lcd->enable = 1;

	lcd->temperature = NORMAL_TEMPERATURE;
	lcd->adaptive_control = !!ACL_STATUS_15P;
	lcd->lux = -1;

	lcd->acl_table = ACL_TABLE;
	lcd->acl_dim_table = ACL_DIM_TABLE;
	lcd->hbm_table = HBM_TABLE;

	lcd->trans_dimming = TRANS_DIMMING_ON;
	lcd->acl_dimming_update_req = 0;
	lcd->display_on_delay_req = 0;

	lcd->acl_dimming = ACL_DIMMING_ON;
	lcd->mask_framedone_check_req = 0;
	lcd->fac_info = 1;

	lcd->brightness_table = brightness_table;
	lcd->pdata->lcm_params->hbm_enable_wait_frame = 2;
	lcd->pdata->lcm_params->hbm_disable_wait_frame = 0;

	mutex_lock(&lcd->lock);
	ea8076_read_init_info(lcd);
	smcdsd_panel_set_brightness(lcd, 1);
	mutex_unlock(&lcd->lock);

	lcd->fac_info = lcd->fac_done = lcd->enable_fd = IS_ENABLED(CONFIG_SEC_FACTORY) ? 1 : 0;

	dev_info(&lcd->ld->dev, "- %s\n", __func__);

	return 0;
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

	if (lcd->state != PANEL_STATE_RESUMED)
		return -EINVAL;

	ret = kstrtouint(buf, 0, &value);
	if (ret < 0)
		return ret;


	dev_info(&lcd->ld->dev, "%s: %d\n", __func__, value);

	lcd->enable_fd = value;

	return size;
}
static DEVICE_ATTR_RW(enable_fd);

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

static ssize_t lcd_type_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct lcd_info *lcd = dev_get_drvdata(dev);

	sprintf(buf, "%s_%02X%02X%02X\n", LCD_TYPE_VENDOR, lcd->id_info.id[0], lcd->id_info.id[1], lcd->id_info.id[2]);

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
	struct lcd_info *lcd = dev_get_drvdata(dev);

	for (i = 0; i <= EXTEND_BRIGHTNESS; i++)
		pos += sprintf(pos, "%3d %4d\n", i, lcd->brightness_table[i]);

	return pos - buf;
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
	int value, rc, temperature_index = 0;
	int ret = 0;

	rc = kstrtoint(buf, 0, &value);
	if (rc < 0)
		return rc;

	if (lcd->state != PANEL_STATE_RESUMED || !lcd->enable) {
		dev_info(&lcd->ld->dev, "%s: panel state is %d, %d\n", __func__, lcd->state, lcd->enable);
		ret = -EIO;
		return ret;
	}

	switch (value) {
	case 1:
		temperature_index = TEMP_ABOVE_MINUS_00_DEGREE;
		value = NORMAL_TEMPERATURE;
		break;
	case 0:
	case -15:
		temperature_index = TEMP_ABOVE_MINUS_15_DEGREE;
		break;
	case -16:
		temperature_index = TEMP_BELOW_MINUS_15_DEGREE;
		break;
	default:
		dev_info(&lcd->ld->dev, "%s: %d is invalid\n", __func__, value);
		return -EINVAL;
	}

	mutex_lock(&lcd->lock);
	lcd->temperature = value;
	lcd->temperature_index = temperature_index;

	if (lcd->state == PANEL_STATE_RESUMED)
		smcdsd_panel_set_brightness(lcd, 0);

	dev_info(&lcd->ld->dev, "%s: %d, %d, %d\n", __func__, value, lcd->temperature, lcd->temperature_index);
	mutex_unlock(&lcd->lock);

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
	int ret = 0;

	rc = kstrtoint(buf, 0, &value);
	if (rc < 0)
		return rc;

	if (lcd->state != PANEL_STATE_RESUMED || !lcd->enable) {
		dev_info(&lcd->ld->dev, "%s: panel state is %d, %d\n", __func__, lcd->state, lcd->enable);
		ret = -EIO;
		return ret;
	}

	if (lcd->lux != value) {
		dev_info(&lcd->ld->dev, "++%s: %d, %d\n", __func__, lcd->lux, value);
		mutex_lock(&lcd->lock);
		lcd->lux = value;
		mutex_unlock(&lcd->lock);
		dev_info(&lcd->ld->dev, "--%s: %d, %d\n", __func__, lcd->lux, value);
#if defined(CONFIG_SMCDSD_MDNIE)
		attr_store_for_each(lcd->mdnie_class, attr->attr.name, buf, size);
#endif
	}

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
	int rc;
	unsigned int value;
	int ret = 0;

	rc = kstrtouint(buf, 0, &value);
	if (rc < 0)
		return rc;

	if (lcd->state != PANEL_STATE_RESUMED || !lcd->enable) {
		dev_info(&lcd->ld->dev, "%s: panel state is %d, %d\n", __func__, lcd->state, lcd->enable);
		ret = -EIO;
		return ret;
	}

	if (lcd->adaptive_control != value) {
		mutex_lock(&lcd->lock);
		dev_info(&lcd->ld->dev, "++%s: %d, %d\n", __func__, lcd->adaptive_control, value);
		lcd->adaptive_control = value;
		dev_info(&lcd->ld->dev, "--%s: %d, %d\n", __func__, lcd->adaptive_control, value);
		if (lcd->state == PANEL_STATE_RESUMED)
			smcdsd_panel_set_brightness(lcd, 1);
		mutex_unlock(&lcd->lock);
	}

	return size;
}

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
		mutex_lock(&lcd->lock);
		call_drv_ops(lcd->pdata, doze_init, 0);
		usleep_range(17000, 18000);
		ea8076_displayon(lcd);
		mutex_unlock(&lcd->lock);

		break;
	case ALPM_ON_LOW:
	case HLPM_ON_LOW:
	case ALPM_ON_HIGH:
	case HLPM_ON_HIGH:
		mutex_lock(&lcd->lock);
		if (lcd->prev_alpm.mode == ALPM_OFF)
			lcd->prev_brightness = lcd->bd->props.brightness;
		lcd->bd->props.brightness = (value <= HLPM_ON_LOW) ? 0 : UI_MAX_BRIGHTNESS;
		call_drv_ops(lcd->pdata, doze_init, 1);
		usleep_range(17000, 18000);
		ea8076_displayon(lcd);
		mutex_unlock(&lcd->lock);
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

	if (lcd->current_alpm.state && lcd->enable)
		call_drv_ops(lcd->pdata, doze_init, 1);

	mutex_unlock(&lcd->lock);

	return size;
}

static ssize_t alpm_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct lcd_info *lcd = dev_get_drvdata(dev);

	sprintf(buf, "%06x\n", lcd->current_alpm.value);

	return strlen(buf);
}

static DEVICE_ATTR_RW(alpm);
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

static ssize_t actual_mask_brightness_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct lcd_info *lcd = dev_get_drvdata(dev);

	sprintf(buf, "%d\n", lcd->actual_mask_brightness);

	return strlen(buf);
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
static DEVICE_ATTR_RW(conn_det);

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

	dev_info(&lcd->ld->dev, "%s: %u(%u)\n", __func__, lcd->fac_info, value);

	mutex_lock(&lcd->lock);
	lcd->fac_info = value;
	mutex_unlock(&lcd->lock);

	if (!lcd->fac_done) {
		lcd->fac_done = 1;
		smcdsd_abd_con_register(abd);
		sysfs_update_group(&lcd->ld->dev.kobj, &lcd_sysfs_attr_group);
	}

	return size;
}

static DEVICE_ATTR_RO(actual_mask_brightness);
static DEVICE_ATTR_RW(mask_brightness);

static DEVICE_ATTR_RO(lcd_type);
static DEVICE_ATTR_RO(window_type);
static DEVICE_ATTR_RO(brightness_table);
static DEVICE_ATTR_RW(lux);
static DEVICE_ATTR_RW(fac_info);
static DEVICE_ATTR_RW(adaptive_control);
static DEVICE_ATTR_RO(manufacture_code);
static DEVICE_ATTR_RO(cell_id);
static DEVICE_ATTR_RW(temperature);
static DEVICE_ATTR_RO(color_coordinate);
static DEVICE_ATTR_RO(manufacture_date);
static DEVICE_ATTR_RO(octa_id);

static DEVICE_ATTR(SVC_OCTA, 0444, cell_id_show, NULL);
static DEVICE_ATTR(SVC_OCTA_CHIPID, 0444, octa_id_show, NULL);
static DEVICE_ATTR(SVC_OCTA_DDI_CHIPID, 0444, manufacture_code_show, NULL);

static DEVICE_ATTR(xtalk_mode, 0220, NULL, xtalk_mode_store);
static DEVICE_ATTR(brt_avg, 0444, brt_avg_show, NULL);

static struct attribute *lcd_sysfs_attributes[] = {
	&dev_attr_lcd_type.attr,
	&dev_attr_window_type.attr,
	&dev_attr_brightness_table.attr,
	&dev_attr_lux.attr,
	&dev_attr_adaptive_control.attr,
	&dev_attr_alpm.attr,
	&dev_attr_manufacture_code.attr,
	&dev_attr_cell_id.attr,
	&dev_attr_temperature.attr,
	&dev_attr_color_coordinate.attr,
	&dev_attr_manufacture_date.attr,
	&dev_attr_octa_id.attr,
	&dev_attr_conn_det.attr,
	&dev_attr_enable_fd.attr,
	&dev_attr_mask_brightness.attr,
	&dev_attr_actual_mask_brightness.attr,
	&dev_attr_xtalk_mode.attr,
	&dev_attr_brt_avg.attr,
#if defined(CONFIG_SMCDSD_DPUI)
	&dev_attr_dpui.attr,
	&dev_attr_dpui_dbg.attr,
#endif
	NULL,
};

static umode_t lcd_sysfs_is_visible(struct kobject *kobj,
	struct attribute *attr, int index)
{
	struct device *dev = container_of(kobj, struct device, kobj);
	struct lcd_info *lcd = dev_get_drvdata(dev);
	umode_t mode = attr->mode;

	if (attr == &dev_attr_conn_det.attr ||
		attr == &dev_attr_enable_fd.attr) {
		if (!lcd->fac_info && !lcd->fac_done)
			mode = 0;
	}

	return mode;
}

static const struct attribute_group lcd_sysfs_attr_group = {
	.is_visible = lcd_sysfs_is_visible,
	.attrs = lcd_sysfs_attributes,
};

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

	// check needlock, check framedone
	ret = smcdsd_dsi_tx_set(lcd, seq, num); // mdnie is sent only lcd enable,
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
		dev_info(&lcd->ld->dev, "sysfs_create_group fail\n");

	ret = sysfs_create_file(&lcd->bd->dev.kobj, &dev_attr_fac_info.attr);
	if (ret < 0)
		dev_info(&lcd->ld->dev, "sysfs_create_file fail\n");

	lcd_init_svc(lcd);

	init_debugfs_backlight(lcd->bd, lcd->brightness_table, NULL);
}

//@ From primary display
static int smcdsd_panel_init(struct platform_device *p)
{
	struct lcd_info *lcd = get_lcd_info(p);

	dev_info(&lcd->ld->dev, "+ %s: state(%d)\n", __func__, lcd->state);

	if (lcd->state == PANEL_STATE_SUSPENED) {
		ea8076_init(lcd);

		lcd->state = PANEL_STATE_RESUMED;
	}

	dev_info(&lcd->ld->dev, "- %s: state(%d) connected(%d)\n", __func__, lcd->state, lcd->connected);

	return 0;
}

static int smcdsd_panel_reset(struct platform_device *p, unsigned int on)
{
	struct lcd_info *lcd = get_lcd_info(p);

	dev_info(&lcd->ld->dev, "+ %s: on(%d)\n", __func__, on);

	if (on)
		run_list(&p->dev, "panel_reset_enable");
	else
		run_list(&p->dev, "panel_reset_disable");

	dev_info(&lcd->ld->dev, "- %s: state(%d) connected(%d)\n", __func__, lcd->state, lcd->connected);

	return 0;
}

static int smcdsd_panel_power(struct platform_device *p, unsigned int on)
{
	struct lcd_info *lcd = get_lcd_info(p);

	dev_info(&lcd->ld->dev, "+ %s: on(%d)\n", __func__, on);

	if (on)
		run_list(&p->dev, "panel_power_enable");
	else
		run_list(&p->dev, "panel_power_disable");

	dev_info(&lcd->ld->dev, "- %s: state(%d) connected(%d)\n", __func__, lcd->state, lcd->connected);

	return 0;
}

//@ From primary display
static int smcdsd_panel_exit(struct platform_device *p)
{
	struct lcd_info *lcd = get_lcd_info(p);

	dev_info(&lcd->ld->dev, "+ %s: state(%d)\n", __func__, lcd->state);

	if (lcd->state == PANEL_STATE_SUSPENED)
		goto exit;

	ea8076_exit(lcd);

	lcd->state = PANEL_STATE_SUSPENED;

	dev_info(&lcd->ld->dev, "- %s: state(%d) connected(%d)\n", __func__, lcd->state, lcd->connected);

exit:
	return 0;
}

static int smcdsd_panel_set_mask(struct platform_device *p, int on)
{
	struct lcd_info *lcd = get_lcd_info(p);
	struct mipi_dsi_lcd_common *pdata = lcd->pdata;
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
	lcd->mask_delay = get_mask_brightness_delay(lcd);

	/* Update BR with NO LCD mutex */
//	_smcdsd_panel_set_brightness(lcd, 1, false);
	smcdsd_panel_set_mask_brightness(lcd);
	lcd->mask_framedone_check_req = true;

	if (on)
		smcdsd_abd_save_str(abd, "mask_brightness");
	else
		smcdsd_abd_save_str(abd, "prev_brightness");

	return 0;
}

static int smcdsd_panel_get_mask(struct platform_device *p)
{
	struct lcd_info *lcd = get_lcd_info(p);

	return lcd->mask_state;
}

static int smcdsd_panel_path_lock(struct platform_device *p, bool locking)
{
	struct lcd_info *lcd = get_lcd_info(p);

	if (locking) {
		mutex_lock(&lcd->lock);
		lcd->need_primary_lock = 0;
	} else {
		lcd->need_primary_lock = 1;
		mutex_unlock(&lcd->lock);
	}

	dbg_info("%s: path_lock(%d)\n", __func__, locking);

	return 0;
}

static int smcdsd_panel_framedone_mask(struct platform_device *p)
{
	struct lcd_info *lcd = get_lcd_info(p);
	struct mipi_dsi_lcd_common *pdata = lcd->pdata;
	struct abd_protect *abd = &pdata->abd;

	if (lcd->mask_framedone_check_req) {
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
	}
	return 0;
}

#if defined(CONFIG_SMCDSD_DOZE)
//@ From primary display
static int smcdsd_panel_initalpm(struct platform_device *p, unsigned int on)
{
	struct lcd_info *lcd = get_lcd_info(p);

	dev_info(&lcd->ld->dev, "+ %s: state(%d) on(%d)\n", __func__, lcd->state, on);

	if (on) {
		if (lcd->state == PANEL_STATE_SUSPENED) {
			ea8076_init(lcd);
			lcd->display_on_delay_req = 1;
			lcd->state = PANEL_STATE_RESUMED;
		}
		ea8076_enteralpm(lcd);
	} else {
		if (lcd->state == PANEL_STATE_SUSPENED) {
			ea8076_init(lcd);

			lcd->state = PANEL_STATE_RESUMED;
		}

		ea8076_exitalpm(lcd);

		smcdsd_panel_set_brightness(lcd, 1);
	}

	dev_info(&lcd->ld->dev, "- %s: state(%d) connected(%d) on(%d)\n", __func__, lcd->state, lcd->connected, on);

	return 0;
}
#endif

static int smcdsd_panel_framedone(struct platform_device *p)
{
	smcdsd_panel_framedone_mask(p);
	return 0;
}

static int smcdsd_panel_probe(struct platform_device *p)
{
	struct device *dev = &p->dev;
	int ret = 0;
	struct lcd_info *lcd;

	lcd = kzalloc(sizeof(struct lcd_info), GFP_KERNEL);
	if (!lcd) {
		dbg_info("%s: failed to allocate for lcd\n", __func__);
		ret = -ENOMEM;
		goto exit;
	}

	lcd->ld = lcd_device_register("panel", dev, lcd, NULL);
	if (IS_ERR(lcd->ld)) {
		dbg_info("%s: failed to register lcd device\n", __func__);
		ret = PTR_ERR(lcd->ld);
		goto exit;
	}

	lcd->bd = backlight_device_register("panel", dev, lcd, &panel_backlight_ops, NULL);
	if (IS_ERR(lcd->bd)) {
		dbg_info("%s: failed to register backlight device\n", __func__);
		ret = PTR_ERR(lcd->bd);
		goto exit;
	}

	mutex_init(&lcd->lock);
	lcd->need_primary_lock = 1;

	lcd->pdata = get_lcd_common_info(p);

	set_lcd_info(p, lcd);

	ea8076_probe(lcd);

	ea8076_register_notifier(lcd);

	lcd_init_sysfs(lcd);

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

struct mipi_dsi_lcd_driver ea8076_mipi_lcd_driver = {
	.driver = {
		.name = "ea8076",
	},
	.probe		= smcdsd_panel_probe,
	.init		= smcdsd_panel_init,
	.exit		= smcdsd_panel_exit,
	.panel_reset	= smcdsd_panel_reset,
	.panel_power	= smcdsd_panel_power,
#ifdef CONFIG_MTK_HIGH_FRAME_RATE
	.set_fps		= smcdsd_panel_change_fps,
	.is_new_fps		= smcdsd_panel_is_new_fps,
#endif
#if defined(CONFIG_SMCDSD_DOZE)
	.doze_init	= smcdsd_panel_initalpm,
#endif
	.set_mask	= smcdsd_panel_set_mask,
	.get_mask	= smcdsd_panel_get_mask,
	.framedone_notify	= smcdsd_panel_framedone,
	.path_lock	= smcdsd_panel_path_lock,
};
__XX_ADD_LCD_DRIVER(ea8076_mipi_lcd_driver);

static int __init panel_conn_init(void)
{
	struct lcd_info *lcd = NULL;
	struct mipi_dsi_lcd_common *pdata = NULL;
	struct platform_device *pdev = NULL;

	pdev = of_find_device_by_path("/panel");
	if (!pdev) {
		dbg_info("%s: of_find_device_by_path fail\n", __func__);
		return 0;
	}

	pdata = platform_get_drvdata(pdev);
	if (!pdata) {
		dbg_info("%s: platform_get_drvdata fail\n", __func__);
		return 0;
	}

	if (!pdata->drv) {
		dbg_info("%s: lcd_driver invalid\n", __func__);
		return 0;
	}

	if (!pdata->drv->pdev) {
		dbg_info("%s: lcd_driver pdev invalid\n", __func__);
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
late_initcall_sync(panel_conn_init);

