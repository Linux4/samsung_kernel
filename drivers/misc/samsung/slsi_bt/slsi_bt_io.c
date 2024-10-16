/******************************************************************************
 *                                                                            *
 * Copyright (c) 2021 Samsung Electronics Co., Ltd. All rights reserved       *
 *                                                                            *
 * S.LSI Bluetooth Device Driver                                              *
 *                                                                            *
 * This driver manages S.LSI bluetooth service and data transaction           *
 *                                                                            *
 ******************************************************************************/
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/fs.h>
#include <linux/poll.h>
#include <linux/device.h>
#include <linux/skbuff.h>
#include <linux/wait.h>
#include <linux/types.h>
#include <linux/mutex.h>
#include <linux/cdev.h>
#include <linux/proc_fs.h>
#include <linux/seq_file.h>

#include "slsi_bt_io.h"
#include "slsi_bt_err.h"
#include "slsi_bt_controller.h"
#include "hci_pkt.h"
#include "hci_trans.h"
#include "slsi_bt_property.h"
#ifdef CONFIG_SLSI_BT_H4
#include "hci_h4.h"
#endif
#ifdef CONFIG_SLSI_BT_BCSP
#include "hci_bcsp.h"
#endif
#ifdef CONFIG_SLSI_BT_USE_UART_INTERFACE
#include "slsi_bt_tty.h"
#endif

#define SLSI_BT_MODDESC "SLSI Maxwell BT Module Driver"
#define SLSI_BT_MODAUTH "Samsung Electronics Co., Ltd"
#define SLSI_BT_MODVERSION "-devel"

/* S.LSI Bluetooth Device Driver Context */
static struct {
	dev_t                          dev;
	struct class                   *class;

	struct cdev                    cdev;
	struct device                  *device;

	atomic_t                       err_cnt;

	bool                           open_status;
} bt_drv;

/* Data transport Context */
static struct {
	struct hci_trans               tr;              /* own transporter */
	unsigned int                   enabled_trs;     /* Using transpoters */

	struct sk_buff_head            recv_q;

	wait_queue_head_t              read_wait;

	unsigned int                   send_data_count;
	unsigned int                   recv_data_count;
	unsigned int                   read_data_count;

	unsigned int                   recv_pkt_count;
	unsigned int                   read_pkt_count;
} bt_trans;

static void slsi_bt_error_handle(int reason)
{
	const char hci_event_hw_error[4] = { HCI_EVENT_PKT, 0x10, 1, 0 };
	struct sk_buff *skb = alloc_hci_pkt_skb(4);

	SET_HCI_PKT_HW_ERROR(skb);
	skb_put_data(skb, hci_event_hw_error, sizeof(hci_event_hw_error));

	bt_trans.tr.recv_skb(&bt_trans.tr, skb);
}

static int slsi_bt_skb_recv(struct hci_trans *htr, struct sk_buff *skb)
{
	if (!skb)
		return -EINVAL;

	if (TEST_HCI_PKT_HW_ERROR(skb)) {
		BT_WARNING("HW Error packet is received\n");
		skb_queue_purge(&bt_trans.recv_q);
	} else if (GET_HCI_PKT_TYPE(skb) == HCI_PROPERTY_PKT) {
		/* Property */
		return slsi_bt_property_recv(htr, skb);
	}

	bt_trans.recv_pkt_count++;
	bt_trans.recv_data_count += skb->len;

	skb_queue_tail(&bt_trans.recv_q, skb);
	BT_DBG("received messages: %d\n", skb_queue_len(&bt_trans.recv_q));

	wake_up(&bt_trans.read_wait);

	return 0;
}

static unsigned int slsi_bt_poll(struct file *file, poll_table *wait)
{
	if (!skb_queue_empty(&bt_trans.recv_q)) {
		BT_DBG("queue(s) is not empty\n");
		return POLLIN | POLLRDNORM;
	}

	BT_DBG("wait\n");
	poll_wait(file, &bt_trans.read_wait, wait);

	if (skb_queue_empty(&bt_trans.recv_q)) {
		BT_DBG("no change\n");
		return POLLOUT;
	}

	BT_DBG("queue changed\n");
	return POLLIN | POLLRDNORM;
}

