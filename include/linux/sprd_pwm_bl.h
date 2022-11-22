#ifndef __SPRD_PWM_BL_H
#define __SPRD_PWM_BL_H

struct sprd_pwm_bl_platform_data {
        int brightness_max;
        int brightness_min;
        int pwm_index;
        int gpio_ctrl_pin;
        int gpio_active_level;
};

#endif
