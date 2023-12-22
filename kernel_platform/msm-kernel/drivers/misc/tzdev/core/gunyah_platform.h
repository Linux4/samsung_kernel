/*
 * Copyright (C) 2022, Samsung Electronics Co., Ltd.
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

#ifndef __TZ_GUNYAH_H__
#define __TZ_GUNYAH_H__

#include <linux/gunyah/gh_dbl.h>
#include <linux/gunyah/gh_mem_notifier.h>
#include <linux/types.h>

int gunyah_get_pvm_vmid(void);
int gunyah_get_tvm_vmid(void);

int gunyah_mem_notify(uint32_t mem_handle);
int gunyah_share_memory(struct sg_table *sgt, uint32_t *mem_handle);

#endif /* __TZ_GUNYAH_H__ */