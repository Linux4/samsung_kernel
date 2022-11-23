#ifndef __SPRD_DEBUGSTAT_H__
#define __SPRD_DEBUGSTAT_H__

#include <linux/types.h>

struct subsys_sleep_info {
	char     subsystem_name[8];		//subsys name
	uint64_t total_duration;		//sum of tick from boot，total = active + sleep + idle.
	uint64_t sleep_duration_total;		//sum of tick from sleep.
	uint64_t idle_duration_total;		//sum of tick from idle
	uint32_t current_status;		//state of subsys，1: active/idle, 0: sleep.
	uint32_t subsystem_reboot_count;	//reboot timers
	uint32_t wakeup_reason;			//reboot reason.
	uint32_t last_wakeup_duration;		//wakeup time.
	uint32_t last_sleep_duration;		//sleep time.
	uint32_t active_core;			//cpu mask.
	uint32_t internal_irq_count;		//interrupt number
	uint32_t irq_to_ap_count;		//irq to ap.
	uint32_t reserve[4];			//reserve.
};

/* Get subsys sleep info struct */
typedef struct subsys_sleep_info *get_info_t(void *data);

#if defined(CONFIG_SPRD_POWER_STAT)
/**
 * stat_info_register - Register subsys sleep info get interface
 * @name: subsys name, like AP, PUBCP and so on.
 * @get: get subsys_sleep_info point, if get success, this interface return not NULL
 * @data: parameter of get interface
 */
int stat_info_register(char *name, get_info_t *get, void *data);
/**
 * stat_info_unregister - Unregister subsys sleep info node
 * @name: subsys name, like AP, PUBCP and so on.
 */
int stat_info_unregister(char *name);
#else
static inline int stat_info_register(char *name, get_info_t *get, void *data)
{
	return 0;
}
static inline int stat_info_unregister(char *name)
{
	return 0;
}
#endif

#endif /* __SPRD_DEBUGSTAT_H__ */
