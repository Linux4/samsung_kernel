/* iodebug_common.c
 *
 * add this file for IO debug.
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 1, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */
#include <linux/sprd_iodebug.h>
#include <linux/init.h>
#include <linux/skbuff.h>
#include <linux/netlink.h>
#include <net/netlink.h>
#include <net/net_namespace.h>
#include <linux/delay.h>
#include <linux/timer.h>
#include <linux/sched.h>
#include <linux/vmalloc.h>
#include <linux/oom.h>
#include <linux/buffer_head.h>

#ifdef CONFIG_SPRD_IODEBUG_VFS
extern int iodebug_vfs_trace_init(void);
extern int iodebug_vfs_timer_init(void);
#endif
#ifdef CONFIG_SPRD_IODEBUG_BDI
extern int iodebug_bdi_trace_init(void);
#endif
struct dentry *iodebug_root_dir;
static unsigned int iodebug_trace_enable = 0;
asmlinkage int iodebug_trace(const char *fmt, ...)
{
	va_list args;
	int r;

	if (iodebug_trace_enable){
		va_start(args, fmt);
		r = vprintk_emit(0, -1, NULL, 0, fmt, args);
		va_end(args);
	}
	return r;
}
EXPORT_SYMBOL(iodebug_trace);


/*
 * io_schedule debug
 */
#ifdef CONFIG_SPRD_IODEBUG_IOSCHEDULE
struct ioschedule_timer {
	struct timer_list lock_timer;
	struct list_head entry;
	int alloc_flag;
};

struct ioschedule_timer_list {
	struct list_head free;
	struct list_head used;
	spinlock_t list_lock;
};

#define IOSCHEDULE_TIMER_NUM 32
#define FROM_LIST 77
#define FROM_ALLOC 88

struct ioschedule_timer ioschedule_timers[IOSCHEDULE_TIMER_NUM];
struct ioschedule_timer_list  ioschedule_timerlist;
static int ioschedule_alerts_threshold = 3000;
static int ioschedule_alerts_enable = 1;
static void ioschedule_timer_list_init(struct ioschedule_timer_list* timerlist)
{
	int i=0;
	struct list_head* free_head = &timerlist->free;
	struct list_head* used_head = &timerlist->used;
	struct ioschedule_timer *ptimer;

	INIT_LIST_HEAD(free_head);
	INIT_LIST_HEAD(used_head);
	spin_lock_init(&timerlist->list_lock);

	for(; i<IOSCHEDULE_TIMER_NUM; i++){
		/*add timer to free_head*/
		ptimer = &ioschedule_timers[i];
		memset(ptimer,0,sizeof(struct ioschedule_timer));
		INIT_LIST_HEAD(&ptimer->entry);
		list_add_tail(&ptimer->entry,free_head);
	}
	printk("iodebug: ioschedule_timer_list_init done.\n");
}

static struct ioschedule_timer* get_one_timer(struct ioschedule_timer_list* timerlist)
{
	struct ioschedule_timer *ptimer;

	spin_lock_bh(&timerlist->list_lock);
	if (!list_empty(&timerlist->free)){
		/*get the first element from a list*/
		ptimer = list_first_entry(&timerlist->free, struct ioschedule_timer, entry);
		ptimer->alloc_flag = FROM_LIST;
		/*move the timer to used_head*/
		list_move_tail(&ptimer->entry, &timerlist->used);
		spin_unlock_bh(&timerlist->list_lock);
	}else{
		spin_unlock_bh(&timerlist->list_lock);
		/*no free timer, alloc a timer*/
		printk("iodebug: the timerlist is empty, alloc one.\n");
		ptimer = __vmalloc(sizeof(struct ioschedule_timer), GFP_ATOMIC, PAGE_KERNEL);
		if (ptimer)
			ptimer->alloc_flag = FROM_ALLOC;
	}

	return ptimer;
}

static void release_one_timer(struct ioschedule_timer_list* timerlist,struct timer_list* timer)
{
	struct ioschedule_timer* ptimer = container_of(timer, struct ioschedule_timer, lock_timer);

	spin_lock_bh(&timerlist->list_lock);
	if (ptimer->alloc_flag == FROM_LIST){
		/*moveback the timer from used_head to free_head*/
		list_move_tail(&ptimer->entry, &timerlist->free);
		spin_unlock_bh(&timerlist->list_lock);
	}else{
		spin_unlock_bh(&timerlist->list_lock);
		/*free the timer*/
		vfree(ptimer);
	}
}

