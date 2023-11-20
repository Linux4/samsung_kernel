/*
 * Copyright (C) 2013 Spreadtrum Communications Inc.
 *
 * Authors:
 * Keguang Zhang <keguang.zhang@spreadtrum.com>
 * Wenjie Zhang <wenjie.zhang@spreadtrum.com>
 * Jingxiang Li <jingxiang.li@spreadtrum.com>
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
#include <linux/ipv6.h>
#include <linux/ip.h>
#include <linux/inetdevice.h>
#include <asm/byteorder.h>
#include <linux/platform_device.h>
#include <linux/suspend.h>
#ifdef CONFIG_SPRDWL_ENHANCED_PM
#include <linux/earlysuspend.h>
#endif
#ifdef CONFIG_OF
#include <linux/of.h>
#include <linux/of_device.h>
#endif
#include <linux/sipc.h>
#include <linux/atomic.h>

#include <linux/sprd_2351.h>

#include "sprdwl.h"
#include "cfg80211.h"
#include "sipc.h"
#include "npi.h"
#include "wapi.h"

#define SPRDWL_DEV_NAME		"sprd_wlan"
#define SPRDWL_WLAN_INTF_NAME		"wlan%d"
#define SPRDWL_P2P_INTF_NAME		"p2p%d"

#define SETH_RESEND_MAX_NUM	10
#define SIOGETSSID		0x89F2
#ifdef CONFIG_SPRDWL_FW_ZEROCOPY
#define SIPC_TRANS_OFFSET	50
#endif

static void str2mac(const char *mac_addr, unsigned char mac[ETH_ALEN])
{
	unsigned int m[ETH_ALEN];
	if (sscanf(mac_addr, "%02x:%02x:%02x:%02x:%02x:%02x",
		   &m[0], &m[1], &m[2], &m[3], &m[4], &m[5]) != ETH_ALEN)
		pr_err("Failed to parse mac address '%s'", mac_addr);
	mac[0] = m[0];
	mac[1] = m[1];
	mac[2] = m[2];
	mac[3] = m[3];
	mac[4] = m[4];
	mac[5] = m[5];
}

struct sprdwl_vif *sprdwl_get_report_vif(struct sprdwl_priv *priv)
{
	if (priv->wlan_vif->wdev.iftype == NL80211_IFTYPE_AP)
		return priv->wlan_vif;
	else if (priv->p2p_vif->wdev.iftype == NL80211_IFTYPE_P2P_GO)
		return priv->p2p_vif;
	else
		return priv->cur_vif;
}

static int sprdwl_rx_handler(struct napi_struct *napi, int budget)
{
	struct sprdwl_vif *vif = container_of(napi, struct sprdwl_vif, napi);
	struct net_device *ndev = vif->ndev;
	struct wlan_sblock_recv_data *data;
	struct sblock blk;
	struct sk_buff *skb;
	u16 decryp_data_len = 0;
	u32 work_done, length = 0;
	int ret;
#ifdef CONFIG_SPRDWL_FW_ZEROCOPY
	u8 offset = 0;
#endif
	for (work_done = 0; work_done < budget; work_done++) {
		ret = sblock_receive(WLAN_CP_ID, WLAN_SBLOCK_CH, &blk, 0);
		if (ret) {
			/*netdev_dbg(ndev,
				   "got %d packets, no more sblock to read\n",
				   work_done);*/
			break;
		}
#ifdef CONFIG_SPRDWL_FW_ZEROCOPY
		offset = *(u8 *)blk.addr;
		length = blk.length - 2 - offset;
#else
		length = blk.length;
#endif
		/*16 bytes align */
		skb = dev_alloc_skb(length + NET_IP_ALIGN);
		if (!skb) {
			netdev_err(ndev, "Failed to allocate skbuff!\n");
			ndev->stats.rx_dropped++;
			goto rx_failed;
		}
#ifdef CONFIG_SPRDWL_FW_ZEROCOPY
		data = (struct wlan_sblock_recv_data *)(blk.addr + 2 + offset);
#else
		data = (struct wlan_sblock_recv_data *)blk.addr;
