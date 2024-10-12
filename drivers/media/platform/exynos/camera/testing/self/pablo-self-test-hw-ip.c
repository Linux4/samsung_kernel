// SPDX-License-Identifier: GPL-2.0
/*
 * Samsung Exynos SoC series Pablo driver
 *
 * Copyright (c) 2022 Samsung Electronics Co., Ltd
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/slab.h>

#include "pablo-self-test-hw-ip.h"
#include "pablo-hw-chain-info.h"
#include "pablo-lib.h"
#include "is-core.h"
#include "is-hw-common-dma.h"

static struct is_region region;

static int pst_set_multi_target_irq(const char *val, const struct kernel_param *kp);
static int pst_get_multi_target_irq(char *buffer, const struct kernel_param *kp);
static const struct kernel_param_ops pablo_param_ops_multi_target_irq = {
	.set = pst_set_multi_target_irq,
	.get = pst_get_multi_target_irq,
};
module_param_cb(test_multi_target_irq, &pablo_param_ops_multi_target_irq, NULL, 0644);

static unsigned long result_multi_target_irq;

static int pst_set_multi_target_irq(const char *val, const struct kernel_param *kp)
{
	return 0;
}

static int pst_get_multi_target_irq(char *buffer, const struct kernel_param *kp)
{
	int ret;
	unsigned long result = 0;
	struct pablo_lib_platform_data *plpd = pablo_lib_get_platform_data();
	u32 start = plpd->mtarget_range[PABLO_MTARGET_RSTART];
	u32 end = plpd->mtarget_range[PABLO_MTARGET_REND];

	if (result_multi_target_irq > BIT(start) && result_multi_target_irq < BIT(end + 1))
		set_bit(0, &result);

	result_multi_target_irq = 0;

	pr_info("%s: result(0x%llx)\n", __func__, result);
	ret = sprintf(buffer, "1 ");
	ret += sprintf(buffer + ret, "%llx\n", result);

	return ret;
}

extern u32 is_hardware_dma_cfg(char *name, struct is_hw_ip *hw_ip,
			struct is_frame *frame, int cur_idx, u32 num_buffers,
			u32 *cmd, u32 plane,
			pdma_addr_t *dst_dva, dma_addr_t *src_dva);
extern void is_hardware_dma_cfg_kva64(char *name, struct is_hw_ip *hw_ip,
			struct is_frame *frame, int cur_idx,
			u32 *cmd, u32 plane,
			u64 *dst_kva, u64 *src_kva);

static void pst_is_hardware_frame_start(struct is_hw_ip *hw_ip, u32 instance)
{
	atomic_set(&hw_ip->status.Vvalid, V_VALID);

	if (pkv_in_hardirq())
		result_multi_target_irq |= BIT(raw_smp_processor_id());
}

static int pst_is_hardware_frame_done(struct is_hw_ip *hw_ip, struct is_frame *frame,
	int wq_id, u32 output_id, enum ShotErrorType done_type, bool get_meta)
{
	atomic_set(&hw_ip->status.Vvalid, V_BLANK);

	if (pkv_in_hardirq())
		result_multi_target_irq |= BIT(raw_smp_processor_id());

	return 0;
}

static const struct is_hardware_ops pst_is_hardware_ops = {
	.frame_start	= pst_is_hardware_frame_start,
	.frame_done	= pst_is_hardware_frame_done,
	.dma_cfg	= is_hardware_dma_cfg,
	.dma_cfg_kva64	= is_hardware_dma_cfg_kva64,
};

static void pst_update_param(u32 param[][PARAMETER_MAX_MEMBER], unsigned int param_index,
				unsigned int sub_index, u32 value)
{
	pr_info("param_index(%d), sub_index(%d), value(%d)\n",
		param_index, sub_index, value);

	param[param_index][sub_index] = value;
}

static void pst_update_vector(struct size_cr_set *cr_set, unsigned int index,
			u32 reg_addr, u32 reg_data)
{
	cr_set->size = index;
	cr_set->cr[index].reg_addr = reg_addr;
	cr_set->cr[index].reg_data = reg_data;
}

static void pst_start_hw_ip(struct is_hw_ip *hw_ip, struct is_frame *frame)
{
	u32 instance = 0;
	ulong hw_map = 0;

	set_bit(hw_ip->id, &hw_map);

	CALL_HWIP_OPS(hw_ip, open, instance);

	CALL_HWIP_OPS(hw_ip, init, instance, 0, 0);

	CALL_HWIP_OPS(hw_ip, set_param, &region, frame->pmap, instance, hw_map);

	CALL_HWIP_OPS(hw_ip, enable, instance, hw_map);

	CALL_HWIP_OPS(hw_ip, shot, frame, hw_map);

	fsleep(PST_WAIT_TIME_START_ISR);

	CALL_HWIP_OPS(hw_ip, get_meta, frame, hw_map);

	CALL_HWIP_OPS(hw_ip, disable, instance, hw_map);

	CALL_HWIP_OPS(hw_ip, deinit, instance);

	CALL_HWIP_OPS(hw_ip, close, instance);
}

static void pst_start_hw_group(struct is_frame *frame)
{
	int i;
	u32 instance = frame->instance;
	struct is_core *core = is_get_is_core();
	struct is_hardware *hw = &core->hardware;
	struct is_hw_ip *hw_ip = NULL;
	const struct is_hw_group_ops *hw_grp_ops = is_hw_get_m2m_group_ops();

	for (i = HW_SLOT_MAX - 1; i >= 0; i--) {
		if (frame->hw_slot_id[i] >= HW_SLOT_MAX)
			continue;

		hw_ip = &hw->hw_ip[frame->hw_slot_id[i]];
		set_bit(hw_ip->id, &hw->hw_map[instance]);
		hw_ip->group[instance] = NULL;
	}

	hw_grp_ops->open(hw, instance, frame->hw_slot_id);
	is_hardware_open(hw, instance, false);

	is_hardware_process_start(hw, instance);

	is_hardware_set_param(hw, instance, &region, frame->pmap, hw->hw_map[instance]);

	hw_grp_ops->shot(hw, instance, frame);

	fsleep(PST_WAIT_TIME_START_ISR);

	hw_grp_ops->stop(hw, instance, 0, frame->hw_slot_id);

	is_hardware_close(hw, instance);
	hw_grp_ops->close(hw, instance, frame->hw_slot_id);

	hw->hw_map[instance] = 0;
}

static void pst_deinit_frame(struct is_frame **frame)
{
	if (*frame) {
		vfree((*frame)->shot);
		(*frame)->shot = NULL;

		vfree((*frame)->parameter);
		(*frame)->parameter = NULL;
	}

	vfree(*frame);
	*frame = NULL;
}

static int pst_init_frame(struct is_frame **frame)
{
	int ret;

	*frame = vzalloc(sizeof(struct is_frame));
	if (!(*frame)) {
		pr_err("failed to allocate frame");
		return -ENOMEM;
	}

	(*frame)->parameter = vzalloc(sizeof(struct is_param_region));
	if (!(*frame)->parameter) {
		pr_err("failed to allocate parameter");
		ret = -ENOMEM;
		goto err_alloc_parameter;
	}

	(*frame)->shot = vzalloc(sizeof(struct camera2_shot));
	if (!(*frame)->shot) {
		pr_err("failed to allocate shot");
		ret = -ENOMEM;
		goto err_alloc_shot;
	}

	return 0;

err_alloc_shot:
	vfree((*frame)->parameter);

err_alloc_parameter:
	vfree(*frame);

	return ret;
}

static void pst_deinit_hw_ip(struct is_hw_ip **hw_ip)
{
	struct is_core *core = is_get_is_core();
	struct is_hardware *hardware = &core->hardware;

	if (*hw_ip) {
		vfree((*hw_ip)->framemgr);
		(*hw_ip)->framemgr = NULL;

		vfree((*hw_ip)->hardware);
		(*hw_ip)->hardware = hardware;

		*hw_ip = NULL;
	}
}

static int pst_init_hw_ip(struct is_hw_ip **hw_ip, enum is_hardware_id hw_id)
{
	int ret;
	struct is_core *core = is_get_is_core();
	struct is_hardware *hardware = &core->hardware;
	u32 hw_slot;

	hw_slot = CALL_HW_CHAIN_INFO_OPS(hardware, get_hw_slot_id, hw_id);
	if (hw_slot >= HW_SLOT_MAX) {
		pr_err("Invalid hw_id(%d)\n", hw_id);
		return -ENOMEM;
	}

	*hw_ip = &hardware->hw_ip[hw_slot];

	(*hw_ip)->hardware = vzalloc(sizeof(struct is_hardware));
	if (!(*hw_ip)->hardware) {
		pr_err("failed to allocate hardware");
		return -ENOMEM;
	}

	(*hw_ip)->hw_ops = &pst_is_hardware_ops;
	pablo_hw_chain_info_probe((*hw_ip)->hardware);

	(*hw_ip)->framemgr = vzalloc(sizeof(struct is_framemgr));
	if (!(*hw_ip)->framemgr) {
		pr_err("failed to allocate framemgr");
		ret = -ENOMEM;
		goto err_alloc_framemgr;
	}

	return 0;

err_alloc_framemgr:
	vfree((*hw_ip)->hardware);
	*hw_ip = NULL;

	return ret;
}

static void pst_result_hw_ip(struct is_hw_ip *hw_ip, unsigned long *result, u32 idx)
{
	int fs = atomic_read(&hw_ip->count.fs);
	int fe = atomic_read(&hw_ip->count.fe);

	bitmap_set(result, idx + 1, 1); /* This indicates number of total TC */

	if (fs && fs == fe)
		bitmap_set(result, idx, 1);
	else
		bitmap_clear(result, idx, 1);

	atomic_set(&hw_ip->count.fs, 0);
	atomic_set(&hw_ip->count.fe, 0);
}

