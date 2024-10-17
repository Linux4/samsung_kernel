#if IS_ENABLED(CONFIG_SOC_S5E8535)
#include <linux/regulator/consumer.h>
#endif

#include <soc/samsung/exynos-smc.h>

#include "include/pmucal_cp.h"
#include "include/pmucal_rae.h"

static struct device *pmucal_cp_dev;
#if IS_ENABLED(CONFIG_SOC_S5E8535)
static struct regulator *vdd_cpu_cp;
#define CPU_CP_VOLTAGE	600000	/* microvolt */
#endif

/**
 *  pmucal_cp_init - init cp.
 *		        exposed to PWRCAL interface.

 *  Returns 0 on success. Otherwise, negative error code.
 */
int pmucal_cp_init(void)
{
	int ret;

	pr_info("%s%s()\n", PMUCAL_PREFIX, __func__);

	if (!pmucal_cp_list.init) {
		pr_err("%s there is no sequence element for cp-init.\n",
				PMUCAL_PREFIX);
		return -ENOENT;
	}

#if IS_ENABLED(CONFIG_SOC_S5E8535)
	if (!vdd_cpu_cp) {
		pr_info("%s %s: get vdd_cpu_cp\n", PMUCAL_PREFIX, __func__);
		vdd_cpu_cp = devm_regulator_get(pmucal_cp_dev, "vdd_cpu_cp");
		if (IS_ERR(vdd_cpu_cp)) {
			pr_err("%s %s: get vdd_cpu_cp is failed\n",
				PMUCAL_PREFIX, __func__);
			return PTR_ERR(vdd_cpu_cp);
		}
	}
	pr_info("%s %s: set vdd_cpu_cp to %d\n",
		PMUCAL_PREFIX, __func__, CPU_CP_VOLTAGE);
	ret = regulator_set_voltage(vdd_cpu_cp, CPU_CP_VOLTAGE, CPU_CP_VOLTAGE);
	if (ret) {
		pr_err("%s %s: regulator_set_voltage() error:%d\n",
			PMUCAL_PREFIX, __func__, ret);
		return ret;
	}
#endif

	ret = pmucal_rae_handle_cp_seq(pmucal_cp_list.init,
				pmucal_cp_list.num_init);
	if (ret) {
		pr_err("%s %s: error on handling cp-init sequence.\n",
				PMUCAL_PREFIX, __func__);
		return ret;
	}

	return 0;
}

/**
 *  pmucal_cp_status - get cp status.
 *		        exposed to PWRCAL interface.

 *  Returns 1 when the cp is initialized, 0 when not.
 *  Otherwise, negative error code.
 */
int pmucal_cp_status(unsigned int pmu_cp_status)
{
	int ret;
	pr_info("%s%s()\n", PMUCAL_PREFIX, __func__);

	if (!pmucal_cp_list.status) {
		pr_err("%s there is no sequence element for cp-status.\n",
				PMUCAL_PREFIX);
		return -ENOENT;
	}

	ret = pmucal_rae_handle_cp_seq(pmucal_cp_list.status,
				pmucal_cp_list.num_status);

	if (ret < 0) {
		pr_err("%s %s: error on handling cp-status sequence.\n",
				PMUCAL_PREFIX, __func__);
		return ret;
	}

	if (pmucal_cp_list.status->value == pmu_cp_status)
		return 1;
	else
		return 0;
}

/**
 *  pmucal_cp_reset_assert - reset assert cp.
 *		        exposed to PWRCAL interface.
 *
 *  Returns 0 on success. Otherwise, negative error code.
 */
int pmucal_cp_reset_assert(void)
{
	int ret;
	pr_info("%s%s()\n", PMUCAL_PREFIX, __func__);

	if (!pmucal_cp_list.reset_assert) {
		pr_err("%s there is no sequence element for cp-reset_assert.\n",
				PMUCAL_PREFIX);
		return -ENOENT;
	}

	ret = pmucal_rae_handle_cp_seq(pmucal_cp_list.reset_assert,
				pmucal_cp_list.num_reset_assert);
	if (ret) {
		pr_err("%s %s: error on handling cp-reset_assert sequence.\n",
				PMUCAL_PREFIX, __func__);
		return ret;
	}

	return 0;
}

/**
 *  pmucal_cp_reset_release - reset_release cp.
 *		        exposed to PWRCAL interface.
 *
 *  Returns 0 on success. Otherwise, negative error code.
 */
int pmucal_cp_reset_release(void)
{
	int ret;
	pr_info("%s%s()\n", PMUCAL_PREFIX, __func__);

	if (!pmucal_cp_list.reset_release) {
		pr_err("%s there is no sequence element for cp-reset_release.\n",
				PMUCAL_PREFIX);
		return -ENOENT;
	}

#if IS_ENABLED(CONFIG_SOC_S5E8535)
	pr_info("%s %s: set vdd_cpu_cp to %d\n",
		PMUCAL_PREFIX, __func__, CPU_CP_VOLTAGE);
	ret = regulator_set_voltage(vdd_cpu_cp, CPU_CP_VOLTAGE, CPU_CP_VOLTAGE);
	if (ret) {
		pr_err("%s %s: regulator_set_voltage() error:%d\n",
			PMUCAL_PREFIX, __func__, ret);
		return ret;
	}
#endif

	ret = pmucal_rae_handle_cp_seq(pmucal_cp_list.reset_release,
				pmucal_cp_list.num_reset_release);
	if (ret) {
		pr_err("%s %s: error on handling cp-reset_release sequence.\n",
				PMUCAL_PREFIX, __func__);
		return ret;
	}

	return 0;
}

