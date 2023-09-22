/* sec_bsp.c
 * SeungJun Lee <roy.lee@samsung.com>
 * Copyright (C) 2014 Samsung Electronics
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
#include <linux/sec_class.h>
#include <soc/qcom/boot_stats.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/sec_bsp.h>

struct boot_event {
	unsigned int type;
	const char *string;
	unsigned int time;
	unsigned int ktime;
};

static struct boot_event boot_initcall[] = {
	{0,"early",},
	{0,"core",},
	{0,"postcore",},
	{0,"arch",},
	{0,"subsys",},
	{0,"fs",},
	{0,"device",},
	{0,"late",},
	{0,NULL,}
};

enum boot_events_type {
	SYSTEM_START_UEFI,
	SYSTEM_START_LINUXLOADER,
	SYSTEM_START_LINUX,
	SYSTEM_START_INIT_PROCESS,
	PLATFORM_START_PRELOAD,
	PLATFORM_END_PRELOAD,
	PLATFORM_START_INIT_AND_LOOP,
	PLATFORM_START_PACKAGEMANAGERSERVICE,
	PLATFORM_END_PACKAGEMANAGERSERVICE,
	PLATFORM_START_NETWORK,
	PLATFORM_END_NETWORK,
	PLATFORM_END_INIT_AND_LOOP,
	PLATFORM_PERFORMENABLESCREEN,
	PLATFORM_ENABLE_SCREEN,
	PLATFORM_BOOT_COMPLETE,
	PLATFORM_VOICE_SVC,
	PLATFORM_DATA_SVC,
	PLATFORM_PHONEAPP_ONCREATE,
	RIL_UNSOL_RIL_CONNECTED,
	RIL_SETRADIOPOWER_ON,
	RIL_SETUICCSUBSCRIPTION,
	RIL_SIM_RECORDSLOADED,
	RIL_RUIM_RECORDSLOADED,
	RIL_SETUPDATA_RECORDSLOADED,
	RIL_SETUPDATACALL,
	RIL_RESPONSE_SETUPDATACALL,
	RIL_DATA_CONNECTION_ATTACHED,
	RIL_DCT_IMSI_READY,
	RIL_COMPLETE_CONNECTION,
	RIL_CS_REG,
	RIL_GPRS_ATTACH,
};

static struct boot_event boot_events[] = {
	{SYSTEM_START_UEFI,"Uefi start",0,0},
	{SYSTEM_START_LINUXLOADER,"Linux loader start",0,0},
	{SYSTEM_START_LINUX,"Linux start",0,0},
	{SYSTEM_START_INIT_PROCESS,"!@Boot: start init process",0,0},
	{PLATFORM_START_PRELOAD,"!@Boot: Begin of preload()",0,0},
	{PLATFORM_END_PRELOAD,"!@Boot: End of preload()",0,0},
	{PLATFORM_START_INIT_AND_LOOP,"!@Boot: Entered the Android system server!",0,0},
	{PLATFORM_START_PACKAGEMANAGERSERVICE,"!@Boot: Start PackageManagerService",0,0},
	{PLATFORM_END_PACKAGEMANAGERSERVICE,"!@Boot: End PackageManagerService",0,0},
	{PLATFORM_START_NETWORK,"!@Boot_DEBUG: start networkManagement",0,0},
	{PLATFORM_END_NETWORK,"!@Boot_DEBUG: end networkManagement",0,0},
	{PLATFORM_END_INIT_AND_LOOP,"!@Boot: Loop forever",0,0},
	{PLATFORM_PERFORMENABLESCREEN,"!@Boot: performEnableScreen",0,0},
	{PLATFORM_ENABLE_SCREEN,"!@Boot: Enabling Screen!",0,0},
	{PLATFORM_BOOT_COMPLETE,"!@Boot: bootcomplete",0,0},
	{PLATFORM_VOICE_SVC,"!@Boot: Voice SVC is acquired",0,0},
	{PLATFORM_DATA_SVC,"!@Boot: Data SVC is acquired",0,0},
	{PLATFORM_PHONEAPP_ONCREATE,"!@Boot_SVC : PhoneApp OnCrate",0,0},
	{RIL_UNSOL_RIL_CONNECTED,"!@Boot_SVC : RIL_UNSOL_RIL_CONNECTED",0,0},
	{RIL_SETRADIOPOWER_ON,"!@Boot_SVC : setRadioPower on",0,0},
	{RIL_SETUICCSUBSCRIPTION,"!@Boot_SVC : setUiccSubscription",0,0},
	{RIL_SIM_RECORDSLOADED,"!@Boot_SVC : SIM onAllRecordsLoaded",0,0},
	{RIL_RUIM_RECORDSLOADED,"!@Boot_SVC : RUIM onAllRecordsLoaded",0,0},
	{RIL_SETUPDATA_RECORDSLOADED,"!@Boot_SVC : SetupDataRecordsLoaded",0,0},
	{RIL_SETUPDATACALL,"!@Boot_SVC : setupDataCall",0,0},
	{RIL_RESPONSE_SETUPDATACALL,"!@Boot_SVC : Response setupDataCall",0,0},
	{RIL_DATA_CONNECTION_ATTACHED,"!@Boot_SVC : onDataConnectionAttached",0,0},
	{RIL_DCT_IMSI_READY,"!@Boot_SVC : IMSI Ready",0,0},
	{RIL_COMPLETE_CONNECTION,"!@Boot_SVC : completeConnection",0,0},
	{RIL_CS_REG,"!@Boot_SVC : CS Registered",0,0},
	{RIL_GPRS_ATTACH,"!@Boot_SVC : GPRS Attached",0,0},
	{0,NULL,0,0},
};


LIST_HEAD(device_init_time_list);

static int sec_boot_stat_proc_show(struct seq_file *m, void *v)
{
	unsigned int i = 0;
	unsigned int delta = 0;
	struct list_head *tmp = NULL;
	struct device_init_time_entry *entry;


	seq_printf(m,"boot event                                   " \
			    "     time     ktime    delta\n");
	seq_printf(m,"-----------------------------------------" \
				"---------------------------------\n");

	while(boot_events[i].string != NULL)
	{
		seq_printf(m,"%-45s : %6d    %6d    %6d\n",boot_events[i].string,
				boot_events[i].time*1000/32768, boot_events[i].ktime, delta);
		delta = boot_events[i+1].time*1000/32768 - \
			boot_events[i].time*1000/32768;

		if(i == PLATFORM_BOOT_COMPLETE)
			seq_printf(m,"\n");

		i = i + 1;
	}

	seq_printf(m,"-----------------------------------------" \
				"---------------------------------\n");
	seq_printf(m,"kernel extra info\n\n");
	
	i = 0;
	while(boot_initcall[i].string != NULL)
	{
		seq_printf(m,"%-45s : %6d    %6d    %6d\n",boot_initcall[i].string,
				boot_initcall[i].time*1000/32768, boot_initcall[i].ktime, delta);
		delta = boot_initcall[i+1].ktime - boot_initcall[i].ktime;

		i = i + 1;
	}

	seq_printf(m,"\ndevice init time over %d ms\n\n",
				DEVICE_INIT_TIME_100MS/1000);

	list_for_each(tmp, &device_init_time_list) {
		entry = list_entry(tmp, struct device_init_time_entry, next);
		seq_printf(m,"%-20s : %lld usces\n",entry->buf, entry->duration); 
	}
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

void sec_bootstat_add_initcall(const char *s)
{
	int i = 0;
	unsigned long long t = 0;

	while(boot_initcall[i].string != NULL)
	{
		if(strcmp(s, boot_initcall[i].string) == 0)
		{
			t = local_clock();
			do_div(t, 1000000);
			boot_initcall[i].ktime = (unsigned int)t;
			break;
		}
		i = i + 1;
	}
}

void sec_boot_stat_add(const char * c)
{
	int i;
	unsigned long long t = 0;

	i = 0;
	while(boot_events[i].string != NULL)
	{
		if(strcmp(c, boot_events[i].string) == 0)
		{
			if (boot_events[i].time == 0) {
                boot_events[i].time = get_boot_stat_time();
				t = local_clock();
				do_div(t, 1000000);
				boot_events[i].ktime = (unsigned int)t;
			}
			break;
		}
		i = i + 1;
	}
}

static struct device *sec_bsp_dev;

static ssize_t store_boot_stat(struct device *dev,
				struct device_attribute *attr, const char *buf, size_t count)
{
	unsigned long long t = 0;

	if(!strncmp(buf,"!@Boot: start init process",26)) {
		boot_events[SYSTEM_START_INIT_PROCESS].time = get_boot_stat_time();
		t = local_clock();
		do_div(t, 1000000);
		boot_events[SYSTEM_START_INIT_PROCESS].ktime = (unsigned int)t; 
	}

	return count;
}

struct suspend_resume_event {
	unsigned int type;
	unsigned int state;
	const char *string;
	unsigned int cnt;
	unsigned int time;
	unsigned int ktime;
};

static unsigned int suspend_resume_cnt = 0;
enum suspend_resume_type {
	TYPE_SUSPEND,
	TYPE_RESUME,
};
enum suspend_resume_state {
	SUSPEND_EVENT_1,
	SUSPEND_EVENT_2,
	RESUME_EVENT_1,
	RESUME_EVENT_2,
};

static struct suspend_resume_event suspend_resume_event[] = {
	{TYPE_SUSPEND,SUSPEND_EVENT_1,"Syncing FS+",0,0,0},
	{TYPE_SUSPEND,SUSPEND_EVENT_2,"Syncing FS-",0,0,0},
	{TYPE_RESUME,RESUME_EVENT_1,"Freeze User Process+",0,0,0},
	{TYPE_RESUME,RESUME_EVENT_2,"Freeze User Process-",0,0,0},
	{TYPE_SUSPEND,SUSPEND_EVENT_1,"Freeze Remaining+",0,0,0},
	{TYPE_SUSPEND,SUSPEND_EVENT_1,"Freeze Remaining-",0,0,0},
	{TYPE_SUSPEND,SUSPEND_EVENT_1,"Suspending console",0,0,0},
	{0,0,NULL,0,0,0},
};

static int sec_suspend_resume_proc_show(struct seq_file *m, void *v)
{
	unsigned int i;

	i = 0;

	seq_printf(m,"Suspend Resume Progress(Count)                        " \
			    "     time     ktime    \n");
	seq_printf(m,"-----------------------------------------" \
				"---------------------------------\n");
	while(suspend_resume_event[i].string != NULL)
	{
		seq_printf(m,"%-45s(%d) : %6d    %6d    \n",suspend_resume_event[i].string,
				suspend_resume_event[i].cnt,
				suspend_resume_event[i].time*1000/32768, suspend_resume_event[i].ktime);

		i = i + 1;
	}

	return 0;
}

static int sec_suspend_resume_proc_open(struct inode *inode, struct file *file)
{
    return single_open(file, sec_suspend_resume_proc_show, NULL);
}

static const struct file_operations sec_suspend_resume_proc_fops = {
	.open    = sec_suspend_resume_proc_open,
	.read    = seq_read,
	.llseek  = seq_lseek,
	.release = single_release,
};

void sec_suspend_resume_add(const char * c)
{
	int i;
	unsigned long long t = 0;

	i = 0;

	while(suspend_resume_event[i].string != NULL)
	{
		if(strcmp(c, suspend_resume_event[i].string) == 0)
		{
			if(strcmp(c, "Suspending console") == 0) {
				suspend_resume_cnt = (suspend_resume_cnt + 1)%1000;
			}
			suspend_resume_event[i].cnt = suspend_resume_cnt;
     		suspend_resume_event[i].time = get_boot_stat_time();
				t = local_clock();
				do_div(t, 1000000);
				suspend_resume_event[i].ktime = (unsigned int)t;
			break;
		}
		i = i + 1;
	}
}

static ssize_t store_suspend_resume(struct device *dev,
				struct device_attribute *attr, const char *buf, size_t count)
{
	unsigned long long t = 0;
	t++;
	return count;
}

static DEVICE_ATTR(suspend_resume, 0220, NULL, store_suspend_resume);
static DEVICE_ATTR(boot_stat, 0220, NULL, store_boot_stat);

static int __init sec_bsp_init(void)
{
	struct proc_dir_entry *entry;

	entry = proc_create("boot_stat",S_IRUGO, NULL,
							&sec_boot_stat_proc_fops);
	if (!entry)
		return -ENOMEM;

	boot_events[SYSTEM_START_UEFI].time = bs_uefi_start;
	boot_events[SYSTEM_START_LINUXLOADER].time = bs_linuxloader_start;
	boot_events[SYSTEM_START_LINUX].time = bs_linux_start; 

	sec_bsp_dev = device_create(sec_class, NULL, 0, NULL, "bsp");
	if (IS_ERR(sec_bsp_dev))
		pr_err("%s:Failed to create devce\n",__func__);

	if (device_create_file(sec_bsp_dev, &dev_attr_boot_stat) < 0)
		pr_err("%s: Failed to create device file\n",__func__);

	/* Power State Logging */
	entry = proc_create("suspend_resume",S_IRUGO, NULL,
							&sec_suspend_resume_proc_fops);
	if (!entry)
		return -ENOMEM;

	if (device_create_file(sec_bsp_dev, &dev_attr_suspend_resume) < 0)
		pr_err("%s: Failed to create device file1\n",__func__);


	return 0;
}


module_init(sec_bsp_init);
