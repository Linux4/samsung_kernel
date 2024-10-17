#include <linux/list.h>

#define NAME_MAX_LEN				32
#define ACTIVATION_EVENT			"machine_suspend"
#define INACTIVATION_EVENT			"thaw_processes"
#define CLEAR_EVENT					"suspend_enter"
#define PROC_READING_START_EVENT	"read_start"
#define PROC_READING_END_EVENT		"read_end"

enum {
	INACTIVE_LIST_EMPTY,
	INACTIVE_LIST_FULL,
	ACTIVE,
	PROC_READING
};

struct pm_major_event {
	struct list_head list;
	char name[NAME_MAX_LEN];
	bool start;
	u64 timestamp;
	struct list_head devpm_list;
};

struct pm_devpm_event {
	struct list_head list;
	char drv_name[NAME_MAX_LEN];
	char fn_name[NAME_MAX_LEN];
	u64 timestamp;
	u64 duration;
};

