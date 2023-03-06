/* /linux/drivers/modem_if_v2/modem_link_device_mipi.c
 *
 * Copyright (C) 2012 Samsung Electronics.
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

#include <linux/init.h>
#include <linux/module.h>
#include <linux/device.h>
#include <linux/sched.h>
#include <linux/irq.h>
#include <linux/poll.h>
#include <linux/gpio.h>
#include <linux/if_arp.h>
#include <linux/wakelock.h>
#include <linux/slab.h>
#include <linux/mutex.h>
#include <linux/interrupt.h>
#include <linux/dma-mapping.h>
#include <linux/platform_device.h>
#include <linux/hsi/hsi.h>

#include <linux/platform_data/modem.h>
#include "modem_prj.h"
#include "modem_link_device_mipi_edlp.h"
#include "modem_utils.h"


/** mipi hsi link configuration
 *
 * change inside this table, do not fix if_hsi_init() directly
 *
 */
static struct hsi_channel_info ch_info[HSI_NUM_OF_USE_CHANNELS] = {
	{ "cmd", WQ_HIGHPRI, HSI_RX_0_CHAN_MAX_PDU, 0 , 0, 0},
	{ "raw0", WQ_HIGHPRI, HSI_RX_IPDATA_CHAN_PDU,
				HSI_TX_NETCTR_CHAN_PDU, 1, 1 },
	{ "raw1", WQ_HIGHPRI, HSI_RX_IPDATA_CHAN_PDU,
				HSI_TX_NETCTR_CHAN_PDU, 1, 1 },
	{ "raw2", WQ_HIGHPRI, HSI_RX_IPDATA_CHAN_PDU,
				HSI_TX_NETCTR_CHAN_PDU, 1, 1 },
	{ "raw3", WQ_HIGHPRI, HSI_RX_IPDATA_CHAN_PDU,
				HSI_TX_NETCTR_CHAN_PDU, 1, 1 },
	{ "m_fmt", 0, HSI_RX_FORMAT_CHAN_PDU,
				HSI_TX_NETCTR_CHAN_PDU, 0, 0 },
	{ "dbg", 0, HSI_RX_CPDEBUG_CHAN_PDU,
				HSI_TX_NETCTR_CHAN_PDU, 0, 0 },
	{ "extra", 0, HSI_RX_EXTRA_CHAN_PDU,
				HSI_TX_NETCTR_CHAN_PDU, 0, 0 },
};

static int mipi_hsi_init_communication(struct link_device *ld,
			struct io_device *iod)
{
	struct mipi_link_device *mipi_ld = to_mipi_link_device(ld);

	switch (iod->format) {
	case IPC_FMT:
		if (iod->id == SIPC5_CH_ID_FMT_0)
			return if_hsi_init_handshake(mipi_ld,
						HSI_INIT_MODE_NORMAL);

	case IPC_BOOT:
	case IPC_RAMDUMP:
	case IPC_RFS:
	case IPC_RAW:
	default:
		return 0;
	}
}

static void mipi_hsi_terminate_communication(
			struct link_device *ld, struct io_device *iod)
{
	struct mipi_link_device *mipi_ld = to_mipi_link_device(ld);

	switch (iod->format) {
	case IPC_FMT:
	case IPC_RFS:
	case IPC_RAW:
	case IPC_RAMDUMP:
		if (wake_lock_active(&mipi_ld->wlock)) {
			wake_unlock(&mipi_ld->wlock);
			mipi_debug("wake_unlock\n");
		}
		break;

	case IPC_BOOT:
	default:
		break;
	}
}

static int mipi_hsi_ioctl(struct link_device *ld, struct io_device *iod,
		unsigned int cmd, unsigned long arg)
{
	struct mipi_link_device *mipi_ld = to_mipi_link_device(ld);
	int err = 0;
	int speed;

	switch (cmd) {
	case IOCTL_BOOT_LINK_SPEED_CHG:
		if (copy_from_user(&speed, (void __user *)arg, sizeof(int)))
			return -EFAULT;

		if (speed == 0)
			return if_hsi_init_handshake(mipi_ld,
					HSI_INIT_MODE_FLASHLESS_BOOT);
		else {
			if (iod->format == IPC_RAMDUMP)
				return if_hsi_init_handshake(mipi_ld,
					HSI_INIT_MODE_CP_RAMDUMP);
			else
				return if_hsi_init_handshake(mipi_ld,
					HSI_INIT_MODE_FLASHLESS_BOOT_EBL);
		}

	default:
		mif_err("%s: ERR! invalid cmd 0x%08X\n", ld->name, cmd);
		err = -EINVAL;
		break;
	}

	return err;
}

static int mipi_hsi_send(struct link_device *ld, struct io_device *iod,
			struct sk_buff *skb)
{
	int ret;
	struct mipi_link_device *mipi_ld = to_mipi_link_device(ld);
	size_t tx_size = skb->len;
	unsigned int ch_id = -1;

	/* save io device into cb area */
	*((struct io_device **)skb->cb) = iod;

	switch (iod->format) {
	/* handle asynchronous write */
	case IPC_RAW:
		if (iod->id == SIPC_CH_ID_PDP_0)
			ch_id = HSI_RAW_NET_A_CHANNEL;
		else if (iod->id == SIPC_CH_ID_PDP_1)
			ch_id = HSI_RAW_NET_B_CHANNEL;
		else if (iod->id == SIPC_CH_ID_PDP_2)
			ch_id = HSI_RAW_NET_C_CHANNEL;
		else if (iod->id == SIPC_CH_ID_PDP_3)
			ch_id = HSI_RAW_NET_D_CHANNEL;
		else if (iod->id == SIPC_CH_ID_CPLOG1)
			ch_id = HSI_MODEM_DEBUG_CHANNEL;
		else
			ch_id = HSI_MULTI_FMT_CHANNEL;
		break;
	case IPC_FMT:
	case IPC_MULTI_RAW:
	case IPC_RFS:
		ch_id = HSI_MULTI_FMT_CHANNEL;
		break;

	/* handle synchronous write */
	case IPC_BOOT:
		ret = if_hsi_write(mipi_ld, HSI_FLASHLESS_CHANNEL,
				(u32 *)skb->data, skb->len, HSI_TX_MODE_SYNC);
		if (ret < 0) {
			mipi_err("write fail : %d\n", ret);
			dev_kfree_skb_any(skb);
			return ret;
		} else
			mipi_debug("write Done\n");
		dev_kfree_skb_any(skb);
		return tx_size;

	case IPC_RAMDUMP:
		ret = if_hsi_write(mipi_ld, HSI_CP_RAMDUMP_CHANNEL,
				(u32 *)skb->data, skb->len, HSI_TX_MODE_SYNC);
		if (ret < 0) {
			mipi_err("write fail : %d\n", ret);
			dev_kfree_skb_any(skb);
			return ret;
		} else
			mipi_debug("write Done\n");
		dev_kfree_skb_any(skb);
		return ret;

	default:
		mipi_err("does not support %d type\n", iod->format);
		dev_kfree_skb_any(skb);
		return -ENOTSUPP;
	}

	/* set wake_lock to prevent to sleep before tx_work thread run */
	if (!wake_lock_active(&mipi_ld->wlock)) {
		wake_lock_timeout(&mipi_ld->wlock, HZ / 2);
		mipi_debug("wake_lock\n");
	}

	/* en queue skb data */
	skb_queue_tail(&mipi_ld->hsi_channels[ch_id].tx_q, skb);

	mipi_debug("ch %d: len:%d, %p:%p:%p\n", ch_id, skb->len, skb->prev,
				skb, skb->next);
	queue_work(mipi_ld->hsi_channels[ch_id].tx_wq,
				&mipi_ld->hsi_channels[ch_id].tx_w);

	return tx_size;
}

static inline int mipi_calc_n_packets(struct sk_buff_head *tx_q,
						int max_sz, int force_pad)
{
	struct sk_buff *skb;
	unsigned long flags;
	int n_packets = 0;
	int hd_sz, pk_sz;

	/* 1. peek first skb */
	spin_lock_irqsave(&tx_q->lock, flags);
	skb = skb_peek(tx_q);
	spin_unlock_irqrestore(&tx_q->lock, flags);
	if (!skb)
		return n_packets;

	n_packets++;
	hd_sz = get_edlp_header_size(n_packets);
	pk_sz = get_edlp_align_size(skb->len + force_pad);
	if (hd_sz + pk_sz > max_sz) {
		n_packets--;
		return n_packets;
	}

	/* 2. more than one packet */
	while (n_packets < skb_queue_len(tx_q)) {
		spin_lock_irqsave(&tx_q->lock, flags);
		skb = skb_peek_next(skb, tx_q);
		spin_unlock_irqrestore(&tx_q->lock, flags);
		if (!skb || !skb->len)
			break;
		n_packets++;
		hd_sz = get_edlp_header_size(n_packets);
		pk_sz += get_edlp_align_size(skb->len + force_pad);
		if (hd_sz + pk_sz > max_sz) {
			n_packets--;
		break;
		}
	}
	return n_packets;
}

