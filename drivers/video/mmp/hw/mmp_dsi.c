/*
 * linux/drivers/video/mmp/phy/mmp_dsi.c
 *
 * Copyright (C) 2013 Marvell Technology Group Ltd.
 * Authors:  Guoqing Li <ligq@marvell.com>
 *          Lisa Du <cldu@marvell.com>
 *          Zhou Zhu <zzhu3@marvell.com>
 *          Jing Xiang <jxiang@marvell.com>
 *          Yu Xu <yuxu@marvell.com>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along with
 * this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 */

#include <linux/kernel.h>
#include <linux/io.h>
#include <linux/platform_device.h>
#include <linux/delay.h>
#include <linux/mutex.h>
#include <linux/module.h>
#include <linux/err.h>
#include <linux/interrupt.h>
#include <linux/completion.h>
#include <linux/clk.h>
#include <linux/of.h>
#include <linux/slab.h>
#include <linux/regulator/consumer.h>
#include <video/mipi_display.h>
#include "mmp_ctrl.h"
#include "mmp_dsi.h"
#include <linux/timer.h>

#ifdef CONFIG_MMP_ADAPTIVE_FPS
static void count_fps_work(struct work_struct *pwork);
extern int adaptive_fps_init(struct device *dev);
extern void adaptive_fps_uninit(struct device *dev);
extern void update_fps(struct mmp_dsi *mmp_dsi, unsigned int fps);
#endif

/* dsi phy timing */
static struct dsi_phy_timing dphy_timing = {
	.hs_prep_constant = 40,
	.hs_prep_ui = 4,
	.hs_zero_constant = 145,
	.hs_zero_ui = 10,
	.hs_trail_constant = 60,
	.hs_trail_ui = 4,
	.hs_exit_constant = 100,
	.hs_exit_ui = 0,
	.ck_zero_constant = 300,
	.ck_zero_ui = 0,
	.ck_trail_constant = 60,
	.ck_trail_ui = 0,
	.req_ready = 0x3c,
	.wakeup_constant = 1000000,
	.wakeup_ui = 0,
	/*
	 * According to the D-PHY spec, Tlpx >= 50 ns
	 * For the panel otm1281a, it works unstable when Tlpx = 50,
	 * so increase to 60ns.
	 */
	.lpx_constant = 60,
	.lpx_ui = 0,
};

static u8 get_bit(u32 index, u8 *pdata)
{
	u32 cindex, bindex;

	cindex = index / 8;
	bindex = index % 8;

	return (pdata[cindex] & (1 << bindex)) ? 1 : 0;
}

static u8 calculate_ecc(u8 *pdata)
{
	u8 ret, p[8];

	p[7] = 0x0;
	p[6] = 0x0;

	p[5] = (u8) (
	(
		get_bit(10, pdata) ^
		get_bit(11, pdata) ^
		get_bit(12, pdata) ^
		get_bit(13, pdata) ^
		get_bit(14, pdata) ^
		get_bit(15, pdata) ^
		get_bit(16, pdata) ^
		get_bit(17, pdata) ^
		get_bit(18, pdata) ^
		get_bit(19, pdata) ^
		get_bit(21, pdata) ^
		get_bit(22, pdata) ^
		get_bit(23, pdata)
		)
	);
	p[4] = (u8) (
		get_bit(4, pdata) ^
		get_bit(5, pdata) ^
		get_bit(6, pdata) ^
		get_bit(7, pdata) ^
		get_bit(8, pdata) ^
		get_bit(9, pdata) ^
		get_bit(16, pdata) ^
		get_bit(17, pdata) ^
		get_bit(18, pdata) ^
		get_bit(19, pdata) ^
		get_bit(20, pdata) ^
		get_bit(22, pdata) ^
		get_bit(23, pdata)
	);
	p[3] = (u8) (
	(
		get_bit(1, pdata) ^
		get_bit(2, pdata) ^
		get_bit(3, pdata) ^
		get_bit(7, pdata) ^
		get_bit(8, pdata) ^
		get_bit(9, pdata) ^
		get_bit(13, pdata) ^
		get_bit(14, pdata) ^
		get_bit(15, pdata) ^
		get_bit(19, pdata) ^
		get_bit(20, pdata) ^
		get_bit(21, pdata) ^
		get_bit(23, pdata)
		)
	);
	p[2] = (u8) (
	(
		get_bit(0, pdata) ^
		get_bit(2, pdata) ^
		get_bit(3, pdata) ^
		get_bit(5, pdata) ^
		get_bit(6, pdata) ^
		get_bit(9, pdata) ^
		get_bit(11, pdata) ^
		get_bit(12, pdata) ^
		get_bit(15, pdata) ^
		get_bit(18, pdata) ^
		get_bit(20, pdata) ^
		get_bit(21, pdata) ^
		get_bit(22, pdata)
		)
	);
	p[1] = (u8) (
		(
		get_bit(0, pdata) ^
		get_bit(1, pdata) ^
		get_bit(3, pdata) ^
		get_bit(4, pdata) ^
		get_bit(6, pdata) ^
		get_bit(8, pdata) ^
		get_bit(10, pdata) ^
		get_bit(12, pdata) ^
		get_bit(14, pdata) ^
		get_bit(17, pdata) ^
		get_bit(20, pdata) ^
		get_bit(21, pdata) ^
		get_bit(22, pdata) ^
		get_bit(23, pdata)
		)
	);
	p[0] = (u8) (
		(
		get_bit(0, pdata) ^
		get_bit(1, pdata) ^
		get_bit(2, pdata) ^
		get_bit(4, pdata) ^
		get_bit(5, pdata) ^
		get_bit(7, pdata) ^
		get_bit(10, pdata) ^
		get_bit(11, pdata) ^
		get_bit(13, pdata) ^
		get_bit(16, pdata) ^
		get_bit(20, pdata) ^
		get_bit(21, pdata) ^
		get_bit(22, pdata) ^
		get_bit(23, pdata)
		)
	);
	ret = (u8)(
		p[0] |
		(p[1] << 0x1) |
		(p[2] << 0x2) |
		(p[3] << 0x3) |
		(p[4] << 0x4) |
		(p[5] << 0x5)
	);
	return   ret;
}

