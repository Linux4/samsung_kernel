// SPDX-License-Identifier: GPL-2.0-only
/*
* @file sgpu_bpmd_log.c
* @copyright 2023 Samsung Electronics
*/

#include "sgpu_bpmd_log.h"
#include "sgpu_dmsg.h"

int sgpu_bpmd_log_init(struct amdgpu_device *adev)
{
	const char *dev_name = "SGPU";
	struct memlog *desc = NULL;
	int ret = memlog_register(dev_name, adev->dev, &desc);

	if (!ret && desc) {
		/* Allocate static amount of memory as only registers are dumped in this mode */
		adev->bpmd.memlog_text = memlog_alloc_printf(desc, SZ_4M,
			NULL, "textual_bpmd", MEMLOG_UFALG_NO_TIMESTAMP);

		if (!adev->bpmd.memlog_text) {
			DRM_ERROR("Failed to allocate memlogger");
			return -ENOMEM;
		}
	} else {
		DRM_ERROR("Cannot register memlogger");
	}

	return ret;
}

int sgpu_bpmd_log_system_info(
	struct amdgpu_device *adev,
	const struct sgpu_bpmd_hw_revision *hw_revision)
{
	if (!adev->bpmd.memlog_text)
		return -ENOMEM;

	memlog_write_printf(adev->bpmd.memlog_text, MEMLOG_LEVEL_NOTICE, "HW revision: %d.%d.%d.%d\n",
		hw_revision->generation,
		hw_revision->model,
		hw_revision->major,
		hw_revision->minor
	);

	return 0;
}

int sgpu_bpmd_log_registers(
	struct amdgpu_device *adev,
	const struct sgpu_bpmd_layout_reg32 *regs,
	uint32_t num_regs)
{
	int i = 0;

	if (!adev->bpmd.memlog_text)
		return -ENOMEM;

	for (; i < num_regs; i++) {
		memlog_write_printf(adev->bpmd.memlog_text, MEMLOG_LEVEL_NOTICE, "%x:%x\n", regs[i].index, regs[i].value);
	}
	return 0;
}

int sgpu_bpmd_log_packet_reg32(
	struct amdgpu_device *adev,
	const struct sgpu_bpmd_layout_reg32_packet *packet)
{
	if (!adev->bpmd.memlog_text)
		return -ENOMEM;

	return memlog_write_printf(adev->bpmd.memlog_text, MEMLOG_LEVEL_NOTICE, "GRBM gfx index:%x\n", packet->grbm_gfx_index);
}

int sgpu_bpmd_log_packet_indexed_reg32(
	struct amdgpu_device *adev,
	const struct sgpu_bpmd_layout_indexed_reg32_packet *packet)
{
	if (!adev->bpmd.memlog_text)
		return -ENOMEM;

	return memlog_write_printf(adev->bpmd.memlog_text, MEMLOG_LEVEL_NOTICE, "GRBM gfx index:%x, index:%x\n", packet->grbm_gfx_index, packet->index_register);
}

int sgpu_bpmd_log_packet_multiple_read_reg32(
	struct amdgpu_device *adev,
	const struct sgpu_bpmd_layout_reg32_multiple_read_packet *packet)
{
	int i = 0;

	if (!adev->bpmd.memlog_text)
		return -ENOMEM;

	memlog_write_printf(adev->bpmd.memlog_text, MEMLOG_LEVEL_NOTICE, "GRBM gfx index:%x, index:%x, data:%x\n", packet->grbm_gfx_index, packet->index_register, packet->data_register);
	for (; i < packet->read_count; i++)
		memlog_write_printf(adev->bpmd.memlog_text, MEMLOG_LEVEL_NOTICE, "%x\n", packet->values[i]);

	return 0;
}

int sgpu_bpmd_log_packet_footer(struct amdgpu_device *adev)
{
	return memlog_write_printf(adev->bpmd.memlog_text, MEMLOG_LEVEL_NOTICE, "=== End ===\n");
}
