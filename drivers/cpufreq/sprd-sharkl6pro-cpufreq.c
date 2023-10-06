// SPDX-License-Identifier: GPL-2.0
// Copyright (C) 2021 Unisoc, Inc.

#include <linux/arch_topology.h>
#include <linux/cpu.h>
#include <linux/cpufreq.h>
#include <linux/delay.h>
#include <linux/device.h>
#include <linux/io.h>
#include <linux/jiffies.h>
#include <linux/kernel.h>
#include <linux/mfd/syscon.h>
#include <linux/mutex.h>
#include <linux/module.h>
#include <linux/nvmem-consumer.h>
#include <linux/of.h>
#include <linux/of_platform.h>
#include <linux/platform_device.h>
#include <linux/pm_opp.h>
#include <linux/printk.h>
#include <linux/random.h>
#include <linux/regmap.h>
#include <linux/slab.h>
#include <linux/sched.h>
#include <linux/topology.h>
#include <linux/types.h>
#include <linux/workqueue.h>
#include <linux/sprd_sip_svc.h>

#define DVFS_NVMEM_NAME			"dvfs_bin"

#define DVFS_CHIP_STR_HIGH		"T7528"
#define DVFS_CHIP_STR_MID		"T7525"
#define DVFS_CHIP_STR_LOW		"T7520"

#define DVFS_BIN_STR_FF	 		0
#define DVFS_BIN_STR_TT	 		1
#define DVFS_BIN_STR_SS	 		2
#define DVFS_BIN_STR_OD	 		3

#define DVFS_TEMP_INVALID		0xFF

#define DVFS_CHIP_IDX_DEFAULT		0
#define DVFS_CHIP_IDX_HIGH		1
#define DVFS_CHIP_IDX_MID		2
#define DVFS_CHIP_IDX_LOW		3

#define DVFS_BIN_IDX_DEFAULT	 	0
#define DVFS_BIN_IDX_FF	 		1
#define DVFS_BIN_IDX_TT	 		2
#define DVFS_BIN_IDX_SS	 		3
#define DVFS_BIN_IDX_OD	 		4

struct cluster_ops {
	int (*phy_enable)(u32 cluster);
	int (*table_update)(u32 cluster, u32 *num, u32 temp);
	int (*cluster_config)(u32 cluster, u32 bin, u32 margin, u32 step);

	int (*freq_set)(u32 cluster, u32 index);
	int (*freq_get)(u32 cluster, u64 *freq);
	int (*index_info)(u32 cluster, u32 index, u64 *freq, u64 *vol);
};

struct chip_ops {
	int (*chip_config)(u32 ver);
};

struct cluster_info {
	u32 id;

	struct mutex mutex;
	struct cluster_ops *ops;
	struct device_node *node;

	u32 cpu_bin;
	u32 voltage_step;
	u32 voltage_margin;
	u32 transition_delay;

	u32 table_num;
	int table_temp;

	u32 boost_duration;
	unsigned long boost_duration_tick;

	u32 temp_threshold_num;
	int *temp_threshold_list;
};

struct chip_info {
	u32 ver;
	struct chip_ops *ops;
	u32 cluster_num;
	struct cluster_info *cluster_info;
};

static struct device *chip_dev;
static struct chip_info *chip_info;

/* common interface */

static struct cluster_info *sprd_cluster_info(u32 cpu)
{
	struct cluster_info *cluster_info;
	u32 cluster_index;

	if (!chip_info || !chip_info->cluster_info || cpu >= nr_cpu_ids)
		return NULL;

	cluster_index = topology_physical_package_id(cpu);
	if (cluster_index >= chip_info->cluster_num)
		return NULL;

	cluster_info = chip_info->cluster_info + cluster_index;

	return cluster_info;
}

static struct device_node *sprd_cluster_node(u32 cpu)
{
	struct device_node *node;
	struct device *dev;

	dev = get_cpu_device(cpu);
	if (!dev)
		return NULL;

	node = of_parse_phandle(dev->of_node, "cpufreq-data-v1", 0);

	return node;
}

/* sprd_cpufreq_driver interface */

