/*
 * linux/drivers/video/fbdev/exynos/panel/tft_common/tft_common.c
 *
 * TFT_COMMON Dimming Driver
 *
 * Copyright (c) 2016 Samsung Electronics
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */
#include <linux/of_gpio.h>
#include <video/mipi_display.h>
#include "../panel.h"
#include "../panel_debug.h"
#include "ili7807_a14x_02_panel.h"

#if defined(CONFIG_USDM_PANEL_VCOM_TRIM_TEST)
int ili7807_a14x_02_vcom_trim_test(struct panel_device *panel, void *data, u32 len)
{
	int ret;
	char read_buf[ILI7807_A14X_02_VCOM_TRIM_LEN + ILI7807_A14X_02_VCOM_MARK1_LEN + ILI7807_A14X_02_VCOM_MARK2_LEN] = { 0xFF, 0xFF, 0xFF };

	if (!panel)
		return -EINVAL;

	ret = panel_do_seqtbl_by_name(panel, PANEL_VCOM_TRIM_TEST_SEQ);
	if (unlikely(ret < 0)) {
		panel_err("failed to write vcom-trim-test seq\n");
		return -EBUSY;
	}
	ret = panel_resource_copy(panel, read_buf, "ili7807_a14x_02_vcom_trim");
	if (unlikely(ret < 0)) {
		panel_err("ili7807_a14x_02_vcom_trim copy failed\n");
		return -ENODATA;
	}

	ret = panel_resource_copy(panel, read_buf + ILI7807_A14X_02_VCOM_MARK1_LEN, "ili7807_a14x_02_vcom_mark1");
	if (unlikely(ret < 0)) {
		panel_err("ili7807_a14x_02_vcom_mark1 copy failed\n");
		return -ENODATA;
	}

	ret = panel_resource_copy(panel,
		read_buf + ILI7807_A14X_02_VCOM_MARK1_LEN + ILI7807_A14X_02_VCOM_MARK2_LEN, "ili7807_a14x_02_vcom_mark2");
	if (unlikely(ret < 0)) {
		panel_err("ili7807_a14x_02_vcom_mark2 copy failed\n");
		return -ENODATA;
	}

	ret = PANEL_VCOM_TRIM_TEST_FAIL;
	if (read_buf[0] == 0x00 && read_buf[1] == 0x11 && read_buf[2] == 0x01)
		ret = PANEL_VCOM_TRIM_TEST_PASS;

	panel_info("ret %d, [0]: 0x%02x [1]: 0x%02x [2]: 0x%02x\n",
		ret, read_buf[0], read_buf[1], read_buf[2]);
	snprintf((char *)data, len, "%02x %02x %02x", read_buf[0], read_buf[1], read_buf[2]);

	return ret;
}
#endif

static int __init ili7807_a14x_02_panel_init(void)
{
	register_common_panel(&ili7807_a14x_02_panel_info);
	return 0;
}

static void __exit ili7807_a14x_02_panel_exit(void)
{
	deregister_common_panel(&ili7807_a14x_02_panel_info);
}

module_init(ili7807_a14x_02_panel_init)
module_exit(ili7807_a14x_02_panel_exit)

MODULE_DESCRIPTION("Samsung Mobile Panel Driver");
MODULE_LICENSE("GPL");
