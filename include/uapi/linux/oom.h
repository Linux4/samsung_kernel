#ifndef _UAPI__INCLUDE_LINUX_OOM_H
#define _UAPI__INCLUDE_LINUX_OOM_H

/*
 * /proc/<pid>/oom_score_adj set to OOM_SCORE_ADJ_MIN disables oom killing for
 * pid.
 */
#define OOM_SCORE_ADJ_MIN	(-1000)
#define OOM_SCORE_ADJ_MAX	(1000)

/*
 * /proc/<pid>/oom_adj set to -17 protects from the oom killer for legacy
 * purposes.
 */
#define OOM_DISABLE (-17)
/* inclusive */
#define OOM_ADJUST_MIN (-16)
#define OOM_ADJUST_MAX (15)

#ifdef CONFIG_SCHED_TASK_BEHAVIOR
#define OOM_PERSISTENT_TASK	(-12)
static inline int oom_adj_convert(struct task_struct *task)
{
	int oom_adj = OOM_ADJUST_MIN;
	int mult = 1;
	if (task->signal->oom_score_adj == OOM_SCORE_ADJ_MAX) {
		oom_adj = OOM_ADJUST_MAX;
	} else {
		if (task->signal->oom_score_adj < 0)
			mult = -1;
		oom_adj = roundup(mult * task->signal->oom_score_adj *
			-OOM_DISABLE, OOM_SCORE_ADJ_MAX) /
			OOM_SCORE_ADJ_MAX * mult;
	}
	return oom_adj;
}
#endif /* CONFIG_SCHED_TASK_BEHAVIOR */

#endif /* _UAPI__INCLUDE_LINUX_OOM_H */
