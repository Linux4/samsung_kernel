/*
 * linux/drivers/video/fbdev/exynos/panel/s6e3fa9/s6e3fa9_aod.h
 *
 * Header file for AOD Driver
 *
 * Copyright (c) 2016 Samsung Electronics
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef __S6E3FA9_AOD_H__
#define __S6E3FA9_AOD_H__

#include "../panel.h"
#include "../aod/aod_drv.h"

#define SELFMASK_CHECKSUM_VALID1	0x0F
#define SELFMASK_CHECKSUM_VALID2	0x09

#define SELFMASK_CHECKSUM_LEN		2

void s6e3fa9_copy_self_mask_ctrl(struct maptbl *tbl, u8 *dst);
int s6e3fa9_init_self_mask_ctrl(struct maptbl *tbl);

#endif
