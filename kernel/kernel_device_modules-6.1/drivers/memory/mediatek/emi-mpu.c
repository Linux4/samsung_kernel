// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

#include <emi_mpu.h>
#include <linux/arm-smccc.h>
#include <linux/device.h>
#include <linux/interrupt.h>
#include <linux/io.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/of_irq.h>
#include <linux/platform_device.h>
#include <linux/printk.h>
#include <linux/slab.h>
#include <linux/soc/mediatek/mtk_sip_svc.h>
#include <mt-plat/aee.h>
#include <soc/mediatek/emi.h>

/* global pointer for exported functions */
struct emi_mpu *global_emi_mpu;
EXPORT_SYMBOL_GPL(global_emi_mpu);
unsigned int smc_clear;

static void set_regs(
	struct reg_info_t *reg_list, unsigned int reg_cnt,
	void __iomem *emi_cen_base)
{
	unsigned int i, j;

	for (i = 0; i < reg_cnt; i++)
		for (j = 0; j < reg_list[i].leng; j++)
			writel(reg_list[i].value, emi_cen_base +
				reg_list[i].offset + 4 * j);

	/*
	 * Use the memory barrier to make sure the interrupt signal is
	 * de-asserted (by programming registers) before exiting the
	 * ISR and re-enabling the interrupt.
	 */
	mb();
}

static void clear_violation(
	struct emi_mpu *mpu, unsigned int emi_id)
{
	void __iomem *emi_cen_base;
	struct arm_smccc_res smc_res;

	emi_cen_base = mpu->emi_cen_base[emi_id];

	if (smc_clear) {
		arm_smccc_smc(MTK_SIP_EMIMPU_CONTROL, MTK_EMI_CLEAR,
				emi_id, 0, 0, 0, 0, 0, &smc_res);
		if (smc_res.a0) {
			pr_info("%s:%d failed to clear emi violation, ret=0x%lx\n",
				__func__, __LINE__, smc_res.a0);
		}
	} else {
		set_regs(mpu->clear_reg,
			mpu->clear_reg_cnt, emi_cen_base);
	}
}

static void emimpu_vio_dump(struct work_struct *work)
{
	struct emi_mpu *mpu;
	struct emimpu_dbg_cb *curr_dbg_cb;

	mpu = global_emi_mpu;
	if (!mpu)
		return;

	for (curr_dbg_cb = mpu->dbg_cb_list; curr_dbg_cb;
		curr_dbg_cb = curr_dbg_cb->next_dbg_cb)
		curr_dbg_cb->func();

	if (mpu->vio_msg)
		aee_kernel_exception("EMIMPU", mpu->vio_msg);

	mpu->in_msg_dump = 0;
}
static DECLARE_WORK(emimpu_work, emimpu_vio_dump);

