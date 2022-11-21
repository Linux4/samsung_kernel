/*
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

#include <linux/sched.h>
#include <linux/wakelock.h>
#include <linux/version.h>
#if LINUX_VERSION_CODE >= KERNEL_VERSION(3, 4, 0)
#include <linux/export.h>
#endif
#include <linux/netdevice.h> /* dev_kfree_skb_any */

#include "shm_share.h"
#include "acipcd.h"
#include "shm.h"
#include "msocket.h"
#include "data_path.h"

/*
 * as we do tx/rx in interrupt context, we should avoid lock up the box
 */
#define MAX_TX_SHOTS   16
#define MAX_RX_SHOTS   16

extern struct wake_lock acipc_wakeup;
static struct wake_lock dp_rx_wakeup;
static struct wake_lock dp_tx_wakelock;

static struct data_path data_path[dp_type_total_cnt];

static enum shm_rb_type dp_rb[dp_type_total_cnt] = {
	shm_rb_psd
};

enum data_path_state {
	dp_state_idle,
	dp_state_opening,
	dp_state_opened,
	dp_state_closing,
};

static void data_path_tx_func(unsigned long arg)
{
	struct data_path *dp = (struct data_path *)arg;
	struct shm_rbctl *rbctl = dp->rbctl;
	struct shm_skctl *skctl = rbctl->skctl_va;
	struct shm_psd_skhdr *skhdr;
	struct sk_buff *packet;
	int slot;
	int i;

	wake_lock(&dp_tx_wakelock);

	DP_ENTER();

	for (i = 0; i < MAX_TX_SHOTS; i++) {
		if (rbctl->is_ap_xmit_stopped)
			break;

		if (!msocket_is_synced)
			break;

		if (shm_is_xmit_full(rbctl)) {
			rbctl->is_ap_xmit_stopped = true;
			acipc_notify_ap_psd_tx_stopped();

			DP_ERROR("%s tx stop\n", __func__);

			if (dp->cbs->tx_stop)
				dp->cbs->tx_stop();

			break;
		}

		if (dp->cbs->get_packet)
			packet = dp->cbs->get_packet();
		else
			packet = NULL;

		if (!packet) {
			DP_PRINT("%s: no packet available\n", __func__);
			break;
		}

		if (packet->len + sizeof *skhdr > rbctl->tx_skbuf_size) {
			DP_ERROR("%s: packet too large %d\n", __func__, packet->len);
			dev_kfree_skb_any(packet);
			if (dp->cbs->finish_tx)
				dp->cbs->finish_tx(dp_tx_packet_too_large);
			break;
		}

		/* push to ring buffer */
		slot = shm_get_next_tx_slot(dp->rbctl, skctl->ap_wptr);

		skhdr = (struct shm_psd_skhdr *)
			SHM_PACKET_PTR(rbctl->tx_va, slot, rbctl->tx_skbuf_size);

		skhdr->length = packet->len;
		memcpy(skhdr + 1, packet->data, packet->len);

		skctl->ap_wptr = slot;

		DP_PRINT("%s: xmit to shm with length %d\n", __func__, length);

		dev_kfree_skb_any(packet);
		if (dp->cbs->finish_tx)
			dp->cbs->finish_tx(dp_success);
	}

	if (i > 0)
		acipc_notify_psd_packet_sent();

	if (i == MAX_TX_SHOTS)
		data_path_schedule_tx(dp);

	wake_unlock(&dp_tx_wakelock);
	DP_LEAVE();
}

