
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/sched.h>
#include <linux/time.h>
#include <linux/init.h>
#include <linux/interrupt.h>
#include <linux/hrtimer.h>
#include <linux/delay.h>
#include <linux/io.h>
#include <linux/semaphore.h>
#include <linux/spinlock.h>
#include <linux/fb.h>
#include <linux/msm_mdp.h>
#include <linux/ktime.h>
#include <linux/wakelock.h>
#include <linux/time.h>
#include <asm/system.h>
#include <asm/mach-types.h>
#include <mach/hardware.h>


#define XENTRY	256

struct tlog {
	u64 tick;
	char *name;
	char *data0_name;
	u32 data0;
	char *data1_name;
	u32 data1;
	char *data2_name;
	u32 data2;
	char *data3_name;
	u32 data3;
	char *data4_name;
	u32 data4;
	u32 data5;
};

struct klog {
	struct tlog logs[XENTRY];
	int first;
	int last;
	int init;
} llist;

spinlock_t xlock;

void xlog_fence(char *name, char *data0_name, u32 data0,
				char *data1_name, u32 data1,
				char *data2_name, u32 data2,
				char *data3_name, u32 data3,
				char *data4_name, u32 data4, u32 data5)
{
	unsigned long flags;
	struct tlog *log;

	if (llist.init == 0) {
		spin_lock_init(&xlock);
		llist.init = 1;
	}

	spin_lock_irqsave(&xlock, flags);

	log = &llist.logs[llist.first];
	log->tick = sched_clock();
	log->name = name;
	log->data0_name = data0_name;
	log->data0 = data0;
	log->data1_name = data1_name;
	log->data1 = data1;
	log->data2_name = data2_name;
	log->data2 = data2;
	log->data3_name = data3_name;
	log->data3 = data3;
	log->data4_name = data4_name;
	log->data4 = data4;
	log->data5 = data5;
	llist.last = llist.first;
	llist.first++;
	llist.first %= XENTRY;

	spin_unlock_irqrestore(&xlock, flags);
}

void xlog_fence_dump(void)
{
	int i, n;
	unsigned long flags;
	struct tlog *log;

	spin_lock_irqsave(&xlock, flags);
	i = llist.first;
	pr_info("====== Start Xlog dump for Fence Dubug ======\n");
	for (n = 0 ; n < XENTRY; n++) {
		log = &llist.logs[i++];
		pr_info("[%llu] %s: %s(%d) %s(%d) %s(%d) %s(%d) %s(%d) flag=%x\n",
			log->tick, log->name, log->data0_name, (int)log->data0,
			log->data1_name, (int)log->data1,
			log->data2_name, (int)log->data2,
			log->data3_name, (int)log->data3,
			log->data4_name, (int)log->data4, (int)log->data5);
		i %= XENTRY;
	}
	pr_info("====== Finish Xlog dump for Fence Dubug ======\n");
	spin_unlock_irqrestore(&xlock, flags);

}
