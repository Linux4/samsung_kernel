/*
 *
 *  Copyright (C) 2015 Spreadtrum Communication - All Rights Reserved.
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 */

#define DEBUG

#include <linux/capability.h>
#include <linux/fs.h>
#include <linux/kallsyms.h>
#include <linux/mutex.h>
#include <linux/proc_fs.h>
#include <linux/root_recorder.h>
#include <linux/rtc.h>
#include <linux/seq_file.h>
#include <linux/sched.h>
#include <linux/slab.h>
#include <linux/string.h>
#include <linux/uaccess.h>
#include <linux/utsname.h>
#include <linux/workqueue.h>


#define ROOT_MAGIC 0x524F4F54
#define ROOT_OFFSET 512


struct root_stat {
	u32 magic;
	u32 root_flag;
};

struct root_record {
	char *process_name;
	char *parent_name;
	int is_pgl;
	int uid;
	int euid;
	int sid;
};

struct write_struct {
	struct work_struct work;
	void (*write_emmc_func)(void);
	void (*write_file_func)(void);
};

static struct root_record kill_process_table[] = {
	{"zygote", "init", 1, 0, 0, 0},
	{"vold", "init", 1, 0, 0, 0},
	{"debuggerd", "init", 1, 0, 0, 0},
	{"system_server", "zygote", 0, 1000, 1000, 0},
	{"debuggerd", "debuggerd", 0, 0, 0, 0},
	{"system_server", "main", 0, 1000, 1000, 0},
	{"dumpstate", "init", 1, 0, 0, 0},
	{"netd", "init", 1, 0, 0, 0},
	{NULL, NULL, 0, 0, 0, 0},
};

static struct root_record sys_module_table[] = {
	{"tc", "netd", 0, 0, 0, 0},
	{"wlan_loader", "init", 1, 0, 0, 0},
	{"system_server", "main", 0, 1000, 1000, 0},
	{"system_server", "zygote", 0, 1000, 1000, 0},
	{"main", "init", 1, 0, 9999, 0},
	{"zygote", "init", 1, 0, 9999, 0},
	{NULL, NULL, 0, 0, 0, 0},
};

static struct root_record sys_admin_table[] = {
	{"init", "init", 0, 0, 0, 0},
	{"vold", "init", 1, 0, 0, 0},
	{"netd", "init", 1, 0, 0, 0},
	{"debuggerd", "debuggerd", 0, 0, 0, 0},
	{"sdcard", "init", 1, 0, 0, 0},
	{"swapon", "zram.sh", 0, 0, 0, 0},
	{"main", "init", 1, 0, 0, 0},
	{"zygote", "init", 1, 0, 0, 0},
	{"system_server", "main", 0, 1000, 1000, 0},
	{"system_server", "zygote", 0, 1000, 1000, 0},
	{"vold", "init", 1, 0, 0, 0},
	{"main", "main", 0, 0, 0, 0},
	{NULL, NULL, 0, 0, 0, 0},
};

static struct root_record sys_ptrace_table[] = {
	{"debuggerd", "init", 1, 0, 0, 0},
	{"debuggerd", "debuggerd", 0, 0, 0, 0},
	{"query_task_fd", "sh", 0, 0, 0, 0},
	{"dumpstate", "init", 1, 0, 0, 0},
	{"vold", "init", 1, 0, 0, 0},
	{NULL, NULL, 0, 0, 0, 0},
};

static struct root_record setpcap_table[] = {
	{NULL, NULL, 0, 0, 0, 0},
};

static struct root_record set_uid_table[] = {
	{"zygote", "init", 1, 0, 9999, 0},
	{"main", "init", 1, 0, 9999, 0},
	{NULL, NULL, 0, 0, 0},
};

static int is_stat_writed_emmc;
static int is_stat_writed_file;

static int inspect_root_capability(struct root_record *table,
					struct task_struct *task)
{
	int i = 0;
	int flag;
	for (i = 0; table[i].process_name; i++) {
		if (!strncmp(task->group_leader->comm,
			table[i].process_name, TASK_COMM_LEN) &&
			!strncmp(task->real_parent->group_leader->comm,
			table[i].parent_name, TASK_COMM_LEN)) {

			if (!strncmp(table[i].parent_name, "init", 5)) {
				if (task->real_parent->group_leader->pid
				    != 1) {
					return 1;
				}
			}
			flag = task_pgrp_vnr(task) == task->tgid ? 1 : 0;
			if (flag == table[i].is_pgl &&
				task_uid(task) == table[i].uid &&
				task_euid(task) == table[i].euid) {

				if (task_session_vnr(task) == table[i].sid)
					return 0;
			}
		}
	}
	return 1;
}

