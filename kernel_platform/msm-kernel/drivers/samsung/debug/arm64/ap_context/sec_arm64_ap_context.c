// SPDX-License-Identifier: GPL-2.0
/*
 * COPYRIGHT(C) 2020 Samsung Electronics Co., Ltd. All Right Reserved.
 */

#define pr_fmt(fmt)	KBUILD_MODNAME ":%s() " fmt, __func__

#include <linux/device.h>
#include <linux/init.h>
#include <linux/io.h>
#include <linux/kdebug.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/platform_device.h>

#include <trace/hooks/debug.h>

#include <linux/samsung/builder_pattern.h>
#include <linux/samsung/debug/sec_arm64_ap_context.h>
#include <linux/samsung/debug/sec_debug_region.h>

#define __ap_context_read_special_reg(x) ({ \
	uint64_t val; \
	asm volatile ("mrs %0, " # x : "=r"(val)); \
	val; \
})

struct ap_context_drvdata {
	struct builder bd;
	const char *name;
	uint32_t unique_id;
	struct sec_dbg_region_client *client;
	struct sec_arm64_ap_context *ctx;
	struct notifier_block nb_die;
	struct notifier_block nb_panic;
};

enum {
	TYPE_VH_SHOW_REGS = 0,
	TYPE_VH_IPI_STOP,
	/* */
	TYPE_VH_MAX,
	TYPE_VH_UNKNOWN = -EINVAL,
};

struct sec_arm64_ap_context *ap_context[TYPE_VH_MAX];

static void __always_inline __ap_context_save_core_regs_from_pt_regs(
		struct sec_arm64_ap_context *ctx, struct pt_regs *regs)
{
	memcpy_toio(&ctx->core_regs, regs, sizeof(struct pt_regs));
}

/* FIXME: tempoary workaround to prevent linking errors */
void __naked __ap_context_save_core_regs_on_current(struct pt_regs *regs)
{
	asm volatile (
		"stp	x1, x2, [sp, #-0x30]! \n\t"
		"stp	x3, x4, [sp, #0x10] \n\t"
		"stp	x0, x29, [sp, #0x20] \n\t"

		"add	x0, x0, #0x8 \n\t"

		/* x1 ~ x30 */
		"stp	x1, x2, [x0], #0x10 \n\t"
		"stp	x3, x4, [x0], #0x10 \n\t"
		"stp	x5, x6, [x0], #0x10 \n\t"
		"stp	x7, x8, [x0], #0x10 \n\t"
		"stp	x9, x10, [x0], #0x10 \n\t"
		"stp	x11, x12, [x0], #0x10 \n\t"
		"stp	x13, x14, [x0], #0x10 \n\t"
		"stp	x15, x16, [x0], #0x10 \n\t"
		"stp	x17, x18, [x0], #0x10 \n\t"
		"stp	x19, x20, [x0], #0x10 \n\t"
		"stp	x21, x22, [x0], #0x10 \n\t"
		"stp	x23, x24, [x0], #0x10 \n\t"
		"stp	x25, x26, [x0], #0x10 \n\t"
		"stp	x27, x28, [x0], #0x10 \n\t"
		"stp	x29, x30, [x0], #0x10 \n\t"

		/* sp, pc(lr) */
		/* NOTE: To fake a simulator or other snapshot based debugging tools.
		* Using a sp, pc of the caller makes to increase poissbility of
		* unwinding stacks.
		*/
		"add	x29, sp, #0x30 \n\t"
		"stp	x29, x30, [x0], #0x10 \n\t"

		/* pstate */
		"mrs	x1, nzcv \n\t"
		"bic	x1, x1, #0xFFFFFFFF0FFFFFFF \n\t"
		"mrs	x2, daif \n\t"
		"bic	x2, x2, #0xFFFFFFFFFFFFFC3F \n\t"
		"orr	x1, x1, x2 \n\t"
		"mrs	x3, currentel \n\t"
		"bic	x3, x3, #0xFFFFFFFFFFFFFFF3 \n\t"
		"orr	x1, x1, x3 \n\t"
		"mrs	x4, spsel \n\t"
		"bic	x4, x4, #0xFFFFFFFFFFFFFFFE \n\t"
		"orr	x1, x1, x4 \n\t"
		"str	x1, [x0] \n\t"

		/* x0 */
		"ldp	x0, x29, [sp, #0x20] \n\t"
		"str	x0, [x0] \n\t"

		"ldp	x3, x4, [sp, #0x10] \n\t"
		"ldp	x1, x2, [sp], #0x30 \n\t"

		/* restore lr */
		"ldr	x30, [x0, #0xF0] \n\t"
		"ret \n\t"
	);
}

