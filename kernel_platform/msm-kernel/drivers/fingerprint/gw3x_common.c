/*
 *  Copyright (C) 2020, Samsung Electronics Co. Ltd. All Rights Reserved.
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 */

#include "fingerprint.h"
#include "gw3x_common.h"

#define GF_DEV_NAME "goodix_fp"
#define GF_DEV_MAJOR 0	/* assigned */
#define GF_CLASS_NAME "goodix_fp"
#define GF_NETLINK_ROUTE 25
#define MAX_NL_MSG_LEN 16
#define WAKELOCK_HOLD_TIME 1000 /* in ms */

static LIST_HEAD(device_list);
static DEFINE_MUTEX(device_list_lock);

static const struct of_device_id gw3x_of_match[] = {
	{ .compatible = "goodix,fingerprint", },
	{},
};
MODULE_DEVICE_TABLE(of, gw3x_of_match);

static struct gf_device *g_data;
struct debug_logger *g_logger;

static ssize_t bfs_values_show(struct device *dev,
				      struct device_attribute *attr, char *buf)
{
	struct gf_device *gf_dev = dev_get_drvdata(dev);

	return snprintf(buf, PAGE_SIZE, "\"FP_SPICLK\":\"%d\"\n",
			gf_dev->clk_setting->spi_speed);
}

static ssize_t type_check_show(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	struct gf_device *gf_dev = dev_get_drvdata(dev);

	return snprintf(buf, PAGE_SIZE, "%d\n", gf_dev->sensortype);
}

static ssize_t vendor_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	return snprintf(buf, PAGE_SIZE, "%s\n", "GOODIX");
}

static ssize_t name_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct gf_device *gf_dev = dev_get_drvdata(dev);

	return snprintf(buf, PAGE_SIZE, "%s\n", gf_dev->chipid);
}

static ssize_t adm_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	return snprintf(buf, PAGE_SIZE, "%d\n", DETECT_ADM);
}

static ssize_t position_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct gf_device *gf_dev = dev_get_drvdata(dev);

	return snprintf(buf, PAGE_SIZE, "%s\n", gf_dev->position);
}

static ssize_t intcnt_show(struct device *dev,
			       struct device_attribute *attr, char *buf)
{
	struct gf_device *gf_dev = dev_get_drvdata(dev);

	return snprintf(buf, PAGE_SIZE, "%d\n", gf_dev->interrupt_count);
}

static ssize_t intcnt_store(struct device *dev,
				struct device_attribute *attr, const char *buf,
				size_t size)
{
	struct gf_device *gf_dev = dev_get_drvdata(dev);

	if (sysfs_streq(buf, "c")) {
		gf_dev->interrupt_count = 0;
		pr_info("initialization is done\n");
	}
	return size;
}

static ssize_t resetcnt_show(struct device *dev,
			       struct device_attribute *attr, char *buf)
{
	struct gf_device *gf_dev = dev_get_drvdata(dev);

	return snprintf(buf, PAGE_SIZE, "%d\n", gf_dev->reset_count);
}

static ssize_t resetcnt_store(struct device *dev,
				struct device_attribute *attr, const char *buf,
				size_t size)
{
	struct gf_device *gf_dev = dev_get_drvdata(dev);

	if (sysfs_streq(buf, "c")) {
		gf_dev->reset_count = 0;
		pr_info("initialization is done\n");
	}
	return size;
}

static DEVICE_ATTR_RO(bfs_values);
static DEVICE_ATTR_RO(type_check);
static DEVICE_ATTR_RO(vendor);
static DEVICE_ATTR_RO(name);
static DEVICE_ATTR_RO(adm);
static DEVICE_ATTR_RO(position);
static DEVICE_ATTR_RW(intcnt);
static DEVICE_ATTR_RW(resetcnt);

static struct device_attribute *fp_attrs[] = {
	&dev_attr_bfs_values,
	&dev_attr_type_check,
	&dev_attr_vendor,
	&dev_attr_name,
	&dev_attr_adm,
	&dev_attr_position,
	&dev_attr_intcnt,
	&dev_attr_resetcnt,
	NULL,
};

static void gw3x_enable_irq(struct gf_device *gf_dev)
{
	struct irq_desc *desc = irq_to_desc(gf_dev->irq);
#if KERNEL_VERSION(5, 4, 0) <= LINUX_VERSION_CODE
	struct irq_data *d = irq_get_irq_data(gf_dev->irq);
#endif
	if (gf_dev->irq_enabled == 1) {
		pr_err("irq already enabled\n");
	} else {
		/* Pending IRQ clear */
		if ((!IS_ERR_OR_NULL(desc)) && (desc->irq_data.chip->irq_ack))
			desc->irq_data.chip->irq_ack(&desc->irq_data);
#if KERNEL_VERSION(5, 4, 0) <= LINUX_VERSION_CODE
		if ((!IS_ERR_OR_NULL(d)) && (d->parent_data)) /* QC case 05534084 */
			irq_chip_set_parent_state(d, IRQCHIP_STATE_PENDING, false);
#endif
		enable_irq(gf_dev->irq);
		enable_irq_wake(gf_dev->irq);
		gf_dev->irq_enabled = 1;
		pr_debug("enable interrupt!\n");
	}
}

static void gw3x_disable_irq(struct gf_device *gf_dev)
{
	if (gf_dev->irq_enabled == 0) {
		pr_err("irq already disabled\n");
	} else {
		disable_irq_wake(gf_dev->irq);
		disable_irq(gf_dev->irq);
		gf_dev->irq_enabled = 0;
		pr_debug("disable interrupt!\n");
	}
}

