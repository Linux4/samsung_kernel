#include "pmucal_system.h"
#include "pmucal_rae.h"
#ifdef CONFIG_SOC_EXYNOS7570
#include "./S5E7570/pmucal_system_exynos7570.h"
#endif

/**
 *  pmucal_system_enter - prepares to enter a system power mode.
 *		        exposed to PWRCAL interface.
 *
 *  @mode: index of system power mode described in pmucal_system.h.
 *
 *  Returns 0 on success. Otherwise, negative error code.
 */
int pmucal_system_enter(int mode)
{
	int ret;

	exynos_ss_printk("%s %s+\n", PMUCAL_PREFIX, __func__);
	trace_printk("%s %s+\n", PMUCAL_PREFIX, __func__);

	if (mode >= NUM_SYS_POWERDOWN) {
		pr_err("%s %s: mode index(%d) is out of supported range (0~%d).\n",
				PMUCAL_PREFIX, __func__, mode, NUM_SYS_POWERDOWN);
		return -EINVAL;
	}

	if (!pmucal_lpm_list[mode].enter) {
		pr_err("%s %s: there is no sequence element for entering mode(%d).\n",
				PMUCAL_PREFIX, __func__, mode);
		return -ENOENT;
	}

	pmucal_rae_save_seq(pmucal_lpm_list[mode].save, pmucal_lpm_list[mode].num_save);

	ret = pmucal_rae_handle_seq(pmucal_lpm_list[mode].enter,
				pmucal_lpm_list[mode].num_enter);
	if (ret) {
		pr_err("%s %s: error on handling enter sequence. (mode : %d)\n",
				PMUCAL_PREFIX, __func__, mode);
		return ret;
	}

	exynos_ss_printk("%s %s-\n", PMUCAL_PREFIX, __func__);
	trace_printk("%s %s-\n", PMUCAL_PREFIX, __func__);

	return 0;
}

/**
 *  pmucal_system_exit - prepares to exit a system power mode.
 *		        exposed to PWRCAL interface.
 *
 *  @mode: index of system power mode described in pmucal_system.h.
 *
 *  Returns 0 on success. Otherwise, negative error code.
 */
int pmucal_system_exit(int mode)
{
	int ret;

	exynos_ss_printk("%s %s+\n", PMUCAL_PREFIX, __func__);
	trace_printk("%s %s+\n", PMUCAL_PREFIX, __func__);

	if (mode >= NUM_SYS_POWERDOWN) {
		pr_err("%s %s: mode index(%d) is out of supported range (0~%d).\n",
				PMUCAL_PREFIX, __func__, mode, NUM_SYS_POWERDOWN);
		return -EINVAL;
	}

	if (!pmucal_lpm_list[mode].exit) {
		pr_err("%s %s: there is no sequence element for exiting mode(%d).\n",
				PMUCAL_PREFIX, __func__, mode);
		return -ENOENT;
	}

	ret = pmucal_rae_handle_seq(pmucal_lpm_list[mode].exit,
				pmucal_lpm_list[mode].num_exit);
	if (ret) {
		pr_err("%s %s: error on handling exit sequence. (mode : %d)\n",
				PMUCAL_PREFIX, __func__, mode);
		return ret;
	}

	ret = pmucal_rae_restore_seq(pmucal_lpm_list[mode].save,
				pmucal_lpm_list[mode].num_save);
	if (ret) {
		pr_err("%s %s: error on handling restore sequence. (mode : %d)\n",
				PMUCAL_PREFIX, __func__, mode);
		return ret;
	}

	exynos_ss_printk("%s %s-\n", PMUCAL_PREFIX, __func__);
	trace_printk("%s %s-\n", PMUCAL_PREFIX, __func__);

	return 0;
}

/**
 *  pmucal_system_earlywakeup - prepares to early-wakeup from a system power mode.
 *				exposed to PWRCAL interface.
 *
 *  @mode: index of system power mode described in pmucal_system.h.
 *
 *  Returns 0 on success. Otherwise, negative error code.
 */
