/**
@file		link_device_memory_main.c
@brief		common functions for all types of memory interface media
@date		2014/02/05
@author		Hankook Jang (hankook.jang@samsung.com)
*/

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

#include "modem_prj.h"
#include "modem_utils.h"
#include "link_device_memory.h"

#ifdef GROUP_MEM_LINK_DEVICE
/**
@weakgroup group_mem_link_device
@{
*/

/**
@brief		check whether or not IPC link is active

@param mld	the pointer to a mem_link_device instance

@retval "TRUE"	if IPC via the mem_link_device instance is possible.
@retval "FALSE"	otherwise.
*/
static inline bool ipc_active(struct mem_link_device *mld)
{
	bool active;
	unsigned int magic = get_magic(mld);
	unsigned int access = get_access(mld);
	bool crash = mld->forced_cp_crash;

	if (magic != MEM_IPC_MAGIC || access != 1 || crash)
		active =  false;
	else
		active = true;

#ifdef DEBUG_MODEM_IF
	if (!active) {
		struct link_device *ld = &mld->link_dev;
		struct modem_ctl *mc = ld->mc;
		mif_err("%s<->%s: ERR! magic:0x%X access:%d crash:%d <%pf>\n",
			ld->name, mc->name, magic, access, crash, CALLER);
	}
#endif

	return active;
}

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

	tasklet_hi_schedule(&mld->rx_tsk);
}

/**
@}
*/
#endif

/*============================================================================*/

#ifdef GROUP_MEM_IPC_TX

/**
@weakgroup group_mem_ipc_tx

@dot
digraph mem_tx {
graph [
	label="\n<TX Flow of Memory-type Interface>\n\n"
	labelloc="top"
	fontname="Helvetica"
	fontsize=20
];

node [shape=box fontname="Helvetica"];

edge [fontname="Helvetica"];

node []
	mem_send [
		label="mem_send"
		URL="@ref mem_send"
	];

	xmit_udl [
		label="xmit_udl"
		URL="@ref xmit_udl"
	];

	xmit_ipc [
		label="xmit_ipc"
		URL="@ref xmit_ipc"
	];

	skb_txq [
		shape=record
		label="<f0>| |skb_txq| |<f1>"
	];

	start_tx_timer [
		label="start_tx_timer"
		URL="@ref start_tx_timer"
	];

	tx_timer_func [
		label="tx_timer_func"
		URL="@ref tx_timer_func"
	];

	tx_frames_to_dev [
		label="tx_frames_to_dev"
		URL="@ref tx_frames_to_dev"
	];

	txq_write [
		label="txq_write"
		URL="@ref txq_write"
	];

edge [color=blue fontcolor=blue];
	mem_send -> xmit_udl [
		label="1. UDL frame\n(skb)"
	];

	xmit_udl -> skb_txq:f1 [
		label="2. skb_queue_tail\n(UDL frame skb)"
		arrowhead=vee
		style=dashed
	];

	xmit_udl -> start_tx_timer [
		label="3. Scheduling"
	];

edge [color=brown fontcolor=brown];
	mem_send -> xmit_ipc [
		label="1. IPC frame\n(skb)"
	];

	xmit_ipc -> skb_txq:f1 [
		label="2. skb_queue_tail\n(IPC frame skb)"
		arrowhead=vee
		style=dashed
	];

	xmit_ipc -> start_tx_timer [
		label="3. Scheduling"
	];

edge [color=black fontcolor=black];
	start_tx_timer -> tx_timer_func [
		label="4. HR timer"
		arrowhead=vee
		style=dotted
	];

	tx_timer_func -> tx_frames_to_dev [
		label="for (every IPC device)"
	];

	skb_txq:f0 -> tx_frames_to_dev [
		label="5. UDL/IPC frame\n(skb)"
		arrowhead=vee
		style=dashed
	];

	tx_frames_to_dev -> txq_write [
		label="6. UDL/IPC frame\n(skb)"
	];
}
@enddot
*/

/**
@weakgroup group_mem_ipc_tx
@{
*/

/**
@brief		check the free space in a circular TXQ

@param mld	the pointer to a mem_link_device instance
@param dev	the pointer to a mem_ipc_device instance
@param qsize	the size of the buffer in @b @@dev TXQ
@param in	the IN (HEAD) pointer value of the TXQ
@param out	the OUT (TAIL) pointer value of the TXQ
@param count	the size of the data to be transmitted

@retval "> 0"	the size of free space in the @b @@dev TXQ
@retval "< 0"	an error code
*/
static inline int check_txq_space(struct mem_link_device *mld,
				  struct mem_ipc_device *dev,
				  unsigned int qsize, unsigned int in,
				  unsigned int out, unsigned int count)
{
	struct link_device *ld = &mld->link_dev;
	struct modem_ctl *mc = ld->mc;
	unsigned int usage;
	unsigned int space;

	if (!circ_valid(qsize, in, out)) {
		mif_err("%s: ERR! Invalid %s_TXQ{qsize:%d in:%d out:%d}\n",
			ld->name, dev->name, qsize, in, out);
		return -EIO;
	}

	usage = circ_get_usage(qsize, in, out);
	if (unlikely(usage > SHM_UL_USAGE_LIMIT) && cp_online(mc)) {
#ifdef DEBUG_MODEM_IF
		mif_debug("%s: CAUTION! BUSY in %s_TXQ{qsize:%d in:%d out:%d "\
			"usage:%d (count:%d)}\n", ld->name, dev->name, qsize,
			in, out, usage, count);
#endif
		return -EBUSY;
	}

	space = circ_get_space(qsize, in, out);
	if (unlikely(space < count)) {
#ifdef DEBUG_MODEM_IF
		if (cp_online(mc)) {
			mif_err("%s: CAUTION! NOSPC in %s_TXQ{qsize:%d in:%d "\
				"out:%d space:%d count:%d}\n", ld->name,
				dev->name, qsize, in, out, space, count);
		}
#endif
		return -ENOSPC;
	}

	return space;
}

