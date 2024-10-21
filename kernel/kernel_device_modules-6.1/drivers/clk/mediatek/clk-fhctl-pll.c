// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2020 MediaTek Inc.
 * Author: Yu-Chang Wang <Yu-Chang.Wang@mediatek.com>
 */
#include <linux/device.h>
#include <linux/module.h>
#include "clk-fhctl-pll.h"
#include "clk-fhctl-util.h"

#define REG_ADDR(base, x) ((void __iomem *)((unsigned long)base + (x)))

struct match {
	char *compatible;
	struct fh_pll_domain **domain_list;
};

static int init_v1(struct fh_pll_domain *d,
		void __iomem *fhctl_base,
		void __iomem *apmixed_base)
{
	struct fh_pll_data *data;
	struct fh_pll_offset *offset;
	struct fh_pll_regs *regs;
	char *name;

	name = d->name;
	data = d->data;
	offset = d->offset;
	regs = d->regs;

	if (regs->reg_hp_en) {
		FHDBG("domain<%s> inited\n", name);
		return 0;
	}

	FHDBG("init domain<%s>\n", name);
	while (data->dds_mask != 0) {
		int regs_offset;

		/* fhctl common part */
		regs->reg_hp_en = REG_ADDR(fhctl_base,
				offset->offset_hp_en);
		regs->reg_clk_con = REG_ADDR(fhctl_base,
				offset->offset_clk_con);
		regs->reg_rst_con = REG_ADDR(fhctl_base,
				offset->offset_rst_con);
		regs->reg_slope0 = REG_ADDR(fhctl_base,
				offset->offset_slope0);
		regs->reg_slope1 = REG_ADDR(fhctl_base,
				offset->offset_slope1);
		/* If SET/CLR register defined */
		if (offset->offset_hp_en_set != 0) {
			regs->reg_hp_en_set = REG_ADDR(fhctl_base,
					offset->offset_hp_en_set);
			regs->reg_hp_en_clr = REG_ADDR(fhctl_base,
					offset->offset_hp_en_clr);
			regs->reg_rst_con_set = REG_ADDR(fhctl_base,
					offset->offset_rst_con_set);
			regs->reg_rst_con_clr = REG_ADDR(fhctl_base,
					offset->offset_rst_con_clr);
			regs->reg_clk_con_set = REG_ADDR(fhctl_base,
					offset->offset_clk_con_set);
			regs->reg_clk_con_clr = REG_ADDR(fhctl_base,
					offset->offset_clk_con_clr);
		} else {
			regs->reg_hp_en_set = NULL;
			regs->reg_hp_en_clr = NULL;
			regs->reg_rst_con_set = NULL;
			regs->reg_rst_con_clr = NULL;
			regs->reg_clk_con_set = NULL;
			regs->reg_clk_con_clr = NULL;
		}

		/* fhctl pll part */
		regs_offset = offset->offset_fhctl + offset->offset_cfg;
		regs->reg_cfg = REG_ADDR(fhctl_base, regs_offset);
		regs->reg_updnlmt = REG_ADDR(regs->reg_cfg,
				offset->offset_updnlmt);
		regs->reg_dds = REG_ADDR(regs->reg_cfg,
				offset->offset_dds);
		regs->reg_dvfs = REG_ADDR(regs->reg_cfg,
				offset->offset_dvfs);
		regs->reg_mon = REG_ADDR(regs->reg_cfg,
				offset->offset_mon);

		/* apmixed part */
		regs->reg_con_pcw = REG_ADDR(apmixed_base,
				offset->offset_con_pcw);
		regs->reg_con_postdiv = REG_ADDR(apmixed_base,
				offset->offset_con_postdiv);

		FHDBG("pll<%s>, dds_mask<%d>, data<%lx> offset<%lx> regs<%lx> %s\n",
				data->name, data->dds_mask, (unsigned long)data, (unsigned long)offset,
				(unsigned long)regs, regs->reg_hp_en_set?"Support SET/CLR":"");
		data++;
		offset++;
		regs++;
	}

	return 0;
}

/* 6878 begin */
#define SIZE_6878_TOP0 (sizeof(mt6878_top0_data)\
	/sizeof(struct fh_pll_data))
#define SIZE_6878_TOP1 (sizeof(mt6878_top1_data)\
	/sizeof(struct fh_pll_data))
#define SIZE_6878_GPU0 (sizeof(mt6878_gpu0_data)\
	/sizeof(struct fh_pll_data))
#define SIZE_6878_GPU1 (sizeof(mt6878_gpu1_data)\
	/sizeof(struct fh_pll_data))
#define SIZE_6878_GPU2 (sizeof(mt6878_gpu2_data)\
	/sizeof(struct fh_pll_data))

#define DATA_6878_CONVERT(_name) {				\
		.name = _name,						\
		.dds_mask = GENMASK(21, 0),			\
		.postdiv_mask = GENMASK(26, 24),		    \
		.postdiv_offset = 24,				        \
		.slope0_value = 0x6003c97,			\
		.slope1_value = 0x6003c97,			\
		.sfstrx_en = BIT(2),				\
		.frddsx_en = BIT(1),				\
		.fhctlx_en = BIT(0),				\
		.tgl_org = BIT(31),					\
		.dvfs_tri = BIT(31),				\
		.pcwchg = BIT(31),					\
		.dt_val = 0x0,						\
		.df_val = 0x9,						\
		.updnlmt_shft = 16,					\
		.msk_frddsx_dys = GENMASK(23, 20),	\
		.msk_frddsx_dts = GENMASK(19, 16),	\
	}
#define REG_6878_CONVERT(_fhctl, _con_pcw) {	\
		.offset_fhctl = _fhctl,				\
		.offset_con_pcw = _con_pcw,			\
		.offset_con_postdiv = _con_pcw,		\
		.offset_hp_en = 0x0,                \
		.offset_hp_en_set = 0x100,          \
		.offset_hp_en_clr = 0x104,			\
		.offset_clk_con = 0x8,				\
		.offset_clk_con_set = 0x108,        \
		.offset_clk_con_clr = 0x10C,         \
		.offset_rst_con = 0xc,				\
		.offset_rst_con_set = 0x110,        \
		.offset_rst_con_clr = 0x114,        \
		.offset_slope0 = 0x10,				\
		.offset_slope1 = 0x14,				\
		.offset_cfg = 0x0,					\
		.offset_updnlmt = 0x4,				\
		.offset_dds = 0x8,					\
		.offset_dvfs = 0xc,					\
		.offset_mon = 0x10,					\
	}
//TINYSYS no slope1, map to slope0 for compatibility
#define REG_6878_TINYSYS_CONVERT(_fhctl, _con_pcw) {	\
		.offset_fhctl = _fhctl,				\
		.offset_con_pcw = _con_pcw,			\
		.offset_con_postdiv = _con_pcw,		\
		.offset_hp_en = 0x0,				\
		.offset_clk_con = 0x8,				\
		.offset_rst_con = 0xc,				\
		.offset_slope0 = 0x10,				\
		.offset_slope1 = 0x10,				\
		.offset_cfg = 0x0,					\
		.offset_updnlmt = 0x4,				\
		.offset_dds = 0x8,					\
		.offset_dvfs = 0xc,					\
		.offset_mon = 0x10,					\
	}

