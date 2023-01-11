/*
 * Copyright (C) 2013 Marvell Inc.
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#ifndef __MV_USB2_H
#define __MV_USB2_H

#define MV_USB2_PHY_INDEX	0
#define MV_USB2_OTG_PHY_INDEX	1

#define PHY_28LP	0x2800
#define PHY_40LP	0x4000
#define PHY_55LP	0x5500

#define MV_PHY_FLAG_PLL_LOCK_BYPASS	(1 << 0)

/* phy_flag is used to record the feature supported */
struct mv_usb2_phydata {
	unsigned long phy_type;
	u32 phy_rev;
	u32 phy_flag;
};

struct mv_usb2_phy {
	struct usb_phy		phy;
	struct platform_device	*pdev;
	void __iomem		*base;
	struct clk		*clk;
	struct mv_usb2_phydata  drv_data;
};

/*
 * PHY revision: For those has small difference with default setting.
 * bit [15..8]: represent PHY IP as below:
 *     PHY_55LP        0x5500,
 *     PHY_40LP        0x4000,
 *     PHY_28LP        0x2800,
 */
#define REV_PXA168     0x5500
#define REV_PXA910     0x5501

#ifdef CONFIG_USB_GADGET_CHARGE_ONLY
extern int is_charge_only_mode(void);
extern void charge_only_send_uevent(int event);
extern void usb_phy_force_dp_dm(struct usb_phy *phy, bool is_force);
#endif /* CONFIG_USB_GADGET_CHARGE_ONLY */

#endif
