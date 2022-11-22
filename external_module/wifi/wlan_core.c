/*
 * Copyright (C) 2014 Spreadtrum Communications Inc.
 *
 * Authors:<jinglong.chen@spreadtrum.com>
 * Owner:
 *      jinglong.chen
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

#include "wlan_common.h"
#include "wlan_fifo.h"
#include "wlan_cmd.h"
#include "wlan_cfg80211.h"
#include "wlan_wapi.h"

wlan_info_t g_wlan = { 0 };

unsigned int g_dbg = 0xFFFFFFFF;
static int g_wifi_chn = 9;

wlan_ieee80211_regdomain wlan_regdom_11 = {
	.n_reg_rules = 1,
	.alpha2 = "US",
	.reg_rules = {
		      wlan_2GHZ_CH01_11,
              }
};

wlan_ieee80211_regdomain wlan_regdom_12_13 = {
	.n_reg_rules = 2,
	.alpha2 = "00",
	.reg_rules = {
		      wlan_2GHZ_CH01_11,
			  wlan_2GHZ_CH12_13,
		      }
};

struct ieee80211_regdomain ieee80211_regdom_11 = {
	.n_reg_rules = 1,
	.alpha2 = "US",
	.reg_rules = {
		      wlan_2GHZ_CH01_11,
              }
};

struct ieee80211_regdomain ieee80211_regdom_12_13 = {
	.n_reg_rules = 2,
	.alpha2 = "00",
	.reg_rules = {
		      wlan_2GHZ_CH01_11,
			  wlan_2GHZ_CH12_13,
		      }
};

static void wlan_tx_chn_flush(unsigned char  mode);

void core_up(void)
{
	up(&(g_wlan.wlan_core.sem));
}
void core_down(void)
{
	down(&(g_wlan.wlan_core.sem));
}

void core_try_down(int retry)
{
	int sem_count;	
	wlan_thread_t *thread;
	thread = &(g_wlan.wlan_core);
	if(  thread->need_rx != thread->done_rx)
		return;
	if( (thread->need_tx != thread->done_tx) && (retry == 0) )
		return;
	atomic_set(&(thread->retry), retry);
	sem_count = g_wlan.wlan_core.sem.count ;
	do {
		if (g_wlan.wlan_core.exit_flag)
			break;
		down(&(g_wlan.wlan_core.sem));
	}
	while(sem_count--);
	return;
}

void core_try_up(void )
{
	if( atomic_read(&g_wlan.wlan_core.retry) > 0)
	{
		atomic_dec(&g_wlan.wlan_core.retry);
		up(&(g_wlan.wlan_core.sem));
	}
}

void trans_up(void )
{
	up(&(g_wlan.wlan_trans.sem));
}
void trans_down(void )
{
	down(&(g_wlan.wlan_trans.sem));
}

static void wlan_thread_exit(void)
{
	printkd("[%s] enter\n", __func__);
	if (g_wlan.wlan_core.exit_flag == 0) {
		g_wlan.wlan_core.exit_flag = 1;
		do {
			core_up();
			usleep_range(300, 500);
		} while (1 != atomic_read(&(g_wlan.wlan_core.exit_status)));
	}

	if (g_wlan.wlan_trans.exit_flag == 0) {
		g_wlan.wlan_trans.exit_flag = 1;
		do {
			trans_up();
			usleep_range(300, 500);
		} while (1 != atomic_read(&(g_wlan.wlan_trans.exit_status)));
	}
	printkd("[%s] exit\n", __func__);
}

bool stop_net(unsigned char id )
{
	int i;
	
	if( (NULL == g_wlan.netif[id].ndev) || 
		  (false == g_wlan.netif[id].flow_ctrl_enable) )
		return false;
	for(i=0; i < 10; i++)
	{
		netif_stop_queue(g_wlan.netif[id].ndev);
		if( netif_queue_stopped(g_wlan.netif[id].ndev) )
		{
			g_wlan.netif[id].flow_ctrl_set = true;
			printkd("[%s][%d]\n", __func__, id);
			return true;
		}
	}
	printkd("[%s][%d][fail]\n", __func__, id);
	return false;
}

bool wake_net(unsigned char id)
{
	int i;
	
	if( (NULL == g_wlan.netif[id].ndev) ||
		(false == g_wlan.netif[id].flow_ctrl_enable) ||
		(1 == g_wlan.sync.exit) ||
		(1 != g_wlan.sync.cp2_status) )
		return false;
	for(i=0; i < 10; i++)
	{
		netif_wake_queue(g_wlan.netif[id].ndev);
		if(! netif_queue_stopped(g_wlan.netif[id].ndev) )
		{
			g_wlan.netif[id].flow_ctrl_set = false;
			printkd("[%s][%d]\n", __func__, id);
			return true;
		}
	}
	printkd("[%s][%d][fail]\n", __func__, id);
	return false;
}

static int hw_rx(const unsigned short chn, unsigned char *buf, unsigned int *len)
{
	int ret;
	unsigned int read_len = 0, seq;	
	static unsigned int cnt = 0, save_seq = 0;
	if(NULL == buf)
		return ERROR;
	if(2 == g_wlan.sync.cp2_status)
		return ERROR;	
	ret = sdio_dev_read(chn, buf, &read_len);
	*len = read_len;
	if(0 != ret)
	{
		printke("call sdio_dev_read err:%d\n", ret);
		return HW_READ_ERROR;
	}
	memcpy((char *)(&seq), buf+4, 4);
	if( seq != (save_seq+1) )
		printke("sdio_read seq fail(%d,%d)\r\n", seq, save_seq);
	save_seq = seq;
	if( (8 == chn) || (9 == chn) )
		g_wifi_chn = chn;
	printkp("[rx][%d]\n", chn );
	wlan_rx_buf_decode(buf, read_len);
	cnt++;
	*len = read_len;
	g_wlan.hw.rx_cnt++;
	return OK;
}

static int hw_tx(const unsigned short chn, unsigned char *buf, unsigned int len)
{
	int ret;
	static unsigned int cnt = 0;
	static unsigned int skb = 0;
	tx_big_hdr_t *big_hdr;

	big_hdr = (tx_big_hdr_t *)buf;
	if ((PKT_AGGR_NUM < big_hdr->msg_num) || (0 == big_hdr->msg_num))
	{
		ASSERT();
		return ERROR;
	}
	big_hdr->tx_cnt = g_wlan.hw.tx_cnt;
	printkp("[tx][%d][%d]\n",big_hdr->tx_cnt, chn);
	len = (len+1023)&0xFC00;
	wlan_tx_buf_decode(buf, len);
	if(2 == g_wlan.sync.cp2_status)
		return ERROR;	
	ret = sdio_dev_write(chn, buf, len);
	if(0 != ret)
	{
		printke("call sdio_dev_write err:%d\n", ret);
		return HW_WRITE_ERROR;
	}
	skb = skb + big_hdr->msg_num;
	cnt++;
	if(1000 == cnt)
	{
		printkd("w%d\n", skb);
		cnt = 0;
		skb = 0;
	}
	g_wlan.hw.tx_cnt++;
	return OK;
}

static int wlan_rx_skb_process(const unsigned char vif_id, unsigned char *pData, unsigned short len)
{
	struct sk_buff *skb;
	struct net_device *ndev = g_wlan.netif[vif_id].ndev;
	if((NULL == pData) || (0 == len) || (NULL == ndev))
	{
		printkd("[%s][%d][err]\n", __func__, (int )vif_id);
		return ERROR;
	}
	skb = dev_alloc_skb(len + NET_IP_ALIGN);
	if(NULL == skb)
		return ERROR;
	skb_reserve(skb, NET_IP_ALIGN);
	memcpy(skb->data,   pData,  len);

	skb_put(skb, len);
	skb->dev = ndev;
	skb->protocol = eth_type_trans(skb, ndev);
	ndev->stats.rx_packets++;
	printkp("rx_skb:%d\n", (int)(ndev->stats.rx_packets) );
	ndev->stats.rx_bytes += skb->len;
	if ( in_interrupt() )
		netif_rx(skb);
	else
		netif_rx_ni(skb);
    return OK;
}

static int wlan_rx_wapi_process(const unsigned char vif_id, unsigned char *pData, unsigned short len)
{
	struct ieee80211_hdr_3addr *addr;
	int decryp_data_len = 0;
	struct sk_buff *skb;
	u8 snap_header[6] = { 0xaa, 0xaa, 0x03,0x00, 0x00, 0x00};
	wlan_vif_t   *vif;
	struct net_device *ndev;

	vif  = &(g_wlan.netif[vif_id]);
	ndev = vif->ndev;
	if((NULL == pData) || (0 == len) || (NULL == ndev))
	{
		printkd("[%s][%d][err]\n", __func__, (int )vif_id);
		return ERROR;
	}
	addr = (struct ieee80211_hdr_3addr *)pData;
	skb = dev_alloc_skb(len + NET_IP_ALIGN);
	if(NULL == skb)
		return ERROR;
	skb_reserve(skb, NET_IP_ALIGN);

	decryp_data_len = wlan_rx_wapi_decryption(vif, (unsigned char  *)addr, 24, (len -24),(skb->data + 12));
	if (decryp_data_len == 0)
	{
		dev_kfree_skb(skb);
		return ERROR;
	}
	if ( memcmp((skb->data + 12), snap_header, sizeof(snap_header)) == 0)
	{
		skb_reserve(skb, 6);
		memcpy(skb->data,addr->addr1, 6);
		memcpy(skb->data + 6,addr->addr2, 6);
		skb_put(skb, (decryp_data_len + 6));
	}
	else
	{
		/* copy eth header */
		memcpy(skb->data,addr->addr3, 6);
		memcpy(skb->data + 6, addr->addr2, 6);
		skb_put(skb, (decryp_data_len + 12) );
	}
	skb->dev = ndev;
	skb->protocol = eth_type_trans(skb, ndev);
	ndev->stats.rx_packets++;
	printkp("rx_skb:%d\n", (int)(ndev->stats.rx_packets) );
	ndev->stats.rx_bytes += skb->len;
	if ( in_interrupt() )
		netif_rx(skb);
	else
		netif_rx_ni(skb);
	return OK;
}

