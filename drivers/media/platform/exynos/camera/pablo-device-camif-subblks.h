// SPDX-License-Identifier: GPL-2.0
/*
 * Samsung Exynos SoC series Pablo driver
 *
 * Copyright (c) 2021 Samsung Electronics Co., Ltd
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef PABLO_DEVICE_CAMIF_SUBBLKS_H
#define PABLO_DEVICE_CAMIF_SUBBLKS_H

#include "is-hw.h"

struct pablo_camif_subblks_data {
	spinlock_t slock;
	int (*probe)(struct platform_device *pdev,
			struct pablo_camif_subblks_data *data);
	int (*remove)(struct platform_device *pdev,
			struct pablo_camif_subblks_data *data);
};

enum pablo_camif_otf_out_id {
	CAMIF_OTF_OUT_SINGLE,
	CAMIF_OTF_OUT_LONG = CAMIF_OTF_OUT_SINGLE,
	CAMIF_OTF_OUT_SHORT,
	CAMIF_OTF_OUT_MID,
	CAMIF_OTF_OUT_ID_NUM,
};

enum pablo_camif_vc_id {
	CAMIF_VC_IMG,
	CAMIF_VC_HPD,
	CAMIF_VC_VPD,
	CAMIF_VC_ID_NUM,
};

/**
  * struct pablo_camif_otf_info - Overall information about CSIS link & CSIS_PDP_TOP OTF out configuration.
  *
  *	[Sensor] -> Sensor VC -> [CSI_Link] -> CSI Link VC -> [IBUF_MUX] -> CSI OTF output LC --> PDP_IN_MUX
  *											      |-> CSIS_DMA_MUX
  *
  * @csi_ch:		CSIS link HW ID.
  * @otf_out_ch:	CSIS OTF output channel after IBUF
  * @otf_out_num:	The number of available OTF output channel.
  * @req_otf_out_num:	The requested number of OTF output channel.
  * @act_otf_out_num:	Currently working OTF output channel.
  * @link_vc:		CSI link VC information for each OTF output ID.
  * 			It's valid from CSIS link to IBUF input.
  * @otf_lc:		CSI OTF output LC(Logical Channel) information.
  *			It's valid after IBUF outout.
  * @otf_lc_num:	The valid number of CSI OTF output LC for each OTF output channel.
  * @link_vc_list:	Link vc list for each OTF output channel.
  */
struct pablo_camif_otf_info {
	u32 csi_ch;
	u32 otf_out_ch[CAMIF_OTF_OUT_ID_NUM];
	u32 otf_out_num;
	u32 req_otf_out_num;
	u32 act_otf_out_num;
	u32 link_vc[CAMIF_OTF_OUT_ID_NUM][CAMIF_VC_ID_NUM];
	u32 otf_lc[CAMIF_OTF_OUT_ID_NUM][CAMIF_VC_ID_NUM];
	u32 otf_lc_num[CAMIF_OTF_OUT_ID_NUM];
	u32 link_vc_list[CAMIF_OTF_OUT_ID_NUM][CSIS_OTF_CH_LC_NUM];
};

#define CALL_CAMIF_TOP_OPS(top, op, args...) \
	((((top)->ops && (top)->ops->op)) ? ((top)->ops->op(top, args)) : 0)

struct pablo_camif_csis_pdp_top;
struct pablo_camif_csis_pdp_top_ops {
	void (*s_otf_out_mux)(struct pablo_camif_csis_pdp_top *top,
			struct pablo_camif_otf_info *otf_info,
			u32 otf_out_id, bool en);
	void (*set_ibuf)(struct pablo_camif_csis_pdp_top *top,
			struct pablo_camif_otf_info *otf_info, u32 otf_out_id,
			struct is_sensor_cfg *sensor_cfg);
	void (*enable_ibuf_ptrn_gen)(struct pablo_camif_csis_pdp_top *top,
			u32 otf_ch, bool sensor_sync);
};

struct pablo_camif_csis_pdp_top {
	void __iomem	*regs;
	const struct pablo_camif_csis_pdp_top_ops *ops;

	u8		csi_ch[MAX_NUM_CSIS_OTF_CH];
	spinlock_t	csi_ch_slock;
};
struct pablo_camif_csis_pdp_top *pablo_camif_csis_pdp_top_get(void);

void pablo_camif_csis_pdp_top_dump(u32 __iomem *base_reg);

struct pablo_camif_bns {
	struct device *dev;
	struct pablo_camif_subblks_data *data;

	void __iomem 	*regs;
	void __iomem 	*mux_regs;
	u32 		*mux_val;
	u32 		mux_elems;
	u32		dma_mux_val;

	bool		en;
	u32		width;
	u32		height;
};

#if IS_ENABLED(CONFIG_PABLO_CAMIF_BNS)
struct pablo_camif_bns *pablo_camif_bns_get(void);
void pablo_camif_bns_cfg(struct pablo_camif_bns *bns,
		struct is_sensor_cfg *sensor_cfg,
		u32 ch);
void pablo_camif_bns_reset(struct pablo_camif_bns *bns);
#else
#define pablo_camif_bns_get() 	({ NULL; })
#define pablo_camif_bns_cfg(bns, cfg, ch) do {} while(0)
#define pablo_camif_bns_reset(bns) {} do {} while(0)
#endif

struct pablo_camif_mcb {
	void __iomem		*regs;
	struct mutex		lock;
	unsigned long		active_ch;
};

struct pablo_camif_mcb *pablo_camif_mcb_get(void);

struct pablo_camif_ebuf {
	void __iomem		*regs;
	int			irq;
	struct mutex		lock;
	unsigned int		num_of_ebuf;
	unsigned int		fake_done_offset;
};

struct pablo_camif_ebuf *pablo_camif_ebuf_get(void);
struct platform_driver *pablo_camif_subblks_get_platform_driver(void);

#endif /* PABLO_DEVICE_CAMIF_SUBBLKS_H */
