
// SPDX-License-Identifier: GPL-2.0+
/*
 * u_ether.c -- Ethernet-over-USB link layer utilities for Gadget stack
 *
 * Copyright (C) 2003-2005,2008 David Brownell
 * Copyright (C) 2003-2004 Robert Schwebel, Benedikt Spranger
 * Copyright (C) 2008 Nokia Corporation
 */

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/gfp.h>
#include <linux/device.h>
#include <linux/ctype.h>
#include <linux/etherdevice.h>
#include <linux/ethtool.h>
#include <linux/if_vlan.h>
#include <linux/hrtimer.h>

#include "u_ether.h"
#include "rndis.h"

static inline int is_promisc(u16 cdc_filter)
{
	return cdc_filter & USB_CDC_PACKET_TYPE_PROMISCUOUS;
}

#ifdef NCM_WITH_TIMER
static void tx_complete_ncm_timer(struct usb_ep *ep, struct usb_request *req)
{
	struct sk_buff	*skb = req->context;
	struct eth_dev	*dev = ep->driver_data;

	switch (req->status) {
	default:
		dev->net->stats.tx_errors++;
		VDBG(dev, "tx err %d\n", req->status);
#ifdef CONFIG_USB_NCM_SUPPORT_MTU_CHANGE
		printk(KERN_ERR"usb:%s tx err %d\n", __func__, req->status);
#endif
		/* FALLTHROUGH */
	case -ECONNRESET:		/* unlink */
	case -ESHUTDOWN:		/* disconnect etc */
		break;
	case 0:
		if (!req->zero && !dev->zlp)
			dev->net->stats.tx_bytes += req->length-1;
		else
			dev->net->stats.tx_bytes += req->length;

		if (skb)
			dev_consume_skb_any(skb);
	}
	dev->net->stats.tx_packets++;

	spin_lock(&dev->tx_req_lock);

	req->length = 0;
	list_add_tail(&req->list, &dev->tx_reqs);

	spin_unlock(&dev->tx_req_lock);

	atomic_dec(&dev->tx_qlen);

	if (netif_carrier_ok(dev->net))
		netif_wake_queue(dev->net);
}

static int tx_task_ncm(struct eth_dev *dev, struct usb_request *req)
{
	struct usb_ep *in = dev->port_usb->in_ep;
	int length = req->length;
	int retval;

	req->complete = tx_complete_ncm_timer;

	/* NCM requires no zlp if transfer is dwNtbInMaxSize */
	if (dev->port_usb->is_fixed && length == dev->port_usb->fixed_in_len &&
		(length % in->maxpacket) == 0)
		req->zero = 0;
	else
		req->zero = 1;

	/* use zlp framing on tx for strict CDC-Ether conformance,
	 * though any robust network rx path ignores extra padding.
	 * and some hardware doesn't like to write zlps.
	 */
	if (req->zero && !dev->zlp && (length % in->maxpacket) == 0) {
		req->zero = 0;
		length++;
	}
	req->length = length;

#ifdef HS_THROTTLE_IRQ
	/* throttle highspeed IRQ rate back slightly */
	if (gadget_is_dualspeed(dev->gadget) &&
		(dev->gadget->speed == USB_SPEED_HIGH)) {
		atomic_inc(&dev->tx_qlen);
		if (atomic_read(&dev->tx_qlen) == (dev->qmult/2)) {
			req->no_interrupt = 0;
			atomic_set(&dev->tx_qlen, 0);
		} else {
			req->no_interrupt = 1;
		}
	} else {
		req->no_interrupt = 0;
	}
#else
	atomic_inc(&dev->tx_qlen);
#endif
	retval = usb_ep_queue(in, req, GFP_ATOMIC);

	return retval;
}

enum hrtimer_restart tx_timeout_ncm(struct hrtimer *data)
{
	struct eth_dev *dev = container_of(data, struct eth_dev, tx_timer);
	struct usb_request *req = NULL;
	int retval;
	unsigned long flags;

	spin_lock_irqsave(&dev->tx_req_lock, flags);
	dev->tx_skb_hold_count = 0;

	/*
	* this freelist can be empty if an interrupt triggered disconnect()
	* and reconfigured the gadget (shutting down this queue) after the
	* network stack decided to xmit but before we got the spinlock.
	*/

	if (list_empty(&dev->tx_reqs)) {
		spin_unlock_irqrestore(&dev->tx_req_lock, flags);
		return HRTIMER_NORESTART;
	}

	req = container_of(dev->tx_reqs.next, struct usb_request, list);
	req->func_flag = NCM_ADD_HEADER;

	list_del(&req->list);

