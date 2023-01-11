/*
 * drivers/video/mmp/hw/mmp_dsi.h
 *
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

#ifndef _MMP_DSI_H_
#define _MMP_DSI_H_

#include <video/mmp_disp.h>

/* DSI Controller Registers */
struct dsi_lcd_regs {
#define DSI_LCD1_CTRL_0  0x100   /* DSI Active Panel 1 Control register 0 */
#define DSI_LCD1_CTRL_1  0x104   /* DSI Active Panel 1 Control register 1 */
	u32 ctrl0;
	u32 ctrl1;
	u32 reserved1[2];

#define DSI_LCD1_TIMING_0		0x110   /* Timing register 0 */
#define DSI_LCD1_TIMING_1		0x114   /* Timing register 1 */
#define DSI_LCD1_TIMING_2		0x118   /* Timing register 2 */
#define DSI_LCD1_TIMING_3		0x11C   /* Timing register 3 */
#define DSI_LCD1_WC_0			0x120   /* Word Count register 0 */
#define DSI_LCD1_WC_1			0x124   /* Word Count register 1 */
#define DSI_LCD1_WC_2			0x128   /* Word Count register 2 */
	u32 timing0;
	u32 timing1;
	u32 timing2;
	u32 timing3;
	u32 wc0;
	u32 wc1;
	u32 wc2;
	u32 reserved2[1];
	u32 slot_cnt0;
	u32 slot_cnt1;
	u32 reserved3[2];
	u32 status_0;
	u32 status_1;
	u32 status_2;
	u32 status_3;
	u32 status_4;
};

/* updated for DC4 */
struct dsi_phy {
	u32 phy_ctrl0;
	u32 phy_ctrl1;
	u32 phy_ctrl2;
	u32 phy_ctrl3;
	u32 phy_status0;
	u32 phy_status1;
	u32 phy_lprx0;
	u32 phy_lprx1;
	u32 phy_lptx0;
	u32 phy_lptx1;
	u32 phy_lptx2;
	u32 phy_status2;
	u32 phy_rcomp0;
	u32 phy_rcomp1;
	u32 reserved[2];
	u32 phy_timing0;
	u32 phy_timing1;
	u32 phy_timing2;
	u32 phy_timing3;
	u32 phy_code_0;
	u32 phy_code_1;
};

struct mmp_dsi_regs {
#define DSI_CTRL_0	  0x000   /* DSI control register 0 */
#define DSI_CTRL_1	  0x004   /* DSI control register 1 */
	u32 ctrl0;
	u32 ctrl1;
	u32 reserved1[2];
	u32 irq_status;
	u32 irq_mask;
	u32 reserved2[2];

#define DSI_CPU_CMD_0   0x020   /* DSI CPU packet command register 0 */
#define DSI_CPU_CMD_1   0x024   /* DSU CPU Packet Command Register 1 */
#define DSI_CPU_CMD_3   0x02C   /* DSU CPU Packet Command Register 3 */
#define DSI_CPU_WDAT_0  0x030   /* DSI CUP */
	u32 cmd0;
	u32 cmd1;
	u32 cmd2;
	u32 cmd3;
	u32 dat0;
	u32 status0;
	u32 status1;
	u32 status2;
	u32 status3;
	u32 status4;
	u32 reserved3[1];

	u32 smt_status1;
	u32 smt_cmd;
	u32 smt_ctrl0;
	u32 smt_ctrl1;
	u32 smt_status0;

	u32 rx0_status;
/* Rx Packet Header - data from slave device */
#define DSI_RX_PKT_HDR_0 0x064
	u32 rx0_header;
	u32 rx1_status;
	u32 rx1_header;
	u32 rx_ctrl;
	u32 rx_ctrl1;
	u32 rx2_status;
	u32 rx2_header;
	u32 reserved4[1];

