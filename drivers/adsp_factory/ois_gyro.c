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
#include "adsp.h"
#define CHIP_ID "CAM_OIS"
#define CMD_DATA_SIZE 32
#define TIMEOUT_CNT_FOR_OIS 200
#define READ_RETRY_CNT 3
//static int cam_ois_cmd_data_size = 0;

static int32_t cam_ois_cmd_data32[CMD_DATA_SIZE];


/*
extern int cam_ois_cmd_notifier_register(struct notifier_block *nb);
extern int cam_ois_cmd_notifier_unregister(struct notifier_block *nb);
extern int cam_ois_reg_read_notifier_register(struct notifier_block *nb);
extern int cam_ois_reg_read_notifier_unregister(struct notifier_block *nb);
extern int cam_ois_factory_mode_notifier_register(struct notifier_block *nb);
extern int cam_ois_factory_mode_notifier_unregister(struct notifier_block *nb);
*/
static int (*cam_ois_cmd_notifier_register)(struct notifier_block *nb) = NULL;
static int (*cam_ois_cmd_notifier_unregister)(struct notifier_block *nb) = NULL;
static int (*cam_ois_reg_read_notifier_register)(struct notifier_block *nb) = NULL;
static int (*cam_ois_reg_read_notifier_unregister)(struct notifier_block *nb) = NULL;
static int (*cam_ois_factory_mode_notifier_register)(struct notifier_block *nb) = NULL;
static int (*cam_ois_factory_mode_notifier_unregister)(struct notifier_block *nb) = NULL;

void register_vois_noti(int (*fac_noti_reg)(struct notifier_block *nb), int (*fac_noti_unreg)(struct notifier_block *nb),
							int (*read_noti_reg)(struct notifier_block *nb),int (*read_noti_unreg)(struct notifier_block *nb),
							int (*cmd_noti_reg)(struct notifier_block *nb),int (*cmd_noti_unreg)(struct notifier_block *nb))
{
	cam_ois_factory_mode_notifier_register = fac_noti_reg;
	cam_ois_factory_mode_notifier_unregister = fac_noti_unreg;
	cam_ois_reg_read_notifier_register = read_noti_reg;
	cam_ois_reg_read_notifier_unregister = read_noti_unreg;
	cam_ois_cmd_notifier_register = cmd_noti_reg;
	cam_ois_cmd_notifier_unregister = cmd_noti_unreg;
	pr_info("[OIS_GYRO_FACTORY] %s: complete\n", __func__);
}
EXPORT_SYMBOL(register_vois_noti);

void send_ois_cmd_data(struct adsp_data *data)
{
    int cnt = 0;
	int retry_cnt = 0;
	int ret[3] = {-1,-1,-1};
	int size = cam_ois_cmd_data32[0]*4 + 16;
    do{
		adsp_unicast(cam_ois_cmd_data32, size, MSG_OIS_GYRO, 0, MSG_TYPE_SET_REGISTER);		
		//pr_info("[OIS_GYRO_FACTORY] %s: adsp_unicast complete\n", __func__);

		while (!(data->ready_flag[MSG_TYPE_SET_REGISTER] & 1 << MSG_OIS_GYRO) && cnt++ < TIMEOUT_CNT)
			usleep_range(500,500);

		//pr_info("[OIS_GYRO_FACTORY] %s: Ready Flag = %u First\n", __func__, data->ready_flag[MSG_TYPE_SET_REGISTER]);

		data->ready_flag[MSG_TYPE_SET_REGISTER] &= ~(1 << MSG_OIS_GYRO);
	
		//pr_info("[OIS_GYRO_FACTORY] %s: Ready Flag = %u Second\n", __func__, data->ready_flag[MSG_TYPE_SET_REGISTER]);

		if (cnt >= TIMEOUT_CNT)
			pr_err("[OIS_GYRO_FACTORY] %s: Timeout!!!\n", __func__);
		retry_cnt++;	    
	}while(cnt >= TIMEOUT_CNT_FOR_OIS && retry_cnt < READ_RETRY_CNT);
	if (retry_cnt >= READ_RETRY_CNT)
		pr_err("[OIS_GYRO_FACTORY] %s: Write Fail!!!\n", __func__);
	else
	{
       ret[0] = (int32_t)data->msg_buf[MSG_OIS_GYRO][0];
       ret[1] = (int32_t)data->msg_buf[MSG_OIS_GYRO][1];
       ret[2] = (int32_t)data->msg_buf[MSG_OIS_GYRO][2];
	   pr_info("[OIS_GYRO_FACTORY] %s: msg_buf Size = %d, Addr = 0x%x, Val = %d  Count = %d\n", __func__, ret[0], ret[1], ret[2],cnt);
	}
}

