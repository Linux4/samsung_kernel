/*
 * Copyright (C) 2015 Spreadtrum Communications Inc.
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#ifndef _SPRD_ADF_OVERLAY_ENGINE_H_
#define _SPRD_ADF_OVERLAY_ENGINE_H_

#include "sprd_adf_common.h"

#define SPRD_ADF_MAX_ENG 4
struct sprd_adf_overlay_engine *sprd_adf_create_overlay_engines(
				struct sprd_adf_device *dev,
				size_t n_engs);

int sprd_adf_destory_overlay_engines(
				struct sprd_adf_overlay_engine *engs,
				size_t n_engs);
#endif
