/*
 * Copyright (C) 2019 Samsung Electronics.
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/sched.h>
#include <linux/fs.h>
#include <linux/poll.h>
#include <linux/if_arp.h>
#include <linux/ip.h>
#include <linux/if_ether.h>
#include <linux/etherdevice.h>
#include <linux/device.h>
#include <linux/module.h>
#include <linux/of_gpio.h>
#include <linux/proc_fs.h>

#include "modem_prj.h"
// todo, #include "modem_utils.h"

#include "ccci_core.h"
#include "ccci_bm.h"
#include "port_ipc.h"
#include "port/port_char.h"
#include "port/port_proxy.h"
#include "inc/ccci_fsm.h"

extern const struct file_operations *get_port_char_ops(void);

static struct file_operations *ccci_port_ops;

void print_ipc_pkt(u16 ch, struct sk_buff *skb)
{
	if (ch == CCCI_RIL_IPC0_RX || ch == CCCI_RIL_IPC1_RX)
		print_hex_dump(KERN_INFO, "mif: RX: ",
				DUMP_PREFIX_NONE, 32, 1, skb->data, 32, 0);
	else if (ch == CCCI_RIL_IPC0_TX || ch == CCCI_RIL_IPC1_TX)
		print_hex_dump(KERN_INFO, "mif: TX: ",
				DUMP_PREFIX_NONE, 32, 1, skb->data, 32, 0);
}

static int misc_open(struct inode *inode, struct file *file)
{
	struct io_device *iod = to_io_device(file->private_data);
	int minor = iod->minor;
	struct port_t *port;

	pr_err("mif: open: %s, minor=%d\n",
		file->f_path.dentry->d_iname, minor);

	port = port_get_by_minor(MD_SYS1, minor);
	if (!port) {
		pr_err("mif: open: port is null\n");
		return -EBUSY;
	}
	/*if (atomic_read(&port->usage_cnt)) {
		pr_err("mif: open: count=%d\n", atomic_read(&port->usage_cnt));
		return -EBUSY;
	}*/

	pr_err("mif: open: %s\n", port->name);

	atomic_inc(&port->usage_cnt);
	file->private_data = port;
	nonseekable_open(inode, file);
	port_user_register(port);

	return 0;
}

static int misc_release(struct inode *inode, struct file *filp)
{
	if (!ccci_port_ops)
		return 0;

	pr_err("mif: release: %s(%s)\n",
		filp->f_path.dentry->d_iname, current->comm);

	return ccci_port_ops->release(inode, filp);
}

static ssize_t misc_write(struct file *filp, const char __user *data,
		size_t count, loff_t *fpos)
{
	if (!ccci_port_ops)
		return 0;

	return ccci_port_ops->write(filp, data, count, fpos);
}

static ssize_t misc_read(struct file *filp, char *buf, size_t count,
		loff_t *fpos)
{
	if (!ccci_port_ops)
		return 0;

	return ccci_port_ops->read(filp, buf, count, fpos);
}

static unsigned int misc_poll(struct file *filp, struct poll_table_struct *wait)
{
	struct port_t *port = filp->private_data;
	int md_id = port->md_id;
	int md_state = ccci_fsm_get_md_state(md_id);
	unsigned long mask = 0;

	if (!ccci_port_ops)
		return 0;

	mask = ccci_port_ops->poll(filp, wait);

	if (port->rx_ch != CCCI_RIL_IPC0_RX && port->rx_ch != CCCI_RIL_IPC1_RX)
		return mask;

	/* only for S-RIL IPC */
	switch (md_state) {
	case GATED:
	case BOOT_WAITING_FOR_HS2:
	case RESET:
	case EXCEPTION:
		if (port->md_state_changed) {
			port->md_state_changed = 0;
			mask |= POLLHUP;
		}
		mif_err("%s: state == %d\n", "ipc", md_state);
		break;

	default:
		break;
	}

	return mask;
}

