// SPDX-License-Identifier: GPL-2.0
/*
 * Samsung Exynos SoC series dsp driver
 *
 * Copyright (c) 2019 Samsung Electronics Co., Ltd.
 *              http://www.samsung.com/
 */

#include "dsp-binary.h"
#include "dl/dsp-dl-engine.h"
#include "dl/dsp-common.h"
#include "dl/dsp-lib-manager.h"
#include "dl/dsp-pm-manager.h"
#include "dl/dsp-gpt-manager.h"
#include "dl/dsp-dl-out-manager.h"
#include "dl/dsp-xml-parser.h"

#define DSP_DL_GKT_NAME			"dsp_gkt.xml"
#define DSP_DL_RULE_NAME		"dsp_reloc_rules.bin"
#define DSP_DL_LIB_LOG_NAME		"liblog.elf"
#define DSP_DL_LIB_IVP_NAME		"libivp.elf"
#define DSP_DL_COMMON_SIZE		(2)
#define DSP_DL_GPT_OFFSET		(SZ_1K * 30)
#define DSP_DL_GPT_SIZE			(SZ_1K * 1)

static struct dsp_dl_param *dl_param;

static enum dsp_dl_status __dsp_dl_init_common_lib(void)
{
	int ret;
	int pm_inv;
	struct dsp_lib **libs;
	struct dsp_dl_lib_info *common_libs = dl_param->common_libs;
	int common_size = dl_param->common_size;

	dsp_dbg(DL_BORDER);
	dsp_dbg("Init common lib\n");

	dsp_dbg("Get common libs\n");
	libs = dsp_lib_manager_get_libs(common_libs, common_size);
	if (libs == NULL) {
		dsp_err("Getting library is failed\n");
		return DSP_DL_FAIL;
	}

	dsp_lib_manager_inc_ref_cnt(libs, common_size);

	dsp_dbg("Get common lib elf\n");
	ret = dsp_elf32_load_libs(common_libs, libs, common_size);
	if (ret == -1) {
		dsp_lib_manager_dec_ref_cnt(libs, common_size);
		dsp_lib_manager_delete_unloaded_libs(libs, common_size);
		dsp_dl_free(libs);
		return DSP_DL_FAIL;
	}

	dsp_dbg("Allocate common_lib pm\n");
	ret = dsp_pm_manager_alloc_libs(libs, common_size,
			&pm_inv);
	if (ret == -1) {
		dsp_lib_manager_dec_ref_cnt(libs, common_size);
		dsp_lib_manager_delete_unloaded_libs(libs, common_size);
		dsp_dl_free(libs);
		return DSP_DL_FAIL;
	}

	dsp_dbg("Link common_lib\n");
	ret = dsp_linker_link_libs(libs, common_size, NULL, 0);
	if (ret == -1) {
		dsp_lib_manager_dec_ref_cnt(libs, common_size);
		dsp_lib_manager_delete_unloaded_libs(libs, common_size);
		dsp_dl_free(libs);
		return DSP_DL_FAIL;
	}

	dsp_dbg("Load common_lib\n");
	ret = dsp_lib_manager_load_libs(libs, common_size);
	if (ret == -1) {
		dsp_lib_manager_dec_ref_cnt(libs, common_size);
		dsp_lib_manager_delete_unloaded_libs(libs, common_size);
		dsp_dl_free(libs);
		return DSP_DL_FAIL;
	}

	dsp_dl_free(libs);

	return DSP_DL_SUCCESS;
}

enum dsp_dl_status __dsp_dl_close_common_lib(void)
{
	int idx;
	struct dsp_lib **libs;
	struct dsp_dl_lib_info *common_libs = dl_param->common_libs;
	int common_size = dl_param->common_size;

	dsp_dbg(DL_BORDER);
	dsp_dbg("Close common lib\n");

	libs = dsp_lib_manager_get_libs(common_libs, common_size);
	if (libs == NULL) {
		dsp_dbg("Getting library is failed\n");
		return DSP_DL_FAIL;
	}

	for (idx = 0; idx < common_size; idx++) {
		if (!libs[idx]->loaded || libs[idx]->ref_cnt < 1) {
			dsp_err("libraries already unloaded or ended\n");
			dsp_lib_manager_delete_unloaded_libs(libs, common_size);
			dsp_dl_free(libs);
			return DSP_DL_FAIL;
		}
	}

	dsp_lib_manager_unload_libs(libs, common_size);
	dsp_dl_free(libs);

	return DSP_DL_SUCCESS;
}

enum dsp_dl_status dsp_dl_init(struct dsp_dl_param *param)
{
	int ret;