#endif

		if (data->is_encrypted == 1) {
			if (vif->sm_state == SPRDWL_CONNECTED &&
			    vif->prwise_crypto == SPRDWL_CIPHER_WAPI &&
			    vif->key_len[GROUP][vif->key_index[GROUP]] != 0 &&
			    vif->key_len[PAIRWISE][vif->key_index[PAIRWISE]] !=
			    0) {
				u8 snap_header[6] = { 0xaa, 0xaa, 0x03,
					0x00, 0x00, 0x00
				};
				skb_reserve(skb, NET_IP_ALIGN);
				decryp_data_len = wlan_rx_wapi_decryption(vif,
						   (u8 *)&data->u2.encrypt,
						   data->u1.encrypt.header_len,
						   (length -
						   sizeof(data->is_encrypted) -
						   sizeof(data->u1) -
						   data->u1.encrypt.header_len),
						   (skb->data + 12));
				if (decryp_data_len == 0) {
					netdev_err(ndev,
						   "Failed to decrypt WAPI data!\n");
					ndev->stats.rx_dropped++;
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
					       mac_header.addr1, ETH_ALEN);
					memcpy(skb->data + ETH_ALEN,
					       data->u2.encrypt.
					       mac_header.addr2, ETH_ALEN);
					skb_put(skb, (decryp_data_len + 6));
				} else {
					/* copy eth header */
					memcpy(skb->data,
					       data->u2.encrypt.
					       mac_header.addr3, ETH_ALEN);
					memcpy(skb->data + ETH_ALEN,
					       data->u2.encrypt.
					       mac_header.addr2, ETH_ALEN);
					skb_put(skb, (decryp_data_len + 12));
				}
			} else {
				netdev_err(ndev, "wrong encryption data!\n");
				ndev->stats.rx_dropped++;
				dev_kfree_skb(skb);
				goto rx_failed;
			}
		} else if (data->is_encrypted == 0) {
			skb_reserve(skb, NET_IP_ALIGN);
			/* dec the first encrypt byte */
			memcpy(skb->data, (u8 *)&data->u2,
			       (length - sizeof(data->is_encrypted) -
				sizeof(data->u1)));
			skb_put(skb,
				(length - sizeof(data->is_encrypted) -
				 sizeof(data->u1)));
		} else {
			netdev_err(ndev, "wrong data fromat received!\n");
			ndev->stats.rx_dropped++;
			dev_kfree_skb(skb);
			goto rx_failed;
		}

#ifdef DUMP_RECEIVE_PACKET
		print_hex_dump(KERN_DEBUG, "RX packet: ", DUMP_PREFIX_OFFSET,
			       16, 1, skb->data, skb->len, 0);
#endif
		skb->dev = ndev;
		skb->protocol = eth_type_trans(skb, ndev);
		/* CHECKSUM_UNNECESSARY not supported by our hardware */
		/* skb->ip_summed = CHECKSUM_UNNECESSARY; */

		ndev->stats.rx_packets++;
		ndev->stats.rx_bytes += skb->len;

		napi_gro_receive(napi, skb);

rx_failed:
		ret = sblock_release(WLAN_CP_ID, WLAN_SBLOCK_CH, &blk);
		if (ret)
			netdev_err(ndev,
				   "Failed to release sblock(%d)!\n", ret);
	}
	if (work_done < budget)
		napi_complete(napi);

	return work_done;
}

#ifdef CONFIG_SPRDWL_FW_ZEROCOPY
/*
 * Tx_flow_control handler.
 */
static void sprdwl_tx_complete(struct sprdwl_vif *vif)
{
	struct sprdwl_priv *priv = vif->priv;

	if (atomic_read(&priv->tx_free) > TX_FLOW_HIGH) {
		/* netdev_dbg(vif->ndev, "tx flow control send data\n"); */
		vif->ndev->trans_start = jiffies;
		netif_wake_queue(vif->ndev);
	}
}
#endif

static void sprdwl_handler(int event, void *data)
{
	struct sprdwl_priv *priv = (struct sprdwl_priv *)data;
	struct sprdwl_vif *vif;

	vif = sprdwl_get_report_vif(priv);
	if (!vif) {
		pr_err("%s:not connected event:%d\n", __func__, event);
		if (event == SBLOCK_NOTIFY_GET)
			atomic_inc(&priv->tx_free);
		return;
	}
	switch (event) {
	case SBLOCK_NOTIFY_GET:
#ifdef CONFIG_SPRDWL_FW_ZEROCOPY
		sprdwl_tx_complete(vif);
#endif
		atomic_inc(&priv->tx_free);
		/* netdev_dbg(vif->ndev, "SBLOCK_NOTIFY_GET is received\n"); */
		break;
	case SBLOCK_NOTIFY_RECV:
		/* netdev_dbg(vif->ndev, "SBLOCK_NOTIFY_RECV is received\n"); */
		napi_schedule(&vif->napi);
		break;
	case SBLOCK_NOTIFY_STATUS:
		netdev_dbg(vif->ndev, "SBLOCK_NOTIFY_STATUS is received\n");
		break;
	case SBLOCK_NOTIFY_OPEN:
		netdev_dbg(vif->ndev, "SBLOCK_NOTIFY_OPEN is received\n");
		break;
	case SBLOCK_NOTIFY_CLOSE:
		netdev_dbg(vif->ndev, "SBLOCK_NOTIFY_CLOSE is received\n");
		break;
	default:
		netdev_err(vif->ndev, "invalid data event: %d\n", event);
	}
}

