#include <linux/types.h>

#ifdef CONFIG_SSP_ENG_DEBUG

void sensorhub_system_check(struct ssp_data *data, uint32_t test_delay_us, uint32_t test_count);
void ssp_system_checker_init(void);
bool is_system_checking(void);
bool is_event_order_checking(void);
void system_ready_cb(void);
void comm_test_cb(int32_t type);
void event_test_cb(int32_t type, uint64_t timestamp);
void order_test_cb(int32_t type, uint64_t timestamp);
void ssp_system_check_lock(void);
void ssp_system_check_unlock(void);

#endif
