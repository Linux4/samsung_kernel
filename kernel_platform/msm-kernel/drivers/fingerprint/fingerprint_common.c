#include "fingerprint_common.h"

void set_sensor_type(const int type_value, int *result)
{
	if (type_value >= SENSOR_OOO && type_value < SENSOR_MAXIMUM) {
		if (type_value == SENSOR_OOO && *result == SENSOR_FAILED) {
			pr_info("maintain type check from out of order :%s\n",
				sensor_status[*result + 2]);
		} else {
			*result = type_value;
			pr_info("FP_SET_SENSOR_TYPE :%s\n", sensor_status[*result + 2]);
		}
	} else {
		pr_err("FP_SET_SENSOR_TYPE invalid value %d\n", type_value);
		*result = SENSOR_UNKNOWN;
	}
}

void enable_fp_debug_timer(struct debug_logger *logger)
{
	mod_timer(&logger->dbg_timer, round_jiffies_up(jiffies + DEBUG_TIMER_SEC));
}

void disable_fp_debug_timer(struct debug_logger *logger)
{
	del_timer_sync(&logger->dbg_timer);
	cancel_work_sync(&logger->work_debug);
}

#if LINUX_VERSION_CODE < KERNEL_VERSION(4, 14, 0)
void timer_func(unsigned long ptr)
#else
void timer_func(struct timer_list *t)
#endif
{
	queue_work(g_logger->wq_dbg, &g_logger->work_debug);
	enable_fp_debug_timer(g_logger);
}

int set_fp_debug_timer(struct debug_logger *logger,
					void (*func)(struct work_struct *work))
{
	int rc = 0;
#if LINUX_VERSION_CODE < KERNEL_VERSION(4, 14, 0)
	setup_timer(&logger->dbg_timer, timer_func, 0);
#else
	timer_setup(&logger->dbg_timer, timer_func, 0);
#endif
	logger->wq_dbg = create_singlethread_workqueue("fingerprint_debug_wq");
	if (!logger->wq_dbg) {
		rc = -ENOMEM;
		pr_err("could not create workqueue\n");
		return rc;
	}
	INIT_WORK(&logger->work_debug, func);

	return rc;
}
