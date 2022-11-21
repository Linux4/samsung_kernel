#ifndef __SHUB_UTILITY_H_
#define __SHUB_UTILITY_H_

#include <linux/kernel.h>
#include <linux/errno.h>
#include <linux/rtc.h>

#define shub_dbg(fmt, ...) do { \
	pr_debug("[SHUB] " fmt "\n", ##__VA_ARGS__); \
	} while (0)

#define shub_info(fmt, ...) do { \
	pr_info("[SHUB] " fmt "\n", ##__VA_ARGS__); \
	} while (0)

#define shub_conditional(condition, fmt, ...) do { \
	if (condition) \
		pr_info("[SHUB] " fmt "\n", ##__VA_ARGS__); \
	} while (0)

#define shub_err(fmt, ...) do { \
	pr_err("[SHUB] " fmt "\n", ##__VA_ARGS__); \
	} while (0)

#define shub_dbgf(fmt, ...) do { \
	pr_debug("[SHUB] %20s(%4d): " fmt "\n", __func__, __LINE__, ##__VA_ARGS__); \
	} while (0)

#define shub_infof(fmt, ...) do { \
	pr_info("[SHUB] %20s(%4d): " fmt "\n", __func__, __LINE__, ##__VA_ARGS__); \
	} while (0)

#define shub_errf(fmt, ...) do { \
	pr_err("[SHUB] %20s(%4d): " fmt "\n", __func__, __LINE__, ##__VA_ARGS__); \
	} while (0)

#define MAKE_WORD(H,L) ((((u16)H) << 8 ) & 0xff00 ) | ((((u16)L)) & 0x00ff )
#define WORD_TO_LOW(w) ((u8)((w) & 0xff ))
#define WORD_TO_HIGH(w) ((u8)(((w) >>8 ) & 0xff))

#define ARRAY_LEN(ary) (sizeof(ary)/sizeof(ary[0]))
#define ABS(a) ((a) > 0 ? (a) : -(a))

#define BITS_PER_BYTE           8

u64 get_current_timestamp(void);
void get_tm(struct rtc_time *tm);

#endif /* __SHUB_UTILITY_H_ */
