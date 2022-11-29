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

#include "td4150_a01core_param.h"

#include "dd.h"
#include "timenval.h"

#if defined(CONFIG_SMCDSD_DPUI)
#include "dpui.h"

#define	DPUI_VENDOR_NAME	"BOE"
#define DPUI_MODEL_NAME		"TD4150"
#endif

#define dbg_info(fmt, ...)	pr_info(pr_fmt("%s: %3d: %s: " fmt), "lcd panel", __LINE__, __func__, ##__VA_ARGS__)

#define PANEL_STATE_SUSPENED	0
#define PANEL_STATE_RESUMED	1

#define DSI_WRITE(cmd, size)		do {				\
{	\
	int tx_ret = 0;	\
	tx_ret = smcdsd_dsi_tx_data(lcd, cmd, size);			\
	if (tx_ret < 0)							\
		dev_info(&lcd->ld->dev, "%s: tx_ret(%d) failed to write %s\n", __func__, tx_ret, #cmd);	\
}	\
} while (0)

struct lcd_info {
	unsigned int			connected;
	unsigned int			brightness;
	unsigned int			state;
	unsigned int			enable;
	unsigned int			shutdown;

	struct lcd_device		*ld;
	struct backlight_device		*bd;

	union {
		struct {
			u8		reserved;
			u8		id[LDI_LEN_ID];
		};
		u32			value;
	} id_info;

	int				lux;

	struct mipi_dsi_lcd_parent	*pdata;
	struct mutex			lock;
	struct mutex			protos_lock;

	struct notifier_block		fb_notifier;
	struct notifier_block		reboot_notifier;

#if defined(CONFIG_SMCDSD_DPUI)
	struct notifier_block		dpui_notif;
#endif

	unsigned int			display_on_delay_req;
	unsigned int			conn_init_done;
	unsigned int			conn_det_enable;
	unsigned int			conn_det_count;
	struct workqueue_struct *conn_workqueue;
	struct work_struct		conn_work;
};

#if defined(CONFIG_SMCDSD_PROTOS_PLUS)
static char *protos_state_names[PROTOS_MAX] = {
	"protos_none",
	"protos_ready",
	"protos_awake",
	"protos_sleep",
};
#endif

static struct i2c_client *_lcm_i2c_client;

unsigned int hw_rev;

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

