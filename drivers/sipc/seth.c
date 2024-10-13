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
#include <asm/byteorder.h>
#include <linux/tty.h>
#include <linux/platform_device.h>
#ifdef CONFIG_OF
#include <linux/of_device.h>
#endif

#include <linux/sipc.h>
#include <linux/seth.h>

/* debugging macros */
#define SETH_INFO(x...)		pr_info("SETH: " x)
#define SETH_DEBUG(x...)	pr_debug("SETH: " x)
#define SETH_ERR(x...)		pr_err("SETH: Error: " x)

#define SETH_BLOCK_SIZE	(ETH_HLEN + ETH_DATA_LEN + NET_IP_ALIGN)

#define DEV_ON 1
#define DEV_OFF 0

#define SETH_RESEND_MAX_NUM	10
/*
 * Device instance data.
 */
typedef struct SEth {
	struct net_device_stats stats;	/* net statistics */
	struct net_device* netdev;	/* Linux net device */
	struct seth_init_data* pdata;	/* platform data */
	int state;			/* device state */
	int txstate;			/* device txstate */
	int stopped;		/* sblock indicator */
	int txrcnt;			/* seth tx resend count*/
} SEth;

/*
 * Tx_ready handler.
 */
static void
seth_tx_ready_handler (void* data)
{
	SEth* seth = (SEth*) data;

	printk(KERN_INFO "seth_tx_ready_handler state %0x\n", seth->state);
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
		/*
		netif_carrier_on (seth->netdev);
		*/
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

	printk(KERN_INFO "seth_tx_open_handler state %0x\n", seth->state);
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

	printk(KERN_INFO "seth_tx_close_handler state %0x\n", seth->state);
	if (seth->state != DEV_OFF) {
		seth->state = DEV_OFF;
		seth->txstate = DEV_OFF;
		if (netif_carrier_ok (seth->netdev)) {
			netif_carrier_off (seth->netdev);
		}
	}
}

static void
seth_rx_handler (void* data)
{
	SEth* seth = (SEth *)data;
	struct seth_init_data *pdata = seth->pdata;
	struct sblock blk;
	struct sk_buff* skb;
	int ret;

	if (seth->state != DEV_ON) {
		SETH_ERR ("rx_handler the state of %s is off!\n", seth->netdev->name);
		seth->stats.rx_errors++;
		ret = sblock_receive(pdata->dst, pdata->channel, &blk, -1);
		if (ret) {
			SETH_ERR ("receive sblock failed (%d)\n", ret);
			seth->stats.rx_errors++;
			return;
		}
		goto rx_failed;
	}

	ret = sblock_receive(pdata->dst, pdata->channel, &blk, -1);
	if (ret) {
		SETH_ERR ("receive sblock failed (%d)\n", ret);
		seth->stats.rx_errors++;
		goto rx_failed;
	}
	
	skb = dev_alloc_skb (blk.length + NET_IP_ALIGN); //16 bytes align
	if (!skb) {
		SETH_ERR ("alloc skbuff failed!\n");
		seth->stats.rx_dropped++;
		goto rx_failed;
	}

	skb_reserve(skb, NET_IP_ALIGN);

	memcpy(skb->data, blk.addr, blk.length);

	skb_put (skb, blk.length);

	skb->dev = seth->netdev;
	skb->protocol  = eth_type_trans (skb, seth->netdev);
	skb->ip_summed = CHECKSUM_NONE;

	seth->stats.rx_packets++;
	seth->stats.rx_bytes += skb->len;

	netif_rx (skb);

	seth->netdev->last_rx = jiffies;

rx_failed:
	ret = sblock_release(pdata->dst, pdata->channel, &blk);
	if (ret) {
		SETH_ERR ("release sblock failed (%d)\n", ret);
	}
	return;
}

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
		netif_wake_queue(seth->netdev);
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

/*
 * Transmit interface
 */