///////////////////////////////////top0
static struct fh_pll_data mt6878_top0_data[] = {
	DATA_6878_CONVERT("Nouse_ARMPLL_LL"),
	DATA_6878_CONVERT("Nouse_ARMPLL_BL"),
	DATA_6878_CONVERT("Nouse_CCIPLL"),
	DATA_6878_CONVERT("MSDCPLL"),
	DATA_6878_CONVERT("UFSPLL"),
	DATA_6878_CONVERT("MMPLL"),
	DATA_6878_CONVERT("MAINPLL"),
	{}
};
static struct fh_pll_offset mt6878_top0_offset[SIZE_6878_TOP0] = {
	REG_6878_CONVERT(0x003C, 0x0208),  // DATA_6878_CONVERT("Nouse_ARMPLL_LL")
	REG_6878_CONVERT(0x0050, 0x0218),  // DATA_6878_CONVERT("Nouse_ARMPLL_BL")
	REG_6878_CONVERT(0x0064, 0x0228),  // DATA_6878_CONVERT("Nouse_CCIPLL")
	REG_6878_CONVERT(0x0078, 0x0360),  // DATA_6878_CONVERT("MSDCPLL")
	REG_6878_CONVERT(0x008C, 0x0370),  // DATA_6878_CONVERT("UFSPLL")
	REG_6878_CONVERT(0x00A0, 0x0328),  // DATA_6878_CONVERT("MMPLL")
	REG_6878_CONVERT(0x00B4, 0x0308),  // DATA_6878_CONVERT("MAINPLL")
	{}
};
static struct fh_pll_regs mt6878_top0_regs[SIZE_6878_TOP0];
static struct fh_pll_domain mt6878_top0 = {
	.name = "top0",
	.data = (struct fh_pll_data *)&mt6878_top0_data,
	.offset = (struct fh_pll_offset *)&mt6878_top0_offset,
	.regs = (struct fh_pll_regs *)&mt6878_top0_regs,
	.init = &init_v1,
};
///////////////////////////////////top1 (MCU PLL)
static struct fh_pll_data mt6878_top1_data[] = {
	DATA_6878_CONVERT("ARMPLL_LL"),
	DATA_6878_CONVERT("ARMPLL_BL"),
	DATA_6878_CONVERT("CCIPLL"),
	{}
};
static struct fh_pll_offset mt6878_top1_offset[SIZE_6878_TOP1] = {
	REG_6878_CONVERT(0x003C, 0x0208),  // DATA_6878_CONVERT("ARMPLL_LL")
	REG_6878_CONVERT(0x0050, 0x0218),  // DATA_6878_CONVERT("ARMPLL_BL")
	REG_6878_CONVERT(0x0064, 0x0228),  // DATA_6878_CONVERT("CCIPLL")
	{}
};
static struct fh_pll_regs mt6878_top1_regs[SIZE_6878_TOP1];
static struct fh_pll_domain mt6878_top1 = {
	.name = "top1",
	.data = (struct fh_pll_data *)&mt6878_top1_data,
	.offset = (struct fh_pll_offset *)&mt6878_top1_offset,
	.regs = (struct fh_pll_regs *)&mt6878_top1_regs,
	.init = &init_v1,
};
///////////////////////////////////gpu0
static struct fh_pll_data mt6878_gpu0_data[] = {
	DATA_6878_CONVERT("MFGPLL"),
	{}
};
static struct fh_pll_offset mt6878_gpu0_offset[SIZE_6878_GPU0] = {
	REG_6878_TINYSYS_CONVERT(0x14, 0xC),
	{}
};
static struct fh_pll_regs mt6878_gpu0_regs[SIZE_6878_GPU0];
static struct fh_pll_domain mt6878_gpu0 = {
	.name = "gpu0",
	.data = (struct fh_pll_data *)&mt6878_gpu0_data,
	.offset = (struct fh_pll_offset *)&mt6878_gpu0_offset,
	.regs = (struct fh_pll_regs *)&mt6878_gpu0_regs,
	.init = &init_v1,
};

///////////////////////////////////gpu1
static struct fh_pll_data mt6878_gpu1_data[] = {
	DATA_6878_CONVERT("GPUEBPLL"),
	{}
};
static struct fh_pll_offset mt6878_gpu1_offset[SIZE_6878_GPU1] = {
	REG_6878_TINYSYS_CONVERT(0x14, 0xC),
	{}
};
static struct fh_pll_regs mt6878_gpu1_regs[SIZE_6878_GPU1];
static struct fh_pll_domain mt6878_gpu1 = {
	.name = "gpu1",
	.data = (struct fh_pll_data *)&mt6878_gpu1_data,
	.offset = (struct fh_pll_offset *)&mt6878_gpu1_offset,
	.regs = (struct fh_pll_regs *)&mt6878_gpu1_regs,
	.init = &init_v1,
};

///////////////////////////////////gpu2
static struct fh_pll_data mt6878_gpu2_data[] = {
	DATA_6878_CONVERT("MFGSCPLL"),
	{}
};
static struct fh_pll_offset mt6878_gpu2_offset[SIZE_6878_GPU2] = {
	REG_6878_TINYSYS_CONVERT(0x14, 0xC),
	{}
};
static struct fh_pll_regs mt6878_gpu2_regs[SIZE_6878_GPU2];
static struct fh_pll_domain mt6878_gpu2 = {
	.name = "gpu2",
	.data = (struct fh_pll_data *)&mt6878_gpu2_data,
	.offset = (struct fh_pll_offset *)&mt6878_gpu2_offset,
	.regs = (struct fh_pll_regs *)&mt6878_gpu2_regs,
	.init = &init_v1,
};

static struct fh_pll_domain *mt6878_domain[] = {
	&mt6878_top0,
	&mt6878_top1,
	&mt6878_gpu0,
	&mt6878_gpu1,
	&mt6878_gpu2,
	NULL
};
static struct match mt6878_match = {
	.compatible = "mediatek,mt6878-fhctl",
	.domain_list = (struct fh_pll_domain **)mt6878_domain,
};
/* 6878 end */

/* 6897 begin */
#define SIZE_6897_TOP (sizeof(mt6897_top_data)\
	/sizeof(struct fh_pll_data))
#define SIZE_6897_GPU0 (sizeof(mt6897_gpu0_data)\
	/sizeof(struct fh_pll_data))
/*
#define SIZE_6897_GPU1 (sizeof(mt6897_gpu1_data)\
	/sizeof(struct fh_pll_data))
#define SIZE_6897_GPU2 (sizeof(mt6897_gpu2_data)\
	/sizeof(struct fh_pll_data))
*/
#define SIZE_6897_GPU3 (sizeof(mt6897_gpu3_data)\
	/sizeof(struct fh_pll_data))
#define SIZE_6897_MCU0 (sizeof(mt6897_mcu0_data)\
	/sizeof(struct fh_pll_data))
#define SIZE_6897_MCU1 (sizeof(mt6897_mcu1_data)\
	/sizeof(struct fh_pll_data))
#define SIZE_6897_MCU2 (sizeof(mt6897_mcu2_data)\
	/sizeof(struct fh_pll_data))
#define SIZE_6897_MCU3 (sizeof(mt6897_mcu3_data)\
	/sizeof(struct fh_pll_data))
#define SIZE_6897_MCU4 (sizeof(mt6897_mcu4_data)\
	/sizeof(struct fh_pll_data))


#define DATA_6897_CONVERT(_name) {				\
		.name = _name,						\
		.dds_mask = GENMASK(21, 0),			\
		.postdiv_mask = GENMASK(26, 24),	\
		.postdiv_offset = 24,				\
		.slope0_value = 0x6003c97,			\
		.slope1_value = 0x6003c97,			\
		.sfstrx_en = BIT(2),				\
		.frddsx_en = BIT(1),				\
		.fhctlx_en = BIT(0),				\
		.tgl_org = BIT(31),					\
		.dvfs_tri = BIT(31),				\
		.pcwchg = BIT(31),					\
		.dt_val = 0x0,						\
		.df_val = 0x9,						\
		.updnlmt_shft = 16,					\
		.msk_frddsx_dys = GENMASK(23, 20),	\
		.msk_frddsx_dts = GENMASK(19, 16),	\
	}
