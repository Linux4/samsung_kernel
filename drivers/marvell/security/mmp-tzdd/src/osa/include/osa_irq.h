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
 * Filename     : osa_irq.h
 * Author       : Dafu Lv
 * Date Created : 21/03/08
 * Description  : This is the header file of irq-related functions in osa layer.
 *
 */

#ifndef _OSA_IRQ_H_
#define _OSA_IRQ_H_

#include <osa.h>

#define OSA_IRQ_INVALID     ((osa_irq_t)(0xFFFFFFFF))

struct osa_irq_param {
	uint32_t flag;
	uint32_t priority;
};

extern void osa_disable_irq(void);
extern void osa_enable_irq(void);

extern void osa_save_irq(uint32_t *flags);
extern void osa_restore_irq(uint32_t *flags);

extern osa_irq_t osa_hook_irq(int32_t irq,
		void (*isr) (void *), void (*dsr) (void *),
		void *arg, struct osa_irq_param *param);
extern osa_err_t osa_free_irq(osa_irq_t handle);
extern void osa_mask_irq(uint32_t irq);
extern void osa_unmask_irq(uint32_t irq);

#endif /* _OSA_IRQ_H_ */
