/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (C) 2023 MediaTek Inc.
 */

#ifndef SMMU_SECURE_H
#define SMMU_SECURE_H

#define SMC_SMMU_SUCCESS			(0)
#define SMC_SMMU_FAIL				(1)
#define SMC_SMMU_NOSUPPORT			(-1)

enum smmu_test_enum {
	TEST_FUNC_BASIC = 0,
	TEST_FUNC_FULLY,
	TEST_FUNC_MAX
};

#if IS_ENABLED(CONFIG_MTK_IOMMU_MISC_SECURE)
int mtk_smmu_sec_init(u32 smmu_type);
int mtk_smmu_sec_irq_setup(u32 smmu_type, bool enable);
int mtk_smmu_sec_tf_handler(u32 smmu_type, bool *need_handle,
			    unsigned long *fault_iova,
			    unsigned long *fault_pa,
			    unsigned long *fault_id);
int mtk_smmu_pm_get(uint32_t smmu_type);
int mtk_smmu_pm_put(uint32_t smmu_type);
int mtk_smmu_dump_sid(uint32_t smmu_type, uint32_t sid);
#else
static inline int mtk_smmu_sec_init(u32 smmu_type)
{
	pr_info("%s not support\n", __func__);
	return 0;
}

static inline int mtk_smmu_sec_irq_setup(u32 smmu_type, bool enable)
{
	pr_info("%s not support\n", __func__);
	return 0;
}

static inline int mtk_smmu_sec_tf_handler(u32 smmu_type, bool *need_handle,
					  unsigned long *fault_iova,
					  unsigned long *fault_pa,
					  unsigned long *fault_id)
{
	pr_info("%s not support\n", __func__);
	return 0;
}

static inline int mtk_smmu_pm_get(uint32_t smmu_type)
{
	return 0;
}

static inline int mtk_smmu_pm_put(uint32_t smmu_type)
{
	return 0;
}

static inline int mtk_smmu_dump_sid(uint32_t smmu_type, uint32_t sid)
{
	return 0;
}
#endif /* CONFIG_MTK_IOMMU_MISC_SECURE */

#if IS_ENABLED(CONFIG_MTK_IOMMU_MISC_SECURE) && IS_ENABLED(CONFIG_MTK_IOMMU_DEBUG)
int mtk_smmu_sec_config_cqdma(bool enable);
int mtk_smmu_sec_test(u32 smmu_type, u32 lvl);
#else
static inline int mtk_smmu_sec_config_cqdma(bool enable)
{
	return 0;
}

static inline int mtk_smmu_sec_test(u32 smmu_type, u32 lvl)
{
	return 0;
}
#endif /* CONFIG_MTK_IOMMU_MISC_SECURE && CONFIG_MTK_IOMMU_DEBUG */
#endif /* SMMU_SECURE_H */
