// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2015-2019, MICROTRUST Incorporated
 * All Rights Reserved.
 *
 */

#include <linux/irq.h>
#include <linux/types.h>
#include <linux/delay.h>
#include <linux/mutex.h>
#include <linux/freezer.h>
#include <linux/semaphore.h>
#include <linux/interrupt.h>
#include <linux/of_irq.h>
#include <linux/cpu.h>
#include <linux/notifier.h>
#include <linux/slab.h>
#include <linux/vmalloc.h>

#include "teei_id.h"
#include <teei_secure_api.h>
#include <backward_driver.h>
#include <nt_smc_call.h>
#include <teei_client_main.h>
#include <utdriver_macro.h>
#include <utdriver_irq.h>
#include <switch_queue.h>
#include <notify_queue.h>
#include <fdrv.h>
#include <teei_smc_call.h>
#include "teei_task_link.h"
#include "../tee/soter/soter_private.h"
#include <imsg_log.h>

#define TEEI_BACK_SW		(0x01)
#define TEEI_WORK_DONE		(0x02)
#define TEEI_UNKNOWN_WORK	(0x03)

static unsigned long irq_input_count;
static unsigned long irq_output_count;

static int nt_sched_irq_handler(void)
{
	schedule();

	return TEEI_BACK_SW;
}

static int nt_error_irq_handler(void)
{
	unsigned long error_num = 0;

	error_num = teei_secure_call(N_GET_SE_OS_STATE, 0, 0, 0);
	IMSG_ERROR("secure system ERROR ! error_num = %ld\n",
					(error_num));
	soter_error_flag = 1;

	up(&(boot_sema));

	WARN_ON(1);

	return TEEI_WORK_DONE;
}


static int nt_boot_irq_handler(void)
{
	if (boot_soter_flag == START_STATUS)
		boot_soter_flag = END_STATUS;

	up(&(boot_sema));

	return TEEI_WORK_DONE;
}

void teei_handle_bdrv_call(struct NQ_entry *entry)
{
	struct bdrv_work_struct *work_ent = NULL;
	struct NQ_entry *bdrv_ent = NULL;

	work_ent = kmalloc(sizeof(struct bdrv_work_struct), GFP_KERNEL);
	if (work_ent == NULL) {
		IMSG_ERROR("NO enough memory for work in %s!\n", __func__);
		return;
	}

	bdrv_ent = kmalloc(sizeof(struct NQ_entry), GFP_KERNEL);
	if (bdrv_ent == NULL) {
		IMSG_ERROR("NO enough memory for bdrv in %s!\n", __func__);
		kfree(work_ent);
		return;
	}

	INIT_LIST_HEAD(&(work_ent->c_link));

	memcpy(bdrv_ent, entry, sizeof(struct NQ_entry));

	work_ent->bdrv_work_type = TEEI_BDRV_TYPE;
	work_ent->param_p = bdrv_ent;

	teei_add_to_bdrv_link(&(work_ent->c_link));

	teei_notify_bdrv_fn();

	/* Waiting for the teei_bdrv_thread done
	 * if teei_switch_thread can become a RT thread
	 */
	teei_wait_for_bdrv_done();
}

void teei_handle_schedule_call(struct NQ_entry *entry)
{
}

void teei_handle_capi_call(struct NQ_entry *entry)
{
	struct completion *bp = NULL;

	bp = (struct completion *)(entry->block_p);
	if (bp == NULL) {
		IMSG_ERROR("The block_p of entry is NULL!\n");
		return;
	}

	complete(bp);
}

void teei_handle_config_call(struct NQ_entry *entry)
{
	struct completion *bp = NULL;

	bp = (struct completion *)(entry->block_p);
	if (bp == NULL) {
		IMSG_ERROR("The block_p of entry is NULL!\n");
		return;
	}

	complete(bp);
}

