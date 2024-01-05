/*
 * Copyright (C) 2012 Spreadtrum Communications Inc.
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */
#include <linux/kernel.h>
#include <linux/slab.h>
#include <linux/interrupt.h>
#include <linux/bitops.h>
#include <linux/semaphore.h>
#include <linux/delay.h>
#include <asm/io.h>
#include <mach/hardware.h>
#include "scale_drv.h"
#include "gen_scale_coef.h"
#include "dcam_drv.h"

/*#define SCALE_DRV_DEBUG*/
#define SCALE_LOWEST_ADDR 0x800
#define SCALE_ADDR_INVALIDE(addr) ((addr) < SCALE_LOWEST_ADDR)
#define SCALE_YUV_ADDR_INVALIDE(y,u,v) \
	(SCALE_ADDR_INVALIDE(y) && \
	SCALE_ADDR_INVALIDE(u) && \
	SCALE_ADDR_INVALIDE(v))

#define SC_COEFF_H_TAB_OFFSET 0x1400
#define SC_COEFF_V_TAB_OFFSET 0x14F0
#define SC_COEFF_V_CHROMA_TAB_OFFSET 0x18F0
#define SC_COEFF_BUF_SIZE (24 << 10)
#define SC_COEFF_COEF_SIZE (1 << 10)
#define SC_COEFF_TMP_SIZE (21 << 10)
#define SC_H_COEF_SIZE (0xC0)
#define SC_V_COEF_SIZE (0x210)
#define SC_V_CHROM_COEF_SIZE (0x210)
#define SC_COEFF_H_NUM (SC_H_COEF_SIZE / 4)
#define SC_COEFF_V_NUM (SC_V_COEF_SIZE / 4)
#define SC_COEFF_V_CHROMA_NUM (SC_V_CHROM_COEF_SIZE / 4)
#define SCALE_PIXEL_ALIGNED 4
#define SCALE_SLICE_HEIGHT_ALIGNED 4
#define IO_PTR volatile void __iomem *
#define REG_RD(a) __raw_readl((IO_PTR)(a))
#define REG_WR(a,v) __raw_writel((v),(IO_PTR)(a))
#define REG_AWR(a,v) __raw_writel((__raw_readl((IO_PTR)(a)) & (v)), ((IO_PTR)(a)))
#define REG_OWR(a,v) __raw_writel((__raw_readl((IO_PTR)(a)) | (v)), ((IO_PTR)(a)))
#define REG_XWR(a,v) __raw_writel((__raw_readl((IO_PTR)(a)) ^ (v)), ((IO_PTR)(a)))
#define REG_MWR(a,m,v) \
	do { \
		uint32_t _tmp = __raw_readl((IO_PTR)(a)); \
		_tmp &= ~(m); \
		__raw_writel((_tmp | ((m) & (v))), ((IO_PTR)(a))); \
	}while(0)

#define SCALE_CHECK_PARAM_ZERO_POINTER(n) \
	do { \
		if (0 == (int)(n)) \
			return -SCALE_RTN_PARA_ERR; \
	} while(0)

#define SCALE_RTN_IF_ERR if(rtn) return rtn

typedef void (*scale_isr)(void);

struct scale_desc {
	struct scale_size input_size;
	struct scale_rect input_rect;
	struct scale_size sc_input_size;
	struct scale_addr input_addr;
	uint32_t input_format;
	struct scale_size output_size;
	struct scale_addr output_addr;
	uint32_t output_format;
	uint32_t scale_mode;
	uint32_t slice_height;
	uint32_t slice_out_height;
	uint32_t slice_in_height;
	uint32_t is_last_slice;
	scale_isr_func user_func;
	void *user_data;
	atomic_t start_flag;
	uint32_t sc_deci_val;
};

static struct scale_desc scale_path;
static struct scale_desc *g_path = &scale_path;
static uint32_t s_wait_flag = 0;
static struct semaphore scale_done_sema = __SEMAPHORE_INITIALIZER(scale_done_sema, 0);
static uint32_t *s_scaler_scaling_coeff_addr = NULL;

static DEFINE_SPINLOCK(scale_lock);
static int32_t _scale_cfg_scaler(void);
static int32_t _scale_calc_sc_size(void);
static int32_t _scale_set_sc_coeff(void);
static void _scale_reg_trace(void);
static int _scale_isr_root(struct dcam_frame* dcam_frm, void* u_data);

