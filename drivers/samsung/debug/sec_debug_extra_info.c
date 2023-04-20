/*
 * sec_debug_extra_info.c
 *
 * Copyright (c) 2019 Samsung Electronics Co., Ltd
 *              http://www.samsung.com
 *
 *  This program is free software; you can redistribute  it and/or modify it
 *  under  the terms of  the GNU General  Public License as published by the
 *  Free Software Foundation;  either version 2 of the  License, or (at your
 *  option) any later version.
 *
 */

#include <linux/kernel.h>
#include <linux/string.h>
#include <linux/sched.h>
#include <linux/proc_fs.h>
#include <linux/seq_file.h>
#include <linux/sched/clock.h>
#include <linux/sec_debug.h>
#include <asm/stacktrace.h>

#include "sec_debug_internal.h"

#define SDEI_DEBUG	(0)
#define EXTRA_VERSION	"RI25"

#define V3_TEST_PHYS_ADDR		(0x91700000)
#define OFFSET_SHARED_BUFFER		(0xC00)

#define MAX_EXTRA_INFO_HDR_LEN	6
#define SEC_DEBUG_BADMODE_MAGIC	0x6261646d

#define SEC_DEBUG_SHARED_MAGIC0 0xFFFFFFFF
#define SEC_DEBUG_SHARED_MAGIC1 0x95308180
#define SEC_DEBUG_SHARED_MAGIC2 0x14F014F0
#define SEC_DEBUG_SHARED_MAGIC3 0x00010001

#include "sec_debug_extra_info_keys.c"

static bool exin_ready;
static struct sec_debug_shared_buffer *sh_buf;
static void *slot_end_addr;

/* get dram info from bootloader by cmdline */
#define MAX_DRAMINFO	15
static char dram_info[MAX_DRAMINFO + 1];

/*****************************************************************/
/*                        UNIT FUNCTIONS                         */
/*****************************************************************/
static int get_val_len(const char *s)
{
	if (s)
		return strnlen(s, MAX_ITEM_VAL_LEN);

	return 0;
}

static int get_key_len(const char *s)
{
	return strnlen(s, MAX_ITEM_KEY_LEN);
}

static void *get_slot_addr(int idx)
{
	return (void *)phys_to_virt(sh_buf->sec_debug_sbidx[idx].paddr);
}

static int get_max_len(void *p)
{
	int i;

	if (p < get_slot_addr(SLOT_32) || (p >= slot_end_addr)) {
		pr_crit("%s: addr(%p) is not in range\n", __func__, p);

		return 0;
	}

	for (i = SLOT_32 + 1; i < NR_SLOT; i++)
		if (p < get_slot_addr(i))
			return sh_buf->sec_debug_sbidx[i - 1].size;

	return sh_buf->sec_debug_sbidx[SLOT_END].size;
}

static void *__get_item(int slot, unsigned int idx)
{
	void *p, *base;
	unsigned int size, nr;

	size = sh_buf->sec_debug_sbidx[slot].size;
	nr = sh_buf->sec_debug_sbidx[slot].nr;

	if (nr <= idx) {
		pr_crit("%s: SLOT %d has %d items (%d)\n",
					__func__, slot, nr, idx);

		return NULL;
	}

	base = get_slot_addr(slot);
	p = (void *)(base + (idx * size));

	return p;
}

static void *search_item_by_key(const char *key, int start, int end)
{
	int s, i, keylen = get_key_len(key);
	void *p;
	char *keyname;
	unsigned int max;

	if (!sh_buf || !exin_ready) {
		pr_info("%s: (%s) extra_info is not ready\n", __func__, key);

		return NULL;
	}

	for (s = start; s < end; s++) {
		if (sh_buf->sec_debug_sbidx[s].cnt)
			max = sh_buf->sec_debug_sbidx[s].cnt;
		else
			max = sh_buf->sec_debug_sbidx[s].nr;

		for (i = 0; i < max; i++) {
			p = __get_item(s, i);
			if (!p)
				break;

			keyname = (char *)p;
			if (keylen != get_key_len(keyname))
				continue;

			if (!strncmp(keyname, key, keylen))
				return p;
		}
	}

	return NULL;
}

static void *get_item(const char *key)
{
	return search_item_by_key(key, SLOT_32, NR_MAIN_SLOT);
}

static void *get_bk_item(const char *key)
{
	return search_item_by_key(key, SLOT_BK_32, NR_SLOT);
}

static char *get_item_val(const char *key)
{
	void *p;

	p = get_item(key);
	if (!p) {
		pr_crit("%s: no key %s\n", __func__, key);

		return NULL;
	}

	return ((char *)p + MAX_ITEM_KEY_LEN);
}