static void mipi_hsi_send_work(struct work_struct *work)
{
	struct if_hsi_channel *channel =
				container_of(work, struct if_hsi_channel, tx_w);
	struct mipi_link_device *mipi_ld = to_mipi_link_device(channel->ld);
	struct sk_buff *skb;
	struct sk_buff_head *tx_q = &channel->tx_q;
	struct hsi_msg *tx_msg;
	unsigned long int flags = in_interrupt() ? GFP_ATOMIC : GFP_KERNEL;
	int ret;
	u32 *hd_ptr32;
	int n_packets, hd_sz, pk_sz, pk_offset;
	int force_pad = channel->is_dma_ch ? HSI_NET_PAD_FOR_CP : 0;
	struct io_device *iod = NULL;

check_online:
	/* check it every tx transcation */
	if ((channel->ld->com_state != COM_ONLINE) ||
		(mipi_ld->ld.mc->phone_state != STATE_ONLINE)) {
		mipi_debug("CP not ready\n");
		return;
	}

	if (skb_queue_len(tx_q)) {
		mipi_debug("ch%d: tx qlen : %d\n", channel->channel_id,
						skb_queue_len(tx_q));

		/* check channel open */
		if (!atomic_read(&channel->opened)) {
			ret = if_hsi_open_channel(mipi_ld, channel->channel_id);
			if (ret < 0) {
				mipi_err("if_hsi_open_channel fail: %d\n", ret);
				return;
			}

			/* when ch uses credits, wait for credits */
			if (channel->use_credits) {
				wait_event_timeout(channel->credit_waitq,
					atomic_read(&channel->credits), HZ);
				mipi_info("ch:%d, got first credits: %d\n",
					channel->channel_id,
					atomic_read(&channel->credits));
			}
		}

		/* check credits */
		if (channel->use_credits) {
			spin_lock_irqsave(&mipi_ld->credit_lock, flags);
			if (atomic_read(&channel->credits) <= 0) {
				mipi_info("ch %d:no credits, %d pks are in q\n",
				channel->channel_id, skb_queue_len(tx_q));

				iod = if_hsi_get_iod_with_hsi_channel(mipi_ld,
							channel->channel_id);
				if (iod)
					iodev_netif_stop(iod, NULL);

				spin_unlock_irqrestore(&mipi_ld->credit_lock,
							flags);
				return;
			}
			spin_unlock_irqrestore(&mipi_ld->credit_lock, flags);
			flags = in_interrupt() ? GFP_ATOMIC : GFP_KERNEL;
		}

		/* Allocate a new TX msg */
		tx_msg = hsi_alloc_msg(1, flags);
		if (!tx_msg) {
			mipi_err("Out of memory (tx_msg)\n");
			return;
		}
		mipi_debug("hsi_alloc_msg done\n");

		tx_msg->cl = mipi_ld->client;
		tx_msg->channel = channel->channel_id;
		tx_msg->ttype = HSI_MSG_WRITE;
		tx_msg->context = NULL;
		tx_msg->complete = if_hsi_write_done;
		tx_msg->destructor = if_hsi_msg_destruct;

		/* Allocate data buffer */
		channel->tx_data = if_hsi_buffer_alloc(mipi_ld, tx_msg,
					channel->tx_pdu_size);
		if (!channel->tx_data) {
			mipi_err("Out of memory (hsi_msg buffer => size: %d)\n",
					channel->tx_pdu_size);
			hsi_free_msg(tx_msg);
			return;
		}
		sg_set_buf(tx_msg->sgt.sgl, channel->tx_data,
					channel->tx_pdu_size);
		mipi_debug("allocate tx buffer done\n");

		n_packets = 0;
		hd_ptr32 = (u32 *)channel->tx_data;

		/* caclulate number of packets for edlp framing */
		n_packets = mipi_calc_n_packets(
				tx_q, channel->tx_pdu_size, force_pad);

		mipi_debug("%d packets in PDU in ch %d\n", n_packets,
							channel->channel_id);
		/* process pdu */
		hd_sz = get_edlp_header_size(n_packets);
		pk_offset = hd_sz;
		*hd_ptr32 = HSI_EDLP_PDU_SIGNATURE;

		/* pop skb from skb queue */
		while (n_packets--) {
			skb = skb_dequeue(tx_q);
			if (!skb) {
				/* case: skb queue purge has done during
				 * PDU generation. Drop current data(pdu, skb..)
				 * and check again if tx q has skbs or not */
				/* Free the TX buff & msg */
				if_hsi_buffer_free(mipi_ld, tx_msg);
				hsi_free_msg(tx_msg);
				goto check_online;
			}

			/* set header info */
			hd_ptr32++;
			*hd_ptr32 = pk_offset;
			hd_ptr32++;
			pk_sz = get_edlp_align_size(skb->len + force_pad);
			/* add N field in packet size */
			if (!n_packets) /* last packet in this PDU */
				*hd_ptr32 = set_as_last(skb->len + force_pad);
			else
				*hd_ptr32 = set_as_middle(skb->len + force_pad);

			/* copy data to pdu */
			memcpy((void *)(channel->tx_data + pk_offset +
					force_pad), skb->data, skb->len);

			if (channel->channel_id == HSI_MULTI_FMT_CHANNEL)
				print_hex_dump(KERN_DEBUG, "FMT-TX: ",
						DUMP_PREFIX_NONE,
						1, 1,
						(void *)skb->data,
						skb->len <= 16 ?
						(size_t)skb->len :
						(size_t)16, false);
			/* packet offset */
			pk_offset += pk_sz;
			/* free skb */
			dev_kfree_skb_any(skb);
			skb = NULL;
		}
		/* print_hex_dump(KERN_INFO, "EDLP-TX-PDU: ",
				DUMP_PREFIX_OFFSET, 16, 4, (void *)buff,
				pk_offset > 128 ? 128 : pk_offset, true); */

		channel->edlp_tx_seq_num++;

		mod_timer(&mipi_ld->hsi_acwake_down_timer,
				jiffies + HSI_ACWAKE_DOWN_TIMEOUT);
		if_hsi_set_wakeline(mipi_ld, 1);

		/* Send the TX HSI msg */
		ret = hsi_async(mipi_ld->client, tx_msg);
		if (ret) {
			mipi_err("ch=%d, hsi_async(write) fail=%d\n",
						channel->channel_id, ret);
			return;
		}

		if (down_timeout(&channel->write_done_sem,
						HSI_WRITE_DONE_TIMEOUT) < 0) {
			mipi_err("ch=%d, hsi_write_done timeout\n",
						channel->channel_id);

			/* cp force crash to get cp ramdump */
			if (mipi_ld->ld.mc->gpio_ap_dump_int)
				mipi_ld->ld.mc->ops.modem_force_crash_exit(
							mipi_ld->ld.mc);
			return;
		}

		/* when it comes to down timeout at tx. but still there's
		 * possibility of that CP receive PDU even AP get timed out.
		 * Am I right? But now we are only decrease credits only if
		 * tx has done successfully.
		 */
		if (channel->use_credits &&
				atomic_dec_if_positive(&channel->credits) < 0) {
			mipi_info("ch %d:no credits, %d packets are in q\n",
				channel->channel_id, skb_queue_len(tx_q));
			return;
		}
		goto check_online;
	}
}

static int __devinit if_hsi_probe(struct device *dev);
static void if_hsi_shutdown(struct device *dev);
static struct hsi_client_driver if_hsi_driver = {
	.driver = {
		   .name = "svnet_hsi_link_lte",
		   .owner = THIS_MODULE,
		   .probe = if_hsi_probe,
		   .shutdown = if_hsi_shutdown,
		   },
};

static int if_hsi_set_wakeline(struct mipi_link_device *mipi_ld,
			unsigned int state)
{
	unsigned long int flags;
	u32 nop_command;

	spin_lock_irqsave(&mipi_ld->acwake_lock, flags);
	if (atomic_read(&mipi_ld->acwake) == state) {
		spin_unlock_irqrestore(&mipi_ld->acwake_lock, flags);
		return 0;
	}
	atomic_set(&mipi_ld->acwake, state);
	spin_unlock_irqrestore(&mipi_ld->acwake_lock, flags);

	if (state)
		hsi_start_tx(mipi_ld->client);
	else {
		if ((mipi_ld->ld.com_state == COM_ONLINE) &&
			(mipi_ld->ld.mc->phone_state == STATE_ONLINE)) {
			/* Send the NOP command to avoid tailing bits issue */
			nop_command = if_hsi_create_cmd(HSI_EDLP_MSG_NOP,
						HSI_CONTROL_CHANNEL, 0);
			if_hsi_write(mipi_ld, HSI_CONTROL_CHANNEL, &nop_command,
						4, HSI_TX_MODE_ASYNC);
			mipi_debug("SEND CMD : %08x\n", nop_command);
		}

		hsi_stop_tx(mipi_ld->client);
	}

	mipi_info("ACWAKE(%d)\n", state);
	return 0;
}

static void if_hsi_acwake_down_func(unsigned long data)
{
	struct mipi_link_device *mipi_ld = (struct mipi_link_device *)data;

	if_hsi_set_wakeline(mipi_ld, 0);
}