	u32 phy_ctrl1;
#define DSI_PHY_CTRL_2		0x088   /* DSI DPHI Control Register 2 */
#define DSI_PHY_CTRL_3		0x08C   /* DPHY Control Register 3 */
	u32 phy_ctrl2;
	u32 phy_ctrl3;
	u32 phy_status0;
	u32 phy_status1;
	u32 reserved5[5];
	u32 phy_status2;

#define DSI_PHY_RCOMP_0		0x0B0   /* DPHY Rcomp Control Register */
	u32 phy_rcomp0;
	u32 phy_rcomp1;
	u32 reserved6[2];
#define DSI_PHY_TIME_0		0x0C0   /* DPHY Timing Control Register 0 */
#define DSI_PHY_TIME_1		0x0C4   /* DPHY Timing Control Register 1 */
#define DSI_PHY_TIME_2		0x0C8   /* DPHY Timing Control Register 2 */
#define DSI_PHY_TIME_3		0x0CC   /* DPHY Timing Control Register 3 */
#define DSI_PHY_TIME_4		0x0D0   /* DPHY Timing Control Register 4 */
#define DSI_PHY_TIME_5		0x0D4   /* DPHY Timing Control Register 5 */
	u32 phy_timing0;
	u32 phy_timing1;
	u32 phy_timing2;
	u32 phy_timing3;
	u32 phy_code_0;
	u32 phy_code_1;
	u32 reserved7[2];
	u32 mem_ctrl;
	u32 tx_timer;
	u32 rx_timer;
	u32 turn_timer;
	u32 top_status0;
	u32 top_status1;
	u32 top_status2;
	u32 top_status3;

	struct dsi_lcd_regs lcd1;
	u32 reserved8[11];
	union {
		struct dsi_lcd_regs lcd2;
		struct dsi_phy phy;
	};
};

#define DSI_LCD2_CTRL_0  0x180   /* DSI Active Panel 2 Control register 0 */
#define DSI_LCD2_CTRL_1  0x184   /* DSI Active Panel 2 Control register 1 */
#define DSI_LCD2_TIMING_0		0x190   /* Timing register 0 */
#define DSI_LCD2_TIMING_1		0x194   /* Timing register 1 */
#define DSI_LCD2_TIMING_2		0x198   /* Timing register 2 */
#define DSI_LCD2_TIMING_3		0x19C   /* Timing register 3 */
#define DSI_LCD2_WC_0			0x1A0   /* Word Count register 0 */
#define DSI_LCD2_WC_1			0x1A4   /* Word Count register 1 */
#define DSI_LCD2_WC_2			0x1A8   /* Word Count register 2 */

/* DSI_CTRL_0 0x0000 DSI Control Register 0 */
#define DSI_CTRL_0_CFG_SOFT_RST			(1<<31)
#define DSI_CTRL_0_CFG_SOFT_RST_REG		(1<<30)
#define CFG_CLR_PHY_FIFO			(1<<29)
#define CFG_RST_TXLP				(1<<28)
#define CFG_RST_CPU				(1<<27)
#define CFG_RST_CPN				(1<<26)
#define CFG_RST_VPN2				(1<<25)
#define CFG_RST_VPN1				(1<<24)
#define CFG_DSI_PHY_RST				(1<<23)
#define DSI_CTRL_0_CFG_LCD1_TX_EN		(1<<8)
#define DSI_CTRL_0_CFG_LCD1_SLV			(1<<4)
#define DSI_CTRL_0_CFG_LCD1_EN			(1<<0)

/* DSI_CTRL_1 0x0004 DSI Control Register 1 */
#define DSI_CTRL_1_CFG_EOTP			(1<<8)
#define DSI_CTRL_1_CFG_RSVD			(2<<4)
#define DSI_CTRL_1_CFG_LCD2_VCH_NO_MASK		(3<<2)
#define DSI_CTRL_1_CFG_LCD2_VCH_NO_SHIFT	2
#define DSI_CTRL_1_CFG_LCD1_VCH_NO_MASK		(3<<0)
#define DSI_CTRL_1_CFG_LCD1_VCH_NO_SHIFT	0