	/* temporarily stop TX queue when the freelist empties */
	if (list_empty(&dev->tx_reqs))
		netif_stop_queue(dev->net);

	spin_unlock_irqrestore(&dev->tx_req_lock, flags);

	dev->occured_timeout = 1;
	retval = tx_task_ncm(dev, req);
	switch (retval) {
		default:
			DBG(dev, "tx queue err %d\n", retval);
			break;
#if 0
		case 0:
			dev->net->trans_start = jiffies;
#endif
	}

    if (retval) {
		req->length = 0;
		dev->net->stats.tx_dropped++;
		spin_lock_irqsave(&dev->tx_req_lock, flags);
		if (list_empty(&dev->tx_reqs))
			netif_start_queue(dev->net);
		list_add(&req->list, &dev->tx_reqs);
		spin_unlock_irqrestore(&dev->tx_req_lock, flags);
	}

	return HRTIMER_NORESTART;
}

netdev_tx_t eth_start_xmit_ncm_timer(struct sk_buff *skb,
						struct net_device *net)
{
	struct eth_dev		*dev = netdev_priv(net);
	int			length = 0;
	int			retval;
	struct usb_request	*req = NULL;
	unsigned long		flags;
	struct usb_ep		*in;
	u16			cdc_filter;
	int added_offset = 0;
	bool eth_supports_multi_frame = 0;

	if (dev->en_timer) {
		hrtimer_cancel(&dev->tx_timer);
		dev->en_timer = 0;
	}

	spin_lock_irqsave(&dev->lock, flags);
	if (dev->port_usb) {
		in = dev->port_usb->in_ep;
		cdc_filter = dev->port_usb->cdc_filter;
		eth_supports_multi_frame = dev->port_usb->supports_multi_frame;
	} else {
		in = NULL;
		cdc_filter = 0;
	}
	spin_unlock_irqrestore(&dev->lock, flags);

	/* fix prevent */
	if (!skb)
		return NETDEV_TX_BUSY;

	if (skb && !in) {
		dev_kfree_skb_any(skb);
		return NETDEV_TX_OK;
	}

	/* apply outgoing CDC or RNDIS filters */
	if (skb && !is_promisc(cdc_filter)) {
		u8		*dest = skb->data;

		if (is_multicast_ether_addr(dest)) {
			u16	type;

			/* ignores USB_CDC_PACKET_TYPE_MULTICAST and host
			 * SET_ETHERNET_MULTICAST_FILTERS requests
			 */
			if (is_broadcast_ether_addr(dest))
				type = USB_CDC_PACKET_TYPE_BROADCAST;
			else
				type = USB_CDC_PACKET_TYPE_ALL_MULTICAST;
			if (!(cdc_filter & type)) {
				dev_kfree_skb_any(skb);
				return NETDEV_TX_OK;
			}
		}
		/* ignores USB_CDC_PACKET_TYPE_DIRECTED */
	}
	spin_lock_irqsave(&dev->tx_req_lock, flags);
	/*
	 * this freelist can be empty if an interrupt triggered disconnect()
	 * and reconfigured the gadget (shutting down this queue) after the
	 * network stack decided to xmit but before we got the spinlock.
	 */
	if (list_empty(&dev->tx_reqs)) {
		spin_unlock_irqrestore(&dev->tx_req_lock, flags);
		return NETDEV_TX_BUSY;
	}

	req = container_of(dev->tx_reqs.next, struct usb_request, list);
	list_del(&req->list);

	/* temporarily stop TX queue when the freelist empties */
	if (list_empty(&dev->tx_reqs)) {
		dev->net->stats.tx_dropped++;
		netif_stop_queue(net);
	}
	spin_unlock_irqrestore(&dev->tx_req_lock, flags);

	/* no buffer copies needed, unless the network stack did it
	 * or the hardware can't use skb buffers.
	 * or there's not enough space for extra headers we need
	 */
	if (dev->wrap) {
		unsigned long	flags;

		spin_lock_irqsave(&dev->lock, flags);
		if (dev->port_usb) {
			skb = dev->wrap(dev->port_usb, skb);
		}

		dev->tx_skb_hold_count = 0;
		spin_unlock_irqrestore(&dev->lock, flags);
		if (!skb) {
			/* Multi frame CDC protocols may store the frame for
			 * later which is not a dropped frame.
			 */
			if (eth_supports_multi_frame)
				goto multiframe;
			goto drop;
		}
	}

	if (req->func_flag == NCM_ADD_HEADER) {
		spin_lock_irqsave(&dev->lock, flags);
		added_offset = NCM_HEADER_SIZE;
		dev->tx_skb_hold_count = 0;
		ncm_add_header(added_offset, req->buf, skb->len);
		spin_unlock_irqrestore(&dev->lock, flags);
	}