static int sprd_opp_update(struct cpufreq_policy *policy, uint32_t temp)
{
	struct cpufreq_frequency_table *table;
	struct cluster_info *cluster;
	struct cluster_ops *ops;
	struct device *dev;
	u64 freq, vol;
	int i, ret;

	if (!policy) {
		dev_err(chip_dev, "policy is error\n");
		return -EINVAL;
	}

	cluster = (struct cluster_info *)policy->driver_data;
	if (!cluster) {
		dev_err(chip_dev, "cluster is error\n");
		return -EINVAL;
	}

	ops = cluster->ops;
	if (!ops || !ops->table_update || !ops->index_info) {
		dev_err(chip_dev, "cluster %u ops is error\n", cluster->id);
		return -EINVAL;
	}

	dev = get_cpu_device(policy->cpu);
	if (!dev) {
		dev_err(chip_dev, "cpu %u dev get error\n", policy->cpu);
		return -ENODEV;
	}

	/* update dvfs table with new temperature */
	ret = ops->table_update(cluster->id, &cluster->table_num, temp);
	if (ret) {
		dev_err(chip_dev, "cluster %u table update error\n", cluster->id);
		return -EINVAL;
	}

	cluster->table_temp = temp;

	dev_info(chip_dev, "cluster %u table list:\n", cluster->id);

	for (i = 0; i < cluster->table_num; ++i) {
		/* gets the pair of frequency and voltage */
		ret = ops->index_info(cluster->id, i, &freq, &vol);
		if (ret) {
			dev_err(chip_dev, "cluster %u index %u pair get error\n", cluster->id, i);
			return -EINVAL;
		}

		dev_info(chip_dev, "%luHz / %luuV\n", freq, vol);

		ret = dev_pm_opp_add(dev, freq, vol);
		if (ret) {
			dev_err(chip_dev, "add to opp failed, ret = %d\n", ret);
			return -EINVAL;
		}
	}

	/* get the frequency table */
	ret = dev_pm_opp_init_cpufreq_table(dev, &table);
	if (ret) {
		dev_err(chip_dev, "cluster %u freq table init error, ret = %d\n", cluster->id, ret);
		goto free_opp;
	}

	/* verify and init policy the frequency table */
	ret = cpufreq_table_validate_and_show(policy, table);
	if (ret) {
		dev_err(chip_dev, "cluster %u freq table is invalid, ret = %d\n", cluster->id, ret);
		goto free_table;
	}

	return 0;

free_table:
	dev_pm_opp_free_cpufreq_table(dev, &table);

free_opp:
	dev_pm_opp_of_remove_table(dev);

	return -EINVAL;
}

static int sprd_cpufreq_init(struct cpufreq_policy *policy)
{
	struct cluster_info *cluster;
	struct cluster_ops *ops;
	u64 freq;
	int ret;

	if (!policy) {
		dev_err(chip_dev, "policy is error\n");
		return -EINVAL;
	}

	cluster = sprd_cluster_info(policy->cpu);
	if (!cluster) {
		dev_err(chip_dev, "cluster get error\n", policy->cpu);
		return -EINVAL;
	}

	mutex_lock(&cluster->mutex);

	ops = cluster->ops;
	if (!ops || !ops->freq_get || !ops->phy_enable) {
		dev_err(chip_dev, "cluster %u ops is error\n", cluster->id);
		goto unlock_ret;
	}

	/* init driver data */
	policy->driver_data = cluster;

	/* CPUs in the same cluster share same clock and power domain */
	cpumask_or(policy->cpus, policy->cpus, cpu_coregroup_mask(policy->cpu));

	/* init policy transition delay */
	policy->transition_delay_us = cluster->transition_delay;

	/* init other cpu policy link in same cluster */
	policy->dvfs_possible_from_any_cpu = true;

	/* init opp table */
	ret = sprd_opp_update(policy, DVFS_TEMP_INVALID);
	if (ret) {
		dev_err(chip_dev, "cluster %u opp update error\n", cluster->id);
		goto unlock_ret;
	}

	/* init current freq */
	ret = ops->freq_get(cluster->id, &freq);
	if (ret) {
		dev_err(chip_dev, "cluster %u freq get error\n", cluster->id);
		goto unlock_ret;
	}

	policy->cur = (u32)(freq / 1000U); /* form Hz to KHz */

	/* enable apcpu dvfs */
	dev_info(chip_dev, "enable cluster %u dvfs\n", cluster->id);

	ret = ops->phy_enable(cluster->id);
	if (ret) {
		dev_err(chip_dev, "cluster %u dvfs enable error\n", cluster->id);
		goto unlock_ret;
	}

	mutex_unlock(&cluster->mutex);

	return 0;

unlock_ret:
	mutex_unlock(&cluster->mutex);

	return -EINVAL;
}

