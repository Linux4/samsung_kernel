/*
    portqueue.c Created on: Jul 29, 2010, Jinhua Huang <jhhuang@marvell.com>

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

#include <linux/spinlock.h>
#include <linux/list.h>
#include <linux/slab.h>
#include <linux/skbuff.h>
#include <linux/wait.h>
#include <linux/sched.h>
#include <linux/workqueue.h>
#include <linux/wakelock.h>

#include "shm_share.h"
#include "acipcd.h"
#include "shm.h"
#include "portqueue.h"
#include "msocket.h"
#include "diag.h"
#include <linux/wakelock.h>

/*Note: nvm_shared.h contain same definition, if want to modify, please modify both*/
#define NVMReqProcId 0x0003
#define NVMCnfProcId 0x0004

extern struct wake_lock acipc_wakeup; //used to ensure Workqueue scheduled.
static struct wake_lock port_rx_wakeup;
static struct wake_lock port_rx_NVM_wakeup;
static struct wake_lock port_tx_wakelock;
int NVM_wake_lock_num;
int NVM_wake_unlock_num;
int rx_workq_sch_num;
static int dump_flag = 0;


/* forward static function prototype */
static void portq_tx_worker(struct work_struct *work);
static void portq_rx_worker(struct work_struct *work);

/* portq work queue and work */
static struct workqueue_struct *portq_wq;
static DECLARE_WORK(tx_work, portq_tx_worker);
static DECLARE_DELAYED_WORK(rx_work, portq_rx_worker);

struct shm_rbctl *portq_rbctl;

/* port queue list and lock */
struct portq *portq_array[PORTQ_NUM_MAX];
static struct list_head tx_portq_head;	/* tx port queue list header */
spinlock_t portq_list_lock;	/* port queue lock */
/* port queue priority definition */
static const int portq_priority[PORTQ_NUM_MAX] = {
	0,			/* 0: NOT USED */
	10,			/* 1: CISTUB_PORT */
	10,			/* 2: NVMSRV_PORT */
	50,			/* 3: CIDATASTUB_PORT */
	60,			/* 4: DIAG_PORT */
	30,			/* 5: AUDIOSTUB_PORT */
	40,			/* 6: CICSDSTUB_PORT */
	0,			/* 7: NOT USED */
	70,			/* 8: TEST_PORT */
};

/* rx work queue state and lock */
bool portq_is_rx_work_running = false;
spinlock_t portq_rx_work_lock;

/* tx work queue state and lock */
static bool portq_is_tx_work_running = false;
static spinlock_t portq_tx_work_lock;

/*
 * reflected variable of CP port flow control state.
 * can be accessed by flow control call-back, so it's a extern variable
 * since it is only CP write, AP read, no lock is needed
 */
unsigned int portq_cp_port_fc = 0;

/*
 * reflected variable of CP port flow control state.
 * need lock carefully.
 */
static unsigned int portq_ap_port_fc = 0;
static spinlock_t portq_ap_port_fc_lock;

/* check if tx queue is full */
static inline bool portq_is_tx_full(struct portq *portq)
{
	return skb_queue_len(&portq->tx_q) >= portq->tx_q_limit;
}

/* check if rx queue is empty */
static inline bool portq_is_rx_empty(struct portq *portq)
{
	return skb_queue_empty(&portq->rx_q);
}

/* check if rx queue is below low water-mark */
static inline bool portq_is_rx_below_lo_wm(struct portq *portq)
{
	return skb_queue_len(&portq->rx_q) < portq->rx_q_low_wm;
}

/* check if rx queue is above high water-mark */
static inline bool portq_is_rx_above_hi_wm(struct portq *portq)
{
	return skb_queue_len(&portq->rx_q) >= portq->rx_q_high_wm;
}

/* check if cp xmit has been throttled by ap */
static inline bool portq_is_cp_xmit_throttled(struct portq *portq)
{
	return (1 << portq->port) & portq_ap_port_fc;
}

/* check if ap xmit has been throttled by cp */
static inline bool portq_is_ap_xmit_throttled(struct portq *portq)
{
	return (1 << portq->port) & portq_cp_port_fc;
}

void portq_packet_send_cb(struct shm_rbctl *rbctl)
{
	wake_lock_timeout(&acipc_wakeup, HZ * 5);
	spin_lock(&portq_rx_work_lock);
	if (!portq_is_rx_work_running) {
		portq_is_rx_work_running = true;
		portq_schedule_rx();
	}
	spin_unlock(&portq_rx_work_lock);
}

