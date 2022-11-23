// SPDX-License-Identifier: GPL-2.0
/*
 * COPYRIGHT(C) 2010-2020 Samsung Electronics Co., Ltd. All Right Reserved.
 */

#define pr_fmt(fmt)     KBUILD_MODNAME ":%s() " fmt, __func__

#include <linux/device.h>
#include <linux/io.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/string.h>
#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/of_platform.h>
#include <linux/of_reserved_mem.h>
#include <linux/platform_device.h>
#include <linux/proc_fs.h>
#include <linux/slab.h>
#include <linux/uaccess.h>
#include <linux/vmalloc.h>

#include <linux/samsung/of_early_populate.h>

#include "sec_log_buf.h"

struct log_buf_drvdata *sec_log_buf __read_mostly;

static struct sec_log_buf_head *s_log_buf __read_mostly;
static size_t sec_log_buf_size __read_mostly;

static void (*__log_buf_memcpy_fromio)(void *, const void *, size_t) __read_mostly;
static void (*__log_buf_memcpy_toio)(void *, const void *, size_t) __read_mostly;

static void notrace ____log_buf_memcpy_fromio(void *dst, const void *src, size_t cnt)
{
	memcpy_fromio(dst, src, cnt);
}

static void notrace ____log_buf_memcpy_toio(void *dst, const void *src, size_t cnt)
{
	memcpy_toio(dst, src, cnt);
}

static void notrace ____log_buf_memcpy(void *dst, const void *src, size_t cnt)
{
	memcpy(dst, src, cnt);
}

const struct sec_log_buf_head *sec_log_buf_get_header(void)
{
	if (!__log_buf_is_probed())
		return ERR_PTR(-ENODEV);

	return s_log_buf;
}
EXPORT_SYMBOL(sec_log_buf_get_header);

ssize_t sec_log_buf_get_buf_size(void)
{
	if (!__log_buf_is_probed())
		return -ENODEV;

	return sec_log_buf_size;
}
EXPORT_SYMBOL(sec_log_buf_get_buf_size);

#if IS_ENABLED(CONFIG_SEC_LOG_LAST_KMSG)
static int __last_kmsg_alloc_buffer(struct builder *bd)
{
	struct log_buf_drvdata *drvdata =
			container_of(bd, struct log_buf_drvdata, bd);
	struct last_kmsg_data *last_kmsg = &drvdata->last_kmsg;

	last_kmsg->buf = vmalloc(sec_log_buf_size);
	if (!last_kmsg->buf)
		return -ENOMEM;

	return 0;
}

static void __last_kmsg_free_buffer(struct builder *bd)
{
	struct log_buf_drvdata *drvdata =
			container_of(bd, struct log_buf_drvdata, bd);
	struct last_kmsg_data *last_kmsg = &drvdata->last_kmsg;

	vfree(last_kmsg->buf);
}

static int __last_kmsg_pull_last_log(struct builder *bd)
{
	struct log_buf_drvdata *drvdata =
			container_of(bd, struct log_buf_drvdata, bd);
	struct last_kmsg_data *last_kmsg = &drvdata->last_kmsg;
	char *buf = last_kmsg->buf;
	const size_t max_size = sec_log_buf_size;
	size_t head;

	if (s_log_buf->idx > max_size) {
		head = (size_t)s_log_buf->idx % sec_log_buf_size;
		__log_buf_memcpy_fromio(buf, &s_log_buf->buf[head],
				sec_log_buf_size - head);
		if (head != 0)
			__log_buf_memcpy_fromio(&buf[sec_log_buf_size - head],
					s_log_buf->buf, head);
		last_kmsg->size = max_size;
	} else {
		__log_buf_memcpy_fromio(buf, s_log_buf->buf, s_log_buf->idx);
		last_kmsg->size = s_log_buf->idx;
	}

	return 0;
}

static ssize_t sec_last_kmsg_buf_read(struct file *file, char __user *buf,
		size_t len, loff_t *offset)
{
	struct last_kmsg_data *last_kmsg = PDE_DATA(file_inode(file));
	loff_t pos = *offset;
	ssize_t count;

	if (pos >= last_kmsg->size || !last_kmsg->buf) {
		pr_warn("pos %lld, size %zu\n", pos, last_kmsg->size);
		return 0;
	}

	count = min(len, (size_t)(last_kmsg->size - pos));
	if (copy_to_user(buf, last_kmsg->buf + pos, count))
		return -EFAULT;

	*offset += count;

	return count;
}

