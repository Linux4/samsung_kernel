/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019 MediaTek Inc.
 */
#ifndef MBRAINK_PMU_H
#define MBRAINK_PMU_H

#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/io.h>
#include "mbraink_ioctl_struct_def.h"

int mbraink_enable_pmu_inst_spec(bool enable);
int mbraink_get_pmu_inst_spec(struct mbraink_pmu_info *pmuInfo);

#endif /*end of MBRAINK_PMU_H*/