int32_t scale_module_en(void)
{
	int ret = 0;

	ret = dcam_get_resizer(0);
	if (ret) {
		printk("scale_module_en, failed to get review path %d \n", ret);
		goto fail_get_resizer;
	}

	ret = dcam_module_en();
	if (ret) {
		printk("scale_module_en, failed to enable scale module %d \n", ret);
		goto fail_dcam_eb;
	}

	memset(g_path, 0, sizeof(scale_path));
	return ret;

fail_dcam_eb:
	dcam_rel_resizer();
fail_get_resizer:
	return ret;
}

int32_t scale_module_dis(void)
{
	int ret = 0;

	ret = dcam_module_dis();
	if (ret) {
		printk("scale_module_dis, failed to disable scale module %d \n", ret);
	}

	ret = dcam_rel_resizer();
	if (ret) {
		printk("scale_module_dis, failed to free review path %d \n", ret);
	}

	return ret;
}

static int32_t scale_check_deci_slice_mode(uint32_t deci_val, uint32_t slice_h)
{
	enum scale_drv_rtn rtn = SCALE_RTN_SUCCESS;

	if (deci_val > 0) {
		if ((slice_h >= deci_val) && (0 == (slice_h % deci_val))) {
			rtn = SCALE_RTN_SUCCESS;
		} else {
			rtn = SCALE_RTN_SC_ERR;
		}
	}
	return rtn;
}

int  scale_coeff_alloc(void)
{
	int ret = 0;

	if (NULL == s_scaler_scaling_coeff_addr) {
		s_scaler_scaling_coeff_addr = (uint32_t *)kmalloc(SC_COEFF_BUF_SIZE, GFP_KERNEL);
		if (NULL == s_scaler_scaling_coeff_addr) {
			printk("SCALE DRV: scale_coeff_alloc fail.\n");
			ret = -1;
		}
	}
	return ret;
}

void  scale_coeff_free(void)
{
	if (s_scaler_scaling_coeff_addr) {
		kfree(s_scaler_scaling_coeff_addr);
		s_scaler_scaling_coeff_addr = NULL;
	}
}

static uint32_t *get_scale_coeff_addr(void)
{
	return s_scaler_scaling_coeff_addr;
}

int32_t scale_start(void)
{
	enum scale_drv_rtn rtn = SCALE_RTN_SUCCESS;

	SCALE_TRACE("SCALE DRV: scale_start: %d \n", g_path->scale_mode);

	if (SCALE_MODE_NORMAL == g_path->scale_mode) {
		if (g_path->output_size.w > SCALE_FRAME_WIDTH_MAX) {
			rtn = SCALE_RTN_SC_ERR;
			goto exit;
		}
	} else {
		if (g_path->output_size.w > SCALE_LINE_BUF_LENGTH) {
			rtn = SCALE_RTN_SC_ERR;
			goto exit;
		}
	}

	g_path->slice_in_height = 0;
	g_path->slice_out_height = 0;
	g_path->is_last_slice = 0;
	g_path->sc_deci_val = 0;
	REG_MWR(SCALE_CFG, (SCALE_DEC_X_EB_BIT|SCALE_DEC_Y_EB_BIT), 0);

	rtn = _scale_cfg_scaler();
	if (rtn) goto exit;

	if (SCALE_MODE_NORMAL != g_path->scale_mode) {
		g_path->slice_in_height += g_path->slice_height;
		rtn = scale_check_deci_slice_mode(g_path->sc_deci_val, g_path->slice_height);
		if (rtn) goto exit;
	}
	dcam_glb_reg_mwr(SCALE_BASE, SCALE_PATH_MASK, SCALE_PATH_SELECT, DCAM_CFG_REG);
	dcam_glb_reg_owr(SCALE_BASE, SCALE_PATH_EB_BIT, DCAM_CFG_REG);
	_scale_reg_trace();

	dcam_glb_reg_owr(SCALE_CTRL, (SCALE_FRC_COPY_BIT|SCALE_COEFF_FRC_COPY_BIT), DCAM_CONTROL_REG);
	atomic_inc(&g_path->start_flag);
	dcam_glb_reg_owr(SCALE_CTRL, SCALE_START_BIT, DCAM_CONTROL_REG);
	return SCALE_RTN_SUCCESS;

exit:
	printk("SCALE DRV: ret %d \n", rtn);
	return rtn;
}

