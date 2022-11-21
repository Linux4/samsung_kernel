/*
 * Samsung Mobile.
 *
 * kernel/power/rpm_request.c
 *
 * Drivers for sending rpm dump request message and PMIC register list.
 *
 * Copyright (C) 2013, Samsung Electronics.
 *
 * This program is free software. You can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation
 */

#include <linux/module.h>
#include <linux/proc_fs.h>
#include <linux/delay.h>
#include <linux/types.h>
#include <linux/timer.h>
#include <linux/fs.h>
#include <linux/kernel.h>
#include <soc/qcom/rpm-smd.h>
#include <linux/debugfs.h>
#include "register_list.h"

#define RPM_DUMP_REQ	0x77777777
#define DATA_SLEEP	1
#define DATA_FORCE	2
#define DATA_SEND	3
#define DATA_START	0x0
#define DATA_END	0xFFFF
#define DATA_KEY	0x12345678
#define SEND_KEY	0x111
#define RPM_DUMP_RESOURCE_ID	0


static int atoi(const char *str){
        int result = 0;
        int count = 0;
        if (str == NULL)
                return -EIO;
        while (str[count] != 0  /* NULL */
                && str[count] >= '0' && str[count] <= '9'){
                result = result * 10 + str[count] - '0';
                ++count;
        }
        return result;
}

static void send_data(u16 reg_address,u8 offset){
	int rc=0;
	struct msm_rpm_kvp kvp_active[] = {
                {
                        .key = reg_address,
                        .data = (void *)&offset,
                        .length = sizeof(offset),
                }
	};
	pr_info("[secpmic_dump][%s] data = %x\n",__func__,reg_address);

	rc = msm_rpm_send_message(MSM_RPM_CTX_ACTIVE_SET,
             RPM_DUMP_REQ, RPM_DUMP_RESOURCE_ID, kvp_active,
                                                        ARRAY_SIZE(kvp_active));
	if(rc < 0){
		pr_info("[secpmic_dump][%s] error = %d\n",__func__,rc);
	}
}

static void send_reg_list(void){
	int i;
	send_data(DATA_START,0x0); //Start Data
	for(i=0;i<ARRAY_0_SIZE;i++){
		send_data(base_address0[i],offset_count0[i]);
	}
	for(i=0;i<ARRAY_1_SIZE;i++){
		send_data(base_address1[i],offset_count1[i]);
	}
	send_data(DATA_END,0x0);
}

static void init_kvp(int data1){
	int rc=0;
	struct msm_rpm_kvp kvp_active[] = {
                {
                        .key = DATA_KEY,
                        .data = (void *)&data1,
                        .length = sizeof(data1),
                }
	};
	pr_info("[secpmic_dump][%s] data = %d\n",__func__,data1);

	rc = msm_rpm_send_message(MSM_RPM_CTX_ACTIVE_SET,
             RPM_DUMP_REQ, RPM_DUMP_RESOURCE_ID, kvp_active,
                                                        ARRAY_SIZE(kvp_active));
	if(rc < 0){
		pr_info("[secpmic_dump][%s] error = %d\n",__func__,rc);
	}
}


static ssize_t rpm_dump_request_write(
	struct file *filp, const char __user *ubuf, size_t cnt,
                    loff_t *ppos){
	int temp;

	temp = atoi(ubuf);
	pr_info("[secpmic_dump][%s] buffer = %s\n", __func__, ubuf);
	switch(temp){
		case DATA_SLEEP:
			init_kvp(DATA_SLEEP);
			break;
		case DATA_FORCE:
			mdelay(20000); //10sec Timer
			init_kvp(DATA_FORCE);
			break;
		case DATA_SEND:
			send_reg_list();
			break;
		default:
			pr_info("[secpmic_dump][%s] Invalid Input\n",__func__);
	}

	return cnt;
}

static const struct file_operations rpm_request_operations = {
	.write		= rpm_dump_request_write,
};

static int __init secpmic_dump_init(void){
	(void) debugfs_create_file("rpm_request", 0644,
                                NULL, NULL, &rpm_request_operations);
	pr_info("[secpmic_dump] %s has been initialized!!!\n",__func__);

	return 0;
}

module_init(secpmic_dump_init);

MODULE_AUTHOR("arun.am2@samsung.com");
MODULE_DESCRIPTION("SEC PMIC Dump Request Driver");
MODULE_LICENSE("GPL");
