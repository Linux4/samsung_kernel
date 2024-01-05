/*
 * Copyright (C) 2013 Spreadtrum Communications Inc.
 *
 * Authors:
 * Keguang Zhang <keguang.zhang@spreadtrum.com>
 * Wenjie Zhang <wenjie.zhang@spreadtrum.com>
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/string.h>
#include <linux/delay.h>
#include <linux/errno.h>
#include <linux/interrupt.h>
#include <linux/init.h>
#include <linux/netdevice.h>
#include <linux/skbuff.h>
#include <linux/wakelock.h>
#include <linux/workqueue.h>
#include <linux/ipv6.h>
#include <linux/ip.h>
#include <linux/inetdevice.h>
#include <asm/byteorder.h>
#include <linux/platform_device.h>
#include <linux/suspend.h>
#if defined(CONFIG_HAS_EARLYSUSPEND) && defined(CONFIG_ITM_WLAN_ENHANCED_PM)
#include <linux/earlysuspend.h>
#endif

#include <linux/sipc.h>
#include <linux/atomic.h>

#include <linux/sprd_2351.h>

#include "ittiam.h"
#include "sipc.h"
#include "cfg80211.h"
#include "npi.h"
#include "wapi.h"

#define ITM_DEV_NAME		"itm_wlan"
#define ITM_INTF_NAME		"wlan%d"

#define SETH_RESEND_MAX_NUM	10
#define SIOGETSSID 0x89F2

void ittiam_nvm_init(void);

/*
 * Tx_ready handler.
 */
static void itm_wlan_tx_ready_handler(struct itm_priv *priv)
{
	if (!netif_carrier_ok(priv->ndev)) {
		dev_dbg(&priv->ndev->dev, "netif_carrier_on\n");
		netif_carrier_on(priv->ndev);
	}
}

/*
 * Tx_open handler.
 */
static void itm_wlan_tx_open_handler(struct itm_priv *priv)
{
	if (!netif_carrier_ok(priv->ndev)) {
		dev_dbg(&priv->ndev->dev, "netif_carrier_on\n");
		netif_carrier_on(priv->ndev);
	}
}

/*
 * Tx_close handler.
 */
static void itm_wlan_tx_close_handler(struct itm_priv *priv)
{
	if (netif_carrier_ok(priv->ndev)) {
		dev_dbg(&priv->ndev->dev, "netif_carrier_off\n");
		netif_carrier_off(priv->ndev);
	}
}

