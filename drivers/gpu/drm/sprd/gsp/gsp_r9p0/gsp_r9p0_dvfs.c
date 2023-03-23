#include <linux/interrupt.h>

#include "gsp_r9p0_dvfs.h"
#include "sprd_dvfs_gsp.h"

#ifdef CONFIG_DRM_SPRD_GSP_DVFS
void gsp_dvfs_tasklet_schedule(struct gsp_r9p0_core *core, unsigned long data)
{
	core->dvfs_task.data = data;
	tasklet_schedule(&core->dvfs_task);
}

void gsp_dvfs_task_func(unsigned long data)
{
	gsp_dvfs_notifier_call_chain(&data);
}

void gsp_dvfs_task_init(struct gsp_r9p0_core *core)
{
	tasklet_init(&core->dvfs_task, gsp_dvfs_task_func, 0);
}
#else
void gsp_dvfs_tasklet_schedule(struct gsp_r9p0_core *core, unsigned long data)
{
	GSP_DEBUG("core do not support dvfs: %s", __func__);
}

void gsp_dvfs_task_func(unsigned long data)
{
	GSP_DEBUG("core do not support dvfs: %s", __func__);
}

void gsp_dvfs_task_init(struct gsp_r9p0_core *core)
{
	GSP_DEBUG("core do not support dvfs: %s", __func__);
}
#endif
