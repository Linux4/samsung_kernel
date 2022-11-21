/*
 * linux/drivers/video/pxa688_phy.c -- Marvell PXA168 LCD Controller
 *
 *  Copyright (C) 2008 Marvell International Ltd.
 *  All rights reserved.
 *
 *  2009-02-16  adapted from original version for PXA168
 *		Kevin Liu <kliu5@marvell.com>
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License. See the file COPYING in the main directory of this archive for
 * more details.
 */
#include <linux/delay.h>
#include <linux/clk.h>

#ifdef CONFIG_PXA688_PHY
#include "pxa168fb_common.h"

/* dsi phy timing */
static struct dsi_phy phy = {
	.hs_prep_constant	= 40,    /* Unit: ns. */
	.hs_prep_ui		= 4,
	.hs_zero_constant	= 145,
	.hs_zero_ui		= 10,
	.hs_trail_constant	= 0,
	.hs_trail_ui		= 64,
	.hs_exit_constant	= 100,
	.hs_exit_ui		= 0,
	.ck_zero_constant	= 300,
	.ck_zero_ui		= 0,
	.ck_trail_constant	= 60,
	.ck_trail_ui		= 0,
	.req_ready		= 0x3c,
	.wakeup_constant        = 1000000,
	.wakeup_ui      = 0,
	.lpx_constant       = 50,
	.lpx_ui     = 0,
};

#define dsi_ex_pixel_cnt		0
#define dsi_hex_en			0
/* (Unit: Mhz) */
#define dsi_hsclk			(clk_get_rate(fbi->clk)/1000000)
#define dsi_lpclk			3

#define to_dsi_bcnt(timing, bpp)	(((timing) * (bpp)) >> 3)

static unsigned int dsi_lane[5] = {0, 0x1, 0x3, 0x7, 0xf};

static unsigned char dsi_bit(unsigned int index, unsigned char *pdata)
{
	unsigned char ret;
	unsigned int cindex, bindex;
	cindex = index / 8;
	bindex = index % 8;

	if (pdata[cindex] & (0x1 << bindex))
		ret = (unsigned char) 0x1;
	else
		ret = (unsigned char) 0x0;

	return ret;
}

static unsigned char calculate_ecc(unsigned char *pdata)
{
	unsigned char ret;
	unsigned char p[8];

	p[7] = (unsigned char) 0x0;
	p[6] = (unsigned char) 0x0;

	p[5] = (unsigned char) (
	(
		dsi_bit(10, pdata) ^ \
		dsi_bit(11, pdata) ^ \
		dsi_bit(12, pdata) ^ \
		dsi_bit(13, pdata) ^ \
		dsi_bit(14, pdata) ^ \
		dsi_bit(15, pdata) ^ \
		dsi_bit(16, pdata) ^ \
		dsi_bit(17, pdata) ^ \
		dsi_bit(18, pdata) ^ \
		dsi_bit(19, pdata) ^ \
		dsi_bit(21, pdata) ^ \
		dsi_bit(22, pdata) ^ \
		dsi_bit(23, pdata)
		)
	);
	p[4] = (unsigned char) (
		dsi_bit(4, pdata) ^ \
		dsi_bit(5, pdata) ^ \
		dsi_bit(6, pdata) ^ \
		dsi_bit(7, pdata) ^ \
		dsi_bit(8, pdata) ^ \
		dsi_bit(9, pdata) ^ \
		dsi_bit(16, pdata) ^ \
		dsi_bit(17, pdata) ^ \
		dsi_bit(18, pdata) ^ \
		dsi_bit(19, pdata) ^ \
		dsi_bit(20, pdata) ^ \
		dsi_bit(22, pdata) ^ \
		dsi_bit(23, pdata)
	);
	p[3] = (unsigned char) (
	(
		dsi_bit(1, pdata) ^ \
		dsi_bit(2, pdata) ^ \
		dsi_bit(3, pdata) ^ \
		dsi_bit(7, pdata) ^ \
		dsi_bit(8, pdata) ^ \
		dsi_bit(9, pdata) ^ \
		dsi_bit(13, pdata) ^ \
		dsi_bit(14, pdata) ^ \
		dsi_bit(15, pdata) ^ \
		dsi_bit(19, pdata) ^ \
		dsi_bit(20, pdata) ^ \
		dsi_bit(21, pdata) ^ \
		dsi_bit(23, pdata)
		)
	);
	p[2] = (unsigned char) (
	(
		dsi_bit(0, pdata) ^ \
		dsi_bit(2, pdata) ^ \
		dsi_bit(3, pdata) ^ \
		dsi_bit(5, pdata) ^ \
		dsi_bit(6, pdata) ^ \
		dsi_bit(9, pdata) ^ \
		dsi_bit(11, pdata) ^ \
		dsi_bit(12, pdata) ^ \
		dsi_bit(15, pdata) ^ \
		dsi_bit(18, pdata) ^ \
		dsi_bit(20, pdata) ^ \
		dsi_bit(21, pdata) ^ \
		dsi_bit(22, pdata)
		)
	);
	p[1] = (unsigned char) (
		(
		dsi_bit(0, pdata) ^ \
		dsi_bit(1, pdata) ^ \
		dsi_bit(3, pdata) ^ \
		dsi_bit(4, pdata) ^ \
		dsi_bit(6, pdata) ^ \
		dsi_bit(8, pdata) ^ \
		dsi_bit(10, pdata) ^ \
		dsi_bit(12, pdata) ^ \
		dsi_bit(14, pdata) ^ \
		dsi_bit(17, pdata) ^ \
		dsi_bit(20, pdata) ^ \
		dsi_bit(21, pdata) ^ \
		dsi_bit(22, pdata) ^ \
		dsi_bit(23, pdata)
		)
	);
	p[0] = (unsigned char) (
		(
		dsi_bit(0, pdata) ^ \
		dsi_bit(1, pdata) ^ \
		dsi_bit(2, pdata) ^ \
		dsi_bit(4, pdata) ^ \
		dsi_bit(5, pdata) ^ \
		dsi_bit(7, pdata) ^ \
		dsi_bit(10, pdata) ^ \
		dsi_bit(11, pdata) ^ \
		dsi_bit(13, pdata) ^ \
		dsi_bit(16, pdata) ^ \
		dsi_bit(20, pdata) ^ \
		dsi_bit(21, pdata) ^ \
		dsi_bit(22, pdata) ^ \
		dsi_bit(23, pdata)
		)
	);
	ret = (unsigned char)(
		p[0] | \
		(p[1] << 0x1) | \
		(p[2] << 0x2) | \
		(p[3] << 0x3) | \
		(p[4] << 0x4) | \
		(p[5] << 0x5)
	);
	return   ret;
}

static unsigned short gs_crc16_generation_code = 0x8408;
static unsigned short calculate_crc16(unsigned char *pdata, unsigned
		short count)
{
	unsigned short byte_counter;
	unsigned char bit_counter;
	unsigned char data;
	unsigned short crc16_result = 0xFFFF;
	if (count > 0) {
		for (byte_counter = 0; byte_counter < count;
			byte_counter++) {
			data = *(pdata + byte_counter);
			for (bit_counter = 0; bit_counter < 8; bit_counter++) {
				if (((crc16_result & 0x0001) ^ ((0x0001 *
					data) & 0x0001)) > 0)
					crc16_result = ((crc16_result >> 1)
					& 0x7FFF) ^ gs_crc16_generation_code;
				else
					crc16_result = (crc16_result >> 1)
					& 0x7FFF;
				data = (data >> 1) & 0x7F;
			}
		}
	}
	return crc16_result;
}

