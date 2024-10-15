// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2022 Samsung Electronics.
 *
 */

#include <linux/poll.h>

#include "modem_prj.h"
#include "modem_utils.h"

int iodev_open(struct inode *inode, struct file *filp)
{
	struct io_device *iod = to_io_device(inode->i_cdev);
	struct modem_shared *msd = iod->msd;
	struct link_device *ld;
	int ret;

	filp->private_data = (void *)iod;

	atomic_inc(&iod->opened);

	iod->opened_time = ktime_get();

	list_for_each_entry(ld, &msd->link_dev_list, list) {
		if (IS_CONNECTED(iod, ld) && ld->init_comm) {
			ret = ld->init_comm(ld, iod);
			if (ret < 0) {
				mif_err("%s<->%s: ERR! init_comm fail(%d)\n",
					iod->name, ld->name, ret);
				atomic_dec(&iod->opened);
				return ret;
			}
		}
	}

	mif_info("%s (opened %d) by %s\n", iod->name, atomic_read(&iod->opened), current->comm);

	return 0;
}

int iodev_release(struct inode *inode, struct file *filp)
{
	struct io_device *iod = (struct io_device *)filp->private_data;
	struct modem_shared *msd = iod->msd;
	struct link_device *ld;

	do {
		int i;

		if (!atomic_dec_and_test(&iod->opened) &&
		    (iod->io_typ != IODEV_BOOTDUMP || strcmp(current->comm, "cbd")))
			break;

		skb_queue_purge(&iod->sk_rx_q);

		if (!iod->sk_multi_q)
			break;

		/* purge multi_frame queue */
		for (i = 0; i < NUM_SIPC_MULTI_FRAME_IDS; i++)
			skb_queue_purge(&iod->sk_multi_q[i]);
	} while (0);

	list_for_each_entry(ld, &msd->link_dev_list, list) {
		if (IS_CONNECTED(iod, ld) && ld->terminate_comm)
			ld->terminate_comm(ld, iod);
	}

	mif_info("%s (opened %d) by %s\n", iod->name, atomic_read(&iod->opened), current->comm);

	return 0;
}

unsigned int iodev_poll(struct file *filp, struct poll_table_struct *wait)
{
	struct io_device *iod = (struct io_device *)filp->private_data;
	struct modem_ctl *mc;
	struct sk_buff_head *rxq;
	struct link_device *ld;

	if (!iod)
		return POLLERR;

	mc = iod->mc;
	rxq = &iod->sk_rx_q;
	ld = get_current_link(iod);

	if (skb_queue_empty(rxq))
		poll_wait(filp, &iod->wq, wait);

	switch (mc->phone_state) {
	case STATE_BOOTING:
	case STATE_ONLINE:
		if (!skb_queue_empty(rxq))
			return POLLIN | POLLRDNORM;
		break;
	case STATE_CRASH_EXIT:
	case STATE_CRASH_RESET:
	case STATE_NV_REBUILDING:
	case STATE_CRASH_WATCHDOG:
		if (is_boot_iod(iod) || is_dump_iod(iod)) {
			if (!skb_queue_empty(rxq))
				return POLLIN | POLLRDNORM;
		}

		mif_err_limited("%s: %s.state == %s\n", iod->name, mc->name, mc_state(mc));

		if (is_fmt_iod(iod) || is_boot_iod(iod))
			return POLLHUP;
		break;
	case STATE_RESET:
		mif_err_limited("%s: %s.state == %s\n", iod->name, mc->name, mc_state(mc));

		if (iod->attrs & IO_ATTR_STATE_RESET_NOTI)
			return POLLHUP;
		break;
	default:
		break;
	}

	return 0;
}

