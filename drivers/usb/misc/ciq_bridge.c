/*
 * Copyright (c) 2012, The Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

/* add additional information to our printk's */
#define pr_fmt(fmt) "%s: " fmt "\n", __func__

#include <linux/kernel.h>
#include <linux/errno.h>
#include <linux/init.h>
#include <linux/slab.h>
#include <linux/module.h>
#include <linux/kref.h>
#include <linux/platform_device.h>
#include <linux/ratelimit.h>
#include <linux/uaccess.h>
#include <linux/usb.h>
#include <linux/debugfs.h>
#include <linux/seq_file.h>
#include <linux/miscdevice.h>
#include <linux/list.h>
#include <linux/wait.h>
#include <linux/poll.h>

#define DRIVER_DESC	"USB host ciq bridge driver"
#define DRIVER_VERSION	"1.0"

struct data_pkt {
	int			n_read;
	char			*buf;
	size_t			len;
	struct list_head	list;
	void			*ctxt;
};

#define FILE_OPENED		BIT(0)
#define USB_DEV_CONNECTED	BIT(1)
#define NO_RX_REQS		10
#define MAX_DATA_PKT_SIZE	16384
#define PENDING_URB_TIMEOUT	10

struct ciq_bridge {
	char			*name;
	spinlock_t		lock;
	struct workqueue_struct	*wq;
	struct work_struct	to_mdm_work;
	struct work_struct	start_rx_work;
	struct list_head	to_mdm_list;
	struct list_head	to_ciq_list;
	wait_queue_head_t	ciq_wait_q;
	wait_queue_head_t	pending_urb_wait;
	struct miscdevice	*fs_dev;
	atomic_t		tx_pending_cnt;
	atomic_t		rx_pending_cnt;
#if defined(CONFIG_MACH_TRLTE_ATT)
	atomic_t		pm_cnt;
#endif

	/* usb specific */
	struct usb_device	*udev;
	struct usb_interface	*ifc;
	__u8			in_epAddr;
	__u8			out_epAddr;
	unsigned int		in_pipe;
	unsigned int		out_pipe;
	struct usb_anchor	submitted;

	unsigned long		flags;

#define DBG_MSG_LEN   40
#define DBG_MAX_MSG   500
	unsigned int	dbg_idx;
	rwlock_t	dbg_lock;
	char     (dbgbuf[DBG_MAX_MSG])[DBG_MSG_LEN];   /* buffer */

	atomic_t pmlock_cnt;
};

struct ciq_bridge *ciq;

/* by default debugging is enabled */
static unsigned int enable_dbg = 1;
module_param(enable_dbg, uint, S_IRUGO | S_IWUSR);

static void
dbg_log_event(struct ciq_bridge *ciq, char *event, int d1, int d2)
{
	unsigned long flags;
	unsigned long long t;
	unsigned long nanosec;

	if (!enable_dbg)
		return;

	write_lock_irqsave(&ciq->dbg_lock, flags);
	t = cpu_clock(smp_processor_id());
	nanosec = do_div(t, 1000000000)/1000;
	scnprintf(ciq->dbgbuf[ciq->dbg_idx], DBG_MSG_LEN, "%5lu.%06lu:%s:%x:%x",
			(unsigned long)t, nanosec, event, d1, d2);

	ciq->dbg_idx++;
	ciq->dbg_idx = ciq->dbg_idx % DBG_MAX_MSG;
	write_unlock_irqrestore(&ciq->dbg_lock, flags);
}

static
struct data_pkt *ciq_alloc_data_pkt(size_t count, gfp_t flags, void *ctxt)
{
	struct data_pkt *pkt;

	pkt = kzalloc(sizeof(struct data_pkt), flags);
	if (!pkt) {
		pr_err("failed to allocate data packet\n");
		return ERR_PTR(-ENOMEM);
	}

	pkt->buf = kmalloc(count, flags);
	if (!pkt->buf) {
		pr_err("failed to allocate data buffer\n");
		kfree(pkt);
		return ERR_PTR(-ENOMEM);
	}

	pkt->len = count;
	INIT_LIST_HEAD(&pkt->list);
	pkt->ctxt = ctxt;

	return pkt;
}

static void ciq_free_data_pkt(struct data_pkt *pkt)
{
	kfree(pkt->buf);
	kfree(pkt);
}

