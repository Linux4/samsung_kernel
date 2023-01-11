/*
    msocket.c Created on: Aug 2, 2010, Jinhua Huang <jhhuang@marvell.com>

    Marvell PXA9XX ACIPC-MSOCKET driver for Linux
    Copyright (C) 2010 Marvell International Ltd.

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License version 2 as
    published by the Free Software Foundation.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#include <linux/kernel.h>
#include <linux/fs.h>
#include <linux/errno.h>
#include <linux/completion.h>
#include <linux/miscdevice.h>
#include <linux/proc_fs.h>
#include <linux/seq_file.h>
#include <linux/sched.h>
#include <linux/version.h>
#include <linux/string.h>
#include <linux/export.h>
#include <linux/module.h>
#include <linux/cdev.h>
#include <linux/notifier.h>
#include <linux/reboot.h>
#ifdef CONFIG_COMPAT
#include <linux/compat.h>
#endif
#include "acipcd.h"
#include "amipcd.h"
#include "shm.h"
#include "portqueue.h"
#include "msocket.h"
#include "shm_share.h"
#include "data_path.h"
#include "direct_rb.h"
#include "common_regs.h"
#include "pxa_m3_rm.h"

#define CMSOCKDEV_NR_DEVS PORTQ_NUM_MAX

static int cp_shm_ch_inited;
static int m3_shm_ch_inited;
static int aponly;

int cmsockdev_major;
int cmsockdev_minor;
int cmsockdev_nr_devs = CMSOCKDEV_NR_DEVS;
struct cmsockdev_dev {
	struct cdev cdev;	/* Char device structure */
};

static unsigned dump_flag;
struct cmsockdev_dev *cmsockdev_devices;
static struct class *cmsockdev_class;

/* forward msocket sync related static function prototype */
static void cp_sync_worker(struct work_struct *work);
static void msocket_connect(enum portq_grp_type grp_type);
static void msocket_disconnect(enum portq_grp_type grp_type);

static DECLARE_WORK(sync_work, cp_sync_worker);
static volatile bool cp_is_sync_canceled;
static spinlock_t cp_sync_lock;
bool cp_is_synced;
EXPORT_SYMBOL(cp_is_synced);
bool cp_recv_up_ioc;
DECLARE_COMPLETION(cp_peer_sync);

bool m3_is_synced;
DECLARE_COMPLETION(m3_peer_sync);

static void dump(const unsigned char *data, unsigned int len)
{
	int i;
	char *buf = NULL;
	char *tmp_buf = NULL;
	buf = kmalloc(2 * len + 1, GFP_ATOMIC);
	if (buf == NULL) {
		pr_err("msocket: %s: malloc error\n", __func__);
		return;
	}
	tmp_buf = buf;
	for (i = 0; i < len; i++)
		tmp_buf += sprintf(tmp_buf, "%02x", data[i]);
	pr_info("%s\n", buf);
	kfree(buf);
	return;
}

void data_dump(const unsigned char *data, unsigned int len, int port,
	       int direction)
{
	if (direction == DATA_TX) {
		if ((dump_flag & DUMP_TX) && (dump_flag & DUMP_PORT(port))) {
			pr_info(
			     " Msocket TX: DUMP BEGIN port :%d, Length:%d --\n",
			     port, len);
			if (!(dump_flag & DUMP_TX_SP))
				dump(data, len);
		}
	} else if (direction == DATA_RX) {
		if ((dump_flag & DUMP_RX) && (dump_flag & DUMP_PORT(port))) {
			pr_info(
			     " Msocket RX: DUMP BEGIN port :%d, Length:%d --\n",
			     port, len);
			if (!(dump_flag & DUMP_RX_SP))
				dump(data, len);
		}
	}
	return;
}

/* open a msocket in kernel */
int msocket(int port)
{
	return msocket_with_cb(port, NULL, NULL);
}
EXPORT_SYMBOL(msocket);

/* open a msocket with receive callback in kernel */
int msocket_with_cb(int port,
		void (*clbk)(struct sk_buff *, void *), void *arg)
{
	struct portq *portq;

	portq = portq_open_with_cb(port, clbk, arg);
	if (IS_ERR(portq)) {
		pr_err("MSOCK: can't open queue port %d\n", port);
		return -1;
	}

	return port;
}
EXPORT_SYMBOL(msocket_with_cb);

/* close a msocket */
int mclose(int sock)
{
	struct portq *portq;

	portq = (struct portq *)portq_get(sock);

	if (!portq) {
		pr_err("MSOCK: closed socket %d failed\n", sock);
		return -1;
	}

	portq_close(portq);

	return 0;
}
EXPORT_SYMBOL(mclose);

/* send packet to msocket */
int msend(int sock, const void *buf, int len, int flags)
{
	struct portq *portq;
	struct portq_group *pgrp;
	struct sk_buff *skb;
	struct shm_skhdr *hdr;
	bool block = flags == MSOCKET_KERNEL;

	portq = (struct portq *)portq_get(sock);
	if (!portq) {
		pr_err("MSOCK: %s: sock %d not opened!\n",
		       __func__, sock);
		return -1;
	}

	pgrp = portq_get_group(portq);

	/* check the len first */
	if (len > (pgrp->rbctl->tx_skbuf_size - sizeof(*hdr))) {
		pr_err("MSOCK: %s: port %d, len is %d!!\n",
		       __func__, portq->port, len);
		portq->stat_tx_drop++;
		return -1;
	}

	/* alloc space */
	if (block)
		skb = alloc_skb(len + sizeof(*hdr), GFP_KERNEL);
	else
		skb = alloc_skb(len + sizeof(*hdr), GFP_ATOMIC);

	if (!skb) {
		pr_err("MSOCK: %s: out of memory.\n", __func__);
		return -ENOMEM;
	}
	skb_reserve(skb, sizeof(*hdr));	/* reserve header space */

	memcpy(skb_put(skb, len), buf, len);

	/* push header back */
	hdr = (struct shm_skhdr *)skb_push(skb, sizeof(*hdr));

	hdr->address = 0;
	hdr->port = portq->port;
	hdr->checksum = 0;
	hdr->length = len;

	if (!portq_is_synced(portq) || portq_xmit(portq, skb, block) < 0) {
		kfree_skb(skb);
		pr_err("MSOCK: %s: port %d xmit error.\n",
		       __func__, portq->port);
		return -1;
	}

	return len;
}
EXPORT_SYMBOL(msend);

