/*
 * Copyright (C) 2020 Uniso Communications Inc.
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

#ifndef _UFS_SPRD_H_
#define _UFS_SPRD_H_
#include <linux/bits.h>

struct syscon_ufs {
	struct regmap *regmap;
	u32 reg;
	u32 mask;
};

struct ufs_sprd_host {
	struct ufs_hba *hba;
	void __iomem *ufs_analog_reg;
	struct syscon_ufs aon_apb_ufs_en;
	struct syscon_ufs ap_ahb_ufs_clk;
	struct syscon_ufs ap_apb_ufs_rst;
	struct syscon_ufs ap_apb_ufs_en;
	struct syscon_ufs ufs_refclk_on;
	struct syscon_ufs ahb_ufs_lp;
	struct syscon_ufs ahb_ufs_force_isol;
	struct syscon_ufs ahb_ufs_cb;
	struct clk *hclk_source;
	struct clk *pclk_source;
	struct clk *hclk;
	struct clk *pclk;

};

/* UFS analog registers */
#define MPHY_2T2R_APB_REG1 0x68
#define MPHY_2T2R_APB_RESETN (0x1 << 3)

#define FIFO_ENABLE_MASK (0x1 << 15)

/* UFS mphy registers */
#define MPHY_LANE0_FIFO 0xc08c
#define MPHY_LANE1_FIFO 0xc88c
#define MPHY_TACTIVATE_TIME_LANE0 0xc088
#define MPHY_TACTIVATE_TIME_LANE1 0xc888

#define FIFO_ENABLE_MASK (0x1 << 15)
#define MPHY_TACTIVATE_TIME_200US (0x1 << 17)

/* UFS HC register */
#define HCLKDIV_REG 0xFC
#define CLKDIV 0x100

#define	MPHY_DIG_CFG7_LANE0 0xC01c
#define	MPHY_DIG_CFG7_LANE1 0xC81c
#define	MPHY_CDR_MONITOR_BYPASS_MASK GENMASK(24, 24)
#define	MPHY_CDR_MONITOR_BYPASS_ENABLE BIT(24)

#define	MPHY_DIG_CFG20_LANE0 0xC050
#define	MPHY_RXOFFSETCALDONEOVR_MASK GENMASK(5, 4)
#define	MPHY_RXOFFSETCALDONEOVR_ENABLE (BIT(5) | BIT(4))
#define	MPHY_RXOFFOVRVAL_MASK GENMASK(11, 10)
#define	MPHY_RXOFFOVRVAL_ENABLE (BIT(11) | BIT(10))

#define	MPHY_DIG_CFG49_LANE0 0xC0C4
#define	MPHY_DIG_CFG49_LANE1 0xC8C4
#define	MPHY_RXCFGG1_MASK GENMASK(23, 0)
#define	MPHY_RXCFGG1_VAL (0x0C0C0C << 0)

#define	MPHY_DIG_CFG51_LANE0 0xC0CC
#define	MPHY_DIG_CFG51_LANE1 0xC8CC
#define	MPHY_RXCFGG3_MASK GENMASK(23, 0)
#define	MPHY_RXCFGG3_VAL (0x0D0D0D << 0)

#define	MPHY_DIG_CFG72_LANE0 0xC120
#define	MPHY_DIG_CFG72_LANE1 0xC920
#define	MPHY_RXHSG3SYNCCAP_MASK GENMASK(15, 8)
#define	MPHY_RXHSG3SYNCCAP_VAL (0x4B << 8)

#define	MPHY_DIG_CFG60_LANE0 0xC0F0
#define	MPHY_DIG_CFG60_LANE1 0xC8F0
#define	MPHY_RX_STEP4_CYCLE_G3_MASK GENMASK(31, 16)
#define	MPHY_RX_STEP4_CYCLE_G3_VAL  BIT(23)

#endif/* _UFS_SPRD_H_ */