static const struct proc_ops last_kmsg_buf_pops = {
	.proc_read = sec_last_kmsg_buf_read,
};

#define LAST_LOG_BUF_NODE		"last_kmsg"

static int __last_kmsg_procfs_create(struct builder *bd)
{
	struct log_buf_drvdata *drvdata =
			container_of(bd, struct log_buf_drvdata, bd);
	struct device *dev = bd->dev;
	struct last_kmsg_data *last_kmsg = &drvdata->last_kmsg;

	last_kmsg->proc = proc_create_data(LAST_LOG_BUF_NODE, 0444,
			NULL, &last_kmsg_buf_pops, last_kmsg);
	if (!last_kmsg->proc) {
		dev_warn(dev, "failed to create proc entry. ram console may be present\n");
		return -ENODEV;
	}

	proc_set_size(last_kmsg->proc, last_kmsg->size);

	return 0;
}

static void __last_kmsg_procfs_remove(struct builder *bd)
{
	struct log_buf_drvdata *drvdata =
			container_of(bd, struct log_buf_drvdata, bd);
	struct last_kmsg_data *last_kmsg = &drvdata->last_kmsg;

	proc_remove(last_kmsg->proc);
}
#else /* CONFIG_SEC_LOG_LAST_KMSG */
static int __last_kmsg_alloc_buffer(struct builder *bd) { return 0; }
static void __last_kmsg_free_buffer(struct builder *bd) {}
static int __last_kmsg_pull_last_log(struct builder *bd) { return 0; }
static int __last_kmsg_procfs_create(struct builder *bd) { return 0; }
static void __last_kmsg_procfs_remove(struct builder *bd) {}
#endif /* CONFIG_SEC_LOG_LAST_KMSG */

void notrace __log_buf_write(const char *s, size_t count)
{
	size_t f_len, s_len, remain_space;
	size_t idx;

	idx = s_log_buf->idx % sec_log_buf_size;
	remain_space = sec_log_buf_size - idx;
	f_len = min(count, remain_space);
	__log_buf_memcpy_toio(&(s_log_buf->buf[idx]), s, f_len);

	s_len = count - f_len;
	if (unlikely(s_len))
		__log_buf_memcpy_toio(s_log_buf->buf, &s[f_len], s_len);

	s_log_buf->idx += (uint32_t)count;
}

static __always_inline size_t __log_buf_print_process(unsigned int cpu,
		char *buf, size_t sz_buf)
{
	return scnprintf(buf, sz_buf, " %c[%2u:%15s:%5u] ",
			in_interrupt() ? 'I' : ' ',
			cpu, current->comm, (unsigned int)current->pid);
}

static __always_inline size_t __log_buf_print_kmsg(unsigned int cpu,
		char *buf, size_t sz_buf)
{
	struct kmsg_dumper *dumper = &sec_log_buf->dumper;
	size_t len;

	if (kmsg_dump_get_line(dumper, true, buf, sz_buf, &len))
		return len;

	return 0;
}

#define SZ_TASK_BUF		32
#define SZ_KMSG_BUF		256

struct log_buf_kmsg_ctx {
	char task[SZ_TASK_BUF];
	size_t task_len;
	char head[SZ_KMSG_BUF];
	size_t head_len;
	char *tail;
	size_t tail_len;
};

static DEFINE_PER_CPU(struct log_buf_kmsg_ctx, kmsg_ctx);
static DEFINE_PER_CPU(struct log_buf_kmsg_ctx, kmsg_ctx_irq);

static bool __log_buf_kmsg_check_level_text(struct log_buf_kmsg_ctx *ctx)
{
	char *head = ctx->head;
	char *endp;
	long l;

	if (head[0] != '<')
		return false;

	/* NOTE: simple_strto{?} fucnctions are not recommended for normal cases.
	 * Because the position of next token is requried, kstrto{?} functions
	 * can not be used or more complex implementation is needed.
	 */
	l = simple_strtol(&head[1], &endp, 10);
	if (!endp || endp[0] != '>')
		return false;

	return true;
}