static int itm_wlan_rx_handler(struct napi_struct *napi, int budget)
{
	struct itm_priv *priv = container_of(napi, struct itm_priv, napi);
	struct sblock blk;
	struct sk_buff *skb;
	int ret, work_done;
	u16 decryp_data_len = 0;
	struct wlan_sblock_recv_data *data;

	for (work_done = 0; work_done < budget; work_done++) {
		ret = sblock_receive(WLAN_CP_ID, WLAN_SBLOCK_CH, &blk, 0);
		if (ret) {
			dev_dbg(&priv->ndev->dev, "no more sblock to read\n");
			break;
		}

		/*16 bytes align */
		skb = dev_alloc_skb(blk.length + NET_IP_ALIGN);
		if (!skb) {
			dev_err(&priv->ndev->dev,
				"Failed to allocate skbuff!\n");
			priv->ndev->stats.rx_dropped++;
			goto rx_failed;
		}

		data = (struct wlan_sblock_recv_data *)blk.addr;
		if (data->is_encrypted == 1) {
			if (priv->connect_status == ITM_CONNECTED &&
			    priv->cipher_type == WAPI &&
			    priv->key_len[GROUP][priv->key_index[GROUP]] != 0 &&
			    priv->key_len[PAIRWISE][priv->
						    key_index[PAIRWISE]] != 0) {
				u8 snap_header[6] = {0xaa, 0xaa, 0x03,
						     0x00, 0x00, 0x00};
				skb_reserve(skb, NET_IP_ALIGN);
				decryp_data_len = wlan_rx_wapi_decryption(priv,
						   (u8 *)&data->u2.encrypt,
						   data->u1.encrypt.header_len,
						   (blk.length -
						   sizeof(data->is_encrypted) -
						   sizeof(data->u1) -
						   data->u1.encrypt.header_len),
						   (skb->data + 12));
				if (decryp_data_len == 0) {
					dev_err(&priv->ndev->dev,
						"wapi data decryption failed!\n");
					priv->ndev->stats.rx_dropped++;
					dev_kfree_skb(skb);
					goto rx_failed;
				}
				if (memcmp((skb->data + 12), snap_header,
					   sizeof(snap_header)) == 0) {
					skb_reserve(skb, 6);
					/* copy the eth address from eth header,
					 * but not copy eth type
					 */
					memcpy(skb->data,
					       data->u2.encrypt.
					       mac_header.addr1, 6);
					memcpy(skb->data + 6,
					       data->u2.encrypt.
					       mac_header.addr2, 6);
					skb_put(skb, (decryp_data_len + 6));
				} else {
					/* copy eth header */
					memcpy(skb->data,
					       data->u2.encrypt.
					       mac_header.addr3, 6);
					memcpy(skb->data + 6,
					       data->u2.encrypt.
					       mac_header.addr2, 6);
					skb_put(skb, (decryp_data_len + 12));
				}
			} else {
				dev_err(&priv->ndev->dev,
					"wrong encryption data!\n");
				priv->ndev->stats.rx_dropped++;
				dev_kfree_skb(skb);
				goto rx_failed;
			}
		} else if (data->is_encrypted == 0) {
			skb_reserve(skb, NET_IP_ALIGN);
			/* dec the first encrypt byte */
			memcpy(skb->data, (u8 *)&data->u2,
			       (blk.length - sizeof(data->is_encrypted) -
				sizeof(data->u1)));
			skb_put(skb,
				(blk.length - sizeof(data->is_encrypted) -
				 sizeof(data->u1)));
		} else {
			dev_err(&priv->ndev->dev,
				"wrong data fromat recieve!\n");
			priv->ndev->stats.rx_dropped++;
			dev_kfree_skb(skb);
			goto rx_failed;
		}

#ifdef DUMP_RECEIVE_PACKET
		print_hex_dump(KERN_DEBUG, "receive packet: ",
			       DUMP_PREFIX_OFFSET, 16, 1, skb->data, skb->len,
			       0);
#endif
		skb->dev = priv->ndev;
		skb->protocol = eth_type_trans(skb, priv->ndev);
		/*skb->ip_summed = CHECKSUM_UNNECESSARY; *//*not supported by our hardware */

		priv->ndev->stats.rx_packets++;
		priv->ndev->stats.rx_bytes += skb->len;

		/*netif_rx(skb);*/
		netif_receive_skb(skb);

rx_failed:
		ret = sblock_release(WLAN_CP_ID, WLAN_SBLOCK_CH, &blk);
		if (ret)
			dev_err(&priv->ndev->dev,
				"Failed to release sblock (%d)\n", ret);
	}
	if (work_done < budget) {
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(3, 10, 0))
		napi_gro_flush(napi, false);
#else
		napi_gro_flush(napi);
#endif
		__napi_complete(napi);
	}

	return work_done;
}

static void itm_wlan_handler(int event, void *data)
{
	struct itm_priv *priv = (struct itm_priv *)data;

	switch (event) {
	case SBLOCK_NOTIFY_GET:
		dev_dbg(&priv->ndev->dev, "SBLOCK_NOTIFY_GET is received\n");
		break;
	case SBLOCK_NOTIFY_RECV:
		dev_dbg(&priv->ndev->dev, "SBLOCK_NOTIFY_RECV is received\n");
		/*itm_wlan_rx_handler(priv);*/
		if (likely(napi_schedule_prep(&priv->napi)))
			__napi_schedule(&priv->napi);
		break;
	case SBLOCK_NOTIFY_STATUS:
		dev_dbg(&priv->ndev->dev, "SBLOCK_NOTIFY_STATUS is received\n");
		itm_wlan_tx_ready_handler(priv);
		break;
	case SBLOCK_NOTIFY_OPEN:
		dev_dbg(&priv->ndev->dev, "SBLOCK_NOTIFY_OPEN is received\n");
		itm_wlan_tx_open_handler(priv);
		break;
	case SBLOCK_NOTIFY_CLOSE:
		dev_dbg(&priv->ndev->dev, "SBLOCK_NOTIFY_CLOSE is received\n");
		itm_wlan_tx_close_handler(priv);
		break;
	default:
		dev_err(&priv->ndev->dev,
			"Received event is invalid(event=%d)\n", event);
	}
}