static void data_path_rx_func(unsigned long arg)
{
	struct data_path *dp = (struct data_path *)arg;
	struct shm_rbctl *rbctl = dp->rbctl;
	struct shm_skctl *skctl = rbctl->skctl_va;
	struct shm_psd_skhdr *skhdr;
	int slot;
	int count;
	enum data_path_result result;
	int i;

	DP_ENTER();

	for (i = 0; i < MAX_RX_SHOTS; i++) {
		if (!msocket_is_synced) {
			/* if not sync, just return */
			break;
		}

		/* process share memory socket buffer flow control */
		if (rbctl->is_cp_xmit_stopped
		    && shm_has_enough_free_rx_skbuf(rbctl)) {
			rbctl->is_cp_xmit_stopped = false;
			acipc_notify_cp_psd_tx_resume();
		}

		if (shm_is_recv_empty(rbctl)) {
			break;
		}

		slot = shm_get_next_rx_slot(rbctl, skctl->ap_rptr);

		skhdr =
		    (struct shm_psd_skhdr *)SHM_PACKET_PTR(rbctl->rx_va, slot,
							   rbctl->
							   rx_skbuf_size);

		count = skhdr->length + sizeof(*skhdr);

		if (count > rbctl->rx_skbuf_size) {
			DP_ERROR(KERN_EMERG
			       "%s: slot = %d, count = %d\n", __func__, slot,
			       count);
			goto error_length;
		}

		DP_PRINT("%s: recv from shm with length %d\n", __func__, skhdr->length);

		if (dp->cbs && dp->cbs->data_rx)
			result =
			    dp->cbs->data_rx((unsigned char *)(skhdr + 1),
						skhdr->length);
		else
			result = dp_success;

		DP_PRINT("%s: result of data_rx: %d\n", __func__, result);

		/*
		 * upper layer decide to keep the packet as pending
		 * and we need to return now
		 */
		if (result == dp_rx_keep_pending) {
			DP_ERROR("%s: packet is pending\n", __func__);
			break;
		}
error_length:
		skctl->ap_rptr = slot;
	}

	if (i == MAX_RX_SHOTS)
		data_path_schedule_rx(dp);
	else
		wake_unlock(&acipc_wakeup);

	DP_LEAVE();
}

void data_path_schedule_tx(struct data_path *dp)
{
	DP_ENTER();

	if (dp && atomic_read(&dp->state) == dp_state_opened)
		tasklet_schedule(&dp->tx_tl);

	DP_LEAVE();
}
EXPORT_SYMBOL(data_path_schedule_tx);

void data_path_schedule_rx(struct data_path *dp)
{
	DP_ENTER();

	if (dp && atomic_read(&dp->state) == dp_state_opened)
		tasklet_schedule(&dp->rx_tl);

	DP_LEAVE();
}
EXPORT_SYMBOL(data_path_schedule_rx);

void data_path_broadcast_msg(int proc)
{
	struct data_path *dp;
	const struct data_path *dp_end = data_path + dp_type_total_cnt;

	DP_ENTER();

	for (dp = data_path; dp != dp_end; ++dp) {
		if (atomic_read(&dp->state) == dp_state_opened) {
			if (proc == MsocketLinkdownProcId
			    && dp->cbs && dp->cbs->link_down)
				dp->cbs->link_down();
			else if (proc == MsocketLinkupProcId
				 && dp->cbs && dp->cbs->link_up)
				dp->cbs->link_up();
		}
	}

	DP_LEAVE();
}

struct data_path *data_path_open(enum data_path_type dp_type,
				       struct data_path_callback *cbs)
{
	DP_ENTER();

	if (dp_type >= dp_type_total_cnt || dp_type < 0) {
		DP_ERROR("%s: incorrect type %d\n", __func__, dp_type);
		return NULL;
	}

	if (!cbs) {
		DP_ERROR("%s: cbs is NULL\n", __func__);
		return NULL;
	}

	if (atomic_cmpxchg(&data_path[dp_type].state, dp_state_idle,
			   dp_state_opening) != dp_state_idle) {
		DP_ERROR("%s: path is already opened(state %d)\n",
		       __func__, atomic_read(&data_path[dp_type].state));
		return NULL;
	}

	data_path[dp_type].cbs = cbs;
	tasklet_init(&data_path[dp_type].tx_tl, data_path_tx_func,
		     (unsigned long)&data_path[dp_type]);
	tasklet_init(&data_path[dp_type].rx_tl, data_path_rx_func,
		     (unsigned long)&data_path[dp_type]);

	atomic_set(&data_path[dp_type].state, dp_state_opened);

	DP_LEAVE();

	return &data_path[dp_type];
}
EXPORT_SYMBOL(data_path_open);

