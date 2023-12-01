/* SPDX-License-Identifier: GPL */
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

#ifndef PABLO_ICPU_MAILBOX_H
#define PABLO_ICPU_MAILBOX_H

enum icpu_mbox_chan_type {
	ICPU_MBOX_CHAN_TX = 0,
	ICPU_MBOX_CHAN_RX = 1,
	ICPU_MBOX_CHAN_MAX,
};

struct pablo_icpu_mbox_chan {
	struct icpu_mbox_client *cl;
	struct icpu_mbox_controller *mbox;
	spinlock_t lock; /* Serialise access to the channel */
};

struct icpu_mbox_client {
	/* TODO: code clean
	struct device *dev;
	bool tx_block;
	unsigned long tx_tout;
	bool knows_txdone; */

	void (*rx_callback)(struct icpu_mbox_client *cl, u32 *rx_data, u32 len);
	void (*tx_prepare)(struct icpu_mbox_client *cl, void *msg);
	void (*tx_done)(struct icpu_mbox_client *cl, void *msg, int len);
};

enum icpu_mbox_state
{
	ICPU_MBOX_STATE_NO_INIT 	= 0x0,
	ICPU_MBOX_STATE_INIT		= 0x1 << 1,
	ICPU_MBOX_STATE_START		= 0x1 << 2,
	ICPU_MBOX_STATE_READY		= ICPU_MBOX_STATE_INIT | ICPU_MBOX_STATE_START,
};

struct icpu_mbox_chan_ops {
	int (*send_data)(struct pablo_icpu_mbox_chan *chan, u32 *data, u32 len);
	int (*startup)(struct pablo_icpu_mbox_chan *chan);
	void (*shutdown)(struct pablo_icpu_mbox_chan *chan);
};

enum icpu_mbox_mode {
	ICPU_MBOX_MODE_TX = 0,
	ICPU_MBOX_MODE_RX = 1,
	ICPU_MBOX_MODE_MAX,
};

struct icpu_mbox_controller {
	void *hw_info;
	const struct icpu_mbox_chan_ops *ops;
	unsigned int irq_flags;
	u32 debug_timeout;
	spinlock_t lock;

	u32 *rx_data;

	enum icpu_mbox_state state;
};

struct icpu_mbox_controller *pablo_icpu_mbox_request(enum icpu_mbox_mode mode, void *hw_info);
void pablo_icpu_mbox_free(struct icpu_mbox_controller *mbox);

#if IS_ENABLED(CONFIG_PABLO_KUNIT_TEST)
int pablo_icpu_mbox_isr_wrap(struct pablo_icpu_mbox_chan *chan);
#endif

#endif
