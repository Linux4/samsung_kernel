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
 * Filename     : osa_list.h
 * Author       : Dafu Lv
 * Date Created : 21/03/08
 * Description  : This is the header file of list-related functions in osa.
 *
 */

#ifndef _OSA_LIST_H_
#define _OSA_LIST_H_

#include "osa.h"

#define OSA_LIST(head) \
	struct osa_list head = { &(head), &(head) }

#define OSA_LIST_HEAD(head) { &(head), &(head) }

#define OSA_LIST_ENTRY(_addr, _type, _member, _entry)   do {           \
		_type _tmp;                                                    \
		ulong_t _offset = ((ulong_t)(&_tmp._member) - (ulong_t)&_tmp); \
		_entry = (_type *)((ulong_t)_addr - _offset);                  \
} while (0)

#define osa_list_iterate(head, entry) \
	for ((entry) = (head)->next; (entry) != (head); (entry) = (entry)->next)

#define osa_list_iterate_safe(head, entry, n) \
	for (entry = (head)->next, n = entry->next; entry != (head); \
		entry = n, n = entry->next)

extern void osa_list_init_head(struct osa_list *head);
extern void osa_list_add(struct osa_list *entry, struct osa_list *head);
extern void osa_list_add_tail(struct osa_list *entry, struct osa_list *head);
extern void osa_list_del(struct osa_list *entry);
extern bool osa_list_empty(struct osa_list *head);

#endif /* _OSA_LIST_H_ */
