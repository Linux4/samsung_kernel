/*

 * Copyright (c) 2021-2022 Samsung Electronics Co., Ltd.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.


*/

#include <linux/proc_fs.h>
#include <linux/of_gpio.h>
#include <linux/of.h>

enum sim_mode {
	SIM_NONE = 0,
	SIM_SINGLE,
	SIM_DUAL,
	SIM_TRIPLE,
};

static int simslot_count(struct seq_file *m, void *v)
{
	enum sim_mode mode = (enum sim_mode)m->private;

	seq_printf(m, "%u\n", mode);
	return 0;
}

static int simslot_count_open(struct inode *inode, struct file *file)
{
	return single_open(file, simslot_count, PDE_DATA(inode));
}

static const struct file_operations simslot_count_fops = {
	.open	= simslot_count_open,
	.read	= seq_read,
	.llseek	= seq_lseek,
	.release = single_release,
};

static enum sim_mode get_sim_mode()
{
	struct device_node *np = NULL;
	enum sim_mode mode = SIM_SINGLE;
	int gpio_ds_det;
	int retval;

	np = of_find_compatible_node(NULL, NULL, "sim_type");

	gpio_ds_det = of_get_named_gpio(np, "qcom,sim_type", 0);
	if (!gpio_is_valid(gpio_ds_det)) {
		pr_err("DT error: failed to get sim mode\n");
		goto make_proc;
	}

	retval = gpio_request(gpio_ds_det, "sim_type");
	if (retval) {
		pr_err("Failed to reqeust GPIO(%d)\n", retval);
		goto make_proc;
	} else {
		gpio_direction_input(gpio_ds_det);
	}

	retval = gpio_get_value(gpio_ds_det);
	if (!retval)
		mode = SIM_DUAL;

	pr_err("sim_mode: %d\n", mode);
	gpio_free(gpio_ds_det);

make_proc:
	if (!proc_create_data("simslot_count", 0, NULL, &simslot_count_fops,
			(void *)(long)mode)) {
		pr_err("Failed to create proc\n");
		mode = SIM_SINGLE;
	}

	return mode;
}

static int __init simslot_count_init(void)
{
	get_sim_mode();
	return 0;
}

late_initcall(simslot_count_init);
