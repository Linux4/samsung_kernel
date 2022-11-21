#ifndef __TZPM_H__
#define __TZPM_H__

extern struct device *tzdev_mcd;

#define TZDEV_USE_DEVICE_TREE

#define NO_SLEEP_REQ	0
#define REQ_TO_SLEEP	1

#define NORMAL_EXECUTION	0
#define READY_TO_SLEEP		1

/* How much time after resume the daemon should backoff */
#define DAEMON_BACKOFF_TIME	500

/* Initialize secure crypto clocks */
int qc_pm_clock_initialize(void);
/* Free secure crypto clocks */
void qc_pm_clock_finalize(void);
/* Enable secure crypto clocks */
int qc_pm_clock_enable(void);
/* Disable secure crypto clocks */
void qc_pm_clock_disable(void);

#endif /* __TZPM_H__ */