/**
@brief		copy data in an skb to a circular TXQ

Enqueues a frame in @b @@skb to the @b @@dev TXQ if there is enough space in the
TXQ, then releases @b @@skb.

@param mld	the pointer to a mem_link_device instance
@param dev	the pointer to a mem_ipc_device instance
@param skb	the pointer to an sk_buff instance

@retval "> 0"	the size of the frame written in the TXQ
@retval "< 0"	an error code (-EBUSY, -ENOSPC, or -EIO)
*/
static int txq_write(struct mem_link_device *mld, struct mem_ipc_device *dev,
		     struct sk_buff *skb)
{
	char *src = skb->data;
	unsigned int count = skb->len;
	char *dst = get_txq_buff(dev);
	unsigned int qsize = get_txq_buff_size(dev);
	unsigned int in = get_txq_head(dev);
	unsigned int out = get_txq_tail(dev);
	int space;

	space = check_txq_space(mld, dev, qsize, in, out, count);
	if (unlikely(space < 0))
		return space;

#ifdef DEBUG_MODEM_IF_LINK_TX
	/* Record the time-stamp */
	getnstimeofday(&skbpriv(skb)->ts);
#endif

	circ_write(dst, src, qsize, in, count);

	set_txq_head(dev, circ_new_ptr(qsize, in, count));

	/* Commit the item before incrementing the head */
	smp_mb();

#ifdef DEBUG_MODEM_IF_LINK_TX
	if (likely(src))
		log_ipc_pkt(sipc5_get_ch_id(src), LINK, TX, skb, true, true);
#endif

	dev_kfree_skb_any(skb);

	return count;
}

/**
@brief		try to transmit all IPC frames in an skb_txq to CP

@param mld	the pointer to a mem_link_device instance
@param dev	the pointer to a mem_ipc_device instance

@retval "> 0"	accumulated data size written to a circular TXQ
@retval "< 0"	an error code
*/
static int tx_frames_to_dev(struct mem_link_device *mld,
			    struct mem_ipc_device *dev)
{
	struct sk_buff_head *skb_txq = dev->skb_txq;
	int tx_bytes = 0;
	int ret = 0;

	while (1) {
		struct sk_buff *skb;

		skb = skb_dequeue(skb_txq);
		if (unlikely(!skb))
			break;

		ret = txq_write(mld, dev, skb);
		if (unlikely(ret < 0)) {
			/* Take the skb back to the skb_txq */
			skb_queue_head(skb_txq, skb);
			break;
		}

		tx_bytes += ret;
	}

	return (ret < 0) ? ret : tx_bytes;
}

static enum hrtimer_restart tx_timer_func(struct hrtimer *timer)
{
	struct mem_link_device *mld;
	struct link_device *ld;
	struct modem_ctl *mc;
	int i;
	bool need_schedule;
	u16 mask;

	mld = container_of(timer, struct mem_link_device, tx_timer);
	ld = &mld->link_dev;
	mc = ld->mc;

	if (unlikely(cp_online(mc) && !ipc_active(mld)))
		goto exit;

	need_schedule = false;
	mask = 0;

	for (i = IPC_FMT; i < MAX_SIPC5_DEV; i++) {
		struct mem_ipc_device *dev = mld->dev[i];
		int ret;

		if (unlikely(under_tx_flow_ctrl(mld, dev))) {
			ret = check_tx_flow_ctrl(mld, dev);
			if (ret < 0) {
				if (ret == -EBUSY || ret == -ETIME) {
					need_schedule = true;
					continue;
				} else {
					mem_forced_cp_crash(mld);
					goto exit;
				}
			}
		}

		ret = tx_frames_to_dev(mld, dev);
		if (unlikely(ret < 0)) {
			if (ret == -EBUSY || ret == -ENOSPC) {
				need_schedule = true;
				start_tx_flow_ctrl(mld, dev);
				continue;
			} else {
				mem_forced_cp_crash(mld);
				goto exit;
			}
		}

		mask |= msg_mask(dev);

		if (!skb_queue_empty(dev->skb_txq))
			need_schedule = true;
	}

	if (mask)
		send_ipc_irq(mld, mask2int(mask));

	if (need_schedule) {
		ktime_t ktime = ns_to_ktime(ms2ns(TX_PERIOD_MS));
		hrtimer_forward_now(timer, ktime);
		return HRTIMER_RESTART;
	}

exit:
#ifdef CONFIG_LINK_POWER_MANAGEMENT
	mld->permit_cp_sleep(mld);
#endif
	return HRTIMER_NORESTART;
}

static inline void start_tx_timer(struct mem_link_device *mld,
				  struct hrtimer *timer)
{
	ktime_t ktime = ktime_set(0, ms2ns(TX_PERIOD_MS));

	if (hrtimer_active(timer))
		return;

#ifdef CONFIG_LINK_POWER_MANAGEMENT
	mld->forbid_cp_sleep(mld);
#endif
	hrtimer_start(timer, ktime, HRTIMER_MODE_REL);
}

/**
@brief		check whether or not TX is possible via the link

@param mld	the pointer to a mem_link_device instance
@param dev	the pointer to a mem_ipc_device instance (IPC_FMT, etc.)
@param skb	the pointer to an skb that will be transmitted

@retval "> 0"	the size of the data in @b @@skb if there is NO ERROR
@retval "< 0"	an error code (-EIO or -EBUSY)
*/
static inline int check_tx_link(struct mem_link_device *mld,
				struct mem_ipc_device *dev,
				struct sk_buff *skb)
{
	struct link_device *ld = &mld->link_dev;
	struct modem_ctl *mc = ld->mc;
	struct sk_buff_head *skb_txq = dev->skb_txq;
	int ret = skb->len;

	if (unlikely(cp_online(mc) && !ipc_active(mld)))
		return -EIO;

	if (unlikely(skb_txq->qlen >= MAX_SKB_TXQ_DEPTH)) {
#ifdef DEBUG_MODEM_IF
		struct io_device *iod = skbpriv(skb)->iod;
		mif_debug("%s: %s->%s: ERR! %s "\
			  "SKB_TXQ qlen %d >= limit %d\n",
			  ld->name, iod->name, mc->name,
			  dev->name, skb_txq->qlen, MAX_SKB_TXQ_DEPTH);
#endif
		return -EBUSY;
	}

	return ret;
}

/**
@brief		transmit an IPC message packet

@param mld	the pointer to a mem_link_device instance
@param ch	the channel ID
@param skb	the pointer to an skb that will be transmitted

@retval "> 0"	the size of the data in @b @@skb
@retval "< 0"	an error code (-EIO or -EBUSY)
*/
static int xmit_ipc(struct mem_link_device *mld, enum sipc_ch_id ch,
		    struct sk_buff *skb)
{
	struct mem_ipc_device *dev = mld->dev[dev_id(ch)];
	int ret;
	unsigned long flags;