static int32_t scale_continue(void)
{
	enum scale_drv_rtn rtn = SCALE_RTN_SUCCESS;
	uint32_t slice_h = g_path->slice_height;

	SCALE_TRACE("SCALE DRV: continue %d, %d, %d \n",
		g_path->slice_height, g_path->slice_in_height, g_path->scale_mode);

	if (SCALE_MODE_NORMAL != g_path->scale_mode) {
		if (g_path->slice_in_height + g_path->slice_height >= g_path->input_rect.h) {
			slice_h = g_path->input_rect.h - g_path->slice_in_height;
			if (scale_check_deci_slice_mode(g_path->sc_deci_val, slice_h))
				return SCALE_RTN_SC_ERR;
			g_path->is_last_slice = 1;
			REG_MWR(SCALE_REV_SLICE_CFG, SCALE_INPUT_SLICE_HEIGHT_MASK, slice_h);
			REG_OWR(SCALE_REV_SLICE_CFG, SCALE_IS_LAST_SLICE_BIT);
			SCALE_TRACE("SCALE DRV: continue, last slice, 0x%x \n", REG_RD(SCALE_REV_SLICE_CFG));
		} else {
			g_path->is_last_slice = 0;
			REG_MWR(SCALE_REV_SLICE_CFG, SCALE_IS_LAST_SLICE_BIT, 0);
		}
		g_path->slice_in_height += g_path->slice_height;
	}

	_scale_reg_trace();
	dcam_glb_reg_owr(SCALE_CTRL, SCALE_START_BIT, DCAM_CONTROL_REG);
	atomic_inc(&g_path->start_flag);
	SCALE_TRACE("SCALE DRV: continue %x.\n", REG_RD(SCALE_CFG));

	return rtn;
}

int32_t scale_stop(void)
{
	unsigned long flag;
	enum scale_drv_rtn rtn = SCALE_RTN_SUCCESS;

	spin_lock_irqsave(&scale_lock, flag);
	if (atomic_read(&g_path->start_flag)) {
		s_wait_flag = 1;
		spin_unlock_irqrestore(&scale_lock, flag);
		if (down_interruptible(&scale_done_sema)) {
			printk("scale_stop down error!\n");
		}
	} else {
		spin_unlock_irqrestore(&scale_lock, flag);
	}

	dcam_glb_reg_mwr(SCALE_BASE, SCALE_PATH_EB_BIT, 0, DCAM_CFG_REG);

	SCALE_TRACE("SCALE DRV: stop is OK.\n");
	return rtn;
}

int32_t scale_reg_isr(enum scale_irq_id id, scale_isr_func user_func, void* u_data)
{
	enum scale_drv_rtn rtn = SCALE_RTN_SUCCESS;
	unsigned long flag;

	if(id >= SCALE_IRQ_NUMBER) {
		rtn = SCALE_RTN_ISR_ID_ERR;
	} else {
		spin_lock_irqsave(&scale_lock, flag);
		g_path->user_func = user_func;
		g_path->user_data = u_data;
		spin_unlock_irqrestore(&scale_lock, flag);
		if (user_func) {
			dcam_reg_isr(DCAM_PATH2_DONE, _scale_isr_root, (void*)NULL);
			dcam_reg_isr(DCAM_PATH2_SLICE_DONE, _scale_isr_root, (void*)NULL);
		} else {
			dcam_reg_isr(DCAM_PATH2_DONE, NULL, (void*)NULL);
			dcam_reg_isr(DCAM_PATH2_SLICE_DONE, NULL, (void*)NULL);
		}
	}

	return rtn;
}

