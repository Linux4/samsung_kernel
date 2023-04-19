/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) Samsung Electronics Co., Ltd.
 * Gwanghui Lee <gwanghui.lee@samsung.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef __OLED_COMMON_MDNIE_H__
#define __OLED_COMMON_MDNIE_H__

#include "../maptbl.h"

int getidx_mdnie_scenario_mode_maptbl(struct maptbl *tbl);
#ifdef CONFIG_SUPPORT_HMD
int getidx_mdnie_hmd_maptbl(struct maptbl *tbl);
#endif
int getidx_mdnie_hdr_maptbl(struct maptbl *tbl);
int getidx_mdnie_trans_mode_maptbl(struct maptbl *tbl);
int getidx_mdnie_night_mode_maptbl(struct maptbl *tbl);
int getidx_color_lens_maptbl(struct maptbl *tbl);
void copy_scr_white_maptbl(struct maptbl *tbl, u8 *dst);
void copy_scr_cr_maptbl(struct maptbl *tbl, u8 *dst);
#ifdef CONFIG_SUPPORT_AFC
void copy_afc_maptbl(struct maptbl *tbl, u8 *dst);
#endif
#endif /* __OLED_COMMON_MDNIE_H__ */
