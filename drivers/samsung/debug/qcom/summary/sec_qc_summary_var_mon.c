// SPDX-License-Identifier: GPL-2.0
/*
 * COPYRIGHT(C) 2016-2020 Samsung Electronics Co., Ltd. All Right Reserved.
 */

#include <linux/ctype.h>
#include <linux/init.h>
#include <linux/io.h>
#include <linux/kernel.h>
#include <linux/of.h>
#include <linux/slab.h>
#include <linux/string.h>

#include <linux/samsung/debug/sec_debug.h>
#include <linux/samsung/debug/qcom/sec_qc_summary.h>

#include <asm/setup.h>
#include <soc/qcom/watchdog.h>

#include "sec_qc_summary.h"
#include "sec_qc_summary_external.h"
#include "sec_qc_summary_var_mon.h"

void __summary_add_var_to_info_mon(
		struct sec_qc_summary_simple_var_mon *info_mon,
		const char *name, phys_addr_t pa, unsigned int size)
{
	if (info_mon->idx >= ARRAY_SIZE(info_mon->var)) {
		pr_warn("index variable is out of bound!\n");
		return;
	}

	strlcpy(info_mon->var[info_mon->idx].name,
			name, sizeof(info_mon->var[0].name));
	info_mon->var[info_mon->idx].sizeof_type = size;
	info_mon->var[info_mon->idx].var_paddr = pa;

	info_mon->idx++;
}

void __summary_add_var_to_var_mon(
		struct sec_qc_summary_simple_var_mon *var_mon,
		const char *name, phys_addr_t pa, unsigned int size)
{
	if (var_mon->idx >= ARRAY_SIZE(var_mon->var)) {
		pr_warn("index variable is out of bound!\n");
		return;
	}

	strlcpy(var_mon->var[var_mon->idx].name, name,
			sizeof(var_mon->var[0].name));
	var_mon->var[var_mon->idx].sizeof_type = size;
	var_mon->var[var_mon->idx].var_paddr = pa;

	var_mon->idx++;
}

void __summary_add_array_to_var_mon(
		struct sec_qc_summary_simple_var_mon *var_mon,
		const char *name, phys_addr_t pa, size_t index,
		unsigned int size)
{
	char *varname;

	varname = kasprintf(GFP_KERNEL, "%s_%zu", name, index);

	__summary_add_var_to_var_mon(var_mon, varname, pa, size);

	kfree(varname);
}

static void __summary_info_mon_init_build_root(
		struct qc_summary_drvdata *drvdata,
		struct sec_qc_summary_simple_var_mon *info_mon)
{
	struct device *dev = drvdata->bd.dev;
#ifdef __SEC_QC_SUMMARY_BUILD_ROOT
	const char __build_root[] =
			__stringify(__SEC_QC_SUMMARY_BUILD_ROOT);
#else
	const char __build_root[] = "UNKNOWN";
#endif
	const char *build_root;

	build_root = devm_kstrdup_const(dev, __build_root, GFP_KERNEL);
	__qc_summary_add_info_mon(info_mon, "build_root",
			virt_to_phys(build_root), -1);
}

static phys_addr_t __summary_info_mon_get_erased_cmdline(const char *cmdline, struct device *dev)
{
	static char *erased_command_line;
	const size_t cmdline_len = COMMAND_LINE_SIZE;
	size_t len;
	char *token_begin, *token_end, *to_be_zero;
	static const char *to_be_deleted[] = {
		"ap_serial=0x",
		"serialno=",
		"em.did=",
		"androidboot.un=W",
		"androidboot.un=H",
	};
	size_t i;

	if (erased_command_line)
		return virt_to_phys(erased_command_line);

	erased_command_line = devm_kzalloc(dev, COMMAND_LINE_SIZE, GFP_KERNEL);
	strlcpy(erased_command_line, cmdline, cmdline_len);

	for (i = 0; i < ARRAY_SIZE(to_be_deleted); i++) {
		len = strlen(to_be_deleted[i]);
		token_begin = strstr(erased_command_line,
				to_be_deleted[i]);
		if (!token_begin)
			continue;

		token_end = strstr(token_begin, " ");
		if (!token_end)	/* last command line option */
			token_end = &(erased_command_line[cmdline_len - 1]);
		to_be_zero = &(token_begin[len]);
		while (to_be_zero < token_end)
			*to_be_zero++ = '0';
	}

	return virt_to_phys(erased_command_line);
}

static phys_addr_t __summary_info_mon_get_saved_cmdline(const char *cmdline, struct device *dev)
{
	if (IS_ENABLED(CONFIG_SAMSUNG_PRODUCT_SHIP))
		return __summary_info_mon_get_erased_cmdline(cmdline, dev);
	else
		return virt_to_phys(cmdline);
}

static void __summary_info_mon_init_kernel_cmdline(
		struct qc_summary_drvdata *drvdata,
		struct sec_qc_summary_simple_var_mon *info_mon)
{
	struct device *dev = drvdata->bd.dev;
	phys_addr_t phys_saved_cmdline;

	if (IS_BUILTIN(CONFIG_SEC_QC_SUMMARY))
		phys_saved_cmdline = __summary_info_mon_get_saved_cmdline(
				saved_command_line, dev);
	else {
		const char **__saved_command_line_ptr =
				(const char **)__qc_summary_kallsyms_lookup_name(
					drvdata, "saved_command_line");
		const char *__saved_command_line = __saved_command_line_ptr ?
				*__saved_command_line_ptr : NULL;

		phys_saved_cmdline = __summary_info_mon_get_saved_cmdline(
				__saved_command_line, dev);
	}

	__qc_summary_add_info_mon(info_mon, "Kernel cmdline",
			phys_saved_cmdline, -1);
}

