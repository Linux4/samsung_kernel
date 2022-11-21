/*
** (C) Copyright 2011 Marvell International Ltd.
**  		All Rights Reserved

** This software file (the "File") is distributed by Marvell International Ltd.
** under the terms of the GNU General Public License Version 2, June 1991 (the "License").
** You may use, redistribute and/or modify this File in accordance with the terms and
** conditions of the License, a copy of which is available along with the File in the
** license.txt file or by writing to the Free Software Foundation, Inc., 59 Temple Place,
** Suite 330, Boston, MA 02111-1307 or on the worldwide web at http://www.gnu.org/licenses/gpl.txt.
** THE FILE IS DISTRIBUTED AS-IS, WITHOUT WARRANTY OF ANY KIND, AND THE IMPLIED WARRANTIES
** OF MERCHANTABILITY OR FITNESS FOR A PARTICULAR PURPOSE ARE EXPRESSLY DISCLAIMED.
** The License provides additional details about this warranty disclaimer.
*/

#include <linux/fs.h>
#include <linux/miscdevice.h>
#include <linux/pagemap.h>
#include <linux/poll.h>
#include <linux/vmalloc.h>
#include <linux/string.h>
#include <linux/preempt.h>
#include <linux/sched.h>
#include <linux/version.h>
#include <linux/cpufreq.h>
#include <linux/errno.h>
#include <linux/ptrace.h>
#include <linux/mqueue.h>
#include <linux/ipc_namespace.h>
#include <linux/mman.h>
#include <asm/io.h>
#include <asm/traps.h>

#include "ufunc_hook_internal.h"
#include "common.h"

#define ARM_NOP_INST       0xe1a00000        /* MOV   R0, R0 */
#define THUMB16_NOP_INST   0x4600            /* MOV   R0, R0 */
#define THUMB32_NOP_INST   0x0000ea4f        /* MOV   R0, R0 */

extern int (*access_process_vm_func)(struct task_struct *tsk, unsigned long addr, void *buf, int len, int write);



#define COND_EQ         0x0
#define COND_NE         0x1
#define COND_CS         0x2
#define COND_CC         0x3
#define COND_MI         0x4
#define COND_PL         0x5
#define COND_VS         0x6
#define COND_VC         0x7
#define COND_HI         0x8
#define COND_LS         0x9
#define COND_GE         0xa
#define COND_LT         0xb
#define COND_GT         0xc
#define COND_LE         0xd
#define COND_AL         0xe
#define COND_NV         0xf

#define CPSR_N		0x80000000
#define CPSR_Z		0x40000000
#define CPSR_C		0x20000000
#define CPSR_V		0x10000000
#define FLAG_T		0x20

static void split_thumb32_inst(u32 inst, u16 * first_inst, u16 * second_inst)
{
	if (first_inst != NULL)
	{
		*first_inst = inst & 0xffff;
	}

	if (second_inst != NULL)
	{
		*second_inst = inst >> 16;
	}
}

static u32 make_thumb32_inst(u16 first_inst, u16 second_inst)
{
	return (second_inst << 16) + first_inst;
}

static bool condition_pass(int cond, struct pt_regs * regs)
{
	bool z_flag;
	bool c_flag;
	bool n_flag;
	bool v_flag;

	z_flag = ((regs->ARM_cpsr & CPSR_Z) != 0);
	c_flag = ((regs->ARM_cpsr & CPSR_C) != 0);
	n_flag = ((regs->ARM_cpsr & CPSR_N) != 0);
	v_flag = ((regs->ARM_cpsr & CPSR_V) != 0);

	if ((cond == COND_AL) || (cond == COND_NV))
		return true;

	switch (cond)
	{
	case COND_EQ:
		return z_flag;
	case COND_NE:
		return !z_flag;
	case COND_CS:
		return c_flag;
	case COND_CC:
		return !c_flag;
	case COND_MI:
		return n_flag;
	case COND_PL:
		return !n_flag;
	case COND_VS:
		return v_flag;
	case COND_VC:
		return !v_flag;
	case COND_HI:
		return c_flag && !z_flag;
	case COND_LS:
		return !c_flag && z_flag;
	case COND_GE:
		return (n_flag == v_flag);
	case COND_LT:
		return (n_flag != v_flag);
	case COND_GT:
		return !z_flag && (n_flag == v_flag);
	case COND_LE:
		return !z_flag && (n_flag != v_flag);
	}

	return false;
}

static unsigned int read_reg_value(int reg_num, struct pt_regs * regs, unsigned int inst_addr)
{
	if (reg_num != 15)
		return regs->uregs[reg_num];
	else
	{
		if (!thumb_mode(regs))
			return (inst_addr + 8) & ~0x1;
		else
			return (inst_addr + 4) & ~0x1;
	}
}

static void write_reg_value(int reg_num, unsigned int value, struct pt_regs * regs)
{
	regs->uregs[reg_num] = value;
}

void branch_write_pc(unsigned int address, struct pt_regs * regs)
{
	if (regs->ARM_cpsr & FLAG_T)
	{
		/* thumb mode */
		regs->ARM_pc = (address & ~0x1);
	}
	else
	{
		/* arm mode */
		regs->ARM_pc = (address & ~0x3);
	}
}

void bx_write_pc(unsigned int address, struct pt_regs * regs)
{
	if (px_get_bit(address, 0) == 1)
	{
		regs->ARM_cpsr |= FLAG_T;
		regs->ARM_pc = (address & ~0x1);
	}
	else
	{
		regs->ARM_cpsr &= ~FLAG_T;
		regs->ARM_pc = address;
	}
}

void load_write_pc(unsigned int address, struct pt_regs * regs)
{
	return bx_write_pc(address, regs);
}

static int arm_displace_unchanged(struct pt_regs * regs, struct displaced_desc * dsc)
{
	dsc->displaced_insts[0].inst = dsc->orig_inst;
	dsc->displaced_insts[0].size = ARM_INST_SIZE;
	dsc->displaced_insts_num = 1;

	dsc->pre_handler = NULL;
	dsc->post_handler = NULL;

	return 0;
}

static int arm_displace_unpredicable(struct pt_regs * regs, struct displaced_desc * dsc)
{
	return arm_displace_unchanged(regs, dsc);
}

static int arm_displace_undef(struct pt_regs * regs, struct displaced_desc * dsc)
{
	return arm_displace_unchanged(regs, dsc);
}

static int arm_blx_reg_pre_handler(struct pt_regs * regs, struct displaced_desc * dsc)
{
	unsigned int orig_inst = dsc->orig_inst;

	int cond = px_get_bits(orig_inst, 28, 31);

	if (condition_pass(cond, regs))
	{
		/* if the branch will be taken, replace the instruction with
		 *
		 *     MOV R0, R0
		 */
		dsc->displaced_insts[0].inst = ARM_NOP_INST;
		dsc->displaced_insts[0].size = ARM_INST_SIZE;
		dsc->displaced_insts_num = 1;
	}
	else
	{
		dsc->displaced_insts[0].inst = ARM_NOP_INST;
		dsc->displaced_insts[0].size = ARM_INST_SIZE;
		dsc->displaced_insts_num = 1;
	}

	return 0;
}

static int arm_blx_reg_post_handler(struct pt_regs * regs, struct displaced_desc * dsc)
{
	int rm;
	unsigned int rm_value;
	unsigned int pc;
	unsigned int orig_inst = dsc->orig_inst;
	int cond = px_get_bits(orig_inst, 28, 31);

	rm = px_get_bits(orig_inst, 0, 3);
	pc = read_reg_value(15, &dsc->orig_regs, dsc->orig_regs.ARM_pc);

	if (condition_pass(cond, &dsc->orig_regs))
	{
		rm_value = read_reg_value(rm, &dsc->orig_regs, dsc->orig_regs.ARM_pc);

		write_reg_value(14, pc - 4, regs);
		//write_reg_value(15, rm_value & ~0x1, regs);
		bx_write_pc(rm_value, regs);

		return -1;
	}
	else
	{
		return 0;
	}
}

static int arm_displace_blx_reg(struct pt_regs * regs, struct displaced_desc * dsc)
{
	int rm;
	unsigned int orig_inst = dsc->orig_inst;

	rm = px_get_bits(orig_inst, 0, 3);

	if (rm == 15)
		return arm_displace_unpredicable(regs, dsc);

	dsc->pre_handler  = &arm_blx_reg_pre_handler;
	dsc->post_handler = &arm_blx_reg_post_handler;

	return 0;
}

static int arm_bx_reg_pre_handler(struct pt_regs * regs, struct displaced_desc * dsc)
{
	unsigned int orig_inst = dsc->orig_inst;

	int cond = px_get_bits(orig_inst, 28, 31);

	if (condition_pass(cond, regs))
	{
		/* if the branch will be taken, replace the instruction with
		 *
		 *     MOV R0, R0
		 */
		dsc->displaced_insts[0].inst = ARM_NOP_INST;
		dsc->displaced_insts[0].size = ARM_INST_SIZE;
		dsc->displaced_insts_num = 1;
	}
	else
	{
		dsc->displaced_insts[0].inst = ARM_NOP_INST;
		dsc->displaced_insts[0].size = ARM_INST_SIZE;
		dsc->displaced_insts_num = 1;
	}

	return 0;
}

static int arm_bx_reg_post_handler(struct pt_regs * regs, struct displaced_desc * dsc)
{
	int rm;
	unsigned int rm_value;

	unsigned int orig_inst = dsc->orig_inst;

	int cond = px_get_bits(orig_inst, 28, 31);

	rm = px_get_bits(orig_inst, 0, 3);

	if (condition_pass(cond, &dsc->orig_regs))
	{
		rm_value = read_reg_value(rm, &dsc->orig_regs, dsc->orig_regs.ARM_pc);
		//write_reg_value(15, rm_value & ~0x1, regs);
		bx_write_pc(rm_value, regs);

		return -1;
	}
	else
	{
		return 0;
	}
}

static int arm_displace_bx_reg(struct pt_regs * regs, struct displaced_desc * dsc)
{
	dsc->pre_handler  = &arm_bx_reg_pre_handler;
	dsc->post_handler = &arm_bx_reg_post_handler;
	return 0;
}

static int arm_blx_immed_pre_handler(struct pt_regs * regs, struct displaced_desc * dsc)
{
	unsigned int orig_inst = dsc->orig_inst;

	int cond = px_get_bits(orig_inst, 28, 31);

	if (condition_pass(cond, regs))
	{
		/* if the branch will be taken, replace the instruction with
		 *
		 *     MOV R0, R0
		 */
		dsc->displaced_insts[0].inst = ARM_NOP_INST;
		dsc->displaced_insts[0].size = ARM_INST_SIZE;
		dsc->displaced_insts_num = 1;
	}
	else
	{
		dsc->displaced_insts[0].inst = ARM_NOP_INST;
		dsc->displaced_insts[0].size = ARM_INST_SIZE;
		dsc->displaced_insts_num = 1;
	}

	return 0;
}

static int arm_blx_immed_post_handler(struct pt_regs * regs, struct displaced_desc * dsc)
{
	unsigned int imm24;
	unsigned int imm32;
	unsigned int pc;
	int h_bit;

	unsigned int orig_inst = dsc->orig_inst;

	int cond = px_get_bits(orig_inst, 28, 31);

	//pc = dsc->orig_regs.ARM_pc + 8;
	pc = read_reg_value(15, &dsc->orig_regs, dsc->orig_regs.ARM_pc);

	imm24 = px_get_bits(orig_inst, 0, 23);
	h_bit = px_get_bit(orig_inst, 24);

	imm32 = sign_extend((imm24 << 2) + (h_bit << 1), 25);

	if (condition_pass(cond, &dsc->orig_regs))
	{
		write_reg_value(14, pc - 4, regs);
		//write_reg_value(15, pc + imm32, regs);
		branch_write_pc(pc + imm32, regs);

		regs->ARM_cpsr |= FLAG_T;

		return -1;
	}
	else
	{
		return 0;
	}

}

static int arm_displace_blx_immed(struct pt_regs * regs, struct displaced_desc * dsc)
{
	dsc->pre_handler  = &arm_blx_immed_pre_handler;
	dsc->post_handler = &arm_blx_immed_post_handler;

	return 0;
}

static int arm_b_immed_pre_handler(struct pt_regs * regs, struct displaced_desc * dsc)
{
	unsigned int orig_inst = dsc->orig_inst;

	int cond = px_get_bits(orig_inst, 28, 31);

	if (condition_pass(cond, regs))
	{
		/* if the branch will be taken, replace the instruction with
		 *
		 *     MOV R0, R0
		 */
		dsc->displaced_insts[0].inst = ARM_NOP_INST;
		dsc->displaced_insts[0].size = ARM_INST_SIZE;
		dsc->displaced_insts_num = 1;
	}
	else
	{
		dsc->displaced_insts[0].inst = ARM_NOP_INST;
		dsc->displaced_insts[0].size = ARM_INST_SIZE;
		dsc->displaced_insts_num = 1;
	}

	return 0;
}

static int arm_b_immed_post_handler(struct pt_regs * regs, struct displaced_desc * dsc)
{
	unsigned int imm24;
	unsigned int imm32;
	unsigned int pc;

	unsigned int orig_inst = dsc->orig_inst;

	int cond = px_get_bits(orig_inst, 28, 31);

	//pc = dsc->orig_regs.ARM_pc + 8;
	pc = read_reg_value(15, &dsc->orig_regs, dsc->orig_regs.ARM_pc);

	imm24 = px_get_bits(orig_inst, 0, 23);

	imm32 = sign_extend(imm24 << 2, 25);

	if (condition_pass(cond, &dsc->orig_regs))
	{
		//write_reg_value(15, pc + imm32, regs);
		branch_write_pc(pc + imm32, regs);
		return -1;
	}
	else
	{
		return 0;
	}

}

static int arm_displace_b_immed(struct pt_regs * regs, struct displaced_desc * dsc)
{
	dsc->pre_handler  = &arm_b_immed_pre_handler;
	dsc->post_handler = &arm_b_immed_post_handler;

	return 0;
}

static int arm_bl_immed_pre_handler(struct pt_regs * regs, struct displaced_desc * dsc)
{
	unsigned int orig_inst = dsc->orig_inst;

	int cond = px_get_bits(orig_inst, 28, 31);

	if (condition_pass(cond, regs))
	{
		/* if the branch will be taken, replace the instruction with
		 *
		 *     MOV R0, R0
		 */
		dsc->displaced_insts[0].inst = ARM_NOP_INST;
		dsc->displaced_insts[0].size = ARM_INST_SIZE;
		dsc->displaced_insts_num = 1;
	}
	else
	{
		dsc->displaced_insts[0].inst = ARM_NOP_INST;
		dsc->displaced_insts[0].size = ARM_INST_SIZE;
		dsc->displaced_insts_num = 1;
	}

	return 0;
}

static int arm_bl_immed_post_handler(struct pt_regs * regs, struct displaced_desc * dsc)
{
	unsigned int imm24;
	unsigned int imm32;
	unsigned int pc;

	unsigned int orig_inst = dsc->orig_inst;

	int cond = px_get_bits(orig_inst, 28, 31);

	//pc = dsc->orig_regs.ARM_pc + 8;
	pc = read_reg_value(15, &dsc->orig_regs, dsc->orig_regs.ARM_pc);

	imm24 = px_get_bits(orig_inst, 0, 23);

	imm32 = sign_extend(imm24 << 2, 25);

	if (condition_pass(cond, &dsc->orig_regs))
	{
		write_reg_value(14, pc - 4, regs);
		//write_reg_value(15, pc + imm32, regs);
		branch_write_pc(pc + imm32, regs);
		return -1;
	}
	else
	{
		return 0;
	}

}

static int arm_displace_bl_immed(struct pt_regs * regs, struct displaced_desc * dsc)
{
	dsc->pre_handler  = &arm_bl_immed_pre_handler;
	dsc->post_handler = &arm_bl_immed_post_handler;

	return 0;
}

static int arm_ldc_ldc2_pre_handler(struct pt_regs * regs, struct displaced_desc * dsc)
{
	unsigned int orig_inst;
	int rn;
	unsigned int rn_value;
	unsigned int data;

	orig_inst = dsc->orig_inst;

	rn = px_get_bits(orig_inst, 16, 19);

	rn_value = read_reg_value(rn, regs, regs->ARM_pc);

	write_reg_value(0, rn_value, regs);

	data = orig_inst;
	data = px_change_bits(data, 16, 19, 0);        /* set Rn to R0 */

	dsc->displaced_insts[0].inst = data;
	dsc->displaced_insts[0].size = ARM_INST_SIZE;
	dsc->displaced_insts_num = 1;

	return 0;
}

static int arm_ldc_ldc2_post_handler(struct pt_regs * regs, struct displaced_desc * dsc)
{
	int rn;
	unsigned int orig_inst;

	orig_inst = dsc->orig_inst;

	rn = px_get_bits(orig_inst, 16, 19);

	/* save the value of R0 to Rn */
	write_reg_value(rn, regs->ARM_r0, regs);

	/* restore the registers values */
	write_reg_value(0, dsc->orig_regs.ARM_r0, regs);

	return 0;
}

static int arm_displace_ldc_ldc2_immed(struct pt_regs * regs, struct displaced_desc * dsc)
{
	int rn;
	unsigned int orig_inst;

	orig_inst = dsc->orig_inst;

	rn = px_get_bits(orig_inst, 16, 19);

	if (rn != 15)
		return arm_displace_unchanged(regs, dsc);

	dsc->pre_handler  = &arm_ldc_ldc2_pre_handler;
	dsc->post_handler = &arm_ldc_ldc2_post_handler;

	return 0;
}

static int arm_displace_ldc_ldc2_literal(struct pt_regs * regs, struct displaced_desc * dsc)
{
	int rn;
	unsigned int orig_inst;

	orig_inst = dsc->orig_inst;

	rn = px_get_bits(orig_inst, 16, 19);

	if (rn != 15)
		return arm_displace_unchanged(regs, dsc);

	dsc->pre_handler  = &arm_ldc_ldc2_pre_handler;
	dsc->post_handler = &arm_ldc_ldc2_post_handler;

	return 0;
}


static int arm_stc_stc2_pre_handler(struct pt_regs * regs, struct displaced_desc * dsc)
{
	unsigned int orig_inst;
	int rn;
	unsigned int rn_value;
	unsigned int data;

	orig_inst = dsc->orig_inst;

	rn = px_get_bits(orig_inst, 16, 19);

	rn_value = read_reg_value(rn, regs, regs->ARM_pc);

	write_reg_value(0, rn_value, regs);

	data = orig_inst;
	data = px_change_bits(data, 16, 19, 0);        /* set Rn to R0 */

	dsc->displaced_insts[0].inst = data;
	dsc->displaced_insts[0].size = ARM_INST_SIZE;
	dsc->displaced_insts_num = 1;

	return 0;
}

static int arm_stc_stc2_post_handler(struct pt_regs * regs, struct displaced_desc * dsc)
{
	int rn;
	unsigned int orig_inst;

	orig_inst = dsc->orig_inst;

	rn = px_get_bits(orig_inst, 16, 19);

	/* save the value of R0 to Rn */
	write_reg_value(rn, regs->ARM_r0, regs);

	/* restore the registers values */
	write_reg_value(0, dsc->orig_regs.ARM_r0, regs);

	return 0;
}

static int arm_displace_stc_stc2(struct pt_regs * regs, struct displaced_desc * dsc)
{
	int rn;

	unsigned int orig_inst;

	orig_inst = dsc->orig_inst;

	rn = px_get_bits(orig_inst, 16, 19);

	if (rn != 15)
		return arm_displace_unchanged(regs, dsc);

	dsc->pre_handler  = &arm_stc_stc2_pre_handler;
	dsc->post_handler = &arm_stc_stc2_post_handler;

	return 0;
}

static int arm_displace_mcr_mcr2(struct pt_regs * regs, struct displaced_desc * dsc)
{
	return arm_displace_unchanged(regs, dsc);
}

static int arm_displace_mrc_mrc2(struct pt_regs * regs, struct displaced_desc * dsc)
{
	return arm_displace_unchanged(regs, dsc);
}

static int arm_displace_advaned_simd_ldr_str(struct pt_regs * regs, struct displaced_desc * dsc)
{
	unsigned int orig_inst = dsc->orig_inst;
	unsigned int rn = px_get_bits(orig_inst, 16, 19);

	if (rn == 15)
	{
		return arm_displace_unpredicable(regs, dsc);
	}

	return arm_displace_unchanged(regs, dsc);
}