/*Transmit interface*/
static int sprdwl_start_xmit(struct sk_buff *skb, struct net_device *ndev)
{
	struct sprdwl_vif *vif = netdev_priv(ndev);
	struct sprdwl_priv *priv = vif->priv;
	struct sblock blk;
	u8 *addr = NULL;
	int ret;

#ifdef CONFIG_SPRDWL_FW_ZEROCOPY
	if (atomic_read(&priv->tx_free) < TX_FLOW_LOW) {
		netdev_err(ndev, "tx flow control full\n");
		netif_stop_queue(ndev);
		ndev->stats.tx_fifo_errors++;
		return NETDEV_TX_BUSY;
	}
#endif
	/*Get a free sblock.*/
	ret = sblock_get(WLAN_CP_ID, WLAN_SBLOCK_CH, &blk, 0);
	if (ret) {
		netdev_err(ndev, "Failed to get free sblock(%d)!\n", ret);
		netif_stop_queue(ndev);
		ndev->stats.tx_fifo_errors++;
		return NETDEV_TX_BUSY;
	}

	if (blk.length < skb->len) {
		netdev_err(ndev, "The size of sblock is so tiny!\n");
		ndev->stats.tx_fifo_errors++;
		sblock_put(WLAN_CP_ID, WLAN_SBLOCK_CH, &blk);
		dev_kfree_skb_any(skb);
		priv->txrcnt = 0;
		return NETDEV_TX_OK;
	}
#ifdef CONFIG_SPRDWL_FW_ZEROCOPY
	addr = blk.addr + SIPC_TRANS_OFFSET;
#else
	addr = blk.addr;
#endif
	atomic_dec(&priv->tx_free);
	if (vif->sm_state == SPRDWL_CONNECTED &&
	    vif->prwise_crypto == SPRDWL_CIPHER_WAPI &&
/*            vif->key_len[GROUP][vif->key_index[GROUP]] != 0 &&*/
	    vif->key_len[PAIRWISE][vif->key_index[PAIRWISE]] != 0 &&
	    (*(u16 *)((u8 *)skb->data + ETH_PKT_TYPE_OFFSET) != 0xb488)) {
		memcpy(((u8 *)addr), skb->data, ETHERNET_HDR_LEN);
		blk.length = wlan_tx_wapi_encryption(vif,
					skb->data,
					(skb->len - ETHERNET_HDR_LEN),
					((u8 *)addr + ETHERNET_HDR_LEN))
					+ ETHERNET_HDR_LEN;
	} else {
		blk.length = skb->len;
		memcpy(((u8 *)addr), skb->data, skb->len);
	}

#ifdef DUMP_TRANSMIT_PACKET
	print_hex_dump(KERN_DEBUG, "TX packet: ", DUMP_PREFIX_OFFSET,
		       16, 1, skb->data, skb->len, 0);
#endif
	ret = sblock_send(WLAN_CP_ID, WLAN_SBLOCK_CH, &blk);
	if (ret) {
		netdev_err(ndev, "Failed to send sblock(%d)!\n", ret);
		sblock_put(WLAN_CP_ID, WLAN_SBLOCK_CH, &blk);
		atomic_inc(&priv->tx_free);
		ndev->stats.tx_fifo_errors++;
		if (priv->txrcnt > SETH_RESEND_MAX_NUM)
			netif_stop_queue(ndev);
		priv->txrcnt++;
		return NETDEV_TX_BUSY;
	}

	/*Statistics*/
	ndev->stats.tx_bytes += skb->len;
	ndev->stats.tx_packets++;
	ndev->trans_start = jiffies;
	priv->txrcnt = 0;

	dev_kfree_skb_any(skb);

	return NETDEV_TX_OK;
}