	spin_lock_irqsave(dev->tx_lock, flags);

	ret = check_tx_link(mld, dev, skb);
	if (unlikely(ret < 0))
		goto exit;

	skb_queue_tail(dev->skb_txq, skb);

	start_tx_timer(mld, &mld->tx_timer);

exit:
	spin_unlock_irqrestore(dev->tx_lock, flags);

	return ret;
}

/**
@brief		transmit a BOOT/DUMP data packet

@param mld	the pointer to a mem_link_device instance
@param ch	the channel ID
@param skb	the pointer to an skb that will be transmitted

@retval "> 0"	the size of the data in @b @@skb
@retval "< 0"	an error code (-EIO or -EBUSY)
*/
static int xmit_udl(struct mem_link_device *mld, enum sipc_ch_id ch,
		    struct sk_buff *skb)
{
	struct mem_ipc_device *dev = mld->dev[dev_id(ch)];
	int ret = skb->len;
	unsigned long flags;

	spin_lock_irqsave(dev->tx_lock, flags);

	skb_queue_tail(dev->skb_txq, skb);

	start_tx_timer(mld, &mld->tx_timer);

	spin_unlock_irqrestore(dev->tx_lock, flags);

	return ret;
}

/**
@}
*/
#endif

/*============================================================================*/

#ifdef GROUP_MEM_IPC_RX

/**
@weakgroup group_mem_ipc_rx

@dot
digraph mem_rx {
graph [
	label="\n<RX Flow of Memory-type Interface>\n\n"
	labelloc="top"
	fontname="Helvetica"
	fontsize=20
];

node [shape=box fontname="Helvetica"];

edge [fontname="Helvetica"];

node []
	subgraph irq_handling {
	node []
		mem_irq_handler [
			label="mem_irq_handler"
			URL="@ref mem_irq_handler"
		];

		mem_rx_task [
			label="mem_rx_task"
			URL="@ref mem_rx_task"
		];

		udl_rx_work [
			label="udl_rx_work"
			URL="@ref udl_rx_work"
		];

		ipc_rx_func [
			label="ipc_rx_func"
			URL="@ref ipc_rx_func"
		];

	edge [color="red:green4:blue" fontcolor=black];
		mem_irq_handler -> mem_rx_task [
			label="Tasklet"
		];

	edge [color=blue fontcolor=blue];
		mem_rx_task -> udl_rx_work [
			label="Delayed\nWork"
			arrowhead=vee
			style=dotted
		];

		udl_rx_work -> ipc_rx_func [
			label="Process\nContext"
		];

	edge [color="red:green4" fontcolor=brown];
		mem_rx_task -> ipc_rx_func [
			label="Tasklet\nContext"
		];
	}

	subgraph rx_processing {
	node []
		mem_cmd_handler [
			label="mem_cmd_handler"
			URL="@ref mem_cmd_handler"
		];

		recv_ipc_frames [
			label="recv_ipc_frames"
			URL="@ref recv_ipc_frames"
		];

		rx_frames_from_dev [
			label="rx_frames_from_dev"
			URL="@ref rx_frames_from_dev"
		];

		rxq_read [
			label="rxq_read"
			URL="@ref rxq_read"
		];

		schedule_link_to_demux [
			label="schedule_link_to_demux"
			URL="@ref schedule_link_to_demux"
		];

		link_to_demux_work [
			label="link_to_demux_work"
			URL="@ref link_to_demux_work"
		];

		pass_skb_to_demux [
			label="pass_skb_to_demux"
			URL="@ref pass_skb_to_demux"
		];

		skb_rxq [
			shape=record
			label="<f0>| |skb_rxq| |<f1>"
			color=green4
			fontcolor=green4
		];

	edge [color=black fontcolor=black];
		ipc_rx_func -> mem_cmd_handler [
			label="command"
		];

	edge [color="red:green4:blue" fontcolor=black];
		ipc_rx_func -> recv_ipc_frames [
			label="while (msb)"
		];

		recv_ipc_frames -> rx_frames_from_dev [
			label="1. for (each IPC device)"
		];

		rx_frames_from_dev -> rxq_read [
			label="2.\nwhile (rcvd < size)"
		];

	edge [color=blue fontcolor=blue];
		rxq_read -> rx_frames_from_dev [
			label="2-1.\nUDL skb\n[process]"
		];

		rx_frames_from_dev -> pass_skb_to_demux [
			label="2-2.\nUDL skb\n[process]"
		];

	edge [color="red:green4" fontcolor=brown];
		rxq_read -> rx_frames_from_dev [
			label="2-1.\nFMT skb\nRFS skb\nPS skb\n[tasklet]"
		];

	edge [color=red fontcolor=red];
		rx_frames_from_dev -> pass_skb_to_demux [
			label="2-2.\nFMT skb\nRFS skb\n[tasklet]"
		];

	edge [color=green4 fontcolor=green4];
		rx_frames_from_dev -> skb_rxq:f1 [
			label="2-2.\nPS skb\n[tasklet]"
			arrowhead=vee
			style=dashed
		];

	edge [color="red:green4" fontcolor=brown];
		rx_frames_from_dev -> recv_ipc_frames [
			label="3. Return"
		];

	edge [color=green4 fontcolor=green4];
		recv_ipc_frames -> schedule_link_to_demux [
			label="4. Scheduling"
		];

	edge [color="red:green4" fontcolor=brown];
		recv_ipc_frames -> ipc_rx_func [
			label="5. Return"
		];

	edge [color=green4 fontcolor=green4];
		schedule_link_to_demux -> link_to_demux_work [
			label="6. Delayed\nwork\n[process]"
			arrowhead=vee
			style=dotted
		];

		skb_rxq:f0 -> link_to_demux_work [
			label="6-1.\nPS skb"
			arrowhead=vee
			style=dashed
		];

		link_to_demux_work -> pass_skb_to_demux [
			label="6-2.\nPS skb"
		];
	}
}
@enddot
*/

