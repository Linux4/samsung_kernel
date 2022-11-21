#ifndef __LINUX_FAB5405_H
#define __LINUX_FAB5405_H

#include <linux/ioctl.h>

#define CHARGE_STAT_READY	0
#define CHARGE_STAT_INPROGRESS  1
#define CHARGE_STAT_DONE	2
#define CHARGE_STAT_FAULT       3


typedef unsigned char BYTE;

typedef enum{
	FAN5405_MONITOR_NONE,
	FAN5405_MONITOR_CV,
	FAN5405_MONITOR_VBUS_VALID,
	FAN5405_MONITOR_IBUS,
	FAN5405_MONITOR_ICHG,
	FAN5405_MONITOR_T_120,
	FAN5405_MONITOR_LINCHG,
	FAN5405_MONITOR_VBAT_CMP,
	FAN5405_MONITOR_ITERM_CMP
}fan5405_monitor_status;

struct fan5405_platform_data {
	uint16_t version;
// reserve just in case
};
struct sprd_ext_ic_operations {
	void (*ic_init)(void);
	void (*charge_start_ext)(int);
	void (*charge_stop_ext)(void);
	int  (*get_charging_status)(void);
	void (*timer_callback_ext)(void);
	void (*otg_charge_ext)(int enable);
};

extern void fan5405_TA_StartCharging(void);
extern void fan5405_USB_StartCharging(void);
extern void fan5405_StopCharging(void);
extern fan5405_monitor_status fan5405_Monitor(void);
extern int fan5405_GetCharge_Stat(void);

const struct sprd_ext_ic_operations *sprd_get_ext_ic_ops(void);

#endif
