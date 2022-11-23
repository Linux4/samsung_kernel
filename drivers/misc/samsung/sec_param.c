/*
 * drivers/misc/samsung/sec_param.c
 *
 * COPYRIGHT(C) 2011-2016 Samsung Electronics Co., Ltd. All Right Reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 */

#define pr_fmt(fmt)     KBUILD_MODNAME ":%s() " fmt, __func__

#include <linux/module.h>
#include <linux/device.h>
#include <linux/slab.h>
#include <linux/workqueue.h>
#include <linux/fs.h>
#include <linux/uaccess.h>
#include <linux/sec_param.h>
#include <linux/file.h>
#include <linux/syscalls.h>
#include <linux/delay.h>
#include <linux/sec_debug.h>
#include <linux/muic/muic.h>
#ifdef CONFIG_SEC_QUEST
#include <linux/sec_quest.h>
#endif

#define PARAM_RD		0
#define PARAM_WR		1

#define MAX_PARAM_BUFFER	128
#define SAPA_KPARAM_MAGIC       0x41504153

static DEFINE_MUTEX(sec_param_mutex);

/* single global instance */
struct sec_param_data *param_data;
struct sec_param_data_s sched_sec_param_data;
static char* salescode_from_cmdline;

static unsigned int param_file_size;

static const char *param_name = "/dev/block/bootdevice/by-name/param";

static int __init sec_get_param_file_size_setup(char *str)
{
	int ret;
	unsigned long long size;

	ret = kstrtoull(str, 0, &size);
	if (ret)
		pr_err("str:%s\n", str);
	else
		param_file_size = (unsigned int)size;

	return 1;
}
__setup("sec_param_file_size=", sec_get_param_file_size_setup);

static void param_sec_operation(struct work_struct *work)
{
	/* Read from PARAM(parameter) partition  */
	struct file *filp;
	mm_segment_t fs;
	int ret;
	struct sec_param_data_s *sched_param_data =
		container_of(work, struct sec_param_data_s, sec_param_work);
	int flag = (sched_param_data->direction == PARAM_WR)
			? (O_RDWR | O_SYNC) : O_RDONLY;

	pr_debug("%p %x %d %d\n", sched_param_data->value,
			sched_param_data->offset, sched_param_data->size,
			sched_param_data->direction);

	sched_sec_param_data.success = false;

	filp = filp_open(param_name, flag, 0);

	if (IS_ERR(filp)) {
		pr_err("filp_open failed. (%ld)\n", PTR_ERR(filp));
		complete(&sched_sec_param_data.work);
		return;
	}

	fs = get_fs();
	set_fs(get_ds());

	ret = vfs_llseek(filp, sched_param_data->offset, SEEK_SET);
	if (unlikely(ret < 0)) {
		pr_err("FAIL LLSEEK\n");
		goto param_sec_debug_out;
	}

	if (sched_param_data->direction == PARAM_RD)
		vfs_read(filp,
				(char __user *)sched_param_data->value,
				sched_param_data->size, &filp->f_pos);
	else if (sched_param_data->direction == PARAM_WR)
		vfs_write(filp,
				(char __user *)sched_param_data->value,
				sched_param_data->size, &filp->f_pos);

	sched_sec_param_data.success = true;

param_sec_debug_out:
	set_fs(fs);
	filp_close(filp, NULL);
	complete(&sched_sec_param_data.work);
}

static bool sec_open_param(void)
{
	static bool is_param_loaded;

	pr_info("start\n");

	if (!param_data)
		param_data = kmalloc(sizeof(struct sec_param_data), GFP_KERNEL);

	if (unlikely(!param_data)) {
		pr_err("failed to alloc for param_data\n");
		return false;
	}

	sched_sec_param_data.value = param_data;
	sched_sec_param_data.offset = SEC_PARAM_FILE_OFFSET;
	sched_sec_param_data.size = sizeof(struct sec_param_data);
	sched_sec_param_data.direction = PARAM_RD;

	schedule_work(&sched_sec_param_data.sec_param_work);
	wait_for_completion(&sched_sec_param_data.work);

	is_param_loaded = sched_sec_param_data.success;
	if (!is_param_loaded) {
		pr_err("%s: param_sec_operation open failed\n", __func__);
		return false;
	}
	pr_info("end\n");

	return true;
}

