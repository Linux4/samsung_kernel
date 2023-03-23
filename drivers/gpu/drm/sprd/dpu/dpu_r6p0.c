/*
 * Copyright (C) 2021 UNISOC Technologies Co.,Ltd.
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

#include <linux/backlight.h>
#include <linux/delay.h>
#include <linux/of_address.h>
#include <linux/wait.h>
#include <linux/workqueue.h>
#include "sprd_bl.h"
#include "sprd_dpu.h"
#include "sprd_dvfs_dpu.h"
#include "disp_trusty.h"
#include "dpu_r6p0_corner_param.h"
#include "dpu_enhance_param.h"
#include "dpu_r6p0_scale_param.h"
#include "sprd_dsi.h"

#define DPU_INT_DONE_MASK			BIT(0)
#define DPU_INT_TE_MASK				BIT(1)
#define DPU_INT_ERR_MASK			BIT(2)
#define DPU_INT_EDPI_TE_MASK			BIT(3)
#define DPU_INT_DPI_VSYNC_MASK			BIT(4)
#define DPU_INT_WB_DONE_MASK			BIT(5)
#define DPU_INT_WB_FAIL_MASK			BIT(6)
#define DPU_INT_FBC_HDR_ERR_MASK		BIT(7)
#define DPU_INT_FBC_PLD_ERR_MASK		BIT(8)
#define DPU_INT_MMU0_STS_MASK			BIT(14)
#define DPU_INT_MMU1_STS_MASK			BIT(15)
#define DPU_INT_ALL_UPDATE_DONE_MASK		BIT(16)
#define DPU_INT_REG_UPDATE_DONE_MASK		BIT(17)
#define DPU_INT_LAY_REG_UPDATE_DONE_MASK	BIT(18)
#define DPU_INT_PQ_REG_UPDATE_DONE_MASK		BIT(19)
#define DPU_INT_PQ_LUT_UPDATE_DONE_MASK		BIT(20)
#define DPU_INT_HSV_LUT_UPDATE_DONE_MASK	BIT(21)
#define DPU_INT_SLP_LUT_UPDATE_DONE_MASK	BIT(22)
#define DPU_INT_GAMMA_LUT_UPDATE_DONE_MASK	BIT(23)
#define DPU_INT_3D_LUT_UPDATE_DONE_MASK		BIT(24)

#define DISPC_INT_MMU_PAOR_WR_MASK      BIT(7)
#define DISPC_INT_MMU_PAOR_RD_MASK      BIT(6)
#define DISPC_INT_MMU_UNS_WR_MASK       BIT(5)
#define DISPC_INT_MMU_UNS_RD_MASK       BIT(4)
#define DISPC_INT_MMU_INV_WR_MASK       BIT(3)
#define DISPC_INT_MMU_INV_RD_MASK       BIT(2)
#define DISPC_INT_MMU_VAOR_WR_MASK      BIT(1)
#define DISPC_INT_MMU_VAOR_RD_MASK      BIT(0)

#define XFBC8888_HEADER_SIZE(w, h) (ALIGN((ALIGN((w), 16)) * \
				(ALIGN((h), 16)) / 16, 128))
#define XFBC8888_PAYLOAD_SIZE(w, h) (ALIGN((w), 16) * ALIGN((h), 16) * 4)
#define XFBC8888_BUFFER_SIZE(w, h) (XFBC8888_HEADER_SIZE(w, h) \
				+ XFBC8888_PAYLOAD_SIZE(w, h))

#define SLP_BRIGHTNESS_THRESHOLD 0x20

#define DPU_LUTS_SIZE			(29 * 4096)
#define DPU_LUTS_SLP_OFFSET		0
#define DPU_LUTS_GAMMA_OFFSET		4096
#define DPU_LUTS_HSV_OFFSET		(4096 * 3)
#define DPU_LUTS_LUT3D_OFFSET		(4096 * 6)

#define CABC_MODE_UI                    (1 << 2)
#define CABC_MODE_GAME                  (1 << 3)
#define CABC_MODE_VIDEO                 (1 << 4)
#define CABC_MODE_IMAGE                 (1 << 5)
#define CABC_MODE_CAMERA                (1 << 6)
#define CABC_MODE_FULL_FRAME            (1 << 7)

/* DSC_PicW_PicH_SliceW_SliceH  */
#define DSC_1440_2560_720_2560	0
#define DSC_1080_2408_540_8	1
#define DSC_720_2560_720_8	2

static u32 dpu_luts_paddr;
static u8 *dpu_luts_vaddr;
static u32 *lut_slp_vaddr;
static u32 *lut_gamma_vaddr;
static u32 *lut_hsv_vaddr;
static u32 *lut_lut3d_vaddr;

struct layer_reg {
	u32 addr[4];
	u32 ctrl;
	u32 dst_size;
	u32 src_size;
	u32 pitch;
	u32 pos;
	u32 alpha;
	u32 ck;
	u32 pallete;
	u32 crop_start;
	u32 reserved[3];
};

struct dpu_reg {
	u32 dpu_version;
	u32 dpu_mode;
	u32 dpu_ctrl;
	u32 dpu_cfg0;
	u32 dpu_cfg1;
	u32 dpu_secure;
	u32 panel_size;
	u32 blend_size;
	u32 scl_en;
	u32 bg_color;
	u32 reserverd_0x28;
	u32 layer_enable;
	struct layer_reg layers[8];
	u32 wb_base_addr;
	u32 wb_ctrl;
	u32 wb_cfg;
	u32 wb_pitch;
	u32 reserved_0x240_0x24c[4];
	u32 dpu_int_en;
	u32 dpu_int_clr;
	u32 dpu_int_sts;
	u32 dpu_int_raw;
	u32 dpi_ctrl;
	u32 dpi_h_timing;
	u32 dpi_v_timing;
	u32 reserved_0x26c_0x2fc[37];
	u32 scl_coef_hor_cfg[32];
	u32 scl_coef_ver_cfg[32];
	u32 reserved_0x400_0x4ac[44];
	u32 checksum_en;
	u32 checksum0_start_pos;
	u32 checksum0_end_pos;
	u32 checksum1_start_pos;
	u32 checksum1_end_pos;
	u32 checksum0_result;
	u32 checksum1_result;
	u32 reserved_0x4cc;
	u32 corner_config;
	u32 top_corner_lut_addr;
	u32 top_corner_lut_wdata;
	u32 top_corner_lut_rdata;
	u32 bot_corner_lut_addr;
	u32 bot_corner_lut_wdata;
	u32 bot_corner_lut_rdata;
	u32 reserved_0x4ec;
	u32 notch_config;
	u32 notch_lut_addr;
	u32 notch_lut_wdata;
	u32 notch_lut_rdata;
	u32 dpu_enhance_cfg;
	u32 enhance_update;
	u32 enhance_sts;
	u32 reserved_0x50c;
	u32 slp_lut_base_addr;
	u32 threed_lut_base_addr;
	u32 hsv_lut_base_addr;
	u32 gamma_lut_base_addr;
	u32 epf_epsilon;
	u32 epf_gain0_3;
	u32 epf_gain4_7;
	u32 epf_diff;
	u32 cm_coef01_00;
	u32 cm_coef03_02;
	u32 cm_coef11_10;
	u32 cm_coef13_12;
	u32 cm_coef21_20;
	u32 cm_coef23_22;
	u32 reserved_0x548_0x54c[2];
	u32 slp_cfg0;
	u32 slp_cfg1;
	u32 slp_cfg2;
	u32 slp_cfg3;
	u32 slp_cfg4;
	u32 slp_cfg5;
	u32 slp_cfg6;
	u32 slp_cfg7;
	u32 slp_cfg8;
	u32 slp_cfg9;
	u32 slp_cfg10;
	u32 sw_tuning_bth_step;
	u32 hsv_cfg;
	u32 reserved_0x584_0x58c[3];
	u32 cabc_cfg[6];
	u32 reserved_0x5a8_0x5ac[2];
	u32 ud_cfg0;
	u32 ud_cfg1;
	u32 reserved_0x5b8_0x5fc[18];
	u32 cabc_hist[64];
	u32 dpu_sts[24];
	u32 reserved_0x760_0x77c[8];
	u32 gamma_lut_addr;
	u32 gamma_lut_rdata;
	u32 reserved_0x788_0x794[4];
	u32 slp_lut_addr;
	u32 slp_lut_rdata;
	u32 hsv_lut0_addr;
	u32 hsv_lut0_raddr;
	u32 hsv_lut1_addr;
	u32 hsv_lut1_raddr;
	u32 hsv_lut2_addr;
	u32 hsv_lut2_raddr;
	u32 hsv_lut3_addr;
	u32 hsv_lut3_raddr;
	u32 threed_lut0_addr;
	u32 threed_lut0_rdata;
	u32 threed_lut1_addr;
	u32 threed_lut1_rdata;
	u32 threed_lut2_addr;
	u32 threed_lut2_rdata;
	u32 threed_lut3_addr;
	u32 threed_lut3_rdata;
	u32 threed_lut4_addr;
	u32 threed_lut4_rdata;
	u32 threed_lut5_addr;
	u32 threed_lut5_rdata;
	u32 threed_lut6_addr;
	u32 threed_lut6_rdata;
	u32 threed_lut7_addr;
	u32 threed_lut7_rdata;
	u32 reserved_0x800_0x17fc[1024];
	u32 mmu_0x1800_0x1858[23];
	u32 mmu_inv_addr_rd;
	u32 mmu_inv_addr_wr;
	u32 mmu_uns_addr_rd;
	u32 mmu_uns_addr_wr;
	u32 mmu_0x186c_0x189c[13];
	u32 mmu_int_en;
	u32 mmu_int_clr;
	u32 mmu_int_sts;
	u32 mmu_int_raw;
	u32 s_mmu_0x18b0_0x19ac[64];
	u32 reserved_0x19b0_0x19fc[20];
	u32 dsc_ctrl;
	u32 dsc_pic_size;
	u32 dsc_grp_size;
	u32 dsc_slice_size;
	u32 dsc_h_timing;
	u32 dsc_v_timing;
	u32 dsc_cfg0;
	u32 dsc_cfg1;
	u32 dsc_cfg2;
	u32 dsc_cfg3;
	u32 dsc_cfg4;
	u32 dsc_cfg5;
	u32 dsc_cfg6;
	u32 dsc_cfg7;
	u32 dsc_cfg8;
	u32 dsc_cfg9;
	u32 dsc_cfg10;
	u32 dsc_cfg11;
	u32 dsc_cfg12;
	u32 dsc_cfg13;
	u32 dsc_cfg14;
	u32 dsc_cfg15;
	u32 dsc_cfg16;
	u32 dsc_sts0;
	u32 dsc_sts1;
	u32 dsc_version;
	u32 reserved_0x1a68_0x1afc[38];
	u32 dsc1_ctrl;
	u32 dsc1_pic_size;
	u32 dsc1_grp_size;
	u32 dsc1_slice_size;
	u32 dsc1_h_timing;
	u32 dsc1_v_timing;
	u32 dsc1_cfg0;
	u32 dsc1_cfg1;
	u32 dsc1_cfg2;
	u32 dsc1_cfg3;
	u32 dsc1_cfg4;
	u32 dsc1_cfg5;
	u32 dsc1_cfg6;
	u32 dsc1_cfg7;
	u32 dsc1_cfg8;
	u32 dsc1_cfg9;
	u32 dsc1_cfg10;
	u32 dsc1_cfg11;
	u32 dsc1_cfg12;
	u32 dsc1_cfg13;
	u32 dsc1_cfg14;
	u32 dsc1_cfg15;
	u32 dsc1_cfg16;
	u32 dsc1_sts0;
	u32 dsc1_sts1;
	u32 dsc1_version;
};

struct enhance_module {
	u32 epf_en: 1;
	u32 hsv_en: 1;
	u32 cm_en: 1;
	u32 gamma_en: 1;
	u32 lut3d_en: 1;
	u32 dither_en: 1;
	u32 slp_en: 1;
	u32 ltm_en: 1;
	u32 slp_mask_en: 1;
	u32 cabc_en: 1;
	u32 ud_en: 1;
	u32 ud_local_en: 1;
	u32 ud_mask_en: 1;
	u32 scl_en: 1;
};

struct luts_typeindex {
	u16 type;
	u16 index;
};

struct scale_cfg {
	u32 in_w;
	u32 in_h;
};

struct epf_cfg {
	u16 e0;
	u16 e1;
	u8  e2;
	u8  e3;
	u8  e4;
	u8  e5;
	u8  e6;
	u8  e7;
	u8  e8;
	u8  e9;
	u8  e10;
	u8  e11;
};

struct cm_cfg {
	short c00;
	short c01;
	short c02;
	short c03;
	short c10;
	short c11;
	short c12;
	short c13;
	short c20;
	short c21;
	short c22;
	short c23;
};

struct slp_cfg {
	u16 s0;
	u16 s1;
	u8  s2;
	u8  s3;
	u8  s4;
	u8  s5;
	u8  s6;
	u8  s7;
	u8  s8;
	u8  s9;
	u8  s10;
	u8  s11;
	u16 s12;
	u16 s13;
	u8  s14;
	u8  s15;
	u8  s16;
	u8  s17;
	u8  s18;
	u8  s19;
	u8  s20;
	u8  s21;
	u8  s22;
	u8  s23;
	u16 s24;
	u16 s25;
	u8  s26;
	u8  s27;
	u8  s28;
	u8  s29;
	u8  s30;
	u8  s31;
	u8  s32;
	u8  s33;
	u8  s34;
	u8  s35;
	u8  s36;
	u8  s37;
	u8  s38;
};

