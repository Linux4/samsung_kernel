
/* linux/drivers/video/fbdev/exynos/decon/panels/dsim_panel.c
 *
 * Copyright (c) 2015 Samsung Electronics
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
*/

#include <linux/lcd.h>
#include "../dsim.h"

#include "dsim_panel.h"

#if defined(CONFIG_PANEL_S6E3FA3_A7Y17)
struct mipi_dsim_lcd_driver *mipi_lcd_driver = &s6e3fa3_mipi_lcd_driver;
#elif defined(CONFIG_PANEL_S6E3FA3_A5Y17)
struct mipi_dsim_lcd_driver *mipi_lcd_driver = &s6e3fa3_mipi_lcd_driver;
#endif

int dsim_panel_ops_init(struct dsim_device *dsim)
{
	int ret = 0;

	if (dsim)
		dsim->panel_ops = mipi_lcd_driver;

	return ret;
}

int register_lcd_driver(struct mipi_dsim_lcd_driver *drv)
{
	struct device_node *node;
	int count = 0;
	char *dts_name = "lcd_info";

	node = of_find_node_with_property(NULL, dts_name);
	if (!node) {
		pr_info("%s: of_find_node_with_property\n", __func__);
		goto exit;
	}

	count = of_count_phandle_with_args(node, dts_name, NULL);
	if (!count) {
		pr_info("%s: of_count_phandle_with_args\n", __func__);
		goto exit;
	}

	node = of_parse_phandle(node, dts_name, 0);
	if (!node) {
		pr_info("%s: of_parse_phandle\n", __func__);
		goto exit;
	}

	if (count != 1) {
		pr_info("%s: we need only one phandle in lcd_info\n", __func__);
		goto exit;
	}

	if (IS_ERR_OR_NULL(drv) || IS_ERR_OR_NULL(drv->name)) {
		pr_info("%s: we need lcd_drv name to compare with device tree name(%s)\n", __func__, node->name);
		goto exit;
	}

	if (strstarts(node->name, drv->name)) {
		mipi_lcd_driver = drv;
		pr_info("%s: %s is registered\n", __func__, mipi_lcd_driver->name);
	}

exit:
	return 0;
}


unsigned int lcdtype;
EXPORT_SYMBOL(lcdtype);

static int __init get_lcd_type(char *arg)
{
	get_option(&arg, &lcdtype);

	dsim_info("--- Parse LCD TYPE ---\n");
	dsim_info("LCDTYPE : %08x\n", lcdtype);

	return 0;
}
early_param("lcdtype", get_lcd_type);