static bool sec_write_param(void)
{
	pr_info("start\n");

	sched_sec_param_data.value = param_data;
	sched_sec_param_data.offset = SEC_PARAM_FILE_OFFSET;
	sched_sec_param_data.size = sizeof(struct sec_param_data);
	sched_sec_param_data.direction = PARAM_WR;

	schedule_work(&sched_sec_param_data.sec_param_work);
	wait_for_completion(&sched_sec_param_data.work);

	if (!sched_sec_param_data.success) {
		pr_err("%s: param_sec_operation write failed\n", __func__);
		return false;
	}

	pr_info("end\n");

	return true;
}

bool sec_get_param(enum sec_param_index index, void *value)
{
	bool ret;

	mutex_lock(&sec_param_mutex);

	ret = sec_open_param();
	if (unlikely(!ret)) {
		pr_err("can't open a param partition\n");
		goto out;
	}

	switch (index) {
	case param_index_debuglevel:
		memcpy(value, &(param_data->debuglevel), sizeof(unsigned int));
		break;
	case param_index_uartsel:
		memcpy(value, &(param_data->uartsel), sizeof(unsigned int));
		break;
	case param_rory_control:
		memcpy(value, &(param_data->rory_control),
				sizeof(unsigned int));
		break;
	case param_index_movinand_checksum_done:
		memcpy(value, &(param_data->movinand_checksum_done),
				sizeof(unsigned int));
		break;
	case param_index_movinand_checksum_pass:
		memcpy(value, &(param_data->movinand_checksum_pass),
				sizeof(unsigned int));
		break;
#ifdef CONFIG_GSM_MODEM_SPRD6500
	case param_update_cp_bin:
		memcpy(value, &(param_data->update_cp_bin),
				sizeof(unsigned int));
		pr_info("param_data.update_cp_bin :[%d]!!",
				param_data->update_cp_bin);
		break;
#endif
#ifdef CONFIG_RTC_AUTO_PWRON_PARAM
	case param_index_sapa:
		memcpy(value, param_data->sapa, sizeof(unsigned int)*3);
		break;
#endif
#ifdef CONFIG_WIRELESS_IC_PARAM
	case param_index_wireless_ic:
		memcpy(value, &(param_data->wireless_ic), sizeof(unsigned int));
		break;
#endif
#ifdef CONFIG_WIRELESS_CHARGER_HIGH_VOLTAGE
	case param_index_wireless_charging_mode:
		memcpy(value, &(param_data->wireless_charging_mode),
				sizeof(unsigned int));
		break;
#endif
#ifdef CONFIG_MUIC_HV
	case param_index_afc_disable:
		memcpy(value, &(param_data->afc_disable), sizeof(unsigned int));
		break;
#endif
	case param_index_cp_reserved_mem:
		memcpy(value, &(param_data->cp_reserved_mem),
				sizeof(unsigned int));
		break;
	case param_index_lcd_resolution:
		memcpy(value, param_data->param_lcd_resolution,
			sizeof(param_data->param_lcd_resolution));
		break;
	case param_index_reboot_recovery_cause:
		memcpy(value, param_data->reboot_recovery_cause,
				sizeof(param_data->reboot_recovery_cause));
		break;
	case param_index_api_gpio_test:
		memcpy(value, &(param_data->api_gpio_test),
			sizeof(param_data->api_gpio_test));
		break;
	case param_index_api_gpio_test_result:
		memcpy(value, param_data->api_gpio_test_result,
			sizeof(param_data->api_gpio_test_result));
		break;
	case param_index_FMM_lock:
		memcpy(value, &(param_data->FMM_lock),
				sizeof(param_data->FMM_lock));
		break;
#ifdef CONFIG_SEC_QUEST
	case param_index_quest:
		sched_sec_param_data.value = value;
		sched_sec_param_data.offset = SEC_PARAM_QUEST_OFFSET;
		sched_sec_param_data.size = sizeof(struct param_quest_t);
		sched_sec_param_data.direction = PARAM_RD;
		schedule_work(&sched_sec_param_data.sec_param_work);
		wait_for_completion(&sched_sec_param_data.work);
		break;
	case param_index_quest_ddr_result:
		sched_sec_param_data.value = value;
		sched_sec_param_data.offset = SEC_PARAM_QUEST_DDR_RESULT_OFFSET;
		sched_sec_param_data.size = sizeof(struct param_quest_ddr_test_result_t);
		sched_sec_param_data.direction = PARAM_RD;
		schedule_work(&sched_sec_param_data.sec_param_work);
		wait_for_completion(&sched_sec_param_data.work);
		break;
#endif
	default:
		ret = false;
	}

out:
	mutex_unlock(&sec_param_mutex);
	return ret;
}
EXPORT_SYMBOL(sec_get_param);

