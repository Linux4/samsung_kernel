#include <linux/delay.h>
#include <asm/io.h>
#include <asm/cacheflush.h>
#include <soc/sprd/system.h>
#include <linux/notifier.h>
#include <linux/reboot.h>

#if defined(CONFIG_SEC_DEBUG)
#include <linux/pm.h>
#include <mach/io.h>
#include <mach/gpio.h>
#include <soc/sprd/sec_debug.h>
#endif
#if defined(CONFIG_SEC_DEBUG64)
#include <soc/sprd/sec_debug64.h>
#endif

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

