/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) Samsung Electronics Co., Ltd.
 * Gwanghui Lee <gwanghui.lee@samsung.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef __OLED_PROPERTY_H__
#define __OLED_PROPERTY_H__

#include "../panel_property.h"

enum {
	OLED_BR_HBM_OFF,
	OLED_BR_HBM_ON,
	MAX_OLED_BR_HBM,
};

enum {
	OLED_PREV_BR_HBM_OFF,
	OLED_PREV_BR_HBM_ON,
	MAX_OLED_PREV_BR_HBM,
};

#define OLED_BR_HBM_PROPERTY ("oled_br_hbm")
#define OLED_PREV_BR_HBM_PROPERTY ("oled_prev_br_hbm")
#define OLED_TSET_PROPERTY ("oled_tset")


extern struct panel_prop_list oled_property_array[];
extern unsigned int oled_property_array_size;

#endif /* __OLED_PROPERTY_H__ */
