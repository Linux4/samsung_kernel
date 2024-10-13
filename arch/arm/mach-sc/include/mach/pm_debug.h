#ifndef _MACH_PM_DEBUG_H
#define _MACH_PM_DEBUG_H

#include<mach/hardware.h>
#include<linux/io.h>

/*print message every 30 second*/
#define PM_PRINT_ENABLE

/* use this method we can get time everytime */
#define TIMER_REG(off) (SPRD_TIMER_BASE + (off))
#define SYSCNT_REG(off) (SPRD_SYSCNT_BASE + (off))
#define SYSCNT_COUNT    SYSCNT_REG(0x0004)

static u32 inline get_sys_cnt(void)
{
	u32 val1, val2;
        val1 = __raw_readl(SYSCNT_COUNT);
        val2 = __raw_readl(SYSCNT_COUNT);
        while(val2 != val1) {
             val1 = val2;
             val2 = __raw_readl(SYSCNT_COUNT);
        }
        return val2;
}


/*just for pm_sc8810.c & pm_vlx.c*/

/*sleep mode info*/
#define SLP_MODE_ARM  0
#define SLP_MODE_MCU  1
#define SLP_MODE_DEP  2
#define SLP_MODE_LIT  3
#define SLP_MODE_NON 4

extern void pm_debug_init(void);
extern void print_statisic(void);
extern void wakeup_irq_set(void);
extern void print_irq_loop(void);
extern void hard_irq_set(void);
extern void print_int_status(void);
extern void clr_sleep_mode(void);
extern void set_sleep_mode(int sm);
extern void time_statisic_begin(void);
extern void time_statisic_end(void);
extern void print_time(void);
extern void irq_wakeup_set(void);
extern void time_add(unsigned int time, int ret);
extern void print_hard_irq_inloop(int ret);
extern void pm_debug_save_ahb_glb_regs(void);
extern void pm_debug_set_wakeup_timer(void);
extern int is_print_linux_clock;
extern int is_print_modem_clock;

/* *just for ddi.c */
extern void inc_irq(int irq);

#endif
