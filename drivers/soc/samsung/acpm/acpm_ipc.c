/*
 * Copyright (c) 2015 Samsung Electronics Co., Ltd.
 *		http://www.samsung.com
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/io.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/platform_device.h>
#include <linux/debugfs.h>
#include <linux/interrupt.h>
#include <linux/of_irq.h>
#include <linux/workqueue.h>

#include "acpm.h"
#include "acpm_ipc.h"

static struct acpm_ipc_info *acpm_ipc;

unsigned int acpm_ipc_request_channel(struct device_node *np, ipc_callback handler,
		unsigned int *id, unsigned int *size)
{
	struct device_node *node;
	int i;

	if (!np)
		return -ENODEV;

	node = of_parse_phandle(np, "acpm-ipc-channel", 0);

	if (!node)
		return -ENOENT;

	for(i = 0; i < acpm_ipc->channel_num; i++) {
		if (acpm_ipc->channel[i].ch_node == node) {
			*id = acpm_ipc->channel[i].id;
			*size = acpm_ipc->channel[i].buff_size;

			if (handler)
				acpm_ipc->channel[i].ipc_callback = handler;

			return 0;
		}
	}

	return -ENODEV;
}

unsigned int acpm_ipc_release_channel(unsigned int channel_id)
{
	struct acpm_ipc_ch *channel = &acpm_ipc->channel[channel_id];

	channel->ipc_callback = NULL;

	return 0;
}

static irqreturn_t acpm_ipc_handler(int irq, void *p)
{
	struct acpm_ipc_info *ipc = p;
	unsigned int status;
	unsigned int int_check = 0;
	int i;

	/* ACPM IPC INTERRUPT STATUS REGISTER */
	status = __raw_readl(acpm_ipc->intr + INTSR1);

	for (i = 0; i < acpm_ipc->channel_num; i++) {
		if (status & (0x1 << ipc->channel[i].id)) {
			tasklet_schedule(&ipc->channel[i].dequeue_task);
			/* ACPM IPC INTERRUPT PENDING CLEAR */
			__raw_writel(1 << ipc->channel[i].id, ipc->intr + INTCR1);
		}
		int_check |= 0x1 << ipc->channel[i].id;
	}

	if (status & ~int_check) {
		pr_err("[ACPM] unknown interrupt, status:0x%x\n", status);
		return IRQ_NONE;
	}

	return IRQ_HANDLED;
}

#ifdef CONFIG_ACPM_IPC_PROFILE

#define ACPM_IPC_ITERATION		(1000)
#define ACPM_IPC_CH_NUM			(4)
struct time_data {
	unsigned long min;
	unsigned long max;
	unsigned int cnt;
};

struct time_data time_d[ACPM_IPC_CH_NUM];
struct time_data en_time_d[ACPM_IPC_CH_NUM];
#endif

static void dequeue_policy(struct acpm_ipc_ch *channel)
{
	unsigned int front;
	unsigned int rear;
#ifdef CONFIG_ACPM_IPC_PROFILE
	unsigned long after, before;

	before = sched_clock();
#endif
	spin_lock_bh(&channel->rx_lock);

	/* IPC command dequeue */
	front = __raw_readl(channel->rx_front);
	rear = __raw_readl(channel->rx_rear);

	while (rear != front) {
		memcpy_align_4(channel->cmd, channel->rx_base + channel->buff_size * rear,
				channel->buff_size);

		if (channel->ipc_callback)
			channel->ipc_callback(channel->cmd, channel->buff_size);

		if (channel->buff_len == (rear + 1))
			rear = 0;
		else
			rear++;

		__raw_writel(rear, channel->rx_rear);
		front = __raw_readl(channel->rx_front);
	}

	acpm_log_print();
	spin_unlock_bh(&channel->rx_lock);

#ifdef CONFIG_ACPM_IPC_PROFILE
	after = sched_clock();

	if (time_d[channel->id].max < (after - before))
		time_d[channel->id].max = after - before;
	if (time_d[channel->id].min > (after - before))
		time_d[channel->id].min = after - before;
	time_d[channel->id].cnt += 1;
	if (time_d[channel->id].cnt == ACPM_IPC_ITERATION) {
		pr_err("[dequeue_ACPM][%u] chnnale:%u, min:%lu, max:%lu\n",
				time_d[channel->id].cnt,
				channel->id, time_d[channel->id].min,
				time_d[channel->id].max);
		time_d[channel->id].cnt = 0;
		time_d[channel->id].min = ~0;
		time_d[channel->id].max = 0;
	}
#endif
}

static void dequeue_tasklet(unsigned long data)
{
	struct acpm_ipc_ch *channel = (struct acpm_ipc_ch *)data;

	dequeue_policy(channel);
}

static void apm_interrupt_gen(unsigned int id)
{
	/* APM NVIC INTERRUPT GENERATE */
	__raw_writel((1 << id) << 16, acpm_ipc->intr + INTGR0);
}

