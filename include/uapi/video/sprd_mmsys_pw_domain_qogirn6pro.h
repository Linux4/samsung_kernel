/*
 * Copyright (C) 2020 Spreadtrum Communications Inc.
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */
#ifndef _CAM_PW_DOMAIN_QOGIRN6PRO_H_
#define _CAM_PW_DOMAIN_QOGIRN6PRO_H_

#include <linux/notifier.h>

/* no need
 * int sprd_cam_pw_domain_init(struct platform_device *pdev);
 * int sprd_cam_pw_domain_deinit(void);
 */

int sprd_dcam_pw_on(void);
int sprd_dcam_pw_off(void);
int sprd_isp_pw_on(void);
int sprd_isp_pw_off(void);
int sprd_isp_blk_cfg_en(void);
int sprd_isp_blk_dis(void);


/* power on/off call back function, use as follow:
 * 1: static int ssss_event(struct notifier_block *self, unsigned long event,
 *		void *ptr) {  do something  }
 * 2: static struct notifier_block ssss_notifier = {
 *	.notifier_call = ssss_event,
 *    };
 * 3: sprd_mm_pw_notify_register(&ssss_notifier);
 */


#endif /* _CAM_PW_DOMAIN_H_ */