static u16 calculate_crc16(u8 *pdata, u16 count)
{
	u16 byte_counter;
	u8 bit_counter;
	u8 data;
	u16 crc16_result = 0xFFFF;
	u16 gs_crc16_generation_code = 0x8408;

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

static void dsi_lanes_enable(struct mmp_dsi *dsi, int en)
{
	struct mmp_dsi_regs *dsi_regs = dsi->reg_base;
	void *phy_ctrl2_reg = (!DISP_GEN4(dsi->version)) ?
		(&dsi_regs->phy_ctrl2) : (&dsi_regs->phy.phy_ctrl2);
	u32 reg1 = readl_relaxed(phy_ctrl2_reg) &
		(~DSI_PHY_CTRL_2_CFG_CSR_LANE_EN_MASK);
	u32 reg2 = readl_relaxed(&dsi_regs->cmd1) &
		~(0xf << DSI_CPU_CMD_1_CFG_TXLP_LPDT_SHIFT);

	if (en) {
		reg1 |= (DSI_LANES(dsi->setting.lanes) <<
				DSI_PHY_CTRL_2_CFG_CSR_LANE_EN_SHIFT);
		reg2 |= (DSI_LANES(dsi->setting.lanes) <<
				DSI_CPU_CMD_1_CFG_TXLP_LPDT_SHIFT);
	}

	writel_relaxed(reg1, phy_ctrl2_reg);
	writel_relaxed(reg2, &dsi_regs->cmd1);
}

static void dsi_set_irq(struct mmp_dsi *dsi,
		unsigned int mask)
{
	struct mmp_dsi_regs *dsi_regs = dsi->reg_base;

	u32 reg = readl_relaxed(&dsi_regs->irq_mask);

	if (!DISP_GEN4(dsi->version))
		writel_relaxed(reg & ~mask , &dsi_regs->irq_mask);
	else
		writel_relaxed(reg | mask , &dsi_regs->irq_mask);
}

static void dsi_clear_irq(struct mmp_dsi *dsi,
		unsigned int mask)
{
	struct mmp_dsi_regs *dsi_regs = dsi->reg_base;

	u32 reg = readl_relaxed(&dsi_regs->irq_mask);

	if (!DISP_GEN4(dsi->version))
		writel_relaxed(reg | mask , &dsi_regs->irq_mask);
	else
		writel_relaxed(reg & (~mask) , &dsi_regs->irq_mask);
}

static int dsi_send_cmds(struct mmp_dsi *dsi, u8 *parameter,
		u8 count, u8 lp)
{
	struct mmp_dsi_regs *dsi_regs = dsi->reg_base;
	u32 send_data = 0, waddr, timeout, i, turnaround;
	u32 len;

	/* write all packet bytes to packet data buffer */
	for (i = 0; i < count; i++) {
		send_data |= parameter[i] << ((i % 4) * 8);
		if (!((i + 1) % 4)) {
			writel_relaxed(send_data, &dsi_regs->dat0);
			waddr = DSI_CFG_CPU_DAT_REQ_MASK |
				DSI_CFG_CPU_DAT_RW_MASK |
				((i - 3) << DSI_CFG_CPU_DAT_ADDR_SHIFT);
			writel_relaxed(waddr, &dsi_regs->cmd3);
			/* wait write operation done */
			timeout = 1000;
			while ((readl_relaxed(&dsi_regs->cmd3) &
					DSI_CFG_CPU_DAT_REQ_MASK) && timeout)
				timeout--;
			if (unlikely(!timeout))
				dev_warn(dsi->dev,
						"DSI write data to packet buffer not done\n");
			send_data = 0;
		}
	}

	/* handle last none 4Byte align data */
	if (i % 4) {
		writel_relaxed(send_data, &dsi_regs->dat0);
		waddr = DSI_CFG_CPU_DAT_REQ_MASK |
			DSI_CFG_CPU_DAT_RW_MASK |
			((4 * (i / 4)) << DSI_CFG_CPU_DAT_ADDR_SHIFT);
		writel_relaxed(waddr, &dsi_regs->cmd3);
		/* wait write operation done */
		timeout = 1000;
		while ((readl_relaxed(&dsi_regs->cmd3) &
					DSI_CFG_CPU_DAT_REQ_MASK) && timeout)
			timeout--;
		if (unlikely(!timeout))
			dev_warn(dsi->dev,
					"DSI write data to packet buffer not done.\n");
		send_data = 0;
	}

	if (parameter[0] == MIPI_DSI_DCS_READ)
		turnaround = 0x1;
	else
		turnaround = 0x0;

	len = count;

	if ((parameter[0] == MIPI_DSI_DCS_LONG_WRITE ||
		parameter[0] == MIPI_DSI_GENERIC_LONG_WRITE) &&
		(!(lp || DISP_GEN4(dsi->version))))
			len = count - 6;

	waddr = DSI_CFG_CPU_CMD_REQ_MASK |
		((count == 4) ? DSI_CFG_CPU_SP_MASK : 0) |
		(turnaround << DSI_CFG_CPU_TURN_SHIFT) |
		(lp ? DSI_CFG_CPU_TXLP_MASK : 0) |
		(len << DSI_CFG_CPU_WC_SHIFT);

	dsi_set_irq(dsi, IRQ_CPU_TX_DONE);
	/* send out the packet */
	writel_relaxed(waddr, &dsi_regs->cmd0);
	/* wait packet be sent out */
	if (!wait_for_completion_interruptible_timeout(&dsi->tx_done,
		HZ / 20)) {
			dev_warn(dsi->dev, "DSI send out packet timeout, irq status %x\n",
				readl_relaxed(&dsi_regs->irq_status));
			return -1;
	}

	return 0;
}

static int dsi_tx_cmds_common(struct mmp_dsi_port *dsi_port,
		struct mmp_dsi_cmd_desc cmds[], int count)
{
	struct mmp_dsi *dsi = mmp_dsi_port_to_dsi(dsi_port);
	struct mmp_dsi_cmd_desc cmd_line;
	u8 command, parameter[DSI_MAX_DATA_BYTES], len, *temp;
	u32 crc, loop;
	int i, j;
	int ret = 0;

	for (loop = 0; loop < count; loop++) {
		cmd_line = cmds[loop];
		command = cmd_line.data_type;
		len = cmd_line.length;
		memset(parameter, 0x00, len + 6);
		parameter[0] = command & 0xff;
		switch (command) {
		case MIPI_DSI_DCS_SHORT_WRITE:
		case MIPI_DSI_DCS_SHORT_WRITE_PARAM:
		case MIPI_DSI_DCS_READ:
		case MIPI_DSI_SET_MAXIMUM_RETURN_PACKET_SIZE:
			memcpy(&parameter[1], cmd_line.data, len);
			len = 4;
			break;
		case MIPI_DSI_DCS_LONG_WRITE:
		case MIPI_DSI_GENERIC_LONG_WRITE:
			parameter[1] = len & 0xff;
			parameter[2] = 0;
			memcpy(&parameter[4], cmd_line.data, len);
			crc = calculate_crc16(&parameter[4], len);
			parameter[len + 4] = crc & 0xff;
			parameter[len + 5] = (crc >> 8) & 0xff;
			len += 6;
			break;
		default:
			dev_err(dsi->dev, "%s: data type not supported 0x%x\n",
					__func__, command);
			break;
		}
		parameter[3] = calculate_ecc(parameter);

		/*
		 * support to send lower power mode command on multi lanes,
		 * ulc1 and helan3 hw only use lane0 for lower power commands
		 */
		if (!(DISP_GEN4_LITE(dsi->version) || DISP_GEN4_PLUS(dsi->version))
			&& cmd_line.lp) {
			temp = kzalloc(sizeof(u8) * DSI_MAX_DATA_BYTES * dsi->setting.lanes,
				GFP_KERNEL);
			if (temp == NULL)
				goto error;
			for (i = 0; i < len * dsi->setting.lanes; i += dsi->setting.lanes)
				for (j = 0; j < dsi->setting.lanes; j++)
					temp[i + j] = parameter[i/dsi->setting.lanes];
			len = dsi->setting.lanes * len;
			memcpy(parameter, temp, len);
			kfree(temp);
		}

		/* send dsi commands */
		ret = dsi_send_cmds(dsi, parameter, len, cmd_line.lp);
		if (ret < 0)
			goto error;

		if (cmd_line.delay)
			mdelay(cmd_line.delay);
	}

	return loop;
error:
	return -1;
}

static void dsi_set_dphy_timing(struct mmp_dsi *dsi)
{
	struct mmp_dsi_regs *dsi_regs = dsi->reg_base;
	int ui, lpx_clk, lpx_time, ta_get, ta_go, wakeup, reg;
	int hs_prep, hs_zero, hs_trail, hs_exit, ck_zero, ck_trail, ck_exit;
	int sclk_src, dsi_hsclk;
	void *phy_timing0_reg, *phy_timing1_reg,
	     *phy_timing2_reg, *phy_timing3_reg;

	sclk_src = clk_get_rate(dsi->clk);
	dsi_hsclk = sclk_src / 1000000;

	ui = 1000 / dsi_hsclk + 1;
	dev_dbg(dsi->dev, "ui:%d\n", ui);

	lpx_clk = (dphy_timing.lpx_constant + dphy_timing.lpx_ui * ui) /
		DSI_ESC_CLK_T + 1;
	lpx_time = lpx_clk * DSI_ESC_CLK_T;
	dev_dbg(dsi->dev, "lpx_clk:%d, condition (TIME_LPX:%d > 50)\n",
		lpx_clk, lpx_time);

	/* Below is for NT35451 */
	ta_get = lpx_time * 5 / DSI_ESC_CLK_T - 1;
	ta_go = lpx_time * 4 / DSI_ESC_CLK_T - 1;
	dev_dbg(dsi->dev,
		"ta_get:%d, condition (TIME_TA_GET:%d == 5*TIME_LPX:%d)\n",
		ta_get, (ta_get + 1) * DSI_ESC_CLK_T, lpx_time * 5);
	dev_dbg(dsi->dev,
		"ta_go:%d, condition (TIME_TA_GO:%d == 4*TIME_LPX:%d)\n",
		ta_go, (ta_go + 1) * DSI_ESC_CLK_T, lpx_time * 4);

	wakeup = dphy_timing.wakeup_constant;
	wakeup = wakeup / DSI_ESC_CLK_T + 1;
	dev_dbg(dsi->dev, "wakeup:%d, condition (WAKEUP:%d > MIN:%d)\n",
		wakeup, (wakeup + 1) * DSI_ESC_CLK_T, 1000000);

	hs_prep = dphy_timing.hs_prep_constant + dphy_timing.hs_prep_ui * ui;
	hs_prep = hs_prep / DSI_ESC_CLK_T + 1;
	dev_dbg(dsi->dev,
		"hs_prep:%d, condition (HS_PREP_MAX:%d > HS_PREP:%d > HS_PREP_MIN:%d)\n",
		hs_prep, 85 + 6 * ui, (hs_prep + 1) * DSI_ESC_CLK_T, 40 + 4 * ui);

	/*
	 * Our hardware added 3-byte clk automatically.
	 * 3-byte 3 * 8 * ui.
	 */
	hs_zero = dphy_timing.hs_zero_constant + dphy_timing.hs_zero_ui * ui -
		(hs_prep + 1) * DSI_ESC_CLK_T;
	hs_zero = (hs_zero - (3 * ui << 3)) / DSI_ESC_CLK_T + 4;
	if (hs_zero < 0)
		hs_zero = 0;
	dev_dbg(dsi->dev,
		"hs_zero:%d, condition (HS_ZERO + HS_PREP:%d > SUM_MIN:%d)\n",
		hs_zero, (hs_zero - 2) * DSI_ESC_CLK_T + 24 * ui +
		(hs_prep + 1) * DSI_ESC_CLK_T, 145 + 10 * ui);

	hs_trail = dphy_timing.hs_trail_constant + dphy_timing.hs_trail_ui * ui;
	hs_trail = ((8 * ui) >= hs_trail) ? (8 * ui) : hs_trail;
	hs_trail = hs_trail / DSI_ESC_CLK_T + 1;
	if (hs_trail > 3)
		hs_trail -= 3;
	else
		hs_trail = 0;
	dev_dbg(dsi->dev,
		"hs_trail:%d, condition (HS_TRAIL:%d > MIN1:%d / MIN2:%d)\n",
		hs_trail, (hs_trail + 3) * DSI_ESC_CLK_T, 8 * ui, 60 + 4 * ui);

	hs_exit = dphy_timing.hs_exit_constant + dphy_timing.hs_exit_ui * ui;
	hs_exit = hs_exit / DSI_ESC_CLK_T + 1;
	dev_dbg(dsi->dev, "hs_exit:%d, condition (HS_EXIT:%d > MIN:%d)\n",
		hs_exit, (hs_exit + 1) * DSI_ESC_CLK_T, 100);

	ck_zero = dphy_timing.ck_zero_constant + dphy_timing.ck_zero_ui * ui -
		(hs_prep + 1) * DSI_ESC_CLK_T;
	ck_zero = ck_zero / DSI_ESC_CLK_T + 1;
	dev_dbg(dsi->dev,
		"ck_zero:%d, condition (CK_ZERO + CK_PREP:%d > SUM_MIN:%d)\n",
		ck_zero, (ck_zero + 1) * DSI_ESC_CLK_T +
		(hs_prep + 1) * DSI_ESC_CLK_T, 300);

	ck_trail = dphy_timing.ck_trail_constant + dphy_timing.ck_trail_ui * ui;
	ck_trail = ck_trail / DSI_ESC_CLK_T + 1;
	dev_dbg(dsi->dev, "ck_trail:%d, condition (CK_TRIAL:%d > MIN:%d)\n",
		ck_trail, (ck_trail + 1) * DSI_ESC_CLK_T, 60);

	ck_exit = hs_exit;
	dev_dbg(dsi->dev, "ck_exit:%d\n", ck_exit);

	if (!DISP_GEN4(dsi->version)) {
		/* bandgap ref enabled by default */
		reg = readl_relaxed(&dsi_regs->phy_rcomp0);
		reg |= DPHY_BG_VREF_EN;
		writel_relaxed(reg, &dsi_regs->phy_rcomp0);

		phy_timing0_reg = &dsi_regs->phy_timing0;
		phy_timing1_reg = &dsi_regs->phy_timing1;
		phy_timing2_reg = &dsi_regs->phy_timing2;
		phy_timing3_reg = &dsi_regs->phy_timing3;
	} else {
		/* bandgap ref enabled by default */
		reg = readl_relaxed(&dsi_regs->phy.phy_rcomp0);
		reg |= DPHY_BG_VREF_EN_DC4;
		writel_relaxed(reg, &dsi_regs->phy.phy_rcomp0);

		phy_timing0_reg = &dsi_regs->phy.phy_timing0;
		phy_timing1_reg = &dsi_regs->phy.phy_timing1;
		phy_timing2_reg = &dsi_regs->phy.phy_timing2;
		phy_timing3_reg = &dsi_regs->phy.phy_timing3;
	}

	/* timing_0 */
	reg = (hs_exit << DSI_PHY_TIME_0_CFG_CSR_TIME_HS_EXIT_SHIFT)
		| (hs_trail << DSI_PHY_TIME_0_CFG_CSR_TIME_HS_TRAIL_SHIFT)
		| (hs_zero << DSI_PHY_TIME_0_CDG_CSR_TIME_HS_ZERO_SHIFT)
		| (hs_prep);
	writel_relaxed(reg, phy_timing0_reg);

	reg = (ta_get << DSI_PHY_TIME_1_CFG_CSR_TIME_TA_GET_SHIFT)
		| (ta_go << DSI_PHY_TIME_1_CFG_CSR_TIME_TA_GO_SHIFT)
		| wakeup;
	writel_relaxed(reg, phy_timing1_reg);

	reg = (ck_exit << DSI_PHY_TIME_2_CFG_CSR_TIME_CK_EXIT_SHIFT)
		| (ck_trail << DSI_PHY_TIME_2_CFG_CSR_TIME_CK_TRAIL_SHIFT)
		| (ck_zero << DSI_PHY_TIME_2_CFG_CSR_TIME_CK_ZERO_SHIFT)
		| (lpx_clk - 1);
	writel_relaxed(reg, phy_timing2_reg);

	reg = ((lpx_clk - 1) << DSI_PHY_TIME_3_CFG_CSR_TIME_LPX_SHIFT) |
	      dphy_timing.req_ready;
	writel_relaxed(reg, phy_timing3_reg);
}

void dsi_set_ctrl0(struct mmp_dsi_regs *dsi_regs,
		u32 mask, u32 set)
{
	u32 tmp;

	tmp = readl_relaxed(&dsi_regs->ctrl0);
	tmp &= ~mask;
	tmp |= set;
	writel_relaxed(tmp, &dsi_regs->ctrl0);
}

static void dsi_reset(struct mmp_dsi_regs *dsi_regs, int hold)
{
	/* software reset DSI module */
	dsi_set_ctrl0(dsi_regs, 0, DSI_CTRL_0_CFG_SOFT_RST);
	/* Note: there need some delay after set DSI_CTRL_0_CFG_SOFT_RST */
	usleep_range(800, 1000);
	/* disable video panel, reset DSI configuration registers*/
	dsi_set_ctrl0(dsi_regs, DSI_CTRL_0_CFG_LCD1_EN,
			DSI_CTRL_0_CFG_SOFT_RST_REG);

	if (!hold) {
		dsi_set_ctrl0(dsi_regs, DSI_CTRL_0_CFG_SOFT_RST, 0);
		usleep_range(800, 1000);
		dsi_set_ctrl0(dsi_regs, DSI_CTRL_0_CFG_SOFT_RST_REG, 0);
	}
}

/*
 * The format is different from LCD color format.
 * It's the input data format for DSI module,
 * which depends on the panel connected to DSI.
 *   For example, we should select PIXFMT_RGB565
 *   if the panel only supports it.
 *
 * return the bpp of the inputfmt
 */
static u32 dsi_get_rgb_type_from_inputfmt(int fmt, u32 *rgb_type)
{
	switch (fmt) {
	case PIXFMT_BGR565:
		*rgb_type = 0;
		return 16;
	case PIXFMT_BGR666PACK:
		*rgb_type = 1;
		return 18;
	case PIXFMT_BGR666UNPACK:
		*rgb_type = 2;
		return 24;
	case PIXFMT_BGR888PACK:
		*rgb_type = 3;
		return 24;
	default:
		pr_err("%s, err: fmt not support or mode not set\n", __func__);
		return 0;
	}
}

static int get_total_screen_size(struct mmp_mode *mode,
		u32 *total_w, u32 *total_h)
{
	*total_w = mode->xres + mode->left_margin +
		 mode->right_margin + mode->hsync_len;
	*total_h = mode->yres + mode->upper_margin +
		 mode->lower_margin + mode->vsync_len;
	if (!(*total_w) || !(*total_h)) {
		pr_err("%s, err: we should set mode firstly\n", __func__);
		return -EINVAL;
	}

	return 0;
}

static void dsi_set_video_panel(struct mmp_dsi *dsi)
{
	struct mmp_dsi_regs *dsi_regs = dsi->reg_base;
	struct mmp_mode *mode = &dsi->mode;
	/* FIXME: use active panel1 by default */
	struct dsi_lcd_regs *dsi_lcd = &dsi_regs->lcd1;
	u32 hsync_b, hbp_b, hact_b, hex_b, hfp_b, httl_b;
	u32 hsync, hbp, hact, hfp, httl, total_w, total_h;
	u32 hsa_wc, hbp_wc, hact_wc, hex_wc, hfp_wc, hlp_wc, bpp, rgb_type;
	u32 hss_bcnt = 4, hse_bct = 4, lgp_over_head = 6, reg;
	u32 slot_cnt0, slot_cnt1;
	union dsi_lcd_ctrl1 lcd_ctrl1;

	bpp = dsi_get_rgb_type_from_inputfmt(mode->pix_fmt_out, &rgb_type);
	if (unlikely(!bpp))
		return;
	if (get_total_screen_size(mode, &total_w, &total_h) < 0)
		return;

	hact_b = to_dsi_bcnt(mode->xres, bpp);
	hfp_b = to_dsi_bcnt(mode->right_margin, bpp);
	hbp_b = to_dsi_bcnt(mode->left_margin, bpp);
	hsync_b = to_dsi_bcnt(mode->hsync_len, bpp);
	hex_b = to_dsi_bcnt(DSI_EX_PIXEL_CNT, bpp);
	httl_b = hact_b + hsync_b + hfp_b + hbp_b + hex_b;

	hact = hact_b / dsi->setting.lanes;
	hfp = hfp_b / dsi->setting.lanes;
	hbp = hbp_b / dsi->setting.lanes;
	hsync = hsync_b / dsi->setting.lanes;
	httl = (hact_b + hfp_b + hbp_b + hsync_b) / dsi->setting.lanes;

	slot_cnt0 = slot_cnt1 = httl - hact + 3;
	/* word count in the unit of byte */
	hsa_wc = (dsi->setting.burst_mode == DSI_BURST_MODE_SYNC_PULSE) ?
		(hsync_b - hss_bcnt - lgp_over_head) : 0;

	/* Hse is with backporch */
	hbp_wc = (dsi->setting.burst_mode == DSI_BURST_MODE_SYNC_PULSE) ?
		(hbp_b - hse_bct - lgp_over_head)
		: (hsync_b + hbp_b - hss_bcnt - lgp_over_head);

	hfp_wc = ((dsi->setting.burst_mode == DSI_BURST_MODE_BURST) &&
		(DSI_HEX_EN == 0)) ?
		(hfp_b + hex_b - lgp_over_head - lgp_over_head) :
		(hfp_b - lgp_over_head - lgp_over_head);

	hact_wc =  ((mode->xres) * bpp) >> 3;

	/* disable Hex currently */
	hex_wc = 0;

	/* There is no hlp with active data segment. */
	hlp_wc = (dsi->setting.burst_mode == DSI_BURST_MODE_SYNC_PULSE) ?
		(httl_b - hsync_b - hse_bct - lgp_over_head) :
		(httl_b - hss_bcnt - lgp_over_head);

	/*
	 * FIXME - need to double check the (*3) is bytes_per_pixel from
	 * input data or output to panel
	 */
	/* dsi_lane_enable - Set according to specified DSI lane count */
	if (!DISP_GEN4(dsi->version))
		writel_relaxed(DSI_LANES(dsi->setting.lanes) <<
			DSI_PHY_CTRL_2_CFG_CSR_LANE_EN_SHIFT,
			&dsi_regs->phy_ctrl2);
	else
		writel_relaxed(DSI_LANES(dsi->setting.lanes) <<
			DSI_PHY_CTRL_2_CFG_CSR_LANE_EN_SHIFT,
			&dsi_regs->phy.phy_ctrl2);
	writel_relaxed(
		DSI_LANES(dsi->setting.lanes) << DSI_CPU_CMD_1_CFG_TXLP_LPDT_SHIFT,
		&dsi_regs->cmd1);

	/* SET UP LCD1 TIMING REGISTERS FOR DSI BUS */
	/* NOTE: Some register values were obtained by trial and error */
	writel_relaxed((hact << 16) | httl, &dsi_lcd->timing0);
	writel_relaxed((hsync << 16) | hbp, &dsi_lcd->timing1);
	/*
	 * For now the active size is set really low (we'll use 10) to allow
	 * the hardware to attain V Sync. Once the DSI bus is up and running,
	 * the final value will be put in place for the active size (this is
	 * done below).
	 */
	writel_relaxed(((mode->yres) << 16) | (total_h), &dsi_lcd->timing2);
	writel_relaxed(((mode->vsync_len) << 16) | (mode->upper_margin),
		&dsi_lcd->timing3);

	/* SET UP LCD1 WORD COUNT REGISTERS FOR DSI BUS */
	/* Set up for word(byte) count register 0 */
	writel_relaxed((hbp_wc << 16) | hsa_wc, &dsi_lcd->wc0);
	writel_relaxed((hfp_wc << 16) | hact_wc, &dsi_lcd->wc1);
	writel_relaxed((hex_wc << 16) | hlp_wc, &dsi_lcd->wc2);

	writel_relaxed((slot_cnt0 << 16) | slot_cnt0, &dsi_lcd->slot_cnt0);
	writel_relaxed((slot_cnt1 << 16) | slot_cnt1, &dsi_lcd->slot_cnt1);

	/* Configure LCD control register 1 FOR DSI BUS */
	lcd_ctrl1.val = readl_relaxed(&dsi_lcd->ctrl1);
	lcd_ctrl1.bit.input_fmt = rgb_type;
	lcd_ctrl1.bit.burst_mode = dsi->setting.burst_mode;
	lcd_ctrl1.bit.lpm_frame_en = dsi->setting.lpm_frame_en;
	lcd_ctrl1.bit.last_line_turn = dsi->setting.last_line_turn;
	lcd_ctrl1.bit.hex_slot_en = 0;
	if (dsi->setting.burst_mode) {
		lcd_ctrl1.bit.hsa_pkt_en = dsi->setting.hsa_en;
		lcd_ctrl1.bit.hse_pkt_en = dsi->setting.hse_en;
	} else {
		lcd_ctrl1.bit.hsa_pkt_en = 0;
		lcd_ctrl1.bit.hse_pkt_en = 0;
	}
	lcd_ctrl1.bit.hbp_pkt_en = dsi->setting.hbp_en;
	lcd_ctrl1.bit.hfp_pkt_en = dsi->setting.hfp_en;
	lcd_ctrl1.bit.hex_pkt_en = 0;
	lcd_ctrl1.bit.hlp_pkt_en = dsi->setting.hlp_en;
	lcd_ctrl1.bit.vsync_rst_en = 1;
	if (!DISP_GEN4(dsi->version)) {
		/* These bits were removed for DC4 */
		lcd_ctrl1.bit.lpm_line_en = dsi->setting.lpm_line_en;
		lcd_ctrl1.bit.hact_pkt_en = dsi->setting.hact_en;
		lcd_ctrl1.bit.m2k_en = 0;
		lcd_ctrl1.bit.all_slot_en = 0;
	} else {
		/*
		 * FIXME: some extend features in DC4, default enable
		 * 1. word count auto calculation.
		 * 2. check timing before request DPHY for TX.
		 * 3. auto vsync delay count calculation.
		 */
		lcd_ctrl1.bit.auto_wc_dis = 0;
		lcd_ctrl1.bit.hact_wc_en = 0;
		lcd_ctrl1.bit.timing_check_dis = 0;
		lcd_ctrl1.bit.auto_dly_dis = 0;
	}
	writel_relaxed(lcd_ctrl1.val, &dsi_lcd->ctrl1);

	/* Start the transfer of LCD data over the DSI bus */
	/* DSI_CTRL_1 */
	reg = readl_relaxed(&dsi_regs->ctrl1);
	reg &= ~(DSI_CTRL_1_CFG_EOTP);
	if (dsi->setting.eotp_en)
		reg |= DSI_CTRL_1_CFG_EOTP;  /* EOTP */
	writel_relaxed(reg, &dsi_regs->ctrl1);

	/* DSI_CTRL_0 */
	reg = dsi->setting.master_mode ? 0 : DSI_CTRL_0_CFG_LCD1_SLV;
	dsi_set_ctrl0(dsi_regs, DSI_CTRL_0_CFG_LCD1_SLV,
		reg | DSI_CTRL_0_CFG_LCD1_TX_EN | DSI_CTRL_0_CFG_LCD1_EN);
}

static void print_ack_errors(u32 packet)
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

	pr_err("Ack error packet = 0x%x", packet);
	for (i = 0; i < 15; i++) {
		if (packet & (1 << i))
			pr_err("\t\tBit %d = %s",
					i, ack_error_info[i]);
	}
}

