/*
 * Copyright 2021 Advanced Micro Devices, Inc.
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
#include "amdgpu_atombios.h"
#include "hdp_dummy.h"

static void hdp_dummy_flush_hdp(struct amdgpu_device *adev,
				struct amdgpu_ring *ring)
{
	return;
}

static void hdp_dummy_invalidate_hdp(struct amdgpu_device *adev,
				    struct amdgpu_ring *ring)
{
	return;
}

static void hdp_dummy_update_clock_gating(struct amdgpu_device *adev,
					      bool enable)
{
	return;
}

static void hdp_dummy_get_clockgating_state(struct amdgpu_device *adev,
					    u32 *flags)
{
	return;
}

static void hdp_dummy_init_registers(struct amdgpu_device *adev)
{
	return;
}

const struct amdgpu_hdp_funcs hdp_dummy_funcs = {
	.flush_hdp = hdp_dummy_flush_hdp,
	.invalidate_hdp = hdp_dummy_invalidate_hdp,
	.update_clock_gating = hdp_dummy_update_clock_gating,
	.get_clock_gating_state = hdp_dummy_get_clockgating_state,
	.init_registers = hdp_dummy_init_registers,
};