struct hsv_params {
	short h0;
	short h1;
	short h2;
};

struct ud_cfg {
	short u0;
	short u1;
	short u2;
	short u3;
	short u4;
	short u5;
};

struct hsv_lut_table {
	u16 s_g[64];
	u16 h_o[64];
};

struct hsv_luts {
	struct hsv_lut_table hsv_lut[4];
};

struct hsv_luts_all {
	struct hsv_luts luts_all[3];
};

struct gamma_entry {
	u16 r;
	u16 g;
	u16 b;
};

struct gamma_lut {
	u16 r[256];
	u16 g[256];
	u16 b[256];
};

struct threed_lut {
	uint16_t r[729];
	uint16_t g[729];
	uint16_t b[729];
};

struct lut_3ds {
	struct threed_lut luts[8];
};

struct lut_3ds_all {
	struct lut_3ds luts_all[3];
};

struct dpu_cfg1 {
	u8 arqos_low;
	u8 arqos_high;
	u8 awqos_low;
	u8 awqos_high;
};

static struct dpu_cfg1 qos_cfg = {
	.arqos_low = 0xa,
	.arqos_high = 0xc,
	.awqos_low = 0xa,
	.awqos_high = 0xc,
};

enum {
	CABC_WORKING,
	CABC_STOPPING,
	CABC_DISABLED
};

enum {
	LUTS_GAMMA_TYPE,
	LUTS_HSV_TYPE,
	LUTS_LUT3D_TYPE,
	LUTS_ALL
};
struct lut_base_addrs {
	u32 lut_gamma_addr;
	u32 lut_hsv_addr;
	u32 lut_lut3d_addr;
};

struct cabc_para {
	u32 cabc_hist[64];
	u32 cfg0;
	u32 cfg1;
	u32 cfg2;
	u32 cfg3;
	u32 cfg4;
	u16 bl_fix;
	u16 cur_bl;
	u8 video_mode;
};

struct dpu_dsc_cfg {
	char name[128];
	bool dual_dsi_en;
	bool dsc_en;
	int  dsc_mode;
};

static struct scale_cfg scale_copy;

static struct cm_cfg cm_copy;
static struct slp_cfg slp_copy;
static struct gamma_lut gamma_copy;
static struct threed_lut lut3d_copy;
static struct hsv_luts hsv_copy;
static u32 hsv_cfg_copy;
static struct epf_cfg epf_copy;
static struct ud_cfg ud_copy;
static struct luts_typeindex typeindex_cpy;
struct lut_base_addrs lut_addrs_cpy;

static u32 enhance_en;

static DECLARE_WAIT_QUEUE_HEAD(wait_queue);
static bool panel_ready = true;
static bool need_scale;
static bool mode_changed;
static bool wb_size_changed;
static bool evt_update;
static bool evt_all_update;
static bool evt_stop;
static int frame_no;
static bool cabc_bl_set;
static int cabc_state = CABC_DISABLED;
static int cabc_bl_set_delay;
static struct cabc_para cabc_para;
static struct backlight_device *bl_dev;
static int wb_en;
static int max_vsync_count;
static int vsync_count;
static int secure_debug;
static int time = 5000;
module_param(time, int, 0644);
module_param(secure_debug, int, 0644);
static struct sprd_dpu_layer wb_layer;
static int wb_xfbc_en;
static int corner_radius;
static struct device_node *g_np;
static bool first_frame;
module_param(wb_xfbc_en, int, 0644);
module_param(max_vsync_count, int, 0644);
module_param(cabc_bl_set_delay, int, 0644);
module_param(cabc_state, int, 0644);
module_param(frame_no, int, 0644);

/*
 * FIXME:
 * We don't know what's the best binding to link the panel with dpu dsc.
 * Fow now, we just add all panels that we support dsc, and search them
 */
static struct dpu_dsc_cfg dsc_cfg[] = {
	{
		.name = "lcd_nt35597_boe_mipi_qhd",
		.dual_dsi_en = 0,
		.dsc_en = 1,
		.dsc_mode = 0,
	},
	{
		.name = "lcd_nt57860_boe_mipi_qhd",
		.dual_dsi_en = 1,
		.dsc_en = 1,
		.dsc_mode = 2,
	},
	{
		.name = "lcd_nt36672c_truly_mipi_fhd",
		.dual_dsi_en = 0,
		.dsc_en = 1,
		.dsc_mode = 1,
	},
};

static void dpu_sr_config(struct dpu_context *ctx);
static void dpu_enhance_reload(struct dpu_context *ctx);
static void dpu_clean_all(struct dpu_context *ctx);
static void dpu_layer(struct dpu_context *ctx,
		    struct sprd_dpu_layer *hwlayer);
static int dpu_cabc_trigger(struct dpu_context *ctx);

const char *lcd_name_str;
static int __init lcd_name_get(char *str)
{
	if (str != NULL)
		lcd_name_str = str;
	DRM_INFO("lcd name from uboot: %s\n", lcd_name_str);
	return 0;
}
__setup("lcd_name=", lcd_name_get);

static u32 dpu_get_version(struct dpu_context *ctx)
{
	struct dpu_reg *reg = (struct dpu_reg *)ctx->base;

	return reg->dpu_version;
}

static int dpu_parse_dt(struct dpu_context *ctx,
				struct device_node *np)
{
	int ret = 0;
	struct device_node *qos_np = NULL;

	g_np = np;

	ret = of_property_read_u32(np, "sprd,corner-radius",
					&corner_radius);
	if (!ret)
		pr_info("round corner support, radius = %d.\n",
					corner_radius);

	qos_np = of_parse_phandle(np, "sprd,qos", 0);
	if (!qos_np)
		pr_warn("can't find dpu qos cfg node\n");

	ret = of_property_read_u8(qos_np, "arqos-low",
					&qos_cfg.arqos_low);
	if (ret)
		pr_warn("read arqos-low failed, use default\n");

	ret = of_property_read_u8(qos_np, "arqos-high",
					&qos_cfg.arqos_high);
	if (ret)
		pr_warn("read arqos-high failed, use default\n");

	ret = of_property_read_u8(qos_np, "awqos-low",
					&qos_cfg.awqos_low);
	if (ret)
		pr_warn("read awqos_low failed, use default\n");

	ret = of_property_read_u8(qos_np, "awqos-high",
					&qos_cfg.awqos_high);
	if (ret)
		pr_warn("read awqos-high failed, use default\n");

	return ret;
}

static void dpu_corner_init(struct dpu_context *ctx)
{
	struct dpu_reg *reg = (struct dpu_reg *)ctx->base;
	int i;

	reg->corner_config = (corner_radius << 24) |
				(corner_radius << 8);

	for (i = 0; i < corner_radius; i++) {
		reg->top_corner_lut_addr = i;
		reg->top_corner_lut_wdata = r6p0_corner_param[corner_radius][i];
		reg->bot_corner_lut_addr = i;
		reg->bot_corner_lut_wdata =
			r6p0_corner_param[corner_radius][corner_radius - i - 1];
	}

	reg->corner_config |= (TOP_CORNER_EN | BOT_CORNER_EN);
}

static void dpu_dump(struct dpu_context *ctx)
{
	u32 *reg = (u32 *)ctx->base;
	int i;

	pr_info("      0          4          8          C\n");
	for (i = 0; i < 256; i += 4) {
		pr_info("%04x: 0x%08x 0x%08x 0x%08x 0x%08x\n",
				i * 4, reg[i], reg[i + 1], reg[i + 2], reg[i + 3]);
	}
}

static u32 check_mmu_isr(struct dpu_context *ctx, u32 reg_val)
{
	struct dpu_reg *reg = (struct dpu_reg *)ctx->base;
	u32 mmu_mask = DISPC_INT_MMU_VAOR_RD_MASK |
		DISPC_INT_MMU_VAOR_WR_MASK |
		DISPC_INT_MMU_INV_RD_MASK |
		DISPC_INT_MMU_INV_WR_MASK |
		DISPC_INT_MMU_UNS_WR_MASK |
		DISPC_INT_MMU_UNS_RD_MASK |
		DISPC_INT_MMU_PAOR_WR_MASK |
		DISPC_INT_MMU_PAOR_RD_MASK;
	u32 val = reg_val & mmu_mask;

	if (val) {
		pr_err("--- iommu interrupt err: 0x%04x ---\n", val);

		pr_err("iommu invalid read error, addr: 0x%08x\n",
				reg->mmu_inv_addr_rd);
		pr_err("iommu invalid write error, addr: 0x%08x\n",
				reg->mmu_inv_addr_wr);
		pr_err("iommu unsecurity read error, addr: 0x%08x\n",
				reg->mmu_uns_addr_rd);
		pr_err("iommu unsecurity  write error, addr: 0x%08x\n",
				reg->mmu_uns_addr_wr);

		pr_err("BUG: iommu failure at %s:%d/%s()!\n",
				__FILE__, __LINE__, __func__);

		dpu_dump(ctx);

		/* panic("iommu panic\n"); */
	}

	return val;
}

static u32 dpu_isr(struct dpu_context *ctx)
{
	struct dpu_reg *reg = (struct dpu_reg *)ctx->base;
	u32 reg_val, int_mask = 0;
	u32 mmu_reg_val, mmu_int_mask = 0;

	reg_val = reg->dpu_int_sts;
	reg->dpu_int_clr = reg_val;

	mmu_reg_val = reg->mmu_int_sts;
	/* disable err interrupt */
	if (reg_val & DPU_INT_ERR_MASK)
		int_mask |= DPU_INT_ERR_MASK;

	/* dpu vsync isr */
	if (reg_val & DPU_INT_DPI_VSYNC_MASK) {
		/* write back feature */
		if ((vsync_count == max_vsync_count) && wb_en)
			schedule_work(&ctx->wb_work);

		/* cabc update backlight */
		if (cabc_bl_set)
			schedule_work(&ctx->cabc_bl_update);

		vsync_count++;
	}

	/* dpu update done isr */
	if (reg_val & DPU_INT_LAY_REG_UPDATE_DONE_MASK) {
		/* dpu dvfs feature */
		tasklet_schedule(&ctx->dvfs_task);

		evt_update = true;
		wake_up_interruptible_all(&wait_queue);
	}

	if (reg_val & DPU_INT_ALL_UPDATE_DONE_MASK) {
		evt_all_update = true;
		wake_up_interruptible_all(&wait_queue);
	}

	/* dpu stop done isr */
	if (reg_val & DPU_INT_DONE_MASK) {
		evt_stop = true;
		wake_up_interruptible_all(&wait_queue);
	}

	/* dpu write back done isr */
	if (reg_val & DPU_INT_WB_DONE_MASK) {
		/*
		 * The write back is a time-consuming operation. If there is a
		 * flip occurs before write back done, the write back buffer is
		 * no need to display. Otherwise the new frame will be covered
		 * by the write back buffer, which is not what we wanted.
		 */
		if (wb_en && (vsync_count > max_vsync_count)) {
			wb_en = false;
			schedule_work(&ctx->wb_work);
			/*reg_val |= DPU_INT_FENCE_SIGNAL_REQUEST;*/
		}

		pr_debug("wb done\n");
	}

	/* dpu write back error isr */
	if (reg_val & DPU_INT_WB_FAIL_MASK) {
		pr_err("dpu write back fail\n");
		/*give a new chance to write back*/
		wb_en = true;
		vsync_count = 0;
	}

	/* dpu afbc payload error isr */
	if (reg_val & DPU_INT_FBC_PLD_ERR_MASK) {
		int_mask |= DPU_INT_FBC_PLD_ERR_MASK;
		pr_err("dpu afbc payload error\n");
	}

	/* dpu afbc header error isr */
	if (reg_val & DPU_INT_FBC_HDR_ERR_MASK) {
		int_mask |= DPU_INT_FBC_HDR_ERR_MASK;
		pr_err("dpu afbc header error\n");
	}

	mmu_int_mask |= check_mmu_isr(ctx, mmu_reg_val);

	reg->dpu_int_en &= ~int_mask;

	reg->mmu_int_clr = mmu_reg_val;
	reg->mmu_int_en &= ~mmu_int_mask;

	if (reg_val & DPU_INT_DPI_VSYNC_MASK)
		reg_val |= DISPC_INT_DPI_VSYNC_MASK;

	return reg_val;
}

static int dpu_wait_stop_done(struct dpu_context *ctx)
{
	int rc;

	if (ctx->is_stopped)
		return 0;

	/* wait for stop done interrupt */
	rc = wait_event_interruptible_timeout(wait_queue, evt_stop,
					       msecs_to_jiffies(500));
	evt_stop = false;

	ctx->is_stopped = true;

	if (!rc) {
		/* time out */
		pr_err("dpu wait for stop done time out!\n");
		return -1;
	}

	return 0;
}

static int dpu_wait_update_done(struct dpu_context *ctx)
{
	int rc;

	/* clear the event flag before wait */
	evt_update = false;

	/* wait for reg update done interrupt */
	rc = wait_event_interruptible_timeout(wait_queue, evt_update,
					       msecs_to_jiffies(500));

	if (!rc) {
		/* time out */
		pr_err("dpu wait for reg update done time out!\n");
		return -1;
	}

	return 0;
}

static int dpu_wait_all_update_done(struct dpu_context *ctx)
{
	int rc;

	/* clear the event flag before wait */
	evt_all_update = false;

	/* wait for reg update done interrupt */
	rc = wait_event_interruptible_timeout(wait_queue, evt_all_update,
					       msecs_to_jiffies(500));

	if (!rc) {
		/* time out */
		pr_err("dpu wait for reg update done time out!\n");
		return -1;
	}

	return 0;
}
static void dpu_stop(struct dpu_context *ctx)
{
	struct dpu_reg *reg = (struct dpu_reg *)ctx->base;

	if (ctx->if_type == SPRD_DISPC_IF_DPI)
		reg->dpu_ctrl |= BIT(1);

	dpu_wait_stop_done(ctx);
	pr_info("dpu stop\n");
}

