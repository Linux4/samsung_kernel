/* drivers/video/sprdfb/lcd_hx8389c_mipi.c
 *
 * Support for hx8389c mipi LCD device
 *
 * Copyright (C) 2014 Spreadtrum
 * Author: Haibing.Yang <haibing.yang@spreadtrum.com>
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */


#define pr_fmt(fmt)		"sprdfb_hx8389c: " fmt

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/bug.h>
#include <linux/delay.h>

#include "../sprdfb_panel.h"

#define MAX_DATA				100
#define DATA_TYPE_SET_RX_PKT_SIZE		0x37
#define PANEL_READ_CNT				4

#define PANEL_ID_LENGTH 			4
#define PANEL_RETURN_ID				0x3344
#define PANEL_ACTUAL_ID				0x8389
#define PANEL_ID_REG				0x04
#define PANEL_ID_OFFSET				1

#define PANEL_POWER_MODE_REG			0x0A
#define PANEL_POWER_MODE_LEN			2
#define PANEL_NOT_DEAD_STS			0x1C
#define PANEL_POWER_MODE_OFFSET			1

#define LCM_TAG_SHIFT				24
#define LCM_DATA_TYPE_SHIFT			16
#define LCM_TAG_MASK ((1 << LCM_DATA_TYPE_SHIFT) - 1)

#define LCM_TAG_SEND (1 << LCM_TAG_SHIFT)
#define LCM_TAG_SLEEP (2 << LCM_TAG_SHIFT)

#define LCM_SEND(len) (LCM_TAG_SEND | len)
#define LCM_SEND_WITH_TYPE(len, type) \
		(LCM_TAG_SEND | len | (type << LCM_DATA_TYPE_SHIFT))
#define LCM_SLEEP(ms) (LCM_TAG_SLEEP | ms)

struct panel_cmds {
	u32 tag;
	u8 data[MAX_DATA];
};