void wlan_rx_chn_isr(int chn)
{
	static unsigned int cnt = 1;
	printkp("[irq][%d]\n", cnt);
	cnt++;
	trans_up();
}

static int wlan_xmit(struct sk_buff *skb, struct net_device *dev)
{
	wlan_vif_t      *vif;
	tx_msg_t         msg = {0};
	msg_q_t         *msg_q;
	int              ret,q_index,addr_len = 0;
	struct sk_buff  *wapi_skb;

	if(2 == g_wlan.sync.cp2_status)
	{
		dev_kfree_skb(skb);
		return NETDEV_TX_OK;
	}
	vif     = ndev_to_vif(dev);
	msg_q = wlan_q_match_ack(vif, skb->data, skb->len);
	if(NULL == msg_q)
	{
		q_index = qos_match_q(&(vif->qos), skb->data, skb->len);
		if( ( msg_num(&(vif->msg_q[q_index])) >= (vif->msg_q[q_index].num * 9 / 10) ) && (false == vif->flow_ctrl_set) )
		{
			printkd("msg_q[%d] is full, count = %d\n", q_index, msg_num(&(vif->msg_q[q_index])));
			stop_net(vif->id);
		}
		vif->qos.count[q_index][0]++;
		msg_q = vif->qos.msg_q + q_index;
	}
	if (vif->cfg80211.cipher_type == WAPI && vif->cfg80211.connect_status == ITM_CONNECTED &&
		vif->cfg80211.key_len[PAIRWISE][vif->cfg80211.key_index[PAIRWISE]] != 0 &&(*(u16 *)((u8 *)skb->data + ETH_PKT_TYPE_OFFSET) != 0xb488)) {
		wapi_skb = dev_alloc_skb(skb->len+100+NET_IP_ALIGN);
		if (!wapi_skb)
		{
			printkd("L-PKT\n");
			dev_kfree_skb(skb);
			return NETDEV_TX_OK;
		}
		skb_reserve(wapi_skb, NET_IP_ALIGN);
		memcpy( wapi_skb->data, skb->data, ETHERNET_HDR_LEN );
		addr_len = wlan_tx_wapi_encryption(vif, skb->data, (skb->len - ETHERNET_HDR_LEN),  ( (unsigned char *)(wapi_skb->data) + ETHERNET_HDR_LEN)  );
		addr_len = addr_len + ETHERNET_HDR_LEN;
		skb_put(wapi_skb, addr_len);
		dev_kfree_skb(skb);
		skb = wapi_skb;
	}
	msg.p = (void *)skb;
	msg.slice[0].data = skb->data;
	msg.slice[0].len  = skb->len;
	msg.hdr.mode      = vif->id;
	msg.hdr.type      = HOST_SC2331_PKT;
	msg.hdr.subtype   = 0;
	msg.hdr.len = skb->len;
	
	ret = msg_q_in(msg_q, &msg);
	if(OK != ret){
		printkd("L-PKT\n");
		dev_kfree_skb(skb);
		return NETDEV_TX_OK;
	}
	vif->ndev->stats.tx_bytes += skb->len;
	vif->ndev->stats.tx_packets++;
	dev->trans_start = jiffies;	
	g_wlan.wlan_core.need_tx++;
	core_up();
	return NETDEV_TX_OK;
}

void wlan_rx_malformed_handler()
{
	static int count = 0;

	count++;
	if (MAX_RX_MALFORMED_NUM == count) {
		WARN_ON(1);
		count = 0;
		wlan_tx_chn_flush(1);
	}
}

static int wlan_rx_process(unsigned char *buf, unsigned int max_len)
{
	static unsigned int cnt = 0;
	static unsigned int skb = 0;
	unsigned int     p = 0;
	r_msg_hdr_t     *msg = NULL;
	unsigned char    vif_id;	
	wlan_vif_t      *vif;
	unsigned char   *pData = NULL;
	unsigned short   len;
	unsigned char    event;
	if((NULL == buf) || (0 == max_len))
	{
		printke("[%s][ERROR]\n", __func__);
		return OK;
	}
	buf      = buf + 8;
	msg      = (r_msg_hdr_t *)(buf);
	max_len  = max_len - 8;
	while(p < max_len)
	{
		
		vif_id = msg->mode;
		vif    = &(g_wlan.netif[vif_id]);
		pData  = (unsigned char *)(msg+1);
		len    = msg->len;
		
		if( (0x7F == msg->type) || (0xFF == msg->subtype) )     // type is 7 bit
			break;
		if(HOST_SC2331_PKT == msg->type)
		{
			pData = pData + msg->subtype;
			len   = len   - msg->subtype;
			wlan_rx_skb_process(vif_id, pData, len);
		}
		else if(HOST_SC2331_WAPI == msg->type
			&& WAPI == vif->cfg80211.cipher_type)
		{
			wlan_rx_wapi_process(vif_id, pData, len);
		}
		else if(SC2331_HOST_RSP == msg->type)
		{
			wlan_rx_rsp_process(vif_id, msg);
		}
		else if(HOST_SC2331_CMD == msg->type)
		{
			event = msg->subtype;
			wlan_rx_event_process(vif_id, event, pData, len);
		}
		else
		{
			printke("[%s][RX DATA ERR]\n", __func__);
			break;
		}
		p = p + sizeof(t_msg_hdr_t) + ALIGN_4BYTE(msg->len);
		msg = (r_msg_hdr_t *)(buf+p);
		skb++;
	}
	cnt++;
	if(1000 == cnt)
	{
		printkd("r%d\n", skb);
		cnt = 0;
		skb = 0;
	}
	return OK;
}

static void str2mac(const char *mac_addr, unsigned char mac[ETH_ALEN])
{
	unsigned int m[ETH_ALEN];
	if (sscanf(mac_addr, "%02x:%02x:%02x:%02x:%02x:%02x",
	   &m[0], &m[1], &m[2], &m[3], &m[4], &m[5]) != ETH_ALEN)
		printke("Failed to parse mac address '%s'", mac_addr);
	mac[0] = m[0];
	mac[1] = m[1];
	mac[2] = m[2];
	mac[3] = m[3];
	mac[4] = m[4];
	mac[5] = m[5];
}