static long misc_ioctl(struct file *filp, unsigned int cmd, unsigned long arg)
{
	struct port_t *port = filp->private_data;
	int md_id = port->md_id;
	int md_state = ccci_fsm_get_md_state(md_id);

	if (!ccci_port_ops)
		return 0;

	switch (cmd) {
	case IOCTL_MODEM_STATUS:
		/*
		 * MD_BOOT_STAGE_0 = STATE_OFFLINE,
		 * MD_BOOT_STAGE_1 = STATE_BOOTING,
		 * MD_BOOT_STAGE_2 = STATE_ONLINE,
		 *
		 */
		switch (md_state) {
		case GATED:
		case RESET:
			return STATE_OFFLINE;
		case BOOT_WAITING_FOR_HS2:
			return STATE_BOOTING;
		case READY:
			return STATE_ONLINE;
		case EXCEPTION:
			return STATE_CRASH_EXIT;
		default:
			break;
		}
		return ccci_port_ops->unlocked_ioctl(filp, CCCI_IOC_GET_MD_STATE, arg);

	case IOCTL_MODEM_FORCE_CRASH_EXIT:
		pr_err("mif: %s: cmd=0x%x\n", __func__, cmd);
		return ccci_port_ops->unlocked_ioctl(filp, CCCI_IOC_FORCE_MD_ASSERT, arg);

	case IOCTL_MODEM_RESET:
		if (md_state == EXCEPTION) {
			pr_err("mif: %s skip IOCTL_MODEM_RESET md_state:EXCEPTION\n", __func__);
            return 0;
		}
		
		pr_err("mif: %s: cmd=0x%x\n", __func__, cmd);
		return ccci_port_ops->unlocked_ioctl(filp, CCCI_IOC_MD_RESET, arg);

	case CCCI_IOC_ENTER_DEEP_FLIGHT_ENHANCED:
	case CCCI_IOC_LEAVE_DEEP_FLIGHT_ENHANCED:
		pr_err("mif: %s: cmd=0x%x\n", __func__, cmd);
		return ccci_port_ops->unlocked_ioctl(filp, cmd, arg);

#if 0 // todo
	default:
		/* If you need to handle the ioctl for specific link device,
		 * then assign the link ioctl handler to ld->ioctl
		 * It will be call for specific link ioctl */
		if (ld->ioctl)
			return ld->ioctl(ld, iod, cmd, arg);

		mif_info("%s: ERR! undefined cmd 0x%X\n", iod->name, cmd);
		return -EINVAL;
#endif
	}

	return 0;
}

static const struct file_operations misc_io_fops = {
	.owner = THIS_MODULE,
	.open = misc_open,
	.release = misc_release,
	.poll = misc_poll,
	.unlocked_ioctl = misc_ioctl,
#ifdef CONFIG_COMPAT
	.compat_ioctl = misc_ioctl,
#endif
	.write = misc_write,
	.read = misc_read,
};

int sipc5_init_io_device(struct io_device *iod)
{
	int ret = 0;

	switch (iod->io_typ) {
	case IODEV_MISC:
		iod->miscdev.minor = MISC_DYNAMIC_MINOR;
		iod->miscdev.name = iod->name;
		iod->miscdev.fops = &misc_io_fops;
		ret = misc_register(&iod->miscdev);
		if (ret)
			mif_info("%s: ERR! misc_register failed\n", iod->name);
		break;

	case IODEV_DUMMY:
		iod->miscdev.minor = MISC_DYNAMIC_MINOR;
		iod->miscdev.name = iod->name;
		iod->miscdev.fops = &misc_io_fops;

		ret = misc_register(&iod->miscdev);
		if (ret)
			mif_info("%s: ERR! misc_register fail\n", iod->name);
		break;

	default:
		mif_info("%s: ERR! wrong io_type %d\n", iod->name, iod->io_typ);
		return -EINVAL;
	}

	return ret;
}

#ifdef CONFIG_OF
static int parse_dt_common_pdata(struct device_node *np,
		struct modem_data *pdata)
{
	mif_dt_read_string(np, "mif,name", pdata->name);
	mif_dt_read_u32(np, "mif,num_iodevs", pdata->num_iodevs);

	return 0;
}

static int parse_dt_iodevs_pdata(struct device *dev, struct device_node *np,
		struct modem_data *pdata)
{
	struct device_node *child = NULL;
	size_t size = sizeof(struct io_device) * pdata->num_iodevs;
	int i = 0;

	pdata->iodevs = devm_kzalloc(dev, size, GFP_KERNEL);
	if (!pdata->iodevs) {
		mif_err("iodevs: failed to alloc memory\n");
		return -ENOMEM;
	}

	for_each_child_of_node(np, child) {
		struct io_device *iod = &pdata->iodevs[i];

		mif_dt_read_string(child, "iod,name", iod->name);
		mif_dt_read_u32(child, "iod,minor", iod->minor);
		mif_dt_read_enum(child, "iod,format", iod->format);
		mif_dt_read_enum(child, "iod,io_type", iod->io_typ);
		mif_dt_read_u32(child, "iod,attrs", iod->attrs);
		/* mif_dt_read_string(child, "iod,app", iod->app); */

		i++;
	}
	return 0;
}
#endif

