/*
 * Copyright (C) 2011 Samsung Electronics.
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#define CREATE_TRACE_POINTS

#include <linux/module.h>

#include "modem_prj.h"
#include "modem_utils.h"
#include "link_device_memory.h"
#include "link_ctrlmsg_iosm.h"

static unsigned long tx_timer_ns = 1000000;
module_param(tx_timer_ns, ulong, S_IRUGO | S_IWUSR);
MODULE_PARM_DESC(tx_timer_ns, "modem_v1 tx_timer period time");

/**
@brief		common interrupt handler for all MEMORY interfaces

@param mld	the pointer to a mem_link_device instance
@param msb	the pointer to a mst_buff instance
*/
void mem_irq_handler(struct mem_link_device *mld, struct mst_buff *msb)
{
	struct link_device *ld = &mld->link_dev;
	struct modem_ctl *mc = ld->mc;
	u16 intr = msb->snapshot.int2ap;

	if (unlikely(!int_valid(intr))) {
		mif_err("%s: ERR! invalid intr 0x%X\n", ld->name, intr);
		msb_free(msb);
		return;
	}

	if (unlikely(!rx_possible(mc))) {
		mif_err("%s: ERR! %s.state == %s\n", ld->name, mc->name,
			mc_state(mc));
		msb_free(msb);
		return;
	}

	msb_queue_tail(&mld->msb_rxq, msb);

	tasklet_schedule(&mld->rx_tsk);
}

/**
@brief		check whether or not IPC link is active

@param mld	the pointer to a mem_link_device instance

@retval "TRUE"	if IPC via the mem_link_device instance is possible.
@retval "FALSE"	otherwise.
*/
static inline bool ipc_active(struct mem_link_device *mld)
{
	struct link_device *ld = &mld->link_dev;
	struct modem_ctl *mc = ld->mc;

	if (unlikely(!cp_online(mc))) {
		mif_err("%s<->%s: %s.state %s != ONLINE <%pf>\n",
			ld->name, mc->name, mc->name, mc_state(mc), CALLER);
		return false;
	}

	if (atomic_read(&mc->forced_cp_crash)) {
		mif_err("%s<->%s: ERR! forced_cp_crash:%d <%pf>\n",
			ld->name, mc->name, atomic_read(&mc->forced_cp_crash),
			CALLER);
		return false;
	}

	if (mld->dpram_magic) {
		unsigned int magic = get_magic(mld);
		unsigned int access = get_access(mld);
		if (magic != MEM_IPC_MAGIC || access != 1) {
			mif_err("%s<->%s: ERR! magic:0x%X access:%d <%pf>\n",
				ld->name, mc->name, magic, access, CALLER);
			return false;
		}
	}

	return true;
}

static inline void stop_tx(struct mem_link_device *mld)
{
	struct link_device *ld = &mld->link_dev;
	stop_net_ifaces(ld);
}

static inline void purge_txq(struct mem_link_device *mld)
{
	struct link_device *ld = &mld->link_dev;
	int i;

	/* Purge the skb_q in every TX RB */
	if (ld->sbd_ipc) {
		struct sbd_link_device *sl = &mld->sbd_link_dev;
		for (i = 0; i < sl->num_channels; i++) {
			struct sbd_ring_buffer *rb = sbd_id2rb(sl, i, TX);
			skb_queue_purge(&rb->skb_q);
		}
	}

	/* Purge the skb_txq in every IPC device (IPC_FMT, IPC_RAW, etc.) */
	for (i = 0; i < MAX_SIPC5_DEVICES; i++) {
		struct mem_ipc_device *dev = mld->dev[i];
		skb_queue_purge(dev->skb_txq);
	}
}

static inline void start_tx_timer(struct mem_link_device *mld,
				  struct hrtimer *timer)
{
	struct link_device *ld = &mld->link_dev;
	struct modem_ctl *mc = ld->mc;
	unsigned long flags;

	spin_lock_irqsave(&mc->lock, flags);

	if (unlikely(cp_offline(mc)))
		goto exit;

	if (!hrtimer_is_queued(timer)) {
		ktime_t ktime = ktime_set(0, tx_timer_ns);
		hrtimer_start(timer, ktime, HRTIMER_MODE_REL);
	}

exit:
	spin_unlock_irqrestore(&mc->lock, flags);
}

static inline void cancel_tx_timer(struct mem_link_device *mld,
				   struct hrtimer *timer)
{
	struct link_device *ld = &mld->link_dev;
	struct modem_ctl *mc = ld->mc;
	unsigned long flags;

