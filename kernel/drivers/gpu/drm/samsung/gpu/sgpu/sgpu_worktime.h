/* SPDX-License-Identifier: GPL-2.0 WITH Linux-syscall-note */
/*
 * Copyright (c) 2023 Samsung Electronics Co., Ltd.
 *               http://www.samsung.com
 */

#ifndef _SGPU_WORKTIME_H_
#define _SGPU_WORKTIME_H_

#if IS_ENABLED(CONFIG_DRM_SGPU_WORKTIME)

#include <linux/atomic.h>
#include <linux/spinlock.h>

struct sgpu_worktime {
	struct kref		refcount;
	spinlock_t 		lock;

	uint32_t		gpu_id;
	uint32_t		uid;
	uint64_t		start_time;

	/* Count if multiple jobs are running */
	int			usage_count;
};

int sgpu_worktime_init(struct amdgpu_fpriv *fpriv, const uint32_t gid);
void sgpu_worktime_deinit(struct amdgpu_fpriv *fpriv);
void sgpu_worktime_start(struct amdgpu_job *job, struct dma_fence *fence);
void sgpu_worktime_cleanup(struct amdgpu_device *adev);

#else

#define sgpu_worktime_init(fpriv, gid)		(0)
#define sgpu_worktime_deinit(fpriv)		do { } while (0)
#define sgpu_worktime_start(job, fence)		do { } while (0)
#define sgpu_worktime_cleanup(adev)		do { } while (0)

#endif /* CONFIG_DRM_SGPU_WORKTIME */

#endif /* _SGPU_WORKTIME_H_ */
