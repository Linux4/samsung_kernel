/*
 * Copyright (C) 2021 Samsung Electronics
 *
 * This program is free software; you can redistribute  it and/or modify it
 * under  the terms of  the GNU General  Public License as published by the
 * Free Software Foundation;  either version 2 of the  License, or (at your
 * option) any later version.
 */

#ifndef __CAMELLIA_PM_H__
#define __CAMELLIA_PM_H__

uint32_t strong_power_on(void);
uint32_t strong_power_off(void);

int camellia_pm_init(void);
void camellia_pm_deinit(void);

#endif
