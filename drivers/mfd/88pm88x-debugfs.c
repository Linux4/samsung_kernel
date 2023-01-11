/*
 * Copyright (C) Marvell 2014
 *
 * Author: Yi Zhang <yizhang@marvell.com>
 * License Terms: GNU General Public License v2
 */
/*
 * 88pm88x register access
 *
 * read:
 * # echo -0x[page] 0x[reg] > <debugfs>/pm88x/compact_addr
 * # cat <debugfs>/pm88x/page-address   ---> get page address
 * # cat <debugfs>/pm88x/register-value ---> get register address
 * or:
 * # echo page > <debugfs>/pm88x/page-address
 * # echo addr > <debugfs>/pm88x/register-address
 * # cat <debugfs>/pm88x/register-value
 *
 * write:
 * # echo -0x[page] 0x[reg] > <debugfs>/pm88x/compact_addr
 * # echo value > <debugfs>/pm88x/register-value ---> set value
 * or:
 * # echo page > <debugfs>/pm88x/page-address
 * # echo addr > <debugfs>/pm88x/register-address
 * # cat <debugfs>/pm88x/register-value
 *
 */

#include <linux/seq_file.h>
#include <linux/uaccess.h>
#include <linux/fs.h>
#include <linux/module.h>
#include <linux/debugfs.h>
#include <linux/platform_device.h>
#include <linux/interrupt.h>
#include <linux/kobject.h>
#include <linux/slab.h>
#include <linux/irq.h>

#include <linux/mfd/88pm88x.h>
#include <linux/mfd/88pm886.h>

#ifdef CONFIG_DEBUG_FS
#include <linux/string.h>
#include <linux/ctype.h>
#endif

#define PM88X_NAME		"88pm88x"
#define PM88X_PAGES_NUM		(0x8)

static struct dentry *pm88x_dir;
static u8 debug_page_addr, debug_reg_addr, debug_reg_val;

static int pm88x_reg_addr_print(struct seq_file *s, void *p)
{
	return seq_printf(s, "0x%02x\n", debug_reg_addr);
}

static int pm88x_reg_addr_open(struct inode *inode, struct file *file)
{
	return single_open(file, pm88x_reg_addr_print, inode->i_private);
}

static ssize_t pm88x_reg_addr_write(struct file *file,
				     const char __user *user_buf,
				     size_t count, loff_t *ppos)
{
	struct pm88x_chip *chip = file->private_data;
	unsigned long user_reg;
	int err;

	err = kstrtoul_from_user(user_buf, count, 0, &user_reg);
	if (err)
		return err;

	/* FIXME: suppose it's 0xff first */
	if (user_reg >= 0xff) {
		dev_err(chip->dev, "debugfs error input > number of pages\n");
		return -EINVAL;
	}
	debug_reg_addr = user_reg;

	return count;
}

static const struct file_operations pm88x_reg_addr_fops = {
	.open = pm88x_reg_addr_open,
	.write = pm88x_reg_addr_write,
	.read = seq_read,
	.release = single_release,
	.owner = THIS_MODULE,
};

static int pm88x_page_addr_print(struct seq_file *s, void *p)
{
	return seq_printf(s, "0x%02x\n", debug_page_addr);
}

static int pm88x_page_addr_open(struct inode *inode, struct file *file)
{
	return single_open(file, pm88x_page_addr_print, inode->i_private);
}

static ssize_t pm88x_page_addr_write(struct file *file,
				     const char __user *user_buf,
				     size_t count, loff_t *ppos)
{
	struct pm88x_chip *chip =
		((struct seq_file *)(file->private_data))->private;
	u8 user_page;
	int err;

	err = kstrtou8_from_user(user_buf, count, 0, &user_page);
	if (err)
		return err;

	if (user_page >= PM88X_PAGES_NUM) {
		dev_err(chip->dev, "debugfs error input > number of pages\n");
		return -EINVAL;
	}
	switch (user_page) {
	case 0:
	case 1:
	case 2:
	case 3:
	case 4:
	case 7:
		pr_info("set page number as: 0x%x\n", user_page);
		break;
	default:
		pr_info("wrong page number: 0x%x\n", user_page);
		return -EINVAL;
	}

	debug_page_addr = user_page;

	return count;
}