static int _smcdsd_panel_set_brightness(struct lcd_info *lcd, int force, bool lock)
{
	int ret = 0;
	unsigned char bl_reg[2] = {0, };

	if (lock)
		mutex_lock(&lcd->lock);

	lcd->brightness = lcd->bd->props.brightness;

	if (!force && (lcd->state != PANEL_STATE_RESUMED || !lcd->enable)) {
		dev_info(&lcd->ld->dev, "%s: brightness: %d(%d), panel_state: %d, %d\n", __func__,
		lcd->brightness, lcd->bd->props.brightness, lcd->state, lcd->enable);
		goto exit;
	}

	bl_reg[0] = LDI_REG_BRIGHTNESS;
	bl_reg[1] = brightness_table[lcd->brightness];

	DSI_WRITE(bl_reg, ARRAY_SIZE(bl_reg));

	dev_info(&lcd->ld->dev, "%s: brightness: %3d, %3d(%2x), lx: %d\n", __func__,
		lcd->brightness, brightness_table[lcd->brightness], bl_reg[1], lcd->lux);

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

#if defined(CONFIG_SEC_FACTORY)
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

static int td4150_read_id(struct lcd_info *lcd)
{
	int ret = 0;
	struct mipi_dsi_lcd_parent *pdata = lcd->pdata;
	static char *LDI_BIT_DESC_ID[BITS_PER_BYTE * LDI_LEN_ID] = {
		[0 ... 23] = "ID Read Fail",
	};

	lcd->id_info.value = 0;
	pdata->lcdconnected = lcd->connected = lcdtype ? 1 : 0;

	ret = smcdsd_dsi_rx_info(lcd, 0xDA, 1, lcd->id_info.id);
	ret = smcdsd_dsi_rx_info(lcd, 0xDB, 1, lcd->id_info.id+1);
	ret = smcdsd_dsi_rx_info(lcd, 0xDC, 1, lcd->id_info.id+2);
	if (ret < 0 || !lcd->id_info.value) {
		pdata->lcdconnected = lcd->connected = 0;
		dev_info(&lcd->ld->dev, "%s: connected lcd is invalid\n", __func__);

		if (lcdtype && pdata)
			smcdsd_abd_save_bit(&pdata->abd, BITS_PER_BYTE * LDI_LEN_ID, cpu_to_be32(lcd->id_info.value), LDI_BIT_DESC_ID);
	}

	dev_info(&lcd->ld->dev, "%s: %x\n", __func__, cpu_to_be32(lcd->id_info.value));

	return ret;
}
#endif

#if defined(CONFIG_SMCDSD_DPUI)
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

static int panel_inc_dpui_u32_field(struct lcd_info *lcd, enum dpui_key key, u32 value)
{
	if (lcd->connected) {
		inc_dpui_u32_field(key, value);
		if (value)
			dev_info(&lcd->ld->dev, "%s: key(%d) invalid\n", __func__, key);
	}

	return 0;
}

static int td4150_read_rddpm(struct lcd_info *lcd)
{
	int ret = 0;

	ret = panel_read_bit_info(lcd, LDI_BIT_ENUM_RDDPM, &lcd->rddpm);
	if (ret < 0)
		dev_info(&lcd->ld->dev, "%s: fail\n", __func__);

	return ret;
}
#endif

static int td4150_read_init_info(struct lcd_info *lcd)
{
	int ret = 0;

	struct mipi_dsi_lcd_parent *pdata = lcd->pdata;

	pdata->lcdconnected = lcd->connected = lcdtype ? 1 : 0;

	lcd->id_info.id[0] = (lcdtype & 0xFF0000) >> 16;
	lcd->id_info.id[1] = (lcdtype & 0x00FF00) >> 8;
	lcd->id_info.id[2] = (lcdtype & 0x0000FF) >> 0;

	dev_info(&lcd->ld->dev, "%s: %x\n", __func__, cpu_to_be32(lcd->id_info.value));

	return ret;
}

static int td4150_exit(struct lcd_info *lcd)
{
	int ret = 0;

	dev_info(&lcd->ld->dev, "%s\n", __func__);

#if defined(CONFIG_SMCDSD_DPUI)
	td4150_read_rddpm(lcd);
#endif


	DSI_WRITE(SEQ_DISPLAY_OFF, ARRAY_SIZE(SEQ_DISPLAY_OFF));
	DSI_WRITE(SEQ_SLEEP_IN, ARRAY_SIZE(SEQ_SLEEP_IN));

	msleep(70);/* wait 4 frame */

	return ret;
}

static int td4150_displayon(struct lcd_info *lcd)
{
	int ret = 0;

	dev_info(&lcd->ld->dev, "%s\n", __func__);

	DSI_WRITE(SEQ_DISPLAY_ON, ARRAY_SIZE(SEQ_DISPLAY_ON));

	return ret;
}

static int __init get_hw_rev(char *arg)
{
        get_option(&arg, &hw_rev);

        dbg_info("%s: hw_rev: %d\n", __func__, hw_rev);

		return 0;
}
early_param("androidboot.revision", get_hw_rev);

static int td4150_init(struct lcd_info *lcd)
{
	int ret = 0;

	dev_info(&lcd->ld->dev, "%s hw_rev = %d\n", __func__,hw_rev);

#if defined(CONFIG_SEC_FACTORY)
	td4150_read_id(lcd);
#endif
#if defined(CONFIG_A01CORE_LTN_OPEN)
    if(hw_rev==0) {
#else
	if(hw_rev<=2) {
#endif
		DSI_WRITE(SEQ_TD4150_DV_B0_OPEN, ARRAY_SIZE(SEQ_TD4150_DV_B0_OPEN));
		DSI_WRITE(SEQ_TD4150_DV_B6, ARRAY_SIZE(SEQ_TD4150_DV_B6));
		DSI_WRITE(SEQ_TD4150_DV_B7, ARRAY_SIZE(SEQ_TD4150_DV_B7));
		DSI_WRITE(SEQ_TD4150_DV_B8, ARRAY_SIZE(SEQ_TD4150_DV_B8));
		DSI_WRITE(SEQ_TD4150_DV_B9, ARRAY_SIZE(SEQ_TD4150_DV_B9));
		DSI_WRITE(SEQ_TD4150_DV_BA, ARRAY_SIZE(SEQ_TD4150_DV_BA));
		DSI_WRITE(SEQ_TD4150_DV_BB, ARRAY_SIZE(SEQ_TD4150_DV_BB));
		DSI_WRITE(SEQ_TD4150_DV_BC, ARRAY_SIZE(SEQ_TD4150_DV_BC));
		DSI_WRITE(SEQ_TD4150_DV_BD, ARRAY_SIZE(SEQ_TD4150_DV_BD));
		DSI_WRITE(SEQ_TD4150_DV_BE, ARRAY_SIZE(SEQ_TD4150_DV_BE));
		DSI_WRITE(SEQ_TD4150_DV_C0, ARRAY_SIZE(SEQ_TD4150_DV_C0));
		DSI_WRITE(SEQ_TD4150_DV_C1, ARRAY_SIZE(SEQ_TD4150_DV_C1));
		DSI_WRITE(SEQ_TD4150_DV_C2, ARRAY_SIZE(SEQ_TD4150_DV_C2));
		DSI_WRITE(SEQ_TD4150_DV_C3, ARRAY_SIZE(SEQ_TD4150_DV_C3));
		DSI_WRITE(SEQ_TD4150_DV_C4, ARRAY_SIZE(SEQ_TD4150_DV_C4));
		DSI_WRITE(SEQ_TD4150_DV_C5, ARRAY_SIZE(SEQ_TD4150_DV_C5));
		DSI_WRITE(SEQ_TD4150_DV_C6, ARRAY_SIZE(SEQ_TD4150_DV_C6));
		DSI_WRITE(SEQ_TD4150_DV_C7, ARRAY_SIZE(SEQ_TD4150_DV_C7));
		DSI_WRITE(SEQ_TD4150_DV_C8, ARRAY_SIZE(SEQ_TD4150_DV_C8));
		DSI_WRITE(SEQ_TD4150_DV_C9, ARRAY_SIZE(SEQ_TD4150_DV_C9));
		DSI_WRITE(SEQ_TD4150_DV_CA, ARRAY_SIZE(SEQ_TD4150_DV_CA));
		DSI_WRITE(SEQ_TD4150_DV_CB, ARRAY_SIZE(SEQ_TD4150_DV_CB));
		DSI_WRITE(SEQ_TD4150_DV_CE, ARRAY_SIZE(SEQ_TD4150_DV_CE));
		DSI_WRITE(SEQ_TD4150_DV_CF, ARRAY_SIZE(SEQ_TD4150_DV_CF));
		DSI_WRITE(SEQ_TD4150_DV_D0, ARRAY_SIZE(SEQ_TD4150_DV_D0));
		DSI_WRITE(SEQ_TD4150_DV_D1, ARRAY_SIZE(SEQ_TD4150_DV_D1));
		DSI_WRITE(SEQ_TD4150_DV_D2, ARRAY_SIZE(SEQ_TD4150_DV_D2));
		DSI_WRITE(SEQ_TD4150_DV_D3, ARRAY_SIZE(SEQ_TD4150_DV_D3));
		DSI_WRITE(SEQ_TD4150_DV_E5, ARRAY_SIZE(SEQ_TD4150_DV_E5));
		DSI_WRITE(SEQ_TD4150_DV_D6, ARRAY_SIZE(SEQ_TD4150_DV_D6));
		DSI_WRITE(SEQ_TD4150_DV_D7, ARRAY_SIZE(SEQ_TD4150_DV_D7));
		DSI_WRITE(SEQ_TD4150_DV_D9, ARRAY_SIZE(SEQ_TD4150_DV_D9));
		DSI_WRITE(SEQ_TD4150_DV_DD, ARRAY_SIZE(SEQ_TD4150_DV_DD));
		DSI_WRITE(SEQ_TD4150_DV_DE, ARRAY_SIZE(SEQ_TD4150_DV_DE));
		DSI_WRITE(SEQ_TD4150_DV_E3, ARRAY_SIZE(SEQ_TD4150_DV_E3));
		DSI_WRITE(SEQ_TD4150_DV_E9, ARRAY_SIZE(SEQ_TD4150_DV_E9));
		DSI_WRITE(SEQ_TD4150_DV_EA, ARRAY_SIZE(SEQ_TD4150_DV_EA));
		DSI_WRITE(SEQ_TD4150_DV_EB, ARRAY_SIZE(SEQ_TD4150_DV_EB));
		DSI_WRITE(SEQ_TD4150_DV_EC, ARRAY_SIZE(SEQ_TD4150_DV_EC));
		DSI_WRITE(SEQ_TD4150_DV_ED, ARRAY_SIZE(SEQ_TD4150_DV_ED));
		DSI_WRITE(SEQ_TD4150_DV_EE, ARRAY_SIZE(SEQ_TD4150_DV_EE));
		DSI_WRITE(SEQ_TD4150_DV_B0_CLOSE, ARRAY_SIZE(SEQ_TD4150_DV_B0_CLOSE));
		DSI_WRITE(SEQ_DISPLAY_ON, ARRAY_SIZE(SEQ_DISPLAY_ON));

		DSI_WRITE(SEQ_SLEEP_OUT, ARRAY_SIZE(SEQ_SLEEP_OUT));
		msleep(70);/* wait 4 frame */
		DSI_WRITE(SEQ_BRIGHTNESS_MODE, ARRAY_SIZE(SEQ_BRIGHTNESS_MODE));
	} else { /* PV panel   */
		DSI_WRITE(SEQ_TD4150_PV_B0_OPEN, ARRAY_SIZE(SEQ_TD4150_PV_B0_OPEN));
		DSI_WRITE(SEQ_TD4150_PV_B6, ARRAY_SIZE(SEQ_TD4150_PV_B6));
		DSI_WRITE(SEQ_TD4150_PV_B7, ARRAY_SIZE(SEQ_TD4150_PV_B7));
		DSI_WRITE(SEQ_TD4150_PV_B8, ARRAY_SIZE(SEQ_TD4150_PV_B8));
		DSI_WRITE(SEQ_TD4150_PV_B9, ARRAY_SIZE(SEQ_TD4150_PV_B9));
		DSI_WRITE(SEQ_TD4150_PV_BA, ARRAY_SIZE(SEQ_TD4150_PV_BA));
		DSI_WRITE(SEQ_TD4150_PV_BB, ARRAY_SIZE(SEQ_TD4150_PV_BB));
		DSI_WRITE(SEQ_TD4150_PV_BC, ARRAY_SIZE(SEQ_TD4150_PV_BC));
		DSI_WRITE(SEQ_TD4150_PV_BD, ARRAY_SIZE(SEQ_TD4150_PV_BD));
		DSI_WRITE(SEQ_TD4150_PV_BE, ARRAY_SIZE(SEQ_TD4150_PV_BE));
		DSI_WRITE(SEQ_TD4150_PV_C0, ARRAY_SIZE(SEQ_TD4150_PV_C0));
		DSI_WRITE(SEQ_TD4150_PV_C1, ARRAY_SIZE(SEQ_TD4150_PV_C1));
		DSI_WRITE(SEQ_TD4150_PV_C2, ARRAY_SIZE(SEQ_TD4150_PV_C2));
		DSI_WRITE(SEQ_TD4150_PV_C3, ARRAY_SIZE(SEQ_TD4150_PV_C3));
		DSI_WRITE(SEQ_TD4150_PV_C4, ARRAY_SIZE(SEQ_TD4150_PV_C4));
		DSI_WRITE(SEQ_TD4150_PV_C5, ARRAY_SIZE(SEQ_TD4150_PV_C5));
		DSI_WRITE(SEQ_TD4150_PV_C6, ARRAY_SIZE(SEQ_TD4150_PV_C6));
		DSI_WRITE(SEQ_TD4150_PV_C7, ARRAY_SIZE(SEQ_TD4150_PV_C7));
		DSI_WRITE(SEQ_TD4150_PV_C8, ARRAY_SIZE(SEQ_TD4150_PV_C8));
		DSI_WRITE(SEQ_TD4150_PV_C9, ARRAY_SIZE(SEQ_TD4150_PV_C9));
		DSI_WRITE(SEQ_TD4150_PV_CA, ARRAY_SIZE(SEQ_TD4150_PV_CA));
		DSI_WRITE(SEQ_TD4150_PV_CB, ARRAY_SIZE(SEQ_TD4150_PV_CB));
		DSI_WRITE(SEQ_TD4150_PV_CE, ARRAY_SIZE(SEQ_TD4150_PV_CE));
		DSI_WRITE(SEQ_TD4150_PV_CF, ARRAY_SIZE(SEQ_TD4150_PV_CF));
		DSI_WRITE(SEQ_TD4150_PV_D0, ARRAY_SIZE(SEQ_TD4150_PV_D0));
		DSI_WRITE(SEQ_TD4150_PV_D1, ARRAY_SIZE(SEQ_TD4150_PV_D1));
		DSI_WRITE(SEQ_TD4150_PV_D2, ARRAY_SIZE(SEQ_TD4150_PV_D2));
		DSI_WRITE(SEQ_TD4150_PV_D3, ARRAY_SIZE(SEQ_TD4150_PV_D3));
		DSI_WRITE(SEQ_TD4150_PV_E5, ARRAY_SIZE(SEQ_TD4150_PV_E5));
		DSI_WRITE(SEQ_TD4150_PV_D6, ARRAY_SIZE(SEQ_TD4150_PV_D6));
		DSI_WRITE(SEQ_TD4150_PV_D7, ARRAY_SIZE(SEQ_TD4150_PV_D7));
		DSI_WRITE(SEQ_TD4150_PV_D9, ARRAY_SIZE(SEQ_TD4150_PV_D9));
		DSI_WRITE(SEQ_TD4150_PV_DD, ARRAY_SIZE(SEQ_TD4150_PV_DD));
		DSI_WRITE(SEQ_TD4150_PV_DE, ARRAY_SIZE(SEQ_TD4150_PV_DE));
		DSI_WRITE(SEQ_TD4150_PV_E3, ARRAY_SIZE(SEQ_TD4150_PV_E3));
		DSI_WRITE(SEQ_TD4150_PV_E9, ARRAY_SIZE(SEQ_TD4150_PV_E9));
		DSI_WRITE(SEQ_TD4150_PV_EA, ARRAY_SIZE(SEQ_TD4150_PV_EA));
		DSI_WRITE(SEQ_TD4150_PV_EB, ARRAY_SIZE(SEQ_TD4150_PV_EB));
		DSI_WRITE(SEQ_TD4150_PV_EC, ARRAY_SIZE(SEQ_TD4150_PV_EC));
		DSI_WRITE(SEQ_TD4150_PV_ED, ARRAY_SIZE(SEQ_TD4150_PV_ED));
		DSI_WRITE(SEQ_TD4150_PV_EE, ARRAY_SIZE(SEQ_TD4150_PV_EE));
		DSI_WRITE(SEQ_TD4150_PV_B0_CLOSE, ARRAY_SIZE(SEQ_TD4150_PV_B0_CLOSE));
		DSI_WRITE(SEQ_DISPLAY_ON, ARRAY_SIZE(SEQ_DISPLAY_ON));

		DSI_WRITE(SEQ_SLEEP_OUT, ARRAY_SIZE(SEQ_SLEEP_OUT));
		msleep(70);/* wait 4 frame */
		DSI_WRITE(SEQ_BRIGHTNESS_MODE, ARRAY_SIZE(SEQ_BRIGHTNESS_MODE));
		DSI_WRITE(SEQ_CABC_ON, ARRAY_SIZE(SEQ_CABC_ON));
		DSI_WRITE(SEQ_CMB_ON, ARRAY_SIZE(SEQ_CMB_ON));
	}
    dev_info(&lcd->ld->dev, "%d hw_rev is ",hw_rev);
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

		mutex_unlock(&lcd->lock);
		smcdsd_panel_set_brightness(lcd, 1);
	}

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

static int td4150_register_notifier(struct lcd_info *lcd)
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

	dev_info(&lcd->ld->dev, "%s\n", __func__);

	return 0;
}

static int lm36274_array_write(u8 *ptr, u8 len)
{
	unsigned int i = 0;
	int ret = 0;
	struct i2c_client *client = _lcm_i2c_client;
	char write_data[2] = { 0 };

	if (!client)
		return ret;

	if (!lcdtype) {
		pr_err("%s: lcdtype: %d\n", __func__, lcdtype);
		return ret;
	}

	if (len % 2) {
		pr_err("%s: length(%d) invalid\n", __func__, len);
		return ret;
	}

	for (i = 0; i < len; i += 2) {
		write_data[0] = ptr[i + 0];
		write_data[1] = ptr[i + 1];

		ret = i2c_master_send(client, write_data, 2);
		if (ret < 0)
			pr_err("%s: fail. %2x, %2x, %d\n", __func__, write_data[0], write_data[1], ret);
	}

	return ret;
}

static int lm36274_probe(struct i2c_client *client,
	const struct i2c_device_id *id)
{
	int ret = 0;

	pr_info("%s\n", __func__);
	pr_info("[I2C]: name=%s addr=0x%x\n",
								client->name, (unsigned int)client->addr);
	_lcm_i2c_client = client;

	return ret;
}

static struct i2c_device_id lm36274_i2c_id[] = {
	{LCM_I2C_ID_NAME, 0},
	{},
};

MODULE_DEVICE_TABLE(i2c, lm36274_i2c_id);

static const struct of_device_id lm36274_i2c_dt_ids[] = {
	{ .compatible = "mediatek,I2C_LCD_BIAS" },
	{ }
};

MODULE_DEVICE_TABLE(of, lm36274_i2c_dt_ids);

static struct i2c_driver lm36274_i2c_driver = {
	.driver = {
		.owner	= THIS_MODULE,
		.name	= LCM_I2C_ID_NAME,
		.of_match_table	= of_match_ptr(lm36274_i2c_dt_ids),
	},
	.id_table = lm36274_i2c_id,
	.probe = lm36274_probe,
};

static int td4150_probe(struct lcd_info *lcd)
{
	int ret = 0;

	dev_info(&lcd->ld->dev, "+ %s\n", __func__);

	lcd->bd->props.max_brightness = EXTEND_BRIGHTNESS;
	lcd->bd->props.brightness = UI_DEFAULT_BRIGHTNESS;

	lcd->state = PANEL_STATE_RESUMED;
	lcd->enable = 1;
	lcd->lux = -1;

	lcd->display_on_delay_req = 0;

	ret = td4150_read_init_info(lcd);
	if (ret < 0)
		dev_info(&lcd->ld->dev, "%s: failed to init information\n", __func__);

	i2c_add_driver(&lm36274_i2c_driver);

	smcdsd_panel_set_brightness(lcd, 1);

	dev_info(&lcd->ld->dev, "- %s\n", __func__);

	return 0;
}

static ssize_t lcd_type_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct lcd_info *lcd = dev_get_drvdata(dev);

	sprintf(buf, "BOE_%02X%02X%02X\n", lcd->id_info.id[0], lcd->id_info.id[1], lcd->id_info.id[2]);

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
	}

	return size;
}