void portq_peer_sync_cb(struct shm_rbctl *rbctl)
{
	complete_all(&msocket_peer_sync);
}

void portq_port_fc_cb(struct shm_rbctl *rbctl)
{
	unsigned int temp_cp_port_fc =
		portq_rbctl->skctl_va->cp_port_fc;
	unsigned int changed = portq_cp_port_fc ^ temp_cp_port_fc;
	int port = 0;
	struct portq *portq;

	wake_lock_timeout(&acipc_wakeup, HZ * 2);
	spin_lock(&portq_list_lock);

	while ((changed >>= 1)) {
		port++;
		if (changed & 1) {
			portq = portq_array[port];
			if (!portq)
				continue;

			spin_lock(&portq->lock);
			if (temp_cp_port_fc & (1 << port)) {
				portq->stat_fc_cp_throttle_ap++;
				printk(KERN_WARNING
					"MSOCK: port %d is throttled by CP!!!\n",
					port);
			} else {
				portq->stat_fc_cp_unthrottle_ap++;
				printk(KERN_WARNING
					"MSOCK: port %d is unthrottled by CP!!!\n",
					port);
			}
			spin_unlock(&portq->lock);
		}
	}

	spin_unlock(&portq_list_lock);

	portq_cp_port_fc = temp_cp_port_fc;

	/* schedule portq tx_worker to try potential transmission */
	portq_schedule_tx();
}

void portq_rb_stop_cb(struct shm_rbctl *rbctl)
{
	wake_lock_timeout(&acipc_wakeup, HZ * 5);

	/*
	 * schedule portq rx_worker to try potential wakeup, since share memory
	 * maybe already empty when we acqiure this interrupt event
	 */
	printk(KERN_WARNING
		"MSOCK: portq_rb_stop_cb!!!\n");
	portq_schedule_rx();
}

void portq_rb_resume_cb(struct shm_rbctl *rbctl)
{
	wake_lock_timeout(&acipc_wakeup, HZ * 2);

	/* schedule portq tx_worker to try potential transmission */
	printk(KERN_WARNING
		"MSOCK: portq_rb_resume_cb!!!\n");
	portq_schedule_tx();
}


struct shm_callback portq_shm_cb = {
	.peer_sync_cb    = portq_peer_sync_cb,
	.packet_send_cb  = portq_packet_send_cb,
	.port_fc_cb      = portq_port_fc_cb,
	.rb_stop_cb      = portq_rb_stop_cb,
	.rb_resume_cb    = portq_rb_resume_cb,
};

/* portq_init */
int portq_init(void)
{
	int i;

	/* open main ring buffer */
	portq_rbctl = shm_open(shm_rb_main, &portq_shm_cb, NULL);
	if (!portq_rbctl) {
		printk(KERN_ERR "%s open shm error\n", __func__);
		return -1;
	}

	INIT_LIST_HEAD(&tx_portq_head);
	spin_lock_init(&portq_list_lock);
	wake_lock_init(&port_tx_wakelock, WAKE_LOCK_SUSPEND, "port_tx_wakeups");
	wake_lock_init(&port_rx_wakeup,WAKE_LOCK_SUSPEND, "port_rx_wakeups");
	wake_lock_init(&port_rx_NVM_wakeup,WAKE_LOCK_SUSPEND,"port_rx_NVM_wakeups");
	NVM_wake_lock_num = 0;
	NVM_wake_unlock_num = 0;
	rx_workq_sch_num = 0;
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 36)
	portq_wq = alloc_workqueue("acipc-msock", WQ_MEM_RECLAIM | WQ_HIGHPRI, 0);
#else
	portq_wq = create_rt_workqueue("acipc-msock");
#endif
	if (!portq_wq) {
		printk(KERN_ERR "%s: can't create workqueue\n", __func__);
		shm_close(portq_rbctl);
		return -1;
	}
	spin_lock_init(&portq_tx_work_lock);
	spin_lock_init(&portq_rx_work_lock);
	spin_lock_init(&portq_ap_port_fc_lock);

	for (i = 0; i < PORTQ_NUM_MAX; i++)
		portq_array[i] = NULL;

	return 0;
}

/* portq_exit */
void portq_exit(void)
{
	destroy_workqueue(portq_wq);
	wake_lock_destroy(&port_tx_wakelock);
	wake_lock_destroy(&port_rx_wakeup);
	wake_lock_destroy(&port_rx_NVM_wakeup);

	shm_close(portq_rbctl);
}

