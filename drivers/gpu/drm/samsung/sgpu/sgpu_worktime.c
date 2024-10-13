/*
 * @file sgpu_worktime.c
 * @copyright 2022 Samsung Electronics
 */

#include <linux/ktime.h>

#include "amdgpu.h"

/* It MUST specified at least once to use tracepoint functions. */
#define CREATE_TRACE_POINTS
#include "sgpu_worktime_trace.h"


/*
 * invoke tracepoint
 * if a process is in gpu activation, modify active_start_time and report.
 */
static inline void sgpu_worktime_report(struct sgpu_worktime *wt)
{
	wt->end_time = ktime_get_ns();

	if (atomic_read(&wt->usage_count) > 0) {
		wt->active_duration += (wt->end_time - wt->active_start_time);
		wt->active_start_time = wt->end_time;
	}

	trace_gpu_work_period(wt->gpu_id, wt->uid, wt->start_time,
			wt->end_time, wt->active_duration);
	trace_gpu_work_period_debug(wt, wt->uid,
			wt->start_time, wt->end_time,
			wt->active_duration);

	wt->start_time = wt->end_time;
	wt->active_duration = 0;
}


/*
 * invoked when amdgpu_fpriv struct is created.
 * initialize sgpu_worktime structure members.
 */
void sgpu_worktime_init(struct amdgpu_fpriv *fpriv, const uint32_t gid)
{
	struct sgpu_worktime *wt = &fpriv->worktime;

	atomic_set(&wt->usage_count, 0);

	wt->gpu_id = gid;
	wt->uid = (uint32_t)fpriv->tgid;
	wt->start_time = ktime_get_ns();
	wt->end_time = 0;
	wt->active_duration = 0;
	wt->active_start_time = 0;

	trace_gpu_work_period_open(wt, wt->uid);
}


/*
 * invoked when amdgpu_fpriv struct is destroyed.
 * not need to report because last report is succeeded at last job_free_cb()
 */
void sgpu_worktime_deinit(struct amdgpu_fpriv *fpriv)
{
	struct sgpu_worktime *wt = &fpriv->worktime;

	trace_gpu_work_period_close(wt, wt->uid);
}

/*
 * invoked when gpu job is ended (job's fence is signaled)
 * decrease usage_count to prevent double counting from multible jobs.
 * report active period every end of job
 */
static void sgpu_worktime_end(struct amdgpu_job *job)
{
	struct amdgpu_fpriv *fpriv;
	struct sgpu_worktime *wt;

	if (!job || !job->vm)
		return;

	fpriv = container_of(job->vm, struct amdgpu_fpriv, vm);
	wt = &fpriv->worktime;

	/*
	 * Need to calcuate active_duration if it is last job,
	 * because sgpu_worktime_report() doesn't calculate active_duration
	 * since usage_count is already 0 at invoke time.
	 */
	if (atomic_dec_return(&wt->usage_count) == 0)
		wt->active_duration += (ktime_get_ns() - wt->active_start_time);

	sgpu_worktime_report(wt);
}

/*
 * sgpu_worktime_end_cb - the callback for measureing end_time
 */
static void sgpu_worktime_end_cb(struct dma_fence *f, struct dma_fence_cb *cb)
{
	struct amdgpu_job *job = container_of(cb, struct amdgpu_job, wt_cb);

	sgpu_worktime_end(job);
}

/*
 * invoked when gpu job is started (job_run() invoked)
 * increase usage_count to prevent double counting from multible jobs.
 * report inactive period and set active_start_time when first job is started.
 */
void sgpu_worktime_start(struct amdgpu_job *job, struct dma_fence *fence)
{
	struct amdgpu_fpriv *fpriv;
	struct sgpu_worktime *wt;
	int r;

	if (!job || !job->vm)
		return;

	fpriv = container_of(job->vm, struct amdgpu_fpriv, vm);
	wt = &fpriv->worktime;

	/* Check if it's first work */
	if (atomic_inc_return(&wt->usage_count) == 1)
		wt->active_start_time = ktime_get_ns();

	r = dma_fence_add_callback(fence, &job->wt_cb, sgpu_worktime_end_cb);
	if (r)
		atomic_dec(&wt->usage_count);
}

