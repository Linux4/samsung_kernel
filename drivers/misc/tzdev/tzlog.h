#ifndef __TZLOG_H__
#define __TZLOG_H__

extern int tzdev_verbosity;
#define tzdev_print(lvl, fmt, ...) \
	do { \
		if (lvl <= tzdev_verbosity) \
			printk("%s: "fmt, __func__, ##__VA_ARGS__); \
	} while (0)

#endif /* __TZLOG_H__ */
