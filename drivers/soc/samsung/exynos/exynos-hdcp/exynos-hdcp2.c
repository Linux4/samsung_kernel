/*
 * drivers/soc/samsung/exynos-hdcp/exynos-hdcp.c
 *
 * Copyright (c) 2016 Samsung Electronics Co., Ltd.
 *		http://www.samsung.com
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/uaccess.h>
#include <linux/smc.h>
#include <asm/cacheflush.h>
#include <soc/samsung/exynos-smc.h>
#include "exynos-hdcp2-teeif.h"
#include "exynos-hdcp2-log.h"
#include "exynos-hdcp2-dplink-if.h"
#include "exynos-hdcp2-dplink.h"
#include "exynos-hdcp2-dplink-selftest.h"
#include "exynos-hdcp2-dplink-inter.h"
#include <linux/of_irq.h>
#include <linux/platform_device.h>
#include <linux/interrupt.h>
#include <linux/irq.h>
#include <linux/irqreturn.h>
#include <linux/dma-mapping.h>
#if defined(CONFIG_HDCP2_EMULATION_MODE)
#include <exynos-hdcp2-crypto.h>
#endif

#define EXYNOS_HDCP_DEV_NAME	"hdcp2"

static struct miscdevice hdcp;
struct device *device_hdcp;
static DEFINE_MUTEX(hdcp_lock);
enum hdcp_result hdcp_link_ioc_authenticate(void);
extern enum dp_state dp_hdcp_state;

struct hdcp_ctx {
	struct delayed_work work;
	/* debugfs root */
	struct dentry *debug_dir;
	/* seclog can be disabled via debugfs */
	bool enabled;
	unsigned int irq;
};

static struct hdcp_ctx h_ctx;
static uint32_t inst_num;

static int hdcp_open(struct inode *inode, struct file *file)
{
	struct miscdevice *miscdev = file->private_data;
	struct device *dev = miscdev->this_device;
	struct hdcp_info *info;

	info = kzalloc(sizeof(struct hdcp_info), GFP_KERNEL);
	if (!info)
		return -ENOMEM;

	info->dev = dev;
	file->private_data = info;

	mutex_lock(&hdcp_lock);
	inst_num++;
	/* todo: hdcp device initialize ? */
	mutex_unlock(&hdcp_lock);

	return 0;
}

static int hdcp_release(struct inode *inode, struct file *file)
{
	struct hdcp_info *info = file->private_data;

	/* disable drm if we were the one to turn it on */
	mutex_lock(&hdcp_lock);
	inst_num--;
	/* todo: hdcp device finalize ? */
	mutex_unlock(&hdcp_lock);

	kfree(info);
	return 0;
}

static void exynos_hdcp_worker(struct work_struct *work)
{
	int ret;

	if (dp_hdcp_state == DP_DISCONNECT) {
		hdcp_err("dp_disconnected\n");
		return;
	}

	hdcp_info("Exynos HDCP interrupt occur by LDFW.\n");
	ret = hdcp_dplink_auth_check(HDCP_DRM_ON);
}

static irqreturn_t exynos_hdcp_irq_handler(int irq, void *dev_id)
{
	if (h_ctx.enabled) {
		if (dp_hdcp_state == DP_HDCP_READY)
			schedule_delayed_work(&h_ctx.work, msecs_to_jiffies(0));
		else
			schedule_delayed_work(&h_ctx.work, msecs_to_jiffies(2500));
	}

	return IRQ_HANDLED;
}

static int exynos_hdcp_probe(struct platform_device *pdev)
{
	struct irq_data *hdcp_irqd = NULL;
	irq_hw_number_t hwirq = 0;
	int err;

	h_ctx.irq = irq_of_parse_and_map(pdev->dev.of_node, 0);
	if (!h_ctx.irq) {
		dev_err(&pdev->dev, "Fail to get irq from dt\n");
		return -EINVAL;
	}

	/* Get irq_data for secure log */
	hdcp_irqd = irq_get_irq_data(h_ctx.irq);
	if (!hdcp_irqd) {
		dev_err(&pdev->dev, "Fail to get irq_data\n");
		return -EINVAL;
	}

	/* Get hardware interrupt number */
	hwirq = irqd_to_hwirq(hdcp_irqd);
	err = devm_request_irq(&pdev->dev, h_ctx.irq,
			exynos_hdcp_irq_handler,
			IRQF_TRIGGER_RISING, pdev->name, NULL);

	device_hdcp = &pdev->dev;

	pdev->dev.dma_mask = &pdev->dev.coherent_dma_mask;
	dma_set_mask(&pdev->dev, DMA_BIT_MASK(36));

	if (err) {
		dev_err(&pdev->dev,
				"Fail to request IRQ handler. err(%d) irq(%d)\n",
				err, h_ctx.irq);
		return err;
	}
	/* Set workqueue for Secure log as bottom half */
	INIT_DELAYED_WORK(&h_ctx.work, exynos_hdcp_worker);
	h_ctx.enabled = true;
	err = exynos_smc(SMC_HDCP_NOTIFY_INTR_NUM, 0, 0, hwirq);
	hdcp_info("Exynos HDCP driver probe done! (%d)\n", err);

	return err;
}

