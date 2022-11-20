/* 
 * ss_platform_log.c
 *
 * Copyright (C) 2016 Samsung Electronics
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307 USA
 *
 */

#define pr_fmt(fmt) KBUILD_MODNAME ": " fmt

#include <linux/types.h>
#include <linux/errno.h>
#include <linux/time.h>
#include <linux/sched.h>
#include <linux/io.h>
#include <linux/platform_device.h>
#include <linux/module.h>
#include <linux/vmalloc.h>

#ifdef CONFIG_SEC_BSP
#include <linux/sec_bsp.h>
#endif

/* This defines are for PSTORE */
#define SS_LOGGER_LEVEL_HEADER 	(1)
#define SS_LOGGER_LEVEL_PREFIX 	(2)
#define SS_LOGGER_LEVEL_TEXT		(3)
#define SS_LOGGER_LEVEL_MAX		(4)
#define SS_LOGGER_SKIP_COUNT		(4)
#define SS_LOGGER_STRING_PAD		(1)
#define SS_LOGGER_HEADER_SIZE		(68)

#define SS_LOG_ID_MAIN 		(0)
#define SS_LOG_ID_RADIO		(1)
#define SS_LOG_ID_EVENTS		(2)
#define SS_LOG_ID_SYSTEM		(3)
#define SS_LOG_ID_CRASH		(4)
#define SS_LOG_ID_KERNEL		(5)

typedef struct __attribute__((__packed__)) {
	uint8_t magic;
	uint16_t len;
	uint16_t uid;
	uint16_t pid;
} ss_pmsg_log_header_t;

typedef struct __attribute__((__packed__)) {
	unsigned char id;
	uint16_t tid;
	int32_t tv_sec;
	int32_t tv_nsec;
} ss_android_log_header_t;

typedef struct ss_logger {
	uint16_t	len;
	uint16_t	id;
	uint16_t	pid;
	uint16_t	tid;
	uint16_t	uid;
	uint16_t	level;
	int32_t		tv_sec;
	int32_t		tv_nsec;
	char		msg[0];
	char*		buffer;
	void		(*func_hook_logger)(const char*, size_t);
} __attribute__((__packed__)) ss_logger;

static ss_logger logger;

static int ss_combine_pmsg(char *buffer, size_t count, unsigned int level)
{
	char *logbuf = logger.buffer;
	if (!logbuf)
		return -ENOMEM;

	switch(level) {
	case SS_LOGGER_LEVEL_HEADER:
		{
			struct tm tmBuf;
			u64 tv_kernel;
			unsigned int logbuf_len;
			unsigned long rem_nsec;

			if (logger.id == SS_LOG_ID_EVENTS)
				break;

			tv_kernel = local_clock();
			rem_nsec = do_div(tv_kernel, 1000000000);
			time_to_tm(logger.tv_sec, 0, &tmBuf);

			logbuf_len = snprintf(logbuf, SS_LOGGER_HEADER_SIZE,
					"\n[%5lu.%06lu][%d:%16s] %02d-%02d %02d:%02d:%02d.%03d %5d %5d  ",
					(unsigned long)tv_kernel, rem_nsec / 1000,
					raw_smp_processor_id(), current->comm,
					tmBuf.tm_mon + 1, tmBuf.tm_mday,
					tmBuf.tm_hour, tmBuf.tm_min, tmBuf.tm_sec,
					logger.tv_nsec / 1000000, logger.pid, logger.tid);

			logger.func_hook_logger(logbuf, logbuf_len - 1);
		}
		break;
	case SS_LOGGER_LEVEL_PREFIX:
		{
			static const char* kPrioChars = "!.VDIWEFS";
			unsigned char prio = logger.msg[0];

			if (logger.id == SS_LOG_ID_EVENTS)
				break;

			logbuf[0] = prio < strlen(kPrioChars) ? kPrioChars[prio] : '?';
			logbuf[1] = ' ';

			logger.func_hook_logger(logbuf, SS_LOGGER_LEVEL_PREFIX);
		}
		break;
	case SS_LOGGER_LEVEL_TEXT:
		{
			char *eatnl = buffer + count - SS_LOGGER_STRING_PAD;

			if (logger.id == SS_LOG_ID_EVENTS)
				break;
			if (count == SS_LOGGER_SKIP_COUNT && *eatnl != '\0')
				break;

			if (count > 1 && strncmp(buffer, "!@", 2) == 0) {
				pr_info("%s\n", buffer);
#ifdef CONFIG_SEC_BSP
				if (count > 5 && strncmp(buffer, "!@Boot", 6) == 0)
					sec_boot_stat_add(buffer);
#endif
			}
			logger.func_hook_logger(buffer, count - 1);
		}
		break;
	default:
		break;
	}
	return 0;
}

