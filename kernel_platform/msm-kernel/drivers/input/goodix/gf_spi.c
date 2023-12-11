/*
 * TEE driver for goodix fingerprint sensor
 * Copyright (C) 2016 Goodix
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
 */
#define DEBUG
#define pr_fmt(fmt)		KBUILD_MODNAME ": " fmt

#include <linux/init.h>
#include <linux/module.h>
#include <linux/ioctl.h>
#include <linux/fs.h>
#include <linux/device.h>
#include <linux/input.h>
#include <linux/clk.h>
#include <linux/err.h>
#include <linux/list.h>
#include <linux/errno.h>
#include <linux/mutex.h>
#include <linux/slab.h>
#include <linux/compat.h> 
#include <linux/delay.h>
#include <linux/uaccess.h>
#include <linux/ktime.h>
#include <linux/interrupt.h>
#include <linux/proc_fs.h>
#include <linux/irq.h>
#include <linux/gpio.h>
#include <linux/regulator/consumer.h>
#include <linux/of_gpio.h>
#include <linux/timer.h>
#include <linux/pm_qos.h>
#include <linux/cpufreq.h>
#include <linux/pm_wakeup.h>
#include <drm/drm_bridge.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/timer.h>
#include <linux/time.h>
#include <linux/types.h>
#include <net/sock.h>
#include <net/netlink.h>

#include <linux/delay.h>
#include <linux/workqueue.h>
#include <linux/of_gpio.h>
#include <linux/gpio.h>
#include <linux/regulator/consumer.h>
#include <linux/timer.h>
#include <linux/err.h>
#include "gf_spi.h"


#ifdef FB_CALLBACK_NOTITY
#include <drm/drm_notifier.h>
#include <linux/notifier.h>
#include <linux/fb.h>
#endif

#if defined(USE_SPI_BUS)
#include <linux/spi/spi.h>
#include <linux/spi/spidev.h>
#elif defined(USE_PLATFORM_BUS)
#include <linux/platform_device.h>
#endif

#define NETLINK_TEST 25
#define MAX_MSGSIZE 32

#define PROC_NAME  "fp_info"
static struct proc_dir_entry *proc_entry;

#ifdef SAMSUNG_FP_SUPPORT
#define FINGERPRINT_ADM_CLASS_NAME "fingerprint"
#endif

int stringlength(char *s);
void sendnlmsg(char *message);
static int pid = -1;
static int gf_fp_adm = 0;
static int  gf_adm_release = 0;
struct sock *nl_sk;

#define VER_MAJOR	1
#define VER_MINOR	2
#define PATCH_LEVEL 1

#define WAKELOCK_HOLD_TIME 2000	/* in ms */

#define GF_SPIDEV_NAME	   "goodix,fingerprint"
/*device name after register in charater*/
#define GF_DEV_NAME			"goodix_fp"
#define GF_INPUT_NAME		"uinput-goodix"	/*"goodix_fp" */

#define	CHRD_DRIVER_NAME	"goodix_fp_spi"
#define	CLASS_NAME			"goodix_fp"

#define N_SPI_MINORS		32	/* ... up to 256 */

#if defined(USE_SPI_BUS)
#include <linux/spi/spi.h>
#include <linux/spi/spidev.h>
#elif defined(USE_PLATFORM_BUS)
#include <linux/platform_device.h>
#endif

#define FAIL (-1)

static int SPIDEV_MAJOR;

static DECLARE_BITMAP(minors, N_SPI_MINORS);
static LIST_HEAD(device_list);
static DEFINE_MUTEX(device_list_lock);
static struct wakeup_source fp_wakelock;
static struct gf_dev gf;

#ifdef SAMSUNG_FP_SUPPORT
static ssize_t adm_show(struct device *dev,struct device_attribute *attr,char *buf)
{
  return snprintf(buf,3,"1\n");
}

static DEVICE_ATTR(adm, 0664, adm_show, NULL);
struct class *fingerprint_adm_class;
struct device *fingerprint_adm_dev;
#endif

struct gf_key_map maps[] = {
	{EV_KEY, GF_KEY_INPUT_HOME},
	{EV_KEY, GF_KEY_INPUT_MENU},
	{EV_KEY, GF_KEY_INPUT_BACK},
	{EV_KEY, GF_KEY_INPUT_POWER},
#if defined(SUPPORT_NAV_EVENT)
	{EV_KEY, GF_NAV_INPUT_UP},
	{EV_KEY, GF_NAV_INPUT_DOWN},
	{EV_KEY, GF_NAV_INPUT_RIGHT},
	{EV_KEY, GF_NAV_INPUT_LEFT},
	{EV_KEY, GF_KEY_INPUT_CAMERA},
	{EV_KEY, GF_NAV_INPUT_CLICK},
	{EV_KEY, GF_NAV_INPUT_DOUBLE_CLICK},
	{EV_KEY, GF_NAV_INPUT_LONG_PRESS},
	{EV_KEY, GF_NAV_INPUT_HEAVY},
#endif
};

#ifdef CONFIG_FP_DRM
static struct drm_panel *active_panel;

