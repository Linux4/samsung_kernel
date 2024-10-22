// SPDX-License-Identifier: GPL-2.0

/*
 * Copyright (c) 2019 MediaTek Inc.
 */

#include <asm/cputype.h>
#include <linux/arm-smccc.h>
#include <linux/atomic.h>
#include <linux/bug.h>
#include <linux/delay.h>
#include <linux/device.h>
#include <linux/interrupt.h>
#include <linux/irq.h>
#include <linux/irqreturn.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/of_address.h>
#include <linux/of_device.h>
#include <linux/of_irq.h>
#include <linux/platform_device.h>
#include <linux/printk.h>
#include <linux/workqueue.h>
#include <linux/sched/clock.h>
#include <linux/soc/mediatek/mtk_sip_svc.h>
#include <mt-plat/aee.h>
#include "sda.h"
#include "dbg_error_flag.h"

struct dbg_error_flag_elem {
	void __iomem *base;
	unsigned int status0_offset;
	unsigned int status1_offset;
	unsigned int systimer_l_offset;
	unsigned int systimer_h_offset;
	unsigned int status_stage_offset;
	unsigned int error_mask;
	unsigned int irq;
	unsigned int err_flag_stage;
	unsigned int err_flag_status_1;
	unsigned int err_flag_status_2;
	unsigned int err_flag_systimer_l;
	unsigned int err_flag_systimer_h;
	struct work_struct wk;
};

struct dbg_error_flag {
	struct dbg_error_flag_elem *err_flag_str;
	unsigned int version;
	unsigned int nr_error_flag;
};

#if IS_ENABLED(CONFIG_MTK_AEE_FEATURE)
#define DEF_LOG(fmt, ...) \
	do { \
		pr_notice(fmt, __VA_ARGS__); \
		aee_sram_printk(fmt, __VA_ARGS__); \
	} while (0)
#else
#define DEF_LOG(fmt, ...)
#endif

static struct dbg_error_flag dbg_error_flag;
static DEFINE_SPINLOCK(dbg_error_flag_isr_lock);
static DEFINE_SPINLOCK(dbg_error_flag_wq_lock);

static BLOCKING_NOTIFIER_HEAD(dbg_error_flag_dump_list);

void dbg_error_flag_register_notify(struct notifier_block *nb)
{
	blocking_notifier_chain_register(&dbg_error_flag_dump_list, nb);
	pr_debug("%s register finished\n", __func__);
}
EXPORT_SYMBOL_GPL(dbg_error_flag_register_notify);

unsigned int get_dbg_error_flag_mask(unsigned int err_flag_enum)
{
	if (err_flag_enum >= DBG_ERROR_FLAG_TOTAL) {
		DEF_LOG("invalid err_flag_enum(%d)\n", err_flag_enum);
		return 0;
	}
	if (dbg_error_flag_desc[err_flag_enum].support == true)
		return dbg_error_flag_desc[err_flag_enum].mask;
	else
		return 0;
}
EXPORT_SYMBOL_GPL(get_dbg_error_flag_mask);

static void dbg_error_flag_irq_work(struct work_struct *w)
{
	unsigned int num_error_flag, i;
	unsigned int unmask_status = 0;
	struct arm_smccc_res res;

	num_error_flag = dbg_error_flag.nr_error_flag;

	for (i = 0; i < num_error_flag; i++) {
		if (dbg_error_flag.err_flag_str[i].err_flag_stage) {
			DEF_LOG("%s: err_flag stage is 0x%x.\n",
				__func__, dbg_error_flag.err_flag_str[i].err_flag_stage);
			DEF_LOG("%s err_flag systimer_L is 0x%x.\n",
				__func__, dbg_error_flag.err_flag_str[i].err_flag_systimer_l);
			DEF_LOG("%s err_flag systimer_H is 0x%x.\n",
				__func__, dbg_error_flag.err_flag_str[i].err_flag_systimer_h);
			DEF_LOG("%s: err_flag status1 is 0x%x.\n",
				__func__, dbg_error_flag.err_flag_str[i].err_flag_status_1);
			DEF_LOG("%s: err_flag status2 is 0x%x.\n",
				__func__, dbg_error_flag.err_flag_str[i].err_flag_status_2);

			/* per feature dump nofifier */
			if (dbg_error_flag.err_flag_str[i].err_flag_stage == 0x1) {
				blocking_notifier_call_chain(&dbg_error_flag_dump_list,
					dbg_error_flag.err_flag_str[i].err_flag_status_1, NULL);
				unmask_status = dbg_error_flag.err_flag_str[i].err_flag_status_1;
			} else if (dbg_error_flag.err_flag_str[i].err_flag_stage == 0x2 ||
					dbg_error_flag.err_flag_str[i].err_flag_stage == 0x3) {
				blocking_notifier_call_chain(&dbg_error_flag_dump_list,
					dbg_error_flag.err_flag_str[i].err_flag_status_2, NULL);
				unmask_status = dbg_error_flag.err_flag_str[i].err_flag_status_2;
			}

			/* clear status after WQ */
			dbg_error_flag.err_flag_str[i].err_flag_stage = 0;
			dbg_error_flag.err_flag_str[i].err_flag_status_1 = 0;
			dbg_error_flag.err_flag_str[i].err_flag_status_2 = 0;
			dbg_error_flag.err_flag_str[i].err_flag_systimer_l = 0;
			dbg_error_flag.err_flag_str[i].err_flag_systimer_h = 0;
		}
		enable_irq(dbg_error_flag.err_flag_str[i].irq);
	}

	/* unmask error flag */
	spin_lock(&dbg_error_flag_wq_lock);

	arm_smccc_smc(MTK_SIP_SDA_CONTROL, SDA_ERR_FLAG, DBG_ERR_FLAG_UNMASK, unmask_status,
			0, 0, 0, 0, &res);

	if (res.a0)
		pr_notice("%s: can't unmask error flag(0x%lx)\n",
			__func__, res.a0);

	spin_unlock(&dbg_error_flag_wq_lock);
}

