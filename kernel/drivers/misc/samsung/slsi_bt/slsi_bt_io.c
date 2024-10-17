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
#include <linux/delay.h>

#include <scsc/scsc_wakelock.h>

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

#define SLSI_BT_MODULE_NAME "scsc_bt"

static DEFINE_MUTEX(bt_start_mutex);

/* S.LSI Bluetooth Device Driver Context */
static struct {
	dev_t                          dev;
	struct class                   *class;

	struct cdev                    cdev;
	struct device                  *device;

#define SLSI_BT_PROC_DIR    "driver/"SLSI_BT_MODULE_NAME
	struct proc_dir_entry          *procfs_dir;

	struct scsc_wake_lock          wake_lock;       /* common & write */
	struct scsc_wake_lock          read_wake_lock;

	bool                           open_status;
	atomic_t                       users;
} bt_drv;

/* Data transport Context */
static struct {
	struct hci_trans               *htr;            /* own transporter */
	unsigned int                   enabled_trs;     /* Using transpoters */

	struct sk_buff_head            recv_q;
	struct mutex                   recv_q_lock;

	wait_queue_head_t              read_wait;

	unsigned int                   send_data_count;
	unsigned int                   recv_data_count;
	unsigned int                   read_data_count;

	unsigned int                   recv_pkt_count;
	unsigned int                   read_pkt_count;
} bt_trans;

static struct sk_buff *hw_error_packet(void)
{
	const char data[] = { 0 };
	struct sk_buff *skb = alloc_hci_pkt_skb(4);

	if (skb) {
		SET_HCI_PKT_HW_ERROR(skb);
		skb_put_u8(skb, HCI_EVENT_PKT);
		skb_put_u8(skb, HCI_EVENT_HARDWARE_ERROR_EVENT);
		skb_put_u8(skb, (sizeof(data))&0xFF);
		skb_put_data(skb, data, sizeof(data));
	} else
		BT_ERR("failed to allocate skb for hw error\n");

	return skb;
}

struct sk_buff *syserr_info_packet(void)
{
	unsigned char buf[10];
	struct sk_buff *skb = alloc_hci_pkt_skb(4+sizeof(buf));
	size_t len = slsi_bt_controller_get_syserror_info(buf, sizeof(buf));

	if (skb) {
		skb_put_u8(skb, HCI_EVENT_PKT);
		skb_put_u8(skb, HCI_EVENT_VENDOR_SPECIFIC_EVENT);
		skb_put_u8(skb, (1+len)&0xFF);
		skb_put_u8(skb, HCI_VSE_SYSTEM_ERROR_INFO_SUB_CODE);
		skb_put_data(skb, buf, len);
	} else
		BT_WARNING("failed to allocate skb for sys error info\n");

	return skb;
}

static void slsi_bt_error_packet_ready(void)
{
	struct sk_buff *skb = NULL;

	mutex_lock(&bt_trans.recv_q_lock);
	skb = skb_peek_tail(&bt_trans.recv_q);
	if (skb && TEST_HCI_PKT_HW_ERROR(skb)) {
		/* There is already HW_ERROR packet in the recv queue */
		mutex_unlock(&bt_trans.recv_q_lock);
		return;
	}

	skb = skb_dequeue(&bt_trans.recv_q);
	skb_queue_purge(&bt_trans.recv_q);
	if (skb) {
		/* Save the first packet. It is in reading operation */
		if (GET_HCI_PKT_TR_TYPE(skb) == HCI_TRANS_ON_READING)
			skb_queue_tail(&bt_trans.recv_q, skb);
		else
			kfree_skb(skb);
	}

	skb = syserr_info_packet();
	if (skb)
		skb_queue_tail(&bt_trans.recv_q, skb);

	skb = hw_error_packet();
	if (skb)
		skb_queue_tail(&bt_trans.recv_q, skb);

	mutex_unlock(&bt_trans.recv_q_lock);
}

static void slsi_bt_error_handle(int reason, bool lazy)
{
	TR_WARNING("HW Error is occured\n");

	mutex_lock(&bt_start_mutex);
	if (bt_drv.open_status == false) {
		mutex_unlock(&bt_start_mutex);
		return;
	}

	/* This closes tty first to avoid receiving unnecessary garbage data
	 * in the abnormal situations. It can be make race condition with
	 * slsi_bt_transport_deinit() called by slsi_bt_release(). */
	BT_DBG("try to close tty\n");
	slsi_bt_tty_close();
	slsi_bt_error_packet_ready();
	mutex_unlock(&bt_start_mutex);

	if (!lazy)
		wake_up(&bt_trans.read_wait);
}

