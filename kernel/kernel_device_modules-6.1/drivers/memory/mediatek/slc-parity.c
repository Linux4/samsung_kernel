// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2023 MediaTek Inc.
 */
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
#include <mt-plat/aee.h>
#include <soc/mediatek/emi.h>
#include <linux/soc/mediatek/mtk_sip_svc.h>

#if IS_ENABLED(CONFIG_MTK_AEE_FEATURE)
#define ECC_LOG(fmt, ...) \
	aee_sram_printk(fmt, __VA_ARGS__)
#else
#define ECC_LOG(fmt, ...)
#endif

enum error_type {
	no_error = 0,
	correctable_error,
	uncorrectable_error
};

struct slc_parity {
	struct reg_info_t parity_err;
	struct reg_info_t parity_err_ext;
	struct reg_info_t parity_err_content;
	struct reg_info_t parity_err_status;

	struct reg_info_t clear_reg;

	unsigned int slc_parity_cnt;
	void __iomem **slc_parity_base;

	/* interrupt id */
	unsigned int irq;

	unsigned int port_num;
	unsigned int cs_num;
	unsigned int chn_num;
	int *error_position;

	/* debugging log for EMI MPU violation */
	char *vio_msg;

};

/* global pointer for exported functions */
static struct slc_parity *global_slc_parity;

static void clear_violation(
	struct slc_parity *slc, unsigned int emi_id)
{
	struct arm_smccc_res smc_res;

	arm_smccc_smc(MTK_SIP_EMIMPU_CONTROL, MTK_SLC_PARITY_CLEAR,
			emi_id, 0, 0, 0, 0, 0, &smc_res);
	if (smc_res.a0) {
		pr_info("%s:%d failed to clear slc parity, ret=0x%lx\n",
			__func__, __LINE__, smc_res.a0);
	}
}

static void slc_parity_vio_dump(struct work_struct *work)
{
	struct slc_parity *slc;

	slc = global_slc_parity;
	if (!slc)
		return;

	if (slc->vio_msg)
		aee_kernel_exception("SLC_PARITY", slc->vio_msg);
}
static DECLARE_WORK(slc_parity_work, slc_parity_vio_dump);

static void read_parity_content(void __iomem *reg_base, char *vio_msg, ssize_t *msg_len, unsigned int chn_cs_idx)
{
	unsigned int err_content_num = 10;
	unsigned int m;
	unsigned int content;

	m = 0;
	content = readl( reg_base + ((chn_cs_idx * err_content_num + m)<<2) );
	pr_info("%s: %x\n", "address", content);
}

static enum error_type read_parity_status(void __iomem *reg_base, char *vio_msg, ssize_t *msg_len
						, unsigned int chn_cs_idx)
{
	int n;
	unsigned int m;
	enum error_type partial_error_type = no_error;
	enum error_type total_error_type = no_error;
	unsigned int content;
	unsigned int error_bit = 0;
	char buffer[100];
	unsigned int buffer_len = 0;

	content = readl( reg_base + (chn_cs_idx<<2) );

	for (m = 0; m<2; m++) {
		if (((content & 0x3) == 0x1) || ((content & 0x3) == 0x2)) {
			partial_error_type = correctable_error;
			error_bit += 1;
		} else if ((content & 0x3) == 0x3) {
			partial_error_type = uncorrectable_error;
			error_bit += 2;
		} else {
			partial_error_type = no_error;
		}
		n = snprintf(buffer+buffer_len, sizeof(buffer)-buffer_len, "bit %u~%u: %d bit error, ", m*128,
			(m+1)*128-1, partial_error_type);
		buffer_len += (buffer_len+n < sizeof(buffer)) ? (ssize_t)n : sizeof(buffer)-buffer_len;

		content >>=2;
	}
	if (error_bit > 1) {
		pr_info("%s", buffer);
		total_error_type = uncorrectable_error;
	} else
		total_error_type = correctable_error;

	return total_error_type;
}

static void get_ecc_sram(struct slc_parity *slc, unsigned long long parity_err_tol)
{
	unsigned int chn_idx = 0, port_idx = 0, cs_idx = 0;
	int total_idx = 0;

	for (chn_idx = 0; chn_idx < slc->chn_num; chn_idx++) {
		for (port_idx = 0; port_idx < slc->port_num; port_idx++) {
			for (cs_idx = 0; cs_idx < slc->cs_num; cs_idx++) {
				total_idx = port_idx * slc->chn_num * slc->cs_num + chn_idx * slc->cs_num + cs_idx;
				if ((parity_err_tol & 0x1) == 0x0)
					slc->error_position[total_idx] = 0;
				else
					slc->error_position[total_idx] = 1;
				parity_err_tol >>= 1;
			}
		}
	}
}

