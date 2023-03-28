#ifndef __SARDSI_H__
#define __SARDSI_H__

#define SAR_DSI_NEAR  (1)
#define SAR_DSI_FAR   (0)

struct sardsi_t {
	unsigned int irq;
	unsigned int gpiopin;
	unsigned int curr_mode;
	unsigned int retry_cnt;
	struct input_dev * dsi_dev;
	spinlock_t   spinlock;
};

#endif /* __SARDSI_H__*/