/*Open interface*/
static int sprdwl_open(struct net_device *ndev)
{
	struct sprdwl_vif *vif = netdev_priv(ndev);
	struct sprdwl_priv *priv = vif->priv;
	int ret = 0;

	netdev_info(ndev, "%s\n", __func__);

#ifdef CONFIG_SPRDWL_WIFI_DIRECT
	if (vif == priv->p2p_vif)
		goto done;
#endif /* CONFIG_SPRDWL_WIFI_DIRECT */
	ret = sprdwl_open_mac(vif, vif->wdev.iftype);
	if (ret) {
		wiphy_err(priv->wiphy,
			  "%s Failed to open mac!\n", __func__);
		return -EIO;
	}
#ifdef CONFIG_SPRDWL_WIFI_DIRECT
done:
#endif /* CONFIG_SPRDWL_WIFI_DIRECT */
	napi_enable(&vif->napi);
	netif_start_queue(ndev);

	return ret;
}

/*Close interface*/
static int sprdwl_close(struct net_device *ndev)
{
	struct sprdwl_vif *vif = netdev_priv(ndev);
	struct sprdwl_priv *priv = vif->priv;
	int ret = 0;

	netdev_info(ndev, "%s\n", __func__);
	netif_stop_queue(ndev);
	napi_disable(&vif->napi);
	if (timer_pending(&vif->scan_timer))
		del_timer_sync(&vif->scan_timer);
	spin_lock_bh(&vif->scan_lock);
	if (vif->scan_req) {
		/*If there's a pending scan request,abort it */
		cfg80211_scan_done(vif->scan_req, true);
		vif->scan_req = NULL;
		priv->scan_vif = NULL;
	}
	spin_unlock_bh(&vif->scan_lock);
#ifdef CONFIG_SPRDWL_WIFI_DIRECT
	if (vif == priv->p2p_vif)
		goto done;
#endif /* CONFIG_SPRDWL_WIFI_DIRECT */

	ret = sprdwl_close_mac(vif);
	if (ret)
		netdev_err(ndev, "%s Failed to close mac!\n", __func__);
#ifdef CONFIG_SPRDWL_WIFI_DIRECT
done:
#endif /* CONFIG_SPRDWL_WIFI_DIRECT */

	return ret;
}

static struct net_device_stats *sprdwl_get_stats(struct net_device *ndev)
{
	return &(ndev->stats);
}

static void sprdwl_tx_timeout(struct net_device *ndev)
{
	netdev_info(ndev, "%s\n", __func__);
	ndev->trans_start = jiffies;
	netif_wake_queue(ndev);
}

#define CMD_BLACKLIST_ENABLE		"BLOCK"
#define CMD_BLACKLIST_DISABLE		"UNBLOCK"

int sprdwl_priv_cmd(struct net_device *ndev, struct ifreq *ifr)
{
	struct sprdwl_vif *vif = netdev_priv(ndev);
	struct sprdwl_priv *priv = vif->priv;
	struct android_wifi_priv_cmd priv_cmd;
	char *command = NULL;
	u8 addr[ETH_ALEN] = {0};
	int bytes_written = 0;
	int ret = 0;

	if (!ifr->ifr_data)
		return -EINVAL;
	if (copy_from_user(&priv_cmd, ifr->ifr_data,
			   sizeof(struct android_wifi_priv_cmd)))
		return -EFAULT;

	command = kmalloc(priv_cmd.total_len, GFP_KERNEL);
	if (!command) {
		netdev_err(ndev, "%s: Failed to allocate command!\n", __func__);
		return -ENOMEM;
	}
	if (copy_from_user(command, priv_cmd.buf, priv_cmd.total_len)) {
		ret = -EFAULT;
		goto exit;
	}

	if (strnicmp(command, CMD_BLACKLIST_ENABLE,
		     strlen(CMD_BLACKLIST_ENABLE)) == 0) {
		int skip = strlen(CMD_BLACKLIST_ENABLE) + 1;

		netdev_err(ndev,
			   "%s, Received regular blacklist enable command\n",
			   __func__);
		str2mac(command + skip, addr);
		bytes_written = sprdwl_set_blacklist_cmd(priv->sipc, addr, 1);
	} else if (strnicmp(command, CMD_BLACKLIST_DISABLE,
			    strlen(CMD_BLACKLIST_DISABLE)) == 0) {
		int skip = strlen(CMD_BLACKLIST_DISABLE) + 1;

		netdev_err(ndev,
			   "%s, Received regular blacklist disable command\n",
			   __func__);
		str2mac(command + skip, addr);
		bytes_written = sprdwl_set_blacklist_cmd(priv->sipc, addr, 0);
	}

	if (bytes_written < 0)
		ret = bytes_written;

exit:
	kfree(command);

	return ret;
}

