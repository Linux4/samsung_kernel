#ifndef LINUX_SPI_CMMB_H
#define LINUX_SPI_CMMB_H

struct cmmb_platform_data {
	int (*power_on) (void);
	int (*power_off) (void);
	int (*power_reset) (void);
	int (*cs_assert) (void);
	int (*cs_deassert) (void);

	int (*cmmb_regulator) (bool en);

	int gpio_power;
	int gpio_reset;
	int gpio_cs;
	int gpio_defined;
};

#endif
