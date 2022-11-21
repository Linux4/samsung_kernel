/*
 * Copyright (C) 2012 Spreadtrum Communications Inc.
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
#include <linux/etherdevice.h>
#include <linux/skbuff.h>
#include <linux/wakelock.h>
#include <linux/workqueue.h>
#include <linux/ipv6.h>
#include <linux/ip.h>
#include <linux/tcp.h>
#include <linux/icmp.h>
#include <linux/icmpv6.h>
#include <asm/byteorder.h>
#include <linux/tty.h>
#include <linux/platform_device.h>
#include <linux/atomic.h>
#include <linux/sched.h>
#include <linux/kthread.h>
#ifdef CONFIG_DEBUG_FS
#include <linux/debugfs.h>
#include <linux/seq_file.h>
#endif

#ifdef CONFIG_OF
#include <linux/of_device.h>
#endif

#include <linux/sipc.h>
#include <linux/seth.h>

#define seth_max(a,b) ((a) > (b) ? (a) : (b))
#define seth_min(a,b) ((a) < (b) ? (a) : (b))

/* debugging macros */
#define SETH_INFO(x...)		pr_info("SETH: " x)
#define SETH_DEBUG(x...)	pr_debug("SETH: " x)
#define SETH_ERR(x...)		pr_err("SETH: Error: " x)

/* defination of sblock length */
#define SETH_BLOCK_SIZE	(ETH_HLEN + ETH_DATA_LEN + NET_IP_ALIGN)

/* device status */
#define DEV_ON 1
#define DEV_OFF 0

/* enable NAPI */
#define SETH_NAPI

/* tx pkt return value */
#define SETH_TX_SUCCESS		 0
#define SETH_TX_NO_BLK		-1
#define SETH_TX_INVALID_BLK	-2

/* Macro of Packet Field  */
#define SETH_IPV4_HLEN 20
#define SETH_IPV6_HLEN 40
#define SETH_ETH_HLEN  ETH_HLEN
#define SETH_ETH_FRAME_LEN ETH_FRAME_LEN
#define SETH_IPV4_VERSION 0x04
#define SETH_IPV6_VERSION 0x06

/* Macro of protocol type */
#define SETH_PRO_TCP	6
#define SETH_PRO_ICMP	1

/* Macro of ICMP type */
#define SETH_ICMP_PING	ICMP_ECHO
#define SETH_ICMP6_PING	ICMPV6_ECHO_REQUEST

#ifdef SETH_NAPI
#define SETH_NAPI_WEIGHT 64
#define SETH_TX_WEIGHT 16

/* struct of data transfer statistics */
struct seth_dtrans_stats {
	uint32_t rx_pkt_max;
	uint32_t rx_pkt_min;
	uint32_t rx_sum;
	uint32_t rx_cnt;
	uint32_t rx_alloc_fails;

	uint32_t tx_pkt_max;
	uint32_t tx_pkt_min;
	uint32_t tx_sum;
	uint32_t tx_cnt;
	uint32_t tx_ping_cnt;
	uint32_t tx_ack_cnt;
};

#endif /* end of SETH_NAPI*/

/*
 * Device instance data.
 */
typedef struct SEth {
	struct net_device_stats stats;	/* net statistics */
	struct net_device* netdev;	/* Linux net device */
	struct seth_init_data* pdata;	/* platform data */
	int state;			/* device state */
	int txstate;			/* device txstate */

#ifdef SETH_NAPI
	atomic_t rx_busy;
	struct timer_list rx_timer;

	atomic_t txpending;	/* seth tx resend count*/
	struct timer_list tx_timer;
	struct napi_struct napi; /* napi instance */
	struct seth_dtrans_stats dt_stats; /* record data_transfer statistics*/
#endif /* SETH_NAPI */
} SEth;

#ifdef CONFIG_SETH_GRO_DISABLE
uint32_t seth_gro_enable = 0;
#else
uint32_t seth_gro_enable = 1;
#endif

#ifdef CONFIG_DEBUG_FS
struct dentry *root = NULL;
uint32_t seth_print_seq = 0;
uint32_t seth_print_ipid;
static int seth_debugfs_mknod(void *root, void * data);
#endif

#ifdef SETH_NAPI
static void seth_rx_timer_handler(unsigned long data);

static inline void seth_dt_stats_init(struct seth_dtrans_stats * stats)
{
	memset(stats, 0, sizeof(struct seth_dtrans_stats));
	stats->rx_pkt_min = 0xff;
	stats->tx_pkt_min = 0xff;
}

static inline void seth_rx_stats_update(struct seth_dtrans_stats *stats, uint32_t cnt)
{
	stats->rx_pkt_max = seth_max(stats->rx_pkt_max, cnt);
	stats->rx_pkt_min = seth_min(stats->rx_pkt_min, cnt);
	stats->rx_sum += cnt;
	stats->rx_cnt++;
}

