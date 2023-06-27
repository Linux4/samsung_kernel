/* include/linux/leds/rt5033_fled.h
 * Header of Richtek RT5033 Flash LED Driver
 *
 * Copyright (C) 2013 Richtek Technology Corp.
 * Author: Patrick Chang <patrick_chang@richtek.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 */

#ifndef LINUX_LEDS_RT5033_FLED_H
#define LINUX_LEDS_RT5033_FLED_H
#include <linux/kernel.h>



#define RT5033_STROBE_CURRENT(mA) (((mA - 50) / 25) & 0x1f)
#define RT5033_TIMEOUT_LEVEL(mA) (((mA - 50) / 25) & 0x07)
#define RT5033_STROBE_TIMEOUT(ms) (((ms - 64) / 32) & 0x3f)
#define RT5033_TORCH_CURRENT(mA) (((mA * 10 - 125) / 125) & 0x0f)
#define RT5033_LV_PROTECTION(mV) (((mV-2900) / 100) & 0x07)
#define RT5033_MID_REGULATION_LEVEL(mV) (((mV - 3625) / 25) & 0x3f)

typedef struct rt5033_fled_platform_data {
    unsigned int fled1_en : 1;
    unsigned int fled2_en : 1;
    unsigned int fled_mid_track_alive : 1;
    unsigned int fled_mid_auto_track_en : 1;
    unsigned int fled_timeout_current_level;
    unsigned int fled_strobe_current;
    unsigned int fled_strobe_timeout;
    unsigned int fled_torch_current;
    unsigned int fled_lv_protection;
    unsigned int fled_mid_level;
} rt5033_fled_platform_data_t;

#define RT5033_FLED_RESET           0x21

#define RT5033_FLED_FUNCTION1       0x21
#define RT5033_FLED_FUNCTION2       0x22
#define RT5033_FLED_STROBE_CONTROL1 0x23
#define RT5033_FLED_STROBE_CONTROL2 0x24
#define RT5033_FLED_CONTROL1        0x25
#define RT5033_FLED_CONTROL2        0x26
#define RT5033_FLED_CONTROL3        0x27
#define RT5033_FLED_CONTROL4        0x28
#define RT5033_FLED_CONTROL5        0x29

#define RT5033_CHG_FLED_CTRL        0x67

#endif /*LINUX_LEDS_RT5033_FLED_H*/
