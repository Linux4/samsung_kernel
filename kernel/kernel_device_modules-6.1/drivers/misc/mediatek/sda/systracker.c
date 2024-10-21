// SPDX-License-Identifier: GPL-2.0

/*
 * Copyright (c) 2021 MediaTek Inc.
 */

#include <asm/cputype.h>
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
#include <linux/fs.h>
#include <linux/slab.h>
#include <linux/string.h>
#include <linux/sched/clock.h>
#include <linux/soc/mediatek/mtk_sip_svc.h>
#include <linux/arm-smccc.h>
#include <linux/unistd.h>
#include <mt-plat/aee.h>
#include "sda.h"

/* bits in BUS_DBG_CON, show the irq flags */
#define AR_1ST_TIMEOUT_IRQ_FLAG     (1 << 8)
#define AW_1ST_TIMEOUT_IRQ_FLAG     (1 << 9)
#define AW_WP_HIT_IRQ_FLAG          (1 << 10)
#define AR_WP_HIT_IRQ_FLAG          (1 << 11)
#define AR_SLV_ERR_IRQ_FLAG         (1 << 12)
#define AW_SLV_ERR_IRQ_FLAG         (1 << 13)
#define IRQ_FLAG_MASK_SHIFT         (8)
#define IRQ_FLAG_MASK               (AR_1ST_TIMEOUT_IRQ_FLAG | AW_1ST_TIMEOUT_IRQ_FLAG | \
					AW_WP_HIT_IRQ_FLAG | AR_WP_HIT_IRQ_FLAG | \
					AR_SLV_ERR_IRQ_FLAG | AW_SLV_ERR_IRQ_FLAG)

/* systracker registers offset */
#define BUS_DBG_CON         0x0
#define SECURE_CMD_MASK     0x3
#define WP_REGS_SIZE        0x100

#define MAX_ENTRY_NUM       (5) /* the maximum number to enter isr */
#define NAME_MAX_LEN        (32)

#define TRACKER_COMPATIBLE  "mediatek,tracker"
#define SUBSYS_NODE_NAME    "subsys"

#define DRIVER_ATTR(_name, _mode, _show, _store) \
	struct driver_attribute driver_attr_##_name = \
	__ATTR(_name, _mode, _show, _store)

enum TRACKER_INST {
	AP_TRACKER_INST,
	INFRA_TRACKER_INST,
	VLP_TRACKER_INST,
	NR_TRACKER_INST,
};

enum TRACKER_IRQ_EN_OPS {
	IRQ_EN_OPS_NONE		= 0x0,
	IRQ_EN_OPS_AR_WP	= 0x1 << 0,
	IRQ_EN_OPS_AW_WP	= 0x1 << 1,
	IRQ_EN_OPS_AR_1ST_TMO	= 0x1 << 2,
	IRQ_EN_OPS_AW_1ST_TMO	= 0x1 << 3,
	IRQ_EN_OPS_AR_SLV_ERR	= 0x1 << 4,
	IRQ_EN_OPS_AW_SLV_ERR	= 0x1 << 5,
	IRQ_EN_OPS_WP		= IRQ_EN_OPS_AR_WP | IRQ_EN_OPS_AW_WP,
	IRQ_EN_OPS_TMO		= IRQ_EN_OPS_AR_1ST_TMO | IRQ_EN_OPS_AW_1ST_TMO,
	IRQ_EN_OPS_SLE		= IRQ_EN_OPS_AR_SLV_ERR | IRQ_EN_OPS_AW_SLV_ERR,
	IRQ_EN_OPS_ALL		= IRQ_EN_OPS_AR_WP | IRQ_EN_OPS_AW_WP |
				  IRQ_EN_OPS_AR_1ST_TMO | IRQ_EN_OPS_AW_1ST_TMO,
};

struct tracker_wp_inst {
	char name[NAME_MAX_LEN];
	char irq_name[NAME_MAX_LEN];
	unsigned int irq;
	unsigned int enter_count;
	unsigned int irq_en_ops;
	unsigned int security_option;
	unsigned long long monitored_address;
	unsigned long long address_mask;
	unsigned int flags[MAX_ENTRY_NUM];
	unsigned long long ts[MAX_ENTRY_NUM];
	void __iomem *tracker_base;
	struct tasklet_struct tasklet;
};

struct tracker_wp_info {
	bool is_sysfs_created;
	unsigned int nr_wp_inst;
	struct tracker_wp_inst *wp_inst[NR_TRACKER_INST];
};

static struct tracker_wp_info tracker_wp;