static int sprd_cpufreq_exit(struct cpufreq_policy *policy)
{
	struct cluster_info *cluster;
	struct device *dev;

	if (!policy) {
		dev_err(chip_dev, "policy is error\n");
		return -EINVAL;
	}

	cluster = (struct cluster_info *)policy->driver_data;
	if (!cluster) {
		dev_err(chip_dev, "policy is not init\n");
		return -EINVAL;
	}

	mutex_lock(&cluster->mutex);

	dev = get_cpu_device(policy->cpu);
	if (!dev) {
		dev_err(chip_dev, "cpu device get failed\n");
		goto unlock_ret;
	}

	dev_pm_opp_free_cpufreq_table(dev, &policy->freq_table);
	dev_pm_opp_of_remove_table(dev);

	policy->driver_data = NULL;

	mutex_unlock(&cluster->mutex);

	return 0;

unlock_ret:
	mutex_unlock(&cluster->mutex);

	return -EINVAL;
}

static int sprd_cpufreq_table_verify(struct cpufreq_policy *policy)
{
	return cpufreq_generic_frequency_table_verify(policy);
}

static int sprd_cpufreq_set_target_index(struct cpufreq_policy *policy, u32 index)
{
	struct cluster_info *cluster;
	struct cluster_ops *ops;
	unsigned long freq;
	int ret;

	if (!policy) {
		dev_err(chip_dev, "policy is error\n");
		return -EINVAL;
	}

	cluster = (struct cluster_info *)policy->driver_data;
	if (!cluster) {
		dev_err(chip_dev, "cluster is error\n");
		return -EINVAL;
	}

	mutex_lock(&cluster->mutex);

	ops = cluster->ops;
	if (!ops || !ops->freq_set) {
		dev_err(chip_dev, "cluster %u ops is error\n", cluster->id);
		goto unlock_ret_error;
	}

	if (index >= cluster->table_num) {
		dev_err(chip_dev, "cluster %u index %u is error\n", cluster->id, index);
		goto unlock_ret_error;
	}

	if (cluster->boost_duration && unlikely(!time_after(jiffies, cluster->boost_duration_tick)))
		goto unlock_ret_success;

	ret = ops->freq_set(cluster->id, index);
	if (ret) {
		dev_err(chip_dev, "cluster %u index %u set error\n", cluster->id, index);
		goto unlock_ret_error;
	}

	freq = policy->freq_table[index].frequency;

	arch_set_freq_scale(policy->related_cpus, freq, policy->cpuinfo.max_freq);

unlock_ret_success:
	mutex_unlock(&cluster->mutex);

	return 0;

unlock_ret_error:
	mutex_unlock(&cluster->mutex);

	return -EINVAL;
}

static u32 sprd_cpufreq_get(u32 cpu)
{
	struct cluster_info *cluster;
	struct cluster_ops *ops;
	u64 freq;
	int ret;

	cluster = sprd_cluster_info(cpu);
	if (!cluster) {
		dev_err(chip_dev, "cluster get error\n", cpu);
		return 0;
	}

	mutex_lock(&cluster->mutex);

	ops = cluster->ops;
	if (!ops || !ops->freq_get) {
		dev_err(chip_dev, "cluster %u ops is error\n", cluster->id);
		goto unlock_ret;
	}

	ret = ops->freq_get(cluster->id, &freq);
	if (ret) {
		dev_err(chip_dev, "cluster %u freq get error\n", cluster->id);
		goto unlock_ret;
	}

	mutex_unlock(&cluster->mutex);

	return (u32)(freq / 1000U); /* from Hz to KHz */

unlock_ret:
	mutex_unlock(&cluster->mutex);

	return 0;
}

static int sprd_cpufreq_suspend(struct cpufreq_policy *policy)
{
	if (!strcmp(policy->governor->name, "userspace")) {
		pr_info("Do nothing for governor-%s\n", policy->governor->name);
		return 0;
	}

	return cpufreq_generic_suspend(policy);
}

static int sprd_cpufreq_resume(struct cpufreq_policy *policy)
{
	if (!strcmp(policy->governor->name, "userspace")) {
		pr_info("Do nothing for governor-%s\n", policy->governor->name);
		return 0;
	}

	return cpufreq_generic_suspend(policy);
}