static int cam_ois_cmd_notifier(struct notifier_block *nb,
				unsigned long action, void *cmd_data)
{
	int i = 0;
	uint32_t *cmd_data_buf_p;
	struct adsp_data *data = container_of(nb, struct adsp_data, cam_ois_cmd_nb);

	cmd_data_buf_p = cmd_data;

    if (cmd_data_buf_p[0] > CMD_DATA_SIZE - 4)
	{
		pr_info("[OIS_GYRO_FACTORY] cmd data size is greater than 28 current_size = %d\n",cmd_data_buf_p[0]);
		cmd_data_buf_p[0] = CMD_DATA_SIZE - 4;
	}

	for (i = 0; i < cmd_data_buf_p[0] + 4; i++) {
		cam_ois_cmd_data32[i] = cmd_data_buf_p[i];
        //pr_info("[OIS_GYRO_FACTORY] %s: [%d]:[%d]\n", __func__, i, cam_ois_cmd_data32[i]);
	}
	
	send_ois_cmd_data(data);

	return 0;
}

void read_ois_reg_data(struct adsp_data *data,int32_t *reg_data)
{
    int cnt = 0;
	int retry_cnt = 0;
    int32_t msg_buf[3];	
    msg_buf[0] = reg_data[0];
    msg_buf[1] = reg_data[1];
    msg_buf[2] = reg_data[2];
	do{
		adsp_unicast(msg_buf, sizeof(msg_buf), MSG_OIS_GYRO, 0, MSG_TYPE_GET_REGISTER);
		//pr_info("[OIS_GYRO_FACTORY] %s: adsp_unicast complete\n", __func__);

		
		cnt = 0;
		while (!(data->ready_flag[MSG_TYPE_GET_REGISTER] & 1 << MSG_OIS_GYRO) &&
			cnt++ < TIMEOUT_CNT_FOR_OIS) // 200ms max
			usleep_range(500, 550);

		data->ready_flag[MSG_TYPE_GET_REGISTER] &= ~(1 << MSG_OIS_GYRO);

		if (cnt >= TIMEOUT_CNT_FOR_OIS)
			pr_err("[OIS_GYRO_FACTORY] %s: Timeout!!!\n", __func__);
		retry_cnt++;		
	}while(cnt >= TIMEOUT_CNT_FOR_OIS && retry_cnt < READ_RETRY_CNT);
	
	if (retry_cnt >= READ_RETRY_CNT)
		pr_err("[OIS_GYRO_FACTORY] %s: Read Fail!!!\n", __func__);
	else {	
	    pr_info("[OIS_GYRO_FACTORY] %s: Reg:[0x%x], Val:[%d] Count:[%d]\n", __func__, reg_data[0], reg_data[3], cnt);
	}
	reg_data[3] = (int32_t)data->msg_buf[MSG_OIS_GYRO][0];
}
void set_factory_mode(struct adsp_data *data,int32_t *reg_data)
{
	int cnt = 0;
	int retry_cnt = 0;
    int32_t msg_buf[4];	
    msg_buf[0] = reg_data[0];
    msg_buf[1] = reg_data[1];
    msg_buf[2] = reg_data[2];
	msg_buf[3] = reg_data[3];
	pr_info("[OIS_GYRO_FACTORY] %s: msg_buf = [%d]\n", __func__, msg_buf[0]);

	do{
		adsp_unicast(msg_buf, sizeof(msg_buf), MSG_OIS_GYRO, 0, MSG_TYPE_SET_THRESHOLD);
		//pr_info("[OIS_GYRO_FACTORY] %s: adsp_unicast complete\n", __func__);

		
		cnt = 0;
		while (!(data->ready_flag[MSG_TYPE_SET_THRESHOLD] & 1 << MSG_OIS_GYRO) &&
			cnt++ < TIMEOUT_CNT_FOR_OIS) // 200ms max
			usleep_range(500, 550);

		data->ready_flag[MSG_TYPE_SET_THRESHOLD] &= ~(1 << MSG_OIS_GYRO);

		if (cnt >= TIMEOUT_CNT_FOR_OIS)
			pr_err("[OIS_GYRO_FACTORY] %s: Timeout!!!\n", __func__);
		retry_cnt++;		
	}while(cnt >= TIMEOUT_CNT_FOR_OIS && retry_cnt < READ_RETRY_CNT);
	
	if (retry_cnt >= READ_RETRY_CNT)
		pr_err("[OIS_GYRO_FACTORY] %s: factory_mode Fail!!!\n", __func__);
	else
	{
		pr_info("[OIS_GYRO_FACTORY] %s: factory_mode complete\n", __func__);		
	}	
	
}

