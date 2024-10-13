/* sec_bsp.c
 *
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
#include <linux/smp.h>
#include <linux/sched.h>
#include <soc/sprd/sec_bsp.h>
#include <linux/io.h>

struct boot_event {
	unsigned int type;
	const char *string;
	unsigned int time;
};

extern struct class *sec_class;

enum boot_events_type {
	SYSTEM_START_INIT_PROCESS,
    PLATFORM_START_PRELOAD,
    PLATFORM_END_PRELOAD,
	PLATFORM_START_INIT_AND_LOOP,
	PLATFORM_START_PACKAGEMANAGERSERVICE,
	PLATFORM_END_PACKAGEMANAGERSERVICE,
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
	RIL_SETUPDATACALL
};

static struct boot_event boot_events[] = {
	{SYSTEM_START_INIT_PROCESS,"!@Boot: start init process",0},
	{PLATFORM_START_PRELOAD,"!@Boot: Begin of preload()",0},
	{PLATFORM_END_PRELOAD,"!@Boot: End of preload()",0},
	{PLATFORM_START_INIT_AND_LOOP,"!@Boot: Entered the Android system server!",0},
	{PLATFORM_START_PACKAGEMANAGERSERVICE,"!@Boot: Start PackageManagerService",0},
	{PLATFORM_END_PACKAGEMANAGERSERVICE,"!@Boot: End PackageManagerService",0},
	{PLATFORM_END_INIT_AND_LOOP,"!@Boot: Loop forever",0},
	{PLATFORM_PERFORMENABLESCREEN,"!@Boot: performEnableScreen",0},
	{PLATFORM_ENABLE_SCREEN,"!@Boot: Enabling Screen!",0},
	{PLATFORM_BOOT_COMPLETE,"!@Boot: bootcomplete",0},
	{PLATFORM_VOICE_SVC,"!@Boot: Voice SVC is acquired",0},
	{PLATFORM_DATA_SVC,"!@Boot: Data SVC is acquired",0},
	{PLATFORM_PHONEAPP_ONCREATE,"!@Boot_SVC : PhoneApp OnCrate",0},
	{RIL_UNSOL_RIL_CONNECTED,"!@Boot_SVC : RIL_UNSOL_RIL_CONNECTED",0},
	{RIL_SETRADIOPOWER_ON,"!@Boot_SVC : setRadioPower on",0},
	{RIL_SETUICCSUBSCRIPTION,"!@Boot_SVC : setUiccSubscription",0},
    {RIL_SIM_RECORDSLOADED,"!@Boot_SVC : SIM onAllRecordsLoaded",0},
    {RIL_RUIM_RECORDSLOADED,"!@Boot_SVC : RUIM onAllRecordsLoaded",0},
	{RIL_SETUPDATACALL,"!@Boot_SVC : setupDataCall",0},
	{0,NULL,0},
};

enum bl_boot_event_type {
	SBOOT_START,
	LCD_DISPLAY,
	LOAD_CP_START,
	LOAD_CP_DONE,
	LOAD_KERNEL_DONE,
	BL_DONE,
};

static struct boot_event bl_boot_event[]={
	{SBOOT_START, "SBOOT : Start sboot", 0},
	{LCD_DISPLAY, "SBOOT : LCD ON", 0},
	{LOAD_CP_START, "SBOOT : Load CP binary", 0},
	{LOAD_CP_DONE, "SBOOT : Loaded CP binary", 0},
	{LOAD_KERNEL_DONE, "SBOOT : Loaded kernel binary", 0},
	{BL_DONE, "SBOOT : Jump to Kernel", 0},
	{0, NULL, 0},
};

static int bl_boot_stat_addr;

static int get_bl_boot_stat(void){
	int i = 0;
	int buf[100];
	void __iomem *addr = ioremap(0x1000, SZ_4K);

	bl_boot_stat_addr = addr + 0xFA0;

	memcpy(buf, bl_boot_stat_addr, 0x60);

	while(bl_boot_event[i].string != NULL){
		bl_boot_event[i].time = buf[i];
		i++;
	}
	
	iounmap(addr);
}

static int sec_boot_stat_proc_show(struct seq_file *m, void *v)
{
	unsigned int i = 0, j= 0;
	int delta = 0;

	seq_printf(m,"boot event                                     time (ms)" \
				"       delta\n");
	seq_printf(m,"--------------------------------------------------------" \
				"------------\n");

	while(bl_boot_event[j].string != NULL){
		if(bl_boot_event[j].type != LOAD_CP_START){
	                seq_printf(m,"%-50s : %5u    %5d\n",bl_boot_event[j].string,
                                bl_boot_event[j].time, delta);
			delta = 0;
		}

		if(j < BL_DONE)
	                delta += bl_boot_event[j+1].time - \
       		                bl_boot_event[j].time;

		j++;
	}
	
	delta += (boot_events[i].time - bl_boot_event[j-1].time);

	while(boot_events[i].string != NULL)
	{
		seq_printf(m,"%-50s : %5u    %5d\n",boot_events[i].string,
				boot_events[i].time, delta);

		delta = boot_events[i+1].time - \
			boot_events[i].time;
		i = i + 1;
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

void sec_boot_stat_add(const char * c)
{
	int i;
	unsigned long long t;
	
	i = 0;
	while(boot_events[i].string != NULL)
	{
		if(strcmp(c, boot_events[i].string) == 0)
		{
			t = local_clock();
			do_div(t, 1000000);
			boot_events[i].time = (unsigned int)t + bl_boot_event[BL_DONE].time;
			break;
		}
		i = i + 1;
	}
}

static struct device *sec_bsp_dev;

static ssize_t store_boot_stat(struct device *dev,
				struct device_attribute *attr, const char *buf, size_t count)
{
	unsigned long long t;

	if(!strncmp(buf,"!@Boot: start init process",26)) {
		t = local_clock();
		do_div(t, 1000000);
		boot_events[SYSTEM_START_INIT_PROCESS].time = (unsigned int)t + bl_boot_event[BL_DONE].time;
	}

	return count;
}

static DEVICE_ATTR(boot_stat, S_IWUSR | S_IWGRP, NULL, store_boot_stat);

static int __init sec_bsp_init(void)
{
	struct proc_dir_entry *entry;

	entry = proc_create("boot_stat",S_IRUGO, NULL,
							&sec_boot_stat_proc_fops);
	if (!entry)
		return -ENOMEM;

	
	BUG_ON(!sec_class);
	sec_bsp_dev = device_create(sec_class, NULL, 0, NULL, "bsp");
	BUG_ON(!sec_bsp_dev);
	if (IS_ERR(sec_bsp_dev))
		pr_err("%s:Failed to create devce\n",__func__);

	if (device_create_file(sec_bsp_dev, &dev_attr_boot_stat) < 0)
		pr_err("%s: Failed to create device file\n",__func__);

	get_bl_boot_stat();

	return 0;
}

module_init(sec_bsp_init);