static const struct of_device_id exynos_hdcp_of_match_table[] = {
	{ .compatible = "samsung,exynos-hdcp", },
	{ },
};

static struct platform_driver exynos_hdcp_driver = {
	.probe = exynos_hdcp_probe,
	.driver = {
		.name = "exynos-hdcp",
		.owner = THIS_MODULE,
		.of_match_table = of_match_ptr(exynos_hdcp_of_match_table),
	}
};

static int __init hdcp_init(void)
{
	int ret;

	hdcp_info("hdcp2 driver init\n");

	ret = misc_register(&hdcp);
	if (ret) {
		hdcp_err("hdcp can't register misc on minor=%d\n",
				MISC_DYNAMIC_MINOR);
		return ret;
	}

	hdcp_session_list_init();
#if defined(CONFIG_HDCP2_DP_ENABLE)
	if (hdcp_dplink_init() < 0) {
		hdcp_err("hdcp_dplink_init fail\n");
		return -EINVAL;
	}
#endif
	ret = hdcp_tee_open();
	if (ret) {
		hdcp_err("hdcp_tee_open fail\n");
		return -EINVAL;
	}

	return platform_driver_register(&exynos_hdcp_driver);
}

static void __exit hdcp_exit(void)
{
	/* todo: do clear sequence */
	cancel_delayed_work_sync(&h_ctx.work);

	misc_deregister(&hdcp);
	hdcp_session_list_destroy();
	hdcp_tee_close();
	platform_driver_unregister(&exynos_hdcp_driver);
}

#if defined(CONFIG_HDCP2_EMULATION_MODE)
static ssize_t hdcp_write(struct file *filp, const char __user *buf,
		size_t count, loff_t *f_pos)
{
	s32 value = 1;
	int8_t test_msg[] = "SHA1 test message";
	uint8_t temp_buf[30];

	if (count == sizeof(s32)) {
		if (copy_from_user(&value, buf, sizeof(s32)))
			return -EFAULT;
	}

	dp_hdcp_state = DP_CONNECT;

	if (value == 0) {
		hdcp_info("[%s:%d]\n", __func__, __LINE__);
		/* dummy call for TEM */
		hdcp_calc_sha1(temp_buf, (uint8_t *)test_msg, sizeof(test_msg));
		hdcp_dplink_set_paring_available();
		hdcp_dplink_set_hprime_available();
		hdcp_dplink_set_rp_ready();
		hdcp_dplink_set_reauth();
		hdcp_dplink_set_integrity_fail();

		hdcp_dplink_cancel_auth();
		hdcp_dplink_connect_state(DP_DISCONNECT);

		hdcp_dplink_clear_all();
	} else {
		hdcp_info("[%s:%d]\n", __func__, __LINE__);
		exynos_hdcp_irq_handler(0, NULL);
	}

	return count;
}
#endif

static const struct file_operations hdcp_fops = {
	.owner		= THIS_MODULE,
	.open		= hdcp_open,
	.release	= hdcp_release,
#if defined(CONFIG_HDCP2_EMULATION_MODE)
	.write		= hdcp_write,
#endif
};

static struct miscdevice hdcp = {
	.minor	= MISC_DYNAMIC_MINOR,
	.name	= EXYNOS_HDCP_DEV_NAME,
	.fops	= &hdcp_fops,
};

module_init(hdcp_init);
module_exit(hdcp_exit);

MODULE_DESCRIPTION("Exynos Secure hdcp driver");
MODULE_AUTHOR("<hakmin_1.kim@samsung.com>");
MODULE_LICENSE("GPL");