static void __always_inline __ap_context_save_core_extra_regs(
		struct sec_arm64_ap_context *ctx)
{
	uint64_t pstate, which_el;
	uint64_t *regs = &ctx->core_extra_regs[0];

	pstate = __ap_context_read_special_reg(CurrentEl);
	which_el = pstate & PSR_MODE_MASK;

	regs[IDX_CORE_EXTRA_SP_EL0] = __ap_context_read_special_reg(sp_el0);

	if (which_el >= PSR_MODE_EL2t) {
		regs[IDX_CORE_EXTRA_SP_EL1] =
				__ap_context_read_special_reg(sp_el1);
		regs[IDX_CORE_EXTRA_ELR_EL1] =
				__ap_context_read_special_reg(elr_el1);
		regs[IDX_CORE_EXTRA_SPSR_EL1] =
				__ap_context_read_special_reg(spsr_el1);
		regs[IDX_CORE_EXTRA_SP_EL2] =
				__ap_context_read_special_reg(sp_el2);
		regs[IDX_CORE_EXTRA_ELR_EL2] =
				__ap_context_read_special_reg(elr_el2);
		regs[IDX_CORE_EXTRA_SPSR_EL2] =
				__ap_context_read_special_reg(spsr_el2);
	}
}

static void __always_inline __ap_context_save_mmu_regs(
		struct sec_arm64_ap_context *ctx)
{
	uint64_t *mmu = &ctx->mmu_regs[0];

	mmu[IDX_MMU_TTBR0_EL1] = __ap_context_read_special_reg(TTBR0_EL1);
	mmu[IDX_MMU_TTBR1_EL1] = __ap_context_read_special_reg(TTBR1_EL1);
	mmu[IDX_MMU_TCR_EL1] = __ap_context_read_special_reg(TCR_EL1);
	mmu[IDX_MMU_MAIR_EL1] = __ap_context_read_special_reg(MAIR_EL1);
	mmu[IDX_MMU_AMAIR_EL1] = __ap_context_read_special_reg(AMAIR_EL1);
}

static ssize_t __ap_context_unique_id_to_type(uint32_t unique_id)
{
	ssize_t type;

	switch (unique_id) {
	case SEC_ARM64_VH_SHOW_REGS_MAGIC:
		type = TYPE_VH_SHOW_REGS;
		break;
	case SEC_ARM64_VH_IPI_STOP_MAGIC:
		type = TYPE_VH_IPI_STOP;
		break;
	default:
		type = TYPE_VH_UNKNOWN;
		break;
	}

	return type;
}

static int __ap_context_parse_dt_name(struct builder *bd,
		struct device_node *np)
{
	struct ap_context_drvdata *drvdata =
			container_of(bd, struct ap_context_drvdata, bd);

	return of_property_read_string(np, "sec,name", &drvdata->name);
}

static int __ap_context_parse_dt_unique_id(struct builder *bd,
                struct device_node *np)
{
        struct ap_context_drvdata *drvdata =
                        container_of(bd, struct ap_context_drvdata, bd);
        u32 unique_id;
        int err;

        err = of_property_read_u32(np, "sec,unique_id", &unique_id);
        if (err)
                return -EINVAL;

        drvdata->unique_id = (uint32_t)unique_id;

        return 0;
}

static struct dt_builder __ap_context_dt_builder[] = {
	DT_BUILDER(__ap_context_parse_dt_name),
	DT_BUILDER(__ap_context_parse_dt_unique_id),
};

static int __ap_context_parse_dt(struct builder *bd)
{
	return sec_director_parse_dt(bd, __ap_context_dt_builder,
			ARRAY_SIZE(__ap_context_dt_builder));
}

