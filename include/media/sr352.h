/* include/media/sr352.h
 *
 * Copyright (C) 2010, SAMSUNG ELECTRONICS
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */

#ifndef MEDIA_SR352_H
#define MEDIA_SR352_H

#define SR352_DRIVER_NAME	"SR352"

#define CONFIG_VIDEO_SR352_V_1_1 //many add

struct sr352_platform_data {
	unsigned int default_width;
	unsigned int default_height;
	unsigned int pixelformat;
	int freq;	/* MCLK in Hz */

#if 1//many add
	/* This SoC supports Parallel & CSI-2 */
	int is_mipi;
#endif

#if 1 //many del//zhuxuezhen 2012-12-27 recovery
	int (*flash_onoff)(int);
	int (*af_assist_onoff)(int);
	int (*torch_onoff)(int);
#endif
	/* ISP interrupt */
	int (*config_isp_irq)(void);
	int irq;

	int (*set_power)(struct device *dev, int on);
	int	gpio_rst;
	bool	enable_rst;

};

/**
* struct sr352_platform_data - platform data for SR352 driver
* @irq:   GPIO getting the irq pin of SR352
* @gpio_rst:  GPIO driving the reset pin of SR352
 * @enable_rst:	the pin state when reset pin is enabled
* @set_power:	an additional callback to a board setup code
 *		to be called after enabling and before disabling
*		the sensor device supply regulators
 */
#endif	/* MEDIA_S5K4EC_H */