static void bus_dbg_tracker_bh(struct tasklet_struct *tsklet)
{
	unsigned int id;
	struct tracker_wp_inst *wp;
	struct arm_smccc_res res;

	for (id = 0; id < tracker_wp.nr_wp_inst; id++) {
		wp = tracker_wp.wp_inst[id];
		if (tsklet != &wp->tasklet)
			continue;

		pr_notice("[%s]: %s tracker irq triggered for %dth time, kernel_time = %llu ns.\n",
			__func__, wp->name, wp->enter_count + 1, wp->ts[wp->enter_count]);
		pr_notice("[%s]: irq_flag is 0x%x.\n", __func__, wp->flags[wp->enter_count]);

		wp->enter_count++;

		if (wp->enter_count < MAX_ENTRY_NUM) {
			enable_irq(wp->irq);
		} else {
			arm_smccc_smc(MTK_SIP_SDA_CONTROL, SDA_SYSTRACKER, TRACKER_IRQ_SWITCH, id,
				IRQ_EN_OPS_NONE, 0, 0, 0, &res);
			if (res.a0)
				pr_notice("%s: disable %s tracker irq failed.\n",
					__func__, wp->name);
			pr_notice("[%s]: %s tracker irq was disabled because more than %d times.\n",
				__func__, wp->name, MAX_ENTRY_NUM);
		}
	}
}

static irqreturn_t bus_dbg_tracker_isr(int irq, void *dev_id)
{
	int id;
	const char *tracker_name = dev_id;
	struct tracker_wp_inst *wp = NULL;
	struct arm_smccc_res res;

	disable_irq_nosync(irq);

	for (id = 0; id < tracker_wp.nr_wp_inst; id++) {
		if (!strncmp(tracker_wp.wp_inst[id]->name, tracker_name,
			strlen(tracker_name))) {
			wp = tracker_wp.wp_inst[id];
			break;
		}
	}

	if (!wp)
		return IRQ_NONE;

	wp->ts[wp->enter_count] = local_clock();
	wp->flags[wp->enter_count] = (ioread32(wp->tracker_base + BUS_DBG_CON)
					& IRQ_FLAG_MASK) >> IRQ_FLAG_MASK_SHIFT;

	arm_smccc_smc(MTK_SIP_SDA_CONTROL, SDA_SYSTRACKER, TRACKER_SW_RST, id,
		0, 0, 0, 0, &res);
	if (res.a0) {
		pr_notice("%s: fail to reset %s tracker.\n", __func__, wp->name);
		return IRQ_NONE;
	}

	tasklet_schedule(&wp->tasklet);

	return IRQ_HANDLED;
}

static ssize_t tracker_status_show(struct device_driver *driver, char *buf)
{
	int tracker_id, count;
	int n = 0;
	struct tracker_wp_inst *wp;

	for (tracker_id = 0; tracker_id < tracker_wp.nr_wp_inst; tracker_id++) {
		wp = tracker_wp.wp_inst[tracker_id];

		n += snprintf(buf + n, PAGE_SIZE - n, "\n===>> %s Tracker:\n", wp->name);
		n += snprintf(buf + n, PAGE_SIZE - n,
			" ==== AR Watchpoint Hit IRQ is Enabled: %s =====\n",
			(wp->irq_en_ops & IRQ_EN_OPS_AR_WP) ? "True" : "False");
		n += snprintf(buf + n, PAGE_SIZE - n,
			" ==== AW Watchpoint Hit IRQ is Enabled: %s =====\n",
			(wp->irq_en_ops & IRQ_EN_OPS_AW_WP) ? "True" : "False");
		n += snprintf(buf + n, PAGE_SIZE - n,
			" ==== AR 1st Stage Timeout IRQ is Enabled: %s =====\n",
			(wp->irq_en_ops & IRQ_EN_OPS_AR_1ST_TMO) ? "True" : "False");
		n += snprintf(buf + n, PAGE_SIZE - n,
			" ==== AW 1st Stage Timeout IRQ is Enabled: %s =====\n",
			(wp->irq_en_ops & IRQ_EN_OPS_AW_1ST_TMO) ? "True" : "False");
		n += snprintf(buf + n, PAGE_SIZE - n,
			" ==== AR SLVERR Capture IRQ is Enabled: %s =====\n",
			(wp->irq_en_ops & IRQ_EN_OPS_AR_SLV_ERR) ? "True" : "False");
		n += snprintf(buf + n, PAGE_SIZE - n,
			" ==== AW SLVERR Capture IRQ is Enabled: %s =====\n\n",
			(wp->irq_en_ops & IRQ_EN_OPS_AW_SLV_ERR) ? "True" : "False");

		if (!(wp->irq_en_ops & IRQ_EN_OPS_ALL))
			continue;

		n += snprintf(buf + n, PAGE_SIZE - n,
			"Secure Option: 0x%x, Monitored PAddress: 0x%llx ~ 0x%llx.\n>>>>\n",
			wp->security_option, wp->monitored_address & (~wp->address_mask),
			wp->monitored_address | wp->address_mask);

		if (wp->enter_count) {
			for (count = 0; count < wp->enter_count; count++) {
				n += snprintf(buf + n, PAGE_SIZE - n,
				"%s tracker: the %uth time (timestamp: %llu ns, irq flags: 0x%x)\n",
				wp->name, count+1, wp->ts[count], wp->flags[count]);
			}

			n += snprintf(buf + n, PAGE_SIZE - n, "\n =====> IRQ Flag Bitmap <=====\n");
			n += snprintf(buf + n, PAGE_SIZE - n, " ==> 0x01 : AR_1ST_TIMEOUT <==\n");
			n += snprintf(buf + n, PAGE_SIZE - n, " ==> 0x02 : AW_1ST_TIMEOUT <==\n");
			n += snprintf(buf + n, PAGE_SIZE - n, " ==> 0x04 : AW_WP_HIT      <==\n");
			n += snprintf(buf + n, PAGE_SIZE - n, " ==> 0x08 : AR_WP_HIT      <==\n");
			n += snprintf(buf + n, PAGE_SIZE - n, " ==> 0x10 : AR_SLV_ERR_IRQ <==\n");
			n += snprintf(buf + n, PAGE_SIZE - n, " ==> 0x20 : AW_SLV_ERR_IRQ <==\n");
			n += snprintf(buf + n, PAGE_SIZE - n, " =============================\n\n");
		}
	}

	return n;
}