static int sprdwl_ioctl(struct net_device *ndev, struct ifreq *req, int cmd)
{
	struct sprdwl_vif *vif = netdev_priv(ndev);
	struct iwreq *wrq = (struct iwreq *)req;

	switch (cmd) {
	case SIOCDEVPRIVATE + 1:
		return sprdwl_priv_cmd(ndev, req);
		break;
	case SIOGETSSID:
		if (vif->ssid_len > 0) {
			if (copy_to_user(wrq->u.essid.pointer, vif->ssid,
					 vif->ssid_len))
				return -EFAULT;
			wrq->u.essid.length = vif->ssid_len;
		} else {
			netdev_err(ndev, "ssid len is zero\n");
			return -EFAULT;
		}
		break;
	default:
		netdev_err(ndev, "ioctl cmd %d is not supported\n", cmd);
		return -ENOTSUPP;
	}

	return 0;
}

static struct net_device_ops sprdwl_netdev_ops = {
	.ndo_open = sprdwl_open,
	.ndo_stop = sprdwl_close,
	.ndo_start_xmit = sprdwl_start_xmit,
	.ndo_get_stats = sprdwl_get_stats,
	.ndo_tx_timeout = sprdwl_tx_timeout,
	.ndo_do_ioctl = sprdwl_ioctl,
};

#ifdef CONFIG_SPRDWL_ENHANCED_PM
static void sprdwl_early_suspend(struct early_suspend *es)
{
	struct sprdwl_vif *vif = container_of(es, struct sprdwl_vif,
					      early_suspend);
	struct sprdwl_priv *priv = vif->priv;
	int ret = 0;

	netdev_info(vif->ndev, "%s\n", __func__);

	ret = sprdwl_pm_early_suspend_cmd(priv->sipc);
	if (ret)
		netdev_err(vif->ndev, "Failed to early suspend(%d)!\n", ret);
}

static void sprdwl_late_resume(struct early_suspend *es)
{
	struct sprdwl_vif *vif = container_of(es, struct sprdwl_vif,
					      early_suspend);
	struct sprdwl_priv *priv = vif->priv;
	int ret = 0;

	netdev_info(vif->ndev, "%s\n", __func__);

	ret = sprdwl_pm_later_resume_cmd(priv->sipc);
	if (ret)
		netdev_err(vif->ndev, "Failed to late resume(%d)!\n", ret);
}
#endif

#ifdef CONFIG_PM
static int sprdwl_suspend(struct device *dev)
{
	struct sprdwl_priv *priv = dev_get_drvdata(dev);
	int ret = 0;

	wiphy_info(priv->wiphy, "%s\n", __func__);

	if (!mutex_trylock(&priv->sipc->pm_lock)) {
		wiphy_err(priv->wiphy, "Failed to suspend(%d)!\n", ret);
		ret = -EBUSY;
	}

	return ret;
}

static int sprdwl_resume(struct device *dev)
{
	struct sprdwl_priv *priv = dev_get_drvdata(dev);
	int ret = 0;

	wiphy_info(priv->wiphy, "%s\n", __func__);

	if (mutex_is_locked(&priv->sipc->pm_lock))
		mutex_unlock(&priv->sipc->pm_lock);

	return ret;
}

static const struct dev_pm_ops sprdwl_pm = {
	.suspend = sprdwl_suspend,
	.resume = sprdwl_resume,
};
#else
static const struct dev_pm_ops sprdwl_pm;
#endif

static int sprdwl_inetaddr_event(struct notifier_block *this,
				 unsigned long event, void *ptr)
{
	struct net_device *ndev;
	struct sprdwl_vif *vif;
	struct in_ifaddr *ifa = (struct in_ifaddr *)ptr;

	ndev = ifa->ifa_dev ? ifa->ifa_dev->dev : NULL;

	if (ndev == NULL)
		return NOTIFY_DONE;

	vif = netdev_priv(ndev);

	if (!vif)
		return NOTIFY_DONE;

	if (!strncmp(ndev->name, SPRDWL_WLAN_INTF_NAME, 4))
		goto get_ip;
	else if (!strncmp(ndev->name, SPRDWL_P2P_INTF_NAME, 3))
		goto get_ip;

	return NOTIFY_DONE;

get_ip:
	switch (event) {
	case NETDEV_UP:
		sprdwl_get_ip_cmd(vif->priv, (u8 *)&ifa->ifa_address);
		break;
	case NETDEV_DOWN:
		break;
	default:
		break;
	}

	return NOTIFY_DONE;
}