static unsigned int ciq_fs_poll(struct file *fp, poll_table *wait)
{
	struct ciq_bridge *ciq = fp->private_data;
	unsigned int mask = 0;

	pr_debug("Enter %s\n", __func__);

	if (!ciq)
		return POLLERR;

	poll_wait(fp, &ciq->ciq_wait_q, wait);

	if (!test_bit(USB_DEV_CONNECTED, &ciq->flags)) {
		pr_err( "%s: Device not connected\n",__func__);
		return POLLERR;
	}

	if (!list_empty(&ciq->to_ciq_list)) {
		mask |= POLLIN | POLLRDNORM;
	}

	pr_debug("%s return = %d\n", __func__, mask);
	return mask;

}

static void
submit_one_urb(struct ciq_bridge *ciq, gfp_t flags, struct data_pkt *pkt);
static ssize_t ciq_fs_read(struct file *fp, char __user *buf,
				size_t count, loff_t *pos)
{
	int ret;
	unsigned long flags;
	struct ciq_bridge *ciq = fp->private_data;
	struct data_pkt *pkt = NULL;
	size_t space, copied;

	pr_debug("Enter %s\n", __func__);

	if (!test_bit(USB_DEV_CONNECTED, &ciq->flags))
		return -ENODEV;

	spin_lock_irqsave(&ciq->lock, flags);

	space = count;
	copied = 0;
	while (!list_empty(&ciq->to_ciq_list) && space &&
			test_bit(USB_DEV_CONNECTED, &ciq->flags)) {
		size_t len;

		pkt = list_first_entry(&ciq->to_ciq_list, struct data_pkt, list);
		list_del_init(&pkt->list);
		len = min_t(size_t, space, pkt->len - pkt->n_read);
		spin_unlock_irqrestore(&ciq->lock, flags);

		ret = copy_to_user(buf + copied, pkt->buf + pkt->n_read, len);
		if (ret) {
			pr_err("copy_to_user failed err:%d\n", ret);
			ciq_free_data_pkt(pkt);
			return -EFAULT;
		}

		pkt->n_read += len;
		space -= len;
		copied += len;

		if (pkt->n_read == pkt->len) {
			/*
			 * re-init the packet and queue it
			 * for more data.
			 */
			pkt->n_read = 0;
			pkt->len = MAX_DATA_PKT_SIZE;
			submit_one_urb(ciq, GFP_KERNEL, pkt);
			pkt = NULL;
		}
		spin_lock_irqsave(&ciq->lock, flags);
	}

	/* put the partial packet back in the list */
	if (!space && pkt && pkt->n_read != pkt->len) {
		if (test_bit(USB_DEV_CONNECTED, &ciq->flags))
			list_add(&pkt->list, &ciq->to_ciq_list);
		else
			ciq_free_data_pkt(pkt);
	}
	spin_unlock_irqrestore(&ciq->lock, flags);

	dbg_log_event(ciq, "CIQ_READ", copied, 0);

	pr_info("count:%d space:%d copied:%d", count, space, copied);

	return copied;
}

static void ciq_tx_cb(struct urb *urb)
{
	struct data_pkt *pkt = urb->context;
	struct ciq_bridge *ciq = pkt->ctxt;

	dbg_log_event(ciq, "C TX_URB", urb->status, 0);
	pr_debug("status:%d", urb->status);

	if (test_bit(USB_DEV_CONNECTED, &ciq->flags))
		usb_autopm_put_interface_async(ciq->ifc);

	if (urb->status < 0)
		pr_err_ratelimited("urb failed with err:%d", urb->status);

	ciq_free_data_pkt(pkt);

	atomic_dec(&ciq->tx_pending_cnt);
	wake_up(&ciq->pending_urb_wait);
}

