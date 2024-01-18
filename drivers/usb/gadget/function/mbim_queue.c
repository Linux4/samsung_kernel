/*
 * Copyright (C) 2011 Samsung Electronics.
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
#include <linux/mbim_queue.h>
#include <linux/netdevice.h>

#ifdef CONFIG_OF
#include <linux/of_device.h>
#include <linux/of_gpio.h>
#endif

struct mbim_queue *dev;
static struct sk_buff_head	shared_dl;

int mbim_queue_empty(void)
{
	return skb_queue_empty(&shared_dl);
}

int mbim_queue_head(struct sk_buff *skb)
{
	unsigned long flags;

	if (dev == NULL) {
		pr_err("%s: no device\n", __func__);
		return -ENODEV;
	}

	if (dev->queue_cnt < MAX_QUEUE_CNT) {
		skb_queue_head(&shared_dl, skb);

		spin_lock_irqsave(&shared_dl.lock, flags);
		dev->queue_cnt++;
		spin_unlock_irqrestore(&shared_dl.lock, flags);

		dev->stats.rx_packets++;
		dev->stats.rx_bytes += skb->len;
	} else {
		dev_kfree_skb_any(skb);
		dev->stats.rx_dropped++;
	}
	wake_up(&dev->dl_wait);
	return 0;
}

int mbim_queue_tail(struct sk_buff *skb)
{
	skb_queue_tail(&shared_dl, skb);
	wake_up(&dev->dl_wait);
	return 0;
}

struct sk_buff *mbim_dequeue(void)
{
	unsigned long flags;
	struct sk_buff *result;

	spin_lock_irqsave(&shared_dl.lock, flags);
	result = __skb_dequeue(&shared_dl);
	spin_unlock_irqrestore(&shared_dl.lock, flags);
	return result;
}

struct sk_buff *mbim_dequeue_tail(void)
{
	unsigned long flags;
	struct sk_buff *result;

	spin_lock_irqsave(&shared_dl.lock, flags);
	result = __skb_dequeue_tail(&shared_dl);
	dev->queue_cnt--;
	spin_unlock_irqrestore(&shared_dl.lock, flags);

	return result;
}

void mbim_queue_purge(void)
{
	skb_queue_purge(&shared_dl);
}

void mnet_start_queue(struct mbim_queue *dev)
{
	atomic_set(&dev->open_excl, 1);
}

void mnet_stop_queue(struct mbim_queue *dev)
{
	atomic_set(&dev->open_excl, 0);
}

int mbim_read_opened(void)
{
	return atomic_read(&dev->open_excl);
}

void mbim_enter_u3(void)
{
	if (!dev) {
		pr_err("%s: dev init not yet\n", __func__);
		return;
	}

	atomic_set(&dev->usb_enter_u3, 1);
}

void mbim_exit_u3(void)
{
	if (!dev) {
		pr_err("%s: dev init not yet\n", __func__);
		return;
	}

	atomic_set(&dev->usb_enter_u3, 0);
}

int mbim_read_u3(void)
{
	if (!dev) {
		pr_err("%s: dev init not yet\n", __func__);
		return 0;
	}

	return atomic_read(&dev->usb_enter_u3);
}
void request_wakeup(void)
{
	// add gpio contorol function
	printk("usb: %s : Set gpio low \n",__func__);
	gpio_direction_output(dev->wakeup_gpio, 0);
	udelay(50);
	gpio_direction_output(dev->wakeup_gpio, 1);
	/* We do not know how long to wait for PC to wakeup */
	udelay(10);
}
void register_mbim_gpio_info(int gpio)
{
	if (dev)
		dev->wakeup_gpio = gpio;
}

struct mbim_queue *mbim_queue_init(void)
{
	if (dev)
		return dev;

	dev = kzalloc(sizeof(*dev), GFP_KERNEL);
	if (!dev) {
		return ERR_PTR(-ENOMEM);
	}

	init_waitqueue_head(&dev->dl_wait);

	skb_queue_head_init(&shared_dl);

	dev->stats.rx_packets = 0;
	dev->stats.tx_packets = 0;
	dev->stats.rx_bytes = 0;
	dev->stats.tx_bytes = 0;
	dev->stats.rx_dropped = 0;
	dev->stats.tx_dropped = 0;

	return dev;
}

int mbim_send_skb_to_cpif(struct sk_buff *skb)
{
	dev->stats.tx_packets++;
	dev->stats.tx_bytes += skb->len;

	return mbim_xmit(skb);
}
