
/*
 * Copyright (C) 2010 Innofidei Corporation
 * Author:      sean <zhaoguangyu@innofidei.com>
 * 
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 * 
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 * 
 */


#ifndef __INNO_PLATFORM_H_
#define __INNO_PLATFORM_H_


#include <linux/spi/spi.h>
#include <linux/interrupt.h>
#include <linux/spi/cmmb.h>

/**
 * inno_platform - innofidei demod driver platform implement layer struct 
 * @power:power on/off demod
 * @spi_transfer:do spi transfer due to msg
 * @irq_handler: platform layer should call this function if interrupt occur 
 */
struct inno_platform {
	struct spi_device *spi;
	void (*power)(unsigned char on);
	int (*spi_transfer)(struct spi_message *msg);
	irq_handler_t irq_handler;
	struct cmmb_platform_data *pdata;
};

/**
 * during inno_core.ko startup ,it will call this api to let platform initialize struct inno_platform 
 * platform can initialize gpio config for reset and interrupt pins
 */
extern int inno_platform_init(struct inno_platform *plat);
/**
 * inno_demod_irq_handler: platform layer should call this function in irq handler phase
 */
extern int inno_demod_irq_handler(void);

extern struct spi_device *g_spi;
#endif
