#ifndef __ACPM_H__
#define __ACPM_H__

struct acpm_info {
	unsigned int plugin_num;
	struct device *dev;
	void __iomem *timer_base;
	unsigned int timer_cnt;
	unsigned int enter_wfi;
};

struct acpm_ipc_drvdata {
	bool is_mailbox_master;
	bool is_apm;
	bool is_asm;
};

extern void *memcpy_align_4(void *dest, const void *src, unsigned int n);

#define EXYNOS_PMU_CORTEX_APM_CONFIGURATION                           (0x0100)
#define EXYNOS_PMU_CORTEX_APM_STATUS                                  (0x0104)
#define EXYNOS_PMU_CORTEX_APM_OPTION                                  (0x0108)
#define EXYNOS_PMU_CORTEX_APM_DURATION0                               (0x0110)
#define EXYNOS_PMU_CORTEX_APM_DURATION1                               (0x0114)
#define EXYNOS_PMU_CORTEX_APM_DURATION2                               (0x0118)
#define EXYNOS_PMU_CORTEX_APM_DURATION3                               (0x011C)

#define EXYNOS_TIMER_APM_TCVR					      (0x0008)
#define EXYNOS_PERI_TIMER_MAX					      (0xFFFF)


#define APM_LOCAL_PWR_CFG_RESET         (~(0x1 << 0))

extern struct acpm_framework *acpm_initdata;
extern void __iomem *acpm_srambase;
extern void exynos_acpm_timer_clear(void);
extern u32 exynos_get_peri_timer_icvra(void);
extern u32 acpm_enable_flexpmu_profiler(u32 start);
#if defined(CONFIG_EXYNOS_ACPM) || defined(CONFIG_EXYNOS_ACPM_MODULE)
extern void *get_fvmap_base(void);
#else
static inline void *get_fvmap_base(void)
{
	return (void *)0;
}
#endif
extern void acpm_init_eint_clk_req(u32 eint_num);
extern void acpm_init_eint_nfc_clk_req(u32 eint_num);
extern void acpm_set_sram_addr(void __iomem *base);

extern struct task_info *task_info;
extern bool *task_profile_running;

#endif
