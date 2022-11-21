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
	if (thread->need_rx != thread->done_rx)
		return;
	if ((thread->need_tx != thread->done_tx) && (retry == 0))
		return;
	atomic_set(&(thread->retry), retry);
	sem_count = g_wlan.wlan_core.sem.count;
	do {
		if(g_wlan.sync.exit)
			break;
		down(&(g_wlan.wlan_core.sem));
	}
	while (sem_count--);
	return;
}

void core_try_up(void)
{
	if (atomic_read(&g_wlan.wlan_core.retry) > 0) {
		atomic_dec(&g_wlan.wlan_core.retry);
		up(&(g_wlan.wlan_core.sem));
	}
}

void trans_up(void)
{
	up(&(g_wlan.wlan_trans.sem));
}

void trans_down(void)
{
	down(&(g_wlan.wlan_trans.sem));
}

bool stop_net(unsigned char id)
{
	return false;
	if (NULL == g_wlan.netif[id].ndev)
		return false;
	if (!netif_queue_stopped(g_wlan.netif[id].ndev)) {
		netif_stop_queue(g_wlan.netif[id].ndev);
		return true;
	}
	return false;
}

bool wake_net(unsigned char id)
{
	return false;
	if ((NULL == g_wlan.netif[id].ndev) || (1 == g_wlan.sync.exit))
		return false;
	if (netif_queue_stopped(g_wlan.netif[id].ndev)) {
		netif_wake_queue(g_wlan.netif[id].ndev);
		return true;
	}
	return false;
}

static int hw_rx(const unsigned short chn, unsigned char *buf,
		 unsigned int *len)
{
	int ret;
	unsigned int read_len = 0;
	static unsigned int cnt;
	if (NULL == buf)
		return ERROR;
	ret = sdio_dev_read(chn, buf, &read_len);
	*len = read_len;
	if (0 != ret) {
		printke("call sdio_dev_read err:%d\n", ret);
		return HW_READ_ERROR;
	}
	printkp("[rx][%d]\n", chn);
	wlan_rx_buf_decode(buf, read_len);
	cnt++;
	*len = read_len;
	g_wlan.hw.rx_cnt++;
	return OK;
}

static int hw_tx(const unsigned short chn, unsigned char *buf, unsigned int len)
{
	int ret;
	static unsigned int cnt;
	static unsigned int skb;
	tx_big_hdr_t *big_hdr;

	big_hdr = (tx_big_hdr_t *) buf;
	if ((PKT_AGGR_NUM < big_hdr->msg_num) || (0 == big_hdr->msg_num)) {
		ASSERT();
		return ERROR;
	}
	big_hdr->tx_cnt = g_wlan.hw.tx_cnt;
	printkp("[tx][%d][%d]\n", big_hdr->tx_cnt, chn);
	len = (len + 1023) & 0xFC00;
	wlan_tx_buf_decode(buf, len);
	ret = sdio_dev_write(chn, buf, len);
	if (0 != ret) {
		printke("call sdio_dev_write err:%d\n", ret);
		return HW_WRITE_ERROR;
	}
	skb = skb + big_hdr->msg_num;
	cnt++;
	if (1000 == cnt) {
		printkd("w%d\n", skb);
		cnt = 0;
		skb = 0;
	}
	g_wlan.hw.tx_cnt++;
	return OK;
}

static int wlan_rx_skb_process(const unsigned char vif_id, unsigned char *pData,
			       unsigned short len)
{
	struct sk_buff *skb;
	struct net_device *ndev = g_wlan.netif[vif_id].ndev;
	if ((NULL == pData) || (0 == len) || (NULL == ndev)) {
		printkd("[%s][%d][err]\n", __func__, (int)vif_id);
		return ERROR;
	}
	skb = dev_alloc_skb(len + NET_IP_ALIGN);
	if (NULL == skb)
		return ERROR;
	skb_reserve(skb, NET_IP_ALIGN);
	memcpy(skb->data, pData, len);

	skb_put(skb, len);
	skb->dev = ndev;
	skb->protocol = eth_type_trans(skb, ndev);
	ndev->stats.rx_packets++;
	printkp("rx_skb:%d\n", (int)(ndev->stats.rx_packets));
	ndev->stats.rx_bytes += skb->len;
	if (in_interrupt())
		netif_rx(skb);
	else
		netif_rx_ni(skb);
	return OK;
}