static int dsi_tx_cmds(struct mmp_dsi_port *dsi_port,
		struct mmp_dsi_cmd_desc cmds[], int count)
{
	int ret;

	mutex_lock(&dsi_port->dsi_ok);
	ret = dsi_tx_cmds_common(dsi_port, cmds, count);
	if (ret < 0)
		goto error;
	mutex_unlock(&dsi_port->dsi_ok);
	return 0;
error:
	mutex_unlock(&dsi_port->dsi_ok);
	return -1;
}

static int dsi_rx_cmds(struct mmp_dsi_port *dsi_port, struct mmp_dsi_buf *dbuf,
		struct mmp_dsi_cmd_desc cmds[], int count)
{
	struct mmp_dsi *dsi = mmp_dsi_port_to_dsi(dsi_port);
	struct mmp_dsi_regs *dsi_regs = dsi->reg_base;
	u8 parameter[DSI_MAX_DATA_BYTES];
	u32 i, rx_reg, timeout, tmp, packet,
	    data_pointer, packet_count, byte_count;
	int ret = 0;

	mutex_lock(&dsi_port->dsi_ok);
	memset(dbuf, 0x0, sizeof(struct mmp_dsi_buf));
	ret = dsi_tx_cmds_common(dsi_port, cmds, count);
	if (ret < 0)
		goto error;