char *get_bk_item_val(const char *key)
{
	void *p;

	p = get_bk_item(key);
	if (!p)
		return NULL;

	return ((char *)p + MAX_ITEM_KEY_LEN);
}

void get_bk_item_val_as_string(const char *key, char *buf)
{
	void *v;
	int len;

	v = get_bk_item_val(key);
	if (v) {
		len = get_val_len(v);
		if (len)
			memcpy(buf, v, len);
	}
}

static int is_key_in_blacklist(const char *key)
{
	char blkey[][MAX_ITEM_KEY_LEN] = {
		"KTIME", "BAT", "ODR", "DDRID",
		"PSTIE", "ASB",
	};

	int nr_blkey, keylen, i;

	keylen = get_key_len(key);
	nr_blkey = ARRAY_SIZE(blkey);

	for (i = 0; i < nr_blkey; i++)
		if (!strncmp(key, blkey[i], keylen))
			return 1;

	return 0;
}

static int is_key_in_once_list(const char *s, const char *key)
{
	char blkey[][MAX_ITEM_KEY_LEN] = {
		"SPCNT", "HLFREQ",
	};

	int nr_blkey, val_len, i;
	int ret = 0;

	val_len = get_val_len(s);
	nr_blkey = ARRAY_SIZE(blkey);

	for (i = 0; i < nr_blkey; i++) {
		if (!strncmp(key, blkey[i], strlen(key)))
			ret++;
	}

	if (!ret)
		return 0;

	for (i = 0; i < nr_blkey; i++) {
		if (strnstr(s, blkey[i], val_len))
			return 1;
	}

	return 0;
}

static DEFINE_SPINLOCK(keyorder_lock);

static void set_key_order(const char *key)
{
	void *p;
	char *v;
	char tmp[MAX_ITEM_VAL_LEN] = {0, };
	int max = MAX_ITEM_VAL_LEN;
	int len_prev, len_remain, len_this;

	if (is_key_in_blacklist(key))
		return;

	spin_lock(&keyorder_lock);

	p = get_item("ODR");
	if (!p) {
		pr_crit("%s: fail to find %s\n", __func__, key);

		goto unlock_keyorder;
	}

	max = get_max_len(p);
	if (!max) {
		pr_crit("%s: fail to get max len %s\n", __func__, key);

		goto unlock_keyorder;
	}

	v = get_item_val(p);

	if (is_key_in_once_list(v, key))
		 goto unlock_keyorder;

	/* keep previous value */
	len_prev = get_val_len(v);
	if ((!len_prev) || (len_prev >= MAX_ITEM_VAL_LEN))
		len_prev = MAX_ITEM_VAL_LEN - 1;

	snprintf(tmp, len_prev + 1, "%s", v);

	/* calculate the remained size */
	len_remain = max;

	/* get_item_val returned address without key */
	len_remain -= MAX_ITEM_KEY_LEN;

	/* need comma */
	len_this = get_key_len(key) + 1;
	len_remain -= len_this;

	/* put last key at the first of ODR */
	/* +1 to add NULL (by snprintf) */
	snprintf(v, len_this + 1, "%s,", key);

	/* -1 to remove NULL between KEYS */
	/* +1 to add NULL (by snprintf) */
	snprintf((char *)(v + len_this), len_remain + 1, "%s", tmp);

unlock_keyorder:
	spin_unlock(&keyorder_lock);
}

static void set_item_val(const char *key, const char *fmt, ...)
{
	va_list args;
	void *p;
	char *v;
	int max = MAX_ITEM_VAL_LEN;

	p = get_item(key);
	if (!p) {
		pr_crit("%s: fail to find %s\n", __func__, key);

		return;
	}

	max = get_max_len(p);
	if (!max) {
		pr_crit("%s: fail to get max len %s\n", __func__, key);

		return;
	}

	v = get_item_val(p);
	if (!get_val_len(v)) {
		va_start(args, fmt);
		vsnprintf(v, max - MAX_ITEM_KEY_LEN, fmt, args);
		va_end(args);

		set_key_order(key);
	}
}

