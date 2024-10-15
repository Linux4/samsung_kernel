// SPDX-License-Identifier: GPL-2.0-only
/*
* @file sgpu_bpmd.c
* @copyright 2020-2021 Samsung Electronics
*/

#include <linux/atomic.h>
#include <linux/crc32.h>
#include <linux/fs.h>
#include <linux/kernel.h>
#include <linux/pm_runtime.h>

#include "amdgpu.h"
#include "sgpu_bpmd.h"
#include "sgpu_bpmd_layout.h"

/**
 * sgpu_bpmd_register_bo - register a BO to be dumped
 *
 * @bo_dump_list: list in which to register the BO
 * @bo: bo to register
 */
static void sgpu_bpmd_register_bo(struct list_head *bo_dump_list, struct amdgpu_bo *bo)
{
	if (list_empty(&bo->bpmd_list)) {
		list_add(&bo->bpmd_list, bo_dump_list);
	}
}

/**
 * sgpu_bpmd_write - write a buffer to a file with alignment
 *
 * @sbo:  pointer a structure defining the output target for the BPMD
 * @buf: pointer to a buffer
 * @count: byte count to write
 * @align: alignment in byte
 *
 * Return the number of bytes written
 */
ssize_t sgpu_bpmd_write(struct sgpu_bpmd_output *sbo,  const void *buf, size_t count,
			uint32_t align)
{
	ssize_t r 	    = 0;
	ssize_t copy_sz = min((sbo->data_size) - (sbo->offset), count);
	loff_t pos 	    = ALIGN(sbo->offset, align);
	loff_t zeros_count = pos - sbo->offset;
	loff_t zeros_written = 0;

	const u8 zero = 0;

#if IS_ENABLED(CONFIG_DRM_SGPU_BPMD_MEMLOGGER_TEXT)
	if (sbo->output_type == SGPU_BPMD_MEMLOGGER)
		return count;
#endif

	if (sbo->output_type == SGPU_BPMD_NONE) {
		/* early out for a sizing pass, don't bother calculating CRC32 */
		r = count;
		sbo->offset += count + zeros_count;
		return r;
	}

	for(zeros_written = 0; zeros_written < zeros_count; zeros_written++) {
		sbo->crc32 = crc32(sbo->crc32, &zero, 1);
	}
	sbo->crc32 = crc32(sbo->crc32, buf, count);

	switch (sbo->output_type) {
		case SGPU_BPMD_USERPTR: /* Write to user space buffer */
			sbo->offset += zeros_count;
			r = copy_to_user(&sbo->user_data[sbo->offset], buf, copy_sz);
			if (r) {
				DRM_ERROR("failed to write %ld bytes to userptr\n", r);
			}
			sbo->offset += (copy_sz - r);
			r = copy_sz - r;
			break;
		case SGPU_BPMD_PTR:  /* Write to kernel space buffer */
			sbo->offset += zeros_count;
			memcpy(&sbo->kernel_data[sbo->offset], buf, copy_sz);
			sbo->offset += copy_sz;
			r = copy_sz;
			break;
#ifdef CONFIG_DRM_SGPU_BPMD_FILE_DUMP
		case SGPU_BPMD_FILE: /* Write to file */
			r = kernel_write(sbo->f, buf, count, &pos);
			if (r > 0) {
				sbo->f->f_pos = pos;
				sbo->offset = pos;
			}
			break;
#endif
		default:
			break;
	}

	if (r != count) {
		DRM_ERROR("failed to write r %ld count %ld\n", r, count);
	}

	return r;
}

/**
 * sgpu_bpmd_dump_header - dump header
 *
 * @sdev: pointer to sgpu_device
 * @sbo: pointer to output target description
 *
 */
static void sgpu_bpmd_dump_header(struct amdgpu_device *sdev, struct sgpu_bpmd_output *sbo)
{
	struct sgpu_bpmd *bpmd = &sdev->bpmd;

	DRM_DEBUG("%s: start dumping header \n", __func__);

	bpmd->funcs->dump_header(sbo);
}

static void sgpu_bpmd_dump_footer(struct amdgpu_device *sdev, struct sgpu_bpmd_output *sbo)
{
	sdev->bpmd.funcs->dump_footer(sdev, sbo);
}