	tmp = readl_relaxed(&dsi_regs->irq_status);
	if (tmp & IRQ_RX_TRG2)
		dev_dbg(dsi->dev, "ACK receiveddf\n");
	if (tmp & IRQ_RX_TRG1)
		dev_dbg(dsi->dev, "TE trigger received\n");
	if (tmp & IRQ_RX_ERR) {
		dev_err(dsi->dev, "ACK with error report\n");
		tmp = readl_relaxed(&dsi_regs->rx0_header);
		print_ack_errors(tmp);
	}

	packet = readl_relaxed(&dsi_regs->rx0_status);
	if (packet & RX_PKT0_ST_VLD)
		dev_dbg(dsi->dev, "Rx packet 0 status valid, 0x%x\n", packet);
	if (packet & RX_PKT0_ST_SP)
		dev_dbg(dsi->dev, "Rx packet is short packet\n");
	else
		dev_dbg(dsi->dev, "Rx packet is long packet\n");

	data_pointer = (packet & RX_PKT0_PKT_PTR_MASK) >> RX_PKT0_PKT_PTR_SHIFT;
	dev_dbg(dsi->dev, "Rx FIFO data pointer %x\n", data_pointer);
	tmp = readl_relaxed(&dsi_regs->rx_ctrl1);
	packet_count = (tmp & RX_PKT_CNT_MASK) >> RX_PKT_CNT_SHIFT;
	byte_count = tmp & RX_PKT_BCNT_MASK;
	dev_dbg(dsi->dev, "Rx FIFO packet count %d, byte count %d\n",
			packet_count, byte_count);

