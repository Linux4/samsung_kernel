#ifndef _FINGERPRINT_COMMON_H_
#define _FINGERPRINT_COMMON_H_

#include "fingerprint.h"
#include <linux/pm_qos.h>
#include <linux/delay.h>

struct boosting_config {
	unsigned int min_cpufreq_limit;
	struct pm_qos_request pm_qos;
};
void set_sensor_type(const int type_value, int *sensortype);
int cpu_speedup_enable(struct boosting_config *boosting);
int cpu_speedup_disable(struct boosting_config *boosting);

#endif /* _FINGERPRINT_COMMON_H_ */
