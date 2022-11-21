/*
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * (C) Copyright 2006 Marvell International Ltd.
 * All Rights Reserved
 */
#include <linux/version.h>
#include <linux/kernel.h>
#include <linux/jiffies.h>
#include <linux/module.h>
#include <linux/interrupt.h>
#include <linux/fs.h>
#include <linux/types.h>
#include <linux/string.h>
#include <linux/socket.h>
#include <linux/errno.h>
#include <linux/fcntl.h>
#include <linux/in.h>
#include <linux/init.h>
#include <linux/miscdevice.h>
#include <linux/platform_device.h>

#include <asm/system.h>
#include <asm/uaccess.h>
#include <asm/io.h>

#include <linux/inet.h>
#include <linux/netdevice.h>
#include <linux/etherdevice.h>
#include <linux/skbuff.h>
#include <linux/ethtool.h>
#include <net/sock.h>
#include <net/checksum.h>
#include <linux/if_ether.h>
#include <linux/if_arp.h>
#include <linux/ip.h>
#include <linux/tcp.h>
#include <linux/percpu.h>
#include <linux/cdev.h>
#include "common_datastub.h"
#include <linux/platform_device.h>
#include "data_channel_kernel.h"
#include <linux/kthread.h>

#define MAX_CID_NUM    8

struct ccinet_priv {
	unsigned char cid;
	int status;                                             /* indicate status of the interrupt */
	spinlock_t lock;                                        /* spinlock use to protect critical session	*/
	struct  net_device_stats net_stats;                     /* status of the network device	*/
};
static struct net_device *net_devices[MAX_CID_NUM];

#ifdef DATA_IND_BUFFERLIST
struct ccirx_packet {
	char  *pktdata;
	int length;
	unsigned char cid;
};

typedef struct _cci_rxind_buf_list
{
	struct ccirx_packet rx_packet;
	struct _cci_rxind_buf_list *next;
}RXINDBUFNODE;

//static RXINDBUFNODE *rxbuflist = NULL;
struct completion dataChanRxFlagRef;
#define rxDataMask 0x0010
struct task_struct *cinetDataRcvTaskRef;
#endif


#if  1
#define DPRINT(fmt, args ...)     printk(fmt, ## args)
#define DBGMSG(fmt, args ...)     printk(KERN_DEBUG "CIN: " fmt, ## args)
#define ENTER()                 printk(KERN_DEBUG "CIN: ENTER %s\n", __FUNCTION__)
#define LEAVE()                 printk(KERN_DEBUG "CIN: LEAVE %s\n", __FUNCTION__)
#define FUNC_EXIT()                     printk(KERN_DEBUG "CIN: EXIT %s\n", __FUNCTION__)
#define ASSERT(a)  if (!(a)) { \
		while (1) { \
			printk(KERN_ERR "ASSERT FAIL AT FILE %s FUNC %s LINE %d\n", __FILE__, __FUNCTION__, __LINE__); \
		} \
}

#else
#define DPRINT(fmt, args ...)     printk("CIN: " fmt, ## args)
#define DBGMSG(fmt, args ...)     do {} while (0)
#define ENTER()                 do {} while (0)
#define LEAVE()                 do {} while (0)
#define FUNC_EXIT()                     do {} while (0)
#define ASSERT(a) if (!(a)) { \
		while (1) { \
			printk(KERN_CRIT "ASSERT FAIL AT FILE %s FUNC %s LINE %d\n", __FILE__, __FUNCTION__, __LINE__); \
		} \
}
#endif


///////////////////////////////////////////////////////////////////////////////////////
// Network Operations
///////////////////////////////////////////////////////////////////////////////////////
static int ccinet_open(struct net_device* netdev)
{
	ENTER();

//	netif_carrier_on(netdev);
	netif_start_queue(netdev);
	LEAVE();
	return 0;
}

static int ccinet_stop(struct net_device* netdev)
{
	ENTER();
	struct ccinet_priv* devobj = netdev_priv(netdev);
	netif_stop_queue(netdev);
//	netif_carrier_off(netdev);
	memset(&devobj->net_stats, 0, sizeof(devobj->net_stats));
	LEAVE();
	return 0;
}