#if defined(CONFIG_SMCDSD_PROTOS_PLUS)
static ssize_t protos_control_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct mipi_dsi_lcd_parent *panel_data = NULL;
	struct lcd_info *lcd = dev_get_drvdata(dev);

	if (lcd == NULL) {
		dev_info(&lcd->ld->dev, "%s: panel is null\n", __func__);
		return -EINVAL;
	}
	else
		panel_data = lcd->pdata;

	snprintf(buf, PAGE_SIZE, "%d\n",
			panel_data->protos_mode);

	return strlen(buf);
}

static ssize_t protos_control_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t size)
{
	struct mipi_dsi_lcd_parent *panel_data = NULL;
	struct lcd_info *lcd = dev_get_drvdata(dev);
	int rc;
	u32 value, prev_protos_state = PROTOS_NONE;

	if (lcd == NULL) {
		dev_info(&lcd->ld->dev, "%s: panel is null\n", __func__);
		return -EINVAL;
	}
	else
		panel_data = lcd->pdata;

	rc = kstrtouint(buf, 0, &value);
	if (rc < 0)
		return rc;

	if (value >= PROTOS_MAX) {
		dev_info(&lcd->ld->dev, "%s: invalid protos_control: %d\n", __func__, value);
		return -EINVAL;
	}

	dev_info(&lcd->ld->dev, "%s: [%s(%d)] => [%s(%d)] ++\n",
		__func__,
		protos_state_names[panel_data->protos_mode], panel_data->protos_mode,
		protos_state_names[value], value);

	smcdsd_abd_save_str(&panel_data->abd, protos_state_names[value]);

	if (panel_data->protos_mode == value) {
		dev_info(dev, "%s: duplicated control: %u\n", __func__, value);
		return -EINVAL;
	}

	mutex_lock(&lcd->protos_lock);

	/* keep prev state */
	prev_protos_state = panel_data->protos_mode;
	/* set target state */
	panel_data->protos_mode = value;

	switch (value) {
	case PROTOS_NONE:
		dev_info(dev, "%s: PROTOS SCENARIO [%s(%d)]\n", __func__, protos_state_names[value], value);

		break;
	case PROTOS_READY:
		dev_info(dev, "%s: PROTOS SCENARIO [%s(%d)]\n", __func__, protos_state_names[value], value);

		break;
	case PROTOS_AWAKE:
		dev_info(dev, "%s: PROTOS SCENARIO [%s(%d)]\n", __func__, protos_state_names[value], value);

		smcdsd_set_doze(DOZE);

		if (prev_protos_state != PROTOS_READY && prev_protos_state != PROTOS_SLEEP) {
			dev_info(dev, "%s: invalid PROTOS SCENARIO [%s(%d)] => [%s(%d)]\n", __func__,
			protos_state_names[prev_protos_state], prev_protos_state,
			protos_state_names[value], value);
		}

		break;
	case PROTOS_SLEEP:
		dev_info(dev, "%s: PROTOS SCENARIO [%s(%d)]\n", __func__, protos_state_names[value], value);

		smcdsd_set_doze(DOZE_SUSPEND);

		if (prev_protos_state != PROTOS_AWAKE) {
			dev_info(dev, "%s: invalid PROTOS SCENARIO [%s(%d)] => [%s(%d)]\n", __func__,
			protos_state_names[prev_protos_state], prev_protos_state,
			protos_state_names[value], value);
		}

		break;
	}

	dev_info(dev, "%s: [%s(%d)] => [%s(%d)] --\n",
		__func__,
		protos_state_names[prev_protos_state], prev_protos_state,
		protos_state_names[value], value);

	smcdsd_abd_save_str(&panel_data->abd, protos_state_names[value]);

	mutex_unlock(&lcd->protos_lock);

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

static DEVICE_ATTR(lcd_type, 0444, lcd_type_show, NULL);
static DEVICE_ATTR(window_type, 0444, window_type_show, NULL);
static DEVICE_ATTR(brightness_table, 0444, brightness_table_show, NULL);
static DEVICE_ATTR(lux, 0644, lux_show, lux_store);
#if defined(CONFIG_SMCDSD_PROTOS_PLUS)
static DEVICE_ATTR(protos_control, 0644, protos_control_show, protos_control_store);
#endif

static struct attribute *lcd_sysfs_attributes[] = {
	&dev_attr_lcd_type.attr,
	&dev_attr_window_type.attr,
	&dev_attr_brightness_table.attr,
	&dev_attr_lux.attr,
#if defined(CONFIG_SMCDSD_DPUI)
	&dev_attr_dpui.attr,
	&dev_attr_dpui_dbg.attr,
#endif
#if defined(CONFIG_SMCDSD_PROTOS_PLUS)
	&dev_attr_protos_control.attr,
#endif
	NULL,
};

static const struct attribute_group lcd_sysfs_attr_group = {
	.attrs = lcd_sysfs_attributes,
};

static void lcd_init_sysfs(struct lcd_info *lcd)
{
	int ret = 0;

	ret = sysfs_create_group(&lcd->ld->dev.kobj, &lcd_sysfs_attr_group);
	if (ret < 0)
		dev_info(&lcd->ld->dev, "failed to add lcd sysfs\n");

	init_debugfs_backlight(lcd->bd, brightness_table, NULL);
}

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
	mutex_init(&lcd->protos_lock);

	ret = td4150_probe(lcd);
	if (ret < 0)
		dev_info(&lcd->ld->dev, "%s: failed to probe panel\n", __func__);

	lcd_init_sysfs(lcd);

	td4150_register_notifier(lcd);

	dev_info(&lcd->ld->dev, "%s: %s: done\n", kbasename(__FILE__), __func__);

exit:
	return ret;
}