static int if_hsi_open_channel(struct mipi_link_device *mipi_ld,
			unsigned int ch_num)
{
	struct if_hsi_channel *channel = &mipi_ld->hsi_channels[ch_num];

	if (atomic_read(&channel->opened))
		return -1;

	if_hsi_send_command(mipi_ld, HSI_EDLP_MSG_OPEN_CONN,
				channel->channel_id, channel->tx_pdu_size);
	if (down_timeout(&channel->open_conn_done_sem,
				HSI_ACK_DONE_TIMEOUT) < 0) {
		mipi_info("timeout for start open command\n");
		return -1;
	}
	mipi_debug("Got ACK for open command: ch=%d\n", channel->channel_id);

	atomic_inc(&channel->opened);
	mipi_info("hsi_open Done : %d\n", channel->channel_id);
	return 0;
}

static int if_hsi_close_channel(struct mipi_link_device *mipi_ld,
			unsigned int ch_num)
{
	struct if_hsi_channel *channel = &mipi_ld->hsi_channels[ch_num];

	if (!atomic_read(&channel->opened))
		return -1;

	if_hsi_send_command(mipi_ld, HSI_EDLP_MSG_CLOSE_CONN,
				channel->channel_id, 0x2);
	if (down_timeout(&channel->close_conn_done_sem,
				HSI_ACK_DONE_TIMEOUT) < 0) {
		mipi_info("timeout for start close command\n");
		return -1;
	}
	mipi_debug("Got ACK for close command: ch=%d\n", channel->channel_id);

	/* reset credits to stop tx */
	if (channel->use_credits) {
		atomic_set(&channel->credits, 0);
		/* Do I have to clear TxQ also ? */
		/* skb_queue_purge(&channel->tx_q); */
	}

	atomic_set(&channel->opened, 0);
	mipi_info("hsi_close Done : %d\n", channel->channel_id);
	return 0;
}

static void if_hsi_start_work(struct work_struct *work)
{
	int ret;
	u32 start_cmd = 0xC2;
	struct mipi_link_device *mipi_ld =
			container_of(work, struct mipi_link_device,
						start_work.work);

	mipi_ld->ld.com_state = COM_HANDSHAKE;
	ret = if_hsi_edlp_send(mipi_ld, HSI_MULTI_FMT_CHANNEL, &start_cmd, 1);
	if (ret < 0) {
		mipi_err("Send HandShake 0xC2 fail: %d\n", ret);
		schedule_delayed_work(&mipi_ld->start_work, 3 * HZ);
		return;
	}

	mipi_ld->ld.com_state = COM_ONLINE;
	mipi_info("Send HandShake 0xC2 Done\n");
}

static int if_hsi_init_handshake(struct mipi_link_device *mipi_ld, int mode)
{
	int ret;
	int i;

	if (!mipi_ld->client) {
		mipi_err("no hsi client data\n");
		return -ENODEV;
	}

	switch (mode) {
	case HSI_INIT_MODE_NORMAL:
		if (timer_pending(&mipi_ld->hsi_acwake_down_timer))
			del_timer(&mipi_ld->hsi_acwake_down_timer);

		if (mipi_ld->ld.mc->phone_state != STATE_ONLINE) {
			mipi_err("CP not ready\n");
			return -ENODEV;
		}

		for (i = 0; i < HSI_NUM_OF_USE_CHANNELS; i++)
			atomic_set(&mipi_ld->hsi_channels[i].opened, 0);

		if (!hsi_port_claimed(mipi_ld->client)) {
			mipi_err("hsi_port is not ready\n");
			return -EACCES;
		}

		ret = hsi_flush(mipi_ld->client);
		if (unlikely(ret)) {
			mipi_err("hsi_flush failed (%d)\n", ret);
			hsi_release_port(mipi_ld->client);
			return ret;
		}

		mipi_ld->client->tx_cfg.mode = HSI_MODE_FRAME;
		mipi_ld->client->tx_cfg.channels = 8;
		mipi_ld->client->tx_cfg.speed = 200000; /* Speed: 200MHz */
		mipi_ld->client->tx_cfg.arb_mode = HSI_ARB_RR;
		mipi_ld->client->rx_cfg.mode = HSI_MODE_FRAME;
		mipi_ld->client->rx_cfg.channels = 8;
		mipi_ld->client->rx_cfg.flow = HSI_FLOW_PIPE;

		/* Setup the HSI controller */
		ret = hsi_setup(mipi_ld->client);
		if (unlikely(ret)) {
			mipi_err("hsi_setup failed (%d)\n", ret);
			hsi_release_port(mipi_ld->client);
			return ret;
		}

		if (mipi_ld->ld.com_state != COM_ONLINE)
			mipi_ld->ld.com_state = COM_HANDSHAKE;

		ret = if_hsi_read_async(mipi_ld, HSI_CONTROL_CHANNEL, 4);
		if (ret)
			mipi_err("if_hsi_read_async fail : %d\n", ret);

		if (mipi_ld->ld.com_state != COM_ONLINE)
			schedule_delayed_work(&mipi_ld->start_work, 3 * HZ);

		mipi_debug("hsi_init_handshake Done : MODE_NORMAL\n");
		return 0;

	case HSI_INIT_MODE_FLASHLESS_BOOT:
		mipi_ld->ld.com_state = COM_BOOT;

		if (timer_pending(&mipi_ld->hsi_acwake_down_timer))
			del_timer(&mipi_ld->hsi_acwake_down_timer);

		if (hsi_port_claimed(mipi_ld->client)) {
			if_hsi_set_wakeline(mipi_ld, 0);
			mipi_debug("Releasing the HSI controller\n");
			hsi_release_port(mipi_ld->client);
		}

		/* Claim the HSI port */
		ret = hsi_claim_port(mipi_ld->client, 1);
		if (unlikely(ret)) {
			mipi_err("hsi_claim_port failed (%d)\n", ret);
			return ret;
		}

		mipi_ld->client->tx_cfg.mode = HSI_MODE_FRAME;
		mipi_ld->client->tx_cfg.channels = 1;
		mipi_ld->client->tx_cfg.speed = 25000; /* Speed: 25MHz */
		mipi_ld->client->tx_cfg.arb_mode = HSI_ARB_RR;
		mipi_ld->client->rx_cfg.mode = HSI_MODE_FRAME;
		mipi_ld->client->rx_cfg.channels = 1;
		mipi_ld->client->rx_cfg.flow = HSI_FLOW_SYNC;

		/* Setup the HSI controller */
		ret = hsi_setup(mipi_ld->client);
		if (unlikely(ret)) {
			mipi_err("hsi_setup failed (%d)\n", ret);
			hsi_release_port(mipi_ld->client);
			return ret;
		}

		/* Restore the events callback*/
		hsi_register_port_event(mipi_ld->client, if_hsi_port_event);

		if_hsi_set_wakeline(mipi_ld, 1);

		ret = if_hsi_read_async(mipi_ld, HSI_FLASHLESS_CHANNEL, 4);
		if (ret)
			mipi_err("if_hsi_read_async fail : %d\n", ret);

		mipi_debug("hsi_init_handshake Done : FLASHLESS_BOOT\n");
		return 0;

	case HSI_INIT_MODE_FLASHLESS_BOOT_EBL:
		mipi_ld->ld.com_state = COM_BOOT_EBL;

		if (!hsi_port_claimed(mipi_ld->client)) {
			mipi_err("hsi_port is not ready\n");
			return -EACCES;
		}

		ret = hsi_flush(mipi_ld->client);
		if (unlikely(ret)) {
			mipi_err("hsi_flush failed (%d)\n", ret);
			hsi_release_port(mipi_ld->client);
			return ret;
		}

		mipi_ld->client->tx_cfg.mode = HSI_MODE_FRAME;
		mipi_ld->client->tx_cfg.channels = 1;
		mipi_ld->client->tx_cfg.speed = 100000; /* Speed: 100MHz */
		mipi_ld->client->tx_cfg.arb_mode = HSI_ARB_RR;
		mipi_ld->client->rx_cfg.mode = HSI_MODE_FRAME;
		mipi_ld->client->rx_cfg.channels = 1;
		mipi_ld->client->rx_cfg.flow = HSI_FLOW_SYNC;

		/* Setup the HSI controller */
		ret = hsi_setup(mipi_ld->client);
		if (unlikely(ret)) {
			mipi_err("hsi_setup failed (%d)\n", ret);
			hsi_release_port(mipi_ld->client);
			return ret;
		}

		ret = if_hsi_read_async(mipi_ld, HSI_FLASHLESS_CHANNEL, 4);
		if (ret)
			mipi_err("if_hsi_read_async fail : %d\n", ret);

		mipi_debug("hsi_init_handshake Done : FLASHLESS_BOOT_EBL\n");
		return 0;

	case HSI_INIT_MODE_CP_RAMDUMP:
		mipi_ld->ld.com_state = COM_CRASH;

		if (!hsi_port_claimed(mipi_ld->client)) {
			mipi_err("hsi_port is not ready\n");
			return -EACCES;
		}

		ret = hsi_flush(mipi_ld->client);
		if (unlikely(ret)) {
			mipi_err("hsi_flush failed (%d)\n", ret);
			hsi_release_port(mipi_ld->client);
			return ret;
		}

		mipi_ld->client->tx_cfg.mode = HSI_MODE_FRAME;
		mipi_ld->client->tx_cfg.channels = 1;
		mipi_ld->client->tx_cfg.speed = 100000; /* Speed: 100MHz */
		mipi_ld->client->tx_cfg.arb_mode = HSI_ARB_RR;
		mipi_ld->client->rx_cfg.mode = HSI_MODE_FRAME;
		mipi_ld->client->rx_cfg.channels = 1;
		mipi_ld->client->rx_cfg.flow = HSI_FLOW_SYNC;

		/* Setup the HSI controller */
		ret = hsi_setup(mipi_ld->client);
		if (unlikely(ret)) {
			mipi_err("hsi_setup failed (%d)\n", ret);
			hsi_release_port(mipi_ld->client);
			return ret;
		}

		ret = if_hsi_read_async(mipi_ld, HSI_CP_RAMDUMP_CHANNEL,
					DUMP_ERR_INFO_SIZE * 4);
		if (ret)
			mipi_err("if_hsi_read_async fail : %d\n", ret);

		mipi_debug("hsi_init_handshake Done : RAMDUMP\n");
		return 0;

	default:
		return -EINVAL;
	}
}

