/*
 * Header file for TFT_COMMON Dimming Driver
 *
 * Copyright (c) 2016 Samsung Electronics
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef __ILI7807_A14X_02_H__
#define __ILI7807_A14X_02_H__

#if defined(CONFIG_USDM_PANEL_VCOM_TRIM_TEST)
#define ILI7807_A14X_02_VCOM_TRIM_REG			0x7F
#define ILI7807_A14X_02_VCOM_TRIM_OFS			0
#define ILI7807_A14X_02_VCOM_TRIM_LEN			1

#define ILI7807_A14X_02_VCOM_MARK1_REG			0xAB
#define ILI7807_A14X_02_VCOM_MARK1_OFS			0
#define ILI7807_A14X_02_VCOM_MARK1_LEN			1

#define ILI7807_A14X_02_VCOM_MARK2_REG			0xAC
#define ILI7807_A14X_02_VCOM_MARK2_OFS			0
#define ILI7807_A14X_02_VCOM_MARK2_LEN			1

int ili7807_a14x_02_vcom_trim_test(struct panel_device *panel, void *data, u32 len);
#endif
#endif
