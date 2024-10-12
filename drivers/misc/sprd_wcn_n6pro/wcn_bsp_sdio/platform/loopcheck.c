#include <linux/completion.h>
#include <linux/delay.h>
#include <linux/kernel.h>
#include <linux/sched/clock.h>
#include <linux/slab.h>
#include <linux/timer.h>
#include <linux/workqueue.h>

#include "wcn_glb.h"
#include "wcn_misc.h"
#include "wcn_procfs.h"

#define LOOPCHECK_TIMER_INTERVAL      5
#define WCN_LOOPCHECK_INIT	1
#define WCN_LOOPCHECK_OPEN	2
#define WCN_LOOPCHECK_FAIL	3

struct wcn_loopcheck {
	unsigned long status;
	struct completion completion;
	struct delayed_work work;
	struct workqueue_struct *workqueue;
};

static struct wcn_loopcheck loopcheck;

#ifndef CONFIG_WCN_PCIE_REMOVE
static int loopcheck_send(char *buf, unsigned int len)
{
	unsigned char *send_buf = NULL;
	struct mbuf_t *head = NULL;
	struct mbuf_t *tail = NULL;
	int num = 1;

	WCN_INFO("tx:%s\n", buf);
	if (unlikely(!marlin_get_module_status())) {
		WCN_ERR("WCN module have not open\n");
		return -EIO;
	}

	send_buf = kzalloc(len + PUB_HEAD_RSV + 1, GFP_KERNEL);
	if (!send_buf)
		return -ENOMEM;
	memcpy(send_buf + PUB_HEAD_RSV, buf, len);

	if (!sprdwcn_bus_list_alloc(mdbg_proc_ops[MDBG_AT_TX_OPS].channel,
				    &head, &tail, &num)) {
		head->buf = send_buf;
		head->len = len;
		head->next = NULL;
		sprdwcn_bus_push_list(mdbg_proc_ops[MDBG_AT_TX_OPS].channel,
				      head, tail, num);
	} else {
		kfree(send_buf);
	}

	return len;
}
#else
static int loopcheck_send(char *cmd, unsigned int len)
{
	struct mbuf_t *head = NULL;
	struct mbuf_t *tail = NULL;
	int num = 1;
	int ret;
	/* dma_buf for dma */
	static struct dma_buf dma_buf;
	static int at_buf_flag;
	struct wcn_pcie_info *pcie_dev;

	pcie_dev = get_wcn_device_info();
	if (!pcie_dev) {
		WCN_ERR("%s:PCIE device link error\n", __func__);
		return -1;
	}

	ret = sprdwcn_bus_list_alloc(0, &head, &tail, &num);
	if (ret || head == NULL || tail == NULL) {
		WCN_ERR("%s:%d mbuf_link_alloc fail\n", __func__, __LINE__);
		return -1;
	}

	if (at_buf_flag == 0) {
		ret = dmalloc(pcie_dev, &dma_buf, 128);
		if (ret != 0) {
			return -1;
		}
		at_buf_flag = 1;
	}
	head->buf = (unsigned char *)(dma_buf.vir);
	head->phy = (unsigned long)(dma_buf.phy);
	head->len = len;
	memset(head->buf, 0x0, dma_buf.size);
	memcpy(head->buf, cmd, len);
	head->next = NULL;
	ret = sprdwcn_bus_push_list(0, head, tail, num);
	if (ret)
		WCN_INFO("sprdwcn_bus_push_list error=%d\n", ret);

	WCN_INFO("tx:%s in %s\n", cmd, __func__);

	return len;
}
#endif

static void loopcheck_work_queue(struct work_struct *work)
{
	int ret;
	unsigned long timeleft, ns, time;
	unsigned int ap_t, marlin_boot_t;
	char a[64];

	ns = local_clock();
	time = marlin_bootup_time_get();
	ap_t = MARLIN_64B_NS_TO_32B_MS(ns);
	marlin_boot_t = MARLIN_64B_NS_TO_32B_MS(time);
	snprintf(a, (size_t)sizeof(a), "at+loopcheck=%u,%u\r\n",
		 ap_t, marlin_boot_t);

	if (!test_bit(WCN_LOOPCHECK_OPEN, &loopcheck.status))
		return;

	loopcheck_send(a, strlen(a));
	timeleft = wait_for_completion_timeout(&loopcheck.completion, (3 * HZ));
	if (!test_bit(WCN_LOOPCHECK_OPEN, &loopcheck.status))
		return;
	if (!timeleft) {
		set_bit(WCN_LOOPCHECK_FAIL, &loopcheck.status);
		WCN_ERR("didn't get loopcheck ack\n");
		mdbg_assert_interface("WCN loopcheck erro!");
		clear_bit(WCN_LOOPCHECK_FAIL, &loopcheck.status);

		return;
	}

	ret = queue_delayed_work(loopcheck.workqueue, &loopcheck.work,
				 LOOPCHECK_TIMER_INTERVAL * HZ);
}

void start_loopcheck(void)
{
	if (!test_bit(WCN_LOOPCHECK_INIT, &loopcheck.status) ||
	    test_and_set_bit(WCN_LOOPCHECK_OPEN, &loopcheck.status))
		return;
	WCN_INFO("%s\n", __func__);
	reinit_completion(&loopcheck.completion);
	queue_delayed_work(loopcheck.workqueue, &loopcheck.work, HZ);
}

void stop_loopcheck(void)
{
	if (!test_bit(WCN_LOOPCHECK_INIT, &loopcheck.status) ||
	    !test_and_clear_bit(WCN_LOOPCHECK_OPEN, &loopcheck.status) ||
	    test_bit(WCN_LOOPCHECK_FAIL, &loopcheck.status))
		return;
	WCN_INFO("%s\n", __func__);
	complete_all(&loopcheck.completion);
	cancel_delayed_work_sync(&loopcheck.work);
}

void complete_kernel_loopcheck(void)
{
	complete(&loopcheck.completion);
}

int loopcheck_init(void)
{
	loopcheck.status = 0;
	init_completion(&loopcheck.completion);
	loopcheck.workqueue =
		create_singlethread_workqueue("WCN_LOOPCHECK_QUEUE");
	if (!loopcheck.workqueue) {
		WCN_ERR("WCN_LOOPCHECK_QUEUE create failed");
		return -ENOMEM;
	}
	set_bit(WCN_LOOPCHECK_INIT, &loopcheck.status);
	INIT_DELAYED_WORK(&loopcheck.work, loopcheck_work_queue);

	return 0;
}

int loopcheck_deinit(void)
{
	stop_loopcheck();
	destroy_workqueue(loopcheck.workqueue);
	loopcheck.status = 0;

	return 0;
}
