/*
 * sec_adc.h
 * Samsung Mobile Charger Header, for adc manipulation
 *
 * Copyright (C) 2019 Samsung Electronics, Inc.
 *
 * This program is free software: you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 *
 */

#ifndef __SEC_ADC_H
#define __SEC_ADC_H __FILE__

#include <linux/iio/consumer.h>
#include "sec_battery.h"
#include "sec_charging_common.h"

#define RETRY_CNT 3

#if defined(CONFIG_MTK_AUXADC)
extern int IMM_GetOneChannelValue(int dwChannel, int data[4], int *rawdata);
#endif

#endif /* __SEC_ADC_H */
