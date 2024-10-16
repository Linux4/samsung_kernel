// SPDX-License-Identifier: GPL-2.0
/*
 * Samsung Exynos SoC series Pablo driver
 *
 * Exynos Pablo image subsystem functions
 *
 * Copyright (c) 2022 Samsung Electronics Co., Ltd
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef PABLO_HW_HELPER_H
#define PABLO_HW_HELPER_H

#include "is-hw.h"
#include "pablo-mmio.h"

int pablo_hw_helper_set_iq_set(void __iomem *base, u32 set_id, u32 hw_id,
				struct is_hw_iq *iq_set,
				struct is_hw_iq *cur_iq_set);
int pablo_hw_helper_set_cr_set(void __iomem *base, unsigned int offset,
				u32 regs_size, struct cr_set *regs);
int pablo_hw_helper_ext_fsync(struct pablo_mmio *pmio, unsigned int offset,
			      void *buf, const void *items, size_t block_count);
int pablo_hw_helper_probe(struct is_hw_ip *hw_ip);

#endif
