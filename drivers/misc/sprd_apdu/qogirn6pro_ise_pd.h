/* SPDX-License-Identifier: GPL-2.0 */
#ifndef __QOGIRN6PRO_ISE_PD_H__
#define __QOGIRN6PRO_ISE_PD_H__

long qogirn6pro_ise_pd_status_check(void *apdu_dev);
long qogirn6pro_ise_cold_power_on(void *apdu_dev);
long qogirn6pro_ise_full_power_down(void *apdu_dev);
long qogirn6pro_ise_hard_reset(void *apdu_dev);
long qogirn6pro_ise_soft_reset(void *apdu_dev);
long qogirn6pro_ise_hard_reset_set(void *apdu_dev);
long qogirn6pro_ise_hard_reset_clr(void *apdu_dev);

#endif