/*
 * Transmit interface
 */
static int itm_wlan_start_xmit(struct sk_buff *skb, struct net_device *dev)
{
	struct itm_priv *priv = netdev_priv(dev);
	struct sblock blk;
	int ret;
	/*
	 * Get a free sblock.
	 */
	ret = sblock_get(WLAN_CP_ID, WLAN_SBLOCK_CH, &blk, 0);
	if (ret) {
		dev_err(&dev->dev, "Failed to get free sblock (%d)\n", ret);
		netif_stop_queue(dev);
		priv->ndev->stats.tx_fifo_errors++;
		return NETDEV_TX_BUSY;
	}

	if (blk.length < skb->len) {
		dev_err(&dev->dev, "The size of sblock is so tiny!\n");
		priv->ndev->stats.tx_fifo_errors++;
		sblock_put(WLAN_CP_ID, WLAN_SBLOCK_CH, &blk);
		dev_kfree_skb_any(skb);
		priv->txrcnt = 0;
		return NETDEV_TX_OK;
	}

	if (priv->connect_status == ITM_CONNECTED &&
	    priv->cipher_type == WAPI &&
/*            priv->key_len[GROUP][priv->key_index[GROUP]] != 0 &&*/
	    priv->key_len[PAIRWISE][priv->key_index[PAIRWISE]] != 0 &&
	    (*(u16 *)((u8 *)skb->data + ETH_PKT_TYPE_OFFSET) != 0xb488)) {
		memcpy(((u8 *)blk.addr), skb->data, ETHERNET_HDR_LEN);
		blk.length = wlan_tx_wapi_encryption(priv,
					skb->data,
					(skb->len - ETHERNET_HDR_LEN),
					((u8 *)blk.addr + ETHERNET_HDR_LEN))
					+ ETHERNET_HDR_LEN;
	} else {
		blk.length = skb->len;
		memcpy(((u8 *)blk.addr), skb->data, skb->len);
	}

#ifdef DUMP_TRANSMIT_PACKET
	print_hex_dump(KERN_DEBUG, "transmit packet: ", DUMP_PREFIX_OFFSET,
		       16, 1, skb->data, skb->len, 0);
#endif
	ret = sblock_send(WLAN_CP_ID, WLAN_SBLOCK_CH, &blk);
	if (ret) {
		dev_err(&dev->dev, "Failed to send sblock (%d)\n", ret);
		sblock_put(WLAN_CP_ID, WLAN_SBLOCK_CH, &blk);
		priv->ndev->stats.tx_fifo_errors++;
		if (priv->txrcnt > SETH_RESEND_MAX_NUM)
			netif_stop_queue(dev);
		priv->txrcnt++;
		return NETDEV_TX_BUSY;
	}

	/*
	 * Statistics.
	 */
	priv->ndev->stats.tx_bytes += skb->len;
	priv->ndev->stats.tx_packets++;
	dev->trans_start = jiffies;
	priv->txrcnt = 0;

	dev_kfree_skb_any(skb);

	return NETDEV_TX_OK;
}

/*
 * Open interface
 */
static int itm_wlan_open(struct net_device *dev)
{
	struct itm_priv *priv = netdev_priv(dev);

	dev_info(&dev->dev, "%s\n", __func__);

	/* if first open net device do nothing, otherwise act as resume */
	napi_enable(&priv->napi);
	if (netif_queue_stopped(dev))
		netif_device_attach(dev);

	return 0;
}

/*
 * Close interface
 */
static int itm_wlan_close(struct net_device *dev)
{
	struct itm_priv *priv = netdev_priv(dev);

	dev_info(&dev->dev, "%s\n", __func__);

	/* if netdevice carrier off, do nothing */
	if (netif_queue_stopped(dev))
		netif_device_attach(dev);
	napi_disable(&priv->napi);

	return 0;
}

