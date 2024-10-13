#ifndef __ESCA_DRV_H__
#define __ESCA_DRV_H__

#include <linux/io.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/firmware.h>
#include <linux/notifier.h>
#include <linux/panic_notifier.h>
#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/of_address.h>
#include <linux/of_irq.h>
#include <linux/platform_device.h>
#include <linux/debugfs.h>
#include <linux/interrupt.h>
#include <linux/slab.h>
#include <linux/reset/exynos/exynos-reset.h>
#include <soc/samsung/exynos/debug-snapshot.h>
#include <soc/samsung/exynos/exynos-soc.h>
#include <linux/sched/clock.h>
#include <linux/module.h>
#include <linux/math.h>
#include <linux/delay.h>
#include <linux/list.h>
#include <linux/wait.h>
#include <linux/workqueue.h>
#include <linux/kdebug.h>
#include <soc/samsung/exynos-pmu-if.h>

#include <dt-bindings/soc/samsung/esca.h>
#include <dt-bindings/soc/samsung/esca-ipc.h>
#include "fw_header/framework.h"
#include "esca_ipc.h"
#include "esca_dbg.h"
#include "esca_plg.h"

struct esca_info {
	struct device *dev;
	void __iomem *sram_base;
	void __iomem *timer_base;
	struct esca_ipc_info ipc;
	struct esca_debug_info dbg;
	u32 initdata_base;
	struct esca_framework *initdata;
	u32 layer;
	u32 num_cores;
};

#if IS_ENABLED(CONFIG_EXYNOS_ESCAV2)
extern struct esca_info *exynos_esca[NR_ESCA_LAYERS];

extern int esca_send_data(struct device_node *node, unsigned int check_id,
			struct ipc_config *config);
#else
static inline int esca_send_data(struct device_node *node, unsigned int check_id,
				struct ipc_config *config)
{
	return -1;
}
#endif

static inline void *memcpy_align_4(void *dest, const void *src, unsigned int n)
{
	unsigned int *dp = dest;
	const unsigned int *sp = src;
	int i;

	if ((n % 4))
		BUG();

	n = n >> 2;

	for (i = 0; i < n; i++)
		*dp++ = *sp++;

	return dest;
}

#if defined(CONFIG_SOC_S5E9935)
#define EXYNOS_PMU_SPARE7							(0xa7c)
#define EXYNOS_SPARE_CTRL__DATA7					(0x3c14)
#elif defined(CONFIG_SOC_S5E9945)
#define EXYNOS_PMU_SPARE7							(0x3bc)
#define EXYNOS_SPARE_CTRL__DATA7					(0x3c30)
#endif

extern void exynos_esca_reboot(void);

/* ESCA gptimer(RTC) related */
static inline u32 exynos_get_gptimer_cur_tick(void)
{
       return __raw_readl(exynos_esca[ESCA_TIMER_LAYER]->timer_base);
}
#endif		/* __ESCA_DRV_H__ */
