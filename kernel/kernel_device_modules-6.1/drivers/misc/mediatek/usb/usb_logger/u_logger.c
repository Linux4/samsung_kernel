// SPDX-License-Identifier: GPL-2.0
/*
 * MTK USB Logger Driver
 * *
 * Copyright (c) 2023 MediaTek Inc.
 */

#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/of_platform.h>
#include <linux/platform_device.h>
#include <linux/slab.h>
#include <linux/proc_fs.h>

#include "u_logger.h"
#include "u_logger_trace.h"

#include "xhci-trace.h"
#include "mtu3.h"
#include "mtu3_trace.h"

#define PROC_USB_LOGGER "mtk_usb_logger"
#define PROC_CMD        "cmd"
#define PROC_SUB_CMD    "sub_cmd"

static bool check_xfer_type(struct u_logger *logger, struct mtk_urb *urb)
{
	u8 type_mask = (logger->sub_cmd & LOGGER_TYPE_MASK);
	int type = usb_endpoint_type(urb->desc);
	bool allow = (type_mask & (0x1 << type));

	dumper_dbg(logger, "type:%d mask:%d allow:%d\n", type, type_mask, allow);

	return allow;
}

/* translate struct mtu3_request(device mode) to struct mtk_urb */
static int req_to_mtk(struct mtu3_request *mreq, struct mtk_urb *mtk)
{
	struct usb_request *req = &mreq->request;

	if (!mreq || !mtk || !req)
		return -EINVAL;

	mtk->addr = (void *)req;
	mtk->transfer_buffer = (void *)req->buf;
	mtk->transfer_buffer_length = req->length;
	mtk->actual_length = req->actual;
	mtk->setup_pkt = NULL;
	mtk->desc = mreq->mep->desc;

	if (!mtk->transfer_buffer || !mtk->desc)
		return -EINVAL;

	return 0;
}

/* translate struct urb(host mode) to struct mtk_urb */
static int urb_to_mtk(struct urb *urb, struct mtk_urb *mtk)
{
	if (!urb || !mtk)
		return -EINVAL;

	mtk->addr = (void *)urb;
	mtk->transfer_buffer = (void *)urb->transfer_buffer;
	mtk->transfer_buffer_length = urb->transfer_buffer_length;
	mtk->actual_length = urb->actual_length;
	mtk->setup_pkt = (struct usb_ctrlrequest *) urb->setup_packet;
	mtk->desc = &urb->ep->desc;

	if (!mtk->transfer_buffer || !mtk->desc)
		return -EINVAL;

	return 0;
}

static void xhci_urb_enqueue_dbg(void *data, struct urb *urb)
{
	struct u_logger *logger = (struct u_logger *)data;
	struct mtk_urb mtk = {0};

	if (is_dumper_enable(logger) && is_trace_enqueue(logger) &&
		!urb_to_mtk(urb, &mtk) &&
		check_xfer_type(logger, &mtk))
		trace_urb_enqueue(logger, &mtk); //trigger log_urb_info
}

static void xhci_urb_giveback_dbg(void *data, struct urb *urb)
{
	struct u_logger *logger = (struct u_logger *)data;
	struct mtk_urb mtk = {0};

	if (is_dumper_enable(logger) &&	is_trace_dequeue(logger) &&
		!urb_to_mtk(urb, &mtk) &&
		check_xfer_type(logger, &mtk))
		trace_urb_giveback(logger, &mtk); //trigger log_urb_data
}

static int xhci_trace_init(struct u_logger *logger)
{
	WARN_ON(register_trace_xhci_urb_enqueue_(xhci_urb_enqueue_dbg, logger));
	WARN_ON(register_trace_xhci_urb_giveback_(xhci_urb_giveback_dbg, logger));
	return 0;
}

static void mtu3_gadget_queue_dbg(void *data, struct mtu3_request *mreq)
{
	struct u_logger *logger = (struct u_logger *)data;
	struct mtk_urb mtk = {0};

	if (is_dumper_enable(logger) && is_trace_enqueue(logger) &&
		!req_to_mtk(mreq, &mtk) &&
		check_xfer_type(logger, &mtk))
		trace_urb_enqueue(logger, &mtk); //trigger log_urb_info
}

static void mtu3_req_complete_dbg(void *data, struct mtu3_request *mreq)
{
	struct u_logger *logger = (struct u_logger *)data;
	struct mtk_urb mtk = {0};

	if (is_dumper_enable(logger) &&	is_trace_dequeue(logger) &&
		!req_to_mtk(mreq, &mtk) &&
		check_xfer_type(logger, &mtk))
		trace_urb_giveback(logger, &mtk); //trigger log_urb_data
}

static int mtu3_trace_init(struct u_logger *logger)
{
	WARN_ON(register_trace_mtu3_gadget_queue(mtu3_gadget_queue_dbg, logger));
	WARN_ON(register_trace_mtu3_req_complete(mtu3_req_complete_dbg, logger));
	return 0;
}