static void __init set_bk_item_val(const char *key, int slot, const char *fmt, ...)
{
	va_list args;
	void *p;
	char *v;
	int max = MAX_ITEM_VAL_LEN;
	unsigned int cnt;

	p = get_bk_item(key);
	if (!p) {
		pr_crit("%s: fail to find %s\n", __func__, key);

		if (slot > SLOT_MAIN_END) {
			cnt = sh_buf->sec_debug_sbidx[slot].cnt;
			sh_buf->sec_debug_sbidx[slot].cnt++;

			p = __get_item(slot, cnt);
			if (!p) {
				pr_crit("%s: slot%2d cnt: %d, fail\n", __func__, slot, cnt);

				return;
			}

			snprintf((char *)p, get_key_len(key) + 1, "%s", key);

			v = ((char *)p + MAX_ITEM_KEY_LEN);

			pr_crit("%s: add slot%2d cnt: %d (%s)\n", __func__, slot, cnt, (char *)p);

			goto set_val;
		}

		return;
	}

	max = get_max_len(p);
	if (!max) {
		pr_crit("%s: fail to get max len %s\n", __func__, key);

		if (slot > SLOT_MAIN_END) {
			max = MAX_ITEM_VAL_LEN;
		} else {
			pr_crit("%s: slot(%d) is not in bk slot\n", __func__, slot);

			return;
		}
	}

	v = get_bk_item_val(p);
	if (!v) {
		pr_crit("%s: fail to find value address for %s\n", __func__, key);
		return;
	}

	if (get_val_len(v)) {
		pr_crit("%s: some value is in %s\n", __func__, key);
		return;
	}

set_val:
	va_start(args, fmt);
	vsnprintf(v, max, fmt, args);
	va_end(args);
}

static void clear_item_val(const char *key)
{
	void *p;
	int max_len;

	p = get_item(key);
	if (!p) {
		pr_crit("%s: fail to find %s\n", __func__, key);

		return;
	}

	max_len = get_max_len(p);
	if (!max_len) {
		pr_crit("%s: fail to get max len %s\n", __func__, key);

		return;
	}

	memset(get_item_val(p), 0, max_len - MAX_ITEM_KEY_LEN);
}

static void dump_slot(int type, void *ptr)
{
	struct seq_file *m;
	unsigned int cnt, i;
	char *p, *v;

	if (ptr)
		m = (struct seq_file *)ptr;
	else
		m = NULL;

	if (!exin_ready) {
		pr_crit("%s: EXIN is not ready\n", __func__);
		return;
	}

	/* temporally for backup slot */
	cnt = sh_buf->sec_debug_sbidx[type].cnt;

	for (i = 0; i < cnt; i++) {
		p = __get_item(type, i);
		if (!p)
			break;

		v = p + MAX_ITEM_KEY_LEN;

		if (m)
			seq_printf(m, "%s: %s - %s\n", __func__, p, v);
		else
			pr_crit("%s: %s - %s\n", __func__, p, v);
	}
}

static void dump_slots(int start, int end, void *ptr)
{
	int i;

	for (i = start; i < end; i++)
		dump_slot(i, ptr);
}

static void __init dump_all_keys(void)
{
	void *p;
	int s, i;
	unsigned int cnt;

	if (!exin_ready) {
		pr_crit("%s: EXIN is not ready\n", __func__);

		return;
	}

	for (s = 0; s < NR_SLOT; s++) {
		cnt = sh_buf->sec_debug_sbidx[s].cnt;

		for (i = 0; i < cnt; i++) {
			p = __get_item(s, i);
			if (!p) {
				pr_crit("%s: %d/%d: no item %p\n", __func__, s, i, p);
				break;
			}

			if (!get_key_len(p))
				break;

			pr_crit("%s: [%d][%02d] key %s\n",
							__func__, s, i, (char *)p);
		}
	}
}

static void __init init_shared_buffer(int type, int nr_keys, void *ptr)
{
	char (*keys)[MAX_ITEM_KEY_LEN];
	unsigned int base, size, nr;
	void *addr;
	int i;

	keys = (char (*)[MAX_ITEM_KEY_LEN])ptr;

	base = sh_buf->sec_debug_sbidx[type].paddr;
	size = sh_buf->sec_debug_sbidx[type].size;
	nr = sh_buf->sec_debug_sbidx[type].nr;

	addr = phys_to_virt(base);
	memset(addr, 0, size * nr);

	pr_crit("%s: SLOT%d: nr keys: %d\n", __func__, type, nr_keys);

	for (i = 0; i < nr_keys; i++) {
		/* NULL is considered as +1 */
		snprintf((char *)addr, get_key_len(keys[i]) + 1, "%s", keys[i]);

		base += size;
		addr = phys_to_virt(base);
	}

	sh_buf->sec_debug_sbidx[type].cnt = i;
}

static void __init sec_debug_extra_info_key_init(void)
{
	int nr_keys;

	nr_keys = ARRAY_SIZE(key32);
	init_shared_buffer(SLOT_32, nr_keys, (void *)key32);

	nr_keys = ARRAY_SIZE(key64);
	init_shared_buffer(SLOT_64, nr_keys, (void *)key64);

	nr_keys = ARRAY_SIZE(key256);
	init_shared_buffer(SLOT_256, nr_keys, (void *)key256);

	nr_keys = ARRAY_SIZE(key1024);
	init_shared_buffer(SLOT_1024, nr_keys, (void *)key1024);
}