/* send sk_buf packet to msocket */
int msendskb(int sock, struct sk_buff *skb, int len, int flags)
{
	struct portq *portq;
	struct portq_group *pgrp;
	struct shm_skhdr *hdr;
	int length;
	bool block = flags == MSOCKET_KERNEL;
	if (NULL == skb) {
		pr_err("MSOCK:%s:skb buff is NULL!\n", __func__);
		return -1;
	}
	portq = (struct portq *)portq_get(sock);
	if (!portq) {
		pr_err("MSOCK: %s: sock %d not opened!\n",
		       __func__, sock);
		kfree_skb(skb);
		return -1;
	}

	pgrp = portq_get_group(portq);

	length = skb->len;
	if (length > (pgrp->rbctl->tx_skbuf_size - sizeof(*hdr))) {
		pr_err(
		       "MSOCK: %s: port %d, len is %d larger than tx_skbuf_size\n",
		       __func__, portq->port, len);
		kfree_skb(skb);
		portq->stat_tx_drop++;
		return -1;
	}

	hdr = (struct shm_skhdr *)skb_push(skb, sizeof(*hdr));
	hdr->address = 0;
	hdr->port = portq->port;
	hdr->checksum = 0;
	hdr->length = len;

	if (!portq_is_synced(portq) || portq_xmit(portq, skb, block) < 0) {
		kfree_skb(skb);
		pr_err("MSOCK: %s: port %d xmit error.\n",
		       __func__, portq->port);
		return -1;
	}
	return 0;
}
EXPORT_SYMBOL(msendskb);

/* receive packet from msocket */
int mrecv(int sock, void *buf, int len, int flags)
{
	struct portq *portq;
	struct sk_buff *skb;
	struct shm_skhdr *hdr;
	int packet_len;
	bool block = flags == MSOCKET_KERNEL;

	portq = (struct portq *)portq_get(sock);
	if (!portq) {
		pr_err("MSOCK: %s: sock %d not opened!\n",
		       __func__, sock);
		return -1;
	}

	skb = portq_recv(portq, block);
	if (IS_ERR(skb)) {
		pr_debug("MSOCK: %s: portq_recv returns %p\n",
		       __func__, skb);
		return -1;
	}

	if (!skb)
		return 0;

	hdr = (struct shm_skhdr *)skb->data;
	packet_len = hdr->length;
	if (packet_len > len) {
		pr_err("MSOCK: %s: error: no enough space.\n",
		       __func__);
		kfree_skb(skb);
		return -1;	/* error */
	}

	memcpy(buf, skb_pull(skb, sizeof(*hdr)), hdr->length);

	kfree_skb(skb);

	return packet_len;
}
EXPORT_SYMBOL(mrecv);

struct sk_buff *mrecvskb(int sock, int len, int flags)
{
	struct portq *portq;
	struct sk_buff *skb;
	struct shm_skhdr *hdr;
	bool block = flags == MSOCKET_KERNEL;

	portq = (struct portq *)portq_get(sock);
	if (!portq) {
		pr_err("MSOCK: %s: sock %d not opened!\n",
		       __func__, sock);
		return NULL;
	}

	skb = portq_recv(portq, block);
	if (IS_ERR(skb)) {
		pr_debug("MSOCK: %s: portq_recv returns %p\n",
		       __func__, skb);
		return NULL;
	}

	if (!skb)
		return NULL;

	hdr = (struct shm_skhdr *)skb->data;
	if (hdr->length > len) {
		pr_err("MSOCK: %s: error: no enough space.\n",
		       __func__);
		kfree_skb(skb);
		return NULL;	/* error */
	}
	skb_pull(skb, sizeof(*hdr));
	return skb;
}
EXPORT_SYMBOL(mrecvskb);

static void (*cp_synced_callback)(void);

void register_first_cp_synced(void (*ready_cb)(void))
{
	cp_synced_callback = ready_cb;
}
EXPORT_SYMBOL(register_first_cp_synced);


static void cp_sync_worker(struct work_struct *work)
{
	bool cb_notify;

	cb_notify = false;

	/* acquire lock first */
	spin_lock(&cp_sync_lock);

	while (!cp_is_sync_canceled) {
		/* send peer sync notify */
		shm_notify_peer_sync(portq_grp[portq_grp_cp_main].rbctl);

		/* unlock before wait completion */
		spin_unlock(&cp_sync_lock);

		if (wait_for_completion_timeout(&cp_peer_sync, HZ)) {
			/* we get CP sync response here */
			pr_info("msocket connection sync with CP O.K.!\n");
			/* acquire lock again */
			spin_lock(&cp_sync_lock);

			if (!cp_is_sync_canceled) {
				/* if no one cancel me */
				cp_is_synced = true;
				/* only when we have received linkup ioctl
				 * can we report the linkup message */
				if (cp_recv_up_ioc) {
					portq_broadcast_msg(
						portq_grp_cp_main,
						MsocketLinkupProcId);
					data_path_link_up();
					direct_rb_broadcast_msg
					    (MsocketLinkupProcId);
					cp_recv_up_ioc = false;
				} else {
					cb_notify  = true;
				}
			}
			break;
		}
		/* acquire lock again */
		spin_lock(&cp_sync_lock);
	}

	/* unlock before return */
	spin_unlock(&cp_sync_lock);

	if (cb_notify && cp_synced_callback)
		cp_synced_callback();
}

/* thottle portq receive by msocket */
void msocket_recv_throttled(int sock)
{
	struct portq *portq;
	portq = (struct portq *)portq_get(sock);
	if (!portq) {
		pr_err("MSOCK: %s: sock %d not opened!\n",
		       __func__, sock);
		return;
	}

	portq_recv_throttled(portq);
}
EXPORT_SYMBOL(msocket_recv_throttled);