	dsp_info("Init dynamic loader\n");

	ret = dsp_common_init();
	if (ret < 0) {
		dsp_err("CHK_ERR\n");
		return DSP_DL_FAIL;
	}

	dl_param = (struct dsp_dl_param *)dsp_dl_malloc(
			sizeof(*dl_param), "DL param");
	memcpy(dl_param, param, sizeof(*dl_param));

	dsp_dbg(DL_BORDER);
	dsp_dbg("Global Kernel table init(addr:%p, size:%zu\n",
		dl_param->gkt.mem, dl_param->gkt.size);
	dsp_xml_parser_init();
	ret = dsp_dl_add_gkt(&dl_param->gkt, NULL, 0);
	if (ret == -1) {
		dsp_err("CHK_ERR\n");
		return DSP_DL_FAIL;
	}

	dsp_dbg("LIB manager init\n");
	ret = dsp_lib_manager_init(dl_param->lib_path);
	if (ret == -1) {
		dsp_err("CHK_ERR\n");
		return DSP_DL_FAIL;
	}

	dsp_dbg("Linker init(addr:%p, size:%zu\n",
		dl_param->rule.mem, dl_param->rule.size);
	ret = dsp_linker_init(&dl_param->rule);
	if (ret == -1) {
		dsp_err("CHK_ERR\n");
		return DSP_DL_FAIL;
	}

	dsp_info("PM init(size:%zu, ivp main:%u)\n",
		dl_param->pm.size, dl_param->pm_offset);
	dsp_dbg("PM init(addr:0x%lx, size:%zu, ivp main:%u)\n",
		dl_param->pm.addr, dl_param->pm.size, dl_param->pm_offset);
	ret = dsp_pm_manager_init(dl_param->pm.addr, dl_param->pm.size,
			dl_param->pm_offset);
	if (ret == -1) {
		dsp_err("PM initialize is failed\n");
		return DSP_DL_FAIL;
	}

	dsp_info("GPT init(addr:0x%lx, size:%zu)\n",
		dl_param->gpt.addr, dl_param->gpt.size);
	ret = dsp_gpt_manager_init(dl_param->gpt.addr, dl_param->gpt.size);
	if (ret == -1) {
		dsp_err("GPT initialize is failed\n");
		return DSP_DL_FAIL;
	}

	dsp_info("DL out init(size:%zu)\n", dl_param->dl_out.size);
	dsp_dbg("DL out init(addr:0x%lx, size:%zu)\n",
		dl_param->dl_out.addr, dl_param->dl_out.size);
	ret = dsp_dl_out_manager_init(dl_param->dl_out.addr,
			dl_param->dl_out.size);
	if (ret == -1) {
		dsp_err("DL out initialize is failed\n");
		return DSP_DL_FAIL;
	}

	ret = __dsp_dl_init_common_lib();
	if (ret == -1) {
		dsp_err("Common Lib initialize is failed\n");
		return DSP_DL_FAIL;
	}

	return DSP_DL_SUCCESS;
}

enum dsp_dl_status dsp_dl_add_gkt(struct dsp_dl_lib_file *gkt,
		struct string_manager *str_manager, u32 unique_id)
{
	int ret = 0;
	dsp_dbg("dsp_dl_add_gkt start unique_id[%u]\n", unique_id);

	ret = dsp_xml_parser_parse(gkt, str_manager, unique_id);
	if (ret == -1) {
		dsp_err("CHK_ERR\n");
		return DSP_DL_FAIL;
	}

	return DSP_DL_SUCCESS;
}

enum dsp_dl_status dsp_dl_close(void)
{
	dsp_info("Close dynamic loader\n");

	__dsp_dl_close_common_lib();
	dsp_dl_free(dl_param);

	dsp_dbg("XML parser free\n");
	dsp_xml_parser_free();

	dsp_dbg("LIB manager free\n");
	dsp_lib_manager_free();

	dsp_dbg("Linker free\n");
	dsp_linker_free();

	dsp_dbg("PM manager free\n");
	dsp_pm_manager_free();

	dsp_dbg("GPT manager free\n");
	dsp_gpt_manager_free();

	dsp_dbg("DL out manager free\n");
	dsp_dl_out_manager_free();

	dsp_common_free();
	return DSP_DL_SUCCESS;
}


struct dsp_dl_load_status dsp_dl_load_libraries(
	struct dsp_dl_lib_info *infos, int size)
{
	int ret;
	int idx;
	struct dsp_dl_load_status load_status;
	struct dsp_lib **libs;
	struct dsp_lib **commons = NULL;
	struct dsp_dl_lib_info *common_libs = dl_param->common_libs;
	int common_size = dl_param->common_size;

