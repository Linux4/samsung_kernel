/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

#ifndef MTK_HCP_ISP7sp_H
#define MTK_HCP_ISP7sp_H

#include "mtk-hcp.h"

int isp7sp_release_working_buffer(struct mtk_hcp *hcp_dev);
int isp7sp_allocate_working_buffer(struct mtk_hcp *hcp_dev, unsigned int mode);
int isp7sp_get_init_info(struct img_init_info *info);
void *isp7sp_get_gce_virt(void);
void *isp7sp_get_hwid_virt(void);
int isp7sp_partial_flush(struct mtk_hcp *hcp_dev, struct flush_buf_info *b_info);

extern struct mtk_hcp_data isp7sp_hcp_data;

#endif /* _MTK_HCP_ISP7sp_H */
