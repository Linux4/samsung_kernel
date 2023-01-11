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

#ifndef _TEE_MEMM_H_
#define _TEE_MEMM_H_
typedef void *tee_memm_ss_t;
typedef struct _tee_mem_page_t {
	void *phy_addr;
	size_t len;
	uint32_t cacheable;
	uint32_t rsvd;
} tee_mem_page_t;
tee_memm_ss_t tee_memm_create_ss(void);
void tee_memm_destroy_ss(tee_memm_ss_t tee_memm_ss);
tee_stat_t tee_memm_set_phys_pages(tee_memm_ss_t tee_memm_ss,
				   void *virt, uint32_t size,
				   tee_mem_page_t *pages, uint32_t *pages_num);
tee_stat_t tee_memm_get_user_mem(tee_memm_ss_t tee_memm_ss, void __user *virt,
					uint32_t size, void __kernel **kvirt);
void tee_memm_put_user_mem(tee_memm_ss_t tee_memm_ss, void *vm);

#endif /* _TEE_MEMM_H_ */