static int wlan_rx_wapi_process(const unsigned char vif_id,
				unsigned char *pData, unsigned short len)
{
	struct ieee80211_hdr_3addr *addr;
	int decryp_data_len = 0;
	struct sk_buff *skb;
	u8 snap_header[6] = { 0xaa, 0xaa, 0x03, 0x00, 0x00, 0x00 };
	wlan_vif_t *vif;
	struct net_device *ndev;

	vif = &(g_wlan.netif[vif_id]);
	ndev = vif->ndev;
	if ((NULL == pData) || (0 == len) || (NULL == ndev)) {
		printkd("[%s][%d][err]\n", __func__, (int)vif_id);
		return ERROR;
	}
	addr = (struct ieee80211_hdr_3addr *)pData;
	skb = dev_alloc_skb(len + NET_IP_ALIGN);
	if (NULL == skb)
		return ERROR;
	skb_reserve(skb, NET_IP_ALIGN);

	decryp_data_len =
	    wlan_rx_wapi_decryption(vif, (unsigned char *)addr, 24, (len - 24),
				    (skb->data + 12));
	if (decryp_data_len == 0) {
		dev_kfree_skb(skb);
		return ERROR;
	}
	if (memcmp((skb->data + 12), snap_header, sizeof(snap_header)) == 0) {
		skb_reserve(skb, 6);
		memcpy(skb->data, addr->addr1, 6);
		memcpy(skb->data + 6, addr->addr2, 6);
		skb_put(skb, (decryp_data_len + 6));
	} else {
		/* copy eth header */
		memcpy(skb->data, addr->addr3, 6);
		memcpy(skb->data + 6, addr->addr2, 6);
		skb_put(skb, (decryp_data_len + 12));
	}
	skb->dev = ndev;
	skb->protocol = eth_type_trans(skb, ndev);
	ndev->stats.rx_packets++;
	printkp("rx_skb:%d\n", (int)(ndev->stats.rx_packets));
	ndev->stats.rx_bytes += skb->len;
	if (in_interrupt())
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
	wlan_vif_t *vif;
	tx_msg_t msg = { 0 };
	msg_q_t *msg_q;
	int ret, addr_len = 0;
	struct sk_buff *wapi_skb;

	vif = ndev_to_vif(dev);
	msg_q = wlan_tcpack_q(vif, skb->data, skb->len);

	if (vif->cfg80211.cipher_type == WAPI
	    && vif->cfg80211.connect_status == ITM_CONNECTED
	    && vif->cfg80211.key_len[PAIRWISE][vif->cfg80211.
					       key_index[PAIRWISE]] != 0
	    && (*(u16 *) ((u8 *) skb->data + ETH_PKT_TYPE_OFFSET) != 0xb488)) {
		wapi_skb = dev_alloc_skb(skb->len + 100 + NET_IP_ALIGN);
		skb_reserve(wapi_skb, NET_IP_ALIGN);
		memcpy(wapi_skb->data, skb->data, ETHERNET_HDR_LEN);
		addr_len =
		    wlan_tx_wapi_encryption(vif, skb->data,
					    (skb->len - ETHERNET_HDR_LEN),
					    ((unsigned char *)(wapi_skb->data) +
					     ETHERNET_HDR_LEN));
		addr_len = addr_len + ETHERNET_HDR_LEN;
		skb_put(wapi_skb, addr_len);
		dev_kfree_skb(skb);
		skb = wapi_skb;
	}

	msg.p = (void *)skb;
	msg.slice[0].data = skb->data;
	msg.slice[0].len = skb->len;
	msg.hdr.mode = vif->id;
	msg.hdr.type = HOST_SC2331_PKT;
	msg.hdr.subtype = 0;
	msg.hdr.len = skb->len;

	ret = msg_q_in(msg_q, &msg);
	if (OK != ret) {
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

static int wlan_rx_process(unsigned char *buf, unsigned int max_len)
{
	static unsigned int cnt;
	static unsigned int skb;
	unsigned int p = 0;
	r_msg_hdr_t *msg = NULL;
	unsigned char vif_id;
	unsigned char *pData = NULL;
	unsigned short len;
	unsigned char event;
	if ((NULL == buf) || (0 == max_len)) {
		printke("[%s][ERROR]\n", __func__);
		return OK;
	}
	buf = buf + 8;
	msg = (r_msg_hdr_t *) (buf);
	max_len = max_len - 8;
	while (p < max_len) {

		vif_id = msg->mode;
		pData = (unsigned char *)(msg + 1);
		len = msg->len;

		if ((0xFF == msg->type) || (0xFF == msg->subtype))
			break;
		if (HOST_SC2331_PKT == msg->type) {
			pData = pData + msg->subtype;
			len = len - msg->subtype;
			wlan_rx_skb_process(vif_id, pData, len);
		} else if (HOST_SC2331_WAPI == msg->type) {
			wlan_rx_wapi_process(vif_id, pData, len);
		} else if (SC2331_HOST_RSP == msg->type) {
			wlan_rx_rsp_process(vif_id, msg);
		} else if (HOST_SC2331_CMD == msg->type) {
			event = msg->subtype;
			wlan_rx_event_process(vif_id, event, pData, len);
		} else {
			printke("[%s][RX DATA ERR]\n", __func__);
			break;
		}
		p = p + sizeof(t_msg_hdr_t) + ALIGN_4BYTE(msg->len);
		msg = (r_msg_hdr_t *) (buf + p);
		skb++;
	}
	cnt++;
	if (1000 == cnt) {
		printkd("r%d\n", skb);
		cnt = 0;
		skb = 0;
	}
	return OK;
}

static int wlan_open(struct net_device *dev)
{
	printkd("%s open\n", dev->name);
	netif_start_queue(dev);
	return 0;
}

static int wlan_close(struct net_device *dev)
{
	printkd("%s %s enter\n", __func__, dev->name);
	netif_stop_queue(dev);
	printkd("%s %s ok\n", __func__, dev->name);
	return 0;
}

static struct net_device_stats *wlan_status(struct net_device *dev)
{
	return &(dev->stats);
}

static void wlan_tx_timeout(struct net_device *dev)
{
	wlan_vif_t *vif;
	printke("%s tx timeout\n", dev->name);
	vif = ndev_to_vif(dev);

	if (!netif_carrier_ok(dev))
		netif_carrier_on(dev);
	dev->trans_start = jiffies;
	wake_net(vif->id);
	core_up();
	return;
}

#define CMD_BLACKLIST_ENABLE		"BLOCK"
#define CMD_BLACKLIST_DISABLE		"UNBLOCK"

int marlin_priv_cmd(struct net_device *ndev, struct ifreq *ifr)
{
	struct wlan_vif_t *vif;
	struct android_wifi_priv_cmd priv_cmd;
	char *command = NULL;
	u8 addr[ETH_ALEN] = {0};
	int bytes_written = 0;
	int ret = 0;

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
		sscanf(command + skip, "" MACSTR "", STR2MAC(addr));
		bytes_written = wlan_cmd_add_blacklist(vif, addr);
	} else if (strnicmp(command, CMD_BLACKLIST_DISABLE,
			    strlen(CMD_BLACKLIST_DISABLE)) == 0) {
		int skip = strlen(CMD_BLACKLIST_DISABLE) + 1;

		printke("%s, Received regular blacklist disable command\n",
			   __func__);
		sscanf(command + skip, "" MACSTR "", STR2MAC(addr));
		bytes_written = wlan_cmd_del_blacklist(vif, addr);
	}

	if (bytes_written < 0)
		ret = bytes_written;

exit:
	kfree(command);

	return ret;
}

static int wlan_ioctl(struct net_device *ndev, struct ifreq *req, int cmd)
{
	struct wlan_vif_t *vif = netdev_priv(ndev);
	struct iwreq *wrq = (struct iwreq *)req;

	switch (cmd) {
	case SIOCDEVPRIVATE + 1:
		return marlin_priv_cmd(ndev, req);
		break;
	default:
		printke("ioctl cmd %d is not supported\n", cmd);
		return -ENOTSUPP;
	}
	return 0;
}

struct net_device_ops wlan_ops = {
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

int wlan_wakeup(void)
{
	int ret;
	if (0 != g_wlan.hw.wakeup)
		return OK;
	wake_lock(&g_wlan.hw.wlan_lock);
	printkd("time[1]\n");
	mod_timer(&(g_wlan.hw.wakeup_timer),
		  jiffies + msecs_to_jiffies(g_wlan.hw.wakeup_time));
	g_wlan.hw.wakeup = 1;
	g_wlan.hw.can_sleep = 0;
	return ret;
}

void wlan_sleep(void)
{
	if ((1 != g_wlan.hw.wakeup) || (0 == g_wlan.hw.can_sleep))
		return;
	g_wlan.hw.wakeup = 0;
	printkd("time[0]\n");
	wake_unlock(&g_wlan.hw.wlan_lock);
	return;
}

void wakeup_timer_func(unsigned long data)
{
	if ((g_wlan.wlan_trans.sem.count <= 0) && (0 == g_wlan.hw.can_sleep)) {
		printkd("timer[3]\n");
		g_wlan.hw.can_sleep = 1;
		trans_up();
		return;
	}
	printkd("timer[2]\n");
	mod_timer(&(g_wlan.hw.wakeup_timer),
		  jiffies + msecs_to_jiffies(g_wlan.hw.wakeup_time));
}

int wlan_rx(rxfifo_t *rx_fifo, int cnt)
{
	int i, num, rx_cnt, ret;
	rx_cnt = 0;
	num = rx_fifo_used(rx_fifo);
	if (num > cnt)
		num = cnt;
	for (i = 0; i < num; i++) {
		ret = rx_fifo_out(rx_fifo, wlan_rx_process);
		if (ERROR == ret)
			break;
		rx_cnt++;
	}
	return rx_cnt;
}

int wlan_tx(txfifo_t *tx_fifo, msg_q_t *msg_q, int cnt)
{
	int tx_cnt, i, ret;
	tx_msg_t *msg;
	tx_cnt = 0;
	for (i = 0; i < cnt; i++) {
		msg = msg_get(msg_q);
		if (NULL == msg) {
			break;
		}
		ret = tx_fifo_in(tx_fifo, msg);
		if (TX_FIFO_FULL == ret) {
			ret = TX_FIFO_FULL;
			break;
		}
		trans_up();
		if (HOST_SC2331_CMD == msg->hdr.type) {
			if ((msg->slice[0].len > 0)
			    && (NULL != msg->slice[0].data))
				kfree(msg->slice[0].data);
		} else if (HOST_SC2331_PKT == msg->hdr.type) {
			if ((NULL != msg->p))
				dev_kfree_skb((struct sk_buff *)(msg->p));
		} else {
		}

		memset((unsigned char *)msg, 0, sizeof(tx_msg_t));
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
	ret = sched_setscheduler(current, SCHED_FIFO, &param);
	printkd("sched_setscheduler, prio:%d,ret:%d\n", param.sched_priority,
		ret);
	return;
}

void thread_sleep_policy(wlan_thread_t *thread)
{
	if (thread->null_run > thread->max_null_run) {
		usleep_range(thread->idle_sleep - 50, thread->idle_sleep + 50);
		/*thread->null_run = 0;*/
	}
	return;
}

static int wlan_core_thread(void *data)
{
	int i, j, ret, retry, vif_id, done, sem_count, q_index, need_tx, tmp,
	    sleep_flag, ack_retry;
	unsigned long timeout;
	wlan_vif_t *vif;
	rxfifo_t *rx_fifo;
	txfifo_t *tx_fifo;
	wlan_thread_t *thread;
	msg_q_t *msg_q;

	thread = &(g_wlan.wlan_core);
	thread->null_run = 0;
	thread->max_null_run = 200;
	thread->idle_sleep = 300;
	thread->prio = 90;
	sleep_flag = timeout = 0;
	atomic_set(&(thread->retry), 0);
	rx_fifo = &(g_wlan.rxfifo);
	printke("%s enter new\n", __func__);
	up(&(g_wlan.sync.sem));
	core_down();
	do {
		ack_retry = retry = done = 0;
		sem_count = g_wlan.wlan_core.sem.count;

		if (thread->null_run > thread->max_null_run) {
			usleep_range(thread->idle_sleep - 50,
				     thread->idle_sleep + 50);
		}
		ret = wlan_rx(rx_fifo, 1);
		if (1 == ret) {
			done++;
			thread->done_rx++;
		}
		for (vif_id = NETIF_0_ID; vif_id < WLAN_MAX_ID; vif_id++) {
			vif = &(g_wlan.netif[vif_id]);
			tx_fifo = &(vif->txfifo);

			ret = wlan_tcpack_tx(vif, &tmp);
			if (OK != ret)
				ack_retry = 1;
			done += tmp;
			thread->done_tx += tmp;

			for (q_index = 0; q_index < 2; q_index++) {
				msg_q = &(vif->msg_q[q_index]);
				need_tx = msg_num(msg_q);
				for (i = 0, j = 0; i < need_tx; i++) {
					ret = wlan_tx(tx_fifo, msg_q, 1);
					if (TX_FIFO_FULL == ret) {
						if (0 == sleep_flag) {
							timeout =
							    jiffies +
							    msecs_to_jiffies
							    (WLAN_CORE_SLEEP_TIME);
							sleep_flag = 1;
						} else {
							if (time_after
							    (jiffies,
							     timeout)) {
								printke
								    ("%s [TIMEOUT][%lu] jiffies:%lu\n",
								     __func__,
								     timeout,
								     jiffies);
								msleep(300);
								sleep_flag = 0;
							}
						}
						retry++;
						continue;
					} else if (TX_FIFO_EMPTY == ret) {
						/* no problem as need_tx, excpt get_event erro */
						ASSERT();
						msleep(10);
						retry++;
						continue;
					} else {
					}
					if (sleep_flag)
						sleep_flag = 0;
					done += ret;
					thread->done_tx += ret;
					j++;
					if (j >= 6) {
						ret = wlan_rx(rx_fifo, 1);
						done = done + ret;
						thread->done_rx += ret;
						j = 0;
					}
				}
			}

		}
		if (g_wlan.sync.exit)
			break;

		if (done > 0)
			thread->null_run = 0;
		else
			thread->null_run++;
		if (ack_retry != 1)
			core_try_down(retry);

	} while (!kthread_should_stop());

	printke("%s exit!\n", __func__);
	up(&(g_wlan.sync.sem));
	return 0;
}

static int check_valid_chn(int flag, unsigned short status,
			   sdio_chn_t *chn_info)
{
	int i, index = -1;

	if (1 == flag)
		status = (status & (chn_info->bit_map));
	else
		status = ((status & chn_info->bit_map) ^ (chn_info->bit_map));
	if (0 == status)
		return -1;
	for (i = 0; i < chn_info->num; i++) {
		if (status & (0x1 << chn_info->chn[i])) {
			index = chn_info->chn[i];
			break;
		}
	}
	return index;
}

static int wlan_trans_thread(void *data)
{
	int i, vif_id, ret, done, retry, sem_count, send_pkt, index, wake_flag,
	    gpio_status;
	rxfifo_t *rx_fifo;
	txfifo_t *tx_fifo;
	wlan_vif_t *vif;
	sdio_chn_t *tx_chn;
	sdio_chn_t *rx_chn;
	unsigned short status;
	wlan_thread_t *thread;
	u32 rx_gpio;

	thread = &(g_wlan.wlan_trans);
	sdiodev_readchn_init(8, (void *)wlan_rx_chn_isr, 1);
	sdiodev_readchn_init(9, (void *)wlan_rx_chn_isr, 1);
	rx_chn = &(g_wlan.hw.sdio_rx_chn);
	tx_chn = &(g_wlan.hw.sdio_tx_chn);
	rx_fifo = &(g_wlan.rxfifo);
	rx_gpio = g_wlan.hw.rx_gpio;
	up(&(g_wlan.sync.sem));
	printke("%s enter\n", __func__);

	thread->null_run = 0;
	thread->max_null_run = 100;
	thread->idle_sleep = 400;
	thread->prio = 90;
	wake_flag = 0;
	thread_sched_policy(thread);
	trans_down();
	do {
		thread_sleep_policy(thread);
		send_pkt = retry = done = 0;
		sem_count = g_wlan.wlan_trans.sem.count;
/*
		if (0 == wake_flag)
		{
			wake_lock(&g_wlan.hw.wlan_lock);
			wake_flag = 1;
		}
*/
RX:
		gpio_status = gpio_get_value(rx_gpio);
		if (!gpio_status) {
			if (true == rx_chn->gpio_high) {
				rx_chn->gpio_high = false;
				rx_chn->timeout_flag = false;
			}
			goto TX;
		} else {
			if (false == rx_chn->gpio_high) {
				rx_chn->gpio_high = true;
			}
		}
		wlan_wakeup();
		ret = set_marlin_wakeup(0, 1);

	#if 0
		if (0 != ret) {
			printke("rx call set_marlin_wakeup error:%d\n", ret);
			if (ret != -ETIMEDOUT)
				msleep(200);
			goto TX;
		}
	#endif

		if (0 != ret) {
			if( (ITM_NONE_MODE != g_wlan.netif[0].mode) || (ITM_NONE_MODE != g_wlan.netif[1].mode) )	
			{
				if(-2 != ret)
				{
					printke("rx call set_marlin_wakeup return:%d:%d\n", ret);
					msleep(200);
					goto TX;
				}
			}
			else
			{
				printke("rx retry open wlan\n", ret);
				msleep(200);
				goto TX;
			}
		}
		
		ret = sdio_chn_status(rx_chn->bit_map, &status);
		if (0 != ret) {
			printke("rx call sdio_chn_status error:%d\n", ret);
			goto RX_SLEEP;
		}
		index = check_valid_chn(1, status, rx_chn);
		if (index < 0) {

RX_SLEEP:
			if (false == rx_chn->timeout_flag) {
				rx_chn->timeout_flag = true;
				rx_chn->timeout =
				    jiffies +
				    msecs_to_jiffies(rx_chn->timeout_time);
			} else {
				if (time_after(jiffies, rx_chn->timeout)) {
					printke
					    ("[SDIO_RX_CHN][TIMEOUT][%lu] jiffies:%lu\n",
					     rx_chn->timeout_time, jiffies);
					msleep(300);
					rx_chn->timeout_flag = false;
				}
			}
			goto TX;
		}
		if (true == rx_chn->timeout_flag) {
			rx_chn->timeout_flag = false;
		}
		if (14 == index) {
			mdbg_sdio_read();
			goto TX;
		}
		if (11 == index) {
			mdbg_at_cmd_read();
			goto TX;
		}
		if (15 == index) {
			mdbg_loopcheck_read();
			goto TX;
		}
		ret = rx_fifo_in(index, rx_fifo, hw_rx);
		if (OK != ret) {
			if (HW_READ_ERROR == ret)
				msleep(100);
			retry++;
			goto TX;
		}
		g_wlan.wlan_core.need_rx++;
		core_up();

TX:
		for (vif_id = NETIF_0_ID; vif_id < WLAN_MAX_ID; vif_id++) {
			vif = &(g_wlan.netif[vif_id]);
			tx_fifo = &(vif->txfifo);
			ret = tx_fifo_used(tx_fifo);
			if (0 == ret)
				continue;
			wlan_wakeup();
			ret = set_marlin_wakeup(0, 1);
	#if 0
			if (0 != ret) {
				printke("tx call set_marlin_wakeup error:%d\n",
					ret);
				if (ret != -ETIMEDOUT)
					msleep(200);
				retry++;
				continue;
			}
	#endif

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
			if (ret) {
				printke("tx call sdio_chn_status error:%d\n",
					ret);
				goto TX_SLEEP;
			}
			index = check_valid_chn(0, status, tx_chn);
			if (index < 0) {
TX_SLEEP:
				if (false == tx_chn->timeout_flag) {
					tx_chn->timeout_flag = true;
					tx_chn->timeout =
					    jiffies +
					    msecs_to_jiffies(tx_chn->
							     timeout_time);
				} else {
					if (time_after
					    (jiffies, tx_chn->timeout)) {
						printke
						    ("[SDIO_TX_CHN][TIMEOUT][%lu] jiffies:%lu\n",
						     tx_chn->timeout_time,
						     jiffies);
						msleep(300);
						tx_chn->timeout_flag = false;
					}
				}
				retry++;
				continue;
			}
			if (true == tx_chn->timeout_flag) {
				tx_chn->timeout_flag = false;
			}
			ret =
			    tx_fifo_out(vif_id, index, tx_fifo, hw_tx,
					&send_pkt);
			if (OK != ret) {
				if (HW_WRITE_ERROR == ret) {
					msleep(100);
					retry++;
				}
				continue;
			}
			done = done + send_pkt;
			core_try_up();
		}

		if (g_wlan.sync.exit) {
/*
			if(1 == wake_flag)
			{
				wake_unlock(&g_wlan.hw.wlan_lock);
				wake_flag = 0;
			}
*/
			break;
		}
		gpio_status = gpio_get_value(rx_gpio);
		if (gpio_status) {
			if (g_wlan.wlan_trans.sem.count - done <= 1) {
				done =
				    (g_wlan.wlan_trans.sem.count >
				     0) ? (g_wlan.wlan_trans.sem.count -
					   1) : (0);
			}
		} else {
			if ((0 == done) && (0 == retry))
				done = ((0 == sem_count) ? (1) : (sem_count));
		}
		if (done > 0)
			thread->null_run = 0;
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
		for (i = 0; i < done; i++) {
			trans_down();
		}
	} while (!kthread_should_stop());
	sdiodev_readchn_uninit(8);
	sdiodev_readchn_uninit(9);
	mdbg_sdio_read();
	del_timer_sync(&(g_wlan.hw.wakeup_timer));
	printke("%s exit\n", __func__);
	up(&(g_wlan.sync.sem));
	core_up();
	return OK;
}

static int wlan_tx_msg_q_alloc(wlan_vif_t *vif)
{
	txfifo_conf_t fifo_conf = { 0 };
	int ret, q_id;

	msg_q_alloc(&(vif->msg_q[0]), sizeof(tx_msg_t), 5);
	msg_q_alloc(&(vif->msg_q[1]), sizeof(tx_msg_t), 150);

	fifo_conf.cp2_txRam = HW_TX_SIZE - sizeof(tx_big_hdr_t);
	fifo_conf.max_msg_num = PKT_AGGR_NUM;
	fifo_conf.size = 1024 * 256;
	ret = tx_fifo_alloc(&(vif->txfifo), &fifo_conf);
	if (ERROR == ret)
		return ERROR;
	return OK;
}

static int wlan_tx_msg_q_free(wlan_vif_t *vif)
{
	int q_id, num, i;
	msg_q_t *msg_q;
	tx_msg_t *msg;
	for (q_id = 0; q_id < 2; q_id++) {
		msg_q = &(vif->msg_q[q_id]);
		num = msg_num(msg_q);
		for (i = 0; i < num; i++) {
			msg = msg_get(msg_q);
			if (NULL == msg)
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

static int wlan_rx_buf_free(void)
{
	int ret;
	rxfifo_t *rx_buf = &(g_wlan.rxfifo);
	ret = rx_fifo_free(rx_buf);
	return ret;
}

static int wlan_hw_init(hw_info_t *hw)
{
	struct device_node *np;

	memset(hw, 0, sizeof(hw_info_t));

	hw->sdio_tx_chn.num = 3;
	hw->sdio_tx_chn.chn[0] = 0;
	hw->sdio_tx_chn.chn[1] = 1;
	hw->sdio_tx_chn.chn[2] = 2;
	hw->sdio_tx_chn.bit_map = 0x0007;
	hw->sdio_tx_chn.timeout_time = 600;
	hw->sdio_tx_chn.timeout_flag = false;

	hw->sdio_rx_chn.num = 6;
	hw->sdio_rx_chn.chn[0] = 8;
	hw->sdio_rx_chn.chn[1] = 9;
	hw->sdio_rx_chn.chn[2] = 14;
	hw->sdio_rx_chn.chn[3] = 11;
	hw->sdio_rx_chn.chn[4] = 15;
	hw->sdio_rx_chn.chn[5] = 13;
	hw->sdio_rx_chn.bit_map = 0xeb00;
	hw->sdio_rx_chn.gpio_high = false;
	hw->sdio_rx_chn.timeout_time = 600;
	hw->sdio_rx_chn.timeout_flag = false;

	np = of_find_node_by_name(NULL, "sprd-marlin");
	if (!np) {
		printke("sprd-marlin not found");
		return -1;
	}
	hw->rx_gpio = of_get_gpio(np, 1);

	printke("[SDIO_TX_CHN][0x%x][0x%x]\n", hw->sdio_tx_chn.bit_map,
		HW_TX_SIZE);
	printke("[SDIO_RX_CHN][0x%x][0x%x]\n", hw->sdio_rx_chn.bit_map,
		HW_RX_SIZE);

	hw->wakeup = 0;
	spin_lock_init(&(hw->sdio_rx_chn.lock));
	wake_lock_init(&g_wlan.hw.wlan_lock, WAKE_LOCK_SUSPEND,
		       "wlan_sc2331_lock");
	init_timer(&g_wlan.hw.wakeup_timer);
	g_wlan.hw.wakeup_timer.function = wakeup_timer_func;
	g_wlan.hw.wakeup_time = 500;
	return OK;
}

static int send_arp(char vif_id, char *mac, char *ip)
{
	int              len;
	struct sk_buff  *skb;
	msg_q_t         *msg_q;
	tx_msg_t         msg = {0};
	unsigned char arp_hex[] =
	{
		0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0x40, 0x45, 0xda,
		0x75, 0xe4, 0x76, 0x08, 0x06, 0x00, 0x01, 0x08, 0x00,
		0x06, 0x04, 0x00, 0x01, 0x40, 0x45, 0xda, 0x75, 0xe4,
		0x76, 0xc0, 0xa8, 0x01, 0x9c, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0xc0, 0xa8, 0x01, 0x01
	};
	memcpy(&(arp_hex[6]),  mac,    6);
	memcpy(&(arp_hex[22]), mac,    6);
	memcpy(&(arp_hex[28]), ip,     4);
	memcpy(&(arp_hex[38]), ip,     4);
	arp_hex[41] = 0x01;
	len = sizeof(arp_hex);
	skb = dev_alloc_skb(len + NET_IP_ALIGN);
	skb_reserve(skb, NET_IP_ALIGN);
	memcpy(skb->data,   (char *)&(arp_hex[0]),  len);
	skb_put(skb, len);
	msg.p = (void *)skb;
	msg.slice[0].data = skb->data;
	msg.slice[0].len  = skb->len;
	msg.hdr.mode      = vif_id;
	msg.hdr.type      = HOST_SC2331_PKT;
	msg.hdr.subtype   = 0;
	msg.hdr.len = skb->len;
	msg_q = &(g_wlan.netif[vif_id].msg_q[1]);
	msg_q_in(msg_q, &msg);
	return 0;
}

static int wlan_inetaddr_event(struct notifier_block *this,
			       unsigned long event, void *ptr)
{
	unsigned char vif_id;
	wlan_vif_t *vif;
	struct net_device *dev;
	printkd("inetaddr callback is comming in !\n");

	struct in_ifaddr *ifa = (struct in_ifaddr *)ptr;

	dev = ifa->ifa_dev ? ifa->ifa_dev->dev : NULL;

	if ((dev != (id_to_vif(0)->ndev)) && (dev != (id_to_vif(1)->ndev))) {
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
		wlan_cmd_get_ip(vif_id, (u8 *)&ifa->ifa_address);
		if(vif_id == NETIF_1_ID)
			send_arp(vif_id, (char *)&(dev->dev_addr[0]),
			          (char *) & ifa->ifa_address );
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

static ssize_t wlan_proc_write(struct file *file, const char __user *buffer,
			       size_t count, loff_t *pos)
{
	int id, cmd, value;
	char kbuf[32] = { 0 };
	if (copy_from_user(kbuf, buffer, ((count <= 32) ? (count) : (32))))
		return -EFAULT;
	sscanf(kbuf, "%d %d %d\n", &id, &cmd, &value);

	printke("[%s][%d][%d][%d]\n", __func__, id, cmd, value);
	switch (id) {
	case 1:
		SET_BIT(g_dbg, cmd);
		break;
	case 2:
		CLEAR_BIT(g_dbg, cmd);
		break;
	default:
		break;
	}
	return count;
}

static const struct file_operations wlan_proc_fops = {
	.owner = THIS_MODULE,
	.write = wlan_proc_write,
};

static const struct file_operations lte_concur_proc_fops = {
	.owner = THIS_MODULE,
	.unlocked_ioctl = lte_concur_proc_ioctl,
	.open = lte_concur_proc_open,
	.release = lte_concur_proc_release,
};

int wlan_module_init(struct device *dev)
{
	int ret;

	printke("[%s] [ version:0x28 ] [ time(%s %s) ]\n", __func__, __DATE__,
		__TIME__);
	if (NULL == dev)
		return -EPERM;
	ret = get_sdiohal_status();
	if (1 != ret) {
		printke("######## %s sdio is not ready  ##########\n",
			__func__);
		return -EPERM;
	} else {
		printke("sdio is ready !!!\n");
	}
	//marlin_pa_enable(true);

	memset((unsigned char *)(&g_wlan), 0, sizeof(wlan_info_t));
	g_wlan.dev = dev;
	g_wlan.netif[NETIF_0_ID].id = NETIF_0_ID;
	g_wlan.netif[NETIF_1_ID].id = NETIF_1_ID;
	g_wlan.sync.exit = 0;

	sema_init(&g_wlan.sync.sem, 0);
	wlan_hw_init(&(g_wlan.hw));

	wlan_tx_msg_q_alloc(&(g_wlan.netif[NETIF_0_ID]));
	wlan_tx_msg_q_alloc(&(g_wlan.netif[NETIF_1_ID]));

	g_wlan.netif[NETIF_0_ID].tcp_ack_suppress = true;
	g_wlan.netif[NETIF_1_ID].tcp_ack_suppress = true;
	wlan_tcpack_buf_malloc(&(g_wlan.netif[NETIF_0_ID]));
	wlan_tcpack_buf_malloc(&(g_wlan.netif[NETIF_1_ID]));
	ret = wlan_rx_buf_alloc();
	if (OK != ret)
		return -EPERM;
	wlan_cmd_init();

	sema_init(&g_wlan.wlan_trans.sem, 0);
	sema_init(&g_wlan.wlan_core.sem, 0);
	ret = wlan_wiphy_new(&(g_wlan));
	if (OK != ret)
		return -EPERM;
	ret =
	    wlan_vif_init(&(g_wlan.netif[NETIF_0_ID]), NL80211_IFTYPE_ADHOC,
			  "wlan0", (void *)(&wlan_ops));
	if (OK != ret)
		return -EPERM;
	ret =
	    wlan_vif_init(&(g_wlan.netif[NETIF_1_ID]),
			  NL80211_IFTYPE_P2P_DEVICE, "p2p0",
			  (void *)(&wlan_ops));
	if (OK != ret)
		return -EPERM;

	g_wlan.wlan_trans.task =
	    kthread_create(wlan_trans_thread, (void *)(dev), "wlan_trans");
	if (NULL != g_wlan.wlan_trans.task)
		wake_up_process(g_wlan.wlan_trans.task);
	down(&(g_wlan.sync.sem));

	g_wlan.wlan_core.task =
	    kthread_create(wlan_core_thread, (void *)(dev), "wlan_core");
	if (NULL != g_wlan.wlan_core.task)
		wake_up_process(g_wlan.wlan_core.task);
	down(&(g_wlan.sync.sem));

	wlan_nl_init();

	g_wlan.hw.early_suspend.suspend = wlan_early_suspend;
	g_wlan.hw.early_suspend.resume = wlan_late_resume;
	g_wlan.hw.early_suspend.level = EARLY_SUSPEND_LEVEL_BLANK_SCREEN - 1;
	register_early_suspend(&g_wlan.hw.early_suspend);

	ret = register_inetaddr_notifier(&itm_inetaddr_cb);
	if (ret) {
		printke("Couldn't register inetaddr notifier \n");
	}
	if (!proc_create("wlan", 0666, NULL, &wlan_proc_fops)) {
		printke("Couldn't create the /proc/wlan \n");
	}
	if (!proc_create("lte_concur", 0666, NULL, &lte_concur_proc_fops)) {
		printke("Couldn't create the /proc/lte_concur \n");
	}
	g_dbg = 0x0;
	SET_BIT(g_dbg, 1);
	printke("%s ok!\n", __func__);
	return OK;
}

EXPORT_SYMBOL_GPL(wlan_module_init);

int wlan_module_exit(struct device *dev)
{
	printke("%s enter\n", __func__);
	unregister_inetaddr_notifier(&itm_inetaddr_cb);
	//marlin_pa_enable(false);
	if (ITM_NONE_MODE != g_wlan.netif[NETIF_0_ID].mode)
		wlan_cmd_mac_close(NETIF_0_ID, g_wlan.netif[NETIF_0_ID].mode);
	if (ITM_NONE_MODE != g_wlan.netif[NETIF_1_ID].mode)
		wlan_cmd_mac_close(NETIF_1_ID, g_wlan.netif[NETIF_1_ID].mode);
	g_wlan.sync.exit = 1;
	core_up();
	down(&(g_wlan.sync.sem));
	trans_up();
	down(&(g_wlan.sync.sem));
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
	printke("%s ok!\n", __func__);
	return OK;
}
EXPORT_SYMBOL_GPL(wlan_module_exit);

static int  sprd_wlan_probe(struct platform_device *pdev)
{
	return wlan_module_init(&(pdev->dev));
}

static int  sprd_wlan_remove(struct platform_device *pdev)
{
	return wlan_module_exit(&(pdev->dev));
}

#define DEVICE_NAME "sc2331"
static struct platform_device *sprd_wlan_device;
static struct platform_driver sprd_wlan_driver = {
	.probe = sprd_wlan_probe,
	.remove = sprd_wlan_remove,
	.driver = {
		   .owner = THIS_MODULE,
		   .name = DEVICE_NAME,
		   },
};

static int sprd_wlan_init(void)
{
	sprd_wlan_device =
	    platform_device_register_simple(DEVICE_NAME, 0, NULL, 0);
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
