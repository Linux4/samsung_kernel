#include <linux/module.h>
#include <linux/thermal.h>
#include <linux/cpufreq.h>
#include <linux/err.h>
#include <linux/slab.h>
#include <linux/cpu.h>
#include <linux/pm_qos.h>
#include <linux/devfreq.h>
#include <linux/cooling_dev_mrvl.h>

struct freq_cool_device {
	struct thermal_cooling_device *cool_dev;
	unsigned int *freqs;
	unsigned int max_state;
	unsigned int cur_state;
	struct pm_qos_request qos_max;
};

struct cpuhotplug_cool_device {
	struct thermal_cooling_device *cool_dev;
	unsigned int max_state;
	unsigned int cur_state;
	struct pm_qos_request cpu_num_max;
	struct pm_qos_request cpu_num_min;
};

#ifdef CONFIG_HOTPLUG_CPU
/* cpuhotplug cooling device callback functions are defined below */
static int cpuhotplug_get_max_state(struct thermal_cooling_device *cdev,
				 unsigned long *state)
{
	struct cpuhotplug_cool_device *cpuhotplug_device = cdev->devdata;

	*state = cpuhotplug_device->max_state;

	return 0;
}

static int cpuhotplug_get_cur_state(struct thermal_cooling_device *cdev,
				 unsigned long *state)
{
	struct cpuhotplug_cool_device *cpuhotplug_device = cdev->devdata;

	*state = cpuhotplug_device->cur_state;

	return 0;
}

static int cpuhotplug_set_cur_state(struct thermal_cooling_device *cdev,
				 unsigned long state)
{
	struct cpuhotplug_cool_device *cpuhotplug_device = cdev->devdata;
	int cpu_num = num_possible_cpus();

	if (state > cpuhotplug_device->max_state)
		return -EINVAL;
	cpuhotplug_device->cur_state = state;
	pm_qos_update_request(&cpuhotplug_device->cpu_num_max, cpu_num - state);

	if (!strcmp(cdev->type, "helan3-hotplug-cool"))
		pm_qos_update_request(&cpuhotplug_device->cpu_num_min, cpu_num - state - 1);

	return 0;
}

/* Bind cpufreq callbacks to thermal cooling device ops */
static struct thermal_cooling_device_ops const cpuhotplug_cooling_ops = {
	.get_max_state = cpuhotplug_get_max_state,
	.get_cur_state = cpuhotplug_get_cur_state,
	.set_cur_state = cpuhotplug_set_cur_state,
};

struct thermal_cooling_device *cpuhotplug_cool_register(const char *cpu_name)
{
	struct thermal_cooling_device *cool_dev;
	struct cpuhotplug_cool_device *cpuhotplug_dev = NULL;
	char cool_name[THERMAL_NAME_LENGTH];


	cpuhotplug_dev = kzalloc(sizeof(struct cpuhotplug_cool_device),
			      GFP_KERNEL);
	if (!cpuhotplug_dev)
		return ERR_PTR(-ENOMEM);

	snprintf(cool_name, sizeof(cool_name), "%s-hotplug-cool", cpu_name);

	cool_dev = thermal_cooling_device_register(cool_name,
			cpuhotplug_dev, &cpuhotplug_cooling_ops);
	if (!cool_dev) {
		kfree(cpuhotplug_dev);
		return ERR_PTR(-EINVAL);
	}

	cpuhotplug_dev->max_state = num_possible_cpus() - 1;
	cpuhotplug_dev->cool_dev = cool_dev;
	cpuhotplug_dev->cur_state = 0;

	cpuhotplug_dev->cpu_num_max.name = "req_thermal";
	pm_qos_add_request(&cpuhotplug_dev->cpu_num_max,
		PM_QOS_CPU_NUM_MAX, num_possible_cpus());

