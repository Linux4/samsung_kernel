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
#include <linux/clk.h>
#include <linux/err.h>
#include <asm/io.h>
#include <linux/kthread.h>
#include <mach/hardware.h>
#include <mach/irqs.h>
#include <mach/sci.h>
#include <linux/vmalloc.h>
#include <linux/wakelock.h>
#include "dcam_drv.h"
#include "gen_scale_coef.h"

#define LOCAL static
/*#define LOCAL*/

//#define DCAM_DRV_DEBUG
#define DCAM_LOWEST_ADDR                               0x800
#define DCAM_ADDR_INVALID(addr)                       ((uint32_t)(addr) < DCAM_LOWEST_ADDR)
#define DCAM_YUV_ADDR_INVALID(y,u,v) \
	(DCAM_ADDR_INVALID(y) && \
	DCAM_ADDR_INVALID(u) && \
	DCAM_ADDR_INVALID(v))

#define DCAM_SC1_H_TAB_OFFSET                          0x400
#define DCAM_SC1_V_TAB_OFFSET                          0x4F0
#define DCAM_SC1_V_CHROMA_TAB_OFFSET                   0x8F0

#define DCAM_SC2_H_TAB_OFFSET                          0x1400
#define DCAM_SC2_V_TAB_OFFSET                          0x14F0
#define DCAM_SC2_V_CHROMA_TAB_OFFSET                   0x18F0

#define DCAM_SC_COEFF_BUF_SIZE                         (24 << 10)
#define DCAM_SC_COEFF_COEF_SIZE                        (1 << 10)
#define DCAM_SC_COEFF_TMP_SIZE                         (21 << 10)

#define DCAM_SC_H_COEF_SIZE                            (0xC0)
#define DCAM_SC_V_COEF_SIZE                            (0x210)
#define DCAM_SC_V_CHROM_COEF_SIZE                      (0x210)

#define DCAM_SC_COEFF_H_NUM                            (DCAM_SC_H_COEF_SIZE/4)
#define DCAM_SC_COEFF_V_NUM                            (DCAM_SC_V_COEF_SIZE/4)
#define DCAM_SC_COEFF_V_CHROMA_NUM                     (DCAM_SC_V_CHROM_COEF_SIZE/4)

#define DCAM_AXI_STOP_TIMEOUT                          100
#define DCAM_CLK_DOMAIN_AHB                            1
#define DCAM_CLK_DOMAIN_DCAM                           0
#define DCAM_PATH_TIMEOUT                              msecs_to_jiffies(500)
#define DCAM_FRM_QUEUE_LENGTH                          4

#define DCAM_CHECK_PARAM_ZERO_POINTER(n) \
	do { \
		if (0 == (int)(n)) \
			return -DCAM_RTN_PARA_ERR; \
	} while(0)

#define DCAM_CLEAR(a) \
	do { \
		memset((void *)(a), 0, sizeof(*(a))); \
	} while(0)

#define DEBUG_STR                                      "Error L %d, %s \n"
#define DEBUG_ARGS                                     __LINE__,__FUNCTION__
#define DCAM_RTN_IF_ERR \
	do { \
		if(rtn) { \
			printk(DEBUG_STR, DEBUG_ARGS); \
			return -(rtn); \
		} \
	} while(0)

#define DCAM_IRQ_LINE_MASK                             0x0007FFFFUL

typedef void (*dcam_isr)(void);

enum {
	DCAM_FRM_UNLOCK = 0,
	DCAM_FRM_LOCK_WRITE = 0x10011001,
	DCAM_FRM_LOCK_READ = 0x01100110
};

enum {
	DCAM_ST_STOP = 0,
	DCAM_ST_START,
};

#define DCAM_IRQ_ERR_MASK \
	((1 << DCAM_PATH0_OV) | (1 << DCAM_PATH1_OV) | (1 << DCAM_PATH2_OV) | \
	(1 << DCAM_SN_LINE_ERR) | (1 << DCAM_SN_FRAME_ERR) | \
	(1 << DCAM_ISP_OV) | (1 << DCAM_MIPI_OV))

#define DCAM_IRQ_JPEG_OV_MASK                          (1 << DCAM_JPEG_BUF_OV)

#define DCAM_CHECK_ZERO(a) \
	do { \
		if (DCAM_ADDR_INVALID(a)) { \
			printk("DCAM, zero pointer \n"); \
			printk(DEBUG_STR, DEBUG_ARGS); \
			return -EFAULT; \
		} \
	} while(0)

#define DCAM_CHECK_ZERO_VOID(a) \
	do { \
		if (DCAM_ADDR_INVALID(a)) { \
			printk("DCAM, zero pointer \n"); \
			printk(DEBUG_STR, DEBUG_ARGS); \
			return; \
		} \
	} while(0)

struct dcam_cap_desc {
	uint32_t                   interface;
	uint32_t                   input_format;
	uint32_t                   frame_deci_factor;
	uint32_t                   img_x_deci_factor;
	uint32_t                   img_y_deci_factor;
};

struct dcam_path_valid {
	uint32_t                   input_size    :1;
	uint32_t                   input_rect    :1;
	uint32_t                   output_size   :1;
	uint32_t                   output_format :1;
	uint32_t                   src_sel       :1;
	uint32_t                   data_endian   :1;
	uint32_t                   frame_deci    :1;
	uint32_t                   scale_tap     :1;
	uint32_t                   v_deci        :1;
};

struct dcam_frm_queue {
	struct dcam_frame          *frm_array[DCAM_FRM_QUEUE_LENGTH];
	uint32_t                   valid_cnt;
};

struct dcam_path_desc {
	struct dcam_size           input_size;
	struct dcam_rect           input_rect;
	struct dcam_size           sc_input_size;
	struct dcam_size           output_size;
	struct dcam_frame          input_frame;
	struct dcam_frm_queue      frame_queue;
	struct dcam_frame          *output_frame_head;
	struct dcam_frame          *output_frame_cur;
	struct dcam_endian_sel     data_endian;
	struct dcam_sc_tap         scale_tap;
	struct dcam_deci           deci_val;
	struct dcam_path_valid     valid_param;
	uint32_t                   output_frame_count;
	uint32_t                   output_format;
	uint32_t                   src_sel;
	uint32_t                   rot_mode;
	uint32_t                   frame_deci;
	uint32_t                   valide;
	uint32_t                   status;
	struct semaphore           tx_done_sema;
	struct semaphore           sof_sema;
	uint32_t                   wait_for_done;
	uint32_t                   is_update;
	uint32_t                   wait_for_sof;
	uint32_t                   need_stop;
};

struct dcam_module {
	uint32_t                   dcam_mode;
	uint32_t                   module_addr;
	struct dcam_cap_desc       dcam_cap;
	struct dcam_path_desc      dcam_path0;
	struct dcam_path_desc      dcam_path1;
	struct dcam_path_desc      dcam_path2;
	struct dcam_frame          path0_frame[DCAM_PATH_0_FRM_CNT_MAX];
	struct dcam_frame          path1_frame[DCAM_PATH_1_FRM_CNT_MAX];
	struct dcam_frame          path2_frame[DCAM_PATH_2_FRM_CNT_MAX];
	struct semaphore           stop_sema;
	uint32_t                   wait_stop;
	struct semaphore           resize_done_sema;
	uint32_t                   wait_resize_done;
	struct semaphore           rotation_done_sema;
	uint32_t                   wait_rotation_done;
	uint32_t                   err_happened;
	struct semaphore           scale_coeff_mem_sema;
};

LOCAL atomic_t                 s_dcam_users = ATOMIC_INIT(0);
LOCAL atomic_t                 s_resize_flag = ATOMIC_INIT(0);
LOCAL atomic_t                 s_rotation_flag = ATOMIC_INIT(0);
LOCAL struct clk*              s_dcam_clk = NULL;
LOCAL struct dcam_module*      s_p_dcam_mod = 0;
LOCAL uint32_t                 s_dcam_irq = 0x5A0000A5;
LOCAL dcam_isr_func            s_user_func[DCAM_IRQ_NUMBER];
LOCAL void*                    s_user_data[DCAM_IRQ_NUMBER];
LOCAL uint32_t                 *s_dcam_scaling_coeff_addr = NULL;
LOCAL struct wake_lock         dcam_wakelock;

LOCAL DEFINE_MUTEX(dcam_sem);
LOCAL DEFINE_MUTEX(dcam_scale_sema);
LOCAL DEFINE_MUTEX(dcam_rot_sema);
LOCAL DEFINE_MUTEX(dcam_module_sema);
LOCAL DEFINE_SPINLOCK(dcam_mod_lock);
LOCAL DEFINE_SPINLOCK(dcam_lock);
LOCAL DEFINE_SPINLOCK(dcam_glb_reg_cfg_lock);
LOCAL DEFINE_SPINLOCK(dcam_glb_reg_control_lock);
LOCAL DEFINE_SPINLOCK(dcam_glb_reg_mask_lock);
LOCAL DEFINE_SPINLOCK(dcam_glb_reg_clr_lock);
LOCAL DEFINE_SPINLOCK(dcam_glb_reg_ahbm_sts_lock);
LOCAL DEFINE_SPINLOCK(dcam_glb_reg_endian_lock);

LOCAL void        _dcam_path0_set(void);
LOCAL void        _dcam_path1_set(void);
LOCAL void        _dcam_path2_set(void);
LOCAL void        _dcam_frm_clear(enum dcam_path_index path_index);
LOCAL void        _dcam_link_frm(uint32_t base_id);
LOCAL int32_t     _dcam_path_set_next_frm(enum dcam_path_index path_index, uint32_t is_1st_frm);
/*LOCAL int32_t     _dcam_path_trim(enum dcam_path_index path_index);*/
LOCAL int32_t     _dcam_path_scaler(enum dcam_path_index path_index);
LOCAL int32_t     _dcam_calc_sc_size(enum dcam_path_index path_index);
LOCAL int32_t     _dcam_set_sc_coeff(enum dcam_path_index path_index);
LOCAL void        _dcam_force_copy_ext(enum dcam_path_index path_index, uint32_t path_copy, uint32_t coef_copy);
LOCAL void        _dcam_auto_copy_ext(enum dcam_path_index path_index, uint32_t path_copy, uint32_t coef_copy);
LOCAL void        _dcam_force_copy(enum dcam_path_index path_index);
LOCAL void        _dcam_auto_copy(enum dcam_path_index path_index);
LOCAL void        _dcam_reg_trace(void);
/*LOCAL void        _dcam_sensor_sof(void);*/
LOCAL void        _dcam_sensor_eof(void);
LOCAL void        _dcam_cap_sof(void);
LOCAL void        _dcam_cap_eof(void);
LOCAL void        _dcam_path0_done(void);
LOCAL void        _dcam_path0_overflow(void);
LOCAL void        _dcam_path1_done(void);
LOCAL void        _dcam_path1_overflow(void);
LOCAL void        _dcam_sensor_line_err(void);
LOCAL void        _dcam_sensor_frame_err(void);
LOCAL void        _dcam_jpeg_buf_ov(void);
LOCAL void        _dcam_path2_done(void);
LOCAL void        _dcam_path2_ov(void);
LOCAL void        _dcam_isp_ov(void);
LOCAL void        _dcam_mipi_ov(void);
LOCAL void        _dcam_path1_slice_done(void);
LOCAL void        _dcam_path2_slice_done(void);
LOCAL void        _dcam_raw_slice_done(void);
LOCAL irqreturn_t _dcam_isr_root(int irq, void *dev_id);
LOCAL void        _dcam_wait_for_stop(void);
LOCAL void        _dcam_stopped_notice(void);
LOCAL void        _dcam_stopped(void);
LOCAL int32_t     _dcam_path_check_deci(enum dcam_path_index path_index, uint32_t *is_deci);
extern void       _dcam_isp_root(void);
LOCAL int         _dcam_internal_init(void);
LOCAL void        _dcam_internal_deinit(void);
LOCAL void        _dcam_wait_path_done(enum dcam_path_index path_index, uint32_t *p_flag);
LOCAL void        _dcam_path_done_notice(enum dcam_path_index path_index);
LOCAL void        _dcam_rot_done(void);
LOCAL void        _dcam_err_pre_proc(void);
LOCAL void        _dcam_frm_queue_clear(struct dcam_frm_queue *queue);
LOCAL int32_t     _dcam_frame_enqueue(struct dcam_frm_queue *queue, struct dcam_frame *frame);
LOCAL int32_t     _dcam_frame_dequeue(struct dcam_frm_queue *queue, struct dcam_frame **frame);


LOCAL const dcam_isr isr_list[DCAM_IRQ_NUMBER] = {
	NULL,//_dcam_isp_root,
	_dcam_sensor_eof,
	_dcam_cap_sof,
	_dcam_cap_eof,
	_dcam_path0_done,
	_dcam_path0_overflow,
	_dcam_path1_done,
	_dcam_path1_overflow,
	_dcam_path2_done,
	_dcam_path2_ov,
	_dcam_sensor_line_err,
	_dcam_sensor_frame_err,
	_dcam_jpeg_buf_ov,
	_dcam_isp_ov,
	_dcam_mipi_ov,
	_dcam_rot_done,
	_dcam_path1_slice_done,
	_dcam_path2_slice_done,
	_dcam_raw_slice_done
};

void dcam_glb_reg_awr(uint32_t addr, uint32_t val, uint32_t reg_id)
{
	unsigned long flag;

	switch(reg_id) {
	case DCAM_CFG_REG:
		spin_lock_irqsave(&dcam_glb_reg_cfg_lock, flag);
		REG_WR(addr, REG_RD(addr) & (val));
		spin_unlock_irqrestore(&dcam_glb_reg_cfg_lock, flag);
		break;
	case DCAM_CONTROL_REG:
		spin_lock_irqsave(&dcam_glb_reg_control_lock, flag);
		REG_WR(addr, REG_RD(addr) & (val));
		spin_unlock_irqrestore(&dcam_glb_reg_control_lock, flag);
		break;
	case DCAM_INIT_MASK_REG:
		spin_lock_irqsave(&dcam_glb_reg_mask_lock, flag);
		REG_WR(addr, REG_RD(addr) & (val));
		spin_unlock_irqrestore(&dcam_glb_reg_mask_lock, flag);
		break;
	case DCAM_INIT_CLR_REG:
		spin_lock_irqsave(&dcam_glb_reg_clr_lock, flag);
		REG_WR(addr, REG_RD(addr) & (val));
		spin_unlock_irqrestore(&dcam_glb_reg_clr_lock, flag);
		break;
	case DCAM_AHBM_STS_REG:
		spin_lock_irqsave(&dcam_glb_reg_ahbm_sts_lock, flag);
		REG_WR(addr, REG_RD(addr) & (val));
		spin_unlock_irqrestore(&dcam_glb_reg_ahbm_sts_lock, flag);
		break;
	case DCAM_ENDIAN_REG:
		spin_lock_irqsave(&dcam_glb_reg_endian_lock, flag);
		REG_WR(addr, REG_RD(addr) & (val));
		spin_unlock_irqrestore(&dcam_glb_reg_endian_lock, flag);
		break;
	default:
		REG_WR(addr, REG_RD(addr) & (val));
		break;
	}
}

void dcam_glb_reg_owr(uint32_t addr, uint32_t val, uint32_t reg_id)
{
	unsigned long flag;

	switch(reg_id) {
	case DCAM_CFG_REG:
		spin_lock_irqsave(&dcam_glb_reg_cfg_lock, flag);
		REG_WR(addr, REG_RD(addr) | (val));
		spin_unlock_irqrestore(&dcam_glb_reg_cfg_lock, flag);
		break;
	case DCAM_CONTROL_REG:
		spin_lock_irqsave(&dcam_glb_reg_control_lock, flag);
		REG_WR(addr, REG_RD(addr) | (val));
		spin_unlock_irqrestore(&dcam_glb_reg_control_lock, flag);
		break;
	case DCAM_INIT_MASK_REG:
		spin_lock_irqsave(&dcam_glb_reg_mask_lock, flag);
		REG_WR(addr, REG_RD(addr) | (val));
		spin_unlock_irqrestore(&dcam_glb_reg_mask_lock, flag);
		break;
	case DCAM_INIT_CLR_REG:
		spin_lock_irqsave(&dcam_glb_reg_clr_lock, flag);
		REG_WR(addr, REG_RD(addr) | (val));
		spin_unlock_irqrestore(&dcam_glb_reg_clr_lock, flag);
		break;
	case DCAM_AHBM_STS_REG:
		spin_lock_irqsave(&dcam_glb_reg_ahbm_sts_lock, flag);
		REG_WR(addr, REG_RD(addr) | (val));
		spin_unlock_irqrestore(&dcam_glb_reg_ahbm_sts_lock, flag);
		break;
	case DCAM_ENDIAN_REG:
		spin_lock_irqsave(&dcam_glb_reg_endian_lock, flag);
		REG_WR(addr, REG_RD(addr) | (val));
		spin_unlock_irqrestore(&dcam_glb_reg_endian_lock, flag);
		break;
	default:
		REG_WR(addr, REG_RD(addr) | (val));
		break;
	}
}

void dcam_glb_reg_mwr(uint32_t addr, uint32_t mask, uint32_t val, uint32_t reg_id)
{
	unsigned long flag;
	uint32_t tmp = 0;

	switch(reg_id) {
	case DCAM_CFG_REG:
		spin_lock_irqsave(&dcam_glb_reg_cfg_lock, flag);
		{
			tmp = REG_RD(addr);
			tmp &= ~(mask);
			REG_WR(addr, tmp | ((mask) & (val)));
		}
		spin_unlock_irqrestore(&dcam_glb_reg_cfg_lock, flag);
		break;
	case DCAM_CONTROL_REG:
		spin_lock_irqsave(&dcam_glb_reg_control_lock, flag);
		{
			tmp = REG_RD(addr);
			tmp &= ~(mask);
			REG_WR(addr, tmp | ((mask) & (val)));
		}
		spin_unlock_irqrestore(&dcam_glb_reg_control_lock, flag);
		break;
	case DCAM_INIT_MASK_REG:
		spin_lock_irqsave(&dcam_glb_reg_mask_lock, flag);
		{
			tmp = REG_RD(addr);
			tmp &= ~(mask);
			REG_WR(addr, tmp | ((mask) & (val)));
		}
		spin_unlock_irqrestore(&dcam_glb_reg_mask_lock, flag);
		break;
	case DCAM_INIT_CLR_REG:
		spin_lock_irqsave(&dcam_glb_reg_clr_lock, flag);
		{
			tmp = REG_RD(addr);
			tmp &= ~(mask);
			REG_WR(addr, tmp | ((mask) & (val)));
		}
		spin_unlock_irqrestore(&dcam_glb_reg_clr_lock, flag);
		break;
	case DCAM_AHBM_STS_REG:
		spin_lock_irqsave(&dcam_glb_reg_ahbm_sts_lock, flag);
		{
			tmp = REG_RD(addr);
			tmp &= ~(mask);
			REG_WR(addr, tmp | ((mask) & (val)));
		}
		spin_unlock_irqrestore(&dcam_glb_reg_ahbm_sts_lock, flag);
		break;
	case DCAM_ENDIAN_REG:
		spin_lock_irqsave(&dcam_glb_reg_endian_lock, flag);
		{
			tmp = REG_RD(addr);
			tmp &= ~(mask);
			REG_WR(addr, tmp | ((mask) & (val)));
		}
		spin_unlock_irqrestore(&dcam_glb_reg_endian_lock, flag);
		break;
	default:
		{
			tmp = REG_RD(addr);
			tmp &= ~(mask);
			REG_WR(addr, tmp | ((mask) & (val)));
		}
		break;
	}
}