#define REG_6897_CONVERT(_fhctl, _con_pcw) {	\
		.offset_fhctl = _fhctl,				\
		.offset_con_pcw = _con_pcw,			\
		.offset_con_postdiv = _con_pcw,		\
		.offset_hp_en = 0x0,				\
		.offset_hp_en_set = 0x168,			\
		.offset_hp_en_clr = 0x16c,			\
		.offset_clk_con = 0x8,				\
		.offset_clk_con_set = 0x170,		\
		.offset_clk_con_clr = 0x174,		\
		.offset_rst_con = 0xc,				\
		.offset_rst_con_set = 0x178,		\
		.offset_rst_con_clr = 0x17c,		\
		.offset_slope0 = 0x10,				\
		.offset_slope1 = 0x14,				\
		.offset_cfg = 0x0,					\
		.offset_updnlmt = 0x4,				\
		.offset_dds = 0x8,					\
		.offset_dvfs = 0xc,					\
		.offset_mon = 0x10,					\
	}
//TINYSYS no slope1, map to slope0 for compatibility
#define REG_6897_TINYSYS_CONVERT(_fhctl, _con_pcw) {	\
		.offset_fhctl = _fhctl,				\
		.offset_con_pcw = _con_pcw,			\
		.offset_con_postdiv = _con_pcw,		\
		.offset_hp_en = 0x0,				\
		.offset_clk_con = 0x8,				\
		.offset_rst_con = 0xc,				\
		.offset_slope0 = 0x10,				\
		.offset_slope1 = 0x10,				\
		.offset_cfg = 0x0,					\
		.offset_updnlmt = 0x4,				\
		.offset_dds = 0x8,					\
		.offset_dvfs = 0xc,					\
		.offset_mon = 0x10,					\
	}
static struct fh_pll_data mt6897_top_data[] = {
/*
	DATA_6897_CONVERT("mainpll2"),
	DATA_6897_CONVERT("mmpll2"),
*/
	DATA_6897_CONVERT("nouse-mempll"),
	DATA_6897_CONVERT("nouse-emipll"),
	DATA_6897_CONVERT("mpll"),
	DATA_6897_CONVERT("mmpll"),
	DATA_6897_CONVERT("mainpll"),
	DATA_6897_CONVERT("msdcpll"),
	DATA_6897_CONVERT("adsppll"),
	DATA_6897_CONVERT("imgpll"),
	DATA_6897_CONVERT("tvdpll"),
	{}
};
static struct fh_pll_offset mt6897_top_offset[SIZE_6897_TOP] = {
/*
	REG_6897_CONVERT(0x003C, 0x0284),  //	DATA_6897_CONVERT("mainpll2"),
	REG_6897_CONVERT(0x0050, 0x02A4),  //	DATA_6897_CONVERT("mmpll2"),
*/
	REG_6897_CONVERT(0x0064, 0xffff),  //	DATA_6897_CONVERT("mempll"),
	REG_6897_CONVERT(0x0078, 0x03B4),  //	DATA_6897_CONVERT("emipll"),
	REG_6897_CONVERT(0x008C, 0x0394),  //	DATA_6897_CONVERT("mpll"),
	REG_6897_CONVERT(0x00A0, 0x03A4),  //	DATA_6897_CONVERT("mmpll"),
	REG_6897_CONVERT(0x00B4, 0x0354),  //	DATA_6897_CONVERT("mainpll"),
	REG_6897_CONVERT(0x00C8, 0x0364),  //	DATA_6897_CONVERT("msdcpll "),
	REG_6897_CONVERT(0x00DC, 0x0384),  //	DATA_6897_CONVERT("adsppll"),
	REG_6897_CONVERT(0x00F0, 0x0374),  //	DATA_6897_CONVERT("imgpll"),
	REG_6897_CONVERT(0x0104, 0x024c),  //	DATA_6897_CONVERT("tvdpll"),
	{}
};
static struct fh_pll_regs mt6897_top_regs[SIZE_6897_TOP];
static struct fh_pll_domain mt6897_top = {
	.name = "top",
	.data = (struct fh_pll_data *)&mt6897_top_data,
	.offset = (struct fh_pll_offset *)&mt6897_top_offset,
	.regs = (struct fh_pll_regs *)&mt6897_top_regs,
	.init = &init_v1,
};

///////////////////////////////////gpu0
static struct fh_pll_data mt6897_gpu0_data[] = {
	DATA_6897_CONVERT("mfgpll"),
	{}
};
static struct fh_pll_offset mt6897_gpu0_offset[SIZE_6897_GPU0] = {
	REG_6897_TINYSYS_CONVERT(0x14, 0xC),
	{}
};
static struct fh_pll_regs mt6897_gpu0_regs[SIZE_6897_GPU0];
static struct fh_pll_domain mt6897_gpu0 = {
	.name = "gpu0",
	.data = (struct fh_pll_data *)&mt6897_gpu0_data,
	.offset = (struct fh_pll_offset *)&mt6897_gpu0_offset,
	.regs = (struct fh_pll_regs *)&mt6897_gpu0_regs,
	.init = &init_v1,
};

/*
///////////////////////////////////gpu1
static struct fh_pll_data mt6897_gpu1_data[] = {
	DATA_6897_CONVERT("mfgnr"),
	{}
};
static struct fh_pll_offset mt6897_gpu1_offset[SIZE_6897_GPU1] = {
	REG_6897_TINYSYS_CONVERT(0x14, 0xC),
	{}
};
static struct fh_pll_regs mt6897_gpu1_regs[SIZE_6897_GPU1];
static struct fh_pll_domain mt6897_gpu1 = {
	.name = "gpu1",
	.data = (struct fh_pll_data *)&mt6897_gpu1_data,
	.offset = (struct fh_pll_offset *)&mt6897_gpu1_offset,
	.regs = (struct fh_pll_regs *)&mt6897_gpu1_regs,
	.init = &init_v1,
};

///////////////////////////////////gpu2
static struct fh_pll_data mt6897_gpu2_data[] = {
	DATA_6897_CONVERT("gpuebpll"),
	{}
};
static struct fh_pll_offset mt6897_gpu2_offset[SIZE_6897_GPU2] = {
	REG_6897_TINYSYS_CONVERT(0x14, 0xC),
	{}
};
static struct fh_pll_regs mt6897_gpu2_regs[SIZE_6897_GPU2];
static struct fh_pll_domain mt6897_gpu2 = {
	.name = "gpu2",
	.data = (struct fh_pll_data *)&mt6897_gpu2_data,
	.offset = (struct fh_pll_offset *)&mt6897_gpu2_offset,
	.regs = (struct fh_pll_regs *)&mt6897_gpu2_regs,
	.init = &init_v1,
};
*/

///////////////////////////////////gpu3
static struct fh_pll_data mt6897_gpu3_data[] = {
	DATA_6897_CONVERT("mfgscpll"),
	{}
};
static struct fh_pll_offset mt6897_gpu3_offset[SIZE_6897_GPU3] = {
	REG_6897_TINYSYS_CONVERT(0x14, 0xC),
	{}
};
static struct fh_pll_regs mt6897_gpu3_regs[SIZE_6897_GPU3];
static struct fh_pll_domain mt6897_gpu3 = {
	.name = "gpu3",
	.data = (struct fh_pll_data *)&mt6897_gpu3_data,
	.offset = (struct fh_pll_offset *)&mt6897_gpu3_offset,
	.regs = (struct fh_pll_regs *)&mt6897_gpu3_regs,
	.init = &init_v1,
};