int marlin_priv_cmd(struct net_device *ndev, struct ifreq *ifr)
{
	wlan_vif_t *vif;
	android_wifi_priv_cmd priv_cmd;
	char *command = NULL, *p = NULL, *q = NULL;
	u8 addr[ETH_ALEN] = {0};
	int bytes_written = 0;
	int ret = 0;
	int counter = 0, i = 0, cmd_len = 0;
	u8 *mac_addrs = NULL;

	vif =  ndev_to_vif(ndev);
	if (!ifr->ifr_data)
		return -EINVAL;
	if (copy_from_user(&priv_cmd, ifr->ifr_data,
			   sizeof(struct android_wifi_priv_cmd)))
		return -EFAULT;

	command = kmalloc(priv_cmd.total_len, GFP_KERNEL);
	if (!command) {
		printke("%s: Failed to allocate command!\n", __func__);
		return -ENOMEM;
	}

	if (copy_from_user(command, priv_cmd.buf, priv_cmd.total_len)) {
		ret = -EFAULT;
		goto exit;
	}

	if (strnicmp(command, CMD_BLACKLIST_ENABLE,
		     strlen(CMD_BLACKLIST_ENABLE)) == 0) {
		int skip = strlen(CMD_BLACKLIST_ENABLE) + 1;

		printke("%s, Received regular blacklist enable command\n",
			__func__);
		str2mac(command + skip, addr);
		bytes_written = wlan_cmd_add_blacklist(vif->id, addr);
	} else if (strnicmp(command, CMD_BLACKLIST_DISABLE,
			    strlen(CMD_BLACKLIST_DISABLE)) == 0) {
		int skip = strlen(CMD_BLACKLIST_DISABLE) + 1;

		printke("%s, Received regular blacklist disable command\n",
			__func__);
		str2mac(command + skip, addr);
		bytes_written = wlan_cmd_del_blacklist(vif->id, addr);
	} else if (strnicmp(command, CMD_ADD_WHITELIST,
					strlen(CMD_ADD_WHITELIST)) == 0) {
		int skip = strlen(CMD_ADD_WHITELIST) + 1;
		printke("CMD_ADD_WHITELIST: %s\n", command);

		str2mac(command + skip, addr);
		bytes_written = wlan_cmd_add_whitelist(vif->id, addr);

	} else if (strnicmp(command, CMD_DEL_WHITELIST,
					strlen(CMD_DEL_WHITELIST)) == 0) {
		int skip = strlen(CMD_DEL_WHITELIST) + 1;
		printke("CMD_DEL_WHITELIST: %s\n", command);

		str2mac(command + skip, addr);
		bytes_written = wlan_cmd_del_whitelist(vif->id, addr);

	} else if (strnicmp(command, CMD_ENABLE_WHITELIST,
					strlen(CMD_ENABLE_WHITELIST)) == 0) {
		printke("CMD_ENABLE_WHITELIST: %s, totalen = %d\n", command, priv_cmd.total_len);
		cmd_len = strlen(CMD_ENABLE_WHITELIST);
		counter = command[cmd_len + 1];
		printke("CMD_ENABLE_WHITELIST counter = %d\n", counter);
		if (!counter) {
			ret = wlan_cmd_enable_whitelist(vif->id, NULL);
			goto exit;
		}

		q = mac_addrs = kmalloc(6 * counter + 1, GFP_KERNEL);
		if (IS_ERR(mac_addrs)) {
			ret = -ENOMEM;
			goto exit;
		}

		mac_addrs[0] = counter; //num of macs
		mac_addrs++;
		p = command + cmd_len + 2;

		for (i = 0; i < counter; i++) {
			str2mac(p, mac_addrs);
			mac_addrs += 6;
			p += 18;
		}

		ret = wlan_cmd_enable_whitelist(vif->id, q);
	} else if (strnicmp(command, CMD_DISABLE_WHITELIST,
					strlen(CMD_DISABLE_WHITELIST)) == 0) {
		cmd_len = strlen(CMD_DISABLE_WHITELIST);
		counter = command[cmd_len + 1];
		printke("CMD_DIS_WHITELIST counter = %d\n", counter);
		if (!counter)
		{
			ret = wlan_cmd_disable_whitelist(vif->id, NULL);
			goto exit;
		}

		q = mac_addrs = kmalloc(6 * counter + 1, GFP_KERNEL);
		if (IS_ERR(mac_addrs)) {
			ret = -ENOMEM;
			goto exit;
		}

		mac_addrs[0] = counter;//num of macs
		mac_addrs++;
		p = command + cmd_len + 2;

		for (i = 0; i < counter; i++) {
			str2mac(p, mac_addrs);
			mac_addrs += 6;
			p += 18;
		}

		ret = wlan_cmd_disable_whitelist(vif->id, q);
#ifdef CONFIG_WCN_EXTENSION
	} else if (strnicmp(command, HIDDEN_SSID, strlen(HIDDEN_SSID)) == 0) {
		int skip = strlen(HIDDEN_SSID) + 1;
		struct wlan_cmd_hidden_ssid *hssid = &(vif->hssid);

		sscanf(command + skip, "%x %x",
				&(hssid->ignore_broadcast_ssid),
				&(hssid->ssid_len));
		strncpy(hssid->ssid,
				command + strlen(HIDDEN_SSID) +1 +10,
				hssid->ssid_len);
		printke("broadcast:%x\n", hssid->ignore_broadcast_ssid);
		printke("len:%x\n", hssid->ssid_len);
		printke("ssid:%s\n", hssid->ssid);
#endif
	} else if (strnicmp(command, SET_FCC_CHANNEL, strlen(SET_FCC_CHANNEL)) == 0) {
			printkd("set fcc channel\n");
			int skip = strlen(SET_FCC_CHANNEL) + 1;
		if (strncmp(command + skip, "0", 1) == 0)
			wlan_cmd_set_fcc_channel(vif->id, 0);
		else
			wlan_cmd_set_fcc_channel(vif->id, -1);
	} else {
		printkd("command not support\n");
		ret = -1;
	}

	if (bytes_written < 0)
		ret = bytes_written;

exit:
	kfree(command);

	return ret;
}

static int wlan_open(struct net_device *dev)
{
	printkd("%s open\n", dev->name );
	netif_start_queue(dev);
	return 0;
}
static int wlan_close(struct net_device *dev)
{
	printkd("%s %s enter\n", __func__, dev->name );
	netif_stop_queue(dev);
	printkd("%s %s ok\n", __func__, dev->name );
	return 0;
}
static struct net_device_stats *wlan_status(struct net_device *dev)
{
	return &(dev->stats);
}

static void wlan_tx_timeout(struct net_device *dev)
{
	wlan_vif_t   *vif;
	printke("%s tx timeout\n", dev->name);
	vif = ndev_to_vif(dev);
	
	if (!netif_carrier_ok(dev))
		netif_carrier_on(dev);
	dev->trans_start = jiffies;
	wake_net(vif->id);
	core_up();
	return;
}


static int wlan_send_common_data_to_ic(struct net_device *ndev, struct ifreq *req)
{
	int ret;
	char *buf = NULL;
	wlan_vif_t *vif;
	struct android_wifi_priv_cmd priv_cmd;

	if (copy_from_user(&priv_cmd, req->ifr_ifru.ifru_data,
						sizeof(priv_cmd))) {
		printk("copy usr data failed! \n");
		return -EFAULT;
	}

	buf = kmalloc(priv_cmd.total_len, GFP_KERNEL);
	if (buf == NULL) {
		printk("%s malloc error!\n", __func__);
		return -ENOMEM;
	}

	if (copy_from_user(buf, priv_cmd.buf, priv_cmd.total_len)) {
		printk("copy failed from user space\n");
		kfree(buf);
		return -EFAULT;
	}

	vif = ndev_to_vif(ndev);

	ret = wlan_cmd_send_common_data(vif->id, buf, priv_cmd.total_len);
	if (ret < 0) {
		printkd("wlan_cmd_req_lte_concur failed with ret %d\n",	ret);
	}

	return ret;
}