static void gw3x_netlink_send(struct gf_device *gf_dev, const int command)
{
	struct nlmsghdr *nlh = NULL;
	struct sk_buff *skb = NULL;
	int ret;

	if (gf_dev->nl_sk == NULL) {
		pr_err("invalid socket\n");
		return;
	}

	if (gf_dev->pid == 0) {
		pr_err("invalid native process pid\n");
		return;
	}

	/* alloc data buffer for sending to native */
	/* malloc data space at least 1500 bytes, which is ethernet data length */
	skb = alloc_skb(MAX_NL_MSG_LEN, GFP_ATOMIC);
	if (skb == NULL)
		return;

	nlh = nlmsg_put(skb, 0, 0, 0, MAX_NL_MSG_LEN, 0);
	if (!nlh) {
		pr_err("nlmsg_put failed\n");
		kfree_skb(skb);
		return;
	}

	NETLINK_CB(skb).portid = 0;
	NETLINK_CB(skb).dst_group = 0;

	*(char *)NLMSG_DATA(nlh) = command;
	ret = netlink_unicast(gf_dev->nl_sk, skb, gf_dev->pid, MSG_DONTWAIT);
	if (ret == 0) {
		pr_err("send failed\n");
		return;
	}
}

static void gw3x_netlink_recv(struct sk_buff *__skb)
{
	struct sk_buff *skb = NULL;
	struct nlmsghdr *nlh = NULL;
	char str[128];

	skb = skb_get(__skb);
	if (skb == NULL) {
		pr_err("skb_get return NULL\n");
		return;
	}

	/* presume there is 5byte payload at leaset */
	if (skb->len >= NLMSG_SPACE(0)) {
		nlh = nlmsg_hdr(skb);
		memcpy(str, NLMSG_DATA(nlh), sizeof(str));
		g_data->pid = nlh->nlmsg_pid;
		pr_info("pid: %d, msg: %s\n", g_data->pid, str);
	} else {
		pr_err("not enough data length\n");
	}

	kfree_skb(skb);
}

static int gw3x_netlink_init(struct gf_device *gf_dev)
{
	struct netlink_kernel_cfg cfg;

	memset(&cfg, 0, sizeof(struct netlink_kernel_cfg));
	cfg.input = gw3x_netlink_recv;

	gf_dev->nl_sk =
		netlink_kernel_create(&init_net, GF_NETLINK_ROUTE, &cfg);
	if (gf_dev->nl_sk == NULL) {
		pr_err("netlink create failed\n");
		return -1;
	}

	pr_info("netlink create success\n");
	return 0;
}

static int gw3x_netlink_destroy(struct gf_device *gf_dev)
{
	if (gf_dev->nl_sk != NULL) {
		netlink_kernel_release(gf_dev->nl_sk);
		gf_dev->nl_sk = NULL;
		return 0;
	}

	pr_err("no netlink socket yet\n");
	return -1;
}

static ssize_t gw3x_read(struct file *filp, char __user *buf,
		size_t count, loff_t *f_pos)
{
	return -EFAULT;
}

static ssize_t gw3x_write(struct file *filp, const char __user *buf,
			size_t count, loff_t *f_pos)
{
	return -EFAULT;
}

static irqreturn_t gw3x_irq(int irq, void *handle)
{
	struct gf_device *gf_dev = (struct gf_device *)handle;

	pr_info("Entry\n");
	__pm_wakeup_event(gf_dev->wake_lock,
			msecs_to_jiffies(WAKELOCK_HOLD_TIME));
	gw3x_netlink_send(gf_dev, GF_NETLINK_IRQ);
	gf_dev->interrupt_count++;

	return IRQ_HANDLED;
}

