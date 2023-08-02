#include <linux/init.h>
#include <linux/suspend.h>
#include <linux/kobject.h>
#include <linux/fs.h>
#include <linux/io.h>
#include <linux/errno.h>
#include <linux/mm.h>
#include <linux/io.h>
#include <linux/debugfs.h>
#include <linux/delay.h>
#include <linux/wakelock.h>
#include <linux/module.h>
#include <linux/kthread.h>
#include <mach/common.h>
#include <mach/hardware.h>
#include <mach/sci.h>
#include <linux/earlysuspend.h>
#include <mach/sci_glb_regs.h>
#include <linux/random.h>
#include <asm/hardware/cache-l2x0.h>
#include <asm/cacheflush.h>
#include <asm/suspend.h>
#include <linux/vmalloc.h>
#include <linux/cpu.h>
#include <linux/printk.h>
#include <mach/__sc8830_dmc_dfs.h>
#include "mach/chip_x30g/dram_phy_28nm.h"

#ifdef EMC_FREQ_AUTO_TEST
static u32 dfs_freq_array[] = {
	//192,
	200,
	//384,
	400
	//466
	//533
};

static struct wake_lock dfs_test_lock;
static DEFINE_SPINLOCK(emc_freq_spinlock);

extern int scxx30_all_nonboot_cpus_died(void);
extern u32 emc_clk_set(u32 new_clk, u32 sene);

static int emc_freq_test_thread(void * data)
{
	u32 i = 0;
	unsigned long spinlock_flags = 0;
	msleep(17000);
	wake_lock(&dfs_test_lock);

	/* close cpu. */
//	cpu_down(1);
//	msleep(200);

//	cpu_down(2);
//	msleep(200);

//	cpu_down(3);
//	msleep(2000);

	while(1){
		spin_lock_irqsave(&emc_freq_spinlock, spinlock_flags);

		/* check current cpu number is 1 or not. */
		if (scxx30_all_nonboot_cpus_died() != 1){
			printk("*** %s, num_online_cpus:%d ***\n", __func__, num_online_cpus() );
			return 0;
		}
		
		//set_current_state(TASK_INTERRUPTIBLE);
//		i = get_random_int();
//		i = i % 2;//(sizeof(dfs_freq_array)/ sizeof(dfs_freq_array[0]));
		printk("emc_freq_test_thread i = %x, clk=%d\n", i, dfs_freq_array[i]);

		emc_clk_set(dfs_freq_array[i], 0);

		printk("e\n");

		i++;
		if (i >= 2) {
			i = 0;
		}

		/* resume irq, cpu and so on. */
		spin_unlock_irqrestore(&emc_freq_spinlock, spinlock_flags);

//		schedule_timeout(10);
		msleep(50);
	}

	/* open cpu */
//	cpu_up(1);
//	cpu_up(2);
//	cpu_up(3);
	return 0;
}

/* used this test must make sure cpu is only one. */
/* platsmp.c : sci_get_core_num()  return 1 means signal core */
void __emc_freq_test(void)
{
	struct task_struct * task;
	wake_lock_init(&dfs_test_lock, WAKE_LOCK_SUSPEND,"emc_freq_test_wakelock");
	task = kthread_create(emc_freq_test_thread, NULL, "emc freq test test");
	if (task == 0) {
		printk("Can't crate emc freq test thread!\n");
	}else {
		wake_up_process(task);
	}
}
#endif
