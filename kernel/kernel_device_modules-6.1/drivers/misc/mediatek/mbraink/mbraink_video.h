/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2022 MediaTek Inc.
 */
#ifndef MBRAINK_VIDEO_H
#define MBRAINK_VIDEO_H

#if IS_ENABLED(CONFIG_MTK_LPM_MT6985) && \
	IS_ENABLED(CONFIG_VIDEO_MEDIATEK_VCODEC) && \
	IS_ENABLED(CONFIG_VIDEO_MEDIATEK_VCODEC_V2) && \
	IS_ENABLED(CONFIG_MTK_TINYSYS_VCP_SUPPORT)

#define MTK_VDEC_HW_NUM 5

extern int mbraink_dec_is_power_on[MTK_VDEC_HW_NUM];
extern int mbraink_vcp_is_power_on;
#endif

int mbraink_get_video_info(char *buffer);

#endif /*end of MBRAINK_VIDEO_H*/
