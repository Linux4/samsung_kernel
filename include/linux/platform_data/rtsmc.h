/* include/linux/platform_data/rtsmc.h
 * Driver to Richtek switch mdoe charger device
 *
 * Copyright (C) 2012
 * Author: Patrick Chang <weichung.chang@gmail.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef __RTSMC_H_
#define __RTSMC_H_
#include <linux/types.h>

typedef enum __ERT945xEvent
        {eRT_START_CHARING,
        eRT_STOP_CHARING,
        eRT_ATTACH_OTG,
        eRT_DETACH_OTG,
        eRT_CHARING_FAULT,
        } ERTEvent;

typedef enum __ERT945xResult
        {eRT945x_CHARGE_DONE,
        eRT945x_FAULT_CHGVINUV,
        eRT945x_FALUT_CHGSLP,
        eRT945x_FALUT_CHGOT,
        eRT945x_FALUT_CHGBATAB,
        eRT945x_FALUT_CHGBATOV,
        eRT945x_FALUT_CHGVINOV,
        eRT945x_FALUT_CHGDPM,
        eRT945x_FALUT_BSTOT,
        eRT945x_FALUT_BSTBATOV,
        eRT945x_FALUT_BSTBATUV,
        eRT945x_FALUT_BSTOLP,
        eRT945x_FALUT_BSTVINOV,
        eRT945x_START_CHARGE,
        eRT945x_STOP_CHARGE,
        } ERT945xResult;


struct rtsmc_platform_data {
        void (*rtsmc_feedback)(ERT945xResult event);
        };

void RT945x_enable_charge(int32_t en);
void RT945x_enable_otg(int32_t en);

#endif /*__RTSMC_H_*/
