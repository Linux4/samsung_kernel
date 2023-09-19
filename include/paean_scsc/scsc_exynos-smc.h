#include <linux/smc.h>
#if !IS_ENABLED(CONFIG_SOC_S5E5515) || \
	(LINUX_VERSION_CODE >= KERNEL_VERSION(5, 15, 0))
#include <soc/samsung/exynos-smc.h>
#endif