static int arm_displace_preload_pre_handler(struct pt_regs * regs, struct displaced_desc * dsc)
{
	int rn;
	unsigned int rn_value;
	unsigned int data;
	unsigned int orig_inst = dsc->orig_inst;

	rn = px_get_bits(orig_inst, 16, 19);

	rn_value = read_reg_value(rn, regs, regs->ARM_pc);

	write_reg_value(0, rn_value, regs);

	data = orig_inst;
	data = px_change_bits(data, 16, 19, 0);        /* set Rn to R0 */

	dsc->displaced_insts[0].inst = data;
	dsc->displaced_insts[0].size = ARM_INST_SIZE;
	dsc->displaced_insts_num = 1;

	return 0;
}

static int arm_displace_preload_post_handler(struct pt_regs * regs, struct displaced_desc * dsc)
{
	unsigned int orig_inst;
	int rn;

	orig_inst = dsc->orig_inst;
	rn = px_get_bits(orig_inst, 16, 19);

	/* save the value of R0 to Rn */
	write_reg_value(rn, regs->ARM_r0, regs);

	/* restore the registers values */
	write_reg_value(0, dsc->orig_regs.ARM_r0, regs);

	return 0;
}

static int arm_displace_pli(struct pt_regs * regs, struct displaced_desc * dsc)
{
	unsigned int orig_inst = dsc->orig_inst;

	int rn = px_get_bits(orig_inst, 16, 19);

	if (rn != 15)
		return arm_displace_unchanged(regs, dsc);

	dsc->pre_handler  = &arm_displace_preload_pre_handler;
	dsc->post_handler = &arm_displace_preload_post_handler;

	return 0;
}

static int arm_displace_pld_pldw_immed(struct pt_regs * regs, struct displaced_desc * dsc)
{
	unsigned int orig_inst = dsc->orig_inst;

	int rn = px_get_bits(orig_inst, 16, 19);

	if (rn != 15)
		return arm_displace_unchanged(regs, dsc);

	dsc->pre_handler  = &arm_displace_preload_pre_handler;
	dsc->post_handler = &arm_displace_preload_post_handler;

	return 0;
}

static int arm_displace_pld_pldw_literal(struct pt_regs * regs, struct displaced_desc * dsc)
{
	unsigned int orig_inst = dsc->orig_inst;

	int rn = px_get_bits(orig_inst, 16, 19);

	if (rn != 15)
		return arm_displace_unchanged(regs, dsc);

	dsc->pre_handler  = &arm_displace_preload_pre_handler;
	dsc->post_handler = &arm_displace_preload_post_handler;

	return 0;
}

static int arm_displace_misc_memhint_advancedsimd(struct pt_regs * regs, struct displaced_desc * dsc)
{
	int op1, op2;
	int rn;
	unsigned int orig_inst = dsc->orig_inst;

	op1 = px_get_bits(orig_inst, 20, 26);
	op2 = px_get_bits(orig_inst, 4, 7);
	rn  = px_get_bits(orig_inst, 16, 19);

	if ((op1 == 0x10) && ((op2 & 0x2) == 0) && ((rn & 0x1) == 0))
	{
		/* CPS */
		return arm_displace_unchanged(regs, dsc);
	}
	else if ((op1 == 0x10) && (op2 == 0) && ((rn & 0x1) == 1))
	{
		/* SETEND */
		return arm_displace_unchanged(regs, dsc);
	}
	else if ((op1 & 0x60) == 0x20)
	{
		/* Advanced SIMD data-processing instructions */
		return arm_displace_unchanged(regs, dsc);
	}
	else if ((op1 & 0x71) == 0x40)
	{
		/* Advanced SIMD element or structure load/store instructions */
		return arm_displace_advaned_simd_ldr_str(regs, dsc);
	}

	switch (op1)
	{
	case 0x41:
	case 0x48:
		/* Unallocated memory hint */
		return arm_displace_unchanged(regs, dsc);
	case 0x45:
	case 0x4d:
		/* PLI (immediate, literal) */
		return arm_displace_pli(regs, dsc);
	case 0x51:
	case 0x59:
		if (rn != 0xf)
		{
			/* PLD, PLDW (immediate) */
			return arm_displace_pld_pldw_immed(regs, dsc);
		}
		else
		{
			/* UNPREDICTABLE*/
			return arm_displace_unpredicable(regs, dsc);
		}

	case 0x55:
	case 0x5d:
		if (rn != 0xf)
		{
			/* PLD, PLDW (immediate) */
			return arm_displace_pld_pldw_immed(regs, dsc);
		}
		else
		{
			/* PLD (literal) */
			return arm_displace_pld_pldw_literal(regs, dsc);
		}

	case 57:
		switch (op2)
		{
		case 1:  /* CLREX */
			return arm_displace_unchanged(regs, dsc);
		case 4:  /* DSB */
			return arm_displace_unchanged(regs, dsc);
		case 5:  /* DMB */
			return arm_displace_unchanged(regs, dsc);
		case 6:  /* ISB */
			return arm_displace_unchanged(regs, dsc);
		default:
			break;
		}
	default:
		break;
	}

	return arm_displace_unchanged(regs, dsc);
}

static int arm_dp_immed_pre_handler(struct pt_regs * regs, struct displaced_desc * dsc)
{
	int rn, rd;

	unsigned int rn_value, rd_value;

	unsigned int data;
	unsigned int orig_inst = dsc->orig_inst;

	rn = px_get_bits(orig_inst, 16, 19);
	rd = px_get_bits(orig_inst, 12, 15);

	rn_value = read_reg_value(rn, regs, regs->ARM_pc);
	rd_value = read_reg_value(rd, regs, regs->ARM_pc);

	write_reg_value(0, rd_value, regs);
	write_reg_value(1, rn_value, regs);

	data = orig_inst;
	data = px_change_bits(data, 12, 15, 0);        /* set Rd to R0 */
	data = px_change_bits(data, 16, 19, 1);        /* set Rn to R1 */

	dsc->displaced_insts[0].inst = data;
	dsc->displaced_insts[0].size = ARM_INST_SIZE;
	dsc->displaced_insts_num = 1;

	//dsc->rd = rd;

	return 0;
}

static int arm_dp_immed_post_handler(struct pt_regs * regs, struct displaced_desc * dsc)
{
	unsigned int orig_inst;
	int rd;

	orig_inst = dsc->orig_inst;
	rd = px_get_bits(orig_inst, 12, 15);

	/* save the value of R0 to Rd */
	if ((rd != 15) || ((regs->ARM_r0 & 0x1) == 0))
	{
		write_reg_value(rd, regs->ARM_r0, regs);
	}
	else
	{
		write_reg_value(rd, regs->ARM_r0 & ~0x1, regs);

		regs->ARM_cpsr |= PSR_T_BIT;
	}

	/* restore the registers values */
	if (rd != 0)
		write_reg_value(0, dsc->orig_regs.ARM_r0, regs);

	if (rd != 1)
		write_reg_value(1, dsc->orig_regs.ARM_r1, regs);

	return 0;
}

static int arm_displace_dp_immed(struct pt_regs * regs, struct displaced_desc * dsc)
{
	unsigned int orig_inst = dsc->orig_inst;

	int rd = px_get_bits(orig_inst, 12, 15);
	int rn = px_get_bits(orig_inst, 16, 19);

	if ((rd != 15) && (rn != 15))
		return arm_displace_unchanged(regs, dsc);

	dsc->pre_handler  = &arm_dp_immed_pre_handler;
	dsc->post_handler = &arm_dp_immed_post_handler;

	return 0;
}

static int arm_ldr_ldrb_immed_literal_pre_handler(struct pt_regs * regs, struct displaced_desc * dsc)
{
	unsigned int orig_inst;
	int rn, rt;
	unsigned int rn_value, rt_value;
	unsigned int data;

	orig_inst = dsc->orig_inst;

	rn = px_get_bits(orig_inst, 16, 19);
	rt = px_get_bits(orig_inst, 12, 15);

	rn_value = read_reg_value(rn, regs, regs->ARM_pc);
	rt_value = read_reg_value(rt, regs, regs->ARM_pc);

	write_reg_value(0, rt_value, regs);
	write_reg_value(1, rn_value, regs);

	data = orig_inst;
	data = px_change_bits(data, 12, 15, 0);        /* set Rt to R0 */
	data = px_change_bits(data, 16, 19, 1);        /* set Rn to R1 */

	dsc->displaced_insts[0].inst = data;
	dsc->displaced_insts[0].size = ARM_INST_SIZE;
	dsc->displaced_insts_num = 1;

	//dsc->rd = rd;

	return 0;
}

static int arm_ldr_ldrb_immed_literal_post_handler(struct pt_regs * regs, struct displaced_desc * dsc)
{
	int rt, rn;
	unsigned int orig_inst;
	int p_bit, u_bit, w_bit;

	bool wback, add, index;

	orig_inst = dsc->orig_inst;

	rt = px_get_bits(orig_inst, 12, 15);
	rn = px_get_bits(orig_inst, 16, 19);

	p_bit = px_get_bit(orig_inst, 24);
	u_bit = px_get_bit(orig_inst, 23);
	w_bit = px_get_bit(orig_inst, 21);

	wback = ((p_bit == 0) || (w_bit == 1));
	add   = (u_bit == 1);
	index = (p_bit == 1);

	/* save the value of R0 to Rt */
	if (rt != 15)
	{
		write_reg_value(rt, regs->ARM_r0, regs);
	}
	else
	{
		load_write_pc(regs->ARM_r0, regs);
	}

	/* save the value of R1 to Rn */
	if (wback)
		write_reg_value(rn, regs->ARM_r1, regs);

	/* restore the registers values */
	if (rt != 0)
		write_reg_value(0, dsc->orig_regs.ARM_r0, regs);

	if (rt != 1)
		write_reg_value(1, dsc->orig_regs.ARM_r1, regs);

	return 0;
}

static int arm_ldr_ldrb_reg_pre_handler(struct pt_regs * regs, struct displaced_desc * dsc)
{
	unsigned int orig_inst;
	int rn, rt, rm;
	unsigned int rn_value, rt_value, rm_value;
	unsigned int data;

	orig_inst = dsc->orig_inst;

	rn = px_get_bits(orig_inst, 16, 19);
	rt = px_get_bits(orig_inst, 12, 15);
	rm = px_get_bits(orig_inst, 0, 3);

	rn_value = read_reg_value(rn, regs, regs->ARM_pc);
	rt_value = read_reg_value(rt, regs, regs->ARM_pc);
	rm_value = read_reg_value(rm, regs, regs->ARM_pc);

	write_reg_value(0, rt_value, regs);
	write_reg_value(1, rn_value, regs);
	write_reg_value(2, rm_value, regs);

	data = orig_inst;
	data = px_change_bits(data, 12, 15, 0);        /* set Rt to R0 */
	data = px_change_bits(data, 16, 19, 1);        /* set Rn to R1 */
	data = px_change_bits(data, 0, 3, 2);          /* set Rm to R2 */

	dsc->displaced_insts[0].inst = data;
	dsc->displaced_insts[0].size = ARM_INST_SIZE;
	dsc->displaced_insts_num = 1;

	return 0;
}

static int arm_ldr_ldrb_reg_post_handler(struct pt_regs * regs, struct displaced_desc * dsc)
{
	int rt, rn, rm;
	unsigned int orig_inst;
	int p_bit, u_bit, w_bit;

	bool wback;

	orig_inst = dsc->orig_inst;

	rt = px_get_bits(orig_inst, 12, 15);
	rn = px_get_bits(orig_inst, 16, 19);
	rm = px_get_bits(orig_inst, 0, 3);

	p_bit = px_get_bit(orig_inst, 24);
	u_bit = px_get_bit(orig_inst, 23);
	w_bit = px_get_bit(orig_inst, 21);

	wback = ((p_bit == 0) || (w_bit == 1));

	/* save the value of R0 to Rt */
	write_reg_value(rt, regs->ARM_r0, regs);

	/* save the value of R1 to Rn */
	if (wback)
		write_reg_value(rn, regs->ARM_r1, regs);

	/* save the value of R2 to Rm */
	//write_reg_value(rm, regs->ARM_r2, regs);

	/* restore the registers values */
	if (rt != 0)
		write_reg_value(0, dsc->orig_regs.ARM_r0, regs);

	if (rt != 1)
		write_reg_value(1, dsc->orig_regs.ARM_r1, regs);

	if (rt != 2)
		write_reg_value(2, dsc->orig_regs.ARM_r2, regs);

	if (rt == 15)
		return -1;
	else
		return 0;
}

static int arm_displace_ldr_ldrb(struct pt_regs * regs, struct displaced_desc * dsc)
{
	unsigned int orig_inst;
	int register_form;

	orig_inst = dsc->orig_inst;

	register_form = px_get_bit(orig_inst, 25);

	if (register_form == 0)
	{
		/*
		 * LDR[B][T]  <Rt>, [<Rn>, #+/-<offset_12>]
		 * LDR[B][T]  <Rt>, [<Rn>, #+/-<offset_12>]!
		 * LDR[B][T]  <Rt>, [<Rn>], #+/-<offset_12>
		 * LDR[B][T]  <Rt>, <label>
		 */
		int rn, rt;

		rn = px_get_bits(orig_inst, 16, 19);
		rt = px_get_bits(orig_inst, 12, 15);

		if ((rn != 15) && (rt != 15))
			return arm_displace_unchanged(regs, dsc);

		dsc->pre_handler  = &arm_ldr_ldrb_immed_literal_pre_handler;
		dsc->post_handler = &arm_ldr_ldrb_immed_literal_post_handler;
	}
	else
	{
		/*
		 * LDR[B][T] <Rt>, [<Rn>, +/-<Rm>{, <shift>}]
		 * LDR[B][T] <Rt>, [<Rn>, +/-<Rm>{, <shift>}]!
		 * LDR[B][T] <Rt>, [<Rn>], +/-<Rm>{, <shift>}
		 */
		int rn, rt, rm;

		rn = px_get_bits(orig_inst, 16, 19);
		rt = px_get_bits(orig_inst, 12, 15);
		rm = px_get_bits(orig_inst, 0, 3);

		if ((rn != 15) && (rt != 15) && (rm != 15))
			return arm_displace_unchanged(regs, dsc);

		dsc->pre_handler  = &arm_ldr_ldrb_reg_pre_handler;
		dsc->post_handler = &arm_ldr_ldrb_reg_post_handler;
	}

	return 0;
}

static int arm_str_strb_immed_literal_pre_handler(struct pt_regs * regs, struct displaced_desc * dsc)
{
	unsigned int orig_inst;
	int rn, rt;
	unsigned int rn_value, rt_value;
	unsigned int data;

	orig_inst = dsc->orig_inst;

	rn = px_get_bits(orig_inst, 16, 19);
	rt = px_get_bits(orig_inst, 12, 15);

	rn_value = read_reg_value(rn, regs, regs->ARM_pc);
	rt_value = read_reg_value(rt, regs, regs->ARM_pc);

	write_reg_value(0, rt_value, regs);
	write_reg_value(1, rn_value, regs);

	data = orig_inst;
	data = px_change_bits(data, 12, 15, 0);        /* set Rt to R0 */
	data = px_change_bits(data, 16, 19, 1);        /* set Rn to R1 */

	dsc->displaced_insts[0].inst = data;
	dsc->displaced_insts[0].size = ARM_INST_SIZE;
	dsc->displaced_insts_num = 1;

	return 0;
}

static int arm_str_strb_immed_literal_post_handler(struct pt_regs * regs, struct displaced_desc * dsc)
{
	int rt, rn;
	unsigned int orig_inst;
	int p_bit, u_bit, w_bit;

	bool wback, add, index;

	orig_inst = dsc->orig_inst;

	rt = px_get_bits(orig_inst, 12, 15);
	rn = px_get_bits(orig_inst, 16, 19);
	p_bit = px_get_bit(orig_inst, 24);
	u_bit = px_get_bit(orig_inst, 23);
	w_bit = px_get_bit(orig_inst, 21);

	wback = ((p_bit == 0) || (w_bit == 1));
	add   = (u_bit == 1);
	index = (p_bit == 1);

	/* save the value of R0 to Rt */
	//write_reg_value(rt, regs->ARM_r0, regs);


	/* save the value of R1 to Rn */
	if (wback)
		write_reg_value(rn, regs->ARM_r1, regs);

	/* restore the registers values */
	if (rt != 0)
		write_reg_value(0, dsc->orig_regs.ARM_r0, regs);

	if (rt != 1)
		write_reg_value(1, dsc->orig_regs.ARM_r1, regs);

	return 0;
}

static int arm_str_strb_reg_pre_handler(struct pt_regs * regs, struct displaced_desc * dsc)
{
	unsigned int orig_inst;
	int rn, rt, rm;
	unsigned int rn_value, rt_value, rm_value;
	unsigned int data;

	orig_inst = dsc->orig_inst;

	rn = px_get_bits(orig_inst, 16, 19);
	rt = px_get_bits(orig_inst, 12, 15);
	rm = px_get_bits(orig_inst, 0, 3);

	rn_value = read_reg_value(rn, regs, regs->ARM_pc);
	rt_value = read_reg_value(rt, regs, regs->ARM_pc);
	rm_value = read_reg_value(rm, regs, regs->ARM_pc);

	write_reg_value(0, rt_value, regs);
	write_reg_value(1, rn_value, regs);
	write_reg_value(2, rm_value, regs);

	data = orig_inst;
	data = px_change_bits(data, 12, 15, 0);        /* set Rt to R0 */
	data = px_change_bits(data, 16, 19, 1);        /* set Rn to R1 */
	data = px_change_bits(data, 0, 3, 2);          /* set Rm to R2 */

	dsc->displaced_insts[0].inst = data;
	dsc->displaced_insts[0].size = ARM_INST_SIZE;
	dsc->displaced_insts_num = 1;

	return 0;
}

static int arm_str_strb_reg_post_handler(struct pt_regs * regs, struct displaced_desc * dsc)
{
	int rt, rn, rm;
	unsigned int orig_inst;
	int p_bit, u_bit, w_bit;

	bool wback;

	orig_inst = dsc->orig_inst;

	rt = px_get_bits(orig_inst, 12, 15);
	rn = px_get_bits(orig_inst, 16, 19);
	rm = px_get_bits(orig_inst, 0, 3);

	p_bit = px_get_bit(orig_inst, 24);
	u_bit = px_get_bit(orig_inst, 23);
	w_bit = px_get_bit(orig_inst, 21);

	wback = ((p_bit == 0) || (w_bit == 1));
	/* save the value of R0 to Rt */
	//write_reg_value(rt, regs->ARM_r0, regs);

	/* save the value of R1 to Rn */
	if (wback)
		write_reg_value(rn, regs->ARM_r1, regs);

	/* save the value of R2 to Rm */
	//write_reg_value(rm, regs->ARM_r2, regs);

	/* restore the registers values */
	if (rt != 0)
		write_reg_value(0, dsc->orig_regs.ARM_r0, regs);

	if (rt != 1)
		write_reg_value(1, dsc->orig_regs.ARM_r1, regs);

	if (rt != 2)
		write_reg_value(2, dsc->orig_regs.ARM_r2, regs);

	return 0;
}

static int arm_displace_str_strb(struct pt_regs * regs, struct displaced_desc * dsc)
{
	unsigned int orig_inst;
	int register_form;

	orig_inst = dsc->orig_inst;

	register_form = px_get_bit(orig_inst, 25);

	if (register_form == 0)
	{
		/*
		 * STR[B][T]  <Rt>, [<Rn>, #+/-<offset_12>]
		 * STR[B][T]  <Rt>, [<Rn>, #+/-<offset_12>]!
		 * STR[B][T]  <Rt>, [<Rn>], #+/-<offset_12>
		 * STR[B][T]  <Rt>, <label>
		 */
		int rn, rt;

		rn = px_get_bits(orig_inst, 16, 19);
		rt = px_get_bits(orig_inst, 12, 15);

		if ((rn != 15) && (rt != 15))
			return arm_displace_unchanged(regs, dsc);

		dsc->pre_handler  = &arm_str_strb_immed_literal_pre_handler;
		dsc->post_handler = &arm_str_strb_immed_literal_post_handler;
	}
	else
	{
		/*
		 * STR[B][T] <Rt>, [<Rn>, +/-<Rm>{, <shift>}]
		 * STR[B][T] <Rt>, [<Rn>, +/-<Rm>{, <shift>}]!
		 * STR[B][T] <Rt>, [<Rn>], +/-<Rm>{, <shift>}
		 */
		int rn, rt, rm;

		rn = px_get_bits(orig_inst, 16, 19);
		rt = px_get_bits(orig_inst, 12, 15);
		rm = px_get_bits(orig_inst, 0, 3);

		if ((rn != 15) && (rt != 15) && (rm != 15))
			return arm_displace_unchanged(regs, dsc);

		dsc->pre_handler  = &arm_str_strb_reg_pre_handler;
		dsc->post_handler = &arm_str_strb_reg_post_handler;
	}

	return 0;
}

