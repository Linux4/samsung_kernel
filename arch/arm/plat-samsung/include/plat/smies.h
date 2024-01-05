/*
 * arch/arm/plat-samsung/include/plat/smies.h
 *
 * Copyright 2014 Samsung Electronics Co., Ltd.
 *	Jiun Yu <jiun.yu@samsung.com>
 *
 * Samsung MIES driver functions
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef __SAMSUNG_PLAT_SMIES_H
#define __SAMSUNG_PLAT_SMIES_H __FILE__

struct s5p_smies_platdata {
	int sae_on;
	int scr_on;
	int gamma_on;
	int dither_on;
	int sae_skin_check;
	u32 sae_gain;
	u32 scr_r;
	u32 scr_g;
	u32 scr_b;
	u32 scr_c;
	u32 scr_m;
	u32 scr_y;
	u32 scr_white;
	u32 scr_black;
	u32 width;
	u32 height;
};

extern void s5p_smies_set_platdata(struct s5p_smies_platdata *pd);
extern int smies_enable_by_fimd(struct device *dev);
extern int smies_disable_by_fimd(struct device *dev);

#endif /* __SAMSUNG_PLAT_SMIES_H */
