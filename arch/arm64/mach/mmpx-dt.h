#ifndef __MMPX_DT_H
#define __MMPX_DT_H

extern void mmp_clk_of_init(void);
extern void __init mmp_reserve_seclogmem(void);
extern void __init mmp_reserve_fbmem(void);
extern void __init mmp_reserve_sec_ftrace_mem(void);

#ifdef CONFIG_MRVL_LOG
extern void __init pxa_reserve_logmem(void);
#endif

#endif