static struct cpufreq_driver sprd_cpufreq_driver = {
	.name = "sprd-cpufreq",
	.flags = CPUFREQ_STICKY | CPUFREQ_NEED_INITIAL_FREQ_CHECK |
		 CPUFREQ_HAVE_GOVERNOR_PER_POLICY,
	.init = sprd_cpufreq_init,
	.exit = sprd_cpufreq_exit,
	.verify = sprd_cpufreq_table_verify,
	.target_index = sprd_cpufreq_set_target_index,
	.get = sprd_cpufreq_get,
	.suspend = sprd_cpufreq_suspend,
	.resume = sprd_cpufreq_resume,
	.attr = cpufreq_generic_attr,
};

/* init inerface */

static int sprd_cluster_config(struct cluster_info *cluster)
{
	uint32_t bin, margin, step;
	struct cluster_ops *ops;
	int ret;

	if (!cluster) {
		dev_err(chip_dev, "cluster is error\n");
		return -EINVAL;
	}

	ops = cluster->ops;
	if (!ops || !ops->cluster_config) {
		dev_err(chip_dev, "cluster %u ops is error\n", cluster->id);
		return -EINVAL;
	}

	bin = cluster->cpu_bin;
	step = cluster->voltage_step;
	margin = cluster->voltage_margin;

	ret = ops->cluster_config(cluster->id, bin, margin, step);
	if (ret) {
		dev_err(chip_dev, "cluster %u config error\n", cluster->id);
		return -EINVAL;
	}

	return 0;
}

static int sprd_cluster_temp_init(struct cluster_info *cluster)
{
	struct property *pro;
	size_t len;
	int i, ret;
	u32 *val;

	if (!cluster || !cluster->node) {
		dev_err(chip_dev, "cluster is error\n");
		return -EINVAL;
	}

	pro = of_find_property(cluster->node, "temp-threshold", &cluster->temp_threshold_num);
	if (!pro) {
		dev_warn(chip_dev, "temperature threshold not found\n");
		cluster->temp_threshold_num = 0U;
		return 0;
	}

	cluster->temp_threshold_num /= sizeof(u32);
	len = cluster->temp_threshold_num * sizeof(int);

	cluster->temp_threshold_list = devm_kzalloc(chip_dev, len, GFP_KERNEL);
	if (!cluster->temp_threshold_list) {
		dev_err(chip_dev, "memory alloc for temp threshold list is error\n");
		return -ENOMEM;
	}

	for (i = 0; i < cluster->temp_threshold_num; i++) {
		val = (u32 *)(cluster->temp_threshold_list + i);

		ret = of_property_read_u32_index(cluster->node, "temp-threshold", i, val);
		if (ret) {
			dev_err(chip_dev, "temp threshold get failed\n");
			return -EINVAL;
		}
	}

	return 0;
}

static int sprd_cluster_param_init(struct cluster_info *cluster)
{
	int ret;

	/* check parameter */
	if (!cluster || !cluster->node) {
		dev_err(chip_dev, "cluster is error\n");
		return -EINVAL;
	}

	/* init voltage step */
	ret = of_property_read_u32(cluster->node, "voltage-step", &cluster->voltage_step);
	if (ret) {
		dev_warn(chip_dev, "voltage-step is not found\n");
		cluster->voltage_step = 0U;
	} else {
		dev_info(chip_dev, "voltage step is %uuV\n", cluster->voltage_step);
	}

	/* init voltage margin */
	ret = of_property_read_u32(cluster->node, "voltage-margin", &cluster->voltage_margin);
	if (ret) {
		dev_warn(chip_dev, "voltage-margin get error\n");
		cluster->voltage_margin = 0U;
	} else {
		dev_info(chip_dev, "voltage margin is %ucycle\n", cluster->voltage_margin);
	}

	/* init boost duration */
	ret = of_property_read_u32(cluster->node, "boost-duration", &cluster->boost_duration);
	if (ret) {
		dev_warn(chip_dev, "boost-duration is not found\n");
		cluster->boost_duration = 0U;
	} else {
		dev_info(chip_dev, "boost delay is %us\n", cluster->boost_duration);
		cluster->boost_duration_tick = jiffies + cluster->boost_duration * HZ;
	}

	/* init transition delay */
	ret = of_property_read_u32(cluster->node, "transition-delay", &cluster->transition_delay);
	if (ret) {
		dev_warn(chip_dev, "transition-delay get error\n");
		cluster->transition_delay = 0U;
	} else {
		dev_info(chip_dev, "transition delay is %uus\n", cluster->transition_delay);
	}

	return 0;
}