static void goodix_fb_state_chg_callback(enum panel_event_notifier_tag tag,
		 struct panel_event_notification *notification, void *client_data)
{
	char temp[4] = { 0x0 };
	temp[0] = GF_NET_EVENT_IRQ;
	if (!notification) {
		pr_err("Invalid notification\n");
		return;
	}

	pr_err("Notification type:%d, early_trigger:%d",
			notification->notif_type,
			notification->notif_data.early_trigger);

	switch (notification->notif_type) {
	case DRM_PANEL_EVENT_UNBLANK:
		pr_err("screen on:  Notification type:%d\n", notification->notif_type);
                if (notification->notif_data.early_trigger){
			temp[0] = GF_NET_EVENT_FB_UNBLACK;
			sendnlmsg(temp);
                }
		break;
	case DRM_PANEL_EVENT_BLANK:
		pr_err("screen off:  Notification type:%d\n", notification->notif_type);
                if (notification->notif_data.early_trigger){
			temp[0] = GF_NET_EVENT_FB_BLACK;
			sendnlmsg(temp);
                }
		break;
	default:
		pr_err("default:  Notification type:%d\n", notification->notif_type);
		break;
	}
}

static void gf_register_for_panel_events(void)
{
	void *cookie = NULL;
	pr_err("registered for panel notifications panel enter\n");
	cookie = panel_event_notifier_register(PANEL_EVENT_NOTIFICATION_PRIMARY,
			PANEL_EVENT_NOTIFIER_CLIENT_FINGERPRINT_GOODIX, active_panel,
			&goodix_fb_state_chg_callback, &gf);
	if (!cookie) {
		pr_err("Failed to register for panel events\n");
		return;
	}

	pr_err("registered for panel notifications panel\n");

	gf.drm_notifier = cookie;
}

#endif

static int proc_show_ver(struct seq_file *file,void *v)
{
	seq_printf(file,"[Fingerprint]:goodix_fp  [IC]:GF3626\n");
	return 0;
}

static int proc_open(struct inode *inode,struct file *file)
{
	pr_info("goodix proc_open\n");
	single_open(file,proc_show_ver,NULL);
	return 0;
}

static const struct proc_ops proc_file_fpc_ops = {
	.proc_open = proc_open,
	.proc_read = seq_read,
	.proc_release = single_release,
};

int gf_parse_dts(struct gf_dev *gf_dev)
{
	int i = 0;
	int count = 0;
	struct device_node *node = NULL;
	struct drm_panel *panel = NULL;

#ifdef GF_PW_CTL
	int rc = 0;
	/*get pwr resource */
	gf_dev->pwr_gpio =
	    of_get_named_gpio(gf_dev->spi->dev.of_node, "power_enable", 0);
	if (!gpio_is_valid(gf_dev->pwr_gpio)) {
		pr_err("PWR GPIO is invalid.\n");
		return FAIL;
	}
	rc = gpio_request(gf_dev->pwr_gpio, "power_enable");
	if (rc) {
		dev_err(&gf_dev->spi->dev,
			"Failed to request PWR GPIO. rc = %d\n", rc);
		return FAIL;
	}
#endif

	/*get reset resource */
	gf_dev->reset_gpio =
	    of_get_named_gpio(gf_dev->spi->dev.of_node, "fp-gpio-reset", 0);
	if (!gpio_is_valid(gf_dev->reset_gpio)) {
		pr_err("RESET GPIO is invalid.\n");
		return -EPERM;
	}

	/*get irq resourece */
	gf_dev->irq_gpio = of_get_named_gpio(gf_dev->spi->dev.of_node, "fp-gpio-irq", 0);
	pr_err("gf::irq_gpio:%d\n", gf_dev->irq_gpio);
	if (!gpio_is_valid(gf_dev->irq_gpio)) {
		pr_err("IRQ GPIO is invalid.\n");
		return -EPERM;
	}
#ifdef CONFIG_FP_DRM
	pr_err("find drm_panel count  in\n");
	count = of_count_phandle_with_args(gf_dev->spi->dev.of_node, "panel", NULL);
	if (count <= 0) {
		pr_err("find drm_panel count(%d) fail", count);
		return 0;
	}
	pr_err("find drm_panel count(%d) ", count);
	for (i = 0; i < count; i++) {
		node = of_parse_phandle(gf_dev->spi->dev.of_node, "panel", i);
		pr_err("node = %s\n", node->name);

		panel = of_drm_find_panel(node);
		pr_err("panel = %p \n", panel);

		of_node_put(node);
		if (!IS_ERR(panel)) {
			pr_err("find drm_panel successfully");
			active_panel = panel;
			//return 0;
		}
	}
#endif
	return 0;
}

void gf_cleanup(struct gf_dev *gf_dev)
{
	pr_info("[info] %s\n", __func__);
	if (gpio_is_valid(gf_dev->irq_gpio)) {
		gpio_free(gf_dev->irq_gpio);
		pr_info("remove irq_gpio success\n");
	}
	if (gpio_is_valid(gf_dev->reset_gpio)) {
		gpio_free(gf_dev->reset_gpio);
		pr_info("remove reset_gpio success\n");
	}
#ifdef GF_PW_CTL
	if (gpio_is_valid(gf_dev->pwr_gpio)) {
		gpio_free(gf_dev->pwr_gpio);
		pr_info("remove pwr_gpio success\n");
	}
#endif
}

