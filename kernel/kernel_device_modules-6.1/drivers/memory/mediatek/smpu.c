// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2022 MediaTek Inc.
 */

#include <soc/mediatek/smpu.h>
#include <linux/arm-smccc.h>
#include <linux/device.h>
#include <linux/interrupt.h>
#include <linux/io.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/of_irq.h>
#include <linux/kthread.h>
#include <linux/platform_device.h>
#include <linux/printk.h>
#include <linux/slab.h>
#include <linux/soc/mediatek/mtk_sip_svc.h>
#include <mt-plat/aee.h>
#include <linux/ratelimit.h>
#include <linux/soc/mediatek/mtk_sip_svc.h>
#include <linux/delay.h>

extern int mtk_clear_smpu_log(unsigned int emi_id);
//static struct kthread_worker *smpu_kworker;
struct smpu *global_ssmpu;
EXPORT_SYMBOL_GPL(global_ssmpu);

struct smpu *global_nsmpu;
EXPORT_SYMBOL_GPL(global_nsmpu);

struct smpu *global_skp, *global_nkp;

static void set_regs(struct smpu_reg_info_t *reg_list, unsigned int reg_cnt,
		     void __iomem *smpu_base)
{
	unsigned int i, j;

	for (i = 0; i < reg_cnt; i++) {
		for (j = 0; j < reg_list[i].leng; j++)
			writel(reg_list[i].value,
			       smpu_base + reg_list[i].offset + 4 * j);
	}
	/*
	 * Use the memory barrier to make sure the interrupt signal is
	 * de-asserted (by programming registers) before exiting the
	 * ISR and re-enabling the interrupt.
	 */
	mb();
}
static void clear_violation(struct smpu *mpu)
{
	void __iomem *mpu_base;
	//struct arm_smccc_res smc_res;

	mpu_base = mpu->mpu_base;

	set_regs(mpu->clear_reg, mpu->clear_cnt, mpu_base);
	//	pr_info("smpu clear vio done\n");
}

static void mask_irq(struct smpu *mpu)
{
	void __iomem *mpu_base;

	mpu_base = mpu->mpu_base;
	set_regs(mpu->mask_reg, mpu->mask_cnt, mpu_base);
}

static void clear_kp_violation(unsigned int emi_id)
{
	struct arm_smccc_res smc_res;

	arm_smccc_smc(MTK_SIP_EMIMPU_CONTROL, MTK_EMIMPU_CLEAR_KP, emi_id, 0, 0,
		      0, 0, 0, &smc_res);
}
void smpu_clear_md_violation(void)
{
	struct smpu *smpu;
	void __iomem *mpu_base;
	bool flag = false;
	struct arm_smccc_res smc_res;

	if (global_nsmpu) {
		smpu = global_nsmpu;
		mpu_base = smpu->mpu_base;
		if (smpu->clear_md_reg) {
			set_regs(smpu->clear_md_reg, smpu->clear_md_cnt,
				 mpu_base);
			flag = true;
		}
	}
	if (global_ssmpu) {
		smpu = global_ssmpu;
		mpu_base = smpu->mpu_base;
		if (smpu->clear_md_reg)
			set_regs(smpu->clear_md_reg, smpu->clear_md_cnt,
				 mpu_base);
	}

	if (!flag) {
		pr_info("smpu_clear_md_vio enter\n");
		arm_smccc_smc(MTK_SIP_EMIMPU_CONTROL, MTK_EMIMPU_CLEAR_MD, 0, 0,
			      0, 0, 0, 0, &smc_res);
		if (smc_res.a0) {
			pr_info("%s:%d failed to clear md violation, ret=0x%lx\n",
				__func__, __LINE__, smc_res.a0);
			return;
		}
	}
}
EXPORT_SYMBOL(smpu_clear_md_violation);

