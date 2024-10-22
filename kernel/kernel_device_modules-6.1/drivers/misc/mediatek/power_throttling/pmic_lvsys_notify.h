/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2022 MediaTek Inc.
 */

#ifndef __MTK_LVSYS_NOTIFY_H__
#define __MTK_LVSYS_NOTIFY_H__

#include <linux/notifier.h>

#define LVSYS_F_3400	3400
#define LVSYS_F_3300	3300
#define LVSYS_F_3200	3200
#define LVSYS_F_3100	3100
#define LVSYS_F_3000	3000
#define LVSYS_F_2900    2900
#define LVSYS_F_2800	2800
#define LVSYS_F_2700	2700
#define LVSYS_F_2600	2600
#define LVSYS_F_2500	2500
#define LVSYS_R_2600    (BIT(15) | 2600)
#define LVSYS_R_2700    (BIT(15) | 2700)
#define LVSYS_R_2800    (BIT(15) | 2800)
#define LVSYS_R_2900    (BIT(15) | 2900)
#define LVSYS_R_3000    (BIT(15) | 3000)
#define LVSYS_R_3100    (BIT(15) | 3100)
#define LVSYS_R_3200    (BIT(15) | 3200)
#define LVSYS_R_3300    (BIT(15) | 3300)
#define LVSYS_R_3400    (BIT(15) | 3400)
#define LVSYS_R_3500	(BIT(15) | 3500)

int lvsys_register_notifier(struct notifier_block *nb);
int lvsys_unregister_notifier(struct notifier_block *nb);

#endif /* __MTK_LVSYS_NOTIFY_H__ */
