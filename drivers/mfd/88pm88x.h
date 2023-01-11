#ifndef _MFD_88PM88X_H
#define _MFD_88PM88X_H

#include <linux/kernel.h>
#include <linux/of.h>
#include <linux/regmap.h>
#include <linux/mfd/core.h>

struct pmic_cell_info {
	const struct mfd_cell *cells;
	int		cell_nr;
};

#define CELL_IRQ_RESOURCE(_name, _irq) { \
	.name = _name, \
	.start = _irq, .end = _irq, \
	.flags = IORESOURCE_IRQ, \
	}
#define CELL_DEV(_name, _r, _compatible, _id) { \
	.name = _name, \
	.of_compatible = _compatible, \
	.num_resources = ARRAY_SIZE(_r), \
	.resources = _r, \
	.id = _id, \
	}

/* 88pm886 */
extern const struct regmap_config pm886_base_i2c_regmap;
extern const struct regmap_config pm886_power_i2c_regmap;
extern const struct regmap_config pm886_gpadc_i2c_regmap;
extern const struct regmap_config pm886_battery_i2c_regmap;
extern const struct regmap_config pm886_test_i2c_regmap;

extern const struct mfd_cell pm886_cell_devs[];
extern struct pmic_cell_info pm886_cell_info;

int pm886_apply_patch(struct pm88x_chip *chip);

/* 88pm880 */
extern const struct regmap_config pm880_base_i2c_regmap;
extern const struct regmap_config pm880_power_i2c_regmap;
extern const struct regmap_config pm880_gpadc_i2c_regmap;
extern const struct regmap_config pm880_battery_i2c_regmap;
extern const struct regmap_config pm880_test_i2c_regmap;

extern const struct mfd_cell pm880_cell_devs[];
extern struct pmic_cell_info pm880_cell_info;

int pm880_apply_patch(struct pm88x_chip *chip);

#endif