/**
@weakgroup group_mem_ipc_rx
@{
*/

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
	struct io_device *iod;
	int ch;
	int ret;

	ch = sipc5_get_ch_id(skb->data);

	iod = link_get_iod_with_channel(ld, ch);
	if (unlikely(!iod)) {
		mif_err("%s: ERR! No IO device for Ch.%d\n", ld->name, ch);
		dev_kfree_skb_any(skb);
		mem_forced_cp_crash(mld);
		return;
	}

	/* Record the RX IO device into the "iod" field in &skb->cb */
	skbpriv(skb)->iod = iod;

	/* Record the RX link device into the "ld" field in &skb->cb */
	skbpriv(skb)->ld = ld;

#ifdef DEBUG_MODEM_IF_LINK_RX
	log_ipc_pkt(sipc5_get_ch_id(skb->data), LINK, RX, skb, true, true);
#endif

	ret = iod->recv_skb_single(iod, ld, skb);
	if (unlikely(ret < 0)) {
		mif_err("%s: ERR! %s->recv_skb_single fail (%d)\n",
			ld->name, iod->name, ret);
		dev_kfree_skb_any(skb);
	}
}

/**
@brief		pass socket buffers in every skb_rxq to the DEMUX layer

@param ws	the pointer to a work_struct instance

@see		schedule_link_to_demux()
@see		rx_frames_from_dev()
@see		mem_create_link_device()
*/
static void link_to_demux_work(struct work_struct *ws)
{
	struct link_device *ld;
	struct mem_link_device *mld;
	int i;

	ld = container_of(ws, struct link_device, rx_delayed_work.work);
	mld = to_mem_link_device(ld);

	for (i = IPC_FMT; i < MAX_SIPC5_DEV; i++) {
		struct mem_ipc_device *dev = mld->dev[i];
		struct sk_buff_head *skb_rxq = dev->skb_rxq;

		while (1) {
			struct sk_buff *skb;

			skb = skb_dequeue(skb_rxq);
			if (!skb)
				break;

			pass_skb_to_demux(mld, skb);
		}
	}
}

static inline void schedule_link_to_demux(struct mem_link_device *mld)
{
	struct link_device *ld = &mld->link_dev;
	struct delayed_work *dwork = &ld->rx_delayed_work;

#if 0/*def CONFIG_USB_ANDROID_SAMSUNG_COMPOSITE*/
	if (is_rndis_use() && cpu_online(1))
		queue_delayed_work_on(1, ld->rx_wq, dwork, 0);
	else
		queue_delayed_work(ld->rx_wq, dwork, 0);
#else
	queue_delayed_work(ld->rx_wq, dwork, 0);
#endif
}

/**
@brief		copy each IPC link frame from a circular queue to an skb

1) Analyzes a link frame header and get the size of the current link frame.\n
2) Allocates a socket buffer (skb).\n
3) Extracts a link frame from the current @b $out (tail) pointer in the @b
   @@dev RXQ up to @b @@in (head) pointer in the @b @@dev RXQ, then copies it
   to the skb allocated in the step 2.\n
4) Updates the TAIL (OUT) pointer in the @b @@dev RXQ.\n

@param mld	the pointer to a mem_link_device instance
@param dev	the pointer to a mem_ipc_device instance (IPC_FMT, etc.)
@param in	the IN (HEAD) pointer value of the @b @@dev RXQ

@retval "struct sk_buff *"	if there is NO error
@retval "NULL"		if there is ANY error
*/
static struct sk_buff *rxq_read(struct mem_link_device *mld,
				struct mem_ipc_device *dev,
				unsigned int in)
{
	struct link_device *ld = &mld->link_dev;
	struct sk_buff *skb;
	gfp_t priority;
	char *src = get_rxq_buff(dev);
	unsigned int qsize = get_rxq_buff_size(dev);
	unsigned int out = get_rxq_tail(dev);
	unsigned int rest = circ_get_usage(qsize, in, out);
	unsigned int len;
	char hdr[SIPC5_MIN_HEADER_SIZE];

	/* Copy the header in a frame to the header buffer */
	circ_read(hdr, src, qsize, out, SIPC5_MIN_HEADER_SIZE);

	/* Check the config field in the header */
	if (unlikely(!sipc5_start_valid(hdr))) {
		mif_err("%s: ERR! %s BAD CFG 0x%02X (in:%d out:%d rest:%d)\n",
			ld->name, dev->name, hdr[SIPC5_CONFIG_OFFSET],
			in, out, rest);
		goto bad_msg;
	}

	/* Check the channel ID field in the header */
	if (unlikely(!sipc5_get_ch_id(hdr))) {
		mif_err("%s: ERR! %s BAD CH.ID 0x%02X (in:%d out:%d rest:%d)\n",
			ld->name, dev->name, hdr[SIPC5_CH_ID_OFFSET],
			in, out, rest);
		goto bad_msg;
	}

	/* Verify the length of the frame (data + padding) */
	len = sipc5_get_total_len(hdr);
	if (unlikely(len > rest)) {
		mif_err("%s: ERR! %s BAD LEN %d > rest %d\n",
			ld->name, dev->name, len, rest);
		goto bad_msg;
	}

	/* Allocate an skb */
	priority = in_interrupt() ? GFP_ATOMIC : GFP_KERNEL;
	skb = alloc_skb(len + NET_SKB_PAD, priority);
	if (!skb) {
		mif_err("%s: ERR! %s alloc_skb(%d,0x%x) fail\n",
			ld->name, dev->name, (len + NET_SKB_PAD), priority);
		goto no_mem;
	}
	skb_reserve(skb, NET_SKB_PAD);

	/* Read the frame from the RXQ */
	circ_read(skb_put(skb, len), src, qsize, out, len);

	/* Update tail (out) pointer to the frame to be read in the future */
	set_rxq_tail(dev, circ_new_ptr(qsize, out, len));

	/* Finish reading data before incrementing tail */
	smp_mb();

#ifdef DEBUG_MODEM_IF_LINK_RX
	/* Record the time-stamp */
	getnstimeofday(&skbpriv(skb)->ts);
#endif

	return skb;

bad_msg:
#ifdef DEBUG_MODEM_IF
	pr_ipc(1, "CP2AP: BAD MSG", (src + out), 4);
#endif
	set_rxq_tail(dev, in);	/* Reset tail (out) pointer */
	mem_forced_cp_crash(mld);

no_mem:
	return NULL;
}