static struct io_device *if_hsi_get_iod_with_hsi_channel(
			struct mipi_link_device *mipi_ld, unsigned ch_num)
{
	struct if_hsi_channel *channel = &mipi_ld->hsi_channels[ch_num];
	struct io_device *iod = NULL;
	int iod_id = 0;
	enum dev_format format_type = 0;

	switch (channel->channel_id) {
	case HSI_RAW_NET_A_CHANNEL:
		iod_id = SIPC_CH_ID_PDP_0;
		format_type = IPC_RAW;
		break;
	case HSI_RAW_NET_B_CHANNEL:
		iod_id = SIPC_CH_ID_PDP_1;
		format_type = IPC_RAW;
		break;
	case HSI_RAW_NET_C_CHANNEL:
		iod_id = SIPC_CH_ID_PDP_2;
		format_type = IPC_RAW;
		break;
	case HSI_RAW_NET_D_CHANNEL:
		iod_id = SIPC_CH_ID_PDP_3;
		format_type = IPC_RAW;
		break;
	case HSI_MODEM_DEBUG_CHANNEL:
		iod_id = SIPC_CH_ID_CPLOG1;
		format_type = IPC_RAW;
		break;
	case HSI_MULTI_FMT_CHANNEL:
	case HSI_EXTRA_CHANNEL:
		format_type = IPC_FMT;
		break;
	default:
		mipi_err("received nouse channel: %d\n",
					channel->channel_id);
		return NULL;
	}

	if (format_type == IPC_RAW)
		iod = link_get_iod_with_channel(&mipi_ld->ld, iod_id);
	else
		iod = link_get_iod_with_format(&mipi_ld->ld, format_type);
	return iod;
}

static void if_hsi_conn_err_recovery(struct mipi_link_device *mipi_ld)
{
	int ret;
	unsigned long int flags;

	if (timer_pending(&mipi_ld->hsi_acwake_down_timer))
		del_timer(&mipi_ld->hsi_acwake_down_timer);

	ret = if_hsi_read_async(mipi_ld, HSI_CONTROL_CHANNEL, 4);
	if (ret)
		mipi_err("if_hsi_read_async fail : %d\n", ret);

	mipi_info("if_hsi_conn_err_recovery Done\n");
}

static u32 if_hsi_create_cmd(u32 cmd_type, int ch, u32 param)
{
	u32 cmd = 0;

	switch (cmd_type) {
	case HSI_EDLP_MSG_BREAK:
		return 0;

	case HSI_EDLP_MSG_ECHO:
		cmd = HSI_EDLP_MSG_ECHO_CHECK_PATTERN;
		cmd |= ((HSI_EDLP_MSG_ECHO & 0x0000000F) << 28);
		return cmd;

	case HSI_EDLP_MSG_NOP:
		cmd = HSI_EDLP_MSG_NOP_CHECK_PATTERN;
		cmd |= ((HSI_EDLP_MSG_NOP & 0x0000000F) << 28);
		return cmd;

	case HSI_EDLP_MSG_CONF_CH:
		cmd = param; /* config data */
		cmd |= ((HSI_EDLP_MSG_CONF_CH & 0x0000000F) << 28)
					|((ch & 0x000000FF) << 24);
		return cmd;

	case HSI_EDLP_MSG_OPEN_CONN:
		cmd = param / 4; /* PDU size in 32bit word */
		cmd |= ((HSI_EDLP_MSG_OPEN_CONN & 0x0000000F) << 28)
					|((ch & 0x000000FF) << 24);
		return cmd;

	case HSI_EDLP_MSG_CLOSE_CONN:
		cmd = (param & 0x000000FF) << 16; /* Direction */
		cmd |= ((HSI_EDLP_MSG_CLOSE_CONN & 0x0000000F) << 28)
					|((ch & 0x000000FF) << 24);
		return cmd;

	case HSI_EDLP_MSG_ACK:
		cmd = param; /* Cmd ID & Echoed parameter */
		cmd |= ((HSI_EDLP_MSG_ACK & 0x0000000F) << 28)
					|((ch & 0x000000FF) << 24);
		return cmd;

	case HSI_EDLP_MSG_NAK:
		cmd = param; /* Cmd ID & Echoed parameter */
		cmd |= ((HSI_EDLP_MSG_NAK & 0x0000000F) << 28)
					|((ch & 0x000000FF) << 24);
		return cmd;

	case HSI_EDLP_MSG_CREDITS:
		cmd = (param & 0x000000FF) << 16; /* Credits */
		cmd |= ((HSI_EDLP_MSG_CREDITS & 0x0000000F) << 28)
					|((ch & 0x000000FF) << 24);
		return cmd;

	default:
		mipi_err("ERROR... CMD Not supported : %08x\n",
					cmd_type);
		return 0;
	}
}

static int if_hsi_send_command(struct mipi_link_device *mipi_ld,
			u32 cmd_type, int ch, u32 param)
{
	unsigned long int flags;
	struct if_hsi_channel *channel =
				&mipi_ld->hsi_channels[HSI_CONTROL_CHANNEL];
	u32 command;
	int ret;

	command = if_hsi_create_cmd(cmd_type, ch, param);
	mipi_debug("made command : %08x\n", command);

	mod_timer(&mipi_ld->hsi_acwake_down_timer,
			jiffies + HSI_ACWAKE_DOWN_TIMEOUT);
	if_hsi_set_wakeline(mipi_ld, 1);

	ret = if_hsi_write(mipi_ld, channel->channel_id, &command, 4,
				HSI_TX_MODE_ASYNC);
	if (ret < 0) {
		mipi_err("write command fail : %d\n", ret);
		return ret;
	}

	return 0;
}

static int if_hsi_decode_cmd(struct mipi_link_device *mipi_ld,
			u32 *cmd_data, u32 *cmd, u32 *ch, u32 *param)
{
	u32 data = *cmd_data;

	*cmd = ((data & 0xF0000000) >> 28);
	switch (*cmd) {
	case HSI_EDLP_MSG_BREAK:
		*ch = 0;
		*param = 0;
		break;

	case HSI_EDLP_MSG_ECHO:
		*ch = 0;
		if ((data & 0x00FFFFFF) != HSI_EDLP_MSG_ECHO_CHECK_PATTERN) {
			mipi_err("ECHO pattern not match:%x\n", data);
			return -1;
		}
		break;

	case HSI_EDLP_MSG_NOP:
		*ch = 0;
		if ((data & 0x00FFFFFF) != HSI_EDLP_MSG_NOP_CHECK_PATTERN) {
			mipi_err("NOP pattern not match:%x\n", data);
			return -1;
		}
		break;

	case HSI_EDLP_MSG_CONF_CH:
		*ch = ((data & 0x0F000000) >> 24);
		*param = ((data & 0x00FFFF00) >> 8);
		break;

	case HSI_EDLP_MSG_OPEN_CONN:
		*ch = ((data & 0x0F000000) >> 24);
		*param = (data & 0x000FFFFF) * 4; /* PDU size in 32bit word */
		break;

	case HSI_EDLP_MSG_CLOSE_CONN:
	case HSI_EDLP_MSG_CREDITS:
		*ch = ((data & 0x0F000000) >> 24);
		*param = ((data & 0x00FF0000) >> 16);
		break;

	case HSI_EDLP_MSG_ACK:
	case HSI_EDLP_MSG_NAK:
		*ch = ((data & 0x0F000000) >> 24);
		*param = data & 0x00FFFFFF;
		break;
	default:
		mipi_err("Invalid command received : %08x\n", *cmd);
					*cmd = HSI_EDLP_MSG_INVALID;
					*ch  = HSI_EDLP_INVALID_CHANNEL;
		return -1;
	}
	return 0;
}

