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
#include<linux/completion.h>
#include <linux/fs.h>
#include <linux/genhd.h>
#include <linux/kobject.h>

#define ROOT_MAGIC 0x524F4F54
#define ROOT_OFFSET 512
#define ROOT_COUNT_LIMIT 20


struct root_stat {
	u32 magic;
	u32 root_flag;
	u32 count;
};

struct root_record {
	char process_name[TASK_COMM_LEN];
	char parent_name[TASK_COMM_LEN];
	int is_pgl;
	int uid;
	int euid;
	int sid;
	int cap; /* capbility of process*/
};

struct write_struct {
	struct work_struct work;
	void (*write_emmc_func)(void);
};

struct get_struct {
	struct work_struct work;
	struct completion comp;
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
	{"slog", "init", 1, 0, 0, 0},
	{"", "", 0, 0, 0, 0},
};

static struct root_record sys_module_table[] = {
	{"tc", "netd", 0, 0, 0, 0},
	{"wlan_loader", "init", 1, 0, 0, 0},
	{"system_server", "main", 0, 1000, 1000, 0},
	{"system_server", "zygote", 0, 1000, 1000, 0},
	{"main", "init", 1, 0, 9999, 0},
	{"zygote", "init", 1, 0, 9999, 0},
	{"netd", "init", 1, 0, 0, 0},
	{"dhcp6c", "init", 1, 0, 0, 0},
	{"", "", 0, 0, 0, 0},
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
	{"recovery", "init", 1, 0, 0, 0},
	{"sdcard", "vold", 0, 0, 0, 0},
	{"vold", "vold", 0, 0, 0, 0},
	{"adbd", "init", 1, 0, 0, 0},
	{"", "", 0, 0, 0, 0},
};

static struct root_record sys_ptrace_table[] = {
	{"debuggerd", "init", 1, 0, 0, 0},
	{"debuggerd", "debuggerd", 0, 0, 0, 0},
	{"query_task_fd", "sh", 0, 0, 0, 0},
	{"dumpstate", "init", 1, 0, 0, 0},
	{"vold", "init", 1, 0, 0, 0},
	{"main", "init", 1, 0, 0, 0},
	{"busybox", "sh", 0, 0, 0, 0},
	{"ps", "sh", 0, 0, 0, 0},
	{"", "", 0, 0, 0, 0},
};

static struct root_record setpcap_table[] = {
	{"", "", 0, 0, 0, 0},
};

static struct root_record set_uid_table[] = {
	{"zygote", "init", 1, 0, 9999, 0},
	{"main", "init", 1, 0, 9999, 0},
	{"", "", 0, 0, 0, 0},
};

static int is_stat_read_emmc;
static int is_stat_has_path;
static char miscdata_path[32];
static struct root_record write_record;
static struct root_stat get_stat;
static struct get_struct get_task;
static struct kobject *rootrecorder_kobj;


static void find_miscdata(struct block_device *bdev, void *arg)
{
	struct disk_part_iter piter;
	struct hd_struct *part = bdev->bd_part;
	struct gendisk *disk = bdev->bd_disk;

	/*pr_debug_once("find_miscdata:partno=%d, partname=%s\n", part->partno,
		part->info->volname);*/
	if (disk != NULL) {
		disk_part_iter_init(&piter, disk, 0);
		while ((part = disk_part_iter_next(&piter))) {
			/*pr_debug_once("iterator:partno=%d, partname=%s\n",
				part->partno, part->info->volname);*/
			if (part->info->volname && (0 == strncmp("miscdata",
				part->info->volname, 8))) {
				/*pr_debug_once("found miscdata: devpath=%s, devname=%s\n",
					kobject_get_path(&(part->__dev.kobj), GFP_KERNEL),
					part->__dev.kobj.name);*/
				*(char **)arg = (char *)(part->__dev.kobj.name);
				break;
			}
		}
		disk_part_iter_exit(&piter);
	}
}

static int inspect_root_capability(struct root_record *table,
					struct task_struct *task)
{
	int i = 0;
	int flag;
	
