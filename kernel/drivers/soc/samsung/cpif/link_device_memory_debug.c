// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2011 Samsung Electronics.
 *
 */

#include <linux/irq.h>
#include <linux/gpio.h>
#include <linux/interrupt.h>
#include <linux/platform_device.h>
#include <linux/time.h>
#include <linux/timer.h>
#include <linux/delay.h>
#include <linux/wait.h>
#include <linux/sched.h>
#include <linux/vmalloc.h>
#include <linux/netdevice.h>

#include "modem_prj.h"
#include "modem_utils.h"
#include "link_device_memory.h"

void print_mem_snapshot(struct mem_link_device *mld, struct mem_snapshot *mst)
{
	struct link_device *ld = &mld->link_dev;

	mif_info("%s: [%s] ACC{%X %d} FMT{TI:%u TO:%u RI:%u RO:%u} RAW{TI:%u TO:%u RI:%u RO:%u} INTR{RX:0x%X TX:0x%X}\n",
		ld->name, ipc_dir(mst->dir), mst->magic, mst->access,
		mst->head[IPC_MAP_FMT][TX], mst->tail[IPC_MAP_FMT][TX],
		mst->head[IPC_MAP_FMT][RX], mst->tail[IPC_MAP_FMT][RX],
		mst->head[IPC_MAP_NORM_RAW][TX], mst->tail[IPC_MAP_NORM_RAW][TX],
		mst->head[IPC_MAP_NORM_RAW][RX], mst->tail[IPC_MAP_NORM_RAW][RX],
		mst->int2ap, mst->int2cp);
}

void print_dev_snapshot(struct mem_link_device *mld, struct mem_snapshot *mst,
			struct legacy_ipc_device *dev)
{
	struct link_device *ld = &mld->link_dev;
	enum legacy_ipc_map id = dev->id;

	if (id >= IPC_MAP_MAX)
		return;

	mif_info("%s: [%s] %s | TXQ{in:%u out:%u} RXQ{in:%u out:%u} | INTR{0x%02X}\n",
		ld->name, ipc_dir(mst->dir), dev->name,
		mst->head[id][TX], mst->tail[id][TX],
		mst->head[id][RX], mst->tail[id][RX],
		(mst->dir == RX) ? mst->int2ap : mst->int2cp);
}