static ssize_t tracker_status_store(struct device_driver *driver, const char *buf, size_t count)
{
	return count;
}
static DRIVER_ATTR(tracker_status, 0664, tracker_status_show, tracker_status_store);

static int systracker_watchpoint_probe(struct platform_device *pdev);
static int systracker_watchpoint_remove(struct platform_device *pdev);

static const struct of_device_id systracker_of_ids[] = {
	{ .compatible = TRACKER_COMPATIBLE, },
	{}
};

static struct platform_driver systracker_wp_drv = {
	.driver = {
		.name = "systracker",
		.bus = &platform_bus_type,
		.owner = THIS_MODULE,
		.of_match_table = systracker_of_ids,
	},
	.probe = systracker_watchpoint_probe,
	.remove = systracker_watchpoint_remove,
};

static int systracker_watchpoint_probe(struct platform_device *pdev)
{
	int ret;
	struct device *dev = &pdev->dev;
	struct device_node *np = pdev->dev.of_node;
	struct device_node *p_subsys;
	struct device_node *child;
	unsigned int value;
	unsigned int irq_en_ops;
	unsigned long long extend;
	struct tracker_wp_inst *wp;
	const char *str;
	struct arm_smccc_res res;

	dev_info(dev, "systracker watchpoint probed.\n");

	tracker_wp.nr_wp_inst = 0;
	tracker_wp.is_sysfs_created = false;

	p_subsys = of_get_child_by_name(np, SUBSYS_NODE_NAME);
	if (!p_subsys) {
		dev_info(dev, "%s: fail to parse subsys from node.\n", __func__);
		return -EINVAL;
	}

	for_each_child_of_node(p_subsys, child) {
		if (tracker_wp.nr_wp_inst >= NR_TRACKER_INST)
			return 0;

		wp = devm_kzalloc(dev, sizeof(struct tracker_wp_inst), GFP_KERNEL);
		tracker_wp.wp_inst[tracker_wp.nr_wp_inst] = wp;

		ret = of_property_read_string(child, "sys-name", &str);
		if (ret) {
			dev_info(dev, "%s: fail to parse sys-name from node.\n", __func__);
			return -EINVAL;
		}
		strncpy(wp->name, str, strlen(str));

		ret = of_property_read_u32(child, "sys-base", &value);
		if (ret) {
			dev_info(dev, "%s: fail to parse sys-base from node.\n", __func__);
			return -EINVAL;
		}

		wp->tracker_base = ioremap(value, WP_REGS_SIZE);
		if (!wp->tracker_base) {
			dev_info(dev, "%s: fail to remap tracker_base.\n", __func__);
			return -EFAULT;
		}
		value = 0;

		ret = of_property_read_u32(child, "irq-en-ops", &irq_en_ops);
		if (ret) {
			dev_info(dev, "%s: fail to parse irq-en-ops from node.\n", __func__);
			return -EINVAL;
		}

		wp->irq_en_ops = irq_en_ops & IRQ_EN_OPS_ALL;
		if (wp->irq_en_ops) {
			ret = of_property_read_u32(child, "wp-addr", &value);
			if (ret) {
				dev_info(dev, "%s: fail to parse wp-addr from node.\n", __func__);
				return -EINVAL;
			}
			wp->monitored_address = value;

			ret = of_property_read_u32(child, "wp-addr-ext", &value);
			if (ret) {
				dev_info(dev, "%s: fail to parse wp-addr-ext from node.\n",
					__func__);
				return -EINVAL;
			}
			extend = value;
			wp->monitored_address |= extend << 32;

			ret = of_property_read_u32(child, "wp-mask", &value);
			if (ret) {
				dev_info(dev, "%s: fail to parse wp-mask from node.\n", __func__);
				return -EINVAL;
			}
			wp->address_mask = value;

			ret = of_property_read_u32(child, "wp-mask-ext", &value);
			if (ret) {
				dev_info(dev, "%s: fail to parse wp-mask-ext from node.\n",
					__func__);
				return -EINVAL;
			}
			extend = value;
			wp->address_mask |= extend << 32;

			ret = of_property_read_u32(child, "wp-secure-option", &value);
			if (ret) {
				dev_info(dev, "%s: fail to parse wp-secure-option from node.\n",
					__func__);
				return -EINVAL;
			}
			wp->security_option = value & SECURE_CMD_MASK;

			ret = of_property_read_string(child, "irq-name", &str);
			if (ret) {
				dev_info(dev, "%s: fail to parse irq-name from node.\n", __func__);
				return -EINVAL;
			}
			strncpy(wp->irq_name, str, strlen(str));

			wp->irq = irq_of_parse_and_map(child, 0);
			if (!wp->irq) {
				dev_info(dev, "%s: fail to parse interrupt from node.\n", __func__);
				return -EINVAL;
			}

			ret = devm_request_irq(dev, wp->irq, bus_dbg_tracker_isr,
					IRQF_TRIGGER_NONE, wp->irq_name, wp->name);
			if (ret) {
				dev_info(dev, "[%s]: request %s tracker irq failed.\n",
					__func__, wp->name);
				return -EFAULT;
			}

			tasklet_setup(&wp->tasklet, bus_dbg_tracker_bh);

			if (!tracker_wp.is_sysfs_created) {
				ret = driver_create_file(&systracker_wp_drv.driver,
							&driver_attr_tracker_status);
				if (ret)
					dev_info(dev, "%s: fail to create tracker_status file.\n",
						__func__);
				else
					tracker_wp.is_sysfs_created = true;
			}

			arm_smccc_smc(MTK_SIP_SDA_CONTROL, SDA_SYSTRACKER, TRACKER_IRQ_SWITCH,
				tracker_wp.nr_wp_inst, wp->irq_en_ops, 0, 0, 0, &res);
			if (res.a0) {
				dev_info(dev, "%s: fail to enable %s tracker irq.\n", __func__,
					wp->name);
				return -EFAULT;
			}
		} else {
			dev_info(dev, "%s: %s tracker irq is not enabled.\n", __func__, wp->name);
		}

		tracker_wp.nr_wp_inst++;
	}

	return 0;
}

