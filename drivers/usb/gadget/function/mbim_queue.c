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

struct mbim_queue *dev;
static struct sk_buff_head	shared_dl;

int mbim_queue_empty(void)
{
	return skb_queue_empty(&shared_dl);
}

int mbim_queue_head(struct sk_buff *skb)
{

	pr_info("%s: +++\n", __func__);
	skb_queue_head(&shared_dl, skb);
	wake_up(&dev->dl_wait);
	pr_info("%s: ---\n", __func__);
	return 0;
}

int mbim_queue_tail(struct sk_buff *skb)
{
	pr_info("%s: +++\n", __func__);
	skb_queue_tail(&shared_dl, skb);
	wake_up(&dev->dl_wait);
	pr_info("%s: ---\n", __func__);
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
	spin_unlock_irqrestore(&shared_dl.lock, flags);

	return result;
}

void mbim_queue_purge(void)
{
	skb_queue_purge(&shared_dl);
}

int mbim_read_opened(void)
{
	return atomic_read(&dev->open_excl);
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

	return dev;
}

int mbim_send_skb_to_cpif(struct sk_buff *skb)
{
	return mbim_xmit(skb);
}
