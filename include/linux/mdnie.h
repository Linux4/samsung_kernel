#ifndef __MDNIE_H__
#define __MDNIE__

struct platform_mdnie_data {
	unsigned int	display_type;
    unsigned int    support_pwm;

	struct lcd_platform_data	*lcd_pd;
};

#endif