int gf_power_on(struct gf_dev *gf_dev)
{
	int rc = 0;

#ifdef GF_PW_CTL
	if (gpio_is_valid(gf_dev->pwr_gpio)) {
		rc = gpio_direction_output(gf_dev->pwr_gpio, 1);
		pr_info("---- power on result: %d----\n", rc);
	} else {
		pr_info("%s: gpio_is_invalid\n", __func__);
	}
#endif

	msleep(10);
	gpio_direction_output(gf_dev->reset_gpio, 1);
	return rc;
}

int gf_power_off(struct gf_dev *gf_dev)
{
	int rc = 0;
#ifdef GF_PW_CTL
	if (gpio_is_valid(gf_dev->pwr_gpio)) {
		rc = gpio_direction_output(gf_dev->pwr_gpio, 0);
		pr_info("---- power off result: %d----\n", rc);
	} else {
		pr_info("%s: gpio_is_invalid\n", __func__);
	}
#endif
	msleep(10);
	gpio_direction_output(gf_dev->reset_gpio, 0);
	return rc;
}

int gf_hw_reset(struct gf_dev *gf_dev, unsigned int delay_ms)
{
	if (gf_dev == NULL) {
		pr_info("Input buff is NULL.\n");
		return -EPERM;
	}
	gpio_direction_output(gf_dev->reset_gpio, 0);
	mdelay(10);
	gpio_set_value(gf_dev->reset_gpio, 1);
	mdelay(delay_ms);
	pr_info("%s success\n", __func__);
	return 0;
}

int gf_irq_num(struct gf_dev *gf_dev)
{
	if (gf_dev == NULL) {
		pr_info("Input buff is NULL.\n");
		return -EPERM;
	} else {
		return gpio_to_irq(gf_dev->irq_gpio);
	}
}


void sendnlmsg(char *message)
{
	struct sk_buff *skb_1;
	struct nlmsghdr *nlh;
	int len = NLMSG_SPACE(MAX_MSGSIZE);
	int slen = 0;
	int ret = 0;
	if (!message || !nl_sk || !pid) {
		return;
	}
	skb_1 = alloc_skb(len, GFP_KERNEL);
	if (!skb_1) {
		pr_err("alloc_skb error\n");
		return;
	}
	slen = strlen(message);
	nlh = nlmsg_put(skb_1, 0, 0, 0, MAX_MSGSIZE, 0);

	NETLINK_CB(skb_1).portid = 0;
	NETLINK_CB(skb_1).dst_group = 0;

	message[slen] = '\0';
	memcpy(NLMSG_DATA(nlh), message, slen + 1);

	ret = netlink_unicast(nl_sk, skb_1, pid, MSG_DONTWAIT);
	if (!ret) {
		/*kfree_skb(skb_1); */
		pr_err("send msg from kernel to usespace failed ret 0x%x\n",
		       ret);
	}

}

void nl_data_ready(struct sk_buff *__skb)
{
	struct sk_buff *skb;
	struct nlmsghdr *nlh;
	char str[100];
	skb = skb_get(__skb);
	if (skb->len >= NLMSG_SPACE(0)) {
		nlh = nlmsg_hdr(skb);
		memcpy(str, NLMSG_DATA(nlh), sizeof(str));
		pid = nlh->nlmsg_pid;
		kfree_skb(skb);
	}
}

int netlink_init(void)
{
	struct netlink_kernel_cfg netlink_cfg;
	memset(&netlink_cfg, 0, sizeof(struct netlink_kernel_cfg));

	netlink_cfg.groups = 0;
	netlink_cfg.flags = 0;
	netlink_cfg.input = nl_data_ready;
	netlink_cfg.cb_mutex = NULL;

	nl_sk = netlink_kernel_create(&init_net, NETLINK_TEST, &netlink_cfg);

	if (!nl_sk) {
		pr_err("create netlink socket error\n");
		return 1;
	}
	return 0;
}

void netlink_exit(void)
{
	if (nl_sk != NULL) {
		netlink_kernel_release(nl_sk);
		nl_sk = NULL;
	}
	pr_info("self module exited\n");
}


static void gf_enable_irq(struct gf_dev *gf_dev)
{
	if (gf_dev->irq_enabled) {
		pr_warn("IRQ has been enabled.\n");
	} else {
		enable_irq(gf_dev->irq);
		gf_dev->irq_enabled = 1;
	}
}

static void gf_disable_irq(struct gf_dev *gf_dev)
{
	if (gf_dev->irq_enabled) {
		gf_dev->irq_enabled = 0;
		disable_irq(gf_dev->irq);
	} else {
		pr_warn("IRQ has been disabled.\n");
	}
}