int32_t dcam_module_init(enum dcam_cap_if_mode if_mode,
	              enum dcam_cap_sensor_mode sn_mode)
{
	enum dcam_drv_rtn       rtn = DCAM_RTN_SUCCESS;
	struct dcam_cap_desc    *cap_desc = NULL;

	if (if_mode >= DCAM_CAP_IF_MODE_MAX) {
		rtn = -DCAM_RTN_CAP_IF_MODE_ERR;
	} else {
		if (sn_mode >= DCAM_CAP_MODE_MAX) {
			rtn = -DCAM_RTN_CAP_SENSOR_MODE_ERR;
		} else {
			_dcam_internal_init();
			_dcam_link_frm(0); /* set default base frame index as 0 */
			cap_desc = &s_p_dcam_mod->dcam_cap;
			cap_desc->interface = if_mode;
			cap_desc->input_format = sn_mode;
			/*REG_OWR(DCAM_EB, BIT_13);//MM_EB*/
			/*REG_OWR(DCAM_MATRIX_EB, BIT_10|BIT_5);*/
			if (DCAM_CAP_IF_CSI2 == if_mode) {
			/*	REG_OWR(CSI2_DPHY_EB, MIPI_EB_BIT);*/
				//ret = _dcam_mipi_clk_en();
				dcam_glb_reg_owr(DCAM_CFG, BIT_9, DCAM_CFG_REG);
				REG_MWR(CAP_MIPI_CTRL, BIT_2 | BIT_1, sn_mode << 1);
			} else {
				/*REG_OWR(DCAM_EB, CCIR_IN_EB_BIT);
				REG_OWR(DCAM_EB, CCIR_EB_BIT);*/
				//ret = _dcam_ccir_clk_en();
				dcam_glb_reg_mwr(DCAM_CFG, BIT_9, 0 << 9, DCAM_CFG_REG);
				REG_MWR(CAP_CCIR_CTRL, BIT_2 | BIT_1, sn_mode << 1);
			}
			rtn = DCAM_RTN_SUCCESS;
		}
	}

	return -rtn;
}

int32_t dcam_module_deinit(enum dcam_cap_if_mode if_mode,
	              enum dcam_cap_sensor_mode sn_mode)
{
	enum dcam_drv_rtn       rtn = DCAM_RTN_SUCCESS;

	if (DCAM_CAP_IF_CSI2 == if_mode) {
		/*REG_MWR(CSI2_DPHY_EB, MIPI_EB_BIT, 0 << 10);*/
		dcam_glb_reg_mwr(DCAM_CFG, BIT_9, 0 << 9, DCAM_CFG_REG);
		//_dcam_mipi_clk_dis();
	} else {
		/*REG_MWR(DCAM_EB, CCIR_IN_EB_BIT, 0 << 2);
		REG_MWR(DCAM_EB, CCIR_EB_BIT, 0 << 9);*/
		dcam_glb_reg_mwr(DCAM_CFG, BIT_9, 0 << 9, DCAM_CFG_REG);
		//_dcam_ccir_clk_dis();
	}

	_dcam_internal_deinit();

	return -rtn;
}

int dcam_scale_coeff_alloc(void)
{
	int ret = 0;

	if (NULL == s_dcam_scaling_coeff_addr) {
		s_dcam_scaling_coeff_addr = (uint32_t *)vzalloc(DCAM_SC_COEFF_BUF_SIZE);
		if (NULL == s_dcam_scaling_coeff_addr) {
			printk("DCAM: _dcam_scale_coeff_alloc fail.\n");
			ret = -1;
		}
	}
	return ret;
}

void dcam_scale_coeff_free(void)
{
	if (s_dcam_scaling_coeff_addr) {
		vfree(s_dcam_scaling_coeff_addr);
		s_dcam_scaling_coeff_addr = NULL;
	}
}

LOCAL uint32_t *dcam_get_scale_coeff_addr(void)
{
	return s_dcam_scaling_coeff_addr;
}

int32_t dcam_module_en(struct device_node *dn)
{
	int		ret = 0;
	unsigned int	irq_no = 0;

	DCAM_TRACE("DCAM: dcam_module_en, In %d \n", s_dcam_users.counter);

	mutex_lock(&dcam_module_sema);
	if (atomic_inc_return(&s_dcam_users) == 1) {

		wake_lock_init(&dcam_wakelock, WAKE_LOCK_SUSPEND,
			"pm_message_wakelock_dcam");

		wake_lock(&dcam_wakelock);

		ret = clk_mm_i_eb(dn,1);
		if (ret) {
			ret = -DCAM_RTN_MAX;
			goto fail_exit;
		}

		ret = dcam_set_clk(dn,DCAM_CLK_256M);
		if (ret) {
			clk_mm_i_eb(dn,0);
			ret = -DCAM_RTN_MAX;
			goto fail_exit;
		}

		parse_baseaddress(dn);

		dcam_reset(DCAM_RST_ALL, 0);
		atomic_set(&s_resize_flag, 0);
		atomic_set(&s_rotation_flag, 0);
		memset((void*)s_user_func, 0, sizeof(s_user_func));
		memset((void*)s_user_data, 0, sizeof(s_user_data));
		printk("DCAM: register isr, 0x%x \n", REG_RD(DCAM_INT_MASK));

		irq_no = parse_irq(dn);
		printk("DCAM: irq_no = 0x%x \n", irq_no);
		ret = request_irq(irq_no,
				_dcam_isr_root,
				IRQF_SHARED,
				"DCAM",
				(void*)&s_dcam_irq);
		if (ret) {
			printk("DCAM: dcam_start, error %d \n", ret);
			dcam_set_clk(dn,DCAM_CLK_NONE);
			clk_mm_i_eb(dn,0);
			ret = -DCAM_RTN_MAX;
			goto fail_exit;
		}

		DCAM_TRACE("DCAM: dcam_module_en end \n");
	}
	DCAM_TRACE("DCAM: dcam_module_en, Out %d \n", s_dcam_users.counter);
	mutex_unlock(&dcam_module_sema);
	return 0;

fail_exit:
	wake_unlock(&dcam_wakelock);
	wake_lock_destroy(&dcam_wakelock);
	atomic_dec(&s_dcam_users);
	mutex_unlock(&dcam_module_sema);
	return ret;
}

int32_t dcam_module_dis(struct device_node *dn)
{
	enum dcam_drv_rtn	rtn = DCAM_RTN_SUCCESS;
	int			ret = 0;
	unsigned int		irq_no = 0;

	DCAM_TRACE("DCAM: dcam_module_dis, In %d \n", s_dcam_users.counter);

	mutex_lock(&dcam_module_sema);
	if (atomic_dec_return(&s_dcam_users) == 0) {

		sci_glb_clr(DCAM_EB, DCAM_EB_BIT);
		dcam_set_clk(dn,DCAM_CLK_NONE);
		printk("DCAM: un register isr \n");
		irq_no = parse_irq(dn);
		free_irq(irq_no, (void*)&s_dcam_irq);
		ret = clk_mm_i_eb(dn,0);
		if (ret) {
			rtn = -DCAM_RTN_MAX;
		}
		wake_unlock(&dcam_wakelock);
		wake_lock_destroy(&dcam_wakelock);
	}

	DCAM_TRACE("DCAM: dcam_module_dis, Out %d \n", s_dcam_users.counter);
	mutex_unlock(&dcam_module_sema);

	return rtn;
}

int32_t dcam_reset(enum dcam_rst_mode reset_mode, uint32_t is_isr)
{
	enum dcam_drv_rtn       rtn = DCAM_RTN_SUCCESS;
	uint32_t                time_out = 0;

	DCAM_TRACE("DCAM: reset: %d \n", reset_mode);

	if (!is_isr) {
		mutex_lock(&dcam_scale_sema);
		mutex_lock(&dcam_rot_sema);
	}

	if (DCAM_RST_ALL == reset_mode) {
		if (atomic_read(&s_dcam_users)) {
			/* firstly, stop AXI writing */
			dcam_glb_reg_owr(DCAM_AHBM_STS, BIT_6, DCAM_AHBM_STS_REG);
		}

		/* then wait for AHB busy cleared */
		while (++time_out < DCAM_AXI_STOP_TIMEOUT) {
			if (0 == (REG_RD(DCAM_AHBM_STS) & BIT_0))
				break;
		}
		if (time_out >= DCAM_AXI_STOP_TIMEOUT)
			return DCAM_RTN_TIMEOUT;
	}

	/* do reset action */
	switch (reset_mode) {
	case DCAM_RST_PATH0:
		sci_glb_set(DCAM_RST, PATH0_RST_BIT);
		sci_glb_clr(DCAM_RST, PATH0_RST_BIT);
		DCAM_TRACE("DCAM: reset path0 \n");
		break;

	case DCAM_RST_PATH1:
		sci_glb_set(DCAM_RST, PATH1_RST_BIT);
		sci_glb_clr(DCAM_RST, PATH1_RST_BIT);
		DCAM_TRACE("DCAM: reset path1 \n");
		break;

	case DCAM_RST_PATH2:
		sci_glb_set(DCAM_RST, PATH2_RST_BIT);
		sci_glb_clr(DCAM_RST, PATH2_RST_BIT);
		DCAM_TRACE("DCAM: reset path2 \n");
		break;

	case DCAM_RST_ALL:
		sci_glb_set(DCAM_RST, DCAM_MOD_RST_BIT | CCIR_RST_BIT);
		sci_glb_clr(DCAM_RST, DCAM_MOD_RST_BIT | CCIR_RST_BIT);
		dcam_glb_reg_owr(DCAM_INT_CLR,
					DCAM_IRQ_LINE_MASK,
					DCAM_INIT_CLR_REG);
		dcam_glb_reg_owr(DCAM_INT_MASK,
						DCAM_IRQ_LINE_MASK,
						DCAM_INIT_MASK_REG);
		printk("DCAM: reset all \n");
		break;
	default:
		rtn = DCAM_RTN_PARA_ERR;
		break;
	}

	if (DCAM_RST_ALL == reset_mode) {
		if (atomic_read(&s_dcam_users)) {
			/* the end, enable AXI writing */
			dcam_glb_reg_awr(DCAM_AHBM_STS, ~BIT_6, DCAM_AHBM_STS_REG);
		}
	}

	if (!is_isr) {
		mutex_unlock(&dcam_rot_sema);
		mutex_unlock(&dcam_scale_sema);
	}

	DCAM_TRACE("DCAM: reset_mode=%x  end \n", reset_mode);

	return -rtn;
}

int32_t dcam_set_clk(struct device_node *dn, enum dcam_clk_sel clk_sel)
{
	enum dcam_drv_rtn       rtn = DCAM_RTN_SUCCESS;
	struct clk              *clk_parent;
	char                    *parent = "clk_256m";
	int                     ret = 0;

	switch (clk_sel) {
	case DCAM_CLK_256M:
		parent = "clk_256m";
		break;
	case DCAM_CLK_128M:
		parent = "clk_128m";
		break;
	case DCAM_CLK_48M:
		parent = "clk_48m";
		break;
	case DCAM_CLK_76M8:
		parent = "clk_76p8m";
		break;
	case DCAM_CLK_NONE:
		printk("DCAM close CLK %d \n", (int)clk_get_rate(s_dcam_clk));
		if (s_dcam_clk) {
			clk_disable(s_dcam_clk);
			clk_put(s_dcam_clk);
			s_dcam_clk = NULL;
		}
		return 0;
	default:
		parent = "clk_128m";
		break;
	}

	if (NULL == s_dcam_clk) {
		s_dcam_clk = parse_clk(dn,"clk_dcam");
		if (IS_ERR(s_dcam_clk)) {
			printk("DCAM: clk_get fail, %d \n", (int)s_dcam_clk);
			return -1;
		} else {
			DCAM_TRACE("DCAM: get clk_parent ok \n");
		}
	} else {
		clk_disable(s_dcam_clk);
	}

	clk_parent = clk_get(NULL, parent);
	if (IS_ERR(clk_parent)) {
		printk("DCAM: dcam_set_clk fail, %d \n", (int)clk_parent);
		return -1;
	} else {
		DCAM_TRACE("DCAM: get clk_parent ok \n");
	}

	ret = clk_set_parent(s_dcam_clk, clk_parent);
	if(ret){
		printk("DCAM: clk_set_parent fail, %d \n", ret);
	}

	ret = clk_enable(s_dcam_clk);
	if (ret) {
		printk("enable dcam clk error.\n");
		return -1;
	}
	return rtn;
}

int32_t dcam_update_path(enum dcam_path_index path_index, struct dcam_size *in_size,
		struct dcam_rect *in_rect, struct dcam_size *out_size)
{
	enum dcam_drv_rtn       rtn = DCAM_RTN_SUCCESS;
	unsigned long           flags;
	uint32_t                is_deci = 0;

	DCAM_CHECK_ZERO(s_p_dcam_mod);

	if ((DCAM_PATH_IDX_1 & path_index) && s_p_dcam_mod->dcam_path1.valide) {
		rtn = dcam_path1_cfg(DCAM_PATH_INPUT_SIZE, in_size);
		DCAM_RTN_IF_ERR;
		rtn = dcam_path1_cfg(DCAM_PATH_INPUT_RECT, in_rect);
		DCAM_RTN_IF_ERR;
		rtn = dcam_path1_cfg(DCAM_PATH_OUTPUT_SIZE, out_size);
		DCAM_RTN_IF_ERR;
		rtn = _dcam_path_check_deci(DCAM_PATH_IDX_1, &is_deci);
		DCAM_RTN_IF_ERR;
		if ((s_p_dcam_mod->dcam_path1.deci_val.deci_x_en != is_deci) || is_deci) {
			printk("DCAM: need restart \n");
			return 1;
/*
			DCAM_TRACE("DCAM: To restart path1 \n");
			DCAM_TRACE("DCAM: restart path1, deci cur=%d, will_set_to=%d \n",
				s_p_dcam_mod->dcam_path1.deci_val.deci_x_en, is_deci);
			dcam_stop_path(DCAM_PATH_IDX_1);
			s_p_dcam_mod->dcam_path1.output_frame_count = DCAM_PATH_1_FRM_CNT_MAX;
			s_p_dcam_mod->dcam_path1.valide = 1;
			dcam_start_path(DCAM_PATH_IDX_1);
*/
		} else {
			DCAM_TRACE("DCAM: To update path1 \n");
			rtn = _dcam_path_scaler(DCAM_PATH_IDX_1);
			DCAM_RTN_IF_ERR;
			DCAM_TRACE("DCAM: dcam_update_path 1 \n");
			_dcam_wait_path_done(DCAM_PATH_IDX_1, &s_p_dcam_mod->dcam_path1.is_update);
		}
		_dcam_wait_path_done(DCAM_PATH_IDX_1, NULL);
	}

	if ((DCAM_PATH_IDX_2 & path_index) && s_p_dcam_mod->dcam_path2.valide) {
		local_irq_save(flags);
		if(s_p_dcam_mod->dcam_path2.is_update){
			local_irq_restore(flags);
			DCAM_TRACE("DCAM: dcam_update_path 2:  updating return \n");
			return rtn;
		}
		local_irq_restore(flags);

		rtn = dcam_path2_cfg(DCAM_PATH_INPUT_SIZE, in_size);
		DCAM_RTN_IF_ERR;
		rtn = dcam_path2_cfg(DCAM_PATH_INPUT_RECT, in_rect);
		DCAM_RTN_IF_ERR;
		rtn = dcam_path2_cfg(DCAM_PATH_OUTPUT_SIZE, out_size);
		DCAM_RTN_IF_ERR;

		rtn = _dcam_path_scaler(DCAM_PATH_IDX_2);
		DCAM_RTN_IF_ERR;

		local_irq_save(flags);
		s_p_dcam_mod->dcam_path2.is_update = 1;
		local_irq_restore(flags);
	}

	DCAM_TRACE("DCAM: dcam_update_path: done \n");

	return -rtn;
}