static int smcdsd_panel_init(struct platform_device *p)
{
	struct lcd_info *lcd = get_lcd_info(p);

	dev_info(&lcd->ld->dev, "+ %s: %d  %d\n", __func__, lcd->state, lcd->pdata->protos_mode);

	if (lcd->state == PANEL_STATE_SUSPENED) {
		td4150_init(lcd);

		mutex_lock(&lcd->lock);
		lcd->state = PANEL_STATE_RESUMED;
		mutex_unlock(&lcd->lock);
	}

	dev_info(&lcd->ld->dev, "- %s: %d, %d\n", __func__, lcd->state, lcd->connected);

	return 0;
}

static int smcdsd_panel_i2c_init(struct platform_device *p)
{
	struct lcd_info *lcd = get_lcd_info(p);

	dev_info(&lcd->ld->dev, "+ %s\n", __func__);

	lm36274_array_write(LM36274_INIT, ARRAY_SIZE(LM36274_INIT));

	dev_info(&lcd->ld->dev, "- %s: %d, %d\n", __func__, lcd->state, lcd->connected);

	return 0;
}

static int smcdsd_panel_i2c_exit(struct platform_device *p)
{
	struct lcd_info *lcd = get_lcd_info(p);

	dev_info(&lcd->ld->dev, "+ %s\n", __func__);

	lm36274_array_write(LM36274_EXIT, ARRAY_SIZE(LM36274_EXIT));

	dev_info(&lcd->ld->dev, "- %s: %d, %d\n", __func__, lcd->state, lcd->connected);

	return 0;
}