/* portq_open */
struct portq *portq_open(int port)
{
	struct portq *portq;

	spin_lock_irq(&portq_list_lock);
	if (portq_array[port]) {
		portq_array[port]->refcounts ++;
		spin_unlock_irq(&portq_list_lock);
		return portq_array[port];
	}

	/* Allocate a new port structure */
	portq = kmalloc(sizeof(*portq), GFP_ATOMIC);
	if (!portq) {
		spin_unlock_irq(&portq_list_lock);
		return ERR_PTR(-ENOMEM);
	}

	/* port queue init */
	skb_queue_head_init(&portq->tx_q);
	skb_queue_head_init(&portq->rx_q);
	init_waitqueue_head(&portq->tx_wq);
	init_waitqueue_head(&portq->rx_wq);
	portq->port = port;
	portq->priority = portq_priority[port];
	portq->status = 0;
	spin_lock_init(&portq->lock);

	portq->tx_q_limit = PORTQ_TX_MSG_NUM;
	portq->rx_q_low_wm = PORTQ_RX_MSG_LO_WM;
	portq->rx_q_high_wm = PORTQ_RX_MSG_HI_WM;
	switch (portq->port) {
	case CIDATASTUB_PORT:
		portq->tx_q_limit = PORTQ_TX_MSG_HUGE_NUM;
		portq->rx_q_low_wm =
		    max((portq_rbctl->rx_skbuf_num >> 1),
			PORTQ_RX_MSG_HUGE_LO_WM);
		portq->rx_q_high_wm =
		    max((portq_rbctl->rx_skbuf_num << 1),
			PORTQ_RX_MSG_HUGE_HI_WM);
		break;

	case DIAG_PORT:
		portq->rx_q_low_wm =
		    max((portq_rbctl->rx_skbuf_num >> 1),
			PORTQ_RX_MSG_HUGE_LO_WM);
		portq->rx_q_high_wm =
		    max((portq_rbctl->rx_skbuf_num << 1),
			PORTQ_RX_MSG_HUGE_HI_WM);
		break;

	default:
		break;
	}

	portq->stat_tx_request = 0L;
	portq->stat_tx_sent = 0L;
	portq->stat_tx_drop = 0L;
	portq->stat_tx_queue_max = 0L;
	portq->stat_rx_indicate = 0L;
	portq->stat_rx_got = 0L;
	portq->stat_rx_queue_max = 0L;
	portq->stat_fc_ap_throttle_cp = 0L;
	portq->stat_fc_ap_unthrottle_cp = 0L;
	portq->stat_fc_cp_throttle_ap = 0L;
	portq->stat_fc_cp_unthrottle_ap = 0L;

	/* add new portq to port array */
	portq_array[port] = portq;
	portq->refcounts = 1;
	spin_unlock_irq(&portq_list_lock);

	return portq;
}

/* portq_close */
void portq_close(struct portq *portq)
{
	spin_lock_irq(&portq_list_lock);

	portq_array[portq->port]->refcounts --;

	if(portq_array[portq->port]->refcounts > 0)
	{
		spin_unlock_irq(&portq_list_lock);
		return;
	}
	spin_lock(&portq->lock);
	if (portq->status & PORTQ_STATUS_XMIT_QUEUED) {
		list_del(&portq->tx_list);
	}
	/* clean pending packets */
	skb_queue_purge(&portq->tx_q);
	skb_queue_purge(&portq->rx_q);
	spin_unlock(&portq->lock);

	/* delete port from the portq array */
	portq_array[portq->port] = NULL;

	spin_unlock_irq(&portq_list_lock);

	/* free memory */
	kfree(portq);
}

