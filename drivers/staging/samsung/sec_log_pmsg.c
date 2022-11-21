#include <linux/kernel.h>
#include <linux/sched.h>
#include <linux/module.h>
#include <linux/memblock.h>
#include <linux/vmalloc.h>
#include <linux/platform_device.h>
#include <linux/pstore_ram.h>
#include <linux/sec_bsp.h>

static char *sec_pmsg_curr_ptr;
static char *sec_pmsg_buf;
static unsigned sec_pmsg_size;

static char *sec_pstore_buf;
static unsigned sec_pstore_size;

static void register_hook_logger(void (*func)(const char *buf, size_t size));

static inline int exynos_ss_check_eob(size_t size)
{
	size_t max, cur;

	max = (size_t)(sec_pmsg_buf + sec_pmsg_size);
	cur = (size_t)(sec_pmsg_curr_ptr + size);

	if (unlikely(cur > max))
		return -1;
	else
		return 0;
}

static void sec_pmsg_logger(const char *buf, size_t size)
{
	if (unlikely(!sec_pmsg_buf))
		return;

	if (unlikely(exynos_ss_check_eob(size)))
		sec_pmsg_curr_ptr = sec_pmsg_buf;

	memcpy(sec_pmsg_curr_ptr, buf, size);
	sec_pmsg_curr_ptr += size;
}

static int __init sec_log_pmsg_setup(char *str)
{
	unsigned size = memparse(str, &str);
	unsigned long base = 0;

	/* If we encounter any problem parsing str ... */
	if (!size || size != roundup_pow_of_two(size) || *str != '@'
	    || kstrtoul(str + 1, 0, &base))
		goto out;

#if 0/*def CONFIG_SEC_LOG_NONCACHED*/
	log_buf_iodesc[0].pfn = __phys_to_pfn((unsigned long)base - 0x100000);
	log_buf_iodesc[0].length = (unsigned long)(size + 0x100000);
	iotable_init(log_buf_iodesc, ARRAY_SIZE(log_buf_iodesc));
	flush_tlb_all();
	sec_log_mag = (S3C_VA_KLOG_BUF + 0x100000) - 8;
	sec_log_ptr = (S3C_VA_KLOG_BUF + 0x100000) - 4;
	sec_log_buf = S3C_VA_KLOG_BUF + 0x100000;
#else
	sec_pmsg_buf = phys_to_virt(base);
#endif
	sec_pmsg_curr_ptr = sec_pmsg_buf;
	sec_pmsg_size = size;
	pr_err("%s: sec_log_pmsg_buf:%p sec_log_pmsg_size:%d\n",
		__func__, sec_pmsg_buf, sec_pmsg_size);

	memset((size_t *)sec_pmsg_buf, 0, sec_pmsg_size);

out:
	return 0;
}
__setup("sec_log_pmsg=", sec_log_pmsg_setup);

static int __init sec_log_pmsg_early_setup(char *str)
{
	unsigned size = memparse(str, &str);
	unsigned long base = 0;

	/* If we encounter any problem parsing str ... */
	if (!size || size != roundup_pow_of_two(size) || *str != '@'
	    || kstrtoul(str + 1, 0, &base))
		goto out;

	if (memblock_reserve(base - 8, size + 8)) {
		pr_err("%s: failed reserving size %d + 8 at base 0x%lx - 8\n",
			__func__, size, base);
	}
out:
	return 0;
}
early_param("sec_log_pmsg", sec_log_pmsg_early_setup);

static int __init sec_pstore_setup(char *str)
{
	unsigned size = memparse(str, &str);
	unsigned long base = 0;

	/* If we encounter any problem parsing str ... */
	if (!size || size != roundup_pow_of_two(size) || *str != '@'
		|| kstrtoul(str + 1, 0, &base)) {
			goto out;
	}

	sec_pstore_buf = phys_to_virt(base);
	sec_pstore_size = size;

	pr_info("%s: sec_pstore_buf:%p sec_pstore_size:0x%x\n",
		__func__, sec_pstore_buf, sec_pstore_size);

out:
	return 0;
}
__setup("sec_pstore=", sec_pstore_setup);