long iodev_ioctl(struct file *filp, unsigned int cmd, unsigned long arg)
{
	struct io_device *iod = (struct io_device *)filp->private_data;
	struct link_device *ld = get_current_link(iod);
	struct modem_ctl *mc = iod->mc;

	switch (cmd) {
	case IOCTL_GET_CP_STATUS: {
		enum modem_state p_state;

		mif_debug("%s: IOCTL_GET_CP_STATUS\n", iod->name);

		p_state = mc->phone_state;

		if (p_state != STATE_ONLINE) {
			mif_debug("%s: IOCTL_GET_CP_STATUS (state %s)\n",
				  iod->name, cp_state_str(p_state));
		}

		switch (p_state) {
		case STATE_NV_REBUILDING:
			mc->phone_state = STATE_ONLINE;
			break;
		/* Do not return an internal state */
		case STATE_RESET:
			p_state = STATE_OFFLINE;
			break;
		default:
			break;
		}

		return p_state;
	}
	case IOCTL_GET_TIME_AFTER_RESET: {
		unsigned long err;
		ktime_t curr_time = ktime_get();
		long long duration = (long long)((curr_time - mc->reset_time) / 1000000);
		mif_info("%s: IOCTL_GET_TIME_AFTER_RESET\n", iod->name);

		err = copy_to_user((void __user *)arg, &duration, sizeof(long long));
		if (err) {
			mif_err("copy_to_user fail:%lu\n", err);
			return err;
		}

		return 0;
	}
	case IOCTL_SET_MINIDUMP_MODE: {
		if (copy_from_user(&ld->minidump_mode, (const void __user *)arg,
			sizeof(ld->minidump_mode))) {
			mif_err("IOCTL_SET_MINIDUMP_MODE err\n");
			return -EFAULT;
		}

		mif_info("minidump mode:%d\n", ld->minidump_mode);

		return 0;
	}
	case IOCTL_TRIGGER_CP_CRASH: {
		u32 crash_type = CRASH_REASON_NONE;
		char reason[CP_CRASH_INFO_SIZE];

		memset(reason, 0, sizeof(reason));

		switch (ld->protocol) {
		case PROTOCOL_SIPC:
			if (!arg) {
				mif_info("No argument from USER\n");
				break;
			}

			crash_type = (u32)arg;
			break;

		case PROTOCOL_SIT:
			crash_type = CRASH_REASON_RIL_TRIGGER_CP_CRASH;

			if (!arg ||
			    copy_from_user(reason, (void __user *)arg, CP_CRASH_INFO_SIZE)) {
				mif_info("No argument from USER\n");
				break;
			}

			mif_info("Crash Reason:%s\n", reason);
			break;

		default:
			mif_err("ERR: unknown protocol\n");
			break;
		}

		mif_info("%s: IOCTL_TRIGGER_CP_CRASH type:%u\n", iod->name, crash_type);
		ld->link_trigger_cp_crash(to_mem_link_device(ld), crash_type, reason);

		return 0;
	}
	case IOCTL_LOAD_GNSS_IMAGE:
		if (!ld->load_gnss_image) {
			mif_err("%s: load_gnss_image is null\n", iod->name);
			return -EINVAL;
		}

		mif_info("%s: IOCTL_LOAD_GNSS_IMAGE\n", iod->name);
		return ld->load_gnss_image(ld, iod, arg);

	case IOCTL_READ_GNSS_IMAGE:
		if (!ld->read_gnss_image) {
			mif_err("%s: read_gnss_image is null\n", iod->name);
			return -EINVAL;
		}

		mif_info("%s: IOCTL_READ_GNSS_IMAGE\n", iod->name);
		return ld->read_gnss_image(ld, iod, arg);

	default:
		 /* If you need to handle the ioctl for specific link device,
		  * then assign the link ioctl handler to ld->ioctl
		  * It will be call for specific link ioctl
		  */
		if (ld->ioctl)
			return ld->ioctl(ld, iod, cmd, arg);

		mif_info("%s: ERR! undefined cmd 0x%X\n", iod->name, cmd);
		return -EINVAL;
	}

	return 0;
}

