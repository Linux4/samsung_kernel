/*
 * Copyright (C) 2018-2021 Advanced Micro Devices, Inc. All rights Reserved.
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
#include "amdgpu.h"
#include "soc15.h"

#include "gc/gc_10_4_0_offset.h"
#include "gc/gc_10_4_0_sh_mask.h"
#include "gc/gc_10_4_0_default.h"

#include "gc/gc_10_4_0_offset.h"
#include "gc/gc_10_4_0_sh_mask.h"
#include "gc/gc_10_4_0_default.h"

#include "soc15_common.h"
#include "soc15_hw_ip.h"
#include "emu_mobile0_asic_init.h"
#include "emu_mobile1_asic_init.h"
#include "emu_mobile2_asic_init.h"

static void emu_mobile0_asic_init(struct amdgpu_device *adev)
{

	int i = 0;

	for (i = 0; i < ARRAY_SIZE(emu_mobile0_setting_cl78945); i++) {
		unsigned int val = emu_mobile0_setting_cl78945[i].val;
		unsigned int reg = emu_mobile0_setting_cl78945[i].reg;

		WREG32(reg, val);
		DRM_INFO("M0, try to set reg:0x%x to value:0x%x\n", reg,
				val);
	}
}

static void emu_mobile1_asic_init(struct amdgpu_device *adev)
{
	int i = 0;

	for (i = 0; i < ARRAY_SIZE(emu_mobile1_setting_revision_3); i++) {
		uint32_t reg = emu_mobile1_setting_revision_3[i].reg;
		uint32_t val = emu_mobile1_setting_revision_3[i].val;

		/* The original reg address is byte alignment */
		WREG32(reg / 4, val);
		DRM_DEBUG("M1 init 0x%x(dword:0x%x)=0x%x!\n", reg, reg / 4, val);
	}
}

static void emu_mobile2_asic_init(struct amdgpu_device *adev)
{
	int i = 0;

	for (i = 0; i < ARRAY_SIZE(emu_mobile2_setting_cl135710); i++) {
		uint32_t reg = emu_mobile2_setting_cl135710[i].reg;
		uint32_t val = emu_mobile2_setting_cl135710[i].val;

		/* The original reg address is byte alignment */
		WREG32(reg / 4, val);
		DRM_INFO("M2 init 0x%x(dword:0x%x)=0x%x!\n", reg, reg / 4, val);
	}
}

int emu_soc_asic_init(struct amdgpu_device *adev)
{
	if (AMDGPU_IS_MGFX0(adev->grbm_chip_rev))
		emu_mobile0_asic_init(adev);
	else if (AMDGPU_IS_MGFX1(adev->grbm_chip_rev))
		emu_mobile1_asic_init(adev);
	else if (AMDGPU_IS_MGFX2(adev->grbm_chip_rev))
		emu_mobile2_asic_init(adev);

	/* Change register's default value:
	 * SQ_CONFIG: NEW_TRANS_ARB_SCHEME (bit 7)
	 */
	if (AMDGPU_IS_MGFX0(adev->grbm_chip_rev) ||
	    AMDGPU_IS_MGFX1(adev->grbm_chip_rev)) {
		uint32_t data = RREG32_SOC15(GC, 0, mmSQ_CONFIG);

		data = REG_SET_FIELD(data, SQ_CONFIG,
				NEW_TRANS_ARB_SCHEME, 1);
		WREG32(SOC15_REG_OFFSET(GC, 0, mmSQ_CONFIG), data);
	}

	/*
	 * [SW Workaround] New wave prefetch from SQG during GCR requests
	 * corrupts SQC I$ tags:
	 * Disable the prefetch logic by setting SQG_CONFIG.SQG_ICPFT_EN=0
	 */
	if (AMDGPU_IS_MGFX0(adev->grbm_chip_rev)) {
		uint32_t data = RREG32_SOC15(GC, 0, mmSQG_CONFIG);

		data = REG_SET_FIELD(data, SQG_CONFIG,
				SQG_ICPFT_EN, 0);
		WREG32_SOC15(GC, 0, mmSQG_CONFIG, data);
	}

	/* EVT0: Reject/SWFIX/GOLDENFIX: Golden value change to disable this
	 * MGCG domain
	 * EVT1: RTL Fix
	 * So only EVT0 is required to program this fix
	 */
	if (AMDGPU_IS_MGFX1_EVT0(adev->grbm_chip_rev))
		WREG32_SOC15(GC, 0, mmDB_CGTT_CLK_CTRL_0, 0x00001100);


	/* Change register's default value:
	 * SQ_CONFIG: NEW_TRANS_ARB_SCHEME (bit 7)
	 */
	if (AMDGPU_IS_MGFX0(adev->grbm_chip_rev) ||
	    AMDGPU_IS_MGFX1(adev->grbm_chip_rev)) {
		uint32_t data = RREG32_SOC15(GC, 0, mmSQ_CONFIG);

		data = REG_SET_FIELD(data, SQ_CONFIG,
				NEW_TRANS_ARB_SCHEME, 1);
		WREG32(SOC15_REG_OFFSET(GC, 0, mmSQ_CONFIG), data);
	}

	/*
	 * [SW Workaround] New wave prefetch from SQG during GCR requests
	 * corrupts SQC I$ tags:
	 * Disable the prefetch logic by setting SQG_CONFIG.SQG_ICPFT_EN=0
	 */
	if (AMDGPU_IS_MGFX0(adev->grbm_chip_rev)) {
		uint32_t data = RREG32_SOC15(GC, 0, mmSQG_CONFIG);

		data = REG_SET_FIELD(data, SQG_CONFIG,
				SQG_ICPFT_EN, 0);
		WREG32_SOC15(GC, 0, mmSQG_CONFIG, data);
	}

	/* EVT0: Reject/SWFIX/GOLDENFIX: Golden value change to disable this
	 * MGCG domain
	 * EVT1: RTL Fix
	 * So only EVT0 is required to program this fix
	 */
	if (AMDGPU_IS_MGFX1_EVT0(adev->grbm_chip_rev))
		WREG32_SOC15(GC, 0, mmDB_CGTT_CLK_CTRL_0, 0x00000100);

	return 0;
}

