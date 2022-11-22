#ifndef _FINGERPRINT_COMMON_H_
#define _FINGERPRINT_COMMON_H_

#include "fingerprint.h"
#include <linux/pm_qos.h>
#include <linux/delay.h>
#include <linux/pm_wakeup.h>
#include <linux/clk.h>

struct boosting_config {
	unsigned int min_cpufreq_limit;
	struct pm_qos_request pm_qos;
};

struct spi_clk_setting {
	bool enabled_clk;
	u32 spi_speed;
	struct wakeup_source *spi_wake_lock;
#ifdef ENABLE_SENSORS_FPRINT_SECURE
	struct clk *fp_spi_pclk;
	struct clk *fp_spi_sclk;
#endif
};

void set_sensor_type(const int type_value, int *sensortype);
int spi_clk_enable(struct spi_clk_setting *clk_setting);
int spi_clk_disable(struct spi_clk_setting *clk_setting);
int cpu_speedup_enable(struct boosting_config *boosting);
int cpu_speedup_disable(struct boosting_config *boosting);

#endif /* _FINGERPRINT_COMMON_H_ */
