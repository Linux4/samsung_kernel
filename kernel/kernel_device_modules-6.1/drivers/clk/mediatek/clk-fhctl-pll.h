/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2020 MediaTek Inc.
 * Author: Yu-Chang Wang <Yu-Chang.Wang@mediatek.com>
 */
#ifndef __CLK_FHCTL_PLL_H
#define __CLK_FHCTL_PLL_H
#include <linux/types.h>
#include <linux/bitops.h>
#include <linux/iopoll.h>

struct fh_pll_data {
	char *name;
	unsigned int dds_mask;
	unsigned int postdiv_mask;
	unsigned int postdiv_offset;
	unsigned int slope0_value;
	unsigned int slope1_value;
	unsigned int sfstrx_en;
	unsigned int frddsx_en;
	unsigned int fhctlx_en;
	unsigned int tgl_org;
	unsigned int dvfs_tri;
	unsigned int pcwchg;
	unsigned int dt_val;
	unsigned int df_val;
	unsigned int updnlmt_shft;
	unsigned int msk_frddsx_dys;
	unsigned int msk_frddsx_dts;
};
struct fh_pll_offset {
	int offset_fhctl;
	int offset_con_pcw;
	int offset_con_postdiv;
	int offset_hp_en;
	int offset_hp_en_set;
	int offset_hp_en_clr;
	int offset_clk_con;
	int offset_clk_con_set;
	int offset_clk_con_clr;
	int offset_rst_con;
	int offset_rst_con_set;
	int offset_rst_con_clr;
	int offset_slope0;
	int offset_slope1;
	int offset_cfg;
	int offset_updnlmt;
	int offset_dds;
	int offset_dvfs;
	int offset_mon;
};
struct fh_pll_regs {
	/* Do keep EN -> EN_SET -> EN_CLR structure order */
	void __iomem *reg_hp_en;
	void __iomem *reg_hp_en_set;
	void __iomem *reg_hp_en_clr;
	void __iomem *reg_clk_con;
	void __iomem *reg_clk_con_set;
	void __iomem *reg_clk_con_clr;
	void __iomem *reg_rst_con;
	void __iomem *reg_rst_con_set;
	void __iomem *reg_rst_con_clr;
	void __iomem *reg_slope0;
	void __iomem *reg_slope1;
	void __iomem *reg_cfg;
	void __iomem *reg_updnlmt;
	void __iomem *reg_dds;
	void __iomem *reg_dvfs;
	void __iomem *reg_mon;
	void __iomem *reg_con_pcw;
	void __iomem *reg_con_postdiv;
};
struct fh_pll_domain {
	char *name;
	struct fh_pll_data *data;
	struct fh_pll_offset *offset;
	struct fh_pll_regs *regs;
	int (*init)(struct fh_pll_domain *d,
		void __iomem *fhctl_base,
		void __iomem *apmixed_base);
};
extern struct fh_pll_domain *get_fh_domain(const char *name);
extern void init_fh_domain(const char *domain_name,
		char *comp_name,
		void __iomem *fhctl_base,
		void __iomem *apmixed_base);
#endif