static void pst_result_hw_group(struct is_frame *frame, unsigned long *result, u32 idx)
{
	int i;
	struct is_hw_ip *hw_ip;
	struct is_core *core = is_get_is_core();
	struct is_hardware *hw = &core->hardware;
	bool accum_result = true;

	for (i = HW_SLOT_MAX - 1; i >= 0; i--) {
		if (frame->hw_slot_id[i] >= HW_SLOT_MAX)
			continue;

		hw_ip = &hw->hw_ip[frame->hw_slot_id[i]];

		pst_result_hw_ip(hw_ip, result, idx);
		accum_result &= test_bit(idx, result);
	}

	if (!accum_result)
		bitmap_clear(result, idx, 1);
}

static int pst_start(enum is_hardware_id hw_id,
			struct is_frame *frame,
			u32 tc_idx,
			unsigned long *result,
			const struct pst_callback_ops *pst_cb)
{
	int ret;
	struct is_hw_ip *hw_ip = NULL;

	ret = pst_init_frame(&frame);
	if (ret) {
		pr_err("[HW_IP%d] failed to pst_init_frame(%d)\n", hw_id, ret);
		return ret;
	}

	CALL_PST_CB(pst_cb, set_param, frame);

	if (hw_id >= DEV_HW_END) {
		pst_start_hw_group(frame);
		pst_result_hw_group(frame, result, tc_idx);
	} else {
		ret = pst_init_hw_ip(&hw_ip, hw_id);
		if (ret) {
			pr_err("[HW_IP%d] failed to pst_init_hw_ip(%d)\n", hw_id, ret);
			goto err_pst_init_hw_ip;
		}

		pst_start_hw_ip(hw_ip, frame);
		pst_result_hw_ip(hw_ip, result, tc_idx);

		pst_deinit_hw_ip(&hw_ip);
	}

	CALL_PST_CB(pst_cb, clr_param, frame);

err_pst_init_hw_ip:
	pst_deinit_frame(&frame);

	return ret;
}