static inline void seth_tx_stats_update(struct seth_dtrans_stats *stats, uint32_t cnt)
{
	stats->tx_pkt_max = seth_max(stats->tx_pkt_max, cnt);
	stats->tx_pkt_min = seth_min(stats->tx_pkt_min, cnt);
	stats->tx_sum += cnt;
	stats->tx_cnt++;
}

static inline int pkt_is_ping(void *data)
{
	uint8_t *idata = (uint8_t *)data;
	uint8_t ver, protocol;
	uint32_t ip_hlen;
	int ret = 0;

	/* parse the version of IP protocol */
	ver = (idata[SETH_ETH_HLEN] & 0xf0) >> 4;
	if (ver == SETH_IPV4_VERSION) {
		struct iphdr * ip_hdr;
		struct icmphdr *icmp_hdr;

		ip_hdr = (struct iphdr *)(&idata[SETH_ETH_HLEN]);
		/* get length of ip header */
		ip_hlen = ip_hdr->ihl * 4;
		/* get the protocol */
		protocol = ip_hdr->protocol;
		icmp_hdr = (struct icmphdr *)(&idata[SETH_ETH_HLEN + ip_hlen]);
		if (protocol == SETH_PRO_ICMP && icmp_hdr->type == SETH_ICMP_PING) {
			ret = 1;
		}
	} else if (ver == SETH_IPV6_VERSION) {
		struct ipv6hdr *ipv6_hdr;
		struct icmp6hdr *icmp6_hdr;

		ipv6_hdr = (struct ipv6hdr *)(&idata[SETH_ETH_HLEN]);
		ip_hlen = SETH_ETH_HLEN;
		/* get the protocol */
		protocol = ipv6_hdr->nexthdr;
		icmp6_hdr = (struct icmp6hdr *)(&idata[SETH_ETH_HLEN + ip_hlen]);
		if (protocol == SETH_PRO_ICMP && icmp6_hdr->icmp6_type == SETH_ICMP6_PING) {
			ret = 1;
		}
	}

	return ret;
}

static inline int pkt_is_ack(void *data)
{
	uint8_t *idata = (uint8_t *)data;
	uint8_t ver, protocol;
	uint32_t ip_hlen;
	struct tcphdr *tcp_hdr;
	int ret = 0;

	/* parse the version of IP protocol */
	ver = (idata[SETH_ETH_HLEN] & 0xf0) >> 4;
	if (ver == SETH_IPV4_VERSION) {
		struct iphdr *ip_hdr;

		ip_hdr = (struct iphdr *)(&idata[SETH_ETH_HLEN]);
		/*get length of ip header */
		ip_hlen = ip_hdr->ihl * 4;
		/* get the protocol*/
		protocol = ip_hdr->protocol;
	} else if (ver == SETH_IPV6_VERSION){
		struct ipv6hdr *ipv6_hdr;

		ipv6_hdr = (struct ipv6hdr *)(&idata[SETH_ETH_HLEN]);
		ip_hlen = SETH_IPV6_HLEN;
		/* get the protocol */
		protocol = ipv6_hdr->nexthdr;
	} else {
		return 0;
	}

	tcp_hdr = (struct tcphdr *)(&idata[SETH_ETH_HLEN + ip_hlen]);
	if (protocol == SETH_PRO_TCP && (tcp_hdr->ack || tcp_hdr->syn)) {
		ret = 1;
	}

	return ret;
}

static inline void pkt_seq_print(void *data)
{
	uint8_t *idata = (uint8_t *)data;
	uint8_t ver, proto;
	uint32_t seq, ip_hlen;
	static uint32_t nr = 0;
	struct tcphdr *tcp_hdr;

	/* parse the version of IP protocol */
	ver = (idata[SETH_ETH_HLEN] & 0xf0) >> 4;

	if (ver == SETH_IPV4_VERSION) {
		struct iphdr *ip_hdr;

		ip_hdr = (struct iphdr *)(&idata[SETH_ETH_HLEN]);
		/* get length of ip header */
		ip_hlen = ip_hdr->ihl * 4;
		/* get the protocol */
		proto = ip_hdr->protocol;
	} else if (ver == SETH_IPV6_VERSION) {
		struct ipv6hdr *ipv6_hdr;

		ipv6_hdr = (struct ipv6hdr *)(&idata[SETH_ETH_HLEN]);
		ip_hlen = SETH_IPV6_HLEN;
		/* get  protocol */
		proto = ipv6_hdr->nexthdr;
	} else {
		return;
	}

	/* print tcp seq */
	if (proto == SETH_PRO_TCP) {
		tcp_hdr = (struct tcphdr*)(&idata[SETH_ETH_HLEN + ip_hlen]);
		seq = ntohl(tcp_hdr->seq);
		pr_err("SETH: rx pkt: no.%u, ipv%u, nseq = 0x%08x, hseq = 0x%08x\n",
				nr++, proto, tcp_hdr->seq, seq);
	}
}

