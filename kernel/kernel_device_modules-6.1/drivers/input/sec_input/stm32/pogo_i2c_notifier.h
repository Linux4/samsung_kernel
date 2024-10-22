/*
 * pogo_i2c_notifier.h is old header for pogo v1&v2 and it is not supported from sec_input_2024
 * but when v3 is enabled, other modules can be include this file and it works as v3
 */

#ifndef __POGO_I2C_NOTIFIER_H__
#define __POGO_I2C_NOTIFIER_H__
#if IS_ENABLED(CONFIG_KEYBOARD_STM32_POGO_V3)
#include <linux/input/pogo_notifier_v3.h>
#endif
#endif
