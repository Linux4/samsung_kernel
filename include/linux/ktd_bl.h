#ifndef __KTD_BL_H__
#define __KTD_BL_H__
struct brt_value {
	int level;		// Platform setting values
	int tune_level;		// Chip Setting values
};

struct ktd_bl_info {
	char *name;
	unsigned char max_brightness;
	unsigned char min_brightness;
	unsigned char def_brightness;
	int gpio_bl_ctrl;
	int gpio_bl_pwm_en;
	struct brt_value *brt_table;
	unsigned int sz_table;
};

int ktd_backlight_disable(void);
int ktd_backlight_enable(void);
void backlight_set_brightness(int);
extern int wakeup_brightness;
#endif	/* __KTD_BL_H__ */