	load_status.status = DSP_DL_FAIL;
	load_status.pm_inv = 0;

	if (size <= 0) {
		dsp_err("No libraries to load\n");
		return load_status;
	}

	dsp_dbg(DL_BORDER);

	dsp_dbg("Load libraries(%d)\n", size);
	for (idx = 0; idx < size; idx++) {
		dsp_dbg("Load : %s\n", infos[idx].name);
	}

	dsp_dbg("Get common lib\n");
	commons = dsp_lib_manager_get_libs(common_libs, common_size);
	if (commons == NULL) {
		dsp_err("Getting common library is failed\n");
		return load_status;
	}

	dsp_dbg("Get libs from lib manager\n");
	libs = dsp_lib_manager_get_libs(infos, size);
	if (libs == NULL) {
		dsp_err("Getting library is failed\n");
		return load_status;
	}

	dsp_lib_manager_inc_ref_cnt(libs, size);

	dsp_dbg("Load ELFs\n");
	ret = dsp_elf32_load_libs(infos, libs, size);
	if (ret == -1) {
		dsp_lib_manager_dec_ref_cnt(libs, size);
		dsp_lib_manager_delete_unloaded_libs(libs, size);
		dsp_dl_free(libs);
		dsp_dl_free(commons);
		return load_status;
	}

	dsp_dbg("Load PM\n");
	ret = dsp_pm_manager_alloc_libs(libs, size,
			&load_status.pm_inv);
	if (ret == -1) {
		dsp_lib_manager_dec_ref_cnt(libs, size);
		dsp_lib_manager_delete_unloaded_libs(libs, size);
		dsp_dl_free(libs);
		dsp_dl_free(commons);
		return load_status;
	}

	dsp_dbg("Load GPT\n");
	ret = dsp_gpt_manager_alloc_libs(libs, size,
			&load_status.pm_inv);
	if (ret == -1) {
		dsp_lib_manager_dec_ref_cnt(libs, size);
		dsp_lib_manager_delete_unloaded_libs(libs, size);
		dsp_dl_free(libs);
		dsp_dl_free(commons);
		return load_status;
	}

	dsp_dbg("Load DL out\n");
	ret = dsp_dl_out_manager_alloc_libs(libs, size,
			&load_status.pm_inv);
	if (ret == -1) {
		dsp_lib_manager_dec_ref_cnt(libs, size);
		dsp_lib_manager_delete_unloaded_libs(libs, size);
		dsp_dl_free(libs);
		dsp_dl_free(commons);
		return load_status;
	}

	dsp_dbg("Link libs\n");
	ret = dsp_linker_link_libs(libs, size, commons, common_size);
	if (ret == -1) {
		dsp_lib_manager_dec_ref_cnt(libs, size);
		dsp_lib_manager_delete_unloaded_libs(libs, size);
		dsp_dl_free(libs);
		dsp_dl_free(commons);
		return load_status;
	}

	dsp_dbg("Load libs\n");
	ret = dsp_lib_manager_load_libs(libs, size);
	if (ret == -1) {
		dsp_lib_manager_dec_ref_cnt(libs, size);
		dsp_lib_manager_delete_unloaded_libs(libs, size);
		dsp_dl_free(libs);
		dsp_dl_free(commons);
		return load_status;
	}

	dsp_dl_free(libs);
	dsp_dl_free(commons);

	load_status.status = DSP_DL_SUCCESS;
	return load_status;
}

enum dsp_dl_status dsp_dl_unload_libraries(
	struct dsp_dl_lib_info *infos, int size, bool is_user_kernel)
{
	struct dsp_lib **libs;
	int idx;

	if (size <= 0) {
		dsp_err("No libraries to unload\n");
		return DSP_DL_SUCCESS;
	}

	dsp_dbg(DL_BORDER);

	dsp_info("Unload libraries(%d)\n", size);
	for (idx = 0; idx < size; idx++)
		dsp_dbg("Unload : %s\n", infos[idx].name);

	libs = dsp_lib_manager_get_libs(infos, size);
	if (libs == NULL) {
		dsp_dbg("Getting library is failed\n");
		return DSP_DL_FAIL;
	}

	for (idx = 0; idx < size; idx++) {
		if (!libs[idx]->loaded || libs[idx]->ref_cnt < 1) {
			dsp_dbg("libraries already unloaded or ended\n");
			dsp_lib_manager_delete_unloaded_libs(libs, size);
			dsp_dl_free(libs);
			return DSP_DL_FAIL;
		}
		if (is_user_kernel)
			kfree(infos[idx].name);
	}

