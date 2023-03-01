#ifndef __INTERNAL__SEC_BOOT_STAT_PROC_H__
#define __INTERNAL__SEC_BOOT_STAT_PROC_H__

struct boot_stat_proc {
	struct proc_dir_entry *proc;
	struct mutex lock;
	bool is_completed;
	unsigned long long ktime_completed;
	size_t *event_record;
	size_t total_event;
	size_t nr_event;
	struct list_head systemserver_init_time_head;
};

enum {
	EVT_PLATFORM = 0,
	EVT_RIL,
	EVT_DEBUG,
	EVT_SYSTEMSERVER,
	EVT_INVALID,
	NUM_OF_BOOT_PREFIX = EVT_INVALID,
};

struct boot_prefix {
	const char *head;
	size_t head_len;
};

struct boot_event {
	size_t prefix_idx;
	const char *message;
	size_t message_len;
	unsigned long long ktime;
};

#endif /* __INTERNAL__SEC_BOOT_STAT_PROC_H__ */
