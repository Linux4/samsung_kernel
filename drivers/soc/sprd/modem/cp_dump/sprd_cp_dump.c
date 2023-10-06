/*
 * SPRD cp dump driver in AP side.
 *
 * Copyright (C) 2021 Spreadtrum Co., Ltd.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 of
 * the License as published by the Free Software Foundation.
 *
 * This program is used for CP dump in AP side for
 * Spreadtrum SoCs.
 */

#include <linux/cdev.h>
#include <linux/kernel.h>
#include <linux/mm.h>
#include <linux/module.h>
#include <linux/io.h>
#include <linux/of_device.h>
#include <linux/poll.h>
#include <linux/delay.h>
#include <linux/platform_device.h>
#include <linux/slab.h>
#include <linux/sipc.h>
#include <linux/mfd/syscon.h>
#include <linux/regmap.h>
#include <linux/debugfs.h>
#include <linux/gpio/consumer.h>
#include <linux/soc/sprd/sprd_sysdump.h>

#include "sprd_cp_dump.h"

#define CP_DUMP_MAGIC 'N'

#define CP_DUMP_READ_LOCK_CMD _IO(CP_DUMP_MAGIC, 0x1)
#define CP_DUMP_READ_UNLOCK_CMD _IO(CP_DUMP_MAGIC, 0x2)
#define CP_DUMP_WRITE_LOCK_CMD _IO(CP_DUMP_MAGIC, 0x3)
#define CP_DUMP_WRITE_UNLOCK_CMD _IO(CP_DUMP_MAGIC, 0x4)
#define CP_DUMP_GET_DUMP_INFO_CMD _IOR(CP_DUMP_MAGIC, 0x5, struct cp_dump_info)
#define CP_DUMP_SET_DUMP_INFO_CMD _IOW(CP_DUMP_MAGIC, 0x6, struct cp_dump_info)
#define CP_DUMP_SET_READ_REGION_CMD _IOR(CP_DUMP_MAGIC, 0x7, int)

#define CP_DUMP_NAME "cp_dump"

static struct class *cp_dump_class;

static int cp_dump_open(struct inode *inode, struct file *filp)
{
	struct cp_dump_device *cp_dump;

	cp_dump = container_of(inode->i_cdev, struct cp_dump_device, cdev);
	filp->private_data = cp_dump;

	return 0;
}

static int cp_dump_release(struct inode *inode, struct file *filp)
{
	return 0;
}

static void *cp_dump_map_memory(phys_addr_t start, size_t size, size_t *map_size_ptr)
{
	size_t map_size = size;
	void *map;

	do {
		map_size = PAGE_ALIGN(map_size);
		map = modem_ram_vmap_nocache(0, start, map_size);
		if (map) {
			if (map_size_ptr)
				*map_size_ptr = map_size;

			return map;
		}
		map_size /= 2;
	} while (map_size >= PAGE_SIZE);

	return NULL;
}