static void dsi_send_cmds(struct pxa168fb_info *fbi, u8 *parameter,
		u8 count, u8 lp)
{
	struct pxa168fb_mach_info *mi = fbi->dev->platform_data;
	struct dsi_info *di = (struct dsi_info *)mi->phy_info;
	struct dsi_regs *dsi = (struct dsi_regs *)di->regs;
	u32 send_data = 0, waddr, i, loop;

	/* write all packet bytes to packet data buffer */
	for (i = 0; i < count; i++) {
		send_data |= parameter[i] << ((i % 4) * 8);
		if (!((i + 1) % 4)) {
			writel(send_data, &dsi->dat0);
			waddr = DSI_CFG_CPU_DAT_REQ_MASK |
				DSI_CFG_CPU_DAT_RW_MASK |
				((i - 3) << DSI_CFG_CPU_DAT_ADDR_SHIFT);
			writel(waddr, &dsi->cmd3);
			loop = 1000;
			/* wait write operation done */
			while (readl(&dsi->cmd3) & DSI_CFG_CPU_DAT_REQ_MASK) {
				//msleep(1);
				loop--;
				if (loop <= 0){
				   pr_err("%s error!\n", __func__);
				   break;
				}
			}
			send_data = 0;
		}
	}

	/* handle last none 4Byte align data */
	if (i % 4) {
		writel(send_data, &dsi->dat0);
		waddr = DSI_CFG_CPU_DAT_REQ_MASK |
			DSI_CFG_CPU_DAT_RW_MASK |
			((4 * (i / 4)) << DSI_CFG_CPU_DAT_ADDR_SHIFT);
		writel(waddr, &dsi->cmd3);
		loop = 1000;
		/* wait write operation done */
		while (readl(&dsi->cmd3) & DSI_CFG_CPU_DAT_REQ_MASK) {
			//msleep(1);
			loop--;
			if (loop <= 0) {
				pr_err("%s error!\n", __func__);
				break;
			}
		}

		send_data = 0;
	}

	/* send out the packet */
	if (count == 4)
		waddr = DSI_CFG_CPU_SP_MASK;
	else {
		#if 0
		/*
		 * To be double confirmed:
		 * re-send the packet header for long packet????
		 */
		for (i = 0; i < 4; i++) {
			send_data |= parameter[i] << ((i % 4) * 8);
			if (!((i + 1) % 4)) {
				writel(send_data, &dsi->dat0);
				waddr = DSI_CFG_CPU_DAT_REQ_MASK |
					DSI_CFG_CPU_DAT_RW_MASK |
					((i - 3) << DSI_CFG_CPU_DAT_ADDR_SHIFT);
				writel(waddr, &dsi->cmd3);
				/* wait write operation done */
				while (readl(&dsi->cmd3) &
						DSI_CFG_CPU_DAT_REQ_MASK)
					msleep(1);
				send_data = 0;
			}
		}
		#endif
		waddr = 0;
	}
	waddr |= DSI_CFG_CPU_CMD_REQ_MASK |
		#if !defined(CONFIG_MACH_LT02) && !defined(CONFIG_MACH_COCOA7)
		DSI_CFG_CPU_TURN_MASK |
		#endif
		(lp ? DSI_CFG_CPU_TXLP_MASK : 0) |
		(count << DSI_CFG_CPU_WC_SHIFT);

	/* send out the packet */
	writel(waddr, &dsi->cmd0);
	loop = 10000;
	/* wait packet be sent out */
	while (readl(&dsi->cmd0) & DSI_CFG_CPU_CMD_REQ_MASK) {
		//msleep(1);
		loop--;
		if (loop <= 0){
			pr_err("%s error!\n", __func__);
			break;
		}
	}
}

void dsi_cmd_array_tx(struct pxa168fb_info *fbi, struct dsi_cmd_desc cmds[],
		int count)
{
	struct dsi_cmd_desc cmd_line;
	u8 command, parameter[DSI_MAX_DATA_BYTES], len;
	u32 crc, loop;

	for (loop = 0; loop < count; loop++) {
		cmd_line = cmds[loop];
		command = cmd_line.data_type;
		len = cmd_line.length;
		memset(parameter, 0x00, len + 6);
		parameter[0] = command & 0xff;
		switch (command) {
		case DSI_DI_DCS_SWRITE:
		case DSI_DI_DCS_SWRITE1:
		case DSI_DI_DCS_READ:
		case DSI_DI_SET_MAX_PKT_SIZE:
			memcpy(&parameter[1], cmd_line.data, len);
			len = 4;
			break;
		case  DSI_DI_DCS_LWRITE:
			parameter[1] = len & 0xff;
			parameter[2] = 0;
			memcpy(&parameter[4], cmd_line.data, len);
			crc = calculate_crc16(&parameter[4], len);
			parameter[len + 4] = crc & 0xff;
			parameter[len + 5] = (crc >> 8) & 0xff;
			len += 6;
			break;
		case  DSI_DI_DCS_GEN_LWRITE:
			parameter[1] = len & 0xff;
			parameter[2] = 0;
			memcpy(&parameter[4], cmd_line.data, len);
			crc = calculate_crc16(&parameter[4], len);
			parameter[len + 4] = crc & 0xff;
			parameter[len + 5] = (crc >> 8) & 0xff;
			len += 6;
			break;
			
		default:
			pr_err("%s: data type not supported 0x%x\n",
					__func__, command);
			break;
		}
		parameter[3] = calculate_ecc(parameter);

		/* send dsi commands */
		dsi_send_cmds(fbi, parameter, len, cmd_line.lp);

		if (cmd_line.delay)
			mdelay(cmd_line.delay);
	}
}
#ifdef VSYNC_DSI_CMD

/*
	dsi_prepare_cmd_array_tx()
	Prepare DSI Command packet
	fbi		: Current LCD
	cmds	: command array pointer
	count	: How many commands in the command array
	buffer	: Should be allocated by caller with count * DSI_MAX_DATA_BYTES
	packet_len[] : Should be allocated by caller with count
*/
void dsi_prepare_cmd_array_tx(struct pxa168fb_info *fbi, struct dsi_cmd_desc cmds[],
		int count, u8 *buffer, u8 *packet_len)
{
	struct dsi_cmd_desc cmd_line;
	u8 command,len;
	u32 crc, loop;
	u8 *parameter;
	
	//printk("%s\n", __func__);
	for (loop = 0; loop < count; loop++) {
		parameter = buffer + (DSI_MAX_DATA_BYTES*loop);
		cmd_line = cmds[loop];
		command = cmd_line.data_type;
		len = cmd_line.length;
		memset(parameter, 0x00, len + 6);
		parameter[0] = command & 0xff;
		switch (command) {
		case DSI_DI_DCS_SWRITE:
		case DSI_DI_DCS_SWRITE1:
		case DSI_DI_DCS_READ:
		case DSI_DI_SET_MAX_PKT_SIZE:
			memcpy(&parameter[1], cmd_line.data, len);
			len = 4;
			break;
		case  DSI_DI_DCS_LWRITE:
			parameter[1] = len & 0xff;
			parameter[2] = 0;
			memcpy(&parameter[4], cmd_line.data, len);
			crc = calculate_crc16(&parameter[4], len);
			parameter[len + 4] = crc & 0xff;
			parameter[len + 5] = (crc >> 8) & 0xff;
			len += 6;
			break;
		default:
			pr_err("%s: data type not supported 0x%x\n",
					__func__, command);
			break;
		}
		parameter[3] = calculate_ecc(buffer);
		packet_len[loop] = len;
	}
}

/*
	dsi_send_prepared_cmd_tx()
	Send one data cmd to DSI which is prepared by dsi_prepare_cmd_array_tx()
	
	eg)
		count is dsi_prepare_cmd_array_tx()'s count
		int loop
		for (loop = 0; loop < count; loop++)
			dsi_send_prepared_cmd_tx(fbi, buffer[loop], packet_len[loop])
			
	fbi 	: Current LCD
	cmds	: command array pointer
	count	: How many commands in the command array
	buffer	: got from dsi_prepare_cmd_array_tx
	len 		: Sgot from dsi_prepare_cmd_array_tx
*/

void dsi_send_prepared_cmd_tx(struct pxa168fb_info *fbi, struct dsi_cmd_desc cmds,
	u8 *buffer,u8 len)
{
	//printk("%s : len = %d\n", __func__,len);
	dsi_send_cmds(fbi, buffer, len, cmds.lp);
	
	if (cmds.delay)
		mdelay(cmds.delay);
}
#endif
void dsi_cclk_set(struct pxa168fb_info *fbi, int en)
{
	struct pxa168fb_mach_info *mi = fbi->dev->platform_data;
	struct dsi_info *di = (struct dsi_info *)mi->phy_info;
	struct dsi_regs *dsi = (struct dsi_regs *)di->regs;

	if (en)
		writel(0x1, &dsi->phy_ctrl1);
	else
		writel(0x0, &dsi->phy_ctrl1);
}

void dsi_lanes_enable(struct pxa168fb_info *fbi, int en)
{
	struct pxa168fb_mach_info *mi = fbi->dev->platform_data;
	struct dsi_info *di = (struct dsi_info *)mi->phy_info;
	struct dsi_regs *dsi = (struct dsi_regs *)di->regs;
	u32 reg = readl(&dsi->phy_ctrl2) & ~(0xf << 4);

	reg &= ~(0xf << 4);
	if (en)
		reg |= (dsi_lane[di->lanes] << 4);

	pr_debug("%s %d: phy_ctrl2 0x%x\n", __func__, en, reg);
	writel(reg, &dsi->phy_ctrl2);
}