	//Logically, each session has an independent self object,
	//so it must be deleted at the end time.
	dsp_lib_manager_unload_libs(libs, size);
	for (idx = 0; idx < size; idx++) {
		if (libs[idx]->ref_cnt == 0) {
			dsp_lib_manager_delete_lib(libs[idx]);
		}
	}

	dsp_dl_free(libs);
	return DSP_DL_SUCCESS;
}


void dsp_dl_print_status(void)
{
	dsp_pm_manager_print();
	dsp_gpt_manager_print();
	dsp_dl_out_manager_print();
	//dsp_lib_manager_print();
}

int dsp_kernel_manager_dl_init(struct device *dev, struct dsp_dl_param *dl_param,
						struct npu_memory_buffer *ivp_mem, struct npu_memory_buffer *dl_out_mem)
{
	int ret;
	void *load;
	size_t loaded_size;

	dsp_enter();
	dl_param->common_libs = kmalloc(sizeof(struct dsp_dl_lib_info) *
			DSP_DL_COMMON_SIZE, GFP_KERNEL);

	if (!dl_param->common_libs) {
		ret = -ENOMEM;
		dsp_err("Failed to allocate dl common libs\n");
		goto p_err;
	}

	ret = dsp_binary_alloc_load(dev, DSP_DL_GKT_NAME, NULL, NULL, &load,
			&loaded_size);
	if (ret)
		goto p_err_gkt;

	dl_param->gkt.size = loaded_size;
	dl_param->gkt.mem = load;

	ret = dsp_binary_alloc_load(dev, DSP_DL_RULE_NAME, NULL, NULL,
			&load, &loaded_size);
	if (ret)
		goto p_err_rule;

	dl_param->rule.size = loaded_size;
	dl_param->rule.mem = load;

	ret = dsp_binary_alloc_load(dev, DSP_DL_LIB_LOG_NAME, NULL, NULL,
			&load, &loaded_size);
	if (ret)
		goto p_err_log;

	dl_param->common_libs[0].file.size = loaded_size;
	dl_param->common_libs[0].name = DSP_DL_LIB_LOG_NAME;
	dl_param->common_libs[0].file.mem = load;

	ret = dsp_binary_alloc_load(dev, DSP_DL_LIB_IVP_NAME, NULL, NULL,
			&load, &loaded_size);
	if (ret)
		goto p_err_ivp;

	dl_param->common_libs[1].file.size = loaded_size;
	dl_param->common_libs[1].name = DSP_DL_LIB_IVP_NAME;
	dl_param->common_libs[1].file.mem = load;

	dl_param->common_size = DSP_DL_COMMON_SIZE;

	dl_param->pm.addr = (unsigned long)ivp_mem->vaddr;
	dl_param->pm.size = ivp_mem->size;
	// TODO temp patch, need to change binary load location
	if (!ivp_mem->used_size) {
		dsp_warn("IVP PM size is unstable\n");
		ivp_mem->used_size =  SZ_1M;
	}
	dl_param->pm_offset = ivp_mem->used_size;
	dl_param->gpt.addr = DSP_DL_GPT_OFFSET;
	dl_param->gpt.size = DSP_DL_GPT_SIZE;

	dl_param->dl_out.addr = (unsigned long)dl_out_mem->vaddr;
	dl_param->dl_out.size = dl_out_mem->size;

	ret = dsp_dl_init(dl_param);
	if (ret) {
		dsp_err("Failed to init DL(%d)\n", ret);
		goto p_err_init;
	}

	dsp_info("DL is initilized.\n");
	dsp_leave();
	return 0;
p_err_init:
	vfree(dl_param->common_libs[1].file.mem);
p_err_ivp:
	vfree(dl_param->common_libs[0].file.mem);
p_err_log:
	vfree(dl_param->rule.mem);
p_err_rule:
	vfree(dl_param->gkt.mem);
p_err_gkt:
	kfree(dl_param->common_libs);
p_err:
	return ret;
}

void dsp_kernel_manager_dl_deinit(struct dsp_dl_param *dl_param)
{
	int ret;

	dsp_enter();
	ret = dsp_dl_close();
	if (ret)
		dsp_err("Failed to close DL(%d)\n", ret);

	vfree(dl_param->common_libs[1].file.mem);
	vfree(dl_param->common_libs[0].file.mem);
	vfree(dl_param->rule.mem);
	vfree(dl_param->gkt.mem);
	kfree(dl_param->common_libs);
	dsp_leave();
}
