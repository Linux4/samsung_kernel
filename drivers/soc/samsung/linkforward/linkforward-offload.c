/*
 * Copyright (c) 2019 Samsung Electronics Co., Ltd.
 *		http://www.samsung.com/
 *
 * EXYNOS DIT(Direct IP Translator) Driver support
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/poll.h>
#include <linux/miscdevice.h>
#include <linux/if_ether.h>
#include <linux/netdevice.h>
#include <linux/linkforward.h>
#include <net/net_namespace.h>
#include "linkforward-ioctl.h"
#include "linkforward-offload.h"

static struct linkforward_offload_ctl_t offload;

static inline bool offload_check_if_by_name(char *name)
{
	if (__dev_get_by_name(&init_net, name))
		return true;

	return false;
}

static inline bool offload_check_forward_ready(void)
{
	/* If we got both OFFLOAD_IOCTL_SET_UPSTRM_PARAM & OFFLOAD_IOCTL_ADD_DOWNSTREAM, */
	/* this function should return true.                                             */
	if (offload_check_if_by_name(offload.upstream.iface)
		&& (offload_check_if_by_name(offload.downstream.iface)))
		return true;
	else
		return false;
}

static inline void offload_change_status(enum offload_state st)
{
	offload.status = st;
}

static int offload_open(struct inode *inode, struct file *filp)
{
	return 0;
}

static unsigned int offload_poll(struct file *filp, struct poll_table_struct *wait)
{
	poll_wait(filp, &offload.wq, wait);

	if (!list_empty(&offload.events))
		return POLLIN | POLLRDNORM;

	return 0;
}

static ssize_t offload_read(struct file *filp, char *buf, size_t count, loff_t *fpos)
{
	int err = 0;
	struct linkforward_event *evt;
	struct linkforward_cb_event cb_evt;
	unsigned long flags;

	spin_lock_irqsave(&offload.lock, flags);

	if (list_empty(&offload.events)) {
		lf_err("no event\n");
		spin_unlock_irqrestore(&offload.lock, flags);
		return 0;
	}

	evt = list_first_entry(&offload.events, struct linkforward_event, list);
	list_del(&evt->list);
	spin_unlock_irqrestore(&offload.lock, flags);

	cb_evt.cb_event = evt->cb_event;

	err = copy_to_user((void __user *)buf,
			(void *)&cb_evt, sizeof(cb_evt));

	kfree(evt);

	lf_info("%s callback event=%d\n", OFFLOAD_DEV_NAME, cb_evt.cb_event);

	return sizeof(cb_evt);
}

static int offload_release(struct inode *inode, struct file *file)
{
	return 0;
}