static int slsi_bt_skb_recv(struct hci_trans *htr, struct sk_buff *skb)
{
	if (!skb)
		return -EINVAL;

	if (GET_HCI_PKT_TYPE(skb) == HCI_PROPERTY_PKT)
		return slsi_bt_property_recv(htr, skb);

	bt_trans.recv_pkt_count++;
	bt_trans.recv_data_count += skb->len;

	mutex_lock(&bt_trans.recv_q_lock);
	skb_queue_tail(&bt_trans.recv_q, skb);
	mutex_unlock(&bt_trans.recv_q_lock);

	TR_DBG("%d message(s) is remained\n", skb_queue_len(&bt_trans.recv_q));
	if (!wake_lock_active(&bt_drv.read_wake_lock))
		wake_lock(&bt_drv.read_wake_lock);

	wake_up(&bt_trans.read_wait);

	return 0;
}

static unsigned int slsi_bt_poll(struct file *file, poll_table *wait)
{
	if (!skb_queue_empty(&bt_trans.recv_q)) {
		TR_DBG("queue is not empty\n");
		return POLLIN | POLLRDNORM;
	}

	TR_DBG("wait\n");
	poll_wait(file, &bt_trans.read_wait, wait);

	if (skb_queue_empty(&bt_trans.recv_q)) {
		TR_DBG("no change\n");
		return POLLOUT;
	}

	TR_DBG("queue changed\n");
	return POLLIN | POLLRDNORM;
}

static ssize_t slsi_bt_read(struct file *file, char __user *buf, size_t nr,
				loff_t *loff)
{
	struct sk_buff *skb;
	size_t len, offset = 0;

	TR_DBG("number of read: %zu\n", nr);

	if (buf == NULL)
		return -EINVAL;

	if (wait_event_interruptible(bt_trans.read_wait,
				!skb_queue_empty(&bt_trans.recv_q)) != 0)
		return 0;

	if (bt_drv.open_status == false)
		return 0;

	while (nr > 0 && (skb = skb_dequeue(&bt_trans.recv_q)) != NULL) {
		mutex_lock(&bt_trans.recv_q_lock);

		len = min(nr, (size_t)skb->len);
		TR_DBG("confirmed length: %zu\n", len);
		if (copy_to_user(buf + offset, skb->data, len) != 0) {
			TR_WARNING("copy_to_user returned error\n");
			skb_queue_head(&bt_trans.recv_q, skb);
			mutex_unlock(&bt_trans.recv_q_lock);
			return -EACCES;
		}

		if (skb->len > len) {
			skb_pull(skb, len);
			SET_HCI_PKT_TR_TYPE(skb, HCI_TRANS_ON_READING);
			skb_queue_head(&bt_trans.recv_q, skb);
			TR_DBG("%u byte(s) are left in the packet\n", skb->len);
		} else { /* Complete read one packet */
			bt_trans.read_pkt_count++;
			kfree_skb(skb);
		}
		nr -= len;
		offset += len;

		mutex_unlock(&bt_trans.recv_q_lock);
	}

	if (skb_queue_len(&bt_trans.recv_q) > 0 &&
	    wake_lock_active(&bt_drv.read_wake_lock))
		wake_unlock(&bt_drv.read_wake_lock);

	TR_DBG("%zu bytes read complete. remaining message(s)=%d\n", offset,
		skb_queue_len(&bt_trans.recv_q));
	bt_trans.read_data_count += offset;
	return offset;
}

static ssize_t slsi_bt_write(struct file *file,
			const char __user *data, size_t count, loff_t *offset)
{
	struct hci_trans *htr = hci_trans_get_next(bt_trans.htr);
	ssize_t ret = 0;
	size_t len = count;

	TR_DBG("count of write: %zu\n", count);

	if (slsi_bt_err_status()) {
		TR_ERR("bt controller is in error state. wait for recovery!\n");
		return -EBUSY;
	}

	if (htr == NULL || htr->send == NULL)
		return -EFAULT;

	wake_lock(&bt_drv.wake_lock);
	while (ret >= 0 && len) {
		ret = htr->send(htr, data, len, HCITRFLAG_UBUF);
		if (ret < 0) {
			TR_ERR("send failed to %s\n", hci_trans_get_name(htr));
			slsi_bt_err(SLSI_BT_ERR_SEND_FAIL);
			break;
		}
		bt_trans.send_data_count += ret;
		len -= ret;
	}
	wake_unlock(&bt_drv.wake_lock);
	return count - len;
}

