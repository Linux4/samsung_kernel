/*
 * Copyright (c) [2009-2013] Marvell International Ltd. and its affiliates.
 * All rights reserved.
 * This software file (the "File") is owned and distributed by Marvell
 * International Ltd. and/or its affiliates ("Marvell") under the following
 * licensing terms.
 * If you received this File from Marvell, you may opt to use, redistribute
 * and/or modify this File in accordance with the terms and conditions of
 * the General Public License Version 2, June 1991 (the "GPL License"), a
 * copy of which is available along with the File in the license.txt file
 * or by writing to the Free Software Foundation, Inc., 59 Temple Place,
 * Suite 330, Boston, MA 02111-1307 or on the worldwide web at
 * http://www.gnu.org/licenses/gpl.txt. THE FILE IS DISTRIBUTED AS-IS,
 * WITHOUT WARRANTY OF ANY KIND, AND THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY OR FITNESS FOR A PARTICULAR PURPOSE ARE EXPRESSLY
 * DISCLAIMED. The GPL License provides additional details about this
 * warranty disclaimer.
 */

#ifndef _OSA_DRV_H_
#define _OSA_DRV_H_

#include <osa.h>

struct osa_pseudo_drv_ops {
	osa_err_t(*open) (int8_t *, int32_t);
	osa_err_t(*close) (osa_drv_t);
	osa_err_t(*ioctl) (osa_drv_t, int32_t, ulong_t);
};

/* called by OS */
extern osa_err_t osa_pseudo_drv_init(void);
extern osa_err_t osa_pseudo_drv_cleanup(void);

/* called by OS or driver */
extern osa_err_t osa_pseudo_drv_reg(int8_t *name,
					struct osa_pseudo_drv_ops *ops,
					int32_t flags);
extern osa_err_t osa_pseudo_drv_unreg(int8_t *name);

/* called by middle-ware */
extern osa_drv_t osa_pseudo_drv_open(int8_t *name, int32_t flags);
extern osa_err_t osa_pseudo_drv_close(osa_drv_t drv);
extern osa_err_t osa_pseudo_drv_ioctl(osa_drv_t drv, int32_t cmd, ulong_t arg);

#endif /* _OSA_DRV_H_ */
