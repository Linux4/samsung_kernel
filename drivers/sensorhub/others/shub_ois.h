#ifndef __SHUB_OIS_H_
#define __SHUB_OIS_H_
struct ois_sensor_interface {
	void *core;
	void (*ois_func)(void *);
};

void notify_ois_reset(void);

#endif
