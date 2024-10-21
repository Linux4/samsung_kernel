// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2023 MediaTek Inc.
 */
#include "sapu_plat.h"

void plat_detect(void)
{
	struct device_node *np;
	const char *platname;

	np = of_find_node_by_path("/");
	platname = of_get_property(np, "model", NULL);

	pr_info("sapu detected platform - %s\n", platname);
}
