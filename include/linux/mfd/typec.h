/**
 * typec_sprd.h - Spreadtrum Type-C header file
 *
 * Copyright (c) 2015 Spreadtrum Co., Ltd.
 *		http://www.spreadtrum.com
 *
 * Author: Miao Zhu <miao.zhu@spreadtrum.com>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2  of
 * the License as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */
#ifndef __LINUX_MDF_TYPEC_SPRD_H__
#define __LINUX_MDF_TYPEC_SPRD_H__

enum typec_status {
	TYPEC_STATUS_UNKNOWN,
	TYPEC_STATUS_DEV_CONN = 1,	/* USB Should play a role as DEVICE */
	TYPEC_STATUS_DEV_DISCONN,
	TYPEC_STATUS_HOST_CONN,			/* USB Should play a role as HOST */
	TYPEC_STATUS_HOST_DISCONN,
	/*
	 * If these features would be supported in the futrue, unmask them.
	 */
	/*
	TYPEC_STATUS_POWERED_CABLE_CONN,
	TYPEC_STATUS_POWERED_CABLE_DISCONN,
	TYPEC_STATUS_AUDIO_CABLE_CONN,
	TYPEC_STATUS_AUDIO_CABLE_DISCONN,
	 */
};

int register_typec_notifier(struct notifier_block *nb);
int unregister_typec_notifier(struct notifier_block *nb);

#endif /* __LINUX_MDF_TYPEC_SPRD_H__ */
