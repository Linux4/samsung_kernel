/*
 * Copyright 2012 Marvell International Ltd.
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#ifndef _PXA_ION_H
#define _PXA_ION_H

#include "../ion.h"

struct ion_pxa_region {
	struct ion_handle *handle;
	unsigned long addr;
	size_t len;
};


enum {
	PXA_DMA_BIDIRECTIONAL = 0,
	PXA_DMA_TO_DEVICE = 1,
	PXA_DMA_FROM_DEVICE = 2,
	PXA_DMA_NONE = 3,
};

struct ion_pxa_cache_region {
	int fd;				/* buf id used in 3.4 */
	struct ion_handle *handle;	/* handle used in 3.0, will remove */
	unsigned long offset;
	size_t len;
	unsigned int dir;
};

extern int arm_iommu_power_switch(struct device *dev, bool on);

#endif /* _PXA_ION_H */