static int slsi_bt_transport_init(unsigned int en_trs)
{
	struct hci_trans *htr = NULL;
	int ret;

	BT_INFO("en_trs: 0x%x\n", en_trs);

	/* Setup the transport layers */
	init_waitqueue_head(&bt_trans.read_wait);
	bt_trans.htr = hci_trans_new("slsi_bt");
	bt_trans.htr->recv_skb = slsi_bt_skb_recv;

#ifdef CONFIG_SLSI_BT_H4
	if (en_trs & SLSI_BT_TR_EN_H4) {
		/* Connect to h4(reverse) */
		htr = hci_trans_new("H4 Transport (revert)");
		hci_trans_add_tail(htr, bt_trans.htr);
		ret = hci_h4_init(htr, true);
		if (ret)
			return ret;
	}
#endif
#ifdef CONFIG_SLSI_BT_BCSP
	if (en_trs & SLSI_BT_TR_EN_BCSP) {
		/* Connect to bcsp */
		htr = hci_trans_new("BCSP Transport");
		hci_trans_add_tail(htr, bt_trans.htr);
		ret = hci_bcsp_init(htr);
		if (ret)
			return ret;
	}
#endif
#ifdef CONFIG_SLSI_BT_USE_UART_INTERFACE
	if (en_trs & SLSI_BT_TR_EN_TTY) {
		/* Connect to controller for sleep/wakeup */
		htr = hci_trans_new("BT Controller Transport");
		hci_trans_add_tail(htr, bt_trans.htr);
		ret = slsi_bt_controller_transport_configure(htr);
		if (ret)
			return ret;

		/* Connect to tty */
		ret = slsi_bt_tty_open();
		if (ret)
			return ret;
		htr = hci_trans_new("tty Transport");
		hci_trans_add_tail(htr, bt_trans.htr);
		ret = slsi_bt_tty_transport_configure(htr);
		if (ret)
			return ret;
	}
#endif
	bt_trans.enabled_trs = en_trs;
	return 0;
}

static void slsi_bt_transport_deinit(void)
{
	struct hci_trans *del = NULL;
	struct list_head *pos = NULL, *tmp = NULL;

#ifdef CONFIG_SLSI_BT_USE_UART_INTERFACE
	/* The lowest layer first to prevent receving */
	if (bt_trans.enabled_trs & SLSI_BT_TR_EN_TTY) {
		slsi_bt_tty_close();
		msleep(100);
	}
#endif

	/* Deinitialize all of transport layer */
	list_for_each_safe(pos, tmp, &bt_trans.htr->list) {
		del = list_entry(pos, struct hci_trans, list);
		hci_trans_del(del);
		BT_DBG("free transport: %s\n", hci_trans_get_name(del));
		hci_trans_free(del);
	}
	bt_trans.htr = NULL;
	bt_trans.enabled_trs = 0;
}

static int slsi_bt_transport_change(unsigned int en_trs)
{
	if (bt_trans.enabled_trs == en_trs) {
		BT_DBG("Same as current enabled trans: %u\n", en_trs);
		return 0;
	} else if (bt_trans.enabled_trs != 0) {
		BT_DBG("Change transport layers, from %u to %u\n",
						bt_trans.enabled_trs, en_trs);
		slsi_bt_transport_deinit();
	}
	return slsi_bt_transport_init(en_trs);
}