static void dpu_run(struct dpu_context *ctx)
{
	struct dpu_reg *reg = (struct dpu_reg *)ctx->base;

	reg->dpu_ctrl |= BIT(4) | BIT(0);

	ctx->is_stopped = false;

	pr_info("dpu run\n");

	if (ctx->if_type == SPRD_DISPC_IF_EDPI) {
		/*
		 * If the panel read GRAM speed faster than
		 * DSI write GRAM speed, it will display some
		 * mass on screen when backlight on. So wait
		 * a TE period after flush the GRAM.
		 */
		if (!panel_ready) {
			dpu_wait_stop_done(ctx);
			/* wait for TE again */
			mdelay(20);
			panel_ready = true;
		}
	}
}

static void dpu_cabc_work_func(struct work_struct *data)
{
	struct dpu_context *ctx =
		container_of(data, struct dpu_context, cabc_work);
	struct dpu_reg *reg = (struct dpu_reg *)ctx->base;

	down(&ctx->refresh_lock);
	if (ctx->is_inited) {
		dpu_cabc_trigger(ctx);
		reg->enhance_update |= BIT(0);
	}
	up(&ctx->refresh_lock);
}

static void dpu_cabc_bl_update_func(struct work_struct *data)
{
	struct sprd_backlight *bl = bl_get_data(bl_dev);

	msleep(cabc_bl_set_delay);
	if (bl_dev) {
		if (cabc_state == CABC_WORKING) {
			sprd_backlight_normalize_map(bl_dev, &cabc_para.cur_bl);
			bl->cabc_en = true;
			bl->cabc_level = cabc_para.bl_fix *
					cabc_para.cur_bl / 1020;
			bl->cabc_refer_level = cabc_para.cur_bl;
			sprd_cabc_backlight_update(bl_dev);
		} else {
			bl->cabc_en = false;
			backlight_update_status(bl_dev);
		}
	}

	cabc_bl_set = false;
}

static void dpu_wb_trigger(struct dpu_context *ctx, u8 count, bool debug)
{
	struct dpu_reg *reg = (struct dpu_reg *)ctx->base;
	int mode_width  = reg->blend_size & 0xFFFF;
	int mode_height = reg->blend_size >> 16;

	if (wb_size_changed) {
		wb_layer.dst_w = mode_width;
		wb_layer.dst_h = mode_height;
		wb_layer.src_w = mode_width;
		wb_layer.src_h = mode_height;
		wb_layer.pitch[0] = ALIGN(mode_width, 16) * 4;
		wb_layer.header_size_r = XFBC8888_HEADER_SIZE(mode_width,
			mode_height) / 128;
		reg->wb_pitch = ALIGN((mode_width), 16);
		if (wb_xfbc_en)
			reg->wb_cfg = (wb_layer.header_size_r << 16) | BIT(0);
		else
			reg->wb_cfg = 0;
	}

	if (debug) {
		mode_width  = reg->panel_size & 0xFFFF;
		reg->wb_pitch = ALIGN((mode_width), 16);
		reg->wb_cfg = 0;
	}

	if (debug || wb_size_changed) {
		/* update trigger */
		reg->dpu_ctrl |= BIT(4);
		dpu_wait_update_done(ctx);
		wb_size_changed = false;
	}

	if (debug)
		/* writeback debug trigger */
		reg->wb_ctrl = BIT(1);
	else
		reg->wb_ctrl |= BIT(0);

	pr_debug("write back trigger\n");
}

static void dpu_wb_flip(struct dpu_context *ctx)
{
	struct dpu_reg *reg = (struct dpu_reg *)ctx->base;

	dpu_clean_all(ctx);
	dpu_layer(ctx, &wb_layer);
	reg->dpu_ctrl |= BIT(4);

	dpu_wait_update_done(ctx);
	pr_debug("write back flip\n");
}

static void dpu_wb_work_func(struct work_struct *data)
{
	struct dpu_context *ctx =
		container_of(data, struct dpu_context, wb_work);

	down(&ctx->refresh_lock);

	if (!ctx->is_inited) {
		up(&ctx->refresh_lock);
		pr_err("dpu is not initialized\n");
		return;
	}

	if (ctx->disable_flip) {
		up(&ctx->refresh_lock);
		pr_warn("dpu flip is disabled\n");
		return;
	}

	if (wb_en && (vsync_count > max_vsync_count))
		dpu_wb_trigger(ctx, 1, false);
	else if (!wb_en)
		dpu_wb_flip(ctx);

	up(&ctx->refresh_lock);
}

static int dpu_wb_buf_alloc(struct sprd_dpu *dpu, size_t size,
			 u32 *paddr)
{
	struct device_node *node;
	u64 size64;
	struct resource r;

	node = of_parse_phandle(dpu->dev.of_node,
					"sprd,wb-memory", 0);
	if (!node) {
		pr_err("no sprd,wb-memory specified\n");
		return -EINVAL;
	}

	if (of_address_to_resource(node, 0, &r)) {
		pr_err("invalid wb reserved memory node!\n");
		return -EINVAL;
	}

	*paddr = r.start;
	size64 = resource_size(&r);

	if (size64 < size) {
		pr_err("unable to obtain enough wb memory\n");
		return -ENOMEM;
	}

	return 0;
}

static int dpu_write_back_config(struct dpu_context *ctx)
{
	int ret;
	static int need_config = 1;
	size_t wb_buf_size;
	struct sprd_dpu *dpu =
		(struct sprd_dpu *)container_of(ctx, struct sprd_dpu, ctx);
	struct dpu_reg *reg = (struct dpu_reg *)ctx->base;
	int mode_width  = reg->blend_size & 0xFFFF;

	if (!need_config) {
		reg->wb_base_addr = ctx->wb_addr_p;
		reg->wb_pitch = ALIGN((mode_width), 16);
		if (wb_xfbc_en)
			reg->wb_cfg = (wb_layer.header_size_r << 16) | BIT(0);
		else
			reg->wb_cfg = 0;
		pr_debug("write back has configed\n");
		return 0;
	}

	wb_buf_size = XFBC8888_BUFFER_SIZE(dpu->mode->hdisplay,
						dpu->mode->vdisplay);
	pr_info("use wb_reserved memory for writeback, size:0x%zx\n", wb_buf_size);
	ret = dpu_wb_buf_alloc(dpu, wb_buf_size, &ctx->wb_addr_p);
	if (ret) {
		max_vsync_count = 0;
		return -1;
	}

	wb_layer.index = 7;
	wb_layer.planes = 1;
	wb_layer.alpha = 0xff;
	wb_layer.format = DRM_FORMAT_ABGR8888;
	wb_layer.addr[0] = ctx->wb_addr_p;
	reg->wb_base_addr = ctx->wb_addr_p;
	reg->wb_pitch = ALIGN((mode_width), 16);
	if (wb_xfbc_en) {
		wb_layer.xfbc = wb_xfbc_en;
		reg->wb_cfg = (wb_layer.header_size_r << 16) | BIT(0);
	}
	max_vsync_count = 4;
	need_config = 0;

	INIT_WORK(&ctx->wb_work, dpu_wb_work_func);

	return 0;
}

static int dpu_luts_alloc(void)
{
	static bool is_configed;
	u8 *buf;

	if (is_configed) {
		pr_debug("dpu luts buffer has been configed\n");
		return 0;
	}

	 buf = (u8 *)__get_free_pages(GFP_DMA | GFP_ATOMIC,
			get_order(DPU_LUTS_SIZE));
	if (!buf) {
		pr_err("dpu luts alloc failed\n");
		return  -ENOMEM;
	}

	dpu_luts_paddr = virt_to_phys(buf);
	dpu_luts_vaddr = buf;
	lut_slp_vaddr = (u32 *)(dpu_luts_vaddr + DPU_LUTS_SLP_OFFSET);
	lut_gamma_vaddr = (u32 *)(dpu_luts_vaddr + DPU_LUTS_GAMMA_OFFSET);
	lut_hsv_vaddr = (u32 *)(dpu_luts_vaddr + DPU_LUTS_HSV_OFFSET);
	lut_lut3d_vaddr = (u32 *)(dpu_luts_vaddr + DPU_LUTS_LUT3D_OFFSET);

	pr_debug("dpu luts alloc scucess phy %x virt %p\n",
		 dpu_luts_paddr, dpu_luts_vaddr);

	is_configed = true;

	return 0;

}

static void dpu_dvfs_task_func(unsigned long data)
{
	struct dpu_context *ctx = (struct dpu_context *)data;
	struct dpu_reg *reg = (struct dpu_reg *)ctx->base;
	struct sprd_dpu_layer layer, layers[8];
	int i, j, max_x, max_y, min_x, min_y;
	int layer_en, max, maxs[8], count = 0;
	u32 dvfs_freq;

	if (!ctx->is_inited) {
		pr_err("dpu is not initialized\n");
		return;
	}

	/*
	 * Count the current total number of active layers
	 * and the corresponding pos_x, pos_y, size_x and size_y.
	 */
	for (i = 0; i < ARRAY_SIZE(reg->layers); i++) {
		layer_en = reg->layer_enable & BIT(i);
		if (layer_en) {
			layers[count].dst_x = reg->layers[i].pos & 0xffff;
			layers[count].dst_y = reg->layers[i].pos >> 16;
			layers[count].dst_w = reg->layers[i].dst_size & 0xffff;
			layers[count].dst_h = reg->layers[i].dst_size >> 16;
			count++;
		}
	}

	/*
	 * Calculate the number of overlaps between each
	 * layer with other layers, not include itself.
	 */
	for (i = 0; i < count; i++) {
		layer.dst_x = layers[i].dst_x;
		layer.dst_y = layers[i].dst_y;
		layer.dst_w = layers[i].dst_w;
		layer.dst_h = layers[i].dst_h;
		maxs[i] = 1;

		for (j = 0; j < count; j++) {
			if (layer.dst_x + layer.dst_w > layers[j].dst_x &&
				layers[j].dst_x + layers[j].dst_w > layer.dst_x &&
				layer.dst_y + layer.dst_h > layers[j].dst_y &&
				layers[j].dst_y + layers[j].dst_h > layer.dst_y &&
				i != j) {
				max_x = max(layers[i].dst_x, layers[j].dst_x);
				max_y = max(layers[i].dst_y, layers[j].dst_y);
				min_x = min(layers[i].dst_x + layers[i].dst_w,
					layers[j].dst_x + layers[j].dst_w);
				min_y = min(layers[i].dst_y + layers[i].dst_h,
					layers[j].dst_y + layers[j].dst_h);

				layer.dst_x = max_x;
				layer.dst_y = max_y;
				layer.dst_w = min_x - max_x;
				layer.dst_h = min_y - max_y;

				maxs[i]++;
			}
		}
	}

	/* take the maximum number of overlaps */
	max = maxs[0];
	for (i = 1; i < count; i++) {
		if (maxs[i] > max)
			max = maxs[i];
	}

	/*
	 * Determine which frequency to use based on the
	 * maximum number of overlaps.
	 * Every IP here may be different, so need to modify it
	 * according to the actual dpu core clock.
	 */
	if (max <= 2)
		dvfs_freq = 384000000;
	else if (max == 3)
		dvfs_freq = 409600000;
	else if (max == 4)
		dvfs_freq = 51200000;
	else
		dvfs_freq = 614400000;

	dpu_dvfs_notifier_call_chain(&dvfs_freq);
}

static void dpu_dvfs_task_init(struct dpu_context *ctx)
{
	static int need_config = 1;

	if (!need_config)
		return;

	need_config = 0;
	tasklet_init(&ctx->dvfs_task, dpu_dvfs_task_func,
			(unsigned long)ctx);
}

static void dpu_scl_coef_cfg(struct dpu_context *ctx)
{
	struct dpu_reg *reg = (struct dpu_reg *)ctx->base;
	int i, j;

	for (i = 0, j = 0; i < 64; i += 2) {
		reg->scl_coef_hor_cfg[j] = r6p0_scl_coef[i] +
			(r6p0_scl_coef[i+1] << 16);
		reg->scl_coef_ver_cfg[j] = r6p0_scl_coef[i] +
			(r6p0_scl_coef[i+1] << 16);
		j++;
	}
}

/*
 * FIXME:
 * We don't know what's the best binding to link the panel with dpu dsc.
 * Fow now, we just hunt for all panels that we support, and get dsc cfg
 */
static void dpu_get_dsc_cfg(struct dpu_context *ctx)
{
	int index;

	for (index = 0; index < ARRAY_SIZE(dsc_cfg); index++) {
		if (!strcmp(dsc_cfg[index].name, lcd_name_str)) {
			ctx->dual_dsi_en = dsc_cfg[index].dual_dsi_en;
			ctx->dsc_en = dsc_cfg[index].dsc_en;
			ctx->dsc_mode = dsc_cfg[index].dsc_mode;
			return;
		}
	}
	pr_info("no found compatible, use dsc off\n");
}

