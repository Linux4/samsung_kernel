/*
 * Copyright (C) 2012 Spreadtrum Communications Inc.
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#include <linux/sched.h>
#include <linux/sysrq.h>
#include <linux/nmi.h>

void show_state_group(unsigned long state_filter)
{
	struct task_struct *p, *g;

	rcu_read_lock();
	do_each_thread(g, p) {
		touch_nmi_watchdog();
		if (p->state & state_filter){
			printk(KERN_INFO "\nGroup: %s - %d\n", g->comm, g->pid);
			/* dump all threads stack in same group */
			p = g;
			do{
				sched_show_task(p);
			}while_each_thread(g, p);
			break;
		}
	} while_each_thread(g, p);
	rcu_read_unlock();
}

static void sysrq_handle_show_blocked_group(int key)
{
	show_state_group(TASK_UNINTERRUPTIBLE);
}

struct sysrq_key_op sysrq_show_blocked_group_op = {
	.handler	= sysrq_handle_show_blocked_group,
	.help_msg	= "show-blocked-group(x)",
	.action_msg	= "Show Blocked Group State",
	.enable_mask	= SYSRQ_ENABLE_DUMP,
};

