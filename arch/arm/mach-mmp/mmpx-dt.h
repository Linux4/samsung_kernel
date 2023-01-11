#ifndef __MMPX_DT_H
#define __MMPX_DT_H

extern void mmp_clk_of_init(void);

#ifdef CONFIG_SMP
extern bool __init mmp_smp_init_ops(void);
#endif

#endif
