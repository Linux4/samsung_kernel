// SPDX-License-Identifier: GPL-2.0
/*
 * Samsung Exynos SoC series dsp driver
 *
 * Copyright (c) 2019 Samsung Electronics Co., Ltd.
 *              http://www.samsung.com/
 */

#include "dl/dsp-dl-out-manager.h"
#include "dl/dsp-elf-loader.h"
#include "dl/dsp-lib-manager.h"
#include "dl/dsp-xml-parser.h"

#define DL_DL_OUT_ALIGN		(4)
#define DL_HASH_SIZE		(300)
#define DL_HASH_END		(0xFFFFFFFF)

static unsigned int *dl_hash;
static struct dsp_tlsf *out_manager;

void dsp_dl_hash_init(void)
{
	int idx;
	unsigned int *node;

	for (idx = 0; idx < DL_HASH_SIZE; idx++) {
		node = dl_hash + idx;
		*node = DL_HASH_END;
	}
}

void dsp_dl_hash_push(struct dsp_dl_out *dl_out)
{
	unsigned int key = dsp_hash_get_key(dl_out->data);

	dl_out->hash_next = dl_hash[key];
	dl_hash[key] = (unsigned long)dl_out - (unsigned long)dl_hash;
	dsp_dbg("key : %u, head : %u\n", key, dl_hash[key]);
}

void dsp_dl_hash_pop(char *k)
{
	unsigned int key = dsp_hash_get_key(k);
	unsigned int *node = dl_hash + key;
	struct dsp_dl_out *next;

	while (*node != DL_HASH_END) {
		next = (struct dsp_dl_out *)((unsigned long)dl_hash + *node);

		if (strcmp(next->data, k) == 0) {
			*node = next->hash_next;
			return;
		}

		node = &next->hash_next;
	}

	dsp_err("No hash node for %s\n", k);
}

#ifndef CONFIG_NPU_KUNIT_TEST
void dsp_dl_hash_print(void)
{
	int idx;
	unsigned int *node;
	struct dsp_dl_out *next;

	for (idx = 0; idx < DL_HASH_SIZE; idx++) {
		node = dl_hash + idx;

		while (*node != DL_HASH_END) {
			dsp_dump("\n");
			dsp_dump("Hash node: key(%u) offset(%u)\n", idx, *node);
			next = (struct dsp_dl_out *)(
					(unsigned long)dl_hash + *node);
			dsp_dump("Library name: %s\n", next->data);
			dsp_dump("Hash next offset: 0x%x\n", next->hash_next);
			node = &next->hash_next;
		}
	}
}
#endif

static unsigned int __dsp_dl_out_offset_align(unsigned int offset)
{
	return (offset + 0x3) & ~0x3;
}

int dsp_dl_out_create(struct dsp_lib *lib)
{
	int ret;
	unsigned int offset = 0;
	struct dsp_xml_lib *xml_lib;

	dsp_dbg("DL out create\n");
	lib->dl_out = (struct dsp_dl_out *)dsp_dl_malloc(
			sizeof(*lib->dl_out), "lib dl_out");

	lib->dl_out->hash_next = DL_HASH_END;

	offset += strlen(lib->name) + 1;
	offset = __dsp_dl_out_offset_align(offset);

	lib->dl_out->kernel_table.offset = offset;
	ret = dsp_hash_get(&xml_libs->lib_hash, lib->name,
			(void **)&xml_lib);
	if (ret == -1) {
		dsp_err("No global table for %s\n", lib->name);
		dsp_dl_free(lib->dl_out);
		lib->dl_out = NULL;
		return -1;
	}

	lib->dl_out->kernel_table.size = xml_lib->kernel_cnt *
		sizeof(struct dsp_dl_kernel_table);
	offset += lib->dl_out->kernel_table.size;
	offset = __dsp_dl_out_offset_align(offset);

	lib->dl_out->DM_sh.offset = offset;
	lib->dl_out->DM_sh.size = dsp_elf32_get_mem_size(&lib->elf->DMb,
			lib->elf);
	offset += lib->dl_out->DM_sh.size;
	offset = __dsp_dl_out_offset_align(offset);

	lib->dl_out->DM_local.offset = offset;
	lib->dl_out->DM_local.size = dsp_elf32_get_mem_size(
			&lib->elf->DMb_local, lib->elf);
	offset += lib->dl_out->DM_local.size;
	offset = __dsp_dl_out_offset_align(offset);

	lib->dl_out->TCM_sh.offset = offset;
	lib->dl_out->TCM_sh.size = dsp_elf32_get_mem_size(&lib->elf->TCMb,
			lib->elf);
	offset += lib->dl_out->TCM_sh.size;
	offset = __dsp_dl_out_offset_align(offset);

	lib->dl_out->TCM_local.offset = offset;
	lib->dl_out->TCM_local.size = dsp_elf32_get_mem_size(
			&lib->elf->TCMb_local, lib->elf);
	offset += lib->dl_out->TCM_local.size;
	offset = __dsp_dl_out_offset_align(offset);

	lib->dl_out->sh_mem.offset = offset;

	lib->dl_out->sh_mem.size = dsp_elf32_get_mem_size(&lib->elf->SFRw,
			lib->elf);
	return 0;
}