int acpm_ipc_send_data(unsigned int channel_id, struct ipc_config *cfg)
{
	unsigned int front;
	unsigned int rear;
	unsigned int tmp_index;
	struct acpm_ipc_ch *channel;
	unsigned int buf;
	bool timeout_flag = 0;
#ifdef CONFIG_ACPM_IPC_PROFILE
	unsigned long after, before;

	before = sched_clock();
#endif
	if (channel_id >= acpm_ipc->channel_num)
		return -EIO;

	channel = &acpm_ipc->channel[channel_id];

	spin_lock(&channel->tx_lock);

	front = __raw_readl(channel->tx_front);
	rear = __raw_readl(channel->tx_rear);

	tmp_index = front + 1;

	if (tmp_index >= channel->buff_len)
		tmp_index = 0;

	/* buffer full check */
	UNTIL_EQUAL(true, tmp_index != __raw_readl(channel->tx_rear), timeout_flag);
	if (timeout_flag) {
		acpm_log_print();
		spin_unlock(&channel->tx_lock);
		return -ETIMEDOUT;
	}

	memcpy_align_4(channel->tx_base + channel->buff_size * front, cfg->cmd,
			channel->buff_size);

	if (cfg->cmd[0] & (1 << ACPM_IPC_PROTOCOL_INDIRECTION)) {
		/* another indirection command check */
		while (rear != front) {
			buf = __raw_readl(channel->tx_base + channel->buff_size * rear);

			if (buf & (1 << ACPM_IPC_PROTOCOL_INDIRECTION)) {

				UNTIL_EQUAL(true, rear != __raw_readl(channel->tx_rear),
						timeout_flag);

				if (timeout_flag) {
					acpm_log_print();
					spin_unlock(&channel->tx_lock);
					return -ETIMEDOUT;
				} else {
					rear = __raw_readl(channel->tx_rear);
				}

			} else {
				if (channel->buff_len == (rear + 1))
					rear = 0;
				else
					rear++;
			}
		}

		if (cfg->indirection)
			memcpy_align_4(channel->tx_direction, cfg->indirection,
					cfg->indirection_size);
		else
			return -EINVAL;
	}

	__raw_writel(tmp_index, channel->tx_front);

	timestamp_write();
	apm_interrupt_gen(channel->id);

	spin_unlock(&channel->tx_lock);

#ifdef CONFIG_ACPM_IPC_PROFILE
	after = sched_clock();
	if (en_time_d[channel->id].max < (after - before))
		en_time_d[channel->id].max = after - before;
	if (en_time_d[channel->id].min > (after - before))
		en_time_d[channel->id].min = after - before;
	en_time_d[channel->id].cnt += 1;
	if (en_time_d[channel->id].cnt == ACPM_IPC_ITERATION) {
		pr_err("[enqueue_ACPM][%u] chnnale:%u, min:%lu, max:%lu\n",
				en_time_d[channel->id].cnt,
				channel->id, en_time_d[channel->id].min,
				en_time_d[channel->id].max);
		en_time_d[channel->id].cnt = 0;
		en_time_d[channel->id].min = ~0;
		en_time_d[channel->id].max = 0;
	}
#endif
	if (channel->polling && cfg->responce) {
		UNTIL_EQUAL(true, !!(__raw_readl(acpm_ipc->intr + INTSR1) & (1 << channel->id)),
				timeout_flag);
		if (timeout_flag) {
			acpm_log_print();
			return -ETIMEDOUT;
		}

		dequeue_policy(channel);

		__raw_writel((1 << channel->id), acpm_ipc->intr + INTCR1);
	}

	return 0;
}