static void __init sec_debug_extra_info_copy_shared_buffer(bool mflag)
{
	int i;
	unsigned int total_size = 0, slot_base;
	char *backup_base;

	for (i = 0; i < NR_MAIN_SLOT; i++)
		total_size += sh_buf->sec_debug_sbidx[i].nr * sh_buf->sec_debug_sbidx[i].size;

	slot_base = sh_buf->sec_debug_sbidx[SLOT_32].paddr;

	backup_base = phys_to_virt(slot_base + total_size);

	pr_crit("%s: dst: %p src: %p (%x)\n",
				__func__, backup_base, phys_to_virt(slot_base), total_size);

	memcpy(backup_base, phys_to_virt(slot_base), total_size);

	/* backup shared buffer header info */
	memcpy(&(sh_buf->sec_debug_sbidx[SLOT_BK_32]),
				&(sh_buf->sec_debug_sbidx[SLOT_32]),
				sizeof(struct sec_debug_sb_index) * (NR_SLOT - NR_MAIN_SLOT));

	for (i = SLOT_BK_32; i < NR_SLOT; i++) {
		sh_buf->sec_debug_sbidx[i].paddr += total_size;
		if (SDEI_DEBUG) {
			pr_crit("%s: SLOT %2d: paddr: 0x%x\n",
					__func__, i, sh_buf->sec_debug_sbidx[i].paddr);
			pr_crit("%s: SLOT %2d: size: %d\n",
					__func__, i, sh_buf->sec_debug_sbidx[i].size);
			pr_crit("%s: SLOT %2d: nr: %d\n",
					__func__, i, sh_buf->sec_debug_sbidx[i].nr);
			pr_crit("%s: SLOT %2d: cnt: %d\n",
					__func__, i, sh_buf->sec_debug_sbidx[i].cnt);
		}
	}
}

static void __init sec_debug_extra_info_dump_sb_index(void)
{
	int i;

	for (i = 0; i < NR_SLOT; i++) {
		pr_info("%s: SLOT%02d: paddr: %x\n",
					__func__, i, sh_buf->sec_debug_sbidx[i].paddr);
		pr_info("%s: SLOT%02d: cnt: %d\n",
					__func__, i, sh_buf->sec_debug_sbidx[i].cnt);
		pr_info("%s: SLOT%02d: blmark: %lx\n",
					__func__, i, sh_buf->sec_debug_sbidx[i].blmark);
		pr_info("\n");
	}
}

static void __init sec_debug_init_extra_info_sbidx(int type, struct sec_debug_sb_index idx, bool mflag)
{
	sh_buf->sec_debug_sbidx[type].paddr = idx.paddr;
	sh_buf->sec_debug_sbidx[type].size = idx.size;
	sh_buf->sec_debug_sbidx[type].nr = idx.nr;
	sh_buf->sec_debug_sbidx[type].cnt = idx.cnt;
	sh_buf->sec_debug_sbidx[type].blmark = 0;

	pr_crit("%s: slot: %d / paddr: 0x%x / size: %d / nr: %d\n",
					__func__, type,
					sh_buf->sec_debug_sbidx[type].paddr,
					sh_buf->sec_debug_sbidx[type].size,
					sh_buf->sec_debug_sbidx[type].nr);
}

static bool __init sec_debug_extra_info_check_magic(void)
{
	if (sh_buf->magic[0] != SEC_DEBUG_SHARED_MAGIC0)
		return false;

	if (sh_buf->magic[1] != SEC_DEBUG_SHARED_MAGIC1)
		return false;

	if (sh_buf->magic[2] != SEC_DEBUG_SHARED_MAGIC2)
		return false;

	return true;
}