static long offload_ioctl(struct file *filp, unsigned int cmd, unsigned long arg)
{
	int err = 0;
	struct forward_stat stat;
	struct iface_info param;
	uint64_t limit;

	switch (cmd) {
	case OFFLOAD_IOCTL_INIT_OFFLOAD:
		lf_info("OFFLOAD_IOCTL_INIT_OFFLOAD\n");
		offload_init_ctrl();
		break;

	case OFFLOAD_IOCTL_STOP_OFFLOAD:
		lf_info("OFFLOAD_IOCTL_STOP_OFFLOAD\n");
		offload_change_status(STATE_OFFLOAD_OFFLINE);
		offload_gen_event(INTERNAL_OFFLOAD_STOPPED);
		break;

	case OFFLOAD_IOCTL_SET_LOCAL_PRFIX:
		lf_info("OFFLOAD_IOCTL_SET_LOCAL_PRFIX\n");
		break;

	case OFFLOAD_IOCTL_GET_FORWD_STATS:
		stat.rxBytes =
			atomic64_read(&offload.stat[OFFLOAD_RX_FORWARD].fwd_bytes);
		stat.txBytes =
			atomic64_read(&offload.stat[OFFLOAD_TX_FORWARD].fwd_bytes);
		atomic64_set(&offload.stat[OFFLOAD_RX_FORWARD].fwd_bytes, 0);
		atomic64_set(&offload.stat[OFFLOAD_TX_FORWARD].fwd_bytes, 0);

		lf_debug("ioctl forward_stat: stat.rxBytes = %llu, stat.txBytes = %llu\n",
			stat.rxBytes, stat.txBytes);

		err = copy_to_user((void __user *)arg,
				(void *)&stat, sizeof(stat));
		if (err) {
			lf_err("copy_to_user() error:%d\n", err);
			return err;
		}
		break;

	/*
	 * hardware/interfaces/tetheroffload/control/1.0/IOffloadControl.hal
	 * This limit must replace any previous limit.  It may be interpreted as "tell me when
	 * <limit> bytes have been transferred (in either direction) on <upstream>, starting
	 * now and counting from zero."
	 */
	case OFFLOAD_IOCTL_SET_DATA_LIMIT:
		err = copy_from_user(&limit, (const void __user *)arg,
				sizeof(uint64_t));
		if (err) {
			lf_err("copy_from_user() error:%d\n", err);
			return err;
		}

		lf_info("OFFLOAD_IOCTL_SET_DATA_LIMIT = %llu\n", limit);

		atomic64_set(&offload.stat[OFFLOAD_RX_FORWARD].limit_fwd_bytes,
				limit);
		atomic64_set(&offload.stat[OFFLOAD_TX_FORWARD].limit_fwd_bytes,
				limit);
		atomic64_set(&offload.stat[OFFLOAD_RX_FORWARD].reqst_fwd_bytes,
				0);
		atomic64_set(&offload.stat[OFFLOAD_TX_FORWARD].reqst_fwd_bytes,
				0);
		break;

	case OFFLOAD_IOCTL_SET_UPSTRM_PARAM:
		/* Set upstream interface */
		err = copy_from_user(&offload.upstream, (const void __user *)arg,
				sizeof(struct iface_info));
		if (err) {
			lf_err("copy_from_user() error:%d\n", err);
			return err;
		}

		lf_info("OFFLOAD_IOCTL_SET_UPSTRM_PARAM = %s\n", offload.upstream.iface);

		if (offload_check_forward_ready()) {
			offload_change_status(STATE_OFFLOAD_ONLINE);
			offload_gen_event(OFFLOAD_STARTED);
		}

		break;

	case OFFLOAD_IOCTL_ADD_DOWNSTREAM:
		err = copy_from_user(&param, (const void __user *)arg,
				sizeof(struct iface_info));
		if (err) {
			lf_err("copy_from_user() error:%d\n", err);
			return err;
		}
		lf_info("OFFLOAD_IOCTL_ADD_DOWNSTREAM = %s\n", param.iface);

		/* Allow only RNDIS,NCM device for ADD_DOWNSTREAM */
		if (strncmp(param.iface, NET_NAME_RNDIS, strlen(NET_NAME_RNDIS)) == 0 ||
				strncmp(param.iface, NET_NAME_NCM, strlen(NET_NAME_NCM)) == 0) {
			memcpy(&offload.downstream, &param,
					sizeof(struct iface_info));
			if (offload_check_forward_ready()) {
				offload_change_status(STATE_OFFLOAD_ONLINE);
				offload_gen_event(OFFLOAD_STARTED);
			}
		} else
			lf_err("Not allowed device:%s for ADD_DOWNSTREAM\n",
					param.iface);
		break;

	case OFFLOAD_IOCTL_REMOVE_DOWNSTRM:
		err = copy_from_user(&param, (const void __user *)arg,
				sizeof(struct iface_info));
		if (err) {
			lf_err("copy_from_user() error:%d\n", err);
			return err;
		}
		lf_info("OFFLOAD_IOCTL_REMOVE_DOWNSTRM = %s\n", param.iface);

		/* Allow only RNDIS,NCM device for REMOVE_DOWNSTRM */
		if (strncmp(param.iface, NET_NAME_RNDIS, strlen(NET_NAME_RNDIS)) == 0 ||
				strncmp(param.iface, NET_NAME_NCM, strlen(NET_NAME_NCM)) == 0) {
			offload.downstream.iface[0] = '\0';
			if (offload_check_forward_ready()) {
				offload_change_status(STATE_OFFLOAD_OFFLINE);
				offload_gen_event(OFFLOAD_STOPPED_ERROR);
			}
		} else
			lf_err("Not allowed device:%s for REMOVE_DOWNSTRM\n",
					param.iface);
		break;

	case OFFLOAD_IOCTL_CONF_SET_HANDLES:
		lf_info("OFFLOAD_IOCTL_CONF_SET_HANDLES\n");
		break;

	default:
		lf_info("Unknown command\n");
		break;
	}

	return err;
}