static int channel_init(struct device_node *node, struct acpm_ipc_info *acpm_ipc)
{
	struct device_node *child;
	const __be32 *prop;
	unsigned int len;
	unsigned int max_buff_size;
	int i = 0;
	void __iomem *addr;
	int polling = 0;

	acpm_ipc->channel_num = of_get_child_count(node);

	prop = of_get_property(node, "polling", &len);
	if (prop)
		polling = be32_to_cpup(prop);

	if (polling)
		disable_irq(acpm_ipc->irq);

	acpm_ipc->channel = devm_kzalloc(acpm_ipc->dev,
			sizeof(struct acpm_ipc_ch) * acpm_ipc->channel_num, GFP_KERNEL);

	prop = of_get_property(node, "max_buff_size", &len);
	if (prop)
		max_buff_size = be32_to_cpup(prop);
	else
		max_buff_size = 0x300 * acpm_ipc->channel_num;

	for_each_child_of_node(node, child) {
		acpm_ipc->channel[i].ch_node = child;
		acpm_ipc->channel[i].polling = polling;

		prop = of_get_property(child, "channel_id", &len);
		if (prop)
			acpm_ipc->channel[i].id = be32_to_cpup(prop);
		else
			acpm_ipc->channel[i].id = i;

		prop = of_get_property(child, "buff_size", &len);
		if (prop)
			acpm_ipc->channel[i].buff_size = be32_to_cpup(prop);
		else
			acpm_ipc->channel[i].buff_size = 12;

		prop = of_get_property(child, "buff_len", &len);
		if (prop)
			acpm_ipc->channel[i].buff_len = be32_to_cpup(prop);
		else
			acpm_ipc->channel[i].buff_len = 8;

		prop = of_get_property(child, "direction_buff", &len);
		if (prop)
			acpm_ipc->channel[i].d_buff_size = be32_to_cpup(prop);
		else
			acpm_ipc->channel[i].d_buff_size = 0;

		if (max_buff_size < (acpm_ipc->channel[i].buff_size *
					acpm_ipc->channel[i].buff_len * 2 +
					acpm_ipc->channel[i].d_buff_size + 8)) {
			dev_err(acpm_ipc->dev, "exceed the buff size-"
					"max:%u, size:%u, len:%u, direction:%u\n",
					max_buff_size,
					acpm_ipc->channel[i].buff_size,
					acpm_ipc->channel[i].buff_len,
					acpm_ipc->channel[i].d_buff_size);
			return -ENOMEM;
		}

		prop = of_get_property(child, "rx_buff", &len);
		if (prop)
			addr = ioremap(be32_to_cpup(prop), acpm_ipc->channel[i].buff_size *
					acpm_ipc->channel[i].buff_len + 8);

		acpm_ipc->channel[i].rx_rear = addr;
		acpm_ipc->channel[i].rx_front = addr + 4;
		acpm_ipc->channel[i].rx_base = addr + 8;

		acpm_ipc->channel[i].cmd = devm_kzalloc(acpm_ipc->dev,
				acpm_ipc->channel[i].buff_size, GFP_KERNEL);

		prop = of_get_property(child, "tx_buff", &len);
		if (prop)
			addr = ioremap(be32_to_cpup(prop), acpm_ipc->channel[i].buff_size *
					acpm_ipc->channel[i].buff_len +
					acpm_ipc->channel[i].d_buff_size + 8);

		/* acpm_ipc queue base address init */
		acpm_ipc->channel[i].tx_rear = addr;
		acpm_ipc->channel[i].tx_front = addr + 4;
		acpm_ipc->channel[i].tx_base = addr + 8;
		if (acpm_ipc->channel[i].d_buff_size)
			acpm_ipc->channel[i].tx_direction = addr + 8 +
				acpm_ipc->channel[i].buff_size *
				acpm_ipc->channel[i].buff_len;

		tasklet_init(&acpm_ipc->channel[i].dequeue_task, dequeue_tasklet,
				(unsigned long)&acpm_ipc->channel[i]);

		spin_lock_init(&acpm_ipc->channel[i].rx_lock);
		spin_lock_init(&acpm_ipc->channel[i].tx_lock);
		i++;
	}

	return 0;
}

static int acpm_ipc_probe(struct platform_device *pdev)
{
	struct device_node *node = pdev->dev.of_node;
	struct resource *res;
	int ret = 0;

	if (!node) {
		dev_err(&pdev->dev, "driver doesnt support"
				"non-dt devices\n");
		return -ENODEV;
	}

	dev_info(&pdev->dev, "acpm_ipc probe\n");

	acpm_ipc = devm_kzalloc(&pdev->dev,
			sizeof(struct acpm_ipc_info), GFP_KERNEL);

	res = platform_get_resource(pdev, IORESOURCE_IRQ, 0);
	acpm_ipc->irq = res->start;
	ret = devm_request_irq(&pdev->dev, acpm_ipc->irq,
			acpm_ipc_handler, IRQF_SHARED,
			dev_name(&pdev->dev), acpm_ipc);
	if (ret) {
		dev_err(&pdev->dev, "failed to register acpm_ipc interrupt:%d\n", ret);
		return ret;
	}

	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	acpm_ipc->intr = devm_ioremap_resource(&pdev->dev, res);

	acpm_ipc->channel = devm_kzalloc(&pdev->dev,
			sizeof(struct acpm_ipc_info), GFP_KERNEL);

	if (IS_ERR(acpm_ipc))
		return PTR_ERR(acpm_ipc);

	acpm_ipc->dev = &pdev->dev;

	node = of_get_child_by_name(node, "channels");

	if (node) {
		ret = channel_init(node, acpm_ipc);
	}

	return ret;
}

static int acpm_ipc_remove(struct platform_device *pdev)
{
	return 0;
}

static const struct of_device_id acpm_ipc_match[] = {
	{ .compatible = "samsung,exynos-acpm-ipc" },
	{},
};

static struct platform_driver samsung_acpm_ipc_driver = {
	.probe	= acpm_ipc_probe,
	.remove	= acpm_ipc_remove,
	.driver	= {
		.name = "exynos-acpm-ipc",
		.owner	= THIS_MODULE,
		.of_match_table	= acpm_ipc_match,
	},
};

static int __init exynos_acpm_ipc_init(void)
{

	return platform_driver_register(&samsung_acpm_ipc_driver);
}
arch_initcall(exynos_acpm_ipc_init);
