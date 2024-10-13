#ifndef __SEC_BOOT_STAT_TEST_H__
#define __SEC_BOOT_STAT_TEST_H__

#include <linux/miscdevice.h>

#include "../sec_boot_stat.h"

struct message_head {
	bool expected;
	const char *log;
};

/* sec_boot_stat_main.c */
extern bool __is_for_enh_boot_time(const char *log);

/* sec_boot_stat_proc. */
extern int __boot_stat_init_boot_prefixes(void);
extern int __boot_stat_init_boot_events(void);
extern bool __boot_stat_is_boot_event(const char *log);
extern ssize_t __boot_stat_get_message_offset_from_plog(const char *log, size_t *offset);
extern struct boot_event boot_events[35];
extern void __boot_stat_record_boot_event_locked(struct boot_stat_proc *boot_stat, const char *message);

#endif /* __SEC_BOOT_STAT_TEST_H__ */
