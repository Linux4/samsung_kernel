/*
 * dbmd2-interface.h  --  DBMD2 interface definitions
 *
 * Copyright (C) 2014 DSP Group
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef _DBMD2_H
#define _DBMD2_H

#include <linux/device.h>

enum dbmd2_va_speeds {
	DBMD2_VA_SPEED_NORMAL = 0,
	DBMD2_VA_SPEED_BUFFERING,
	DBMD2_VA_SPEED_MAX,
	DBMD2_VA_NR_OF_SPEEDS,
};

struct va_speed {
	u32	cfg;
	u32	uart_baud;
	u32	i2c_rate;
	u32	spi_rate;
};

struct dbd2_uart_data {
	const char			*uart_dev;
	unsigned int		software_flow_control;
};

struct dbd2_platform_data {
	int				gpio_wakeup;
	int				gpio_reset;
	int				gpio_sv;
	int				gpio_d2strap1;
	int				gpio_d2strap1_rst_val;

	const char			*va_firmware_name;
	const char			*vqe_firmware_name;
	const char			*vqe_non_overlay_firmware_name;

	int				constant_clk_rate;
	int				master_clk_rate;

	int				feature_va;
	int				feature_vqe;
	int				feature_fw_overlay;
	unsigned int			va_cfg_values;
	u32				*va_cfg_value;
	unsigned int			vqe_cfg_values;
	u32				*vqe_cfg_value;
	unsigned int			vqe_modes_values;
	u32				*vqe_modes_value;

	int				auto_buffering;
	int				auto_detection;
	int				uart_low_speed_enabled;

	struct va_speed	va_speed_cfg[DBMD2_VA_NR_OF_SPEEDS];
	u32				va_mic_config[3];
	u32				va_initial_mic_config;
	int				va_audio_channels;
	u32				vqe_tdm_bypass_config;
};

#endif