#if defined SUPPORT_NAV_EVENT
static void nav_event_input(struct gf_dev *gf_dev, gf_nav_event_t nav_event)
{
	uint32_t nav_input = 0;

	switch (nav_event) {
	case GF_NAV_FINGER_DOWN:
		pr_debug("%s nav finger down\n", __func__);
		break;

	case GF_NAV_FINGER_UP:
		pr_debug("%s nav finger up\n", __func__);
		break;

	case GF_NAV_DOWN:
		nav_input = GF_NAV_INPUT_DOWN;
		pr_debug("%s nav down\n", __func__);
		break;

	case GF_NAV_UP:
		nav_input = GF_NAV_INPUT_UP;
		pr_debug("%s nav up\n", __func__);
		break;

	case GF_NAV_LEFT:
		nav_input = GF_NAV_INPUT_LEFT;
		pr_debug("%s nav left\n", __func__);
		break;

	case GF_NAV_RIGHT:
		nav_input = GF_NAV_INPUT_RIGHT;
		pr_debug("%s nav right\n", __func__);
		break;

	case GF_NAV_CLICK:
		nav_input = GF_NAV_INPUT_CLICK;
		pr_debug("%s nav click\n", __func__);
		break;

	case GF_NAV_HEAVY:
		nav_input = GF_NAV_INPUT_HEAVY;
		pr_debug("%s nav heavy\n", __func__);
		break;

	case GF_NAV_LONG_PRESS:
		nav_input = GF_NAV_INPUT_LONG_PRESS;
		pr_debug("%s nav long press\n", __func__);
		break;

	case GF_NAV_DOUBLE_CLICK:
		nav_input = GF_NAV_INPUT_DOUBLE_CLICK;
		pr_debug("%s nav double click\n", __func__);
		break;

	default:
		pr_warn("%s unknown nav event: %d\n", __func__, nav_event);
		break;
	}

	if ((nav_event != GF_NAV_FINGER_DOWN)
	    && (nav_event != GF_NAV_FINGER_UP)) {
		input_report_key(gf_dev->input, nav_input, 1);
		input_sync(gf_dev->input);
		input_report_key(gf_dev->input, nav_input, 0);
		input_sync(gf_dev->input);
	}
}
#endif

static void gf_kernel_key_input(struct gf_dev *gf_dev, struct gf_key *gf_key)
{
	uint32_t key_input = 0;

	if (GF_KEY_HOME == gf_key->key) {
		key_input = GF_KEY_INPUT_HOME;
	} else if (GF_KEY_POWER == gf_key->key) {
		key_input = GF_KEY_INPUT_POWER;
	} else if (GF_KEY_CAMERA == gf_key->key) {
		key_input = GF_KEY_INPUT_CAMERA;
	} else {
		/* add special key define */
		key_input = gf_key->key;
	}
	pr_debug("%s: received key event[%d], key=%d, value=%d\n",
		 __func__, key_input, gf_key->key, gf_key->value);

	if ((GF_KEY_POWER == gf_key->key || GF_KEY_CAMERA == gf_key->key)
	    && (gf_key->value == 1)) {
		input_report_key(gf_dev->input, key_input, 1);
		input_sync(gf_dev->input);
		input_report_key(gf_dev->input, key_input, 0);
		input_sync(gf_dev->input);
	}

	if (GF_KEY_HOME == gf_key->key) {
		input_report_key(gf_dev->input, key_input, gf_key->value);
		input_sync(gf_dev->input);
	}
}