/**
@brief		extract all IPC link frames from a circular queue

In a while loop,\n
1) Receives each IPC link frame stored in the @b @@dev RXQ.\n
2) If the frame is a PS (network) data frame, stores it to an skb_rxq and
   schedules a delayed work for PS data reception.\n
3) Otherwise, passes it to the DEMUX layer immediately.\n

@param mld	the pointer to a mem_link_device instance
@param dev	the pointer to a mem_ipc_device instance (IPC_FMT, etc.)

@retval "> 0"	if valid data received
@retval "= 0"	if no data received
@retval "< 0"	if ANY error
*/
static int rx_frames_from_dev(struct mem_link_device *mld,
			      struct mem_ipc_device *dev)
{
	struct sk_buff_head *skb_rxq = dev->skb_rxq;
	unsigned int qsize = get_rxq_buff_size(dev);
	unsigned int in = get_rxq_head(dev);
	unsigned int out = get_rxq_tail(dev);
	unsigned int size = circ_get_usage(qsize, in, out);
	int rcvd = 0;

	if (unlikely(circ_empty(in, out)))
		return 0;

	while (rcvd < size) {
		struct sk_buff *skb;

		skb = rxq_read(mld, dev, in);
		if (!skb)
			break;

		/* The $rcvd must be accumulated here, because $skb can be freed
		   in pass_skb_to_demux(). */
		rcvd += skb->len;

		if (likely(sipc_ps_ch(sipc5_get_ch_id(skb->data))))
			skb_queue_tail(skb_rxq, skb);
		else
			pass_skb_to_demux(mld, skb);
	}

#ifdef DEBUG_MODEM_IF
	if (rcvd < size) {
		struct link_device *ld = &mld->link_dev;
		mif_err("%s: WARN! rcvd %d < size %d\n", ld->name, rcvd, size);
	}
#endif

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
static void recv_ipc_frames(struct mem_link_device *mld,
			    struct mem_snapshot *mst)
{
	int i;
	u16 intr = mst->int2ap;

