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

int set_lcm(struct mtk_ddic_dsi_msg *cmd_msg,
			bool blocking, bool need_lock)
{
	return mtk_ddic_dsi_send_cmd(cmd_msg, blocking, need_lock);
}

int read_lcm(struct mtk_ddic_dsi_msg *cmd_msg, bool need_lock)
{
	return mtk_ddic_dsi_read_cmd(cmd_msg, need_lock);
}