static long gf_ioctl(struct file *filp, unsigned int cmd, unsigned long arg)
{
	struct gf_dev *gf_dev = &gf;
	struct gf_key gf_key;
#if defined(SUPPORT_NAV_EVENT)
	gf_nav_event_t nav_event = GF_NAV_NONE;
#endif
	int retval = 0;
	u8 netlink_route = NETLINK_TEST;
	struct gf_ioc_chip_info info;

	if (_IOC_TYPE(cmd) != GF_IOC_MAGIC)
		return -ENODEV;

	if (_IOC_DIR(cmd) & _IOC_READ)
		retval = !access_ok((void __user *)arg, _IOC_SIZE(cmd));//kernel5.0 maybe use this 2 params
	else if (_IOC_DIR(cmd) & _IOC_WRITE)
		retval =!access_ok((void __user *)arg, _IOC_SIZE(cmd));//kernel5.0 maybe use this 2 params
	if (retval)
		return -EFAULT;

	if (gf_dev->device_available == 0) {
		if ((cmd == GF_IOC_ENABLE_POWER)
		    || (cmd == GF_IOC_DISABLE_POWER)) {
			pr_debug("power cmd\n");
		} else {
			pr_debug
			    ("get cmd %d, but sensor is power off currently.\n",
			     _IOC_NR(cmd));
			return -ENODEV;
		}
	}

	switch (cmd) {
	case GF_IOC_INIT:
		pr_debug("%s GF_IOC_INIT\n", __func__);
		proc_entry = proc_create(PROC_NAME, 0644, NULL, &proc_file_fpc_ops);
		if (NULL == proc_entry) {
			pr_info("goodix Couldn't create proc entry!");
			return -ENOMEM;
		} else {
			pr_info("goodix Create proc entry success!");
		}

		if (copy_to_user
		    ((void __user *)arg, (void *)&netlink_route, sizeof(u8))) {
			retval = -EFAULT;
			break;
		}

               #ifdef SAMSUNG_FP_SUPPORT
		if(gf_fp_adm){
                       gf_fp_adm = 0;
		       fingerprint_adm_class = class_create(THIS_MODULE,FINGERPRINT_ADM_CLASS_NAME);
		       if (IS_ERR(fingerprint_adm_class))
		       {
			 pr_debug( "%s : fingerprint_adm_class error \n", __func__);
			 return PTR_ERR(fingerprint_adm_class);
		       }

		       fingerprint_adm_dev = device_create(fingerprint_adm_class, NULL, 0,
				                           NULL, FINGERPRINT_ADM_CLASS_NAME);

		       if (sysfs_create_file(&(fingerprint_adm_dev->kobj), &dev_attr_adm.attr))
		       {
			 pr_debug(" %s : fail to creat fingerprint_adm_device %d \n", __func__, &dev_attr_adm.attr);
		       }else{
		         pr_debug(" %s : success to creat fingerprint_adm_device %d \n", __func__, &dev_attr_adm.attr);
		       }
                       gf_adm_release = 1;
               }
               #endif

		break;
	case GF_IOC_EXIT:
		pr_debug("%s GF_IOC_EXIT\n", __func__);
		break;
	case GF_IOC_DISABLE_IRQ:
		pr_debug("%s GF_IOC_DISABEL_IRQ\n", __func__);
		gf_disable_irq(gf_dev);
		break;
	case GF_IOC_ENABLE_IRQ:
		pr_debug("%s GF_IOC_ENABLE_IRQ\n", __func__);
		gf_enable_irq(gf_dev);
		break;
	case GF_IOC_RESET:
		pr_debug("%s GF_IOC_RESET.\n", __func__);
		gf_hw_reset(gf_dev, 3);
		break;
	case GF_IOC_INPUT_KEY_EVENT:
		if (copy_from_user
		    (&gf_key, (struct gf_key *)arg, sizeof(struct gf_key))) {
			pr_debug
			    ("Failed to copy input key event from user to kernel\n");
			retval = -EFAULT;
			break;
		}

		gf_kernel_key_input(gf_dev, &gf_key);
		break;
#if defined(SUPPORT_NAV_EVENT)
	case GF_IOC_NAV_EVENT:
		pr_debug("%s GF_IOC_NAV_EVENT\n", __func__);
		if (copy_from_user
		    (&nav_event, (gf_nav_event_t *) arg,
		     sizeof(gf_nav_event_t))) {
			pr_debug
			    ("Failed to copy nav event from user to kernel\n");
			retval = -EFAULT;
			break;
		}

		nav_event_input(gf_dev, nav_event);
		break;
#endif

	case GF_IOC_ENABLE_SPI_CLK:
		break;
	case GF_IOC_DISABLE_SPI_CLK:
		break;
	case GF_IOC_ENABLE_POWER:
		pr_debug("%s GF_IOC_ENABLE_POWER\n", __func__);
		if (gf_dev->device_available == 1)
			pr_debug("Sensor has already powered-on.\n");
		else
			gf_power_on(gf_dev);
		gf_dev->device_available = 1;
		break;
	case GF_IOC_DISABLE_POWER:
		pr_debug("%s GF_IOC_DISABLE_POWER\n", __func__);
		if (gf_dev->device_available == 0)
			pr_debug("Sensor has already powered-off.\n");
		else
			gf_power_off(gf_dev);
		gf_dev->device_available = 0;
		break;
	case GF_IOC_ENTER_SLEEP_MODE:
		pr_debug("%s GF_IOC_ENTER_SLEEP_MODE\n", __func__);
		break;
	case GF_IOC_GET_FW_INFO:
		pr_debug("%s GF_IOC_GET_FW_INFO\n", __func__);
		break;
	case GF_IOC_REMOVE:
		pr_debug("%s GF_IOC_REMOVE\n", __func__);
		break;
	case GF_IOC_CHIP_INFO:
		pr_debug("%s GF_IOC_CHIP_INFO\n", __func__);
		if (copy_from_user
		    (&info, (struct gf_ioc_chip_info *)arg,
		     sizeof(struct gf_ioc_chip_info))) {
			retval = -EFAULT;
			break;
		}
		pr_debug("vendor_id : 0x%x\n", info.vendor_id);
		pr_debug("mode : 0x%x\n", info.mode);
		pr_debug("operation: 0x%x\n", info.operation);
		break;
	default:
		pr_warn("unsupport cmd:0x%x\n", cmd);
		break;
	}

	return retval;
}

#ifdef CONFIG_COMPAT
static long gf_compat_ioctl(struct file *filp, unsigned int cmd,
			    unsigned long arg)
{
	return gf_ioctl(filp, cmd, (unsigned long)compat_ptr(arg));
}
#endif /*CONFIG_COMPAT */

static irqreturn_t gf_irq(int irq, void *handle)
{
#if defined(GF_NETLINK_ENABLE)
	char temp[4] = { 0x0 };
	temp[0] = GF_NET_EVENT_IRQ;
	__pm_wakeup_event(&fp_wakelock, WAKELOCK_HOLD_TIME);
	sendnlmsg(temp);
#endif

	return IRQ_HANDLED;
}

