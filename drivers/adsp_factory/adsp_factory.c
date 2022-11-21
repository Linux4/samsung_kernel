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

#define RAWDATA_TIMER_MS	2000
#define RAWDATA_TIMER_MARGIN_MS	20

/* The netlink socket */
struct adsp_data *data;
extern struct class *sec_class;

DEFINE_MUTEX(factory_mutex);

int sensors_register(struct device **dev, void *drvdata,
	struct device_attribute *attributes[], char *name);
void sensors_unregister(struct device *dev,
	struct device_attribute *attributes[]);

int adsp_get_sensor_data(int sensor_type)
{
	int val = 0;

	switch (sensor_type) {
	case ADSP_FACTORY_PROX:
		val = (int)data->sensor_data[ADSP_FACTORY_PROX].prox;
		break;
	default:
		break;
	}

	return val;
}

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
	case ADSP_FACTORY_ACCEL:
		dev_name = "accelerometer_sensor";
		break;
	case ADSP_FACTORY_GYRO:
		dev_name = "gyro_sensor";
		break;
	case ADSP_FACTORY_MAG:
		dev_name = "magnetic_sensor";
		break;
	case ADSP_FACTORY_PRESSURE:
		dev_name = "barometer_sensor";
		break;
	case ADSP_FACTORY_LIGHT:
		dev_name = "light_sensor";
		break;
	case ADSP_FACTORY_PROX:
		dev_name = "proximity_sensor";
		break;
	case ADSP_FACTORY_RGB:
		dev_name = "light_sensor";
		break;
	case ADSP_FACTORY_SSC_CORE:
		dev_name = "ssc_core";
		break;
#ifdef CONFIG_SUPPORT_HIDDEN_HOLE
	case ADSP_FACTORY_HH_HOLE:
		dev_name = "hidden_hole";
		break;
#endif
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

#if 0
int adsp_mobeam_register(struct device_attribute *attributes[])
{
	u8 i;

	data->mobeam_device = device_create(sec_class, NULL, 0,
	data, "sec_barcode_emul");

	for (i = 0; attributes[i] != NULL; i++)	{
		if (device_create_file(data->mobeam_device, attributes[i]) < 0)
			pr_err("%s fail to create %d", __func__, i);
	}

	return 0;
}

int adsp_mobeam_unregister(struct device_attribute *attributes[])
{
	u8 i;

	for (i = 0; attributes[i] != NULL; i++)
		device_remove_file(data->mobeam_device, attributes[i]);

	return 0;
}
#endif

void adsp_factory_start_timer(int sensor_type, const unsigned int ms)
{
	mod_timer(&data->command_timer[sensor_type],
		jiffies + msecs_to_jiffies(ms));
}

bool adsp_start_raw_data(int sensor_type)
{
	struct msg_data message;
	unsigned long timeout = jiffies + RAWDATA_TIMER_MS;

	if (!(data->raw_data_stream & (1 << sensor_type))) {
		pr_info("[FACTORY] %s: sensor_type:%d Start!!!\n",
			__func__, sensor_type);
		mutex_lock(&data->raw_stream_lock[sensor_type]);
		data->raw_data_stream |= (1 << sensor_type);
		message.sensor_type = sensor_type;
		adsp_unicast(&message, sizeof(message),
			NETLINK_MESSAGE_GET_RAW_DATA, 0, 0);
		mutex_unlock(&data->raw_stream_lock[sensor_type]);
	}

	while (!(data->data_ready_flag & 1 << sensor_type)) {
		msleep(RAWDATA_TIMER_MARGIN_MS);
		if (time_after(jiffies, timeout)) {
			adsp_stop_raw_data(sensor_type);
			pr_info("[FACTORY] %s: sensor_type:%d Timeout!!!\n",
				__func__, sensor_type);
			return false;
		}
	}

	data->data_ready_flag &= ~(1 << sensor_type);
	adsp_factory_start_timer(sensor_type, RAWDATA_TIMER_MS);
	return true;
}