static irqreturn_t emimpu_violation_irq(int irq, void *dev_id)
{
	struct emi_mpu *mpu = (struct emi_mpu *)dev_id;
	struct reg_info_t *dump_reg = mpu->dump_reg;
	void __iomem *emi_cen_base;
	unsigned int emi_id, i;
	ssize_t msg_len;
	int n, nr_vio;
	bool violation;
	char md_str[MTK_EMI_MAX_CMD_LEN + 10] = {'\0'};
	const unsigned int hp_mask = 0x600000;

	if (mpu->in_msg_dump)
		goto ignore_violation;

	n = snprintf(mpu->vio_msg, MTK_EMI_MAX_CMD_LEN, "violation\n");
	msg_len = (n < 0) ? 0 : (ssize_t)n;

	nr_vio = 0;
	for (emi_id = 0; emi_id < mpu->emi_cen_cnt; emi_id++) {
		violation = false;
		emi_cen_base = mpu->emi_cen_base[emi_id];

		for (i = 0; i < mpu->dump_cnt; i++) {
			dump_reg[i].value = readl(
				emi_cen_base + dump_reg[i].offset);

			if (msg_len < MTK_EMI_MAX_CMD_LEN) {
				n = snprintf(mpu->vio_msg + msg_len,
					MTK_EMI_MAX_CMD_LEN - msg_len,
					"%s(%d),%s(%x),%s(%x);\n",
					"emi", emi_id,
					"off", dump_reg[i].offset,
					"val", dump_reg[i].value);
				msg_len += (n < 0) ? 0 : (ssize_t)n;
			}

			if (dump_reg[i].value)
				violation = true;
		}

		if (dump_reg[2].value & hp_mask)
			violation = false;

		if (!violation)
			continue;

		nr_vio++;

		/*
		 * Whenever there is an EMI MPU violation, the Modem
		 * software would like to be notified immediately.
		 * This is because the Modem software wants to do
		 * its coredump as earlier as possible for debugging
		 * and analysis.
		 * (Even if the violated master is not Modem, it
		 *  may still need coredump for clarification.)
		 * Have a hook function in the EMI MPU ISR for this
		 * purpose.
		 */
		if (mpu->md_handler) {
			strncpy(md_str, "emi-mpu.c", 10);
			strncat(md_str, mpu->vio_msg, sizeof(md_str) - strlen(md_str) - 1);
			mpu->md_handler(md_str);
		}
	}

	if (nr_vio) {
		pr_info("%s: %s", __func__, mpu->vio_msg);
		mpu->in_msg_dump = 1;
		schedule_work(&emimpu_work);
	}

ignore_violation:
	for (emi_id = 0; emi_id < mpu->emi_cen_cnt; emi_id++)
		clear_violation(mpu, emi_id);

	return IRQ_HANDLED;
}

/*
 * mtk_emimpu_md_handling_register - register callback for md handling
 * @md_handling_func:	function point for md handling
 *
 * Return 0 for success, -EINVAL for fail
 */
int mtk_emimpu_md_handling_register(emimpu_md_handler md_handling_func)
{
	struct emi_mpu *mpu;

	mpu = global_emi_mpu;
	if (!mpu)
		return -EINVAL;

	if (!md_handling_func) {
		pr_info("%s: md_handling_func is NULL\n", __func__);
		return -EINVAL;
	}

	mpu->md_handler = md_handling_func;

	pr_info("%s: md_handling_func registered!!\n", __func__);

	return 0;
}
EXPORT_SYMBOL(mtk_emimpu_md_handling_register);

/*
 * mtk_clear_md_violation - clear irq for md violation
 *
 * No return
 */
void mtk_clear_md_violation(void)
{
	struct emi_mpu *mpu;
	void __iomem *emi_cen_base;
	struct arm_smccc_res smc_res;
	unsigned int emi_id;

	mpu = global_emi_mpu;
	if (!mpu)
		return;

	for (emi_id = 0; emi_id < mpu->emi_cen_cnt; emi_id++) {
		emi_cen_base = mpu->emi_cen_base[emi_id];
		if (smc_clear) {
			arm_smccc_smc(MTK_SIP_EMIMPU_CONTROL, MTK_EMI_CLEAR,
					emi_id, 1, 0, 0, 0, 0, &smc_res);
			if (smc_res.a0) {
				pr_info("%s:%d failed to clear md violation, ret=0x%lx\n",
					__func__, __LINE__, smc_res.a0);
			}
		} else {
			set_regs(mpu->clear_md_reg,
				mpu->clear_md_reg_cnt, emi_cen_base);
		}
	}

	pr_info("%s:version %d\n", __func__, mpu->version);
}
EXPORT_SYMBOL(mtk_clear_md_violation);

/*
 * mtk_clear_smpu_log - clear smpu log in emi
 *
 * Return 0 for no cpu prefetch, 1 for cpu prefetch, 2 for not smpu log
 */