static int gf_open(struct inode *inode, struct file *filp)
{
	struct gf_dev *gf_dev;
	int status = -ENXIO;
	int rc = 0;

	mutex_lock(&device_list_lock);

	list_for_each_entry(gf_dev, &device_list, device_entry) {
		if (gf_dev->devt == inode->i_rdev) {
			pr_debug("Found\n");
			status = 0;
			break;
		}
	}

#ifdef CONFIG_FINGERPRINT_FP_VREG_CONTROL
	pr_info("Try to enable fp_vdd_vreg\n");
	gf_dev->vreg = regulator_get(&gf_dev->spi->dev, "fp_vdd_vreg");
	if (gf_dev->vreg == NULL) {
		dev_err(&gf_dev->spi->dev,
			"fp_vdd_vreg regulator get failed!\n");
		mutex_unlock(&device_list_lock);
		return -EPERM;
	}

	if (regulator_is_enabled(gf_dev->vreg)) {
		pr_info("fp_vdd_vreg is already enabled!\n");
	} else {
		rc = regulator_enable(gf_dev->vreg);
		if (rc) {
			dev_err(&gf_dev->spi->dev,
				"error enabling fp_vdd_vreg!\n");
			regulator_put(gf_dev->vreg);
			gf_dev->vreg = NULL;
			mutex_unlock(&device_list_lock);
			return -EPERM;
		}
	}
	pr_info("fp_vdd_vreg is enabled!\n");
#endif

	if (status == 0) {
		gf_parse_dts(gf_dev);
        gf_dev->irq = gf_irq_num(gf_dev);
		pr_err("gf_dev->irq = %d\n",gf_dev->irq);
		rc = gpio_request(gf_dev->reset_gpio, "fp-gpio-reset");
		if (rc) {
			dev_err(&gf_dev->spi->dev,
				"Failed to request RESET GPIO. rc = %d\n", rc);
			mutex_unlock(&device_list_lock);
			return -EPERM;
		}
		gpio_direction_output(gf_dev->pwr_gpio, 0);
		gpio_direction_output(gf_dev->reset_gpio, 0);
		mdelay(20);
		pr_err("goodixfp -------0601-test\n");
		rc = gpio_request(gf_dev->irq_gpio, "fp-gpio-irq");
		if (rc) {
			dev_err(&gf_dev->spi->dev,
				"Failed to request IRQ GPIO. rc = %d\n", rc);
			mutex_unlock(&device_list_lock);
			return -EPERM;
		}
		gpio_direction_input(gf_dev->irq_gpio);

		rc = request_threaded_irq(gf_dev->irq, gf_irq, NULL,
					  IRQF_TRIGGER_RISING | IRQF_ONESHOT,
					  "gf", gf_dev);

		if (!rc) {
			enable_irq_wake(gf_dev->irq);
			gf_dev->irq_enabled = 1;
			gf_disable_irq(gf_dev);
		}

		gf_dev->users++;
		filp->private_data = gf_dev;
		nonseekable_open(inode, filp);
	#ifdef CONFIG_FP_DRM
		gf_register_for_panel_events();
	#endif
		pr_debug("Succeed to open device. irq = %d\n", gf_dev->irq);
	} else {
		pr_debug("No device for minor %d\n", iminor(inode));
	}
	mutex_unlock(&device_list_lock);
	return status;
}

static int gf_release(struct inode *inode, struct file *filp)
{
	struct gf_dev *gf_dev;
	int status = 0;
	pr_debug("%s\n", __func__);
	mutex_lock(&device_list_lock);
	gf_dev = filp->private_data;
	filp->private_data = NULL;

	/*
	 *Disable fp_vdd_vreg regulator
	 */
#ifdef CONFIG_FINGERPRINT_FP_VREG_CONTROL
	pr_info("disable fp_vdd_vreg!\n");
	if (regulator_is_enabled(gf_dev->vreg)) {
		regulator_disable(gf_dev->vreg);
		regulator_put(gf_dev->vreg);
		gf_dev->vreg = NULL;
	}
#endif

	/*last close?? */
	gf_dev->users--;
	if (!gf_dev->users) {
		if (NULL == proc_entry) {
			pr_info("goodixfp fp_info is null!");
		} else {
			remove_proc_entry(PROC_NAME,NULL);
			pr_info("goodixfp fp_info remove!");
		}
                #ifdef SAMSUNG_FP_SUPPORT
                if(gf_adm_release){
                 device_destroy(fingerprint_adm_class,0);
                 class_destroy(fingerprint_adm_class);
                 gf_adm_release = 0;
                 pr_info("goodixfp fingerprint_adm_device remove!");
                 }
                #endif

		pr_debug("disble_irq. irq = %d\n", gf_dev->irq);
		gf_disable_irq(gf_dev);
		/*power off the sensor */
		gf_dev->device_available = 0;
		free_irq(gf_dev->irq, gf_dev);
		//gpio_free(gf_dev->irq_gpio);
		//gpio_free(gf_dev->reset_gpio);
		gf_power_off(gf_dev);
		gf_cleanup(gf_dev);
	}
	mutex_unlock(&device_list_lock);
	return status;
}

