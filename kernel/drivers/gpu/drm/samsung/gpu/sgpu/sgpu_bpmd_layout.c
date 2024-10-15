// SPDX-License-Identifier: GPL-2.0-only
/*
* @file sgpu_bpmd_layout.c
* @copyright 2020-2021 Samsung Electronics
*/

#include "amdgpu.h"
#include "amdgpu_vm.h"
#include "sgpu_bpmd.h"
#include "sgpu_bpmd_layout.h"
#include "sgpu_bpmd_log.h"

/**
 * sgpu_bpmd_layout_dump_header_packet - dump a bpmd header
 *
 * @sbo: pointer to output target description
 *
 */
void sgpu_bpmd_layout_dump_header(struct sgpu_bpmd_output *sbo)
{
	sgpu_bpmd_layout_dump_header_version(sbo,
		SGPU_BPMD_LAYOUT_MAJOR_VERSION,
		SGPU_BPMD_LAYOUT_MINOR_VERSION);
}

/**
 * sgpu_bpmd_layout_dump_registers - dumps common registers from
 * sgpu_bpmd_layout_dump_reg32_packet/sgpu_bpmd_layout_dump_indexed_reg32_packet/
 * sgpu_bpmd_layout_dump_reg32_multiple_read_packet.
 *
 * @sbo: pointer to output target description
 * @packet: packet
 * @packet_size: size of the packet
 * @reg32: pointer to array of registers
 * @total_reg_size: total size of registers
 *
 * Return error
 */
static inline uint32_t sgpu_bpmd_layout_dump_registers(
	struct sgpu_bpmd_output *sbo,
	const void *packet,
	size_t packet_size,
	const void *reg32,
	size_t total_reg_size)
{
	ssize_t r = 0;
	r = sgpu_bpmd_write(sbo, packet, packet_size,
			    SGPU_BPMD_LAYOUT_PACKET_ALIGN_BYTE);
	if (r != packet_size) {
		DRM_ERROR("failed to dump reg32 packet\n");
		return SGPU_BPMD_LAYOUT_INVALID_PACKET_ID;
	}

	r = sgpu_bpmd_write(sbo, reg32, total_reg_size,
			    SGPU_BPMD_LAYOUT_PACKET_ALIGN_BYTE);
	if (r != total_reg_size) {
		DRM_ERROR("failed to dump reg32 packet data\n");
		return SGPU_BPMD_LAYOUT_INVALID_PACKET_ID;
	}

	return 0;
}

/**
 * sgpu_bpmd_layout_dump_reg32_packet - dump reg32 packet
 *
 * @sbo: pointer to output target description
 * @domain: bpmd_layout_reg_domain
 * @num_regs: number of registers
 * @reg32: pointer to bpmd_layout_reg32 array
 * @grbm_gfx_index: value written to GRBM_GFX_INDEX when register is read
 *
 * Return a packet ID
 */
uint32_t sgpu_bpmd_layout_dump_reg32_packet(
	struct amdgpu_device *adev,
	struct sgpu_bpmd_output *sbo,
	enum sgpu_bpmd_layout_reg_domain domain,
	uint32_t num_regs,
	struct sgpu_bpmd_layout_reg32 *reg32,
	uint32_t grbm_gfx_index)
{
	ssize_t r = 0;
	size_t total_reg_size = num_regs * sizeof(reg32[0]);
	const size_t total_size
		= sizeof(struct sgpu_bpmd_layout_reg32_packet)
		+ num_regs * sizeof(struct sgpu_bpmd_layout_reg32);
	struct sgpu_bpmd_layout_reg32_packet packet = {
		.header = {
			.kind   = SGPU_BPMD_LAYOUT_PACKET_REG,
			.id     = sgpu_bpmd_get_packet_id(),
			.length = total_size - sizeof(struct sgpu_bpmd_layout_packet_header)
		},
		.domain         = domain,
		.num_regs       = num_regs,
		.grbm_gfx_index = grbm_gfx_index
	};

	DRM_DEBUG("%s: start dumping a reg32 packet\n", __func__);

	BUG_ON((num_regs == 0) || (reg32 == NULL));

	DRM_DEBUG("%s: dump reg32 packet(id %u), num %d \n",
		 __func__, packet.header.id, num_regs);

	if (sbo->output_type == SGPU_BPMD_MEMLOGGER) {
		sgpu_bpmd_log_packet_reg32(adev, &packet);
		sgpu_bpmd_log_registers(adev, reg32, num_regs);
	}

	r = sgpu_bpmd_layout_dump_registers(sbo, (const void *) &packet,
					sizeof(packet), (const void *)reg32, total_reg_size);
	return r ? r : packet.header.id;
}