static void __init sec_debug_extra_info_buffer_init(void)
{
	unsigned long tmp_addr;
	struct sec_debug_sb_index tmp_idx;
	bool flag_valid = false;

	flag_valid = sec_debug_extra_info_check_magic();

	if (SDEI_DEBUG)
		sec_debug_extra_info_dump_sb_index();

	tmp_idx.cnt = 0;
	tmp_idx.blmark = 0;

	/* SLOT_32, 32B, 64 items */
	tmp_addr = secdbg_base_get_buf_base(SDN_MAP_EXTRA_INFO);
	tmp_idx.paddr = (unsigned int)tmp_addr;
	tmp_idx.size = 32;
	tmp_idx.nr = 64;
	sec_debug_init_extra_info_sbidx(SLOT_32, tmp_idx, flag_valid);

	/* SLOT_64, 64B, 64 items */
	tmp_addr += tmp_idx.size * tmp_idx.nr;
	tmp_idx.paddr = (unsigned int)tmp_addr;
	tmp_idx.size = 64;
	tmp_idx.nr = 64;
	sec_debug_init_extra_info_sbidx(SLOT_64, tmp_idx, flag_valid);

	/* SLOT_256, 256B, 16 items */
	tmp_addr += tmp_idx.size * tmp_idx.nr;
	tmp_idx.paddr = (unsigned int)tmp_addr;
	tmp_idx.size = 256;
	tmp_idx.nr = 16;
	sec_debug_init_extra_info_sbidx(SLOT_256, tmp_idx, flag_valid);

	/* SLOT_1024, 1024B, 16 items */
	tmp_addr += tmp_idx.size * tmp_idx.nr;
	tmp_idx.paddr = (unsigned int)tmp_addr;
	tmp_idx.size = 1024;
	tmp_idx.nr = 16;
	sec_debug_init_extra_info_sbidx(SLOT_1024, tmp_idx, flag_valid);

	/* backup shared buffer contents */
	sec_debug_extra_info_copy_shared_buffer(flag_valid);

	sec_debug_extra_info_key_init();

	if (SDEI_DEBUG)
		dump_all_keys();

	slot_end_addr = (void *)phys_to_virt(sh_buf->sec_debug_sbidx[SLOT_END].paddr +
				((phys_addr_t)(sh_buf->sec_debug_sbidx[SLOT_END].size) *
				 (phys_addr_t)(sh_buf->sec_debug_sbidx[SLOT_END].nr)));
}

#define MAX_EXTRA_INFO_LEN	(MAX_ITEM_KEY_LEN + MAX_ITEM_VAL_LEN)

static void sec_debug_store_extra_info(char (*keys)[MAX_ITEM_KEY_LEN], int nr_keys, char *ptr)
{
	int i;
	unsigned long len, max_len;
	void *p;
	char *v, *start_addr = ptr;

	memset(ptr, 0, SZ_1K);

	for (i = 0; i < nr_keys; i++) {
		p = get_bk_item(keys[i]);
		if (!p) {
			pr_crit("%s: no key: %s\n", __func__, keys[i]);

			continue;
		}

		v = p + MAX_ITEM_KEY_LEN;

		/* get_key_len returns length of the key + 1 */
		len = (unsigned long)ptr + strlen(p) + get_val_len(v)
				+ MAX_EXTRA_INFO_HDR_LEN;

		max_len = (unsigned long)start_addr + SZ_1K;

		if (len > max_len)
			break;

		ptr += snprintf(ptr, MAX_EXTRA_INFO_LEN, "\"%s\":\"%s\"", (char *)p, v);

		if ((i + 1) != nr_keys)
			ptr += sprintf(ptr, ",");
	}

	pr_info("%s: %s\n", __func__, ptr);
}

/******************************************************************************
 * sec_debug_extra_info details
 *
 *	etr_a : basic reset information
 *	etr_b : basic reset information
 *	etr_c : hard-lockup information (callstack)
 *	etr_m : mfc error information
 *
 ******************************************************************************/
void secdbg_exin_get_extra_info_A(char *ptr)
{
	int nr_keys;

	nr_keys = ARRAY_SIZE(akeys);

	sec_debug_store_extra_info(akeys, nr_keys, ptr);
}

void secdbg_exin_get_extra_info_B(char *ptr)
{
	int nr_keys;

	nr_keys = ARRAY_SIZE(bkeys);

	sec_debug_store_extra_info(bkeys, nr_keys, ptr);
}

void secdbg_exin_get_extra_info_C(char *ptr)
{
	int nr_keys;

	nr_keys = ARRAY_SIZE(ckeys);

	sec_debug_store_extra_info(ckeys, nr_keys, ptr);
}

void secdbg_exin_get_extra_info_M(char *ptr)
{
	int nr_keys;

	nr_keys = ARRAY_SIZE(mkeys);

	sec_debug_store_extra_info(mkeys, nr_keys, ptr);
}

void secdbg_exin_get_extra_info_F(char *ptr)
{
	int nr_keys;

	nr_keys = ARRAY_SIZE(fkeys);

	sec_debug_store_extra_info(fkeys, nr_keys, ptr);
}

static void __init sec_debug_set_extra_info_id(void)
{
	struct timespec ts;

	getnstimeofday(&ts);

	set_bk_item_val("ID", SLOT_BK_32, "%09lu%s", ts.tv_nsec, EXTRA_VERSION);

	set_item_val("ASB", "%d", id_get_asb_ver());
	set_item_val("PSITE", "%d", id_get_product_line());
	set_item_val("DDRID", "%s", dram_info);
}