void offload_gen_event(int event)
{
	struct linkforward_event *evt;
	unsigned long flags;

	evt = kmalloc(sizeof(struct linkforward_event), GFP_ATOMIC);
	if (evt == NULL)
		return;

	evt->cb_event = event;
	spin_lock_irqsave(&offload.lock, flags);
	list_add_tail(&evt->list, &offload.events);
	spin_unlock_irqrestore(&offload.lock, flags);

	wake_up(&offload.wq);
}

bool offload_keeping_bw(void)
{
	struct linkforward_offload_stat_t *rxstat, *txstat;
	bool ret = false;

	if (offload.status != STATE_OFFLOAD_ONLINE)
		return false;

	rxstat = &offload.stat[OFFLOAD_RX_FORWARD];
	txstat = &offload.stat[OFFLOAD_TX_FORWARD];

	ret = (atomic64_read(&rxstat->limit_fwd_bytes) >= atomic64_read(&rxstat->reqst_fwd_bytes)) &&
		(atomic64_read(&txstat->limit_fwd_bytes) >= atomic64_read(&txstat->reqst_fwd_bytes));

	if (!ret) {
		lf_info("OFFLOAD_STOPPED_LIMIT_REACHED: RX limit = %llu, fwd_bytes = %llu\n",
			atomic64_read(&rxstat->limit_fwd_bytes),
			atomic64_read(&rxstat->reqst_fwd_bytes));
		lf_info("OFFLOAD_STOPPED_LIMIT_REACHED: TX limit = %llu, fwd_bytes = %llu\n",
			atomic64_read(&txstat->limit_fwd_bytes),
			atomic64_read(&txstat->reqst_fwd_bytes));
		offload_change_status(STATE_OFFLOAD_LIMIT_REACHED);
		offload_gen_event(OFFLOAD_STOPPED_LIMIT_REACHED);
	}

	return ret;
}

int offload_get_status(void)
{
	return offload.status;
}

bool offload_enabled(void) {
	if (offload.status == STATE_OFFLOAD_ONLINE)
		return true;
	else
		return false;
}

void offload_update_tx_stat(int len)
{
	atomic64_add(len,
		&offload.stat[OFFLOAD_TX_FORWARD].fwd_bytes);
	atomic64_add(len,
		&offload.stat[OFFLOAD_TX_FORWARD].reqst_fwd_bytes);
}

void offload_update_rx_stat(int len)
{
	atomic64_add((len - sizeof(struct ethhdr)),
		&offload.stat[OFFLOAD_RX_FORWARD].fwd_bytes);
	atomic64_add((len - sizeof(struct ethhdr)),
		&offload.stat[OFFLOAD_RX_FORWARD].reqst_fwd_bytes);
}

u64 get_rx_offload_fwd_bytes(void)
{
	return atomic64_read(&offload.stat[OFFLOAD_RX_FORWARD].reqst_fwd_bytes);
}

u64 get_tx_offload_fwd_bytes(void)
{
	return atomic64_read(&offload.stat[OFFLOAD_TX_FORWARD].reqst_fwd_bytes);
}

struct linkforward_offload_ctl_t *offload_init_ctrl(void)
{
	unsigned long flags;