static int dpu_config_dsc_param(struct dpu_context *ctx)
{
	struct dpu_reg *reg = (struct dpu_reg *)ctx->base;
	struct sprd_dpu *dpu =
		(struct sprd_dpu *)container_of(ctx, struct sprd_dpu, ctx);

	if (ctx->dual_dsi_en)
		reg->dsc_pic_size = (ctx->vm.vactive << 16) |
			((ctx->vm.hactive >> 1)  << 0);
	else
		reg->dsc_pic_size = (ctx->vm.vactive << 16) |
			(ctx->vm.hactive << 0);
	if (ctx->dual_dsi_en)
		reg->dsc_h_timing = ((ctx->vm.hsync_len >> 1) << 0) |
			((ctx->vm.hback_porch  >> 1) << 8) |
			((ctx->vm.hfront_porch >> 1) << 20);
	else
		reg->dsc_h_timing = (ctx->vm.hsync_len << 0) |
			(ctx->vm.hback_porch  << 8) |
			(ctx->vm.hfront_porch << 20);
	reg->dsc_v_timing = (ctx->vm.vsync_len << 0) |
			(ctx->vm.vback_porch  << 8) |
			(ctx->vm.vfront_porch << 20);

	reg->dsc_cfg0 = 0x306c81db;
	reg->dsc_cfg3 = 0x12181800;
	reg->dsc_cfg4 = 0x003316b6;
	reg->dsc_cfg5 = 0x382a1c0e;
	reg->dsc_cfg6 = 0x69625446;
	reg->dsc_cfg7 = 0x7b797770;
	reg->dsc_cfg8 = 0x00007e7d;
	reg->dsc_cfg9 = 0x01000102;
	reg->dsc_cfg10 = 0x09be0940;
	reg->dsc_cfg11 = 0x19fa19fc;
	reg->dsc_cfg12 = 0x1a3819f8;
	reg->dsc_cfg13 = 0x1ab61a78;
	reg->dsc_cfg14 = 0x2b342af6;
	reg->dsc_cfg15 = 0x3b742b74;
	reg->dsc_cfg16 = 0x00006bf4;

	switch (ctx->dsc_mode) {
	case DSC_1440_2560_720_2560:
		reg->dsc_grp_size = 0x000000f0;
		reg->dsc_slice_size = 0x04096000;
		reg->dsc_cfg1 = 0x000ae4bd;
		reg->dsc_cfg2 = 0x0008000a;
		break;
	case DSC_1080_2408_540_8:
		reg->dsc_grp_size = 0x800b4;
		reg->dsc_slice_size = 0x050005a0;
		reg->dsc_cfg1 = 0x7009b;
		reg->dsc_cfg2 = 0xcb70db7;
		break;
	case DSC_720_2560_720_8:
		reg->dsc_grp_size = 0x800f0;
		reg->dsc_slice_size = 0x1000780;
		reg->dsc_cfg1 = 0x000a00b1;
		reg->dsc_cfg2 = 0x9890db7;
		if (ctx->dual_dsi_en) {
			reg->dsc1_pic_size = (ctx->vm.vactive << 16) |
				((ctx->vm.hactive >> 1)  << 0);
			reg->dsc1_h_timing = ((ctx->vm.hsync_len >> 1) << 0) |
				((ctx->vm.hback_porch  >> 1) << 8) |
				((ctx->vm.hfront_porch >> 1) << 20);
			reg->dsc1_v_timing = (ctx->vm.vsync_len << 0) |
				(ctx->vm.vback_porch  << 8) |
				(ctx->vm.vfront_porch << 20);
			reg->dsc1_cfg0 = 0x306c81db;
			reg->dsc1_cfg3 = 0x12181800;
			reg->dsc1_cfg4 = 0x003316b6;
			reg->dsc1_cfg5 = 0x382a1c0e;
			reg->dsc1_cfg6 = 0x69625446;
			reg->dsc1_cfg7 = 0x7b797770;
			reg->dsc1_cfg8 = 0x00007e7d;
			reg->dsc1_cfg9 = 0x01000102;
			reg->dsc1_cfg10 = 0x09be0940;
			reg->dsc1_cfg11 = 0x19fa19fc;
			reg->dsc1_cfg12 = 0x1a3819f8;
			reg->dsc1_cfg13 = 0x1ab61a78;
			reg->dsc1_cfg14 = 0x2b342af6;
			reg->dsc1_cfg15 = 0x3b742b74;
			reg->dsc1_cfg16 = 0x00006bf4;
			reg->dsc1_grp_size = 0x800f0;
			reg->dsc1_slice_size = 0x1000780;
			reg->dsc1_cfg1 = 0x000a00b1;
			reg->dsc1_cfg2 = 0x9890db7;
			if (dpu->dsi->ctx.work_mode == DSI_MODE_CMD)
				reg->dsc1_ctrl = 0x2000010b;
			else
				reg->dsc1_ctrl = 0x2000000b;
		}

		break;
	default:
		reg->dsc_grp_size = 0x000000f0;
		reg->dsc_slice_size = 0x04096000;
		reg->dsc_cfg1 = 0x000ae4bd;
		reg->dsc_cfg2 = 0x0008000a;
		break;
	}

	if (dpu->dsi->ctx.work_mode == DSI_MODE_CMD)
		reg->dsc_ctrl = 0x2000010b;
	else
		reg->dsc_ctrl = 0x2000000b;

	return 0;
}

static int dpu_init(struct dpu_context *ctx)
{
	struct dpu_reg *reg = (struct dpu_reg *)ctx->base;
	static bool tos_msg_alloc = false;
	u32 size;
	int ret;
	struct sprd_dpu *dpu =
		(struct sprd_dpu *)container_of(ctx, struct sprd_dpu, ctx);

	dpu_get_dsc_cfg(ctx);

	if (ctx->dual_dsi_en)
		reg->dpu_mode = BIT(0);

	if (ctx->dsc_en)
		dpu_config_dsc_param(ctx);

	/* set bg color */
	reg->bg_color = 0;

	/* set dpu output size */
	size = (ctx->vm.vactive << 16) | ctx->vm.hactive;
	reg->panel_size = size;
	reg->blend_size = size;

	reg->dpu_cfg0 = 0;
	if ((dpu->dsi->ctx.work_mode == DSI_MODE_CMD) && ctx->dsc_en) {
		reg->dpu_cfg0 |= BIT(1);
		ctx->is_single_run = true;
	}

	reg->dpu_cfg1 = (qos_cfg.awqos_high << 12) |
		(qos_cfg.awqos_low << 8) |
		(qos_cfg.arqos_high << 4) |
		(qos_cfg.arqos_low) | BIT(18) | BIT(22) | BIT(23);

	if (ctx->is_stopped)
		dpu_clean_all(ctx);

	dpu_scl_coef_cfg(ctx);

	reg->dpu_int_clr = 0xffff;

	ret = dpu_luts_alloc();
	if (ret)
		pr_err("DPU lUTS table alloc buffer fail\n");

	dpu_enhance_reload(ctx);

	dpu_write_back_config(ctx);

	if (corner_radius)
		dpu_corner_init(ctx);

	dpu_dvfs_task_init(ctx);
	INIT_WORK(&ctx->cabc_work, dpu_cabc_work_func);
	INIT_WORK(&ctx->cabc_bl_update, dpu_cabc_bl_update_func);

	frame_no = 0;

	ctx->base_offset[0] = 0x0;
	ctx->base_offset[1] = sizeof(struct dpu_reg) / 4;

	/* Allocate memory for trusty */
	if(!tos_msg_alloc){
		ctx->tos_msg = kmalloc(sizeof(struct disp_message) +
			sizeof(struct layer_reg), GFP_KERNEL);
		if(!ctx->tos_msg)
			return -ENOMEM;
		tos_msg_alloc = true;
	}

	return 0;
}

static void dpu_uninit(struct dpu_context *ctx)
{
	struct dpu_reg *reg = (struct dpu_reg *)ctx->base;

	reg->dpu_int_en = 0;
	reg->dpu_int_clr = 0xff;

	panel_ready = false;
}

enum {
	DPU_LAYER_FORMAT_YUV422_2PLANE,
	DPU_LAYER_FORMAT_YUV420_2PLANE,
	DPU_LAYER_FORMAT_YUV420_3PLANE,
	DPU_LAYER_FORMAT_ARGB8888,
	DPU_LAYER_FORMAT_RGB565,
	DPU_LAYER_FORMAT_XFBC_ARGB8888 = 8,
	DPU_LAYER_FORMAT_XFBC_RGB565,
	DPU_LAYER_FORMAT_XFBC_YUV420,
	DPU_LAYER_FORMAT_MAX_TYPES,
};

enum {
	DPU_LAYER_ROTATION_0,
	DPU_LAYER_ROTATION_90,
	DPU_LAYER_ROTATION_180,
	DPU_LAYER_ROTATION_270,
	DPU_LAYER_ROTATION_0_M,
	DPU_LAYER_ROTATION_90_M,
	DPU_LAYER_ROTATION_180_M,
	DPU_LAYER_ROTATION_270_M,
};

static u32 to_dpu_rotation(u32 angle)
{
	u32 rot = DPU_LAYER_ROTATION_0;

	switch (angle) {
	case 0:
	case DRM_MODE_ROTATE_0:
		rot = DPU_LAYER_ROTATION_0;
		break;
	case DRM_MODE_ROTATE_90:
		rot = DPU_LAYER_ROTATION_90;
		break;
	case DRM_MODE_ROTATE_180:
		rot = DPU_LAYER_ROTATION_180;
		break;
	case DRM_MODE_ROTATE_270:
		rot = DPU_LAYER_ROTATION_270;
		break;
	case DRM_MODE_REFLECT_Y:
		rot = DPU_LAYER_ROTATION_180_M;
		break;
	case (DRM_MODE_REFLECT_Y | DRM_MODE_ROTATE_90):
		rot = DPU_LAYER_ROTATION_90_M;
		break;
	case DRM_MODE_REFLECT_X:
		rot = DPU_LAYER_ROTATION_0_M;
		break;
	case (DRM_MODE_REFLECT_X | DRM_MODE_ROTATE_90):
		rot = DPU_LAYER_ROTATION_270_M;
		break;
	default:
		pr_err("rotation convert unsupport angle (drm)= 0x%x\n", angle);
		break;
	}

	return rot;
}

static u32 dpu_img_ctrl(u32 format, u32 blending, u32 compression, u32 y2r_coef,
		u32 rotation)
{
	int reg_val = 0;

	switch (format) {
	case DRM_FORMAT_BGRA8888:
		/* BGRA8888 -> ARGB8888 */
		reg_val |= SPRD_IMG_DATA_ENDIAN_B3B2B1B0 << 8;
		if (compression)
			/* XFBC-ARGB8888 */
			reg_val |= (DPU_LAYER_FORMAT_XFBC_ARGB8888 << 4);
		else
			reg_val |= (DPU_LAYER_FORMAT_ARGB8888 << 4);
		break;
	case DRM_FORMAT_RGBX8888:
	case DRM_FORMAT_RGBA8888:
		/* RGBA8888 -> ABGR8888 */
		reg_val |= SPRD_IMG_DATA_ENDIAN_B3B2B1B0 << 8;
	case DRM_FORMAT_ABGR8888:
		/* rb switch */
		reg_val |= BIT(10);
	case DRM_FORMAT_ARGB8888:
		if (compression)
			/* XFBC-ARGB8888 */
			reg_val |= (DPU_LAYER_FORMAT_XFBC_ARGB8888 << 4);
		else
			reg_val |= (DPU_LAYER_FORMAT_ARGB8888 << 4);
		break;
	case DRM_FORMAT_XBGR8888:
		/* rb switch */
		reg_val |= BIT(10);
	case DRM_FORMAT_XRGB8888:
		if (compression)
			/* XFBC-ARGB8888 */
			reg_val |= (DPU_LAYER_FORMAT_XFBC_ARGB8888 << 4);
		else
			reg_val |= (DPU_LAYER_FORMAT_ARGB8888 << 4);
		break;
	case DRM_FORMAT_BGR565:
		/* rb switch */
		reg_val |= BIT(12);
	case DRM_FORMAT_RGB565:
		if (compression)
			/* XFBC-RGB565 */
			reg_val |= (DPU_LAYER_FORMAT_XFBC_RGB565 << 4);
		else
			reg_val |= (DPU_LAYER_FORMAT_RGB565 << 4);
		break;
	case DRM_FORMAT_NV12:
		if (compression)
			/*2-Lane: Yuv420 */
			reg_val |= DPU_LAYER_FORMAT_XFBC_YUV420 << 4;
		else
			reg_val |= DPU_LAYER_FORMAT_YUV420_2PLANE << 4;
		/*Y endian */
		reg_val |= SPRD_IMG_DATA_ENDIAN_B0B1B2B3 << 8;
		/*UV endian */
		reg_val |= SPRD_IMG_DATA_ENDIAN_B0B1B2B3 << 10;
		break;
	case DRM_FORMAT_NV21:
		if (compression)
			/*2-Lane: Yuv420 */
			reg_val |= DPU_LAYER_FORMAT_XFBC_YUV420 << 4;
		else
			reg_val |= DPU_LAYER_FORMAT_YUV420_2PLANE << 4;
		/*Y endian */
		reg_val |= SPRD_IMG_DATA_ENDIAN_B0B1B2B3 << 8;
		/*UV endian */
		reg_val |= SPRD_IMG_DATA_ENDIAN_B3B2B1B0 << 10;
		break;
	case DRM_FORMAT_NV16:
		/*2-Lane: Yuv422 */
		reg_val |= DPU_LAYER_FORMAT_YUV422_2PLANE << 4;
		/*Y endian */
		reg_val |= SPRD_IMG_DATA_ENDIAN_B3B2B1B0 << 8;
		/*UV endian */
		reg_val |= SPRD_IMG_DATA_ENDIAN_B3B2B1B0 << 10;
		break;
	case DRM_FORMAT_NV61:
		/*2-Lane: Yuv422 */
		reg_val |= DPU_LAYER_FORMAT_YUV422_2PLANE << 4;
		/*Y endian */
		reg_val |= SPRD_IMG_DATA_ENDIAN_B0B1B2B3 << 8;
		/*UV endian */
		reg_val |= SPRD_IMG_DATA_ENDIAN_B0B1B2B3 << 10;
		break;
	case DRM_FORMAT_YUV420:
		reg_val |= DPU_LAYER_FORMAT_YUV420_3PLANE << 4;
		/*Y endian */
		reg_val |= SPRD_IMG_DATA_ENDIAN_B0B1B2B3 << 8;
		/*UV endian */
		reg_val |= SPRD_IMG_DATA_ENDIAN_B0B1B2B3 << 10;
		break;
	default:
		pr_err("error: invalid format %c%c%c%c\n", format,
						format >> 8,
						format >> 16,
						format >> 24);
		break;
	}

	switch (blending) {
	case DRM_MODE_BLEND_PIXEL_NONE:
		/* don't do blending, maybe RGBX */
		/* alpha mode select - layer alpha */
		reg_val |= BIT(2);
		break;
	case DRM_MODE_BLEND_COVERAGE:
		/* alpha mode select - combo alpha */
		reg_val |= BIT(3);
		/* blending mode select - normal mode */
		reg_val &= (~BIT(16));
		break;
	case DRM_MODE_BLEND_PREMULTI:
		/* alpha mode select - combo alpha */
		reg_val |= BIT(3);
		/* blending mode select - pre-mult mode */
		reg_val |= BIT(16);
		break;
	default:
		/* alpha mode select - layer alpha */
		reg_val |= BIT(2);
		break;
	}

	reg_val |= y2r_coef << 28;
	rotation = to_dpu_rotation(rotation);
	reg_val |= (rotation & 0x7) << 20;

	return reg_val;
}