int ss_hook_pmsg(char *buffer, size_t count)
{
	ss_android_log_header_t header;
	ss_pmsg_log_header_t pmsg_header;
	
	if (!logger.buffer)
		return -ENOMEM;
	
	switch(count) {
	case sizeof(pmsg_header):
		memcpy((void *)&pmsg_header, buffer, count);
		if (pmsg_header.magic != 'l') {
			ss_combine_pmsg(buffer, count, SS_LOGGER_LEVEL_TEXT);
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
		if (logger.id > 7) {
			/* write string */
			ss_combine_pmsg(buffer, count, SS_LOGGER_LEVEL_TEXT);
		} else {
			/* write header */
			ss_combine_pmsg(buffer, count, SS_LOGGER_LEVEL_HEADER);
		}
		break;
	case sizeof(unsigned char):
		logger.msg[0] = buffer[0];
		/* write char for prefix */
		ss_combine_pmsg(buffer, count, SS_LOGGER_LEVEL_PREFIX);
		break;
	default:
		/* write string */
		ss_combine_pmsg(buffer, count, SS_LOGGER_LEVEL_TEXT);
		break;
	}

	return 0;
}
EXPORT_SYMBOL(ss_hook_pmsg);

static ulong mem_address;
module_param(mem_address, ulong, 0400);
MODULE_PARM_DESC(mem_address,
		"start of reserved RAM used to store platform log");

static ulong mem_size;
module_param(mem_size, ulong, 0400);
MODULE_PARM_DESC(mem_size,
		"size of reserved RAM used to store platform log");


static char *platform_log_buf;
static unsigned platform_log_idx;
static inline void emit_sec_platform_log_char(char c)
{
	platform_log_buf[platform_log_idx & (mem_size - 1)] = c;
	platform_log_idx++;
}

static inline void ss_hook_logger(
					 const char *buf, size_t size)
{
	int i;

	for (i = 0; i < size ; i++)
		emit_sec_platform_log_char(buf[i]);
}

struct ss_plog_platform_data {
	unsigned long mem_size;
	unsigned long mem_address;
};

static int ss_plog_parse_dt(struct platform_device *pdev,
		struct ss_plog_platform_data *pdata)
{
	struct resource *res;

	dev_dbg(&pdev->dev, "using Device Tree\n");

	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if (!res) {
		dev_err(&pdev->dev,
			"failed to locate DT /reserved-memory resource\n");
		return -EINVAL;
	}

	pdata->mem_size = resource_size(res);
	pdata->mem_address = res->start;

	return 0;
}

static int ss_plog_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct ss_plog_platform_data *pdata = dev->platform_data;
	void __iomem *va = 0;
	int err = -EINVAL;

	if (dev->of_node && !pdata) {
		pdata = devm_kzalloc(&pdev->dev, sizeof(*pdata), GFP_KERNEL);
		if (!pdata) {
			err = -ENOMEM;
			goto fail_out;
		}

		err = ss_plog_parse_dt(pdev, pdata);
		if (err < 0)
			goto fail_out;
	}

	if (!pdata->mem_size) { 
		pr_err("The memory size must be non-zero\n");
		err = -ENOMEM;
		goto fail_out;
	}

	mem_size = pdata->mem_size;
	mem_address = pdata->mem_address;

	pr_info("attached 0x%lx@0x%llx\n",mem_size, (unsigned long long)mem_address);

	platform_set_drvdata(pdev, pdata);


	if(!request_mem_region(mem_address,mem_size,"ss_plog")) {
		pr_err("request mem region (0x%llx@0x%llx) failed\n",
			(unsigned long long)mem_size, (unsigned long long)mem_address);
		goto fail_out;
	}

	va = ioremap_nocache((phys_addr_t)(mem_address), mem_size);
	if (!va) {
		pr_err("Failed to remap plaform log region\n");
		err = -ENOMEM;
		goto fail_out;
	}

	platform_log_buf = va;
	platform_log_idx = 0;
	
	logger.func_hook_logger = ss_hook_logger;
	logger.buffer = vmalloc(PAGE_SIZE * 3);

	if (logger.buffer)
		pr_info("logger buffer alloc address: 0x%p\n", logger.buffer);

	return 0;

fail_out:
	return err;


}

static const struct of_device_id dt_match[] = {
	{ .compatible = "ss_plog" },
	{}
};

static struct platform_driver ss_plog_driver = {
	.probe		= ss_plog_probe,
	.driver		= {
		.name		= "ss_plog",
		.of_match_table	= dt_match,
	},
};

static int __init ss_plog_init(void)
{
	return platform_driver_register(&ss_plog_driver);
}
device_initcall(ss_plog_init);

static void __exit ss_plog_exit(void)
{
	platform_driver_unregister(&ss_plog_driver);
}
module_exit(ss_plog_exit);