static long gw3x_ioctl(struct file *filp, unsigned int cmd, unsigned long arg)
{
	struct gf_device *gf_dev = NULL;
	unsigned int onoff = 0;
	int retval = 0;
	u8  temp = 0;
	u8 netlink_route = GF_NETLINK_ROUTE;

	if (_IOC_TYPE(cmd) != GF_IOC_MAGIC)
		return -EINVAL;

	if (!filp || !filp->private_data) {
		pr_err("NULL pointer passed\n");
		return -EINVAL;
	}

	if (IS_ERR((void __user *)arg)) {
		pr_err("invalid user space pointer %lu\n", arg);
		return -EINVAL;
	}

	gf_dev = (struct gf_device *)filp->private_data;

	switch (cmd) {
	case GF_IOC_INIT:
		pr_info("GF_IOC_INIT\n");
		if (copy_to_user((void __user *)arg, (void *)&netlink_route,
					sizeof(u8))) {
			retval = -EFAULT;
			break;
		}
		if (gf_dev->system_status) {
			pr_info("system re-started\n");
			break;
		}
		gf_dev->system_status = 1;
		break;

	case GF_IOC_EXIT:
		pr_info("GF_IOC_EXIT\n");
		gw3x_disable_irq(gf_dev);
		if (gf_dev->irq) {
			free_irq(gf_dev->irq, gf_dev);
			gf_dev->irq = 0;
		}
		gf_dev->system_status = 0;
		break;

	case GF_IOC_RESET:
		pr_info("GF_IOC_RESET\n");
		gw3x_hw_reset(gf_dev, 0);
		break;

	case GF_IOC_ENABLE_IRQ:
		pr_info("GF_IOC_ENABLE_IRQ\n");
		gw3x_enable_irq(gf_dev);
		break;

	case GF_IOC_DISABLE_IRQ:
		pr_info("GF_IOC_DISABLE_IRQ\n");
		gw3x_disable_irq(gf_dev);
		break;

	case GF_IOC_ENABLE_SPI_CLK:
		pr_info("GF_IOC_ENABLE_SPI_CLK, Clock : %d\n", gf_dev->clk_setting->spi_speed);
		spi_clk_enable(gf_dev->clk_setting);
		break;

	case GF_IOC_DISABLE_SPI_CLK:
		pr_info("GF_IOC_DISABLE_SPI_CLK\n");
		spi_clk_disable(gf_dev->clk_setting);
		break;

	case GF_IOC_ENABLE_POWER:
		pr_info("GF_IOC_ENABLE_POWER\n");
		gw3x_hw_power_enable(gf_dev, 1);
		break;

	case GF_IOC_DISABLE_POWER:
		pr_info("GF_IOC_DISABLE_POWER\n");
		gw3x_hw_power_enable(gf_dev, 0);
		break;

	case GF_IOC_POWER_CONTROL:
		if (copy_from_user(&onoff, (void __user *)arg,
					sizeof(onoff))) {
			pr_err("Failed to copy onoff value from user to kernel\n");
			retval = -EFAULT;
			break;
		}
		pr_info("GF_IOC_POWER_CONTROL %d\n", onoff);
		gw3x_hw_power_enable(gf_dev, onoff);
		break;

	case GF_IOC_GET_FW_INFO:
		temp = 1;
		pr_debug("GET_FW_INFO : 0x%x\n", temp);
		if (copy_to_user((void __user *)arg, (void *)&temp,
					sizeof(temp))) {
			pr_err("Failed to copy data to user\n");
			retval = -EFAULT;
		}
		break;

#ifndef ENABLE_SENSORS_FPRINT_SECURE
	case GF_IOC_TRANSFER_RAW_CMD:
		mutex_lock(&gf_dev->buf_lock);
		retval = gw3x_ioctl_transfer_raw_cmd(gf_dev, arg, (unsigned int)TANSFER_MAX_LEN);
		mutex_unlock(&gf_dev->buf_lock);
		break;
#endif /* !ENABLE_SENSORS_FPRINT_SECURE */
#ifdef ENABLE_SENSORS_FPRINT_SECURE
	case GF_IOC_SET_SENSOR_TYPE:
		if (copy_from_user(&onoff, (void __user *)arg,
				   sizeof(onoff)) != 0) {
			pr_err("Failed to copy sensor type from user to kernel\n");
			return -EFAULT;
		}
		set_sensor_type((int)onoff, &gf_dev->sensortype);
		break;
#endif
	case GF_IOC_SPEEDUP:
		if (copy_from_user(&onoff, (void __user *)arg,
				   sizeof(onoff)) != 0) {
			pr_err("Failed to copy speedup from user to kernel\n");
			return -EFAULT;
		}
		if (onoff)
			retval = cpu_speedup_enable(gf_dev->boosting);
		else
			retval = cpu_speedup_disable(gf_dev->boosting);
		break;

	case GF_IOC_GET_ORIENT:
		pr_info("GF_IOC_GET_ORIENT: %d\n",	gf_dev->orient);
		if (copy_to_user((void __user *)arg, &(gf_dev->orient),
					sizeof(gf_dev->orient))) {
			pr_err("Failed to copy data to user\n");
			retval = -EFAULT;
		}
		break;

	default:
		pr_err("doesn't support this command(%x)\n", cmd);
		break;
	}

	return retval;
}

#ifdef CONFIG_COMPAT
static long gw3x_compat_ioctl(struct file *filp, unsigned int cmd,
		unsigned long arg)
{
	int retval = 0;

	retval = filp->f_op->unlocked_ioctl(filp, cmd, arg);

	return retval;
}
#endif

static unsigned int gw3x_poll(struct file *filp,
		struct poll_table_struct *wait)
{
	pr_err("Not support poll opertion in TEE version\n");
	return -EFAULT;
}

static int gw3x_open(struct inode *inode, struct file *filp)
{
	struct gf_device *gf_dev = NULL;
	int retval = -ENXIO;

	pr_info("Entry\n");
	mutex_lock(&device_list_lock);
	list_for_each_entry(gf_dev, &device_list, device_entry) {
		if (gf_dev->devno == inode->i_rdev) {
			pr_info("Found\n");
			retval = 0;
			break;
		}
	}
	mutex_unlock(&device_list_lock);

	if (retval == 0) {
		filp->private_data = gf_dev;
		nonseekable_open(inode, filp);
		pr_info("Success to open device\n");
	} else {
		pr_err("No device for minor %d\n", iminor(inode));
	}
	return retval;
}

static int gw3x_release(struct inode *inode, struct file *filp)
{
	struct gf_device *gf_dev = NULL;

	pr_info("Entry\n");
	gf_dev = filp->private_data;
	if (gf_dev->irq)
		gw3x_disable_irq(gf_dev);
	gf_dev->need_update = 0;
	return 0;
}

static const struct file_operations gw3x_fops = {
	.owner =	THIS_MODULE,
	/* REVISIT switch to aio primitives, so that userspace
	 * gets more complete API coverage.	It'll simplify things
	 * too, except for the locking.
	 */
	.write =	gw3x_write,
	.read =		gw3x_read,
	.unlocked_ioctl = gw3x_ioctl,
#ifdef CONFIG_COMPAT
	.compat_ioctl = gw3x_compat_ioctl,
#endif
	.open =		gw3x_open,
	.release =	gw3x_release,
	.poll	= gw3x_poll,
};


static void gw3x_work_func_debug(struct work_struct *work)
{
	u8 rst_value = -1;
	u8 irq_value = -1;
	struct debug_logger *logger;
	struct gf_device *gf_dev;

	logger = container_of(work, struct debug_logger, work_debug);
	gf_dev = dev_get_drvdata(logger->dev);
	if (gf_dev->reset_gpio)
		rst_value = gpio_get_value(gf_dev->reset_gpio);
	if (gf_dev->irq_gpio)
		irq_value = gpio_get_value(gf_dev->irq_gpio);

	pr_info("ldo: %d, sleep: %d, irq: %d tz: %d type: %s\n",
		gf_dev->ldo_onoff, rst_value, irq_value, gf_dev->tz_mode,
		gf_dev->sensortype > 0 ? gf_dev->chipid : sensor_status[gf_dev->sensortype + 2]);
}

