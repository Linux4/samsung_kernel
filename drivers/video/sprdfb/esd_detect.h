#ifndef __ESD_DETECT_H__
#define __ESD_DETECT_H__

#include <linux/interrupt.h>
#include <linux/workqueue.h>
#include <linux/irq.h>
#include <linux/gpio.h>
#include <linux/delay.h>
#include <linux/mutex.h>

enum {
	ESD_STATUS_OK = 0,
	ESD_STATUS_NG = 1,
};

enum {
	ESD_POLLING = IRQF_TRIGGER_NONE,
	ESD_TRIGGER_RISING = IRQF_TRIGGER_RISING,
	ESD_TRIGGER_FALLING = IRQF_TRIGGER_FALLING,
	ESD_TRIGGER_HIGH = IRQF_TRIGGER_HIGH,
	ESD_TRIGGER_LOW = IRQF_TRIGGER_LOW,
};

struct esd_det_info {
	char *name;	/* device name */
	void *pdata;	/* private data for device */
	int type;	/* esd detect mode (interrupt or polling) */
	int gpio;	/* esd detect gpio num */

	/* internal use work queue */
	struct workqueue_struct *wq;
	struct delayed_work work;

	struct mutex *lock;		/* device lock */
	bool (*is_active)(void *pdata);	/* device activated */
	int (*is_normal)(void *pdata);	/* device esd checking */
	int (*recover)(void *pdata);	/* device recovery */
};

int esd_det_init(struct esd_det_info *det);
int esd_det_enable(struct esd_det_info *det);
int esd_det_disable(struct esd_det_info *det);
#endif	/* __ESD_DETECT_H__ */
