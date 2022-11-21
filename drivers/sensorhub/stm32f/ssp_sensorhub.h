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

#define BIG_DATA_SIZE			256
#define PRINT_TRUNCATE			6

#define RESET_REASON_KERNEL_RESET            0x01
#define RESET_REASON_MCU_CRASHED             0x02

int ssp_sensorhub_initialize(struct ssp_data *ssp_data);
void ssp_sensorhub_remove(struct ssp_data *ssp_data);
void ssp_sensorhub_log(const char *, const char *, int);

#endif
