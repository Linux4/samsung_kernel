/*
    Marvell PXA9XX ACIPC-MSOCKET driver for Linux
    Copyright (C) 2010 Marvell International Ltd.

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License version 2 as
    published by the Free Software Foundation.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#include <linux/sched.h>
#include <linux/device.h>
#include <linux/pm_wakeup.h>
#include <linux/version.h>
#include <linux/export.h>
#include <linux/module.h>
#include <linux/debugfs.h>
#include <linux/ctype.h>
#include <linux/skbuff.h>
#include <linux/kthread.h>
#include "skb_llist.h"

struct queue_ops {
	void (*init)(void *);
	u32 (*length)(void *);
	void (*enqueue)(void *, struct sk_buff *skb);
	void (*queue_head)(void *, struct sk_buff *skb);
	struct sk_buff * (*peek)(void *);
	struct sk_buff * (*dequeue)(void *);
};

struct queue {
	void *data;
	struct queue_ops *ops;
};

struct queue_ops skb_llist_ops = {
	.init = skb_llist_init,
	.length = skb_llist_len,
	.enqueue = skb_llist_enqueue,
	.queue_head = skb_llist_queue_head,
	.peek = skb_llist_peek,
	.dequeue = skb_llist_dequeue,
};

struct queue_ops skb_queue_ops = {
	.init = skb_queue_head_init,
	.length = skb_queue_len,
	.enqueue = skb_queue_tail,
	.queue_head = skb_queue_head,
	.peek = skb_peek,
	.dequeue = skb_dequeue,
};

struct skb_llist llist;
struct sk_buff_head head;

struct queue skb_llist = {
	.data = &llist,
	.ops = &skb_llist_ops,
};

struct queue skb_queue = {
	.data = &head,
	.ops = &skb_queue_ops,
};

#define SKB_IDX(skb) (*(unsigned long *)(skb)->cb)
#define SKB_OWNER(skb) (*(unsigned char *)&(skb)->cb[4])

void sequential_test(struct queue *queue, struct sk_buff **skb, int len,
	int iter)
{
	struct timeval start_tv;
	struct timeval end_tv;
	unsigned long long time;
	int i, j;
	unsigned long enqueue_idx = 0;
	unsigned long dequeue_idx = 0;

	queue->ops->init(queue->data);

	do_gettimeofday(&start_tv);

	for (i = 0; i < iter; i++) {
		for (j = 0; j < len; j++) {
			SKB_IDX(skb[j]) = ++enqueue_idx;
			queue->ops->enqueue(queue->data, skb[j]);
		}
		for (j = 0; j < len; j++) {
			struct sk_buff *tmp = queue->ops->dequeue(queue->data);
			unsigned long idx = SKB_IDX(tmp);
			if ((long)idx - (long)dequeue_idx != 1) {
				pr_info("%s: idx does not match, idx: %lu, dequeue_idx: %lu\n",
					__func__, idx, dequeue_idx);
				WARN_ON(1);
				return;
			}
			dequeue_idx = idx;
		}
	}
	do_gettimeofday(&end_tv);

	time = end_tv.tv_sec - start_tv.tv_sec;
	time *= USEC_PER_SEC;
	time += (long long)((long)end_tv.tv_usec - (long)start_tv.tv_usec);
	do_div(time, USEC_PER_MSEC);

	pr_info("%s: len %d, iter %d, total_time %llu(ms)\n",
		__func__, len, iter, time);
}

struct rbuffer {
	struct sk_buff **skb;
	size_t len; /* must be the power of 2 */

	unsigned char owner;

	unsigned long ridx;
	unsigned long widx;
};

static inline struct sk_buff *rb_get(struct rbuffer *rb)
{
	struct sk_buff *skb = NULL;
	if (ACCESS_ONCE(rb->ridx) < ACCESS_ONCE(rb->widx))
		skb = rb->skb[rb->ridx++ & (rb->len - 1)];
	return skb;
}

static inline void rb_put(struct rbuffer *rb)
{
	rb->widx++;
}

/*
 * totally 6 buffer, 2 for two producer
 * and the other 4 for each cpu
 */
struct rbuffer rbuf[6];

void deinit_rb(unsigned char owner)
{
	struct rbuffer *rb = &rbuf[owner];
	int i;
	for (i = 0; i < rb->len; i++)
		kfree_skb(rb->skb[i]);
	kfree(rb->skb);
}

