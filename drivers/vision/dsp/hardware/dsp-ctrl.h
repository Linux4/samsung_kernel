/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Samsung Exynos SoC series dsp driver
 *
 * Copyright (c) 2019 Samsung Electronics Co., Ltd.
 *              http://www.samsung.com/
 */

#ifndef __DSP_CTRL_H__
#define __DSP_CTRL_H__

#include <linux/seq_file.h>

#include "dsp-hw-ctrl.h"

struct dsp_system;

unsigned int dsp_ctrl_sm_readl(unsigned int reg_addr);
int dsp_ctrl_sm_writel(unsigned int reg_addr, int val);
unsigned int dsp_ctrl_offset_readl(unsigned int reg_id, unsigned int offset);
int dsp_ctrl_offset_writel(unsigned int reg_id, unsigned int offset, int val);
unsigned int dsp_ctrl_readl(unsigned int reg_id);
int dsp_ctrl_writel(unsigned int reg_id, int val);

int dsp_ctrl_common_init(struct dsp_ctrl *ctrl);
int dsp_ctrl_init(struct dsp_ctrl *ctrl);
int dsp_ctrl_all_init(struct dsp_ctrl *ctrl);
int dsp_ctrl_start(struct dsp_ctrl *ctrl);
int dsp_ctrl_reset(struct dsp_ctrl *ctrl);
int dsp_ctrl_force_reset(struct dsp_ctrl *ctrl);

void dsp_ctrl_reg_print(unsigned int reg_id);
void dsp_ctrl_dump(void);
void dsp_ctrl_pc_dump(void);
void dsp_ctrl_reserved_sm_dump(void);
void dsp_ctrl_userdefined_dump(void);
void dsp_ctrl_fw_info_dump(void);

void dsp_ctrl_user_reg_print(struct seq_file *file, unsigned int reg_id);
void dsp_ctrl_user_dump(struct seq_file *file);
void dsp_ctrl_user_pc_dump(struct seq_file *file);
void dsp_ctrl_user_reserved_sm_dump(struct seq_file *file);
void dsp_ctrl_user_userdefined_dump(struct seq_file *file);
void dsp_ctrl_user_fw_info_dump(struct seq_file *file);

int dsp_ctrl_open(struct dsp_ctrl *ctrl);
int dsp_ctrl_close(struct dsp_ctrl *ctrl);
int dsp_ctrl_probe(struct dsp_system *sys);
void dsp_ctrl_remove(struct dsp_ctrl *ctrl);

#endif