	spin_lock_irqsave(&mc->lock, flags);

	if (hrtimer_active(timer))
		hrtimer_cancel(timer);

	spin_unlock_irqrestore(&mc->lock, flags);
}

static enum hrtimer_restart sbd_tx_timer_func(struct hrtimer *timer)
{
	struct mem_link_device *mld;
	struct link_device *ld;
	struct modem_ctl *mc;
	struct sbd_link_device *sl;
	int i;
	bool need_schedule;
	u16 mask;
	unsigned long flags = 0;

	mld = container_of(timer, struct mem_link_device, sbd_tx_timer);
	ld = &mld->link_dev;
	mc = ld->mc;
	sl = &mld->sbd_link_dev;

	need_schedule = false;
	mask = 0;

#ifdef CONFIG_LINK_POWER_MANAGEMENT
	if (cp_online(mc) && mld->forbid_cp_sleep) {
		mld->forbid_cp_sleep(mld, REFCNT_SBD);
	}
#endif

	spin_lock_irqsave(&mc->lock, flags);
	if (unlikely(!ipc_active(mld))) {
		spin_unlock_irqrestore(&mc->lock, flags);
		goto exit;
	}
	spin_unlock_irqrestore(&mc->lock, flags);

	if (mld->link_active) {
		if (!mld->link_active(mld)) {
			need_schedule = true;
			goto exit;
		}
	}

	for (i = 0; i < sl->num_channels; i++) {
		struct sbd_ring_buffer *rb = sbd_id2rb(sl, i, TX);
		int ret;

		ret = tx_frames_to_rb(rb);

		if (unlikely(ret < 0)) {
			if (ret == -EBUSY || ret == -ENOSPC) {
				need_schedule = true;
				mask = MASK_SEND_DATA;
				continue;
			} else {
				modemctl_notify_event(MDM_CRASH_INVALID_RB);
				need_schedule = false;
				goto exit;
			}
		}

		if (ret > 0)
			mask = MASK_SEND_DATA;

		if (!skb_queue_empty(&rb->skb_q))
			need_schedule = true;
	}

	if (!need_schedule) {
		for (i = 0; i < sl->num_channels; i++) {
			struct sbd_ring_buffer *rb;

			rb = sbd_id2rb(sl, i, TX);
			if (!rb_empty(rb)) {
				need_schedule = true;
				break;
			}
		}
	}

	if (mask) {
		spin_lock_irqsave(&mc->lock, flags);
		if (unlikely(!ipc_active(mld))) {
			spin_unlock_irqrestore(&mc->lock, flags);
			need_schedule = false;
			goto exit;
		}
		send_ipc_irq(mld, mask2int(mask));
		spin_unlock_irqrestore(&mc->lock, flags);
	}

exit:
	if (need_schedule) {
		ktime_t ktime = ktime_set(0, ms2ns(TX_PERIOD_MS));
		hrtimer_start(timer, ktime, HRTIMER_MODE_REL);
#ifdef CONFIG_LINK_POWER_MANAGEMENT
	} else {
		if (mld->permit_cp_sleep)
			mld->permit_cp_sleep(mld, REFCNT_SBD);
#endif
	}

	return HRTIMER_NORESTART;
}

/**
@brief		transmit an IPC message packet

@param mld	the pointer to a mem_link_device instance
@param ch	the channel ID
@param skb	the pointer to an skb that will be transmitted

@retval "> 0"	the size of the data in @b @@skb
@retval "< 0"	an error code (-ENODEV, -EBUSY)
*/
static int xmit_ipc_to_rb(struct mem_link_device *mld, enum sipc_ch_id ch,
			  struct sk_buff *skb)
{
	int ret;
	struct link_device *ld = &mld->link_dev;
	struct io_device *iod = skbpriv(skb)->iod;
	struct modem_ctl *mc = ld->mc;
	struct sbd_ring_buffer *rb = sbd_ch2rb(&mld->sbd_link_dev, ch, TX);
	struct sk_buff_head *skb_txq;
	unsigned long flags;

	if (!rb) {
		mif_err("%s: %s->%s: ERR! NO SBD RB {ch:%d}\n",
			ld->name, iod->name, mc->name, ch);
		return -ENODEV;
	}

	skb_txq = &rb->skb_q;

	spin_lock_irqsave(&rb->lock, flags);