static int sprd_cluster_bin_init(struct cluster_info *cluster)
{
	struct nvmem_cell *cell;
	u32 cpu_bin;
	size_t len;
	void *buf;
	int ret;

	if (!cluster || !cluster->node) {
		dev_err(chip_dev, "cluster is error\n");
		return -EINVAL;
	}

	ret = of_property_match_string(cluster->node, "nvmem-cell-names", DVFS_NVMEM_NAME);
	if (ret == -EINVAL) {
		dev_warn(chip_dev, "'%s' nvmem cell not found\n", DVFS_NVMEM_NAME);
		cluster->cpu_bin = DVFS_BIN_IDX_DEFAULT;
		return 0;
	}

	cell = of_nvmem_cell_get(cluster->node, DVFS_NVMEM_NAME);
	if (IS_ERR(cell)) {
		dev_err(chip_dev, "%s nvmem cell get error\n", DVFS_NVMEM_NAME);
		return PTR_ERR(cell);
	}

	buf = nvmem_cell_read(cell, &len);
	if (IS_ERR(buf)) {
		dev_err(chip_dev, "%s nvmem cell read error\n", DVFS_NVMEM_NAME);
		nvmem_cell_put(cell);
		return PTR_ERR(buf);
	}

	if (len > sizeof(u32)) {
		dev_warn(chip_dev, "%s nvmem value len is error\n", DVFS_NVMEM_NAME);
		len = sizeof(u32);
	}

	memcpy(&cpu_bin, buf, len);

	kfree(buf);
	nvmem_cell_put(cell);

	if (!cpu_bin) {
		dev_warn(chip_dev, "cpu bin is not set\n");
		cluster->cpu_bin = DVFS_BIN_IDX_DEFAULT;
		return 0;
	}

	dev_info(chip_dev, "cpu bin is %u\n", cpu_bin);

	switch (cpu_bin) {
	case DVFS_BIN_STR_FF:
		cluster->cpu_bin = DVFS_BIN_IDX_FF;
		return 0;
	case DVFS_BIN_STR_TT:
		cluster->cpu_bin = DVFS_BIN_IDX_TT;
		return 0;
	case DVFS_BIN_STR_SS:
		cluster->cpu_bin = DVFS_BIN_IDX_SS;
		return 0;
	case DVFS_BIN_STR_OD:
		cluster->cpu_bin = DVFS_BIN_IDX_OD;
		return 0;
	default:
		dev_err(chip_dev, "cpu bin is invalid, value = %u\n", cpu_bin);
		return -EINVAL;
	}
}

static int sprd_cluster_ops_init(struct cluster_info *cluster)
{
	struct sprd_sip_svc_dvfs_ops *ops;
	struct sprd_sip_svc_handle *sip;

	if (!cluster) {
		dev_err(chip_dev, "cluster is error\n");
		return -EINVAL;
	}

	sip = sprd_sip_svc_get_handle();
	if (!sip) {
		dev_err(chip_dev, "dvfs sip handle get error\n");
		return -EINVAL;
	}

	ops = &sip->dvfs_ops;

	cluster->ops = devm_kzalloc(chip_dev, sizeof(struct cluster_ops), GFP_KERNEL);
	if (!cluster->ops) {
		dev_err(chip_dev, "memory alloc for cluster ops failed\n");
		return -ENOMEM;
	}

	cluster->ops->phy_enable = ops->phy_enable;
	cluster->ops->table_update = ops->table_update;
	cluster->ops->cluster_config = ops->cluster_config;

	cluster->ops->freq_set = ops->freq_set;
	cluster->ops->freq_get = ops->freq_get;
	cluster->ops->index_info = ops->index_info;

	return 0;
}