static const struct file_operations gf_fops = {
	.owner = THIS_MODULE,
	/* REVISIT switch to aio primitives, so that userspace
	 * gets more complete API coverage.  It'll simplify things
	 * too, except for the locking.
	 */
	.unlocked_ioctl = gf_ioctl,
#ifdef CONFIG_COMPAT
	.compat_ioctl = gf_compat_ioctl,
#endif /*CONFIG_COMPAT */
	.open = gf_open,
	.release = gf_release,
};

#ifdef FB_CALLBACK_NOTITY
static int goodix_fb_state_chg_callback(struct notifier_block *nb,
					unsigned long val, void *data)
{
	struct gf_dev *gf_dev;
	struct fb_event *evdata = data;
	unsigned int blank;
	char temp[4] = { 0x0 };

	if (val != DRM_EVENT_BLANK)
		return 0;
	pr_debug
	    ("[info] %s go to the goodix_fb_state_chg_callback value = %d\n",
	     __func__, (int)val);
	gf_dev = container_of(nb, struct gf_dev, notifier);
	if (evdata && evdata->data && val == DRM_EVENT_BLANK && gf_dev) {
		blank = *(int *)(evdata->data);
		switch (blank) {
		case DRM_BLANK_POWERDOWN:
			if (gf_dev->device_available == 1) {
				gf_dev->fb_black = 1;
				gf_dev->wait_finger_down = true;
#if defined(GF_NETLINK_ENABLE)
				temp[0] = GF_NET_EVENT_FB_BLACK;
				sendnlmsg(temp);
#endif
			}
			break;
		case DRM_BLANK_UNBLANK:
			if (gf_dev->device_available == 1) {
				gf_dev->fb_black = 0;
#if defined(GF_NETLINK_ENABLE)
				temp[0] = GF_NET_EVENT_FB_UNBLACK;
				sendnlmsg(temp);
#endif
			}
			break;
		default:
			pr_debug("%s defalut\n", __func__);
			break;
		}
	}
	return NOTIFY_OK;
}

static struct notifier_block goodix_noti_block = {
	.notifier_call = goodix_fb_state_chg_callback,
};
#endif

static struct class *gf_class;
#if defined(USE_SPI_BUS)
static int gf_probe(struct spi_device *spi)
#elif defined(USE_PLATFORM_BUS)
static int gf_probe(struct platform_device *pdev)
#endif
{
	struct gf_dev *gf_dev = &gf;
	int status = -EINVAL;
	unsigned long minor;
	int i;
        gf_fp_adm = 1;
	/* Initialize the driver data */
	INIT_LIST_HEAD(&gf_dev->device_entry);
#if defined(USE_SPI_BUS)
	gf_dev->spi = spi;
#elif defined(USE_PLATFORM_BUS)
	gf_dev->spi = pdev;
#endif
	gf_dev->irq_gpio = -EINVAL;
	gf_dev->reset_gpio = -EINVAL;
	gf_dev->pwr_gpio = -EINVAL;
	gf_dev->device_available = 0;
	gf_dev->fb_black = 0;
	gf_dev->wait_finger_down = false;

	/* If we can allocate a minor number, hook up this device.
	 * Reusing minors is fine so long as udev or mdev is working.
	 */
	mutex_lock(&device_list_lock);
	minor = find_first_zero_bit(minors, N_SPI_MINORS);
	if (minor < N_SPI_MINORS) {
		struct device *dev;

		gf_dev->devt = MKDEV(SPIDEV_MAJOR, minor);
		dev = device_create(gf_class, &gf_dev->spi->dev, gf_dev->devt,
				    gf_dev, GF_DEV_NAME);
		status = IS_ERR(dev) ? PTR_ERR(dev) : 0;
	} else {
		dev_dbg(&gf_dev->spi->dev, "no minor number available!\n");
		status = -ENODEV;
		mutex_unlock(&device_list_lock);
		goto error_hw;
	}

	if (status == 0) {
		set_bit(minor, minors);
		list_add(&gf_dev->device_entry, &device_list);
	} else {
		gf_dev->devt = 0;
	}
	mutex_unlock(&device_list_lock);

	if (status == 0) {
		/*input device subsystem */
		gf_dev->input = input_allocate_device();
		if (gf_dev->input == NULL) {
			pr_err("%s, failed to allocate input device\n",
			       __func__);
			status = -ENOMEM;
			goto error_dev;
		}
		for (i = 0; i < ARRAY_SIZE(maps); i++)
			input_set_capability(gf_dev->input, maps[i].type,
					     maps[i].code);

		gf_dev->input->name = GF_INPUT_NAME;
		status = input_register_device(gf_dev->input);
		if (status) {
			pr_err("failed to register input device\n");
			goto error_input;
		}
	}

#ifdef FB_CALLBACK_NOTITY
	gf_dev->notifier = goodix_noti_block;
	drm_register_client(&gf_dev->notifier);
#endif

	wakeup_source_add(&fp_wakelock);
	pr_debug("version V%d.%d.%02d\n", VER_MAJOR, VER_MINOR, PATCH_LEVEL);

	return status;

error_input:
	if (gf_dev->input != NULL)
		input_free_device(gf_dev->input);
error_dev:
	if (gf_dev->devt != 0) {
		pr_debug("Err: status = %d\n", status);
		mutex_lock(&device_list_lock);
		list_del(&gf_dev->device_entry);
		device_destroy(gf_class, gf_dev->devt);
		clear_bit(MINOR(gf_dev->devt), minors);
		mutex_unlock(&device_list_lock);
	}
error_hw:
	gf_cleanup(gf_dev);
	gf_dev->device_available = 0;
	return status;
}

