/*
 * Samsung Exynos5 SoC series Flash driver
 *
 *
 * Copyright (c) 2014 Samsung Electronics Co., Ltd
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifdef FLASH_S2MPU06
#ifdef CAMERA_SYSFS_V2
enum torch_current {
	TORCH_OUT_I_20MA = 0,
	TORCH_OUT_I_40MA,
	TORCH_OUT_I_60MA,
	TORCH_OUT_I_80MA,
	TORCH_OUT_I_100MA,
	TORCH_OUT_I_120MA,
	TORCH_OUT_I_140MA,
	TORCH_OUT_I_160MA,
	TORCH_OUT_I_180MA,
	TORCH_OUT_I_200MA,
	TORCH_OUT_I_220MA,
	TORCH_OUT_I_240MA,
	TORCH_OUT_I_260MA,
	TORCH_OUT_I_280MA,
	TORCH_OUT_I_300MA,
	TORCH_OUT_I_MAX,
};
#endif

#define FLASH_STEP 100
#define TORCH_STEP 20
#define USE_FLASH_STEP(value)		((value)-1001)
#define S2MPU06_FLASH_CURRENT(mA)	((mA)?((((mA-FLASH_STEP)/FLASH_STEP)<<4) & 0xf0):(0xf0))


#define S2MPU06_TORCH_CURRENT(mA)	(((mA-TORCH_STEP)/TORCH_STEP) & 0x0f)
#endif