	if (!strcmp("helan3", cpu_name)) {
		cpuhotplug_dev->cpu_num_min.name = "req_thermal";
		pm_qos_add_request(&cpuhotplug_dev->cpu_num_min,
			PM_QOS_CPU_NUM_MIN, 0);
	}

	return cool_dev;
}
EXPORT_SYMBOL_GPL(cpuhotplug_cool_register);

void cpuhotplug_cool_unregister(struct thermal_cooling_device *cdev)
{
	struct cpuhotplug_cool_device *cpuhotplug_dev = cdev->devdata;

	pm_qos_remove_request(&cpuhotplug_dev->cpu_num_max);
	thermal_cooling_device_unregister(cpuhotplug_dev->cool_dev);
	kfree(cpuhotplug_dev);
}
EXPORT_SYMBOL_GPL(cpuhotplug_cool_unregister);
#endif


#ifdef CONFIG_CPU_FREQ
/* cpufreq cooling device callback functions are defined below */
static int cpufreq_get_max_state(struct thermal_cooling_device *cdev,
				 unsigned long *state)
{
	struct freq_cool_device *cpufreq_device = cdev->devdata;

	*state = cpufreq_device->max_state;

	return 0;
}

static int cpufreq_get_cur_state(struct thermal_cooling_device *cdev,
				 unsigned long *state)
{
	struct freq_cool_device *cpufreq_device = cdev->devdata;

	*state = cpufreq_device->cur_state;

	return 0;
}

static int cpufreq_set_cur_state(struct thermal_cooling_device *cdev,
				 unsigned long state)
{
	struct freq_cool_device *cpufreq_device = cdev->devdata;

	if (state > cpufreq_device->max_state)
		return -EINVAL;

	cpufreq_device->cur_state = state;

	pm_qos_update_request(&cpufreq_device->qos_max,
			cpufreq_device->freqs[state]);

	return 0;
}

/* Bind cpufreq callbacks to thermal cooling device ops */
static struct thermal_cooling_device_ops const cpufreq_cooling_ops = {
	.get_max_state = cpufreq_get_max_state,
	.get_cur_state = cpufreq_get_cur_state,
	.set_cur_state = cpufreq_set_cur_state,
};

struct thermal_cooling_device *cpufreq_cool_register(const char *cpu_name)
{
	struct thermal_cooling_device *cool_dev;
	struct freq_cool_device *cpufreq_dev = NULL;
	int i, index = 0, descend = -1;
	unsigned int max_level = 0;
	unsigned int freq = CPUFREQ_ENTRY_INVALID;
	char cool_name[THERMAL_NAME_LENGTH];
	struct cpufreq_frequency_table *table;

	cpufreq_dev = kzalloc(sizeof(struct freq_cool_device),
			      GFP_KERNEL);
	if (!cpufreq_dev)
		return ERR_PTR(-ENOMEM);

	snprintf(cool_name, sizeof(cool_name), "%s-freq-cool", cpu_name);
	cool_dev = thermal_cooling_device_register(cool_name, cpufreq_dev,
						   &cpufreq_cooling_ops);
	if (!cool_dev) {
		kfree(cpufreq_dev);
		return ERR_PTR(-EINVAL);
	}

	/* get frequency number */
	if (!strcmp("cluster0", cpu_name))
		table = cpufreq_frequency_get_table(0);
	else if (!strcmp("cluster1", cpu_name))
		table = cpufreq_frequency_get_table(4);
	else
		table = cpufreq_frequency_get_table(0);

	if (!table)
		return ERR_PTR(-EINVAL);