static struct net_device_stats *itm_wlan_get_stats(struct net_device *dev)
{
	return &(dev->stats);
}

static void itm_wlan_tx_timeout(struct net_device *dev)
{
	dev_info(&dev->dev, "%s\n", __func__);
	dev->trans_start = jiffies;
	netif_wake_queue(dev);
}

#define CMD_BLACKLIST_ENABLE		"BLOCK"
#define CMD_BLACKLIST_DISABLE		"UNBLOCK"

int itm_priv_cmd(struct net_device *dev, struct ifreq *ifr)
{
	struct itm_priv *priv = netdev_priv(dev);
	int ret = 0;
	char *command = NULL;
	int bytes_written = 0;
	android_wifi_priv_cmd priv_cmd;
	u8 addr[6] = {0};

	if (!ifr->ifr_data) {
		ret = -EINVAL;
		goto exit;
	}
	if (copy_from_user(&priv_cmd, ifr->ifr_data,
			   sizeof(android_wifi_priv_cmd))) {
		ret = -EFAULT;
		goto exit;
	}

	command = kmalloc(priv_cmd.total_len, GFP_KERNEL);
	if (!command) {
		dev_err(&priv->wdev->netdev->dev,
			"%s: failed to allocate memory\n", __func__);
		ret = -ENOMEM;
		goto exit;
	}
	if (copy_from_user(command, priv_cmd.buf, priv_cmd.total_len)) {
		ret = -EFAULT;
		goto exit;
	}

	if (strnicmp(command, CMD_BLACKLIST_ENABLE,
		     strlen(CMD_BLACKLIST_ENABLE)) == 0) {
		int skip = strlen(CMD_BLACKLIST_ENABLE) + 1;

		dev_err(&priv->wdev->netdev->dev,
			"%s, Received regular blacklist enable command\n",
			__func__);
		sscanf(command + skip, "%02x:%02x:%02x:%02x:%02x:%02x",
		       (unsigned int *)&(addr[0]),
		       (unsigned int *)&(addr[1]),
		       (unsigned int *)&(addr[2]),
		       (unsigned int *)&(addr[3]),
		       (unsigned int *)&(addr[4]),
		       (unsigned int *)&(addr[5]));
		bytes_written = itm_wlan_set_blacklist_cmd(priv->wlan_sipc,
							   addr, 1);
	} else if (strnicmp(command, CMD_BLACKLIST_DISABLE,
			    strlen(CMD_BLACKLIST_DISABLE)) == 0) {
		int skip = strlen(CMD_BLACKLIST_DISABLE) + 1;

		dev_err(&priv->wdev->netdev->dev,
			"%s, Received regular blacklist disable command\n",
			__func__);
		sscanf(command + skip, "%02x:%02x:%02x:%02x:%02x:%02x",
		       (unsigned int *)&(addr[0]),
		       (unsigned int *)&(addr[1]),
		       (unsigned int *)&(addr[2]),
		       (unsigned int *)&(addr[3]),
		       (unsigned int *)&(addr[4]),
		       (unsigned int *)&(addr[5]));
		bytes_written = itm_wlan_set_blacklist_cmd(priv->wlan_sipc,
							   addr, 0);
	}

	if (bytes_written < 0)
		ret = bytes_written;

exit:
	kfree(command);

	return ret;
}

static int itm_wlan_ioctl(struct net_device *dev, struct ifreq *req, int cmd)
{
	struct itm_priv *priv = netdev_priv(dev);
	struct iwreq *wrq = (struct iwreq *)req;

	switch (cmd) {
	case SIOCDEVPRIVATE + 1:
		return itm_priv_cmd(dev, req);
		break;
	case SIOGETSSID:
		if (priv->ssid_len > 0) {
			if (copy_to_user(wrq->u.essid.pointer, priv->ssid,
					 priv->ssid_len))
				return -EFAULT;
			wrq->u.essid.length = priv->ssid_len;
		} else {
			dev_err(&dev->dev, "ssid len is zero\n");
			return -EFAULT;
		}
		break;
	default:
		dev_err(&dev->dev, "ioctl cmd %d is not supported\n", cmd);
		return -ENOTSUPP;
	}

	return 0;
}

