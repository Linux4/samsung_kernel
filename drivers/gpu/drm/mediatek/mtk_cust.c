/*
 * Copyright (c) 2021 MediaTek Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */
#include "mtk_drm_drv.h"

int set_lcm(struct mtk_ddic_dsi_msg *cmd_msg)
{
	return set_lcm_wrapper(cmd_msg, 1);
}

int read_lcm(struct mtk_ddic_dsi_msg *cmd_msg)
{
	return read_lcm_wrapper(cmd_msg);
}
