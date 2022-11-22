/*
 * Copyright (c) 2020 Samsung Electronics Co., Ltd.
 *      http://www.samsung.com
 *
 * Samsung debugging code
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/kernel.h>
#include <linux/sched.h>
#include <linux/sched/signal.h>
#include <linux/sec_debug.h>

#define DEBUGGER_SIGNAL 35 //__SIGRTMIN+3

void secdbg_send_sig_debuggerd(struct task_struct *p, int opt)
{
	struct task_struct *parent;
	siginfo_t info;

	if (p == NULL) {
		pr_err("%s task structure address is NULL\n", __func__);
		return;
	}

	if (!p->pid) {
		pr_err("%s pid is 0\n", __func__);
		return;
	}

	if (p->real_parent == NULL) {
		pr_err("%s Task %s (pid:%d) parent is NULL\n", __func__, p->comm, p->pid);
		return;
	}

	parent = rcu_dereference(p->real_parent);

	/* native daemon : ppid  init task (pid 1)
	 * java thread   : ppid  main task
	 * kernel thread : ppid  kthreadd  
	 */
	if (!strcmp(parent->comm, "init"))
		pr_info("%s Native Daemon PID: %d, Task Name: %s\n", __func__, p->pid, p->comm);
	else if (!strcmp(parent->comm, "main"))
		pr_info("%s Java thread PID: %d, Task Name: %s\n", __func__, p->pid, p->comm);
	else if (!strcmp(p->comm, "init") || !strcmp(p->comm, "main"))
		pr_info("%s PID  %d, Task Name %s\n", __func__, p->pid, p->comm);
	else {
		pr_crit("%s Do not send sig. PID: %d, Task Name: %s\n", __func__, p->pid, p->comm);
		return;
	}

	info.si_int = opt;
	info.si_signo = DEBUGGER_SIGNAL;
	info.si_code = SI_QUEUE;
	send_sig_info(DEBUGGER_SIGNAL, &info, p);
}
