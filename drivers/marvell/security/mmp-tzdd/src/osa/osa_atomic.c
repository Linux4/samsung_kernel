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

#include <osa.h>

int osa_atomic_read(osa_atomic_t *v)
{
	return atomic_read(v);
}
OSA_EXPORT_SYMBOL(osa_atomic_read);

void osa_atomic_set(osa_atomic_t *v, int i)
{
	atomic_set(v, i);

	return;
}
OSA_EXPORT_SYMBOL(osa_atomic_set);

void osa_atomic_add(int i, osa_atomic_t *v)
{
	atomic_add(i, v);

	return;
}
OSA_EXPORT_SYMBOL(osa_atomic_add);

void osa_atomic_sub(int i, osa_atomic_t *v)
{
	atomic_sub(i, v);

	return;
}
OSA_EXPORT_SYMBOL(osa_atomic_sub);

void osa_atomic_inc(osa_atomic_t *v)
{
	atomic_inc(v);

	return;
}
OSA_EXPORT_SYMBOL(osa_atomic_inc);

void osa_atomic_dec(osa_atomic_t *v)
{
	atomic_dec(v);

	return;
}
OSA_EXPORT_SYMBOL(osa_atomic_dec);
