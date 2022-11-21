/*

 *(C) Copyright 2007 Marvell International Ltd.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * All Rights Reserved
 */

#include "common_datastub.h"

#include <linux/netdevice.h> /* dev_kfree_skb_any */
#include <linux/export.h>
#include "data_channel_kernel.h"
#include "data_path.h"

#define PSD_TX_QUEUE_MAX_LEN    64

#define DATA_ALIGN_SIZE 8
static inline unsigned padding_size(unsigned len)
{
	return (~len + 1) & (DATA_ALIGN_SIZE - 1);
}

struct pduhdr {
	__be16 length;
	__u8 reserved;
	__u8 offset;
	int cid;
}__attribute__((packed));

struct sk_buff_head tx_q;
spinlock_t tx_q_lock;

struct data_path *psd_dp;

void sendPSDData(int cid, struct sk_buff *skb)
{
	struct pduhdr *hdr;
	struct sk_buff *skb2;
	unsigned len;
	unsigned tailpad;
	unsigned long flags;

	len = skb->len;
	tailpad = padding_size(sizeof *hdr + len);

	if (likely(!skb_cloned(skb))) {
		int	headroom = skb_headroom(skb);
		int tailroom = skb_tailroom(skb);

		/* enough room as-is? */
		if (likely(sizeof *hdr + tailpad <= headroom + tailroom)) {
			/* do not need to be readjusted */
			if(sizeof *hdr <= headroom && tailpad <= tailroom)
				goto fill;

			skb->data = memmove(skb->head + sizeof *hdr,
				skb->data, len);
			skb_set_tail_pointer(skb, len);
			goto fill;
		}
	}

	/* create a new skb, with the correct size (and tailpad) */
	skb2 = skb_copy_expand(skb, sizeof *hdr, tailpad + 1, GFP_ATOMIC);
	dev_kfree_skb_any(skb);
	if (unlikely(!skb2))
		return;
	skb = skb2;

	/* fill out the pdu header */
fill:
	hdr = (void *) __skb_push(skb, sizeof *hdr);
	memset(hdr, 0, sizeof *hdr);
	hdr->length = cpu_to_be16(len);
	hdr->cid = cid;
	memset(skb_put(skb, tailpad), 0, tailpad);

	/* link is down */
	if(!data_path_is_link_up()) {
		dev_kfree_skb_any(skb);
		return;
	}

	spin_lock_irqsave(&tx_q_lock, flags);
	/* drop the packet if queue is full */
	if (skb_queue_len(&tx_q) >= PSD_TX_QUEUE_MAX_LEN) {
		struct sk_buff *first = skb_dequeue(&tx_q);
		dev_kfree_skb_any(first);
	}

	skb_queue_tail(&tx_q, skb);
	spin_unlock_irqrestore(&tx_q_lock, flags);

	data_path_schedule_tx(psd_dp);
}
EXPORT_SYMBOL(sendPSDData);

struct sk_buff *psd_get_packet(void)
{
	struct sk_buff *skb;

	DP_ENTER();

	spin_lock_irq(&tx_q_lock);
	skb = skb_dequeue(&tx_q);
	spin_unlock_irq(&tx_q_lock);

	DP_LEAVE();
	return skb;
}

void psd_finish_tx(enum data_path_result result)
{
	DP_ENTER();

	DP_LEAVE();
}

extern DataRxCallbackFunc dataRxCbFunc[];
enum data_path_result psd_data_rx (unsigned char *data,
				     unsigned int length)
{
	unsigned char *p = data;
	unsigned int remains = length;
	int ret = 0;
	DP_ENTER();

	while (remains > 0) {
		struct pduhdr			*hdr = (void *)p;
		u32						iplen, offset_len;
		u32						tailpad;


		iplen = be16_to_cpu(hdr->length);
		offset_len = hdr->offset;
		tailpad = padding_size(sizeof *hdr + iplen + offset_len);

		DP_PRINT("%s: remains, %d, iplen %ld, offset %ld, cid %d, tailpad %d\n",
			__func__, remains, iplen, offset_len, hdr->cid, tailpad);

		if (unlikely(remains < (iplen + offset_len + sizeof *hdr))) {
			DP_ERROR("%s: packet length error\n", __func__);
			return dp_rx_packet_error;
		}

		/* offset domain data */
		p += sizeof *hdr;
		remains -= sizeof *hdr;

		/* ip payload */
		p += offset_len;
		remains -= offset_len;

		/*Since we need to distinguish the received packets for PPP or directly IP, so
		if PPP call back function registered, first forward the packet to PPP call back function check, if this packet is
		for PPP, the call back function will return 1, otherwise return 0*/
		if(dataRxCbFunc[PDP_PPP_MODEM])
			ret = dataRxCbFunc[PDP_PPP_MODEM](p, iplen, hdr->cid);
		if(ret == 0)
		{
			if (dataRxCbFunc[PDP_DIRECTIP])
				dataRxCbFunc[PDP_DIRECTIP](p, iplen, hdr->cid);
			else
				return dp_rx_packet_dropped;
		}

		p += iplen + tailpad;

		remains -= iplen + tailpad;
	}
	return dp_success;
}

void psd_tx_stop(void)
{
	DP_ENTER();
	DP_LEAVE();
	return;
}

void psd_tx_resume(void)
{
	DP_ENTER();

	if (skb_queue_len(&tx_q))
		data_path_schedule_tx(psd_dp);

	DP_LEAVE();
	return;
}

void psd_rx_stop(void)
{
	DP_ENTER();

	data_path_schedule_rx(psd_dp);

	DP_LEAVE();
	return;
}

void psd_link_down(void)
{
	DP_ENTER();

	spin_lock_irq(&tx_q_lock);
	skb_queue_purge(&tx_q);
	spin_unlock_irq(&tx_q_lock);

	DP_LEAVE();
	return;
}

void psd_link_up(void)
{
	DP_ENTER();
	DP_LEAVE();
	return;
}

struct data_path_callback psd_cbs = {
	.get_packet = psd_get_packet,
	.finish_tx = psd_finish_tx,
	.data_rx = psd_data_rx,
	.tx_stop = psd_tx_stop,
	.tx_resume = psd_tx_resume,
	.rx_stop = psd_rx_stop,
	.link_down = psd_link_down,
	.link_up = psd_link_up,
};


int psd_data_channel_init(void)
{
	psd_dp = data_path_open(dp_type_psd, &psd_cbs);
	if (!psd_dp)
		return -1;

	skb_queue_head_init(&tx_q);
	spin_lock_init(&tx_q_lock);

	return 0;
}

void psd_data_channel_exit(void)
{
	skb_queue_purge(&tx_q);
	data_path_close(psd_dp);
}

