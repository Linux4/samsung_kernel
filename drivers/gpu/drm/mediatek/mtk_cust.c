// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2022 MediaTek Inc.
 * Author Harry.Lee <Harry.Lee@mediatek.com>
 */

#include "mtk_drm_drv.h"
#include "mtk_debug.h"

int set_lcm(struct mtk_ddic_dsi_msg *cmd_msg)
{
	return set_lcm_wrapper(cmd_msg, 1);
}
EXPORT_SYMBOL(set_lcm);

int read_lcm(struct mtk_ddic_dsi_msg *cmd_msg)
{
	return read_lcm_wrapper(cmd_msg);
}
EXPORT_SYMBOL(read_lcm);
