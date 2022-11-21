/* sec_bsp.c
 *
 * Copyright (C) 2016 Samsung Electronics
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307 USA
 *
 */

#include <linux/seq_file.h>
#include <linux/fs.h>
#include <linux/proc_fs.h>
#include <linux/device.h>
#include <linux/smp.h>
#include <linux/sched.h>
#include <linux/sched/clock.h>
#include <linux/sec_class.h>
#include <linux/sec_ext.h>
#include <linux/slab.h>
#include <linux/module.h>

struct boot_event {
	const char *string;
	unsigned int time;
	int freq[2];
	int online;
	int temp[4];
	int order;
};

static struct boot_event boot_initcall[] = {
	{"early",},
	{"core",},
	{"postcore",},
	{"arch",},
	{"subsys",},
	{"fs",},
	{"device",},
	{"late",},
};

static struct boot_event boot_events[] = {
	{"!@Boot: start init process", },
	{"!@Boot: Begin of preload()", },
	{"!@Boot: End of preload()", },
	{"!@Boot: Entered the Android system server!", },
	{"!@Boot: Start PackageManagerService", },
	{"!@Boot: End PackageManagerService", },
	{"!@Boot: Loop forever", },
	{"!@Boot: performEnableScreen", },
	{"!@Boot: Enabling Screen!", },
	{"!@Boot: bootcomplete", },
	{"!@Boot: Voice SVC is acquired", },
	{"!@Boot: Data SVC is acquired", },
	{"!@Boot: setIconVisibility: ims_volte: [SHOW]", },
	{"!@Boot_SVC : PhoneApp OnCrate", },
	{"!@Boot_DEBUG: start networkManagement", },
	{"!@Boot_DEBUG: end networkManagement", },
	{"!@Boot_DEBUG: finishUserUnlockedCompleted", },
	{"!@Boot_DEBUG: Launcher.onCreate()", },
	{"!@Boot_DEBUG: Launcher.LoaderTask.run() start", },
	{"!@Boot_DEBUG: Launcher.HomeLoader.bindPageItems()-finish first Bind", },
	{"!@Boot_SVC : RIL_UNSOL_RIL_CONNECTED", },
	{"!@Boot_SVC : setRadioPower on", },
	{"!@Boot_SVC : setUiccSubscription", },
	{"!@Boot_SVC : SIM onAllRecordsLoaded", },
	{"!@Boot_SVC : RUIM onAllRecordsLoaded", },
	{"!@Boot_SVC : setupDataCall", },
	{"!@Boot_SVC : Response setupDataCall", },
	{"!@Boot_SVC : onDataConnectionAttached", },
	{"!@Boot_SVC : IMSI Ready", },
	{"!@Boot_SVC : completeConnection", },
};

#define MAX_LENGTH_OF_BOOTING_LOG 90
#define DELAY_TIME_EBS 10000
#define MAX_EVENTS_EBS 130

struct enhanced_boot_time {
	struct list_head next;
	char buf[MAX_LENGTH_OF_BOOTING_LOG];
	unsigned int time;
	int freq[2];
	int online;
};

static bool bootcompleted = false;
static bool ebs_finished = false;
unsigned long long boot_complete_time = 0;
static int events_ebs = 0;
#ifdef CONFIG_SEC_DEVICE_BOOTSTAT
LIST_HEAD(device_init_time_list);
#endif
static LIST_HEAD(systemserver_init_time_list);
LIST_HEAD(enhanced_boot_time_list);

static int mct_start_kernel;
int board_rev;
int bootingtime_preloader;
int bootingtime_lk;

static int board_rev_setup(char *str)
{
	int n;
	if (!get_option(&str, &n))
		return 0;
	board_rev = n;

	return 1;
}
__setup("androidboot.revision=", board_rev_setup);

static int __init boottime_preloader_setup(char *str)
{
	int time = 0;

	time = memparse(str, NULL);

	bootingtime_preloader = time;

	return 0;
}
early_param("bootprof.pl_t", boottime_preloader_setup);

