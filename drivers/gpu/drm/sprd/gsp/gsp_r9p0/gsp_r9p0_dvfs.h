#ifndef _GSP_R9P0_DVFS_H_
#define _GSP_R9P0_DVFS_H_

#include "gsp_r9p0_core.h"

void gsp_dvfs_tasklet_schedule(struct gsp_r9p0_core *core, unsigned long data);
void gsp_dvfs_task_func(unsigned long data);
void gsp_dvfs_task_init(struct gsp_r9p0_core *core);

#endif