static struct panel_cmds hx8389c_init_cmds[] = {
	{
		LCM_SEND(06), {04, 0,
		0xB9, 0xFF, 0x83, 0x89}
	},
	{ /* Set power */
		LCM_SEND(22), {20, 0,
		0xB1, 0x7F, 0x10, 0x10, 0xF2, 0x92, 0x10, 0xFA,
		0x58, 0x80, 0x20, 0x20, 0xF8, 0xAA, 0xAA, 0xA3,
		0x00, 0x80, 0x30, 0x00}
	},
	{ /* Set display related register */
		LCM_SEND(13), {11, 0,
		0xB2, 0x80, 0x50, 0x05, 0x07, 0x40, 0x38, 0x11,
		0x64, 0x5D, 0x09}
	},
	{ /* Set panel driving timing */
		LCM_SEND(14), {12, 0,
		0xB4, 0x7A, 0x84, 0x82, 0x72, 0x00, 0x00, 0x10,
		0x7E, 0x10, 0x7E, 0xF0}
	},
	{LCM_SEND(2), {0xD2, 0x33}},
	{LCM_SLEEP(10)},
	/* SETPANEL CCh: can set normally-white/black panel */
	{LCM_SEND(2), {0xCC, 0x02}},
	{ /* Set GIP */
		LCM_SEND(38), {36, 0,
		0xD3, 0x00, 0x08, 0x00, 0x00, 0x00, 0x08, 0x00,
		0x32, 0x10, 0x00, 0x00, 0x00, 0x03, 0xC6, 0x03,
		0xC6, 0x00, 0x00, 0x00, 0x00, 0x35, 0x33, 0x04,
		0x04, 0x37, 0x00, 0x00, 0x00, 0x05, 0x08, 0x00,
		0x00, 0x0A, 0x00, 0x01}
	},
	{ /* Set GIP */
		LCM_SEND(41), {39, 0,
		0xD5, 0x18, 0x18, 0x18, 0x18, 0x19, 0x19, 0x18,
		0x18, 0x20, 0x21, 0x24, 0x25, 0x18, 0x18, 0x18,
		0x18, 0x00, 0x01, 0x04, 0x05, 0x02, 0x03, 0x06,
		0x07, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18,
		0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18}
	},
	{ /* Set GIP */
		LCM_SEND(41), {39, 0,
		0xD6, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x19,
		0x19, 0x25, 0x24, 0x21, 0x20, 0x18, 0x18, 0x18,
		0x18, 0x07, 0x06, 0x03, 0x02, 0x05, 0x04, 0x01,
		0x00, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18,
		0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18}
	},
	{ /* SETTE: Set internal TE function */
		LCM_SEND(6), {4, 0,
		0xB7, 0x20, 0x80, 0x00}
	},
	/* Set mDNIe Enable */
	{LCM_SEND(2), {0xCD, 0x5A}},
	{ /* Analog gamma setting */
		LCM_SEND(45), {43, 0,
		0xE0, 0x00, 0x01, 0x05, 0x33, 0x3A, 0x3F, 0x17,
		0x40, 0x06, 0x0A, 0x0C, 0x17, 0x0F, 0x12, 0x15,
		0x13, 0x14, 0x06, 0x11, 0x13, 0x18, 0x00, 0x01,
		0x05, 0x33, 0x3A, 0x3F, 0x17, 0x40, 0x06, 0x09,
		0x0C, 0x17, 0x0E, 0x12, 0x14, 0x12, 0x13, 0x06,
		0x11, 0x13, 0x18}
	},
#if 0
	{LCM_SEND(2), {0xBD, 0x00}}, /* BANK: RED */
	{
		LCM_SEND(46), {44, 0,
		0xC1, 0x01, 0x00, 0x08, 0x10, 0x18, 0x20, 0x28,
		0x30, 0x38, 0x40, 0x48, 0x50, 0x58, 0x60, 0x68,
		0x70, 0x78, 0x80, 0x88, 0x90, 0x98, 0xA0, 0xA8,
		0xB0, 0xB8, 0xC0, 0xC8, 0xD0, 0xD8, 0xE0, 0xE8,
		0xF0, 0xF8, 0xFF, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00
		}
	},
	{LCM_SEND(2), {0xBD, 0x01}}, /* BANK: GREEN */
	{
		LCM_SEND(45), {43, 0,
		0xC1, 0x00, 0x08, 0x10, 0x18, 0x20, 0x28, 0x30,
		0x38, 0x40, 0x48, 0x50, 0x58, 0x60, 0x68, 0x70,
		0x78, 0x80, 0x88, 0x90, 0x98, 0xA0, 0xA8, 0xB0,
		0xB8, 0xC0, 0xC8, 0xD0, 0xD8, 0xE0, 0xE8, 0xF0,
		0xF8, 0xFF, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00,
		}
	},
	{LCM_SEND(2), {0xBD, 0x02}}, /* BANK: BLUE */
	{
		LCM_SEND(46), {44, 0,
		0xC1, 0x00, 0x08, 0x10, 0x18, 0x20, 0x28, 0x30,
		0x38, 0x40, 0x48, 0x50, 0x58, 0x60, 0x68, 0x70,
		0x78, 0x80, 0x88, 0x90, 0x98, 0xA0, 0xA8, 0xB0,
		0xB8, 0xC0, 0xC8, 0xD0, 0xD8, 0xE0, 0xE8, 0xF0,
		0xF8, 0xFF, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00,
		}
	},
#endif
	/* Set_address_mode (36h): RGB order and source/gate dir */
	{LCM_SEND(2), {0x36, 0x03}},

	/* sleep out */
	{LCM_SEND(1), {0x11}},
	{LCM_SLEEP(120)},
	{LCM_SEND(1), {0x29}},
	{LCM_SLEEP(10)},
};

static struct panel_cmds sleep_in[] = {
	{LCM_SEND(1), {0x28}},
	{LCM_SLEEP(10)},
	{LCM_SEND(1), {0x10}},
	{LCM_SLEEP(120)},
};

static struct panel_cmds sleep_out[] = {
	{LCM_SEND(1), {0x11}},
	{LCM_SLEEP(120)},
	{LCM_SEND(1), {0x29}},
	{LCM_SLEEP(10)},
};

int sprdfb_dsi_tx_cmds(struct panel_spec *panel,
		struct panel_cmds *cmds, u32 cmds_len, bool force_lp)
{
	int i, time;
	struct ops_mipi *ops = panel->info.mipi->ops;
	u16 work_mode = panel->info.mipi->work_mode;
	u8 write_type = (cmds->tag >> 16) & 0xff;