	spin_lock_irqsave(&offload.lock, flags);
	while (!list_empty(&offload.events)) {
		struct linkforward_event *evt;

		evt = list_first_entry(&offload.events, struct linkforward_event, list);
		list_del(&evt->list);
		kfree(evt);
	}
	spin_unlock_irqrestore(&offload.lock, flags);

	memset(&offload.upstream, 0, sizeof(struct iface_info));
	memset(&offload.downstream, 0, sizeof(struct iface_info));
	memset(&offload.stat, 0, sizeof(struct linkforward_offload_stat_t) * OFFLOAD_MAX_FORWARD);
	atomic64_set(&offload.stat[OFFLOAD_RX_FORWARD].limit_fwd_bytes, LONG_MAX);
	atomic64_set(&offload.stat[OFFLOAD_TX_FORWARD].limit_fwd_bytes, LONG_MAX);

	offload_change_status(STATE_OFFLOAD_OFFLINE);

	lf_info("done");

	return &offload;
}

static const struct file_operations offload_fops = {
	.owner		= THIS_MODULE,
	.open		= offload_open,
	.poll		= offload_poll,
	.read		= offload_read,
	.release	= offload_release,
	.compat_ioctl = offload_ioctl,
	.unlocked_ioctl = offload_ioctl,
};

static struct miscdevice miscdev = {
	.minor	= MISC_DYNAMIC_MINOR,
	.name	= OFFLOAD_DEV_NAME,
	.fops	= &offload_fops,
};

static inline int isDownstreamdevice(struct net_device *ndev)
{
	if (strcmp(ndev->name, offload.downstream.iface) == 0)
		return 1;

	return 0;
}

static inline int isUpstreamDevice(struct net_device *ndev)
{
	if (strcmp(ndev->name, offload.upstream.iface) == 0) /* SHOULD BE SAME */
		return 1;

	return 0;
}

static int offload_netdev_event(struct notifier_block *this,
					  unsigned long event, void *ptr)
{
	struct net_device *ndev = netdev_notifier_info_to_dev(ptr);

	/* skip unknown devices */
	if (strncmp(ndev->name, NET_NAME_RMNET, strlen(NET_NAME_RMNET)) != 0 &&
		strncmp(ndev->name, NET_NAME_V4RMNET, strlen(NET_NAME_V4RMNET)) != 0 &&
		strncmp(ndev->name, NET_NAME_RNDIS, strlen(NET_NAME_RNDIS)) != 0 &&
		strncmp(ndev->name, NET_NAME_NCM, strlen(NET_NAME_NCM)) != 0)
		return NOTIFY_DONE;

	switch (event) {
	case NETDEV_UP:
		lf_info("%s:%s\n", ndev->name, "NETDEV_UP");
		if (offload_check_forward_ready()) {
			lf_info("OFFLOAD_STARTED\n");
			offload_change_status(STATE_OFFLOAD_ONLINE);
			offload_gen_event(OFFLOAD_STARTED);
		}
		break;

	case NETDEV_DOWN:
		lf_info("%s:%s\n", ndev->name, "NETDEV_DOWN");
		if (offload_check_forward_ready()) {
			lf_info("OFFLOAD_STOPPED_ERROR\n");
			offload_change_status(STATE_OFFLOAD_OFFLINE);
			offload_gen_event(OFFLOAD_STOPPED_ERROR);
		}
		break;

	default:
		break;
	}

	return NOTIFY_DONE;
}

static struct notifier_block offload_netdev_notifier = {
	.notifier_call	= offload_netdev_event,
};

int offload_initialize(void)
{
	init_waitqueue_head(&offload.wq);
	INIT_LIST_HEAD(&offload.events);
	spin_lock_init(&offload.lock);
	register_netdevice_notifier(&offload_netdev_notifier);
	offload_change_status(STATE_OFFLOAD_OFFLINE);

	return misc_register(&miscdev);
}
