#include <linux/delay.h>
#include <linux/pm.h>
#include <linux/io.h>
#include <mach/memory.h>
#include <mach/hardware.h>
#include <asm/cacheflush.h>
#include <asm/system.h>
#include <mach/system.h>
#include <mach/gpio.h>
#include <mach/sec_debug.h>

#define REBOOT_MODE_PREFIX	0x12345670
#define REBOOT_MODE_NONE	0
#define REBOOT_MODE_DOWNLOAD	1
#define REBOOT_MODE_UPLOAD	2
#define REBOOT_MODE_CHARGING	3
#define REBOOT_MODE_RECOVERY	4
#define REBOOT_MODE_FOTA	5
#define REBOOT_MODE_FOTA_BL	6	/* update bootloader */
#define REBOOT_MODE_SECURE	7	/* image secure check fail */

#define REBOOT_SET_PREFIX	0xabc00000
#define REBOOT_SET_DEBUG	0x000d0000
#define REBOOT_SET_SWSEL	0x000e0000
#define REBOOT_SET_SUD		0x000f0000

static void sec_reboot(char str, const char *cmd)
{
	local_irq_disable();

	pr_emerg("%s (%d, %s)\n", __func__, str, cmd ? cmd : "(null)");

	__raw_writel(0x12345678,SPRD_INFORM2);	/* Don't enter lpm mode */

	if (!cmd) {
		__raw_writel(REBOOT_MODE_PREFIX | REBOOT_MODE_NONE, SPRD_INFORM3);
	} else {
		unsigned long value;
		if (!strcmp(cmd, "fota"))
			__raw_writel(REBOOT_MODE_PREFIX | REBOOT_MODE_FOTA,
			       SPRD_INFORM3);
		else if (!strcmp(cmd, "fota_bl"))
			__raw_writel(REBOOT_MODE_PREFIX | REBOOT_MODE_FOTA_BL,
			       SPRD_INFORM3);
		else if (!strcmp(cmd, "recovery"))
			__raw_writel(REBOOT_MODE_PREFIX | REBOOT_MODE_RECOVERY,
			       SPRD_INFORM3);
		else if (!strcmp(cmd, "download"))
			__raw_writel(REBOOT_MODE_PREFIX | REBOOT_MODE_DOWNLOAD,
			       SPRD_INFORM3);
		else if (!strcmp(cmd, "upload"))
			__raw_writel(REBOOT_MODE_PREFIX | REBOOT_MODE_UPLOAD,
			       SPRD_INFORM3);
		else if (!strcmp(cmd, "secure"))
			__raw_writel(REBOOT_MODE_PREFIX | REBOOT_MODE_SECURE,
			       SPRD_INFORM3);
		else if (!strncmp(cmd, "debug", 5)
			 && !kstrtoul(cmd + 5, 0, &value))
			__raw_writel(REBOOT_SET_PREFIX | REBOOT_SET_DEBUG | value,
			       SPRD_INFORM3);
		else if (!strncmp(cmd, "swsel", 5)
			 && !kstrtoul(cmd + 5, 0, &value))
			__raw_writel(REBOOT_SET_PREFIX | REBOOT_SET_SWSEL | value,
			       SPRD_INFORM3);
		else if (!strncmp(cmd, "sud", 3)
			 && !kstrtoul(cmd + 3, 0, &value))
			__raw_writel(REBOOT_SET_PREFIX | REBOOT_SET_SUD | value,
			       SPRD_INFORM3);
		else if (!strncmp(cmd, "emergency", 9))
			__raw_writel(0, SPRD_INFORM3);
		else
			__raw_writel(REBOOT_MODE_PREFIX | REBOOT_MODE_NONE,
			       SPRD_INFORM3);
	}

	flush_cache_all();
	outer_flush_all();

    arch_reset(0, cmd);

	mdelay(1000);
	pr_emerg("%s: waiting for reboot\n", __func__);
	while (1);
}

static int __init sec_reboot_init(void)
{
	/* to support system shut down */
//	pm_power_off = sec_power_off;
	arm_pm_restart = sec_reboot;
	return 0;
}

subsys_initcall(sec_reboot_init);