/* decode for ldr/str instruction */
static int arm_displace_ldr_str_ldrb_strb(struct pt_regs * regs, struct displaced_desc * dsc)
{
	unsigned int orig_inst = dsc->orig_inst;
	int load = px_get_bit(orig_inst, 20);


	if (load)
	{
		return arm_displace_ldr_ldrb(regs, dsc);
	}
	else
	{
		return arm_displace_str_strb(regs, dsc);
	}

	return 0;
}

static int arm_dp_reg_pre_handler(struct pt_regs * regs, struct displaced_desc * dsc)
{
	int rn, rd, rm;

	unsigned int rn_value, rd_value, rm_value;

	unsigned int data;
	unsigned int orig_inst = dsc->orig_inst;

	rm = px_get_bits(orig_inst, 0, 3);
	rn = px_get_bits(orig_inst, 16, 19);
	rd = px_get_bits(orig_inst, 12, 15);

	rm_value = read_reg_value(rm, regs, regs->ARM_pc);
	rn_value = read_reg_value(rn, regs, regs->ARM_pc);
	rd_value = read_reg_value(rd, regs, regs->ARM_pc);

	write_reg_value(0, rd_value, regs);
	write_reg_value(1, rn_value, regs);
	write_reg_value(2, rm_value, regs);

	data = orig_inst;
	data = px_change_bits(data, 12, 15, 0);        /* set Rd to R0 */
	data = px_change_bits(data, 16, 19, 1);        /* set Rn to R1 */
	data = px_change_bits(data, 0, 3, 2);          /* set Rm to R2 */

	dsc->displaced_insts[0].inst = data;
	dsc->displaced_insts[0].size = ARM_INST_SIZE;
	dsc->displaced_insts_num = 1;

	//dsc->rd = rd;

	return 0;
}

static int arm_dp_reg_post_handler(struct pt_regs * regs, struct displaced_desc * dsc)
{
	int rd;
	unsigned int orig_inst;

	orig_inst = dsc->orig_inst;

	rd = px_get_bits(orig_inst, 12, 15);

	/* save the value of R0 to Rd */
	if ((rd != 15) || ((regs->ARM_r0 & 0x1) == 0))
	{
		write_reg_value(rd, regs->ARM_r0, regs);
	}
	else
	{
		write_reg_value(rd, regs->ARM_r0 & ~0x1, regs);

		regs->ARM_cpsr |= PSR_T_BIT;
	}

	/* restore the registers values */
	if (rd != 0)
		write_reg_value(0, dsc->orig_regs.ARM_r0, regs);

	if (rd != 1)
		write_reg_value(1, dsc->orig_regs.ARM_r1, regs);

	if (rd != 2)
		write_reg_value(2, dsc->orig_regs.ARM_r2, regs);

	return -1;
}

static int arm_displace_dp_reg(struct pt_regs * regs, struct displaced_desc * dsc)
{
	unsigned int orig_inst = dsc->orig_inst;
	int rn = px_get_bits(orig_inst, 16, 19);
	int rd = px_get_bits(orig_inst, 12, 15);
	int rm = px_get_bits(orig_inst, 0, 3);

	if ((rm != 15) && (rn != 15) && (rd != 15))
		return arm_displace_unchanged(regs, dsc);

	dsc->pre_handler  = &arm_dp_reg_pre_handler;
	dsc->post_handler = &arm_dp_reg_post_handler;

	return 0;
}


static int arm_dp_reg_shift_reg_pre_handler(struct pt_regs * regs, struct displaced_desc * dsc)
{
	int rn, rd, rm, rs;
	unsigned int data;
	unsigned int orig_inst = dsc->orig_inst;
	unsigned int rd_value, rn_value, rm_value, rs_value;

	rm = px_get_bits(orig_inst, 0, 3);
	rs = px_get_bits(orig_inst, 8, 11);
	rd = px_get_bits(orig_inst, 12, 15);
	rn = px_get_bits(orig_inst, 16, 19);

	rd_value = read_reg_value(rd, regs, regs->ARM_pc);
	rn_value = read_reg_value(rn, regs, regs->ARM_pc);
	rm_value = read_reg_value(rm, regs, regs->ARM_pc);
	rs_value = read_reg_value(rs, regs, regs->ARM_pc);

	write_reg_value(0, rd_value, regs);
	write_reg_value(1, rn_value, regs);
	write_reg_value(2, rm_value, regs);
	write_reg_value(3, rs_value, regs);

	data = orig_inst;
	data = px_change_bits(data, 12, 15, 0);        /* set Rd to R0 */
	data = px_change_bits(data, 16, 19, 1);        /* set Rn to R1 */
	data = px_change_bits(data, 0, 3, 2);          /* set Rm to R2 */
	data = px_change_bits(data, 8, 11, 2);         /* set Rs to R3 */

	dsc->displaced_insts[0].inst = data;
	dsc->displaced_insts[0].size = ARM_INST_SIZE;
	dsc->displaced_insts_num = 1;

	//dsc->rd = rd;

	return 0;
}

static int arm_dp_reg_shift_reg_post_handler(struct pt_regs * regs, struct displaced_desc * dsc)
{
	int rd;
	unsigned int orig_inst;

	orig_inst = dsc->orig_inst;

	rd = px_get_bits(orig_inst, 12, 15);

	/* save the value of R0 to Rd */
	if ((rd != 15) || ((regs->ARM_r0 & 0x1) == 0))
	{
		write_reg_value(rd, regs->ARM_r0, regs);
	}
	else
	{
		write_reg_value(rd, regs->ARM_r0 & ~0x1, regs);

		regs->ARM_cpsr |= PSR_T_BIT;
	}

	/* restore the registers values */
	if (rd != 0)
		write_reg_value(0, dsc->orig_regs.ARM_r0, regs);

	if (rd != 1)
		write_reg_value(1, dsc->orig_regs.ARM_r1, regs);

	if (rd != 2)
		write_reg_value(2, dsc->orig_regs.ARM_r2, regs);

	if (rd != 3)
		write_reg_value(3, dsc->orig_regs.ARM_r3, regs);

	return 0;
}

static int arm_displace_dp_reg_shift_reg(struct pt_regs * regs, struct displaced_desc * dsc)
{
	unsigned int orig_inst = dsc->orig_inst;
	int rn = px_get_bits(orig_inst, 16, 19);
	int rd = px_get_bits(orig_inst, 12, 15);
	int rs = px_get_bits(orig_inst, 11, 8);
	int rm = px_get_bits(orig_inst, 3, 0);

	if ((rn != 15) && (rd != 15) && (rs != 15) && (rm != 15))
		return arm_displace_unchanged(regs, dsc);

	dsc->pre_handler  = &arm_dp_reg_shift_reg_pre_handler;
	dsc->post_handler = &arm_dp_reg_shift_reg_post_handler;

	return 0;
}

static int arm_displace_misc(struct pt_regs * regs, struct displaced_desc * dsc)
{
	unsigned int orig_inst = dsc->orig_inst;
	int op2 = px_get_bits(orig_inst, 4, 6);
	int op  = px_get_bits(orig_inst, 21, 22);

	switch (op2)
	{
	case 0x0:
		/* MRS and MSR */
		return arm_displace_unchanged(regs, dsc);

	case 0x1:
		if (op == 0x1)
			return arm_displace_bx_reg(regs, dsc);
		/* CLZ */
		if (op == 0x3)
			return arm_displace_unchanged(regs, dsc);

		break;

	case 0x2:
		/* BXJ */
		return arm_displace_unchanged(regs, dsc);

	case 0x3:
		/* BLX(register) */
		return arm_displace_blx_reg(regs, dsc);

	case 0x5:
		/* Saturating addition and subtraction */
		return arm_displace_unchanged(regs, dsc);

	case 0x7:
		/* BKPT */
		if (op == 0x1)
			return arm_displace_unchanged(regs, dsc);

		/* SMC */
		if (op == 0x3)
			return arm_displace_unchanged(regs, dsc);

		break;
	}

	return arm_displace_undef(regs, dsc);
}

static int arm_ldrd_strd_reg_pre_handler(struct pt_regs * regs, struct displaced_desc * dsc)
{
	int rt, rt2, rn, rm;
	unsigned int rt_value, rt2_value, rm_value, rn_value;
	unsigned int orig_inst = dsc->orig_inst;
	unsigned int data;

	rn = px_get_bits(orig_inst, 16, 19);
	rm = px_get_bits(orig_inst, 0, 3);
	rt = px_get_bits(orig_inst, 12, 15);
	rt2 = rt + 1;

	rn_value  = read_reg_value(rn, regs, regs->ARM_pc);
	rm_value  = read_reg_value(rm, regs, regs->ARM_pc);
	rt_value  = read_reg_value(rt, regs, regs->ARM_pc);
	rt2_value = read_reg_value(rt2, regs, regs->ARM_pc);

	write_reg_value(0, rn_value, regs);
	write_reg_value(1, rm_value, regs);
	write_reg_value(2, rt_value, regs);
	write_reg_value(3, rt2_value, regs);

	data = orig_inst;
	data = px_change_bits(data, 16, 19, 0);     /* change Rn to R0 */
	data = px_change_bits(data, 0, 3, 1);       /* change Rm to R1 */
	data = px_change_bits(data, 12, 15, 2);     /* change Rt to R2 */

	dsc->displaced_insts[0].inst = data;
	dsc->displaced_insts[0].size = ARM_INST_SIZE;
	dsc->displaced_insts_num     = 1;

	return 0;
}

static int arm_ldrd_strd_reg_post_handler(struct pt_regs * regs, struct displaced_desc * dsc)
{
	int rt, rt2, rn, rm;
	unsigned int orig_inst = dsc->orig_inst;
	int p_bit, u_bit, w_bit;
	bool wback;

	rn = px_get_bits(orig_inst, 16, 19);
	rm = px_get_bits(orig_inst, 0, 3);
	rt = px_get_bits(orig_inst, 12, 15);
	rt2 = rt + 1;

	p_bit = px_get_bit(orig_inst, 24);
	u_bit = px_get_bit(orig_inst, 23);
	w_bit = px_get_bit(orig_inst, 21);

	wback = ((p_bit == 0) || (w_bit == 1));

	/* set value of R0 to Rn */
	if (wback)
		write_reg_value(rn, regs->ARM_r0, regs);

	/* set value of R1 to Rm */
	//write_reg_value(rm, regs->ARM_r1, regs);

	/* set value of R2 to Rt */
	write_reg_value(rt, regs->ARM_r2, regs);

	/* set value of R3 to Rt2 */
	write_reg_value(rt2, regs->ARM_r3, regs);

	/* restore registers value */
	if ((rn != 0) && (rm != 0) && (rt != 0) && (rt2 != 0))
		write_reg_value(0, dsc->orig_regs.ARM_r0, regs);

	if ((rn != 1) && (rm != 1) && (rt != 1) && (rt2 != 1))
		write_reg_value(1, dsc->orig_regs.ARM_r1, regs);

	if ((rn != 2) && (rm != 2) && (rt != 2) && (rt2 != 2))
		write_reg_value(2, dsc->orig_regs.ARM_r2, regs);

	if ((rn != 3) && (rm != 3) && (rt != 3) && (rt2 != 3))
		write_reg_value(3, dsc->orig_regs.ARM_r3, regs);

	return 0;
}

static int arm_ldrd_strd_immed_literal_pre_handler(struct pt_regs * regs, struct displaced_desc * dsc)
{
	int rt, rt2, rn;
	unsigned int rt_value, rt2_value, rn_value;
	unsigned int orig_inst = dsc->orig_inst;
	unsigned int data;

	rn = px_get_bits(orig_inst, 16, 19);
	rt = px_get_bits(orig_inst, 12, 15);
	rt2 = rt + 1;

	rn_value  = read_reg_value(rn, regs, regs->ARM_pc);
	rt_value  = read_reg_value(rt, regs, regs->ARM_pc);
	rt2_value = read_reg_value(rt2, regs, regs->ARM_pc);

	write_reg_value(0, rn_value, regs);
	write_reg_value(2, rt_value, regs);
	write_reg_value(3, rt2_value, regs);

	data = orig_inst;
	data = px_change_bits(data, 16, 19, 0);     /* change Rn to R0 */
	data = px_change_bits(data, 12, 15, 2);     /* change Rt to R2 */

	dsc->displaced_insts[0].inst = data;
	dsc->displaced_insts[0].size = ARM_INST_SIZE;
	dsc->displaced_insts_num     = 1;

	return 0;
}

static int arm_ldrd_strd_immed_literal_post_handler(struct pt_regs * regs, struct displaced_desc * dsc)
{
	int rn, rt, rt2;
	unsigned int orig_inst = dsc->orig_inst;
	int p_bit, u_bit, w_bit;
	bool wback;

	rn = px_get_bits(orig_inst, 16, 19);
	rt = px_get_bits(orig_inst, 12, 15);
	rt2 = rt + 1;

	p_bit = px_get_bit(orig_inst, 24);
	u_bit = px_get_bit(orig_inst, 23);
	w_bit = px_get_bit(orig_inst, 21);

	wback = ((p_bit == 0) || (w_bit == 1));

	/* set value of R0 to Rn */
	if (wback)
		write_reg_value(rn, regs->ARM_r0, regs);

	/* set value of R2 to Rt */
	write_reg_value(rt, regs->ARM_r2, regs);

	/* set value of R3 to Rt2 */
	write_reg_value(rt2, regs->ARM_r3, regs);

	/* restore registers value */
	write_reg_value(0, dsc->orig_regs.ARM_r0, regs);
	write_reg_value(2, dsc->orig_regs.ARM_r2, regs);
	write_reg_value(3, dsc->orig_regs.ARM_r3, regs);

	return 0;
}

static int arm_displace_ldrd_strd(struct pt_regs * regs, struct displaced_desc * dsc)
{
	int rt, rt2, rn, rm;
	bool register_form;
	unsigned int orig_inst = dsc->orig_inst;

	rn = px_get_bits(orig_inst, 16, 19);
	rm = px_get_bits(orig_inst, 0, 3);
	rt = px_get_bits(orig_inst, 12, 15);
	rt2 = rt + 1;

	if (px_get_bit(orig_inst, 22) == 0)
		register_form = true;
	else
		register_form = false;

	if (register_form)
	{
		if ((rn != 15) && (rm != 15) && (rt != 15) && (rt2 != 15))
			return arm_displace_unchanged(regs, dsc);

			dsc->pre_handler  = &arm_ldrd_strd_reg_pre_handler;
		dsc->post_handler = &arm_ldrd_strd_reg_post_handler;
	}
	else
	{
		if ((rn != 15) && (rt != 15) && (rt2 != 15))
			return arm_displace_unchanged(regs, dsc);

		dsc->pre_handler  = &arm_ldrd_strd_immed_literal_pre_handler;
		dsc->post_handler = &arm_ldrd_strd_immed_literal_post_handler;
	}

	return 0;
}

static int arm_displace_ldrh_strh_ldrsh_strsh_ldrsb_strsb_reg_pre_handler(struct pt_regs * regs, struct displaced_desc * dsc)
{
	int rt, rn, rm;
	unsigned int rt_value, rm_value, rn_value;
	unsigned int orig_inst = dsc->orig_inst;
	unsigned int data;

	rn = px_get_bits(orig_inst, 16, 19);
	rm = px_get_bits(orig_inst, 0, 3);
	rt = px_get_bits(orig_inst, 12, 15);

	rn_value  = read_reg_value(rn, regs, regs->ARM_pc);
	rm_value  = read_reg_value(rm, regs, regs->ARM_pc);
	rt_value  = read_reg_value(rt, regs, regs->ARM_pc);

	write_reg_value(0, rn_value, regs);
	write_reg_value(1, rm_value, regs);
	write_reg_value(2, rt_value, regs);

	data = orig_inst;
	data = px_change_bits(data, 16, 19, 0);     /* change Rn to R0 */
	data = px_change_bits(data, 0, 3, 1);       /* change Rm to R1 */
	data = px_change_bits(data, 12, 15, 2);     /* change Rt to R2 */

	dsc->displaced_insts[0].inst = data;
	dsc->displaced_insts[0].size = ARM_INST_SIZE;
	dsc->displaced_insts_num     = 1;

	return 0;
}

static int arm_displace_ldrh_strh_ldrsh_strsh_ldrsb_strsb_reg_post_handler(struct pt_regs * regs, struct displaced_desc * dsc)
{
	int rt, rn, rm;
	unsigned int orig_inst = dsc->orig_inst;
	int p_bit, u_bit, w_bit;
	bool wback;

	p_bit = px_get_bit(orig_inst, 24);
	u_bit = px_get_bit(orig_inst, 23);
	w_bit = px_get_bit(orig_inst, 21);

	wback = ((p_bit == 0) || (w_bit == 1));

	rn = px_get_bits(orig_inst, 16, 19);
	rm = px_get_bits(orig_inst, 0, 3);
	rt = px_get_bits(orig_inst, 12, 15);

	/* set value of R0 to Rn */
	if (wback)
		write_reg_value(rn, regs->ARM_r0, regs);

	/* set value of R1 to Rm */
	//write_reg_value(rm, regs->ARM_r1, regs);

	/* set value of R2 to Rt */
	write_reg_value(rt, regs->ARM_r2, regs);

	/* restore registers value */
	if ((rn != 0) && (rm != 0) && (rt != 0))
		write_reg_value(0, dsc->orig_regs.ARM_r0, regs);

	if ((rn != 1) && (rm != 1) && (rt != 1))
		write_reg_value(1, dsc->orig_regs.ARM_r1, regs);

	if ((rn != 2) && (rm != 2) && (rt != 2))
		write_reg_value(2, dsc->orig_regs.ARM_r2, regs);

	return 0;
}

static int arm_displace_ldrh_strh_ldrsh_strsh_ldrsb_strsb_immed_literal_pre_handler(struct pt_regs * regs, struct displaced_desc * dsc)
{
	int rt, rn;
	unsigned int rt_value, rn_value;
	unsigned int orig_inst = dsc->orig_inst;
	unsigned int data;

	rn = px_get_bits(orig_inst, 16, 19);
	rt = px_get_bits(orig_inst, 12, 15);

	rn_value  = read_reg_value(rn, regs, regs->ARM_pc);
	rt_value  = read_reg_value(rt, regs, regs->ARM_pc);

	write_reg_value(0, rn_value, regs);
	write_reg_value(2, rt_value, regs);

	data = orig_inst;
	data = px_change_bits(data, 16, 19, 0);     /* change Rn to R0 */
	data = px_change_bits(data, 12, 15, 2);     /* change Rt to R2 */

	dsc->displaced_insts[0].inst = data;
	dsc->displaced_insts[0].size = ARM_INST_SIZE;
	dsc->displaced_insts_num     = 1;

	return 0;
}

static int arm_displace_ldrh_strh_ldrsh_strsh_ldrsb_strsb_immed_literal_post_handler(struct pt_regs * regs, struct displaced_desc * dsc)
{
	int rn, rt;
	unsigned int orig_inst = dsc->orig_inst;

	int p_bit, u_bit, w_bit;
	bool wback;

	p_bit = px_get_bit(orig_inst, 24);
	u_bit = px_get_bit(orig_inst, 23);
	w_bit = px_get_bit(orig_inst, 21);

	wback = ((p_bit == 0) || (w_bit == 1));

	rn = px_get_bits(orig_inst, 16, 19);
	rt = px_get_bits(orig_inst, 12, 15);

	/* set value of R0 to Rn */
	if (wback)
		write_reg_value(rn, regs->ARM_r0, regs);

	/* set value of R2 to Rt */
	write_reg_value(rt, regs->ARM_r2, regs);


	/* restore registers value */
	write_reg_value(0, dsc->orig_regs.ARM_r0, regs);
	write_reg_value(2, dsc->orig_regs.ARM_r2, regs);

	return 0;
}


