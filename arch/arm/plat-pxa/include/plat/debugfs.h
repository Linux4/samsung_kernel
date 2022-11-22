/*
 * arch/arm/plat-pxa/debugfs.h
 *
 * Author:	Neil Zhang <zhangwm@marvell.com>
 * Copyright:	(C) 2012 Marvell International Ltd.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */

#ifndef __PXA_DEBUGFS_H_
#define __PXA_DEBUGFS_H_

#ifdef CONFIG_CACHE_L2X0
extern void __iomem *l2x0_base;
#endif

extern struct dentry *pxa;

#endif  /* __PXA_DEBUGFS_H_ */
