// SPDX-License-Identifier: GPL-2.0
/*
 * Samsung Exynos SoC series Pablo IS driver
 *
 * Copyright (c) 2022 Samsung Electronics Co., Ltd
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef IS_HW_API_CSI_H
#define IS_HW_API_CSI_H

#include "exynos-is-sensor.h"
#include "is-type.h"

enum csis_hw_type {
	CSIS_LINK			= 0,
	CSIS_WDMA			= 1,
};

/*
 * This enum will be used for current error status by reading interrupt source.
 */
enum csis_hw_err_id {
	CSIS_ERR_ID,
	CSIS_ERR_CRC,
	CSIS_ERR_ECC,
	CSIS_ERR_WRONG_CFG,
	CSIS_ERR_OVERFLOW_VC,
	CSIS_ERR_LOST_FE_VC,
	CSIS_ERR_LOST_FS_VC,
	CSIS_ERR_SOT_VC,
	CSIS_ERR_INVALID_CODE_HS,
	CSIS_ERR_SOT_SYNC_HS,
	CSIS_ERR_DESKEW_OVER,
	CSIS_ERR_SKEW,
	CSIS_ERR_MAL_CRC,
	CSIS_ERR_VRESOL_MISMATCH,
	CSIS_ERR_HRESOL_MISMATCH,
	CSIS_ERR_CRC_CPHY,
	CSIS_ERR_DMA_OTF_OVERLAP_VC,
	CSIS_ERR_DMA_DMAFIFO_FULL,
	CSIS_ERR_DMA_ABORT_DONE,
	CSIS_ERR_DMA_FRAME_DROP_VC,
	CSIS_ERR_DMA_LASTDATA_ERROR,
	CSIS_ERR_DMA_LASTADDR_ERROR,
	CSIS_ERR_DMA_FSTART_IN_FLUSH_VC,
	CSIS_ERR_DMA_C2COM_LOST_FLUSH_VC,
	CSIS_ERR_DMA_ADDR_VIOLATION_IRQ_VC,
	CSIS_ERR_END
};

/*
 * This enum will be used in csi_hw_s_control api to set specific functions.
 */
enum csis_hw_control_id {
	CSIS_CTRL_INTERLEAVE_MODE,
	CSIS_CTRL_LINE_RATIO,
	CSIS_CTRL_BUS_WIDTH,
	CSIS_CTRL_DMA_ABORT_REQ,
	CSIS_CTRL_ENABLE_LINE_IRQ,
	CSIS_CTRL_PIXEL_ALIGN_MODE,
	CSIS_CTRL_LRTE,
	CSIS_CTRL_DESCRAMBLE,
};

/*
 * This struct will be used in csi_hw_g_irq_src api.
 * In csi_hw_g_irq_src, all interrupt source status should be
 * saved to this structure. otf_start, otf_end, dma_start, dma_end,
 * line_end fields has virtual channel bit set info.
 * Ex. If otf end interrupt occurred in virtual channel 0 and 2 and
 *        dma end interrupt occurred in virtual channel 0 only,
 *   .otf_end = 0b0101
 *   .dma_end = 0b0001
 */
struct csis_irq_src {
	u32 otf_start;
	u32 otf_end;
	u32 otf_dbg;
	u32 dma_start;
	u32 dma_end;
	u32 line_end;
	u32 dma_abort;
	bool err_flag;
	ulong err_id[CSI_VIRTUAL_CH_MAX];
	u32 ebuf_status;
};

struct is_wdma_info;

/*
 * ******************
 * MIPI-CSIS H/W APIS
 * ******************
 */
u32 csi_hw_get_version(u32 __iomem *base_reg);
int csi_hw_get_ppc_mode(u32 __iomem *base_reg);
u32 csi_hw_s_fcount(u32 __iomem *base_reg, u32 vc, u32 count);
u32 csi_hw_g_fcount(u32 __iomem *base_reg, u32 vc);
int csi_hw_reset(u32 __iomem *base_reg);
int csi_hw_fifo_reset(u32 __iomem *base_reg, u32 ch);
int csi_hw_s_phy_sctrl_n(u32 __iomem *base_reg, u32 ctrl, u32 n);
int csi_hw_s_phy_bctrl_n(u32 __iomem *base_reg, u32 ctrl, u32 n);
int csi_hw_s_lane(u32 __iomem *base_reg, u32 lanes, u32 use_cphy);
int csi_hw_s_control(u32 __iomem *base_reg, u32 id, u32 value);
int csi_hw_s_config(u32 __iomem *base_reg, u32 channel, struct is_vci_config *config,
	u32 width, u32 height, bool potf);
int csi_hw_s_irq_msk(u32 __iomem *base_reg, bool on, bool f_id_dec);
int csi_hw_g_irq_src(u32 __iomem *base_reg, struct csis_irq_src *src, bool clear);
int csi_hw_enable(u32 __iomem *base_reg, u32 use_cphy);
int csi_hw_disable(u32 __iomem *base_reg);
int csi_hw_dump(u32 __iomem *base_reg);
int csi_hw_vcdma_dump(u32 __iomem *base_reg);
int csi_hw_vcdma_cmn_dump(u32 __iomem *base_reg);
int csi_hw_phy_dump(u32 __iomem *base_reg, u32 instance);
int csi_hw_common_dma_dump(u32 __iomem *base_reg);
int csi_hw_mcb_dump(u32 __iomem *base_reg);
int csi_hw_ebuf_dump(u32 __iomem *base_reg);
int csi_hw_cdump(u32 __iomem *base_reg);
int csi_hw_vcdma_cdump(u32 __iomem *base_reg);
int csi_hw_vcdma_cmn_cdump(u32 __iomem *base_reg);
int csi_hw_phy_cdump(u32 __iomem *base_reg, u32 instance);
int csi_hw_common_dma_cdump(u32 __iomem *base_reg);
int csi_hw_mcb_cdump(u32 __iomem *base_reg);
int csi_hw_ebuf_cdump(u32 __iomem *base_reg);
void csi_hw_dma_reset(u32 __iomem *base_reg);
void csi_hw_s_frameptr(u32 __iomem *base_reg, u32 vc, u32 number, bool clear);
u32 csi_hw_g_frameptr(u32 __iomem *base_reg, u32 vc);
u32 csi_hw_g_dma_sbwc_type(u32 __iomem *ctl_reg);
const char *csi_hw_g_dma_sbwc_name(u32 __iomem *ctl_reg);
void csi_hw_s_dma_addr(u32 __iomem *ctl_reg, u32 __iomem *vc_reg,
		u32 vc, u32 number, dma_addr_t addr);