	for (i = IPC_FMT; i < MAX_SIPC5_DEV; i++) {
		struct mem_ipc_device *dev = mld->dev[i];
		int rcvd;

#if 0/*defined(DEBUG_MODEM_IF)*/
		print_dev_snapshot(mld, mst, dev);
#endif

		if (req_ack_valid(dev, intr))
			recv_req_ack(mld, dev, mst);

		rcvd = rx_frames_from_dev(mld, dev);
		if (rcvd < 0)
			break;

		schedule_link_to_demux(mld);

		if (req_ack_valid(dev, intr))
			send_res_ack(mld, dev);

		if (res_ack_valid(dev, intr))
			recv_res_ack(mld, dev, mst);
	}
}

/**
@brief		function for IPC message reception

Invokes mem_cmd_handler for a command or recv_ipc_frames for IPC messages.

@param mld	the pointer to a mem_link_device instance
*/
static void ipc_rx_func(struct mem_link_device *mld)
{
	while (1) {
		struct mst_buff *msb;
		u16 intr;

		msb = msb_dequeue(&mld->msb_rxq);
		if (!msb)
			break;

		intr = msb->snapshot.int2ap;

		if (cmd_valid(intr))
			mem_cmd_handler(mld, int2cmd(intr));

		recv_ipc_frames(mld, &msb->snapshot);

#if 0/*defined(DEBUG_MODEM_IF)*/
		msb_queue_tail(&mld->msb_log, msb);
#else
		msb_free(msb);
#endif
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

/**
@}
*/
#endif

/*============================================================================*/

#ifdef GROUP_MEM_CP_CRASH
/**
@weakgroup group_mem_cp_crash
@{
*/

static void set_modem_state(struct mem_link_device *mld, enum modem_state state)
{
	struct link_device *ld = &mld->link_dev;
	struct io_device *iod;

	/* Change the modem state to STATE_CRASH_EXIT for the FMT IO device */
	iod = link_get_iod_with_format(ld, IPC_FMT);
	if (iod)
		iod->modem_state_changed(iod, state);

	/* time margin for taking state changes by rild */
	mdelay(100);

	/* Change the modem state to STATE_CRASH_EXIT for the BOOT IO device */
	iod = link_get_iod_with_format(ld, IPC_BOOT);
	if (iod)
		iod->modem_state_changed(iod, state);
}

void mem_handle_cp_crash(struct mem_link_device *mld, enum modem_state state)
{
	struct link_device *ld = &mld->link_dev;
	struct modem_ctl *mc = ld->mc;
	int i;

	/* Disable normal IPC */
	set_magic(mld, MEM_CRASH_MAGIC);
	set_access(mld, 0);

	if (!wake_lock_active(&mld->dump_wlock))
		wake_lock(&mld->dump_wlock);

	stop_net_ifaces(ld);

	/* Purge the skb_txq in every IPC device (IPC_FMT, IPC_RAW, etc.) */
	for (i = 0; i < MAX_SIPC5_DEV; i++)
		skb_queue_purge(mld->dev[i]->skb_txq);

	if (cp_online(mc))
		set_modem_state(mld, state);

	mld->forced_cp_crash = false;
}

/**
@brief		handle no CRASH_ACK from CP

This function will be invoked if there will have been no CRASH_ACK from CP in
FORCE_CRASH_ACK_TIMEOUT after AP sends a CP_CRASH request to CP.

@param arg	the pointer to a mem_link_device instance
*/
static void handle_no_cp_crash_ack(unsigned long arg)
{
	struct mem_link_device *mld = (struct mem_link_device *)arg;
	struct link_device *ld = &mld->link_dev;

	mif_err("%s: ERR! No CRASH_EXIT ACK from CP\n", ld->name);

	mem_handle_cp_crash(mld, STATE_CRASH_EXIT);
}

/**
@brief		trigger an enforced CP crash

@param mld	the pointer to a mem_link_device instance
*/
void mem_forced_cp_crash(struct mem_link_device *mld)
{
	struct link_device *ld = &mld->link_dev;
	struct modem_ctl *mc = ld->mc;
	unsigned long flags;
	bool duplicated = false;
#ifdef DEBUG_MODEM_IF
	struct utc_time t;
#endif

#ifdef DEBUG_MODEM_IF
	get_utc_time(&t);
#endif

	/* Disable normal IPC */
	set_magic(mld, MEM_CRASH_MAGIC);
	set_access(mld, 0);

	spin_lock_irqsave(&mld->lock, flags);
	if (mld->forced_cp_crash)
		duplicated = true;
	else
		mld->forced_cp_crash = true;
	spin_unlock_irqrestore(&mld->lock, flags);

	if (duplicated) {
#ifdef DEBUG_MODEM_IF
		evt_log(HMSU_FMT " %s: %s: ALREADY in progress <%pf>\n",
			t.hour, t.min, t.sec, t.us, CALLEE, ld->name, CALLER);
#endif
		return;
	}

	if (!cp_online(mc)) {
#ifdef DEBUG_MODEM_IF
		evt_log(HMSU_FMT " %s: %s: %s.state %s != ONLINE <%pf>\n",
			t.hour, t.min, t.sec, t.us,
			CALLEE, ld->name, mc->name, mc_state(mc), CALLER);
#endif
		return;
	}

	if (!wake_lock_active(&mld->dump_wlock))
		wake_lock(&mld->dump_wlock);

	stop_net_ifaces(ld);

	/**
	 * If there is no CRASH_ACK from a CP in FORCE_CRASH_ACK_TIMEOUT,
	 * handle_no_cp_crash_ack() will be executed.
	 */
	mif_add_timer(&mld->crash_ack_timer, FORCE_CRASH_ACK_TIMEOUT,
		      handle_no_cp_crash_ack, (unsigned long)mld);

	/* Send CRASH_EXIT command to a CP */
	send_ipc_irq(mld, cmd2int(CMD_CRASH_EXIT));

#ifdef DEBUG_MODEM_IF
	evt_log(HMSU_FMT " CRASH_EXIT: %s->%s: CP_CRASH_REQ <by %pf>\n",
		t.hour, t.min, t.sec, t.us, ld->name, mc->name, CALLER);

	if (in_interrupt())
		queue_work(system_nrt_wq, &mld->dump_work);
	else
		save_mem_dump(mld);
#endif
}

/**
@}
*/
#endif

/*============================================================================*/

#ifdef GROUP_MEM_LINK_COMMAND
/**
@weakgroup group_mem_link_command
@{
*/

/**
@brief		reset all member variables in every IPC device

@param mld	the pointer to a mem_link_device instance
*/
static inline void reset_ipc_map(struct mem_link_device *mld)
{
	int i;

	for (i = 0; i < MAX_SIPC5_DEV; i++) {
		struct mem_ipc_device *dev = mld->dev[i];

		set_txq_head(dev, 0);
		set_txq_tail(dev, 0);
		set_rxq_head(dev, 0);
		set_rxq_tail(dev, 0);
	}
}

static inline void reset_ipc_devices(struct mem_link_device *mld)
{
	int i;

	for (i = 0; i < MAX_SIPC5_DEV; i++) {
		struct mem_ipc_device *dev = mld->dev[i];

		skb_queue_purge(dev->skb_txq);
		atomic_set(&dev->txq.busy, 0);
		dev->req_ack_cnt[TX] = 0;

		skb_queue_purge(dev->skb_rxq);
		atomic_set(&dev->rxq.busy, 0);
		dev->req_ack_cnt[RX] = 0;
	}
}

int mem_reset_ipc_link(struct mem_link_device *mld)
{
	struct link_device *ld = &mld->link_dev;
	unsigned int magic;
	unsigned int access;

	set_access(mld, 0);
	set_magic(mld, 0);

	reset_ipc_map(mld);
	reset_ipc_devices(mld);

	atomic_set(&ld->netif_stopped, 0);

	set_magic(mld, MEM_IPC_MAGIC);
	set_access(mld, 1);

	magic = get_magic(mld);
	access = get_access(mld);
	if (magic != MEM_IPC_MAGIC || access != 1)
		return -EACCES;

	return 0;
}

/**
@}
*/
#endif

/*============================================================================*/

#ifdef GROUP_MEM_LINK_METHOD
/**
@weakgroup group_mem_link_method
@{
*/

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
		if (likely(sipc5_ipc_ch(ch)))
			return xmit_ipc(mld, ch, skb);
		else
			return xmit_udl(mld, ch, skb);

	case IPC_BOOT:
	case IPC_RAMDUMP:
		if (sipc5_udl_ch(ch))
			return xmit_udl(mld, ch, skb);
		break;

	default:
		break;
	}

	mif_err("%s:%s->%s: ERR! Invalid IO device (format:%s id:%d)\n",
		ld->name, iod->name, mc->name, dev_str(id), ch);

	return -ENODEV;
}

#ifdef CONFIG_LINK_DEVICE_SHMEM
/**
@brief		function for the @b xmit_boot method in a link_device instance

Copies a CP bootloader binary in a user space to the BOOT region for CP

@param ld	the pointer to a link_device instance
@param iod	the pointer to an io_device instance
@param arg	the pointer to a modem_firmware instance

@retval "= 0"	if NO error
@retval "< 0"	an error code
*/
static int mem_xmit_boot(struct link_device *ld, struct io_device *iod,
		     unsigned long arg)
{
	struct mem_link_device *mld = to_mem_link_device(ld);
	struct modem_ctl *mc = iod->mc;
	void __iomem *dst;
	void __user *src;
	int err;
	struct modem_firmware mf;

	atomic_set(&mld->cp_boot_done, 0);

	/**
	 * CP must be stopped before AP writes CP bootloader.
	 */
	if (cp_online(mc) || cp_booting(mc))
		mc->pmu->stop();

	/**
	 * Get the information about the boot image
	 */
	memset(&mf, 0, sizeof(struct modem_firmware));

	err = copy_from_user(&mf, (const void __user *)arg, sizeof(mf));
	if (err) {
		mif_err("%s: ERR! INFO copy_from_user fail\n", ld->name);
		return -EFAULT;
	}

	/**
	 * Check the size of the boot image
	 */
	if (mf.size > mld->boot_size) {
		mif_err("%s: ERR! Invalid BOOT size %d\n", ld->name, mf.size);
		return -EINVAL;
	}
	mif_err("%s: BOOT size = %d bytes\n", ld->name, mf.size);

	/**
	 * Copy the boot image to the BOOT region
	 */
	memset(mld->boot_base, 0, mld->boot_size);

	dst = (void __iomem *)mld->boot_base;
	src = (void __user *)mf.binary;
	err = copy_from_user(dst, src, mf.size);
	if (err) {
		mif_err("%s: ERR! BOOT copy_from_user fail\n", ld->name);
		return err;
	}

	return 0;
}
#endif

/**
@brief		function for the @b dload_start method in a link_device instance

Set all flags and environments for CP binary download

@param ld	the pointer to a link_device instance
@param iod	the pointer to an io_device instance
*/
static int mem_start_download(struct link_device *ld, struct io_device *iod)
{
	struct mem_link_device *mld = to_mem_link_device(ld);
	unsigned int magic;

	atomic_set(&mld->cp_boot_done, 0);

	reset_ipc_map(mld);
	reset_ipc_devices(mld);

	set_magic(mld, SHM_BOOT_MAGIC);
	magic = get_magic(mld);
	if (magic != SHM_BOOT_MAGIC) {
		mif_err("%s: ERR! magic 0x%08X != SHM_BOOT_MAGIC 0x%08X\n",
			ld->name, magic, SHM_BOOT_MAGIC);
		return -EFAULT;
	}

	return 0;
}

/**
@brief		function for the @b firm_update method in a link_device instance

Updates download information for each CP binary image by copying download
information for a CP binary image from a user space to a local buffer in a
mem_link_device instance.

@param ld	the pointer to a link_device instance
@param iod	the pointer to an io_device instance
@param arg	the pointer to a std_dload_info instance
*/
static int mem_update_firm_info(struct link_device *ld, struct io_device *iod,
				unsigned long arg)
{
	struct mem_link_device *mld = to_mem_link_device(ld);
	int ret;

	ret = copy_from_user(&mld->img_info, (void __user *)arg,
			     sizeof(struct std_dload_info));
	if (ret) {
		mif_err("ERR! copy_from_user fail!\n");
		return -EFAULT;
	}

	return 0;
}

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

