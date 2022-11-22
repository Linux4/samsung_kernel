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
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/interrupt.h>
#include <asm/io.h>
#include <video/sprd_rot_k.h>
#include <mach/hardware.h>
#include <mach/sci.h>
#include "img_rot.h"
#include "rot_drv.h"
#include "dcam_drv.h"

#define ALGIN_FOUR 0x03

int rot_k_module_en(struct device_node *dn)
{
	int ret = 0;

	ret = dcam_module_en(dn);

	if (ret) {
		printk("dcam_module_en, failed  %d \n", ret);
	}

	return ret;
}

int rot_k_module_dis(struct device_node *dn)
{
	int ret = 0;

	ret = dcam_module_dis(dn);

	if (ret) {
		printk("rot_k_module_dis, failed  %d \n", ret);
	}

	return ret;
}

static void rot_k_ahb_reset(void)
{
	sci_glb_set(REG_ROTATION_AHB_RESET, ROT_AHB_RESET_BIT);
	sci_glb_clr(REG_ROTATION_AHB_RESET, ROT_AHB_RESET_BIT);
}

static void rot_k_set_src_addr(uint32_t src_addr)
{
	REG_WR(REG_ROTATION_SRC_ADDR, src_addr);
}

static void rot_k_set_dst_addr(uint32_t dst_addr)
{
	REG_WR(REG_ROTATION_DST_ADDR, dst_addr);
}

static void rot_k_set_img_size(ROT_SIZE_T * size)
{
	REG_WR(REG_ROTATION_OFFSET_START, 0x00000000);
	REG_WR(REG_ROTATION_IMG_SIZE,
		((size->w & 0x1FFF) | ((size->h & 0x1FFF) << 13)));
	REG_WR(REG_ROTATION_ORIGWIDTH, (size->w & 0x1FFF));
}

static void rot_k_set_endian(ROT_ENDIAN_E src_end, ROT_ENDIAN_E dst_end)
{
	dcam_glb_reg_awr(REG_ROTATION_ENDIAN_SEL,
		(~(ROT_RD_ENDIAN_MASK |ROT_WR_ENDIAN_MASK)), DCAM_ENDIAN_REG);

	dcam_glb_reg_owr(REG_ROTATION_ENDIAN_SEL,
		(ROT_AXI_RD_WORD_ENDIAN_BIT |
		ROT_AXI_WR_WORD_ENDIAN_BIT |
		(src_end << 16) |(dst_end << 14)), DCAM_ENDIAN_REG);
}

static void rot_k_set_pixel_mode(ROT_PIXEL_FORMAT_E pixel_format)
{
	REG_AWR(REG_ROTATION_PATH_CFG, (~ROT_PIXEL_FORMAT_BIT));
	REG_OWR(REG_ROTATION_PATH_CFG, ((pixel_format & 0x1) << 1));
	return;
}

static void rot_k_set_UV_mode(ROT_UV_MODE_E uv_mode)
{
	REG_AWR(REG_ROTATION_PATH_CFG, (~ROT_UV_MODE_BIT));
	REG_OWR(REG_ROTATION_PATH_CFG, ((uv_mode & 0x1) << 4));
	return;
}

static void rot_k_set_dir(ROT_ANGLE_E angle)
{
	REG_AWR(REG_ROTATION_PATH_CFG, (~ROT_MODE_MASK));
	REG_OWR(REG_ROTATION_PATH_CFG, ((angle & 0x3) << 2));
}

static void rot_k_enable(void)
{
	REG_OWR(REG_ROTATION_PATH_CFG, ROT_EB_BIT);
}

static void rot_k_disable(void)
{
	REG_AWR(REG_ROTATION_PATH_CFG, (~ROT_EB_BIT));
}

static void rot_k_start(void)
{
#if defined(CONFIG_ARCH_SCX30G)
	REG_OWR(REG_ROTATION_PATH_CFG, ROT_START_BIT);
#else
	dcam_glb_reg_owr(REG_ROTATION_CTRL, ROT_START_BIT, DCAM_CONTROL_REG);
#endif
}


int rot_k_isr(struct dcam_frame* dcam_frm, void* u_data)
{
	unsigned long flag;
	rot_isr_func user_isr_func;
	struct rot_drv_private *private = (struct rot_drv_private *)u_data;

	(void)dcam_frm;

	if (!private) {
		goto isr_exit;
	}

	dcam_rotation_end();

	spin_lock_irqsave(&private->rot_drv_lock, flag);
	user_isr_func = private->user_isr_func;
	if (user_isr_func) {
		(*user_isr_func)(private->rot_fd);
	}
	spin_unlock_irqrestore(&private->rot_drv_lock, flag);

isr_exit:
	return 0;
}

int rot_k_isr_reg(rot_isr_func user_func,struct rot_drv_private *drv_private)
{
	int rtn = 0;
	unsigned long flag;

	if (user_func) {
		if (!drv_private) {
			rtn = -EFAULT;
			goto reg_exit;
		}

		spin_lock_irqsave(&drv_private->rot_drv_lock, flag);
		drv_private->user_isr_func = user_func;
		spin_unlock_irqrestore(&drv_private->rot_drv_lock, flag);

		dcam_reg_isr(DCAM_ROT_DONE, rot_k_isr, drv_private);
	} else {
		dcam_reg_isr(DCAM_ROT_DONE, NULL, NULL);
	}

reg_exit:
	return rtn;
}

int rot_k_is_end(ROT_PARAM_CFG_T *s)
{
	return s->is_end ;
}