	if (unlikely(skb_txq->qlen >= MAX_SKB_TXQ_DEPTH)) {
		mif_err_limited("%s: %s->%s: ERR! {ch:%d} "
				"skb_txq.len %d >= limit %d\n",
				ld->name, iod->name, mc->name, ch,
				skb_txq->qlen, MAX_SKB_TXQ_DEPTH);
		ret = -EBUSY;
	} else {
		skb->len = min_t(int, skb->len, rb->buff_size);

		ret = skb->len;
		skb_queue_tail(skb_txq, skb);
		start_tx_timer(mld, &mld->sbd_tx_timer);

		trace_mif_event(skb, skb->len, FUNC);
	}

	spin_unlock_irqrestore(&rb->lock, flags);

	return ret;
}

/**
@brief		pass a socket buffer to the DEMUX layer

Invokes the recv_skb_single method in the io_device instance to perform
receiving IPC messages from each skb.

@param mld	the pointer to a mem_link_device instance
@param skb	the pointer to an sk_buff instance

@retval "> 0"	if succeeded to pass an @b @@skb to the DEMUX layer
@retval "< 0"	an error code
*/
static void pass_skb_to_demux(struct mem_link_device *mld, struct sk_buff *skb)
{
	struct link_device *ld = &mld->link_dev;
	struct io_device *iod = skbpriv(skb)->iod;
	int ret;
	u8 ch = skbpriv(skb)->sipc_ch;

	if (unlikely(!iod)) {
		mif_err("%s: ERR! No IOD for CH.%d\n", ld->name, ch);
		dev_kfree_skb_any(skb);
		modemctl_notify_event(MDM_CRASH_INVALID_IOD);
		return;
	}

	log_ipc_pkt(LNK_RX, ch, skb);

	ret = iod->recv_skb_single(iod, ld, skb);
	if (unlikely(ret < 0)) {
		struct modem_ctl *mc = ld->mc;
		mif_err_limited("%s: %s<-%s: ERR! %s->recv_skb fail (%d)\n",
				ld->name, iod->name, mc->name, iod->name, ret);
		dev_kfree_skb_any(skb);
	}
}

static void pass_skb_to_net(struct mem_link_device *mld, struct sk_buff *skb)
{
	struct link_device *ld = &mld->link_dev;
	struct skbuff_private *priv;
	struct io_device *iod;
	int ret;

	priv = skbpriv(skb);
	if (unlikely(!priv)) {
		mif_err("%s: ERR! No PRIV in skb@%p\n", ld->name, skb);
		dev_kfree_skb_any(skb);
		modemctl_notify_event(MDM_CRASH_INVALID_SKBCB);
		return;
	}

	iod = priv->iod;
	if (unlikely(!iod)) {
		mif_err("%s: ERR! No IOD in skb@%p\n", ld->name, skb);
		dev_kfree_skb_any(skb);
		modemctl_notify_event(MDM_CRASH_INVALID_SKBIOD);
		return;
	}

	log_ipc_pkt(LNK_RX, iod->id, skb);

	ret = iod->recv_net_skb(iod, ld, skb);
	if (unlikely(ret < 0)) {
		struct modem_ctl *mc = ld->mc;
		mif_err_limited("%s: %s<-%s: ERR! %s->recv_net_skb fail (%d)\n",
				ld->name, iod->name, mc->name, iod->name, ret);
		dev_kfree_skb_any(skb);
	}
}

/**
@brief		try to extract all PS network frames from an SBD RB

In a while loop,\n
1) Receives each PS (network) data frame stored in the @b @@rb RB.\n
2) Pass the skb to NET_RX.\n

@param rb	the pointer to a mem_ring_buffer instance
@param budget	maximum number of packets to be received at one time

@retval "> 0"	if valid data received
@retval "= 0"	if no data received
@retval "< 0"	if ANY error
*/
static int rx_net_frames_from_rb(struct sbd_ring_buffer *rb, int budget)
{
	int rcvd = 0;
	struct link_device *ld = rb->ld;
	struct mem_link_device *mld = ld_to_mem_link_device(ld);
	unsigned int num_frames;

#ifdef CONFIG_LINK_DEVICE_NAPI
	num_frames = min_t(unsigned int, rb_usage(rb), budget);
#else
	num_frames = rb_usage(rb);
#endif

	while (rcvd < num_frames) {
		struct sk_buff *skb;

		skb = sbd_pio_rx(rb);
		if (!skb)
			break;

		/* The $rcvd must be accumulated here, because $skb can be freed
		   in pass_skb_to_net(). */
		rcvd++;

		pass_skb_to_net(mld, skb);
	}

	if (rcvd < num_frames) {
		struct io_device *iod = rb->iod;
		struct link_device *ld = rb->ld;
		struct modem_ctl *mc = ld->mc;
		mif_err("%s: %s<-%s: WARN! rcvd %d < num_frames %d\n",
			ld->name, iod->name, mc->name, rcvd, num_frames);
	}

	return rcvd;
}