	if (atomic_read(&mld->cp_boot_done))
		return 0;

	switch (id) {
	case SIPC5_CH_ID_FMT_0 ... SIPC5_CH_ID_FMT_9:
		check_iod = link_get_iod_with_channel(ld, (id + fmt2rfs));
		if (check_iod ? atomic_read(&check_iod->opened) : true) {
			mif_err("%s: %s->%s: Send 0xC2 (INIT_END)\n",
				ld->name, iod->name, mc->name);
			send_ipc_irq(mld, cmd2int(CMD_INIT_END));
			atomic_set(&mld->cp_boot_done, 1);
		} else {
			mif_err("%s is not opened yet\n", check_iod->name);
		}
		break;

	case SIPC5_CH_ID_RFS_0 ... SIPC5_CH_ID_RFS_9:
		check_iod = link_get_iod_with_channel(ld, (id + rfs2fmt));
		if (check_iod) {
			if (atomic_read(&check_iod->opened)) {
				mif_err("%s: %s->%s: Send 0xC2 (INIT_END)\n",
					ld->name, iod->name, mc->name);
				send_ipc_irq(mld, cmd2int(CMD_INIT_END));
				atomic_set(&mld->cp_boot_done, 1);
			} else {
				mif_err("%s not opened yet\n", check_iod->name);
			}
		}
		break;

	default:
		break;
	}

	return 0;
}

/**
@brief		function for the @b force_dump method in a link_device instance

@param ld	the pointer to a link_device instance
@param iod	the pointer to an io_device instance
*/
static int mem_force_dump(struct link_device *ld, struct io_device *iod)
{
	struct mem_link_device *mld = to_mem_link_device(ld);
	mif_err("+++\n");
	mem_forced_cp_crash(mld);
	mif_err("---\n");
	return 0;
}

/**
@brief		function for the @b dump_start method in a link_device instance

@param ld	the pointer to a link_device instance
@param iod	the pointer to an io_device instance
*/
static int mem_start_upload(struct link_device *ld, struct io_device *iod)
{
	struct mem_link_device *mld = to_mem_link_device(ld);

	reset_ipc_map(mld);
	reset_ipc_devices(mld);

	mif_err("%s: magic = 0x%08X\n", ld->name, SHM_DUMP_MAGIC);
	set_magic(mld, SHM_DUMP_MAGIC);

	return 0;
}

/**
@}
*/
#endif

/*============================================================================*/

#ifdef GROUP_MEM_LINK_SETUP
/**
@weakgroup group_mem_link_setup
@{
*/

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
	char name[MAX_NAME_LEN];
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

	mld->type = type;

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

	ld->mdm_data = modem;
	ld->name = modem->link_name;
	ld->aligned = 1;
	ld->ipc_version = modem->ipc_version;

	ld->init_comm = mem_init_comm;

	ld->send = mem_send;

#ifdef CONFIG_LINK_DEVICE_SHMEM
	if (type == MEM_SYS_SHMEM)
		ld->xmit_boot = mem_xmit_boot;
#endif

	ld->dload_start = mem_start_download;
	ld->firm_update = mem_update_firm_info;

	ld->force_dump = mem_force_dump;
	ld->dump_start = mem_start_upload;

	INIT_LIST_HEAD(&ld->list);

	skb_queue_head_init(&ld->sk_fmt_tx_q);
	skb_queue_head_init(&ld->sk_raw_tx_q);

	skb_queue_head_init(&ld->sk_fmt_rx_q);
	skb_queue_head_init(&ld->sk_raw_rx_q);

	init_completion(&ld->init_cmpl);

	for (i = 0; i < MAX_SIPC5_DEV; i++) {
		spin_lock_init(&ld->tx_lock[i]);
		spin_lock_init(&ld->rx_lock[i]);
	}

	spin_lock_init(&ld->netif_lock);
	atomic_set(&ld->netif_stopped, 0);

	snprintf(name, MAX_NAME_LEN, "%s_tx_wq", ld->name);
	ld->tx_wq = create_singlethread_workqueue(name);
	if (!ld->tx_wq) {
		mif_err("%s: ERR! fail to create tx_wq\n", ld->name);
		goto error;
	}

	snprintf(name, MAX_NAME_LEN, "%s_rx_wq", ld->name);
	ld->rx_wq = create_singlethread_workqueue(name);
	if (!ld->rx_wq) {
		mif_err("%s: ERR! fail to create rx_wq\n", ld->name);
		goto error;
	}

	INIT_DELAYED_WORK(&ld->rx_delayed_work, link_to_demux_work);

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

	hrtimer_init(&mld->tx_timer, CLOCK_MONOTONIC, HRTIMER_MODE_REL);
	mld->tx_timer.function = tx_timer_func;

	/*
	** Initialize variables for CP booting and crash dump
	*/
	atomic_set(&mld->cp_boot_done, 0);

	spin_lock_init(&mld->pif_init_lock);

	INIT_DELAYED_WORK(&mld->udl_rx_dwork, udl_rx_work);

	snprintf(name, MAX_NAME_LEN, "%s_dump_wlock", ld->name);
	wake_lock_init(&mld->dump_wlock, WAKE_LOCK_SUSPEND, name);

