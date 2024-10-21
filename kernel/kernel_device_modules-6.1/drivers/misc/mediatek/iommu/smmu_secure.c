// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2023 MediaTek Inc.
 */

#define pr_fmt(fmt)    "mtk_smmu: sec " fmt

#include <linux/soc/mediatek/mtk_sip_svc.h>
#include <linux/arm-smccc.h>

#include "mtk-smmu-v3.h"
#include "smmu_secure.h"
#include "iommu_debug.h"

#define SMMU_SUCCESS			(0)
#define SMMU_ID_ERR			(1)
#define SMMU_CMD_ERR			(2)
#define SMMU_PARA_INVALID		(3)
#define SMMU_NEED			(4)
#define SMMU_NONEED			(5)

/*
 * SMMU TF-A SMC cmd format:
 * sec[11:11] + smmu_type[10:8] + cmd_id[7:0]
 */
#define SMMU_ATF_SET_CMD(smmu_type, sec, cmd_id) \
	((cmd_id) | (smmu_type << 8) | (sec << 11))

enum smmu_atf_cmd {
	SMMU_SECURE_INIT,
	SMMU_SECURE_IRQ_SETUP,
	SMMU_SECURE_TF_HANDLE,
	SMMU_SECURE_PM_GET,
	SMMU_SECURE_PM_PUT,
	SMMU_SECURE_DUMP_SID,
	SMMU_SECURE_AID_SID_MAP,
	SMMU_SECURE_TRIGGER_IRQ,
	SMMU_SECURE_TEST,
	SMMU_SECURE_CONFIG_CQDMA,
	SMMU_SECURE_DUMP_REG,
	SMMU_SECURE_DUMP_PGTABLE,
	SMMU_CMD_NUM
};

static int mtk_smmu_hw_is_valid(uint32_t smmu_type)
{
	if (smmu_type >= SMMU_TYPE_NUM) {
		pr_info("%s type is invalid, %u\n", __func__, smmu_type);
		return SMC_SMMU_FAIL;
	}

	return SMC_SMMU_SUCCESS;
}

/*
 * a0/in0 = MTK_IOMMU_SECURE_CONTROL(IOMMU SMC ID)
 * a1/in1 = SMMU TF-A SMC cmd (sec + smmu_type + cmd_id)
 * a2/in2 ~ a7/in7: user defined
 */
static int mtk_smmu_atf_call(uint32_t smmu_type, unsigned long cmd,
			     unsigned long in2, unsigned long in3, unsigned long in4,
			     unsigned long in5, unsigned long in6, unsigned long in7)
{
	struct arm_smccc_res res;
	int ret;

	ret = mtk_smmu_hw_is_valid(smmu_type);
	if (ret) {
		pr_info("%s, SMMU HW type is invalid, type:%u\n", __func__, smmu_type);
		return SMC_SMMU_FAIL;
	}

	arm_smccc_smc(MTK_IOMMU_SECURE_CONTROL, cmd, in2, in3, in4, in5, in6, in7, &res);

	return res.a0;
}

static int mtk_smmu_atf_call_common(u32 smmu_type, unsigned long cmd_id)
{
	unsigned long cmd = SMMU_ATF_SET_CMD(smmu_type, 1, cmd_id);

	return mtk_smmu_atf_call(smmu_type, cmd, 0, 0, 0, 0, 0, 0);
}

static int mtk_smmu_atf_call_res(uint32_t smmu_type, unsigned long cmd,
				 unsigned long in2, unsigned long in3, unsigned long in4,
				 unsigned long in5, unsigned long in6, unsigned long in7,
				 unsigned long *out1, unsigned long *out2, unsigned long *out3)
{
	struct arm_smccc_res res;
	int ret;

	ret = mtk_smmu_hw_is_valid(smmu_type);
	if (ret) {
		pr_info("%s, SMMU HW type is invalid, type:%u\n", __func__, smmu_type);
		return SMC_SMMU_FAIL;
	}

	arm_smccc_smc(MTK_IOMMU_SECURE_CONTROL, cmd, in2, in3, in4, in5, in6, in7, &res);
	*out1 = (unsigned long)res.a1;
	*out2 = (unsigned long)res.a2;
	*out3 = (unsigned long)res.a3;

	return res.a0;
}

