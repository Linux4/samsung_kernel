#ifndef _LINUX_CORESIGHT_APETB_H
#define _LINUX_CORESIGHT_APETB_H

#include <linux/device.h>
#include <linux/perf_event.h>
#include <linux/sched.h>

#define MAX_ETB_SOURCE_NUM			8

struct apetb_device;

struct apetb_ops {
	void (*init)(struct apetb_device *dbg);
	void (*exit)(struct apetb_device *dbg);
};

struct apetb_device {
	struct device dev;
	struct apetb_ops *ops;
	void *apetb_sink;
	void *apetb_source[MAX_ETB_SOURCE_NUM];
	u32 source_num;
	bool activated;
};

struct apetb_device *apetb_device_register(struct device *parent,
					       struct apetb_ops *ops,
					       const char *serdes_name);

#define to_apetb_device(d) container_of(d, struct apetb_device, dev)

#endif
