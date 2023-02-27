/*
 * Copyright (C) 2012 Spreadtrum Communications Inc.
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */
#ifndef _IMG_SCALE_H_
#define _IMG_SCALE_H_

#include "scale_drv.h"

#include <asm/ioctl.h>
#include <linux/types.h>

struct scale_k_private{
	struct semaphore start_sem;
	void *coeff_addr;
};

enum scale_k_status{
	SCALE_K_IDLE,
	SCALE_K_RUNNING,
	SCALE_K_DONE,
	SCALE_K_MAX
};

struct scale_k_file{
	struct scale_k_private *scale_private;

	struct semaphore scale_done_sem;

	struct scale_drv_private drv_private;

	struct device_node *dn;
};

#endif