int mtk_smpu_isr_hook_register(smpu_isr_hook hook)
{
	struct smpu *ssmpu, *nsmpu, *skp, *nkp;

	ssmpu = global_ssmpu;
	nsmpu = global_nsmpu;
	skp = global_skp;
	nkp = global_nkp;

	if (!nsmpu || !nkp)
		return -EINVAL;

	pr_info("%s:hook-register half", __func__);

	if (!hook) {
		pr_info("%s: hook is NULL\n", __func__);
		return -EINVAL;
	}

	nsmpu->by_plat_isr_hook = hook;
	nkp->by_plat_isr_hook = hook;

	if (ssmpu && skp) {
		ssmpu->by_plat_isr_hook = hook;
		skp->by_plat_isr_hook = hook;
	}

	return 0;
}
EXPORT_SYMBOL(mtk_smpu_isr_hook_register);

int mtk_smpu_md_handling_register(smpu_md_handler md_handling_func)
{
	struct smpu *ssmpu, *nsmpu, *skp, *nkp;

	ssmpu = global_ssmpu;
	nsmpu = global_nsmpu;
	skp = global_skp;
	nkp = global_nkp;

	if (!nsmpu || !nkp)
		return -EINVAL;

	if (!md_handling_func) {
		pr_info("%s: md_handling_func is NULL\n", __func__);
		return -EINVAL;
	}

	nsmpu->md_handler = md_handling_func;
	nkp->md_handler = md_handling_func;

	if (ssmpu && skp) {
		ssmpu->md_handler = md_handling_func;
		skp->md_handler = md_handling_func;
	}

	pr_info("%s:md_handling_func registered!\n", __func__);

	return 0;
}
EXPORT_SYMBOL(mtk_smpu_md_handling_register);

static void smpu_violation_callback(struct work_struct *work)
{
	struct smpu *ssmpu, *nsmpu, *skp, *nkp, *mpu;
	struct arm_smccc_res smc_res;
	struct device_node *smpu_node = of_find_node_by_name(NULL, "smpu");
	int by_pass_aid[3] = { 240, 241, 243 };
	int by_pass_region[10] = { 22, 28, 39, 41, 44, 45, 57, 59, 61, 62 };
	int i, j, by_pass_flag = 0;
	char md_str[MTK_SMPU_MAX_CMD_LEN + 5] = { '\0' };

	ssmpu = global_ssmpu;
	nsmpu = global_nsmpu;
	skp = global_skp;
	nkp = global_nkp;

	if (nsmpu && nsmpu->vio_msg && nsmpu->is_vio)
		mpu = nsmpu;
	else if (ssmpu && ssmpu->vio_msg && ssmpu->is_vio)
		mpu = ssmpu;
	else if (nkp && nkp->vio_msg && nkp->is_vio)
		mpu = nkp;
	else if (skp && skp->vio_msg && skp->is_vio)
		mpu = skp;
	else
		return;

	pr_info("%s: %s", __func__, mpu->vio_msg);

	if (mpu->md_handler) {
		strncpy(md_str, "smpu", 5);
		strncat(md_str, mpu->vio_msg,
			sizeof(md_str) - strlen(md_str) - 1);
		mpu->md_handler(md_str);
	}
	/* check vio region addr */
	if ((nsmpu && nsmpu->is_vio) || (ssmpu && ssmpu->is_vio)) {
		if (mpu->dump_reg[7].value != 0) {
			/*type(0sa 1ea) region aid_shift*/
			arm_smccc_smc(MTK_SIP_EMIMPU_CONTROL, MTK_EMIMPU_READ,
				      0, mpu->dump_reg[7].value, 0, 0, 0, 0,
				      &smc_res);
			arm_smccc_smc(MTK_SIP_EMIMPU_CONTROL, MTK_EMIMPU_READ,
				      1, mpu->dump_reg[7].value, 0, 0, 0, 0,
				      &smc_res);
		}
		if (mpu->dump_reg[16].value != 0) {
			arm_smccc_smc(MTK_SIP_EMIMPU_CONTROL, MTK_EMIMPU_READ,
				      0, mpu->dump_reg[16].value, 0, 0, 0, 0,
				      &smc_res);
			arm_smccc_smc(MTK_SIP_EMIMPU_CONTROL, MTK_EMIMPU_READ,
				      1, mpu->dump_reg[16].value, 0, 0, 0, 0,
				      &smc_res);
		}
	}

	msleep(30);

	if ((nsmpu && nsmpu->is_vio) || (ssmpu && ssmpu->is_vio)) {
		for (i = 0; i < 3; i++) {
			for (j = 0; j < 10; j++) {
				if (mpu->dump_reg[5].value == by_pass_aid[i] &&
				    mpu->dump_reg[7].value ==
					    by_pass_region[j]) {
					by_pass_flag++;
					break;
				}
			}
			if (by_pass_flag > 0)
				break;
		}

		if (by_pass_flag > 0 && // by pass WCE, this will be temp patch
		    of_property_count_elems_of_size(smpu_node, "bypass-wce",
						    sizeof(char))) {
			pr_info("%s:AID == 0x%x && region = 0x%x without KERNEL_API!!\n",
				__func__, mpu->dump_reg[5].value,
				mpu->dump_reg[7].value);
		} else if (!mpu->is_bypass) // by pass GPU write vio
			aee_kernel_exception("SMPU", mpu->vio_msg); // for smpu_vio case
	}else
		aee_kernel_exception("SMPU", mpu->vio_msg); // for KP case

	if (nsmpu)
		clear_violation(nsmpu);
	if (ssmpu)
		clear_violation(ssmpu);
	if (nkp)
		clear_violation(nkp);
	if (skp)
		clear_violation(skp);

	mpu->is_bypass = false;
	mpu->is_vio = false;
}
static DECLARE_WORK(smpu_work, smpu_violation_callback);