/**
@brief		extract all IPC link frames from an SBD RB

In a while loop,\n
1) receives each IPC link frame stored in the @b @@RB.\n
2) passes it to the DEMUX layer immediately.\n

@param rb	the pointer to a mem_ring_buffer instance

@retval "> 0"	if valid data received
@retval "= 0"	if no data received
@retval "< 0"	if ANY error
*/
static int rx_ipc_frames_from_rb(struct sbd_ring_buffer *rb)
{
	int rcvd = 0;
	struct link_device *ld = rb->ld;
	struct mem_link_device *mld = ld_to_mem_link_device(ld);
	unsigned int qlen = rb->len;
	unsigned int in = *rb->wp;
	unsigned int out = *rb->rp;
	unsigned int num_frames = circ_get_usage(qlen, in, out);

	while (rcvd < num_frames) {
		struct sk_buff *skb;

		skb = sbd_pio_rx(rb);
		if (!skb) {
#ifdef CONFIG_SEC_MODEM_DEBUG
			panic("skb alloc failed.");
#else
			modemctl_notify_event(MDM_CRASH_NO_MEM);
#endif
			break;
		}

		/* The $rcvd must be accumulated here, because $skb can be freed
		   in pass_skb_to_demux(). */
		rcvd++;

		if (skbpriv(skb)->lnk_hdr) {
			u8 ch = rb->ch;
			u8 fch = sipc5_get_ch(skb->data);
			if (fch != ch) {
				mif_err("frm.ch:%d != rb.ch:%d\n", fch, ch);
				dev_kfree_skb_any(skb);

				modemctl_notify_event(MDM_EVENT_CP_ABNORMAL_RX);
				continue;
			}
		}

		pass_skb_to_demux(mld, skb);
	}

	if (rcvd < num_frames) {
		struct io_device *iod = rb->iod;
		struct modem_ctl *mc = ld->mc;
		mif_err("%s: %s<-%s: WARN! rcvd %d < num_frames %d\n",
			ld->name, iod->name, mc->name, rcvd, num_frames);
	}

	return rcvd;
}

int mem_netdev_poll(struct napi_struct *napi, int budget)
{
	int rcvd;
	struct vnet *vnet = netdev_priv(napi->dev);
	struct mem_link_device *mld =
		container_of(vnet->ld, struct mem_link_device, link_dev);
	struct sbd_ring_buffer *rb =
		sbd_ch2rb(&mld->sbd_link_dev, vnet->iod->id, RX);

	rcvd = rx_net_frames_from_rb(rb, budget);

	/* no more ring buffer to process */
	if (rcvd < budget) {
		napi_complete(napi);
		//vnet->ld->enable_irq(vnet->ld);
	}

	mif_debug("%d pkts\n", rcvd);

	return rcvd;
}

/**
@brief		receive all @b IPC message frames in all RXQs

In a for loop,\n
1) Checks any REQ_ACK received.\n
2) Receives all IPC link frames in every RXQ.\n
3) Sends RES_ACK if there was REQ_ACK from CP.\n
4) Checks any RES_ACK received.\n

@param mld	the pointer to a mem_link_device instance
@param mst	the pointer to a mem_snapshot instance
*/
void recv_sbd_ipc_frames(struct mem_link_device *mld)
{
	struct sbd_link_device *sl = &mld->sbd_link_dev;
	int i;

	for (i = 0; i < sl->num_channels; i++) {
		struct sbd_ring_buffer *rb = sbd_id2rb(sl, i, RX);

		if (unlikely(rb_empty(rb)))
			continue;

		if (likely(sipc_ps_ch(rb->ch))) {
#ifdef CONFIG_LINK_DEVICE_NAPI
			//mld->link_dev.disable_irq(&mld->link_dev);
			if (napi_schedule_prep(&rb->iod->napi))
				__napi_schedule(&rb->iod->napi);
#else
			rx_net_frames_from_rb(rb, 0);
#endif
		} else {
			rx_ipc_frames_from_rb(rb);
		}
	}
}