static void sgpu_worktime_remove_callback(struct amdgpu_device *adev)
{
	struct drm_gpu_scheduler *sched;
	struct drm_sched_job *s_job, *tmp;
	struct amdgpu_ring *ring;
	struct amdgpu_job *a_job;
	int i;

	for (i = 0; i < AMDGPU_MAX_RINGS; ++i) {
		ring = adev->rings[i];
		if (!ring || !ring->sched.thread)
			continue;

		sched = &ring->sched;
		list_for_each_entry_safe(s_job, tmp,
					&sched->pending_list, list) {
			a_job = container_of(s_job, struct amdgpu_job, base);
			if (s_job->s_fence->parent)
				dma_fence_remove_callback(s_job->s_fence->parent,
						&a_job->wt_cb);
		}
	}
}

/*
 * invoked when gpu recover is operating.
 * resubmitting job should occur during gpu recovery,
 * so usage_count value should be cleared.
 */
void sgpu_worktime_fault_recover(struct amdgpu_device *adev)
{
	struct drm_device *ddev = adev_to_drm(adev);
	struct drm_file *file = NULL;
	struct amdgpu_fpriv *fpriv = NULL;
	struct sgpu_worktime *wt;
	int r = 0;

	r = mutex_lock_interruptible(&ddev->filelist_mutex);

	if (r)
		return;

	list_for_each_entry(file, &ddev->filelist, lhead) {
		fpriv = file->driver_priv;
		if (!fpriv || drm_is_primary_client(file))
			continue;

		wt = &fpriv->worktime;

		sgpu_worktime_report(wt);

		atomic_set(&wt->usage_count, 0);
	}

	mutex_unlock(&ddev->filelist_mutex);

	/* should remove sgpu_worktime_end_cb from hw_fence */
	sgpu_worktime_remove_callback(adev);
}

/*
 * invoked when system suspend occurs.
 * invoke trace for all drm_files because all submitted jobs should be stopped.
 */
void sgpu_worktime_suspend(struct amdgpu_device *adev)
{
	struct drm_device *ddev = adev_to_drm(adev);
	struct drm_file *file = NULL;
	struct amdgpu_fpriv *fpriv = NULL;
	struct sgpu_worktime *wt;
	uint64_t curr_time;
	int r = 0;

	r = mutex_lock_interruptible(&ddev->filelist_mutex);

	if (r)
		return;

	curr_time = ktime_get_ns();
	trace_gpu_work_period_suspend(curr_time);

	list_for_each_entry(file, &ddev->filelist, lhead) {
		fpriv = file->driver_priv;
		if (!fpriv || drm_is_primary_client(file))
			continue;

		wt = &fpriv->worktime;
		sgpu_worktime_report(wt);
	}

	mutex_unlock(&ddev->filelist_mutex);

	/* should remove sgpu_worktime_end_cb from hw_fence */
	sgpu_worktime_remove_callback(adev);
}

/*
 * invoked when system resume occurs.
 * clear usage_count for all drm_files because resubmitting job will occur.
 */
void sgpu_worktime_resume(struct amdgpu_device *adev)
{
	struct drm_device *ddev = adev_to_drm(adev);
	struct drm_file *file = NULL;
	struct amdgpu_fpriv *fpriv = NULL;
	struct sgpu_worktime *wt;
	uint64_t curr_time;
	int r = 0;

	r = mutex_lock_interruptible(&ddev->filelist_mutex);

	if (r)
		return;

	curr_time = ktime_get_ns();
	trace_gpu_work_period_resume(curr_time);

	list_for_each_entry(file, &ddev->filelist, lhead) {
		fpriv = file->driver_priv;
		if (!fpriv || drm_is_primary_client(file))
			continue;

		wt = &fpriv->worktime;
		wt->start_time = curr_time;
		wt->end_time = 0;
		wt->active_duration = 0;
		wt->active_start_time = 0;

		atomic_set(&wt->usage_count, 0);
	}

	mutex_unlock(&ddev->filelist_mutex);
}