static int sprd_cluster_info_init(struct chip_info *chip)
{
	struct cluster_info *cluster;
	u32 cpu, index;
	size_t len;
	int ret;

	/* check parameter */
	if (!chip) {
		dev_err(chip_dev, "chip is error\n");
		return -EINVAL;
	}

	/* init cluster number */
	for_each_possible_cpu(cpu) {
		index = topology_physical_package_id(cpu) + 1U; /* index to number */
		if (index <= chip->cluster_num)
			continue;
		chip->cluster_num = index;
	}

	dev_info(chip_dev, "cluster number is %u\n", chip->cluster_num);

	/* alloc memory for cluster info */
	len = chip->cluster_num * sizeof(struct cluster_info);

	chip->cluster_info = devm_kzalloc(chip_dev, len, GFP_KERNEL);
	if (!chip->cluster_info) {
		dev_err(chip_dev, "memory alloc for cluster info is failed\n");
		return -ENOMEM;
	}

	/* init cluster info */
	for_each_possible_cpu(cpu) {
		cluster = sprd_cluster_info(cpu);
		if (!cluster) {
			dev_err(chip_dev, "cluster info get error\n");
			return -EINVAL;
		}

		if (cluster->node) {
			dev_info(chip_dev, "cluster info is also init\n");
			continue;
		}

		/* Cluster id init */
		cluster->id = topology_physical_package_id(cpu);

		/* Cluster node init */
		cluster->node = sprd_cluster_node(cpu);
		if (!cluster->node) {
			dev_err(chip_dev, "cluster node get error\n");
			return -EINVAL;
		}

		/* Mutex init */
		mutex_init(&cluster->mutex);

		/* cluster ops init */
		ret = sprd_cluster_ops_init(cluster);
		if (ret) {
			dev_err(chip_dev, "cluster ops init error\n");
			return -EINVAL;
		}

		/* CPU bin init */
		ret = sprd_cluster_bin_init(cluster);
		if (ret) {
			dev_err(chip_dev, "cluster bin init error\n");
			return -EINVAL;
		}

		/* cluster par init */
		ret = sprd_cluster_param_init(cluster);
		if (ret) {
			dev_err(chip_dev, "cluster par init error\n");
			return -EINVAL;
		}

		/* cluster temp init */
		ret = sprd_cluster_temp_init(cluster);
		if (ret) {
			dev_err(chip_dev, "cluster temp init error\n");
			return -EINVAL;
		}

		/* init cluster config */
		ret = sprd_cluster_config(cluster);
		if (ret) {
			dev_err(chip_dev, "cluster config error\n");
			return -EINVAL;
		}
	}

	return 0;
}

static int sprd_chip_ver_init(struct chip_info *chip)
{
	struct device_node *hwf;
	struct chip_ops *ops;
	const char *value;
	int ret;

	if (!chip) {
		dev_err(chip_dev, "chip is error\n");
		return -EINVAL;
	}

	ops = chip->ops;
	if (!ops || !ops->chip_config) {
		dev_err(chip_dev, "chip ops is error\n");
		return -EINVAL;
	}

	if (!of_property_read_bool(chip_dev->of_node, "multi-version")) {
		dev_warn(chip_dev, "chip has no different versions\n");
		chip->ver = DVFS_CHIP_IDX_DEFAULT;
		goto chip_version_set;
	}

	/* We distinguish soc as 3 kinds in UMS9620, T7528, T7525, T7520 */
	hwf = of_find_node_by_path("/hwfeature/auto");
	if (IS_ERR_OR_NULL(hwf)) {
		dev_err(chip_dev, "hwfeature node not found\n");
		return PTR_ERR(hwf);
	}

	value = of_get_property(hwf, "efuse", NULL);

	if (!strcmp(value, DVFS_CHIP_STR_HIGH)) {
		dev_dbg(chip_dev, "chip is %s\n", DVFS_CHIP_STR_HIGH);
		chip->ver = DVFS_CHIP_IDX_HIGH;
		goto chip_version_set;
	}

	if (!strcmp(value, DVFS_CHIP_STR_MID)) {
		dev_dbg(chip_dev, "chip is %s\n", DVFS_CHIP_STR_MID);
		chip->ver = DVFS_CHIP_IDX_MID;
		goto chip_version_set;
	}
	if (!strcmp(value, DVFS_CHIP_STR_LOW)) {
		dev_dbg(chip_dev, "chip is %s\n", DVFS_CHIP_STR_LOW);
		chip->ver = DVFS_CHIP_IDX_LOW;
		goto chip_version_set;
	}

	dev_err(chip_dev, "chip ver is error, value = %s\n", value);

	return -EINVAL;

chip_version_set:
	ret = ops->chip_config(chip->ver);
	if (ret) {
		dev_err(chip_dev, "chip ver set error\n");
		return -EINVAL;
	}

	return 0;
}