static irqreturn_t dbg_error_flag_isr(int irq, void *dev_id)
{
	unsigned int status = 0;
	unsigned int num_error_flag, i;
	struct arm_smccc_res res;

	disable_irq_nosync(irq);

	num_error_flag = dbg_error_flag.nr_error_flag;

	for (i = 0; i < num_error_flag; i++) {
		if (dbg_error_flag.err_flag_str[i].base != NULL &&
				irq == dbg_error_flag.err_flag_str[i].irq) {
			dbg_error_flag.err_flag_str[i].err_flag_stage =
				readl(dbg_error_flag.err_flag_str[i].base +
					dbg_error_flag.err_flag_str[i].status_stage_offset);
			dbg_error_flag.err_flag_str[i].err_flag_status_1 =
				readl(dbg_error_flag.err_flag_str[i].base +
					dbg_error_flag.err_flag_str[i].status0_offset);
			dbg_error_flag.err_flag_str[i].err_flag_status_2 =
				readl(dbg_error_flag.err_flag_str[i].base +
					dbg_error_flag.err_flag_str[i].status1_offset);
			dbg_error_flag.err_flag_str[i].err_flag_systimer_l =
				readl(dbg_error_flag.err_flag_str[i].base +
					dbg_error_flag.err_flag_str[i].systimer_l_offset);
			dbg_error_flag.err_flag_str[i].err_flag_systimer_h =
				readl(dbg_error_flag.err_flag_str[i].base +
					dbg_error_flag.err_flag_str[i].systimer_h_offset);

			schedule_work(&dbg_error_flag.err_flag_str[i].wk);
			break;
		}
	}

	spin_lock(&dbg_error_flag_isr_lock);

	arm_smccc_smc(MTK_SIP_SDA_CONTROL, SDA_ERR_FLAG, DBG_ERR_FLAG_CLR, status,
			0, 0, 0, 0, &res);

	if (res.a0)
		pr_notice("%s: can't clear error flag(0x%lx)\n",
				__func__, res.a0);

	spin_unlock(&dbg_error_flag_isr_lock);

	return IRQ_HANDLED;
}

