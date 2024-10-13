// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2023 Samsung Electronics Co., Ltd.
 *               http://www.samsung.com
 */

#include <linux/ktime.h>

#include "amdgpu.h"

#define CREATE_TRACE_POINTS
#include "sgpu_worktime_trace.h"

static DEFINE_IDR(wt_mgr_idr);
static DEFINE_MUTEX(wt_mgr_lock);

static void sgpu_worktime_release(struct kref *ref)
{
	struct sgpu_worktime *tmp, *wt = container_of(ref, struct sgpu_worktime,
							refcount);

	if (wt->usage_count)
		DRM_WARN("unstable worktime release : uid(%d) usage_count(%d)",
			wt->uid, wt->usage_count);

	tmp = idr_remove(&wt_mgr_idr, wt->uid);
	if (!tmp)
		DRM_WARN("%s : not found uid(%d)", __func__, wt->uid);

	kfree(wt);
}

/**
 * sgpu_worktime_init() - initialize sgpu_worktime
 * @adev: pointer to amdgpu_device
 * @fpriv: pointer to amdgpu_fpriv
 * @gid: GPU id at which the GPU work will be running
 *
 * Initialize sgpu_worktime in amdgpu_fpriv. For the case when system uses
 * multiple GPUs, gid should be specified to indicate where each GPU work is
 * running. Since gpu_work_period is recorded per UID, multiple drm_files can
 * have same UID. The driver tries finding sgpu_worktime object which has
 * requested UID or creates new object if it's failed.
 *
 * Return: 0 if initialization is succeeded.
 */
int sgpu_worktime_init(struct amdgpu_fpriv *fpriv, const uint32_t gid)
{
	struct sgpu_worktime *wt;
	uint32_t uid = (uint32_t)current_uid().val;
	int r;

	mutex_lock(&wt_mgr_lock);

	wt = (struct sgpu_worktime *)idr_find(&wt_mgr_idr, uid);
	if (wt) {
		kref_get(&wt->refcount);
		fpriv->worktime = wt;
		mutex_unlock(&wt_mgr_lock);
		return 0;
	}

	wt = kmalloc(sizeof(*wt), GFP_KERNEL);
	if (!wt) {
		r = -ENOMEM;
		goto error_alloc;
	}

	r = idr_alloc(&wt_mgr_idr, wt, uid, uid + 1, GFP_KERNEL);
	if (r != uid)
		goto error_idr;

	kref_init(&wt->refcount);
	spin_lock_init(&wt->lock);

	wt->usage_count = 0;

	wt->gpu_id = gid;
	wt->uid = uid;
	wt->start_time = ktime_get_ns();

	fpriv->worktime = wt;

	trace_gpu_work_period(wt->gpu_id, wt->uid, wt->start_time,
				wt->start_time + 1, 0);

	mutex_unlock(&wt_mgr_lock);

	return 0;

error_idr:
	kfree(wt);
error_alloc:
	mutex_unlock(&wt_mgr_lock);

	return r;
}

/**
 * sgpu_worktime_deinit() - deinitialize sgpu_worktime
 * @fpriv: pointer to amdgpu_fpriv
 *
 * Deinitialize sgpu_worktime. No need to report worktime because last report
 * should be done when drm_file is destroyed.
 */
void sgpu_worktime_deinit(struct amdgpu_fpriv *fpriv)
{
	mutex_lock(&wt_mgr_lock);
	kref_put(&fpriv->worktime->refcount, sgpu_worktime_release);
	mutex_unlock(&wt_mgr_lock);
}

/**
 * sgpu_worktime_report() - report worktime via tracepoint
 * @wt: pointer to sgpu_worktime
 *
 * If reported process is still in GPU activation period, modify start_time and
 * report. Left active_duration will be reported next time when remained jobs
 * get finished. If the job execution time is over a second, the tracepoint will
 * be emitted multiple times with separated period.
 */
static inline void sgpu_worktime_report(struct sgpu_worktime *wt)
{
	uint64_t start_time = wt->start_time;
	uint64_t end_time = ktime_get_ns();
	uint64_t active_duration = end_time - start_time;

	if (start_time > end_time) {
		DRM_WARN("skip trace : uid(%u) start(%llu) end(%llu) use(%d)",
				wt->uid, start_time, end_time, wt->usage_count);
		return;
	}

	while (end_time - start_time > NSEC_PER_SEC) {
		trace_gpu_work_period(wt->gpu_id, wt->uid, start_time,
				start_time + NSEC_PER_SEC, NSEC_PER_SEC);
		start_time += NSEC_PER_SEC;
		active_duration -= NSEC_PER_SEC;
	}

	trace_gpu_work_period(wt->gpu_id, wt->uid, start_time,
				end_time, active_duration);

	wt->start_time = end_time;
}

