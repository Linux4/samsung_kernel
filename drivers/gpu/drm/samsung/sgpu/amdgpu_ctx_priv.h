/*
 * Copyright 2022 Advanced Micro Devices, Inc.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
 * THE COPYRIGHT HOLDER(S) OR AUTHOR(S) BE LIABLE FOR ANY CLAIM, DAMAGES OR
 * OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
 * ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
 * OTHER DEALINGS IN THE SOFTWARE.
 *
 */

#ifndef __AMDGPU_CTX_PRIV_H__
#define __AMDGPU_CTX_PRIV_H__
/* KMD VM map for TMZ and CWSR */

//max priviate ring within one drm handle
#define AMDGPU_PRIV_MAX_RING (8)

//support up to 128 WB
#define AMDGPU_PRIV_WB_SIZE (2 * PAGE_SIZE)

//each EOP needs 2K bytes, add 2K byte for page alignment.
//support up to 8 private ring, 8 Pages are reserved.
#define AMDGPU_PRIV_MEC_HQD_EOP_SIZE (PAGE_SIZE)

//each MQD has 2K bytes. add 2K byte for page alignment.
//support up to 8 priviate ring, 8 Pages are reserved.
#define AMDGPU_PRIV_MQD_SIZE (PAGE_SIZE)

//each ring bo has 8K bytes. Totally, 16 Pages are reserved.
#define AMDGPU_PRIV_RING_BUF_SIZE (2 * PAGE_SIZE)
#define AMDGPU_PRIV_RING_MAX_DW   (1024)

//0x7ffe000
#define AMDGPU_PRIV_WB_OFFSET (AMDGPU_VA_RESERVED_SIZE - \
			       AMDGPU_PRIV_WB_SIZE)

//0x7ff6000
#define AMDGPU_PRIV_HQD_EOP_OFFSET (AMDGPU_PRIV_WB_OFFSET - \
				    AMDGPU_PRIV_MEC_HQD_EOP_SIZE * 8)

//0x7fee000
#define AMDGPU_PRIV_MQD_OFFSET (AMDGPU_PRIV_HQD_EOP_OFFSET - \
				AMDGPU_PRIV_MQD_SIZE * 8)

//0x7fde000
#define AMDGPU_PRIV_RING_BUF_OFFSET (AMDGPU_PRIV_MQD_OFFSET - \
				    AMDGPU_PRIV_RING_BUF_SIZE * 8)

/*
 * Size of the per-process TBA+TMA buffer: 2 pages
 *
 * The first page is the TBA used for the CWSR ISA code. The second
 * page is used as TMA for daisy changing a user-mode trap handler.
 */
#define AMDGPU_PRIV_TBA_TMA_SIZE (PAGE_SIZE * 2)
#define AMDGPU_PRIV_TMA_OFFSET (AMDGPU_PRIV_RING_BUF_OFFSET - \
				PAGE_SIZE)
#define AMDGPU_PRIV_TBA_OFFSET (AMDGPU_PRIV_TMA_OFFSET - \
				PAGE_SIZE)

//sr buffer needs around 8M bytes for NV14
#define AMDGPU_PRIV_SR_OFFSET 0x1100000

#define AMDGPU_PRIV_VGPR_SIZE_PER_CU(asic_family)   \
		((asic_family) == CHIP_ARCTURUS ? 0x80000 : 0x40000)
#define AMDGPU_PRIV_SGPR_SIZE_PER_CU        0x4000
#define AMDGPU_PRIV_LDS_SIZE_PER_CU         0x10000
#define AMDGPU_PRIV_HWREG_SIZE_PER_CU       0x1000
#define AMDGPU_PRIV_WG_CONTEXT_DATA_SIZE_PER_CU(asic_family)  \
		(AMDGPU_PRIV_VGPR_SIZE_PER_CU(asic_family) + \
		AMDGPU_PRIV_SGPR_SIZE_PER_CU + AMDGPU_PRIV_LDS_SIZE_PER_CU + \
		AMDGPU_PRIV_HWREG_SIZE_PER_CU)

#define AMDGPU_PRIV_WAVES_PER_CU                    32
#define AMDGPU_PRIV_CNTL_STACK_BYTES_PER_WAVE       8

#endif