void dsi_set_dphy(struct pxa168fb_info *fbi)
{
	struct pxa168fb_mach_info *mi = fbi->dev->platform_data;
	struct dsi_info *di = (struct dsi_info *)mi->phy_info;
	struct dsi_regs *dsi = (struct dsi_regs *)di->regs;
	int ui, lpx_clk, lpx_time, ta_get, ta_go, wakeup, reg;
	int hs_prep, hs_zero, hs_trail, hs_exit, ck_zero, ck_trail, ck_exit;

	ui = 1000/dsi_hsclk + 1;
	pr_debug("ui:%d\n", ui);

	lpx_clk = (phy.lpx_constant + phy.lpx_ui * ui) / DSI_ESC_CLK_T;
	lpx_time = (lpx_clk + 1) * DSI_ESC_CLK_T;
	pr_debug("lpx_clk:%d, condition (TIME_LPX:%d > 50)\n",
		lpx_clk, lpx_time);

	/* Below is for NT35451 */
	ta_get = lpx_time * 5 / DSI_ESC_CLK_T - 1;
	ta_go = lpx_time * 4 / DSI_ESC_CLK_T - 1;
	pr_debug("ta_get:%d, condition (TIME_TA_GET:%d == 5*TIME_LPX:%d)\n",
		ta_get, (ta_get + 1) * DSI_ESC_CLK_T, lpx_time * 5);
	pr_debug("ta_go:%d, condition (TIME_TA_GO:%d == 4*TIME_LPX:%d)\n",
		ta_go, (ta_go + 1) * DSI_ESC_CLK_T, lpx_time * 4);

	wakeup = phy.wakeup_constant;
	wakeup = wakeup / DSI_ESC_CLK_T + 1;
	pr_debug("wakeup:%d, condition (WAKEUP:%d > MIN:%d)\n",
		wakeup, (wakeup + 1) * DSI_ESC_CLK_T, 1000000);

	hs_prep = phy.hs_prep_constant + phy.hs_prep_ui * ui;
	hs_prep = hs_prep / DSI_ESC_CLK_T + 1;
	pr_debug("hs_prep:%d, condition (HS_PREP_MAX:%d > HS_PREP:%d "
		"> HS_PREP_MIN:%d)\n", hs_prep, 85 + 6 * ui,
		(hs_prep + 1) * DSI_ESC_CLK_T, 40 + 4 * ui);

	/* Our hardware added 3-byte clk automatically.
	 * 3-byte 3 * 8 * ui.
	 */
	hs_zero = phy.hs_zero_constant + phy.hs_zero_ui * ui -
		(hs_prep + 1) * DSI_ESC_CLK_T;
	hs_zero = (hs_zero - (3 * ui << 3)) / DSI_ESC_CLK_T + 4;
	if (hs_zero < 0)
		hs_zero = 0;
	pr_debug("hs_zero:%d, condition (HS_ZERO + HS_PREP:%d > SUM_MIN:%d)\n",
		hs_zero, (hs_zero - 2) * DSI_ESC_CLK_T + 24 * ui +
		(hs_prep + 1) * DSI_ESC_CLK_T, 145 + 10 * ui);

	hs_trail = phy.hs_trail_constant + phy.hs_trail_ui * ui;
	hs_trail = hs_trail / DSI_ESC_CLK_T + 1;
	pr_debug("hs_trail:%d, condition (HS_TRAIL:%d > MIN1:%d / MIN2:%d "
		"/ MIN3:%d)\n", hs_trail, (hs_trail + 1) * DSI_ESC_CLK_T,
		8 * ui, 60 + 4 * ui, 64 * ui);

	hs_exit = phy.hs_exit_constant + phy.hs_exit_ui * ui;
	hs_exit = hs_exit / DSI_ESC_CLK_T + 1;
	pr_debug("hs_exit:%d, condition (HS_EXIT:%d > MIN:%d)\n",
		hs_exit, (hs_exit + 1) * DSI_ESC_CLK_T, 100);

	ck_zero = phy.ck_zero_constant + phy.ck_zero_ui * ui -
		(hs_prep + 1) * DSI_ESC_CLK_T;
	ck_zero = ck_zero / DSI_ESC_CLK_T + 1;
	pr_debug("ck_zero:%d, condition (CK_ZERO + CK_PREP:%d > SUM_MIN:%d)\n",
		ck_zero, (ck_zero + 1) * DSI_ESC_CLK_T +
		(hs_prep + 1) * DSI_ESC_CLK_T, 300);

	ck_trail = phy.ck_trail_constant + phy.ck_trail_ui * ui;
	ck_trail = ck_trail / DSI_ESC_CLK_T + 1;
	pr_debug("ck_trail:%d, condition (CK_TRIAL:%d > MIN:%d)\n",
		ck_trail, (ck_trail + 1) * DSI_ESC_CLK_T, 60);

	ck_exit = hs_exit;
	pr_debug("ck_exit:%d\n", ck_exit);

	/* bandgap ref enable */
	reg = readl(&dsi->phy_rcomp0);
	reg |= (1 << 9);
	writel(reg, &dsi->phy_rcomp0);

	/* timing_0 */
	reg = (hs_exit << DSI_PHY_TIME_0_CFG_CSR_TIME_HS_EXIT_SHIFT)
		| (hs_trail << DSI_PHY_TIME_0_CFG_CSR_TIME_HS_TRAIL_SHIFT)
		| (hs_zero << DSI_PHY_TIME_0_CDG_CSR_TIME_HS_ZERO_SHIFT)
		| (hs_prep);
	writel(reg, &dsi->phy_timing0);

	reg = (ta_get << DSI_PHY_TIME_1_CFG_CSR_TIME_TA_GET_SHIFT)
		| (ta_go << DSI_PHY_TIME_1_CFG_CSR_TIME_TA_GO_SHIFT)
		| wakeup;
	writel(reg, &dsi->phy_timing1);

	reg = (ck_exit << DSI_PHY_TIME_2_CFG_CSR_TIME_CK_EXIT_SHIFT)
		| (ck_trail << DSI_PHY_TIME_2_CFG_CSR_TIME_CK_TRAIL_SHIFT)
		| (ck_zero << DSI_PHY_TIME_2_CFG_CSR_TIME_CK_ZERO_SHIFT)
		| lpx_clk;
	writel(reg, &dsi->phy_timing2);


	reg = (lpx_clk << DSI_PHY_TIME_3_CFG_CSR_TIME_LPX_SHIFT) | \
	      phy.req_ready;
	writel(reg, &dsi->phy_timing3);

	/* calculated timing on brownstone:
	 * DSI_PHY_TIME_0 0x06080204
	 * DSI_PHY_TIME_1 0x6d2bfff0
	 * DSI_PHY_TIME_2 0x603130a
	 * DSI_PHY_TIME_3 0xa3c
	 */
}

void dsi_reset(struct pxa168fb_info *fbi, int hold)
{
	struct pxa168fb_mach_info *mi = fbi->dev->platform_data;
	struct dsi_info *di = (struct dsi_info *)mi->phy_info;
	struct dsi_regs *dsi = (struct dsi_regs *)di->regs;
	volatile unsigned int reg;

	pr_debug("%s\n", __func__);
	writel(0x0, &dsi->ctrl0);
	reg = readl(&dsi->ctrl0);
	reg |= DSI_CTRL_0_CFG_SOFT_RST | DSI_CTRL_0_CFG_SOFT_RST_REG;

	if (!hold) {
		writel(reg, &dsi->ctrl0);
		reg &= ~(DSI_CTRL_0_CFG_SOFT_RST | DSI_CTRL_0_CFG_SOFT_RST_REG);
		mdelay(1);
	}
	writel(reg, &dsi->ctrl0);
}