/* queue the packet to the xmit queue */
int portq_xmit(struct portq *portq, struct sk_buff *skb, bool block)
{
	struct list_head *list;
	unsigned long flags;

	spin_lock_irqsave(&portq->lock, flags);

	/* Make sure there's space to write */
	while (portq_is_tx_full(portq)) {
		if (block) {
			portq->status |= PORTQ_STATUS_XMIT_FULL;

			/* release the lock before wait */
			spin_unlock_irqrestore(&portq->lock, flags);

			if (wait_event_interruptible(portq->tx_wq,
						     !(portq->status
						       &
						       PORTQ_STATUS_XMIT_FULL)))
			{
				/* signal: tell the fs layer to handle it */
				return -ERESTARTSYS;
			}

			/* otherwise loop, but first reacquire the lock */
			spin_lock_irqsave(&portq->lock, flags);

		} else {
			/* remove and free the first skb in queue */
			struct sk_buff *first = skb_dequeue(&portq->tx_q);
			portq->stat_tx_drop++;
			kfree_skb(first);
		}
	}

	if (!msocket_is_synced) {
		spin_unlock_irqrestore(&portq->lock, flags);
		return -1;
	}

	/* add message to queue */
	skb_queue_tail(&portq->tx_q, skb);
	portq->stat_tx_request++;
	if (skb_queue_len(&portq->tx_q) > portq->stat_tx_queue_max)
		portq->stat_tx_queue_max = skb_queue_len(&portq->tx_q);

	if (!(portq->status & PORTQ_STATUS_XMIT_QUEUED)) {
		portq->status |= PORTQ_STATUS_XMIT_QUEUED;

		/* release the lock */
		spin_unlock_irqrestore(&portq->lock, flags);

		spin_lock_irqsave(&portq_list_lock, flags);
		list_for_each(list, &tx_portq_head) {
			struct portq *next =
			    list_entry(list, struct portq, tx_list);
			if (next->priority > portq->priority)
				break;
		}
		list_add_tail(&portq->tx_list, list);
		spin_unlock_irqrestore(&portq_list_lock, flags);
	} else {
		/* release the lock */
		spin_unlock_irqrestore(&portq->lock, flags);
	}

	/* if necessarily, schedule tx_work to send packet */
	spin_lock_irqsave(&portq_tx_work_lock, flags);
	if (!portq_is_tx_work_running) {
		portq_is_tx_work_running = true;
		portq_schedule_tx();
	}
	spin_unlock_irqrestore(&portq_tx_work_lock, flags);

	return 0;
}

/* get a new packet from rx queue */
struct sk_buff *portq_recv(struct portq *portq, bool block)
{
	struct sk_buff *skb;
	unsigned long flags;

	spin_lock_irqsave(&portq->lock, flags);

	/* Make sure there's packet to be read */
	while (block && portq_is_rx_empty(portq)) {

		portq->status |= PORTQ_STATUS_RECV_EMPTY;

		/* release the lock before wait */
		spin_unlock_irqrestore(&portq->lock, flags);

		if (wait_event_interruptible(portq->rx_wq,
					     !(portq->status &
					       PORTQ_STATUS_RECV_EMPTY))) {
			/* signal: tell the fs layer to handle it */
			return ERR_PTR(-ERESTARTSYS);
		}

		/* otherwise loop, but first reacquire the lock */
		spin_lock_irqsave(&portq->lock, flags);
	}

	/* delete message from queue */
	skb = skb_dequeue(&portq->rx_q);
	portq->stat_rx_got++;

	if (skb &&
	    msocket_is_synced &&
	    portq_is_cp_xmit_throttled(portq) &&
	    portq_is_rx_below_lo_wm(portq)) {
		unsigned long flags;

		/* it is not necessarily to put lock before "if" */
		spin_lock_irqsave(&portq_ap_port_fc_lock, flags);
		portq_ap_port_fc &= ~(1 << portq->port);
		portq_rbctl->skctl_va->ap_port_fc = portq_ap_port_fc;
		portq->stat_fc_ap_unthrottle_cp++;
		shm_notify_port_fc(portq_rbctl);
		printk(KERN_WARNING "MSOCK: port %d AP unthrottle CP!!!\n",
		       portq->port);
		spin_unlock_irqrestore(&portq_ap_port_fc_lock, flags);
	}

	/* release the lock */
	spin_unlock_irqrestore(&portq->lock, flags);

	return skb;
}

unsigned int portq_poll(struct portq *portq, struct file *filp, poll_table *wait)
{
	unsigned int mask = 0;
	unsigned long flags;

	poll_wait(filp, &portq->rx_wq, wait);
	poll_wait(filp, &portq->tx_wq, wait);

	spin_lock_irqsave(&portq->lock, flags);

	if (portq_is_rx_empty(portq))
	{
		/*must set this, otherwise rx_worker will not wakeup polling thread*/
		portq->status |= PORTQ_STATUS_RECV_EMPTY;
	}
	else
	{
		mask |= POLLIN | POLLRDNORM;
	}

	if (portq_is_tx_full(portq))
	{
		/*must set this, otherwise tx_worker will not wakeup polling thread*/
		portq->status |= PORTQ_STATUS_XMIT_FULL;
	}
	else
	{
		mask |= POLLOUT | POLLWRNORM;
	}
	spin_unlock_irqrestore(&portq->lock, flags);

	return mask;
}