static int __init sec_pstore_early_setup(char *str)
{
	unsigned size = memparse(str, &str);
	unsigned long base = 0;

	/* If we encounter any problem parsing str ... */
	if (!size || size != roundup_pow_of_two(size) || *str != '@'
		|| kstrtoul(str + 1, 0, &base)) {
			goto out;
	}

	if (memblock_reserve(base - 8, size + 8)) {
			pr_err("%s: failed reserving size %d at base 0x%lx\n",
					__func__, size, base);
			goto out;
	}
out:
	return 0;
}
early_param("sec_pstore", sec_pstore_early_setup);

static int __init sec_log_pmsg_init(void)
{
	if (sec_pmsg_buf)
		register_hook_logger(sec_pmsg_logger);

	return 0;
}
early_initcall(sec_log_pmsg_init);

/* This defines are for PSTORE */
#define ESS_LOGGER_LEVEL_HEADER		(1)
#define ESS_LOGGER_LEVEL_PREFIX		(2)
#define ESS_LOGGER_LEVEL_TEXT		(3)
#define ESS_LOGGER_LEVEL_MAX		(4)
#define ESS_LOGGER_SKIP_COUNT		(4)
#define ESS_LOGGER_STRING_PAD		(1)
#define ESS_LOGGER_HEADER_SIZE		(68)

#define ESS_LOG_ID_MAIN			(0)
#define ESS_LOG_ID_RADIO		(1)
#define ESS_LOG_ID_EVENTS		(2)
#define ESS_LOG_ID_SYSTEM		(3)
#define ESS_LOG_ID_CRASH		(4)
#define ESS_LOG_ID_KERNEL		(5)

typedef struct __attribute__((__packed__)) {
	uint8_t magic;
	uint16_t len;
	uint16_t uid;
	uint16_t pid;
} sec_pmsg_log_header_t;

typedef struct __attribute__((__packed__)) {
	unsigned char id;
	uint16_t tid;
	int32_t tv_sec;
	int32_t tv_nsec;
} sec_android_log_header_t;

typedef struct ess_logger {
	uint16_t	len;
	uint16_t	id;
	uint16_t	pid;
	uint16_t	tid;
	uint16_t	uid;
	uint16_t	level;
	int32_t		tv_sec;
	int32_t		tv_nsec;
	char		msg[1];
	char		*buffer;
	void		(*func_hook_logger)(const char*, size_t);
} __attribute__((__packed__)) sec_logger;

static sec_logger logger;

static void register_hook_logger(void (*func)(const char *buf, size_t size))
{
	logger.func_hook_logger = func;
	logger.buffer = vmalloc(PAGE_SIZE * 3);

	if (logger.buffer)
		pr_info("exynos-pstore: logger buffer alloc address: 0x%p\n",
				logger.buffer);
}

static inline void sec_log_write_into_klog(char *buffer, size_t count)
{
	buffer[count-1] = '\0';
	pr_info("%s\n", buffer);

#ifdef CONFIG_SEC_BSP
	if (count > 5 && strncmp(buffer, "!@Boot", 6) == 0)
		sec_boot_stat_add(buffer);
#endif /* CONFIG_SEC_BSP */
}

static void sec_log_find_marker(char *buffer, size_t count)
{
	char *eatnl = buffer + count - ESS_LOGGER_STRING_PAD;

	if (logger.id == ESS_LOG_ID_EVENTS)
		return;

	if (count == ESS_LOGGER_SKIP_COUNT && *eatnl != '\0')
		return;

	if (count > 1 && strncmp(buffer, "!@", 2) == 0)
		sec_log_write_into_klog(buffer, count);
}

