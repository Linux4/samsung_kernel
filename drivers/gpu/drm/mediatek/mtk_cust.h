/*
 * Copyright (C) 2021 MediaTek Inc.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 */

#ifndef __MTK_CUST_H
#define __MTK_CUST_H

#include "mtk_debug.h"
#include "mtk_panel_ext.h"

int set_lcm(struct mtk_ddic_dsi_msg *cmd_msg);
int read_lcm(struct mtk_ddic_dsi_msg *cmd_msg);


#endif
