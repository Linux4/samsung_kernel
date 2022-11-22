#include <linux/types.h>

#ifdef CONFIG_SHUB_DEBUG

void sensorhub_system_check(uint32_t test_delay_us, uint32_t test_count);
void shub_system_checker_init(void);
bool is_system_checking(void);
void system_ready_cb(void);
void comm_test_cb(int32_t type);
void event_test_cb(int32_t type, uint64_t timestamp);
void shub_system_check_lock(void);
void shub_system_check_unlock(void);

#endif