static struct notifier_block sprdwl_inetaddr_cb = {
	.notifier_call = sprdwl_inetaddr_event,
};

#define ENG_MAC_ADDR_PATH "/data/misc/wifi/wifimac.txt"
int sprdwl_get_mac_from_cfg(struct sprdwl_vif *vif)
{
	struct file *fp = 0;
	mm_segment_t fs;
	loff_t *pos;
	u8 file_data[64] = { 0 };
	u8 mac_addr[18] = { 0 };
	u8 *tmp_p = NULL;

	fp = filp_open(ENG_MAC_ADDR_PATH, O_RDONLY, 0);
	if (IS_ERR(fp))
		return -ENOENT;

	fs = get_fs();
	set_fs(KERNEL_DS);

	pos = &(fp->f_pos);
	vfs_read(fp, file_data, sizeof(file_data), pos);
	tmp_p = file_data;
	if (tmp_p != NULL) {
		memcpy(mac_addr, tmp_p, sizeof(mac_addr));
		str2mac(mac_addr, vif->ndev->dev_addr);
	}

	filp_close(fp, NULL);
	set_fs(fs);

	return 0;
}

struct net_device *sprdwl_register_netdev(struct sprdwl_priv *priv,
					  const char *name,
					  enum nl80211_iftype type,
					  u8 *addr)
{
	struct net_device *ndev;
	struct sprdwl_vif *vif;
	int ret;

	ndev = alloc_netdev(sizeof(struct sprdwl_vif), name, ether_setup);
	if (!ndev) {
		pr_err("%s Failed to allocate net_device!\n", __func__);
		goto out;
	}

	ndev->netdev_ops = &sprdwl_netdev_ops;
	ndev->destructor = free_netdev;
	ndev->watchdog_timeo = 1 * HZ;

	vif = netdev_priv(ndev);
	vif->ndev = ndev;
	vif->priv = priv;
	ndev->ieee80211_ptr = &vif->wdev;
	vif->wdev.netdev = ndev;
	vif->wdev.wiphy = priv->wiphy;
	SET_NETDEV_DEV(ndev, wiphy_dev(vif->wdev.wiphy));

	vif->wdev.iftype = type;

	vif->sm_state = SPRDWL_DISCONNECTED;
	spin_lock_init(&vif->scan_lock);
	vif->scan_req = NULL;
	setup_timer(&vif->scan_timer, sprdwl_scan_timeout, (unsigned long)vif);
	memset(vif->ssid, 0, sizeof(vif->ssid));
	memset(vif->bssid, 0, sizeof(vif->bssid));
#ifdef CONFIG_SPRDWL_WIFI_DIRECT
	vif->reg = 0;
	vif->frame_type = 0xffff;
	INIT_WORK(&vif->work, register_frame_work_func);
#endif

	if (addr && is_valid_ether_addr(addr))
		memcpy(ndev->dev_addr, addr, ETH_ALEN);
	else if (sprdwl_get_mac_from_cfg(vif))
		random_ether_addr(ndev->dev_addr);

	/* register new Ethernet interface */
	ret = register_netdevice(ndev);
	if (ret) {
		netdev_err(ndev, "Failed to regitster netdev(%d)!\n", ret);
		goto err_register_netdev;
	}
#ifdef CONFIG_SPRDWL_ENHANCED_PM
	vif->early_suspend.suspend = sprdwl_early_suspend;
	vif->early_suspend.resume = sprdwl_late_resume;
	vif->early_suspend.level = EARLY_SUSPEND_LEVEL_BLANK_SCREEN - 1;
	register_early_suspend(&vif->early_suspend);
#endif
	spin_lock_bh(&priv->list_lock);
	list_add_tail(&vif->list, &priv->vif_list);
	spin_unlock_bh(&priv->list_lock);
	netif_napi_add(ndev, &vif->napi, sprdwl_rx_handler, 64);

	netdev_info(ndev, "virtual interface '%s'(%pM) added\n",
		    ndev->name, ndev->dev_addr);
	return ndev;

err_register_netdev:
	free_netdev(ndev);
out:
	return NULL;
}

