#include <linux/kernel.h>
#include <linux/time.h>
#include <linux/ktime.h>
#include <linux/rtc.h>

u64 get_current_timestamp(void)
{
	u64 timestamp;
	struct timespec ts;

	ts = ktime_to_timespec(ktime_get_boottime());
	timestamp = ts.tv_sec * 1000000000ULL + ts.tv_nsec;

	return timestamp;
}

void get_tm(struct rtc_time *tm)
{
	struct timespec ts;

	getnstimeofday(&ts);
	rtc_time_to_tm(ts.tv_sec, tm);
}