static int seth_rx_poll_handler(struct napi_struct * napi, int budget)
{
	struct SEth *seth = container_of(napi, struct SEth, napi);
	struct sk_buff * skb;
	struct seth_init_data *pdata;
	struct sblock blk;
	struct seth_dtrans_stats *dt_stats;
	int skb_cnt, blk_ret, ret;

	if (!seth) {
		SETH_ERR("seth_rx_poll_handler no seth device\n");
		return 0;
	}

	pdata = seth->pdata;
	dt_stats = &seth->dt_stats;

	blk_ret = 0;
	skb_cnt = 0;
	/* keep polling, until the sblock rx ring is empty */
	while ((budget - skb_cnt) && !blk_ret) {
		blk_ret = sblock_receive(pdata->dst, pdata->channel, &blk, 0);
		if (blk_ret) {
			SETH_DEBUG("receive sblock error %d\n", blk_ret);
			continue;
		}

#ifdef CONFIG_DEBUG_FS
		/* print the sequence field of tcp packet for debug purpose*/
		if (seth_print_seq) {
			pkt_seq_print((void *)(blk.addr + NET_IP_ALIGN));
		}
#endif

#ifdef CONFIG_SETH_OPT
		/*
		 * start from SHARKL, the first 2 bytes of sblock are reserved for optimization,
		 * so IP field has been aligned to 4 Bytes already
		 */
		skb = dev_alloc_skb (blk.length);
		if (!skb) {
			SETH_ERR ("failed to alloc skb!\n");
			seth->stats.rx_dropped++;
			ret = sblock_release(pdata->dst, pdata->channel, &blk);
			if (ret) {
				SETH_ERR ("release sblock failed %d\n", ret);
			}
			dt_stats->rx_alloc_fails++;
			continue;
		}

		unalign_memcpy(skb->data, blk.addr, blk.length);
		ret = sblock_release(pdata->dst, pdata->channel, &blk);
		if (ret) {
			SETH_ERR("release sblock error %d\n", ret);
		}

		/* update skb poiter */
		skb_reserve(skb, NET_IP_ALIGN);
		skb_put (skb, blk.length - NET_IP_ALIGN);
#else
		skb = dev_alloc_skb (blk.length + NET_IP_ALIGN);
		if (!skb) {
			SETH_ERR ("alloc skbuff failed!\n");
			seth->stats.rx_dropped++;
			ret = sblock_release(pdata->dst, pdata->channel, &blk);
			if (ret) {
				SETH_ERR ("release sblock failed %d\n", ret);
			}
			continue;
		}

		skb_reserve(skb, NET_IP_ALIGN);
		unalign_memcpy(skb->data, blk.addr, blk.length);
		ret = sblock_release(pdata->dst, pdata->channel, &blk);
		if (ret) {
			SETH_ERR("release sblock error %d\n", ret);
		}
		skb_put (skb, blk.length);
#endif

		skb->protocol = eth_type_trans(skb, seth->netdev);
		skb->ip_summed = CHECKSUM_NONE;

		/* update fifo rd_ptr */
		seth->stats.rx_bytes += skb->len;
		seth->stats.rx_packets++;

		if (likely(seth_gro_enable))
			napi_gro_receive(napi, skb);
		else
			netif_receive_skb(skb);
		/* update skb counter*/
		skb_cnt++;
	}

	SETH_DEBUG("napi polling done, budget %d, skb cnt %d, cpu %d\n",
			budget, skb_cnt, smp_processor_id());

	/* update rx statistics */
	seth_rx_stats_update(dt_stats, skb_cnt);

	if (skb_cnt >= 0 && budget > skb_cnt) {
		napi_complete(napi);
		atomic_dec(&seth->rx_busy);

		/* to guarantee that any arrived sblock(s) can be processed even if there are no events issued by CP */
		if (sblock_get_arrived_count(pdata->dst, pdata->channel)) {
			/* start a timer with 2 jiffies expries (20 ms) */
			seth->rx_timer.function = seth_rx_timer_handler;
			seth->rx_timer.expires = jiffies + HZ/50;
			seth->rx_timer.data = (unsigned long)seth;
			mod_timer(&seth->rx_timer, seth->rx_timer.expires);
			SETH_INFO("start rx_timer, jiffies %lu.\n", jiffies);
		}
	}

	return skb_cnt;
}

#endif

/*
 * Tx_ready handler.
 */