#if defined(USE_SPI_BUS)
static int gf_remove(struct spi_device *spi)
#elif defined(USE_PLATFORM_BUS)
static int gf_remove(struct platform_device *pdev)
#endif
{
	struct gf_dev *gf_dev = &gf;
	wakeup_source_remove(&fp_wakelock);
	/* make sure ops on existing fds can abort cleanly */
	if (gf_dev->irq)
		free_irq(gf_dev->irq, gf_dev);

	if (gf_dev->input != NULL)
		input_unregister_device(gf_dev->input);
	input_free_device(gf_dev->input);

	/* prevent new opens */
	mutex_lock(&device_list_lock);
	list_del(&gf_dev->device_entry);
	device_destroy(gf_class, gf_dev->devt);
	clear_bit(MINOR(gf_dev->devt), minors);
	if (NULL == proc_entry) {
		pr_info("goodixfp fp_info is null!");
	} else {
		remove_proc_entry(PROC_NAME,NULL);
		pr_info("goodixfp fp_info remove!");
	}
        #ifdef SAMSUNG_FP_SUPPORT
         if(gf_adm_release)
         {
            device_destroy(fingerprint_adm_class,0);
            class_destroy(fingerprint_adm_class);
            gf_adm_release = 0;
            pr_info("goodixfp fingerprint_adm_device remove!");
         }
        #endif

	if (gf_dev->users == 0)
		gf_cleanup(gf_dev);

#ifdef FB_CALLBACK_NOTITY
	drm_unregister_client(&gf_dev->notifier);
#endif

	mutex_unlock(&device_list_lock);

	return 0;
}

static struct of_device_id gx_match_table[] = {
	{.compatible = GF_SPIDEV_NAME},
	{},
};

#if defined(USE_SPI_BUS)
static struct spi_driver gf_driver = {
#elif defined(USE_PLATFORM_BUS)
static struct platform_driver gf_driver = {
#endif
	.driver = {
		   .name = GF_DEV_NAME,
		   .owner = THIS_MODULE,
		   .of_match_table = gx_match_table,
		   },
	.probe = gf_probe,
	.remove = gf_remove,
};

static int __init gf_init(void)
{
	int status;
	/* Claim our 256 reserved device numbers.  Then register a class
	 * that will key udev/mdev to add/remove /dev nodes.  Last, register
	 * the driver which manages those device numbers.
	 */
	BUILD_BUG_ON(N_SPI_MINORS > 256);
	status = register_chrdev(SPIDEV_MAJOR, CHRD_DRIVER_NAME, &gf_fops);
	if (status < 0) {
		pr_warn("Failed to register char device!\n");
		return status;
	}
	SPIDEV_MAJOR = status;
	gf_class = class_create(THIS_MODULE, CLASS_NAME);
	if (IS_ERR(gf_class)) {
		unregister_chrdev(SPIDEV_MAJOR, gf_driver.driver.name);
		pr_warn("Failed to create class.\n");
		return PTR_ERR(gf_class);
	}
#if defined(USE_PLATFORM_BUS)
	status = platform_driver_register(&gf_driver);
#elif defined(USE_SPI_BUS)
	status = spi_register_driver(&gf_driver);
#endif
	if (status < 0) {
		class_destroy(gf_class);
		unregister_chrdev(SPIDEV_MAJOR, gf_driver.driver.name);
		pr_warn("Failed to register SPI driver.\n");
	}
#ifdef GF_NETLINK_ENABLE
	netlink_init();
#endif
	pr_debug("goodix fingerprint fod init status = 0x%x\n", status);
	return 0;
}

module_init(gf_init);

static void __exit gf_exit(void)
{
#ifdef GF_NETLINK_ENABLE
	netlink_exit();
#endif
#if defined(USE_PLATFORM_BUS)
	platform_driver_unregister(&gf_driver);
#elif defined(USE_SPI_BUS)
	spi_unregister_driver(&gf_driver);
#endif
	class_destroy(gf_class);
	unregister_chrdev(SPIDEV_MAJOR, gf_driver.driver.name);
}

module_exit(gf_exit);
MODULE_SOFTDEP("pre: msm_drm");
MODULE_AUTHOR("Jiangtao Yi, <yijiangtao@goodix.com>");
MODULE_AUTHOR("Jandy Gou, <gouqingsong@goodix.com>");
MODULE_DESCRIPTION("goodix fingerprint sensor device driver");
MODULE_LICENSE("GPL v2");
