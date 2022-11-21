#ifndef __SWTP_H__
#define __SWTP_H__

#define SWTP_INT_PIN_PLUG_IN  (1)
#define SWTP_INT_PIN_PLUG_OUT (0)

struct swtp_t {
	unsigned int irq;
	unsigned int gpiopin;
	unsigned int curr_mode;
	unsigned int retry_cnt;
	struct input_dev * swtp_dev;
	spinlock_t   spinlock;
};

#endif /* __SWTP_H__*/