ssize_t iodev_write(struct file *filp, const char __user *data,
		    size_t count, loff_t *fpos)
{
#define INIT_END_WAIT_MS	150

	struct io_device *iod = (struct io_device *)filp->private_data;
	struct link_device *ld = get_current_link(iod);
	struct mem_link_device *mld = to_mem_link_device(ld);
	struct modem_ctl *mc = iod->mc;
	u8 cfg = 0;
	u16 cfg_sit = 0;
	unsigned int headroom = 0;
	unsigned int copied = 0, tot_frame = 0, copied_frm = 0;
	/* 64bit prevent */
	unsigned int cnt = (unsigned int)count;
	struct timespec64 ts;
	int curr_init_end_cnt;
	int retry = 0;

	/* Record the timestamp */
	ktime_get_ts64(&ts);

	if (iod->format <= IPC_RFS && iod->ch == 0)
		return -EINVAL;

	if (unlikely(!cp_online(mc)) && is_ipc_iod(iod)) {
		mif_debug("%s: ERR! %s->state == %s\n", iod->name, mc->name, mc_state(mc));
		return -EPERM;
	}

	if (iod->link_header) {
		switch (ld->protocol) {
		case PROTOCOL_SIPC:
			cfg = sipc5_build_config(iod, ld, cnt);
			headroom = sipc5_get_hdr_len(&cfg);
			break;
		case PROTOCOL_SIT:
			cfg_sit = exynos_build_fr_config(iod, ld, cnt);
			headroom = EXYNOS_HEADER_SIZE;
			break;
		default:
			mif_err("protocol error %d\n", ld->protocol);
			return -EINVAL;
		}
	}

	if (iod->io_typ != IODEV_IPC)
		goto send;

	/* Wait for a while if a new CMD_INIT_END is sent */
	while ((curr_init_end_cnt = atomic_read(&mld->init_end_cnt)) != mld->last_init_end_cnt &&
	       retry++ < 3) {
		mif_info_limited("%s: wait for INIT_END done (%dms) cnt:%d last:%d cmd:0x%02X\n",
				 iod->name, INIT_END_WAIT_MS,
				 curr_init_end_cnt, mld->last_init_end_cnt,
				 mld->read_ap2cp_irq(mld));

		if (atomic_inc_return(&mld->init_end_busy) > 1)
			curr_init_end_cnt = -1;

		msleep(INIT_END_WAIT_MS);
		if (curr_init_end_cnt >= 0)
			mld->last_init_end_cnt = curr_init_end_cnt;

		atomic_dec(&mld->init_end_busy);
	}

	if (unlikely(!mld->last_init_end_cnt)) {
		mif_err_limited("%s: INIT_END is not done\n", iod->name);
		return -EAGAIN;
	}

send:
	while (copied < cnt) {
		struct sk_buff *skb;
		char *buff;
		unsigned int remains = cnt - copied;
		unsigned int tailroom = 0;
		unsigned int tx_bytes;
		unsigned int alloc_size;
		int ret;

		if (check_add_overflow(remains, headroom, &alloc_size))
			alloc_size = SZ_2K;

		switch (ld->protocol) {
		case PROTOCOL_SIPC:
			if (iod->max_tx_size)
				alloc_size = min_t(unsigned int, alloc_size, iod->max_tx_size);
			break;
		case PROTOCOL_SIT:
			if (iod->io_typ == IODEV_IPC)
				alloc_size = min_t(unsigned int, alloc_size, SZ_2K);
			break;
		default:
			mif_err("protocol error %d\n", ld->protocol);
			return -EINVAL;
		}

		/* Calculate tailroom for padding size */
		if (iod->link_header && ld->aligned)
			tailroom = ld->calc_padding_size(alloc_size);

		alloc_size += tailroom;

		skb = alloc_skb(alloc_size, GFP_KERNEL);
		if (!skb) {
			mif_info("%s: ERR! alloc_skb fail (alloc_size:%u)\n",
				 iod->name, alloc_size);
			return -ENOMEM;
		}

		tx_bytes = alloc_size - headroom - tailroom;

		/* Reserve the space for a link header */
		skb_reserve(skb, headroom);

		/* Copy an IPC message from the user space to the skb */
		buff = skb_put(skb, tx_bytes);
		if (copy_from_user(buff, data + copied, tx_bytes)) {
			mif_err("%s->%s: ERR! copy_from_user fail(count %lu)\n",
				iod->name, ld->name, (unsigned long)count);
			dev_kfree_skb_any(skb);
			return -EFAULT;
		}

		/* Update size of copied payload */
		copied += tx_bytes;
		/* Update size of total frame included hdr, pad size */
		tot_frame += alloc_size;

		/* Store the IO device, the link device, etc. */
		skbpriv(skb)->iod = iod;
		skbpriv(skb)->ld = ld;

		skbpriv(skb)->lnk_hdr = iod->link_header;
		skbpriv(skb)->sipc_ch = iod->ch;

		/* Copy the timestamp to the skb */
		skbpriv(skb)->ts = ts;

		/* Build SIPC5 link header*/
		if (cfg || cfg_sit) {
			buff = skb_push(skb, headroom);

			switch (ld->protocol) {
			case PROTOCOL_SIPC:
				sipc5_build_header(iod, buff, cfg, tx_bytes, cnt - copied);
				break;
			case PROTOCOL_SIT:
				exynos_build_header(iod, ld, buff, cfg_sit, 0, tx_bytes);
				/* modify next link header for multiframe */
				if (iod->io_typ == IODEV_IPC &&
				    ((cfg_sit >> 8) & EXYNOS_SINGLE_MASK) != EXYNOS_SINGLE_MASK)
					cfg_sit = modify_next_frame(cfg_sit);
				break;
			default:
				mif_err("protocol error %d\n", ld->protocol);
				return -EINVAL;
			}
		}

		/* Apply padding */
		if (tailroom)
			skb_put(skb, tailroom);

		/**
		 * Send the skb with a link device
		 */
		ret = ld->send(ld, iod, skb);
		if (ret < 0) {
			mif_err("%s->%s: %s->send fail(%d, tx:%d len:%lu)\n",
				iod->name, mc->name, ld->name,
				ret, tx_bytes, (unsigned long)count);
			dev_kfree_skb_any(skb);
			return ret;
		}
		copied_frm += ret;
	}

	if (copied_frm != tot_frame) {
		mif_info("%s->%s: WARN! %s->send ret:%d (len:%lu)\n",
			 iod->name, mc->name, ld->name, copied_frm, (unsigned long)count);
	}

	return count;
}