#if defined(CONFIG_HAS_EARLYSUSPEND) && defined(CONFIG_ITM_WLAN_ENHANCED_PM)
static void itm_wlan_early_suspend(struct early_suspend *es)
{
	struct itm_priv *priv = container_of(es, struct itm_priv,
					     early_suspend);
	int ret = 0;

	dev_info(&priv->ndev->dev, "%s\n", __func__);

	ret = itm_wlan_pm_early_suspend_cmd(priv->wlan_sipc);
	if (ret)
		dev_err(&priv->ndev->dev, "Failed to early suspend (%d)\n",
			ret);
}

static void itm_wlan_late_resume(struct early_suspend *es)
{
	struct itm_priv *priv = container_of(es, struct itm_priv,
					     early_suspend);
	int ret = 0;

	dev_info(&priv->ndev->dev, "%s\n", __func__);

	ret = itm_wlan_pm_later_resume_cmd(priv->wlan_sipc);
	if (ret)
		dev_err(&priv->ndev->dev, "Failed to late resume(%d)\n", ret);
}
#endif

#if defined(CONFIG_PM) && defined(CONFIG_ITM_WLAN_PM_POWERSAVE)
static int itm_wlan_suspend(struct device *dev)
{
	struct net_device *ndev = dev_get_drvdata(dev);
	struct itm_priv *priv = netdev_priv(ndev);
	int ret = 0;

	dev_info(dev, "%s\n", __func__);

	netif_device_detach(ndev);
	napi_disable(&priv->napi);

	ret = itm_wlan_pm_enter_ps_cmd(priv);
	if (ret)
		dev_err(dev, "Failed to suspend (%d)\n", ret);

	return ret;
}

static int itm_wlan_resume(struct device *dev)
{
	struct net_device *ndev = dev_get_drvdata(dev);
	struct itm_priv *priv = netdev_priv(ndev);
	int ret = 0;

	dev_info(dev, "%s\n", __func__);

	ret = itm_wlan_pm_exit_ps_cmd(priv->wlan_sipc);
	if (ret)
		dev_err(dev, "Failed to resume (%d)\n", ret);

	napi_enable(&priv->napi);
	netif_device_attach(ndev);
	return ret;
}

static const struct dev_pm_ops itm_wlan_pm = {
	.suspend = itm_wlan_suspend,
	.resume = itm_wlan_resume,
};
#else
static const struct dev_pm_ops itm_wlan_pm;
#endif

static struct net_device_ops itm_wlan_ops = {
	.ndo_open = itm_wlan_open,
	.ndo_stop = itm_wlan_close,
	.ndo_start_xmit = itm_wlan_start_xmit,
	.ndo_get_stats = itm_wlan_get_stats,
	.ndo_tx_timeout = itm_wlan_tx_timeout,
	.ndo_do_ioctl = itm_wlan_ioctl,
};

static int itm_inetaddr_event(struct notifier_block *this,
			      unsigned long event, void *ptr)
{
	struct itm_priv *priv;
	struct net_device *dev;

	struct in_ifaddr *ifa = (struct in_ifaddr *)ptr;

	dev = ifa->ifa_dev ? ifa->ifa_dev->dev : NULL;

	if (dev == NULL)
		goto done;

	priv = netdev_priv(dev);

	if (!priv)
		goto done;

	if (strncmp(dev->name, ITM_INTF_NAME, 4) != 0)
		goto done;

	switch (event) {
	case NETDEV_UP:
		itm_wlan_get_ip_cmd(priv, (u8 *) & ifa->ifa_address);
		break;
	case NETDEV_DOWN:
		break;
	default:
		break;
	}

done:
	return NOTIFY_DONE;
}

static struct notifier_block itm_inetaddr_cb = {
	.notifier_call = itm_inetaddr_event,
};

