/*
 * @file sgpu_worktime.h
 * @copyright 2022 Samsung Electronics
 */

#ifndef __SGPU_WORKTIME_H__
#define __SGPU_WORKTIME_H__

/*
 * this header file is created to implement below specification in :
 * platform/frameworks/native/services/gpuservice/gpuwork/bpfprogs/gpu_work.c
 */

#include <linux/atomic.h>

struct sgpu_worktime {
	uint32_t                gpu_id;
	uint32_t                uid;
	uint64_t                start_time;
	uint64_t                end_time;
	uint64_t                active_duration;

	/* Start time of gpu activation */
	uint64_t                active_start_time;

	/* To check if multiple job is running */
	atomic_t                usage_count;
};

void sgpu_worktime_init(struct amdgpu_fpriv *fpriv, const uint32_t gid);
void sgpu_worktime_deinit(struct amdgpu_fpriv *fpriv);

void sgpu_worktime_start(struct amdgpu_job *job, struct dma_fence *fence);

void sgpu_worktime_fault_recover(struct amdgpu_device *adev);

void sgpu_worktime_suspend(struct amdgpu_device *adev);
void sgpu_worktime_resume(struct amdgpu_device *adev);

#endif
