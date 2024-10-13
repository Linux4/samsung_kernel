/*
 * Copyright (c) 2023 Samsung Electronics Co., Ltd.
 *              http://www.samsung.com
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/of_address.h>
#include <linux/of_platform.h>
#include <linux/sched.h>
#include <linux/fs.h>
#include <linux/debugfs.h>

#if IS_ENABLED(CONFIG_EXYNOS_ESCAV2)
#include "../esca_v2/esca_drv.h"
#else
#include "../acpm/acpm.h"
#endif

#include "power-rail-dbg.h"

static struct exynos_power_rail_dbg_info *dbg_info;
const char** power_rail_name;
const char** dvfs_domain_name;
int domain_name_size, power_rail_name_size;

static ssize_t exynos_power_rail_dbg_read(struct file *file, char __user *user_buf,
		size_t count, loff_t *ppos)
{
	struct acpm_idle_data *data = (struct acpm_idle_data*)dbg_info->idle_data;

	unsigned char volt;
	unsigned int reg_volt, domain_id, index;
	bool is_found;
	int i, j, r = 0;
	char* buf;

	buf = kmalloc(INFO_SIZE, GFP_KERNEL);

	r += sprintf(buf + r,"\n### display_power_rail_minlock_volt:\n");

	for(i = 0; i < power_rail_name_size; i++) {
		domain_id = data->max_voltage[i].req_id;
		volt = data->max_voltage[i].volt;
		reg_volt = volt * RGT_STEP_SIZE;
		is_found = false;
		r += sprintf(buf + r,"[%s]: %d[uV]\n", power_rail_name[i], reg_volt);
		for(j = 0; j < dbg_info->power_rail_info[i].size; j++) {
			index = dbg_info->power_rail_info[i].domain_list[j];
			if(domain_id == index) {
				r += sprintf(buf + r,"\t*%s\n", dvfs_domain_name[index]);
				is_found = true;
			}
			else {
				r += sprintf(buf + r,"\t%s\n", dvfs_domain_name[index]);
			}
		}
		if(!is_found && (domain_id < domain_name_size))
			r += sprintf(buf + r, "\t*%s by voltage minlock\n", dvfs_domain_name[domain_id]);
	}

	r = simple_read_from_buffer(user_buf, count, ppos, buf, r);
	kfree(buf);

	return r;
}

static const struct file_operations exynos_power_rail_dbg_fops = {
	.open = simple_open,
	.read = exynos_power_rail_dbg_read,
	.llseek = default_llseek,
};

static int exynos_power_rail_dbg_parse_dt(struct platform_device *pdev)
{
	struct exynos_power_rail_dbg_info *info = platform_get_drvdata(pdev);
	struct device_node *node, *power_rails_np, *child;
	const __be32 *prop;
	int len, size, ret, i = 0;
	unsigned int offset;

	node = pdev->dev.of_node;
	if(!node) {
		pr_err("%s %s: driver doesn't support non-dt devices\n",
				EXYNOS_POWER_RAIL_DBG_PREFIX, __func__);
		goto err_parse;
	}

	prop = of_get_property(node, "power-rail-offset", &len);
	if(prop) {
		offset = be32_to_cpup(prop);
	}
	else {
		pr_err("%s %s: could not parsing power-rail-offset\n",
				EXYNOS_POWER_RAIL_DBG_PREFIX, __func__);
		goto err_parse;
	}

	info->idle_data = (struct acpm_idle_data *)(get_fvmap_base() + offset);

	power_rail_name_size = of_property_count_strings(node, "power-rail-names");
	if(power_rail_name_size <= 0) {
		pr_err("%s %s: could not parsing power-rail-names size\n",
				EXYNOS_POWER_RAIL_DBG_PREFIX, __func__);
		goto err_parse;
	}

	power_rail_name = kzalloc(sizeof(const char*) * power_rail_name_size, GFP_KERNEL);
	if(!power_rail_name) {
		pr_err("%s %s: could not allocate mem for power-rail-names\n",
				EXYNOS_POWER_RAIL_DBG_PREFIX, __func__);
		goto err_parse;
	}

	ret = of_property_read_string_array(node, "power-rail-names",
			power_rail_name, power_rail_name_size);
	if(ret < 0) {
		pr_err("%s %s: could not parsing power-rail-names\n",
				EXYNOS_POWER_RAIL_DBG_PREFIX, __func__);
		goto err_power_rail_names;
	}

	domain_name_size = of_property_count_strings(node, "dvfs-domain-names");
	if(domain_name_size <= 0) {
		pr_err("%s %s: could not parsing dvfs-domain-names size\n",
				EXYNOS_POWER_RAIL_DBG_PREFIX, __func__);
		goto err_power_rail_names;
	}

	dvfs_domain_name = kzalloc(sizeof(const char*) * domain_name_size, GFP_KERNEL);
	if(!dvfs_domain_name) {
		pr_err("%s %s: could not allocate mem for dvfs_domain_name\n",
				EXYNOS_POWER_RAIL_DBG_PREFIX, __func__);
		goto err_power_rail_names;
	}

	ret = of_property_read_string_array(node, "dvfs-domain-names",
			dvfs_domain_name, domain_name_size);
	if(ret < 0) {
		pr_err("%s %s: could not parsing dvfs-domain-names\n",
				EXYNOS_POWER_RAIL_DBG_PREFIX, __func__);
		goto err_dvfs_domain_names;
	}

	power_rails_np = of_find_node_by_name(node, "power-rails");
	size = of_get_child_count(power_rails_np);
	info->power_rail_info = kzalloc(sizeof(struct exynos_power_rail_info) * size, GFP_KERNEL);

	for_each_available_child_of_node(power_rails_np, child) {
		struct exynos_power_rail_info *rail_info = &info->power_rail_info[i++];

		rail_info->size = ret = of_property_count_u32_elems(child, "domain_id");
		if(size < 0) {
			pr_err("%s %s: could not get domain_id size\n",
					EXYNOS_POWER_RAIL_DBG_PREFIX, __func__);
			goto err_dvfs_domain_names;
		}

		rail_info->domain_list = kzalloc(sizeof(unsigned int) * rail_info->size, GFP_KERNEL);
		if(of_property_read_u32_array(child, "domain_id", rail_info->domain_list, rail_info->size)){
			pr_err("%s %s: could not parsing domain_id in child node\n",
					EXYNOS_POWER_RAIL_DBG_PREFIX, __func__);
			goto err_dvfs_domain_names;

		}
	}

	return 1;
err_dvfs_domain_names:
	kfree(dvfs_domain_name);
err_power_rail_names:
	kfree(power_rail_name);
err_parse:
	return 0;
}

static int exynos_power_rail_dbg_probe(struct platform_device *pdev)
{
	int ret;
	struct exynos_power_rail_dbg_info *info;
	struct dentry *exynos_power_rail_dbg_root;

	info = kzalloc(sizeof(struct exynos_power_rail_dbg_info), GFP_KERNEL);
	if(!info) {
		pr_err("%s %s: could not allocate mem for info\n",
				EXYNOS_POWER_RAIL_DBG_PREFIX, __func__);
		ret = -ENOMEM;
		goto err_dbg_info;
	}
	info->dev = &pdev->dev;
	dbg_info = info;
	platform_set_drvdata(pdev, info);

	ret = exynos_power_rail_dbg_parse_dt(pdev);
	if(!ret) {
		pr_err("%s %s: could not parse dt\n",
				EXYNOS_POWER_RAIL_DBG_PREFIX, __func__);
		goto err_dbgfs_root;
	}

	exynos_power_rail_dbg_root = debugfs_create_dir("exynos-power-rail", NULL);
	if(!exynos_power_rail_dbg_root) {
		pr_err("%s %s: could not created debugfs dir\n",
				EXYNOS_POWER_RAIL_DBG_PREFIX, __func__);
		ret = -ENOMEM;
		goto err_dbgfs_root;
	}

	info->fops = exynos_power_rail_dbg_fops;
	info->d = debugfs_create_file("power-rail-dbg", 0644,
			exynos_power_rail_dbg_root, info->dev, &info->fops);
	if(!info->d) {
		pr_err("%s %s: could not created debugfs file\n",
				EXYNOS_POWER_RAIL_DBG_PREFIX, __func__);
		ret = -ENOMEM;
		goto err_dbgfs_power_rail;
	}
	pr_info("power rail driver probe done\n");
	return 0;
err_dbgfs_power_rail:
	debugfs_remove_recursive(exynos_power_rail_dbg_root);
	kfree(power_rail_name);
	kfree(dvfs_domain_name);
err_dbgfs_root:
	kfree(info);
err_dbg_info:
	return 0;
}

static int exynos_power_rail_dbg_remove(struct platform_device *pdev)
{
	kfree(dbg_info);
	kfree(power_rail_name);
	kfree(dvfs_domain_name);
	return 0;
}

static const struct of_device_id exynos_power_rail_dbg_match[] = {
	{
		.compatible = "samsung,exynos-power-rail-dbg",
	},
	{},
};

static struct platform_driver exynos_power_rail_dbg_drv = {
	.probe		= exynos_power_rail_dbg_probe,
	.remove		= exynos_power_rail_dbg_remove,
	.driver		= {
		.name	= "exynos_power_rail_dbg",
		.owner	= THIS_MODULE,
		.of_match_table = exynos_power_rail_dbg_match,
	},
};

int exynos_power_rail_dbg_init(void)
{
	return platform_driver_register(&exynos_power_rail_dbg_drv);
}

MODULE_LICENSE("GPL");