int32_t dcam_start_path(enum dcam_path_index path_index)
{
	enum dcam_drv_rtn       rtn = DCAM_RTN_SUCCESS;
	uint32_t                cap_en = 0;
	uint32_t                is_deci = 0;

	DCAM_CHECK_ZERO(s_p_dcam_mod);

	DCAM_TRACE("DCAM: start_path: path %d, mode %x path 0,1,2 {%d %d %d} \n",
		path_index,
		s_p_dcam_mod->dcam_mode,
		s_p_dcam_mod->dcam_path0.valide,
		s_p_dcam_mod->dcam_path1.valide,
		s_p_dcam_mod->dcam_path2.valide);

	dcam_glb_reg_owr(DCAM_AHBM_STS, BIT_8, DCAM_AHBM_STS_REG); // aiden add: write arbit mode

	cap_en = REG_RD(DCAM_CONTROL) & BIT_2;
	DCAM_TRACE("DCAM: cap_eb %d \n", cap_en);
	if ((DCAM_PATH_IDX_0 & path_index) && s_p_dcam_mod->dcam_path0.valide) {
		_dcam_path0_set();
		rtn = _dcam_path_set_next_frm(DCAM_PATH_IDX_0, true);
		DCAM_RTN_IF_ERR;
		if (cap_en) {
			/* if cap is already open, the sequence is:
			   cap force copy -> path 0 enable -> cap auto copy */
			dcam_glb_reg_mwr(DCAM_CONTROL, BIT_0, 1 << 0, DCAM_CONTROL_REG); /* Cap force copy */
			dcam_glb_reg_owr(DCAM_CFG, BIT_0, DCAM_CFG_REG);             /* Enable Path 0 */
			dcam_glb_reg_mwr(DCAM_CONTROL, BIT_1, 1 << 1, DCAM_CONTROL_REG); /* Cap auto copy, trigger path 0 enable */
		} else {
			dcam_glb_reg_owr(DCAM_CFG, BIT_0, DCAM_CFG_REG);             /* Enable Path 0 */
		}
	}

	if ((DCAM_PATH_IDX_1 & path_index) && s_p_dcam_mod->dcam_path1.valide) {

		//rtn = _dcam_path_trim(DCAM_PATH_IDX_1);
		//DCAM_RTN_IF_ERR;
		rtn = _dcam_path_scaler(DCAM_PATH_IDX_1);
		DCAM_RTN_IF_ERR;

		_dcam_path1_set();
		DCAM_TRACE("DCAM: start path: path_control=%x \n", REG_RD(DCAM_CONTROL));

		rtn = _dcam_path_set_next_frm(DCAM_PATH_IDX_1, true);
		DCAM_RTN_IF_ERR;
		_dcam_force_copy_ext(DCAM_PATH_IDX_1, true, true);
		DCAM_TRACE("DCAM: int= %x \n", REG_RD(DCAM_INT_STS));
		dcam_glb_reg_owr(DCAM_CFG, BIT_1, DCAM_CFG_REG);
	}

	if ((DCAM_PATH_IDX_2 & path_index) && s_p_dcam_mod->dcam_path2.valide) {
		rtn = _dcam_path_check_deci(DCAM_PATH_IDX_2, &is_deci);
		DCAM_RTN_IF_ERR;
		if (0 == is_deci) {
			if (s_p_dcam_mod->dcam_path2.input_rect.h >= DCAM_HEIGHT_MIN) {
				s_p_dcam_mod->dcam_path2.input_rect.h--;
			}
		}

		rtn = _dcam_path_scaler(DCAM_PATH_IDX_2);
		DCAM_RTN_IF_ERR;

		_dcam_path2_set();

		rtn = _dcam_path_set_next_frm(DCAM_PATH_IDX_2, true);
		DCAM_RTN_IF_ERR;
		_dcam_force_copy_ext(DCAM_PATH_IDX_2, true, true);
		dcam_glb_reg_owr(DCAM_CFG, BIT_2, DCAM_CFG_REG);
	}

	_dcam_reg_trace();

	printk("DCAM PATH S: %d \n", path_index);

	if (0 == cap_en) {
#ifdef DCAM_DEBUG
		REG_MWR(CAP_CCIR_FRM_CTRL, BIT_5 | BIT_4, 1 << 4);
		REG_MWR(CAP_MIPI_FRM_CTRL, BIT_5 | BIT_4, 1 << 4);
#endif
		dcam_glb_reg_mwr(DCAM_CONTROL, BIT_0, 1 << 0, DCAM_CONTROL_REG); /* Cap force copy */
		//REG_MWR(DCAM_CONTROL, BIT_1, 1 << 1); /* Cap auto  copy */
		dcam_glb_reg_mwr(DCAM_CONTROL, BIT_2, 1 << 2, DCAM_CONTROL_REG); /* Cap Enable */
	}
	if ((DCAM_PATH_IDX_0 & path_index) && s_p_dcam_mod->dcam_path0.valide) {
		rtn = _dcam_path_set_next_frm(DCAM_PATH_IDX_0, false);
		DCAM_RTN_IF_ERR;
		_dcam_auto_copy(DCAM_PATH_IDX_0);
		s_p_dcam_mod->dcam_path0.status = DCAM_ST_START;
	}

	if ((DCAM_PATH_IDX_1 & path_index) && s_p_dcam_mod->dcam_path1.valide) {
		rtn = _dcam_path_set_next_frm(DCAM_PATH_IDX_1, false);
		DCAM_RTN_IF_ERR;
		_dcam_auto_copy(DCAM_PATH_IDX_1);
		s_p_dcam_mod->dcam_path1.status = DCAM_ST_START;
	}

	if ((DCAM_PATH_IDX_2 & path_index) && s_p_dcam_mod->dcam_path2.valide) {
		rtn = _dcam_path_set_next_frm(DCAM_PATH_IDX_2, false);
		DCAM_RTN_IF_ERR;
		_dcam_auto_copy(DCAM_PATH_IDX_2);
		s_p_dcam_mod->dcam_path2.status = DCAM_ST_START;
	}

	if (DCAM_PATH_IDX_ALL != path_index) {
		if ((DCAM_PATH_IDX_0 & path_index)) {
			_dcam_wait_path_done(DCAM_PATH_IDX_0, NULL);
		} else if ((DCAM_PATH_IDX_1 & path_index)) {
			_dcam_wait_path_done(DCAM_PATH_IDX_1, NULL);
		} else if ((DCAM_PATH_IDX_2 & path_index)) {
			_dcam_wait_path_done(DCAM_PATH_IDX_2, NULL);
		}
	}

	DCAM_TRACE("DCAM: start_path E\n");
	return -rtn;
}

int32_t dcam_start(void)
{
	enum dcam_drv_rtn       rtn = DCAM_RTN_SUCCESS;
	int                     ret = 0;

	DCAM_CHECK_ZERO(s_p_dcam_mod);

	DCAM_TRACE("DCAM: dcam_start %x \n", s_p_dcam_mod->dcam_mode);

	ret = dcam_start_path(DCAM_PATH_IDX_ALL);
	//ret = dcam_start_path(DCAM_PATH_IDX_1);

	return -rtn;
}

int32_t dcam_stop_cap(void)
{
	enum dcam_drv_rtn       rtn = DCAM_RTN_SUCCESS;

	DCAM_CHECK_ZERO(s_p_dcam_mod);

	printk("DCAM: stop cap %d \n", s_p_dcam_mod->dcam_mode);
	dcam_glb_reg_mwr(DCAM_CONTROL, BIT_2, 0, DCAM_CONTROL_REG); /* Cap Enable */
	DCAM_TRACE("DCAM: stop cap, s_resize_flag %d \n", atomic_read(&s_resize_flag));
	if (atomic_read(&s_resize_flag)) {
		s_p_dcam_mod->wait_resize_done = 1;
		rtn = down_timeout(&s_p_dcam_mod->resize_done_sema, DCAM_PATH_TIMEOUT);
	}
	if (atomic_read(&s_rotation_flag)) {
		s_p_dcam_mod->wait_rotation_done = 1;
		rtn = down_timeout(&s_p_dcam_mod->rotation_done_sema, DCAM_PATH_TIMEOUT);
	}

	dcam_reset(DCAM_RST_ALL, 0);
	DCAM_TRACE("DCAM: stop cap, Out \n");

	return -rtn;
}

int32_t dcam_stop_path(enum dcam_path_index path_index)
{
	enum dcam_drv_rtn       rtn = DCAM_RTN_SUCCESS;

	DCAM_CHECK_ZERO(s_p_dcam_mod);

	printk("DCAM: stop_path 0x%x \n", path_index);

	if (path_index < DCAM_PATH_IDX_0 ||
		path_index >= DCAM_PATH_IDX_ALL) {
		printk("DCAM: error path_index \n");
		return -rtn;
	}

	if ((DCAM_PATH_IDX_0 & path_index) && s_p_dcam_mod->dcam_path0.valide) {
		_dcam_wait_path_done(DCAM_PATH_IDX_0, &s_p_dcam_mod->dcam_path0.need_stop);
		if (DCAM_CAPTURE_MODE_MULTIPLE == s_p_dcam_mod->dcam_mode) {
			_dcam_wait_for_stop();
			_dcam_wait_for_stop();
		}
		dcam_reset(DCAM_RST_PATH0, 0);
		_dcam_frm_clear(DCAM_PATH_IDX_0);
		s_p_dcam_mod->dcam_path0.status = DCAM_ST_STOP;
		s_p_dcam_mod->dcam_path0.valide = 0;
	}

	if ((DCAM_PATH_IDX_1 & path_index) && s_p_dcam_mod->dcam_path1.valide) {
		_dcam_wait_path_done(DCAM_PATH_IDX_1, &s_p_dcam_mod->dcam_path1.need_stop);
		if (DCAM_CAPTURE_MODE_MULTIPLE == s_p_dcam_mod->dcam_mode) {
			_dcam_wait_for_stop();
			_dcam_wait_for_stop();
		}
		dcam_reset(DCAM_RST_PATH1, 0);
		_dcam_frm_clear(DCAM_PATH_IDX_1);
		s_p_dcam_mod->dcam_path1.status = DCAM_ST_STOP;
		s_p_dcam_mod->dcam_path1.valide = 0;
	}

	if ((DCAM_PATH_IDX_2 & path_index) && s_p_dcam_mod->dcam_path2.valide) {
		DCAM_TRACE("DCAM: stop path2 In \n");
		_dcam_wait_path_done(DCAM_PATH_IDX_2, &s_p_dcam_mod->dcam_path2.need_stop);
		if (DCAM_CAPTURE_MODE_MULTIPLE == s_p_dcam_mod->dcam_mode) {
			_dcam_wait_for_stop();
			/*_dcam_wait_for_stop();*/
		}
		DCAM_TRACE("DCAM: stop path2 Out \n");
		dcam_reset(DCAM_RST_PATH2, 0);
		_dcam_frm_clear(DCAM_PATH_IDX_2);
		s_p_dcam_mod->dcam_path2.status = DCAM_ST_STOP;
		s_p_dcam_mod->dcam_path2.valide = 0;
	}

	DCAM_TRACE("DCAM dcam_stop_path E: %d \n", path_index);

	return -rtn;
}
int32_t dcam_stop(void)
{
	enum dcam_drv_rtn       rtn = DCAM_RTN_SUCCESS;

	DCAM_CHECK_ZERO(s_p_dcam_mod);

	printk("DCAM dcam_stop In \n");

	if (s_p_dcam_mod->dcam_path0.valide) {
		_dcam_wait_path_done(DCAM_PATH_IDX_0, &s_p_dcam_mod->dcam_path0.need_stop);
	} else if (s_p_dcam_mod->dcam_path1.valide) {
		_dcam_wait_path_done(DCAM_PATH_IDX_1, &s_p_dcam_mod->dcam_path1.need_stop);
	} else if (s_p_dcam_mod->dcam_path2.valide) {
		_dcam_wait_path_done(DCAM_PATH_IDX_2, &s_p_dcam_mod->dcam_path2.need_stop);
	} else {
		printk("DCAM: No path valide");
	}
	if (DCAM_CAPTURE_MODE_MULTIPLE == s_p_dcam_mod->dcam_mode) {
		_dcam_wait_for_stop();
	}

	_dcam_frm_clear(DCAM_PATH_IDX_0);
	s_p_dcam_mod->dcam_path0.status = DCAM_ST_STOP;
	s_p_dcam_mod->dcam_path0.valide = 0;

	_dcam_frm_clear(DCAM_PATH_IDX_1);
	s_p_dcam_mod->dcam_path1.status = DCAM_ST_STOP;
	s_p_dcam_mod->dcam_path1.valide = 0;

	_dcam_frm_clear(DCAM_PATH_IDX_2);
	s_p_dcam_mod->dcam_path2.status = DCAM_ST_STOP;
	s_p_dcam_mod->dcam_path2.valide = 0;

	DCAM_TRACE("DCAM dcam_stop Out \n");

	return -rtn;
}

int32_t dcam_resume(void)
{
	enum dcam_drv_rtn       rtn = DCAM_RTN_SUCCESS;

	DCAM_CHECK_ZERO(s_p_dcam_mod);

	if (s_p_dcam_mod->dcam_path1.valide) {
		rtn = _dcam_path_set_next_frm(DCAM_PATH_IDX_1, true);
		DCAM_RTN_IF_ERR;
		_dcam_force_copy(DCAM_PATH_IDX_1);
		_dcam_frm_clear(DCAM_PATH_IDX_1);
	}

	if (s_p_dcam_mod->dcam_path2.valide) {
		rtn = _dcam_path_set_next_frm(DCAM_PATH_IDX_2, true);
		DCAM_RTN_IF_ERR;
		_dcam_force_copy(DCAM_PATH_IDX_2);
		_dcam_frm_clear(DCAM_PATH_IDX_2);
	}


	if (s_p_dcam_mod->dcam_path1.valide) {
		rtn = _dcam_path_set_next_frm(DCAM_PATH_IDX_1, false);
		DCAM_RTN_IF_ERR;
		dcam_glb_reg_owr(DCAM_CFG, BIT_1, DCAM_CFG_REG);
		_dcam_auto_copy_ext(DCAM_PATH_IDX_1, true, true);
	}

	if (s_p_dcam_mod->dcam_path2.valide) {
		rtn = _dcam_path_set_next_frm(DCAM_PATH_IDX_2, false);
		DCAM_RTN_IF_ERR;
		dcam_glb_reg_owr(DCAM_CFG, BIT_2, DCAM_CFG_REG);
		_dcam_auto_copy_ext(DCAM_PATH_IDX_2, true, true);
	}

	printk("DCAM R \n");

	dcam_glb_reg_owr(DCAM_CONTROL, BIT_2, DCAM_CONTROL_REG); /* Enable CAP */
	return -rtn;
}

int32_t dcam_pause(void)
{
	enum dcam_drv_rtn       rtn = DCAM_RTN_SUCCESS;

	dcam_glb_reg_awr(DCAM_CONTROL, ~BIT_2, DCAM_CONTROL_REG); /* Disable CAP */
	printk("DCAM P \n");
	return -rtn;
}

int32_t dcam_reg_isr(enum dcam_irq_id id, dcam_isr_func user_func, void* user_data)
{
	enum dcam_drv_rtn       rtn = DCAM_RTN_SUCCESS;
	unsigned long           flag;

	if(id >= DCAM_IRQ_NUMBER) {
		rtn = DCAM_RTN_ISR_ID_ERR;
	} else {
		spin_lock_irqsave(&dcam_lock, flag);
		s_user_func[id] = user_func;
		s_user_data[id] = user_data;
		spin_unlock_irqrestore(&dcam_lock, flag);
	}
	return -rtn;
}