void sprdwl_unregister_netdev(struct net_device *ndev)
{
	struct sprdwl_vif *vif = netdev_priv(ndev);

	netdev_info(ndev, "virtual interface '%s'(%pM) deleted\n",
		    ndev->name, ndev->dev_addr);
#ifdef CONFIG_SPRDWL_ENHANCED_PM
	unregister_early_suspend(&vif->early_suspend);
#endif
#ifdef CONFIG_SPRDWL_WIFI_DIRECT
	cancel_work_sync(&vif->work);
#endif
	netif_napi_del(&vif->napi);
	unregister_netdevice(ndev);
}

void sprdwl_unregister_all_ifaces(struct sprdwl_priv *priv)
{
	struct sprdwl_vif *vif, *tmp_vif;

	spin_lock_bh(&priv->list_lock);
	list_for_each_entry_safe(vif, tmp_vif, &priv->vif_list, list) {
		list_del(&vif->list);
		spin_unlock_bh(&priv->list_lock);
		rtnl_lock();
		sprdwl_unregister_netdev(vif->ndev);
		rtnl_unlock();
		spin_lock_bh(&priv->list_lock);
	}
	spin_unlock_bh(&priv->list_lock);
}

static int sprdwl_init_misc(struct sprdwl_priv *priv)
{
	int ret;

#ifndef CONFIG_OF
	ret = sblock_register_notifier(WLAN_CP_ID, WLAN_SBLOCK_CH,
				       sprdwl_handler, priv);
	if (ret) {
		pr_err("%s Failed to regitster data sblock notifier(%d)!\n",
		       __func__, ret);
		goto err_data_sblock;
	}
	ret = sblock_register_notifier(WLAN_CP_ID, WLAN_EVENT_SBLOCK_CH,
				       wlan_sipc_sblock_handler, priv);
	if (ret) {
		pr_err("%s Failed to regitster event sblock notifier (%d)!\n",
		       __func__, ret);
		goto err_event_sblock;
	}
#else
	ret = sprdwl_sipc_sblock_init(WLAN_SBLOCK_CH, sprdwl_handler, priv);
	if (ret) {
		pr_err("%s Failed to init data sblock %d for wifi:%d\n",
			__func__, WLAN_SBLOCK_CH, ret);
		goto err_dt_data_sblock;
	}
	ret = sprdwl_sipc_sblock_init(WLAN_EVENT_SBLOCK_CH,
			wlan_sipc_sblock_handler, priv);
	if (ret) {
		pr_err("%s Failed to init event sblock %d for wifi:%d\n",
			__func__, WLAN_EVENT_SBLOCK_CH, ret);
		goto err_dt_event_sblock;
	}
#endif

	return ret;

#ifndef CONFIG_OF
err_event_sblock:
	sblock_register_notifier(WLAN_CP_ID, WLAN_EVENT_SBLOCK_CH, NULL, NULL);
err_data_sblock:
#else
err_dt_event_sblock:
	sprdwl_sipc_sblock_deinit(WLAN_SBLOCK_CH);
err_dt_data_sblock:
#endif
	return ret;
}

static void sprdwl_deinit_misc(struct sprdwl_priv *priv)
{
#ifndef CONFIG_OF
	sblock_register_notifier(WLAN_CP_ID, WLAN_SBLOCK_CH, NULL, NULL);
	sblock_register_notifier(WLAN_CP_ID, WLAN_EVENT_SBLOCK_CH, NULL, NULL);
#else
	sprdwl_sipc_sblock_deinit(WLAN_SBLOCK_CH);
	sprdwl_sipc_sblock_deinit(WLAN_EVENT_SBLOCK_CH);
#endif
}

