/* include/linux/platform_data/rt_charger.h
 * Driver to Richtek RT8969 switch mdoe charger device
 *
 * Copyright (C) 2012
 * Author: Patrick Chang <weichung.chang@gmail.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef __RT_CHARGER_H_
#define __RT_CHARGER_H_
#include <linux/types.h>

void rt9524_disable(void);
void rt9524_setmode(int nMode);
#define RT9542_USB500_MODE	0
#define RT9542_ISET_MODE	1
#define RT9542_USB100_MODE	2
#define RT9542_FACTORY_MODE	3

#endif /*__RT_CHARGER_H_*/
