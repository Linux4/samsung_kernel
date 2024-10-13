/*
 * Copyright (C) 2014-2020 NXP Semiconductors, All Rights Reserved.
 * Copyright 2020-2021 GOODIX, All Rights Reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 */

#ifndef __TFA_SYSFS_INC__
#define __TFA_SYSFS_INC__

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/device.h>
#include <linux/string.h>
#include <linux/fs.h>
#include <linux/sysfs.h>
#include <linux/file.h>

#include "tfa_internal.h"
#include "tfa_service.h"
#include "tfa_container.h"

#define TFA_CLASS_NAME	"tfa"

enum tfa_sysfs_device_id {
	DEV_ID_TFA_NULL,
	DEV_ID_TFA_CAL,
	DEV_ID_TFA_LOG,
	DEV_ID_TFA_VVAL,
	DEV_ID_TFA_STC,
	DEV_ID_TFA_MAX
};

int tfa98xx_cal_init(struct class *tfa_class);
void tfa98xx_cal_exit(struct class *tfa_class);

int tfa98xx_log_init(struct class *tfa_class);
void tfa98xx_log_exit(struct class *tfa_class);

int tfa98xx_vval_init(struct class *tfa_class);
void tfa98xx_vval_exit(struct class *tfa_class);

int tfa98xx_stc_init(struct class *tfa_class);
void tfa98xx_stc_exit(struct class *tfa_class);

#endif /* __TFA_SYSFS_INC__ */