int gw3x_pin_control(struct gf_device *gf_dev, bool pin_set)
{
	int retval = 0;

	pr_info("Pin control entry");
	gf_dev->p->state = NULL;
	if (pin_set) {
		if (!IS_ERR(gf_dev->pins_poweron)) {
			retval = pinctrl_select_state(gf_dev->p, gf_dev->pins_poweron);
			if (retval)
				pr_err("can't set pin wakeup state\n");
			pr_debug("idle\n");
		}
	} else {
		if (!IS_ERR(gf_dev->pins_poweroff)) {
			retval = pinctrl_select_state(gf_dev->p, gf_dev->pins_poweroff);
			if (retval)
				pr_err("can't set pin sleep state\n");
			pr_debug("sleep\n");
		}
	}
	return retval;
}

void gw3x_hw_power_enable(struct gf_device *gf_dev, u8 onoff)
{
	int retval = 0;

	if (onoff && !gf_dev->ldo_onoff) {
		gw3x_pin_control(gf_dev, 1);
		if (gf_dev->pwr_gpio) {
			gpio_set_value(gf_dev->pwr_gpio, 1);
		} else if (gf_dev->regulator_3p3 != NULL) {
			retval = regulator_enable(gf_dev->regulator_3p3);
			if (retval) {
				pr_err("regulator enable failed, rc=%d\n", retval);
				goto regulator_failed;
			}
		}
		if (gf_dev->reset_gpio) {
			usleep_range(11000, 11050);
			gpio_set_value(gf_dev->reset_gpio, 1);
		}
		gf_dev->ldo_onoff = 1;
	} else if (!onoff && gf_dev->ldo_onoff) {
		if (gf_dev->reset_gpio) {
			gpio_set_value(gf_dev->reset_gpio, 0);
			usleep_range(11000, 11050);
		}
		if (gf_dev->pwr_gpio) {
			gpio_set_value(gf_dev->pwr_gpio, 0);
		} else if (gf_dev->regulator_3p3 != NULL) {
			retval = regulator_disable(gf_dev->regulator_3p3);
			if (retval) {
				pr_err("regulator disable failed, rc=%d\n", retval);
				if (gf_dev->reset_gpio)
					gpio_set_value(gf_dev->reset_gpio, 1);
				goto regulator_failed;
			}
		}
		gf_dev->ldo_onoff = 0;
		gw3x_pin_control(gf_dev, 0);
	} else if (onoff == 0 || onoff == 1) {
		pr_err("power is already %s\n",
				(gf_dev->ldo_onoff ? "Enabled" : "Disabled"));
	} else {
		pr_err("can't support this value:%d\n", onoff);
	}
	pr_debug("status = %d\n", gf_dev->ldo_onoff);
regulator_failed:
	return;
}

void gw3x_hw_reset(struct gf_device *gf_dev, u8 delay)
{
	if (gf_dev->reset_gpio) {
		gpio_set_value(gf_dev->reset_gpio, 0);
		usleep_range(3000, 3050);
		gpio_set_value(gf_dev->reset_gpio, 1);
		usleep_range((delay * 1000), (delay * 1000) + 50);
		gf_dev->reset_count++;
	}
}

/* GPIO pins reference */
int gw3x_get_gpio_dts_info(struct device *dev, struct gf_device *gf_dev)
{
	struct device_node *np = dev->of_node;
	int retval = 0;

	gf_dev->p = NULL;
	gf_dev->pins_poweroff = NULL;
	gf_dev->pins_poweron = NULL;

    /* get pwr resource */
	gf_dev->pwr_gpio = of_get_named_gpio(np, "goodix,gpio_pwr", 0);
	if (!gpio_is_valid(gf_dev->pwr_gpio)) {
		pr_info("not use PWR GPIO\n");
		gf_dev->pwr_gpio = 0;
	} else {
		pr_info("goodix_pwr:%d\n", gf_dev->pwr_gpio);
		retval = gpio_request(gf_dev->pwr_gpio, "goodix_pwr");
		if (retval < 0) {
			pr_err("Failed to request PWR GPIO = %d\n", retval);
			return retval;
		}
		gpio_direction_output(gf_dev->pwr_gpio, 0);
	}

	if (of_property_read_string(np, "goodix,btp-regulator", &gf_dev->btp_vdd) < 0) {
		pr_info("not use btp_regulator\n");
		gf_dev->btp_vdd = NULL;
	} else {
		gf_dev->regulator_3p3 = regulator_get(dev, gf_dev->btp_vdd);
		if ((gf_dev->regulator_3p3) == NULL) {
			pr_info("fail to get regulator_3p3\n");
			return -EINVAL;
		} else if (IS_ERR(gf_dev->regulator_3p3)) {
			pr_info("fail to get regulator_3p3: %ld", PTR_ERR(gf_dev->regulator_3p3));
			return PTR_ERR(gf_dev->regulator_3p3);
		}
		pr_info("btp_regulator ok\n");
	}

    /* get reset resource */
	gf_dev->reset_gpio = of_get_named_gpio(np, "goodix,gpio_reset", 0);
	if (!gpio_is_valid(gf_dev->reset_gpio)) {
		pr_err("RESET GPIO is invalid.\n");
		return gf_dev->reset_gpio;
	}
	pr_info("goodix_reset:%d\n", gf_dev->reset_gpio);
	retval = gpio_request(gf_dev->reset_gpio, "goodix_reset");
	if (retval < 0) {
		pr_err("Failed to request RESET GPIO = %d\n", retval);
		return retval;
	}
	gpio_direction_output(gf_dev->reset_gpio, 0);

	 /*get irq resourece*/
	gf_dev->irq_gpio = of_get_named_gpio(np, "goodix,gpio_irq", 0);
	if (!gpio_is_valid(gf_dev->irq_gpio)) {
		pr_err("IRQ GPIO is invalid.\n");
		return gf_dev->irq_gpio;
	}
	pr_info("irq_gpio:%d\n", gf_dev->irq_gpio);
	retval = gpio_request(gf_dev->irq_gpio, "goodix_irq");
	if (retval < 0) {
		pr_err("Failed to request IRQ GPIO. rc = %d\n", retval);
		return retval;
	}
	gpio_direction_input(gf_dev->irq_gpio);

	if (of_property_read_u32(np, "goodix,min_cpufreq_limit",
			&gf_dev->boosting->min_cpufreq_limit))
		gf_dev->boosting->min_cpufreq_limit = 0;

	if (of_property_read_string_index(np, "goodix,chip_id", 0,
			(const char **)&gf_dev->chipid))
		gf_dev->chipid = "NULL";
	pr_info("Chip ID:%s\n", gf_dev->chipid);

	if (of_property_read_u32(np, "goodix,orient", &gf_dev->orient))
		gf_dev->orient = 0;
	pr_info("orient: %d\n", gf_dev->orient);

	if (of_property_read_string_index(np, "goodix,position", 0,
			(const char **)&gf_dev->position))
		gf_dev->position = "NA";
	pr_info("position:%s\n", gf_dev->position);

	if (of_property_read_u32(np, "goodix,spiclk_speed", &gf_dev->clk_setting->spi_speed))
		gf_dev->clk_setting->spi_speed = gw3x_SPI_BAUD_RATE;
#ifndef ENABLE_SENSORS_FPRINT_SECURE
	gf_dev->spi->max_speed_hz = gf_dev->clk_setting->spi_speed;
#endif
	pr_info("spi_speed: %d\n", gf_dev->clk_setting->spi_speed);

	gf_dev->p = pinctrl_get_select_default(dev);
	if (IS_ERR(gf_dev->p)) {
		pr_err("failed pinctrl_get\n");
		return PTR_ERR(gf_dev->p);
	}

#if !defined(ENABLE_SENSORS_FPRINT_SECURE) || defined(DISABLED_GPIO_PROTECTION)
	pr_info("Pin control entry");
	gf_dev->pins_poweroff = pinctrl_lookup_state(gf_dev->p, "pins_poweroff");
#else
	pr_info("Pin control_tz entry");
	gf_dev->pins_poweroff = pinctrl_lookup_state(gf_dev->p, "pins_poweroff_tz");
#endif
	if (IS_ERR(gf_dev->pins_poweroff)) {
		pr_err("could not get pins sleep_state (%li)\n",
			PTR_ERR(gf_dev->pins_poweroff));
		pinctrl_put(gf_dev->p);
		return PTR_ERR(gf_dev->pins_poweroff);
	}

#if !defined(ENABLE_SENSORS_FPRINT_SECURE) || defined(DISABLED_GPIO_PROTECTION)
	pr_info("Pin control entry");
	gf_dev->pins_poweron = pinctrl_lookup_state(gf_dev->p, "pins_poweron");
#else
	pr_info("Pin control_tz entry");
	gf_dev->pins_poweron = pinctrl_lookup_state(gf_dev->p, "pins_poweron_tz");
#endif
	if (IS_ERR(gf_dev->pins_poweron)) {
		pr_err("could not get pins idle_state (%li)\n",
			PTR_ERR(gf_dev->pins_poweron));
		pinctrl_put(gf_dev->p);
		return PTR_ERR(gf_dev->pins_poweron);
	}
	return retval;
}

