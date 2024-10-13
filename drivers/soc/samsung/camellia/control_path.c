/*
 * Copyright (C) 2021 Samsung Electronics
 *
 * This program is free software; you can redistribute  it and/or modify it
 * under  the terms of  the GNU General  Public License as published by the
 * Free Software Foundation;  either version 2 of the  License, or (at your
 * option) any later version.
 */

#include <linux/kernel.h>
#include <linux/types.h>
#include <linux/list.h>
#include <linux/time.h>

#include "utils.h"
#include "control_path.h"
#include "log.h"
#include "logsink.h"
#include "dump.h"
#include "pm.h"
#include "cache.h"

#define PM_ON			0
#define PM_OFF			1
#define PM_REBOOT		2
#define PM_HIBERNATE	3
#define PM_CANCEL		4

typedef void (*control_message_handler)(u8 event);

void camellia_handle_power_event(u8 event)
{
	switch (event) {
	case PM_HIBERNATE:
		mod_timer(&camellia_dev.pm_timer, jiffies + (PM_WAIT_TIME * HZ));
		camellia_dev.pm_timer_set = CAMELLIA_PM_TIMER_ON;
		break;
	case PM_CANCEL:
		del_timer(&camellia_dev.pm_timer);
		camellia_dev.pm_timer_set = CAMELLIA_PM_TIMER_OFF;
		break;
	default:
		LOG_ERR("Not supported event: %d", event);
		break;
	}
}

void camellia_handle_logger_event(u8 event)
{
	LOG_INFO("Handle logger event");

	camellia_call_log_work(event);
}

void camellia_handle_coredump_event(u8 event)
{
	LOG_INFO("Handle coredump event");

	camellia_call_coredump_work(event);
}

void camellia_handle_security_event(u8 event)
{
	LOG_INFO("Handle security event");
}

static control_message_handler control_message_handlers[] = {
	camellia_handle_power_event,
	camellia_handle_security_event,
	camellia_handle_logger_event,
	camellia_handle_coredump_event,
};

void camellia_dispatch_control_msg(struct camellia_control_msg *msg)
{
	LOG_DBG("Control message - command: %d, sub command: %d",
		msg->cmd, msg->sub_cmd);

	if (msg->cmd < ARRAY_SIZE(control_message_handlers)) {
		control_message_handlers[msg->cmd](msg->sub_cmd);
	} else {
		LOG_ERR("Unknown command: %s", msg->cmd);
	}
	camellia_cache_free(msg);
}