int pmucal_system_earlywakeup(int mode)
{
	int ret;

	exynos_ss_printk("%s %s+\n", PMUCAL_PREFIX, __func__);
	trace_printk("%s %s+\n", PMUCAL_PREFIX, __func__);

	if (mode >= NUM_SYS_POWERDOWN) {
		pr_err("%s %s: mode index(%d) is out of supported range (0~%d).\n",
				PMUCAL_PREFIX, __func__, mode, NUM_SYS_POWERDOWN);
		return -EINVAL;
	}

	if (!pmucal_lpm_list[mode].early_wakeup) {
		pr_err("%s %s: there is no sequence element for early_wkup mode(%d).\n",
				PMUCAL_PREFIX, __func__, mode);
		return -ENOENT;
	}

	ret = pmucal_rae_handle_seq(pmucal_lpm_list[mode].early_wakeup,
				pmucal_lpm_list[mode].num_early_wakeup);
	if (ret) {
		pr_err("%s %s: error on handling elry_wkup sequence. (mode : %d)\n",
				PMUCAL_PREFIX, __func__, mode);
		return ret;
	}

	ret = pmucal_rae_restore_seq(pmucal_lpm_list[mode].save,
				pmucal_lpm_list[mode].num_save);
	if (ret) {
		pr_err("%s %s: error on handling restore sequence. (mode : %d)\n",
				PMUCAL_PREFIX, __func__, mode);
		return ret;
	}

	exynos_ss_printk("%s %s-\n", PMUCAL_PREFIX, __func__);
	trace_printk("%s %s-\n", PMUCAL_PREFIX, __func__);

	return 0;
}

/**
 *  pmucal_system_init - Init function of PMUCAL SYSTEM common logic.
 *		            exposed to PWRCAL interface.
 *
 *  Returns 0 on success. Otherwise, negative error code.
 */
int __init pmucal_system_init(void)
{
	int ret = 0, i;

	if (!ARRAY_SIZE(pmucal_lpm_init) || !ARRAY_SIZE(pmucal_lpm_list)) {
		pr_err("%s %s: there is no lpm init seq or lpm list. aborting init...\n",
				PMUCAL_PREFIX, __func__);
		return -ENOENT;
	}

	/* convert physical base address to virtual addr */
	for (i = 0; i < ARRAY_SIZE(pmucal_lpm_list); i++) {
		/* skip non-existing power mode */
		if (!pmucal_lpm_list[i].num_enter && !pmucal_lpm_list[i].num_exit && !pmucal_lpm_list[i].num_early_wakeup)
			continue;

		ret = pmucal_rae_phy2virt(pmucal_lpm_list[i].enter,
					pmucal_lpm_list[i].num_enter);
		if (ret) {
			pr_err("%s %s: error on PA2VA conversion. seq:enter, mode_id:%d. aborting init...\n",
					PMUCAL_PREFIX, __func__, i);
			goto out;
		}

		ret = pmucal_rae_phy2virt(pmucal_lpm_list[i].save,
					pmucal_lpm_list[i].num_save);
		if (ret) {
			pr_err("%s %s: error on PA2VA conversion. seq:save, mode_id:%d. aborting init...\n",
					PMUCAL_PREFIX, __func__, i);
			goto out;
		}

		ret = pmucal_rae_phy2virt(pmucal_lpm_list[i].exit,
					pmucal_lpm_list[i].num_exit);
		if (ret) {
			pr_err("%s %s: error on PA2VA conversion. seq:exit, mode_id:%d. aborting init...\n",
					PMUCAL_PREFIX, __func__, i);
			goto out;
		}

		ret = pmucal_rae_phy2virt(pmucal_lpm_list[i].early_wakeup,
					pmucal_lpm_list[i].num_early_wakeup);
		if (ret) {
			pr_err("%s %s: error on PA2VA conversion. seq:erlywkup, mode_id:%d. aborting init...\n",
					PMUCAL_PREFIX, __func__, i);
			goto out;
		}
	}

	ret = pmucal_rae_phy2virt(pmucal_lpm_init, ARRAY_SIZE(pmucal_lpm_init));
	if (ret) {
		pr_err("%s %s: error on PA2VA conversion for lpm_init seq. aborting init...\n",
				PMUCAL_PREFIX, __func__);
		goto out;
	}

	/* do default settings for PMU */
	ret = pmucal_rae_handle_seq(pmucal_lpm_init, ARRAY_SIZE(pmucal_lpm_init));
	if (ret) {
		pr_err("%s %s: error on handling lpm_init sequence.\n",
				PMUCAL_PREFIX, __func__);
		goto out;
	}

out:
	return ret;
}