	memset(parameter, 0x0, byte_count);
	for (i = data_pointer; i < data_pointer + byte_count; i++) {
		rx_reg = readl_relaxed(&dsi_regs->rx_ctrl);
		rx_reg &= ~RX_PKT_RD_PTR_MASK;
		rx_reg |= RX_PKT_RD_REQ | (i << RX_PKT_RD_PTR_SHIFT);
		writel_relaxed(rx_reg, &dsi_regs->rx_ctrl);
		timeout = 10000;
		while (((rx_reg = readl_relaxed(&dsi_regs->rx_ctrl)) &
				RX_PKT_RD_REQ) && timeout)
			timeout--;
		if (unlikely(!timeout))
			dev_err(dsi->dev,
				"Rx packet FIFO read request not assert\n");
		parameter[i - data_pointer] = rx_reg & 0xff;
	}
	switch (parameter[0]) {
	case MIPI_DSI_RX_ACKNOWLEDGE_AND_ERROR_REPORT:
		dev_dbg(dsi->dev, "Acknowledge with erro report\n");
		break;
	case MIPI_DSI_RX_END_OF_TRANSMISSION:
		dev_dbg(dsi->dev, "End of Transmission packet\n");
		break;
	case MIPI_DSI_RX_GENERIC_SHORT_READ_RESPONSE_1BYTE:
	case MIPI_DSI_RX_DCS_SHORT_READ_RESPONSE_1BYTE:
		dbuf->data_type = parameter[0];
		dbuf->length = 1;
		memcpy(dbuf->data, &parameter[1], dbuf->length);
		break;
	case MIPI_DSI_RX_GENERIC_SHORT_READ_RESPONSE_2BYTE:
	case MIPI_DSI_RX_DCS_SHORT_READ_RESPONSE_2BYTE:
		dbuf->data_type = parameter[0];
		dbuf->length = 2;
		memcpy(dbuf->data, &parameter[1], dbuf->length);
		break;
	case MIPI_DSI_RX_GENERIC_LONG_READ_RESPONSE:
	case MIPI_DSI_RX_DCS_LONG_READ_RESPONSE:
		dbuf->data_type = parameter[0];
		dbuf->length = (parameter[2] << 8) | parameter[1];
		if (dbuf->length > DSI_MAX_DATA_BYTES)
			dbuf->length = DSI_MAX_DATA_BYTES;
		memcpy(dbuf->data, &parameter[4], dbuf->length);
		break;
	default:
		dev_err(dsi->dev, "%s: not support\n", __func__);
		goto error;
	}

