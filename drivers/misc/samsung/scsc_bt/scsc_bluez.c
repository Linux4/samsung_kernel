/****************************************************************************
 *
 * Copyright (c) 2014 - 2016 Samsung Electronics Co., Ltd. All rights reserved
 *
 * BT BlueZ interface
 *
 ****************************************************************************/

#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/types.h>
#include <linux/fs.h>
#include <linux/module.h>
#include <linux/firmware.h>
#include <linux/poll.h>
#include <linux/slab.h>
#include <linux/cdev.h>
#include <linux/device.h>
#include <linux/kthread.h>
#include <linux/proc_fs.h>
#include <asm/io.h>
#include <asm/termios.h>
#include <linux/wakelock.h>
#include <linux/delay.h>
#include <linux/skbuff.h>
#include <linux/rfkill.h>
#include <linux/platform_device.h>

#include <net/bluetooth/bluetooth.h>
#include <net/bluetooth/hci_core.h>

#include <scsc/scsc_logring.h>

static struct sk_buff_head 		txq;
static struct hci_dev      		*hdev;
static struct device 			*dev_ref;
static const struct file_operations 	*bt_fs;
static struct workqueue_struct          *wq;
static struct work_struct		open_worker;
static struct work_struct		close_worker;
static struct work_struct		read_work;
static u8				receive_buffer[1024];
static bool	                 	terminate_read;
static atomic_t 			*error_count_ref;
static wait_queue_head_t       		*read_wait_ref;
static struct file			s_file;

static struct platform_device *slsi_btz_pdev;
static struct rfkill *btz_rfkill;

static int slsi_bt_open(struct hci_dev *hdev)
{
	int err;

	SCSC_TAG_INFO(BT_COMMON, "enter\n");

	err = bt_fs->open(NULL, NULL);

	if (0 == err) {
		terminate_read = false;
		queue_work(wq, &read_work);
	}

	SCSC_TAG_INFO(BT_COMMON, "done\n");

	return err;
}

static int slsi_bt_close(struct hci_dev *hdev)
{
	int ret;

	SCSC_TAG_INFO(BT_COMMON, "terminating reader thread\n");

	terminate_read = true;
	atomic_inc(error_count_ref);
	wake_up(read_wait_ref);
	cancel_work_sync(&read_work);

	SCSC_TAG_INFO(BT_COMMON, "releasing service\n");

	ret = bt_fs->release(NULL, NULL);

	SCSC_TAG_INFO(BT_COMMON, "done\n");

	return ret;
}

static int slsi_bt_flush(struct hci_dev *hdev)
{
	skb_queue_purge(&txq);
	return 0;
}

static int slsi_bt_send_frame(struct hci_dev *hdev, struct sk_buff *skb)
{
	struct sk_buff *transfer_skb;
	int err;

	memcpy(skb_push(skb, 1), &bt_cb(skb)->pkt_type, 1);
	skb_queue_tail(&txq, skb);

	transfer_skb = skb_dequeue(&txq);
	if (transfer_skb) {
		SCSC_TAG_INFO(BT_H4, "sending frame(data=%p, len=%u)\n", skb->data, skb->len);

		err = bt_fs->write(NULL, skb->data, skb->len, NULL);
		kfree_skb(transfer_skb);
	}

	return err;
}

static void slsi_bt_fs_read_func(struct work_struct *work)
{
	int ret;
	struct sk_buff *skb = NULL;

	while ((ret = bt_fs->read(&s_file, receive_buffer, sizeof(receive_buffer), NULL)) >= 0) {
		if (terminate_read)
			break;

		if (ret > 0) {
			skb = bt_skb_alloc(ret, GFP_ATOMIC);
			if (!skb)
				SCSC_TAG_ERR(BT_COMMON, "bt_skb_alloc failed\n");

			skb->dev = (void *) hdev;
			bt_cb(skb)->pkt_type = receive_buffer[0];
			memcpy(skb_put(skb, ret - 1), &receive_buffer[1], ret - 1);

			ret = hci_recv_frame(hdev, skb);
			if (ret < 0) {
				SCSC_TAG_ERR(BT_COMMON, "unable to send skb to HCI layer\n");
				/* skb freed in HCI layer */
				skb = NULL;
			}
		}
	}

	SCSC_TAG_INFO(BT_COMMON, "BT BlueZ: Exiting %s \n", __func__);
}

