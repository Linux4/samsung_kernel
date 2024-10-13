#include <linux/string.h>
#include <linux/device.h>
#include <linux/kobject.h>
#include <linux/slab.h>
#include <linux/cpufreq.h>
#include <linux/uaccess.h>
#include <linux/debugfs.h>
#include <linux/thermal.h>
#include <asm/topology.h>
#include <linux/kthread.h>
#include <linux/sched.h>
#include <soc/samsung/gpu_cooling.h>
#include <linux/of.h>
#include <linux/platform_device.h>
#include <soc/samsung/cal-if.h>
#include <soc/samsung/exynos-devfreq.h>
#include "../../../kernel/sched/ems/ems.h"
#include <linux/cpumask.h>
#include <linux/kernel.h>
#include <soc/samsung/exynos_pm_qos.h>
#include <trace/events/sched.h>
#include <trace/events/ems_debug.h>
#include <soc/samsung/freq-qos-tracer.h>
#include "xperf.h"
#include <linux/exynos-dsufreq.h>

static DEFINE_PER_CPU(struct xperf_cpu, xperf_cpu);
static struct profile_info p_info;

static const char *prefix = "xperf";
static struct kobject *prof_kobj;

static unsigned int pbuf_sz, pbuf_data;
static char *pbuf;

struct cluster_info cls[MAX_CLUSTER];
int cl_cnt;

int get_cl_idx(int cpu)
{
	struct xperf_cpu *xcpu = &per_cpu(xperf_cpu, cpu);
	return xcpu->cl_info->cl_idx;
}

static int get_cpufreq_table_size(struct cpufreq_frequency_table *cursor,
				struct cpufreq_policy *policy)
{
	int ret = 0;
	int table_size = 0;

	cpufreq_for_each_entry(cursor, policy->freq_table)
		table_size++;

	return table_size ? table_size : ret;
}

