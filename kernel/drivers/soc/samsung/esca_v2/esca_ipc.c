/*
 * Copyright (c) 2015 Samsung Electronics Co., Ltd.
 *		http://www.samsung.com
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include "esca_drv.h"

u32 ESCA_MBOX__APP = -1, ESCA_MBOX__PHY = -1;

unsigned int esca_ipc_request_channel(struct device_node *np, ipc_callback handler,
		unsigned int *id, unsigned int *size)
{
	struct esca_ipc_info *ipc;
	struct callback_info *cb;
	int i, len, req_ch_id, _req_ch_id;
	const __be32 *prop;

	if (!np)
		return -ENODEV;

	prop = of_get_property(np, "esca-ipc-channel", &len);
	if (!prop)
		return -ENOENT;
	req_ch_id = be32_to_cpup(prop);
	_req_ch_id = req_ch_id & ESCA_IPC_CHANNEL_MASK;

	if (req_ch_id & ESCA_IPC_LAYER_MASK)
		ipc = &exynos_esca[ESCA_MBOX__APP]->ipc;
	else
		ipc = &exynos_esca[ESCA_MBOX__PHY]->ipc;

	for(i = 0; i < ipc->num_channels; i++) {
		if (ipc->channel[i].id == _req_ch_id) {
			*id = req_ch_id;
			*size = ipc->channel[i].tx_ch.size;

			if (handler) {
				cb = devm_kzalloc(ipc->dev, sizeof(struct callback_info),
						GFP_KERNEL);
				if (cb == NULL)
					return -ENOMEM;
				cb->ipc_callback = handler;
				cb->client = np;

				spin_lock(&ipc->channel[i].ch_lock);
				list_add(&cb->list, &ipc->channel[i].list);
				spin_unlock(&ipc->channel[i].ch_lock);
			}

			return 0;
		}
	}

	return -ENODEV;
}
EXPORT_SYMBOL_GPL(esca_ipc_request_channel);

unsigned int esca_ipc_release_channel(struct device_node *np, unsigned int channel_id)
{
	struct esca_ipc_info *ipc;
	struct esca_ipc_ch *channel;
	struct list_head *cb_list;
	struct callback_info *cb;

	if (channel_id & ESCA_IPC_LAYER_MASK)
		ipc = &exynos_esca[ESCA_MBOX__APP]->ipc;
	else
		ipc = &exynos_esca[ESCA_MBOX__PHY]->ipc;

	channel = &ipc->channel[channel_id & ESCA_IPC_CHANNEL_MASK];
	cb_list = &channel->list;

	list_for_each_entry(cb, cb_list, list) {
		if (cb->client == np) {
			spin_lock(&channel->ch_lock);
			list_del(&cb->list);
			spin_unlock(&channel->ch_lock);
			devm_kfree(ipc->dev, cb);
			break;
		}
	}

	return 0;
}
EXPORT_SYMBOL_GPL(esca_ipc_release_channel);

static bool check_response(struct esca_ipc_info *ipc,
			   struct esca_ipc_ch *channel,
			   struct ipc_config *cfg)
{
	unsigned int front;
	unsigned int rear;
	struct list_head *cb_list = &channel->list;
	struct callback_info *cb;
	unsigned int tmp_seq_num;
	bool ret = true;
	unsigned int i;
	unsigned long flags;

	spin_lock_irqsave(&channel->rx_lock, flags);
	/* IPC command dequeue */
	front = __raw_readl(channel->rx_ch.front);
	rear = __raw_readl(channel->rx_ch.rear);

	i = rear;

	while (i != front) {
		tmp_seq_num = __raw_readl(channel->rx_ch.base + channel->rx_ch.size * i);
		tmp_seq_num = (tmp_seq_num >> ESCA_IPC_PROTOCOL_SEQ_NUM) & 0x3f;
		if (tmp_seq_num == ((cfg->cmd[0] >> ESCA_IPC_PROTOCOL_SEQ_NUM) & 0x3f)) {
			memcpy_align_4(cfg->cmd, channel->rx_ch.base + channel->rx_ch.size * i,
					channel->rx_ch.size);
			memcpy_align_4(channel->cmd, channel->rx_ch.base + channel->rx_ch.size * i,
					channel->rx_ch.size);

			/* i: target command, rear: another command
			 * 1. i index command dequeue
			 * 2. rear index command copy to i index position
			 * 3. incresed rear index
			 */
			if (i != rear)
				memcpy_align_4(channel->rx_ch.base + channel->rx_ch.size * i,
						channel->rx_ch.base + channel->rx_ch.size * rear,
						channel->rx_ch.size);

			list_for_each_entry(cb, cb_list, list)
				if (cb && cb->ipc_callback)
					cb->ipc_callback(channel->cmd, channel->rx_ch.size);

			rear++;
			rear = rear % channel->rx_ch.len;

			__raw_writel(rear, channel->rx_ch.rear);
			front = __raw_readl(channel->rx_ch.front);

			if (!channel->interrupt && rear == front) {
				esca_interrupt_clr(ipc, channel->id);
				if (rear != __raw_readl(channel->rx_ch.front))
					esca_interrupt_self_gen(ipc, channel->id);
			}
			ret = false;
			channel->seq_num_flag[tmp_seq_num] = 0;
			break;
		}
		i++;
		i = i % channel->rx_ch.len;
	}

	spin_unlock_irqrestore(&channel->rx_lock, flags);

	return ret;
}