static void
seth_tx_ready_handler (void* data)
{
	SEth* seth = (SEth*) data;

	SETH_DEBUG("seth_tx_ready_handler state %0x\n", seth->state);
	if (seth->state != DEV_ON) {
		seth->state = DEV_ON;
		seth->txstate = DEV_ON;
		if (!netif_carrier_ok (seth->netdev)) {
			netif_carrier_on (seth->netdev);
		}
	}
	else {
		seth->state = DEV_OFF;
		seth->txstate = DEV_OFF;
		if (netif_carrier_ok (seth->netdev)) {
			netif_carrier_off (seth->netdev);
		}
	}
}

/*
 * Tx_open handler.
 */
static void
seth_tx_open_handler (void* data)
{
	SEth* seth = (SEth*) data;

	SETH_DEBUG("seth_tx_open_handler state %0x\n", seth->state);
	if (seth->state != DEV_ON) {
		seth->state = DEV_ON;
		seth->txstate = DEV_ON;
		if (!netif_carrier_ok (seth->netdev)) {
			netif_carrier_on (seth->netdev);
		}
	}
}

/*
 * Tx_close handler.
 */
static void
seth_tx_close_handler (void* data)
{
	SEth* seth = (SEth*) data;

	SETH_INFO("seth_tx_close_handler state %0x\n", seth->state);
	if (seth->state != DEV_OFF) {
		seth->state = DEV_OFF;
		seth->txstate = DEV_OFF;
		if (netif_carrier_ok (seth->netdev)) {
			netif_carrier_off (seth->netdev);
		}
	}
}

#ifdef SETH_NAPI
static void seth_rx_handler (void * data)
{
	SEth* seth = (SEth *)data;

	if (!seth) {
		SETH_ERR("seth_rx_handler data is NULL.\n");
		return;
	}

	/* if the poll handler has been done, trigger to schedule*/
	if (!atomic_cmpxchg(&seth->rx_busy, 0, 1)) {
		/* update rx stats*/
		napi_schedule(&seth->napi);
		/* trigger a NET_RX_SOFTIRQ softirq directly */
		raise_softirq(NET_RX_SOFTIRQ);
	}
}

static void seth_rx_timer_handler(unsigned long data)
{
	SETH_INFO("rx_timer expried, jiffies %lu.\n", jiffies);
	seth_rx_handler((void *)data);
}

#else
static void seth_rx_handler (void* data)
{
	SEth* seth = (SEth *)data;
	struct seth_init_data *pdata = seth->pdata;
	struct sblock blk;
	struct sk_buff* skb;
	uint32_t cnt;
	int ret, sblkret;

	sblkret = 0;
	cnt = 0;
	while (!sblkret){
		sblkret = sblock_receive(pdata->dst, pdata->channel, &blk, 0);
		if (sblkret) {
			SETH_DEBUG("receive sblock error %d\n", sblkret);
			break;
		}

		/* if the seth state is off, drop the comming packet */
		if (seth->state != DEV_ON) {
			SETH_ERR ("rx_handler the state of %s is off!\n", seth->netdev->name);
			sblock_release(pdata->dst, pdata->channel, &blk);
			continue;
		}

#ifdef CONFIG_SETH_OPT
		/*
		 * start from SHARKL, the first 2 bytes of sblock are reserved for optimization,
		 * so IP field has been aligned to 4 Bytes already
		 */
		skb = dev_alloc_skb (blk.length);
		if (!skb) {
			SETH_ERR ("failed to alloc skb!\n");
			seth->stats.rx_dropped++;
			ret = sblock_release(pdata->dst, pdata->channel, &blk);
			if (ret) {
				SETH_ERR ("release sblock failed %d\n", ret);
			}
			dt_stats->rx_alloc_fails++;
			continue;
		}

		unalign_memcpy(skb->data, blk.addr, blk.length);
		ret = sblock_release(pdata->dst, pdata->channel, &blk);
		if (ret) {
			SETH_ERR("release sblock error %d\n", ret);
		}

		/* update skb poiter */
		skb_reserve(skb, NET_IP_ALIGN);
		skb_put (skb, blk.length - NET_IP_ALIGN);
#else
		skb = dev_alloc_skb (blk.length + NET_IP_ALIGN);
		if (!skb) {
			SETH_ERR ("alloc skbuff failed!\n");
			seth->stats.rx_dropped++;
			ret = sblock_release(pdata->dst, pdata->channel, &blk);
			if (ret) {
				SETH_ERR ("release sblock failed %d\n", ret);
			}
			continue;
		}

		skb_reserve(skb, NET_IP_ALIGN);
		unalign_memcpy(skb->data, blk.addr, blk.length);
		ret = sblock_release(pdata->dst, pdata->channel, &blk);
		if (ret) {
			SETH_ERR("release sblock error %d\n", ret);
		}
		skb_put (skb, blk.length);
#endif

		skb->dev = seth->netdev;
		skb->protocol  = eth_type_trans (skb, seth->netdev);
		skb->ip_summed = CHECKSUM_NONE;

		seth->stats.rx_packets++;
		seth->stats.rx_bytes += skb->len;

		netif_rx (skb);

		seth->netdev->last_rx = jiffies;
	}

	return;
}
#endif