static const struct file_operations pm88x_page_addr_fops = {
	.open = pm88x_page_addr_open,
	.write = pm88x_page_addr_write,
	.read = seq_read,
	.release = single_release,
	.owner = THIS_MODULE,
};

static int pm88x_reg_val_get(struct seq_file *s, void *p)
{
	struct pm88x_chip *chip = s->private;
	int err;
	struct regmap *map;
	unsigned int user_val;

	dev_info(chip->dev, "--->page: 0x%02x, reg: 0x%02x\n",
		 debug_page_addr, debug_reg_addr);
	switch (debug_page_addr) {
	case 0:
		map = chip->base_regmap;
		break;
	case 1:
		map = chip->ldo_regmap;
		break;
	case 2:
		map = chip->gpadc_regmap;
		break;
	case 3:
		map = chip->battery_regmap;
		break;
	case 4:
		map = chip->buck_regmap;
		break;
	case 7:
		map = chip->test_regmap;
		break;
	default:
		pr_err("unsupported pages.\n");
		return -EINVAL;
	}

	err = regmap_read(map, debug_reg_addr, &user_val);
	if (err < 0) {
		dev_err(chip->dev, "read register fail.\n");
		return -EINVAL;
	}
	debug_reg_val = user_val;

	seq_printf(s, "0x%02x\n", debug_reg_val);
	return 0;
}

static int pm88x_reg_val_open(struct inode *inode, struct file *file)
{
	return single_open(file, pm88x_reg_val_get, inode->i_private);
}

static ssize_t pm88x_reg_val_write(struct file *file,
				const char __user *user_buf,
				size_t count, loff_t *ppos)
{
	struct pm88x_chip *chip = ((struct seq_file *)(file->private_data))->private;
	unsigned long user_val;
	static struct regmap *map;
	int err;

	err = kstrtoul_from_user(user_buf, count, 0, &user_val);
	if (err)
		return err;

	if (user_val > 0xff) {
		dev_err(chip->dev, "debugfs error input > 0xff\n");
		return -EINVAL;
	}

	switch (debug_page_addr) {
	case 0:
		map = chip->base_regmap;
		break;
	case 1:
		map = chip->ldo_regmap;
		break;
	case 2:
		map = chip->gpadc_regmap;
		break;
	case 3:
		map = chip->battery_regmap;
		break;
	case 4:
		map = chip->buck_regmap;
		break;
	case 7:
		map = chip->test_regmap;
		break;
	default:
		pr_err("unsupported pages.\n");
		return -EINVAL;
	}

	err = regmap_write(map, debug_reg_addr, user_val);
	if (err < 0) {
		dev_err(chip->dev, "write register fail.\n");
		return -EINVAL;
	}

	debug_reg_val = user_val;
	return count;
}

static const struct file_operations pm88x_reg_val_fops = {
	.open = pm88x_reg_val_open,
	.read = seq_read,
	.write = pm88x_reg_val_write,
	.llseek = seq_lseek,
	.release = single_release,
	.owner = THIS_MODULE,
};

static ssize_t pm88x_compact_addr_write(struct file *file,
				const char __user *user_buf,
				size_t count, loff_t *ppos)
{
	struct pm88x_chip *chip = file->private_data;
	int err, index1, index2;

	char msg[20] = { 0 };
	err = copy_from_user(msg, user_buf, count);
	if (err)
		return err;
	err = sscanf(msg, "-0x%x 0x%x", &index1, &index2);
	if (err != 2) {
		dev_err(chip->dev,
			"right format: -0x[page_addr] 0x[reg_addr]\n");
		return count;
	}
	debug_page_addr = (u8)index1;
	debug_reg_addr = (u8)index2;

	return count;
}

static const struct file_operations pm88x_compact_addr_fops = {
	.open = simple_open,
	.write = pm88x_compact_addr_write,
	.owner = THIS_MODULE,
};

static int pm88x_registers_print(struct pm88x_chip *chip, u8 page,
			     struct seq_file *s)
{
	static u8 reg;
	static struct regmap *map;
	static int value;

