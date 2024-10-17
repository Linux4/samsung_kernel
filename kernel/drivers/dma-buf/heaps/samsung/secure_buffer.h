// SPDX-License-Identifier: GPL-2.0
/*
 * DMABUF Heap Secure Structure header to interface ldfw
 *
 * Copyright (C) 2021 Samsung Electronics Co., Ltd.
 * Author: <hyesoo.yu@samsung.com> for Samsung
 */

#ifndef _SECURE_BUFFER_H
#define _SECURE_BUFFER_H

#include <linux/arm-smccc.h>

/*
 * struct buffer_prot_info - buffer protection information
 * @chunk_count: number of physically contiguous memory chunks to protect
 *               each chunk should has the same size.
 * @dma_addr:    device virtual address for protected memory access
 * @flags:       protection flags but actually, protectid
 * @chunk_size:  length in bytes of each chunk.
 * @bus_address: This is physically linear address for device to access.
 * @chunk_array: This is the kmalloc array address of elements that contains
 *               the physical address of each chunk. It is used if chunk_size > 1.
 */
struct buffer_prot_info {
	unsigned int chunk_count;
	unsigned int dma_addr;
	unsigned int flags;
	unsigned int chunk_size;
	union {
		unsigned long bus_address;
		unsigned long *chunk_array;
	};
};

void *samsung_dma_buffer_copy_offset_info(struct buffer_prot_info *org_info, size_t offset);
void samsung_dma_buffer_release_copied_info(struct buffer_prot_info *info);

#define SMC_DRM_PPMP_PROT		(0x82002110)
#define SMC_DRM_PPMP_UNPROT		(0x82002111)

#define HVC_DRM_TZMP2_TRAP_MASK		(0x0800)

#if defined(CONFIG_EXYNOS_HYP)
#define HVC_DRM_TZMP2_PROT		(0x82002110 | HVC_DRM_TZMP2_TRAP_MASK)
#define HVC_DRM_TZMP2_UNPROT		(0x82002111 | HVC_DRM_TZMP2_TRAP_MASK)
#else
#define HVC_DRM_TZMP2_PROT		(0x82002110)
#define HVC_DRM_TZMP2_UNPROT		(0x82002111)
#endif

#define E_DRMPLUGIN_BUFFER_LIST_FULL 0x2002

static inline unsigned long ppmp_smc(unsigned long cmd, unsigned long arg0,
				     unsigned long arg1, unsigned long arg2)
{
	struct arm_smccc_res res;

	arm_smccc_smc(cmd, arg0, arg1, arg2, 0, 0, 0, 0, &res);
	return (unsigned long)res.a0;
}

static inline unsigned long ppmp_hvc(unsigned long cmd, unsigned long arg0,
				     unsigned long arg1, unsigned long arg2,
				     unsigned long arg3)
{
	struct arm_smccc_res res;

	if (cmd & HVC_DRM_TZMP2_TRAP_MASK)
		arm_smccc_smc(cmd, arg0, arg1, arg2, arg3, 0, 0, 0, &res);
	else
		arm_smccc_hvc(cmd, arg0, arg1, arg2, arg3, 0, 0, 0, &res);

	return (unsigned long)res.a0;
}
#endif