/* unthottle portq receive by msocket */
void msocket_recv_unthrottled(int sock)
{
	struct portq *portq;
	portq = (struct portq *)portq_get(sock);
	if (!portq) {
		pr_err("MSOCK: %s: sock %d not opened!\n",
		       __func__, sock);
		return;
	}

	portq_recv_unthrottled(portq);
}
EXPORT_SYMBOL(msocket_recv_unthrottled);

/* start msocket sync */
static void msocket_connect(enum portq_grp_type grp_type)
{
	if (grp_type == portq_grp_cp_main) {
		spin_lock(&cp_sync_lock);
		cp_is_sync_canceled = false;
		spin_unlock(&cp_sync_lock);

		portq_schedule_sync(&portq_grp[portq_grp_cp_main], &sync_work);
	}
}

/* stop msocket sync */
static void msocket_disconnect(enum portq_grp_type grp_type)
{
	if (grp_type == portq_grp_cp_main) {
		spin_lock(&cp_sync_lock);

		/* flag used to cancel any new packet activity */
		cp_is_synced = false;

		/* flag used to cancel potential peer sync worker */
		cp_is_sync_canceled = true;

		spin_unlock(&cp_sync_lock);
	} else if (grp_type == portq_grp_m3) {
		m3_is_synced = false;
	}

	/* ensure that any scheduled work has run to completion */
	portq_flush_workqueue(grp_type);

	/*
	 * and now no work is active or will be schedule, so we can
	 * cleanup any packet queued and initialize some key data
	 * structure to the beginning state
	 */
	shm_rb_data_init(portq_grp[grp_type].rbctl);
	portq_flush_init(grp_type);

}

/*
 * msocket device driver <-------------------------------------->
 */

enum seq_dump_item {
	/* 0 ~ portq_grp_cnt - 1 */
	seq_dump_dummy = portq_grp_cnt,
	seq_dump_diag,
	seq_dump_end,
};

static inline int pos2seq(int pos)
{
	struct portq_group *pgrp = portq_grp;
	struct portq_group *pgrp_end = portq_grp + portq_grp_cnt;
	int max_port = (pgrp_end - 1)->port_offset +
		(pgrp_end - 1)->port_cnt;

	for (; pgrp != pgrp_end; pgrp++) {
		if (pos < pgrp->port_offset + pgrp->port_cnt &&
			pos >= pgrp->port_offset)
			return pgrp - portq_grp;
	}
	/*
	 * ugly workaround
	 * TODO: rewrite them
	 */

	if (pos < max_port)
		return seq_dump_dummy;

	return pos - max_port + 1 + seq_dump_dummy;
}

#define PROC_FILE_NAME		"driver/msocket"

/*
 * This function is called at the beginning of a sequence.
 */
static void *msocket_seq_start(struct seq_file *s, loff_t *pos)
{
	void *v;
	int seq = pos2seq((int)*pos);

	if (seq < portq_grp_cnt)
		spin_lock_irq(&portq_grp[seq].list_lock);

	v = (void *)(*pos + SEQ_START_TOKEN);

	/* return a non null value to begin the sequence */
	return v;
}

/*
 * This function is called after the beginning of a sequence.
 * It's called until the return is NULL (this ends the sequence).
 */
static void *msocket_seq_next(struct seq_file *s, void *v, loff_t *pos)
{
	int old_seq = pos2seq((int)*pos);
	int new_seq = pos2seq((int)*pos + 1);

	if (old_seq != new_seq) {
		if (old_seq < portq_grp_cnt)
			spin_unlock_irq(&portq_grp[old_seq].list_lock);
		if (new_seq < portq_grp_cnt)
			spin_lock_irq(&portq_grp[new_seq].list_lock);
	}

	if (new_seq >= seq_dump_end)
		return NULL;

	++*pos;

	v = (void *)(*pos + SEQ_START_TOKEN);

	/* return a non null value to step the sequence */
	return v;
}

/*
 * This function is called at the end of a sequence
 */
static void msocket_seq_stop(struct seq_file *s, void *v)
{
	if (v) {
		long pos = (long)v - (long)SEQ_START_TOKEN;
		int seq = pos2seq(pos);

		if (seq < portq_grp_cnt)
			spin_unlock_irq(&portq_grp[seq].list_lock);
	}
}

/*
 * This function is called for each "step" of a sequence
 */