	switch (page) {
	case 0:
		map = chip->base_regmap;
		break;
	case 1:
		map = chip->power_regmap;
		break;
	case 2:
		map = chip->gpadc_regmap;
		break;
	case 3:
		map = chip->battery_regmap;
		break;
	case 4:
		map = chip->buck_regmap;
		break;
	case 7:
		map = chip->test_regmap;
		break;
	default:
		pr_err("unsported pages.\n");
		return -EINVAL;
	}

	for (reg = 0; reg < 0xff; reg++) {
		regmap_read(map, reg, &value);
		seq_printf(s, "[0x%x:0x%2x] = 0x%02x\n", page, reg, value);
	}

	return 0;
}

static int pm88x_print_whole_page(struct seq_file *s, void *p)
{
	struct pm88x_chip *chip = s->private;
	u8 page_addr = debug_page_addr;

	seq_puts(s, "88pm88x register values:\n");
	seq_printf(s, " page 0x%02x:\n", page_addr);

	pm88x_registers_print(chip, debug_page_addr, s);
	return 0;
}

static int pm88x_whole_page_open(struct inode *inode, struct file *file)
{
	return single_open(file, pm88x_print_whole_page, inode->i_private);
}

static const struct file_operations pm88x_whole_page_fops = {
	.open = pm88x_whole_page_open,
	.read = seq_read,
	.release = single_release,
	.owner = THIS_MODULE,
};

static ssize_t pm88x_read_power_down(struct file *file, char __user *user_buf,
				     size_t count, loff_t *ppos)
{
	struct pm88x_chip *chip = file->private_data;
	unsigned int i;
	int len = 0;
	char buf[100];
	char *powerdown1_name[] = {
		"OVER_TEMP",
		"UV_VANA5",
		"SW_PDOWN",
		"FL_ALARM",
		"WD",
		"LONG_ONKEY",
		"OV_VSYS",
		"RTC_RESET"
	};
	char *powerdown2_name[] = {
		"HYB_DONE",
		"UV_VBAT",
		"HW_RESET2",
		"PGOOD_PDOWN",
		"LONKEY_RTC",
		"HW_RESET1",
	};

	if (!chip)
		return -EINVAL;

	len += sprintf(&buf[len], "0x%x,0x%x ",
			chip->powerdown1, chip->powerdown2);
	if (!chip->powerdown1 && !chip->powerdown2) {
		len += sprintf(&buf[len], "(NO_PMIC_RESET)\n");
		return simple_read_from_buffer(user_buf, count, ppos, buf, len);
	}

	len += sprintf(&buf[len], "(");
	for (i = 0; i < ARRAY_SIZE(powerdown1_name); i++) {
		if ((1 << i) & chip->powerdown1)
			len += sprintf(&buf[len], "%s ", powerdown1_name[i]);
	}

	for (i = 0; i < ARRAY_SIZE(powerdown2_name); i++) {
		if ((1 << i) & chip->powerdown2)
			len += sprintf(&buf[len], "%s ", powerdown2_name[i]);
	}

	len--;
	len += sprintf(&buf[len], ")\n");

	return simple_read_from_buffer(user_buf, count, ppos, buf, len);
}

static const struct file_operations pm88x_power_down_fops = {
	.open = simple_open,
	.read = pm88x_read_power_down,
	.owner = THIS_MODULE,
};

static ssize_t pm88x_read_power_up(struct file *file, char __user *user_buf,
				   size_t count, loff_t *ppos)
{
	struct pm88x_chip *chip = file->private_data;
	unsigned int i;
	int len = 0;
	char buf[100];
	char *powerup_name[] = {
		"ONKEY_WAKEUP",
		"CHG_WAKEUP",
		"EXTON_WAKEUP",
		"SMPL_WAKEUP",
		"ALARM_WAKEUP",
		"FAULT_WAKEUP",
		"BAT_WAKEUP",
		"WLCHG_WAKEUP",
	};

	if (!chip)
		return -EINVAL;

	for (i = 0; i < ARRAY_SIZE(powerup_name); i++) {
		if ((1 << i) & chip->powerup)
			len += sprintf(&buf[len], "0x%x (%s)\n",
				       chip->powerup, powerup_name[i]);
	}

	return simple_read_from_buffer(user_buf, count, ppos, buf, len);
}

static const struct file_operations pm88x_power_up_fops = {
	.open = simple_open,
	.read = pm88x_read_power_up,
	.owner = THIS_MODULE,
};

