/* SPDX-License-Identifier: GPL-2.0 */
/*
 * TTM swap implementation for SGPU
 *
 * Copyright (c) 2022 Samsung Electronics Co., Ltd.
 */
#ifndef __SGPU_SWAP_H__
#define __SGPU_SWAP_H__

#ifdef CONFIG_DRM_SGPU_GRAPHIC_MEMORY_RECLAIM

#include <linux/list.h>
#include <linux/mutex.h>
#include <linux/kobject.h>

struct sgpu_proc_node;
struct sgpu_proc_ctx {
	struct list_head node;
	struct list_head bo_list;
	struct mutex lock;
	struct sgpu_proc_node *proc;
};

struct amdgpu_fpriv;
struct amdgpu_bo;

int sgpu_sysfs_proc_init(struct kobject *gpu_root);
int sgpu_proc_add_context(struct amdgpu_fpriv *fpriv);
void sgpu_proc_remove_context(struct amdgpu_fpriv *fpriv);

int sgpu_tt_swapin(struct ttm_tt *ttm);
void sgpu_swap_init_bo(struct amdgpu_bo *bo);
void sgpu_swap_add_bo(struct amdgpu_fpriv *fpriv, struct amdgpu_bo *bo);
void sgpu_swap_remove_bo(struct amdgpu_fpriv *fpriv, struct amdgpu_bo *bo);
void sgpu_tt_destroy_notify(struct ttm_tt *ttm);

extern struct kobj_attribute attr_gpu_swap_ctrl;

void sgpu_swap_lock(struct amdgpu_fpriv *fpriv);
void sgpu_swap_unlock(struct amdgpu_fpriv *fpriv);
#else
static inline int sgpu_sysfs_proc_init(struct kobject *gpu_root)
{
	return 0;
}

static inline int sgpu_proc_add_context(struct amdgpu_fpriv *fpriv)
{
	return 0;
}

#define sgpu_proc_remove_context(fpriv) do { } while (0)

static int sgpu_tt_swapin(struct ttm_tt *ttm)
{
	return 0;
}

#define sgpu_swap_init_bo(bo) do { } while (0)
#define sgpu_swap_add_bo(fpriv, bo) do { } while (0)
#define sgpu_swap_remove_bo(fpriv, bo) do { } while (0)
#define sgpu_tt_destroy_notify(ttm) do { } while (0)
#define sgpu_swap_lock(fpriv) do { } while (0)
#define sgpu_swap_unlock(fpriv) do { } while (0)
#endif

#endif /* __SGPU_SWAP_H__ */