	mutex_unlock(&dsi_port->dsi_ok);
	return 0;
error:
	mutex_unlock(&dsi_port->dsi_ok);
	return -1;
}

static unsigned long clk_calculate(struct mmp_dsi *dsi)
{
	struct mmp_path *path = mmp_get_path(dsi->plat_path_name);
	struct mmphw_ctrl *ctrl = path_to_ctrl(path);
	struct mmp_mode *mode = &dsi->mode;
	u32 total_w, total_h, byteclk, bitclk, bitclk_div = 1, bpp, rgb_type, refresh;

	bpp = dsi_get_rgb_type_from_inputfmt(mode->pix_fmt_out, &rgb_type);
	if (unlikely(!bpp))
		return 0;
	if (get_total_screen_size(mode, &total_w, &total_h) < 0)
		return 0;

	refresh = mode->real_refresh ? mode->real_refresh : mode->refresh;
	byteclk = ((total_w * (bpp >> 3)) * total_h *
			refresh) / dsi->setting.lanes;
	bitclk = byteclk << 3;

	/* The minimum of DSI pll is 150MHz */
	if (bitclk < 150000000)
		bitclk_div = 150000000 / bitclk + 1;

	return mmp_disp_clk_round_rate(ctrl, bitclk * bitclk_div);
}

#ifdef CONFIG_OF
static void dsi_powerdomain_on(struct mmp_dsi *dsi, int on)
{
	static struct regulator *dsi_avdd;

	if (!dsi_avdd) {
		dsi_avdd = regulator_get(dsi->dev, "dsi_avdd");
		if (IS_ERR(dsi_avdd)) {
			pr_err("%s no power domain\n", __func__);
			return;
		}
	} else if (IS_ERR(dsi_avdd))
		return;

	if (on) {
		regulator_set_voltage(dsi_avdd, 1200000, 1200000);
		if (regulator_enable(dsi_avdd) < 0) {
			dsi_avdd = NULL;
			return;
		}
		usleep_range(10000, 12000);
	} else
		regulator_disable(dsi_avdd);
}
#else
static void dsi_powerdomain_on(struct mmp_dsi *dsi, int on) {}
#endif

static void dsi_set_dphy_ctrl1(struct mmp_dsi_regs *dsi_regs,
		u32 mask, u32 set, u32 version)
{
	void *phy_ctrl1_reg = (!DISP_GEN4(version)) ?
		(&dsi_regs->phy_ctrl1) : (&dsi_regs->phy.phy_ctrl1);
	u32 tmp;

	tmp = readl_relaxed(phy_ctrl1_reg);
	tmp &= ~mask;
	tmp |= set;
	writel_relaxed(tmp, phy_ctrl1_reg);
}

static void dsi_ulps_set_on(struct mmp_dsi_port *dsi_port, int status)
{
	struct mmp_dsi *dsi = mmp_dsi_port_to_dsi(dsi_port);
	struct mmp_dsi_regs *dsi_regs = dsi->reg_base;
	u32 reg;

	if (status) {
		/* dsi set panel intf */
		dsi_set_ctrl0(dsi_regs,
			DSI_CTRL_0_CFG_LCD1_TX_EN | DSI_CTRL_0_CFG_LCD1_EN,
			0x0);

		if (!dsi->setting.non_continuous_clk)
			/* disable continuous clock */
			dsi_set_dphy_ctrl1(dsi_regs, 0x1, 0x0, dsi->version);

		/* reset DSI modules */
		dsi_set_ctrl0(dsi_regs, 0x0, DSI_CTRL_0_CFG_SOFT_RST);
		usleep_range(800, 1000);
		dsi_set_ctrl0(dsi_regs, DSI_CTRL_0_CFG_SOFT_RST, 0x0);

		/* enter ulps mode */
		reg = readl(&dsi_regs->cmd1) |
			DSI_CPU_CMD_1_CFG_TXLP_ULPS_MASK;
		writel(reg, &dsi_regs->cmd1);

		dsi_set_dphy_ctrl1(dsi_regs, 0x0, DPHY_CFG_FORCE_ULPS,
				dsi->version);
	} else {
		reg = readl(&dsi_regs->cmd1) &
			~DSI_CPU_CMD_1_CFG_TXLP_ULPS_MASK;
		writel(reg, &dsi_regs->cmd1);

		dsi_set_dphy_ctrl1(dsi_regs, DPHY_CFG_FORCE_ULPS, 0x0,
				dsi->version);
	}
}

static void dsi_set_on(struct mmp_dsi *dsi, int status)
{
	struct mmp_dsi_regs *dsi_regs = dsi->reg_base;
	struct mmp_path *path = mmp_get_path(dsi->plat_path_name);

	if (status) {
		dsi_powerdomain_on(dsi, 1);
		clk_prepare_enable(dsi->clk);

		/* reset and de-assert DSI controller */
		dsi_reset(dsi_regs, 0);

		/* digital and analog power on */
		dsi_set_dphy_ctrl1(dsi_regs, 0, DPHY_CFG_VDD_VALID,
				dsi->version);

		if (path && path->panel && path->panel->set_power)
			path->panel->set_power(path->panel, MMP_ON);

		/* set dphy timing */
		dsi_set_dphy_timing(dsi);

		/* enable data lanes */
		dsi_lanes_enable(dsi, 1);

		if (path && path->panel && path->panel->set_status)
			path->panel->set_status(path->panel, MMP_ON);

		if (!dsi->setting.non_continuous_clk)
			/* turn on DSI continuous clock for HS */
			dsi_set_dphy_ctrl1(dsi_regs, 0x0, 0x1, dsi->version);

		/* set video panel */
		dsi_set_video_panel(dsi);

		if (path && path->panel && path->panel->set_mode)
			path->panel->set_mode(path->panel, &path->mode);
		if (path && path->panel && path->panel->panel_start)
			path->panel->panel_start(path->panel, 1);
	} else {
		if (path && path->panel && path->panel->panel_start)
			path->panel->panel_start(path->panel, 0);

		/* disable data lanes */
		dsi_lanes_enable(dsi, 0);

		if (!dsi->setting.non_continuous_clk)
			/* disable continuous clock for HS */
			dsi_set_dphy_ctrl1(dsi_regs, 0x1, 0x0, dsi->version);

		if (path && path->panel && path->panel->set_status)
			path->panel->set_status(path->panel, MMP_OFF);

		/* digital and analog power off */
		dsi_set_dphy_ctrl1(dsi_regs, DPHY_CFG_VDD_VALID, 0,
				dsi->version);

		/* reset DSI controller */
		dsi_reset(dsi_regs, 1);

		clk_disable_unprepare(dsi->clk);
		dsi_powerdomain_on(dsi, 0);
	}

	dev_info(dsi->dev, "%s status %d\n", __func__, status);
}

static void dsi_set_status(struct mmp_dsi *dsi, int status)
{
	struct mmp_path *path = mmp_get_path(dsi->plat_path_name);

	if (MMP_RESET == status) {
		mutex_lock(&dsi->lock);
		if (dsi->status) {
			dsi_set_on(dsi, MMP_OFF);
			dsi_set_on(dsi, MMP_ON);
		}
		mutex_unlock(&dsi->lock);
		return;
	}

	if (!status && path->panel && path->panel->esd_set_onoff)
		path->panel->esd_set_onoff(path->panel, 0);
	mutex_lock(&dsi->lock);
	dsi->status = status_is_on(status);
	if (MMP_ON_REDUCED == status) {
		dsi_powerdomain_on(dsi, 1);
		clk_prepare_enable(dsi->clk);
	} else
		dsi_set_on(dsi, status_is_on(status));
	mutex_unlock(&dsi->lock);
	if (status && path->panel && path->panel->esd_set_onoff)
		path->panel->esd_set_onoff(path->panel, 1);
}

static inline int check_mode_is_changed(struct mmp_mode *mode_old,
		struct mmp_mode *mode)
{
	return memcmp((void *)mode_old, (void *)mode,
			sizeof(struct mmp_mode));
}