static ssize_t pm88x_buck1_info_read(struct file *file, char __user *user_buf,
				   size_t count, loff_t *ppos)
{
	struct pm88x_chip *chip = file->private_data;
	struct pm88x_dvc *dvc = chip->dvc;
	int uv, i, len = 0;
	char str[1000];

	for (i = 0; i < dvc->desc.max_level; i++) {
		uv = pm88x_dvc_get_volt(i);
		if (uv < dvc->desc.min_uV)
			dev_err(chip->dev, "get buck 1 voltage failed of level %d.\n", i);
		else
			len += snprintf(str + len, sizeof(str),
					"buck 1, level %d, voltage %duV.\n", i, uv);
	}

	return simple_read_from_buffer(user_buf, count, ppos, str, len);
}

static ssize_t pm88x_buck1_info_write(struct file *file, const char __user *user_buf,
				      size_t count, loff_t *ppos)
{
	struct pm88x_chip *chip = file->private_data;
	struct pm88x_dvc *dvc = chip->dvc;
	int i, ret, lvl, volt, uv;
	char arg;

	ret = sscanf(user_buf, "-%c", &arg);
	if (ret < 1) {
		ret = sscanf(user_buf, "%d\n", &volt);
		if (ret < 1) {
			pr_err("Type \"echo -h > <debugfs>/88pm88x/buck1_info\" for help.\n");
		} else {
			for (i = 0; i < dvc->desc.max_level; i++) {
				pm88x_dvc_set_volt(i, volt);
				uv = pm88x_dvc_get_volt(i);
				pr_info("buck 1, level %d, voltage %d uV\n", i, uv);
			}
		}
	} else {
		switch (arg) {
		case 'l':
			ret = sscanf(user_buf, "-%c %d %d", &arg, &lvl, &volt);
			if (ret >= 2) {
				if (lvl > dvc->desc.max_level) {
					pr_err("Please check voltage level.\n");
					return count;
				}
				if (ret == 3)
					pm88x_dvc_set_volt(lvl, volt);
				uv = pm88x_dvc_get_volt(lvl);
				pr_info("buck 1, level %d, voltage %d uV\n", lvl, uv);
			} else {
				pr_err("Type \"echo -h > ");
				pr_err("<debugfs>/88pm88x/buck1_info\" for help.\n");
			}
			break;
		case 'h':
			pr_info("Usage of buck1_info:\n");
			pr_info("1: cat <debugfs>/88pm88x/buck1_info\n");
			pr_info("   dump voltages for all levels.\n");
			pr_info("2: echo [voltage-in-uV] > <debugfs>/88pm88x/buck1_info\n");
			pr_info("   set same voltages for all levels.\n");
			pr_info("3: echo -l [level] > <debugfs>/88pm88x/buck1_info\n");
			pr_info("   dump voltage of [level].\n");
			pr_info("4: echo -l [level] [voltage-in-uV] > ");
			pr_info("<debugfs>/88pm88x/buck1_info\n");
			pr_info("   set voltage of [level].\n");
			break;
		default:
			pr_err("Type \"echo -h > <debugfs>/88pm88x/buck1_info\" for help.\n");
			break;
		}
	}

	return count;
}

static const struct file_operations pm88x_buck1_info_fops = {
	.open = simple_open,
	.read = pm88x_buck1_info_read,
	.write = pm88x_buck1_info_write,
	.owner = THIS_MODULE,
};