static enum error_type get_ecc_info(struct slc_parity *slc, void __iomem *slc_parity_base, unsigned int emi_id
	, ssize_t *msg_len)
{
	unsigned int chn_idx = 0, port_idx = 0, cs_idx = 0, total_idx = 0;
	struct arm_smccc_res smc_res;
	unsigned int chn_cs_idx;
	enum error_type partial_error_type = no_error;
	enum error_type total_error_type = no_error;
	int n;

	for (port_idx = 0; port_idx < slc->port_num; port_idx++) {
		arm_smccc_smc(MTK_SIP_EMIMPU_CONTROL, MTK_SLC_PARITY_SELECT,
				emi_id, port_idx, 0, 0, 0, 0, &smc_res);
		if (smc_res.a0) {
			pr_info("%s:%d failed to clear slc parity, ret=0x%lx\n",
				__func__, __LINE__, smc_res.a0);
		}
		for (chn_idx = 0; chn_idx < slc->chn_num; chn_idx++) {
			for (cs_idx = 0; cs_idx < slc->cs_num; cs_idx++) {
				total_idx = port_idx * slc->chn_num * slc->cs_num + chn_idx * slc->cs_num + cs_idx;
				if (slc->error_position[total_idx] == 0)
					continue;

				chn_cs_idx = chn_idx * slc->cs_num + cs_idx;
				partial_error_type = read_parity_status(slc_parity_base
					+ slc->parity_err_status.offset, slc->vio_msg, msg_len, chn_cs_idx);

				n = snprintf(slc->vio_msg + *msg_len,
					MTK_SLC_PARITY_MAX_LEN - *msg_len,
					"%u,%u,%u:%u\n", port_idx, chn_idx, cs_idx, partial_error_type);
				*msg_len += (*msg_len+n < MTK_SLC_PARITY_MAX_LEN) ?
					(ssize_t)n : MTK_SLC_PARITY_MAX_LEN-*msg_len;

				if (partial_error_type == uncorrectable_error) {
					pr_info("chn_idx: %u, port_idx: %u, cs_idx: %u\n",
						chn_idx, port_idx, cs_idx);

					read_parity_content(slc_parity_base
						+ slc->parity_err_content.offset, slc->vio_msg, msg_len, chn_cs_idx);
				}

				if (total_error_type < partial_error_type)
					total_error_type = partial_error_type;
			}
		}
	}
	return total_error_type;
}

static irqreturn_t slc_parity_violation_irq(int irq, void *dev_id)
{
	struct slc_parity *slc = (struct slc_parity *)dev_id;
	void __iomem *slc_parity_base;
	unsigned int emi_id;
	ssize_t msg_len = 0;
	int n;
	unsigned long long parity_err_tol = 0;
	enum error_type partial_error_type = no_error;
	enum error_type total_error_type = no_error;


	for (emi_id = 0; emi_id < slc->slc_parity_cnt; emi_id++) {
		slc_parity_base = slc->slc_parity_base[emi_id];
		slc->parity_err.value = readl(slc_parity_base + slc->parity_err.offset);
		slc->parity_err_ext.value = readl(slc_parity_base + slc->parity_err_ext.offset);

		if (slc->parity_err.value > 0 || slc->parity_err_ext.value > 0) {
			n = snprintf(slc->vio_msg + msg_len,
				MTK_SLC_PARITY_MAX_LEN - msg_len,
				"\n%u:%x,%x\n", emi_id, slc->parity_err.value, slc->parity_err_ext.value);
			msg_len += (msg_len+n < MTK_SLC_PARITY_MAX_LEN) ? (ssize_t)n : MTK_SLC_PARITY_MAX_LEN-msg_len;

			pr_info("emi_id %u\n", emi_id);
			pr_info("%x, %x\n",
				slc->parity_err.offset, slc->parity_err.value);
			pr_info("%x, %x\n",
				slc->parity_err_ext.offset, slc->parity_err_ext.value);

			parity_err_tol = slc->parity_err.value + ((unsigned long long)slc->parity_err_ext.value << 32);

			get_ecc_sram(slc, parity_err_tol);
			partial_error_type = get_ecc_info(slc, slc_parity_base, emi_id, &msg_len);
			if (total_error_type < partial_error_type)
				total_error_type = partial_error_type;
		}
		clear_violation(slc, emi_id);
	}

	if (msg_len) {
		ECC_LOG("%s\n", slc->vio_msg);
		pr_info("error type: %d bit error\n", total_error_type);
		if (total_error_type == uncorrectable_error)
			schedule_work(&slc_parity_work);
	}

	return IRQ_HANDLED;
}

static const struct of_device_id slc_parity_of_ids[] = {
	{.compatible = "mediatek,common-slc-parity",},
	{}
};
MODULE_DEVICE_TABLE(of, slc_parity_of_ids);

