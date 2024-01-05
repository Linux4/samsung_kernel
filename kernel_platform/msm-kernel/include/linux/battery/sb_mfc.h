/*
 * sb_mfc.h
 * Samsung Mobile SEC Battery MFC Header
 *
 * Copyright (C) 2021 Samsung Electronics, Inc.
 *
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#ifndef __SB_MFC_H
#define __SB_MFC_H __FILE__

#include <linux/slab.h>
#include <linux/i2c.h>

#define MFC_MODULE_NAME	"sb-mfc"

#define WRL_PARAM_CHIP_ID_SHIFT			24
#define WRL_PARAM_CHIP_ID_MASK			(0xFF << WRL_PARAM_CHIP_ID_SHIFT)
#define WRL_PARAM_FW_VER_SHIFT			8
#define WRL_PARAM_FW_VER_MASK			(0xFFFF << WRL_PARAM_FW_VER_SHIFT)
#define WRL_PARAM_FW_MODE_SHIFT			4
#define WRL_PARAM_FW_MODE_MASK			(0xF << WRL_PARAM_FW_MODE_SHIFT)

#define WRL_PARAM_GET_CHIP_ID(param)	((param & 0xFF000000) >> WRL_PARAM_CHIP_ID_SHIFT)
#define WRL_PARAM_GET_FW_VER(param)		((param & 0x00FFFF00) >> WRL_PARAM_FW_VER_SHIFT)
#define WRL_PARAM_GET_FW_MODE(param)	((param & 0x000000F0) >> WRL_PARAM_FW_MODE_SHIFT)

#define CHIP_ID_NOT_SET		0
#define CHIP_ID_MATCHED		1

#define MFC_CHIP_ID_P9320		0x20
#define MFC_CHIP_ID_S2MIW04		0x04

#define SB_MFC_DISABLE	(-3700)
#if IS_ENABLED(CONFIG_SB_MFC)
int sb_mfc_check_chip_id(int chip_id);
#else
static inline int sb_mfc_check_chip_id(int chip_id)
{ return SB_MFC_DISABLE; }
#endif

#endif /* __SB_MFC_H */

