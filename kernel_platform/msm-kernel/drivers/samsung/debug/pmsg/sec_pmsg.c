// SPDX-License-Identifier: GPL-2.0-only
/*
 * COPYRIGHT(C) 2016-2021 Samsung Electronics Co., Ltd. All Right Reserved.
 */

#define pr_fmt(fmt)     KBUILD_MODNAME ":%s() " fmt, __func__

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/mutex.h>
#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/of_reserved_mem.h>
#include <linux/platform_device.h>
#include <linux/pstore.h>
#include <linux/sched/clock.h>
#include <linux/slab.h>
#include <linux/time.h>
#include <linux/uaccess.h>

#include <linux/samsung/builder_pattern.h>
#include <linux/samsung/debug/sec_boot_stat.h>
#include <linux/samsung/debug/sec_debug.h>

/* This defines are for PSTORE */
#define SS_LOGGER_LEVEL_HEADER		(1)
#define SS_LOGGER_LEVEL_PREFIX		(2)
#define SS_LOGGER_LEVEL_TEXT		(3)
#define SS_LOGGER_LEVEL_MAX		(4)
#define SS_LOGGER_SKIP_COUNT		(4)
#define SS_LOGGER_STRING_PAD		(1)
#define SS_LOGGER_HEADER_SIZE		(80)

#define SS_LOG_ID_MAIN			(0)
#define SS_LOG_ID_RADIO			(1)
#define SS_LOG_ID_EVENTS		(2)
#define SS_LOG_ID_SYSTEM		(3)
#define SS_LOG_ID_CRASH			(4)
#define SS_LOG_ID_KERNEL		(5)

#define MAX_BUFFER_SIZE	1024

struct ss_pmsg_log_header_t {
	uint8_t magic;
	uint16_t len;
	uint16_t uid;
	uint16_t pid;
} __attribute__((__packed__));

struct ss_android_log_header_t {
	unsigned char id;
	uint16_t tid;
	int32_t tv_sec;
	int32_t tv_nsec;
} __attribute__((__packed__));

struct pmsg_logger;

struct pmsg_buffer {
	char buffer[MAX_BUFFER_SIZE];
};

struct pmsg_drvdata {
	struct builder bd;
	struct reserved_mem *rmem;
	phys_addr_t paddr;
	size_t size;
	struct pstore_info *pstore;
	struct pmsg_logger *logger;
	struct pmsg_buffer __percpu *buf;
};

struct pmsg_drvdata *sec_pmsg __read_mostly;

static char *pmsg_buf __read_mostly;
static size_t pmsg_size __read_mostly;
static size_t pmsg_idx;

static void (*__pmsg_memcpy_toio)(void *, const void *, size_t) __read_mostly;

static void notrace ____pmsg_memcpy_toio(void *dst, const void *src, size_t cnt)
{
	memcpy_toio(dst, src, cnt);
}

static void notrace ____pmsg_memcpy(void *dst, const void *src, size_t cnt)
{
	memcpy(dst, src, cnt);
}

static inline void __pmsg_logger(const char *buf, size_t size)
{
	size_t f_len, s_len, remain_space;
	size_t idx;

	idx = pmsg_idx % pmsg_size;
	remain_space = pmsg_size - idx;
	f_len = min(size, remain_space);
	__pmsg_memcpy_toio(&(pmsg_buf[idx]), buf, f_len);

	s_len = size - f_len;
	if (unlikely(s_len))
		__pmsg_memcpy_toio(pmsg_buf, &buf[f_len], s_len);

	pmsg_idx += size;
}

struct pmsg_logger {
	uint16_t len;
	uint16_t id;
	uint16_t pid;
	uint16_t tid;
	uint16_t uid;
	uint16_t level;
	int32_t tv_sec;
	int32_t tv_nsec;
	char msg[0];
};

static inline void __logger_level_header(struct pmsg_logger *logger,
		char *buffer, size_t count)
{
	struct tm tm_buf;
	u64 tv_kernel;
	int buffer_len;
	u64 rem_nsec;

	if (IS_ENABLED(CONFIG_SEC_PMSG_USE_EVENT_LOG)
			&& logger->id == SS_LOG_ID_EVENTS)
		return;

	tv_kernel = local_clock();
	rem_nsec = do_div(tv_kernel, 1000000000);
	time64_to_tm(logger->tv_sec, 0, &tm_buf);

	buffer_len = scnprintf(buffer, SS_LOGGER_HEADER_SIZE,
			"\n[%5llu.%06llu][%d:%16s] %02d-%02d "
			"%02d:%02d:%02d.%03d %5d %5d  ",
			(unsigned long long)tv_kernel,
			(unsigned long long)rem_nsec / 1000,
			raw_smp_processor_id(), current->comm,
			tm_buf.tm_mon + 1, tm_buf.tm_mday,
			tm_buf.tm_hour, tm_buf.tm_min,
			tm_buf.tm_sec, logger->tv_nsec / 1000000,
			logger->pid, logger->tid);

	__pmsg_logger(buffer, buffer_len - 1);
}