static irqreturn_t smpu_violation(int irq, void *dev_id)
{
	struct smpu *mpu = (struct smpu *)dev_id;
	struct smpu_reg_info_t *dump_reg = mpu->dump_reg;
	void __iomem *mpu_base;
	int i, vio_dump_idx, vio_dump_pos, prefetch;
	int vio_type = 6;
	bool violation;
	ssize_t msg_len = 0;
	//	struct task_struct *tsk;

	irqreturn_t irqret;
	static DEFINE_RATELIMIT_STATE(ratelimit, 1 * HZ, 3);

	violation = false;
	mpu_base = mpu->mpu_base;

	if (!(strcmp(mpu->name, "nsmpu")))
		vio_type = VIO_TYPE_NSMPU;
	else if (!(strcmp(mpu->name, "ssmpu")))
		vio_type = VIO_TYPE_SSMPU;
	else if (!(strcmp(mpu->name, "nkp")))
		vio_type = VIO_TYPE_NKP;
	else if (!(strcmp(mpu->name, "skp")))
		vio_type = VIO_TYPE_SKP;
	else
		goto clear_violation;

	//	pr_info("%s:vio_type = %d\n", __func__, vio_type);
	//record dump reg
	for (i = 0; i < mpu->dump_cnt; i++)
		dump_reg[i].value = readl(mpu_base + dump_reg[i].offset);

	//record vioinfo
	for (i = 0; i < mpu->vio_dump_cnt; i++) {
		vio_dump_idx = mpu->vio_reg_info[i].vio_dump_idx;
		vio_dump_pos = mpu->vio_reg_info[i].vio_dump_pos;
		if (CHECK_BIT(dump_reg[vio_dump_idx].value, vio_dump_pos)) {
			violation = true;
			mpu->is_vio = true;
		}
	}

	if (!violation) {
		if (__ratelimit(&ratelimit))
			pr_info("smpu no violation");
		clear_violation(mpu);
		return IRQ_HANDLED;
	}

	if (violation) {
		if (vio_type == VIO_TYPE_NSMPU || vio_type == VIO_TYPE_SSMPU) {
			//smpu violation
			if (mpu->by_plat_isr_hook) {
				irqret = mpu->by_plat_isr_hook(
					dump_reg, mpu->dump_cnt, vio_type);

				if (irqret == IRQ_HANDLED) {
					violation = true;
					mpu->is_vio = true;
					mpu->is_bypass = true;
					goto clear_violation;
				}
			}
		}

		if (msg_len < MTK_SMPU_MAX_CMD_LEN) {
			prefetch = mtk_clear_smpu_log(vio_type % 2);
			msg_len += scnprintf(mpu->vio_msg + msg_len,
					     MTK_SMPU_MAX_CMD_LEN - msg_len,
					     "\ncpu-prefetch:%d", prefetch);
			msg_len += scnprintf(mpu->vio_msg + msg_len,
					     MTK_SMPU_MAX_CMD_LEN - msg_len,
					     "\n[SMPU]%s\n", mpu->name);
		}

		for (i = 0; i < mpu->dump_cnt; i++) {
			if (msg_len < MTK_SMPU_MAX_CMD_LEN)
				msg_len += scnprintf(
					mpu->vio_msg + msg_len,
					MTK_SMPU_MAX_CMD_LEN - msg_len,
					"[%x]%x;", dump_reg[i].offset,
					dump_reg[i].value);
		}
		printk_deferred("%s: %s", __func__, mpu->vio_msg);
	}

clear_violation:
	mask_irq(mpu);
	if (violation)
		schedule_work(&smpu_work);

	if (vio_type == VIO_TYPE_NKP || vio_type == VIO_TYPE_SKP)
		clear_kp_violation(vio_type % 2);

	return IRQ_HANDLED;
}

