/*
*  Copyright (C) 2012, Samsung Electronics Co. Ltd. All Rights Reserved.
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
#include <linux/init.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <net/sock.h>
#include <net/netlink.h>
#include <linux/skbuff.h>
#include <linux/netlink.h>
#include <linux/mutex.h>
#include <linux/device.h>
#include <linux/err.h>
#include "adsp.h"

/* The netlink socket */
struct adsp_data *data;
extern struct class *sec_class;

DEFINE_MUTEX(factory_mutex);

int sensors_register(struct device **dev, void *drvdata,
	struct device_attribute *attributes[], char *name);
void sensors_unregister(struct device *dev,
	struct device_attribute *attributes[]);

/* Function used to send message to the user space */
int adsp_unicast(void *param, int param_size, int type, u32 portid, int flags)
{
	struct sk_buff *skb;
	struct nlmsghdr *nlh;
	void *msg;
	int ret = -1;

	pr_info("[FACTORY] %s type:%d, param_size:%d\n", __func__,
		type, param_size);
	skb = nlmsg_new(param_size, GFP_KERNEL);
	if (!skb) {
		pr_err("[FACTORY] %s - nlmsg_new fail\n", __func__);
		return -ENOMEM;
	}
	nlh = nlmsg_put(skb, portid, 0, type, param_size, flags);

	if (nlh == NULL) {
		nlmsg_free(skb);
		return -EMSGSIZE;
	}
	msg = nlmsg_data(nlh);
	memcpy(msg, param, param_size);
	NETLINK_CB(skb).dst_group = 0;
	ret = nlmsg_unicast(data->adsp_skt, skb, PID);
	if (ret != 0)
		pr_err("[FACTORY] %s - ret = %d\n", __func__, ret);
	return ret;
}

int adsp_factory_register(unsigned int type,
	struct device_attribute *attributes[])
{
	int ret = 0;
	char *dev_name;

	switch (type) {
	case ADSP_FACTORY_GRIP:
		dev_name = "grip_sensor";
		break;
	default:
		dev_name = "unknown_sensor";
		break;
	}

	data->sensor_attr[type] = attributes;
	ret = sensors_register(&data->sensor_device[type], data,
		data->sensor_attr[type], dev_name);

	data->sysfs_created[type] = true;
	pr_info("[FACTORY] %s - type:%u ptr:%p\n",
		__func__, type, data->sensor_device[type]);

	return ret;
}

int adsp_factory_unregister(unsigned int type)
{
	pr_info("[FACTORY] %s - type:%u ptr:%p\n",
		__func__, type, data->sensor_device[type]);

	if (data->sysfs_created[type]) {
		sensors_unregister(data->sensor_device[type],
			data->sensor_attr[type]);
		data->sysfs_created[type] = false;
	} else {
		pr_info("[FACTORY] %s: skip type %u\n", __func__, type);
	}
	return 0;
}

static int process_received_msg(struct sk_buff *skb, struct nlmsghdr *nlh)
{
	switch (nlh->nlmsg_type) {
	case NETLINK_MESSAGE_GRIP_ON:
	case NETLINK_MESSAGE_GRIP_OFF:
	{
		struct sensor_onoff *pdata =
			(struct sensor_onoff *)NLMSG_DATA(nlh);
		pr_info("[FACTORY] %s: NETLINK_MESSAGE_GRIP_ON_OFF on_off=%d\n",
			__func__, pdata->onoff);
		break;
	}
	default:
		break;
	}
	return 0;
}

static void factory_receive_skb(struct sk_buff *skb)
{
	struct nlmsghdr *nlh;
	int len;
	int err;

	nlh = (struct nlmsghdr *)skb->data;
	len = skb->len;
	while (NLMSG_OK(nlh, len)) {
		err = process_received_msg(skb, nlh);
		/* if err or if this message says it wants a response */
		if (err || (nlh->nlmsg_flags & NLM_F_ACK))
			netlink_ack(skb, nlh, err);
		nlh = NLMSG_NEXT(nlh, len);
	}
}

/* Receive messages from netlink socket. */
static void factory_test_result_receive(struct sk_buff *skb)
{
	mutex_lock(&factory_mutex);
	factory_receive_skb(skb);
	mutex_unlock(&factory_mutex);
}

struct netlink_kernel_cfg netlink_cfg = {
	.input = factory_test_result_receive,
};

static int __init factory_adsp_init(void)
{
	int i;

	pr_info("[FACTORY] %s\n", __func__);
	data = kzalloc(sizeof(*data), GFP_KERNEL);
	data->adsp_skt = netlink_kernel_create(&init_net,
		NETLINK_ADSP_FAC, &netlink_cfg);

	for (i = 0; i < ADSP_FACTORY_SENSOR_MAX; i++) {
		data->sysfs_created[i] = false;
	}
	mutex_init(&data->remove_sysfs_mutex);

	pr_info("[FACTORY] %s: Timer Init\n", __func__);
	return 0;
}

static void __exit factory_adsp_exit(void)
{
	mutex_destroy(&data->remove_sysfs_mutex);
	pr_info("[FACTORY] %s\n", __func__);
}

module_init(factory_adsp_init);
module_exit(factory_adsp_exit);
MODULE_DESCRIPTION("Support for factory test sensors (adsp)");
MODULE_LICENSE("GPL");