/**
 * sgpu_bpmd_layout_dump_indexed_reg32_packet - dump index reg32 packet
 *
 * Packet for indexed register, which contains index field to save the index
 * of registers that are indexed by first writing to another register. The
 * value of this field is the offset of the register that was written to.
 *
 * @sbo: pointer to output target description
 * @domain: bpmd_layout_reg_domain
 * @num_regs: number of registers
 * @reg32: pointer to bpmd_layout_reg32 array
 * @grbm_gfx_index: value written to GRBM_GFX_INDEX when register is read
 * @index_value: value written to the indexing register
 *
 * Return a packet ID
 */
uint32_t sgpu_bpmd_layout_dump_indexed_reg32_packet(
	struct amdgpu_device *adev,
	struct sgpu_bpmd_output *sbo,
	enum sgpu_bpmd_layout_reg_domain domain,
	uint32_t num_regs,
	struct sgpu_bpmd_layout_reg32 *reg32,
	uint32_t grbm_gfx_index,
	uint32_t index_value)
{
	ssize_t r = 0;
	size_t total_reg_size = num_regs * sizeof(reg32[0]);
	const size_t total_size
		= sizeof(struct sgpu_bpmd_layout_indexed_reg32_packet)
		+ num_regs * sizeof(struct sgpu_bpmd_layout_reg32);
	struct sgpu_bpmd_layout_indexed_reg32_packet packet = {
		.header = {
			.kind   = SGPU_BPMD_LAYOUT_PACKET_INDEXED_REG,
			.id     = sgpu_bpmd_get_packet_id(),
			.length = total_size - sizeof(struct sgpu_bpmd_layout_packet_header)
		},
		.domain         = domain,
		.num_regs       = num_regs,
		.grbm_gfx_index = grbm_gfx_index,
		.index_register = index_value
	};

	DRM_DEBUG("%s: start dumping a indexed reg32 packet\n", __func__);

	BUG_ON((num_regs == 0) || (reg32 == NULL));

	DRM_DEBUG("%s: dump indexed reg32 packet(id %u), num %d \n",
		 __func__, packet.header.id, num_regs);

	if (sbo->output_type == SGPU_BPMD_MEMLOGGER) {
		sgpu_bpmd_log_packet_indexed_reg32(adev, &packet);
		sgpu_bpmd_log_registers(adev, reg32, num_regs);
	}

	r = sgpu_bpmd_layout_dump_registers(sbo, (const void *) &packet,
					sizeof(packet), (const void *)reg32, total_reg_size);
	return r ? r : packet.header.id;
}

/**
 * sgpu_bpmd_layout_dump_reg32_multiple_read_packet - dump reg32_multiple_read packet
 *
 * This function dumps packet is used by registers which are meant to be read
 * multiple times as they contain more than 32 bits of data.
 *
 * @sbo: pointer to output target description
 * @domain: bpmd_layout_reg_domain
 * @index_register: the offset of the register used to select the index
 * @data_register: the offset of the register used to read data from
 * @read_count: number of times the data register was read and recorded
 * @values: value of the register read in the index range [0..read_count]
 *
 * Return a packet ID
 */
