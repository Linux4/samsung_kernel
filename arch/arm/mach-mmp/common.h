#define ARRAY_AND_SIZE(x)	(x), ARRAY_SIZE(x)

struct sys_timer;

extern void timer_init(int irq);

extern void __init icu_init_irq(void);
extern void __init mmp_map_io(void);
extern void __init mmp_wakeupgen_init(void);
extern void mmp_restart(char, const char *);
extern void mmp_arch_reset(char mode, const char *cmd);

#ifdef CONFIG_SMP
extern void __iomem *pxa_scu_base_addr(void);
#else
static inline void __iomem *pxa_scu_base_addr(void)
{
	return NULL;
}
#endif
