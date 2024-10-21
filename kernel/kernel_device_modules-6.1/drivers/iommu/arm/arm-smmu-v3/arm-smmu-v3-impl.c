// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2023 MediaTek Inc.
 * Author: Ning Li <ning.li@mediatek.com>
 */

#include "arm-smmu-v3.h"

struct arm_smmu_device *arm_smmu_v3_impl_init(struct arm_smmu_device *smmu)
{
	if (of_device_is_compatible(smmu->dev->of_node, "mtk,smmu-v700"))
		return mtk_smmu_v3_impl_init(smmu);

	return smmu;
}

MODULE_DESCRIPTION("MediaTek SMMUv3 Customization");
MODULE_LICENSE("GPL");