	if (dev->port_usb->multi_pkt_xfer) {
		int skb_len;
		unsigned long	tx_timeout;

		/* Copy received IP data from SKB */
		memcpy(req->buf + req->length + added_offset, skb->data, skb->len);

		/* Increment req length by skb data length */
		req->length = req->length + skb->len + added_offset;
		skb_len = skb->len;
		length = req->length;
		dev_kfree_skb_any(skb);
		req->context = NULL;

		spin_lock_irqsave(&dev->tx_req_lock, flags);

		if (req->func_flag == NCM_ADD_DGRAM) {
			ncm_add_datagram(dev->port_usb, (__le16 *)(req->buf),
					skb_len, dev->tx_skb_hold_count);
		}
		dev->tx_skb_hold_count++;

		if (dev->tx_skb_hold_count < dev->dl_max_pkts_per_xfer) {
			req->func_flag = NCM_ADD_DGRAM;
			list_add(&req->list, &dev->tx_reqs);
			spin_unlock_irqrestore(&dev->tx_req_lock, flags);
			tx_timeout = dev->occured_timeout ?
						MIN_TX_TIMEOUT_NSECS : MAX_TX_TIMEOUT_NSECS;
			dev->occured_timeout = 0;
			hrtimer_start(&dev->tx_timer, ktime_set(0, tx_timeout),
					HRTIMER_MODE_REL);
			dev->en_timer = 1;
			goto success;
		}
		req->func_flag = NCM_ADD_HEADER;

		dev->tx_skb_hold_count = 0;
		spin_unlock_irqrestore(&dev->tx_req_lock, flags);

	} else {
		length = skb->len;
		req->buf = skb->data;
		req->context = skb;
	}

	retval = tx_task_ncm(dev, req);

	if (retval) {
		if (!dev->port_usb->multi_pkt_xfer)
			dev_kfree_skb_any(skb);
drop:
		dev->net->stats.tx_dropped++;
		req->length = 0;
multiframe:
		spin_lock_irqsave(&dev->tx_req_lock, flags);
		if (list_empty(&dev->tx_reqs))
			netif_start_queue(net);
		list_add(&req->list, &dev->tx_reqs);
		spin_unlock_irqrestore(&dev->tx_req_lock, flags);
	}
success:
	return NETDEV_TX_OK;
}

#else /* NCM WITHOUT TIMER */

static void tx_complete_ncm(struct usb_ep *ep, struct usb_request *req)
{
	struct sk_buff	*skb = req->context;
	struct eth_dev	*dev = ep->driver_data;
	struct usb_request *new_req;
	struct usb_ep *in;
	int length;
	int retval;
	int pkts_compl;

	switch (req->status) {
	default:
		dev->net->stats.tx_errors++;
		VDBG(dev, "tx err %d\n", req->status);
#ifdef CONFIG_USB_NCM_SUPPORT_MTU_CHANGE
		printk(KERN_ERR"usb:%s tx err %d\n", __func__, req->status);
#endif
		/* FALLTHROUGH */
	case -ECONNRESET:		/* unlink */
	case -ESHUTDOWN:		/* disconnect etc */
		break;
	case 0:
		if (!req->zero && !dev->zlp)
			dev->net->stats.tx_bytes += req->length-1;
		else
			dev->net->stats.tx_bytes += req->length;

		if (skb)
			dev_consume_skb_any(skb);
	}
	dev->net->stats.tx_packets++;

	pkts_compl = dev->port_usb->multi_pkt_xfer ? dev->dl_max_pkts_per_xfer : 1;
	netdev_completed_queue(dev->net, pkts_compl, req->length);

	spin_lock(&dev->tx_req_lock);

	/* dev->tx_skb_hold_count = 0; */
	req->length = 0;
	list_add_tail(&req->list, &dev->tx_reqs);

	/* list and and just return worh usb reset or shutdown */
	if (req->status == -ESHUTDOWN) {
		spin_unlock(&dev->tx_req_lock);
		return;
	}

	if (dev->port_usb->multi_pkt_xfer) {
		dev->no_tx_req_used--;
		in = dev->port_usb->in_ep;

		if (!list_empty(&dev->tx_reqs)) {
			new_req = container_of(dev->tx_reqs.next,
					struct usb_request, list);
			new_req->func_flag = NCM_ADD_HEADER;
			/* dev->tx_skb_hold_count = 0; */

			list_del(&new_req->list);
			spin_unlock(&dev->tx_req_lock);
			if (new_req->length > 0) {
				length = new_req->length;

				new_req->zero = 0;
				if ((length % in->maxpacket) == 0) {
					new_req->zero = 1;
					dev->no_of_zlp++;
				}

				/* NCM requires no zlp if transfer is dwNtbInMaxSize */
				if (dev->port_usb->is_fixed) {
					if (length == dev->port_usb->fixed_in_len) {
						new_req->zero = 0;
						dev->no_of_zlp--;
					}
				}

				/* use zlp framing on tx for strict CDC-Ether
				 * conformance, though any robust network rx
				 * path ignores extra padding. and some hardware
				 * doesn't like to write zlps.
				 */
				if (new_req->zero && !dev->zlp &&
						(length % in->maxpacket) == 0) {
					length++;
				}

				new_req->length = length;
				new_req->complete = tx_complete_ncm;

				retval = usb_ep_queue(in, new_req, GFP_ATOMIC);
				switch (retval) {
				default:
					printk(KERN_ERR"usb: dropped tx_complete_newreq(%pK)\n", new_req);
					DBG(dev, "tx queue err %d\n", retval);
					new_req->length = 0;
					spin_lock(&dev->tx_req_lock);
					list_add_tail(&new_req->list, &dev->tx_reqs);
					spin_unlock(&dev->tx_req_lock);
					break;
				case 0:
					spin_lock(&dev->tx_req_lock);
					dev->no_tx_req_used++;
					spin_unlock(&dev->tx_req_lock);
					//net->trans_start = jiffies;
				}
			} else {
				spin_lock(&dev->tx_req_lock);
				list_add_tail(&new_req->list, &dev->tx_reqs);
				spin_unlock(&dev->tx_req_lock);
			}
		} else {
			spin_unlock(&dev->tx_req_lock);
		}
	} else {
		spin_unlock(&dev->tx_req_lock);
		dev_kfree_skb_any(skb);

	}

	atomic_dec(&dev->tx_qlen);
	if (netif_carrier_ok(dev->net))
		netif_wake_queue(dev->net);
}