int32_t dcam_cap_cfg(enum dcam_cfg_id id, void *param)
{
	enum dcam_drv_rtn       rtn = DCAM_RTN_SUCCESS;
	struct dcam_cap_desc    *cap_desc = NULL;

	DCAM_CHECK_ZERO(s_p_dcam_mod);
	cap_desc = &s_p_dcam_mod->dcam_cap;
	switch (id) {
	case DCAM_CAP_SYNC_POL:
	{
		struct dcam_cap_sync_pol *sync_pol = (struct dcam_cap_sync_pol*)param;

		DCAM_CHECK_PARAM_ZERO_POINTER(param);

		if (DCAM_CAP_IF_CCIR == cap_desc->interface) {

			if (sync_pol->vsync_pol > 1 ||
			    sync_pol->hsync_pol > 1 ||
			    sync_pol->pclk_pol > 1) {
				rtn = DCAM_RTN_CAP_SYNC_POL_ERR;
			} else {
				sci_glb_set(DCAM_CCIR_PCLK_EB, CCIR_PCLK_EB_BIT);
				REG_MWR(CAP_CCIR_CTRL, BIT_3, sync_pol->hsync_pol << 3);
				REG_MWR(CAP_CCIR_CTRL, BIT_4, sync_pol->vsync_pol << 4);
				//REG_MWR(CLK_DLY_CTRL,  BIT_19, sync_pol->pclk_pol << 19); // aiden todo

				if (sync_pol->pclk_src == 0x00) {
					sci_glb_clr(CAP_CCIR_PLCK_SRC, (BIT_25 | BIT_26));
					DCAM_TRACE("DCAM: set pclk src0\n");
				} else if (sync_pol->pclk_src == 0x01) {
					sci_glb_clr(CAP_CCIR_PLCK_SRC, (BIT_25 | BIT_26));
					sci_glb_set(CAP_CCIR_PLCK_SRC, (BIT_25));
					DCAM_TRACE("DCAM: set pclk src1\n");
				} else {
					DCAM_TRACE("DCAM: set pclk src default\n");
				}
			}
		} else {
			if (sync_pol->need_href) {
				REG_MWR(CAP_MIPI_CTRL, BIT_5, 1 << 5);
			} else {
				REG_MWR(CAP_MIPI_CTRL, BIT_5, 0 << 5);
			}
		}
		break;
	}

	case DCAM_CAP_DATA_BITS:
	{
		enum dcam_cap_data_bits bits = *(enum dcam_cap_data_bits*)param;

		DCAM_CHECK_PARAM_ZERO_POINTER(param);

		if (DCAM_CAP_IF_CCIR == cap_desc->interface) {
			if (DCAM_CAP_8_BITS == bits || DCAM_CAP_10_BITS == bits) {
				REG_MWR(CAP_CCIR_CTRL,  BIT_10 | BIT_9, 0 << 9);
			} else if (DCAM_CAP_4_BITS == bits) {
				REG_MWR(CAP_CCIR_CTRL,  BIT_10 | BIT_9, 1 << 9);
			} else if (DCAM_CAP_2_BITS == bits) {
				REG_MWR(CAP_CCIR_CTRL,  BIT_10 | BIT_9, 2 << 9);
			} else if (DCAM_CAP_1_BITS == bits) {
				REG_MWR(CAP_CCIR_CTRL,  BIT_10 | BIT_9, 3 << 9);
			} else {
				rtn = DCAM_RTN_CAP_IN_BITS_ERR;
			}

		} else {
			if (DCAM_CAP_12_BITS == bits) {
				REG_MWR(CAP_MIPI_CTRL,  BIT_4 | BIT_3, 2 << 3);
			} else if (DCAM_CAP_10_BITS == bits) {
				REG_MWR(CAP_MIPI_CTRL,  BIT_4 | BIT_3, 1 << 3);
			} else if (DCAM_CAP_8_BITS == bits) {
				REG_MWR(CAP_MIPI_CTRL,  BIT_4 | BIT_3, 0 << 3);
			} else {
				rtn = DCAM_RTN_CAP_IN_BITS_ERR;
			}
		}
		break;
	}

	case DCAM_CAP_YUV_TYPE:
	{
		enum dcam_cap_pattern pat = *(enum dcam_cap_pattern*)param;

		DCAM_CHECK_PARAM_ZERO_POINTER(param);

		if (pat < DCAM_PATTERN_MAX) {
			if (DCAM_CAP_IF_CCIR == cap_desc->interface)
				REG_MWR(CAP_CCIR_CTRL, BIT_8 | BIT_7, pat << 7);
			else
				REG_MWR(CAP_MIPI_CTRL, BIT_8 | BIT_7, pat << 7);
		} else {
			rtn = DCAM_RTN_CAP_IN_YUV_ERR;
		}
		break;
	}

	case DCAM_CAP_PRE_SKIP_CNT:
	{
		uint32_t skip_num = *(uint32_t*)param;

		DCAM_CHECK_PARAM_ZERO_POINTER(param);

		if (skip_num > DCAM_CAP_SKIP_FRM_MAX) {
			rtn = DCAM_RTN_CAP_SKIP_FRAME_ERR;
		} else {
			if (DCAM_CAP_IF_CCIR == cap_desc->interface)
				REG_MWR(CAP_CCIR_FRM_CTRL, BIT_3 | BIT_2 | BIT_1 | BIT_0, skip_num);
			else
				REG_MWR(CAP_MIPI_FRM_CTRL, BIT_3 | BIT_2 | BIT_1 | BIT_0, skip_num);
		}
		break;
	}

	case DCAM_CAP_FRM_DECI:
	{
		uint32_t deci_factor = *(uint32_t*)param;

		DCAM_CHECK_PARAM_ZERO_POINTER(param);

		if (deci_factor < DCAM_FRM_DECI_FAC_MAX) {
			if (DCAM_CAP_IF_CCIR == cap_desc->interface)
				REG_MWR(CAP_CCIR_FRM_CTRL, BIT_5 | BIT_4, deci_factor << 4);
			else
				REG_MWR(CAP_MIPI_FRM_CTRL, BIT_5 | BIT_4, deci_factor << 4);
		} else {
			rtn = DCAM_RTN_CAP_FRAME_DECI_ERR;
		}
		break;
	}

	case DCAM_CAP_FRM_COUNT_CLR:
		if (DCAM_CAP_IF_CCIR == cap_desc->interface)
			REG_MWR(CAP_CCIR_FRM_CTRL, BIT_22, 1 << 22);
		else
			REG_MWR(CAP_MIPI_FRM_CTRL, BIT_22, 1 << 22);
		break;

	case DCAM_CAP_INPUT_RECT:
	{
		struct dcam_rect *rect = (struct dcam_rect*)param;
		uint32_t         tmp = 0;

		DCAM_CHECK_PARAM_ZERO_POINTER(param);
#if 0
		if (rect->x > DCAM_CAP_FRAME_WIDTH_MAX ||
		rect->y > DCAM_CAP_FRAME_HEIGHT_MAX ||
		rect->w > DCAM_CAP_FRAME_WIDTH_MAX ||
		rect->h > DCAM_CAP_FRAME_HEIGHT_MAX ) {
			rtn = DCAM_RTN_CAP_FRAME_SIZE_ERR;
			return -rtn;
		}
#endif

		if (DCAM_CAP_IF_CCIR == cap_desc->interface) {
			if (DCAM_CAP_MODE_RAWRGB == cap_desc->input_format) {
				tmp = rect->x | (rect->y << 16);
				REG_WR(CAP_CCIR_START, tmp);
				tmp = (rect->x + rect->w - 1);
				tmp |= (rect->y + rect->h - 1) << 16;
				REG_WR(CAP_CCIR_END, tmp);
			} else {
				tmp = (rect->x << 1) | (rect->y << 16);
				REG_WR(CAP_CCIR_START, tmp);
				tmp = ((rect->x + rect->w) << 1) - 1;
				tmp |= (rect->y + rect->h - 1) << 16;
				REG_WR(CAP_CCIR_END, tmp);
			}
		} else {
			tmp = rect->x | (rect->y << 16);
			REG_WR(CAP_MIPI_START, tmp);
			tmp = (rect->x + rect->w - 1);
			tmp |= (rect->y + rect->h - 1) << 16;
			REG_WR(CAP_MIPI_END, tmp);
		}
		break;
	}

	case DCAM_CAP_IMAGE_XY_DECI:
	{
		struct dcam_cap_dec *cap_dec = (struct dcam_cap_dec*)param;

		DCAM_CHECK_PARAM_ZERO_POINTER(param);

		if (cap_dec->x_factor > DCAM_CAP_X_DECI_FAC_MAX ||
		cap_dec->y_factor > DCAM_CAP_Y_DECI_FAC_MAX ) {
			rtn = DCAM_RTN_CAP_XY_DECI_ERR;
		} else {
			if (DCAM_CAP_MODE_RAWRGB == cap_desc->input_format) {
				if (cap_dec->x_factor > 1 ||
				    cap_dec->y_factor > 1) {
					rtn = DCAM_RTN_CAP_XY_DECI_ERR;
				}
			}
			if (DCAM_CAP_IF_CCIR == cap_desc->interface) {
				REG_MWR(CAP_CCIR_IMG_DECI, BIT_1 | BIT_0, cap_dec->x_factor);
				REG_MWR(CAP_CCIR_IMG_DECI, BIT_3 | BIT_2, cap_dec->y_factor << 2);
			} else {
				if (DCAM_CAP_MODE_RAWRGB == cap_desc->input_format) {
					// REG_MWR(CAP_MIPI_IMG_DECI, BIT_0, cap_dec->x_factor); // for camera path
					REG_MWR(CAP_MIPI_IMG_DECI, BIT_1, cap_dec->x_factor << 1);//for ISP
				} else {
					REG_MWR(CAP_MIPI_IMG_DECI, BIT_1 | BIT_0, cap_dec->x_factor);
					REG_MWR(CAP_MIPI_IMG_DECI, BIT_3 | BIT_2, cap_dec->y_factor << 2);
				}
			}
		}

		break;
	}

	case DCAM_CAP_JPEG_SET_BUF_LEN:
	{
		uint32_t jpg_buf_size = *(uint32_t*)param;

		DCAM_CHECK_PARAM_ZERO_POINTER(param);
		jpg_buf_size = jpg_buf_size/DCAM_JPG_BUF_UNIT;
		if (jpg_buf_size >= DCAM_JPG_UNITS) {
			rtn = DCAM_RTN_CAP_JPEG_BUF_LEN_ERR;
		} else {
			if (DCAM_CAP_IF_CCIR == cap_desc->interface)
				REG_WR(CAP_CCIR_JPG_CTRL,jpg_buf_size);
			else
				REG_WR(CAP_MIPI_JPG_CTRL,jpg_buf_size);
		}
		break;
	}

	case DCAM_CAP_TO_ISP:
	{
		uint32_t need_isp = *(uint32_t*)param;

		DCAM_CHECK_PARAM_ZERO_POINTER(param);

		if (need_isp) {
			dcam_glb_reg_mwr(DCAM_CFG, BIT_7, 1 << 7, DCAM_CFG_REG);
		} else {
			dcam_glb_reg_mwr(DCAM_CFG, BIT_7, 0 << 7, DCAM_CFG_REG);
		}
		break;
	}

	case DCAM_CAP_DATA_PACKET:
	{
		uint32_t is_loose = *(uint32_t*)param;

		DCAM_CHECK_PARAM_ZERO_POINTER(param);

		if (DCAM_CAP_IF_CSI2 == cap_desc->interface &&
			DCAM_CAP_MODE_RAWRGB == cap_desc->input_format) {
			if (is_loose) {
				REG_MWR(CAP_MIPI_CTRL, BIT_0, 1);
			} else {
				REG_MWR(CAP_MIPI_CTRL, BIT_0, 0);
			}
		} else {
			rtn = DCAM_RTN_MODE_ERR;
		}

		break;
	}

	case DCAM_CAP_SAMPLE_MODE:
	{
		enum dcam_capture_mode samp_mode = *(enum dcam_capture_mode*)param;

		DCAM_CHECK_PARAM_ZERO_POINTER(param);

		if (samp_mode >= DCAM_CAPTURE_MODE_MAX) {
			rtn = DCAM_RTN_MODE_ERR;
		} else {
			if (DCAM_CAP_IF_CSI2 == cap_desc->interface) {
				REG_MWR(CAP_MIPI_CTRL, BIT_6, samp_mode << 6);
			} else {
				REG_MWR(CAP_CCIR_CTRL, BIT_6, samp_mode << 6);
			}
			s_p_dcam_mod->dcam_mode = samp_mode;
		}
		break;
	}
	default:
		rtn = DCAM_RTN_IO_ID_ERR;
		break;

	}

	return -rtn;
}

int32_t dcam_cap_get_info(enum dcam_cfg_id id, void *param)
{
	enum dcam_drv_rtn       rtn = DCAM_RTN_SUCCESS;
	struct dcam_cap_desc    *cap_desc = NULL;

	DCAM_CHECK_ZERO(s_p_dcam_mod);

	cap_desc = &s_p_dcam_mod->dcam_cap;
	DCAM_CHECK_PARAM_ZERO_POINTER(param);

	switch(id) {
		case DCAM_CAP_FRM_COUNT_GET:
			if (DCAM_CAP_IF_CSI2 == cap_desc->interface) {
				*(uint32_t*)param = REG_RD(CAP_MIPI_FRM_CTRL) >> 16;
			} else {
				*(uint32_t*)param = REG_RD(CAP_CCIR_FRM_CTRL) >> 16;
			}
			break;

		case DCAM_CAP_JPEG_GET_LENGTH:
			if (DCAM_CAP_IF_CSI2 == cap_desc->interface) {
				*(uint32_t*)param = REG_RD(CAP_MIPI_FRM_SIZE);
			} else {
				*(uint32_t*)param = REG_RD(CAP_CCIR_FRM_SIZE);
			}
			break;
		default:
			rtn = DCAM_RTN_IO_ID_ERR;
			break;
	}
	return -rtn;
}

int32_t dcam_path0_cfg(enum dcam_cfg_id id, void *param)
{
	enum dcam_drv_rtn       rtn = DCAM_RTN_SUCCESS;
	struct dcam_path_desc   *path = NULL;

	DCAM_CHECK_ZERO(s_p_dcam_mod);
	path = &s_p_dcam_mod->dcam_path0;
	switch (id) {

	case DCAM_PATH_INPUT_SIZE:
	{
		struct dcam_size *size = (struct dcam_size*)param;

		DCAM_CHECK_PARAM_ZERO_POINTER(param);

		DCAM_TRACE("DCAM: DCAM_PATH0_INPUT_SIZE {%d %d} \n", size->w, size->h);
#if 0
		if (size->w > DCAM_PATH_FRAME_WIDTH_MAX ||
		size->h > DCAM_PATH_FRAME_HEIGHT_MAX) {
			rtn = DCAM_RTN_PATH_SRC_SIZE_ERR;
		} else
#endif
		 {
			path->input_size.w = size->w;
			path->input_size.h = size->h;
			path->valid_param.input_size = 1;
		}
		break;
	}

	case DCAM_PATH_OUTPUT_FORMAT:
	{
		enum dcam_output_mode format = *(enum dcam_output_mode*)param;

		DCAM_CHECK_PARAM_ZERO_POINTER(param);

		if((DCAM_OUTPUT_WORD != format) &&
		(DCAM_OUTPUT_HALF_WORD != format)){
			rtn = DCAM_RTN_PATH_OUT_FMT_ERR;
			path->output_format = DCAM_FTM_MAX;
		}else{
			path->output_format = format;
			path->valid_param.output_format = 1;
		}
		break;
	}

	case DCAM_PATH_FRAME_BASE_ID:
	{
		struct dcam_frame *frame  = path->output_frame_head;
		uint32_t          base_id = *(uint32_t*)param, i;

		DCAM_CHECK_PARAM_ZERO_POINTER(param);

		DCAM_TRACE("DCAM: DCAM_PATH_FRAME_BASE_ID 0x%x \n", base_id);
		for (i = 0; i < DCAM_PATH_0_FRM_CNT_MAX; i++) {
			frame->fid = base_id + i;
			frame = frame->next;
		}
		break;
	}

	case DCAM_PATH_FRAME_TYPE:
	{
		struct dcam_frame *frame  = path->output_frame_head;
		uint32_t          frm_type = *(uint32_t*)param, i;

		DCAM_CHECK_PARAM_ZERO_POINTER(param);

		DCAM_TRACE("DCAM: DCAM_PATH_FRAME_TYPE 0x%x \n", frm_type);
		for (i = 0; i < DCAM_PATH_0_FRM_CNT_MAX; i++) {
			frame->type = frm_type;
			frame = frame->next;
		}
		break;
	}

	case DCAM_PATH_OUTPUT_ADDR:
	{
		struct dcam_addr *p_addr = (struct dcam_addr*)param;

		DCAM_CHECK_PARAM_ZERO_POINTER(param);

		if (DCAM_YUV_ADDR_INVALID(p_addr->yaddr, p_addr->uaddr, p_addr->vaddr)) {
			rtn = DCAM_RTN_PATH_ADDR_ERR;
		} else {
			if (path->output_frame_count > DCAM_PATH_0_FRM_CNT_MAX - 1) {
				rtn = DCAM_RTN_PATH_FRAME_TOO_MANY;
			} else {
				path->output_frame_cur->yaddr = p_addr->yaddr;
				path->output_frame_cur->uaddr = p_addr->uaddr;
				path->output_frame_cur->vaddr = p_addr->vaddr;
				path->output_frame_cur = path->output_frame_cur->next;
				DCAM_TRACE("DCAM: Path 0 DCAM_PATH_OUTPUT_ADDR, i=%d, y=0x%x, u=0x%x, v=0x%x \n",
					path->output_frame_count, p_addr->yaddr, p_addr->uaddr, p_addr->vaddr);

				path->output_frame_count ++;
			}
		}
		break;
	}

	case DCAM_PATH_FRM_DECI:
	{
		uint32_t deci_factor = *(uint32_t*)param;

		DCAM_CHECK_PARAM_ZERO_POINTER(param);

		if (deci_factor >= DCAM_FRM_DECI_FAC_MAX) {
			rtn = DCAM_RTN_PATH_FRM_DECI_ERR;
		} else {
			path->frame_deci = deci_factor;
			path->valid_param.frame_deci = 1;
		}
		break;
	}

	case DCAM_PATH_DATA_ENDIAN:
	{
		struct dcam_endian_sel *endian = (struct dcam_endian_sel*)param;

		DCAM_CHECK_PARAM_ZERO_POINTER(param);

		if (endian->y_endian >= DCAM_ENDIAN_MAX ||
			endian->uv_endian >= DCAM_ENDIAN_MAX) {
			rtn = DCAM_RTN_PATH_ENDIAN_ERR;
		} else {
			path->data_endian.y_endian	= endian->y_endian;
			path->valid_param.data_endian = 1;
		}
		break;
	}


	case DCAM_PATH_ENABLE:
	{
		DCAM_CHECK_PARAM_ZERO_POINTER(param);
		path->valide = *(uint32_t*)param;
		break;
	}

	default:
		rtn = DCAM_RTN_IO_ID_ERR;
		break;
	}

	return -rtn;
}

int32_t dcam_path1_cfg(enum dcam_cfg_id id, void *param)
{
	enum dcam_drv_rtn       rtn = DCAM_RTN_SUCCESS;
	struct dcam_path_desc   *path = NULL;

	DCAM_CHECK_ZERO(s_p_dcam_mod);
	path = &s_p_dcam_mod->dcam_path1;
	switch (id) {

	case DCAM_PATH_INPUT_SIZE:
	{
		struct dcam_size *size = (struct dcam_size*)param;

		DCAM_CHECK_PARAM_ZERO_POINTER(param);

		DCAM_TRACE("DCAM: DCAM_PATH1_INPUT_SIZE {%d %d} \n", size->w, size->h);
		if (size->w > DCAM_PATH_FRAME_WIDTH_MAX ||
		size->h > DCAM_PATH_FRAME_HEIGHT_MAX) {
			rtn = DCAM_RTN_PATH_SRC_SIZE_ERR;
		} else {
			path->input_size.w = size->w;
			path->input_size.h = size->h;
			path->valid_param.input_size = 1;
		}
		break;
	}

	case DCAM_PATH_INPUT_RECT:
	{
		struct dcam_rect *rect = (struct dcam_rect*)param;

		DCAM_CHECK_PARAM_ZERO_POINTER(param);

		DCAM_TRACE("DCAM: DCAM_PATH1_INPUT_RECT  {%d %d %d %d} \n",
			rect->x,
			rect->y,
			rect->w,
			rect->h);

		if (rect->x > DCAM_PATH_FRAME_WIDTH_MAX ||
		rect->y > DCAM_PATH_FRAME_HEIGHT_MAX ||
		rect->w > DCAM_PATH_FRAME_WIDTH_MAX ||
		rect->h > DCAM_PATH_FRAME_HEIGHT_MAX) {
			rtn = DCAM_RTN_PATH_TRIM_SIZE_ERR;
		} else {
			memcpy((void*)&path->input_rect,
				(void*)rect,
				sizeof(struct dcam_rect));
			//if (path->input_rect.h >= DCAM_HEIGHT_MIN) {
			//	path->input_rect.h--;
			//}
			path->valid_param.input_rect = 1;
		}
		break;
	}

	case DCAM_PATH_OUTPUT_SIZE:
	{
		struct dcam_size *size = (struct dcam_size*)param;

		DCAM_CHECK_PARAM_ZERO_POINTER(param);

		DCAM_TRACE("DCAM: DCAM_PATH1_OUTPUT_SIZE {%d %d} \n", size->w, size->h);
		if (size->w > DCAM_PATH_FRAME_WIDTH_MAX ||
		size->h > DCAM_PATH_FRAME_HEIGHT_MAX) {
			rtn = DCAM_RTN_PATH_SRC_SIZE_ERR;
		} else {
			path->output_size.w = size->w;
			path->output_size.h = size->h;
			path->valid_param.output_size = 1;
		}
		break;
	}

	case DCAM_PATH_OUTPUT_FORMAT:
	{
		enum dcam_fmt format = *(enum dcam_fmt*)param;

		DCAM_CHECK_PARAM_ZERO_POINTER(param);

		path->output_format = format;

		if((DCAM_YUV422 != format) &&
		(DCAM_YUV420 != format) &&
		(DCAM_YUV420_3FRAME != format)){
			rtn = DCAM_RTN_PATH_OUT_FMT_ERR;
			path->output_format = DCAM_FTM_MAX;
		}else{
			path->valid_param.output_format = 1;
		}
		break;
	}

	case DCAM_PATH_OUTPUT_ADDR:
	{
		struct dcam_addr *p_addr = (struct dcam_addr*)param;

		DCAM_CHECK_PARAM_ZERO_POINTER(param);

		if (DCAM_YUV_ADDR_INVALID(p_addr->yaddr, p_addr->uaddr, p_addr->vaddr)) {
			rtn = DCAM_RTN_PATH_ADDR_ERR;
		} else {
			if (path->output_frame_count > DCAM_PATH_1_FRM_CNT_MAX - 1) {
				rtn = DCAM_RTN_PATH_FRAME_TOO_MANY;
			} else {
				path->output_frame_cur->yaddr = p_addr->yaddr;
				path->output_frame_cur->uaddr = p_addr->uaddr;
				path->output_frame_cur->vaddr = p_addr->vaddr;
				path->output_frame_cur = path->output_frame_cur->next;
				DCAM_TRACE("DCAM: Path 1 DCAM_PATH_OUTPUT_ADDR, i=%d, y=0x%x, u=0x%x, v=0x%x \n",
					path->output_frame_count, p_addr->yaddr, p_addr->uaddr, p_addr->vaddr);

				path->output_frame_count ++;
			}
		}
		break;
	}

	case DCAM_PATH_SRC_SEL:
	{
		uint32_t       src_sel = *(uint32_t*)param;

		DCAM_CHECK_PARAM_ZERO_POINTER(param);

		if (src_sel >= DCAM_PATH_FROM_NONE) {
			rtn = DCAM_RTN_PATH_SRC_ERR;
		} else {
			path->src_sel = src_sel;
			path->valid_param.src_sel = 1;
		}
		break;
	}

	case DCAM_PATH_FRAME_BASE_ID:
	{
		struct dcam_frame *frame  = path->output_frame_head;
		uint32_t          base_id = *(uint32_t*)param, i;

		DCAM_CHECK_PARAM_ZERO_POINTER(param);

		DCAM_TRACE("DCAM: DCAM_PATH_FRAME_BASE_ID 0x%x \n", base_id);
		for (i = 0; i < DCAM_PATH_1_FRM_CNT_MAX; i++) {
			frame->fid = base_id + i;
			frame = frame->next;
		}
		break;
	}

	case DCAM_PATH_DATA_ENDIAN:
	{
		struct dcam_endian_sel *endian = (struct dcam_endian_sel*)param;

		DCAM_CHECK_PARAM_ZERO_POINTER(param);

		if (endian->y_endian >= DCAM_ENDIAN_MAX ||
			endian->uv_endian >= DCAM_ENDIAN_MAX) {
			rtn = DCAM_RTN_PATH_ENDIAN_ERR;
		} else {
			path->data_endian.y_endian  = endian->y_endian;
			path->data_endian.uv_endian = endian->uv_endian;
			path->valid_param.data_endian = 1;
		}
		break;
	}

	case DCAM_PATH_ENABLE:
	{
		DCAM_CHECK_PARAM_ZERO_POINTER(param);
		path->valide = *(uint32_t*)param;
		break;
	}

	case DCAM_PATH_FRAME_TYPE:
	{
		struct dcam_frame *frame  = path->output_frame_head;
		uint32_t          frm_type = *(uint32_t*)param, i;

		DCAM_CHECK_PARAM_ZERO_POINTER(param);

		DCAM_TRACE("DCAM: DCAM_PATH_FRAME_TYPE 0x%x \n", frm_type);
		for (i = 0; i < DCAM_PATH_1_FRM_CNT_MAX; i++) {
			frame->type = frm_type;
			frame = frame->next;
		}
		break;
	}

	case DCAM_PATH_FRM_DECI:
	{
		uint32_t deci_factor = *(uint32_t*)param;

		DCAM_CHECK_PARAM_ZERO_POINTER(param);

		if (deci_factor >= DCAM_FRM_DECI_FAC_MAX) {
			rtn = DCAM_RTN_PATH_FRM_DECI_ERR;
		} else {
			path->frame_deci = deci_factor;
			path->valid_param.frame_deci = 1;
		}
		break;
	}

	default:
		break;
	}

	return -rtn;
}

