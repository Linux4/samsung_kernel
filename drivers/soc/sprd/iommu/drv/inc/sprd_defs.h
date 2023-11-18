/*
 * Copyright (C) 2019 Spreadtrum Communications Inc.
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

#ifndef _SPRD_DEFS_H_
#define _SPRD_DEFS_H_

#include <linux/sprd_iommu.h>
#include <linux/slab.h>

#define SPRD_DECLARE_HANDLE(module_hdl) \
	typedef struct{int dummy; } module_hdl##__;\
	typedef module_hdl##__ * module_hdl
#define SHIFT_1M       (20)

/* General driver error definition */
enum {
	SPRD_NO_ERR = 0x100,
	SPRD_ERR_INVALID_PARAM,
	SPRD_ERR_INITIALIZED,
	SPRD_ERR_INVALID_HDL,
	SPRD_ERR_STATUS,
	SPRD_ERR_RESOURCE_BUSY,
	SPRD_ERR_ILLEGAL_PARAM,
	SPRD_ERR_MAX,
};

#endif  /*END OF : define  _SPRD_DEFS_H_ */