/* DSI_IRQ_ST 0x0010 DSI Interrupt Status Register */
#define IRQ_VPN_VSYNC				(1<<31)
#define IRQ_TA_TIMEOUT				(1<<29)
#define IRQ_RX_TIMEOUT				(1<<28)
#define IRQ_TX_TIMEOUT				(1<<27)
#define IRQ_RX_STATE_ERR			(1<<26)
#define IRQ_RX_ERR				(1<<25)
#define IRQ_RX_FIFO_FULL_ERR			(1<<24)
#define IRQ_PHY_FIFO_UNDERRUN			(1<<23)
#define IRQ_REQ_CNT_ERR				(1<<22)
#define IRQ_VACT_DONE					(1<<15)
#define IRQ_DPHY_ERR_SYNC_ESC			(1<<10)
#define IRQ_DPHY_ERR_ESC			(1<<9)
#define IRQ_DPHY_RX_LINE_ERR			(1<<8)
#define IRQ_RX_TRG3				(1<<7)
#define IRQ_RX_TRG2				(1<<6)
#define IRQ_RX_TRG1				(1<<5)
#define IRQ_RX_TRG0				(1<<4)
#define IRQ_RX_PKT				(1<<2)
#define IRQ_CPU_TX_DONE				(1<<0)

union dsi_lcd_ctrl1 {
	struct {
		u32 input_fmt:2;     /* 1:0 */
		u32 burst_mode:2;    /* 3:2 */
		u32 reserved0:4;     /* 7:4 */
		u32 lpm_line_en:1;   /* 8 */
		u32 lpm_frame_en:1;  /* 9 */
		u32 last_line_turn:1;/* 10 */
		u32 reserved1:3;     /* 13:11 */
		u32 hex_slot_en:1;   /* 14 */
		u32 all_slot_en:1;   /* 15 */
		u32 hsa_pkt_en:1;    /* 16 */
		u32 hse_pkt_en:1;    /* 17 */
		u32 hbp_pkt_en:1;    /* 18 */
		u32 hact_pkt_en:1;   /* 19 */
		u32 hfp_pkt_en:1;    /* 20 */
		u32 hex_pkt_en:1;    /* 21 */
		u32 hlp_pkt_en:1;    /* 22 */
		u32 reserved2:1;     /* 23 */
		u32 auto_dly_dis:1;  /* 24 */
		u32 timing_check_dis:1;/* 25 */
		u32 hact_wc_en:1;    /* 26 */
		u32 auto_wc_dis:1;   /* 27 */
		u32 reserved3:2;     /* 29:28 */
		u32 m2k_en:1;        /* 30 */
		u32 vsync_rst_en:1;  /* 31 */
	} bit;
	u32 val;
};

#define DPHY_CFG_VDD_VALID	(0x3 << 16)
#define DPHY_CFG_FORCE_ULPS	(0x3 << 1)
#define DPHY_BG_VREF_EN		(0x1 << 9)
#define DPHY_BG_VREF_EN_DC4	(0x1 << 18)
/* DSI_PHY_CTRL_2 0x0088	DPHY Control Register 2 */
/* Bit(s) DSI_PHY_CTRL_2_RSRV_31_12 reserved */
/* DPHY LP Receiver Enable */
#define	DSI_PHY_CTRL_2_CFG_CSR_LANE_RESC_EN_MASK	(0xf<<8)
#define	DSI_PHY_CTRL_2_CFG_CSR_LANE_RESC_EN_SHIFT	8
/* DPHY Data Lane Enable */
#define	DSI_PHY_CTRL_2_CFG_CSR_LANE_EN_MASK		(0xf<<4)
#define	DSI_PHY_CTRL_2_CFG_CSR_LANE_EN_SHIFT		4
/* DPHY Bus Turn Around */
#define	DSI_PHY_CTRL_2_CFG_CSR_LANE_TURN_MASK		(0xf)
#define	DSI_PHY_CTRL_2_CFG_CSR_LANE_TURN_SHIFT		0

/* DSI_CPU_CMD_0 0x0020	DSI CPU Packet Command Register 0 */
#define DSI_CFG_CPU_CMD_REQ_MASK			(0x1 << 31)
#define DSI_CFG_CPU_CMD_REQ_SHIFT			31
#define DSI_CFG_CPU_SP_MASK				(0x1 << 30)
#define DSI_CFG_CPU_SP_SHIFT				30
#define DSI_CFG_CPU_TURN_MASK				(0x1 << 29)
#define DSI_CFG_CPU_TURN_SHIFT				29
#define DSI_CFG_CPU_TXLP_MASK				(0x1 << 27)
#define DSI_CFG_CPU_TXLP_SHIFT				27
#define DSI_CFG_CPU_WC_MASK				(0xff)
#define DSI_CFG_CPU_WC_SHIFT				0