	pr_info("%s, len: %d\n", __func__, cmds_len);

	if (!ops->mipi_set_cmd_mode ||
			!ops->mipi_dcs_write ||
			!ops->mipi_set_hs_mode ||
			!ops->mipi_set_lp_mode ||
			!ops->mipi_force_write) {
		pr_err("%s: Expected functions are NULL.\n", __func__);
		return -EFAULT;
	}
	if (force_lp) {
		if (work_mode == SPRDFB_MIPI_MODE_CMD)
			ops->mipi_set_lp_mode();
		else
			ops->mipi_set_data_lp_mode();
	}

	ops->mipi_set_cmd_mode();

	for (i = 0; i < cmds_len; i++) {
		if (cmds->tag & LCM_TAG_SEND) {
			if (write_type)
				ops->mipi_force_write(write_type, cmds->data,
						cmds->tag & LCM_TAG_MASK);
			else
				ops->mipi_dcs_write(cmds->data, cmds->tag & LCM_TAG_MASK);
		} else if (cmds->tag & LCM_TAG_SLEEP) {
			time = (cmds->tag & LCM_TAG_MASK) * 1000;
			usleep_range(time, time);
		}
		cmds++;
	}

	if (force_lp) {
		if (work_mode == SPRDFB_MIPI_MODE_CMD)
			ops->mipi_set_hs_mode();
		else
			ops->mipi_set_data_hs_mode();
	}
	return 0;
}

int sprdfb_dsi_rx_cmds(struct panel_spec *panel,
		u8 regs, u16 get_len, u8 *data, bool in_bllp)
{
	int ret;
	struct ops_mipi *ops = panel->info.mipi->ops;
	u8 pkt_size[2];
	u16 work_mode = panel->info.mipi->work_mode;

	pr_debug("%s, read data len: %d\n", __func__, get_len);
	if (!data) {
		pr_err("Expected registers or data is NULL.\n");
		return -EIO;
	}
	if (!ops->mipi_set_cmd_mode ||
			!ops->mipi_force_read ||
			!ops->mipi_force_write) {
		pr_err("%s: Expected functions are NULL.\n", __func__);
		return -EFAULT;
	}

	if (!in_bllp) {
		if (work_mode == SPRDFB_MIPI_MODE_CMD)
			ops->mipi_set_lp_mode();
		else
			ops->mipi_set_data_lp_mode();
		/*
		 * If cmds is transmitted in LP mode, not in BLLP,
		 * commands mode should be enabled
		 * */
		ops->mipi_set_cmd_mode();
	}

	pkt_size[0] = get_len & 0xff;
	pkt_size[1] = (get_len >> 8) & 0xff;

	ops->mipi_force_write(DATA_TYPE_SET_RX_PKT_SIZE,
			pkt_size, sizeof(pkt_size));

	ret = ops->mipi_force_read(regs, get_len, data);
	if (ret < 0) {
		pr_err("%s: dsi read id fail\n", __func__);
		return -EINVAL;
	}

	if (!in_bllp) {
		if (work_mode == SPRDFB_MIPI_MODE_CMD)
			ops->mipi_set_hs_mode();
		else
			ops->mipi_set_data_hs_mode();
	}

	return 0;
}

int sprdfb_dsi_panel_sleep(struct panel_spec *panel, u8 is_sleep)
{
	int len;
	struct panel_cmds *sleep_inout;

	if (is_sleep) {
		sleep_inout = sleep_in;
		len = ARRAY_SIZE(sleep_in);
	} else {
		sleep_inout = sleep_out;
		len = ARRAY_SIZE(sleep_out);
	}

	return sprdfb_dsi_tx_cmds(panel, sleep_inout, len, false);
}