uint32_t sgpu_bpmd_layout_dump_reg32_multiple_read_packet(
	struct amdgpu_device *adev,
	struct sgpu_bpmd_output *sbo,
	enum sgpu_bpmd_layout_reg_domain domain,
	uint32_t index_register,
	uint32_t data_register,
	uint32_t read_count,
	uint32_t *values,
	uint32_t grbm_gfx_index)
{
	ssize_t r = 0;
	size_t total_reg_size = read_count * sizeof(values[0]);
	const size_t total_size
		= sizeof(struct sgpu_bpmd_layout_reg32_multiple_read_packet)
		+ read_count * sizeof(uint32_t);
	struct sgpu_bpmd_layout_reg32_multiple_read_packet packet = {
		.header = {
			.kind   = SGPU_BPMD_LAYOUT_PACKET_MULTI_READ_REG,
			.id     = sgpu_bpmd_get_packet_id(),
			.length = total_size - sizeof(struct sgpu_bpmd_layout_packet_header)
		},
		.domain         = domain,
		.index_register = index_register,
		.data_register  = data_register,
		.read_count     = read_count,
		.grbm_gfx_index = grbm_gfx_index,
	};

	DRM_DEBUG("%s: start dumping multiple read packet\n", __func__);

	BUG_ON((read_count == 0) || (values == NULL));

	DRM_DEBUG("%s: dump multiple read packet(id %u), num %d \n",
		 __func__, packet.header.id, read_count);

	if (sbo->output_type == SGPU_BPMD_MEMLOGGER)
		sgpu_bpmd_log_packet_multiple_read_reg32(adev, &packet);

	r = sgpu_bpmd_layout_dump_registers(sbo, (const void *) &packet,
					sizeof(packet), (const void *)values, total_reg_size);
	return r ? r : packet.header.id;
}

/**
 * sgpu_bpmd_layout_dump_bo_packet - dump a bo packet
 *
 * @note: This function shouldn't be called immediately when discovering a BO.
 * Instead, call sgpu_bpmd_register_bo() to add it to a list that will be dumped
 * later on using sgpu_bpmd_dump_bo_list(). Once references to the packet_id
 * have been consumed, the list can be safely cleaned with
 * sgpu_bpmd_clean_bo_list().
 *
 * @sbo: pointer to output target description
 * @bo: pointer to a amgpu_bo to dump
 *
 * Return a packet ID
 */
uint32_t sgpu_bpmd_layout_dump_bo_packet(struct sgpu_bpmd_output *sbo, struct amdgpu_bo *bo)
{
	ssize_t r = 0;
	size_t size = 0;
	const void *buf = amdgpu_bo_kptr(bo);
	const size_t bo_size = amdgpu_bo_size(bo);
	const size_t total_size
		= sizeof(struct sgpu_bpmd_layout_bo_packet)
		+ bo_size;
	struct sgpu_bpmd_layout_bo_packet packet = {
		.header = {
			.kind   = SGPU_BPMD_LAYOUT_PACKET_BO,
			.id     = sgpu_bpmd_get_packet_id(),
			.length = total_size - sizeof(struct sgpu_bpmd_layout_packet_header)
		},
		.domain      = SGPU_BPMD_LAYOUT_MEMORY_DOMAIN_GTT,
		.metadata_flags = bo->metadata_flags,
		.data_size   = bo_size
	};

	if(bo->bpmd_packet_id != SGPU_BPMD_LAYOUT_INVALID_PACKET_ID)
		DRM_WARN("BO was already dumped as packet %d", bo->bpmd_packet_id);

	DRM_DEBUG("%s: start dumping a bo packet", __func__);

	BUG_ON(buf == NULL);

	DRM_DEBUG("%s: dump bo packet(id %u), size %d \n",
		 __func__, packet.header.id, packet.data_size);

	size = sizeof(packet);
	r = sgpu_bpmd_write(sbo, (const void *) &packet, sizeof(packet),
			    SGPU_BPMD_LAYOUT_PACKET_ALIGN_BYTE);
	if (r != size) {
		DRM_ERROR("failed to dump bo packet\n");
		return SGPU_BPMD_LAYOUT_INVALID_PACKET_ID;
	}

	size = packet.data_size;
	r = sgpu_bpmd_write(sbo, buf, size, SGPU_BPMD_LAYOUT_PACKET_ALIGN_BYTE);
	if (r != size) {
		DRM_ERROR("failed to dump bo packet data\n");
		return SGPU_BPMD_LAYOUT_INVALID_PACKET_ID;
	}

	bo->bpmd_packet_id = packet.header.id;

	return packet.header.id;
}

