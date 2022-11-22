#include <linux/types.h>

enum {
	SHUB_LOG_EVENT_TIMESTAMP = 0,
	SHUB_LOG_DATA_PACKET,
	SHUB_LOG_MAX,
};

int init_shub_debug(void);
void exit_shub_debug(void);
void enable_debug_timer(void);
void disable_debug_timer(void);

int init_shub_debug_sysfs(void);
void remove_shub_debug_sysfs(void);

int print_mcu_debug(char *dataframe, int *index, int frame_len);
void print_dataframe(char *dataframe, int frame_len);
int print_system_info(char *dataframe, int *index);

bool check_debug_log_state(int log_type);