u32 sprdfb_dsi_panel_readid(struct panel_spec *panel)
{
	int i, cnt = PANEL_READ_CNT;
	u8 regs = PANEL_ID_REG;
	u8 data[PANEL_ID_LENGTH] = { 0 };
	u8 id[PANEL_ID_LENGTH] = { 0 };

	i = PANEL_ID_OFFSET;
	id[i++] = PANEL_RETURN_ID & 0xFF;
	id[i] = (PANEL_RETURN_ID >> 8) & 0xFF;
	do {
		sprdfb_dsi_rx_cmds(panel, regs, PANEL_ID_LENGTH, data, true);
		i = PANEL_ID_OFFSET;
		pr_debug("%s: reading panel id(0x%2x) is 0x%x, 0x%x",
				__func__, regs, data[i], data[i+1]);
		if (data[i] == id[i] && data[i + 1] == id[i + 1])
			return PANEL_ACTUAL_ID;
	} while (cnt-- > 0);

	pr_err("%s: failed to read panel id, 0x%x, 0x%x.\n",
			__func__, data[i], data[i + 1]);
	return 0;
}

static int hx8389c_dsi_panel_dead(struct panel_spec *panel)
{
	int i, cnt = PANEL_READ_CNT;
	u8 regs = PANEL_POWER_MODE_REG;
	u8 data[PANEL_POWER_MODE_LEN] = { 0 };
	bool in_bllp = false;

#ifdef FB_CHECK_ESD_IN_VFP
	in_bllp = true;
#endif
	do {
		sprdfb_dsi_rx_cmds(panel, regs, PANEL_POWER_MODE_LEN, data, in_bllp);
		i = PANEL_POWER_MODE_OFFSET;
		pr_debug("%s: reading panel power mode(0x%2x) is 0x%x",
				__func__, regs, data[i]);
		if ((data[i] & PANEL_NOT_DEAD_STS) == PANEL_NOT_DEAD_STS)
			return true;
	} while (cnt-- > 0);

	pr_err("panel is dead, read power mode is 0x%x.\n", data[i]);
	return false;
}

static int hx8389c_dsi_panel_init(struct panel_spec *panel)
{
	return sprdfb_dsi_tx_cmds(panel, hx8389c_init_cmds,
			ARRAY_SIZE(hx8389c_init_cmds), false);
}

static struct panel_operations lcd_hx8389c_mipi_ops = {
	.panel_init = hx8389c_dsi_panel_init,
	.panel_readid = sprdfb_dsi_panel_readid,
	.panel_enter_sleep = sprdfb_dsi_panel_sleep,
	.panel_esd_check = hx8389c_dsi_panel_dead,
};

static struct timing_rgb lcd_hx8389c_mipi_timing = {
	.hfp = 20, /* unit: pixel */
	.hbp = 20,
	.hsync = 30,
	.vfp = 9, /*unit: line */
	.vbp = 5,
	.vsync = 2,
};

static struct info_mipi lcd_hx8389c_mipi_info = {
	.work_mode  = SPRDFB_MIPI_MODE_VIDEO,
	.video_bus_width = 24,
	.lan_number = 2,
	.phy_feq = 500000,
	.h_sync_pol = SPRDFB_POLARITY_POS,
	.v_sync_pol = SPRDFB_POLARITY_POS,
	.de_pol = SPRDFB_POLARITY_POS,
	.te_pol = SPRDFB_POLARITY_POS,
	.color_mode_pol = SPRDFB_POLARITY_POS,
	.shut_down_pol = SPRDFB_POLARITY_POS,
	.timing = &lcd_hx8389c_mipi_timing,
	.ops = NULL,
};

static struct panel_spec lcd_hx8389c_mipi_spec = {
	.width = 540,
	.height = 960,
	.fps = 60,
	.type = LCD_MODE_DSI,
	.direction = LCD_DIRECT_NORMAL,
	.info = {
		.mipi = &lcd_hx8389c_mipi_info
	},
	.ops = &lcd_hx8389c_mipi_ops,
};

static struct panel_cfg lcd_hx8389c_mipi = {
	.dev_id = SPRDFB_MAINLCD_ID,
	.lcd_id = PANEL_ACTUAL_ID,
	.lcd_name = "lcd_hx8389c_mipi",
	.panel = &lcd_hx8389c_mipi_spec,
};

static int __init lcd_hx8389c_mipi_init(void)
{
	return sprdfb_panel_register(&lcd_hx8389c_mipi);
}
subsys_initcall(lcd_hx8389c_mipi_init);
