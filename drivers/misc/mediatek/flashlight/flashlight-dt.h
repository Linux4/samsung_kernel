/*
 * Copyright (C) 2015 MediaTek Inc.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 */

#ifndef _FLASHLIGHT_DT_H
#define _FLASHLIGHT_DT_H

#ifdef mt6757
#define S2MU005_DTNAME    "mediatek,flashlights_s2mu005"
#ifdef CONFIG_LEDS_S2MU005_FLASH
extern void ss_rear_flash_led_flash_on(void);
extern void ss_rear_flash_led_torch_on(void);
extern void ss_rear_flash_led_turn_off(void);
extern void ss_rear_torch_set_flashlight(int torch_mode);
extern void ss_front_flash_led_turn_on(void);
extern void ss_front_flash_led_turn_off(void);
#endif
#define LM3643_DTNAME     "mediatek,flashlights_lm3643"
#define LM3643_DTNAME_I2C "mediatek,strobe_main"
#define RT5081_DTNAME     "mediatek,flashlights_rt5081"
#endif

#ifdef mt6799
#define MT6336_DTNAME     "mediatek,flashlights_mt6336"
#endif

#endif /* _FLASHLIGHT_DT_H */