static int wlan_set_suspend_mode(struct net_device *ndev, struct ifreq *ifr)
{
    wlan_vif_t *vif;
    struct android_wifi_priv_cmd priv_cmd;
    int ret = 0; 
    char *command = NULL, *ptr = NULL;

    vif = ndev_to_vif(ndev);

    if (!ifr->ifr_data)
        return -EINVAL;

    if (copy_from_user(&priv_cmd, ifr->ifr_data,
                sizeof(struct android_wifi_priv_cmd)))
        return -EFAULT;

    command = kmalloc(priv_cmd.total_len + 1, GFP_KERNEL);
    if (!command) {
        printke("%s: %s Failed to allocate command!\n",
                __func__, ndev->name);
        return -ENOMEM;
    }    

    memset(command, 0x00, priv_cmd.total_len + 1);
    if (copy_from_user(command, priv_cmd.buf, priv_cmd.total_len)) {
        printke("%s: %s Failed to copy user buf data!\n",
                __func__, ndev->name);
        ret = -EFAULT;
        goto exit;
    }    

    ptr = strchr(command, ' ');
    if (ptr == NULL) {
        printke("Command format error!\n");
        ret = -1;
        goto exit;
    }    

    *ptr = 0; 
    ptr++;

    if ((strcmp(command, CMD_SETSUSPENDMODE) == 0) &&
            (strlen(ptr) == 1)) {
        if (*ptr == '0') {
            ret = wlan_cmd_set_suspend_mode(vif->id, false);
        } else if (*ptr == '1'){
            ret = wlan_cmd_set_suspend_mode(vif->id, true);
        } else {
            printke("Argument 2 not support\n");
            ret = -1;
        }    
    } else {
        printke("command [%s] not support\n", command);
        ret = -1;
    }    

exit:
    kfree(command);
    return ret; 
}


int wlan_set_country_code(struct net_device *dev, struct ifreq *ifr)
{
    
    int ret = 0;
    char *buf = NULL, *ptr;
    struct android_wifi_priv_cmd priv_cmd;
    wlan_vif_t *vif;
    unsigned char vif_id;
    int rd_size;

    vif = ndev_to_vif(dev);
    vif_id = vif->id;

	printkd("[%s][enter]\n", __func__);
    if (!ifr->ifr_data)
        return -EINVAL;
    if (copy_from_user(&priv_cmd, ifr->ifr_data,
                            sizeof(struct android_wifi_priv_cmd))) {
        printke("copy usr data failed! \n");
        return -EFAULT;
    }

    buf = kmalloc(priv_cmd.total_len + 1, GFP_KERNEL);
    if (!buf) {
        printke("malloc error!\n");
        return -ENOMEM;
    }
	
	memset(buf, 0x00, priv_cmd.total_len + 1);
    if (copy_from_user(buf, priv_cmd.buf, priv_cmd.total_len + 1)) {
        printke("copy failed from user space\n");
        kfree(buf);
        return -EFAULT;
    } 

    ptr = strchr(buf, ' ');
    if (ptr == NULL) {
        printke("Command format error!\n");
        kfree(buf);
        return -1;
    }

    *ptr = 0; 
    ptr++;

    if ((strcmp(buf, "COUNTRY") == 0) &&
                    (strlen(ptr) == 2)) {
        if (strcmp(ptr, "US") == 0 || strcmp(ptr, "CU") == 0) {
            rd_size = sizeof(wlan_ieee80211_regdomain) + sizeof(struct ieee80211_reg_rule);
            wlan_cmd_set_regdom(0, (unsigned char *)(&wlan_regdom_11), rd_size);
			wiphy_apply_custom_regulatory(vif->wdev.wiphy, &ieee80211_regdom_11);
        }else{
            rd_size = sizeof(wlan_ieee80211_regdomain) + 2 * sizeof(struct ieee80211_reg_rule);
            wlan_cmd_set_regdom(0, (unsigned char *)(&wlan_regdom_12_13), rd_size);
			wiphy_apply_custom_regulatory(vif->wdev.wiphy, &ieee80211_regdom_12_13);
        }
    }else{
        printke("Key words not support!\n");
        kfree(buf);
        return -1;
    }
    
    kfree(buf);
    return 0;
}

static int wlan_ioctl(struct net_device *ndev, struct ifreq *req, int cmd)
{
	struct wlan_vif_t *vif = netdev_priv(ndev);
	struct iwreq *wrq = (struct iwreq *)req;

	switch (cmd) {
	case SIOCDEVPRIVATE + 1:
		return marlin_priv_cmd(ndev, req);
	case SIOCDEVPRIVATE + 2:
		return wlan_send_common_data_to_ic(ndev, req);
	case SIOCDEVPRIVATE + 3:
		return marlin_priv_cmd(ndev, req);
	case SIOCDEVPRIVATE + 4: 
        return wlan_set_suspend_mode(ndev, req);
	case SIOCDEVPRIVATE + 5:
        return wlan_set_country_code(ndev, req);
	default:
		printke("ioctl cmd %d is not supported\n", cmd);
		return -ENOTSUPP;
	}
	return 0;
}

struct net_device_ops wlan_ops = 
{
	.ndo_open                 = wlan_open,
	.ndo_stop                 = wlan_close,
	.ndo_start_xmit           = wlan_xmit,
	.ndo_get_stats            = wlan_status,
	.ndo_tx_timeout           = wlan_tx_timeout,
	.ndo_do_ioctl             = wlan_ioctl,
};

static void wlan_early_suspend(struct early_suspend *es)
{
	printkd("[%s]\n", __func__);
	wlan_cmd_sleep(1);
}

static void wlan_late_resume(struct early_suspend *es)
{
	printkd("[%s]\n", __func__);
	wlan_cmd_sleep(2);
}

void wlan_wakeup(void )
{
	if(0 != g_wlan.hw.wakeup)
		return;
	wake_lock(&g_wlan.hw.wlan_lock);
	printkd("time[1]\n");
	mod_timer(&(g_wlan.hw.wakeup_timer),  jiffies + msecs_to_jiffies(g_wlan.hw.wakeup_time) );
	g_wlan.hw.wakeup    = 1;
	g_wlan.hw.can_sleep = 0;
}

void wlan_sleep(void )
{
	if( (1 != g_wlan.hw.wakeup) || (0 == g_wlan.hw.can_sleep) )
		return;
	g_wlan.hw.wakeup = 0;
	printkd("time[0]\n");
	wake_unlock(&g_wlan.hw.wlan_lock);
	return;
}

void wakeup_timer_func(unsigned long data)
{
	if((g_wlan.wlan_trans.sem.count <= 0) && (0 == g_wlan.hw.can_sleep) )
	{
		printkd("timer[3]\n");
		g_wlan.hw.can_sleep = 1;
		trans_up();
		return;
	}
	printkd("timer[2]\n");
	mod_timer(&(g_wlan.hw.wakeup_timer),  jiffies + msecs_to_jiffies(g_wlan.hw.wakeup_time) );
}

int wlan_rx(rxfifo_t *rx_fifo, int cnt)
{
	int i,num,rx_cnt,ret;
	rx_cnt = 0;
	num = rx_fifo_used(rx_fifo);
	if(num > cnt)
		num = cnt;
	for(i=0; i < num ; i++)	
	{
		ret = rx_fifo_out(rx_fifo, wlan_rx_process);
		if(ERROR == ret)
			break;
		rx_cnt++;
	}
	return rx_cnt;
}

int wlan_tx(txfifo_t *tx_fifo, msg_q_t *msg_q, int cnt)
{
	int tx_cnt,i,ret = -1;
	tx_msg_t  *msg;
	tx_cnt = 0;
	for(i=0; i<cnt; i++)
	{
		msg = msg_get(msg_q);
		if(NULL == msg)
		{
			break;
		}
		ret = tx_fifo_in(tx_fifo, msg);
		if(TX_FIFO_FULL == ret)
		{
			// ret = TX_FIFO_FULL;
			break;
		}
		trans_up();
		if( HOST_SC2331_CMD == msg->hdr.type) 
		{
			if((msg->slice[0].len > 0) &&(NULL != msg->slice[0].data))
				kfree(msg->slice[0].data);
		}
		else if(HOST_SC2331_PKT == msg->hdr.type)
		{
			if((NULL != msg->p))
				dev_kfree_skb((struct sk_buff  *)(msg->p));
		}
		else
		{}
		
		memset((unsigned char *)msg,  0,  sizeof(tx_msg_t));
		msg_free(msg_q, msg);
		tx_cnt++;
	}
	return tx_cnt ? tx_cnt : ret;
}

#define WLAN_CORE_SLEEP_TIME	600
void thread_sched_policy(wlan_thread_t *thread)
{
	int ret;
	struct sched_param param;
	param.sched_priority = thread->prio;
	ret = sched_setscheduler(current, SCHED_FIFO,   &param);
	printkd("sched_setscheduler, prio:%d,ret:%d\n", param.sched_priority, ret);
	return;
}

void thread_sleep_policy(wlan_thread_t *thread)
{
	if(thread->null_run > thread->max_null_run)
	{
		usleep_range(thread->idle_sleep - 50, thread->idle_sleep + 50);
		//thread->null_run = 0;
	}
	return;
}

