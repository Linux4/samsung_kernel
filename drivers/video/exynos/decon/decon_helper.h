/*
 * Copyright (c) 2014 Samsung Electronics Co., Ltd.
 *		http://www.samsung.com
 *
 * Header file for Exynos DECON driver
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
*/

#ifndef __SAMSUNG_DECON_HELPER_H__
#define __SAMSUNG_DECON_HELPER_H__

#include <linux/device.h>

#include "decon.h"

int decon_clk_set_parent(struct device *dev, const char *c, const char *p);
int decon_clk_set_rate(struct device *dev, const char *conid, unsigned int rate);
unsigned long decon_clk_get_rate(struct device *dev, const char *clkid);
void decon_to_psr_info(struct decon_device *decon, struct decon_psr_info *psr);
void decon_to_init_param(struct decon_device *decon, struct decon_init_param *p);

#ifdef CONFIG_FB_DSU
void decon_get_window_rect_log( char* buffer, struct decon_device *decon, struct decon_win_config_data *win_data );
void decon_store_window_rect_log( struct decon_device *decon, struct decon_win_config_data *win_data );
char* decon_last_window_rect_log( void );
void decon_print_bufered_window_rect_log( void );
#endif

#endif /* __SAMSUNG_DECON_HELPER_H__ */