static int __init boottime_lk_setup(char *str)
{
	int time = 0;

	time = memparse(str, NULL);

	bootingtime_lk = time;

	return 0;
}
early_param("bootprof.lk_t", boottime_lk_setup);

void sec_boot_stat_set_bl_boot_time(int pl_t, int lk_t)
{
	bootingtime_preloader = pl_t;
	bootingtime_lk = lk_t;
	mct_start_kernel = bootingtime_preloader + bootingtime_lk;	

	pr_info("bootingtime_preloader=%d, bootingtime_lk=%d, mct_start_kernel=%d\n", bootingtime_preloader, bootingtime_lk, mct_start_kernel);
}

void sec_boot_stat_get_start_kernel(void)
{
	mct_start_kernel = bootingtime_preloader + bootingtime_lk;
	pr_info("mct_start_kernel=%d\n", mct_start_kernel);
}

void sec_boot_stat_add_initcall(const char *s)
{
	size_t i = 0;
	unsigned long long t = 0;

	for (i = 0; i < ARRAY_SIZE(boot_initcall); i++) {
		if (!strcmp(s, boot_initcall[i].string)) {
			t = local_clock();
			do_div(t, 1000000);
			boot_initcall[i].time = (unsigned int)t;
			break;
		}
	}
}

void sec_enhanced_boot_stat_record(const char *buf)
{
	unsigned long long t = 0;
	struct enhanced_boot_time *entry;
	entry = kmalloc(sizeof(*entry), GFP_KERNEL);
	if (!entry)
		return;
	strncpy(entry->buf, buf, MAX_LENGTH_OF_BOOTING_LOG);
	entry->buf[MAX_LENGTH_OF_BOOTING_LOG-1] = '\0';
	t = local_clock();
	do_div(t, 1000000);
	entry->time = (unsigned int)t;
	list_add(&entry->next, &enhanced_boot_time_list);
	events_ebs++;
}

static int prev;
void sec_boot_stat_add(const char *c)
{
	size_t i = 0;
	unsigned long long t = 0;

	if (bootcompleted && !ebs_finished) {
		t = local_clock();
		do_div(t, 1000000);
		if ((t - boot_complete_time) >= DELAY_TIME_EBS)
			ebs_finished = true;
	}

	// Collect Boot_EBS from java side
	if (!ebs_finished && events_ebs < MAX_EVENTS_EBS){
		if (!strncmp(c, "!@Boot_EBS: ", 12)) {
			sec_enhanced_boot_stat_record(c + 12);
			return;
		}
		else if (!strncmp(c, "!@Boot_EBS_", 11)) {
			sec_enhanced_boot_stat_record(c);
			return;
		}
	}

	if (!bootcompleted && !strncmp(c, "!@Boot_SystemServer: ", 21)) {
		struct systemserver_init_time_entry *entry;
		entry = kmalloc(sizeof(struct systemserver_init_time_entry), GFP_KERNEL);
		if (!entry)
			return;
		strncpy(entry->buf, c + 21, MAX_LENGTH_OF_SYSTEMSERVER_LOG);
		entry->buf[MAX_LENGTH_OF_SYSTEMSERVER_LOG - 1] = '\0';
		list_add(&entry->next, &systemserver_init_time_list);
		return;
	}

	for (i = 0; i < ARRAY_SIZE(boot_events); i++) {
		if (!strcmp(c, boot_events[i].string)) {
			if (boot_events[i].time == 0) {
				boot_events[prev].order = i;
				prev = i;

				t = local_clock();
				do_div(t, 1000000);
				boot_events[i].time = (unsigned int)t;
			}

			// careful check bootcomplete message index 9
			if(i == 9) {
				bootcompleted = true;
				boot_complete_time = local_clock();
				do_div(boot_complete_time, 1000000);
			}
			break;
		}
	}
}