static void dequeue_policy(struct esca_ipc_ch *channel)
{
	unsigned int front;
	unsigned int rear;
	struct list_head *cb_list = &channel->list;
	struct callback_info *cb;
	unsigned long flags;

	spin_lock_irqsave(&channel->rx_lock, flags);

	if (channel->type == TYPE_BUFFER) {
		memcpy_align_4(channel->cmd, channel->rx_ch.base, channel->rx_ch.size);
		spin_unlock_irqrestore(&channel->rx_lock, flags);
		list_for_each_entry(cb, cb_list, list)
			if (cb && cb->ipc_callback)
				cb->ipc_callback(channel->cmd, channel->rx_ch.size);

		return;
	}

	/* IPC command dequeue */
	front = __raw_readl(channel->rx_ch.front);
	rear = __raw_readl(channel->rx_ch.rear);

	while (rear != front) {
		memcpy_align_4(channel->cmd, channel->rx_ch.base + channel->rx_ch.size * rear,
				channel->rx_ch.size);

		list_for_each_entry(cb, cb_list, list)
			if (cb && cb->ipc_callback)
				cb->ipc_callback(channel->cmd, channel->rx_ch.size);

		rear++;
		rear = rear % channel->rx_ch.len;

		if (!channel->polling)
			complete(&channel->wait);

		__raw_writel(rear, channel->rx_ch.rear);
		front = __raw_readl(channel->rx_ch.front);
	}

	esca_log_idx_update();
	spin_unlock_irqrestore(&channel->rx_lock, flags);
}

static irqreturn_t esca_ipc_irq_handler(int irq, void *data)
{
	struct esca_ipc_info *ipc = data;
	unsigned int status;
	int i;

	/* ESCA IPC INTERRUPT STATUS REGISTER */
	status = get_esca_interrupt_status(ipc);
	ipc->intr_status = 0;

	for (i = 0; i < ipc->num_channels; i++) {
		if (status & (0x1 << ipc->channel[i].id)) {
			if (ipc->channel[i].interrupt) {
				esca_interrupt_clr(ipc, ipc->channel[i].id);
				complete(&ipc->channel[i].wait);
			} else if(!ipc->channel[i].polling) {
				esca_interrupt_clr(ipc, ipc->channel[i].id);
				ipc->intr_status = (1 << i);
				dequeue_policy(&ipc->channel[i]);
			}
		}
	}

	/*
	 * Threaded IRQ wake is unused.
	if (ipc->intr_status)
		return IRQ_WAKE_THREAD;
	*/

	return IRQ_HANDLED;
}

static irqreturn_t esca_ipc_irq_handler_thread(int irq, void *data)
{
	struct esca_ipc_info *ipc = data;
	int i;

	for (i = 0; i < ipc->num_channels; i++)
		if (!ipc->channel[i].polling && (ipc->intr_status & (1 << i)))
			dequeue_policy(&ipc->channel[i]);

	return IRQ_HANDLED;
}

static int __esca_ipc_send_data(struct esca_ipc_info *ipc, unsigned int channel_id,
			 struct ipc_config *cfg, bool w_mode)
{
	unsigned int front;
	unsigned int rear;
	unsigned int tmp_index;
	struct esca_ipc_ch *channel;
	bool timeout_flag = 0;
	int ret;
	u64 timeout, now;
	u32 retry_cnt = 0;
	u32 tmp_seq_num;
	u32 seq_cnt = 0;
	unsigned long flags;

	if (channel_id >= ipc->num_channels && !cfg)
		return -EIO;

	esca_ktime_sram_sync();
	channel = &ipc->channel[channel_id];

	if (channel->interrupt && cfg->response)
		mutex_lock(&channel->wait_lock);

	spin_lock_irqsave(&channel->tx_lock, flags);