static int if_hsi_rx_cmd_handle(struct mipi_link_device *mipi_ld, u32 cmd,
			u32 ch, u32 param)
{
	int ret;
	struct if_hsi_channel *channel = &mipi_ld->hsi_channels[ch];
	struct io_device *iod = NULL;
	u32 credit_cnt = 0;

	mipi_debug("if_hsi_rx_cmd_handle cmd=0x%x, ch=%d, param=%d\n",
				cmd, ch, param);

	switch (cmd) {
	case HSI_EDLP_MSG_BREAK:
		mipi_ld->ld.com_state = COM_HANDSHAKE;
		mipi_err("Command MSG_BREAK Received\n");

		/* if_hsi_conn_err_recovery(mipi_ld); */

		if_hsi_send_command(mipi_ld, HSI_EDLP_MSG_BREAK,
					HSI_CONTROL_CHANNEL, 0);
		mipi_err("Send MSG BREAK TO CP\n");

		/* Send restart command */
		/* schedule_delayed_work(&mipi_ld->start_work, 0); */
		break;

	case HSI_EDLP_MSG_ECHO:
		mipi_info("Receive ECHO\n");

		/* ToDo: Complete ECHO & Check Send/Recv ECHO */
		ret = if_hsi_send_command(mipi_ld, HSI_EDLP_MSG_ECHO,
					HSI_CONTROL_CHANNEL, 0);
		if (ret) {
			mipi_err("if_hsi_send_command fail=%d\n", ret);
			return ret;
		}
		break;

	case HSI_EDLP_MSG_NOP:
		mipi_debug("Receive NOP\n");
		break;

	case HSI_EDLP_MSG_CONF_CH:
		mipi_info("Receive CONF_CH ch:%d, 0x%x\n", ch, param);

		/* No Use */
		param |= ((HSI_EDLP_MSG_CONF_CH & 0x0000000F) << 20);
		ret = if_hsi_send_command(mipi_ld, HSI_EDLP_MSG_NAK,
					ch, param);
		if (ret) {
			mipi_err("if_hsi_send_command fail=%d\n", ret);
			return ret;
		}
		break;

	case HSI_EDLP_MSG_OPEN_CONN:
		mipi_info("Receive OPEN_CONN ch:%d, 0x%x\n", ch, param);

		if (param > mipi_ld->hsi_channels[ch].rx_pdu_size) {
			mipi_info("exceed size:%d(%d)\n", param, ch);
			param |= ((HSI_EDLP_MSG_OPEN_CONN & 0x0000000F) << 20);
			if_hsi_send_command(mipi_ld, HSI_EDLP_MSG_NAK,
					ch, param);
			break;
		}

		ret = if_hsi_read_async(mipi_ld, channel->channel_id,
					param & 0x000FFFFF);
		if (ret) {
			mipi_err("hsi_read fail : %d\n", ret);
			return ret;
		}

		param = (param & 0x000FFFFF) / 4;
		param |= ((HSI_EDLP_MSG_OPEN_CONN & 0x0000000F) << 20);
		ret = if_hsi_send_command(mipi_ld, HSI_EDLP_MSG_ACK,
					ch, param);
		if (ret) {
			mipi_err("if_hsi_send_command fail=%d\n", ret);
			return ret;
		}
		break;

	case HSI_EDLP_MSG_CLOSE_CONN:
		mipi_info("Receive CLOSE_CONN ch:%d, 0x%x\n", ch, param);

		param |= ((HSI_EDLP_MSG_CLOSE_CONN & 0x0000000F) << 20);
		ret = if_hsi_send_command(mipi_ld, HSI_EDLP_MSG_ACK,
					ch, param);
		if (ret) {
			mipi_err("if_hsi_send_command fail=%d\n", ret);
			return ret;
		}
		break;

	case HSI_EDLP_MSG_ACK:
		mipi_debug("Receive ACK ch:%d, 0x%x\n", ch, param);

		switch ((param & 0x00F00000) >> 20) {
		case HSI_EDLP_MSG_CONF_CH:
			mipi_debug("got CONF_CH ACK\n");
			up(&channel->conf_ch_done_sem);
			break;
		case HSI_EDLP_MSG_OPEN_CONN:
			mipi_debug("got OPEN_CONN ACK\n");
			up(&channel->open_conn_done_sem);
			break;
		case HSI_EDLP_MSG_CLOSE_CONN:
			mipi_debug("got CLOSE_CONN ACK\n");
			up(&channel->close_conn_done_sem);
			break;
		case HSI_EDLP_MSG_CREDITS:
			mipi_debug("got CREDITS ACK\n");
			up(&channel->credits_done_sem);
			break;
		default:
			mipi_err("wrong cmd_id:%d\n",
					(param & 0x00F00000) >> 20);
			return -1;
		}
		break;

	case HSI_EDLP_MSG_NAK:
		mipi_info("Receive NAK ch:%d, 0x%x\n", ch, param);

		switch ((param & 0x00F00000) >> 20) {
		case HSI_EDLP_MSG_CONF_CH:
			mipi_info("got CONF_CH NAK\n");
			/* ToDo: Handle NAK */
			up(&channel->conf_ch_done_sem);
			break;
		case HSI_EDLP_MSG_OPEN_CONN:
			mipi_info("got OPEN_CONN NAK\n");
			/* ToDo: Handle NAK */
			up(&channel->open_conn_done_sem);
			break;
		case HSI_EDLP_MSG_CLOSE_CONN:
			mipi_info("got CLOSE_CONN NAK\n");
			/* ToDo: Handle NAK */
			up(&channel->close_conn_done_sem);
			break;
		case HSI_EDLP_MSG_CREDITS:
			mipi_info("got CREDITS ACK\n");
			/* ToDo: Handle NAK */
			up(&channel->credits_done_sem);
			break;
		default:
			mipi_err("wrong cmd_id:%d\n",
					(param & 0x00F00000) >> 20);
			return -1;
		}
		break;

	case HSI_EDLP_MSG_CREDITS:
		credit_cnt = param;
		mipi_debug("Receive CREDITS ch:%d, 0x%x\n", ch, credit_cnt);

		param |= (ch & 0x0000000F) << 24;
		param |= (HSI_EDLP_MSG_CREDITS & 0x0000000F) << 20;
		if_hsi_send_command(mipi_ld, HSI_EDLP_MSG_ACK, ch, param);
		mipi_debug("respond CREDITS ch:%d, 0x%x\n", ch, param);
		break;

	default:
		mipi_err("ERROR... CMD Not supported : %08x\n", cmd);
		return -EINVAL;
	}
	return 0;
}

static void if_hsi_handle_control(struct mipi_link_device *mipi_ld,
						struct if_hsi_channel *channel)
{
	struct io_device *iod;
	struct sk_buff *skb;
	int ret;
	u32 cmd = 0, ch = 0, param = 0;
	gfp_t gfp_val = (in_interrupt()) ? GFP_ATOMIC : GFP_KERNEL;

	switch (mipi_ld->ld.com_state) {
	case COM_HANDSHAKE:
	case COM_ONLINE:
		mipi_debug("RECV CMD : %08x\n", *(u32 *)channel->rx_data);

		if (channel->rx_count != 4) {
			mipi_err("wrong command len : %d\n", channel->rx_count);
			return;
		}

		ret = if_hsi_decode_cmd(mipi_ld, (u32 *)channel->rx_data,
					&cmd, &ch, &param);
		if (ret)
			mipi_err("decode_cmd fail=%d, cmd=%x\n", ret, cmd);
		else {
			mipi_debug("decode_cmd : %08x\n", cmd);
			ret = if_hsi_rx_cmd_handle(mipi_ld, cmd, ch, param);
			if (ret)
				mipi_debug("handle cmd cmd=%x\n", cmd);
		}

		ret = if_hsi_read_async(mipi_ld, channel->channel_id, 4);
		if (ret)
			mipi_err("hsi_read fail : %d\n", ret);
		return;

	case COM_BOOT:
	case COM_BOOT_EBL:
		mipi_debug("receive data : 0x%x(%d)\n",
				*(u32 *)channel->rx_data, channel->rx_count);

		iod = link_get_iod_with_format(&mipi_ld->ld, IPC_BOOT);
		if (iod) {
			skb = alloc_skb(channel->rx_count, gfp_val);
			if (!skb) {
				mif_err("fail alloc skb (%d)\n", __LINE__);
				return;
			}
			memcpy(skb_put(skb, channel->rx_count),
				(void *)channel->rx_data, channel->rx_count);
			ret = iod->recv_skb(iod, &mipi_ld->ld, skb);
			if (ret < 0)
				mipi_err("recv call fail : %d\n", ret);
		}

		ret = if_hsi_read_async(mipi_ld, channel->channel_id, 4);
		if (ret)
			mipi_err("hsi_read fail : %d\n", ret);
		return;

	case COM_CRASH:
		mipi_debug("receive data : 0x%x(%d)\n",
				*(u32 *)channel->rx_data, channel->rx_count);

		iod = link_get_iod_with_format(&mipi_ld->ld, IPC_RAMDUMP);
		if (iod) {
			channel->packet_size = *(u32 *)channel->rx_data;
			mipi_debug("ramdump packet size : %d\n",
						channel->packet_size);
			skb = alloc_skb(channel->packet_size, gfp_val);
			if (!skb) {
				mif_err("fail alloc skb (%d)\n", __LINE__);
				return;
			}
			memcpy(skb_put(skb, channel->packet_size),
					(char *)channel->rx_data + 4,
					channel->packet_size);
			ret = iod->recv_skb(iod, &mipi_ld->ld, skb);
			if (ret < 0)
				mipi_err("recv call fail : %d\n", ret);
		}

		ret = if_hsi_read_async(mipi_ld, channel->channel_id,
					DUMP_PACKET_SIZE * 4);
		if (ret)
			mipi_err("hsi_read fail : %d\n", ret);
		return;

	case COM_NONE:
	default:
		mipi_err("receive data in wrong state : 0x%x(%d)\n",
				*(u32 *)channel->rx_data, channel->rx_count);
		return;
	}
}

