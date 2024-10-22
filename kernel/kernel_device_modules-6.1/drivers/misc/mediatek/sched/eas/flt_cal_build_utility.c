// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2021 MediaTek Inc.
 */
#include <linux/init.h>
#include <linux/module.h>
#include <linux/sched.h>
#include <linux/kernel.h>
#include <linux/io.h>
#include <linux/mm.h>
#include <linux/slab.h>
#include <linux/cpumask.h>
#include <linux/proc_fs.h>
#include <linux/seq_file.h>
#include <linux/timekeeping.h>
#include <linux/energy_model.h>
#include <linux/cgroup.h>
#include <trace/hooks/sched.h>
#include <trace/hooks/cgroup.h>
#include <linux/sched/cputime.h>
#include <sched/sched.h>
#include <mt-plat/mtk_irq_mon.h>
#include "common.h"
#include "flt_init.h"
#include "group.h"
#include "flt_cal.h"
#include "flt_api.h"
#include "flt_utility.h"
#include "eas_trace.h"
#include "grp_awr.h"
#include <sugov/cpufreq.h>
#include "mtk_energy_model/v2/energy_model.h"

/* src code */
#include "flt_cal.c"
#include "grp_awr.c"