/* broadcast the linkdown/linkup messages to all the port */
void portq_broadcase_msg(int proc)
{
	struct portq *portq;
	struct shm_skhdr *hdr;
	int i, size;
	unsigned long flags;

	spin_lock_irqsave(&portq_list_lock, flags);

	for (i = 0; i < PORTQ_NUM_MAX; i++) {
		struct sk_buff *skb;

		if (!(portq = portq_array[i]))
			continue;

		/*
		 * allocate sk_buff first, the size = 32 is enough
		 * to hold all kind of broadcasting messages
		 */
		skb = alloc_skb(32, GFP_ATOMIC);
		if (!skb) {
			/* don't known how to do better, just return */
			printk(KERN_ERR "MSOCK: %s: out of memory.\n",
			       __func__);
			spin_unlock_irqrestore(&portq_list_lock, flags);
			return;
		}

		/* reserve port header space */
		skb_reserve(skb, sizeof(*hdr));

		/* so confused diag port */
		if (portq->port == DIAG_PORT) {
			DiagMsgHeader *msg;
			size = sizeof(*msg);
			msg = (DiagMsgHeader *) skb_put(skb, size);
			msg->diagHeader.packetlen = sizeof(msg->procId);
			msg->diagHeader.seqNo = 0;
			msg->diagHeader.msgType = proc;
			msg->procId = proc;
		} else {
			ShmApiMsg *msg;
			size = sizeof(*msg);
			msg = (ShmApiMsg *) skb_put(skb, size);
			msg->svcId = portq->port;	/* svcId == port */
			msg->procId = proc;
			msg->msglen = 0;
		}

		/* reuse the port header */
		hdr = (struct shm_skhdr *)skb_push(skb, sizeof(*hdr));

		/* fill in shm header */
		hdr->address = 0;
		hdr->port = portq->port;
		hdr->checksum = 0;
		hdr->length = size;

		spin_lock(&portq->lock);

		/* add to port rx queue */
		skb_queue_tail(&portq->rx_q, skb);
		portq->stat_rx_indicate++;

		/* notify upper layer that packet available */
		if (portq->status & PORTQ_STATUS_RECV_EMPTY) {
			portq->status &= ~PORTQ_STATUS_RECV_EMPTY;
			wake_up_interruptible(&portq->rx_wq);
		}

		spin_unlock(&portq->lock);
	}

	spin_unlock_irqrestore(&portq_list_lock, flags);
}

/* send messages to a specified port */
void portq_send_msg(int port, int proc)
{
	struct portq *portq;
	struct shm_skhdr *hdr;
	int size;
	unsigned long flags;
	ShmApiMsg *msg;
	struct sk_buff *skb;

	spin_lock_irqsave(&portq_list_lock, flags);

	if (!(portq = portq_array[port]))
		return;

	/*
	* allocate sk_buff first, the size = 32 is enough
	* to hold all kind of broadcasting messages
	*/
	skb = alloc_skb(32, GFP_ATOMIC);
	if (!skb) {
		/* don't known how to do better, just return */
		printk(KERN_ERR "MSOCK: %s: out of memory.\n",
			      __func__);
		spin_unlock_irqrestore(&portq_list_lock, flags);
		return;
	}

	/* reserve port header space */
	skb_reserve(skb, sizeof(*hdr));

	size = sizeof(*msg);
	msg = (ShmApiMsg *) skb_put(skb, size);
	msg->svcId = portq->port;	/* svcId == port */
	msg->procId = proc;
	msg->msglen = 0;

	/* reuse the port header */
	hdr = (struct shm_skhdr *)skb_push(skb, sizeof(*hdr));

	/* fill in shm header */
	hdr->address = 0;
	hdr->port = portq->port;
	hdr->checksum = 0;
	hdr->length = size;

	spin_lock(&portq->lock);

	/* add to port rx queue */
	skb_queue_tail(&portq->rx_q, skb);
	portq->stat_rx_indicate++;

	/* notify upper layer that packet available */
	if (portq->status & PORTQ_STATUS_RECV_EMPTY) {
		portq->status &= ~PORTQ_STATUS_RECV_EMPTY;
		wake_up_interruptible(&portq->rx_wq);
	}

	spin_unlock(&portq->lock);

	spin_unlock_irqrestore(&portq_list_lock, flags);
}