static int cam_ois_reg_read_notifier(struct notifier_block *nb,
				unsigned long action, void *cmd_data)
{
	int32_t *reg_read_data_buf_p;
    struct adsp_data *data = container_of(nb, struct adsp_data, cam_ois_reg_read_nb);
	reg_read_data_buf_p = cmd_data;	
	read_ois_reg_data(data,reg_read_data_buf_p);
	return 0;
}

static int cam_ois_factory_mode_notifier(struct notifier_block *nb,
				unsigned long action, void *cmd_data)
{
	int32_t *reg_read_data_buf_p;
    struct adsp_data *data = container_of(nb, struct adsp_data, cam_ois_factory_mode_nb);
	reg_read_data_buf_p = cmd_data;	
	set_factory_mode(data,reg_read_data_buf_p);
	return 0;
}

void cam_ois_cmd_init_work(struct adsp_data *data)
{
	pr_info("[OIS_GYRO_FACTORY] : register cam_ois_cmd_init_work notifier\n");
	data->cam_ois_cmd_nb.priority = 1;
	data->cam_ois_cmd_nb.notifier_call = cam_ois_cmd_notifier;
	if(cam_ois_cmd_notifier_register != NULL)
		cam_ois_cmd_notifier_register(&data->cam_ois_cmd_nb);

	pr_info("[OIS_GYRO_FACTORY] %s: Done!\n", __func__);
}

void cam_ois_reg_read_init_work(struct adsp_data *data)
{
	pr_info("[OIS_GYRO_FACTORY] : register cam_ois_reg_read_init_work notifier\n");
	data->cam_ois_reg_read_nb.priority = 1;
	data->cam_ois_reg_read_nb.notifier_call = cam_ois_reg_read_notifier;
	if(cam_ois_reg_read_notifier_register != NULL)
		cam_ois_reg_read_notifier_register(&data->cam_ois_reg_read_nb);

	pr_info("[OIS_GYRO_FACTORY] %s: Done!\n", __func__);
}

void cam_ois_factory_mode_init_work(struct adsp_data *data)
{
	pr_info("[OIS_GYRO_FACTORY] : register cam_ois_factory_mode_init_work notifier\n");
	data->cam_ois_factory_mode_nb.priority = 1;
	data->cam_ois_factory_mode_nb.notifier_call = cam_ois_factory_mode_notifier;
	if(cam_ois_factory_mode_notifier_register != NULL)
	cam_ois_factory_mode_notifier_register(&data->cam_ois_factory_mode_nb);

	pr_info("[OIS_GYRO_FACTORY] %s: Done!\n", __func__);
}
static ssize_t ois_gyro_name_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	return snprintf(buf, PAGE_SIZE, "%s\n", CHIP_ID);
}

static DEVICE_ATTR(name, 0444, ois_gyro_name_show, NULL);


static struct device_attribute *ois_gyro_attrs[] = {
	&dev_attr_name,
	NULL,
};

static int __init ois_gyro_factory_init(void)
{
	adsp_factory_register(MSG_OIS_GYRO, ois_gyro_attrs);

	pr_info("[OIS_GYRO_FACTORY] %s\n", __func__);

	return 0;
}

static void __exit ois_gyro_factory_exit(void)
{
	adsp_factory_unregister(MSG_OIS_GYRO);

	pr_info("[OIS_GYRO_FACTORY] %s\n", __func__);
}
module_init(ois_gyro_factory_init);
module_exit(ois_gyro_factory_exit);