int mtk_smmu_sec_init(u32 smmu_type)
{
	int ret;

	ret = mtk_smmu_atf_call_common(smmu_type, SMMU_SECURE_INIT);
	if (ret) {
		pr_info("%s, smc call fail:%d, type:%u\n", __func__, ret, smmu_type);
		return SMC_SMMU_FAIL;
	}

	return SMC_SMMU_SUCCESS;
}
EXPORT_SYMBOL_GPL(mtk_smmu_sec_init);

int mtk_smmu_sec_irq_setup(u32 smmu_type, bool enable)
{
	unsigned long cmd = SMMU_ATF_SET_CMD(smmu_type, 1, SMMU_SECURE_IRQ_SETUP);
	unsigned long en = enable ? 1 : 0;
	int ret;

	ret = mtk_smmu_atf_call(smmu_type, cmd, en, 0, 0, 0, 0, 0);
	if (ret) {
		pr_info("%s, smc call fail:%d, type:%u\n", __func__, ret, smmu_type);
		return SMC_SMMU_FAIL;
	}

	return SMC_SMMU_SUCCESS;
}
EXPORT_SYMBOL_GPL(mtk_smmu_sec_irq_setup);

int mtk_smmu_sec_tf_handler(u32 smmu_type, bool *need_handle,
			    unsigned long *fault_iova, unsigned long *fault_pa,
			    unsigned long *fault_id)
{
	unsigned long cmd;
	int ret;

	cmd = SMMU_ATF_SET_CMD(smmu_type, 1, SMMU_SECURE_TF_HANDLE);
	ret = mtk_smmu_atf_call_res(smmu_type, cmd, 0, 0, 0, 0, 0, 0,
				    fault_iova, fault_pa, fault_id);
	if (ret == SMMU_NEED) {
		*need_handle = true;
	} else if (ret == SMMU_NONEED) {
		*need_handle = false;
	} else if (ret) {
		pr_info("%s, smc call fail:%d, type:%u\n", __func__, ret, smmu_type);
		return SMC_SMMU_FAIL;
	}

	return SMC_SMMU_SUCCESS;
}
EXPORT_SYMBOL_GPL(mtk_smmu_sec_tf_handler);

int mtk_smmu_pm_get(uint32_t smmu_type)
{
	int ret;

	ret = mtk_smmu_atf_call_common(smmu_type, SMMU_SECURE_PM_GET);
	if (ret)
		return SMC_SMMU_FAIL;

	return SMC_SMMU_SUCCESS;
}
EXPORT_SYMBOL_GPL(mtk_smmu_pm_get);

int mtk_smmu_pm_put(uint32_t smmu_type)
{
	int ret;

	ret = mtk_smmu_atf_call_common(smmu_type, SMMU_SECURE_PM_PUT);
	if (ret) {
		pr_info("%s, smc call fail:%d, type:%u\n", __func__, ret, smmu_type);
		return SMC_SMMU_FAIL;
	}

	return SMC_SMMU_SUCCESS;
}
EXPORT_SYMBOL_GPL(mtk_smmu_pm_put);

int mtk_smmu_dump_sid(uint32_t smmu_type, uint32_t sid)
{
	unsigned long cmd;
	int ret;

	cmd = SMMU_ATF_SET_CMD(smmu_type, 1, SMMU_SECURE_DUMP_SID);
	ret = mtk_smmu_atf_call(smmu_type, cmd, sid, 0, 0, 0, 0, 0);
	if (ret) {
		pr_info("%s, smc call fail:%d, type:%u\n", __func__, ret, smmu_type);
		return SMC_SMMU_FAIL;
	}

	return SMC_SMMU_SUCCESS;
}
EXPORT_SYMBOL_GPL(mtk_smmu_dump_sid);

#if IS_ENABLED(CONFIG_MTK_IOMMU_DEBUG)
static int mtk_smmu_aid2sid(uint32_t smmu_type, uint32_t sec,
			    uint32_t sid, uint32_t aid)
{
	unsigned long cmd;
	int ret;

	cmd = SMMU_ATF_SET_CMD(smmu_type, (!!sec), SMMU_SECURE_AID_SID_MAP);
	ret = mtk_smmu_atf_call(smmu_type, cmd, sid, aid, 0, 0, 0, 0);
	if (ret) {
		pr_info("%s, smc call fail:%d, type:%u\n", __func__, ret, smmu_type);
		return SMC_SMMU_FAIL;
	}

	return SMC_SMMU_SUCCESS;
}

