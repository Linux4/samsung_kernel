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



#ifndef _SEC_DP_API_H_
#define _SEC_DP_API_H_

//include

#include "sec_dp_mtk.h"

struct sec_dp_dev *dp_get_dev(void);
void dp_aux_sel(struct sec_dp_dev *dp, int dir);
void dp_aux_onoff(struct sec_dp_dev *dp, bool on, int dir);

#ifdef CONFIG_SEC_DISPLAYPORT_REDRIVER
u8 (*dp_get_redriver_tuning_table(void))[PHY_SWING_MAX][PHY_PREEMPHASIS_MAX];
#endif

void dp_hpd_changed(struct sec_dp_dev *dp, int hpd);
u32 dp_get_pixel_clock(u8 link_rate, u8 lane_count);
void dp_link_training_postprocess(u8 link_rate, u8 lane_count);
void dp_validate_modes(void);
u32 dp_reduce_audio_capa(u32 caps);

#endif //_SEC_DP_API_H_
