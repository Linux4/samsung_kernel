/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2016 MediaTek Inc.
 * Author: Tiffany Lin <tiffany.lin@mediatek.com>
 */

#ifndef _MTK_VCODEC_ENC_PM_PLAT_H_
#define _MTK_VCODEC_ENC_PM_PLAT_H_

#include "mtk_vcodec_drv.h"

#define ENC_DVFS	1
#define ENC_EMI_BW	1

void mtk_prepare_venc_dvfs(struct mtk_vcodec_dev *dev);
void mtk_unprepare_venc_dvfs(struct mtk_vcodec_dev *dev);
void mtk_prepare_venc_emi_bw(struct mtk_vcodec_dev *dev);
void mtk_unprepare_venc_emi_bw(struct mtk_vcodec_dev *dev);
void mtk_venc_dvfs_reset_vsi_data(struct mtk_vcodec_dev *dev);
void mtk_venc_dvfs_sync_vsi_data(struct mtk_vcodec_ctx *ctx);
void mtk_venc_dvfs_begin_inst(struct mtk_vcodec_ctx *ctx);
void mtk_venc_dvfs_end_inst(struct mtk_vcodec_ctx *ctx);
void mtk_venc_pmqos_begin_inst(struct mtk_vcodec_ctx *ctx);
void mtk_venc_pmqos_end_inst(struct mtk_vcodec_ctx *ctx);
void mtk_venc_prepare_vcp_dvfs_data(struct mtk_vcodec_ctx *ctx, struct venc_enc_param *param);
void mtk_venc_unprepare_vcp_dvfs_data(struct mtk_vcodec_ctx *ctx, struct venc_enc_param *param);
void mtk_venc_pmqos_lock_unlock(struct mtk_vcodec_dev *dev, bool is_lock);

void mtk_venc_pmqos_monitor(struct mtk_vcodec_dev *dev, u32 state);
void mtk_venc_pmqos_monitor_init(struct mtk_vcodec_dev *dev);
void mtk_venc_pmqos_monitor_deinit(struct mtk_vcodec_dev *dev);
void mtk_venc_pmqos_monitor_reset(struct mtk_vcodec_dev *dev);
void mtk_venc_pmqos_frame_req(struct mtk_vcodec_ctx *ctx);
bool mtk_venc_dvfs_monitor_op_rate(struct mtk_vcodec_ctx *ctx, int buf_type);
void mtk_venc_dvfs_check_boost(struct mtk_vcodec_dev *dev);
void mtk_venc_init_boost(struct mtk_vcodec_ctx *ctx);
#endif /* _MTK_VCODEC_ENC_PM_H_ */