static inline int oom_score2oom_adj(int oom_score_adj)
{
	if (oom_score_adj == OOM_SCORE_ADJ_MAX)
		return OOM_ADJUST_MAX;
	else
		return (oom_score_adj * (-OOM_DISABLE) + OOM_SCORE_ADJ_MAX - 1) / OOM_SCORE_ADJ_MAX;
}

static void show_taskstack(struct task_struct *p, struct page *on_page, struct buffer_head *on_buffer)
{
	int oom_adj = OOM_ADJUST_MIN;
	const char stat_nam[] = TASK_STATE_TO_CHAR_STR;
	unsigned state;

	if ((on_page && on_buffer) || (!on_page && !on_buffer)){
		return;
	}

	/*show the blocked task backtrace*/
	oom_adj = oom_score2oom_adj(p->signal->oom_score_adj);
	state = p->state ? __ffs(p->state) + 1 : 0;
	printk("iodebug: %s(pid:%d,adj:%d,state:%c) locked on %s(0x%08lx) over %dms.\n",
			p->comm, p->pid, oom_adj,
			state < sizeof(stat_nam) - 1 ? stat_nam[state] : '?',
			on_page ? "page":"buffer_page",
			on_page ? on_page:(on_buffer->b_page),
			ioschedule_alerts_threshold);
	show_stack(p, NULL);
}


static void ioschedule_timer_action(unsigned long data)
{
	struct task_struct* task;
	struct page *on_page;
	struct buffer_head *on_buffer;

	task = (struct task_struct *)data;
	if (!task->lock_timer || (!task->lock_on_page && !task->lock_on_buffer)
				|| (task->state != TASK_UNINTERRUPTIBLE)){
		return;
	}

	on_page = task->lock_on_page;
	on_buffer = task->lock_on_buffer;
	if (task->state == TASK_UNINTERRUPTIBLE){
		/*send msg to iodebug thread in slog */
		if (iodebug_nl_send_msg(NLMSGTYPE_K2U_SEND_EVENT,NLMSGEVENT_K2U_SHOW_IO_INFO)){
			printk("iodebug: Warning ioschedule_timer_action fail to send!\n");
		}

		/*dump task's backtrace*/
		show_taskstack(task, on_page, on_buffer);
	}
}


void iodebug_ioschedule_timer_add(struct task_struct *task)
{
	struct ioschedule_timer* ptimer;

	if (!ioschedule_alerts_enable || (task->flags & PF_KTHREAD))
		return;

	/*only monitor the task which be locked on page or buffer*/
	if (!task->lock_on_page && !task->lock_on_buffer)
		return;

	if (task->lock_timer){
		return;
	}

	ptimer = get_one_timer(&ioschedule_timerlist);
	if (ptimer){
		init_timer(&ptimer->lock_timer);
		ptimer->lock_timer.data = (unsigned long)task;
		ptimer->lock_timer.function = ioschedule_timer_action;
		ptimer->lock_timer.expires = jiffies + msecs_to_jiffies(ioschedule_alerts_threshold);
		add_timer(&ptimer->lock_timer);
		task->lock_timer = &ptimer->lock_timer;
	}else{
		task->lock_timer = NULL;
		printk("iodebug: Warning failed to get ioschedule_timer!\n");
	}
}
EXPORT_SYMBOL(iodebug_ioschedule_timer_add);

void iodebug_ioschedule_timer_cancel(struct task_struct *task)
{
	struct timer_list* locktimer;

	if (!task->lock_timer){
		task->lock_on_page = NULL;
		task->lock_on_buffer = NULL;
		return;
	}

	/*release ioschedule_timer*/
	locktimer = task->lock_timer;
	del_timer_sync(locktimer);
	release_one_timer(&ioschedule_timerlist, locktimer);
	task->lock_timer = NULL;
	task->lock_on_page = NULL;
	task->lock_on_buffer = NULL;
}
EXPORT_SYMBOL(iodebug_ioschedule_timer_cancel);
#endif


/*
 *iodebug support netlink
 */
