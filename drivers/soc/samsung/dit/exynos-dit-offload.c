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
#include <net/net_namespace.h>

#include "exynos-dit.h" /* dit_info */

#include "exynos-dit-ioctl.h"
#include "exynos-dit-offload.h"

static struct dit_offload_ctl_t offload;

static inline bool offload_check_if_by_name(char *name)
{
	if (__dev_get_by_name(&init_net, name))
		return true;

	return false;
}

static inline bool offload_check_forward_ready(void)
{
	if (offload_check_if_by_name(offload.upstream.iface)
		&& (offload_check_if_by_name(offload.downstream[0].iface)
			|| offload_check_if_by_name(offload.downstream[1].iface)))
		return true;

	return false;
}

void perftest_offload_change_status(enum offload_state st)
{
	offload.status = st;
	wake_up(&offload.wq);
}

static inline void offload_change_status(enum offload_state st)
{
	offload.status = st;
	wake_up(&offload.wq);
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
	struct dit_event *evt;
	struct dit_cb_event	cb_evt;
	unsigned long flags;

	if (list_empty(&offload.events)) {
		dit_err("no event\n");
		return 0;
	}

	spin_lock_irqsave(&offload.lock, flags);
	evt = list_first_entry(&offload.events, struct dit_event, list);
	list_del(&evt->list);
	spin_unlock_irqrestore(&offload.lock, flags);

	cb_evt.cb_event = evt->cb_event;

	err = copy_to_user((void __user *)buf,
			(void *)&cb_evt, sizeof(cb_evt));

	kfree(evt);

	dit_info("%s callback event=%d\n", OFFLOAD_DEV_NAME, cb_evt.cb_event);

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
	int idx;
	struct net_device *ndev;

	switch (cmd) {
	case OFFLOAD_IOCTL_INIT_OFFLOAD:
		dit_info("INIT_OFFLOAD\n");
		dit_init(DIT_INIT_REASON_RUNTIME);
		offload.config_enabled = true;
		break;

	case OFFLOAD_IOCTL_STOP_OFFLOAD:
		dit_info("STOP_OFFLOAD\n");
		offload_gen_event(OFFLOAD_STOPPED_ERROR);
		dit_deinit(DIT_DEINIT_REASON_RUNTIME);
		break;

	case OFFLOAD_IOCTL_SET_LOCAL_PRFIX:
		dit_info("SET_LOCAL_PRFIX\n");
		break;

	case OFFLOAD_IOCTL_GET_FORWD_STATS:
		err = copy_from_user(&stat, (const void __user *)arg,
				sizeof(struct forward_stat));

		offload.stat[OFFLOAD_RX__FORWARD].fwd_bytes[1 - offload.stat_read_idx] = 0;
		offload.stat[OFFLOAD_TX__FORWARD].fwd_bytes[1 - offload.stat_read_idx] = 0;
		stat.rxBytes = offload.stat[OFFLOAD_RX__FORWARD].fwd_bytes[offload.stat_read_idx];
		stat.txBytes = offload.stat[OFFLOAD_TX__FORWARD].fwd_bytes[offload.stat_read_idx];

		offload.stat_read_idx = 1 - offload.stat_read_idx;

		dit_debug("forward_stat: stat.rxBytes = %llu, stat.txBytes = %llu\n",
			stat.rxBytes, stat.txBytes);

		err = copy_to_user((void __user *)arg,
				(void *)&stat, sizeof(stat));

		break;

	case OFFLOAD_IOCTL_SET_DATA_LIMIT:
		err = copy_from_user(&limit, (const void __user *)arg,
				sizeof(uint64_t));
		offload.stat[OFFLOAD_RX__FORWARD].limit_fwd_bytes = limit;
		offload.stat[OFFLOAD_TX__FORWARD].limit_fwd_bytes = limit;
		dit_info("SET_DATA_LIMIT = %llu\n", limit);
		break;

	case OFFLOAD_IOCTL_SET_UPSTRM_PARAM:
		err = copy_from_user(&offload.upstream, (const void __user *)arg,
				sizeof(struct iface_info));

		dit_info("SET_UPSTRM_PARAM = %s\n", offload.upstream.iface);
		ndev = __dev_get_by_name(&init_net, offload.upstream.iface);
		if (ndev)
			dit_setup_upstream_device(ndev);

		if (offload_check_forward_ready()) {
			offload_change_status(STATE_OFFLOAD_ONLINE);
			offload_gen_event(OFFLOAD_STARTED);
		}

		break;

	case OFFLOAD_IOCTL_ADD_DOWNSTREAM:
		err = copy_from_user(&param, (const void __user *)arg,
				sizeof(struct iface_info));

		if (strstr(param.iface, "rndis") || strstr(param.iface, "ncm"))
			idx = 0;
		else
			idx = 1;

		err = copy_from_user(&offload.downstream[idx], (const void __user *)arg,
				sizeof(struct iface_info));

		dit_info("ADD_DOWNSTREAM(%d) = %s\n", idx, offload.downstream[idx].iface);
		ndev = __dev_get_by_name(&init_net, offload.downstream[idx].iface);
		if (ndev)
			dit_setup_downstream_device(ndev);

		if (offload_check_forward_ready()) {
			offload_change_status(STATE_OFFLOAD_ONLINE);
			offload_gen_event(OFFLOAD_STARTED);
		}

		break;

	case OFFLOAD_IOCTL_REMOVE_DOWNSTRM:
		err = copy_from_user(&param, (const void __user *)arg,
				sizeof(struct iface_info));

		if (strstr(param.iface, "rndis") || strstr(param.iface, "ncm"))
			idx = 0;
		else
			idx = 1;

		dit_info("REMOVE_DOWNSTRM(%d) = %s\n", idx, offload.downstream[idx].iface);
		ndev = __dev_get_by_name(&init_net, offload.downstream[idx].iface);
		if (ndev)
			dit_clear_downstream_device(ndev);

		offload.downstream[idx].iface[0] = '\0';

		if (offload_check_forward_ready()) {
			offload_change_status(STATE_OFFLOAD_OFFLINE);
			offload_gen_event(OFFLOAD_STOPPED_ERROR);
		}

		break;

	case OFFLOAD_IOCTL_CONF_SET_HANDLES:
		dit_info("SET_HANDLES\n");
		break;

	default:
		dit_info("Unknown command\n");
		break;
	}

	return err;
}