static int wlan_core_thread(void *data)
{
	int            i,j,ret,retry,vif_id, done, sem_count,q_index,need_tx,tmp,sleep_flag,ack_retry;
	unsigned long  timeout;
	wlan_vif_t    *vif;	
	rxfifo_t      *rx_fifo;
	txfifo_t      *tx_fifo;
	wlan_thread_t *thread;
	msg_q_t       *msg_q;
	
	thread = &(g_wlan.wlan_core);
	thread->null_run     = 0;
	thread->max_null_run = 200;
	thread->idle_sleep   = 300;
	thread->prio         = 90;
	sleep_flag = timeout = 0;
	atomic_set(&(thread->retry), 0);
	rx_fifo = &(g_wlan.rxfifo);
	printke("%s enter new\n", __func__);
	up(&(g_wlan.sync.sem));
	core_down();
	do
	{
		ack_retry = retry = done = 0;
		sem_count = g_wlan.wlan_core.sem.count ;

		if(thread->null_run > thread->max_null_run)
		{
			usleep_range(thread->idle_sleep - 50, thread->idle_sleep + 50);
		}
		ret    = wlan_rx(rx_fifo, 1);
		if(1 == ret)
		{
			done++;
			thread->done_rx++;
		}
		for(vif_id = NETIF_0_ID; vif_id < WLAN_MAX_ID; vif_id++)
		{
			vif     = &(g_wlan.netif[vif_id]);
			tx_fifo = &(vif->txfifo);
			
			msg_q = &(vif->msg_q[MSG_Q_ID_4]);
			if( (msg_num(msg_q) > 0) && ( ret = wlan_tx(tx_fifo, msg_q, msg_num(msg_q)) > 0) )
				thread->done_tx += ret;
			ret = wlan_tcpack_tx(vif, &tmp);
			if(OK != ret)
				ack_retry = 1;
			done            += tmp;
			thread->done_tx += tmp;
			
			qos_sched(&(vif->qos), &msg_q, &need_tx);
			
				for(i=0,j=0; i<need_tx; i++)
				{
					ret = wlan_tx(tx_fifo, msg_q, 1);
					if (TX_FIFO_FULL == ret) 
					{
						if (0 == sleep_flag) 
						{
							timeout = jiffies + msecs_to_jiffies(WLAN_CORE_SLEEP_TIME);
							sleep_flag = 1;
						} 
						else 
						{
							if (time_after(jiffies, timeout)) 
							{
								printke("%s [TIMEOUT][%lu] jiffies:%lu\n",__func__, timeout, jiffies);
								msleep(300);
								sleep_flag = 0;
							}
						}
						retry++;
						continue;
					}
					else if (TX_FIFO_EMPTY == ret) 
					{
						/* no problem as need_tx, excpt get_event erro */
						ASSERT();
						msleep(10);
						retry++;
						continue;
					}
					else
				{
					q_index = vif->qos.index;
					vif->qos.going[q_index]--;
					vif->qos.count[q_index][1]++;
					//eth_printk("get(%d,%d,%d)", q_index, vif->qos.count[q_index][0], vif->qos.count[q_index][1]);
				}
					if (sleep_flag)
						sleep_flag = 0;
					done += ret;
					thread->done_tx += ret;
					j++;
					if(j >= 6)
					{
						ret   = wlan_rx(rx_fifo, 1);
						done  = done + ret;
						thread->done_rx += ret;
						j=0;
					}
				}
			}
			if( true == vif->flow_ctrl_set )
			{
				for(i=0; i<MSG_Q_MAX_ID; i++)
				{
					if (msg_num(&(vif->msg_q[i])) > (vif->msg_q[i].num * 2 / 10))
						break;
				}

				if (i == MSG_Q_MAX_ID)
					wake_net(vif->id);
			}
		if(done > 0)
			thread->null_run= 0;
		else
			thread->null_run++;
		if(ack_retry != 1)
			core_try_down(retry);
		
		if (g_wlan.wlan_core.exit_flag)
			break;
	}while(!kthread_should_stop());
	
	printke("%s exit!\n", __func__);
	atomic_set(&(g_wlan.wlan_core.exit_status), 1);
	return 0;
}

static int check_valid_chn(int flag, unsigned short status, sdio_chn_t *chn_info)
{
	int i,chn = -1;

	if(1 == flag)
		status = ( status & (chn_info->bit_map) );	
	else
		status = ( (status & chn_info->bit_map) ^ (chn_info->bit_map) );
	if(0 == status)
		return -1;
	if(1 == flag)
	{
		if ( status & 0x300 )
		{
			switch( status & 0x300 )
			{
			case 0x300:
				if(8 == g_wifi_chn)
					chn = 9;
				else
					chn = 8;
				break;
			case 0x200:
				chn = 9;
				break;				
			case 0x100:
				chn = 8;
				break;
			default:
				break;
			}
			if(chn == g_wifi_chn)
				printke(" wifi_chn order err\r\n");
			return chn;
		}
		for (i = 2; i < chn_info->num; i++)
		{
			if (status & (0x1 << chn_info->chn[i])) 
			{
				chn = chn_info->chn[i];
				return chn;
			}
		}
	}
	else
	{
	for(i=0; i < chn_info->num; i++)
	{
		if( status & (0x1 << chn_info->chn[i]) )
		{
				chn = chn_info->chn[i];
			break;
			}
		}
	}
	return chn;
}

void sdio_info_dump(void )
{
	int ret;
	unsigned short chn = 0;
	ret = sdio_chn_status(0xFFFF, &chn);
	if (0 == ret)
		printke("[sdio_reg:0x%x][gpio:%d]\r\n", chn, gpio_get_value(SDIO_RX_GPIO) );
	else
		printke("sdio_chn_status error:%d\n", ret);
	return;
}

static void wlan_tx_chn_flush(unsigned char  mode)
{
	int ret,      flag = 0;
	unsigned char *mem = NULL;
	unsigned short reg = 0;
	t_msg_hdr_t   *msg;
	tx_big_hdr_t  *big_hdr;
	unsigned long  timeout;

	mem = kmalloc(68, GFP_KERNEL);
	big_hdr = mem;
	msg     = mem + 64;

	msg->type = HOST_SC2331_CMD;
	msg->mode = 0;
	msg->subtype = WIFI_CMD_SDIO_CHN_FLUSH;
	msg->len = 0;

	big_hdr->mode = mode;
	big_hdr->msg_num =1;
	big_hdr->len = 68;
	big_hdr->tx_cnt = 0;
	memcpy((char *)&(big_hdr->msg[0]), (unsigned char *)msg, sizeof(t_msg_hdr_t) );
	ret = set_marlin_wakeup(0, 1);
	if (0 != ret)
	{
		printkd("[%s][set_marlin_wakeup ret:%d]\n",__func__, ret);
		goto EXIT;
	}
	ret = sdio_chn_status(0x08, &reg);
	if (ret)
	{
		printkd("[%s][sdio_chn_status ret:%d]\n",__func__, ret);
		goto EXIT;
	}
	reg = ( (reg & 0x08) ^ 0x08 );
	if (0 == reg)
	{
		printkd("[%s][channel 3 busy]\n", __func__);
		goto EXIT;
	}
	ret = sdio_dev_write(3, mem, 68);
	if (0 != ret)
	{
		printkd("[%s][sdio_dev_write ret:%d]\n",__func__, ret);
		goto EXIT;
	}
	timeout = jiffies + msecs_to_jiffies(1000);
	while(1)
	{
		ret = sdio_chn_status(0x08, &reg);
		if( 0 == (reg & 0x08) )
		{
			flag = 1;
			break;
		}
		if(1 == g_wlan.sync.exit)
			break;
		if (  time_after(jiffies, timeout) )
		{
			sdio_chn_status(0xFF, &reg);
			printke("[%s][timeout,chn:0x%x]\n", __func__, reg );
			break;
		}
		usleep_range(150, 200);
	}
EXIT:
	kfree(mem);
	if(1 == flag)
		printkd("[%d][ok]\n",    __func__);
	else
		printkd("[%d][ERROR]\n", __func__);
	return;
}

