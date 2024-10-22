// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2022 MediaTek Inc.
 */

#include "mtk_drm_drv.h"
#include "mtk_cust.h"

int set_lcm(struct cmdq_pkt *handle, struct mtk_ddic_dsi_msg *cmd_msg)
{
	return set_lcm_wrapper(handle, cmd_msg, 1);
}

int read_lcm(struct cmdq_pkt *handle, struct mtk_ddic_dsi_msg *cmd_msg)
{
	return read_lcm_wrapper(handle, cmd_msg);
}

int set_lcm_default_parameter(struct cmdq_pkt *handle, struct mtk_ddic_dsi_msg *cmd_msg, f_args in)
{
	int i = in.i ? in.i : 1;

	if (in.i == SET_LCM_NONBLOCKING)
		i = 0;

	pr_info("%s : %d %d\n", __func__, in.i, i);

	return set_lcm_wrapper(handle, cmd_msg, i);
}

int mtk_wait_frame_start(unsigned int timeout)
{
	return wait_frame_condition(DISP_FRAME_START, timeout);
}
EXPORT_SYMBOL(mtk_wait_frame_start);

int mtk_wait_frame_done(unsigned int timeout)
{
	return wait_frame_condition(DISP_FRAME_DONE, timeout);
}
EXPORT_SYMBOL(mtk_wait_frame_done);