///////////////////////////////////mcu0
static struct fh_pll_data mt6897_mcu0_data[] = {
	DATA_6897_CONVERT("buspll"),
	{}
};
static struct fh_pll_offset mt6897_mcu0_offset[SIZE_6897_MCU0] = {
	REG_6897_TINYSYS_CONVERT(0x14, 0xC),
	{}
};
static struct fh_pll_regs mt6897_mcu0_regs[SIZE_6897_MCU0];
static struct fh_pll_domain mt6897_mcu0 = {
	.name = "mcu0",
	.data = (struct fh_pll_data *)&mt6897_mcu0_data,
	.offset = (struct fh_pll_offset *)&mt6897_mcu0_offset,
	.regs = (struct fh_pll_regs *)&mt6897_mcu0_regs,
	.init = &init_v1,
};

///////////////////////////////////mcu1
static struct fh_pll_data mt6897_mcu1_data[] = {
	DATA_6897_CONVERT("cpu0pll"),
	{}
};
static struct fh_pll_offset mt6897_mcu1_offset[SIZE_6897_MCU1] = {
	REG_6897_TINYSYS_CONVERT(0x14, 0xC),
	{}
};
static struct fh_pll_regs mt6897_mcu1_regs[SIZE_6897_MCU1];
static struct fh_pll_domain mt6897_mcu1 = {
	.name = "mcu1",
	.data = (struct fh_pll_data *)&mt6897_mcu1_data,
	.offset = (struct fh_pll_offset *)&mt6897_mcu1_offset,
	.regs = (struct fh_pll_regs *)&mt6897_mcu1_regs,
	.init = &init_v1,
};

///////////////////////////////////mcu2
static struct fh_pll_data mt6897_mcu2_data[] = {
	DATA_6897_CONVERT("cpu1pll"),
	{}
};
static struct fh_pll_offset mt6897_mcu2_offset[SIZE_6897_MCU2] = {
	REG_6897_TINYSYS_CONVERT(0x14, 0xC),
	{}
};
static struct fh_pll_regs mt6897_mcu2_regs[SIZE_6897_MCU2];
static struct fh_pll_domain mt6897_mcu2 = {
	.name = "mcu2",
	.data = (struct fh_pll_data *)&mt6897_mcu2_data,
	.offset = (struct fh_pll_offset *)&mt6897_mcu2_offset,
	.regs = (struct fh_pll_regs *)&mt6897_mcu2_regs,
	.init = &init_v1,
};
///////////////////////////////////mcu3
static struct fh_pll_data mt6897_mcu3_data[] = {
	DATA_6897_CONVERT("cpu2pll"),
	{}
};
static struct fh_pll_offset mt6897_mcu3_offset[SIZE_6897_MCU3] = {
	REG_6897_TINYSYS_CONVERT(0x14, 0xC),
	{}
};
static struct fh_pll_regs mt6897_mcu3_regs[SIZE_6897_MCU3];
static struct fh_pll_domain mt6897_mcu3 = {
	.name = "mcu3",
	.data = (struct fh_pll_data *)&mt6897_mcu3_data,
	.offset = (struct fh_pll_offset *)&mt6897_mcu3_offset,
	.regs = (struct fh_pll_regs *)&mt6897_mcu3_regs,
	.init = &init_v1,
};

///////////////////////////////////mcu4
static struct fh_pll_data mt6897_mcu4_data[] = {
	DATA_6897_CONVERT("ptppll"),
	{}
};
static struct fh_pll_offset mt6897_mcu4_offset[SIZE_6897_MCU4] = {
	REG_6897_TINYSYS_CONVERT(0x14, 0xC),
	{}
};
static struct fh_pll_regs mt6897_mcu4_regs[SIZE_6897_MCU4];
static struct fh_pll_domain mt6897_mcu4 = {
	.name = "mcu4",
	.data = (struct fh_pll_data *)&mt6897_mcu4_data,
	.offset = (struct fh_pll_offset *)&mt6897_mcu4_offset,
	.regs = (struct fh_pll_regs *)&mt6897_mcu4_regs,
	.init = &init_v1,
};

static struct fh_pll_domain *mt6897_domain[] = {
	&mt6897_top,
	&mt6897_gpu0,
/*
	&mt6897_gpu1,
	&mt6897_gpu2,
*/
	&mt6897_gpu3,
	&mt6897_mcu0,
	&mt6897_mcu1,
	&mt6897_mcu2,
	&mt6897_mcu3,
	&mt6897_mcu4,
	NULL
};
static struct match mt6897_match = {
	.compatible = "mediatek,mt6897-fhctl",
	.domain_list = (struct fh_pll_domain **)mt6897_domain,
};
/* 6897 end */


/* 6985 begin */
#define SIZE_6985_TOP0 (sizeof(mt6985_top0_data)\
	/sizeof(struct fh_pll_data))
#define SIZE_6985_TOP1 (sizeof(mt6985_top1_data)\
	/sizeof(struct fh_pll_data))
#define SIZE_6985_GPU0 (sizeof(mt6985_gpu0_data)\
	/sizeof(struct fh_pll_data))
#define SIZE_6985_GPU1 (sizeof(mt6985_gpu1_data)\
	/sizeof(struct fh_pll_data))
#define SIZE_6985_GPU2 (sizeof(mt6985_gpu2_data)\
	/sizeof(struct fh_pll_data))
#define SIZE_6985_GPU3 (sizeof(mt6985_gpu3_data)\
	/sizeof(struct fh_pll_data))
#define SIZE_6985_MCU0 (sizeof(mt6985_mcu0_data)\
	/sizeof(struct fh_pll_data))
#define SIZE_6985_MCU1 (sizeof(mt6985_mcu1_data)\
	/sizeof(struct fh_pll_data))
#define SIZE_6985_MCU2 (sizeof(mt6985_mcu2_data)\
	/sizeof(struct fh_pll_data))
#define SIZE_6985_MCU3 (sizeof(mt6985_mcu3_data)\
	/sizeof(struct fh_pll_data))
#define SIZE_6985_MCU4 (sizeof(mt6985_mcu4_data)\
	/sizeof(struct fh_pll_data))


#define DATA_6985_CONVERT(_name) {				\
		.name = _name,						\
		.dds_mask = GENMASK(21, 0),			\
		.postdiv_mask = GENMASK(26, 24),		    \
		.postdiv_offset = 24,				        \
		.slope0_value = 0x6003c97,			\
		.slope1_value = 0x6003c97,			\
		.sfstrx_en = BIT(2),				\
		.frddsx_en = BIT(1),				\
		.fhctlx_en = BIT(0),				\
		.tgl_org = BIT(31),					\
		.dvfs_tri = BIT(31),				\
		.pcwchg = BIT(31),					\
		.dt_val = 0x0,						\
		.df_val = 0x9,						\
		.updnlmt_shft = 16,					\
		.msk_frddsx_dys = GENMASK(23, 20),	\
		.msk_frddsx_dts = GENMASK(19, 16),	\
	}
#define REG_6985_CONVERT(_fhctl, _con_pcw) {	\
		.offset_fhctl = _fhctl,				\
		.offset_con_pcw = _con_pcw,			\
		.offset_con_postdiv = _con_pcw, 	\
		.offset_hp_en = 0x0,				\
		.offset_clk_con = 0x8,				\
		.offset_rst_con = 0xc,				\
		.offset_slope0 = 0x10,				\
		.offset_slope1 = 0x14,				\
		.offset_cfg = 0x0,					\
		.offset_updnlmt = 0x4,				\
		.offset_dds = 0x8,					\
		.offset_dvfs = 0xc,					\
		.offset_mon = 0x10,					\
	}