size_t dsp_dl_out_get_size(struct dsp_dl_out *dl_out)
{
	return sizeof(*dl_out) + dl_out->sh_mem.offset + dl_out->sh_mem.size;
}

static void __dsp_dl_out_cpy_metadata(struct dsp_dl_out *op1,
	struct dsp_dl_out *op2)
{
	memcpy(op1, op2, sizeof(*op1));
}

#ifndef CONFIG_NPU_KUNIT_TEST
static void __dsp_dl_out_print_sec_data(struct dsp_dl_out *dl_out,
	struct dsp_dl_out_section sec)
{
	unsigned int idx;
	unsigned int *data = (unsigned int *)(dl_out->data + sec.offset);
	unsigned int sec_end = sec.size / sizeof(unsigned int);

	for (idx = 0; idx < sec_end; idx++) {
		DL_BUF_STR("0x%08x ", data[idx]);

		if ((idx + 1) % 4 == 0 || idx == sec_end - 1) {
			DL_BUF_STR("\n");
			DL_PRINT_BUF(DEBUG);
		}
	}
}

static void __dsp_dl_out_print_kernel_table(struct dsp_dl_out *dl_out)
{
	struct dsp_dl_out_section sec = dl_out->kernel_table;
	unsigned int idx;
	unsigned int *data = (unsigned int *)(dl_out->data + sec.offset);
	unsigned int sec_end = sec.size / sizeof(unsigned int);

	for (idx = 0; idx < sec_end; idx++) {
		DL_BUF_STR("0x%08x ", data[idx]);

		if ((idx + 1) % 3 == 0 || idx == sec_end - 1) {
			DL_BUF_STR("\n");
			DL_PRINT_BUF(DEBUG);
		}
	}
}

static void __dsp_dl_out_print_data(struct dsp_dl_out *dl_out)
{
	dsp_dbg("Kernel address table\n");
	dsp_dbg("    Pre        Exe       Post\n");
	__dsp_dl_out_print_kernel_table(dl_out);

	dsp_dbg("DM shared data\n");
	__dsp_dl_out_print_sec_data(dl_out, dl_out->DM_sh);

	dsp_dbg("DM thread local data\n");
	__dsp_dl_out_print_sec_data(dl_out, dl_out->DM_local);

	dsp_dbg("TCM shared data\n");
	__dsp_dl_out_print_sec_data(dl_out, dl_out->TCM_sh);

	dsp_dbg("TCM thread local data\n");
	__dsp_dl_out_print_sec_data(dl_out, dl_out->TCM_local);

	dsp_dbg("Shared memory data\n");
	__dsp_dl_out_print_sec_data(dl_out, dl_out->sh_mem);
}

void dsp_dl_out_print(struct dsp_dl_out *dl_out)
{
	dsp_info("Library name: %s\n", dl_out->data);
	dsp_info("Hash next offset: 0x%x\n", dl_out->hash_next);
	dsp_info("Gpt address: 0x%x\n", dl_out->gpt_addr);
	dsp_info("\n");
	dsp_info("Data offsets\n");
	dsp_info("Kernel table: offset(%u) size(%u)\n",
		dl_out->kernel_table.offset, dl_out->kernel_table.size);
	dsp_info("DM shared data: offset(%u) size(%u\n",
		dl_out->DM_sh.offset, dl_out->DM_sh.size);
	dsp_info("DM thread local data: offset(%u) size(%u)\n",
		dl_out->DM_local.offset, dl_out->DM_local.size);
	dsp_info("TCM shared data: offset(%u) size(%u)\n",
		dl_out->TCM_sh.offset, dl_out->TCM_sh.size);
	dsp_info("TCM thread local data: offset(%u) size(%u)\n",
		dl_out->TCM_local.offset, dl_out->TCM_local.size);
	dsp_info("Shared memory data: offset(%u) size(%u)\n",
		dl_out->sh_mem.offset, dl_out->sh_mem.size);
	dsp_info("\n");
	dsp_dbg("Data loaded\n");
	__dsp_dl_out_print_data(dl_out);
}
#endif