static int mtk_smmu_trigger_irq(u32 smmu_type, u32 sec)
{
	unsigned long cmd;
	int ret;

	cmd = SMMU_ATF_SET_CMD(smmu_type, (!!sec), SMMU_SECURE_TRIGGER_IRQ);
	ret = mtk_smmu_atf_call(smmu_type, cmd, 0, 0, 0, 0, 0, 0);
	if (ret) {
		pr_info("%s, smc call fail:%d, type:%u\n", __func__, ret, smmu_type);
		return SMC_SMMU_FAIL;
	}

	return SMC_SMMU_SUCCESS;
}

static int mtk_smmu_sec_dump_reg(u32 smmu_type)
{
	int ret;

	ret = mtk_smmu_atf_call_common(smmu_type, SMMU_SECURE_DUMP_REG);
	if (ret) {
		pr_info("%s, smc call fail:%d, type:%u\n", __func__, ret, smmu_type);
		return SMC_SMMU_FAIL;
	}

	return SMC_SMMU_SUCCESS;
}

static int mtk_smmu_sec_dump_pgtable(u32 smmu_type, u32 fmt)
{
	unsigned long cmd;
	int ret;

	cmd = SMMU_ATF_SET_CMD(smmu_type, 1, SMMU_SECURE_DUMP_PGTABLE);
	ret = mtk_smmu_atf_call(smmu_type, cmd, fmt, 0, 0, 0, 0, 0);
	if (ret) {
		pr_info("%s, smc call fail:%d, type:%u\n", __func__, ret, smmu_type);
		return SMC_SMMU_FAIL;
	}

	return SMC_SMMU_SUCCESS;
}

int mtk_smmu_sec_config_cqdma(bool enable)
{
	unsigned long cmd = SMMU_ATF_SET_CMD(SOC_SMMU, 0, SMMU_SECURE_CONFIG_CQDMA);
	unsigned long en = enable ? 1 : 0;
	int ret;

	ret = mtk_smmu_atf_call(SOC_SMMU, cmd, en, 0, 0, 0, 0, 0);
	if (ret) {
		pr_info("%s, smc call fail:%d, type:%u\n", __func__, ret, SOC_SMMU);
		return SMC_SMMU_FAIL;
	}

	return SMC_SMMU_SUCCESS;
}
EXPORT_SYMBOL_GPL(mtk_smmu_sec_config_cqdma);

int mtk_smmu_sec_test(u32 smmu_type, u32 lvl)
{
	unsigned long cmd = SMMU_ATF_SET_CMD(smmu_type, 1, SMMU_SECURE_TEST);
	int ret;

	ret = mtk_smmu_atf_call(smmu_type, cmd, lvl, 0, 0, 0, 0, 0);
	if (ret) {
		pr_info("%s, smc call fail:%d, type:%u\n", __func__, ret, smmu_type);
		return SMC_SMMU_FAIL;
	}

	return SMC_SMMU_SUCCESS;
}
EXPORT_SYMBOL_GPL(mtk_smmu_sec_test);

#define DEBUG_SET_PARAM_CMD		GENMASK_ULL(3, 0)
#define DEBUG_SET_PARAM_SMMU		GENMASK_ULL(5, 4)
#define DEBUG_SET_PARAM_SEC		GENMASK_ULL(6, 6)
#define DEBUG_SET_PARAM_EN		GENMASK_ULL(7, 7)
#define DEBUG_SET_PARAM_AID		GENMASK_ULL(15, 8)
#define DEBUG_SET_PARAM_SID		GENMASK_ULL(23, 16)
#define DEBUG_SET_PARAM_FMT		GENMASK_ULL(25, 24)