static void dsi_set_mode(struct mmp_dsi *dsi, struct mmp_mode *mode)
{
	struct mmp_path *path = mmp_get_path(dsi->plat_path_name);
	unsigned long clk_rate;
	static unsigned char initial_once = 1;
	u32 tmp, value;

	if (check_mode_is_changed(&dsi->mode, mode)) {
		memcpy(&dsi->mode, mode, sizeof(struct mmp_mode));
		clk_rate = clk_calculate(dsi);
		if (unlikely(!clk_rate))
			return;

		clk_set_rate(dsi->clk, clk_rate);

		/*
		* pixel clk and dsi bit clk have different register bits to select source,
		* it is possbile that they select different parent clock, in order to avoid
		* this possibility, decide to use dsi->clk to select clock for pixel clk,
		* and pixel clk only can set the divider, can't change parent; so after
		* select the clock source for pixel clk, need copy the clock source from
		* pixel source register bits(31:29) to dsi clock source register bits(13:12)
		*/
		if (DISP_GEN4(dsi->version)) {
			tmp = readl_relaxed(ctrl_regs(path) + LCD_SCLK_DIV);
			if (!DISP_GEN4_LITE(dsi->version))
				value = tmp & SCLK_SOURCE_SELECT_MASK;
			else
				value = tmp & SCLK_SOURCE_SELECT_DC4_LITE_MASK;
			value >>= (SCLK_SOURCE_SELECT_OFFSET - DSI1_BITCLK_SOURCE_SELECT_OFFSET);
			value &= DSI1_BITCLK_SROUCE_SELECT_MASK;
			tmp &= ~(DSI1_BITCLK_SROUCE_SELECT_MASK);
			writel_relaxed(tmp  | value, ctrl_regs(path) + LCD_SCLK_DIV);
		}

		dsi->set_status(dsi, MMP_RESET);

		/*
		 * FIXME: save bit clk rate as path original rate.
		 */
		if (initial_once) {
			path->original_rate = clk_get_rate(dsi->clk) / 1000000;
			initial_once = 0;
		}
	}
}

static int dsi_irq_set(struct mmp_dsi *dsi, int on)
{
	struct mmp_dsi_regs *dsi_regs = dsi->reg_base;
	u32 mask = IRQ_VPN_VSYNC | IRQ_VACT_DONE;
	u32 reg = readl_relaxed(&dsi_regs->irq_mask);

	if (!on) {
		if (atomic_read(&dsi->irq_en_ref))
			if (atomic_dec_and_test(&dsi->irq_en_ref)) {
				if (!DISP_GEN4(dsi->version))
					writel_relaxed(reg | mask , &dsi_regs->irq_mask);
				else
					writel_relaxed(reg & (~mask) , &dsi_regs->irq_mask);
			}
	} else {
		if (!atomic_read(&dsi->irq_en_ref)) {
			if (!DISP_GEN4(dsi->version))
				writel_relaxed(reg & ~mask , &dsi_regs->irq_mask);
			else
				writel_relaxed(reg | mask , &dsi_regs->irq_mask);
		}
		atomic_inc(&dsi->irq_en_ref);
	}

	return atomic_read(&dsi->irq_en_ref) > 0;
}

static u32 dsi_get_sync_val(struct mmp_dsi *dsi, int type)
{
	u32 x, bpp = dsi_get_rgb_type_from_inputfmt(
			dsi->mode.pix_fmt_out, &x);

	switch (type) {
	case DSI_SYNC_RATIO:
		if (DISP_GEN4(dsi->version))
			return 10;
		return ((dsi->setting.lanes > 2) ? 2 : 1)
			* 10 * bpp / (8 * dsi->setting.lanes);
	case DSI_SYNC_CLKREQ:
		if (DISP_GEN4(dsi->version))
			/* FIXME: make sure pixel_clk * bpp > bit_clk * lanes */
			return (u32)(clk_calculate(dsi) * dsi->setting.lanes
					+ bpp - 1) / bpp;
		x = (dsi->setting.lanes > 2) ? 2 : 3;
		return (u32)(clk_calculate(dsi) >> x);
	case DSI_SYNC_MASTER_MODE:
		return dsi->setting.master_mode;
	default:
		dev_err(dsi->dev, "%s: type %d not supported\n",
			__func__, type);
		return 0;
	}
}

static irqreturn_t dsi_handle_irq(int irq, void *dev_id)
{
	struct mmp_dsi *dsi = dev_id;
	struct mmp_dsi_regs *dsi_regs = dsi->reg_base;
	u32 isr_en, irq_mask, irq_status;

	irq_status = readl_relaxed(&dsi_regs->irq_status);
	irq_mask = readl_relaxed(&dsi_regs->irq_mask);
	if (!DISP_GEN4(dsi->version))
		isr_en = irq_status & ~irq_mask;
	else {
		/*
		 * For DC4, set 1 means enable it,
		 * set status as 1 to clear it
		 */
		isr_en = irq_status & irq_mask;
		writel_relaxed(isr_en, &dsi_regs->irq_status);
	}

	if (isr_en & IRQ_VACT_DONE) {
		if (!(isr_en & IRQ_VPN_VSYNC)) {
			if (dsi->special_vsync.handle_irq)
				dsi->special_vsync.handle_irq(&dsi->special_vsync);
		}

		if (dsi->vsync.handle_irq)
			dsi->vsync.handle_irq(&dsi->vsync);
	}

	if (isr_en & IRQ_CPU_TX_DONE) {
		dsi_clear_irq(dsi, IRQ_CPU_TX_DONE);
		complete(&dsi->tx_done);
	}

	return IRQ_HANDLED;
}

static void mmp_dsi_create_dsi_port(struct mmp_dsi *dsi)
{
	/*FIXME: not changed to register yet, just init */
	dsi->dsi_port.name = dsi->name;
	dsi->dsi_port.tx_cmds = dsi_tx_cmds;
	dsi->dsi_port.rx_cmds = dsi_rx_cmds;
	dsi->dsi_port.ulps_set_on = dsi_ulps_set_on;
	mutex_init(&dsi->dsi_port.dsi_ok);
}

#ifdef CONFIG_OF
static int mmp_dsi_probe_dt(struct platform_device *pdev, struct mmp_dsi *dsi)
{
	struct device_node *np = pdev->dev.of_node;
	u32 tmp;
	int ret;

	if (!np)
		return -EINVAL;
	ret = of_property_read_string(np, "marvell,phy-name", &dsi->name);
	if (ret < 0)
		goto out;
	ret = of_property_read_string(np, "marvell,plat-path-name", &dsi->plat_path_name);
	if (ret < 0)
		goto out;
	ret = of_property_read_u32(np, "marvell,dsi-lanes", &tmp);
	if (ret < 0)
		goto out;
	else
		dsi->setting.lanes = tmp;

	ret = of_property_read_u32(np, "marvell,burst-mode", &tmp);
	if (ret < 0)
		goto out;
	else {
		switch (tmp) {
		case 0:
			dsi->setting.burst_mode = DSI_BURST_MODE_SYNC_PULSE;
			break;
		case 1:
			dsi->setting.burst_mode = DSI_BURST_MODE_SYNC_EVENT;
			break;
		case 2:
			dsi->setting.burst_mode = DSI_BURST_MODE_BURST;
			break;
		}
	}

	if (of_property_read_bool(np, "marvell,vsync-slave")) {
		dsi->setting.master_mode = 0;
		dsi->setting.hfp_en = 1;
		dsi->setting.hbp_en = 1;
	} else {
		dsi->setting.master_mode = 1;
		dsi->setting.hfp_en = 0;
		dsi->setting.hbp_en = 0;
	}
	return 0;
out:
	return ret;
}
#endif

