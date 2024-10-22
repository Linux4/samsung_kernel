/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2021 MediaTek Inc.
 */

#ifndef __MTK_DMDP_AAL_H__
#define __MTK_DMDP_AAL_H__

#include <linux/uaccess.h>
#include <uapi/drm/mediatek_drm.h>

void mtk_dmdp_aal_regdump(struct mtk_ddp_comp *comp);
void mtk_dmdp_aal_bypass(struct mtk_ddp_comp *comp, int bypass, struct cmdq_pkt *handle);
void mtk_dmdp_aal_bypass_flag(struct mtk_ddp_comp *comp, int bypass);
void mtk_dmdp_aal_primary_data_update(struct mtk_ddp_comp *comp, const struct DISP_AAL_INITREG *init_regs);
#endif