static void secdbg_exin_set_ktime(void)
{
	u64 ts_nsec;

	ts_nsec = local_clock();
	do_div(ts_nsec, 1000000000);

	set_item_val("KTIME", "%lu", (unsigned long)ts_nsec);
}

void secdbg_exin_set_fault(enum secdbg_exin_fault_type type,
				    unsigned long addr, struct pt_regs *regs)
{

	phys_addr_t paddr = 0;

	if (regs) {
		set_item_val("FAULT", "0x%lx", addr);
		set_item_val("PC", "%pS", regs->pc);
		set_item_val("LR", "%pS",
					 compat_user_mode(regs) ?
					 regs->compat_lr : regs->regs[30]);

		if (type == UNDEF_FAULT && addr >= kimage_voffset) {
			paddr = virt_to_phys((void *)addr);

			pr_crit("%s: 0x%x / 0x%x\n", __func__,
				upper_32_bits(paddr), lower_32_bits(paddr));
//			exynos_pmu_write(EXYNOS_PMU_INFORM8, lower_32_bits(paddr));
//			exynos_pmu_write(EXYNOS_PMU_INFORM9, upper_32_bits(paddr));
		}
	}
}

void secdbg_exin_set_bug(const char *file, unsigned int line)
{
	set_item_val("BUG", "%s:%u", file, line);
}

void secdbg_exin_set_panic(char *str)
{
	if (strstr(str, "\nPC is at"))
		strcpy(strstr(str, "\nPC is at"), "");

	set_item_val("PANIC", "%s", str);
}

void secdbg_exin_set_backtrace(struct pt_regs *regs)
{
	char fbuf[MAX_ITEM_VAL_LEN];
	char buf[64];
	struct stackframe frame;
	int offset = 0;
	int sym_name_len;
	char *v;

	v = get_item_val("STACK");
	if (!v) {
		pr_crit("%s: no STACK in items\n", __func__);
		return;
	}

	if (get_val_len(v)) {
		pr_crit("%s: already %s in STACK\n", __func__, v);
		return;
	}

	memset(fbuf, 0, MAX_ITEM_VAL_LEN);

	pr_crit("sec_debug_store_backtrace\n");

	if (regs) {
		frame.fp = regs->regs[29];
		frame.pc = regs->pc;
	} else {
		frame.fp = (unsigned long)__builtin_frame_address(0);
		frame.pc = (unsigned long)secdbg_exin_set_backtrace;
	}

	while (1) {
		unsigned long where = frame.pc;
		int ret;

		ret = unwind_frame(NULL, &frame);
		if (ret < 0)
			break;

		snprintf(buf, sizeof(buf), "%ps", (void *)where);
		sym_name_len = strlen(buf);

		if (offset + sym_name_len > MAX_ITEM_VAL_LEN)
			break;

		if (offset)
			offset += sprintf(fbuf + offset, ":");

		snprintf(fbuf + offset, MAX_ITEM_VAL_LEN - offset, "%s", buf);
		offset += sym_name_len;
	}

	set_item_val("STACK", fbuf);
}

void secdbg_exin_set_backtrace_cpu(struct pt_regs *regs, int cpu)
{
	char fbuf[MAX_ITEM_VAL_LEN];
	char key[MAX_ITEM_KEY_LEN];
	char buf[64];
	struct stackframe frame;
	int offset = 0;
	int sym_name_len;
	char *v;

	snprintf(key, 5, "CPU%d", cpu);

	v = get_item_val(key);
	if (!v) {
		pr_crit("%s: no %s in items\n", __func__, key);
		return;
	}

	if (get_val_len(v)) {
		pr_crit("%s: already %s in %s\n", __func__, v, key);
		return;
	}

	memset(fbuf, 0, MAX_ITEM_VAL_LEN);

	pr_crit("sec_debug_store_backtrace_cpu(%d)\n", cpu);

	if (regs) {
		frame.fp = regs->regs[29];
		frame.pc = regs->pc;
	} else {
		frame.fp = (unsigned long)__builtin_frame_address(0);
		frame.pc = (unsigned long)secdbg_exin_set_backtrace_cpu;
	}

	while (1) {
		unsigned long where = frame.pc;
		int ret;

		ret = unwind_frame(NULL, &frame);
		if (ret < 0)
			break;

		snprintf(buf, sizeof(buf), "%ps", (void *)where);
		sym_name_len = strlen(buf);

		if (offset + sym_name_len > MAX_ITEM_VAL_LEN)
			break;

		if (offset)
			offset += sprintf(fbuf + offset, ":");

		snprintf(fbuf + offset, MAX_ITEM_VAL_LEN - offset, "%s", buf);
		offset += sym_name_len;
	}

	set_item_val(key, fbuf);
}