netdev_tx_t eth_start_xmit_ncm(struct sk_buff *skb,
					struct net_device *net)
{
	struct eth_dev		*dev = netdev_priv(net);
	int			length = 0;
	int			retval;
	struct usb_request	*req = NULL;
	unsigned long		flags;
	struct usb_ep		*in;
	u16			cdc_filter;
	int added_offset = 0;

	/* fix prevent */
	if (!skb && !dev->port_usb)
		return NETDEV_TX_BUSY;

	spin_lock_irqsave(&dev->lock, flags);
	if (dev->port_usb) {
		in = dev->port_usb->in_ep;
		cdc_filter = dev->port_usb->cdc_filter;
	} else {
		in = NULL;
		cdc_filter = 0;
	}
	spin_unlock_irqrestore(&dev->lock, flags);

	if (skb && !in) {
		dev_kfree_skb_any(skb);
		return NETDEV_TX_OK;
	}

	/* apply outgoing CDC or RNDIS filters */
	if (skb && !is_promisc(cdc_filter)) {
		u8		*dest = skb->data;

		if (is_multicast_ether_addr(dest)) {
			u16	type;

			/* ignores USB_CDC_PACKET_TYPE_MULTICAST and host
			 * SET_ETHERNET_MULTICAST_FILTERS requests
			 */
			if (is_broadcast_ether_addr(dest))
				type = USB_CDC_PACKET_TYPE_BROADCAST;
			else
				type = USB_CDC_PACKET_TYPE_ALL_MULTICAST;
			if (!(cdc_filter & type)) {
				dev_kfree_skb_any(skb);
				return NETDEV_TX_OK;
			}
		}
		/* ignores USB_CDC_PACKET_TYPE_DIRECTED */
	}
	spin_lock_irqsave(&dev->tx_req_lock, flags);
	/*
	 * this freelist can be empty if an interrupt triggered disconnect()
	 * and reconfigured the gadget (shutting down this queue) after the
	 * network stack decided to xmit but before we got the spinlock.
	 */
	if (list_empty(&dev->tx_reqs)) {
		spin_unlock_irqrestore(&dev->tx_req_lock, flags);
		return NETDEV_TX_BUSY;
	}

	req = container_of(dev->tx_reqs.next, struct usb_request, list);
	list_del(&req->list);

	/* temporarily stop TX queue when the freelist empties */
	if (list_empty(&dev->tx_reqs)) {
		dev->net->stats.tx_dropped++;
		netif_stop_queue(net);
	}
	spin_unlock_irqrestore(&dev->tx_req_lock, flags);