void dsi_set_controller(struct pxa168fb_info *fbi)
{
	struct fb_var_screeninfo *var = &(fbi->fb_info->var);
	struct pxa168fb_mach_info *mi = fbi->dev->platform_data;
	struct dsi_info *di = (struct dsi_info *)mi->phy_info;
	struct dsi_regs *dsi = (struct dsi_regs *)di->regs;
	struct dsi_lcd_regs *dsi_lcd = &dsi->lcd1;
	unsigned hsync_b, hbp_b, hact_b, hex_b, hfp_b, httl_b;
	unsigned hsync, hbp, hact, hfp, httl, h_total, v_total;
	unsigned hsa_wc, hbp_wc, hact_wc, hex_wc, hfp_wc, hlp_wc;
	int bpp = di->bpp, hss_bcnt = 4, hse_bct = 4, lgp_over_head = 6, reg;

	if (di->id & 2)
		dsi_lcd = &dsi->lcd2;
	pr_debug("%s dsi %d lanes %d burst_mode %d bpp %d\n",
		__func__, di->id, di->lanes, di->burst_mode, bpp);

	h_total = var->xres + var->left_margin +
		 var->right_margin + var->hsync_len;
	v_total = var->yres + var->upper_margin +
		 var->lower_margin + var->vsync_len;

	hact_b = to_dsi_bcnt(var->xres, bpp);
	hfp_b = to_dsi_bcnt(var->right_margin, bpp);
	hbp_b = to_dsi_bcnt(var->left_margin, bpp);
	hsync_b = to_dsi_bcnt(var->hsync_len, bpp);
	hex_b = to_dsi_bcnt(dsi_ex_pixel_cnt, bpp);
	httl_b = hact_b + hsync_b + hfp_b + hbp_b + hex_b;

	hact = hact_b / di->lanes;
	hfp = hfp_b / di->lanes;
	hbp = hbp_b / di->lanes;
	hsync = hsync_b / di->lanes;
	httl = hact + hfp + hbp + hsync;

	/* word count in the unit of byte */
	hsa_wc = (di->burst_mode == DSI_BURST_MODE_SYNC_PULSE) ? \
		(hsync_b - hss_bcnt - lgp_over_head) : 0;

	/* Hse is with backporch */
	hbp_wc = (di->burst_mode == DSI_BURST_MODE_SYNC_PULSE) ? \
		(hbp_b - hse_bct - lgp_over_head) \
		: (hsync_b + hbp_b - hss_bcnt - lgp_over_head);

	hfp_wc = ((di->burst_mode == DSI_BURST_MODE_BURST) && \
		(dsi_hex_en == 0)) ? \
		(hfp_b + hex_b - lgp_over_head - lgp_over_head) : \
		(hfp_b - lgp_over_head - lgp_over_head);

	hact_wc =  ((var->xres) * bpp) >> 3;

	/* disable Hex currently */
	hex_wc = 0;

	/*  There is no hlp with active data segment.  */
	hlp_wc = (di->burst_mode == DSI_BURST_MODE_SYNC_PULSE) ? \
		(httl_b - hsync_b - hse_bct - lgp_over_head) : \
		(httl_b - hss_bcnt - lgp_over_head);

	/* FIXME - need to double check the (*3) is bytes_per_pixel from
	 * input data or output to panel */
	/* dsi_lane_enable - Set according to specified DSI lane count */
	writel(dsi_lane[di->lanes] << DSI_PHY_CTRL_2_CFG_CSR_LANE_EN_SHIFT,
		 &dsi->phy_ctrl2);
	writel(dsi_lane[di->lanes] << DSI_CPU_CMD_1_CFG_TXLP_LPDT_SHIFT,
		 &dsi->cmd1);

	/* SET UP LCD1 TIMING REGISTERS FOR DSI BUS */
	/* NOTE: Some register values were obtained by trial and error */
	writel((hact << 16) | httl, &dsi_lcd->timing0);
	writel((hsync << 16) | hbp, &dsi_lcd->timing1);
	/*
	 * For now the active size is set really low (we'll use 10) to allow
	 * the hardware to attain V Sync. Once the DSI bus is up and running,
	 * the final value will be put in place for the active size (this is
	 * done below). In a later stepping of the processor this workaround
	 * will not be required.
	 */
	writel(((var->yres)<<16) | (v_total), &dsi_lcd->timing2);

	writel(((var->vsync_len) << 16) | (var->upper_margin),
		 &dsi_lcd->timing3);

	/* SET UP LCD1 WORD COUNT REGISTERS FOR DSI BUS */
	/* Set up for word(byte) count register 0 */
	writel((hbp_wc << 16) | hsa_wc, &dsi_lcd->wc0);
	writel((hfp_wc << 16) | hact_wc, &dsi_lcd->wc1);
	writel((hex_wc << 16) | hlp_wc, &dsi_lcd->wc2);
	/* calculated value on brownstone:
	 * WC0: 0x1a0000
	 * WC1: 0x1500f00
	 * WC2: 0x1076 */

	/* Configure LCD control register 1 FOR DSI BUS */
	reg = ((di->rgb_mode << DSI_LCD2_CTRL_1_CFG_L1_RGB_TYPE_SHIFT)
	  | (di->burst_mode << DSI_LCD1_CTRL_1_CFG_L1_BURST_MODE_SHIFT)
	  | (di->lpm_line_en ? DSI_LCD1_CTRL_1_CFG_L1_LPM_LINE_EN : 0)
	  | (di->lpm_frame_en ? DSI_LCD1_CTRL_1_CFG_L1_LPM_FRAME_EN : 0)
	  | (di->last_line_turn ? DSI_LCD1_CTRL_1_CFG_L1_LAST_LINE_TURN : 0)
	  | (di->hex_slot_en ? 0 : 0)   /* disable Hex slot */
	  | (di->all_slot_en ? 0 : 0)   /* disable all slots */
	  | (di->hbp_en ? DSI_LCD1_CTRL_1_CFG_L1_HBP_PKT_EN : 0)
	  | (di->hact_en ? DSI_LCD1_CTRL_1_CFG_L1_HACT_PKT_EN : 0)
	  | (di->hfp_en ? DSI_LCD1_CTRL_1_CFG_L1_HFP_PKT_EN : 0)
	  | (di->hex_en ? 0 : 0)      /* Hex packet is disabled */
	  | (di->hlp_en ? DSI_LCD1_CTRL_1_CFG_L1_HLP_PKT_EN : 0));

	reg |= (di->burst_mode == DSI_BURST_MODE_SYNC_PULSE) ? \
		(((di->hsa_en) ? DSI_LCD1_CTRL_1_CFG_L1_HSA_PKT_EN : 0)
		| (DSI_LCD1_CTRL_1_CFG_L1_HSE_PKT_EN))  /* Hse is eabled */
		:
		(((di->hsa_en) ? 0 : 0)   /* Hsa packet is disabled */
		| ((di->hse_en) ? 0 : 0)); /* Hse packet is disabled */

	reg |=  DSI_LCD1_CTRL_1_CFG_L1_VSYNC_RST_EN;
	writel(reg, &dsi_lcd->ctrl1);

	/*Start the transfer of LCD data over the DSI bus*/
	/* DSI_CTRL_1 */
	reg = readl(&dsi->ctrl1);
	reg &= ~(DSI_CTRL_1_CFG_LCD2_VCH_NO_MASK |
		 DSI_CTRL_1_CFG_LCD1_VCH_NO_MASK);
	reg |= 0x1 << ((di->id & 1) ? DSI_CTRL_1_CFG_LCD2_VCH_NO_SHIFT :
		 DSI_CTRL_1_CFG_LCD1_VCH_NO_SHIFT);

	reg &= ~(DSI_CTRL_1_CFG_EOTP);
	if (di->eotp_en)
		reg |= DSI_CTRL_1_CFG_EOTP;	/* EOTP */

	writel(reg, &dsi->ctrl1);

	/* DSI_CTRL_0 */
	reg = (di->master_mode ? 0 : DSI_CTRL_0_CFG_LCD1_SLV) |
		DSI_CTRL_0_CFG_LCD1_TX_EN | DSI_CTRL_0_CFG_LCD1_EN;
	if (di->id & 2)
		reg = reg << 1;
	writel(reg, &dsi->ctrl0);

	writel(((var->yres)<<16) | (v_total), &dsi_lcd->timing2);
}

void dsi_set_panel_interface(struct pxa168fb_info *fbi, bool on)
{
	struct pxa168fb_mach_info *mi = fbi->dev->platform_data;
	struct dsi_info *di = (struct dsi_info *)mi->phy_info;
	struct dsi_regs *dsi = (struct dsi_regs *)di->regs;
	int reg;

	printk("zlp get dphy_ctrl2=%x\n",readl(&dsi->phy_ctrl2));
	reg = readl(&dsi->ctrl0);
	if (on)
		reg |= DSI_CTRL_0_CFG_LCD1_EN << ((di->id & 2) ? 1 : 0);
	else
		reg &= ~(DSI_CTRL_0_CFG_LCD1_EN << ((di->id & 2) ? 1 : 0));
	writel(reg, &dsi->ctrl0);
}


void set_dsi_low_power_mode(struct pxa168fb_info *fbi)
{
	struct pxa168fb_mach_info *mi = fbi->dev->platform_data;
	struct dsi_info *di = (struct dsi_info *)mi->phy_info;
	struct dsi_regs *dsi = (struct dsi_regs *)di->regs;
	u32 reg = readl(&dsi->phy_ctrl2);

	reg |= (2 << 4);
	writel(reg, &dsi->phy_ctrl2);

	/* enable data lane data0*/
	reg &= ~(0xf << 4);
	reg |= (1 << 4);
	writel(reg, &dsi->phy_ctrl2);

	/* LPDT TX enabled for data0 */
	reg = readl(&dsi->cmd1);
	reg &= ~(0xf << 20);
	reg |= 1 << DSI_CPU_CMD_1_CFG_TXLP_LPDT_SHIFT;
	writel(reg, &dsi->cmd1);
}

void dsi_set_panel_intf(struct pxa168fb_info *fbi, bool on)
{
	struct pxa168fb_mach_info *mi = fbi->dev->platform_data;
	struct dsi_info *di = (struct dsi_info *)mi->phy_info;
	struct dsi_regs *dsi = (struct dsi_regs *)di->regs;
	int reg;

	reg = readl(&dsi->ctrl0);
	if (on)
		reg |= DSI_CTRL_0_CFG_LCD1_EN << ((di->id & 2) ? 1 : 0);
	else
		reg &= ~(DSI_CTRL_0_CFG_LCD1_EN << ((di->id & 2) ? 1 : 0));
	writel(reg, &dsi->ctrl0);
}

