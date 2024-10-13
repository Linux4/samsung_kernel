/*
 * Copyright 2016, 2020 Advanced Micro Devices, Inc.
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

#include "gc/gc_9_0_offset.h"
#include "gc/gc_9_0_sh_mask.h"

#include "soc15.h"
#include "soc15_common.h"

/**
 * soc15_program_register_sequence - program an array of registers.
 *
 * @adev: amdgpu_device pointer
 * @regs: pointer to the register array
 * @array_size: size of the register array
 *
 * Programs an array or registers with and and or masks.
 * This is a helper for setting golden registers.
 */

void soc15_program_register_sequence(struct amdgpu_device *adev,
					     const struct soc15_reg_golden *regs,
					     const u32 array_size)
{
	const struct soc15_reg_golden *entry;
	u32 tmp, reg;
	int i;

	for (i = 0; i < array_size; ++i) {
		entry = &regs[i];
		reg =  adev->reg_offset[entry->hwip][entry->instance][entry->segment] + entry->reg;

		if (entry->and_mask == 0xffffffff) {
			tmp = entry->or_mask;
		} else {
			tmp = RREG32(reg);
			tmp &= ~(entry->and_mask);
			tmp |= (entry->or_mask & entry->and_mask);
		}

		if (reg == SOC15_REG_OFFSET(GC, 0, mmPA_SC_BINNER_EVENT_CNTL_3) ||
			reg == SOC15_REG_OFFSET(GC, 0, mmPA_SC_ENHANCE) ||
			reg == SOC15_REG_OFFSET(GC, 0, mmPA_SC_ENHANCE_1) ||
			reg == SOC15_REG_OFFSET(GC, 0, mmSH_MEM_CONFIG))
			WREG32_RLC(reg, tmp);
		else
			WREG32(reg, tmp);

	}

}
