// SPDX-License-Identifier: GPL-2.0
/*
 * Samsung Exynos SoC series dsp driver
 *
 * Copyright (c) 2019 Samsung Electronics Co., Ltd.
 *              http://www.samsung.com/
 */

#include "dl/dsp-pm-manager.h"
#include "dl/dsp-tlsf-allocator.h"
#include "dl/dsp-lib-manager.h"

#define DL_PM_ALIGN	(64)

static struct dsp_tlsf *pm_manager;
unsigned long dsp_pm_start_addr;

static struct dsp_lib *pm_init_lib;

int dsp_pm_manager_init(unsigned long start_addr, size_t size,
	unsigned int pm_offset)
{
	int ret;
	struct dsp_tlsf_mem *init_mem;

	dsp_dbg("PM manager init\n");

	pm_manager = (struct dsp_tlsf *)dsp_dl_malloc(
			sizeof(struct dsp_tlsf),
			"PM manager");
	pm_init_lib = (struct dsp_lib *)dsp_dl_malloc(
			sizeof(struct dsp_lib),
			"PM Boot lib");

	dsp_pm_start_addr = start_addr;
	ret = dsp_tlsf_init(pm_manager, start_addr, size, DL_PM_ALIGN);
	if (ret == -1) {
		dsp_err("TLSF init is failed\n");
		dsp_dl_free(pm_manager);
		dsp_dl_free(pm_init_lib);
		return -1;
	}

	dsp_dbg("PM init mem size : %u\n", pm_offset);
	ret = dsp_tlsf_malloc(pm_offset, &init_mem, pm_manager);
	if (ret == -1) {
		dsp_err("CHK_ERR\n");
		dsp_dl_free(pm_manager);
		dsp_dl_free(pm_init_lib);
		return -1;
	}

	pm_init_lib->name = (char *)dsp_dl_malloc(
			strlen("Boot/main pm") + 1,
			"Boot/main pm");
	strcpy(pm_init_lib->name, "Boot/main pm");

	pm_init_lib->ref_cnt = 1;
	pm_init_lib->loaded = 1;
	init_mem->lib = pm_init_lib;
	return 0;
}

void dsp_pm_manager_free(void)
{
	dsp_tlsf_delete(pm_manager);
	dsp_dl_free(pm_manager);
	dsp_dl_free(pm_init_lib->name);
	dsp_dl_free(pm_init_lib);
}

#ifndef CONFIG_NPU_KUNIT_TEST
void dsp_pm_manager_print(void)
{
	dsp_dump(DL_BORDER);
	dsp_dump("Program memory manager\n");
	dsp_dump("Start address: 0x%lx\n", dsp_pm_start_addr);
	dsp_dump("\n");
	dsp_tlsf_print(pm_manager);
}
#endif

int dsp_pm_manager_alloc_libs(struct dsp_lib **libs, int libs_size,
	int *pm_inv)
{
	int ret, idx;

	dsp_dbg("Alloc PM\n");

	*pm_inv = 0;
	for (idx = 0; idx < libs_size; idx++) {
		if (!libs[idx]->pm) {
			size_t text_size;

			dsp_dbg("Alloc PM for library %s\n",
				libs[idx]->name);
			text_size = dsp_elf32_get_text_size(libs[idx]->elf);
			dsp_dbg("PM alloc libs_size : %zu\n", text_size);

			ret = dsp_pm_alloc(text_size, libs[idx], pm_inv);

			if (ret == -1) {
				dsp_err("PM manager allocation failed\n");
				return -1;
			}
		}
	}

	return 0;
}

int dsp_pm_alloc(size_t size, struct dsp_lib *lib,
	int *pm_inv)
{
	int alloc_ret;

	if (!lib) {
		dsp_err("struct dsp_lib is null\n");
		return -1;
	}

	dsp_dbg("Allocate PM pm for lib %s(size : %zu)\n",
		lib->name, size);
	alloc_ret = dsp_tlsf_malloc(size, &lib->pm, pm_manager);

	if (alloc_ret == -1) {
		if (dsp_tlsf_can_be_loaded(pm_manager, size)) {
			dsp_info("PM I-cache invalidation set\n");
			dsp_lib_manager_delete_no_ref();
			*pm_inv = 1;
			return dsp_pm_alloc(size, lib, pm_inv);
		}

		dsp_err("Can not be loaded\n");
		return -1;
	}

	dsp_dbg("Lib(%s) allocation success\n", lib->name);
	lib->pm->lib = lib;
	return 0;
}

void dsp_pm_free(struct dsp_lib *lib)
{
	if (lib->pm) {
		dsp_tlsf_free(lib->pm, pm_manager);
		lib->pm = NULL;
	}
}

#ifndef CONFIG_NPU_KUNIT_TEST
void dsp_pm_print(struct dsp_lib *lib)
{
	unsigned int *text = (unsigned int *)lib->pm->start_addr;
	unsigned long offset = ((unsigned long)text - dsp_pm_start_addr) / 4;
	unsigned int idx;

	for (idx = 0; idx < lib->pm->size / sizeof(unsigned int);
		idx++, offset++) {
		if (idx % 4 == 0) {
			if (idx != 0) {
				DL_BUF_STR("\n");
				DL_PRINT_BUF(DEBUG);
			}
			DL_BUF_STR("0x%lx : ", offset);
		}

		DL_BUF_STR("0x");
		DL_BUF_STR("%08x ", *(unsigned int *)(text + idx));
	}

	DL_BUF_STR("\n");
	DL_PRINT_BUF(DEBUG);
}
#endif