static void print_ack_errors(unsigned int packet)
{
	int i;
	char ack_error_info[16][30] = {
	"SoT Error",
	/*
	 * may be caused by number of lanes used
	 * in both lane turn and phy lanes
	 */
	"SoT Sync Error",
	"EoT Sync Error",
	"Esc Mode Ent Cmd Err",
	"LP Transmit Sync Err",
	"HS Rcv TO Err",
	"False Control Err",
	"Reserved",
	"ECC Error, 1Bit",
	"ECC Error, multi-bit",
	"Chksm Err -Lg Packt",
	"DSI DT Not Recognized",
	"DSI VC ID Invalid",
	"Invalid Transmission Length",
	"Reserved",
	"DSI Protcl Err" };

	pr_info("Ack error packet = 0x%x", packet);
	for (i=0; i < 15; i++) {
		if (packet & (1<<i))
			pr_err("\t\tBit %d = %s",i, ack_error_info[i]);
	}
}

void dsi_cmd_array_rx(struct pxa168fb_info *fbi, struct dsi_buf *dbuf,
		struct dsi_cmd_desc cmds[], int count)
{
	struct pxa168fb_mach_info *mi = fbi->dev->platform_data;
	struct dsi_info *di = (struct dsi_info *)mi->phy_info;
	struct dsi_regs *dsi = (struct dsi_regs *)di->regs;
	u8 parameter[DSI_MAX_DATA_BYTES];
	u32 i, rx_reg, timeout, tmp, packet,
	    data_pointer, packet_count, byte_count;

	memset(dbuf, 0x0, sizeof(struct dsi_buf));
	dsi_cmd_array_tx(fbi, cmds, count);
#if 0
	/*will do BTA after command send, so the following code skip */
	/* do the BTA: dphy bus turn around */
	tmp = readl(&dsi->phy_ctrl2) & ~(0xf);
	tmp |= 0x1;
	writel(tmp, &dsi->phy_ctrl2);

	count = 10000;
	do {
		count--;
		tmp = readl(&dsi->phy_ctrl2);
	} while (((tmp & 0xf) == 0x1) && count);
	if (!count)
		pr_err("dphy bus not turn around phy_ctrl2 0x%x\n",
				readl(&dsi->phy_ctrl2)&0xf);
#endif
	timeout = 1000;
	do {
		timeout --;
		tmp = readl(&dsi->irq_status);
	} while (((tmp & 0x4) == 0) && timeout);
	if (!timeout)
		pr_err("dsi didn't receive packet, irq status 0x%x\n",
				readl(&dsi->irq_status));
	if (tmp & IRQ_RX_TRG2)
		pr_debug("ACK receiveddf\n");
	if (tmp & IRQ_RX_TRG1)
		pr_debug("TE trigger received\n");
	if (tmp & IRQ_RX_ERR) {
		pr_err("ACK with error report\n");
		tmp = readl(&dsi->rx0_header);
		print_ack_errors(tmp);
	}

	packet = readl(&dsi->rx0_status);
	if (packet & RX_PKT0_ST_VLD)
		pr_debug("Rx packet 0 status valid, 0x%x\n", packet);
	if (packet & RX_PKT0_ST_SP)
		pr_debug("Rx packet is short packet\n");
	else
		pr_debug("Rx packet is long packet\n");

	data_pointer = (packet & RX_PKT0_PKT_PTR_MASK) >> RX_PKT0_PKT_PTR_SHIFT;
	pr_debug("Rx FIFO data pointer %x\n", data_pointer);
	tmp = readl(&dsi->rx_ctrl1);
	packet_count = (tmp & RX_PKT_CNT_MASK) >> RX_PKT_CNT_SHIFT;
	byte_count = tmp & RX_PKT_BCNT_MASK;
	pr_debug("Rx FIFO packet count %d, byte count %d\n",
			packet_count, byte_count);

	memset(parameter, 0x00, byte_count);
	for (i = data_pointer; i < data_pointer + byte_count; i++) {
		rx_reg = readl(&dsi->rx_ctrl);
		rx_reg &= ~RX_PKT_RD_PTR_MASK;
		rx_reg |= RX_PKT_RD_REQ | (i << RX_PKT_RD_PTR_SHIFT);
		writel(rx_reg, &dsi->rx_ctrl);
		count = 10000;
		do {
			count--;
			rx_reg = readl(&dsi->rx_ctrl);
		} while (rx_reg & RX_PKT_RD_REQ && count);
		if (!count)
			pr_err("Rx packet FIFO read request not assert\n");
		parameter[i - data_pointer] = rx_reg & 0xff;		
	}
	switch (parameter[0]) {
	case DSI_DI_ACK_ERR_RESP:
		pr_err("Acknowledge with erro report\n");
		break;
	case DSI_DI_EOTP:
		pr_err("End of Transmission packet\n");
		break;
	case DSI_DI_GEN_READ1_RESP:
	case DSI_DI_DCS_READ1_RESP:
		dbuf->data_type = parameter[0];
		dbuf->length = 1;
		memcpy(dbuf->data, &parameter[1], dbuf->length);
		break;
	case DSI_DI_GEN_READ2_RESP:
	case DSI_DI_DCS_READ2_RESP:
		dbuf->data_type = parameter[0];
		dbuf->length = 2;
		memcpy(dbuf->data, &parameter[1], dbuf->length);
		break;
	case DSI_DI_GEN_LREAD_RESP:
	case DSI_DI_DCS_LREAD_RESP:
		dbuf->data_type = parameter[0];
		dbuf->length = (parameter[2] << 8) | parameter[1];
		memcpy(dbuf->data, &parameter[4], dbuf->length);
		break;

	}
}