bool sec_set_param(enum sec_param_index index, void *value)
{
	bool ret;

	mutex_lock(&sec_param_mutex);

	ret = sec_open_param();
	if (unlikely(!ret)) {
		pr_err("can't open a param partition\n");
		goto out;
	}

	switch (index) {
	case param_index_debuglevel:
		if (*(unsigned int*)value == (unsigned int)KERNEL_SEC_DEBUG_LEVEL_LOW || *(unsigned int*)value == (unsigned int)KERNEL_SEC_DEBUG_LEVEL_MID ||
				                           *(unsigned int*)value == (unsigned int)KERNEL_SEC_DEBUG_LEVEL_HIGH) {
			memcpy(&(param_data->debuglevel), value, sizeof(unsigned int));
		}
		break;
	case param_index_uartsel:
		if ( *(unsigned int*)value >= (unsigned int)MUIC_PATH_USB_AP && *(unsigned int*)value <= (unsigned int)MUIC_PATH_AUDIO) {
			memcpy(&(param_data->uartsel), value,
					sizeof(unsigned int));
		}
		break;
	case param_cp_debuglevel:
		if ( *(unsigned int*)value == (unsigned int)ANDROID_CP_DEBUG_ON || *(unsigned int*)value == (unsigned int)ANDROID_CP_DEBUG_OFF) {
			memcpy(&(param_data->cp_debuglevel), value,
					sizeof(unsigned int));
		}
		break;
#ifdef CONFIG_RTC_AUTO_PWRON_PARAM
	case param_index_sapa:
		if (*(unsigned int*)value == (unsigned int)SAPA_KPARAM_MAGIC) {
			memcpy(param_data->sapa, value, sizeof(unsigned int)*3);
		}
		break;
#endif
#ifdef CONFIG_WIRELESS_IC_PARAM
	case param_index_wireless_ic:
		memcpy(&(param_data->wireless_ic), value,
			sizeof(unsigned int));
		break;
#endif
#ifdef CONFIG_WIRELESS_CHARGER_HIGH_VOLTAGE
	case param_index_wireless_charging_mode:
		memcpy(&(param_data->wireless_charging_mode), value,
				sizeof(unsigned int));
		break;
#endif
#ifdef CONFIG_MUIC_HV
	case param_index_afc_disable:
		if (*(char*)value == (char)'0' || *(char*)value == (char)'1') {
			memcpy(&(param_data->afc_disable), value,
					sizeof(unsigned int));
		}
		break;
#endif
	case param_index_cp_reserved_mem:
		if ( *(unsigned int*)value == (unsigned int)CP_MEM_RESERVE_OFF || *(unsigned int*)value == (unsigned int)CP_MEM_RESERVE_ON_1 ||
						 	*(unsigned int*)value == (unsigned int)CP_MEM_RESERVE_ON_2 ) {
			memcpy(&(param_data->cp_reserved_mem), value, sizeof(unsigned int));
		}
		break;

	case param_index_reboot_recovery_cause:
		memcpy(param_data->reboot_recovery_cause,
				value, sizeof(param_data->reboot_recovery_cause));
		break;
	case param_index_api_gpio_test:
		memcpy(&(param_data->api_gpio_test),
				value, sizeof(param_data->api_gpio_test));
		break;
	case param_index_api_gpio_test_result:
		memcpy(&(param_data->api_gpio_test_result),
				value, sizeof(param_data->api_gpio_test_result));
		break;
	case param_index_force_upload_flag:
		if (*(unsigned int*)value == (unsigned int)FORCE_UPLOAD_ON || *(unsigned int*)value == (unsigned int)FORCE_UPLOAD_OFF ) {
			memcpy(&(param_data->force_upload_flag),
					value, sizeof(unsigned int));
		}
		break;
	case param_index_FMM_lock:
		memcpy(&(param_data->FMM_lock),
				value, sizeof(param_data->FMM_lock));

	case param_index_fiemap_update:
		memcpy(&(param_data->fiemap_update), value, sizeof(unsigned int));
		break;

	case param_index_fiemap_result:
		memcpy(&(param_data->fiemap_result),value, sizeof(param_data->fiemap_result));
		break;

#ifdef CONFIG_SEC_QUEST
	case param_index_quest:
		sched_sec_param_data.value = (struct param_quest_t *)value;
		sched_sec_param_data.offset = SEC_PARAM_QUEST_OFFSET;
		sched_sec_param_data.size = sizeof(struct param_quest_t);
		sched_sec_param_data.direction = PARAM_WR;
		schedule_work(&sched_sec_param_data.sec_param_work);
		wait_for_completion(&sched_sec_param_data.work);
		break;
	case param_index_quest_ddr_result:
		sched_sec_param_data.value = (struct param_quest_ddr_test_result_t *)value;
		sched_sec_param_data.offset = SEC_PARAM_QUEST_DDR_RESULT_OFFSET;
		sched_sec_param_data.size = sizeof(struct param_quest_ddr_test_result_t);
		sched_sec_param_data.direction = PARAM_WR;
		schedule_work(&sched_sec_param_data.sec_param_work);
		wait_for_completion(&sched_sec_param_data.work);
		break;
#endif
	default:
		ret = false;
		goto out;
	}

	ret = sec_write_param();

out:
	mutex_unlock(&sec_param_mutex);
	return ret;
}
EXPORT_SYMBOL(sec_set_param);