int pst_set_hw_ip(const char *val,
		enum is_hardware_id hw_id,
		struct is_frame *frame,
		u32 param[][PARAMETER_MAX_MEMBER],
		struct size_cr_set *cr_set,
		size_t preset_size,
		unsigned long *result,
		const struct pst_callback_ops *pst_cb)
{
	int ret, argc;
	char **argv;
	u32 act, tc_idx;
	u32 param_num, param_index, sub_index, value;
	u32 index, reg_addr, reg_data;
	unsigned int i;

	argv = argv_split(GFP_KERNEL, val, &argc);
	if (!argv) {
		pr_err("No argument!");
		return -EINVAL;
	}

	if (argc < 1) {
		pr_err("Not enough parameters. %d < 1", argc);
		ret = -EINVAL;
		goto func_exit;
	}

	ret = kstrtouint(argv[PST_SET_ACT], 0, &act);
	if (ret) {
		pr_err("Invalid act %d ret %d", act, ret);
		goto func_exit;
	}

	pr_info("[HW_IP%d] ACT %d\n", hw_id, act);

	switch (act) {
	case PST_HW_IP_STOP:
		bitmap_zero(result, preset_size);
		break;
	case PST_HW_IP_START:
		ret = kstrtouint(argv[PST_SET_TC_IDX], 0, &tc_idx);
		if (ret) {
			err("Invalid parameters(ret %d)", ret);
			goto func_exit;
		}

		pst_start(hw_id, frame, tc_idx, result, pst_cb);
		break;
	case PST_HW_IP_PRESET:
		for (tc_idx = 0; tc_idx < preset_size; tc_idx++) {
			CALL_PST_CB(pst_cb, init_param, tc_idx, PST_HW_IP_SINGLE);
			pst_start(hw_id, frame, tc_idx, result, pst_cb);
		}
		break;
	case PST_HW_IP_UPDATE_PARAM:
		if (argc < 4) {
			err("Not enough parameters. %d < 4", argc);
			err("ex) echo <3> <index> <index> <value>");
			ret = -EINVAL;
			goto func_exit;
		}

		ret = kstrtouint(argv[PST_UPDATE_PARAM_NUM], 0, &param_num);

		for (i = 0; i < param_num; i++) {
			ret |= kstrtouint(argv[i*3 + PST_UPDATE_PARAM_IDX], 0, &param_index);
			ret |= kstrtouint(argv[i*3 + PST_UPDATE_SUBPARAM_IDX], 0, &sub_index);
			ret |= kstrtouint(argv[i*3 + PST_UPDATE_VALUE], 0, &value);
			if (ret) {
				err("Invalid parameters(ret %d)", ret);
				goto func_exit;
			}
			pst_update_param(param, param_index, sub_index, value);
		}
		break;
	case PST_HW_IP_UPDATE_VECTOR:
		if (argc < 4) {
			err("Not enough parameters. %d < 4", argc);
			err("ex) echo <4> <index> <reg_addr> <reg_data>");
			ret = -EINVAL;
			goto func_exit;
		}

		ret = kstrtouint(argv[1], 0, &index);
		ret |= kstrtouint(argv[2], 0, &reg_addr);
		ret |= kstrtouint(argv[3], 0, &reg_data);
		if (ret) {
			err("Invalid parameters(ret %d)", ret);
			goto func_exit;
		}

		pst_update_vector(cr_set, index, reg_addr, reg_data);
		CALL_PST_CB(pst_cb, set_rta_info, frame, cr_set);
		break;
	default:
		pr_err("NOT supported act %u", act);
		goto func_exit;
	}

func_exit:
	argv_free(argv);
	return ret;
}

