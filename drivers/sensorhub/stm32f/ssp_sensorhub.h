/*
 *  Copyright (C) 2015, Samsung Electronics Co. Ltd. All Rights Reserved.
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

#ifndef __SSP_SENSORHUB_H__
#define __SSP_SENSORHUB_H__

#include <linux/completion.h>
#include <linux/kfifo.h>
#include <linux/kthread.h>
#include <linux/list.h>
#include <linux/spinlock.h>
#include "ssp.h"

/* 'LIST_SIZE' should be be rounded-up to a power of 2 */
#define LIST_SIZE           4
#define MAX_DATA_COPY_TRY       2
#define WAKE_LOCK_TIMEOUT       (3*HZ)
#define COMPLETION_TIMEOUT      (2*HZ)
#define DATA                REL_RX
#define BIG_DATA            REL_RY
#define NOTICE              REL_RZ
#define CALLSTACK_1         REL_X
#define CALLSTACK_2         REL_Y
#define CALLSTACK_3         REL_Z
#define CALLSTACK_4         REL_WHEEL
#define CALLSTACK_5         REL_DIAL
#define BIG_DATA_SIZE           256
#define PRINT_TRUNCATE          6
#define BIN_PATH_SIZE           100

#define CALLSTACK_ALL_DATA_SIZE         20
#define CALLSTACK_ONE_DATA_SIZE         4
#define CALLSTACK_RECEIVE_NONE          0xFFFFFFFF
#define RESET_REASON_KERNEL_RESET            0x01
#define RESET_REASON_MCU_CRASHED             0x02
#define RESET_REASON_SYSFS_REQUEST           0x03

int ssp_sensorhub_initialize(struct ssp_data *ssp_data);
void ssp_sensorhub_remove(struct ssp_data *ssp_data);
void ssp_sensorhub_log(const char *, const char *, int);
void handle_sensorhub_callstack_data(struct ssp_data *data, char *receive_data_frame , int *index);

#endif
