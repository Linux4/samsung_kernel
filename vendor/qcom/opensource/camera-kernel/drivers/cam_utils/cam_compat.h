/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) 2014-2019, The Linux Foundation. All rights reserved.
 */

#ifndef _CAM_COMPAT_H_
#define _CAM_COMPAT_H_

#include <linux/version.h>
#include <linux/dma-buf.h>
#include <linux/dma-iommu.h>

#include "cam_csiphy_dev.h"
#include "cam_cpastop_hw.h"

#if LINUX_VERSION_CODE < KERNEL_VERSION(5, 7, 0)

#define VFL_TYPE_VIDEO VFL_TYPE_GRABBER

#endif

#if LINUX_VERSION_CODE >= KERNEL_VERSION(5, 4, 0)

#include <linux/msm_ion.h>
#include <linux/ion.h>
#include <linux/qcom_scm.h>

#else

#include <linux/msm_ion.h>
#include <linux/ion_kernel.h>
#include <soc/qcom/scm.h>

#endif

struct cam_fw_alloc_info {
	struct device *fw_dev;
	void          *fw_kva;
	uint64_t       fw_hdl;
};

int cam_reserve_icp_fw(struct cam_fw_alloc_info *icp_fw, size_t fw_length);
void cam_unreserve_icp_fw(struct cam_fw_alloc_info *icp_fw, size_t fw_length);
void cam_cpastop_scm_write(struct cam_cpas_hw_errata_wa *errata_wa);
int cam_ife_notify_safe_lut_scm(bool safe_trigger);
int cam_csiphy_notify_secure_mode(struct csiphy_device *csiphy_dev,
	bool protect, int32_t offset);
void cam_free_clear(const void *);
int cam_compat_util_get_dmabuf_va(struct dma_buf *dmabuf, uintptr_t *vaddr);
void cam_compat_util_put_dmabuf_va(struct dma_buf *dmabuf, void *vaddr);
void cam_smmu_util_iommu_custom(struct device *dev,
	dma_addr_t discard_start, size_t discard_length);

#endif /* _CAM_COMPAT_H_ */