static void ciq_tomdm_work(struct work_struct *w)
{
	struct ciq_bridge *ciq = container_of(w, struct ciq_bridge, to_mdm_work);
	struct data_pkt	*pkt;
	unsigned long flags;
	struct urb *urb;
	int ret;

	spin_lock_irqsave(&ciq->lock, flags);
	while (!list_empty(&ciq->to_mdm_list)
			&& test_bit(USB_DEV_CONNECTED, &ciq->flags)) {
		pkt = list_first_entry(&ciq->to_mdm_list,
				struct data_pkt, list);
		list_del_init(&pkt->list);
		spin_unlock_irqrestore(&ciq->lock, flags);

		urb = usb_alloc_urb(0, GFP_KERNEL);
		if (!urb) {
			pr_err_ratelimited("unable to allocate urb");
			ciq_free_data_pkt(pkt);
			return;
		}

		ret = usb_autopm_get_interface(ciq->ifc);
		if (ret < 0 && ret != -EAGAIN && ret != -EACCES) {
			pr_err_ratelimited("autopm_get failed:%d", ret);
			usb_free_urb(urb);
			ciq_free_data_pkt(pkt);
			return;
		}
		usb_fill_bulk_urb(urb, ciq->udev, ciq->out_pipe,
				pkt->buf, pkt->len, ciq_tx_cb, pkt);
		usb_anchor_urb(urb, &ciq->submitted);

		dbg_log_event(ciq, "S TX_URB", pkt->len, 0);

		atomic_inc(&ciq->tx_pending_cnt);
		ret = usb_submit_urb(urb, GFP_KERNEL);
		if (ret) {
			pr_err("out urb submission failed");
			usb_unanchor_urb(urb);
			usb_free_urb(urb);
			ciq_free_data_pkt(pkt);
			usb_autopm_put_interface(ciq->ifc);
			atomic_dec(&ciq->tx_pending_cnt);
			wake_up(&ciq->pending_urb_wait);
			return;
		}

		usb_free_urb(urb);

		spin_lock_irqsave(&ciq->lock, flags);
	}
	spin_unlock_irqrestore(&ciq->lock, flags);
}

static ssize_t ciq_fs_write(struct file *fp, const char __user *buf,
				 size_t count, loff_t *pos)
{
	int			ret;
	struct data_pkt		*pkt;
	unsigned long		flags;
	struct ciq_bridge	*ciq = fp->private_data;

        if (!test_bit(USB_DEV_CONNECTED, &ciq->flags)) {
		pr_err("USB_DEV_CONNECTED is not set");
		return -ENODEV;
	}

	if (count > MAX_DATA_PKT_SIZE)
		count = MAX_DATA_PKT_SIZE;

	pr_info("count:%d cmd:%d", count, *buf);

	pkt = ciq_alloc_data_pkt(count, GFP_KERNEL, ciq);
	if (IS_ERR(pkt)) {
		pr_err("unable to allocate data packet");
		return PTR_ERR(pkt);
	}

	ret = copy_from_user(pkt->buf, buf, count);
	if (ret) {
		pr_err("copy_from_user failed: err:%d", ret);
		ciq_free_data_pkt(pkt);
		return ret;
	}

	spin_lock_irqsave(&ciq->lock, flags);
	list_add_tail(&pkt->list, &ciq->to_mdm_list);
	spin_unlock_irqrestore(&ciq->lock, flags);

	queue_work(ciq->wq, &ciq->to_mdm_work);

	return count;
}



static int ciq_fs_open(struct inode *ip, struct file *fp)
{

	pr_info(":%s", ciq->name);

	dbg_log_event(ciq, "ciq-FS-OPEN", 0, 0);

	fp->private_data = ciq;
	set_bit(FILE_OPENED, &ciq->flags);

	if (test_bit(USB_DEV_CONNECTED, &ciq->flags))
		queue_work(ciq->wq, &ciq->start_rx_work);

	return 0;
}

static int ciq_fs_release(struct inode *ip, struct file *fp)
{
	struct ciq_bridge	*ciq = fp->private_data;

	pr_debug(":%s", ciq->name);
	dbg_log_event(ciq, "FS-RELEASE", 0, 0);

	clear_bit(FILE_OPENED, &ciq->flags);
	fp->private_data = NULL;

	return 0;
}

static const struct file_operations ciq_fops = {
	.owner = THIS_MODULE,
	.read = ciq_fs_read,
	.write = ciq_fs_write,
	.open = ciq_fs_open,
	.release = ciq_fs_release,
	.poll =ciq_fs_poll,
};

static struct miscdevice ciq_dev = {
	.minor = MISC_DYNAMIC_MINOR,
	.name = "smd4_bridge",
	.fops = &ciq_fops,
};