static int
seth_start_xmit (struct sk_buff* skb, struct net_device* dev)
{
	SEth* seth    = netdev_priv (dev);
	struct seth_init_data *pdata = seth->pdata;
	struct sblock blk;
	int ret;

	if (seth->state != DEV_ON) {
		SETH_ERR ("xmit the state of %s is off\n", dev->name);
		netif_carrier_off (dev);
		seth->stats.tx_carrier_errors++;
		dev_kfree_skb_any(skb);
		return NETDEV_TX_OK;
	}
	/*
	 * Get a free sblock.
	 */

	ret = sblock_get(pdata->dst, pdata->channel, &blk, 0);
	if(ret) {
		SETH_INFO("Get free sblock failed(%d), drop data!\n", ret);
		seth->stats.tx_fifo_errors++;
		netif_stop_queue (dev);
		seth->txstate = DEV_OFF;
		return NETDEV_TX_BUSY;
	}

	if(blk.length < skb->len) {
		SETH_ERR("The size of sblock is so tiny!\n");
		sblock_put(pdata->dst, pdata->channel, &blk);
		seth->stats.tx_fifo_errors++;
		dev_kfree_skb_any(skb);
		seth->txrcnt = 0;
		return NETDEV_TX_OK;
	}

	blk.length = skb->len;
	memcpy (blk.addr, skb->data, skb->len);
	ret = sblock_send(pdata->dst, pdata->channel, &blk);
	if(ret) {
		SETH_INFO("send sblock failed(%d)!\n", ret);
		sblock_put(pdata->dst, pdata->channel, &blk);
		seth->stats.tx_fifo_errors++;
		if (seth->txrcnt > SETH_RESEND_MAX_NUM) {
			netif_stop_queue (dev);
			seth->txstate = DEV_OFF;
		}
		seth->txrcnt ++;
		return NETDEV_TX_BUSY;
	}

	/*
	 * Statistics.
	 */
	seth->stats.tx_bytes += skb->len;
	seth->stats.tx_packets++;
	dev->trans_start = jiffies;
	seth->txrcnt = 0;

	dev_kfree_skb_any(skb);

	return NETDEV_TX_OK;
}

/*
 * Open interface
 */
static int seth_open (struct net_device *dev)
{
	SEth* seth = netdev_priv(dev);

	/* Reset stats */
	memset(&seth->stats, 0, sizeof(seth->stats));

	if (seth->state == DEV_ON && !netif_carrier_ok(seth->netdev)) {
		SETH_INFO("seth_open netif_carrier_on\n");
		netif_carrier_on(seth->netdev);
	}

	seth->txstate = DEV_ON;

	netif_start_queue(dev);

	return 0;
}

/*
 * Close interface
 */
static int seth_close (struct net_device *dev)
{
	SEth* seth = netdev_priv(dev);

	netif_stop_queue(dev);

	/*
	seth->state = DEV_OFF;
	*/

	seth->txstate = DEV_OFF;

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

static int  seth_probe(struct platform_device *pdev)
{
	struct seth_init_data *pdata = pdev->dev.platform_data;
	struct net_device* netdev;
	SEth* seth;
	char ifname[IFNAMSIZ];
	int ret;

	if (pdev->dev.of_node && !pdata) {
		ret = seth_parse_dt(&pdata, &pdev->dev);
		if (ret) {
			printk(KERN_ERR "failed to parse seth device tree, ret=%d\n", ret);
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
	seth->stopped = 0;

	netdev->netdev_ops = &seth_ops;
	netdev->watchdog_timeo = 1*HZ;
	netdev->irq = 0;
	netdev->dma = 0;

	random_ether_addr(netdev->dev_addr);

	ret = sblock_create(pdata->dst, pdata->channel,
		pdata->blocknum, SETH_BLOCK_SIZE,
		pdata->blocknum, SETH_BLOCK_SIZE);
	if (ret) {
		SETH_ERR ("create sblock failed (%d)\n", ret);
		free_netdev(netdev);
		seth_destroy_pdata(&pdata);
		return ret;
	}

	ret = sblock_register_notifier(pdata->dst, pdata->channel, seth_handler, seth);
	if (ret) {
		SETH_ERR ("regitster notifier failed (%d)\n", ret);
		free_netdev(netdev);
		sblock_destroy(pdata->dst, pdata->channel);
		return ret;
	}

	/* register new Ethernet interface */
	if ((ret = register_netdev (netdev))) {
		SETH_ERR ("register_netdev() failed (%d)\n", ret);
		free_netdev(netdev);
		sblock_destroy(pdata->dst, pdata->channel);
		seth_destroy_pdata(&pdata);
		return ret;
	}

	/* set link as disconnected */
	netif_carrier_off (netdev);

	platform_set_drvdata(pdev, seth);
	return 0;
}

/*
 * Cleanup Ethernet device driver.
 */
static int  seth_remove (struct platform_device *pdev)
{
	struct SEth* seth = platform_get_drvdata(pdev);
	struct seth_init_data *pdata = seth->pdata;

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

MODULE_DESCRIPTION ("Spreadtrum Ethernet device driver");
MODULE_LICENSE ("GPL");


