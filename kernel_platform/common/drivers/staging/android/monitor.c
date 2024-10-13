/*
 * drivers/staging/android/monitor.c
 *
 *  Copyright (C) 2022 Samsung Electronics
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#include <linux/kobject.h>
#include <linux/syscalls.h>
#include <linux/monitor.h>
#include <linux/delay.h>
#include <linux/cred.h>

#define MAX_BUF_SIZE	3072
#define MAX_THREAD 25
#define MAX_COLLECT 25


struct kobject *monitor_kobj;

struct iaft_collect {
	int threhold;
	int recovery;
	int sleep_time;

	unsigned long start_jiffy;
	int uid ;//set by fw
	int uid_count ;
	int tid ; 
	int tid_count;
	int code ;
	int code_count ;
	int data[10];
	int data_count[10];
};

struct iaft_throttle {
	int threhold;
	int recovery;
	int sleep_time;

	int iaft_uid ;
	int iaft_tid ;
	int iaft_code ;

	unsigned long throttle_jiffy;
	bool throttle;
	int binder_count ;
};

static int system_server;

int threhold = 200;
int recovery = 50;
int sleep_time = 3;


struct iaft_collect collect_uid[MAX_COLLECT];
int collect_uid_idx = 0;

struct iaft_throttle throttle_tid[MAX_THREAD];
bool collect_flag = false;
bool throttle_flag = false;
bool only_collect = false;
int collect_binder_idx(int from_pid, int from_tid, unsigned int code) {
	int i;
	for (i = 0; i < MAX_COLLECT; i++) {
		if (collect_uid[i].uid <= 1000) {
			break;
		}
		if (collect_uid[i].tid > 0) {
			if (collect_uid[i].code >= 0) {
				if (collect_uid[i].tid == from_tid) {
					return -1;
				} else {
					continue;
				}
			} else {
				if (collect_uid[i].tid == from_tid) {
					return i;
				} else if (time_before(jiffies, collect_uid[i].start_jiffy + 3 * HZ))
					return -1;
			}
		} else { 
			if (collect_uid[i].start_jiffy == 0) {
				collect_uid[i].start_jiffy = jiffies;
			}
			if (collect_uid[i].uid == current_uid().val)
				return i;
			else if (time_before(jiffies, collect_uid[i].start_jiffy + 2 * HZ)){
				return -1;
			}
		}
	}
	printk(KERN_INFO "iaft,collect stop %d \n",i);
	collect_flag = false;
	return -1;
}

void collect_binder(int id, int from_pid, int from_tid, unsigned int code) {
	int i;

	collect_uid[id].uid_count++;

	if (collect_uid[id].tid == -1) {
		for (i = 0; i < 10; i++) {
			if (collect_uid[id].data[i] == -1) {
				collect_uid[id].data[i] = from_tid;
				collect_uid[id].data_count[i] = 1;
				break;
			} else if (collect_uid[id].data[i] == from_tid) {
				collect_uid[id].data_count[i]++;
				break;
			}
		} 
	
		if (time_after(jiffies, collect_uid[id].start_jiffy + HZ)) {
			for (i = 0; i < 10; i++) {
				if (collect_uid[id].tid_count < collect_uid[id].data_count[i]) {
					collect_uid[id].tid_count = collect_uid[id].data_count[i];
					collect_uid[id].tid = collect_uid[id].data[i];
				}
				collect_uid[id].data[i] = -1;
				collect_uid[id].data_count[i] = 0;
			}
		}
	
	} else if (collect_uid[id].tid == from_tid && collect_uid[id].code == -1) {
		for (i = 0; i < 10; i++){
			if (collect_uid[id].data[i] == -1) {
				collect_uid[id].data[i] = code;
				collect_uid[id].data_count[i] = 1;
				break;
			} else if (collect_uid[id].data[i] == code) {
				collect_uid[id].data_count[i]++;
				break;
			}
		} 
	
		if (time_after(jiffies, collect_uid[id].start_jiffy + 2 * HZ)) {
			for (i = 0; i < 10; i++) {
				if (collect_uid[id].code_count < collect_uid[id].data_count[i]) {
					collect_uid[id].code_count = collect_uid[id].data_count[i];
					collect_uid[id].code = collect_uid[id].data[i];
				}
				collect_uid[id].data[i] = -1;
				collect_uid[id].data_count[i] = 0;
			}
		}
	
	}

}
int throttle_binder_idx(int from_pid, int from_tid, unsigned int code){
	int i;
	throttle_flag = false;
	for (i = 0; i < MAX_THREAD; i++) {
		if (throttle_tid[i].iaft_tid != -1 && time_after(jiffies, throttle_tid[i].throttle_jiffy + 10 * HZ)) {
			throttle_tid[i].iaft_uid = throttle_tid[i].iaft_tid = throttle_tid[i].iaft_code = -1; 
			throttle_tid[i].binder_count = 0;
			printk(KERN_INFO "iaft binder died? force clear throttle id:%d tid:%d  code:%d ", i,from_tid, code);
			continue;
		}
		if (throttle_tid[i].iaft_tid == -1) {
			continue;
		}
		throttle_flag = true;
		if (throttle_tid[i].iaft_tid == from_tid && throttle_tid[i].iaft_code == code) {
			return i;
		}
	}
	return -1;
}

void throttle_binder(int id, int from_pid, int from_tid, unsigned int code) {
	
	if (throttle_tid[id].binder_count == 0)
		throttle_tid[id].throttle_jiffy = jiffies;
	
	throttle_tid[id].binder_count++;

	if (throttle_tid[id].throttle) {
		msleep(throttle_tid[id].sleep_time);
		if (throttle_tid[id].binder_count >= throttle_tid[id].recovery) {
			if (time_after(jiffies, throttle_tid[id].throttle_jiffy + HZ)) {
				throttle_tid[id].throttle = false;
				printk(KERN_INFO "iaft binder became normal,pid=%d tid=%d code=%d\n", from_pid, throttle_tid[id].iaft_tid,throttle_tid[id].iaft_code);
			}
			throttle_tid[id].binder_count = 0;
		}
	
	} else {
		if (throttle_tid[id].binder_count >= throttle_tid[id].threhold) {
			if (time_before(jiffies, throttle_tid[id].throttle_jiffy + HZ)) {
				throttle_tid[id].throttle = true;
				printk(KERN_INFO "iaft binder_throttle ,pid=%d tid=%d code=%d\n", from_pid, throttle_tid[id].iaft_tid,throttle_tid[id].iaft_code);
			}
			throttle_tid[id].binder_count = 0;
		}
	}

}

void throttle_info_setting(void)
{
	int i,j;
	for (i = 0; i < MAX_COLLECT; i++){
		if (collect_uid[i].code_count > collect_uid[i].threhold) {
			for (j = 0; j < MAX_THREAD; j++){
				if (throttle_tid[j].iaft_tid == collect_uid[i].tid){
					break;
				}
			}
			if (j >= MAX_THREAD) {
				for (j = 0; j < MAX_THREAD; j++) {
					if (throttle_tid[j].iaft_tid == -1) {
						break;
					}
				}
			}
			if (j < MAX_THREAD) {
				throttle_tid[j].threhold = collect_uid[i].threhold;
				throttle_tid[j].recovery = collect_uid[i].recovery;
				throttle_tid[j].sleep_time = collect_uid[i].sleep_time;
				throttle_tid[j].iaft_uid = collect_uid[i].uid;
				throttle_tid[j].iaft_tid = collect_uid[i].tid;
				throttle_tid[j].iaft_code = collect_uid[i].code;
				throttle_tid[j].binder_count = 0;
				throttle_tid[j].throttle_jiffy = jiffies;
				throttle_flag = true;
			}
			printk(KERN_INFO "iaft start control :%d %d %d %d %d\n", collect_uid[i].uid,collect_uid[i].tid, collect_uid[i].code,collect_uid[i].code_count, collect_uid[i].threhold);
		}
	}

}

int binder_monitor(int from_pid, int from_tid, unsigned int code, int target_pid)
{
	int i = -1;
	if(system_server != target_pid )
		return 0;

	if (collect_flag) {
		i = collect_binder_idx(from_pid, from_tid, code);
		if (i >= 0)
			collect_binder(i,from_pid, from_tid, code);
		if (!collect_flag && !only_collect) {
			throttle_info_setting();
		}

	}

	if (throttle_flag) {
		i = throttle_binder_idx(from_pid, from_tid, code);
		if (i >= 0)
			throttle_binder(i,from_pid, from_tid, code);
	}
	return 0;
}

void clear_collect_info(void) {
	int i,j;
	for (i = 0; i < MAX_COLLECT; i++) {
		collect_uid[i].uid = collect_uid[i].tid = collect_uid[i].code = -1;
		collect_uid[i].uid_count = collect_uid[i].tid_count = collect_uid[i].code_count = 0;
		collect_uid[i].start_jiffy = 0;
		for (j = 0; j < 10; j++){
			collect_uid[i].data[j] = -1;
			collect_uid[i].data_count[j] = 0;
		}
	}
	collect_uid_idx = 0;
	printk(KERN_INFO "iaft collect_info clear \n");

}

void clear_throttle_info(void) {
	int i;
	for (i = 0; i < MAX_THREAD; i++) {
		throttle_tid[i].iaft_uid = throttle_tid[i].iaft_tid = throttle_tid[i].iaft_code = -1;
		throttle_tid[i].binder_count = 0;
		throttle_tid[i].throttle_jiffy = 0;
		throttle_tid[i].throttle = false;
	}
	printk(KERN_INFO "iaft throttle_info clear \n");
}

bool iaft_can_monitor(void)
{
	if (!capable(CAP_SYS_ADMIN) && current_uid().val != 1000) {
		printk(KERN_INFO "iaft Permission deny uid:%d euid:%d pid:%d\n",current_uid().val,current_euid().val,current->pid);
		return false;
	}
	return true;
}

static ssize_t sleep_time_show(struct kobject *kobj,
				struct kobj_attribute *attr,
				char *buf)
{

	return snprintf(buf, MAX_BUF_SIZE, "%d\n", sleep_time);
}

static ssize_t sleep_time_store(struct kobject *kobj,
				struct kobj_attribute *attr,
				const char *buf, size_t n)
{

	int val;

	if (sscanf(buf, "%d", &val) != 1)
		return -EINVAL;
	sleep_time = val;
	return n;
}

binder_monitor_attr(sleep_time);


static ssize_t enable_uid_show(struct kobject *kobj,
				struct kobj_attribute *attr,
				char *buf)
{
	ssize_t len = 0;
	int i;
	if (only_collect) {
		for (i = 0; i < MAX_COLLECT; i++) {
			if (collect_uid[i].uid >0 && collect_uid[i].code_count > collect_uid[i].threhold)
				len += snprintf(buf + len, MAX_BUF_SIZE - len,"%d>,<",collect_uid[i].uid);
		}
	} else {
		for (i = 0; i < MAX_THREAD; i++) {
			if (throttle_tid[i].iaft_uid > 0)
				len += snprintf(buf + len, MAX_BUF_SIZE - len,"%d>,<",throttle_tid[i].iaft_uid);
		}
	}
	len += snprintf(buf + len, MAX_BUF_SIZE - len,"-1>,<\n");

	for (i = 0; i < MAX_COLLECT; i++) {
		len += snprintf(buf + len, MAX_BUF_SIZE - len,
			"iaft: %d %d %d uid:%d uid_c:%d tid:%d tid_c:%d  code:%d code_c:%d \n",collect_uid[i].threhold,collect_uid[i].recovery,collect_uid[i].sleep_time,
			collect_uid[i].uid,collect_uid[i].uid_count,collect_uid[i].tid,collect_uid[i].tid_count,collect_uid[i].code,collect_uid[i].code_count);
	}
	for (i = 0; i < MAX_THREAD; i++) {
		len += snprintf(buf + len, MAX_BUF_SIZE - len,"iaft:%d %d %d %d  %d: %d %d\n",throttle_tid[i].threhold,throttle_tid[i].recovery,throttle_tid[i].sleep_time,
			throttle_tid[i].iaft_uid,throttle_tid[i].iaft_tid,throttle_tid[i].iaft_code,throttle_tid[i].binder_count);
	}
	return len;

}


static ssize_t enable_uid_store(struct kobject *kobj,
				struct kobj_attribute *attr,
				const char *buf, size_t n)
{
	int val;

	if (!iaft_can_monitor())
		return -EACCES;
	if (sscanf(buf, "%d", &val) != 1)
		return -EINVAL;

	if (val == -1) {
		clear_throttle_info();
		clear_collect_info();
		return n;
	} else if (val == 0) {
		clear_collect_info();
		return n;
	}

	if (val > 1000) {
		only_collect = false;
	}else if (val < -1000) {
		val = -1 * val;
		only_collect = true;
	}

	if (val > 1000) {
		collect_uid_idx =  collect_uid_idx % MAX_COLLECT;
		collect_uid[collect_uid_idx].uid = val;
		collect_uid[collect_uid_idx].threhold = threhold;
		collect_uid[collect_uid_idx].recovery = recovery;
		collect_uid[collect_uid_idx].sleep_time = sleep_time;
		collect_uid_idx++;
	} else if (val == 1) {
		if (collect_uid_idx > 0) {
			collect_flag = true;
		}
	}
	printk(KERN_INFO "iaft enable_uid:%d %d\n", val,collect_uid_idx);
	return n;
}

binder_monitor_attr(enable_uid);

static ssize_t enable_code_show(struct kobject *kobj,
				struct kobj_attribute *attr,
				char *buf)
{
	int val = 0;
	if (collect_flag)
		val = 1;

	return snprintf(buf, MAX_BUF_SIZE, "%d\n", val);
}

static ssize_t enable_code_store(struct kobject *kobj,
				struct kobj_attribute *attr,
				const char *buf, size_t n)
{
	int val;

	if (!iaft_can_monitor())
		return -EACCES;
	if (sscanf(buf, "%d", &val) != 1)
		return -EINVAL;
	system_server = val;
	return n;
}

binder_monitor_attr(enable_code);

static ssize_t threhold_show(struct kobject *kobj,
				struct kobj_attribute *attr,
				char *buf)
{
	return snprintf(buf, MAX_BUF_SIZE,"%d\n", threhold);
}

static ssize_t threhold_store(struct kobject *kobj,
				struct kobj_attribute *attr,
				const char *buf, size_t n)
{
	int val;

	if (sscanf(buf, "%d", &val) != 1)
		return -EINVAL;
	threhold = val;
	return n;
}

binder_monitor_attr(threhold);

static ssize_t recovery_show(struct kobject *kobj,
				struct kobj_attribute *attr,
				char *buf)
{
	return snprintf(buf, MAX_BUF_SIZE,"%d\n", recovery);
}

static ssize_t recovery_store(struct kobject *kobj,
				struct kobj_attribute *attr,
				const char *buf, size_t n)
{
	int val;

	if (sscanf(buf, "%d", &val) != 1)
		return -EINVAL;
	recovery = val;
	return n;
}

binder_monitor_attr(recovery);

static struct attribute *g[] = {
	&sleep_time_attr.attr,
	&enable_uid_attr.attr,
	&enable_code_attr.attr,
	&threhold_attr.attr,
	&recovery_attr.attr,
	NULL,
};

static const struct attribute_group attr_group = {
	.attrs = g,
};

static const struct attribute_group *attr_groups[] = {
	&attr_group,
	NULL,
};


static int __init kmonitor_init(void)
{
	int error;

	monitor_kobj = kobject_create_and_add("iaft", power_kobj);
	if (!monitor_kobj)
		return -ENOMEM;

	error = sysfs_create_groups(monitor_kobj, attr_groups);
	if (error)
		return error;

	return 0;
}
static void __exit kmonitor_exit(void)
{
	//
}
module_init(kmonitor_init);
module_exit(kmonitor_exit);

MODULE_LICENSE("GPL");