static int wlan_trans_thread(void *data)
{
	int i,vif_id,ret,done, retry,sem_count,send_pkt, index, wake_flag, gpio_status;
	rxfifo_t       *rx_fifo;
	txfifo_t       *tx_fifo;
	wlan_vif_t     *vif;	
	sdio_chn_t     *tx_chn;
	sdio_chn_t     *rx_chn;
	unsigned short  status;
	wlan_thread_t  *thread;
	u32 rx_gpio;
	
	thread = &(g_wlan.wlan_trans);
	sdiodev_readchn_init(8, (void *)wlan_rx_chn_isr, 1);
	sdiodev_readchn_init(9, (void *)wlan_rx_chn_isr, 1);	
	rx_chn       = &(g_wlan.hw.sdio_rx_chn);
	tx_chn       = &(g_wlan.hw.sdio_tx_chn);
	rx_fifo      = &(g_wlan.rxfifo);
	rx_gpio = g_wlan.hw.rx_gpio;
	up(&(g_wlan.sync.sem));
	printke("%s enter\n", __func__);

	thread->null_run     = 0;
	thread->max_null_run = 100;
	thread->idle_sleep   = 400;
	thread->prio         = 90;
	wake_flag            = 0;
	thread_sched_policy(thread);
	trans_down();
	do
	{
		thread_sleep_policy(thread);
		send_pkt  = retry = done = 0;
		sem_count = g_wlan.wlan_trans.sem.count;

RX:
		gpio_status = gpio_get_value(rx_gpio);
		if(!  gpio_status)
		{
			if(true == rx_chn->gpio_high)
			{
				rx_chn->gpio_high    = false;
				rx_chn->timeout_flag = false;
			}
			goto TX;
		}
		else
		{
			if(false == rx_chn->gpio_high)
			{
				rx_chn->gpio_high    = true;
			}
		}
		wlan_wakeup();
		ret = set_marlin_wakeup(0, 1);
		if (0 != ret) {
			if( (ITM_NONE_MODE != g_wlan.netif[0].mode) || (ITM_NONE_MODE != g_wlan.netif[1].mode) )
			{
				if(-2 != ret)
				{
					printke("rx call set_marlin_wakeup return:%d:%d\n", ret);
					msleep(200);
					goto TX;
				}
			}else{
				printke("rx retry open wlan\n", ret);
				msleep(200);
				goto TX;
			}
		}
		ret   = sdio_chn_status( rx_chn->bit_map, &status);
		if(0 != ret)
		{
			printke("rx call sdio_chn_status error:%d\n", ret);
			goto RX_SLEEP;
		}
		index = check_valid_chn(1, status, rx_chn);
		if(index < 0)
		{
			
RX_SLEEP:
			if(false == rx_chn->timeout_flag)
			{
				rx_chn->timeout_flag = true;
				rx_chn->timeout      = jiffies + msecs_to_jiffies(rx_chn->timeout_time); 
			}
			else
			{
				if ( time_after(jiffies, rx_chn->timeout) )
				{
					printke("[SDIO_RX_CHN][TIMEOUT][%lu] jiffies:%lu\n", rx_chn->timeout_time, jiffies);
					sdio_info_dump();
					msleep(300);
					rx_chn->timeout_flag = false;
				}
			}	
			goto TX;
		}
		if(true == rx_chn->timeout_flag)
		{
			rx_chn->timeout_flag = false;
		}
		if(14 == index)
		{
			mdbg_sdio_read();
			goto TX;
		}
		if(11 == index)
		{
			mdbg_at_cmd_read();
			goto TX;
		}
		if(15 == index)
		{
			mdbg_loopcheck_read();
			goto TX;
		}
		ret = rx_fifo_in(index, rx_fifo, hw_rx);
		if(OK != ret )
		{
			if(HW_READ_ERROR == ret)
				msleep(100);
			retry++;
			goto TX;
		}
		g_wlan.wlan_core.need_rx++;
		core_up();

TX:	
		for(vif_id = NETIF_0_ID; vif_id < WLAN_MAX_ID; vif_id++ )
		{
			vif     = &(g_wlan.netif[vif_id]);
			tx_fifo = &(vif->txfifo);
			ret = tx_fifo_used(tx_fifo);
			if(0 == ret)
				continue;
			wlan_wakeup();
			ret = set_marlin_wakeup(0, 1);
			if (0 != ret) {
				if( (ITM_NONE_MODE != g_wlan.netif[0].mode) || (ITM_NONE_MODE != g_wlan.netif[1].mode) )
				{
					// -2: means bt ack high
					if(-2 != ret){
						printke("tx call set_marlin_wakeup return:%d\n", ret);
						msleep(200);
						retry++;
						continue;
					}
				}else{
					printke("tx retry open wlan\n");
					msleep(300);
					retry++;
					continue;
				}
			}
			ret = sdio_chn_status(tx_chn->bit_map, &status);
			if (ret)
			{
				printke("tx call sdio_chn_status error:%d\n", ret);
				goto TX_SLEEP;
			}
			index = check_valid_chn(0, status, tx_chn);
			if(index < 0)
			{
TX_SLEEP:
				if(false == tx_chn->timeout_flag)
				{
					tx_chn->timeout_flag = true;
					tx_chn->timeout      = jiffies + msecs_to_jiffies(tx_chn->timeout_time); 
				} else {
					if (time_after
					    (jiffies, tx_chn->timeout)) {
						printke("[SDIO_TX_CHN][TIMEOUT][%lu] jiffies:%lu\n",tx_chn->timeout_time, jiffies);
						sdio_info_dump();
						wlan_tx_chn_flush(0);
						tx_chn->chn_timeout_cnt++;
						if(tx_chn->chn_timeout_cnt > 20)
						{
							printke("[SDIO_TX_CHN][ERROR][block more than 20 times][need reset CP2]\n");
							mdbg_assert_interface();
							tx_chn->chn_timeout_cnt = 0;
						}
						msleep(300);
						tx_chn->timeout_flag = false;
					}
				}	
				retry++;
				continue;
			}
			if ( (true == tx_chn->timeout_flag) || (tx_chn->chn_timeout_cnt > 0) ) {
				tx_chn->timeout_flag = false;
				tx_chn->chn_timeout_cnt = 0;
			}
			ret    = tx_fifo_out(vif_id, index, tx_fifo, hw_tx, &send_pkt);
			if(OK != ret)
			{
				if(HW_WRITE_ERROR == ret)
				{
					msleep(100);
					retry++;
				}
				continue;
			}
			done = done + send_pkt;
			core_try_up();
		}
		
		gpio_status = gpio_get_value(rx_gpio);
		if(gpio_status)
		{
			if(g_wlan.wlan_trans.sem.count - done <= 1)
			{
				done = (g_wlan.wlan_trans.sem.count > 0)?(g_wlan.wlan_trans.sem.count-1):(0); 
			}
		}
		else
		{
			if( (0 == done) && (0 == retry) )
				done = (  (0 == sem_count)?(1):(sem_count) );
		}
		if(done > 0)
			thread->null_run= 0;
		else
			thread->null_run++;
		wlan_sleep();
/*
		if ((done >= g_wlan.wlan_trans.sem.count) && (wake_flag = 1) &&(!gpio_status) )
		{
			wake_unlock(&g_wlan.hw.wlan_lock);
			wake_flag = 0;
		}
*/
		for(i=0; i<done; i++)
		{
			trans_down();
		}

		if (g_wlan.wlan_trans.exit_flag)
			break;
	}while(!kthread_should_stop());
	sdiodev_readchn_uninit(8);
	sdiodev_readchn_uninit(9);
	mdbg_sdio_read();
	del_timer_sync(&(g_wlan.hw.wakeup_timer));
	printke("%s exit\n", __func__);
	atomic_set(&(g_wlan.wlan_trans.exit_status), 1);
	core_up();
	return OK;
}

static int wlan_tx_buf_alloc(wlan_vif_t *vif)
{
	txfifo_conf_t  fifo_conf  = {0};
	int  ret,q_id;
	
	msg_q_alloc(&(vif->msg_q[MSG_Q_ID_0]), sizeof(tx_msg_t), 256);
	msg_q_alloc(&(vif->msg_q[MSG_Q_ID_1]), sizeof(tx_msg_t), 150);
	msg_q_alloc(&(vif->msg_q[MSG_Q_ID_2]), sizeof(tx_msg_t), 150);
	msg_q_alloc(&(vif->msg_q[MSG_Q_ID_3]), sizeof(tx_msg_t), 150);
	msg_q_alloc(&(vif->msg_q[MSG_Q_ID_4]), sizeof(tx_msg_t), 5);
	vif->qos.msg_q  = &(vif->msg_q[0]);
	fifo_conf.cp2_txRam   = HW_TX_SIZE - sizeof(tx_big_hdr_t);
	fifo_conf.max_msg_num = PKT_AGGR_NUM;
	fifo_conf.size        = 1024*256;
	ret =  tx_fifo_alloc(&(vif->txfifo), &fifo_conf);
	if(ERROR == ret)
		return ERROR;
	return OK;
}