static ssize_t cp_dump_read(struct file *filp,
			  char __user *buf, size_t count, loff_t *ppos)
{
	phys_addr_t base;
	size_t size, offset, copy_size, map_size, r;
	void *vmem;
	struct cp_dump_device *cp_dump = filp->private_data;
	struct pm_reg_ctrl *sp_ctrl = cp_dump->sp_ctrl, *ch_ctrl = cp_dump->ch_ctrl;
	phys_addr_t addr;
	u8 read_region = cp_dump->read_region;
	char *name = cp_dump->dump_info->regions[read_region].name;
	ssize_t ret;

	dev_dbg(cp_dump->p_dev, "read, %s!\n", CP_DUMP_NAME);

	/* only get read lock task can be read */
	if (strcmp(current->comm, cp_dump->rd_lock_name) != 0) {
		dev_err(cp_dump->p_dev,  "read, task %s need get rd lock!\n",
			current->comm);
		return -EACCES;
	}

	base = cp_dump->dump_info->regions[read_region].address;
	size = cp_dump->dump_info->regions[read_region].size;
	offset = *ppos;
	dev_dbg(cp_dump->p_dev, "read, offset = 0x%lx, count = 0x%lx!\n",
		offset, count);

	if (size <= offset)
		return -EINVAL;

	/* get sp | ch sys bus control */
	if (strstr(name, "sp_") == name && sp_ctrl && sp_ctrl->reg_offset) {
		regmap_read(sp_ctrl->ctrl_map, sp_ctrl->reg_offset, &sp_ctrl->reg_save);
		regmap_update_bits(sp_ctrl->ctrl_map, sp_ctrl->reg_offset, sp_ctrl->reg_mask, 0);
	}

	if (strstr(name, "ch_") == name && ch_ctrl && ch_ctrl->reg_offset) {
		regmap_read(ch_ctrl->ctrl_map, ch_ctrl->reg_offset, &ch_ctrl->reg_save);
		regmap_update_bits(ch_ctrl->ctrl_map, ch_ctrl->reg_offset, ch_ctrl->reg_mask, 0);
	}

	count = min_t(size_t, size - offset, count);
	r = count;
	do {
		addr = base + offset + (count - r);
		vmem = cp_dump_map_memory(addr, r, &map_size);
		if (!vmem) {
			dev_err(cp_dump->p_dev,
				"read, Unable to map  base: 0x%llx\n", addr);
			ret = -ENOMEM;
			goto FAIL_READ;
		}

		copy_size = min_t(size_t, r, map_size);
		if (unalign_copy_to_user(buf, vmem, copy_size)) {
			dev_err(cp_dump->p_dev,
				"read, copy data from user err!\n");
			modem_ram_unmap(0, vmem);
			ret = -EFAULT;
			goto FAIL_READ;
		}
		modem_ram_unmap(0, vmem);
		r -= copy_size;
		buf += copy_size;
	} while (r > 0);

	*ppos += (count - r);
	ret = count - r;

FAIL_READ:
	if (strstr(name, "sp_") == name && sp_ctrl && sp_ctrl->reg_offset)
		regmap_update_bits(sp_ctrl->ctrl_map, sp_ctrl->reg_offset,
			       sp_ctrl->reg_mask, sp_ctrl->reg_save);

	if (strstr(name, "ch_") == name && ch_ctrl && ch_ctrl->reg_offset)
		regmap_update_bits(ch_ctrl->ctrl_map, ch_ctrl->reg_offset,
			       ch_ctrl->reg_mask, ch_ctrl->reg_save);

	return ret;
}

static int cp_dump_cmd_lock(struct file *filp,
			  struct cp_dump_device *cp_dump, int b_rx)
{
	struct mutex *mut; /* mutex point to rd_mutex or wt_mutex*/
	struct wakeup_source *ws;
	char *name;

	mut = b_rx ? &cp_dump->rd_mutex : &cp_dump->wt_mutex;
	ws = b_rx ? &cp_dump->rd_ws : &cp_dump->wt_ws;
	name = b_rx ? cp_dump->rd_lock_name : cp_dump->wt_lock_name;

	if (filp->f_flags & O_NONBLOCK) {
		if (!mutex_trylock(mut)) {
			dev_err(cp_dump->p_dev, "lock, %s get lock %d busy!\n",
				current->comm, b_rx);
			return -EBUSY;
		}
	} else {
		dev_dbg(cp_dump->p_dev, "lock, %s has get lock %d !\n",
			current->comm, b_rx);
		mutex_lock(mut);
	}

	/* lock, get wake lock, cpy task to name */
	__pm_stay_awake(ws);
	strcpy(name, current->comm);
	return 0;
}