static void sgpu_bpmd_dump_gart_pt(struct amdgpu_device *sdev, struct sgpu_bpmd_output *sbo)
{
	DRM_DEBUG("%s: start dumping GART PTE \n", __func__);
	sgpu_bpmd_layout_dump_pt(sbo, sdev->gmc.gart_start, sdev->gart.ptr, sdev->gart.num_cpu_pages);
}

static void sgpu_bpmd_dump_system_info(struct amdgpu_device *sdev,
				       struct sgpu_bpmd_output *sbo)
{
	const struct sgpu_bpmd_hw_revision hw_revision = {
		.minor = sdev->minor_rev,
		.major = sdev->major_rev,
		.model = sdev->model_id,
		.generation = sdev->gen_id
	};
	const struct sgpu_bpmd_firmwares_version firmwares[] = {
		{
			.id = SGPU_BPMD_FW_ID_CP_ME,
			.version = sdev->gfx.me_fw_version,
			.feature_version = sdev->gfx.me_feature_version
		},
		{
			.id = SGPU_BPMD_FW_ID_CP_MEC,
			.version = sdev->gfx.mec_fw_version,
			.feature_version = sdev->gfx.mec_feature_version
		},
		{
			.id = SGPU_BPMD_FW_ID_CP_PFP,
			.version = sdev->gfx.pfp_fw_version,
			.feature_version = sdev->gfx.pfp_feature_version
		},
		{
			.id = SGPU_BPMD_FW_ID_RLC,
			.version = sdev->gfx.rlc_fw_version,
			.feature_version = sdev->gfx.rlc_feature_version
		}
	};

	DRM_DEBUG("%s: start dumping firmware version \n", __func__);

	sdev->bpmd.funcs->dump_system_info(sdev, sbo, &hw_revision, ARRAY_SIZE(firmwares), firmwares);
}

/*
 * sgpu_bpmd_dump_regs - dump registers
 *
 * @sdev: pointer to sgpu_device
 * @sbo: pointer to output target description
 *
 */
static void sgpu_bpmd_dump_regs(struct amdgpu_device *sdev, struct sgpu_bpmd_output *sbo)
{
	struct sgpu_bpmd *bpmd = &sdev->bpmd;

	DRM_DEBUG("%s: start dumping regs \n", __func__);

	bpmd->funcs->dump_reg(sbo, sdev);
}

inline void sgpu_bpmd_remove_ib_info(struct sgpu_bpmd_ib_info *ib_info)
{
	list_del(&ib_info->list);
	kfree(ib_info);
}

static void sgpu_bpmd_find_ib_bo_va_bfs(struct list_head *ib_info_list,
					struct amdgpu_ring *ring,
					struct list_head *bo_dump_list)
{
	int32_t r = 0;
	struct sgpu_bpmd *bpmd = &ring->adev->bpmd;

	/* Find out all BOs used for IB chains */
	while (!list_empty(ib_info_list)) {
		struct amdgpu_bo_va_mapping *mapping = NULL;
		struct sgpu_bpmd_ib_info* ib_info = NULL;
		uint64_t gpu_page = 0;
		uint64_t offset_in_bo = 0;
		const uint8_t *buf = NULL;
		struct amdgpu_vm *vm = NULL;

		/* Get an ib_info */
		ib_info = list_first_entry(ib_info_list,
					   struct sgpu_bpmd_ib_info, list);

		/* Find a mapping */
		vm = sgpu_vm_get_vm(ring->adev, ib_info->vmid);
		if (vm) {
			gpu_page = ib_info->gpu_addr >> AMDGPU_GPU_PAGE_SHIFT;
			mapping = amdgpu_vm_bo_lookup_mapping(vm,
							      gpu_page);
		} else
			DRM_ERROR("vmid %d doesn't have a mapping", ib_info->vmid);

		if (mapping == NULL || mapping->bo_va == NULL ||
		    mapping->bo_va->base.bo == NULL) {
			DRM_ERROR("Failed to find bo within vm\n");
			goto pop;
		}

		/* Add mapping->bo_va if it's new */
		sgpu_bpmd_register_bo(bo_dump_list, mapping->bo_va->base.bo);

		/* Traverse other IBs (chain or next level) */
		r = amdgpu_bo_kmap(mapping->bo_va->base.bo, (void **)&buf);
		if (r != 0) {
			DRM_ERROR("failed to map bo\n");
			goto pop;
		}

		offset_in_bo = mapping->offset + ib_info->gpu_addr -
			(mapping->start << AMDGPU_GPU_PAGE_SHIFT);
		buf += offset_in_bo;
		bpmd->funcs->find_ibs((uint32_t*) buf, ib_info->size,
				      ib_info->vmid, ib_info_list);

pop:
		/* Remove ib_info once it's processed */
		sgpu_bpmd_remove_ib_info(ib_info);
	}
}


