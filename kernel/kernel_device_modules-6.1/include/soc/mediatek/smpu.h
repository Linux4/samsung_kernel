/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

#include <linux/types.h>
#include <linux/irqreturn.h>

//#include <soc/mediatek/emi.h>

#define VIO_TYPE_NSMPU 0
#define VIO_TYPE_SSMPU 1
#define VIO_TYPE_NKP 2
#define VIO_TYPE_SKP 3

#define MTK_SMPU_MAX_CMD_LEN 256
#define MAX_GPU_VIO_LEN 256

#ifndef CHECK_BIT
#define CHECK_BIT(var, pos) ((var) & (1 << (pos)))
#endif

#define MTK_SMPU_CLEAR_KP 1
#define AXI_SET_NUM(num) (num / 3)

#define WRITE_SRINFO 0
#define READ_SRINFO 1
#define WRITE_AXI 2
#define READ_AXI 3
#define WRITE_AXI_MSB 4
#define READ_AXI_MSB 5

#define MTK_EMIMPU_READ 2
#define MTK_EMIMPU_CLEAR_MD 7
#define MTK_EMIMPU_CLEAR_KP 9
#define MTK_SMPU_KP_AID_REGF_OFFSET		(16)
#define MTK_SMPU_KP_AID_MASK			((-1U) ^ ((1U << MTK_SMPU_KP_AID_REGF_OFFSET) - 1))
#define MTK_SMPU_KP_AID(smpu_kp_reg)		((smpu_kp_reg & MTK_SMPU_KP_AID_MASK) >>  \
						MTK_SMPU_KP_AID_REGF_OFFSET)

struct smpu_reg_info_t {
	unsigned int offset;
	unsigned int value;
	unsigned int leng;
};

struct bypass_axi_info_t {
	unsigned int port;
	unsigned int axi_mask;
	unsigned int axi_value;
};

struct smpu_vio_dump_info_t {
	unsigned int vio_dump_idx;
	unsigned int vio_dump_pos;
};

typedef void (*smpu_md_handler)(const char *vio_msg);
typedef irqreturn_t (*smpu_isr_hook)(struct smpu_reg_info_t *dump,
				     unsigned int leng, int vio_type);
int mtk_smpu_isr_hook_register(smpu_isr_hook hook);
void smpu_clear_md_violation(void);

struct smpu {
	const char *name;
	char *vio_msg;
	char *vio_msg_gpu;
	bool is_vio;
	bool is_bypass;

	struct smpu_reg_info_t *dump_reg;
	struct smpu_reg_info_t *clear_reg;
	struct smpu_reg_info_t *mask_reg;
	struct smpu_reg_info_t *clear_md_reg;

	struct smpu_vio_dump_info_t *vio_reg_info;

	unsigned int dump_cnt;
	unsigned int clear_cnt;
	unsigned int mask_cnt;
	unsigned int clear_md_cnt;
	unsigned int vio_dump_cnt;
	void __iomem **mpu_base;
	/* interrupt id */
	unsigned int irq;

	smpu_isr_hook by_plat_isr_hook;
	smpu_md_handler md_handler;

	/*bypass list only for smpu node*/
	unsigned int *bypass_miu_reg;
	unsigned int bypass_miu_reg_num;
	struct bypass_axi_info_t *bypass_axi;
	unsigned int *gpu_bypass_list;
	unsigned int bypass_axi_num;
	/* As SLC B mode enable */
	bool slc_b_mode;
};
extern struct smpu *global_nsmpu, *global_ssmpu;
//static struct smpu *global_ssmpu, *global_nsmpu, *global_skp, *global_nkp;

int mtk_smpu_md_handling_register(smpu_md_handler md_handling_func);