int init_rb(unsigned char owner, size_t len)
{
	struct rbuffer *rb = &rbuf[owner];
	int i;

	rb->owner = owner;
	rb->len = len;

	rb->skb = kzalloc(len * sizeof(struct sk_buff *),
		GFP_KERNEL);

	if (!rb->skb) {
		pr_err("alloc skb failed\n");
		goto exit;
	}

	for (i = 0; i < len; i++) {
		rb->skb[i] = alloc_skb(1, GFP_KERNEL);
		if (!rb->skb[i]) {
			pr_err("alloc skb failed\n");
			goto exit;
		}
		SKB_OWNER(rb->skb[i]) = owner;
	}

	rb->widx = len;
	rb->ridx = 0;

	return 0;

exit:
	deinit_rb(owner);
	return -1;
}

struct thread_param {
	struct queue *queue;
	struct rbuffer *rb;
	int iter;
	int len;
};

atomic_t producer_cnt;

DECLARE_COMPLETION(consumer_finished);

static int producer_thread(void *arg)
{
	struct thread_param *param = arg;
	struct sk_buff *skb;
	unsigned long idx = param->len;
	int i = 0;
	struct timeval start_tv;
	struct timeval end_tv;
	unsigned long long time;

	do_gettimeofday(&start_tv);

	for (i = 0; i < param->iter; i++) {
		/* get next skb from rb */
		while ((skb = rb_get(param->rb)) == NULL)
			;
		SKB_IDX(skb) = ++idx;
		param->queue->ops->enqueue(param->queue->data, skb);
	}

	do_gettimeofday(&end_tv);

	time = end_tv.tv_sec - start_tv.tv_sec;
	time *= USEC_PER_SEC;
	time += (long long)((long)end_tv.tv_usec - (long)start_tv.tv_usec);
	do_div(time, USEC_PER_MSEC);

	pr_info("%s: owner: %d, len %d, iter %d, total_time %llu(ms)\n",
		__func__, param->rb->owner, param->len, param->iter, time);

	atomic_dec(&producer_cnt);

	return 0;
}

static int consumer_thread(void *arg)
{
	struct thread_param *param = arg;
	struct sk_buff *skb;
	unsigned long dequeue_idx[6] = {0, 0, 0, 0, 0, 0};
	int total = 0;
	unsigned long idx;
	unsigned char owner;
	struct timeval start_tv;
	struct timeval end_tv;
	unsigned long long time;

	do_gettimeofday(&start_tv);

	while ((skb = param->queue->ops->dequeue
			(param->queue->data)) ||
		atomic_read(&producer_cnt)) {
		if (skb) {
			idx = SKB_IDX(skb);
			owner = SKB_OWNER(skb);

			rb_put(&rbuf[owner]);

			if ((long)idx - (long)dequeue_idx[owner] != 1) {
				pr_info("%s: idx does not match, idx: %lu, dequeue_idx: %lu\n",
					__func__, idx, dequeue_idx[owner]);
				WARN_ON(1);
				complete_all(&consumer_finished);
				return -1;
			}
			dequeue_idx[owner] = idx;
			total++;
		}
	}

	do_gettimeofday(&end_tv);

	time = end_tv.tv_sec - start_tv.tv_sec;
	time *= USEC_PER_SEC;
	time += (long long)((long)end_tv.tv_usec - (long)start_tv.tv_usec);
	do_div(time, USEC_PER_MSEC);

	pr_info("%s: total %d, total_time %llu(ms)\n",
		__func__, total, time);

	complete_all(&consumer_finished);

	return 0;
}

struct thread_param thread_param[3];

unsigned long hammer_idx[4];

void rb_ipi(void *arg)
{
	struct queue *queue = arg;
	int cpu = smp_processor_id();
	struct rbuffer *rb = &rbuf[cpu + 2];
	struct sk_buff *skb;

	/* get next skb from rb */
	skb = rb_get(rb);
	if (skb) {
		SKB_IDX(skb) = ++hammer_idx[cpu];
		queue->ops->enqueue(queue->data, skb);
	}
}