void gw3x_cleanup_info(struct gf_device *gf_dev)
{
	if (gpio_is_valid(gf_dev->reset_gpio)) {
		gpio_free(gf_dev->reset_gpio);
		pr_debug("remove reset_gpio\n");
	}
	if (gpio_is_valid(gf_dev->pwr_gpio)) {
		gpio_free(gf_dev->pwr_gpio);
		pr_debug("remove pwr_gpio\n");
	}
	if (gf_dev->regulator_3p3) {
		regulator_put(gf_dev->regulator_3p3);
		pr_debug("remove regulator\n");
	}
}

#ifndef ENABLE_SENSORS_FPRINT_SECURE
int gw3x_type_check(struct gf_device *gf_dev)
{
	int retval = -ENODEV;
	unsigned char mcuid[4] = {0x00, 0x00, 0x00, 0x00};
	u32 mcuid32 = 0;
	u32 mcuid24 = 0;

	gw3x_hw_power_enable(gf_dev, 1);
	usleep_range(4950, 5000);
	gw3x_hw_reset(gf_dev, 50);

	gw3x_spi_read_bytes(gf_dev, 0x0000, 4, mcuid);
	mcuid32 = (mcuid[2] << 24 | mcuid[3] << 16 | mcuid[0] << 8 | mcuid[1]);
	mcuid24 = (mcuid32 >> 8);
	pr_info("Sensor read : %x %x %x %x\n",
		mcuid[0], mcuid[1], mcuid[2], mcuid[3]);
	pr_info("Sensor read mcuid32 : 0x%08x\n", mcuid32);
	pr_info("Sensor read mcuid24 : 0x%08x\n", mcuid24);
	if (mcuid24 == GF_GW32J_CHIP_ID) {
		gf_dev->sensortype = SENSOR_OK;
		pr_info("%s sensor type is GW32J\n", gf_dev->chipid);
		retval = 0;
	} else if (mcuid24 == GF_GW32N_CHIP_ID) {
		gf_dev->sensortype = SENSOR_OK;
		pr_info("%s sensor type is GW32N\n", gf_dev->chipid);
		retval = 0;
	} else if (mcuid24 == GF_GW36H_CHIP_ID) {
		gf_dev->sensortype = SENSOR_OK;
		pr_info("%s sensor type is GW36H\n", gf_dev->chipid);
		retval = 0;
	} else if (mcuid24 == GF_GW36C_CHIP_ID) {
		gf_dev->sensortype = SENSOR_OK;
		pr_info("%s sensor type is GW36C\n", gf_dev->chipid);
		retval = 0;
	} else if (mcuid24 == GF_GW36T1_CHIP_ID) {
		gf_dev->sensortype = SENSOR_OK;
		pr_info("%s sensor type is GW36T1\n", gf_dev->chipid);
		retval = 0;
	} else if (mcuid24 == GF_GW36T2_CHIP_ID) {
		gf_dev->sensortype = SENSOR_OK;
		pr_info("%s sensor type is GW36T2\n", gf_dev->chipid);
		retval = 0;
	} else if (mcuid24 == GF_GW36T3_CHIP_ID) {
		gf_dev->sensortype = SENSOR_OK;
		pr_info("%s sensor type is GW36T3\n", gf_dev->chipid);
		retval = 0;
	} else if (mcuid24 == GF_GW36T1_SHIFT_CHIP_ID) {
		gf_dev->sensortype = SENSOR_OK;
		pr_info("%s sensor type is GW36T1 SHIFT\n", gf_dev->chipid);
		retval = 0;
	} else if (mcuid24 == GF_GW36T2_SHIFT_CHIP_ID) {
		gf_dev->sensortype = SENSOR_OK;
		pr_info("%s sensor type is GW36T2 SHIFT\n", gf_dev->chipid);
		retval = 0;
	} else if (mcuid24 == GF_GW36T3_SHIFT_CHIP_ID) {
		gf_dev->sensortype = SENSOR_OK;
		pr_info("%s sensor type is GW36T3 SHIFT\n", gf_dev->chipid);
		retval = 0;
	} else if (mcuid32 == GF_GW39B_CHIP_ID) {
		gf_dev->sensortype = SENSOR_OK;
		pr_info("%s sensor type is GW39B\n", gf_dev->chipid);
		retval = 0;
	}  else if (mcuid32 == GF_GW39U_CHIP_ID) {
		gf_dev->sensortype = SENSOR_OK;
		pr_info("%s sensor type is GW39U\n", gf_dev->chipid);
		retval = 0;
	} else {
		gf_dev->sensortype = SENSOR_FAILED;
		pr_err("sensor type is FAILED 0x%x\n", mcuid32);
	}
	gw3x_hw_power_enable(gf_dev, 0);

	return retval;
}
#endif

