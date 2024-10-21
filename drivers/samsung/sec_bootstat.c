/* sec_bootstat.c
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
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 */

#define pr_fmt(fmt) KBUILD_MODNAME ": " fmt

#include <linux/seq_file.h>
#include <linux/fs.h>
#include <linux/proc_fs.h>
#include <linux/device.h>
#include <linux/module.h>
#include <linux/smp.h>
#include <linux/sched.h>
#include <linux/sched/clock.h>
#include <linux/sec_class.h>
#include <linux/slab.h>
#include <linux/thermal.h>
#include <linux/cpufreq.h>
#include <clocksource/arm_arch_timer.h>

/******************************BACKPORTED FROM LINUX 6.1******************************/
/*                                                                                   */
/* https://elixir.bootlin.com/linux/v6.1.43/source/drivers/base/arch_topology.c#L642 */
/*                                                                                   */
/*************************************************************************************/
int cpu_cluster_id[NR_CPUS];

#define topology_cluster_id(cpu)	(cpu_cluster_id[cpu])

static int get_cpu_for_node(struct device_node *node)
{
	struct device_node *cpu_node;
	int cpu;

	cpu_node = of_parse_phandle(node, "cpu", 0);
	if (!cpu_node)
		return -1;

	cpu = of_cpu_node_to_id(cpu_node);

	of_node_put(cpu_node);
	return cpu;
}

static int parse_core(struct device_node *core, int cluster_id, int core_id)
{
	bool leaf = true;
	int cpu;

	cpu = get_cpu_for_node(core);
	if (cpu >= 0) {
		if (!leaf) {
			pr_err("%pOF: Core has both threads and CPU\n",
			       core);
			return -EINVAL;
		}
		cpu_cluster_id[cpu] = cluster_id;
	} else if (leaf && cpu != -ENODEV) {
		pr_err("%pOF: Can't get CPU for leaf core\n", core);
		return -EINVAL;
	}

	return 0;
}

static int parse_cluster(struct device_node *cluster, int cluster_id, int depth)
{
	char name[20];
	bool leaf = true;
	bool has_cores = false;
	struct device_node *c;
	int core_id = 0;
	int i, ret;

	i = 0;
	do {
		snprintf(name, sizeof(name), "cluster%d", i);
		c = of_get_child_by_name(cluster, name);
		if (c) {
			leaf = false;
			ret = parse_cluster(c, i, depth + 1);
			if (depth > 0)
				pr_warn("Topology for clusters of clusters not yet supported\n");
			of_node_put(c);
			if (ret != 0)
				return ret;
		}
		i++;
	} while (c);

	i = 0;
	do {
		snprintf(name, sizeof(name), "core%d", i);
		c = of_get_child_by_name(cluster, name);
		if (c) {
			has_cores = true;

			if (depth == 0) {
				pr_err("%pOF: cpu-map children should be clusters\n",
				       c);
				of_node_put(c);
				return -EINVAL;
			}

			if (leaf) {
				ret = parse_core(c, cluster_id, core_id++);
			} else {
				pr_err("%pOF: Non-leaf cluster with core %s\n",
				       cluster, name);
				ret = -EINVAL;
			}

			of_node_put(c);
			if (ret != 0)
				return ret;
		}
		i++;
	} while (c);

	if (leaf && !has_cores)
		pr_warn("%pOF: empty cluster\n", cluster);

	return 0;
}

static int parse_dt_topology(void)
{
	struct device_node *cn, *map;
	int ret = 0;

	cn = of_find_node_by_path("/cpus");
	if (!cn) {
		pr_err("No CPU information found in DT\n");
		return 0;
	}

	map = of_get_child_by_name(cn, "cpu-map");
	if (!map)
		goto out;

	ret = parse_cluster(map, -1, 0);

	of_node_put(map);
out:
	of_node_put(cn);
	return ret;
}
/*************************************************************************************/

#define FREQ_LEN 4

#define MAX_CLUSTERS NR_CPUS
#define MAX_CLUSTER_OUTPUT ((FREQ_LEN + 1) * MAX_CLUSTERS + 1)

#define MAX_THERMAL_ZONES 10
#define THERMAL_DNAME_LENGTH 5

#define MAX_LENGTH_OF_BOOTING_LOG 90
#define MAX_LENGTH_OF_SYSTEMSERVER_LOG 90

#define MAX_EVENTS_EBS 100
#define DELAY_TIME_EBS 10000

int __read_mostly boot_time_bl1;
module_param(boot_time_bl1, int, 0440);