static int __ap_context_alloc_client(struct builder *bd)
{
	struct ap_context_drvdata *drvdata =
			container_of(bd, struct ap_context_drvdata, bd);
	size_t size = sizeof(struct sec_arm64_ap_context) * num_possible_cpus();
	struct sec_dbg_region_client *client;
	ssize_t type;

	type = __ap_context_unique_id_to_type(drvdata->unique_id);
	if (type >= TYPE_VH_MAX || type == TYPE_VH_UNKNOWN)
		return -ERANGE;

	if (ap_context[type])
		return -EBUSY;

	client = sec_dbg_region_alloc(drvdata->unique_id, size);
	if (!client)
		return -ENOMEM;

	client->name = drvdata->name;
	drvdata->client = client;
	drvdata->ctx = (struct sec_arm64_ap_context *)client->virt;

	ap_context[type] = drvdata->ctx;

	return 0;
}

static void __ap_context_free_client(struct builder *bd)
{
	struct ap_context_drvdata *drvdata =
			container_of(bd, struct ap_context_drvdata, bd);
	ssize_t type;

	type = __ap_context_unique_id_to_type(drvdata->unique_id);
	BUG_ON(type < 0 || type >= TYPE_VH_MAX);

	ap_context[type] = NULL;

	sec_dbg_region_free(drvdata->client);
}

static void __trace_android_vh_show_regs(void *unused, struct pt_regs *regs)
{
	struct sec_arm64_ap_context *__ctx = ap_context[TYPE_VH_SHOW_REGS];
	int cpu = smp_processor_id();
	struct sec_arm64_ap_context *ctx = &__ctx[cpu];

	if (ctx->used)
		return;

	__ap_context_save_core_regs_from_pt_regs(ctx, regs);
	__ap_context_save_core_extra_regs(ctx);
	__ap_context_save_mmu_regs(ctx);

	ctx->used = true;

	pr_emerg("context saved (CPU:%d)\n", cpu);
}

static void __trace_android_vh_ipi_stop(void *unused, struct pt_regs *regs)
{
	struct sec_arm64_ap_context *__ctx = ap_context[TYPE_VH_IPI_STOP];
	int cpu = smp_processor_id();
	struct sec_arm64_ap_context *ctx = &__ctx[cpu];

	if (ctx->used)
		return;

	__ap_context_save_core_regs_on_current(&ctx->core_regs);
	__ap_context_save_core_extra_regs(ctx);
	__ap_context_save_mmu_regs(ctx);

	ctx->used = true;

	pr_emerg("context saved (CPU:%d)\n", cpu);
}

/* NOTE: commit 802f3427132ac1d3ebbde92d6712f35960cc8354 is not merged into
 * android12-5.10
 */
#define	register_trace_android_vh_show_regs(__probe, __data) \
		register_trace_android_vh_ipi_stop(__probe, __data)

#define	unregister_trace_android_vh_show_regs(__probe, __data) \
		unregister_trace_android_vh_ipi_stop(__probe, __data)

static int __ap_context_register_vh(struct builder *bd)
{
	struct ap_context_drvdata *drvdata =
			container_of(bd, struct ap_context_drvdata, bd);
	ssize_t type;
	int err;

	type = __ap_context_unique_id_to_type(drvdata->unique_id);
	if (type >= TYPE_VH_MAX || type == TYPE_VH_UNKNOWN)
		return -ERANGE;

	switch (type) {
	case TYPE_VH_IPI_STOP:
		err = register_trace_android_vh_ipi_stop(
				__trace_android_vh_ipi_stop, NULL);
		break;
	case TYPE_VH_SHOW_REGS:
		err = register_trace_android_vh_show_regs(
				__trace_android_vh_show_regs, NULL);
		break;
	default:
		err = -EINVAL;
	}

	return err;
}