static const struct usb_device_id ciq_usb_ids[] = {
	{ USB_DEVICE(0x5c6, 0x9048),
	.driver_info = (unsigned long)&ciq_dev, },
	{ USB_DEVICE(0x5c6, 0x904C),
	.driver_info = (unsigned long)&ciq_dev, },
	{ USB_DEVICE(0x5c6, 0x9075),
	.driver_info = (unsigned long)&ciq_dev, },
	{ USB_DEVICE(0x5c6, 0x90A4),
	.driver_info = (unsigned long)&ciq_dev, },
	{} /* terminating entry */
};
MODULE_DEVICE_TABLE(usb, ciq_usb_ids);

static void ciq_rx_cb(struct urb *urb);
static void
submit_one_urb(struct ciq_bridge *ciq, gfp_t flags, struct data_pkt *pkt)
{
	struct urb *urb;
	int ret;

	urb = usb_alloc_urb(0, flags);
	if (!urb) {
		pr_err("unable to allocate urb");
		ciq_free_data_pkt(pkt);
		return;
	}

	usb_fill_bulk_urb(urb, ciq->udev, ciq->in_pipe,
			pkt->buf, pkt->len,
			ciq_rx_cb, pkt);
	usb_anchor_urb(urb, &ciq->submitted);

	if (!test_bit(USB_DEV_CONNECTED, &ciq->flags)) {
		usb_unanchor_urb(urb);
		usb_free_urb(urb);
		ciq_free_data_pkt(pkt);
		return;
	}

	atomic_inc(&ciq->rx_pending_cnt);
	ret = usb_submit_urb(urb, flags);
	if (ret) {
		pr_err("in urb submission failed");
		usb_unanchor_urb(urb);
		usb_free_urb(urb);
		ciq_free_data_pkt(pkt);
		atomic_dec(&ciq->rx_pending_cnt);
		wake_up(&ciq->pending_urb_wait);
		return;
	}

	dbg_log_event(ciq, "S RX_URB", pkt->len, 0);

	usb_free_urb(urb);
}
static void ciq_rx_cb(struct urb *urb)
{
	struct data_pkt *pkt = urb->context;
	struct ciq_bridge *ciq = pkt->ctxt;
	bool wakeup = true;

	dbg_log_event(ciq, "C RX_URB", urb->status, urb->actual_length);

	pr_debug("status:%d actual:%d", urb->status, urb->actual_length);

	/*non zero len of data received while unlinking urb*/
	if (urb->status == -ENOENT && (urb->actual_length > 0)) {
		/*
		 * If we wakeup the reader process now, it may
		 * queue the URB before its reject flag gets
		 * cleared.
		 */
		wakeup = false;
		goto add_to_list;
	}

	if (urb->status < 0) {
		if (urb->status != -ESHUTDOWN && urb->status != -ENOENT
				&& urb->status != -EPROTO)
			pr_err_ratelimited("urb failed with err:%d",
					urb->status);
		ciq_free_data_pkt(pkt);
		goto done;
	}

	if (urb->actual_length == 0) {
		submit_one_urb(ciq, GFP_ATOMIC, pkt);
		goto done;
	}

add_to_list:
	spin_lock(&ciq->lock);
	pkt->len = urb->actual_length;
	list_add_tail(&pkt->list, &ciq->to_ciq_list);
	spin_unlock(&ciq->lock);
	/* wake up read thread */
	if (wakeup)
	wake_up(&ciq->ciq_wait_q);
done:
	atomic_dec(&ciq->rx_pending_cnt);
	wake_up(&ciq->pending_urb_wait);
}