/* DSI_CPU_CMD_1 0x0024	DSI CPU Packet Command Register 1 */
/* Bit(s) DSI_CPU_CMD_1_RSRV_31_24 reserved */
/* LPDT TX Enable */
#define	DSI_CPU_CMD_1_CFG_TXLP_LPDT_MASK		(0xf<<20)
#define	DSI_CPU_CMD_1_CFG_TXLP_LPDT_SHIFT		20
/* ULPS TX Enable */
#define	DSI_CPU_CMD_1_CFG_TXLP_ULPS_MASK		(0xf<<16)
#define	DSI_CPU_CMD_1_CFG_TXLP_ULPS_SHIFT		16
/* Low Power TX Trigger Code */
#define	DSI_CPU_CMD_1_CFG_TXLP_TRIGGER_CODE_MASK	(0xffff)
#define	DSI_CPU_CMD_1_CFG_TXLP_TRIGGER_CODE_SHIFT	0

/* DSI_CPU_CMD_3 0x002C	DSI CPU Packet Command Register 3 */
#define DSI_CFG_CPU_DAT_REQ_MASK			(0x1 << 31)
#define DSI_CFG_CPU_DAT_REQ_SHIFT			31
#define DSI_CFG_CPU_DAT_RW_MASK				(0x1 << 30)
#define DSI_CFG_CPU_DAT_RW_SHIFT			30
#define DSI_CFG_CPU_DAT_ADDR_MASK			(0xff << 16)
#define DSI_CFG_CPU_DAT_ADDR_SHIFT			16

/* DSI_RX_PKT_ST_0 0x0060 DSI RX Packet 0 Status Register */
#define RX_PKT0_ST_VLD					(0x1 << 31)
#define RX_PKT0_ST_SP					(0x1 << 24)
#define RX_PKT0_PKT_PTR_MASK				(0x3f << 16)
#define RX_PKT0_PKT_PTR_SHIFT				16

/* DSI_RX_PKT_CTRL 0x0070 DSI RX Packet Read Control Register */
#define RX_PKT_RD_REQ					(0x1 << 31)
#define RX_PKT_RD_PTR_MASK				(0x3f << 16)
#define RX_PKT_RD_PTR_SHIFT				16
#define RX_PKT_RD_DATA_MASK				(0xff)
#define RX_PKT_RD_DATA_SHIFT				0

/* DSI_RX_PKT_CTRL_1 0x0074 DSI RX Packet Read Control1 Register */
#define RX_PKT_CNT_MASK					(0xf << 8)
#define RX_PKT_CNT_SHIFT				8
#define RX_PKT_BCNT_MASK				(0xff)
#define RX_PKT_BCNT_SHIFT				0

/* DSI_PHY_TIME_0 0x00c0 DPHY Timing Control Register 0 */
/* Length of HS Exit Period in tx_clk_esc Cycles */
#define	DSI_PHY_TIME_0_CFG_CSR_TIME_HS_EXIT_MASK	(0xff<<24)
#define	DSI_PHY_TIME_0_CFG_CSR_TIME_HS_EXIT_SHIFT	24
/* DPHY HS Trail Period Length */
#define	DSI_PHY_TIME_0_CFG_CSR_TIME_HS_TRAIL_MASK	(0xff<<16)
#define	DSI_PHY_TIME_0_CFG_CSR_TIME_HS_TRAIL_SHIFT	16
/* DPHY HS Zero State Length */
#define	DSI_PHY_TIME_0_CDG_CSR_TIME_HS_ZERO_MASK	(0xff<<8)
#define	DSI_PHY_TIME_0_CDG_CSR_TIME_HS_ZERO_SHIFT	8
/* DPHY HS Prepare State Length */
#define	DSI_PHY_TIME_0_CFG_CSR_TIME_HS_PREP_MASK	(0xff)
#define	DSI_PHY_TIME_0_CFG_CSR_TIME_HS_PREP_SHIFT	0

