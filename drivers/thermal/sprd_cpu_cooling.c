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
#ifdef CONFIG_OF
#include <linux/of_device.h>
#endif

struct thermal_cooling_info_t {
	struct thermal_cooling_device *cdev;
	unsigned long cooling_state;
	struct sprd_cpu_cooling_platform_data *pdata;
	int max_state;
	int enable;
} thermal_cooling_info = {
	.cdev = NULL,
	.cooling_state = 0,
	.enable = 0,
};

static int get_max_state(struct thermal_cooling_device *cdev,
			 unsigned long *state)
{
	int ret = 0;

	*state = thermal_cooling_info.max_state;

	return ret;
}

static int get_cur_state(struct thermal_cooling_device *cdev,
			 unsigned long *state)
{
	int ret = 0;

	*state = thermal_cooling_info.cooling_state;

	return ret;
}

static int thermal_set_vddarm(struct thermal_cooling_info_t *c_info,
		unsigned long state)
{
	int i, j;
	struct vddarm_update *pvddarm = c_info->pdata->vddarm_update;
	struct freq_vddarm *pfreq_vddarm;

	if (pvddarm == NULL){
		printk("%s vddarm_update is NULL\n", __func__);
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
	struct thermal_cooling_info_t *c_info = &thermal_cooling_info;
	int max_core;
	int limit_freq;

	thermal_set_vddarm(c_info, state);

	if (c_info->enable && c_info->cooling_state == state){
		return 0;
	}else{
		c_info->cooling_state = state;
	}

	limit_freq = c_info->pdata->cpu_state[state].max_freq;
	max_core = c_info->pdata->cpu_state[state].max_core;
	c_info->enable = 1;
	pr_info("cpu cooling %s: %d limit_freq: %d kHz max_core: %d\n",
			__func__, state, limit_freq, max_core);
	cpufreq_thermal_limit(0, limit_freq);
	cpu_core_thermal_limit(0, max_core);

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

static int get_cpu_cooling_dt_data(struct device *dev)
{
	struct sprd_cpu_cooling_platform_data *pdata = NULL;
	struct thermal_cooling_info_t *c_info = &thermal_cooling_info;
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
	ret = of_property_read_u32(np, "state_num", &pdata->state_num);
	if(ret){
		dev_err(dev, "fail to get state_num\n");
		goto error;
	}
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
	c_info->max_state = pdata->state_num - 1;
	get_vddarm_updata_dt_data(np, pdata);
	c_info->pdata = pdata;

	return 0;

error:
	kfree(pdata);
	pdata = NULL;
	return -1;
}
#endif

static int sprd_cpu_cooling_probe(struct platform_device *pdev)
{
	struct thermal_cooling_info_t *c_info = &thermal_cooling_info;
	int ret = 0;

#ifdef CONFIG_OF
	ret = get_cpu_cooling_dt_data(&pdev->dev);
	if (ret < 0){
		return -1;
	}
#else
	struct sprd_cpu_cooling_platform_data *pdata = NULL;
	pdata = dev_get_platdata(&pdev->dev);
	if (NULL == pdata){
		dev_err(&pdev->dev, "%s platform data is NULL!\n", __func__);
		return -1;
	}
	c_info->max_state = pdata->state_num - 1;
	c_info->pdata = pdata;
#endif
	c_info->cdev = thermal_cooling_device_register("thermal-cpufreq-0", 0,
						&sprd_cpufreq_cooling_ops);
	if (IS_ERR(c_info->cdev)){
		return PTR_ERR(c_info->cdev);
	}

	return ret;
}

static int sprd_cpu_cooling_remove(struct platform_device *pdev)
{
#ifdef CONFIG_OF
	int i;
	struct thermal_cooling_info_t *c_info = &thermal_cooling_info;

	if (c_info->pdata->vddarm_update){
		for (i = 0; c_info->pdata->vddarm_update[i].freq_vddarm; ++i){
			kfree(c_info->pdata->vddarm_update[i].freq_vddarm);
			c_info->pdata->vddarm_update[i].freq_vddarm = NULL;
		}
		kfree(c_info->pdata->vddarm_update);
	}
	kfree(c_info->pdata);
	c_info->pdata = NULL;
#endif
	thermal_cooling_device_unregister(thermal_cooling_info.cdev);

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
