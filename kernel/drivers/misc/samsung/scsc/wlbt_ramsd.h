/****************************************************************************
 *
 * Copyright (c) 2014 - 2018 Samsung Electronics Co., Ltd. All rights reserved
 *
 ****************************************************************************/

#ifndef __WLBT_RAMSD_H__
#define __WLBT_RAMSD_H__

void wlbt_ramsd_set_ramrp_address(unsigned long size, unsigned long offset);
int wlbt_ramsd_create(void);
int wlbt_ramsd_destroy(void);
#endif /* SCSC_log_collect_MMAP_H */
