/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (C) 2020 Unisoc Inc.
 */

#ifndef _GSP_INTERFACE_QOGIRL6_H
#define _GSP_INTERFACE_QOGIRL6_H

#include <linux/of.h>
#include <linux/regmap.h>
#include "../gsp_interface.h"


#define GSP_QOGIRL6 "qogirl6"

#define QOGIRL6_AP_AHB_DISP_EB_NAME	  ("clk_ap_ahb_disp_eb")

struct gsp_interface_qogirl6 {
	struct gsp_interface common;

	struct clk *clk_ap_ahb_disp_eb;
	struct regmap *module_en_regmap;
	struct regmap *reset_regmap;
};

int gsp_interface_qogirl6_parse_dt(struct gsp_interface *intf,
				  struct device_node *node);

int gsp_interface_qogirl6_init(struct gsp_interface *intf);
int gsp_interface_qogirl6_deinit(struct gsp_interface *intf);

int gsp_interface_qogirl6_prepare(struct gsp_interface *intf);
int gsp_interface_qogirl6_unprepare(struct gsp_interface *intf);

int gsp_interface_qogirl6_reset(struct gsp_interface *intf);

void gsp_interface_qogirl6_dump(struct gsp_interface *intf);

#endif
