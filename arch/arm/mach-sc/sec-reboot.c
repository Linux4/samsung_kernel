#include <linux/delay.h>
#include <linux/pm.h>
#include <asm/io.h>
#include <mach/io.h>
#include <asm/cacheflush.h>
#include <mach/system.h>
#include <linux/notifier.h>
#include <linux/reboot.h>
#include <mach/gpio.h>
#include <mach/sec_debug.h>

#if 0 /* should be fixed later */
/* charger cable state */
extern bool is_cable_attached;

static void sec_power_off(void)
{
	int poweroff_try = 0;

	local_irq_disable();

	pr_emerg("%s : cable state=%d\n", __func__, is_cable_attached);
	flush_cache_all();
	outer_flush_all();

	while (1) {
		/* Check reboot charging */
		if (is_cable_attached || (poweroff_try >= 5)) {
			pr_emerg
			    ("%s: charger connected(%d) or power"
			     "off failed(%d), reboot!\n",
			     __func__, is_cable_attached, poweroff_try);
			writel(0x0, (void __iomem *)S5P_INFORM2);	/* To enter LP charging */

			flush_cache_all();
			outer_flush_all();
			arch_reset(0, 0);

			pr_emerg("%s: waiting for reboot\n", __func__);
			while (1);
		}

		/* wait for power button release */
		if (gpio_get_value(GPIO_nPOWER)) {
			pr_emerg("%s: set PS_HOLD low\n", __func__);

			/* power off code
			 * PS_HOLD Out/High -->
			 * Low PS_HOLD_CONTROL, R/W, 0x1002_330C
			 */
			writel(readl(S5P_PS_HOLD_CONTROL) & 0xFFFFFEFF,
			       (void __iomem *)S5P_PS_HOLD_CONTROL);

			++poweroff_try;
			pr_emerg
			    ("%s: Should not reach here! (poweroff_try:%d)\n",
			     __func__, poweroff_try);
		} else {
			/* if power button is not released, wait and check TA again */
			pr_info("%s: PowerButton is not released.\n", __func__);
		}
		mdelay(1000);
	}
}

#endif  /* should be fixed later */

#define REBOOT_MODE_PREFIX	0x12345670
#define REBOOT_MODE_NONE	0
#define REBOOT_MODE_DOWNLOAD	1
#define REBOOT_MODE_UPLOAD	2
#define REBOOT_MODE_CHARGING	3
#define REBOOT_MODE_RECOVERY	4
#define REBOOT_MODE_ARM11_FOTA	5
#define REBOOT_MODE_CPREBOOT	6

#define REBOOT_SET_PREFIX	0xabc00000
#define REBOOT_SET_DEBUG	0x000d0000
#define REBOOT_SET_SWSEL	0x000e0000
#define REBOOT_SET_SUD		0x000f0000

//extern int sec_debug_level(void);

static int sec_reboot_notifier(struct notifier_block *nb,
							unsigned long l, void *buf)
{
	unsigned long value;
	char *cmd = buf;


	pr_emerg("%s (%lu, %s)\n", __func__, l, cmd ? cmd : "(null)");

	writel(0x12345678,(void __iomem *)SPRD_INFORM2);	/* Don't enter lpm mode */

	if (!cmd) {
		writel(REBOOT_MODE_PREFIX | REBOOT_MODE_NONE, (void __iomem *)SPRD_INFORM3);
	} else {
		if (!strcmp(cmd, "fota"))
			writel(REBOOT_MODE_PREFIX | REBOOT_MODE_ARM11_FOTA,
			       (void __iomem *)SPRD_INFORM3);
		else if (!strcmp(cmd, "recovery"))
			writel(REBOOT_MODE_PREFIX | REBOOT_MODE_RECOVERY,
			       (void __iomem *)SPRD_INFORM3);
		else if (!strcmp(cmd, "download"))
			writel(REBOOT_MODE_PREFIX | REBOOT_MODE_DOWNLOAD,
			       (void __iomem *)SPRD_INFORM3);
		else if (!strcmp(cmd, "upload")) {
			writel(REBOOT_MODE_PREFIX | REBOOT_MODE_UPLOAD,
			       (void __iomem *)SPRD_INFORM3);
		}
		else if (!strncmp(cmd, "debug", 5)
			 && !kstrtoul(cmd + 5, 0, &value))
			writel(REBOOT_SET_PREFIX | REBOOT_SET_DEBUG | value,
			       (void __iomem *)SPRD_INFORM3);
		else if (!strncmp(cmd, "swsel", 5)
			 && !kstrtoul(cmd + 5, 0, &value))
			writel(REBOOT_SET_PREFIX | REBOOT_SET_SWSEL | value,
			       (void __iomem *)SPRD_INFORM3);
		else if (!strncmp(cmd, "sud", 3)
			 && !kstrtoul(cmd + 3, 0, &value))
			writel(REBOOT_SET_PREFIX | REBOOT_SET_SUD | value,
			       (void __iomem *)SPRD_INFORM3);
		else
			writel(REBOOT_MODE_PREFIX | REBOOT_MODE_NONE,
			       (void __iomem *)SPRD_INFORM3);
	}
	flush_cache_all();
	return NOTIFY_DONE; 

}

static struct notifier_block nb_sec_reboot_block = {
	.notifier_call = sec_reboot_notifier
};
static int __init sec_reboot_init(void)
{
	/* to support system shut down */
	register_reboot_notifier(&nb_sec_reboot_block);
	return 0;
}

subsys_initcall(sec_reboot_init);