int mtk_clear_smpu_log(unsigned int emi_id)
{
	struct emi_mpu *mpu;
	void __iomem *emi_cen_base;
	unsigned int prefetch_flag = 0;
	struct reg_info_t *dump_reg;
	const unsigned int hp_mask = 0x600000;
	const unsigned int prefetch_mask = 0x10000000;
	bool smpu_vio, cpu_prefetch;

	mpu = global_emi_mpu;
	if (!mpu)
		return -EINVAL;

	dump_reg = mpu->dump_reg;

	emi_cen_base = mpu->emi_cen_base[emi_id];

	dump_reg[2].value = readl(
		emi_cen_base + dump_reg[2].offset);

	smpu_vio = (dump_reg[2].value & hp_mask) ? true : false;
	cpu_prefetch = (dump_reg[2].value & prefetch_mask) ? true : false;
	if (smpu_vio) {
		if (!cpu_prefetch)
			prefetch_flag = 0;
		else
			prefetch_flag = 1;
	} else {
		prefetch_flag = 2;
	}

	if (prefetch_flag != 2)
		clear_violation(mpu, emi_id);

	return prefetch_flag;
}
EXPORT_SYMBOL(mtk_clear_smpu_log);

static const struct of_device_id emimpu_of_ids[] = {
	{.compatible = "mediatek,common-emimpu",},
	{}
};
MODULE_DEVICE_TABLE(of, emimpu_of_ids);

