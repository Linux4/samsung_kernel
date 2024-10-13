// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (C) 2020 Samsung Electronics Co.Ltd
 *
 * This program is free software; you can redistribute  it and/or modify it
 * under  the terms of  the GNU General  Public License as published by the
 * Free Software Foundation;  either version 2 of the  License, or (at your
 * option) any later version.
 *
 */

#include <linux/of_address.h>
#include <linux/dma-buf.h>
#include <linux/dma-heap.h>
#include <linux/delay.h>
#include <linux/atomic.h>
#include <linux/platform_device.h>
#include <linux/irq.h>
#include <linux/of_irq.h>
#include <linux/iosys-map.h>
#include <dqe_cal.h>
#include <decon_cal.h>
#include <exynos_drm_decon.h>
#include <exynos_drm_crtc.h>
#include <exynos_drm_dqe.h>
#include <dqe_cal_dummy.h>

static int dpu_dqe_log_level = 6;
module_param(dpu_dqe_log_level, int, 0600);
MODULE_PARM_DESC(dpu_dqe_log_level, "log level for dpu dqe [default : 6]");

#define DQE_DRIVER_NAME "exynos-drmdqe"
#define dqe_info(dqe, fmt, ...)	\
dpu_pr_info(DQE_DRIVER_NAME, (dqe)->id, dpu_dqe_log_level, fmt, ##__VA_ARGS__)

#define dqe_warn(dqe, fmt, ...)	\
dpu_pr_warn(DQE_DRIVER_NAME, (dqe)->id, dpu_dqe_log_level, fmt, ##__VA_ARGS__)

#define dqe_err(dqe, fmt, ...)	\
dpu_pr_err(DQE_DRIVER_NAME, (dqe)->id, dpu_dqe_log_level, fmt, ##__VA_ARGS__)

#define dqe_debug(dqe, fmt, ...)\
dpu_pr_debug(DQE_DRIVER_NAME, (dqe)->id, dpu_dqe_log_level, fmt, ##__VA_ARGS__)

enum dqe_reset_type {
	DQE_RESET_GLOBAL = 0,	// sets when SFR need to be updated
	DQE_RESET_CGC,			// sets when CGC LUT need to be updated
};

enum dqe_reset_ops {
	DQE_RESET_FULL = 0,	// sets when require to reset all
	DQE_RESET_OPT,		// sets when optimized updates are required
};

enum dqe_update_flag {
	DQE_UPDATED_NONE	= 0,
	DQE_UPDATED_CONTEXT	= 1 << 0,
	DQE_UPDATED_CONFIG	= 1 << 1,
};

#define DQE_VER_500 (0x5000000) // 9925 evt0
#define DQE_VER_600 (0x6000000) // 9925 evt1, 9935
#define DQE_VER_610 (0x6100000) // 8825, 8835
#define DQE_VER_800 (0x8000000) // 9945 evt0
#define DQE_VER_810 (0x8100000) // 9945 evt1, 8845

#define DQE_MODE_MAIN 0
#define DQE_MODE_PRESET 1 // start from 1
#define DQE_PRESET_ATTR_NUM 4
#define DQE_PRESET_CM_SHIFT 16
#define DQE_PRESET_RI_SHIFT 0
#define DQE_PRESET_ALL_MASK 0xFFFF

#define IS_INDEXABLE(arg) (sizeof(arg[0]))
#define IS_ARRAY(arg) (IS_INDEXABLE(arg) && (((void *) &arg) == ((void *) arg)))

// saved LUTs of each mode type
struct dqe_ctx_int {
	u32 mode_attr[DQE_PRESET_ATTR_NUM];

	u32 disp_dither[DQE_DISP_DITHER_LUT_MAX];
	u32 cgc_dither[DQE_CGC_DITHER_LUT_MAX];

	int gamma_matrix[DQE_GAMMA_MATRIX_LUT_MAX];
	int degamma_lut_ext;
	u32 degamma_lut[DQE_REG_BPC_MAX][DQE_DEGAMMA_LUT_MAX];
	int regamma_lut_ext;
	u32 regamma_lut[DQE_REG_BPC_MAX][DQE_REGAMMA_LUT_MAX];

	u32 cgc17_encoded[3][17][17][5];
	int cgc17_enc_rgb;
	int cgc17_enc_idx;
	u32 cgc17_con[DQE_CGC_CON_LUT_MAX];
	u32 cgc17_lut[3][DQE_CGC_LUT_MAX];

	int hsc48_lcg_idx;
	u32 hsc48_lcg[3][DQE_HSC_LUT_LSC_GAIN_MAX];
	u32 hsc48_lut[DQE_HSC_LUT_MAX];

	u32 atc_lut[DQE_ATC_LUT_MAX];

	u32 scl_input[DQE_SCL_COEF_MAX+1]; // en + coef sets

	u32 de_lut[DQE_DE_LUT_MAX];
	u32 rcd_lut[DQE_RCD_LUT_MAX];
};

#define DQE_COLORMODE_MAGIC 0xDA
enum dqe_colormode_id {
	DQE_COLORMODE_ID_CGC17_ENC = 1,
	DQE_COLORMODE_ID_CGC17_CON,
	DQE_COLORMODE_ID_CGC_DITHER,
	DQE_COLORMODE_ID_DEGAMMA,
	DQE_COLORMODE_ID_GAMMA,
	DQE_COLORMODE_ID_GAMMA_MATRIX,
	DQE_COLORMODE_ID_HSC48_LCG,
	DQE_COLORMODE_ID_HSC,
	DQE_COLORMODE_ID_SCL,
	DQE_COLORMODE_ID_DE,
	DQE_COLORMODE_ID_MAX,
};

struct dqe_colormode_global_header {
	u8 magic;
	u8 seq_num;
	u16 total_size;
	u16 header_size;
	u16 num_data;
	u16 crc;
	u16 reserved;
};

struct dqe_colormode_data_header {
	u8 magic;
	u8 id;
	u16 total_size;
	u16 header_size;
	u8 attr[4];
	u16 crc;
};

// ID_MAX + cgc (50) + hsc (2) + reg (1) + deg (1) + etc (10)
#define MAX_CRC (DQE_COLORMODE_ID_MAX + 50 + 2 + 1 + 1 + 10)

static struct dqe_ctx_int *dqe_lut;
static u32 mode_idx = DQE_MODE_MAIN;
static char dqe_str_result[1024];

// 2 luts in 1 reg
#define R2L_I(_v, _i)	(((_v) << 1) | ((_i) & 0x1))
#define R2L_D(_v, _i)	(((_v) >> (0 + (16 * ((_i) & 0x1)))) & 0x1fff)

static int dqe_conv_str2lut(const char *buffer, u32 *lut,
				int size, bool printHex);
static int dqe_conv_str2cgc(const char *buffer, struct dqe_ctx_int *lut);
static int dqe_dec_cgc17(struct dqe_ctx_int *lut, int mask, int wr_mode);
static void dqe_set_degamma_lut(struct exynos_dqe *dqe,
				struct dqe_ctx_int *lut, bool clrForced);
static void dqe_set_gamma_lut(struct exynos_dqe *dqe,
				struct dqe_ctx_int *lut, bool clrForced);
static ssize_t dqe_conv_lut2str(u32 *lut, int size, char *out);
static struct device_attribute *dqe_attrs[];

/*===== for legacy model (~v6.1) build ====*/
__weak void dqe_reg_set_cgc_lut(u16 (*ctx)[3], u32 (*lut)[DQE_CGC_LUT_MAX]) {}
__weak void dqe_reg_set_cgc_con(u32 *ctx, u32 *lut) {}
__weak void dqe_reg_set_disp_dither(u32 *ctx, u32 *lut) {}
__weak void dqe_reg_set_cgc_dither(u32 *ctx, u32 *lut, int bit_ext) {}
__weak void dqe_reg_set_gamma_matrix(u32 *ctx, int *lut, u32 shift) {}
__weak void dqe_reg_set_gamma_lut(u32 *ctx, u32 *lut, u32 shift) {}
__weak void dqe_reg_set_degamma_lut(u32 *ctx, u32 *lut, u32 shift) {}
__weak void dqe_reg_set_hsc_lut(u32 *con, u32 *poly, u32 *gain, u32 *lut,
				u32 (*lcg)[DQE_HSC_LUT_LSC_GAIN_MAX], u32 shift)
{
}
__weak void dqe_reg_set_aps_lut(u32 *ctx, u32 *lut, u32 bpc, u32 shift) {}
__weak void dqe_reg_set_scl_lut(u32 *ctx, u32 *lut) {}
__weak void dqe_reg_set_de_lut(u32 *ctx, u32 *lut, u32 shift) {}
__weak void dqe_reg_set_rcd_lut(u32 *ctx, u32 *lut) {}
__weak void dqe_reg_set_atc_img_size(u32 id, u32 width, u32 height) {}
__weak void decon_reg_set_blender_ext(u32 id, bool en) {}
/*===== for legacy model (~v6.1) build ====*/

static inline bool IS_DQE_VER(struct exynos_dqe *dqe, u32 version)
{
	return (dqe->ctx.version == version);
}

static inline bool IS_DQE_VER_RANGE(struct exynos_dqe *dqe, u32 lower, u32 upper)
{
	return (dqe->ctx.version >= lower && dqe->ctx.version <= upper);
}

static inline bool IS_DQE_UPDATE_REQUIRED(struct exynos_dqe *dqe)
{
	return atomic_add_unless(&dqe->update_cnt, -1, 0);
}

// update SFR only when CGC LUT changed if sram retention is enabled
static inline bool IS_DQE_CGCUPDATE_REQUIRED(struct exynos_dqe *dqe)
{
	return !dqe->sram_retention ||
		atomic_add_unless(&dqe->cgc_update_cnt, -1, 0);
}

// maximum 2 updates required for SRAM Set0/1
#define MAX_RESET_CNT 2
static void dqe_reset_update_cnt(struct exynos_dqe *dqe,
				enum dqe_reset_type type, enum dqe_reset_ops ops)
{
	switch (type) {
	case DQE_RESET_GLOBAL:
		// at least 1 update required, no change if higher
		if (ops == DQE_RESET_OPT)
			atomic_cmpxchg(&dqe->update_cnt, 0, 1);
		else
			atomic_set(&dqe->update_cnt, MAX_RESET_CNT);
		break;
	case DQE_RESET_CGC:
		// restore cnt as SRAM indicator also would be reset on dpu off
		if (ops == DQE_RESET_OPT)
			atomic_cmpxchg(&dqe->cgc_update_cnt, 1, MAX_RESET_CNT);
		else
			atomic_set(&dqe->cgc_update_cnt, MAX_RESET_CNT);
		break;
	default:
		dqe_err(dqe, "invalid parameter\n");
		break;
	}
	dqe_debug(dqe, "%d updates reqired\n", atomic_read(&dqe->update_cnt));
}

#define DQE_CTRL_ONOFF		BIT(0)
#define DQE_CTRL_AUTO		BIT(1)
#define DQE_CTRL_FORCEDOFF	BIT(2)

#define SET_FLAG(v, flag) ((v) | (flag))
#define CLR_FLAG(v, flag) ((v) & (~(flag)))
#define UPDATE_FLAG(en, v, flag) ((en > 0) ? SET_FLAG(v, flag) : CLR_FLAG(v, flag))
#define CHECK_FLAG(v, flag) (((v) & (flag)) == (flag))

static inline void dqe_ctrl_forcedoff(u32 *value, int en)
{
	if (!value)
		return;

	*value = UPDATE_FLAG(en, *value, DQE_CTRL_FORCEDOFF);
}

static inline void dqe_ctrl_auto(u32 *value, int en)
{
	if (!value)
		return;

	*value = UPDATE_FLAG(en, *value, DQE_CTRL_AUTO);
}

static inline void dqe_ctrl_onoff(u32 *value, u32 onOff, bool clrForced)
{
	if (!value)
		return;

	if (clrForced)
		*value = CLR_FLAG(*value, DQE_CTRL_FORCEDOFF);

	if (CHECK_FLAG(*value, DQE_CTRL_AUTO)) // on/off control not available
		return;

	*value = UPDATE_FLAG(onOff, *value, DQE_CTRL_ONOFF);
}

static inline bool IS_DQE_FORCEDOFF(u32 *value)
{
	if (!value)
		return false;
	return CHECK_FLAG(*value, DQE_CTRL_FORCEDOFF);
}

static inline bool IS_DQE_ON(u32 *value)
{
	if (!value || IS_DQE_FORCEDOFF(value))
		return false;
	return CHECK_FLAG(*value, DQE_CTRL_ONOFF);
}

static inline bool IS_DQE_AUTO(u32 *value, bool condition)
{
	if (!value || IS_DQE_FORCEDOFF(value))
		return false;
	return (CHECK_FLAG(*value, DQE_CTRL_AUTO) && condition);
}

static inline bool IS_BLENDER_EXT_REQUIRED(struct exynos_dqe *dqe)
{
	return (dqe->cfg.in_bpc == 8);
}

static inline bool IS_DISP_DITHER_REQUIRED(struct exynos_dqe *dqe)
{
	return (dqe->cfg.cur_bpc >= 10 && dqe->cfg.out_bpc == 8);
}

static char *dqe_print_onoff(u32 *value)
{
	if (IS_DQE_AUTO(value, true))
		return "A";
	if (IS_DQE_FORCEDOFF(value))
		return "0*";
	if (IS_DQE_ON(value))
		return "1";
	return "0";
}

static int dqe_alloc_cgc_dma(struct exynos_dqe *dqe, size_t size,
				struct device *dev)
{
	struct dma_buf *buf;
	struct dma_heap *dma_heap;
	struct iosys_map map = IOSYS_MAP_INIT_VADDR(NULL);
	struct dma_buf_attachment *attachment;
	struct sg_table *sg_table;
	dma_addr_t dma_addr;
	int ret;

	dqe_debug(dqe, "+\n");

	size = PAGE_ALIGN(size);

	dqe_debug(dqe, "want %zu bytes\n", size);
	dma_heap = dma_heap_find("system-uncached");
	if (IS_ERR_OR_NULL(dma_heap)) {
		dqe_err(dqe, "dma_heap_find() failed [%p]\n", dma_heap);
		return -ENODEV;
	}

	buf = dma_heap_buffer_alloc(dma_heap, (size_t)size, 0, 0);
	dma_heap_put(dma_heap);
	if (IS_ERR_OR_NULL(buf)) {
		dqe_err(dqe, "dma_buf allocation is failed [%p]\n", buf);
		return PTR_ERR(buf);
	}

	ret = dma_buf_vmap(buf, &map);
	if (ret) {
		dqe_err(dqe, "failed to vmap buffer [%x]\n", ret);
		return -EINVAL;
	}

	/* mapping buffer for translating to DVA */
	attachment = dma_buf_attach(buf, dev);
	if (IS_ERR_OR_NULL(attachment)) {
		dqe_err(dqe, "failed to attach dma_buf [%p]\n", attachment);
		return -EINVAL;
	}

	sg_table = dma_buf_map_attachment(attachment, DMA_TO_DEVICE);
	if (IS_ERR_OR_NULL(sg_table)) {
		dqe_err(dqe, "failed to map attachment [%p]\n", sg_table);
		return -EINVAL;
	}

	dma_addr = sg_dma_address(sg_table->sgl);
	if (IS_ERR_VALUE(dma_addr)) {
		dqe_err(dqe, "failed to map iovmm [%llx]\n", dma_addr);
		return -EINVAL;
	}

	dqe->ctx.cgc_lut = map.vaddr;
	dqe->ctx.cgc_dma = dma_addr;
	dqe_debug(dqe, "cgc lut vaddr %p paddr 0x%llx\n", map.vaddr, dma_addr);

	return 0;
}

static void dqe_load_context(struct exynos_dqe *dqe)
{
	int i, rgb;
	u32 val;
	u32 id = dqe->id;

	dqe_debug(dqe, "\n");

	dqe->ctx.version = dqe_reg_get_version(id);

	for (i = 0; i < DQE_GAMMA_MATRIX_REG_MAX; i++)
		dqe->ctx.gamma_matrix[i] = dqe_reg_get_lut(id, DQE_REG_GAMMA_MATRIX, i, 0);

	for (i = 0; i < DQE_DEGAMMA_REG_MAX; i++)
		dqe->ctx.degamma_lut[i] = dqe_reg_get_lut(id, DQE_REG_DEGAMMA, i, 0);

	for (i = 0; i < DQE_CGC_CON_REG_MAX; i++)
		dqe->ctx.cgc_con[i] = dqe_reg_get_lut(id, DQE_REG_CGC_CON, i, 0);

	for (i = 0; i < DQE_CGC_REG_MAX; i++) {
		for (rgb = 0; rgb < 3; rgb++) {
			val = dqe_reg_get_lut(id, DQE_REG_CGC, i, rgb); // 2 LUTs in 1 REG
			dqe->ctx.cgc_lut[2*i+1][rgb] = R2L_D(val, 2*i+1);
			dqe->ctx.cgc_lut[2*i][rgb] = R2L_D(val, 2*i);
		}
	}

	for (i = 0; i < DQE_REGAMMA_REG_MAX; i++)
		dqe->ctx.regamma_lut[i] = dqe_reg_get_lut(id, DQE_REG_REGAMMA, i, 0);

	dqe->ctx.disp_dither = dqe_reg_get_lut(id, DQE_REG_DISP_DITHER, 0, 0);
	dqe->ctx.cgc_dither = dqe_reg_get_lut(id, DQE_REG_CGC_DITHER, 0, 0);

	for (i = 0; i < DQE_HSC_REG_CTRL_MAX; i++)
		dqe->ctx.hsc_con[i] = dqe_reg_get_lut(id, DQE_REG_HSC, i, 0);
	for (i = 0; i < DQE_HSC_REG_POLY_MAX; i++)
		dqe->ctx.hsc_poly[i] = dqe_reg_get_lut(id, DQE_REG_HSC, i, 1);
	for (i = 0; i < DQE_HSC_REG_GAIN_MAX; i++)
		dqe->ctx.hsc_gain[i] = dqe_reg_get_lut(id, DQE_REG_HSC, i, 2);

	for (i = 0; i < DQE_ATC_REG_MAX; i++)
		dqe->ctx.atc[i] = dqe_reg_get_lut(id, DQE_REG_ATC, i, 0);

	for (i = 0; i < DQE_SCL_REG_MAX; i++)
		dqe->ctx.scl[i] = dqe_reg_get_lut(id, DQE_REG_SCL, i, 0);

	for (i = 0; i < DQE_DE_REG_MAX; i++)
		dqe->ctx.de[i] = dqe_reg_get_lut(id, DQE_REG_DE, i, 0);

	for (i = 0; i < DQE_RCD_REG_MAX; i++)
		dqe->ctx.rcd[i] = dqe_reg_get_lut(id, DQE_REG_RCD, i, 0);

	for (i = 0; i < DQE_LPD_REG_MAX; i++)
		dqe->ctx.lpd[i] = dqe_reg_get_lut(id, DQE_REG_LPD, i, 0);

	for (i = 0; i < DQE_REGAMMA_REG_MAX; i++)
		dqe_debug(dqe, "%08x ", dqe->ctx.regamma_lut[i]);

	for (i = 0; i < DQE_DEGAMMA_REG_MAX; i++)
		dqe_debug(dqe, "%08x ", dqe->ctx.degamma_lut[i]);

	if (IS_DQE_VER_RANGE(dqe, DQE_VER_500, DQE_VER_610))
		dqe_ctrl_auto(&dqe->ctx.disp_dither_on, true);
	else
		dqe_ctrl_auto(&dqe->ctx.cgc_dither_on, true);
	dqe_ctrl_auto(&dqe->ctx.rcd_on, true);
}

static bool dqe_check_crc(struct exynos_dqe *dqe, u8 id, const u8 attr[], u16 value)
{
	struct dqe_colormode_crc *crc = dqe->colormode.crc;
	u32 attr0 = attr[0];
	u32 attr1 = attr[1];
	u32 key = (attr0 << 16) | (attr1 << 8) | id;
	int index = id % MAX_CRC;
	u32 count = 0;

	if (!crc)
		return false;

	while (crc[index].key != 0) {
		if (key == crc[index].key) {
			if (value == crc[index].value)
				return true;
			else
				return false;
		}

		if (--index < 0)
			index = MAX_CRC - 1;

		if (++count > MAX_CRC) {
			dqe_err(dqe, "not enough crc buffer");
			return false;
		}
	}

	return false;
}

static u16 dqe_set_crc(struct exynos_dqe *dqe, u8 id, const u8 attr[], u16 value)
{
	struct dqe_colormode_crc *crc = dqe->colormode.crc;
	u32 attr0 = attr[0];
	u32 attr1 = attr[1];
	u32 key = (attr0 << 16) | (attr1 << 8) | id;
	int index = id % MAX_CRC;
	u32 count = 0;

	if (!crc)
		return 0;

	while (crc[index].key != 0) {
		if (key == crc[index].key) {
			crc[index].value = value;
			return crc[index].key;
		}

		if (--index < 0)
			index = MAX_CRC - 1;

		if (++count > MAX_CRC) {
			dqe_err(dqe, "not enough crc buffer");
			return 0;
		}
	}

	crc[index].key = key;
	crc[index].value = value;

	return key;
}

static void dqe_reset_crc(struct exynos_dqe *dqe, u8 id)
{
	struct dqe_colormode_crc *crc = dqe->colormode.crc;
	int index = 0;

	while (crc && index < MAX_CRC) {
		if ((crc[index].key & 0xFF) == (u32)id)
			crc[index].value = 0;
		index++;
	}
}

static void dqe_init_context(struct exynos_dqe *dqe, struct device *dev)
{
	int i, j, k, mode_type, ret, bpc;
	struct dqe_ctx_int *lut;

	dqe_debug(dqe, "\n");

	memset(&dqe->colormode, 0, sizeof(dqe->colormode));
	dqe->colormode.dqe_fd = -1;
	dqe->colormode.seq_num = (u8)-1;
	dqe->colormode.crc = kcalloc(MAX_CRC, sizeof(*dqe->colormode.crc), GFP_KERNEL);

	memset(&dqe->ctx, 0, sizeof(dqe->ctx));
	// changed array to pointer as exynos_drm_dqe.h header is commonly used
	dqe->ctx.gamma_matrix = kzalloc(DQE_GAMMA_MATRIX_REG_MAX*sizeof(u32), GFP_KERNEL);
	dqe->ctx.degamma_lut = kzalloc(DQE_DEGAMMA_REG_MAX*sizeof(u32), GFP_KERNEL);
	dqe->ctx.regamma_lut = kzalloc(DQE_REGAMMA_REG_MAX*sizeof(u32), GFP_KERNEL);
	dqe->ctx.cgc_con = kzalloc(DQE_CGC_CON_REG_MAX*sizeof(u32), GFP_KERNEL);
	dqe->ctx.hsc_con = kzalloc(DQE_HSC_REG_CTRL_MAX*sizeof(u32), GFP_KERNEL);
	dqe->ctx.hsc_poly = kzalloc(DQE_HSC_REG_POLY_MAX*sizeof(u32), GFP_KERNEL);
	dqe->ctx.hsc_gain = kzalloc(DQE_HSC_REG_GAIN_MAX*sizeof(u32), GFP_KERNEL);
	dqe->ctx.atc = kzalloc(DQE_ATC_REG_MAX*sizeof(u32), GFP_KERNEL);
	dqe->ctx.scl = kzalloc(DQE_SCL_REG_MAX*sizeof(u32), GFP_KERNEL);
	dqe->ctx.de = kzalloc(DQE_DE_REG_MAX*sizeof(u32), GFP_KERNEL);
	dqe->ctx.lpd = kzalloc(DQE_LPD_REG_MAX*sizeof(u32), GFP_KERNEL);
	dqe->ctx.rcd = kzalloc(DQE_RCD_REG_MAX*sizeof(u32), GFP_KERNEL);

	if (dqe->regs.edma_base_regs) {
		ret = dqe_alloc_cgc_dma(dqe, DQE_CGC_LUT_MAX*3*sizeof(u16), dev);
		if (ret < 0) {
			dqe_err(dqe, "failed to alloc cgc lut\n");
			return;
		}
	} else {
		dqe->ctx.cgc_lut = kzalloc(DQE_CGC_LUT_MAX*3*sizeof(u16), GFP_KERNEL);
	}
	dqe_load_context(dqe);

	for (mode_type = DQE_MODE_MAIN; mode_type < dqe->num_lut; mode_type++) {
		lut = &dqe_lut[mode_type];

		for (bpc = 0; bpc < DQE_REG_BPC_MAX; bpc++) {
			lut->degamma_lut[bpc][0] = 0;
			if (IS_DQE_VER(dqe, DQE_VER_500) || IS_DQE_VER(dqe, DQE_VER_610)) {
				for (i = 1; i < DQE_DEGAMMA_LUT_MAX; i++)
					lut->degamma_lut[bpc][i] = ((i - 1) % DQE_DEGAMMA_LUT_CNT) * 64;
			} else {
				for (i = 1; i < DQE_DEGAMMA_LUT_MAX; i++) {
					if ((((i - 1) / DQE_DEGAMMA_LUT_CNT) % 2) == 0) // x 10bit range
						lut->degamma_lut[bpc][i] = ((i - 1) % DQE_DEGAMMA_LUT_CNT) * 32;
					else // y 12bit range
						lut->degamma_lut[bpc][i] = ((i - 1) % DQE_DEGAMMA_LUT_CNT) * 128;
				}
				for (i = DQE_DEGAMMA_LUT_CNT; i < DQE_DEGAMMA_LUT_MAX; i += DQE_DEGAMMA_LUT_CNT)
					lut->degamma_lut[bpc][i] -= lut->degamma_lut[bpc][i-1];
			}

			lut->regamma_lut[bpc][0] = 0;
			if (IS_DQE_VER(dqe, DQE_VER_500) || IS_DQE_VER(dqe, DQE_VER_610)) {
				for (i = 1; i < DQE_REGAMMA_LUT_MAX; i++)
					lut->regamma_lut[bpc][i] = ((i - 1) % DQE_REGAMMA_LUT_CNT) * 64;
			} else {
				for (i = 1; i < DQE_REGAMMA_LUT_MAX; i++) // x,y 12bit range
					lut->regamma_lut[bpc][i] = ((i - 1) % DQE_REGAMMA_LUT_CNT) * 128;
				for (i = DQE_REGAMMA_LUT_CNT; i < DQE_REGAMMA_LUT_MAX; i += DQE_REGAMMA_LUT_CNT)
					lut->regamma_lut[bpc][i] -= lut->regamma_lut[bpc][i-1];
			}
		}

		for (i = 0; i < 17; i++)
			for (j = 0; j < 17; j++)
				for (k = 0; k < 17; k++) {
					int pos = i * 17 * 17 + j * 17 + k;

					lut->cgc17_lut[0][pos] = i * 256;
					lut->cgc17_lut[1][pos] = j * 256;
					lut->cgc17_lut[2][pos] = k * 256;
		}
	}
}

static int dqe_restore_context(struct exynos_dqe *dqe)
{
	int i, opt;
	u32 id = dqe->id;
	u64 time_s, time_c;
	bool isCgcLutUpdated = false;

	if (!dqe->cfg.cur_bpc || !dqe->cfg.width || !dqe->cfg.height) // no received bpc info yet
		return -1;

	mutex_lock(&dqe->lock);

	time_s = ktime_to_us(ktime_get());

	dqe_reg_init(dqe->id, dqe->cfg.width, dqe->cfg.height);

	/* convert to fixed 10bit by blender bit extension */
	decon_reg_set_blender_ext(dqe->decon->id, IS_BLENDER_EXT_REQUIRED(dqe));
	/* convert to 8bit if required */
	decon_reg_set_dither_enable(dqe->decon->id, IS_DISP_DITHER_REQUIRED(dqe));

	/* DQE_DISP_DITHER */
	if (IS_DQE_AUTO(&dqe->ctx.disp_dither_on, IS_DISP_DITHER_REQUIRED(dqe)) ||
		IS_DQE_ON(&dqe->ctx.disp_dither_on)) {
		dqe_reg_set_lut(id, DQE_REG_DISP_DITHER, 0, dqe->ctx.disp_dither, 0);
		dqe_reg_set_lut_on(id, DQE_REG_DISP_DITHER, 1);
	} else {
		dqe_reg_set_lut_on(id, DQE_REG_DISP_DITHER, 0);
	}

	/* DQE_CGC_DITHER */
	if (IS_DQE_AUTO(&dqe->ctx.cgc_dither_on, true) || IS_DQE_ON(&dqe->ctx.cgc_dither_on)) {
		dqe_reg_set_lut(id, DQE_REG_CGC_DITHER, 0, dqe->ctx.cgc_dither, 0);
		dqe_reg_set_lut_on(id, DQE_REG_CGC_DITHER, 1);
	} else {
		/* Dither bit must be configured when it's off */
		dqe_reg_set_lut(id, DQE_REG_CGC_DITHER, 0, dqe->ctx.cgc_dither, 0);
		dqe_reg_set_lut_on(id, DQE_REG_CGC_DITHER, 0);
	}

	/* DQE_GAMMA_MATRIX */
	if (IS_DQE_ON(&dqe->ctx.gamma_matrix_on)) {
		for (i = 1; i < DQE_GAMMA_MATRIX_REG_MAX; i++)
			dqe_reg_set_lut(id, DQE_REG_GAMMA_MATRIX, i, dqe->ctx.gamma_matrix[i], 0);
		dqe_reg_set_lut_on(id, DQE_REG_GAMMA_MATRIX, 1);
	} else {
		dqe_reg_set_lut_on(id, DQE_REG_GAMMA_MATRIX, 0);
	}

	/* DQE_INFO_DEGAMMA */
	if (IS_DQE_ON(&dqe->ctx.degamma_on)) {
		for (i = 1; i < DQE_DEGAMMA_REG_MAX; i++)
			dqe_reg_set_lut(id, DQE_REG_DEGAMMA, i, dqe->ctx.degamma_lut[i], 0);
		dqe_reg_set_lut_on(id, DQE_REG_DEGAMMA, 1);
	} else {
		dqe_reg_set_lut_on(id, DQE_REG_DEGAMMA, 0);
	}

	/* DQE_INFO_CGC & DQE_CGC_LUT */
	if (IS_DQE_ON(&dqe->ctx.cgc_on)) {
		if (IS_DQE_CGCUPDATE_REQUIRED(dqe)) {
			if (dqe->regs.edma_base_regs) {
				dqe_reg_set_lut_on(id, DQE_REG_CGC_DMA, 1);
				edma_reg_set_base_addr(id, dqe->ctx.cgc_dma);
				edma_reg_set_start(id, 1);
				dqe_reg_wait_cgc_dmareq(id);
			} else { // if no edma defined, write thru APB directly.
				for (i = 0; i < DQE_CGC_REG_MAX; i++) {
					for (opt = DQE_REG_CGC_R; opt < DQE_REG_CGC_MAX; opt++) {
						u32 val_h, val_l;

						val_h = CGC_LUT_H(dqe->ctx.cgc_lut[2*i+1][opt]);
						val_l = CGC_LUT_L(dqe->ctx.cgc_lut[2*i][opt]);
						dqe_reg_set_lut(id, DQE_REG_CGC, i, (val_h | val_l), opt);
					}
				}
			}
			isCgcLutUpdated = true;
		}
		for (i = 0; i < DQE_CGC_CON_REG_MAX; i++)
			dqe_reg_set_lut(id, DQE_REG_CGC_CON, i, dqe->ctx.cgc_con[i], 0);
		dqe_reg_set_lut_on(id, DQE_REG_CGC_CON, 1);
	} else {
		dqe_reg_set_lut_on(id, DQE_REG_CGC_CON, 0);
	}

	/* DQE_REGAMMA */
	if (IS_DQE_ON(&dqe->ctx.regamma_on)) {
		for (i = 1; i < DQE_REGAMMA_REG_MAX; i++)
			dqe_reg_set_lut(id, DQE_REG_REGAMMA, i, dqe->ctx.regamma_lut[i], 0);
		dqe_reg_set_lut_on(id, DQE_REG_REGAMMA, 1);
	} else {
		dqe_reg_set_lut_on(id, DQE_REG_REGAMMA, 0);
	}

	/* DQE_HSC */
	if (IS_DQE_ON(&dqe->ctx.hsc_on)) {
		for (i = 1; i < DQE_HSC_REG_CTRL_MAX; i++)
			dqe_reg_set_lut(id, DQE_REG_HSC, i, dqe->ctx.hsc_con[i], DQE_REG_HSC_CON);
		for (i = 0; i < DQE_HSC_REG_POLY_MAX; i++)
			dqe_reg_set_lut(id, DQE_REG_HSC, i, dqe->ctx.hsc_poly[i], DQE_REG_HSC_POLY);
		for (i = 0; i < DQE_HSC_REG_GAIN_MAX; i++)
			dqe_reg_set_lut(id, DQE_REG_HSC, i, dqe->ctx.hsc_gain[i], DQE_REG_HSC_GAIN);
		dqe_reg_set_lut(id, DQE_REG_HSC, 0, dqe->ctx.hsc_con[0], DQE_REG_HSC_CON);
		dqe_reg_set_lut_on(id, DQE_REG_HSC, 1);
	} else {
		dqe_reg_set_lut_on(id, DQE_REG_HSC, 0);
	}

	/* DQE_ATC */
	for (i = 0; i < DQE_ATC_REG_MAX; i++) {
		if (dqe->ctx.atc_dim_off && i == 8)
			dqe_reg_set_lut(id, DQE_REG_ATC, i, 0, 0);
		else
			dqe_reg_set_lut(id, DQE_REG_ATC, i, dqe->ctx.atc[i], 0);
	}

	if (IS_DQE_ON(&dqe->ctx.atc_on)) {
		dqe_reg_set_atc_img_size(id, dqe->cfg.width, dqe->cfg.height);
		dqe_reg_set_lut_on(id, DQE_REG_ATC, 1);
	} else {
		dqe_reg_set_lut_on(id, DQE_REG_ATC, 0);
	}

	/* DQE_DE */
	if (IS_DQE_ON(&dqe->ctx.de_on)) {
		for (i = 0; i < DQE_DE_REG_MAX; i++)
			dqe_reg_set_lut(id, DQE_REG_DE, i, dqe->ctx.de[i], 0);
		dqe_reg_set_lut_on(id, DQE_REG_DE, 1);
	} else {
		dqe_reg_set_lut_on(id, DQE_REG_DE, 0);
	}

	/* POST_SCL */
	if (IS_DQE_ON(&dqe->ctx.scl_on)) {
		for (i = 0; i < DQE_SCL_REG_MAX; i++)
			dqe_reg_set_lut(id, DQE_REG_SCL, i, dqe->ctx.scl[i], 0);
	}

	/* DQE_RCD */
	if (IS_DQE_AUTO(&dqe->ctx.rcd_on, dqe->cfg.rcd_en) || IS_DQE_ON(&dqe->ctx.rcd_on)) {
		for (i = 0; i < DQE_RCD_REG_MAX; i++)
			dqe_reg_set_lut(id, DQE_REG_RCD, i, dqe->ctx.rcd[i], 0);
		dqe_reg_set_lut_on(id, DQE_REG_RCD, 1);
	} else {
		dqe_reg_set_lut_on(id, DQE_REG_RCD, 0);
	}

	time_c = (ktime_to_us(ktime_get())-time_s);
	dqe_debug(dqe, "update-%d,%d/%d %lld.%03lldms di %s/%s gm %s dg %s cgc%s %s rg %s hsc %s atc %s scl %s de %s\n",
		atomic_read(&dqe->update_cnt),
		dqe->cfg.in_bpc, dqe->cfg.cur_bpc,
		time_c/USEC_PER_MSEC, time_c%USEC_PER_MSEC,
		dqe_print_onoff(&dqe->ctx.disp_dither_on),
		dqe_print_onoff(&dqe->ctx.cgc_dither_on),
		dqe_print_onoff(&dqe->ctx.gamma_matrix_on),
		dqe_print_onoff(&dqe->ctx.degamma_on),
		isCgcLutUpdated?"+":"", dqe_print_onoff(&dqe->ctx.cgc_on),
		dqe_print_onoff(&dqe->ctx.regamma_on),
		dqe_print_onoff(&dqe->ctx.hsc_on),
		dqe_print_onoff(&dqe->ctx.atc_on),
		dqe_print_onoff(&dqe->ctx.scl_on),
		dqe_print_onoff(&dqe->ctx.de_on));

	DPU_EVENT_LOG("DQE_RESTORE", dqe->decon->crtc, 0, NULL);

	mutex_unlock(&dqe->lock);

	return 0;
}

static char *dqe_acquire_colormode_ctx(struct exynos_dqe *dqe)
{
	int i, ctx_no, size, cnt;
	char *ctx;

	if (!dqe->colormode.dma_buf || !dqe->colormode.vaddr)
		return NULL;

	size = dqe->colormode.dma_buf->size;
	if (dqe->colormode.ctx_size != size) {
		for (i = 0, cnt = 0; i < MAX_DQE_COLORMODE_CTX; i++) {
			if (dqe->colormode.ctx[i])
				kfree(dqe->colormode.ctx[i]);

			dqe->colormode.ctx[i] = kzalloc(size, GFP_KERNEL | __GFP_NOWARN);
			if (dqe->colormode.ctx[i])
				cnt++;
		}
		dqe_info(dqe, "ctx realloc %d -> %d %s (%d)\n", dqe->colormode.ctx_size, size,
				(cnt == MAX_DQE_COLORMODE_CTX) ? "succeed" : "failed", cnt);
	}
	dqe->colormode.ctx_size = size;

	ctx_no = (atomic_inc_return(&dqe->colormode.ctx_no) & INT_MAX) % MAX_DQE_COLORMODE_CTX;
	ctx = dqe->colormode.ctx[ctx_no];
	if (ctx)
		memcpy(ctx, (void *)dqe->colormode.vaddr, size);
	return ctx;
}

static int dqe_alloc_colormode_dma(struct exynos_dqe *dqe,
					struct drm_crtc_state *crtc_state)
{
	struct dma_buf *buf = NULL;
	struct iosys_map map = IOSYS_MAP_INIT_VADDR(NULL);
	int64_t dqe_fd;
	int ret;
	struct exynos_drm_crtc_state *exynos_crtc_state =
					to_exynos_crtc_state(crtc_state);

	iosys_map_clear(&map);
	dqe_fd = exynos_crtc_state->dqe_fd;
	if (dqe_fd <= 0) {
		dqe_debug(dqe, "not allocated dqe fd %lld\n", dqe_fd);
		goto error;
	}

	buf = dma_buf_get(dqe_fd);
	if (IS_ERR_OR_NULL(buf)) {
		if ((dqe_fd == dqe->colormode.dqe_fd) && IS_ERR(buf)) {
			dqe_debug(dqe, "bad fd [%ld] but continued with old vbuf\n", PTR_ERR(buf));
			goto done;
		}

		dqe_warn(dqe, "failed to get dma buf [%p] of fd [%lld]\n", buf, dqe_fd);
		goto error;
	}

	if ((dqe_fd == dqe->colormode.dqe_fd) && (buf == dqe->colormode.dma_buf)) {
		dma_buf_put(buf);
		goto done;
	}

	// release old buf
	if (dqe->colormode.dma_buf) {
		if (dqe->colormode.vaddr) {
			struct iosys_map old_map =
				IOSYS_MAP_INIT_VADDR(dqe->colormode.vaddr);

			dma_buf_vunmap(dqe->colormode.dma_buf, &old_map);
		}
		dma_buf_put(dqe->colormode.dma_buf);
	}

	ret = dma_buf_vmap(buf, &map);
	if (ret) {
		dqe_err(dqe, "failed to vmap buffer [%x]\n", ret);
		goto error;
	}

	dqe->colormode.dqe_fd = dqe_fd;
	dqe->colormode.dma_buf = buf;
	dqe->colormode.vaddr = map.vaddr;

done:
	exynos_crtc_state->dqe_colormode_ctx = dqe_acquire_colormode_ctx(dqe);
	return 0;

error:
	if (!IS_ERR_OR_NULL(buf)) {
		if (!iosys_map_is_null(&map))
			dma_buf_vunmap(buf, &map);
		dma_buf_put(buf);
	}
	exynos_crtc_state->dqe_colormode_ctx = NULL;
	return -1;
}

static int dqe_prepare_context(struct exynos_dqe *dqe,
				struct drm_crtc_state *crtc_state)
{
	int ret;

	ret = dqe_alloc_colormode_dma(dqe, crtc_state);
	if (ret < 0)
		return -1;
	return 0;
}

// update LUTs from libcolor & HWC
static u32 dqe_update_colormode(struct exynos_dqe *dqe,
				struct drm_crtc_state *crtc_state)
{
	struct dqe_ctx_int *ctx_i;
	u32 updated = 0;
	int ret;
	const struct dqe_colormode_global_header *hdr;
	const struct dqe_colormode_data_header *data_h;
	const char *data;
	u32 *lut;
	u16 crc, size, len, count = 0;
	struct exynos_drm_crtc_state *exynos_crtc_state =
					to_exynos_crtc_state(crtc_state);

	if (!exynos_crtc_state->dqe_colormode_ctx)
		return DQE_UPDATED_NONE;

	hdr = (struct dqe_colormode_global_header *)exynos_crtc_state->dqe_colormode_ctx;
	if (!hdr) {
		dqe_err(dqe, "no allocated virtual buffer\n");
		goto error;
	}

	if (hdr->magic != DQE_COLORMODE_MAGIC) {
		dqe_err(dqe, "invalid header magic %x\n", hdr->magic);
		goto error;
	}

	if (!hdr->total_size || !hdr->header_size ||
			(hdr->total_size < hdr->header_size) ||
			(hdr->header_size < sizeof(*hdr))) {
		dqe_err(dqe, "invalid size: total %d, header %d\n",
					hdr->total_size, hdr->header_size);
		goto error;
	}

	if (hdr->seq_num == dqe->colormode.seq_num)
		goto done;

	dqe->colormode.seq_num = hdr->seq_num;
	crc = hdr->crc;
	size = hdr->header_size;
	while (size < hdr->total_size) {
		data_h = (struct dqe_colormode_data_header *)((char *)hdr + size);
		if (data_h->magic != DQE_COLORMODE_MAGIC) {
			dqe_err(dqe, "invalid data magic %x\n", data_h->magic);
			goto error;
		}

		if (!data_h->total_size || !data_h->header_size ||
				(data_h->total_size < data_h->header_size) ||
				(data_h->header_size < sizeof(*data_h))) {
			dqe_err(dqe, "invalid data size: total %d, header %d\n",
					data_h->total_size, data_h->header_size);
			goto error;
		}

		size += data_h->total_size;

		mutex_lock(&dqe->lock);
		if (crc == DQE_COLORMODE_MAGIC) {
			if (dqe_check_crc(dqe, data_h->id, data_h->attr, data_h->crc)) {
				count++;
				mutex_unlock(&dqe->lock);
				continue;
			}

			dqe_set_crc(dqe, data_h->id, data_h->attr, data_h->crc);
		}

		data = (const char *)((char *)data_h + data_h->header_size);

		ctx_i = &dqe_lut[DQE_MODE_MAIN];
		switch (data_h->id) {
		case DQE_COLORMODE_ID_CGC17_ENC:
			if (data_h->attr[0] > 2 || data_h->attr[1] > 16) {
				dqe_err(dqe, "invalid CGC attr %d/%d\n", data_h->attr[0], data_h->attr[1]);
				goto error_locked;
			}

			ctx_i->cgc17_enc_rgb = data_h->attr[0];
			ctx_i->cgc17_enc_idx = data_h->attr[1];
			lut = (u32 *)ctx_i;
			len = 0;
			break;
		case DQE_COLORMODE_ID_CGC17_CON:
			lut = ctx_i->cgc17_con;
			len = ARRAY_SIZE(ctx_i->cgc17_con);
			break;
		case DQE_COLORMODE_ID_DEGAMMA:
			if (data_h->attr[0] == 8) {
				ctx_i->degamma_lut_ext = 1;
				lut = ctx_i->degamma_lut[DQE_REG_BPC_8];
			} else if (data_h->attr[0] == 10) {
				lut = ctx_i->degamma_lut[DQE_REG_BPC_10];
			} else {
				ctx_i->degamma_lut_ext = 0;
				lut = ctx_i->degamma_lut[DQE_REG_BPC_10];
			}
			len = ARRAY_SIZE(ctx_i->degamma_lut[0]);
			break;
		case DQE_COLORMODE_ID_GAMMA:
			if (data_h->attr[0] == 8) {
				ctx_i->regamma_lut_ext = 1;
				lut = ctx_i->regamma_lut[DQE_REG_BPC_8];
			} else if (data_h->attr[0] == 10) {
				lut = ctx_i->regamma_lut[DQE_REG_BPC_10];
			} else {
				ctx_i->regamma_lut_ext = 0;
				lut = ctx_i->regamma_lut[DQE_REG_BPC_10];
			}
			len = ARRAY_SIZE(ctx_i->regamma_lut[0]);
			break;
		case DQE_COLORMODE_ID_CGC_DITHER:
			lut = ctx_i->cgc_dither;
			len = ARRAY_SIZE(ctx_i->cgc_dither);
			break;
		case DQE_COLORMODE_ID_GAMMA_MATRIX:
			lut = ctx_i->gamma_matrix;
			len = ARRAY_SIZE(ctx_i->gamma_matrix);
			break;
		case DQE_COLORMODE_ID_HSC48_LCG:
			if (data_h->attr[0] >= ARRAY_SIZE(ctx_i->hsc48_lcg)) {
				dqe_err(dqe, "invalid HSC attr %d\n", data_h->attr[0]);
				goto error_locked;
			}

			lut = ctx_i->hsc48_lcg[data_h->attr[0]];
			len = ARRAY_SIZE(ctx_i->hsc48_lcg[0]);
			break;
		case DQE_COLORMODE_ID_HSC:
			lut = ctx_i->hsc48_lut;
			len = DQE_HSC_LUT_CTRL_MAX+DQE_HSC_LUT_POLY_MAX;
			break;
		case DQE_COLORMODE_ID_SCL:
			lut = ctx_i->scl_input;
			len = ARRAY_SIZE(ctx_i->scl_input);
			break;
		case DQE_COLORMODE_ID_DE:
			lut = ctx_i->de_lut;
			len = ARRAY_SIZE(ctx_i->de_lut);
			break;
		default:
			lut = NULL;
			dqe_debug(dqe, "undefined id %d but continued\n", data_h->id);
			break;
		}

		if (lut) {
			if (data_h->id == DQE_COLORMODE_ID_CGC17_ENC)
				ret = dqe_conv_str2cgc(data, ctx_i);
			else
				ret = dqe_conv_str2lut(data, lut, len, false);
			if (ret < 0) {
				dqe_err(dqe, "str2lut error id %d, len %d data %s\n", data_h->id, len, data);
				goto error_locked;
			}
			updated |= BIT(data_h->id);
			count++;
		}
		mutex_unlock(&dqe->lock);
	}

	if (hdr->num_data != count)
		dqe_warn(dqe, "number of data %d vs %d mismatch\n", count, hdr->num_data);

	if (likely(updated)) {
		if (updated & BIT(DQE_COLORMODE_ID_CGC17_ENC)) {
			mutex_lock(&dqe->lock);
			ctx_i = &dqe_lut[DQE_MODE_MAIN];
			ret = dqe_dec_cgc17(ctx_i, 0x7, true);
			if (ret < 0)
				goto error_locked;
			dqe_reset_update_cnt(dqe, DQE_RESET_CGC, DQE_RESET_FULL);
			mutex_unlock(&dqe->lock);
		}

		dqe_debug(dqe, "context updated (%x)\n", updated);
		DPU_EVENT_LOG("DQE_COLORMODE", dqe->decon->crtc, 0, NULL);
		updated = DQE_UPDATED_CONTEXT;
	}
done:
	return updated;

error_locked:
	mutex_unlock(&dqe->lock);
error:
	dqe_err(dqe, "parsing error\n");
	return DQE_UPDATED_NONE;
}

static inline u32 dqe_get_mode_type(struct exynos_dqe *dqe,
					struct drm_crtc_state *crtc_state)
{
	struct exynos_drm_crtc_state *exynos_crtc_state =
					to_exynos_crtc_state(crtc_state);
	u32 mode_type, idx, attr_cm, attr_ri, color_mode, render_intent;
	u32 *attr;

	color_mode = exynos_crtc_state->color_mode;
	render_intent = exynos_crtc_state->render_intent;

	for (mode_type = DQE_MODE_PRESET; mode_type < dqe->num_lut; mode_type++) {
		attr = &dqe_lut[mode_type].mode_attr[0];
		for (idx = 0; idx < DQE_PRESET_ATTR_NUM; idx++) {
			if (attr[idx] == (u32)-1)
				continue;

			attr_cm = (attr[idx] >> DQE_PRESET_CM_SHIFT) & DQE_PRESET_ALL_MASK;
			attr_ri = (attr[idx] >> DQE_PRESET_RI_SHIFT) & DQE_PRESET_ALL_MASK;

			dqe_debug(dqe, "Preset[%d] attr (%x,%x) for (CM %d, RI %d)\n",
							mode_type, attr_cm, attr_ri, color_mode, render_intent);
			if (((attr_cm == DQE_PRESET_ALL_MASK) && (attr_ri == render_intent)) ||
				((attr_cm == color_mode) && (attr_ri == DQE_PRESET_ALL_MASK)) ||
				((attr_cm == color_mode) && (attr_ri == render_intent)))
				break;
		}

		if (idx != DQE_PRESET_ATTR_NUM)
			break;
	}

	if (mode_type == dqe->num_lut)
		mode_type = DQE_MODE_MAIN;
	else
		dqe_debug(dqe, "found preset[%d] for CM %d, RI %d\n",
					mode_type, color_mode, render_intent);
	return mode_type;
}

// update LUTs from preset LUT
static u32 dqe_update_preset(struct exynos_dqe *dqe,
				struct drm_crtc_state *crtc_state)
{
	struct dqe_ctx_int *ctx_from;
	struct dqe_ctx_int *ctx_to;
	u32 updated = 0;
	u32 mode_type = dqe_get_mode_type(dqe, crtc_state);

	if (mode_type >= dqe->num_lut || mode_type == DQE_MODE_MAIN)
		return DQE_UPDATED_NONE;

	mutex_lock(&dqe->lock);
	ctx_from = &dqe_lut[mode_type];
	ctx_to = &dqe_lut[DQE_MODE_MAIN];
	if (IS_ARRAY(ctx_from->disp_dither) && ctx_from->disp_dither[0]) {
		memcpy(ctx_to->disp_dither, ctx_from->disp_dither, sizeof(ctx_from->disp_dither));
		updated |= BIT(DQE_REG_DISP_DITHER);
	}
	if (IS_ARRAY(ctx_from->cgc_dither) && ctx_from->cgc_dither[0]) {
		memcpy(ctx_to->cgc_dither, ctx_from->cgc_dither, sizeof(ctx_from->cgc_dither));
		updated |= BIT(DQE_REG_CGC_DITHER);
	}
	if (IS_ARRAY(ctx_from->gamma_matrix) && ctx_from->gamma_matrix[0]) {
		memcpy(ctx_to->gamma_matrix, ctx_from->gamma_matrix, sizeof(ctx_from->gamma_matrix));
		updated |= BIT(DQE_REG_GAMMA_MATRIX);
	}
	if (IS_ARRAY(ctx_from->degamma_lut) && ctx_from->degamma_lut[0][0]) {
		memcpy(ctx_to->degamma_lut, ctx_from->degamma_lut, sizeof(ctx_from->degamma_lut));
		ctx_to->degamma_lut_ext = ctx_from->degamma_lut_ext;
		updated |= BIT(DQE_REG_DEGAMMA);
	}
	if (IS_ARRAY(ctx_from->regamma_lut) && ctx_from->regamma_lut[0][0]) {
		memcpy(ctx_to->regamma_lut, ctx_from->regamma_lut, sizeof(ctx_from->regamma_lut));
		ctx_to->regamma_lut_ext = ctx_from->regamma_lut_ext;
		updated |= BIT(DQE_REG_REGAMMA);
	}
	if (IS_ARRAY(ctx_from->cgc17_con) && ctx_from->cgc17_con[0]) {
		memcpy(ctx_to->cgc17_con, ctx_from->cgc17_con, sizeof(ctx_from->cgc17_con));
		memcpy(ctx_to->cgc17_lut, ctx_from->cgc17_lut, sizeof(ctx_from->cgc17_lut));
		updated |= BIT(DQE_REG_CGC);
		dqe_reset_update_cnt(dqe, DQE_RESET_CGC, DQE_RESET_FULL);
	}
	if (IS_ARRAY(ctx_from->hsc48_lut) && ctx_from->hsc48_lut[0]) {
		memcpy(ctx_to->hsc48_lut, ctx_from->hsc48_lut, sizeof(ctx_from->hsc48_lut));
		memcpy(ctx_to->hsc48_lcg, ctx_from->hsc48_lcg, sizeof(ctx_from->hsc48_lcg));
		updated |= BIT(DQE_REG_HSC);
	}
	if (IS_ARRAY(ctx_from->atc_lut) && ctx_from->atc_lut[0]) {
		memcpy(ctx_to->atc_lut, ctx_from->atc_lut, sizeof(ctx_from->atc_lut));
		updated |= BIT(DQE_REG_ATC);
	}
	if (IS_ARRAY(ctx_from->scl_input) && ctx_from->scl_input[0]) {
		memcpy(ctx_to->scl_input, ctx_from->scl_input, sizeof(ctx_from->scl_input));
		updated |= BIT(DQE_REG_SCL);
	}
	if (IS_ARRAY(ctx_from->de_lut) && ctx_from->de_lut[0]) {
		memcpy(ctx_to->de_lut, ctx_from->de_lut, sizeof(ctx_from->de_lut));
		updated |= BIT(DQE_REG_DE);
	}
	mutex_unlock(&dqe->lock);

	if (unlikely(updated)) {
		dqe_debug(dqe, "context updated (%x)\n", updated);
		DPU_EVENT_LOG("DQE_PRESET", dqe->decon->crtc, 0, NULL);
		updated = DQE_UPDATED_CONTEXT;
	}
	return updated;
}

static u32 dqe_update_context(struct exynos_dqe *dqe,
				struct drm_crtc_state *crtc_state)
{
	u32 updated = DQE_UPDATED_NONE;

	updated |= dqe_update_colormode(dqe, crtc_state);
	if (unlikely(updated)) // copy sub LUTs to Main when colormode LUT changes
		updated |= dqe_update_preset(dqe, crtc_state);
	return updated;
}

static u32 dqe_update_config(struct exynos_dqe *dqe,
				struct drm_crtc_state *crtc_state)
{
	u32 updated = DQE_UPDATED_NONE;
	u32 width, height;
	struct decon_device *decon = dqe->decon;
	struct decon_config *cfg = &decon->config;
	struct exynos_drm_crtc_state *exynos_crtc_state =
					to_exynos_crtc_state(crtc_state);

	if (decon->crtc->partial && exynos_crtc_state->partial) {
		width = drm_rect_width(&exynos_crtc_state->partial_region);
		height = drm_rect_height(&exynos_crtc_state->partial_region);
	} else {
		width = crtc_state->adjusted_mode.hdisplay;
		height = crtc_state->adjusted_mode.vdisplay;
	}

	if (unlikely(dqe->cfg.width != width) ||
		unlikely(dqe->cfg.height != height) ||
		unlikely(dqe->cfg.in_bpc != cfg->in_bpc) ||
		unlikely(dqe->cfg.out_bpc != cfg->out_bpc) ||
		unlikely(dqe->cfg.rcd_en != cfg->rcd_en)) {

		dqe->cfg.width = width;
		dqe->cfg.height = height;
		dqe->cfg.in_bpc = cfg->in_bpc;
		dqe->cfg.cur_bpc = IS_DQE_VER_RANGE(dqe, DQE_VER_500, DQE_VER_610) ?
					dqe->cfg.in_bpc : 10;
		dqe->cfg.out_bpc = cfg->out_bpc;
		dqe->cfg.rcd_en = cfg->rcd_en;
		dqe_debug(dqe, "w x h (%d x %d), bpc (%d/%d/%d) rcd %d\n",
				dqe->cfg.width, dqe->cfg.height,
				dqe->cfg.in_bpc, dqe->cfg.cur_bpc, dqe->cfg.out_bpc,
				dqe->cfg.rcd_en);
		DPU_EVENT_LOG("DQE_CONFIG", dqe->decon->crtc, 0, NULL);
		updated = DQE_UPDATED_CONFIG;
	}

	return updated;
}

static inline void dqe_get_bpc_info(struct exynos_dqe *dqe,
					u32 *shift, u32 *idx, u32 isExt)
{
	if (isExt) { // if external LUT table exits, uses the separated LUT table.
		if (shift)
			*shift = 0;
		if (idx)
			*idx = (dqe->cfg.cur_bpc == 8) ? DQE_REG_BPC_8 : DQE_REG_BPC_10;
	} else { // if no exist, down-scale of 10bit LUT table
		if (shift)
			*shift = (dqe->cfg.cur_bpc == 8) ? 2 : 0; // 8bit: 0~1023, 10bit: 0~4095;
		if (idx)
			*idx = DQE_REG_BPC_10;
	}
}

static int dqe_remap_regs(struct exynos_dqe *dqe, struct device *dev)
{
	struct device_node *np = dev->of_node;
	int i;
	u32 id = dqe->id;

	i = of_property_match_string(np, "reg-names", "dqe");
	if (i < 0) {
		dqe_err(dqe, "failed to find dqe regs\n");
		goto err;
	}

	dqe->regs.dqe_base_regs = of_iomap(np, i);
	if (!dqe->regs.dqe_base_regs) {
		dqe_err(dqe, "failed to remap dqe registers\n");
		goto err;
	}

	dqe_regs_desc_init(id, dqe->regs.dqe_base_regs, "dqe", REGS_DQE);

	i = of_property_match_string(np, "reg-names", "edma");
	if (i < 0) {
		dqe_warn(dqe, "failed to find edma regs but not an error\n");
		dqe->regs.edma_base_regs = NULL;
		return 0; // return 0 instead of err
	}

	dqe->regs.edma_base_regs = of_iomap(np, i);
	if (!dqe->regs.edma_base_regs) {
		dqe_err(dqe, "failed to remap edma registers\n");
		goto err;
	}

	dqe_regs_desc_init(id, dqe->regs.edma_base_regs, "edma", REGS_EDMA);

	return 0;

err:
	if (dqe->regs.dqe_base_regs)
		iounmap(dqe->regs.dqe_base_regs);
	if (dqe->regs.edma_base_regs)
		iounmap(dqe->regs.edma_base_regs);

	return -EINVAL;
}

static int dqe_parse_preset(struct exynos_dqe *dqe, struct device *dev)
{
	struct device_node *np = dev->of_node;
	int i, j, offset = 0, size;
	u32 cnt;

	dqe->num_lut = 1; // 1 LUT required as default
	dqe_lut = NULL;

	if (of_property_read_u32(np, "dqe_preset_cnt", &cnt) || cnt == 0)
		goto done;

	if (!of_get_property(np, "dqe_preset", &size))
		goto done;

	if ((cnt * sizeof(u32) * DQE_PRESET_ATTR_NUM) != size) {
		dqe_err(dqe, "invalid dt size %d, preset_cnt %d\n", size, cnt);
		goto err;
	}

	dqe->num_lut += cnt; // add preset LUTs
	dqe_lut = kcalloc(dqe->num_lut, sizeof(*dqe_lut), GFP_KERNEL);
	if (!dqe_lut)
		goto err;

	for (i = DQE_MODE_PRESET; i < dqe->num_lut; i++) {
		for (j = 0; j < DQE_PRESET_ATTR_NUM; j++) {
			if (of_property_read_u32_index(np, "dqe_preset", offset++,
							&dqe_lut[i].mode_attr[j]))
				goto err;
			dqe_info(dqe, "Preset[%d] Attr[%d]: %x\n", i, j,
							dqe_lut[i].mode_attr[j]);
		}
	}

done:
	if (!dqe_lut)
		dqe_lut = kcalloc(dqe->num_lut, sizeof(*dqe_lut), GFP_KERNEL);
	dqe_info(dqe, "Allocated %d internal LUTs\n", dqe->num_lut);
	return 0;

err:
	return -EINVAL;
}

static int dqe_parse_others(struct exynos_dqe *dqe, struct device *dev)
{
	struct device_node *np = dev->of_node;

	dqe->sram_retention = of_property_read_bool(np, "sram-retention");
	dqe_info(dqe, "SRAM retention %d\n", dqe->sram_retention);
	return 0;
}

static void dqe_enable_irqs(struct exynos_dqe *dqe)
{
	if (dqe->regs.edma_base_regs) {
		edma_reg_clear_irq_all(dqe->id);
		edma_reg_set_irq_mask_all(dqe->id, 1);
		edma_reg_set_irq_enable(dqe->id, 1);

		if ((dqe->irq_edma) >= 0)
			enable_irq(dqe->irq_edma);
	}
}

static void dqe_disable_irqs(struct exynos_dqe *dqe)
{
	if (dqe->regs.edma_base_regs) {
		if ((dqe->irq_edma) >= 0)
			disable_irq(dqe->irq_edma);

		edma_reg_clear_irq_all(dqe->id);
		edma_reg_set_irq_enable(dqe->id, 0);
	}
}

static irqreturn_t dqe_irq_handler(int irq, void *dev_data)
{
	struct exynos_dqe *dqe = dev_data;
	u32 irqs;

	spin_lock(&dqe->slock);
	if (dqe->regs.edma_base_regs == NULL)
		goto irq_end;
	if (dqe->state != DQE_STATE_ENABLE)
		goto irq_end;

	irqs = edma_reg_get_interrupt_and_clear(dqe->id);
	dqe_debug(dqe, "irq_sts_reg = %x\n", irqs);

irq_end:
	spin_unlock(&dqe->slock);
	return IRQ_HANDLED;
}

static int dqe_register_irqs(struct exynos_dqe *dqe, struct device *dev)
{
	struct device_node *np = dev->of_node;
	struct platform_device *pdev;
	int ret = 0;

	pdev = to_platform_device(dev);

	if (dqe->regs.edma_base_regs) {
		/* EDMA */
		dqe->irq_edma = of_irq_get_byname(np, "edma");
		dqe_info(dqe, "edma irq no = %d\n", dqe->irq_edma);
		if ((dqe->irq_edma) < 0) {
			dqe_err(dqe, "failed to find EDMA irq\n");
			return -1;
		}

		ret = devm_request_irq(dev, dqe->irq_edma, dqe_irq_handler,
				0, pdev->name, dqe);
		if (ret) {
			dqe_err(dqe, "failed to install EDMA irq\n");
			return ret;
		}
		disable_irq(dqe->irq_edma);
	}
	return ret;
}

static int dqe_init_resources(struct exynos_dqe *dqe, struct device *dev)
{
	int i;
	int ret = 0;

	ret = dqe_remap_regs(dqe, dev);
	if (ret) {
		dqe_err(dqe, "failed to remap regs\n");
		goto err;
	}

	ret = dqe_register_irqs(dqe, dev);
	if (ret) {
		dqe_err(dqe, "failed to register irqs\n");
		goto err;
	}

	ret = dqe_parse_preset(dqe, dev);
	if (ret) {
		dqe_err(dqe, "failed to parse preset info\n");
		goto err;
	}

	ret = dqe_parse_others(dqe, dev);
	if (ret) {
		dqe_err(dqe, "failed to parse dt info\n");
		goto err;
	}

	dqe->cls = class_create(THIS_MODULE, "dqe");
	if (IS_ERR_OR_NULL(dqe->cls)) {
		dqe_err(dqe, "failed to create dqe class\n");
		goto err;
	}

	dqe->dev = device_create(dqe->cls, NULL, 0, &dqe, "dqe");
	if (IS_ERR_OR_NULL(dqe->dev)) {
		dqe_err(dqe, "failed to create dqe device\n");
		goto err;
	}

	for (i = 0; dqe_attrs[i] != NULL; i++) {
		ret = device_create_file(dqe->dev, dqe_attrs[i]);
		if (ret) {
			dqe_err(dqe, "failed to create file: %s\n",
				(*dqe_attrs[i]).attr.name);
			goto err;
		}
	}

	return ret;
err:
	if (dqe->cls)
		class_destroy(dqe->cls);
	return -1;
}

/*============== CONFIGURE LUTs ===================*/
static void dqe_dec_cgc17_dpcm(int *encoded, int *ret)
{
	int ii, idx;
	int offset, maxPos;
	int dpcm[16] = {0,};
	int pos = 0;
	int mode = (encoded[0] >> 30) & 0x3;
	int shift = 0;
	int min_pos = 0;

	for (ii = 0; ii < 17; ii++)
		ret[ii] = -1;

	if (mode == 0) {
		ret[0] = (encoded[0] >> 17) & 0x1fff;
		maxPos = encoded[0] & 0xf;
		ret[maxPos + 1] = (encoded[0] >> 4) & 0x1fff;

		for (ii = 1; ii < 5; ii++) {
			dpcm[pos++] = ((encoded[ii] >> 24) & 0xff);
			dpcm[pos++] = ((encoded[ii] >> 16) & 0xff);
			dpcm[pos++] = ((encoded[ii] >> 8) & 0xff);
			dpcm[pos++] = ((encoded[ii]) & 0xff);
		}

		offset = dpcm[maxPos];
		if (offset > 128)
			offset -= 256;

		for (idx = 1, pos = 0; idx < 17; idx++, pos++) {
			if (ret[idx] == -1)
				ret[idx] = ret[idx - 1] + dpcm[pos] + offset;
		}
	} else {
		shift = (mode == 2) ? 0 : 1;
		min_pos = (encoded[0] >> 12) & 0x1f;
		ret[min_pos] = (encoded[0] >> 17) & 0x1fff;

		for (ii = 1; ii < 5; ii++) {
			dpcm[pos++] = ((encoded[ii] >> 24) & 0xff);
			dpcm[pos++] = ((encoded[ii] >> 16) & 0xff);
			dpcm[pos++] = ((encoded[ii] >> 8) & 0xff);
			dpcm[pos++] = ((encoded[ii]) & 0xff);
		}

		pos = 0;
		for (ii = 0; ii < 17; ii++) {
			if (ii != min_pos)
				ret[ii] = ret[min_pos] + (dpcm[pos++] << shift);
		}
	}
}

static int dqe_dec_cgc17(struct dqe_ctx_int *lut, int mask, int wr_mode)
{
	int ii;
	int r_pos, g_pos, b_pos;
	int decoded[17] = {0,};
	int row = ARRAY_SIZE(lut->cgc17_encoded[0]);
	int col = ARRAY_SIZE(lut->cgc17_encoded[0][0]);
	int size = ARRAY_SIZE(decoded);

	if (mask < 0x1 || mask > 0x7) {
		pr_err("%s invalid mask %x\n", __func__, mask);
		return -EINVAL;
	}

	if (mask & 0x1) {
		for (g_pos = 0; g_pos < row; g_pos++) {
			for (r_pos = 0; r_pos < col; r_pos++) {
				dqe_dec_cgc17_dpcm((int *)lut->cgc17_encoded[0][g_pos][r_pos], decoded);

				if (!wr_mode) {
					dqe_conv_lut2str((u32 *)decoded, size, NULL);
				} else {
					for (ii = 0; ii < size; ii++)
						lut->cgc17_lut[0][r_pos * row * col + g_pos * row + ii] = (u32)decoded[ii];
				}
			}
		}
	}

	if (mask & 0x2) {
		for (r_pos = 0; r_pos < row; r_pos++) {
			for (g_pos = 0; g_pos < col; g_pos++) {
				dqe_dec_cgc17_dpcm((int *)lut->cgc17_encoded[1][r_pos][g_pos], decoded);

				if (!wr_mode) {
					dqe_conv_lut2str((u32 *)decoded, size, NULL);
				} else {
					for (ii = 0; ii < size; ii++)
						lut->cgc17_lut[1][r_pos * row * col + g_pos * row + ii] = (u32)decoded[ii];
				}
			}
		}
	}

	if (mask & 0x4) {
		for (g_pos = 0; g_pos < row; g_pos++) {
			for (b_pos = 0; b_pos < col; b_pos++) {
				dqe_dec_cgc17_dpcm((int *)lut->cgc17_encoded[2][g_pos][b_pos], decoded);

				if (!wr_mode) {
					dqe_conv_lut2str((u32 *)decoded, size, NULL);
				} else {
					for (ii = 0; ii < size; ii++)
						lut->cgc17_lut[2][ii * row * col + g_pos * row + b_pos] = (u32)decoded[ii];
				}
			}
		}
	}

	return 0;
}

static void dqe_set_cgc17_lut(struct exynos_dqe *dqe,
				struct dqe_ctx_int *lut, bool clrForced)
{
	dqe_reg_set_cgc_lut(dqe->ctx.cgc_lut, lut->cgc17_lut);
	dqe_reg_set_cgc_con(dqe->ctx.cgc_con, lut->cgc17_con);
	dqe_ctrl_onoff(&dqe->ctx.cgc_on, lut->cgc17_con[0], clrForced);
	if (lut->cgc17_con[0]) {
		dqe_ctrl_onoff(&dqe->ctx.degamma_on, 1, clrForced);
		dqe_ctrl_onoff(&dqe->ctx.regamma_on, 1, clrForced);
	}
}

static void dqe_set_disp_dither(struct exynos_dqe *dqe,
				struct dqe_ctx_int *lut, bool clrForced)
{
	u32 *disp_dither = lut->disp_dither;

	dqe_reg_set_disp_dither(&dqe->ctx.disp_dither, disp_dither);
	dqe_ctrl_onoff(&dqe->ctx.disp_dither_on, disp_dither[0], clrForced);
}

static void dqe_set_cgc_dither(struct exynos_dqe *dqe,
				struct dqe_ctx_int *lut, bool clrForced)
{
	u32 *cgc_dither = lut->cgc_dither;
	int bit_ext = -1;

	if (IS_DQE_AUTO(&dqe->ctx.cgc_dither_on, true))
		bit_ext = IS_DISP_DITHER_REQUIRED(dqe) ? 1 : 0;
	dqe_reg_set_cgc_dither(&dqe->ctx.cgc_dither, cgc_dither, bit_ext);
	dqe_ctrl_onoff(&dqe->ctx.cgc_dither_on, cgc_dither[0], clrForced);
}

static void dqe_set_gamma_matrix(struct exynos_dqe *dqe,
				struct dqe_ctx_int *lut, bool clrForced)
{
	u32 shift;
	int *gamma_matrix = lut->gamma_matrix;

	dqe_get_bpc_info(dqe, &shift, NULL, 0);
	dqe_reg_set_gamma_matrix(dqe->ctx.gamma_matrix, gamma_matrix, shift);
	dqe_ctrl_onoff(&dqe->ctx.gamma_matrix_on, gamma_matrix[0], clrForced);
}

static void dqe_set_gamma_lut(struct exynos_dqe *dqe,
				struct dqe_ctx_int *lut, bool clrForced)
{
	u32 bpc, shift;
	u32 *regamma_lut;

	dqe_get_bpc_info(dqe, &shift, &bpc, lut->regamma_lut_ext);
	regamma_lut = lut->regamma_lut[bpc];
	dqe_reg_set_gamma_lut(dqe->ctx.regamma_lut, regamma_lut, shift);
	dqe_ctrl_onoff(&dqe->ctx.regamma_on, regamma_lut[0], clrForced);
}

static void dqe_set_degamma_lut(struct exynos_dqe *dqe,
				struct dqe_ctx_int *lut, bool clrForced)
{
	u32 bpc, shift;
	u32 *degamma_lut;

	dqe_get_bpc_info(dqe, &shift, &bpc, lut->degamma_lut_ext);
	degamma_lut = lut->degamma_lut[bpc];
	dqe_reg_set_degamma_lut(dqe->ctx.degamma_lut, degamma_lut, shift);
	dqe_ctrl_onoff(&dqe->ctx.degamma_on, degamma_lut[0], clrForced);
}

static void dqe_set_hsc48_lut(struct exynos_dqe *dqe,
				struct dqe_ctx_int *lut, bool clrForced)
{
	u32 shift;
	u32 *hsc48_lut = lut->hsc48_lut;

	dqe_get_bpc_info(dqe, &shift, NULL, 0);
	dqe_reg_set_hsc_lut(dqe->ctx.hsc_con, dqe->ctx.hsc_poly, dqe->ctx.hsc_gain,
				hsc48_lut, lut->hsc48_lcg, shift);
	dqe_ctrl_onoff(&dqe->ctx.hsc_on, hsc48_lut[0], clrForced);
}

static void dqe_set_aps_lut(struct exynos_dqe *dqe,
				struct dqe_ctx_int *lut, bool clrForced)
{
	u32 *atc_lut = lut->atc_lut;
	u32 shift, bpc = dqe->cfg.cur_bpc;

	dqe_get_bpc_info(dqe, &shift, NULL, 0);
	dqe_reg_set_aps_lut(dqe->ctx.atc, atc_lut, bpc, shift);
	dqe_ctrl_onoff(&dqe->ctx.atc_on, atc_lut[0], clrForced);
}

static void dqe_set_scl_lut(struct exynos_dqe *dqe,
				struct dqe_ctx_int *lut, bool clrForced)
{
	u32 *scl_input = lut->scl_input;

	dqe_reg_set_scl_lut(dqe->ctx.scl, scl_input);
	dqe_ctrl_onoff(&dqe->ctx.scl_on, scl_input[0], clrForced);
}

static void dqe_set_de_lut(struct exynos_dqe *dqe,
			struct dqe_ctx_int *lut, bool clrForced)
{
	u32 *de_lut = lut->de_lut;
	u32 shift;

	if (!IS_ARRAY(lut->de_lut))
		return;

	dqe_get_bpc_info(dqe, &shift, NULL, 0);
	dqe_reg_set_de_lut(dqe->ctx.de, de_lut, shift);

	dqe_ctrl_onoff(&dqe->ctx.de_on, de_lut[0], clrForced);
}

static void dqe_set_rcd_lut(struct exynos_dqe *dqe,
			struct dqe_ctx_int *lut, bool clrForced)
{
	u32 *rcd_lut = lut->rcd_lut;

	if (!IS_ARRAY(lut->rcd_lut))
		return;

	dqe_reg_set_rcd_lut(dqe->ctx.rcd, rcd_lut);

	dqe_ctrl_onoff(&dqe->ctx.rcd_on, rcd_lut[0], clrForced);
}

static void dqe_set_all_luts(struct exynos_dqe *dqe)
{
	struct dqe_ctx_int *lut;

	mutex_lock(&dqe->lock);
	lut = &dqe_lut[DQE_MODE_MAIN];
	dqe_set_cgc_dither(dqe, lut, false);
	dqe_set_gamma_matrix(dqe, lut, false);
	dqe_set_gamma_lut(dqe, lut, false);
	dqe_set_degamma_lut(dqe, lut, false);
	dqe_set_cgc17_lut(dqe, lut, false);
	dqe_set_hsc48_lut(dqe, lut, false);
	dqe_set_aps_lut(dqe, lut, false);
	dqe_set_scl_lut(dqe, lut, false);
	dqe_set_de_lut(dqe, lut, false);
	dqe_set_rcd_lut(dqe, lut, false);
	mutex_unlock(&dqe->lock);
}

static void dqe_save_lpd(struct exynos_dqe *dqe)
{
	int i;

	for (i = 0; i < DQE_LPD_REG_MAX; i++)
		dqe->ctx.lpd[i] = dqe_reg_get_lut(dqe->id, DQE_REG_LPD, i, 0);
}

static void dqe_restore_lpd(struct exynos_dqe *dqe)
{
	int i;
	u32 id = dqe->id;

	dqe_reg_set_lut_on(id, DQE_REG_LPD, 1);
	for (i = 0; i < DQE_LPD_REG_MAX; i++)
		dqe_reg_set_lut(id, DQE_REG_LPD, i, dqe->ctx.lpd[i], 0);
}
/*============== CONFIGURE LUTs END ===================*/

/*============== CREATE SYSFS ===================*/
#define DQE_CREATE_SYSFS_FUNC(name) \
	static ssize_t decon_dqe_ ## name ## _show(struct device *dev, \
				struct device_attribute *attr, char *buf) \
	{ \
		int ret = 0; \
		struct exynos_dqe *dqe = dev_get_drvdata(dev); \
		if (dqe == NULL || buf == NULL || dqe_lut == NULL) \
			return -1;\
		mutex_lock(&dqe->lock); \
		dqe_debug(dqe, "+\n"); \
		ret = dqe_ ## name ## _show(dqe, &dqe_lut[mode_idx], buf); \
		if (ret < 0) \
			dqe_err(dqe, "err -\n"); \
		else \
			dqe_debug(dqe, "-\n"); \
		mutex_unlock(&dqe->lock); \
		return ret; \
	} \
	static ssize_t decon_dqe_ ## name ## _store(struct device *dev, \
		struct device_attribute *attr, const char *buffer, size_t count) \
	{ \
		int ret = 0; \
		struct exynos_dqe *dqe = dev_get_drvdata(dev); \
		if (dqe == NULL || count <= 0 || buffer == NULL || dqe_lut == NULL) \
			return -1; \
		mutex_lock(&dqe->lock); \
		dqe_debug(dqe, "+\n"); \
		ret = dqe_ ## name ## _store(dqe, &dqe_lut[mode_idx], buffer, count); \
		if (ret < 0) \
			dqe_err(dqe, "err -\n"); \
		else \
			dqe_debug(dqe, "-\n"); \
		mutex_unlock(&dqe->lock); \
		return ret; \
	} \
	static DEVICE_ATTR(name, 0660, \
					decon_dqe_ ## name ## _show, \
					decon_dqe_ ## name ## _store)

#define DQE_CREATE_SYSFS_FUNC_SHOW(name) \
	static ssize_t decon_dqe_ ## name ## _show(struct device *dev, \
				struct device_attribute *attr, char *buf) \
	{ \
		int ret = 0; \
		struct exynos_dqe *dqe = dev_get_drvdata(dev); \
		if (dqe == NULL || buf == NULL || dqe_lut == NULL) \
			return -1;\
		mutex_lock(&dqe->lock); \
		dqe_debug(dqe, "+\n"); \
		ret = dqe_ ## name ## _show(dqe, &dqe_lut[mode_idx], buf); \
		if (ret < 0) \
			dqe_err(dqe, "err -\n"); \
		else \
			dqe_debug(dqe, "-\n"); \
		mutex_unlock(&dqe->lock); \
		return ret; \
	} \
	static DEVICE_ATTR(name, 0440, \
					decon_dqe_ ## name ## _show, NULL)

static ssize_t dqe_conv_lut2str(u32 *lut, int size, char *out)
{
	int i;
	char str[10] = {0,};
	ssize_t ret = 0;

	dqe_str_result[0] = '\0';
	for (i = 0 ; i < size; i++) {
		snprintf(str, 9, "%d,", lut[i]);
		strcat(dqe_str_result, str);
	}
	dqe_str_result[strlen(dqe_str_result)-1] = '\0';

	if (out)
		ret = sprintf(out, "%s\n", dqe_str_result);
	else
		pr_info("%s %s\n", __func__, dqe_str_result);
	return ret;
}

static ssize_t dqe_conv_cgc2str(struct dqe_ctx_int *lut, char *out)
{
	int i, j;
	char str[10] = {0,};
	int rgb = lut->cgc17_enc_rgb;
	int idx = lut->cgc17_enc_idx;
	int row = ARRAY_SIZE(lut->cgc17_encoded[0][0]);
	int col = ARRAY_SIZE(lut->cgc17_encoded[0][0][0]);

	dqe_str_result[0] = '\0';
	for (i = 0 ; i < row; i++)
		for (j = 0; j < col; j++) {
			snprintf(str, 9, "%08x", lut->cgc17_encoded[rgb][idx][i][j]);
			strcat(dqe_str_result, str);
		}
	dqe_str_result[strlen(dqe_str_result)] = '\0';
	return snprintf(out, PAGE_SIZE, "%s\n", dqe_str_result);
}

static inline int dqe_conv_str2uint(char *head, u32 *buf)
{
	int ret = 0;
	int tmp;

	ret = kstrtou32(head, 0, buf);
	if (ret < 0) {
		if (ret == -ERANGE) {
			pr_err("%s kstrtou32 error (%d)\n", __func__, ret);
		} else {
			ret = kstrtos32(head, 0, &tmp);
			if (ret == 0)
				*buf = (u32)tmp;
			else
				pr_err("%s kstrtos32 error (%d)\n", __func__, ret);
		}
	}

	return ret;
}

static int dqe_conv_str2lut(const char *buffer, u32 *lut, int size, bool printHex)
{
	int idx, k;
	int ret = 0;
	char *ptr = NULL;
	char *head;

	head = (char *)buffer;
	if  (*head == 0) {
		pr_err("%s buffer is null.\n", __func__);
		return -1;
	}

	pr_debug("%s %s\n", __func__, head);

	for (idx = 0; idx < size-1; idx++) {
		ptr = strchr(head, ',');
		if (ptr == NULL) {
			pr_err("%s not found comma.(%d)\n", __func__, idx);
			ret = -EINVAL;
			goto done;
		}
		*ptr = 0;
		ret = dqe_conv_str2uint(head, &lut[idx]);
		if (ret < 0)
			goto done;
		head = ptr + 1;
	}

	k = 0;
	if (*(head + k) == '-')
		k++;
	while (*(head + k) >= '0' && *(head + k) <= '9')
		k++;
	*(head + k) = 0;

	ret = dqe_conv_str2uint(head, &lut[idx]);
	if (ret < 0)
		goto done;

	for (idx = 0; idx < size; idx++) {
		if (printHex)
			pr_debug("%d ", lut[idx]);
		else
			pr_debug("%04x ", lut[idx]);
	}
done:
	return ret;
}

static int dqe_conv_str2cgc(const char *buffer, struct dqe_ctx_int *lut)
{
	int i, j, ret;
	char str[41] = {0,};
	char hex[9] = {0,};
	char *head = NULL;
	char *ptr = NULL;
	unsigned long encoded;
	int row = ARRAY_SIZE(lut->cgc17_encoded[0][0]);
	int col = ARRAY_SIZE(lut->cgc17_encoded[0][0][0]);
	int rgb = lut->cgc17_enc_rgb;
	int idx = lut->cgc17_enc_idx;

	head = (char *)buffer;
	if  (*head == 0) {
		pr_err("%s buffer is null\n", __func__);
		return -1;
	}

	pr_debug("%s %s\n", __func__, head);
	if (strlen(head) < 680 || strlen(head) > 681) {
		pr_err("%s strlen error. (%lu)\n", __func__, strlen(head));
		return -EINVAL;
	}

	if (rgb < 0 || rgb > 2 || idx < 0 || idx > 16) {
		pr_err("%s invalid rgb %d idx %d\n", __func__, rgb, idx);
		return -EINVAL;
	}

	for (i = 0 ; i < row; i++) {
		strncpy(str, head, 40);
		ptr = str;
		for (j = 0; j < col; j++) {
			strncpy(hex, ptr, 8);
			ret = kstrtoul(hex, 16, &encoded);
			if (ret < 0)
				return ret;
			lut->cgc17_encoded[rgb][idx][i][j] = (u32)encoded;
			ptr += 8;
		}

		head += 40;
	}

	return 0;
}

static ssize_t dqe_mode_idx_show(struct exynos_dqe *dqe,
				struct dqe_ctx_int *lut, char *buf)
{
	return snprintf(buf, PAGE_SIZE, "%d\n", mode_idx);
}

static ssize_t dqe_mode_idx_store(struct exynos_dqe *dqe,
				struct dqe_ctx_int *lut, const char *buffer, size_t count)
{
	int ret;
	unsigned int val;

	ret = kstrtouint(buffer, 0, &val);
	if (ret < 0) {
		dqe_err(dqe, "Failed at kstrtouint function: %d\n", ret);
		return ret;
	}

	if (val >= dqe->num_lut) {
		dqe_err(dqe, "index range error: %d\n", val);
		return -EINVAL;
	}

	mode_idx = val;
	return count;
}

DQE_CREATE_SYSFS_FUNC(mode_idx);

static ssize_t dqe_disp_dither_show(struct exynos_dqe *dqe,
					struct dqe_ctx_int *lut, char *buf)
{
	if (!IS_ARRAY(lut->cgc_dither))
		return snprintf(buf, PAGE_SIZE, "unsupported\n");

	return dqe_conv_lut2str(lut->disp_dither, ARRAY_SIZE(lut->disp_dither), buf);
}

static ssize_t dqe_disp_dither_store(struct exynos_dqe *dqe,
			struct dqe_ctx_int *lut, const char *buffer, size_t count)
{
	int ret;

	if (!IS_ARRAY(lut->disp_dither))
		return -EINVAL;

	ret = dqe_conv_str2lut(buffer, lut->disp_dither, ARRAY_SIZE(lut->disp_dither), true);
	if (ret < 0)
		return ret;
	if (mode_idx == DQE_MODE_MAIN) // do not update dqe_ctx_reg for preset LUTs
		dqe_set_disp_dither(dqe, lut, true);
	dqe_reset_update_cnt(dqe, DQE_RESET_GLOBAL, DQE_RESET_OPT);
	return count;
}

DQE_CREATE_SYSFS_FUNC(disp_dither);

static ssize_t dqe_cgc_dither_show(struct exynos_dqe *dqe,
					struct dqe_ctx_int *lut, char *buf)
{
	if (!IS_ARRAY(lut->cgc_dither))
		return snprintf(buf, PAGE_SIZE, "unsupported\n");

	return dqe_conv_lut2str(lut->cgc_dither, ARRAY_SIZE(lut->cgc_dither), buf);
}

static ssize_t dqe_cgc_dither_store(struct exynos_dqe *dqe,
			struct dqe_ctx_int *lut, const char *buffer, size_t count)
{
	int ret;

	if (!IS_ARRAY(lut->cgc_dither))
		return -EINVAL;

	ret = dqe_conv_str2lut(buffer, lut->cgc_dither, ARRAY_SIZE(lut->cgc_dither), true);
	if (ret < 0)
		return ret;
	if (mode_idx == DQE_MODE_MAIN) { // do not update dqe_ctx_reg for preset LUTs
		dqe_set_cgc_dither(dqe, lut, true);
		dqe_reset_crc(dqe, DQE_COLORMODE_ID_CGC_DITHER);
	}
	dqe_reset_update_cnt(dqe, DQE_RESET_GLOBAL, DQE_RESET_OPT);
	return count;
}

DQE_CREATE_SYSFS_FUNC(cgc_dither);

static ssize_t dqe_cgc17_idx_show(struct exynos_dqe *dqe,
				struct dqe_ctx_int *lut, char *buf)
{
	return snprintf(buf, PAGE_SIZE, "%d %d\n", lut->cgc17_enc_rgb, lut->cgc17_enc_idx);
}

static ssize_t dqe_cgc17_idx_store(struct exynos_dqe *dqe,
			struct dqe_ctx_int *lut, const char *buffer, size_t count)
{
	int ret = 0;
	int val1, val2;

	ret = sscanf(buffer, "%d %d", &val1, &val2);
	if (ret != 2) {
		dqe_err(dqe, "Failed at sscanf function: %d\n", ret);
		ret = -EINVAL;
		return ret;
	}

	if (val1 < 0 || val1 > 2 || val2 < 0 || val2 > 16) {
		dqe_err(dqe, "index range error: %d %d\n", val1, val2);
		ret = -EINVAL;
		return ret;
	}

	lut->cgc17_enc_rgb = val1;
	lut->cgc17_enc_idx = val2;
	dqe_debug(dqe, "%d %d\n", lut->cgc17_enc_rgb, lut->cgc17_enc_idx);
	return count;
}

DQE_CREATE_SYSFS_FUNC(cgc17_idx);

static ssize_t dqe_cgc17_enc_show(struct exynos_dqe *dqe,
				struct dqe_ctx_int *lut, char *buf)
{
	return dqe_conv_cgc2str(lut, buf);
}

static ssize_t dqe_cgc17_enc_store(struct exynos_dqe *dqe,
			struct dqe_ctx_int *lut, const char *buffer, size_t count)
{
	int ret;

	ret = dqe_conv_str2cgc(buffer, lut);
	if (ret < 0)
		return ret;
	return count;
}

DQE_CREATE_SYSFS_FUNC(cgc17_enc);

static ssize_t dqe_cgc17_dec_show(struct exynos_dqe *dqe,
				struct dqe_ctx_int *lut, char *buf)
{
	int ret;

	ret = dqe_dec_cgc17(lut, 0x7, false);
	return snprintf(buf, PAGE_SIZE, "dump=%d\n", ret);
}

static ssize_t dqe_cgc17_dec_store(struct exynos_dqe *dqe,
			struct dqe_ctx_int *lut, const char *buffer, size_t count)
{
	int ret;
	unsigned int value = 0;

	if (!IS_ARRAY(lut->cgc17_lut[0]))
		return -EINVAL;

	ret = kstrtouint(buffer, 0, &value);
	if (ret < 0)
		return ret;

	ret = dqe_dec_cgc17(lut, value, true);
	if (ret < 0)
		return ret;
	return count;
}

DQE_CREATE_SYSFS_FUNC(cgc17_dec);

static ssize_t dqe_cgc17_con_show(struct exynos_dqe *dqe,
				struct dqe_ctx_int *lut, char *buf)
{
	if (!IS_ARRAY(lut->cgc17_con))
		return snprintf(buf, PAGE_SIZE, "unsupported\n");

	return dqe_conv_lut2str(lut->cgc17_con, ARRAY_SIZE(lut->cgc17_con), buf);
}

static ssize_t dqe_cgc17_con_store(struct exynos_dqe *dqe,
			struct dqe_ctx_int *lut, const char *buffer, size_t count)
{
	int ret;

	if (!IS_ARRAY(lut->cgc17_con))
		return -EINVAL;

	ret = dqe_conv_str2lut(buffer, lut->cgc17_con, ARRAY_SIZE(lut->cgc17_con), true);
	if (ret < 0)
		return ret;
	if (mode_idx == DQE_MODE_MAIN) { // do not update dqe_ctx_reg for preset LUTs
		dqe_set_cgc17_lut(dqe, lut, true);
		dqe_reset_update_cnt(dqe, DQE_RESET_CGC, DQE_RESET_FULL);
		dqe_reset_crc(dqe, DQE_COLORMODE_ID_CGC17_ENC);
		dqe_reset_crc(dqe, DQE_COLORMODE_ID_CGC17_CON);
	}
	dqe_reset_update_cnt(dqe, DQE_RESET_GLOBAL, DQE_RESET_FULL);
	return count;
}

DQE_CREATE_SYSFS_FUNC(cgc17_con);

static ssize_t dqe_gamma_matrix_show(struct exynos_dqe *dqe,
					struct dqe_ctx_int *lut, char *buf)
{
	if (!IS_ARRAY(lut->gamma_matrix))
		return snprintf(buf, PAGE_SIZE, "unsupported\n");

	return dqe_conv_lut2str(lut->gamma_matrix, ARRAY_SIZE(lut->gamma_matrix), buf);
}

static ssize_t dqe_gamma_matrix_store(struct exynos_dqe *dqe,
				struct dqe_ctx_int *lut, const char *buffer, size_t count)
{
	int ret;

	if (!IS_ARRAY(lut->gamma_matrix))
		return -EINVAL;

	ret = dqe_conv_str2lut(buffer, lut->gamma_matrix, ARRAY_SIZE(lut->gamma_matrix), false);
	if (ret < 0)
		return ret;
	if (mode_idx == DQE_MODE_MAIN) { // do not update dqe_ctx_reg for preset LUTs
		dqe_set_gamma_matrix(dqe, lut, true);
		dqe_reset_crc(dqe, DQE_COLORMODE_ID_GAMMA_MATRIX);
	}
	dqe_reset_update_cnt(dqe, DQE_RESET_GLOBAL, DQE_RESET_OPT);
	return count;
}

DQE_CREATE_SYSFS_FUNC(gamma_matrix);

static ssize_t dqe_gamma_ext_show(struct exynos_dqe *dqe,
				struct dqe_ctx_int *lut, char *buf)
{
	return snprintf(buf, PAGE_SIZE, "%d\n", lut->regamma_lut_ext);
}

static ssize_t dqe_gamma_ext_store(struct exynos_dqe *dqe,
			struct dqe_ctx_int *lut, const char *buffer, size_t count)
{
	int ret = 0;
	unsigned int value = 0;

	ret = kstrtouint(buffer, 0, &value);
	if (ret < 0) {
		dqe_err(dqe, "Failed at kstrtouint function: %d\n", ret);
		return ret;
	}

	if (value > 1) {
		dqe_err(dqe, "index range error: %d\n", value);
		ret = -EINVAL;
		return ret;
	}

	lut->regamma_lut_ext = value;
	dqe_info(dqe, "regamma extension %d\n", lut->regamma_lut_ext);
	return count;
}

DQE_CREATE_SYSFS_FUNC(gamma_ext);

static ssize_t dqe_gamma_show(struct exynos_dqe *dqe,
				struct dqe_ctx_int *lut, char *buf)
{
	u32 bpc = (lut->regamma_lut_ext) ? DQE_REG_BPC_8 : DQE_REG_BPC_10;

	if (!IS_ARRAY(lut->regamma_lut[bpc]))
		return snprintf(buf, PAGE_SIZE, "unsupported\n");

	return dqe_conv_lut2str(lut->regamma_lut[bpc], ARRAY_SIZE(lut->regamma_lut[0]), buf);
}

static ssize_t dqe_gamma_store(struct exynos_dqe *dqe,
			struct dqe_ctx_int *lut, const char *buffer, size_t count)
{
	int ret;
	u32 bpc = (lut->regamma_lut_ext) ? DQE_REG_BPC_8 : DQE_REG_BPC_10;

	if (!IS_ARRAY(lut->regamma_lut[bpc]))
		return -EINVAL;

	ret = dqe_conv_str2lut(buffer, lut->regamma_lut[bpc], ARRAY_SIZE(lut->regamma_lut[0]), false);
	if (ret < 0)
		return ret;
	if (mode_idx == DQE_MODE_MAIN) { // do not update dqe_ctx_reg for preset LUTs
		dqe_set_gamma_lut(dqe, lut, true);
		dqe_reset_crc(dqe, DQE_COLORMODE_ID_GAMMA);
	}
	dqe_reset_update_cnt(dqe, DQE_RESET_GLOBAL, DQE_RESET_OPT);
	return count;
}

DQE_CREATE_SYSFS_FUNC(gamma);

static ssize_t dqe_degamma_ext_show(struct exynos_dqe *dqe,
					struct dqe_ctx_int *lut, char *buf)
{
	return snprintf(buf, PAGE_SIZE, "%d\n", lut->degamma_lut_ext);
}

static ssize_t dqe_degamma_ext_store(struct exynos_dqe *dqe,
			struct dqe_ctx_int *lut, const char *buffer, size_t count)
{
	int ret = 0;
	unsigned int value = 0;

	ret = kstrtouint(buffer, 0, &value);
	if (ret < 0) {
		dqe_err(dqe, "Failed at kstrtouint function: %d\n", ret);
		return ret;
	}

	if (value > 1) {
		dqe_err(dqe, "index range error: %d\n", value);
		ret = -EINVAL;
		return ret;
	}

	lut->degamma_lut_ext = value;
	dqe_info(dqe, "degamma extension %d\n", lut->degamma_lut_ext);
	return count;
}

DQE_CREATE_SYSFS_FUNC(degamma_ext);

static ssize_t dqe_degamma_show(struct exynos_dqe *dqe,
				struct dqe_ctx_int *lut, char *buf)
{
	u32 bpc = (lut->degamma_lut_ext) ? DQE_REG_BPC_8 : DQE_REG_BPC_10;

	if (!IS_ARRAY(lut->degamma_lut[bpc]))
		return snprintf(buf, PAGE_SIZE, "unsupported\n");

	return dqe_conv_lut2str(lut->degamma_lut[bpc], ARRAY_SIZE(lut->degamma_lut[0]), buf);
}

static ssize_t dqe_degamma_store(struct exynos_dqe *dqe,
			struct dqe_ctx_int *lut, const char *buffer, size_t count)
{
	int ret;
	u32 bpc = (lut->degamma_lut_ext) ? DQE_REG_BPC_8 : DQE_REG_BPC_10;

	if (!IS_ARRAY(lut->degamma_lut[bpc]))
		return -EINVAL;

	ret = dqe_conv_str2lut(buffer, lut->degamma_lut[bpc], ARRAY_SIZE(lut->degamma_lut[0]), false);
	if (ret < 0)
		return ret;
	if (mode_idx == DQE_MODE_MAIN) { // do not update dqe_ctx_reg for preset LUTs
		dqe_set_degamma_lut(dqe, lut, true);
		dqe_reset_crc(dqe, DQE_COLORMODE_ID_DEGAMMA);
	}
	dqe_reset_update_cnt(dqe, DQE_RESET_GLOBAL, DQE_RESET_OPT);
	return count;
}

DQE_CREATE_SYSFS_FUNC(degamma);

static ssize_t dqe_hsc48_idx_show(struct exynos_dqe *dqe,
				struct dqe_ctx_int *lut, char *buf)
{
	return snprintf(buf, PAGE_SIZE, "%d\n", lut->hsc48_lcg_idx);
}

static ssize_t dqe_hsc48_idx_store(struct exynos_dqe *dqe,
			struct dqe_ctx_int *lut, const char *buffer, size_t count)
{
	int ret = 0;
	unsigned int value = 0;

	ret = kstrtouint(buffer, 0, &value);
	if (ret < 0) {
		dqe_err(dqe, "Failed at kstrtouint function: %d\n", ret);
		return ret;
	}

	if (value > 2) {
		dqe_err(dqe, "index range error: %d\n", value);
		ret = -EINVAL;
		return ret;
	}

	lut->hsc48_lcg_idx = value;
	dqe_info(dqe, "lcg idx %d\n", lut->hsc48_lcg_idx);
	return count;
}

DQE_CREATE_SYSFS_FUNC(hsc48_idx);

static ssize_t dqe_hsc48_lcg_show(struct exynos_dqe *dqe,
				struct dqe_ctx_int *lut, char *buf)
{
	int idx = lut->hsc48_lcg_idx;

	if (!IS_ARRAY(lut->hsc48_lcg[idx]))
		return snprintf(buf, PAGE_SIZE, "unsupported\n");

	return dqe_conv_lut2str(lut->hsc48_lcg[idx], ARRAY_SIZE(lut->hsc48_lcg[0]), buf);
}

static ssize_t dqe_hsc48_lcg_store(struct exynos_dqe *dqe,
			struct dqe_ctx_int *lut, const char *buffer, size_t count)
{
	int ret;
	int idx = lut->hsc48_lcg_idx;

	if (!IS_ARRAY(lut->hsc48_lcg[idx]))
		return -EINVAL;

	ret = dqe_conv_str2lut(buffer, lut->hsc48_lcg[idx], ARRAY_SIZE(lut->hsc48_lcg[0]), true);
	if (ret < 0)
		return ret;
	return count;
}

DQE_CREATE_SYSFS_FUNC(hsc48_lcg);

static ssize_t dqe_hsc_show(struct exynos_dqe *dqe,
			struct dqe_ctx_int *lut, char *buf)
{
	if (!IS_ARRAY(lut->hsc48_lut))
		return snprintf(buf, PAGE_SIZE, "unsupported\n");

	return dqe_conv_lut2str(lut->hsc48_lut, DQE_HSC_LUT_CTRL_MAX+DQE_HSC_LUT_POLY_MAX, buf);
}

static ssize_t dqe_hsc_store(struct exynos_dqe *dqe,
			struct dqe_ctx_int *lut, const char *buffer, size_t count)
{
	int ret;

	if (!IS_ARRAY(lut->hsc48_lut))
		return -EINVAL;

	ret = dqe_conv_str2lut(buffer, lut->hsc48_lut, DQE_HSC_LUT_CTRL_MAX+DQE_HSC_LUT_POLY_MAX, true);
	if (ret < 0)
		return ret;
	if (mode_idx == DQE_MODE_MAIN) { // do not update dqe_ctx_reg for preset LUTs
		dqe_set_hsc48_lut(dqe, lut, true);
		dqe_reset_crc(dqe, DQE_COLORMODE_ID_HSC48_LCG);
		dqe_reset_crc(dqe, DQE_COLORMODE_ID_HSC);
	}
	dqe_reset_update_cnt(dqe, DQE_RESET_GLOBAL, DQE_RESET_OPT);
	return count;
}

DQE_CREATE_SYSFS_FUNC(hsc);

static ssize_t dqe_aps_show(struct exynos_dqe *dqe,
			struct dqe_ctx_int *lut, char *buf)
{
	if (!IS_ARRAY(lut->atc_lut))
		return snprintf(buf, PAGE_SIZE, "unsupported\n");

	return dqe_conv_lut2str(lut->atc_lut, ARRAY_SIZE(lut->atc_lut), buf);
}

static ssize_t dqe_aps_store(struct exynos_dqe *dqe,
			struct dqe_ctx_int *lut, const char *buffer, size_t count)
{
	int ret;

	if (!IS_ARRAY(lut->atc_lut))
		return -EINVAL;

	ret = dqe_conv_str2lut(buffer, lut->atc_lut, ARRAY_SIZE(lut->atc_lut), false);
	if (ret < 0)
		return ret;
	if (mode_idx == DQE_MODE_MAIN) // do not update dqe_ctx_reg for preset LUTs
		dqe_set_aps_lut(dqe, lut, true);
	dqe_reset_update_cnt(dqe, DQE_RESET_GLOBAL, DQE_RESET_OPT);
	return count;
}

DQE_CREATE_SYSFS_FUNC(aps);

static ssize_t dqe_aps_onoff_show(struct exynos_dqe *dqe,
				struct dqe_ctx_int *lut, char *buf)
{
	return snprintf(buf, PAGE_SIZE, "aps_onoff = %s\n",
					dqe_print_onoff(&dqe->ctx.atc_on));
}

static ssize_t dqe_aps_onoff_store(struct exynos_dqe *dqe,
			struct dqe_ctx_int *lut, const char *buffer, size_t count)
{
	int ret;
	unsigned int value = 0;

	ret = kstrtouint(buffer, 0, &value);
	if (ret < 0)
		return ret;
	dqe_ctrl_forcedoff(&dqe->ctx.atc_on, (value == 0) ? 1 : 0);
	dqe_reset_update_cnt(dqe, DQE_RESET_GLOBAL, DQE_RESET_OPT);
	return count;
}

DQE_CREATE_SYSFS_FUNC(aps_onoff);

static ssize_t dqe_aps_lux_show(struct exynos_dqe *dqe,
				struct dqe_ctx_int *lut, char *buf)
{
	return snprintf(buf, PAGE_SIZE, "lux %d\n", dqe->ctx.atc_lux);
}

static ssize_t dqe_aps_lux_store(struct exynos_dqe *dqe,
			struct dqe_ctx_int *lut, const char *buffer, size_t count)
{
	int ret;
	unsigned int value = 0;

	ret = kstrtouint(buffer, 0, &value);
	if (ret < 0)
		return ret;
	dqe->ctx.atc_lux = value;
	dqe_info(dqe, "lux %d\n", dqe->ctx.atc_lux);
	return count;
}

DQE_CREATE_SYSFS_FUNC(aps_lux);

static ssize_t dqe_aps_dim_off_show(struct exynos_dqe *dqe,
				struct dqe_ctx_int *lut, char *buf)
{
	return snprintf(buf, PAGE_SIZE, "aps_dim_off=%d\n", dqe->ctx.atc_dim_off);
}

static ssize_t dqe_aps_dim_off_store(struct exynos_dqe *dqe,
			struct dqe_ctx_int *lut, const char *buffer, size_t count)
{
	int ret;
	unsigned int value = 0;

	ret = kstrtouint(buffer, 0, &value);
	if (ret < 0)
		return ret;
	dqe->ctx.atc_dim_off = value;
	dqe_info(dqe, "%d\n", value);
	return count;
}

DQE_CREATE_SYSFS_FUNC(aps_dim_off);

static ssize_t dqe_scl_show(struct exynos_dqe *dqe,
			struct dqe_ctx_int *lut, char *buf)
{
	if (!IS_ARRAY(lut->scl_input) || ARRAY_SIZE(lut->scl_input) == 1)
		return snprintf(buf, PAGE_SIZE, "unsupported\n");

	return dqe_conv_lut2str(lut->scl_input, ARRAY_SIZE(lut->scl_input), buf);
}

static ssize_t dqe_scl_store(struct exynos_dqe *dqe,
			struct dqe_ctx_int *lut, const char *buffer, size_t count)
{
	int ret;

	if (!IS_ARRAY(lut->scl_input) || ARRAY_SIZE(lut->scl_input) == 1)
		return -EINVAL;

	ret = dqe_conv_str2lut(buffer, lut->scl_input, ARRAY_SIZE(lut->scl_input), true);
	if (ret < 0)
		return ret;
	if (mode_idx == DQE_MODE_MAIN) { // do not update dqe_ctx_reg for preset LUTs
		dqe_set_scl_lut(dqe, lut, true);
		dqe_reset_crc(dqe, DQE_COLORMODE_ID_SCL);
	}
	dqe_reset_update_cnt(dqe, DQE_RESET_GLOBAL, DQE_RESET_OPT);
	return count;
}

DQE_CREATE_SYSFS_FUNC(scl);

static ssize_t dqe_de_show(struct exynos_dqe *dqe,
			struct dqe_ctx_int *lut, char *buf)
{
	if (!IS_ARRAY(lut->de_lut))
		return snprintf(buf, PAGE_SIZE, "unsupported\n");

	return dqe_conv_lut2str(lut->de_lut, ARRAY_SIZE(lut->de_lut), buf);
}

static ssize_t dqe_de_store(struct exynos_dqe *dqe,
			struct dqe_ctx_int *lut, const char *buffer, size_t count)
{
	int ret;

	if (!IS_ARRAY(lut->de_lut))
		return -EINVAL;

	ret = dqe_conv_str2lut(buffer, lut->de_lut, ARRAY_SIZE(lut->de_lut), false);
	if (ret < 0)
		return ret;
	if (mode_idx == DQE_MODE_MAIN) { // do not update dqe_ctx_reg for preset LUTs
		dqe_set_de_lut(dqe, lut, true);
		dqe_reset_crc(dqe, DQE_COLORMODE_ID_DE);
	}
	dqe_reset_update_cnt(dqe, DQE_RESET_GLOBAL, DQE_RESET_OPT);
	return count;
}

DQE_CREATE_SYSFS_FUNC(de);

static ssize_t dqe_rcd_show(struct exynos_dqe *dqe,
			struct dqe_ctx_int *lut, char *buf)
{
	if (!IS_ARRAY(lut->rcd_lut))
		return snprintf(buf, PAGE_SIZE, "unsupported\n");

	return dqe_conv_lut2str(lut->rcd_lut, ARRAY_SIZE(lut->rcd_lut), buf);
}

static ssize_t dqe_rcd_store(struct exynos_dqe *dqe,
			struct dqe_ctx_int *lut, const char *buffer, size_t count)
{
	int ret;

	if (!IS_ARRAY(lut->rcd_lut))
		return -EINVAL;

	ret = dqe_conv_str2lut(buffer, lut->rcd_lut, ARRAY_SIZE(lut->rcd_lut), false);
	if (ret < 0)
		return ret;
	if (mode_idx == DQE_MODE_MAIN) { // do not update dqe_ctx_reg for preset LUTs
		dqe_set_rcd_lut(dqe, lut, true);
	}
	dqe_reset_update_cnt(dqe, DQE_RESET_GLOBAL, DQE_RESET_OPT);
	return count;
}

DQE_CREATE_SYSFS_FUNC(rcd);

static ssize_t dqe_color_mode_show(struct exynos_dqe *dqe,
				struct dqe_ctx_int *lut, char *buf)
{
	int val = 0;

	return snprintf(buf, PAGE_SIZE, "color_mode = %d\n", val);
}

static ssize_t dqe_color_mode_store(struct exynos_dqe *dqe,
			struct dqe_ctx_int *lut, const char *buffer, size_t count)
{
	int ret;
	unsigned int value = 0;

	ret = kstrtouint(buffer, 0, &value);
	if (ret < 0)
		return ret;
	dqe_info(dqe, "color mode %d\n", value);
	return count;
}

DQE_CREATE_SYSFS_FUNC(color_mode);

enum dqe_off_ctrl {
	DQE_OFF_CTRL_ALL = 0,
	DQE_OFF_CTRL_CGC,
	DQE_OFF_CTRL_DEGAMMA,
	DQE_OFF_CTRL_REGAMMA,
	DQE_OFF_CTRL_GAMMA_MATRIX,
	DQE_OFF_CTRL_HSC,
	DQE_OFF_CTRL_ATC,
	DQE_OFF_CTRL_CGC_DITHER,
	DQE_OFF_CTRL_DISP_DITHER,
	DQE_OFF_CTRL_DE,
	DQE_OFF_CTRL_RCD,
	DQE_OFF_CTRL_MAX
};

static const char *dqe_off_ctrl_name[DQE_OFF_CTRL_MAX] = {
	"ALL",
	"CGC",
	"DEGAMMA",
	"REGAMMA",
	"GAMMA_MATRIX",
	"HSC",
	"ATC",
	"CGC DITH",
	"DISP DITH",
	"DE",
	"RCD",
};

static ssize_t dqe_off_ctrl_show(struct exynos_dqe *dqe,
				struct dqe_ctx_int *lut, char *buf)
{
	char str[24] = {0,};
	int i;
	u32 value;

	dqe_str_result[0] = '\0';
	for (i = 0 ; i < DQE_OFF_CTRL_MAX; i++) {
		switch (i) {
		case DQE_OFF_CTRL_CGC:
			value = dqe->ctx.cgc_on;
			break;
		case DQE_OFF_CTRL_DEGAMMA:
			value = dqe->ctx.degamma_on;
			break;
		case DQE_OFF_CTRL_REGAMMA:
			value = dqe->ctx.regamma_on;
			break;
		case DQE_OFF_CTRL_GAMMA_MATRIX:
			value = dqe->ctx.gamma_matrix_on;
			break;
		case DQE_OFF_CTRL_HSC:
			value = dqe->ctx.hsc_on;
			break;
		case DQE_OFF_CTRL_ATC:
			value = dqe->ctx.atc_on;
			break;
		case DQE_OFF_CTRL_CGC_DITHER:
			value = dqe->ctx.cgc_dither_on;
			break;
		case DQE_OFF_CTRL_DISP_DITHER:
			value = dqe->ctx.disp_dither_on;
			break;
		case DQE_OFF_CTRL_DE:
			value = dqe->ctx.de_on;
			break;
		case DQE_OFF_CTRL_RCD:
			value = dqe->ctx.rcd_on;
			break;
		case DQE_OFF_CTRL_ALL:
		default:
			value = (u32)-1;
			break;
		}
		if (value == (u32)-1)
			snprintf(str, 24, "%s(%d),", dqe_off_ctrl_name[i], i);
		else
			snprintf(str, 24, "%s(%d):%s,", dqe_off_ctrl_name[i], i, dqe_print_onoff(&value));
		strcat(dqe_str_result, str);
	}
	dqe_str_result[strlen(dqe_str_result)-1] = '\0';
	return snprintf(buf, PAGE_SIZE, "%s\n", dqe_str_result);
}

static ssize_t dqe_off_ctrl_store(struct exynos_dqe *dqe,
			struct dqe_ctx_int *lut, const char *buffer, size_t count)
{
	int ret;
	int type, forced = 0;

	ret = sscanf(buffer, "%d %d", &type, &forced);
	if (ret < 1) {
		dqe_err(dqe, "Failed at sscanf function: %d\n", ret);
		ret = -EINVAL;
		return ret;
	}

	if (ret == 1)
		forced = 1; // if arg is 1, set forced off

	switch (type) {
	case DQE_OFF_CTRL_ALL:
		dqe_ctrl_forcedoff(&dqe->ctx.gamma_matrix_on, forced);
		dqe_ctrl_forcedoff(&dqe->ctx.degamma_on, forced);
		dqe_ctrl_forcedoff(&dqe->ctx.cgc_on, forced);
		dqe_ctrl_forcedoff(&dqe->ctx.regamma_on, forced);
		dqe_ctrl_forcedoff(&dqe->ctx.hsc_on, forced);
		dqe_ctrl_forcedoff(&dqe->ctx.atc_on, forced);
		dqe_ctrl_forcedoff(&dqe->ctx.de_on, forced);
		break;
	case DQE_OFF_CTRL_CGC:
		dqe_ctrl_forcedoff(&dqe->ctx.cgc_on, forced);
		break;
	case DQE_OFF_CTRL_DEGAMMA:
		dqe_ctrl_forcedoff(&dqe->ctx.degamma_on, forced);
		break;
	case DQE_OFF_CTRL_REGAMMA:
		dqe_ctrl_forcedoff(&dqe->ctx.regamma_on, forced);
		break;
	case DQE_OFF_CTRL_GAMMA_MATRIX:
		dqe_ctrl_forcedoff(&dqe->ctx.gamma_matrix_on, forced);
		break;
	case DQE_OFF_CTRL_HSC:
		dqe_ctrl_forcedoff(&dqe->ctx.hsc_on, forced);
		break;
	case DQE_OFF_CTRL_ATC:
		dqe_ctrl_forcedoff(&dqe->ctx.atc_on, forced);
		break;
	case DQE_OFF_CTRL_CGC_DITHER:
		dqe_ctrl_forcedoff(&dqe->ctx.cgc_dither_on, forced);
		break;
	case DQE_OFF_CTRL_DISP_DITHER:
		dqe_ctrl_forcedoff(&dqe->ctx.disp_dither_on, forced);
		break;
	case DQE_OFF_CTRL_DE:
		dqe_ctrl_forcedoff(&dqe->ctx.de_on, forced);
		break;
	case DQE_OFF_CTRL_RCD:
		dqe_ctrl_forcedoff(&dqe->ctx.rcd_on, forced);
		break;
	default:
		break;
	}

	dqe_reset_update_cnt(dqe, DQE_RESET_GLOBAL, DQE_RESET_FULL);
	return count;
}

DQE_CREATE_SYSFS_FUNC(off_ctrl);

static ssize_t dqe_xml_show(struct exynos_dqe *dqe,
			struct dqe_ctx_int *lut, char *buf)
{
	if (strlen(dqe->xml_suffix) <= 0)
		return 0;
	sprintf(dqe_str_result, "%s.xml", dqe->xml_suffix);
	pr_info("%s xml_path: %s\n", __func__, dqe_str_result);

	return sprintf(buf, "%s\n", dqe_str_result);
}

DQE_CREATE_SYSFS_FUNC_SHOW(xml);

static ssize_t dqe_dqe_ver_show(struct exynos_dqe *dqe,
				struct dqe_ctx_int *lut, char *buf)
{
	return snprintf(buf, PAGE_SIZE, "%08X\n", dqe->ctx.version);
}

DQE_CREATE_SYSFS_FUNC_SHOW(dqe_ver);

static ssize_t dqe_dim_status_show(struct exynos_dqe *dqe,
				struct dqe_ctx_int *lut, char *buf)
{
	const struct decon_device *decon = dqe->decon;

	return snprintf(buf, PAGE_SIZE, "%d\n", decon->dimming);
}

DQE_CREATE_SYSFS_FUNC_SHOW(dim_status);

static ssize_t dqe_dump_show(struct exynos_dqe *dqe,
				struct dqe_ctx_int *lut, char *buf)
{
	int val = 0;
	struct decon_device *decon = dqe->decon;

	hibernation_block_exit(decon->crtc->hibernation);
	__dqe_dump(dqe->id, &dqe->regs, true);
	hibernation_unblock(decon->crtc->hibernation);
	return snprintf(buf, PAGE_SIZE, "dump = %d\n", val);
}

DQE_CREATE_SYSFS_FUNC_SHOW(dump);

static struct device_attribute *dqe_attrs[] = {
	&dev_attr_mode_idx,
	&dev_attr_disp_dither,
	&dev_attr_cgc_dither,
	&dev_attr_cgc17_idx,
	&dev_attr_cgc17_enc,
	&dev_attr_cgc17_dec,
	&dev_attr_cgc17_con,
	&dev_attr_gamma_matrix,
	&dev_attr_gamma_ext,
	&dev_attr_gamma,
	&dev_attr_degamma_ext,
	&dev_attr_degamma,
	&dev_attr_hsc48_idx,
	&dev_attr_hsc48_lcg,
	&dev_attr_hsc,
	&dev_attr_aps,
	&dev_attr_aps_onoff,
	&dev_attr_aps_lux,
	&dev_attr_aps_dim_off,
	&dev_attr_scl,
	&dev_attr_de,
	&dev_attr_rcd,
	&dev_attr_color_mode,
	&dev_attr_off_ctrl,
	&dev_attr_xml,
	&dev_attr_dqe_ver,
	&dev_attr_dim_status,
	&dev_attr_dump,
	NULL,
};
/*============== CREATE SYSFS END ===================*/
static void __exynos_dqe_dump(struct exynos_dqe *dqe)
{
	__dqe_dump(dqe->id, &dqe->regs, false);
}

static void __exynos_dqe_prepare(struct exynos_dqe *dqe,
				struct drm_crtc_state *crtc_state)
{
	dqe_debug(dqe, "+\n");
	dqe_prepare_context(dqe, crtc_state);
	dqe_debug(dqe, "-\n");
}

static void __exynos_dqe_update(struct exynos_dqe *dqe,
				struct drm_crtc_state *crtc_state)
{
	u32 updated = DQE_UPDATED_NONE;

	dqe_debug(dqe, "+\n");

	updated |= dqe_update_context(dqe, crtc_state);
	updated |= dqe_update_config(dqe, crtc_state);

	if (unlikely(updated)) {
		dqe_set_all_luts(dqe);
		dqe_reset_update_cnt(dqe, DQE_RESET_GLOBAL, DQE_RESET_FULL);
	}

	if (IS_DQE_UPDATE_REQUIRED(dqe))
		dqe_restore_context(dqe);

	dqe_debug(dqe, "-\n");
}

static void __exynos_dqe_enable(struct exynos_dqe *dqe)
{
	dqe_debug(dqe, "+\n");
	dqe_restore_lpd(dqe);
	dqe_reset_update_cnt(dqe, DQE_RESET_GLOBAL, DQE_RESET_FULL);
	dqe_reset_update_cnt(dqe, DQE_RESET_CGC, DQE_RESET_OPT);
	dqe_enable_irqs(dqe);
	dqe->state = DQE_STATE_ENABLE;
	dqe_debug(dqe, "-\n");
}

static void __exynos_dqe_disable(struct exynos_dqe *dqe)
{
	dqe_debug(dqe, "+\n");
	dqe_save_lpd(dqe);
	dqe_disable_irqs(dqe);
	dqe->state = DQE_STATE_DISABLE;
	dqe_debug(dqe, "-\n");
}

static void __exynos_dqe_suspend(struct exynos_dqe *dqe)
{
	dqe_debug(dqe, "+\n");
	dqe_debug(dqe, "-\n");
}

static void __exynos_dqe_resume(struct exynos_dqe *dqe)
{
	dqe_debug(dqe, "+\n");
	// SRAM retention reset on sleep mif down
	dqe_reset_update_cnt(dqe, DQE_RESET_CGC, DQE_RESET_FULL);
	dqe_debug(dqe, "-\n");
}

static void __exynos_dqe_state(struct exynos_dqe *dqe,
				enum dqe_reg_type type, bool *enabled)
{
	dqe_debug(dqe, "+\n");
	mutex_lock(&dqe->lock);
	switch (type) {
	case DQE_REG_GAMMA_MATRIX:
		*enabled = IS_DQE_ON(&dqe->ctx.gamma_matrix_on);
		break;
	case DQE_REG_DISP_DITHER:
		*enabled = IS_DQE_ON(&dqe->ctx.disp_dither_on);
		break;
	case DQE_REG_CGC_DITHER:
		*enabled = IS_DQE_ON(&dqe->ctx.cgc_dither_on);
		break;
	case DQE_REG_CGC_CON:
	case DQE_REG_CGC_DMA:
	case DQE_REG_CGC:
		*enabled = IS_DQE_ON(&dqe->ctx.cgc_on);
		break;
	case DQE_REG_DEGAMMA:
		*enabled = IS_DQE_ON(&dqe->ctx.degamma_on);
		break;
	case DQE_REG_REGAMMA:
		*enabled = IS_DQE_ON(&dqe->ctx.regamma_on);
		break;
	case DQE_REG_HSC:
		*enabled = IS_DQE_ON(&dqe->ctx.hsc_on);
		break;
	case DQE_REG_ATC:
		*enabled = IS_DQE_ON(&dqe->ctx.atc_on);
		break;
	case DQE_REG_SCL:
		*enabled = IS_DQE_ON(&dqe->ctx.scl_on);
		break;
	case DQE_REG_DE:
		*enabled = IS_DQE_ON(&dqe->ctx.de_on);
		break;
	case DQE_REG_RCD:
		*enabled = IS_DQE_ON(&dqe->ctx.rcd_on);
		break;
	default:
		*enabled = false;
		break;
	}
	mutex_unlock(&dqe->lock);
	dqe_debug(dqe, "-\n");
}

static const struct exynos_dqe_funcs dqe_funcs = {
	.dump = __exynos_dqe_dump,
	.prepare = __exynos_dqe_prepare,
	.update = __exynos_dqe_update,
	.enable = __exynos_dqe_enable,
	.disable = __exynos_dqe_disable,
	.suspend = __exynos_dqe_suspend,
	.resume = __exynos_dqe_resume,
	.state = __exynos_dqe_state,
};

void exynos_dqe_dump(struct exynos_dqe *dqe)
{
	const struct exynos_dqe_funcs *funcs;

	if (!dqe || dqe->initialized == false)
		return;

	funcs = dqe->funcs;
	if (funcs)
		funcs->dump(dqe);
}

void exynos_dqe_prepare(struct exynos_dqe *dqe,
			struct drm_crtc_state *crtc_state)
{
	const struct exynos_dqe_funcs *funcs;

	if (!dqe || dqe->initialized == false || !crtc_state)
		return;

	funcs = dqe->funcs;
	if (funcs)
		funcs->prepare(dqe, crtc_state);
}

void exynos_dqe_update(struct exynos_dqe *dqe,
			struct drm_crtc_state *crtc_state)
{
	const struct exynos_dqe_funcs *funcs;

	if (!dqe || dqe->initialized == false || !crtc_state)
		return;

	funcs = dqe->funcs;
	if (funcs)
		funcs->update(dqe, crtc_state);
}

void exynos_dqe_enable(struct exynos_dqe *dqe)
{
	const struct exynos_dqe_funcs *funcs;

	if (!dqe || dqe->initialized == false)
		return;

	funcs = dqe->funcs;
	if (funcs)
		funcs->enable(dqe);
}

void exynos_dqe_disable(struct exynos_dqe *dqe)
{
	const struct exynos_dqe_funcs *funcs;

	if (!dqe || dqe->initialized == false)
		return;

	funcs = dqe->funcs;
	if (funcs)
		funcs->disable(dqe);
}

void exynos_dqe_suspend(struct exynos_dqe *dqe)
{
	const struct exynos_dqe_funcs *funcs;

	if (!dqe || dqe->initialized == false)
		return;

	funcs = dqe->funcs;
	if (funcs)
		funcs->suspend(dqe);
}

void exynos_dqe_resume(struct exynos_dqe *dqe)
{
	const struct exynos_dqe_funcs *funcs;

	if (!dqe || dqe->initialized == false)
		return;

	funcs = dqe->funcs;
	if (funcs)
		funcs->resume(dqe);
}

void exynos_dqe_state(struct exynos_dqe *dqe,
			enum dqe_reg_type type,	bool *enabled)
{
	const struct exynos_dqe_funcs *funcs;

	if (!enabled)
		return;

	*enabled = false;

	if (!dqe || dqe->initialized == false || type >= DQE_REG_MAX)
		return;

	funcs = dqe->funcs;
	if (funcs)
		funcs->state(dqe, type, enabled);
}

struct exynos_dqe *exynos_dqe_register(struct decon_device *decon)
{
	struct exynos_dqe *dqe;
	int ret;

	if (!decon) {
		pr_err("%s decon is NULL\n", __func__);
		return NULL;
	}

	if (decon->id > 0 || !(decon->config.out_type & DECON_OUT_DSI)) {
		pr_warn("%s decon%d no need dqe interface\n", __func__, decon->id);
		return NULL;
	}

	if (decon->emul_mode) {
		pr_info("%s emul-mode no need dqe interface\n", __func__);
		return NULL;
	}

	dqe = devm_kzalloc(decon->dev, sizeof(struct exynos_dqe), GFP_KERNEL);
	if (!dqe)
		return NULL;

	dqe->decon = decon;
	dqe->id = decon->id;

	ret = dqe_init_resources(dqe, decon->dev);
	if (ret) {
		dqe_err(dqe, "failed to init resources\n");
		return NULL;
	}

	spin_lock_init(&dqe->slock);
	mutex_init(&dqe->lock);
	atomic_set(&dqe->update_cnt, 0);
	atomic_set(&dqe->cgc_update_cnt, 0);
	dev_set_drvdata(dqe->dev, dqe);

	memset(&dqe->cfg, 0, sizeof(dqe->cfg));
	dqe->xml_suffix[0] = '\0';
	dqe_init_context(dqe, decon->dev);

	dqe->funcs = &dqe_funcs;
	dqe->state = DQE_STATE_DISABLE;
	dqe->initialized = true;

	dqe_info(dqe, "Display Quality Enhancer is supported\n");

	return dqe;
}