/*
 * Tx_close handler.
 */
static void
seth_tx_pre_handler (void* data)
{
	SEth* seth = (SEth*) data;

	if (seth->txstate != DEV_ON) {
		seth->txstate = DEV_ON;
		SETH_INFO ("seth_tx_ready_handler txstate %0x\n", seth->txstate );
		seth->netdev->trans_start = jiffies;
		if (netif_queue_stopped(seth->netdev)) {
			netif_wake_queue(seth->netdev);
		}
	}
}

static void
seth_handler (int event, void* data)
{
	SEth *seth = (SEth *)data;

	switch(event) {
		case SBLOCK_NOTIFY_GET:
			SETH_DEBUG ("SBLOCK_NOTIFY_GET is received\n");
			seth_tx_pre_handler(seth);
			break;
		case SBLOCK_NOTIFY_RECV:
			SETH_DEBUG ("SBLOCK_NOTIFY_RECV is received\n");
			del_timer(&seth->rx_timer);
			seth_rx_handler(seth);
			break;
		case SBLOCK_NOTIFY_STATUS:
			SETH_DEBUG ("SBLOCK_NOTIFY_STATUS is received\n");
			seth_tx_ready_handler(seth);
			break;
		case SBLOCK_NOTIFY_OPEN:
			SETH_DEBUG ("SBLOCK_NOTIFY_OPEN is received\n");
			seth_tx_open_handler(seth);
			break;
		case SBLOCK_NOTIFY_CLOSE:
			SETH_DEBUG ("SBLOCK_NOTIFY_CLOSE is received\n");
			seth_tx_close_handler(seth);
			break;
		default:
			SETH_ERR ("Received event is invalid(event=%d)\n", event);
	}
}

static int
seth_tx_pkt(void* data, struct sk_buff* skb)
{
	struct sblock blk;
	SEth* seth = netdev_priv (data);
	struct seth_init_data *pdata = seth->pdata;
	int ret;

	/*
	 * Get a free sblock.
	 */
	ret = sblock_get(pdata->dst, pdata->channel, &blk, 0);
	if(ret) {
		SETH_INFO("Get free sblock failed(%d), drop data!\n", ret);
		seth->stats.tx_fifo_errors++;
		return SETH_TX_NO_BLK;
	}

	if(blk.length < (skb->len + NET_IP_ALIGN)) {
		SETH_ERR("The size of sblock is so tiny!\n");
		/* recycle the unused sblock */
		sblock_put(pdata->dst, pdata->channel, &blk);
		seth->stats.tx_fifo_errors++;
		netif_wake_queue(seth->netdev);
		return SETH_TX_INVALID_BLK;
	}

#ifdef CONFIG_SETH_OPT
	/* padding 2 Bytes in the head */
	blk.length = skb->len + NET_IP_ALIGN;
	unalign_memcpy(blk.addr + NET_IP_ALIGN, skb->data, skb->len);
	/* copy the content into smem in mute */
	sblock_send_prepare(pdata->dst, pdata->channel, &blk);
#else
	blk.length = skb->len;
	unalign_memcpy(blk.addr, skb->data,skb->len);
	/* copy the content into smem and trigger a smsg to the peer side */
	sblock_send(pdata->dst, pdata->channel, &blk);
#endif
	/* update the statistics */
	seth->stats.tx_bytes += skb->len;
	seth->stats.tx_packets++;

	dev_kfree_skb_any(skb);

#ifdef CONFIG_SETH_OPT
	atomic_inc(&seth->txpending);
#endif

	return SETH_TX_SUCCESS;
}

static void seth_tx_flush(unsigned long data)
{
	int ret;
	uint32_t cnt;
	SEth* seth = netdev_priv((void *)data);
	struct seth_init_data *pdata = seth->pdata;

	cnt = (uint32_t)atomic_read(&seth->txpending);
	ret = sblock_send_finish(pdata->dst, pdata->channel);
	seth_tx_stats_update(&seth->dt_stats, cnt);
	if(ret) {
		SETH_INFO("seth tx failed(%d)!\n", ret);
	} else {
		atomic_set(&seth->txpending, 0);
	}
}

/*
 * Transmit interface
 */
