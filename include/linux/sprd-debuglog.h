#ifndef __SPRD_DEBUGLOG_H__
#define __SPRD_DEBUGLOG_H__

#include <linux/types.h>

#define WAKEUP_NAME_LEN		(64)
#define INVALID_SUB_NUM		(-1)

enum wakeup_type {
	WAKEUP_GPIO = 1,
	WAKEUP_RTC,
	WAKEUP_MODEM,
	WAKEUP_SENSORHUB,
	WAKEUP_WIFI,
	WAKEUP_EIC_DBNC,
	WAKEUP_EIC_LATCH,
	WAKEUP_EIC_ASYNC,
	WAKEUP_EIC_SYNC,
	WAKEUP_PMIC_EIC,
};

struct wakeup_info {
	char name[WAKEUP_NAME_LEN];
	int type;
	int gpio;
	int source;
	int reason;
	int protocol;
	int version;
	int port;
};

#if defined(CONFIG_SPRD_POWER_LOG)
int wakeup_info_register(int gic_num, int sub_num,
				int (*get)(void *info, void *data), void *data);
int wakeup_info_unregister(int gic_num, int sub_num);
#else
static inline int wakeup_info_register(int gic_num, int sub_num,
				int (*get)(void *info, void *data), void *data)
{
	return 0;
}
static inline int wakeup_info_unregister(int gic_num, int sub_num)
{
	return 0;
}
#endif

#endif /* __SPRD_DEBUGLOG_H__ */