int32_t scale_cfg(enum scale_cfg_id id, void *param)
{
	enum scale_drv_rtn rtn = SCALE_RTN_SUCCESS;

	switch (id) {

	case SCALE_INPUT_SIZE:
	{
		struct scale_size *size = (struct scale_size*)param;
		uint32_t reg_val = 0;

		SCALE_CHECK_PARAM_ZERO_POINTER(param);

		SCALE_TRACE("SCALE DRV: SCALE_INPUT_SIZE {%d %d} \n", size->w, size->h);
		if (size->w > SCALE_FRAME_WIDTH_MAX ||
			size->h > SCALE_FRAME_HEIGHT_MAX) {
			rtn = SCALE_RTN_SRC_SIZE_ERR;
			dcam_resize_end();
		} else {
			reg_val = size->w | (size->h << 16);
			REG_MWR(SCALE_REV_BURST_IN_CFG, SCALE_BURST_SUB_EB_BIT, 0);
			REG_MWR(SCALE_REV_BURST_IN_CFG, SCALE_BURST_SUB_SAMPLE_MASK, 0);
			REG_MWR(SCALE_REV_BURST_IN_CFG, SCALE_BURST_SRC_WIDTH_MASK, size->w);
			g_path->input_size.w = size->w;
			g_path->input_size.h = size->h;
		}
		break;
	}

	case SCALE_INPUT_RECT:
	{
		struct scale_rect *rect = (struct scale_rect*)param;
		uint32_t reg_val = 0;

		SCALE_CHECK_PARAM_ZERO_POINTER(param);

		SCALE_TRACE("SCALE DRV: SCALE_PATH_INPUT_RECT {%d %d %d %d} \n",
			rect->x,
			rect->y,
			rect->w,
			rect->h);

		if (rect->x > SCALE_FRAME_WIDTH_MAX ||
			rect->y > SCALE_FRAME_HEIGHT_MAX ||
			rect->w > SCALE_FRAME_WIDTH_MAX ||
			rect->h > SCALE_FRAME_HEIGHT_MAX) {
			rtn = SCALE_RTN_TRIM_SIZE_ERR;
			dcam_resize_end();
		} else {
			reg_val = rect->x | (rect->y << 16);
			REG_WR(SCALE_REV_BURST_IN_TRIM_START, reg_val);
			REG_WR(SCALE_TRIM_START, 0);
			reg_val = rect->w | (rect->h << 16);
			REG_WR(SCALE_REV_BURST_IN_TRIM_SIZE, reg_val);
			REG_WR(SCALE_TRIM_SIZE, reg_val);
			REG_WR(SCALE_SRC_SIZE, reg_val);
			memcpy((void*)&g_path->input_rect,
				(void*)rect,
				sizeof(struct scale_rect));
		}
		break;
	}

	case SCALE_INPUT_FORMAT:
	{
		enum scale_fmt format = *(enum scale_fmt*)param;

		SCALE_CHECK_PARAM_ZERO_POINTER(param);

		g_path->input_format = format;
		if (SCALE_YUV422 == format ||
			SCALE_YUV420 == format ||
			SCALE_YUV420_3FRAME == format) {
			REG_MWR(SCALE_REV_BURST_IN_CFG, SCALE_BURST_INPUT_MODE_MASK, (g_path->input_format << 16));
		} else {
			rtn = SCALE_RTN_IN_FMT_ERR;
			g_path->input_format = SCALE_FTM_MAX;
			dcam_resize_end();
		}
		break;

	}

	case SCALE_INPUT_ADDR:
	{
		struct scale_addr *p_addr = (struct scale_addr*)param;

		SCALE_CHECK_PARAM_ZERO_POINTER(param);

		if (SCALE_YUV_ADDR_INVALIDE(p_addr->yaddr, p_addr->uaddr, p_addr->vaddr)) {
			rtn = SCALE_RTN_ADDR_ERR;
			dcam_resize_end();
		} else {
			g_path->input_addr.yaddr = p_addr->yaddr;
			g_path->input_addr.uaddr = p_addr->uaddr;
			g_path->input_addr.vaddr = p_addr->vaddr;
			REG_WR(SCALE_FRM_IN_Y, p_addr->yaddr);
			REG_WR(SCALE_FRM_IN_U, p_addr->uaddr);
			REG_WR(SCALE_FRM_IN_V, p_addr->vaddr);
		}
		break;
	}

	case SCALE_INPUT_ENDIAN:
	{
		struct scale_endian_sel *endian = (struct scale_endian_sel*)param;

		SCALE_CHECK_PARAM_ZERO_POINTER(param);

		if (endian->y_endian >= SCALE_ENDIAN_MAX ||
			endian->uv_endian >= SCALE_ENDIAN_MAX) {
			rtn = SCALE_RTN_ENDIAN_ERR;
			dcam_resize_end();
		} else {
			dcam_glb_reg_owr(SCALE_ENDIAN_SEL,(SCALE_AXI_RD_ENDIAN_BIT | SCALE_AXI_WR_ENDIAN_BIT), DCAM_ENDIAN_REG);
			dcam_glb_reg_mwr(SCALE_ENDIAN_SEL, SCALE_INPUT_Y_ENDIAN_MASK, endian->y_endian, DCAM_ENDIAN_REG);
			dcam_glb_reg_mwr(SCALE_ENDIAN_SEL, SCALE_INPUT_UV_ENDIAN_MASK, (endian->uv_endian << 2), DCAM_ENDIAN_REG);
		}
		break;
	}

	case SCALE_OUTPUT_SIZE:
	{
		struct scale_size *size = (struct scale_size*)param;
		uint32_t reg_val = 0;

		SCALE_CHECK_PARAM_ZERO_POINTER(param);

		SCALE_TRACE("SCALE DRV: SCALE_OUTPUT_SIZE {%d %d} \n", size->w, size->h);
		if (size->w > SCALE_FRAME_WIDTH_MAX ||
			size->h > SCALE_FRAME_HEIGHT_MAX) {
			rtn = SCALE_RTN_SRC_SIZE_ERR;
			dcam_resize_end();
		} else {
			reg_val = size->w | (size->h << 16);
			REG_WR(SCALE_DST_SIZE, reg_val);
			g_path->output_size.w = size->w;
			g_path->output_size.h = size->h;
		}
		break;
	}

	case SCALE_OUTPUT_FORMAT:
	{
		enum scale_fmt format = *(enum scale_fmt*)param;

		SCALE_CHECK_PARAM_ZERO_POINTER(param);

		g_path->output_format = format;
		if (SCALE_YUV422 == format) {
			REG_MWR(SCALE_CFG, SCALE_OUTPUT_MODE_MASK, (0 << 6));
		} else if (SCALE_YUV420 == format) {
			REG_MWR(SCALE_CFG, SCALE_OUTPUT_MODE_MASK, (1 << 6));
		} else if (SCALE_YUV420_3FRAME == format) {
			REG_MWR(SCALE_CFG, SCALE_OUTPUT_MODE_MASK, (3 << 6));
		} else {
			rtn = SCALE_RTN_OUT_FMT_ERR;
			g_path->output_format = SCALE_FTM_MAX;
			dcam_resize_end();
		}
		break;
	}

	case SCALE_OUTPUT_ADDR:
	{
		struct scale_addr *p_addr = (struct scale_addr*)param;

		SCALE_CHECK_PARAM_ZERO_POINTER(param);

		if (SCALE_YUV_ADDR_INVALIDE(p_addr->yaddr, p_addr->uaddr, p_addr->vaddr)) {
			rtn = SCALE_RTN_ADDR_ERR;
			dcam_resize_end();
		} else {
			g_path->output_addr.yaddr = p_addr->yaddr;
			g_path->output_addr.uaddr = p_addr->uaddr;
			g_path->output_addr.vaddr = p_addr->vaddr;
			REG_WR(SCALE_FRM_OUT_Y, p_addr->yaddr);
			REG_WR(SCALE_FRM_OUT_U, p_addr->uaddr);
			REG_WR(SCALE_FRM_OUT_V, p_addr->vaddr);
		}
		break;
	}

	case SCALE_OUTPUT_ENDIAN:
	{
		struct scale_endian_sel *endian = (struct scale_endian_sel*)param;

		SCALE_CHECK_PARAM_ZERO_POINTER(param);

		if (endian->y_endian >= SCALE_ENDIAN_MAX ||
			endian->uv_endian >= SCALE_ENDIAN_MAX) {
			rtn = SCALE_RTN_ENDIAN_ERR;
			dcam_resize_end();
		} else {
			dcam_glb_reg_owr(SCALE_ENDIAN_SEL,(SCALE_AXI_RD_ENDIAN_BIT|SCALE_AXI_WR_ENDIAN_BIT), DCAM_ENDIAN_REG);
			dcam_glb_reg_mwr(SCALE_ENDIAN_SEL, SCALE_OUTPUT_Y_ENDIAN_MASK, (endian->y_endian << 10), DCAM_ENDIAN_REG);
			dcam_glb_reg_mwr(SCALE_ENDIAN_SEL, SCALE_OUTPUT_UV_ENDIAN_MASK, (endian->uv_endian << 12), DCAM_ENDIAN_REG);
		}
		break;
	}

	case SCALE_TEMP_BUFF:
		break;

	case SCALE_SCALE_MODE:
	{
		enum scle_mode mode = *(enum scle_mode*)param;

		if (mode >= SCALE_MODE_MAX) {
			rtn = SCALE_RTN_MODE_ERR;
			dcam_resize_end();
		} else {
			g_path->scale_mode = mode;
			if (SCALE_MODE_NORMAL == mode) {
				REG_MWR(SCALE_CFG, SCALE_MODE_MASK, SCALE_MODE_NORMAL_TYPE);
			} else if(SCALE_MODE_SLICE == mode) {
				REG_MWR(SCALE_CFG, SCALE_MODE_MASK, SCALE_MODE_SLICE_TYPE);
				REG_MWR(SCALE_REV_SLICE_CFG, SCALE_SLICE_TYPE_BIT, 0);
			}else{
				REG_MWR(SCALE_CFG, SCALE_MODE_MASK, SCALE_MODE_SLICE_TYPE);
				REG_OWR(SCALE_REV_SLICE_CFG, SCALE_SLICE_TYPE_BIT);
			}
		}

		break;
	}

	case SCALE_SLICE_SCALE_HEIGHT:
	{
		uint32_t height = *(uint32_t*)param;

		SCALE_CHECK_PARAM_ZERO_POINTER(param);

		if (height > SCALE_FRAME_HEIGHT_MAX || (height % SCALE_SLICE_HEIGHT_ALIGNED)) {
			rtn = SCALE_RTN_PARA_ERR;
			dcam_resize_end();
		} else {
			g_path->slice_height = height;
			REG_MWR(SCALE_REV_SLICE_CFG, SCALE_INPUT_SLICE_HEIGHT_MASK, height);
		}
		break;
	}

	case SCALE_START:
	{
		rtn = scale_start();
		if (rtn) {
			dcam_resize_end();
		}
		break;

	}

	case SCALE_CONTINUE:
	{
		rtn = scale_continue();
		break;
	}

	case SCALE_STOP:
	{
		rtn = scale_stop();
		break;
	}

	default:
		printk("SCALE DRV: error io 0x%x \n", id);
		rtn = SCALE_RTN_IO_ID_ERR;
		break;
	}

	return -rtn;
}

