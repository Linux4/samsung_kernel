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
#ifndef _SCALE_DRV_H_
#define _SCALE_DRV_H_

#include <linux/types.h>
#include "scale_reg.h"

/*#define SCALE_DEBUG*/

#ifdef SCALE_DEBUG
	#define SCALE_TRACE             printk
#else
	#define SCALE_TRACE             pr_debug
#endif

enum scale_drv_rtn {
	SCALE_RTN_SUCCESS = 0,
	SCALE_RTN_PARA_ERR = 0x10,
	SCALE_RTN_IO_ID_ERR,
	SCALE_RTN_ISR_ID_ERR,
	SCALE_RTN_MASTER_SEL_ERR,
	SCALE_RTN_MODE_ERR,
	SCALE_RTN_TIMEOUT,

	SCALE_RTN_SRC_SIZE_ERR = 0x30,
	SCALE_RTN_TRIM_SIZE_ERR,
	SCALE_RTN_DES_SIZE_ERR,
	SCALE_RTN_IN_FMT_ERR,
	SCALE_RTN_OUT_FMT_ERR,
	SCALE_RTN_SC_ERR,
	SCALE_RTN_SUB_SAMPLE_ERR,
	SCALE_RTN_ADDR_ERR,
	SCALE_RTN_NO_MEM,
	SCALE_RTN_GEN_COEFF_ERR,
	SCALE_RTN_SRC_ERR,
	SCALE_RTN_ENDIAN_ERR,
	SCALE_RTN_MAX
};

enum scale_fmt {
	SCALE_YUV422 = 0,
	SCALE_YUV420,
	SCALE_YUV400,
	SCALE_YUV420_3FRAME,
	SCALE_RGB565,
	SCALE_RGB888,
	SCALE_FTM_MAX
};

enum scale_irq_id {
	SCALE_TX_DONE = 0,
	SCALE_IRQ_NUMBER
};

enum scale_cfg_id {
	SCALE_INPUT_SIZE = 0,
	SCALE_INPUT_RECT,
	SCALE_INPUT_FORMAT,
	SCALE_INPUT_ADDR,
	SCALE_INPUT_ENDIAN,
	SCALE_OUTPUT_SIZE,
	SCALE_OUTPUT_FORMAT,
	SCALE_OUTPUT_ADDR,
	SCALE_OUTPUT_ENDIAN,
	SCALE_TEMP_BUFF,
	SCALE_SCALE_MODE,
	SCALE_SLICE_SCALE_HEIGHT,
	SCALE_START,
	SCALE_CONTINUE,
	SCALE_IS_DONE,
	SCALE_STOP,
	SCALE_INIT,
	SCALE_DEINIT,
	SCALE_CFG_ID_E_MAX
};

enum scale_iram_owner {
	IRAM_FOR_SCALE = 0,
	IRAM_FOR_OTHER
};

enum scale_clk_sel {
	SCALE_CLK_128M = 0,
	SCALE_CLK_76M8,
	SCALE_CLK_64M,
	SCALE_CLK_48M
};

enum scale_data_endian {
	SCALE_ENDIAN_BIG = 0,
	SCALE_ENDIAN_LITTLE,
	SCALE_ENDIAN_HALFBIG,
	SCALE_ENDIAN_HALFLITTLE,
	SCALE_ENDIAN_MAX
};

enum scle_mode {
	SCALE_MODE_NORMAL = 0,
	SCALE_MODE_SLICE,
	SCALE_MODE_SLICE_READDR,
	SCALE_MODE_MAX
};

enum scale_process {
	SCALE_PROCESS_SUCCESS = 0,
	SCALE_PROCESS_EXIT = -1,
	SCALE_PROCESS_SYS_BUSY = -2,
	SCALE_PROCESS_MAX = 0xFF
};
struct scale_endian_sel {
	uint8_t               y_endian;
	uint8_t               uv_endian;
	uint8_t               reserved0;
	uint8_t               reserved1;
};

struct scale_size {
	uint32_t               w;
	uint32_t               h;
};

struct scale_rect {
	uint32_t               x;
	uint32_t               y;
	uint32_t               w;
	uint32_t               h;
};

struct scale_addr {
	uint32_t               yaddr;
	uint32_t               uaddr;
	uint32_t               vaddr;
};

struct scale_frame {
	uint32_t                type;
	uint32_t                lock;
	uint32_t                flags;
	uint32_t                fid;
	uint32_t                width;
	uint32_t                height;
	uint32_t                height_uv;
	uint32_t                yaddr;
	uint32_t                uaddr;
	uint32_t                vaddr;
	struct scale_endian_sel endian;
	enum scale_process scale_result;
};

typedef void (*scale_isr_func)(struct scale_frame* frame, void* u_data);
int32_t scale_module_en(void);
int32_t scale_module_dis(void);
int  scale_coeff_alloc(void);
void  scale_coeff_free(void);
int32_t scale_start(void);
int32_t scale_stop(void);
int32_t scale_reg_isr(enum scale_irq_id id, scale_isr_func user_func, void* u_data);
int32_t scale_cfg(enum scale_cfg_id id, void *param);
int32_t scale_read_registers(uint32_t* reg_buf, uint32_t *buf_len);
#endif