int dsi_read_data(struct pxa168fb_info *fbi, u8 addr)
{
	struct pxa168fb_mach_info *mi = fbi->dev->platform_data;
	struct dsi_info *di = (struct dsi_info *)mi->phy_info;
	struct dsi_regs *dsi = (struct dsi_regs *)di->regs;
	u8 parameter[DSI_MAX_DATA_BYTES], loop;
	u32 rx_reg, count, ecc, tmp, packet,
	    data_pointer, packet_count, byte_count;

	dsi_set_panel_intf(fbi, 0);
	set_dsi_low_power_mode(fbi);

	memset(parameter, 0x00, 4);
	parameter[0] = 0x37; /* DSI_DI_SMRPS */
	parameter[1] = 0x01;
	parameter[2] = 0x00;
	parameter[3] = calculate_ecc(parameter);
	dsi_send_cmds(fbi, parameter, 4, 1);

	parameter[0] = 0x06; /* DSI_DI_READ */
	parameter[1] = addr;
	parameter[2] = 0x00;
	parameter[3] = calculate_ecc(parameter);
	dsi_send_cmds(fbi, parameter, 4, 1);

	tmp = readl(&dsi->phy_ctrl2) & ~(0xf);
	tmp |= 0x1;
	writel(tmp, &dsi->phy_ctrl2);

	count = 10000;
	do {
		count--;
		tmp = readl(&dsi->phy_ctrl2);
	} while (((tmp & 0xf) == 0x1) && count);
	if (!count)
		pr_err("dphy bus not turn around phy_ctrl2 0x%x\n",
				readl(&dsi->phy_ctrl2)&0xf);

	count = 10000;
	do {
		count--;
		tmp = readl(&dsi->irq_status);
	} while (((tmp & 0x4) == 0) && count);
	if (!count)
		pr_err("dsi didn't receive packet, irq status 0x%x\n",
				readl(&dsi->irq_status));
	if (tmp & (0x1 << 6))
		pr_debug("ACK received(VALUE=0x84)\n");
	if (tmp & (0x1 << 5))
		pr_debug("TE trigger received(VALUE=0xba)\n");
	if (tmp & (0x1 << 25)) {
		pr_err("ACK with error report\n");
		tmp = readl(&dsi->rx0_header);
		print_ack_errors(tmp);
	}

	packet = readl(&dsi->rx0_status);
	if (packet & 0x80000000)
		pr_debug("Rx packet 0 status valid, 0x%x\n", packet);
	if (packet & (0x1 << 24))
		pr_debug("Rx packet is short packet\n");
	else
		pr_debug("Rx packet is long packet\n");
	data_pointer = (packet & (0xf << 16)) >> 16;
	pr_debug("Rx FIFO data pointer %x\n", data_pointer);
	tmp = readl(&dsi->rx_ctrl1);
	packet_count = (tmp & 0xf00) >> 8;
	byte_count = tmp & 0xff;
	pr_debug("Rx FIFO packet count %d, byte count %x\n",
			packet_count, byte_count);

	memset(parameter, 0x00, byte_count);
	for (loop = data_pointer; loop < data_pointer + byte_count; loop++) {
		rx_reg = readl(&dsi->rx_ctrl);
		rx_reg &= ~(0x3f << 16);
		rx_reg |= (1 << 31) | (loop << 16);
		writel(rx_reg, &dsi->rx_ctrl);
		count = 10000;
		do {
			count--;
			rx_reg = readl(&dsi->rx_ctrl);
		} while (rx_reg & (1 << 31) && count);
		if (!count)
			pr_err("Rx packet FIFO read request not assert\n");
		parameter[loop - data_pointer] = rx_reg & 0xff;
	}

	ecc = calculate_ecc(parameter);
	pr_info("DSI receive data:\n");
	pr_info("rx_data_type  rx_dcs  rx_parameter  rx_ecc  ecc\n");
	pr_info("0x%x          0x%x     0x%x          0x%x    0x%x",
		parameter[0], parameter[1], parameter[2], parameter[3], ecc);

	for (loop = 0; loop < (byte_count - 4); loop++)
		pr_info("0x%x \t ", parameter[loop]);
	pr_info("\n");
	if (ecc != parameter[3]) {
		pr_info("ecc error, rx_ecc: 0x%x, cal_ecc: 0x%x!",
				parameter[3], ecc);
		return -1;
	}

	/* put all lanes to LP-11 state  */
	dsi_lanes_enable(fbi, 1);
	dsi_set_panel_intf(fbi, 1);
	return 0;
}
static int dsi_dump(struct pxa168fb_info *fbi, int f, char *buf, int s)
{
	struct pxa168fb_mach_info *mi = fbi->dev->platform_data;
	struct dsi_info *di = (struct dsi_info *)mi->phy_info;
	struct dsi_regs *dsi;
	int dsi_base;

	if (!di) {
		pr_err("%s: no dsi info available\n", __func__);
		return s;
	}
	dsi = (struct dsi_regs *)di->regs;
	dsi_base = (int)(&dsi->ctrl0);

	mvdisp_dump(f, "dsi_info: ch %d lanes %d bpp %d\n\n", di->id,
			di->lanes, di->bpp);

	mvdisp_dump(f, "dsi regs base 0x%p\n", dsi);
	mvdisp_dump(f, "\tctrl0      (@%3x):\t0x%x\n",
		(int)(&dsi->ctrl0) - dsi_base, readl(&dsi->ctrl0));
	mvdisp_dump(f, "\tctrl1      (@%3x):\t0x%x\n",
		(int)(&dsi->ctrl1) - dsi_base, readl(&dsi->ctrl1));
	mvdisp_dump(f, "\tirq_status (@%3x):\t0x%x\n",
		(int)(&dsi->irq_status) - dsi_base, readl(&dsi->irq_status));
	mvdisp_dump(f, "\tirq_mask   (@%3x):\t0x%x\n",
		(int)(&dsi->irq_mask) - dsi_base, readl(&dsi->irq_mask));
	mvdisp_dump(f, "\tcmd0       (@%3x):\t0x%x\n",
		(int)(&dsi->cmd0) - dsi_base, readl(&dsi->cmd0));
	mvdisp_dump(f, "\tcmd1       (@%3x):\t0x%x\n",
		(int)(&dsi->cmd1) - dsi_base, readl(&dsi->cmd1));
	mvdisp_dump(f, "\tcmd2       (@%3x):\t0x%x\n",
		(int)(&dsi->cmd2) - dsi_base, readl(&dsi->cmd2));
	mvdisp_dump(f, "\tcmd3       (@%3x):\t0x%x\n",
		(int)(&dsi->cmd3) - dsi_base, readl(&dsi->cmd3));
	mvdisp_dump(f, "\tdat0       (@%3x):\t0x%x\n",
		(int)(&dsi->dat0) - dsi_base, readl(&dsi->dat0));
	mvdisp_dump(f, "\tstatus0    (@%3x):\t0x%x\n",
		(int)(&dsi->status0) - dsi_base, readl(&dsi->status0));
	mvdisp_dump(f, "\tstatus1    (@%3x):\t0x%x\n",
		(int)(&dsi->status1) - dsi_base, readl(&dsi->status1));
	mvdisp_dump(f, "\tstatus2    (@%3x):\t0x%x\n",
		(int)(&dsi->status2) - dsi_base, readl(&dsi->status2));
	mvdisp_dump(f, "\tstatus3    (@%3x):\t0x%x\n",
		(int)(&dsi->status3) - dsi_base, readl(&dsi->status3));
	mvdisp_dump(f, "\tstatus4    (@%3x):\t0x%x\n",
		(int)(&dsi->status4) - dsi_base, readl(&dsi->status4));
	mvdisp_dump(f, "\tsmt_cmd    (@%3x):\t0x%x\n",
		(int)(&dsi->smt_cmd) - dsi_base, readl(&dsi->smt_cmd));
	mvdisp_dump(f, "\tsmt_ctrl0  (@%3x):\t0x%x\n",
		(int)(&dsi->smt_ctrl0) - dsi_base, readl(&dsi->smt_ctrl0));
	mvdisp_dump(f, "\tsmt_ctrl1  (@%3x):\t0x%x\n",
		(int)(&dsi->smt_ctrl1) - dsi_base, readl(&dsi->smt_ctrl1));
	mvdisp_dump(f, "\trx0_status (@%3x):\t0x%x\n",
		(int)(&dsi->rx0_status) - dsi_base, readl(&dsi->rx0_status));
	mvdisp_dump(f, "\trx0_header (@%3x):\t0x%x\n",
		(int)(&dsi->rx0_header) - dsi_base, readl(&dsi->rx0_header));
	mvdisp_dump(f, "\trx1_status (@%3x):\t0x%x\n",
		(int)(&dsi->rx1_status) - dsi_base, readl(&dsi->rx1_status));
	mvdisp_dump(f, "\trx1_header (@%3x):\t0x%x\n",
		(int)(&dsi->rx1_header) - dsi_base, readl(&dsi->rx1_header));
	mvdisp_dump(f, "\trx_ctrl    (@%3x):\t0x%x\n",
		(int)(&dsi->rx_ctrl) - dsi_base, readl(&dsi->rx_ctrl));
	mvdisp_dump(f, "\trx_ctrl1   (@%3x):\t0x%x\n",
		(int)(&dsi->rx_ctrl1) - dsi_base, readl(&dsi->rx_ctrl1));
	mvdisp_dump(f, "\trx2_status (@%3x):\t0x%x\n",
		(int)(&dsi->rx2_status) - dsi_base, readl(&dsi->rx2_status));
	mvdisp_dump(f, "\trx2_header (@%3x):\t0x%x\n",
		(int)(&dsi->rx2_header) - dsi_base, readl(&dsi->rx2_header));
	mvdisp_dump(f, "\tphy_ctrl1  (@%3x):\t0x%x\n",
		(int)(&dsi->phy_ctrl1) - dsi_base, readl(&dsi->phy_ctrl1));
	mvdisp_dump(f, "\tphy_ctrl2  (@%3x):\t0x%x\n",
		(int)(&dsi->phy_ctrl2) - dsi_base, readl(&dsi->phy_ctrl2));
	mvdisp_dump(f, "\tphy_ctrl3  (@%3x):\t0x%x\n",
		(int)(&dsi->phy_ctrl3) - dsi_base, readl(&dsi->phy_ctrl3));
	mvdisp_dump(f, "\tphy_status0(@%3x):\t0x%x\n",
		(int)(&dsi->phy_status0) - dsi_base, readl(&dsi->phy_status0));
	mvdisp_dump(f, "\tphy_status1(@%3x):\t0x%x\n",
		(int)(&dsi->phy_status1) - dsi_base, readl(&dsi->phy_status1));
	mvdisp_dump(f, "\tphy_status2(@%3x):\t0x%x\n",
		(int)(&dsi->phy_status2) - dsi_base, readl(&dsi->phy_status2));
	mvdisp_dump(f, "\tphy_rcomp0 (@%3x):\t0x%x\n",
		(int)(&dsi->phy_rcomp0) - dsi_base, readl(&dsi->phy_rcomp0));
	mvdisp_dump(f, "\tphy_timing0(@%3x):\t0x%x\n",
		(int)(&dsi->phy_timing0) - dsi_base, readl(&dsi->phy_timing0));
	mvdisp_dump(f, "\tphy_timing1(@%3x):\t0x%x\n",
		(int)(&dsi->phy_timing1) - dsi_base, readl(&dsi->phy_timing1));
	mvdisp_dump(f, "\tphy_timing2(@%3x):\t0x%x\n",
		(int)(&dsi->phy_timing2) - dsi_base, readl(&dsi->phy_timing2));
	mvdisp_dump(f, "\tphy_timing3(@%3x):\t0x%x\n",
		(int)(&dsi->phy_timing3) - dsi_base, readl(&dsi->phy_timing3));
	mvdisp_dump(f, "\tphy_code_0 (@%3x):\t0x%x\n",
		(int)(&dsi->phy_code_0) - dsi_base, readl(&dsi->phy_code_0));
	mvdisp_dump(f, "\tphy_code_1 (@%3x):\t0x%x\n",
		(int)(&dsi->phy_code_1) - dsi_base, readl(&dsi->phy_code_1));
	mvdisp_dump(f, "\tmem_ctrl   (@%3x):\t0x%x\n",
		(int)(&dsi->mem_ctrl) - dsi_base, readl(&dsi->mem_ctrl));
	mvdisp_dump(f, "\ttx_timer   (@%3x):\t0x%x\n",
		(int)(&dsi->tx_timer) - dsi_base, readl(&dsi->tx_timer));
	mvdisp_dump(f, "\trx_timer   (@%3x):\t0x%x\n",
		(int)(&dsi->rx_timer) - dsi_base, readl(&dsi->rx_timer));
	mvdisp_dump(f, "\tturn_timer (@%3x):\t0x%x\n",
		(int)(&dsi->turn_timer) - dsi_base, readl(&dsi->turn_timer));

	mvdisp_dump(f, "\nlcd1 regs\n");
	mvdisp_dump(f, "\tctrl0     (@%3x):\t0x%x\n",
		(int)(&dsi->lcd1.ctrl0) - dsi_base, readl(&dsi->lcd1.ctrl0));
	mvdisp_dump(f, "\tctrl1     (@%3x):\t0x%x\n",
		(int)(&dsi->lcd1.ctrl1) - dsi_base, readl(&dsi->lcd1.ctrl1));
	mvdisp_dump(f, "\ttiming0   (@%3x):\t0x%x\n",
		(int)(&dsi->lcd1.timing0) - dsi_base,
		readl(&dsi->lcd1.timing0));
	mvdisp_dump(f, "\ttiming1   (@%3x):\t0x%x\n",
		(int)(&dsi->lcd1.timing1) - dsi_base,
		readl(&dsi->lcd1.timing1));
	mvdisp_dump(f, "\ttiming2   (@%3x):\t0x%x\n",
		(int)(&dsi->lcd1.timing2) - dsi_base,
		readl(&dsi->lcd1.timing2));
	mvdisp_dump(f, "\ttiming3   (@%3x):\t0x%x\n",
		(int)(&dsi->lcd1.timing3) - dsi_base,
		readl(&dsi->lcd1.timing3));
	mvdisp_dump(f, "\twc0       (@%3x):\t0x%x\n",
		(int)(&dsi->lcd1.wc0) - dsi_base, readl(&dsi->lcd1.wc0));
	mvdisp_dump(f, "\twc1       (@%3x):\t0x%x\n",
		(int)(&dsi->lcd1.wc1) - dsi_base, readl(&dsi->lcd1.wc1));
	mvdisp_dump(f, "\twc2       (@%3x):\t0x%x\n",
		(int)(&dsi->lcd1.wc2) - dsi_base, readl(&dsi->lcd1.wc2));
	mvdisp_dump(f, "\tslot_cnt0 (@%3x):\t0x%x\n",
		(int)(&dsi->lcd1.slot_cnt0) - dsi_base,
		readl(&dsi->lcd1.slot_cnt0));
	mvdisp_dump(f, "\tslot_cnt1 (@%3x):\t0x%x\n",
		(int)(&dsi->lcd1.slot_cnt1) - dsi_base,
		readl(&dsi->lcd1.slot_cnt1));
	mvdisp_dump(f, "\tstatus_0  (@%3x):\t0x%x\n",
		(int)(&dsi->lcd1.status_0) - dsi_base,
		readl(&dsi->lcd1.status_0));
	mvdisp_dump(f, "\tstatus_1  (@%3x):\t0x%x\n",
		(int)(&dsi->lcd1.status_1) - dsi_base,
		readl(&dsi->lcd1.status_1));
	mvdisp_dump(f, "\tstatus_2  (@%3x):\t0x%x\n",
		(int)(&dsi->lcd1.status_2) - dsi_base,
		readl(&dsi->lcd1.status_2));
	mvdisp_dump(f, "\tstatus_3  (@%3x):\t0x%x\n",
		(int)(&dsi->lcd1.status_3) - dsi_base,
		readl(&dsi->lcd1.status_3));
	mvdisp_dump(f, "\tstatus_4  (@%3x):\t0x%x\n",
		(int)(&dsi->lcd1.status_4) - dsi_base,
		readl(&dsi->lcd1.status_4));

	mvdisp_dump(f, "\nlcd2 regs\n");
	mvdisp_dump(f, "\tctrl0     (@%3x):\t0x%x\n",
		(int)(&dsi->lcd2.ctrl0) - dsi_base,
		readl(&dsi->lcd2.ctrl0));
	mvdisp_dump(f, "\tctrl1     (@%3x):\t0x%x\n",
		(int)(&dsi->lcd2.ctrl1) - dsi_base,
		readl(&dsi->lcd2.ctrl1));
	mvdisp_dump(f, "\ttiming0   (@%3x):\t0x%x\n",
		(int)(&dsi->lcd2.timing0) - dsi_base,
		readl(&dsi->lcd2.timing0));
	mvdisp_dump(f, "\ttiming1   (@%3x):\t0x%x\n",
		(int)(&dsi->lcd2.timing1) - dsi_base,
		readl(&dsi->lcd2.timing1));
	mvdisp_dump(f, "\ttiming2   (@%3x):\t0x%x\n",
		(int)(&dsi->lcd2.timing2) - dsi_base,
		readl(&dsi->lcd2.timing2));
	mvdisp_dump(f, "\ttiming3   (@%3x):\t0x%x\n",
		(int)(&dsi->lcd2.timing3) - dsi_base,
		readl(&dsi->lcd2.timing3));
	mvdisp_dump(f, "\twc0       (@%3x):\t0x%x\n",
		(int)(&dsi->lcd2.wc0) - dsi_base, readl(&dsi->lcd2.wc0));
	mvdisp_dump(f, "\twc1       (@%3x):\t0x%x\n",
		(int)(&dsi->lcd2.wc1) - dsi_base, readl(&dsi->lcd2.wc1));
	mvdisp_dump(f, "\twc2       (@%3x):\t0x%x\n",
		(int)(&dsi->lcd2.wc2) - dsi_base, readl(&dsi->lcd2.wc2));
	mvdisp_dump(f, "\tslot_cnt0 (@%3x):\t0x%x\n",
		(int)(&dsi->lcd2.slot_cnt0) - dsi_base,
		readl(&dsi->lcd2.slot_cnt0));
	mvdisp_dump(f, "\tslot_cnt1 (@%3x):\t0x%x\n",
		(int)(&dsi->lcd2.slot_cnt1) - dsi_base,
		readl(&dsi->lcd2.slot_cnt1));
	mvdisp_dump(f, "\tstatus_0  (@%3x):\t0x%x\n",
		(int)(&dsi->lcd2.status_0) - dsi_base,
		readl(&dsi->lcd2.status_0));
	mvdisp_dump(f, "\tstatus_1  (@%3x):\t0x%x\n",
		(int)(&dsi->lcd2.status_1) - dsi_base,
		readl(&dsi->lcd2.status_1));
	mvdisp_dump(f, "\tstatus_2  (@%3x):\t0x%x\n",
		(int)(&dsi->lcd2.status_2) - dsi_base,
		readl(&dsi->lcd2.status_2));
	mvdisp_dump(f, "\tstatus_3  (@%3x):\t0x%x\n",
		(int)(&dsi->lcd2.status_3) - dsi_base,
		readl(&dsi->lcd2.status_3));
	mvdisp_dump(f, "\tstatus_4  (@%3x):\t0x%x\n",
		(int)(&dsi->lcd2.status_4) - dsi_base,
		readl(&dsi->lcd2.status_4));

	mvdisp_dump(f, "\ncommands:\n");
	mvdisp_dump(f, " - dump all DSI controller registers\n");
	mvdisp_dump(f, "\tcat phy\n");
	mvdisp_dump(f, " - dump DSI register @ [offset_hex]\n");
	mvdisp_dump(f, "\techo -0x[offset_hex] > phy\n");
	mvdisp_dump(f, " - set DSI register @ [offset_hex] with [value_hex]\n");
	mvdisp_dump(f, "\techo 0x[value_hex] > phy\n");

	return s;
}