static int msocket_seq_show(struct seq_file *s, void *v)
{
	long pos = (long)v - (long)SEQ_START_TOKEN;
	int seq = pos2seq(pos);

	if (seq < portq_grp_cnt) {
		struct portq_group *pgrp = &portq_grp[seq];
		int idx = pos - pgrp->port_offset;
		struct portq *portq;

		if (!pgrp->is_inited || idx > pgrp->port_cnt)
			return 0;

		portq = pgrp->port_list[idx];

		if (!idx) {
			struct shm_rbctl *rbctl = pgrp->rbctl;

			seq_printf(s, "shm_is_ap_xmit_stopped: %d\n",
				rbctl->is_ap_xmit_stopped);
			seq_printf(s, "shm_is_cp_xmit_stopped: %d\n",
				rbctl->is_cp_xmit_stopped);
			seq_printf(s, "acipc_ap_stopped_num:   %ld\n",
				rbctl->ap_stopped_num);
			seq_printf(s, "acipc_ap_resumed_num:   %ld\n",
				rbctl->ap_resumed_num);
			seq_printf(s, "acipc_cp_stopped_num:   %ld\n",
				rbctl->cp_stopped_num);
			seq_printf(s, "acipc_cp_resumed_num:   %ld\n",
				rbctl->cp_resumed_num);
			seq_printf(s, "tx_socket_total:        %d\n",
				rbctl->tx_skbuf_num);
			seq_printf(s, "tx_socket_free:         %d\n",
				shm_free_tx_skbuf_safe(rbctl));
			seq_printf(s, "rx_socket_total:        %d\n",
				rbctl->rx_skbuf_num);
			seq_printf(s, "rx_socket_free:         %d\n",
				shm_free_rx_skbuf_safe(rbctl));

			seq_printf(s, "rx_workq_sched_num:   %d\n",
				pgrp->rx_workq_sched_num);

			seq_puts(s, "\nport  ");
			seq_puts(s,
				"tx_current  tx_request"
				"  tx_sent  tx_drop  tx_queue_max"
				"  rx_current  rx_indicate "
				"  rx_got  rx_queue_max"
				"  ap_throttle_cp  ap_unthrottle_cp"
				"  cp_throttle_ap  cp_unthrottle_ap\n");
		}

		if (portq) {
			spin_lock(&portq->lock);
			seq_printf(s, "%4d", portq->port);
			seq_printf(s, "%12d", skb_queue_len(&portq->tx_q));
			seq_printf(s, "%12ld", portq->stat_tx_request);
			seq_printf(s, "%9ld", portq->stat_tx_sent);
			seq_printf(s, "%9ld", portq->stat_tx_drop);
			seq_printf(s, "%14ld", portq->stat_tx_queue_max);
			seq_printf(s, "%12d", skb_queue_len(&portq->rx_q));
			seq_printf(s, "%13ld", portq->stat_rx_indicate);
			seq_printf(s, "%9ld", portq->stat_rx_got);
			seq_printf(s, "%14ld", portq->stat_rx_queue_max);
			seq_printf(s, "%16ld", portq->stat_fc_ap_throttle_cp);
			seq_printf(s, "%18ld", portq->stat_fc_ap_unthrottle_cp);
			seq_printf(s, "%16ld", portq->stat_fc_cp_throttle_ap);
			seq_printf(s, "%18ld\n",
				portq->stat_fc_cp_unthrottle_ap);
			spin_unlock(&portq->lock);
		}
	} else if (seq == seq_dump_diag) {
		/* diag */
		struct direct_rbctl *dir_ctl;
		const struct direct_rbctl *dir_ctl_end =
			direct_rbctl + direct_rb_type_total_cnt;
		seq_puts(s, "\ndirect_rb:\n");
		for (dir_ctl = direct_rbctl; dir_ctl != dir_ctl_end;
			++dir_ctl) {
			seq_printf(s,
				"tx_sent: %lu, tx_drop: %lu,"
				" rx_fail: %lu, rx_got: %lu, rx_drop: %lu,"
				" interrupt: %lu, broadcast_msg: %lu\n",
				dir_ctl->stat_tx_sent, dir_ctl->stat_tx_drop,
				dir_ctl->stat_rx_fail, dir_ctl->stat_rx_got,
				dir_ctl->stat_rx_drop, dir_ctl->stat_interrupt,
				dir_ctl->stat_broadcast_msg);
		}
	}

	return 0;
}

static void msocket_dump_port(enum portq_grp_type grp_type)
{
	struct portq_group *pgrp = &portq_grp[grp_type];
	struct shm_rbctl *rbctl = pgrp->rbctl;
	struct portq *portq;
	int i;

	pr_err("shm_is_ap_xmit_stopped: %d\n",
	       rbctl->is_ap_xmit_stopped);
	pr_err("shm_is_cp_xmit_stopped: %d\n",
	       rbctl->is_cp_xmit_stopped);
	pr_err("acipc_ap_stopped_num:   %ld\n",
	       rbctl->ap_stopped_num);
	pr_err("acipc_ap_resumed_num:   %ld\n",
	       rbctl->ap_resumed_num);
	pr_err("acipc_cp_stopped_num:   %ld\n",
	       rbctl->cp_stopped_num);
	pr_err("acipc_cp_resumed_num:   %ld\n",
	       rbctl->cp_resumed_num);
	pr_err("tx_socket_total:        %d\n",
	       rbctl->tx_skbuf_num);
	pr_err("tx_socket_free:         %d\n",
	       shm_free_tx_skbuf(rbctl));
	pr_err("rx_socket_total:        %d\n",
	       rbctl->rx_skbuf_num);
	pr_err("rx_socket_free:         %d\n",
	       shm_free_rx_skbuf(rbctl));

	pr_err("\nport  tx_current  tx_request  tx_sent  tx_drop"
	       "  tx_queue_max"
	       "  rx_current  rx_indicate   rx_got  rx_queue_max"
	       "  ap_throttle_cp  ap_unthrottle_cp"
	       "  cp_throttle_ap  cp_unthrottle_ap\n");
	for (i = 0; i < pgrp->port_cnt; i++) {
		portq = pgrp->port_list[i];
		if (!portq)
			continue;
		pr_err("%4d %12d %12ld %9ld %9ld %14ld %12d %13ld "
		       "%9ld %14ld %16ld %18ld %16ld %18ld\n",
		       portq->port, skb_queue_len(&portq->tx_q),
		       portq->stat_tx_request, portq->stat_tx_sent,
		       portq->stat_tx_drop, portq->stat_tx_queue_max,
		       skb_queue_len(&portq->rx_q), portq->stat_rx_indicate,
		       portq->stat_rx_got, portq->stat_rx_queue_max,
		       portq->stat_fc_ap_throttle_cp,
		       portq->stat_fc_ap_unthrottle_cp,
		       portq->stat_fc_cp_throttle_ap,
		       portq->stat_fc_cp_unthrottle_ap);
	}
}

/**
 * This structure gather "function" to manage the sequence
 *
 */
static const struct seq_operations msocket_seq_ops = {
	.start = msocket_seq_start,
	.next = msocket_seq_next,
	.stop = msocket_seq_stop,
	.show = msocket_seq_show
};

/**
 * This function is called when the /proc file is open.
 *
 */
static int msocket_seq_open(struct inode *inode, struct file *file)
{
	return seq_open(file, &msocket_seq_ops);
};

/**
 * This structure gather "function" that manage the /proc file
 *
 */
static const struct file_operations msocket_proc_fops = {
	.owner = THIS_MODULE,
	.open = msocket_seq_open,
	.read = seq_read,
	.llseek = seq_lseek,
	.release = seq_release
};

/* open msocket */
static int msocket_open(struct inode *inode, struct file *filp)
{
	/*
	 * explicit set private_data to NULL, we'll use this pointer to
	 * associate file and portq
	 */
	filp->private_data = NULL;

	return 0;
}

