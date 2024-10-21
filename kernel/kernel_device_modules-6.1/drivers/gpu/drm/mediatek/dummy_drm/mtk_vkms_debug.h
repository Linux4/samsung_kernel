/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2021 MediaTek Inc.
 */

#ifndef __MTK_VKMS_DEBUG_H
#define __MTK_VKMS_DEBUG_H

#define DEBUG_BUFFER_SIZE 10240
extern int dummy_bitmap_enable;
void disp_dbg_probe(void);
void disp_dbg_init(struct drm_device *dev);
#endif/* __MTK_VKMS_DEBUG_H */