	/* table[i].process_name[0] ?= '\0'*/
	for (i = 0; table[i].process_name[0]; i++) {
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

/*
 * equal to return 0
 */
static int root_stat_cmp(struct root_record *in_emmc,
					struct root_record *in_ram)
{
	if (!strncmp(in_emmc->process_name, in_ram->process_name,
			TASK_COMM_LEN) && !strncmp(in_emmc->parent_name,
			in_ram->parent_name, TASK_COMM_LEN) &&
			in_emmc->is_pgl == in_ram->is_pgl &&
			in_emmc->uid == in_ram->uid &&
			in_emmc->euid == in_ram->euid &&
			in_emmc->sid == in_ram->sid &&
			in_emmc->cap == in_ram->cap) {
		return 0;
	}
	return 1;
}

static void write_root_stat_emmc(void)
{
	int errno, i;
	struct file *filp;
	struct root_stat temp_stat;
	struct root_record emmc_record;
	mm_segment_t old_fs;
	char *miscdata_name;

	if(!is_stat_has_path) {
		miscdata_name = NULL;
		iterate_bdevs(find_miscdata, &miscdata_name);
		if (!miscdata_name) {
			pr_debug_once("[root_recorder] write_root_stat_emmc(), no miscdata dev.\n");
			goto err_file_path;
		}
		snprintf(miscdata_path, sizeof(miscdata_path), "/%s/%s/%s", "dev", "block", miscdata_name);
		is_stat_has_path = 1;
	}

	old_fs = get_fs();
	set_fs(KERNEL_DS);
	filp = filp_open(miscdata_path, O_RDWR, 0);
	if (IS_ERR_OR_NULL(filp)) {
		pr_debug_once("[root_recorder] open misc errno=%ld\n", (long)filp);
		goto err_file_open;
	}


	if (filp->f_op && filp->f_op->llseek) {
		errno = filp->f_op->llseek(filp, ROOT_OFFSET, SEEK_SET);
		if (errno < 0) {
			pr_debug_once("[root_recorder] write_root_stat_emmc llseek errno=%d\n", errno);
			goto err_file_op;
		}
	} else {
		goto err_file_op;
	}

	if (filp->f_op && filp->f_op->read) {
		errno = filp->f_op->read(filp, (char *)&temp_stat, sizeof(temp_stat), &filp->f_pos);
		if (errno != sizeof(temp_stat)) {
			pr_debug_once("[root_recorder] write_root_stat_emmc reademmc.errno=%d\n", errno);
			goto err_file_op;
		}
	} else {
		goto err_file_op;
	}

	if ((temp_stat.magic == ROOT_MAGIC) && (temp_stat.root_flag == 1)) {
		if (temp_stat.count >= ROOT_COUNT_LIMIT) {
			pr_debug_once("[root_recorder] write_root_stat_emmc root count limit.\n");
			goto err_file_op;
		}
		errno = filp->f_op->llseek(filp, ROOT_OFFSET+sizeof(temp_stat), SEEK_SET);
		if (errno < 0) {
			pr_debug_once("[root_recorder] write_root_stat_emmc llseek errno=%d\n", errno);
			goto err_file_op;
		}
		for (i=0; i<temp_stat.count; i++) {
			errno = filp->f_op->read(filp, (char *)&emmc_record, sizeof(emmc_record), &filp->f_pos);
			if (errno != sizeof(emmc_record)) {
				pr_debug_once("[root_recorder] write_root_stat_emmc reademmc.errno=%d\n", errno);
				goto err_file_op;
			}
			if(!root_stat_cmp(&emmc_record, &write_record)) {
				break;
			}
		}
		if (i != temp_stat.count) {
			pr_debug_once("[root_recorder] write_root_stat_emmc recorder exist %d-%d\n", i, temp_stat.count);
			goto err_file_op;
		}

		errno = filp->f_op->llseek(filp, ROOT_OFFSET+sizeof(temp_stat)+temp_stat.count*sizeof(emmc_record), SEEK_SET);
		if (errno < 0) {
			pr_debug_once("[root_recorder] llseek errno=%d\n", errno);
			goto err_file_op;
		}

		temp_stat.count += 1;

	} else {

		errno = filp->f_op->llseek(filp, ROOT_OFFSET+sizeof(temp_stat), SEEK_SET);
		if (errno < 0) {
			pr_debug_once("[root_recorder] llseek errno=%d\n", errno);
			goto err_file_op;
		}

		temp_stat.magic = ROOT_MAGIC;
		temp_stat.root_flag = 1;
		temp_stat.count = 1;
	}


	if (filp->f_op && filp->f_op->write) {
		errno = filp->f_op->write(filp, (const char *)&write_record,
			sizeof(write_record), &filp->f_pos);
		if (errno != sizeof(write_record)) {
			pr_debug_once("[root_recorder] writeemmc.errno=%d\n", errno);
			goto err_file_op;
		}
		pr_debug_once("[root_recorder] rootflag writed emmc.\n");
	} else {
		goto err_file_op;
	}


	errno = filp->f_op->llseek(filp, ROOT_OFFSET, SEEK_SET);
	if (errno < 0) {
		pr_debug_once("[root_recorder] llseek errno=%d\n", errno);
		goto err_file_op;
	}

	errno = filp->f_op->write(filp, (const char *)&temp_stat,
		sizeof(temp_stat), &filp->f_pos);
	if (errno != sizeof(temp_stat)) {
		pr_debug_once("[root_recorder] writeemmc.errno=%d\n", errno);
		goto err_file_op;
	}

err_file_op:
	filp_close(filp, NULL);
err_file_open:
	set_fs(old_fs);
err_file_path:
	return;
}


static void do_write(struct work_struct *work)
{
	struct write_struct *write_task;
	write_task = container_of(work, struct write_struct, work);
	write_task->write_emmc_func();
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
	write_record.is_pgl = (task_pgrp_vnr(current) == current->tgid ? 1 : 0);
	write_record.uid = task_uid(current);
	write_record.euid = task_euid(current);
	write_record.sid = task_session_vnr(current);
	write_record.cap = cap;
	strncpy(write_record.process_name, current->group_leader->comm, TASK_COMM_LEN);
	strncpy(write_record.parent_name, current->real_parent->group_leader->comm, TASK_COMM_LEN);

	write_task->write_emmc_func = write_root_stat_emmc;
	INIT_WORK(&write_task->work, do_write);
	schedule_work(&write_task->work);
}

static void do_get_rootrecorder_work(struct work_struct *work)
{
	int errno;
	struct file *filp;
	mm_segment_t old_fs;
	char *miscdata_name;

	if(!is_stat_has_path) {
		miscdata_name = NULL;
		iterate_bdevs(find_miscdata, &miscdata_name);
		if (!miscdata_name) {
			pr_debug_once("[root_recorder] do_get_rootrecorder_work(), no miscdata dev.\n");
			goto err_file_path;
		}
		snprintf(miscdata_path, sizeof(miscdata_path), "/%s/%s/%s", "dev", "block", miscdata_name);
		is_stat_has_path = 1;
	}

	old_fs = get_fs();
	set_fs(KERNEL_DS);
	filp = filp_open(miscdata_path, O_RDONLY, 0);
	if (IS_ERR_OR_NULL(filp)) {
		pr_debug_once("[root_recorder] do_get_rootrecorder_work(), open misc errno=%ld\n", (long)filp);
		goto err_file_open;
	}

	if (filp->f_op && filp->f_op->llseek) {
		errno = filp->f_op->llseek(filp, ROOT_OFFSET, SEEK_SET);
		if (errno < 0) {
			pr_debug_once("[root_recorder] do_get_rootrecorder_work(), llseek errno=%d\n", errno);
			goto err_file_op;
		}
	} else {
		goto err_file_op;
	}

	if (filp->f_op && filp->f_op->read) {
		errno = filp->f_op->read(filp, (char *)&get_stat, sizeof(get_stat), &filp->f_pos);
		if (errno != sizeof(get_stat)) {
			pr_debug_once("[root_recorder] do_get_rootrecorder_work(), reademmc.errno=%d\n", errno);
			goto err_file_op;
		}
		pr_debug_once("[root_recorder] rootflag read emmc.\n");
	} else {
		goto err_file_op;
	}

err_file_op:
	filp_close(filp, NULL);

err_file_open:
	set_fs(old_fs);
	complete_all(&get_task.comp);

err_file_path:

	return;
}


static ssize_t get_rootrecorder(struct device *dev, struct device_attribute *attr, char *buf)
{
	int ret;

	if (is_stat_read_emmc) {
		pr_debug_once("[root_recorder] rootflag has be read.\n");
		goto out;
	}

	memset(&get_stat, 0x0, sizeof(get_stat));

	init_completion(&get_task.comp);
	schedule_work(&get_task.work);

	ret = wait_for_completion_interruptible_timeout(&get_task.comp, msecs_to_jiffies(1000));
	if (ret <= 0) {
		pr_debug_once("[root_recorder] read error, read timeout\n");
	}

	if ((get_stat.magic == ROOT_MAGIC) && (get_stat.root_flag == 1)) {
		is_stat_read_emmc = 1;
		pr_debug_once("[root_recorder] read magic=%x\n",get_stat.magic);
		pr_debug_once("[root_recorder] read rootflag=%d\n",get_stat.root_flag);
		/*pr_debug_once("[root_recorder] read processname=%s, parentname=%s, PGL=%d, uid=%d, euid=%d, sid=%d, cap=%d\n", 
				get_stat.process_name, get_stat.parent_name,  get_stat.is_pgl, get_stat.uid, get_stat.euid,\
				get_stat.sid, get_stat.cap);*/
	}
out:
	memcpy(buf, &get_stat, sizeof(get_stat));
	ret = sizeof(get_stat);

	return ret;
}


static DEVICE_ATTR(rootrecorder, 0444, get_rootrecorder, NULL);

static struct attribute *root_recorder_fs_attrs[] = {
	&dev_attr_rootrecorder.attr,
	NULL,
};

static struct attribute_group root_recorder_attrs_group = {
	.attrs = (struct attribute **)root_recorder_fs_attrs,
};



int __init root_recorder_init(void)
{
	int ret;

	rootrecorder_kobj = kobject_create_and_add("root_recorder", NULL);
	if (!rootrecorder_kobj) {
		pr_debug_once("[root_recorder] kobject_create_and_add() failed\n");
		return -ENOMEM;
	}

	ret = sysfs_create_group(rootrecorder_kobj, &root_recorder_attrs_group);
	if (ret) {
		pr_debug_once("[root_recorder] sysfs_create_group() failed\n");
		kobject_put(rootrecorder_kobj);
		return ret;

	}

	is_stat_has_path = 0;
	is_stat_read_emmc = 0;
	memset(&get_stat, 0x0, sizeof(get_stat));
	memset(&write_record, 0x0, sizeof(write_record));
	INIT_WORK(&get_task.work, do_get_rootrecorder_work);

	return ret;
}

void __exit root_recorder_exit(void)
{
	sysfs_remove_group(rootrecorder_kobj, &root_recorder_attrs_group);
}


module_init(root_recorder_init);
module_exit(root_recorder_exit);