static int emimpu_probe(struct platform_device *pdev)
{
	struct device_node *emimpu_node = pdev->dev.of_node;
	struct device_node *emicen_node =
		of_parse_phandle(emimpu_node, "mediatek,emi-reg", 0);
	struct emi_mpu *mpu;
	int ret, size, i;
	struct resource *res;
	unsigned int *dump_list;

	dev_info(&pdev->dev, "driver probed\n");

	if (!emicen_node) {
		dev_err(&pdev->dev, "No emi-reg\n");
		return -ENXIO;
	}

	mpu = devm_kzalloc(&pdev->dev,
		sizeof(struct emi_mpu), GFP_KERNEL);
	if (!mpu)
		return -ENOMEM;

	smc_clear = 0;
	ret = of_property_read_u32(emimpu_node,
		"smc-clear", &smc_clear);
	if (!ret)
		dev_info(&pdev->dev, "Use smc to clear vio\n");

	size = of_property_count_elems_of_size(emimpu_node,
		"dump", sizeof(char));
	if (size <= 0) {
		dev_err(&pdev->dev, "No dump\n");
		return -ENXIO;
	}
	dump_list = devm_kmalloc(&pdev->dev, size, GFP_KERNEL);
	if (!dump_list)
		return -ENOMEM;
	size >>= 2;
	mpu->dump_cnt = size;
	ret = of_property_read_u32_array(emimpu_node, "dump",
		dump_list, size);
	if (ret) {
		dev_err(&pdev->dev, "No dump\n");
		return -ENXIO;
	}
	mpu->dump_reg = devm_kmalloc(&pdev->dev,
		size * sizeof(struct reg_info_t), GFP_KERNEL);
	if (!(mpu->dump_reg))
		return -ENOMEM;
	for (i = 0; i < mpu->dump_cnt; i++) {
		mpu->dump_reg[i].offset = dump_list[i];
		mpu->dump_reg[i].value = 0;
		mpu->dump_reg[i].leng = 0;
	}

	size = of_property_count_elems_of_size(emimpu_node,
		"clear", sizeof(char));
	if (size <= 0) {
		dev_err(&pdev->dev, "No clear\n");
		return  -ENXIO;
	}
	mpu->clear_reg = devm_kmalloc(&pdev->dev,
		size, GFP_KERNEL);
	if (!(mpu->clear_reg))
		return -ENOMEM;
	mpu->clear_reg_cnt = size / sizeof(struct reg_info_t);
	size >>= 2;
	ret = of_property_read_u32_array(emimpu_node, "clear",
		(unsigned int *)(mpu->clear_reg), size);
	if (ret) {
		dev_err(&pdev->dev, "No clear\n");
		return -ENXIO;
	}

	size = of_property_count_elems_of_size(emimpu_node,
		"clear-md", sizeof(char));
	if (size <= 0) {
		dev_err(&pdev->dev, "No clear-md\n");
		return -ENXIO;
	}
	mpu->clear_md_reg = devm_kmalloc(&pdev->dev,
		size, GFP_KERNEL);
	if (!(mpu->clear_md_reg))
		return -ENOMEM;
	mpu->clear_md_reg_cnt = size / sizeof(struct reg_info_t);
	size >>= 2;
	ret = of_property_read_u32_array(emimpu_node, "clear-md",
		(unsigned int *)(mpu->clear_md_reg), size);
	if (ret) {
		dev_err(&pdev->dev, "No clear-md\n");
		return -ENXIO;
	}

	mpu->emi_cen_cnt = of_property_count_elems_of_size(
			emicen_node, "reg", sizeof(unsigned int) * 4);
	if (mpu->emi_cen_cnt <= 0) {
		dev_err(&pdev->dev, "No reg\n");
		return -ENXIO;
	}

	mpu->emi_cen_base = devm_kmalloc_array(&pdev->dev,
		mpu->emi_cen_cnt, sizeof(phys_addr_t), GFP_KERNEL);
	if (!(mpu->emi_cen_base))
		return -ENOMEM;

	mpu->emi_mpu_base = devm_kmalloc_array(&pdev->dev,
		mpu->emi_cen_cnt, sizeof(phys_addr_t), GFP_KERNEL);
	if (!(mpu->emi_mpu_base))
		return -ENOMEM;

	for (i = 0; i < mpu->emi_cen_cnt; i++) {
		mpu->emi_cen_base[i] = of_iomap(emicen_node, i);
		if (IS_ERR(mpu->emi_cen_base[i])) {
			dev_err(&pdev->dev, "Failed to map EMI%d CEN base\n",
				i);
			return -EIO;
		}

		res = platform_get_resource(pdev, IORESOURCE_MEM, i);
		mpu->emi_mpu_base[i] =
			devm_ioremap_resource(&pdev->dev, res);
		if (IS_ERR(mpu->emi_mpu_base[i])) {
			dev_err(&pdev->dev, "Failed to map EMI%d MPU base\n",
				i);
			return -EIO;
		}
	}

	mpu->vio_msg = devm_kmalloc(&pdev->dev,
		MTK_EMI_MAX_CMD_LEN, GFP_KERNEL);
	if (!(mpu->vio_msg))
		return -ENOMEM;

	global_emi_mpu = mpu;
	platform_set_drvdata(pdev, mpu);

	mpu->irq = irq_of_parse_and_map(emimpu_node, 0);
	if (mpu->irq == 0) {
		dev_err(&pdev->dev, "Failed to get irq resource\n");
		ret = -ENXIO;
	}
	ret = request_irq(mpu->irq, (irq_handler_t)emimpu_violation_irq,
		IRQF_TRIGGER_NONE, "emimpu", mpu);
	if (ret) {
		dev_err(&pdev->dev, "Failed to request irq");
		ret = -EINVAL;
	}

	mpu->version = EMIMPUVER1;

	devm_kfree(&pdev->dev, dump_list);

	return 0;
}

static int emimpu_remove(struct platform_device *pdev)
{
	struct emi_mpu *mpu = platform_get_drvdata(pdev);

	dev_info(&pdev->dev, "driver removed\n");

	free_irq(mpu->irq, mpu);

	flush_work(&emimpu_work);

	global_emi_mpu = NULL;

	return 0;
}

static struct platform_driver emimpu_driver = {
	.probe = emimpu_probe,
	.remove = emimpu_remove,
	.driver = {
		.name = "emimpu_driver",
		.owner = THIS_MODULE,
		.of_match_table = emimpu_of_ids,
	},
};

static __init int emimpu_init(void)
{
	int ret;

	pr_info("emimpu was loaded\n");

	ret = platform_driver_register(&emimpu_driver);
	if (ret) {
		pr_err("emimpu: failed to register driver\n");
		return ret;
	}

	return 0;
}

module_init(emimpu_init);

MODULE_DESCRIPTION("MediaTek EMI MPU Driver");
MODULE_LICENSE("GPL v2");
