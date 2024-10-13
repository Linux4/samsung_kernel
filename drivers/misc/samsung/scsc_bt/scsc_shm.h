/****************************************************************************
 *
 *       Copyright (c) 2015 Samsung Electronics Co., Ltd
 *
 ****************************************************************************/

/* Shared memory interface API */

#ifndef __SCSC_SHM_H__
#define __SCSC_SHM_H__

#include <scsc/api/bsmhcp.h>

int scsc_bt_shm_init(void);
void scsc_bt_shm_exit(void);

ssize_t scsc_bt_shm_h4_read(struct file *file,
			    char __user *buf,
			    size_t len,
			    loff_t *offset);
ssize_t scsc_bt_shm_h4_write(struct file *file,
			     const char __user *buf,
			     size_t len, loff_t *offset);
unsigned scsc_bt_shm_h4_poll(struct file *file, poll_table *wait);

#endif
