/* Copyright (c) 2011, 2014-2016 The Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
/*HS70 code for HS70-3706 by wuhongwen at 2020/01/09 start*/
#include <kernel_project_defines.h>
/*HS70 code for HS70-3706 by wuhongwen at 2020/01/09 end*/

/*HS70 code for HS70-3706 by wuhongwen at 2020/01/08 start*/
#if defined CONFIG_IRQ_SHOW_RESUME_HS71 || defined CONFIG_IRQ_SHOW_RESUME_HS70
int msm_show_resume_irq_mask = 1;
#else
int msm_show_resume_irq_mask = 0;
#endif
/*HS70 code for HS70-3706 by wuhongwen at 2020/01/08 end*/

module_param_named(
	debug_mask, msm_show_resume_irq_mask, int, S_IRUGO | S_IWUSR | S_IWGRP
);