//---------------------------------------
// thread main
static int profile_thread(void *data)
{
	struct thermal_zone_device *tz;
	struct pid *pgrp;
	struct task_struct *p;

	unsigned int cur_freq[MAX_CLUSTER], max_freq[MAX_CLUSTER];
	unsigned int gpu_util = 0, gpu_clock = 0, mif = 0, dsu = 0;
	unsigned int siop, ocp, total_power;

	int temp_g3d = 0, temp_bat = 0;
	int cpu, cluster, f_idx, i;
	int task_num_cpus[VENDOR_NR_CPUS] = {0,};

	while (p_info.is_running) {
		// cpu temperature
		for_each_cluster(cluster) {
			int temp_cls = 0;
			tz = thermal_zone_get_zone_by_name(tz_name[cluster]);
			if (!IS_ERR(tz)) {
				thermal_zone_get_temp(tz, &temp_cls);
				temp_cls = (temp_cls < 0) ? 0 : temp_cls / 1000;
			}
			pbuf_data += snprintf(pbuf + pbuf_data, pbuf_sz - pbuf_data, "%d ", temp_cls);
		}

		// gpu temperature
		tz = thermal_zone_get_zone_by_name("G3D");
		if (!IS_ERR(tz)) {
			thermal_zone_get_temp(tz, &temp_g3d);
			temp_g3d = (temp_g3d < 0) ? 0 : temp_g3d / 1000;
		}
		pbuf_data += snprintf(pbuf + pbuf_data, pbuf_sz - pbuf_data, "%d ", temp_g3d);

		// siop, ocp
#if IS_ENABLED(CONFIG_EXYNOS_UFCC)
		siop = get_cpufreq_max_limit() / 1000;
		siop = (siop > 4000) ? 0 : siop;
#else
		siop = 0;
#endif

#if IS_ENABLED(CONFIG_EXYNOS_AFM)
		ocp = get_afm_clipped_freq(cls[cl_cnt - 1].last_cpu ) / 1000;
		ocp = (ocp > 4000)? 0 : ocp;
#else
		ocp = 0;
#endif
		// battery
		tz = thermal_zone_get_zone_by_name("battery");
		if (!IS_ERR(tz)) {
			thermal_zone_get_temp(tz, &temp_bat);
			temp_bat = (temp_bat < 0) ? 0 : temp_bat / 1000;
		}
		pbuf_data += snprintf(pbuf + pbuf_data, pbuf_sz - pbuf_data, "%d ", temp_bat);

		// cpufreq
		for_each_cluster_rev(cluster) {
			struct cluster_info *cl_info = &cls[cluster];

			max_freq[cluster] = cpufreq_quick_get_max(cl_info->first_cpu) / 1000;
			cur_freq[cluster] = cpufreq_quick_get(cl_info->first_cpu) / 1000;
			pbuf_data += snprintf(pbuf + pbuf_data, pbuf_sz - pbuf_data, "%d %d ", max_freq[cluster], cur_freq[cluster]);
			if (cluster == (cl_cnt - 1))
				pbuf_data += snprintf(pbuf + pbuf_data, pbuf_sz - pbuf_data, "%d %d ", siop, ocp);
		}

		// dsu
		dsu = dsufreq_get_cur_freq() / 1000;
		pbuf_data += snprintf(pbuf + pbuf_data, pbuf_sz - pbuf_data, "%d ", dsu);

		// mif
		mif = (unsigned int) exynos_devfreq_get_domain_freq(DEVFREQ_MIF) / 1000;

		// gpu util, gpu clock
		gpu_util = gpu_dvfs_get_utilization();
		gpu_clock = gpu_dvfs_get_cur_clock() / 1000;

		pbuf_data += snprintf(pbuf + pbuf_data, pbuf_sz - pbuf_data, "%d %d %d ", mif, gpu_util, gpu_clock);

		// cpu util
		for_each_possible_cpu(cpu)
			pbuf_data += snprintf(pbuf + pbuf_data, pbuf_sz - pbuf_data, "%ld ", ml_cpu_util(cpu));

		// cpu power
		total_power = 0;
		for_each_possible_cpu(cpu) {
			struct xperf_cpu *xcpu = &per_cpu(xperf_cpu, cpu);
			struct cluster_info *cl_info = xcpu->cl_info;
			unsigned int est_power, cal_power;
			unsigned int util_ratio;

			util_ratio = (ml_cpu_util(cpu) * 100) / capacity_cpu_orig(cpu);
			f_idx = et_cur_freq_idx(cpu);
			est_power = cl_info->freqs[f_idx].dyn_power;
			cal_power = util_ratio * est_power / 100;
			total_power += cal_power;
			pbuf_data += snprintf(pbuf + pbuf_data, pbuf_sz - pbuf_data, "%d ", cal_power);
		}
		pbuf_data += snprintf(pbuf + pbuf_data, pbuf_sz - pbuf_data, "%d ", total_power);

		// task multi
		for_each_possible_cpu(cpu)
			task_num_cpus[cpu] = 0;

		pgrp = find_vpid(p_info.pid);
		do_each_pid_thread(pgrp, PIDTYPE_TGID, p) {
			unsigned int util;
			if (READ_ONCE(p->__state) == TASK_RUNNING) {
				util = p->se.avg.util_avg;
				if (util > p_info.util_thd) {
					for (i = 0; i < MAX_TNAMES; i++) {
						if (strlen(p_info.tns[i]) > 1) {
							if (!strncmp(p->comm, p_info.tns[i], strlen(p_info.tns[i]))) {
								pr_info("[%s] util_thd=%d util=%d tid=%d cpu=%d tns[%d]=%s p->comm=%s",
									prefix, p_info.util_thd, util, p->pid, task_cpu(p),
									i, p_info.tns[i], p->comm);
								task_num_cpus[task_cpu(p)] = i + 1; // tnames index + 1
							}
						}
					}
				}
			}
		} while_each_pid_thread(pgrp, PIDTYPE_TGID, p);

		for_each_possible_cpu(cpu)
			pbuf_data += snprintf(pbuf + pbuf_data, pbuf_sz - pbuf_data, "%d ", task_num_cpus[cpu]);

		// tune mode
		pbuf_data += snprintf(pbuf + pbuf_data, pbuf_sz - pbuf_data, "%d %d ", emstune_get_cur_mode(), emstune_get_cur_level());

		pbuf_data -= 1;
		pbuf_data += snprintf(pbuf + pbuf_data, pbuf_sz - pbuf_data, "\n");

		// check buf size
		if (pbuf_data + 256 > pbuf_sz) {
			pr_info("[%s] pbuf_sz: %d + 256 > %d!!", prefix, pbuf_data, pbuf_sz);
			break;
		}
		msleep(p_info.polling_ms);
	}

	return 0;
}