/* RSMU registers required for the boot sequence. This is not the complete regsiter spec */
#define RSMU_AEB_LOCK_0_GC                              0x40800
#define RSMU_AEB_LOCK_1_GC                              0x40804
#define RSMU_COLD_RESETB_GC                             0x40004
#define RSMU_HARD_RESETB_GC                             0x40008
#define RSMU_CUSTOM_HARD_RESETB_GC	                0x402e0
#define RSMU_SEC_MASTER_TRUST_LEVEL_6_GC	        0x40848
#define RSMU_SEC_MISC_MASK_SET0_GROUP_DEFAULT_GC	0x40920
#define RSMU_SEC_MISC_MASK_SET1_GROUP_DEFAULT_GC	0x4092c
#define RSMU_SEC_ACCESS_CONTROL_GROUP_DEFAULT_GC	0x40930
#define RSMU_SEC_START_ADDR_GROUP_0_GC	                0x40934
#define RSMU_SEC_END_ADDR_GROUP_0_GC	                0x40938
#define RSMU_SEC_MISC_MASK_SET0_GROUP_0_GC	        0x40944
#define RSMU_SEC_MISC_MASK_SET1_GROUP_0_GC	        0x40950
#define RSMU_SEC_ACCESS_CONTROL_GROUP_0_GC      	0x40954
#define RSMU_SEC_START_ADDR_GROUP_1_GC          	0x40958
#define RSMU_SEC_END_ADDR_GROUP_1_GC	                0x4095c
#define RSMU_SEC_MISC_MASK_SET0_GROUP_1_GC	        0x40968
#define RSMU_SEC_MISC_MASK_SET1_GROUP_1_GC	        0x40974
#define RSMU_SEC_ACCESS_CONTROL_GROUP_1_GC	        0x40978
#define RSMU_SEC_START_ADDR_GROUP_2_GC	                0x4097c
#define RSMU_SEC_END_ADDR_GROUP_2_GC	                0x40980
#define RSMU_SEC_MISC_MASK_SET0_GROUP_2_GC              0x4098c
#define RSMU_SEC_MISC_MASK_SET1_GROUP_2_GC              0x40998
#define RSMU_SEC_ACCESS_CONTROL_GROUP_2_GC              0x4099c
#define RSMU_SEC_START_ADDR_GROUP_3_GC	                0x409a0
#define RSMU_SEC_END_ADDR_GROUP_3_GC	                0x409a4
#define RSMU_SEC_MISC_MASK_SET0_GROUP_3_GC	        0x409b0
#define RSMU_SEC_MISC_MASK_SET1_GROUP_3_GC	        0x409bc
#define RSMU_SEC_ACCESS_CONTROL_GROUP_3_GC       	0x409c0
#define RSMU_SEC_START_ADDR_GROUP_4_GC	                0x409c4
#define RSMU_SEC_END_ADDR_GROUP_4_GC            	0x409c8
#define RSMU_SEC_MISC_MASK_SET0_GROUP_4_GC              0x409d4
#define RSMU_SEC_MISC_MASK_SET1_GROUP_4_GC              0x409e0
#define RSMU_SEC_ACCESS_CONTROL_GROUP_4_GC              0x409e4
#define RSMU_SEC_MASTER_TRUST_LEVEL_RSMU_GC	        0x40910
#define RSMU_SEC_ACCESS_CONTROL_RSMU_GC	                0x40914
#define RSMU_SEC_SLAVE_RANGE_ENABLE_GC	                0x40db8