	front = __raw_readl(channel->tx_ch.front);
	rear = __raw_readl(channel->tx_ch.rear);

	tmp_index = front + 1;

	if (tmp_index >= channel->tx_ch.len)
		tmp_index = 0;

	/* buffer full check */
	UNTIL_EQUAL(true, tmp_index != __raw_readl(channel->tx_ch.rear), timeout_flag);
	if (timeout_flag) {
		esca_log_print();
		exynos_esca[ESCA_MBOX__PHY]->dbg.debug_log_level = 1;
		spin_unlock_irqrestore(&channel->tx_lock, flags);
		if (channel->interrupt && cfg->response)
			mutex_unlock(&channel->wait_lock);
		pr_err("[%s] tx buffer full! timeout!!!\n", __func__);
		return -ETIMEDOUT;
	}

	if (!cfg->cmd) {
		spin_unlock_irqrestore(&channel->tx_lock, flags);
		if (channel->interrupt && cfg->response)
			mutex_unlock(&channel->wait_lock);
		return -EIO;
	}

	tmp_seq_num = channel->seq_num;
	do {
		if (unlikely(tmp_seq_num != channel->seq_num)) {
			pr_warn("[ESCA IPC] [ESCA_IPC] channel:%d, cmd:0x%x, 0x%x, 0x%x, 0x%x",
					channel->id, cfg->cmd[0], cfg->cmd[1],
					cfg->cmd[2], cfg->cmd[3]);
			pr_warn("[ESCA IPC] duplicate assignment: sequence number:%d, tmp_seq_num:%d, flag:0x%x",
					channel->seq_num, tmp_seq_num, channel->seq_num_flag[tmp_seq_num]);
		}

		if (++tmp_seq_num == SEQUENCE_NUM_MAX)
			tmp_seq_num = 1;

		if (unlikely(seq_cnt++ == SEQUENCE_NUM_MAX)) {
			pr_err("[ESCA IPC] sequence number full! error!!!\n");
			BUG();
		}

	} while (channel->seq_num_flag[tmp_seq_num]);

	channel->seq_num = tmp_seq_num;
	if (channel->polling && cfg->response)
		channel->seq_num_flag[channel->seq_num] = cfg->cmd[0] | (0x1 << 31);

	cfg->cmd[0] &= ~(0x3f << ESCA_IPC_PROTOCOL_SEQ_NUM);
	cfg->cmd[0] |= (channel->seq_num & 0x3f) << ESCA_IPC_PROTOCOL_SEQ_NUM;

	memcpy_align_4(channel->tx_ch.base + channel->tx_ch.size * front, cfg->cmd,
			channel->tx_ch.size);

	cfg->cmd[1] = 0;
	cfg->cmd[2] = 0;
	cfg->cmd[3] = 0;

	writel(tmp_index, channel->tx_ch.front);

	esca_interrupt_gen(ipc, channel->id);
	spin_unlock_irqrestore(&channel->tx_lock, flags);

	if (channel->polling && cfg->response && !channel->interrupt) {
retry:
		timeout = sched_clock() + IPC_TIMEOUT;
		timeout_flag = false;

		while (!(get_esca_interrupt_status(ipc) & (1 << channel->id)) ||
				check_response(ipc, channel, cfg)) {
			now = sched_clock();
			if (timeout < now) {
				if (retry_cnt > 5) {
					timeout_flag = true;
					break;
				} else if (retry_cnt > 0) {
					pr_err("esca_ipc timeout retry %d "
						"now = %llu,"
						"timeout = %llu\n",
						retry_cnt, now, timeout);
					++retry_cnt;
					goto retry;
				} else {
					++retry_cnt;
					continue;
				}
			} else {
				if (w_mode)
					usleep_range(50, 100);
				else
					udelay(10);
			}
		}
	} else if (channel->interrupt && cfg->response) {
		timeout = sched_clock() + IPC_TIMEOUT * 5;
		do {
			ret = wait_for_completion_interruptible_timeout(&channel->wait,
				  nsecs_to_jiffies(IPC_TIMEOUT));
			now = sched_clock();
			if (timeout < now) {
				timeout_flag = true;
				break;
			}
		} while (check_response(ipc, channel, cfg));
		mutex_unlock(&channel->wait_lock);
	} else {
		return 0;
	}

