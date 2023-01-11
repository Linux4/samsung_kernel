#ifndef __HELANX_SMC_H
#define __HELANX_SMC_H

#include <linux/notifier.h>

#define GPIO_0_ADDR		0xd401e0dc
#define GPIO_109_ADDR		0xd401e25c
#define GPIO_110_ADDR		0xd401e298
#define GPIO_116_ADDR		0xd401e2b0
#define GPIO_124_ADDR		0xd401e0d0

/* smc function id must be aligned with ATF */
#define LC_ADD_GPIO_EDGE_WAKEUP	0xc2003002
extern noinline int __invoke_fn_smc(u64 function_id, u64 arg0);
extern int mfp_edge_wakeup_notify(struct notifier_block *nb,
				  unsigned long val, void *data);
#endif

