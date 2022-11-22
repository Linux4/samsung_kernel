/*
 * rtc.h  --  RTC driver for Dialog Semiconductor D2199 PMIC
 *
 * Copyright 2013 Dialog Semiconductor Ltd
 *
 *  This program is free software; you can redistribute  it and/or modify it
 *  under  the terms of  the GNU General  Public License as published by the
 *  Free Software Foundation;  either version 2 of the  License, or (at your
 *  option) any later version.
 *
 */

#ifndef __LINUX_D2199_RTC_H
#define __LINUX_D2199_RTC_H

#include <linux/platform_device.h>

/*
 * RTC Retries.
 */
#define D2199_GET_TIME_RETRIES					5

/*
 * RTC MASK.
 */
#define D2199_RTC_SECS_MASK						0x3F
#define D2199_RTC_MINS_MASK						0x3F
#define D2199_RTC_HRS_MASK						0x1F
#define D2199_RTC_DAY_MASK						0x1F
#define D2199_RTC_MTH_MASK						0x0F
#define D2199_RTC_YRS_MASK						0x3F

#define D2199_RTC_ALMSECS_MASK					0x3F
#define D2199_RTC_ALMMINS_MASK					0x3F
#define D2199_RTC_ALMHRS_MASK			        0x1F
#define D2199_RTC_ALMDAY_MASK			        0x1F
#define D2199_RTC_ALMMTH_MASK			        0x0F
#define D2199_RTC_ALMYRS_MASK			        0x3F

/*
 * Limit values
 */
#define D2199_RTC_SECONDS_LIMIT			        (59)
#define D2199_RTC_MINUTES_LIMIT                 (59)
#define D2199_RTC_HOURS_LIMIT			        (23)
#define D2199_RTC_DAYS_LIMIT			        (31)
#define D2199_RTC_MONTHS_LIMIT			        (12)
#define D2199_RTC_YEARS_LIMIT			        (63)

/*
 * RTC error codes
 */
#define D2199_RTC_INVALID_SECONDS               (3)
#define D2199_RTC_INVALID_MINUTES               (4)
#define D2199_RTC_INVALID_HOURS		            (5)
#define D2199_RTC_INVALID_DAYS                  (6)
#define D2199_RTC_INVALID_MONTHS                (7)
#define D2199_RTC_INVALID_YEARS                 (8)
#define D2199_RTC_INVALID_EVENT                 (9)
#define D2199_RTC_INVALID_IOCTL                 (10)
#define D2199_RTC_INVALID_SETTING               (11)
#define D2199_RTC_EVENT_ALREADY_REGISTERED      (12)
#define D2199_RTC_EVENT_UNREGISTERED            (13)
#define D2199_RTC_EVENT_REGISTRATION_FAILED     (14)
#define D2199_RTC_EVENT_UNREGISTRATION_FAILED	(15)

struct d2199_rtc {
	struct platform_device  *pdev;
	struct rtc_device       *rtc;
	int                     alarm_enabled;      /* used over suspend/resume */
};

struct setSrAlarmData_t {
	bool            isEnabled;
	unsigned long   realAlarm;
};

#endif /* __LINUX_D2199_RTC_H */
