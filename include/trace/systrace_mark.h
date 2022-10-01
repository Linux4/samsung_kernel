#if !defined(_TRACE_SYSTRACE_MARK_H)
#define _TRACE_SYSTRACE_MARK_H

noinline void tracing_mark_write(int type, const char *str);

#define SYSTRACE_MARK_TYPE_BEGIN 0
#define SYSTRACE_MARK_TYPE_END 1

#define SYSTRACE_MARK_BUF_SIZE 256

#define __systrace_mark(type, fmt, args...)			\
do {								\
	char buf[SYSTRACE_MARK_BUF_SIZE];			\
	snprintf(buf, SYSTRACE_MARK_BUF_SIZE, fmt, ##args);	\
	tracing_mark_write(type, buf);				\
} while (0)

#define systrace_mark_begin(fmt, args...)			\
	__systrace_mark(SYSTRACE_MARK_TYPE_BEGIN, fmt, ##args)
#define systrace_mark_end_debug(fmt, args...)			\
	__systrace_mark(SYSTRACE_MARK_TYPE_END, fmt, ##args)
#define systrace_mark_end()					\
    __systrace_mark(SYSTRACE_MARK_TYPE_END, "")

#endif /* _TRACE_SYSTRACE_MARK_H */