#if IS_ENABLED(CONFIG_KUNIT) && IS_ENABLED(CONFIG_UML)
/* NOTE: testing-glue to encapsulate 'struct log_buf_kmsg_ctx' */
bool kunit__log_buf_kmsg_check_level_text(const char *msg)
{
	struct log_buf_kmsg_ctx *ctx;
	bool ret;

	ctx = kzalloc(SZ_TASK_BUF, GFP_KERNEL);

	strlcpy(ctx->head, msg, SZ_KMSG_BUF);
	ctx->head_len = strlen(msg);

	ret = __log_buf_kmsg_check_level_text(ctx);

	kfree(ctx);

	return ret;
}
#endif

static void __log_buf_kmsg_split(struct log_buf_kmsg_ctx *ctx)
{
	char *head = ctx->head;
	char *tail;
	const char *delim = "] ";
	size_t head_len;

	tail = strnstr(head, delim, SZ_KMSG_BUF);
	if (!tail) {
		ctx->tail = NULL;
		return;
	}

	tail = &tail[2];
	head_len = tail - head - 1;
	head[head_len] = '\0';

	ctx->tail = tail;
	ctx->tail_len = ctx->head_len - head_len - 1;

	ctx->head_len = head_len;
}

#if IS_ENABLED(CONFIG_KUNIT) && IS_ENABLED(CONFIG_UML)
/* NOTE: testing-glue to encapsulate 'struct log_buf_kmsg_ctx' */
void kunit__log_buf_kmsg_split(char *msg, char **head, char **tail)
{
	struct log_buf_kmsg_ctx *ctx;
	size_t msg_len;
	off_t offset;

	ctx = kzalloc(sizeof(ctx), GFP_KERNEL);

	strlcpy(ctx->head, msg, SZ_KMSG_BUF);
	ctx->head_len = strlen(msg);
	msg_len = ctx->head_len + 1;	/* include null-termination */

	__log_buf_kmsg_split(ctx);

	memcpy(msg, ctx->head, msg_len);

	kfree(ctx);

	offset = ctx->tail - ctx->head;
	*head = &ctx->head[0];
	*tail = &ctx->head[offset];
}
#endif

static __always_inline void __log_buf_kmg_print(struct log_buf_kmsg_ctx *ctx)
{
	if (__log_buf_kmsg_check_level_text(ctx))
		__log_buf_kmsg_split(ctx);

	__log_buf_write(ctx->head, ctx->head_len);
	if (ctx->tail) {
		__log_buf_write(ctx->task, ctx->task_len);
		__log_buf_write(ctx->tail, ctx->tail_len);
	}
}

static int __log_buf_parse_dt_strategy(struct builder *bd,
		struct device_node *np)
{
	struct log_buf_drvdata *drvdata =
			container_of(bd, struct log_buf_drvdata, bd);
	struct device *dev = bd->dev;
	u32 strategy;
	int err;

	err = of_property_read_u32(np, "sec,strategy", &strategy);
	if (err)
		return -EINVAL;

	if (IS_MODULE(CONFIG_SEC_LOG_BUF) &&
			(strategy == SEC_LOG_BUF_STRATEGY_BUILTIN)) {
		dev_err(dev, "BUILTIN strategy can't be used in the kernel module!\n");
		return -EINVAL;
	}

	if (strategy >= SEC_LOG_BUF_NR_STRATEGIES) {
		dev_err(dev, "invalid strategy (%u)!\n", strategy);
		return -EINVAL;
	}

	drvdata->strategy = (unsigned int)strategy;

	return 0;
}

static int __log_buf_parse_dt_memory_region(struct builder *bd,
		struct device_node *np)
{
	struct log_buf_drvdata *drvdata =
			container_of(bd, struct log_buf_drvdata, bd);
	struct device *dev = bd->dev;
	struct device_node *mem_np;
	struct reserved_mem *rmem;

	mem_np = of_parse_phandle(np, "memory-region", 0);
	if (!mem_np)
		return -EINVAL;

	rmem = of_reserved_mem_lookup(mem_np);
	if (!rmem) {
		dev_warn(dev, "failed to get a reserved memory (%s)\n",
				mem_np->name);
		return -EFAULT;
	}

	drvdata->rmem = rmem;

	return 0;
}

static int __log_buf_parse_dt_lease_region(struct builder *bd,
		struct device_node *np)
{
	struct log_buf_drvdata *drvdata =
			container_of(bd, struct log_buf_drvdata, bd);
	struct reserved_mem *rmem = drvdata->rmem;
	phys_addr_t paddr = rmem->base;
	size_t size = rmem->size;
	u32 region_offset;
	u32 region_size;
	int err;

	if (!of_property_read_bool(np, "sec,use-lease_region"))
		goto use_by_default;