static int
seth_start_xmit (struct sk_buff* skb, struct net_device* dev)
{
	SEth* seth = netdev_priv(dev);
	struct seth_dtrans_stats *dt_stats;
	int flag = 0, ret = 0;
	int blk_cnt = 0;

	if (seth->state != DEV_ON) {
		SETH_ERR ("xmit the state of %s is off\n", dev->name);
		netif_carrier_off (dev);
		seth->stats.tx_carrier_errors++;
		dev_kfree_skb_any(skb);
		return NETDEV_TX_OK;
	}

	/* update tx statistics */
	dt_stats = &seth->dt_stats;
	if (pkt_is_ping((void *)skb->data)) {
		dt_stats->tx_ping_cnt++;
		flag = 1;
	} else if (pkt_is_ack((void *)skb->data)) {
		dt_stats->tx_ack_cnt++;
		flag = 1;
	}

	ret = seth_tx_pkt(dev, skb);
	if (SETH_TX_NO_BLK == ret) {
		/* if there are no available sblks, enter flow control */
		seth->txstate = DEV_OFF;
		netif_stop_queue(dev);
		/* anyway, flush the stored sblks */
		seth_tx_flush((unsigned long)dev);
		return NETDEV_TX_BUSY;
	} else if (SETH_TX_INVALID_BLK == ret) {
		return NETDEV_TX_OK;
	}

	/* if there are no available sblock for subsequent skb, start flow control*/
	blk_cnt = sblock_get_free_count(seth->pdata->dst, seth->pdata->channel);
	if (!blk_cnt) {
		SETH_DEBUG("start flow control\n");
		seth->txstate = DEV_OFF;
		netif_stop_queue(dev);
	}

#ifdef CONFIG_SETH_OPT
	if ((atomic_read(&seth->txpending) >= SETH_TX_WEIGHT) || flag) {
		del_timer(&seth->tx_timer);
		seth_tx_flush((unsigned long)dev);
		SETH_DEBUG ("seth_start_xmit:at once\n");
	} else if (atomic_read(&seth->txpending) == 1){
		/* start a timer with 1 jiffy expries (10 ms) */
		seth->tx_timer.function = seth_tx_flush;
		seth->tx_timer.expires = jiffies + HZ/100;
		seth->tx_timer.data = (unsigned long)dev;
		mod_timer(&seth->tx_timer, seth->tx_timer.expires);
		SETH_DEBUG ("seth_start_xmit:Timer\n");
	}
#endif
	dev->trans_start = jiffies;
	return NETDEV_TX_OK;
}

/*
 * Open interface
 */
static int seth_open (struct net_device *dev)
{
	SEth* seth = netdev_priv(dev);
	struct seth_init_data *pdata;
	struct sblock blk;
	int ret = 0, num = 0;

	SETH_INFO("open seth!\n");

	if (!seth) {
		return -ENODEV;
	}

	pdata = seth->pdata;

	/* clean the resident sblocks */
	while(!ret) {
		ret = sblock_receive(pdata->dst, pdata->channel, &blk, 0);
		if (!ret) {
			sblock_release(pdata->dst, pdata->channel, &blk);
			num++;
		}
	}
	SETH_INFO("seth_open clean %d resident sblocks\n", num);

	/* Reset stats */
	memset(&seth->stats, 0, sizeof(seth->stats));

	if (seth->state == DEV_ON && !netif_carrier_ok(seth->netdev)) {
		SETH_INFO("seth_open netif_carrier_on\n");
		netif_carrier_on(seth->netdev);
	}

#ifdef SETH_NAPI
	atomic_set(&seth->rx_busy, 0);
	napi_enable(&seth->napi);
	seth_dt_stats_init(&seth->dt_stats);
#endif

	seth->txstate = DEV_ON;
	seth->state = DEV_ON;

	netif_start_queue(dev);

	return 0;
}

/*
 * Close interface
 */
static int seth_close (struct net_device *dev)
{
	SEth* seth = netdev_priv(dev);

	SETH_INFO("close seth!\n");

	seth->txstate = DEV_OFF;
	seth->state = DEV_OFF;

#ifdef SETH_NAPI
	napi_disable(&seth->napi);
#endif

	netif_stop_queue(dev);

	return 0;
}

static struct net_device_stats * seth_get_stats(struct net_device *dev)
{
	SEth * seth = netdev_priv(dev);
	return &(seth->stats);
}

static void seth_tx_timeout(struct net_device *dev)
{
	SEth * seth = netdev_priv(dev);

	SETH_INFO ("seth_tx_timeout()\n");
	if (seth->txstate != DEV_ON) {
		seth->txstate = DEV_ON;
		dev->trans_start = jiffies;
		netif_wake_queue(dev);
	}
}