static uint32_t rot_k_get_end_mode(ROT_PARAM_CFG_T *s)
{
	uint32_t ret = 1;

	switch (s->format) {
	case ROT_YUV422:
	case ROT_YUV420:
		ret = 0;
		break;
	case ROT_RGB565:
		ret = 1;
		break;
	default:
		ret = 1;
		break;
	}
	return ret;
}

static ROT_PIXEL_FORMAT_E rot_k_get_pixel_format(ROT_PARAM_CFG_T *s)
{
	ROT_PIXEL_FORMAT_E ret = ROT_ONE_BYTE;

	switch (s->format) {
	case ROT_YUV422:
	case ROT_YUV420:
		ret = ROT_ONE_BYTE;
		break;
	case ROT_RGB565:
		ret = ROT_TWO_BYTES;
		break;
	default:
		ret = ROT_ONE_BYTE;
		break;
	}

	return ret;
}

static int rot_k_set_y_param(ROT_CFG_T * param_ptr,ROT_PARAM_CFG_T *s)
{
	int ret = 0;

	if (!param_ptr || !s) {
		ret = -EFAULT;
		goto param_exit;
	}

	memcpy((void *)&(s->img_size), (void *)&(param_ptr->img_size),
		sizeof(ROT_SIZE_T));
	memcpy((void *)&(s->src_addr), (void *)&(param_ptr->src_addr),
		sizeof(ROT_ADDR_T));
	memcpy((void *)&(s->dst_addr), (void *)&(param_ptr->dst_addr),
		sizeof(ROT_ADDR_T));

	s->s_addr = param_ptr->src_addr.y_addr;
	s->d_addr = param_ptr->dst_addr.y_addr;
	s->format = param_ptr->format;
	s->angle = param_ptr->angle;
	s->pixel_format = rot_k_get_pixel_format(s);
	s->is_end = rot_k_get_end_mode(s);
	s->uv_mode = ROT_NORMAL;
	s->src_endian = 1;/*param_ptr->src_addr;*/
	s->dst_endian = 1;/*param_ptr->dst_endian;*/

param_exit:
	return ret;
}

int rot_k_set_UV_param(ROT_PARAM_CFG_T *s)
{
	s->s_addr = s->src_addr.u_addr;
	s->d_addr = s->dst_addr.u_addr;
	s->img_size.w >>= 0x01;
	s->pixel_format = ROT_TWO_BYTES;
	if (ROT_YUV422 == s->format){
		s->uv_mode = ROT_UV422;
	} else if (ROT_YUV420 == s->format) {
		s->img_size.h >>= 0x01;
	}
	return 0;
}

void rot_k_register_cfg(ROT_PARAM_CFG_T *s)
{
	rot_k_ahb_reset();
	dcam_rotation_start();
	rot_k_set_src_addr(s->s_addr);
	rot_k_set_dst_addr(s->d_addr);
	rot_k_set_img_size(&(s->img_size));
	rot_k_set_pixel_mode(s->pixel_format);
	rot_k_set_UV_mode(s->uv_mode);
	rot_k_set_dir(s->angle);
	rot_k_set_endian(s->src_endian, s->dst_endian);
	rot_k_enable();
	rot_k_start();
	ROTATE_TRACE("rot_k_register_cfg.\n");
}

void rot_k_close(void)
{
	rot_k_disable();
}

static int rot_k_check_param(ROT_CFG_T * param_ptr)
{
	if (NULL == param_ptr) {
		printk("Rotation: the param ptr is null.\n");
		return -1;
	}

	if ((param_ptr->src_addr.y_addr & ALGIN_FOUR)
		|| (param_ptr->src_addr.u_addr & ALGIN_FOUR)
		|| (param_ptr->src_addr.v_addr & ALGIN_FOUR)
		|| (param_ptr->dst_addr.y_addr & ALGIN_FOUR)
		|| (param_ptr->dst_addr.u_addr & ALGIN_FOUR)
		|| (param_ptr->dst_addr.v_addr & ALGIN_FOUR)) {
		printk("Rotation: the addr not algin.\n");
		return -1;
	}

	if (!(ROT_YUV422 == param_ptr->format
		|| ROT_YUV420 == param_ptr->format
		||ROT_RGB565 == param_ptr->format)) {
		printk("Rotation: data for err : %d.\n", param_ptr->format);
		return -1;
	}

	if (ROT_MIRROR < param_ptr->angle) {
		printk("Rotation: data angle err : %d.\n", param_ptr->angle);
		return -1;
	}
#if 0
	if (ROT_ENDIAN_MAX <= param_ptr->src_endian ||
		ROT_ENDIAN_MAX <= param_ptr->dst_endian ) {
		ROTATE_TRACE("Rotation: endian err : %d %d.\n", param_ptr->src_endian,
			param_ptr->dst_endian);
		return -1;
	}
#endif
	return 0;
}

int rot_k_io_cfg(ROT_CFG_T * param_ptr,ROT_PARAM_CFG_T *s)
{
	int ret = 0;
	ROT_CFG_T *p = param_ptr;

	ROTATE_TRACE("rot_k_io_cfg start \n");
	ROTATE_TRACE("w=%d, h=%d \n", p->img_size.w, p->img_size.h);
	ROTATE_TRACE("format=%d, angle=%d \n", p->format, p->angle);
	ROTATE_TRACE("s.y=%x, s.u=%x, s.v=%x \n", p->src_addr.y_addr, p->src_addr.u_addr, p->src_addr.v_addr);
	ROTATE_TRACE("d.y=%x, d.u=%x, d.v=%x \n", p->dst_addr.y_addr, p->dst_addr.u_addr, p->dst_addr.v_addr);

	ret = rot_k_check_param(param_ptr);

	if (0 == ret)
		ret = rot_k_set_y_param(param_ptr,s);

	return ret;
}
