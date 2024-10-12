/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Samsung Exynos SoC series Pablo driver
 *
 * Exynos pablo group manager configurations
 *
 * Copyright (c) 2020 Samsung Electronics Co., Ltd
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */
#ifndef IS_STRIPE_H
#define IS_STRIPE_H

#include "is-subdev-ctrl.h"

#define dbg_stripe(level, fmt, args...) \
	dbg_common((is_get_debug_param(IS_DEBUG_PARAM_STREAM)) >= (level), \
		"[stripe]", fmt, ##args)

#define pablo_is_first_shot(num, idx) \
	((!num) ? true : ((!idx) ? true : false))

#define pablo_is_last_shot(num, idx) \
	((!num) ? true : ((num - 1 == idx) ? true : false))

#define pablo_update_repeat_param(frame, input) \
{ \
	if (frame->repeat_info.num) { \
		input->repeat_idx = frame->repeat_info.idx; \
		input->repeat_num = frame->repeat_info.num; \
		input->repeat_scenario = frame->repeat_info.scenario; \
	} else { \
		input->repeat_idx = 0; \
		input->repeat_num = 0; \
		input->repeat_scenario = PABLO_REPEAT_NO; \
	} \
}

#define ALIGN_UPDOWN_STRIPE_WIDTH(w, align) \
	(w & (align) >> 1 ? ALIGN(w, (align)) : ALIGN_DOWN(w, (align)))

int is_calc_region_num(int input_width, struct is_subdev *subdev);

int is_ischain_g_stripe_cfg(struct is_frame *frame,
		struct camera2_node *node,
		const struct is_crop *in,
		const struct is_crop *incrop,
		const struct is_crop *otcrop);
#endif /* IS_STRIPE_H */
