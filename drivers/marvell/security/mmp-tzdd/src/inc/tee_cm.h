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

#ifndef _TEE_CM_H_
#define _TEE_CM_H_

#include "tz-addr-map.h"

typedef uint32_t tee_cm_stat;
/* initialization function, return err code */
extern tee_cm_stat tee_cm_init(void);
/* deinitialization function, return err code */
extern void tee_cm_cleanup(void);
/* write data to the other end */
extern bool tee_cm_send_data(uint8_t *buf);
/* get the size of current available data */
extern uint32_t tee_cm_get_data_size(void);
/* get the destination buffer for receiver*/
extern void tee_cm_get_recv_buf(void **head);
/* receive data */
extern void tee_cm_recv_data(uint8_t *buf);
extern void tee_cm_smi(uint32_t flag);
extern void tee_cm_get_msgm_head(void *msg_head);

/* read RB phys address */
static inline uint32_t _read_rb_phys_addr(void)
{
	return (uint32_t) RB_PHYS_ADDR;
}

/* read RB size */
static inline uint32_t _read_rb_size(void)
{
	return (uint32_t) RB_SIZE;
}

/* read RC phys address */
static inline uint32_t _read_rc_phys_addr(void)
{
	return (uint32_t) RC_PHYS_ADDR;
}

/* read RC size */
static inline uint32_t _read_rc_size(void)
{
	return (uint32_t) RC_SIZE;
}

#endif /* _TEE_CM_H_ */