static int arm_displace_ldrh_strh_ldrsh_strsh_ldrsb_strsb(struct pt_regs * regs, struct displaced_desc * dsc)
{
	int rt, rn, rm;
	bool register_form;
	unsigned int orig_inst = dsc->orig_inst;

	rn = px_get_bits(orig_inst, 16, 19);
	rm = px_get_bits(orig_inst, 0, 3);
	rt = px_get_bits(orig_inst, 12, 15);

	if (px_get_bit(orig_inst, 22) == 0)
		register_form = true;
	else
		register_form = false;

	if (register_form)
	{
		if ((rn != 15) && (rm != 15) && (rt != 15))
			return arm_displace_unchanged(regs, dsc);

			dsc->pre_handler  = &arm_displace_ldrh_strh_ldrsh_strsh_ldrsb_strsb_reg_pre_handler;
		dsc->post_handler = &arm_displace_ldrh_strh_ldrsh_strsh_ldrsb_strsb_reg_post_handler;
	}
	else
	{
		if ((rn != 15) && (rt != 15))
			return arm_displace_unchanged(regs, dsc);

		dsc->pre_handler  = &arm_displace_ldrh_strh_ldrsh_strsh_ldrsb_strsb_immed_literal_pre_handler;
		dsc->post_handler = &arm_displace_ldrh_strh_ldrsh_strsh_ldrsb_strsb_immed_literal_post_handler;
	}

	return 0;
}

static int arm_displace_extra_ldr_str(struct pt_regs * regs, struct displaced_desc * dsc)
{
	unsigned int orig_inst = dsc->orig_inst;
	int op2  = px_get_bits(orig_inst, 5, 6);
	int op1  = px_get_bits(orig_inst, 20, 24);
	int rn   = px_get_bits(orig_inst, 16, 19);

	if (rn != 15)
	{
		return arm_displace_unchanged(regs, dsc);
	}

	if (op2 == 0x1)
	{
		return arm_displace_ldrh_strh_ldrsh_strsh_ldrsb_strsb(regs, dsc);
	}

	if ((op1 & 0x1) == 0x0)
	{
		return arm_displace_ldrd_strd(regs, dsc);
	}
	else
	{
		return arm_displace_ldrh_strh_ldrsh_strsh_ldrsb_strsb(regs, dsc);
	}
	return 0;
}

static int arm_displace_extra_ldr_str_unprivileged(struct pt_regs * regs, struct displaced_desc * dsc)
{
	/* all registers can't be PC, otherwise it will be an unpredicable instruction */
	return arm_displace_unchanged(regs, dsc);
}

/* decode for data processing and miscellaneous instruction */
static int arm_displace_dp_misc(struct pt_regs * regs, struct displaced_desc * dsc)
{
	unsigned int op, op1, op2;
	unsigned int orig_inst = dsc->orig_inst;

	op  = px_get_bit(orig_inst, 25);
	op1 = px_get_bits(orig_inst, 20, 24);
	op2 = px_get_bits(orig_inst, 4, 7);

	if (op == 1)
	{
		if (op1 == 0x10)
		{
			/* 16-bit immediate load (MOV (immediate)) */
			return arm_displace_unchanged(regs, dsc);
		}
		else if (op1 == 0x14)
		{
			/* High halfword 16-bit immediate load (MOVT) */
			return arm_displace_unchanged(regs, dsc);
		}
		else if ((op1 == 0x12) || (op1 == 0x16))
		{
			/* MSR (immediate), and hints */
			return arm_displace_unchanged(regs, dsc);
		}
		else
		{
			/* Data-processing (immediate) */
			return arm_displace_dp_immed(regs, dsc);
		}
	}
	else
	{
		if ((op1 & 0x19) != 0x10)
		{
		        /* Data-processing (register) */
			if ((op2 & 0x1) == 0x0)
				return arm_displace_dp_reg(regs, dsc);

			/* Data-processing (register-shifted register) */
			if ((op2 & 0x9) == 0x1)
				return arm_displace_dp_reg_shift_reg(regs, dsc);
		}

		if ((op1 & 0x19) == 0x10)
		{
			/* Miscellaneous instructions */
			if ((op2 & 0x8) == 0x0)
				return arm_displace_misc(regs, dsc);

			/* Halfword multiply and multiply-accumulate */
			if ((op2 & 0x9) == 0x8)
				return arm_displace_unchanged(regs, dsc);
		}

		/* Multiply and multiply-accumulate */
		if (((op1 & 0x10) == 0x0) && (op2 == 0x9))
		{
			return arm_displace_unchanged(regs, dsc);
		}

		/* Synchronization primitives */
		if (((op1 & 0x10) == 0x10) && (op2 == 0x9))
		{
			return arm_displace_unchanged(regs, dsc);
		}

		if ((op1 & 0x12) != 0x2)
		{
			/* Extra load/store instructions */
			if ((op2 == 0xb) || (op2 == 0xd) || (op2 == 0xf))
				return arm_displace_extra_ldr_str(regs, dsc);
		}
		if ((op1 & 0x12) == 0x2)
		{
			/* Extra load/store instructions (unprivileged) */
			if ((op2 == 0xb) || (op2 == 0xd) || (op2 == 0xf))
				return arm_displace_extra_ldr_str_unprivileged(regs, dsc);
		}
	}

	return 0;
}

static int arm_displace_media(struct pt_regs * regs, struct displaced_desc * dsc)
{
	unsigned int op1, op2;
	unsigned int orig_inst = dsc->orig_inst;

	op1 = px_get_bits(orig_inst, 20, 24);
	op2 = px_get_bits(orig_inst, 5, 7);

	/* Parallel addition and subtraction, signed */
	if ((op1 & 0x1c) == 0x0)
		return arm_displace_unchanged(regs, dsc);

	/* Parallel addition and subtraction, unsigned */
	if ((op1 & 0x1c) == 0x4)
		return arm_displace_unchanged(regs, dsc);

	/* Packing, unpacking,saturation, and reversal */
	if ((op1 & 0x18) == 0x8)
		return arm_displace_unchanged(regs, dsc);

	/* Signed multiplies */
	if ((op1 & 0x18) == 0x10)
		return arm_displace_unchanged(regs, dsc);

	if (op1 == 0x18)
	{
		/* USAD8 */
		if ((op2 == 0) && (px_get_bits(orig_inst, 12, 15) == 0xf))
			return arm_displace_unchanged(regs, dsc);

		/* USADA8 */
		if ((op2 == 0) && (px_get_bits(orig_inst, 12, 15) != 0xf))
			return arm_displace_unchanged(regs, dsc);
	}

	/* SBFX */
	if (((op1 & 0x1e) == 0x1a) && ((op2 & 0x3) == 0x2))
		return arm_displace_unchanged(regs, dsc);

	if (((op1 & 0x1e) == 0x1c) && ((op2 & 0x3) == 0x0))
	{
		if (px_get_bits(orig_inst, 0, 4) == 0xf)     /* BFC */
			return arm_displace_unchanged(regs, dsc);
		else                                    /* BFI */
			return arm_displace_unchanged(regs, dsc);
	}

	/* UBFX */
	if (((op1 & 0x1e) == 0x1e) && ((op2 & 0x3) == 0x2))
		return arm_displace_unchanged(regs, dsc);

	return arm_displace_undef(regs, dsc);
}

static int arm_displace_ldm_pc_full_list_pre_handler(struct pt_regs * regs, struct displaced_desc * dsc)
{
	dsc->displaced_insts[0].inst = ARM_NOP_INST;
	dsc->displaced_insts[0].size = ARM_INST_SIZE;
	dsc->displaced_insts_num     = 1;

	return 0;
}

static int arm_displace_ldm_pc_full_list_post_handler(struct pt_regs * regs, struct displaced_desc * dsc)
{
	unsigned int orig_inst = dsc->orig_inst;
	int i, j, reg_mask, reg_num, rn, op;
	unsigned int data[16];
	bool wback, inc;
	unsigned int rn_value, address;
	int cond;

	cond = px_get_bits(orig_inst, 28, 31);

	if (!condition_pass(cond, &dsc->orig_regs))
		return 0;

	reg_mask = orig_inst & 0xffff;

	reg_num = px_get_bit_count(reg_mask);
	rn      = px_get_bits(orig_inst, 16, 19);
	op      = px_get_bits(orig_inst, 20, 25);

	wback = (px_get_bit(orig_inst, 21) == 1);

	rn_value = read_reg_value(rn, &dsc->orig_regs, dsc->orig_regs.ARM_pc);

	if ((op == 0x1) || (op == 0x3))
	{
		/* LDMDA/LDMFA */
		address = rn_value - 4 * reg_num + 4;
		inc = false;
	}
	else if ((op == 0x9) || (op == 0xb))
	{
		/* LDM/LDMIA/LDMFD */
		address = rn_value;
		inc = true;
	}
	else  if ((op == 0x11) || (op == 0x13))
	{
		/* LDMDB/LDMEA */
		address = rn_value - 4 * reg_num;
		inc = false;
	}
	else if ((op == 0x19) || (op == 0x1b))
	{
		/* LDMIB/LDMED */
		address = rn_value + 4;
		inc = true;
	}
	else
	{
		return arm_displace_undef(regs, dsc);
	}

	read_proc_vm(current, address, data, reg_num);

	for (i=0, j=0; i<16; i++)
	{
		if ((reg_mask & (1 << i)) != 0)
		{
			/* if R[j] is in the list */
			write_reg_value(j, data[i], regs);

			j++;
		}
	}

	if (wback)
	{
		if (inc)
			write_reg_value(rn, rn_value + 4 * reg_num, &dsc->orig_regs);
		else
			write_reg_value(rn, rn_value - 4 * reg_num, &dsc->orig_regs);
	}

	return -1;
}

static int arm_displace_ldm_pc_full_list(struct pt_regs * regs, struct displaced_desc * dsc)
{
	dsc->pre_handler  = &arm_displace_ldm_pc_full_list_pre_handler;
	dsc->post_handler  = &arm_displace_ldm_pc_full_list_post_handler;

	return 0;
}

static int arm_displace_ldm_pc_non_full_list_pre_handler(struct pt_regs * regs, struct displaced_desc * dsc)
{
	unsigned int mask;
	unsigned int num_in_list;
	unsigned int orig_inst = dsc->orig_inst;
	unsigned int data;
	int i;

	num_in_list = px_get_bit_count(orig_inst & 0xffff);

	/* change the registers in the list */
	mask = 0;

	for (i=0; i<num_in_list; i++)
	{
		mask |= 1 << i;
	}

	data = (orig_inst & 0xffff0000) | mask;

	/* ignore the write back */
	data = data & ~(1 << 21);

	dsc->displaced_insts[0].inst = data;
	dsc->displaced_insts[0].size = ARM_INST_SIZE;
	dsc->displaced_insts_num = 1;

	return -1;
}

static int arm_displace_ldm_pc_non_full_list_post_handler(struct pt_regs * regs, struct displaced_desc * dsc)
{
	unsigned int orig_inst = dsc->orig_inst;
	unsigned int write_back = px_get_bit(orig_inst, 21);
	unsigned int increase   = px_get_bit(orig_inst, 23);
	unsigned int rn         = px_get_bits(orig_inst, 16, 19);

	unsigned int num_in_list;
	int i, n;

	unsigned int value[16];

	num_in_list = px_get_bit_count(orig_inst & 0xffff);

	for (i=0; i<num_in_list; i++)
	{
		value[i] = read_reg_value(i, regs, regs->ARM_pc);
	}

	for (i=0, n=0; i<16; i++)
	{
		if (orig_inst & (1 << i))
		{
			/* if R[i] is in the list */
			write_reg_value(i, value[n], regs);
			n++;
		}
	}

	if (write_back)
	{
		/* simulate the write back */
		unsigned int rn_value;

		rn_value = read_reg_value(rn, regs, regs->ARM_pc);

		if (increase)
		{
			rn_value += num_in_list * 4;
		}
		else
		{
			rn_value -= num_in_list * 4;
		}

		write_reg_value(rn, rn_value, regs);
	}

	return -1;
}

static int arm_displace_ldm_pc_non_full_list(struct pt_regs * regs, struct displaced_desc * dsc)
{
	dsc->pre_handler  = &arm_displace_ldm_pc_non_full_list_pre_handler;
	dsc->post_handler = &arm_displace_ldm_pc_non_full_list_post_handler;

	return 0;
}

static int arm_stm_pre_handler(struct pt_regs * regs, struct displaced_desc * dsc)
{
	dsc->displaced_insts[0].inst = dsc->orig_inst;
	dsc->displaced_insts[0].size = ARM_INST_SIZE;
	dsc->displaced_insts_num     = 1;

	return 0;
}

static int arm_stm_post_handler(struct pt_regs * regs, struct displaced_desc * dsc)
{
	unsigned int orig_inst = dsc->orig_inst;
	bool wback, inc;
	int cond, rn, reg_list, reg_num, op;
	unsigned int rn_value, pc, address;

	cond = px_get_bits(orig_inst, 28, 31);
	rn   = px_get_bits(orig_inst, 16, 19);
	op   = px_get_bits(orig_inst, 20, 25);

	if (!condition_pass(cond, &dsc->orig_regs))
		return 0;

	wback = (px_get_bit(orig_inst, 21) == 1);
	reg_list = px_get_bits(orig_inst, 0, 15);
	reg_num = px_get_bit_count(reg_list);

	rn_value = read_reg_value(rn, regs, regs->ARM_pc);

	if ((op == 0x0) || (op == 0x2))
	{
		/* STMDA/STMED */
		address = rn_value - 4 * reg_num + 4;
		inc = false;
	}
	else if ((op == 0x8) || (op == 0xa))
	{
		/* STM/STMIA/STMEA */
		address = rn_value;
		inc = true;
	}
	else  if ((op == 0x10) || (op == 0x12))
	{
		/* STMDB/STMFD */
		address = rn_value - 4 * reg_num;
		inc = false;
	}
	else if ((op == 0x18) || (op == 0x1a))
	{
		/* STMIB/STMFA */
		address = rn_value + 4;
		inc = true;
	}
	else
	{
		return arm_displace_undef(regs, dsc);
	}

	pc = read_reg_value(15, &dsc->orig_regs, dsc->orig_regs.ARM_pc);

	/* the pc value stored is not correct, we need to store the correct pc value */
	if (inc)
		write_proc_vm(current, rn_value - 4, &pc, 4);
	else
		write_proc_vm(current, rn_value + 4, &pc, 4);

	return 0;
}

static int arm_displace_stm(struct pt_regs * regs, struct displaced_desc * dsc)
{
	unsigned int orig_inst = dsc->orig_inst;

	if (px_get_bit(orig_inst, 15) != 1)
	{
		return arm_displace_unchanged(regs, dsc);
	}

	dsc->pre_handler  = &arm_stm_pre_handler;
	dsc->post_handler = &arm_stm_post_handler;

	return 0;
}

static int arm_displace_ldmstm(struct pt_regs * regs, struct displaced_desc * dsc)
{
	unsigned int orig_inst = dsc->orig_inst;
	int load = px_get_bit(orig_inst, 20);
	int rn = px_get_bits(orig_inst, 16, 19);

	if ((rn != 15) && (px_get_bit(orig_inst, 15) != 1))
		return arm_displace_unchanged(regs, dsc);

	if (load == 1)
	{
		if (px_get_bits(orig_inst, 0, 15) == 0xffff)
		{
			return arm_displace_ldm_pc_full_list(regs, dsc);
		}
		else
		{
			return arm_displace_ldm_pc_non_full_list(regs, dsc);
		}
	}
	else
	{
		return arm_displace_stm(regs, dsc);
	}

	return 0;
}

static int arm_displace_b_bl_ldmstm(struct pt_regs * regs, struct displaced_desc * dsc)
{
	unsigned int orig_inst = dsc->orig_inst;

	if (px_get_bit(orig_inst, 25) == 1)
	{
		if (px_get_bit(orig_inst, 24) == 0)
			return arm_displace_b_immed(regs, dsc);
		else
			return arm_displace_bl_immed(regs, dsc);
	}
	else
	{
		return arm_displace_ldmstm(regs, dsc);
	}
	return 0;
}

static int arm_displace_svc(struct pt_regs * regs, struct displaced_desc * dsc)
{
	return arm_displace_unchanged(regs, dsc);
}

static int arm_ext_reg_load_store_pre_handler(struct pt_regs * regs, struct displaced_desc * dsc)
{
	unsigned int orig_inst = dsc->orig_inst;
	unsigned int pc, data;

	data = px_change_bits(orig_inst, 16, 19, 0);      /* change Rn to R0 */

	pc = read_reg_value(15, regs, regs->ARM_pc);
	write_reg_value(0, pc, regs);

	dsc->displaced_insts[0].inst = data;
	dsc->displaced_insts[0].size = ARM_INST_SIZE;
	dsc->displaced_insts_num     = 1;

	return 0;
}

static int arm_ext_reg_load_store_post_handler(struct pt_regs * regs, struct displaced_desc * dsc)
{
	int rn;
	unsigned int orig_inst = dsc->orig_inst;

	rn = px_get_bits(orig_inst, 16, 19);

	/* set the value of R0 to Rn */
	write_reg_value(rn, regs->ARM_r0, regs);

	/* restore the registers value */
	write_reg_value(0, dsc->orig_regs.ARM_r0, regs);

	return 0;
}


static int arm_displace_ext_reg_load_store(struct pt_regs * regs, struct displaced_desc * dsc)
{
	unsigned int orig_inst = dsc->orig_inst;
	int rn, opcode;

	opcode = px_get_bits(orig_inst, 20, 24);
	rn     = px_get_bits(orig_inst, 16, 19);

	if ((opcode == 0x4) || (opcode == 0x5))
	{
		/* 64-bit transfers between ARM core and extension registers */
		return arm_displace_unchanged(regs, dsc);
	}

	if (rn != 15)
		return arm_displace_unchanged(regs, dsc);

	dsc->pre_handler  = &arm_ext_reg_load_store_pre_handler;
	dsc->post_handler = &arm_ext_reg_load_store_post_handler;

	return 0;
}

static int arm_displace_svc_coproc(struct pt_regs * regs, struct displaced_desc * dsc)
{
	unsigned int orig_inst = dsc->orig_inst;

	int op1 = px_get_bits(orig_inst, 20, 25);
	int op  = px_get_bit(orig_inst, 4);
	int coproc = px_get_bits(orig_inst, 8, 11);
	int rn  = px_get_bits(orig_inst, 16, 19);

	if (((op1 & 0x20) == 0x0) && ((coproc & 0xe) == 0xa))
	{
		/* Extension register load/store instructions */
		return arm_displace_ext_reg_load_store(regs, dsc);
	}

	if (((op1 & 0x21) == 0x0) && ((coproc & 0xe) != 0xa))
	{
		/* STC, STC2 */
		return arm_displace_stc_stc2(regs, dsc);
	}

	if (((op1 & 0x21) == 0x1) && ((coproc & 0xe) != 0xa))
	{
		if (rn != 15)
		{
			/* LDC, LDC2 (immediate) */
			return arm_displace_unchanged(regs, dsc);
		}
		else
		{
			/* LDC, LDC2 (literal) */
			return arm_displace_ldc_ldc2_literal(regs, dsc);
		}
	}

	if (((op1 == 0x4) || (op1 == 0x5)) && ((coproc == 0xa) || (coproc == 0xb)))
	{
		/* 64-bit transfers between ARM core and extension registers */
		return arm_displace_unchanged(regs, dsc);
	}

	if ((op1 == 0x4) && ((coproc & 0xe) != 0xa))
	{
		/* MCRR, MCRR2 */
		return arm_displace_unchanged(regs, dsc);
	}

	if ((op1 == 0x5) && ((coproc & 0xe) != 0xa))
	{
		/* MRRC, MRRC2 */
		return arm_displace_unchanged(regs, dsc);
	}

	if (((op1 & 0x30) == 0x20) && (op == 0) && ((coproc & 0xe) == 0xa))
	{
		/* VFP data-processing instructions */
		return arm_displace_unchanged(regs, dsc);
	}

	if (((op1 & 0x30) == 0x20) && (op == 0) && ((coproc & 0xe) != 0xa))
	{
		/* CDP, CDP2 */
		return arm_displace_unchanged(regs, dsc);
	}

	if (((op1 & 0x30) == 0x20) && (op == 1) && ((coproc & 0xe) == 0xa))
	{
		/* 8, 16, and 32-bit transfer between ARM core and extension registers */
		return arm_displace_unchanged(regs, dsc);
	}

	if (((op1 & 0x31) == 0x20) && (op == 1) && ((coproc & 0xe) != 0xa))
	{
		/* MCR, MCR2 */
		return arm_displace_unchanged(regs, dsc);
	}

	if (((op1 & 0x31) == 0x21) && (op == 1) && ((coproc & 0xe) != 0xa))
	{
		/* MRC, MRC2 */
		return arm_displace_unchanged(regs, dsc);
	}

	if ((op1 & 0x30) == 0x30)
	{
		/* SVC (previously SWI) */
		return arm_displace_svc(regs, dsc);
	}

	return arm_displace_undef(regs, dsc);
}