static ssize_t proc_cmd_write(struct file *file,
	const char __user *ubuf, size_t count, loff_t *ppos)
{
	struct seq_file *s = file->private_data;
	struct u_logger *logger = s->private;
	char buf[20];
	unsigned int val;
	u8 tmp;

	memset(buf, 0x00, sizeof(buf));
	if (copy_from_user(&buf, ubuf, min_t(size_t, sizeof(buf) - 1, count)))
		return -EFAULT;

	if (kstrtouint(buf, 16, &val))
		return -EINVAL;

	tmp = logger->cmd;
	logger->cmd = (val & 0xff);
	dev_info(logger->dev, "cmd=0x%x->0x%x\n", tmp, logger->cmd);

	return count;
}

static int proc_cmd_show(struct seq_file *s, void *unused)
{
	struct u_logger *logger = s->private;

	seq_printf(s, "cmd=0x%x\n", logger->cmd);
	return 0;
}

static int proc_cmd_open(struct inode *inode, struct file *file)
{
	return single_open(file, proc_cmd_show, pde_data(inode));
}

static const struct proc_ops proc_cmd_fops = {
	.proc_open = proc_cmd_open,
	.proc_write = proc_cmd_write,
	.proc_read = seq_read,
	.proc_lseek = seq_lseek,
	.proc_release = single_release,
};

static ssize_t proc_sub_cmd_write(struct file *file,
	const char __user *ubuf, size_t count, loff_t *ppos)
{
	struct seq_file *s = file->private_data;
	struct u_logger *logger = s->private;
	char buf[20];
	unsigned int val;
	u8 tmp;

	memset(buf, 0x00, sizeof(buf));
	if (copy_from_user(&buf, ubuf, min_t(size_t, sizeof(buf) - 1, count)))
		return -EFAULT;

	if (kstrtouint(buf, 16, &val))
		return -EINVAL;

	tmp = logger->sub_cmd;
	logger->sub_cmd = (val & 0xff);
	dev_info(logger->dev, "sub_cmd=0x%x->0x%x\n", tmp, logger->sub_cmd);

	return count;
}

static int proc_sub_cmd_show(struct seq_file *s, void *unused)
{
	struct u_logger *logger = s->private;

	seq_printf(s, "sub_cmd=0x%x\n", logger->sub_cmd);
	return 0;
}

static int proc_sub_cmd_open(struct inode *inode, struct file *file)
{
	return single_open(file, proc_sub_cmd_show, pde_data(inode));
}

static const struct proc_ops proc_sub_cmd_fops = {
	.proc_open = proc_sub_cmd_open,
	.proc_write = proc_sub_cmd_write,
	.proc_read = seq_read,
	.proc_lseek = seq_lseek,
	.proc_release = single_release,
};

static int u_logger_procfs_init(struct u_logger *logger)
{
	struct device *dev = logger->dev;
	struct proc_dir_entry *root, *file;
	int ret = 0;

	root = proc_mkdir(PROC_USB_LOGGER, NULL);
	if (!root) {
		dev_info(dev, "failed to creat %s dir\n", PROC_USB_LOGGER);
		ret = -ENOMEM;
		goto err0;
	}

	file = proc_create_data(PROC_CMD, 0644,
				root, &proc_cmd_fops, logger);
	if (!file) {
		dev_info(dev, "failed to creat %s file\n", PROC_CMD);
		ret = -ENOMEM;
		goto err1;
	}

	file = proc_create_data(PROC_SUB_CMD, 0644,
				root, &proc_sub_cmd_fops, logger);
	if (!file) {
		dev_info(dev, "failed to creat %s file\n", PROC_SUB_CMD);
		ret = -ENOMEM;
		goto err1;
	}

	logger->root = root;
	return 0;
err1:
	proc_remove(root);
err0:
	return ret;
}

static int u_logger_procfs_exit(struct u_logger *logger)
{
	proc_remove(logger->root);
	return 0;
}

static int u_logger_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct u_logger *logger;
	int ret = 0;

	logger = kzalloc(sizeof(*logger), GFP_KERNEL);
	if (!logger)
		return -ENOMEM;

	logger->dev = dev;

	platform_set_drvdata(pdev, logger);

	mtu3_trace_init(logger);
	xhci_trace_init(logger);

	/* create procfs for logger */
	u_logger_procfs_init(logger);

	return ret;
}

static int u_logger_remove(struct platform_device *pdev)
{
	struct u_logger *logger = dev_get_drvdata(&pdev->dev);

	u_logger_procfs_exit(logger);
	return 0;
}

static const struct of_device_id u_logger_ids[] = {
	{.compatible = "mediatek,usb-logger",},
	{},
};

static struct platform_driver u_logger_driver = {
	.probe = u_logger_probe,
	.remove = u_logger_remove,
	.driver = {
		.name = "mtk-usb-logger",
		.of_match_table = u_logger_ids,
	},
};

module_platform_driver(u_logger_driver);

MODULE_DESCRIPTION("Mediatek USB logger driver");
MODULE_LICENSE("GPL");