static struct gf_device *alloc_platformdata(struct device *dev)
{
	struct gf_device *gf_dev;

	gf_dev = devm_kzalloc(dev, sizeof(struct gf_device), GFP_KERNEL);
	if (gf_dev == NULL)
		return NULL;

	gf_dev->clk_setting = devm_kzalloc(dev, sizeof(*gf_dev->clk_setting), GFP_KERNEL);
	if (gf_dev->clk_setting == NULL)
		return NULL;

	gf_dev->boosting = devm_kzalloc(dev, sizeof(*gf_dev->boosting), GFP_KERNEL);
	if (gf_dev->boosting == NULL)
		return NULL;

	gf_dev->logger = devm_kzalloc(dev, sizeof(*gf_dev->logger), GFP_KERNEL);
	if (gf_dev->logger == NULL)
		return NULL;

	return gf_dev;
}

static int gw3x_probe_common(struct device *dev, struct gf_device *gf_dev)
{
	int retval = -EINVAL;
#ifndef ENABLE_SENSORS_FPRINT_SECURE
	int retry = 0;
#endif
	pr_info("Entry\n");

	spin_lock_init(&gf_dev->spi_lock);
	INIT_LIST_HEAD(&gf_dev->device_entry);

	/* Initialize the driver data */
	gf_dev->dev = dev;
	gf_dev->device_count = 0;
	gf_dev->ldo_onoff = 0;
	gf_dev->pid = 0;
	gf_dev->reset_count = 0;
	gf_dev->interrupt_count = 0;
	g_data = gf_dev;
	gf_dev->logger->dev = dev;
	gf_dev->irq = 0;
	dev_set_drvdata(dev, gf_dev);

	/* get gpio info from dts or defination */
	retval = gw3x_get_gpio_dts_info(dev, gf_dev);
	if (retval < 0) {
		pr_err("Failed to get gpio info:%d\n", retval);
		goto gw3x_probe_get_gpio;
	}
#ifndef ENABLE_SENSORS_FPRINT_SECURE
	if (spi_setup(gf_dev->spi)) {
		pr_err("failed to setup spi conf\n");
		goto gw3x_probe_spi_setup_failed;
	} else {
		pr_info("setup spi success.\n");
	}
	gw3x_spi_setup_conf(gf_dev, 1);
#endif

	/* set AP spectific configuration */
	retval = spi_clk_register(gf_dev->clk_setting, dev);
	if (retval < 0) {
		pr_err("Failed to register spi clk:%d\n", retval);
		goto gw3x_probe_spi_clk_register;
	}

	/* create class */
	gf_dev->class = class_create(THIS_MODULE, GF_CLASS_NAME);
	if (IS_ERR(gf_dev->class)) {
		pr_err("Failed to create class.\n");
		retval = -ENODEV;
		goto gw3x_probe_class_create;
	}

	/* get device no */
	if (GF_DEV_MAJOR > 0) {
		gf_dev->devno = MKDEV(GF_DEV_MAJOR, gf_dev->device_count++);
		retval = register_chrdev_region(gf_dev->devno, 1, GF_DEV_NAME);
	} else {
		retval = alloc_chrdev_region(&gf_dev->devno,
				gf_dev->device_count++, 1, GF_DEV_NAME);
	}
	if (retval < 0) {
		pr_err("Failed to alloc devno.\n");
		goto gw3x_probe_devno;
	} else {
		pr_info("major=%d, minor=%d\n",
				MAJOR(gf_dev->devno), MINOR(gf_dev->devno));
	}

	/* create device */
	gf_dev->fp_device = device_create(gf_dev->class, dev,
			gf_dev->devno, gf_dev, GF_DEV_NAME);
	if (IS_ERR(gf_dev->fp_device)) {
		pr_err("Failed to create device.\n");
		retval = -ENODEV;
		goto gw3x_probe_device;
	} else {
		mutex_lock(&device_list_lock);
		list_add(&gf_dev->device_entry, &device_list);
		mutex_unlock(&device_list_lock);
		pr_info("device create success.\n");
	}

#ifndef ENABLE_SENSORS_FPRINT_SECURE
	do {
		retval = gw3x_type_check(gf_dev);
		pr_info("type (%u), retry (%d)\n", gf_dev->sensortype, retry);
	} while (!gf_dev->sensortype && ++retry < 3);

	if (retval == -ENODEV)
		pr_err("type_check failed\n");
#endif

	/* create sysfs */
	retval = fingerprint_register(gf_dev->fp_device,
		gf_dev, fp_attrs, "fingerprint");
	if (retval) {
		pr_err("sysfs register failed\n");
		goto gw3x_probe_sysfs;
	}

	/* cdev init and add */
	cdev_init(&gf_dev->cdev, &gw3x_fops);
	gf_dev->cdev.owner = THIS_MODULE;
	retval = cdev_add(&gf_dev->cdev, gf_dev->devno, 1);
	if (retval) {
		pr_err("Failed to add cdev.\n");
		goto gw3x_probe_cdev;
	}

	/* netlink interface init */
	retval = gw3x_netlink_init(gf_dev);
	if (retval == -1) {
		pr_err("Failed to init netlink.\n");
		goto gw3x_probe_netlink_init;
	}

#if KERNEL_VERSION(4, 19, 188) > LINUX_VERSION_CODE
	// 4.19 R
	wakeup_source_init(gf_dev->wake_lock, "gw3x_irq_wake_lock");
	// 4.19 Q
	if (!(gf_dev->wake_lock)) {
		gf_dev->wake_lock = wakeup_source_create("gw3x_irq_wake_lock");
		if (gf_dev->wake_lock)
			wakeup_source_add(gf_dev->wake_lock);
	}
#ifdef ENABLE_SENSORS_FPRINT_SECURE
	wakeup_source_init(gf_dev->clk_setting->spi_wake_lock, "gw3x_spi_wake_lock");
	if (!(gf_dev->clk_setting->spi_wake_lock)) {
		gf_dev->clk_setting->spi_wake_lock = wakeup_source_create("gw3x_spi_wake_lock");
		if (gf_dev->clk_setting->spi_wake_lock)
			wakeup_source_add(gf_dev->clk_setting->spi_wake_lock);
	}
#endif
#else
	// 5.4 R
	gf_dev->wake_lock = wakeup_source_register(gf_dev->dev, "gw3x_irq_wake_lock");
#ifdef ENABLE_SENSORS_FPRINT_SECURE
	gf_dev->clk_setting->spi_wake_lock = wakeup_source_register(gf_dev->dev, "gw3x_spi_wake_lock");
#endif
#endif
	gf_dev->irq = gpio_to_irq(gf_dev->irq_gpio);
	irq_set_status_flags(gf_dev->irq, IRQ_DISABLE_UNLAZY);
	retval = request_threaded_irq(gf_dev->irq, NULL, gw3x_irq,
			IRQF_TRIGGER_RISING | IRQF_ONESHOT, "goodix_fp_irq",
			gf_dev);
	if (retval) {
		pr_err("irq thread request failed, retval=%d\n", retval);
		goto gw3x_probe_request_irq;
	}

	enable_irq_wake(gf_dev->irq);
	gf_dev->irq_enabled = 1;
	gw3x_disable_irq(gf_dev);

	g_logger = gf_dev->logger;
	retval = set_fp_debug_timer(gf_dev->logger, gw3x_work_func_debug);
	if (retval)
		goto gw3x_probe_debug_timer;
	enable_fp_debug_timer(gf_dev->logger);

	pr_info("probe finished\n");
	return 0;

gw3x_probe_debug_timer:
	cdev_del(&gf_dev->cdev);
gw3x_probe_request_irq:
	gw3x_netlink_destroy(gf_dev);
gw3x_probe_netlink_init:
	cdev_del(&gf_dev->cdev);
gw3x_probe_cdev:
	fingerprint_unregister(gf_dev->fp_device, fp_attrs);
gw3x_probe_sysfs:
	device_destroy(gf_dev->class, gf_dev->devno);
	list_del(&gf_dev->device_entry);
gw3x_probe_device:
	unregister_chrdev_region(gf_dev->devno, 1);
gw3x_probe_devno:
	class_destroy(gf_dev->class);
gw3x_probe_class_create:
	spi_clk_unregister(gf_dev->clk_setting);
gw3x_probe_spi_clk_register:
#ifndef ENABLE_SENSORS_FPRINT_SECURE
gw3x_probe_spi_setup_failed:
#endif
gw3x_probe_get_gpio:
	pr_err("failed. %d", retval);
	return retval;
}