/* flush queue and init */
void portq_flush_init(void)
{
	struct portq *portq;
	unsigned long flags;
	int i;

	spin_lock_irqsave(&portq_list_lock, flags);

	for (i = 0; i < PORTQ_NUM_MAX; i++) {

		if (!(portq = portq_array[i]))
			continue;

		/* acquire queue lock first */
		spin_lock(&portq->lock);

		if (portq->status & PORTQ_STATUS_XMIT_QUEUED) {
			portq->status &= ~PORTQ_STATUS_XMIT_QUEUED;
			list_del(&portq->tx_list);
		}

		/* purge any tx packet in queue */
		skb_queue_purge(&portq->tx_q);

		/* wakeup blocked write */
		if (portq->status & PORTQ_STATUS_XMIT_FULL) {
			portq->status &= ~PORTQ_STATUS_XMIT_FULL;
			wake_up_interruptible(&portq->tx_wq);
		}

		/* purge any rx packet in queue */
		skb_queue_purge(&portq->rx_q);

		/* initialize statistics data */
		portq->stat_tx_request = 0L;
		portq->stat_tx_sent = 0L;
		portq->stat_tx_queue_max = 0L;
		portq->stat_rx_indicate = 0L;
		portq->stat_rx_got = 0L;
		portq->stat_rx_queue_max = 0L;
		portq->stat_fc_ap_throttle_cp = 0L;
		portq->stat_fc_ap_unthrottle_cp = 0L;
		portq->stat_fc_cp_throttle_ap = 0L;
		portq->stat_fc_cp_unthrottle_ap = 0L;

		spin_unlock(&portq->lock);
	}

	spin_unlock_irqrestore(&portq_list_lock, flags);

	/* make sure these variable in it's initial state */
	portq_is_rx_work_running = false;
	portq_is_tx_work_running = false;
	portq_cp_port_fc = 0;
	portq_ap_port_fc = 0;
	NVM_wake_lock_num = 0;
	NVM_wake_unlock_num = 0;
	rx_workq_sch_num = 0;
}

/* schedule portq tx_work */
void portq_schedule_tx(void)
{
	queue_work(portq_wq, &tx_work);
}

/* schedule portq rx_work */
void portq_schedule_rx(void)
{
	queue_work(portq_wq, &rx_work.work);
}

/* schedule msocket peer sync */
void portq_schedule_sync(struct work_struct *work)
{
	queue_work(portq_wq, work);
}

/* ensure that any scheduled work has run to completion */
void portq_flush_workqueue(void)
{
	flush_workqueue(portq_wq);
}

/*
 * portq transmitter worker
 */