static int itm_pm_notifier(struct notifier_block *notifier,
			   unsigned long pm_event, void *unused)
{
	int ret = NOTIFY_DONE;
	struct itm_priv *priv = container_of(notifier,
					     struct itm_priv,
					     pm_notifier);

	/* We should not suspend when there is sipc cmd send and recv. */
	switch (pm_event) {
	case PM_SUSPEND_PREPARE:
		if (!mutex_trylock(&priv->wlan_sipc->pm_lock)) {
			priv->pm_status = false;
			ret = NOTIFY_BAD;
		} else {
			priv->pm_status = true;
			ret = NOTIFY_OK;
		}
		break;

	/* Restore from hibernation failed. We need to clean
	 * up in exactly the same way, so fall through.
	 */
	case PM_POST_SUSPEND:
		if (priv->pm_status == true) {
			mutex_unlock(&priv->wlan_sipc->pm_lock);
			priv->pm_status = false;
		}
		ret = NOTIFY_OK;
		break;
	default:
		break;
	}

	return ret;
}

static void itm_register_pm_notifier(struct itm_priv *priv)
{
	priv->pm_notifier.notifier_call = itm_pm_notifier;
	register_pm_notifier(&priv->pm_notifier);
}

static void itm_unregister_pm_notifier(struct itm_priv *priv)
{
	unregister_pm_notifier(&priv->pm_notifier);
}

/*
 * Initialize WLAN device.
 */
static int __devinit itm_wlan_probe(struct platform_device *pdev)
{
	struct net_device *ndev;
	struct itm_priv *priv;
	int ret;
    #if defined(CONFIG_MACH_SP7730EC) || defined(CONFIG_MACH_SP7730GA) || defined(CONFIG_MACH_SPX35EC) || defined(CONFIG_MACH_SP8830GA) \
        || defined(CONFIG_MACH_SP7715EA) || defined(CONFIG_MACH_SP7715EATRISIM) || defined(CONFIG_MACH_SP7715GA) || defined(CONFIG_MACH_SP7715GATRISIM)||defined(CONFIG_MACH_SP5735C1EA)
    rf2351_gpio_ctrl_power_enable(1);
    #endif
	ndev =
	    alloc_netdev(sizeof(struct itm_priv), ITM_INTF_NAME, ether_setup);
	if (!ndev) {
		dev_err(&pdev->dev, "Failed to allocate net_device\n");
		ret = -ENOMEM;
		goto out;
	}

	priv = netdev_priv(ndev);
	priv->ndev = ndev;
	atomic_set(&priv->stopped, 0);
	ndev->netdev_ops = &itm_wlan_ops;
	ndev->watchdog_timeo = 1 * HZ;

	priv->pm_status = false;

	/*FIXME: If get mac from cfg file error, got random addr */
	ret = itm_get_mac_from_cfg(priv);
	if (ret)
		random_ether_addr(ndev->dev_addr);
/*
	ret = sblock_create(WLAN_CP_ID, WLAN_SBLOCK_CH,
			    WLAN_SBLOCK_NUM, WLAN_SBLOCK_SIZE,
			    WLAN_SBLOCK_NUM, WLAN_SBLOCK_SIZE);
	if (ret) {
		dev_err(&pdev->dev, "Failed to create sblock (%d)\n", ret);
		goto err_sblock;
	}
*/
	ret =
	    sblock_register_notifier(WLAN_CP_ID, WLAN_SBLOCK_CH,
				     itm_wlan_handler, priv);
	if (ret) {
		dev_err(&pdev->dev,
			"Failed to regitster sblock notifier (%d)\n", ret);
		goto err_notify_sblock;
	}

	netif_napi_add(ndev, &priv->napi, itm_wlan_rx_handler, 64);

	/*Init MAC and get the capabilities */
#if 0
	ret = itm_hw_init();
	if (ret) {
		dev_err(&pdev->dev, "Failed to detect MAC controller (%d)\n",
			ret);
		goto err_notify_sblock;
	}
#endif

	ret = itm_wdev_alloc(priv, &pdev->dev);
	if (ret) {
		dev_err(&pdev->dev, "Failed to register wiphy (%d)\n", ret);
		goto err_notify_sblock;
	}

	/* register new Ethernet interface */
	ret = register_netdev(ndev);
	if (ret) {
		dev_err(&pdev->dev, "Failed to regitster net_dev (%d)\n", ret);
		goto err_register_netdev;
	}

	wake_lock_init(&priv->scan_done_lock, WAKE_LOCK_SUSPEND, "scan_lock");

	ret = npi_init_netlink();
	if (ret) {
		dev_err(&pdev->dev, "Failed to init npi netlink (%d)\n", ret);
		goto err_npi_netlink;
	}
	platform_set_drvdata(pdev, ndev);

	ittiam_nvm_init();
#if defined(CONFIG_HAS_EARLYSUSPEND) && defined(CONFIG_ITM_WLAN_ENHANCED_PM)
	priv->early_suspend.suspend = itm_wlan_early_suspend;
	priv->early_suspend.resume = itm_wlan_late_resume;
	priv->early_suspend.level = EARLY_SUSPEND_LEVEL_DISABLE_FB + 1;
	register_early_suspend(&priv->early_suspend);
#endif
#ifdef CONFIG_INET
	register_inetaddr_notifier(&itm_inetaddr_cb);
#endif
	/* Register PM notifiers */
	itm_register_pm_notifier(priv);
	dev_info(&pdev->dev, "%s sucessfully\n", __func__);

	return 0;
err_npi_netlink:
	unregister_netdev(ndev);
err_register_netdev:
	itm_wdev_free(priv);
err_notify_sblock:
	netif_napi_del(&priv->napi);
	sblock_destroy(WLAN_CP_ID, WLAN_SBLOCK_CH);
/*err_sblock:
	free_netdev(ndev);*/
out:
	return ret;
}

