/* SPDX-License-Identifier: GPL-2.0-only */
/*
* @file sgpu_bpmd_log.h
* @copyright 2023 Samsung Electronics
*/

#ifndef _SGPU_BPMD_LOG_H_
#define _SGPU_BPMD_LOG_H_

#include "amdgpu.h"
#include "amdgpu_vm.h"
#include "sgpu_bpmd.h"
#include "sgpu_bpmd_layout.h"

# if IS_ENABLED(CONFIG_DRM_SGPU_BPMD_MEMLOGGER_TEXT)
#include <soc/samsung/exynos/memlogger.h>

int sgpu_bpmd_log_init(struct amdgpu_device *adev);
int sgpu_bpmd_log_warning(struct amdgpu_device *adev, const char* msg);
int sgpu_bpmd_log_system_info(
	struct amdgpu_device *adev,
	const struct sgpu_bpmd_hw_revision *hw_revision);
int sgpu_bpmd_log_registers(
	struct amdgpu_device *adev,
	const struct sgpu_bpmd_layout_reg32 *regs,
	uint32_t num_regs);
int sgpu_bpmd_log_packet_reg32(
	struct amdgpu_device *adev,
	const struct sgpu_bpmd_layout_reg32_packet *packet);
int sgpu_bpmd_log_packet_indexed_reg32(
	struct amdgpu_device *adev,
	const struct sgpu_bpmd_layout_indexed_reg32_packet *packet);
int sgpu_bpmd_log_packet_multiple_read_reg32(
	struct amdgpu_device *adev,
	const struct sgpu_bpmd_layout_reg32_multiple_read_packet *packet);
int sgpu_bpmd_log_packet_footer(struct amdgpu_device *adev);

# else /* !CONFIG_DRM_SGPU_BPMD_MEMLOGGER_TEXT */

static inline int sgpu_bpmd_log_init(struct amdgpu_device *adev)
{
	return 0;
}

static inline int sgpu_bpmd_log_system_info(
	struct amdgpu_device *adev,
	const struct sgpu_bpmd_hw_revision *hw_revision)
{
	return 0;
}

static inline int sgpu_bpmd_log_registers(
	struct amdgpu_device *adev,
	const struct sgpu_bpmd_layout_reg32 *regs,
	uint32_t num_regs)
{
	return 0;
}

static inline int sgpu_bpmd_log_packet_reg32(
	struct amdgpu_device *adev,
	const struct sgpu_bpmd_layout_reg32_packet *packet)
{
	return 0;
}

static inline int sgpu_bpmd_log_packet_indexed_reg32(
	struct amdgpu_device *adev,
	const struct sgpu_bpmd_layout_indexed_reg32_packet *packet)
{
	return 0;
}

static inline int sgpu_bpmd_log_packet_multiple_read_reg32(
	struct amdgpu_device *adev,
	const struct sgpu_bpmd_layout_reg32_multiple_read_packet *packet)
{
	return 0;
}

static inline int sgpu_bpmd_log_packet_footer(struct amdgpu_device *adev)
{
	return 0;
}

# endif /* CONFIG_DRM_SGPU_BPMD_MEMLOGGER_TEXT */

#endif /* _SGPU_BPMD_LOG_H_ */