extern void sendPSDData(int cid, struct sk_buff *skb);
static int ccinet_tx(struct sk_buff* skb, struct net_device* netdev)
{
	struct ccinet_priv* devobj = netdev_priv(netdev);
	netdev->trans_start = jiffies;

	/* strip Ethernet header */
	skb_pull(skb, ETH_HLEN);

	sendPSDData(devobj->cid, skb);
	 
	/* update network statistics */
	devobj->net_stats.tx_packets++;
	devobj->net_stats.tx_bytes += skb->len;

	return 0;

}

static void ccinet_tx_timeout(struct net_device* netdev)
{
	struct ccinet_priv* devobj = netdev_priv(netdev);

	ENTER();
	devobj->net_stats.tx_errors++;
//	netif_wake_queue(netdev); // Resume tx
	return;
}

static struct net_device_stats *ccinet_get_stats(struct net_device *dev)
{
	struct ccinet_priv* devobj;

	devobj = netdev_priv(dev);
	ASSERT(devobj);
	return &devobj->net_stats;
}

static int ccinet_rx(struct net_device* netdev, char* packet, int pktlen)
{

	struct sk_buff *skb;
	struct ccinet_priv *priv = netdev_priv(netdev);
	struct iphdr* ip_header = (struct iphdr*)packet;
	struct ethhdr ether_header;

	if (ip_header->version == 4) {
		ether_header.h_proto = htons(ETH_P_IP);
	} else if (ip_header->version == 6) {
		ether_header.h_proto = htons(ETH_P_IPV6);
	} else {
		printk(KERN_ERR "ccinet_rx: invalid ip version: %d\n", ip_header->version);
		priv->net_stats.rx_dropped++;
		goto out;
	}
	memcpy(ether_header.h_dest, netdev->dev_addr, ETH_ALEN);
	memset(ether_header.h_source, 0, ETH_ALEN);

	//ENTER();
	//DBGMSG("ccinet_rx:pktlen=%d\n", pktlen);
	skb = dev_alloc_skb(pktlen + NET_IP_ALIGN + sizeof(ether_header));
	ASSERT(skb);

	if (!skb)
	{

		if (printk_ratelimit(  ))

			printk(KERN_NOTICE "ccinet_rx: low on mem - packet dropped\n");

		priv->net_stats.rx_dropped++;

		goto out;

	}
	skb_reserve(skb, NET_IP_ALIGN);
	memcpy(skb_put(skb, sizeof(ether_header)), &ether_header, sizeof(ether_header));

	memcpy(skb_put(skb, pktlen), packet, pktlen);

	/* Write metadata, and then pass to the receive level */

	skb->dev = netdev;
	skb->protocol = eth_type_trans(skb, netdev); //htons(ETH_P_IP);//eth_type_trans(skb, netdev);
	skb->ip_summed = CHECKSUM_NONE; /* don't check it */
	priv->net_stats.rx_packets++;
	priv->net_stats.rx_bytes += pktlen + sizeof(ether_header);
	netif_rx(skb);
	//where to free skb?

	return 0;

 out:
	return -1;
}

static int validate_addr(struct net_device* netdev)
{
	ENTER();
	return 0;
}

#ifdef DATA_IND_BUFFERLIST
void addrxnode(RXINDBUFNODE *newBufNode)
{
	RXINDBUFNODE *pCurrNode;

	if (rxbuflist == NULL)
	{
		rxbuflist = newBufNode;
	}
	else
	{
		pCurrNode = rxbuflist;
		while (pCurrNode->next != NULL)
			pCurrNode = pCurrNode->next;

		pCurrNode->next = newBufNode;

	}
	return;
}

