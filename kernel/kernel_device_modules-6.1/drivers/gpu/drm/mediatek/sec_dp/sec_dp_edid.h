// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2024 Samsung Electronics Co., Ltd.
 *		http://www.samsung.com
 *
 * SEC Displayport
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */



#ifndef _SEC_DP_EDID_H_
#define _SEC_DP_EDID_H_

//include
#include <linux/version.h>
#include <linux/device.h>
#include <linux/kernel.h>
#include <linux/delay.h>
#include <linux/mutex.h>
#include <linux/list.h>
#include <linux/wait.h>
#include <linux/ktime.h>

#include <drm/drm_modes.h>
#include <drm/drm_edid.h>
#include <drm/drm_device.h>
#if LINUX_VERSION_CODE < KERNEL_VERSION(6, 1, 0)
#include <drm/drm_dp_helper.h>
#else
#include <drm/display/drm_dp_helper.h>
#endif
#include <drm/drm_atomic_helper.h>
#include <drm/drm_crtc_helper.h>

#include "sec_dp_mtk.h"

#define MAX_EDID_BLOCK 4

#define MODE_FILTER_TEST_SINK_SUPPORT	(1 << 0)
#define MODE_FILTER_MAX_RESOLUTION	(1 << 1)
#define MODE_FILTER_PIXEL_CLOCK		(1 << 2)
#define MODE_FILTER_DEX_MODE		(1 << 3)
#define MODE_FILTER_DEX_ADAPTER		(1 << 4)
#define MODE_FILTER_MIN_FPS		(1 << 5)
#define MODE_FILTER_MAX_FPS		(1 << 6)

#define MODE_MIN_FPS	50
#define MODE_MAX_FPS	60

#define MODE_FILTER_MIRROR	(MODE_FILTER_MAX_RESOLUTION |\
				MODE_FILTER_PIXEL_CLOCK |\
				MODE_FILTER_TEST_SINK_SUPPORT)

#define MODE_FILTER_DEX		(MODE_FILTER_MAX_RESOLUTION |\
				MODE_FILTER_PIXEL_CLOCK |\
				MODE_FILTER_DEX_MODE |\
				MODE_FILTER_MIN_FPS |\
				MODE_FILTER_MAX_FPS)

#define MODE_MAX_RESOLUTION	(8192 * 4320 * 30)

struct drm_display_mode *dp_get_best_mode(void);
void dp_parse_edid(struct edid *edid);
void dp_print_all_modes(struct list_head *modes);

#endif //_SEC_DP_EDID_H_