static void ciq_start_rx_work(struct work_struct *w)
{
	struct ciq_bridge *ciq =
			container_of(w, struct ciq_bridge, start_rx_work);
	struct data_pkt	*pkt;
	struct urb *urb;
	int i = 0;
	int ret;
	bool put = true;

	ret = usb_autopm_get_interface(ciq->ifc);
	if (ret < 0) {
		if (ret != -EAGAIN && ret != -EACCES) {
			pr_err_ratelimited("autopm_get failed:%d", ret);
			return;
		}
		put = false;
	}
	for (i = 0; i < NO_RX_REQS; i++) {

		if (!test_bit(USB_DEV_CONNECTED, &ciq->flags))
			break;

		pkt = ciq_alloc_data_pkt(MAX_DATA_PKT_SIZE, GFP_KERNEL, ciq);
		if (IS_ERR(pkt)) {
			pr_err("unable to allocate data pkt");
			break;
		}

		urb = usb_alloc_urb(0, GFP_KERNEL);
		if (!urb) {
			pr_err("unable to allocate urb");
			ciq_free_data_pkt(pkt);
			break;
		}

		usb_fill_bulk_urb(urb, ciq->udev, ciq->in_pipe,
				pkt->buf, pkt->len,
				ciq_rx_cb, pkt);
		usb_anchor_urb(urb, &ciq->submitted);

		dbg_log_event(ciq, "S RX_URB", pkt->len, 0);

		atomic_inc(&ciq->rx_pending_cnt);
		ret = usb_submit_urb(urb, GFP_KERNEL);
		if (ret) {
			pr_err("in urb submission failed");
			usb_unanchor_urb(urb);
			usb_free_urb(urb);
			ciq_free_data_pkt(pkt);
			atomic_dec(&ciq->rx_pending_cnt);
			wake_up(&ciq->pending_urb_wait);
			break;
		}

		usb_free_urb(urb);
	}
	if (put)
		usb_autopm_put_interface_async(ciq->ifc);
}

static int
ciq_usb_probe(struct usb_interface *ifc, const struct usb_device_id *id)
{
	__u8				ifc_num;
	struct usb_host_interface	*ifc_desc;
	struct usb_endpoint_descriptor	*ep_desc;
	int				i;
	unsigned long			flags;
	struct data_pkt			*pkt;

	ifc_num = ifc->cur_altsetting->desc.bInterfaceNumber;

	switch (id->idProduct) {
	case 0x9048:
	case 0x904C:
	case 0x9075:
	case 0x90A4:
		if (ifc_num != 6)
			return -ENODEV;
		break;
	default:
		return -ENODEV;
	}

	ciq->udev = usb_get_dev(interface_to_usbdev(ifc));
	ciq->ifc = ifc;
	ifc_desc = ifc->cur_altsetting;

	for (i = 0; i < ifc_desc->desc.bNumEndpoints; i++) {
		ep_desc = &ifc_desc->endpoint[i].desc;

		if (!ciq->in_epAddr && usb_endpoint_is_bulk_in(ep_desc))
			ciq->in_epAddr = ep_desc->bEndpointAddress;

		if (!ciq->out_epAddr && usb_endpoint_is_bulk_out(ep_desc))
			ciq->out_epAddr = ep_desc->bEndpointAddress;
	}

	if (!(ciq->in_epAddr && ciq->out_epAddr)) {
		pr_err("could not find bulk in and bulk out endpoints");
		usb_put_dev(ciq->udev);
		ciq->ifc = NULL;
		return -ENODEV;
	}

	ciq->in_pipe = usb_rcvbulkpipe(ciq->udev, ciq->in_epAddr);
	ciq->out_pipe = usb_sndbulkpipe(ciq->udev, ciq->out_epAddr);

	usb_set_intfdata(ifc, ciq);
	set_bit(USB_DEV_CONNECTED, &ciq->flags);
	atomic_set(&ciq->tx_pending_cnt, 0);
	atomic_set(&ciq->rx_pending_cnt, 0);
#if defined(CONFIG_MACH_TRLTE_ATT)
	atomic_set(&ciq->pm_cnt, 0);
#endif

	dbg_log_event(ciq, "PID-ATT", id->idProduct, 0);

	/*free up stale buffers if any from previous disconnect*/
	spin_lock_irqsave(&ciq->lock, flags);
	while (!list_empty(&ciq->to_ciq_list)) {
		pkt = list_first_entry(&ciq->to_ciq_list,
				struct data_pkt, list);
		list_del_init(&pkt->list);
		ciq_free_data_pkt(pkt);
	}
	while (!list_empty(&ciq->to_mdm_list)) {
		pkt = list_first_entry(&ciq->to_mdm_list,
				struct data_pkt, list);
		list_del_init(&pkt->list);
		ciq_free_data_pkt(pkt);
	}
	spin_unlock_irqrestore(&ciq->lock, flags);

	ciq->fs_dev = (struct miscdevice *)id->driver_info;
	misc_register(ciq->fs_dev);

	if (device_can_wakeup(&ciq->udev->dev)) {
		ifc->needs_remote_wakeup = 1;
		usb_enable_autosuspend(ciq->udev);
	}
        atomic_set(&ciq->pmlock_cnt, 0);

	pr_info("usb ciq dev connected");

	return 0;
}