static int mtk_smmu_sec_debug_set(void *data, u64 input)
{
	u32 index = (input & DEBUG_SET_PARAM_CMD);
	u32 smmu_type = (input & DEBUG_SET_PARAM_SMMU) >> 4;
	u32 sec = !!((input & DEBUG_SET_PARAM_SEC) >> 6);
	u32 enable = !!((input & DEBUG_SET_PARAM_EN) >> 7);
	u32 aid = (input & DEBUG_SET_PARAM_AID) >> 8;
	u32 sid = (input & DEBUG_SET_PARAM_SID) >> 16;
	u32 fmt = (input & DEBUG_SET_PARAM_FMT) >> 24;
	unsigned long fault_iova = 0, fault_pa = 0, fault_id = 0;
	bool need_handle = false;
	int ret = 0;

	pr_info("%s input:0x%llx, index:%u, smmu:%u, sec:%u, enable:%u, aid:%u, sid:%u, fmt:%u\n",
		__func__, input, index, smmu_type, sec, enable, aid, sid, fmt);

	switch (index) {
	case SMMU_SECURE_INIT:
		pr_info("%s, SMMU_SECURE_INIT:\n", __func__);
		ret = mtk_smmu_sec_init(smmu_type);
		break;
	case SMMU_SECURE_IRQ_SETUP:
		pr_info("%s, SMMU_SECURE_IRQ_SETUP:\n", __func__);
		ret = mtk_smmu_sec_irq_setup(smmu_type, enable);
		break;
	case SMMU_SECURE_TF_HANDLE:
		pr_info("%s, SMMU_SECURE_TF_HANDLE:\n", __func__);
		ret = mtk_smmu_sec_tf_handler(smmu_type, &need_handle, &fault_iova,
					      &fault_pa, &fault_id);
		pr_info("%s, need_handle:%d, fault_iova:ox%lx, fault_pa:ox%lx, fault_id:ox%lx\n",
			__func__, need_handle, fault_iova, fault_pa, fault_id);
		break;
	case SMMU_SECURE_DUMP_SID:
		pr_info("%s, SMMU_SECURE_DUMP_SID:\n", __func__);
		ret = mtk_smmu_dump_sid(smmu_type, sid);
		break;
	case SMMU_SECURE_AID_SID_MAP:
		pr_info("%s, SMMU_SECURE_AID_SID_MAP:\n", __func__);
		ret = mtk_smmu_aid2sid(smmu_type, sec, sid, aid);
		break;
	case SMMU_SECURE_TRIGGER_IRQ:
		pr_info("%s, SMMU_SECURE_TRIGGER_IRQ:\n", __func__);
		ret = mtk_smmu_trigger_irq(smmu_type, sec);
		break;
	case SMMU_SECURE_DUMP_REG:
		pr_info("%s, SMMU_SECURE_DUMP_REG\n", __func__);
		ret = mtk_smmu_sec_dump_reg(smmu_type);
		break;
	case SMMU_SECURE_DUMP_PGTABLE:
		pr_info("%s, SMMU_SECURE_DUMP_PGTABLE\n", __func__);
		ret = mtk_smmu_sec_dump_pgtable(smmu_type, fmt);
		break;
	default:
		pr_info("%s error, index:%u\n", __func__, index);
		break;
	}

	if (ret)
		pr_info("%s failed, input:0x%llx, index:%u, ret:%d\n",
			__func__, input, index, ret);

	return 0;
}

static int mtk_smmu_sec_debug_get(void *data, u64 *val)
{
	*val = 0;
	return 0;
}

DEFINE_PROC_ATTRIBUTE(mtk_smmu_sec_debug_fops, mtk_smmu_sec_debug_get,
		      mtk_smmu_sec_debug_set, "%llu\n");

static int mtk_smmu_sec_debug_init(void)
{
	struct proc_dir_entry *debug_root;
	struct proc_dir_entry *debug_file;

	debug_root = proc_mkdir("smmu_sec", NULL);
	if (IS_ERR_OR_NULL(debug_root))
		pr_info("%s failed to create debug dir\n", __func__);

	debug_file = proc_create_data("debug",
		S_IFREG | 0640, debug_root, &mtk_smmu_sec_debug_fops, NULL);

	if (IS_ERR_OR_NULL(debug_file))
		pr_info("%s failed to create debug file\n", __func__);

	return 0;
}

static int __init mtk_smmu_secure_init(void)
{
	pr_info("%s+\n", __func__);
	mtk_smmu_sec_debug_init();
	pr_info("%s-\n", __func__);

	return 0;
}

static void __exit mtk_smmu_secure_exit(void)
{
	pr_info("%s, no do some thing\n", __func__);
}
#else  /* CONFIG_MTK_IOMMU_DEBUG */
static int __init mtk_smmu_secure_init(void)
{
	return 0;
}

static void __exit mtk_smmu_secure_exit(void)
{
}
#endif  /* CONFIG_MTK_IOMMU_DEBUG */

module_init(mtk_smmu_secure_init);
module_exit(mtk_smmu_secure_exit);
MODULE_DESCRIPTION("MediaTek SMMUv3 Secure Implement");
MODULE_LICENSE("GPL");