/* select LVDS_PHY_CTL_EXTx */
#define lvds_ext_select(ext, tmp, reg) do {				\
	if ((ext < 0) || (ext > 5)) {					\
		pr_err("%s ext %d not supported\n", __func__, ext);	\
		return 0;						\
	}								\
	reg = (u32)gfx_info.fbi[0]->reg_base + LVDS_PHY_CTL;		\
	if (ext) {							\
		/* select LVDS_PHY_CTL_EXTx */				\
		tmp = readl(reg) & (~LVDS_PHY_EXT_MASK);		\
		writel(tmp | (ext - 1) << LVDS_PHY_EXT_SHIFT, reg);	\
		/* switch to LVDS_PHY_CTL_EXTx */			\
		reg -= LVDS_PHY_CTL; reg += LVDS_PHY_CTL_EXT;		\
	}								\
} while (0)

static u32 lvds_get(int ext)
{
	u32 reg, tmp;

	lvds_ext_select(ext, tmp, reg);

	return readl(reg);
}

static int lvds_set(int ext, u32 mask, u32 val)
{
	u32 reg, tmp, tmp2;

	lvds_ext_select(ext, tmp, reg);

	tmp = tmp2 = readl(reg);
	tmp2 &= ~mask; tmp2 |= val;
	if (tmp != tmp2)
		writel(tmp2, reg);

	return 0;
}

