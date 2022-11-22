/* drivers/staging/android/pomemr.c
 *
 * page order memory reclaim
 *
 */
#include <linux/mm.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/err.h>
#include <linux/kthread.h>
#include <linux/freezer.h>
#include <linux/cpu.h>
#include <linux/compaction.h>
#include <linux/swap.h>
#include <linux/types.h>
#include <linux/device.h>
#include <linux/vmstat.h>

#include <asm/atomic.h>


extern int get_buddyinfo_higherorder(long reclaim_size[MAX_ORDER_RECLAIM]);
static struct task_struct *kpomemr;
static atomic_t kpomemr_running;

long reclaim_size[MAX_ORDER] = {0};

static int freepage_order_size = MAX_ORDER_RECLAIM;
#ifdef CONFIG_POMEMR_RECLAIM_FILE
#ifndef PKB
#define PKB(x) (x << 2)
#endif
#endif /* CONFIG_POMEMR_RECLAIM_FILE */

static int kpomemr_thread(void * nothing)
{
	int porder = 0;
	unsigned long nr_reclaimed;

	unsigned long did_some_progress;
#ifdef CONFIG_POMEMR_RECLAIM_FILE
	__kernel_ulong_t mem_inactive_file;
#endif /* CONFIG_POMEMR_RECLAIM_FILE */

	set_freezable();
	for ( ; ; ) {
		try_to_freeze();
		if (kthread_should_stop())
			break;
		if (atomic_read(&kpomemr_running) == 1) {
			pr_info("%s\n", __func__);		
			for (porder = MIN_ORDER_RECLAIM; porder < MAX_ORDER_RECLAIM; porder++) {
					if(reclaim_size[porder] > 0) {
						nr_reclaimed = try_to_free_pages_alloc_fast(reclaim_size[porder], porder, LRU_ACTIVE_ANON);
						pr_info("%s: %d ANON recalimed %lu\n", __func__, porder, nr_reclaimed );
					}
					else
						pr_info("%s: %d skip recalim\n", __func__, porder);
			}

            if(get_buddyinfo_higherorder(reclaim_size) == 1) {
                for (porder = MIN_ORDER_RECLAIM; porder < MAX_ORDER_RECLAIM; porder++) {
                    if(reclaim_size[porder] > 0) {
                        did_some_progress = try_to_compact_pages_order(porder);
                        if (did_some_progress != COMPACT_SKIPPED)
                            pr_info("%s: %d compaction SUCCESS\n", __func__, porder);
                        else
                            pr_info("%s: %d compaction FAIL\n", __func__, porder);
                    }
                    else
                        pr_info("%s: %d skip compact\n", __func__, porder);
                }
            }

#ifdef CONFIG_POMEMR_RECLAIM_FILE
			if(get_buddyinfo_higherorder(reclaim_size) == 1) {
                mem_inactive_file = PKB(global_page_state(NR_INACTIVE_FILE));
                pr_info("%s: inactive file %lukB\n", __func__, mem_inactive_file);
                if (mem_inactive_file > 150000) {
                    for (porder = MIN_ORDER_RECLAIM; porder < MAX_ORDER_RECLAIM; porder++) {
                        if(reclaim_size[porder] > 0) {
                            nr_reclaimed = try_to_free_pages_alloc_fast(reclaim_size[porder], porder, LRU_ACTIVE_FILE);
                            pr_info("%s: %d FILE recalimed %lu\n", __func__, porder, nr_reclaimed );
                        }
                        else
                            pr_info("%s: %d skip FILE recalim\n", __func__, porder);
                    }
                }
            }
#endif /* CONFIG_POMEMR_RECLAIM_FILE */

		}
		atomic_set(&kpomemr_running, 0);
		set_current_state(TASK_INTERRUPTIBLE);
		schedule();
	}
	return 0;
}

static int __init pomemr_init(void)
{
	kpomemr = kthread_run(kpomemr_thread, NULL, "kpomemr");
	if (IS_ERR(kpomemr)) {
		/* Failure at boot is fatal */
		BUG_ON(system_state == SYSTEM_BOOTING);
	}
	
	set_user_nice(kpomemr, 5);
	atomic_set(&kpomemr_running, 0);

	return 0;
}


static void __exit pomemr_exit(void)
{
	if (kpomemr) {
		atomic_set(&kpomemr_running, 0);
		kthread_stop(kpomemr);
		kpomemr= NULL;
	}
}

static int page_order_array_set(const char *val, const struct kernel_param *kp)
{
	int porder;
	int ret;
	
	for (porder = MIN_ORDER_RECLAIM; porder < MAX_ORDER_RECLAIM; porder++)
		reclaim_size[porder] = 0;
	ret = param_array_ops.set(val, kp);
	if(get_buddyinfo_higherorder(reclaim_size) == 1) {
        atomic_set(&kpomemr_running, 1);
        wake_up_process(kpomemr);
        pr_info ("%s kpomemr thread wakeup\n", __func__);
    }
    else
        pr_info ("%s kpomemr thread wakeup not needed\n", __func__);
	return ret;
}

static int page_order_array_get(char *buffer, const struct kernel_param *kp)
{
	return param_array_ops.get(buffer, kp);
}

static void page_order_array_free(void *arg)
{
	param_array_ops.free(arg);
}


static struct kernel_param_ops freepage_orders_array_ops= {
	.set = page_order_array_set,
	.get = page_order_array_get,
	.free = page_order_array_free,
};

static const struct kparam_array __freepage_arr_adj= {
	.max = ARRAY_SIZE(reclaim_size),
	.num = &freepage_order_size,
	.ops = &param_ops_long,
	.elemsize = sizeof(reclaim_size[0]),
	.elem = reclaim_size,
};
__module_param_call(MODULE_PARAM_PREFIX, freepage_order,
		    &freepage_orders_array_ops,
		    .arr = &__freepage_arr_adj,
		    S_IRUGO | S_IWUSR, -1);
__MODULE_PARM_TYPE(freepage_order, "array of long");

module_init(pomemr_init);
module_exit(pomemr_exit);

MODULE_LICENSE("GPL");
