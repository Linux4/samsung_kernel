// SPDX-License-Identifier: GPL
/*
 * Samsung Exynos SoC series Pablo driver
 *
 * Exynos Pablo image subsystem functions
 *
 * Copyright (c) 2021 Samsung Electronics Co., Ltd
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/module.h>
#include <linux/slab.h>
#include <linux/io.h>
#include <linux/delay.h>
#include <linux/interrupt.h>

#include "pablo-icpu.h"
#include "pablo-icpu-mbox.h"
#include "pablo-icpu-hw.h"

static struct icpu_logger _log = {
	.level = LOGLEVEL_INFO,
	.prefix = "[ICPU-MBOX]",
};

struct icpu_logger *get_icpu_mbox_log(void)
{
	return &_log;
}

static int __verify_mbox_tx_info(struct icpu_mbox_tx_info *info)
{
	if (!info->int_enable_reg
		|| !info->int_gen_reg
		|| !info->int_status_reg
		|| !info->data_reg
		|| !info->data_max_len
		)
		return -ENOMEM;

	return 0;
}

static int __verify_mbox_rx_info(struct icpu_mbox_rx_info *info)
{
	if (!info->int_status_reg
		|| !info->data_reg
		|| !info->data_max_len
		|| !info->irq
		)
		return -ENOMEM;

	return 0;
}

static int __verify_mbox_hw_info(enum icpu_mbox_mode mode, void *hw_info)
{
	int ret;

	switch (mode) {
	case ICPU_MBOX_MODE_TX:
		ret = __verify_mbox_tx_info(hw_info);
		break;
	case ICPU_MBOX_MODE_RX:
		ret = __verify_mbox_rx_info(hw_info);
		break;
	case ICPU_MBOX_MODE_MAX:
	default:
		ICPU_ERR("inalid mode");
		ret = -EINVAL;
		break;
	}

	return ret;
}

static inline u32 __io_read_and_clear(void __iomem *reg)
{
	u32 val;

	val = ioread32(reg);
	iowrite32(0, reg);

	return val;
}

static irqreturn_t __pablo_icpu_mbox_isr(int irq, void *cookie)
{
	int i;
	struct pablo_icpu_mbox_chan *chan = cookie;
	struct icpu_mbox_controller *mbox;
	struct icpu_mbox_rx_info *info;
	u32 int_val;

	if (!chan) {
		ICPU_ERR("channel is null");
		return IRQ_NONE;
	}

	mbox = chan->mbox;
	if (!mbox) {
		ICPU_ERR("mbox is null");
		return IRQ_NONE;
	}

	info = mbox->hw_info;
	if (!info) {
		ICPU_ERR("info is null");
		return IRQ_NONE;
	}

	for (i = 0; i < info->data_max_len; i++)
		mbox->rx_data[i] = __io_read_and_clear(info->data_reg + (i * 4));

	int_val = readl(info->int_status_reg + 0x10);
	int_val = int_val & 0xfffffeff;
	writel(int_val, info->int_status_reg + 0x10);

	ICPU_DEBUG("gen 0x%x, status: 0x%x\n", readl(info->int_status_reg + 0x10), readl(info->int_status_reg));
	ICPU_DEBUG("rx_data[0]: 0x%x, rx_data[1]: %d\n", mbox->rx_data[0], mbox->rx_data[1]);

	if (chan->cl->rx_callback)
		chan->cl->rx_callback(chan->cl, mbox->rx_data, info->data_max_len);

	return IRQ_HANDLED;
}

static int __wait_mbox_idle(struct icpu_mbox_controller *mbox)
{
	ICPU_TIME_DECLARE();
	u32 timeout = 50;
	struct icpu_mbox_tx_info *info = mbox->hw_info;

	if (mbox->debug_timeout > 0) {
		timeout = mbox->debug_timeout;
		ICPU_INFO("debug timeout is set to %d ms", timeout);
	}

	ICPU_TIME_BEGIN();
	while (readl(info->int_status_reg)) {
		if (--timeout <= 0) {
			ICPU_ERR("timed out");
			ICPU_TIME_END("timed out (timeout:%d)", timeout);
			return -EBUSY;
		}

		udelay(10);
	}
	ICPU_TIME_END("done (timeout:%d)", timeout);

	return 0;
}

static int __pablo_icpu_mbox_send_message(struct pablo_icpu_mbox_chan *chan, u32 *tx_data, u32 len)
{
	int ret;
	u32 i;
	struct icpu_mbox_controller *mbox;
	struct icpu_mbox_tx_info *info;

	mbox = chan->mbox;

	if (!tx_data)
		return -EINVAL;

	info = mbox->hw_info;
	if (info->data_max_len < len)
		return -EINVAL;

	ret = __wait_mbox_idle(mbox);
	if (ret)
		return ret;

	for (i = 0; i < len; i++)
		writel(tx_data[i], info->data_reg + (i * 4));

	writel(0x100, info->int_gen_reg);

	if (chan->cl->tx_done)
		chan->cl->tx_done(chan->cl, tx_data, len);

	return 0;
}

static int pablo_icpu_mbox_send_message_lock(struct pablo_icpu_mbox_chan *chan, u32 *tx_data, u32 len)
{
	int ret;
	struct icpu_mbox_controller *mbox;
	unsigned long _lock_flags;

	if (!chan)
		return -EINVAL;

	mbox = chan->mbox;
	if (!mbox || mbox->state != ICPU_MBOX_STATE_READY)
		return -ENODEV;

	spin_lock_irqsave(&mbox->lock, _lock_flags);
	ret = __pablo_icpu_mbox_send_message(chan, tx_data, len);
	spin_unlock_irqrestore(&mbox->lock, _lock_flags);

	return ret;
}

static int pablo_icpu_mbox_tx_startup(struct pablo_icpu_mbox_chan *chan)
{
	struct icpu_mbox_controller *mbox;

	if (!chan || !chan->mbox)
		return -EINVAL;

	mbox = chan->mbox;

	mbox->state |= ICPU_MBOX_STATE_START;

	ICPU_INFO("done");

	return 0;
}

static int pablo_icpu_mbox_rx_startup(struct pablo_icpu_mbox_chan *chan)
{
	int ret;
	struct icpu_mbox_controller *mbox;
	struct icpu_mbox_rx_info *info;

	if (!chan || !chan->mbox)
		return -EINVAL;

	mbox = chan->mbox;
	info = mbox->hw_info;

	ret = request_irq(info->irq, __pablo_icpu_mbox_isr,
			mbox->irq_flags, "ICPU_MBOX_IRQ", chan);
	if (ret)
		return ret;

	mbox->state |= ICPU_MBOX_STATE_START;

	ICPU_INFO("done");

	return 0;
}

static void pablo_icpu_mbox_tx_shutdown(struct pablo_icpu_mbox_chan *chan)
{
	struct icpu_mbox_controller *mbox;

	if (!chan || !chan->mbox)
		return;

	mbox = chan->mbox;

	mbox->state = ICPU_MBOX_STATE_INIT;

	ICPU_INFO("done");
}

static void pablo_icpu_mbox_rx_shutdown(struct pablo_icpu_mbox_chan *chan)
{
	struct icpu_mbox_controller *mbox;
	struct icpu_mbox_rx_info *info;

	if (!chan || !chan->mbox)
		return;

	mbox = chan->mbox;
	info = mbox->hw_info;

	free_irq(info->irq, chan);

	mbox->state = ICPU_MBOX_STATE_INIT;

	ICPU_INFO("done");
}

static struct icpu_mbox_chan_ops tx_ops = {
	.send_data = pablo_icpu_mbox_send_message_lock,
	.startup = pablo_icpu_mbox_tx_startup,
	.shutdown = pablo_icpu_mbox_tx_shutdown,
};

static struct icpu_mbox_chan_ops rx_ops = {
	.send_data = NULL,
	.startup = pablo_icpu_mbox_rx_startup,
	.shutdown = pablo_icpu_mbox_rx_shutdown,
};

struct icpu_mbox_controller *pablo_icpu_mbox_request(enum icpu_mbox_mode mode, void *hw_info)
{
	struct icpu_mbox_controller *mbox;

	if (!hw_info || __verify_mbox_hw_info(mode, hw_info)) {
		ICPU_ERR("Invalid argument");
		return NULL;
	}

	mbox = kzalloc(sizeof(struct icpu_mbox_controller), GFP_KERNEL);
	if (!mbox) {
		ICPU_ERR("mbox alloc fail");
		return NULL;
	}

	switch (mode) {
	case ICPU_MBOX_MODE_TX:
		mbox->ops = &tx_ops;
		mbox->hw_info = hw_info;

		if (IS_ENABLED(CONFIG_ARCH_VELOCE_HYCON))
			mbox->debug_timeout = 5000;
		break;
	case ICPU_MBOX_MODE_RX:
		mbox->ops = &rx_ops;
		mbox->hw_info = hw_info;
		mbox->rx_data = kzalloc(sizeof(u32) * ((struct icpu_mbox_rx_info*)mbox->hw_info)->data_max_len, GFP_KERNEL);
		if (!mbox->rx_data) {
			ICPU_ERR("rx_data alloc fail");
			goto rx_data_alloc_fail;
		}
		break;
	case ICPU_MBOX_MODE_MAX:
	default:
		ICPU_ERR("inalid mode");
		goto  invalid_mode;
		break;
	}

	spin_lock_init(&mbox->lock);

	mbox->state = ICPU_MBOX_STATE_INIT;

	return mbox;

rx_data_alloc_fail:
invalid_mode:
	kfree(mbox);

	return NULL;
}
KUNIT_EXPORT_SYMBOL(pablo_icpu_mbox_request);

void pablo_icpu_mbox_free(struct icpu_mbox_controller *mbox)
{
	if (mbox) {
		if (mbox->rx_data)
			kfree(mbox->rx_data);

		kfree(mbox);
	}
}
KUNIT_EXPORT_SYMBOL(pablo_icpu_mbox_free);

#if IS_ENABLED(CONFIG_PABLO_KUNIT_TEST)
int pablo_icpu_mbox_isr_wrap(struct pablo_icpu_mbox_chan *chan)
{
	struct icpu_mbox_rx_info *info;

	if (!chan || !chan->mbox)
		return -EINVAL;

	info = chan->mbox->hw_info;

	return __pablo_icpu_mbox_isr(info->irq, chan);

}
EXPORT_SYMBOL_GPL(pablo_icpu_mbox_isr_wrap);
#endif
