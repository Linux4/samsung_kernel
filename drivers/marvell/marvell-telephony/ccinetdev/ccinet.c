/*
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * (C) Copyright 2006 Marvell International Ltd.
 * All Rights Reserved
 */

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/errno.h>
#include <linux/netdevice.h>
#include <linux/skbuff.h>
#include <linux/if_arp.h>
#include <linux/ip.h>
#include <linux/debugfs.h>

#include <net/checksum.h>

#include "common_datastub.h"
#include "data_channel_kernel.h"
#include "psd_data_channel.h"
#include "debugfs.h"

#define PERSIST_DEVICE_NUM    8

#define CCINET_IOCTL_AQUIRE SIOCDEVPRIVATE
#define CCINET_IOCTL_RELEASE (SIOCDEVPRIVATE+1)
#define CCINET_IOCTL_SET_V6_CID (SIOCDEVPRIVATE+2)
#define CCINET_IOCTL_SET_EMBMS_CID (SIOCDEVPRIVATE+3)
#define CCINET_IOCTL_IF_ENABLE (SIOCDEVPRIVATE+4)
#define CCINET_IOCTL_IF_DISABLE (SIOCDEVPRIVATE+5)

struct ccinet_priv {
	struct psd_user psd_user;
	unsigned char is_used;
	unsigned char sim_id;
	unsigned char cid;
	signed char v6_cid;
	spinlock_t lock;	/* use to protect critical session */
	struct net_device_stats net_stats; /* status of the network device */
};
static struct net_device *net_devices[MAX_CID_NUM];

static u32 checksum_enable = true;

static void ccinet_setup(struct net_device *netdev);

/**********************************************************/
/* Network Operations */
/**********************************************************/
static int ccinet_open(struct net_device *netdev)
{
	struct ccinet_priv *devobj;
	int rc;

	devobj = netdev_priv(netdev);
	rc = psd_register(&devobj->psd_user, devobj->cid);
	if (rc == 0) {
		/* netif_carrier_on(netdev); */
		netif_start_queue(netdev);
	}
	return rc;
}

static int ccinet_stop(struct net_device *netdev)
{
	struct ccinet_priv *devobj = netdev_priv(netdev);
	netif_stop_queue(netdev);
	/* netif_carrier_off(netdev); */
	memset(&devobj->net_stats, 0, sizeof(devobj->net_stats));
	if (devobj->v6_cid > -1) {
		psd_unregister(&devobj->psd_user, devobj->v6_cid);
		devobj->v6_cid = -1;
	}
	psd_unregister(&devobj->psd_user, devobj->cid);
	return 0;
}

static netdev_tx_t ccinet_tx(struct sk_buff *skb, struct net_device *netdev)
{
	struct ccinet_priv *devobj = netdev_priv(netdev);
	int cid = devobj->cid;
	struct iphdr *ip_header = (struct iphdr *)ip_hdr(skb);
	int ret;

	if (ip_header->version == 6) {
		if (devobj->v6_cid > -1)
			cid = devobj->v6_cid;
	}
	netdev->trans_start = jiffies;

	if (skb->tstamp.tv64 == 0)
		__net_timestamp(skb);

	ret = sendPSDData(cid | devobj->sim_id << 31, skb);
	if (ret == PSD_DATA_SEND_BUSY) {
		return NETDEV_TX_BUSY;
	} else if (ret == PSD_DATA_SEND_DROP) {
		devobj->net_stats.tx_dropped++;
	} else {
		devobj->net_stats.tx_packets++;
		devobj->net_stats.tx_bytes += skb->len;
	}
	return NETDEV_TX_OK;

}

static void ccinet_fc_cb(void *user_obj, bool is_throttle)
{
	struct net_device *dev = (struct net_device *)user_obj;

	netdev_notice(dev, "ccinet_fc_cb(is_throttle:%d)\n", is_throttle);
	if (is_throttle)
		netif_stop_queue(dev);
	else
		netif_wake_queue(dev);
}

static void ccinet_tx_timeout(struct net_device *netdev)
{
	struct ccinet_priv *devobj = netdev_priv(netdev);

	netdev_warn(netdev, "%s\n", __func__);
	devobj->net_stats.tx_errors++;
	/* netif_wake_queue(netdev); */   /* Resume tx */
	return;
}

static struct net_device_stats *ccinet_get_stats(struct net_device *dev)
{
	struct ccinet_priv *devobj;

	devobj = netdev_priv(dev);
	return &devobj->net_stats;
}

/*
 * extra header size need to be reserved to avoid memory copying
 * in RNDIS driver and WiFi driver
 */
#define CCINET_RESERVE_HDR_LEN (192)