	/* no buffer copies needed, unless the network stack did it
	 * or the hardware can't use skb buffers.
	 * or there's not enough space for extra headers we need
	 */
	if (dev->wrap) {
		unsigned long	flags;

		spin_lock_irqsave(&dev->lock, flags);
		if (dev->port_usb) {
			skb = dev->wrap(dev->port_usb, skb);
		}

		dev->tx_skb_hold_count = 0;
		spin_unlock_irqrestore(&dev->lock, flags);
		if (!skb) {
			/* Multi frame CDC protocols may store the frame for
			 * later which is not a dropped frame.
			 */
			if (dev->port_usb->supports_multi_frame)
				goto multiframe;
			goto drop;
		}
	}

	if (req->func_flag == NCM_ADD_HEADER) {
		spin_lock_irqsave(&dev->lock, flags);
		added_offset = NCM_HEADER_SIZE;
		dev->tx_skb_hold_count = 0;
		ncm_add_header(added_offset, req->buf, skb->len);
		spin_unlock_irqrestore(&dev->lock, flags);
	}

	if (dev->port_usb->multi_pkt_xfer) {
		int skb_len;

		/* Copy received IP data from SKB */
		memcpy(req->buf + req->length + added_offset, skb->data, skb->len);

		/* Increment req length by skb data length */
		req->length = req->length + skb->len + added_offset;
		skb_len = skb->len;
		length = req->length;
		dev_kfree_skb_any(skb);
		req->context = NULL;

		spin_lock_irqsave(&dev->tx_req_lock, flags);

		if (req->func_flag == NCM_ADD_DGRAM) {
			ncm_add_datagram(dev->port_usb, (__le16 *)(req->buf),
					skb_len, dev->tx_skb_hold_count);
		}
		dev->tx_skb_hold_count++;

		if (dev->tx_skb_hold_count < dev->dl_max_pkts_per_xfer) {
			req->func_flag = NCM_ADD_DGRAM;
			if (dev->no_tx_req_used > TX_REQ_THRESHOLD) {
				list_add(&req->list, &dev->tx_reqs);
				spin_unlock_irqrestore(&dev->tx_req_lock, flags);
				goto success;
			}
		}
		req->func_flag = NCM_ADD_HEADER;

		dev->no_tx_req_used++;
		dev->tx_skb_hold_count = 0;
		spin_unlock_irqrestore(&dev->tx_req_lock, flags);

	} else {
		length = skb->len;
		req->buf = skb->data;
		req->context = skb;
	}
	req->complete = tx_complete_ncm;

	req->zero = 0;
	if ((length % in->maxpacket) == 0) {
		req->zero = 1;
		dev->no_of_zlp++;
	}

	/* NCM requires no zlp if transfer is dwNtbInMaxSize */
	if (dev->port_usb) {
		if (dev->port_usb->is_fixed) {
			if (length == dev->port_usb->fixed_in_len) {
				req->zero = 0;
				dev->no_of_zlp--;
			}
		}
	}

	/* use zlp framing on tx for strict CDC-Ether conformance,
	 * though any robust network rx path ignores extra padding.
	 * and some hardware doesn't like to write zlps.
	 */
	if (req->zero && !dev->zlp && (length % in->maxpacket) == 0) {
		length++;
	}

	req->length = length;

	netdev_sent_queue(dev->net, length);

#ifdef HS_THROTTLE_IRQ
	/* throttle highspeed IRQ rate back slightly */
	if (gadget_is_dualspeed(dev->gadget) &&
			 (dev->gadget->speed == USB_SPEED_HIGH)) {
		atomic_inc(&dev->tx_qlen);
		if (atomic_read(&dev->tx_qlen) == (dev->qmult/2)) {
			req->no_interrupt = 0;
			atomic_set(&dev->tx_qlen, 0);
		} else {
			req->no_interrupt = 1;
		}
	} else {
		req->no_interrupt = 0;
	}
#else
	atomic_inc(&dev->tx_qlen);
#endif
	retval = usb_ep_queue(in, req, GFP_ATOMIC);

	if (retval) {
		if (!dev->port_usb->multi_pkt_xfer)
			dev_kfree_skb_any(skb);
drop:
		dev->net->stats.tx_dropped++;
		dev->no_tx_req_used--;
		req->length = 0;
multiframe:
		spin_lock_irqsave(&dev->tx_req_lock, flags);
		if (list_empty(&dev->tx_reqs))
			netif_start_queue(net);
		list_add(&req->list, &dev->tx_reqs);
		spin_unlock_irqrestore(&dev->tx_req_lock, flags);
	}
success:
	return NETDEV_TX_OK;
}
#endif