static int ciq_usb_suspend(struct usb_interface *ifc, pm_message_t message)
{
	struct ciq_bridge *ciq = usb_get_intfdata(ifc);
	unsigned long flags;

	dbg_log_event(ciq, "SUSPEND", 0, 0);

#if defined(CONFIG_MACH_TRLTE_ATT)
	pr_info("%s", __func__);
#else
	pr_debug("%s", __func__);
#endif

	usb_kill_anchored_urbs(&ciq->submitted);

	spin_lock_irqsave(&ciq->lock, flags);

	if (!list_empty(&ciq->to_ciq_list)) {
		spin_unlock_irqrestore(&ciq->lock, flags);
		dbg_log_event(ciq, "SUSPEND ABORT", 0, 0);
		/*
		 * Now wakeup the reader process and queue
		 * Rx URBs for more data.
		 */
		wake_up(&ciq->ciq_wait_q);
		queue_work(ciq->wq, &ciq->start_rx_work);
		return -EBUSY;
	}

#if defined(CONFIG_MACH_TRLTE_ATT)
	if (atomic_inc_return(&ciq->pm_cnt) > 1) {
		pr_err("%s: pm_cnt : %d", __func__, atomic_read(&ciq->pm_cnt));
		panic("wrong ciq pm_cnt");
	}
#endif
	spin_unlock_irqrestore(&ciq->lock, flags);

	return 0;
}

static int ciq_usb_resume(struct usb_interface *ifc)
{
	struct ciq_bridge *ciq = usb_get_intfdata(ifc);
#if defined(CONFIG_MACH_TRLTE_ATT)
	unsigned long flags;
#endif

	dbg_log_event(ciq, "RESUME", 0, 0);

#if defined(CONFIG_MACH_TRLTE_ATT)
        pr_info("%s", __func__);

	spin_lock_irqsave(&ciq->lock, flags);
	if (!atomic_dec_and_test(&ciq->pm_cnt)) {
		pr_err("%s: pm_cnt: %d", __func__, atomic_read(&ciq->pm_cnt));
		panic("wrong ciq pm_cnt");
	}
	spin_unlock_irqrestore(&ciq->lock, flags);
#endif
        pr_debug("%s", __func__);

	if (test_bit(FILE_OPENED, &ciq->flags))
		queue_work(ciq->wq, &ciq->start_rx_work);

	return 0;
}

static void ciq_usb_disconnect(struct usb_interface *ifc)
{
	struct ciq_bridge *ciq = usb_get_intfdata(ifc);
	unsigned long flags;
	struct data_pkt *pkt;

	dbg_log_event(ciq, "PID-DETACH", 0, 0);

	clear_bit(USB_DEV_CONNECTED, &ciq->flags);
	wake_up(&ciq->ciq_wait_q);
	cancel_work_sync(&ciq->to_mdm_work);
	cancel_work_sync(&ciq->start_rx_work);

	misc_deregister(ciq->fs_dev);

	usb_kill_anchored_urbs(&ciq->submitted);

	wait_event_interruptible_timeout(
					ciq->pending_urb_wait,
					!atomic_read(&ciq->tx_pending_cnt) &&
					!atomic_read(&ciq->rx_pending_cnt),
					msecs_to_jiffies(PENDING_URB_TIMEOUT));

	spin_lock_irqsave(&ciq->lock, flags);
	while (!list_empty(&ciq->to_ciq_list)) {
		pkt = list_first_entry(&ciq->to_ciq_list,
				struct data_pkt, list);
		list_del_init(&pkt->list);
		ciq_free_data_pkt(pkt);
	}
	while (!list_empty(&ciq->to_mdm_list)) {
		pkt = list_first_entry(&ciq->to_mdm_list,
				struct data_pkt, list);
		list_del_init(&pkt->list);
		ciq_free_data_pkt(pkt);
	}
	spin_unlock_irqrestore(&ciq->lock, flags);

	ifc->needs_remote_wakeup = 0;
	usb_put_dev(ciq->udev);
	ciq->ifc = NULL;
	usb_set_intfdata(ifc, NULL);

	return;
}