/**
 * sgpu_worktime_end() - calculate GPU active duration
 * @job: pointer to amdgpu_job which is finished
 *
 * Decrease usage_count after calling sgpu_worktime_report() because it doesn't
 * calculate active_duration when usage_count is zero.
 */
static void sgpu_worktime_end(struct amdgpu_job *job)
{
	struct amdgpu_fpriv *fpriv;
	struct sgpu_worktime *wt;
	unsigned long flags;

	fpriv = container_of(job->vm, struct amdgpu_fpriv, vm);
	wt = fpriv->worktime;

	spin_lock_irqsave(&wt->lock, flags);

	sgpu_worktime_report(wt);
	--wt->usage_count;

	spin_unlock_irqrestore(&wt->lock, flags);
}

/**
 * sgpu_worktime_end_cb() - callback to measure GPU active duration
 * @f: pointer to dma_fence
 * @cb: pointer to dma_fence_cb in which this function is registered
 *
 * When each GPU job is finished, the job's fence gets signaled. This function
 * is registered as the fence's callback so that GPU driver calculates and
 * reports active_duration when the job is finished.
 */
static void sgpu_worktime_end_cb(struct dma_fence *f, struct dma_fence_cb *cb)
{
	struct amdgpu_job *job;

	if (!cb) {
		DRM_ERROR("Skip measuring gpu_work_period");
		return;
	}

	job = container_of(cb, struct amdgpu_job, wt_cb);

	sgpu_worktime_end(job);
}

/**
 * sgpu_worktime_start() - add callback function on job's fence
 * @job: pointer to amdgpu_job which will be running on GPU HW
 * @fence: pointer to dma_fence connected to the job
 *
 * When the job is pushed into GPU HW, it means GPU is activated for the job.
 * Checking usage_count of the process(amdgpu_fpriv), update start_time if it is
 * first work. The callback function is added on the job's fence to calculate
 * active_duration when the job is finished.
 */
void sgpu_worktime_start(struct amdgpu_job *job, struct dma_fence *fence)
{
	struct amdgpu_fpriv *fpriv;
	struct sgpu_worktime *wt;
	unsigned long flags;
	int r;

	if (!job || !job->vm)
		return;

	fpriv = container_of(job->vm, struct amdgpu_fpriv, vm);
	wt = fpriv->worktime;

	r = dma_fence_add_callback(fence, &job->wt_cb, sgpu_worktime_end_cb);
	if (r)
		return;

	spin_lock_irqsave(&wt->lock, flags);

	/* Update start_time if it's first job */
	if (++wt->usage_count == 1)
		wt->start_time = ktime_get_ns();

	spin_unlock_irqrestore(&wt->lock, flags);
}

/**
 * sgpu_worktime_cleanup() - clear worktime data
 * @adev: pointer to amdgpu_device
 *
 * When drm_sched_stop() and drm_sched_start() are called, all HW-pushed jobs
 * are going to be resubmitted. This function makes all sgpu_worktime objects
 * skip reporting each active_duration remained and clear usage_count value.
 * Also only job done callback is removed from jobs' fence in drm_sched_stop().
 * So GPU driver needs to manually remove worktime callback from the fences.
 */
void sgpu_worktime_cleanup(struct amdgpu_device *adev)
{
	struct sgpu_worktime *wt;
	uint32_t id;

	struct drm_gpu_scheduler *sched;
	struct drm_sched_job *s_job, *tmp;
	struct amdgpu_ring *ring;
	struct amdgpu_job *a_job;
	int i;

	/* worktime callback should be removed from the jobs' fence */
	for (i = 0; i < AMDGPU_MAX_RINGS; ++i) {
		ring = adev->rings[i];
		if (!ring || !ring->sched.thread)
			continue;

		sched = &ring->sched;
		list_for_each_entry_safe(s_job, tmp,
					&sched->pending_list, list) {
			a_job = container_of(s_job, struct amdgpu_job, base);
			if (s_job->s_fence->parent)
				dma_fence_remove_callback(
						s_job->s_fence->parent,
						&a_job->wt_cb);
		}
	}

	mutex_lock(&wt_mgr_lock);

	/*
	 * Since there's no chance to call ftrace after removing callbacks,
	 * reset usage_count only without setting critical section.
	 */
	idr_for_each_entry(&wt_mgr_idr, wt, id)
		wt->usage_count = 0;

	mutex_unlock(&wt_mgr_lock);
}