int sendDataIndtoInternalList(unsigned char cid, char* buf, int len)
{
	RXINDBUFNODE* rxnode=NULL;
	UINT32 flag;
	int ret=0;

	rxnode = kmalloc(sizeof(RXINDBUFNODE), GFP_KERNEL);
	if (!rxnode) {
		ret = -ENOMEM;
		goto err;
	}
	rxnode->rx_packet.pktdata = kmalloc(len, GFP_KERNEL);
	if (!(rxnode->rx_packet.pktdata)) {
		ret = -ENOMEM;
		goto err;
	}
	memcpy(rxnode->rx_packet.pktdata, buf, len);
	rxnode->rx_packet.length = len;
	rxnode->rx_packet.cid = cid;
	rxnode->next = NULL;
	addrxnode(rxnode);
#if 0
	OSAFlagPeek(dataChanRxFlagRef, &flag);

	if (!(flag & rxDataMask))
	{
		OSAFlagSet(dataChanRxFlagRef, rxDataMask, OSA_FLAG_OR);
	}
#endif
	complete(&dataChanRxFlagRef);
	return 0;

err:
	if (rxnode)
		free(rxnode);
	return ret;
}
void recvDataOptTask(void *data)
{
	CINETDEVLIST *pdrv;
	int err;
	RXINDBUFNODE *tmpnode;
	UINT32 flags;
	OS_STATUS osaStatus;

	//while(!kthread_should_stop())
	while (1)
	{

		if (rxbuflist != NULL)
		{
			err = search_list_by_cid(rxbuflist->rx_packet.cid, &pdrv);
			ASSERT(err == 0);

			ccinet_rx(pdrv->ccinet, rxbuflist->rx_packet.pktdata, rxbuflist->rx_packet.length);
			tmpnode = rxbuflist;
			rxbuflist = rxbuflist->next;
			kfree(tmpnode->rx_packet.pktdata);
			kfree(tmpnode);

		}
		else
		{
			wait_for_completion_interruptible(&dataChanRxFlagRef);
		}
	}

}
void initDataChanRecv()
{

	ENTER();

	init_completion(&dataChanRxFlagRef);

	cinetDataRcvTaskRef = kthread_run(recvDataOptTask, NULL, "recvDataOptTask");
	LEAVE();
}

#endif

static int data_rx(char* packet, int len, unsigned char cid)
{
	if (cid >= MAX_CID_NUM)
		return -1;
#ifndef DATA_IND_BUFFERLIST
	ccinet_rx(net_devices[cid], packet, len);
#else
	int err = sendDataIndtoInternalList(cid, packet, len);
	if (err != 0)
		return -1;
#endif
	return len;
}

///////////////////////////////////////////////////////////////////////////////////////
// Initialization
///////////////////////////////////////////////////////////////////////////////////////

static const struct net_device_ops cci_netdev_ops = {
	.ndo_open		= ccinet_open,
	.ndo_stop		= ccinet_stop,
	.ndo_start_xmit 	= ccinet_tx,
	.ndo_tx_timeout		= ccinet_tx_timeout,
	.ndo_get_stats 	= ccinet_get_stats,
	.ndo_validate_addr	= validate_addr
};

static void ccinet_setup(struct net_device* netdev)
{
	ENTER();
	ether_setup(netdev);
	netdev->netdev_ops = &cci_netdev_ops;

	netdev->watchdog_timeo = 5;  //jiffies
	netdev->flags |= IFF_NOARP;
	random_ether_addr(netdev->dev_addr);
	LEAVE();
}

static int __init ccinet_init(void)
{
	int i;
	for (i = 0; i < MAX_CID_NUM; i++) {
		char ifname[32];
		struct net_device *dev;
		struct ccinet_priv *priv;
		int ret;

		sprintf(ifname, "rmnet%d", i);
		dev = alloc_netdev(sizeof(struct ccinet_priv), ifname, ccinet_setup);

		if (!dev) {
			printk(KERN_ERR "%s: alloc_netdev for %s fail\n", __FUNCTION__, ifname);
			return -ENOMEM;
		}
		ret = register_netdev(dev);
		if (ret) {
			printk(KERN_ERR "%s: register_netdev for %s fail\n", __FUNCTION__, ifname);
			free_netdev(dev);
			return ret;
		}
		priv = netdev_priv(dev);
		memset(priv, 0, sizeof(struct ccinet_priv));
		spin_lock_init(&priv->lock);
		priv->cid = i;
		net_devices[i] = dev;
	}

	registerRxCallBack(PDP_DIRECTIP, data_rx);

#ifdef  DATA_IND_BUFFERLIST
	initDataChanRecv();
#endif
	return 0;
};

static void __exit ccinet_exit(void)
{
	int i;
	for (i = 0; i < MAX_CID_NUM; i++) {
		unregister_netdev(net_devices[i]);
		free_netdev(net_devices[i]);
		net_devices[i] = NULL;
	}
#ifdef  DATA_IND_BUFFERLIST
	kthread_stop(cinetDataRcvTaskRef);
#endif

}

module_init(ccinet_init);
module_exit(ccinet_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Marvell");
MODULE_DESCRIPTION("Marvell CI Network Driver");

