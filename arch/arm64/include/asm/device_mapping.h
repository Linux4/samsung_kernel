/*
 * linux/arch/arm64/include/device_mapping.h
 *
 * Author:	Neil Zhang <zhangwm@marvell.com>
 * Copyright:	(C) 2012 Marvell International Ltd.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */

#ifndef __DEVICE_MAPPING_H__
#define __DEVICE_MAPPING_H__

extern void __iomem *axi_virt_base;
extern void __iomem *apb_virt_base;

extern int __init device_mapping_init(void);

#endif