static int wlan_tx_msg_q_free(wlan_vif_t *vif)
{
	int         q_id, num,i;
	msg_q_t    *msg_q;
	tx_msg_t   *msg;	
	for(q_id=0; q_id<MSG_Q_MAX_ID; q_id++)
	{
		msg_q = &(vif->msg_q[q_id]);
		num   = msg_num(msg_q);
		for(i=0; i<num; i++)
		{
			msg = msg_get(msg_q);
			if(NULL == msg)
				break;
			msg_free(msg_q, msg);
		}
		msg_q_free(msg_q);
	}
	tx_fifo_free(&(vif->txfifo));
	return OK;
}

static int wlan_rx_buf_alloc(void)
{
	int ret;
	rxfifo_t *rx_buf = &(g_wlan.rxfifo);
	ret = rx_fifo_alloc(rx_buf);
	return ret;
}

static int wlan_rx_buf_free(void )
{
	int ret;
	rxfifo_t *rx_buf = &(g_wlan.rxfifo);
	ret = rx_fifo_free(rx_buf);
	return ret;
}

static int wlan_hw_init(hw_info_t *hw)
{
	struct device_node *np;
	memset(hw, 0, sizeof(hw_info_t) );
	
	hw->sdio_tx_chn.num          = 3;
	hw->sdio_tx_chn.chn[0]       = 0;
	hw->sdio_tx_chn.chn[1]       = 1;
	hw->sdio_tx_chn.chn[2]       = 2;
	hw->sdio_tx_chn.bit_map      = 0x0007;
	hw->sdio_tx_chn.timeout_time = 600;
	hw->sdio_tx_chn.timeout_flag = false;

	hw->sdio_rx_chn.num          = 6;
	hw->sdio_rx_chn.chn[0]       = 8;
	hw->sdio_rx_chn.chn[1]       = 9;
	hw->sdio_rx_chn.chn[2]       = 14;
	hw->sdio_rx_chn.chn[3]       = 11;
	hw->sdio_rx_chn.chn[4]       = 15;
	hw->sdio_rx_chn.chn[5]       = 13;
	hw->sdio_rx_chn.bit_map      = 0xeb00;
	hw->sdio_rx_chn.gpio_high    = false;	
	hw->sdio_rx_chn.timeout_time = 600;
	hw->sdio_rx_chn.timeout_flag = false;

	np = of_find_node_by_name(NULL, "sprd-marlin");
	if (!np) {
		printke("sprd-marlin not found");
		return -1;
	}
	hw->rx_gpio = of_get_gpio(np, 1);

	printke("[SDIO_TX_CHN][0x%x][0x%x]\n", hw->sdio_tx_chn.bit_map, HW_TX_SIZE);
	printke("[SDIO_RX_CHN][0x%x][0x%x]\n", hw->sdio_rx_chn.bit_map, HW_RX_SIZE);
	
	hw->wakeup = 0;
	spin_lock_init(&(hw->sdio_rx_chn.lock));
	wake_lock_init(&g_wlan.hw.wlan_lock, WAKE_LOCK_SUSPEND, "wlan_sc2331_lock");
	init_timer(&g_wlan.hw.wakeup_timer);
	g_wlan.hw.wakeup_timer.function = wakeup_timer_func;
	g_wlan.hw.wakeup_time  = 500;
	return OK;
}
static int wlan_inetaddr_event(struct notifier_block *this,
			      unsigned long event, void *ptr)
{
	unsigned  char      vif_id;
	wlan_vif_t          *vif;
	struct net_device   *dev;
	printkd("inetaddr callback is comming in !\n");

	struct in_ifaddr *ifa = (struct in_ifaddr *)ptr;

	dev = ifa->ifa_dev ? ifa->ifa_dev->dev : NULL;

	if((dev != (id_to_vif(0)->ndev)) && (dev != (id_to_vif(1)->ndev)))
	{
		printkd("dev id not equal to 0 or 1!\n");
		goto done;
	}	

	if (dev == NULL)
		goto done;
	printkd(" inetaddr dev not equal to null !\n");

	vif = ndev_to_vif(dev);
	vif_id = vif->id;

	if (!vif)
		goto done;
	printkd("inetaddr vif not equal to null !\n");

	switch (event) {
	case NETDEV_UP:
	printkd("inetaddr UP event is comming in !\n");
		wlan_cmd_get_ip(vif_id, (u8 *) & ifa->ifa_address);
		break;
	case NETDEV_DOWN:
	printkd("inetaddr DOWN event is comming in  !\n");
		break;
	default:
	printkd("inetaddr defaut is comming in !\n");
		break;
	}

done:
	return NOTIFY_DONE;
}

static struct notifier_block itm_inetaddr_cb = {
	.notifier_call = wlan_inetaddr_event,
};

static ssize_t wlan_proc_write(struct file *file, const char __user *buffer, size_t count, loff_t *pos)
{
	int id, cmd, value;
	char kbuf[32] = {0};
	if (copy_from_user(kbuf, buffer, ( (count <= 32)?(count):(32) ) )  )
		return -EFAULT;
	sscanf(kbuf, "%d %d %d\n", &id, &cmd, &value);
	
	printke("[%s][%d][%d][%d]\n", __func__,id,cmd, value);
	switch (id)
	{
	case 1:
		SET_BIT(g_dbg, cmd);
		break;
	case 2:
		CLEAR_BIT(g_dbg, cmd);
		break;	
	case 3:
		if( (0 != cmd) && (1 != cmd) )
			break;
		if( (0 != value) && (1 != value) )
			break;
		printke("set wlan_qos->enable[%d][%d]\r\n", cmd, value);
		qos_enable(&g_wlan.netif[cmd].qos, value);
		break;
	default:
		break;
	}
	return count;
}

static int wlan_proc_open(struct inode *inode, struct file *filp)
{
	return 0;
}

static int wlan_proc_release(struct inode *inode, struct file *filp)
{
	return 0;
}

static int wlan_proc_ioctl(struct file *filp, unsigned int cmd, unsigned long arg)
{
	int ret;
	unsigned char r_buf[128] = {0};
	unsigned char s_buf[32] = {0};
	printkd("[%s][cmd][%d]\n", __func__, cmd);
	if (copy_from_user (r_buf, (unsigned char *)arg, 32 ) )
	{
		printke("[%s][err]\n", __func__);
		return -EFAULT;
	}
	switch (cmd) 
	{
	case 0x10:
		g_wlan.sync.cp2_status = 2;
		g_wlan.sync.exit = 1;
		g_wlan.cmd.wakeup = 1;
		wake_up(&g_wlan.cmd.waitQ);
		wiphy_rfkill_set_hw_state(g_wlan.wiphy, 1);
		wlan_thread_exit();
		printke("[CP2][ASSERT]\n");
		if(2 != g_wlan.sync.drv_status)
			break;
		ret = 0x11;
		memcpy(&s_buf[0], (unsigned char *)(&ret), 4);
		ret = copy_to_user( (unsigned char *)arg, &s_buf[0], 4 );
		if (0 != ret)
		{
			printkd("[%s][%d][err]\n", __func__, __LINE__);
			return -EFAULT;
		}
		break;
	default:
		break;
	}
	return 0;
}

static const struct file_operations wlan_proc_fops = 
{
	.owner   = THIS_MODULE,
	.write   = wlan_proc_write,
	.unlocked_ioctl = wlan_proc_ioctl,
	.open    = wlan_proc_open,
	.release = wlan_proc_release,
};

static const struct file_operations lte_concur_proc_fops = 
{
	.owner		= THIS_MODULE,
	.unlocked_ioctl  = lte_concur_proc_ioctl,
	.open       = lte_concur_proc_open,
	.release    = lte_concur_proc_release,
};

