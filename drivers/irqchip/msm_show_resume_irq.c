// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2011, 2014-2016, 2018, The Linux Foundation. All rights reserved.
 */

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>

//+Chk106693, madongyu.wt, modify, 20211125, turn on mask
int msm_show_resume_irq_mask = 1;
//-Chk106693, madongyu.wt, modify, 20211125, turn on mask

module_param_named(
	debug_mask, msm_show_resume_irq_mask, int, 0664);
