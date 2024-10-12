/*
 * Copyright (C) 2021 Samsung Electronics
 *
 * This program is free software; you can redistribute  it and/or modify it
 * under  the terms of  the GNU General  Public License as published by the
 * Free Software Foundation;  either version 2 of the  License, or (at your
 * option) any later version.
 */

#include <linux/delay.h>
#include <linux/workqueue.h>

#if IS_ENABLED(CONFIG_EXYNOS_MEMORY_LOGGER)
#include <soc/samsung/exynos/memlogger.h>
#endif

#include "data_path.h"
#include "log.h"
#include "logsink.h"
#include "mailbox.h"
#include "utils.h"

#if IS_ENABLED(CONFIG_EXYNOS_MEMORY_LOGGER)
struct device camellia_memlog_dev;
struct memlog *camellia_memlog_desc;
struct memlog_obj *camellia_memlog_obj;
struct memlog_obj *camellia_memlog_file;
struct memlog_obj *camellia_panic_memlog_file;
void *log_base_vaddr;
#endif

#define LOG_NONE_EVENT         0
#define LOG_BUFFER_RESET_EVENT 1
#define MEMLOG_FILE_NUMBER     5
#define MEMLOG_FILE_SIZE       0x20800

static void camellia_log_work(struct work_struct *work) {
	struct camellia_log_work *log_work = NULL;
	size_t msg_len, sent_msg_len = 0;

	const u8 LOG_BUFFER_RESET_COMMAND = 2;
	const u8 COMMAND_LENGTH = 1;
	const u32 CAMELLIA_LOG_SOURCE = 0;
	const u32 LOGGER_SERVICE_ID = 30;
	int ret = 0;

	LOG_INFO("Log Work Function");

	if (!log_base_vaddr) {
		LOG_ERR("Failed to get log base address");
		return;
	}

	log_work = container_of(work, struct camellia_log_work, work);
	if (log_work->event == LOG_BUFFER_RESET_EVENT) {
		// This is called from SEE logsink
		if (camellia_memlog_file) {
			char buffer[64];
			struct camellia_data_msg *data_msg = (struct camellia_data_msg *)&buffer[0];

			ret = memlog_write_file(camellia_memlog_file, log_base_vaddr, MEMLOG_FILE_SIZE);
			if (ret < 0) {
				LOG_INFO("Failed to write mem log file, ret: %d", ret);
				return;
			}

			data_msg->header.destination = LOGGER_SERVICE_ID;
			data_msg->header.length = COMMAND_LENGTH;
			memcpy(data_msg->payload, &LOG_BUFFER_RESET_COMMAND, COMMAND_LENGTH);

			msg_len = sizeof(struct camellia_data_msg) + COMMAND_LENGTH;
			msleep(4); // This is to avoid RV_MB_AP_ERR_CNT_TIMEOUT or RV_MB_ST_ERR_INVALID_CMD
			sent_msg_len = camellia_send_data_msg(CAMELLIA_LOG_SOURCE, data_msg);
			if (sent_msg_len != msg_len) {
				LOG_ERR("Sent msg leng is not matched, sent len = (%d)", sent_msg_len);
			}
		} else {
			LOG_ERR("Failed to get memlog file object");
		}
	} else {
		// This is called from strong driver
		if (camellia_panic_memlog_file) {
			ret = memlog_write_file(camellia_panic_memlog_file, log_base_vaddr, MEMLOG_FILE_SIZE);
			if (ret < 0) {
				LOG_ERR("Failed to write panic mem log file, ret: %d", ret);
			}
		} else {
			LOG_ERR("Failed to get panic memlog file object");
		}
	}
}

int camellia_memlog_register(void)
{
#if IS_ENABLED(CONFIG_EXYNOS_MEMORY_LOGGER)
	int ret;
	unsigned long dump_base = revmem.base + LOG_OFFSET;

	device_initialize(&camellia_memlog_dev);
	ret = memlog_register("camellia", &camellia_memlog_dev, &camellia_memlog_desc);
	if (ret) {
		LOG_ERR("Failed to register memlog, ret:%d", ret);
		return ret;
	}

	camellia_memlog_file =
			memlog_alloc_file(camellia_memlog_desc, "camellia-file", SZ_256K, 1, 200, MEMLOG_FILE_NUMBER);
	if (camellia_memlog_file) {
		memlog_obj_set_sysfs_mode(camellia_memlog_file, true);
	}

	camellia_panic_memlog_file =
			memlog_alloc_file(camellia_memlog_desc, "camellia-panic-file", SZ_256K, 1, 200, MEMLOG_FILE_NUMBER);
	if (camellia_panic_memlog_file) {
		memlog_obj_set_sysfs_mode(camellia_panic_memlog_file, true);
	}

	log_base_vaddr = camellia_request_region(dump_base, LOG_SIZE);
	if (log_base_vaddr == NULL) {
		LOG_ERR("Failed to map dump area");
	}

	camellia_dev.log_wq = alloc_workqueue("camellia_log", WQ_UNBOUND | WQ_SYSFS, 0);
	if (!camellia_dev.log_wq) {
		LOG_ERR("Failed to allocate workqueue");
	}
	INIT_WORK(&(camellia_dev.log_work.work), camellia_log_work);

	return ret;
#else
	return 0;
#endif
}

void camellia_call_log_work(u8 event)
{
	mutex_lock(&camellia_dev.logsink_lock);
	if (camellia_dev.log_wq) {
		camellia_dev.log_work.event = event;
		queue_work(camellia_dev.log_wq, &(camellia_dev.log_work.work));
	}
	mutex_unlock(&camellia_dev.logsink_lock);
}

void camellia_write_log_file(void)
{
	camellia_call_log_work(LOG_NONE_EVENT);
}