static void portq_tx_worker(struct work_struct *work)
{
	struct portq *portq, *next;
	struct sk_buff *skb;
	int num, total = 0;
	int priority;
	ShmApiMsg *pShmArgs;
	while (1) {
		/*
		 * we do lock/unlock inner the loop, giving the other cpu(s) a
		 * chance to run the same worker when in SMP environments
		 */
		spin_lock_irq(&portq_list_lock);
		wake_lock(&port_tx_wakelock);
		if (portq_rbctl->is_ap_xmit_stopped) {
			/* if any packet sent, notify cp */
			if (total > 0)
				shm_notify_packet_sent(portq_rbctl);
			wake_unlock(&port_tx_wakelock);
			spin_unlock_irq(&portq_list_lock);
			return;
		}

		/*
		 * num is used to count sent packet numbers during the
		 * following loop
		 */
		num = 0;

		priority = 0;	/* set 0 as a special value */

		/*
		 * priority schedule transmitting one packet every queue
		 * with the same priority
		 */
		list_for_each_entry_safe(portq, next, &tx_portq_head, tx_list) {

			if (!msocket_is_synced) {
				/* if not sync, just return */
				wake_unlock(&port_tx_wakelock);
				spin_unlock_irq(&portq_list_lock);
				return;
			}

			/* acquire queue lock first */
			spin_lock(&portq->lock);

			if (portq_is_ap_xmit_throttled(portq)) {
				/*
				 * port throttled by CP, skip this queue
				 */
				spin_unlock(&portq->lock);
				continue;
			}

			if (shm_is_xmit_full(portq_rbctl)) {	/* if ring buffer is full */
				/*
				 * notify CP that AP transmission stopped
				 * because of no tx ring buffer available,
				 * CP should wake me up after there are
				 * enough ring buffers free
				 */
				shm_notify_ap_tx_stopped(portq_rbctl);

				/* release portq lock */
				spin_unlock(&portq->lock);

				/* if any packet sent, notify cp */
				if (total > 0)
					shm_notify_packet_sent(portq_rbctl);

				wake_unlock(&port_tx_wakelock);
				spin_unlock_irq(&portq_list_lock);
				return;
			}

			if (!priority) {
				/* first packet during this loop */
				priority = portq->priority;
			} else if (priority != portq->priority) {
				/*
				 * we only send packet with the same priority
				 * during this loop
				 */
				spin_unlock(&portq->lock);	/* release lock */
				break;
			}

			/*
			 * otherwise, we meet either the first packet or the
			 * same priority packet situation, so do the sending
			 * operation
			 */
			skb = skb_dequeue(&portq->tx_q);	/* rm skb from tx_q */
			portq->stat_tx_sent++;
			if (skb_queue_empty(&portq->tx_q)) {
				portq->status &= ~PORTQ_STATUS_XMIT_QUEUED;
				list_del(&portq->tx_list);
			}
			shm_xmit(portq_rbctl, skb);	/* write to rb */
			if((dump_flag & DUMP_TX) && (dump_flag & DUMP_PORT(portq->port)))
			{
				int i;
				printk(" Msocket TX: DUMP BEGIN port :%d, Length:%d --\n", portq->port, skb->len);
				if(!(dump_flag & DUMP_TX_SP))
				{
					unsigned char * data = skb->data;
					for(i = 0; i < skb->len; i++)
					printk("%02x", data[i]);
					printk("\n");
				}
			}
			pShmArgs = (ShmApiMsg *)skb_pull(skb,sizeof(struct shm_skhdr));
			if((portq->port == NVMSRV_PORT )&&(pShmArgs->procId == NVMCnfProcId))	//NVM server port
			{
				wake_unlock(&port_rx_NVM_wakeup);
				NVM_wake_unlock_num ++;
			}
			kfree_skb(skb);	/* free skb memory */
			num++, total++;	/* count */

			/* notify upper layer that free socket available */
			if (portq->status & PORTQ_STATUS_XMIT_FULL) {
				portq->status &= ~PORTQ_STATUS_XMIT_FULL;
				wake_up_interruptible(&portq->tx_wq);
			}

			spin_unlock(&portq->lock);	/* release lock */
		}

		/* very trick code, lock must be here */
		spin_lock(&portq_tx_work_lock);
		if (num == 0) {
			/* all the tx queue is empty or flow control blocked */

			portq_is_tx_work_running = false;

			spin_unlock(&portq_tx_work_lock);

			/* if any packet sent, notify cp */
			if (total > 0)
				shm_notify_packet_sent(portq_rbctl);
			/* unlock and return */
			wake_unlock(&port_tx_wakelock);
			spin_unlock_irq(&portq_list_lock);
			return;
		}
		spin_unlock(&portq_tx_work_lock);

		/*
		 * as a good citizen, we should give other people a
		 * chance to do their emergency jobs
		 *
		 * NOTICE: total may > PORTQ_TX_WORKQ_SHOTS
		 */
		if (total >= portq_rbctl->tx_skbuf_num) {
			/* reschedule self again */
			portq_schedule_tx();

			/* notify CP */
			shm_notify_packet_sent(portq_rbctl);

			/* unlock and return */
			wake_unlock(&port_tx_wakelock);
			spin_unlock_irq(&portq_list_lock);
			return;
		}
		wake_unlock(&port_tx_wakelock);
		spin_unlock_irq(&portq_list_lock);
	}
}

/*
 * portq receiver worker
 */