/**
 *  pmucal_cp_active_clear - cp_active clear
 *		        exposed to PWRCAL interface.
 *
 *  Returns 0 on success. Otherwise, negative error code.
 */
int pmucal_cp_active_clear(void)
{
	int ret;
	pr_info("%s%s()\n", PMUCAL_PREFIX, __func__);

	if (!pmucal_cp_list.cp_active_clear) {
		pr_err("%s there is no sequence element for cp_active_clear.\n",
				PMUCAL_PREFIX);
		return -ENOENT;
	}

	ret = pmucal_rae_handle_cp_seq(pmucal_cp_list.cp_active_clear,
				pmucal_cp_list.num_cp_active_clear);
	if (ret) {
		pr_err("%s %s: error on handling cp_active_clear sequence.\n",
				PMUCAL_PREFIX, __func__);
		return ret;
	}

	return 0;
}

/**
 *  pmucal_cp_enable_dump_pc_no_pg - enable cp dump_pc_no_pg
 *		        exposed to PWRCAL interface.
 *
 *  Returns 0 on success. Otherwise, negative error code.
 */
int pmucal_cp_enable_dump_pc_no_pg(void)
{
	int ret;
	pr_info("%s%s()\n", PMUCAL_PREFIX, __func__);

	if (!pmucal_cp_list.cp_enable_dump_pc_no_pg) {
		pr_err("%s there is no sequence element for cp_enable_dump_pc_no_pg.\n",
				PMUCAL_PREFIX);
		return -ENOENT;
	}

	ret = pmucal_rae_handle_cp_seq(pmucal_cp_list.cp_enable_dump_pc_no_pg,
				pmucal_cp_list.num_cp_enable_dump_pc_no_pg);
	if (ret) {
		pr_err("%s %s: error on handling cp_enable_dump_pc_no_pg sequence.\n",
				PMUCAL_PREFIX, __func__);
		return ret;
	}

	return 0;
}

/**
 *  pmucal_cp_disable_dump_pc_no_pg - enable cp dump_pc_no_pg
 *		        exposed to PWRCAL interface.
 *
 *  Returns 0 on success. Otherwise, negative error code.
 */
int pmucal_cp_disable_dump_pc_no_pg(void)
{
	int ret;
	pr_info("%s%s()\n", PMUCAL_PREFIX, __func__);

	if (!pmucal_cp_list.cp_disable_dump_pc_no_pg) {
		pr_err("%s there is no sequence element for cp_disable_dump_pc_no_pg.\n",
				PMUCAL_PREFIX);
		return -ENOENT;
	}

	ret = pmucal_rae_handle_cp_seq(pmucal_cp_list.cp_disable_dump_pc_no_pg,
				pmucal_cp_list.num_cp_disable_dump_pc_no_pg);
	if (ret) {
		pr_err("%s %s: error on handling cp_disable_dump_pc_no_pg sequence.\n",
				PMUCAL_PREFIX, __func__);
		return ret;
	}

	return 0;
}

int pmucal_is_cp_regs(int reg) {
	int i;
	int is_cp_regs = 0;

	for (i = 0; i < pmucal_cp_list.num_init; i++) {
		if (reg == pmucal_cp_list.init[i].base_pa + pmucal_cp_list.init[i].offset) {
			is_cp_regs = 1;
			goto out;
		}
	}

	for (i = 0; i < pmucal_cp_list.num_reset_assert; i++) {
		if (reg == pmucal_cp_list.reset_assert[i].base_pa + pmucal_cp_list.reset_assert[i].offset) {
			is_cp_regs = 1;
			goto out;
		}
	}

	for (i = 0; i < pmucal_cp_list.num_reset_release; i++) {
		if (reg == pmucal_cp_list.reset_release[i].base_pa + pmucal_cp_list.reset_release[i].offset) {
			is_cp_regs = 1;
			goto out;
		}
	}

	for (i = 0; i < pmucal_cp_list.num_cp_active_clear; i++) {
		if (reg == pmucal_cp_list.cp_active_clear[i].base_pa + pmucal_cp_list.cp_active_clear[i].offset) {
			is_cp_regs = 1;
			goto out;
		}
	}

	for (i = 0; i < pmucal_cp_list.num_cp_reset_req_clear; i++) {
		if (reg == pmucal_cp_list.cp_reset_req_clear[i].base_pa + pmucal_cp_list.cp_reset_req_clear[i].offset) {
			is_cp_regs = 1;
			goto out;
		}
	}

	for (i = 0; i < pmucal_cp_list.num_cp_enable_dump_pc_no_pg; i++) {
		if (reg == pmucal_cp_list.cp_enable_dump_pc_no_pg[i].base_pa + pmucal_cp_list.cp_enable_dump_pc_no_pg[i].offset) {
			is_cp_regs = 1;
			goto out;
		}
	}

	for (i = 0; i < pmucal_cp_list.num_cp_disable_dump_pc_no_pg; i++) {
		if (reg == pmucal_cp_list.cp_disable_dump_pc_no_pg[i].base_pa + pmucal_cp_list.cp_disable_dump_pc_no_pg[i].offset) {
			is_cp_regs = 1;
			goto out;
		}
	}
out:
	return is_cp_regs;
}