static void dpu_clean_all(struct dpu_context *ctx)
{
	struct dpu_reg *reg = (struct dpu_reg *)ctx->base;

	reg->layer_enable = 0;
}

static void dpu_bgcolor(struct dpu_context *ctx, u32 color)
{
	struct dpu_reg *reg = (struct dpu_reg *)ctx->base;

	if (ctx->if_type == SPRD_DISPC_IF_EDPI)
		dpu_wait_stop_done(ctx);

	reg->bg_color = color;

	dpu_clean_all(ctx);

	if (ctx->is_single_run) {
		reg->dpu_ctrl |= BIT(4);
		reg->dpu_ctrl |= BIT(0);
	} else if (ctx->if_type == SPRD_DISPC_IF_EDPI) {
		reg->dpu_ctrl |= BIT(0);
		ctx->is_stopped = false;
	} else if (ctx->if_type == SPRD_DISPC_IF_DPI) {
		if (!ctx->is_stopped) {
			reg->dpu_ctrl |= BIT(4);
			dpu_wait_update_done(ctx);
		}
	}
}

static void dpu_dma_request(struct dpu_context *ctx)
{
	struct dpu_reg *reg = (struct dpu_reg *)ctx->base;

	reg->cabc_cfg[5] = 1;
}

static void dpu_layer(struct dpu_context *ctx,
		    struct sprd_dpu_layer *hwlayer)
{
	struct dpu_reg *reg = (struct dpu_reg *)ctx->base;
	struct layer_reg *layer;
	struct layer_reg tmp = {};
	u32 dst_size, src_size, offset, wd, rot;
	int i;

	layer = &reg->layers[hwlayer->index];
	offset = (hwlayer->dst_x & 0xffff) | ((hwlayer->dst_y) << 16);
	src_size = (hwlayer->src_w & 0xffff) | ((hwlayer->src_h) << 16);
	dst_size = (hwlayer->dst_w & 0xffff) | ((hwlayer->dst_h) << 16);
	layer->ctrl = 0;

	if (hwlayer->pallete_en) {
		tmp.pos = offset;
		tmp.src_size = src_size;
		tmp.dst_size = dst_size;
		tmp.alpha = hwlayer->alpha;
		tmp.pallete = hwlayer->pallete_color;

		/* pallete layer enable */
		tmp.ctrl = 0x2005;
		//reg->layer_enable |= (1 << hwlayer->index);
		pr_debug("dst_x = %d, dst_y = %d, dst_w = %d, dst_h = %d, pallete:%d\n",
				hwlayer->dst_x, hwlayer->dst_y,
				hwlayer->dst_w, hwlayer->dst_h, tmp.pallete);
	} else {
		if (src_size != dst_size) {
			rot = to_dpu_rotation(hwlayer->rotation);
			if ((rot == DPU_LAYER_ROTATION_90) || (rot == DPU_LAYER_ROTATION_270) ||
					(rot == DPU_LAYER_ROTATION_90_M) || (rot == DPU_LAYER_ROTATION_270_M))
				dst_size = (hwlayer->dst_h & 0xffff) | ((hwlayer->dst_w) << 16);
			tmp.ctrl = BIT(24);
		}

		for (i = 0; i < hwlayer->planes; i++) {
			if (hwlayer->addr[i] % 16)
				pr_err("layer addr[%d] is not 16 bytes align, it's 0x%08x\n",
						i, hwlayer->addr[i]);
			tmp.addr[i] = hwlayer->addr[i];
		}

		tmp.pos = offset;
		tmp.src_size = src_size;
		tmp.dst_size = dst_size;
		tmp.crop_start = (hwlayer->src_y << 16) | hwlayer->src_x;
		tmp.alpha = hwlayer->alpha;

		wd = drm_format_plane_cpp(hwlayer->format, 0);
		if (wd == 0) {
			pr_err("layer[%d] bytes per pixel is invalid\n", hwlayer->index);
			return;
		}

		if (hwlayer->planes == 3)
			/* UV pitch is 1/2 of Y pitch*/
			tmp.pitch = (hwlayer->pitch[0] / wd) |
				(hwlayer->pitch[0] / wd << 15);
		else
			tmp.pitch = hwlayer->pitch[0] / wd;

		tmp.ctrl |= dpu_img_ctrl(hwlayer->format, hwlayer->blending,
				hwlayer->xfbc, hwlayer->y2r_coef, hwlayer->rotation);
	}

	if (hwlayer->secure_en || secure_debug) {
		if (!reg->dpu_secure) {
			disp_ca_connect();
			udelay(time);
		}
		ctx->tos_msg->cmd = TA_FIREWALL_SET;
		ctx->tos_msg->version = DPU_R6P0;
		disp_ca_write(ctx->tos_msg, sizeof(*ctx->tos_msg));
		disp_ca_wait_response();

		memcpy(ctx->tos_msg + 1, &tmp, sizeof(tmp));

		ctx->tos_msg->cmd = TA_REG_SET;
		ctx->tos_msg->version = DPU_R6P0;
		disp_ca_write(ctx->tos_msg, sizeof(*ctx->tos_msg) + sizeof(tmp));
		disp_ca_wait_response();

		return;
	} else if (reg->dpu_secure) {
		ctx->tos_msg->cmd = TA_REG_CLR;
		ctx->tos_msg->version = DPU_R6P0;
		disp_ca_write(ctx->tos_msg, sizeof(*ctx->tos_msg));
		disp_ca_wait_response();

		ctx->tos_msg->cmd = TA_FIREWALL_CLR;
		disp_ca_write(ctx->tos_msg, sizeof(*ctx->tos_msg));
		disp_ca_wait_response();
	}

	layer = &reg->layers[hwlayer->index];
	for (i = 0; i < 4; i++)
		layer->addr[i] = tmp.addr[i];
	layer->pos = tmp.pos;
	layer->src_size = tmp.src_size;
	layer->dst_size = tmp.dst_size;
	layer->crop_start = tmp.crop_start;
	layer->alpha = tmp.alpha;
	layer->pitch = tmp.pitch;
	layer->ctrl = tmp.ctrl;

	pr_debug("dst_x = %d, dst_y = %d, dst_w = %d, dst_h = %d\n",
			hwlayer->dst_x, hwlayer->dst_y,
			hwlayer->dst_w, hwlayer->dst_h);
	pr_debug("start_x = %d, start_y = %d, start_w = %d, start_h = %d\n",
			hwlayer->src_x, hwlayer->src_y,
			hwlayer->src_w, hwlayer->src_h);

	reg->layer_enable |= (1 << hwlayer->index);
}

static void dpu_scaling(struct dpu_context *ctx,
			struct sprd_dpu_layer layers[], u8 count)
{
	int i;
	struct sprd_dpu_layer *top_layer;

	if (mode_changed) {
		top_layer = &layers[count - 1];
		pr_debug("------------------------------------\n");
		for (i = 0; i < count; i++) {
			pr_debug("layer[%d] : %dx%d --- (%d)\n", i,
				layers[i].dst_w, layers[i].dst_h,
				scale_copy.in_w);
		}

		if  (top_layer->dst_w <= scale_copy.in_w) {
			dpu_sr_config(ctx);
			mode_changed = false;

			pr_info("do scaling enhace: 0x%x, top layer(%dx%d)\n",
				enhance_en, top_layer->dst_w,
				top_layer->dst_h);
		}
	}
}

static void dpu_flip(struct dpu_context *ctx,
		     struct sprd_dpu_layer layers[], u8 count)
{
	struct dpu_reg *reg = (struct dpu_reg *)ctx->base;
	int i;

	vsync_count = 0;
	if (max_vsync_count > 0 && count > 1)
		wb_en = true;
	/*
	 * Make sure the dpu is in stop status. DPU_r6p0 has no shadow
	 * registers in EDPI mode. So the config registers can only be
	 * updated in the rising edge of DPU_RUN bit.
	 */
	if (ctx->if_type == SPRD_DISPC_IF_EDPI)
		dpu_wait_stop_done(ctx);

	/* reset the bgcolor to black */
	reg->bg_color = 0;

	/* disable all the layers */
	dpu_clean_all(ctx);

	/* to check if dpu need scaling the frame for SR */
	dpu_scaling(ctx, layers, count);

	/* start configure dpu layers */
	for (i = 0; i < count; i++)
		dpu_layer(ctx, &layers[i]);

	/* update trigger and wait */
	if (ctx->is_single_run) {
		reg->dpu_ctrl |= BIT(4);
		reg->dpu_ctrl |= BIT(0);
	} else if (ctx->if_type == SPRD_DISPC_IF_EDPI) {
		reg->dpu_ctrl |= BIT(0);

		ctx->is_stopped = false;
	} else if (ctx->if_type == SPRD_DISPC_IF_DPI) {
		if (!ctx->is_stopped) {
			if (first_frame == true) {
				reg->dpu_ctrl |= BIT(2);
				dpu_wait_all_update_done(ctx);
				first_frame = false;
			} else {
				reg->dpu_ctrl |= BIT(4);
				dpu_wait_update_done(ctx);
			}
		}

		reg->dpu_int_en |= DPU_INT_ERR_MASK;
	}

	/*
	 * If the following interrupt was disabled in isr,
	 * re-enable it.
	 */
	reg->dpu_int_en |= DPU_INT_FBC_PLD_ERR_MASK |
			   DPU_INT_FBC_HDR_ERR_MASK;
}


static void dpu_epf_set(struct dpu_reg *reg, struct epf_cfg *epf)
{
	reg->epf_epsilon = (epf->e1 << 16) | epf->e0;
	reg->epf_gain0_3 = (epf->e5 << 24) | (epf->e4 << 16) |
				(epf->e3 << 8) | epf->e2;
	reg->epf_gain4_7 = (epf->e9 << 24) | (epf->e8 << 16) |
				(epf->e7 << 8) | epf->e6;
	reg->epf_diff = (epf->e11 << 8) | epf->e10;
}


static void dpu_dpi_init(struct dpu_context *ctx)
{
	struct dpu_reg *reg = (struct dpu_reg *)ctx->base;
	u32 int_mask = 0;

	if (ctx->if_type == SPRD_DISPC_IF_DPI) {
		/* use dpi as interface */
		reg->dpu_cfg0 &= ~BIT(0);

		/* enable Halt function for SPRD DSI */
		reg->dpi_ctrl |= BIT(16);

		if (ctx->is_single_run)
			reg->dpi_ctrl |= BIT(0);

		/* set dpi timing */
		reg->dpi_h_timing = (ctx->vm.hsync_len << 0) |
				    (ctx->vm.hback_porch << 8) |
				    (ctx->vm.hfront_porch << 20);
		reg->dpi_v_timing = (ctx->vm.vsync_len << 0) |
				    (ctx->vm.vback_porch << 8) |
				    (ctx->vm.vfront_porch << 20);
		if (ctx->vm.vsync_len + ctx->vm.vback_porch < 32)
			pr_warn("Warning: (vsync + vbp) < 32, "
				"underflow risk!\n");

		/* enable dpu update done INT */
		int_mask |= DPU_INT_ALL_UPDATE_DONE_MASK;
		int_mask |= DPU_INT_REG_UPDATE_DONE_MASK;
		int_mask |= DPU_INT_LAY_REG_UPDATE_DONE_MASK;
		int_mask |= DPU_INT_PQ_REG_UPDATE_DONE_MASK;
		/* enable dpu DONE  INT */
		int_mask |= DPU_INT_DONE_MASK;
		/* enable dpu dpi vsync */
		int_mask |= DPU_INT_DPI_VSYNC_MASK;
		/* enable dpu TE INT */
		int_mask |= DPU_INT_TE_MASK;
		/* enable underflow err INT */
		int_mask |= DPU_INT_ERR_MASK;
		/* enable write back done INT */
		int_mask |= DPU_INT_WB_DONE_MASK;
		/* enable write back fail INT */
		int_mask |= DPU_INT_WB_FAIL_MASK;

	} else if (ctx->if_type == SPRD_DISPC_IF_EDPI) {
		/* use edpi as interface */
		reg->dpu_cfg0 |= BIT(0);

		/* use external te */
		reg->dpi_ctrl |= BIT(10);

		/* enable te */
		reg->dpi_ctrl |= BIT(8);

		/* enable stop DONE INT */
		int_mask |= DPU_INT_DONE_MASK;
		/* enable TE INT */
		int_mask |= DPU_INT_TE_MASK;
	}

	/* enable ifbc payload error INT */
	int_mask |= DPU_INT_FBC_PLD_ERR_MASK;
	/* enable ifbc header error INT */
	int_mask |= DPU_INT_FBC_HDR_ERR_MASK;

	reg->dpu_int_en = int_mask;
}