#ifdef ENABLE_SENSORS_FPRINT_SECURE
static int gw3x_probe(struct platform_device *pdev)
{
	int retval = -ENOMEM;
	struct gf_device *gf_dev;

	pr_info("platform_driver Entry\n");

	gf_dev = alloc_platformdata(&pdev->dev);
	if (gf_dev == NULL)
		goto gw3x_platform_alloc_failed;
	gf_dev->clk_setting->enabled_clk = false;
	gf_dev->tz_mode = true;
	gf_dev->sensortype = SENSOR_UNKNOWN;
	gf_dev->clk_setting->spi_speed = gw3x_SPI_BAUD_RATE;

	retval = gw3x_probe_common(&pdev->dev, gf_dev);
	if (retval)
		goto gw3x_platform_probe_failed;

	pr_info("is successful\n");
	return retval;

gw3x_platform_probe_failed:
	gf_dev = NULL;
gw3x_platform_alloc_failed:
	pr_err("is failed : %d\n", retval);
	return retval;
}
#else
static int gw3x_probe(struct spi_device *spi)
{
	int retval = -ENOMEM;
	struct gf_device *gf_dev;

	pr_info("spi_driver Entry\n");

	gf_dev = alloc_platformdata(&spi->dev);
	if (gf_dev == NULL)
		goto gw3x_spi_alloc_failed;

	spi->mode = SPI_MODE_0;
	spi->max_speed_hz = gw3x_SPI_BAUD_RATE;
	spi->bits_per_word = 8;
	gf_dev->clk_setting->spi_speed = gw3x_SPI_BAUD_RATE;
	gf_dev->prev_bits_per_word = 8;
	gf_dev->tz_mode = false;
	gf_dev->spi = spi;
	mutex_init(&gf_dev->buf_lock);

	/* init transfer buffer */
	retval = gw3x_init_buffer(gf_dev);
	if (retval < 0) {
		pr_err("Failed to Init transfer buffer.\n");
		goto gw3x_spi_init_buffer_failed;
	}

	retval = gw3x_probe_common(&spi->dev, gf_dev);
	if (retval)
		goto gw3x_spi_probe_failed;

	pr_info("is successful\n");
	return retval;

gw3x_spi_probe_failed:
	gw3x_free_buffer(gf_dev);
gw3x_spi_init_buffer_failed:
	mutex_destroy(&gf_dev->buf_lock);
	gf_dev->spi = NULL;
	gf_dev = NULL;
gw3x_spi_alloc_failed:
	pr_err("is failed : %d\n", retval);
	return retval;
}
#endif