static ssize_t pm88x_debug_read(struct file *file, char __user *user_buf,
				size_t count, loff_t *ppos)
{
	struct pm88x_chip *chip = file->private_data;
	int len = 0;
	ssize_t ret = -EINVAL;
	char *buf;

	if (!chip) {
		pr_err("Cannot find chip!\n");
		return -EINVAL;
	}

	buf = kzalloc(8000, GFP_KERNEL);
	if (!buf) {
		pr_err("Cannot allocate buffer!\n");
		return -ENOMEM;
	}

	ret = pm88x_display_buck(chip, buf);
	if (ret < 0) {
		pr_err("Error in printing the buck list!\n");
		goto out_print;
	}
	len += ret;

	ret = pm88x_display_vr(chip, buf + len);
	if (ret < 0) {
		pr_err("Error in printing the virtual regulator list!\n");
		goto out_print;
	}
	len += ret;

	ret = pm88x_display_ldo(chip, buf + len);
	if (ret < 0) {
		pr_err("Error in printing the ldo list!\n");
		goto out_print;
	}
	len += ret;

	ret = pm88x_display_dvc(chip, buf + len);
	if (ret < 0) {
		pr_err("Error in printing the dvc!\n");
		goto out_print;
	}
	len += ret;

	ret = pm88x_display_gpadc(chip, buf + len);
	if (ret < 0) {
		pr_err("Error in printing the GPADC values!\n");
		goto out_print;
	}
	len += ret;

	ret = simple_read_from_buffer(user_buf, count, ppos, buf, len);
out_print:
	kfree(buf);
	return ret;
}

static const struct file_operations pm88x_debug_fops = {
	.open = simple_open,
	.read = pm88x_debug_read,
	.write = NULL,
	.owner = THIS_MODULE,
};

static int pm88x_debug_probe(struct platform_device *pdev)
{
	struct dentry *file;
	struct pm88x_chip *chip = dev_get_drvdata(pdev->dev.parent);

	pm88x_dir = debugfs_create_dir(PM88X_NAME, NULL);
	if (!pm88x_dir)
		goto err;

	file = debugfs_create_file("register-address", (S_IRUGO | S_IWUSR | S_IWGRP),
		pm88x_dir, chip, &pm88x_reg_addr_fops);
	if (!file)
		goto err;

	file = debugfs_create_file("page-address", (S_IRUGO | S_IWUSR | S_IWGRP),
		pm88x_dir, chip, &pm88x_page_addr_fops);
	if (!file)
		goto err;

	file = debugfs_create_file("register-value", (S_IRUGO | S_IWUSR | S_IWGRP),
		pm88x_dir, chip, &pm88x_reg_val_fops);
	if (!file)
		goto err;

	file = debugfs_create_file("compact-address", (S_IRUGO | S_IWUSR | S_IWGRP),
		pm88x_dir, chip, &pm88x_compact_addr_fops);
	if (!file)
		goto err;
	file = debugfs_create_file("whole-page", (S_IRUGO | S_IRUSR | S_IRGRP),
		pm88x_dir, chip, &pm88x_whole_page_fops);
	if (!file)
		goto err;
	file = debugfs_create_file("power-down-log", (S_IRUGO | S_IRUSR | S_IRGRP),
		pm88x_dir, chip, &pm88x_power_down_fops);
	if (!file)
		goto err;
	file = debugfs_create_file("power-up-log", (S_IRUGO | S_IRUSR | S_IRGRP),
		pm88x_dir, chip, &pm88x_power_up_fops);
	if (!file)
		goto err;
	file = debugfs_create_file("buck1_info", (S_IRUGO | S_IWUSR | S_IWGRP),
		pm88x_dir, chip, &pm88x_buck1_info_fops);
	if (!file)
		goto err;
	file = debugfs_create_file("pm88x_debug", (S_IRUGO | S_IFREG),
		pm88x_dir, chip, &pm88x_debug_fops);
	if (!file)
		goto err;
	return 0;
err:
	debugfs_remove_recursive(pm88x_dir);
	dev_err(&pdev->dev, "failed to create debugfs entries.\n");
	return -ENOMEM;
}

static int pm88x_debug_remove(struct platform_device *pdev)
{
	debugfs_remove_recursive(pm88x_dir);

	return 0;
}


static struct platform_driver pm88x_debug_driver = {
	.driver = {
		.name = "88pm88x-debugfs",
		.owner = THIS_MODULE,
	},
	.probe  = pm88x_debug_probe,
	.remove = pm88x_debug_remove
};

static int pm88x_debug_init(void)
{
	return platform_driver_register(&pm88x_debug_driver);
}

static void pm88x_debug_exit(void)
{
	platform_driver_unregister(&pm88x_debug_driver);
}
subsys_initcall(pm88x_debug_init);
module_exit(pm88x_debug_exit);

MODULE_AUTHOR("Yi Zhang <yizhang@marvell.com>");
MODULE_DESCRIPTION("88pm88x debug interface");
MODULE_LICENSE("GPL v2");