/**
@brief		function for IPC message reception

Invokes cmd_handler for a command or recv_ipc_frames for IPC messages.

@param mld	the pointer to a mem_link_device instance
*/
static void ipc_rx_func(struct mem_link_device *mld)
{
	struct sbd_link_device *sl = &mld->sbd_link_dev;

	while (1) {
		struct mst_buff *msb;
		u16 intr;

		msb = msb_dequeue(&mld->msb_rxq);
		if (!msb)
			break;

		intr = msb->snapshot.int2ap;

		if (cmd_valid(intr))
			mld->cmd_handler(mld, int2cmd(intr));

		if (sbd_active(sl))
			recv_sbd_ipc_frames(mld);
		else
			recv_ipc_frames(mld, &msb->snapshot);

		msb_free(msb);
	}
}

/**
@brief		function for BOOT/DUMP data reception

@param ws	the pointer to a work_struct instance

@remark		RX for BOOT/DUMP must be performed by a WORK in order to avoid
		memory shortage.
*/
static void udl_rx_work(struct work_struct *ws)
{
	struct mem_link_device *mld;

	mld = container_of(ws, struct mem_link_device, udl_rx_dwork.work);

	ipc_rx_func(mld);
}

static void mem_rx_task(unsigned long data)
{
	struct mem_link_device *mld = (struct mem_link_device *)data;
	struct link_device *ld = &mld->link_dev;
	struct modem_ctl *mc = ld->mc;

	if (likely(cp_online(mc)))
		ipc_rx_func(mld);
	else
		queue_delayed_work(ld->rx_wq, &mld->udl_rx_dwork, 0);
}

#ifdef GROUP_MEM_LINK_METHOD
/**
@brief		function for the @b init_comm method in a link_device instance

@param ld	the pointer to a link_device instance
@param iod	the pointer to an io_device instance
*/
static int mem_init_comm(struct link_device *ld, struct io_device *iod)
{
	struct mem_link_device *mld = to_mem_link_device(ld);
	struct modem_ctl *mc = ld->mc;
	struct io_device *check_iod;
	int id = iod->id;
	int fmt2rfs = (SIPC5_CH_ID_RFS_0 - SIPC5_CH_ID_FMT_0);
	int rfs2fmt = (SIPC5_CH_ID_FMT_0 - SIPC5_CH_ID_RFS_0);

	if (sipc5_udl_ch(id))
		return 0;

	if (atomic_read(&mld->cp_boot_done)) {
#ifdef CONFIG_LINK_CONTROL_MSG_IOSM
		struct sbd_link_device *sl = &mld->sbd_link_dev;
		struct sbd_ipc_device *sid = sbd_ch2dev(sl, iod->id);

		if (sid && atomic_read(&sid->config_done)) {
			tx_iosm_message(mld, IOSM_A2C_OPEN_CH, (u32 *)&id);
		} else {
			mif_err("%s isn't configured channel\n", iod->name);
			return -ENODEV;
		}
#endif
		return 0;
	}

	switch (id) {
	case SIPC5_CH_ID_FMT_0 ... SIPC5_CH_ID_FMT_9:
		tx_iosm_message(mld, IOSM_A2C_OPEN_CH, (u32 *)&id);
		check_iod = link_get_iod_with_channel(ld, (id + fmt2rfs));
		if (check_iod ? atomic_read(&check_iod->opened) : true) {
			mif_err("%s: %s->INIT_END->%s\n",
				ld->name, iod->name, mc->name);
			__tx_iosm_message(mld, IOSM_A2C_INIT_END);
			atomic_set(&mld->cp_boot_done, 1);
		} else {
			mif_err("%s is not opened yet\n", check_iod->name);
		}
		break;

	case SIPC5_CH_ID_RFS_0 ... SIPC5_CH_ID_RFS_9:
		tx_iosm_message(mld, IOSM_A2C_OPEN_CH, (u32 *)&id);
		check_iod = link_get_iod_with_channel(ld, (id + rfs2fmt));
		if (check_iod) {
			if (atomic_read(&check_iod->opened)) {
				mif_err("%s: %s->INIT_END->%s\n",
					ld->name, iod->name, mc->name);
				__tx_iosm_message(mld, IOSM_A2C_INIT_END);
				atomic_set(&mld->cp_boot_done, 1);
			} else {
				mif_err("%s not opened yet\n", check_iod->name);
			}
		}
		break;
#ifdef CONFIG_LINK_CONTROL_MSG_IOSM
	default:
		mif_err("%s channel is not ready\n", iod->name);
		return -ENODEV;
#endif
	}

	return 0;
}