/* open msocket */
static int msockdev_open(struct inode *inode, struct file *filp)
{
	struct portq *portq;
	int port;
	struct cmsockdev_dev *dev;	/* device information */
	struct direct_rbctl *drbctl = NULL;

	dev = container_of(inode->i_cdev, struct cmsockdev_dev, cdev);
	/* Extract Minor Number */
	port = MINOR(dev->cdev.dev);

	if (port == DIAG_PORT) {
		drbctl = direct_rb_open(direct_rb_type_diag, port);
		if (drbctl) {
			pr_info("diag is opened by process id:%d (%s)\n",
			       current->tgid, current->comm);
		} else {
			pr_info("diag open fail by process id:%d (%s)\n",
			       current->tgid, current->comm);
			return -1;
		}
	}
	portq = portq_open(port);
	if (IS_ERR(portq)) {
		if (port == DIAG_PORT)
			direct_rb_close(drbctl);
		pr_info("MSOCK: binding port %d error, %ld\n",
		       port, PTR_ERR(portq));
		return PTR_ERR(portq);
	} else {
		filp->private_data = portq;
		pr_info("MSOCK: binding port %d, success.\n", port);
		pr_info("MSOCK: port %d is opened by process id:%d (%s)\n",
		       port, current->tgid, current->comm);
		return 0;
	}
	return 0;
}

/* close msocket */
static int msocket_close(struct inode *inode, struct file *filp)
{
	struct portq *portq = filp->private_data;

	int port;
	struct cmsockdev_dev *dev;	/* device information */

	dev = container_of(inode->i_cdev, struct cmsockdev_dev, cdev);
	/* Extract Minor Number */
	port = MINOR(dev->cdev.dev);
	if (port == DIAG_PORT) {
		direct_rb_close(&direct_rbctl[direct_rb_type_diag]);
		pr_info(
		       "%s diag rb is closed by process id:%d (%s)\n",
		       __func__, current->tgid, current->comm);
	}
	if (portq) {		/* file already bind to portq */
		pr_info(
		       "MSOCK: port %d is closed by process id:%d (%s)\n",
		       portq->port, current->tgid, current->comm);
		portq_close(portq);
	}

	return 0;
}

/* read from msocket */
static ssize_t
msocket_read(struct file *filp, char __user *buf, size_t len, loff_t *f_pos)
{
	struct portq *portq;
	struct sk_buff *skb;
	struct shm_skhdr *hdr;
	int rc = -EFAULT;

	portq = (struct portq *)filp->private_data;
	if (!portq) {
		pr_err("MSOCK: %s: port not bind.\n", __func__);
		return rc;
	}

	if (portq->port == DIAG_PORT)
		return direct_rb_recv(direct_rb_type_diag, buf, len);
	skb = portq_recv(portq, true);

	if (IS_ERR(skb)) {
		pr_debug("MSOCK: %s: portq_recv returns %p\n",
		       __func__, skb);
		return PTR_ERR(skb);
	}

	hdr = (struct shm_skhdr *)skb->data;
	if (hdr->length > len) {
		pr_err("MSOCK: %s: error: no enough space.\n",
		       __func__);
		goto err_exit;
	}

	if (copy_to_user(buf, skb_pull(skb, sizeof(*hdr)), hdr->length))
		pr_err("MSOCK: %s: copy_to_user failed.\n", __func__);
	else
		rc = hdr->length;

err_exit:
	kfree_skb(skb);
	return rc;
}

static unsigned int msocket_poll(struct file *filp, poll_table *wait)
{
	struct portq *portq;

	portq = (struct portq *)filp->private_data;

	if (!portq) {
		pr_err("MSOCK: %s: port not bind.\n", __func__);
		return 0;
	}

	return portq_poll(portq, filp, wait);
}

/* write to msocket */
static ssize_t
msocket_write(struct file *filp, const char __user *buf, size_t len,
	      loff_t *f_pos)
{
	struct portq *portq;
	struct portq_group *pgrp;
	struct sk_buff *skb;
	struct shm_skhdr *hdr;
	int rc = -EFAULT;

	portq = (struct portq *)filp->private_data;
	if (!portq) {
		pr_err("MSOCK: %s: port not bind.\n", __func__);
		return rc;
	}

	if (portq->port == DIAG_PORT)
		return direct_rb_xmit(direct_rb_type_diag, buf, len);

	pgrp = portq_get_group(portq);

	if (len > (pgrp->rbctl->tx_skbuf_size - sizeof(*hdr))) {
		pr_err("MSOCK: %s: port %d, len is %d!!\n",
		       __func__, portq->port, (int)len);
		return rc;
	}

	skb = alloc_skb(len + sizeof(*hdr), GFP_KERNEL);
	if (!skb) {
		pr_err("MSOCK: %s: out of memory.\n", __func__);
		return -ENOMEM;
	}
	skb_reserve(skb, sizeof(*hdr));	/* reserve header space */

	if (copy_from_user(skb_put(skb, len), buf, len)) {
		kfree_skb(skb);
		pr_err("MSOCK: %s: copy_from_user failed.\n",
		       __func__);
		return rc;
	}

	skb_push(skb, sizeof(*hdr));
	hdr = (struct shm_skhdr *)skb->data;
	hdr->address = 0;
	hdr->port = portq->port;
	hdr->checksum = 0;
	hdr->length = len;

	if (!portq_is_synced(portq) || portq_xmit(portq, skb, true) < 0) {
		kfree_skb(skb);
		pr_err("MSOCK: %s: portq xmit error.\n", __func__);
		return -1;
	}

	return len;
}