/**
 * sgpu_bpmd_find_ib_bos -
 *
 * @ring: pointer to the ring to walk for IBs
 * @bo_dump_list: list in which BOs will be accumulated for future dump
 *
 */
static void sgpu_bpmd_find_ib_bos(struct amdgpu_ring *ring,
				  struct list_head *bo_dump_list)
{
	int32_t r = 0;
	uint32_t rptr = ring->funcs->get_rptr(ring) & ring->buf_mask;
	uint32_t wptr = ring->funcs->get_wptr(ring) & ring->buf_mask;
	struct sgpu_bpmd *bpmd = &ring->adev->bpmd;
	struct list_head ib_info_list = {0};
	const uint32_t *buf = NULL;
	const uint32_t *start = NULL;
	static const uint32_t dw_byte = 4;
	uint32_t size =  0;

	DRM_DEBUG("%s: start dumping ib bos %d, %d", __func__, rptr, wptr);

	if (ring->ring_obj == NULL || rptr == wptr) {
		DRM_DEBUG("%s: nothing to dump - no pending command", __func__);
		return;
	}

	r = amdgpu_bo_kmap(ring->ring_obj, (void **)&buf);
	if (r != 0) {
		DRM_ERROR("failed to map bo\n");
		return;
	}

	INIT_LIST_HEAD(&ib_info_list);
	if (rptr < wptr) {
		start = buf + rptr;
		size = (wptr - rptr) * dw_byte;
		bpmd->funcs->find_ibs(start, size, SGPU_BPMD_INVALID_VMID,
				      &ib_info_list);
	} else {
		start = buf + rptr;
		size =  amdgpu_bo_size(ring->ring_obj) - (rptr * dw_byte);
		bpmd->funcs->find_ibs(start, size, SGPU_BPMD_INVALID_VMID,
				      &ib_info_list);
		start = buf;
		size = wptr * dw_byte;
		bpmd->funcs->find_ibs(start, size, SGPU_BPMD_INVALID_VMID,
				      &ib_info_list);
	}

	sgpu_bpmd_find_ib_bo_va_bfs(&ib_info_list, ring, bo_dump_list);
}

static void sgpu_bpmd_search_bos(struct amdgpu_device *sdev,
				 struct list_head *bo_dump_list)
{
	unsigned int i = 0;
	for (i = 0; i < sdev->gfx.num_gfx_rings; i++) {
		struct amdgpu_ring *ring = &sdev->gfx.gfx_ring[i];
		DRM_INFO("%s: dump gfx ring %d\n", __func__, i);
		sgpu_bpmd_register_bo(bo_dump_list, ring->ring_obj);
		sgpu_bpmd_register_bo(bo_dump_list, ring->mqd_obj);
		sgpu_bpmd_find_ib_bos(ring, bo_dump_list);
	}
}

/**
 * sgpu_bpmd_dump_ring - dump rings
 *
 * @sdev: pointer to sgpu_device
 * @sbo: pointer to output target description
 *
 */
static void sgpu_bpmd_dump_rings(struct amdgpu_device *sdev,
				 struct sgpu_bpmd_output *sbo)
{
	struct sgpu_bpmd *bpmd = &sdev->bpmd;
	uint32_t i = 0;

	DRM_DEBUG("%s: start dumping rings \n", __func__);

	for (i = 0; i < sdev->gfx.num_gfx_rings; i++) {
		struct amdgpu_ring *ring = &sdev->gfx.gfx_ring[i];
		DRM_INFO("%s: dump gfx ring %d\n", __func__, i);
		bpmd->funcs->dump_ring(sbo, ring);
	}

	// TODO: enable this when compute ring is ready on qemu
	/*
	for (i = 0; i < sdev->gfx.num_compute_rings; i++)
		DRM_INFO("%s: dump compute ring %d\n", __func__, i);
		bpmd->funcs->dump_ring(sbo, &sdev->gfx.compute_ring[i]);
	*/

	// TODO: Dump more rings
}