int __read_mostly boot_time_bl2;
module_param(boot_time_bl2, int, 0440);

int __read_mostly boot_time_bl3;
module_param(boot_time_bl3, int, 0440);

extern void register_hook_bootstat(void (*func)(const char *buf));

struct boot_event {
	const char *string;
	bool bootcomplete;
	unsigned int time;
	int freq[MAX_CLUSTERS];
	int online;
	int temp[MAX_THERMAL_ZONES];
	int order;
};

struct tz_info {
	char name[THERMAL_NAME_LENGTH];
	char display_name[THERMAL_DNAME_LENGTH];
};

struct enhanced_boot_time {
	struct list_head next;
	char buf[MAX_LENGTH_OF_BOOTING_LOG];
	unsigned int time;
	int freq[MAX_CLUSTERS];
	int online;
};

struct systemserver_init_time_entry {
	struct list_head next;
	char buf[MAX_LENGTH_OF_SYSTEMSERVER_LOG];
};

static struct boot_event boot_events[] = {
	{"!@Boot: start init process",},
	{"!@Boot: Begin of preload()",},
	{"!@Boot: End of preload()",},
	{"!@Boot: Entered the Android system server!",},
	{"!@Boot: Start PackageManagerService",},
	{"!@Boot: End PackageManagerService",},
	{"!@Boot: Loop forever",},
	{"!@Boot: performEnableScreen",},
	{"!@Boot: Enabling Screen!",},
	{"!@Boot: bootcomplete", true,},
	{"!@Boot: Voice SVC is acquired",},
	{"!@Boot: Data SVC is acquired",},
	{"!@Boot_SVC : PhoneApp OnCrate",},
	{"!@Boot_DEBUG: start networkManagement",},
	{"!@Boot_DEBUG: end networkManagement",},
	{"!@Boot_SVC : RIL_UNSOL_RIL_CONNECTED",},
	{"!@Boot_SVC : setRadioPower on",},
	{"!@Boot_SVC : setUiccSubscription",},
	{"!@Boot_SVC : SIM onAllRecordsLoaded",},
	{"!@Boot_SVC : RUIM onAllRecordsLoaded",},
	{"!@Boot_SVC : setupDataCall",},
	{"!@Boot_SVC : Response setupDataCall",},
	{"!@Boot_SVC : onDataConnectionAttached",},
	{"!@Boot_SVC : IMSI Ready",},
	{"!@Boot_SVC : completeConnection",},
	{"!@Boot_DEBUG: finishUserUnlockedCompleted",},
	{"!@Boot: setIconVisibility: ims_volte: [SHOW]",},
	{"!@Boot_DEBUG: Launcher.onCreate()",},
	{"!@Boot_DEBUG: Launcher.onResume()",},
	{"!@Boot_DEBUG: Launcher.LoaderTask.run() start",},
	{"!@Boot_DEBUG: Launcher - FinishFirstBind",},
#ifdef CONFIG_SEC_FACTORY
	{"!@Boot: Factory Process [Boot Completed]",},
#endif /* CONFIG_SEC_FACTORY */
};

static u32 arch_timer_start;

static bool bootcompleted;
static bool ebs_finished;
unsigned long long boot_complete_time;
static int events_ebs;

LIST_HEAD(systemserver_init_time_list);
LIST_HEAD(enhanced_boot_time_list);

static int num_clusters;
static int num_cpus_cluster[MAX_CLUSTERS];

static int num_thermal_zones;
static struct tz_info thermal_zones[MAX_THERMAL_ZONES];

static void sec_bootstat_get_cpuinfo(int *freq, int *online)
{
	int cpu;

	*online = cpumask_bits(cpu_online_mask)[0];
	for_each_online_cpu(cpu)
		freq[topology_cluster_id(cpu)] = cpufreq_get(cpu);
}

static void sec_bootstat_get_thermal(int *temp)
{
	int i, ret;
	struct thermal_zone_device *tzd;

	for (i = 0; i < num_thermal_zones; i++) {
		tzd = thermal_zone_get_zone_by_name(thermal_zones[i].name);
		if (IS_ERR(tzd)) {
			pr_err("can't find thermal zone %s\n", thermal_zones[i].name);
			continue;
		}
		ret = thermal_zone_get_temp(tzd, &temp[i]);
		if (ret)
			pr_err("failed to get temperature for %s (%d)\n", tzd->type, ret);
	}
}