int rb_hammer_test(void *arg)
{
	memset(hammer_idx, 0, sizeof(hammer_idx));
	while (atomic_read(&producer_cnt) > 1) {
		smp_call_function(rb_ipi, arg, 1);
		schedule();
	}

	atomic_dec(&producer_cnt);

	return 0;
}

void concurrent_test(struct queue *queue, int len, int iter)
{
	int i, j;
	struct task_struct *task[3];
	struct task_struct *rb_hammer;

	queue->ops->init(queue->data);

	atomic_set(&producer_cnt, 3);

	for (i = 0; i < 2; i++) {
		unsigned long idx = 0;
		thread_param[i].queue = queue;
		thread_param[i].rb = &rbuf[i];
		thread_param[i].iter = iter;
		thread_param[i].len = len;
		task[i] = kthread_create(producer_thread, &thread_param[i],
			"producer/%d", i);
		kthread_bind(task[i], 1 + i);

		for (j = 0; j < len; ++j) {
			struct sk_buff *skb = rb_get(&rbuf[i]);
			SKB_IDX(skb) = ++idx;
			queue->ops->enqueue(queue->data, skb);
		}
	}

	thread_param[2].queue = queue;
	thread_param[2].iter = iter;
	thread_param[2].len = len;
	task[2] = kthread_create(consumer_thread, &thread_param[2],
		"consumer");
	kthread_bind(task[2], 3);

	for (i = 0; i < 3; i++)
		wake_up_process(task[i]);

	rb_hammer = kthread_create(rb_hammer_test, queue, "hammer");
	kthread_bind(rb_hammer, 0);
	wake_up_process(rb_hammer);
}

static inline unsigned next_power_of_2(unsigned v)
{
	v--;
	v |= v >> 1;
	v |= v >> 2;
	v |= v >> 4;
	v |= v >> 8;
	v |= v >> 16;
	v++;

	return v;
}

static uint32_t len = 128;
module_param_named(len, len, uint, S_IWUSR | S_IRUGO);

static uint32_t iter = 100000;
module_param_named(iter, iter, uint, S_IWUSR | S_IRUGO);

static int test;
static int set_test(const char *val, const struct kernel_param *kp)
{
	int rv = param_set_int(val, kp);

	if (rv)
		return rv;

	switch (test) {
	case 0: /* sequential test */
	{
		int i;

		struct sk_buff **skb = kzalloc(len * sizeof(struct sk_buff *),
			GFP_KERNEL);

		if (!skb) {
			pr_err("alloc skb failed\n");
			goto exit;
		}

		for (i = 0; i < len; i++) {
			skb[i] = alloc_skb(1, GFP_KERNEL);
			if (!skb[i]) {
				pr_err("alloc skb failed\n");
				goto exit;
			}
		}

		pr_info("skb_llist test begin\n");
		sequential_test(&skb_llist, skb, len, iter);
		pr_info("skb_llist test end\n");

		pr_info("skb_queue test begin\n");
		sequential_test(&skb_queue, skb, len, iter);
		pr_info("skb_queue test end\n");

exit:
		for (i = 0; i < len; i++)
			kfree_skb(skb[i]);
		kfree(skb);
		break;
	}
	case 1: /* concurrent_test */
	{
		int i;
		int rc;

		for (i = 0; i < sizeof(rbuf) / sizeof(rbuf[0]); i++) {
			rc = init_rb(i, next_power_of_2(len) * 2);
			if (rc < 0) {
				pr_err("alloc ring buffer failed\n");
				goto exit1;
			}
		}

		pr_info("skb_llist test begin\n");
		reinit_completion(&consumer_finished);
		concurrent_test(&skb_llist, len, iter);
		wait_for_completion(&consumer_finished);
		pr_info("skb_llist test end\n");

		pr_info("skb_queue test begin\n");
		reinit_completion(&consumer_finished);
		concurrent_test(&skb_queue, len, iter);
		wait_for_completion(&consumer_finished);
		pr_info("skb_queue test end\n");

exit1:
		for (i = 0; i < sizeof(rbuf) / sizeof(rbuf[0]); i++)
			deinit_rb(i);

		break;
	}
	}

	return 0;
}

static struct kernel_param_ops test_param_ops = {
	.set = set_test,
	.get = param_get_int,
};

module_param_cb(test, &test_param_ops,
	&test, S_IWUSR | S_IRUGO);

