#ifndef __SEC_KBOOT_STAT_H__
#define __SEC_KBOOT_STAT_H__

#if IS_ENABLED(CONFIG_SEC_KBOOT_STAT)
extern bool sec_kboot_stat_en;

extern int __sec_initcall_debug(initcall_t fn);

static inline int sec_initcall_debug(initcall_t fn)
{
	if (likely(!sec_kboot_stat_en))
		return fn();

	return __sec_initcall_debug(fn);
}

extern int __sec_probe_debug(int (*really_probe)(struct device *dev, struct device_driver *drv),
		struct device *dev, struct device_driver *drv);


static inline int sec_probe_debug(
		int (*really_probe)(struct device *dev, struct device_driver *drv),
		struct device *dev, struct device_driver *drv)
{
	if (likely(!sec_kboot_stat_en))
		return really_probe(dev, drv);

	return __sec_probe_debug(really_probe, dev, drv);
}
#else
static inline int sec_initcall_debug(initcall_t fn)
{
	return fn();
}

static inline int sec_probe_debug(
		int (*really_probe)(struct device *dev, struct device_driver *drv),
		struct device *dev, struct device_driver *drv)
{
	return really_probe(dev, drv);
}
#endif

#endif /* __SEC_KBOOT_STAT_H__ */