static inline void __logger_level_prefix(struct pmsg_logger *logger,
		char *buffer, size_t count)
{
	const char *prio_magic = "!.VDIWEFS";
	const size_t prio_magic_len = 9;	// == strlen(prio_magic);
	size_t prio = (size_t)logger->msg[0];

	if (IS_ENABLED(CONFIG_SEC_PMSG_USE_EVENT_LOG) &&
			logger->id == SS_LOG_ID_EVENTS)
		return;

	buffer[0] = prio < prio_magic_len ? prio_magic[prio] : '?';

	if (IS_ENABLED(CONFIG_SEC_PMSG_USE_EVENT_LOG))
		logger->msg[0] = 0xff;

	__pmsg_logger(buffer, 1);
}

static inline void __ss_logger_level_text_event_log(struct pmsg_logger *logger,
		char *buffer, size_t count)
{
	/* TODO: CONFIG_SEC_PMSG_USE_EVENT_LOG (CONFIG_SEC_EVENT_LOG in
	 * a legacy implementation) is never used, yet.
	 * It's maybe deprecated and I'll implement it if it is required.
	 */
}

static inline void ____logger_level_text(struct pmsg_logger *logger,
		char *buffer, size_t count)
{
	char *eatnl = &buffer[count - SS_LOGGER_STRING_PAD];

	buffer[count - 1] = '\0';

	if (count == SS_LOGGER_SKIP_COUNT && *eatnl != '\0')
		return;

	if (count > 1 && *(uint16_t*)buffer == *(uint16_t *)"!@") {
		/* To prevent potential buffer overrun
		 * put a null at the end of the buffer if required
		 */
		if (buffer[count - 1] != '\0')
			buffer[count - 1] = '\0';

		/* FIXME: print without a module and a function name */
		printk(KERN_INFO "%s\n", buffer);

		sec_boot_stat_add(buffer);
	}

	__pmsg_logger(buffer, count - 1);
}

static inline void __logger_level_text(struct pmsg_logger *logger,
		char *buffer, size_t count)
{
	if (unlikely(logger->id == SS_LOG_ID_EVENTS)) {
		__ss_logger_level_text_event_log(logger, buffer, count);
		return;
	}

	____logger_level_text(logger, buffer, count);
}

static inline int __logger_combine_pmsg(struct pmsg_logger *logger,
		char *buffer, size_t count, unsigned int level)
{
	switch (level) {
	case SS_LOGGER_LEVEL_HEADER:
		__logger_level_header(logger, buffer, count);
		break;
	case SS_LOGGER_LEVEL_PREFIX:
		__logger_level_prefix(logger, buffer, count);
		break;
	case SS_LOGGER_LEVEL_TEXT:
		__logger_level_text(logger, buffer, count);
		break;
	default:
		pr_warn("unknown logger level : %u\n", level);
		break;
	}

	__pmsg_logger(" ", 1);

	return 0;
}

static __always_inline void __logger_write_user_pmsg_log_header(
		struct pmsg_logger *logger, char *buffer, size_t count)
{
	struct ss_pmsg_log_header_t *pmsg_header =
			(struct ss_pmsg_log_header_t *)buffer;

	if (pmsg_header->magic != 'l') {
		__logger_combine_pmsg(logger, buffer, count, SS_LOGGER_LEVEL_TEXT);
	} else {
		logger->pid = pmsg_header->pid;
		logger->uid = pmsg_header->uid;
		logger->len = pmsg_header->len;
	}
}

static __always_inline void __logger_write_user_android_log_header(
		struct pmsg_logger *logger, char *buffer, size_t count)
{
	struct ss_android_log_header_t *header =
			(struct ss_android_log_header_t *)buffer;

	logger->id = header->id;
	logger->tid = header->tid;
	logger->tv_sec = header->tv_sec;
	logger->tv_nsec  = header->tv_nsec;

	if (logger->id > 7)
		__logger_combine_pmsg(logger, buffer, count, SS_LOGGER_LEVEL_TEXT);
	else
		__logger_combine_pmsg(logger, buffer, count, SS_LOGGER_LEVEL_HEADER);
}