/*Initialize WLAN device*/
static int sprdwl_probe(struct platform_device *pdev)
{
	struct net_device *ndev, *p2p_ndev;
	struct wiphy *wiphy;
	struct sprdwl_priv *priv;
	int ret;

	rf2351_gpio_ctrl_power_enable(1);
	rf2351_vddwpa_ctrl_power_enable(1);

	wiphy = sprdwl_register_wiphy(&pdev->dev);
	if (wiphy == NULL) {
		ret = -ENOMEM;
		goto out;
	}
	priv = wiphy_priv(wiphy);
	platform_set_drvdata(pdev, priv);

	rtnl_lock();
	ndev = sprdwl_register_netdev(priv, SPRDWL_WLAN_INTF_NAME,
				      NL80211_IFTYPE_STATION, NULL);
	rtnl_unlock();
	if (!ndev) {
		ret = -ENOMEM;
		goto err_register_intf;
	}
	priv->wlan_vif = netdev_priv(ndev);

	/*Due to the flaw of framework, we have to prepare p2p intf for them*/
	rtnl_lock();
	p2p_ndev = sprdwl_register_netdev(priv, SPRDWL_P2P_INTF_NAME,
					  NL80211_IFTYPE_P2P_DEVICE, NULL);
	rtnl_unlock();
	if (!p2p_ndev) {
		ret = -ENOMEM;
		goto err_register_p2p_intf;
	}
	priv->p2p_vif = netdev_priv(p2p_ndev);

	ret = register_inetaddr_notifier(&sprdwl_inetaddr_cb);
	if (ret) {
		pr_err("%s Failed to register inetaddr notifier(%d)!\n",
		       __func__, ret);
		goto err_register_inetaddr_notifier;
	}

	ret = npi_init_netlink();
	if (ret) {
		pr_err("%s Failed to init NPI netlink(%d)!\n", __func__, ret);
		goto err_npi_netlink;
	}

	ret = sprdwl_init_misc(priv);
	if (ret) {
		pr_err("%s Failed to init apcp trans(%d)!\n", __func__, ret);
		goto err_apcp_tans;
	}
	netdev_info(ndev, "%s successfully", __func__);
	return 0;

err_apcp_tans:
	npi_exit_netlink();
err_npi_netlink:
	unregister_inetaddr_notifier(&sprdwl_inetaddr_cb);
err_register_inetaddr_notifier:
	sprdwl_unregister_netdev(p2p_ndev);
err_register_p2p_intf:
	platform_set_drvdata(pdev, NULL);
	sprdwl_unregister_netdev(ndev);
err_register_intf:
	sprdwl_unregister_wiphy(wiphy);
out:
	rf2351_vddwpa_ctrl_power_enable(0);
	rf2351_gpio_ctrl_power_enable(0);
	return ret;
}

/*Cleanup WLAN device*/
static int sprdwl_remove(struct platform_device *pdev)
{
	struct sprdwl_priv *priv = platform_get_drvdata(pdev);
	struct wiphy *wiphy = priv->wiphy;

	dev_info(&pdev->dev, "%s\n", __func__);

	sprdwl_deinit_misc(priv);
	npi_exit_netlink();
	unregister_inetaddr_notifier(&sprdwl_inetaddr_cb);
	platform_set_drvdata(pdev, NULL);
	sprdwl_unregister_all_ifaces(priv);
	sprdwl_unregister_wiphy(wiphy);

	rf2351_vddwpa_ctrl_power_enable(0);
	rf2351_gpio_ctrl_power_enable(0);
	return 0;
}

/* if Macro CONFIG_OF is defined, then Device Tree is used */
#ifdef CONFIG_OF
static const struct of_device_id of_match_table_sprdwl[] = {
	{.compatible = "sprd,sprd_wlan",},
	{},
};
#endif

static struct platform_driver sprdwl_driver = {
	.probe = sprdwl_probe,
	.remove = sprdwl_remove,
	.driver = {
		   .owner = THIS_MODULE,
		   .name = SPRDWL_DEV_NAME,
#ifdef CONFIG_OF
		   .of_match_table = of_match_ptr(of_match_table_sprdwl),
#endif
		   .pm = &sprdwl_pm,
		   },
};

#ifndef CONFIG_OF
static struct platform_device *sprdwl_device;
#endif
static int __init sprdwl_init(void)
{
	pr_info("Spreadtrum Wireless LAN Driver (%s %s)\n", __DATE__, __TIME__);
#ifndef CONFIG_OF
	sprdwl_device =
	    platform_device_register_simple(SPRDWL_DEV_NAME, 0, NULL, 0);
	if (IS_ERR(sprdwl_device))
		return PTR_ERR(sprdwl_device);
#endif

	return platform_driver_register(&sprdwl_driver);
}

static void __exit sprdwl_exit(void)
{
	platform_driver_unregister(&sprdwl_driver);
#ifndef CONFIG_OF
	platform_device_unregister(sprdwl_device);
	sprdwl_device = NULL;
#endif
}

module_init(sprdwl_init);
module_exit(sprdwl_exit);

MODULE_DESCRIPTION("Spreadtrum Wireless LAN Driver");
MODULE_AUTHOR("Keguang Zhang <keguang.zhang@spreadtrum.com>");
MODULE_AUTHOR("Wenjie Zhang <wenjie.zhang@spreadtrum.com>");
MODULE_AUTHOR("Jingxiang Li <jingxiang.li@spreadtrum.com>");
MODULE_LICENSE("GPL");
