/*
 * sec_auth_qos_devfreq_setting.c
 *  Only for exynos models as of now
 */

#include <linux/cpufreq.h>
#include <linux/pm_qos.h>
#include <linux/smp.h>
#include <linux/sched.h>
#include <soc/samsung/exynos_pm_qos.h>
#include <soc/samsung/exynos-devfreq.h>

#define MAX_CLUSTERS	5

static int num_clusters, max_cpufreq;
// Cluster number and cpu id of each cluster
static unsigned int clusters[MAX_CLUSTERS];
// Maximum frequency of each cluster
static unsigned int max_freq_all_clusters[MAX_CLUSTERS];
static struct freq_qos_request req;
// Frequency array of devfreq 
static unsigned int devfreq_freq_array[6];
// Maximum frequency of devfreq (INT) array
static unsigned int max_devfreq_int;
static unsigned int current_cpu, current_cluster;
static struct exynos_pm_qos_request int_qos;

//Initialise current cpu of the process and current cluster of cpu 
static void get_current_cpu_and_cluster(void)
{
	int i;

	current_cpu = smp_processor_id();
	
	for(i=0;i<(num_clusters-1);i++)
	{
		if(current_cpu >= clusters[i] && current_cpu < clusters[i+1])
		{
			current_cluster = i;
			break;
		}
		
		if(i == (num_clusters-2))
			current_cluster = i+1;
	}
	
	pr_info("%s: current cpu(%d), current_cluster(%d)\n", __func__,
			current_cpu, current_cluster);
}

// Add Maximum frequency request to current cluster
int sec_auth_add_qos_request(void)
{
	int ret;
	struct cpufreq_policy *policy;
	
	get_current_cpu_and_cluster();

	
	policy = cpufreq_cpu_get(clusters[current_cluster]);
	
	memset(&req, 0, sizeof(req));
	
	ret = freq_qos_add_request(&policy->constraints, &req, FREQ_QOS_MIN,
			max_freq_all_clusters[current_cluster]);
	pr_info("%s: request added for maximum freq[%d], current cpu(%d), current_cluster(%d), ret(%d)\n", __func__,
			max_freq_all_clusters[current_cluster], current_cpu, current_cluster, ret);

	
	return 0;
}

//Remove Maximum frequency request of current cluster
int sec_auth_remove_qos_request(void)
{
	int ret;
	
	ret = freq_qos_remove_request(&req);
	pr_info("%s: removal request for freq[%d], ret(%d)\n", __func__, max_freq_all_clusters[current_cluster], ret);

	return 0;
}

// Add maximum devfreq frequency request to INT
int sec_auth_add_devfreq_int_request(unsigned int frequency)
{
	if (frequency > 0) {
		exynos_pm_qos_add_request(&int_qos,
			PM_QOS_DEVICE_THROUGHPUT, frequency);
		pr_info("%s: request for %d\n", __func__, frequency);
	} else {
		exynos_pm_qos_add_request(&int_qos,
			PM_QOS_DEVICE_THROUGHPUT, max_devfreq_int);
		pr_info("%s: request for %d\n", __func__, max_devfreq_int);
	}
	
	return 0;
}

// Remove maximum devfreq frequency request to INT
int sec_auth_remove_devfreq_int_request(void)
{
	exynos_pm_qos_remove_request(&int_qos);
	
	pr_info("%s: request for %d removed\n", __func__, max_devfreq_int);
	
	return 0;
}

// Find maximum frequency of all clusters from the frequency table of each cluster
static int fill_max_frequency_all_clusters(void)
{
	struct cpufreq_policy *policy;
	struct cpufreq_frequency_table *pos, *table;
	int i;
	
	for(i = 0; i < num_clusters; i++)
	{
		policy =  cpufreq_cpu_get(clusters[i]);
		
		if(!policy)
			continue;
		
		table = policy->freq_table;
		
		cpufreq_for_each_valid_entry(pos, table) {
			pr_info("%s: frequency = %d, max as of now for cluster %d = %d\n", __func__, pos->frequency, i, max_freq_all_clusters[i]);
			
			if(max_freq_all_clusters[i] < pos->frequency)
				max_freq_all_clusters[i] = pos->frequency;

		}
		if(max_cpufreq < max_freq_all_clusters[i])
			max_cpufreq = max_freq_all_clusters[i];
		
		pr_info("%s: Max frequency of cluster[%d] = %d\n", __func__, i, max_freq_all_clusters[i]);
	}
	pr_info("%s: Maximum frequency[%d]\n", __func__, max_cpufreq);

	return 0;
}

// Parse Example : s5e9945-cpu.dtsi 
static int sec_auth_parse_cpu_clusters(void)
{
	struct device_node *cpus, *map, *cluster, *cpu_node;
	char name[20];
	num_clusters = 0;
	
	cpus = of_find_node_by_path("/cpus");
	if(!cpus)
	{
		pr_err("%s: No CPU node found\n", __func__);
		return -ENOENT;
	}
	
	map = of_get_child_by_name(cpus, "cpu-map");
	if (!map) {
		pr_err("%s: No CPU Map node found\n", __func__);
		return -ENOENT;
	}
	
	do {
		snprintf(name, sizeof(name), "cluster%d", num_clusters);
		cluster = of_get_child_by_name(map, name);
		if (cluster) {
			cpu_node = of_get_child_by_name(cluster, "core0");
			if (cpu_node) {
				cpu_node = of_parse_phandle(cpu_node, "cpu", 0);
				clusters[num_clusters] =
					of_cpu_node_to_id(cpu_node);
				pr_info("%s: cluster[%d] = %d\n", __func__, num_clusters, clusters[num_clusters]);
			}
			num_clusters++;
		} else {
			break;
		}
	} while (cluster);

	return 0;
}

// Parse example: s5e9945-devfreq.dtsi
static int sec_auth_devfreq_int_parse(void)
{
	struct device_node *devfreqs, *devfreqint;
	
	devfreqs = of_find_node_by_path("/exynos_devfreq");
	if(!devfreqs)
	{
		pr_err("%s: No Exynos_devfreq node found\n", __func__);
		return -ENOENT;
	}
	
	devfreqint = of_get_child_by_name(devfreqs, "devfreq_int");
	if(!devfreqint)
	{
		pr_err("%s: No devfreq_int node found\n", __func__);
		return -ENOENT;
	}
	
	if (of_property_read_u32_array(devfreqint, "freq_info", (u32 *)&devfreq_freq_array,
				       (size_t)(ARRAY_SIZE(devfreq_freq_array))))
			return -ENODEV;
	
	
	max_devfreq_int = devfreq_freq_array[4];
	
	pr_info("%s: max devfreq int frequency = %d\n", __func__, max_devfreq_int);
	
	return 0;
	
	
}

// Parse and fill data for CPU and Devfreq
int sec_auth_cpu_related_things_init(void)
{
	if(sec_auth_parse_cpu_clusters())
		return 1;
	
	if(sec_auth_devfreq_int_parse())
		return 1;
	
	fill_max_frequency_all_clusters();
	
	return 0;
}