static void parse_tz_name(void)
{
	struct device_node *np, *child;
	const char *tz_info;
	int cnt = 0;

	np = of_find_node_by_path("/xperf/cl_info");
	if (!np) {
		pr_err("%s: failed to find nodes\n", __func__);
		return;
	}

	for_each_child_of_node(np, child) {
		if (!of_property_read_string(child, "tz-name", &tz_info)) {
			strcpy(tz_name[cnt], tz_info);
			pr_info("%s: tz_name %s\n", __func__, tz_name[cnt]);
			cnt++;
		}
	}
}

static int init_cluster_info(void)
{
	struct cpufreq_policy *policy;
	struct cpufreq_frequency_table *cursor = NULL;
	int cpu, idx = 0;
	int table_size;
	int i;

	for_each_cpu(cpu, cpu_possible_mask) {
		struct xperf_cpu *xcpu = &per_cpu(xperf_cpu, cpu);

		policy = cpufreq_cpu_get(cpu);
		if (!policy) {
			pr_err("[%s] %s: failed to get cpufreq policy, cpu%d\n", prefix, __func__, cpu);
			continue;
		}

		xcpu->cpu = cpu;
		if (policy->cpu != cpu) {
			struct xperf_cpu *xcpu_policy = &per_cpu(xperf_cpu, policy->cpu);
			xcpu->cl_info = xcpu_policy->cl_info;
			continue;
		}

		table_size = get_cpufreq_table_size(cursor, policy);
		if (!table_size) {
			pr_err("[%s] %s: there is no table size %d, cpu %d\n",
				prefix, __func__, table_size, policy->cpu);
			continue;
		}

		xcpu->cl_info = &cls[idx];
		xcpu->cl_info->cl_idx = idx;
		xcpu->cl_info->siblings = policy->related_cpus;
		xcpu->cl_info->first_cpu = cpumask_first(xcpu->cl_info->siblings);
		xcpu->cl_info->last_cpu = cpumask_last(xcpu->cl_info->siblings);
		xcpu->cl_info->freq_size = table_size;
		xcpu->cl_info->freqs = kcalloc(table_size, sizeof(struct freq), GFP_KERNEL);

		for (i = 0; i < xcpu->cl_info->freq_size; i++)
			xcpu->cl_info->freqs[i].freq = policy->freq_table[i].frequency;

		idx++;
	}

	cl_cnt = idx;
	parse_tz_name();

	return idx;
}

static void init_cpu_power(void)
{
	u64 f, v, c, p;
	struct device *dev;
	unsigned long f_hz;
	int cnt, i;

	for_each_cluster(cnt) {
		cls[cnt].coeff = p_info.power_coeffs[cnt];
		dev = get_cpu_device(cls[cnt].first_cpu);
		for (i = 0; i < cls[cnt].freq_size; i++) {
			struct dev_pm_opp *opp;
			f_hz = cls[cnt].freqs[i].freq * 1000;
			opp = dev_pm_opp_find_freq_ceil(dev, &f_hz);
			cls[cnt].freqs[i].volt = dev_pm_opp_get_voltage(opp) / 1000;	// uV -> mV
			f = cls[cnt].freqs[i].freq / 1000;	// kHz -> MHz
			v = cls[cnt].freqs[i].volt;
			c = cls[cnt].coeff;
			p = c * v * v * f;
			p = p / 1000000000;
			cls[cnt].freqs[i].dyn_power = p;	// dynamic power
		}
	}
}

static int init_profile_info(struct device_node *dn)
{
	int count;
	int ret = 0;

	count = of_property_count_u32_elems(dn, "power-coeff");
	if (count < 0) {
		pr_err("[%s] %s: fail to get count from device-tree", prefix, __func__);
		ret = count;
		goto out;
	}

	ret = of_property_read_u32_array(dn, "power-coeff", p_info.power_coeffs, count);
	if (ret) {
		pr_err("[%s] %s: fail to parse power-coeff from device-tree", prefix, __func__);
		goto out;
	}

	p_info.task = NULL;
	p_info.polling_ms = DEFAULT_POLLING_MS;
	p_info.pid = DEFAULT_PID;
	p_info.util_thd = DEFAULT_UTIL_THD;
	p_info.is_running = 0;
	pbuf_sz = KMALLOC_MAX_SIZE;
	pbuf_data = 0;
	pbuf = NULL;

	init_cpu_power();
out:
	return ret;
}