int arm_displace_inst(struct pt_regs * regs, struct displaced_desc * dsc)
{
	unsigned int orig_inst = dsc->orig_inst;

	if (px_get_bits(orig_inst, 28, 31) != 0xf)
	{
		int op1, op2;

		op1 = px_get_bits(orig_inst, 25, 27);
		op2 = px_get_bit(orig_inst, 4);

		if ((op1 & 0x6) == 0x0)
			return arm_displace_dp_misc(regs, dsc);

		if (op1 == 0x2)
			return arm_displace_ldr_str_ldrb_strb(regs, dsc);

		if (op1 == 0x3)
		{
			if (op2 == 0)
				return arm_displace_ldr_str_ldrb_strb(regs, dsc);
			else
				return arm_displace_media(regs, dsc);
		}

		if ((op1 & 0x6) == 0x4)
			return arm_displace_b_bl_ldmstm(regs, dsc);

		if ((op1 & 0x6) == 0x6)
			return arm_displace_svc_coproc(regs, dsc);
	}
	else
	{
		int op1 = px_get_bits(orig_inst, 20, 27);
		int op = px_get_bit(orig_inst, 4);
		int rn = px_get_bits(orig_inst, 16, 19);

		if ((op1 & 0x80) == 0x0)
			return arm_displace_misc_memhint_advancedsimd(regs, dsc);

		/* SRS */
		if ((op1 & 0xe5) == 0x84)
			return arm_displace_unchanged(regs, dsc);

		/* RFE */
		if ((op1 & 0xe5) == 0x81)
			return arm_displace_unchanged(regs, dsc);

		/* BL, BLX (immediate) */
		if ((op1& 0xe0) == 0xa0)
			return arm_displace_blx_immed(regs, dsc);

		/* LDC, LDC2 (immediate) */
		if (((op1 & 0xfb) == 0xc3) && (rn != 0xf))
			return arm_displace_ldc_ldc2_immed(regs, dsc);

		/* LDC, LDC2 (literal) */
		if (((op1 & 0xf9) == 0xc9) && (rn == 0xf))
			return arm_displace_ldc_ldc2_literal(regs, dsc);

		/* LDC, LDC2 (literal) */
		if (((op1 & 0xf1) == 0xd1) && (rn == 0xf))
			return arm_displace_ldc_ldc2_literal(regs, dsc);

		/* STC, STC2 */
		if (((op1 & 0xfb) == 0xc2) || ((op1 & 0xf9) == 0xc8) || ((op1 & 0xf1) == 0xd0))
			return arm_displace_stc_stc2(regs, dsc);

		/* MCRR/MCRR2 */
		if (op1 == 0xc4)
			return arm_displace_unchanged(regs, dsc);

		/* MRRC/MRRC2 */
		if (op1 == 0xc5)
			return arm_displace_unchanged(regs, dsc);

		/* CDP/CDP2/VFP instructions*/
		if (((op1 & 0xf0) == 0xe0) && (op == 0))
			return arm_displace_unchanged(regs, dsc);

		/* MCR/MCR2/Advanced SIMD and VFP */
		if (((op1 & 0xf1) == 0xe0) && (op == 1))
			return arm_displace_mcr_mcr2(regs, dsc);

		/* MRC/MRC2/Advanced SIMD and VFP */
		if (((op1 & 0xf1) == 0xe1) && (op == 1))
			return arm_displace_mrc_mrc2(regs, dsc);
	}

	return 0;
}

static int thumb16_displace_unchanged(struct pt_regs * regs, struct displaced_desc * dsc)
{
	dsc->displaced_insts[0].inst = dsc->orig_inst;
	dsc->displaced_insts[0].size = THUMB16_INST_SIZE;
	dsc->displaced_insts_num = 1;

	dsc->pre_handler = NULL;
	dsc->post_handler = NULL;

	return 0;
}

static int thumb16_displace_unpredicable(struct pt_regs * regs, struct displaced_desc * dsc)
{
	return thumb16_displace_unchanged(regs, dsc);
}

static int thumb16_displace_undef(struct pt_regs * regs, struct displaced_desc * dsc)
{
	return thumb16_displace_unchanged(regs, dsc);
}

static int thumb16_add_high_reg_pre_handler(struct pt_regs * regs, struct displaced_desc * dsc)
{
	unsigned int orig_inst = dsc->orig_inst;
	u16 data;
	int rdn, rm, rd;
	unsigned int rd_value, rm_value;

	rm  = px_get_bits(orig_inst, 3, 6);
	rdn = px_get_bits(orig_inst, 0, 2);
	rd  = (px_get_bit(orig_inst, 7) << 3) | rdn;

	rd_value = read_reg_value(rd, regs, regs->ARM_pc);
	rm_value = read_reg_value(rm, regs, regs->ARM_pc);

	write_reg_value(8, rd_value, regs);
	write_reg_value(9, rm_value, regs);

	data = orig_inst;
	data = px_change_bits(data, 0, 7, 0xc8);        /* set Rd to R8, Rm to R9 */

	dsc->displaced_insts[0].inst = data;
	dsc->displaced_insts[0].size = THUMB16_INST_SIZE;
	dsc->displaced_insts_num     = 1;

	return 0;
}

static int thumb16_add_high_reg_post_handler(struct pt_regs * regs, struct displaced_desc * dsc)
{
	unsigned int orig_inst = dsc->orig_inst;
	int rdn, rm, rd;

	rm  = px_get_bits(orig_inst, 3, 6);
	rdn = px_get_bits(orig_inst, 0, 2);
	rd  = (px_get_bit(orig_inst, 7) << 3) | rdn;

	/* set the value of R8 to Rd */
	write_reg_value(rd, regs->ARM_r8, regs);

	/* restore the registers value */

	if (rd != 8)
		write_reg_value(8, dsc->orig_regs.ARM_r8, regs);

	if (rd != 9)
		write_reg_value(9, dsc->orig_regs.ARM_r9, regs);

	return 0;
}

static int thumb16_displace_add_high_reg(struct pt_regs * regs, struct displaced_desc * dsc)
{
	unsigned int orig_inst = dsc->orig_inst;
	int rdn, rm, rd;

	rm  = px_get_bits(orig_inst, 3, 6);
	rdn = px_get_bits(orig_inst, 0, 2);
	rd  = (px_get_bit(orig_inst, 7) << 3) | rdn;

	if ((rm != 15) && (rd != 15))
		return thumb16_displace_unchanged(regs, dsc);

	dsc->pre_handler  = &thumb16_add_high_reg_pre_handler;
	dsc->post_handler = &thumb16_add_high_reg_post_handler;

	return 0;
}

static int thumb16_displace_cmp_high_reg(struct pt_regs * regs, struct displaced_desc * dsc)
{
	return thumb16_displace_unchanged(regs, dsc);
}

static int thumb16_mov_high_reg_pre_handler(struct pt_regs * regs, struct displaced_desc * dsc)
{
	unsigned int orig_inst = dsc->orig_inst;
	u16 data;
	int rdn, rm, rd;
	unsigned int rd_value, rm_value;

	rm  = px_get_bits(orig_inst, 3, 6);
	rdn = px_get_bits(orig_inst, 0, 2);
	rd  = (px_get_bit(orig_inst, 7) << 3) | rdn;

	rd_value = read_reg_value(rd, regs, regs->ARM_pc);
	rm_value = read_reg_value(rm, regs, regs->ARM_pc);

	write_reg_value(8, rd_value, regs);
	write_reg_value(9, rm_value, regs);

	data = orig_inst;
	data = px_change_bits(data, 0, 7, 0xc8);        /* set Rd to R8, Rm to R9 */

	dsc->displaced_insts[0].inst = data;
	dsc->displaced_insts[0].size = THUMB16_INST_SIZE;
	dsc->displaced_insts_num     = 1;

	return 0;
}

static int thumb16_mov_high_reg_post_handler(struct pt_regs * regs, struct displaced_desc * dsc)
{
	unsigned int orig_inst = dsc->orig_inst;
	int rdn, rm, rd;

	rm  = px_get_bits(orig_inst, 3, 6);
	rdn = px_get_bits(orig_inst, 0, 2);
	rd  = (px_get_bit(orig_inst, 7) << 3) | rdn;

	/* set the value of R8 to Rd */
	write_reg_value(rd, regs->ARM_r8, regs);

	/* restore the registers value */
	if (rd != 8)
		write_reg_value(8, dsc->orig_regs.ARM_r8, regs);

	if (rd != 9)
	write_reg_value(9, dsc->orig_regs.ARM_r9, regs);

	return 0;
}

static int thumb16_displace_mov_high_reg(struct pt_regs * regs, struct displaced_desc * dsc)
{
	unsigned int orig_inst = dsc->orig_inst;
	int rdn, rm, rd;

	rm  = px_get_bits(orig_inst, 3, 6);
	rdn = px_get_bits(orig_inst, 0, 2);
	rd  = (px_get_bit(orig_inst, 7) << 3) | rdn;

	if ((rm != 15) && (rd != 15))
		return thumb16_displace_unchanged(regs, dsc);

	dsc->pre_handler  = &thumb16_mov_high_reg_pre_handler;
	dsc->post_handler = &thumb16_mov_high_reg_post_handler;

	return 0;
}

static int thumb16_bx_reg_pre_handler(struct pt_regs * regs, struct displaced_desc * dsc)
{
	dsc->displaced_insts[0].inst = THUMB16_NOP_INST;
	dsc->displaced_insts[0].size = THUMB16_INST_SIZE;
	dsc->displaced_insts_num = 1;

	return 0;
}

static int thumb16_bx_reg_post_handler(struct pt_regs * regs, struct displaced_desc * dsc)
{
	int rm;
	unsigned int orig_inst;
	unsigned int rm_value;

	orig_inst = dsc->orig_inst;

	rm = px_get_bits(orig_inst, 3, 6);

	rm_value = read_reg_value(rm, &dsc->orig_regs, dsc->orig_regs.ARM_pc);
/*
	if (rm_value & 0x1)
	{
		regs->ARM_cpsr |= FLAG_T;
	}
	else
	{
		regs->ARM_cpsr &= ~FLAG_T;
	}
*/
	//write_reg_value(15, rm_value & ~0x1, regs);
	bx_write_pc(rm_value, regs);

	return -1;
}

static int thumb16_displace_bx(struct pt_regs * regs, struct displaced_desc * dsc)
{
	dsc->pre_handler  = &thumb16_bx_reg_pre_handler;
	dsc->post_handler = &thumb16_bx_reg_post_handler;
	return 0;
}

static int thumb16_blx_reg_pre_handler(struct pt_regs * regs, struct displaced_desc * dsc)
{
	dsc->displaced_insts[0].inst = THUMB16_NOP_INST;
	dsc->displaced_insts[0].size = THUMB16_INST_SIZE;
	dsc->displaced_insts_num = 1;

	return 0;
}

static int thumb16_blx_reg_post_handler(struct pt_regs * regs, struct displaced_desc * dsc)
{
	int rm;
	unsigned int orig_inst;
	unsigned int rm_value;
	unsigned int pc;

	orig_inst = dsc->orig_inst;

	pc = dsc->orig_regs.ARM_pc + 4;
	rm = px_get_bits(orig_inst, 3, 6);

	rm_value = read_reg_value(rm, &dsc->orig_regs, dsc->orig_regs.ARM_pc);
/*
	if (rm_value & 0x1)
	{
		regs->ARM_cpsr |= FLAG_T;
	}
	else
	{
		regs->ARM_cpsr &= ~FLAG_T;
	}
*/
	write_reg_value(14, (pc - 2) | 0x1, regs);
	//write_reg_value(15, rm_value, regs);
	bx_write_pc(rm_value, regs);

	return -1;
}

static int thumb16_displace_blx_reg(struct pt_regs * regs, struct displaced_desc * dsc)
{
	dsc->pre_handler  = &thumb16_blx_reg_pre_handler;
	dsc->post_handler = &thumb16_blx_reg_post_handler;
	return 0;
}

static int thumb16_displace_special_dp_bx_blx(struct pt_regs * regs, struct displaced_desc * dsc)
{
	unsigned int orig_inst;
	int opcode;

	orig_inst = dsc->orig_inst;
	opcode = px_get_bits(orig_inst, 6, 9);

	switch (opcode)
	{
	case 0x0:
		/* Add Low Registers */
		return thumb16_displace_unchanged(regs, dsc);

	case 0x1:
	case 0x2:
	case 0x3:
		/* Add High Registers */
		return thumb16_displace_add_high_reg(regs, dsc);

	case 0x4:
		return thumb16_displace_unpredicable(regs, dsc);

	case 0x5:
	case 0x6:
	case 0x7:
		/* Compare High Registers */
		return thumb16_displace_cmp_high_reg(regs, dsc);

	case 0x8:
		/* Move Low Registers */
		return thumb16_displace_unchanged(regs, dsc);

	case 0x9:
	case 0xa:
	case 0xb:
		/* Move High Registers */
		return thumb16_displace_mov_high_reg(regs, dsc);

	case 0xc:
	case 0xd:
		/* BX */
		return thumb16_displace_bx(regs, dsc);

	case 0xe:
	case 0xf:
		/* BLX (register) */
		return thumb16_displace_blx_reg(regs, dsc);
	}

	return 0;
}

static int thumb16_ldr_literal_pre_handler(struct pt_regs * regs, struct displaced_desc * dsc)
{
	dsc->displaced_insts[0].inst = THUMB16_NOP_INST;
	dsc->displaced_insts[0].size = THUMB16_INST_SIZE;
	dsc->displaced_insts_num     = 1;

	return 0;
}

static int thumb16_ldr_literal_post_handler(struct pt_regs * regs, struct displaced_desc * dsc)
{
	unsigned int orig_inst = dsc->orig_inst;
	int rt;
	unsigned int imm8, imm32, pc, data;

	rt   = px_get_bits(orig_inst, 8, 10);
	imm8 = px_get_bits(orig_inst, 0, 7);

	imm32 = imm8 << 2;

	pc = read_reg_value(15, &dsc->orig_regs, dsc->orig_regs.ARM_pc);

	read_proc_vm(current, (pc & ~0x3) + imm32, &data, 4);

	write_reg_value(rt, data, regs);

	return 0;
}

static int thumb16_displace_ldr_literal(struct pt_regs * regs, struct displaced_desc * dsc)
{
	dsc->pre_handler  = &thumb16_ldr_literal_pre_handler;
	dsc->post_handler  = &thumb16_ldr_literal_post_handler;

	return 0;
}


static int thumb16_cbnz_cbz_pre_handler(struct pt_regs * regs, struct displaced_desc * dsc)
{
	dsc->displaced_insts[0].inst = THUMB16_NOP_INST;
	dsc->displaced_insts[0].size = THUMB16_INST_SIZE;
	dsc->displaced_insts_num = 1;

	return 0;
}

static int thumb16_cbnz_cbz_post_handler(struct pt_regs * regs, struct displaced_desc * dsc)
{
	unsigned int imm5, imm32;
	int op;
	int i;
	int rn;
	bool nonzero;
	unsigned int rn_value;
	unsigned int pc;

	unsigned int orig_inst = dsc->orig_inst;

	pc = dsc->orig_regs.ARM_pc + 4;

	i    = px_get_bit(orig_inst, 9);
	op   = px_get_bit(orig_inst, 11);
	rn   = px_get_bits(orig_inst, 0, 2);
	imm5 = px_get_bits(orig_inst, 3, 7);

	rn_value = read_reg_value(rn, &dsc->orig_regs, dsc->orig_regs.ARM_pc);

	//imm32 = sign_extend((i << 6) + (imm5 << 1), 6);
	imm32 = zero_extend((i << 6) + (imm5 << 1), 6);
	nonzero = (op == 1);

	if (nonzero ^ (rn_value == 0))
	{
		//write_reg_value(15, pc + imm32, regs);
		branch_write_pc(pc + imm32, regs);
		return -1;
	}
	else
	{
		return 0;
	}

}

static int thumb16_displace_cbnz_cbz(struct pt_regs * regs, struct displaced_desc * dsc)
{
	dsc->pre_handler  = &thumb16_cbnz_cbz_pre_handler;
	dsc->post_handler = &thumb16_cbnz_cbz_post_handler;

	return 0;
}

static int thumb16_pop_pre_handler(struct pt_regs * regs, struct displaced_desc * dsc)
{
	dsc->displaced_insts[0].inst = THUMB16_NOP_INST;
	dsc->displaced_insts[0].size = THUMB16_INST_SIZE;
	dsc->displaced_insts_num     = 1;

	return 0;
}

static int thumb16_pop_post_handler(struct pt_regs * regs, struct displaced_desc * dsc)
{
	unsigned int data;
	int i;

	/* simulate the instruction */
	for (i=0; i<=7; i++)
	{
		if (px_get_bit(dsc->orig_inst, i) == 1)
		{
			read_proc_vm(current, regs->ARM_sp, &data, 4);
			write_reg_value(i, data, regs);
			regs->ARM_sp += 4;
		}
	}

	read_proc_vm(current, regs->ARM_sp, &data, 4);
	load_write_pc(data, regs);
	regs->ARM_sp += 4;

	return -1;
}


static int thumb16_displace_pop(struct pt_regs * regs, struct displaced_desc * dsc)
{
	unsigned int orig_inst = dsc->orig_inst;

	if (px_get_bit(orig_inst, 8) == 0)
	{
		/* pc is not in the list */
		return thumb16_displace_unchanged(regs, dsc);
	}

	dsc->pre_handler  = &thumb16_pop_pre_handler;
	dsc->post_handler = &thumb16_pop_post_handler;

	return 0;
}

static int thumb16_displace_misc(struct pt_regs * regs, struct displaced_desc * dsc)
{
	unsigned int orig_inst = dsc->orig_inst;
	int opcode = px_get_bits(orig_inst, 5, 11);

	if (opcode == 0x32)
	{
		/* SETEND */
		return thumb16_displace_unchanged(regs, dsc);
	}
	else if (opcode == 0x33)
	{
		/* CPS */
		return thumb16_displace_unchanged(regs, dsc);
	}
	else if ((opcode & 0x7c) == 0x0)
	{
		/* ADD (SP plus immediate */
		return thumb16_displace_unchanged(regs, dsc);
	}
	else if ((opcode & 0x7c) == 0x4)
	{
		/* SUB (SP minus immediate */
		return thumb16_displace_unchanged(regs, dsc);
	}
	else if ((opcode & 0x78) == 0x8)
	{
		/* CBNZ, CBZ */
		return thumb16_displace_cbnz_cbz(regs, dsc);
	}
	else if ((opcode == 0x10) || (opcode == 0x11))
	{
		/* SXTH */
		return thumb16_displace_unchanged(regs, dsc);
	}
	else if ((opcode == 0x12) || (opcode == 0x13))
	{
		/* SXTB */
		return thumb16_displace_unchanged(regs, dsc);
	}
	else if ((opcode == 0x14) || (opcode == 0x15))
	{
		/* UXTH */
		return thumb16_displace_unchanged(regs, dsc);
	}
	else if ((opcode == 0x16) || (opcode == 0x17))
	{
		/* UXTB */
		return thumb16_displace_unchanged(regs, dsc);
	}
	else if ((opcode & 0x78) == 0x18)
	{
		/* CBNZ, CBZ */
		return thumb16_displace_cbnz_cbz(regs, dsc);
	}
	else if ((opcode & 0x70) == 0x20)
	{
		/* PUSH */
		return thumb16_displace_unchanged(regs, dsc);
	}
	else if ((opcode & 0x78) == 0x48)
	{
		/* CBNZ, CBZ */
		return thumb16_displace_cbnz_cbz(regs, dsc);
	}
	else if ((opcode == 0x50) || (opcode == 0x51))
	{
		/* REV */
		return thumb16_displace_unchanged(regs, dsc);
	}
	else if ((opcode == 0x52) || (opcode == 0x53))
	{
		/* REV16 */
		return thumb16_displace_unchanged(regs, dsc);
	}
	else if ((opcode == 0x56) || (opcode == 0x57))
	{
		/* REVSH */
		return thumb16_displace_unchanged(regs, dsc);
	}
	else if ((opcode & 0x78) == 0x58)
	{
		/* CBNZ, CBZ */
		return thumb16_displace_cbnz_cbz(regs, dsc);
	}
	else if ((opcode & 0x70) == 0x60)
	{
		/* POP */
		return thumb16_displace_pop(regs, dsc);
	}
	else if ((opcode & 0x78) == 0x70)
	{
		/* BKPT */
		return thumb16_displace_unchanged(regs, dsc);
	}
	else if ((opcode & 0x78) == 0x78)
	{
		/* If-Then, and hints */
		return thumb16_displace_unchanged(regs, dsc);
	}

	return thumb16_displace_undef(regs, dsc);
}