static int lvds_dump(struct pxa168fb_info *fbi, int f, char *buf, int s)
{
	struct pxa168fb_mach_info *mi = fbi->dev->platform_data;
	struct lvds_info *lvds = (struct lvds_info *)mi->phy_info;
	u32 reg = (u32)fbi->reg_base + LCD_2ND_BLD_CTL;
	char *str;

	switch (lvds->src) {
	case LVDS_SRC_PN:
		str = "PN";
		break;
	case LVDS_SRC_CMU:
		str = "CMU";
		break;
	case LVDS_SRC_PN2:
		str = "PN2";
		break;
	case LVDS_SRC_TV:
		str = "TV";
		break;
	default:
		str = "?";
		break;
	};

	mvdisp_dump(f, "lvds_info: src %s fmt %s\n", str,
		(lvds->fmt & LVDS_FMT_18BIT) ? "18bit" : "24bit");
	mvdisp_dump(f, "LCD_2ND_BLD_CTL(0x%x): 0x%x\n\n",
		reg & 0xfff, readl(reg));

	mvdisp_dump(f, "LVDS_PHY_CTL: 0x%x\n", lvds_get(0));
	mvdisp_dump(f, "        EXT1: 0x%x\n", lvds_get(1));
	mvdisp_dump(f, "        EXT2: 0x%x\n", lvds_get(2));
	mvdisp_dump(f, "        EXT3: 0x%x\n", lvds_get(3));
	mvdisp_dump(f, "        EXT4: 0x%x\n", lvds_get(4));
	mvdisp_dump(f, "        EXT5: 0x%x\n", lvds_get(5));

	return s;
}

int pxa688_lvds_config(struct lvds_info *lvds)
{
	u32 reg = (u32)gfx_info.fbi[0]->reg_base + LCD_2ND_BLD_CTL;
	u32 val = readl(reg) & ~(LVDS_SRC_MASK | LVDS_FMT_MASK);

	val |= (lvds->src << LVDS_SRC_SHIFT) | (lvds->fmt << LVDS_FMT_SHIFT);
	writel(val, reg);

	return 0;
}

int pxa688_lvds_init(struct pxa168fb_info *fbi)
{
	struct pxa168fb_mach_info *mi = fbi->dev->platform_data;
	struct lvds_info *lvds = (struct lvds_info *)mi->phy_info;
	int count = 100000;
	u32 mask, val;

	/* configure lvds src and fmt */
	pxa688_lvds_config(lvds);

	/* release LVDS PHY from reset */
	lvds_set(0, LVDS_RST, 0);
	mdelay(1);

	/* disable LVDS channel 0-5 power-down */
	lvds_set(0, LVDS_PD_CH_MASK, 0);

	/* select LVDS_PCLK instead of REFCLK as LVDS PHY clock */
	lvds_set(0, LVDS_CLK_SEL, LVDS_CLK_SEL_LVDS_PCLK);

	/* power up IP */
	lvds_set(0, LVDS_PU_IVREF, LVDS_PU_IVREF);

	/* REFDIV = 0x3, reference clock divider
	 * FBDIV = 0xa, feedback clock divider
	 * KVCO = 0x4, 1.7G - 1.9G */
	mask = LVDS_REFDIV_MASK | LVDS_FBDIV_MASK | LVDS_REFDIV_MASK
		| LVDS_CTUNE_MASK | LVDS_VREG_IVREF_MASK
		| LVDS_VDDL_MASK | LVDS_VDDM_MASK;
	val = (0x6 << LVDS_REFDIV_SHIFT) | (0x1 << LVDS_FBDIV_SHIFT)
		| (0x4 << LVDS_KVCO_SHIFT | (0x2 << LVDS_CTUNE_SHIFT)
		| (0x2 << LVDS_VREG_IVREF_SHIFT) | (0x9 << LVDS_VDDL_SHIFT)
		| (0x1 << LVDS_VDDM_SHIFT));
	lvds_set(3, mask, val);

	/* VCO_VRNG = 0x3, LVDS PLL V to I gain control, for KVCO[3:0] = 0x4 */
	mask = LVDS_VCO_VRNG_MASK | LVDS_ICP_MASK | LVDS_PI_EN
		| LVDS_VCODIV_SEL_SE_MASK | LVDS_INTPI_MASK;
	val = (0x3 << LVDS_VCO_VRNG_SHIFT) | (0x1 << LVDS_ICP_SHIFT)
		| LVDS_PI_EN | (0xd << LVDS_VCODIV_SEL_SE_SHIFT)
		| (0x3 << LVDS_INTPI_SHIFT);
	lvds_set(4, mask, val);

	/* enable PUPLL/PUTX to power up rest of PLL and TX */
	lvds_set(0, LVDS_PU_TX | LVDS_PU_PLL, LVDS_PU_TX | LVDS_PU_PLL);

	/* poll on lock bit until LVDS PLL locks */
	while (!(lvds_get(0) & LVDS_PLL_LOCK) && count--);
	if (count <= 0) {
		pr_err("%s failed\n", __func__);
		lvds_dump(fbi, DUMP_PRINFO, NULL, 0);
	}

	/* enable common mode feedback circuit */
	mask = LVDS_SELLV_OP9_MASK | LVDS_SELLV_OP7_MASK | LVDS_SELLV_OP6_MASK
		| LVDS_SELLV_TXDATA_MASK | LVDS_SELLV_TXCLK_MASK | LVDS_TX_DIF_CM_MASK
		| LVDS_TX_DIF_AMP_MASK | LVDS_TX_TERM_EN | LVDS_TX_CMFB_EN;
	val = (0x1 << LVDS_SELLV_OP9_SHIFT) | (0x1 << LVDS_SELLV_OP7_SHIFT)
		| (0x1 << LVDS_SELLV_OP6_SHIFT)
		| (0xa << LVDS_SELLV_TXDATA_SHIFT)
		| (0xa << LVDS_SELLV_TXCLK_SHIFT)
		| (0x3 << LVDS_TX_DIF_CM_SHIFT)
		| (0x8 << LVDS_TX_DIF_AMP_SHIFT) | LVDS_TX_CMFB_EN;
	lvds_set(2, mask, val);

	/* Flip all the N\P pins in order to get correct display,
	 * the pins might be inverted in the chip */
	lvds_set(1, LVDS_POL_SWAP_MASK, 0x3f << LVDS_POL_SWAP_SHIFT);

	return 0;
}

static void dsi_store(struct pxa168fb_mach_info *mi,
	const char *buf, size_t size)
{
	struct dsi_info *di = (struct dsi_info *)mi->phy_info;
	struct dsi_regs *dsi = (struct dsi_regs *)di->regs;
	static u32 mvdsi_reg;
	char vol[30];
	u32 addr, tmp;

	if (size > 30) {
		pr_err("%s size = %d > max 30 chars\n", __func__, size);
		return;
	}

	addr = (u32)&dsi->ctrl0;
	if ('-' == buf[0]) {
		memcpy(vol, buf + 1, size - 1);
		mvdsi_reg = (int)simple_strtoul(vol, NULL, 16);
		pr_info("dsi reg @ 0x%x: 0x%x\n", mvdsi_reg,
			__raw_readl(addr + mvdsi_reg));
	} else if ('0' == buf[0] && 'x' == buf[1]) {
		/* set the register value */
		tmp = (int)simple_strtoul(buf, NULL, 16);
		__raw_writel(tmp, addr + mvdsi_reg);
		pr_info("set dsi reg @ 0x%x: 0x%x\n", mvdsi_reg,
			__raw_readl(addr + mvdsi_reg));
	}
}

ssize_t phy_show(struct device *dev, struct device_attribute *attr,
		char *buf)
{
	struct pxa168fb_info *fbi = dev_get_drvdata(dev);
	struct pxa168fb_mach_info *mi = fbi->dev->platform_data;
	int s = 0;

	if (!mi)
		goto out;

	if (mi->phy_type & LVDS)
		s += lvds_dump(fbi, DUMP_SPRINTF, buf, s);

	if ((mi->phy_type & (DSI | DSI2DPI)))
		s += dsi_dump(fbi, DUMP_SPRINTF, buf, s);

out:
	return s;
}

ssize_t phy_store(
		struct device *dev, struct device_attribute *attr,
		const char *buf, size_t size)
{
	struct pxa168fb_info *fbi = dev_get_drvdata(dev);
	struct pxa168fb_mach_info *mi = fbi->dev->platform_data;
	int value;

	if (mi->phy_type & (DSI | DSI2DPI))
		dsi_store(mi, buf, size);
	else
		sscanf(buf, "%d", &value);

	return size;
}
DEVICE_ATTR(phy, S_IRUGO | S_IWUSR, phy_show, phy_store);

#endif