int32_t scale_read_registers(uint32_t* reg_buf, uint32_t *buf_len)
{
	uint32_t *reg_addr = (uint32_t*)SCALE_BASE;

	if (NULL == reg_buf || NULL == buf_len || 0 != (*buf_len % 4)) {
		return -1;
	}

	while (buf_len != 0 && (uint32_t)reg_addr < SCALE_REG_END) {
		*reg_buf++ = REG_RD(reg_addr);
		reg_addr++;
		*buf_len -= 4;
	}

	*buf_len = (uint32_t)reg_addr - SCALE_BASE;
	return 0;
}

static int32_t _scale_cfg_scaler(void)
{
	enum scale_drv_rtn rtn = SCALE_RTN_SUCCESS;

	rtn = _scale_calc_sc_size();
	SCALE_RTN_IF_ERR;

	if (g_path->sc_input_size.w != g_path->output_size.w ||
		g_path->sc_input_size.h != g_path->output_size.h ||
		SCALE_YUV420 == g_path->input_format) {
		REG_MWR(SCALE_CFG, SCALE_BYPASS_BIT, 0);
		rtn = _scale_set_sc_coeff();
	} else {
		REG_OWR(SCALE_CFG, SCALE_BYPASS_BIT);
	}

	return rtn;
}

static int32_t _scale_calc_sc_size(void)
{
	uint32_t reg_val = 0;
	enum scale_drv_rtn rtn = SCALE_RTN_SUCCESS;
	uint32_t div_factor = 1;
	uint32_t i = 0, pixel_aligned_num = 0;

	if (g_path->input_rect.w > (g_path->output_size.w * SCALE_SC_COEFF_MAX * (1 << SCALE_DECI_FAC_MAX)) ||
		g_path->input_rect.h > (g_path->output_size.h * SCALE_SC_COEFF_MAX * (1 << SCALE_DECI_FAC_MAX)) ||
		g_path->input_rect.w * SCALE_SC_COEFF_MAX < g_path->output_size.w ||
		g_path->input_rect.h * SCALE_SC_COEFF_MAX < g_path->output_size.h) {
		SCALE_TRACE("SCALE DRV: Target too small or large \n");
		rtn = SCALE_RTN_SC_ERR;
	} else {
		g_path->sc_input_size.w = g_path->input_rect.w;
		g_path->sc_input_size.h = g_path->input_rect.h;
		if (g_path->input_rect.w > g_path->output_size.w * SCALE_SC_COEFF_MAX ||
			g_path->input_rect.h > g_path->output_size.h * SCALE_SC_COEFF_MAX) {
			for (i = 0; i < SCALE_DECI_FAC_MAX; i++) {
				div_factor = (uint32_t)(SCALE_SC_COEFF_MAX * (1 << (1 + i)));
				if (g_path->input_rect.w <= (g_path->output_size.w * div_factor) &&
					g_path->input_rect.h <= (g_path->output_size.h * div_factor)) {
					break;
				}
			}
			g_path->sc_deci_val = (1 << (1 + i));
			pixel_aligned_num = (g_path->sc_deci_val >= SCALE_PIXEL_ALIGNED) ? g_path->sc_deci_val : SCALE_PIXEL_ALIGNED;
			g_path->sc_input_size.w = g_path->input_rect.w >> (1 + i);
			g_path->sc_input_size.h = g_path->input_rect.h >> (1 + i);
			if ((g_path->sc_input_size.w % pixel_aligned_num) ||
				(g_path->sc_input_size.h % pixel_aligned_num)) {
				g_path->sc_input_size.w = g_path->sc_input_size.w / pixel_aligned_num * pixel_aligned_num;
				g_path->sc_input_size.h = g_path->sc_input_size.h / pixel_aligned_num * pixel_aligned_num;
				g_path->input_rect.w = g_path->sc_input_size.w << (1 + i);
				g_path->input_rect.h = g_path->sc_input_size.h << (1 + i);
			}

			REG_WR(SCALE_TRIM_START, 0);
			reg_val = g_path->sc_input_size.w | (g_path->sc_input_size.h << 16);
			REG_WR(SCALE_TRIM_SIZE, reg_val);
			REG_WR(SCALE_SRC_SIZE, reg_val);

			REG_OWR(SCALE_REV_BURST_IN_CFG, SCALE_BURST_SUB_EB_BIT);
			REG_MWR(SCALE_REV_BURST_IN_CFG, SCALE_BURST_SUB_SAMPLE_MASK, (i << 13));
			REG_MWR(SCALE_REV_BURST_IN_CFG, SCALE_BURST_SRC_WIDTH_MASK, g_path->input_size.w);
			reg_val = g_path->input_rect.x | (g_path->input_rect.y << 16);
			REG_WR(SCALE_REV_BURST_IN_TRIM_START, reg_val);
			reg_val = g_path->input_rect.w | (g_path->input_rect.h << 16);
			REG_WR(SCALE_REV_BURST_IN_TRIM_SIZE, reg_val);
		}

	}

	return rtn;
}