void print_format(struct boot_event *data,
	struct seq_file *m, int index, int delta)
{
	seq_printf(m, "%-50s %6u %6u %6d %4d %4d L%d%d%d%d B%d%d%d%d %2d %2d %2d %2d\n",
		data[index].string, data[index].time + mct_start_kernel,
		data[index].time, delta,
		data[index].freq[0]/1000,
		data[index].freq[1]/1000,
		data[index].online&1,
		(data[index].online>>1)&1,
		(data[index].online>>2)&1,
		(data[index].online>>3)&1,
		(data[index].online>>4)&1,
		(data[index].online>>5)&1,
		(data[index].online>>6)&1,
		(data[index].online>>7)&1,
		data[index].temp[0],
		data[index].temp[1],
		data[index].temp[2],
		data[index].temp[3]
		);
}

static int sec_boot_stat_proc_show(struct seq_file *m, void *v)
{
	size_t i = 0;
	unsigned int last_time = 0;
#ifdef CONFIG_SEC_DEVICE_BOOTSTAT
	struct device_init_time_entry *entry;
#endif
	struct systemserver_init_time_entry *systemserver_entry;

	seq_puts(m, "boot event\t\t\t\t\t     time  ktime  delta f_c0 f_c1 online mask  B  L  G  I\n");
	seq_puts(m, "---------------------------------------------------------------------------------------------------------\n");
	seq_puts(m, "BOOTLOADER - KERNEL\n");
	seq_puts(m, "---------------------------------------------------------------------------------------------------------\n");
	seq_printf(m, "start booting on bootloader \t\t\t   %6u %6u %6u\n", 0, 0, 0);
	seq_printf(m, "start booting on kernel \t\t\t   %6u %6u %6u\n", mct_start_kernel, 0, mct_start_kernel);

	for (i = 0; i < ARRAY_SIZE(boot_initcall); i++) {
		print_format(boot_initcall, m, i, boot_initcall[i].time - last_time);
		last_time = boot_initcall[i].time;
	}

	seq_puts(m, "---------------------------------------------------------------------------------------------------------\n");
	seq_puts(m, "FRAMEWORK\n");
	seq_puts(m, "---------------------------------------------------------------------------------------------------------\n");
	i = 0;
	do {
		if (boot_events[i].time != 0) {
			print_format(boot_events, m, i, boot_events[i].time - last_time);
			last_time = boot_events[i].time;
		}

		if (i != boot_events[i].order)
		i = boot_events[i].order;
		else
			break;
	}
	while (0 < i && i < ARRAY_SIZE(boot_events));
#ifdef CONFIG_SEC_DEVICE_BOOTSTAT
	seq_printf(m, "\ndevice init time over %d ms\n\n",
		DEVICE_INIT_TIME_100MS / 1000);
	list_for_each_entry (entry, &device_init_time_list, next)
		seq_printf(m, "%-20s : %lld usces\n", entry->buf, entry->duration);
#endif
	seq_puts(m, "---------------------------------------------------------------------------------------------------------\n");
	seq_puts(m, "SystemServer services that took long time\n\n");
	list_for_each_entry (systemserver_entry, &systemserver_init_time_list, next)
		seq_printf(m, "%s\n",systemserver_entry->buf);

	return 0;
}

static int sec_boot_stat_proc_open(struct inode *inode, struct file *file)
{
	return single_open(file, sec_boot_stat_proc_show, NULL);
}

static const struct file_operations sec_boot_stat_proc_fops = {
	.open    = sec_boot_stat_proc_open,
	.read    = seq_read,
	.llseek  = seq_lseek,
	.release = single_release,
};