//TINYSYS no slope1, map to slope0 for compatibility
#define REG_6985_TINYSYS_CONVERT(_fhctl, _con_pcw) {	\
		.offset_fhctl = _fhctl,				\
		.offset_con_pcw = _con_pcw,			\
		.offset_con_postdiv = _con_pcw, 	\
		.offset_hp_en = 0x0,				\
		.offset_clk_con = 0x8,				\
		.offset_rst_con = 0xc,				\
		.offset_slope0 = 0x10,				\
		.offset_slope1 = 0x10,				\
		.offset_cfg = 0x0,					\
		.offset_updnlmt = 0x4,				\
		.offset_dds = 0x8,					\
		.offset_dvfs = 0xc,					\
		.offset_mon = 0x10,					\
	}
static struct fh_pll_data mt6985_top0_data[] = {
	DATA_6985_CONVERT("mainpll2"),
	DATA_6985_CONVERT("mmpll2"),
	DATA_6985_CONVERT("mempll"),
	DATA_6985_CONVERT("emipll"),
	DATA_6985_CONVERT("mpll"),
	DATA_6985_CONVERT("mmpll"),
	DATA_6985_CONVERT("mainpll"),
	DATA_6985_CONVERT("msdcpll"),
	DATA_6985_CONVERT("adsppll"),
	DATA_6985_CONVERT("imgpll"),
	DATA_6985_CONVERT("tvdpll"),
	{}
};
static struct fh_pll_offset mt6985_top0_offset[SIZE_6985_TOP0] = {
	REG_6985_CONVERT(0x003C, 0x0284),  //	DATA_6985_CONVERT("mainpll2"),
	REG_6985_CONVERT(0x0050, 0x02A4),  //	DATA_6985_CONVERT("mmpll2"),
	REG_6985_CONVERT(0x0064, 0xffff),  //	DATA_6985_CONVERT("mempll"),
	REG_6985_CONVERT(0x0078, 0x03B4),  //	DATA_6985_CONVERT("emipll"),
	REG_6985_CONVERT(0x008C, 0x0394),  //	DATA_6985_CONVERT("mpll"),
	REG_6985_CONVERT(0x00A0, 0x03A4),  //	DATA_6985_CONVERT("mmpll"),
	REG_6985_CONVERT(0x00B4, 0x0354),  //	DATA_6985_CONVERT("mainpll"),
	REG_6985_CONVERT(0x00C8, 0x0364),  //	DATA_6985_CONVERT("msdcpll "),
	REG_6985_CONVERT(0x00DC, 0x0384),  //	DATA_6985_CONVERT("adsppll"),
	REG_6985_CONVERT(0x00F0, 0x0374),  //	DATA_6985_CONVERT("imgpll"),
	REG_6985_CONVERT(0x0104, 0x024c),  //	DATA_6985_CONVERT("tvdpll"),
	{}
};
static struct fh_pll_regs mt6985_top0_regs[SIZE_6985_TOP0];
static struct fh_pll_domain mt6985_top0 = {
	.name = "top0",
	.data = (struct fh_pll_data *)&mt6985_top0_data,
	.offset = (struct fh_pll_offset *)&mt6985_top0_offset,
	.regs = (struct fh_pll_regs *)&mt6985_top0_regs,
	.init = &init_v1,
};

///////////////////////////////////top1
static struct fh_pll_data mt6985_top1_data[] = {
	DATA_6985_CONVERT("nouse"),
	DATA_6985_CONVERT("nouse"),
	DATA_6985_CONVERT("nouse"),
	DATA_6985_CONVERT("nouse"),
	DATA_6985_CONVERT("nouse"),
	DATA_6985_CONVERT("nouse"),
	DATA_6985_CONVERT("nouse"),
	DATA_6985_CONVERT("nouse"),
	DATA_6985_CONVERT("nouse"),
	DATA_6985_CONVERT("imgpll"),
	{}
};
static struct fh_pll_offset mt6985_top1_offset[SIZE_6985_TOP1] = {
	REG_6985_CONVERT(0x003C, 0x0284),  //	DATA_6985_CONVERT("mainpll2"),
	REG_6985_CONVERT(0x0050, 0x02A4),  //	DATA_6985_CONVERT("mmpll2"),
	REG_6985_CONVERT(0x0064, 0xffff),  //	DATA_6985_CONVERT("mempll"),
	REG_6985_CONVERT(0x0078, 0x03B4),  //	DATA_6985_CONVERT("emipll"),
	REG_6985_CONVERT(0x008C, 0x0394),  //	DATA_6985_CONVERT("mpll"),
	REG_6985_CONVERT(0x00A0, 0x03A4),  //	DATA_6985_CONVERT("mmpll"),
	REG_6985_CONVERT(0x00B4, 0x0354),  //	DATA_6985_CONVERT("mainpll"),
	REG_6985_CONVERT(0x00C8, 0x0364),  //	DATA_6985_CONVERT("msdcpll "),
	REG_6985_CONVERT(0x00DC, 0x0384),  //	DATA_6985_CONVERT("adsppll"),
	REG_6985_CONVERT(0x00F0, 0x0374),  //	DATA_6985_CONVERT("imgpll"),
	{}
};
static struct fh_pll_regs mt6985_top1_regs[SIZE_6985_TOP1];
static struct fh_pll_domain mt6985_top1 = {
	.name = "top1",
	.data = (struct fh_pll_data *)&mt6985_top1_data,
	.offset = (struct fh_pll_offset *)&mt6985_top1_offset,
	.regs = (struct fh_pll_regs *)&mt6985_top1_regs,
	.init = &init_v1,
};

///////////////////////////////////gpu0
static struct fh_pll_data mt6985_gpu0_data[] = {
	DATA_6985_CONVERT("mfgpll"),
	{}
};
static struct fh_pll_offset mt6985_gpu0_offset[SIZE_6985_GPU0] = {
	REG_6985_TINYSYS_CONVERT(0x14, 0xC),
	{}
};
static struct fh_pll_regs mt6985_gpu0_regs[SIZE_6985_GPU0];
static struct fh_pll_domain mt6985_gpu0 = {
	.name = "gpu0",
	.data = (struct fh_pll_data *)&mt6985_gpu0_data,
	.offset = (struct fh_pll_offset *)&mt6985_gpu0_offset,
	.regs = (struct fh_pll_regs *)&mt6985_gpu0_regs,
	.init = &init_v1,
};
///////////////////////////////////gpu1

static struct fh_pll_data mt6985_gpu1_data[] = {
	DATA_6985_CONVERT("mfgnr"),
	{}
};
static struct fh_pll_offset mt6985_gpu1_offset[SIZE_6985_GPU1] = {
	REG_6985_TINYSYS_CONVERT(0x14, 0xC),
	{}
};
static struct fh_pll_regs mt6985_gpu1_regs[SIZE_6985_GPU1];
static struct fh_pll_domain mt6985_gpu1 = {
	.name = "gpu1",
	.data = (struct fh_pll_data *)&mt6985_gpu1_data,
	.offset = (struct fh_pll_offset *)&mt6985_gpu1_offset,
	.regs = (struct fh_pll_regs *)&mt6985_gpu1_regs,
	.init = &init_v1,
};

