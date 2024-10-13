/*
 * Samsung Exynos SoC series Pablo driver
 *
 * Copyright (c) 2019 Samsung Electronics Co., Ltd
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef IS_HW_PDP_H
#define IS_HW_PDP_H

#include "is-hw-pdp.h"
#include "is-device-sensor.h"
#include "pablo-hw-api-common.h"

#define COREX_OFFSET 0x8000

#define COREX_IGNORE			(0)
#define COREX_COPY			(1)
#define COREX_SWAP			(2)

#define HW_TRIGGER			(0)
#define SW_TRIGGER			(1)

enum pdp_event_type {
	PE_START,
	PE_END,
	PE_PAF_STAT0,
	PE_PAF_STAT1,
	PE_PAF_STAT2,
	PE_ERR_INT1,
	PE_ERR_INT2,
	PE_PAF_OVERFLOW,
	PE_ERR_RDMA_IRQ,
	PE_SPECIAL,
};

enum pdp_input_path_type {
	OTF = 0,
	STRGEN = 1,
	DMA = 2,
};

enum pdp_ctx_ch {
	PDP_CTX_MAIN,
	PDP_CTX_SUB,
};

/* status */
u32 pdp_hw_g_ip_version(void __iomem *base);
unsigned int pdp_hw_g_idle_state(void __iomem *base);
void pdp_hw_get_line(void __iomem *base, u32 *total_line, u32 *curr_line);

/* config */
void pdp_hw_s_global_enable(void __iomem *base, bool enable);
void pdp_hw_s_global_enable_clear(void __iomem *base);
int pdp_hw_s_one_shot_enable(struct is_pdp *pdp);
void pdp_hw_s_corex_enable(void __iomem *base, bool enable);
void pdp_hw_s_corex_type(void __iomem *base, u32 type);
void pdp_hw_g_corex_state(void __iomem *base, u32 *corex_enable);
void pdp_hw_corex_resume(void __iomem *base);
void pdp_hw_s_core(struct is_pdp *pdp, bool pd_enable, struct is_sensor_cfg *sensor_cfg,
	u32 pd_width, u32 pd_height, u32 pd_hwformat, u32 sensor_type, u32 path, u32 en_votf,
	u32 num_buffers);
void pdp_hw_s_init(void __iomem *base);
void pdp_hw_s_reset(void __iomem *base);
void pdp_hw_s_path(void __iomem *base, u32 path);
void pdp_hw_s_wdma_init(void __iomem *base, u32 ch, u32 pd_dump_mode);
dma_addr_t pdp_hw_g_wdma_addr(void __iomem *base);
void pdp_hw_s_wdma_enable(void __iomem *base, dma_addr_t address);
void pdp_hw_s_wdma_disable(void __iomem *base);
void pdp_hw_s_af_rdma_addr(void __iomem *base, dma_addr_t *address, u32 num_buffers, u32 direct);
void pdp_hw_s_post_frame_gap(void __iomem *base, u32 interval);
void pdp_hw_s_fro(void __iomem *base, u32 num_buffers);
int pdp_hw_wait_idle(void __iomem *base, unsigned long state, u32 frame_time);
int pdp_hw_rdma_wait_idle(void __iomem *base);

/* stat */
int pdp_hw_g_stat0(void __iomem *base, void *buf, size_t len);
void pdp_hw_g_pdstat_size(u32 *width, u32 *height, u32 *bits_per_pixel);
void pdp_hw_s_pdstat_path(void __iomem *base, bool enable);

/* sensor mode */
bool pdp_hw_to_sensor_type(u32 pd_mode, u32 *sensor_type);
void pdp_hw_g_input_vc(u32 mux_val, u32 *img_vc, u32 *af_vc);

/* interrupt */
unsigned int pdp_hw_g_int1_state(void __iomem *base, bool clear, u32 *irq_state);
unsigned int pdp_hw_g_int1_mask(void __iomem *base);
unsigned int pdp_hw_g_int2_state(void __iomem *base, bool clear, u32 *irq_state);
unsigned int pdp_hw_g_int2_mask(void __iomem *base);
unsigned int pdp_hw_is_occurred(unsigned int state, enum pdp_event_type type);
void pdp_hw_g_int1_str(const char **int_str);
void pdp_hw_g_int2_str(const char **int_str);

void pdp_hw_s_input_mux(struct is_pdp *pdp, u32 lc[CAMIF_VC_ID_NUM]);
void pdp_hw_s_input_enable(struct is_pdp *pdp, bool on);

void pdp_hw_s_default_blk_cfg(void __iomem *base);

/* debug */
void pdp_hw_s_config_default(void __iomem *base);
int pdp_hw_dump(void __iomem *base);

#if IS_ENABLED(CONFIG_PDP_BPC_ROD_SRAM)
/* bpc/reorder sram size for MSPD mode */
int pdp_hw_get_bpc_reorder_sram_size(u32 width, int vc_sensor_mode);
#else
#define pdp_hw_get_bpc_reorder_sram_size(width, vc_sensor_mode) 0
#endif

void pdp_hw_strgen_enable(void __iomem *base);
void pdp_hw_strgen_disable(void __iomem *base);
#endif