static void enable_vsync(struct dpu_context *ctx)
{
	struct dpu_reg *reg = (struct dpu_reg *)ctx->base;

	reg->dpu_int_en |= DPU_INT_DPI_VSYNC_MASK;
}

static void disable_vsync(struct dpu_context *ctx)
{
	//struct dpu_reg *reg = (struct dpu_reg *)ctx->base;

	//reg->dpu_int_en &= ~DPU_INT_DPI_VSYNC_MASK;
}


static void dpu_luts_copyfrom_user(u32 *param)
{
	static u32 i;
	u8 *luts_tmp_vaddr;

	/* 58 means 58*2k, all luts size.
	 * each time writes 2048 bytes from
	 * user space.
	 */
	if (i == 58)
		i = 0;

	luts_tmp_vaddr = (u8 *)lut_gamma_vaddr;
	luts_tmp_vaddr += 2048 * i;
	memcpy(luts_tmp_vaddr, param, 2048);

	i++;
}

static void dpu_luts_backup(void *param)
{
	u32 *p32 = param;
	struct hsv_params *hsv_cfg;
	int ret = 0;

	memcpy(&typeindex_cpy, param, sizeof(typeindex_cpy));

	switch (typeindex_cpy.type) {
	case LUTS_GAMMA_TYPE:
		lut_addrs_cpy.lut_gamma_addr = dpu_luts_paddr +
			DPU_LUTS_GAMMA_OFFSET + typeindex_cpy.index * 4096;
		enhance_en |= BIT(3) | BIT(5);
		break;
	case LUTS_HSV_TYPE:
		p32++;
		hsv_cfg = (struct hsv_params *) p32;
		hsv_cfg_copy = (hsv_cfg->h0 & 0xff) |
			((hsv_cfg->h1 & 0x3) << 8) |
			((hsv_cfg->h2 & 0x3) << 12);
		lut_addrs_cpy.lut_hsv_addr = dpu_luts_paddr +
			DPU_LUTS_HSV_OFFSET + typeindex_cpy.index * 4096;
		enhance_en |= BIT(1);
		break;
	case LUTS_LUT3D_TYPE:
		lut_addrs_cpy.lut_lut3d_addr = dpu_luts_paddr +
			DPU_LUTS_LUT3D_OFFSET + typeindex_cpy.index * 4096 * 6;
		enhance_en |= BIT(4);
		break;
	case LUTS_ALL:
		ret = dpu_luts_alloc();
		if (ret) {
			pr_err("DPU lUTS table alloc buffer fail\n");
			break;
		}
		p32++;
		dpu_luts_copyfrom_user(p32);
		break;
	default:
		pr_err("The type %d is unavaiable\n", typeindex_cpy.type);
		break;
	}
}

static void dpu_enhance_backup(u32 id, void *param)
{
	u32 *p;

	switch (id) {
	case ENHANCE_CFG_ID_ENABLE:
		p = param;
		enhance_en |= *p;
		pr_info("enhance module enable backup: 0x%x\n", *p);
		break;
	case ENHANCE_CFG_ID_DISABLE:
		p = param;
		enhance_en &= ~(*p);
		pr_info("enhance module disable backup: 0x%x\n", *p);
		break;
	case ENHANCE_CFG_ID_SCL:
		memcpy(&scale_copy, param, sizeof(scale_copy));
		enhance_en |= BIT(13);
		pr_info("enhance scaling backup\n");
		break;
	case ENHANCE_CFG_ID_HSV:
		memcpy(&hsv_copy, param, sizeof(hsv_copy));
		enhance_en |= BIT(1);
		pr_info("enhance hsv backup\n");
		break;
	case ENHANCE_CFG_ID_CM:
		memcpy(&cm_copy, param, sizeof(cm_copy));
		enhance_en |= BIT(2);
		pr_info("enhance cm backup\n");
		break;
	case ENHANCE_CFG_ID_LTM:
	case ENHANCE_CFG_ID_SLP:
		memcpy(&slp_copy, param, sizeof(slp_copy));
		if (!cabc_state) {
			slp_copy.s37 = 0;
			slp_copy.s38 = 255;
		}
		enhance_en |= BIT(6) | BIT(7);
		pr_info("enhance slp backup\n");
		break;
	case ENHANCE_CFG_ID_GAMMA:
		memcpy(&gamma_copy, param, sizeof(gamma_copy));
		enhance_en |= BIT(3) | BIT(5);
		pr_info("enhance gamma backup\n");
		break;
	case ENHANCE_CFG_ID_EPF:
		memcpy(&epf_copy, param, sizeof(epf_copy));
		enhance_en |= BIT(0);
		pr_info("enhance epf backup\n");
		break;
	case ENHANCE_CFG_ID_LUT3D:
		memcpy(&lut3d_copy, param, sizeof(lut3d_copy));
		enhance_en |= BIT(4);
		pr_info("enhance lut3d backup\n");
		break;
	case ENHANCE_CFG_ID_CABC_STATE:
		p = param;
		cabc_state = *p;
		return;
	case ENHANCE_CFG_ID_UPDATE_LUTS:
		dpu_luts_backup(param);
		pr_info("enhance ddr luts backup\n");
	default:
		break;
	}
}

static void dpu_luts_update(struct dpu_context *ctx, void *param)
{
	struct dpu_reg *reg = (struct dpu_reg *)ctx->base;
	struct hsv_params *hsv_cfg;
	u32 *p32 = param;

	memcpy(&typeindex_cpy, param, sizeof(typeindex_cpy));

	switch (typeindex_cpy.type) {
	case LUTS_GAMMA_TYPE:
		reg->gamma_lut_base_addr = dpu_luts_paddr +
			DPU_LUTS_GAMMA_OFFSET + typeindex_cpy.index * 4096;
		lut_addrs_cpy.lut_gamma_addr = reg->gamma_lut_base_addr;
		reg->dpu_enhance_cfg |= BIT(3) | BIT(5);
		break;
	case LUTS_HSV_TYPE:
		p32++;
		hsv_cfg = (struct hsv_params *) p32;
		reg->hsv_cfg = (hsv_cfg->h0 & 0xff) |
			((hsv_cfg->h1 & 0x3) << 8) |
			((hsv_cfg->h2 & 0x3) << 12);
		hsv_cfg_copy = reg->hsv_cfg;
		reg->hsv_lut_base_addr = dpu_luts_paddr +
			DPU_LUTS_HSV_OFFSET + typeindex_cpy.index * 4096;
		lut_addrs_cpy.lut_hsv_addr = reg->hsv_lut_base_addr;
		reg->dpu_enhance_cfg |= BIT(1);
		break;
	case LUTS_LUT3D_TYPE:
		reg->threed_lut_base_addr = dpu_luts_paddr +
			DPU_LUTS_LUT3D_OFFSET + typeindex_cpy.index * 4096 * 6;
		lut_addrs_cpy.lut_lut3d_addr = reg->threed_lut_base_addr;
		reg->dpu_enhance_cfg |= BIT(4);
		break;
	case LUTS_ALL:
		p32++;
		dpu_luts_copyfrom_user(p32);
		break;
	default:
		pr_err("The type %d is unavaiable\n", typeindex_cpy.type);
		break;
	}
}

static void dpu_enhance_set(struct dpu_context *ctx, u32 id, void *param)
{
	struct dpu_reg *reg = (struct dpu_reg *)ctx->base;
	struct scale_cfg *scale;
	struct cm_cfg cm;
	struct slp_cfg *slp;
	struct gamma_lut *gamma;
	struct threed_lut *lut3d;
	struct hsv_luts *hsv_table;
	struct epf_cfg *epf;
	struct ud_cfg *ud;
	struct cabc_para cabc_param;
	u32 *p32, *tmp32;
	int i, j;

	if (!ctx->is_inited) {
		dpu_enhance_backup(id, param);
		return;
	}

	if (ctx->if_type == SPRD_DISPC_IF_EDPI)
		dpu_wait_stop_done(ctx);

	switch (id) {
	case ENHANCE_CFG_ID_ENABLE:
		p32 = param;
		reg->dpu_enhance_cfg |= *p32;
		pr_info("enhance module enable: 0x%x\n", *p32);
		break;
	case ENHANCE_CFG_ID_DISABLE:
		p32 = param;
		reg->dpu_enhance_cfg &= ~(*p32);
		pr_info("enhance module disable: 0x%x\n", *p32);
		break;
	case ENHANCE_CFG_ID_SCL:
		memcpy(&scale_copy, param, sizeof(scale_copy));
		scale = &scale_copy;
		reg->blend_size = (scale->in_h << 16) | scale->in_w;
		reg->dpu_enhance_cfg |= BIT(13);
		reg->scl_en = BIT(0);
		pr_info("enhance scaling: %ux%u\n", scale->in_w, scale->in_h);
		break;
	case ENHANCE_CFG_ID_HSV:
		memcpy(&hsv_copy, param, sizeof(hsv_copy));
		hsv_table = &hsv_copy;
		p32 = lut_hsv_vaddr;
		for (i = 0; i < 4; i++) {
			for (j = 0; j < 64; j++) {
				*p32 = (hsv_table->hsv_lut[i].h_o[j] << 9) |
					hsv_table->hsv_lut[i].s_g[j];
				p32 += 4;
			}
			p32 = lut_hsv_vaddr;
			p32 += i * 4;
		}

		reg->dpu_enhance_cfg |= BIT(1);
		pr_info("enhance hsv set\n");
		break;
	case ENHANCE_CFG_ID_CM:
		memcpy(&cm_copy, param, sizeof(cm_copy));
		memcpy(&cm, &cm_copy, sizeof(struct cm_cfg));
		reg->cm_coef01_00 = (cm.c01 << 16) | cm.c00;
		reg->cm_coef03_02 = (cm.c03 << 16) | cm.c02;
		reg->cm_coef11_10 = (cm.c11 << 16) | cm.c10;
		reg->cm_coef13_12 = (cm.c13 << 16) | cm.c12;
		reg->cm_coef21_20 = (cm.c21 << 16) | cm.c20;
		reg->cm_coef23_22 = (cm.c23 << 16) | cm.c22;
		reg->dpu_enhance_cfg |= BIT(2);
		pr_info("enhance cm set\n");
		break;
	case ENHANCE_CFG_ID_LTM:
		reg->dpu_enhance_cfg |= BIT(7);
		pr_info("enhance ltm set\n");
	case ENHANCE_CFG_ID_SLP:
		memcpy(&slp_copy, param, sizeof(slp_copy));
		if (!cabc_state) {
			slp_copy.s37 = 0;
			slp_copy.s38 = 255;
		}
		slp = &slp_copy;
		reg->slp_cfg0 = (slp->s0 << 0)|
			((slp->s1 & 0x7f) << 16);
		reg->slp_cfg1 = ((slp->s5 & 0x7f) << 21) |
			((slp->s4 & 0x7f) << 14) |
			((slp->s3 & 0x7f) << 7) |
			((slp->s2 & 0x7f) << 0);
		reg->slp_cfg2 = ((slp->s9 & 0x7f) << 25) |
			((slp->s8 & 0x7f) << 18) |
			((slp->s7 & 0x3) << 16) |
			((slp->s6 & 0x7f) << 9);
		reg->slp_cfg3 = ((slp->s14 & 0xfff) << 19) |
			((slp->s13 & 0xf) << 15) |
			((slp->s12 & 0xf) << 11) |
			((slp->s11 & 0xf) << 7) |
			((slp->s10 & 0xf) << 3);
		reg->slp_cfg4 = (slp->s18 << 24) |
			(slp->s17 << 16) | (slp->s16 << 8) |
			(slp->s15 << 0);
		reg->slp_cfg5 = (slp->s22 << 24) |
			(slp->s21 << 16) | (slp->s20 << 8) |
			(slp->s19 << 0);
		reg->slp_cfg6 = (slp->s26 << 24) |
			(slp->s25 << 16) | (slp->s24 << 8) |
			(slp->s23 << 0);
		reg->slp_cfg7 = ((slp->s29 & 0x1ff) << 23) |
			((slp->s28 & 0x1ff) << 14) |
			((slp->s27 & 0x1ff) << 5);
		reg->slp_cfg8 = ((slp->s32 & 0x1ff) << 23) |
			((slp->s31 & 0x1ff) << 14) |
			((slp->s30 & 0x1fff) << 0);
		reg->slp_cfg9 = ((slp->s36 & 0x7f) << 25) |
			((slp->s35 & 0xff) << 17) |
			((slp->s34 & 0xf) << 13) |
			((slp->s33 & 0x7f) << 6);
		reg->slp_cfg10 = (slp->s38 << 8) | (slp->s37 << 0);
		reg->dpu_enhance_cfg |= BIT(6);
		pr_info("enhance slp set\n");
		break;
	case ENHANCE_CFG_ID_GAMMA:
		memcpy(&gamma_copy, param, sizeof(gamma_copy));
		gamma = &gamma_copy;
		p32 = lut_gamma_vaddr;
		for (j = 0; j < 256; j++) {
			*p32 = (gamma->r[j] << 20) |
				(gamma->g[j] << 10) |
				gamma->b[j];
			p32 += 2;
		}
		reg->dpu_enhance_cfg |= BIT(3) | BIT(5);
		pr_info("enhance gamma set\n");
		break;
	case ENHANCE_CFG_ID_EPF:
		memcpy(&epf_copy, param, sizeof(epf_copy));
		epf = &epf_copy;
		dpu_epf_set(reg, epf);
		reg->dpu_enhance_cfg |= BIT(0);
		pr_info("enhance epf set\n");
		break;
	case ENHANCE_CFG_ID_LUT3D:
		memcpy(&lut3d_copy, param, sizeof(lut3d_copy));
		lut3d = &lut3d_copy;
		p32 = lut_lut3d_vaddr;
		for (i = 0; i < 8; i++) {
			tmp32 = p32;
			p32 = lut_lut3d_vaddr + 4 * i;
			for (j = 0; j < 729; j++) {
				*p32 = (lut3d->r[i] << 20) |
					(lut3d->g[i] << 10) |
					lut3d->b[i];
				p32 += 8;
			}
			p32 = ++tmp32;
		}
		reg->dpu_enhance_cfg |= BIT(4);
		pr_info("enhance lut3d set\n");
		enhance_en = reg->dpu_enhance_cfg;
		break;
	case ENHANCE_CFG_ID_CABC_MODE:
		p32 = param;
		if (*p32 & CABC_MODE_UI)
			cabc_para.video_mode = 0;
		else if (*p32 & CABC_MODE_FULL_FRAME)
			cabc_para.video_mode = 1;
		else if (*p32 & CABC_MODE_VIDEO)
			cabc_para.video_mode = 2;
		pr_info("enhance CABC mode: 0x%x\n", *p32);
		return;
	case ENHANCE_CFG_ID_CABC_PARAM:
		memcpy(&cabc_param, param, sizeof(struct cabc_para));
		cabc_para.bl_fix = cabc_param.bl_fix;
		cabc_para.cfg0 = cabc_param.cfg0;
		cabc_para.cfg1 = cabc_param.cfg1;
		cabc_para.cfg2 = cabc_param.cfg2;
		cabc_para.cfg3 = cabc_param.cfg3;
		cabc_para.cfg4 = cabc_param.cfg4;
		return;
	case ENHANCE_CFG_ID_CABC_RUN:
		if (cabc_state != CABC_DISABLED)
			schedule_work(&ctx->cabc_work);
		return;
	case ENHANCE_CFG_ID_CABC_STATE:
		p32 = param;
		cabc_state = *p32;
		frame_no = 0;
		return;
	case ENHANCE_CFG_ID_UD:
		memcpy(&ud_copy, param, sizeof(ud_copy));
		ud = param;
		reg->ud_cfg0 = ud->u0 | (ud->u1 << 16) | (ud->u2 << 24);
		reg->ud_cfg1 = ud->u3 | (ud->u4 << 16) | (ud->u5 << 24);
		reg->dpu_enhance_cfg |= BIT(10) | BIT(11) | BIT(12);
		pr_info("enhance ud set\n");
		break;
	case ENHANCE_CFG_ID_UPDATE_LUTS:
		dpu_luts_update(ctx, param);
	default:
		break;
	}

	if ((ctx->if_type == SPRD_DISPC_IF_DPI) && !ctx->is_stopped) {
		reg->dpu_ctrl |= BIT(2);
		dpu_wait_all_update_done(ctx);
	} else if ((ctx->if_type == SPRD_DISPC_IF_EDPI) && panel_ready) {
		/*
		 * In EDPI mode, we need to wait panel initializatin
		 * completed. Otherwise, the dpu enhance settings may
		 * start before panel initialization.
		 */
		reg->dpu_ctrl |= BIT(0);
		ctx->is_stopped = false;
	}

	enhance_en = reg->dpu_enhance_cfg | (reg->scl_en << 13);

}