int32_t dcam_path2_cfg(enum dcam_cfg_id id, void *param)
{
	enum dcam_drv_rtn       rtn = DCAM_RTN_SUCCESS;
	struct dcam_path_desc   *path = NULL;

	DCAM_CHECK_ZERO(s_p_dcam_mod);
	path = &s_p_dcam_mod->dcam_path2;
	switch (id) {

	case DCAM_PATH_INPUT_SIZE:
	{
		struct dcam_size *size = (struct dcam_size*)param;

		DCAM_CHECK_PARAM_ZERO_POINTER(param);

		DCAM_TRACE("DCAM: DCAM_PATH2_INPUT_SIZE {%d %d} \n", size->w, size->h);
		if (size->w > DCAM_PATH_FRAME_WIDTH_MAX ||
		size->h > DCAM_PATH_FRAME_HEIGHT_MAX) {
			rtn = DCAM_RTN_PATH_SRC_SIZE_ERR;
		} else {
			path->input_size.w = size->w;
			path->input_size.h = size->h;
			path->valid_param.input_size = 1;

		}
		break;
	}

	case DCAM_PATH_INPUT_RECT:
	{
		struct dcam_rect *rect = (struct dcam_rect*)param;

		DCAM_CHECK_PARAM_ZERO_POINTER(param);

		DCAM_TRACE("DCAM: DCAM_PATH2_INPUT_RECT {%d %d %d %d} \n",
			rect->x,
			rect->y,
			rect->w,
			rect->h);

		if (rect->x > DCAM_PATH_FRAME_WIDTH_MAX ||
		rect->y > DCAM_PATH_FRAME_HEIGHT_MAX ||
		rect->w > DCAM_PATH_FRAME_WIDTH_MAX ||
		rect->h > DCAM_PATH_FRAME_HEIGHT_MAX) {
			rtn = DCAM_RTN_PATH_TRIM_SIZE_ERR;
		} else {
			memcpy((void*)&path->input_rect,
				(void*)rect,
				sizeof(struct dcam_rect));
#if 0
			if (path->input_rect.h >= DCAM_HEIGHT_MIN) {
				path->input_rect.h--;
			}
#endif
			path->valid_param.input_rect = 1;
		}
		break;
	}

	case DCAM_PATH_OUTPUT_SIZE:
	{
		struct dcam_size *size = (struct dcam_size*)param;

		DCAM_CHECK_PARAM_ZERO_POINTER(param);

		DCAM_TRACE("DCAM: DCAM_PATH2_OUTPUT_SIZE {%d %d} \n", size->w, size->h);
		if (size->w > DCAM_PATH_FRAME_WIDTH_MAX ||
		size->h > DCAM_PATH_FRAME_HEIGHT_MAX) {
			rtn = DCAM_RTN_PATH_SRC_SIZE_ERR;
		} else {
			path->output_size.w = size->w;
			path->output_size.h = size->h;
			path->valid_param.output_size = 1;
		}
		break;
	}

	case DCAM_PATH_OUTPUT_FORMAT:
	{
		enum dcam_fmt format = *(enum dcam_fmt*)param;

		DCAM_CHECK_PARAM_ZERO_POINTER(param);

		if((DCAM_YUV422 != format) &&
		(DCAM_YUV420 != format) &&
		(DCAM_YUV420_3FRAME != format)){
			rtn = DCAM_RTN_PATH_OUT_FMT_ERR;
			path->output_format = DCAM_FTM_MAX;
		}else{
			path->output_format = format;
			path->valid_param.output_format = 1;
		}
		break;
	}

	case DCAM_PATH_OUTPUT_ADDR:
	{
		struct dcam_addr *p_addr = (struct dcam_addr*)param;

		DCAM_CHECK_PARAM_ZERO_POINTER(param);

		if (DCAM_YUV_ADDR_INVALID(p_addr->yaddr, p_addr->uaddr, p_addr->vaddr)) {
			rtn = DCAM_RTN_PATH_ADDR_ERR;
		} else {
			if (path->output_frame_count > DCAM_PATH_2_FRM_CNT_MAX - 1) {
				DCAM_TRACE("DCAM: Path 2 Error, DCAM_PATH_OUTPUT_ADDR, i=%d, y=0x%x, u=0x%x, v=0x%x \n",
					path->output_frame_count, p_addr->yaddr, p_addr->uaddr, p_addr->vaddr);
				rtn = DCAM_RTN_PATH_FRAME_TOO_MANY;
			} else {
				path->output_frame_cur->yaddr = p_addr->yaddr;
				path->output_frame_cur->uaddr = p_addr->uaddr;
				path->output_frame_cur->vaddr = p_addr->vaddr;
				path->output_frame_cur = path->output_frame_cur->next;
				DCAM_TRACE("DCAM: Path 2 DCAM_PATH_OUTPUT_ADDR, i=%d, y u 0x%x 0x%x \n",
					path->output_frame_count,
					p_addr->yaddr,
					p_addr->uaddr);

				path->output_frame_count ++;
			}
		}
		break;
	}
	case DCAM_PATH_SRC_SEL:
	{
		uint32_t       src_sel = *(uint32_t*)param;

		DCAM_CHECK_PARAM_ZERO_POINTER(param);

		if (src_sel >= DCAM_PATH_FROM_NONE) {
			rtn = DCAM_RTN_PATH_SRC_ERR;
		} else {
			path->src_sel = src_sel;
			path->valid_param.src_sel = 1;

		}
		break;
	}

	case DCAM_PATH_FRAME_BASE_ID:
	{
		struct dcam_frame *frame  = path->output_frame_head;
		uint32_t          base_id = *(uint32_t*)param, i;

		DCAM_CHECK_PARAM_ZERO_POINTER(param);

		DCAM_TRACE("DCAM: DCAM_PATH_FRAME_BASE_ID 0x%x \n", base_id);
		for (i = 0; i < DCAM_PATH_2_FRM_CNT_MAX; i++) {
			frame->fid = base_id + i;
			frame = frame->next;
		}
		break;
	}

	case DCAM_PATH_DATA_ENDIAN:
	{
		struct dcam_endian_sel *endian = (struct dcam_endian_sel*)param;

		DCAM_CHECK_PARAM_ZERO_POINTER(param);

		if (endian->y_endian >= DCAM_ENDIAN_MAX ||
			endian->uv_endian >= DCAM_ENDIAN_MAX) {
			rtn = DCAM_RTN_PATH_ENDIAN_ERR;
		} else {
			path->data_endian.y_endian  = endian->y_endian;
			path->data_endian.uv_endian = endian->uv_endian;
			path->valid_param.data_endian = 1;
		}
		break;
	}

	case DCAM_PATH_ENABLE:
	{
		DCAM_CHECK_PARAM_ZERO_POINTER(param);

		path->valide = *(uint32_t*)param;

		break;
	}

	case DCAM_PATH_FRAME_TYPE:
	{
		struct dcam_frame *frame  = path->output_frame_head;
		uint32_t          frm_type = *(uint32_t*)param, i;

		DCAM_CHECK_PARAM_ZERO_POINTER(param);

		DCAM_TRACE("DCAM: DCAM_PATH_FRAME_TYPE 0x%x \n", frm_type);
		for (i = 0; i < DCAM_PATH_2_FRM_CNT_MAX; i++) {
			frame->type = frm_type;
			frame = frame->next;
		}
		break;
	}

	case DCAM_PATH_FRM_DECI:
	{
		uint32_t deci_factor = *(uint32_t*)param;

		DCAM_CHECK_PARAM_ZERO_POINTER(param);

		if (deci_factor >= DCAM_FRM_DECI_FAC_MAX) {
			rtn = DCAM_RTN_PATH_FRM_DECI_ERR;
		} else {
			path->frame_deci = deci_factor;
			path->valid_param.frame_deci = 1;
		}
		break;
	}

	default:
		break;
	}

	return -rtn;
}

int32_t    dcam_get_resizer(uint32_t wait_opt)
{
	int32_t                 rtn = 0;

	printk("resizer_get:%d\n",current->pid);
	if( 0 == wait_opt) {
		rtn = mutex_trylock(&dcam_sem) ? 0 : 1;
		return rtn;
	} else if (DCAM_WAIT_FOREVER == wait_opt){
		mutex_lock(&dcam_sem);
		return rtn;
	} else {
		return 0;//mutex_timeout(&dcam_sem, wait_opt);
	}
}

int32_t    dcam_rel_resizer(void)
{
	printk("resizer_put:%d\n",current->pid);
	mutex_unlock(&dcam_sem);
	return 0;
}

int32_t    dcam_resize_start(void)
{
	atomic_inc(&s_resize_flag);
	mutex_lock(&dcam_scale_sema);
	return 0;
}

int32_t    dcam_resize_end(void)
{
	unsigned long flag;

	atomic_dec(&s_resize_flag);
	mutex_unlock(&dcam_scale_sema);

	spin_lock_irqsave(&dcam_mod_lock, flag);
	if (DCAM_ADDR_INVALID(s_p_dcam_mod)) {
		spin_unlock_irqrestore(&dcam_mod_lock, flag);
		return 0;
	}

	if (s_p_dcam_mod) {
		if (s_p_dcam_mod->wait_resize_done) {
			up(&s_p_dcam_mod->resize_done_sema);
			s_p_dcam_mod->wait_resize_done = 0;
		}
	}
	spin_unlock_irqrestore(&dcam_mod_lock, flag);
	return 0;
}

int32_t    dcam_rotation_start(void)
{
	atomic_inc(&s_rotation_flag);
	mutex_lock(&dcam_rot_sema);
	return 0;
}

int32_t    dcam_rotation_end(void)
{
	unsigned long flag;

	atomic_dec(&s_rotation_flag);
	mutex_unlock(&dcam_rot_sema);

	spin_lock_irqsave(&dcam_mod_lock, flag);
	if (DCAM_ADDR_INVALID(s_p_dcam_mod)) {
		spin_unlock_irqrestore(&dcam_mod_lock, flag);
		return 0;
	}

	if (s_p_dcam_mod) {
		if (s_p_dcam_mod->wait_rotation_done) {
			up(&s_p_dcam_mod->rotation_done_sema);
			s_p_dcam_mod->wait_rotation_done = 0;
		}
	}
	spin_unlock_irqrestore(&dcam_mod_lock, flag);
	return 0;
}

void dcam_int_en(void)
{
	if (atomic_read(&s_dcam_users) == 1) {
		enable_irq(DCAM_IRQ);
	}
}

void dcam_int_dis(void)
{
	if (atomic_read(&s_dcam_users) == 1) {
		disable_irq(DCAM_IRQ);
	}
}

int32_t dcam_frame_is_locked(struct dcam_frame *frame)
{
	uint32_t                rtn = 0;
	unsigned long           flags;

	/*To disable irq*/
	local_irq_save(flags);
	if (frame)
		rtn = frame->lock == DCAM_FRM_LOCK_WRITE ? 1 : 0;
	local_irq_restore(flags);
	/*To enable irq*/

	return -rtn;
}

int32_t dcam_frame_lock(struct dcam_frame *frame)
{
	uint32_t                rtn = 0;
	unsigned long           flags;

	DCAM_CHECK_ZERO(s_p_dcam_mod);

	DCAM_TRACE("DCAM: lock %d \n", (uint32_t)(0xF&frame->fid));

	/*To disable irq*/
	local_irq_save(flags);
	if (likely(frame))
		frame->lock = DCAM_FRM_LOCK_WRITE;
	else
		rtn = DCAM_RTN_PARA_ERR;
	local_irq_restore(flags);
	/*To enable irq*/

	return -rtn;
}

int32_t dcam_frame_unlock(struct dcam_frame *frame)
{
	uint32_t                rtn = 0;
	unsigned long           flags;

	DCAM_CHECK_ZERO(s_p_dcam_mod);

	DCAM_TRACE("DCAM: unlock %d \n", (uint32_t)(0xF&frame->fid));

	/*To disable irq*/
	local_irq_save(flags);
	if (likely(frame))
		frame->lock = DCAM_FRM_UNLOCK;
	else
		rtn = DCAM_RTN_PARA_ERR;
	local_irq_restore(flags);
	/*To enable irq*/

	return -rtn;
}

int32_t    dcam_read_registers(uint32_t* reg_buf, uint32_t *buf_len)
{
	uint32_t                *reg_addr = (uint32_t*)DCAM_BASE;

	if (NULL == reg_buf || NULL == buf_len || 0 != (*buf_len % 4)) {
		return -1;
	}

	while (buf_len != 0 && (uint32_t)reg_addr < DCAM_END) {
		*reg_buf++ = REG_RD(reg_addr);
		reg_addr++;
		*buf_len -= 4;
	}

	*buf_len = (uint32_t)reg_addr - DCAM_BASE;
	return 0;
}

LOCAL irqreturn_t _dcam_isr_root(int irq, void *dev_id)
{
	uint32_t                irq_line, status;
	unsigned long           flag;
	int32_t                 i;

	DCAM_TRACE("DCAM: ISR 0x%x \n", REG_RD(DCAM_INT_STS));
	status = REG_RD(DCAM_INT_STS) & DCAM_IRQ_LINE_MASK;
	if (unlikely(0 == status)) {
		return IRQ_NONE;
	}

	irq_line = status;
	if (unlikely(DCAM_IRQ_ERR_MASK & status)) {
		_dcam_err_pre_proc();
	}

	spin_lock_irqsave(&dcam_lock,flag);

	//for (i = DCAM_IRQ_NUMBER - 1; i >= 0; i--) {
	for (i = 0; i < DCAM_IRQ_NUMBER; i++) {
		if (irq_line & (1 << (uint32_t)i)) {
			if (isr_list[i]) {
				isr_list[i]();
			}
		}
		irq_line &= ~(uint32_t)(1 << (uint32_t)i); //clear the interrupt flag
		if(!irq_line) //no interrupt source left
			break;
	}

	REG_WR(DCAM_INT_CLR, status);
	DCAM_TRACE("DCAM: status 0x%x, ISR 0x%x \n", status, REG_RD(DCAM_INT_STS));

	spin_unlock_irqrestore(&dcam_lock, flag);

	return IRQ_HANDLED;
}