///////////////////////////////////gpu2
static struct fh_pll_data mt6985_gpu2_data[] = {
	DATA_6985_CONVERT("gpuebpll"),
	{}
};
static struct fh_pll_offset mt6985_gpu2_offset[SIZE_6985_GPU2] = {
	REG_6985_TINYSYS_CONVERT(0x14, 0xC),
	{}
};
static struct fh_pll_regs mt6985_gpu2_regs[SIZE_6985_GPU2];
static struct fh_pll_domain mt6985_gpu2 = {
	.name = "gpu2",
	.data = (struct fh_pll_data *)&mt6985_gpu2_data,
	.offset = (struct fh_pll_offset *)&mt6985_gpu2_offset,
	.regs = (struct fh_pll_regs *)&mt6985_gpu2_regs,
	.init = &init_v1,
};
///////////////////////////////////gpu3
static struct fh_pll_data mt6985_gpu3_data[] = {
	DATA_6985_CONVERT("mfgscpll"),
	{}
};
static struct fh_pll_offset mt6985_gpu3_offset[SIZE_6985_GPU3] = {
	REG_6985_TINYSYS_CONVERT(0x14, 0xC),
	{}
};
static struct fh_pll_regs mt6985_gpu3_regs[SIZE_6985_GPU3];
static struct fh_pll_domain mt6985_gpu3 = {
	.name = "gpu3",
	.data = (struct fh_pll_data *)&mt6985_gpu3_data,
	.offset = (struct fh_pll_offset *)&mt6985_gpu3_offset,
	.regs = (struct fh_pll_regs *)&mt6985_gpu3_regs,
	.init = &init_v1,
};
///////////////////////////////////mcu0
static struct fh_pll_data mt6985_mcu0_data[] = {
	DATA_6985_CONVERT("buspll"),
	{}
};
static struct fh_pll_offset mt6985_mcu0_offset[SIZE_6985_MCU0] = {
	REG_6985_TINYSYS_CONVERT(0x14, 0xC),
	{}
};
static struct fh_pll_regs mt6985_mcu0_regs[SIZE_6985_MCU0];
static struct fh_pll_domain mt6985_mcu0 = {
	.name = "mcu0",
	.data = (struct fh_pll_data *)&mt6985_mcu0_data,
	.offset = (struct fh_pll_offset *)&mt6985_mcu0_offset,
	.regs = (struct fh_pll_regs *)&mt6985_mcu0_regs,
	.init = &init_v1,
};
///////////////////////////////////mcu1

static struct fh_pll_data mt6985_mcu1_data[] = {
	DATA_6985_CONVERT("cpu0pll"),
	{}
};
static struct fh_pll_offset mt6985_mcu1_offset[SIZE_6985_MCU1] = {
	REG_6985_TINYSYS_CONVERT(0x14, 0xC),
	{}
};
static struct fh_pll_regs mt6985_mcu1_regs[SIZE_6985_MCU1];
static struct fh_pll_domain mt6985_mcu1 = {
	.name = "mcu1",
	.data = (struct fh_pll_data *)&mt6985_mcu1_data,
	.offset = (struct fh_pll_offset *)&mt6985_mcu1_offset,
	.regs = (struct fh_pll_regs *)&mt6985_mcu1_regs,
	.init = &init_v1,
};

///////////////////////////////////mcu2
static struct fh_pll_data mt6985_mcu2_data[] = {
	DATA_6985_CONVERT("cpu1pll"),
	{}
};
static struct fh_pll_offset mt6985_mcu2_offset[SIZE_6985_MCU2] = {
	REG_6985_TINYSYS_CONVERT(0x14, 0xC),
	{}
};
static struct fh_pll_regs mt6985_mcu2_regs[SIZE_6985_MCU2];
static struct fh_pll_domain mt6985_mcu2 = {
	.name = "mcu2",
	.data = (struct fh_pll_data *)&mt6985_mcu2_data,
	.offset = (struct fh_pll_offset *)&mt6985_mcu2_offset,
	.regs = (struct fh_pll_regs *)&mt6985_mcu2_regs,
	.init = &init_v1,
};
///////////////////////////////////mcu3
static struct fh_pll_data mt6985_mcu3_data[] = {
	DATA_6985_CONVERT("cpu2pll"),
	{}
};
static struct fh_pll_offset mt6985_mcu3_offset[SIZE_6985_MCU3] = {
	REG_6985_TINYSYS_CONVERT(0x14, 0xC),
	{}
};
static struct fh_pll_regs mt6985_mcu3_regs[SIZE_6985_MCU3];
static struct fh_pll_domain mt6985_mcu3 = {
	.name = "mcu3",
	.data = (struct fh_pll_data *)&mt6985_mcu3_data,
	.offset = (struct fh_pll_offset *)&mt6985_mcu3_offset,
	.regs = (struct fh_pll_regs *)&mt6985_mcu3_regs,
	.init = &init_v1,
};
///////////////////////////////////mcu4
static struct fh_pll_data mt6985_mcu4_data[] = {
	DATA_6985_CONVERT("ptppll"),
	{}
};
static struct fh_pll_offset mt6985_mcu4_offset[SIZE_6985_MCU4] = {
	REG_6985_TINYSYS_CONVERT(0x14, 0xC),
	{}
};
static struct fh_pll_regs mt6985_mcu4_regs[SIZE_6985_MCU4];
static struct fh_pll_domain mt6985_mcu4 = {
	.name = "mcu4",
	.data = (struct fh_pll_data *)&mt6985_mcu4_data,
	.offset = (struct fh_pll_offset *)&mt6985_mcu4_offset,
	.regs = (struct fh_pll_regs *)&mt6985_mcu4_regs,
	.init = &init_v1,
};

static struct fh_pll_domain *mt6985_domain[] = {
	&mt6985_top0,
	&mt6985_top1,
	&mt6985_gpu0,
	&mt6985_gpu1,
	&mt6985_gpu2,
	&mt6985_gpu3,
	&mt6985_mcu0,
	&mt6985_mcu1,
	&mt6985_mcu2,
	&mt6985_mcu3,
	&mt6985_mcu4,
	NULL
};
static struct match mt6985_match = {
	.compatible = "mediatek,mt6985-fhctl",
	.domain_list = (struct fh_pll_domain **)mt6985_domain,
};
/* 6985 end */

/* 6989 begin */
#define SIZE_6989_TOP0 (sizeof(mt6989_top0_data)\
	/sizeof(struct fh_pll_data))
#define SIZE_6989_TOP1 (sizeof(mt6989_top1_data)\
	/sizeof(struct fh_pll_data))
#define SIZE_6989_GPU0 (sizeof(mt6989_gpu0_data)\
	/sizeof(struct fh_pll_data))
#define SIZE_6989_GPU1 (sizeof(mt6989_gpu1_data)\
	/sizeof(struct fh_pll_data))
#define SIZE_6989_GPU2 (sizeof(mt6989_gpu2_data)\
	/sizeof(struct fh_pll_data))
#define SIZE_6989_MCU0 (sizeof(mt6989_mcu0_data)\
	/sizeof(struct fh_pll_data))
#define SIZE_6989_MCU1 (sizeof(mt6989_mcu1_data)\
	/sizeof(struct fh_pll_data))
#define SIZE_6989_MCU2 (sizeof(mt6989_mcu2_data)\
	/sizeof(struct fh_pll_data))
#define SIZE_6989_MCU3 (sizeof(mt6989_mcu3_data)\
	/sizeof(struct fh_pll_data))
#define SIZE_6989_MCU4 (sizeof(mt6989_mcu4_data)\
	/sizeof(struct fh_pll_data))


#define DATA_6989_CONVERT(_name) {				\
		.name = _name,						\
		.dds_mask = GENMASK(21, 0),			\
		.postdiv_mask = GENMASK(26, 24),		    \
		.postdiv_offset = 24,				        \
		.slope0_value = 0x6003c97,			\
		.slope1_value = 0x6003c97,			\
		.sfstrx_en = BIT(2),				\
		.frddsx_en = BIT(1),				\
		.fhctlx_en = BIT(0),				\
		.tgl_org = BIT(31),					\
		.dvfs_tri = BIT(31),				\
		.pcwchg = BIT(31),					\
		.dt_val = 0x0,						\
		.df_val = 0x9,						\
		.updnlmt_shft = 16,					\
		.msk_frddsx_dys = GENMASK(23, 20),	\
		.msk_frddsx_dts = GENMASK(19, 16),	\
	}