static int slsi_bt_open(struct inode *inode, struct file *file)
{
	int users, ret = 0;

	BT_INFO("(open_status=%u)\n", bt_drv.open_status ? 1 : 0);
	/*
	 * Only 1 user allowed,
	 * but this driver allows the multi users when it open same time such
	 * as using pipe and redirection operator.
	 * TODO: This is support multi-users by suing the slow opening
	 * operation. It needs to be improved.
	 */
	if (bt_drv.open_status)
		return -EBUSY;

	wake_lock(&bt_drv.wake_lock);
	mutex_lock(&bt_start_mutex);
	atomic_inc(&bt_drv.users);
	users = atomic_read(&bt_drv.users);
	if (users > 1) {
		BT_DBG("success as multiple user: %d\n", users);
		goto out;
	}
	mutex_init(&bt_trans.recv_q_lock);
	skb_queue_head_init(&bt_trans.recv_q);

	/* Error manager */
	slsi_bt_err_init(slsi_bt_error_handle);

	/* Enable BT Controller */
	ret = slsi_bt_controller_start();
	if (ret != 0) {
		BT_ERR("open failed to start controller: %d\n", ret);
		goto out;
	}

	/* Configure Transpoters */
	ret = slsi_bt_transport_change(SLSI_BT_TR_EN_DEFAULT);
	if (ret) {
		BT_ERR("open failed to init transports: %d\n", ret);
		slsi_bt_transport_deinit();
		slsi_bt_controller_stop();
		goto out;
	}

	/* Init own context */
	bt_trans.recv_pkt_count = 0;
	bt_trans.send_data_count = 0;
	bt_trans.recv_data_count = 0;
	bt_drv.open_status = true;

	slsi_bt_log_set_transport((void*)bt_trans.htr);
	BT_INFO("success\n");
out:
	if (ret)
		atomic_dec(&bt_drv.users);
	mutex_unlock(&bt_start_mutex);
	wake_unlock(&bt_drv.wake_lock);
	return ret;
}

static int slsi_bt_release(struct inode *inode, struct file *file)
{
	BT_INFO("users=%d\n", atomic_read(&bt_drv.users));
	if (!atomic_dec_and_test(&bt_drv.users))
		goto out;

	wake_lock(&bt_drv.wake_lock);
	mutex_lock(&bt_start_mutex);
	BT_DBG("pending recv_q=%d, en_trs=%u\n",
		skb_queue_len(&bt_trans.recv_q), bt_trans.enabled_trs);

	/* step1. stop the data flow to avoid receiving any data */
	slsi_bt_transport_deinit();
	slsi_bt_controller_stop();

	/* step2. clear the remain data */
	skb_queue_purge(&bt_trans.recv_q);
	mutex_destroy(&bt_trans.recv_q_lock);
	wake_up(&bt_trans.read_wait);

	bt_drv.open_status = false;
	mutex_unlock(&bt_start_mutex);
	wake_unlock(&bt_drv.wake_lock);
	if (wake_lock_active(&bt_drv.read_wake_lock))
		wake_unlock(&bt_drv.read_wake_lock);
	slsi_bt_err_deinit();
out:
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
		ret = slsi_bt_transport_change((unsigned int)arg);
		if (ret) {
			BT_ERR("failed to change transports: %d\n", ret);
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
	struct hci_trans *tr = bt_trans.htr;
	int i = 0;

	seq_puts(m, "Driver statistics:\n");
	seq_printf(m, "  open status            = %d\n", bt_drv.open_status);
	for (tr = bt_trans.htr; tr != NULL; tr = hci_trans_get_next(tr), i++)
		seq_printf(m, "  Transporter Stack(%d)   = %s\n", i,
			hci_trans_get_name(tr));

	if (bt_trans.htr->name) {
		seq_printf(m, "\n  %s:\n", hci_trans_get_name(bt_trans.htr));
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
	for (tr = bt_trans.htr; tr != NULL; tr = hci_trans_get_next(tr))
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

	BT_INFO("%s %s (C) %s\n", SLSI_BT_MODDESC, SLSI_BT_MODVERSION,
		SLSI_BT_MODAUTH);

	memset(&bt_drv, 0, sizeof(bt_drv));
	atomic_set(&bt_drv.users, 0);

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
	bt_drv.procfs_dir = proc_mkdir(SLSI_BT_PROC_DIR, NULL);
	if (NULL != bt_drv.procfs_dir) {
		proc_create_data("stats", S_IRUSR | S_IRGRP, bt_drv.procfs_dir,
				 &scsc_bt_procfs_fops, NULL);
	}

	wake_lock_init(NULL, &bt_drv.wake_lock.ws, "bt_drv_wake_lock");
	wake_lock_init(NULL, &bt_drv.read_wake_lock.ws, "bt_read_wake_lock");

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
	BT_INFO("success. cdev=%p class=%p\n", bt_drv.device, bt_drv.class);

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

	slsi_bt_log_set_transport(NULL);
#ifdef CONFIG_SLSI_BT_USE_UART_INTERFACE
	slsi_bt_tty_exit();
#endif
	slsi_bt_controller_exit();
	wake_lock_destroy(&bt_drv.wake_lock);
	wake_lock_destroy(&bt_drv.read_wake_lock);

	remove_proc_entry("stats", bt_drv.procfs_dir);
	remove_proc_entry(SLSI_BT_PROC_DIR, NULL);

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