ssize_t iodev_read(struct file *filp, char *buf, size_t count, loff_t *fpos)
{
	struct io_device *iod = (struct io_device *)filp->private_data;
	struct sk_buff_head *rxq = &iod->sk_rx_q;
	struct sk_buff *skb;
	int copied;

	if (skb_queue_empty(rxq)) {
		long tmo = msecs_to_jiffies(100);

		wait_event_timeout(iod->wq, !skb_queue_empty(rxq), tmo);
	}

	skb = skb_dequeue(rxq);
	if (unlikely(!skb)) {
		mif_info("%s: NO data in RXQ\n", iod->name);
		return 0;
	}

	copied = skb->len > count ? count : skb->len;

	if (copy_to_user(buf, skb->data, copied)) {
		mif_err("%s: ERR! copy_to_user fail\n", iod->name);
		dev_kfree_skb_any(skb);
		return -EFAULT;
	}

	/* dummy netdev */
	if (iod->ndev) {
		iod->ndev->stats.rx_packets++;
		iod->ndev->stats.rx_bytes += copied;
	}

	mif_debug("%s: data:%d copied:%d qlen:%d\n", iod->name, skb->len, copied, rxq->qlen);

	if (skb->len > copied) {
		skb_pull(skb, copied);
		skb_queue_head(rxq, skb);
	} else {
		dev_consume_skb_any(skb);
	}

	return copied;
}

static const struct file_operations iodev_io_fops = {
	.owner = THIS_MODULE,
	.open = iodev_open,
	.release = iodev_release,
	.poll = iodev_poll,
	.unlocked_ioctl = iodev_ioctl,
	.compat_ioctl = iodev_ioctl,
	.write = iodev_write,
	.read = iodev_read,
};

const struct file_operations *get_iodev_io_fops(void)
{
	return &iodev_io_fops;
}