#define NLMSG_USER_PID_MAXLEN 4
static struct sock *iodebug_nlsock;
static int nlsock_user_data = 0;
static int nlsock_user_pid = 0;
static int iodebug_nl_rcv_pid(struct sk_buff *skb, struct nlmsghdr *nlh)
{
	int len = nlmsg_len(nlh);

	if (len > NLMSG_USER_PID_MAXLEN){
		printk("iodebug: Invalid data len.\n");
		return -ERANGE;
	}
	nlsock_user_data = *(int *)nlmsg_data(nlh);
	nlsock_user_pid = nlh->nlmsg_pid;
	printk("iodebug: Establish connection with client, data:0x%x, pid:%d.\n",nlsock_user_data,nlsock_user_pid );
#ifdef CONFIG_SPRD_IODEBUG_VFS
	iodebug_vfs_timer_init();
#endif
	return 0;
}

int iodebug_nl_send_msg(int msg_type, int event_key)
{
	int res;
	char *data;
	struct nlmsghdr *nlh;
	struct sk_buff *skb_out;
	int msg_size;

	if (!nlsock_user_pid){
		printk("iodebug: Please establish connection firstly!\n");
		return -1;
	}
	msg_size = sizeof(event_key);
	/*alloc sk_buff, include nlmsg head and data*/
	skb_out = nlmsg_new(msg_size, 0);
	if (!skb_out){
		printk("iodebug: Failed to allocate new skb.\n");
		return -1;
	}

	nlh = nlmsg_put(skb_out, nlsock_user_pid, 0, msg_type, msg_size, 0);
	if (!nlh){
		kfree_skb(skb_out);
		printk("iodebug: Failed to nlmsg_put skb.\n");
		return -1;
	}
	NETLINK_CB(skb_out).dst_group = 0; /* not in mcast group */
	data = nlmsg_data(nlh);
	*(int *)data = event_key;

	res = nlmsg_unicast(iodebug_nlsock, skb_out, nlsock_user_pid);
	if (res < 0){
		printk("iodebug: Error while sending event to user.\n");
		return -1;
	}
	return 0;
}
EXPORT_SYMBOL(iodebug_nl_send_msg);


static int iodebug_nl_rcv_msg(struct sk_buff *skb, struct nlmsghdr *nlh)
{
	unsigned short type;
	type = nlh->nlmsg_type;
	switch (type) {
	case NLMSGTYPE_U2K_GET_PID:
		return iodebug_nl_rcv_pid(skb, nlh);
	default:
		return -EINVAL;
	}
}

static void iodebug_nl_rcv(struct sk_buff *skb)
{
	netlink_rcv_skb(skb, &iodebug_nl_rcv_msg);
}

static int iodebug_nl_init(void)
{
	struct netlink_kernel_cfg cfg = {
		.input  = iodebug_nl_rcv,
	};

	iodebug_nlsock = netlink_kernel_create(&init_net, NETLINK_IODEBUG, &cfg);
	if (!iodebug_nlsock){
		printk("iodebug: iodebug netlink init failed.\n");
		return -ENOMEM;
	}
	printk("iodebug: iodebug netlink init done.\n");
	return 0;
}



/*iodebug init*/
static int __init iodebug_init(void)
{
	int ret;

	/*create io_debug directory*/
	iodebug_root_dir = debugfs_create_dir("iodebug", NULL);
	if (!iodebug_root_dir){
		printk(KERN_ERR "Can't create debugfs dir for IO debug.\n");
		return ENOENT;
	}
	/*netlink init*/
	iodebug_nl_init();

#ifdef CONFIG_SPRD_IODEBUG_HOTPOINT
	iodebug_point_monitor_init();
#endif
#ifdef CONFIG_SPRD_IODEBUG_BDI
	iodebug_bdi_trace_init();
#endif
#ifdef CONFIG_SPRD_IODEBUG_BDI
	iodebug_vfs_trace_init();
#endif

#ifdef CONFIG_SPRD_IODEBUG_IOSCHEDULE
	ioschedule_timer_list_init(&ioschedule_timerlist);
#endif
	return ret;
}


/*iodebug exit*/
static void __exit iodebug_exit(void)
{
	netlink_kernel_release(iodebug_nlsock);
}

module_param_named(trace_enable, iodebug_trace_enable, int, S_IRUGO | S_IWUSR);
module_param_named(ioschedule_alerts_threshold, ioschedule_alerts_threshold, int, S_IRUGO | S_IWUSR);
module_param_named(ioschedule_alerts_enable, ioschedule_alerts_enable, int, S_IRUGO | S_IWUSR);

module_init(iodebug_init);
module_exit(iodebug_exit);

MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("IO debug feature.");