/**
 * sgpu_bpmd_dump_vms - The VM dump should happen after every BOs are dumped
 * This is because the VA will map to a BO's packet ID, which can
 * only be know after it is dumped.
 *
 * @sdev: pointer to sgpu_device
 * @sbo:  pointer a structure defining the output target for the BPMD
 *
 */
static void sgpu_bpmd_dump_vms(struct amdgpu_device *sdev, struct sgpu_bpmd_output *sbo)
{
	int i = 0;
	struct amdgpu_vm *vm = NULL;

	for(i = 0; i < AMDGPU_NUM_VMID; ++i) {
		vm = sgpu_vm_get_vm(sdev, i);
		if (vm) {
			sgpu_bpmd_dump_vm(sbo, vm, i);
		}
	}
}

/**
 * sgpu_bpmd_is_valid - check bpmd setting
 *
 * @sdev: pointer to sgpu_device
 *
 * Return true if bpmd is valid, flase otherwise
 */
static bool sgpu_bpmd_is_valid(struct amdgpu_device *sdev)
{
	if ((sdev->bpmd.funcs == NULL) ||
	    (sdev->bpmd.funcs->dump_header == NULL) ||
	    (sdev->bpmd.funcs->dump_reg == NULL) ||
	    (sdev->bpmd.funcs->dump_ring == NULL) ||
	    (sdev->bpmd.funcs->dump_system_info == NULL) ||
	    (sdev->bpmd.funcs->dump_bo == NULL)) {
		DRM_ERROR("bpmd function is not fully set");
		return false;
	}

	return true;
}

/**
 *  Dump BOs in list
 */
static void sgpu_bpmd_dump_bo_list(struct amdgpu_device *sdev,
				   struct sgpu_bpmd_output *sbo,
				   struct list_head *bo_list)
{
	struct sgpu_bpmd *bpmd = &sdev->bpmd;
	struct amdgpu_bo *bo;

	DRM_INFO("Dumping bo list");
	list_for_each_entry(bo, bo_list, bpmd_list) {
		bpmd->funcs->dump_bo(sbo, bo);
	}
}

static void sgpu_bpmd_clean_bo_list(struct list_head *bo_list)
{
	struct list_head *pos = NULL;
	struct list_head *n   = NULL;

	DRM_INFO("Cleaning bo list");
	list_for_each_safe(pos, n, bo_list) {
		struct amdgpu_bo *bo = list_entry(pos,
						  struct amdgpu_bo,
						  bpmd_list);

		bo->bpmd_packet_id = SGPU_BPMD_LAYOUT_INVALID_PACKET_ID;
		list_del_init(pos);
	}
}

/**
 * sgpu_bpmd_dump_core - create and dump a bpmd.
 * Generic core function, invoked by wrappers dependent on where the BPMD
 * is to be written.
 *
 * @sdev: pointer to sgpu_device
 * @sbo:  pointer a structure defining the output target for the BPMD
 */
static void sgpu_bpmd_dump_core(struct amdgpu_device *sdev, struct sgpu_bpmd_output *sbo)
{
	struct list_head bo_dump_list;
	/* @todo GFXSW-6022 - Remove the mutex */
	static DEFINE_MUTEX(dump_lock);
	INIT_LIST_HEAD(&bo_dump_list);

	if (sgpu_bpmd_is_valid(sdev) == false) {
		DRM_WARN("%s: skip bpmd", __func__);
		return;
	}

	mutex_lock(&dump_lock);
	pm_runtime_get_sync(sdev->dev);
	sgpu_ifpo_lock(sdev);

	sgpu_bpmd_dump_header(sdev, sbo);
	/* system_info is to follow immediately the header per file specs */
	if (sbo->section_filter & SGPU_BPMD_SECTION_SYS)
		sgpu_bpmd_dump_system_info(sdev, sbo);
	if (sbo->section_filter & SGPU_BPMD_SECTION_REG)
		sgpu_bpmd_dump_regs(sdev, sbo);

	if (SGPU_BPMD_ENABLE_UNVERIFIED) {
		if (sbo->section_filter & SGPU_BPMD_SECTION_BO) {
			sgpu_bpmd_search_bos(sdev, &bo_dump_list);
			/* BOs needs to be dumped beore the VM as the each mappings in
				the VM dump will refer to a BO's packet ID */
			sgpu_bpmd_dump_bo_list(sdev, sbo, &bo_dump_list);
		}
		/* mqd and ring object must be registered before the rings are dumped */
		if (sbo->section_filter & SGPU_BPMD_SECTION_RING)
			sgpu_bpmd_dump_rings(sdev, sbo);
		if (sbo->section_filter & SGPU_BPMD_SECTION_VM)
			sgpu_bpmd_dump_vms(sdev, sbo);
		if (sbo->section_filter & SGPU_BPMD_SECTION_GART_PT)
			sgpu_bpmd_dump_gart_pt(sdev, sbo);
	}

	sgpu_bpmd_dump_footer(sdev, sbo);

	if (SGPU_BPMD_ENABLE_UNVERIFIED) {
		sgpu_bpmd_clean_bo_list(&bo_dump_list);
	}

	sgpu_ifpo_unlock(sdev);
	pm_runtime_put(sdev->dev);
	mutex_unlock(&dump_lock);
}