static int if_hsi_edlp_send(struct mipi_link_device *mipi_ld,
					int ch, void *data, int len)
{
	struct hsi_msg *tx_msg;
	struct if_hsi_channel *channel = &mipi_ld->hsi_channels[ch];
	unsigned long int flags = in_interrupt() ? GFP_ATOMIC : GFP_KERNEL;
	u32 *hd_ptr32;
	int hd_sz, pk_sz, pk_offset;
	int ret;

	/* check channel open */
	if (!atomic_read(&channel->opened)) {
		ret = if_hsi_open_channel(mipi_ld, channel->channel_id);
		if (ret < 0) {
			mipi_err("if_hsi_open_channel fail: %d\n", ret);
			return ret;
		}
	}

	/* Allocate a new TX msg */
	tx_msg = hsi_alloc_msg(1, flags);
	if (!tx_msg) {
		mipi_err("Out of memory (tx_msg)\n");
		return -ENOMEM;
	}
	mipi_debug("hsi_alloc_msg done\n");

	tx_msg->cl = mipi_ld->client;
	tx_msg->channel = channel->channel_id;
	tx_msg->ttype = HSI_MSG_WRITE;
	tx_msg->context = NULL;
	tx_msg->complete = if_hsi_write_done;
	tx_msg->destructor = if_hsi_msg_destruct;

	/* Allocate data buffer */
	channel->tx_data = if_hsi_buffer_alloc(mipi_ld, tx_msg,
				channel->tx_pdu_size);
	if (!channel->tx_data) {
		mipi_err("Out of memory (hsi_msg buffer => size: %d)\n",
				channel->tx_pdu_size);
		hsi_free_msg(tx_msg);
		return -ENOMEM;
	}
	sg_set_buf(tx_msg->sgt.sgl, channel->tx_data, channel->tx_pdu_size);
	mipi_debug("allocate tx buffer done\n");

	hd_ptr32 = (u32 *)channel->tx_data;

	/* check single packet legnth*/
	hd_sz = get_edlp_header_size(1);
	pk_sz = get_edlp_align_size(len);
	if (hd_sz > channel->tx_pdu_size ||
				hd_sz + pk_sz > channel->tx_pdu_size) {
		mipi_err("too small pdu size for %d bytes\n", len);
		/* Free the TX buff & msg */
		if_hsi_buffer_free(mipi_ld, tx_msg);
		hsi_free_msg(tx_msg);
		return -1;
	}

	/* process pdu */
	/* hd_sz = get_edlp_header_size(n_packets); */
	pk_offset = hd_sz;
	*hd_ptr32 = HSI_EDLP_PDU_SIGNATURE;
	mipi_debug("sig : %x\n", *hd_ptr32);
	/* set header info */
	hd_ptr32++;
	*hd_ptr32 = pk_offset;
	mipi_debug("offset : %x\n", *hd_ptr32);
	hd_ptr32++;
	pk_sz = get_edlp_align_size(len);
	/* add N field in packet size */
	*hd_ptr32 = set_as_last(len);
	mipi_debug("size : %x\n", *hd_ptr32);

	/* copy data to pdu */
	memcpy((void *)channel->tx_data, data, len);

	channel->edlp_tx_seq_num++;

	mod_timer(&mipi_ld->hsi_acwake_down_timer,
			jiffies + HSI_ACWAKE_DOWN_TIMEOUT);
	if_hsi_set_wakeline(mipi_ld, 1);

	/* Send the TX HSI msg */
	ret = hsi_async(mipi_ld->client, tx_msg);
	if (ret) {
		mipi_err("ch=%d, hsi_async(write) fail=%d\n",
					channel->channel_id, ret);
		return ret;
	}

	if (down_timeout(&channel->write_done_sem,
					HSI_WRITE_DONE_TIMEOUT) < 0) {
		mipi_err("ch=%d, hsi_write_done timeout\n",
					channel->channel_id);
		return -ETIMEDOUT;
	}

	return 0;
}

static int if_hsi_edlp_recv(struct io_device *iod,
	struct mipi_link_device *mipi_ld, unsigned int ch, int actual_len)
{
	struct if_hsi_channel *channel = &mipi_ld->hsi_channels[ch];
	int ret;
	u32 pk_last, np_fld;
	struct sk_buff *skb;
	void *buff;
	u32 *hd_ptr32;
	int n_packets, pk_sz, pk_offset;
	int force_pad = channel->is_dma_ch ? HSI_NET_PAD_FOR_CP : 0;

	if (!channel->rx_data) {
		mipi_err("rx_data is null\n");
		return -ENOMEM;
	}
	/* check rx length does not exceed MAX pdu size */
	/* now fixed pdu size used,
	 * if thd actual length does not matches to rx_pdu_size
	 * it could lead packet drop.
	 */
	if (actual_len != channel->rx_pdu_size) {
		mipi_err("rx data size is crashed: %d:%d(%d)\n",
			actual_len, channel->rx_pdu_size, channel->channel_id);
		return -EINVAL;
	}

	mipi_debug("len:%d\n", actual_len);

	/* set address variable */
	buff = (void *)channel->rx_data;
	hd_ptr32 = (u32 *)buff;

	/* check sign first */
	if (!if_hsi_edlp_check_sign(hd_ptr32)) {
		mipi_err("signature not match: %p(0x%x), ch:%d\n", hd_ptr32,
					*(u32 *)buff, channel->channel_id);
		print_hex_dump(KERN_DEBUG, "ERROR-PDU: ",
					DUMP_PREFIX_NONE,
					1, 1,
					(void *)buff,
					(size_t)16, false);

		/* cp force crash to get cp ramdump */
		if (iod->mc->gpio_ap_dump_int)
			iod->mc->ops.modem_force_crash_exit(
						iod->mc);
		return -EINVAL;
	}
	/* proceed to packet info */
	hd_ptr32++;
	/* start loop msg sequence */

	/* print_hex_dump(KERN_INFO, "EDLP-RX-PDU: ",
				DUMP_PREFIX_OFFSET, 16, 4, (void *)buff,
				128, true); */

	/* now ptr points first start address */
	n_packets = 0;
	do {
		mipi_debug("pdu start address: 0x%p, cur pkt 0x%p",
						channel->rx_data, hd_ptr32);
		/**
				<packet header structure>
				 31	 23	 15	 7     0
				|	|	|	|	|
			4 byte	|	start address		|
			4 byte4	|n|p |	rsv   |	     size	|

			n field :	0b0: last, 0b1: more packet field
			p field :	0b11 : complete, 0b10 : start,
					0b01 : end, 0b00 : middle

		*/
		/* get packet info from current pdu offset */
		pk_offset = *hd_ptr32;
		hd_ptr32++;
		pk_sz = *hd_ptr32;
		hd_ptr32++;
		np_fld = pk_sz & (HSI_EDLP_N_FLD_MASK | HSI_EDLP_P_FLD_MASK);
		pk_sz = HSI_EDLP_SIZE(pk_sz);

		/* P field : ignored, currently it is not supported */
		/* N field : check last packet of this PDU */
		pk_last = HSI_EDLP_CHECK_LAST(np_fld);

		mipi_debug("[#%d]: offset 0x%x, np = 0x%x, sz 0x%x, last? %d\n",
				n_packets, pk_offset, np_fld, pk_sz, pk_last);

		/* offset ptr cannot exceed rx_len, but also offset + size */
		if (pk_offset + pk_sz > channel->rx_pdu_size)
			return -EINVAL;

		if (iod->ndev) {
			skb = netdev_alloc_skb_ip_align(iod->ndev,
						pk_sz - force_pad);
			if (skb) {
				skb_reset_mac_header(skb);
				skb->ip_summed = CHECKSUM_UNNECESSARY;
			}
		} else
			skb = alloc_skb(pk_sz - force_pad, GFP_ATOMIC);
		if (!skb) {
			mif_err("fail alloc skb (%d)\n", __LINE__);
			return -ENOMEM;
		}

		memcpy(skb_put(skb, pk_sz - force_pad),
		(void *)(buff + pk_offset + force_pad), pk_sz - force_pad);

		ret = iod->recv_skb(iod, &mipi_ld->ld, skb);
		if (ret < 0) {
			mipi_err("recv call fail : %d\n", ret);
			return ret;
		}
		mipi_debug("read pk_size: %d\n", pk_sz);

		if ((channel->channel_id == HSI_MULTI_FMT_CHANNEL) ||
			(channel->channel_id == HSI_EXTRA_CHANNEL))
			print_hex_dump(KERN_DEBUG, "FMT-RX: ",
					DUMP_PREFIX_NONE,
					1, 1,
					(void *)(buff + pk_offset + force_pad),
					(pk_sz - force_pad) <= 16 ?
					(size_t)(pk_sz - force_pad) :
					(size_t)16, false);

		if (pk_last)
			break;

	/* it's no use to compare max count, if packet number only follows
	 * actual rx, not pre-defined limit
	 * But it can prevent infinite loop from mean data stream */
	} while (n_packets++ < EDLP_PACKET_MAX);