#define REG_6989_CONVERT(_fhctl, _con_pcw) {	\
		.offset_fhctl = _fhctl,				\
		.offset_con_pcw = _con_pcw,			\
		.offset_con_postdiv = _con_pcw,		\
		.offset_hp_en = 0x0,                \
		.offset_hp_en_set = 0x168,          \
		.offset_hp_en_clr = 0x16c,			\
		.offset_clk_con = 0x8,				\
		.offset_clk_con_set = 0x170,        \
		.offset_clk_con_clr = 0x174,        \
		.offset_rst_con = 0xc,				\
		.offset_rst_con_set = 0x178,        \
		.offset_rst_con_clr = 0x17c,        \
		.offset_slope0 = 0x10,				\
		.offset_slope1 = 0x14,				\
		.offset_cfg = 0x0,					\
		.offset_updnlmt = 0x4,				\
		.offset_dds = 0x8,					\
		.offset_dvfs = 0xc,					\
		.offset_mon = 0x10,					\
	}
//TINYSYS no slope1, map to slope0 for compatibility
#define REG_6989_TINYSYS_CONVERT(_fhctl, _con_pcw) {	\
		.offset_fhctl = _fhctl,				\
		.offset_con_pcw = _con_pcw,			\
		.offset_con_postdiv = _con_pcw,		\
		.offset_hp_en = 0x0,				\
		.offset_clk_con = 0x8,				\
		.offset_rst_con = 0xc,				\
		.offset_slope0 = 0x10,				\
		.offset_slope1 = 0x10,				\
		.offset_cfg = 0x0,					\
		.offset_updnlmt = 0x4,				\
		.offset_dds = 0x8,					\
		.offset_dvfs = 0xc,					\
		.offset_mon = 0x10,					\
	}


///////////////////////////////////top0
static struct fh_pll_data mt6989_top0_data[] = {
	DATA_6989_CONVERT("mainpll2"),
	DATA_6989_CONVERT("mmpll2"),
	DATA_6989_CONVERT("noHW-mempll"),
	DATA_6989_CONVERT("emipll2"),
	DATA_6989_CONVERT("emipll"),
	DATA_6989_CONVERT("noHW-mpll"),
	DATA_6989_CONVERT("mmpll"),
	DATA_6989_CONVERT("mainpll"),
	DATA_6989_CONVERT("msdcpll"),
	DATA_6989_CONVERT("adsppll"),
	DATA_6989_CONVERT("nouse-imgpll"),
	DATA_6989_CONVERT("tvdpll"),
	{}
};
static struct fh_pll_offset mt6989_top0_offset[SIZE_6989_TOP0] = {
	REG_6989_CONVERT(0x003C, 0x0284),  //	DATA_6989_CONVERT("mainpll2"),
	REG_6989_CONVERT(0x0050, 0x02A4),  //	DATA_6989_CONVERT("mmpll2"),
	REG_6989_CONVERT(0x0064, 0xffff),  //	DATA_6989_CONVERT("mempll"),
	REG_6989_CONVERT(0x0078, 0x03C4),  //	DATA_6989_CONVERT("emipll2"),

	REG_6989_CONVERT(0x008C, 0x03B4),  //   DATA_6989_CONVERT("emipll"),
	REG_6989_CONVERT(0x00A0, 0xffff),  //	can not find mpll con1@ apmix CODA
	REG_6989_CONVERT(0x00B4, 0x03A4),  //	DATA_6989_CONVERT("mmpll"),
	REG_6989_CONVERT(0x00C8, 0x0354),  //	DATA_6989_CONVERT("mainpll"),
	REG_6989_CONVERT(0x00DC, 0x0364),  //	DATA_6989_CONVERT("msdcpll "),
	REG_6989_CONVERT(0x00F0, 0x0384),  //	DATA_6989_CONVERT("adsppll"),
	REG_6989_CONVERT(0x0104, 0x0374),  //	DATA_6989_CONVERT("imgpll"),
	REG_6989_CONVERT(0x0118, 0x024c),  //	DATA_6989_CONVERT("tvdpll"),
	{}
};
static struct fh_pll_regs mt6989_top0_regs[SIZE_6989_TOP0];
static struct fh_pll_domain mt6989_top0 = {
	.name = "top0",
	.data = (struct fh_pll_data *)&mt6989_top0_data,
	.offset = (struct fh_pll_offset *)&mt6989_top0_offset,
	.regs = (struct fh_pll_regs *)&mt6989_top0_regs,
	.init = &init_v1,
};
///////////////////////////////////top1
static struct fh_pll_data mt6989_top1_data[] = {
	DATA_6989_CONVERT("nouse"),
	DATA_6989_CONVERT("nouse"),
	DATA_6989_CONVERT("nouse"),
	DATA_6989_CONVERT("nouse"),
	DATA_6989_CONVERT("nouse"),
	DATA_6989_CONVERT("nouse"),
	DATA_6989_CONVERT("nouse"),
	DATA_6989_CONVERT("nouse"),
	DATA_6989_CONVERT("nouse"),
	DATA_6989_CONVERT("nouse"),
	DATA_6989_CONVERT("imgpll"),
	{}
};
static struct fh_pll_offset mt6989_top1_offset[SIZE_6989_TOP1] = {
	REG_6989_CONVERT(0x003C, 0x0284),  //	DATA_6989_CONVERT("mainpll2"),
	REG_6989_CONVERT(0x0050, 0x02A4),  //	DATA_6989_CONVERT("mmpll2"),
	REG_6989_CONVERT(0x0064, 0xffff),  //	DATA_6989_CONVERT("mempll"),
	REG_6989_CONVERT(0x0078, 0x03C4),  //	DATA_6989_CONVERT("emipll2"),

	REG_6989_CONVERT(0x008C, 0x03B4),  //   DATA_6989_CONVERT("emipll"),
	REG_6989_CONVERT(0x00A0, 0xffff),  //	can not find mpll con1@ apmix CODA
	REG_6989_CONVERT(0x00B4, 0x03A4),  //	DATA_6989_CONVERT("mmpll"),
	REG_6989_CONVERT(0x00C8, 0x0354),  //	DATA_6989_CONVERT("mainpll"),
	REG_6989_CONVERT(0x00DC, 0x0364),  //	DATA_6989_CONVERT("msdcpll "),
	REG_6989_CONVERT(0x00F0, 0x0384),  //	DATA_6989_CONVERT("adsppll"),
	REG_6989_CONVERT(0x0104, 0x0374),  //	DATA_6989_CONVERT("imgpll"),
	{}
};
static struct fh_pll_regs mt6989_top1_regs[SIZE_6989_TOP1];
static struct fh_pll_domain mt6989_top1 = {
	.name = "top1",
	.data = (struct fh_pll_data *)&mt6989_top1_data,
	.offset = (struct fh_pll_offset *)&mt6989_top1_offset,
	.regs = (struct fh_pll_regs *)&mt6989_top1_regs,
	.init = &init_v1,
};
///////////////////////////////////gpu0
static struct fh_pll_data mt6989_gpu0_data[] = {
	DATA_6989_CONVERT("mfg-ao-mfgpll"),
	{}
};
static struct fh_pll_offset mt6989_gpu0_offset[SIZE_6989_GPU0] = {
	REG_6989_TINYSYS_CONVERT(0x14, 0xC),
	{}
};
static struct fh_pll_regs mt6989_gpu0_regs[SIZE_6989_GPU0];
static struct fh_pll_domain mt6989_gpu0 = {
	.name = "gpu0",
	.data = (struct fh_pll_data *)&mt6989_gpu0_data,
	.offset = (struct fh_pll_offset *)&mt6989_gpu0_offset,
	.regs = (struct fh_pll_regs *)&mt6989_gpu0_regs,
	.init = &init_v1,
};
///////////////////////////////////gpu1

