enum power_type {
	PWR_ALW,
	PWR_REG,
	PWR_LDO,
	PWR_NONE,
};

struct irled_power {
	enum power_type type;
	struct regulator *regulator;
	unsigned ldo;
};

int of_irled_power_parse_dt(struct device *dev, struct irled_power *irled_power);
int irled_power_on(struct irled_power *irled_power);
int irled_power_off(struct irled_power *irled_power);
