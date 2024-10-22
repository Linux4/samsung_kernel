// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2021 MediaTek Inc.
 */

#include "ksym.h"
#include <mrdump_helper.h>

DEBUG_SET_LEVEL(DEBUG_LEVEL_ERR);

static void mkp_addr_find_krn_info(unsigned long *stext,
	unsigned long *etext, unsigned long *init_begin)
{
	*stext = aee_get_stext();
	*etext = aee_get_etext();
	*init_begin = aee_get_init_begin();
}
void mkp_get_krn_info(void **p_stext, void **p_etext,
	void **p__init_begin)
{
	unsigned long stext, etext, init_begin;

	mkp_addr_find_krn_info(&stext, &etext, &init_begin);
	*p_stext = (void *)stext;
	*p_etext = (void *)etext;
	*p__init_begin = (void *)init_begin;

	MKP_DEBUG("_stext: %p, _etext: %p\n", *p_stext, *p_etext);
	MKP_DEBUG(" __init_begin: %p\n", *p__init_begin);
}

void mkp_get_krn_code(void **p_stext, void **p_etext)
{
	if (*p_stext && *p_etext)
		return;

	*p_stext = (void *)aee_get_stext();
	*p_etext = (void *)aee_get_etext();

	if (!(*p_etext)) {
		MKP_ERR("%s: _stext not found\n", __func__);
		return;
	}
	if (!(*p_etext)) {
		MKP_ERR("%s: _etext not found\n", __func__);
		return;
	}
	MKP_DEBUG("_stext: %p, _etext: %p\n", *p_stext, *p_etext);
}

void mkp_get_krn_rodata(void **p_etext, void **p__init_begin)
{
	if (*p_etext && *p__init_begin)
		return;

	*p_etext = (void *)aee_get_etext();
	*p__init_begin = (void *)aee_get_init_begin();

	if (!(*p_etext)) {
		MKP_ERR("%s: _etext not found\n", __func__);
		return;
	}
	if (!(*p__init_begin)) {
		MKP_ERR("%s: __init_begin not found\n", __func__);
		return;
	}
	MKP_DEBUG("_etext: %p, __init_begin: %p\n", *p_etext, *p__init_begin);
}