	err = of_property_read_u32_index(np, "sec,lease_region",
			0, &region_offset);
	if (err)
		return -EINVAL;

	err = of_property_read_u32_index(np, "sec,lease_region",
			1, &region_size);
	if (err)
		return -EINVAL;

	paddr += (phys_addr_t)region_offset;
	size = (size_t)region_size;

use_by_default:
	if (!size || (size != roundup_pow_of_two(size)))
		return -EFAULT;

	drvdata->paddr = paddr;
	drvdata->size = size;
	return 0;
}

static int __log_buf_parse_dt_test_no_map(struct builder *bd,
		struct device_node *np)
{
	struct log_buf_drvdata *drvdata =
			container_of(bd, struct log_buf_drvdata, bd);
	struct device_node *mem_np;

	mem_np = of_parse_phandle(np, "memory-region", 0);
	if (!mem_np)
		return -EINVAL;

	if (!of_property_read_bool(mem_np, "no-map")) {
		s_log_buf = phys_to_virt(drvdata->paddr);
		__log_buf_memcpy_fromio = ____log_buf_memcpy;
		__log_buf_memcpy_toio = ____log_buf_memcpy;
	} else {
		__log_buf_memcpy_fromio = ____log_buf_memcpy_fromio;
		__log_buf_memcpy_toio = ____log_buf_memcpy_toio;
	}

	return 0;
}

static struct dt_builder __log_buf_dt_builder[] = {
	DT_BUILDER(__log_buf_parse_dt_strategy),
	DT_BUILDER(__log_buf_parse_dt_memory_region),
	DT_BUILDER(__log_buf_parse_dt_lease_region),
	DT_BUILDER(__log_buf_parse_dt_test_no_map),
};

static int __log_buf_parse_dt(struct builder *bd)
{
	return sec_director_parse_dt(bd, __log_buf_dt_builder,
			ARRAY_SIZE(__log_buf_dt_builder));
}

static void __iomem *__log_buf_ioremap(struct log_buf_drvdata *drvdata)
{
	struct device *dev = drvdata->bd.dev;

	if (IS_ENABLED(CONFIG_UML))
		return NULL;

	if (s_log_buf)
		return s_log_buf;

	return devm_ioremap(dev, drvdata->paddr, drvdata->size);
}

static inline void __log_buf_prepare_buffer_raw(struct log_buf_drvdata *drvdata)
{
	struct device *dev = drvdata->bd.dev;

	dev_warn(dev, "sec_log_magic is not valid : 0x%x at 0x%p\n",
			s_log_buf->magic, &(s_log_buf->magic));

	s_log_buf->magic = SEC_LOG_MAGIC;
	s_log_buf->idx = 0;
	s_log_buf->prev_idx = 0;
}

static int __log_buf_prepare_buffer(struct builder *bd)
{
	struct log_buf_drvdata *drvdata =
			container_of(bd, struct log_buf_drvdata, bd);

	s_log_buf = __log_buf_ioremap(drvdata);
	if (!s_log_buf)
		return -EFAULT;

	if (s_log_buf->magic != SEC_LOG_MAGIC)
		__log_buf_prepare_buffer_raw(drvdata);

	sec_log_buf_size = drvdata->size -
			offsetof(struct sec_log_buf_head, buf);

	return 0;
}

static int __log_buf_pull_early_buffer(struct builder *bd)
{
	struct log_buf_drvdata *drvdata =
			container_of(bd, struct log_buf_drvdata, bd);
	struct kmsg_dumper *dumper = &drvdata->dumper;
	char line[256];
	size_t len;

	dumper->active = true;
	kmsg_dump_rewind(dumper);
	while (kmsg_dump_get_line(dumper, true, line, sizeof(line), &len))
		__log_buf_write(line, len);

	return 0;
}

void notrace __log_buf_store_from_kmsg_dumper(void)
{
	unsigned int cpu;
	struct log_buf_kmsg_ctx *ctx_pcpu;

	cpu = get_cpu();

	if (in_irq())
		ctx_pcpu = this_cpu_ptr(&kmsg_ctx_irq);
	else
		ctx_pcpu = this_cpu_ptr(&kmsg_ctx);

	ctx_pcpu->head_len = __log_buf_print_kmsg(cpu,
			ctx_pcpu->head, SZ_KMSG_BUF);
	if (!ctx_pcpu->head_len)
		goto print_nothing;

	ctx_pcpu->task_len = __log_buf_print_process(cpu,
			ctx_pcpu->task, SZ_TASK_BUF);

	do {
		__log_buf_kmg_print(ctx_pcpu);

		ctx_pcpu->head_len = __log_buf_print_kmsg(cpu,
				ctx_pcpu->head, SZ_KMSG_BUF);
	} while (ctx_pcpu->head_len);

print_nothing:
	put_cpu();
}