LOCAL void _dcam_path0_set(void)
{
	uint32_t                reg_val = 0;
	struct dcam_path_desc   *path = NULL;

	DCAM_CHECK_ZERO_VOID(s_p_dcam_mod);
	path = &s_p_dcam_mod->dcam_path0;
	if (path->valid_param.input_size) {
		reg_val = path->input_size.w | (path->input_size.h << 16);
		REG_WR(DCAM_PATH0_SRC_SIZE, reg_val);
	}

	if (path->valid_param.output_format) {
		enum dcam_output_mode format = path->output_format;
		REG_MWR(DCAM_PATH0_CFG, BIT_2, format << 2);
		DCAM_TRACE("DCAM: path 0: output_format=0x%x \n", format);
	}

	if (path->valid_param.frame_deci) {
		REG_MWR(DCAM_PATH0_CFG, BIT_1 | BIT_0, path->frame_deci << 0);
	}

	if (path->valid_param.data_endian) {
		dcam_glb_reg_mwr(DCAM_ENDIAN_SEL, BIT_5 | BIT_4, path->data_endian.y_endian << 4, DCAM_ENDIAN_REG);
		dcam_glb_reg_mwr(DCAM_ENDIAN_SEL, BIT_18, BIT_18, DCAM_ENDIAN_REG); // axi write
		dcam_glb_reg_mwr(DCAM_ENDIAN_SEL, BIT_19, BIT_19, DCAM_ENDIAN_REG); // axi read
		DCAM_TRACE("DCAM DRV: path 0: data_endian y=0x%x, uv=0x%x \n",
			path->data_endian.y_endian, path->data_endian.uv_endian);
	}

}
LOCAL void _dcam_path1_set(void)
{
	struct dcam_path_desc   *path = NULL;
	uint32_t                reg_val = 0;

	DCAM_CHECK_ZERO_VOID(s_p_dcam_mod);
	path = &s_p_dcam_mod->dcam_path1;
	if (path->valid_param.input_size) {
		reg_val = path->input_size.w | (path->input_size.h << 16);
		REG_WR(DCAM_PATH1_SRC_SIZE, reg_val);
		DCAM_TRACE("DCAM: path1 set: src {%d %d} \n",path->input_size.w, path->input_size.h);
	}

	if (path->valid_param.input_rect) {
		reg_val = path->input_rect.x | (path->input_rect.y << 16);
		REG_WR(DCAM_PATH1_TRIM_START, reg_val);
		reg_val = path->input_rect.w | (path->input_rect.h << 16);
		REG_WR(DCAM_PATH1_TRIM_SIZE, reg_val);
		DCAM_TRACE("DCAM: path1 set: rect {%d %d %d %d} \n",
			path->input_rect.x,
			path->input_rect.y,
			path->input_rect.w,
			path->input_rect.h);
	}

	if (path->valid_param.output_size) {
		reg_val = path->output_size.w | (path->output_size.h << 16);
		REG_WR(DCAM_PATH1_DST_SIZE, reg_val);
		DCAM_TRACE("DCAM: path1 set: dst {%d %d} \n",path->output_size.w, path->output_size.h);
	}

	if (path->valid_param.output_format) {
		enum dcam_fmt format = path->output_format;
		if (DCAM_YUV422 == format) {
			REG_MWR(DCAM_PATH1_CFG, BIT_7 | BIT_6, 0 << 6);
		} else if (DCAM_YUV420 == format) {
			REG_MWR(DCAM_PATH1_CFG, BIT_7 | BIT_6, 1 << 6);
		} else if (DCAM_YUV420_3FRAME == format) {
			REG_MWR(DCAM_PATH1_CFG, BIT_7 | BIT_6, 3 << 6);
		} else {
			DCAM_TRACE("DCAM: invalid path 1 output format %d \n", format);
		}
		DCAM_TRACE("DCAM: path 1: output_format=0x%x \n", format);
	}

	if (path->valid_param.src_sel) {
		REG_MWR(DCAM_CFG, BIT_12 | BIT_11, path->src_sel << 11);
		DCAM_TRACE("DCAM: path 1: src_sel=0x%x \n", path->src_sel);
	}

	if (path->valid_param.data_endian) {
		dcam_glb_reg_mwr(DCAM_ENDIAN_SEL, BIT_7 | BIT_6, path->data_endian.y_endian << 6, DCAM_ENDIAN_REG);
		dcam_glb_reg_mwr(DCAM_ENDIAN_SEL, BIT_9 | BIT_8, path->data_endian.uv_endian << 8, DCAM_ENDIAN_REG);
		dcam_glb_reg_mwr(DCAM_ENDIAN_SEL, BIT_18, BIT_18, DCAM_ENDIAN_REG); // axi write
		dcam_glb_reg_mwr(DCAM_ENDIAN_SEL, BIT_19, BIT_19, DCAM_ENDIAN_REG); // axi read
		DCAM_TRACE("DCAM: path 1: data_endian y=0x%x, uv=0x%x \n",
			path->data_endian.y_endian, path->data_endian.uv_endian);
	}

	if (path->valid_param.frame_deci) {
		REG_MWR(DCAM_PATH0_CFG, BIT_24 | BIT_23, path->frame_deci << 23);
		DCAM_TRACE("DCAM: path 1: frame_deci=0x%x \n", path->frame_deci);
	}

	if (path->valid_param.scale_tap) {
		path->valid_param.scale_tap = 0;
		REG_MWR(DCAM_PATH1_CFG, BIT_19 | BIT_18 | BIT_17 | BIT_16, (path->scale_tap.y_tap & 0x0F) << 16);
		REG_MWR(DCAM_PATH1_CFG, BIT_15 | BIT_14 | BIT_13 | BIT_12 | BIT_11, (path->scale_tap.uv_tap & 0x1F) << 11);
		DCAM_TRACE("DCAM: path 1: scale_tap, y=0x%x, uv=0x%x \n", path->scale_tap.y_tap, path->scale_tap.uv_tap);
	}

	if (path->valid_param.v_deci) {
		path->valid_param.v_deci = 0;
		REG_MWR(DCAM_PATH1_CFG, BIT_2, path->deci_val.deci_x_en << 2);
		REG_MWR(DCAM_PATH1_CFG, BIT_1 | BIT_0, path->deci_val.deci_x);

		REG_MWR(DCAM_PATH1_CFG, BIT_5, path->deci_val.deci_y_en << 5);
		REG_MWR(DCAM_PATH1_CFG, BIT_4 | BIT_3, path->deci_val.deci_y << 3);
		DCAM_TRACE("DCAM: path 1: deci, x_en=%d, x=%d, y_en=%d, y=%d \n",
			path->deci_val.deci_x_en, path->deci_val.deci_x, path->deci_val.deci_y_en, path->deci_val.deci_y);
	}
}

LOCAL void _dcam_path2_set(void)
{
	struct dcam_path_desc   *path = NULL;
	uint32_t                reg_val = 0;

	DCAM_CHECK_ZERO_VOID(s_p_dcam_mod);
	path = &s_p_dcam_mod->dcam_path2;
	if (path->valid_param.input_size) {
		reg_val = path->input_size.w | (path->input_size.h << 16);
		REG_WR(DCAM_PATH2_SRC_SIZE, reg_val);
		DCAM_TRACE("DCAM: path2 set: src {%d %d} \n",path->input_size.w, path->input_size.h);
	}

	if (path->valid_param.input_rect) {
		reg_val = path->input_rect.x | (path->input_rect.y << 16);
		REG_WR(DCAM_PATH2_TRIM_START, reg_val);
		reg_val = path->input_rect.w | (path->input_rect.h << 16);
		REG_WR(DCAM_PATH2_TRIM_SIZE, reg_val);
		DCAM_TRACE("DCAM: path2 set: rect {%d %d %d %d} \n",
			path->input_rect.x,
			path->input_rect.y,
			path->input_rect.w,
			path->input_rect.h);
	}

	if(path->valid_param.output_size){
		reg_val = path->output_size.w | (path->output_size.h << 16);
		REG_WR(DCAM_PATH2_DST_SIZE, reg_val);
		DCAM_TRACE("DCAM: path2 set: dst {%d %d} \n",path->output_size.w, path->output_size.h);
	}

	if (path->valid_param.output_format) {
		enum dcam_fmt format = path->output_format;

		// REG_MWR(DCAM_PATH2_CFG, BIT_8, 0 << 8); // aiden todo

		if (DCAM_YUV422 == format) {
			REG_MWR(DCAM_PATH2_CFG, BIT_7 | BIT_6, 0 << 6);
		} else if (DCAM_YUV420 == format) {
			REG_MWR(DCAM_PATH2_CFG, BIT_7 | BIT_6, 1 << 6);
		} else if (DCAM_YUV420_3FRAME == format) {
			REG_MWR(DCAM_PATH2_CFG, BIT_7 | BIT_6, 3 << 6);
		} else {
			DCAM_TRACE("DCAM: invalid path 2 output format %d \n", format);
		}
		DCAM_TRACE("DCAM: path 2: output_format=0x%x \n", format);
	}

	if (path->valid_param.src_sel) {
		REG_MWR(DCAM_CFG, BIT_14 | BIT_13, path->src_sel << 13);
	}

	if (path->valid_param.data_endian) {
		dcam_glb_reg_mwr(DCAM_ENDIAN_SEL, BIT_11 | BIT_10, path->data_endian.y_endian << 10,DCAM_ENDIAN_REG );
		dcam_glb_reg_mwr(DCAM_ENDIAN_SEL, BIT_13 | BIT_12, path->data_endian.uv_endian << 12, DCAM_ENDIAN_REG);
		dcam_glb_reg_mwr(DCAM_ENDIAN_SEL, BIT_18, BIT_18, DCAM_ENDIAN_REG); // axi write
		dcam_glb_reg_mwr(DCAM_ENDIAN_SEL, BIT_19, BIT_19, DCAM_ENDIAN_REG); // axi read
		DCAM_TRACE("DCAM: path 2: data_endian y=0x%x, uv=0x%x \n",
			path->data_endian.y_endian, path->data_endian.uv_endian);
	}

	if (path->valid_param.frame_deci) {
		REG_MWR(DCAM_PATH0_CFG, BIT_24 | BIT_23, path->frame_deci << 23);
		DCAM_TRACE("DCAM: path 2: frame_deci=0x%x \n", path->frame_deci);
	}

	if (path->valid_param.scale_tap) {
		path->valid_param.scale_tap = 0;
		REG_MWR(DCAM_PATH2_CFG, BIT_19 | BIT_18 | BIT_17 | BIT_16, (path->scale_tap.y_tap & 0x0F) << 16);
		REG_MWR(DCAM_PATH2_CFG, BIT_15 | BIT_14 | BIT_13 | BIT_12 | BIT_11, (path->scale_tap.uv_tap & 0x1F) << 11);
		DCAM_TRACE("DCAM: path 2: scale_tap, y=0x%x, uv=0x%x \n",
			path->scale_tap.y_tap, path->scale_tap.uv_tap);
	}

	if (path->valid_param.v_deci) {
		path->valid_param.v_deci = 0;
		REG_MWR(DCAM_PATH2_CFG, BIT_2, path->deci_val.deci_x_en << 2);
		REG_MWR(DCAM_PATH2_CFG, BIT_1 | BIT_0, path->deci_val.deci_x);

		REG_MWR(DCAM_PATH2_CFG, BIT_5, path->deci_val.deci_y_en << 5);
		REG_MWR(DCAM_PATH2_CFG, BIT_4 | BIT_3, path->deci_val.deci_y << 3);
		DCAM_TRACE("DCAM: path 2: deci, x_en=%d, x=%d, y_en=%d, y=%d \n",
			path->deci_val.deci_x_en, path->deci_val.deci_x, path->deci_val.deci_y_en, path->deci_val.deci_y);
	}
}

LOCAL void _dcam_frm_queue_clear(struct dcam_frm_queue *queue)
{
	if (DCAM_ADDR_INVALID(queue)) {
		printk("DCAM: invalid heap 0x%x \n", (uint32_t)queue);
		return;
	}

	memset((void*)queue, 0, sizeof(struct dcam_frm_queue));
	return;
}

LOCAL int32_t _dcam_frame_enqueue(struct dcam_frm_queue *queue, struct dcam_frame *frame)
{
	if (DCAM_ADDR_INVALID(queue) || DCAM_ADDR_INVALID(frame)) {
		printk("DCAM: enq, invalid param 0x%x, 0x%x \n",
			(uint32_t)queue,
			(uint32_t)frame);
		return -1;
	}
	if (queue->valid_cnt >= DCAM_FRM_QUEUE_LENGTH) {
		printk("DCAM: q over flow \n");
		return -1;
	}
	queue->frm_array[queue->valid_cnt] = frame;
	queue->valid_cnt ++;
	DCAM_TRACE("DCAM: en queue, %d, %d \n",
		(0xF & frame->fid),
		queue->valid_cnt);
	return 0;
}

LOCAL int32_t _dcam_frame_dequeue(struct dcam_frm_queue *queue, struct dcam_frame **frame)
{
	uint32_t                i = 0;

	if (DCAM_ADDR_INVALID(queue) || DCAM_ADDR_INVALID(frame)) {
		printk("DCAM: deq, invalid param 0x%x, 0x%x \n",
			(uint32_t)queue,
			(uint32_t)frame);
		return -1;
	}
	if (queue->valid_cnt == 0) {
		printk("DCAM: q under flow \n");
		return -1;
	}

	*frame  = queue->frm_array[0];
	queue->valid_cnt--;
	for (i = 0; i < queue->valid_cnt; i++) {
		queue->frm_array[i] = queue->frm_array[i+1];
	}
	DCAM_TRACE("DCAM: de queue, %d, %d \n",
		(0xF & (*frame)->fid),
		queue->valid_cnt);
	return 0;
}

LOCAL void _dcam_frm_clear(enum dcam_path_index path_index)
{
	uint32_t                i = 0;
	struct dcam_frame       *path0_frame = NULL;
	struct dcam_frame       *path1_frame = NULL;
	struct dcam_frame       *path2_frame = NULL;

	DCAM_CHECK_ZERO_VOID(s_p_dcam_mod);

	path0_frame = &s_p_dcam_mod->path0_frame[0];
	path1_frame = &s_p_dcam_mod->path1_frame[0];
	path2_frame = &s_p_dcam_mod->path2_frame[0];
	if (DCAM_PATH_IDX_0 & path_index) {
		for (i = 0; i < DCAM_PATH_0_FRM_CNT_MAX; i++) {
			(path0_frame+i)->lock = DCAM_FRM_UNLOCK;
		}
		s_p_dcam_mod->dcam_path0.output_frame_head  = path0_frame;
		s_p_dcam_mod->dcam_path0.output_frame_cur   = path0_frame;
		s_p_dcam_mod->dcam_path0.output_frame_count = 0;
		_dcam_frm_queue_clear(&s_p_dcam_mod->dcam_path0.frame_queue);
	}

	if (DCAM_PATH_IDX_1 & path_index) {
		for (i = 0; i < DCAM_PATH_1_FRM_CNT_MAX; i++) {
			(path1_frame+i)->lock = DCAM_FRM_UNLOCK;
		}
		s_p_dcam_mod->dcam_path1.output_frame_head  = path1_frame;
		s_p_dcam_mod->dcam_path1.output_frame_cur   = path1_frame;
		s_p_dcam_mod->dcam_path1.output_frame_count = 0;
		_dcam_frm_queue_clear(&s_p_dcam_mod->dcam_path1.frame_queue);
	}

	if (DCAM_PATH_IDX_2 & path_index) {
		for (i = 0; i < DCAM_PATH_2_FRM_CNT_MAX; i++) {
			(path2_frame+i)->lock = DCAM_FRM_UNLOCK;
		}
		s_p_dcam_mod->dcam_path2.output_frame_head  = path2_frame;
		s_p_dcam_mod->dcam_path2.output_frame_cur   = path2_frame;
		s_p_dcam_mod->dcam_path2.output_frame_count = 0;
		_dcam_frm_queue_clear(&s_p_dcam_mod->dcam_path2.frame_queue);

	}

	return;
}

LOCAL void _dcam_link_frm(uint32_t base_id)
{
	uint32_t                i = 0;
	struct dcam_frame       *path0_frame = NULL;
	struct dcam_frame       *path1_frame = NULL;
	struct dcam_frame       *path2_frame = NULL;

	DCAM_CHECK_ZERO_VOID(s_p_dcam_mod);

	path0_frame = &s_p_dcam_mod->path0_frame[0];
	path1_frame = &s_p_dcam_mod->path1_frame[0];
	path2_frame = &s_p_dcam_mod->path2_frame[0];
	for (i = 0; i < DCAM_PATH_0_FRM_CNT_MAX; i++) {
		DCAM_CLEAR(path0_frame + i);
		(path0_frame+i)->next = path0_frame + (i + 1) % DCAM_PATH_0_FRM_CNT_MAX;
		(path0_frame+i)->prev = path0_frame + (i - 1 + DCAM_PATH_0_FRM_CNT_MAX) % DCAM_PATH_0_FRM_CNT_MAX;
		(path0_frame+i)->fid  = base_id + i;
	}

	for (i = 0; i < DCAM_PATH_1_FRM_CNT_MAX; i++) {
		DCAM_CLEAR(path1_frame + i);
		(path1_frame+i)->next = path1_frame + (i + 1) % DCAM_PATH_1_FRM_CNT_MAX;
		(path1_frame+i)->prev = path1_frame + (i - 1 + DCAM_PATH_1_FRM_CNT_MAX) % DCAM_PATH_1_FRM_CNT_MAX;
		(path1_frame+i)->fid  = base_id + i;
	}

	for (i = 0; i < DCAM_PATH_2_FRM_CNT_MAX; i++) {
		DCAM_CLEAR(path2_frame + i);
		(path2_frame+i)->next = path2_frame+(i + 1) % DCAM_PATH_2_FRM_CNT_MAX;
		(path2_frame+i)->prev = path2_frame+(i - 1 + DCAM_PATH_2_FRM_CNT_MAX) % DCAM_PATH_2_FRM_CNT_MAX;
		(path2_frame+i)->fid  = base_id + i;
	}

	s_p_dcam_mod->dcam_path0.output_frame_head = path0_frame;
	s_p_dcam_mod->dcam_path1.output_frame_head = path1_frame;
	s_p_dcam_mod->dcam_path2.output_frame_head = path2_frame;
	s_p_dcam_mod->dcam_path0.output_frame_cur = path0_frame;
	s_p_dcam_mod->dcam_path1.output_frame_cur = path1_frame;
	s_p_dcam_mod->dcam_path2.output_frame_cur = path2_frame;

	return;
}

LOCAL int32_t _dcam_path_set_next_frm(enum dcam_path_index path_index, uint32_t is_1st_frm)
{
	enum dcam_drv_rtn       rtn = DCAM_RTN_SUCCESS;
	struct dcam_frame       *frame = NULL;
	struct dcam_frame       *orig_frame = NULL;
	struct dcam_path_desc   *path = NULL;
	uint32_t                yuv_reg[3] = {0};
	uint32_t                path_max_frm_cnt;
	uint32_t                flag = 0;
	struct dcam_frm_queue   *p_heap = NULL;

	DCAM_CHECK_ZERO(s_p_dcam_mod);

	if (DCAM_PATH_IDX_0 == path_index) {
		frame = &s_p_dcam_mod->path0_frame[0];
		path = &s_p_dcam_mod->dcam_path0;
		yuv_reg[0] = DCAM_FRM_ADDR0;
		path_max_frm_cnt = DCAM_PATH_0_FRM_CNT_MAX;
		p_heap = &s_p_dcam_mod->dcam_path0.frame_queue;
	} else if (DCAM_PATH_IDX_1 == path_index) {
		frame = &s_p_dcam_mod->path1_frame[0];
		path = &s_p_dcam_mod->dcam_path1;
		yuv_reg[0] = DCAM_FRM_ADDR1;
		yuv_reg[1] = DCAM_FRM_ADDR2;
		yuv_reg[2] = DCAM_FRM_ADDR3;
		path_max_frm_cnt = DCAM_PATH_1_FRM_CNT_MAX;
		p_heap = &s_p_dcam_mod->dcam_path1.frame_queue;
	} else /*if(DCAM_PATH_IDX_2 == path_index)*/ {
		frame = &s_p_dcam_mod->path2_frame[0];
		path = &s_p_dcam_mod->dcam_path2;
		yuv_reg[0] = DCAM_FRM_ADDR4;
		yuv_reg[1] = DCAM_FRM_ADDR5;
		yuv_reg[2] = DCAM_FRM_ADDR6;
		path_max_frm_cnt = DCAM_PATH_2_FRM_CNT_MAX;
		p_heap = &s_p_dcam_mod->dcam_path2.frame_queue;
	}

	if (is_1st_frm) {
		if (path->output_frame_count < path_max_frm_cnt) {
			frame = path->output_frame_cur->prev;
			frame->next = path->output_frame_head;
			path->output_frame_head->prev = frame;
			path->output_frame_cur = path->output_frame_head;
		}
	}

	frame      = path->output_frame_cur;
	orig_frame = path->output_frame_cur;
	do {
		if (0 == dcam_frame_is_locked(frame)) {
			flag = 1;
			break;
		}
		DCAM_TRACE("DCAM: search \n");
		frame = frame->next;
	} while (frame != orig_frame);

	if (flag) {
		REG_WR(yuv_reg[0], frame->yaddr);
		if ((DCAM_YUV400 > path->output_format) && (DCAM_PATH_IDX_0 != path_index)) {
			REG_WR(yuv_reg[1], frame->uaddr);
			if (DCAM_YUV420_3FRAME == path->output_format) {
				REG_WR(yuv_reg[2], frame->vaddr);
			}
		}
		if (0 == _dcam_frame_enqueue(p_heap, frame)) {
			dcam_frame_lock(frame);
			path->output_frame_cur = frame->next;
		} else {
			rtn = DCAM_RTN_PATH_FRAME_LOCKED;
		}
	} else {
		DCAM_TRACE("DCAM: No freed frame \n");
		rtn = DCAM_RTN_PATH_FRAME_LOCKED;
	}

	return -rtn;
}