static int sec_log_combine_pmsg(char *buffer, size_t count, unsigned int level)
{
	char *logbuf = logger.buffer;

	if (!logbuf) {
		if (level == ESS_LOGGER_LEVEL_TEXT)
			sec_log_find_marker(buffer, count);

		return -ENOMEM;
	}

	switch (level) {
	case ESS_LOGGER_LEVEL_HEADER:
		{
			struct tm tmbuf;
			u64 tv_kernel;
			unsigned int logbuf_len;
			unsigned long rem_nsec;

			if (logger.id == ESS_LOG_ID_EVENTS)
				break;

			tv_kernel = local_clock();
			rem_nsec = do_div(tv_kernel, 1000000000);
			time_to_tm(logger.tv_sec, 0, &tmbuf);

			logbuf_len = snprintf(logbuf, ESS_LOGGER_HEADER_SIZE,
					"\n[%5lu.%06lu][%d:%16s] %02d-%02d %02d:%02d:%02d.%03d %5d %5d  ",
					(unsigned long)tv_kernel, rem_nsec / 1000,
					raw_smp_processor_id(), current->comm,
					tmbuf.tm_mon + 1, tmbuf.tm_mday,
					tmbuf.tm_hour, tmbuf.tm_min, tmbuf.tm_sec,
					logger.tv_nsec / 1000000, logger.pid, logger.tid);

			logger.func_hook_logger(logbuf, logbuf_len - 1);
		}
		break;
	case ESS_LOGGER_LEVEL_PREFIX:
		{
			static const char *kPrioChars = "!.VDIWEFS";
			unsigned char prio = logger.msg[0];

			if (logger.id == ESS_LOG_ID_EVENTS)
				break;

			logbuf[0] = prio < strlen(kPrioChars) ? kPrioChars[prio] : '?';
			logbuf[1] = ' ';

			logger.func_hook_logger(logbuf, ESS_LOGGER_LEVEL_PREFIX);
		}
		break;
	case ESS_LOGGER_LEVEL_TEXT:
		{
			char *eatnl = buffer + count - ESS_LOGGER_STRING_PAD;

			if (logger.id == ESS_LOG_ID_EVENTS)
				break;
			if (count == ESS_LOGGER_SKIP_COUNT && *eatnl != '\0')
				break;

			logger.func_hook_logger(buffer, count - 1);

			if (count > 1 && strncmp(buffer, "!@", 2) == 0)
				sec_log_write_into_klog(buffer, count);
		}
		break;
	default:
		break;
	}
	return 0;
}

int sec_log_hook_pmsg(char *buffer, size_t count)
{
	sec_android_log_header_t header;
	sec_pmsg_log_header_t pmsg_header;

	switch (count) {
	case sizeof(pmsg_header):
		memcpy((void *)&pmsg_header, buffer, count);
		if (pmsg_header.magic != 'l') {
			sec_log_combine_pmsg(buffer, count, ESS_LOGGER_LEVEL_TEXT);
		} else {
			/* save logger data */
			logger.pid = pmsg_header.pid;
			logger.uid = pmsg_header.uid;
			logger.len = pmsg_header.len;
		}
		break;
	case sizeof(header):
		/* save logger data */
		memcpy((void *)&header, buffer, count);
		logger.id = header.id;
		logger.tid = header.tid;
		logger.tv_sec = header.tv_sec;
		logger.tv_nsec  = header.tv_nsec;
		if (logger.id < 0 || logger.id > 7) {
			/* write string */
			sec_log_combine_pmsg(buffer, count, ESS_LOGGER_LEVEL_TEXT);
		} else {
			/* write header */
			sec_log_combine_pmsg(buffer, count, ESS_LOGGER_LEVEL_HEADER);
		}
		break;
	case sizeof(unsigned char):
		logger.msg[0] = buffer[0];
		/* write char for prefix */
		sec_log_combine_pmsg(buffer, count, ESS_LOGGER_LEVEL_PREFIX);
		break;
	default:
		/* write string */
		sec_log_combine_pmsg(buffer, count, ESS_LOGGER_LEVEL_TEXT);
		break;
	}

	return 0;
}
EXPORT_SYMBOL(sec_log_hook_pmsg);

/*
 *  To support pstore/pmsg/pstore_ram, following is implementation for exynos-snapshot
 *  ess_ramoops platform_device is used by pstore fs.
 */

static struct ramoops_platform_data sec_log_ramoops_data = {
	.record_size	= SZ_512K,
//	.console_size	= SZ_512K,
//	.ftrace_size	= SZ_512K,
	.pmsg_size	= SZ_512K,
	.dump_oops	= 1,
};

static struct platform_device sec_log_ramoops = {
	.name = "ramoops",
	.dev = {
		.platform_data = &sec_log_ramoops_data,
	},
};

static int __init sec_log_pstore_init(void)
{
	if (sec_pstore_buf) {
		sec_log_ramoops_data.mem_size = sec_pstore_size;
		sec_log_ramoops_data.mem_address = virt_to_phys(sec_pstore_buf);
	}
	return platform_device_register(&sec_log_ramoops);
}

static void __exit sec_log_pstore_exit(void)
{
	platform_device_unregister(&sec_log_ramoops);
}
module_init(sec_log_pstore_init);
module_exit(sec_log_pstore_exit);

MODULE_DESCRIPTION("Exynos Snapshot pstore module");
MODULE_LICENSE("GPL");