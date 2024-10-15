/*
 * Copyright 2019 Advanced Micro Devices, Inc.
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
 * Author: Jonathan Kim <jonathan.kim@amd.com>
 *
 */

#include <linux/perf_event.h>
#include <linux/init.h>
#include "amdgpu.h"
#include "amdgpu_pmu.h"

#define PMU_NAME_SIZE 32

/* record to keep track of pmu entry per pmu type per device */
struct amdgpu_pmu_entry {
	struct list_head entry;
	struct amdgpu_device *adev;
	struct pmu pmu;
	unsigned int pmu_perf_type;
};

static LIST_HEAD(amdgpu_pmu_list);

/* init amdgpu_pmu */
int amdgpu_pmu_init(struct amdgpu_device *adev)
{
	switch (adev->asic_type) {
	default:
		return 0;
	}

	return 0;
}


/* destroy all pmu data associated with target device */
void amdgpu_pmu_fini(struct amdgpu_device *adev)
{
	struct amdgpu_pmu_entry *pe, *temp;

	list_for_each_entry_safe(pe, temp, &amdgpu_pmu_list, entry) {
		if (pe->adev == adev) {
			list_del(&pe->entry);
			perf_pmu_unregister(&pe->pmu);
			kfree(pe);
		}
	}
}