static int32_t _scale_set_sc_coeff(void)
{
	uint32_t i = 0;
	uint32_t h_coeff_addr = SCALE_BASE;
	uint32_t v_coeff_addr = SCALE_BASE;
	uint32_t v_chroma_coeff_addr = SCALE_BASE;
	uint32_t *tmp_buf = NULL;
	uint32_t *h_coeff = NULL;
	uint32_t *v_coeff = NULL;
	uint32_t *v_chroma_coeff = NULL;
	uint32_t scale2yuv420 = 0;
	uint8_t y_tap = 0;
	uint8_t uv_tap = 0;

	h_coeff_addr += SC_COEFF_H_TAB_OFFSET;
	v_coeff_addr += SC_COEFF_V_TAB_OFFSET;
	v_chroma_coeff_addr += SC_COEFF_V_CHROMA_TAB_OFFSET;

	if(SCALE_YUV420 == g_path->output_format)
		scale2yuv420 = 1;

	tmp_buf = get_scale_coeff_addr();
	if (NULL == tmp_buf) {
		printk("SCALE DRV: No mem to alloc coeff buffer! \n");
		return SCALE_RTN_NO_MEM;
	}

	h_coeff = tmp_buf;
	v_coeff = tmp_buf + (SC_COEFF_COEF_SIZE/4);
	v_chroma_coeff = v_coeff + (SC_COEFF_COEF_SIZE/4);

	if (!(GenScaleCoeff((int16_t)g_path->sc_input_size.w,
		(int16_t)g_path->sc_input_size.h,
		(int16_t)g_path->output_size.w,
		(int16_t)g_path->output_size.h,
		h_coeff,
		v_coeff,
		v_chroma_coeff,
		scale2yuv420,
		&y_tap,
		&uv_tap,
		tmp_buf + (SC_COEFF_COEF_SIZE*3/4),
		SC_COEFF_TMP_SIZE))) {
		printk("SCALE DRV: _scale_set_sc_coeff error! \n");
		return SCALE_RTN_GEN_COEFF_ERR;
	}

	for (i = 0; i < SC_COEFF_H_NUM; i++) {
		REG_WR(h_coeff_addr, *h_coeff);
		h_coeff_addr += 4;
		h_coeff++;
	}

	for (i = 0; i < SC_COEFF_V_NUM; i++) {
		REG_WR(v_coeff_addr, *v_coeff);
		v_coeff_addr += 4;
		v_coeff++;
	}

	for (i = 0; i < SC_COEFF_V_CHROMA_NUM; i++) {
		REG_WR(v_chroma_coeff_addr, *v_chroma_coeff);
		v_chroma_coeff_addr += 4;
		v_chroma_coeff++;
	}

	REG_MWR(SCALE_CFG, (BIT_19 | BIT_18 | BIT_17 | BIT_16), ((y_tap & 0x0F) << 16));
	REG_MWR(SCALE_CFG, (BIT_15 | BIT_14 | BIT_13 | BIT_12 | BIT_11), ((uv_tap & 0x1F) << 11));

	return SCALE_RTN_SUCCESS;
}

