/* Copyright (c) 2015-2016, 2018, The Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 * 
 * Author		: Vijeth (vijeth.po@pathpartnertech.com)
 * Revised Date	: 07-06-19
 * Description	: TI Smartamp algorithm control interface
 *
 */

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/version.h>
#include <linux/device.h>
#include <linux/slab.h>
#include <linux/debugfs.h>
#include <linux/kthread.h>
#include <linux/uaccess.h>
#include <linux/wait.h>
#include <linux/jiffies.h> 
#include <linux/sched.h>
#include <linux/delay.h>
#include <linux/syscalls.h>
#include <linux/fs.h>
#include <dsp/q6afe-v2.h>

#include <dsp/smart_amp.h>

static int spk_count = 1;
uint32_t port = 0x0;
/*Mutex to serialize DSP read/write commands*/
static struct mutex routing_lock;

int afe_smartamp_get_set(u8 *user_data, uint32_t param_id,
	uint8_t get_set, uint32_t length, uint32_t module_id)
{
	int ret = 0;
	struct afe_smartamp_get_calib calib_resp;

	switch (get_set) {
		case TAS_SET_PARAM:
			ret = afe_smartamp_set_calib_data(param_id,
				(struct afe_smartamp_set_params_t *)user_data, length, module_id, port);
		break;
		case TAS_GET_PARAM:
			memset(&calib_resp, 0, sizeof(calib_resp));
			ret = afe_smartamp_get_calib_data(&calib_resp,
				param_id, module_id, port);
			memcpy(user_data, calib_resp.res_cfg.payload, length);
		break;
		default:
			goto fail_cmd;
	}
fail_cmd:
	return ret;
}

/*Wrapper around set/get parameter, all set/get commands pass through this wrapper*/
int afe_smartamp_algo_ctrl(u8 *user_data, uint32_t param_id,
	uint8_t get_set, uint32_t length, uint32_t module_id)
{
	int ret = 0;
	mutex_lock(&routing_lock);
	ret = afe_smartamp_get_set(user_data, param_id, get_set, length, module_id);
	mutex_unlock(&routing_lock);
	return ret;
}

void smartamp_add_algo(uint8_t channels, int port_id)
{
	pr_info("[TI-SmartPA:%s] Adding Smartamp algo functions, spk_count=%d, port_id=0x%x",
				__func__, channels, port_id);
	
	mutex_init(&routing_lock);
	port = (uint32_t)port_id;
	spk_count = channels;
}

void smartamp_remove_algo(void)
{
	pr_info("[TI-SmartPA:%s] Removing Smartamp Algorithm functions", __func__);
	
	mutex_destroy(&routing_lock);
}

MODULE_AUTHOR("Texas Instruments Inc.");
MODULE_DESCRIPTION("TAS25XX Algorithm");
MODULE_LICENSE("GPL v2");