/* DSI_PHY_TIME_1 0x00c4 DPHY Timing Control Register 1 */
/* Time to Drive LP-00 by New Transmitter */
#define	DSI_PHY_TIME_1_CFG_CSR_TIME_TA_GET_MASK		(0xff<<24)
#define	DSI_PHY_TIME_1_CFG_CSR_TIME_TA_GET_SHIFT	24
/* Time to Drive LP-00 after Turn Request */
#define	DSI_PHY_TIME_1_CFG_CSR_TIME_TA_GO_MASK		(0xff<<16)
#define	DSI_PHY_TIME_1_CFG_CSR_TIME_TA_GO_SHIFT		16
/* DPHY HS Wakeup Period Length */
#define	DSI_PHY_TIME_1_CFG_CSR_TIME_WAKEUP_MASK		(0xffff)
#define	DSI_PHY_TIME_1_CFG_CSR_TIME_WAKEUP_SHIFT	0

/* DSI_PHY_TIME_2 0x00c8 DPHY Timing Control Register 2 */
/* DPHY CLK Exit Period Length */
#define	DSI_PHY_TIME_2_CFG_CSR_TIME_CK_EXIT_MASK	(0xff<<24)
#define	DSI_PHY_TIME_2_CFG_CSR_TIME_CK_EXIT_SHIFT	24
/* DPHY CLK Trail Period Length */
#define	DSI_PHY_TIME_2_CFG_CSR_TIME_CK_TRAIL_MASK	(0xff<<16)
#define	DSI_PHY_TIME_2_CFG_CSR_TIME_CK_TRAIL_SHIFT	16
/* DPHY CLK Zero State Length */
#define	DSI_PHY_TIME_2_CFG_CSR_TIME_CK_ZERO_MASK	(0xff<<8)
#define	DSI_PHY_TIME_2_CFG_CSR_TIME_CK_ZERO_SHIFT	8
/* DPHY CLK LP Length */
#define	DSI_PHY_TIME_2_CFG_CSR_TIME_CK_LPX_MASK		(0xff)
#define	DSI_PHY_TIME_2_CFG_CSR_TIME_CK_LPX_SHIFT	0

/* DSI_PHY_TIME_3 0x00cc DPHY Timing Control Register 3 */
/* Bit(s) DSI_PHY_TIME_3_RSRV_31_16 reserved */
/* DPHY LP Length */
#define	DSI_PHY_TIME_3_CFG_CSR_TIME_LPX_MASK		(0xff<<8)
#define	DSI_PHY_TIME_3_CFG_CSR_TIME_LPX_SHIFT		8
/* DPHY HS req to rdy Length */
#define	DSI_PHY_TIME_3_CFG_CSR_TIME_REQRDY_MASK		(0xff)
#define	DSI_PHY_TIME_3_CFG_CSR_TIME_REQRDY_SHIFT	0

#define DSI_ESC_CLK				52  /* Unit: Mhz */
#define DSI_ESC_CLK_T				19  /* Unit: ns */

#define DSI_EX_PIXEL_CNT	0
#define DSI_HEX_EN		0
#define to_dsi_bcnt(timing, bpp)	(((timing) * (bpp)) >> 3)

#define DSI_LANES(n)	((1 << (n)) - 1)

/* Unit: ns. */
struct dsi_phy_timing {
	u32 hs_prep_constant;
	u32 hs_prep_ui;
	u32 hs_zero_constant;
	u32 hs_zero_ui;
	u32 hs_trail_constant;
	u32 hs_trail_ui;
	u32 hs_exit_constant;
	u32 hs_exit_ui;
	u32 ck_zero_constant;
	u32 ck_zero_ui;
	u32 ck_trail_constant;
	u32 ck_trail_ui;
	u32 req_ready;
	u32 wakeup_constant;
	u32 wakeup_ui;
	u32 lpx_constant;
	u32 lpx_ui;
};

extern int dsi_dbg_init(struct device *dev);
extern void dsi_dbg_uninit(struct device *dev);
#endif	/* _MMP_DSI_H_ */