/**
 * @brief Run a sizing pass at BPMD dump - don't actually write anything.
 * All output is generated, but discarded to allow the required size of
 * output buffer to be determined for use with sgpu_bpmd_dump_to_userptr()
 * or sgpu_bpmd_dump_to_kmem().
 *
 * @sdev: pointer to sgpu_device
 * @section_filter: Bitmask of sections to output
 */
uint32_t sgpu_bpmd_dump_size(struct amdgpu_device *sdev, uint32_t section_filter)
{
	struct sgpu_bpmd_output sbo = {
		.output_type = SGPU_BPMD_NONE,
		.data_size = U32_MAX,
		.offset = 0,
		.section_filter = section_filter,
		.crc32 = 0xffffffff,
		.kernel_data = NULL
	};

	if (sgpu_bpmd_is_valid(sdev) == false)
		DRM_WARN("%s: skip bpmd", __func__);
	else
		sgpu_bpmd_dump_core(sdev, &sbo);

	return sbo.offset;
}

/**
 * @brief Dump BPMD to a userspace buffer
 *
 * @sdev: pointer to sgpu_device
 * @data: user pointer to a buffer to write BPMD to
 * @data_size: size in bytes of the buffer pointed to by data
 * @section_filter: Bitmask of sections to output
 */
uint32_t sgpu_bpmd_dump_to_userptr(struct amdgpu_device *sdev, uint8_t __user *data,
				uint32_t data_size, uint32_t section_filter)
{
	struct sgpu_bpmd_output sbo = {
		.output_type = SGPU_BPMD_USERPTR,
		.data_size = data_size,
		.offset = 0,
		.section_filter = section_filter,
		.crc32 = 0xffffffff,
		.user_data = data
	};

	if (clear_user(sbo.user_data, sbo.data_size)) {
		DRM_ERROR("failed to clear userptr\n");
	}

	sgpu_bpmd_dump_core(sdev, &sbo);
	return sbo.offset;
}

/**
 * @brief Dump BPMD to a kernel space buffer
 *
 * @sdev: pointer to sgpu_device
 * @data: kernel pointer to a buffer to write BPMD to
 * @data_size: size in bytes of the buffer pointed to by data
 * @section_filter: Bitmask of sections to output
 */
uint32_t sgpu_bpmd_dump_to_kmem(struct amdgpu_device *sdev, uint8_t *data,
				uint32_t data_size, uint32_t section_filter)
{
	struct sgpu_bpmd_output sbo = {
		.output_type = SGPU_BPMD_PTR,
		.data_size = data_size,
		.offset = 0,
		.section_filter = section_filter,
		.crc32 = 0xffffffff,
		.kernel_data = data
	};

	memset(sbo.kernel_data, 0U, sbo.data_size);

	sgpu_bpmd_dump_core(sdev, &sbo);
	return sbo.offset;
}

/**
 * sgpu_bpmd_dump_to_memlogger - create and dump a bpmd into memlogger
 *
 * @sdev: pointer to sgpu_device
 * @section_filter: Bitmask of sections to output
 */
uint32_t sgpu_bpmd_dump_to_memlogger(struct amdgpu_device *sdev, uint32_t section_filter)
{
	struct sgpu_bpmd_output sbo = {
		.output_type = SGPU_BPMD_MEMLOGGER,
		.data_size = 0,
		.offset = 0,
		.section_filter = section_filter,
		.crc32 = 0xffffffff,
	};

	sgpu_bpmd_dump_core(sdev, &sbo);
	return sbo.offset;
}

