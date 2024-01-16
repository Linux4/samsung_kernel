#ifndef __TAS256X_BIGDATA__
#define __TAS256X_BIGDATA__

#define TAS256X_SYSFS_CLASS_NAME	"tas256x_dev"
#define TAS256X_IRQ_DIR_NAME		"irqs"
#define TAS256X_REGISTERS_DIR_NAME		"registers"

/* Added for Big Data */
struct irq_bd {
	uint32_t count_clock_error;
	uint32_t count_spk_over_current;
	uint32_t count_spk_over_voltage;
	uint32_t count_spk_under_voltage;
	uint32_t count_die_over_temp;
	uint32_t count_brown_out;
	uint32_t persist_count_clock_error;
	uint32_t persist_count_spk_over_current;
	uint32_t persist_count_spk_over_voltage;
	uint32_t persist_count_spk_under_voltage;
	uint32_t persist_count_die_over_temp;
	uint32_t persist_count_brown_out;
};

struct registers_bd {
	uint8_t register_1;
	uint8_t register_2;
	uint8_t register_3;
};

struct tas_bigdata {
	struct class *bd_class;
	struct device *irq_dev;
	struct device *registers_dev;
	struct irq_bd irqs[2];
	struct registers_bd registers[2];
};

void tas256x_update_bigdata(int chn, int err_code);

#endif /* __TAS256X_BIGDATA__ */
