/* SPDX-License-Identifier: GPL-2.0 WITH Linux-syscall-note */
/*
 * Copyright (c) 2023 Samsung Electronics Co., Ltd.
 *            http://www.samsung.com
 */

#ifndef _SGPU_IFPO_H_
#define _SGPU_IFPO_H_

enum sgpu_ifpo_type {
	IFPO_DISABLED	= 0,
	IFPO_ABORT	= 1,
	IFPO_HALF_AUTO	= 2,

	IFPO_TYPE_LAST
};

struct sgpu_ifpo_func {
	void (*enable)(struct amdgpu_device *adev);
	void (*suspend)(struct amdgpu_device *adev);
	void (*resume)(struct amdgpu_device *adev);
	void (*power_off)(struct amdgpu_device *adev);
	void (*lock)(struct amdgpu_device *adev);
	void (*unlock)(struct amdgpu_device *adev);
	void (*reset)(struct amdgpu_device *adev);
	void (*set_user_enable)(struct amdgpu_device *adev, bool value);
	void (*set_runtime_enable)(struct amdgpu_device *adev, bool value);
	void (*set_afm_enable)(struct amdgpu_device *adev, bool value);
};

struct sgpu_ifpo {
	enum sgpu_ifpo_type	type;
	uint32_t		cal_id;

	atomic_t		count;
	bool			state;
	struct mutex		lock;

	bool			user_enable;
	bool			runtime_enable;
	bool			afm_enable;

	struct sgpu_ifpo_func	*func;
};

#ifdef CONFIG_DRM_SGPU_EXYNOS

void sgpu_ifpo_init(struct amdgpu_device *adev);
uint32_t sgpu_ifpo_get_hw_lock_value(struct amdgpu_device *adev);

#define sgpu_ifpo_enable(adev)				((adev)->ifpo.func->enable(adev))
#define sgpu_ifpo_suspend(adev)				((adev)->ifpo.func->suspend(adev))
#define sgpu_ifpo_resume(adev)				((adev)->ifpo.func->resume(adev))
#define sgpu_ifpo_power_off(adev)			((adev)->ifpo.func->power_off(adev))
#define sgpu_ifpo_lock(adev)				((adev)->ifpo.func->lock(adev))
#define sgpu_ifpo_unlock(adev)				((adev)->ifpo.func->unlock(adev))
#define sgpu_ifpo_reset(adev)				((adev)->ifpo.func->reset(adev))
#define sgpu_ifpo_set_user_enable(adev, value)		((adev)->ifpo.func->set_user_enable((adev), (value)))
#define sgpu_ifpo_set_runtime_enable(adev, value)	((adev)->ifpo.func->set_runtime_enable((adev), (value)))

#ifdef CONFIG_DRM_SGPU_AFM
#define sgpu_ifpo_set_afm_enable(adev, value)		((adev)->ifpo.func->set_afm_enable((adev), (value)))
#else
#define sgpu_ifpo_set_afm_enable(adev, value)		do { } while (0)
#endif

#else

#define sgpu_ifpo_init(adev)				do { } while (0)
#define sgpu_ifpo_get_hw_lock_value(adev)		(0)

#define sgpu_ifpo_enable(adev)				do { } while (0)
#define sgpu_ifpo_suspend(adev)				do { } while (0)
#define sgpu_ifpo_resume(adev)				do { } while (0)
#define sgpu_ifpo_power_off(adev)			do { } while (0)
#define sgpu_ifpo_lock(adev)				do { } while (0)
#define sgpu_ifpo_unlock(adev)				do { } while (0)
#define sgpu_ifpo_reset(adev)				do { } while (0)
#define sgpu_ifpo_set_user_enable(adev, value)		do { } while (0)
#define sgpu_ifpo_set_runtime_enable(adev, value)	do { } while (0)
#define sgpu_ifpo_set_afm_enable(adev, value)		do { } while (0)

#endif /* CONFIG_DRM_SGPU_EXYNOS */

#endif /* _SGPU_IFPO_H_ */