static void slsi_bt_open_worker(struct work_struct *work)
{
	int err;

	if (!hdev) {
		hdev = hci_alloc_dev();
		if (!hdev) {
			SCSC_TAG_ERR(BT_COMMON, "failed to allocate hci device\n");
			return;
		}

		hdev->bus = HCI_VIRTUAL;
		hdev->dev_type = HCI_BREDR;

		SET_HCIDEV_DEV(hdev, dev_ref);

		hdev->open     = slsi_bt_open;
		hdev->close    = slsi_bt_close;
		hdev->flush    = slsi_bt_flush;
		hdev->send     = slsi_bt_send_frame;

		err = hci_register_dev(hdev);
		if (err < 0) {
			SCSC_TAG_ERR(BT_COMMON, "failed to register hci device (err: %d)\n", err);
			hci_free_dev(hdev);
			hdev = NULL;
		}
	}
}

static void slsi_bt_close_worker(struct work_struct *work)
{
	if (hdev) {
		hci_unregister_dev(hdev);
		hci_free_dev(hdev);
		hdev = NULL;
	}
}

void slsi_bt_notify_probe(struct device *dev,
			  const struct file_operations *fs,
			  atomic_t *error_count,
			  wait_queue_head_t *read_wait)
{
	bt_fs           = fs;
	error_count_ref = error_count;
	read_wait_ref   = read_wait;
	dev_ref         = dev;

	skb_queue_head_init(&txq);

	wq = create_singlethread_workqueue("slsi_bt_bluez_wq");
	INIT_WORK(&read_work, slsi_bt_fs_read_func);

	SCSC_TAG_INFO(BT_COMMON, "SLSI BT BlueZ probe\n");
}

void slsi_bt_notify_remove(void)
{
	error_count_ref = NULL;
	read_wait_ref   = NULL;
	dev_ref         = NULL;
}

static int slsi_bt_power_control_set_param_cb(const char *buffer,
					      const struct kernel_param *kp)
{
	int ret;
	u32 value;

	ret = kstrtou32(buffer, 0, &value);
	if (!ret) {
		if (value && dev_ref) {
			INIT_WORK(&open_worker, slsi_bt_open_worker);
			schedule_work(&open_worker);
		} else if (0 == value) {
			INIT_WORK(&close_worker, slsi_bt_close_worker);
			schedule_work(&close_worker);
		}
	}

	return ret;
}

static int slsi_bt_power_control_get_param_cb(char *buffer,
					      const struct kernel_param *kp)
{
	return sprintf(buffer, "%u\n", (hdev != NULL ? 1 : 0));
}

static struct kernel_param_ops slsi_bt_power_control_ops = {
	.set = slsi_bt_power_control_set_param_cb,
	.get = slsi_bt_power_control_get_param_cb,
};

module_param_cb(power_control, &slsi_bt_power_control_ops, NULL, S_IRUGO | S_IWUSR);
MODULE_PARM_DESC(power_control,
		 "Enables/disable BlueZ registration");

static int btz_rfkill_set(void *data, bool blocked)
{
	if (!blocked && dev_ref) {
		INIT_WORK(&open_worker, slsi_bt_open_worker);
		schedule_work(&open_worker);
	} else if (blocked) {
		INIT_WORK(&close_worker, slsi_bt_close_worker);
		schedule_work(&close_worker);
	}

	return 0;
}

static const struct rfkill_ops btz_rfkill_ops = {
       .set_block = btz_rfkill_set,
};

static int __init slsi_bluez_init(void)
{
	int ret;

	slsi_btz_pdev = platform_device_alloc("scsc-bluez", -1);
	if (!slsi_btz_pdev) {
		return -ENOMEM;
	}

	ret = platform_device_add(slsi_btz_pdev);
	if (ret) {
		goto err_slsi_btz_pdev;
	}

	btz_rfkill = rfkill_alloc("scsc-bluez-rfkill", &slsi_btz_pdev->dev,
								RFKILL_TYPE_BLUETOOTH, &btz_rfkill_ops, NULL);
	if (!btz_rfkill) {
		goto err_btz_rfkill_alloc;
	}

	rfkill_init_sw_state(btz_rfkill, 1);

	ret = rfkill_register(btz_rfkill);
	if (ret) {
		goto err_btz_rfkill_reg;
	}

	return 0;

err_btz_rfkill_reg:
	rfkill_destroy(btz_rfkill);

err_btz_rfkill_alloc:
	platform_device_del(slsi_btz_pdev);

err_slsi_btz_pdev:
	platform_device_put(slsi_btz_pdev);

	return ret;
}

static void __exit slsi_bluez_exit(void)
{
	platform_device_unregister(slsi_btz_pdev);
	rfkill_unregister(btz_rfkill);
	rfkill_destroy(btz_rfkill);
}

module_init(slsi_bluez_init);
module_exit(slsi_bluez_exit);

MODULE_DESCRIPTION("SCSC BT Bluez");
MODULE_AUTHOR("SLSI");
MODULE_LICENSE("GPL and additional rights");