static ssize_t slsi_bt_read(struct file *file, char __user *buf, size_t nr,
				loff_t *loff)
{
	struct sk_buff *skb;
	size_t len, offset = 0;

	BT_INFO("number of read: %zu\n", nr);

	if (buf == NULL)
		return -EINVAL;

	if (wait_event_interruptible(bt_trans.read_wait,
				!skb_queue_empty(&bt_trans.recv_q)) != 0)
		return 0;

	if (bt_drv.open_status == false)
		return 0;

	while (nr > 0 && (skb = skb_dequeue(&bt_trans.recv_q)) != NULL) {
		len = min(nr, (size_t)skb->len);
		BT_DBG("len: %zu\n", len);
		if (copy_to_user(buf + offset, skb->data, len) != 0) {
			BT_WARNING("copy_to_user returned error\n");
			skb_queue_head(&bt_trans.recv_q, skb);
			return -EACCES;
		}

		if (skb->len > len) {
			skb_pull(skb, len);
			skb_queue_head(&bt_trans.recv_q, skb);
			BT_DBG("remained %u data\n", skb->len);
		} else { /* Complete read one packet */
			bt_trans.read_pkt_count++;
			kfree_skb(skb);
		}
		nr -= len;
		offset += len;
	}

	BT_DBG("%zu data read complete. remained message=%d\n", offset,
		skb_queue_len(&bt_trans.recv_q));
	bt_trans.read_data_count += offset;
	return offset;
}

static ssize_t slsi_bt_write(struct file *file,
			const char __user *data, size_t count, loff_t *offset)
{
	ssize_t ret;

	BT_INFO("count: %zu\n", count);

	if (slsi_bt_err_status()) {
		BT_DBG("bt controller is in error state. wait for recovery\n");
		return -EBUSY;
	}

	ret = bt_trans.tr.target->send(bt_trans.tr.target, data, count,
			HCITRFLAG_UBUF);
	if (ret < 0) {
		slsi_bt_err(SLSI_BT_ERR_SEND_FAIL);
	}
	bt_trans.send_data_count += ret;

	return ret;
}

static int slsi_bt_transport_init(unsigned int en_trs)
{
	struct hci_trans *htr, *next = NULL;
	struct hci_trans *next_host = &bt_trans.tr;
	int ret;

	BT_INFO("en_trs: 0x%x\n", en_trs);

#ifdef CONFIG_SLSI_BT_USE_UART_INTERFACE
	if (en_trs & SLSI_BT_TR_EN_TTY) {
		/* Connect to tty */
		ret = slsi_bt_tty_open(&htr);
		if (ret) {
			BT_ERR("error slsi_bt_tty_open %d\n", ret);
			return ret;
		}
		next = htr;

	}
#endif
#ifdef CONFIG_SLSI_BT_BCSP
	if (en_trs & SLSI_BT_TR_EN_BCSP) {
		/* Connect to bcsp */
		htr = kmalloc(sizeof(struct hci_trans), GFP_KERNEL);
		if (htr == NULL) {
			BT_ERR("Failed to alloc memory\n");
			return -ENOMEM;
		}
		hci_bcsp_init(htr, next);
		next = htr;
	}
#endif
#ifdef CONFIG_SLSI_BT_H4
	if (en_trs & SLSI_BT_TR_EN_H4) {
		/* Connect to h4(reverse) */
		htr = kmalloc(sizeof(struct hci_trans), GFP_KERNEL);
		if (htr == NULL) {
			BT_ERR("Failed to alloc memory\n");
			return -ENOMEM;
		}
		hci_h4_init(htr, true);
		hci_trans_setup_host_target(htr, next);
		next = htr;
	}
#endif

	/* Setup the transport layers */
	hci_trans_init(&bt_trans.tr, "slsi_bt");
	hci_trans_setup_host_target(&bt_trans.tr, next);

	bt_trans.tr.recv_skb = slsi_bt_skb_recv;
	init_waitqueue_head(&bt_trans.read_wait);
	next_host = &bt_trans.tr;

	bt_trans.enabled_trs = en_trs;

	return 0;
}

static void slsi_bt_transport_deinit(void)
{
	struct hci_trans *host = bt_trans.tr.target, *del;

#ifdef CONFIG_SLSI_BT_USE_UART_INTERFACE
	if (bt_trans.enabled_trs & SLSI_BT_TR_EN_TTY)
		slsi_bt_tty_close();
#endif

	/* Deinitlize from the lowest transport layer */
	while (host && host->target)
		host = host->target;

	while (host) {
		del = host;
		host = host->host;
		if (del->deinit)
			del->deinit(del);
	}
}