static void __ap_context_unregister_vh(struct builder *bd)
{
	struct ap_context_drvdata *drvdata =
			container_of(bd, struct ap_context_drvdata, bd);
	struct device *dev = bd->dev;
	ssize_t type;

	type = __ap_context_unique_id_to_type(drvdata->unique_id);
	if (type >= TYPE_VH_MAX || type == TYPE_VH_UNKNOWN) {
		dev_warn(dev, "invalid type number - %zd\n", type);
		return;
	}

	switch (type) {
	case TYPE_VH_IPI_STOP:
		unregister_trace_android_vh_ipi_stop(
				__trace_android_vh_ipi_stop, NULL);
		break;
	case TYPE_VH_SHOW_REGS:
		unregister_trace_android_vh_show_regs(
				__trace_android_vh_show_regs, NULL);
		break;
	default:
		dev_warn(dev, "%zd is not a valid vendor hook\n", type);
	}
}

static __always_inline void __ap_context_hack_core_regs_for_panic(
		struct pt_regs *regs)
{
	uint64_t *fp;

	/* FIXME: stack is corrupted by another callees of 'panic'. */
	fp = (uint64_t *)regs->regs[29];
	regs->regs[29] = fp[0];
	regs->sp = fp[0];	/* maybe incorrect value */
	regs->pc = PAGE_OFFSET | fp[1];
}

static int __used __sec_arm64_ap_context_on_panic(struct pt_regs *regs)
{
	struct notifier_block *this = (void *)regs->regs[0];
	struct ap_context_drvdata *drvdata =
			container_of(this, struct ap_context_drvdata, nb_panic);
	struct sec_arm64_ap_context *__ctx = drvdata->ctx;
	struct sec_arm64_ap_context *ctx;
	int cpu;

	if (!__ctx)
		return NOTIFY_DONE;

	cpu = smp_processor_id();
	ctx = &__ctx[cpu];

	if (ctx->used)
		return NOTIFY_DONE;

	__ap_context_hack_core_regs_for_panic(regs);
	__ap_context_save_core_regs_from_pt_regs(ctx, regs);
	__ap_context_save_core_extra_regs(ctx);
	__ap_context_save_mmu_regs(ctx);

	ctx->used = true;

	pr_emerg("context saved (CPU:%d)\n", cpu);

	return NOTIFY_OK;
}

static int __naked sec_arm64_ap_context_on_panic(struct notifier_block *nb,
		unsigned long l, void *d)
{
	asm volatile (
		"stp	x0, x30, [sp, #-0x10]! \n\t"

		/* 'sp' indicates 'struct pt_regs' */
		"sub	sp, sp, %0 \n\t"
		"mov	x0, sp \n\t"
		"bl	__ap_context_save_core_regs_on_current \n\t"

		/* x0, x30 registers */
		"ldp	x0, x30, [sp, %0] \n\t"
		"str	x0, [sp] \n\t"
		"str	x30, [sp, %1] \n\t"

		/* sp, pc : use the previous sp and the current lr */
		"add	x0, sp, %0 \n\t"
		"add	x0, x0, #0x10 \n\t"
		"stp	x0, x30, [sp, %2] \n\t"

		/* concrete notifier */
		"mov	x0, sp \n\t"
		"bl	__sec_arm64_ap_context_on_panic \n\t"
		"add	sp, sp, %0 \n\t"
		"ldp	x1, x30, [sp], #0x10 \n\t"
		"ret \n\t"
		:
		: "i"(sizeof(struct pt_regs)),
		  "i"(offsetof(struct pt_regs, regs[30])),
		  "i"(offsetof(struct pt_regs, sp))
		:
	);
}

static int __ap_context_register_panic_notifier(struct builder *bd)
{
	struct ap_context_drvdata *drvdata =
			container_of(bd, struct ap_context_drvdata, bd);
	struct notifier_block *nb = &drvdata->nb_panic;

	nb->notifier_call = sec_arm64_ap_context_on_panic;

	return atomic_notifier_chain_register(&panic_notifier_list, nb);
}

static void __ap_context_unregister_panic_notifier(struct builder *bd)
{
	struct ap_context_drvdata *drvdata =
			container_of(bd, struct ap_context_drvdata, bd);
	struct notifier_block *nb = &drvdata->nb_panic;

	atomic_notifier_chain_unregister(&panic_notifier_list, nb);
}