static int thumb16_adr_pre_handler(struct pt_regs * regs, struct displaced_desc * dsc)
{
	dsc->displaced_insts[0].inst = THUMB16_NOP_INST;
	dsc->displaced_insts[0].size = THUMB16_INST_SIZE;
	dsc->displaced_insts_num = 1;

	return 0;
}

static int thumb16_adr_post_handler(struct pt_regs * regs, struct displaced_desc * dsc)
{
	int rd;
	unsigned int orig_inst;
	unsigned int imm8, imm32;
	unsigned int pc;

	orig_inst = dsc->orig_inst;

	pc = dsc->orig_regs.ARM_pc + 4;

	/* align pc to multiple of 4 */
	pc = pc & ~0x3;

	rd   = px_get_bits(orig_inst, 8, 10);
	imm8 = px_get_bits(orig_inst, 0, 7);

	//imm32 = sign_extend(imm8 << 2, 9);
	imm32 = zero_extend(imm8 << 2, 9);

	write_reg_value(rd, pc + imm32, regs);

	return 0;
}

static int thumb16_displace_adr(struct pt_regs * regs, struct displaced_desc * dsc)
{
	dsc->pre_handler  = &thumb16_adr_pre_handler;
	dsc->post_handler = &thumb16_adr_post_handler;

	return 0;
}

static int thumb16_conditional_branch_pre_handler(struct pt_regs * regs, struct displaced_desc * dsc)
{
	dsc->displaced_insts[0].inst = THUMB16_NOP_INST;
	dsc->displaced_insts[0].size = THUMB16_INST_SIZE;
	dsc->displaced_insts_num     = 1;

	return 0;
}

static int thumb16_conditional_branch_post_handler(struct pt_regs * regs, struct displaced_desc * dsc)
{
	unsigned int orig_inst = dsc->orig_inst;
	int cond;
	unsigned int imm8, imm32, pc;

	cond = px_get_bits(orig_inst, 8, 11);
	imm8 = px_get_bits(orig_inst, 0, 7);
	imm32 = sign_extend(imm8 << 1, 8);

	if (condition_pass(cond, regs))
	{
		pc = read_reg_value(15, &dsc->orig_regs, dsc->orig_regs.ARM_pc);
		//write_reg_value(15, pc + imm32, regs);
		branch_write_pc(pc + imm32, regs);
		return -1;
	}
	else
	{
		return 0;
	}
}

static int thumb16_displace_conditional_branch(struct pt_regs * regs, struct displaced_desc * dsc)
{
	dsc->pre_handler  = &thumb16_conditional_branch_pre_handler;
	dsc->post_handler = &thumb16_conditional_branch_post_handler;

	return 0;
}

static int thumb16_displace_svc(struct pt_regs * regs, struct displaced_desc * dsc)
{
	return thumb16_displace_unchanged(regs, dsc);
}

static int thumb16_displace_conditional_branch_svc(struct pt_regs * regs, struct displaced_desc * dsc)
{
	unsigned int orig_inst = dsc->orig_inst;
	int opcode;

	opcode = px_get_bits(orig_inst, 8, 11);

	switch (opcode)
	{
	case 0xf:
		return thumb16_displace_svc(regs, dsc);

	case 0xe:
		return thumb16_displace_undef(regs, dsc);

	default:
		return thumb16_displace_conditional_branch(regs, dsc);
	}

	return 0;
}

static int thumb16_unconditional_branch_pre_handler(struct pt_regs * regs, struct displaced_desc * dsc)
{
	dsc->displaced_insts[0].inst = THUMB16_NOP_INST;
	dsc->displaced_insts[0].size = THUMB16_INST_SIZE;
	dsc->displaced_insts_num = 1;

	return 0;
}

static int thumb16_unconditional_branch_post_handler(struct pt_regs * regs, struct displaced_desc * dsc)
{
	unsigned int orig_inst;
	unsigned int imm11, imm32;
	unsigned int pc;

	orig_inst = dsc->orig_inst;

	imm11 = px_get_bits(orig_inst, 0, 10);

	//pc = dsc->orig_regs.ARM_pc + 4;
	pc = read_reg_value(15, &dsc->orig_regs, dsc->orig_regs.ARM_pc);

	imm32 = sign_extend(imm11 << 1, 11);

	//write_reg_value(15, pc + imm32, regs);
	branch_write_pc(pc + imm32, regs);

	return -1;
}

static int thumb16_displace_unconditional_branch(struct pt_regs * regs, struct displaced_desc * dsc)
{
	dsc->pre_handler  = &thumb16_unconditional_branch_pre_handler;
	dsc->post_handler = &thumb16_unconditional_branch_post_handler;

	return 0;
}

static int thumb16_displace_inst(struct pt_regs * regs, struct displaced_desc * dsc)
{
	int opcode;
	u16 orig_inst = dsc->orig_inst;

	opcode = px_get_bits(orig_inst, 10, 15);

	if ((opcode & 0x30) == 0x0)
	{
		/* Shift (immediate), add, subtract, move, and compare */
		return thumb16_displace_unchanged(regs, dsc);
	}
	else if (opcode == 0x10)
	{
		/* Data-processing */
		return thumb16_displace_unchanged(regs, dsc);
	}
	else if (opcode == 0x11)
	{
		/* Special data instructions and branch and exchange */
		return thumb16_displace_special_dp_bx_blx(regs, dsc);
	}
	else if ((opcode == 0x12) || (opcode == 0x13))
	{
		/* Load from Literal Pool, see LDR (literal) */
		return thumb16_displace_ldr_literal(regs, dsc);
	}
	else if (((opcode & 0x3c) == 0x14) || ((opcode & 0x38) == 0x18) || ((opcode & 0x38) == 0x20))
	{
		/* Load/store single data item */
		return thumb16_displace_unchanged(regs, dsc);
	}
	else if ((opcode == 0x28) || (opcode == 0x29))
	{
		/* Generate PC-relative address, see ADR */
		return thumb16_displace_adr(regs, dsc);
	}
	else if ((opcode == 0x2a) || (opcode == 0x2b))
	{
		/* Generate SP-relative address, see ADD (SP plus immediate) */
		return thumb16_displace_unchanged(regs, dsc);
	}
	else if ((opcode & 0x3c) == 0x2c)
	{
		/* Miscellaneous 16-bit instructions */
		return thumb16_displace_misc(regs, dsc);
	}
	else if ((opcode == 0x30) || (opcode == 0x31))
	{
		/* Store multiple registers, see STM / STMIA / STMEA */
		return thumb16_displace_unchanged(regs, dsc);
	}
	else if ((opcode == 0x32) || (opcode == 0x33))
	{
		/* Load multiple registers, see LDM / LDMIA / LDMFD */
		return thumb16_displace_unchanged(regs, dsc);
	}
	else if ((opcode & 0x3c) == 0x34)
	{
		/* Conditional branch, and Supervisor Call */
		return thumb16_displace_conditional_branch_svc(regs, dsc);
	}
	else if ((opcode == 0x38) || (opcode == 0x39))
	{
		/* Unconditional Branch, see B */
		return thumb16_displace_unconditional_branch(regs, dsc);
	}

	return thumb16_displace_unchanged(regs, dsc);
}

static int thumb32_displace_unchanged(struct pt_regs * regs, struct displaced_desc * dsc)
{
	dsc->displaced_insts[0].inst = dsc->orig_inst;
	dsc->displaced_insts[0].size = THUMB32_INST_SIZE;
	dsc->displaced_insts_num = 1;

	dsc->pre_handler = NULL;
	dsc->post_handler = NULL;

	return 0;
}

static int thumb32_displace_undef(struct pt_regs * regs, struct displaced_desc * dsc)
{
	return thumb32_displace_unchanged(regs, dsc);
}

static int thumb32_displace_unpredicable(struct pt_regs * regs, struct displaced_desc * dsc)
{
	return thumb32_displace_unchanged(regs, dsc);
}

static int thumb32_adr_pre_handler(struct pt_regs * regs, struct displaced_desc * dsc)
{
	return 0;
}

static int thumb32_adr_post_handler(struct pt_regs * regs, struct displaced_desc * dsc)
{
	unsigned int orig_inst = dsc->orig_inst;
	u16 inst1, inst2;
	int i, d;
	unsigned int imm3, imm8, imm32, pc, result;

	split_thumb32_inst(orig_inst, &inst1, &inst2);

	//pc = dsc->orig_regs.ARM_pc + 4;
	pc = read_reg_value(15, &dsc->orig_regs, dsc->orig_regs.ARM_pc);

	i    = px_get_bit(inst1, 10);
	d    = px_get_bits(inst2, 8, 11);
	imm3 = px_get_bits(inst2, 12, 14);
	imm8 = px_get_bits(inst2, 0, 7);

	imm32 = (i << 11) + (imm3 << 8) + imm8;

	if (px_get_bits(inst1, 5, 9) == 0x10)
		result = (pc & ~0x3) + imm32;
	else
		result = (pc & ~0x3) - imm32;

	write_reg_value(d, result, regs);

	return 0;
}

static int thumb32_displace_adr(struct pt_regs * regs, struct displaced_desc * dsc)
{
	unsigned int orig_inst = dsc->orig_inst;
	u16 inst1, inst2;
	int d;

	split_thumb32_inst(orig_inst, &inst1, &inst2);

	d = px_get_bits(inst2, 8, 11);

	if ((d == 15) || (d == 13))
		return thumb32_displace_unpredicable(regs, dsc);

	dsc->pre_handler  = &thumb32_adr_pre_handler;
	dsc->post_handler = &thumb32_adr_post_handler;

	return 0;
}

static int thumb32_displace_dp_plan_binary_immed(struct pt_regs * regs, struct displaced_desc * dsc)
{
	int op;
	int rn;
	unsigned int orig_inst = dsc->orig_inst;
	u16 inst1, inst2;

	split_thumb32_inst(orig_inst, &inst1, &inst2);

	op = px_get_bits(inst1, 4, 8);
	rn = px_get_bits(inst1, 0, 3);

	if ((op == 0x0) && (rn == 15))
	{
		return thumb32_displace_adr(regs, dsc);
	}
	else if ((op == 0xa) && (rn == 15))
	{
		return thumb32_displace_adr(regs, dsc);
	}
	else
	{
		return thumb32_displace_unchanged(regs, dsc);
	}

}

static int thumb32_ldc_ldc2_literal_pre_handler(struct pt_regs * regs, struct displaced_desc * dsc)
{
	unsigned int orig_inst = dsc->orig_inst;
	u16 inst1, inst2;
	unsigned int pc, data;

	split_thumb32_inst(orig_inst, &inst1, &inst2);

	//pc = regs->ARM_pc + 4;
	pc = read_reg_value(15, regs, regs->ARM_pc);

	write_reg_value(0, pc, regs);

	data = inst1;
	data = px_change_bits(data, 0, 3, 0);        /* change PC to R0 */

	dsc->displaced_insts[0].inst = make_thumb32_inst(data, inst2);
	dsc->displaced_insts[0].size = THUMB32_INST_SIZE;
	dsc->displaced_insts_num     = 1;

	return 0;
}

static int thumb32_ldc_ldc2_literal_post_handler(struct pt_regs * regs, struct displaced_desc * dsc)
{
	/* restore the registers value */
	write_reg_value(0, regs->ARM_r0, regs);

	return 0;
}

static int thumb32_displace_ldc_ldc2_literal(struct pt_regs * regs, struct displaced_desc * dsc)
{
	unsigned int orig_inst = dsc->orig_inst;
	int rn;
	u16 inst1, inst2;

	split_thumb32_inst(orig_inst, &inst1, &inst2);

	rn = px_get_bits(inst1, 0, 3);

	if (rn != 15)
		return thumb32_displace_unchanged(regs, dsc);

	dsc->pre_handler  = &thumb32_ldc_ldc2_literal_pre_handler;
	dsc->post_handler = &thumb32_ldc_ldc2_literal_post_handler;

	return 0;
}

static int thumb32_displace_vldr_pre_handler(struct pt_regs * regs, struct displaced_desc * dsc)
{
	unsigned int data;
	unsigned int orig_inst = dsc->orig_inst;
	u16 inst1, inst2;
	unsigned int rn_value;
	int rn;

	split_thumb32_inst(orig_inst, &inst1, &inst2);

	rn = px_get_bits(inst1, 0, 3);

	rn_value = read_reg_value(rn, regs, regs->ARM_pc);
	write_reg_value(0, rn_value, regs);

	data = inst1;
	data = px_change_bits(inst1, 0, 3, 0);       /* change Rn to R0 */

	dsc->displaced_insts[0].inst = make_thumb32_inst(data, inst2);
	dsc->displaced_insts[0].size = THUMB32_INST_SIZE;
	dsc->displaced_insts_num     = 1;

	return 0;
}

static int thumb32_displace_vldr_post_handler(struct pt_regs * regs, struct displaced_desc * dsc)
{
	/* restore the register value */
	write_reg_value(0, dsc->orig_regs.ARM_r0, regs);

	return 0;
}

static int thumb32_displace_vldr(struct pt_regs * regs, struct displaced_desc * dsc)
{
	unsigned int orig_inst = dsc->orig_inst;
	u16 inst1, inst2;
	int rn;

	split_thumb32_inst(orig_inst, &inst1, &inst2);
	rn = px_get_bits(inst1, 0, 3);

	if (rn != 15)
		return thumb32_displace_unchanged(regs, dsc);

	dsc->pre_handler  = &thumb32_displace_vldr_pre_handler;
	dsc->post_handler  = &thumb32_displace_vldr_post_handler;

	return 0;
}


static int thumb32_displace_ext_reg_load_str(struct pt_regs * regs, struct displaced_desc * dsc)
{
	unsigned int orig_inst = dsc->orig_inst;
	u16 inst1, inst2;
	int opcode;

	split_thumb32_inst(orig_inst, &inst1, &inst2);

	opcode = px_get_bits(inst1, 4, 8);

	if ((opcode & 0x13) == 0x11)
	{
		/* VLDR */
		return thumb32_displace_vldr(regs, dsc);
	}

	return thumb32_displace_unchanged(regs, dsc);
}

static int thumb32_displace_coprocessor(struct pt_regs * regs, struct displaced_desc * dsc)
{
	unsigned int orig_inst;
	int op1, op, rn, coproc;
	u16 inst1, inst2;

	orig_inst = dsc->orig_inst;

	split_thumb32_inst(orig_inst, &inst1, &inst2);

	op1    = px_get_bits(inst1, 4, 9);
	rn     = px_get_bits(inst1, 0, 3);
	coproc = px_get_bits(inst2, 8, 11);
	op     = px_get_bit(inst2, 4);

	if ((op1 & 0x30) == 0x30)
	{
		/* Advanced SIMD data-processing instructions */
		return thumb32_displace_unchanged(regs, dsc);
	}

	if ((op1 == 0x0) || (op1 == 0x1))
	{
		/* UNDEFINED */
		return thumb32_displace_undef(regs, dsc);
	}

	if ((coproc == 0xa) || (coproc == 0xb)) /* coproc is 101x */
	{
		if (((op1 & 0x3a) == 0x2) || ((op1 & 0x38) == 0x8) || ((op1 & 0x30) == 0x10))
		{
			/* Extension register load/store instructions */
			return thumb32_displace_ext_reg_load_str(regs, dsc);
		}

		if ((op1 == 0x4) || (op1 == 0x5))
		{
			/* 64-bit transfers between ARM core and extension registers */
			return thumb32_displace_unchanged(regs, dsc);
		}

		if ((op == 0) && ((op1 & 0x30) == 0x20))
		{
			/* VFP data-processing instructions */
			return thumb32_displace_unchanged(regs, dsc);
		}

		if (((op1 & 0x30) == 0x20) && (op == 1))
		{
			/* 8, 16, and 32-bit transfer between ARM core and extension registers */
			return thumb32_displace_unchanged(regs, dsc);
		}
	}
	else      /* coproc not 101x */
	{
		if (((op1 & 0x3b) == 0x2) || ((op1 & 0x39) == 0x8) || ((op1 & 0x31) == 0x10))
		{
			/* STC, STC2 */
			return thumb32_displace_unchanged(regs, dsc);
		}

		if (((op1 & 0x3b) == 0x3) || ((op1 & 0x39) == 0x9) || ((op1 & 0x31) == 0x11))
		{
			if (rn != 15)
			{
				/* LDC, LDC2 (immediate) */
				return thumb32_displace_unchanged(regs, dsc);
			}
			else
			{
				/* LDC, LDC2 (literal) */
				return thumb32_displace_ldc_ldc2_literal(regs, dsc);
			}
		}

		if (op1 == 0x4)
		{
			/* MCRR, MCRR2 */
			return thumb32_displace_unchanged(regs, dsc);
		}

		if (op1 == 0x5)
		{
			/* MRRC, MRRC2 */
			return thumb32_displace_unchanged(regs, dsc);
		}

		if ((op1 & 0x30) == 0x20)
		{
			/* CDP, CDP2 */
			return thumb32_displace_unchanged(regs, dsc);
		}

		if (((op1 & 0x31) == 0x20) && (op == 1))
		{
			/* MCR, MCR2 */
			return thumb32_displace_unchanged(regs, dsc);
		}

		if (((op1 & 0x31) == 0x21) && (op == 1))
		{
			/* MRC, MRC2 */
			return thumb32_displace_unchanged(regs, dsc);
		}
	}

	return thumb32_displace_undef(regs, dsc);
}

static int thumb32_ldm_pop_pre_handler(struct pt_regs * regs, struct displaced_desc * dsc)
{
	/* if pc is in the list, lr should not be in the list, otherwise it is unpredicable */
	dsc->displaced_insts[0].inst = THUMB32_NOP_INST;
	dsc->displaced_insts[0].size = THUMB32_INST_SIZE;
	dsc->displaced_insts_num     = 1;

	return 0;
}

static int thumb32_ldm_pop_post_handler(struct pt_regs * regs, struct displaced_desc * dsc)
{
	unsigned int orig_inst;
	u16 inst1, inst2;
	int rn, op, reg_num;
	unsigned int data, rn_value, address = 0;
	bool wback;
	int i;
	int ret;

	orig_inst = dsc->orig_inst;

	split_thumb32_inst(orig_inst, &inst1, &inst2);

	rn    = px_get_bits(inst1, 0, 3);
	wback = (px_get_bit(inst1, 5) == 1);
	op    = px_get_bits(inst1, 7, 8);
	reg_num = px_get_bit_count(inst2);

	rn_value = read_reg_value(rn, &dsc->orig_regs, dsc->orig_regs.ARM_pc);

	if ((op != 0x1) && (op != 0x2))
		return 0;

	if (op == 0x1)
		address = rn_value;

	if (op == 0x2)
		address = rn_value - 4 * reg_num;

	/* we simulate the instruction */
	for (i=0; i<14; i++)
	{
		if (inst2 & (0x1 << i))
		{
			/* R[i] is in the list */
			read_proc_vm(current, address, &data, 4);
			write_reg_value(i, data, regs);

			address += 4;
		}
	}

	if (px_get_bit(inst2, 15) == 1)
	{
		read_proc_vm(current, address, &data, 4);

		//write_reg_value(15, data, regs);
		load_write_pc(data, regs);

		ret = -1;
	}
	else
	{
		ret = 0;
	}

	if (wback)
	{
		if (op == 0x1)
			write_reg_value(rn, rn_value + 4 * reg_num, regs);

		if (op == 0x2)
			write_reg_value(rn, rn_value - 4 * reg_num, regs);
	}

	return ret;
}

