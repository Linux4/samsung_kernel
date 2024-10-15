/* SPDX-License-Identifier: GPL-2.0 */
#ifndef __LINUX_RESET_EXYNOS_RESET_H__
#define __LINUX_RESET_EXYNOS_RESET_H__

#if IS_ENABLED(CONFIG_POWER_RESET_EXYNOS)
#if IS_ENABLED(CONFIG_EXYNOS_ESCAV2)
extern void exynos_reboot_register_esca_ops(void *acpm_reboot_func);
#else
extern void exynos_reboot_register_acpm_ops(void *acpm_reboot_func);
#endif
extern void exynos_reboot_register_pmic_ops(void *main_wa,
					    void *sub_wa,
					    void *seq_wa,
					    void *pwrkey);
#else
#if IS_ENABLED(CONFIG_EXYNOS_ESCAV2)
#define exynos_reboot_register_esca_ops(a)	do { } while (0)
#else
#define exynos_reboot_register_acpm_ops(a)	do { } while (0)
#endif
#define exynos_reboot_register_pmic_ops(a,b,c,d)	do { } while (0)
#endif

struct exynos_reboot_helper_ops {
	int (*pmic_off_main_wa)(void);
	int (*pmic_off_sub_wa)(void);
	int (*pmic_off_seq_wa)(void);
	int (*pmic_pwrkey_status)(void);
	void (*acpm_reboot)(void);
};

#endif /* __LINUX_RESET_EXYNOS_RESET_H__ */