/*  the ioctl() implementation */
static long msocket_ioctl(struct file *filp,
			  unsigned int cmd, unsigned long arg)
{
	struct portq *portq;
	int port, status, network_mode;

	struct direct_rbctl *drbctl = NULL;
	struct shm_rbctl *portq_rbctl = portq_grp[portq_grp_cp_main].rbctl;

	/*
	 * extract the type and number bitfields, and don't decode
	 * wrong cmds: return ENOTTY (inappropriate ioctl) before access_ok()
	 */
	if (_IOC_TYPE(cmd) != MSOCKET_IOC_MAGIC)
		return -ENOTTY;
	if (_IOC_NR(cmd) > MSOCKET_IOC_MAXNR)
		return -ENOTTY;

	if (aponly && (MSOCKET_IOC_ERRTO != cmd) && (MSOCKET_IOC_RECOVERY != cmd))
		return -1;

	switch (cmd) {
	case MSOCKET_IOC_BIND:
		port = arg;

		if (port == DIAG_PORT) {
			drbctl = direct_rb_open(direct_rb_type_diag, port);
			if (drbctl) {
				pr_info("diag path is opened by process id:%d (%s)\n",
				       current->tgid, current->comm);
			} else {
				pr_info("diag path open fail by process id:%d (%s)\n",
				       current->tgid, current->comm);
				return -1;
			}
		}
		portq = portq_open(port);
		if (IS_ERR(portq)) {
			if (port == DIAG_PORT)
				direct_rb_close(drbctl);
			pr_info("MSOCK: binding port %d error, %p\n",
			       port, portq);
			return (long)portq;
		} else {
			filp->private_data = portq;
			pr_info("MSOCK: binding port %d, success.\n",
			       port);
			pr_info("MSOCK: port %d is opened by process id:%d (%s)\n",
			       port, current->tgid, current->comm);
			return 0;
		}

	case MSOCKET_IOC_UP:
		pr_info("MSOCK: MSOCKET_UP is received!\n");
		/*
		 * in the case AP initiative reset CP, AP will first
		 * make msocket linkdown then hold, so CP still can
		 * send packet to share memory in this interval
		 * cleanup share memory one more time in msocket linkup
		 */
		shm_rb_data_init(portq_rbctl);
		spin_lock(&cp_sync_lock);
		cp_recv_up_ioc = true;
		spin_unlock(&cp_sync_lock);
		/* ensure completion cleared before start */
		reinit_completion(&cp_peer_sync);
		msocket_connect(portq_grp_cp_main);
		if (wait_for_completion_timeout(&cp_peer_sync, 5 * HZ) ==
		    0) {
			pr_info("MSOCK: sync with CP FAIL\n");
			return -1;
		}
		return 0;

	case MSOCKET_IOC_DOWN:
		pr_info("MSOCK: MSOCKET_DOWN is received!\n");
		msocket_dump_port(portq_grp_cp_main);
		msocket_dump_direct_rb();
		msocket_disconnect(portq_grp_cp_main);
		/* ok! the world's silent then notify the upper layer */
		portq_broadcast_msg(portq_grp_cp_main, MsocketLinkdownProcId);
		data_path_link_down();
		direct_rb_broadcast_msg(MsocketLinkdownProcId);
		return 0;

	case MSOCKET_IOC_PMIC_QUERY:
		pr_info("MSOCK: MSOCKET_PMIC_QUERY is received!\n");
		status = shm_is_cp_pmic_master(portq_rbctl) ? 1 : 0;
		if (copy_to_user((void *)arg, &status, sizeof(int)))
			return -1;
		else
			return 0;

	case MSOCKET_IOC_CONNECT:
		pr_info("MSOCK: MSOCKET_IOC_CONNECT is received!\n");
		msocket_connect(portq_grp_cp_main);
		return 0;

	case MSOCKET_IOC_RESET_CP_REQUEST:
		pr_info("MSOCK: MSOCKET_IOC_RESET_CP_REQUEST is received!\n");
		acipc_reset_cp_request();
		return 0;

	case MSOCKET_IOC_ERRTO: /* m3 timeout */
		pr_info("MSOCK: MSOCKET_IOC_ERRTO is received!\n");
		amipc_dump_debug_info();
		msocket_disconnect(portq_grp_m3);
		portq_broadcast_msg(portq_grp_m3, MsocketLinkdownProcId);
		return 0;

	case MSOCKET_IOC_RECOVERY: /* m3 recovery */
		reinit_completion(&m3_peer_sync);
		msocket_connect(portq_grp_m3);
		pr_info("MSOCK: MSOCKET_IOC_RECOVERY is received!\n");
		if (wait_for_completion_timeout(&m3_peer_sync, 5 * HZ) ==
			0) {
			pr_info("MSOCK: sync with M3 FAIL\n");
			return -1;
		}
		portq_broadcast_msg(portq_grp_m3, MsocketLinkupProcId);
		return 0;
	case MSOCKET_IOC_NETWORK_MODE_CP_NOTIFY: /*notify CP network mode*/
		network_mode = (int)arg;
		mutex_lock(&portq_rbctl->va_lock);
		if (portq_rbctl->skctl_va) {
			portq_rbctl->skctl_va->network_mode = network_mode;
			pr_info("MSOCK: network mode:%d\n",
				network_mode);
		}
		mutex_unlock(&portq_rbctl->va_lock);
		return 0;
	default:
		return -ENOTTY;
	}
}

/* driver methods */
static const struct file_operations msocket_fops = {
	.owner = THIS_MODULE,
	.open = msocket_open,
	.release = msocket_close,
	.read = msocket_read,
	.write = msocket_write,
	.unlocked_ioctl = msocket_ioctl,
#ifdef CONFIG_COMPAT
	.compat_ioctl = msocket_ioctl,
#endif
};

/* driver methods */
static const struct file_operations msockdev_fops = {
	.owner = THIS_MODULE,
	.open = msockdev_open,
	.release = msocket_close,
	.read = msocket_read,
	.write = msocket_write,
	.poll = msocket_poll,
	.unlocked_ioctl = msocket_ioctl,
#ifdef CONFIG_COMPAT
	.compat_ioctl = msocket_ioctl,
#endif
};

/* misc structure */
static struct miscdevice msocket_dev = {
	.minor = MISC_DYNAMIC_MINOR,
	.name = "msocket",
	.fops = &msocket_fops
};

static int msocketDump_open(struct inode *inode, struct file *filp)
{
	filp->private_data = (void *)(long)dump_flag;
	return 0;
}

static int msocketDump_close(struct inode *inode, struct file *filp)
{
	return 0;
}

static ssize_t msocketDump_read(struct file *filp, char __user *buf,
				size_t len, loff_t *f_pos)
{
	char temp[256];
	unsigned flag = dump_flag;

	sprintf(temp, "0x%08x", flag);
	if (copy_to_user(buf, (void *)&temp, strlen(temp) + 1)) {
		pr_err("MSOCKDUMP: %s: copy_to_user failed.\n",
		       __func__);
		return -EFAULT;
	}
	pr_info("msocketDump:get flag :%s\n", temp);
	/* return strlen(temp)+1; */
	return 0;
}

