/*
 * PXA988 DDR Settings
 *
 * This software program is licensed subject to the GNU General Public License
 * (GPL).Version 2,June 1991, available at http://www.fsf.org/copyleft/gpl.html
 *
 * (C) Copyright 2011 Marvell International Ltd.
 * All Rights Reserved
 */

#ifndef __PXA988_DDR_H__
#define __PXA988_DDR_H__

struct ddr_setting_entry {
	unsigned int reg;
	unsigned int val;
};

struct ddr_timing {
	/* PXA988 MCK4 controller has 8 timing registers*/
	struct ddr_setting_entry entry[8];
};

struct ddr_phy_ds {
	/*
	 * We have to set 3 phy registers for drive strength,
	 * PHY_CTRL7, PHY_CTRL8, PHY_CTRL9.
	 * If the value is always same for different frequencies,
	 * we could remove this part in the future.
	 */
	struct ddr_setting_entry entry[3];
};

struct ddr_phy_misc {
	/*
	 * Some phy settings which are different for different
	 * frequencies.
	 */
	struct ddr_setting_entry entry[1];
};

struct platform_ddr_setting {
	unsigned int ddr_freq;
	unsigned int cas_latency;
	unsigned int table_idx;
	struct ddr_timing timing;
	struct ddr_phy_ds phy_ds;
	struct ddr_phy_misc phy_misc;
};

#endif