/**
 * sgpu_bpmd_layout_dump_ring_packet - dump a ring packet
 *
 * @sbo: pointer to output target description
 * @ring: pointer to a amgpu_ring
 * @reg32_packet_id: packet id for a reg32
 *
 * Return a packet ID
 */
uint32_t sgpu_bpmd_layout_dump_ring_packet(struct sgpu_bpmd_output *sbo,
					   struct amdgpu_ring *ring,
					   uint32_t reg32_packet_id)
{
	ssize_t r = 0;
	size_t size = 0;
	const size_t total_size = sizeof(struct sgpu_bpmd_layout_ring_packet);
	struct sgpu_bpmd_layout_ring_packet packet = {
		. header = {
			.kind   = SGPU_BPMD_LAYOUT_PACKET_RING,
			.id     = sgpu_bpmd_get_packet_id(),
			.length = total_size - sizeof(struct sgpu_bpmd_layout_packet_header)
		}
	};

	DRM_INFO("%s: start dumping a ring packet", __func__);

	packet.type = sgpu_bpmd_layout_get_ring_type(ring->funcs->type);
	packet.me                = ring->me;
	packet.pipe              = ring->pipe;
	packet.queue             = ring->queue;
	packet.rptr              = ring->funcs->get_rptr(ring);
	packet.wptr              = ring->funcs->get_wptr(ring);
	packet.buf_mask          = ring->buf_mask;
	packet.align_mask        = ring->funcs->align_mask;
	packet.sync_seq          = ring->fence_drv.sync_seq;
	packet.last_seq          = atomic_read(&ring->fence_drv.last_seq);
	packet.num_fences_mask   = ring->fence_drv.num_fences_mask;
	packet.hw_submission_limit = ring->sched.hw_submission_limit;
	packet.hw_rq_count       = atomic_read(&ring->sched.hw_rq_count);
	packet.job_id_count      = atomic64_read(&ring->sched.job_id_count);
	packet.num_jobs          = 0;
	packet.ring_bo_packet_id = ring->ring_obj
		? ring->ring_obj->bpmd_packet_id
		: SGPU_BPMD_LAYOUT_INVALID_PACKET_ID;
	packet.mqd_bo_packet_id  = ring->ring_obj
		? ring->mqd_obj->bpmd_packet_id
		: SGPU_BPMD_LAYOUT_INVALID_PACKET_ID;
	packet.reg32_packet_id   = reg32_packet_id;

	DRM_DEBUG("%s: dump ring packet(id %u), me %d, pipe %d, queue %d\n",
		  __func__, packet.header.id, ring->me, ring->pipe, ring->queue);

	size = sizeof(packet);
	r = sgpu_bpmd_write(sbo, (const void *) &packet, size,
			    SGPU_BPMD_LAYOUT_PACKET_ALIGN_BYTE);
	if (r != size) {
		DRM_ERROR("failed to dump ring packet");
		return SGPU_BPMD_LAYOUT_INVALID_PACKET_ID;
	}

	return packet.header.id;
}

uint32_t sgpu_bpmd_layout_dump_system_info_packet(
	struct amdgpu_device *adev,
	struct sgpu_bpmd_output *sbo,
	const struct sgpu_bpmd_hw_revision *hw_revision,
	size_t firmware_count,
	const struct sgpu_bpmd_firmwares_version *firmwares)
{
	int i = 0;
	int r = 0;
	const size_t total_size
		= sizeof(struct sgpu_bpmd_layout_firmwares_version_packet)
		+ firmware_count * sizeof(struct sgpu_bpmd_firmwares_version);
	struct sgpu_bpmd_layout_firmwares_version_packet packet = {
		. header = {
			.kind   = SGPU_BPMD_LAYOUT_PACKET_FW_VERSION,
			.id     = sgpu_bpmd_get_packet_id(),
			.length = total_size - sizeof(struct sgpu_bpmd_layout_packet_header)
		},
		.page_shift = AMDGPU_GPU_PAGE_SHIFT,
		.hw_revision = *hw_revision,
		.num_firmwares = firmware_count
	};

	if (sbo->output_type == SGPU_BPMD_MEMLOGGER)
		sgpu_bpmd_log_system_info(adev, hw_revision);

	r = sgpu_bpmd_write(sbo, (const void *) &packet, sizeof(packet),
			    SGPU_BPMD_LAYOUT_PACKET_ALIGN_BYTE);

	for (i = 0; i < firmware_count; i++)
		r += sgpu_bpmd_write(sbo,
				     (const void *) &firmwares[i],
				     sizeof(struct sgpu_bpmd_firmwares_version),
				     SGPU_BPMD_LAYOUT_PACKET_ALIGN_BYTE);

	if (r != total_size) {
		DRM_ERROR("failed to dump firmware version packet");
		return SGPU_BPMD_LAYOUT_INVALID_PACKET_ID;
	} else {
		return packet.header.id;
	}
}