void adsp_stop_raw_data(int sensor_type)
{
	struct msg_data message;

	if ((data->raw_data_stream & (1 << sensor_type))) {
		pr_info("[FACTORY] %s: sensor_type:%d Stop!!!\n",
			__func__, sensor_type);
		mutex_lock(&data->raw_stream_lock[sensor_type]);
		data->raw_data_stream &= ~(1 << sensor_type);
		data->data_ready_flag &= ~(1 << sensor_type);
		message.sensor_type = sensor_type;
		adsp_unicast(&message, sizeof(message),
			NETLINK_MESSAGE_STOP_RAW_DATA, 0, 0);
		mutex_unlock(&data->raw_stream_lock[sensor_type]);
	}
}

void stop_raw_data_worker(struct work_struct *work)
{
	int i;

	pr_info("[FACTORY] %s: flag=%x\n", __func__, data->stop_raw_data_flag);
	for (i = 0; i < ADSP_FACTORY_SENSOR_MAX; i++) {
		if (data->stop_raw_data_flag & (1 << i)) {
			data->stop_raw_data_flag &= ~(1 << i);
			adsp_stop_raw_data(i);
		}
	}
}

static void factory_adsp_command_timer(unsigned long value)
{
	pr_info("[FACTORY] %s: value=%ld\n", __func__, value);
	data->stop_raw_data_flag |= (1 << value);
	schedule_work(&data->timer_stop_data_work);
}