#if 0
LOCAL int32_t _dcam_path_trim(enum dcam_path_index path_index)
{
	enum dcam_drv_rtn       rtn = DCAM_RTN_SUCCESS;
	struct dcam_path_desc   *path;
	uint32_t                cfg_reg, ctrl_bit;

	DCAM_CHECK_ZERO(s_p_dcam_mod);

	if (DCAM_PATH_IDX_1 == path_index) {
		path = &s_p_dcam_mod->dcam_path1;
		cfg_reg = DCAM_PATH1_CFG;
		ctrl_bit = 8;
	} else if (DCAM_PATH_IDX_2 == path_index) {
		path = &s_p_dcam_mod->dcam_path2;
		cfg_reg = DCAM_PATH2_CFG;
		ctrl_bit = 1;
	} else {
		printk("DCAM: _dcam_path_trim invalid path_index=%d \n", path_index);
		return -1;
	}

	if (path->input_size.w != path->input_rect.w ||
		path->input_size.h != path->input_rect.h) {
		/*REG_OWR(cfg_reg, 1 << ctrl_bit);*/
	} else {
		/*REG_MWR(cfg_reg, 1 << ctrl_bit, 0 << ctrl_bit);*/
	}

	return rtn;

}
#endif

LOCAL int32_t _dcam_path_scaler(enum dcam_path_index path_index)
{
	enum dcam_drv_rtn       rtn = DCAM_RTN_SUCCESS;
	struct dcam_path_desc   *path = NULL;
	uint32_t                cfg_reg = 0;

	DCAM_CHECK_ZERO(s_p_dcam_mod);

	if (DCAM_PATH_IDX_1 == path_index) {
		path = &s_p_dcam_mod->dcam_path1;
		cfg_reg = DCAM_PATH1_CFG;
	} else if (DCAM_PATH_IDX_2 == path_index){
		path = &s_p_dcam_mod->dcam_path2;
		cfg_reg = DCAM_PATH2_CFG;
	}

	DCAM_CHECK_ZERO(path);
	if (DCAM_RAWRGB == path->output_format ||
		DCAM_JPEG == path->output_format) {
		DCAM_TRACE("DCAM: _dcam_path_scaler out format is %d, no need scaler \n", path->output_format);
		return DCAM_RTN_SUCCESS;
	}

	rtn = _dcam_calc_sc_size(path_index);
	if (DCAM_RTN_SUCCESS != rtn) {
		return rtn;
	}

	if (path->sc_input_size.w == path->output_size.w &&
		path->sc_input_size.h == path->output_size.h &&
		DCAM_YUV422 == path->output_format) {
		REG_MWR(cfg_reg, BIT_20, 1 << 20);// bypass scaler if the output size equals input size for YUV422 format
	} else {
		REG_MWR(cfg_reg, BIT_20, 0 << 20);
		rtn = _dcam_set_sc_coeff(path_index);
	}

	return rtn;
}

LOCAL int32_t _dcam_path_check_deci(enum dcam_path_index path_index, uint32_t *is_deci)
{
	enum dcam_drv_rtn       rtn = DCAM_RTN_SUCCESS;
	struct dcam_path_desc   *path = NULL;
	uint32_t				w_deci = 0;
	uint32_t				h_deci = 0;

	DCAM_CHECK_ZERO(s_p_dcam_mod);

	if (DCAM_PATH_IDX_1 == path_index) {
		path = &s_p_dcam_mod->dcam_path1;
	} else if (DCAM_PATH_IDX_2 == path_index) {
		path = &s_p_dcam_mod->dcam_path2;
	} else {
		rtn = DCAM_RTN_PATH_SC_ERR;
		goto dcam_path_err;
	}

	if (path->input_rect.w > (path->output_size.w * DCAM_SC_COEFF_DOWN_MAX * (1<<DCAM_PATH_DECI_FAC_MAX)) ||
		path->input_rect.h > (path->output_size.h * DCAM_SC_COEFF_DOWN_MAX * (1<<DCAM_PATH_DECI_FAC_MAX)) ||
		path->input_rect.w * DCAM_SC_COEFF_UP_MAX < path->output_size.w ||
		path->input_rect.h * DCAM_SC_COEFF_UP_MAX < path->output_size.h) {
		rtn = DCAM_RTN_PATH_SC_ERR;
	} else {
		if (path->input_rect.w > path->output_size.w * DCAM_SC_COEFF_DOWN_MAX)
			w_deci = 1;
		if (path->input_rect.h > path->output_size.h * DCAM_SC_COEFF_DOWN_MAX)
			h_deci = 1;

		if(w_deci || h_deci)
			*is_deci = 1;
		else
			*is_deci = 0;
	}

dcam_path_err:
	DCAM_TRACE("DCAM: _dcam_path_check_deci: path_index=%d, is_deci=%d, rtn=%d \n", path_index, *is_deci, rtn);

	return rtn;
}

LOCAL uint32_t _dcam_get_path_deci_factor(uint32_t src_size, uint32_t dst_size)
{
	uint32_t                 factor = 0;

	if (0 == src_size || 0 == dst_size) {
		return factor;
	}

	/* factor: 0 - 1/2, 1 - 1/4, 2 - 1/8, 3 - 1/16 */
	for (factor = 0; factor < DCAM_PATH_DECI_FAC_MAX; factor ++) {
		if (src_size < (uint32_t)(dst_size * (1 << (factor + 1)))) {
			break;
		}
	}

	return factor;
}

LOCAL int32_t _dcam_calc_sc_size(enum dcam_path_index path_index)
{
	enum dcam_drv_rtn       rtn = DCAM_RTN_SUCCESS;
	struct dcam_path_desc   *path = NULL;
	uint32_t                cfg_reg = 0;
	uint32_t                tmp_dstsize = 0;
	uint32_t                align_size = 0;

	DCAM_CHECK_ZERO(s_p_dcam_mod);

	if (DCAM_PATH_IDX_1 == path_index) {
		path = &s_p_dcam_mod->dcam_path1;
		cfg_reg = DCAM_PATH1_CFG;
	} else if (DCAM_PATH_IDX_2 == path_index){
		path = &s_p_dcam_mod->dcam_path2;
		cfg_reg = DCAM_PATH2_CFG;
	}

	DCAM_CHECK_ZERO(path);
	if (path->input_rect.w > (path->output_size.w * DCAM_SC_COEFF_DOWN_MAX * (1<<DCAM_PATH_DECI_FAC_MAX)) ||
		path->input_rect.h > (path->output_size.h * DCAM_SC_COEFF_DOWN_MAX * (1<<DCAM_PATH_DECI_FAC_MAX)) ||
		path->input_rect.w * DCAM_SC_COEFF_UP_MAX < path->output_size.w ||
		path->input_rect.h * DCAM_SC_COEFF_UP_MAX < path->output_size.h) {
		rtn = DCAM_RTN_PATH_SC_ERR;
	} else {
		path->sc_input_size.w = path->input_rect.w;
		path->sc_input_size.h = path->input_rect.h;
		if (path->input_rect.w > path->output_size.w * DCAM_SC_COEFF_DOWN_MAX) {
			tmp_dstsize = path->output_size.w * DCAM_SC_COEFF_DOWN_MAX;
			path->deci_val.deci_x = _dcam_get_path_deci_factor(path->input_rect.w, tmp_dstsize);
			path->deci_val.deci_x_en = 1;
			path->valid_param.v_deci = 1;
			align_size = (1 << (path->deci_val.deci_x+1))*DCAM_PIXEL_ALIGN_WIDTH;
			path->input_rect.w = (path->input_rect.w) & ~(align_size-1);
			path->input_rect.x = (path->input_rect.x) & ~(align_size-1);
			path->sc_input_size.w = path->input_rect.w >> (path->deci_val.deci_x+1);
		} else {
			path->deci_val.deci_x = 0;
			path->deci_val.deci_x_en = 0;
			path->valid_param.v_deci = 1;
		}

		if (path->input_rect.h > path->output_size.h * DCAM_SC_COEFF_DOWN_MAX) {
			tmp_dstsize = path->output_size.h * DCAM_SC_COEFF_DOWN_MAX;
			path->deci_val.deci_y = _dcam_get_path_deci_factor(path->input_rect.h, tmp_dstsize);
			path->deci_val.deci_y_en = 1;
			path->valid_param.v_deci = 1;
			align_size = (1 << (path->deci_val.deci_y+1))*DCAM_PIXEL_ALIGN_HEIGHT;
			path->input_rect.h = (path->input_rect.h) & ~(align_size-1);
			path->input_rect.y = (path->input_rect.y) & ~(align_size-1);
			path->sc_input_size.h = path->input_rect.h >> (path->deci_val.deci_y+1);
		} else {
			path->deci_val.deci_y = 0;
			path->deci_val.deci_y_en = 0;
			path->valid_param.v_deci = 1;
		}

	}

	DCAM_TRACE("DCAM: _dcam_calc_sc_size, path=%d, x_en=%d, deci_x=%d, y_en=%d, deci_y=%d \n",
		path_index, path->deci_val.deci_x_en,
		path->deci_val.deci_x, path->deci_val.deci_y_en,
		path->deci_val.deci_y);

	return -rtn;
}

LOCAL int32_t _dcam_set_sc_coeff(enum dcam_path_index path_index)
{
	struct dcam_path_desc   *path = NULL;
	uint32_t                i = 0;
	uint32_t                h_coeff_addr = DCAM_BASE;
	uint32_t                v_coeff_addr  = DCAM_BASE;
	uint32_t                v_chroma_coeff_addr  = DCAM_BASE;
	uint32_t                *tmp_buf = NULL;
	uint32_t                *h_coeff = NULL;
	uint32_t                *v_coeff = NULL;
	uint32_t                *v_chroma_coeff = NULL;
	uint32_t                clk_switch_bit = 0;
	uint32_t                clk_switch_shift_bit = 0;
	uint32_t                clk_status_bit = 0;
	uint32_t                ver_tap_reg = 0;
	uint32_t                scale2yuv420 = 0;
	uint8_t                 y_tap = 0;
	uint8_t                 uv_tap = 0;

	DCAM_CHECK_ZERO(s_p_dcam_mod);

	if (DCAM_PATH_IDX_1 != path_index && DCAM_PATH_IDX_2 != path_index)
		return -DCAM_RTN_PARA_ERR;

	if (DCAM_PATH_IDX_1 == path_index) {
		path = &s_p_dcam_mod->dcam_path1;
		h_coeff_addr += DCAM_SC1_H_TAB_OFFSET;
		v_coeff_addr += DCAM_SC1_V_TAB_OFFSET;
		v_chroma_coeff_addr += DCAM_SC1_V_CHROMA_TAB_OFFSET;
		clk_switch_bit = BIT_3;
		clk_switch_shift_bit = 3;
		clk_status_bit = BIT_5;
		ver_tap_reg = DCAM_PATH1_CFG;
	} else if (DCAM_PATH_IDX_2 == path_index) {
		path = &s_p_dcam_mod->dcam_path2;
		h_coeff_addr += DCAM_SC2_H_TAB_OFFSET;
		v_coeff_addr += DCAM_SC2_V_TAB_OFFSET;
		v_chroma_coeff_addr += DCAM_SC2_V_CHROMA_TAB_OFFSET;
		clk_switch_bit = BIT_4;
		clk_switch_shift_bit = 4;
		clk_status_bit = BIT_6;
		ver_tap_reg = DCAM_PATH2_CFG;
	}

	if (DCAM_YUV420 == path->output_format) {
	    scale2yuv420 = 1;
	}

	DCAM_TRACE("DCAM: _dcam_set_sc_coeff {%d %d %d %d}, 420=%d \n",
		path->sc_input_size.w,
		path->sc_input_size.h,
		path->output_size.w,
		path->output_size.h, scale2yuv420);


	tmp_buf = dcam_get_scale_coeff_addr();

	if (NULL == tmp_buf) {
		return -DCAM_RTN_PATH_NO_MEM;
	}

	h_coeff = tmp_buf;
	v_coeff = tmp_buf + (DCAM_SC_COEFF_COEF_SIZE/4);
	v_chroma_coeff = v_coeff + (DCAM_SC_COEFF_COEF_SIZE/4);

	down(&s_p_dcam_mod->scale_coeff_mem_sema);

	if (!(Dcam_GenScaleCoeff((int16_t)path->sc_input_size.w,
		(int16_t)path->sc_input_size.h,
		(int16_t)path->output_size.w,
		(int16_t)path->output_size.h,
		h_coeff,
		v_coeff,
		v_chroma_coeff,
		scale2yuv420,
		&y_tap,
		&uv_tap,
		tmp_buf + (DCAM_SC_COEFF_COEF_SIZE*3/4),
		DCAM_SC_COEFF_TMP_SIZE))) {
		printk("DCAM: _dcam_set_sc_coeff Dcam_GenScaleCoeff error! \n");
		up(&s_p_dcam_mod->scale_coeff_mem_sema);
		return -DCAM_RTN_PATH_GEN_COEFF_ERR;
	}

	for (i = 0; i < DCAM_SC_COEFF_H_NUM; i++) {
		REG_WR(h_coeff_addr, *h_coeff);
		h_coeff_addr += 4;
		h_coeff++;
	}

	for (i = 0; i < DCAM_SC_COEFF_V_NUM; i++) {
		REG_WR(v_coeff_addr, *v_coeff);
		v_coeff_addr += 4;
		v_coeff++;
	}

	for (i = 0; i < DCAM_SC_COEFF_V_CHROMA_NUM; i++) {
		REG_WR(v_chroma_coeff_addr, *v_chroma_coeff);
		v_chroma_coeff_addr += 4;
		v_chroma_coeff++;
	}

	path->scale_tap.y_tap = y_tap;
	path->scale_tap.uv_tap = uv_tap;
	path->valid_param.scale_tap = 1;

	up(&s_p_dcam_mod->scale_coeff_mem_sema);

	return DCAM_RTN_SUCCESS;
}

LOCAL void _dcam_force_copy_ext(enum dcam_path_index path_index, uint32_t path_copy, uint32_t coef_copy)
{
	uint32_t         reg_val = 0;

	if (DCAM_PATH_IDX_1 == path_index) {
		if(path_copy)
			reg_val |= BIT_10;
		if(coef_copy)
			reg_val |= BIT_14;

		dcam_glb_reg_mwr(DCAM_CONTROL, reg_val, reg_val, DCAM_CONTROL_REG);
	} else if(DCAM_PATH_IDX_2 == path_index) {
		if(path_copy)
			reg_val |= BIT_12;
		if(coef_copy)
			reg_val |= BIT_16;

		dcam_glb_reg_mwr(DCAM_CONTROL, reg_val, reg_val, DCAM_CONTROL_REG);
	} else {
		DCAM_TRACE("DCAM: _dcam_force_copy_ext invalid path index: %d \n", path_index);
	}
}

LOCAL void _dcam_auto_copy_ext(enum dcam_path_index path_index, uint32_t path_copy, uint32_t coef_copy)
{
	uint32_t         reg_val = 0;

	if (DCAM_PATH_IDX_0 == path_index) {
		if (path_copy)
			reg_val |= BIT_9;
		dcam_glb_reg_mwr(DCAM_CONTROL, reg_val, reg_val, DCAM_CONTROL_REG);
	} else if (DCAM_PATH_IDX_1 == path_index) {
		if (path_copy)
			reg_val |= BIT_11;
		if (coef_copy)
			reg_val |= BIT_15;
		dcam_glb_reg_mwr(DCAM_CONTROL, reg_val, reg_val, DCAM_CONTROL_REG);
	} else if (DCAM_PATH_IDX_2 == path_index) {
		if (path_copy)
			reg_val |= BIT_13;
		if (coef_copy)
			reg_val |= BIT_17;
		dcam_glb_reg_mwr(DCAM_CONTROL, reg_val, reg_val, DCAM_CONTROL_REG);
	} else {
		DCAM_TRACE("DCAM: _dcam_auto_copy_ext invalid path index: %d \n", path_index);
	}

}

LOCAL void _dcam_force_copy(enum dcam_path_index path_index)
{
	if (DCAM_PATH_IDX_1 == path_index) {
		dcam_glb_reg_mwr(DCAM_CONTROL, BIT_10, 1 << 10, DCAM_CONTROL_REG);
	} else if (DCAM_PATH_IDX_2 == path_index) {
		dcam_glb_reg_mwr(DCAM_CONTROL, BIT_12, 1 << 12, DCAM_CONTROL_REG);
	} else {
		DCAM_TRACE("DCAM: _dcam_force_copy invalid path index: %d \n", path_index);
	}
}

LOCAL void _dcam_auto_copy(enum dcam_path_index path_index)
{
	if (DCAM_PATH_IDX_0 == path_index) {
		dcam_glb_reg_mwr(DCAM_CONTROL, BIT_9, 1 << 9, DCAM_CONTROL_REG);
	} else if(DCAM_PATH_IDX_1 == path_index) {
		dcam_glb_reg_mwr(DCAM_CONTROL, BIT_11, 1 << 11, DCAM_CONTROL_REG);
	} else if(DCAM_PATH_IDX_2 == path_index) {
		dcam_glb_reg_mwr(DCAM_CONTROL, BIT_13, 1 << 13, DCAM_CONTROL_REG);
	} else {
		DCAM_TRACE("DCAM: _dcam_auto_copy invalid path index: %d \n", path_index);
	}
}

LOCAL void _dcam_reg_trace(void)
{
#ifdef DCAM_DRV_DEBUG
	uint32_t                addr = 0;

	printk("DCAM: Register list");
	for (addr = DCAM_CFG; addr <= CAP_SENSOR_CTRL; addr += 16) {
		printk("\n 0x%x: 0x%x 0x%x 0x%x 0x%x",
			addr,
			REG_RD(addr),
			REG_RD(addr + 4),
			REG_RD(addr + 8),
			REG_RD(addr + 12));
	}

	printk("\n");
#endif
}

#if 0
LOCAL void    _dcam_sensor_sof(void)
{
	dcam_isr_func           user_func = s_user_func[DCAM_SN_SOF];
	void                    *data = s_user_data[DCAM_SN_SOF];

	DCAM_CHECK_ZERO_VOID(s_p_dcam_mod);

	DCAM_TRACE("DCAM: _sn_sof \n");

	if (user_func) {
		(*user_func)(NULL, data);
	}

	return;
}
#endif