static int nt_switch_irq_handler(void)
{
	struct NQ_entry *entry = NULL;
	unsigned long long cmd_id = 0;
	int retVal = 0;

	entry = get_nq_entry();
	if (entry == NULL) {
		IMSG_ERROR("Can NOT get entry from t_nt_buffer!\n");
		return TEEI_UNKNOWN_WORK;
	}

	cmd_id = entry->cmd_ID;

	switch (cmd_id) {
	case TEEI_CREAT_FDRV:
	case TEEI_CREAT_BDRV:
	case TEEI_LOAD_TEE:
		switch_output_index =
			((unsigned long)switch_output_index + 1) % 10000;
		up(&boot_sema);
		retVal = TEEI_WORK_DONE;
		break;
	case TEEI_FDRV_CALL:
		switch_output_index =
			((unsigned long)switch_output_index + 1) % 10000;
		teei_handle_fdrv_call(entry);
		retVal = TEEI_WORK_DONE;
		break;
	case NEW_CAPI_CALL:
		switch_output_index =
			((unsigned long)switch_output_index + 1) % 10000;
		teei_handle_capi_call(entry);
		retVal = TEEI_WORK_DONE;
		break;
	case TEEI_BDRV_CALL:
		teei_handle_bdrv_call(entry);
		retVal = TEEI_WORK_DONE;
		break;
	case TEEI_SCHED_CALL:
		teei_handle_schedule_call(entry);
		retVal = TEEI_BACK_SW;
		break;
	case TEEI_MODIFY_TEE_CONFIG:
		switch_output_index =
			((unsigned long)switch_output_index + 1) % 10000;
		teei_handle_config_call(entry);
		retVal = TEEI_WORK_DONE;
		break;
	default:
		IMSG_ERROR("[%s][%d] Unknown command ID!\n",
					__func__, __LINE__);
		retVal = TEEI_UNKNOWN_WORK;
	}

	return retVal;
}

static int nt_load_img_handler(void)
{
	struct bdrv_work_struct *work_ent = NULL;

	work_ent = kmalloc(sizeof(struct bdrv_work_struct), GFP_KERNEL);
	if (work_ent == NULL) {
		IMSG_ERROR("NO enough memory for work in %s!\n", __func__);
		return TEEI_WORK_DONE;
	}

	INIT_LIST_HEAD(&(work_ent->c_link));

	work_ent->bdrv_work_type = TEEI_LOAD_IMG_TYPE;

	teei_add_to_bdrv_link(&(work_ent->c_link));

	teei_notify_bdrv_fn();

	return TEEI_WORK_DONE;
}


static int ut_ffa_smc_handler(int val)
{
	int retVal = 0;

	switch (val) {
	case SCHED_IRQ:
		retVal = nt_sched_irq_handler();
		break;
	case LOAD_IMG_IRQ:
		retVal = nt_load_img_handler();
		break;
	case SWITCH_IRQ:
		retVal = nt_switch_irq_handler();
		break;
	case SOTER_ERROR_IRQ:
		retVal = nt_error_irq_handler();
		break;
	case BOOT_IRQ:
		switch_output_index =
			((unsigned long)switch_output_index + 1) % 10000;
		retVal = nt_boot_irq_handler();
		break;
	default:
		retVal = TEEI_UNKNOWN_WORK;
		IMSG_ERROR("get undefine IRQ from secure OS!\n");
	}

	return retVal;
}

void teei_add_irq_count(void)
{
	irq_input_count = (irq_input_count + 1) % 10000;
}

static unsigned long teei_need_notify(void)
{
	if (irq_input_count != irq_output_count) {
		irq_output_count = (irq_output_count + 1) % 10000;
		return 1;
	}

	return 0;
}

int teei_smc(unsigned long long smc_id, unsigned long long p1,
		unsigned long long p2)
{
	unsigned long data0 = 0;
	unsigned long data1 = 0;
	unsigned long data2 = 0;
	unsigned long data3 = 0;
	long result = 0;
	int retVal = 0;

	data0 = smc_id;
	data1 = p1;
	data2 = p2;

	while (1) {

		data3 = teei_need_notify();

		result = teei_secure_call(data0, data1, data2, data3);
		if (result < 0) {
			IMSG_ERROR("teei_secure_call return is wrong\n");
			retVal = (int)result;
			goto done;
		}

		if (result == SMC_CALL_INTERRUPTED_IRQ) {
			data0 = NT_SCHED_T;
			data1 = 0;
			data2 = 0;
			continue;
		}

		retVal = ut_ffa_smc_handler((unsigned long)result);
		if (retVal == TEEI_BACK_SW) {
			data0 = NT_SCHED_T;
			data1 = 0;
			data2 = 0;
			continue;
		} else if (retVal == TEEI_WORK_DONE) {
			retVal = 0;
			break;
		} else {
			IMSG_ERROR("ut_smc_handler return %d!\n", retVal);
			retVal = -EIO;
			break;
		}
	}
done:
	return retVal;
}

static irqreturn_t ut_drv_irq_handler(int irq, void *dev)
{
	return IRQ_HANDLED;
}

int register_ut_irq_handler(int irq)
{
	return request_irq(irq, ut_drv_irq_handler,
				0, "tz_drivers_service", NULL);
}


