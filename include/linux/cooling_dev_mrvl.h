#ifndef __COOLING_DEV_MRVL_H__
#define __COOLING_DEV_MRVL_H__

#include <linux/thermal.h>
#include <linux/cpumask.h>

#ifdef CONFIG_COOLING_DEV_MRVL

#ifdef CONFIG_CPU_FREQ
struct thermal_cooling_device *cpufreq_cool_register(const char *cpu_name);
void cpufreq_cool_unregister(struct thermal_cooling_device *cdev);
#else
static inline struct thermal_cooling_device *cpufreq_cool_register(const char *cpu_name)
{
	return NULL;
}
static inline
void cpufreq_cool_unregister(struct thermal_cooling_device *cdev)
{
	return;
}
#endif

#ifdef CONFIG_HOTPLUG_CPU
struct thermal_cooling_device *cpuhotplug_cool_register(const char *cpu_name);
void cpuhotplug_cool_unregister(struct thermal_cooling_device *cdev);
#else
static inline struct thermal_cooling_device *cpuhotplug_cool_register(const char *cpu_name)
{
	return NULL;
}
static inline
void cpuhotplug_cool_unregister(struct thermal_cooling_device *cdev)
{
	return;
}
#endif

#ifdef CONFIG_DDR_DEVFREQ
struct thermal_cooling_device *ddrfreq_cool_register(void);
void ddrfreq_cool_unregister(struct thermal_cooling_device *cdev);
#else
static inline struct thermal_cooling_device *ddrfreq_cool_register(void)
{
	return NULL;
}
static inline
void ddrfreq_cool_unregister(struct thermal_cooling_device *cdev)
{
	return;
}
#endif

#ifdef CONFIG_VPU_DEVFREQ
struct thermal_cooling_device *vpufreq_cool_register(unsigned int dev_id);
void vpufreq_cool_unregister(struct thermal_cooling_device *cdev);
#else
static inline struct thermal_cooling_device *vpufreq_cool_register(unsigned int dev_id)
{
	return NULL;
}
static inline void vpufreq_cool_unregister(struct thermal_cooling_device *cdev)
{
	return;
}
#endif

#ifdef CONFIG_PM_DEVFREQ
struct thermal_cooling_device *gpufreq_cool_register(const char *gc_name);
void gpufreq_cool_unregister(struct thermal_cooling_device *cdev);
#else
static inline struct thermal_cooling_device *gpufreq_cool_register(const char *gc_name)
{
	return NULL;
}
static inline void gpufreq_cool_unregister(struct thermal_cooling_device *cdev)
{
	return;
}
#endif

#else /* !CONFIG_COOLING_DEV_MRVL */
static inline struct thermal_cooling_device *cpufreq_cool_register(const char *cpu_name)
{
	return NULL;
}
static inline
void cpufreq_cool_unregister(struct thermal_cooling_device *cdev)
{
	return;
}
static inline struct thermal_cooling_device *cpuhotplug_cool_register(const char *cpu_name)
{
	return NULL;
}
static inline
void cpuhotplug_cool_unregister(struct thermal_cooling_device *cdev)
{
	return;
}
static inline struct thermal_cooling_device *ddrfreq_cool_register(void)
{
	return NULL;
}
static inline
void ddrfreq_cool_unregister(struct thermal_cooling_device *cdev)
{
	return;
}
static inline struct thermal_cooling_device *vpufreq_cool_register(unsigned int dev_id)
{
	return NULL;
}
static inline void vpufreq_cool_unregister(struct thermal_cooling_device *cdev)
{
	return;
}
static inline struct thermal_cooling_device *gpufreq_cool_register(const char *gc_name)
{
	return NULL;
}
static inline void gpufreq_cool_unregister(struct thermal_cooling_device *cdev)
{
	return;
}
#endif	/* CONFIG_COOLING_DEV_MRVL */

#endif /* __COOLING_DEV_MRVL_H__ */