static void dpu_luts_copyto_user(u32 *param)
{
	u32 *p32;
	u32 *ptmp;
	u32 i, j;
	u16 r, g, b;
	u16 h_o, s_g;
	u16 mode, mode_index;
	struct luts_typeindex *type_index;

	type_index = (struct luts_typeindex *)param;
	pr_info("%s type =%d, index =%d\n", __func__,
		type_index->type, type_index->index);

	if (type_index->type == LUTS_GAMMA_TYPE) {
		p32 = lut_gamma_vaddr + 1024 * type_index->index;
		pr_info("gamma:type_index %d\n", type_index->type);
		for (j = 0; j < 256; j++) {
			r = (*p32 >> 20) & 0x3ff;
			g = (*p32 >> 10) & 0x3ff;
			b = *p32 & 0x3ff;
			p32 += 2;
			pr_info("r %d, g %d, b %d\n", r, g, b);
		}
	}

	if (type_index->type == LUTS_HSV_TYPE) {
		p32 = lut_hsv_vaddr + 1024 * type_index->index;
		ptmp = p32;
		pr_info("hsv_mode%d\n", type_index->type);
		for (i = 0; i < 4; i++) {
			p32 = ptmp;
			pr_info("hsv_index%d\n\n", i);
			for (j = 0; j < 64; j++) {
				h_o =  (*p32 >> 9) & 0x7f;
				s_g = *p32 & 0x1ff;
				pr_info("h_o %d, s_g %d\n", h_o, s_g);
				p32 += 4;
			}
			ptmp++;
		}
	}

	if (type_index->type == LUTS_LUT3D_TYPE) {
		p32 = lut_lut3d_vaddr;
		mode = type_index->index >> 8;
		mode_index = type_index->index & 0xff;
		pr_info("3dlut: mode%d index%d\n", mode, mode_index);
		p32 += mode * 1024 * 6;
		p32 += mode_index;
		for (i = 0; i < 729; i++) {
			r = (*p32 >> 20) & 0x3ff;
			g = (*p32 >> 10) & 0x3ff;
			b = *p32 & 0x3ff;
			pr_info("r %d, g %d, b %d\n", r, g, b);
			p32 += 8;
		}
	}
}


static void dpu_enhance_get(struct dpu_context *ctx, u32 id, void *param)
{
	struct dpu_reg *reg = (struct dpu_reg *)ctx->base;
	struct scale_cfg *scale;
	struct epf_cfg *epf;
	struct slp_cfg *slp;
	struct cm_cfg *cm;
	struct gamma_lut *gamma;
	struct hsv_luts *hsv_table;
	struct ud_cfg *ud;
	struct hsv_params *hsv_cfg;
	int i, j, val;
	u16 *p16;
	u32 *dpu_lut_addr, *dpu_lut_raddr, *p32;
	int *vsynccount;
	int *frameno;

	switch (id) {
	case ENHANCE_CFG_ID_ENABLE:
		p32 = param;
		*p32 = reg->dpu_enhance_cfg;
		pr_info("enhance module enable get\n");
		break;
	case ENHANCE_CFG_ID_SCL:
		scale = param;
		val = reg->blend_size;
		scale->in_w = val & 0xffff;
		scale->in_h = val >> 16;
		pr_info("enhance scaling get\n");
		break;
	case ENHANCE_CFG_ID_EPF:
		epf = param;

		val = reg->epf_epsilon;
		epf->e0 = val;
		epf->e1 = val >> 16;

		val = reg->epf_gain0_3;
		epf->e2 = val;
		epf->e3 = val >> 8;
		epf->e4 = val >> 16;
		epf->e5 = val >> 24;

		val = reg->epf_gain4_7;
		epf->e6 = val;
		epf->e7 = val >> 8;
		epf->e8 = val >> 16;
		epf->e9 = val >> 24;

		val = reg->epf_diff;
		epf->e10 = val;
		epf->e11 = val >> 8;
		pr_info("enhance epf get\n");
		break;
	case ENHANCE_CFG_ID_HSV:
		dpu_stop(ctx);
		hsv_cfg = (struct hsv_params *)param;
		hsv_cfg->h0 = reg->hsv_cfg & 0xff;
		hsv_cfg->h1 = (reg->hsv_cfg >> 8) & 0x3;
		hsv_cfg->h2 = (reg->hsv_cfg >> 12) & 0x3;
		hsv_cfg++;
		hsv_table = (struct hsv_luts *)hsv_cfg;
		dpu_lut_addr = (u32 *)&reg->hsv_lut0_addr;
		dpu_lut_raddr = (u32 *)&reg->hsv_lut0_raddr;
		for (i = 0; i < 4; i++) {
			for (j = 0; j < 64; j++) {
				*dpu_lut_addr = j;
				udelay(1);
				hsv_table->hsv_lut[i].h_o[j] =
					(*dpu_lut_raddr >> 9)  & 0x7f;
				hsv_table->hsv_lut[i].s_g[j] =
					*dpu_lut_raddr & 0x1ff;
			}
			dpu_lut_addr += 2;
			dpu_lut_raddr += 2;
		}
		dpu_run(ctx);
		pr_info("enhance hsv get\n");
		break;
	case ENHANCE_CFG_ID_CM:
		cm = (struct cm_cfg *)param;
		cm->c00 = reg->cm_coef01_00 & 0x3fff;
		cm->c01 = (reg->cm_coef01_00 >> 16) & 0x3fff;
		cm->c02 = reg->cm_coef03_02 & 0x3fff;
		cm->c03 = (reg->cm_coef03_02 >> 16) & 0x3fff;
		cm->c10 = reg->cm_coef11_10 & 0x3fff;
		cm->c11 = (reg->cm_coef11_10 >> 16) & 0x3fff;
		cm->c12 = reg->cm_coef13_12 & 0x3fff;
		cm->c13 = (reg->cm_coef13_12 >> 16) & 0x3fff;
		cm->c20 = reg->cm_coef21_20 & 0x3fff;
		cm->c21 = (reg->cm_coef21_20 >> 16) & 0x3fff;
		cm->c22 = reg->cm_coef23_22 & 0x3fff;
		cm->c23 = (reg->cm_coef23_22 >> 16) & 0x3fff;

		pr_info("enhance cm get\n");
		break;
	case ENHANCE_CFG_ID_LTM:
	case ENHANCE_CFG_ID_SLP:
		slp = param;
		val = reg->slp_cfg0;
		slp->s0 = (val >> 0) & 0xffff;
		slp->s1 = (val >> 16) & 0x7f;

		val = reg->slp_cfg1;
		slp->s5 = (val >> 21) & 0x7f;
		slp->s4 = (val >> 14) & 0x7f;
		slp->s3 = (val >> 7) & 0x7f;
		slp->s2 = (val >> 0) & 0x7f;

		val = reg->slp_cfg2;
		slp->s9 = (val >> 25) & 0x7f;
		slp->s8 = (val >> 18) & 0x7f;
		slp->s7 = (val >> 16) & 0x3;
		slp->s6 = (val >> 9) & 0x7f;

		val = reg->slp_cfg3;
		slp->s14 = (val >> 19) & 0xfff;
		slp->s13 = (val >> 15) & 0xf;
		slp->s12 = (val >> 11) & 0xf;
		slp->s11 = (val >> 7) & 0xf;
		slp->s10 = (val >> 3) & 0xf;

		val = reg->slp_cfg4;
		slp->s18 = (val >> 24) & 0xff;
		slp->s17 = (val >> 16) & 0xff;
		slp->s16 = (val >> 8) & 0xff;
		slp->s15 = (val >> 0) & 0xff;

		val = reg->slp_cfg5;
		slp->s22 = (val >> 24) & 0xff;
		slp->s21 = (val >> 16) & 0xff;
		slp->s20 = (val >> 8) & 0xff;
		slp->s19 = (val >> 0) & 0xff;

		val = reg->slp_cfg6;
		slp->s26 = (val >> 24) & 0xff;
		slp->s25 = (val >> 16) & 0xff;
		slp->s24 = (val >> 8) & 0xff;
		slp->s23 = (val >> 0) & 0xff;

		val = reg->slp_cfg7;
		slp->s29 = (val >> 23) & 0x1ff;
		slp->s28 = (val >> 14) & 0x1ff;
		slp->s27 = (val >> 5) & 0x1ff;

		val = reg->slp_cfg8;
		slp->s32 = (val >> 23) & 0x1ff;
		slp->s31 = (val >> 14) & 0x1ff;
		slp->s30 = (val >> 0) & 0x1fff;

		val = reg->slp_cfg9;
		slp->s36 = (val >> 25) & 0x7f;
		slp->s35 = (val >> 17) & 0xff;
		slp->s34 = (val >> 13) & 0xf;
		slp->s33 = (val >> 6) & 0x7f;

		/* FIXME SLP_CFG10 just for CABC*/
		val = reg->slp_cfg10;
		slp->s38 = (val >> 8) & 0xff;
		slp->s37 = (val >> 0) & 0xff;
		pr_info("enhance slp get\n");
		break;
	case ENHANCE_CFG_ID_GAMMA:
		dpu_stop(ctx);
		gamma = param;
		for (i = 0; i < 256; i++) {
			reg->gamma_lut_addr = i;
			udelay(1);
			val = reg->gamma_lut_rdata;
			gamma->r[i] = (val >> 20) & 0x3FF;
			gamma->g[i] = (val >> 10) & 0x3FF;
			gamma->b[i] = val & 0x3FF;
			pr_debug("0x%02x: r=%u, g=%u, b=%u\n", i,
				gamma->r[i], gamma->g[i], gamma->b[i]);
		}
		dpu_run(ctx);
		pr_info("enhance gamma get\n");
		break;
	case ENHANCE_CFG_ID_SLP_LUT:
		dpu_stop(ctx);
		p32 = param;
		for (i = 0; i < 256; i++) {
			reg->slp_lut_addr = i;
			udelay(1);
			*p32++ = reg->slp_lut_rdata;
		}
		dpu_run(ctx);
		pr_info("enhance slp lut get\n");
		break;
	case ENHANCE_CFG_ID_LUT3D:
		dpu_stop(ctx);
		p32 = param;
		dpu_lut_addr = (u32 *)&reg->threed_lut0_addr;
		dpu_lut_raddr = (u32 *)&reg->threed_lut0_rdata;
		for (j = 0; j < 729; j++) {
			*dpu_lut_addr = j;
			udelay(1);
			*p32++ = *dpu_lut_raddr;
		}
		dpu_run(ctx);
		pr_info("enhance lut3d get\n");
		break;
	case ENHANCE_CFG_ID_UD:
		ud = param;
		ud->u0 = reg->ud_cfg0 & 0xfff;
		ud->u1 = (reg->ud_cfg0 >> 16) & 0x3f;
		ud->u2 = (reg->ud_cfg0 >> 24) & 0x3f;
		ud->u3 = reg->ud_cfg1 & 0xfff;
		ud->u4 = (reg->ud_cfg1 >> 16) & 0x3f;
		ud->u5 = (reg->ud_cfg1 >> 24) & 0x3f;
		pr_info("enhance ud get\n");
		break;
	case ENHANCE_CFG_ID_CABC_HIST_V2:
		p32 = param;
		for (i = 0; i < 64; i++) {
			*p32++ = reg->cabc_hist[i];
			udelay(1);
		}
		break;
	case ENHANCE_CFG_ID_CABC_CUR_BL:
		p16 = param;
		*p16 = cabc_para.cur_bl;
		break;
	case ENHANCE_CFG_ID_VSYNC_COUNT:
		vsynccount = param;
		*vsynccount = vsync_count;
		break;
	case ENHANCE_CFG_ID_FRAME_NO:
		frameno = param;
		*frameno = frame_no;
		break;
	case ENHANCE_CFG_ID_CABC_STATE:
		p32 = param;
		*p32 = cabc_state;
		break;
	case ENHANCE_CFG_ID_UPDATE_LUTS:
		dpu_luts_copyto_user(param);
		break;
	default:
		break;
	}

}

