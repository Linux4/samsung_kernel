/*
 * Copyright (C) 2012 Spreadtrum Communications Inc.
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

#ifndef __SPRD_BUSMONITOR_H__
#define __SPRD_BUSMONITOR_H__

enum sci_bm_index {
	AXI_BM0 = 0x0,
	AXI_BM1,
	AXI_BM2,
	AXI_BM3,
	AXI_BM4,
	AXI_BM5,
	AXI_BM6,
	AXI_BM7,
	AXI_BM8,
	AXI_BM9,
	AHB_BM0,
	AHB_BM1,
	AHB_BM2,
	BM_SIZE,
};

enum sci_bm_chn {
	CHN0 = 0x0,
	CHN1,
	CHN2,
	CHN3,
};

enum sci_bm_mode {
	R_MODE,
	W_MODE,
	RW_MODE,
	PER_MODE,
	RST_BUF_MODE,
};

struct sci_bm_cfg {
	u32 addr_min;
	u32 addr_max;
	/*only be effective for AHB busmonitor*/
	u64 data_min;
	u64 data_max;
	enum sci_bm_mode bm_mode;
};

#define SPRD_AHB_BM_NAME "sprd_ahb_busmonitor"
#define SPRD_AXI_BM_NAME "sprd_axi_busmonitor"

extern void dmc_mon_cnt_reset(void);
extern void dmc_mon_cnt_stop(void);
extern void dmc_mon_cnt_start(void);
extern void dmc_mon_cnt_clr(void);
extern unsigned int dmc_mon_cnt_bw(void);
extern void dmc_mon_resume(void);

#endif