static void portq_rx_worker(struct work_struct *work)
{
	struct portq *portq;
	struct sk_buff *skb;
	int i, port;
	ShmApiMsg *pShmArgs;

	rx_workq_sch_num ++;
	for (i = 0; i < portq_rbctl->rx_skbuf_num; i++) {

		if (!msocket_is_synced) {
			/* if not sync, just return */
			spin_lock_irq(&portq_rx_work_lock);
			portq_is_rx_work_running = false;
			spin_unlock_irq(&portq_rx_work_lock);
			return;
		}

		/*
		 * we do lock/unlock inner the loop, giving the other cpu(s) a
		 * chance to run the same worker when in SMP environments
		 */
		spin_lock_irq(&portq_list_lock);

		/* process share memory socket buffer flow control */
		if (portq_rbctl->is_cp_xmit_stopped
		    && shm_has_enough_free_rx_skbuf(portq_rbctl)) {
			shm_notify_cp_tx_resume(portq_rbctl);
		}

		/* be carefull and careful again, lock must be here */
		spin_lock(&portq_rx_work_lock);
		if (shm_is_recv_empty(portq_rbctl)) {
			portq_is_rx_work_running = false;
			spin_unlock(&portq_rx_work_lock);
			spin_unlock_irq(&portq_list_lock);
			wake_unlock(&acipc_wakeup);
			return;
		}
		spin_unlock(&portq_rx_work_lock);

		skb = shm_recv(portq_rbctl);
		if (!skb) {
			/* if out of memory, try reschedule after a tick */
			queue_delayed_work(portq_wq, &rx_work, 1);
			spin_lock(&portq_rx_work_lock);
			portq_is_rx_work_running = false;
			spin_unlock(&portq_rx_work_lock);
			spin_unlock_irq(&portq_list_lock);
			printk("portq_rx_worker: invalid skb for receiving packet\n");
			return;
		}

		/* get port */
		port = ((struct shm_skhdr *)skb->data)->port;
		if (port <= 0 || port >= PORTQ_NUM_MAX){
			kfree_skb(skb);
			spin_unlock_irq(&portq_list_lock);
			printk("portq_rx_worker: receiving packet in invalid port:%d\n", port);
			continue;
		}
 
		portq = portq_array[port];

		if (!portq) {	/* port is closed */
			kfree_skb(skb);
			spin_unlock_irq(&portq_list_lock);
			printk("portq_rx_worker: receiving packet,but port:%d is closed\n", port);
			continue;
		}
		pShmArgs = (ShmApiMsg *)skb_pull(skb,sizeof(struct shm_skhdr));
		if((port == NVMSRV_PORT)&& (pShmArgs->procId == NVMReqProcId))	//NVM server port
		{
			wake_lock_timeout(&port_rx_NVM_wakeup,8*HZ);
			NVM_wake_lock_num++;
		}
		else
		{
			wake_lock_timeout(&port_rx_wakeup,2*HZ);
		}
		skb_push(skb,sizeof(struct shm_skhdr));
		if((dump_flag & DUMP_RX) && (dump_flag & DUMP_PORT(portq->port)))
		{
			int i;
			printk(" Msocket RX: DUMP BEGIN port :%d, Length:%d --\n", portq->port, skb->len);
			if(!(dump_flag & DUMP_RX_SP))
			{
				unsigned char * data = skb->data;
				for(i = 0; i < skb->len; i++)
					printk("%02x", data[i]);
				printk("\n");
			}
		}
		spin_lock(&portq->lock);

		skb_queue_tail(&portq->rx_q, skb);
		portq->stat_rx_indicate++;
		if (skb_queue_len(&portq->rx_q) > portq->stat_rx_queue_max)
			portq->stat_rx_queue_max = skb_queue_len(&portq->rx_q);

		/* process port flow control */
		if (!portq_is_cp_xmit_throttled(portq)
		    && portq_is_rx_above_hi_wm(portq)) {
			/* it is not necessarily to put lock before "if" */
			spin_lock(&portq_ap_port_fc_lock);
			portq_ap_port_fc |= (1 << portq->port);
			portq_rbctl->skctl_va->ap_port_fc =
			    portq_ap_port_fc;
			portq->stat_fc_ap_throttle_cp++;
			shm_notify_port_fc(portq_rbctl);
			printk(KERN_WARNING
			       "MSOCK: port %d AP throttle CP!!!\n",
			       portq->port);
			spin_unlock(&portq_ap_port_fc_lock);
		}

		/* notify upper layer that packet available */
		if (portq->status & PORTQ_STATUS_RECV_EMPTY) {
			portq->status &= ~PORTQ_STATUS_RECV_EMPTY;
			wake_up_interruptible(&portq->rx_wq);
		}

		spin_unlock(&portq->lock);

		spin_unlock_irq(&portq_list_lock);
	}

	/*
	 * if goes here, maybe there are pending packets to process,
	 * so we reschedule it
	 */
	portq_schedule_rx();
}

void portq_set_dumpflag(int flag)
{
	dump_flag = flag;
}
int portq_get_dumpflag()
{
	return dump_flag;
}