	/* get frequency number and order*/
	for (i = 0; table[i].frequency != CPUFREQ_TABLE_END; i++) {
		if (table[i].frequency == CPUFREQ_ENTRY_INVALID)
			continue;

		if (freq != CPUFREQ_ENTRY_INVALID && descend == -1)
			descend = !!(freq > table[i].frequency);

		freq = table[i].frequency;
		max_level++;
	}
	cpufreq_dev->max_state = max_level - 1;
	/* init freqs */
	cpufreq_dev->freqs = kzalloc(sizeof(unsigned int) * max_level,
			GFP_KERNEL);
	if (!cpufreq_dev->freqs) {
		thermal_cooling_device_unregister(cool_dev);
		kfree(cpufreq_dev);
		return ERR_PTR(-ENOMEM);
	}
	for (i = 0; table[i].frequency != CPUFREQ_TABLE_END; i++) {
		if (table[i].frequency == CPUFREQ_ENTRY_INVALID)
			continue;

		index = descend ? i : (max_level - i - 1);
		cpufreq_dev->freqs[index] = table[i].frequency;
	}

	cpufreq_dev->cool_dev = cool_dev;
	cpufreq_dev->cur_state = 0;

	cpufreq_dev->qos_max.name = "req_thermal";

	if (!strcmp("cluster0", cpu_name))
		pm_qos_add_request(&cpufreq_dev->qos_max,
			PM_QOS_CPUFREQ_L_MAX, INT_MAX);
	else if (!strcmp("cluster1", cpu_name))
		pm_qos_add_request(&cpufreq_dev->qos_max,
			PM_QOS_CPUFREQ_B_MAX, INT_MAX);
	else
		pm_qos_add_request(&cpufreq_dev->qos_max,
			PM_QOS_CPUFREQ_MAX, INT_MAX);

	return cool_dev;
}
EXPORT_SYMBOL_GPL(cpufreq_cool_register);

void cpufreq_cool_unregister(struct thermal_cooling_device *cdev)
{
	struct freq_cool_device *cpufreq_dev = cdev->devdata;

	pm_qos_remove_request(&cpufreq_dev->qos_max);
	thermal_cooling_device_unregister(cpufreq_dev->cool_dev);
	kfree(cpufreq_dev->freqs);
	kfree(cpufreq_dev);
}
EXPORT_SYMBOL_GPL(cpufreq_cool_unregister);
#endif

static int freq_get_max_state(struct thermal_cooling_device *cdev,
				 unsigned long *state)
{
	struct freq_cool_device *freq_device = cdev->devdata;

	*state = freq_device->max_state;

	return 0;
}

static int freq_get_cur_state(struct thermal_cooling_device *cdev,
				 unsigned long *state)
{
	struct freq_cool_device *freq_device = cdev->devdata;

	*state = freq_device->cur_state;

	return 0;
}

static int freq_set_cur_state(struct thermal_cooling_device *cdev,
				 unsigned long state)
{
	struct freq_cool_device *freq_device = cdev->devdata;

	if (state > freq_device->max_state)
		return -EINVAL;

	freq_device->cur_state = state;
	pm_qos_update_request(&freq_device->qos_max,
			freq_device->freqs[state]);
	return 0;
}

/* Bind cpufreq callbacks to thermal cooling device ops */
static struct thermal_cooling_device_ops const freq_cooling_ops = {
	.get_max_state = freq_get_max_state,
	.get_cur_state = freq_get_cur_state,
	.set_cur_state = freq_set_cur_state,
};
#ifdef CONFIG_DDR_DEVFREQ
struct thermal_cooling_device *ddrfreq_cool_register(void)
{
	struct thermal_cooling_device *cool_dev;
	struct freq_cool_device *freq_dev = NULL;
	int i, descend = -1, index = 0;
	unsigned int max_level = 0, freq = 0;
	struct devfreq_frequency_table *table =
		devfreq_frequency_get_table(DEVFREQ_DDR);

	freq_dev = kzalloc(sizeof(struct freq_cool_device),
			      GFP_KERNEL);
	if (!freq_dev)
		return ERR_PTR(-ENOMEM);