static ssize_t msocketDump_write(struct file *filp, const char __user *buf,
				 size_t len, loff_t *f_pos)
{
	unsigned flag = 0;

	if (kstrtouint_from_user(buf, len, 10, &flag) < 0) {
		pr_err("MSOCKDUMP: %s: kstrtoint error.\n",
			__func__);
		return -EFAULT;
	}
	pr_info("msocketDump:set flag :%08x\n", flag);
	dump_flag = flag;
	return len;
}

static const struct file_operations msocketDump_fops = {
	.owner = THIS_MODULE,
	.open = msocketDump_open,
	.release = msocketDump_close,
	.read = msocketDump_read,
	.write = msocketDump_write
};

static struct miscdevice msocketDump_dev = {
	.minor = MISC_DYNAMIC_MINOR,
	.name = "msocket_dump",
	.fops = &msocketDump_fops
};

static int cmsockdev_setup_cdev(struct cmsockdev_dev *dev, int index)
{
	int err = 0;
	int devno = MKDEV(cmsockdev_major, cmsockdev_minor + index);

	cdev_init(&dev->cdev, &msockdev_fops);
	dev->cdev.owner = THIS_MODULE;
	err = cdev_add(&dev->cdev, devno, 1);
	/* Fail gracefully if need be */
	if (err)
		pr_notice("Error %d adding cmsockdev%d", err, index);
	return err;
}

/*@cmsockdev_added: the number of successfully added cmsockt devices.
*/
void cmsockdev_cleanup_module(int cmsockdev_added)
{
	int i;
	dev_t devno = MKDEV(cmsockdev_major, cmsockdev_minor);

	/* Get rid of our char dev entries */
	if (cmsockdev_devices) {
		for (i = 0; i < cmsockdev_added; i++) {
			cdev_del(&cmsockdev_devices[i].cdev);
			device_destroy(cmsockdev_class,
				       MKDEV(cmsockdev_major,
					     cmsockdev_minor + i));
		}
		kfree(cmsockdev_devices);
	}

	class_destroy(cmsockdev_class);

	/* cleanup_module is never called if registering failed */
	unregister_chrdev_region(devno, cmsockdev_nr_devs);

}

int cmsockdev_init_module(void)
{
	int result, i = 0;
	dev_t dev = 0;
	char name[256];

	/*
	 * Get a range of minor numbers to work with, asking for a dynamic
	 * major unless directed otherwise at load time.
	 */
	if (cmsockdev_major) {
		dev = MKDEV(cmsockdev_major, cmsockdev_minor);
		result =
		    register_chrdev_region(dev, cmsockdev_nr_devs, "cmsockdev");
	} else {
		result =
		    alloc_chrdev_region(&dev, cmsockdev_minor,
					cmsockdev_nr_devs, "cmsockdev");
		cmsockdev_major = MAJOR(dev);
	}

	if (result < 0) {
		pr_warn("cmsockdev: can't get major %d\n",
		       cmsockdev_major);
		return result;
	}

	/*
	 * allocate the devices -- we can't have them static, as the number
	 * can be specified at load time
	 */
	cmsockdev_devices =
	    kzalloc(cmsockdev_nr_devs * sizeof(struct cmsockdev_dev),
		    GFP_KERNEL);
	if (!cmsockdev_devices) {
		result = -ENOMEM;
		goto fail;
	}

	/* Initialize each device. */
	cmsockdev_class = class_create(THIS_MODULE, "cmsockdev");
	for (i = 0; i < cmsockdev_nr_devs; i++) {
		sprintf(name, "%s%d", "cmsockdev", cmsockdev_minor + i);
		device_create(cmsockdev_class, NULL,
			      MKDEV(cmsockdev_major, cmsockdev_minor + i), NULL,
			      name);
		result = cmsockdev_setup_cdev(&cmsockdev_devices[i], i);
		if (result < 0)
			goto fail;

	}

	/* At this point call the init function for any friend device */

	return 0;		/* succeed */

fail:
	cmsockdev_cleanup_module(i);

	return result;
}

static int reboot_notifier_func(struct notifier_block *this,
	unsigned long code, void *cmd)
{
	pr_info("reboot notifier, notify CP\n");
	pr_info("%s: APMU_DEBUG byte3 %02x\n", __func__,
	       __raw_readb(APMU_DEBUG + 3));
	if (cp_is_synced)
		acipc_reset_cp_request();
	return 0;
}

static struct notifier_block reboot_notifier = {
	.notifier_call = reboot_notifier_func,
};

int cp_shm_ch_init(const struct cpload_cp_addr *addr, u32 lpm_qos)
{
	int rc;

	if (cp_shm_ch_inited) {
		pr_info("%s: channel is already inited\n",
			__func__);
		return 0;
	}

	if (addr->aponly) {
		pr_info("%s: aponly version\n",
			__func__);
		aponly = 1;
		return 0;
	}

	/* share memory area init */
	rc = tel_shm_init(shm_grp_cp, addr);
	if (rc < 0) {
		pr_err("%s: shm init failed %d\n",
			__func__, rc);
		return rc;
	}

	/* port queue group init */
	rc = portq_grp_init(&portq_grp[portq_grp_cp_main]);
	if (rc < 0) {
		pr_err("%s: portq group init failed %d\n",
			__func__, rc);
		goto portq_err;
	}

	/* data channel init */
	rc = data_path_init();
	if (rc < 0) {
		pr_err("%s: data path init failed %d\n",
			__func__, rc);
		goto dp_err;
	}

	/* direct rb init */
	rc = direct_rb_init();
	if (rc < 0) {
		pr_err("%s: direct rb init failed %d\n",
			__func__, rc);
		goto direct_rb_err;
	}

	/* acipc init */
	rc = acipc_init(lpm_qos);
	if (rc < 0) {
		pr_err("%s: acipcd init failed %d\n",
			__func__, rc);
		goto acipc_err;
	}

	/* start msocket peer sync */
	msocket_connect(portq_grp_cp_main);

	cp_shm_ch_inited = 1;

	pr_info("%s: shm channel init success\n", __func__);

	return 0;

acipc_err:
	direct_rb_exit();
direct_rb_err:
	data_path_exit();
dp_err:
	portq_grp_exit(&portq_grp[portq_grp_cp_main]);
portq_err:
	tel_shm_exit(shm_grp_cp);

	return rc;
}
EXPORT_SYMBOL(cp_shm_ch_init);