static int sec_enhanced_boot_stat_proc_show(struct seq_file *m, void *v)
{
	size_t i = 0;
	unsigned int last_time = 0;
	struct enhanced_boot_time *entry;

	seq_printf(m, "%-90s %6s %6s %6s\n", "Boot Events", "time", "ktime", "delta");
	seq_puts(m, "---------------------------------------------------------------------------------------------------------------\n");
	seq_puts(m, "BOOTLOADER - KERNEL\n");
	seq_puts(m, "---------------------------------------------------------------------------------------------------------------\n");
	seq_printf(m, "%-90s %6u %6u %6u %4d %4d %4d\n", "!@Boot_EBS_B: boot_time_pl_t", bootingtime_preloader, 0, bootingtime_preloader, 0, 0, 0);
	seq_printf(m, "%-90s %6u %6u %6u %4d %4d %4d\n", "!@Boot_EBS_B: boot_time_lk", bootingtime_lk, 0, bootingtime_lk, 0, 0, 0);
	seq_printf(m, "%-90s %6u %6u %6u %4d %4d %4d\n", "!@Boot_EBS_B: boot_time_kernel", mct_start_kernel, 0, mct_start_kernel, 0, 0, 0);
	seq_printf(m, "%-90s %6u %6u %6u\n", "start booting on bootloader", 0, 0, 0);
	seq_printf(m, "%-90s %6u %6u %6u\n", "start booting on kernel", mct_start_kernel, 0, mct_start_kernel);

	for (i = 0; i < ARRAY_SIZE(boot_initcall); i++) {
		seq_printf(m, "%-90s %6u %6u %6u\n", boot_initcall[i].string, boot_initcall[i].time + mct_start_kernel, boot_initcall[i].time,
				boot_initcall[i].time - last_time);
		last_time = boot_initcall[i].time;
	}

	seq_puts(m, "---------------------------------------------------------------------------------------------------------------\n");
	seq_puts(m, "FRAMEWORK\n");
	seq_puts(m, "---------------------------------------------------------------------------------------------------------------\n");
	list_for_each_entry_reverse (entry, &enhanced_boot_time_list, next){
		if (entry->buf[0] == '!') {
			seq_printf(m, "%-90s %6u %6u %6u\n", entry->buf, entry->time + mct_start_kernel, entry->time, entry->time - last_time);
			last_time = entry->time;
		}
		else {
			seq_printf(m, "%-90s %6u %6u\n", entry->buf, entry->time + mct_start_kernel, entry->time);
		}
	}
	return 0;
}

static int sec_enhanced_boot_stat_proc_open(struct inode *inode, struct file *file)
{
	return single_open(file, sec_enhanced_boot_stat_proc_show, NULL);
}

static const struct file_operations sec_enhanced_boot_stat_proc_fops = {
	.open    = sec_enhanced_boot_stat_proc_open,
	.read    = seq_read,
	.llseek  = seq_lseek,
	.release = single_release,
};

static ssize_t store_boot_stat(struct device *dev,
				struct device_attribute *attr, const char *buf, size_t count)
{
	unsigned long long t = 0;

	// Collect Boot_EBS from native side
	if (!ebs_finished && events_ebs < MAX_EVENTS_EBS) {
		if (!strncmp(buf, "!@Boot_EBS: ", 12)) {
			sec_enhanced_boot_stat_record(buf + 12);
			return count;
		}
		else if (!strncmp(buf, "!@Boot_EBS_", 11)) {
			sec_enhanced_boot_stat_record(buf);
			return count;
		}
	}

	if (!strncmp(buf, "!@Boot: start init process", 26)) {
		t = local_clock();
		do_div(t, 1000000);
		boot_events[0].time = (unsigned int)t;
	}

	return count;
}

static DEVICE_ATTR(boot_stat, 0220, NULL, store_boot_stat);

static int __init sec_bsp_init(void)
{
	struct proc_dir_entry *entry;
	struct proc_dir_entry *enhanced_entry;
	struct device *dev;

	// proc
	entry = proc_create("boot_stat", S_IRUGO, NULL,
				&sec_boot_stat_proc_fops);
	if (!entry)
		return -ENOMEM;

	enhanced_entry = proc_create("enhanced_boot_stat", S_IRUGO, NULL, &sec_enhanced_boot_stat_proc_fops);
	if (!enhanced_entry)
		return -ENOMEM;

	// sysfs
	dev = sec_device_create(NULL, "bsp");
	BUG_ON(!dev);
	if (IS_ERR(dev))
		pr_err("%s:Failed to create devce\n", __func__);

	if (device_create_file(dev, &dev_attr_boot_stat) < 0)
		pr_err("%s: Failed to create device file\n", __func__);

	return 0;
}

module_init(sec_bsp_init);