static __always_inline int __pmsg_write_user(struct pstore_record *record,
		const char __user *buf, size_t count)
{
	struct pmsg_drvdata *drvdata = record->psi->data;
	struct pmsg_logger *logger = drvdata->logger;
	char *big_buffer = NULL;
	char *buffer;
	int err;

	if (unlikely(count > MAX_BUFFER_SIZE)) {
		big_buffer = kmalloc(count, GFP_KERNEL);
		if (unlikely(!big_buffer))
			return -ENOMEM;

		buffer = big_buffer;
	} else {
		struct pmsg_buffer *buf =
				per_cpu_ptr(drvdata->buf, raw_smp_processor_id());
		buffer = &buf->buffer[0];
	}

	err = __copy_from_user(buffer, buf, count);
	if (unlikely(err))
		return -EFAULT;

	switch (count) {
	case sizeof(struct ss_pmsg_log_header_t):
		__logger_write_user_pmsg_log_header(logger, buffer, count);
		break;
	case sizeof(struct ss_android_log_header_t):
		__logger_write_user_android_log_header(logger, buffer, count);
		break;
	case sizeof(unsigned char):
		logger->msg[0] = buffer[0];
		__logger_combine_pmsg(logger, buffer, count, SS_LOGGER_LEVEL_PREFIX);
		break;
	default:
		__logger_combine_pmsg(logger, buffer, count, SS_LOGGER_LEVEL_TEXT);
		break;
	}

	kfree(big_buffer);

	return 0;
}

static int notrace sec_pmsg_write_user(struct pstore_record *record,
		const char __user *buf)
{
	if (unlikely(record->type != PSTORE_TYPE_PMSG))
		return -EINVAL;

	return __pmsg_write_user(record, buf, record->size);
}

static ssize_t notrace sec_pmsg_read(struct pstore_record *record)
{
	/* FIXME: I don't do anything. */
	return 0;
}

static int notrace sec_pmsg_write(struct pstore_record *record)
{
	/* FIXME: I don't do anything. */
	return 0;
}

static struct pstore_info sec_pmsg_pstore = {
	.owner = THIS_MODULE,
	.name = "samsung,pstore_pmsg",
	.read = sec_pmsg_read,
	.write = sec_pmsg_write,
	.write_user = sec_pmsg_write_user,
	.flags = PSTORE_FLAGS_PMSG,
};

static int __pmsg_parse_dt_memory_region(struct builder *bd,
		struct device_node *np)
{
	struct pmsg_drvdata *drvdata =
			container_of(bd, struct pmsg_drvdata, bd);
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

static int __pmsg_parse_dt_lease_region(struct builder *bd,
		struct device_node *np)
{
	struct pmsg_drvdata *drvdata =
			container_of(bd, struct pmsg_drvdata, bd);
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

static int __pmsg_parse_dt_test_no_map(struct builder *bd,
		struct device_node *np)
{
	struct pmsg_drvdata *drvdata =
			container_of(bd, struct pmsg_drvdata, bd);
	struct device_node *mem_np;

	mem_np = of_parse_phandle(np, "memory-region", 0);
	if (!mem_np)
		return -EINVAL;

	if (!of_property_read_bool(mem_np, "no-map")) {
		pmsg_buf = phys_to_virt(drvdata->paddr);
		__pmsg_memcpy_toio = ____pmsg_memcpy;
	} else {
		__pmsg_memcpy_toio = ____pmsg_memcpy_toio;
	}