static int thumb32_displace_ldm_pop(struct pt_regs * regs, struct displaced_desc * dsc)
{
	unsigned int orig_inst;
	u16 inst1, inst2;

	orig_inst = dsc->orig_inst;

	split_thumb32_inst(orig_inst, &inst1, &inst2);

	/* if pc is not in the list */
	if (px_get_bit(inst2, 15) != 1)
	{
		return thumb32_displace_unchanged(regs, dsc);
	}
	else
	{
		if (px_get_bit(inst2, 14) == 1)
		{
			/* if both pc and lr is in the list */
			return thumb32_displace_unpredicable(regs, dsc);
		}
		else
		{
			dsc->pre_handler  = &thumb32_ldm_pop_pre_handler;
			dsc->post_handler = &thumb32_ldm_pop_post_handler;
		}
	}

	return 0;
}

static int thumb32_displace_load_store_multiple(struct pt_regs * regs, struct displaced_desc * dsc)
{
	unsigned int orig_inst = dsc->orig_inst;
	u16 inst1, inst2;
	int op, load;

	split_thumb32_inst(orig_inst, &inst1, &inst2);

	op   = px_get_bits(inst1, 7, 8);
	load = px_get_bit(inst1, 4);

	if ((op == 0x0) || (op == 0x3))
	{
		/* SRS or RFE */
		return thumb32_displace_unchanged(regs, dsc);
	}

	if (load == 0)
	{
		/* it is a STM instrution, where pc is not allowd */
		return thumb32_displace_unchanged(regs, dsc);
	}

	return thumb32_displace_ldm_pop(regs, dsc);
}

static int thumb32_ldrd_literal_pre_handler(struct pt_regs * regs, struct displaced_desc * dsc)
{
	int rn, rt, rt2;
	unsigned int rn_value, rt_value, rt2_value;
	unsigned int data1, data2;
	unsigned int orig_inst;
	u16 inst1, inst2;

	orig_inst = dsc->orig_inst;
	split_thumb32_inst(orig_inst, &inst1, &inst2);

	rn = px_get_bits(inst1, 0, 3);
	rt = px_get_bits(inst2, 12, 15);
	rt2 = px_get_bits(inst2, 8, 11);

	rn_value  = read_reg_value(rn, regs, regs->ARM_pc);
	rt_value  = read_reg_value(rt, regs, regs->ARM_pc);
	rt2_value = read_reg_value(rt2, regs, regs->ARM_pc);

	write_reg_value(0, rt_value, regs);
	write_reg_value(1, rt2_value, regs);
	write_reg_value(2, rn_value & ~0x3, regs);    /* make rn align to 4 */

	data1 = inst1;
	data1 = px_change_bits(data1, 0, 3, 2);      /* set Rn to R2 */

	data2 = inst2;
	data2 = px_change_bits(data2, 12, 15, 0);    /* set Rt to R0 */
	data2 = px_change_bits(data2, 8, 11, 1);     /* set Rt2 to R1 */

	dsc->displaced_insts[0].inst = make_thumb32_inst(data1, data2);
	dsc->displaced_insts[0].size = THUMB32_INST_SIZE;
	dsc->displaced_insts_num = 1;

	return 0;
}

static int thumb32_ldrd_literal_post_handler(struct pt_regs * regs, struct displaced_desc * dsc)
{
	int rn, rt, rt2;
	unsigned int orig_inst;
	u16 inst1, inst2;

	orig_inst = dsc->orig_inst;
	split_thumb32_inst(orig_inst, &inst1, &inst2);

	rn  = px_get_bits(inst1, 0, 3);
	rt  = px_get_bits(inst2, 12, 15);
	rt2 = px_get_bits(inst2, 8, 11);

	/* save the value of R0 to Rt */
	write_reg_value(rt, regs->ARM_r0, regs);

	/* save the value of R1 to Rt2 */
	write_reg_value(rt2, regs->ARM_r1, regs);

	/* save the value of R2 to Rn */
	//write_reg_value(rn, regs->ARM_r2, regs);

	/* restore the registers values */
	if ((rn != 0) && (rt != 0) && (rt2 != 0))
		write_reg_value(0, dsc->orig_regs.ARM_r0, regs);

	if ((rn != 1) && (rt != 1) && (rt2 != 1))
		write_reg_value(1, dsc->orig_regs.ARM_r1, regs);

	if ((rn != 2) && (rt != 2) && (rt2 != 2))
		write_reg_value(2, dsc->orig_regs.ARM_r2, regs);

	return 0;
}

static int thumb32_displace_ldrd_literal(struct pt_regs * regs, struct displaced_desc * dsc)
{
	dsc->pre_handler  = &thumb32_ldrd_literal_pre_handler;
	dsc->post_handler = &thumb32_ldrd_literal_post_handler;

	return 0;
}


static int thumb32_tbb_tbh_pre_handler(struct pt_regs * regs, struct displaced_desc * dsc)
{
	dsc->displaced_insts[0].inst = THUMB32_NOP_INST;
	dsc->displaced_insts[0].size = THUMB32_INST_SIZE;
	dsc->displaced_insts_num     = 1;

	return 0;
}

static int thumb32_tbb_tbh_post_handler(struct pt_regs * regs, struct displaced_desc * dsc)
{
	unsigned int orig_inst = dsc->orig_inst;
	u16 inst1, inst2;
	bool is_tbh;
	int rn, rm;
	unsigned int rn_value, rm_value, pc;
	u16 data = 0;
	int ret;

	split_thumb32_inst(orig_inst, &inst1, &inst2);

	rn = px_get_bits(inst1, 0, 3);
	rm = px_get_bits(inst2, 0, 3);

	rn_value = read_reg_value(rn, &dsc->orig_regs, dsc->orig_regs.ARM_pc);
	rm_value = read_reg_value(rm, &dsc->orig_regs, dsc->orig_regs.ARM_pc);

	if (px_get_bit(inst2, 4) == 1)
		is_tbh = true;
	else
		is_tbh = false;

	if (is_tbh)
	{
		ret = read_proc_vm(current, rn_value + (rm_value << 1), &data, 2);
	}
	else
	{
		ret = read_proc_vm(current, rn_value + rm_value, &data, 1);
	}

	pc = read_reg_value(15, &dsc->orig_regs, dsc->orig_regs.ARM_pc);

	//write_reg_value(15, pc + 2 * data, regs);
	branch_write_pc(pc + 2 * data, regs);

	return -1;
}

static int thumb32_displace_tbb_tbh(struct pt_regs * regs, struct displaced_desc * dsc)
{
	dsc->pre_handler  = &thumb32_tbb_tbh_pre_handler;
	dsc->post_handler  = &thumb32_tbb_tbh_post_handler;

	return 0;
}

static int thumb32_displace_load_store_dual_exclusive_table_branch(struct pt_regs * regs, struct displaced_desc * dsc)
{
	unsigned int orig_inst = dsc->orig_inst;
	u16 inst1, inst2;
	int op1, op2, op3, rn;

	split_thumb32_inst(orig_inst, &inst1, &inst2);

	op1 = px_get_bits(inst1, 7, 8);
	op2 = px_get_bits(inst1, 4, 5);
	rn  = px_get_bits(inst1, 0, 3);
	op3 = px_get_bits(inst2, 4, 7);

	if (((op1 & 0x2) == 0x0) && (op2 == 0x3) && (rn == 15))
	{
		/* LDRD (literal) */
		return thumb32_displace_ldrd_literal(regs, dsc);
	}

	if (((op1 & 0x2) == 0x2) && ((op2 & 0x1) == 0x1) && (rn == 15))
	{
		/* LDRD (literal) */
		return thumb32_displace_ldrd_literal(regs, dsc);
	}

	if ((op1 == 0x1) && (op2 == 0x1) && ((op3 == 0x0) || (op3 == 0x1)))
	{
		/* TBB, TBH */
		return thumb32_displace_tbb_tbh(regs, dsc);
	}

	return thumb32_displace_unchanged(regs, dsc);
}

static int thumb32_mov_reg_pre_handler(struct pt_regs * regs, struct displaced_desc * dsc)
{
	dsc->displaced_insts[0].inst = THUMB32_NOP_INST;
	dsc->displaced_insts[0].size = THUMB32_INST_SIZE;
	dsc->displaced_insts_num     = 1;

	return 0;
}

static int thumb32_mov_reg_post_handler(struct pt_regs * regs, struct displaced_desc * dsc)
{
	unsigned int orig_inst = dsc->orig_inst;
	u16 inst1, inst2;
	unsigned int rm_value;
	int rd, rm, s;

	split_thumb32_inst(orig_inst, &inst1, &inst2);

	rd = px_get_bits(inst2, 8, 11);
	rm = px_get_bits(inst2, 0, 3);
	s  = px_get_bit(inst1, 4);

	rm_value = read_reg_value(rm, &dsc->orig_regs, dsc->orig_regs.ARM_pc);

	write_reg_value(rd, rm_value, regs);

	return 0;
}

static int thumb32_displace_mov_reg(struct pt_regs * regs, struct displaced_desc * dsc)
{
	unsigned int orig_inst = dsc->orig_inst;
	int rd, rm, s;
	u16 inst1, inst2;

	split_thumb32_inst(orig_inst, &inst1, &inst2);

	rd = px_get_bits(inst2, 8, 11);
	rm = px_get_bits(inst2, 0, 3);
	s  = px_get_bit(inst1, 4);

	if (rm == 15)
	{
		dsc->pre_handler  = &thumb32_mov_reg_pre_handler;
		dsc->post_handler = &thumb32_mov_reg_post_handler;
	}

	return thumb32_displace_unchanged(regs, dsc);
}

static int thumb32_displace_dp_shifted_reg(struct pt_regs * regs, struct displaced_desc * dsc)
{
	int op, rn, s, rd;
	unsigned int orig_inst;
	u16 inst1, inst2;

	orig_inst = dsc->orig_inst;
	split_thumb32_inst(orig_inst, &inst1, &inst2);

	op = px_get_bits(inst1, 5, 8);
	rn = px_get_bits(inst1, 0, 3);
	s  = px_get_bit(inst1, 4);
	rd = px_get_bits(inst2, 8, 11);

	if ((op == 0x2) && (rn == 15))
	{
		/* MOV (register) */
		return thumb32_displace_mov_reg(regs, dsc);
	}
	else
	{
		return thumb32_displace_unchanged(regs, dsc);
	}

	return 0;
}

static int thumb32_displace_exception_return_pre_handler(struct pt_regs * regs, struct displaced_desc * dsc)
{
	dsc->displaced_insts[0].inst = THUMB32_NOP_INST;
	dsc->displaced_insts[0].size = THUMB32_INST_SIZE;
	dsc->displaced_insts_num     = 1;

	return 0;
}

static int thumb32_displace_exception_return_post_handler(struct pt_regs * regs, struct displaced_desc * dsc)
{
	unsigned int orig_inst = dsc->orig_inst;
	unsigned int imm8, imm32, lr_value;

	imm8 = px_get_bits(orig_inst, 0, 7);

	imm32 = imm8;

	lr_value = read_reg_value(14, &dsc->orig_regs, dsc->orig_regs.ARM_pc);

	//write_reg_value(15, lr_value - imm32, &dsc->orig_regs);
	branch_write_pc(lr_value - imm32, regs);

	//regs->ARM_cpsr = regs->ARM_spsr;

	return -1;
}

static int thumb32_displace_exception_return(struct pt_regs * regs, struct displaced_desc * dsc)
{
	dsc->pre_handler  = &thumb32_displace_exception_return_pre_handler;
	dsc->post_handler = &thumb32_displace_exception_return_post_handler;

	return 0;
}

static int thumb32_b_pre_handler(struct pt_regs * regs, struct displaced_desc * dsc)
{
	dsc->displaced_insts[0].inst = THUMB32_NOP_INST;
	dsc->displaced_insts[0].size = THUMB32_INST_SIZE;
	dsc->displaced_insts_num     = 1;

	return 0;
}

static int thumb32_b_post_handler(struct pt_regs * regs, struct displaced_desc * dsc)
{
	unsigned int orig_inst = dsc->orig_inst;
	u16 inst1, inst2;

	split_thumb32_inst(orig_inst, &inst1, &inst2);

	if (px_get_bit(inst2, 12) == 1)
	{
		int s, i1, i2, j1, j2;
		unsigned int imm10, imm11, imm32, pc;

		s    = px_get_bit(inst1, 10);
		imm10 = px_get_bits(inst1, 0, 9);

		j1    = px_get_bit(inst2, 13);
		j2    = px_get_bit(inst2, 11);
		imm11 = px_get_bits(inst2, 0, 10);

		i1 = !(j1 ^ s);
		i2 = !(j2 ^ s);

		imm32 = (s << 24) + (i1 << 23) + (i2 << 22) + (imm10 << 12) + (imm11 << 1);
		imm32 = sign_extend(imm32, 24);

		//pc = dsc->orig_regs.ARM_pc + 4;
		pc = read_reg_value(15, &dsc->orig_regs, dsc->orig_regs.ARM_pc);

		//write_reg_value(15, pc + imm32, regs);
		branch_write_pc(pc + imm32, regs);
		return -1;
	}
	else
	{
		int s, cond, j1, j2;
		unsigned int imm6, imm11, imm32, pc;

		s    = px_get_bit(inst1, 10);
		cond = px_get_bits(inst1, 6, 9);
		imm6 = px_get_bits(inst1, 0, 5);

		j1    = px_get_bit(inst2, 13);
		j2    = px_get_bit(inst2, 11);
		imm11 = px_get_bits(inst2, 0, 10);

		imm32 = (s << 20) + (j2 << 19) + (j1 << 18) + (imm6 << 12) + (imm11 << 1);
		imm32 = sign_extend(imm32, 20);

		//pc = dsc->orig_regs.ARM_pc + 4;
		pc = read_reg_value(15, &dsc->orig_regs, dsc->orig_regs.ARM_pc);

		if (condition_pass(cond, &dsc->orig_regs))
		{
			//write_reg_value(15, pc + imm32, regs);
			branch_write_pc(pc + imm32, regs);
			return -1;
		}
		else
		{
			return 0;
		}

	}
	return 0;
}

static int thumb32_displace_b(struct pt_regs * regs, struct displaced_desc * dsc)
{
	dsc->pre_handler  = &thumb32_b_pre_handler;
	dsc->post_handler = &thumb32_b_post_handler;

	return 0;
}

static int thumb32_bl_blx_immed_pre_handler(struct pt_regs * regs, struct displaced_desc * dsc)
{
	dsc->displaced_insts[0].inst = THUMB32_NOP_INST;
	dsc->displaced_insts[0].size = THUMB32_INST_SIZE;

	/* 
	 * In the armv5t instruction set, 
	 * mov.w r0, r0 is an undefined instruction
	 * so we can't use it as a NOP instruction
	 */
	dsc->displaced_insts_num     = 0;

	return 0;
}

static int thumb32_bl_blx_immed_post_handler(struct pt_regs * regs, struct displaced_desc * dsc)
{
	unsigned int pc;
	unsigned int orig_inst = dsc->orig_inst;
	u16 inst1, inst2;
	unsigned int imm_high, imm_low, imm32;
	bool to_arm;
	int i1, i2, j1, j2, s;

	split_thumb32_inst(orig_inst, &inst1, &inst2);

	imm_high = px_get_bits(inst1, 0, 9);
	imm_low  = px_get_bits(inst2, 0, 10);
	s = px_get_bit(inst1, 10);
	j1 = px_get_bit(inst2, 13);
	j2 = px_get_bit(inst2, 11);

	i1 = !(j1 ^ s);
	i2 = !(j2 ^ s);

	imm32 = (s << 24) | (i1 << 23) | (i2 << 22) | (imm_high << 12) | (imm_low << 1);
	imm32 = sign_extend(imm32, 24);

	pc = read_reg_value(15, &dsc->orig_regs, dsc->orig_regs.ARM_pc);

	if (px_get_bit(inst2, 12) == 1)
	{
		to_arm = false;
	}
	else
	{
		to_arm = true;
	}

	//if (condition_pass(cond, &dsc->orig_regs))
	{
		if (to_arm)
		{
			regs->ARM_cpsr &= ~FLAG_T;
		}
		else
		{
			regs->ARM_cpsr |= FLAG_T;
		}

		write_reg_value(14, pc | 0x1, regs);
		//write_reg_value(15, pc + imm32, regs);
		branch_write_pc(pc + imm32, regs);

		return -1;
	}
}

static int thumb32_displace_bl_blx_immed(struct pt_regs * regs, struct displaced_desc * dsc)
{
	dsc->pre_handler  = &thumb32_bl_blx_immed_pre_handler;
	dsc->post_handler = &thumb32_bl_blx_immed_post_handler;

	return 0;
}

static int thumb32_displace_branch_misc_control(struct pt_regs * regs, struct displaced_desc * dsc)
{
	unsigned int orig_inst;
	u16 inst1, inst2;
	int op, op1, op2;

	orig_inst = dsc->orig_inst;


	split_thumb32_inst(orig_inst, &inst1, &inst2);

	op1 = px_get_bits(inst2, 12, 14);
	op2 = px_get_bits(inst2, 8, 11);
	op  = px_get_bits(inst1, 4, 10);

	if ((op1 == 0x0) || (op1 == 0x2))
	{
		if ((op == 0x38) || (op == 0x39))
		{
			/* MSR (register) */
			return thumb32_displace_unchanged(regs, dsc);
		}

		if (op == 0x3a)
		{
			/* Change Processor State, and hints */
			return thumb32_displace_unchanged(regs, dsc);
		}

		if (op == 0x3b)
		{
			/* Miscellaneous control instructions */
			return thumb32_displace_unchanged(regs, dsc);
		}

		if (op == 0x3c)
		{
			/* BXJ */
			return thumb32_displace_unchanged(regs, dsc);
		}

		if (op == 0x3d)
		{
			/* SUBS PC, LR and related instructions */
			return thumb32_displace_exception_return(regs, dsc);
		}

		if ((op == 0x3e) || (op == 0x3f))
		{
			/* MRS */
			return thumb32_displace_unchanged(regs, dsc);
		}

		if ((op & 0x38) != 0x38)
		{
			/* B */
			return thumb32_displace_b(regs, dsc);
		}
	}

	if ((op1 == 0x0) && (op == 0x7f))
	{
		/* SMC */
		return thumb32_displace_unchanged(regs, dsc);
	}

	if ((op1 == 0x2) && (op == 0x7f))
	{
		/* Permanently UNDEFINED */
		return thumb32_displace_undef(regs, dsc);
	}

	if ((op1 == 0x1) || (op1 == 0x3))
	{
		/* B */
		return thumb32_displace_b(regs, dsc);
	}

	if ((op1 == 0x4) || (op1 == 0x6))
	{
		/* BL, BLX */
		return thumb32_displace_bl_blx_immed(regs, dsc);
	}

	if ((op1 == 0x5) || (op1 == 0x7))
	{
		/* BL, BLX */
		return thumb32_displace_bl_blx_immed(regs, dsc);
	}

	return 0;
}

static int thumb32_vst_pre_handler(struct pt_regs * regs, struct displaced_desc * dsc)
{
	unsigned int orig_inst = dsc->orig_inst;
	u16 inst1, inst2;
	unsigned int data;
	int rn;
	unsigned int rn_value;

	split_thumb32_inst(orig_inst, &inst1, &inst2);

	rn = px_get_bits(inst1, 0, 3);
	rn_value = read_reg_value(rn, regs, regs->ARM_pc);

	write_reg_value(0, rn_value, regs);

	data = inst1;
	data = px_change_bits(data, 0, 3, 0);     /* change Rn to R0 */

	dsc->displaced_insts[0].inst = make_thumb32_inst(data, inst2);
	dsc->displaced_insts[0].size = THUMB32_NOP_INST;
	dsc->displaced_insts_num     = 1;

	return 0;
}

static int thumb32_vst_post_handler(struct pt_regs * regs, struct displaced_desc * dsc)
{
	unsigned int orig_inst = dsc->orig_inst;
	u16 inst1, inst2;
	int rn;

	split_thumb32_inst(orig_inst, &inst1, &inst2);

	rn = px_get_bits(inst1, 0, 3);

	/* save the value of R0 to Rn */
	write_reg_value(rn, regs->ARM_r0, regs);

	/* restore register value */
	write_reg_value(0, dsc->orig_regs.ARM_r0, regs);

	return 0;
}

static int thumb32_displace_vst(struct pt_regs * regs, struct displaced_desc * dsc)
{
	dsc->pre_handler  = &thumb32_vst_pre_handler;
	dsc->post_handler  = &thumb32_vst_post_handler;
	return 0;
}


