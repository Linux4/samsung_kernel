#ifndef _HALL_H
#define _HALL_H

#ifdef CONFIG_SEC_SYSFS
#define SEC_KEYPAD_PATH	"sec_key"
#endif

struct hall_platform_data {
	unsigned int rep:1;		/* enable input subsystem auto repeat */
	int gpio_flip_cover;
};
//extern struct device *sec_key;

#ifdef CONFIG_SEC_SYSFS
extern struct device *sec_dev_get_by_name(char *name);
#endif

#endif /* _HALL_H */