static DEFINE_RAW_SPINLOCK(ebs_list_lock);
void sec_enhanced_boot_stat_record(const char *buf)
{
	unsigned long long t = 0;
	struct enhanced_boot_time *entry;
	unsigned long flags;

	entry = kmalloc(sizeof(*entry), GFP_KERNEL);
	if (!entry)
		return;
	strscpy(entry->buf, buf, MAX_LENGTH_OF_BOOTING_LOG);
	t = local_clock();
	do_div(t, 1000000);
	entry->time = (unsigned int)t;
	sec_bootstat_get_cpuinfo(entry->freq, &entry->online);

	raw_spin_lock_irqsave(&ebs_list_lock, flags);
	list_add(&entry->next, &enhanced_boot_time_list);
	events_ebs++;

	raw_spin_unlock_irqrestore(&ebs_list_lock, flags);
}

static int prev;
void sec_bootstat_add(const char *buf)
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
	if (!ebs_finished && events_ebs < MAX_EVENTS_EBS) {
		if (!strncmp(buf, "!@Boot_EBS: ", 12)) {
			sec_enhanced_boot_stat_record(buf + 12);
			return;
		} else if (!strncmp(buf, "!@Boot_EBS_", 11)) {
			sec_enhanced_boot_stat_record(buf);
			return;
		}
	}

	if (!bootcompleted && !strncmp(buf, "!@Boot_SystemServer: ", 21)) {
		struct systemserver_init_time_entry *entry;
		entry = kmalloc(sizeof(*entry), GFP_KERNEL);
		if (!entry)
			return;
		strscpy(entry->buf, buf + 21, MAX_LENGTH_OF_SYSTEMSERVER_LOG);
		list_add(&entry->next, &systemserver_init_time_list);
		return;
	}

	for (i = 0; i < ARRAY_SIZE(boot_events); i++) {
		if (!strncmp(buf, boot_events[i].string, strlen(boot_events[i].string))) {
			if (boot_events[i].time == 0) {
				boot_events[prev].order = i;
				prev = i;

				t = local_clock();
				do_div(t, 1000000);
				boot_events[i].time = (unsigned int)t;
				sec_bootstat_get_cpuinfo(boot_events[i].freq, &boot_events[i].online);
				sec_bootstat_get_thermal(boot_events[i].temp);
			}
			// careful check bootcomplete message index 9
			if (i == 9) {
				bootcompleted = true;
				boot_complete_time = local_clock();
				do_div(boot_complete_time, 1000000);
			}
			break;
		}
	}
}

void print_format(struct boot_event *data, struct seq_file *m, int index, int delta)
{
	int i, cpu, offset;
	char out[MAX_CLUSTER_OUTPUT];

	seq_printf(m, "%-50s %6u %6u %6d ", data[index].string,
		data[index].time + arch_timer_start, data[index].time, delta);

	// print frequency of clusters
	offset = 0;
	for (i = 0; i < num_clusters; ++i)
		offset += sprintf(out + offset, "%-*d ", FREQ_LEN, data[index].freq[i] / 1000);
	seq_printf(m, "%-*s", (int)sizeof("Frequency ") - 1, out);

	// print online cpus by cluster
	offset = 0;
	for (i = 0; i < num_clusters; i++) {
		offset += sprintf(out + offset, "[");
		for_each_possible_cpu(cpu)
			if (topology_cluster_id(cpu) == i)
				offset += sprintf(out + offset, "%d", (data[index].online >> cpu) & 1);
		offset += sprintf(out + offset, "]");
	}
	offset += sprintf(out + offset, " ");
	seq_printf(m, "%-*s", (int)sizeof("Online Mask ") - 1, out);

	// print temperature of thermal zones
	for (i = 0; i < num_thermal_zones; i++)
		seq_printf(m, "[%4d]", data[index].temp[i] / 1000);
	seq_putc(m, '\n');
}