static const struct of_device_id smpu_of_ids[] = {
	{
		.compatible = "mediatek,smpu",
	},
	{}
};
MODULE_DEVICE_TABLE(of, smpu_of_ids);

/* As SLC b mode enable CPU will write clean evict, which may trigger SMPU violation.
 * and those data may cached in CPU L3 cache through CPU prefetch.
 */
static void smpu_clean_cpu_write_vio(struct smpu *mpu)
{
	int sec_cpu_aid = 240;
	int ns_cpu_aid = 241;
	int hyp_cpu_aid = 243;
	bool slc_enable  = mpu->slc_b_mode;
	int i;
	void __iomem *mpu_base = mpu->mpu_base;
	struct smpu_reg_info_t *dump_reg = mpu->dump_reg;
	ssize_t msg_len = 0;

	/* smpu check violation */
	if (slc_enable) {
		/* read SMPU/KP vio reg */
		for (i = 0; i < mpu->dump_cnt; i++)
			dump_reg[i].value = readl(mpu_base + dump_reg[i].offset);
		if (msg_len < MTK_SMPU_MAX_CMD_LEN) {
			msg_len += scnprintf(mpu->vio_msg + msg_len,
					     MTK_SMPU_MAX_CMD_LEN - msg_len,
					     "\n[SMPU]%s\n", mpu->name);
		}
		for (i = 0; i < mpu->dump_cnt; i++) {
			if (msg_len < MTK_SMPU_MAX_CMD_LEN)
				msg_len += scnprintf(
					mpu->vio_msg + msg_len,
					MTK_SMPU_MAX_CMD_LEN - msg_len,
					"[%x]%x;", dump_reg[i].offset,
					dump_reg[i].value);
		}

		/* check whether cpu type master lead this smpu violation */
		if ((mpu == global_ssmpu) || (mpu == global_nsmpu)) {
			/* check smpu write violation aid reg */
			if ((mpu->dump_reg[5].value == sec_cpu_aid) ||
			    (mpu->dump_reg[5].value == ns_cpu_aid) ||
			    (mpu->dump_reg[5].value == hyp_cpu_aid)) {
				pr_info("%s: %s", __func__, mpu->vio_msg);
				clear_violation(mpu);
			}
		} else {
			/* check kp write violation aid reg */
			if ((MTK_SMPU_KP_AID(mpu->dump_reg[1].value) == sec_cpu_aid) ||
			    (MTK_SMPU_KP_AID(mpu->dump_reg[1].value) == ns_cpu_aid) ||
			    (MTK_SMPU_KP_AID(mpu->dump_reg[1].value) == hyp_cpu_aid)) {
				pr_info("%s: %s", __func__, mpu->vio_msg);
				clear_violation(mpu);
			}
		}
		pr_info("%s: WCE dump finish!!\n",__func__);
	}
}

