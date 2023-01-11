#include <linux/reboot.h>
#define ARRAY_AND_SIZE(x)	(x), ARRAY_SIZE(x)

extern void __init mmp_map_io(void);
extern void mmp_restart(enum reboot_mode, const char *);
extern void mmp_arch_restart(enum reboot_mode mode, const char *cmd);
extern void __init pxa168_clk_init(void);
extern void __init pxa910_clk_init(void);
extern void __init mmp2_clk_init(void);
extern void __init mmp_of_wakeup_init(void);
