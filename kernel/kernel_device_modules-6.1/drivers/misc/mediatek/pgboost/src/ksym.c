// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2023 MediaTek Inc.
 */

#include <inc/ksym.h>

static unsigned long *tools_ka;
static int *tools_ko;
static unsigned long *tools_krb;
static unsigned int *tools_kns;
static u8 *tools_kn;
static unsigned int *tools_km;
static u8 *tools_ktt;
static u16 *tools_kti;

void __init *tools_abt_addr(void *ssa)
{
	void *pos;
	u8 abt[SM_SIZE];
	int i;
	unsigned long s_left;

	for (i = 0; i < SM_SIZE; i++) {
		if (i & 0x1)
			abt[i] = 0;
		else
			abt[i] = 0x61 + i / 2;
	}

	if ((unsigned long)ssa >= KV + S_MAX) {
		pr_info("out of range: 0x%lx\n", (unsigned long)ssa);
		return NULL;
	}

	pos = ssa;
	s_left = KV + S_MAX - (unsigned long)ssa;
	while ((u64)pos < (u64)(KV + S_MAX)) {
		pos = memchr(pos, 'a', s_left);

		if (!pos) {
			pr_info("fail at: 0x%lx @ 0x%lx\n", (unsigned long)ssa, s_left);
			return NULL;
		}

		if (!memcmp(pos, (const void *)abt, sizeof(abt)))
			return pos;

		pos = (char *)pos + 1;
		s_left = KV + S_MAX - (unsigned long)pos;
	}

	pr_info("fail at end: 0x%lx @ 0x%lx\n", (unsigned long)ssa, s_left);
	return NULL;
}

unsigned long __init *tools_krb_addr(void)
{
	void *abt_addr = (void *)KV;
	void *ssa = (void *)KV;
	unsigned long *pos;

	while ((u64)ssa < KV + S_MAX) {
		abt_addr = tools_abt_addr(ssa);
		if (!abt_addr) {
			pr_info("krb not found: 0x%lx\n", (unsigned long)ssa);
			return NULL;
		}

		abt_addr = (void *)round_up((unsigned long)abt_addr, 8);
		for (pos = (unsigned long *)abt_addr;
			(u64)pos > (u64)ssa ; pos--) {
			/*
			 * When kallsyms_relative_base scan at _stext, we
			 * need to break. Because we already hit the start
			 * of kernel image.
			 */
			if ((u64)pos == (u64)KV)
				break;
			/*
			 * kallsyms_relative_base store the _text value.
			 * _text is equal to kimage_vaddr. If we find the
			 * value, this position is krb addr.
			 */
			if (*pos == kimage_vaddr)
				return pos;
		}
		ssa = abt_addr + 1;
	}

	pr_info("krb not found: 0x%lx\n", (unsigned long)ssa);
	return NULL;
}

unsigned int __init *tools_km_addr(void)
{
	const u8 *name = tools_kn;
	unsigned int loop = *tools_kns;

	while (loop--)
		name = name + (*name) + 1;

	return (unsigned int *)round_up((unsigned long)name, 8);
}

u16 __init *tools_kti_addr(void)
{
	const u8 *pch = tools_ktt;
	int loop = TT_SIZE;

	while (loop--) {
		for (; *pch; pch++)
			;
		pch++;
	}

	return (u16 *)round_up((unsigned long)pch, 8);
}

int __init tools_ka_init(void)
{
	unsigned int kns;

	tools_krb = tools_krb_addr();
	if (!tools_krb)
		return -EINVAL;

	tools_kns = (unsigned int *)(tools_krb + 1);
	tools_kn = (u8 *)(tools_krb + 2);
	kns = *tools_kns;
	tools_ko = (int *)(tools_krb - (round_up(kns, 2) / 2));
	tools_km = tools_km_addr();
	tools_ktt = (u8 *)round_up((unsigned long)(tools_km +
				    (round_up(kns, 256) / 256)), 8);
	tools_kti = tools_kti_addr();

	return 0;
}

unsigned int __init tools_checking_names(unsigned int off,
					   char *namebuf, size_t buflen)
{
	int len, skipped_first = 0;
	const u8 *tptr, *data;

	data = tools_kn + off;
	len = *data;
	data++;
	off += len + 1;

	while (len) {
		tptr = tools_ktt + *(tools_kti + *data);
		data++;
		len--;

		while (*tptr) {
			if (skipped_first) {
				if (buflen <= 1)
					goto tail;
				*namebuf = *tptr;
				namebuf++;
				buflen--;
			} else
				skipped_first = 1;
			tptr++;
		}
	}

tail:
	if (buflen)
		*namebuf = '\0';

	return off;
}

unsigned long __init tools_idx2addr(int idx)
{
	if (!IS_ENABLED(CONFIG_KALLSYMS_BASE_RELATIVE))
		return *(tools_ka + idx);

	if (!IS_ENABLED(CONFIG_KALLSYMS_ABSOLUTE_PERCPU))
		return *tools_krb + (u32)(*(tools_ko + idx));

	if (*(tools_ko + idx) >= 0)
		return *(tools_ko + idx);

	return *tools_krb - 1 - *(tools_ko + idx);
}

unsigned long __init tools_addr_find(const char *name)
{
	char strbuf[NAME_LEN];
	unsigned long i;
	unsigned int off;

	for (i = 0, off = 0; i < *tools_kns; i++) {
		off = tools_checking_names(off, strbuf, ARRAY_SIZE(strbuf));

		if (strcmp(strbuf, name) == 0)
			return tools_idx2addr(i);
	}
	return 0;
}