static int slc_parity_probe(struct platform_device *pdev)
{
	struct device_node *slc_parity_node = pdev->dev.of_node;
	struct slc_parity *slc;
	struct resource *res;
	int ret, size, i;
	unsigned int *dump_list;



	dev_info(&pdev->dev, "driver probed\n");


//slc init
	slc = devm_kzalloc(&pdev->dev,
		sizeof(struct slc_parity), GFP_KERNEL);
	if (!slc)
		return -ENOMEM;

	ret = of_property_count_elems_of_size(slc_parity_node,
		"reg", sizeof(unsigned int) * 4);
	if (ret <= 0) {
		pr_info("No reg\n");
		return -ENXIO;
	}
	slc->slc_parity_cnt = (unsigned int)ret;

	slc->slc_parity_base = devm_kmalloc_array(&pdev->dev,
		slc->slc_parity_cnt, sizeof(phys_addr_t), GFP_KERNEL);
	if (!slc->slc_parity_base)
		return -ENOMEM;

	for (i = 0; i < slc->slc_parity_cnt; i++) {
		res = platform_get_resource(pdev, IORESOURCE_MEM, i);
		slc->slc_parity_base[i] = devm_ioremap(&pdev->dev, res->start, resource_size(res));
		if (IS_ERR(slc->slc_parity_base[i])) {
			pr_info("Failed to map EMI%d SLC base\n", i);
			return -EIO;
		}
	}

	slc->vio_msg = devm_kmalloc(&pdev->dev,
		MTK_SLC_PARITY_MAX_LEN, GFP_KERNEL);
	if (!(slc->vio_msg))
		return -ENOMEM;

	global_slc_parity = slc;
	platform_set_drvdata(pdev, slc);
//slc end


//dump
	size = of_property_count_elems_of_size(slc_parity_node,
		"dump", sizeof(char));
	if (size <= 0) {
		pr_info("No dump\n");
		return -ENXIO;
	}

	dump_list = devm_kmalloc(&pdev->dev, size, GFP_KERNEL);
	if (!dump_list)
		return -ENOMEM;
	size >>= 2;
	ret = of_property_read_u32_array(slc_parity_node, "dump",
		dump_list, size);
	if (ret) {
		pr_info("No dump\n");
		return -ENXIO;
	}
	slc->parity_err.offset = dump_list[0];
	slc->parity_err_ext.offset = dump_list[1];
	slc->parity_err_content.offset = dump_list[2];
	slc->parity_err_status.offset = dump_list[3];
//dump end

	ret = of_property_read_u32(slc_parity_node, "port-num",
		&(slc->port_num));
	if (ret) {
		pr_info("No port-num\n");
		return -ENXIO;
	}

	ret = of_property_read_u32(slc_parity_node, "cs-num",
		&(slc->cs_num));
	if (ret) {
		pr_info("No cs-num\n");
		return -ENXIO;
	}

	ret = of_property_read_u32(slc_parity_node, "chn-num",
		&(slc->chn_num));
	if (ret) {
		pr_info("No chn-num\n");
		return -ENXIO;
	}

	slc->error_position = devm_kzalloc(&pdev->dev,
		sizeof(int) * slc->port_num * slc->cs_num * slc->chn_num, GFP_KERNEL);
	if (!slc->error_position)
		return -ENOMEM;

//clear
	ret = of_property_read_u32(slc_parity_node, "clear",
		&(slc->clear_reg.offset));
	if (ret) {
		pr_info("No clear\n");
		return -ENXIO;
	}
//clear end


//irq
	slc->irq = irq_of_parse_and_map(slc_parity_node, 0);
	if (slc->irq == 0) {
		pr_info("Failed to get irq resource\n");
		return -ENXIO;
	}
	ret = request_threaded_irq(slc->irq, NULL,
		(irq_handler_t)slc_parity_violation_irq,
		IRQF_TRIGGER_NONE | IRQF_ONESHOT, "slc-parity", slc);
	if (ret) {
		pr_info("Failed to request irq\n");
		return -EINVAL;
	}
//irq end

	return 0;
}

static int slc_parity_remove(struct platform_device *pdev)
{
	struct slc_parity *slc = platform_get_drvdata(pdev);

	dev_info(&pdev->dev, "driver removed\n");

	free_irq(slc->irq, slc);

	flush_work(&slc_parity_work);

	global_slc_parity = NULL;

	return 0;
}

static struct platform_driver slc_parity_driver = {
	.probe = slc_parity_probe,
	.remove = slc_parity_remove,
	.driver = {
		.name = "slc_parity_driver",
		.owner = THIS_MODULE,
		.of_match_table = slc_parity_of_ids,
	},
};

static __init int slc_parity_init(void)
{
	int ret;

	pr_info("slc parity was loaded\n");

	ret = platform_driver_register(&slc_parity_driver);
	if (ret) {
		pr_info("slc parity: failed to register driver\n");
		return ret;
	}

	return 0;
}

module_init(slc_parity_init);

MODULE_DESCRIPTION("MediaTek EMI SLC PARITY Driver");
MODULE_LICENSE("GPL");