static struct usb_driver ciq_usb_driver = {
	.name =		"ciq_bridge",
	.probe =	ciq_usb_probe,
	.disconnect =	ciq_usb_disconnect,
	.suspend =	ciq_usb_suspend,
	.resume =	ciq_usb_resume,
	.reset_resume =	ciq_usb_resume,
	.id_table =	ciq_usb_ids,
	.supports_autosuspend = 1,
};

static ssize_t ciq_debug_show(struct seq_file *s, void *unused)
{
	unsigned long		flags;
	struct ciq_bridge	*ciq = s->private;
	int			i;

	read_lock_irqsave(&ciq->dbg_lock, flags);
	for (i = 0; i < DBG_MAX_MSG; i++) {
		if (i == (ciq->dbg_idx - 1))
			seq_printf(s, "-->%s\n", ciq->dbgbuf[i]);
		else
			seq_printf(s, "%s\n", ciq->dbgbuf[i]);
	}
	read_unlock_irqrestore(&ciq->dbg_lock, flags);

	return 0;
}

static int ciq_debug_open(struct inode *ip, struct file *fp)
{
	return single_open(fp, ciq_debug_show, ip->i_private);

	return 0;
}

static const struct file_operations dbg_fops = {
	.open = ciq_debug_open,
	.read = seq_read,
	.llseek = seq_lseek,
	.release = single_release,
};
static struct dentry *dbg_dir;
static int __init ciq_init(void)
{
	int ret = 0;

	dbg_dir = debugfs_create_dir("ciq_bridge", NULL);
	if (IS_ERR(dbg_dir))
		pr_err("unable to create debug dir");

	ciq = kzalloc(sizeof(struct ciq_bridge), GFP_KERNEL);
	if (!ciq) {
		pr_err("unable to allocat mem for ciq_bridge");
		ret =  -ENOMEM;
		goto dev_free;
	}

	ciq->name = kasprintf(GFP_KERNEL, "ciq_bridge:%i", 0);
	if (!ciq->name) {
		pr_info("unable to allocate name");
		kfree(ciq);
		ret = -ENOMEM;
		goto dev_free;
	}

	spin_lock_init(&ciq->lock);
	INIT_LIST_HEAD(&ciq->to_mdm_list);
	INIT_LIST_HEAD(&ciq->to_ciq_list);
	init_waitqueue_head(&ciq->ciq_wait_q);
	init_waitqueue_head(&ciq->pending_urb_wait);
	ciq->wq = create_singlethread_workqueue(ciq->name);
	if (!ciq->wq) {
		pr_err("unable to allocate workqueue");
		kfree(ciq->name);
		kfree(ciq);
		ret = -ENOMEM;
		goto dev_free;
	}

	INIT_WORK(&ciq->to_mdm_work, ciq_tomdm_work);
	INIT_WORK(&ciq->start_rx_work, ciq_start_rx_work);
	init_usb_anchor(&ciq->submitted);

	ciq->dbg_idx = 0;
	ciq->dbg_lock = __RW_LOCK_UNLOCKED(lck);

	if (!IS_ERR(dbg_dir))
		debugfs_create_file(ciq->name, S_IRUGO, dbg_dir,
				ciq, &dbg_fops);

	ret = usb_register(&ciq_usb_driver);
	if (ret) {
		pr_err("unable to register ciq bridge driver");
		goto dev_free;
	}

	pr_info("init done");

	return 0;

dev_free:
	if (!IS_ERR(dbg_dir))
		debugfs_remove_recursive(dbg_dir);

	destroy_workqueue(ciq->wq);
	kfree(ciq->name);
	kfree(ciq);

	return ret;

}

static void __exit ciq_exit(void)
{

	if (!IS_ERR(dbg_dir))
		debugfs_remove_recursive(dbg_dir);

	usb_deregister(&ciq_usb_driver);


	destroy_workqueue(ciq->wq);
	kfree(ciq->name);
	kfree(ciq);

}

module_init(ciq_init);
module_exit(ciq_exit);

MODULE_DESCRIPTION(DRIVER_DESC);
MODULE_VERSION(DRIVER_VERSION);
MODULE_LICENSE("GPL v2");
