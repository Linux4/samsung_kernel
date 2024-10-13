/* SPDX-License-Identifier: GPL-2.0-only */
/*
* @file sgpu_bpmd.h
* @copyright 2020 Samsung Electronics
*/

#ifndef _SGPU_BPMD_H_
#define _SGPU_BPMD_H_

#include <drm/drm_print.h>
#include <linux/limits.h>
#include <linux/types.h>

#include <sgpu_bpmd_layout_common.h>

/* forward declaration */
struct amdgpu_bo;
struct amdgpu_bo_va;
struct amdgpu_device;
struct amdgpu_ring;
struct file;

#define SGPU_BPMD_INVALID_VMID UINT_MAX
#define SGPU_BPMD_ENABLE_UNVERIFIED false


/* RFC: Should this use amdgpu_ib */
struct sgpu_bpmd_ib_info {
	uint64_t         gpu_addr;
	uint64_t         size;
	uint32_t         vmid;
	struct list_head list;
};

enum sgpu_bpmd_output_type {
	/** Generate no output, BPMD sizing pass only */
	SGPU_BPMD_NONE      = 0,
	/** Output to kernel space buffer */
	SGPU_BPMD_PTR       = 1,
	/** Output to user space buffer */
	SGPU_BPMD_USERPTR   = 2,
	/** Output to memlogger */
	SGPU_BPMD_MEMLOGGER = 3,
	/** Output to a file : only permissable for non-GKI builds */
	SGPU_BPMD_FILE      = 4,
	SGPU_BPMD_TYPE_MAX
};

enum sgpu_bpmd_sections {
	section_reg = 0,
	section_ring,
	section_bo,
	section_ibs,
	section_sys,
	section_vm,
	section_gart_pt,
	section_all
};

#define SGPU_BPMD_SECTION_REG     BIT(section_reg)
#define SGPU_BPMD_SECTION_RING    BIT(section_ring)
#define SGPU_BPMD_SECTION_BO      BIT(section_bo)
#define SGPU_BPMD_SECTION_IBS     BIT(section_ibs)
#define SGPU_BPMD_SECTION_SYS     BIT(section_sys)
#define SGPU_BPMD_SECTION_VM      BIT(section_vm)
#define SGPU_BPMD_SECTION_GART_PT BIT(section_gart_pt)
#define SGPU_BPMD_SECTION_ALL     GENMASK(section_all, 0)

/** Defines the output target for a BPMD */
struct sgpu_bpmd_output {
	enum sgpu_bpmd_output_type output_type;
	size_t                     data_size;
	size_t                     offset;
	uint32_t                   section_filter;
	uint32_t                   crc32;
	union {
		struct file *          f;
		uint8_t __user *       user_data;
		uint8_t *              kernel_data;
	};
};

struct sgpu_bpmd_funcs {
	void (*dump_header)(struct sgpu_bpmd_output *sbo);
	int (*dump_reg)(struct sgpu_bpmd_output *sbo, struct amdgpu_device *sdev);
	uint32_t (*dump_ring)(struct sgpu_bpmd_output *sbo, struct amdgpu_ring *ring);
	uint32_t (*dump_bo)(struct sgpu_bpmd_output *sbo, struct amdgpu_bo *bo);
	void (*find_ibs)(const uint32_t *addr, uint32_t size, uint32_t vmid,
			 struct list_head *list);
	uint32_t (*dump_system_info)(struct amdgpu_device *adev, struct sgpu_bpmd_output *sbo,
				     const struct sgpu_bpmd_hw_revision *hw_revison,
				     size_t firmware_count,
				     const struct sgpu_bpmd_firmwares_version *firmwares);
	uint32_t (*dump_footer)(struct amdgpu_device *adev, struct sgpu_bpmd_output *sbo);
};

struct sgpu_bpmd {
	const struct sgpu_bpmd_funcs *funcs;
# if IS_ENABLED(CONFIG_DRM_SGPU_BPMD_MEMLOGGER_TEXT)
	struct memlog_obj *memlog_text;
#endif
};

static inline bool sgpu_bpmd_add_ib_info(struct list_head* list,
					 uint64_t gpu_addr, uint64_t size,
					 uint32_t vmid)
{
	struct sgpu_bpmd_ib_info *ib_info =
		kmalloc(sizeof(struct sgpu_bpmd_ib_info), GFP_KERNEL);
	if (ib_info == NULL) {
		DRM_ERROR("Out of memory");
		return false;
	}
	ib_info->gpu_addr = gpu_addr;
	ib_info->size     = size;
	ib_info->vmid     = vmid;

	list_add(&ib_info->list, list);
	return true;
}

uint32_t sgpu_bpmd_dump_size(struct amdgpu_device *sdev, uint32_t section_filter);
uint32_t sgpu_bpmd_dump_to_userptr(struct amdgpu_device *sdev, uint8_t __user *data, uint32_t data_size, uint32_t section_filter);
uint32_t sgpu_bpmd_dump_to_kmem(struct amdgpu_device *sdev, uint8_t *data, uint32_t data_size, uint32_t section_filter);
uint32_t sgpu_bpmd_dump_to_memlogger(struct amdgpu_device *sdev, uint32_t section_filter);
uint32_t sgpu_bpmd_dump_to_file(struct amdgpu_device *sdev, struct file *f, uint32_t section_filter);

#ifdef CONFIG_DRM_SGPU_BPMD
void sgpu_bpmd_dump(struct amdgpu_device *sdev);
#else
static inline void sgpu_bpmd_dump(struct amdgpu_device *sdev) { do { } while (0); }
#endif /* CONFIG_DRM_SGPU_BPMD */

ssize_t sgpu_bpmd_write(struct sgpu_bpmd_output *sbo,  const void *buf, size_t count,
			uint32_t align);
uint32_t sgpu_bpmd_get_packet_id(void);

#endif  /* _SGPU_BPMD_H_ */
