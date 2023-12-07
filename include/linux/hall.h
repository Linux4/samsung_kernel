#ifndef _HALL_H
#define _HALL_H
struct hall_platform_data {
	unsigned int rep:1;		/* enable input subsystem auto repeat */
	int gpio_flip_cover;
	int gpio_certify_cover;
};
extern struct device *sec_key;

#ifdef CONFIG_SENSORS_HALL_IRQ_CTRL
extern void hall_irq_set(int state, bool auth_changed);
enum state {
	disable = 0,
	enable
};
#endif

#ifdef CONFIG_CERTIFY_HALL_NFC_WA
extern int felica_ant_tuning(int parameter);
#endif

#ifdef CONFIG_SENSORS_HALL_FOLDER
void hall_ic_register_notify(struct notifier_block *nb);
#endif

#endif /* _HALL_H */