int wlan_module_init(struct device *dev)
{
	int ret;

	printke("[%s] [ version:0x28 ] [ time(%s %s) ][(20160107)(for tpt & fix BUG_ON)]\n", __func__,
		__DATE__,__TIME__);
	if(NULL == dev)
		return -EPERM;
	ret = get_sdiohal_status();
	if(1 != ret)
	{
		printke("######## %s sdio is not ready  ##########\n", __func__);
		return -EPERM;
	}
	else
	{
		printke("sdio is ready !!!\n");
	}
	g_wlan.sync.cp2_status = 0;
	g_wlan.sync.drv_status = 0;
	//marlin_pa_enable(true);
	memset((unsigned char *)(&g_wlan), 0, sizeof(wlan_info_t));
	g_wlan.dev = dev;
	g_wlan.netif[NETIF_0_ID].id= NETIF_0_ID;
	g_wlan.netif[NETIF_1_ID].id= NETIF_1_ID;
	g_wlan.sync.exit = 0;

	sema_init(&g_wlan.sync.sem, 0);
	wlan_hw_init(&(g_wlan.hw));

	wlan_tx_buf_alloc(&(g_wlan.netif[NETIF_0_ID]));
	wlan_tx_buf_alloc(&(g_wlan.netif[NETIF_1_ID]));
	
	g_wlan.netif[NETIF_0_ID].tcp_ack_suppress = true;
	g_wlan.netif[NETIF_1_ID].tcp_ack_suppress = true;
	g_wlan.netif[NETIF_0_ID].flow_ctrl_enable      = true;
	g_wlan.netif[NETIF_0_ID].flow_ctrl_set         = false;
	g_wlan.netif[NETIF_1_ID].flow_ctrl_enable      = true;
	g_wlan.netif[NETIF_1_ID].flow_ctrl_set         = false;
	wlan_tcpack_buf_malloc(&(g_wlan.netif[NETIF_0_ID]));
	wlan_tcpack_buf_malloc(&(g_wlan.netif[NETIF_1_ID]));
	ret = wlan_rx_buf_alloc();
	if(OK != ret)
		return -EPERM;
	wlan_cmd_init();
	sema_init(&g_wlan.wlan_trans.sem, 0);
	sema_init(&g_wlan.wlan_core.sem, 0);

	atomic_set(&g_wlan.wlan_trans.exit_status, 0);
	atomic_set(&g_wlan.wlan_core.exit_status, 0);

	g_wlan.wlan_trans.exit_flag = 0;
	g_wlan.wlan_core.exit_flag = 0;
	
	ret = wlan_wiphy_new(&(g_wlan));
	if(OK != ret)
		return -EPERM;
	ret = wlan_vif_init(&(g_wlan.netif[NETIF_0_ID]), NL80211_IFTYPE_ADHOC,       "wlan0",    (void *)(&wlan_ops) );
	if(OK != ret)
		return -EPERM;
	ret = wlan_vif_init(&(g_wlan.netif[NETIF_1_ID]), NL80211_IFTYPE_P2P_DEVICE,  "p2p0",     (void *)(&wlan_ops) );
	if(OK != ret)
		return -EPERM;

	g_wlan.wlan_trans.task = kthread_create(wlan_trans_thread,
					       (void *)(dev), "wlan_trans");
	if (NULL != g_wlan.wlan_trans.task)
		wake_up_process(g_wlan.wlan_trans.task);
	down(&(g_wlan.sync.sem));

	g_wlan.wlan_core.task  = kthread_create(wlan_core_thread,
					       (void *)(dev), "wlan_core");
	if (NULL != g_wlan.wlan_core.task)
		wake_up_process(g_wlan.wlan_core.task);
	down(&(g_wlan.sync.sem));
	wlan_nl_init();
	
	g_wlan.hw.early_suspend.suspend  = wlan_early_suspend;
	g_wlan.hw.early_suspend.resume   = wlan_late_resume;
	g_wlan.hw.early_suspend.level    = EARLY_SUSPEND_LEVEL_BLANK_SCREEN - 1;	
	register_early_suspend(&g_wlan.hw.early_suspend);
	
	ret = register_inetaddr_notifier(&itm_inetaddr_cb);
	if (ret) {
		printke("Couldn't register inetaddr notifier \n");
	}
	if (!proc_create("wlan", 0666, NULL, &wlan_proc_fops)) 
	{
		printke("Couldn't create the /proc/wlan \n");
	}
	if (!proc_create("lte_concur", 0666, NULL, &lte_concur_proc_fops))
	{
		printke("Couldn't create the /proc/lte_concur \n");
	}	
	g_dbg = 0x0;
	SET_BIT(g_dbg,1);
	g_wlan.sync.drv_status = 1;
	g_wlan.sync.cp2_status = 1;	
	printke("%s ok!\n", __func__);
	return OK;
}
EXPORT_SYMBOL_GPL(wlan_module_init);

int wlan_module_exit(struct device *dev)
{
	printke("%s enter\n", __func__);
	g_wlan.sync.drv_status = 2;
	unregister_inetaddr_notifier(&itm_inetaddr_cb);
	//marlin_pa_enable(false);
	if(1 == g_wlan.sync.cp2_status)
	{
		if (ITM_NONE_MODE != g_wlan.netif[NETIF_0_ID].mode)
			wlan_cmd_mac_close(NETIF_0_ID, g_wlan.netif[NETIF_0_ID].mode);
		if (ITM_NONE_MODE != g_wlan.netif[NETIF_1_ID].mode)
			wlan_cmd_mac_close(NETIF_1_ID, g_wlan.netif[NETIF_1_ID].mode);
	}
	else
	{
		g_wlan.sync.exit = 1;
		g_wlan.cmd.wakeup = 1;
		wake_up(&g_wlan.cmd.waitQ);
	}
	g_wlan.sync.exit = 1;
	wlan_thread_exit();
	wlan_vif_free(&(g_wlan.netif[NETIF_0_ID]));
	wlan_vif_free(&(g_wlan.netif[NETIF_1_ID]));
	wlan_wiphy_free(&g_wlan);
	wlan_cmd_deinit();
	wlan_tx_msg_q_free(&(g_wlan.netif[NETIF_0_ID]));
	wlan_tx_msg_q_free(&(g_wlan.netif[NETIF_1_ID]));
	wlan_tcpack_buf_free(&(g_wlan.netif[NETIF_0_ID]));
	wlan_tcpack_buf_free(&(g_wlan.netif[NETIF_1_ID]));	
	wlan_rx_buf_free();
	wlan_nl_deinit();
	remove_proc_entry("wlan", NULL);
	remove_proc_entry("lte_concur", NULL);
	unregister_early_suspend(&g_wlan.hw.early_suspend);
	wake_lock_destroy(&g_wlan.hw.wlan_lock);
	g_wlan.sync.drv_status = 3;
	printke("%s ok!\n", __func__);
	return OK;
}
EXPORT_SYMBOL_GPL(wlan_module_exit);

static int  sprd_wlan_probe(struct platform_device *pdev)
{
#ifdef CONFIG_WCN_EXTENSION
	printke("CONFIG_WCN_EXTENSION is defined!\n");
#else
	printke("CONFIG_WCN_EXTENSION is not defined!\n");
#endif
	
	return wlan_module_init(&(pdev->dev));
}

static int  sprd_wlan_remove(struct platform_device *pdev)
{
	return wlan_module_exit(&(pdev->dev));
}

#define DEVICE_NAME "sc2331"
static struct platform_device *sprd_wlan_device;
static struct platform_driver  sprd_wlan_driver =
{
	.probe =   sprd_wlan_probe,
	.remove =  sprd_wlan_remove,
	.driver = 
	{
		.owner = THIS_MODULE,
		.name = DEVICE_NAME,
	},
};
static int  sprd_wlan_init(void)
{
	sprd_wlan_device = platform_device_register_simple(DEVICE_NAME, 0, NULL, 0);
	if (IS_ERR(sprd_wlan_device))
		return PTR_ERR(sprd_wlan_device);
	return platform_driver_register(&(sprd_wlan_driver));
}

static void  sprd_wlan_exit(void)
{
	platform_driver_unregister(&sprd_wlan_driver);
	platform_device_unregister(sprd_wlan_device);
	sprd_wlan_device = NULL;
}

module_init(sprd_wlan_init);
module_exit(sprd_wlan_exit);
MODULE_DESCRIPTION("SPRD sc2331 Wireless Network Adapter");
MODULE_AUTHOR("jinglong.chen");
MODULE_LICENSE("GPL");
MODULE_VERSION("1.0");