static int smpu_probe(struct platform_device *pdev)
{
	struct device_node *smpu_node = pdev->dev.of_node;
	struct smpu *mpu;
	const char *name = NULL;

	int ret, i, size, axi_set_num;
	unsigned int *dump_list, *miumpu_bypass_list, *gpu_bypass_list;
	//	struct resource *res;

	dev_info(&pdev->dev, "driver probe");
	if (!smpu_node) {
		dev_err(&pdev->dev, "No smpu-reg");
		return -ENXIO;
	}

	mpu = devm_kzalloc(&pdev->dev, sizeof(struct smpu), GFP_KERNEL);
	if (!mpu)
		return -ENOMEM;

	if (!of_property_read_string(smpu_node, "name", &name))
		mpu->name = name;
	//is_vio default value
	mpu->is_vio = false;
	mpu->is_bypass = false;

	// dump_reg
	size = of_property_count_elems_of_size(smpu_node, "dump", sizeof(char));
	if (size <= 0) {
		dev_err(&pdev->dev, "No smpu node dump\n");
		return -ENXIO;
	}
	dump_list = devm_kmalloc(&pdev->dev, size, GFP_KERNEL);
	if (!dump_list)
		return -ENXIO;

	size >>= 2;
	mpu->dump_cnt = size;
	ret = of_property_read_u32_array(smpu_node, "dump", dump_list, size);
	if (ret) {
		dev_err(&pdev->dev, "no smpu dump\n");
		return -ENXIO;
	}

	mpu->dump_reg = devm_kmalloc(
		&pdev->dev, size * sizeof(struct smpu_reg_info_t), GFP_KERNEL);
	if (!(mpu->dump_reg))
		return -ENOMEM;

	for (i = 0; i < mpu->dump_cnt; i++) {
		mpu->dump_reg[i].offset = dump_list[i];
		mpu->dump_reg[i].value = 0;
		mpu->dump_reg[i].leng = 0;
	}
	//dump_reg end
	//dump_clear
	size = of_property_count_elems_of_size(smpu_node, "clear",
					       sizeof(char));
	if (size <= 0) {
		dev_err(&pdev->dev, "No clear smpu");
		return -ENXIO;
	}
	mpu->clear_reg = devm_kmalloc(&pdev->dev, size, GFP_KERNEL);
	if (!(mpu->clear_reg))
		return -ENOMEM;

	mpu->clear_cnt = size / sizeof(struct smpu_reg_info_t);
	size >>= 2;
	ret = of_property_read_u32_array(
		smpu_node, "clear", (unsigned int *)(mpu->clear_reg), size);
	if (ret) {
		dev_err(&pdev->dev, "No clear reg");
		return -ENXIO;
	}
	//dump_clear end
	//dump_clear_md
	size = of_property_count_elems_of_size(smpu_node, "clear-md",
					       sizeof(char));
	if (size <= 0)
		dev_err(&pdev->dev, "No clear_md smpu");
	if (size > 0) {
		mpu->clear_md_reg = devm_kmalloc(&pdev->dev, size, GFP_KERNEL);
		if (!(mpu->clear_md_reg))
			return -ENOMEM;

		mpu->clear_md_cnt = size / sizeof(struct smpu_reg_info_t);
		size >>= 2;
		ret = of_property_read_u32_array(
			smpu_node, "clear-md",
			(unsigned int *)(mpu->clear_md_reg), size);
		if (ret) {
			dev_err(&pdev->dev, "No clear-md reg");
			return -ENXIO;
		}
	}

	//dump_clear_md_end
	//dump vio info
	size = of_property_count_elems_of_size(smpu_node, "vio-info",
					       sizeof(char));

	mpu->vio_dump_cnt = size / sizeof(struct smpu_vio_dump_info_t);
	mpu->vio_reg_info = devm_kmalloc(&pdev->dev, size, GFP_KERNEL);
	if (!(mpu->vio_reg_info))
		return -ENOMEM;
	size >>= 2;
	ret = of_property_read_u32_array(smpu_node, "vio-info",
					 (unsigned int *)(mpu->vio_reg_info),
					 size);
	if (ret)
		return -ENXIO;
	//dump vio-info end
	size = of_property_count_elems_of_size(smpu_node, "mask", sizeof(char));
	if (size <= 0) {
		pr_info("No clear smpu\n");
		return -ENXIO;
	}
	mpu->mask_reg = devm_kmalloc(&pdev->dev, size, GFP_KERNEL);
	if (!(mpu->clear_reg))
		return -ENOMEM;

	mpu->mask_cnt = size / sizeof(struct smpu_reg_info_t);
	size >>= 2;
	ret = of_property_read_u32_array(smpu_node, "mask",
			(unsigned int *)(mpu->mask_reg), size);
	if (ret) {
		pr_info("No mask reg\n");
		return -ENXIO;
	}

	//only for smpu node
	if ((!(strcmp(mpu->name, "ssmpu"))) ||
	    (!(strcmp(mpu->name, "nsmpu")))) {
		//bypass_axi
		size = of_property_count_elems_of_size(smpu_node, "bypass-axi",
						       sizeof(char));
		miumpu_bypass_list = devm_kmalloc(&pdev->dev, size, GFP_KERNEL);
		if (!miumpu_bypass_list)
			return -ENOMEM;

		size /= sizeof(unsigned int);
		axi_set_num = AXI_SET_NUM(size);
		mpu->bypass_axi_num = axi_set_num;
		ret = of_property_read_u32_array(smpu_node, "bypass-axi",
						 miumpu_bypass_list, size);
		if (ret) {
			pr_info("No bypass miu mpu\n");
			return -ENXIO;
		}
		mpu->bypass_axi = devm_kmalloc(
			&pdev->dev,
			axi_set_num * sizeof(struct bypass_axi_info_t),
			GFP_KERNEL);
		if (!(mpu->bypass_axi))
			return -ENOMEM;

		for (i = 0; i < mpu->bypass_axi_num; i++) {
			mpu->bypass_axi[i].port =
				miumpu_bypass_list[(i * 3) + 0];
			mpu->bypass_axi[i].axi_mask =
				miumpu_bypass_list[(i * 3) + 1];
			mpu->bypass_axi[i].axi_value =
				miumpu_bypass_list[(i * 3) + 2];
		}

		//bypass_axi end
		//bypass miumpu start
		size = of_property_count_elems_of_size(smpu_node, "bypass",
						       sizeof(char));

		miumpu_bypass_list = devm_kmalloc(&pdev->dev, size, GFP_KERNEL);
		if (!miumpu_bypass_list)
			return -ENOMEM;

		size /= sizeof(unsigned int);
		mpu->bypass_miu_reg_num = size;
		ret = of_property_read_u32_array(smpu_node, "bypass",
						 miumpu_bypass_list, size);
		if (ret) {
			pr_info("No bypass miu mpu\n");
			return -ENXIO;
		}
		mpu->bypass_miu_reg = devm_kmalloc(
			&pdev->dev, size * sizeof(unsigned int), GFP_KERNEL);
		if (!(mpu->bypass_miu_reg))
			return -ENOMEM;

		for (i = 0; i < mpu->bypass_miu_reg_num; i++)
			mpu->bypass_miu_reg[i] = miumpu_bypass_list[i];

		size = of_property_count_elems_of_size(smpu_node, "bypass-gpu",
						       sizeof(char));
		if (size > 0) {
			gpu_bypass_list =
				devm_kmalloc(&pdev->dev, size, GFP_KERNEL);
			if (!gpu_bypass_list)
				return -ENOMEM;

			size >>= 2;
			ret = of_property_read_u32_array(
				smpu_node, "bypass-gpu", gpu_bypass_list, size);
			if (!gpu_bypass_list) {
				pr_info("no gpu-bypass\n");
				return -ENXIO;
			}

			mpu->gpu_bypass_list =
				devm_kmalloc(&pdev->dev,
					     size * sizeof(unsigned int),
					     GFP_KERNEL);

			if (!mpu->gpu_bypass_list)
				return -ENOMEM;

			for (i = 0; i < 2; i++)
				mpu->gpu_bypass_list[i] = gpu_bypass_list[i];
		}
		//bypass_miumpu end

	} //only for smpu end

	//reg base
	mpu->mpu_base = of_iomap(smpu_node, 0);
	if (IS_ERR(mpu->mpu_base)) {
		dev_err(&pdev->dev, "Failed to map smpu range base");
		return -EIO;
	}
	//reg base end

	mpu->vio_msg =
		devm_kmalloc(&pdev->dev, MTK_SMPU_MAX_CMD_LEN, GFP_KERNEL);
	if (!(mpu->vio_msg))
		return -ENOMEM;

	mpu->vio_msg_gpu =
		devm_kmalloc(&pdev->dev, MAX_GPU_VIO_LEN, GFP_KERNEL);
	if (!mpu->vio_msg_gpu)
		return -ENOMEM;

	//transt global

	if (!strcmp(mpu->name, "ssmpu"))
		global_ssmpu = mpu;
	if (!strcmp(mpu->name, "nsmpu"))
		global_nsmpu = mpu;
	if (!strcmp(mpu->name, "skp"))
		global_skp = mpu;
	if (!strcmp(mpu->name, "nkp"))
		global_nkp = mpu;

	if (of_property_read_bool(smpu_node, "mediatek,slc-b-mode"))
		mpu->slc_b_mode = true;

	smpu_clean_cpu_write_vio(mpu);

	mpu->irq = irq_of_parse_and_map(smpu_node, 0);
	if (mpu->irq == 0) {
		dev_err(&pdev->dev, "Failed to get irq resource\n");
		return -ENXIO;
	}

	ret = request_irq(mpu->irq, (irq_handler_t)smpu_violation,
			  IRQF_TRIGGER_NONE, "smpu", mpu);
	if (ret) {
		dev_err(&pdev->dev, "Failed to request irq");
		return -EINVAL;
	}

	devm_kfree(&pdev->dev, dump_list);

	return 0;
}
static int smpu_remove(struct platform_device *pdev)
{
	struct smpu *mpu = platform_get_drvdata(pdev);

	dev_info(&pdev->dev, "driver removed\n");

	free_irq(mpu->irq, mpu);

	if (!strcmp(mpu->name, "ssmpu"))
		global_ssmpu = NULL;
	else if (!strcmp(mpu->name, "nsmpu"))
		global_nsmpu = NULL;
	else if (!strcmp(mpu->name, "nkp"))
		global_skp = NULL;
	else if (!strcmp(mpu->name, "skp"))
		global_nkp = NULL;

	return 0;
}

static struct platform_driver smpu_driver = {
	.probe = smpu_probe,
	.remove = smpu_remove,
	.driver = {
		.name = "smpu_driver",
		.owner = THIS_MODULE,
		.of_match_table = smpu_of_ids,
	},
};

static __init int smpu_init(void)
{
	int ret;

	pr_info("smpu was loaded\n");

	ret = platform_driver_register(&smpu_driver);
	if (ret) {
		pr_info("smpu:failed to register driver");
		return ret;
	}

	return 0;
}

module_init(smpu_init);

MODULE_DESCRIPTION("MediaTek SMPU Driver");
MODULE_LICENSE("GPL");