static int slsi_bt_open(struct inode *inode, struct file *file)
{
	int ret = 0;

	BT_INFO("(open_status=%u)\n", bt_drv.open_status ? 1 : 0);

	/* Only 1 user allowed */
	if (bt_drv.open_status)
		return -EBUSY;

	/* Error manager */
	slsi_bt_err_init(slsi_bt_error_handle);

	/* Enable BT Controller */
	ret = slsi_bt_controller_start();
	if (ret != 0) {
		BT_ERR("open failed: controller start failed(%d)\n", ret);
		return ret;
	}

	/* Configure Transpoters */
	ret = slsi_bt_transport_init(SLSI_BT_TR_EN_DEFAULT);
	if (ret) {
		BT_ERR("error bt_transport_init: %d\n", ret);
		slsi_bt_transport_deinit();
		slsi_bt_controller_stop();
		return ret;
	}

	/* Init own context */
	skb_queue_head_init(&bt_trans.recv_q);
	bt_trans.recv_pkt_count = 0;
	bt_trans.send_data_count = 0;
	bt_trans.recv_data_count = 0;

	bt_drv.open_status = true;

	BT_INFO("BT open is succeed\n");
	return ret;
}

static int slsi_bt_release(struct inode *inode, struct file *file)
{
	BT_INFO("pending recv_q=%d, en_trs=%u\n",
		skb_queue_len(&bt_trans.recv_q), bt_trans.enabled_trs);

	skb_queue_purge(&bt_trans.recv_q);
	wake_up(&bt_trans.read_wait);
	slsi_bt_transport_deinit();
	slsi_bt_controller_stop();
	slsi_bt_err_deinit();
	bt_drv.open_status = false;

	BT_INFO("done.\n");
	return 0;
}

static long slsi_bt_ioctl(struct file *file, unsigned int cmd,
		unsigned long arg)
{
	int ret = 0;

	switch (cmd) {
	case SBTIOCT_CHANGE_TRS:
		/*
		if (bt_trans.send_data_count || bt_trans.recv_data_count) {
			BT_ERR("SBTIOCT_CHANGE_TRS should be set before any "
				"data transmission");
			return -EINVAL;
		}
		*/
		slsi_bt_transport_deinit();
		ret = slsi_bt_transport_init((unsigned int)arg);
		if (ret) {
			BT_ERR("error bt_transport_init: %d\n", ret);
			slsi_bt_transport_deinit();
			slsi_bt_err(SLSI_BT_ERR_TR_INIT_FAIL);
			return ret;
		}
		return ret;
	}

#ifdef CONFIG_SLSI_BT_USE_UART_INTERFACE
	if (bt_trans.enabled_trs & SLSI_BT_TR_EN_TTY)
		return slsi_bt_tty_ioctl(file, cmd, arg);
#endif
	return ret;
}

static const struct file_operations slsi_bt_fops = {
	.owner            = THIS_MODULE,
	.open             = slsi_bt_open,
	.release          = slsi_bt_release,
	.poll             = slsi_bt_poll,
	.read             = slsi_bt_read,
	.write            = slsi_bt_write,
	.unlocked_ioctl   = slsi_bt_ioctl,
};

static int slsi_bt_service_proc_show(struct seq_file *m, void *v)
{
	struct hci_trans *tr;
	int i = 0;

	seq_puts(m, "Driver statistics:\n");
	seq_printf(m, "  open status            = %d\n", bt_drv.open_status);
	for (i = 0, tr = &bt_trans.tr; tr != NULL; i++, tr = tr->target)
		seq_printf(m, "  Transporter Stack(%d)   = %s\n", i, tr->name);

	if (bt_trans.tr.name) {
		seq_printf(m, "\n  %s:\n", bt_trans.tr.name);
		seq_printf(m, "    Sent data count        = %u\n",
			bt_trans.send_data_count);
		seq_printf(m, "    Received data count    = %u\n",
			bt_trans.recv_data_count);
		seq_printf(m, "    Read data count        = %u\n",
			bt_trans.recv_data_count);
		seq_printf(m, "    Received packet count  = %u\n",
			bt_trans.recv_pkt_count);
		seq_printf(m, "    Read packet count      = %u\n",
			bt_trans.read_pkt_count);
		seq_printf(m, "    Remeaind packet count  = %u\n",
			skb_queue_len(&bt_trans.recv_q));
	}

	/* Loop transporter's proc_show */
	for (tr = bt_trans.tr.target; tr != NULL; tr = tr->target)
		if (tr->proc_show)
			tr->proc_show(tr, m);

	slsi_bt_err_proc_show(m, v);

	return 0;
}