static struct modem_data *modem_if_parse_dt_pdata(struct device *dev)
{
	struct modem_data *pdata;
	struct device_node *iodevs = NULL;

	pdata = devm_kzalloc(dev, sizeof(struct modem_data), GFP_KERNEL);
	if (!pdata) {
		mif_err("modem_data: alloc fail\n");
		return ERR_PTR(-ENOMEM);
	}

	if (parse_dt_common_pdata(dev->of_node, pdata)) {
		mif_err("DT error: failed to parse common\n");
		goto error;
	}

	iodevs = of_get_child_by_name(dev->of_node, "iodevs");
	if (!iodevs) {
		mif_err("DT error: failed to get child node\n");
		goto error;
	}

	if (parse_dt_iodevs_pdata(dev, iodevs, pdata)) {
		mif_err("DT error: failed to parse iodevs\n");
		goto error;
	}

	dev->platform_data = pdata;
	mif_info("DT parse complete!\n");
	return pdata;

error:
	if (pdata) {
		if (pdata->iodevs)
			devm_kfree(dev, pdata->iodevs);
		devm_kfree(dev, pdata);
	}
	return ERR_PTR(-EINVAL);
}

enum mif_sim_mode {
	MIF_SIM_NONE = 0,
	MIF_SIM_SINGLE,
	MIF_SIM_DUAL,
	MIF_SIM_TRIPLE,
};

static int simslot_count(struct seq_file *m, void *v)
{
	enum mif_sim_mode mode = (enum mif_sim_mode)m->private;

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

static void get_sim_mode(struct device_node *of_node)
{
	enum mif_sim_mode mode = MIF_SIM_DUAL;
	int gpio_ds_det;
	int retval;

	gpio_ds_det = of_get_named_gpio(of_node, "mif,gpio_ds_det", 0);
	if (!gpio_is_valid(gpio_ds_det)) {
		pr_err("DT error: failed to get sim mode\n");
		goto make_proc;
	}

	retval = gpio_get_value(gpio_ds_det);
	if (retval)
		mode = MIF_SIM_SINGLE;

	pr_err("sim_mode: %d\n", mode);
	gpio_free(gpio_ds_det);

make_proc:
	if (!proc_create_data("simslot_count", 0, NULL, &simslot_count_fops,
			(void *)(long)mode)) {
		pr_err("Failed to create proc\n");
		mode = MIF_SIM_DUAL;
	}
}

static int modem_probe(struct platform_device *pdev)
{
	int i;
	struct device *dev = &pdev->dev;
	struct modem_data *pdata = dev->platform_data;

	mif_info("%s: +++\n", pdev->name);

	if (!dev->of_node)
		return -EINVAL;

	pdata = modem_if_parse_dt_pdata(dev);

	get_sim_mode(dev->of_node);

	for (i = 0; i < pdata->num_iodevs; i++) {
		struct io_device *iod = &pdata->iodevs[i];

		INIT_LIST_HEAD(&iod->list);
		atomic_set(&iod->opened, 0);

		if (sipc5_init_io_device(iod)) {
			mif_err("%s: iod[%d] == NULL\n", pdata->name, i);
			goto free_iod;
		}
	}

	// todo: platform_set_drvdata(pdev, modemctl);

	ccci_port_ops = (struct file_operations *)get_port_char_ops();

	mif_info("%s: ---\n", pdev->name);
	return 0;

free_iod:
	kfree(pdata->iodevs);

	mif_err("%s: xxx\n", pdev->name);
	return -ENOMEM;
}

static const struct of_device_id sec_modem_match[] = {
	{ .compatible = "sec_modem,modem_pdata", },
	{},
};
MODULE_DEVICE_TABLE(of, sec_modem_match);

static struct platform_driver modem_driver = {
	.probe = modem_probe,
	.driver = {
		.name = "mif_sipc5",
		.owner = THIS_MODULE,
#ifdef CONFIG_OF
		.of_match_table = of_match_ptr(sec_modem_match),
#endif
	},
};

void register_port_ops(const struct file_operations *ops)
{
	if (!ccci_port_ops)
		ccci_port_ops = (struct file_operations *)ops;
}

module_platform_driver(modem_driver);

MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("Samsung Modem Interface Driver");