int pst_get_hw_ip(char *buffer,
		const char *name,
		const size_t preset_size,
		const unsigned long *result)
{
	int ret, i;
	size_t num_of_tc = find_last_bit(result, BITS_PER_TYPE(long));;

	pr_info("[%s] %s\n", name, __func__);
	ret = sprintf(buffer, "%zu ", num_of_tc);

	for (i = 0; i < BITS_TO_LONGS(preset_size); i++)
		ret += sprintf(buffer + ret, "%llx ", result[i]);

	return ret;
}

void *pst_get_param(struct is_frame *frame, u32 param_idx)
{
	ulong dst_base, dst_param;

	dst_base = (ulong)frame->parameter;
	dst_param = (dst_base + (param_idx * PARAMETER_MAX_SIZE));

	return (void *)dst_param;
}

void pst_set_param(struct is_frame *frame, u32 *src_params, u32 param_idx)
{
	u32 *dst_params = pst_get_param(frame, param_idx);

	memcpy(dst_params, src_params, PARAMETER_MAX_SIZE);
	set_bit(param_idx, frame->pmap);
}

int pst_cmp_param(struct is_frame *frame, void *param_dst, u32 param_idx)
{
	int i;
	void *param_src = pst_get_param(frame, param_idx);
	u32 *src = (u32 *)param_src;
	u32 *dst = (u32 *)param_dst;

	for (i = 0; i < PARAMETER_MAX_MEMBER; i++)
		pr_info("PARAM[%d]: [%d] src(%d) dst(%d)\n", param_idx, i, src[i], dst[i]);

	return memcmp(param_dst, param_src, sizeof(struct param_dma_input));
}