/* DSI IP initialisation */
static int mmp_dsi_probe(struct platform_device *pdev)
{
	struct mmp_mach_dsi_info *mi;
	struct mmp_dsi *dsi = NULL;
	struct resource *res;
	struct mmp_path *path;
	struct mmphw_ctrl *ctrl;
	int irq = 0, ret = 0;

	/* get resources from platform data */
	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if (res == NULL) {
		dev_err(&pdev->dev, "%s: no IO memory defined\n", __func__);
		ret = -ENOENT;
		goto failed;
	}

	irq = platform_get_irq(pdev, 0);
	if (irq < 0) {
		dev_err(&pdev->dev, "%s: no IRQ defined\n", __func__);
		ret = -ENOENT;
		goto failed;
	}

	dsi = devm_kzalloc(&pdev->dev, sizeof(*dsi), GFP_KERNEL);
	if (dsi == NULL) {
		ret = -ENOMEM;
		goto failed;
	}

	if (IS_ENABLED(CONFIG_OF)) {
		ret = mmp_dsi_probe_dt(pdev, dsi);
		if (ret)
			return ret;
	} else {
		/* get configs from platform data */
		mi = pdev->dev.platform_data;
		if (mi == NULL) {
			dev_err(&pdev->dev, "%s: no platform data defined\n", __func__);
			ret = -EINVAL;
			goto failed;
		}
		dsi->name = mi->name;
		dsi->plat_path_name = mi->plat_path_name;
		dsi->setting.lanes = mi->lanes;
		dsi->setting.burst_mode = mi->burst_mode;
		dsi->setting.hbp_en = mi->hbp_en;
		dsi->setting.hfp_en = mi->hfp_en;
		dsi->setting.master_mode = 1;
		dsi->setting.non_continuous_clk = 0;
	}
	dsi->dev = &pdev->dev;
	dsi->set_status = dsi_set_status;
	dsi->set_mode = dsi_set_mode;
	dsi->get_sync_val = dsi_get_sync_val;
	dsi->set_irq = dsi_irq_set;
	dsi->status = MMP_OFF;
	mutex_init(&dsi->lock);
	platform_set_drvdata(pdev, dsi);
	mmp_dsi_create_dsi_port(dsi);

	dsi->vsync.dsi = dsi;
	dsi->vsync.type = DSI_VSYNC;
	mmp_vsync_init(&dsi->vsync);
	dsi->special_vsync.dsi = dsi;
	dsi->special_vsync.type = DSI_SPECIAL_VSYNC;
	mmp_vsync_init(&dsi->special_vsync);

	/* map registers.*/
	if (!devm_request_mem_region(dsi->dev, res->start,
			resource_size(res), dsi->name)) {
		dev_err(dsi->dev,
			"can't request region for resource %pR\n", res);
		ret = -EINVAL;
		goto failed;
	}

	dsi->reg_base = devm_ioremap_nocache(dsi->dev,
			res->start, resource_size(res));
	if (dsi->reg_base == NULL) {
		dev_err(dsi->dev, "%s: res %lx - %lx map failed\n", __func__,
			(unsigned long)res->start, (unsigned long)res->end);
		ret = -ENOMEM;
		goto failed;
	}

	/* request irq */
	ret = devm_request_irq(dsi->dev, irq, dsi_handle_irq,
		IRQF_SHARED, "dsi_controller", dsi);
	if (ret < 0) {
		dev_err(dsi->dev, "%s unable to request IRQ %d\n",
				__func__, irq);
		ret = -ENXIO;
		goto  failed;
	}
	init_completion(&dsi->tx_done);

	mmp_register_dsi(dsi);

	/* get clock */
	dsi->clk = devm_clk_get(dsi->dev, dsi->name);
	if (IS_ERR(dsi->clk)) {
		dev_err(dsi->dev, "unable to get clk %s\n", dsi->name);
		ret = -ENOENT;
		goto failed;
	}

	path = mmp_get_path(dsi->plat_path_name);
	ctrl = path_to_ctrl(path);
	dsi->version = ctrl->version;

	ret = dsi_dbg_init(&pdev->dev);
	if (ret < 0) {
		dev_err(dsi->dev, "%s: Failed to register dsi dbg interface\n", __func__);
		goto failed;
	}

#ifdef CONFIG_MMP_ADAPTIVE_FPS
	ret = adaptive_fps_init(&pdev->dev);
	if (ret < 0) {
		dev_err(dsi->dev, "%s: Failed to register dsi dbg interface\n", __func__);
		goto failed;
	}

	{
		extern void mmp_register_adaptive_fps_handler(struct device *,
				struct mmp_vsync *);
		mmp_register_adaptive_fps_handler(dsi->dev, &dsi->special_vsync);
	}

	dsi->framecnt =
		(struct mmp_framecnt *)kmalloc(sizeof(struct mmp_framecnt), GFP_KERNEL);
	dsi->framecnt->dsi= dsi;

	dsi->framecnt->wq =
		alloc_workqueue("fps-cnt", WQ_HIGHPRI |
				WQ_UNBOUND | WQ_MEM_RECLAIM, 1);
	if (!dsi->framecnt->wq) {
		pr_info("%s, Adaptive fps alloc_workqueue failed \n", __func__);
		return 0;
	}
	INIT_WORK(&dsi->framecnt->work, count_fps_work);

	atomic_set(&dsi->framecnt->frame_cnt, 0);
	dsi->framecnt->is_doing_wq = 0;
#endif
#ifdef CONFIG_MMP_DISP_DFC
	mmp_register_dfc_handler(dsi->dev, &dsi->special_vsync);
#endif


	return 0;
failed:
	if (dsi)
		mmp_unregister_dsi(dsi);

	platform_set_drvdata(pdev, NULL);
	dev_err(&pdev->dev, "dsi device init failed\n");

	return ret;
}

#ifdef CONFIG_MMP_ADAPTIVE_FPS
static void count_fps_work(struct work_struct *pwork)
{
	struct mmp_framecnt *pframecnt =
		container_of(pwork, struct mmp_framecnt, work);
	struct mmp_dsi *dsi= pframecnt->dsi;
	struct mmp_mode *mode = &dsi->mode;
	int ori_fps = dsi->framecnt->default_fps;
	int cur_fps = 0;

	pframecnt->is_doing_wq = 1;

	if (mode->real_refresh != ori_fps)
		update_fps(dsi, ori_fps);

	while (1) {
		cur_fps = atomic_read(&dsi->framecnt->frame_cnt);
		msleep(dsi->framecnt->wait_time);

		if (cur_fps == (int)atomic_read(&dsi->framecnt->frame_cnt)) {
			update_fps(dsi, dsi->framecnt->switch_fps);
			atomic_set(&dsi->framecnt->frame_cnt, 0);
			pframecnt->is_doing_wq = 0;
			return;
		}

		atomic_set(&dsi->framecnt->frame_cnt, 0);
	}
}
#endif

static int mmp_dsi_remove(struct platform_device *pdev)
{
	struct mmp_dsi *dsi = platform_get_drvdata(pdev);

#ifdef CONFIG_MMP_ADAPTIVE_FPS
	adaptive_fps_uninit(dsi->dev);
	destroy_workqueue(dsi->framecnt->wq);
#endif
	dsi_dbg_uninit(dsi->dev);
	mmp_unregister_dsi(dsi);
	platform_set_drvdata(pdev, NULL);

	return 0;
}

#ifdef CONFIG_OF
static const struct of_device_id mmp_dsi_dt_match[] = {
	{ .compatible = "marvell,mmp-dsi" },
	{},
};
#endif

static struct platform_driver mmp_dsi_driver = {
	.probe          = mmp_dsi_probe,
	.remove         = mmp_dsi_remove,
	.driver         = {
		.name   = "mmp_dsi",
		.owner  = THIS_MODULE,
		.of_match_table = of_match_ptr(mmp_dsi_dt_match),
	},
};

int phy_dsi_register(void)
{
	return platform_driver_register(&mmp_dsi_driver);
}

void phy_dsi_unregister(void)
{
	return platform_driver_unregister(&mmp_dsi_driver);
}

MODULE_AUTHOR("Guoqing Li<ligq@marvell.com>");
MODULE_DESCRIPTION("DSI port driver");
MODULE_LICENSE("GPL");