unsigned int alloc_buf_for_profile(void)
{
	int try_alloc_cnt = 3;
	int ret = 0;

	if (pbuf) {
		pr_info("%s: already alloc memory\n", __func__);
		vfree(pbuf);
	}

	while (try_alloc_cnt-- > 0) {
		pbuf = vmalloc(pbuf_sz);
		if (!pbuf) {
			pr_err("[%s] %s: kmalloc failed: try again...%d\n", prefix, __func__, try_alloc_cnt);
			pbuf_sz -= (1024 * 1024);
			continue;
		} else {
			break;
		}
	}

	if (try_alloc_cnt <= 0) {
		pr_err("[%s] %s: kmalloc failed: pbuf_sz: %d\n", prefix, __func__, pbuf_sz);
		ret = -1;
	} else {
		memset(pbuf, 0, pbuf_sz);
		pr_info("%s: kmalloc ok. buf=%p, pbuf_sz=%d\n", __func__, pbuf, pbuf_sz);
	}

	return ret;
}

void prepare_header_profile(void)
{
	int grp_num = 1;
	int cluster, cpu;

	pbuf_data = 0;

	// temperature
	pbuf_data += snprintf(pbuf + pbuf_data, pbuf_sz - pbuf_data, "%02d-temp_BIG ", grp_num);
	pbuf_data += snprintf(pbuf + pbuf_data, pbuf_sz - pbuf_data, "%02d-temp_LITTLE ", grp_num);
	if (cl_cnt > 2)
		pbuf_data += snprintf(pbuf + pbuf_data, pbuf_sz - pbuf_data, "%02d-temp_MID ", grp_num);
	if (cl_cnt > 3)
		pbuf_data += snprintf(pbuf + pbuf_data, pbuf_sz - pbuf_data, "%02d-temp_MID_H ", grp_num);

	pbuf_data += snprintf(pbuf + pbuf_data, pbuf_sz - pbuf_data, "%02d-temp_G3D ", grp_num);
	pbuf_data += snprintf(pbuf + pbuf_data, pbuf_sz - pbuf_data, "%02d-temp_BAT ", grp_num++);

	// cpufreq
	for_each_cluster_rev(cluster) {
		cpu = cls[cluster].first_cpu;
		pbuf_data += snprintf(pbuf + pbuf_data, pbuf_sz - pbuf_data, "%02d-cpu%d_max ", grp_num, cpu);
		pbuf_data += snprintf(pbuf + pbuf_data, pbuf_sz - pbuf_data, "%02d-cpu%d_cur ", grp_num, cpu);
		if (cluster == (cl_cnt - 1)) { // if big cluster
			pbuf_data += snprintf(pbuf + pbuf_data, pbuf_sz - pbuf_data, "%02d-cpu%d_siop ", grp_num, cpu);
			pbuf_data += snprintf(pbuf + pbuf_data, pbuf_sz - pbuf_data, "%02d-cpu%d_ocp ", grp_num, cpu);
		}
		grp_num++;
	}

	// dsu
	pbuf_data += snprintf(pbuf + pbuf_data, pbuf_sz - pbuf_data, "%02d-dsu_cur ", grp_num++);

	// mif
	pbuf_data += snprintf(pbuf + pbuf_data, pbuf_sz - pbuf_data, "%02d-mif_cur ", grp_num++);

	// gpu
	pbuf_data += snprintf(pbuf + pbuf_data, pbuf_sz - pbuf_data, "%02d-gpu_util ", grp_num);
	pbuf_data += snprintf(pbuf + pbuf_data, pbuf_sz - pbuf_data, "%02d-gpu_cur ", grp_num++);

	// cpu util
	for_each_possible_cpu(cpu)
		pbuf_data += snprintf(pbuf + pbuf_data, pbuf_sz - pbuf_data, "%02d-util_cpu%d ", grp_num, cpu);
	grp_num++;

	// cpu power
	for_each_possible_cpu(cpu)
		pbuf_data += snprintf(pbuf + pbuf_data, pbuf_sz - pbuf_data, "%02d-power_cpu%d ", grp_num, cpu);

	pbuf_data += snprintf(pbuf + pbuf_data, pbuf_sz - pbuf_data, "%02d-power_total ", grp_num++);

	// task multi
	for_each_possible_cpu(cpu)
		pbuf_data += snprintf(pbuf + pbuf_data, pbuf_sz - pbuf_data, "%02d-task_cpu%d ", grp_num, cpu);
	grp_num++;

	// tune mode
	pbuf_data += snprintf(pbuf + pbuf_data, pbuf_sz - pbuf_data, "%02d-ems_mode %02d-ems_level ", grp_num, grp_num);

	pbuf_data -= 1;
	pbuf_data += snprintf(pbuf + pbuf_data, pbuf_sz - pbuf_data, "\n");
}