static int cp_dump_cmd_unlock(struct cp_dump_device *cp_dump, int b_rx)
{
	struct mutex *mut; /* mutex point to rd_mutex or wt_mutex*/
	struct wakeup_source *ws;
	char *name;

	mut = b_rx ? &cp_dump->rd_mutex : &cp_dump->wt_mutex;
	ws = b_rx ? &cp_dump->rd_ws : &cp_dump->wt_ws;
	name = b_rx ? cp_dump->rd_lock_name : cp_dump->wt_lock_name;

	if (strlen(name) == 0)
		/* means no lock, so don't unlock */
		return 0;

	/* unlock, release wake lock, set name[0] to 0 */
	name[0] = 0;
	__pm_relax(ws);
	mutex_unlock(mut);

	dev_dbg(cp_dump->p_dev,
		"unlock, %s has unlock %d!\n",
		current->comm, b_rx);

	return 0;
}

static int cp_dump_get_something(struct cp_dump_device *cp_dump,
			       void *from,
			       unsigned int cmd,
			       unsigned long arg)
{
	if (strcmp(current->comm, cp_dump->rd_lock_name) != 0) {
		dev_err(cp_dump->p_dev, "get, task %s need get rd lock!\n",
			current->comm);
		return -EBUSY;
	}

	if (copy_to_user((void __user *)arg, from, _IOC_SIZE(cmd)))
		return -EFAULT;

	dev_dbg(cp_dump->p_dev, "get, %s arg 0x%lx!\n", current->comm, arg);

	return 0;
}

static int cp_dump_set_something(struct cp_dump_device *cp_dump,
				void *to, unsigned int cmd, unsigned long arg)
{
	dev_dbg(cp_dump->p_dev, "set, %s cmd 0x%x!\n", current->comm, cmd);
	if (strcmp(current->comm, cp_dump->wt_lock_name) != 0) {
		dev_err(cp_dump->p_dev, "set, task %s need get wt lock!\n",
				current->comm);
		return -EBUSY;
	}
	if (copy_from_user(to, (void __user *)arg, _IOC_SIZE(cmd)))
		return -EFAULT;

	return 0;
}

static long cp_dump_ioctl(struct file *filp, unsigned int cmd, unsigned long arg)
{
	int ret = -EINVAL;
	int access = 0;
	int param = 0;
	struct cp_dump_info tmp;
	struct cp_dump_device *cp_dump = (struct cp_dump_device *)filp->private_data;
	unsigned char i = 0;

	dev_dbg(cp_dump->p_dev, "ioctl, cmd=0x%x (%c nr=%d len=%d dir=%d)\n", cmd,
		_IOC_TYPE(cmd), _IOC_NR(cmd), _IOC_SIZE(cmd), _IOC_DIR(cmd));

	if (_IOC_DIR(cmd) & _IOC_READ)
		access = !access_ok(VERIFY_WRITE,
				 (void __user *)arg, _IOC_SIZE(cmd));
	else if (_IOC_DIR(cmd) & _IOC_WRITE)
		access = !access_ok(VERIFY_READ,
				 (void __user *)arg, _IOC_SIZE(cmd));

	if (access) {
		dev_err(cp_dump->p_dev, "ioctl, access isn't ok! ret=%d\n", ret);
		return -EFAULT;
	}

	dev_dbg(cp_dump->p_dev, "ioctl, arg = 0x%lx!", arg);

	switch (cmd) {
	case CP_DUMP_READ_LOCK_CMD:
		ret = cp_dump_cmd_lock(filp, cp_dump, 1);
		break;

	case CP_DUMP_READ_UNLOCK_CMD:
		ret = cp_dump_cmd_unlock(cp_dump, 1);
		break;

	case CP_DUMP_WRITE_LOCK_CMD:
		ret = cp_dump_cmd_lock(filp, cp_dump, 0);
		break;

	case CP_DUMP_WRITE_UNLOCK_CMD:
		ret = cp_dump_cmd_unlock(cp_dump, 0);
		break;

	case CP_DUMP_GET_DUMP_INFO_CMD:
		ret = cp_dump_get_something(cp_dump, cp_dump->dump_info, cmd, arg);
		break;

	case CP_DUMP_SET_READ_REGION_CMD:
		ret = cp_dump_set_something(cp_dump, &param, cmd, arg);
		cp_dump->read_region = (u8)param;
		break;

	case CP_DUMP_SET_DUMP_INFO_CMD:
		ret = cp_dump_set_something(cp_dump, &tmp, cmd, arg);
		if (ret)
			break;
		cp_dump->dump_info->region_cnt = tmp.region_cnt;
		for (i = 0; i < tmp.region_cnt; i++) {
			cp_dump->dump_info->regions[i].address = (u64)tmp.regions[i].address;
			cp_dump->dump_info->regions[i].size = (u32)tmp.regions[i].size;
			strcpy(cp_dump->dump_info->regions[i].name, tmp.regions[i].name);
			cp_dump->dump_info->regions[i].mini_dump_flag = tmp.regions[i].mini_dump_flag;
			if (tmp.regions[i].mini_dump_flag) {
				ret = minidump_save_extend_information(tmp.regions[i].name,
						tmp.regions[i].address,
						tmp.regions[i].address + tmp.regions[i].size);
			}
			if (ret)
				dev_err(cp_dump->p_dev, "miniump err\n");
		}
		break;
	default:
		ret = -EINVAL;
		break;
	}

	return ret;
}