void offload_gen_event(int event)
{
	struct dit_event *evt;
	unsigned long flags;

	evt = kmalloc(sizeof(struct dit_event), GFP_KERNEL);
	evt->cb_event = event;
	spin_lock_irqsave(&offload.lock, flags);
	list_add(&evt->list, &offload.events);
	spin_unlock_irqrestore(&offload.lock, flags);

	wake_up(&offload.wq);
}

bool offload_keeping_bw(void)
{
	struct dit_offload_stat_t *rxstat, *txstat;
	bool ret = false;

	if (offload.status != STATE_OFFLOAD_ONLINE)
		return false;

	rxstat = &offload.stat[OFFLOAD_RX__FORWARD];
	txstat = &offload.stat[OFFLOAD_TX__FORWARD];

	ret = (rxstat->limit_fwd_bytes >= rxstat->reqst_fwd_bytes)
			&& (txstat->limit_fwd_bytes >= txstat->reqst_fwd_bytes);

	if (!ret) {
		dit_info("OFFLOAD_STOPPED_LIMIT_REACHED: RX limit = %llu, fwd_bytes = %llu\n",
			rxstat->limit_fwd_bytes, rxstat->reqst_fwd_bytes);
		dit_info("OFFLOAD_STOPPED_LIMIT_REACHED: TX limit = %llu, fwd_bytes = %llu\n",
			txstat->limit_fwd_bytes, txstat->reqst_fwd_bytes);
		offload_gen_event(OFFLOAD_STOPPED_LIMIT_REACHED);
		offload_change_status(STATE_OFFLOAD_LIMIT_REACHED);
	}

	return ret;
}

bool offload_config_enabled(void)
{
	return offload.config_enabled;
}

int offload_get_status(void)
{
	return offload.status;
}

void offload_update_reqst(int DIR, int len)
{
	offload.stat[DIR].reqst_fwd_bytes += len;
}

void offload_update_stat(int DIR, int len)
{
	offload.stat[DIR].fwd_bytes[offload.stat_read_idx] +=
		len - (OFFLOAD_RX__FORWARD == DIR ? sizeof(struct ethhdr) : 0);
}

struct dit_offload_ctl_t *offload_init_ctrl(void)
{
	unsigned long flags;

	spin_lock_irqsave(&offload.lock, flags);
	while (!list_empty(&offload.events)) {
		struct dit_event *evt;

		evt = list_first_entry(&offload.events, struct dit_event, list);
		list_del(&evt->list);
		kfree(evt);
	}
	spin_unlock_irqrestore(&offload.lock, flags);

	memset(&offload.upstream, 0, sizeof(struct iface_info));
	memset(&offload.downstream[0], 0, sizeof(struct iface_info) * OFFLOAD_MAX_DOWNSTREAM);
	memset(&offload.stat, 0, sizeof(struct dit_offload_stat_t) * OFFLOAD_MAX_FORWARD);
	offload.stat[OFFLOAD_RX__FORWARD].limit_fwd_bytes = ULONG_MAX;
	offload.stat[OFFLOAD_TX__FORWARD].limit_fwd_bytes = ULONG_MAX;

	offload.stat_read_idx = 0;
	offload_reset();

	dit_info("done");

	return &offload;
}

void offload_reset(void)
{
	offload_change_status(STATE_OFFLOAD_OFFLINE);
	offload.config_enabled = false;
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
	int i;

	for (i = 0; i < OFFLOAD_MAX_DOWNSTREAM; i++)
		if (strcmp(ndev->name, offload.downstream[i].iface) == 0)
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

	if (!isLANdevice(ndev) && !isRANdevice(ndev))
		return NOTIFY_DONE;

	switch (event) {
	case NETDEV_UP:
		dit_info("%s:%s\n", ndev->name, "NETDEV_UP");
		if (isLANdevice(ndev))
			dit_setup_downstream_device(ndev);
		else if (isRANdevice(ndev))
			dit_setup_upstream_device(ndev);

		if (offload_check_forward_ready()) {
			dit_info("OFFLOAD_STARTED\n");
			offload_change_status(STATE_OFFLOAD_ONLINE);
			offload_gen_event(OFFLOAD_STARTED);
		}

		break;

	case NETDEV_DOWN:
		dit_info("%s:%s\n", ndev->name, "NETDEV_DOWN");
		if (isLANdevice(ndev))
			dit_clear_downstream_device(ndev);
		else if (isRANdevice(ndev))
			dit_clear_upstream_device(ndev);

		if (offload_check_forward_ready()) {
			dit_info("OFFLOAD_STOPPED_ERROR\n");
			offload_change_status(STATE_OFFLOAD_OFFLINE);
			offload_gen_event(OFFLOAD_STOPPED_ERROR);
		}

		break;

	case NETDEV_CHANGE:
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

	return misc_register(&miscdev);
}