static int init_profile_kthread(void)
{
	struct cpumask mask;
	int ret = 0;

	cpulist_parse("0-3", &mask);
	cpumask_and(&mask, cpu_possible_mask, &mask);
	p_info.task = kthread_create(profile_thread, NULL, "xperf%u", 0);
	if (IS_ERR(p_info.task)) {
		ret = PTR_ERR(p_info.task);
		goto out;
	}

	set_cpus_allowed_ptr(p_info.task, &mask);
out:
	return ret;
}

//===========================================================
// sysfs nodes
//===========================================================

// run
static ssize_t run_show(struct kobject *k, struct kobj_attribute *attr, char *b)
{
	int ret = 0;
	ret += sprintf(b + ret, "%d\n", p_info.is_running);
	return ret;
}

static ssize_t run_store(struct kobject *k, struct kobj_attribute *attr, const char *b, size_t count)
{
	int ret = 0;
	int run = 0;

	if (sscanf(b, "%d", &run) != 1)
		return -EINVAL;

	if (run) {
		if (p_info.is_running) {
			pr_err("[%s] %s: already running profile_thread!!\n", prefix, __func__);
			goto out;
		}

		ret = init_profile_kthread();
		if (ret) {
			pr_err("[%s] %s: failed to create profile_thread.\n", prefix, __func__);
			goto out;
		}

		ret = alloc_buf_for_profile();
		if (ret) {
			pr_err("[%s] %s: failed to start profile by alloc fail\n", prefix, __func__);
			goto out;
		}

		p_info.is_running = 1;
		prepare_header_profile();
		wake_up_process(p_info.task);

		pr_info("%s: profile start\n", __func__);
	} else {
		p_info.is_running = 0;

		pr_info("%s: profile done\n", __func__);
	}

out:
	return count;
}

// power
static ssize_t power_show(struct kobject *k, struct kobj_attribute *attr, char *b)
{
	int i, j;
	int ret = 0;
	for_each_cluster(i) {
		for (j = 0; j < cls[i].freq_size; j++)
			ret += sprintf(b + ret, "%u %u %u\n",
					cls[i].freqs[j].freq,
					cls[i].freqs[j].dyn_power,
					cls[i].freqs[j].sta_power);
		ret += sprintf(b + ret, "\n");
	}
	b[ret] = '\0';
	return ret;
}

// volt
static ssize_t volt_show(struct kobject *k, struct kobj_attribute *attr, char *b)
{
	int i, j;
	int ret = 0;
	for_each_cluster(i) {
		for (j = 0; j < cls[i].freq_size; j++)
			ret += sprintf(b + ret, "%u %u\n", cls[i].freqs[j].freq, cls[i].freqs[j].volt);
		ret += sprintf(b + ret, "\n");
	}
	b[ret] = '\0';
	return ret;
}

// init
static ssize_t init_store(struct kobject *k, struct kobj_attribute *attr, const char *b, size_t count)
{
	init_cpu_power();
	return count;
}

// tnames
static ssize_t tnames_show(struct kobject *k, struct kobj_attribute *attr, char *buf)
{
	int ret = 0, i;
	for (i = 0; i < MAX_TNAMES; i++)
		ret += sprintf(buf + ret, "%d:%s\n", i, p_info.tns[i]);
	return ret;
}