static struct net_device_ops seth_ops = {
	.ndo_open = seth_open,
	.ndo_stop = seth_close,
	.ndo_start_xmit = seth_start_xmit,
	.ndo_get_stats = seth_get_stats,
	.ndo_tx_timeout = seth_tx_timeout,
};

static int seth_parse_dt(struct seth_init_data **init, struct device *dev)
{
#ifdef CONFIG_OF
	struct seth_init_data *pdata = NULL;
	struct device_node *np = dev->of_node;
	int ret;
	uint32_t data;

	pdata = kzalloc(sizeof(struct seth_init_data), GFP_KERNEL);
	if (!pdata) {
		return -ENOMEM;
	}

	ret = of_property_read_string(np, "sprd,name", (const char**)&pdata->name);
	if (ret) {
		goto error;
	}

	ret = of_property_read_u32(np, "sprd,dst", (uint32_t *)&data);
	if (ret) {
		goto error;
	}
	pdata->dst = (uint8_t)data;

	ret = of_property_read_u32(np, "sprd,channel", (uint32_t *)&data);
	if (ret) {
		goto error;
	}
	pdata->channel = (uint8_t)data;

	ret = of_property_read_u32(np, "sprd,blknum", (uint32_t *)&pdata->blocknum);
	if (ret) {
		goto error;
	}

	*init = pdata;
	return 0;
error:
	kfree(pdata);
	*init = NULL;
	return ret;
#else
	return -ENODEV;
#endif
}

static inline void seth_destroy_pdata(struct seth_init_data **init)
{
#ifdef CONFIG_OF
	struct seth_init_data *pdata = *init;

	if (pdata) {
		kfree(pdata);
	}
	*init = NULL;
#else
	return;
#endif
}

static int seth_probe(struct platform_device *pdev)
{
	struct seth_init_data *pdata = pdev->dev.platform_data;
	struct net_device* netdev;
	SEth* seth;
	char ifname[IFNAMSIZ];
	int ret;

	if (pdev->dev.of_node && !pdata) {
		ret = seth_parse_dt(&pdata, &pdev->dev);
		if (ret) {
			SETH_ERR( "failed to parse seth device tree, ret=%d\n", ret);
			return ret;
		}
	}
	SETH_INFO("after parse device tree, name=%s, dst=%u, channel=%u, blocknum=%u\n",
		pdata->name, pdata->dst, pdata->channel, pdata->blocknum);

	if(pdata->name[0])
		strlcpy(ifname, pdata->name, IFNAMSIZ);
	else
		strcpy(ifname, "veth%d");

	netdev = alloc_netdev (sizeof (SEth), ifname, ether_setup);
	if (!netdev) {
		seth_destroy_pdata(&pdata);
		SETH_ERR ("alloc_netdev() failed.\n");
		return -ENOMEM;
	}
	seth = netdev_priv (netdev);
	seth->pdata = pdata;
	seth->netdev = netdev;
	seth->state = DEV_OFF;

#ifdef SETH_NAPI
	atomic_set(&seth->rx_busy, 0);
	atomic_set(&seth->txpending, 0);

	init_timer(&seth->rx_timer);
	init_timer(&seth->tx_timer);
	seth_dt_stats_init(&seth->dt_stats);
#endif

	netdev->netdev_ops = &seth_ops;
	netdev->watchdog_timeo = 1*HZ;
	netdev->irq = 0;
	netdev->dma = 0;

	random_ether_addr(netdev->dev_addr);

#ifdef SETH_NAPI
	netif_napi_add(netdev, &seth->napi, seth_rx_poll_handler, SETH_NAPI_WEIGHT);
#endif

	ret = sblock_create(pdata->dst, pdata->channel,
		pdata->blocknum, SETH_BLOCK_SIZE,
		pdata->blocknum, SETH_BLOCK_SIZE);
	if (ret) {
		SETH_ERR ("create sblock failed (%d)\n", ret);
#ifdef SETH_NAPI
		netif_napi_del(&seth->napi);
#endif
		free_netdev(netdev);
		seth_destroy_pdata(&pdata);
		return ret;
	}

	ret = sblock_register_notifier(pdata->dst, pdata->channel, seth_handler, seth);
	if (ret) {
		SETH_ERR ("regitster notifier failed (%d)\n", ret);
#ifdef SETH_NAPI
		netif_napi_del(&seth->napi);
#endif
		free_netdev(netdev);
		sblock_destroy(pdata->dst, pdata->channel);
		seth_destroy_pdata(&pdata);
		return ret;
	}

	/* register new Ethernet interface */
	if ((ret = register_netdev (netdev))) {
		SETH_ERR ("register_netdev() failed (%d)\n", ret);
#ifdef SETH_NAPI
		netif_napi_del(&seth->napi);
#endif
		free_netdev(netdev);
		sblock_destroy(pdata->dst, pdata->channel);
		seth_destroy_pdata(&pdata);
		return ret;
	}

	/* set link as disconnected */
	netif_carrier_off (netdev);

	platform_set_drvdata(pdev, seth);
#ifdef CONFIG_DEBUG_FS
	seth_debugfs_mknod(root, (void *)seth);
#endif
	return 0;
}

