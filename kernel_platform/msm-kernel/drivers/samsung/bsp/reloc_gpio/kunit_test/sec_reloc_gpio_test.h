#ifndef __KUNIT__SEC_RELOC_GPIO_TEST_H__
#define __KUNIT__SEC_RELOC_GPIO_TEST_H__

#include <linux/gpio/driver.h>

#include "../sec_reloc_gpio.h"

extern int __reloc_gpio_parse_dt_reloc_base(struct builder *bd, struct device_node *np);
extern int __reloc_gpio_parse_dt_gpio_label(struct builder *bd, struct device_node *np);
extern bool __reloc_gpio_is_valid_gpio_num(struct reloc_gpio_chip *chip, int nr_gpio, int gpio_num);
extern bool __reloc_gpio_is_matched(struct gpio_chip *gc, struct reloc_gpio_chip *chip, int gpio_num);
extern int sec_reloc_gpio_is_matched_gpio_chip(struct gpio_chip *gc, void *__drvdata);
extern int __reloc_gpio_from_legacy_number(struct reloc_gpio_drvdata *drvdata, struct gpio_chip *gc);

#endif /* __KUNIT__SEC_RELOC_GPIO_TEST_H__ */