/*
 * Cleanup WLAN device.
 */
static int __devexit itm_wlan_remove(struct platform_device *pdev)
{
	struct net_device *ndev = platform_get_drvdata(pdev);
	struct itm_priv *priv = netdev_priv(ndev);
	int ret;
   #if defined(CONFIG_MACH_SP7730EC) || defined(CONFIG_MACH_SP7730GA) || defined(CONFIG_MACH_SPX35EC) || defined(CONFIG_MACH_SP8830GA) \
       || defined(CONFIG_MACH_SP7715EA) || defined(CONFIG_MACH_SP7715EATRISIM) || defined(CONFIG_MACH_SP7715GA) || defined(CONFIG_MACH_SP7715GATRISIM)||defined(CONFIG_MACH_SP5735C1EA)
    rf2351_gpio_ctrl_power_enable(0);
    #endif
/*	sblock_destroy(WLAN_CP_ID, WLAN_SBLOCK_CH);*/ /*FIXME*/
	/* FIXME it is a ugly method */
	ret =
		sblock_register_notifier(WLAN_CP_ID, WLAN_SBLOCK_CH,
					 NULL, NULL);
	if (ret) {
		dev_err(&pdev->dev,
			"Failed to regitster sblock notifier (%d)\n", ret);
	}

	itm_unregister_pm_notifier(priv);
	unregister_early_suspend(&priv->early_suspend);
	wake_lock_destroy(&priv->scan_done_lock);
#ifdef CONFIG_INET
	unregister_inetaddr_notifier(&itm_inetaddr_cb);
#endif
	unregister_netdev(ndev);
	itm_wdev_free(priv);
	free_netdev(ndev);
	npi_exit_netlink();
	platform_set_drvdata(pdev, NULL);

	return 0;
}

static struct platform_driver itm_wlan_driver __refdata = {
	.probe = itm_wlan_probe,
	.remove = __devexit_p(itm_wlan_remove),
	.driver = {
		   .owner = THIS_MODULE,
		   .name = ITM_DEV_NAME,
		   .pm = &itm_wlan_pm,
		   },
};

static struct platform_device *itm_wlan_device;
static int __init itm_wlan_init(void)
{
	pr_info("ITTIAM Wireless Network Adapter (%s %s)\n", __DATE__,
		__TIME__);
	itm_wlan_device =
	    platform_device_register_simple(ITM_DEV_NAME, 0, NULL, 0);
	if (IS_ERR(itm_wlan_device))
		return PTR_ERR(itm_wlan_device);

	return platform_driver_register(&itm_wlan_driver);
}

static void __exit itm_wlan_exit(void)
{
	platform_driver_unregister(&itm_wlan_driver);
	platform_device_unregister(itm_wlan_device);
	itm_wlan_device = NULL;
}

module_init(itm_wlan_init);
module_exit(itm_wlan_exit);

MODULE_DESCRIPTION("ITTIAM Wireless Network Adapter");
MODULE_LICENSE("GPL");