static int cp_dump_parse_dt(struct cp_dump_device *cp_dump, struct device_node *np)
{
	struct pm_reg_ctrl *pm_reg_ctl;
	struct of_phandle_args out_args;
	int ret = -1;

	ret = of_parse_phandle_with_args(np, "sprd,sys-bus-ctrl-sp", "#syscon-cells", 0, &out_args);
	if (!ret) {
		of_node_put(out_args.np);
		pm_reg_ctl = devm_kzalloc(cp_dump->p_dev, sizeof(struct pm_reg_ctrl), GFP_KERNEL);
		if (!pm_reg_ctl)
			return -ENOMEM;

		pm_reg_ctl->reg_offset = out_args.args[0];
		pm_reg_ctl->reg_mask = out_args.args[1];
		cp_dump->sp_ctrl = pm_reg_ctl;
		pm_reg_ctl->ctrl_map = syscon_regmap_lookup_by_phandle(np, "sprd,sys-bus-ctrl-sp");
		if (IS_ERR(pm_reg_ctl->ctrl_map)) {
			dev_err(cp_dump->p_dev, "failed to find pm_aon_apb reg sp\n");
			return -EINVAL;
		}

		dev_info(cp_dump->p_dev, "offset = 0x%x, mask = 0x%x \n",
				pm_reg_ctl->reg_offset, pm_reg_ctl->reg_mask);
	}

	ret = of_parse_phandle_with_args(np, "sprd,sys-bus-ctrl-ch", "#syscon-cells", 0, &out_args);
	if (!ret) {
		of_node_put(out_args.np);
		pm_reg_ctl = devm_kzalloc(cp_dump->p_dev, sizeof(struct pm_reg_ctrl), GFP_KERNEL);
		if (!pm_reg_ctl)
			return -ENOMEM;

		pm_reg_ctl->reg_offset = out_args.args[0];
		pm_reg_ctl->reg_mask = out_args.args[1];
		cp_dump->ch_ctrl = pm_reg_ctl;
		pm_reg_ctl->ctrl_map = syscon_regmap_lookup_by_phandle(np, "sprd,sys-bus-ctrl-ch");
		if (IS_ERR(pm_reg_ctl->ctrl_map)) {
			dev_err(cp_dump->p_dev, "failed to find pm_aon_apb reg ch\n");
			return -EINVAL;
		}

		dev_info(cp_dump->p_dev, "offset = 0x%x, mask = 0x%x \n",
				pm_reg_ctl->reg_offset, pm_reg_ctl->reg_mask);
	}

	return 0;
}