static int gw3x_remove_common(struct device *dev)
{
	struct gf_device *gf_dev = dev_get_drvdata(dev);

	pr_info("Entry\n");

	/* make sure ops on existing fds can abort cleanly */
	if (gf_dev->irq) {
		free_irq(gf_dev->irq, gf_dev);
		gf_dev->irq_enabled = 0;
		gf_dev->irq = 0;
	}

	gw3x_hw_power_enable(gf_dev, 0);
	spi_clk_unregister(gf_dev->clk_setting);
	disable_fp_debug_timer(gf_dev->logger);
	wakeup_source_unregister(gf_dev->wake_lock);
#ifdef ENABLE_SENSORS_FPRINT_SECURE
	wakeup_source_unregister(gf_dev->clk_setting->spi_wake_lock);
#endif
	gw3x_cleanup_info(gf_dev);
	fingerprint_unregister(gf_dev->fp_device, fp_attrs);
	gw3x_netlink_destroy(gf_dev);
	cdev_del(&gf_dev->cdev);
	device_destroy(gf_dev->class, gf_dev->devno);
	list_del(&gf_dev->device_entry);

	unregister_chrdev_region(gf_dev->devno, 1);
	class_destroy(gf_dev->class);
	return 0;
}

#ifdef ENABLE_SENSORS_FPRINT_SECURE
static int gw3x_remove(struct platform_device *pdev)
{
	struct gf_device *gf_dev = platform_get_drvdata(pdev);

	gw3x_remove_common(&pdev->dev);
	gf_dev = NULL;
	return 0;
}

#else
#if LINUX_VERSION_CODE > KERNEL_VERSION(6, 1, 0)
static void gw3x_remove(struct spi_device *spi)
{
	struct gf_device *gf_dev = spi_get_drvdata(spi);

	gw3x_free_buffer(gf_dev);
	gw3x_remove_common(&spi->dev);

	mutex_destroy(&gf_dev->buf_lock);
	spin_lock_irq(&gf_dev->spi_lock);
	gf_dev->spi = NULL;
	spin_unlock_irq(&gf_dev->spi_lock);
	gf_dev = NULL;
}
#else
static int gw3x_remove(struct spi_device *spi)
{
	struct gf_device *gf_dev = spi_get_drvdata(spi);

	gw3x_free_buffer(gf_dev);
	gw3x_remove_common(&spi->dev);

	mutex_destroy(&gf_dev->buf_lock);
	spin_lock_irq(&gf_dev->spi_lock);
	gf_dev->spi = NULL;
	spin_unlock_irq(&gf_dev->spi_lock);
	gf_dev = NULL;
	return 0;
}
#endif
#endif

static int gw3x_pm_suspend(struct device *dev)
{
	struct gf_device *gf_dev = dev_get_drvdata(dev);

	pr_info("Entry\n");
	disable_fp_debug_timer(gf_dev->logger);
	return 0;
}

static int gw3x_pm_resume(struct device *dev)
{
	struct gf_device *gf_dev = dev_get_drvdata(dev);

	pr_info("Entry\n");
	enable_fp_debug_timer(gf_dev->logger);
	return 0;
}

static const struct dev_pm_ops gw3x_pm_ops = {
	.suspend = gw3x_pm_suspend,
	.resume = gw3x_pm_resume
};

#ifndef ENABLE_SENSORS_FPRINT_SECURE
static struct spi_driver gw3x_spi_driver = {
#else
static struct platform_driver gw3x_spi_driver = {
#endif
	.driver = {
		.name = GF_DEV_NAME,
#ifndef ENABLE_SENSORS_FPRINT_SECURE
		.bus = &spi_bus_type,
#endif
		.owner = THIS_MODULE,
		.pm = &gw3x_pm_ops,
		.of_match_table = gw3x_of_match,
	},
	.probe = gw3x_probe,
	.remove = gw3x_remove,
};

static int __init gw3x_init(void)
{
	int status = 0;

	pr_info("Entry\n");

#ifndef ENABLE_SENSORS_FPRINT_SECURE
	status = spi_register_driver(&gw3x_spi_driver);
#else
	status = platform_driver_register(&gw3x_spi_driver);
#endif
	if (status < 0) {
		pr_err("Failed to register SPI driver.\n");
		return -EINVAL;
	}
	return status;
}
module_init(gw3x_init);

static void __exit gw3x_exit(void)
{
	pr_info("Entry\n");
#ifndef ENABLE_SENSORS_FPRINT_SECURE
	spi_unregister_driver(&gw3x_spi_driver);
#else
	platform_driver_unregister(&gw3x_spi_driver);
#endif
}
module_exit(gw3x_exit);

MODULE_AUTHOR("fp.sec@samsung.com");
MODULE_DESCRIPTION("Samsung Electronics Inc. gw3x driver");
MODULE_LICENSE("GPL");
