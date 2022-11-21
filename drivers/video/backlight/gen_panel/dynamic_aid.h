#ifndef _DYNAMIC_AID_H_
#define _DYNAMIC_AID_H_

#include <linux/kernel.h>
#include <linux/errno.h>
#include <linux/string.h>

struct smartdim_conf {
	void (*generate_gamma)(int cd, char *str);
	void (*get_min_lux_table)(char *str, int size);
	void (*init)(void);
	void (*print_aid_log)(void);
	char *mtp_buffer;
	int *lux_tab;
	int lux_tabsize;
	int *rgb_offset;
	int *candela_offset;
	int *base_lux_table;
	int *gamma_curve_table;
	int vregout_voltage;
	unsigned int man_id;
};

/* Define the smart dimming LDIs*/
struct smartdim_conf *sdim_get_conf(void);

typedef struct smartdim_conf *(* GET_CONF_FUNC)(void);

struct smartdim_get_conf_map {
	const char *name;
	GET_CONF_FUNC func;
};

static struct smartdim_get_conf_map smartdim_confs[] = {
	{ .name = "sdim_oled", .func = sdim_get_conf, },
};

static inline GET_CONF_FUNC smartdim_get_conf_func(const char *name)
{
	int i;

	for (i = 0; i < ARRAY_SIZE(smartdim_confs); i++)
		if (!strncmp(name, smartdim_confs[i].name,
				strlen(smartdim_confs[i].name)))
			return smartdim_confs[i].func;

	return NULL;
}
#endif /* _DYNAMIC_AID_H_ */