/**
@brief		function for the @b terminate_comm method
		in a link_device instance

@param ld	the pointer to a link_device instance
@param iod	the pointer to an io_device instance
*/
static void mem_terminate_comm(struct link_device *ld, struct io_device *iod)
{
	struct mem_link_device *mld = to_mem_link_device(ld);
#ifdef CONFIG_LINK_CONTROL_MSG_IOSM
	struct sbd_link_device *sl = &mld->sbd_link_dev;
	struct sbd_ipc_device *sid = sbd_ch2dev(sl, iod->id);

	if (sid && atomic_read(&mld->cp_boot_done))
		tx_iosm_message(mld, IOSM_A2C_CLOSE_CH, (u32 *)&iod->id);
#endif
	if (sipc5_udl_ch(iod->id) && !atomic_read(&iod->opened))
		atomic_set(&mld->cp_boot_done, 0);
}

/**
@brief		function for the @b send method in a link_device instance

@param ld	the pointer to a link_device instance
@param iod	the pointer to an io_device instance
@param skb	the pointer to an skb that will be transmitted

@retval "> 0"	the length of data transmitted if there is NO ERROR
@retval "< 0"	an error code
*/
static int mem_send(struct link_device *ld, struct io_device *iod,
		    struct sk_buff *skb)
{
	struct mem_link_device *mld = to_mem_link_device(ld);
	struct modem_ctl *mc = ld->mc;
	enum dev_format id = iod->format;
	u8 ch = iod->id;

	switch (id) {
	case IPC_FMT:
	case IPC_RAW:
	case IPC_RFS:
		if (likely(sipc5_ipc_ch(ch))) {
			if (unlikely(!ipc_active(mld)))
				return -EIO;

			if (iod->sbd_ipc) {
				if (likely(sbd_active(&mld->sbd_link_dev)))
					return xmit_ipc_to_rb(mld, ch, skb);
				else
					return -ENODEV;
			} else {
				BUG_ON(1);
			}
		} else {
			return xmit_udl(mld, iod, ch, skb);
		}
		break;

	case IPC_BOOT:
	case IPC_DUMP:
		if (sipc5_udl_ch(ch))
			return xmit_udl(mld, iod, ch, skb);
		break;

	default:
		break;
	}

	mif_err("%s:%s->%s: ERR! Invalid IO device (format:%s id:%d)\n",
		ld->name, iod->name, mc->name, dev_str(id), ch);

	return -ENODEV;
}

/**
@brief		function for the @b boot_on method in a link_device instance

@param ld	the pointer to a link_device instance
@param iod	the pointer to an io_device instance
*/
static void mem_boot_on(struct link_device *ld, struct io_device *iod)
{
	struct mem_link_device *mld = to_mem_link_device(ld);
	unsigned long flags;

	atomic_set(&mld->cp_boot_done, 0);

	spin_lock_irqsave(&ld->lock, flags);
	ld->state = LINK_STATE_OFFLINE;
	spin_unlock_irqrestore(&ld->lock, flags);

#ifdef CONFIG_LTE_MODEM_XMM7260
	sbd_deactivate(&mld->sbd_link_dev);
#endif
	cancel_tx_timer(mld, &mld->sbd_tx_timer);

	if (mld->iosm) {
		memset(mld->base + CMD_RGN_OFFSET, 0, CMD_RGN_SIZE);
		mif_info("Control message region has been initialized\n");
	}

#ifdef CONFIG_LINK_CONTROL_MSG_IOSM
	iowrite32(IOSM_MAGIC, mld->base + CMD_RGN_OFFSET + IOSM_MAGIC_OFFSET);
#endif

	purge_txq(mld);
}


/**
@brief		function for the @b stop method in a link_device instance

@param ld	the pointer to a link_device instance

@remark		It must be invoked after mc->state has already been changed to
		not STATE_ONLINE.
*/
static void mem_close_tx(struct link_device *ld)
{
	struct mem_link_device *mld = to_mem_link_device(ld);
	unsigned long flags;

	spin_lock_irqsave(&ld->lock, flags);
	ld->state = LINK_STATE_OFFLINE;
	spin_unlock_irqrestore(&ld->lock, flags);

	stop_tx(mld);
}
#endif

#ifdef GROUP_MEM_LINK_SETUP
void __iomem *mem_vmap(phys_addr_t pa, size_t size, struct page *pages[])
{
	size_t num_pages = (size >> PAGE_SHIFT);
	pgprot_t prot = pgprot_writecombine(PAGE_KERNEL);
	size_t i;

	for (i = 0; i < num_pages; i++) {
		pages[i] = phys_to_page(pa);
		pa += PAGE_SIZE;
	}

	return vmap(pages, num_pages, VM_MAP, prot);
}