struct is_priv_buf *pst_set_dva(struct is_frame *frame, dma_addr_t *dva, size_t *size, u32 gid)
{
	int i;
	size_t size_sum = 0;
	struct is_priv_buf *pb;
	struct is_core *core = is_get_is_core();
	struct is_mem *mem = CALL_HW_CHAIN_INFO_OPS(&core->hardware, get_iommu_mem, gid);

	for (i = 0; size[i]; i++)
		size_sum += size[i];

	pr_info("%s: size(%zu)\n", __func__, size_sum);

	pb = CALL_PTR_MEMOP(mem, alloc, mem->priv, size_sum, NULL, 0);
	if (IS_ERR_OR_NULL(pb)) {
		pr_err("fail alloc");
		return NULL;
	}

	dva[0] = CALL_BUFOP(pb, dvaddr, pb);
	CALL_BUFOP(pb, kvaddr, pb);
	pr_info("%s: pb: %p, dva[0]: %pad\n", __func__, (void *)pb, &dva[0]);

	for (i = 1; size[i]; i++) {
		dva[i] = dva[i - 1] + size[i - 1];
		pr_info("dva[%d]: %pad\n", i, &dva[i]);
	}

	return pb;
}

void pst_clr_dva(struct is_priv_buf *pb)
{
	CALL_VOID_BUFOP(pb, free, pb);

	pr_info("%s: pb: %p\n", __func__, (void *)pb);
}

static void pst_get_size_by_format(enum dma_inout_format format, u32 plane,
				u32 stride, u32 height, size_t *size)
{
	switch (format) {
	case DMA_INOUT_FORMAT_BAYER:
	case DMA_INOUT_FORMAT_BAYER_PACKED:
	case DMA_INOUT_FORMAT_Y:
		switch (plane) {
		case 1:
			size[0] = stride * height;
			break;
		default:
			pr_err("Invalid plane BAYER(%d)\n", plane);
			break;
		}
		break;
	case DMA_INOUT_FORMAT_YUV422:
		switch (plane) {
		case 1:
			size[0] = stride * height * 2;
			break;
		case 2:
			size[0] = stride * height;
			size[1] = stride * height;
			break;
		default:
			pr_err("Invalid plane YUV422(%d)\n", plane);
			break;
		}
		break;
	case DMA_INOUT_FORMAT_YUV420:
		switch (plane) {
		case 2:
			size[0] = stride * height;
			size[1] = stride * height / 2;
			break;
		case 3:
			size[0] = stride * height;
			size[1] = stride * height / 4;
			size[2] = stride * height / 4;
			break;
		default:
			pr_err("Invalid plane YUV420(%d)\n", plane);
			break;
		}
		break;
	case DMA_INOUT_FORMAT_RGB:
		switch (plane) {
		case 1:
			size[0] = stride * height;
			break;
		default:
			pr_err("Invalid plane RGB(%d)\n", plane);
			break;
		}
		break;
	default:
		pr_err("Invalid format YUV(%d)\n", format);
		break;
	}

	pr_info("%s: fmt(%d), plane(%d), size(%zu, %zu, %zu)\n",
		__func__, format, plane, size[0], size[1], size[2]);
}