	if (timeout_flag) {
		if (!check_response(ipc, channel, cfg))
			return 0;
		pr_err("%s Timeout error! now = %llu, timeout = %llu\n",
				__func__, now, timeout);
		pr_err("[ESCA] int_status:0x%x, ch_id: 0x%x\n",
				get_esca_interrupt_status(ipc),
				1 << channel->id);
		pr_err("[ESCA] queue, rx_rear:%u, rx_front:%u\n",
				__raw_readl(channel->rx_ch.rear),
				__raw_readl(channel->rx_ch.front));
		pr_err("[ESCA] queue, tx_rear:%u, tx_front:%u\n",
				__raw_readl(channel->tx_ch.rear),
				__raw_readl(channel->tx_ch.front));

		exynos_esca[ESCA_MBOX__PHY]->dbg.debug_log_level = 1;
		esca_log_print();
		exynos_esca[ESCA_MBOX__PHY]->dbg.debug_log_level = 0;
		esca_ramdump();

		dump_stack();
		mdelay(1000);
		dbg_snapshot_expire_watchdog();
	}

	if (!is_esca_stop_log) {
		if (exynos_esca[ESCA_MBOX__PHY]->dbg.debug_log_level)
			queue_work(update_log_wq, &exynos_esca[ESCA_MBOX__PHY]->dbg.update_log_work);
		else
			esca_log_idx_update();
	}

	return 0;
}

int esca_ipc_send_data(unsigned int channel_id, struct ipc_config *cfg)
{
	struct esca_ipc_info *ipc;
	unsigned int _channel_id = channel_id & ESCA_IPC_CHANNEL_MASK;

	if (channel_id & ESCA_IPC_LAYER_MASK)
		ipc = &exynos_esca[ESCA_MBOX__APP]->ipc;
	else
		ipc = &exynos_esca[ESCA_MBOX__PHY]->ipc;

	return __esca_ipc_send_data(ipc, _channel_id, cfg, false);

}
EXPORT_SYMBOL_GPL(esca_ipc_send_data);

bool is_esca_ipc_busy(unsigned ch_id)
{
	struct esca_ipc_ch *channel;
	unsigned int tx_front, rx_front;
	unsigned int _ch_id = ch_id & ESCA_IPC_CHANNEL_MASK;

	if (ch_id & ESCA_IPC_LAYER_MASK)
		channel = &exynos_esca[ESCA_MBOX__APP]->ipc.channel[_ch_id];
	else
		channel = &exynos_esca[ESCA_MBOX__PHY]->ipc.channel[_ch_id];

	tx_front = __raw_readl(channel->tx_ch.front);
	rx_front = __raw_readl(channel->rx_ch.front);

	return (tx_front != rx_front);
}
EXPORT_SYMBOL_GPL(is_esca_ipc_busy);

static int esca_ipc_die_handler(struct notifier_block *nb,
		unsigned long l, void *buf)
{
	if (!esca_stop_log_req)
		esca_stop_log();
	return NOTIFY_DONE;
}

static struct notifier_block nb_die_block = {
	.notifier_call = esca_ipc_die_handler,
};

static struct notifier_block nb_panic_block = {
	.notifier_call = esca_ipc_die_handler,
	.priority = INT_MAX,
};

static int channel_init(struct esca_ipc_info *ipc, u32 *ch_buf, int len)
{
	int i, j;
	unsigned int mask = 0;
	struct ipc_channel *ipc_ch;

	ipc->num_channels = ipc->initdata->ipc_ap_max;

	ipc->channel = devm_kzalloc(ipc->dev,
			sizeof(struct esca_ipc_ch) * ipc->num_channels, GFP_KERNEL);

	for (i = 0; i < ipc->num_channels; i++) {
		ipc_ch = (struct ipc_channel *)(ipc->sram_base + ipc->initdata->ipc_channels);
		ipc->channel[i].polling = ipc_ch[i].ap_poll;
		ipc->channel[i].id = ipc_ch[i].id;
		ipc->channel[i].type = ipc_ch[i].type;
		mask |= ipc->channel[i].polling << ipc->channel[i].id;

		/* Channel's RX buffer info */
		ipc->channel[i].rx_ch.size = ipc_ch[i].ch.q_elem_size;
		ipc->channel[i].rx_ch.len = ipc_ch[i].ch.q_len;
		ipc->channel[i].rx_ch.rear = ipc->sram_base + ipc_ch[i].ch.tx_rear;
		ipc->channel[i].rx_ch.front = ipc->sram_base + ipc_ch[i].ch.tx_front;
		ipc->channel[i].rx_ch.base = ipc->sram_base + ipc_ch[i].ch.tx_base;
		/* Channel's TX buffer info */
		ipc->channel[i].tx_ch.size = ipc_ch[i].ch.q_elem_size;
		ipc->channel[i].tx_ch.len = ipc_ch[i].ch.q_len;
		ipc->channel[i].tx_ch.rear = ipc->sram_base + ipc_ch[i].ch.rx_rear;
		ipc->channel[i].tx_ch.front = ipc->sram_base + ipc_ch[i].ch.rx_front;
		ipc->channel[i].tx_ch.base = ipc->sram_base + ipc_ch[i].ch.rx_base;
		ipc->channel[i].cmd = devm_kzalloc(ipc->dev,
				ipc->channel[i].tx_ch.size, GFP_KERNEL);

		init_completion(&ipc->channel[i].wait);
		spin_lock_init(&ipc->channel[i].rx_lock);
		spin_lock_init(&ipc->channel[i].tx_lock);
		spin_lock_init(&ipc->channel[i].ch_lock);
		INIT_LIST_HEAD(&ipc->channel[i].list);
		mutex_init(&ipc->channel[i].wait_lock);

		if (!ch_buf)
			continue;

		for (j = 0; j < len; j++) {
			if (i == ch_buf[j]) {
				ipc->channel[i].interrupt = true;
				mask &= ~(0x1 << i);
				pr_info("esca interrupt-ch #%d enabled\n", i);
			}
		}
	}
	for (i = 0; i < ESCA_IPC_NUM_MAX; i++)
		esca_interrupt_clr(ipc, i);
	esca_interrupt_mask(ipc, mask);

	return 0;
}