/*
 * Cleanup Ethernet device driver.
 */
static int seth_remove (struct platform_device *pdev)
{
	struct SEth* seth = platform_get_drvdata(pdev);
	struct seth_init_data *pdata = seth->pdata;

#ifdef SETH_NAPI
	netif_napi_del(&seth->napi);

	del_timer_sync(&seth->rx_timer);
	del_timer_sync(&seth->tx_timer);
#endif

	sblock_destroy(pdata->dst, pdata->channel);
	seth_destroy_pdata(&pdata);
	unregister_netdev(seth->netdev);
	free_netdev(seth->netdev);

	platform_set_drvdata(pdev, NULL);

	return 0;
}

static const struct of_device_id seth_match_table[] = {
	{ .compatible = "sprd,seth", },
	{ },
};

static struct platform_driver seth_driver = {
	.probe = seth_probe,
	.remove = seth_remove,
	.driver = {
		.owner = THIS_MODULE,
		.name = "seth",
		.of_match_table = seth_match_table,
	},
};

static int __init seth_init(void)
{
	return platform_driver_register(&seth_driver);
}

static void __exit seth_exit(void)
{
	platform_driver_unregister(&seth_driver);
}

module_init (seth_init);
module_exit (seth_exit);

#ifdef CONFIG_DEBUG_FS
static int seth_debug_show(struct seq_file *m, void *v)
{
	struct SEth *seth = (struct SEth*)(m->private);
	struct seth_dtrans_stats *stats;
	struct seth_init_data *pdata;

	if(!seth) {
		SETH_ERR("invalid data, seth is NULL\n");
		return -EINVAL;
	}

	pdata = seth->pdata;
	stats = &seth->dt_stats;

	seq_printf(m, "******************************************************************\n");
	seq_printf(m, "DEVICE: %s, state %d, NET_SKB_PAD %u\n", pdata->name, seth->state, NET_SKB_PAD);
	seq_printf(m, "\nRX statistics:\n");
	seq_printf(m, "rx_pkt_max=%u, rx_pkt_min=%u, rx_sum=%u, rx_cnt=%u\n",
			stats->rx_pkt_max, stats->rx_pkt_min, stats->rx_sum, stats->rx_cnt);
	seq_printf(m, "rx_alloc_fails=%u, rx_busy=%d\n", stats->rx_alloc_fails, atomic_read(&seth->rx_busy));

	seq_printf(m, "\nTX statistics:\n");
	seq_printf(m, "tx_pkt_max=%u, tx_pkt_min=%u, tx_sum=%u, tx_cnt=%u\n",
			stats->tx_pkt_max, stats->tx_pkt_min, stats->tx_sum, stats->tx_cnt);
	seq_printf(m, "tx_ping_cnt=%u, tx_ack_cnt=%u\n",
			stats->tx_ping_cnt, stats->tx_ack_cnt);
	seq_printf(m, "******************************************************************\n");
	return 0;
}

static int seth_debug_open(struct inode *inode, struct file *file)
{
        return single_open(file, seth_debug_show, inode->i_private);
}

static const struct file_operations seth_debug_fops = {
        .open = seth_debug_open,
        .read = seq_read,
        .llseek = seq_lseek,
        .release = single_release,
};

static int seth_debugfs_mknod(void *root, void * data)
{
	struct SEth *seth = (struct SEth*) data;
	struct seth_init_data *pdata;

	if (!seth) {
		return -ENODEV;
	}
	pdata = seth->pdata;

        if (!root)
                return -ENXIO;

        debugfs_create_file(pdata->name, S_IRUGO, (struct dentry *)root, data, &seth_debug_fops);
        return 0;
}

static int __init seth_debugfs_init(void)
{
	root = debugfs_create_dir("seth", NULL);
	if (!root) {
		SETH_ERR("failed to create seth debugfs dir\n");
		return -ENODEV;
	}

	debugfs_create_u32("print_seq", S_IRUGO, root, &seth_print_seq);
	debugfs_create_u32("print_ipid", S_IRUGO,
		root, &seth_print_ipid);
	debugfs_create_u32("gro_enable", S_IRUGO,
		root, &seth_gro_enable);

	return 0;
}

fs_initcall(seth_debugfs_init);
#endif

MODULE_AUTHOR("Qiu Yi");
MODULE_DESCRIPTION ("Spreadtrum Ethernet device driver");
MODULE_LICENSE ("GPL");