void secdbg_exin_set_backtrace_task(struct task_struct *tsk)
{
	char fbuf[MAX_ITEM_VAL_LEN];
	char buf[64];
	struct stackframe frame;
	int offset = 0;
	int sym_name_len;
	char *v;

	if (!tsk) {
		pr_crit("%s: no TASK, quit\n", __func__);
		return;
	}

	if (!try_get_task_stack(tsk)) {
		pr_crit("%s: fail to get task stack, quit\n", __func__);
		return;
	}

	v = get_item_val("STACK");
	if (!v) {
		pr_crit("%s: no STACK in items\n", __func__);
		goto out;
	}

	if (get_val_len(v)) {
		pr_crit("%s: already %s in STACK\n", __func__, v);
		goto out;
	}

	memset(fbuf, 0, MAX_ITEM_VAL_LEN);

	pr_crit("sec_debug_store_backtrace_task\n");

	frame.fp = thread_saved_fp(tsk);
	frame.pc = thread_saved_pc(tsk);

	while (1) {
		unsigned long where = frame.pc;
		int ret;

		ret = unwind_frame(tsk, &frame);
		if (ret < 0)
			break;

		snprintf(buf, sizeof(buf), "%ps", (void *)where);
		sym_name_len = strlen(buf);

		if (offset + sym_name_len > MAX_ITEM_VAL_LEN)
			break;

		if (offset)
			offset += sprintf(fbuf + offset, ":");

		snprintf(fbuf + offset, MAX_ITEM_VAL_LEN - offset, "%s", buf);
		offset += sym_name_len;
	}

	set_item_val("STACK", fbuf);

out:
	put_task_stack(tsk);
}


void secdbg_exin_set_sysmmu(char *str)
{
	set_item_val("SMU", "%s", str);
}

void secdbg_exin_set_busmon(char *str)
{
	set_item_val("BUS", "%s", str);
}

void secdbg_exin_set_dpm_timeout(char *devname)
{
	set_item_val("DPM", "%s", devname);
}

void secdbg_exin_set_smpl(unsigned long count)
{
	clear_item_val("SMP");
	set_item_val("SMP", "%lu", count);
}

void secdbg_exin_set_esr(unsigned int esr)
{
	set_item_val("ESR", "%s (0x%08x)",
	esr_get_class_string(esr), esr);
}

void secdbg_exin_set_merr(char *merr)
{
	set_item_val("MER", "%s", merr);
}

void secdbg_exin_set_hint(unsigned long hint)
{
	if (hint)
		set_item_val("HINT", "%llx", hint);
}

void secdbg_exin_set_decon(unsigned int err)
{
	set_item_val("DCN", "%08x", err);
}

void secdbg_exin_set_batt(int cap, int volt, int temp, int curr)
{
	clear_item_val("BAT");
	set_item_val("BAT", "%03d/%04d/%04d/%06d", cap, volt, temp, curr);
}

void secdbg_exin_set_ufs_error(char *str)
{
	set_item_val("ETC", "%s", str);
}

void secdbg_exin_set_zswap(char *str)
{
	set_item_val("ETC", "%s", str);
}

void secdbg_exin_set_finish(void)
{
	secdbg_exin_set_ktime();
}

void secdbg_exin_set_mfc_error(char *str)
{
	clear_item_val("STACK");
	set_item_val("STACK", "MFC ERROR");
	set_item_val("MFC", "%s", str);
}

void secdbg_exin_set_aud(char *str)
{
	set_item_val("AUD", "%s", str);
}

void secdbg_exin_set_epd(char *str)
{
	set_item_val("EPD", "%s", str);
}

#define MAX_UNFZ_VAL_LEN (240)

void secdbg_exin_set_unfz(const char *comm, int pid)
{
	void *p;
	char *v;
	char tmp[MAX_UNFZ_VAL_LEN] = {0, };
	int max = MAX_UNFZ_VAL_LEN;
	int len_prev, len_remain, len_this;

	p = get_item("UNFZ");
	if (!p) {
		pr_crit("%s: fail to find %s\n", __func__, comm);

		return;
	}

	max = get_max_len(p);
	if (!max) {
		pr_crit("%s: fail to get max len %s\n", __func__, comm);

		return;
	}

	v = get_item_val(p);

	/* keep previous value */
	len_prev = get_val_len(v);
	if ((!len_prev) || (len_prev >= MAX_UNFZ_VAL_LEN))
		len_prev = MAX_UNFZ_VAL_LEN - 1;

	snprintf(tmp, len_prev + 1, "%s", v);

	/* calculate the remained size */
	len_remain = max;

	/* get_item_val returned address without key */
	len_remain -= MAX_ITEM_KEY_LEN;

	/* put last key at the first of ODR */
	/* +1 to add NULL (by snprintf) */
	if (pid < 0)
		len_this = scnprintf(v, len_remain, "%s/", comm);
	else
		len_this = scnprintf(v, len_remain, "%s:%d/", comm, pid);

	/* -1 to remove NULL between KEYS */
	/* +1 to add NULL (by snprintf) */
	snprintf((char *)(v + len_this), len_remain - len_this, "%s", tmp);
}