static int esca_ipc_dt_parse(struct platform_device *pdev)
{
	int ret = 0;
	struct device_node *np = pdev->dev.of_node;
	struct esca_info *esca;
	const __be32 *prop;
	unsigned int len, val;
	struct resource *res;
	u32 *ch_buf;

	prop = of_get_property(np, "esca-layer", &len);
	val = be32_to_cpup(prop);
	esca = exynos_esca[val];

	res = platform_get_resource_byname(pdev, IORESOURCE_MEM, "mbox");
	if (res) {
		if (esca->layer == ESCA_LAYER__APP)
			ESCA_MBOX__APP = esca->layer;
		else if (esca->layer >= ESCA_LAYER__PHY0 && esca->layer <= ESCA_LAYER__PHY1)
			ESCA_MBOX__PHY = esca->layer;
	} else
		return 0;

	esca->ipc.intr = devm_ioremap_resource(&pdev->dev, res);
	if (IS_ERR(esca->ipc.intr))
		return PTR_ERR(esca->ipc.intr);

	esca->ipc.dev = &pdev->dev;

	np = of_get_child_by_name(np, "ipc");
	if (!np) {
		dev_err(&pdev->dev, "ipc info is missing.\n");
		return -ENODEV;
	}

	esca->ipc.initdata = esca->initdata;
	esca->ipc.sram_base = esca->sram_base;
	esca->ipc.irq = irq_of_parse_and_map(np, 0);
	if (!esca->ipc.irq) {
		dev_err(&pdev->dev, "failed to register esca_ipc interrupt:\n");
		return -ENODEV;
	}

	prop = of_get_property(np, "esca-mbox-master", &len);
	if (prop)
		esca->ipc.is_mailbox_master = !!be32_to_cpup(prop);

	/* Get interrupt mode channel info */
	len = of_property_count_u32_elems(np, "interrupt-ch");
	if (len > 0) {
		ch_buf = devm_kzalloc(&pdev->dev, sizeof(u32) * len, GFP_KERNEL);
		if (of_property_read_u32_array(np, "interrupt-ch", ch_buf, len))
			ch_buf = NULL;
	} else {
		ch_buf = NULL;
		dev_info(&pdev->dev, "interrupt channels empty\n");
	}
	channel_init(&esca->ipc, ch_buf, len);

	ret = devm_request_threaded_irq(&pdev->dev, esca->ipc.irq,
			esca_ipc_irq_handler,
			esca_ipc_irq_handler_thread,
			IRQF_ONESHOT,
			dev_name(&pdev->dev), &esca->ipc);
	if (ret < 0) {
		dev_err(&pdev->dev, "failed to request threaded irq:\n");
		return ret;
	}

	if (esca->layer == ESCA_MBOX__PHY) {
		register_die_notifier(&nb_die_block);
		atomic_notifier_chain_register(&panic_notifier_list, &nb_panic_block);
		exynos_reboot_register_esca_ops(exynos_esca_reboot);
	}

	return ret;
}

int esca_ipc_init(struct platform_device *pdev)
{
	int ret = 0;

	ret = esca_ipc_dt_parse(pdev);
	if (ret < 0) {
		dev_err(&pdev->dev, "IPC DT parse failed\n");
		return ret;
	}

	return ret;
}