static int slsi_bt_service_proc_open(struct inode *inode, struct file *file)
{
	return single_open(file, slsi_bt_service_proc_show, NULL);
}

static const struct proc_ops scsc_bt_procfs_fops = {
	.proc_open    = slsi_bt_service_proc_open,
	.proc_read    = seq_read,
	.proc_lseek   = seq_lseek,
	.proc_release = single_release,
};

static int __init slsi_bt_module_init(void)
{
	int ret;
	struct proc_dir_entry *procfs_dir;

	BT_INFO("%s %s (C) %s\n", SLSI_BT_MODDESC, SLSI_BT_MODVERSION,
		SLSI_BT_MODAUTH);

	memset(&bt_drv, 0, sizeof(bt_drv));

	/* register character device */
	ret = alloc_chrdev_region(&bt_drv.dev, 0, SLSI_BT_MINORS,
				  SLSI_BT_DEV_NAME);
	if (ret) {
		BT_ERR("error alloc_chrdev_region %d\n", ret);
		return ret;
	}

	bt_drv.class = class_create(THIS_MODULE, SLSI_BT_DEV_NAME);
	if (IS_ERR(bt_drv.class)) {
		ret = PTR_ERR(bt_drv.class);
		goto error;
	}

	cdev_init(&bt_drv.cdev, &slsi_bt_fops);
	ret = cdev_add(&bt_drv.cdev, MKDEV(MAJOR(bt_drv.dev),
			MINOR(SLSI_BT_MINOR_CTRL)), 1);
	if (ret) {
		BT_ERR("cdev_add failed for (%s)\n", SLSI_BT_DEV_NAME);
		bt_drv.cdev.dev = 0;
		goto error;
	}

	bt_drv.device = device_create(bt_drv.class, NULL, bt_drv.cdev.dev,
				      NULL, SLSI_BT_DEV_NAME);
	if (bt_drv.device == NULL) {
		cdev_del(&bt_drv.cdev);
		ret = -EFAULT;
		goto error;
	}

	/* register proc */
	procfs_dir = proc_mkdir("driver/scsc_bt", NULL);
	if (NULL != procfs_dir) {
		proc_create_data("stats", S_IRUSR | S_IRGRP,
				 procfs_dir, &scsc_bt_procfs_fops, NULL);
	}

	/* bt controller init */
	ret = slsi_bt_controller_init();
	if (ret) {
		BT_ERR("error bt_ctrl-init %d\n", ret);
		goto error;
	}

#ifdef CONFIG_SLSI_BT_USE_UART_INTERFACE
	/* tty init */
	ret = slsi_bt_tty_init();
	if (ret) {
		BT_ERR("error slsi_bt_tty_init %d\n", ret);
		slsi_bt_controller_exit();
		goto error;
	}
#endif
	slsi_bt_log_init((void*)&bt_trans.tr);
	BT_DBG("cdev=%p class=%p\n", bt_drv.device, bt_drv.class);

	return 0;

error:
	BT_ERR("error class_create bt device\n");
	unregister_chrdev_region(bt_drv.dev, SLSI_BT_MINORS);
	bt_drv.dev = 0u;

	return ret;
}

static void __exit slsi_bt_module_exit(void)
{
	BT_INFO("enter\n");

	slsi_bt_log_deinit();
#ifdef CONFIG_SLSI_BT_USE_UART_INTERFACE
	slsi_bt_tty_exit();
#endif
	slsi_bt_controller_exit();

	if (bt_drv.device) {
		device_destroy(bt_drv.class, bt_drv.cdev.dev);
		bt_drv.device = NULL;
	}

	cdev_del(&bt_drv.cdev);
	class_destroy(bt_drv.class);
	unregister_chrdev_region(bt_drv.dev, SLSI_BT_MINORS);
	BT_INFO("exit, module unloaded\n");
}

module_init(slsi_bt_module_init);
module_exit(slsi_bt_module_exit);

MODULE_DESCRIPTION(SLSI_BT_MODDESC);
MODULE_AUTHOR(SLSI_BT_MODAUTH);
MODULE_LICENSE("GPL");
MODULE_VERSION(SLSI_BT_MODVERSION);
