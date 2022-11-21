/*
 *  linux/drivers/thermal/sprd_cpu_cooling.c
 *
 * ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; version 2 of the License.
 *
 *  This program is distributed in the hope that it will be useful, but
 *  WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License along
 *  with this program; if not, write to the Free Software Foundation, Inc.,
 *  59 Temple Place, Suite 330, Boston, MA 02111-1307 USA.
 *
 * ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
 */

#include <linux/kernel.h>
#include <linux/slab.h>
#include <linux/err.h>
#include <linux/types.h>
#include <linux/thermal.h>
#include <linux/sprd_cpu_cooling.h>
#include <linux/platform_device.h>
#include <linux/cpumask.h>
#ifdef CONFIG_OF
#include <linux/of_device.h>
#endif

#define SPRDCPU_DEBUG__
#ifdef SPRDCPU_DEBUG__
#define SPRDCPU_DEBUG(format, arg...) pr_info("cpu-cooling: " format, ## arg)
#else
#define SPRDCPU_DEBUG(format, arg...)
#endif

struct cpu_cooling_param_t {
	int state;
	int max_core;
	int limit_freq;
	int low_freq;
	int low_vol;
};

struct thermal_cooling_info_t {
	unsigned long cooling_state;
	struct thermal_cooling_device *cdev;
	struct sprd_cpu_cooling_platform_data *pdata;
	int max_state;
	int enable;
	int cluster;
	struct cpu_cooling_param_t param;
#if defined(CONFIG_ARCH_SCX35L) && !defined(CONFIG_ARCH_SCX35LT8)
	int binning;
#endif
};

static ssize_t sprd_cpu_store_caliberate(struct device *dev,
					struct device_attribute *attr,
					const char *buf, size_t count);
static ssize_t sprd_cpu_show_caliberate(struct device *dev,
				       struct device_attribute *attr,
				       char *buf);

/* sys I/F for cooling device */
#define to_cooling_device(_dev)	\
		container_of(_dev, struct thermal_cooling_device, device)