static inline void __summary_init_arch_desc(char *arch_desc, size_t max_len)
{
	const char *machine_name;
	int err;

	err = of_property_read_string(of_root, "model", &machine_name);
	if (!err && machine_name)
		goto use_machine_name_from_dt;

	strlcpy(arch_desc,
			"UNKNOWN SAMSUNG Device - machine_name is not detected",
			max_len);

	return;

use_machine_name_from_dt:
	snprintf(arch_desc, max_len, "%s (DT)", machine_name);
}

static void __summary_info_mon_init_hardware_name(
		struct qc_summary_drvdata *drvdata,
		struct sec_qc_summary_simple_var_mon *info_mon)
{
	struct device *dev = drvdata->bd.dev;
	char __arch_desc[128];
	const char *arch_desc;

	__summary_init_arch_desc(__arch_desc, sizeof(__arch_desc));
	arch_desc = devm_kstrdup_const(dev, __arch_desc, GFP_KERNEL);
	__qc_summary_add_info_mon(info_mon, "Hardware name",
			virt_to_phys(arch_desc), -1);
}

static void __summary_info_mon_init_linux_banner(
		struct qc_summary_drvdata *drvdata,
		struct sec_qc_summary_simple_var_mon *info_mon)
{
	if (IS_BUILTIN(CONFIG_SEC_QC_SUMMARY))
		__qc_summary_add_str_to_info_mon(info_mon, linux_banner);
	else {
		const char *__linux_banner =
				(char *)__qc_summary_kallsyms_lookup_name(
					drvdata, "linux_banner");
		__qc_summary_add_info_mon(info_mon, "linux_banner",
				virt_to_phys(__linux_banner), -1);
	}
}

int __qc_summary_info_mon_init(struct builder *bd)
{
	struct qc_summary_drvdata *drvdata =
			container_of(bd, struct qc_summary_drvdata, bd);
	struct sec_qc_summary_simple_var_mon *info_mon =
			&(secdbg_apss(drvdata)->info_mon);

	__summary_info_mon_init_build_root(drvdata, info_mon);
	__summary_info_mon_init_kernel_cmdline(drvdata, info_mon);
	__summary_info_mon_init_hardware_name(drvdata, info_mon);
	__summary_info_mon_init_linux_banner(drvdata, info_mon);

	return 0;
}

static unsigned long long *qc_summary_last_pet;

static int sec_qc_summary_pet_notifier_call(struct notifier_block *this,
		unsigned long l, void *drvdata)
{
	struct msm_watchdog_data *wdog_dd = drvdata;
	unsigned long long pet_ts, last_pet_ts, delta_ts;
	unsigned long pet_nsec, last_pet_nsec, delta_nsec;

	last_pet_ts = *qc_summary_last_pet;
	pet_ts = *qc_summary_last_pet = wdog_dd->last_pet;
	delta_ts = pet_ts - last_pet_ts;

	pet_nsec = do_div(pet_ts, 1000000000);
	last_pet_nsec = do_div(last_pet_ts, 1000000000);
	delta_nsec = do_div(delta_ts, 1000000000);

	/* FIXME: print without a module and a function name */
	printk(KERN_WARNING "msm_watchdog_data - pet: %llu.%06lu / last_pet: %llu.%06lu (delta: %llu.%06lu)\n",
			pet_ts, pet_nsec / 1000,
			last_pet_ts, last_pet_nsec / 1000,
			delta_ts, delta_nsec / 1000);

	return NOTIFY_OK;
}

static struct notifier_block qc_summary_nb_pet = {
	.notifier_call = sec_qc_summary_pet_notifier_call,
};

int __qc_summary_var_mon_init_last_pet(struct builder *bd)
{
	struct qc_summary_drvdata *drvdata =
			container_of(bd, struct qc_summary_drvdata, bd);
	struct sec_qc_summary_simple_var_mon *var_mon =
			&(secdbg_apss(drvdata)->var_mon);
	struct device *dev = bd->dev;

	qc_summary_last_pet = devm_kzalloc(dev, sizeof(*qc_summary_last_pet), GFP_KERNEL);
	__summary_add_var_to_var_mon(var_mon, "last_pet",
			virt_to_phys(qc_summary_last_pet),
			sizeof(*qc_summary_last_pet));

	return qcom_wdt_pet_register_notifier(&qc_summary_nb_pet);
}

int __qc_summary_var_mon_init_last_ns(struct builder *bd)
{
	struct qc_summary_drvdata *drvdata =
			container_of(bd, struct qc_summary_drvdata, bd);
	struct sec_qc_summary_simple_var_mon *var_mon =
			&(secdbg_apss(drvdata)->var_mon);
	atomic64_t *last_ns;
	struct device *dev = bd->dev;

#if IS_BUILTIN(CONFIG_SEC_QC_SUMMARY)
	last_ns = &sec_qc_summary_last_ns;
#else
	last_ns = (atomic64_t *)__qc_summary_kallsyms_lookup_name(drvdata,
			"sec_qc_summary_last_ns");
#endif
	if (!last_ns) {
		dev_warn(drvdata->bd.dev,
				"This kernel is maybe GKI. Use a dummy value...\n");
		last_ns = devm_kmalloc(dev, sizeof(*last_ns), GFP_KERNEL);
		if (!last_ns)
			return -ENOMEM;

		atomic64_set(last_ns, 1234567890);
	}

	__summary_add_var_to_var_mon(var_mon, "last_ns",
				virt_to_phys(last_ns), sizeof(*last_ns));

	return 0;
}

void __qc_summary_var_mon_exit_last_pet(struct builder *bd)
{
	qcom_wdt_pet_unregister_notifier(&qc_summary_nb_pet);
}