#ifdef CONFIG_SUPPORT_HIDDEN_HOLE
extern void hidden_hole_data_read(struct adsp_data *data);
#endif
static int process_received_msg(struct sk_buff *skb, struct nlmsghdr *nlh)
{
	switch (nlh->nlmsg_type) {
	case NETLINK_MESSAGE_GET_STATUS:
	{
		struct sensor_status *pdata =
			(struct sensor_status *)NLMSG_DATA(nlh);
		pr_info("[FACTORY] %s: NETLINK_MESSAGE_GET_STATUS type=%d status:%d\n",
			__func__, pdata->sensor_type, pdata->status);
		if (pdata->status != 0)
			adsp_factory_unregister(pdata->sensor_type);
		break;
	}
	case NETLINK_MESSAGE_RAW_DATA_RCVD:
	{
		struct sensor_value *pdata =
			(struct sensor_value *)NLMSG_DATA(nlh);
		data->sensor_data[pdata->sensor_type] = *pdata;
		if (data->raw_data_stream & 1 << pdata->sensor_type)
			data->data_ready_flag |= 1 << pdata->sensor_type;
		break;
	}
	case NETLINK_MESSAGE_CALIB_DATA_RCVD:
	{
		struct sensor_calib_value *pdata =
			(struct sensor_calib_value *)NLMSG_DATA(nlh);
		pr_info("[FACTORY] %s: NETLINK_MESSAGE_CALIB_DATA_RCVD type=%d, x=%d, y=%d, z=%d\n",
			__func__, pdata->sensor_type,
			pdata->x, pdata->y, pdata->z);
#ifdef CONFIG_SUPPORT_PROX_AUTO_CAL
		if (pdata->sensor_type == ADSP_FACTORY_PROX)
			pr_info("[FACTORY] %s: DH=%d, DL=%d\n", __func__,
				pdata->threDetectLo, pdata->threDetectHi);
#endif
		data->sensor_calib_data[pdata->sensor_type] = *pdata;
		data->calib_ready_flag |= 1 << pdata->sensor_type;
		break;
	}
	case NETLINK_MESSAGE_CALIB_STORE_RCVD:
	{
		struct sensor_calib_store_result *pdata =
			(struct sensor_calib_store_result *)NLMSG_DATA(nlh);
		pr_info("[FACTORY] %s: NETLINK_MESSAGE_CALIB_STORE_RCVD type=%d,nCalibstoreresult=%d\n",
			__func__, pdata->sensor_type, pdata->result);
		data->sensor_calib_result[pdata->sensor_type] = *pdata;
			data->calib_store_ready_flag |= 1 << pdata->sensor_type;
		break;
	}
	case NETLINK_MESSAGE_SELFTEST_SHOW_RCVD:
	{
		struct sensor_selftest_show_result *pdata =
			(struct sensor_selftest_show_result *)NLMSG_DATA(nlh);
		pr_info("[FACTORY] %s: NETLINK_MESSAGE_SELFTEST_SHOW_RCVD type=%d, SelftestResult1=%d, SelftestResult2=%d\n",
			__func__, pdata->sensor_type,
			pdata->result1, pdata->result2);
		data->sensor_selftest_result[pdata->sensor_type] = *pdata;
		data->selftest_ready_flag |= 1 << pdata->sensor_type;
		break;
	}
	case NETLINK_MESSAGE_GYRO_SELFTEST_SHOW_RCVD:
	{
		struct sensor_gyro_st_value *pdata =
			(struct sensor_gyro_st_value *)NLMSG_DATA(nlh);
		pr_info("[FACTORY] %s: NETLINK_MESSAGE_GYRO_SELFTEST_SHOW_RCVD type=%d, SelftestResult1=%d, SelftestResult2=%d\n",
			__func__, pdata->sensor_type,
			pdata->result1, pdata->result2);
		data->gyro_st_result = *pdata;
		data->selftest_ready_flag |= 1 << pdata->sensor_type;
		break;
	}
	case NETLINK_MESSAGE_GYRO_TEMP:
	{
		struct sensor_gyro_st_value *pdata =
			(struct sensor_gyro_st_value *)NLMSG_DATA(nlh);
		pr_info("[FACTORY] %s: NETLINK_MESSAGE_GYRO_TEMP_RCVD type = %d, gyro_temp = %d\n",
			__func__, pdata->sensor_type, pdata->result1);
		data->gyro_st_result = *pdata;
		data->selftest_ready_flag |= 1 << pdata->sensor_type;
		break;
	}
	case NETLINK_MESSAGE_ACCEL_LPF_ON:
	case NETLINK_MESSAGE_ACCEL_LPF_OFF:
	{
		struct sensor_accel_lpf_value *pdata =
			(struct sensor_accel_lpf_value *)NLMSG_DATA(nlh);
		pr_info("[FACTORY] %s: NETLINK_MESSAGE_ACCEL_LPF_ON_OFF type=%d, result=%d, lpf_on_off=%d\n",
			__func__, pdata->sensor_type,
			pdata->result, pdata->lpf_on_off);
		data->accel_lpf_result = *pdata;
		data->selftest_ready_flag |= 1 << pdata->sensor_type;
		break;
	}
	case NETLINK_MESSAGE_DUMP_REGISTER_RCVD:
	{
		struct dump_register *pdata =
			(struct dump_register *)NLMSG_DATA(nlh);

		pr_info("[FACTORY] %s: DUMP_REGS (%d)\n",
			__func__, pdata->sensor_type);

		data->dump_registers[pdata->sensor_type] = *pdata;
		data->dump_reg_ready_flag |= 1 << pdata->sensor_type;
		break;
	}
	case NETLINK_MESSAGE_STOP_RAW_DATA:
	{
		struct sensor_stop_value *pdata =
			(struct sensor_stop_value *)NLMSG_DATA(nlh);
		pr_info("[FACTORY] %s: NETLINK_MESSAGE_STOP_RAW_DATA type=%d, StopResult=%d\n",
			__func__, pdata->sensor_type, pdata->result);
		break;
	}
	case NETLINK_MESSAGE_MAG_READ_FUSE_ROM:
	{
		struct sensor_mag_factory_value *pdata =
			(struct sensor_mag_factory_value *)NLMSG_DATA(nlh);
		pr_info("[FACTORY] %s: NETLINK_MESSAGE_MAG_READ_FUSE_ROM type=%d, mag_fuseX=%d, mag_fuseY=%d, mag_fuseZ=%d\n",
			__func__, pdata->sensor_type,
			pdata->fuserom_x, pdata->fuserom_y, pdata->fuserom_z);
		data->sensor_mag_factory_result = *pdata;
		data->magtest_ready_flag |= 1 << pdata->sensor_type;
		break;
	}
	case NETLINK_MESSAGE_MAG_READ_REGISTERS:
	{
		struct sensor_mag_factory_value *pdata =
			(struct sensor_mag_factory_value *)NLMSG_DATA(nlh);
		pr_info("[FACTORY] %s: NETLINK_MESSAGE_MAG_READ_REGISTERS type=%d, reg_0=%d, reg_1=%d, reg_2=%d\n",
			__func__, pdata->sensor_type, pdata->registers[0],
			pdata->registers[1], pdata->registers[2]);
		data->sensor_mag_factory_result = *pdata;
		data->magtest_ready_flag |= 1 << pdata->sensor_type;
		break;
	}
	case NETLINK_MESSAGE_DUMPSTATE:
	{
		struct sensor_status *pdata =
			(struct sensor_status *)NLMSG_DATA(nlh);
		pr_info("[FACTORY] %s: NETLINK_MESSAGE_DUMPSTATE type=%d status:%d\n",
			__func__, pdata->sensor_type, pdata->status);
		data->dump_status = pdata->status;
		data->dump_ready_flag |= 1 << pdata->sensor_type;
		break;
	}
	case NETLINK_MESSAGE_THD_HI_LO_DATA_RCVD:
	{
		struct prox_th_value *pdata =
			(struct prox_th_value *)NLMSG_DATA(nlh);
		pr_info("[FACTORY] %s: NETLINK_MESSAGE_THD_HI_LO_DATA_RCVD %d:%d\n",
			__func__, pdata->th_high, pdata->th_low);
		data->pth = *pdata;
		data->th_read_flag |= 1 << PROX_THRESHOLD;
		break;
	}
#ifdef CONFIG_SUPPORT_PROX_AUTO_CAL
	case NETLINK_MESSAGE_HD_THD_HI_LO_DATA_RCVD:
	{
		struct prox_th_value *pdata =
			(struct prox_th_value *)NLMSG_DATA(nlh);
		pr_info("[FACTORY] %s: NETLINK_MESSAGE_HD_THD_HI_LO_DATA_RCVD %d:%d\n",
			__func__, pdata->hd_th_high, pdata->hd_th_low);
		data->pth = *pdata;
		data->th_read_flag |= 1 << PROX_HD_THRESHOLD;
		break;
	}
#endif
#ifdef CONFIG_SUPPORT_HIDDEN_HOLE
	case NETLINK_MESSAGE_HIDDEN_HOLE_READ_DATA:
	{
		struct sensor_status *pdata =
			(struct sensor_status *)NLMSG_DATA(nlh);
		pr_info("[FACTORY] %s: NETLINK_MESSAGE_HIDDEN_HOLE_READ_DATA type=%d\n",
			__func__, pdata->sensor_type);
		hidden_hole_data_read(data);
		break;
	}
#endif
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

	data->data_ready_flag = 0;
	data->calib_ready_flag = 0;
	data->calib_store_ready_flag = 0;
	data->selftest_ready_flag = 0;
	data->magtest_ready_flag = 0;
	data->stop_raw_data_flag = 0;

	INIT_WORK(&data->timer_stop_data_work, stop_raw_data_worker);
	for (i = 0; i < ADSP_FACTORY_SENSOR_MAX; i++) {
		setup_timer(&data->command_timer[i],
			factory_adsp_command_timer, (unsigned long)i);
		add_timer(&data->command_timer[i]);
		mutex_init(&data->raw_stream_lock[i]);
		data->sysfs_created[i] = false;
	}
	mutex_init(&data->remove_sysfs_mutex);

	pr_info("[FACTORY] %s: Timer Init\n", __func__);
	return 0;
}

static void __exit factory_adsp_exit(void)
{
	int i;

	for (i = 0; i < ADSP_FACTORY_SENSOR_MAX; i++) {
		del_timer(&data->command_timer[i]);
		mutex_destroy(&data->raw_stream_lock[i]);
	}
	mutex_destroy(&data->remove_sysfs_mutex);
	pr_info("[FACTORY] %s\n", __func__);
}

module_init(factory_adsp_init);
module_exit(factory_adsp_exit);
MODULE_DESCRIPTION("Support for factory test sensors (adsp)");
MODULE_LICENSE("GPL");