char *secdbg_exin_get_unfz(void)
{
	void *p;
	int max = MAX_UNFZ_VAL_LEN;

	p = get_item("UNFZ");
	if (!p) {
		pr_crit("%s: fail to find\n", __func__);

		return NULL;
	}

	max = get_max_len(p);
	if (!max) {
		pr_crit("%s: fail to get max len\n", __func__);

		return NULL;
	}

	return get_item_val(p);
}

/*********** TEST V3 **************************************/
static void test_v3(void *seqm)
{
	struct seq_file *m = (struct seq_file *)seqm;

	seq_printf(m, " -- SEC DEBUG SHARED INFO V3 (SHARED BUFFER) --\n");
	dump_slots(SLOT_32, NR_MAIN_SLOT, m);

	seq_printf(m, " -- SEC DEBUG SHARED INFO V3 (SHARED BUFFER BK) --\n");
	dump_slots(SLOT_BK_32, NR_SLOT, m);
}

/*********** TEST V3 **************************************/
static int secdbg_exin_set_debug_reset_rwc_proc_show(struct seq_file *m, void *v)
{
	char *rstcnt;

	rstcnt = get_bk_item_val("RSTCNT");
	seq_printf(m, "%s", rstcnt);

	return 0;
}

static int secdbg_exin_reset_rwc_proc_open(struct inode *inode, struct file *file)
{
	return single_open(file, secdbg_exin_set_debug_reset_rwc_proc_show, NULL);
}

static const struct file_operations secdbg_exin_reset_rwc_proc_fops = {
	.open = secdbg_exin_reset_rwc_proc_open,
	.read = seq_read,
	.llseek = seq_lseek,
	.release = single_release,
};

static int set_debug_reset_extra_info_proc_show(struct seq_file *m, void *v)
{
	char buf[SZ_1K];

	if (0)
		test_v3(m);

	secdbg_exin_get_extra_info_A(buf);
	seq_printf(m, "%s", buf);

	return 0;
}

static int sec_debug_reset_extra_info_proc_open(struct inode *inode, struct file *file)
{
	return single_open(file, set_debug_reset_extra_info_proc_show, NULL);
}

static const struct file_operations sec_debug_reset_extra_info_proc_fops = {
	.open = sec_debug_reset_extra_info_proc_open,
	.read = seq_read,
	.llseek = seq_lseek,
	.release = single_release,
};

static int __init sec_hw_param_get_dram_info(char *arg)
{
	if (strlen(arg) > MAX_DRAMINFO)
		return 0;

	memcpy(dram_info, arg, (int)strlen(arg));

	return 0;
}
early_param("androidboot.dram_info", sec_hw_param_get_dram_info);

void simulate_extra_info_force_error(unsigned int magic)
{
	if (!exin_ready) {
		pr_crit("%s: EXIN is not ready\n", __func__);
		return;
	}

	sh_buf->magic[0] = magic;
}

static int __init secdbg_extra_info_init(void)
{
	struct proc_dir_entry *entry;

	sh_buf = secdbg_base_get_debug_base(SDN_MAP_EXTRA_INFO);
	if (!sh_buf) {
		pr_err("%s: No extra info buffer\n", __func__);
		return -EFAULT;
	}

	sec_debug_extra_info_buffer_init();

	sh_buf->magic[0] = SEC_DEBUG_SHARED_MAGIC0;
	sh_buf->magic[1] = SEC_DEBUG_SHARED_MAGIC1;
	sh_buf->magic[2] = SEC_DEBUG_SHARED_MAGIC2;
	sh_buf->magic[3] = SEC_DEBUG_SHARED_MAGIC3;

	exin_ready = true;

	entry = proc_create("reset_reason_extra_info",
			    0644, NULL, &sec_debug_reset_extra_info_proc_fops);
	if (!entry)
		return -ENOMEM;

	proc_set_size(entry, SZ_1K);

	entry = proc_create("reset_rwc", S_IWUGO, NULL,
				&secdbg_exin_reset_rwc_proc_fops);

	if (!entry)
		return -ENOMEM;

	sec_debug_set_extra_info_id();

	return 0;
}
late_initcall(secdbg_extra_info_init);
