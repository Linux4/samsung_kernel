// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2023 MediaTek Inc.
 */

#include <linux/notifier.h>
#include "swpm_module.h"
#include "sspm_reservedmem.h"

#include "mtk_swpm_common_sysfs.h"
#include "mtk_swpm_sysfs.h"
#include "swpm_dbg_common_v1.h"
#include "swpm_disp_v6989.h"

static struct disp_swpm_data *disp_swpm_data_ptr;

static void update_disp_info(void)
{
	if (!disp_swpm_data_ptr)
		return;
	disp_swpm_data_ptr->dsi_lane_num = 4;
	disp_swpm_data_ptr->dsi_data_rate = mtk_disp_get_dsi_data_rate(0);
	disp_swpm_data_ptr->dsi_phy_type = mtk_disp_get_dsi_data_rate(1);
	disp_swpm_data_ptr->disp_aal = mtk_disp_get_pq_data(DISP_PQ_AAL);
	disp_swpm_data_ptr->disp_ccorr = mtk_disp_get_pq_data(DISP_PQ_CCORR);
	disp_swpm_data_ptr->disp_c3d = mtk_disp_get_pq_data(DISP_PQ_C3D);
	disp_swpm_data_ptr->disp_gamma = mtk_disp_get_pq_data(DISP_PQ_GAMMA);
	disp_swpm_data_ptr->disp_color = mtk_disp_get_pq_data(DISP_PQ_COLOR);
	disp_swpm_data_ptr->disp_tdshp = mtk_disp_get_pq_data(DISP_PQ_TDSHP);
	disp_swpm_data_ptr->disp_dither = mtk_disp_get_pq_data(DISP_PQ_DITHER);
}

static int disp_swpm_event(struct notifier_block *nb,
			unsigned long event, void *v)
{
	switch (event) {
	case SWPM_LOG_DATA_NOTIFY:
		update_disp_info();
		break;
	default:
		break;
	}

	return NOTIFY_DONE;
}

static struct notifier_block disp_swpm_notifier = {
	.notifier_call = disp_swpm_event,
};

int swpm_disp_v6989_init(void)
{
	unsigned int offset;

	offset = swpm_set_and_get_cmd(0, 0, DISP_GET_SWPM_ADDR, DISP_CMD_TYPE);

	disp_swpm_data_ptr = (struct disp_swpm_data *)
		sspm_sbuf_get(offset);

	/* exception control for illegal sbuf request */
	if (!disp_swpm_data_ptr)
		return -1;

	swpm_register_event_notifier(&disp_swpm_notifier);

	return 0;
}

void swpm_disp_v6989_exit(void)
{
	swpm_unregister_event_notifier(&disp_swpm_notifier);
}
