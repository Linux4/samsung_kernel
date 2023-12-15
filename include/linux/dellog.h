/*  dellog internals
 *
 */

#ifndef _LINUX_DELLOG_H
#define  _LINUX_DELLOG_H

#define DELLOG_ACTION_CLOSE          	0
#define DELLOG_ACTION_OPEN           	1
#define DELLOG_ACTION_READ		2
#define DELLOG_ACTION_READ_ALL		3
#define DELLOG_ACTION_WRITE    		4
#define DELLOG_ACTION_SIZE_BUFFER   	5

#define DELLOG_FROM_READER		0
#define DELLOG_FROM_PROC		1

int do_dellog(int type, char __user *buf, int count, bool from_file);
int do_dellog_write(int type, const char __user *buf, int count, bool from_file);
int vdellog(const char *fmt, va_list args);
int dellog(const char *fmt, ...);

#ifdef CONFIG_PROC_DELLOG
#define DEL_LOG(fmt,...) dellog(fmt,##__VA_ARGS__)
#else
#define DEL_LOG(fmt,...)
#endif /* CONFIG_PROC_DELLOG */


#endif /* _DELLOG_H */
