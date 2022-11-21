#ifndef __INTERNAL__SEC_QC_LOG_BUF_H__
#define __INTERNAL__SEC_QC_LOG_BUF_H__

#include <linux/console.h>
#include <linux/kmsg_dump.h>
#include <linux/kprobes.h>

#include <linux/samsung/builder_pattern.h>
#include <linux/samsung/debug/sec_log_buf.h>

struct last_kmsg_data {
	char *buf;
	size_t size;
	struct proc_dir_entry *proc;
};

struct log_buf_drvdata;

struct log_buf_logger {
	int (*probe)(struct log_buf_drvdata *);
	void (*remove)(struct log_buf_drvdata *);
};

struct log_buf_drvdata {
	struct builder bd;
	struct reserved_mem *rmem;
	phys_addr_t paddr;
	size_t size;
	struct kmsg_dumper dumper;
	unsigned int strategy;
	union {
		struct console con;
		struct kprobe probe;
	};
	const struct log_buf_logger *logger;
	struct last_kmsg_data last_kmsg;
};

extern struct log_buf_drvdata *sec_log_buf;

static __always_inline bool __log_buf_is_probed(void)
{
	return !!sec_log_buf;
}

/* sec_log_buf_main.c */
extern void __log_buf_write(const char *s, size_t count);
extern void __log_buf_store_from_kmsg_dumper(void);

/* sec_log_buf_logger.c */
extern int __log_buf_logger_init(struct builder *bd);
extern void __log_buf_logger_exit(struct builder *bd);

/* sec_log_buf_builtin.c */
extern const struct log_buf_logger *__log_buf_logger_builtin_creator(void);

/* sec_log_buf_kprobe.c */
#if IS_ENABLED(CONFIG_SEC_LOG_BUF_USING_KPROBE)
extern const struct log_buf_logger *__log_buf_logger_kprobe_creator(void);
#else
static inline struct log_buf_logger *__log_buf_logger_kprobe_creator(void) { return ERR_PTR(-ENODEV); }
#endif

/* sec_log_buf_console.c */
extern const struct log_buf_logger *__log_buf_logger_console_creator(void);

/* sec_log_buf_vh_log_buf.c */
extern const struct log_buf_logger *__log_buf_logger_vh_logbuf_creator(void);

#endif /* __INTERNAL__SEC_QC_LOG_BUF_H__ */