u32 csi_hw_g_img_stride(u32 width, u32 bpp, u32 bitwidth, u32 sbwc_type);
void csi_hw_s_dma_header_addr(u32 __iomem *ctl_reg, u32 __iomem *vc_reg,
		u32 buf_i, dma_addr_t dva);
void csi_hw_s_multibuf_dma_addr(u32 __iomem *ctl_reg, u32 __iomem *vc_reg,
		u32 vc, u32 number, dma_addr_t addr);
void csi_hw_s_multibuf_dma_fcntseq(u32 __iomem *vc_reg, u32 buffer_num, dma_addr_t addr,
				bool clear);
void csi_hw_s_dma_dbg_cnt(u32 __iomem *vc_reg, struct csis_irq_src *src, u32 vc);
void csi_hw_s_output_dma(u32 __iomem *base_reg, u32 vc, bool enable);
bool csi_hw_g_output_dma_enable(u32 __iomem *base_reg, u32 vc);
bool csi_hw_g_output_cur_dma_enable(u32 __iomem *base_reg, u32 vc);
int csi_hw_s_config_dma(u32 __iomem *ctl_reg, u32 __iomem *vc_reg,
		u32 vc, struct is_frame_cfg *cfg, u32 hwformat, u32 dummy_pixel);
void csi_hw_dma_common_reset(u32 __iomem *base_reg, bool on);
int csi_hw_s_dma_common_dynamic(u32 __iomem *base_reg, size_t size, u32 dma_ch);
int csi_hw_s_dma_common_pattern_enable(u32 __iomem *base_reg, u32 width,
		u32 height, u32 fps, u32 clk);
void csi_hw_s_dma_common_pattern_disable(u32 __iomem *base_reg);
int csi_hw_s_dma_common_votf_cfg(u32 __iomem *base_reg, u32 width, u32 dma_ch, u32 vc, bool on);
int csi_hw_s_dma_common_frame_id_decoder(u32 __iomem *base_reg, u32 dma_ch,
		u32 enable);
int csi_hw_g_dma_common_frame_id(u32 __iomem *base_reg, u32 batch_num, u32 *frame_id);
void csi_hw_s_dma_common_sbwc_ch(u32 __iomem *top_reg, u32 dma_ch);
int csi_hw_clear_fro_count(u32 __iomem *dma_top_reg, u32 __iomem *vc_reg);
int csi_hw_s_fro_count(u32 __iomem *vc_cmn_reg, u32 batch_num, u32 vc);

int csi_hw_s_dma_irq_msk(u32 __iomem *base_reg, bool on);
int csi_hw_g_dma_irq_src(u32 __iomem *base_reg, struct csis_irq_src *src, bool clear);
int csi_hw_g_dma_irq_src_vc(u32 __iomem *base_reg, struct csis_irq_src *src,
		u32 vc_abs, bool clear);
int csi_hw_s_config_dma_cmn(u32 __iomem *base_reg, u32 vc, u32 extformat,
		u32 hwformat, bool potf);

void csi_hw_s_mcb_qch(u32 __iomem *base_reg, bool on);
void csi_hw_s_ebuf_enable(u32 __iomem *base_reg, bool on, u32 ebuf_ch, int mode,
			u32 num_of_ebuf, u32 offset_fake_frame_done);
int csi_hw_s_cfg_ebuf(u32 __iomem *base_reg, u32 ebuf_ch, u32 vc, u32 width,
		u32 height);
void csi_hw_g_ebuf_irq_src(u32 __iomem *base_reg, struct csis_irq_src *src, int ebuf_ch,
			unsigned int offset_fake_frame_done);
void csi_hw_s_ebuf_fake_sign(u32 __iomem *base_reg, u32 ebuf_ch);

bool csi_hw_s_bns_cfg(u32 __iomem *reg, struct is_sensor_cfg *sensor_cfg,
		u32 *width, u32 *height);
void csi_hw_s_bns_ch(u32 __iomem *reg, u32 ch);
void csi_hw_reset_bns(u32 __iomem *reg);
int csi_hw_bns_dump(u32 __iomem *reg);

void csi_hw_s_otf_preview_only(u32 __iomem *reg, u32 otf_ch, u32 en);
int csi_hw_fro_dump(u32 __iomem *reg);

void csi_hw_s_dma_input_mux(void __iomem *mux_regs,
		u32 wdma_index, bool bns_en, u32 bns_dma_mux_val,
		u32 csi_ch, int otf_ch, int otf_out_id,
		u32* link_vc_list, struct is_wdma_info* wdma_info);
void csi_hw_s_init_input_mux(void __iomem *mux_regs, u32 index);
void csi_hw_s_potf_ctrl(u32 __iomem *base_reg);

#endif