static int dbg_error_flag_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct device_node *node;
	struct platform_device *err_flag_pdev;
	size_t size;
	int ret, i, j;
	int err_flags_num;

	dev_info(dev, "driver probed\n");

	err_flags_num = of_count_phandle_with_args(
		pdev->dev.of_node, "mediatek,error-flag", NULL);

	dbg_error_flag.nr_error_flag = err_flags_num;

	size = sizeof(struct dbg_error_flag_elem) * err_flags_num;
	dbg_error_flag.err_flag_str = devm_kzalloc(dev, size, GFP_KERNEL);
	if (!dbg_error_flag.err_flag_str)
		return -ENOMEM;

	for (i = 0; i < err_flags_num; i++) {
		node = of_parse_phandle(pdev->dev.of_node, "mediatek,error-flag", i);
		if (!node) {
			dev_info(dev, "fail to parse mediatek,error-flag\n");
			return 0;
		}

		err_flag_pdev = of_find_device_by_node(node);
		if (WARN_ON(!err_flag_pdev)) {
			of_node_put(node);
			dev_info(dev, "no error flag for idx %d\n", i);
			return 0;
		}

		dbg_error_flag.err_flag_str[i].base = of_iomap(node, 0);
		if (!dbg_error_flag.err_flag_str->base) {
			dev_info(dev, "cant't map dbg error flag\n");
			return 0;
		}

		ret = of_property_read_u32_index(node, "status0-offset", 0,
				&dbg_error_flag.err_flag_str[i].status0_offset);
		if (ret) {
			dev_info(dev, "can't read status0-offset(%d)\n", ret);
			return ret;
		}
		dev_info(dev, "get status0-offset(0x%x)\n",
			dbg_error_flag.err_flag_str[i].status0_offset);

		ret = of_property_read_u32_index(node, "status1-offset", 0,
				&dbg_error_flag.err_flag_str[i].status1_offset);
		if (ret) {
			dev_info(dev, "can't read status1-offset(%d)\n", ret);
			return ret;
		}
		dev_info(dev, "get status1-offset(0x%x)\n",
			dbg_error_flag.err_flag_str[i].status1_offset);

		ret = of_property_read_u32_index(node, "error-stage-offset", 0,
				&dbg_error_flag.err_flag_str[i].status_stage_offset);
		if (ret) {
			dev_info(dev, "can't read error-stage offset(%d)\n", ret);
			return ret;
		}
		dev_info(dev, "get error-stage-offset(0x%x)\n",
			dbg_error_flag.err_flag_str[i].status_stage_offset);

		ret = of_property_read_u32_index(node, "systimer-l-offset", 0,
				&dbg_error_flag.err_flag_str[i].systimer_l_offset);
		if (ret) {
			dev_info(dev, "can't read systimer-l-offset(%d)\n", ret);
			return ret;
		}
		dev_info(dev, "get systimer-l-offset(0x%x)\n",
			dbg_error_flag.err_flag_str[i].systimer_l_offset);

		ret = of_property_read_u32_index(node, "systimer-h-offset", 0,
				&dbg_error_flag.err_flag_str[i].systimer_h_offset);
		if (ret) {
			dev_info(dev, "can't read systimer-h-offset(%d)\n", ret);
			return ret;
		}
		dev_info(dev, "get systimer-h-offset(0x%x)\n",
			dbg_error_flag.err_flag_str[i].systimer_h_offset);

		for (j = 0; j < DBG_ERROR_FLAG_TOTAL; j++) {
			ret = of_property_read_u32_index(node, dbg_error_flag_desc[j].mask_name, 0,
				&dbg_error_flag_desc[j].mask);
			if (!ret) {
				dbg_error_flag_desc[j].support = true;
				dev_info(dev, "get %s(mask = 0x%x)\n",
				dbg_error_flag_desc[j].mask_name, dbg_error_flag_desc[j].mask);
			}
		}

		dbg_error_flag.err_flag_str[i].irq = irq_of_parse_and_map(node, 0);
		if (!dbg_error_flag.err_flag_str->irq) {
			dev_info(dev, "can't map error flag irq\n");
			return -EINVAL;
		}

		/* init WQ for bottom half ISR */
		INIT_WORK(&dbg_error_flag.err_flag_str[i].wk, dbg_error_flag_irq_work);

		ret = devm_request_irq(dev, dbg_error_flag.err_flag_str[i].irq, dbg_error_flag_isr,
			IRQF_ONESHOT | IRQF_TRIGGER_NONE, "dbg_error_flag", NULL);
		if (ret) {
			dev_info(dev, "can't request error flag irq(%d)\n", ret);
			return ret;
		}
	}

	return 0;
}

static int dbg_error_flag_remove(struct platform_device *pdev)
{
	int i;

	dev_info(&pdev->dev, "driver removed\n");

	for (i = 0; i < dbg_error_flag.nr_error_flag; i++)
		flush_work(&dbg_error_flag.err_flag_str[i].wk);

	return 0;
}

static const struct of_device_id dbg_error_flag_of_ids[] = {
	{ .compatible = "mediatek,dbg-error-flag", },
	{}
};

static struct platform_driver dbg_error_flag_drv = {
	.driver = {
		.name = "dbg-error-flag",
		.owner = THIS_MODULE,
		.of_match_table = dbg_error_flag_of_ids,
	},
	.probe = dbg_error_flag_probe,
	.remove = dbg_error_flag_remove,
};

static int __init dbg_error_flag_init(void)
{
	int ret;

	ret = platform_driver_register(&dbg_error_flag_drv);
	if (ret)
		return ret;

	return 0;
}

static __exit void dbg_error_flag_exit(void)
{
	platform_driver_unregister(&dbg_error_flag_drv);
}

module_init(dbg_error_flag_init);
module_exit(dbg_error_flag_exit);

MODULE_DESCRIPTION("MediaTek Bus Parity Driver");
MODULE_LICENSE("GPL");
