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
 * @bus_address: if @chunk_count is 1, this is the physical address the chunk.
 *               if @chunk_count > 1, this is the physical address of unsigned
 *               long array of @chunk_count elements that contains the physical
 *               address of each chunk.
 */
struct buffer_prot_info {
	unsigned int chunk_count;
	unsigned int dma_addr;
	unsigned int flags;
	unsigned int chunk_size;
	unsigned long bus_address;
};

void *samsung_dma_buffer_copy_offset_info(struct buffer_prot_info *org_info, size_t offset);
void samsung_dma_buffer_release_copied_info(struct buffer_prot_info *info);

#define SMC_DRM_PPMP_PROT		(0x82002110)
#define SMC_DRM_PPMP_UNPROT		(0x82002111)

#define HVC_DRM_TZMP2_PROT		(0x82002110)
#define HVC_DRM_TZMP2_UNPROT		(0x82002111)

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

	arm_smccc_hvc(cmd, arg0, arg1, arg2, arg3, 0, 0, 0, &res);
	return (unsigned long)res.a0;
}

/*
 * buffer id is consist of unique id and magic value.
 * buffer id [31:8] : unique id
 * buffer id [7:4]  : protection id
 * buffer id [3:0]  : non-zero value as 0xA
 */
#define SECURE_UID_WIDTH		24
#define SECURE_UID_SHIFT		8
#define SECURE_UID_MAX			((1 << SECURE_UID_WIDTH) - 1)
#define SECURE_UID_MASK(x)		(x & ((1 << SECURE_UID_WIDTH) - 1))
#define SECURE_PID_WIDTH		4
#define SECURE_PID_SHIFT		4
#define SECURE_PID_MASK(x)		(x & ((1 << SECURE_PID_WIDTH) - 1))
#define SECURE_BUFID_MAGIC		0xA
#define SECURE_BUFID_TO_UID(bufid)	SECURE_UID_MASK((bufid) >> SECURE_UID_SHIFT)

#define SECURE_MAKE_BUFID(flags, uid)	((SECURE_UID_MASK(uid)   << SECURE_UID_SHIFT) |	\
					 (SECURE_PID_MASK(flags) << SECURE_PID_SHIFT) | \
					  SECURE_BUFID_MAGIC)
#endif