static int smcdsd_panel_displayon_late(struct platform_device *p)
{
	struct lcd_info *lcd = get_lcd_info(p);

	dev_info(&lcd->ld->dev, "+ %s: %d  %d\n", __func__, lcd->state, lcd->pdata->protos_mode);

	mutex_lock(&lcd->lock);
	td4150_displayon(lcd);
	mutex_unlock(&lcd->lock);

	dev_info(&lcd->ld->dev, "- %s: %d, %d\n", __func__, lcd->state, lcd->connected);

	return 0;
}

static int smcdsd_panel_exit(struct platform_device *pdev)
{
	struct lcd_info *lcd = platform_get_drvdata(pdev);

	dev_info(&lcd->ld->dev, "+ %s: %d  %d\n", __func__, lcd->state, lcd->pdata->protos_mode);

	if (lcd->state == PANEL_STATE_SUSPENED)
		goto exit;

	td4150_exit(lcd);

	mutex_lock(&lcd->lock);
	lcd->state = PANEL_STATE_SUSPENED;
	mutex_unlock(&lcd->lock);

	dev_info(&lcd->ld->dev, "- %s: %d, %d\n", __func__, lcd->state, lcd->connected);

exit:
	return 0;
}

#if defined(CONFIG_SMCDSD_DOZE) && defined(CONFIG_SMCDSD_PROTOS_PLUS)
static int smcdsd_panel_initalpm(struct platform_device *p)
{
	struct lcd_info *lcd = get_lcd_info(p);

	dev_info(&lcd->ld->dev, "+ %s: %d  %d\n", __func__, lcd->state, lcd->pdata->protos_mode);

	if (lcd->pdata->protos_mode != PROTOS_SLEEP)
		DSI_WRITE(SEQ_DISPLAY_OFF, ARRAY_SIZE(SEQ_DISPLAY_OFF));

	DSI_WRITE(SEQ_SLEEP_IN, ARRAY_SIZE(SEQ_SLEEP_IN));
	msleep(70);/* wait 4 frame */

	dev_info(&lcd->ld->dev, "- %s: %d, %d\n", __func__, lcd->state, lcd->connected);

	return 0;
}