static int sec_boot_stat_proc_show(struct seq_file *m, void *v)
{
	int offset;
	char out[MAX_CLUSTER_OUTPUT];
	size_t i = 0;
	unsigned int last_time = 0;
	struct systemserver_init_time_entry *systemserver_entry;

	seq_printf(m, "%71s %-*s%-*s %s\n", "", (FREQ_LEN + 1) * num_clusters, "Frequency ",
			2 * num_clusters + nr_cpu_ids, "Online Mask", "Temperature");
	seq_printf(m, "%-50s %6s %6s %6s ", "Boot Events", "time", "ktime", "delta");

	offset = 0;
	for (i = 0; i < num_clusters; i++)
		offset += sprintf(out + offset, "f_c%lu ", i);
	seq_printf(m, "%-*s", (int)sizeof("Frequency ") - 1, out);

	offset = 0;
	for (i = 0; i < num_clusters; i++)
		offset += sprintf(out + offset, "C%lu%*s", i, num_cpus_cluster[i], "");
	offset += sprintf(out + offset, " ");
	seq_printf(m, "%-*s", (int)sizeof("Online Mask ") - 1, out);

	for (i = 0; i < num_thermal_zones; i++)
		seq_printf(m, "[%4s]", thermal_zones[i].display_name);
	seq_puts(m, "\n");
	seq_puts(m, "--------------------------------------------------------------------------------------------------------------\n");
	seq_puts(m, "BOOTLOADER - KERNEL\n");
	seq_puts(m, "--------------------------------------------------------------------------------------------------------------\n");
	seq_printf(m, "%-50s %6u %6u %6u\n", "MCT is initialized in bl2", 0, 0, 0);
	seq_printf(m, "%-50s %6u %6u %6u\n", "start kernel timer", arch_timer_start, 0, arch_timer_start);
	seq_puts(m, "--------------------------------------------------------------------------------------------------------------\n");
	seq_puts(m, "FRAMEWORK\n");
	seq_puts(m, "--------------------------------------------------------------------------------------------------------------\n");
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
	} while (i > 0 && i < ARRAY_SIZE(boot_events));

	seq_puts(m, "--------------------------------------------------------------------------------------------------------------\n");
	seq_puts(m, "SystemServer services that took long time\n\n");
	list_for_each_entry (systemserver_entry, &systemserver_init_time_list, next)
		seq_printf(m, "%s\n", systemserver_entry->buf);

	return 0;
}

static int sec_enhanced_boot_stat_proc_show(struct seq_file *m, void *v)
{
	int i;
	unsigned int last_time = 0;
	struct enhanced_boot_time *entry;

	seq_printf(m, "%-90s %6s %6s %6s ", "Boot Events", "time", "ktime", "delta");

	for (i = 0; i < num_clusters; i++)
		seq_printf(m, "f_c%d ", (int)i);

	seq_putc(m, '\n');
	seq_puts(m, "-------------------------------------------------------------------------------------------------------------------------\n");
	seq_puts(m, "BOOTLOADER - KERNEL\n");
	seq_puts(m, "-------------------------------------------------------------------------------------------------------------------------\n");
	seq_printf(m, "%-90s %6u %6u %6u\n", "!@Boot_EBS_B: MCT_is_initialized", 0, 0, 0);
	seq_printf(m, "%-90s %6u %6u %6u\n", "!@Boot_EBS_B: boot_time_bl1", boot_time_bl1, 0, boot_time_bl1);
	seq_printf(m, "%-90s %6u %6u %6u\n", "!@Boot_EBS_B: boot_time_bl2", boot_time_bl2, 0,
			boot_time_bl2 - boot_time_bl1);
	seq_printf(m, "%-90s %6u %6u %6u\n", "!@Boot_EBS_B: boot_time_bl3", boot_time_bl3, 0,
			boot_time_bl3 - boot_time_bl2);
	seq_printf(m, "%-90s %6u %6u %6u\n", "!@Boot_EBS_B: start_kernel_timer", arch_timer_start, 0,
			arch_timer_start - boot_time_bl3);
	seq_puts(m, "-------------------------------------------------------------------------------------------------------------------------\n");
	seq_puts(m, "FRAMEWORK\n");
	seq_puts(m, "-------------------------------------------------------------------------------------------------------------------------\n");
	list_for_each_entry_reverse (entry, &enhanced_boot_time_list, next) {
		seq_printf(m, "%-90s %6u %6u ", entry->buf, entry->time + arch_timer_start, entry->time);
		if (entry->buf[0] == '!') {
			seq_printf(m, "%6u ", entry->time - last_time);
			last_time = entry->time;
		} else {
			seq_printf(m, "%6s ", "");
		}
		for (i = 0; i < num_clusters; i++)
			seq_printf(m, "%4d ", entry->freq[i] / 1000);
		seq_putc(m, '\n');
	}
	return 0;
}

static int sec_boot_stat_proc_open(struct inode *inode, struct file *file)
{
	return single_open(file, sec_boot_stat_proc_show, NULL);
}

static int sec_enhanced_boot_stat_proc_open(struct inode *inode, struct file *file)
{
	return single_open(file, sec_enhanced_boot_stat_proc_show, NULL);
}

static const struct proc_ops sec_boot_stat_proc_fops = {
	.proc_open    = sec_boot_stat_proc_open,
	.proc_read    = seq_read,
	.proc_lseek  = seq_lseek,
	.proc_release = single_release,
};

