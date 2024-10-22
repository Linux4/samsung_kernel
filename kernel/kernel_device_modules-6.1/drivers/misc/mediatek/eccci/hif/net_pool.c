// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2016 MediaTek Inc.
 */

#include <linux/list.h>
#include <linux/device.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/err.h>
#include <linux/kdev_t.h>
#include <linux/slab.h>
#include <linux/wait.h>
#include <linux/sched/clock.h> /* local_clock() */
#include <linux/kthread.h>
#include <linux/delay.h>
#include <linux/interrupt.h>
#include <linux/timer.h>
#include <linux/fs.h>
#include <linux/netdevice.h>
#include <linux/ip.h>
#include <linux/random.h>
#include <linux/platform_device.h>
#include <linux/of.h>
#include <linux/of_fdt.h>
#include <linux/of_irq.h>
#include <linux/of_address.h>
#include <linux/syscore_ops.h>

#include <mt-plat/mtk_ccci_common.h>

#include "ccci_config.h"
#include "ccci_debug.h"
#include "net_pool.h"
#include "ccci_dpmaif_debug.h"

#define TAG "pool"

#define DL_POOL_LEN	51200


struct fifo_t {
	atomic_t w;
	atomic_t r;
	void *buf[DL_POOL_LEN];

	u32 dp_cnt;
};

static u32 s_q_num;
static struct fifo_t *s_dl_pools;



static inline u32 fifo_avail(struct fifo_t *fifo)
{
	if (atomic_read(&fifo->r) > atomic_read(&fifo->w))
		return (atomic_read(&fifo->r) - atomic_read(&fifo->w) - 1);
	else
		return (DL_POOL_LEN - atomic_read(&fifo->w) + atomic_read(&fifo->r) - 1);
}

static inline void fifo_write(struct fifo_t *fifo, void *skb)
{
	int w_idx = 0;

	w_idx = atomic_read(&fifo->w);
	if (w_idx < 0 || w_idx >= DL_POOL_LEN) {
		CCCI_ERROR_LOG(-1, TAG,
			"[%s] error\n", __func__);
		return;
	}

	/* make sure read correct w_idx */
	rmb();
	fifo->buf[w_idx] = skb;

	/* wait: fifo->buf[fifo->w] = skb done */
	wmb();

	if (w_idx < (DL_POOL_LEN - 1))
		atomic_inc(&fifo->w);
	else
		atomic_set(&fifo->w, 0);
}

static inline void *fifo_read(struct fifo_t *fifo)
{
	void *data = NULL;
	int r_idx = 0;

	r_idx = atomic_read(&fifo->r);
	if (r_idx < 0 || r_idx >= DL_POOL_LEN) {
		CCCI_ERROR_LOG(-1, TAG,
			"[%s] error\n", __func__);
		return NULL;
	}
	/* make sure read correct r_idx */
	rmb();
	data = fifo->buf[r_idx];

	/* wait: data = fifo->buf[r] done*/
	mb();

	if (r_idx < (DL_POOL_LEN - 1))
		atomic_inc(&fifo->r);
	else
		atomic_set(&fifo->r, 0);

	return data;
}

static inline u32 fifo_len(struct fifo_t *fifo)
{
	if (atomic_read(&fifo->w) >= atomic_read(&fifo->r))
		return (atomic_read(&fifo->w) - atomic_read(&fifo->r));

	return (DL_POOL_LEN - atomic_read(&fifo->r) + atomic_read(&fifo->w));
}

int ccci_dl_pool_init(u32 q_num)
{
	int len;

	if (s_dl_pools)
		return 0;

	if (q_num == 0) {
		CCCI_ERROR_LOG(-1, TAG,
			"[%s] error: q_num = 0\n", __func__);
		return -1;
	}

	s_q_num = q_num;

	len = sizeof(struct fifo_t) * q_num;
	s_dl_pools = kzalloc(len, GFP_KERNEL);
	if (!s_dl_pools) {
		CCCI_ERROR_LOG(-1, TAG,
			"[%s] error: kzalloc fifo_t fail. q_num: %u\n",
			__func__, q_num);

		s_q_num = 0;
		return -1;
	}

	return 0;
}

inline void ccci_dl_enqueue(u32 qno, struct sk_buff *skb)
{
	struct fifo_t *fifo = NULL;

	if (!s_dl_pools)
		goto _free_sk;

	if (qno >= s_q_num)
		goto _free_sk;

	fifo = &s_dl_pools[qno];
	if (fifo_avail(fifo)) {
		fifo_write(fifo, skb);
		return;
	}

_free_sk:
	if (g_debug_flags & DEBUG_DROP_SKB) {
		struct debug_drop_skb_hdr hdr;

		hdr.type = TYPE_DROP_SKB_ID;
		hdr.qidx = qno;
		hdr.time = (unsigned int)(local_clock() >> 16);
		hdr.from = DROP_SKB_FROM_RX_ENQUEUE;
		hdr.bid  = ((struct lhif_header *)skb->data)->pdcp_count;
		hdr.ipid = ((struct iphdr *)(skb->data + sizeof(struct lhif_header)))->id;
		hdr.len  = skb->len;
		ccci_dpmaif_debug_add(&hdr, sizeof(hdr));
	}

	dev_kfree_skb_any(skb);

	/* ensure free skb is completed before proceeding to the next flow */
	wmb();

	if (!fifo)
		return;

	if ((fifo->dp_cnt & 0xFF) == 0)
		CCCI_ERROR_LOG(0, TAG, "[%s] qno: %u; dp_cnt: %u\n",
			__func__, qno, fifo->dp_cnt);

	fifo->dp_cnt++;
}

inline void *ccci_dl_dequeue(u32 qno)
{
	if ((!s_dl_pools) || (qno >= s_q_num))
		return NULL;

	if (fifo_len(&s_dl_pools[qno]))
		return fifo_read(&s_dl_pools[qno]);

	return NULL;
}

inline u32 ccci_dl_queue_len(u32 qno)
{
	if ((!s_dl_pools) || (qno >= s_q_num))
		return 0;

	return fifo_len(&s_dl_pools[qno]);
}

u32 ccci_get_dl_queue_dp_cnt(u32 qno)
{
	struct fifo_t *fifo = NULL;

	if ((!s_dl_pools) || (qno >= s_q_num))
		return 0;

	fifo = &s_dl_pools[qno];

	return fifo->dp_cnt;
}