int dsp_dl_out_manager_init(unsigned long start_addr, size_t size)
{
	unsigned long mem_str;
	size_t mem_size;

	dsp_dbg("DL out manager init\n");
	out_manager = (struct dsp_tlsf *)dsp_dl_malloc(
			sizeof(struct dsp_tlsf), "Out manager");

	if (size <= sizeof(unsigned int) * DL_HASH_SIZE) {
		dsp_err("Size(%zu) is not enough for DL_out\n", size);
		dsp_dl_free(out_manager);
		return -1;
	}

	dl_hash = (unsigned int *)start_addr;
	dsp_dbg("DL hash : 0x%p\n", dl_hash);

	dsp_dl_hash_init();

	mem_str = (unsigned long)((unsigned int *)start_addr + DL_HASH_SIZE);
	mem_size = size - sizeof(unsigned int) * DL_HASH_SIZE;
	dsp_tlsf_init(out_manager, mem_str, mem_size, DL_DL_OUT_ALIGN);
	return 0;
}

int dsp_dl_out_manager_free(void)
{
	dsp_tlsf_delete(out_manager);
	dsp_dl_free(out_manager);
	return 0;
}

#ifndef CONFIG_NPU_KUNIT_TEST
void dsp_dl_out_manager_print(void)
{
	dsp_dump(DL_BORDER);
	dsp_dump("Dynamic loader output manager\n");
	dsp_dump("\n");
	dsp_tlsf_print(out_manager);
	dsp_dump("\n");
	dsp_dump("Output hash table\n");
	dsp_dl_hash_print();
}
#endif

int dsp_dl_out_manager_alloc_libs(struct dsp_lib **libs, int libs_size,
	int *pm_inv)
{
	int ret, idx;

	dsp_dbg("Alloc DL out\n");

	for (idx = 0; idx < libs_size; idx++) {
		if (!libs[idx]->dl_out) {
			dsp_dbg("Alloc DL out for library %s\n",
				libs[idx]->name);
			ret = dsp_dl_out_alloc(libs[idx], pm_inv);

			if (ret == -1) {
				dsp_err("Alloc DL out failed\n");
				return -1;
			}
		}
	}

	return 0;
}

static int __dsp_dl_out_alloc_mem(size_t size, struct dsp_lib *lib,
	int *pm_inv)
{
	int ret;

	ret = dsp_tlsf_malloc(size, &lib->dl_out_mem,
			out_manager);
	if (ret == -1) {
		if (dsp_tlsf_can_be_loaded(out_manager, size)) {
			dsp_lib_manager_delete_no_ref();
			*pm_inv = 1;
			__dsp_dl_out_alloc_mem(size, lib, pm_inv);
		} else {
			dsp_err("Can not alloc DL out memory for library %s\n",
				lib->name);
			return -1;
		}
	}

	return 0;
}

int dsp_dl_out_alloc(struct dsp_lib *lib, int *pm_inv)
{
	int ret;
	size_t dl_out_size;
	int alloc_ret;
	unsigned long mem_addr;

	dsp_dbg("DL out alloc\n");
	ret = dsp_dl_out_create(lib);
	if (ret == -1) {
		dsp_err("CHK_ERR\n");
		return -1;
	}

	dl_out_size = dsp_dl_out_get_size(lib->dl_out);
	dsp_dbg("DL_out_size : %zu\n", dl_out_size);

	alloc_ret = __dsp_dl_out_alloc_mem(dl_out_size, lib,
			pm_inv);
	if (alloc_ret == -1) {
		dsp_dl_free(lib->dl_out);
		lib->dl_out = NULL;
		lib->dl_out_mem = NULL;
		return -1;
	}

	lib->dl_out_mem->lib = lib;

	mem_addr = lib->dl_out_mem->start_addr;
	__dsp_dl_out_cpy_metadata((struct dsp_dl_out *)mem_addr, lib->dl_out);
	dsp_dl_free(lib->dl_out);

	lib->dl_out = (struct dsp_dl_out *)mem_addr;

	strcpy(lib->dl_out->data, lib->name);
	dsp_dbg("lib name : %s\n", lib->dl_out->data);

	dsp_dl_hash_push(lib->dl_out);

	return 0;
}

void dsp_dl_out_free(struct dsp_lib *lib)
{
	if (lib->dl_out && lib->dl_out_mem) {
		dsp_dl_hash_pop(lib->dl_out->data);
		dsp_tlsf_free(lib->dl_out_mem, out_manager);
		lib->dl_out = NULL;
		lib->dl_out_mem = NULL;
	}
}