/**
 * sgpu_bpmd_dump_to_file - create and dump a bpmd file
 * This does NOT close the file after writing to it.
 *
 * @note Deprecated, do not use (not allowed in GKI kernel)
 *
 * @sdev: pointer to sgpu_device
 * @f: file to write to
 * @section_filter: Bitmask of sections to output
 */
uint32_t sgpu_bpmd_dump_to_file(struct amdgpu_device *sdev, struct file *f, uint32_t section_filter)
{
	struct sgpu_bpmd_output sbo = {
		.output_type = SGPU_BPMD_FILE,
		.data_size = 0,
		.offset = 0,
		.section_filter = section_filter,
		.crc32 = 0xffffffff,
		.f = f
	};

	sgpu_bpmd_dump_core(sdev, &sbo);
	return sbo.offset;
}

#ifdef CONFIG_DRM_SGPU_BPMD_FILE_DUMP
/**
 * sgpu_bpmd_file_open - open bpmd file and return file pointer
 *
 * @filename: path to open
 *
 */
static struct file *sgpu_bpmd_file_open(const char *filename)
{
	struct file *f = NULL;

	f = filp_open(filename, O_WRONLY | O_LARGEFILE | O_CREAT,
		      S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH);
	if (IS_ERR(f)) {
		DRM_ERROR("failed to open a file (%s) - %ld\n",
			  filename, PTR_ERR(f));
		return NULL;
	}

	return f;
}

/**
 * sgpu_bpmd_get_filename - set a bpmd file name including a path
 *
 * @buf: pointer to a buffer
 * @size: size of buf
 *
 * Return the number of characters (<=size),
 * otherwise a negative number is returned if an encoding error occurs
 */
static int sgpu_bpmd_get_filename(char *buf, size_t size)
{
	int r = 0;
	static const char *name = "bpmd";
	static const char *ext  = "bin";
	static atomic_t seq = ATOMIC_INIT(0);

	r = snprintf(buf, size, "%s/%s.%d.%s", sgpu_bpmd_path, name,
		     (uint32_t) atomic_inc_return(&seq), ext);
	if (r <= 0) {
		DRM_ERROR("failed to set a filename %d\n", r);
	}

	return r;
}
#endif /* CONFIG_DRM_SGPU_BPMD_FILE_DUMP */

/**
 * sgpu_bpmd_dump - Dump the bpmd data to memlogger
 * or create and dump a bpmd file.
 * This is a backwards compatibility wrapper around
 * sgpu_bpmd_dump_to_file().
 *
 * @sdev: pointer to sgpu_device
 */
void sgpu_bpmd_dump(struct amdgpu_device *sdev)
{
#ifdef CONFIG_DRM_SGPU_BPMD_FILE_DUMP
	struct file *f = NULL;
	char filename[256] = {};
	int r = 0;
#endif /* CONFIG_DRM_SGPU_BPMD_FILE_DUMP */
	uint32_t section_filter = SGPU_BPMD_SECTION_ALL;

	if (sgpu_bpmd_is_valid(sdev) == false) {
		DRM_WARN("%s: skip bpmd", __func__);
		return;
	}

	if (IS_ENABLED(CONFIG_DRM_SGPU_BPMD_MEMLOGGER_TEXT)) {
		if (!sdev->bpmd.memlog_text) {
			DRM_ERROR("BPMD Memlogger memory is not allocated, aborting");
			return;
		}

		sgpu_bpmd_dump_to_memlogger(sdev, section_filter);
	} else {
#ifdef CONFIG_DRM_SGPU_BPMD_FILE_DUMP
		r = sgpu_bpmd_get_filename(filename, sizeof(filename));
		if(r <= 0)
			return;

		f = sgpu_bpmd_file_open(filename);
		if (f == NULL) {
			DRM_ERROR("BPMD file cannot be opened, aborting");
			return;
		}

		DRM_INFO("%s: start dumping to file (%s)\n", __func__, filename);

		sgpu_bpmd_dump_to_file(sdev, f, section_filter);

		filp_close(f, NULL);

		DRM_INFO("%s: end dumping\n", __func__);
#endif /* CONFIG_DRM_SGPU_BPMD_FILE_DUMP */
	}
}

/**
 * sgpu_bpmd_get_packet_id - get a packet id
 *
 * Return a packet id
 */
uint32_t sgpu_bpmd_get_packet_id(void)
{
	static atomic_t id = ATOMIC_INIT(0);
	uint32_t ret = (uint32_t) atomic_inc_return(&id);
	return ret;
}