void mem_vunmap(void *va)
{
	vunmap(va);
}


/**
@brief		register a physical memory region for an IPC region

@param mld	the pointer to a mem_link_device instance
@param start	the physical address of an IPC region
@param size	the size of the IPC region
*/
int mem_register_ipc_rgn(struct mem_link_device *mld, phys_addr_t start,
			 size_t size)
{
	struct link_device *ld = &mld->link_dev;
	size_t num_pages = (size >> PAGE_SHIFT);
	struct page **pages;

	pages = kmalloc(sizeof(struct page *) * num_pages, GFP_ATOMIC);
	if (!pages)
		return -ENOMEM;

	mif_err("%s: IPC_RGN start:%pa size:%zu\n", ld->name, &start, size);

	mld->start = start;
	mld->size = size;
	mld->pages = pages;

	return 0;
}

/**
@brief		unregister a physical memory region for an IPC region

@param mld	the pointer to a mem_link_device instance
*/
void mem_unregister_ipc_rgn(struct mem_link_device *mld)
{
	kfree(mld->pages);
	mld->pages = NULL;
	mld->size = 0;
	mld->start = 0;
}

/**
@brief		setup the logical map for an IPC region

@param mld	the pointer to a mem_link_device instance
@param start	the physical address of an IPC region
@param size	the size of the IPC region
*/
int mem_setup_ipc_map(struct mem_link_device *mld)
{
	struct link_device *ld = &mld->link_dev;
	phys_addr_t start = mld->start;
	size_t size = mld->size;
	struct page **pages = mld->pages;
	u8 __iomem *base;

	if (mem_type_shmem(mld->type) && mld->size < SZ_4M)
		return -EINVAL;

	base = mem_vmap(start, size, pages);
	if (!base) {
		mif_err("%s: ERR! mem_vmap fail\n", ld->name);
		return -EINVAL;
	}
	memset(base, 0, size);

	mld->base = base;

	mif_err("%s: IPC_RGN phys_addr:%pa virt_addr:%p size:%zu\n",
		ld->name, &start, base, size);

	remap_4mb_map_to_ipc_dev(mld);

	return 0;
}

static int mem_rx_setup(struct link_device *ld)
{
	struct mem_link_device *mld = to_mem_link_device(ld);

	if (!zalloc_cpumask_var(&mld->dmask, GFP_KERNEL))
		return -ENOMEM;
	if (!zalloc_cpumask_var(&mld->imask, GFP_KERNEL))
		return -ENOMEM;
	if (!zalloc_cpumask_var(&mld->tmask, GFP_KERNEL))
		return -ENOMEM;

#ifdef CONFIG_ARGOS
	/* Below hard-coded mask values should be removed later on.
	 * Like net-sysfs, argos module also should support sysfs knob,
	 * so that user layer must be able to control these cpu mask. */
	cpumask_copy(mld->dmask, &hmp_slow_cpu_mask);

	cpumask_or(mld->imask, mld->imask, cpumask_of(3));

	argos_irq_affinity_setup_label(241, "IPC", mld->imask, mld->dmask);
#endif

	ld->rx_wq = alloc_workqueue(
			"mem_rx_work", WQ_HIGHPRI | WQ_CPU_INTENSIVE, 1);
	if (!ld->rx_wq) {
		mif_err("%s: ERR! fail to create rx_wq\n", ld->name);
		return -ENOMEM;
	}

	INIT_DELAYED_WORK(&ld->rx_delayed_work, link_to_demux_work);

	return 0;
}

/**
@brief		create a mem_link_device instance

@param type	the type of a memory interface medium
@param modem	the pointer to a modem_data instance (i.e. modem platform data)
*/
struct mem_link_device *mem_create_link_device(enum mem_iface_type type,
					       struct modem_data *modem)
{
	struct mem_link_device *mld;
	struct link_device *ld;
	int i;
	mif_err("+++\n");

	if (modem->ipc_version < SIPC_VER_50) {
		mif_err("%s<->%s: ERR! IPC version %d < SIPC_VER_50\n",
			modem->link_name, modem->name, modem->ipc_version);
		return NULL;
	}

