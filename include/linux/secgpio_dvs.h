/*
	 secgpio_dvs.h -- Samsung GPIO debugging and verification system
 */

#ifndef __SECGPIO_DVS_H
#define __SECGPIO_DVS_H

#include <linux/types.h>

enum gdvs_phone_status {
	PHONE_INIT = 0,
	PHONE_SLEEP,
	GDVS_PHONE_STATUS_MAX
};

struct gpiomap_result {
	unsigned char *init;
	unsigned char *sleep;
};

enum gdvs_io_value {
	GDVS_IO_FUNC = 0x00,
	GDVS_IO_IN,
	GDVS_IO_OUT,
	GDVS_IO_INT,
	GDVS_IO_PREV,
	GDVS_IO_HI_Z,
	GDVS_IO_RSV = 0x3E,
	GDVS_IO_ERR
};


struct gpio_dvs {
	struct gpiomap_result *result;
	unsigned int count;
	bool check_sleep;
	void (*check_gpio_status)(unsigned char phonestate);
};

void gpio_dvs_check_initgpio(void);
void gpio_dvs_check_sleepgpio(void);

#endif /* __SECGPIO_DVS_H */