static int sec_arm64_ap_context_on_die(struct notifier_block *this,
		unsigned long l, void *data)
{
	struct ap_context_drvdata *drvdata =
			container_of(this, struct ap_context_drvdata, nb_die);
	struct die_args *args = data;
	struct pt_regs *regs = args->regs;
	struct sec_arm64_ap_context *__ctx = drvdata->ctx;
	struct sec_arm64_ap_context *ctx;
	int cpu;

	if (!__ctx)
		return NOTIFY_DONE;

	cpu = smp_processor_id();
	ctx = &__ctx[cpu];

	if (ctx->used)
		return NOTIFY_DONE;

	__ap_context_save_core_regs_from_pt_regs(ctx, regs);
	__ap_context_save_core_extra_regs(ctx);
	__ap_context_save_mmu_regs(ctx);

	ctx->used = true;

	pr_emerg("context saved (CPU:%d)\n", cpu);

	return NOTIFY_OK;
}

static int __ap_context_register_die_notifier(struct builder *bd)
{
	struct ap_context_drvdata *drvdata =
			container_of(bd, struct ap_context_drvdata, bd);
	struct notifier_block *nb = &drvdata->nb_die;

	nb->notifier_call = sec_arm64_ap_context_on_die;

	return register_die_notifier(nb);
}

static void __ap_context_unregister_die_notifier(struct builder *bd)
{
	struct ap_context_drvdata *drvdata =
			container_of(bd, struct ap_context_drvdata, bd);
	struct notifier_block *nb = &drvdata->nb_die;

	unregister_die_notifier(nb);
}

static int __ap_context_probe(struct platform_device *pdev,
		struct dev_builder *builder, ssize_t n)
{
	struct device *dev = &pdev->dev;
	struct ap_context_drvdata *drvdata;

	drvdata = devm_kzalloc(dev, sizeof(*drvdata), GFP_KERNEL);
	if (!drvdata)
		return -ENOMEM;

	drvdata->bd.dev = dev;

	return sec_director_probe_dev(&drvdata->bd, builder, n);
}

static int __ap_context_remove(struct platform_device *pdev,
		struct dev_builder *builder, ssize_t n)
{
	struct ap_context_drvdata *drvdata = platform_get_drvdata(pdev);

	sec_director_destruct_dev(&drvdata->bd, builder, n, n);

	return 0;
}

static struct dev_builder __ap_context_dev_builder[] = {
	DEVICE_BUILDER(__ap_context_parse_dt, NULL),
	DEVICE_BUILDER(__ap_context_alloc_client, __ap_context_free_client),
	DEVICE_BUILDER(__ap_context_register_vh, __ap_context_unregister_vh),
	DEVICE_BUILDER(__ap_context_register_panic_notifier,
		       __ap_context_unregister_panic_notifier),
	DEVICE_BUILDER(__ap_context_register_die_notifier,
		       __ap_context_unregister_die_notifier),
};

static int sec_ap_context_probe(struct platform_device *pdev)
{
	return __ap_context_probe(pdev, __ap_context_dev_builder,
			ARRAY_SIZE(__ap_context_dev_builder));
}

static int sec_ap_context_remove(struct platform_device *pdev)
{
	return __ap_context_remove(pdev, __ap_context_dev_builder,
			ARRAY_SIZE(__ap_context_dev_builder));
}

static const struct of_device_id sec_ap_context_match_table[] = {
	{ .compatible = "samsung,ap_context" },
	{},
};
MODULE_DEVICE_TABLE(of, sec_ap_context_match_table);

static struct platform_driver sec_ap_context_driver = {
	.driver = {
		.name = "samsung,ap_context",
		.of_match_table = of_match_ptr(sec_ap_context_match_table),
	},
	.probe = sec_ap_context_probe,
	.remove = sec_ap_context_remove,
};

static int __init sec_ap_context_init(void)
{
	return platform_driver_register(&sec_ap_context_driver);
}
module_init(sec_ap_context_init);

static void __exit sec_ap_context_exit(void)
{
	platform_driver_unregister(&sec_ap_context_driver);
}
module_exit(sec_ap_context_exit);

MODULE_AUTHOR("Samsung Electronics");
MODULE_DESCRIPTION("AP CORE/MMU context snaphot");
MODULE_LICENSE("GPL v2");
