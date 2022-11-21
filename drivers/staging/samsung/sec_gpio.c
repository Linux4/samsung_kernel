/*
 * Copyright (c) 2018 Samsung Electronics Co., Ltd.
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

#include <linux/module.h>
#include <linux/fs.h>
#include <linux/proc_fs.h>
#include <linux/seq_file.h>
#include <linux/gpio.h>
#include <linux/of_gpio.h>
#include <linux/of.h>

int subfpcb_gpio = -1;

static int get_subfpcb_gpio(void)
{
	struct device_node *np;

	np = of_find_node_by_path("/fpcb_type");
	if (!np)
	{
		pr_err("%s: there's no fpcb_type node on dtb\n", __func__);
		return -1;
	}

	subfpcb_gpio = of_get_gpio(np, 0);
	
	of_node_put(np);

	if (!gpio_is_valid(subfpcb_gpio)) {
		pr_err("%s: could not find fpcb gpio key node\n", __func__);
		subfpcb_gpio = -1;
		
		return -1;
	}

	pr_info("%s: sub fpcb gpio is %d\n", __func__, subfpcb_gpio);

	return 0;	
}

static int check_subfpcb_type(struct seq_file *m, void *v)
{
	int retval = -1;

	if (!gpio_is_valid(subfpcb_gpio))
		goto out;

	retval = gpio_get_value(subfpcb_gpio);
	pr_info("%s: fpcb gpio value is %d\n", __func__, retval);

out:
	seq_printf(m, "%u\n", retval);

	return 0;
}

static int check_subfpcb_type_open(struct inode *inode, struct file *file)
{
	return single_open(file, check_subfpcb_type, NULL);
}

static const struct file_operations check_subfpcb_type_fops = {
	.open	= check_subfpcb_type_open,
	.read	= seq_read,
	.llseek	= seq_lseek,
	.release= single_release,
};

static int __init sec_gpio_detect_init(void)
{
	struct proc_dir_entry *entry;

	pr_info("%s start\n", __func__);

	entry = proc_create("subfpcb_type", S_IRUGO, NULL, &check_subfpcb_type_fops);
	if (!entry)
	{
		pr_err("%s: failed to create a proc fs node\n", __func__);
		return -ENOMEM;
	}

	if (get_subfpcb_gpio())
	{
		pr_err("%s: failed to get gpio node\n", __func__);
		return -ENOMEM;
	}

	return 0;
}

module_init(sec_gpio_detect_init);