#define SPRD_CPU_CALIBERATE_ATTR(_name)                         \
{                                       \
	.attr = { .name = #_name, .mode = S_IRUGO | S_IWUSR | S_IWGRP,},  \
	.show = sprd_cpu_show_caliberate,                  \
	.store = sprd_cpu_store_caliberate,                              \
}
#define SPRD_CPU_CALIBERATE_ATTR_RO(_name)                         \
{                                       \
	.attr = { .name = #_name, .mode = S_IRUGO, },  \
	.show = sprd_cpu_show_caliberate,                  \
}
#define SPRD_CPU_CALIBERATE_ATTR_WO(_name)                         \
{                                       \
	.attr = { .name = #_name, .mode = S_IWUSR | S_IWGRP, },  \
	.store = sprd_cpu_store_caliberate,                              \
}

static struct device_attribute sprd_cpu_caliberate[] = {
	SPRD_CPU_CALIBERATE_ATTR(cur_ctrl_param),
};

static int sprd_cpu_creat_caliberate_attr(struct device *dev)
{
	int i, rc;

	for (i = 0; i < ARRAY_SIZE(sprd_cpu_caliberate); i++) {
		rc = device_create_file(dev, &sprd_cpu_caliberate[i]);
		if (rc)
			goto sprd_attrs_failed;
	}
	goto sprd_attrs_succeed;

sprd_attrs_failed:
	while (i--)
		device_remove_file(dev, &sprd_cpu_caliberate[i]);

sprd_attrs_succeed:
	return rc;
}

static int sprd_cpu_remove_caliberate_attr(struct device *dev)
{
	int i;

	for (i = 0; i < ARRAY_SIZE(sprd_cpu_caliberate); i++) {
		device_remove_file(dev, &sprd_cpu_caliberate[i]);
	}
	return 0;
}


static ssize_t sprd_cpu_show_caliberate(struct device *dev,
				       struct device_attribute *attr, char *buf)
{
	int i = 0;
	int offset = 0;
	struct thermal_cooling_device *cdev = to_cooling_device(dev);
	struct thermal_cooling_info_t *info = cdev->devdata;
	unsigned long *data = &info->param;
	int len = sizeof(info->param) / sizeof(info->param.state);

	for (i = 0; i < len; i++)
		offset += sprintf(buf + offset, "%ld,", data[i]);
	buf[offset - 1] = '\n';

	SPRDCPU_DEBUG("dev_attr_cur_ctrl_param: %s\n", buf);
	return offset;
}

#define BUF_LEN	64
static ssize_t sprd_cpu_store_caliberate(struct device *dev,
					struct device_attribute *attr,
					const char *buf, size_t count)
{
	int i = 0;
	char *str = NULL;
	char *pbuf;
	char *after = NULL;
	char buffer[BUF_LEN];
	struct thermal_cooling_device *cdev = to_cooling_device(dev);
	struct thermal_cooling_info_t *info = cdev->devdata;
	int *data = &info->param;
	int len = sizeof(info->param) / sizeof(info->param.state);

	if (count > BUF_LEN){
		count = BUF_LEN;
	}
	strncpy(buffer, buf, count);
	buffer[count] = '\0';
	printk("buf param form usespace:%s\n", buf);
	printk("buffer parm:%s\n", buffer);
	pbuf = buffer;
	while ((pbuf!= NULL) && (i < len))
	{
		str = strsep(&pbuf, ",");
		data[i++] = simple_strtoul(str, &after, 0);
	}
	printk("sprd_cpu_cooling_param_t.status: %d\n", info->param.state);
	printk("sprd_cpu_cooling_param_t.core_num: %d\n", info->param.max_core);
	printk("sprd_cpu_cooling_param_t.freq: %d\n", info->param.limit_freq);
	printk("sprd_cpu_cooling_param_t.low_freq: %d\n", info->param.low_freq);
	printk("sprd_cpu_cooling_param_t.low_vol: %d\n", info->param.low_vol);

	cpufreq_thermal_limit(info->cluster, info->param.limit_freq);
	cpu_core_thermal_limit(info->cluster, info->param.max_core);
#if defined(CONFIG_ARCH_SCX35L) && !defined(CONFIG_ARCH_SCX35LT8)
	if (info->binning == 0){
#endif
        if (info->param.low_freq > 0 && info->param.low_vol > 0){
	        cpufreq_table_thermal_update(info->param.low_freq, info->param.low_vol);
		info->param.low_freq = 0;
		info->param.low_vol = 0;
        }
#if defined(CONFIG_ARCH_SCX35L) && !defined(CONFIG_ARCH_SCX35LT8)
	}
#endif

	return count;
}

#if !defined(CONFIG_SPRD_CPU_DYNAMIC_HOTPLUG) && !defined(CONFIG_CPU_FREQ_GOV_SPRDEMAND)
int cpu_core_thermal_limit(int cluster, int max_core)
{
#ifdef CONFIG_HOTPLUG_CPU
	int cpuid, cpus;
	int first_cpu, last_cpu;

	if (cluster){
		first_cpu = NR_CPUS / 2;
		last_cpu = NR_CPUS - 1;
	}else{
		first_cpu = 0;
#ifdef CONFIG_SCHED_HMP
		last_cpu = NR_CPUS / 2 -1;
#else
		last_cpu = NR_CPUS - 1;
#endif
	}
	for (cpus = 0, cpuid = first_cpu; cpuid <= last_cpu; ++cpuid){
		if (cpu_online(cpuid)){
			cpus++;
		}
	}
	if (max_core == cpus){
		return 0;
	}
	if (cpus < max_core){
		/* plug cpu */
		for (cpuid = first_cpu; cpuid <= last_cpu; ++cpuid){
			if (cpu_online(cpuid)){
				continue;
			}
			pr_info("cpu-cooling: we gonna plugin cpu%d  !!\n", cpuid);
			if (cpu_up(cpuid)){
				pr_info("plug cpu%d failed!\n", cpuid);
			}
			if (++cpus >= max_core){
				return 0;
			}
		}
	}else{
		/* unplug cpu */
		for (cpuid = last_cpu; cpuid >= first_cpu; --cpuid){
			if (!cpu_online(cpuid)){
				continue;
			}
			pr_info("cpu-cooling: we gonna unplug cpu%d  !!\n", cpuid);
			if (cpu_down(cpuid)){
				pr_info("unplug cpu%d failed!\n", cpuid);
			}
			if (--cpus <= max_core){
				return 0;
			}
		}
	}
#endif

	return 0;
}
#endif

static int get_max_state(struct thermal_cooling_device *cdev,
			 unsigned long *state)
{
	struct thermal_cooling_info_t *info = cdev->devdata;
	int ret = 0;

	*state = info->max_state;

	return ret;
}

static int get_cur_state(struct thermal_cooling_device *cdev,
			 unsigned long *state)
{
	struct thermal_cooling_info_t *info = cdev->devdata;
	int ret = 0;

	*state = info->cooling_state;

	return ret;
}

static int thermal_set_vddarm(struct thermal_cooling_info_t *c_info,
		unsigned long state)
{
	int i, j;
	struct vddarm_update *pvddarm = c_info->pdata->vddarm_update;
	struct freq_vddarm *pfreq_vddarm;

	if (pvddarm == NULL){
		printk("%s vddarm_update isn't configed.\n", __func__);
		return -1;
	}
	for (i = 0; pvddarm[i].freq_vddarm; ++i){
		if (state != pvddarm[i].state){
			continue;
		}
		pfreq_vddarm = pvddarm[i].freq_vddarm;
		for (j = 0; pfreq_vddarm[j].freq; ++j){
			cpufreq_table_thermal_update(pfreq_vddarm[j].freq,
					pfreq_vddarm[j].vddarm_mv);
		}
	}

	return 0;
}

static int set_cur_state(struct thermal_cooling_device *cdev,
			 unsigned long state)
{
	struct thermal_cooling_info_t *c_info = cdev->devdata;
	int max_core;
	int limit_freq;

	if (c_info->max_state <= 0){
		return 0;
	}
	thermal_set_vddarm(c_info, state);

	if (c_info->enable && c_info->cooling_state == state){
		return 0;
	}else{
		c_info->cooling_state = state;
	}

	limit_freq = c_info->pdata->cpu_state[state].max_freq;
	max_core = c_info->pdata->cpu_state[state].max_core;
	c_info->enable = 1;
	pr_info("%s %s: %d limit_freq: %d kHz max_core: %d\n",
			cdev->type, __func__, state, limit_freq, max_core);
	cpufreq_thermal_limit(c_info->cluster, limit_freq);
	cpu_core_thermal_limit(c_info->cluster, max_core);

	return 0;
}

static struct thermal_cooling_device_ops sprd_cpufreq_cooling_ops = {
	.get_max_state = get_max_state,
	.get_cur_state = get_cur_state,
	.set_cur_state = set_cur_state,
};

#ifdef CONFIG_OF
static int get_vddarm_updata_dt_data(struct device_node *np,
		struct sprd_cpu_cooling_platform_data *pdata)
{
	struct vddarm_update *pvddarm;
	int vddarm_nr[MAX_CPU_STATE];
	char name[32];
	int data[32];
	int freq_nr;
	int i, j, k, ret;

	ret = of_property_read_u32_array(np, "vddarm_nr", vddarm_nr, 1);
	if(ret){
		printk(KERN_ERR "fail to get vddarm_nr\n");
		goto error;
	}

	ret = of_property_read_u32_array(np, "vddarm_nr", vddarm_nr, vddarm_nr[0] + 1);
	if (ret){
		printk(KERN_ERR "fail to get all vddarm_nr data.\n");
		goto error;
	}
	printk("%s vddarm_nr:<", __func__);
	for (i = 0; i < vddarm_nr[0]; ++i){
		printk("%d ", vddarm_nr[i + 1]);
	}
	printk(">\n");

	pvddarm = kzalloc(sizeof(*pvddarm) * (vddarm_nr[0] + 1), GFP_KERNEL);
	if (pvddarm == NULL){
		goto error;
	}

	for (i = 0; i < vddarm_nr[0]; ++i){
		sprintf(name, "vddarm_update%d", i);
		ret = of_property_read_u32_array(np, name, data, vddarm_nr[i + 1]);
		if(ret){
			printk(KERN_ERR "fail to get %s len:%d\n", name, vddarm_nr[i + 1]);
			goto vddarm_err;
		}
		freq_nr = (vddarm_nr[i + 1] - 1) / 2;
		pvddarm[i].freq_vddarm = kzalloc(sizeof(struct freq_vddarm) * (freq_nr + 1), GFP_KERNEL);
		if (pvddarm[i].freq_vddarm == NULL){
			goto vddarm_err;
		}
		printk("%s state: <", __func__);
		for (j = 0; j < vddarm_nr[i + 1]; ++j){
			printk("%d ", data[j]);
		}
		printk(">\n");
		pvddarm[i].state = data[0];
		for (j = 0, k = 1; j < freq_nr; ++j){
			pvddarm[i].freq_vddarm[j].freq = data[k++];
			pvddarm[i].freq_vddarm[j].vddarm_mv = data[k++];
		}
		pvddarm[i].freq_vddarm[j].freq = 0;
	}
	pvddarm[i].freq_vddarm = NULL;
	pdata->vddarm_update = pvddarm;

	return 0;
vddarm_err:
	for (j = 0; j < i; ++j){
		if (pvddarm[j].freq_vddarm){
			kfree(pvddarm[j].freq_vddarm);
			pvddarm[j].freq_vddarm = NULL;
		}
	}
	kfree(pvddarm);
error:
	pdata->vddarm_update = NULL;
	return -1;
}

static struct sprd_cpu_cooling_platform_data *get_cpu_cooling_dt_data(
		struct device *dev, struct thermal_cooling_info_t *info)
{
	struct sprd_cpu_cooling_platform_data *pdata = NULL;
	struct device_node *np = dev->of_node;
	int max_freq[MAX_CPU_STATE];
	int max_core[MAX_CPU_STATE];
	int ret, i;

	if (!np) {
		dev_err(dev, "device node not found\n");
		return -EINVAL;
	}
	pdata = kzalloc(sizeof(*pdata), GFP_KERNEL);
	if (!pdata) {
		dev_err(dev, "could not allocate memory for platform data\n");
		return -1;
	}
	ret = of_property_read_u32(np, "cluster", &info->cluster);
	if(ret){
		info->cluster = 0;
	}
	dev_info(dev, "cluster: %s\n", info->cluster ? "big" : "little");
	ret = of_property_read_u32(np, "state_num", &pdata->state_num);
	if(ret || pdata->state_num <= 0){
		dev_info(dev, "no state_num dts config, Don't use kernel thermal policy.\n");
		pdata->state_num = 0;
		info->max_state = 0;
		goto done;
	}
	dev_info(dev, "state_num=%d\n", pdata->state_num);
	ret = of_property_read_u32_array(np, "max_freq", max_freq, pdata->state_num);
	if(ret){
		dev_err(dev, "fail to get max_freq\n");
		goto error;
	}
	ret = of_property_read_u32_array(np, "max_core", max_core, pdata->state_num);
	if(ret){
		dev_err(dev, "fail to get max_core\n");
		goto error;
	}
	for (i = 0; i < pdata->state_num; ++i){
		pdata->cpu_state[i].max_freq = max_freq[i];
		pdata->cpu_state[i].max_core = max_core[i];
		dev_info(dev, "state:%d, max_freq:%d, max_core:%d\n", i, max_freq[i], max_core[i]);
	}
	info->max_state = pdata->state_num - 1;
done:
	get_vddarm_updata_dt_data(np, pdata);

	return pdata;

error:
	kfree(pdata);
	pdata = NULL;
	return pdata;
}
#endif

#if defined(CONFIG_ARCH_SCX35L) && !defined(CONFIG_ARCH_SCX35LT8)
extern int sci_efuse_Dhryst_binning_get(int *val);
#endif
static int sprd_cpu_cooling_probe(struct platform_device *pdev)
{
	struct thermal_cooling_info_t *info = NULL;
	struct sprd_cpu_cooling_platform_data *pdata = NULL;
	char cdev_name[32];
	int ret = 0;
#if defined(CONFIG_ARCH_SCX35L) && !defined(CONFIG_ARCH_SCX35LT8)
	int val = 0;
#endif

	if (NULL == pdev){
		return -1;
	}
	info = kzalloc(sizeof(*info), GFP_KERNEL);
	if (!info) {
		dev_err(&pdev->dev, "could not allocate memory for info data\n");
		return -1;
	}
#ifdef CONFIG_OF
	ret = of_property_read_u32(pdev->dev.of_node, "id", &pdev->id);
	if(ret || pdev->id < 0){
		pdev->id = 0;
	}
	pdata = get_cpu_cooling_dt_data(&pdev->dev, info);
	if (!pdata){
		ret = -1;
		goto err;
	}
	pdev->dev.platform_data = pdata;
#else
	pdata = dev_get_platdata(&pdev->dev);
	if (NULL == pdata){
		dev_err(&pdev->dev, "%s platform data is NULL!\n", __func__);
		ret = -1;
		goto err;
	}
	info->max_state = pdata->state_num - 1;
#endif

	pdata->devdata = info;
	info->pdata = pdata;
	info->cooling_state = 0,
	info->enable = 0,

	info->param.state = 0;
	info->param.max_core = 0;
	info->param.limit_freq = 0;
	info->param.low_freq = 0;
	info->param.low_vol = 0;
#if defined(CONFIG_ARCH_SCX35L) && !defined(CONFIG_ARCH_SCX35LT8)
	sci_efuse_Dhryst_binning_get(&val);
	dev_info(&pdev->dev, "sci_efuse_Dhryst_binning_get--val : %d\n", val);
	if ((val < 28) && (val >= 22)){
		info->binning = 1;
	}else{
		info->binning = 0;
	}
	dev_info(&pdev->dev, "binning : %d\n", info->binning);
#endif

	sprintf(cdev_name, "thermal-cpufreq-%d", pdev->id);
	info->cdev = thermal_cooling_device_register(cdev_name, info,
						&sprd_cpufreq_cooling_ops);
	if (IS_ERR(info->cdev)){
		ret = PTR_ERR(info->cdev);
		goto err;
	}

	sprd_cpu_creat_caliberate_attr(&info->cdev->device);

	return ret;
err:
	kfree(info);
	return ret;
}

static int sprd_cpu_cooling_remove(struct platform_device *pdev)
{
	struct sprd_cpu_cooling_platform_data *pdata = dev_get_platdata(&pdev->dev);
	struct thermal_cooling_info_t *info = pdata->devdata;

	sprd_cpu_remove_caliberate_attr(&info->cdev->device);

#ifdef CONFIG_OF
	int i;

	if (pdata->vddarm_update){
		for (i = 0; pdata->vddarm_update[i].freq_vddarm; ++i){
			kfree(pdata->vddarm_update[i].freq_vddarm);
			pdata->vddarm_update[i].freq_vddarm = NULL;
		}
		kfree(pdata->vddarm_update);
	}
	kfree(pdata);
	pdata = NULL;
#endif
	thermal_cooling_device_unregister(info->cdev);
	kfree(info);

	return 0;
}

#ifdef CONFIG_OF
static const struct of_device_id cpu_cooling_of_match[] = {
       { .compatible = "sprd,sprd-cpu-cooling", },
       { }
};
#endif

static struct platform_driver cpu_cooling_driver = {
	.probe = sprd_cpu_cooling_probe,
	.remove = sprd_cpu_cooling_remove,
	.driver = {
		.owner = THIS_MODULE,
		.name = "sprd-cpu-cooling",
#ifdef CONFIG_OF
		.of_match_table = of_match_ptr(cpu_cooling_of_match),
#endif
	},
};


static int __init sprd_cpu_cooling_init(void)
{
	return platform_driver_register(&cpu_cooling_driver);
}

static void __exit sprd_cpu_cooling_exit(void)
{
	platform_driver_unregister(&cpu_cooling_driver);
}

late_initcall(sprd_cpu_cooling_init);
module_exit(sprd_cpu_cooling_exit);