void sgpu_mm_wreg32(void __iomem *addr, uint32_t value)
{
	writel(value, addr);
}

void emu_write_rsmu_registers(struct amdgpu_device *adev)
{
	DRM_INFO("%s] Configure RSMU: register mmio base: 0x%px\n",__func__, (void*)adev->rmmio_base);
	DRM_INFO("%s] Configure RSMU: register mmio size: %u\n",__func__, (unsigned)adev->rmmio_size);
	if (adev->no_hw_access != 0)
		return;
	sgpu_mm_wreg32(adev->rmmio + RSMU_AEB_LOCK_0_GC, 0xfffffff8);
	sgpu_mm_wreg32(adev->rmmio + RSMU_AEB_LOCK_1_GC, 0xffffffff);
	sgpu_mm_wreg32(adev->rmmio + RSMU_COLD_RESETB_GC, 0x00000001);
	sgpu_mm_wreg32(adev->rmmio + RSMU_HARD_RESETB_GC, 0x00000001);
	sgpu_mm_wreg32(adev->rmmio + RSMU_CUSTOM_HARD_RESETB_GC, 0x00000007);
	sgpu_mm_wreg32(adev->rmmio + RSMU_SEC_MASTER_TRUST_LEVEL_6_GC, 0x00ffffdb);
	sgpu_mm_wreg32(adev->rmmio + RSMU_SEC_MISC_MASK_SET0_GROUP_DEFAULT_GC, 0x000001ff);
	sgpu_mm_wreg32(adev->rmmio + RSMU_SEC_MISC_MASK_SET1_GROUP_DEFAULT_GC, 0x000002ff);
	sgpu_mm_wreg32(adev->rmmio + RSMU_SEC_ACCESS_CONTROL_GROUP_DEFAULT_GC, 0x00000036);
	sgpu_mm_wreg32(adev->rmmio + RSMU_SEC_START_ADDR_GROUP_0_GC, 0x0003f000);
	sgpu_mm_wreg32(adev->rmmio + RSMU_SEC_END_ADDR_GROUP_0_GC, 0x0003ffff);
	sgpu_mm_wreg32(adev->rmmio + RSMU_SEC_MISC_MASK_SET0_GROUP_0_GC, 0x00000101);
	sgpu_mm_wreg32(adev->rmmio + RSMU_SEC_MISC_MASK_SET1_GROUP_0_GC, 0x00000201);
	sgpu_mm_wreg32(adev->rmmio + RSMU_SEC_ACCESS_CONTROL_GROUP_0_GC, 0x00000036);
	sgpu_mm_wreg32(adev->rmmio + RSMU_SEC_START_ADDR_GROUP_1_GC, 0x0003e000);
	sgpu_mm_wreg32(adev->rmmio + RSMU_SEC_END_ADDR_GROUP_1_GC, 0x0003efff);
	sgpu_mm_wreg32(adev->rmmio + RSMU_SEC_MISC_MASK_SET0_GROUP_1_GC, 0x00000107);
	sgpu_mm_wreg32(adev->rmmio + RSMU_SEC_MISC_MASK_SET1_GROUP_1_GC, 0x00000207);
	sgpu_mm_wreg32(adev->rmmio + RSMU_SEC_ACCESS_CONTROL_GROUP_1_GC, 0x00000036);
	sgpu_mm_wreg32(adev->rmmio + RSMU_SEC_START_ADDR_GROUP_2_GC, 0x0003a000);
	sgpu_mm_wreg32(adev->rmmio + RSMU_SEC_END_ADDR_GROUP_2_GC, 0x0003afff);
	sgpu_mm_wreg32(adev->rmmio + RSMU_SEC_MISC_MASK_SET0_GROUP_2_GC,0x00000107);
	sgpu_mm_wreg32(adev->rmmio + RSMU_SEC_MISC_MASK_SET1_GROUP_2_GC,0x00000207);
	sgpu_mm_wreg32(adev->rmmio + RSMU_SEC_ACCESS_CONTROL_GROUP_2_GC,0x00000036);
	sgpu_mm_wreg32(adev->rmmio + RSMU_SEC_START_ADDR_GROUP_3_GC, 0x0003a01c);
	sgpu_mm_wreg32(adev->rmmio + RSMU_SEC_END_ADDR_GROUP_3_GC, 0x0003a027);
	sgpu_mm_wreg32(adev->rmmio + RSMU_SEC_MISC_MASK_SET0_GROUP_3_GC, 0x00000107);
	sgpu_mm_wreg32(adev->rmmio + RSMU_SEC_MISC_MASK_SET1_GROUP_3_GC, 0x000002ff);
	sgpu_mm_wreg32(adev->rmmio + RSMU_SEC_ACCESS_CONTROL_GROUP_3_GC, 0x00000036);
	sgpu_mm_wreg32(adev->rmmio + RSMU_SEC_START_ADDR_GROUP_4_GC, 0x0003b20c);
	sgpu_mm_wreg32(adev->rmmio + RSMU_SEC_END_ADDR_GROUP_4_GC, 0x0003b21b);
	sgpu_mm_wreg32(adev->rmmio + RSMU_SEC_MISC_MASK_SET0_GROUP_4_GC, 0x00000107);
	sgpu_mm_wreg32(adev->rmmio + RSMU_SEC_MISC_MASK_SET1_GROUP_4_GC, 0x00000207);
	sgpu_mm_wreg32(adev->rmmio + RSMU_SEC_ACCESS_CONTROL_GROUP_4_GC, 0x00000036);
	sgpu_mm_wreg32(adev->rmmio + RSMU_SEC_MASTER_TRUST_LEVEL_RSMU_GC, 0x004bffff);
	sgpu_mm_wreg32(adev->rmmio + RSMU_SEC_ACCESS_CONTROL_RSMU_GC, 0x00007fff);
	sgpu_mm_wreg32(adev->rmmio + RSMU_SEC_SLAVE_RANGE_ENABLE_GC, 0x0000001f);
}