static int ccinet_rx(void *user_obj, const unsigned char *packet,
		unsigned int pktlen)
{
	struct net_device *netdev = (struct net_device *)user_obj;
	struct sk_buff *skb;
	struct ccinet_priv *priv = netdev_priv(netdev);
	struct iphdr *ip_header = (struct iphdr *)packet;
	__be16	protocol;

	if (ip_header->version == 4) {
		protocol = htons(ETH_P_IP);
	} else if (ip_header->version == 6) {
		protocol = htons(ETH_P_IPV6);
	} else {
		netdev_err(netdev, "ccinet_rx: invalid ip version: %d\n",
		       ip_header->version);
		priv->net_stats.rx_dropped++;
		goto out;
	}

	skb = dev_alloc_skb(pktlen + CCINET_RESERVE_HDR_LEN);
	if (!skb) {
		pr_notice_ratelimited(
			"ccinet_rx: low on mem - packet dropped\n");

		priv->net_stats.rx_dropped++;

		goto out;

	}
	skb_reserve(skb, CCINET_RESERVE_HDR_LEN);
	memcpy(skb_put(skb, pktlen), packet, pktlen);

	/* Write metadata, and then pass to the receive level */

	skb->dev = netdev;
	skb->protocol = protocol;
	if (!checksum_enable)
		skb->ip_summed = CHECKSUM_UNNECESSARY;	/* don't check it */
	if (netif_rx(skb) == NET_RX_SUCCESS) {
		priv->net_stats.rx_packets++;
		priv->net_stats.rx_bytes += pktlen;
	} else {
		priv->net_stats.rx_dropped++;
		pr_notice_ratelimited(
			"ccinet_rx: packet dropped by netif_rx\n");
		goto out;
	}

	return 0;

out:
	return -1;
}

static int create_ccinet_netdev(unsigned char cid, bool locked)
{
	char ifname[32];
	struct net_device *dev;
	struct ccinet_priv *priv;
	int ret;

	if (net_devices[cid])
		return -EEXIST;
#ifdef	CONFIG_SSIPC_SUPPORT
	sprintf(ifname, "rmnet%d", cid);
#else
	sprintf(ifname, "ccinet%d", cid);
#endif
	dev =
	    alloc_netdev(sizeof(struct ccinet_priv), ifname,
			 ccinet_setup);
	if (!dev) {
		pr_err("%s: alloc_netdev for %s fail\n",
		       __func__, ifname);
		return -ENOMEM;
	}
	ret = locked ? register_netdevice(dev) : register_netdev(dev);
	if (ret) {
		pr_err("%s: register_netdev for %s fail\n",
		       __func__, ifname);
		free_netdev(dev);
		return ret;
	}
	priv = netdev_priv(dev);
	memset(priv, 0, sizeof(struct ccinet_priv));
	spin_lock_init(&priv->lock);
	priv->psd_user.priv = dev;
	priv->psd_user.on_receive = ccinet_rx;
	priv->psd_user.on_throttle = ccinet_fc_cb;
	priv->cid = cid;
	priv->v6_cid = -1;
	net_devices[cid] = dev;
	return 0;
}

static int destroy_ccinet_netdev(int cid, bool locked)
{
	struct net_device *dev;

	dev = net_devices[cid];
	if (!dev)
		return -ENOENT;

	if (dev->flags & IFF_UP)
		ccinet_stop(dev);

	if (locked)
		unregister_netdevice(dev);
	else
		unregister_netdev(dev);
	net_devices[cid] = NULL;

	return 0;
}

