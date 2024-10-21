/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (C) 2016 MediaTek Inc.
 */

#ifndef __PORT_KERN_INTF_H__
#define __PORT_KERN_INTF_H__

#if IS_ENABLED(CONFIG_MTK_SLBC)
extern int register_ccci_sys_call_back(unsigned int id, ccci_sys_cb_func_t func);
extern int ccci_register_md_state_receiver(unsigned char ch_id,
	void (*callback)(enum MD_STATE, enum MD_STATE));

extern int slbc_gid_request(enum slc_ach_uid uid, int *gid, struct slbc_gid_data *d);
extern int slbc_gid_release(enum slc_ach_uid uid, int gid);
extern int slbc_validate(enum slc_ach_uid uid, int gid);
extern int slbc_invalidate(enum slc_ach_uid uid, int gid);
#endif

#endif	/* __PORT_KERN_INTF_H__ */
