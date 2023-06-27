#include <linux/i2c.h>

#define ARRAY_AND_SIZE(x)	(x), ARRAY_SIZE(x)

struct pxa_i2c_board_gpio {
	char		type[I2C_NAME_SIZE];
	int		gpio;
};

struct sys_timer;

extern void timer_init(int irq);
extern void __init apb_timer_init(void);

extern void __init icu_init_irq(void);
extern void __init mmp_map_io(void);
extern void __init mmp_wakeupgen_init(void);
extern void mmp_restart(char, const char *);
extern void mmp_arch_reset(char mode, const char *cmd);
#ifdef CONFIG_HAVE_ARM_SCU
extern void __iomem *pxa_scu_base_addr(void);
#endif

#define VER_1V0 0x10
#define VER_1V1 0x11
#define VER_T7  0x70

extern int get_board_id(void);
extern int get_recoverymode(void);

extern void pxa_init_i2c_gpio_irq(struct pxa_i2c_board_gpio *, unsigned len,
				struct i2c_board_info *, unsigned size);