static int systracker_watchpoint_remove(struct platform_device *pdev)
{
	dev_info(&pdev->dev, "systracker watchpoint removed.\n");
	return 0;
}

static int __init systracker_watchpoint_init(void)
{
	return platform_driver_register(&systracker_wp_drv);
}

static __exit void systracker_watchpoint_exit(void)
{
	int id;
	struct tracker_wp_inst *wp;
	struct arm_smccc_res res;

	for (id = 0; id < tracker_wp.nr_wp_inst; id++) {
		wp = tracker_wp.wp_inst[id];
		if (!wp)
			continue;

		if (wp->irq_en_ops != IRQ_EN_OPS_NONE) {
			arm_smccc_smc(MTK_SIP_SDA_CONTROL, SDA_SYSTRACKER, TRACKER_IRQ_SWITCH, id,
				IRQ_EN_OPS_NONE, 0, 0, 0, &res);
			if (res.a0)
				pr_notice("%s: fail to disable %s tracker irq.\n",
					__func__, wp->name);

			tasklet_kill(&wp->tasklet);
		}
	}

	if (tracker_wp.is_sysfs_created)
		driver_remove_file(&systracker_wp_drv.driver, &driver_attr_tracker_status);

	platform_driver_unregister(&systracker_wp_drv);
}

module_init(systracker_watchpoint_init);
module_exit(systracker_watchpoint_exit);

MODULE_DESCRIPTION("MediaTek Systracker WP Driver");
MODULE_LICENSE("GPL");