static void dpu_enhance_reload(struct dpu_context *ctx)
{
	struct dpu_reg *reg = (struct dpu_reg *)ctx->base;
	struct scale_cfg *scale;
	struct cm_cfg *cm;
	struct slp_cfg *slp;
	struct epf_cfg *epf;
	struct ud_cfg *ud;
	int i;
	u16 *p16;

	p16 = (u16 *)lut_slp_vaddr;
	for (i = 0; i < 256; i++)
		*p16++ = slp_lut[i];
	reg->enhance_update |= BIT(4);

	if (enhance_en & BIT(13)) {
		scale = &scale_copy;
		reg->blend_size = (scale->in_h << 16) | scale->in_w;
		reg->scl_en = BIT(0);
		pr_info("enhance scaling from %ux%u to %ux%u\n", scale->in_w,
			scale->in_h, ctx->vm.hactive, ctx->vm.vactive);
	}

	if (enhance_en & BIT(0)) {
		epf = &epf_copy;
		dpu_epf_set(reg, epf);
		pr_info("enhance epf reload\n");
	}

	if (enhance_en & BIT(1)) {
		reg->hsv_cfg = hsv_cfg_copy;
		reg->hsv_lut_base_addr = lut_addrs_cpy.lut_hsv_addr;
		reg->enhance_update |= BIT(2);
		pr_info("enhance hsv set\n");
		pr_info("enhance hsv reload\n");
	}

	if (enhance_en & BIT(2)) {
		cm = &cm_copy;
		reg->cm_coef01_00 = (cm->c01 << 16) | cm->c00;
		reg->cm_coef03_02 = (cm->c03 << 16) | cm->c02;
		reg->cm_coef11_10 = (cm->c11 << 16) | cm->c10;
		reg->cm_coef13_12 = (cm->c13 << 16) | cm->c12;
		reg->cm_coef21_20 = (cm->c21 << 16) | cm->c20;
		reg->cm_coef23_22 = (cm->c23 << 16) | cm->c22;
		pr_info("enhance cm reload\n");
	}

	if (enhance_en & BIT(6)) {
		slp = &slp_copy;
		reg->slp_cfg0 = (slp->s0 << 0) |
			((slp->s1 & 0x7f) << 16);
		reg->slp_cfg1 = ((slp->s5 & 0x7f) << 21) |
			((slp->s4 & 0x7f) << 14) |
			((slp->s3 & 0x7f) << 7) |
			((slp->s2 & 0x7f) << 0);
		reg->slp_cfg2 = ((slp->s9 & 0x7f) << 25) |
			((slp->s8 & 0x7f) << 18) |
			((slp->s7 & 0x3) << 16) |
			((slp->s6 & 0x7f) << 9);
		reg->slp_cfg3 = ((slp->s14 & 0xfff) << 19) |
			((slp->s13 & 0xf) << 15) |
			((slp->s12 & 0xf) << 11) |
			((slp->s11 & 0xf) << 7) |
			((slp->s10 & 0xf) << 3);
		reg->slp_cfg4 = (slp->s18 << 24) |
			(slp->s17 << 16) | (slp->s16 << 8) |
			(slp->s15 << 0);
		reg->slp_cfg5 = (slp->s22 << 24) |
			(slp->s21 << 16) |
			(slp->s20 << 8) |
			(slp->s19 << 0);
		reg->slp_cfg6 = (slp->s26 << 24) |
			(slp->s25 << 16) | (slp->s24 << 8) |
			(slp->s23 << 0);
		reg->slp_cfg7 = ((slp->s29 & 0x1ff) << 23) |
			((slp->s28 & 0x1ff) << 14) |
			((slp->s27 & 0x1ff) << 5);
		reg->slp_cfg8 = ((slp->s32 & 0x1ff) << 23) |
			((slp->s31 & 0x1ff) << 14) |
			((slp->s30 & 0x1fff) << 0);
		reg->slp_cfg9 = ((slp->s36 & 0x7f) << 25) |
			((slp->s35 & 0xff) << 17) |
			((slp->s34 & 0xf) << 13) |
			((slp->s33 & 0x7f) << 6);
		reg->slp_cfg10 = (slp->s38 << 8) |
			(slp->s37 << 0);
		pr_info("enhance slp reload\n");
	}

	if (enhance_en & BIT(3)) {
		reg->gamma_lut_base_addr = lut_addrs_cpy.lut_gamma_addr;
		reg->enhance_update |= BIT(1);
		pr_info("enhance gamma reload\n");
	}

	if (enhance_en & BIT(7)) {
		slp = &slp_copy;
		reg->slp_cfg8 = ((slp->s32 & 0x1ff) << 23) |
			((slp->s31 & 0x1ff) << 14) |
			((slp->s30 & 0x1fff) << 0);
		pr_info("enhance ltm reload\n");
	}

	if (enhance_en & BIT(4)) {
		reg->threed_lut_base_addr = lut_addrs_cpy.lut_lut3d_addr;
		reg->enhance_update |= BIT(3);
		pr_info("enhance lut3d reload\n");
	}

	if (enhance_en & BIT(10)) {
		ud = &ud_copy;
		reg->ud_cfg0 = ud->u0 | (ud->u1 << 16) | (ud->u2 << 24);
		reg->ud_cfg1 = ud->u3 | (ud->u4 << 16) | (ud->u5 << 24);
		pr_info("enhance ud set\n");
		pr_info("enhance ud reload\n");
	}

	reg->dpu_enhance_cfg = enhance_en;
	first_frame = true;
}

static void dpu_sr_config(struct dpu_context *ctx)
{
	struct dpu_reg *reg = (struct dpu_reg *)ctx->base;

	reg->blend_size = (scale_copy.in_h << 16) | scale_copy.in_w;
	if (need_scale) {
		enhance_en |= BIT(13);
		reg->scl_en = BIT(0);
		reg->dpu_enhance_cfg = enhance_en;
	} else {
		enhance_en &= ~(BIT(13));
		reg->scl_en = 0;
		reg->dpu_enhance_cfg = enhance_en;
	}
}

static int dpu_cabc_trigger(struct dpu_context *ctx)
{
	struct dpu_reg *reg = (struct dpu_reg *)ctx->base;
	struct device_node *backlight_node;
	int i;

	/*
	 * add the cabc_hist outliers filtering,
	 * if the 64 elements of cabc_hist are equal to 0, then end the cabc_trigger
	 * process and return 0 directly.
	 */

	if (frame_no != 0) {
		for (i = 0; i < 64; i++)
			if (reg->cabc_hist[i] != 0)
				break;

		if (i == 64)
			return 0;
	}

	if (cabc_state) {
		if ((cabc_state == CABC_STOPPING) && bl_dev) {
			memset(&cabc_para, 0, sizeof(cabc_para));
			reg->cabc_cfg[0] = cabc_cfg0;
			reg->cabc_cfg[1] = cabc_cfg1;
			reg->cabc_cfg[2] = cabc_cfg2;
			reg->cabc_cfg[3] = cabc_cfg3;
			reg->cabc_cfg[4] = cabc_cfg4;
			cabc_bl_set = true;
			frame_no = 0;
			cabc_state = CABC_DISABLED;
			enhance_en &= ~(BIT(9));
			reg->dpu_enhance_cfg = enhance_en;

		}
		return 0;
	}

	if (frame_no == 0) {
		if (!bl_dev) {
			backlight_node = of_parse_phandle(g_np,
						 "sprd,backlight", 0);
			if (backlight_node) {
				bl_dev =
				of_find_backlight_by_node(backlight_node);
				of_node_put(backlight_node);
			} else {
				pr_warn("dpu backlight node not found\n");
			}
		}
		if (bl_dev)
			sprd_backlight_normalize_map(bl_dev, &cabc_para.cur_bl);

		reg->cabc_cfg[0] = cabc_cfg0;
		reg->cabc_cfg[1] = cabc_cfg1;
		reg->cabc_cfg[2] = cabc_cfg2;
		reg->cabc_cfg[3] = cabc_cfg3;
		reg->cabc_cfg[4] = cabc_cfg4;
		enhance_en |= BIT(9);
		reg->dpu_enhance_cfg |= enhance_en;
		frame_no++;
	} else {
		reg->cabc_cfg[0] = cabc_para.cfg0;
		reg->cabc_cfg[1] = cabc_para.cfg1;
		reg->cabc_cfg[2] = cabc_para.cfg2;
		reg->cabc_cfg[3] = cabc_para.cfg3;
		reg->cabc_cfg[4] = cabc_para.cfg4;
		if (bl_dev)
			cabc_bl_set = true;

		if (frame_no == 1)
			frame_no++;
	}
	return 0;
}

static int dpu_modeset(struct dpu_context *ctx,
		struct drm_mode_modeinfo *mode)
{
	scale_copy.in_w = mode->hdisplay;
	scale_copy.in_h = mode->vdisplay;

	if ((mode->hdisplay != ctx->vm.hactive) ||
	    (mode->vdisplay != ctx->vm.vactive))
		need_scale = true;
	else
		need_scale = false;

	mode_changed = true;
	wb_size_changed = true;
	pr_info("begin switch to %u x %u\n", mode->hdisplay, mode->vdisplay);

	return 0;
}

static const u32 primary_fmts[] = {
	DRM_FORMAT_XRGB8888, DRM_FORMAT_XBGR8888,
	DRM_FORMAT_ARGB8888, DRM_FORMAT_ABGR8888,
	DRM_FORMAT_RGBA8888, DRM_FORMAT_BGRA8888,
	DRM_FORMAT_RGBX8888, DRM_FORMAT_BGRX8888,
	DRM_FORMAT_RGB565, DRM_FORMAT_BGR565,
	DRM_FORMAT_NV12, DRM_FORMAT_NV21,
	DRM_FORMAT_NV16, DRM_FORMAT_NV61,
	DRM_FORMAT_YUV420,
};

static int dpu_capability(struct dpu_context *ctx,
			struct dpu_capability *cap)
{
	if (!cap)
		return -EINVAL;

	cap->max_layers = 6;
	cap->fmts_ptr = primary_fmts;
	cap->fmts_cnt = ARRAY_SIZE(primary_fmts);

	return 0;
}

static struct dpu_core_ops dpu_r6p0_ops = {
	.parse_dt = dpu_parse_dt,
	.version = dpu_get_version,
	.init = dpu_init,
	.uninit = dpu_uninit,
	.run = dpu_run,
	.stop = dpu_stop,
	.isr = dpu_isr,
	.ifconfig = dpu_dpi_init,
	.capability = dpu_capability,
	.flip = dpu_flip,
	.bg_color = dpu_bgcolor,
	.enable_vsync = enable_vsync,
	.disable_vsync = disable_vsync,
	.enhance_set = dpu_enhance_set,
	.enhance_get = dpu_enhance_get,
	.modeset = dpu_modeset,
	.write_back = dpu_wb_trigger,
	.dma_request = dpu_dma_request,
};

static struct ops_entry entry = {
	.ver = "dpu-r6p0",
	.ops = &dpu_r6p0_ops,
};

static int __init dpu_core_register(void)
{
	return dpu_core_ops_register(&entry);
}

subsys_initcall(dpu_core_register);