static void pst_get_size_by_sbwc(enum dma_input_sbwc_type sbwc_type,
				u32 bpp, u32 w, u32 h, u32 align,
				u32 block_w, u32 block_h, size_t *size)
{
	u32 sbwc_en, comp_64b_align, lossy_byte32num = 0;
	u32 payload_size, header_size;

	sbwc_en = is_hw_dma_get_comp_sbwc_en(sbwc_type, &comp_64b_align);

	payload_size = is_hw_dma_get_payload_stride(sbwc_en, bpp, w, comp_64b_align,
						lossy_byte32num, block_w, block_h)
			* DIV_ROUND_UP(h, block_h);

	header_size = is_hw_dma_get_header_stride(w, block_w, align)
			* DIV_ROUND_UP(h, block_h);

	size[0] = payload_size + header_size;

	pr_info("%s: payload(%u), header(%u), total(%zu)\n",
		__func__, payload_size, header_size, size[0]);
}

void pst_get_size_of_dma_input(void *param, u32 align, u32 block_w, u32 block_h, size_t *size)
{
	struct param_dma_input *dma_in = param;
	u32 stride = ALIGN(dma_in->width * dma_in->bitwidth / 8, align);
	u32 sbwc_type = dma_in->sbwc_type;

	if (dma_in->cmd == DMA_INPUT_COMMAND_DISABLE)
		return;

	if (sbwc_type == NONE)
		pst_get_size_by_format(dma_in->format, dma_in->plane,
					stride, dma_in->height, size);
	else
		pst_get_size_by_sbwc(sbwc_type, dma_in->bitwidth,
				dma_in->width, dma_in->height, align,
				block_w, block_h, size);
}

void pst_get_size_of_dma_output(void *param, u32 align, size_t *size)
{
	struct param_dma_output *dma_out = param;
	u32 width = max(dma_out->width, dma_out->stride_plane0);
	u32 stride = ALIGN(width * dma_out->bitwidth / 8, align);

	if (dma_out->cmd == DMA_OUTPUT_COMMAND_DISABLE)
		return;

	pst_get_size_by_format(dma_out->format, dma_out->plane,
				stride, dma_out->height, size);
}

void pst_get_size_of_mcs_output(void *param, u32 align, size_t *size)
{
	struct param_mcs_output *mcs_out = param;
	u32 stride = ALIGN(mcs_out->width * mcs_out->dma_bitwidth / 8, align);

	if (mcs_out->dma_cmd == DMA_OUTPUT_COMMAND_DISABLE)
		return;

	pst_get_size_by_format(mcs_out->dma_format, mcs_out->plane,
				stride, mcs_out->height, size);
}

void pst_get_size_of_lme_input(void *param, u32 align, size_t *size)
{
	struct param_lme_dma *lme_in = param;
	u32 stride = ALIGN(lme_in->cur_input_width * lme_in->input_bitwidth / 8, align);

	if (lme_in->cur_input_cmd == DMA_INPUT_COMMAND_DISABLE)
		return;

	pst_get_size_by_format(lme_in->input_format, lme_in->input_plane,
				stride, lme_in->cur_input_height, size);
}