#ifdef DEBUG_MODEM_IF
	INIT_WORK(&mld->dump_work, mem_dump_work);
#endif

	mif_err("---\n");
	return mld;

error:
	kfree(mld);
	mif_err("xxx\n");
	return NULL;
}

/**
@brief		setup the logical map for an BOOT region

@param mld	the pointer to a mem_link_device instance
@param start	the physical address of an IPC region
@param size	the size of the IPC region
*/
int mem_setup_boot_map(struct mem_link_device *mld, unsigned long start,
		       unsigned long size)
{
	struct link_device *ld = &mld->link_dev;
	char __iomem *base;

	if (!mld->remap_region) {
		mif_err("%s: ERR! NO remap_region method\n", ld->name);
		return -EFAULT;
	}

	base = mld->remap_region(start, size);
	if (!base) {
		mif_err("%s: ERR! remap_region fail\n", ld->name);
		return -EINVAL;
	}

	mif_err("%s: BOOT_RGN phys_addr:0x%08lx virt_addr:0x%08x size:%lu\n",
		ld->name, start, (int)base, size);

	mld->boot_start = start;
	mld->boot_size = size;
	mld->boot_base = (char __iomem *)base;

	return 0;
}

static void remap_4mb_map_to_ipc_dev(struct mem_link_device *mld)
{
	struct link_device *ld = &mld->link_dev;
	struct shmem_4mb_phys_map *map;
	struct mem_ipc_device *dev;

	map = (struct shmem_4mb_phys_map *)mld->base;

	/* magic code and access enable fields */
	mld->magic = (u32 __iomem *)&map->magic;
	mld->access = (u32 __iomem *)&map->access;

	/* IPC_FMT */
	dev = &mld->ipc_dev[IPC_FMT];

	dev->id = IPC_FMT;
	strcpy(dev->name, "FMT");

	spin_lock_init(&dev->txq.lock);
	atomic_set(&dev->txq.busy, 0);
	dev->txq.head = &map->fmt_tx_head;
	dev->txq.tail = &map->fmt_tx_tail;
	dev->txq.buff = &map->fmt_tx_buff[0];
	dev->txq.size = SHM_4M_FMT_TX_BUFF_SZ;

	spin_lock_init(&dev->rxq.lock);
	atomic_set(&dev->rxq.busy, 0);
	dev->rxq.head = &map->fmt_rx_head;
	dev->rxq.tail = &map->fmt_rx_tail;
	dev->rxq.buff = &map->fmt_rx_buff[0];
	dev->rxq.size = SHM_4M_FMT_RX_BUFF_SZ;

	dev->msg_mask = MASK_SEND_FMT;
	dev->req_ack_mask = MASK_REQ_ACK_FMT;
	dev->res_ack_mask = MASK_RES_ACK_FMT;

	dev->tx_lock = &ld->tx_lock[IPC_FMT];
	dev->skb_txq = &ld->sk_fmt_tx_q;

	dev->rx_lock = &ld->rx_lock[IPC_FMT];
	dev->skb_rxq = &ld->sk_fmt_rx_q;

	dev->req_ack_cnt[TX] = 0;
	dev->req_ack_cnt[RX] = 0;

	mld->dev[IPC_FMT] = dev;

	/* IPC_RAW */
	dev = &mld->ipc_dev[IPC_RAW];

	dev->id = IPC_RAW;
	strcpy(dev->name, "RAW");

	spin_lock_init(&dev->txq.lock);
	atomic_set(&dev->txq.busy, 0);
	dev->txq.head = &map->raw_tx_head;
	dev->txq.tail = &map->raw_tx_tail;
	dev->txq.buff = &map->raw_tx_buff[0];
	dev->txq.size = SHM_4M_RAW_TX_BUFF_SZ;

	spin_lock_init(&dev->rxq.lock);
	atomic_set(&dev->rxq.busy, 0);
	dev->rxq.head = &map->raw_rx_head;
	dev->rxq.tail = &map->raw_rx_tail;
	dev->rxq.buff = &map->raw_rx_buff[0];
	dev->rxq.size = SHM_4M_RAW_RX_BUFF_SZ;

	dev->msg_mask = MASK_SEND_RAW;
	dev->req_ack_mask = MASK_REQ_ACK_RAW;
	dev->res_ack_mask = MASK_RES_ACK_RAW;

	dev->tx_lock = &ld->tx_lock[IPC_RAW];
	dev->skb_txq = &ld->sk_raw_tx_q;

	dev->rx_lock = &ld->rx_lock[IPC_RAW];
	dev->skb_rxq = &ld->sk_raw_rx_q;

	dev->req_ack_cnt[TX] = 0;
	dev->req_ack_cnt[RX] = 0;

	mld->dev[IPC_RAW] = dev;
}

/**
@brief		setup the logical map for an IPC region

@param mld	the pointer to a mem_link_device instance
@param start	the physical address of an IPC region
@param size	the size of the IPC region
*/
int mem_setup_ipc_map(struct mem_link_device *mld, unsigned long start,
		      unsigned long size)
{
	struct link_device *ld = &mld->link_dev;
	char __iomem *base;

	if (!mld->remap_region) {
		mif_err("%s: ERR! NO remap_region method\n", ld->name);
		return -EFAULT;
	}

	base = mld->remap_region(start, size);
	if (!base) {
		mif_err("%s: ERR! remap_region fail\n", ld->name);
		return -EINVAL;
	}

	mif_err("%s: IPC_RGN phys_addr:0x%08lx virt_addr:0x%08x size:%lu\n",
		ld->name, start, (int)base, size);
	mld->start = start;
	mld->size = size;
	mld->base = (char __iomem *)base;

	if (mem_type_shmem(mld->type) && mld->size == SZ_4M)
		remap_4mb_map_to_ipc_dev(mld);
	else
		return -EINVAL;

	memset(mld->base, 0, mld->size);

	return 0;
}

/**
@}
*/
#endif