	/*
	** Alloc an instance of mem_link_device structure
	*/
	mld = kzalloc(sizeof(struct mem_link_device), GFP_KERNEL);
	if (!mld) {
		mif_err("%s<->%s: ERR! mld kzalloc fail\n",
			modem->link_name, modem->name);
		return NULL;
	}

	/*
	** Retrieve modem-specific attributes value
	*/
	mld->type = type;
	mld->attrs = modem->link_attrs;

	/*====================================================================*\
		Initialize "memory snapshot buffer (MSB)" framework
	\*====================================================================*/
	if (msb_init() < 0) {
		mif_err("%s<->%s: ERR! msb_init() fail\n",
			modem->link_name, modem->name);
		goto error;
	}

	/*====================================================================*\
		Set attributes as a "link_device"
	\*====================================================================*/
	ld = &mld->link_dev;

	ld->name = modem->link_name;

	if (mld->attrs & LINK_ATTR(LINK_ATTR_SBD_IPC)) {
		mif_err("%s<->%s: LINK_ATTR_SBD_IPC\n", ld->name, modem->name);
		ld->sbd_ipc = true;
	}

	if (mld->attrs & LINK_ATTR(LINK_ATTR_IPC_ALIGNED)) {
		mif_err("%s<->%s: LINK_ATTR_IPC_ALIGNED\n",
			ld->name, modem->name);
		ld->aligned = true;
	}

	ld->ipc_version = modem->ipc_version;

	ld->mdm_data = modem;

	/*
	Set up link device methods
	*/
	ld->init_comm = mem_init_comm;
	ld->terminate_comm = mem_terminate_comm;
	ld->send = mem_send;
	ld->netdev_poll = mem_netdev_poll;

	ld->boot_on = mem_boot_on;
	if (mld->attrs & LINK_ATTR(LINK_ATTR_MEM_BOOT)) {
		if (mld->attrs & LINK_ATTR(LINK_ATTR_XMIT_BTDLR))
			ld->xmit_boot = mem_xmit_boot;
		ld->dload_start = mem_start_download;
		ld->firm_update = mem_update_firm_info;
	}

	if (mld->attrs & LINK_ATTR(LINK_ATTR_MEM_DUMP)) {
		ld->dump_start = mem_start_upload;
	}

	ld->close_tx = mem_close_tx;

	INIT_LIST_HEAD(&ld->list);

	skb_queue_head_init(&ld->sk_fmt_tx_q);
	skb_queue_head_init(&ld->sk_raw_tx_q);

	skb_queue_head_init(&ld->sk_fmt_rx_q);
	skb_queue_head_init(&ld->sk_raw_rx_q);

	for (i = 0; i < MAX_SIPC5_DEVICES; i++) {
		spin_lock_init(&ld->tx_lock[i]);
		spin_lock_init(&ld->rx_lock[i]);
	}

	spin_lock_init(&ld->netif_lock);
	atomic_set(&ld->netif_stopped, 0);

	if (mem_rx_setup(ld) < 0)
		goto error;

	/*====================================================================*\
		Set attributes as a "memory link_device"
	\*====================================================================*/
	if (mld->attrs & LINK_ATTR(LINK_ATTR_DPRAM_MAGIC)) {
		mif_err("%s<->%s: LINK_ATTR_DPRAM_MAGIC\n",
			ld->name, modem->name);
		mld->dpram_magic = true;
	}

#ifdef CONFIG_LINK_CONTROL_MSG_IOSM
	mld->iosm = true;
	mld->cmd_handler = iosm_event_bh;
	INIT_WORK(&mld->iosm_w, iosm_event_work);
#else
	mld->cmd_handler = mem_cmd_handler;
#endif

	/*====================================================================*\
		Initialize MEM locks, completions, bottom halves, etc
	\*====================================================================*/
	spin_lock_init(&mld->lock);

	/*
	** Initialize variables for TX & RX
	*/
	msb_queue_head_init(&mld->msb_rxq);
	msb_queue_head_init(&mld->msb_log);

	tasklet_init(&mld->rx_tsk, mem_rx_task, (unsigned long)mld);

	hrtimer_init(&mld->sbd_tx_timer, CLOCK_MONOTONIC, HRTIMER_MODE_REL);
	mld->sbd_tx_timer.function = sbd_tx_timer_func;

	/*
	** Initialize variables for CP booting and crash dump
	*/
	INIT_DELAYED_WORK(&mld->udl_rx_dwork, udl_rx_work);

	mif_err("---\n");
	return mld;

error:
	kfree(mld);
	mif_err("xxx\n");
	return NULL;
}

#endif