	cool_dev = thermal_cooling_device_register("ddr-freq-cool", freq_dev,
						   &freq_cooling_ops);
	if (!cool_dev) {
		kfree(freq_dev);
		return ERR_PTR(-EINVAL);
	}
	freq_dev->cool_dev = cool_dev;
	freq_dev->cur_state = 0;
	/* get frequency number */
	for (i = 0; table[i].frequency != DEVFREQ_TABLE_END; i++) {
		if (freq != 0 && descend == -1)
			descend = !!(freq > table[i].frequency);
		freq = table[i].frequency;
		max_level++;
	}

	freq_dev->max_state = max_level - 1;
	/* init freqs */
	freq_dev->freqs = kzalloc(sizeof(unsigned int) * max_level,
			GFP_KERNEL);
	if (!freq_dev->freqs) {
		thermal_cooling_device_unregister(cool_dev);
		kfree(freq_dev);
		return ERR_PTR(-ENOMEM);
	}
	for (i = 0; table[i].frequency != DEVFREQ_TABLE_END; i++) {
		index = descend ? i : (max_level - i - 1);
		freq_dev->freqs[index] = table[i].frequency;
	}

	freq_dev->qos_max.name = "req_thermal";
	pm_qos_add_request(&freq_dev->qos_max,
		PM_QOS_DDR_DEVFREQ_MAX, PM_QOS_DEFAULT_VALUE);

	return cool_dev;
}
EXPORT_SYMBOL_GPL(ddrfreq_cool_register);

void ddrfreq_cool_unregister(struct thermal_cooling_device *cdev)
{
	struct freq_cool_device *freq_dev = cdev->devdata;

	pm_qos_remove_request(&freq_dev->qos_max);
	thermal_cooling_device_unregister(freq_dev->cool_dev);
	kfree(freq_dev->freqs);
	kfree(freq_dev);
}
EXPORT_SYMBOL_GPL(ddrfreq_cool_unregister);
#endif

#ifdef CONFIG_VPU_DEVFREQ
struct thermal_cooling_device *vpufreq_cool_register(unsigned int dev_id)
{
	struct thermal_cooling_device *cool_dev;
	struct freq_cool_device *freq_dev = NULL;
	int i, descend = -1, index = 0;
	unsigned int max_level = 0, freq = 0;
	struct devfreq_frequency_table *table =
		devfreq_frequency_get_table(dev_id);

	freq_dev = kzalloc(sizeof(struct freq_cool_device),
			      GFP_KERNEL);
	if (!freq_dev)
		return ERR_PTR(-ENOMEM);

	cool_dev = thermal_cooling_device_register("vpu-freq-cool", freq_dev,
						   &freq_cooling_ops);
	if (!cool_dev) {
		kfree(freq_dev);
		return ERR_PTR(-EINVAL);
	}
	freq_dev->cool_dev = cool_dev;
	freq_dev->cur_state = 0;
	/* get frequency number */
	for (i = 0; table[i].frequency != DEVFREQ_TABLE_END; i++) {
		if (freq != 0 && descend == -1)
			descend = !!(freq > table[i].frequency);
		freq = table[i].frequency;
		max_level++;
	}

	freq_dev->max_state = max_level - 1;
	/* init freqs */
	freq_dev->freqs = kzalloc(sizeof(unsigned int) * max_level,
			GFP_KERNEL);
	if (!freq_dev->freqs) {
		thermal_cooling_device_unregister(cool_dev);
		kfree(freq_dev);
		return ERR_PTR(-ENOMEM);
	}
	for (i = 0; table[i].frequency != DEVFREQ_TABLE_END; i++) {
		index = descend ? i : (max_level - i - 1);
		freq_dev->freqs[index] = table[i].frequency;
	}

	freq_dev->qos_max.name = "req_thermal";
	pm_qos_add_request(&freq_dev->qos_max,
		PM_QOS_VPU_DEVFREQ_MAX, PM_QOS_DEFAULT_VALUE);

	return cool_dev;
}
EXPORT_SYMBOL_GPL(vpufreq_cool_register);