static int thumb32_pld_pldw_pli_pre_handler(struct pt_regs * regs, struct displaced_desc * dsc)
{
	unsigned int orig_inst = dsc->orig_inst;
	u16 inst1, inst2;
	int rn;
	unsigned int pc, data;

	split_thumb32_inst(orig_inst, &inst1, &inst2);
	rn = px_get_bits(inst1, 0, 3);
	//pc = regs->ARM_pc + 4;
	pc = read_reg_value(15, regs, regs->ARM_pc);

	data = inst1;
	data = px_change_bits(inst1, 0, 3, 0);         /* change PC to R0 */

	write_reg_value(0, pc, regs);

	dsc->displaced_insts[0].inst = make_thumb32_inst(data, inst2);
	dsc->displaced_insts[0].size = THUMB32_INST_SIZE;
	dsc->displaced_insts_num     = 1;

	return 0;
}

static int thumb32_pld_pldw_pli_post_handler(struct pt_regs * regs, struct displaced_desc * dsc)
{
	/* restore the registers value */
	write_reg_value(0, dsc->orig_regs.ARM_r0, regs);

	return 0;
}

static int thumb32_displace_pld_pldw_pli(struct pt_regs * regs, struct displaced_desc * dsc)
{
	dsc->pre_handler  = &thumb32_pld_pldw_pli_pre_handler;
	dsc->post_handler  = &thumb32_pld_pldw_pli_post_handler;

	return 0;
}

static int thumb32_ldr_literal_pre_handler(struct pt_regs * regs, struct displaced_desc * dsc)
{
	dsc->displaced_insts[0].inst = THUMB32_NOP_INST;
	dsc->displaced_insts[0].size = THUMB32_INST_SIZE;
	dsc->displaced_insts_num = 1;

	return 0;
}

static int thumb32_ldr_literal_post_handler(struct pt_regs * regs, struct displaced_desc * dsc)
{
	int rt;
	int u;
	unsigned int base;
	unsigned int data;
	u16 inst1, inst2;
	unsigned int address;
	unsigned int imm12, imm32;

	/* simulate the instruction */
	split_thumb32_inst(dsc->orig_inst, &inst1, &inst2);

	rt    = px_get_bits(inst2, 12, 15);
	u     = px_get_bit(inst1, 7);
	imm12 = px_get_bits(inst2, 0, 11);

	//imm32 = sign_extend(imm12, 11);
	imm32 = zero_extend(imm12, 11);

	base = read_reg_value(15, &dsc->orig_regs, dsc->orig_regs.ARM_pc);
	base &= ~0x3;

	if (u == 1)
		address = base + imm32;
	else
		address = base - imm32;

	read_proc_vm(current, address, &data, 4);

	if (rt == 15)
	{
		//write_reg_value(15, data, regs);
		load_write_pc(data, regs);
	}
	else
	{
		write_reg_value(rt, data, regs);
	}

	return 0;
}

static int thumb32_displace_ldr_literal(struct pt_regs * regs, struct displaced_desc * dsc)
{
	dsc->pre_handler  = &thumb32_ldr_literal_pre_handler;
	dsc->post_handler = &thumb32_ldr_literal_post_handler;

	return 0;
}

static int thumb32_ldr_immed_pre_handler(struct pt_regs * regs, struct displaced_desc * dsc)
{
	int rn, rt;
	unsigned int rn_value, rt_value;
	unsigned int data1, data2;
	unsigned int orig_inst;
	u16 inst1, inst2;

	orig_inst = dsc->orig_inst;
	split_thumb32_inst(orig_inst, &inst1, &inst2);

	rn = px_get_bits(inst1, 0, 3);
	rt = px_get_bits(inst2, 12, 15);

	rn_value = read_reg_value(rn, regs, regs->ARM_pc);
	rt_value = read_reg_value(rt, regs, regs->ARM_pc);

	write_reg_value(0, rn_value, regs);
	write_reg_value(1, rt_value, regs);

	data1 = inst1;
	data1 = px_change_bits(data1, 0, 3, 0);      /* set Rn to R0 */

	data2 = inst2;
	data2 = px_change_bits(data2, 12, 15, 1);    /* set Rt to R1 */

	dsc->displaced_insts[0].inst = make_thumb32_inst(data1, data2);
	dsc->displaced_insts[0].size = THUMB32_INST_SIZE;
	dsc->displaced_insts_num = 1;

	return 0;
}

static int thumb32_ldr_immed_post_handler(struct pt_regs * regs, struct displaced_desc * dsc)
{
	int rn, rt;
	unsigned int orig_inst;
	u16 inst1, inst2;

	orig_inst = dsc->orig_inst;
	split_thumb32_inst(orig_inst, &inst1, &inst2);

	rn = px_get_bits(inst1, 0, 3);
	rt = px_get_bits(inst2, 12, 15);

	/* save the value of R0 to Rn */
	write_reg_value(rn, regs->ARM_r0, regs);

	/* save the value of R1 to Rt */
	if (rt != 15)
	{
		write_reg_value(rt, regs->ARM_r1, regs);
	}
	else
	{
		load_write_pc(regs->ARM_r1, regs);
/*
		if (px_get_bit(regs->ARM_r1, 0) == 1)
		{
			regs->ARM_cpsr |= FLAG_T;
			write_reg_value(15, regs->ARM_r1 & ~0x1, regs);
		}
		else
		{
			regs->ARM_cpsr &= ~FLAG_T;
			write_reg_value(15, regs->ARM_r1 & ~0x3, regs);
		}
*/

	}

	/* restore the registers values */
	if ((rn != 0) && (rt != 0))
		write_reg_value(0, dsc->orig_regs.ARM_r0, regs);

	if ((rn != 1) && (rt != 1))
		write_reg_value(1, dsc->orig_regs.ARM_r1, regs);

	if (rt == 15)
		return -1;
	else
		return 0;
}

static int thumb32_displace_ldr_immed(struct pt_regs * regs, struct displaced_desc * dsc)
{
	int rn, rt;
	unsigned int orig_inst;
	u16 inst1, inst2;

	orig_inst = dsc->orig_inst;
	split_thumb32_inst(orig_inst, &inst1, &inst2);

	rn = px_get_bits(inst1, 0, 3);
	rt = px_get_bits(inst2, 12, 15);

	if ((rn != 15) && (rt != 15))
	{
		return thumb32_displace_unchanged(regs, dsc);
	}

	dsc->pre_handler  = &thumb32_ldr_immed_pre_handler;
	dsc->post_handler = &thumb32_ldr_immed_post_handler;

	return 0;
}

static int thumb32_ldr_reg_pre_handler(struct pt_regs * regs, struct displaced_desc * dsc)
{
	int rn, rt, rm;
	unsigned int rn_value, rt_value, rm_value;
	unsigned int data1, data2;
	unsigned int orig_inst;
	u16 inst1, inst2;

	orig_inst = dsc->orig_inst;
	split_thumb32_inst(orig_inst, &inst1, &inst2);

	rn = px_get_bits(inst1, 0, 3);
	rt = px_get_bits(inst2, 12, 15);
	rm = px_get_bits(inst2, 0, 3);

	rn_value = read_reg_value(rn, regs, regs->ARM_pc);
	rt_value = read_reg_value(rt, regs, regs->ARM_pc);
	rm_value = read_reg_value(rm, regs, regs->ARM_pc);

	write_reg_value(0, rn_value, regs);
	write_reg_value(1, rt_value, regs);
	write_reg_value(2, rm_value, regs);

	data1 = inst1;
	data1 = px_change_bits(data1, 0, 3, 0);      /* set Rn to R0 */

	data2 = inst2;
	data2 = px_change_bits(data2, 12, 15, 1);    /* set Rt to R1 */
	data2 = px_change_bits(data2, 0, 3, 2);      /* set Rm to R2 */

	dsc->displaced_insts[0].inst = make_thumb32_inst(data1, data2);
	dsc->displaced_insts[0].size = THUMB32_INST_SIZE;
	dsc->displaced_insts_num = 1;

	return 0;
}

static int thumb32_ldr_reg_post_handler(struct pt_regs * regs, struct displaced_desc * dsc)
{
	int rn, rt, rm;
	unsigned int orig_inst;
	u16 inst1, inst2;

	orig_inst = dsc->orig_inst;
	split_thumb32_inst(orig_inst, &inst1, &inst2);

	rn = px_get_bits(inst1, 0, 3);
	rt = px_get_bits(inst2, 12, 15);
	rm = px_get_bits(inst2, 0, 3);

	/* save the value of R0 to Rn */
	write_reg_value(rn, regs->ARM_r0, regs);

	/* save the value of R1 to Rt */
	if (rt != 15)
	{
		write_reg_value(rt, regs->ARM_r1, regs);
	}
	else
	{
		load_write_pc(regs->ARM_r1, regs);
/*
		if (px_get_bit(regs->ARM_r1, 0) == 1)
		{
			regs->ARM_cpsr |= FLAG_T;
			write_reg_value(15, regs->ARM_r1 & ~0x1, regs);
		}
		else
		{
			regs->ARM_cpsr &= ~FLAG_T;
			write_reg_value(15, regs->ARM_r1 & ~0x3, regs);
		}
*/
	}

	/* save the value of R2 to Rm */
	write_reg_value(rm, regs->ARM_r2, regs);

	/* restore the registers values */
	if ((rn != 0) && (rt != 0) && (rm != 0))
		write_reg_value(0, dsc->orig_regs.ARM_r0, regs);

	if ((rn != 1) && (rt != 1) && (rm != 1))
		write_reg_value(1, dsc->orig_regs.ARM_r1, regs);

	if ((rn != 2) && (rt != 2) && (rm != 2))
		write_reg_value(2, dsc->orig_regs.ARM_r2, regs);

	if (rt == 15)
		return -1;
	else
		return 0;
}

static int thumb32_displace_ldr_reg(struct pt_regs * regs, struct displaced_desc * dsc)
{
	int rn, rt, rm;
	unsigned int orig_inst;
	u16 inst1, inst2;

	orig_inst = dsc->orig_inst;
	split_thumb32_inst(orig_inst, &inst1, &inst2);

	rn = px_get_bits(inst1, 0, 3);
	rt = px_get_bits(inst2, 12, 15);
	rm = px_get_bits(inst2, 0, 3);

	if ((rn != 15) && (rm != 15) && (rt != 15))
	{
		return thumb32_displace_unchanged(regs, dsc);
	}

	dsc->pre_handler  = &thumb32_ldr_reg_pre_handler;
	dsc->post_handler = &thumb32_ldr_reg_post_handler;

	return 0;
}


static int thumb32_displace_ldr(struct pt_regs * regs, struct displaced_desc * dsc)
{
	int rn, rt;
	unsigned int orig_inst;
	u16 inst1, inst2;
	int op1, op2;

	orig_inst = dsc->orig_inst;
	split_thumb32_inst(orig_inst, &inst1, &inst2);

	rn = px_get_bits(inst1, 0, 3);
	rt = px_get_bits(inst2, 12, 15);

	op1 = px_get_bits(inst1, 7, 8);
	op2 = px_get_bits(inst2, 6, 11);

	if ((rn != 15) && (rt != 15))
		return thumb32_displace_unchanged(regs, dsc);

	if (rn == 15)
	{
		return thumb32_displace_ldr_literal(regs, dsc);
	}
	else
	{
		if ((op1 == 0) && (op2 == 0))
			return thumb32_displace_ldr_reg(regs, dsc);
		else
			return thumb32_displace_ldr_immed(regs, dsc);
	}

	return 0;
}

static int thumb32_ldrb_ldrsb_literal_pre_handler(struct pt_regs * regs, struct displaced_desc * dsc)
{
	dsc->displaced_insts[0].inst = THUMB32_NOP_INST;
	dsc->displaced_insts[0].size = THUMB32_INST_SIZE;
	dsc->displaced_insts_num = 1;

	return 0;
}

static int thumb32_ldrb_ldrsb_literal_post_handler(struct pt_regs * regs, struct displaced_desc * dsc)
{
	int rt;
	int u;
	unsigned int base;
	u16 inst1, inst2;
	unsigned int address;
	unsigned int imm12, imm32;
	unsigned int data = 0;
	bool sb_form;           /* it is true for ldrsb, false for ldrb */

	/* simulate the instruction */
	split_thumb32_inst(dsc->orig_inst, &inst1, &inst2);

	rt    = px_get_bits(inst2, 12, 15);
	u     = px_get_bit(inst1, 7);
	imm12 = px_get_bits(inst2, 0, 11);

	if (px_get_bit(inst1, 8) == 1)
		sb_form = true;
	else
		sb_form = false;

	//imm32 = sign_extend(imm12, 11);
	imm32 = zero_extend(imm12, 11);

	base = read_reg_value(15, &dsc->orig_regs, dsc->orig_regs.ARM_pc);
	base &= ~0x3;

	if (u == 1)
		address = base + imm32;
	else
		address = base - imm32;

	read_proc_vm(current, address, &data, 1);

	if (sb_form)
	{
		write_reg_value(rt, sign_extend(data, 7), regs);
	}
	else
	{
		write_reg_value(rt, data, regs);
	}

	return 0;
}

static int thumb32_displace_ldrb_ldrsb_literal(struct pt_regs * regs, struct displaced_desc * dsc)
{
	dsc->pre_handler  = &thumb32_ldrb_ldrsb_literal_pre_handler;
	dsc->post_handler = &thumb32_ldrb_ldrsb_literal_post_handler;

	return 0;
}

static int thumb32_displace_ldrb_ldrsb_pld(struct pt_regs * regs, struct displaced_desc * dsc)
{
	unsigned int orig_inst;
	u16 inst1, inst2;
	int rn, rt, op1;

	orig_inst = dsc->orig_inst;

	split_thumb32_inst(orig_inst, &inst1, &inst2);

	op1 = px_get_bits(inst1, 7, 8);
	rn  = px_get_bits(inst1, 0, 3);
	rt  = px_get_bits(inst2, 12, 15);

	if (rt == 15)
	{
		return thumb32_displace_pld_pldw_pli(regs, dsc);
	}
	else if (rn == 15)
	{
		return thumb32_displace_ldrb_ldrsb_literal(regs, dsc);
	}
	else
	{
		return thumb32_displace_unchanged(regs, dsc);
	}
}

static int thumb32_ldrh_ldrsh_literal_pre_handler(struct pt_regs * regs, struct displaced_desc * dsc)
{
	dsc->displaced_insts[0].inst = THUMB32_NOP_INST;
	dsc->displaced_insts[0].size = THUMB32_INST_SIZE;
	dsc->displaced_insts_num = 1;

	return 0;
}

static int thumb32_ldrh_ldrsh_literal_post_handler(struct pt_regs * regs, struct displaced_desc * dsc)
{
	int rt;
	int u;
	unsigned int base;
	u16 inst1, inst2;
	unsigned int address;
	unsigned int imm12, imm32;
	unsigned int data = 0;
	bool sh_form;           /* it is true for ldrsh, false for ldrh */

	/* simulate the instruction */
	split_thumb32_inst(dsc->orig_inst, &inst1, &inst2);

	rt    = px_get_bits(inst2, 12, 15);
	u     = px_get_bit(inst1, 7);
	imm12 = px_get_bits(inst2, 0, 11);

	if (px_get_bit(inst1, 8) == 1)
		sh_form = true;
	else
		sh_form = false;

	//imm32 = sign_extend(imm12, 11);
	imm32 = zero_extend(imm12, 11);

	base = read_reg_value(15, &dsc->orig_regs, dsc->orig_regs.ARM_pc);
	base &= ~0x3;

	if (u == 1)
		address = base + imm32;
	else
		address = base - imm32;

	read_proc_vm(current, address, &data, 2);

	if (sh_form)
	{
		write_reg_value(rt, sign_extend(data, 15), regs);
	}
	else
	{
		write_reg_value(rt, data, regs);
	}

	return 0;
}

static int thumb32_displace_ldrh_ldrsh_literal(struct pt_regs * regs, struct displaced_desc * dsc)
{
	dsc->pre_handler  = &thumb32_ldrh_ldrsh_literal_pre_handler;
	dsc->post_handler = &thumb32_ldrh_ldrsh_literal_post_handler;

	return 0;
}

static int thumb32_displace_ldrh_ldrsh_pld(struct pt_regs * regs, struct displaced_desc * dsc)
{
	unsigned int orig_inst;
	u16 inst1, inst2;
	int rn, rt, op1;

	orig_inst = dsc->orig_inst;

	split_thumb32_inst(orig_inst, &inst1, &inst2);

	op1 = px_get_bits(inst1, 7, 8);
	rn  = px_get_bits(inst1, 0, 3);
	rt  = px_get_bits(inst2, 12, 15);

	if (rt == 15)
	{
		return thumb32_displace_pld_pldw_pli(regs, dsc);
	}
	else if (rn == 15)
	{
		return thumb32_displace_ldrh_ldrsh_literal(regs, dsc);
	}
	else
	{
		return thumb32_displace_unchanged(regs, dsc);
	}

	return 0;
}

static int thumb32_displace_inst(struct pt_regs * regs, struct displaced_desc * dsc)
{
	int op1, op2, op;
	unsigned int orig_inst;
	u16 inst1, inst2;

	orig_inst = dsc->orig_inst;
	split_thumb32_inst(orig_inst, &inst1, &inst2);

	op1 = px_get_bits(inst1, 11, 12);
	op2 = px_get_bits(inst1, 4, 10);
	op  = px_get_bit(inst2, 15);

	switch (op1)
	{
	case 1:
		if ((op2 & 0x64) == 0x0)
		{
			/* Load/store multiple */
			return thumb32_displace_load_store_multiple(regs, dsc);
		}
		else if ((op2 & 0x64) == 0x4)
		{
			/* Load/store dual, load/store exclusive, table branch */
			return thumb32_displace_load_store_dual_exclusive_table_branch(regs, dsc);
		}
		else if ((op2 & 0x60) == 0x40)
		{
			/* Data-processing (shifted register) */
			return thumb32_displace_dp_shifted_reg(regs, dsc);
		}
		else
		{
			/* Coprocessor instructions */
			return thumb32_displace_coprocessor(regs, dsc);
		}
	case 2:
		if (op == 0)
		{
			if ((op2 & 0x20) == 0)
			{
				/* Data-processing (modified immediate) */
				return thumb32_displace_unchanged(regs, dsc);
			}
			else
			{
				/* Data-processing (plain binary immediate) */
				return thumb32_displace_dp_plan_binary_immed(regs, dsc);
			}
		}
		else
		{
			/* Branches and miscellaneous control */
			return thumb32_displace_branch_misc_control(regs, dsc);
		}
	case 3:
		if ((op2 & 0x71) == 0x0)
		{
			/* Store single data item */
			return thumb32_displace_unchanged(regs, dsc);
		}
		else if ((op2 & 0x71) == 0x10)
		{
			/* Advanced SIMD element or structure load/store instructions */
			return thumb32_displace_vst(regs, dsc);
		}
		else if ((op2 & 0x67) == 0x1)
		{
			/* Load byte, memory hints */
			return thumb32_displace_ldrb_ldrsb_pld(regs, dsc);
		}
		else if ((op2 & 0x67) == 0x3)
		{
			/* Load halfword, memory hints */
			return thumb32_displace_ldrh_ldrsh_pld(regs, dsc);
		}
		else if ((op2 & 0x67) == 0x5)
		{
			/* Load word */
			return thumb32_displace_ldr(regs, dsc);
		}
		else if ((op2 & 0x67) == 0x7)
		{
			/* undefined */
			return thumb32_displace_undef(regs, dsc);
		}
		else if ((op2 & 0x70) == 0x20)
		{
			/* Data-processing (register) */
			return thumb32_displace_unchanged(regs, dsc);
		}
		else if ((op2 & 0x78) == 0x30)
		{
			/* Multiply, multiply accumulate, and absolute difference */
			return thumb32_displace_unchanged(regs, dsc);
		}
		else if ((op2 & 0x78) == 0x38)
		{
			/* Long multiply, long multiply accumulate, and divide */
			return thumb32_displace_unchanged(regs, dsc);
		}
		else if ((op2 & 0x40) == 0x40)
		{
			/* Coprocessor instructions */
			return thumb32_displace_coprocessor(regs, dsc);
		}
	}

	return 0;
}

int thumb_displace_inst(struct pt_regs * regs, struct displaced_desc * dsc)
{
	unsigned int orig_inst = dsc->orig_inst;

	switch (px_get_bits(orig_inst, 11, 15))
	{
	case 0x1d:
	case 0x1e:
	case 0x1f:
		return thumb32_displace_inst(regs, dsc);
	default:
		return thumb16_displace_inst(regs, dsc);
	}
}