void cp_shm_ch_deinit(void)
{
	if (!cp_shm_ch_inited)
		return;
	cp_shm_ch_inited = 0;

	/* reverse order of initialization */
	msocket_disconnect(portq_grp_cp_main);
	acipc_exit();
	direct_rb_exit();
	data_path_exit();
	portq_grp_exit(&portq_grp[portq_grp_cp_main]);
	tel_shm_exit(shm_grp_cp);
}
EXPORT_SYMBOL(cp_shm_ch_deinit);

int m3_shm_ch_init(const struct rm_m3_addr *addr)
{
	int rc;

	if (m3_shm_ch_inited) {
		pr_info("%s: channel is already inited\n",
			__func__);
		return 0;
	}

	/* share memory area init */
	rc = tel_shm_init(shm_grp_m3, addr);
	if (rc < 0) {
		pr_err("%s: shm init failed %d\n",
			__func__, rc);
		return rc;
	}

	/* port queue group init */
	rc = portq_grp_init(&portq_grp[portq_grp_m3]);
	if (rc < 0) {
		pr_err("%s: portq group init failed %d\n",
			__func__, rc);
		goto portq_err;
	}

	/* amipc init */
	rc = amipc_init();
	if (rc < 0) {
		pr_err("%s: acipcd init failed %d\n",
			__func__, rc);
		goto amipc_err;
	}

	/* start msocket peer sync */
	msocket_connect(portq_grp_m3);

	m3_shm_ch_inited = 1;

	pr_info("%s: shm channel init success\n", __func__);

	return 0;

amipc_err:
	portq_grp_exit(&portq_grp[portq_grp_m3]);
portq_err:
	tel_shm_exit(shm_grp_m3);

	return rc;
}
EXPORT_SYMBOL(m3_shm_ch_init);

void m3_shm_ch_deinit(void)
{
	if (!m3_shm_ch_inited)
		return;
	m3_shm_ch_inited = 0;

	/* reverse order of initialization */
	msocket_disconnect(portq_grp_m3);
	amipc_exit();
	portq_grp_exit(&portq_grp[portq_grp_m3]);
	tel_shm_exit(shm_grp_m3);
}
EXPORT_SYMBOL(m3_shm_ch_deinit);


#define APMU_DEBUG_BUS_PROTECTION (1 << 4)
/* module initialization */
static int __init msocket_init(void)
{
	int rc;
	u8 apmu_debug_byte3;

	/* map common register base address */
	if (!map_apmu_base_va()) {
		pr_err("error to ioremap APMU_BASE_ADDR\n");
		return -ENOENT;
	}

	/* init lock */
	spin_lock_init(&cp_sync_lock);
	shm_lock_init();

	rc = shm_debugfs_init();
	if (rc < 0) {
		pr_err("%s: shm debugfs init failed\n", __func__);
		goto unmap_apmu;
	}

	/* create proc file */
	if (!proc_create(PROC_FILE_NAME, 0666, NULL, &msocket_proc_fops)) {
		pr_err("%s: create proc failed\n", __func__);
		rc = -1;
		goto proc_err;
	}

	cp_shm_ch_inited = 0;
	m3_shm_ch_inited = 0;

	/*enable bus protection*/
	apmu_debug_byte3 = __raw_readb(APMU_DEBUG + 3);
	apmu_debug_byte3 |= APMU_DEBUG_BUS_PROTECTION;
	__raw_writeb(apmu_debug_byte3, APMU_DEBUG + 3);
	register_reboot_notifier(&reboot_notifier);

	/* port queue init */
	rc = portq_init();
	if (rc < 0) {
		pr_err("%s: portq init failed %d\n",
			__func__, rc);
		goto portq_err;
	}

	/* register misc device */
	rc = misc_register(&msocket_dev);
	if (rc < 0) {
		pr_err("%s: register msock driver failed %d\n",
			__func__, rc);
		goto misc_err;
	}

	rc = misc_register(&msocketDump_dev);
	if (rc < 0) {
		pr_err("%s: register msock dump driver failed %d\n",
			__func__, rc);
		goto msocketDump_err;
	}

	rc = cmsockdev_init_module();
	if (rc < 0) {
		pr_err("%s: init cmoskdev failed %d\n",
			__func__, rc);
		goto cmsock_err;
	}

	return 0;

cmsock_err:
	misc_deregister(&msocketDump_dev);
msocketDump_err:
	misc_deregister(&msocket_dev);
misc_err:
	portq_exit();
portq_err:
	unregister_reboot_notifier(&reboot_notifier);
	remove_proc_entry(PROC_FILE_NAME, NULL);
proc_err:
	shm_debugfs_exit();
unmap_apmu:
	unmap_apmu_base_va();
	return rc;
}

/* module exit */
static void __exit msocket_exit(void)
{
	portq_exit();
	cmsockdev_cleanup_module(cmsockdev_nr_devs);
	misc_deregister(&msocketDump_dev);
	misc_deregister(&msocket_dev);
	unregister_reboot_notifier(&reboot_notifier);
	remove_proc_entry(PROC_FILE_NAME, NULL);
	shm_debugfs_exit();
	if (cp_shm_ch_inited)
		cp_shm_ch_deinit();
	cp_shm_ch_inited = 0;
	if (m3_shm_ch_inited)
		m3_shm_ch_deinit();
	m3_shm_ch_inited = 0;

	/* unmap common registers */
	unmap_apmu_base_va();
}

module_init(msocket_init);
module_exit(msocket_exit);


MODULE_LICENSE("GPL");
MODULE_AUTHOR("Marvell");
MODULE_DESCRIPTION("Marvell MSocket Driver");