static int smcdsd_panel_exitalpm(struct platform_device *p)
{
	struct lcd_info *lcd = get_lcd_info(p);

	dev_info(&lcd->ld->dev, "+ %s: %d  %d\n", __func__, lcd->state, lcd->pdata->protos_mode);

	DSI_WRITE(SEQ_SLEEP_OUT, ARRAY_SIZE(SEQ_SLEEP_OUT));
	msleep(70);/* wait 4 frame */

	dev_info(&lcd->ld->dev, "- %s: %d, %d\n", __func__, lcd->state, lcd->connected);

	return 0;
}
#endif

#if defined(CONFIG_SMCDSD_PROTOS_PLUS)
static int smcdsd_get_curr_protos_mode(struct platform_device *pdev)
{
	struct lcd_info *lcd = platform_get_drvdata(pdev);

	return lcd->pdata->protos_mode;
}
#endif

struct mipi_dsi_lcd_driver td4150_mipi_lcd_driver = {
	.driver = {
		.name = "td4150",
	},
	.probe		= smcdsd_panel_probe,
	.init		= smcdsd_panel_init,
	.exit		= smcdsd_panel_exit,
	.displayon_late = smcdsd_panel_displayon_late,
	.panel_i2c_init = smcdsd_panel_i2c_init,
	.panel_i2c_exit = smcdsd_panel_i2c_exit,
#if defined(CONFIG_SMCDSD_DOZE) && defined(CONFIG_SMCDSD_PROTOS_PLUS)
	.doze_init	= smcdsd_panel_initalpm,
	.doze_exit	= smcdsd_panel_exitalpm,
#endif
	.set_mask	= NULL,
	.get_mask	= NULL,
	.framedone_notify	= NULL,
	.lock		= NULL,
#if defined(CONFIG_SMCDSD_PROTOS_PLUS)
	.get_curr_protos_mode = smcdsd_get_curr_protos_mode,
#endif
};

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