	return 0;
}

static struct dt_builder __pmsg_dt_builder[] = {
	DT_BUILDER(__pmsg_parse_dt_memory_region),
	DT_BUILDER(__pmsg_parse_dt_lease_region),
	DT_BUILDER(__pmsg_parse_dt_test_no_map),
};

static int __pmsg_parse_dt(struct builder *bd)
{
	return sec_director_parse_dt(bd, __pmsg_dt_builder,
			ARRAY_SIZE(__pmsg_dt_builder));
}

static int __pmsg_prepare_logger(struct builder *bd)
{
	struct pmsg_drvdata *drvdata =
			container_of(bd, struct pmsg_drvdata, bd);
	struct device *dev = bd->dev;
	struct pmsg_logger *logger;

	logger = devm_kmalloc(dev, sizeof(*drvdata->logger), GFP_KERNEL);
	if (!logger)
		return -ENOMEM;

	drvdata->logger = logger;

	return 0;
}

static int __pmsg_prepare_buffer(struct builder *bd)
{
	struct pmsg_drvdata *drvdata =
			container_of(bd, struct pmsg_drvdata, bd);
	struct device *dev = bd->dev;
	struct pmsg_buffer *buf;

	buf = devm_alloc_percpu(dev, struct pmsg_buffer);
	if (!buf)
		return -ENOMEM;

	drvdata->buf = buf;

	return 0;
}

static void *__pmsg_ioremap(struct pmsg_drvdata *drvdata)
{
	struct device *dev = drvdata->bd.dev;

	if (pmsg_buf)
		return pmsg_buf;

	return devm_ioremap(dev, drvdata->paddr, drvdata->size);
}

static int __pmsg_prepare_carveout(struct builder *bd)
{
	struct pmsg_drvdata *drvdata =
			container_of(bd, struct pmsg_drvdata, bd);

	pmsg_buf = __pmsg_ioremap(drvdata);
	if (!pmsg_buf)
		return -EFAULT;

	pmsg_size = drvdata->size;
	pmsg_idx = 0;

	return 0;
}

static int __pmsg_pstore_register(struct builder *bd)
{
	struct pmsg_drvdata *drvdata =
			container_of(bd, struct pmsg_drvdata, bd);

	sec_pmsg_pstore.data = drvdata;
	drvdata->pstore= &sec_pmsg_pstore;

	return pstore_register(drvdata->pstore);
}

static void __pmsg_pstore_unregister(struct builder *bd)
{
	struct pmsg_drvdata *drvdata =
			container_of(bd, struct pmsg_drvdata, bd);

	sec_pmsg_pstore.data = NULL;

	pstore_unregister(drvdata->pstore);
}

static int __pmsg_probe_epilog(struct builder *bd)
{
	struct pmsg_drvdata *drvdata =
			container_of(bd, struct pmsg_drvdata, bd);
	struct device *dev = bd->dev;

	dev_set_drvdata(dev, drvdata);
	sec_pmsg = drvdata;

	return 0;
}

static void __pmsg_remove_prolog(struct builder *bd)
{
	sec_pmsg = NULL;
}

static int __pmsg_probe(struct platform_device *pdev,
		struct dev_builder *builder, ssize_t n)
{
	struct device *dev = &pdev->dev;
	struct pmsg_drvdata *drvdata;

	drvdata = devm_kzalloc(dev, sizeof(*drvdata), GFP_KERNEL);
	if (!drvdata)
		return -ENOMEM;

	drvdata->bd.dev = dev;

	return sec_director_probe_dev(&drvdata->bd, builder, n);
}

static int __pmsg_remove(struct platform_device *pdev,
		struct dev_builder *builder, ssize_t n)
{
	struct pmsg_drvdata *drvdata = platform_get_drvdata(pdev);

	sec_director_destruct_dev(&drvdata->bd, builder, n, n);

	return 0;
}

static struct dev_builder __pmsg_dev_builder[] = {
	DEVICE_BUILDER(__pmsg_parse_dt, NULL),
	DEVICE_BUILDER(__pmsg_prepare_logger, NULL),
	DEVICE_BUILDER(__pmsg_prepare_buffer, NULL),
	DEVICE_BUILDER(__pmsg_prepare_carveout, NULL),
	DEVICE_BUILDER(__pmsg_pstore_register, __pmsg_pstore_unregister),
	DEVICE_BUILDER(__pmsg_probe_epilog, __pmsg_remove_prolog),
};

static int sec_pmsg_probe(struct platform_device *pdev)
{
	return __pmsg_probe(pdev, __pmsg_dev_builder,
			ARRAY_SIZE(__pmsg_dev_builder));
}

static int sec_pmsg_remove(struct platform_device *pdev)
{
	return __pmsg_remove(pdev, __pmsg_dev_builder,
			ARRAY_SIZE(__pmsg_dev_builder));
}

static const struct of_device_id sec_pmsg_match_table[] = {
	{ .compatible = "samsung,pstore_pmsg" },
	{},
};
MODULE_DEVICE_TABLE(of, sec_pmsg_match_table);

static struct platform_driver sec_pmsg_driver = {
	.driver = {
		.name = "samsung,pstore_pmsg",
		.of_match_table = of_match_ptr(sec_pmsg_match_table),
	},
	.probe = sec_pmsg_probe,
	.remove = sec_pmsg_remove,
};

static int __init sec_pmsg_init(void)
{
	return platform_driver_register(&sec_pmsg_driver);
}
module_init(sec_pmsg_init);

static void __exit sec_pmsg_exit(void)
{
	platform_driver_unregister(&sec_pmsg_driver);
}
module_exit(sec_pmsg_exit);

MODULE_AUTHOR("Samsung Electronics");
MODULE_DESCRIPTION("PSTORE backend for saving android platform log");
MODULE_LICENSE("GPL v2");