static int __log_buf_probe_epilog(struct builder *bd)
{
	struct log_buf_drvdata *drvdata =
			container_of(bd, struct log_buf_drvdata, bd);
	struct device *dev = bd->dev;

	dev_set_drvdata(dev, drvdata);
	sec_log_buf = drvdata;	/* set a singleton */

	dev_info(dev, "buf base virtual addrs 0x%p phy=%pa\n", s_log_buf,
			&sec_log_buf->paddr);

	return 0;
}

static void __log_buf_remove_prolog(struct builder *bd)
{
	/* FIXME: This is not a graceful exit.
	 * 'sec_log_buf' can be used in some calling 'printk'.
	 */
	sec_log_buf = NULL;
}

static int __log_buf_probe(struct platform_device *pdev,
		struct dev_builder *builder, ssize_t n)
{
	struct device *dev = &pdev->dev;
	struct log_buf_drvdata *drvdata;

	drvdata = devm_kzalloc(dev, sizeof(*drvdata), GFP_KERNEL);
	if (!drvdata)
		return -ENOMEM;

	drvdata->bd.dev = dev;

	return sec_director_probe_dev(&drvdata->bd, builder, n);
}

static int __log_buf_remove(struct platform_device *pdev,
		struct dev_builder *builder, ssize_t n)
{
	struct log_buf_drvdata *drvdata = platform_get_drvdata(pdev);

	sec_director_destruct_dev(&drvdata->bd, builder, n, n);

	return 0;
}

static struct dev_builder __log_buf_dev_builder[] = {
	DEVICE_BUILDER(__log_buf_parse_dt, NULL),
	DEVICE_BUILDER(__log_buf_prepare_buffer, NULL),
	DEVICE_BUILDER(__last_kmsg_alloc_buffer, __last_kmsg_free_buffer),
	DEVICE_BUILDER(__last_kmsg_pull_last_log, NULL),
	DEVICE_BUILDER(__last_kmsg_procfs_create, __last_kmsg_procfs_remove),
	DEVICE_BUILDER(__log_buf_pull_early_buffer, NULL),
	DEVICE_BUILDER(__log_buf_logger_init, __log_buf_logger_exit),
	DEVICE_BUILDER(__log_buf_probe_epilog, __log_buf_remove_prolog),
};

static int sec_log_buf_probe(struct platform_device *pdev)
{
	return __log_buf_probe(pdev, __log_buf_dev_builder,
			ARRAY_SIZE(__log_buf_dev_builder));
}

static int sec_log_buf_remove(struct platform_device *pdev)
{
	return __log_buf_remove(pdev, __log_buf_dev_builder,
			ARRAY_SIZE(__log_buf_dev_builder));
}

static const struct of_device_id sec_log_buf_match_table[] = {
	{ .compatible = "samsung,kernel_log_buf" },
	{},
};
MODULE_DEVICE_TABLE(of, sec_log_buf_match_table);

static struct platform_driver sec_log_buf_driver = {
	.driver = {
		.name = "samsung,kernel_log_buf",
		.of_match_table = of_match_ptr(sec_log_buf_match_table),
	},
	.probe = sec_log_buf_probe,
	.remove = sec_log_buf_remove,
};

static int __init sec_log_buf_init(void)
{
	int err;

	err = platform_driver_register(&sec_log_buf_driver);
	if (err)
		return err;

	err = __of_platform_early_populate_init(sec_log_buf_match_table);
	if (err)
		return err;

	return 0;
}
#if IS_BUILTIN(CONFIG_SEC_LOG_BUF)
core_initcall_sync(sec_log_buf_init);
#else
module_init(sec_log_buf_init);
#endif

static void __exit sec_log_buf_exit(void)
{
	platform_driver_unregister(&sec_log_buf_driver);
}
module_exit(sec_log_buf_exit);

MODULE_AUTHOR("Samsung Electronics");
MODULE_DESCRIPTION("Kernel log buffer shared with boot loader");
MODULE_LICENSE("GPL v2");