static struct fh_pll_data mt6989_gpu1_data[] = {
	DATA_6989_CONVERT("mfgsc0-ao-mfgpll-sc0"),
	{}
};
static struct fh_pll_offset mt6989_gpu1_offset[SIZE_6989_GPU1] = {
	REG_6989_TINYSYS_CONVERT(0x14, 0xC),
	{}
};
static struct fh_pll_regs mt6989_gpu1_regs[SIZE_6989_GPU1];
static struct fh_pll_domain mt6989_gpu1 = {
	.name = "gpu1",
	.data = (struct fh_pll_data *)&mt6989_gpu1_data,
	.offset = (struct fh_pll_offset *)&mt6989_gpu1_offset,
	.regs = (struct fh_pll_regs *)&mt6989_gpu1_regs,
	.init = &init_v1,
};

///////////////////////////////////gpu2
static struct fh_pll_data mt6989_gpu2_data[] = {
	DATA_6989_CONVERT("mfgsc1-ao-mfgpll-sc1"),
	{}
};
static struct fh_pll_offset mt6989_gpu2_offset[SIZE_6989_GPU2] = {
	REG_6989_TINYSYS_CONVERT(0x14, 0xC),
	{}
};
static struct fh_pll_regs mt6989_gpu2_regs[SIZE_6989_GPU2];
static struct fh_pll_domain mt6989_gpu2 = {
	.name = "gpu2",
	.data = (struct fh_pll_data *)&mt6989_gpu2_data,
	.offset = (struct fh_pll_offset *)&mt6989_gpu2_offset,
	.regs = (struct fh_pll_regs *)&mt6989_gpu2_regs,
	.init = &init_v1,
};
///////////////////////////////////mcu0
static struct fh_pll_data mt6989_mcu0_data[] = {
	DATA_6989_CONVERT("buspll"),
	{}
};
static struct fh_pll_offset mt6989_mcu0_offset[SIZE_6989_MCU0] = {
	REG_6989_TINYSYS_CONVERT(0x14, 0xC),
	{}
};
static struct fh_pll_regs mt6989_mcu0_regs[SIZE_6989_MCU0];
static struct fh_pll_domain mt6989_mcu0 = {
	.name = "mcu0",
	.data = (struct fh_pll_data *)&mt6989_mcu0_data,
	.offset = (struct fh_pll_offset *)&mt6989_mcu0_offset,
	.regs = (struct fh_pll_regs *)&mt6989_mcu0_regs,
	.init = &init_v1,
};
///////////////////////////////////mcu1

static struct fh_pll_data mt6989_mcu1_data[] = {
	DATA_6989_CONVERT("cpu0pll"),
	{}
};
static struct fh_pll_offset mt6989_mcu1_offset[SIZE_6989_MCU1] = {
	REG_6989_TINYSYS_CONVERT(0x14, 0xC),
	{}
};
static struct fh_pll_regs mt6989_mcu1_regs[SIZE_6989_MCU1];
static struct fh_pll_domain mt6989_mcu1 = {
	.name = "mcu1",
	.data = (struct fh_pll_data *)&mt6989_mcu1_data,
	.offset = (struct fh_pll_offset *)&mt6989_mcu1_offset,
	.regs = (struct fh_pll_regs *)&mt6989_mcu1_regs,
	.init = &init_v1,
};

///////////////////////////////////mcu2
static struct fh_pll_data mt6989_mcu2_data[] = {
	DATA_6989_CONVERT("cpu1pll"),
	{}
};
static struct fh_pll_offset mt6989_mcu2_offset[SIZE_6989_MCU2] = {
	REG_6989_TINYSYS_CONVERT(0x14, 0xC),
	{}
};
static struct fh_pll_regs mt6989_mcu2_regs[SIZE_6989_MCU2];
static struct fh_pll_domain mt6989_mcu2 = {
	.name = "mcu2",
	.data = (struct fh_pll_data *)&mt6989_mcu2_data,
	.offset = (struct fh_pll_offset *)&mt6989_mcu2_offset,
	.regs = (struct fh_pll_regs *)&mt6989_mcu2_regs,
	.init = &init_v1,
};
///////////////////////////////////mcu3
static struct fh_pll_data mt6989_mcu3_data[] = {
	DATA_6989_CONVERT("cpu2pll"),
	{}
};
static struct fh_pll_offset mt6989_mcu3_offset[SIZE_6989_MCU3] = {
	REG_6989_TINYSYS_CONVERT(0x14, 0xC),
	{}
};
static struct fh_pll_regs mt6989_mcu3_regs[SIZE_6989_MCU3];
static struct fh_pll_domain mt6989_mcu3 = {
	.name = "mcu3",
	.data = (struct fh_pll_data *)&mt6989_mcu3_data,
	.offset = (struct fh_pll_offset *)&mt6989_mcu3_offset,
	.regs = (struct fh_pll_regs *)&mt6989_mcu3_regs,
	.init = &init_v1,
};
///////////////////////////////////mcu4
static struct fh_pll_data mt6989_mcu4_data[] = {
	DATA_6989_CONVERT("ptppll"),
	{}
};
static struct fh_pll_offset mt6989_mcu4_offset[SIZE_6989_MCU4] = {
	REG_6989_TINYSYS_CONVERT(0x14, 0xC),
	{}
};
static struct fh_pll_regs mt6989_mcu4_regs[SIZE_6989_MCU4];
static struct fh_pll_domain mt6989_mcu4 = {
	.name = "mcu4",
	.data = (struct fh_pll_data *)&mt6989_mcu4_data,
	.offset = (struct fh_pll_offset *)&mt6989_mcu4_offset,
	.regs = (struct fh_pll_regs *)&mt6989_mcu4_regs,
	.init = &init_v1,
};

static struct fh_pll_domain *mt6989_domain[] = {
	&mt6989_top0,
	&mt6989_top1,
	&mt6989_gpu0,
	&mt6989_gpu1,
	&mt6989_gpu2,
	&mt6989_mcu0,
	&mt6989_mcu1,
	&mt6989_mcu2,
	&mt6989_mcu3,
	&mt6989_mcu4,
	NULL
};
static struct match mt6989_match = {
	.compatible = "mediatek,mt6989-fhctl",
	.domain_list = (struct fh_pll_domain **)mt6989_domain,
};
/* 6989 end */


static const struct match *matches[] = {
	&mt6878_match,
	&mt6897_match,
	&mt6985_match,
	&mt6989_match,
	NULL
};

static struct fh_pll_domain **get_list(char *comp)
{
	struct match **match;
	static struct fh_pll_domain **list;

	match = (struct match **)matches;

	/* name used only if !list */
	if (!list) {
		while (*matches != NULL) {
			if (strcmp(comp,
						(*match)->compatible) == 0) {
				list = (*match)->domain_list;
				FHDBG("target<%s>\n", comp);
				break;
			}
			match++;
		}
	}
	return list;
}
void init_fh_domain(const char *domain,
		char *comp,
		void __iomem *fhctl_base,
		void __iomem *apmixed_base)
{
	struct fh_pll_domain **list;

	list = get_list(comp);

	while (*list != NULL) {
		if (strcmp(domain,
					(*list)->name) == 0) {
			(*list)->init(*list,
					fhctl_base,
					apmixed_base);
			return;
		}
		list++;
	}
}
struct fh_pll_domain *get_fh_domain(const char *domain)
{
	struct fh_pll_domain **list;

	list = get_list(NULL);

	/* find instance */
	while (*list != NULL) {
		if (strcmp(domain,
					(*list)->name) == 0)
			return *list;
		list++;
	}
	return NULL;
}