LOCAL void    _dcam_sensor_eof(void)
{
	dcam_isr_func           user_func = s_user_func[DCAM_SN_EOF];
	void                    *data = s_user_data[DCAM_SN_EOF];

	DCAM_CHECK_ZERO_VOID(s_p_dcam_mod);

	DCAM_TRACE("DCAM: _sn_eof \n");

	if (user_func) {
		(*user_func)(NULL, data);
	}

	return;
}

LOCAL void    _dcam_cap_sof(void)
{
	dcam_isr_func           user_func = s_user_func[DCAM_CAP_SOF];
	void                    *data = s_user_data[DCAM_CAP_SOF];

	DCAM_CHECK_ZERO_VOID(s_p_dcam_mod);

	DCAM_TRACE("DCAM: _cap_sof \n");

	_dcam_stopped_notice();

	if (user_func) {
		(*user_func)(NULL, data);
	}

	return;
}

LOCAL void    _dcam_cap_eof(void)
{
	dcam_isr_func           user_func = s_user_func[DCAM_CAP_EOF];
	void                    *data = s_user_data[DCAM_CAP_EOF];

	DCAM_CHECK_ZERO_VOID(s_p_dcam_mod);

	if (user_func) {
		(*user_func)(NULL, data);
	}

	return;
}

LOCAL void    _dcam_path0_done(void)
{
	enum dcam_drv_rtn       rtn = DCAM_RTN_SUCCESS;
	dcam_isr_func           user_func = s_user_func[DCAM_PATH0_DONE];
	void                    *data = s_user_data[DCAM_PATH0_DONE];
	struct dcam_path_desc   *path;
	struct dcam_frame       *frame;

	DCAM_CHECK_ZERO_VOID(s_p_dcam_mod);
	path = &s_p_dcam_mod->dcam_path0;
	if (0 == path->valide) {
		printk("DCAM: path0 not valid \n");
		return;
	}
	if (path->need_stop) {
		dcam_glb_reg_awr(DCAM_CFG, ~BIT_0, DCAM_CFG_REG);
		path->need_stop = 0;
	}
	_dcam_path_done_notice(DCAM_PATH_IDX_0);
	DCAM_TRACE("DCAM 0 \n");
	rtn = _dcam_path_set_next_frm(DCAM_PATH_IDX_0, false);
	if (rtn) {
		printk("DCAM: path 0 wait \n");
		return;
	}
	_dcam_auto_copy(DCAM_PATH_IDX_0);

	rtn = _dcam_frame_dequeue(&path->frame_queue, &frame);
	if (0 == rtn) {
		frame->width = path->output_size.w;
		frame->height = path->output_size.h;
		if (user_func) {
			(*user_func)(frame, data);
		}
	}
	return;
}

LOCAL void    _dcam_path0_overflow(void)
{
	dcam_isr_func           user_func = s_user_func[DCAM_PATH0_OV];
	void                    *data = s_user_data[DCAM_PATH0_OV];
	struct dcam_path_desc   *path;
	struct dcam_frame       *frame = NULL;

	DCAM_CHECK_ZERO_VOID(s_p_dcam_mod);

	printk("DCAM: _path0_overflow \n");
	path = &s_p_dcam_mod->dcam_path0;

	if (path->output_frame_cur) {
		if (path->output_frame_cur->prev) {
			frame = path->output_frame_cur->prev->prev;

			if (user_func) {
				(*user_func)(frame, data);
			}
		}
	} else {
		printk("DCAM: path0 not cfg  \n");
	}

	return;
}

LOCAL void    _dcam_path1_done(void)
{
	enum dcam_drv_rtn       rtn = DCAM_RTN_SUCCESS;
	dcam_isr_func           user_func = s_user_func[DCAM_PATH1_DONE];
	void                    *data = s_user_data[DCAM_PATH1_DONE];
	struct dcam_path_desc   *path;
	struct dcam_frame       *frame;

	DCAM_CHECK_ZERO_VOID(s_p_dcam_mod);
	path = &s_p_dcam_mod->dcam_path1;
	if (0 == path->valide) {
		printk("DCAM: path1 not valid \n");
		return;
	}

	DCAM_TRACE("DCAM: 1\n");

	if (path->need_stop) {
		dcam_glb_reg_awr(DCAM_CFG, ~BIT_1, DCAM_CFG_REG);
		path->need_stop = 0;
	}

	rtn = _dcam_path_set_next_frm(DCAM_PATH_IDX_1, false);
	if (path->is_update) {
		_dcam_path1_set();
		path->is_update = 0;
		DCAM_TRACE("DCAM: path1 updated \n");
		_dcam_auto_copy_ext(DCAM_PATH_IDX_1, true, true);
	} else {
		_dcam_auto_copy(DCAM_PATH_IDX_1);
	}

	_dcam_path_done_notice(DCAM_PATH_IDX_1);

	if (rtn) {
		printk("DCAM: 1 wait \n");
		return;
	}
	rtn = _dcam_frame_dequeue(&path->frame_queue, &frame);
	if (0 == rtn) {
		frame->width = path->output_size.w;
		frame->height = path->output_size.h;
		DCAM_TRACE("DCAM: path1 frame 0x%x, y uv, 0x%x 0x%x \n",
			(int)frame, frame->yaddr, frame->uaddr);
		if (user_func) {
			(*user_func)(frame, data);
		}
	}
	return;
}

LOCAL void    _dcam_path1_overflow(void)
{
	dcam_isr_func           user_func = s_user_func[DCAM_PATH1_OV];
	void                    *data = s_user_data[DCAM_PATH1_OV];
	struct dcam_path_desc   *path;
	struct dcam_frame       *frame = NULL;

	DCAM_CHECK_ZERO_VOID(s_p_dcam_mod);

	printk("DCAM: _path1_overflow \n");
	path = &s_p_dcam_mod->dcam_path1;
	frame = path->output_frame_cur->prev->prev;

	if (user_func) {
		(*user_func)(frame, data);
	}

	return;
}

LOCAL void    _dcam_sensor_line_err(void)
{
	dcam_isr_func           user_func = s_user_func[DCAM_SN_LINE_ERR];
	void                    *data = s_user_data[DCAM_SN_LINE_ERR];

	DCAM_CHECK_ZERO_VOID(s_p_dcam_mod);

	printk("DCAM: _line_err \n");

	if (user_func) {
		(*user_func)(NULL, data);
	}

	return;
}

LOCAL void    _dcam_sensor_frame_err(void)
{
	dcam_isr_func           user_func = s_user_func[DCAM_SN_FRAME_ERR];
	void                    *data = s_user_data[DCAM_SN_FRAME_ERR];

	DCAM_CHECK_ZERO_VOID(s_p_dcam_mod);

	printk("DCAM: _frame_err \n");

	if (user_func) {
		(*user_func)(NULL, data);
	}

	return;
}

LOCAL void    _dcam_jpeg_buf_ov(void)
{
	dcam_isr_func           user_func = s_user_func[DCAM_JPEG_BUF_OV];
	void                    *data = s_user_data[DCAM_JPEG_BUF_OV];
	struct dcam_path_desc   *path;
	struct dcam_frame       *frame = NULL;

	DCAM_CHECK_ZERO_VOID(s_p_dcam_mod);

	printk("DCAM: _jpeg_overflow \n");
	path = &s_p_dcam_mod->dcam_path0;
	frame = path->output_frame_cur->prev->prev;

	if (user_func) {
		(*user_func)(frame, data);
	}

	return;
}

LOCAL void    _dcam_path2_done(void)
{
	enum dcam_drv_rtn       rtn = DCAM_RTN_SUCCESS;
	dcam_isr_func           user_func = s_user_func[DCAM_PATH2_DONE];
	void                    *data = s_user_data[DCAM_PATH2_DONE];
	struct dcam_path_desc   *path;
	struct dcam_frame       *frame;

	if (atomic_read(&s_resize_flag)) {
		frame = NULL;
	} else {
		DCAM_CHECK_ZERO_VOID(s_p_dcam_mod);
		path = &s_p_dcam_mod->dcam_path2;

		DCAM_TRACE("DCAM: 2 \n");

		if (path->need_stop) {
			dcam_glb_reg_awr(DCAM_CFG, ~BIT_2, DCAM_CFG_REG);
			path->need_stop = 0;
		}

		rtn = _dcam_path_set_next_frm(DCAM_PATH_IDX_2, false);
		if (path->is_update) {
			_dcam_path2_set();
			path->is_update = 0;
			DCAM_TRACE("DCAM: path2 updated \n");
			_dcam_auto_copy_ext(DCAM_PATH_IDX_2, true, true);
		} else {
			_dcam_auto_copy(DCAM_PATH_IDX_2);
		}

		_dcam_path_done_notice(DCAM_PATH_IDX_2);

		if (rtn) {
			printk("DCAM: 2 wait \n");
			return;
		}

		rtn = _dcam_frame_dequeue(&path->frame_queue, &frame);
		if (0 == rtn) {
			DCAM_TRACE("DCAM: path2 frame 0x%x, y uv, 0x%x 0x%x \n",
				(int)frame, frame->yaddr, frame->uaddr);
			frame->width = path->output_size.w;
			frame->height = path->output_size.h;

		}
	}

	if (user_func) {
		(*user_func)(frame, data);
	}

	return;
}

LOCAL void    _dcam_path2_ov(void)
{
	dcam_isr_func           user_func = s_user_func[DCAM_PATH2_OV];
	void                    *data = s_user_data[DCAM_PATH2_OV];
	struct dcam_path_desc   *path;
	struct dcam_frame       *frame = NULL;

	DCAM_CHECK_ZERO_VOID(s_p_dcam_mod);

	printk("DCAM: _path2_overflow \n");
	path = &s_p_dcam_mod->dcam_path2;
	frame = path->output_frame_cur->prev->prev;

	if (user_func) {
		(*user_func)(frame, data);
	}

	return;
}

LOCAL void    _dcam_isp_ov(void)
{
	dcam_isr_func           user_func = s_user_func[DCAM_ISP_OV];
	void                    *data = s_user_data[DCAM_ISP_OV];

	DCAM_CHECK_ZERO_VOID(s_p_dcam_mod);

	printk("DCAM: _isp_overflow \n");

	if (user_func) {
		(*user_func)(NULL, data);
	}

	return;
}

LOCAL void    _dcam_mipi_ov(void)
{
	dcam_isr_func           user_func = s_user_func[DCAM_MIPI_OV];
	void                    *data = s_user_data[DCAM_MIPI_OV];

	DCAM_CHECK_ZERO_VOID(s_p_dcam_mod);


	if (user_func) {
		(*user_func)(NULL, data);
	}

	printk("DCAM: _mipi_overflow \n");
	return;
}

LOCAL void    _dcam_rot_done(void)
{
	dcam_isr_func           user_func = s_user_func[DCAM_ROT_DONE];
	void                    *data = s_user_data[DCAM_ROT_DONE];

	DCAM_TRACE("DCAM: rot_done \n");

	if (user_func) {
		(*user_func)(NULL, data);
	}

	return;
}

LOCAL void    _dcam_path1_slice_done(void)
{
	dcam_isr_func           user_func = s_user_func[DCAM_PATH1_SLICE_DONE];
	void                    *data = s_user_data[DCAM_PATH1_SLICE_DONE];

	DCAM_TRACE("DCAM: 1 slice done \n");

	if (user_func) {
		(*user_func)(NULL, data);
	}

	return;
}

LOCAL void    _dcam_path2_slice_done(void)
{
	dcam_isr_func           user_func = s_user_func[DCAM_PATH2_SLICE_DONE];
	void                    *data = s_user_data[DCAM_PATH2_SLICE_DONE];

	DCAM_TRACE("DCAM: 2 slice done \n");

	if (user_func) {
		(*user_func)(NULL, data);
	}

	return;
}

LOCAL void    _dcam_raw_slice_done(void)
{
	dcam_isr_func           user_func = s_user_func[DCAM_RAW_SLICE_DONE];
	void                    *data = s_user_data[DCAM_RAW_SLICE_DONE];

	DCAM_TRACE("DCAM: 0 slice done \n");

	if (user_func) {
		(*user_func)(NULL, data);
	}

	return;
}

LOCAL void    _dcam_err_pre_proc(void)
{
	DCAM_CHECK_ZERO_VOID(s_p_dcam_mod);

	s_p_dcam_mod->err_happened = 1;
	printk("DCAM: err, 0x%x, ISP int 0x%x \n",
		REG_RD(DCAM_INT_STS),
		REG_RD(SPRD_ISP_BASE + 0x2080));

	dcam_glb_reg_mwr(DCAM_CONTROL, BIT_2, 0, DCAM_CONTROL_REG); /* Cap Disable */
	_dcam_reg_trace();
	_dcam_stopped();
	if (0 == atomic_read(&s_resize_flag) &&
		0 == atomic_read(&s_rotation_flag)) {
			dcam_reset(DCAM_RST_ALL, 1);
	}
	return;
}
LOCAL void    _dcam_wait_for_stop(void)
{
	int                     rtn = -1;

	DCAM_CHECK_ZERO_VOID(s_p_dcam_mod);
	if (s_p_dcam_mod->err_happened) {
		return;
	}
	if (DCAM_CAPTURE_MODE_SINGLE == s_p_dcam_mod->dcam_mode) {
		return;
	}

	DCAM_TRACE("DCAM: Wait for stop \n");
	mdelay(5);

	s_p_dcam_mod->wait_stop = 1;
	rtn = down_timeout(&s_p_dcam_mod->stop_sema, DCAM_PATH_TIMEOUT);
	if (rtn) {
		printk("DCAM: Failed to wait for stop \n");
	}
	udelay(1000);
	return;
}

LOCAL void    _dcam_stopped(void)
{
	DCAM_CHECK_ZERO_VOID(s_p_dcam_mod);

	DCAM_TRACE("DCAM: stopped, %d \n", s_p_dcam_mod->wait_stop);

	_dcam_path_done_notice(DCAM_PATH_IDX_0);
	_dcam_path_done_notice(DCAM_PATH_IDX_1);
	_dcam_path_done_notice(DCAM_PATH_IDX_2);
	_dcam_stopped_notice();
	return;
}

LOCAL int  _dcam_internal_init(void)
{
	int                     ret = 0;

	s_p_dcam_mod = (struct dcam_module*)vzalloc(sizeof(struct dcam_module));
	DCAM_CHECK_ZERO(s_p_dcam_mod);

	sema_init(&s_p_dcam_mod->stop_sema, 0);
	sema_init(&s_p_dcam_mod->dcam_path0.tx_done_sema, 0);
	sema_init(&s_p_dcam_mod->dcam_path1.tx_done_sema, 0);
	sema_init(&s_p_dcam_mod->dcam_path2.tx_done_sema, 0);
	sema_init(&s_p_dcam_mod->resize_done_sema, 0);
	sema_init(&s_p_dcam_mod->rotation_done_sema, 0);
	sema_init(&s_p_dcam_mod->scale_coeff_mem_sema, 1);
	return ret;
}
LOCAL void _dcam_internal_deinit(void)
{
	unsigned long flag;

	spin_lock_irqsave(&dcam_mod_lock, flag);
	if (!DCAM_ADDR_INVALID(s_p_dcam_mod)) {
		vfree(s_p_dcam_mod);
		s_p_dcam_mod = NULL;
	}
	spin_unlock_irqrestore(&dcam_mod_lock, flag);
	return;
}

LOCAL void _dcam_wait_path_done(enum dcam_path_index path_index, uint32_t *p_flag)
{
	int                     ret = 0;
	struct dcam_path_desc   *p_path = NULL;
	unsigned long           flag;

	DCAM_CHECK_ZERO_VOID(s_p_dcam_mod);

	if (s_p_dcam_mod->err_happened) {
		return;
	}
	if (DCAM_CAPTURE_MODE_SINGLE == s_p_dcam_mod->dcam_mode) {
		return;
	}
	if (DCAM_PATH_IDX_0 == path_index) {
		p_path = &s_p_dcam_mod->dcam_path0;
	} else if (DCAM_PATH_IDX_1 == path_index) {
		p_path = &s_p_dcam_mod->dcam_path1;
	} else if (DCAM_PATH_IDX_2 == path_index) {
		p_path = &s_p_dcam_mod->dcam_path2;
	} else {
		printk("DCAM: Wrong index 0x%x \n", path_index);
		return;
	}
	DCAM_TRACE("DCAM: path done wait %d, %d \n", p_path->wait_for_done, p_path->tx_done_sema.count);

	spin_lock_irqsave(&dcam_lock, flag);
	if (p_flag) {
		*p_flag = 1;
	}
	p_path->wait_for_done = 1;
	spin_unlock_irqrestore(&dcam_lock, flag);
	ret = down_timeout(&p_path->tx_done_sema, DCAM_PATH_TIMEOUT);
	if (ret) {
		printk("DCAM: Failed to wait path 0x%x done \n", path_index);
	}

	return;
}

LOCAL void _dcam_path_done_notice(enum dcam_path_index path_index)
{
	struct dcam_path_desc   *p_path = NULL;

	DCAM_CHECK_ZERO_VOID(s_p_dcam_mod);

	if (DCAM_PATH_IDX_0 == path_index) {
		p_path = &s_p_dcam_mod->dcam_path0;
	} else if(DCAM_PATH_IDX_1 == path_index) {
		p_path = &s_p_dcam_mod->dcam_path1;
	} else if(DCAM_PATH_IDX_2 == path_index) {
		p_path = &s_p_dcam_mod->dcam_path2;
	} else {
		printk("DCAM: Wrong index 0x%x \n", path_index);
		return;
	}
	DCAM_TRACE("DCAM: path done notice %d, %d \n", p_path->wait_for_done, p_path->tx_done_sema.count);
	if (p_path->wait_for_done) {
		up(&p_path->tx_done_sema);
		p_path->wait_for_done = 0;
	}

	return;
}

LOCAL void _dcam_stopped_notice(void)
{
	DCAM_CHECK_ZERO_VOID(s_p_dcam_mod);

	if (s_p_dcam_mod->wait_stop) {
		up(&s_p_dcam_mod->stop_sema);
		s_p_dcam_mod->wait_stop = 0;
	}
}

void mm_clk_register_trace(void)
{
   uint32_t i = 0;

   printk("mm_clk_reg, part 1 \n");
   for (i = 0 ; i <= 8; i += 4 ) {
		printk("MMAHB: %x val:%x \b\n",
			(SPRD_MMAHB_BASE + i),
			REG_RD(SPRD_MMAHB_BASE + i));
   }

   printk("mm_clk_reg, part 2 \n");
   for (i = 0x20 ; i <= 0x38; i += 4 ) {
		printk("MMCKG: %x val:%x \b\n",
			(SPRD_MMCKG_BASE + i),
			REG_RD(SPRD_MMCKG_BASE + i));
   }
}
