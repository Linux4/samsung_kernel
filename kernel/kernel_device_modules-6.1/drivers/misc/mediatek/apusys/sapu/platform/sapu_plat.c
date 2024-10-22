// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2023 MediaTek Inc.
 */

#include "sapu_plat.h"

struct sapu_platdata sapu_platdata = {
	.ops = {
		/* only common callback, others will be runtime set */
		.detect = plat_detect,
	}
};

int set_sapu_platdata_by_dts(void)
{
	struct device_node *np;
	u32 pwr_ver;
	int ret;

	np = of_find_node_by_path("/trusty/trusty-sapu");
	ret = of_property_read_u32(np, "power-version", &pwr_ver);
	if (ret) {
		pr_info("%s read dts prop failed\n", __func__);
		return ret;
	}

	switch (pwr_ver) {
	case 1:
		sapu_platdata.ops.power_ctrl = power_ctrl_v1;
		break;
	case 2:
		sapu_platdata.ops.power_ctrl = power_ctrl_v2;
		break;
	default:
		pr_info("Unknown power version (%u)\n", pwr_ver);
		break;
	}

	return 0;
}
