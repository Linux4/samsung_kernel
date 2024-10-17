/*
 * Copyright (c) 2020 Samsung Electronics Co., Ltd
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#include <linux/pm_qos.h>

struct ucc_req {
	char			name[20];
	bool			active;
	int			value;
	struct list_head	list;
};

enum ufc_ctrl_type {
	PM_QOS_MIN_LIMIT = 0,
	PM_QOS_MAX_LIMIT,
	PM_QOS_OVER_LIMIT,
	PM_QOS_LITTLE_MAX_LIMIT,
	PM_QOS_MIN_LIMIT_WO_BOOST,
	CTRL_TYPE_END,
};

enum ufc_user_type {
	USERSPACE,
	UFC_INPUT,
	FINGER,
	ARGOS,
	USER_TYPE_END,
};

#if IS_ENABLED(CONFIG_EXYNOS_UFCC)
extern void ucc_add_request(struct ucc_req *req, int value);
extern void ucc_update_request(struct ucc_req *req, int value);
extern void ucc_remove_request(struct ucc_req *req);
extern int ufc_update_request(enum ufc_user_type user, enum ufc_ctrl_type type,
		int freq);

#else
static inline void ucc_add_request(struct ucc_req *req, int value) { }
static inline void ucc_update_request(struct ucc_req *req, int value) { }
static inline void ucc_remove_request(struct ucc_req *req) { }
static inline int ufc_update_request(enum ufc_user_type user, enum ufc_ctrl_type type,
		int freq) { return 0; }
#endif