static const struct file_operations cp_dump_fops = {
	.open = cp_dump_open,
	.release = cp_dump_release,
	.read = cp_dump_read,
	.unlocked_ioctl = cp_dump_ioctl,
	.owner = THIS_MODULE
};

static int cp_dump_probe(struct platform_device *pdev)
{
	struct cp_dump_device *cp_dump;
	int ret = 0;
	struct device *dev;
	struct device_node *np = pdev->dev.of_node;

	cp_dump = devm_kzalloc(&(pdev->dev),
			sizeof(struct cp_dump_device),
			GFP_KERNEL);
	if (!cp_dump)
		return -ENOMEM;

	cp_dump->p_dev = &(pdev->dev);
	cp_dump->dump_info = devm_kzalloc(cp_dump->p_dev,
				sizeof(struct cp_dump_info),
				GFP_KERNEL);
	if (!cp_dump->dump_info)
		return -ENOMEM;

	ret = cp_dump_parse_dt(cp_dump, np);
	if (ret)
		return ret;

	ret = alloc_chrdev_region(&cp_dump->devid, 0, 1, CP_DUMP_NAME);
	if (ret != 0) {
		dev_err(cp_dump->p_dev, "get name fail, ret = %d!\n", ret);
		return ret;
	}

	cdev_init(&(cp_dump->cdev), &cp_dump_fops);
	ret = cdev_add(&(cp_dump->cdev), cp_dump->devid, 1);
	if (ret != 0) {
		unregister_chrdev_region(cp_dump->devid, 1);
		dev_err(cp_dump->p_dev, "add dev fail, ret = %d!\n", ret);
		return ret;
	}

	dev = device_create(cp_dump_class, NULL,
			cp_dump->devid,
			NULL, "%s", CP_DUMP_NAME);
	if (IS_ERR(dev))
		dev_err(cp_dump->p_dev, "device_create fail,ERRNO = %ld!\n",
			PTR_ERR(dev));

	mutex_init(&cp_dump->rd_mutex);
	mutex_init(&cp_dump->wt_mutex);
	wakeup_source_init(&cp_dump->rd_ws, "cp_dump_rd_wakeup");
	wakeup_source_init(&cp_dump->wt_ws, "cp_dump_wt_wakeup");
	platform_set_drvdata(pdev, cp_dump);

	return 0;
}

static int  cp_dump_remove(struct platform_device *pdev)
{
	struct cp_dump_device *cp_dump = platform_get_drvdata(pdev);

	if (cp_dump) {
		wakeup_source_trash(&cp_dump->rd_ws);
		wakeup_source_trash(&cp_dump->wt_ws);
		mutex_destroy(&cp_dump->rd_mutex);
		mutex_destroy(&cp_dump->wt_mutex);
		device_destroy(cp_dump_class, cp_dump->devid);
		cdev_del(&cp_dump->cdev);
		unregister_chrdev_region(cp_dump->devid, 1);
		platform_set_drvdata(pdev, NULL);
	}

	return 0;
}

static struct of_device_id cp_dump_match_table[] = {
	{.compatible = "sprd,cp_dump"},
	{ },
};

static struct platform_driver cp_dump_driver = {
	.driver = {
		.name = "sprd-cp_dump",
		.of_match_table = cp_dump_match_table,
	},
	.probe = cp_dump_probe,
	.remove = cp_dump_remove,
};

static int __init cp_dump_init(void)
{
	cp_dump_class = class_create(THIS_MODULE, "cp_dump");
	if (IS_ERR(cp_dump_class))
		return PTR_ERR(cp_dump_class);
	return platform_driver_register(&cp_dump_driver);
}

static void __exit cp_dump_exit(void)
{
	class_destroy(cp_dump_class);
	platform_driver_unregister(&cp_dump_driver);
}

module_init(cp_dump_init);
module_exit(cp_dump_exit);

MODULE_AUTHOR("yi yang");
MODULE_DESCRIPTION("cp_dump driver");
MODULE_LICENSE("GPL v2");