	return 0;
}

static void *if_hsi_buffer_alloc(struct mipi_link_device *mipi_ld,
			struct hsi_msg *msg, unsigned int size)
{
	struct if_hsi_channel *channel = &mipi_ld->hsi_channels[msg->channel];
	int flags = in_interrupt() ? GFP_ATOMIC : GFP_KERNEL;
	void *buff = NULL;

	if (channel->is_dma_ch)
		buff = dma_alloc_coherent(mipi_ld->controller, size,
				&sg_dma_address(msg->sgt.sgl), flags);
	else {
		if ((size >= PAGE_SIZE) && (size & (PAGE_SIZE - 1)))
			buff = (void *)__get_free_pages(flags,
						get_order(size));
		else
			buff = kmalloc(size, flags);
	}

	return buff;
}

static void if_hsi_buffer_free(struct mipi_link_device *mipi_ld,
			struct hsi_msg *msg)
{
	struct if_hsi_channel *channel = &mipi_ld->hsi_channels[msg->channel];

	if (channel->is_dma_ch)
		dma_free_coherent(mipi_ld->controller,
				msg->sgt.sgl->length,
				sg_virt(msg->sgt.sgl),
				sg_dma_address(msg->sgt.sgl));
	else {
		if ((msg->sgt.sgl->length >= PAGE_SIZE) &&
			(msg->sgt.sgl->length & (PAGE_SIZE - 1)))
			free_pages((unsigned int)sg_virt(msg->sgt.sgl),
				get_order(msg->sgt.sgl->length));
		else
			kfree(sg_virt(msg->sgt.sgl));
	}
}

static void if_hsi_msg_destruct(struct hsi_msg *msg)
{
	struct mipi_link_device *mipi_ld =
		(struct mipi_link_device *)hsi_client_drvdata(msg->cl);

	if (msg->ttype == HSI_MSG_WRITE)
		/* Free the buff */
		if_hsi_buffer_free(mipi_ld, msg);

	/* Free the msg */
	hsi_free_msg(msg);
}

static void if_hsi_write_done(struct hsi_msg *msg)
{
	unsigned long int flags;
	struct mipi_link_device *mipi_ld =
			(struct mipi_link_device *)hsi_client_drvdata(msg->cl);
	struct if_hsi_channel *channel = &mipi_ld->hsi_channels[msg->channel];
	struct io_device *iod = NULL;
	u32 command = 0;
	u32 ch = 0;
	u32 credit_cnt = 0;
	int total_credit = 0;

	mipi_debug("got write data=0x%x(%d)\n", *(u32 *)sg_virt(msg->sgt.sgl),
				msg->actual_len);

	if ((mipi_ld->ld.com_state == COM_ONLINE) &&
		(channel->channel_id == HSI_CONTROL_CHANNEL)) {
		command = *(u32 *)sg_virt(msg->sgt.sgl);
		if ((((command & 0xF0000000) >> 28) == HSI_EDLP_MSG_ACK) &&
		(((command & 0x00F00000) >> 20) == HSI_EDLP_MSG_CREDITS)) {
			ch = ((command & 0x0F000000) >> 24);
			credit_cnt = (command & 0x000000FF);
			mipi_info("send credit-ack done: ch=%d, 0x%x, " \
				"add_credit=%d\n", ch, command, credit_cnt);

			if ((ch < HSI_NUM_OF_USE_CHANNELS) &&
				mipi_ld->hsi_channels[ch].use_credits) {
				spin_lock_irqsave(&mipi_ld->credit_lock, flags);
				/* add credits to current value
				 * and return sum of credits */
				if (!atomic_read(
					&mipi_ld->hsi_channels[ch].credits)) {
					mipi_info("prev CREDITS is zero\n");
					iod = if_hsi_get_iod_with_hsi_channel(
							mipi_ld, ch);
					if (iod)
						iodev_netif_wake(iod, NULL);
				}

				/* does it needs lock ?? */
				total_credit = atomic_add_return(credit_cnt,
					&mipi_ld->hsi_channels[ch].credits);
				mipi_info("total CREDITS ch:%d, %d(+%d)\n", ch,
						total_credit, credit_cnt);

				/* with credits, re-start tx work */
				wake_up(
				&mipi_ld->hsi_channels[ch].credit_waitq);
				queue_work(mipi_ld->hsi_channels[ch].tx_wq,
					&mipi_ld->hsi_channels[ch].tx_w);
				spin_unlock_irqrestore(&mipi_ld->credit_lock,
							flags);
			}
		}
	}

	channel->tx_count = msg->actual_len;
	up(&channel->write_done_sem);

	/* Free the TX buff & msg */
	if_hsi_buffer_free(mipi_ld, msg);
	hsi_free_msg(msg);
}

static int if_hsi_write(struct mipi_link_device *mipi_ld, unsigned int ch_num,
			u32 *data, unsigned int size, int tx_mode)
{
	int ret;
	unsigned long int flags = in_interrupt() ? GFP_ATOMIC : GFP_KERNEL;
	struct if_hsi_channel *channel = &mipi_ld->hsi_channels[ch_num];
	struct hsi_msg *tx_msg = NULL;

	mipi_debug("submit write data : 0x%x(%d)\n", *data, size);

	/* Allocate a new TX msg */
	tx_msg = hsi_alloc_msg(1, flags);
	if (!tx_msg) {
		mipi_err("Out of memory (tx_msg)\n");
		return -ENOMEM;
	}
	mipi_debug("hsi_alloc_msg done\n");

	tx_msg->cl = mipi_ld->client;
	tx_msg->channel = ch_num;
	tx_msg->ttype = HSI_MSG_WRITE;
	tx_msg->context = NULL;
	tx_msg->complete = if_hsi_write_done;
	tx_msg->destructor = if_hsi_msg_destruct;

	/* Allocate data buffer */
	channel->tx_count = ALIGN(size, 4);
	channel->tx_data = if_hsi_buffer_alloc(mipi_ld, tx_msg,
				channel->tx_count);
	if (!channel->tx_data) {
		mipi_err("Out of memory (hsi_msg buffer => size: %d)\n",
				channel->tx_count);
		hsi_free_msg(tx_msg);
		return -ENOMEM;
	}
	sg_set_buf(tx_msg->sgt.sgl, channel->tx_data, channel->tx_count);
	mipi_debug("allocate tx buffer done\n");

	/* Copy Data */
	memcpy(sg_virt(tx_msg->sgt.sgl), data, size);
	mipi_debug("copy tx buffer done\n");

	/* Send the TX HSI msg */
	ret = hsi_async(mipi_ld->client, tx_msg);
	if (ret) {
		mipi_err("ch=%d, hsi_async(write) fail=%d\n",
					channel->channel_id, ret);

		/* Free the TX buff & msg */
		/* if_hsi_buffer_free(mipi_ld, tx_msg);
		hsi_free_msg(tx_msg); */
		return ret;
	}
	mipi_debug("hsi_async done\n");

	if (tx_mode != HSI_TX_MODE_SYNC)
		return channel->tx_count;

	if (down_timeout(&channel->write_done_sem,
				HSI_WRITE_DONE_TIMEOUT) < 0) {
		mipi_err("ch=%d, hsi_write_done timeout : %d\n",
					channel->channel_id, size);

		/* Free the TX buff & msg */
		/* if_hsi_buffer_free(mipi_ld, tx_msg);
		hsi_free_msg(tx_msg); */
		return -ETIMEDOUT;
	}

	return channel->tx_count;
}

static void if_hsi_read_done(struct hsi_msg *msg)
{
	int ret;
	struct mipi_link_device *mipi_ld =
			(struct mipi_link_device *)hsi_client_drvdata(msg->cl);
	struct if_hsi_channel *channel = &mipi_ld->hsi_channels[msg->channel];
	struct io_device *iod;
	int iod_id = 0;
	enum dev_format format_type = 0;

	mipi_debug("got read data=0x%x(%d)\n", *(u32 *)channel->rx_data,
				msg->actual_len);

	channel->rx_count = msg->actual_len;
	hsi_free_msg(msg);

	if (channel->channel_id == HSI_CONTROL_CHANNEL) {
		if_hsi_handle_control(mipi_ld, channel);
		return;
	}

	iod = if_hsi_get_iod_with_hsi_channel(mipi_ld, channel->channel_id);
	if (iod) {
		mipi_debug("RECV DATA : %08x(%d)-%d\n",
				*(u32 *)channel->rx_data, channel->rx_count,
				iod->format);

		ret = if_hsi_edlp_recv(iod, mipi_ld, channel->channel_id,
							channel->rx_count);
		if (ret < 0) {
			mipi_err("edlp recv call fail : %d\n", ret);
			mipi_err("discard data: channel=%d, packet_size=%d\n",
				channel->channel_id, channel->rx_count);
		}

		ret = if_hsi_read_async(mipi_ld, channel->channel_id,
					channel->rx_pdu_size);
		if (ret)
			mipi_err("hsi_read fail : %d\n", ret);
	}
}

