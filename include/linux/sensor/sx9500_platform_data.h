/*
 * include/linux/input/sx9500_platform_data.h
 *
 * SX9500 Platform Data
 * 2 cap differential
 *
 * Copyright 2012 Semtech Corp.
 *
 * Licensed under the GPL-2 or later.
 */

#ifndef _SX9500_PLATFORM_DATA_H_
#define _SX9500_PLATFORM_DATA_H_

struct sx9500_platform_data {
  int gpioNirq;
  int gpio_en_rf_touch;
  int gpio_cond_n;
};

#endif
