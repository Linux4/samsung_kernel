/*
 * Copyright (C) 2019 Spreadtrum Communications Inc.
 *
 * KUnit test for thermal.
 */
#ifndef __SPRD_CPU_DEVICE_TEST_H__
#define __SPRD_CPU_DEVICE_TEST_H__

#include <test/test.h>
#include <test/mock.h>
#include <linux/cpumask.h>

#define CPU_MASK_CPU4							\
(cpumask_t) { {								\
	[0] =  0xfUL							\
} }

u64 get_core_dyn_power(int cluster_id,
	unsigned int freq_mhz, unsigned int voltage_mv);

u64 get_cluster_dyn_power(int cluster_id,
	unsigned int freq_mhz, unsigned int voltage_mv);

u32 get_cluster_min_cpufreq(int cluster_id);

u32 get_cluster_min_cpunum(int cluster_id);

u32 get_cluster_resistance_ja(int cluster_id);

int get_static_power(cpumask_t *cpumask,
	int interval, unsigned long u_volt, u32 *power, int temperature);

int get_core_static_power(cpumask_t *cpumask,
	int interval, unsigned long u_volt, u32 *power, int temperature);

int get_all_core_temp(int cluster_id, int cpu);

u32 get_core_cpuidle_tp(int cluster_id, int first_cpu, int cpu, int *temp);

u32 get_cpuidle_temp_point(int cluster_id);

void get_core_temp(int cluster_id, int cpu, int *temp);

u64 get_cluster_temperature_scale(int cluster_id, u64 temp);

u64 get_core_temperature_scale(int cluster_id, u64 temp);

u64 get_cluster_voltage_scale(int cluster_id, unsigned long u_volt);

u64 get_core_voltage_scale(int cluster_id, unsigned long u_volt);

int get_cache_static_power_coeff(int cluster_id);

int get_cpu_static_power_coeff(int cluster_id);

ssize_t sprd_cpu_show_min_freq(struct device *dev,
	struct device_attribute *attr, char *buf);

ssize_t sprd_cpu_store_min_freq(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t count);

ssize_t sprd_cpu_show_min_core_num(struct device *dev,
	struct device_attribute *attr, char *buf);

ssize_t sprd_cpu_store_min_core_num(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t count);

int cpu_cooling_pm_notify(struct notifier_block *nb,
	unsigned long mode, void *_unused);

u64 get_leak_base(int cluster_id, int val, int *coeff);

int create_cpu_cooling_device(void);

int destroy_cpu_cooling_device(void);

#endif /* __SPRD_CPU_DEVICE_TEST_H__ */