/**
 *  pmucal_cp_reset_req_clear - cp_reset_req clear
 *		        exposed to PWRCAL interface.
 *
 *  Returns 0 on success. Otherwise, negative error code.
 */
int pmucal_cp_reset_req_clear(void)
{
	int ret;
	pr_info("%s%s()\n", PMUCAL_PREFIX, __func__);

	if (!pmucal_cp_list.cp_reset_req_clear) {
		pr_err("%s there is no sequence element for cp_reset_req_clear.\n",
				PMUCAL_PREFIX);
		return -ENOENT;
	}

	ret = pmucal_rae_handle_cp_seq(pmucal_cp_list.cp_reset_req_clear,
				pmucal_cp_list.num_cp_reset_req_clear);
	if (ret) {
		pr_err("%s %s: error on handling cp_reset_req_clear sequence.\n",
				PMUCAL_PREFIX, __func__);
		return ret;
	}

	return 0;
}

/**
 *  pmucal_cp_initialize - Initialize function of PMUCAL CP common logic.
 *		            exposed to PWRCAL interface.
 *
 *  Returns 0 on success. Otherwise, negative error code.
 */
int pmucal_cp_initialize(void)
{
	int ret = 0;
	pr_info("%s%s()\n", PMUCAL_PREFIX, __func__);

	if (!pmucal_cp_list_size) {
		pr_err("%s %s: there is no cp list. aborting init...\n",
				PMUCAL_PREFIX, __func__);
		return -ENOENT;
	}

	/* convert physical base address to virtual addr */
	ret = pmucal_rae_phy2virt(pmucal_cp_list.init,
				pmucal_cp_list.num_init);
	if (ret) {
		pr_err("%s %s: error on PA2VA conversion. aborting init...\n",
				PMUCAL_PREFIX, __func__);
		goto out;
	}

	ret = pmucal_rae_phy2virt(pmucal_cp_list.status,
				pmucal_cp_list.num_status);
	if (ret) {
		pr_err("%s %s: error on PA2VA conversion. aborting status...\n",
				PMUCAL_PREFIX, __func__);
		goto out;
	}

	ret = pmucal_rae_phy2virt(pmucal_cp_list.reset_assert,
				pmucal_cp_list.num_reset_assert);
	if (ret) {
		pr_err("%s %s: error on PA2VA conversion. aborting reset_assert...\n",
				PMUCAL_PREFIX, __func__);
		goto out;
	}

	ret = pmucal_rae_phy2virt(pmucal_cp_list.reset_release,
				pmucal_cp_list.num_reset_release);
	if (ret) {
		pr_err("%s %s: error on PA2VA conversion. aborting reset_release...\n",
				PMUCAL_PREFIX, __func__);
		goto out;
	}

	ret = pmucal_rae_phy2virt(pmucal_cp_list.cp_active_clear,
				pmucal_cp_list.num_cp_active_clear);
	if (ret) {
		pr_err("%s %s: error on PA2VA conversion. aborting cp_active_clear...\n",
				PMUCAL_PREFIX, __func__);
		goto out;
	}

	ret = pmucal_rae_phy2virt(pmucal_cp_list.cp_reset_req_clear,
				pmucal_cp_list.num_cp_reset_req_clear);
	if (ret) {
		pr_err("%s %s: error on PA2VA conversion. aborting cp_reset_req_clear...\n",
				PMUCAL_PREFIX, __func__);
		goto out;
	}

	ret = pmucal_rae_phy2virt(pmucal_cp_list.cp_enable_dump_pc_no_pg,
				pmucal_cp_list.num_cp_enable_dump_pc_no_pg);
	if (ret) {
		pr_err("%s %s: error on PA2VA conversion. aborting cp_enable_dump_pc_no_pg...\n",
				PMUCAL_PREFIX, __func__);
		goto out;
	}

	ret = pmucal_rae_phy2virt(pmucal_cp_list.cp_disable_dump_pc_no_pg,
				pmucal_cp_list.num_cp_disable_dump_pc_no_pg);
	if (ret) {
		pr_err("%s %s: error on PA2VA conversion. aborting cp_disable_dump_pc_no_pg...\n",
				PMUCAL_PREFIX, __func__);
		goto out;
	}

out:
	return ret;
}

void cp_set_device(struct device *dev)
{
	pmucal_cp_dev = dev;
}