static int if_hsi_read_async(struct mipi_link_device *mipi_ld,
			unsigned int ch_num, unsigned int size)
{
	int ret;
	unsigned long int flags = in_interrupt() ? GFP_ATOMIC : GFP_KERNEL;
	struct if_hsi_channel *channel = &mipi_ld->hsi_channels[ch_num];
	struct hsi_msg *rx_msg = NULL;

	mipi_debug("submit read data : (%d)\n", size);

	/* Allocate a new RX msg */
	rx_msg = hsi_alloc_msg(1, flags);
	if (!rx_msg) {
		mipi_err("Out of memory (rx_msg)\n");
		return -ENOMEM;
	}
	mipi_debug("hsi_alloc_msg done\n");

	rx_msg->cl = mipi_ld->client;
	rx_msg->channel = ch_num;
	rx_msg->ttype = HSI_MSG_READ;
	rx_msg->context = NULL;
	rx_msg->complete = if_hsi_read_done;
	rx_msg->destructor = if_hsi_msg_destruct;

	sg_set_buf(rx_msg->sgt.sgl, channel->rx_data, size);
	mipi_debug("allocate rx buffer done\n");

	/* Send the RX HSI msg */
	ret = hsi_async(mipi_ld->client, rx_msg);
	if (ret) {
		mipi_err("ch=%d, hsi_async(read) fail=%d\n",
					channel->channel_id, ret);
		/* Free the RX msg */
		hsi_free_msg(rx_msg);
		return ret;
	}
	mipi_debug("hsi_async done\n");

	return 0;
}

static void if_hsi_port_event(struct hsi_client *cl, unsigned long event)
{
	struct mipi_link_device *mipi_ld =
			(struct mipi_link_device *)hsi_client_drvdata(cl);

	switch (event) {
	case HSI_EVENT_START_RX:
		mipi_info("CAWAKE(1)\n");
		return;

	case HSI_EVENT_STOP_RX:
		mipi_info("CAWAKE(0)\n");
		return;

	default:
		mipi_err("Unknown Event : %ld\n", event);
		return;
	}
}

static int __devinit if_hsi_probe(struct device *dev)
{
	struct mipi_link_device *mipi_ld =
			(struct mipi_link_device *)if_hsi_driver.priv_data;

	/* The parent of our device is the HSI port,
	 * the parent of the HSI port is the HSI controller device */
	struct device *controller = dev->parent->parent;
	struct hsi_client *client = to_hsi_client(dev);

	/* Save the controller & client */
	mipi_ld->controller = controller;
	mipi_ld->client = client;
	mipi_ld->is_dma_capable = is_device_dma_capable(controller);

	/* Warn if no DMA capability */
	if (!mipi_ld->is_dma_capable)
		mipi_info("HSI device is not DMA capable !\n");

	/* Save private Data */
	hsi_client_set_drvdata(client, (void *)mipi_ld);

	mipi_debug("if_hsi_probe() Done\n");
	return 0;
}

static void if_hsi_shutdown(struct device *dev)
{
	struct mipi_link_device *mipi_ld =
			(struct mipi_link_device *)if_hsi_driver.priv_data;

	if (hsi_port_claimed(mipi_ld->client)) {
		mipi_info("Releasing the HSI controller\n");
		hsi_release_port(mipi_ld->client);
	}

	mipi_info("if_hsi_shutdown() Done\n");
}

static inline void if_hsi_free_buffer(struct mipi_link_device *mipi_ld)
{
	int i;
	struct if_hsi_channel *channel;

	for (i = 0; i < HSI_NUM_OF_USE_CHANNELS; i++) {
		channel = &mipi_ld->hsi_channels[i];
		if (channel->rx_data != NULL) {
			kfree(channel->rx_data);
			channel->rx_data = NULL;
		}
	}
}

static inline void if_hsi_destroy_workqueue(struct mipi_link_device *mipi_ld)
{
	int i;
	struct if_hsi_channel *channel;

	for (i = 0; i < HSI_NUM_OF_USE_CHANNELS; i++) {
		channel = &mipi_ld->hsi_channels[i];
		if (channel->tx_wq) {
			destroy_workqueue(channel->tx_wq);
			channel->tx_wq = NULL;
		}
	}
}

static int if_hsi_init(struct link_device *ld)
{
	struct mipi_link_device *mipi_ld = to_mipi_link_device(ld);
	struct if_hsi_channel *channel;
	int i, ret;

	for (i = 0; i < HSI_NUM_OF_USE_CHANNELS; i++) {
		mipi_debug("make ch %d\n", i);
		channel = &mipi_ld->hsi_channels[i];
		channel->channel_id = i;

		channel->use_credits = ch_info[i].use_credits;
		atomic_set(&channel->credits, 0);
		channel->is_dma_ch = ch_info[i].is_dma_ch;

		/* create tx workqueue */
		if (ch_info[i].wq_priority == WQ_HIGHPRI)
			channel->tx_wq = create_hi_prio_wq(ch_info[i].name);
		else
			channel->tx_wq = create_norm_prio_wq(ch_info[i].name);
		if (!channel->tx_wq) {
			ret = -ENOMEM;
			mipi_err("fail to make wq\n");
			goto destory_wq;
		}
		mipi_debug("tx_wq : %p\n", channel->tx_wq);

		/* allocate rx / tx data buffer */
		channel->tx_pdu_size = ch_info[i].tx_pdu_sz;
		channel->rx_pdu_size = ch_info[i].rx_pdu_sz;
		if (!channel->rx_pdu_size)
			goto variable_init;

		channel->rx_data = kmalloc(channel->rx_pdu_size,
							GFP_DMA | GFP_ATOMIC);
		if (!channel->rx_data) {
			mipi_err("alloc ch %d rx_data fail\n", i);
			ret = -ENOMEM;
			goto free_buffer;
		}

variable_init:
		mipi_debug("rx_buf = %p\n", channel->rx_data);

		atomic_set(&channel->opened, 0);

		sema_init(&channel->write_done_sem, HSI_SEMAPHORE_COUNT);
		sema_init(&channel->conf_ch_done_sem, HSI_SEMAPHORE_COUNT);
		sema_init(&channel->open_conn_done_sem, HSI_SEMAPHORE_COUNT);
		sema_init(&channel->close_conn_done_sem, HSI_SEMAPHORE_COUNT);
		sema_init(&channel->credits_done_sem, HSI_SEMAPHORE_COUNT);

		skb_queue_head_init(&channel->tx_q);

		if (i != HSI_CONTROL_CHANNEL)
			/* raw dma tx composition can be varied with fmt */
			INIT_WORK(&channel->tx_w, mipi_hsi_send_work);

		/* save referece to link device */
		channel->ld = ld;

		init_waitqueue_head(&channel->credit_waitq);
	}

	/* init delayed work for IPC handshaking */
	INIT_DELAYED_WORK(&mipi_ld->start_work, if_hsi_start_work);

	/* init timer to stop tx if there's no more transit */
	setup_timer(&mipi_ld->hsi_acwake_down_timer, if_hsi_acwake_down_func,
				(unsigned long)mipi_ld);
	atomic_set(&mipi_ld->acwake, 0);
	spin_lock_init(&mipi_ld->acwake_lock);
	spin_lock_init(&mipi_ld->credit_lock);

	/* Register HSI Client driver */
	if_hsi_driver.priv_data = (void *)mipi_ld;
	ret = hsi_register_client_driver(&if_hsi_driver);
	if (ret) {
		mipi_err("hsi_register_client_driver() fail : %d\n", ret);
		goto free_buffer;
	}
	mipi_debug("hsi_register_client_driver() Done: %d\n", ret);

	return 0;

free_buffer:
	if_hsi_free_buffer(mipi_ld);
destory_wq:
	if_hsi_destroy_workqueue(mipi_ld);
	return ret;
}

struct link_device *mipi_edlp_create_link_device(struct platform_device *pdev)
{
	int ret;
	struct mipi_link_device *mipi_ld;
	struct link_device *ld;

	mipi_ld = kzalloc(sizeof(struct mipi_link_device), GFP_KERNEL);
	if (!mipi_ld)
		return NULL;

	wake_lock_init(&mipi_ld->wlock, WAKE_LOCK_SUSPEND, "mipi_link");

	ld = &mipi_ld->ld;

	ld->name = "mipi_hsi";
	ld->init_comm = mipi_hsi_init_communication;
	ld->terminate_comm = mipi_hsi_terminate_communication;
	ld->ioctl = mipi_hsi_ioctl;
	ld->send = mipi_hsi_send;
	ld->com_state = COM_NONE;

	ret = if_hsi_init(ld);
	if (ret)
		return NULL;

	return ld;
}