bool sales_code_is(char* str)
{
	bool status = 0;
	char* salescode;

	salescode = kmalloc(4, GFP_KERNEL);
	if (!salescode) {
		goto out;
	}
	memset(salescode, 0x00,4);

	salescode = salescode_from_cmdline;

	pr_info("%s: %s\n", __func__,salescode);

	if(!strncmp((char *)salescode, str, 3))
		status = 1;

out:	return status;
}
EXPORT_SYMBOL(sales_code_is);

static int __init sales_code_setup(char *str)
{
	salescode_from_cmdline = str;
	return 1;
}
__setup("androidboot.sales_code=", sales_code_setup);

static int __init sec_param_work_init(void)
{
	pr_info("start\n");

	sched_sec_param_data.offset = 0;
	sched_sec_param_data.direction = 0;
	sched_sec_param_data.size = 0;
	sched_sec_param_data.value = NULL;

	init_completion(&sched_sec_param_data.work);
	INIT_WORK(&sched_sec_param_data.sec_param_work, param_sec_operation);

	pr_info("end\n");

	return 0;
}
module_init(sec_param_work_init);

static void __exit sec_param_work_exit(void)
{
	cancel_work_sync(&sched_sec_param_data.sec_param_work);
	pr_info("exit\n");
}
module_exit(sec_param_work_exit);