void vpufreq_cool_unregister(struct thermal_cooling_device *cdev)
{
	struct freq_cool_device *freq_dev = cdev->devdata;

	pm_qos_remove_request(&freq_dev->qos_max);
	thermal_cooling_device_unregister(freq_dev->cool_dev);
	kfree(freq_dev->freqs);
	kfree(freq_dev);
}
EXPORT_SYMBOL_GPL(vpufreq_cool_unregister);
#endif

#ifdef CONFIG_PM_DEVFREQ
struct thermal_cooling_device *gpufreq_cool_register(const char *gc_name)
{
	struct thermal_cooling_device *cool_dev;
	struct freq_cool_device *freq_dev = NULL;
	int i, descend = -1, index = 0;
	unsigned int max_level = 0, freq = 0;
	char cool_name[THERMAL_NAME_LENGTH];
	struct devfreq_frequency_table *table;

	freq_dev = kzalloc(sizeof(struct freq_cool_device),
			      GFP_KERNEL);
	if (!freq_dev)
		return ERR_PTR(-ENOMEM);

	snprintf(cool_name, sizeof(cool_name), "%s-freq-cool", gc_name);
	cool_dev = thermal_cooling_device_register(cool_name, freq_dev,
						   &freq_cooling_ops);
	if (!cool_dev) {
		kfree(freq_dev);
		return ERR_PTR(-EINVAL);
	}
	freq_dev->cool_dev = cool_dev;
	freq_dev->cur_state = 0;
	/* get frequency number */
	if (!strcmp("gc3d", gc_name))
		table = devfreq_frequency_get_table(DEVFREQ_GPU_3D);
	else if (!strcmp("gc2d", gc_name))
		table = devfreq_frequency_get_table(DEVFREQ_GPU_2D);
	else
		table = devfreq_frequency_get_table(DEVFREQ_GPU_SH);

	/* get frequency number */
	for (i = 0; table[i].frequency != DEVFREQ_TABLE_END; i++) {
		if (freq != 0 && descend == -1)
			descend = !!(freq > table[i].frequency);
		freq = table[i].frequency;
		max_level++;
	}

	freq_dev->max_state = max_level - 1;
	/* init freqs */
	freq_dev->freqs = kzalloc(sizeof(unsigned int) * max_level,
			GFP_KERNEL);
	if (!freq_dev->freqs) {
		thermal_cooling_device_unregister(cool_dev);
		kfree(freq_dev);
		return ERR_PTR(-ENOMEM);
	}
	for (i = 0; table[i].frequency != DEVFREQ_TABLE_END; i++) {
		index = descend ? i : (max_level - i - 1);
		freq_dev->freqs[index] = table[i].frequency;
	}

	freq_dev->qos_max.name = "req_thermal";

	if (!strcmp("gc3d", gc_name))
		pm_qos_add_request(&freq_dev->qos_max,
			PM_QOS_GPUFREQ_3D_MAX, INT_MAX);
	else if (!strcmp("gc2d", gc_name))
		pm_qos_add_request(&freq_dev->qos_max,
			PM_QOS_GPUFREQ_2D_MAX, INT_MAX);
	else
		pm_qos_add_request(&freq_dev->qos_max,
			PM_QOS_GPUFREQ_SH_MAX, INT_MAX);

	return cool_dev;
}
EXPORT_SYMBOL_GPL(gpufreq_cool_register);

void gpufreq_cool_unregister(struct thermal_cooling_device *cdev)
{
	struct freq_cool_device *freq_dev = cdev->devdata;

	pm_qos_remove_request(&freq_dev->qos_max);
	thermal_cooling_device_unregister(freq_dev->cool_dev);
	kfree(freq_dev->freqs);
	kfree(freq_dev);
}
EXPORT_SYMBOL_GPL(gpufreq_cool_unregister);
#endif