static ssize_t tnames_store(struct kobject *k, struct kobj_attribute *attr, const char *buf, size_t count)
{
	int i;
	char tbuf[1000], *_tbuf, *_ttmp;

	if (sscanf(buf, "%999s", tbuf) != 1)
		return -EINVAL;
	_tbuf = tbuf;

	i = 0;
	while ((_ttmp = strsep(&_tbuf, ",")) != NULL && i < MAX_TNAMES) {
		strncpy(p_info.tns[i], _ttmp, TASK_COMM_LEN);
		i++;
	}
	return count;
}

static struct kobj_attribute run_attr 	 = __ATTR_RW(run);
static struct kobj_attribute power_attr  = __ATTR_RO(power);
static struct kobj_attribute volt_attr 	 = __ATTR_RO(volt);
static struct kobj_attribute init_attr   = __ATTR_WO(init);
static struct kobj_attribute tnames_attr = __ATTR_RW(tnames);

#define DEF_NODE(name) \
	static ssize_t show_##name(struct kobject *k, struct kobj_attribute *attr, char *buf) { \
		int ret = 0; \
		ret += sprintf(buf + ret, "%d\n", p_info.name); \
		return ret; } \
	static ssize_t store_##name(struct kobject *k, struct kobj_attribute *attr, const char *buf, size_t count) { \
		if (sscanf(buf, "%d", &p_info.name) != 1) \
			return -EINVAL; \
		return count; } \
	static struct kobj_attribute name##_attr = __ATTR(name, 0644, show_##name, store_##name);

DEF_NODE(polling_ms)
DEF_NODE(pid)
DEF_NODE(util_thd)

static struct attribute *prof_attrs[] = {
	&run_attr.attr,
	&power_attr.attr,
	&polling_ms_attr.attr,
	&pid_attr.attr,
	&util_thd_attr.attr,
	&volt_attr.attr,
	&tnames_attr.attr,
	&init_attr.attr,
	NULL
};

static struct attribute_group prof_group = {
	.attrs = prof_attrs,
};

//================================
// debugfs
static int result_seq_show(struct seq_file *file, void *iter)
{
       if (p_info.is_running)
               seq_printf(file, "NO RESULT\n");
       else
               seq_printf(file, "%s", pbuf);    // PRINT RESULT

       return 0;
}

static ssize_t result_seq_write(struct file *file, const char __user *buffer, size_t count, loff_t *off)
{
       return count;
}

static int result_debugfs_open(struct inode *inode, struct file *file)
{
       return single_open(file, result_seq_show, inode->i_private);
}

static struct file_operations result_debugfs_fops = {
       .owner          = THIS_MODULE,
       .open           = result_debugfs_open,
       .write          = result_seq_write,
       .read           = seq_read,
       .llseek         = seq_lseek,
       .release        = single_release,
};

int xperf_prof_init(struct platform_device *pdev, struct kobject *kobj)
{
	struct device_node *dn = pdev->dev.of_node;
	struct dentry *root, *d;
	int ret = 0;

	ret = init_cluster_info();	// get cl_cnt
	if (!ret) {
		pr_err("[%s] %s: cannot get cluster cnt %d\n", prefix, __func__, ret);
		return -ENODATA;
	}

	ret = init_profile_info(dn);
	if (ret) {
		pr_err("[%s] %s: fail to init profile info.\n", prefix, __func__);
		return -ENODATA;
	}

	// sysfs: normal nodes
	prof_kobj = kobject_create_and_add("prof", kobj);
	if (!prof_kobj) {
		pr_err("[%s] %s: create node failed\n", prefix, __func__);
		return -EINVAL;
	}

	ret = sysfs_create_group(prof_kobj, &prof_group);
	if (ret) {
		pr_err("[%s] %s: create group failed\n", prefix, __func__);
		return -EINVAL;
	}

	// debugfs: only for result
	root = debugfs_create_dir("xperf_prof", NULL);
	if (!root) {
		pr_err("[%s] %s: create debugfs dir\n", prefix, __func__);
		return -ENOMEM;
	}

	d = debugfs_create_file("result", S_IRUSR, root,
				(unsigned int *)0,
				&result_debugfs_fops);
	if (!d) {
		pr_err("[%s] %s: failed create debugfs file\n", prefix, __func__);
		return -ENOMEM;
	}

	return 0;
} 