static int sprd_chip_ops_init(struct chip_info *chip)
{
	struct sprd_sip_svc_dvfs_ops *ops;
	struct sprd_sip_svc_handle *sip;

	if (!chip) {
		dev_err(chip_dev, "chip is error\n");
		return -EINVAL;
	}

	sip = sprd_sip_svc_get_handle();
	if (!sip) {
		dev_err(chip_dev, "dvfs sip handle get error\n");
		return -EINVAL;
	}

	ops = &sip->dvfs_ops;

	chip->ops = devm_kzalloc(chip_dev, sizeof(struct chip_ops), GFP_KERNEL);
	if (!chip->ops) {
		dev_err(chip_dev, "memory alloc for dvfs ops is failed\n");
		return -ENOMEM;
	}

	chip->ops->chip_config = ops->chip_config;

	return 0;
}

static int sprd_cpufreq_probe(struct platform_device *pdev)
{
	int ret;

	chip_dev = &pdev->dev;

	dev_info(chip_dev, "probe sharkl6pro cpufreq driver\n");

	chip_info = devm_kzalloc(chip_dev, sizeof(struct chip_info), GFP_KERNEL);
	if (!chip_info) {
		dev_err(chip_dev, "memory alloc for chip info is failed\n");
		return -ENOMEM;
	}

	/* chip ops init */
	ret = sprd_chip_ops_init(chip_info);
	if (ret) {
		dev_err(chip_dev, "chip ops init error\n");
		return -EINVAL;
	}

	/* chip version init */
	ret = sprd_chip_ver_init(chip_info);
	if (ret) {
		dev_err(chip_dev, "chip ver init error\n");
		return -EINVAL;
	}

	/* Cluster info init */
	ret = sprd_cluster_info_init(chip_info);
	if (ret) {
		dev_err(chip_dev, "cluster info init error\n");
		return -EINVAL;
	}

	/* Register cpufreq driver */
	ret = cpufreq_register_driver(&sprd_cpufreq_driver);
	if (ret)
		dev_err(chip_dev, "register cpufreq driver failed, ret = %d\n", ret);
	else
		dev_info(chip_dev, "register cpufreq driver succeeded\n");

	return 0;
}

/**
 * sprd_cpufreq_update_opp() - returns the max freq of a cpu and update dvfs
 * table by temp_now
 *
 * @cpu: which cpu you want to update dvfs table
 * @temp_now: current temperature on this cpu, mini-degree.
 *
 * Return:
 * 1.cluster is not working, then return 0
 * 2.succeed to update dvfs table then return max freq(KHZ) of this cluster
 */
unsigned int sprd_cpufreq_update_opp(int cpu, int temp_now)
{
	struct cluster_info *cluster;
	struct cluster_ops *ops;
	u64 freq;
	int ret;

	cluster = sprd_cluster_info(cpu);
	if (!cluster) {
		dev_err(chip_dev, "cluster get error\n", cpu);
		return 0;
	}

	mutex_lock(&cluster->mutex);

	ops = cluster->ops;
	if (!ops || !ops->freq_get) {
		dev_err(chip_dev, "cluster %u ops is error\n", cluster->id);
		goto unlock_ret;
	}

	ret = ops->freq_get(cluster->id, &freq);
	if (ret) {
		dev_err(chip_dev, "cluster %u freq get error\n", cluster->id);
		goto unlock_ret;
	}

	mutex_unlock(&cluster->mutex);

	return (u32)(freq / 1000U); /* from Hz to KHz */

unlock_ret:
	mutex_unlock(&cluster->mutex);

	return 0;
}
EXPORT_SYMBOL_GPL(sprd_cpufreq_update_opp);

static const struct of_device_id sprd_cpufreq_of_match[] = {
	{
		.compatible = "sprd,sharkl6pro-cpufreq",
	},
	{
		/* Sentinel */
	},
};

static struct platform_driver sprd_cpufreq_platform_driver = {
	.driver = {
		.name = "sprd-cpufreq",
		.of_match_table = sprd_cpufreq_of_match,
	},
	.probe = sprd_cpufreq_probe,
};

static int __init sprd_cpufreq_platform_driver_register(void)
{
	return platform_driver_register(&sprd_cpufreq_platform_driver);
}

device_initcall(sprd_cpufreq_platform_driver_register);

MODULE_DESCRIPTION("sprd sharkl6pro cpufreq driver");
MODULE_LICENSE("GPL v2");