/**
 * sgpu_bpmd_layout_dump_pt - dump page table entries
 *
 * @sbo: pointer to output target description
 * @va_map_start: virtual base address
 * @pt: Page table address
 * @num_pages: Number of pages that are stored (all entries including empty ones)
 *
 * Return a packet ID
 */
uint32_t sgpu_bpmd_layout_dump_pt(struct sgpu_bpmd_output *sbo,
				uint64_t va_map_start,
				void *pt,
				unsigned num_pages)
{
	int32_t r = 0;
	const size_t total_size = sizeof(struct sgpu_bpmd_layout_pt_packet)
                               + num_pages * sizeof(uint64_t);
	struct sgpu_bpmd_layout_pt_packet packet = {
		. header = {
			.kind   = SGPU_BPMD_LAYOUT_PACKET_PT,
			.id     = sgpu_bpmd_get_packet_id(),
			.length = total_size - sizeof(struct sgpu_bpmd_layout_packet_header)
		},
		.num_pt_entries = num_pages,
		.base_va        = va_map_start
	};

	if (!pt)
		return SGPU_BPMD_LAYOUT_INVALID_PACKET_ID;

	r = sgpu_bpmd_write(sbo, (const void *) &packet, sizeof(packet),
			    SGPU_BPMD_LAYOUT_PACKET_ALIGN_BYTE);
	if (r != sizeof(packet)) {
		DRM_ERROR("failed to dump packet data\n");
		return SGPU_BPMD_LAYOUT_INVALID_PACKET_ID;
	}

	r = sgpu_bpmd_write(sbo, (const void *) pt, num_pages * sizeof(uint64_t),
			SGPU_BPMD_LAYOUT_PACKET_ALIGN_BYTE);
	if (r != num_pages * sizeof(uint64_t)) {
		DRM_ERROR("failed to dump flags data\n");
		return SGPU_BPMD_LAYOUT_INVALID_PACKET_ID;
	}

	return packet.header.id;
}

uint32_t sgpu_bpmd_layout_dump_footer_packet(struct amdgpu_device *adev, struct sgpu_bpmd_output *sbo)
{
	const size_t total_size = sizeof(struct sgpu_bpmd_layout_footer_packet);
	struct sgpu_bpmd_layout_footer_packet packet = {
		. header = {
			.kind   = SGPU_BPMD_LAYOUT_PACKET_FOOTER,
			.id     = sgpu_bpmd_get_packet_id(),
			.length = total_size - sizeof(struct sgpu_bpmd_layout_packet_header)
		},
		.crc32 = sbo->crc32 ^ 0xffffffff,
	};

	int r = sgpu_bpmd_write(sbo,
				(const void *) &packet,
				sizeof(struct sgpu_bpmd_layout_footer_packet),
				SGPU_BPMD_LAYOUT_PACKET_ALIGN_BYTE);

	if (sbo->output_type == SGPU_BPMD_MEMLOGGER)
		sgpu_bpmd_log_packet_footer(adev);

	if (r != total_size) {
		DRM_ERROR("failed to dump footer packet");
		return SGPU_BPMD_LAYOUT_INVALID_PACKET_ID;
	} else {
		return packet.header.id;
	}
}