int _scale_isr_root(struct dcam_frame* dcam_frm, void* u_data)
{
	struct scale_frame frame;
	unsigned long flag;

	(void)dcam_frm; (void)u_data;

	SCALE_TRACE("SCALE DRV: _scale_isr_root \n");
	spin_lock_irqsave(&scale_lock, flag);
	if (g_path->user_func) {
		memset(&frame, 0, sizeof(frame));
		frame.yaddr = g_path->output_addr.yaddr;
		frame.uaddr = g_path->output_addr.uaddr;
		frame.vaddr = g_path->output_addr.vaddr;
		frame.width = g_path->output_size.w;
		if (SCALE_MODE_NORMAL != g_path->scale_mode) {
			frame.height_uv = REG_RD(SCALE_REV_SLICE_O_VCNT);
			frame.height_uv = (frame.height_uv >> 16) & SCALE_OUTPUT_SLICE_HEIGHT_MASK;
			frame.height = REG_RD(SCALE_REV_SLICE_O_VCNT);
			frame.height = frame.height & SCALE_OUTPUT_SLICE_HEIGHT_MASK;
			g_path->slice_out_height += frame.height;
		} else {
			frame.height = g_path->output_size.h;
		}
		g_path->user_func(&frame, g_path->user_data);
	}

	atomic_dec(&g_path->start_flag);
	if (s_wait_flag) {
		up(&scale_done_sema);
		s_wait_flag = 0;
	}
	if (SCALE_MODE_NORMAL == g_path->scale_mode ||
		(SCALE_MODE_NORMAL != g_path->scale_mode && g_path->output_size.h == g_path->slice_out_height)) {
		dcam_resize_end();
	}
	spin_unlock_irqrestore(&scale_lock, flag);

	return 0;
}

static void _scale_reg_trace(void)
{
#ifdef SCALE_DRV_DEBUG
	uint32_t addr = 0;

	for (addr = SCALE_REG_START; addr <= SCALE_REG_END; addr += 16) {
		printk("%x: %x %x %x %x \n",
			 addr,
			REG_RD(addr),
			REG_RD(addr + 4),
			REG_RD(addr + 8),
			REG_RD(addr + 12));
	}
#endif
}