void emu_write_rsmu_registers_m1(struct amdgpu_device *adev)
{
	DRM_INFO("%s] Configure RSMU: register mmio base: 0x%px\n",__func__, (void*)adev->rmmio_base);
	DRM_INFO("%s] Configure RSMU: register mmio size: %u\n",__func__, (unsigned)adev->rmmio_size);
	if (adev->no_hw_access != 0)
		return;

	sgpu_mm_wreg32(adev->rmmio + RSMU_AEB_LOCK_0_GC, 0xfffffff8);
	sgpu_mm_wreg32(adev->rmmio + RSMU_AEB_LOCK_1_GC, 0xffffffff);
	sgpu_mm_wreg32(adev->rmmio + RSMU_SEC_START_ADDR_GROUP_0_GC, 0x0003e000);
	sgpu_mm_wreg32(adev->rmmio + RSMU_SEC_END_ADDR_GROUP_0_GC, 0x0003f0ff);
	sgpu_mm_wreg32(adev->rmmio + RSMU_SEC_MISC_MASK_SET0_GROUP_0_GC, 0x00000101);
	sgpu_mm_wreg32(adev->rmmio + RSMU_SEC_MISC_MASK_SET1_GROUP_0_GC, 0x00000201);
	sgpu_mm_wreg32(adev->rmmio + RSMU_SEC_START_ADDR_GROUP_1_GC, 0x0003f110);
	sgpu_mm_wreg32(adev->rmmio + RSMU_SEC_END_ADDR_GROUP_1_GC, 0x0003f13f);
	sgpu_mm_wreg32(adev->rmmio + RSMU_SEC_START_ADDR_GROUP_2_GC, 0x0003f18c);
	sgpu_mm_wreg32(adev->rmmio + RSMU_SEC_END_ADDR_GROUP_2_GC, 0x0003afff);
	sgpu_mm_wreg32(adev->rmmio + RSMU_SEC_MISC_MASK_SET0_GROUP_2_GC,0x00000101);
	sgpu_mm_wreg32(adev->rmmio + RSMU_SEC_MISC_MASK_SET1_GROUP_2_GC,0x00000201);
	sgpu_mm_wreg32(adev->rmmio + RSMU_SEC_SLAVE_RANGE_ENABLE_GC, 0x00000007);
	sgpu_mm_wreg32(adev->rmmio + RSMU_SEC_ACCESS_CONTROL_RSMU_GC, 0x00000000);
	sgpu_mm_wreg32(adev->rmmio + RSMU_HARD_RESETB_GC, 0x00000001);
}
