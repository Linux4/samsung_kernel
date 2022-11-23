#ifndef __ESD_DETECT_H__
#define __ESD_DETECT_H__

#include <linux/interrupt.h>
#include <linux/workqueue.h>
#include <linux/irq.h>
#include <linux/gpio.h>
#include <linux/delay.h>
#include <linux/mutex.h>

enum esd_det_mode {
	ESD_DET_POLLING,
	ESD_DET_INTERRUPT,
};

enum esd_det_level {
	ESD_DET_LOW = 0,
	ESD_DET_HIGH = 1,
};

enum esd_det_result {
	ESD_DETECTED,
	ESD_NOT_DETECTED,
};

enum esd_det_status {
	ESD_DET_NOT_INITIALIZED,
	ESD_DET_OFF,
	ESD_DET_ON,
};

struct esd_det_info {
	char *name;	/* device name */
	void *pdata;	/* private data for device */
	int mode;	/* esd detect mode (interrupt or polling) */
	int gpio;	/* esd detect gpio num */
	int level;	/* esd detect gpio level */
	int state;	/* esd detect driver state */

	/* internal use work queue */
	struct workqueue_struct *wq;
	struct work_struct work;
	struct delayed_work dwork;

	struct mutex *lock;		/* device lock */
	bool (*is_active)(void *pdata);	/* device activated */
	int (*is_normal)(void *pdata);	/* device esd checking */
	int (*recover)(void *pdata);	/* device recovery */
};

int esd_det_init(struct esd_det_info *det);
int esd_det_enable(struct esd_det_info *det);
int esd_det_disable(struct esd_det_info *det);
#endif	/* __ESD_DETECT_H__ */