int inspect_illegal_root_capability(int cap)
{

	int result;
	if(is_stat_writed_emmc && is_stat_writed_file) {
		pr_debug_once("[root_recorder]root stat writed, stop check\n");
		return 0;
	}
	if (current->mm == NULL)
		return 0;
	if (current->pid == 1)
		return 0;
	if (cap == CAP_KILL)
		result = inspect_root_capability(kill_process_table, current);
	else if (cap == CAP_SYS_MODULE)
		result = inspect_root_capability(sys_module_table, current);
	else if (cap == CAP_SYS_ADMIN)
		result = inspect_root_capability(sys_admin_table, current);
	else if (cap == CAP_SYS_PTRACE)
		result = inspect_root_capability(sys_ptrace_table, current);
	else if (cap == CAP_SETPCAP)
		result = inspect_root_capability(setpcap_table, current);
	else if (cap == -1)
		result = inspect_root_capability(set_uid_table, current);
	else
		result = 0;

	return result;
}

static void write_root_stat_emmc(void)
{
	int errno;
	struct file *filp;
	mm_segment_t old_fs;
	struct root_stat stat;
	if (is_stat_writed_emmc)
		return;
	old_fs = get_fs();
	set_fs(KERNEL_DS);
	filp = filp_open("/dev/block/platform/sdio_emmc/by-name/miscdata",
			 O_RDWR, 0);
	if (IS_ERR_OR_NULL(filp)) {
		pr_debug_once("[root_recorder] open misc errno=%ld\n", (long)filp);
		goto err_file_open;
	}

	if (filp->f_op && filp->f_op->llseek) {
		errno = filp->f_op->llseek(filp, ROOT_OFFSET, SEEK_SET);
		if (errno < 0) {
			pr_debug_once("[root_recorder] llseek errno=%d\n", errno);
			goto err_file_op;
		}
	} else {
		goto err_file_op;
	}
	stat.magic = ROOT_MAGIC;
	stat.root_flag = 1;

	if (filp->f_op && filp->f_op->write) {
		errno = filp->f_op->write(filp, (const char *)&stat,
					  sizeof(stat), &filp->f_pos);
		if (errno != sizeof(stat)) {
			pr_debug_once("[root_recorder] writeemmc.errno=%d\n", errno);
			goto err_file_op;
		}
		is_stat_writed_emmc = 1;
		pr_debug_once("[root_recorder] rootflag writed emmc.\n");
	} else {
		goto err_file_op;
	}
err_file_op:
	filp_close(filp, NULL);
err_file_open:
	set_fs(old_fs);
}

static void write_root_stat_file(void)
{
	int errno;
	struct file *filp;
	mm_segment_t old_fs;
	struct root_stat stat;
	if (is_stat_writed_file)
		return;
	old_fs = get_fs();
	set_fs(KERNEL_DS);
	filp = filp_open("/data/misc/rootrecorder",
		O_CREAT|O_TRUNC|O_RDWR, S_IRUSR|S_IWUSR|S_IRGRP|S_IROTH);
	if (IS_ERR_OR_NULL(filp)) {
		pr_debug_once("[root_recorder] open data errno=%ld\n", (long)filp);
		goto err_file_open;
	}

	stat.magic = ROOT_MAGIC;
	stat.root_flag = 1;

	if (filp->f_op && filp->f_op->write) {
		errno = filp->f_op->write(filp, (const char *)&stat,
					  sizeof(stat), &filp->f_pos);
		if (errno != sizeof(stat)) {
			pr_debug_once("[root_recorder] writefile.errno=%d\n", errno);
			goto err_file_op;
		}
		is_stat_writed_file = 1;
		pr_debug_once("[root_recorder] rootflag writed file.\n");
	} else {
		goto err_file_op;
	}
err_file_op:
	filp_close(filp, NULL);
err_file_open:
	set_fs(old_fs);
}

static void do_write(struct work_struct *work)
{
	struct write_struct *write_task;
	write_task = container_of(work, struct write_struct, work);
	write_task->write_emmc_func();
	write_task->write_file_func();
	kfree(write_task);

}

void record_illegal_root(const char *behavior, int cap)
{
	struct write_struct *write_task;
	pr_debug_once("[root_recorder] rooted PID=%d %s:%s-%d\n", current->tgid,
		      current->group_leader->comm, behavior, cap);
	pr_debug_once("[root_recorder]task pid=%d, processname=%s,\
		      parentname=%s, PGL=%d, uid=%d, euid=%d, sid=%d \n", current->tgid,
		      current->group_leader->comm, current->real_parent->group_leader->comm,
		      (task_pgrp_vnr(current) == current->tgid ? 1 : 0), task_uid(current),
		      task_euid(current), task_session_vnr(current));
	write_task = kzalloc(sizeof(struct write_struct), GFP_KERNEL);
	if (write_task == NULL) {
		pr_debug_once("[root_recorder]write_task create failed\n");
		return;
	}
	write_task->write_emmc_func = write_root_stat_emmc;
	write_task->write_file_func = write_root_stat_file;
	INIT_WORK(&write_task->work, do_write);
	schedule_work(&write_task->work);
}