static const struct proc_ops sec_enhanced_boot_stat_proc_fops = {
	.proc_open    = sec_enhanced_boot_stat_proc_open,
	.proc_read    = seq_read,
	.proc_lseek  = seq_lseek,
	.proc_release = single_release,
};

static ssize_t store_boot_stat(struct device *dev, struct device_attribute *attr,
			const char *buf, size_t count)
{
	sec_bootstat_add(buf);
	return count;
}

static DEVICE_ATTR(boot_stat, 0220, NULL, store_boot_stat);

static int get_thermal_zones(void)
{
	int ret = 0;
	int i;
	struct device_node *np, *tz;
	const char *zname, *dname;

	np = of_find_node_by_path("/sec-bootstat/thermal-zones");
	if (!np) {
		pr_err("No thermal zones description\n");
		return 0;
	}

	for_each_available_child_of_node(np, tz) {
		if (of_property_read_string(tz, "zone-name", &zname) < 0) {
			pr_err("invalid or no zone-name for thermal zone: %s\n", tz->name);
			continue;
		} else {
			strscpy(thermal_zones[ret].name, zname, THERMAL_NAME_LENGTH);
		}

		if (of_property_read_string(tz, "display-name", &dname) < 0) {
			pr_err("invalid or no display-name for thermal zone: %s\n", tz->name);
			strscpy(thermal_zones[ret].display_name, tz->name, THERMAL_DNAME_LENGTH);
		} else {
			strscpy(thermal_zones[ret].display_name, dname, THERMAL_DNAME_LENGTH);
		}

		++ret;
		if (ret == MAX_THERMAL_ZONES) {
			pr_info("reached maximum thermal zones\n");
			break;
		}
	}
	of_node_put(np);

	pr_info("num_thermal_zones = %d\n", ret);
	for (i = 0; i < ret; i++)
		pr_info("thermal zone %d: %s\n", i, thermal_zones[i].name);

	return ret;
}

static u32 get_arch_timer_start(void)
{
	struct device_node *np;
	int ret = 0;
	u32 clock_frequency;
	u64 ts_msec;
	u64 arch_timer_start;

	clock_frequency = arch_timer_get_cntfrq();
	if (!clock_frequency) {
		pr_info("arch_timer_get_cntfrq failed... looking into DT for clock-frequency\n");
		np = of_find_node_by_path("/timer");
		if (!np) {
			pr_err("no timer in DT\n");
			return -ENODEV;
		}

		ret = of_property_read_u32(np, "clock-frequency", &clock_frequency);
		of_node_put(np);

		if (ret < 0) {
			pr_err("no clock-frequency found in DT\n");
			return -EINVAL;
		}
	}

	arch_timer_start = arch_timer_read_counter();
	do_div(arch_timer_start, (clock_frequency / 1000));
	ts_msec = local_clock();
	do_div(ts_msec, 1000000);
	arch_timer_start -= ts_msec;

	return (u32)arch_timer_start;
}

static int get_num_clusters(void)
{
	int cpu;
	int cluster;
	int num_clusters = -1;

	for_each_possible_cpu(cpu) {
		cluster = topology_cluster_id(cpu);
		num_cpus_cluster[cluster]++;
		num_clusters = (num_clusters < cluster ? cluster : num_clusters);
	}

	if (num_clusters == -1)
		return -EINVAL;

	++num_clusters; // +1 of max cluster_id

	pr_info("num_clusters = %d\n", num_clusters);

	return num_clusters;
}

static int __init_or_module sec_bootstat_init(void)
{
	struct proc_dir_entry *entry;
	struct proc_dir_entry *enhanced_entry;
	struct device *dev;

	parse_dt_topology();

	num_clusters = get_num_clusters();
	if (num_clusters <= 0) {
		pr_err("no cluster found\n");
		return -EINVAL;
	}

	arch_timer_start = get_arch_timer_start();
	if (arch_timer_start < 0) {
		pr_err("invalid arch timer start\n");
		return -EINVAL;
	}

	num_thermal_zones = get_thermal_zones();
	if (!num_thermal_zones)
		pr_err("no thermal zone found\n");

	// proc
	entry = proc_create("boot_stat", S_IRUGO, NULL, &sec_boot_stat_proc_fops);
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

	register_hook_bootstat(sec_bootstat_add);

	return 0;
}

module_init(sec_bootstat_init);

MODULE_DESCRIPTION("Samsung boot-stat driver");
MODULE_LICENSE("GPL v2");
