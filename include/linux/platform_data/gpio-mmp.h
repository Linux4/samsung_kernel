#ifndef __GPIO_MMP_H
#define __GPIO_MMP_H

struct mmp_gpio_platform_data {
	unsigned int *bank_offset;
	unsigned int nbank;
};

#ifdef CONFIG_SEC_GPIO_DVS
extern int pxa_direction_get(unsigned int *gpdr, int index);
#endif

#endif /* __GPIO_MMP_H */