static int ccinet_ioctl(struct net_device *netdev, struct ifreq *rq, int cmd)
{
	int err = 0;
	struct ccinet_priv *priv = netdev_priv(netdev);
	switch (cmd) {
	case CCINET_IOCTL_AQUIRE:
	{
		int sim_id = rq->ifr_ifru.ifru_ivalue;
		spin_lock(&priv->lock);
		if (priv->is_used) {
			netdev_err(netdev,
				"%s: SIM%d has used cid%d, SIM%d request fail\n",
				__func__, (int)priv->sim_id + 1, (int)priv->cid,
				sim_id + 1);
			err = -EFAULT;
		} else {
			priv->sim_id = sim_id;
			priv->is_used = 1;
			netdev_info(netdev, "%s: SIM%d now use cid%d\n",
				__func__, sim_id + 1, (int)priv->cid);
		}
		spin_unlock(&priv->lock);
		break;
	}
	case CCINET_IOCTL_RELEASE:
	{
		int sim_id = rq->ifr_ifru.ifru_ivalue;
		spin_lock(&priv->lock);
		if (priv->is_used) {
			if (sim_id != priv->sim_id) {
				spin_unlock(&priv->lock);
				netdev_err(netdev,
					"%s: SIM%d want release cid%d, but it's used by SIM%d\n",
					__func__, sim_id + 1, priv->cid,
					(int)priv->sim_id + 1);
				err = -EFAULT;
			} else {
				priv->sim_id = 0;
				priv->is_used = 0;
				spin_unlock(&priv->lock);
				netdev_info(netdev,
					"%s: SIM%d now release cid%d\n",
					__func__, sim_id + 1, (int)priv->cid);
			}
		} else {
			spin_unlock(&priv->lock);
			netdev_warn(netdev,
				"%s: SIM%d want release cid%d, but not used before\n",
				__func__, sim_id + 1, priv->cid);
		}
		break;
	}
	case CCINET_IOCTL_SET_V6_CID:
	{
		int v6_cid = rq->ifr_ifru.ifru_ivalue;
		if (v6_cid < 0 || v6_cid >= MAX_CID_NUM) {
			err = -EINVAL;
			break;
		}
		priv->v6_cid = v6_cid;
		err = psd_register(&priv->psd_user, priv->v6_cid);
		break;
	}
	case CCINET_IOCTL_SET_EMBMS_CID:
	{
		set_embms_cid(rq->ifr_ifru.ifru_ivalue);
		break;
	}
	case CCINET_IOCTL_IF_ENABLE:
	{
		int cid;
		cid = rq->ifr_ifru.ifru_ivalue;
		if (cid < PERSIST_DEVICE_NUM || cid >= MAX_CID_NUM) {
			err = -EINVAL;
			break;
		}
		err = create_ccinet_netdev((unsigned char)cid, true);
		break;
	}
	case CCINET_IOCTL_IF_DISABLE:
	{
		int cid;
		cid = rq->ifr_ifru.ifru_ivalue;
		if (cid < PERSIST_DEVICE_NUM || cid >= MAX_CID_NUM) {
			err = -EINVAL;
			break;
		}
		destroy_ccinet_netdev((unsigned char)cid, true);
		break;
	}

	default:
		err = -EOPNOTSUPP;
		break;
	}
	return err;
}

/*************************************************************************/
/* Initialization */
/*************************************************************************/

static const struct net_device_ops cci_netdev_ops = {
	.ndo_open		= ccinet_open,
	.ndo_stop		= ccinet_stop,
	.ndo_start_xmit	= ccinet_tx,
	.ndo_tx_timeout		= ccinet_tx_timeout,
	.ndo_get_stats	= ccinet_get_stats,
	.ndo_do_ioctl = ccinet_ioctl
};

static void ccinet_setup(struct net_device *netdev)
{
	netdev->netdev_ops	= &cci_netdev_ops;
	netdev->type		= ARPHRD_VOID;
	netdev->mtu		= 1500;
	netdev->addr_len	= 0;
	netdev->tx_queue_len	= 1000;
	netdev->flags		= IFF_NOARP;
	netdev->hard_header_len	= 16;
	netdev->priv_flags	&= ~IFF_XMIT_DST_RELEASE;
	netdev->destructor	= free_netdev;
}

static struct dentry *tel_debugfs_root_dir;
static struct dentry *ccinet_debugfs_root_dir;

static int ccinet_debugfs_init(void)
{
	tel_debugfs_root_dir = tel_debugfs_get();
	if (!tel_debugfs_root_dir)
		return -ENOMEM;

	ccinet_debugfs_root_dir = debugfs_create_dir("ccinet", tel_debugfs_root_dir);
	if (IS_ERR_OR_NULL(ccinet_debugfs_root_dir))
		goto putrootfs;

	if (IS_ERR_OR_NULL(debugfs_create_bool("checksum_enable", S_IRUGO | S_IWUSR,
			ccinet_debugfs_root_dir, &checksum_enable)))
		goto error;

	return 0;

error:
	debugfs_remove_recursive(ccinet_debugfs_root_dir);
	ccinet_debugfs_root_dir = NULL;
putrootfs:
	tel_debugfs_put(tel_debugfs_root_dir);
	tel_debugfs_root_dir = NULL;
	return -1;
}

static int ccinet_debugfs_exit(void)
{
	debugfs_remove_recursive(ccinet_debugfs_root_dir);
	ccinet_debugfs_root_dir = NULL;
	tel_debugfs_put(tel_debugfs_root_dir);
	tel_debugfs_root_dir = NULL;
	return 0;
}

static int __init ccinet_init(void)
{
	int i, ret;
	for (i = 0; i < PERSIST_DEVICE_NUM; i++) {
		ret = create_ccinet_netdev(i, false);
		if (ret < 0)
			return ret;
	}
	ccinet_debugfs_init();
	return 0;
};

static void __exit ccinet_exit(void)
{
	int i;
	ccinet_debugfs_exit();
	for (i = 0; i < MAX_CID_NUM; i++) {
		destroy_ccinet_netdev(i, false);
	}
}

module_init(ccinet_init);
module_exit(ccinet_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Marvell");
MODULE_DESCRIPTION("Marvell CI Network Driver");