void data_path_close(struct data_path *dp)
{
	DP_ENTER();

	if (!dp) {
		DP_ERROR("%s: empty data channel\n", __func__);
		return;
	}

	if (atomic_cmpxchg(&dp->state, dp_state_opened,
			   dp_state_closing) != dp_state_opened) {
		DP_ERROR("%s: path is already opened(state %d)\n",
		       __func__, atomic_read(&dp->state));
		return;
	}

	tasklet_kill(&dp->tx_tl);
	tasklet_kill(&dp->rx_tl);
	dp->cbs = NULL;

	atomic_set(&dp->state, dp_state_idle);

	DP_LEAVE();
}
EXPORT_SYMBOL(data_path_close);

void dp_rb_stop_cb(struct shm_rbctl *rbctl)
{
	struct data_path *dp;

	DP_ENTER();

	if (!rbctl)
		return;

	dp = rbctl->priv;

	wake_lock_timeout(&acipc_wakeup, HZ * 5);
	printk(KERN_WARNING "MSOCK: dp_rb_stop_cb!!!\n");

	if(dp && (atomic_read(&dp->state) == dp_state_opened)) {
		/* double check whether ring buffer is still full */
		if (shm_has_enough_free_rx_skbuf(rbctl))
			data_path_schedule_rx(dp);
		else if (dp->cbs && dp->cbs->rx_stop)
			dp->cbs->rx_stop();
	}

	DP_LEAVE();
}

void dp_rb_resume_cb(struct shm_rbctl *rbctl)
{
	struct data_path *dp;

	DP_ENTER();

	if (!rbctl)
		return;

	dp = rbctl->priv;

	wake_lock_timeout(&acipc_wakeup, HZ * 2);
	printk(KERN_WARNING "MSOCK: dp_rb_resume_cb!!!\n");

	if(dp && (atomic_read(&dp->state) == dp_state_opened)) {
		if (dp->cbs && dp->cbs->tx_resume)
			dp->cbs->tx_resume();
		data_path_schedule_tx(dp);
	}

	DP_LEAVE();
}

void dp_packet_send_cb(struct shm_rbctl *rbctl)
{
	struct data_path *dp;

	DP_ENTER();

	if (!rbctl)
		return;

	dp = rbctl->priv;

	wake_lock_timeout(&acipc_wakeup, HZ * 5);

	data_path_schedule_rx(dp);

	DP_LEAVE();
}

struct shm_callback dp_shm_cb = {
	.packet_send_cb  = dp_packet_send_cb,
	.rb_stop_cb      = dp_rb_stop_cb,
	.rb_resume_cb    = dp_rb_resume_cb,
};


int data_path_init(void)
{
	struct data_path *dp;
	struct data_path *dp2;
	const struct data_path *dp_end = data_path + dp_type_total_cnt;

	wake_lock_init(&dp_tx_wakelock, WAKE_LOCK_SUSPEND, "dp_tx_wakeups");
	wake_lock_init(&dp_rx_wakeup, WAKE_LOCK_SUSPEND, "dp_rx_wakeups");

	for (dp = data_path; dp != dp_end; ++dp) {
		dp->dp_type = dp - data_path;
		dp->rbctl = shm_open(dp_rb[dp - data_path], &dp_shm_cb, dp);
		if (!dp->rbctl) {
			DP_ERROR("%s: cannot open shm\n", __func__);
			goto exit;
		}
		atomic_set(&dp->state, dp_state_idle);
	}

	return 0;

exit:
	for (dp2 = data_path; dp2 != dp; ++dp2)
		shm_close(dp2->rbctl);

	wake_lock_destroy(&dp_tx_wakelock);
	wake_lock_destroy(&dp_rx_wakeup);

	return -1;
}

void data_path_exit(void)
{
	struct data_path *dp;
	const struct data_path *dp_end = data_path + dp_type_total_cnt;

	for (dp = data_path; dp != dp_end; ++dp)
		shm_close(dp->rbctl);

	wake_lock_destroy(&dp_tx_wakelock);
	wake_lock_destroy(&dp_rx_wakeup);
}
